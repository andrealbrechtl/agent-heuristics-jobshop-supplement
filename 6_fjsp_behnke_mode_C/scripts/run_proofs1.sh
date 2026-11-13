#!/bin/bash
# Proof queue 1: cross-validate every lower bound that so far rests on a single CP-SAT run
# (the supplement's 120 s bounds and today's full-model OPTIMAL results). One instance at a time.
cd "$(dirname "$0")/.."
log() { echo "$(date -Is) $*" >> lb/proofs.log; }
log "start proofs1"
while read i T; do
  log "$i T=$T start"
  lb/prove.sh $i $T 900 6 | tail -1 >> lb/proofs.log
done <<LIST
behnke46 113
behnke29 116
behnke48 124
behnke33 223
behnke55 220
behnke53 211
behnke52 210
behnke54 221
behnke7 117
behnke13 223
behnke38 386
behnke16 391
LIST
log "proofs1 DONE"
