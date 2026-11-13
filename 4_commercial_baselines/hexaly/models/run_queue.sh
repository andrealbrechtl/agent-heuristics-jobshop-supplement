#!/bin/bash
# Hexaly 15.0 run queue: same runs, budgets and threads as ../cpo/run_queue.sh.
# The academic licence has ONE token, so every run is alone on the machine (CP Optimizer ran its 1-core
# job-shop runs 15 at a time and its 120 s Behnke runs 2 at a time). Order: Behnke first.
# Resumable: skips runs whose output exists (move partial runs aside first).
set -u
cd "$(dirname "$0")"
export LD_LIBRARY_PATH=/opt/hexaly_15_0/bin
P=${PYTHON:-python3}   # Python with the hexaly module
J="$(cd ../../../1_jssp_tai_100_1000_1 && pwd)"
G="$(cd ../../../2_fjsp_behnke && pwd)"
JI() { if [ "$1" = 1 ]; then echo $J/instance/tai_100_1000_1.txt; else echo $J/generalization/instances/tai_100_1000_$1.txt; fi; }
mkdir -p runs/jssp_1core runs/jssp_16core runs/behnke_120s runs/behnke_1200s
log() { echo "$(date -Is) $*" >> queue.log; }
export -f JI; export P J G

jssp() { # inst seed threads dir
  local f=$(JI $1) o=runs/$4/tai_$1_s$2
  [ -s $o.txt ] && return
  timeout 3000 $P jssp_hx.py $f $o.txt 1800 $3 $2 $o.json > $o.out 2>&1
  python3 -I $J/check_jssp.py $f $o.txt --quiet > $o.check 2>&1
}
fjsp() { # inst tl threads dir
  local f=$G/generalization/instances/$1.fjs o=runs/$4/$1
  [ -s $o.sched ] && return
  timeout $(( $2 + 600 )) $P fjsp_hx.py $f $o.sched $2 $3 1 $o.json > $o.out 2>&1
  python3 -I $G/check_fjsp.py $f $o.sched > $o.check 2>&1
}
export -f jssp fjsp

log "start block behnke_120s"
seq 1 60 | sed 's/^/behnke/' | xargs -P 1 -I{} bash -c 'fjsp {} 120 8 behnke_120s'
log "start block behnke_1200s"
xargs -P 1 -I{} bash -c 'fjsp {} 1200 16 behnke_1200s' < open_behnke.txt
log "start block jssp_16core"
for i in $(seq 1 10); do for s in 1 2 3; do echo "$i $s"; done; done | xargs -P 1 -n 2 bash -c 'jssp $0 $1 16 jssp_16core'
log "start block jssp_1core"
for i in $(seq 1 10); do for s in 1 2 3; do echo "$i $s"; done; done | xargs -P 1 -n 2 bash -c 'jssp $0 $1 1 jssp_1core'
log "QUEUE DONE"
