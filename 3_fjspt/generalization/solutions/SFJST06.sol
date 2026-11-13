instance SFJST06
makespan 324.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 1 2.0 19.0
op 1 2 1 19.0 59.0
op 1 3 3 67.0 127.0
op 2 1 1 59.0 89.0
op 2 2 1 89.0 239.0
op 2 3 3 247.0 317.0
op 3 1 2 4.0 64.0
op 3 2 2 64.0 234.0
op 3 3 2 234.0 324.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 1 1 1 0 1 0.0 2.0
tr 1 3 1 1 3 59.0 67.0
tr 3 1 2 0 2 0.0 4.0
tr 2 1 2 0 1 8.0 10.0
tr 2 3 2 1 3 239.0 247.0
