instance MFJST02
makespan 468.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 1 10.0 157.0
op 1 2 1 157.0 280.0
op 1 3 4 288.0 438.0
op 2 1 3 20.0 170.0
op 2 2 3 170.0 257.0
op 2 3 7 272.0 422.0
op 3 1 2 71.0 133.0
op 3 2 4 141.0 246.0
op 3 3 5 256.0 356.0
op 4 1 2 6.0 71.0
op 4 2 5 81.0 254.0
op 4 3 6 262.0 398.0
op 5 1 2 133.0 256.0
op 5 2 3 264.0 350.0
op 5 3 5 358.0 468.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 4 1 1 0 2 0.0 6.0
tr 2 1 1 0 3 10.0 20.0
tr 3 1 1 0 2 24.0 30.0
tr 4 2 1 2 5 71.0 81.0
tr 5 1 1 0 2 85.0 91.0
tr 3 2 1 2 4 133.0 141.0
tr 3 3 1 4 5 246.0 256.0
tr 4 3 1 5 6 256.0 262.0
tr 2 3 1 3 7 264.0 272.0
tr 1 3 1 1 4 280.0 288.0
tr 1 1 2 0 1 0.0 10.0
tr 5 2 2 2 3 256.0 264.0
tr 5 3 2 3 5 350.0 358.0
