instance MFJST01
makespan 485.0
# ops: job op machine start end   (job/op 1-based, times in original units)
op 1 1 2 71.0 194.0
op 1 2 2 194.0 324.0
op 1 3 4 333.0 483.0
op 2 1 3 8.0 158.0
op 2 2 3 158.0 245.0
op 2 3 6 390.0 485.0
op 3 1 1 36.0 123.0
op 3 2 4 134.0 239.0
op 3 3 5 251.0 351.0
op 4 1 2 6.0 71.0
op 4 2 5 78.0 251.0
op 4 3 6 254.0 390.0
op 5 1 1 123.0 251.0
op 5 2 3 256.0 342.0
op 5 3 5 351.0 461.0
# transports: job op vehicle from to depart arrive (location 0 = L/U)
tr 4 1 1 0 2 0.0 6.0
tr 5 1 1 0 1 18.0 22.0
tr 3 1 1 0 1 32.0 36.0
tr 4 2 1 2 5 71.0 78.0
tr 5 2 1 1 3 251.0 256.0
tr 5 3 1 3 5 342.0 351.0
tr 2 3 1 3 6 360.0 371.0
tr 2 1 2 0 3 0.0 8.0
tr 1 1 2 0 2 22.0 28.0
tr 3 2 2 1 4 123.0 134.0
tr 3 3 2 4 5 239.0 242.0
tr 4 3 2 5 6 251.0 254.0
tr 1 3 2 2 4 324.0 333.0
