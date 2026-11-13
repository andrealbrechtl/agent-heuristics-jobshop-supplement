#!/usr/bin/env python3
"""Decode a published Berterottiere et al. (2024) FJSPT solution (github.com/lucasberter/FJSPT,
Homayouni_brandimarte/Mk_XX.txt) into an explicit schedule, independently of their code.

The file gives, per machine, the sequence of operations ("M<m> o1 o2 ...", operations numbered
1..N job by job) and, per vehicle, the sequence of transport tasks ("V<v> T<o> ..."), where T<o> is
the loaded transport that brings the job to operation o. We compute the earliest (semi-active)
schedule consistent with these sequences and write it in the format read by check_fjspt.py.

Usage: python decode_berterottiere.py INSTANCE.json Mk_XX.txt OUT.sol
"""
import json
import sys
from fractions import Fraction as F


def main(inst_path, res_path, out_path):
    I = json.load(open(inst_path))
    travel = [[F(str(x)) for x in r] for r in I["travel"]]
    jobs = I["jobs"]
    ids, inv = {}, {}
    n = 0
    for j, J in enumerate(jobs, 1):
        for k in range(1, len(J) + 1):
            n += 1
            ids[(j, k)] = n
            inv[n] = (j, k)
    mseq, vseq, claimed = {}, {}, None
    for line in open(res_path):
        t = line.split()
        if not t:
            continue
        if "Cmax:" in t and claimed is None:
            claimed = F(t[t.index("Cmax:") + 1])
        if t[0][0] == "M" and t[0][1:].isdigit():
            mseq[int(t[0][1:])] = [int(x) for x in t[1:]]
        elif t[0][0] == "V" and t[0][1:].isdigit():
            vseq[int(t[0][1:])] = [int(x[1:]) for x in t[1:]]
    mach = {}
    for m, L in mseq.items():
        for o in L:
            assert o not in mach, f"operation {o} on two machines"
            mach[o] = m
    assert sorted(mach) == list(range(1, n + 1)), "not every operation assigned"
    proc = {}
    for o, (j, k) in inv.items():
        alts = dict((int(a), F(str(p))) for a, p in jobs[j - 1][k - 1])
        assert mach[o] in alts, f"op {o} on ineligible machine {mach[o]}"
        proc[o] = alts[mach[o]]

    def origin(o):
        j, k = inv[o]
        return 0 if k == 1 else mach[ids[(j, k - 1)]]

    needs = {o for o in inv if origin(o) != mach[o]}
    veh = {}
    for v, L in vseq.items():
        for o in L:
            assert o not in veh, f"transport {o} on two vehicles"
            veh[o] = v
    assert set(veh) == needs, f"transports given {len(veh)} vs required {len(needs)}: " \
        f"missing {sorted(needs - set(veh))[:5]} extra {sorted(set(veh) - needs)[:5]}"

    # earliest times by fixed-point iteration over the (acyclic) precedence graph
    S, E, D, A = {}, {}, {}, {}
    mprev = {o: (L[i - 1] if i else None) for L in mseq.values() for i, o in enumerate(L)}
    vprev = {o: (L[i - 1] if i else None) for L in vseq.values() for i, o in enumerate(L)}
    for _ in range(4 * n + 10):
        changed = False
        for o in range(1, n + 1):
            j, k = inv[o]
            ready = F(0) if k == 1 else E.get(ids[(j, k - 1)])
            if ready is None:
                continue
            if o in needs:
                p = vprev[o]
                if p is None:
                    vfree = travel[0][origin(o)]
                elif p in A:
                    vfree = A[p] + travel[mach[p]][origin(o)]
                else:
                    continue
                d = max(ready, vfree)
                arr = d + travel[origin(o)][mach[o]]
                if D.get(o) != d:
                    D[o], A[o], changed = d, arr, True
                ready = A[o]
            mp = mprev[o]
            if mp is not None:
                if mp not in E:
                    continue
                ready = max(ready, E[mp])
            if S.get(o) != ready:
                S[o], E[o], changed = ready, ready + proc[o], True
        if not changed:
            break
    assert len(S) == n, "precedence graph has a cycle (sequences inconsistent)"
    mk = max(E.values())
    with open(out_path, "w") as f:
        f.write(f"instance {I['name']}\nmakespan {float(mk):g}\n")
        f.write(f"# decoded from {res_path.split('/')[-1]} (published Cmax {float(claimed):g})\n")
        for o in range(1, n + 1):
            j, k = inv[o]
            if o in needs:
                f.write(f"tr {j} {k} {veh[o]} {origin(o)} {mach[o]} {float(D[o]):g} {float(A[o]):g}\n")
            f.write(f"op {j} {k} {mach[o]} {float(S[o]):g} {float(E[o]):g}\n")
    print(f"{I['name']}: decoded makespan {float(mk):g}, published {float(claimed):g}, "
          f"{'MATCH' if mk == claimed else 'DIFFERENT'}")
    return mk == claimed


if __name__ == "__main__":
    sys.exit(0 if main(*sys.argv[1:4]) else 1)
