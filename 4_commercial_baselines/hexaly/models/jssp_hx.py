#!/usr/bin/env python3
"""Hexaly, standard JSSP model (the vendor example examples/jobshop/jobshop.py: interval decisions,
one list variable per machine). usage: jssp_hx.py instance out.txt time_limit threads seed trace.json"""
import sys
from common import hx, toks, run_search

inst, out, tl, th, seed, trace = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
t = toks(inst); n, m = int(t[0]), int(t[1]); a = list(map(int, t[2:]))
MA = [[a[2*(j*m+o)] for o in range(m)] for j in range(n)]
DU = [[a[2*(j*m+o)+1] for o in range(m)] for j in range(n)]
assert all(sorted(r) == list(range(m)) for r in MA), "each job must visit each machine once"
PT = [[0]*m for _ in range(n)]
for j in range(n):
    for o in range(m): PT[j][MA[j][o]] = DU[j][o]
max_end = sum(map(sum, DU))
with hx.HexalyOptimizer() as opt:
    md = opt.model
    tasks = [[md.interval(0, max_end) for mu in range(m)] for j in range(n)]
    for j in range(n):
        for mu in range(m): md.constraint(md.length(tasks[j][mu]) == PT[j][mu])
    ta = md.array(tasks)
    for j in range(n):
        for k in range(m - 1): md.constraint(tasks[j][MA[j][k]] < tasks[j][MA[j][k + 1]])
    order = [md.list(n) for mu in range(m)]
    for mu in range(m):
        seq = order[mu]
        md.constraint(md.eq(md.count(seq), n))
        lam = md.lambda_function((lambda sq, mc: (lambda i: md.lt(md.at(ta, sq[i], mc), md.at(ta, sq[i + 1], mc))))(seq, mu))
        md.constraint(md.and_(md.range(0, n - 1), lam))
    mk = md.max([md.end(tasks[j][MA[j][m - 1]]) for j in range(n)])
    md.minimize(mk); md.close()
    tr, bound, status = run_search(opt, mk, tl, th, seed, trace)
    if not tr: sys.exit("no solution")
    with open(out, "w") as f:
        f.write(f"# Hexaly {hx.HxVersion.get_version()} (academic), {tl:g}s, {th} threads, seed {seed}; makespan {tr[-1][1]}; status {status}; bound {bound}\n")
        for j in range(n):
            f.write(" ".join(str(tasks[j][MA[j][o]].value.start()) for o in range(m)) + "\n")
print(f"{inst} makespan={tr[-1][1]} bound={bound} status={status} n_improve={len(tr)}")
