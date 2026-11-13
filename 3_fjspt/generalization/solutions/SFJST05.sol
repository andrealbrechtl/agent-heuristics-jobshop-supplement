instance SFJST05
makespan 123.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 2 2.0 38.0
op 1 2 1 59.0 123.0
op 2 1 1 4.0 38.0
op 2 2 2 42.0 63.0
op 3 1 1 38.0 59.0
op 3 2 2 63.0 100.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 2 1 1 0 1 0.0 4.0
tr 3 1 1 0 1 12.0 16.0
tr 1 2 1 2 1 38.0 42.0
tr 3 2 1 1 2 59.0 63.0
tr 1 1 2 0 2 0.0 2.0
tr 2 2 2 1 2 38.0 42.0
