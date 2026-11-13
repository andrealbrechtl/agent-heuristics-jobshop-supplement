# CP-SAT baseline for the FJSPT (two AGVs, Homayouni & Fontes 2021 model)

`cpsat_fjspt.py` is an exact-model baseline (OR-Tools CP-SAT 9.15) for the rules enforced by `../check_fjspt.py`.
It is meant to be run with a fixed wall-clock budget and compared against heuristics.

## Usage
```bash
python cpsat_fjspt.py ../instances/MKT01.json --time 3600 --workers 8 --seed 0 \
       --out best.sol --log curve.csv [--hint start.sol] [--vehicles 2]
python ../check_fjspt.py ../instances/MKT01.json best.sol
```
* `--time` is the total wall-clock budget (includes model building and hint completion).
* `--out` is rewritten at every improving solution (always the incumbent); `--log` gets `wall_s,objective,bound`
  per improving solution (original time units) plus a final line; same lines are printed to stdout.
* `--hint file.sol`: any `.sol` in the checker format. The hint is completed (auxiliary variables) by a
  1-worker propagation solve; the script prints "hint completed and accepted by the model" or a WARNING if the
  hint is infeasible for the model. Without `--hint`, a built-in greedy dispatch schedule is used as hint
  (`--no-greedy` disables it; note that without any hint CP-SAT needed ~40 s for a first solution on MKT01).
  For a clean "no prior knowledge" baseline use the default greedy start, for a "polish a heuristic solution" use `--hint`.
* `--fix-hint`: debug; fixes every variable to the hint (pure model feasibility test, returns in < 1 s).
* `--param key=value` passes any CP-SAT parameter; `--no-cumulative` drops the redundant vehicle cumulative.
* Times are multiplied by 2 internally (half units); outputs are converted back.

## Model
* Per operation (j,k): machine literal `x[j,k,m]` (exactly one), integer machine var, start S, end E = S + p_m;
  optional fixed-size interval per eligible machine; `NoOverlap` per machine.
* One potential transport "into" every operation, with `dep`, `arr`. `pres` = 1 iff machine(k) != machine(k-1)
  (always for k=1; L/U = location 0). The loaded duration comes from a table constraint
  (machine(k-1), machine(k), T) with duration 0 on equal machines, so `arr = dep + dur` holds for every node and an
  absent transport reduces to `S_k >= E_{k-1}`. Always `dep >= E_{k-1}` (0 for k=1) and `S_k >= arr`.
* Vehicles: nodes = potential transports; `AddMultipleCircuit` with depot = L/U-start/end; absent transports take the
  self-loop (literal `not pres`); at most `--vehicles` routes leave the depot (both vehicles identical, start at L/U at 0).
  Arc i->j (on the same vehicle consecutively): `dep_j >= arr_i + T[machine(i)][pickup(j)]`, where the pickup location of j is
  machine(k-1) of its job (or 0): implemented with `D[i,b] = arr_i + T[machine(i)][b]` (element constraint) and
  `dep_j >= D[i,b]` enforced by (arc literal AND `x[j,k-1,b]`). Depot->j: `dep_j >= T[0][pickup(j)]`. Vehicles may wait.
  Arcs from a transport to an earlier operation of the same job are pruned. The routes are decomposed to vehicle ids on output.
* Redundant: cumulative (capacity 2) on the loaded trips, job-path and head bounds. Objective: min makespan.
* Size: variables ~ transports^2 arcs (MKT10: ~225 nodes, ~50k arcs, ~65k variables, ~450 MB RAM, ~20 s to build + complete hint).

## Validation (4 workers, this machine under shared load)
All output .sol files were accepted by `check_fjspt.py` with identical makespan.
* Fixed-hint feasibility (all variables fixed): `solutions/MKT01.sol` (173), `literature_replay/berterottiere2024_MKT01.sol` (187),
  `solutions/MKT10.sol` (499), and the greedy schedules (MKT01/05/10): feasible, OPTIMAL in < 1 s, checker OK.
  So the model accepts these schedules (not too strict).
* Normal hint (not fixed): hint 173 -> first incumbent 173 at 1.3 s (end 40 s: 173, bound 66);
  hint 187 (Berterottiere) -> incumbent 187 at 1.6 s, not improved in 40 s; MKT02 `solutions/MKT02.sol` -> 124 at 2.9 s.
* Free runs (greedy start, 4 workers, 110 s): MKT01 205 (greedy 285, bound 66.5, best known 173, LB 165);
  MKT02 158 (greedy 224, bound 53, best known 124, LB 118). MKT05 (30 s): 448 from greedy 489; MKT10 (30 s): no improvement over the greedy
  1138.5 (best known 499).

## Expected scaling issues
* The proven bound from CP-SAT stays weak (e.g. 100 on MKT01, below the published bound of 125): the sequence-dependent
  empty travel is only linked through big-M-like enforced linear constraints; the optimality gap will not close beyond
  the smallest instances.
* Model size grows quadratically in the number of operations (arc literals); MKT08-10 (~200-250 transports)
  are large, building and hint completion cost ~20 s, memory ~0.5 GB with 4 workers (more per extra worker).
* Without a good start solution first solutions are slow (MKT01: 37 s); the built-in greedy start is poor on large
  instances (MKT10: 1138 vs 499), so improvement on MKT08-10 within modest budgets is expected to be small.
* 4-worker results are noisy across seeds/parameters (e.g. MKT01 40 s runs gave 196-229); use several seeds and 8+ workers
  for the final comparison, and report the incumbent curves from `--log`.

## Baseline runs used in the paper

`results/`: one run per MKT instance, 600 s, 8 workers, seed 0, greedy start (commands in
`results/run_commands.txt`, run from `results/`). Final makespan (bound): MKT01 184 (100),
MKT02 156 (61), MKT03 556 (210), MKT04 310 (173), MKT05 397 (196), MKT06 457 (121),
MKT07 363 (143), MKT08 1184.5 (587), MKT09 1233 (362.5), MKT10 1080 (240).
All ten `.sol` files pass `../check_fjspt.py`. `*.csv`: incumbent trace (wall s, makespan, bound).
