#!/usr/bin/env python3
"""Feasibility audit of a schedule with Hexaly Optimizer (used only as an independent checker).

Builds the vendor's standard FJSP model (the paper's baseline: one list per machine with a
partition, interval decisions, machine-dependent lengths, precedence along jobs, consecutive
tasks of a machine must not overlap), then fixes every operation to the machine and the start/end
of the schedule file (constraints on the model and the same values as the initial solution) and
asks Hexaly whether a feasible solution exists.  Prints one line:
AUDIT <instance> <file> HEXALY <status> makespan=<v>

usage: hexaly_audit.py INSTANCE.fjs SCHEDULE.sched
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, "/opt/hexaly_15_0/bin/python")
import check_fjsp as C
import hexaly.optimizer as hx

inst_path, sched_path = sys.argv[1], sys.argv[2]
inst = C.read_instance(inst_path)
n, m, avg, jobs, _, ibase = inst
claim, meta, rows = C.read_schedule(sched_path)
bases = [off for off in (0, 1) if not C.check(inst, rows, off)[0]]
off = bases[0] if len(bases) == 1 else 0
sched = {(j, o): (mm - off, st, en) for (j, o, mm, st, en) in rows}

INF = 1000000
tid, P, key = [], [], []
for j, ops in enumerate(jobs):
    row = []
    for o, d in enumerate(ops):
        row.append(len(P)); key.append((j, o))
        P.append([d.get(mu, INF) for mu in range(m)])
    tid.append(row)
T = len(P)
max_end = sum(max(v for v in r if v != INF) for r in P)
with hx.HexalyOptimizer() as opt:
    md = opt.model
    order = [md.list(T) for _ in range(m)]
    machines = md.array(order)
    md.constraint(md.partition(machines))
    for x in range(T):
        for mu in range(m):
            if P[x][mu] == INF:
                md.constraint(md.not_(md.contains(order[mu], x)))
    tm = [md.find(machines, x) for x in range(T)]
    parr = md.array(P)
    tasks = [md.interval(0, max_end) for _ in range(T)]
    for x in range(T):
        md.constraint(md.length(tasks[x]) == md.at(parr, x, tm[x]))
    ta = md.array(tasks)
    for row in tid:
        for a, b in zip(row, row[1:]):
            md.constraint(tasks[a] < tasks[b])
    for mu in range(m):
        seq = order[mu]
        lam = md.lambda_function((lambda sq: (lambda q: ta[sq[q]] < ta[sq[q + 1]]))(seq))
        md.constraint(md.and_(md.range(0, md.count(seq) - 1), lam))
    # ---- fix to the schedule
    bad = None
    for x in range(T):
        mach, st, en = sched[key[x]]
        if P[x][mach] == INF or en - st != P[x][mach]:
            bad = f"op {key[x]} machine {mach} duration {en-st}"
            break
        md.constraint(md.contains(order[mach], x))
        md.constraint(md.start(tasks[x]) == st)
        md.constraint(md.end(tasks[x]) == en)
    if bad:
        print(f"AUDIT {os.path.basename(inst_path).rsplit('.',1)[0]} {sched_path} HEXALY INFEASIBLE({bad})")
        sys.exit(2)
    mk = md.max([md.end(x) for x in tasks])
    md.minimize(mk)
    md.close()
    # initial solution = the schedule itself
    per = {mu: [] for mu in range(m)}
    for x in range(T):
        mach, st, en = sched[key[x]]
        per[mach].append((st, x))
    for mu in range(m):
        order[mu].value.clear()
        for st, x in sorted(per[mu]):
            order[mu].value.add(x)
    for x in range(T):
        mach, st, en = sched[key[x]]
        tasks[x].value = hx.HxInterval(st, en)
    opt.param.time_limit = 20
    opt.param.nb_threads = 2
    opt.param.verbosity = 0
    opt.solve()
    st = opt.solution.status
    ok = st in (hx.HxSolutionStatus.FEASIBLE, hx.HxSolutionStatus.OPTIMAL)
    val = mk.value if ok else None
    print(f"AUDIT {os.path.basename(inst_path).rsplit('.',1)[0]} {sched_path} HEXALY {str(st).split('.')[-1]} makespan={val} claimed={claim}")
    sys.exit(0 if ok and val == claim else 1)
