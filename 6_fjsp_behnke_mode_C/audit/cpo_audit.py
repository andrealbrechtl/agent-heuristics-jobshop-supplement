#!/usr/bin/env python3
"""Feasibility audit of a schedule with IBM CP Optimizer (used only as an independent checker).

Builds the standard FJSP model of the paper's baseline (one optional interval per eligible machine,
alternative per operation, no_overlap per machine, end-before-start along each job), then fixes
every operation to the machine and start time of the schedule file and asks CP Optimizer whether
the fixed assignment is feasible.  Prints one line: AUDIT <instance> <file> CPO <status> makespan=<v>

usage: cpo_audit.py INSTANCE.fjs SCHEDULE.sched
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import check_fjsp as C
from docplex.cp.model import CpoModel
from docplex.cp.solver.solver import CpoSolver

CPO_EXE = os.environ.get("CPO_EXE", "/opt/ibm/ILOG/CPLEX_Studio_222_acad/cpoptimizer/bin/x86-64_linux/cpoptimizer")

inst_path, sched_path = sys.argv[1], sys.argv[2]
inst = C.read_instance(inst_path)
n, m, avg, jobs, _, ibase = inst
claim, meta, rows = C.read_schedule(sched_path)
# machine base of the schedule file: the one under which it is feasible for the independent checker
bases = [off for off in (0, 1) if not C.check(inst, rows, off)[0]]
off = bases[0] if len(bases) == 1 else 0           # if the checker rejects both, try base 0 anyway
sched = {(j, o): (mm - off, st, en) for (j, o, mm, st, en) in rows}

mdl = CpoModel()
OP = []
per_m = {mu: [] for mu in range(m)}
for j, ops in enumerate(jobs):
    row = []
    for o, d in enumerate(ops):
        x = mdl.interval_var(name=f"o{j}_{o}")
        row.append(x)
        alts = []
        for mm, p in d.items():
            y = mdl.interval_var(size=p, optional=True, name=f"a{j}_{o}_{mm}")
            alts.append((mm, y))
            per_m[mm].append(y)
        mdl.add(mdl.alternative(x, [y for _, y in alts]))
        if o:
            mdl.add(mdl.end_before_start(row[o - 1], x))
        # ---- fix to the schedule
        mach, st, en = sched[(j, o)]
        fixed = [y for mm, y in alts if mm == mach]
        if not fixed:
            print(f"AUDIT {os.path.basename(inst_path)} {sched_path} CPO INFEASIBLE(ineligible machine for op {(j,o)})")
            sys.exit(2)
        mdl.add(mdl.presence_of(fixed[0]) == 1)
        mdl.add(mdl.start_of(fixed[0]) == st)
        mdl.add(mdl.end_of(fixed[0]) == en)
    OP.append(row)
for mu, ys in per_m.items():
    if ys:
        mdl.add(mdl.no_overlap(ys))
mk = mdl.max(mdl.end_of(r[-1]) for r in OP)
mdl.add(mdl.minimize(mk))
solver = CpoSolver(mdl, execfile=CPO_EXE, TimeLimit=60, Workers=2, LogVerbosity="Quiet")
res = solver.solve()
status = res.get_solve_status()
val = res.get_objective_values()[0] if res.is_solution() else None
print(f"AUDIT {os.path.basename(inst_path).rsplit('.',1)[0]} {sched_path} CPO {status} makespan={val} claimed={claim}")
sys.exit(0 if res.is_solution() and val == claim else 1)
