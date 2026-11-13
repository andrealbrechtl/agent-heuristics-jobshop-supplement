#!/bin/bash
# UB queue 3: fjsp_ls3 1800 s x 8 threads on the large-gap instances, largest gap first (opposite
# order to ub2, so the two queues meet in the middle), warm start from best/<inst>.sched, seed 23.
cd "$(dirname "$0")/.."
mkdir -p runs/ub3
log() { echo "$(date -Is) $*" >> runs/ub3.log; }
log "start ub3"
for i in behnke16 behnke17 behnke18 behnke20 behnke14 behnke15 behnke19 behnke36 behnke7 behnke11 behnke37 behnke12 behnke39 behnke58 behnke51 behnke13; do
  python3 -I update_status.py --quiet > /dev/null 2>&1
  lb=$(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'] if not s['closed'] else -1)")
  [ "$lb" = "-1" ] && { log "$i already closed, skip"; continue; }
  bin/fjsp_ls3 -i instances/$i.fjs -o runs/ub3/$i.sched -p runs/ub3/$i.csv -w best/$i.sched -T $lb -s 1800 -t 8 -r 23 > runs/ub3/$i.out 2> runs/ub3/$i.err
  log "$i: $(grep best_makespan runs/ub3/$i.out) (LB $lb) $(python3 -I update_status.py --quiet 2>&1 | tail -1 | cut -c1-40)"
done
log "ub3 DONE"
