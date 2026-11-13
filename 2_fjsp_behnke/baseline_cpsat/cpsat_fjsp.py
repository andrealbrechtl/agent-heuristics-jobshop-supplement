#!/usr/bin/env python3
"""Open-source baseline: standard CP-SAT model of the flexible job shop (makespan).

One optional interval per (operation, eligible machine), exactly one chosen per operation,
NoOverlap per machine, job precedence, minimise the makespan. No problem-specific search.

Usage: python cpsat_fjsp.py INSTANCE.fjs SECONDS WORKERS OUT.sched [seed]
Writes the best schedule (rows: job op machine start end, 0-based job/op, 1-based machine)
and prints the incumbent trace (time, makespan, bound) to stdout.
"""
import sys
import time
from ortools.sat.python import cp_model


def read_fjs(path):
    t = open(path).read().split()
    n = int(t[0]); p = 3 if "." in t[2] else 2
    jobs = []
    for _ in range(n):
        k = int(t[p]); p += 1; J = []
        for _ in range(k):
            a = int(t[p]); p += 1
            J.append([(int(t[p + 2 * q]), int(t[p + 2 * q + 1])) for q in range(a)]); p += 2 * a
        jobs.append(J)
    return jobs


class Trace(cp_model.CpSolverSolutionCallback):
    def __init__(self, t0):
        super().__init__(); self.t0 = t0

    def on_solution_callback(self):
        print(f"{time.time() - self.t0:.2f} {self.ObjectiveValue():.0f} {self.BestObjectiveBound():.0f}", flush=True)


def main(inst, secs, workers, out, seed=0):
    jobs = read_fjs(inst)
    horizon = sum(max(d for _, d in alts) for J in jobs for alts in J)
    m = cp_model.CpModel()
    per_machine, ops = {}, {}
    cmax = m.NewIntVar(0, horizon, "cmax")
    for j, J in enumerate(jobs):
        prev_end = None
        for o, alts in enumerate(J):
            lo = min(d for _, d in alts); hi = max(d for _, d in alts)
            s = m.NewIntVar(0, horizon, ""); e = m.NewIntVar(0, horizon, "")
            m.Add(e - s >= lo); m.Add(e - s <= hi)
            lits = []
            for mach, d in alts:
                l = m.NewBoolVar("")
                iv = m.NewOptionalIntervalVar(s, d, e, l, "")
                per_machine.setdefault(mach, []).append(iv)
                lits.append((l, mach))
            m.AddExactlyOne(l for l, _ in lits)
            if prev_end is not None:
                m.Add(s >= prev_end)
            prev_end = e
            ops[j, o] = (s, e, lits)
        m.Add(cmax >= prev_end)
    for ivs in per_machine.values():
        m.AddNoOverlap(ivs)
    m.Minimize(cmax)
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = secs
    solver.parameters.num_workers = workers
    solver.parameters.random_seed = seed
    st = solver.Solve(m, Trace(time.time()))
    if st not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        print("NO SOLUTION", solver.StatusName(st)); return 1
    mk = int(solver.Value(cmax))
    with open(out, "w") as f:
        f.write(f"makespan {mk}\n# CP-SAT {solver.StatusName(st)} bound {solver.BestObjectiveBound():.0f}\n")
        for (j, o), (s, e, lits) in sorted(ops.items()):
            mach = next(mm for l, mm in lits if solver.Value(l))
            f.write(f"{j} {o} {mach} {solver.Value(s)} {solver.Value(e)}\n")
    print(f"FINAL {mk} bound {solver.BestObjectiveBound():.0f} {solver.StatusName(st)}")
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    sys.exit(main(a[0], float(a[1]), int(a[2]), a[3], int(a[4]) if len(a) > 4 else 0))
