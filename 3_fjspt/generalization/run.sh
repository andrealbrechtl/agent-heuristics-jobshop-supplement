#!/bin/bash
# Usage: run.sh WORKDIR   (WORKDIR gets bin/, runs/ ; needs this folder's instances/txt)
# Pass A: unmodified solver (solver/fjspt_sa2.cpp), frozen options, 5 seeds x 60 s, 8 concurrent single-thread processes.
# Pass B: identical source + one extra stderr line on every incumbent improvement (time-to-best only).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); W=${1:?workdir}; SRC=$HERE/../solver
mkdir -p $W/bin $W/runs_A $W/runs_B
g++ -O3 -march=native -std=c++17 -o $W/bin/fjspt_sa2 $SRC/fjspt_sa2.cpp
python3 - $SRC/fjspt_sa2.cpp $W/fjspt_sa2_trace.cpp <<'P'
import sys
s=open(sys.argv[1]).read()
old="if (tc->f < tBest.f - 1e-12) { best = cur; tBest = *tc; }\n            } else undoAll();\n            if ((it & 0xFFFF)"
assert s.count(old)==1
new="if (tc->f < tBest.f - 1e-12) { best = cur; tBest = *tc; fprintf(stderr, \"NEWBEST t=%.3f cmax=%.1f\\n\", elapsed(), tBest.cmax / 2.0); }\n            } else undoAll();\n            if ((it & 0xFFFF)"
open(sys.argv[2],'w').write(s.replace(old,new))
P
g++ -O3 -march=native -std=c++17 -o $W/bin/fjspt_sa2_trace $W/fjspt_sa2_trace.cpp
OPTS="--time 60 --iters 100000000 --T0 4 --T1 0.2 --kick 5 --prestart 0.3 --prr 0.005"
for P in A B; do
  BIN=$W/bin/fjspt_sa2; [ $P = B ] && BIN=$W/bin/fjspt_sa2_trace
  for f in $HERE/instances/txt/*.txt; do n=$(basename $f .txt); for s in 1 2 3 4 5; do
    echo "$BIN --inst $f --seed $s --out $W/runs_$P/${n}_s$s.sol $OPTS > $W/runs_$P/${n}_s$s.out 2> $W/runs_$P/${n}_s$s.log"
  done; done | xargs -P 8 -I{} bash -c "{}"
done
