#!/usr/bin/env python3
"""CP-SAT baseline for the flexible job shop with two transport robots (FJSPT).

Model of Homayouni & Fontes (2021) as verified by ../check_fjspt.py (see README.md).
All times are multiplied by 2 internally (half units become integers).

Usage:
  python cpsat_fjspt.py INSTANCE.json --time 600 --workers 8 --out best.sol \
         [--hint start.sol] [--log curve.csv] [--seed 0] [--vehicles 2] [--fix-hint]

Every improving solution is written to --out (overwritten) and logged to --log as
"wall_s,objective,bound" (original time units; wall_s = seconds since solve start).
"""
import argparse
import json
import sys
import time
from fractions import Fraction as F

from ortools.sat.python import cp_model

SCALE = 2


def to_int(x):
    v = F(str(x)) * SCALE
    if v.denominator != 1:
        raise ValueError(f"time {x} is not a multiple of 1/{SCALE}")
    return int(v)


def load_instance(path):
    I = json.load(open(path))
    jobs = [[{int(m): to_int(p) for m, p in alts} for alts in J] for J in I["jobs"]]
    T = [[to_int(x) for x in row] for row in I["travel"]]
    return I.get("name", path), jobs, T


def load_sol(path):
    ops, trs = {}, {}
    for line in open(path):
        t = line.split("#", 1)[0].split()
        if not t:
            continue
        if t[0] == "op":
            ops[(int(t[1]) - 1, int(t[2]) - 1)] = (int(t[3]), to_int(t[4]), to_int(t[5]))
        elif t[0] == "tr":
            trs[(int(t[1]) - 1, int(t[2]) - 1)] = (int(t[3]), int(t[4]), int(t[5]), to_int(t[6]), to_int(t[7]))
    return ops, trs


class Model:
    def __init__(self, jobs, T, n_veh=2, horizon=None, cumulative=True):
        self.jobs, self.T, self.nv = jobs, T, n_veh
        self.nloc = len(T)
        m = self.m = cp_model.CpModel()
        maxT = max(max(r) for r in T)
        if horizon is None:
            # serial schedule with one vehicle: per op at most empty + loaded trip + processing
            horizon = sum(max(a.values()) + 2 * maxT for J in jobs for a in J)
        self.H = H = horizon
        self.x, self.mach, self.S, self.E = {}, {}, {}, {}
        self.dep, self.arr, self.pres, self.dur = {}, {}, {}, {}
        machine_iv = {}
        for j, J in enumerate(jobs):
            for k, alts in enumerate(J):
                ms = sorted(alts)
                mv = m.new_int_var_from_domain(cp_model.Domain.from_values(ms), f"mach_{j}_{k}")
                S = m.new_int_var(0, H, f"S_{j}_{k}")
                E = m.new_int_var(0, H, f"E_{j}_{k}")
                lits = []
                for q in ms:
                    l = m.new_bool_var(f"x_{j}_{k}_{q}")
                    self.x[j, k, q] = l
                    lits.append(l)
                    iv = m.new_optional_fixed_size_interval_var(S, alts[q], l, f"iv_{j}_{k}_{q}")
                    machine_iv.setdefault(q, []).append(iv)
                m.add_exactly_one(lits)
                m.add(mv == sum(q * self.x[j, k, q] for q in ms))
                m.add(E == S + sum(alts[q] * self.x[j, k, q] for q in ms))
                self.mach[j, k], self.S[j, k], self.E[j, k] = mv, S, E
        for q, ivs in machine_iv.items():
            m.add_no_overlap(ivs)

        # ---- transports: one potential transport "into" each operation
        nodes = []
        for j, J in enumerate(jobs):
            for k in range(len(J)):
                nodes.append((j, k))
                dep = m.new_int_var(0, H, f"dep_{j}_{k}")
                arr = m.new_int_var(0, H, f"arr_{j}_{k}")
                self.dep[j, k], self.arr[j, k] = dep, arr
                i = (j, k)
                if k == 0:
                    self.pres[j, k] = None  # always present (machines are >= 1, L/U = 0)
                    # loaded duration T[0][m]
                    tab = [(q, T[0][q]) for q in sorted(J[0])]
                    d = m.new_int_var(0, maxT, f"dur_{j}_{k}")
                    self.dur[i] = d
                    m.add_allowed_assignments([self.mach[j, k], d], tab)
                    m.add(arr == dep + d)
                    m.add(dep >= 0)
                else:
                    prev = J[k - 1]
                    common = sorted(set(prev) & set(J[k]))
                    sm = []
                    for q in common:
                        s = m.new_bool_var(f"same_{j}_{k}_{q}")
                        m.add_implication(s, self.x[j, k - 1, q])
                        m.add_implication(s, self.x[j, k, q])
                        m.add_bool_or([s, self.x[j, k - 1, q].negated(), self.x[j, k, q].negated()])
                        sm.append(s)
                    p = m.new_bool_var(f"pres_{j}_{k}")
                    m.add(p + sum(sm) == 1)
                    self.pres[j, k] = p
                    tab = [(a, b, 0 if a == b else T[a][b]) for a in sorted(prev) for b in sorted(J[k])]
                    d = m.new_int_var(0, maxT, f"dur_{j}_{k}")
                    self.dur[i] = d
                    m.add_allowed_assignments([self.mach[j, k - 1], self.mach[j, k], d], tab)
                    m.add(arr == dep + d)
                    m.add(dep >= self.E[j, k - 1])  # also for absent: arr == dep, so start >= end(k-1)
                m.add(self.S[j, k] >= arr)
        self.nodes = nodes
        # redundant: loaded trips occupy a vehicle -> cumulative with capacity nv
        ivs = []
        for i in nodes:
            dur = self.dur[i]
            if self.pres[i] is None:
                ivs.append(m.new_interval_var(self.dep[i], dur, self.arr[i], f"tiv_{i}"))
            else:
                ivs.append(m.new_optional_interval_var(self.dep[i], dur, self.arr[i], self.pres[i], f"tiv_{i}"))
        if cumulative:
            m.add_cumulative(ivs, [1] * len(ivs), self.nv)
        # redundant: heads (earliest start) of every operation
        for j, J in enumerate(jobs):
            h = 0
            for k, alts in enumerate(J):
                if k == 0:
                    h = min(T[0][q] for q in alts)
                m.add(self.S[j, k] >= h)
                h += min(alts.values())

        # ---- pickup locations of each node
        def pick_options(j, k):
            """list of (location, literal or None)"""
            if k == 0:
                return [(0, None)]
            return [(q, self.x[j, k - 1, q]) for q in sorted(jobs[j][k - 1])]

        # D[i][b] = arr_i + T[loc(i)][b] for pickup locations b used by someone
        used_b = set()
        for (j, k) in nodes:
            for b, _ in pick_options(j, k):
                used_b.add(b)
        self.D = {}
        for i in nodes:
            j, k = i
            for b in sorted(used_b):
                col = [T[a][b] if a >= 1 else 0 for a in range(self.nloc)]
                tt = m.new_int_var(0, maxT, f"tt_{j}_{k}_{b}")
                m.add_element(self.mach[j, k], col, tt)
                D = m.new_int_var(0, 2 * H, f"D_{j}_{k}_{b}")
                m.add(D == self.arr[i] + tt)
                self.D[i, b] = D

        # ---- vehicle routes: multiple circuit, depot = node 0, at most nv routes
        idx = {n: t + 1 for t, n in enumerate(nodes)}
        arcs = []
        self.arc = {}
        depot_out = []
        for i in nodes:
            if self.pres[i] is not None:
                arcs.append((idx[i], idx[i], self.pres[i].negated()))
            # depot -> i : first transport of a vehicle
            l = m.new_bool_var(f"first_{i}")
            arcs.append((0, idx[i], l))
            depot_out.append(l)
            self.arc[None, i] = l
            for b, lit in pick_options(*i):
                lits = [l] + ([lit] if lit is not None else [])
                m.add(self.dep[i] >= T[0][b]).only_enforce_if(lits)
            l2 = m.new_bool_var(f"last_{i}")
            arcs.append((idx[i], 0, l2))
            self.arc[i, None] = l2
        m.add(sum(depot_out) <= self.nv)
        for i in nodes:
            for jn in nodes:
                if i == jn:
                    continue
                if i[0] == jn[0] and jn[1] < i[1]:
                    continue  # earlier operation of the same job is always transported earlier
                l = m.new_bool_var(f"a_{i}_{jn}")
                arcs.append((idx[i], idx[jn], l))
                self.arc[i, jn] = l
                for b, lit in pick_options(*jn):
                    lits = [l] + ([lit] if lit is not None else [])
                    m.add(self.dep[jn] >= self.D[i, b]).only_enforce_if(lits)
        m.add_multiple_circuit(arcs)

        # ---- objective
        self.cmax = m.new_int_var(0, H, "cmax")
        for j, J in enumerate(jobs):
            m.add(self.cmax >= self.E[j, len(J) - 1])
        # redundant: machine-independent job path bound and (weak) load bound
        for j, J in enumerate(jobs):
            m.add(self.cmax >= sum(min(a.values()) for a in J))
        m.minimize(self.cmax)

    # ----------------------------------------------------------------- hints
    def hint_values(self, ops, trs):
        """full assignment of all variables from a .sol (scaled ints). returns dict var->value"""
        jobs, T = self.jobs, self.T
        val = {}
        for (j, k), (q, s, e) in ops.items():
            for q2 in jobs[j][k]:
                val[self.x[j, k, q2]] = int(q2 == q)
            val[self.mach[j, k]] = q
            val[self.S[j, k]] = s
            val[self.E[j, k]] = e
        val[self.cmax] = max(e for (_, _, e) in ops.values())
        for i in self.nodes:
            j, k = i
            if i in trs:
                v, a, b, d, r = trs[i]
                val[self.dep[i]], val[self.arr[i]] = d, r
                if self.pres[i] is not None:
                    val[self.pres[i]] = 1
            else:
                e = ops[(j, k - 1)][2]
                val[self.dep[i]] = val[self.arr[i]] = e
                if self.pres[i] is not None:
                    val[self.pres[i]] = 0
            q = ops[i][0]
            for (ii, b), D in self.D.items():
                if ii == i:
                    val[D] = val[self.arr[i]] + (T[q][b])
        # route arcs
        nodes = self.nodes
        for a in self.arc.values():
            val[a] = 0
        chains = {}
        for i, (v, *_r) in trs.items():
            chains.setdefault(v, []).append(i)
        for v, L in chains.items():
            L.sort(key=lambda i: (trs[i][3], trs[i][4]))
            val[self.arc[None, L[0]]] = 1
            for a, b in zip(L, L[1:]):
                val[self.arc[a, b]] = 1
            val[self.arc[L[-1], None]] = 1
        return val

    # ------------------------------------------------------------- decoding
    def decode(self, get):
        """get(var)->int. returns (ops, trs) in scaled ints, with vehicles from circuit arcs"""
        jobs = self.jobs
        ops = {}
        for j, J in enumerate(jobs):
            for k in range(len(J)):
                ops[j, k] = (get(self.mach[j, k]), get(self.S[j, k]), get(self.E[j, k]))
        present = {}
        for i in self.nodes:
            j, k = i
            frm = 0 if k == 0 else ops[j, k - 1][0]
            if frm != ops[i][0]:
                present[i] = (frm, ops[i][0], get(self.dep[i]), get(self.arr[i]))
        succ = {}
        starts = []
        for (a, b), l in self.arc.items():
            if get(l):
                if a is None:
                    starts.append(b)
                elif b is not None:
                    succ[a] = b
        trs = {}
        assert len(starts) <= self.nv
        for v, s in enumerate(starts, 1):
            i = s
            while i is not None:
                if i in present:
                    trs[i] = (v,) + present[i]
                i = succ.get(i)
        assert set(trs) == set(present), "route decomposition lost a transport"
        return ops, trs



def greedy_solution(jobs, T, n_veh=2):
    """Simple dispatch heuristic (jobs in round-robin, machine/vehicle with earliest completion).
    Returns (ops, trs) in scaled ints; only used as an optional starting hint."""
    nj = len(jobs)
    nxt = [0] * nj
    jready = [0] * nj
    jloc = [0] * nj
    mfree = {}
    veh = [[0, 0] for _ in range(n_veh)]  # [location, free time]
    ops, trs = {}, {}
    remaining = sum(len(J) for J in jobs)
    while remaining:
        best = None
        for j in range(nj):
            k = nxt[j]
            if k >= len(jobs[j]):
                continue
            for q, p in jobs[j][k].items():
                if q == jloc[j] and k > 0:
                    s = max(jready[j], mfree.get(q, 0))
                    cand = (s + p, j, q, None)
                else:
                    bv = None
                    for v, (l, f) in enumerate(veh):
                        d = max(jready[j], f + T[l][jloc[j]])
                        r = d + T[jloc[j]][q]
                        if bv is None or r < bv[0]:
                            bv = (r, d, v)
                    s = max(bv[0], mfree.get(q, 0))
                    cand = (s + p, j, q, bv)
                if best is None or cand[0] < best[0]:
                    best = cand
        _, j, q, bv = best
        k = nxt[j]
        p = jobs[j][k][q]
        if bv is None:
            s = max(jready[j], mfree.get(q, 0))
        else:
            r, d, v = bv
            trs[j, k] = (v + 1, jloc[j], q, d, r)
            veh[v] = [q, r]
            s = max(r, mfree.get(q, 0))
        ops[j, k] = (q, s, s + p)
        mfree[q] = s + p
        jready[j], jloc[j] = s + p, q
        nxt[j] += 1
        remaining -= 1
    return ops, trs


def fmt_t(x):
    f = F(x, SCALE)
    return str(float(f))


def write_sol(path, name, ops, trs):
    mk = max(e for (_, _, e) in ops.values())
    lines = [f"instance {name}", f"makespan {fmt_t(mk)}",
             "# CP-SAT baseline (cpsat_fjspt.py). op: job op machine start end",
             "# tr: job op vehicle from to depart arrive"]
    for (j, k) in sorted(ops):
        q, s, e = ops[j, k]
        lines.append(f"op {j + 1} {k + 1} {q} {fmt_t(s)} {fmt_t(e)}")
    for (j, k), (v, a, b, d, r) in sorted(trs.items(), key=lambda kv: (kv[1][0], kv[1][3])):
        lines.append(f"tr {j + 1} {k + 1} {v} {a} {b} {fmt_t(d)} {fmt_t(r)}")
    open(path, "w").write("\n".join(lines) + "\n")


class Cb(cp_model.CpSolverSolutionCallback):
    def __init__(self, mdl, name, out, logf, t0):
        super().__init__()
        self.mdl, self.name, self.out, self.logf, self.t0 = mdl, name, out, logf, t0
        self.best = None

    def on_solution_callback(self):
        t = time.time() - self.t0
        obj, bnd = self.objective_value, self.best_objective_bound
        ops, trs = self.mdl.decode(self.value)
        self.best = (obj, ops, trs)
        if self.out:
            write_sol(self.out, self.name, ops, trs)
        line = f"{t:.2f},{obj / SCALE},{bnd / SCALE}"
        if self.logf:
            self.logf.write(line + "\n")
            self.logf.flush()
        print(f"[{t:8.2f}s] makespan {obj / SCALE}  bound {bnd / SCALE}", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("instance")
    ap.add_argument("--time", type=float, default=60)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--out", default=None, help="best solution (.sol), rewritten at each improvement")
    ap.add_argument("--log", default=None, help="CSV time,objective,bound")
    ap.add_argument("--hint", default=None, help=".sol file used as solution hint")
    ap.add_argument("--fix-hint", action="store_true",
                    help="debug: fix every variable to the hinted solution (feasibility test of the model)")
    ap.add_argument("--vehicles", type=int, default=2)
    ap.add_argument("--horizon", type=float, default=None, help="optional upper bound on makespan (original units)")
    ap.add_argument("--no-greedy", action="store_true",
                    help="do not seed the solver with the built-in greedy dispatch schedule when no --hint is given")
    ap.add_argument("--param", action="append", default=[], help="extra CP-SAT parameter key=value (repeatable)")
    ap.add_argument("--no-cumulative", action="store_true")
    ap.add_argument("--log-search", action="store_true")
    a = ap.parse_args()
    t_start = time.time()  # wall clock includes model building and hint completion

    name, jobs, T = load_instance(a.instance)
    hint = load_sol(a.hint) if a.hint else None
    if hint is None and not a.no_greedy:
        hint = greedy_solution(jobs, T, a.vehicles)
        print("greedy start makespan", max(e for (_, _, e) in hint[0].values()) / SCALE, flush=True)
    hz = None
    if a.horizon is not None:
        hz = to_int(a.horizon)
    elif hint:
        hz = max(e for (_, _, e) in hint[0].values())
    mdl = Model(jobs, T, a.vehicles, hz, cumulative=not a.no_cumulative)
    if hint:
        hv = mdl.hint_values(*hint)
        if a.fix_hint:
            for v, x in hv.items():
                mdl.m.add(v == x)
        else:
            # complete the hint (auxiliary variables) by propagating the partial assignment
            aux = mdl.m.clone()
            for v, x in hv.items():
                aux.add(aux.get_int_var_from_proto_index(v.index) == x)
            s2 = cp_model.CpSolver()
            s2.parameters.num_workers = 1
            s2.parameters.max_time_in_seconds = 60
            st2 = s2.solve(aux)
            if st2 in (cp_model.OPTIMAL, cp_model.FEASIBLE):
                for t in range(len(mdl.m.proto.variables)):
                    mdl.m.add_hint(mdl.m.get_int_var_from_proto_index(t), s2.value(aux.get_int_var_from_proto_index(t)))
                print("hint completed and accepted by the model", flush=True)
            else:
                print("WARNING: hint is infeasible for the model (", s2.status_name(st2), "); using partial hint", flush=True)
                for v, x in hv.items():
                    mdl.m.add_hint(v, x)
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = max(1.0, a.time - (time.time() - t_start))  # --time is total wall time
    solver.parameters.num_workers = a.workers
    solver.parameters.random_seed = a.seed
    solver.parameters.log_search_progress = a.log_search
    for kv in a.param:
        k, v = kv.split("=")
        setattr(solver.parameters, k, type(getattr(solver.parameters, k))(float(v)) if not isinstance(getattr(solver.parameters, k), str) else v)
    logf = open(a.log, "w") if a.log else None
    if logf:
        logf.write("wall_s,objective,bound\n")
    t0 = t_start
    cb = Cb(mdl, name, a.out, logf, t0)
    st = solver.solve(mdl.m, cb)
    print("status", solver.status_name(st), "wall", round(time.time() - t0, 1))
    if cb.best:
        print(f"best makespan {cb.best[0] / SCALE}  bound {solver.best_objective_bound / SCALE}")
        if logf:
            logf.write(f"{time.time() - t0:.2f},{cb.best[0] / SCALE},{solver.best_objective_bound / SCALE}\n")
    if logf:
        logf.close()
    return 0 if cb.best else 1


if __name__ == "__main__":
    sys.exit(main())
