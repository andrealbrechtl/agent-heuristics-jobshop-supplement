# Job shop `tai_100_1000_k`, mode C — closing instances starting from the paper's solver

Paper: Sections 4.3 and 6.1, Tables 3 and 10, Table A.2. The paper calls this experiment **mode C**;
the session's own files call it "Protocol C". All sessions ran within the limits of a US$100 Claude Max plan.

Protocol C starts from the paper's corridor solver (`1_jssp_tai_100_1000_1/solver/csp3.c`) and its best
schedules, and gives the coding agent one goal: close as many of the ten `tai_100_1000_k` instances as
possible (makespan = lower bound = longest job), on a 4-core machine, by improving the method rather
than by adding compute. It ran as a mostly autonomous Claude Code session (Claude Opus 5.5) in a normal
chat window, 2026-10-19 11:45 → 2026-10-20 12:05.

## Results

LB = longest job (a lower bound for every schedule), so makespan = LB is optimal.

| k | LB | Paper best (16 cores) | Protocol C | Gap to LB | Status | Schedule |
|---|---|---|---|---|---|---|
| 1 | 522,298 | 525,515 | 525,389 | 3,091 | open | `solutions/tai_100_1000_1_525389.txt` (compaction) |
| 2 | 530,375 | 530,375 | 530,375 | 0 | optimal (already in paper) | `solutions/refound/` (re-found in 47 s) |
| 3 | 530,560 | 530,560 | 530,560 | 0 | optimal (already in paper) | `solutions/refound/` |
| 4 | 527,101 | 528,156 | **527,101** | **0** | **optimal (new)** | `solutions/tai_100_1000_4_527101_optimal.txt` |
| 5 | 517,728 | 525,865 | 525,657 | 7,929 | open | compaction |
| 6 | 522,907 | 524,812 | 524,604 | 1,697 | open | compaction |
| 7 | 522,537 | 527,000 | 526,771 | 4,234 | open | compaction |
| 8 | 526,428 | 528,473 | **527,280** | 852 | open, improved | `solutions/tai_100_1000_8_527280.txt` |
| 9 | 528,097 | 529,470 | **528,097** | **0** | **optimal (new)** | `solutions/tai_100_1000_9_528097_optimal.txt` |
| 10 | 521,766 | 526,000 | 525,782 | 4,016 | open | compaction |

"Compaction" = the paper's 16-core schedule left-shifted with its machine orders kept
(`solver/compact.py`); it is a trivial post-processing step, not a search result.

## Verify

```bash
python verify_protocolC.py      # needs numpy; ~2 min
```

Checks every schedule with `check_jssp.py` (copied unchanged from `1_jssp_tai_100_1000_1/`), checks
that the optimal ones equal the longest job, re-derives the k=8 schedule and, when this folder sits next
to `1_jssp_tai_100_1000_1/`, re-derives every compacted schedule from the paper's 16-core schedules bit
for bit and compares the instance files byte for byte. Ends with `ALL CHECKS PASSED`.

## How each new value was produced

All search runs are single-threaded, one process per core. The search is deterministic for a given
seed (the wall-clock limit only stops it), so the runs below reproduce bit for bit from source: the
same iteration count, the same schedule (re-checked, see `REPRODUCTION.md`).

| k | value | binary | command (from this folder) | time to solution |
|---|---|---|---|---|
| 9 | 528,097 | `csp_protC_v1` | `DOMS=domains/dom_k9_T528097_cpsat.txt JSPV=1 ./csp_protC_v1 instances/tai_100_1000_9.txt out.txt 528097 1800 13 8 100000 none 1000 200000` | 1,059.7 s, 104,107,113 iterations |
| 4 | 527,101 | `csp_protC_v2` | `DOMS=domains/dom_k4_T527101_cpsat.txt JSPV=1 ./csp_protC_v2 instances/tai_100_1000_4.txt out.txt 527101 3600 103 8 100000 none 1000 200000` | 1,693.9 s, 160,780,724 iterations |
| 8 | 527,428 → 527,280 | `csp_protC_v3` | `KN=2 KW=1000 DOMS=domains/dom_k8_T527428_propagator.txt JSPV=1 ./csp_protC_v3 instances/tai_100_1000_8.txt out.txt 527428 1800 5 8 100000 none 1000 200000`, then `python solver/compact.py instances/tai_100_1000_8.txt out.txt out_c.txt` | 1,421.5 s, 125,010,642 iterations |
| 2 | 530,375 (re-found) | `csp_protC_v1`, no domains | `JSPV=1 ./csp_protC_v1 instances/tai_100_1000_2.txt out.txt 530375 600 1 8 1000 none 1000 200000` | 46.7 s, 4,517,119 iterations |

Build: `gcc -O3 -march=native -o csp_protC_vN solver/csp_protC_vN.c -lm` (gcc 13.3). Arguments are the
paper's csp3 arguments: instance, output, target T, seconds, seed, noise, stall limit (M iterations),
seed schedule or `none`, step, shake interval. Domain files: first 100 rows = lower bounds of the
start times (one row per job, routing order), next 100 rows = upper bounds.

These runs are the successful ones; the seeds that failed are in `analysis/runs.csv`. k=9: 1 of 4
seeds run in parallel (the other three were stopped when it succeeded). k=4: the 9th search run of
15–60 min with domains at LB (about 5–6 core-hours before success).

## Folder contents

| path | what |
|---|---|
| `instances/` | the ten instances, byte-identical to `1_jssp_tai_100_1000_1/generalization/instances/` |
| `solutions/` | final schedules (paper format: 100 lines of 1,000 start times) |
| `solutions/refound/` | k=2 and k=3 optima re-found in this protocol (the paper's claims stand) |
| `solutions/raw_solver_output/` | the solver's own output files for k=4, 8, 9 |
| `domains/` | the propagated start-time domains used by the successful runs |
| `solver/csp3_paper.c` | the paper's solver, the starting point (unchanged copy) |
| `solver/csp_protC_v1.c` … `v4.c` | the solver versions written in Protocol C (see below) |
| `solver/propagate.c` | domain propagator + shaving (chain + pairwise machine disjunction), 0.4 s per instance |
| `solver/cpsat_domains.py` | CP-SAT presolve domains (OR-Tools 9.15) — the files used for k=9 and k=4 |
| `solver/compact.py` | left-compaction |
| `solver/pathfinder.c`, `core_cpsat.py`, `race.py`, `common.py` | methods tried along the way that did not close an instance |
| `solver/*.diff` | unified diffs csp3 → v1 → v2 → v3 → v4 |
| `logs/` | logs of the three successful runs; `all_run_logs.tar.gz` = every run log of the session |
| `analysis/runs.csv` | one row per logged run (191 runs): instance, target, method family, duration, lowest overlap, feasible |
| `check_jssp.py` | the paper's independent checker, unchanged |
| `verify_protocolC.py` | re-checks everything above |

## Solver versions

| version | change over the previous one | used for |
|---|---|---|
| `csp3_paper.c` | — (the paper's solver) | starting point |
| `csp_protC_v1.c` | per-op start domains (`DOMS`), started directly at the target (`T0` = target); also experimental modes (ILS, whole-job reroute DP, descent, raw infeasible seed, dump) that are off by default | k=9 (and k=2 re-found, no domains) |
| `csp_protC_v2.c` | + periodic dump of the lowest-overlap state (`BESTDUMP`) | k=4 |
| `csp_protC_v3.c` | + configurable kick size (`KN` ops, `KW` window; csp3 = 4, 2000) | k=8 |
| `csp_protC_v4.c` | + targeted kick (`KT=1`: kick the stuck pair and its machine neighbours) and configurable `MAXDEPTH` | final campaign on k=6, 8 (no new value) |

With all options off, v1–v4 run csp3's search unchanged; the only difference is that the target T is
given directly (the paper's runs descend from `ceil(1.02·LB/1000)·1000` in steps of 1,000).

## Notes

* `solutions/refound/tai_100_1000_2_530375_optimal.txt` comes from the 47 s direct-target run (log
  `log_n2c.txt` in `logs/all_run_logs.tar.gz`). `tai_100_1000_3_530560_optimal.txt` comes from a
  PathFinder run (`pathfinder.c`, 300 s) polished by the csp search started from that state; that early
  log was saved only partly. Neither is a new result.
* The CP-SAT domains come from `fill_tightened_domains_in_response` after presolve. `propagate.c`
  (root propagation only) reproduces them almost exactly (k=9: 8 of 200,000 bounds differ). Domains only
  restrict the search; every claimed schedule is certified by the checker alone.
* No lower bound above the longest job was proven for any instance.
