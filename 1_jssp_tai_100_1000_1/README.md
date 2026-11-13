# Job shop `tai_100_1000_1` — certified makespan 525,515

Paper: Sections 2.1, 3, 4.1, Table 1, Figs. 1–4.

| | value | source |
|---|---|---|
| Lower bound (longest job chain) | 522,298 | JSPLib tracked LB (OptalCP, 180 s) |
| Tracked upper bound | 533,080 | JSPLib, OptalCP, 180 s, no certificate |
| Da Col & Teppan (2022), CP Optimizer, 1 core, 6 h limit | 542,768 | their supplementary material, Table A4, row `tai_j100_m1000_1` (4 cores: 543,736) |
| Yuraszeck et al. (2026), Hexaly, 600 s | 542,479 | their table of new best-known bounds, row `tai_j100_m1000_1` |
| **This work**, agent solver, 16-core run | **525,515** | `solutions/tai_100_1000_1_525515.txt` (= `generalization/16cores/solutions/tai_100_1000_1_r2_525515.txt.gz`) |
| This work, agent solver, 12-worker fleet (first version) | 525,764 | `solutions/tai_100_1000_1_525764.txt` |

## Verify

```bash
python check_jssp.py instance/tai_100_1000_1.txt solutions/tai_100_1000_1_525515.txt
# ok=True neg=0 chain_viol=0 machine_overlaps=0 machines_wrong_count=0 makespan=525515 jobs_at_makespan=94
python check_jssp.py instance/tai_100_1000_1.txt solutions/tai_100_1000_1_525764.txt
# ok=True neg=0 chain_viol=0 machine_overlaps=0 machines_wrong_count=0 makespan=525764 jobs_at_makespan=95
```

`check_jssp.py` (numpy) is independent of the solver: it checks non-negative starts, job-chain
precedence, machine non-overlap and that every machine receives exactly 100 operations.
Schedule format: 100 lines of 1,000 start times; position `o` on line `j` is the start of the
`o`-th operation of job `j` in routing order; `#` lines are comments.

## Instance provenance

`instance/tai_100_1000_1.txt` is **byte-identical** to the JSPLib copy:

* repository `ScheduleOpt/benchmarks`, commit `f89e5b007c90a1c1a70fa48a28d657a72da05665`,
  file `jobshop/instances/text/DacolTeppan2022/standard/tai_100_1000_1.zip`
* sha256 `20f56f835c14d6093bed034294441812001239bdf5db31dc773d57121344eac4`
* format: `n m`, then one line per job of `machine duration` pairs, machines 0-based.

The original supplement of Da Col & Teppan (2022, doi 10.1016/j.orp.2022.100249) could not be
fetched by script; JSPLib distributes it under the `DacolTeppan2022` directory.

## How the 525,515 schedule was produced

Same solver `solver/csp3.c`, unchanged, run on 16 cores of the Ryzen 9 5950X workstation: 16 independent
single-core workers × 1,800 s, start target 533,000; worker 6 of run 2 (seed 2006) closed T = 525,515 after
1,769 s. Details, logs and the other runs: `generalization/16cores/`.

## How the 525,764 schedule was produced

* Solver: `solver/csp3.c` (corridor CSP, written by the coding agent in the 527,000 session and
  recovered from the session log; `solver/RECONSTRUCTION.md` explains the recovery).
* Build: `gcc -O2 -o csp3 csp3.c -lm`
* Run (worker `d_201`, called F14 in the paper's figure):
  `./csp3 tai_100_1000_1.txt d_201.txt 531000 1800 201 8 40 none 1000 200000`
  (arguments: instance, output, initial T, seconds, seed, noise, stall Mit, seed file, step, shake).
* Fleet: 12 workers, one core each, 1,800 s each, 2026-08-06 (the logs do not record the
  hardware or CPU pinning; the archive notes name an AMD Ryzen 9 5950X host).
  Six workers started at T = 531,000 (`d_201…d_206`), six at T = 527,000 with seeds 111–116.
* **Time to solution:** worker `d_201` closed T = 525,764 after 1,373 s of search (sum of its
  level times: 10.2 + 8.7 + 24.5 + 91.9 + 70.4 + 457.5 + 128.0 + 581.9 s), i.e. about 23 min,
  then searched T = 524,764 unsuccessfully until 1,800 s.
* All 12 logs: `logs/fleet_protocol2/`. Final values: 525,764 / 525,801 / 525,871 / 525,901 /
  525,932, and seven workers at 526,000; all 12 below 527,000.
* The search is wall-clock limited, so a re-run will not reproduce 525,764 bit for bit; the
  certificate is the schedule file, the logs document how it was found.

## Autonomous sessions (Fig. 2)

`solutions/autonomous_sessions/` holds the final schedule of each of the 15 sessions that wrote
one. Sessions are numbered 1–15 in order of makespan, as in Fig. 2 of the paper (file name =
session number + makespan); `sessions.csv` gives the model, wall-clock and API cost. All 15 pass
`check_jssp.py` at exactly the plotted value.
`solutions/tai_100_1000_1_530404_assisted_campaign.txt` is the final schedule of the assisted
campaign (Claude Opus 5, tabu search with CP-SAT sequence fixing).

Session 16 exited after 18.1 min without a schedule. The 527,000 schedule of the best session is
`session01_527000.txt` (101.6 min, US$10.07).

The containers had public network access; no session used a web search or fetch tool.

## Single-core timing table (Section 4.1)

No log of the run behind the table (535,000 in 1.7 s … 531,000 in 18.5 s) was kept. Re-running
`csp3` from T = 535,000 on one pinned core of the same CPU model with seven seeds reached
533,000 after 7.8–16.2 s (median ≈ 10 s) and 531,000 after ≈ 18–35 s.
