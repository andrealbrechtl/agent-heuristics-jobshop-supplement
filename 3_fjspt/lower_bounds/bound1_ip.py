#!/usr/bin/env python3
"""Bound (1): Leandro's integer-program bound, re-implemented independently.

Relaxation (all quantities in scaled integer units, C = gran * c):
  * every op chooses one eligible machine (x); each job is a path through its ops' machines
    (pair variables z[j,o,a,b] for consecutive ops, network-flow linking);
  * a loaded trip exists into op o iff o == 0 (from location 0) or machine(o-1) != machine(o);
  * aggregated empty-move flow e[a,b] from drop locations a (machines) to pickup locations b,
    vehicle starts s[b] (first pickup at b, empty move 0->b), vehicle ends f[a];
    conservation: pickups(b) = sum_a e[a,b] + s[b];  drops(a) = sum_b e[a,b] + f[a];
  * number of active vehicles  sum s = sum f = 1 + u  (u = 1: both vehicles move);
    with --force-all-vehicles: u = 1 (Leandro's "|V| routes start at 0");
  * one "last trip" per active vehicle (lst[j,o,b] <= trip into (j,o) on b, sum_jo lst[.,.,b] = f[b]);
    its tail q = p(j,o,b) + R(j,o), R = processing + loaded travel of ops after o along the chosen path;
  * vehicle bound:   |V| C >= W_total + sum q      (always valid)
                     C >= W_total + sum q   if u = 0  (only one vehicle moves)
  * machine bound:   C >= (h_k + t_k) w_k + l_k,  w_k = 1 if machine k used;
  * job bound:       C >= processing + loaded travel along the job's path.

Solved with OR-Tools CP-SAT and with IBM CPLEX (MIP, LP file + interactive optimizer).
Usage: bound1_ip.py INSTANCE.json --solver cpsat|cplex [--time 600] [--threads 8]
                    [--force-all-vehicles] [--sp-empty] [--json out.json]
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from fractions import Fraction as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import Inst, CPLEX_BIN, fmt  # noqa: E402

NV = 2


class Lin:
    """Tiny solver-neutral MIP container (all-integer coefficients)."""

    def __init__(self):
        self.vars = {}      # name -> (lb, ub, kind)  kind in B, I
        self.cons = []      # (name, [(coef, var)], sense, rhs)

    def var(self, name, lb=0, ub=1, kind="B"):
        self.vars[name] = (lb, ub, kind)
        return name

    def add(self, terms, sense, rhs, name=None):
        terms = [(c, v) for c, v in terms if c != 0]
        self.cons.append((name or f"r{len(self.cons)}", terms, sense, rhs))


def floyd(t):
    n = len(t)
    d = [row[:] for row in t]
    for a in range(n):
        d[a][a] = 0
    for k in range(n):
        for a in range(n):
            for b in range(n):
                if d[a][k] + d[k][b] < d[a][b]:
                    d[a][b] = d[a][k] + d[k][b]
    return d


def build(I, force_all=False, sp_empty=False, ablate=()):
    """ablate: 'qpmin' -> q = global min processing time per active vehicle (no job tail);
    'nohead' -> machine rows without heads/tails (load only)."""
    L = Lin()
    te = floyd(I.t) if sp_empty else I.te
    t0 = te[0]  # initial empty move 0 -> b
    locs = range(I.nloc)
    M = I.machines
    g = I.gran
    x, z = {}, {}
    trip_terms = {}   # (j,o,b) -> list of vars whose sum = trip into (j,o) on b
    pick = {b: [] for b in locs}   # loaded departures from b: list of (coef, var) / constants
    pick_const = {b: 0 for b in locs}
    drop = {a: [] for a in locs}
    loaded = []       # (t, var)
    proc = {k: [] for k in M}
    jobload = {}
    for j, J in enumerate(I.jobs):
        jt = []
        for o, alts in enumerate(J):
            for k, p in alts.items():
                x[j, o, k] = L.var(f"x_{j}_{o}_{k}")
                proc[k].append((p, x[j, o, k]))
                jt.append((p, x[j, o, k]))
            L.add([(1, x[j, o, k]) for k in alts], "=", 1)
            if o == 0:
                pick_const[0] += 1
                for b in alts:
                    trip_terms[j, 0, b] = [x[j, 0, b]]
                    drop[b].append((1, x[j, 0, b]))
                    loaded.append((I.t[0][b], x[j, 0, b]))
                    jt.append((I.t[0][b], x[j, 0, b]))
            else:
                prev = J[o - 1]
                for a in prev:
                    for b in alts:
                        z[j, o, a, b] = L.var(f"z_{j}_{o}_{a}_{b}")
                for a in prev:
                    L.add([(1, z[j, o, a, b]) for b in alts] + [(-1, x[j, o - 1, a])], "=", 0)
                for b in alts:
                    L.add([(1, z[j, o, a, b]) for a in prev] + [(-1, x[j, o, b])], "=", 0)
                    trip_terms[j, o, b] = [z[j, o, a, b] for a in prev if a != b]
                for a in prev:
                    for b in alts:
                        if a != b:
                            pick[a].append((1, z[j, o, a, b]))
                            drop[b].append((1, z[j, o, a, b]))
                            loaded.append((I.t[a][b], z[j, o, a, b]))
                            jt.append((I.t[a][b], z[j, o, a, b]))
        jobload[j] = jt

    ntrips_max = sum(len(J) for J in I.jobs)
    e = {}
    for a in M:
        for b in locs:
            e[a, b] = L.var(f"e_{a}_{b}", 0, ntrips_max, "I")
    s = {b: L.var(f"s_{b}", 0, NV, "I") for b in locs}
    f = {a: L.var(f"f_{a}", 0, NV, "I") for a in M}
    u = L.var("u")
    if force_all:
        L.add([(1, u)], "=", 1)
    # conservation
    for b in locs:
        L.add(pick[b] + [(-1, e[a, b]) for a in M] + [(-1, s[b])], "=", -pick_const[b])
    for a in M:
        L.add(drop[a] + [(-1, e[a, b]) for b in locs] + [(-1, f[a])], "=", 0)
    L.add([(1, s[b]) for b in locs] + [(-1, u)], "=", 1)
    L.add([(1, f[a]) for a in M] + [(-1, u)], "=", 1)

    # last trips and tails
    lst = {}
    qterms = []
    for j, J in enumerate(I.jobs):
        for o, alts in enumerate(J):
            for b, p in alts.items():
                tt = trip_terms[j, o, b]
                if not tt:
                    continue
                lst[j, o, b] = L.var(f"l_{j}_{o}_{b}")
                L.add([(1, lst[j, o, b])] + [(-1, v) for v in tt], "<=", 0)
                qterms.append((p, lst[j, o, b]) if "qpmin" not in ablate else (0, lst[j, o, b]))
            if o < len(J) - 1:
                R = []
                Mjo = 0
                for o2 in range(o + 1, len(J)):
                    for k, p in J[o2].items():
                        R.append((p, x[j, o2, k]))
                    Mjo += max(J[o2].values())
                    if True:
                        prev = J[o2 - 1]
                        mt = 0
                        for a in prev:
                            for b in J[o2]:
                                if a != b:
                                    R.append((I.t[a][b], z[j, o2, a, b]))
                                    mt = max(mt, I.t[a][b])
                        Mjo += mt
                G = L.var(f"G_{j}_{o}", 0, Mjo, "I")
                # G >= R - Mjo*(1 - sum_b lst[j,o,b])  <=>  G - R - Mjo*sum lst >= -Mjo
                L.add([(1, G)] + [(-c, v) for c, v in R] + [(-Mjo, lst[j, o, b]) for b in alts if (j, o, b) in lst],
                      ">=", -Mjo)
                if "qpmin" not in ablate:
                    qterms.append((1, G))
            # an op's trip can be last for at most one vehicle (implied by lst <= trip, single trip)
    for a in M:
        L.add([(1, lst[k]) for k in lst if k[2] == a] + [(-1, f[a])], "=", 0)

    if "qpmin" in ablate:
        pmin_all = min(p for J in I.jobs for alts in J for p in alts.values())
        qterms.append((pmin_all, u))
        qterms.append((pmin_all, "one"))
        L.var("one", 1, 1, "I")
    UBc = 10 ** 7
    c = L.var("c", 0, UBc, "I")
    W = loaded + [(te[a][b], e[a, b]) for a in M for b in locs] + [(t0[b], s[b]) for b in locs]
    rhs_terms = W + qterms
    # |V| * g * c >= W + q
    L.add([(NV * g, c)] + [(-cc, v) for cc, v in rhs_terms], ">=", 0, "veh_all")
    # single-vehicle case: g*c >= W + q - Mbig*u
    tmax = max(max(r) for r in I.t)
    pmax_total = sum(max(alts.values()) for J in I.jobs for alts in J)
    # RHS <= loaded + empty + start + 2*(p + R) <= ntrips*tmax*2 + tmax + 2*(pmax_total + ntrips*tmax)
    Mbig = 4 * ntrips_max * tmax + tmax + 2 * pmax_total + 1
    L.add([(g, c), (Mbig, u)] + [(-cc, v) for cc, v in rhs_terms], ">=", 0, "veh_one")
    # machines
    h, tl = {}, {}
    for j in range(I.n):
        H, T = I.heads(j), I.tails(j)
        for o, alts in enumerate(I.jobs[j]):
            for k in alts:
                h[k] = min(h.get(k, 10 ** 9), H[o][k])
                tl[k] = min(tl.get(k, 10 ** 9), T[o][k])
    for k in M:
        w = L.var(f"w_{k}")
        for _, v in proc[k]:
            L.add([(1, w), (-1, v)], ">=", 0)
        hk = 0 if "nohead" in ablate else h[k] + tl[k]
        L.add([(g, c), (-hk, w)] + [(-p, v) for p, v in proc[k]], ">=", 0, f"mach_{k}")
    for j in range(I.n):
        L.add([(g, c)] + [(-cc, v) for cc, v in jobload[j]], ">=", 0, f"job_{j}")
    L.obj = c
    L.meta = dict(h=h, tl=tl)
    return L


def solve_cpsat(L, tlim, threads):
    from ortools.sat.python import cp_model
    m = cp_model.CpModel()
    V = {n: m.NewIntVar(lb, ub, n) for n, (lb, ub, kind) in L.vars.items()}
    for name, terms, sense, rhs in L.cons:
        ex = sum(c * V[v] for c, v in terms)
        if sense == "=":
            m.Add(ex == rhs)
        elif sense == ">=":
            m.Add(ex >= rhs)
        else:
            m.Add(ex <= rhs)
    m.Minimize(V[L.obj])
    sv = cp_model.CpSolver()
    sv.parameters.max_time_in_seconds = tlim
    sv.parameters.num_workers = threads
    t0 = time.time()
    st = sv.Solve(m)
    rt = time.time() - t0
    status = sv.StatusName(st)
    obj = int(sv.ObjectiveValue()) if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else None
    bound = int(round(sv.BestObjectiveBound()))
    sol = {n: int(sv.Value(V[n])) for n in L.vars} if obj is not None else None
    return dict(solver="cpsat", status=status, obj=obj, bound=bound, runtime=rt, optimal=(status == "OPTIMAL"), sol=sol)


def write_lp(L, path):
    def term(c, v):
        return f"{'+' if c >= 0 else '-'} {abs(c)} {v}"
    with open(path, "w") as fh:
        fh.write(f"Minimize\n obj: {L.obj}\nSubject To\n")
        for name, terms, sense, rhs in L.cons:
            s = {"=": "=", ">=": ">=", "<=": "<="}[sense]
            toks = [term(c, v) for c, v in terms]
            body = "\n   ".join(" ".join(toks[i:i + 8]) for i in range(0, len(toks), 8))
            fh.write(f" {name}: {body} {s} {rhs}\n")
        fh.write("Bounds\n")
        for n, (lb, ub, kind) in L.vars.items():
            if kind == "I":
                fh.write(f" {lb} <= {n} <= {ub}\n")
        fh.write("Binaries\n")
        for n, (lb, ub, kind) in L.vars.items():
            if kind == "B":
                fh.write(f" {n}\n")
        fh.write("Generals\n")
        for n, (lb, ub, kind) in L.vars.items():
            if kind == "I":
                fh.write(f" {n}\n")
        fh.write("End\n")


def solve_cplex(L, tlim, threads):
    d = tempfile.mkdtemp(prefix="b1cplex_")
    lp = os.path.join(d, "m.lp")
    sol = os.path.join(d, "m.sol")
    write_lp(L, lp)
    cmds = [f"read {lp}", f"set threads {threads}", f"set timelimit {tlim}",
            "set mip tolerances mipgap 0", "set mip tolerances absmipgap 0",
            "set mip tolerances integrality 0", "set emphasis numerical y",
            "optimize", f"write {sol}", "display solution quality", "quit"]
    t0 = time.time()
    out = subprocess.run([CPLEX_BIN, "-c"] + cmds, capture_output=True, text=True, cwd=d).stdout
    open(os.path.join(d, "cplex.log"), "w").write(out)
    rt = time.time() - t0
    m = re.search(r"MIP - (.*?):\s+Objective =\s+([-0-9.e+]+)", out)
    status = m.group(1).strip() if m else "UNKNOWN"
    obj = float(m.group(2)) if m else None
    bnd = None
    mb = re.findall(r"Current MIP best bound =\s+([-0-9.e+]+)", out)
    if mb:
        bnd = float(mb[-1])
    if status.startswith("Integer optimal") and bnd is None:
        bnd = obj
    vals = {}
    if os.path.exists(sol):
        for mm in re.finditer(r'<variable name="([^"]+)" index="\d+" value="([^"]+)"', open(sol).read()):
            vals[mm.group(1)] = round(float(mm.group(2)))
    optimal = status.startswith("Integer optimal")
    return dict(solver="cplex", status=status, obj=None if obj is None else round(obj), bound=bnd,
                runtime=rt, optimal=optimal, sol=vals or None, log_tail=out[-3000:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inst")
    ap.add_argument("--solver", default="cpsat", choices=["cpsat", "cplex"])
    ap.add_argument("--time", type=float, default=600)
    ap.add_argument("--threads", type=int, default=8)
    ap.add_argument("--force-all-vehicles", action="store_true")
    ap.add_argument("--sp-empty", action="store_true")
    ap.add_argument("--ablate", default="", help="comma list: qpmin,nohead")
    ap.add_argument("--json")
    a = ap.parse_args()
    I = Inst(a.inst)
    L = build(I, a.force_all_vehicles, a.sp_empty, tuple(x for x in a.ablate.split(",") if x))
    r = (solve_cpsat if a.solver == "cpsat" else solve_cplex)(L, a.time, a.threads)
    g = I.gran
    # proven bound on c (integer):  ceil(bound - 1e-6) for CPLEX's float bound
    import math
    cb = r["bound"]
    proven_c = math.ceil(cb - 1e-6) if cb is not None else None
    lb = F(proven_c * g, I.scale) if proven_c is not None else None
    inc = F(r["obj"] * g, I.scale) if r["obj"] is not None else None
    sol = r.pop("sol")
    extra = {}
    if sol:
        W = sum(I.te[int(k.split('_')[1])][int(k.split('_')[2])] * v for k, v in sol.items() if k.startswith("e_"))
        extra = dict(u=sol.get("u"), starts={k: v for k, v in sol.items() if k.startswith("s_") and v},
                     ends={k: v for k, v in sol.items() if k.startswith("f_") and v},
                     empty_into_0=sum(v for k, v in sol.items() if k.startswith("e_") and k.endswith("_0")),
                     empty_time=fmt(F(W, I.scale)))
    out = dict(instance=I.name, variant=("force" if a.force_all_vehicles else "sound") + ("_sp" if a.sp_empty else "") + ("_abl-" + a.ablate.replace(",", "-") if a.ablate else ""),
               solver=r["solver"], status=r["status"], optimal=r["optimal"],
               lb=fmt(lb) if lb is not None else None, incumbent=fmt(inc) if inc is not None else None,
               runtime=round(r["runtime"], 2), nvars=len(L.vars), ncons=len(L.cons), **extra)
    print(json.dumps(out))
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
