#!/usr/bin/env python3
"""Independent exact model (OR-tools CP-SAT) of the FJSPT with 2 vehicles, used to cross-check the
published MILP optima of HF2021 on the instance files built here (no code shared with the solver).
Usage: exact_cpsat.py INSTANCE.json [time_limit_s] [workers]   -> prints status, objective, bound"""
import json, sys
from ortools.sat.python import cp_model
I = json.load(open(sys.argv[1])); TL = float(sys.argv[2]) if len(sys.argv) > 2 else 600
W = int(sys.argv[3]) if len(sys.argv) > 3 else 4
V = 2; K = I["n_machines"]; trav = [[int(round(x)) for x in r] for r in I["travel"]]
assert all(abs(x - round(x)) < 1e-9 for r in I["travel"] for x in r)
jobs = I["jobs"]; H = sum(max(p for _, p in a) for J in jobs for a in J) + sum(max(map(max, trav)) for J in jobs for a in J) * 1 + 10
m = cp_model.CpModel(); flat = [x for r in trav for x in r]
S, E, M, mv, pres, dep, arr, orig = {}, {}, {}, {}, {}, {}, {}, {}
byM = {k: [] for k in range(1, K + 1)}
for j, J in enumerate(jobs):
    for k, alts in enumerate(J):
        S[j, k] = m.NewIntVar(0, H, ""); E[j, k] = m.NewIntVar(0, H, "")
        M[j, k] = m.NewIntVarFromDomain(cp_model.Domain.FromValues([a for a, _ in alts]), "")
        lits = []
        for a, p in alts:
            l = m.NewBoolVar(""); lits.append(l); m.Add(M[j, k] == a).OnlyEnforceIf(l); m.Add(M[j, k] != a).OnlyEnforceIf(l.Not())
            iv = m.NewOptionalIntervalVar(S[j, k], p, E[j, k], l, ""); byM[a].append(iv)
            m.Add(E[j, k] == S[j, k] + p).OnlyEnforceIf(l)
        m.AddExactlyOne(lits)
for k in byM: m.AddNoOverlap(byM[k])
T = []
for j, J in enumerate(jobs):
    for k in range(len(J)):
        o = m.NewIntVar(0, K, ""); orig[j, k] = o
        if k == 0: m.Add(o == 0); m.Add(S[j, k] >= 0)
        pr = m.NewBoolVar(""); pres[j, k] = pr
        if k > 0:
            m.Add(o == M[j, k - 1])
            m.Add(M[j, k - 1] != M[j, k]).OnlyEnforceIf(pr); m.Add(M[j, k - 1] == M[j, k]).OnlyEnforceIf(pr.Not())
            ready = E[j, k - 1]
        else:
            m.Add(M[j, k] != 0).OnlyEnforceIf(pr); m.Add(pr == 1); ready = None
        d = m.NewIntVar(0, H, ""); a_ = m.NewIntVar(0, H, ""); dep[j, k] = d; arr[j, k] = a_
        tv = m.NewIntVar(0, max(flat), ""); m.AddElement(o * (K + 1) + M[j, k], flat, tv)
        if ready is not None: m.Add(d >= ready).OnlyEnforceIf(pr)
        m.Add(a_ == d + tv).OnlyEnforceIf(pr); m.Add(S[j, k] >= a_).OnlyEnforceIf(pr)
        if ready is not None: m.Add(S[j, k] >= ready).OnlyEnforceIf(pr.Not())
        T.append((j, k))
# vehicle routing: one circuit per vehicle over depot(0) + transports; absent transports use self loops
ass = {}
for v in range(V):
    arcs = []; n = len(T)
    for i, t in enumerate(T):
        ass[v, t] = m.NewBoolVar(""); arcs.append((i + 1, i + 1, ass[v, t].Not()))
    arcs.append((0, 0, m.NewBoolVar("")))  # vehicle unused
    first = []; last = []
    for i, t in enumerate(T):
        f = m.NewBoolVar(""); l = m.NewBoolVar(""); first.append(f); last.append(l)
        arcs.append((0, i + 1, f)); arcs.append((i + 1, 0, l))
        m.Add(dep[t] >= 0).OnlyEnforceIf(f)
        tv0 = m.NewIntVar(0, max(flat), ""); m.AddElement(orig[t], trav[0], tv0)
        m.Add(dep[t] >= tv0).OnlyEnforceIf(f)
        for i2, t2 in enumerate(T):
            if i2 == i: continue
            b = m.NewBoolVar(""); arcs.append((i + 1, i2 + 1, b))
            e = m.NewIntVar(0, max(flat), ""); m.AddElement(M[t] * (K + 1) + orig[t2], flat, e)
            m.Add(dep[t2] >= arr[t] + e).OnlyEnforceIf(b)
    m.AddCircuit(arcs)
for t in T: m.Add(sum(ass[v, t] for v in range(V)) == pres[t])
cm = m.NewIntVar(0, H, "")
for j, J in enumerate(jobs): m.Add(cm >= E[j, len(J) - 1])
m.Minimize(cm)
s = cp_model.CpSolver(); s.parameters.max_time_in_seconds = TL; s.parameters.num_workers = W
st = s.Solve(m)
print(I["name"], s.StatusName(st), "obj", s.ObjectiveValue() if st in (cp_model.OPTIMAL, cp_model.FEASIBLE) else None, "bound", s.BestObjectiveBound(), f"{s.WallTime():.1f}s")
