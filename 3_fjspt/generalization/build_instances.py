#!/usr/bin/env python3
"""Build SFJST01-10 / MFJST01-10 (Homayouni & Fontes 2021, data set 3) as FJSPT instance JSONs.
Job data: FAST project file fjspt_hf2020 (Fattahi et al. 2007 sets, precedence format).
Layout: FAST 2to8machines_layouts, layout with the same number of machines as the instance
(HF2021 Sec. 5.4: literature layouts for 4,5,6,8 machines, generated times 2..10 for 2,3,7).
Cross-checks: SchedulingLab/fjsp-instances (fattahi/*.json) and lucasberter/FJSPT (fattahi/*.txt).
Usage: build_instances.py SRC_DIR(OUT/sources) FJSPT_REPO FJSP_INSTANCES_REPO OUT_DIR"""
import json, re, sys
from pathlib import Path
src, rep_fjspt, rep_sl, out = map(Path, sys.argv[1:5])

def parse_layouts(path):
    L = {}
    lines = [l.split() for l in Path(path).read_text().splitlines() if l.strip()]
    i = 0
    while i < len(lines):
        if len(lines[i]) >= 3 and lines[i][1] == "Machine" and lines[i][2] == "layout":
            k = int(lines[i][0]); rows = lines[i + 2: i + 3 + k]   # skip '<k>-M LU 1..k' header
            assert len(rows) == k + 1 and rows[0][0] == "LU"
            M = [[float(x) for x in r[1:]] for r in rows]
            assert all(len(r) == k + 1 for r in M)
            L[k] = M; i += k + 3
        else: i += 1
    return L
lay = parse_layouts(src / "2to8machines_layouts.txt")
lay2 = parse_layouts(src / "4to18machines_layouts-1.txt")
for k in lay:
    if k in lay2: print(f"layout {k}: 2to8 vs 4to18 file:", "identical" if lay[k] == lay2[k] else "DIFFERENT")

def parse_fattahi(path):
    L = [l.split() for l in Path(path).read_text().splitlines() if l.strip()]
    res, i = {}, 0
    while i < len(L):
        if len(L[i]) == 3 and L[i][1].count("*") == 2 and L[i][1].replace("*", "").isdigit():
            name = L[i][0]; M, J, O = map(int, L[i][1].split("*")); i += 1
            prec = []
            while len(L[i]) == 2: prec.append((int(L[i][0]), int(L[i][1]))); i += 1
            ops = []
            for _ in range(O):
                t = list(map(int, L[i])); i += 1
                assert len(t) == 1 + 2 * t[0], (name, t)
                ops.append([[t[1 + 2 * a] + 1, t[2 + 2 * a]] for a in range(t[0])])  # machines 1-based
            nxt = dict(prec); assert len(nxt) == len(prec)
            heads = set(range(O)) - set(nxt.values()); assert len(heads) == J, (name, len(heads), J)
            jobs = []
            for h in sorted(heads):
                ch = [h]
                while ch[-1] in nxt: ch.append(nxt[ch[-1]])
                jobs.append([ops[o] for o in ch]); used = ch
            assert sum(len(j) for j in jobs) == O
            res[name] = (M, jobs, sorted(heads))
        else: i += 1
    return res
data = parse_fattahi(src / "fjspt_hf2020.txt")
out.mkdir(exist_ok=True, parents=True)
report = {}
for kind in ("sfjs", "mfjs"):
    for n in range(1, 11):
        fn = f"{kind}{n:02d}"; nm = ("SFJST" if kind == "sfjs" else "MFJST") + f"{n:02d}"
        M, jobs, heads = data[fn]
        inst = {"name": nm, "n_jobs": len(jobs), "n_machines": M, "jobs": jobs,
                "travel": lay[M], "layout_machines": M, "source_jobset": fn}
        # cross-check 1: SchedulingLab json
        sl = json.load(open(rep_sl / "fattahi" / f"{fn}.json"))
        slj = [[[[a["machine"] + 1, a["processing"]] for a in op] for op in job] for job in sl["jobs"]]
        c1 = (slj == jobs); c1m = sl["machines"] == M
        # cross-check 2: lucasberter fattahi txt (same precedence-style format, O J M header, no name line)
        T = [l.split() for l in (rep_fjspt / "fattahi" / f"{fn}.txt").read_text().splitlines() if l.strip()]
        O_, J_, M_ = map(int, T[0]); k = 1; prec = []
        while len(T[k]) == 2: prec.append((int(T[k][0]), int(T[k][1]))); k += 1
        ops = [[[int(t[1 + 2*a]) + 1, int(t[2 + 2*a])] for a in range(int(t[0]))] for t in map(lambda r: list(map(int, r)), T[k:k + O_])]
        nxt = dict(prec); hs = sorted(set(range(O_)) - set(nxt.values())); jb = []
        for h in hs:
            ch = [h]
            while ch[-1] in nxt: ch.append(nxt[ch[-1]])
            jb.append([ops[o] for o in ch])
        c2 = (jb == jobs and M_ == M)
        report[nm] = (c1 and c1m, c2)
        print(f"{nm}: {M}m {len(jobs)}j {sum(map(len, jobs))}ops  SchedulingLab={'OK' if c1 and c1m else 'DIFF'}  Berterottiere-fattahi={'OK' if c2 else 'DIFF'}")
        json.dump(inst, open(out / f"{nm}.json", "w"))
assert all(a and b for a, b in report.values()), "cross-check failed"
print("ALL job data identical in the three sources")
