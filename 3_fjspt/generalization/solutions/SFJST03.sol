instance SFJST03
makespan 223.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 1 4.0 47.0
op 1 2 2 128.0 223.0
op 2 1 2 2.0 55.0
op 2 2 2 55.0 128.0
op 3 1 1 47.0 172.0
op 3 2 1 172.0 215.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 2 1 1 0 2 0.0 2.0
tr 3 1 1 0 1 6.0 10.0
tr 1 1 2 0 1 0.0 4.0
tr 1 2 2 1 2 47.0 51.0
