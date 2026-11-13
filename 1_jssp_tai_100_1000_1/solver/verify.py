#!/usr/bin/env python3
"""Independent feasibility checker for /app/solution.txt (mirrors the stated rules)."""
import sys

inst = sys.argv[1] if len(sys.argv) > 1 else '/app/tai_100_1000_1.txt'
sol = sys.argv[2] if len(sys.argv) > 2 else '/app/solution.txt'
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 535000

raw = open(inst).read().split('\n')
n, m = map(int, raw[0].split())
jobs = []
for j in range(n):
    t = list(map(int, raw[1 + j].split()))
    assert len(t) == 2 * m
    jobs.append([(t[2 * i], t[2 * i + 1]) for i in range(m)])

# parse solution the way the task describes: strip comments, ignore blank lines
lines = []
for ln in open(sol):
    ln = ln.split('#', 1)[0].strip()
    if ln:
        lines.append(ln)
assert len(lines) == n, 'expected %d content lines, got %d' % (n, len(lines))

start = []
for j, ln in enumerate(lines):
    v = list(map(int, ln.split()))
    assert len(v) == m, 'line %d: expected %d ints, got %d' % (j, m, len(v))
    start.append(v)

errors = 0
makespan = 0
for j in range(n):
    for o in range(m):
        s, (mc, d) = start[j][o], jobs[j][o]
        if s < 0:
            print('job %d op %d: negative start %d' % (j, o, s)); errors += 1
        if o and s < start[j][o - 1] + jobs[j][o - 1][1]:
            print('job %d op %d: starts %d before predecessor ends %d'
                  % (j, o, s, start[j][o - 1] + jobs[j][o - 1][1])); errors += 1
        makespan = max(makespan, s + d)

bym = [[] for _ in range(m)]
for j in range(n):
    for o in range(m):
        mc, d = jobs[j][o]
        bym[mc].append((start[j][o], start[j][o] + d, j, o))
for mc in range(m):
    iv = sorted(bym[mc])
    if len(iv) != n:
        print('machine %d has %d ops, expected %d' % (mc, len(iv), n)); errors += 1
    for a, b in zip(iv, iv[1:]):
        if a[1] > b[0]:
            print('machine %d overlap: job %d op %d [%d,%d) vs job %d op %d [%d,%d)'
                  % (mc, a[2], a[3], a[0], a[1], b[2], b[3], b[0], b[1])); errors += 1
            if errors > 20:
                sys.exit('too many errors')

print('makespan = %d   (limit %d)' % (makespan, limit))
print('constraint violations:', errors)
ok = errors == 0 and makespan < limit
print('FEASIBLE and UNDER LIMIT' if ok else 'FAILED')
sys.exit(0 if ok else 1)
