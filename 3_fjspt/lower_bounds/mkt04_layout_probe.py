#!/usr/bin/env python3
"""MKT04/MKT03 puzzle: which travel matrix reproduces HF2021's published LB1/LB2 (Table 19)?
Tries the MK job data with every FAST layout (n >= 8 machines, restricted to rows/cols 0..8),
the transposed 8-machine layout, and 2to8 layouts. Prints LB1, LB2 (HF definitions, hf_lb.py)."""
import json, re, sys, os, copy, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import Inst, fmt
from hf_lb import hf
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "generalization", "sources") + os.sep

def layouts(path):
    L = {}; lines = open(path).read().splitlines(); i = 0
    while i < len(lines):
        m = re.match(r"\s*(\d+)-M\s+LU\b", lines[i])
        if m:
            n = int(m.group(1)); rows = {}; i += 1
            while i < len(lines) and len(rows) < n + 1:
                t = lines[i].split()
                if t and (t[0] == "LU" or t[0].isdigit()) and len(t) == n + 2:
                    rows[0 if t[0] == "LU" else int(t[0])] = [float(x) for x in t[1:]]
                i += 1
            if sorted(rows) == list(range(n + 1)):
                L[n] = [rows[r] for r in range(n + 1)]
            continue
        i += 1
    return L

LA = layouts(SRC + "4to18machines_layouts-1.txt")
LB_ = layouts(SRC + "2to8machines_layouts.txt")
pub = {"MKT03": (117, 200), "MKT04": (80, 152)}
for name in ["MKT03", "MKT04"]:
    base = json.load(open(f"../instances/{name}.json"))
    print(name, "published LB1/LB2 =", pub[name], " machines used:", sorted({m for J in base["jobs"] for a in J for m, p in a}))
    cands = []
    for src, L in [("4to18", LA), ("2to8", LB_)]:
        for n, T in sorted(L.items()):
            if n >= 8:
                cands.append((f"{src} {n}-M layout restricted", [r[:9] for r in T[:9]]))
    T8 = LA[8]
    cands.append(("8-M transposed", [list(r) for r in zip(*T8)]))
    cands.append(("our file", base["travel"]))
    for lab, T in cands:
        d = dict(base, travel=T)
        p = tempfile.mktemp(suffix=".json"); json.dump(d, open(p, "w"))
        r = hf(Inst(p))
        mark = "  <== matches" if (float(r["LB1"]), float(r["LB2"])) == tuple(map(float, pub[name])) else ""
        print(f"   {lab:32s} LB1={fmt(r['LB1']):>6} LB2={fmt(r['LB2']):>7}{mark}")
