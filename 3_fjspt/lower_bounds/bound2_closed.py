#!/usr/bin/env python3
"""Bound (2): Leandro's closed form, exact arithmetic.

  pi_a   = shortest-path length a -> 0 in the directed travel graph (empty moves cost >= pi_a - pi_b)
  A_j    = min over machine sequences of job j of (loaded travel + pi_{f_j})   (f_j = last machine)
  tau    = min over machines k of (smallest processing time on k - pi_k)
  LB2    = sum_j A_j / |V| + tau,   rounded up to the time grid.

Also prints a strengthened variant tau' = min over (op, k) of (p_ok + tail_ok - pi_k) (q_v >= p + tail).
Usage: bound2_closed.py INSTANCE.json [...]
"""
import os
import sys
from fractions import Fraction as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import Inst, fmt  # noqa: E402

NV = 2


def shortest_to_0(t):
    n = len(t)
    pi = [0 if a == 0 else t[a][0] for a in range(n)]
    for _ in range(n):  # Bellman-Ford
        for a in range(n):
            for b in range(n):
                if a != b and t[a][b] + pi[b] < pi[a]:
                    pi[a] = t[a][b] + pi[b]
    return pi


def bound2(I):
    pi = shortest_to_0(I.t)
    A = []
    for j, J in enumerate(I.jobs):
        D = {k: I.t[0][k] for k in J[0]}  # loaded travel up to op o on k
        for o in range(1, len(J)):
            D = {b: min(D[a] + I.tr(a, b) for a in J[o - 1]) for b in J[o]}
        A.append(min(D[k] + pi[k] for k in J[-1]))
    pmin = {}
    for J in I.jobs:
        for alts in J:
            for k, p in alts.items():
                pmin[k] = min(pmin.get(k, 10 ** 9), p)
    tau = min(pmin[k] - pi[k] for k in pmin)
    tau2 = None
    for j, J in enumerate(I.jobs):
        T = I.tails(j)
        for o, alts in enumerate(J):
            for k, p in alts.items():
                v = p + T[o][k] - pi[k]
                tau2 = v if tau2 is None else min(tau2, v)
    raw = F(sum(A), NV) + tau
    raw2 = F(sum(A), NV) + tau2
    return dict(pi=pi, A=A, tau=tau, raw=F(raw, I.scale), lb=I.ceil_gran(raw),
                raw_strong=F(raw2, I.scale), lb_strong=I.ceil_gran(raw2))


if __name__ == "__main__":
    for path in sys.argv[1:]:
        I = Inst(path)
        r = bound2(I)
        print(f"{I.name}\tLB2={fmt(r['lb'])}\traw={float(r['raw']):.3f}\tsumA={fmt(F(sum(r['A']), I.scale))}"
              f"\ttau={fmt(F(r['tau'], I.scale))}\tLB2_strong={fmt(r['lb_strong'])}")
