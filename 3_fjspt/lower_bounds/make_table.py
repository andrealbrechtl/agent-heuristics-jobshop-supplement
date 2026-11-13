#!/usr/bin/env python3
"""Collect all bound results into results/lower_bounds.csv (one row per instance).

Columns: HF2021 published LB, HF LB1/LB2 recomputed on our files, bound (1) [direct empty moves,
CP-SAT and CPLEX; shortest-path empty moves, CP-SAT; forced |V| routes], closed form (2),
relaxation (3) [CP-SAT, CP Optimizer: proven bound + status], old (removed) bound of the tag
with-fjspt-lower-bound, best certified schedule (from results/soundness.csv), best LB, and the
published values strictly below the best LB ('ruled out').
"""
import csv
import glob
import json
import os
import sys
from fractions import Fraction as F

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from common import Inst, fmt  # noqa: E402
from bound2_closed import bound2  # noqa: E402
from hf_lb import hf  # noqa: E402

HF_PUB = {  # HF2021 Tables 18-20, column LB (and LB1, LB2)
    "MKT01": (57, 125, 125), "MKT02": (53, 83, 83), "MKT03": (117, 200, 206), "MKT04": (80, 152, 152),
    "MKT05": (105, 231, 231), "MKT06": (121, 222.8, 222.8), "MKT07": (75, 150, 150), "MKT08": (250.5, 591.5, 591.5),
    "MKT09": (213, 481.8, 481.8), "MKT10": (203.5, 386.8, 386.8),
    "mt10c1t": (715, 309, 715), "mt10cct": (729, 333, 729), "mt10xt": (719, 302, 719), "mt10xxt": (733, 329, 733),
    "mt10xxxt": (703, 326, 703), "mt10xyt": (729, 325, 729), "mt10xyzt": (701, 324, 701),
    "setb4c9t": (764, 441, 848), "setb4cct": (756, 417, 848), "setb4xt": (766, 436, 848), "setb4xxt": (756, 462, 847),
    "setb4xxxt": (770, 462, 840), "setb4xyt": (756, 429, 859), "setb4xyzt": (772, 443, 859),
    "seti5c12t": (1045, 694, 1045), "seti5cct": (1051, 716, 1051), "seti5xt": (1047, 692, 1047),
    "seti5xxt": (1051, 725, 1051), "seti5xxxt": (1035, 708, 1035), "seti5xyt": (1047, 724, 1047),
    "seti5xyzt": (1035, 691, 1035),
    "SFJST01": (70, 23, 70), "SFJST02": (111, 46, 111), "SFJST03": (172, 47, 223), "SFJST04": (274, 90, 357),
    "SFJST05": (106, 24, 121), "SFJST06": (320, 62, 322), "SFJST07": (409, 79.5, 409), "SFJST08": (236, 87, 259),
    "SFJST09": (220, 58, 220), "SFJST10": (439, 131.5, 519),
    "MFJST01": (418, 120.5, 472), "MFJST02": (417, 117, 452), "MFJST03": (417, 123, 472), "MFJST04": (514, 129, 560),
    "MFJST05": (432, 129, 520), "MFJST06": (640, 135, 640), "MFJST07": (796, 109, 796), "MFJST08": (788, 108, 788),
    "MFJST09": (788, 124, 788), "MFJST10": (960, 129, 960),
}
OLD = {}
for line in open(os.path.join(HERE, "old_tag/supplement/3_fjspt/lower_bound/lb_results.txt")):
    t = line.split()
    if t and not t[0].startswith("#"):
        OLD[t[0]] = t[1]


def published():
    P = {}
    for r in csv.DictReader(open(os.path.join(ROOT, "literature_values.csv"))):
        if r["source"] in ("HF2021_LB",) or "via_Meng" in r["source"] or "NOT_COMPARABLE" in r["source"]:
            continue
        P.setdefault(r["instance"], []).append((r["source"], F(str(float(r["value"])))))
    # generalization/results.csv: published values for SFJST/MFJST
    g = os.path.join(ROOT, "generalization/results.csv")
    if os.path.exists(g):
        for r in csv.DictReader(open(g)):
            for k, v in r.items():
                if k and any(s in k.lower() for s in ("lahc", "ts_", "iga", "aco")) and v not in ("", "-", "–", None):
                    try:
                        P.setdefault(r["instance"], []).append((k, F(str(float(v)))))
                    except ValueError:
                        pass
    return P


def main():
    b1 = {}
    for f in glob.glob(os.path.join(HERE, "results/b1/*.json")):
        d = json.load(open(f))
        b1[d["instance"], os.path.basename(f)[:-5].split("_", 1)[1]] = d
    b3 = {}
    for f in glob.glob(os.path.join(HERE, "results/b3/*.json")):
        d = json.load(open(f))
        key = (d["instance"], d["solver"])
        if key not in b3 or (d["lb"] and F(d["lb"]) > F(b3[key]["lb"] or 0)):
            b3[key] = d
    best = {}
    for r in csv.DictReader(open(os.path.join(HERE, "results/soundness.csv"))):
        if r["file"].endswith(".sol") and r["feasible"] == "True":
            v = F(r["makespan"])
            if r["instance"] not in best or v < best[r["instance"]][0]:
                best[r["instance"]] = (v, r["file"])
    pub = published()
    names = [os.path.basename(p)[:-5] for p in sorted(glob.glob(os.path.join(ROOT, "instances/*.json")))]
    names += [os.path.basename(p)[:-5] for p in sorted(glob.glob(os.path.join(ROOT, "generalization/instances/*.json")))]
    rows = []
    for n in names:
        path = os.path.join(ROOT, "instances", n + ".json")
        if not os.path.exists(path):
            path = os.path.join(ROOT, "generalization/instances", n + ".json")
        I = Inst(path)
        h = hf(I)
        b2r = bound2(I)

        def g1(tag):
            d = b1.get((n, tag))
            return (d["lb"] if d["optimal"] else f"({d['lb']})") if d else ""

        def g3(s):
            d = b3.get((n, s))
            if not d:
                return "", "", ""
            return d["lb"], ("opt" if d["optimal"] else f"ub {d['incumbent']}"), d["runtime"]
        r3a, s3a, t3a = g3("cpsat")
        r3b, s3b, t3b = g3("cpo")
        cand = [F(x) for x in [g1("sound_cpsat"), g1("sound_cplex"), r3a, r3b] if x and not x.startswith("(")]
        cand.append(b2r["lb_strong"])
        bestlb = max(cand)
        cand_sp = [F(x) for x in [g1("sp_cpsat"), r3a, r3b] if x and not x.startswith("(")] + [b2r["lb"]]
        bestlb_sp = max(cand_sp)
        ruled = sorted({f"{s}={fmt(v)}" for s, v in pub.get(n, []) if v < bestlb})
        hp = HF_PUB.get(n, ("", "", ""))
        rows.append(dict(
            instance=n, HF_LB_pub=hp[2], HF_LB1_pub=hp[0], HF_LB2_pub=hp[1],
            HF_LB1_recomp=fmt(h["LB1"]), HF_LB2_recomp=fmt(h["LB2"]),
            b1_direct_cpsat=g1("sound_cpsat"), b1_direct_cplex=g1("sound_cplex"),
            b1_sp_cpsat=g1("sp_cpsat"), b1_forceV_cpsat=g1("force_cpsat"),
            b1_runtime_max_s=max([b1[(n, t)]["runtime"] for t in ("sound_cpsat", "sound_cplex", "sp_cpsat", "force_cpsat") if (n, t) in b1] or [0]),
            b2_closed=fmt(b2r["lb"]), b2_strong=fmt(b2r["lb_strong"]),
            b3_cpsat=r3a, b3_cpsat_status=s3a, b3_cpsat_s=t3a, b3_cpo=r3b, b3_cpo_status=s3b, b3_cpo_s=t3b,
            old_bound_removed=OLD.get(n, ""),
            best_LB=fmt(bestlb), best_LB_sp_semantics=fmt(bestlb_sp),
            best_certified=fmt(best[n][0]) if n in best else "", best_certified_file=best[n][1] if n in best else "",
            published_ruled_out=";".join(ruled)))
    with open(os.path.join(HERE, "results/lower_bounds.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    for r in rows:
        print(r["instance"], r["b1_direct_cpsat"], r["b1_sp_cpsat"], r["b2_closed"], r["b3_cpsat"], r["b3_cpsat_status"],
              r["b3_cpo"], r["b3_cpo_status"], "best", r["best_certified"], "| ruled out:", r["published_ruled_out"])


if __name__ == "__main__":
    main()


def markdown():
    rows = list(csv.DictReader(open(os.path.join(HERE, "results/lower_bounds.csv"))))
    out = ["| Instance | HF LB (pub.) | (1) CP-SAT | (1) CPLEX | (1) sp-empty | (2) closed | (3) CP-SAT | (3) CP Opt. | old bound | best certified | published values ruled out |",
           "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        def s3(v, st):
            return f"{v}" if st == "opt" else f"≥{v} ({st})" if v else ""
        out.append(f"| {r['instance']} | {r['HF_LB_pub']} | {r['b1_direct_cpsat']} | {r['b1_direct_cplex']} | {r['b1_sp_cpsat']} | "
                   f"{r['b2_closed']} | {s3(r['b3_cpsat'], r['b3_cpsat_status'])} | {s3(r['b3_cpo'], r['b3_cpo_status'])} | "
                   f"{r['old_bound_removed']} | {r['best_certified']} | {r['published_ruled_out'].replace(';', ', ')} |")
    open(os.path.join(HERE, "results/table.md"), "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    markdown()
