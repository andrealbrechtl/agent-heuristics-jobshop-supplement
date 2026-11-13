# Source of `behnke14.fjs`, `behnke16.fjs`, `behnke31.fjs`

Retrieved and compared 2026-10-11.

* **Instances:** D. Behnke, M. J. Geiger (2012). Test instances for the flexible job shop
  scheduling problem with work centers. Research report RR-12-01-01, Helmut-Schmidt-Universität,
  Hamburg. ISSN 2192-0826. openHSU: https://openhsu.ub.hsu-hh.de/entities/publication/436
  (doi:10.24405/436). In the report's Table 20, instance 14 = Sm03.4, 16 = Sm04.1, 31 = Med03.1.
* **Original distribution:** instance zip from openHSU
  (https://openhsu.ub.hsu-hh.de/bitstreams/4ed8d5b1-2546-4a30-8f3a-a8f3732ffbbd/download,
  sha256 `0a9fa1367ff99a37a5f6adc0018728bf503abc6bdb3cc372eac0983f409ee43a`), files
  `FJSSPinstances/0_BehnkeGeiger/Behnke14.fjs`, `Behnke16.fjs`, `Behnke31.fjs`. Our files are
  identical after removing the CRLF line endings.
* **Files used (FJSPLib, ScheduleOpt):** https://github.com/ScheduleOpt/benchmarks, commit
  `f89e5b007c90a1c1a70fa48a28d657a72da05665`,
  `flexible-jobshop/instances/fjsp/BehnkeGeiger2012/behnke{14,16,31}.txt`; byte-identical to ours.
  (Not `fajsp/BehnkeGeiger2012/`: same names, assembly job shop, different data.)
* **Also identical** (machine ids +1): SchedulingLab/fjsp-instances, commit
  `ac4c3402312bfbeafcf4472d78be567d4e6b46ab`, `behnke/sm03_4.txt`, `sm04_1.txt`, `med03_1.txt`.
* **Tracked bounds:** ScheduleOpt commit above, `flexible-jobshop/solutions/bks.json`;
  https://scheduleopt.github.io/benchmarks/fjsplib/

| File | Size | sha256 |
|---|---|---|
| `behnke14.fjs` | 50 jobs × 20 machines, 250 ops | `c10f9775771c5c677d216ebe0b7fc6a4b97e132ed5301798336895b02503fa68` |
| `behnke16.fjs` | 100 jobs × 20 machines, 500 ops | `04d050b57b8604a28e9889d4613824032ef1ee93bb1ca172d16df2d7ada006d8` |
| `behnke31.fjs` | 50 jobs × 40 machines, 250 ops | `7e3b5f419b980997322254274e777e3a539439eb1cebc5d7d83aa282e80aba1f` |

Format: standard `.fjs` — header `jobs machines avg_eligible`; per job: number of operations,
then per operation `k` followed by `k` pairs `machine time` (machines 1-based).
