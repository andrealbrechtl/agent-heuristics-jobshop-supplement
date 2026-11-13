# Flexible job shop, Behnke & Geiger (2012) — 404 on `behnke16`, 226 on `behnke31`

Paper: Sections 2.2, 3, 4.2, Table 1, Figs. 5–6.

| Instance | Size | Tracked LB / UB (FJSPLib) | This work | Status |
|---|---|---|---|---|
| `behnke16` | 100×20, 500 ops | 391 / 412 | **404** (also 406) | open; UB improved by 8 |
| `behnke31` | 50×40, 250 ops | 226 / 226 | **226** | tracked as closed; value matched |
| `behnke14` | 50×20, 250 ops | 221 / 230 | 232 | miss (above tracked UB) |

FJSPLib bounds: `ScheduleOpt/benchmarks` commit `f89e5b007c90a1c1a70fa48a28d657a72da05665`,
`flexible-jobshop/solutions/bks.json`, same values on https://scheduleopt.github.io/benchmarks/fjsplib/.
All three UBs are credited to OptalCP, all three LBs to "DOFP2026a" (personal communication,
2026-08-01); none carries a certificate. "Closed" for `behnke31` therefore means *tracked*
LB = UB = 226; no published proof was found.

## Verify

```bash
python check_fjsp.py instances/behnke16.fjs solutions/behnke16_404.txt            # RESULT: OK makespan 404
python check_fjsp.py instances/behnke16.fjs solutions/behnke16_406.sched          # RESULT: OK makespan 406
python check_fjsp.py instances/behnke31.fjs solutions/behnke31_226_seed44.sched   # RESULT: OK makespan 226
python check_fjsp.py instances/behnke31.fjs solutions/behnke31_226_norr_ablation.sched
python check_fjsp.py instances/behnke14.fjs solutions/behnke14_232.sched          # RESULT: OK makespan 232
```

`check_fjsp.py` is independent of the solvers. It checks that every operation is scheduled once,
on an eligible machine, for its exact processing time; job precedence; machine non-overlap; and
the claimed makespan. Schedule rows are `job op machine start end`. The 404 file uses 1-based
machines, the others 0-based; the checker tries both bases and accepts a file only if exactly
one base makes it feasible.

## Instance provenance

* `instances/behnke{14,16,31}.fjs` are byte-identical to `ScheduleOpt/benchmarks`
  (commit above) `flexible-jobshop/instances/fjsp/BehnkeGeiger2012/behnke{14,16,31}.txt`.
  Note: the sibling folder `fajsp/BehnkeGeiger2012/` (assembly job shop) uses the same file names
  for different data.
* They are identical, apart from CRLF line endings, to the original distribution of
  Behnke & Geiger, research report RR-12-01-01, Helmut-Schmidt-Universität Hamburg
  (openHSU, doi 10.24405/436, zip sha256 `0a9fa1367ff99a37a5f6adc0018728bf503abc6bdb3cc372eac0983f409ee43a`),
  where the files are already named `Behnke14/16/31.fjs`. Report Table 20: 14 = Sm03.4,
  16 = Sm04.1, 31 = Med03.1.
* sha256: behnke14 `c10f9775…fa68`, behnke16 `04d050b5…06d8`, behnke31 `7e3b5f41…ba1f`
  (full values in `../CHECKSUMS.sha256`).
* Structure claims verified: every job has 5 operations; in `behnke16` operation 0 is eligible
  on machines 1–4, operations 1–3 on 5–16, operation 4 on 17–20; header average eligibility
  6.10 / 6.47 / 12.93 equals the recomputed 6.096 / 6.472 / 12.928 after rounding.

## All autonomous sessions

`solutions/autonomous_sessions/` holds the final schedule of every autonomous session (Claude
Opus 5, 4–5 h budget): sessions 1–5 on `behnke16` (404, 411, 415, 417, 419) and sessions 6–7 on all
three instances (`behnke14` 235/234, `behnke16` 416/415, `behnke31` 229/226). All pass `check_fjsp.py`.

## How each schedule was produced

| File | Makespan | Solver | Run | Time to this value |
|---|---|---|---|---|
| `behnke16_404.txt` | 404 | `solver/stage_aware_tabu_lns_404/` (`solver2.cpp` + `polish.cpp`, written in the session) | autonomous coding-agent session, ~31 worker threads in a container with a quota of 4 CPUs; `run.sh` | not logged (see below) |
| `behnke16_406.sched` | 406 | `solver/mg_tabu_reusable/fjsp_solver.cpp` | `run_all.sh`: 1,200 s, 32 threads, default seed | 56.5 s |
| `behnke31_226_seed44.sched` | 226 | same binary | `run_variants.sh`: 90 s, 32 threads, seed 44 | 4.15 s |
| `behnke31_226_norr_ablation.sched` | 226 | same source, `FJSP_NO_RR=1` (random kicks instead of ruin-and-recreate) | `run_ablation.sh`: 120 s, 32 threads | 22.3 s |
| `behnke14_232.sched` | 232 | same binary | `run_all.sh`: 1,200 s, 32 threads | 22.9 s |

Build of the reusable tabu: `g++ -O3 -march=native -pthread -std=c++17 -o bin_fjsp fjsp_solver.cpp`;
usage `bin_fjsp instance seconds threads out.sched out.csv [target] [seed]`.
Progress files (time, best makespan) for every run are in `logs/mg_tabu_progress/`
(`variants/` = 90 s runs with seeds 11–66, default and no-ruin-and-recreate).
The 1,200 s run on `behnke31` ended at 227 (72 s); 2 of 6 default 90 s seeds reached 226
(seed 44 at 4.15 s, seed 55 at 81.1 s). Four short `behnke16` runs ended at 408/410/409/410.
On `behnke14`, 12 runs (6 seeds × 2 variants) ended at 232–235.

**404 provenance.** The 404 schedule was written by a coding-agent session (4.3 h container
session, network available but no web tool used) that
wrote and ran `solver2.cpp` (stage-aware tabu + LNS with exact re-optimisation of the last stage,
annealed acceptance) and `polish.cpp`. The session kept no solver log. The progression
448 → 417 → 414 → 412 → 408 → 404 and "about an hour on 31 threads" come from the agent's own
`README_agent.md`; file timestamps indicate the final schedule was written about 3.2 h into the
session. The schedule itself is certified by `check_fjsp.py`.
