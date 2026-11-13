#!/bin/bash
# Proof queue 3: the 100-job instances with gap <= 2, feasibility at T = LB (iterated like proofs2).
# Either refutes T (LB+1) or finds a schedule of makespan T. 1200 s per run, 4 workers.
cd "$(dirname "$0")/.."
log() { echo "$(date -Is) $*" >> lb/proofs.log; }
log "start proofs3"
for i in behnke38 behnke56 behnke57 behnke60 behnke40 behnke59; do
  while true; do
    python3 -I update_status.py --quiet > /dev/null 2>&1
    read lb ub < <(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'], s['ub'])")
    [ "$lb" -ge "$ub" ] && { log "$i closed at $ub"; break; }
    log "$i T=$lb start (UB $ub) [proofs3]"
    v=$(lb/prove.sh $i $lb 1200 4 | tail -1); echo "$v" >> lb/proofs.log
    case "$v" in *PROVED*) continue;; *) break;; esac
  done
done
log "proofs3 DONE"
