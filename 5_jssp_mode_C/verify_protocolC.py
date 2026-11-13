"""Re-check every Protocol C result.

1. Every schedule in solutions/ passes the independent checker check_jssp.py (copied unchanged
   from 1_jssp_tai_100_1000_1/) at exactly the makespan in its file name.
2. Schedules marked _optimal have makespan = longest job (a lower bound), so they are optimal.
3. The raw solver outputs in solutions/raw_solver_output/ are feasible at the logged makespan, and the
   k=4 and k=9 optima are byte-identical to them.
4. The k=8 schedule 527,280 is the left-compaction (solver/compact.py) of the raw 527,428 output.
5. If this folder sits next to 1_jssp_tai_100_1000_1/ (the paper supplement), the compacted
   schedules for k = 1, 5, 6, 7, 10 are recomputed from the paper's 16-core schedules and compared
   bit for bit; otherwise this step is skipped with a note.
6. The instances are byte-identical to the paper supplement copies (if present).

usage: python verify_protocolC.py        (from this folder; numpy required)
"""
import gzip
import hashlib
import io
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'solver'))
from compact import compact, load_instance  # noqa: E402

PAPER = os.path.join(HERE, '..', '1_jssp_tai_100_1000_1')
fails = 0


def report(ok, msg):
    global fails
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        fails += 1


def inst(k):
    return os.path.join(HERE, 'instances', f'tai_100_1000_{k}.txt')


def lower_bound(k):
    _, d = load_instance(inst(k))
    return int(d.sum(1).max())


def checker(k, path):
    out = subprocess.run([sys.executable, os.path.join(HERE, 'check_jssp.py'), inst(k), path],
                         capture_output=True, text=True).stdout
    ok = 'ok=True' in out
    m = re.search(r'makespan=(\d+)', out)
    return ok, int(m.group(1)) if m else None


def load(path):
    if path.endswith('.gz'):
        with gzip.open(path, 'rt') as f:
            return np.loadtxt(io.StringIO(f.read()), comments='#', dtype=np.int64)
    return np.loadtxt(path, comments='#', dtype=np.int64)


# 1 + 2: all final schedules
for sub in ('solutions', os.path.join('solutions', 'refound')):
    for fn in sorted(os.listdir(os.path.join(HERE, sub))):
        m = re.match(r'tai_100_1000_(\d+)_(\d+)(_optimal)?\.txt$', fn)
        if not m:
            continue
        k, claim, opt = int(m.group(1)), int(m.group(2)), bool(m.group(3))
        ok, mk = checker(k, os.path.join(HERE, sub, fn))
        report(ok and mk == claim, f'{sub}/{fn}: checker ok={ok} makespan={mk} (claimed {claim})')
        if opt:
            lb = lower_bound(k)
            report(mk == lb, f'{fn}: makespan {mk} = longest job {lb} -> optimal')

# 3: raw solver outputs
raw = os.path.join(HERE, 'solutions', 'raw_solver_output')
for fn, k, claim in (('tai_100_1000_9_528097_solver_output.txt', 9, 528097),
                     ('tai_100_1000_4_527101_solver_output.txt', 4, 527101),
                     ('tai_100_1000_8_527428_solver_output.txt', 8, 527428)):
    ok, mk = checker(k, os.path.join(raw, fn))
    report(ok and mk == claim, f'raw_solver_output/{fn}: checker ok={ok} makespan={mk}')
for k, mk, final in ((9, 528097, 'tai_100_1000_9_528097_optimal.txt'), (4, 527101, 'tai_100_1000_4_527101_optimal.txt')):
    a = open(os.path.join(raw, f'tai_100_1000_{k}_{mk}_solver_output.txt'), 'rb').read()
    b = open(os.path.join(HERE, 'solutions', final), 'rb').read()
    report(a == b, f'k={k}: final schedule is byte-identical to the solver output')

# 4: k=8 compaction
mach, dur = load_instance(inst(8))
C = compact(mach, dur, load(os.path.join(raw, 'tai_100_1000_8_527428_solver_output.txt')))
report(np.array_equal(C, load(os.path.join(HERE, 'solutions', 'tai_100_1000_8_527280.txt'))),
       'k=8: 527,280 = compact(raw 527,428 solver output), bit for bit')

# 5: compactions of the paper's 16-core schedules
src = {1: 'tai_100_1000_1_r2_525515.txt.gz', 5: 'tai_100_1000_5_r1_525865.txt.gz',
       6: 'tai_100_1000_6_r1_524812.txt.gz', 7: 'tai_100_1000_7_r1_527000.txt.gz',
       10: 'tai_100_1000_10_r3_526000.txt.gz'}
ours = {1: 525389, 5: 525657, 6: 524604, 7: 526771, 10: 525782}
sol16 = os.path.join(PAPER, 'generalization', '16cores', 'solutions')
if os.path.isdir(sol16):
    for k, f in src.items():
        mach, dur = load_instance(inst(k))
        C = compact(mach, dur, load(os.path.join(sol16, f)))
        mine = load(os.path.join(HERE, 'solutions', f'tai_100_1000_{k}_{ours[k]}.txt'))
        report(np.array_equal(C, mine), f'k={k}: {ours[k]} = compact(paper {f}), bit for bit')
else:
    print('SKIP compaction re-derivation: paper supplement folder 1_jssp_tai_100_1000_1/ not found next to this one')

# 6: instance identity
pinst = os.path.join(PAPER, 'generalization', 'instances')
if os.path.isdir(pinst):
    for k in range(1, 11):
        a = hashlib.sha256(open(inst(k), 'rb').read()).hexdigest()
        b = hashlib.sha256(open(os.path.join(pinst, f'tai_100_1000_{k}.txt'), 'rb').read()).hexdigest()
        report(a == b, f'instance k={k} identical to the paper supplement copy (sha256 {a[:12]}...)')
else:
    print('SKIP instance identity: paper supplement folder not found next to this one')

print('ALL CHECKS PASSED' if fails == 0 else f'{fails} CHECK(S) FAILED')
sys.exit(1 if fails else 0)
