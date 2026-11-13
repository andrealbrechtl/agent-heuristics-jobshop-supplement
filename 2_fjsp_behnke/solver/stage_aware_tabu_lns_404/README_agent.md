# behnke16 — flexible job shop heuristic

Target: beat the published upper bound of 412, i.e. reach a makespan ≤ 411.

## Files

| file | purpose |
|---|---|
| `solver2.cpp` / `solver4` | main solver: parallel tabu search + ruin-and-recreate LNS + exact end-stage re-optimisation |
| `polish.cpp` / `polish` | standalone post-optimiser for the two end stages |
| `stageopt.cpp` / `stageopt` | earlier, last-stage-only version of the polisher |
| `check.py` | independent feasibility checker (re-derives the makespan from the rows) |
| `schedule.txt` | best schedule found |
| `run.sh` | driver: long solver run, then polish, keeping the best |

Build: `g++ -O3 -march=native -pthread -o solver4 solver2.cpp`
Check: `python3 check.py /app/instances/behnke16.fjs schedule.txt`

## What the instance actually is

`behnke16` is 100 jobs × 5 operations on 20 machines, but the eligibility sets
reveal it is a **3-stage flexible flow shop**:

| stage | machines | operations | count | best achievable max machine load |
|---|---|---|---|---|
| A | 1–4   | op 0 of every job     | 100 | ≈346 |
| B | 5–16  | ops 1, 2, 3           | 300 | ≈318 |
| C | 17–20 | op 4 of every job     | 100 | ≈350 |

Within stage B the machines split further into sub-pools {5–8}, {9–12},
{13–16}; an operation is eligible on one, two or all three of them.

This structure drives everything:

* Stage-C machines hold ~350 units of work each and **cannot start** until a
  job's first four operations are done (earliest ≈47). So the makespan is
  essentially *"when stage C can start"* + *"how tightly stage C is packed"*.
* Relaxing all upstream sequencing (release dates = the cheapest possible chain
  of ops 0–3) and solving the resulting stage-C partitioning problem gives a
  **lower bound of ≈401**. The mirror-image relaxation on stage A gives ≈398.

So the useful range is roughly 401 ≤ optimum ≤ 412, and the whole game is
played in the last few percent.

## The two exact decompositions

Every *source* of the disjunctive graph is a stage-A operation and every *sink*
is a stage-C operation. Holding the rest of the schedule fixed:

* **Stage C** — each op 4 has a fixed release date `r_i` (completion of op 3)
  and a zero tail. On one machine, ERD order is optimal for the makespan, so
  the subproblem is exactly *"partition 100 operations over 4 unrelated
  machines"*.
* **Stage A** — each op 0 has no predecessor and a fixed tail
  `q_i = tail(op 1) + dur(op 1)`. On one machine, non-increasing tail order
  minimises `max(C_i + q_i)`, so again a pure partitioning problem.

Both are solved by greedy insertion plus descent/annealing. The important part
is not that they polish a finished schedule (a converged solution is already
locally optimal for them) but that they are run **inside** the search: after
every LNS move, stage C is re-solved, so the move is judged against the
bottleneck stage's best response rather than against a stale sequence.

## Search

1. **Construction** — either randomised earliest-completion-time dispatching, or
   a load-balanced machine assignment (annealing on the sum of squared machine
   loads) decoded with a Giffler–Thompson active-schedule builder.
2. **Tabu search** on the disjunctive graph. A move reinserts a *critical*
   operation at some position on some eligible machine. Moves are ranked by an
   O(1) makespan estimate from the current heads and tails; only the best ~24
   are evaluated exactly by a longest-path pass. Tabu attribute is
   (operation, machine-it-came-from), with aspiration on the global best.
3. **Secondary objective.** The makespan landscape is extremely degenerate, so
   ties are broken on the number of near-critical operations
   (`slack ≤ 4`). This alone was worth ~3 units of makespan.
4. **Ruin & recreate (LNS).** Single moves cannot rebalance a stage whose four
   machines all finish at exactly the makespan — any one move that shifts work
   to a sibling immediately makes things worse. So a set of 6–50 operations is
   torn out (at random / from a congested machine / whole jobs) and reinserted
   greedily at the best position, with the end stages re-solved afterwards.
   This is what broke through the 412 plateau.
5. **Parallelism** — ~31 independent workers sharing a global incumbent and an
   elite pool they restart from. Workers differ in how aggressively they use
   LNS.

6. **Annealed LNS.** A third of the workers accept a ruin-and-recreate result
   under a Metropolis rule (one unit of makespan costs 1e5 in the score) on a
   sawtooth cooling schedule with reheats. Accept-only-if-better LNS locked
   hard onto 408 from many independent starts; the annealed variant is what
   walked it down to 404.

Improvements are written to `schedule.txt` as soon as they are found, via a
temp-file rename so the file is always a complete, valid schedule.

## Result

| | makespan |
|---|---|
| published upper bound | 412 |
| required for this task | ≤ 411 |
| **found here** | **404** |
| relaxation-based lower bound | ≈ 401 |

Progress over the run: 448 (first construction) → 417 (tabu search) → 414
(secondary objective) → 412 (LNS) → 408 (end stages re-solved inside the LNS)
→ 404 (annealed LNS, ~1 h on 31 threads).

`schedule.txt` was checked by two independently written verifiers, which
re-derive the makespan from the rows and confirm operation coverage, machine
eligibility, stated durations, job precedence, and machine non-overlap.
