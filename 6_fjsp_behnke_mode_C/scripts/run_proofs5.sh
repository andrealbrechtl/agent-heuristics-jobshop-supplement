#!/bin/bash
# Proof queue 5: first-level proofs only (T = current LB, one attempt), 900 s per run, 4 workers.
cd "$(dirname "$0")/.."
log() { echo "$(date -Is) $*" >> lb/proofs.log; }
log "start proofs5"
for i in behnke7 behnke11 behnke14 behnke15 behnke36 behnke37 behnke39 behnke58 behnke17 behnke18 behnke19 behnke20; do
  python3 -I update_status.py --quiet > /dev/null 2>&1
  read lb ub < <(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'], s['ub'])")
  [ "$lb" -ge "$ub" ] && { log "$i closed at $ub"; continue; }
  [ -s lb/proofs/${i}_T$lb/verdict.json ] && { log "$i T=$lb already attempted"; continue; }
  log "$i T=$lb start (UB $ub) [proofs5]"
  lb/prove.sh $i $lb 900 4 | tail -1 >> lb/proofs.log
done
log "proofs5 DONE"
