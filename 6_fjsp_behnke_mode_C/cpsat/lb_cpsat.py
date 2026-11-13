#!/usr/bin/env python3
"""CP-SAT model of the Behnke flexible job shop for lower bounds and optimality proofs.

Standard FJSP model (one optional fixed-size interval per eligible machine, exactly one chosen,
NoOverlap per machine, job precedence, makespan) plus SOUND redundant constraints that exploit
the work-centre structure of the Behnke instances:

  * Pools.  Machines are partitioned into pools: two machines are in the same pool iff exactly
    the same operations are eligible on them (the atoms of the eligibility sets).  For every pool
    P and operation o eligible on at least one machine of P, an optional interval [s_o, e_o)
    with presence literal l_{o,P} = sum of the machine literals of o in P is added to a
    Cumulative(capacity |P|) over the pool.  Valid because an operation processed on a machine
    of P occupies one of the |P| machines during exactly [s_o, e_o).

Relaxations (all give valid lower bounds for the original problem):
  --relax none   full model (default); a feasible solution is a real schedule and is written out
  --relax cumul  NoOverlap dropped for the machines of the middle pools (those holding neither
                 first nor last operations); the pool cumulatives remain
  --relax lag    middle operations removed; s_last >= e_first + sum of min durations of the
                 middle operations of the job; first/last stage machines kept exactly

Modes:
  --mode min     minimise the makespan (default); CP-SAT reports a proven lower bound
  --mode feas    pure feasibility of 'makespan <= cap' (needs --cap)
  --cap T        add makespan <= T.  INFEASIBLE then proves LB >= T+1 for the model solved.
  --hint FILE    warm start from a schedule file (rows: job op machine start end; any base)

Output: incumbent trace and the final line
  FINAL status=<..> obj=<..|None> bound=<..> relax=<..> cap=<..> wall=<s>
and <out>.json with the same data; <out>.sched with the schedule when relax=none and a
solution was found.
"""
import argparse, json, os, sys, time
from ortools.sat.python import cp_model

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import check_fjsp as C


def pools_of(m, jobs):
    """Partition machines into pools = atoms of the eligibility sets."""
    sig = {mm: [] for mm in range(m)}
    for j, ops in enumerate(jobs):
        for o, d in enumerate(ops):
            for mm in d:
                sig[mm].append((j, o))
    groups = {}
    for mm in range(m):
        groups.setdefault(tuple(sig[mm]), []).append(mm)
    pools = sorted(groups.values())
    return pools


def build(jobs, m, args, horizon_cap=None):
    pools = pools_of(m, jobs)
    pool_of = {}
    for pi, P in enumerate(pools):
        for mm in P:
            pool_of[mm] = pi
    first_pools = {pool_of[mm] for ops in jobs for mm in ops[0]}
    last_pools = {pool_of[mm] for ops in jobs for mm in ops[-1]}
    middle_pools = set(range(len(pools))) - first_pools - last_pools
    middle_machines = {mm for pi in middle_pools for mm in pools[pi]}

    horizon = sum(max(d.values()) for ops in jobs for d in ops)
    if horizon_cap is not None:
        horizon = min(horizon, horizon_cap)
    mdl = cp_model.CpModel()
    cmax = mdl.NewIntVar(0, horizon, 'cmax')
    per_machine = {}
    per_pool = {}
    ops = {}          # (j,o) -> dict(s,e,d,lits{m:lit})
    for j, J in enumerate(jobs):
        nop = len(J)
        kept = list(range(nop))
        if args.relax == 'lag':
            kept = [0, nop - 1] if nop >= 2 else [0]
        prev = None
        for o in kept:
            alts = J[o]
            lo, hi = min(alts.values()), max(alts.values())
            s = mdl.NewIntVar(0, horizon, f's{j}_{o}')
            e = mdl.NewIntVar(0, horizon, f'e{j}_{o}')
            d = mdl.NewIntVar(lo, hi, f'd{j}_{o}')
            mdl.Add(e == s + d)
            lits = {}
            for mm, p in alts.items():
                l = mdl.NewBoolVar(f'x{j}_{o}_{mm}')
                lits[mm] = l
                mdl.Add(d == p).OnlyEnforceIf(l)
                if not (args.relax == 'cumul' and mm in middle_machines):
                    iv = mdl.NewOptionalIntervalVar(s, p, e, l, f'i{j}_{o}_{mm}')
                    per_machine.setdefault(mm, []).append(iv)
            mdl.AddExactlyOne(lits.values())
            if args.pools:
                by_pool = {}
                for mm, l in lits.items():
                    by_pool.setdefault(pool_of[mm], []).append(l)
                for pi, ls in by_pool.items():
                    if len(by_pool) == 1:
                        piv = mdl.NewIntervalVar(s, d, e, f'p{j}_{o}_{pi}')
                    else:
                        lp = mdl.NewBoolVar(f'y{j}_{o}_{pi}')
                        mdl.Add(lp == sum(ls))
                        piv = mdl.NewOptionalIntervalVar(s, d, e, lp, f'p{j}_{o}_{pi}')
                    per_pool.setdefault(pi, []).append(piv)
            if prev is not None:
                if args.relax == 'lag' and o == nop - 1 and nop > 2:
                    lag = sum(min(J[k].values()) for k in range(1, nop - 1))
                    mdl.Add(s >= prev + lag)
                else:
                    mdl.Add(s >= prev)
            prev = e
            ops[j, o] = dict(s=s, e=e, d=d, lits=lits)
        mdl.Add(cmax >= prev)
    for ivs in per_machine.values():
        mdl.AddNoOverlap(ivs)
    if args.pools:
        for pi, ivs in per_pool.items():
            mdl.AddCumulative(ivs, [1] * len(ivs), len(pools[pi]))
    if args.cap is not None:
        mdl.Add(cmax <= args.cap)
    if args.mode == 'min':
        mdl.Minimize(cmax)
    info = dict(pools=pools, middle_pools=sorted(middle_pools), n_pools=len(pools))
    return mdl, cmax, ops, info


class Trace(cp_model.CpSolverSolutionCallback):
    def __init__(self, t0, has_obj):
        super().__init__()
        self.t0 = t0
        self.has_obj = has_obj
        self.trace = []

    def on_solution_callback(self):
        t = time.time() - self.t0
        if self.has_obj:
            v, b = self.ObjectiveValue(), self.BestObjectiveBound()
            self.trace.append((round(t, 2), int(v), int(b)))
            print(f'{t:.2f} sol {v:.0f} bound {b:.0f}', flush=True)
        else:
            self.trace.append((round(t, 2), None, None))
            print(f'{t:.2f} feasible', flush=True)


def add_hint(mdl, ops, inst, path):
    claim, meta, rows = C.read_schedule(path)
    ok = []
    for off in (0, 1):
        errs, ms = C.check(inst, rows, off)
        if not errs:
            ok.append(off)
    if len(ok) != 1:
        print('hint rejected: schedule not feasible under exactly one base', flush=True)
        return None
    off = ok[0]
    n = 0
    for (j, o, mm, st, en) in rows:
        if (j, o) not in ops:
            continue
        v = ops[j, o]
        mdl.AddHint(v['s'], st)
        mdl.AddHint(v['e'], en)
        for k, l in v['lits'].items():
            mdl.AddHint(l, 1 if k == mm - off else 0)
        n += 1
    print(f'hint loaded: {n} operations from {path} (makespan {ms})', flush=True)
    return ms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('instance')
    ap.add_argument('--time', type=float, default=120)
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--cap', type=int, default=None)
    ap.add_argument('--hint', default=None)
    ap.add_argument('--mode', choices=['min', 'feas'], default='min')
    ap.add_argument('--relax', choices=['none', 'cumul', 'lag'], default='none')
    ap.add_argument('--pools', type=int, default=1, help='1 = add the pool cumulatives (default)')
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--out', default=None, help='prefix for .json/.sched/.log outputs')
    ap.add_argument('--log', type=int, default=0, help='1 = CP-SAT search log to stdout')
    ap.add_argument('--strong', type=int, default=0, help='1 = enable the stronger (non-default) scheduling propagators')
    ap.add_argument('--param', action='append', default=[], help='extra CP-SAT parameter name=value (repeatable)')
    args = ap.parse_args()
    if args.mode == 'feas' and args.cap is None:
        sys.exit('--mode feas needs --cap')

    inst = C.read_instance(args.instance)
    n, m, avg, jobs, _, ibase = inst
    name = os.path.basename(args.instance).rsplit('.', 1)[0]
    t0 = time.time()
    mdl, cmax, ops, info = build(jobs, m, args)
    print(f'{name}: {n} jobs {m} machines, {info["n_pools"]} pools, middle pools {info["middle_pools"]}, '
          f'relax={args.relax} mode={args.mode} cap={args.cap} pools={args.pools} time={args.time} workers={args.workers}',
          flush=True)
    hint_ms = add_hint(mdl, ops, inst, args.hint) if args.hint else None

    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = args.time
    solver.parameters.num_workers = args.workers
    solver.parameters.random_seed = args.seed
    solver.parameters.log_search_progress = bool(args.log)
    if args.strong:
        for k in ('use_strong_propagation_in_disjunctive', 'use_overload_checker_in_cumulative',
                  'use_timetable_edge_finding_in_cumulative', 'use_hard_precedences_in_cumulative',
                  'exploit_all_precedences', 'use_dynamic_precedence_in_disjunctive',
                  'use_dynamic_precedence_in_cumulative'):
            setattr(solver.parameters, k, True)
    for kv in args.param:
        k, v = kv.split('=', 1)
        cur = getattr(solver.parameters, k)
        setattr(solver.parameters, k, type(cur)(v) if not isinstance(cur, bool) else v.lower() in ('1', 'true'))
    cb = Trace(t0, args.mode == 'min')
    st = solver.Solve(mdl, cb)
    wall = time.time() - t0
    status = solver.StatusName(st)
    obj = bound = None
    if args.mode == 'min':
        bound = int(solver.BestObjectiveBound()) if st in (cp_model.OPTIMAL, cp_model.FEASIBLE, cp_model.UNKNOWN) else None
        if st in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            obj = int(solver.ObjectiveValue())
    if st == cp_model.INFEASIBLE and args.cap is not None:
        bound = args.cap + 1
    if st == cp_model.OPTIMAL and args.mode == 'min':
        bound = obj
    # valid LB for the ORIGINAL problem: bound (any relaxation is a relaxation)
    print(f'FINAL status={status} obj={obj} bound={bound} relax={args.relax} cap={args.cap} wall={wall:.1f}', flush=True)
    if args.out:
        res = dict(instance=name, status=status, obj=obj, bound=bound, relax=args.relax, mode=args.mode, strong=args.strong, params=args.param,
                   cap=args.cap, pools=args.pools, time=args.time, workers=args.workers, seed=args.seed,
                   wall=round(wall, 1), hint=args.hint, hint_makespan=hint_ms, trace=cb.trace,
                   n_pools=info['n_pools'], middle_pools=info['middle_pools'])
        json.dump(res, open(args.out + '.json', 'w'), indent=1)
        if args.relax == 'none' and st in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            mk = max(solver.Value(v['e']) for v in ops.values())
            with open(args.out + '.sched', 'w') as f:
                f.write(f'makespan {mk}\n# CP-SAT {status} bound {bound} relax none\n# machine ids 0-based\n# job op machine start end\n')
                for (j, o), v in sorted(ops.items()):
                    mm = next(k for k, l in v['lits'].items() if solver.Value(l))
                    f.write(f'{j} {o} {mm} {solver.Value(v["s"])} {solver.Value(v["e"])}\n')


if __name__ == '__main__':
    main()
