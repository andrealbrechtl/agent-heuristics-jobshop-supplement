#!/bin/bash
# Full benchmark run: 20 minutes of wall-clock per instance, 32 threads,
# each instance getting the whole machine (runs are sequential, not concurrent).
set -u
cd "$(dirname "$0")"

SECS=${SECS:-1200}
THREADS=${THREADS:-32}

mkdir -p results schedules logs

for inst in behnke31 behnke16 behnke14; do
    echo "=== $inst : ${SECS}s x ${THREADS} threads : $(date -u +%H:%M:%S) ==="
    ./bin_fjsp "instances/$inst.txt" "$SECS" "$THREADS" \
        "schedules/$inst.sched" "results/${inst}_progress.csv" \
        > "logs/${inst}.out" 2> "logs/${inst}.err"
    tail -5 "logs/${inst}.out"
    echo "--- validating $inst ---"
    python3 src/validate.py "instances/$inst.txt" "schedules/$inst.sched" \
        | tee "logs/${inst}.validate"
    echo "VALIDATOR_EXIT=${PIPESTATUS[0]}"
    echo
done
echo "ALL RUNS COMPLETE $(date -u +%H:%M:%S)"
