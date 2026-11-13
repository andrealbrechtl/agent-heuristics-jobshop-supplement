#!/bin/sh
# Driver: build, run the solver, then polish the end stages, keeping the best.
# Usage: ./run.sh [seconds] [threads]
set -e
cd "$(dirname "$0")"
SECS=${1:-3600}
THREADS=${2:-$(nproc)}

g++ -O3 -march=native -pthread -o solver4 solver2.cpp
g++ -O3 -o polish polish.cpp

# The solver warm-starts from schedule.txt if one is already there, and
# rewrites it (atomically) every time it improves.
./solver4 -i /app/instances/behnke16.fjs -o schedule.txt -t "$THREADS" -s "$SECS" -r "$$"

# Final coordinate-descent polish of the two separable end stages.
./polish -f schedule.txt -o schedule.txt -n 400000 -R 60 || true

python3 check.py /app/instances/behnke16.fjs schedule.txt
