# Independent validation of the FJSPT lower bounds of Section 3.2

Date: 2026-11-10. All three bounds were re-implemented from scratch from their definitions in the paper and
from `../check_fjspt.py`; the code with which the bounds were first computed was not available and was
not used. Solvers: OR-Tools CP-SAT 9.15, IBM CPLEX 22.2 (MIP) and IBM CP Optimizer 22.2, 8 threads each,
on the Ryzen 9 5950X workstation.

Bounds: (1) integer-program bound (vehicle work + job tail, machine load with heads and tails, job paths);
(2) closed form; (3) relaxation: flexible job shop with loaded trips as time lags (no vehicle capacity).

## 0. Verdicts

| Claim | Verdict |
|---|---|
| (1) MKT01–10 = 167, 120, 258, 235, 285, 244.5, 228, 662.5, 566.5, 456 | **Confirmed, all 10**; CP-SAT and CPLEX agree, all proven optimal, ≤ 4.3 s each. These are the values with empty moves charged at **shortest-path** times. With **direct** times (as `check_fjspt.py` charges them) MKT06 = 245 and MKT10 = 459.5; the other 8 are equal. Both are valid; the paper uses the shortest-path version, which is valid under either reading. |
| (2) MKT01 165, MKT04 234, MKT05 282 | **Confirmed** (exact arithmetic). |
| (3) mt10c1t 978, mt10cct 948, mt10xt 975, setb4xyzt 939, seti5c12t 1240, mt10xxxt 963, mt10xyzt 901, mt10xyt 950 | **Confirmed, all 8**; CP-SAT and CP Optimizer both prove optimality, with and without starting solutions. |
| (3) MFJST09 = 1075 | **Confirmed** by both solvers. |
| (3) equals the proven optimum on MFJST01–05, 07, 08, SFJST10 | **Confirmed**; it also equals the optimum on MFJST06 and SFJST01–09. |
| (3) solved on 20 of 21 CB instances | **21 of 21** with both solvers, no hints (CP-SAT ≤ 12.8 s, CP Optimizer ≤ 37.4 s). |
| HA-DQN MKT01 153, MKT04 213, MKT05 251 and LAHC MKT04 225 are below (1) | **Confirmed** (167, 235, 285, 235); the closed form (165, 234, 282) already suffices. |
| HA-DQN CB 969, 933, 942, 932, 1217 below (3) | **Confirmed** (978, 948, 975, 939, 1240); also 872 < 963 and 816 < 901. HA-DQN 958 (setb4c9t), 946 (setb4cct), 1209 (seti5cct), 1208 (seti5xyzt) are **not** ruled out (bounds 954, 940, 1194, 1190). |
| Our MKT05 286 is 1 above the bound 285; our mt10xyt 950 is optimal | **Confirmed.** |
| Soundness | **No violation** (Section 4). |

Additional: relaxation (3) on MFJST10 = 1219 (proven by CP Optimizer and CP-SAT).

## 1. Files

| File | Content |
|---|---|
| `common.py` | loader (exact fractions, scaled ×2), heads/tails, rounding up to the time grid |
| `bound1_ip.py` | bound (1) as a solver-neutral IP (CP-SAT, or CPLEX via an LP file); flags `--sp-empty`, `--force-all-vehicles`, `--ablate qpmin,nohead` |
| `bound2_closed.py` | closed form (2) and a stronger variant |
| `bound3_relax.py` | relaxation (3) in CP-SAT and in CP Optimizer |
| `soundness.py` | schedule-based soundness tests → `results/soundness.csv` |
| `rand_check.py` | random instances against exact optima → `results/rand_check*.{csv,txt}` |
| `hf_lb.py`, `mkt_fast_jobdata_compare.py`, `mkt04_layout_probe.py`, `mkt04_perm_probe.py` | MKT03/MKT04 investigation → `results/hf_lb_recomputed.txt`, `results/mkt04_*.txt` |
| `make_table.py` | `results/lower_bounds.csv`, `results/table.md` |
| `run_bound1.sh`, `run_bound3.sh`, `run_bound3_nohint.sh` | batch runs; one JSON per solve in `results/b1`, `results/b3`, `results/b3_nohint`; logs in `logs/` |
| `results/ablation*` | bound (1) with components switched off |
| `old_tag/` | `git archive` of tag `with-fjspt-lower-bound` (the earlier agent-written bound), read-only |

## 2. Semantics (from `check_fjspt.py`)

- Travel times are used directly: an empty move from drop-off `l` to pickup `a` takes `travel[l][a]`; a loaded trip takes `travel[a][b]`.
- Operation k has a trip into it iff it is the first operation of its job (from location 0) or its machine differs from that of the previous operation.
- Makespan = last completion; no return trip. Time grid g = 0.5 on the 10- and 15-machine layouts, else 1.
- **The triangle inequality fails** on the 15-machine layout (MKT06, MKT10; 255 triples), on every CB layout, on MFJST02–07, SFJST06/09 and on 2 triples of the 10-machine layout.

## 3. Results

See `results/table.md` for the full table (51 instances: (1) CP-SAT/CPLEX direct and shortest-path, (2), (3) CP-SAT/CPO, old bound, best certified schedule, published values ruled out). All 4 × 51 solves of (1) are proven optimal; CP-SAT and CPLEX agree on all 51.

## 4. Soundness tests (no violation)

1. 177 schedules re-certified by `check_fjspt.check` (our 31 best, 100 protocol runs, 10 CP-SAT baselines, 14 literature replays, 20 generalization best, 2 Meng MFJST replays) are ≥ every bound and every variant. 0 violations.
2. The 18 proven optima (16 HF2021 MILP optima SFJST01–10, MFJST01–06; MFJST07 = 898, MFJST08 = 900 by CP-SAT) are ≥ every bound; (3) is tight on all 18.
3. Constraint-level test of IP (1): for each of the 177 schedules, the IP solution it induces (assignment, vehicle trip sequences with empty moves, last-trip indicators with actual tails, c = makespan) satisfies every row. 0 violations; the test detects a makespan lowered by 1 (fails on MKT05 and MKT10, where a vehicle has W_v + q_v equal to the makespan).
4. 403 random instances with exact optima (160 tiny instances by complete enumeration with the old `brute.py`; 146 random 2–6 jobs × 2–4 machines, half non-metric, solved exactly; 97 sub-instances of MKT/CB on their real layouts): 0 violations for every bound.

## 5. Issues in the proofs (no number changes)

- **A. Empty-move convention.** "Last drop-off ≥ W_v" requires W_v to charge each empty move at the time the model requires. With non-metric layouts, direct and shortest-path times differ (MKT06, MKT10). State the convention; shortest-path is valid under both readings.
- **B. "|V| routes start at 0".** Forcing every vehicle to be used is not obviously w.l.o.g. on non-metric layouts. Sound form: at most |V| routes, plus C ≥ W + q when only one vehicle is active. Identical values on all 51 + 403 instances.
- **C. q_v must depend on decisions:** q = processing time of the dropped operation plus the shortest remaining processing and loaded-travel time of its job; in the IP one last-trip indicator per active vehicle.
- **D. Heads/tails** with multi-machine operations: valid if computed with the operation on machine k.
- **E. n − |V| empty trips into location 0:** valid (flow conservation).
- **F. Consecutive operations on the same machine:** no trip, lag, pickup or drop anywhere in the model (verified by the constraint-level test).
- **G. Rounding to the time grid:** valid for a proven optimum or dual bound only, never for an incumbent.
- **H. Closed form (2):** valid also if a vehicle is idle.
- **I. Relaxation (3):** valid; report only proven optima or dual bounds.
- **J.** HF2021 Table 20, setb4xxt LB2 is printed as 462; recomputed 439 (462 is the setb4xxxt entry).

## 6. Old bound versus bound (1)

The removed agent-written bound is bound (1) without the job-specific tail (it used |V| × the instance's minimum processing time) and without machine heads/tails. Switching exactly these off reproduces the old bound on all 10 MKT instances (165, 118, 252, 234, 282, 242.5, 222, 662, 562, 454; 451 on MKT10 with shortest-path empties). Almost all extra strength comes from the job tail q_v; heads/tails add at most 1. On MKT10 compare like with like: 456 vs 451 (shortest-path), 459.5 vs 454 (direct).

## 7. MKT03 and MKT04

- Our MKT04 job data are identical to the HF2021 FAST PDF (MK01–05 newly compared) and the 8-machine layout is identical; Berterottière's MKT04 schedule decodes to exactly 254 on our file.
- HF2021's own LB1/LB2 (Eqs. 26–27) reproduce their published values on 48 of 51 rows; exceptions: MKT03 (117/200 published vs 120/203 recomputed), MKT04 (80/152 vs 81/176) and the setb4xxt typo.
- No alternative layout or machine relabelling explains the MKT03/MKT04 differences. HF2021's MKT03 and MKT04 experiments were probably run on a different data version. On the distributed MKT04, LAHC 225 is infeasible ((2) gives 234, (1) gives 235).

## 8. Reproduce

```bash
P=python3   # any environment with ortools 9.15 and docplex (CPLEX / CP Optimizer 22.2)
./run_bound1.sh
./run_bound3.sh cpsat 300 & ./run_bound3.sh cpo 300
./run_bound3_nohint.sh cpsat 300 & ./run_bound3_nohint.sh cpo 300
python3 bound2_closed.py ../instances/*.json ../generalization/instances/*.json
$P soundness.py
$P rand_check.py --groups TMS --nmed 150 --nsub 100 --procs 4
python3 hf_lb.py ../instances/*.json ../generalization/instances/*.json
$P make_table.py
```
