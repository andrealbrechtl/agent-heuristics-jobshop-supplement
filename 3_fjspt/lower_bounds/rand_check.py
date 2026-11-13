#!/usr/bin/env python3
"""Random-instance soundness test: every bound must be <= the exact optimum.

Group T (tiny, 160 instances): generator and complete-enumeration optimum of the earlier (removed)
  bound's check, imported unchanged from old_tag/.../enumeration_check/brute.py
  (seed 1 metric, seed 2 triangle inequality may fail, seed 3 + zero travel times).
Group M (medium, random): 2-6 jobs, 1-4 ops, 2-4 machines, integer data, travel-dominated or
  processing-dominated, triangle inequality may fail; optimum from ../generalization/exact_cpsat.py
  (independent exact CP-SAT model, validated against the 16 HF2021 MILP optima); only instances
  proven OPTIMAL are counted.
Group S (sub-instances of the real benchmark): first 3-6 jobs of an MKT / CB instance, each job cut
  to its first 1-3 ops, with the real layout (15-/16-machine layouts violate the triangle inequality).

Bounds tested: (1) sound/direct, (1) force |V| routes, (1) shortest-path empties, (2), (2-strong), (3).
Usage: rand_check.py [--groups TMS] [--nmed 120] [--nsub 80] [--procs 4]
"""
import argparse
import glob
import json
import os
import random
import subprocess
import sys
import tempfile
from fractions import Fraction as F
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "old_tag/supplement/3_fjspt/lower_bound/enumeration_check"))
from common import Inst, SOLVERS_PY, fmt  # noqa: E402
import bound1_ip  # noqa: E402
import bound2_closed  # noqa: E402
import bound3_relax  # noqa: E402

EXACT = os.path.join(ROOT, "generalization/exact_cpsat.py")


def all_bounds(path):
    I = Inst(path)
    out = {}
    for tag, kw in [("b1", {}), ("b1_force", dict(force_all=True)), ("b1_sp", dict(sp_empty=True))]:
        try:
            L = bound1_ip.build(I, **kw)
            r = bound1_ip.solve_cpsat(L, 60, 1)
            if r["optimal"]:
                out[tag] = F(r["obj"] * I.gran, I.scale)
            elif r["status"] == "INFEASIBLE":
                out[tag] = None
        except Exception as e:  # noqa
            out[tag] = f"ERR {e}"
    b2 = bound2_closed.bound2(I)
    out["b2"], out["b2_strong"] = b2["lb"], b2["lb_strong"]
    r = bound3_relax.solve_cpsat(I, 60, 1)
    out["b3"] = I.ceil_gran(r["bound"])
    return out


def tiny_instances():
    from brute import gen, to_json
    for seed, N, tri, zero in [(1, 60, False, False), (2, 60, True, False), (3, 40, True, True)]:
        rng = random.Random(seed)
        for i in range(N):
            nj = rng.choice([2, 3]); nops = [rng.randint(1, 2) for _ in range(nj)]
            if sum(nops) > 5:
                nops[0] = 1
            nm = rng.choice([2, 3])
            jobs, T = gen(rng, nj, nops, nm, tri=tri, zero=zero)
            yield f"T{seed}_{i}", jobs, T, to_json(jobs, T, f"T{seed}_{i}")


def med_instance(i):
    rng = random.Random(1000 + i)
    nj = rng.randint(2, 6); nm = rng.randint(2, 4)
    style = rng.choice(["travel", "proc", "mixed"])
    pr = {"travel": (1, 3), "proc": (5, 20), "mixed": (1, 10)}[style]
    jobs = []
    for j in range(nj):
        J = []
        for o in range(rng.randint(1, 4 if nj <= 4 else 3)):
            ms = rng.sample(range(1, nm + 1), rng.randint(1, min(3, nm)))
            J.append([[m, rng.randint(*pr)] for m in ms])
        jobs.append(J)
    n = nm + 1
    T = [[0 if a == b else rng.randint(0 if i % 5 == 0 else 1, 12) for b in range(n)] for a in range(n)]
    if i % 2 == 0:  # metric half
        for k in range(n):
            for a in range(n):
                for b in range(n):
                    if a != b:
                        T[a][b] = min(T[a][b], T[a][k] + T[k][b])
    return dict(name=f"M{i}_{style}", n_jobs=nj, n_machines=nm, jobs=jobs, travel=T)


def sub_instance(i, srcs):
    rng = random.Random(5000 + i)
    src = json.load(open(rng.choice(srcs)))
    if any(x != int(x) for r in src["travel"] for x in r):
        pass
    nj = rng.randint(3, 6)
    js = rng.sample(range(len(src["jobs"])), nj)
    jobs = [src["jobs"][j][:rng.randint(1, 3)] for j in js]
    return dict(name=f"S{i}_{src['name']}", n_jobs=nj, n_machines=src["n_machines"], jobs=jobs, travel=src["travel"])


def exact_opt(path, tl=120):
    out = subprocess.run([SOLVERS_PY, EXACT, path, str(tl), "2"], capture_output=True, text=True).stdout.split()
    # NAME STATUS obj V bound B time
    if len(out) >= 6 and out[1] == "OPTIMAL":
        return F(str(out[3]))
    return None


def work(args):
    kind, name, data, opt = args
    td = tempfile.mkdtemp(prefix="rc_")
    path = os.path.join(td, name + ".json")
    json.dump(data, open(path, "w"))
    if opt is None:
        half = any(x != int(x) for r in data["travel"] for x in r) or any(p != int(p) for J in data["jobs"] for a in J for _, p in a)
        if half:  # exact_cpsat.py needs integers: solve the x2 instance, halve the optimum
            d2 = dict(data, travel=[[2 * x for x in r] for r in data["travel"]],
                      jobs=[[[[m, 2 * p] for m, p in a] for a in J] for J in data["jobs"]])
            p2 = os.path.join(td, name + "_x2.json")
            json.dump(d2, open(p2, "w"))
            o2 = exact_opt(p2)
            opt = None if o2 is None else o2 / 2
        else:
            opt = exact_opt(path)
        if opt is None:
            return name, None, {}
    b = all_bounds(path)
    return name, opt, b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--groups", default="TMS")
    ap.add_argument("--nmed", type=int, default=120)
    ap.add_argument("--nsub", type=int, default=80)
    ap.add_argument("--procs", type=int, default=4)
    a = ap.parse_args()
    tasks = []
    if "T" in a.groups:
        from brute import brute
        for name, jobs, T, js in tiny_instances():
            tasks.append(("T", name, js, F(brute(jobs, T))))
    if "M" in a.groups:
        for i in range(a.nmed):
            d = med_instance(i)
            tasks.append(("M", d["name"], d, None))
    if "S" in a.groups:
        srcs = sorted(glob.glob(os.path.join(ROOT, "instances/*.json")))
        for i in range(a.nsub):
            d = sub_instance(i, srcs)
            tasks.append(("S", d["name"], d, None))
    stats = {}
    viol = []
    tight = {}
    with Pool(a.procs) as pool, open(os.path.join(HERE, "results/rand_check.csv"), "w") as fh:
        fh.write("instance,opt,b1,b1_force,b1_sp,b2,b2_strong,b3\n")
        for name, opt, b in pool.imap_unordered(work, tasks):
            g = name[0]
            st = stats.setdefault(g, dict(n=0, skipped=0))
            if opt is None:
                st["skipped"] += 1
                continue
            st["n"] += 1
            cols = ["b1", "b1_force", "b1_sp", "b2", "b2_strong", "b3"]
            fh.write(",".join([name, fmt(opt)] + [fmt(b[c]) if isinstance(b.get(c), F) else str(b.get(c)) for c in cols]) + "\n")
            for c in cols:
                v = b.get(c)
                if isinstance(v, str):
                    viol.append((name, c, v))
                elif v is not None and v > opt:
                    viol.append((name, c, f"bound {fmt(v)} > opt {fmt(opt)}"))
                elif v is not None and v == opt:
                    tight[c] = tight.get(c, 0) + 1
    print("groups:", stats)
    print("tight counts (bound == opt):", tight)
    print("violations:", len(viol))
    for v in viol:
        print("  VIOLATION", v)


if __name__ == "__main__":
    main()
