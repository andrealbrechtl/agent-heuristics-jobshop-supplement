#!/bin/bash
# LB queue 1: for every instance without an open-source certificate, (a) lag relaxation to optimality
# (120 s, 8 workers), (b) full model with pools, cap = best UB, hint = best schedule (600 s, 8 workers).
# Three runs at a time (24 threads).
set -u
cd "$(dirname "$0")/.."
PY=${PYTHON:-python3}   # Python with OR-Tools 9.15
mkdir -p lb/runs/lag lb/runs/full600
log() { echo "$(date -Is) $*" >> lb/queue1.log; }
one() { # inst ub
  local i=$1 ub=$2
  [ -s lb/runs/lag/$i.json ] || $PY -I lb/lb_cpsat.py instances/$i.fjs --relax lag --time 120 --workers 8 --out lb/runs/lag/$i > lb/runs/lag/$i.log 2>&1
  log "$i lag done: $(grep FINAL lb/runs/lag/$i.log)"
  [ -s lb/runs/full600/$i.json ] || $PY -I lb/lb_cpsat.py instances/$i.fjs --time 600 --workers 8 --cap $ub --hint best/$i.sched --out lb/runs/full600/$i > lb/runs/full600/$i.log 2>&1
  log "$i full600 done: $(grep FINAL lb/runs/full600/$i.log)"
}
export -f one log; export PY
log "start queue1"
python3 -I -c "
import json; s=json.load(open('best/status.json'))
rows=[(v['ub']-v['lb'],k,v['ub']) for k,v in s.items() if not v['closed']]
for g,k,ub in sorted(rows): print(k,ub)
" | xargs -P 3 -n 2 bash -c 'one $0 $1'
log "queue1 DONE"
