#!/bin/bash
# Component ablation on behnke31 (the calibration anchor).
# Each configuration gets the same wall-clock budget and the same thread count,
# so the numbers are comparable.  Short budget on purpose -- this measures the
# contribution of each component, not the final result.
set -u
cd "$(dirname "$0")"

SECS=${SECS:-120}
THREADS=${THREADS:-32}
INST=${INST:-behnke31}
mkdir -p results/ablation logs/ablation

run() {
    local tag=$1; shift
    local thr=${ABL_THREADS:-$THREADS}
    # Slugify: config labels are prose and may contain "/" or spaces, which do not
    # survive as path components.  A "/" in a label silently created a bogus
    # directory path and lost a whole configuration, so map to a safe filename.
    local slug
    slug=$(printf '%s' "$tag" | tr -c 'A-Za-z0-9._-' '_')
    env "$@" ./bin_fjsp2 "instances/$INST.txt" "$SECS" "$thr" \
        "results/ablation/${INST}_${slug}.sched" "results/ablation/${INST}_${slug}.csv" \
        > "logs/ablation/${slug}.out" 2> "logs/ablation/${slug}.err"
    local mk ttb it
    mk=$(grep best_makespan "logs/ablation/${slug}.out" | awk '{print $2}')
    ttb=$(grep time_to_best "logs/ablation/${slug}.out" | awk '{print $2}')
    it=$(grep total_iterations "logs/ablation/${slug}.out" | awk '{print $2}')
    ips=$(grep iterations_per_sec_total "logs/ablation/${slug}.out" | awk '{print $2}')
    printf "| %-34s | %4s | %8s | %14s | %13s |\n" "$tag" "$mk" "$ttb" "$it" "$ips"
}

echo "Ablation on $INST -- ${SECS}s, ${THREADS} threads each"
echo
printf "| %-34s | %4s | %8s | %14s | %13s |\n" "configuration" "best" "t_best" "iterations" "iters/s (all)"
printf "|%s|%s|%s|%s|%s|\n" "------------------------------------" "------" "----------" "----------------" "---------------"

run "full algorithm"                       FJSP_DUMMY=0
run "no tabu memory"                       FJSP_NO_TABU=1
run "random kicks instead of ruin+recreate" FJSP_NO_RR=1
run "no shared incumbent / elite pool"     FJSP_NO_SHARE=1
run "exact eval of every neighbour"        FJSP_EXACT_EVAL=1
run "dispatching rules only (no search)"   FJSP_GT_ONLY=1
ABL_THREADS=1 run "single thread"          FJSP_DUMMY=0

echo
echo "ABLATION COMPLETE"
