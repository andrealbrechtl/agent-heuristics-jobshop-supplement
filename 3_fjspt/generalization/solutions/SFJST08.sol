instance SFJST08
makespan 269.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 2 8.0 33.0
op 1 2 2 33.0 63.0
op 1 3 3 69.0 229.0
op 2 1 1 6.0 36.0
op 2 2 4 46.0 101.0
op 2 3 4 101.0 179.0
op 3 1 1 36.0 92.0
op 3 2 2 98.0 168.0
op 3 3 4 179.0 269.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 1 1 1 0 2 0.0 8.0
tr 3 1 1 0 1 18.0 24.0
tr 3 2 1 1 2 92.0 98.0
tr 2 1 2 0 1 0.0 6.0
tr 2 2 2 1 4 36.0 46.0
tr 1 3 2 2 3 63.0 69.0
tr 3 3 2 2 4 168.0 176.0
