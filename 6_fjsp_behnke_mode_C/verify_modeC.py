#!/usr/bin/env python3
"""Re-check the mode C Behnke schedules with the paper's checker and recompute the paper's counts."""
import json, subprocess, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
B = HERE.parent / "2_fjsp_behnke"
res = json.loads((HERE / "results.json").read_text())
ok = True
for r in res:
    i = r["instance"]
    out = subprocess.run([sys.executable, "-I", str(B / "check_fjsp.py"), str(B / "generalization" / "instances" / f"{i}.fjs"),
                          str(HERE / "solutions" / f"{i}.sched"), "--claim", str(r["makespan"])], capture_output=True, text=True)
    good = out.returncode == 0 and r["closed"] == (r["makespan"] == r["fjsplib_lb"])
    ok &= good
    if not good:
        print("FAIL", i)
closed = [r["instance"] for r in res if r["closed"]]
newly = [r["instance"] for r in res if r["closed"] and r["fjsplib_status"] == "open"]
below = [r["instance"] for r in res if r["makespan"] < r["fjsplib_ub"]]
print(f"60 schedules feasible at the stated makespan: {ok}")
print(f"closed (= FJSPLib LB): {len(closed)}; newly closed: {newly}; below FJSPLib UB: {len(below)}")
ok &= len(closed) == 34 and newly == ["behnke32", "behnke34", "behnke40", "behnke55", "behnke56", "behnke60"] and len(below) == 18
print("ALL CHECKS PASSED" if ok else "CHECK FAILED")
sys.exit(0 if ok else 1)
