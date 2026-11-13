"""Independent re-implementation of the FJSPT relaxation lower bound (HiGHS MILP via SciPy).

Written from the problem definition of Homayouni & Fontes (2021), without the agent's code:
jobs and V vehicles start at L/U (location 0); a transport into op k is needed unless op k-1 is
on the same machine; vehicles carry one job; between trips they drive empty from the last
drop-off to the next pickup. Relaxation: drop-offs are matched to later pickups as a
transportation problem (vehicle starts supply at most V pickups from L/U); ordering ignored.
Bounds: Cmax >= (L+E)/V + pmin ; Cmax >= machine load ; Cmax >= job processing + loaded travel.
"""
import json
import sys

import numpy as np
from scipy.optimize import Bounds, LinearConstraint, milp
from scipy.sparse import lil_matrix


def bound(path, V=2):
    I = json.load(open(path))
    jobs = [[{int(m): float(p) for m, p in alts} for alts in J] for J in I["jobs"]]
    T = np.array(I["travel"], dtype=float)
    nloc = len(T)
    var = {}
    lo, hi, integ, cost = [], [], [], []

    def add(key, l=0.0, h=1.0, i=0):
        var[key] = len(lo); lo.append(l); hi.append(h); integ.append(i); cost.append(0.0)
        return var[key]

    for j, J in enumerate(jobs):
        for k, a in enumerate(J):
            for m in a:
                add(("x", j, k, m), i=1)
            if k:
                for m in set(J[k - 1]) & set(a):
                    add(("s", j, k, m))
                for m in J[k - 1]:
                    for m2 in a:
                        if m != m2:
                            add(("w", j, k, m, m2))
    for a_ in range(nloc):
        for b_ in range(nloc):
            add(("f", a_, b_), h=np.inf)
        add(("g", a_), h=np.inf)
    cm = add(("C",), h=np.inf)
    cost[cm] = 1.0

    rows = []  # (dict coeffs, lo, hi)

    def row(c, l, h):
        rows.append((c, l, h))

    pmin = min(p for J in jobs for a in J for p in a.values())
    P = [dict() for _ in range(nloc)]  # pickups per location (linear expr)
    D = [dict() for _ in range(nloc)]  # drop-offs per location
    Lexpr = {}
    load = [dict() for _ in range(nloc)]

    def acc(d, key, c):
        d[var[key]] = d.get(var[key], 0.0) + c

    for j, J in enumerate(jobs):
        path = {}
        for k, a in enumerate(J):
            row({var[("x", j, k, m)]: 1.0 for m in a}, 1, 1)  # one machine per op
            for m, p in a.items():
                acc(load[m], ("x", j, k, m), p)
                acc(path, ("x", j, k, m), p)
                acc(D[m], ("x", j, k, m), 1.0)
            if k == 0:
                P[0][-1] = P[0].get(-1, 0.0) + 1.0  # constant: pickup at L/U
                for m in a:
                    acc(Lexpr, ("x", j, k, m), T[0][m]); acc(path, ("x", j, k, m), T[0][m])
                continue
            for m in J[k - 1]:
                acc(P[m], ("x", j, k - 1, m), 1.0)
            for m in set(J[k - 1]) & set(a):
                s = var[("s", j, k, m)]
                row({s: 1.0, var[("x", j, k - 1, m)]: -1.0}, -np.inf, 0)
                row({s: 1.0, var[("x", j, k, m)]: -1.0}, -np.inf, 0)
                row({s: 1.0, var[("x", j, k - 1, m)]: -1.0, var[("x", j, k, m)]: -1.0}, -1, np.inf)
                acc(P[m], ("s", j, k, m), -1.0); acc(D[m], ("s", j, k, m), -1.0)
            for m in J[k - 1]:
                for m2 in a:
                    if m != m2:
                        w = var[("w", j, k, m, m2)]
                        row({w: 1.0, var[("x", j, k - 1, m)]: -1.0, var[("x", j, k, m2)]: -1.0}, -1, np.inf)
                        acc(Lexpr, ("w", j, k, m, m2), T[m][m2]); acc(path, ("w", j, k, m, m2), T[m][m2])
        c = {cm: 1.0}
        for v_, x_ in path.items():
            c[v_] = c.get(v_, 0.0) - x_
        row(c, 0, np.inf)
    for m in range(1, nloc):
        c = {cm: 1.0}
        for v_, x_ in load[m].items():
            c[v_] = -x_
        row(c, 0, np.inf)
    # transportation: pickups at b served by drops at a, or by a vehicle start (<= V, from L/U)
    for b_ in range(nloc):
        c = {var[("f", a_, b_)]: 1.0 for a_ in range(nloc)}
        c[var[("g", b_)]] = 1.0
        const = 0.0
        for v_, x_ in P[b_].items():
            if v_ == -1:
                const += x_
            else:
                c[v_] = c.get(v_, 0.0) - x_
        row(c, const, const)
    for a_ in range(nloc):
        c = {var[("f", a_, b_)]: 1.0 for b_ in range(nloc)}
        for v_, x_ in D[a_].items():
            c[v_] = c.get(v_, 0.0) - x_
        row(c, -np.inf, 0)
    row({var[("g", b_)]: 1.0 for b_ in range(nloc)}, -np.inf, V)
    # vehicle work: C >= (L + E)/V + pmin
    c = {cm: 1.0}
    for v_, x_ in Lexpr.items():
        c[v_] = c.get(v_, 0.0) - x_ / V
    for a_ in range(nloc):
        for b_ in range(nloc):
            c[var[("f", a_, b_)]] = -T[a_][b_] / V
        c[var[("g", a_)]] = -T[0][a_] / V
    row(c, pmin, np.inf)

    A = lil_matrix((len(rows), len(lo)))
    rl, rh = [], []
    for r, (c, l, h) in enumerate(rows):
        for v_, x_ in c.items():
            A[r, v_] = x_
        rl.append(l); rh.append(h)
    res = milp(np.array(cost), constraints=LinearConstraint(A.tocsr(), rl, rh),
               integrality=np.array(integ), bounds=Bounds(lo, hi),
               options={"time_limit": 600, "mip_rel_gap": 0})
    return I.get("name", path), res.status, res.fun, res.message


if __name__ == "__main__":
    for p in sys.argv[1:]:
        name, st, val, msg = bound(p)
        print(f"{name}: bound {val:.2f}  (status {st}: {msg})", flush=True)
