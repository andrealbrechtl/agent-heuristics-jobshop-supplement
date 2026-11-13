#!/usr/bin/env python3
"""Replay Meng et al. (2023) Appendix A strings for MFJST09/10 (text in lit/meng2023.txt) with the
supplement's replay() (same encoding as for MKT) and write .sol files for check_fjspt.py.
Usage: replay_meng_mfjst.py meng2023.txt INSTANCE_DIR OUT_DIR"""
import json, re, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "literature_replay"))
from replay_meng_iga import replay
txt, idir, od = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]); od.mkdir(exist_ok=True, parents=True)
T = open(txt).read()
for name, rep in (("MFJST09", 1117), ("MFJST10", 1228)):
    blk = T[T.index(name + ":\noperation sequence"):]
    g = lambda key: re.search(key + r":\s*([0-9 ]+)", blk).group(1)
    I = json.load(open(idir / f"{name}.json"))
    mk, ops, trs = replay(I, g("operation sequence"), g("machine selection"), g("AGV selection"))
    f = lambda x: f"{float(x):g}"
    with open(od / f"meng2023_iga_{name}.sol", "w") as o:
        o.write(f"instance {name}\nmakespan {f(mk)}\n# replay of Meng et al. (2023) Appendix A; reported {rep}\n")
        for t in trs: o.write("tr %d %d %d %d %d %s %s\n" % (*t[:5], f(t[5]), f(t[6])))
        for t in ops: o.write("op %d %d %d %s %s\n" % (*t[:3], f(t[3]), f(t[4])))
    print(f"{name}: replayed {f(mk)}, reported {rep}", "MATCH" if mk == rep else "DIFFERENT")
