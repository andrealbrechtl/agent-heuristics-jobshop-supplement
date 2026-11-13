# 16-core runs of the corridor solver

Paper: Section 6.1, Tables 4, 5 and 10.

The same solver `../../solver/csp3.c`, unchanged (the binary used is byte-identical to
`gcc -O2 -o csp3 ../../solver/csp3.c -lm`), on the same ten instances (`../instances/`), with the
budget of the CP Optimizer 16-core block (`../../../4_commercial_baselines/cpo/`): 30 minutes on
16 cores of the Ryzen 9 5950X workstation.

* One **16-core run** = 16 independent single-core workers with seeds `1000·run + 1 … 1000·run + 16`,
  1,800 s each, started together; the run's value is the best of the 16 workers. Three runs per
  instance, one run at a time, so every worker had its own physical core. Start targets as in
  block 1, `T0 = ceil(1.02 · LB / 1000) · 1000`. Script: `run_corridor16.sh` (run 2026-10-16).
* `results.json`: for each instance and run, the value, worker, seed and time to the best value
  of every worker. Time to best = the time at which the level that produced the value was closed
  (`../parse_csp3_logs.py`), an upper bound on the time of discovery.
* `solutions/tai_100_1000_<k>_r<run>_<makespan>.txt.gz`: the best schedule of each run (30 files).
  All 480 worker schedules passed `../../check_jssp.py`; `../../../verify_all.py` re-checks the 30 kept here.
* `logs/k<k>_r<run>_w<worker>.log.gz`: all 480 worker logs. Once a worker reaches the lower bound,
  csp3 prints the same line until the time limit; such runs of identical consecutive lines are kept once.

| k | LB | Best known before | Runs 1 / 2 / 3 | **Best** | Gap (%) | Status |
|---|---|---|---|---|---|---|
| 1 | 522,298 | 533,080 | 525,581 / 525,515 / 525,696 | **525,515** | 0.62 |  |
| 2 | 530,375 | 538,067 | 530,375 / 530,375 / 530,375 | **530,375** | 0.00 | **optimal** |
| 3 | 530,560 | 538,757 | 530,560 / 530,560 / 530,560 | **530,560** | 0.00 | **optimal** |
| 4 | 527,101 | 534,746 | 528,702 / 528,758 / 528,156 | **528,156** | 0.20 |  |
| 5 | 517,728 | 529,580 | 525,865 / 526,000 / 526,117 | **525,865** | 1.57 |  |
| 6 | 522,907 | 534,969 | 524,812 / 525,000 / 525,000 | **524,812** | 0.36 |  |
| 7 | 522,537 | 534,974 | 527,000 / 527,000 / 527,000 | **527,000** | 0.85 |  |
| 8 | 526,428 | 535,757 | 528,473 / 528,495 / 528,505 | **528,473** | 0.39 |  |
| 9 | 528,097 | 536,993 | 529,927 / 529,470 / 530,184 | **529,470** | 0.26 |  |
| 10 | 521,766 | 529,918 | 526,216 / 526,489 / 526,000 | **526,000** | 0.81 |  |
