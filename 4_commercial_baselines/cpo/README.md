# CP Optimizer baseline (our runs)

See also `../hexaly/` for the Hexaly 15.0 baseline with the same design.

IBM ILOG CP Optimizer 22.2 (academic licence), run by us on the same workstation as the
agent solvers (AMD Ryzen 9 5950X, 16 cores / 32 threads), with the same budgets. These are
baseline runs, not agent runs. Values are used in Tables 4, 5, 7 and 10 and in Sections 6.1,
6.2 and 6.5 of the paper.

## Models (`models/`)

* `jssp_cpo.py`: standard job-shop model (one interval per operation, end-before-start along
  each job, `no_overlap` per machine, minimise the makespan).
* `fjsp_cpo.py`: standard flexible job-shop model (one optional interval per eligible machine,
  `alternative` per operation, `no_overlap` per machine).
* `common.py`: search loop that logs every improving solution with its wall time. Default
  parameters except `RelativeOptimalityTolerance = OptimalityTolerance = 0`. Set `CPO_EXE` to
  the `cpoptimizer` binary of your installation (Python package `docplex`).
* `run_queue.sh`: the queue we ran (absolute paths of our machine), one block at a time so
  that the core counts are real:

| Block | Runs | Settings | Parallel |
|---|---|---|---|
| `jssp_1core` | 10 instances × 3 seeds | 1,800 s, 1 worker | 15 at a time |
| `jssp_16core` | 10 instances × 3 seeds | 1,800 s, 16 workers | 1 at a time |
| `behnke_120s` | 60 instances | 120 s, 8 workers | 2 at a time |
| `behnke_1200s` | 32 open instances | 1,200 s, 16 workers | 1 at a time |

## Results

* `results.json`: all 152 runs (final makespan, makespan after 600 s, final bound, status).
* `traces/`: the incumbent log of every run, `[wall seconds, makespan]` pairs. The
  10-minute values of the paper are read from these logs.
* `solutions/behnke_*`: every Behnke schedule (format of `../../2_fjsp_behnke/check_fjsp.py`).
* `solutions/jssp_*`: the best schedule per instance and block (gzip; format of
  `../../1_jssp_tai_100_1000_1/check_jssp.py`). The other job-shop schedules (about 600 kB
  each) are not included; their final values are in `results.json` and `traces/`.

`verify_all.py` (section 4) re-checks every included schedule with the independent checkers.

One run (`jssp_16core`, `tai_2`, seed 2) was interrupted by a system restart and run again
from the start; only the complete run is reported.
