# Enumeration check of the relaxation lower bound

Checks that the bound computed by `../lb_flow.py` (imported unchanged, CP-SAT) is never above the
exact optimum obtained by complete enumeration, on 160 small random FJSPT instances
(2-3 jobs, 1-2 operations each, at most 5 operations, 2-3 machines, 2 vehicles, half-unit
asymmetric travel times):

| seed | instances | travel matrix |
|------|-----------|---------------|
| 1 | 60 | metric (shortest-path closure applied) |
| 2 | 60 | triangle inequality may be violated |
| 3 | 40 | triangle inequality may be violated, travel times may be 0 |

`brute.py` enumerates machine assignment, machine sequences, vehicle assignment and per-vehicle
transport order, and evaluates each candidate as an earliest-start schedule (cycles are infeasible).
Instances are regenerated deterministically from the seeds (no data files needed).

## Run

    python run_check.py            # requires ortools (any recent version); add --verbose for per-instance lines

## Expected output

    max_gap=9.5
    instances=160 violations=0 tight=86

`tight` counts instances where bound equals optimum. Runtime: about 3.6 minutes single-threaded
(brute force dominates; bound solved with 1 worker).
