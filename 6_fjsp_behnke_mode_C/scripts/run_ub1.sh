#!/bin/bash
# UB queue 1: fjsp_ls3 on every instance without a certificate, smallest gap first,
# warm start from best/<inst>.sched, stop at the lower bound, 600 s, 8 threads, one at a time.
cd "$(dirname "$0")/.."
mkdir -p runs/ub1
log() { echo "$(date -Is) $*" >> runs/ub1.log; }
log "start ub1"
python3 -I -c "
import json; s=json.load(open('best/status.json'))
rows=[(v['ub']-v['lb'],k,v['lb']) for k,v in s.items() if not v['closed']]
for g,k,lb in sorted(rows): print(k,lb)
" | while read i lb; do
  [ -s runs/ub1/$i.out ] && continue
  bin/fjsp_ls3 -i instances/$i.fjs -o runs/ub1/$i.sched -p runs/ub1/$i.csv -w best/$i.sched -T $lb -s 600 -t 8 -r 7 > runs/ub1/$i.out 2> runs/ub1/$i.err
  log "$i: $(grep best_makespan runs/ub1/$i.out) (LB $lb) $(python3 -I update_status.py --quiet 2>&1 | tail -1)"
done
log "ub1 DONE"
