"""Convert instance JSON to the plain-text format read by the C++ solver (all times scaled x2 -> integers)."""
import json, sys
from pathlib import Path
def conv(src, dst):
    I = json.loads(Path(src).read_text())
    out = [I["name"], f"{I['n_jobs']} {I['n_machines']}"]
    for J in I["jobs"]:
        row = [str(len(J))]
        for alts in J:
            row.append(str(len(alts)))
            for m, p in alts:
                row += [str(m), str(int(round(2 * p)))]
        out.append(" ".join(row))
    for r in I["travel"]:
        out.append(" ".join(str(int(round(2 * x))) for x in r))
    Path(dst).write_text("\n".join(out) + "\n")
if __name__ == "__main__":
    Path("instances/txt").mkdir(exist_ok=True)
    for f in sorted(Path("instances").glob("*.json")):
        conv(f, Path("instances/txt") / (f.stem + ".txt"))
    print("converted", len(list(Path("instances/txt").glob("*.txt"))))
