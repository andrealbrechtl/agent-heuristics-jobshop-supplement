instance SFJST07
makespan 409.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 1 3.0 120.0
op 1 2 2 123.0 253.0
op 1 3 4 259.0 409.0
op 2 1 3 7.0 157.0
op 2 2 3 157.0 212.0
op 2 3 3 212.0 277.0
op 3 1 2 23.0 85.0
op 3 2 4 91.0 161.0
op 3 3 5 166.0 266.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 2 1 1 0 3 0.0 7.0
tr 1 2 1 1 2 120.0 123.0
tr 1 1 2 0 1 0.0 3.0
tr 3 1 2 0 2 18.0 23.0
tr 3 2 2 2 4 85.0 91.0
tr 3 3 2 4 5 161.0 166.0
tr 1 3 2 2 4 253.0 259.0
