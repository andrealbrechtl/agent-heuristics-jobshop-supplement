# Flexible job shop with two transport robots (FJSPT) — MKT01–10 and Chambers–Barnes

Paper: Sections 2.3 and 5, Tables 1–4, Figs. 7–11.

## Verify

```bash
# all 31 certified schedules (exact arithmetic, no dependencies)
for f in solutions/*.sol; do python check_fjspt.py instances/$(basename $f .sol).json $f; done
# e.g.  MKT10: FEASIBLE  makespan=499  transports=140

# published schedules of competing papers, re-derived from the authors' own files
python literature_replay/decode_berterottiere.py instances/MKT10.json <lucasberter/FJSPT>/Homayouni_brandimarte/Mk_10.txt out.sol
python literature_replay/replay_meng_iga.py literature_replay/meng2023_appendix_solutions.json instances out/

# anatomy numbers of Section 5.4 (vehicle loads, idle time, critical path)
python anatomy.py instances/MKT10.json solutions/MKT10.sol
```

`../verify_all.py` runs all of this (and the JSSP/FJSP checks) in one go.

## Model checked

Homayouni & Fontes (2021); the same model as Meng et al. (2023) and Berterottière et al. (2024).
Two identical unit-capacity vehicles and all jobs start at the load/unload station L/U
(location 0). Operation k needs a loaded transport from the location of operation k−1 (L/U for
k = 1) unless both are on the same machine. A vehicle drives empty from its last drop-off to the
pickup and may wait there. Machines are unary, buffers unlimited, no return to L/U, objective
makespan. `check_fjspt.py` checks every one of these rules on the explicit start/arrival times
in the `.sol` file, with exact fractions (several layouts use half units). It shares no code
with the solver or with the campaign's original checker.

`.sol` format: `op job op machine start end` and `tr job op vehicle from to depart arrive`
(transport into that operation; 1-based ids; location 0 = L/U).

## Instances and their provenance

`instances/*.json`: `jobs[j][k]` = list of `[machine, processing time]`, `travel[a][b]`
(row/column 0 = L/U), original time units.

Each instance is Brandimarte MKxx (MKT01–10) or a Chambers–Barnes FJSP (21 instances,
suffix `t`) combined with the Homayouni–Fontes travel-time layout of the same machine count.
`instance_sources/fetch_and_compare.sh` downloads all sources at pinned versions and compares
(`ALL OK` on 2026-10-11):

| Source | What is compared | Result |
|---|---|---|
| FAST project page (Homayouni & Fontes), `4to18machines_layouts-1.pdf` | travel matrices, all 31 | identical |
| FAST `chambersbarnes1996-1.pdf` | CB job data | identical, except `setb4xxx` (below) |
| FAST `brandimarte1993-2.pdf` | MK job data | identical where the PDF text is parseable (MK03, 06–10) |
| SchedulingLab/fjsp-instances @ `ac4c340` | job data, all 31 | identical |
| ScheduleOpt/benchmarks @ `f89e5b0` | MK job data | identical |
| wangyuting9836/FJSP_AGV_MILP @ `48b3f21` | MK job data and travel | identical (MK06: on the machines used) |

Findings:
* **`setb4xxx` data error in the distributed file.** The FAST PDF's `setb4xxx` block is a verbatim
  copy of `seti5xxx` (15×18). `setb4xxxt.json` uses the correct `setb4xxx` job data (15×13,
  identical to SchedulingLab) with the 13-machine layout.
* MK06: the header says 15 machines (FAST, ScheduleOpt) or 10 (SchedulingLab, FJSP_AGV_MILP);
  only machines 1–10 are used and the 15-machine layout restricted to them equals the
  10-machine layout, so all versions describe the same instance.
* MK10 uses 11 of its 15 machines (1–10 and 13); MK08 uses 9 of 10 (machine 6 unused).
* 17-machine layout (`seti5cct`, `seti5xxt`, `seti5xyt`): the published matrix has travel time 0
  from machine 7 to machine 6 (and a non-zero diagonal entry in row 6, which is never used).
  We keep the published data; each of our three schedules uses this zero-time move once.

sha256 of the FAST PDFs used: `4to18machines_layouts-1.pdf` 9a84e319…8bc9,
`brandimarte1993-2.pdf` 25e10959…c469, `chambersbarnes1996-1.pdf` 8305e846…3b77
(full values in `../CHECKSUMS.sha256` together with all supplement files).

## Results

"Best published" is the smallest published value; every published value is taken as feasible.
Sources: LAHC (Homayouni & Fontes 2021), IGA (Meng et al. 2023), TS (Berterottière
et al. 2024), HA-DQN (Dong et al. 2025, no schedules published), ACO (He et al. 2025, never the
smallest).

| Instance | Best published | Source | **This work** | Change |
|---|---|---|---|---|
| MKT01 | 153 | HA-DQN | 173 | +13.1 % |
| MKT02 | 122 | HA-DQN | 124 | +1.6 % |
| MKT03 | 342 | IGA, TS | **294** | −14.0 % |
| MKT04 | 213 | HA-DQN | 238 | +11.7 % |
| MKT05 | 251 | HA-DQN | 286 | +13.9 % |
| MKT06 | 321.5 | IGA | **279.5** | −13.1 % |
| MKT07 | 243 | HA-DQN | 245 | +0.8 % |
| MKT08 | 723 | HA-DQN | **696** | −3.7 % |
| MKT09 | 681 | HA-DQN | **598** | −12.2 % |
| MKT10 | 616.5 | TS | **499** | −19.1 % |

Our certified schedules are below every published value on 5 of the 10 MKT instances (MKT03, 06, 08,
09, 10). On the other five, HA-DQN (Dong et al. 2025) reports lower values; no schedules were
published, so they could not be checked with `check_fjspt.py`.

Chambers–Barnes (paper Table 4): against LAHC and IGA, 13 of 21 improved, 1 tie, 7 above (≤ 1.2 %).
HA-DQN reports 11 of these instances. On `mt10xxxt` and `mt10xyzt` its values (872, 816) are below
the FJSPLib optima of the same instances without transport (918, 847), so the next published value is
used there. The other nine are all below ours. Counting them, our values are below every published
value on 8 of 21.

Every published value in the paper's Tables 2 and 4 was checked against the source tables
(HF 2021 Tables 10, 11, 19; Meng et al. 2023 Tables 6, 7; Berterottière et al. 2024 Table 6;
Dong et al. 2025 Tables 6, 7; He et al. 2025 Table 7). Every value with its source location is in
`literature_values.csv`.

## Literature replays (`literature_replay/`)

* `berterottiere2024_MKTxx.sol`: the ten published tabu-search solutions
  (github.com/lucasberter/FJSPT @ `c9b85d8`, `Homayouni_brandimarte/Mk_xx.txt`, machine and
  vehicle sequences) decoded into earliest-start schedules by `decode_berterottiere.py`. All ten
  give exactly the published makespans (187, 134, 342, 254, 310, 336.5, 289, 770.5, 709, 616.5)
  and pass `check_fjspt.py`.
* `meng2023_iga_MKTxx.sol`: the four solutions printed in Appendix A of Meng et al. (2023)
  (MKT01, 02, 03, 05; strings in `meng2023_appendix_solutions.json`, checked verbatim against
  the published text) replayed by `replay_meng_iga.py`: 177, 126, 342, 295, exactly the values
  of their Table 6.

These two replays show that our model and data are those of both papers.

## Runs, solver and time to solution

`RUNS.md`: for each of the 31 certified schedules, the campaign, solver version, exact
command, seed, budget and time to value. `protocol_runs/`: all 100 schedules and logs of the
frozen-method protocol (Table 8; all 100 pass the checker; on the five leading instances all 50 are below the best published value).
`solver/`: `fjspt_sa2.cpp` (v2, the method of the paper), `fjspt_sa.cpp` (v1 screen),
`to_txt.py` (instance conversion for the solver). Build: `g++ -O3 -march=native -std=c++17
-o fjspt_sa fjspt_sa2.cpp`.

**The MKT10 record is reproducible bit for bit:** rebuilding `fjspt_sa2.cpp` and running seed 511
(command in `RUNS.md`) writes a file identical to `solutions/MKT10.sol` after about 18 minutes.
