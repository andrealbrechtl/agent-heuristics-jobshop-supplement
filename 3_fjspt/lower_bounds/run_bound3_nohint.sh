#!/bin/bash
# Relaxation (3) WITHOUT solution hints on the 21 CB instances (+ MFJST10), $1 = cpsat|cpo, $2 = time limit (s)
cd "$(dirname "$0")"
P=${PYTHON:-python3}   # needs ortools 9.15 and docplex with CPLEX/CP Optimizer 22.2
S=$1; T=${2:-300}
for f in ../instances/mt10*.json ../instances/setb4*.json ../instances/seti5*.json ../generalization/instances/MFJST10.json; do
  n=$(basename $f .json); out=results/b3_nohint/${n}_${S}_t${T}.json
  [ -f $out ] || $P bound3_relax.py $f --solver $S --time $T --threads 8 --json $out
done
