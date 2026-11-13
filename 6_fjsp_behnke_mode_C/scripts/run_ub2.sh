#!/bin/bash
# UB queue 2 (starts when LB queue 1 is done): fjsp_ls3 1800 s x 16 threads on every instance
# without a certificate, smallest gap first, warm start from best/<inst>.sched, stop at the LB.
cd "$(dirname "$0")/.."
until grep -q "queue1 DONE" lb/queue1.log 2>/dev/null; do sleep 60; done
mkdir -p runs/ub2
log() { echo "$(date -Is) $*" >> runs/ub2.log; }
log "start ub2"
python3 -I update_status.py --quiet > /dev/null 2>&1
python3 -I -c "
import json; s=json.load(open('best/status.json'))
rows=[(v['ub']-v['lb'],k) for k,v in s.items() if not v['closed']]
for g,k in sorted(rows): print(k)
" | while read i; do
  [ -s runs/ub2/$i.out ] && continue
  python3 -I update_status.py --quiet > /dev/null 2>&1
  lb=$(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'] if not s['closed'] else -1)")
  [ "$lb" = "-1" ] && { log "$i already closed, skip"; continue; }
  bin/fjsp_ls3 -i instances/$i.fjs -o runs/ub2/$i.sched -p runs/ub2/$i.csv -w best/$i.sched -T $lb -s 1800 -t 16 -r 11 > runs/ub2/$i.out 2> runs/ub2/$i.err
  log "$i: $(grep best_makespan runs/ub2/$i.out) (LB $lb) $(python3 -I update_status.py --quiet 2>&1 | tail -1)"
done
log "ub2 DONE"
