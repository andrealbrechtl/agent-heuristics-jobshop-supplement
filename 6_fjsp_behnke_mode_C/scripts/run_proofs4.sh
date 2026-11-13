#!/bin/bash
# Proof queue 4 (after ub1): long frontier proofs. For each listed open instance, feasibility at
# T = current LB with 3600 s x 8 workers, OR-Tools 9.15 (pools 1) then 9.14 (pools 0); PROVED needs both.
cd "$(dirname "$0")/.."
until grep -q "ub1 DONE" runs/ub1.log 2>/dev/null; do sleep 60; done
log() { echo "$(date -Is) $*" >> lb/proofs.log; }
log "start proofs4"
export MATRIX="915:1 914:0" TAG="_long"
for i in behnke35 behnke52 behnke54 behnke13 behnke56 behnke57 behnke60 behnke38 behnke40 behnke59 behnke51; do
  while true; do
    python3 -I update_status.py --quiet > /dev/null 2>&1
    read lb ub < <(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'], s['ub'])")
    [ "$lb" -ge "$ub" ] && { log "$i closed at $ub"; break; }
    log "$i T=$lb start (UB $ub) [proofs4 long]"
    v=$(lb/prove.sh $i $lb 3600 8 | tail -1); echo "$v" >> lb/proofs.log
    case "$v" in *PROVED*) continue;; *) break;; esac
  done
done
log "proofs4 DONE"
