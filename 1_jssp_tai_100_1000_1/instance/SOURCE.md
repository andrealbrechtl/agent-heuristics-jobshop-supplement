# Source of `tai_100_1000_1.txt`

Retrieved and compared 2026-10-11.

* **Instance family:** G. Da Col, E. C. Teppan (2022). Industrial-size job shop scheduling with
  constraint programming. *Operations Research Perspectives* 9:100249.
  doi:10.1016/j.orp.2022.100249. Taillard-style generator: É. Taillard (1993). Benchmarks for
  basic scheduling problems. *European Journal of Operational Research* 64(2):278–285.
* **File used (JSPLib, ScheduleOpt):**
  https://github.com/ScheduleOpt/benchmarks, commit `f89e5b007c90a1c1a70fa48a28d657a72da05665`,
  `jobshop/instances/text/DacolTeppan2022/standard/tai_100_1000_1.zip`
  (raw: https://raw.githubusercontent.com/ScheduleOpt/benchmarks/f89e5b007c90a1c1a70fa48a28d657a72da05665/jobshop/instances/text/DacolTeppan2022/standard/tai_100_1000_1.zip).
  Our file is byte-identical to the file in that zip.
* **sha256:** `20f56f835c14d6093bed034294441812001239bdf5db31dc773d57121344eac4` (778,241 bytes).
* **Tracked bounds:** same commit, `jobshop/solutions/bks.json`; https://scheduleopt.github.io/benchmarks/jsplib/
  (LB 522,298, UB 533,080, OptalCP, 180 s).
* **Format:** first line `n m` (100 1000); then one line per job with 1,000 pairs
  `machine duration` in routing order; machines 0-based.
