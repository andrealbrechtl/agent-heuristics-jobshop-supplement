#!/usr/bin/env python3
"""Soundness tests of bounds (1), (2), (3) against every certified schedule.

For every .sol file in the supplement (our 31 + 20 best schedules, 100 protocol runs, 10 CP-SAT
baselines, 14 + 2 literature replays):
  (a) re-check feasibility with ../check_fjspt.py (exact) and take its makespan;
  (b) makespan >= each bound value of that instance (results/b1, bound2, results/b3);
  (c) inequality-level test of bound (1): build the IP solution *induced by the schedule*
      (its machine assignment, each vehicle's actual trip sequence, first pickups, last drops,
      last-trip tails, c = makespan on the grid) and evaluate every constraint of the IP model
      exactly as built by bound1_ip.build(); any violated row is printed. Also checks, per
      vehicle, the un-aggregated inequality  C >= W_v + q_v.
Also: every proven optimum of HF2021 (SFJST01-10, MFJST01-06) and of our CP-SAT (MFJST07-08)
must be >= each bound.
Usage: soundness.py  -> writes results/soundness.csv and prints violations / summary.
"""
import csv
import glob
import json
import math
import os
import sys
from fractions import Fraction as F

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, ROOT)
from common import Inst, fmt  # noqa: E402
import bound1_ip  # noqa: E402
from bound2_closed import bound2  # noqa: E402
import check_fjspt  # noqa: E402


def inst_path(name):
    for d in ("instances", "generalization/instances"):
        p = os.path.join(ROOT, d, name + ".json")
        if os.path.exists(p):
            return p
    raise FileNotFoundError(name)


def sol_name(path):
    for line in open(path):
        t = line.split()
        if t and t[0] == "instance":
            return t[1]
    b = os.path.basename(path)
    for key in ["MKT", "SFJST", "MFJST", "mt10", "setb4", "seti5"]:
        i = b.find(key)
        if i >= 0:
            return b[i:].split("_")[0].split(".")[0]
    raise ValueError(path)


def load_bounds():
    B = {}
    for f in glob.glob(os.path.join(HERE, "results/b1/*.json")):
        d = json.load(open(f))
        tag = os.path.basename(f)[:-5].split("_", 1)[1]
        if d["optimal"]:
            B.setdefault(d["instance"], {})["b1_" + tag] = F(d["lb"])
    for f in glob.glob(os.path.join(HERE, "results/b3/*.json")):
        d = json.load(open(f))
        if d["lb"] is not None:
            key = "b3_" + d["solver"]
            cur = B.setdefault(d["instance"], {}).get(key)
            B[d["instance"]][key] = max(cur, F(d["lb"])) if cur is not None else F(d["lb"])
    return B


def induced_check(I, L, ops, trs, mk):
    """Evaluate every IP row of bound (1) at the solution induced by the schedule."""
    s = I.scale
    val = {n: 0 for n in L.vars}
    assign = {(j - 1, o - 1): m for (j, o), (m, st, en) in ops.items()}
    for (j, o), m in assign.items():
        val[f"x_{j}_{o}_{m}"] = 1
        if o > 0:
            val[f"z_{j}_{o}_{assign[j, o - 1]}_{m}"] = 1
    byv = {}
    for (j, o), (v, a, b, d, r) in trs.items():
        byv.setdefault(v, []).append((d, r, a, b, j - 1, o - 1))
    perveh = []
    for v, Ls in byv.items():
        Ls.sort()
        loc = None
        W = 0
        for i, (d, r, a, b, j, o) in enumerate(Ls):
            if i == 0:
                val[f"s_{a}"] += 1
                W += I.te[0][a]
            else:
                val[f"e_{loc}_{a}"] += 1
                W += I.te[loc][a]
            W += I.t[a][b]
            loc = b
        d, r, a, b, j, o = Ls[-1]
        val[f"f_{b}"] += 1
        val[f"l_{j}_{o}_{b}"] = 1
        J = I.jobs[j]
        R = 0
        for o2 in range(o + 1, len(J)):
            R += J[o2][assign[j, o2]] + I.tr(assign[j, o2 - 1], assign[j, o2])
        if o < len(J) - 1:
            val[f"G_{j}_{o}"] = R
        q = J[o][b] + R
        perveh.append((v, W, q))
    val["u"] = 1 if len(byv) == 2 else 0
    for k in I.machines:
        val[f"w_{k}"] = int(any(m == k for m in assign.values()))
    mk_s = mk * s
    val["c"] = math.ceil(mk_s / I.gran)
    bad = []
    for name, terms, sense, rhs in L.cons:
        lhs = sum(c * val[v] for c, v in terms)
        ok = (lhs == rhs) if sense == "=" else (lhs >= rhs if sense == ">=" else lhs <= rhs)
        if not ok:
            bad.append(f"{name}: lhs={lhs} {sense} {rhs}")
    for n, (lb, ub, kind) in L.vars.items():
        if not lb <= val[n] <= ub:
            bad.append(f"var {n}={val[n]} outside [{lb},{ub}]")
    for v, W, q in perveh:
        if F(W + q, s) > mk:
            bad.append(f"vehicle {v}: W+q={fmt(F(W + q, s))} > makespan {fmt(mk)}")
    # slack of the aggregated vehicle row
    agg = F(sum(W + q for _, W, q in perveh), s * 2)
    return bad, agg


def main():
    B = load_bounds()
    sols = []
    for pat in ["solutions/*.sol", "protocol_runs/*.sol", "literature_replay/*.sol", "baseline_cpsat/results/*.sol",
                "generalization/solutions/*.sol", "generalization/literature_replay/*.sol"]:
        sols += sorted(glob.glob(os.path.join(ROOT, pat)))
    models = {}
    rows, viol = [], []
    for sp in sols:
        name = sol_name(sp)
        ip = inst_path(name)
        nm, ok, mk, ntr, errs, _ = check_fjspt.check(ip, sp, 2)
        if name not in models:
            I = Inst(ip)
            models[name] = (I, bound1_ip.build(I), bound2(I))
        I, L, b2 = models[name]
        bounds = dict(B.get(name, {}))
        bounds["b2"] = b2["lb"]
        bounds["b2_strong"] = b2["lb_strong"]
        below = [k for k, v in bounds.items() if mk < v and not k.startswith("b1_force")]
        below_force = [k for k, v in bounds.items() if mk < v and k.startswith("b1_force")]
        ops, trs, _, _ = check_fjspt.load_solution(sp)
        bad, agg = induced_check(I, L, ops, trs, mk) if ok else (["infeasible schedule"], None)
        row = dict(file=os.path.relpath(sp, ROOT), instance=name, feasible=ok, makespan=fmt(mk),
                   **{k: fmt(v) for k, v in sorted(bounds.items())},
                   min_slack=fmt(min(mk - v for k, v in bounds.items())),
                   bounds_violated=";".join(below + below_force), induced_ip_rows_violated=len(bad),
                   veh_row_rhs=fmt(agg) if agg is not None else "")
        rows.append(row)
        if not ok or below or below_force or bad:
            viol.append((row["file"], ok, below + below_force, bad[:5]))
    # proven optima
    optima = dict(SFJST01=70, SFJST02=111, SFJST03=223, SFJST04=359, SFJST05=123, SFJST06=324, SFJST07=409,
                  SFJST08=269, SFJST09=220, SFJST10=531, MFJST01=485, MFJST02=463, MFJST03=482, MFJST04=576,
                  MFJST05=532, MFJST06=652, MFJST07=898, MFJST08=900)
    for name, opt in optima.items():
        I = Inst(inst_path(name))
        bounds = dict(B.get(name, {}))
        b2 = bound2(I)
        bounds["b2"], bounds["b2_strong"] = b2["lb"], b2["lb_strong"]
        below = [k for k, v in bounds.items() if opt < v]
        src = "HF2021 MILP" if name < "MFJST07" or name.startswith("SFJST") else "CP-SAT (generalization/)"
        rows.append(dict(file=f"proven optimum ({src})", instance=name, feasible=True, makespan=opt,
                         **{k: fmt(v) for k, v in sorted(bounds.items())},
                         min_slack=fmt(min(opt - v for v in bounds.values())), bounds_violated=";".join(below),
                         induced_ip_rows_violated="", veh_row_rhs=""))
        if below:
            viol.append((f"optimum {name}", True, below, []))
    keys = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    with open(os.path.join(HERE, "results/soundness.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, keys)
        w.writeheader()
        w.writerows(rows)
    print(f"schedules checked: {len(sols)}  (+ {len(optima)} proven optima)")
    print(f"infeasible schedules: {sum(1 for r in rows if r['feasible'] is False)}")
    print(f"violations: {len(viol)}")
    for v in viol:
        print("  VIOLATION", v)
    # tightest cases
    tight = sorted(rows, key=lambda r: F(r["min_slack"]))[:12]
    for r in tight:
        print("  tight:", r["file"], r["instance"], "mk", r["makespan"], "min_slack", r["min_slack"])


if __name__ == "__main__":
    main()
