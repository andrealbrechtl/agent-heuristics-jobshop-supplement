#!/usr/bin/env python3
"""Re-check every certified result of the paper against the value the paper claims.

    python verify_all.py            # schedules, replays, protocol table (needs numpy)

Exit code 0 iff every check passes. Run from anywhere.
"""
import importlib.util
import io
import csv
import json
import statistics
import subprocess
import sys
from contextlib import redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent
J, B, T = HERE / "1_jssp_tai_100_1000_1", HERE / "2_fjsp_behnke", HERE / "3_fjspt"
failures = []


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def report(label, ok, detail=""):
    print(f"  [{'OK' if ok else 'FAIL'}] {label}{(' - ' + detail) if detail else ''}")
    if not ok:
        failures.append(label)


# ---------------------------------------------------------------- 1. JSSP
print("1. Job shop tai_100_1000_1 (check_jssp.py)")
cj = load(J / "check_jssp.py", "check_jssp")
inst = J / "instance" / "tai_100_1000_1.txt"
r = cj.check(str(inst), str(J / "solutions" / "tai_100_1000_1_525515.txt"))
report("record schedule 525,515 (16-core run)", r["ok"] and r["makespan"] == 525515,
       f"makespan {r.get('makespan')}, {r.get('jobs_at_makespan')} jobs end at the makespan")
r = cj.check(str(inst), str(J / "solutions" / "tai_100_1000_1_525764.txt"))
report("12-worker fleet schedule 525,764", r["ok"] and r["makespan"] == 525764,
       f"makespan {r.get('makespan')}, {r.get('jobs_at_makespan')} jobs end at the makespan")
r = cj.check(str(inst), str(J / "solutions" / "tai_100_1000_1_530404_assisted_campaign.txt"))
report("assisted campaign 530,404", r["ok"] and r["makespan"] == 530404)
for line in (J / "solutions" / "autonomous_sessions" / "sessions.csv").read_text().splitlines()[1:]:
    sid, model, mk, *_ = line.split(",")
    if mk == "none":
        continue
    r = cj.check(str(inst), str(J / "solutions" / "autonomous_sessions" / f"session{int(sid):02d}_{mk}.txt"))
    report(f"autonomous session {sid} ({model}) {int(mk):,}", r["ok"] and r["makespan"] == int(mk))


print("   Generalisation: corridor solver on all ten tai_100_1000_k")
GJ = J / "generalization"
for k, (mk, lb, ub) in {1: (525999, 522298, 533080), 2: (530375, 530375, 538067), 3: (530560, 530560, 538757),
                        4: (529000, 527101, 534746), 5: (526333, 517728, 529580), 6: (525000, 522907, 534969),
                        7: (527000, 522537, 534974), 8: (528817, 526428, 535757), 9: (531000, 528097, 536993),
                        10: (526855, 521766, 529918)}.items():
    sol = next(GJ.glob(f"solutions/tai_100_1000_{k}_*.txt"))
    r = cj.check(str(GJ / "instances" / f"tai_100_1000_{k}.txt"), str(sol))
    # the instance lower bound is the longest job: recompute it
    t = (GJ / "instances" / f"tai_100_1000_{k}.txt").read_text().split()
    n, m = int(t[0]), int(t[1]); d = list(map(int, t[2:]))
    chain = max(sum(d[2 * (j * m + o) + 1] for o in range(m)) for j in range(n))
    tag = "OPTIMAL (= longest job)" if mk == chain else f"below best known {ub:,}"
    report(f"tai_100_1000_{k}: {mk:,} {tag}", r["ok"] and r["makespan"] == mk and chain == lb and mk < ub)

print("   Generalisation, 16-core runs (16 single-core workers x 1,800 s, three runs per instance)")
import gzip, tempfile
G16 = GJ / "16cores"
runs16 = json.loads((G16 / "results.json").read_text())
best16 = {}
for run in runs16:
    k = int(run["instance"].split("_")[-1])
    with tempfile.NamedTemporaryFile("wb", suffix=".txt", delete=False) as tmp:
        tmp.write(gzip.decompress((G16 / run["solution"]).read_bytes()))
    r = cj.check(str(GJ / "instances" / f"tai_100_1000_{k}.txt"), tmp.name)
    Path(tmp.name).unlink()
    ok = r["ok"] and r["makespan"] == run["best"] == min(w["makespan"] for w in run["workers"])
    report(f"16-core run {run['run']} tai_100_1000_{k}: {run['best']:,}", ok)
    best16[k] = min(best16.get(k, run["best"]), run["best"])
lbs = {1: 522298, 2: 530375, 3: 530560, 4: 527101, 5: 517728, 6: 522907, 7: 522537, 8: 526428, 9: 528097, 10: 521766}
g = [100 * (best16[k] / lbs[k] - 1) for k in best16]
report(f"16 cores: gap {min(g):.1f}-{max(g):.1f}% (paper 0.0-1.6%)", (round(min(g), 1), round(max(g), 1)) == (0.0, 1.6))
report("16 cores: best of all runs 525,515 / 528,156 / 525,865 / 524,812 / 528,473 / 529,470 / 526,000 on _1/_4/_5/_6/_8/_9/_10",
       [best16[k] for k in (1, 4, 5, 6, 8, 9, 10)] == [525515, 528156, 525865, 524812, 528473, 529470, 526000])

# ---------------------------------------------------------------- 2. Behnke FJSP
print("2. Flexible job shop, Behnke & Geiger (check_fjsp.py)")
for sched, inst_name, claim in [("behnke16_404.txt", "behnke16", 404), ("behnke16_406.sched", "behnke16", 406),
                                ("behnke31_226_seed44.sched", "behnke31", 226),
                                ("behnke31_226_norr_ablation.sched", "behnke31", 226),
                                ("behnke14_232.sched", "behnke14", 232)]:
    out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(B / "instances" / f"{inst_name}.fjs"),
                          str(B / "solutions" / sched), "--claim", str(claim)], capture_output=True, text=True)
    report(f"{sched}", out.returncode == 0, out.stdout.strip().splitlines()[-1] if out.stdout else out.stderr[-200:])


print("   Autonomous Behnke sessions (all runs)")
for f in sorted((B / "solutions" / "autonomous_sessions").glob("*.txt")):
    inst, mk = f.stem.split("_")[0], f.stem.split("_")[-1]
    out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(B / "instances" / f"{inst}.fjs"), str(f),
                          "--claim", mk], capture_output=True, text=True)
    report(f"{f.stem}", out.returncode == 0)

print("   Generalisation: reusable tabu and CP-SAT on all 60 Behnke instances")
GB = B / "generalization"
res = json.load(open(GB / "results.json"))
okall = True
for r in res:
    for m in ("tabu", "cpsat"):
        out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(GB / "instances" / f"{r['instance']}.fjs"),
                              str(GB / "solutions" / m / f"{r['instance']}.sched"), "--claim", str(r[m])], capture_output=True, text=True)
        okall &= out.returncode == 0
report("all 120 schedules feasible at the reported values", okall)
op = [r for r in res if r["status"] == "open"]
report("tabu below FJSPLib UB on 5 open instances (16, 40, 53, 55, 59)",
       sorted(int(r["instance"][6:]) for r in op if r["tabu"] < r["ub"]) == [16, 40, 53, 55, 59])
dt = statistics.mean(100 * (r["tabu"] - r["ub"]) / r["ub"] for r in res)
dc = statistics.mean(100 * (r["cpsat"] - r["ub"]) / r["ub"] for r in res)
report(f"mean deviation from UB: tabu {dt:.2f} %, CP-SAT {dc:.2f} %", round(dt, 2) == 0.60 and round(dc, 2) == 6.42)
okl = True
for r in op:
    out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(GB / "instances" / f"{r['instance']}.fjs"),
                          str(GB / "solutions" / "tabu_1200s" / f"{r['instance']}.sched"), "--claim", str(r["tabu_long"])],
                         capture_output=True, text=True)
    okl &= out.returncode == 0
report("32 long tabu runs (1,200 s) feasible at the reported values", okl)
newbest = sorted(int(r["instance"][6:]) for r in op if min(r["tabu"], r["tabu_long"]) < r["ub"])
report(f"new best-known values on 11 open instances {newbest}", newbest == [13, 16, 32, 35, 40, 53, 55, 56, 57, 59, 60])

print("   Re-run of the tabu with the cores of the CP Optimizer runs (rerun_same_cores/, supplementary)")
RR = GB / "rerun_same_cores"
rr = {r["instance"]: r for r in json.loads((RR / "results.json").read_text())}
okr = True
for i, r in rr.items():
    for key, blk in (("tabu120", "tabu_120s"), ("tabu1200", "tabu_1200s")):
        if key in r:
            out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(GB / "instances" / f"{i}.fjs"),
                                  str(RR / blk / f"{i}.sched"), "--claim", str(r[key])], capture_output=True, text=True)
            okr &= out.returncode == 0
report(f"{sum(len([k for k in ('tabu120', 'tabu1200') if k in r]) for r in rr.values())} re-run schedules feasible at the reported values", okr)
ubr = {r["instance"]: r["ub"] for r in res}
d120 = statistics.mean(100 * (r["tabu120"] / ubr[i] - 1) for i, r in rr.items())
d1200 = statistics.mean(100 * (rr[r["instance"]]["tabu1200"] / r["ub"] - 1) for r in op)
report(f"re-run mean deviation: 120 s {d120:.2f}% (paper 0.45%), 1,200 s open {d1200:.2f}% (paper 0.34%)",
       round(d120, 2) == 0.45 and round(d1200, 2) == 0.34)

# ---------------------------------------------------------------- 3. FJSPT
print("3. FJSPT, two AGVs (check_fjspt.py)")
cf = load(T / "check_fjspt.py", "check_fjspt")
CLAIM = {"MKT01": 173, "MKT02": 124, "MKT03": 294, "MKT04": 238, "MKT05": 286, "MKT06": 279.5, "MKT07": 245,
         "MKT08": 696, "MKT09": 598, "MKT10": 499,
         "mt10c1t": 996, "mt10cct": 973, "mt10xt": 987, "mt10xxt": 992, "mt10xxxt": 990, "mt10xyt": 950,
         "mt10xyzt": 922, "setb4c9t": 1005, "setb4cct": 991, "setb4xt": 1002, "setb4xxt": 993, "setb4xxxt": 978,
         "setb4xyt": 975, "setb4xyzt": 968, "seti5c12t": 1307, "seti5cct": 1287, "seti5xt": 1332, "seti5xxt": 1326,
         "seti5xxxt": 1330, "seti5xyt": 1285, "seti5xyzt": 1274}
# Reference values (Tables 8 and B.1): smallest published value that is not below the lower
# bound of Section 3.2 and whose published schedule, if any, is feasible on the distributed instance. Sources:
# LAHC 2021 (incl. H=100), IGA 2023, TS 2024, HA-DQN 2025, ACO 2025, DCGA 2024 (Han et al.), IVNDGA 2025
# (Jia et al.), Meng et al. 2025 (CP Optimizer models / DCGA-CP). See 3_fjspt/literature_replay/SOURCES_2024_2025.md.
PUB_MKT = {"MKT01": 174, "MKT02": 122, "MKT03": 313, "MKT04": 237, "MKT05": 293, "MKT06": 288.5, "MKT07": 243,
           "MKT08": 722, "MKT09": 632, "MKT10": 525}
LB_MKT = {"MKT01": 167, "MKT02": 120, "MKT03": 258, "MKT04": 235, "MKT05": 285, "MKT06": 244.5, "MKT07": 228,
          "MKT08": 662.5, "MKT09": 566.5, "MKT10": 456}  # bound (1), shortest-path empty moves
# min(LAHC 2021, IGA 2023, DCGA 2024) on Chambers-Barnes, and the smallest valid published value including
# HA-DQN 2025 and Meng et al. 2025 (values below relaxation (3) excluded)
PUB_CB_LI = {"mt10c1t": 991, "mt10cct": 973, "mt10xt": 986, "mt10xxt": 991, "mt10xxxt": 981, "mt10xyt": 959,
             "mt10xyzt": 928, "setb4c9t": 1004, "setb4cct": 979, "setb4xt": 994, "setb4xxt": 1009, "setb4xxxt": 992,
             "setb4xyt": 963, "setb4xyzt": 972, "seti5c12t": 1347, "seti5cct": 1334, "seti5xt": 1355, "seti5xxt": 1373,
             "seti5xxxt": 1350, "seti5xyt": 1346, "seti5xyzt": 1305}
PUB_CB = {"mt10c1t": 979, "mt10cct": 948, "mt10xt": 978, "mt10xxt": 979, "mt10xxxt": 963, "mt10xyt": 950,
          "mt10xyzt": 901, "setb4c9t": 957, "setb4cct": 943, "setb4xt": 961, "setb4xxt": 961, "setb4xxxt": 961,
          "setb4xyt": 948, "setb4xyzt": 939, "seti5c12t": 1283, "seti5cct": 1209, "seti5xt": 1312, "seti5xxt": 1322,
          "seti5xxxt": 1316, "seti5xyt": 1258, "seti5xyzt": 1208}

def chk(inst, sol):
    with redirect_stdout(io.StringIO()):
        name, ok, mk, ntr, errs, stats = cf.check(str(T / "instances" / f"{inst}.json"), str(sol))
    return ok, float(mk), errs, stats


for inst, claim in CLAIM.items():
    ok, mk, errs, _ = chk(inst, T / "solutions" / f"{inst}.sol")
    report(f"{inst} = {claim:g}", ok and mk == claim, errs[0] if errs else "")
improved = sum(CLAIM[i] < PUB_CB_LI[i] for i in PUB_CB_LI)
ties = sum(CLAIM[i] == PUB_CB_LI[i] for i in PUB_CB_LI)
below_mkt = sorted(i for i in PUB_MKT if CLAIM[i] < PUB_MKT[i])
report("MKT: below every valid published value on 7 of 10 (MKT01, 03, 05, 06, 08, 09, 10)",
       below_mkt == ["MKT01", "MKT03", "MKT05", "MKT06", "MKT08", "MKT09", "MKT10"], ", ".join(below_mkt))
report("Chambers-Barnes vs LAHC/IGA/DCGA: 12 improved, 1 tie",
       improved == 12 and ties == 1, f"{improved} improved, {ties} tie")
report("Chambers-Barnes vs all valid published values: 0 improved, 1 tie (mt10xyt), at most 6.5 % above",
       sum(CLAIM[i] < PUB_CB[i] for i in PUB_CB) == 0 and [i for i in PUB_CB if CLAIM[i] == PUB_CB[i]] == ["mt10xyt"]
       and round(max(100 * (CLAIM[i] - PUB_CB[i]) / PUB_CB[i] for i in PUB_CB), 1) == 6.5)

print("   Lower bounds of Section 3.2 (3_fjspt/lower_bounds, validated with two solvers)")
with (T / "lower_bounds" / "results" / "lower_bounds.csv").open() as f:
    LBROWS = {r["instance"]: r for r in csv.DictReader(f)}
report("bound (1), shortest-path empties, MKT01-10 = paper values; CP-SAT and CPLEX agree (direct)",
       all(float(LBROWS[i]["b1_sp_cpsat"]) == LB_MKT[i] for i in LB_MKT)
       and all(LBROWS[i]["b1_direct_cpsat"] == LBROWS[i]["b1_direct_cplex"] for i in LBROWS))
report("relaxation (3): CP-SAT and CP Optimizer prove the same optimum on all 21 Chambers-Barnes instances",
       all(LBROWS[i]["b3_cpsat_status"] == "opt" and LBROWS[i]["b3_cpo_status"] == "opt"
           and LBROWS[i]["b3_cpsat"] == LBROWS[i]["b3_cpo"] for i in PUB_CB))
viol = [i for i in LBROWS if LBROWS[i]["best_certified"] and float(LBROWS[i]["best_certified"]) < float(LBROWS[i]["best_LB_sp_semantics"])]
report("no certified schedule below its lower bound", not viol, ", ".join(viol))
with (T / "lower_bounds" / "results" / "soundness.csv").open() as f:
    srows = list(csv.DictReader(f))
report(f"soundness test: {len(srows)} certified schedules checked against every bound, 0 violations",
       len(srows) >= 177 and all(r["feasible"] == "True" and not r["bounds_violated"]
                                 and r["induced_ip_rows_violated"] in ("0", "") for r in srows))

print("   Replays of Han et al. 2024 and Meng et al. 2025 (literature_replay/literature_replay_2025_2024.csv)")
with (T / "literature_replay" / "literature_replay_2025_2024.csv").open() as f:
    reps = [r for r in csv.DictReader(f) if r["check"] == "FEASIBLE" and r["file"]]
bad = []
for r in reps:
    inst_dir = T / "instances" if (T / "instances" / f"{r['instance']}.json").exists() else T / "generalization" / "instances"
    sub = "meng2025" if r["source"].startswith("Meng") else "han2024"
    with redirect_stdout(io.StringIO()):
        _, ok, mk, _, errs, _ = cf.check(str(inst_dir / f"{r['instance']}.json"), str(T / "literature_replay" / sub / r["file"]))
    if not (ok and float(mk) == float(r["replayed"])):
        bad.append(f"{r['source']} {r['instance']}")
report(f"{len(reps)} published schedules replay feasible at the stated makespan", not bad, ", ".join(bad))

print("   MKT10 anatomy (Section 5.4)")
_, _, _, st = chk("MKT10", T / "solutions" / "MKT10.sol")
loaded = sum(s["loaded"] for s in st.values()); empty = sum(s["empty"] for s in st.values())
idle = sum(s["idle"] for s in st.values()); ntr = sum(s["transports"] for s in st.values())
report("vehicles never idle; loaded 809, empty 172.5, 140 transports",
       idle == 0 and loaded == 809 and empty == 172.5 and ntr == 140)

print("   Literature replays (published schedules re-derived from the authors' files)")
for k, v in {"MKT01": 187, "MKT02": 134, "MKT03": 342, "MKT04": 254, "MKT05": 310, "MKT06": 336.5, "MKT07": 289,
             "MKT08": 770.5, "MKT09": 709, "MKT10": 616.5}.items():
    ok, mk, errs, _ = chk(k, T / "literature_replay" / f"berterottiere2024_{k}.sol")
    report(f"Berterottiere et al. 2024 {k} = {v:g}", ok and mk == v)
with redirect_stdout(io.StringIO()):
    import tempfile
    tmp = Path(tempfile.mkdtemp())
    rm = load(T / "literature_replay" / "replay_meng_iga.py", "replay_meng_iga")
    rm_ok = rm.main(str(T / "literature_replay" / "meng2023_appendix_solutions.json"), str(T / "instances"), str(tmp))
report("Meng et al. 2023 Appendix A replays reproduce 177/126/342/295", rm_ok)
for k, v in {"MKT01": 177, "MKT02": 126, "MKT03": 342, "MKT05": 295}.items():
    ok, mk, errs, _ = chk(k, tmp / f"meng2023_iga_{k}.sol")
    report(f"Meng et al. 2023 {k} replay feasible at {v}", ok and mk == v)

print("   Frozen-method protocol (fixed protocol table)")
TABLE3 = {"MKT01": (173, 174.1, 174, 175), "MKT02": (124, 124.0, 124, 124), "MKT03": (294, 296.4, 297, 298),
          "MKT04": (238, 239.2, 239.5, 240), "MKT05": (286, 287.4, 288, 288), "MKT06": (279.5, 281.65, 281.5, 284),
          "MKT07": (245, 246.0, 246, 247), "MKT08": (696, 701.9, 702.25, 709), "MKT09": (601, 605.75, 606, 610.5),
          "MKT10": (505, 509.1, 509.5, 515)}
below_runs = 0
for k, (b, mean, med, worst) in TABLE3.items():
    files = sorted((T / "protocol_runs").glob(f"{k}_s*.sol"))
    vals = []
    allok = True
    for f in files:
        ok, mk, _, _ = chk(k, f)
        allok &= ok
        vals.append(mk)
    nb = sum(v < PUB_MKT[k] for v in vals); below_runs += nb
    good = (allok and len(vals) == 10 and min(vals) == b and abs(statistics.mean(vals) - mean) < 1e-9
            and statistics.median(vals) == med and max(vals) == worst)
    report(f"{k}: 10 runs feasible, best/mean/median/worst {b:g}/{mean:g}/{med:g}/{worst:g}, {nb}/10 below {PUB_MKT[k]:g}",
           good, f"got {min(vals):g}/{statistics.mean(vals):g}/{statistics.median(vals):g}/{max(vals):g}")

report("Fixed protocol: 61 of 100 runs below the reference value (61 of 70 on the seven leading instances)", below_runs == 61)

print("   Chambers-Barnes: HA-DQN values below the FJSPLib optima without transport (not used as reference)")
report("Dong 2025 mt10xxxt = 872 < 918 (FJSPLib optimum of mt10xxx without transport)", 872 < 918)
report("Dong 2025 mt10xyzt = 816 < 847 (FJSPLib optimum of mt10xyz without transport)", 816 < 847)


print("   Open-source baseline: CP-SAT, 600 s, 8 workers on MKT01-10")
CPS = {"MKT01": 184, "MKT02": 156, "MKT03": 556, "MKT04": 310, "MKT05": 397, "MKT06": 457, "MKT07": 363,
       "MKT08": 1184.5, "MKT09": 1233, "MKT10": 1080}
for k, v in CPS.items():
    ok, mk, errs, _ = chk(k, T / "baseline_cpsat" / "results" / f"{k}.sol")
    report(f"CP-SAT {k} = {v:g} (agent best {CLAIM[k]:g})", ok and mk == v and CLAIM[k] < v)

print("   Generalisation: transport solver on SFJST/MFJST")
import csv
GT = T / "generalization"
for r in csv.DictReader(open(GT / "results.csv")):
    with redirect_stdout(io.StringIO()):
        name, ok, mk, *_ = cf.check(str(GT / "instances" / f"{r['instance']}.json"), str(GT / "solutions" / f"{r['instance']}.sol"))
    ref = {"MFJST09": 1075, "MFJST10": 1227}.get(r["instance"], float(r["ref_value"]))  # paper's reference (Meng et al. 2025; Han et al. 2024)
    report(f"{r['instance']} = {float(r['best']):g} (ref {ref:g})", ok and float(mk) == float(r["best"]))


# ---------------------------------------------------------------- 4. CP Optimizer baseline (our runs)
print("4. CP Optimizer 22.2 baseline (our runs; check_jssp.py / check_fjsp.py)")
C = HERE / "4_commercial_baselines" / "cpo"
runs = json.loads((C / "results.json").read_text())
for blk in ("jssp_1core", "jssp_16core"):
    best = {}
    for k in range(1, 11):
        rs = [r for r in runs if r["block"] == blk and r["instance"] == f"tai_100_1000_{k}"]
        sol = next((C / "solutions" / blk).glob(f"tai_100_1000_{k}_*.txt.gz"))
        claim = int(sol.name.split("_")[-1].split(".")[0])
        with tempfile.NamedTemporaryFile("wb", suffix=".txt", delete=False) as tmp:
            tmp.write(gzip.decompress(sol.read_bytes()))
        r = cj.check(str(J / "generalization" / "instances" / f"tai_100_1000_{k}.txt"), tmp.name)
        Path(tmp.name).unlink()
        best[k] = claim
        report(f"{blk} tai_100_1000_{k}: {claim:,} (best of {len(rs)} runs)",
               r["ok"] and r["makespan"] == claim == min(x["makespan"] for x in rs))
    lbs = {1: 522298, 2: 530375, 3: 530560, 4: 527101, 5: 517728, 6: 522907, 7: 522537, 8: 526428, 9: 528097, 10: 521766}
    g = [100 * (best[k] / lbs[k] - 1) for k in best]
    want = {"jssp_1core": (3.7, 5.8), "jssp_16core": (2.6, 5.3)}[blk]
    report(f"{blk}: 30-min gap {min(g):.1f}-{max(g):.1f}% (paper {want[0]}-{want[1]}%)",
           (round(min(g), 1), round(max(g), 1)) == want)
BG = json.loads((B / "generalization" / "results.json").read_text())
ub = {r["instance"]: r["ub"] for r in BG}
for blk, want in (("behnke_120s", 1.42), ("behnke_1200s", 0.79)):
    rs = [r for r in runs if r["block"] == blk]
    bad = 0
    for r in rs:
        out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(B / "generalization" / "instances" / f"{r['instance']}.fjs"),
                              str(C / "solutions" / blk / f"{r['instance']}.sched"), "--claim", str(r["makespan"])], capture_output=True, text=True)
        bad += out.returncode != 0
    dev = statistics.mean(100 * (r["makespan"] / ub[r["instance"]] - 1) for r in rs)
    report(f"{blk}: {len(rs)} schedules certified; mean deviation {dev:.2f}% (paper {want}%)", bad == 0 and round(dev, 2) == want)
r34 = next(r for r in runs if r["block"] == "behnke_1200s" and r["instance"] == "behnke34")
report("behnke34 = 221 (CP Optimizer, 1,200 s), below FJSPLib 223", r34["makespan"] == 221 < ub["behnke34"])

print("   Hexaly 15.0 baseline (our runs, same design as CP Optimizer)")
HX = HERE / "4_commercial_baselines" / "hexaly"
hruns = json.loads((HX / "results.json").read_text())
for blk, want in (("jssp_1core", (2.7, 4.4)), ("jssp_16core", (2.4, 4.2))):
    g, ok = [], True
    for k in range(1, 11):
        rs = [r for r in hruns if r["block"] == blk and r["instance"] == f"tai_100_1000_{k}"]
        sol = next((HX / "solutions" / blk).glob(f"tai_100_1000_{k}_*.txt.gz"))
        claim = int(sol.name.split("_")[-1].split(".")[0])
        with tempfile.NamedTemporaryFile("wb", suffix=".txt", delete=False) as tmp:
            tmp.write(gzip.decompress(sol.read_bytes()))
        r = cj.check(str(J / "generalization" / "instances" / f"tai_100_1000_{k}.txt"), tmp.name)
        Path(tmp.name).unlink()
        ok &= r["ok"] and r["makespan"] == claim == min(x["makespan"] for x in rs) and len(rs) == 3
        g.append(100 * (claim / lbs[k] - 1))
    report(f"Hexaly {blk}: 10 best schedules certified; 30-min gap {min(g):.1f}-{max(g):.1f}% (paper {want[0]}-{want[1]}%)",
           ok and (round(min(g), 1), round(max(g), 1)) == want)
r1 = min(r["makespan"] for r in hruns if r["block"] == "jssp_16core" and r["instance"] == "tai_100_1000_1")
report(f"Hexaly tai_100_1000_1, 16 cores: {r1:,} (paper 539,566)", r1 == 539566)
for blk, want in (("behnke_120s", 2.85), ("behnke_1200s", 2.01)):
    rs = [r for r in hruns if r["block"] == blk]
    bad = 0
    for r in rs:
        out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(B / "generalization" / "instances" / f"{r['instance']}.fjs"),
                              str(HX / "solutions" / blk / f"{r['instance']}.sched"), "--claim", str(r["makespan"])], capture_output=True, text=True)
        bad += out.returncode != 0
    dev = statistics.mean(100 * (r["makespan"] / ub[r["instance"]] - 1) for r in rs)
    report(f"Hexaly {blk}: {len(rs)} schedules certified; mean deviation {dev:.2f}% (paper {want}%)", bad == 0 and round(dev, 2) == want)

# ---------------------------------------------------------------- 5. Mode C (improvement campaigns by agents)
print("5. Mode C: improvement campaigns (own verifiers of folders 5 and 6)")
for folder, script, key in (("5_jssp_mode_C", "verify_protocolC.py", "ALL CHECKS PASSED"),
                            ("6_fjsp_behnke_mode_C", "verify_modeC.py", "ALL CHECKS PASSED")):
    out = subprocess.run([sys.executable, "-I", script], cwd=HERE / folder, capture_output=True, text=True)
    report(f"{folder}: {script}", out.returncode == 0 and key in out.stdout, (out.stdout.strip().splitlines() or [""])[-1])

print()
print("ALL CHECKS PASSED" if not failures else f"{len(failures)} CHECK(S) FAILED: {failures}")
sys.exit(1 if failures else 0)
