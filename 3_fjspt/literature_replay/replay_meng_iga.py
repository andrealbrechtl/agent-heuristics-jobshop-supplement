#!/usr/bin/env python3
"""Replay the IGA solutions printed in Appendix A of Meng et al. (2023) and write explicit schedules.

Encoding (the only reading under which the reported makespans are reproduced):
  OS  - operation sequence: job numbers, the k-th occurrence of job j is its k-th operation;
  MS  - machine numbers, listed job by job (job 1 op 1, job 1 op 2, ..., job 2 op 1, ...);
  AGV - vehicle numbers, listed in OS order (one entry per position of OS).
Decoding: operations are taken in OS order. If an operation stays on the machine of its
predecessor no transport is made; otherwise the given vehicle drives empty from its current
location to the job's location (jobs and vehicles start at L/U = 0), waits for the job, and
carries it. The operation starts when it has arrived and its machine has finished the previous
operation assigned to it in OS order. No return to L/U.

Usage: python replay_meng_iga.py meng2023_appendix_solutions.json INSTANCE_DIR OUT_DIR
"""
import json
import sys
from fractions import Fraction as F
from pathlib import Path


def replay(I, OS, MS, AGV):
    T = [[F(str(x)) for x in r] for r in I["travel"]]
    jobs = I["jobs"]
    os_ = [int(x) for x in OS.split()]
    ms = [int(x) for x in MS.split()]
    agv = [int(x) for x in AGV.split()]
    n_ops = sum(len(J) for J in jobs)
    assert len(os_) == len(ms) == len(agv) == n_ops, "string lengths do not match the instance"
    first = []
    k = 0
    for J in jobs:
        first.append(k)
        k += len(J)
    nxt = [0] * len(jobs)
    veh_t = {}; veh_loc = {}
    mach_free = {}; job_end = [F(0)] * len(jobs); job_loc = [0] * len(jobs)
    ops, trs = [], []
    for pos, jj in enumerate(os_):
        j = jj - 1; o = nxt[j]; nxt[j] += 1
        m = ms[first[j] + o]
        p = dict((a, F(str(b))) for a, b in jobs[j][o]).get(m)
        assert p is not None, f"machine {m} not eligible for job {jj} op {o + 1}"
        ready = job_end[j]
        if job_loc[j] != m:
            v = agv[pos]
            a = job_loc[j]
            dep = max(veh_t.get(v, F(0)) + T[veh_loc.get(v, 0)][a], ready)
            arr = dep + T[a][m]
            veh_t[v], veh_loc[v] = arr, m
            trs.append((j + 1, o + 1, v, a, m, dep, arr))
            ready = arr
        s = max(ready, mach_free.get(m, F(0)))
        e = s + p
        mach_free[m] = e; job_end[j] = e; job_loc[j] = m
        ops.append((j + 1, o + 1, m, s, e))
    return max(job_end), ops, trs


def main(sol_json, inst_dir, out_dir):
    D = json.load(open(sol_json))
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    ok = True
    for name, s in D["solutions"].items():
        I = json.load(open(Path(inst_dir) / f"{name}.json"))
        mk, ops, trs = replay(I, s["OS"], s["MS"], s["AGV"])
        g = lambda x: f"{float(x):g}"
        with open(Path(out_dir) / f"meng2023_iga_{name}.sol", "w") as f:
            f.write(f"instance {name}\nmakespan {g(mk)}\n# replay of Meng et al. (2023) Appendix A; reported {s['reported_makespan']}\n")
            for t in trs:
                f.write("tr %d %d %d %d %d %s %s\n" % (*t[:5], g(t[5]), g(t[6])))
            for t in ops:
                f.write("op %d %d %d %s %s\n" % (*t[:3], g(t[3]), g(t[4])))
        match = mk == F(str(s["reported_makespan"]))
        ok &= match
        print(f"{name}: replayed makespan {g(mk)}, reported {s['reported_makespan']} -> {'MATCH' if match else 'DIFFERENT'}")
    return ok


if __name__ == "__main__":
    sys.exit(0 if main(*sys.argv[1:4]) else 1)
