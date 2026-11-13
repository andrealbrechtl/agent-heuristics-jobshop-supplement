#!/bin/bash
# Variant comparison + run-to-run variance, in one experiment.
#
# Motivation: the 120 s ablation on behnke31 showed the configuration WITHOUT
# ruin-and-recreate reaching 226 (the proven optimum) at 22 s, while the full
# configuration sat at 227-229.  That is either
#   (a) a real effect -- the ruin-and-recreate perturbation is too destructive on
#       these instances and plain random kicks diversify better, or
#   (b) a single lucky seed.
# Independent seeds on both variants separate the two, and the same data answers
# the "is one long run or a portfolio of short runs better?" question, because
# best-of-N short runs is exactly what a portfolio would have returned.
#
# Short budget on purpose: every instance saturated within 23-72 s of a 1200 s
# run, so 90 s per run buys essentially the whole trajectory at 1/13 the cost.
set -u
cd "$(dirname "$0")"

SECS=${SECS:-90}
THREADS=${THREADS:-32}
mkdir -p results/variants logs/variants

# instance -> how many seeds (behnke16 is 2x the operations, so it gets fewer)
SEEDS_SMALL=${SEEDS_SMALL:-"11 22 33 44 55 66"}
SEEDS_BIG=${SEEDS_BIG:-"11 22 33 44"}

run_variant() {
    local inst=$1 variant=$2 seeds=$3
    local envvar=""
    [ "$variant" = "norr" ] && envvar="FJSP_NO_RR=1"

    local vals=""
    for sd in $seeds; do
        local tag="${inst}_${variant}_s${sd}"
        env ${envvar:+$envvar} ./bin_fjsp "instances/$inst.txt" "$SECS" "$THREADS" \
            "results/variants/${tag}.sched" "results/variants/${tag}.csv" \
            0 "$sd" > "logs/variants/${tag}.out" 2> "logs/variants/${tag}.err"
        local mk tb
        mk=$(grep best_makespan "logs/variants/${tag}.out" | awk '{print $2}')
        tb=$(grep time_to_best "logs/variants/${tag}.out" | awk '{print $2}')
        printf "    seed %-3s -> %s (t_best %ss)\n" "$sd" "$mk" "$tb"
        vals="$vals $mk"
    done

    local sorted best mean
    sorted=$(echo "$vals" | tr ' ' '\n' | grep -v '^$' | sort -n)
    best=$(echo "$sorted" | head -1)
    mean=$(echo "$sorted" | awk '{s+=$1; n++} END {printf "%.2f", s/n}')
    echo "  $inst / $variant : values:$vals"
    echo "  $inst / $variant : best(portfolio)=$best  mean=$mean  worst=$(echo "$sorted" | tail -1)"
    echo "VARIANTROW $inst $variant $best $mean $(echo "$sorted" | tail -1) $(echo "$vals" | wc -w)"
    echo
}

for inst in behnke31 behnke14 behnke16; do
    seeds="$SEEDS_SMALL"
    [ "$inst" = "behnke16" ] && seeds="$SEEDS_BIG"
    echo "=============== $inst : ${SECS}s x ${THREADS} threads ==============="
    echo "  --- variant: default (with ruin-and-recreate) ---"
    run_variant "$inst" default "$seeds"
    echo "  --- variant: norr (random kicks only) ---"
    run_variant "$inst" norr "$seeds"
done

echo "VARIANTS COMPLETE $(date -u +%H:%M:%S)"
