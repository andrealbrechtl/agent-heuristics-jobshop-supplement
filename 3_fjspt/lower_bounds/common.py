"""Shared data handling for the independent lower-bound validation (FJSPT, 2 vehicles).

Semantics follow ../check_fjspt.py exactly:
  * travel[a][b] is used *directly* (no shortest-path routing); a vehicle at drop location l
    needs travel[l][a] to reach pickup a (the checker also charges travel[l][l] when l == a;
    we use 0 on the diagonal for empty moves, which is weaker, hence still valid);
  * a transport into op k exists iff k == 1 (from location 0) or machine(k-1) != machine(k);
  * makespan = latest operation completion, no return trip to location 0.

All data are scaled by SCALE (= lcm of denominators, 2 for half-unit layouts) to integers.
"""
import json
import math
from fractions import Fraction as F

import os as _os, sys as _sys
SOLVERS_PY = _os.environ.get("SOLVERS_PY", _sys.executable)  # Python with ortools and docplex
CPLEX_BIN = "/opt/ibm/ILOG/CPLEX_Studio_222_acad/cplex/bin/x86-64_linux/cplex"
CPO_BIN = "/opt/ibm/ILOG/CPLEX_Studio_222_acad/cpoptimizer/bin/x86-64_linux/cpoptimizer"


class Inst:
    def __init__(self, path):
        I = json.load(open(path))
        self.name = I.get("name", path)
        travel = [[F(str(x)) for x in row] for row in I["travel"]]
        jobs = [[{int(m): F(str(p)) for m, p in alts} for alts in J] for J in I["jobs"]]
        vals = [x for r in travel for x in r] + [p for J in jobs for a in J for p in a.values()]
        self.scale = 1
        for v in vals:
            self.scale = self.scale * v.denominator // math.gcd(self.scale, v.denominator)
        s = self.scale
        self.t = [[int(x * s) for x in row] for row in travel]          # direct travel, scaled
        self.nloc = len(self.t)
        self.jobs = [[{m: int(p * s) for m, p in a.items()} for a in J] for J in jobs]
        self.n = len(self.jobs)
        self.machines = sorted({m for J in self.jobs for a in J for m in a})
        # empty-move cost: direct travel, 0 on the diagonal (checker charges t[l][l]; 0 is weaker)
        self.te = [[0 if a == b else self.t[a][b] for b in range(self.nloc)] for a in range(self.nloc)]
        # granularity of the time grid (in scaled units): gcd of all scaled data
        g = 0
        for v in [x for r in self.t for x in r] + [p for J in self.jobs for a in J for p in a.values()]:
            g = math.gcd(g, v)
        self.gran = g if g else 1

    def unscale(self, v):
        return F(v, self.scale)

    # ---- heads / tails (exact DP over machine choices) -------------------------------
    def tr(self, a, b):
        """loaded travel between consecutive ops on a -> b (0 if same machine)."""
        return 0 if a == b else self.t[a][b]

    def heads(self, j):
        """head[o][k] = min earliest start of op o of job j if processed on k (travel from 0 first)."""
        J = self.jobs[j]
        H = [dict() for _ in J]
        for k in J[0]:
            H[0][k] = self.t[0][k]
        for o in range(1, len(J)):
            for k in J[o]:
                H[o][k] = min(H[o - 1][a] + J[o - 1][a] + self.tr(a, k) for a in J[o - 1])
        return H

    def tails(self, j):
        """tail[o][k] = min time from completion of op o on k to completion of the job."""
        J = self.jobs[j]
        T = [dict() for _ in J]
        for k in J[-1]:
            T[-1][k] = 0
        for o in range(len(J) - 2, -1, -1):
            for k in J[o]:
                T[o][k] = min(self.tr(k, b) + J[o + 1][b] + T[o + 1][b] for b in J[o + 1])
        return T

    def ceil_gran(self, v_scaled_fraction):
        """round a scaled (possibly fractional) bound up to the time grid; returns unscaled Fraction."""
        g = self.gran
        q = math.ceil(F(v_scaled_fraction) / g)
        return F(q * g, self.scale)


def fmt(x):
    x = F(x)
    return str(x.numerator) if x.denominator == 1 else str(float(x))
