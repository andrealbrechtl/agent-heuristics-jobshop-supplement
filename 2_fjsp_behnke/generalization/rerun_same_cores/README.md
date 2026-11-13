# Re-run of the reusable tabu under the conditions of the CP Optimizer baseline

Paper: Section 6.2 (one sentence). The paper reports the first runs (`../solutions/tabu/`, `../solutions/tabu_1200s/`); this re-run is a supplementary check that equal cores do not change the conclusions.

The first generalisation runs (`../solutions/tabu/`, `../solutions/tabu_1200s/`) gave the tabu fewer
cores per run than CP Optimizer got later (`../../../4_commercial_baselines/cpo/`). This re-run repeats
both blocks with exactly the CP Optimizer conditions, on the same Ryzen 9 5950X workstation:

* Solver `../../solver/mg_tabu_reusable/fjsp_solver.cpp`, unchanged, default seed (12345),
  built `g++ -O3 -march=native -pthread -std=c++17`.
* Block 120 s: all 60 instances, one run of 120 s with 8 threads, two runs at a time (as CP Optimizer).
* Block 1,200 s: the 32 open instances (`open_behnke.txt`), one run of 1,200 s with 16 threads,
  one run at a time (as CP Optimizer).
* Script `run_tabu_rerun.sh` (run 2026-10-16/17, after the 16-core job-shop runs had finished, so no
  other experiment shared the machine).
* `tabu_120s/`, `tabu_1200s/`: schedule (`.sched`), progress csv (time, makespan), checker output
  (`.check`) for every run. All 92 schedules pass `../../check_fjsp.py`.
* `results.json`: value and time to best (last row of the csv) per instance and block.

| | Tabu re-run | CP Optimizer 22.2 |
|---|---|---|
| 120 s × 8 threads, 60 instances: mean deviation from FJSPLib UB | +0.45 % | +1.42 % |
| 120 s: better / equal / worse than the other | 36 / 20 / 4 | 4 / 20 / 36 |
| 120 s, open (32): below / equal to UB | 6 / 6 | 2 / – |
| 1,200 s × 16 threads, open (32): mean deviation from UB | +0.34 % | +0.79 % |
| 1,200 s: better / equal / worse than the other | 19 / 5 / 8 | 8 / 5 / 19 |
| 1,200 s, open (32): below / equal to UB | 9 / 5 | 7 / 4 |

Single runs vary: against the first runs of the tabu, the re-run was better on 20, equal on 27 and
worse on 13 instances at 120 s, and better on 11, equal on 8 and worse on 13 open instances at 1,200 s
(at most four units either way); the first runs averaged +0.60 % (120 s) and +0.24 % (1,200 s, open).
The re-run also found `behnke34` = 221 and `behnke56` = 386, below FJSPLib; the paper does not use these
values (it reports one set of runs, the first runs, for the new-best counts).
