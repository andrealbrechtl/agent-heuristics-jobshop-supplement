#!/usr/bin/env python3
"""MKT03/MKT04 puzzle, step 2: is there a relabelling of the 8 machines (permutation of layout
rows/cols 1..8, i.e. a machine-numbering difference) under which the HF LB1/LB2 definitions give the
published (LB1, LB2) of Table 19?  Exhaustive over 8! permutations. Also checks MKT01/02/05 as controls
(6-/4-machine layouts) to show how (un)likely a coincidental match is."""
import json, itertools, sys
pub = {"MKT01": (57, 125), "MKT02": (53, 83), "MKT03": (117, 200), "MKT04": (80, 152), "MKT05": (105, 231)}

def lbs(jobs, T):
    lb1 = 0; sp = 0
    for J in jobs:
        D = {k: T[0][k] + p for k, p in J[0]}; Dt = {k: T[0][k] for k, p in J[0]}
        for o in range(1, len(J)):
            D = {b: min(D[a] + (0 if a == b else T[a][b]) for a in D) + p for b, p in J[o]}
            Dt = {b: min(Dt[a] + (0 if a == b else T[a][b]) for a in Dt) for b, p in J[o]}
        lb1 = max(lb1, min(D.values())); sp += min(Dt.values())
    plast = min(min(p for _, p in J[-1]) for J in jobs)
    return lb1, sp / 2 + plast

for name in sys.argv[1:] or ["MKT03", "MKT04"]:
    I = json.load(open(f"../instances/{name}.json")); T = I["travel"]; m = len(T) - 1
    jobs = I["jobs"]
    hits = []; both1 = 0; both2 = 0
    for perm in itertools.permutations(range(1, m + 1)):
        P = (0,) + perm
        T2 = [[T[P[a]][P[b]] for b in range(m + 1)] for a in range(m + 1)]
        l1, l2 = lbs(jobs, T2)
        both1 += l1 == pub[name][0]; both2 += l2 == pub[name][1]
        if (l1, l2) == pub[name]:
            hits.append(perm)
    print(name, "identity:", lbs(jobs, T), "published:", pub[name], f"| perms matching LB1: {both1}, LB2: {both2}, both: {len(hits)}", hits[:5])
