# Source of the FJSPT instances

Retrieved and compared 2026-10-11 (`../instance_sources/fetch_and_compare.sh` repeats this).

## Literature

* **Instance family (job data + layouts, 2 AGVs):** S. M. Homayouni, D. B. M. M. Fontes (2021).
  Production and transport scheduling in flexible job shop manufacturing systems.
  *Journal of Global Optimization* 79(2):463–502. doi:10.1007/s10898-021-00992-6.
* **Job data of MKT01–10:** P. Brandimarte (1993). Routing and scheduling in a flexible job shop by
  tabu search. *Annals of Operations Research* 41(3):157–183. doi:10.1007/BF02023073.
* **Job data of the 21 Chambers–Barnes instances:** J. B. Chambers, J. W. Barnes (1996). Flexible
  job shop scheduling by tabu search. Technical report ORP96-09, Graduate Program in Operations
  Research and Industrial Engineering, The University of Texas at Austin.

## Files used

| Source | Location | Version |
|---|---|---|
| FAST project page (distribution by Homayouni & Fontes) | https://fastmanufacturingproject.wordpress.com/2019/04/11/fjspt-instances/ | accessed 2026-10-11 |
| travel-time layouts (4–18 machines) | https://fastmanufacturingproject.wordpress.com/wp-content/uploads/2020/05/4to18machines_layouts-1.pdf | sha256 `9a84e3197c0e76ce6ecc8e2d7325c8970b9850531f2505103d51de7aec1e8bc9` |
| Brandimarte job sets | https://fastmanufacturingproject.wordpress.com/wp-content/uploads/2020/05/brandimarte1993-2.pdf | sha256 `25e109593bc71cb730f5b802277fe346943f8fa4007e51fa85ed583e0d1ec469` |
| Chambers–Barnes job sets | https://fastmanufacturingproject.wordpress.com/wp-content/uploads/2020/05/chambersbarnes1996-1.pdf | sha256 `8305e84641aec6099e1a44b39e57f87ee11f55136f2c3d39561715c1c50a3b77` |
| `fjsp-instances` (SchedulingLab) | https://github.com/SchedulingLab/fjsp-instances | commit `ac4c3402312bfbeafcf4472d78be567d4e6b46ab` |
| `benchmarks` (ScheduleOpt, FJSPLib) | https://github.com/ScheduleOpt/benchmarks | commit `f89e5b007c90a1c1a70fa48a28d657a72da05665` |
| `FJSP_AGV_MILP` (Y. Wang) | https://github.com/wangyuting9836/FJSP_AGV_MILP | commit `48b3f2130063e606ecddced4c3f46cd1f93aa061` |

Each JSON file = job data of the FJSP instance below (machines 1-based, original time units)
+ the Homayouni–Fontes layout with the same number of machines (`travel[0]` = L/U station).
Job data identical in every listed source; travel matrix identical to the FAST layout PDF (and,
for MKT, to FJSP_AGV_MILP on the machines used).

| File | Size | FJSP job data | Travel matrix (FAST PDF) | Upstream job-data files compared | sha256 of our file |
|---|---|---|---|---|---|
| `MKT01` | 10×6, 55 ops | Brandimarte (1993) MK01 | 6-machine layout | `fjsp-instances/brandimarte/mk01.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk01.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk1.dat` | `584be865f819067cca1571c9db7be4414ffee1ba8939edf44ff7b14f9e69425e` |
| `MKT02` | 10×6, 58 ops | Brandimarte (1993) MK02 | 6-machine layout | `fjsp-instances/brandimarte/mk02.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk02.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk2.dat` | `863e07099bf1d6f6b2b27e399ba4a2e5e03f3dd86cf0c623b577423e2294785f` |
| `MKT03` | 15×8, 150 ops | Brandimarte (1993) MK03 | 8-machine layout | `fjsp-instances/brandimarte/mk03.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk03.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk3.dat` | `befab6f6c8d0f1794e6610f8f26e4e695c84806052d63f7631030bb0e3f2c93e` |
| `MKT04` | 15×8, 90 ops | Brandimarte (1993) MK04 | 8-machine layout | `fjsp-instances/brandimarte/mk04.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk04.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk4.dat` | `eff351759858fd486a8ecd022d9b4527639a15ca25f1c5252d2eb0e9caf44827` |
| `MKT05` | 15×4, 106 ops | Brandimarte (1993) MK05 | 4-machine layout | `fjsp-instances/brandimarte/mk05.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk05.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk5.dat` | `84f7a72ef609f13d329ac62cdf3da3d09e0c8ef2cf1b746eacf27a4e51223ed8` |
| `MKT06` | 10×15, 150 ops | Brandimarte (1993) MK06 | 15-machine layout | `fjsp-instances/brandimarte/mk06.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk06.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk6.dat` | `045ec2b2b1788ff21a4b9bd4c3eb49c03363aa71857d9e247ba8347785213e8c` |
| `MKT07` | 20×5, 100 ops | Brandimarte (1993) MK07 | 5-machine layout | `fjsp-instances/brandimarte/mk07.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk07.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk7.dat` | `398136188179b252f75905cc2afabcbfce56eb10bf0f534d6bc20e8d87b2ed9e` |
| `MKT08` | 20×10, 225 ops | Brandimarte (1993) MK08 | 10-machine layout | `fjsp-instances/brandimarte/mk08.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk08.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk8.dat` | `47a457154937883b2a89c9cbd689bef77bc042b41406783f314cf3c61b0f29c9` |
| `MKT09` | 20×10, 240 ops | Brandimarte (1993) MK09 | 10-machine layout | `fjsp-instances/brandimarte/mk09.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk09.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk9.dat` | `c4dbea2607a0f9cf700337681d73f96d995a5809907f341aa4064cab2633ac32` |
| `MKT10` | 20×15, 240 ops | Brandimarte (1993) MK10 | 15-machine layout | `fjsp-instances/brandimarte/mk10.txt`; `benchmarks/flexible-jobshop/instances/fjsp/Brandimarte1993/mk10.txt`; `FJSP_AGV_MILP/benchmark/MK/Mk10.dat` | `d67104b72fa8a9bcd773060cb4ecf167eecc38cdadbfdd405996ca105defb485` |
| `mt10c1t` | 10×11, 100 ops | Chambers & Barnes (1996) mt10c1 | 11-machine layout | `fjsp-instances/barnes/mt10c1.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10c1` | `fd021b6fc2efc348e4a7e17f4a21c4bf21ff622448c1c96c7e8e1f3390d127c4` |
| `mt10cct` | 10×12, 100 ops | Chambers & Barnes (1996) mt10cc | 12-machine layout | `fjsp-instances/barnes/mt10cc.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10cc` | `b6507dd69cc37d5bbed2f38762e56adbfa107400a885bf5d03e5eb3fcd7720ba` |
| `mt10xt` | 10×11, 100 ops | Chambers & Barnes (1996) mt10x | 11-machine layout | `fjsp-instances/barnes/mt10x.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10x` | `085dba6cfdd74b06bbdb554219b7f1a237efec52ee8cab01dc5d7f30813cc1e3` |
| `mt10xxt` | 10×12, 100 ops | Chambers & Barnes (1996) mt10xx | 12-machine layout | `fjsp-instances/barnes/mt10xx.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10xx` | `b4348b8383b47290fcffbfd4e6132fba54eb28fac3eb4fbcfb31b34248755e43` |
| `mt10xxxt` | 10×13, 100 ops | Chambers & Barnes (1996) mt10xxx | 13-machine layout | `fjsp-instances/barnes/mt10xxx.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10xxx` | `3aac35370656f5c93637f24cd9f52a0ee5d8548211bcd6880d088ef60d5f6b1a` |
| `mt10xyt` | 10×12, 100 ops | Chambers & Barnes (1996) mt10xy | 12-machine layout | `fjsp-instances/barnes/mt10xy.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10xy` | `e218353ee961f14566c9ad5cfdcf5b2b5228e8a11f73b3bb7ae2f25a3b54e050` |
| `mt10xyzt` | 10×13, 100 ops | Chambers & Barnes (1996) mt10xyz | 13-machine layout | `fjsp-instances/barnes/mt10xyz.txt`; FAST `chambersbarnes1996-1.pdf`, block `mt10xyz` | `85b94d00be8ba698cef01f166dc9913db31617a8d1908a3ed6fbd0df496952cf` |
| `setb4c9t` | 15×11, 150 ops | Chambers & Barnes (1996) setb4c9 | 11-machine layout | `fjsp-instances/barnes/setb4c9.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4c9` | `2e9e099492348bfde693240d03c80f607d3a46b9134daa258690dee03d8c48d4` |
| `setb4cct` | 15×12, 150 ops | Chambers & Barnes (1996) setb4cc | 12-machine layout | `fjsp-instances/barnes/setb4cc.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4cc` | `80c03a79ff68f16cc5e0527488aa2d0f70177672085ba51b8a37cdfb37a3c1d5` |
| `setb4xt` | 15×11, 150 ops | Chambers & Barnes (1996) setb4x | 11-machine layout | `fjsp-instances/barnes/setb4x.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4x` | `6b86f74663d028dc30e272db5d789838c2a0fa7a579da8d5c61cbacaba9566ae` |
| `setb4xxt` | 15×12, 150 ops | Chambers & Barnes (1996) setb4xx | 12-machine layout | `fjsp-instances/barnes/setb4xx.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4xx` | `2227dab3f29d6285085e00cfda8976fa480a63e69428a713b00600cfcc61ecb2` |
| `setb4xxxt` | 15×13, 150 ops | Chambers & Barnes (1996) setb4xxx | 13-machine layout | `fjsp-instances/barnes/setb4xxx.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4xxx` (**PDF block is a copy of seti5xxx; correct data from SchedulingLab used**) | `d294e49fcb3c6bd969c0fb4f1294f135c76b358d3adb225bf618ba0d4165ba6a` |
| `setb4xyt` | 15×12, 150 ops | Chambers & Barnes (1996) setb4xy | 12-machine layout | `fjsp-instances/barnes/setb4xy.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4xy` | `d436a34b15a3e92395ade3b7ea4bd882628d0cc4a9798c704cb887c50c433e13` |
| `setb4xyzt` | 15×13, 150 ops | Chambers & Barnes (1996) setb4xyz | 13-machine layout | `fjsp-instances/barnes/setb4xyz.txt`; FAST `chambersbarnes1996-1.pdf`, block `setb4xyz` | `a8f4ffd1ce66c6221f61d13320ecf06a18c0372c085ffe20a035fa2f9ece5add` |
| `seti5c12t` | 15×16, 225 ops | Chambers & Barnes (1996) seti5c12 | 16-machine layout | `fjsp-instances/barnes/seti5c12.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5c12` | `2c6325a4e1e34ee0a320b9fe6f25ea81cc6691c3e0597fb4b604d5fd015c7bc7` |
| `seti5cct` | 15×17, 225 ops | Chambers & Barnes (1996) seti5cc | 17-machine layout | `fjsp-instances/barnes/seti5cc.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5cc` | `5e9e73c2568a6c9abcdec5079a554bcdc24b01c90a4ff077c345917d51fe53a9` |
| `seti5xt` | 15×16, 225 ops | Chambers & Barnes (1996) seti5x | 16-machine layout | `fjsp-instances/barnes/seti5x.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5x` | `dc853dec7c74acb8aa586b5ed88424dd9a9e71ed0f31b08a9796043ae830bb2b` |
| `seti5xxt` | 15×17, 225 ops | Chambers & Barnes (1996) seti5xx | 17-machine layout | `fjsp-instances/barnes/seti5xx.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5xx` | `a0f15b37b42fc15bf846481d3697ce62f39325f62b3eb5b138cb16d6ea02e5c5` |
| `seti5xxxt` | 15×18, 225 ops | Chambers & Barnes (1996) seti5xxx | 18-machine layout | `fjsp-instances/barnes/seti5xxx.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5xxx` | `e523e3ab5d6731bc674fcff71884640af62594da503b074044cba6d9169bba9d` |
| `seti5xyt` | 15×17, 225 ops | Chambers & Barnes (1996) seti5xy | 17-machine layout | `fjsp-instances/barnes/seti5xy.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5xy` | `598a462d69c183123380a9271a8e22824e3a44ec9880d8c7706ed3d63292e9c4` |
| `seti5xyzt` | 15×18, 225 ops | Chambers & Barnes (1996) seti5xyz | 18-machine layout | `fjsp-instances/barnes/seti5xyz.txt`; FAST `chambersbarnes1996-1.pdf`, block `seti5xyz` | `9582cf8971210a3176ab49b5820aaf7af4bef6aece987498fefb834a8e6f17ca` |
