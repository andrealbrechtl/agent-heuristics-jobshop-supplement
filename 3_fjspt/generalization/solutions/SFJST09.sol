instance SFJST09
makespan 220.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 2 4.0 29.0
op 1 2 2 29.0 59.0
op 1 3 3 66.0 126.0
op 2 1 3 12.0 62.0
op 2 2 1 70.0 120.0
op 2 3 3 128.0 188.0
op 3 1 1 2.0 52.0
op 3 2 2 60.0 130.0
op 3 3 2 130.0 220.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 3 1 1 0 1 0.0 2.0
tr 3 2 1 1 2 52.0 60.0
tr 1 3 1 2 3 60.0 66.0
tr 1 1 2 0 2 0.0 4.0
tr 2 1 2 0 3 8.0 12.0
tr 2 2 2 3 1 62.0 70.0
tr 2 3 2 1 3 120.0 128.0
