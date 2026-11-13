#!/usr/bin/env python3
"""Independent FJSP schedule checker (written from scratch).
Usage: check_fjsp.py INSTANCE.fjs SCHEDULE [--claim N]
Instance: 'n m [avg]' then per job: nops, then per op: k followed by k (machine,time) pairs.
  Machine ids in the instance are inferred (must be dense 0..m-1 or 1..m, else error).
Schedule: free text; blank and '#' lines ignored; 'makespan X' (or 'makespan: X') gives the claim;
  other keyword lines (instance/jobs/machines/operations) are cross-checked if present;
  data rows are 5 ints: job op machine start end.  Machine base of the schedule is NOT assumed:
  both bases 0 and 1 are tried, and a base is accepted only if the whole schedule is feasible under it.
Exit 0 iff feasible under exactly one base (and claim, if any, equals recomputed makespan)."""
import sys, re

def read_instance(path):
    t = open(path).read().split()
    n, m = int(t[0]), int(t[1]); i = 2
    # optional third header token (float average) : detect by '.'
    avg = None
    if '.' in t[2]:
        avg = float(t[2]); i = 3
    jobs = []; ids = set(); elig_total = 0; nops_total = 0
    for j in range(n):
        k = int(t[i]); i += 1; ops = []
        for o in range(k):
            e = int(t[i]); i += 1; d = {}
            for _ in range(e):
                mach, p = int(t[i]), int(t[i+1]); i += 2
                if mach in d: raise SystemExit(f"instance: duplicate machine {mach} job {j} op {o}")
                if p < 0: raise SystemExit("negative duration in instance")
                d[mach] = p; ids.add(mach)
            ops.append(d); elig_total += e; nops_total += 1
        jobs.append(ops)
    if i != len(t): raise SystemExit(f"instance: {len(t)-i} trailing tokens")
    lo, hi = min(ids), max(ids)
    if (lo, hi) == (0, m-1): base = 0
    elif (lo, hi) == (1, m): base = 1
    else: raise SystemExit(f"instance machine ids {lo}..{hi} not dense for m={m}")
    jobs = [[{mm-base: p for mm, p in d.items()} for d in ops] for ops in jobs]
    return n, m, avg, jobs, elig_total / nops_total, base

def read_schedule(path):
    claim = None; meta = {}; rows = []
    for ln, line in enumerate(open(path), 1):
        s = line.strip()
        if not s or s.startswith('#'): continue
        w = s.replace(':', ' ').split()
        if w[0].isalpha() or w[0].isidentifier():
            if w[0] == 'makespan': claim = int(w[1])
            else: meta[w[0]] = w[1] if len(w) > 1 else None
            continue
        if len(w) != 5: raise SystemExit(f"{path}:{ln}: expected 5 ints, got {s!r}")
        rows.append(tuple(int(x) for x in w))
    return claim, meta, rows

def check(inst, rows, off):
    n, m, avg, jobs, _, _ = inst
    errs = []
    seen = {}
    for (j, o, mm, st, en) in rows:
        mm -= off
        if not (0 <= j < n and 0 <= o < len(jobs[j])): errs.append(f"bad (job,op) {(j,o)}"); continue
        if (j, o) in seen: errs.append(f"duplicate op {(j,o)}"); continue
        seen[(j, o)] = (mm, st, en)
        if mm not in jobs[j][o]: errs.append(f"op {(j,o)} on ineligible/unknown machine {mm+off}"); continue
        if en - st != jobs[j][o][mm]: errs.append(f"op {(j,o)} duration {en-st} != {jobs[j][o][mm]}")
        if st < 0: errs.append(f"op {(j,o)} negative start")
    for j in range(n):
        for o in range(len(jobs[j])):
            if (j, o) not in seen: errs.append(f"missing op {(j,o)}")
    for j in range(n):
        for o in range(1, len(jobs[j])):
            if (j, o) in seen and (j, o-1) in seen and seen[(j, o)][1] < seen[(j, o-1)][2]:
                errs.append(f"precedence job {j}: op {o} starts {seen[(j,o)][1]} < end of op {o-1} {seen[(j,o-1)][2]}")
    per = {}
    for (j, o), (mm, st, en) in seen.items(): per.setdefault(mm, []).append((st, en, j, o))
    for mm, L in per.items():
        L.sort()
        for a, b in zip(L, L[1:]):
            if b[0] < a[1]: errs.append(f"overlap on machine {mm+off}: {a} vs {b}")
    ms = max((v[2] for v in seen.values()), default=None)
    return errs, ms

def main():
    a = sys.argv[1:]
    inst = read_instance(a[0]); n, m, avg, jobs, avg_re, ibase = inst
    claim, meta, rows = read_schedule(a[1])
    nops = sum(len(x) for x in jobs)
    print(f"instance {a[0]}: {n} jobs, {m} machines, {nops} ops, instance machine base {ibase}, header avg {avg}, recomputed avg {avg_re:.4f}")
    if avg is not None and abs(avg - avg_re) > 0.005: print("  WARNING header avg mismatch")
    print(f"schedule {a[1]}: {len(rows)} rows, claimed makespan {claim}, meta {meta}")
    for k, v in (('jobs', n), ('machines', m), ('operations', nops)):
        if k in meta and int(meta[k]) != v: print(f"  WARNING header {k}={meta[k]} != instance {v}")
    ok = []
    for off in (0, 1):
        errs, ms = check(inst, rows, off)
        print(f"  schedule machine base {off}: {'FEASIBLE makespan '+str(ms) if not errs else 'infeasible ('+str(len(errs))+' errors, first: '+errs[0]+')'}")
        if not errs: ok.append((off, ms))
    if len(ok) != 1: print("RESULT: FAIL (not feasible under exactly one machine base)"); sys.exit(1)
    off, ms = ok[0]
    if claim is not None and claim != ms: print(f"RESULT: FAIL claimed {claim} != recomputed {ms}"); sys.exit(1)
    cl = [int(x) for x in a[a.index('--claim')+1:a.index('--claim')+2]] if '--claim' in a else []
    if cl and cl[0] != ms: print(f"RESULT: FAIL expected {cl[0]} != {ms}"); sys.exit(1)
    print(f"RESULT: OK makespan {ms} (schedule machine base {off})")

if __name__ == '__main__': main()
