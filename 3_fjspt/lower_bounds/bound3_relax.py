#!/usr/bin/env python3
"""Bound (3): flexible job shop with assignment-dependent time lags (vehicles dropped).

  * op o of job j on one eligible machine k, duration p_ojk, unary machines;
  * first op on k starts >= travel[0][k];
  * op o (on b) starts >= end of op o-1 (on a) + travel[a][b]  (0 if a == b).
No vehicle capacity, no empty moves. Every feasible FJSPT schedule satisfies these constraints,
so the optimum (or any proven dual bound) is a lower bound.

Solvers: OR-Tools CP-SAT  |  IBM CP Optimizer (docplex.cp, optional intervals + alternative;
lags as end_before_start between optional alternatives, active only if both are present).
Usage: bound3_relax.py INSTANCE.json --solver cpsat|cpo [--time 600] [--threads 8] [--json out]
                       [--hint SOLUTION.sol]
"""
import argparse
import json
import math
import os
import sys
import time
from fractions import Fraction as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import Inst, CPO_BIN, fmt  # noqa: E402


def horizon(I):
    return sum(max(a.values()) for J in I.jobs for a in J) + sum(len(J) for J in I.jobs) * max(max(r) for r in I.t)


def read_hint(path, I):
    ops = {}
    for line in open(path):
        t = line.split("#", 1)[0].split()
        if t and t[0] == "op":
            ops[int(t[1]) - 1, int(t[2]) - 1] = (int(t[3]), int(F(t[4]) * I.scale))
    return ops


def solve_cpsat(I, tlim, threads, hint=None):
    from ortools.sat.python import cp_model
    m = cp_model.CpModel()
    H = horizon(I)
    S, E, X, ivs = {}, {}, {}, {k: [] for k in I.machines}
    for j, J in enumerate(I.jobs):
        for o, alts in enumerate(J):
            S[j, o] = m.NewIntVar(0, H, f"S{j}_{o}")
            E[j, o] = m.NewIntVar(0, H, f"E{j}_{o}")
            for k, p in alts.items():
                X[j, o, k] = m.NewBoolVar(f"x{j}_{o}_{k}")
                ivs[k].append(m.NewOptionalFixedSizeIntervalVar(S[j, o], p, X[j, o, k], f"I{j}_{o}_{k}"))
            m.AddExactlyOne(X[j, o, k] for k in alts)
            m.Add(E[j, o] == S[j, o] + sum(p * X[j, o, k] for k, p in alts.items()))
            if o == 0:
                m.Add(S[j, 0] >= sum(I.t[0][k] * X[j, 0, k] for k in alts))
            else:
                m.Add(S[j, o] >= E[j, o - 1])
                for a in J[o - 1]:
                    for b in alts:
                        if a != b and I.t[a][b] > 0:
                            m.Add(S[j, o] >= E[j, o - 1] + I.t[a][b]).OnlyEnforceIf([X[j, o - 1, a], X[j, o, b]])
    for k in I.machines:
        m.AddNoOverlap(ivs[k])
    C = m.NewIntVar(0, H, "C")
    for j, J in enumerate(I.jobs):
        m.Add(C >= E[j, len(J) - 1])
    m.Minimize(C)
    if hint:
        for (j, o), (k, s) in hint.items():
            m.AddHint(S[j, o], s)
            for k2 in I.jobs[j][o]:
                m.AddHint(X[j, o, k2], int(k2 == k))
    sv = cp_model.CpSolver()
    sv.parameters.max_time_in_seconds = tlim
    sv.parameters.num_workers = threads
    t0 = time.time()
    st = sv.Solve(m)
    rt = time.time() - t0
    obj = int(sv.ObjectiveValue()) if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else None
    return dict(status=sv.StatusName(st), optimal=st == cp_model.OPTIMAL, obj=obj,
                bound=int(math.ceil(sv.BestObjectiveBound() - 1e-9)), runtime=rt)


def solve_cpo(I, tlim, threads, hint=None):
    from docplex.cp.model import CpoModel
    mdl = CpoModel()
    H = horizon(I)
    master, alt = {}, {}
    bym = {k: [] for k in I.machines}
    for j, J in enumerate(I.jobs):
        for o, alts in enumerate(J):
            master[j, o] = mdl.interval_var(name=f"M{j}_{o}")
            for k, p in alts.items():
                st = (I.t[0][k], H) if o == 0 else (0, H)
                alt[j, o, k] = mdl.interval_var(size=p, optional=True, start=st, end=(0, H), name=f"A{j}_{o}_{k}")
                bym[k].append(alt[j, o, k])
            mdl.add(mdl.alternative(master[j, o], [alt[j, o, k] for k in alts]))
            if o > 0:
                mdl.add(mdl.end_before_start(master[j, o - 1], master[j, o]))
                for a in J[o - 1]:
                    for b in alts:
                        if a != b and I.t[a][b] > 0:
                            # enforced only when both optional intervals are present
                            mdl.add(mdl.end_before_start(alt[j, o - 1, a], alt[j, o, b], I.t[a][b]))
    for k in I.machines:
        mdl.add(mdl.no_overlap(bym[k]))
    mdl.add(mdl.minimize(mdl.max([mdl.end_of(master[j, len(J) - 1]) for j, J in enumerate(I.jobs)])))
    if hint:
        from docplex.cp.solution import CpoModelSolution
        hs = CpoModelSolution()
        for (j, o), (k, s) in hint.items():
            p = I.jobs[j][o][k]
            hs.add_interval_var_solution(alt[j, o, k], presence=True, start=s, end=s + p)
            hs.add_interval_var_solution(master[j, o], presence=True, start=s, end=s + p)
            for k2 in I.jobs[j][o]:
                if k2 != k:
                    hs.add_interval_var_solution(alt[j, o, k2], presence=False)
        mdl.set_starting_point(hs)
    t0 = time.time()
    res = mdl.solve(execfile=CPO_BIN, TimeLimit=tlim, Workers=threads, LogVerbosity="Quiet",
                    RelativeOptimalityTolerance=0, OptimalityTolerance=0)
    rt = time.time() - t0
    status = res.get_solve_status()
    obj = res.get_objective_values()[0] if res.is_solution() else None
    bnd = res.get_objective_bounds()[0] if res.get_objective_bounds() else None
    optimal = status == "Optimal"
    if optimal and bnd is not None and obj is not None and bnd < obj:
        bnd = obj  # CPO reports optimal only when proven (tolerance 0)
    return dict(status=status, optimal=optimal, obj=None if obj is None else int(obj),
                bound=None if bnd is None else int(math.ceil(bnd - 1e-9)), runtime=rt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inst")
    ap.add_argument("--solver", default="cpsat", choices=["cpsat", "cpo"])
    ap.add_argument("--time", type=float, default=600)
    ap.add_argument("--threads", type=int, default=8)
    ap.add_argument("--hint")
    ap.add_argument("--json")
    a = ap.parse_args()
    I = Inst(a.inst)
    hint = read_hint(a.hint, I) if a.hint else None
    r = (solve_cpsat if a.solver == "cpsat" else solve_cpo)(I, a.time, a.threads, hint)
    lb = I.ceil_gran(r["bound"]) if r["bound"] is not None else None
    out = dict(instance=I.name, solver=a.solver, status=r["status"], optimal=r["optimal"],
               lb=fmt(lb) if lb is not None else None,
               incumbent=fmt(F(r["obj"], I.scale)) if r["obj"] is not None else None,
               runtime=round(r["runtime"], 2), hinted=bool(hint))
    print(json.dumps(out))
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
