"""Compare the 31 FJSPT instances (../instances/*.json) with every upstream source.

Run through fetch_and_compare.sh, which downloads the sources at pinned versions.

Sources (paths given on the command line):
  fast/      FAST project PDFs converted with `pdftotext -layout`
             (4to18machines_layouts-1.txt, chambersbarnes1996-1.txt, brandimarte1993-2.txt)
  fjsp-instances/   SchedulingLab/fjsp-instances (0-based FJSP job data)
  FJSP_AGV_MILP/    wangyuting9836/FJSP_AGV_MILP (MK job data + travel matrix, 1-based)
  ours/      instances/*.json
"""
import json, re, sys, os
from pathlib import Path

fast, sl, milp, ours = map(Path, sys.argv[1:5])
out = None

def num(x):
    v = float(x)
    return v

# ---------- layouts from FAST PDF ----------
layouts = {}
lines = (fast / "4to18machines_layouts-1.txt").read_text().splitlines()
i = 0
while i < len(lines):
    m = re.match(r"\s*(\d+)-M\s+LU\b", lines[i])
    if m:
        n = int(m.group(1)); rows = {}
        i += 1
        while i < len(lines) and len(rows) < n + 1:
            t = lines[i].split()
            if t and (t[0] == "LU" or t[0].isdigit()) and len(t) == n + 2:
                rows[0 if t[0] == "LU" else int(t[0])] = [num(x) for x in t[1:]]
            i += 1
        assert sorted(rows) == list(range(n + 1)), (n, sorted(rows))
        layouts[n] = [rows[r] for r in range(n + 1)]
        continue
    i += 1

# ---------- FJSP job data ----------
def parse_fjs_tokens(tok, base):
    n, m = int(tok[0]), int(tok[1]); p = 2
    # skip optional avg-machines field (float) if present
    if "." in tok[2] or (len(tok) > 3 and False):
        p = 3
    jobs = []
    for _ in range(n):
        k = int(tok[p]); p += 1; J = []
        for _ in range(k):
            a = int(tok[p]); p += 1; alts = []
            for _ in range(a):
                alts.append([int(tok[p]) + (1 - base), num(tok[p + 1])]); p += 2
            J.append(alts)
        jobs.append(J)
    return n, m, jobs, p

def sl_jobs(path):
    tok = path.read_text().split()
    n, m = int(tok[0]), int(tok[1])
    # SchedulingLab txt: header "n m" (no avg), 0-based machines
    p = 2; jobs = []
    for _ in range(n):
        k = int(tok[p]); p += 1; J = []
        for _ in range(k):
            a = int(tok[p]); p += 1
            J.append([[int(tok[p + 2*q]) + 1, num(tok[p + 2*q + 1])] for q in range(a)]); p += 2 * a
        jobs.append(J)
    assert p == len(tok), path
    return n, m, jobs

def milp_mk(path):
    tok = path.read_text().split()
    n, m = int(tok[0]), int(tok[1]); p = 3; jobs = []
    for _ in range(n):
        k = int(tok[p]); p += 1; J = []
        for _ in range(k):
            a = int(tok[p]); p += 1
            J.append([[int(tok[p + 2*q]), num(tok[p + 2*q + 1])] for q in range(a)]); p += 2 * a
        jobs.append(J)
    rest = [num(x) for x in tok[p:]]
    sz = int(round(len(rest) ** 0.5)); assert sz * sz == len(rest)
    travel = [rest[r*sz:(r+1)*sz] for r in range(sz)]
    return n, m, jobs, travel

def fast_cb_blocks():
    """Chambers-Barnes job data as printed in the FAST PDF (1-based)."""
    txt = (fast / "chambersbarnes1996-1.txt").read_text()
    blocks = {}
    for m in re.finditer(r"^\s*([a-z0-9]+):?\s*\n(.*?)(?=^\s*[a-z0-9]+:?\s*$|\Z)", txt, re.S | re.M):
        name, body = m.group(1), m.group(2)
        tok = body.split()
        try:
            n, mm = int(tok[0]), int(tok[1]); p = 3; jobs = []
            for _ in range(n):
                k = int(tok[p]); p += 1; J = []
                for _ in range(k):
                    a = int(tok[p]); p += 1
                    J.append([[int(tok[p + 2*q]), num(tok[p + 2*q + 1])] for q in range(a)]); p += 2 * a
                jobs.append(J)
            blocks[name] = (n, mm, jobs)
        except (IndexError, ValueError) as e:
            blocks[name] = ("PARSE-ERROR", str(e))
    return blocks

def fast_mk_blocks():
    txt = (fast / "brandimarte1993-2.txt").read_text()
    blocks = {}
    for m in re.finditer(r"^\s*Mk(\d+):\s*\n(.*?)(?=^\s*Mk\d+:\s*$|\Z)", txt, re.S | re.M):
        k, body = int(m.group(1)), m.group(2)
        tok = body.split()
        try:
            n, mm = int(tok[0]), int(tok[1]); p = 3; jobs = []
            for _ in range(n):
                kk = int(tok[p]); p += 1; J = []
                for _ in range(kk):
                    a = int(tok[p]); p += 1
                    J.append([[int(tok[p + 2*q]), num(tok[p + 2*q + 1])] for q in range(a)]); p += 2 * a
                jobs.append(J)
            blocks[k] = (n, mm, jobs)
        except (IndexError, ValueError) as e:
            blocks[k] = ("PARSE-ERROR (pdftotext drops spaces in this block)", str(e))
    return blocks

cb_fast = fast_cb_blocks(); mk_fast = fast_mk_blocks()
names = [f"MKT{k:02d}" for k in range(1, 11)] + sorted(
    p.stem for p in ours.glob("*.json") if not p.stem.startswith("MKT"))

report = []; allok = True; notes = []
so = Path(sys.argv[6]) if len(sys.argv) > 6 else None
def so_mk(k):
    tok = (so / f"mk{k:02d}.txt").read_text().split()
    n, m = int(tok[0]), int(tok[1])
    for p0 in (3, 2):
        try:
            p = p0; jobs = []
            for _ in range(n):
                kk = int(tok[p]); p += 1; J = []
                for _ in range(kk):
                    a = int(tok[p]); p += 1
                    J.append([[int(tok[p + 2*q]), num(tok[p + 2*q + 1])] for q in range(a)]); p += 2 * a
                jobs.append(J)
            if p == len(tok):
                return n, m, jobs
        except (IndexError, ValueError):
            pass
    raise ValueError(k)
for name in names:
    O = json.loads((ours / f"{name}.json").read_text())
    checks = []
    if name.startswith("MKT"):
        k = int(name[3:])
        n, m, jobs = sl_jobs(sl / "brandimarte" / f"mk{k:02d}.txt")
        mn, mm, mjobs, mtrav = milp_mk(milp / "benchmark" / "MK" / f"Mk{k}.dat")
        ref_travel = layouts[m]
        sn, sm, sjobs = so_mk(k)
        used = [0] + sorted({mm for J in O["jobs"] for o in J for mm, _ in o})
        def sub(T):
            return [[T[a][b] for b in used] for a in used]
        ref_travel = layouts[O["n_machines"]]
        checks += [("jobs == SchedulingLab mk%02d" % k, O["jobs"] == jobs),
                   ("jobs == ScheduleOpt mk%02d" % k, O["jobs"] == sjobs),
                   ("jobs == FJSP_AGV_MILP Mk%d" % k, O["jobs"] == mjobs),
                   ("travel == FAST %d-machine layout" % O["n_machines"], O["travel"] == ref_travel),
                   ("travel(used machines) == FJSP_AGV_MILP Mk%d" % k, sub(O["travel"]) == sub(mtrav))]
        if m != O["n_machines"] or mm != O["n_machines"] or sm != O["n_machines"]:
            notes.append(f"{name}: header machines ours={O['n_machines']} SchedulingLab={m} ScheduleOpt={sm} FJSP_AGV_MILP={mm}; machines used {used[1:]}")
        fb = mk_fast.get(k)
        if fb and fb[0] != "PARSE-ERROR (pdftotext drops spaces in this block)" and isinstance(fb[0], int):
            checks.append(("jobs == FAST brandimarte PDF", O["jobs"] == fb[2]))
        else:
            checks.append(("jobs == FAST brandimarte PDF", None))
    else:
        base = name[:-1]
        n, m, jobs = sl_jobs(sl / "barnes" / f"{base}.txt")
        checks += [("jobs == SchedulingLab %s" % base, O["jobs"] == jobs and O["n_machines"] == m),
                   ("travel == FAST %d-machine layout" % m, O["travel"] == layouts[m])]
        fb = cb_fast.get(base)
        if base == "setb4xxx":
            # Known data error: the FAST PDF's setb4xxx block is a verbatim copy of seti5xxx
            # (15x18). Our file uses the correct setb4xxx job data (SchedulingLab, 15x13).
            checks.append(("FAST setb4xxx block == seti5xxx copy (known error, repaired)",
                           cb_fast.get("setb4xxx") == cb_fast.get("seti5xxx")))
        elif fb and isinstance(fb[0], int):
            checks.append(("jobs == FAST chambersbarnes PDF", O["jobs"] == fb[2] and fb[1] == m))
        else:
            checks.append(("jobs == FAST chambersbarnes PDF", None))
    ok = all(c is not False for _, c in checks)
    allok &= ok
    report.append((name, O["n_jobs"], O["n_machines"], sum(len(J) for J in O["jobs"]), checks))
    if out:
        out.mkdir(parents=True, exist_ok=True)

for name, n, m, nops, checks in report:
    s = "  ".join(f"{lab}: {'OK' if c else ('n/a (PDF text not parseable)' if c is None else 'MISMATCH')}" for lab, c in checks)
    print(f"{name:10s} {n:2d}x{m:2d} ops={nops:3d}  {s}")
for n_ in notes: print("NOTE", n_)
print("ALL OK" if allok else "SOME MISMATCH")

# extra: the FAST setb4xxx block vs seti5xxx
if "setb4xxx" in cb_fast and "seti5xxx" in cb_fast:
    a, b = cb_fast["setb4xxx"], cb_fast["seti5xxx"]
    print("FAST PDF: setb4xxx block identical to seti5xxx block:", a == b, " setb4xxx header:", a[:2] if isinstance(a[0], int) else a)
