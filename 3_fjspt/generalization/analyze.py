#!/usr/bin/env python3
"""Collect run results -> results.csv, copy best schedule per instance to solutions/, verify with check_fjspt.py.
Usage: analyze.py WORKDIR(with runs_A, runs_B)"""
import csv, json, re, shutil, statistics, subprocess, sys
from pathlib import Path
W = Path(sys.argv[1]); H = Path(__file__).resolve().parent; CHK = H.parent / "check_fjspt.py"
names = [f"{k}{n:02d}" for k in ("SFJST", "MFJST") for n in range(1, 11)]
# published values (see README for table/page references); None = not reported
pub = {  # HF_MILP (optimal if no '*'), HF_LB_M (MILP bound when unsolved), HF_LB (Table 18), LAHC1000, LAHC100, TS_B (repo file), IGA_Meng, ACO_He
 "SFJST01": (70,None,70,70,70,None,None,70), "SFJST02": (111,None,111,111,111,None,None,111),
 "SFJST03": (223,None,223,223,223,None,None,223), "SFJST04": (359,None,357,359,359,None,None,359),
 "SFJST05": (123,None,121,123,123,None,None,123), "SFJST06": (324,None,322,324,324,None,None,324),
 "SFJST07": (409,None,409,409,409,None,None,409), "SFJST08": (269,None,259,269,269,None,None,269),
 "SFJST09": (220,None,220,220,220,None,None,220), "SFJST10": (531,None,519,531,531,None,None,531),
 "MFJST01": (485,None,472,485,485,485,485,485), "MFJST02": (463,None,452,463,468,463,463,463),
 "MFJST03": (482,None,472,482,482,482,482,482), "MFJST04": (576,None,560,576,576,579,576,576),
 "MFJST05": (532,None,520,532,532,532,532,532), "MFJST06": (652,None,640,652,652,652,652,652),
 "MFJST07": (None,788,796,898,907,901,898,907), "MFJST08": (None,780,788,900,918,900,900,914),
 "MFJST09": (None,780,788,1120,1181,1133,1117,1117), "MFJST10": (None,954,960,1238,1310,1270,1228,1267)}
# NB the ACO (He 2025) column is "Our work" in their Tables 5/6.
rows = []
for nm in names:
    I = json.load(open(H / "instances" / f"{nm}.json"))
    vals, ttbA, ttbB, ok = [], [], [], True; bestf = None
    for s in range(1, 6):
        sol = W / "runs_A" / f"{nm}_s{s}.sol"
        r = subprocess.run([sys.executable, str(CHK), str(H / "instances" / f"{nm}.json"), str(sol)], capture_output=True, text=True)
        mk = re.search(r"makespan=([0-9.]+)", r.stdout); feas = "FEASIBLE" in r.stdout and "INFEASIBLE" not in r.stdout
        claimed = float(re.search(r"^makespan ([0-9.]+)", sol.read_text(), re.M).group(1))
        ok &= feas and mk is not None and abs(float(mk.group(1)) - claimed) < 1e-9
        v = float(mk.group(1)); vals.append(v)
        if bestf is None or v < bestf[0]: bestf = (v, sol)
        # time to best: A = end time of first cooling-run log line whose best == final best (upper bound); B = trace
        t = None
        for l in (W / "runs_A" / f"{nm}_s{s}.log").read_text().splitlines():
            m = re.search(r"t=(\d+)s runBest=\S+ best=(\S+)", l)
            if m and float(m.group(2)) <= v + 1e-9: t = float(m.group(1)); break
        ttbA.append(t if t is not None else 60.0)
        nb = [(float(a), float(b)) for a, b in re.findall(r"NEWBEST t=([0-9.]+) cmax=([0-9.]+)", (W / "runs_B" / f"{nm}_s{s}.log").read_text())]
        vb = float(open(W / "runs_B" / f"{nm}_s{s}.out").read().split()[2])
        first = [t_ for t_, c_ in nb if c_ <= vb + 1e-9]   # first moment the final makespan was reached
        ttbB.append((first[0] if first else 0.0, vb))   # no trace line: best came from the initial solution (t~0)
    shutil.copy(bestf[1], H / "solutions" / f"{nm}.sol")
    p = pub[nm]; optimal = p[0] is not None
    best_pub = min(x for x in (p[0], p[3], p[4], p[5], p[6], p[7]) if x is not None)
    ref = p[0] if optimal else best_pub
    rows.append(dict(instance=nm, jobs=I["n_jobs"], machines=I["n_machines"], ops=sum(map(len, I["jobs"])), layout_machines=I["layout_machines"],
        HF21_MILP_opt=p[0] or "", HF21_MILP_LB=p[1] or "", HF21_LB_Table18=p[2], LAHC_H1000=p[3], LAHC_H100=p[4], TS_Berterottiere24=p[5] or "", IGA_Meng23=p[6] or "", ACO_He25=p[7],
        ref_value=ref, ref_is_proven_optimal=optimal, seeds="1-5", best=min(vals), median=statistics.median(vals), worst=max(vals), all_values=" ".join(f"{x:g}" for x in vals),
        seeds_reaching_ref=sum(x <= ref for x in vals), seeds_below_ref=sum(x < ref for x in vals),
        gap_best_pct=round(100 * (min(vals) - ref) / ref, 2), gap_median_pct=round(100 * (statistics.median(vals) - ref) / ref, 2),
        ttb_trace_s_of_best_seed=ttbB[vals.index(min(vals))][0], ttb_trace_s_median_over_seeds=statistics.median([t for t, _ in ttbB if t is not None]),
        ttb_trace_same_final_value=all(abs(b - v) < 1e-9 for (t, b), v in zip(ttbB, vals)), ttb_logA_upper_bound_s_median=statistics.median(ttbA),
        all_checked_feasible=ok))
ex = {}
for fn in H.glob("logs/exact_cpsat_*.txt"):
    for l in fn.read_text().splitlines():
        t = l.split()
        if len(t) >= 6: ex[t[0]] = (t[1], float(t[3]), float(t[5]))
for r in rows:
    e = ex.get(r["instance"], ("", "", ""))
    r["cpsat_status"], r["cpsat_obj"], r["cpsat_bound"] = e   # own exact model, 240-300 s, 8 workers (see README)
    r["proven_optimal_by_cpsat"] = (e[0] == "OPTIMAL")
with open(H / "results.csv", "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
for r in rows:
    print(r["instance"], r["ref_value"], "opt" if r["ref_is_proven_optimal"] else "bk", r["best"], r["median"], r["worst"], f'{r["seeds_reaching_ref"]}/5', r["ttb_trace_s_median_over_seeds"], r["ttb_logA_upper_bound_s_median"], r["ttb_trace_same_final_value"], r["all_checked_feasible"])
