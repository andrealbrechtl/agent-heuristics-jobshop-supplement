#!/bin/bash
# Corridor solver (agent csp3.c, unchanged) on 16 cores: 16 independent single-core
# workers with different seeds, 1,800 s, best of 16 = one "16-core run".
# Three such runs per instance (reps 1-3), one instance-rep at a time, to mirror
# the CP Optimizer 16-core block (3 seeds x 16 workers x 1,800 s).
set -u
cd "$(dirname "$0")"
G="$(cd ../.. && pwd)"   # 1_jssp_tai_100_1000_1
declare -A T0=([1]=533000 [2]=541000 [3]=542000 [4]=538000 [5]=529000 [6]=534000 [7]=533000 [8]=537000 [9]=539000 [10]=533000)
mkdir -p runs logs
log() { echo "$(date -Is) $*" >> queue.log; }
log "start corridor16"
for k in $(seq 1 10); do
  for rep in 1 2 3; do
    tag=k${k}_r${rep}
    [ -f runs/${tag}.done ] && continue
    log "start $tag"
    for w in $(seq 1 16); do
      seed=$((rep*1000 + w))
      ./csp3 $G/generalization/instances/tai_100_1000_${k}.txt runs/${tag}_w${w}.txt ${T0[$k]} 1800 $seed 8 40 none 1000 200000 > logs/${tag}_w${w}.log 2>&1 &
    done
    wait
    for w in $(seq 1 16); do
      [ -s runs/${tag}_w${w}.txt ] && python3 -I $G/check_jssp.py $G/generalization/instances/tai_100_1000_${k}.txt runs/${tag}_w${w}.txt --quiet > runs/${tag}_w${w}.check 2>&1
    done
    touch runs/${tag}.done
    log "done $tag"
  done
done
log "CORRIDOR16 DONE"
