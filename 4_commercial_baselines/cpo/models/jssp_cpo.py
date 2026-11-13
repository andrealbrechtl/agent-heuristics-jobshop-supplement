#!/usr/bin/env python3
"""CP Optimizer, standard JSSP model. usage: jssp_cpo.py instance out.txt time_limit workers seed trace.json"""
import sys
from docplex.cp.model import CpoModel
from common import toks, run_search

inst, out, tl, wk, seed, trace = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
t = toks(inst); n, m = int(t[0]), int(t[1]); a = list(map(int, t[2:]))
MA = [[a[2*(j*m+o)] for o in range(m)] for j in range(n)]
DU = [[a[2*(j*m+o)+1] for o in range(m)] for j in range(n)]
mdl = CpoModel()
X = [[mdl.interval_var(size=DU[j][o], name=f"o{j}_{o}") for o in range(m)] for j in range(n)]
for j in range(n):
    for o in range(1, m):
        mdl.add(mdl.end_before_start(X[j][o-1], X[j][o]))
for mu in range(m):
    mdl.add(mdl.no_overlap([X[j][o] for j in range(n) for o in range(m) if MA[j][o] == mu]))
mk = mdl.max(mdl.end_of(X[j][m-1]) for j in range(n))
mdl.add(mdl.minimize(mk))
best, tr, bound, status = run_search(mdl, mk, tl, wk, seed, trace)
if best is None: sys.exit("no solution")
with open(out, "w") as f:
    f.write(f"# CP Optimizer 22.2 (academic), {tl:g}s, {wk} workers, seed {seed}; makespan {tr[-1][1]}; status {status}; bound {bound}\n")
    for j in range(n):
        f.write(" ".join(str(best.get_var_solution(X[j][o]).get_start()) for o in range(m)) + "\n")
print(f"{inst} makespan={tr[-1][1]} bound={bound} status={status} n_improve={len(tr)}")
