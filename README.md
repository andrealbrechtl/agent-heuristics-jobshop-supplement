# Supplementary material: agent-written heuristics versus commercial solvers on job-shop benchmarks

Instances, certified schedules, independent checkers, solver sources, prompts and run logs for
every result reported in

> **Agent-written heuristics versus commercial solvers on unsolved job-shop benchmark instances**<br>
> Andre Albrecht, Enzo Morosini Frazzon, Leandro C. Coelho, Bruno Vieira<br>
> ProLogIS (Production and Logistics Intelligent Systems), Federal University of Santa Catarina (UFSC), Brazil; CIRRELT and Université Laval, Canada; Federal Institute of Santa Catarina (IFSC), Brazil<br>
> Submitted to *International Transactions in Operational Research*, special issue *Agentic Operations Research: The 2026 Edition*

Coding agents (Claude Code) worked on large, open benchmark instances from three scheduling
families, wrote their own heuristics and models with open-source tools only, and found new
best-known schedules. Every schedule here is verified by an independent checker, and one script
re-checks every number stated in the paper.

## Main results

| Problem | Instance(s) | Best known before | This work |
|---|---|---|---|
| Job shop, 100 jobs × 1,000 machines | `tai_100_1000_1` | 533,080 (OptalCP) | **525,515** (lower bound 522,298) |
| | all ten `tai_100_1000_k` | all open | **10/10 improved**; `_2` (530,375) and `_3` (530,560) **solved to optimality**; with 16 cores 0.0–1.6 % above the lower bound vs 2.6–5.3 % for CP Optimizer 22.2 |
| Flexible job shop (Behnke & Geiger) | `behnke16` | 412 | **404** |
| | all 60 instances | 32 open | **11 new best-known values**; 0.60 % above best known vs 1.42 % for CP Optimizer 22.2 and 6.42 % for CP-SAT at equal budget; an agent campaign (mode C) raised this to 18 and **closed 6** |
| Flexible job shop with 2 transport robots | MKT01–MKT10 | LAHC, IGA, TS, DRL, DCGA, IVNDGA, CP Optimizer approach of Meng et al. (2025) | **7/10 improved**, e.g. MKT10 525 → **499**; new lower bounds show 4 published values infeasible |
| | 21 Chambers–Barnes | Meng et al. (2025) | **none improved**; the CP Optimizer approach is better on 20, tie on 1 (negative result) |

## Verify every result

Requires Python 3.9 or later.

```bash
pip install -r requirements.txt     # numpy (checkers)
python verify_all.py                # all schedules, replays and protocol statistics (about 10 s)
```

The script checks each certificate and compares the result with the value stated in the paper.
It prints one line per check, ends with `ALL CHECKS PASSED`, and exits non-zero on any failure.

File integrity:

```bash
sha256sum -c CHECKSUMS.sha256
```

A single schedule can be checked directly, for example the job-shop record:

```bash
cd 1_jssp_tai_100_1000_1
python check_jssp.py instance/tai_100_1000_1.txt solutions/tai_100_1000_1_525515.txt
```

## Contents

| Folder | Problem | Main result | Read |
|---|---|---|---|
| `0_prompts/` | all | task texts given to the agents, verbatim | `README.md` |
| `1_jssp_tai_100_1000_1/` | job shop, 100 × 1000 (Da Col & Teppan) | 525,515 (LB 522,298); family: all 10 below best known, `_2` and `_3` optimal; 16-core runs in `generalization/16cores/` | `README.md`, `generalization/README.md` |
| `2_fjsp_behnke/` | flexible job shop, Behnke & Geiger | 404 on `behnke16` (UB 412); 226 on `behnke31`; all 60: 11 open instances below best known, CP-SAT baseline; re-run with the cores of CP Optimizer in `generalization/rerun_same_cores/` | `README.md`, `generalization/README.md` |
| `3_fjspt/` | flexible job shop with 2 AGVs, Homayouni & Fontes | MKT01–10 and 21 Chambers–Barnes instances; MKT10 499; small/medium sets, MFJST09 1,090; CP-SAT baseline; validated lower bounds; published schedules replayed | `README.md`, `RUNS.md`, `generalization/README.md`, `baseline_cpsat/README.md` |
| `4_commercial_baselines/` | job shop and Behnke | CP Optimizer 22.2 (`cpo/`) and Hexaly 15.0 (`hexaly/`) on the same workstation with the same budgets as the agent solvers: models, run queue, results, incumbent traces, schedules | `cpo/README.md`, `hexaly/README.md` |
| `5_jssp_mode_C/` | job shop, mode C | improvement campaign by an agent from the corridor solver: new optima `_4` (527,101) and `_9` (528,097), `_8` 527,280, compaction of the other values | `README.md`, `REPRODUCTION.md` |
| `6_fjsp_behnke_mode_C/` | flexible job shop, mode C | improvement campaign by an agent from the two Behnke solvers: 34 of 60 at the FJSPLib lower bound, 6 newly closed, 18 below the FJSPLib upper bound | `README.md` |

Each problem folder contains:

| Folder / file | Contents |
|---|---|
| `instance/` or `instances/` | The exact instance files, with a `SOURCE.md` giving the upstream URL, pinned commit, sha256 and how identity with the literature was checked |
| `solutions/` | Certified schedules with explicit start times, including the result of every autonomous agent session |
| `check_*.py` | Independent checker (standard library, or numpy for the 100,000-operation job shop); it shares no code with the solvers |
| `solver/` | Source code written by the agents, with build and run commands |
| `logs/` | Solver logs and progress files that show when each value was found |
| `generalization/` | The agent-written solver run **unchanged** on similar instances: instances, schedules, logs, exact commands and results |
| `baseline_cpsat/` | Open-source CP-SAT baselines with the same budget (Behnke and FJSPT folders) |
| `README.md` | What is in the folder, how each schedule was produced and how long it took |

The FJSPT folder also contains `literature_replay/` (published schedules replayed with our
checker), `instance_sources/` (scripts that re-download and compare all instance sources),
`protocol_runs/` (the 100 runs of the fixed protocol), `lower_bounds/` and
`literature_values.csv` (every published value with its table and page); the published values of
2024–2026 and the lower bounds are collected in `literature_replay/bounds_and_recent.json`.

The checkers verify feasibility and makespan from explicit start times, so they do not depend on
the solvers or on how the schedules were found.

## Re-running the agent-written solvers

The `README.md` of each folder gives the exact build and run commands. For example, the
job-shop corridor solver:

```bash
cd 1_jssp_tai_100_1000_1/generalization
gcc -O2 -o csp3 ../solver/csp3.c -lm
# run_commands.txt lists the exact runs used in the paper
```

Most solvers are wall-clock limited and multi-threaded, so a re-run reproduces the method and
the quality level, but in general not the identical schedule. The MKT10 = 499 schedule is an
exception: it reproduces bit for bit from source with the recorded seed (`3_fjspt/RUNS.md`).
Scripts that call Python use `python3` unless the `PYTHON` environment variable is set.

## How the results were obtained

* **Agent:** Claude Code 2.1 with Claude Sonnet 5, Claude Opus 5, Claude Opus 5.5 and Claude Fable 5.1 (all released in 2026).
* **Autonomous runs:** one Docker container per run (Harbor task framework), 4 CPUs and 4 GB of
  memory, 4–5 h budget. The agent received only the instance and a task text (format, rules,
  output format and target; see `0_prompts/`). The Behnke task asked for a heuristic but named no
  method, and no commercial solver was available.
* **Assisted runs:** interactive sessions in which we gave goals, papers or more cores, but
  wrote no solver code.
* **Workstation:** AMD Ryzen 9 5950X (16 cores / 32 threads), 125 GB RAM.

Every agent run on the instances of the paper is reported, including the runs that missed their
target (paper, Table 3).

## Instance sources

* **Job shop:** [JSPLib / ScheduleOpt benchmarks](https://github.com/ScheduleOpt/benchmarks), set `DacolTeppan2022`.
* **Flexible job shop:** FJSPLib (same repository) and the original distribution of Behnke & Geiger (openHSU, doi:10.24405/436).
* **Flexible job shop with transport robots:** the data of Homayouni & Fontes (2021), checked against three independent copies.

Pinned commits and checksums are in each `SOURCE.md`.

## Citation

```bibtex
@unpublished{albrecht2026agentheuristics,
  author = {Albrecht, Andre and Frazzon, Enzo Morosini and Coelho, Leandro C. and Vieira, Bruno},
  title  = {Agent-written heuristics versus commercial solvers on unsolved job-shop benchmark instances},
  note   = {Submitted to International Transactions in Operational Research},
  year   = {2026}
}
```

## Contact

Andre Albrecht: andre.albrecht@posgrad.ufsc.br
