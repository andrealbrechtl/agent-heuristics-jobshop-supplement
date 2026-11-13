#!/bin/bash
# Proof queue 2 (after proofs1): iterative destructive bounds. For every open instance with at most
# 50 jobs, smallest gap first: prove makespan <= LB infeasible (LB+1), then LB+1, ... until a run
# times out (OPEN), a schedule is found (FEASIBLE: new UB), or the instance closes. 900 s/run, 6 workers.
cd "$(dirname "$0")/.."
until grep -q "proofs1 DONE" lb/proofs.log 2>/dev/null; do sleep 60; done
log() { echo "$(date -Is) $*" >> lb/proofs.log; }
log "start proofs2"
python3 -I update_status.py --quiet > /dev/null 2>&1
python3 -I -c "
import json; s=json.load(open('best/status.json'))
rows=[]
for k,v in s.items():
    if v['closed']: continue
    n=int(open('instances/%s.fjs'%k).readline().split()[0])
    if n<=50: rows.append((v['ub']-v['lb'],n,k))
for g,n,k in sorted(rows): print(k)
" | while read i; do
  while true; do
    python3 -I update_status.py --quiet > /dev/null 2>&1
    read lb ub < <(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'], s['ub'])")
    [ "$lb" -ge "$ub" ] && { log "$i closed at $ub"; break; }
    log "$i T=$lb start (UB $ub)"
    v=$(lb/prove.sh $i $lb 900 6 | tail -1); echo "$v" >> lb/proofs.log
    case "$v" in *PROVED*) continue;; *) break;; esac
  done
done
log "proofs2 DONE"
