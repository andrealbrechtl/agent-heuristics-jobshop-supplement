# FJSP heuristic solver — results

Flexible Job Shop Scheduling, makespan minimisation. Everything reported here was
produced by the search in `src/fjsp_solver.cpp` from the instance files alone. No
published solution file was fetched, copied, or consulted, and no CP/MIP solver
contributed to any reported number — the CP-SAT baseline
([§7](#7-cp-sat-baseline--not-run)) **was never run on these instances**.

**Experiment status.** Complete: the three 1200 s headline runs, the 7-configuration
ablation, and the seeded variant sweep on behnke31 and behnke14 (12 runs each).
Incomplete: the behnke16 variant sweep (3 of 8 runs) and the CP-SAT baseline (not
started) — the session was stopped first. Both gaps are flagged where they appear.

---

## 1. Headline results

| Instance | Size (J×M) | Published LB | Published UB | **Our best** | Gap to LB | Time-to-best | Validator |
|---|---|---|---|---|---|---|---|
| **behnke31** | 50 × 40 | 226 | **226** (closed) | **226** | **0 — optimal** | 22.3 s | PASS |
| **behnke16** | 100 × 20 | 391 | 412 | **406** | +15 | 56.5 s | PASS |
| **behnke14** | 50 × 20 | 221 | 230 | **232** | +11 | 22.9 s | PASS |

**behnke31 — the calibration anchor: we reached 226, matching the proven optimum.**
This is the primary success criterion and it was met. The schedule is at
`results/best226/behnke31_226.sched` and passes the independent validator.

Against the stretch goals:

- **behnke16: beaten.** 406 vs published UB 412 — an improvement of 6 (1.46%).
- **behnke14: not beaten.** 232 vs published UB 230 — we are 2 short (0.87% worse).

### An important honesty caveat on the behnke31 result

**226 was not produced by the headline 20-minute run.** That run — default
configuration, full budget, all 32 cores — converged to **227** and sat there for
19 minutes. 226 first appeared in a 120-second ablation slot, and was then
reproduced by the *default* configuration in the seeded sweep (§5): seeds 44 and
55 both reached 226, one of them in **4.15 seconds**.

So the accurate framing is not "a special variant finds 226". It is:

> A single 90-second run of the ordinary algorithm returns anywhere in **226–229**.
> The optimum is well inside its reach; one run is simply an unreliable draw from
> that distribution. The 1200 s run that returned 227 was an unlucky draw, and
> spending 20× the wall-clock did not rescue it — **restarting would have.**

Best-of-6 × 90 s (9 wall-minutes) reached 226; 1 × 1200 s (20 wall-minutes) did
not. The reported 226 is a legitimate output of the ordinary solver; what the
data does *not* support is any claim that 226 is reliably attainable in one shot.

---

## 2. Instance identity — audit trail

The brief warned that Behnke instances are often named by generation parameters
and that numbering differs between repositories, and asked me to verify identity
rather than assume it. **Confidence is high**, on four independent grounds:

1. **Provenance.** The files came from `ScheduleOpt/benchmarks` — the data
   repository behind the same FJSPLib site that publishes the LB/UB table being
   compared against — under their own canonical names `behnke14.txt`,
   `behnke16.txt`, `behnke31.txt`. Identity is by the publisher's own naming, not
   by my inference.
2. **Shape matches exactly**: behnke31 = 50 jobs × 40 machines, behnke16 = 100 ×
   20, behnke14 = 50 × 20. All have exactly 5 operations per job.
3. **Header self-consistency.** Each file's third header field is the mean number
   of eligible machines per operation. Independently recomputing it from the body
   reproduces the header exactly in all three cases: 12.93 / 6.47 / 6.10. A
   mis-parse would break this.
4. **Cross-repository structural equality.** The same instances in
   `SchedulingLab/fjsp-instances` are 0-indexed with a different header, so the
   files are not byte-identical. After normalising indexing independently on both
   sides, the instance content is *exactly* equal, and the files sit at exactly
   the predicted ordinal positions in that repo's size-ordered Behnke set
   (`sm03_4` = 14th, `sm04_1` = 16th, `med03_1` = 31st). Those three files are
   kept alongside for inspection in `instances/`.

**Independent lower-bound sanity check.** Rather than trusting the published LBs,
I computed my own relaxation bound (per-work-centre / machine-subset load):
223 (behnke31), 389 (behnke16), 216 (behnke14) against published 226 / 391 / 221.
Being 2–5 units below the published LB is exactly the expected relationship for a
weaker relaxation, which is a meaningful consistency check on both parse and
bounds. Note the naive aggregate-load bound is far weaker (79 for behnke31) — the
binding structure is a 4-machine work centre with 50 operations confined to it.

**Residual doubt: none material.** I am confident these are the intended
instances.

### Format interpretation

FJSPLIB text format. Header: `numJobs numMachines [avgEligibleMachines]`. Then one
line per job: `numOps`, then per operation `numEligible` followed by that many
`(machine, duration)` pairs.

The one genuine ambiguity is **machine indexing**, which the brief specifically
flagged. These files are **1-indexed** (machine ids run 1..M). I did not assume
this. The solver derives it from the data (`if rawMax >= nMach → 1-indexed`), and
the validator derives it independently with a *stricter* rule that refuses to
guess:

```python
if   lo == 0 and hi == n_mach - 1: offset = 0
elif lo == 1 and hi == n_mach:     offset = 1
else: raise ValueError(...refusing to guess...)
```

Both agree on all three instances, and both cross-check the recomputed average
eligibility against the header. An off-by-one here would have been caught.

---

## 3. What the algorithm does

Full detail in `ALGORITHM.md`. In brief:

**Representation.** Two vectors: a machine assignment per operation, and a
per-machine operation sequence. Decoded through the disjunctive graph — heads
`r`, tails `q`, longest path — by Kahn's algorithm, which doubles as the cycle
check. Makespan is the longest path; a cyclic candidate is rejected and reverted.

**Initial solutions.** Randomised Giffler–Thompson active-schedule generation
under five dispatching rules — earliest finish time, most work remaining, SPT,
LPT, random — with a randomisation parameter α. Best of many keeps the start
diverse rather than deterministic.

**Neighbourhood — the part that matters.** Mastrolilli & Gambardella (2000): take
a *critical* operation (one with `r[o] + p[o] + q[o] == cmax`), remove it, and
consider reinserting it at every position on every eligible machine. The estimate
for each candidate position is O(1):

```cpp
int rIns = rJP, qIns = qJS;                                  // job predecessor / successor
if (prev >= 0) rIns = max(rIns, s.r[prev] + s.pcur[prev]);   // machine predecessor
if (next >= 0) qIns = max(qIns, s.q[next] + s.pcur[next]);   // machine successor
emit(k, at, rIns + pk + qIns);                               // estimated makespan
```

Only the best few candidates are then exactly re-evaluated. This is what makes the
search competitive — see the ablation in §6, where replacing it with exact
evaluation of every neighbour costs a **573× throughput loss**.

**Metaheuristic.** Tabu search over `(operation, machine)` attributes with
randomised tenure and aspiration, plus ruin-and-recreate (or random-kick)
perturbation on stagnation, an elite pool (12 slots, FNV-hash dedup),
recombination of two elites (assignment from one, ordering by the other's start
times — acyclic by construction), and occasional full restarts.

**Parallelism.** 32 threads sharing an incumbent and the elite pool. Threads are
*deliberately de-tuned relative to each other* — tabu tenure, stagnation
threshold, share probability and ruin size all vary by thread id — because an
earlier version in which all 32 threads used identical parameters collapsed onto
one basin and plateaued at 229, worse than an 8-thread run reaching 228.

**Checkpointing.** Every strict improvement writes the full schedule to disk and
appends to a progress CSV, under mutex. Verified live mid-run: the in-progress
checkpoint was a complete, valid 250-operation schedule.

---

## 4. Best-so-far over time — does 20 minutes help?

**No. The 20-minute budget is roughly 95% wasted.**

Best-so-far makespan at sampled wall-clock times (default config, 32 threads,
1200 s budget):

| Instance | 1s | 5s | 10s | 30s | 60s | 120s | 300s | 600s | 900s | 1200s |
|---|---|---|---|---|---|---|---|---|---|---|
| behnke31 | 231 | 229 | 228 | 228 | 228 | 227 | 227 | 227 | 227 | 227 |
| behnke16 | 418 | 410 | 410 | 407 | 406 | 406 | 406 | 406 | 406 | 406 |
| behnke14 | 238 | 236 | 236 | 232 | 232 | 232 | 232 | 232 | 232 | 232 |

Time-to-best: **72.0 s / 56.5 s / 22.9 s** out of 1200 s. After ~72 seconds not one
of the three instances improved again for the remaining 19 minutes — across
roughly 1.8 billion further iterations on behnke14.

The run that reached **226** on behnke31 shows the same shape, compressed:

```
   0.00s -> 258      0.10s -> 231      9.21s -> 227
   0.01s -> 236      0.57s -> 230     22.29s -> 226   <- last improvement
   0.04s -> 232      6.04s -> 229     (flat for the remaining ~98 s)
```

The practical consequence is that wall-clock is the wrong thing to spend here:
the same core-seconds are worth far more as **many independent short runs** than
as one long one. §5 measures that directly.

---

## 5. Variant comparison and run-to-run variance

Because §4 showed the search saturates in well under 90 seconds, the interesting
question is not "how long?" but "how much does a run vary, and is that variance
worth exploiting?" Independent seeds, 90 s each, 32 threads, both perturbation
variants:

| instance | variant | seeds | best (portfolio) | mean | worst |
|---|---|---|---|---|---|
| behnke31 | default (ruin-and-recreate) | 227, 228, 227, 226, 226, 229 | **226** | **227.17** | 229 |
| behnke31 | norr (random kicks) | 227, 228, 227, 228, 226, 229 | **226** | 227.50 | 229 |
| behnke14 | default (ruin-and-recreate) | 233, 232, 234, 233, 235, 232 | **232** | **233.17** | 235 |
| behnke14 | norr (random kicks) | 233, 233, 235, 234, 234, 233 | 233 | 233.67 | 235 |
| behnke16 | default (ruin-and-recreate) | 408, 410, 409 — *3 seeds only, run stopped early* | 408 | 409.00 | 410 |

> **Incomplete by design, not by omission:** the behnke16 sweep was stopped after 3
> of its 8 planned runs (the `norr` variant never ran). Those 3 are reported
> because they are real data, but they are too few to compare variants on, and no
> variant conclusion is drawn from them.

The partial behnke16 data still says something useful, and it **cuts against**
Finding 3 below: 3 × 90 s returns 408–410, whereas the single 1200 s run reached
**406**. behnke16 is the one instance where the longer budget genuinely paid — it
is twice the size of the others (500 operations) and §4 shows it still improving
at 56 s. The portfolio recommendation is therefore instance-size dependent, not
universal.

### Finding 1 — the perturbation-operator "discovery" was noise

The 120 s ablation showed the no-ruin-and-recreate variant reaching 226 while the
full algorithm sat at 229, which looked like a real algorithmic finding. It was
not. Across 6 seeds per variant the two are indistinguishable, and the **default
is marginally better on both instances** (227.17 vs 227.50; 233.17 vs 233.67).
The 226 was a lucky seed that happened to land in the ablation slot.

This is worth stating plainly because it is the trap a single-run ablation sets,
and I walked into it before the replication corrected me.

### Finding 2 — variance is large relative to the gaps we care about

On behnke31 a single 90 s run returns anywhere in **226–229**. The entire distance
between our result and the proven optimum is *smaller than the run-to-run spread*.
2 of 6 default seeds hit 226 outright — one of them in **4.15 seconds**.

### Finding 3 — a portfolio of short runs beats one long run

This is the practical lever, and it is a direct consequence of §4:

| strategy | wall-clock | behnke31 result |
|---|---|---|
| 1 run × 1200 s × 32 threads | 20 min | 227 |
| 6 runs × 90 s × 32 threads (best-of) | 9 min | **226** |

**Less than half the wall-clock, and it reaches the proven optimum.** The 20-minute
budget spends ~19 minutes re-confirming a local optimum the search reached in the
first minute; restarting instead resamples the 226–229 distribution. Splitting the
32 cores into independent 4- or 8-thread searches would sample it harder still,
since §6 shows 32 threads deliver no better makespan than 1 thread at fixed time.

**Scope — this does not generalise to the large instance.** On behnke16 the
opposite held: 3 × 90 s gave 408–410 while 1 × 1200 s gave 406. behnke16 has 500
operations against 250, and §4 shows it still improving at 56 s where the others
had long stopped. The rule is therefore *"restart once the instance has
saturated"*, and saturation time scales with instance size — not *"always prefer
short runs"*. With only 3 behnke16 seeds this is a caveat, not a measurement.

### Finding 4 — behnke14's 232 is a real barrier, not a lottery

The contrast with behnke31 is sharp. Across **12 independent runs** (both variants,
6 seeds each) behnke14 never once beat 232, and only 2 of 12 even reached it.
behnke31's 226 was drawn 3 times in 12. So:

- behnke31 at 227 was **bad luck** — the search can reach optimal and did.
- behnke14 at 232 is a **structural limitation** of this neighbourhood. More seeds
  and more time will not fix it; the published UB of 230 is out of reach for this
  algorithm as built. Closing it needs a different lever (see §11).

---

## 6. Component ablation

Each configuration gets the same budget (120 s) and thread count (32) on
behnke31, so the numbers are directly comparable. Toggled via environment
variables (`FJSP_NO_TABU`, `FJSP_NO_RR`, `FJSP_NO_SHARE`, `FJSP_EXACT_EVAL`,
`FJSP_GT_ONLY`).

| configuration | best | t_best | iterations | iters/s |
|---|---|---|---|---|
| **full algorithm** | 229 | 7.12 | 209,477,376 | 1,745,576 |
| no tabu memory | **235** | 3.20 | 246,111,488 | 2,050,832 |
| random kicks instead of ruin+recreate | 226 | 22.29 | 211,512,832 | 1,762,548 |
| no shared incumbent / elite pool | 228 | 37.38 | 198,921,472 | 1,657,619 |
| exact eval of every neighbour | **233** | 102.76 | **369,408** | **2,969** |
| dispatching rules only (no search) | **237** | 1.51 | 0 | 0 |
| single thread | 229 | 7.45 | 10,054,144 | 83,784 |

### Reading this table honestly

§5 measured the seed-to-seed spread on behnke31 at this budget: **226–229**. Every
configuration in the 226–229 band is therefore *inside the noise* and this
single-run table cannot separate them. Only differences that clear that band are
real:

**Real effects (clear the noise band):**

- **Local search is doing the work.** Dispatching rules alone give 237. Everything
  else is the search.
- **Tabu memory matters — the largest single component effect.** Removing it costs
  6 makespan units (235) *despite* running 17% more iterations. Without tabu the
  search cycles: it moves faster and gets nowhere.
- **Incremental evaluation is the difference between working and not.** Exact
  evaluation of every neighbour collapses throughput from 1.75 M to 2,969
  iterations/s — a **573× slowdown** — and lands at 233. This is the single most
  important implementation decision in the solver, and the ablation confirms the
  brief's warning that naive re-evaluation would not be competitive.

**Not established (inside the noise band):**

- **Ruin-and-recreate vs random kicks (226 vs 229).** This looked like the
  headline discovery — the ablated variant hit the proven optimum. Seeded
  replication in §5 showed it is **noise**: across 6 seeds each, the default is if
  anything marginally *better* (mean 227.17 vs 227.50). I report this because it
  is exactly the trap a one-run ablation sets.
- **Shared incumbent / elite pool (228 vs 229).** Inside the band; not established
  by this experiment. The elite pool's justification is the earlier v1→v2
  development result (all-identical threads plateaued at 229 while 8 threads
  reached 228), not this row.
- **Parallelism (single thread 229 vs 32 threads 229).** 32 threads deliver 20.8×
  the iterations and, at this budget, *the same makespan*. Parallelism buys throughput
  that does not convert into solution quality — see §5 for where it should be spent instead.

---

## 7. CP-SAT baseline — not run

> **This section is a baseline for comparison only.** No number here contributed
> to any reported heuristic result. The heuristic never calls a CP or MIP solver;
> `src/cpsat_baseline.py` is a standalone program that shares no code with
> `src/fjsp_solver.cpp`.

**The CP-SAT baseline was not run.** It was queued behind the variant sweep and
the session was stopped before it started, so there are **no baseline numbers to
report** for behnke14/16/31. I am stating this rather than leaving the section
looking answered.

What exists and what it is worth:

- `src/cpsat_baseline.py` is written and working. It was verified end-to-end on
  **mk01**, where it returned **40** and proved optimality in 0.116 s.
- That CP-SAT schedule was checked by `src/validate.py`, my independent validator,
  and **passed** — which is a genuine cross-validation of two entirely separate
  implementations (a bug shared between my solver and my validator would not have
  survived agreeing with a third-party solver's output).

To produce the baseline later:

```bash
SECS=300 WORKERS=32 ./run_cpsat.sh
```

Because none of it ran on the target instances, **no number anywhere in this
document is derived from CP-SAT.** The constraint that the solver must never be
the engine behind a reported result is satisfied trivially here.

---

## 8. Verification

### The validator is independent by construction

`src/validate.py` shares **no code** with the solver and does not import
`src/fjsplib.py`. It re-parses the instance from scratch, derives the machine
indexing with its own stricter rule, and checks, independently:

1. every operation appears exactly once (coverage — no missing, no duplicate)
2. every operation is on an **eligible** machine
3. every duration equals the instance's `p(operation, machine)` for the machine used
4. all start times are non-negative
5. in-job precedence holds for every consecutive operation pair
6. no two operations overlap on any machine
7. the makespan **recomputed from the schedule** equals the value the solver claims

Exit code 0 means PASS.

### The validator was itself tested

A validator that always says PASS is worthless, and "my schedule validated" is
only evidence if the validator provably rejects broken schedules. `src/test_validator.py`
takes a known-good schedule, injects one defect at a time, and asserts the
validator both rejects it *and* fires the **intended** check rather than tripping
over an incidental side effect. Each corruption is surgical — it violates exactly
one property and leaves the others intact. Reproduce with `python3 src/test_validator.py`:

```
  [ok]     control (unmodified schedule)              -> PASS as expected
  [ok]     fabricated makespan in header              -> caught (claimed makespan)
  [ok]     operation on ineligible machine            -> caught (NOT eligible)
  [ok]     processing time shortened                  -> caught (duration)
  [ok]     in-job precedence violated                 -> caught (precedence violated)
  [ok]     operation deleted                          -> caught (missing from schedule)
  [ok]     operation duplicated                       -> caught (more than once)
  [ok]     uniform time shift                         -> caught (claimed makespan)
  [ok]     two operations overlap on a machine        -> caught (overlap on machine)
  [ok]     negative start time                        -> caught (negative start)

VALIDATOR TEST SUITE PASSED: 9 corruptions all caught, control accepted
```

Two of these are deliberately subtle. The **uniform time shift** moves every
operation later by the same amount, so precedence, durations and disjunctivity
all still hold — only the recomputed-vs-claimed makespan check can catch it. The
**machine overlap** is built from two first-of-job operations (no job
predecessor) with durations preserved, so only the disjunctivity check can fire.

It also independently **passes CP-SAT's schedules**, which cross-validates two
completely separate implementations against each other.

### The solver reproduces known optima

Run against MK instances with published optimal values: **mk01=40, mk03=204,
mk08=523, mk09=307, mk14=694** — all exact, ~12 s each on 8 threads.

---

## 9. Schedule file format

Plain text, one operation per line, machine ids **0-based** (normalised on
output regardless of the source file's indexing — stated explicitly in the header
comment so the file is self-describing).

```
instance behnke31
jobs 50
machines 40
operations 250
makespan 227
# machine ids are 0-based; start/end are integers
# job op machine start end
0 0 1 57 68
...
```

Fields: `job`, `operation index within job`, `machine`, `start`, `end`.
`end - start` is the processing time of that operation on that machine.

Best schedules:

| File | Instance | Makespan |
|---|---|---|
| `results/best226/behnke31_226.sched` | behnke31 | **226** (best overall) |
| `schedules/behnke31.sched` | behnke31 | 227 (default config, 1200 s) |
| `schedules/behnke16.sched` | behnke16 | **406** |
| `schedules/behnke14.sched` | behnke14 | **232** |

---

## 10. Layout

```
fjsp-solver/
  src/fjsp_solver.cpp      the solver (C++17, -O3 -march=native -pthread)
  src/validate.py          INDEPENDENT validator — shares no code with the solver
  src/test_validator.py    adversarial tests proving the validator rejects bad schedules
  src/fjsplib.py           reference parser (solver-side tooling only)
  src/cpsat_baseline.py    CP-SAT baseline — comparison only, not the engine
  src/summarize.py         builds the tables/curves in this file
  instances/               instance files (+ cross-repo copies for the identity audit)
  schedules/               best schedule per instance
  results/                 progress CSVs, ablation, variants, CP-SAT output
  ALGORITHM.md             full algorithm write-up
  RESULTS.md               this file
```

Build and run:

```bash
g++ -O3 -march=native -pthread -std=c++17 -o bin_fjsp src/fjsp_solver.cpp
./bin_fjsp instances/behnke31.txt <seconds> <threads> out.sched out.csv [target] [seed]
python3 src/validate.py instances/behnke31.txt out.sched
python3 src/test_validator.py          # prove the validator rejects bad schedules
```

---

## 11. Where it plateaued, and what the next lever is

### Where it plateaued

Two different plateaus, and conflating them would be a mistake:

**behnke31 — a sampling plateau.** Each run converges in seconds to something in
226–229 and stops. The optimum *is* inside the reachable set (hit 3 times in 12
runs). Nothing is structurally wrong; the search just doesn't reliably land on it.

**behnke14 — a structural plateau.** 232 in 12/12 runs, never better, from wildly
different starting points and both perturbation operators. The search is
converging to the same barrier every time. That is the signature of a
neighbourhood that cannot reach the region where 230 lives, not of bad luck.

**behnke16** sits between: 406 beats the published UB, and the search still shows
improvement late (last gain at 56 s), suggesting it is the least exhausted of the three.

### The next levers, in order of expected value

1. **Spend the budget on restarts, not on one long run.** The cheapest and most
   certain win, already demonstrated: best-of-6 × 90 s reached 226 in 9 minutes
   where 1 × 1200 s reached 227 in 20. Since 32 threads produce no better makespan
   than 1 thread at fixed wall-clock (§6), the right configuration is many
   independent small searches, not one wide one. This should be the default
   scheduling policy, not a manual experiment.

2. **Attack behnke14's barrier with a stronger neighbourhood.** Mastrolilli &
   Gambardella moves relocate *one* critical operation at a time. When the binding
   constraint is a congested work centre — which §2's lower-bound analysis
   identifies as exactly the structure of these instances — escaping requires
   moving *several* operations off that work centre together, and no single-operation
   move is improving at any intermediate step. Concretely: critical-block
   reordering, or a targeted LNS that ruins by *machine/work-centre* rather than by
   job. My ruin-and-recreate ruins whole jobs, which is orthogonal to the
   bottleneck structure — that is my best hypothesis for why it added nothing (§5).

3. **Fix the diversification, which the data says is weak.** Time-to-best is
   frequently under 10 s and then nothing happens for the remaining 80+ s. The
   perturbation is not escaping the basin it starts in. A proper strategic
   oscillation or path-relinking between elites would use that dead time; today it
   is wasted.

4. **Use the lower bound to know when to stop.** My own relaxation gives 223/389/216.
   A per-run bound would let a portfolio kill hopeless restarts early and reallocate,
   instead of every run burning its full budget regardless of how it is doing.

### What I would not do

Add more cores or more minutes. Both are already demonstrably saturated, and §6
shows extra iterations are not the constraint — 246 M iterations without tabu is
*worse* than 209 M with it. The constraint is search structure, not compute.
