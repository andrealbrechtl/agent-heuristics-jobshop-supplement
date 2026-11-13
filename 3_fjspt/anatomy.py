"""Anatomy statistics of a certified FJSPT schedule (independent of the authors' scripts)."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fractions import Fraction as F
from check_fjspt import load_instance, load_solution, check

def min_transports(jobs):
    tot = 0
    for J in jobs:  # DP over machine choices minimising machine changes (+1 for the first op)
        best = {m: 1 for m in J[0]}
        for alts in J[1:]:
            best = {m: min(c + (0 if a == m else 1) for a, c in best.items()) for m in alts}
        tot += min(best.values())
    return tot

def critical(inst, sol, prefer):
    name, jobs, T = load_instance(inst); ops, trs, _, _ = load_solution(sol)
    mpred, vpred = {}, {}
    for d, key in [(ops, 'm'), (trs, 'v')]:
        groups = {}
        for k, val in d.items():
            g = val[0]; t0 = val[1] if key == 'm' else val[3]
            groups.setdefault(g, []).append((t0, k))
        for L in groups.values():
            L.sort()
            for (a, k1), (b, k2) in zip(L, L[1:]):
                (mpred if key == 'm' else vpred)[k2] = k1
    node = ('P', max(ops, key=lambda k: ops[k][2])); cnt = {'vehicle': 0, 'machine': 0, 'job': 0, 'start': 0}
    while node:
        typ, k = node; j, o = k
        cands = []
        if typ == 'P':
            m, s, e = ops[k]
            if k in trs and trs[k][4] == s: cands.append(('job', ('T', k)))
            if k not in trs and o > 1 and ops[(j, o-1)][2] == s: cands.append(('job', ('P', (j, o-1))))
            if k in mpred and ops[mpred[k]][2] == s: cands.append(('machine', ('P', mpred[k])))
        else:
            v, a, b, d, r = trs[k]
            if o > 1 and ops[(j, o-1)][2] == d: cands.append(('job', ('P', (j, o-1))))
            if k in vpred:
                pv = vpred[k]
                if trs[pv][4] + T[trs[pv][2]][a] == d: cands.append(('vehicle', ('T', pv)))
            elif d == T[0][a]: cands.append(('start', None))
        if not cands:
            cnt['start'] += 1; break
        cands.sort(key=lambda c: prefer.index(c[0]))
        kind, node = cands[0]; cnt[kind] += 1
    cnt['total'] = sum(cnt.values())
    return cnt

if __name__ == "__main__":
    inst, sol = sys.argv[1], sys.argv[2]
    name, ok, mk, ntr, errs, stats = check(inst, sol)
    _, jobs, _ = load_instance(inst)
    out = dict(instance=name, feasible=ok, makespan=float(mk), transports=ntr, min_transports=min_transports(jobs),
               vehicles={v: {k: float(x) for k, x in s.items()} for v, s in stats.items()},
               critical_path_prefer_job=critical(inst, sol, ['job', 'machine', 'vehicle', 'start']),
               critical_path_prefer_vehicle=critical(inst, sol, ['vehicle', 'machine', 'job', 'start']))
    print(json.dumps(out))
