instance SFJST04
makespan 359.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 2 2.0 65.0
op 1 2 1 129.0 216.0
op 2 1 2 65.0 185.0
op 2 2 2 185.0 337.0
op 3 1 1 4.0 129.0
op 3 2 1 216.0 359.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 3 1 1 0 1 0.0 4.0
tr 1 1 2 0 2 0.0 2.0
tr 2 1 2 0 2 6.0 8.0
tr 1 2 2 2 1 65.0 69.0
