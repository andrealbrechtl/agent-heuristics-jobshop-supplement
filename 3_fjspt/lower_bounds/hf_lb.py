#!/usr/bin/env python3
"""Re-computation of the Homayouni & Fontes (2021) lower bounds LB1 and LB2 (Sect. 5.1, Eqs. 26-27)
on our instance files, to compare with their Tables 18-20.

  LB1 = max_j  shortest LU -> last-op path, node weights = processing, arc weights = travel
        (arc weight 0 between consecutive ops on the same machine)
  LB2 = (1/|V|) sum_j  shortest LU -> last-op path with travel only  +  min_j min_m p(last op of j on m)
No empty travel anywhere. Several variants are printed to see which one reproduces the table:
  LB2a: as above;  LB2b: same-machine arcs charged travel[k][k] (=0 anyway);
  LB2c: min over all ops (not only last) of processing time.
Usage: hf_lb.py INSTANCE.json ...
"""
import os
import sys
from fractions import Fraction as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import Inst, fmt  # noqa: E402

NV = 2


def hf(I):
    s = I.scale
    lb1 = 0
    sp = []
    for j, J in enumerate(I.jobs):
        D = {k: I.t[0][k] + J[0][k] for k in J[0]}
        Dt = {k: I.t[0][k] for k in J[0]}
        for o in range(1, len(J)):
            D = {b: min(D[a] + I.tr(a, b) for a in J[o - 1]) + J[o][b] for b in J[o]}
            Dt = {b: min(Dt[a] + I.tr(a, b) for a in J[o - 1]) for b in J[o]}
        lb1 = max(lb1, min(D.values()))
        sp.append(min(Dt.values()))
    plast = min(min(J[-1].values()) for J in I.jobs)
    pall = min(p for J in I.jobs for a in J for p in a.values())
    lb2 = F(sum(sp), NV) + plast
    lb2c = F(sum(sp), NV) + pall
    return dict(LB1=F(lb1, s), LB2=F(lb2, s), LB2c=F(lb2c, s), sumSP=F(sum(sp), s), plast=F(plast, s))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        I = Inst(p)
        r = hf(I)
        print(I.name, " ".join(f"{k}={fmt(v)}" for k, v in r.items()))
