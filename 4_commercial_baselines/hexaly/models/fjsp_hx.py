#!/usr/bin/env python3
"""Hexaly, standard FJSP model (the vendor example examples/flexiblejobshop/flexible_jobshop.py: one list
variable per machine with a partition, interval decisions). usage: fjsp_hx.py instance.fjs out.sched time_limit threads seed trace.json"""
import sys
from common import hx, run_search

inst, out, tl, th, seed, trace = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
t = open(inst).read().split(); n, m = int(t[0]), int(t[1]); i = 3 if '.' in t[2] else 2
jobs = []
for j in range(n):
    k = int(t[i]); i += 1; ops = []
    for o in range(k):
        e = int(t[i]); i += 1; d = {}
        for _ in range(e):
            d[int(t[i])] = int(t[i+1]); i += 2
        ops.append(d)
    jobs.append(ops)
base = 1 if min(mm for ops in jobs for d in ops for mm in d) == 1 else 0
INF = 1000000
tid = []; P = []
for j, ops in enumerate(jobs):
    row = []
    for d in ops:
        row.append(len(P)); P.append([d.get(mu + base, INF) for mu in range(m)])
    tid.append(row)
T = len(P); max_end = sum(max(v for v in r if v != INF) for r in P)
with hx.HexalyOptimizer() as opt:
    md = opt.model
    order = [md.list(T) for _ in range(m)]
    machines = md.array(order)
    md.constraint(md.partition(machines))
    for x in range(T):
        for mu in range(m):
            if P[x][mu] == INF: md.constraint(md.not_(md.contains(order[mu], x)))
    tm = [md.find(machines, x) for x in range(T)]
    parr = md.array(P)
    tasks = [md.interval(0, max_end) for _ in range(T)]
    for x in range(T): md.constraint(md.length(tasks[x]) == md.at(parr, x, tm[x]))
    ta = md.array(tasks)
    for row in tid:
        for a, b in zip(row, row[1:]): md.constraint(tasks[a] < tasks[b])
    for mu in range(m):
        seq = order[mu]
        lam = md.lambda_function((lambda sq: (lambda q: ta[sq[q]] < ta[sq[q + 1]]))(seq))
        md.constraint(md.and_(md.range(0, md.count(seq) - 1), lam))
    mk = md.max([md.end(x) for x in tasks])
    md.minimize(mk); md.close()
    tr, bound, status = run_search(opt, mk, tl, th, seed, trace)
    if not tr: sys.exit("no solution")
    name = inst.rsplit('/', 1)[-1].rsplit('.', 1)[0]
    with open(out, "w") as f:
        f.write(f"instance {name}\njobs {n}\nmachines {m}\noperations {T}\nmakespan {tr[-1][1]}\n")
        f.write(f"# Hexaly {hx.HxVersion.get_version()} (academic), {tl:g}s, {th} threads, seed {seed}; status {status}; bound {bound}\n# machine ids are 0-based\n# job op machine start end\n")
        for j, row in enumerate(tid):
            for o, x in enumerate(row):
                v = tasks[x].value
                f.write(f"{j} {o} {tm[x].value} {v.start()} {v.end()}\n")
print(f"{name} makespan={tr[-1][1]} bound={bound} status={status} n_improve={len(tr)}")
