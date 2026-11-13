#!/usr/bin/env python3
"""Collect the best known schedule per Behnke instance from every source in the repository,
verify each with the independent checker, and write a normalised copy to best/<inst>.sched
(0-based machines, rows 'job op machine start end').  Also writes best/status.json with the
best verified upper bound, the best known lower bound and their sources.

Lower bounds come from FJSPLib (bks.json copied into results.json) and from the proven bounds
reported by CP-SAT in the supplement.  Open-source sources only (no CP Optimizer / Hexaly).  Later runs append to
best/status.json through update_status.py.
"""
import json, os, sys, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_fjsp as C

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))  # repository root
S2 = ROOT + '/2_fjsp_behnke'
S4 = ROOT + '/4_commercial_baselines'
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = HERE + '/best'
os.makedirs(OUT, exist_ok=True)

SCHED_DIRS = [
    ('tabu120', S2 + '/generalization/solutions/tabu'),
    ('tabu1200', S2 + '/generalization/solutions/tabu_1200s'),
    ('rr_tabu120', S2 + '/generalization/rerun_same_cores/tabu_120s'),
    ('rr_tabu1200', S2 + '/generalization/rerun_same_cores/tabu_1200s'),
    ('cpsat120', S2 + '/generalization/solutions/cpsat'),
]
EXTRA = {
    'behnke16': [('agent404', S2 + '/solutions/behnke16_404.txt'), ('tabu406', S2 + '/solutions/behnke16_406.sched')],
    'behnke31': [('seed44', S2 + '/solutions/behnke31_226_seed44.sched')],
    'behnke14': [('tabu232', S2 + '/solutions/behnke14_232.sched')],
}


def verify(inst, path):
    """Return (makespan, rows0) with 0-based machines if feasible under exactly one base, else None."""
    claim, meta, rows = C.read_schedule(path)
    ok = []
    for off in (0, 1):
        errs, ms = C.check(inst, rows, off)
        if not errs:
            ok.append((off, ms))
    if len(ok) != 1:
        return None
    off, ms = ok[0]
    if claim is not None and claim != ms:
        return None
    return ms, [(j, o, mm - off, st, en) for (j, o, mm, st, en) in rows]


def main():
    g = {r['instance']: r for r in json.load(open(S2 + '/generalization/results.json'))}
    status = {}
    for k in range(1, 61):
        name = f'behnke{k}'
        ipath = f'{S2}/generalization/instances/{name}.fjs'
        inst = C.read_instance(ipath)
        cands = [(src, f'{d}/{name}.sched') for src, d in SCHED_DIRS] + EXTRA.get(name, [])
        best = None
        for src, p in cands:
            if not os.path.exists(p):
                continue
            v = verify(inst, p)
            if v is None:
                print(f'WARNING {name}: {p} failed verification', file=sys.stderr)
                continue
            ms, rows = v
            if best is None or ms < best[0]:
                best = (ms, rows, src, p)
        ms, rows, src, p = best
        with open(f'{OUT}/{name}.sched', 'w') as f:
            f.write(f'makespan {ms}\n# source {src} {p}\n# machine ids 0-based\n# job op machine start end\n')
            for r in sorted(rows):
                f.write(' '.join(map(str, r)) + '\n')
        # re-verify the written file
        v2 = verify(inst, f'{OUT}/{name}.sched')
        assert v2 is not None and v2[0] == ms, name
        # lower bounds
        lbs = {'fjsplib': g[name]['lb'], 'cpsat120': g[name]['cpsat_bound']}
        lb = max(lbs.values())
        lbsrc = [s for s, v in lbs.items() if v == lb]
        status[name] = dict(ub=ms, ub_src=src, ub_file=f'best/{name}.sched', lb=lb, lb_src=lbsrc,
                            fjsplib_lb=g[name]['lb'], fjsplib_ub=g[name]['ub'], fjsplib_status=g[name]['status'],
                            closed=(lb == ms), history=[])
        flag = 'CLOSED' if lb == ms else f'gap {ms - lb}'
        print(f'{name:9s} UB {ms:4d} ({src:12s})  LB {lb:4d} ({",".join(lbsrc)})  {flag}')
    json.dump(status, open(f'{OUT}/status.json', 'w'), indent=1)
    print('closed:', sum(1 for v in status.values() if v['closed']), 'of 60')


if __name__ == '__main__':
    main()
