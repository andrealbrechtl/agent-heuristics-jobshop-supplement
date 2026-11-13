#!/usr/bin/env python3
"""Check that the relaxation bound of ../lb_flow.py never exceeds the exact optimum
(complete enumeration) on 160 small random FJSPT instances.

  seed 1: 60 instances, travel times metric (shortest-path closure)
  seed 2: 60 instances, triangle inequality may be violated
  seed 3: 40 instances, triangle inequality may be violated, travel times may be 0
Usage: python run_check.py [--verbose]   (needs ortools)
"""
import json, os, random, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, os.path.dirname(HERE))
from brute import gen, brute, to_json
from lb_flow import lb  # the paper's relaxation, unchanged

GROUPS = [(1, 60, False, False), (2, 60, True, False), (3, 40, True, True)]  # seed, N, tri, zero


def instances():
    for seed, N, tri, zero in GROUPS:
        rng = random.Random(seed)
        for i in range(N):
            nj = rng.choice([2, 3]); nops = [rng.randint(1, 2) for _ in range(nj)]
            if sum(nops) > 5: nops[0] = 1
            nm = rng.choice([2, 3])
            jobs, T = gen(rng, nj, nops, nm, tri=tri, zero=zero)
            yield f"t{seed}_{i}", jobs, T


def main():
    verbose = "--verbose" in sys.argv
    n = viol = tight = 0; maxgap = 0.0
    with tempfile.TemporaryDirectory() as td:
        for name, jobs, T in instances():
            opt = float(brute(jobs, T))
            path = os.path.join(td, name + ".json")
            json.dump(to_json(jobs, T, name), open(path, "w"))
            bound = float(lb(path, workers=1, tlim=30)[0])
            n += 1
            if bound > opt + 1e-9: viol += 1; print("VIOLATION", name, bound, opt)
            if abs(bound - opt) < 1e-9: tight += 1
            maxgap = max(maxgap, opt - bound)
            if verbose: print(name, "opt", opt, "lb", bound, flush=True)
    print(f"max_gap={maxgap}")
    print(f"instances={n} violations={viol} tight={tight}")


if __name__ == "__main__":
    main()
