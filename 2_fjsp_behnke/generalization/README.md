# Generalisation: the agent's reusable tabu on all 60 Behnke–Geiger instances, with a CP-SAT baseline

Paper: Section 6.2 (Table "Open Behnke instances…", Fig. "reusable tabu … all 60").

* Instances: `instances/behnke1.fjs … behnke60.fjs`, the original files of Behnke & Geiger (openHSU,
  doi:10.24405/436) with CRLF removed. All 60 are byte-identical to FJSPLib
  (`ScheduleOpt/benchmarks` commit `f89e5b007c90a1c1a70fa48a28d657a72da05665`,
  `flexible-jobshop/instances/fjsp/BehnkeGeiger2012/behnke<k>.txt`), checked 2026-10-11.
* Bounds: `flexible-jobshop/solutions/bks.json` at the same commit (32 open, 28 closed). Copied
  into `results.json` (`lb`, `ub`, `status`).
* Agent tabu: `../solver/mg_tabu_reusable/fjsp_solver.cpp`, unchanged.
  Build `g++ -O3 -march=native -pthread -std=c++17 -o bin_fjsp ../solver/mg_tabu_reusable/fjsp_solver.cpp`.
* CP-SAT baseline: `../baseline_cpsat/cpsat_fjsp.py` (standard model: one optional interval per
  eligible machine, NoOverlap per machine, job precedence, minimise makespan), OR-Tools 9.15.
* Budget: one run per instance and method, 120 s, 8 threads, default seed; three runs in parallel
  on the AMD Ryzen 9 5950X (24 of 32 threads). Commands: `run_commands.txt`.
* Outputs: `solutions/tabu/`, `solutions/cpsat/`; tabu progress (time, makespan) in `logs/tabu/*.csv`;
  CP-SAT incumbent trace (time, makespan, bound) and final status in `logs/cpsat/*.log`.
* Block 2 (overnight): the tabu again on the 32 open instances, one run of 1,200 s with 16 threads,
  two runs in parallel, so about 8 of the 16 cores per run (the original `behnke16` run of 406 had
  32 threads on all 16 cores). Commands `run_commands_1200s.txt`,
  schedules `solutions/tabu_1200s/`, progress `logs/tabu_1200s/`; `results.json` fields `tabu_long`,
  `tabu_long_ttb`.
* Re-run: both tabu blocks were re-run with exactly the cores of the CP Optimizer baseline
  (120 s × 8 threads two at a time; 1,200 s × 16 threads one at a time). Folder `rerun_same_cores/`
  (own README). The paper reports the first runs below; the re-run is a supplementary check.
* Every one of the 152 schedules of the first runs passes `../check_fjsp.py` at the value in `results.json`.

## Summary (first runs, as reported in the paper)

| | Agent tabu | CP-SAT |
|---|---|---|
| Open (32): below FJSPLib UB | 5 (behnke16 409, behnke40 418, behnke53 214, behnke55 222, behnke59 394) | 1 (behnke53 212, proven optimal) |
| Open (32): equal to UB | 2 (behnke48, behnke60) | 1 (behnke48 125, proven optimal) |
| Closed (28): optimum reached | 18 | 19 |
| Mean deviation from FJSPLib UB (60) | +0.60 % | +6.42 % |
| Better / equal / worse than the other method | 38 / 16 / 6 | 6 / 16 / 38 |
| Open (32), tabu 1,200 s × 16 threads: below / equal to UB | 10 / 4 | – |
| Open (32): new best-known value in either tabu run | 11 (13, 16, 32, 35, 40, 53, 55, 56, 57, 59, 60) | – |

The best `behnke16` value of this paper (404) comes from an autonomous session, not from this
120 s run.
