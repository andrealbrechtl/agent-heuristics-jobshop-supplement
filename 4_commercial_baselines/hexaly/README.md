# Hexaly baseline (our runs)

Hexaly Optimizer 15.0 (build 20261008, academic licence), run by us on the same workstation as the
agent solvers and the CP Optimizer baseline (AMD Ryzen 9 5950X, 16 cores / 32 threads), with the
same runs, budgets and thread counts as `../cpo/`. These are baseline runs, not agent runs.

## Models (`models/`)

* `jssp_hx.py`: Hexaly's own standard job-shop model (`examples/jobshop/jobshop.py` of the
  distribution: one interval decision per operation, one list variable per machine whose
  consecutive tasks must not overlap, precedence along each job, minimise the makespan). Only the
  instance reader and the output format were adapted to our files.
* `fjsp_hx.py`: Hexaly's own standard flexible job-shop model
  (`examples/flexiblejobshop/flexible_jobshop.py`: one list variable per machine with a partition
  constraint, machine-dependent durations, interval decisions). Same adaptations.
* `common.py`: solve call and a once-per-second callback that logs every improvement of the
  incumbent with its wall time. Default parameters except time limit, thread count and seed.
  Requires `LD_LIBRARY_PATH=<hexaly>/bin` and the Python API in `<hexaly>/bin/python`.
* `run_queue.sh`: the queue we ran (absolute paths of our machine).

| Block | Runs | Settings | Parallel |
|---|---|---|---|
| `behnke_120s` | 60 instances | 120 s, 8 threads, seed 1 | 1 at a time |
| `behnke_1200s` | 32 open instances | 1,200 s, 16 threads, seed 1 | 1 at a time |
| `jssp_16core` | 10 instances × 3 seeds | 1,800 s, 16 threads | 1 at a time |
| `jssp_1core` | 10 instances × 3 seeds | 1,800 s, 1 thread | 1 at a time |

The academic licence allows one optimizer process at a time, so every Hexaly run had the machine
to itself. CP Optimizer ran its 1-core job-shop runs 15 at a time and its 120 s Behnke runs two at a
time; where the two differ, the conditions favour Hexaly. Building the 100,000-operation job-shop
model takes about 3.5 minutes in Python before the solver starts; as for CP Optimizer, this time is
not part of the time limit.

## Results

* `results.json`: all 152 runs (final makespan, makespan after 600 s, final bound, status, version).
* `traces/`: the incumbent log of every run, `[wall seconds, makespan]` pairs, sampled once per
  second. The 10-minute values of the paper are read from these logs.
* `solutions/behnke_*`: every Behnke schedule (format of `../../2_fjsp_behnke/check_fjsp.py`).
* `solutions/jssp_*`: the best schedule per instance and block (gzip; format of
  `../../1_jssp_tai_100_1000_1/check_jssp.py`). The other job-shop schedules are not included;
  their final values are in `results.json` and `traces/`.

`verify_all.py` (section 4) re-checks every included schedule with the independent checkers.
All 152 schedules passed the checkers when the runs finished.
