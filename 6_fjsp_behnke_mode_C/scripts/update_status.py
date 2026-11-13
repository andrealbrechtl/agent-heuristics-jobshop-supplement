#!/usr/bin/env python3
"""Ingest every run result under work/, verify schedules with the independent checker, and
update best/status.json and best/<inst>.sched.

  * schedules: every *.sched / *.txt under runs/, lb/runs/ and lb/proofs/ whose basename starts
    with behnke<k>; a verified makespan below the recorded UB replaces best/<inst>.sched
  * cross-checked lower bounds ('lb'): ONLY from lb/proofs/*/verdict.json with verdict PROVED, i.e.
    the full model, no hint, INFEASIBLE at cap T under both OR-Tools 9.15 and 9.14 and with and
    without the pool constraints (bound T+1).  Single CP-SAT runs are never adopted.
  * candidate lower bounds ('lb_cand'): the best 'bound' of any single lb_cpsat.py run under
    lb/runs/ (guidance only; shown in the table, never used for 'closed')

The initial 'lb' values (FJSPLib, and the CP-SAT 120 s bounds of the supplement) are kept as
recorded by collect_best.py; 'lb_src' tells which.  Usage: update_status.py [--quiet]
"""
import glob, json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import check_fjsp as C

INST_RE = re.compile(r'(behnke\d+)')


def verify(inst, path):
    try:
        claim, meta, rows = C.read_schedule(path)
    except (SystemExit, Exception):
        return None
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
    quiet = '--quiet' in sys.argv
    sp = f'{HERE}/best/status.json'
    status = json.load(open(sp))
    insts = {}
    changes = []
    # ---- schedules
    files = []
    for pat in ('runs/**/*.sched', 'runs/**/*.txt', 'lb/runs/**/*.sched', 'lb/proofs/**/*.sched'):
        files += glob.glob(f'{HERE}/{pat}', recursive=True)
    for f in sorted(files):
        m = INST_RE.search(os.path.basename(f)) or INST_RE.search(os.path.relpath(f, HERE))
        if not m or m.group(1) not in status:
            continue
        name = m.group(1)
        if name not in insts:
            insts[name] = C.read_instance(f'{HERE}/instances/{name}.fjs')
        v = verify(insts[name], f)
        if v is None:
            continue
        ms, rows = v
        if ms < status[name]['ub']:
            rel = os.path.relpath(f, HERE)
            with open(f'{HERE}/best/{name}.sched', 'w') as g:
                g.write(f'makespan {ms}\n# source {rel}\n# machine ids 0-based\n# job op machine start end\n')
                for r in sorted(rows):
                    g.write(' '.join(map(str, r)) + '\n')
            status[name].setdefault('history', []).append(dict(kind='ub', value=ms, prev=status[name]['ub'], src=rel))
            changes.append(f'{name}: UB {status[name]["ub"]} -> {ms} ({rel})')
            status[name]['ub'] = ms
            status[name]['ub_src'] = rel
    # ---- candidate lower bounds (single runs; guidance only)
    for name in status:
        status[name].setdefault('lb_cand', status[name]['lb'])
        status[name].setdefault('lb_cand_src', '')
    for f in sorted(glob.glob(f'{HERE}/lb/runs/**/*.json', recursive=True)):
        try:
            r = json.load(open(f))
        except Exception:
            continue
        name = r.get('instance')
        if name not in status or r.get('bound') is None:
            continue
        b = int(r['bound'])
        if b > status[name]['lb_cand']:
            status[name]['lb_cand'] = b
            status[name]['lb_cand_src'] = os.path.relpath(f, HERE)
    # ---- proven lower bounds (cross-validated verdicts only)
    for f in sorted(glob.glob(f'{HERE}/lb/proofs/*/verdict.json')):
        r = json.load(open(f))
        name = r['instance']
        if name not in status or r.get('verdict') != 'PROVED':
            continue
        b = int(r['bound'])
        if b > status[name]['lb']:
            rel = os.path.relpath(f, HERE)
            status[name].setdefault('history', []).append(dict(kind='lb', value=b, prev=status[name]['lb'], src=rel, proof=r.get('proof')))
            changes.append(f'{name}: LB {status[name]["lb"]} -> {b} (PROVED, {rel})')
            status[name]['lb'] = b
            status[name]['lb_src'] = [rel]
        status[name].setdefault('proofs', [])
        if os.path.relpath(f, HERE) not in status[name]['proofs']:
            status[name]['proofs'].append(os.path.relpath(f, HERE))
    for name, v in status.items():
        v['closed'] = (v['ub'] == v['lb'])
        if v['ub'] < v['lb']:
            print(f'!!! {name}: UB {v["ub"]} < LB {v["lb"]} -- a bound or a checker is wrong', file=sys.stderr)
        if v['ub'] < v['lb_cand']:
            print(f'!!! {name}: UB {v["ub"]} < candidate LB {v["lb_cand"]} ({v["lb_cand_src"]}) -- that run is wrong', file=sys.stderr)
    json.dump(status, open(sp, 'w'), indent=1)
    if not quiet:
        for c in changes:
            print('CHANGE', c)
        print(f'{"inst":9s} {"LB":>4s} {"cand":>4s} {"UB":>4s} gap  FJSPLib(LB/UB)  LB source')
        for name, v in status.items():
            if not v['closed']:
                print(f'{name:9s} {v["lb"]:4d} {v["lb_cand"]:4d} {v["ub"]:4d} {v["ub"]-v["lb"]:3d}  {v["fjsplib_lb"]}/{v["fjsplib_ub"]}  {",".join(v["lb_src"])}')
    nc = sum(1 for v in status.values() if v['closed'])
    new_ub = [k for k, v in status.items() if v['ub'] < v['fjsplib_ub']]
    print(f'closed: {nc} of 60; below FJSPLib UB: {len(new_ub)} {new_ub}')


if __name__ == '__main__':
    main()
