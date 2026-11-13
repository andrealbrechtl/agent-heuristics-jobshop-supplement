# Generalisation: the agent's corridor solver on all ten `tai_100_1000_k`

Paper: Section 6.1, Table 6, Fig. 6.

The solver `../solver/csp3.c` (written by the agent in the 527,000 session) was run **without any
change** on the ten 100×1000 instances of Da Col & Teppan (2022). All ten are open in JSPLib
(commit `f89e5b007c90a1c1a70fa48a28d657a72da05665`, unchanged on 2026-10-11).

* Build: `gcc -O2 -o csp3 ../solver/csp3.c -lm`
* Runs: three seeds per instance, one core and 1,800 s per run, 16 runs in parallel on the
  Ryzen 9 5950X. Start deadline `T0 = ceil(1.02 · LB / 1000) · 1000` (fixed rule, not tuned).
  Exact commands: `run_commands.txt` (run from this folder after building `csp3`).
* Instances: `instances/tai_100_1000_k.txt`, from
  `jobshop/instances/text/DacolTeppan2022/standard/tai_100_1000_k.zip` at the commit above.
  `tai_100_1000_1.txt` is byte-identical to `../instance/tai_100_1000_1.txt`.
* Block 2 (overnight): for the eight instances not closed in block 1, sixteen more seeds
  (101–116) per instance, 1,800 s each, 32 processes on the 16 cores (two per core). Commands
  `run_commands_16runs.txt`, logs `logs_16runs/`. Block 2 improved `_4`, `_5`, `_6`, `_8`, `_9`.
* 16-core block: three runs per instance of 16 single-core workers × 1,800 s, one run
  at a time, the budget of the CP Optimizer 16-core block. Folder `16cores/` (own README). It
  improved `_1`, `_4`, `_5`, `_6`, `_8`, `_9` and `_10` further.
* Logs: `logs/k<k>_s<seed>.log.gz`. `parse_csp3_logs.py` turns a log into a best-so-far trace
  (time at which each deadline level was closed).
* Best schedule per instance: `solutions/`. Every one of the 30 final schedules passes
  `../check_jssp.py`; the two optimal ones were also checked with the separate
  `check_certificate.py` of the earlier campaign.
* `results.json`: per instance, the lower bound, the JSPLib best-known upper bound, the JSPLib
  10-minute results of CP-SAT 9.15, CP Optimizer 22.1 and OptalCP 2026.4
  (`jobshop/solutions/results_*.json` at the commit above), and our values after 600 s and 1,800 s.

| k | LB (longest job) | Best known before | OptalCP 10 min | Ours 10 min | Ours 30 min (block 1) | 16 cores (best of 3) | **Best of all runs** | Status |
|---|---|---|---|---|---|---|---|---|
| 1 | 522,298 | 533,080 | 540,099 | 526,999 | 525,999 | 525,515 | **525,515** | improved |
| 2 | 530,375 | 538,067 | 544,180 | 532,000 | 530,375 | 530,375 | **530,375** | **optimal** (= LB) |
| 3 | 530,560 | 538,757 | 545,590 | 530,560 | 530,560 | 530,560 | **530,560** | **optimal** (= LB) |
| 4 | 527,101 | 534,746 | 541,326 | 529,999 | 529,075 | 528,156 | **528,156** | improved |
| 5 | 517,728 | 529,580 | 537,153 | 527,000 | 527,000 | 525,865 | **525,865** | improved |
| 6 | 522,907 | 534,969 | 542,004 | 526,000 | 525,995 | 524,812 | **524,812** | improved |
| 7 | 522,537 | 534,974 | 541,161 | 527,000 | 527,000 | 527,000 | **527,000** | improved |
| 8 | 526,428 | 535,757 | 543,074 | 530,000 | 529,282 | 528,473 | **528,473** | improved |
| 9 | 528,097 | 536,993 | 544,065 | 532,000 | 531,207 | 529,470 | **529,470** | improved |
| 10 | 521,766 | 529,918 | 538,445 | 527,000 | 526,855 | 526,000 | **526,000** | improved |

"Ours" (block 1) = best of three seeds; "best of all runs" includes block 2 and the 16-core block. Solutions in `solutions/` are the best of blocks 1 and 2; the 16-core schedules are in `16cores/solutions/`. A makespan equal to the longest job is a lower bound for every
schedule, so the schedules for `_2` and `_3` are optimal.
