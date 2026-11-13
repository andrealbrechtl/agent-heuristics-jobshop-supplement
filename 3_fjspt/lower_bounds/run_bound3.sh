#!/bin/bash
# Relaxation (3) on all instances; $1 = cpsat|cpo, $2 = time limit (s). Hinted with our certified schedule.
cd "$(dirname "$0")"
P=${PYTHON:-python3}   # needs ortools 9.15 and docplex with CPLEX/CP Optimizer 22.2
S=$1; T=${2:-300}
for f in ../generalization/instances/*.json ../instances/*.json; do
  n=$(basename $f .json)
  h=$(ls ../solutions/$n.sol ../generalization/solutions/$n.sol 2>/dev/null | head -1)
  out=results/b3/${n}_${S}_t${T}.json
  [ -f $out ] || $P bound3_relax.py $f --solver $S --time $T --threads 8 ${h:+--hint $h} --json $out
done
