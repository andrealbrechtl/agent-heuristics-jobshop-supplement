# Flexible job shop, Behnke & Geiger: mode C (improvement campaign by an agent)

Paper: Sections 4.3 and 6.2, Tables 3 and 10, Table A.2.

In mode C, a coding agent starts from the schedules and the two agent-written solvers of the paper
(the reusable tabu of mode B and the stage-aware tabu/LNS of the 404 session, mode A) and runs its
own campaign to reach optimal schedules. One Claude Fable 5.1 session (Claude Code desktop app)
planned the campaign, wrote all code and ran all queues on the workstation of the paper (AMD Ryzen 9
5950X, 16 cores), 2026-10-19 22:37 to 2026-10-20 12:08, with open-source tools only (g++ 13.3,
OR-Tools CP-SAT 9.15 and 9.14). The author gave the goal, one restriction (open-source tools only),
asked once for a summary and stopped the session. Other processes ran on the workstation at the same
time, so run times document when a value appeared and are not benchmark timings.
(The session's own files call this experiment "Protocol C".)

## Check

```bash
python3 verify_modeC.py      # uses ../2_fjsp_behnke/check_fjsp.py; about 10 s
```

It re-checks all 60 final schedules with the paper's independent checker and recomputes the counts
used in the paper: 34 of 60 instances with a schedule at the FJSPLib lower bound (all 28 closed
instances and 6 that FJSPLib lists as open: behnke32, 34, 40, 55, 56, 60) and 18 values below the
FJSPLib upper bound.

## Contents

| path | content |
|---|---|
| `solutions/behnke<k>.sched` | final schedule of every instance; the `# source` line names the run that produced it (`tabu120`, `tabu1200`, `rr_tabu120`, `cpsat120`, `agent404`, `seed44`: schedules of the paper kept unchanged) |
| `results.json` | per instance: FJSPLib bounds, final makespan, source run (tool, threads, time), closed flag |
| `solver/fjsp_ls3.cpp` | the local search written in the session, merging and extending the two solvers of the paper (build: `g++ -O3 -march=native -pthread -std=c++17 -o fjsp_ls3 fjsp_ls3.cpp`) |
| `solver/lns_cpsat.py` | CP-SAT large-neighbourhood improver (tried, no improvement) |
| `cpsat/lb_cpsat.py`, `cpsat/prove.sh` | the CP-SAT model (standard model plus redundant pool cumulatives) used to search for a schedule with makespan at most T, and its run harness |
| `runs/ub1-3/` | the three `fjsp_ls3` sweeps (progress CSV, output, schedules) |
| `runs/find/`, `lb/proofs/*/`, `lb/runs/full600/` | the CP-SAT runs whose schedules are final (feasibility at a fixed makespan, or minimise with a cap) |
| `runs/bench/`, `runs/lns/` | a 150 s comparison with the paper's tabu; the LNS trials |
| `audit/` | after the session, CP Optimizer 22.2 and Hexaly 15.0 were used only as checkers of the 60 final schedules (all feasible) |
| `scripts/`, `logs/` | queue drivers and their logs |
| `instances/`, `check_fjsp.py` | copies of the paper's instance files (byte-identical) and checker |

The session also ran CP-SAT to test whether schedules at the FJSPLib lower bound exist. Its
"infeasible" verdicts cannot be checked independently, so the paper does not use them and they are
not included here.
