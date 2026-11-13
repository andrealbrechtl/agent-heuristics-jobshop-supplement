"""Replay Meng et al. 2025 (TSMC) solutions given as machine sequences + AGV transport sequences
(GitHub mengleilei/FJSP-AGV, 'improved best known solutions for FJSP-AGV.docx').
Semi-active timing: each op starts as early as allowed by job predecessor, transport arrival and
machine predecessor; each transport departs as early as allowed by job predecessor end and vehicle
(previous drop + empty travel to pickup). Writes .sol files for check_fjspt.py."""
import sys, re, json, csv, subprocess
from fractions import Fraction as F
from pathlib import Path
S = Path(__file__).resolve().parents[2]  # .../3_fjspt
txt, outdir = sys.argv[1], Path(sys.argv[2]); outdir.mkdir(parents=True, exist_ok=True)
T = open(txt).read()
blocks = re.split(r'\n\s*(?=[A-Za-z]+[0-9][A-Za-z0-9]*\s*:\s*\n\s*Makespan)', '\n' + T)
def canon(n):
    n = n.strip()
    if n.lower().startswith('mkt'): return 'MKT' + n[3:]
    if n.lower().startswith('mfjst'): return 'MFJST' + n[5:]
    return n[0].lower() + n[1:]
def ipath(n):
    for d in ('instances', 'generalization/instances'):
        p = S / d / f'{n}.json'
        if p.exists(): return p
    return None
pairs = lambda s: [(int(a), int(b)) for a, b in re.findall(r'\(\s*(\d+)\s+(\d+)\s*\)', s)]
rows = []
for b in blocks:
    m = re.match(r'\s*([A-Za-z]+[0-9][A-Za-z0-9]*)\s*:\s*\n\s*Makespan\s*[:：]\s*([0-9.]+)', b)
    if not m: continue
    name, rep = canon(m.group(1)), m.group(2)
    p = ipath(name)
    if p is None:
        rows.append(dict(instance=name, claimed=rep, replayed='', status='no instance file in supplement')); continue
    I = json.load(open(p)); jobs = I['jobs']; Tr = [[F(str(x)) for x in r] for r in I['travel']]
    mseq = {int(k): pairs(v) for k, v in re.findall(r'machine\s*:\s*(\d+)\s*(.*?)(?=machine\s*:|AGV\s*\d|\Z)', b, re.S)}
    vseq = {int(k): pairs(v) for k, v in re.findall(r'AGV\s*(\d+)\s*:\s*(.*?)(?=machine\s*:|AGV\s*\d|\Z)', b, re.S)}
    err = []
    assign = {}
    for mm, L in mseq.items():
        for jo in L:
            if jo in assign: err.append(f'op {jo} on two machines')
            assign[jo] = mm
    allops = {(j + 1, o + 1) for j, J in enumerate(jobs) for o in range(len(J))}
    if set(assign) != allops: err.append(f'machine lists cover {len(assign)} ops, instance has {len(allops)}; missing {sorted(allops-set(assign))[:5]} extra {sorted(set(assign)-allops)[:5]}')
    proc = {}
    for (j, o), mm in assign.items():
        if (j, o) in allops:
            d = dict(jobs[j - 1][o - 1])
            if mm not in d: err.append(f'op {(j,o)} on ineligible machine {mm}')
            else: proc[(j, o)] = F(str(d[mm]))
    need = {(j, o) for (j, o) in allops if (j,o) in assign and (o == 1 or assign.get((j, o - 1)) != assign[(j, o)])}
    carried = {}
    for v, L in vseq.items():
        for jo in L:
            if jo in carried: err.append(f'op {jo} transported twice')
            carried[jo] = v
    if set(carried) != need:
        err.append(f'transport mismatch: missing {sorted(need-set(carried))[:5]} unnecessary {sorted(set(carried)-need)[:5]}')
    if err:
        rows.append(dict(instance=name, claimed=rep, replayed='', status='DECODE ERROR: ' + '; '.join(err))); continue
    # event-driven fixed-order evaluation
    mpos = {jo: (mm, i) for mm, L in mseq.items() for i, jo in enumerate(L)}
    vpos = {jo: (v, i) for v, L in vseq.items() for i, jo in enumerate(L)}
    S_, E_, TD, TA = {}, {}, {}, {}
    def loc(j, o):  # location of job before op o
        return 0 if o == 1 else assign[(j, o - 1)]
    done_tr, done_op = set(), set()
    changed = True
    while changed:
        changed = False
        for jo in sorted(need - done_tr):
            j, o = jo; v, i = vpos[jo]
            if o > 1 and (j, o - 1) not in done_op: continue
            if i > 0 and vseq[v][i - 1] not in done_tr: continue
            ready = E_[(j, o - 1)] if o > 1 else F(0)
            if i > 0:
                pj = vseq[v][i - 1]; vt, vl = TA[pj], assign[pj]
            else:
                vt, vl = F(0), 0
            a = loc(j, o)
            dep = max(ready, vt + Tr[vl][a]); TD[jo] = dep; TA[jo] = dep + Tr[a][assign[jo]]
            done_tr.add(jo); changed = True
        for jo in sorted(allops - done_op):
            j, o = jo; mm, i = mpos[jo]
            if o > 1 and (j, o - 1) not in done_op: continue
            if jo in need and jo not in done_tr: continue
            if i > 0 and mseq[mm][i - 1] not in done_op: continue
            r = TA[jo] if jo in need else (E_[(j, o - 1)] if o > 1 else F(0))
            if i > 0: r = max(r, E_[mseq[mm][i - 1]])
            S_[jo] = r; E_[jo] = r + proc[jo]; done_op.add(jo); changed = True
    if len(done_op) < len(allops):
        rows.append(dict(instance=name, claimed=rep, replayed='', status=f'DEADLOCK: sequences cyclic ({len(allops)-len(done_op)} ops unschedulable)')); continue
    mk = max(E_.values()); g = lambda x: f'{float(x):g}'
    sol = outdir / f'meng2025_{name}.sol'
    with open(sol, 'w') as f:
        f.write(f'instance {name}\nmakespan {g(mk)}\n# semi-active replay of Meng et al. 2025 (TSMC) GitHub docx machine/AGV sequences; reported {rep}\n')
        for jo in sorted(need, key=lambda x: (TD[x], x)):
            f.write('tr %d %d %d %d %d %s %s\n' % (jo[0], jo[1], carried[jo], loc(*jo), assign[jo], g(TD[jo]), g(TA[jo])))
        for jo in sorted(allops, key=lambda x: (S_[x], x)):
            f.write('op %d %d %d %s %s\n' % (jo[0], jo[1], assign[jo], g(S_[jo]), g(E_[jo])))
    r = subprocess.run([sys.executable, '-I', str(S / 'check_fjspt.py'), str(p), str(sol)], capture_output=True, text=True)
    last = (r.stdout + r.stderr).strip().splitlines()
    rows.append(dict(instance=name, claimed=rep, replayed=g(mk), status=' / '.join(l.strip() for l in last[-3:])[:300]))
for r in rows: print(r)
json.dump(rows, open(outdir / '_rows.json', 'w'))
