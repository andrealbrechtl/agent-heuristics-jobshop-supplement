
# The 12-worker run that produced 525,764

These are the logs of the run on this host, 2026-08-06, that rebuilt `csp3.c` and went past
the 526,000 the original agent had called a wall. They are the provenance for
`../best/tai_100_1000_1.525764.txt`.

Twelve workers, one core each, 1,800 s each. Six named `d_2xx` started cold at T=531,000;
six named `w527_11x` started at T=527,000 with the seeds the original run used.

    ./csp3 tai_100_1000_1.txt /tmp/csp/f/d_20N.txt     531000 1800 20N 8 40 none 1000 200000
    ./csp3 tai_100_1000_1.txt /tmp/csp/f/w527_11N.txt  527000 1800 11N 8 40 none 1000 200000

The instance had its leading `#` line stripped first — this parser does not skip comments:

    grep -v '^#' tests/tai_100_1000_1.txt > tai_100_1000_1.txt

## Result

| worker | started at | finished at | levels closed | reached 527,000 | reached 526,000 | last level tried | shakes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| **d_201** | 531,000 | **525,764** | 7 | 206 s | — | 524,764 | 417 |
| w527_112 | 527,000 | 525,801 | 3 | 89 s | 270 s | 524,801 | 399 |
| w527_111 | 527,000 | 525,871 | 3 | 93 s | 454 s | 524,871 | 510 |
| w527_116 | 527,000 | 525,901 | 3 | 98 s | 590 s | 524,901 | 374 |
| w527_114 | 527,000 | 525,932 | 3 | 93 s | — | 524,932 | 496 |
| d_202 | 531,000 | 526,000 | 6 | 213 s | 815 s | 525,000 | 503 |
| d_203 | 531,000 | 526,000 | 6 | 226 s | 424 s | 525,000 | 430 |
| d_204 | 531,000 | 526,000 | 6 | 269 s | 432 s | 525,000 | 676 |
| d_205 | 531,000 | 526,000 | 6 | 253 s | 590 s | 525,000 | 742 |
| d_206 | 531,000 | 526,000 | 6 | 243 s | 673 s | 525,000 | 731 |
| w527_113 | 527,000 | 526,000 | 2 | 111 s | 576 s | 525,000 | 505 |
| w527_115 | 527,000 | 526,000 | 2 | 132 s | 286 s | 525,000 | 624 |

Twelve of twelve finished below 527,000. Five finished below 526,000. Not one of them was
still at the level the original run treated as a wall.

The two dashes are not failures — those workers skipped the round 526,000 level, see below.

## How to read a log

Three kinds of line:

    N=100 M=1000 LB=522298 T0=531000 noise=8 stallit=40000000 step=1000
      T=526000 it=13M conf=112 amt=20543 shk=54 (299s)
    T=526000: conf 451->133 amt 184766->25546 it=68157440 (457.5s)
      *** FEASIBLE 526000

- The **indented `T=` lines** are progress inside one level, every million moves. `conf` is
  how many machine overlaps are left, `amt` is how much time they overlap by in total, `shk`
  is how many times the search has been shaken loose, and the seconds are since the worker
  started.
- The **flush `T=...:` line** closes a level and shows what it went from and to.
- **`*** FEASIBLE n`** means a full valid schedule at makespan `n` was written to disk.
- `FINAL best=` is the last line.

Because the makespan can never exceed `T` in this formulation, `conf 0` *is* the answer —
there is no separate "and now check the makespan" step.

## Where the odd numbers come from

The levels are not all round. `d_201` went 531,000 → 530,000 → 529,000 → 528,000 → 527,000 →
**526,764** → 525,764 → 524,764.

Two rules in `csp3.c` set the next target:

    T = bestmk - step;                  // after a success
    T = bestmk - (1 + rndint(step));    // after a stall, a random 1..1000 below the best

`d_201` stalled at 526,000 after 68 M moves, so it backed off and drew 236 — giving 526,764,
which it then closed in 128 s. From there each further step of 1,000 keeps the 764. So a
worker that finishes on `...764` or `...801` is one that stalled once and re-drew; it is not
a different method or a broken run.

## No floor was reached

Every worker ran out of clock, not out of progress. `d_201` was still searching at T=524,764
when its 1,800 s expired — conflicts cycling between single digits and the mid-hundreds,
shakes firing, which is exactly what its 525,764 level looked like shortly before it closed.

Nothing here shows where this solver stops. It shows where it was when we stopped it.
