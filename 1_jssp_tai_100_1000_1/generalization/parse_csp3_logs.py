#!/usr/bin/env python3
"""Best-so-far makespan over time from csp3 logs.

csp3 prints, for every closed level, a line "T=<value>: conf a->0 ... (<seconds>s)" whose
time is the duration of that level, followed by "*** FEASIBLE <value>". The time at which a
level is closed is the sum of the durations of all levels so far.
Usage: python parse_csp3_logs.py LOG... ; prints JSON {log: [[seconds, makespan], ...]}.
"""
import gzip
import json
import re
import sys


def trace(path):
    t, out, pending = 0.0, [], None
    for line in (gzip.open(path, 'rt') if path.endswith('.gz') else open(path)):
        m = re.match(r"T=(\d+): conf \d+->(\d+) .*\(([\d.]+)s\)", line)
        if m:
            t += float(m.group(3))
            pending = (int(m.group(1)), int(m.group(2)))
            continue
        m = re.search(r"\*\*\* FEASIBLE (\d+)", line)
        if m and pending:
            out.append([round(t, 1), int(m.group(1))])
            pending = None
    return out


def value_at(tr, sec):
    v = [mk for ts, mk in tr if ts <= sec]
    return min(v) if v else None


if __name__ == "__main__":
    print(json.dumps({p: trace(p) for p in sys.argv[1:]}, indent=1))
