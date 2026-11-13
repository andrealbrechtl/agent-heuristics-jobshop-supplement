#!/usr/bin/env python3
"""Independent feasibility checker for FJSPT schedules (two AGVs, Homayouni & Fontes 2021 model).

Usage:  python check_fjspt.py INSTANCE.json SOLUTION.sol [--vehicles 2]

Model checked (Homayouni & Fontes 2021, as used by Meng et al. 2023 and Berterottiere et al. 2024):
  * every operation is processed exactly once, on an eligible machine, for exactly its processing time;
  * machines process one operation at a time (unlimited buffers, no other machine constraint);
  * all jobs start at the load/unload station L/U (location 0) at time 0;
  * operation k of a job needs a loaded transport from the location of its predecessor
    (L/U for k = 1) to its machine, unless operation k-1 is on the same machine;
    a transport departs no earlier than the predecessor has finished (time 0 for k = 1),
    takes exactly travel[from][to], and the operation starts no earlier than its arrival;
    without a transport, the operation starts no earlier than its predecessor ends;
  * there are V identical unit-capacity vehicles, all at L/U at time 0; a vehicle carries one
    job at a time; between two consecutive transports it drives empty from the previous
    drop-off to the next pickup (travel[drop][pickup]); it may wait;
  * jobs do not return to L/U; makespan = latest operation completion.

Solution format (one record per line, '#' comments allowed):
  instance <name>
  makespan <value>                                (claimed; checked)
  op <job> <op> <machine> <start> <end>           (1-based job/op/machine)
  tr <job> <op> <vehicle> <from> <to> <depart> <arrive>   (transport INTO op; location 0 = L/U)

All arithmetic is exact (fractions), because several layouts use half time units.
This file shares no code with the solver or with the authors' original checker.
"""
import json
import sys
from fractions import Fraction as F
from itertools import permutations


def load_instance(path):
    I = json.load(open(path))
    jobs = [[{int(m): F(str(p)) for m, p in alts} for alts in J] for J in I["jobs"]]
    travel = [[F(str(x)) for x in row] for row in I["travel"]]
    n_loc = len(travel)
    assert all(len(r) == n_loc for r in travel), "travel matrix not square"
    for J in jobs:
        for alts in J:
            assert all(1 <= m < n_loc for m in alts), "machine without a travel row"
    return I.get("name", path), jobs, travel


def load_solution(path):
    ops, trs, claimed, errs = {}, {}, None, []
    for ln, line in enumerate(open(path), 1):
        t = line.split("#", 1)[0].split()
        if not t:
            continue
        if t[0] == "makespan":
            claimed = F(t[1])
        elif t[0] == "op":
            key = (int(t[1]), int(t[2]))
            if key in ops:
                errs.append(f"line {ln}: operation {key} scheduled twice")
            ops[key] = (int(t[3]), F(t[4]), F(t[5]))
        elif t[0] == "tr":
            key = (int(t[1]), int(t[2]))
            if key in trs:
                errs.append(f"line {ln}: transport into {key} given twice")
            trs[key] = (int(t[3]), int(t[4]), int(t[5]), F(t[6]), F(t[7]))
        elif t[0] != "instance":
            errs.append(f"line {ln}: unknown record {t[0]!r}")
    return ops, trs, claimed, errs


def vehicle_ok(tasks, travel):
    """tasks: list of (depart, arrive, frm, to, key). Feasible if some order consistent with
    departure times lets the vehicle serve them one after another starting at L/U at 0."""
    tasks = sorted(tasks)
    errs = []
    loc, free = 0, F(0)
    i = 0
    while i < len(tasks):
        # group tasks with identical departure times (only possible with zero travel times);
        # try every order inside the group
        j = i
        while j < len(tasks) and tasks[j][0] == tasks[i][0]:
            j += 1
        group = tasks[i:j]
        best = None
        for order in permutations(group) if len(group) <= 6 else [group]:
            l, f, ok = loc, free, True
            for d, r, a, b, key in order:
                if d < f + travel[l][a] or r < d:
                    ok = False
                    break
                l, f = b, r
            if ok:
                best = (l, f)
                break
        if best is None:
            d, r, a, b, key = group[0]
            errs.append(f"cannot reach pickup of transport {key}: free at t={free} at location {loc}, "
                        f"needs {travel[loc][a]} to reach {a}, departs at {d}")
            loc, free = group[-1][3], group[-1][1]
        else:
            loc, free = best
        i = j
    return errs


def check(inst_path, sol_path, n_vehicles=2):
    name, jobs, travel = load_instance(inst_path)
    ops, trs, claimed, errs = load_solution(sol_path)

    expected_ops = {(j, k) for j, J in enumerate(jobs, 1) for k in range(1, len(J) + 1)}
    for key in sorted(set(ops) - expected_ops):
        errs.append(f"operation {key} does not exist in the instance")
    for key in sorted(expected_ops - set(ops)):
        errs.append(f"operation {key} missing")

    # operations: eligibility, duration
    for (j, k), (m, s, e) in ops.items():
        if (j, k) not in expected_ops:
            continue
        p = jobs[j - 1][k - 1].get(m)
        if p is None:
            errs.append(f"operation {(j, k)} on ineligible machine {m}")
        elif e - s != p:
            errs.append(f"operation {(j, k)} lasts {e - s}, processing time on machine {m} is {p}")
        if s < 0:
            errs.append(f"operation {(j, k)} starts at negative time {s}")

    # job routing and transports
    needed = set()
    for j, J in enumerate(jobs, 1):
        for k in range(1, len(J) + 1):
            if (j, k) not in ops or (k > 1 and (j, k - 1) not in ops):
                continue
            m, s, _ = ops[(j, k)]
            frm = 0 if k == 1 else ops[(j, k - 1)][0]
            ready = F(0) if k == 1 else ops[(j, k - 1)][2]
            if frm != m:
                needed.add((j, k))
                if (j, k) not in trs:
                    errs.append(f"operation {(j, k)} on machine {m} has no transport from {frm}")
                    continue
                v, a, b, d, r = trs[(j, k)]
                if (a, b) != (frm, m):
                    errs.append(f"transport into {(j, k)} goes {a}->{b}, job must go {frm}->{m}")
                if not 1 <= v <= n_vehicles:
                    errs.append(f"transport into {(j, k)} uses vehicle {v} (only {n_vehicles})")
                if d < ready:
                    errs.append(f"transport into {(j, k)} departs at {d}, job ready at {ready}")
                if r - d != travel[a][b]:
                    errs.append(f"transport into {(j, k)} takes {r - d}, travel {a}->{b} is {travel[a][b]}")
                if s < r:
                    errs.append(f"operation {(j, k)} starts at {s}, before its transport arrives at {r}")
            else:
                if s < ready:
                    errs.append(f"operation {(j, k)} starts at {s}, before its predecessor ends at {ready}")
    for key in sorted(set(trs) - needed):
        errs.append(f"transport into {key} is not required by the routing (same machine or unknown op)")

    # machine capacity
    by_machine = {}
    for key, (m, s, e) in ops.items():
        by_machine.setdefault(m, []).append((s, e, key))
    for m, L in by_machine.items():
        L.sort()
        for (s1, e1, k1), (s2, e2, k2) in zip(L, L[1:]):
            if s2 < e1:
                errs.append(f"machine {m}: {k1} [{s1},{e1}) overlaps {k2} [{s2},{e2})")

    # vehicles
    by_vehicle = {}
    for key, (v, a, b, d, r) in trs.items():
        if key in needed:
            by_vehicle.setdefault(v, []).append((d, r, a, b, key))
    for v, L in sorted(by_vehicle.items()):
        errs += [f"vehicle {v}: {x}" for x in vehicle_ok(L, travel)]

    mk = max(e for (_, _, e) in ops.values()) if ops else None
    if claimed is not None and mk != claimed:
        errs.append(f"claimed makespan {claimed} but schedule ends at {mk}")

    # vehicle statistics (used in the paper's anatomy section)
    stats = {}
    for v, L in by_vehicle.items():
        L.sort()
        loaded = sum(r - d for d, r, *_ in L)
        empty, loc = F(0), 0
        for d, r, a, b, _ in L:
            empty += travel[loc][a]
            loc = b
        last = L[-1][1] if L else F(0)
        stats[v] = dict(transports=len(L), loaded=loaded, empty=empty, idle=last - loaded - empty, last_drop=last)
    return name, (not errs), mk, len(needed), errs, stats


def fmt(x):
    return str(float(x)).rstrip("0").rstrip(".") if x is not None else "-"


if __name__ == "__main__":
    args = sys.argv[1:]
    nv = 2
    if "--vehicles" in args:
        i = args.index("--vehicles"); nv = int(args[i + 1]); del args[i:i + 2]
    name, ok, mk, ntr, errs, stats = check(args[0], args[1], nv)
    print(f"{name}: {'FEASIBLE' if ok else 'INFEASIBLE'}  makespan={fmt(mk)}  transports={ntr}")
    import re
    for e in errs[:25]:
        print("   ", re.sub(r"(\d+)/(\d+)", lambda m: fmt(F(int(m.group(1)), int(m.group(2)))), e))
    if len(errs) > 25:
        print(f"    ... {len(errs) - 25} more")
    sys.exit(0 if ok else 1)
