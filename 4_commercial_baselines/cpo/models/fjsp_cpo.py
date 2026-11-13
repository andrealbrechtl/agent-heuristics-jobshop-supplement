#!/usr/bin/env python3
"""CP Optimizer, standard FJSP model (optional interval per eligible machine).
usage: fjsp_cpo.py instance.fjs out.sched time_limit workers seed trace.json"""
import sys
from docplex.cp.model import CpoModel
from common import run_search

inst, out, tl, wk, seed, trace = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
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
mdl = CpoModel(); OP = []; ALT = {}; per_m = {mu: [] for mu in range(m)}
for j, ops in enumerate(jobs):
    row = []
    for o, d in enumerate(ops):
        x = mdl.interval_var(name=f"o{j}_{o}"); row.append(x); alts = []
        for mm, p in d.items():
            y = mdl.interval_var(size=p, optional=True, name=f"a{j}_{o}_{mm}")
            alts.append((mm - base, y)); per_m[mm - base].append(y)
        mdl.add(mdl.alternative(x, [y for _, y in alts])); ALT[(j, o)] = alts
        if o: mdl.add(mdl.end_before_start(row[o-1], x))
    OP.append(row)
for mu, ys in per_m.items():
    if ys: mdl.add(mdl.no_overlap(ys))
mk = mdl.max(mdl.end_of(r[-1]) for r in OP)
mdl.add(mdl.minimize(mk))
best, tr, bound, status = run_search(mdl, mk, tl, wk, seed, trace)
if best is None: sys.exit("no solution")
name = inst.rsplit('/', 1)[-1].rsplit('.', 1)[0]
with open(out, "w") as f:
    f.write(f"instance {name}\njobs {n}\nmachines {m}\noperations {sum(len(x) for x in jobs)}\nmakespan {tr[-1][1]}\n")
    f.write(f"# CP Optimizer 22.2 (academic), {tl:g}s, {wk} workers, seed {seed}; status {status}; bound {bound}\n# machine ids are 0-based\n# job op machine start end\n")
    for (j, o), alts in ALT.items():
        for mu, y in alts:
            s = best.get_var_solution(y)
            if s.is_present():
                f.write(f"{j} {o} {mu} {s.get_start()} {s.get_end()}\n")
print(f"{name} makespan={tr[-1][1]} bound={bound} status={status} n_improve={len(tr)}")
