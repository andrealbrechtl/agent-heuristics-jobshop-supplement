# How each certified FJSPT schedule was produced

Every file in `solutions/` is byte-identical to the output file of the run listed here
(sha256 compared). Logs of these runs are in `logs/record_runs/`. All runs were made on
2026-09-25 on one AMD Ryzen 9 5950X (16 cores / 32 threads, 125 GB), usually with many runs in
parallel. Commands are given relative to the original working directory; `instances/txt/*.txt`
is produced by `solver/to_txt.py` (integer times, scaled x2).

*Time to value* is the elapsed time of the solver process at the end of the cooling run in which
the value first appeared (the solver logs once per cooling run), i.e. an upper bound on the time
at which the schedule was found.

Solver versions:
* **v1** `solver/fjspt_sa.cpp` (binary `fjspt_sa`): first annealer, used for the 150 s screen.
* **v2** `solver/fjspt_sa2.cpp`: the routing-aware annealer described in the paper. The file is
  the final state of the source; the builds `fjspt_sa2`, `fjspt_sa2_new` and `fjspt_sa3` were
  compiled from it at successive stages of the campaign (intermediate source states were not
  kept). `fjspt_sa3` is the frozen version used for the protocol runs (`fp1`).
  See "Reproduction check" below.

| Instance | Makespan | Campaign | Solver | Seed | Budget (s) | Time to value (s) | Command / note |
|---|---|---|---|---|---|---|---|
| MKT01 | 173 | fp1 | SA v2 frozen (src/fjspt_sa2.cpp = bin/fjspt_sa3) | 1008 | 1200 | ≤ 204 | frozen-method protocol, 28 concurrent processes; `./bin/fjspt_sa3 --inst instances/txt/MKT01.txt --seed 1008 --time 1200 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005` |
| MKT02 | 124 | long1 | SA v2 (src/fjspt_sa2.cpp, early build 'fjspt_sa2') | 1 | 900 | ≤ 5 | `./bin/fjspt_sa2 --inst instances/txt/MKT02.txt --seed 1 --time 900 --sec 0 --secw 0.001 --kick 5` |
| MKT03 | 294 | fp1 | SA v2 frozen (src/fjspt_sa2.cpp = bin/fjspt_sa3) | 1002 | 1200 | ≤ 732 | frozen-method protocol, 28 concurrent processes; `./bin/fjspt_sa3 --inst instances/txt/MKT03.txt --seed 1002 --time 1200 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005` |
| MKT04 | 238 | fp1 | SA v2 frozen (src/fjspt_sa2.cpp = bin/fjspt_sa3) | 1004 | 1200 | ≤ 684 | frozen-method protocol, 28 concurrent processes; `./bin/fjspt_sa3 --inst instances/txt/MKT04.txt --seed 1004 --time 1200 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005` |
| MKT05 | 286 | long1 | SA v2 (src/fjspt_sa2.cpp, early build 'fjspt_sa2') | 2 | 900 | ≤ 797 | `./bin/fjspt_sa2 --inst instances/txt/MKT05.txt --seed 2 --time 900 --sec 1 --secw 0.01 --kick 5` |
| MKT06 | 279.5 | long2 | SA v2 (src/fjspt_sa2.cpp, build 'fjspt_sa2') | 23 | 1200 | ≤ 789 | `./bin/fjspt_sa2 --inst instances/txt/MKT06.txt --seed 23 --time 1200 --iters 40000000 --T1 0.2 --kick 5 --T0 4 --sec 1 --secw 0.01` |
| MKT07 | 245 | fp1 | SA v2 frozen (src/fjspt_sa2.cpp = bin/fjspt_sa3) | 1001 | 1200 | ≤ 1200 | frozen-method protocol, 28 concurrent processes; `./bin/fjspt_sa3 --inst instances/txt/MKT07.txt --seed 1001 --time 1200 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005` |
| MKT08 | 696 | fp1 | SA v2 frozen (src/fjspt_sa2.cpp = bin/fjspt_sa3) | 1003 | 1200 | ≤ 654 | frozen-method protocol, 28 concurrent processes; `./bin/fjspt_sa3 --inst instances/txt/MKT08.txt --seed 1003 --time 1200 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005` |
| MKT09 | 598 | camp3 | SA v2 (src/fjspt_sa2.cpp, build 'fjspt_sa2'), island model | shared incumbent of seeds 131-134 | 2700 | ≤ 1270 | 4 MKT09 processes sharing an incumbent file (+28 MKT10 processes on the same machine); `./bin/fjspt_sa2 --inst instances/txt/MKT09.txt --seed None --time 2700 --T1 0.2 --kick 5` |
| MKT10 | 499 | camp5 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp) | 511 | 1900 | ≤ 1839 | 11 independent processes; `./bin/fjspt_sa2_new --inst instances/txt/MKT10.txt --seed 511 --time 1900 --iters 300000000 --T0 4 --T1 0.2 --kick 5 --prr 0.005` |
| mt10c1t | 996 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 44 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10c1t.txt 1 150 runs/screen_cb/mt10c1t_s1.sol 20 1 3000000` |
| mt10cct | 973 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 102 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10cct.txt 1 150 runs/screen_cb/mt10cct_s1.sol 20 1 3000000` |
| mt10xt | 987 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 28 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10xt.txt 1 150 runs/screen_cb/mt10xt_s1.sol 20 1 3000000` |
| mt10xxt | 992 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 105 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10xxt.txt 1 150 runs/screen_cb/mt10xxt_s1.sol 20 1 3000000` |
| mt10xxxt | 990 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 127 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10xxxt.txt 1 150 runs/screen_cb/mt10xxxt_s1.sol 20 1 3000000` |
| mt10xyt | 950 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 41 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10xyt.txt 1 150 runs/screen_cb/mt10xyt_s1.sol 20 1 3000000` |
| mt10xyzt | 922 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 16 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/mt10xyzt.txt 1 150 runs/screen_cb/mt10xyzt_s1.sol 20 1 3000000` |
| setb4c9t | 1005 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 101 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/setb4c9t.txt 1 150 runs/screen_cb/setb4c9t_s1.sol 20 1 3000000` |
| setb4cct | 991 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 734 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/setb4cct.txt --seed 401 --time 1200 --init best/setb4cct.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| setb4xt | 1002 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 51 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/setb4xt.txt 1 150 runs/screen_cb/setb4xt_s1.sol 20 1 3000000` |
| setb4xxt | 993 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 135 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/setb4xxt.txt 1 150 runs/screen_cb/setb4xxt_s1.sol 20 1 3000000` |
| setb4xxxt | 978 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 724 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/setb4xxxt.txt --seed 401 --time 1200 --init best/setb4xxxt.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| setb4xyt | 975 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 143 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/setb4xyt.txt 1 150 runs/screen_cb/setb4xyt_s1.sol 20 1 3000000` |
| setb4xyzt | 968 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 923 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/setb4xyzt.txt --seed 401 --time 1200 --init best/setb4xyzt.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| seti5c12t | 1307 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 150 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/seti5c12t.txt 1 150 runs/screen_cb/seti5c12t_s1.sol 20 1 3000000` |
| seti5cct | 1287 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 805 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/seti5cct.txt --seed 401 --time 1200 --init best/seti5cct.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| seti5xt | 1332 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 11 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/seti5xt.txt 1 150 runs/screen_cb/seti5xt_s1.sol 20 1 3000000` |
| seti5xxt | 1326 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 522 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/seti5xxt.txt --seed 401 --time 1200 --init best/seti5xxt.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| seti5xxxt | 1330 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 267 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/seti5xxxt.txt --seed 401 --time 1200 --init best/seti5xxxt.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| seti5xyt | 1285 | cb1 | SA v2 build 'fjspt_sa2_new' (src/fjspt_sa2.cpp, campaign-time build) | 401 | 1200 | ≤ 544 | warm start from the screen_cb schedule (150 s); `./bin/fjspt_sa2_new --inst instances/txt/seti5xyt.txt --seed 401 --time 1200 --init best/seti5xyt.sol --iters 40000000 --T0 20 --T1 1 --kick 5 --prr 0.005 --pcl 0.05` |
| seti5xyzt | 1274 | screen_cb | SA v1 (src/fjspt_sa.cpp, bin/fjspt_sa) | 1 | 150 | ≤ 14 | 1 process per instance, 21 in parallel; `./bin/fjspt_sa instances/txt/seti5xyzt.txt 1 150 runs/screen_cb/seti5xyzt_s1.sol 20 1 3000000` |

Notes
* `screen_cb`: one 150 s v1 run per Chambers–Barnes instance
  (`fjspt_sa inst seed seconds out T0 T1 iters_per_cooling_run`).
* `cb1`: one 1,200 s v2 run per Chambers–Barnes instance, warm-started from the `screen_cb`
  schedule (`--init`), with the "hand-over" move still enabled (`--pcl 0.05`). Budget per
  instance therefore 150 s + 1,200 s. The `screen_cb` logs of all 21 instances are included.
* `MKT09` 598: island-model campaign `camp3` (4 MKT09 processes sharing an incumbent file via
  `--share`, alongside 28 MKT10 processes); seed 131 reached 598 by 1,270 s.
* `MKT10` 499: campaign `camp5`, 11 independent 1,900 s processes; seed 511 reached 499 at the
  end of its first cooling run of 3·10^8 moves (1,839 s, one thread). Total compute spent on
  MKT10 up to that point (all campaigns): about 40 thread-hours.
* `fp1`: frozen-method protocol (`protocol_runs/`), 10 seeds × 1,200 s per MKT instance,
  28 concurrent processes. The five MKT records from `fp1` are ordinary protocol runs.

## Reproduction check (2026-10-11)

The MKT10 record is reproducible bit for bit from source. The 499 was found at the end of the
first cooling run, which is bounded by iterations, not by time, so it is deterministic per seed:

```bash
g++ -O3 -march=native -std=c++17 -o fjspt_sa solver/fjspt_sa2.cpp
python solver/to_txt.py            # from 3_fjspt/: writes instances/txt/*.txt
./fjspt_sa --inst instances/txt/MKT10.txt --seed 511 --time 1900 --out MKT10_s511.sol \
           --iters 300000000 --T0 4 --T1 0.2 --kick 5 --prr 0.005
# log: "[MKT10 s511] run 1 t=1061s runBest=499.0 best=499.0 it=300000000"
cmp MKT10_s511.sol solutions/MKT10.sol   # identical
```

A fresh build of `solver/fjspt_sa2.cpp` and the original builds `fjspt_sa2_new` and `fjspt_sa3`
all wrote a file byte-identical to `solutions/MKT10.sol` after 1,061–1,119 s (three runs in
parallel on the Ryzen 9 5950X). The original campaign run took 1,839 s because 11 processes
shared the machine. Other records reached only after a time limit, or through a shared incumbent
(MKT09), are not bit-reproducible in this way.
