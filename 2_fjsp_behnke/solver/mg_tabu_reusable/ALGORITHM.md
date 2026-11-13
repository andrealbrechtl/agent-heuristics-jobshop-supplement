# Algorithm

Reference notes for the solver in `src/fjsp_solver.cpp`. `RESULTS.md` has the numbers.

## Representation

The standard two-vector encoding:

1. **Machine assignment** — `mach[o]`, one eligible machine per operation.
2. **Operation sequence** — `seq[m]`, the order in which machine `m` runs the operations assigned to it.

Together these define a disjunctive graph: nodes are operations, arcs are the
job chains (fixed) plus the machine orders (chosen). A solution is feasible iff
that graph is acyclic.

## Decoding

`Searcher::evaluate` runs Kahn's algorithm over the graph, computing in one
`O(V+E)` pass:

- `r[o]` — the **head**, the longest path into `o`, i.e. its earliest start;
- `q[o]` — the **tail**, the longest path out of `o` to the end;
- `cmax = max(r[o] + p[o])` — the makespan.

If the topological order does not cover every node the graph has a cycle and the
candidate is rejected. Starting every operation at its head yields a
**semi-active** schedule. The constructor below produces genuinely **active**
schedules, and the insertion neighbourhood spans the space of active schedules
(any left-shift of an operation into an earlier idle gap is exactly an insertion
move), so the search is not confined to the semi-active subspace.

An operation `o` is **critical** when `r[o] + p[o] + q[o] == cmax`; only critical
operations can be moved to reduce the makespan, which is what keeps the
neighbourhood small.

## Initial solutions

`Searcher::construct` is a randomised **Giffler–Thompson** active schedule
generator. At each step it finds the minimum earliest-completion-time `C*` over
all (schedulable operation, eligible machine) pairs, fixes that machine `m*`,
builds the conflict set of operations that could start on `m*` strictly before
`C*`, and picks one by a priority rule:

| rule | meaning |
|---|---|
| EFT | earliest finishing time |
| MWR | most work remaining in the job |
| SPT | shortest processing time on `m*` |
| LPT | longest processing time on `m*` |
| random | uniform |

Machine choice is softened by an `alpha` parameter that admits near-minimal
completion times, and ties are broken randomly, so repeated calls give different
schedules. Each thread keeps the best of twelve.

## Neighbourhood — Mastrolilli & Gambardella (2000)

For a critical operation `v`: remove it from its machine and reinsert it at **any
position of any eligible machine**. This changes assignment and sequence at once,
which is what makes it strong on FJSP.

The cost of this neighbourhood is what usually kills naive implementations. With
`rJP` the completion of `v`'s job predecessor and `qJS` the tail of its job
successor, inserting `v` between `u` and `w` on machine `k` gives

```
r'(v) = max(rJP, r[u] + p[u])
q'(v) = max(qJS, q[w] + p[w])
est   = r'(v) + p_k(v) + q'(v)
```

which is an **O(1)** estimate of the resulting makespan, read straight off the
heads and tails already computed for the current solution. `scanInsertions`
sweeps the whole neighbourhood this way. Only the move actually chosen gets a
full `O(V+E)` re-decode, and that re-decode doubles as the cycle check — if the
move made the graph cyclic it is reverted and the next-best candidate is tried.

So the per-iteration cost is *one* exact evaluation plus thousands of O(1)
estimates, rather than thousands of exact evaluations. `FJSP_EXACT_EVAL=1`
switches the estimate off and scores every neighbour exactly; `RESULTS.md`
reports what that costs.

## Metaheuristic

**Tabu search.** After moving `v` off machine `k`, the pair `(v, k)` is tabu for
`tenure` iterations, so `v` cannot immediately go back. Tenure is randomised per
thread. Aspiration: a tabu move is allowed if its estimate beats the global
incumbent.

**Ruin & recreate.** After `stagLimit` non-improving iterations, all operations
of a few random jobs are removed and greedily reinserted at their best estimated
position (sometimes onto a forced random machine, to push the search sideways).
The tabu list is cleared.

**Elite pool and recombination.** A pool of up to 12 structurally distinct good
solutions is kept globally, deduplicated by a hash of the assignment vector.
Restarts draw from a *random* elite rather than always the single incumbent.
Occasionally two elites are recombined: the child takes the machine assignment of
one parent and orders each machine's queue by the other parent's start times.
Because the second parent's start times are a global topological order, the child
is acyclic by construction.

**Parallelism.** 32 independent searchers share the incumbent and the elite pool.
Crucially they do *not* share parameters: tabu tenure, stagnation patience,
perturbation strength and restart-source probability all vary by thread index.
Identical threads just buy 32 correlated searches — this was a measurable
mistake in an earlier version, see the development note in `RESULTS.md`.

## Checkpointing

Every time the global incumbent improves, the schedule is rewritten to
`schedules/<instance>.sched` and a `(seconds, makespan)` row is appended to
`results/<instance>_progress.csv`, both under the shared mutex. A run can be
killed at any point without losing the best solution found.

## Ablation switches

Environment variables, all defaulting to off:

| variable | effect |
|---|---|
| `FJSP_NO_TABU=1` | no tabu memory |
| `FJSP_NO_RR=1` | random kicks instead of ruin & recreate |
| `FJSP_NO_SHARE=1` | no shared incumbent or elite pool |
| `FJSP_EXACT_EVAL=1` | exact evaluation of every neighbour |
| `FJSP_GT_ONLY=1` | dispatching-rule construction only, no local search |
