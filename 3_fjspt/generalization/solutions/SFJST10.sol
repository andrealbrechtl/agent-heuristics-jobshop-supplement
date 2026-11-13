instance SFJST10
makespan 531.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 1 21.0 168.0
op 1 2 4 176.0 316.0
op 1 3 4 316.0 466.0
op 2 1 3 7.0 157.0
op 2 2 2 172.0 238.0
op 2 3 5 253.0 431.0
op 3 1 2 70.0 132.0
op 3 2 3 157.0 337.0
op 3 3 5 431.0 531.0
op 4 1 2 5.0 70.0
op 4 2 5 80.0 253.0
op 4 3 3 337.0 473.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 2 1 1 0 3 0.0 7.0
tr 1 1 1 0 1 18.0 21.0
tr 3 2 1 2 3 132.0 135.0
tr 1 2 1 1 4 168.0 176.0
tr 2 3 1 2 5 238.0 248.0
tr 4 3 1 5 3 253.0 263.0
tr 4 1 2 0 2 0.0 5.0
tr 3 1 2 0 2 18.0 23.0
tr 4 2 2 2 5 70.0 80.0
tr 2 2 2 3 2 157.0 172.0
tr 3 3 2 3 5 337.0 345.0
