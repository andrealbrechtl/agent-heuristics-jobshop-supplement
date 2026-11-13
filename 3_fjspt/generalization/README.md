# Generalization test: unchanged FJSPT solver on SFJST01–10 and MFJST01–10

Question: does the frozen simulated-annealing solver (`../solver/fjspt_sa2.cpp`, options frozen for MKT) work on the
other Homayouni & Fontes (2021) data set (set 3, built from the Fattahi et al. 2007 FJSP sets `sfjs01–10`, `mfjs01–10`;
2 identical AGVs)? Solver and options were **not** changed or tuned. Nothing outside this folder was modified.

## 1. Instances (`instances/*.json`, built by `build_instances.py`)
* Job data: FAST project file `fjspt_hf2020.pdf` (Fattahi sets in precedence format; text in `sources/fjspt_hf2020.txt`,
  PDF sha256 a12b5e2d6fa256120f482dcfb0202db7af55c857b516ec292a8f5f9e3f82d50e). Precedence chains are converted to ordered operations, machines made 1-based.
* Travel matrix: FAST `2to8machines_layouts.pdf` (PDF sha256 777997a4e6b8b5e7ee45ec24441052802da2431768dbfe58bedc326bd29ce312; pdftotext copy in `sources/`), layout with the same machine count as the job set
  (HF2021 §5.4: literature layouts for 4,5,6,8 machines, random 2–10 for 2,3,7; the FAST file carries all of them;
  the 4,5,6,8 layouts are identical to `4to18machines_layouts-1.pdf`). Row/col 0 = L/U. Mapping (instance → job set → layout):
  SFJST01/02/03/04/05 → sfjs01–05 → 2-machine layout; SFJST06, 09 → 3; SFJST08 → 4; SFJST07, 10 → 5; MFJST01 → 6; MFJST02–07 → 7; MFJST08–10 → 8
  (always the layout of the job set's machine count; `layout_machines` field in each JSON).
* Cross-checks done in `build_instances.py`: job data identical (all 20) to SchedulingLab/fjsp-instances @ ac4c340 (`fattahi/*.json`)
  and to lucasberter/FJSPT @ c9b85d8 (`fattahi/*.txt`).

### Validation (all 20 instances accepted)
1. **MFJST01–10 against Berterottière et al. (2024) published schedules** (`FJSPT/Homayouni_fattahi/mfjsNN.txt`,
   decoded with `../literature_replay/decode_berterottiere.py` unchanged; op numbering is job-by-job as for MK):
   decoded makespan = published Cmax **exactly on 10/10** (485, 463, 482, 579, 532, 652, 901, 900, 1133, 1270) and `check_fjspt.py` says FEASIBLE for each.
   This validates job data, layouts 6/7/8 and the decoding. (No such files exist for SFJST.)
2. **MFJST09, MFJST10 against Meng et al. (2023) Appendix A** (`replay_meng_mfjst.py`, `literature_replay/*.sol`): replay gives 1117 and 1228
   exactly as reported; `check_fjspt.py` FEASIBLE.
3. **Independent exact model** (`exact_cpsat.py`, OR-tools CP-SAT, no shared code; logs in `logs/exact_cpsat_*.txt`):
   optimum of our instance files equals the HF2021 MILP optimum on **all 16 instances for which HF2021 proved optimality**
   (SFJST01–10: 70, 111, 223, 359, 123, 324, 409, 269, 220, 531; MFJST01–06: 485, 463, 482, 576, 532, 652).
   This is the validation for SFJST (no published schedules) and for layouts 2–5. Not validated by a published schedule: SFJST only through (3).
   CP-SAT additionally proves MFJST07 = 898 (11 s) and MFJST08 = 900 (37 s) optimal (HF2021 only had bounds 788/780; Meng/LAHC values 898/900 are therefore optimal).
   MFJST09/10 not solved in 300 s: CP-SAT 1094 (LB 944) and 1237 (LB 1039).

## 2. Published values (`results.csv` columns)
Sources: HF2021 Tables 8, 9, 18 (JOGO 79:463–502, pp. 489–491, 500); Berterottière et al. 2024 = `Cmax` in the authors' result files
(600 s runs; only MFJST exist); Meng et al. 2023 Table 5 (IGA column; MILP column = HF2021 optima); He et al. 2025 Tables 5–6 ("Our work", ACO).
Meng and He report no SFJST results of their own beyond He's ACO (equal to the optimum on all 10).

| Instance | MILP opt | MILP LB (open) | LB (HF21 Tab.18) | LAHC H=1000 | LAHC H=100 | TS Berterottière24 | IGA Meng23 | ACO He25 | our CP-SAT |
|---|---|---|---|---|---|---|---|---|---|
| SFJST01 | 70 |  | 70 | 70 | 70 | – | – | 70 | 70 |
| SFJST02 | 111 |  | 111 | 111 | 111 | – | – | 111 | 111 |
| SFJST03 | 223 |  | 223 | 223 | 223 | – | – | 223 | 223 |
| SFJST04 | 359 |  | 357 | 359 | 359 | – | – | 359 | 359 |
| SFJST05 | 123 |  | 121 | 123 | 123 | – | – | 123 | 123 |
| SFJST06 | 324 |  | 322 | 324 | 324 | – | – | 324 | 324 |
| SFJST07 | 409 |  | 409 | 409 | 409 | – | – | 409 | 409 |
| SFJST08 | 269 |  | 259 | 269 | 269 | – | – | 269 | 269 |
| SFJST09 | 220 |  | 220 | 220 | 220 | – | – | 220 | 220 |
| SFJST10 | 531 |  | 519 | 531 | 531 | – | – | 531 | 531 |
| MFJST01 | 485 |  | 472 | 485 | 485 | 485 | 485 | 485 | 485 |
| MFJST02 | 463 |  | 452 | 463 | 468 | 463 | 463 | 463 | 463 |
| MFJST03 | 482 |  | 472 | 482 | 482 | 482 | 482 | 482 | 482 |
| MFJST04 | 576 |  | 560 | 576 | 576 | 579 | 576 | 576 | 576 |
| MFJST05 | 532 |  | 520 | 532 | 532 | 532 | 532 | 532 | 532 |
| MFJST06 | 652 |  | 640 | 652 | 652 | 652 | 652 | 652 | 652 |
| MFJST07 |  | 788 | 796 | 898 | 907 | 901 | 898 | 907 | 898 |
| MFJST08 |  | 780 | 788 | 900 | 918 | 900 | 900 | 914 | 900 |
| MFJST09 |  | 780 | 788 | 1120 | 1181 | 1133 | 1117 | 1117 | 1094 (LB 944) |
| MFJST10 |  | 954 | 960 | 1238 | 1310 | 1270 | 1228 | 1267 | 1237 (LB 1039) |

Proven optimal: SFJST01–10 and MFJST01–06 (HF2021 MILP; confirmed by our CP-SAT), MFJST07–08 (our CP-SAT only).
Best known and open: MFJST09 (previously 1117, Meng IGA), MFJST10 (1228, Meng IGA). HF2021 MILP LB column: Table 9 `LB_M` (788/780/780/954); Table 18 final LB 796/788/788/960 (the two differ in the paper).
MFJST04: Berterottière's 579 is above the optimum 576. HF2021 LAHC H=100 on MFJST02 is 468 (463 is optimal).

## 3. Our results: 5 seeds × 60 s, unchanged solver, one thread per process
Command (per run, seeds 1–5, instance text files via `../solver/to_txt.py`, times ×2 as integers):
`./fjspt_sa2 --inst instances/txt/X.txt --seed S --time 60 --out X_sS.sol --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005`
(frozen MKT options except `--time 1200 → 60`; `run.sh` runs all 100 with `xargs -P 8`; built with `g++ -O3 -march=native -std=c++17` from `../solver/fjspt_sa2.cpp`, sha256 f1181032…a3ca).
All 100 output schedules (and the 100 of pass B) were checked with `../check_fjspt.py`: every one FEASIBLE with makespan equal to the claimed value (`all_checked_feasible` in `results.csv`).
Best schedule per instance: `solutions/*.sol`. Raw logs: `logs/runs_A` (unmodified), `logs/runs_B`.
Time to best: pass B reruns the same seeds with a copy of the source that only adds one stderr line when the incumbent improves (`run.sh` generates it);
its final values were identical to pass A on all 100 runs. Value = first time the final makespan was reached, median over the 5 seeds (the unmodified solver logs only at the end of cooling runs, e.g. `ttb_logA_upper_bound_s_median` in the csv). The 1e8-iteration cooling run lasts 17–57 s for these instances, so each 60 s run is 1–3 (small) or ~1 (medium) cooling runs.
Reference = proven optimum if known, else best published value (open instances).

| Instance | jobs×mach×ops | Reference | Best | Median | Worst | Seeds ≥ ref | Median gap % | Time to best, median (s) |
|---|---|---|---|---|---|---|---|---|
| SFJST01 | 2×2×4 | 70 (opt, MILP) | 70 | 70 | 70 | 5/5 | 0.0 | 0.00 |
| SFJST02 | 2×2×4 | 111 (opt, MILP) | 111 | 111 | 111 | 5/5 | 0.0 | 0.00 |
| SFJST03 | 3×2×6 | 223 (opt, MILP) | 223 | 223 | 240 | 4/5 | 0.0 | 29.18 |
| SFJST04 | 3×2×6 | 359 (opt, MILP) | 359 | 359 | 359 | 5/5 | 0.0 | 0.04 |
| SFJST05 | 3×2×6 | 123 (opt, MILP) | 123 | 123 | 123 | 5/5 | 0.0 | 0.00 |
| SFJST06 | 3×3×9 | 324 (opt, MILP) | 324 | 324 | 324 | 5/5 | 0.0 | 0.00 |
| SFJST07 | 3×5×9 | 409 (opt, MILP) | 409 | 409 | 409 | 5/5 | 0.0 | 0.00 |
| SFJST08 | 3×4×9 | 269 (opt, MILP) | 269 | 269 | 269 | 5/5 | 0.0 | 0.05 |
| SFJST09 | 3×3×9 | 220 (opt, MILP) | 220 | 220 | 220 | 5/5 | 0.0 | 0.19 |
| SFJST10 | 4×5×12 | 531 (opt, MILP) | 531 | 531 | 591 | 4/5 | 0.0 | 0.00 |
| MFJST01 | 5×6×15 | 485 (opt, MILP) | 485 | 485 | 511 | 4/5 | 0.0 | 0.04 |
| MFJST02 | 5×7×15 | 463 (opt, MILP) | 468 | 476 | 479 | 0/5 | 2.81 | 0.00 |
| MFJST03 | 6×7×18 | 482 (opt, MILP) | 482 | 491 | 500 | 1/5 | 1.87 | 0.06 |
| MFJST04 | 7×7×21 | 576 (opt, MILP) | 579 | 579 | 594 | 0/5 | 0.52 | 0.74 |
| MFJST05 | 7×7×21 | 532 (opt, MILP) | 543 | 561 | 596 | 0/5 | 5.45 | 0.08 |
| MFJST06 | 8×7×24 | 652 (opt, MILP) | 652 | 652 | 701 | 4/5 | 0.0 | 0.40 |
| MFJST07 | 8×7×32 | 898 (opt, CP-SAT; published only LB) | 919 | 925 | 981 | 0/5 | 3.01 | 0.33 |
| MFJST08 | 9×8×36 | 900 (opt, CP-SAT; published only LB) | 900 | 947 | 975 | 1/5 | 5.22 | 0.27 |
| MFJST09 | 11×8×44 | 1117 (best known, open) | 1090 **(< ref)** | 1129 | 1134 | 1/5 | 1.07 | 5.17 |
| MFJST10 | 12×8×48 | 1228 (best known, open) | 1244 | 1298 | 1327 | 0/5 | 5.7 | 2.49 |

Summary: optimum reached by at least one seed on SFJST01–10 (10/10) and MFJST01, 03, 06, 08 (reaching 485, 482, 652, 900); not reached on MFJST02 (best 468 vs 463), MFJST04 (579 vs 576), MFJST05 (543 vs 532), MFJST07 (919 vs 898, now known optimal).
On MFJST09 the best of the 5 seeds, 1090, is below the best published value 1117 and below our CP-SAT incumbent 1094 (feasible per `check_fjspt.py`, `solutions/MFJST09.sol`; the same instance reproduces Meng's 1117 and Berterottière's 1133 by independent decoding). MFJST10: 1244 vs 1228.
Runs at or below the reference: SFJST 48/50, MFJST 11/50 (59/100 overall; MFJST09's single run counts at 1090).
Observation: in the median run the final value was reached within about 6 s or less (column above) and no further improvement followed in the remaining time; the absolute temperature range (T0 = 4 → 0.2) is small relative to the processing times of these instances (tens to hundreds). This is stated as an observation, not tested (no tuning was done). Single 60 s budget, 5 seeds: results are not comparable with the published 600 s / 15–20-run protocols.

## Files
`build_instances.py`, `analyze.py` (results.csv, copies best schedules), `run.sh`, `exact_cpsat.py`, `replay_meng_mfjst.py`, `sources/` (FAST text files), `instances/` (+`txt/`), `solutions/`, `literature_replay/`, `logs/`, `results.csv`.
Re-run: `python3 build_instances.py sources <FJSPT clone> <fjsp-instances clone> instances; ./run.sh WORKDIR; python3 analyze.py WORKDIR`.
