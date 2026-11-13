#!/usr/bin/env python3
"""CP-SAT large-neighbourhood search for the flexible job shop (open-source improver).

Starts from a feasible schedule and repeatedly frees a small part of it (a few jobs, a time
window, the operations of one machine pool, or near-critical operations), keeps every other
operation on its machine in its current relative order (times may shift), and re-solves the
freed part exactly with CP-SAT under 'makespan <= current'.  Equal-makespan solutions are
accepted (plateau drift); improvements are written to the output file immediately.

usage: lns_cpsat.py INSTANCE.fjs START.sched OUT.sched [--time S] [--sub S] [--workers W]
                    [--target T] [--seed K] [--log FILE]
"""
import argparse, json, os, random, sys, time
from ortools.sat.python import cp_model
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_fjsp as C


def load_sched(inst, path):
    claim, meta, rows = C.read_schedule(path)
    for off in (0, 1):
        errs, ms = C.check(inst, rows, off)
        if not errs:
            return ms, {(j, o): (mm - off, st, en) for (j, o, mm, st, en) in rows}
    sys.exit(f'{path}: not feasible')


def write_sched(path, name, sched, ms):
    tmp = path + '.tmp'
    with open(tmp, 'w') as f:
        f.write(f'makespan {ms}\n# lns_cpsat; machine ids 0-based\n# job op machine start end\n')
        for (j, o), (mm, st, en) in sorted(sched.items()):
            f.write(f'{j} {o} {mm} {st} {en}\n')
    os.replace(tmp, path)


def pools_of(m, jobs):
    sig = {mm: [] for mm in range(m)}
    for j, ops in enumerate(jobs):
        for o, d in enumerate(ops):
            for mm in d:
                sig[mm].append((j, o))
    groups = {}
    for mm in range(m):
        groups.setdefault(tuple(sig[mm]), []).append(mm)
    return sorted(groups.values())


def slacks(jobs, sched, ms):
    """slack of each op = ms - (head + dur + tail) in the current schedule's disjunctive graph."""
    ops = sorted(sched)
    per = {}
    for (j, o), (mm, st, en) in sched.items():
        per.setdefault(mm, []).append((st, j, o))
    msucc = {}
    for mm, L in per.items():
        L.sort()
        for a, b in zip(L, L[1:]):
            msucc[(a[1], a[2])] = (b[1], b[2])
    # tails by reverse topological order (start-time order is a topological order)
    order = sorted(ops, key=lambda k: -sched[k][1])
    tail = {}
    for k in order:
        j, o = k
        t = 0
        if (j, o + 1) in sched:
            t = max(t, tail[(j, o + 1)] + (sched[(j, o + 1)][2] - sched[(j, o + 1)][1]))
        if k in msucc:
            s = msucc[k]
            t = max(t, tail[s] + (sched[s][2] - sched[s][1]))
        tail[k] = t
    return {k: ms - (sched[k][2] + tail[k]) for k in ops}


def solve_sub(jobs, m, sched, free, cap, sub_time, workers, seed, horizon, seqfree=False):
    mdl = cp_model.CpModel()
    S, E, LIT = {}, {}, {}
    per_machine = {}
    cmax = mdl.NewIntVar(0, cap, 'cmax')
    for j, J in enumerate(jobs):
        prev = None
        for o, alts in enumerate(J):
            mm0, st0, en0 = sched[j, o]
            if (j, o) in free:
                s = mdl.NewIntVar(0, cap, f's{j}_{o}')
                e = mdl.NewIntVar(0, cap, f'e{j}_{o}')
                lits = {}
                for mm, p in alts.items():
                    l = mdl.NewBoolVar(f'x{j}_{o}_{mm}')
                    lits[mm] = l
                    iv = mdl.NewOptionalIntervalVar(s, p, e, l, f'i{j}_{o}_{mm}')
                    per_machine.setdefault(mm, []).append(iv)
                    mdl.AddHint(l, 1 if mm == mm0 else 0)
                mdl.AddExactlyOne(lits.values())
                mdl.AddHint(s, st0); mdl.AddHint(e, en0)
                LIT[j, o] = lits
            else:
                p = alts[mm0]
                s = mdl.NewIntVar(0, cap, f's{j}_{o}')
                e = mdl.NewIntVar(0, cap, f'e{j}_{o}')
                mdl.Add(e == s + p)
                iv = mdl.NewIntervalVar(s, p, e, f'i{j}_{o}')
                per_machine.setdefault(mm0, []).append(iv)
                mdl.AddHint(s, st0)
            S[j, o] = s; E[j, o] = e
            if prev is not None:
                mdl.Add(s >= prev)
            prev = e
        mdl.Add(cmax >= prev)
    # keep the relative order of the fixed operations on each machine (unless --seqfree)
    per = {}
    for (j, o), (mm, st, en) in sched.items():
        if (j, o) not in free:
            per.setdefault(mm, []).append((st, j, o))
    if not seqfree:
        for mm, L in per.items():
            L.sort()
            for a, b in zip(L, L[1:]):
                mdl.Add(S[b[1], b[2]] >= E[a[1], a[2]])
    for ivs in per_machine.values():
        if len(ivs) > 1:
            mdl.AddNoOverlap(ivs)
    mdl.Minimize(cmax)
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = sub_time
    solver.parameters.num_workers = workers
    solver.parameters.random_seed = seed
    st = solver.Solve(mdl)
    if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        return None, solver.StatusName(st)
    new = {}
    for (j, o) in sched:
        if (j, o) in free:
            mm = next(k for k, l in LIT[j, o].items() if solver.Value(l))
        else:
            mm = sched[j, o][0]
        new[j, o] = (mm, solver.Value(S[j, o]), solver.Value(E[j, o]))
    return new, solver.StatusName(st)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('instance'); ap.add_argument('start'); ap.add_argument('out')
    ap.add_argument('--time', type=float, default=600)
    ap.add_argument('--sub', type=float, default=10)
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--target', type=int, default=0)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--log', default=None)
    ap.add_argument('--kjobs', type=int, default=0, help='jobs per neighbourhood (0 = auto)')
    ap.add_argument('--seqfree', type=int, default=0, help='1 = fixed ops keep their machine but not their order')
    args = ap.parse_args()
    rnd = random.Random(args.seed)
    inst = C.read_instance(args.instance)
    n, m, avg, jobs, _, _ = inst
    name = os.path.basename(args.instance).rsplit('.', 1)[0]
    ms, sched = load_sched(inst, args.start)
    if os.path.exists(args.out):
        ms2, sched2 = load_sched(inst, args.out)
        if ms2 < ms:
            ms, sched = ms2, sched2
    pools = pools_of(m, jobs)
    horizon = sum(max(d.values()) for J in jobs for d in J)
    t0 = time.time()
    logf = open(args.log, 'a') if args.log else None
    def log(s):
        print(s, flush=True)
        if logf:
            logf.write(s + '\n'); logf.flush()
    log(f'{name}: start makespan {ms}, {n} jobs, {len(pools)} pools, time {args.time}, sub {args.sub}, workers {args.workers}')
    write_sched(args.out, name, sched, ms)
    it = 0; stats = {}
    kj = args.kjobs or max(4, min(14, n // 6))
    while time.time() - t0 < args.time and (args.target <= 0 or ms > args.target):
        it += 1
        mode = rnd.choice(['jobs', 'jobs', 'window', 'pool', 'crit', 'critjobs'] + (['none'] if args.seqfree and it == 1 else []))
        if it == 1 and args.seqfree:
            mode = 'none'
        free = set()
        if mode == 'jobs':
            for j in rnd.sample(range(n), min(n, kj + rnd.randint(-2, 3))):
                free |= {(j, o) for o in range(len(jobs[j]))}
        elif mode == 'window':
            w = rnd.randint(25, 60); t = rnd.randint(0, max(0, ms - w))
            free = {k for k, (mm, st, en) in sched.items() if st < t + w and en > t}
            if len(free) > 90:
                free = set(rnd.sample(sorted(free), 90))
        elif mode == 'pool':
            P = set(rnd.choice(pools))
            free = {k for k, (mm, st, en) in sched.items() if mm in P}
            # plus the job neighbours of those ops
            extra = set()
            for (j, o) in free:
                extra |= {(j, o - 1), (j, o + 1)}
            free |= {k for k in extra if k in sched}
            if len(free) > 100:
                free = set(rnd.sample(sorted(free), 100))
        elif mode == 'crit':
            sl = slacks(jobs, sched, ms)
            cands = [k for k, v in sl.items() if v <= rnd.randint(2, 8)]
            rnd.shuffle(cands)
            free = set(cands[:rnd.randint(30, 70)])
        else:  # critjobs: whole jobs that have a critical op
            sl = slacks(jobs, sched, ms)
            cj = sorted({k[0] for k, v in sl.items() if v <= 1})
            rnd.shuffle(cj)
            for j in cj[:kj]:
                free |= {(j, o) for o in range(len(jobs[j]))}
        if not free and mode != 'none':
            continue
        new, st = solve_sub(jobs, m, sched, free, ms, args.sub, args.workers, rnd.randint(0, 10**6), horizon, bool(args.seqfree))
        stats.setdefault(mode, [0, 0, 0])
        stats[mode][0] += 1
        if new is None:
            continue
        nms = max(en for (mm, st_, en) in new.values())
        # verify independently before accepting
        rows = [(j, o, mm, s_, e_) for (j, o), (mm, s_, e_) in new.items()]
        errs, ms_chk = C.check(inst, rows, 0)
        if errs or ms_chk != nms:
            log(f'!!! rejected sub-solution: {errs[:1]} {ms_chk} {nms}')
            continue
        if nms < ms:
            stats[mode][1] += 1
            ms, sched = nms, new
            write_sched(args.out, name, sched, ms)
            log(f'[{time.time()-t0:8.1f}s] it {it} {mode:8s} |free|={len(free):3d} -> makespan {ms}')
        elif nms == ms:
            stats[mode][2] += 1
            sched = new
    log(f'done: makespan {ms} after {it} iterations, {time.time()-t0:.0f}s; stats (tries, improvements, equal) {stats}')
    print(f'best_makespan {ms}')


if __name__ == '__main__':
    main()
