
# Reconstructing the 527,000 solver

The container that produced 527,000 is gone. The source is not gone: every file the agent
wrote went through a `Write` or `Edit` tool call, and every later patch went through a
`python3 - <<'PY'` heredoc that does string replacement on the file. All of it is recorded
verbatim in the session trajectory, so the solver can be rebuilt exactly.

Source: the agent trajectory of autonomous session 1 (Fig. 3a of the paper). The trajectory is
not part of the supplement.
Step numbers below are the `step_id` values of the trajectory.

## How it was recovered

1. `csp3.c` was written whole at step 56 (13,766 chars).
2. Exactly one later patch touched it — step 62, a Python heredoc adding the `kick()`
   escape, the periodic shake, and the `shakeit` argument. No other step writes `csp3.c`
   (checked by searching every Bash step for `open('csp3.c','w')`).
3. So the winning binary is **step 56 + step 62**, and nothing else.
4. `csp4.c` is a separate branch: step 71 reads the patched `csp3.c` and writes `csp4.c`,
   adding gap reinsertion. csp4 did *not* produce the winner — the 527,000 file came from
   `/tmp/f3/w527_111.txt`, and `f3` is the csp3 fleet.

Patch application was verified rather than assumed: Python `str.replace` fails silently on a
pattern miss, so the rebuilt file was checked for `static void kick`, `shakeit`, `shk=%lld`
and `kick(4, 2000)` before trusting the build.

## Files here

| file | what it is |
| --- | --- |
| `csp3.c` | the winner — step 56 + step 62, rebuilt and confirmed to compile and run |
| `csp4.c` | step 71 branch; adds gap reinsertion for the `conf<=8` endgame. Did not win. |
| `csp_v1_prototype.c` | the first corridor version (step 36). Slower per step — re-sorts every touched machine list, ~1.2k steps/s — but it is where the first corridor schedule (540,999, started at T = 545,000) was found, so it is the clearest statement of the idea. |
| `verify.py` | the agent's independent feasibility checker |

## Build

    gcc -O2 -o csp3 csp3.c

## Run

    JSPV=1 ./csp3 <instance> <out.txt> <T0> <maxsec> <seed> <noise> <stall_Mit> <seedfile|none> <step> <shakeit>

Arguments in order: instance path, output path, starting target T, wall seconds, RNG seed,
noise (default 8), stall limit in millions of iterations, a warm-start schedule or `none`,
the amount T drops by on each success (1000), and iterations-without-improvement before a
shake (200000).

The exact command whose output became the 527,000 schedule:

    ./csp3 tai_100_1000_1.txt /tmp/f3/w527_111.txt 527000 3300 111 8 40 none 1000 200000

That was one of 16 `csp3` workers, which ran next to 12 `csp4` workers in a container with a quota of 4 CPUs, so each process had about a tenth of a core.

**The instance must have its canary comment line stripped** — the copy under `tests/` carries
a leading `#` line and this parser does not skip comments:

    grep -v '^#' tests/tai_100_1000_1.txt > tai_100_1000_1.txt

## Measured on this host, after reconstruction

Single core, starting at T=535,000 and stepping down:

| T reached feasible | elapsed |
| --- | --- |
| 535,000 | 1.7 s |
| 534,000 | 2.2 s |
| 533,000 | 7.4 s |
| 532,000 | 12.9 s |
| 531,000 | 18.5 s |

A 12-worker fleet (6 starting at T=527,000 with the original seeds 111–116, 6 descending
from T=531,000) reproduced **527,000** in about 8 minutes of wall time, and then went past
it to **526,000** — see below.

## Where it stops — the wall was cores, not the method

The agent's fleet walled at 526,000: 28 processes (16 `csp3`, 12 `csp4`), ~1,500 s each, conflict count stuck between
31 and 286 and never closing. `csp4.c`'s gap reinsertion was written specifically for that
endgame and did not break through either. It read like a structural plateau.

It was not. The same source, rebuilt and given a core per worker for 1,800 s, put **all 12
of 12** independent seeds below 527,000 — five of them below 526,000, best **525,764**. Seed
112 reached 526,000 at 181.5 s from cold. The saved schedule is `d_201`'s, at
`../solutions/tai_100_1000_1_525764.txt`, independently verified feasible.

All twelve logs are in `../logs/fleet_protocol2/`. Six workers (`d_201`…`d_206`, seeds 201–206) started at T = 531,000 and six (seeds 111–116) at T = 527,000; the logs do not record the host.

The difference is throughput per worker. The agent's 28 processes shared 4 CPUs — about a
tenth of a core each (measured by replaying worker states) — while these 12 had a core apiece, so each worker iterates about
ten times faster and reaches its escape mechanisms proportionally sooner. Nothing about
the solver changed.

So do not read a stalled conflict count as a plateau on this solver. Read it as "this worker
does not have enough iterations". `d_201` was still searching at T=524,764 when the clock
stopped, conflicts cycling normally and shakes firing, which is what its 525,764 phase
looked like shortly before it closed. **No level has yet been found where this solver stops
making progress rather than simply running out of time**, so the floor is unmeasured.

If the descent does eventually genuinely stall, the untried candidates are a different
escape mechanism, a non-uniform corridor width (currently every op of a job gets the same
slack `T - L_j`), or the cumulative-delay formulation of session 2, which got to 528,142 by a
different route entirely.
