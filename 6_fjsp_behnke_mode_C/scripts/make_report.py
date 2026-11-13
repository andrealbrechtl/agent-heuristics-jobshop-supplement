#!/usr/bin/env python3
"""Render best/status.json as a Markdown results table (RESULTS.md) and a JSON summary.

Columns: instance, size, FJSPLib LB/UB (ScheduleOpt bks.json, commit f89e5b0), our proven LB with
its provenance, our verified UB with its provenance, gap, status.  'closed' means we hold a
verified schedule whose makespan equals a lower bound that is either FJSPLib's or one of our
cross-validated CP-SAT proofs (see lb/prove.sh).
"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))


def size(name):
    n, m = open(f'{HERE}/instances/{name}.fjs').readline().split()[:2]
    return f'{n}x{m}'


def main():
    s = json.load(open(f'{HERE}/best/status.json'))
    rows = []
    closed = new_ub = new_lb = 0
    newly_closed = []
    for k in range(1, 61):
        name = f'behnke{k}'
        v = s[name]
        lbsrc = ','.join(v['lb_src'])
        if 'fjsplib' in lbsrc:
            lbsrc = 'FJSPLib'
        elif 'cpsat120' in lbsrc:
            lbsrc = 'CP-SAT 120 s (supplement)'
        elif 'proofs' in lbsrc:
            lbsrc = 'proof ' + lbsrc.split('/')[-2]
        ubsrc = v['ub_src'].replace('best/', '')
        gap = v['ub'] - v['lb']
        st = 'closed' if v['closed'] else f'open (gap {gap})'
        if v['closed']:
            closed += 1
            if v['fjsplib_status'] == 'open':
                newly_closed.append(name)
        if v['ub'] < v['fjsplib_ub']:
            new_ub += 1
        if v['lb'] > v['fjsplib_lb']:
            new_lb += 1
        rows.append((name, size(name), f"{v['fjsplib_lb']} / {v['fjsplib_ub']}", v['lb'], lbsrc, v['ub'], ubsrc, st))
    out = [f'# Behnke & Geiger instances: current bounds (open-source tools only)\n',
           f'Closed (verified schedule at a proven or FJSPLib lower bound): **{closed} of 60** '
           f'({len(newly_closed)} newly closed vs FJSPLib: {", ".join(newly_closed)}). '
           f'Upper bounds below FJSPLib: {new_ub}. Lower bounds above FJSPLib: {new_lb}.\n',
           '| instance | size | FJSPLib LB / UB | our LB | LB source | our UB | UB source | status |',
           '|---|---|---|---|---|---|---|---|']
    for r in rows:
        out.append('| ' + ' | '.join(str(x) for x in r) + ' |')
    open(f'{HERE}/RESULTS.md', 'w').write('\n'.join(out) + '\n')
    json.dump(dict(closed=closed, newly_closed=newly_closed, new_ub=new_ub, new_lb=new_lb,
                   open=[r[0] for r in rows if not r[7].startswith('closed')]),
              open(f'{HERE}/best/summary.json', 'w'), indent=1)
    print(f'closed {closed}/60, newly closed {len(newly_closed)}, UB<FJSPLib {new_ub}, LB>FJSPLib {new_lb}')


if __name__ == '__main__':
    main()
