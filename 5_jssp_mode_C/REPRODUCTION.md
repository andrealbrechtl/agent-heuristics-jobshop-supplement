# Bit-for-bit reproduction of the Protocol C runs

The csp search is deterministic for a given seed: the RNG is seeded from the command line only, and
the wall-clock limit only decides when the run stops. A run that reaches zero overlap before its time
limit therefore reproduces exactly: same iteration count, same schedule.

On 2026-10-20 the four key runs were rebuilt from the sources in `solver/` and re-run with the files in
`domains/`, on the same VM (4 × ARM Neoverse-N1, gcc 13.3, `-O3 -march=native`), four runs in parallel:

| run | original (session) | reproduction | schedule |
|---|---|---|---|
| k=9, v1, seed 13, T = LB = 528,097 | 104,107,113 iterations, 1,059.7 s | 104,107,113 iterations, 1,031.2 s | byte-identical (sha256 `8c30c17c32e6d138…`) |
| k=4, v2, seed 103, T = LB = 527,101 | 160,780,724 iterations, 1,693.9 s | 160,780,724 iterations, 1,682.6 s | byte-identical (sha256 `f479e2af03504176…`) |
| k=8, v3, seed 5, KN=2 KW=1000, T = 527,428 | 125,010,642 iterations, 1,421.5 s | 125,010,642 iterations, 1,383.4 s | byte-identical (sha256 `3b2375193d9171c7…`) |
| k=2, v1 without domains, seed 1, T = LB = 530,375 | 4,517,119 iterations, 46.7 s | 4,517,119 iterations, 44.8 s | byte-identical (sha256 `138fd540411b933f…`) |

Commands (from this folder):

```bash
for v in 1 2 3; do gcc -O3 -march=native -o csp_protC_v$v solver/csp_protC_v$v.c -lm; done
DOMS=domains/dom_k9_T528097_cpsat.txt        JSPV=1 ./csp_protC_v1 instances/tai_100_1000_9.txt k9.txt 528097 1800 13  8 100000 none 1000 200000
DOMS=domains/dom_k4_T527101_cpsat.txt        JSPV=1 ./csp_protC_v2 instances/tai_100_1000_4.txt k4.txt 527101 3600 103 8 100000 none 1000 200000
KN=2 KW=1000 DOMS=domains/dom_k8_T527428_propagator.txt JSPV=1 ./csp_protC_v3 instances/tai_100_1000_8.txt k8.txt 527428 1800 5 8 100000 none 1000 200000
JSPV=1 ./csp_protC_v1 instances/tai_100_1000_2.txt k2.txt 530375 600 1 8 1000 none 1000 200000
python solver/compact.py instances/tai_100_1000_8.txt k8.txt k8_compacted.txt     # 527,428 -> 527,280
```

The schedule is written when zero overlap is reached (the `*** FEASIBLE` line of the log); after that
the run keeps searching at lower targets until its time limit, which does not change the file for
these runs. Because the search uses integer arithmetic only, the same iteration counts are expected on
other machines and compilers, but this was tested only on the VM above; the times depend on the CPU.

