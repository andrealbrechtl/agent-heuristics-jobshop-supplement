#!/bin/bash
# X4: re-run the agent's reusable Behnke tabu (unchanged source, default seed 12345)
# under CP Optimizer's conditions: 120 s x 8 threads, two at a time, on all 60;
# then 1,200 s x 16 threads, one at a time, on the 32 open instances.
# Waits for the corridor16 queue so core counts are real. Resumable: skips runs with a schedule.
set -u
cd "$(dirname "$0")"
G="$(cd ../.. && pwd)"   # 2_fjsp_behnke
C="$G/../1_jssp_tai_100_1000_1/generalization/16cores"   # queue of run_corridor16.sh
log() { echo "$(date -Is) $*" >> queue.log; }
mkdir -p runs/tabu_120s runs/tabu_1200s
[ -x bin_fjsp ] || g++ -O3 -march=native -pthread -std=c++17 -o bin_fjsp $G/solver/mg_tabu_reusable/fjsp_solver.cpp
log "waiting for corridor16"
while ! grep -q "CORRIDOR16 DONE" $C/queue.log 2>/dev/null; do sleep 60; done
while pgrep -x csp3 > /dev/null; do sleep 30; done
log "start tabu rerun"
tabu() { # inst tl threads dir
  local f=$G/generalization/instances/$1.fjs o=runs/$4/$1
  [ -s $o.sched ] && [ -s $o.check ] && return
  timeout $(( $2 + 300 )) ./bin_fjsp $f $2 $3 $o.sched $o.csv > $o.out 2>&1
  python3 -I $G/check_fjsp.py $f $o.sched > $o.check 2>&1
}
export -f tabu; export G
log "start block tabu_120s"
seq 1 60 | sed 's/^/behnke/' | xargs -P 2 -I{} bash -c 'tabu {} 120 8 tabu_120s'
log "start block tabu_1200s"
xargs -P 1 -I{} bash -c 'tabu {} 1200 16 tabu_1200s' < open_behnke.txt
log "TABU RERUN DONE"
