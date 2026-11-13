#!/bin/bash
# Cross-validated optimality proof for one instance.
# usage: prove.sh INST T SECONDS WORKERS [RELAX]  -> tries to prove that no schedule with makespan <= T exists
#        RELAX = none (full model, default) | lag | cumul (sound relaxations, see lb_cpsat.py)
# Runs the FULL model (relax none, no hint) in --mode feas --cap T under a matrix of
#   OR-Tools 9.15 / 9.14  x  pools 1 / pools 0   (4 runs, different seeds)
# Results in lb/proofs/<inst>_T<T>/run_<ver>_p<pools>.{log,json}. Prints a one-line verdict:
#   PROVED   all finished runs INFEASIBLE, at least one per version, none FEASIBLE
#   FEASIBLE some run found a schedule <= T (then the schedule is in the run's .sched)
#   OPEN     otherwise (timeouts)
set -u
cd "$(dirname "$0")/.."
I=$1; T=$2; S=${3:-600}; W=${4:-8}; R=${5:-none}
MATRIX=${MATRIX:-"915:1 915:0 914:1 914:0"}      # version:pools pairs; a different matrix gets its own folder
TAG=${TAG:-}
D=lb/proofs/${I}_T${T}_$R; [ "$R" = none ] && D=lb/proofs/${I}_T$T; D=$D$TAG; mkdir -p $D
P915=${PYTHON:-python3}   # Python with OR-Tools 9.15
P914=venv914/bin/python
seed=0
for vp in $MATRIX; do
  ver=${vp%%:*}; pools=${vp##*:}
  PY=$P915; [ $ver = 914 ] && PY=$P914
  for once in 1; do
    seed=$((seed+1)); o=$D/run_${ver}_p$pools
    [ -s $o.json ] || $PY -I lb/lb_cpsat.py instances/$I.fjs --mode feas --cap $T --relax $R --pools $pools --time $S --workers $W --seed $seed --out $o > $o.log 2>&1
    grep -q "status=\(FEASIBLE\|OPTIMAL\)" $o.log && break 2     # a schedule <= T exists: no proof possible
  done
done
python3 -I - "$D" "$I" "$T" "$R" <<'PYEOF'
import json,glob,sys
D,I,T,R=sys.argv[1],sys.argv[2],int(sys.argv[3]),sys.argv[4]
st={}
for f in sorted(glob.glob(D+'/run_*.json')):
    r=json.load(open(f)); st[f.split('/')[-1][:-5]]=r['status']
inf=[k for k,v in st.items() if v=='INFEASIBLE']; fea=[k for k,v in st.items() if v in ('FEASIBLE','OPTIMAL')]
v915=any(k.startswith('run_915') for k in inf); v914=any(k.startswith('run_914') for k in inf)
if fea: verdict='FEASIBLE'
elif inf and v915 and v914 and len(inf)==len(st): verdict='PROVED'
else: verdict='OPEN'
print(f'VERDICT {I} cap {T} relax {R}: {verdict} {st}')
json.dump(dict(instance=I,cap=T,verdict=verdict,runs=st,bound=(T+1 if verdict=='PROVED' else None),relax=R,status=('INFEASIBLE' if verdict=='PROVED' else 'UNKNOWN'),proof=f'cross-validated 9.15+9.14, pools 1+0, no hint, relax {R}'),open(D+'/verdict.json','w'),indent=1)
PYEOF
