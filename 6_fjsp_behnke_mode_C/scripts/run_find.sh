#!/bin/bash
# Find queue: CP-SAT feasibility at T = UB-1 (OR-Tools 9.14, pools on, 4 workers, 1200 s) on every
# open instance, largest instances first; when a schedule is found, repeat at T-1.
cd "$(dirname "$0")/.."
mkdir -p runs/find
log() { echo "$(date -Is) $*" >> runs/find.log; }
log "start find"
for i in behnke58 behnke36 behnke39 behnke20 behnke19 behnke17 behnke18 behnke16 behnke57 behnke56 behnke59 behnke14 behnke15 behnke11 behnke12 behnke13 behnke51 behnke54 behnke35 behnke7; do
  while true; do
    python3 -I update_status.py --quiet > /dev/null 2>&1
    read lb ub < <(python3 -I -c "import json; s=json.load(open('best/status.json'))['$i']; print(s['lb'], s['ub'])")
    [ "$lb" -ge "$ub" ] && { log "$i closed at $ub"; break; }
    T=$((ub-1)); o=runs/find/${i}_T$T
    if [ -s $o.json ]; then log "$i T=$T already tried"; break; fi
    log "$i T=$T start (LB $lb UB $ub)"
    venv914/bin/python -I lb/lb_cpsat.py instances/$i.fjs --mode feas --cap $T --pools 1 --time 1200 --workers 4 --seed 17 --out $o > $o.log 2>&1
    st=$(grep FINAL $o.log | sed 's/.*status=\([A-Z]*\).*/\1/')
    log "$i T=$T -> $st"
    case "$st" in FEASIBLE|OPTIMAL) continue;; *) break;; esac
  done
done
log "find DONE"
