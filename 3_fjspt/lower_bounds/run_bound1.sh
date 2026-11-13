#!/bin/bash
# Runs bound (1) on all instances with CP-SAT and CPLEX (sound variant), plus the
# forced-|V| and shortest-path-empty variants with CP-SAT. Outputs results/b1/*.json
cd "$(dirname "$0")"
P=${PYTHON:-python3}   # needs ortools 9.15 and docplex with CPLEX/CP Optimizer 22.2
INST="../instances/*.json ../generalization/instances/*.json"
for f in $INST; do
  n=$(basename $f .json)
  for s in cpsat cplex; do
    [ -f results/b1/${n}_sound_$s.json ] || $P bound1_ip.py $f --solver $s --time 600 --threads 8 --json results/b1/${n}_sound_$s.json
  done
  [ -f results/b1/${n}_force_cpsat.json ] || $P bound1_ip.py $f --solver cpsat --time 600 --threads 8 --force-all-vehicles --json results/b1/${n}_force_cpsat.json
  [ -f results/b1/${n}_sp_cpsat.json ] || $P bound1_ip.py $f --solver cpsat --time 600 --threads 8 --sp-empty --json results/b1/${n}_sp_cpsat.json
done
