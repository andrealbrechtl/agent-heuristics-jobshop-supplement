#!/usr/bin/env python3
"""Independent feasibility checker for an FJSP schedule in the required format."""
import sys

def parse_instance(path):
    t = open(path).read().split()
    i = 0
    J = int(t[i]); i += 1
    M = int(t[i]); i += 1
    i += 1  # average eligible machines
    jobs = []
    for j in range(J):
        no = int(t[i]); i += 1
        ops = []
        for o in range(no):
            k = int(t[i]); i += 1
            alts = {}
            for _ in range(k):
                m = int(t[i]); i += 1
                d = int(t[i]); i += 1
                alts[m] = min(d, alts.get(m, 10**9))
            ops.append(alts)
        jobs.append(ops)
    assert i == len(t), (i, len(t))
    return J, M, jobs

def main(inst, sched):
    J, M, jobs = parse_instance(inst)
    lines = [l.split() for l in open(sched).read().strip().splitlines() if l.strip()]
    assert lines[0][0] == 'makespan', 'missing makespan header'
    claimed = int(lines[0][1])
    rows = [tuple(map(int, l)) for l in lines[1:]]
    total = sum(len(o) for o in jobs)
    assert len(rows) == total, f'expected {total} rows, got {len(rows)}'
    seen = set()
    bymach = {}
    endof = {}
    for (j, o, m, s, e) in rows:
        assert 0 <= j < J, f'bad job {j}'
        assert 0 <= o < len(jobs[j]), f'bad op {j},{o}'
        assert (j, o) not in seen, f'duplicate op {j},{o}'
        seen.add((j, o))
        assert m in jobs[j][o], f'machine {m} not eligible for op {j},{o}'
        d = jobs[j][o][m]
        assert e - s == d, f'op {j},{o} on m{m}: duration {e-s} != {d}'
        assert s >= 0, f'negative start {j},{o}'
        bymach.setdefault(m, []).append((s, e, j, o))
        endof[(j, o)] = e
    # job precedence
    for j in range(J):
        for o in range(1, len(jobs[j])):
            prev_end = endof[(j, o - 1)]
            cur = [r for r in rows if r[0] == j and r[1] == o][0]
            assert cur[3] >= prev_end, f'precedence violated job {j} op {o}: {cur[3]} < {prev_end}'
    # machine capacity
    for m, iv in bymach.items():
        iv.sort()
        for a, b in zip(iv, iv[1:]):
            assert b[0] >= a[1], f'machine {m} overlap: {a} and {b}'
    mk = max(e for (_, _, _, s, e) in [(0, 0, 0, r[3], r[4]) for r in rows])
    assert mk == claimed, f'header says {claimed}, actual makespan {mk}'
    print(f'OK  feasible schedule, makespan = {mk}  ({len(rows)} operations)')
    return mk

if __name__ == '__main__':
    inst = sys.argv[1] if len(sys.argv) > 1 else '/app/instances/behnke16.fjs'
    sch = sys.argv[2] if len(sys.argv) > 2 else '/app/solution/schedule.txt'
    main(inst, sch)
