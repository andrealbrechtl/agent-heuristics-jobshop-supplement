#!/bin/sh
# Download every upstream source of the FJSPT instances (pinned versions) and compare them
# with ../instances/*.json.  Needs: curl, git, pdftotext (poppler-utils), python3.
# Usage: sh fetch_and_compare.sh [WORKDIR]      (default: ./upstream)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
W=${1:-$HERE/upstream}
mkdir -p "$W/fast" "$W/scheduleopt"

# 1. FAST project page (Homayouni & Fontes): job sets and travel-time layouts (PDF)
B=https://fastmanufacturingproject.wordpress.com/wp-content/uploads/2020/05
for f in 4to18machines_layouts-1.pdf brandimarte1993-2.pdf chambersbarnes1996-1.pdf; do
  curl -sfL -o "$W/fast/$f" "$B/$f"
  pdftotext -layout "$W/fast/$f" "$W/fast/${f%.pdf}.txt"
done

# 2. Independent copies of the job data / travel matrices, pinned commits
clone() { # repo commit dir
  [ -d "$W/$3" ] || git clone -q "https://github.com/$1" "$W/$3"
  git -C "$W/$3" checkout -q "$2"
}
clone SchedulingLab/fjsp-instances      ac4c3402312bfbeafcf4472d78be567d4e6b46ab fjsp-instances
clone wangyuting9836/FJSP_AGV_MILP      48b3f2130063e606ecddced4c3f46cd1f93aa061 FJSP_AGV_MILP
SO=https://raw.githubusercontent.com/ScheduleOpt/benchmarks/f89e5b007c90a1c1a70fa48a28d657a72da05665/flexible-jobshop/instances/fjsp/Brandimarte1993
for k in 01 02 03 04 05 06 07 08 09 10; do curl -sfL -o "$W/scheduleopt/mk$k.txt" "$SO/mk$k.txt"; done

echo "sha256 of the downloaded PDFs (expected values in SOURCE.md):"
(cd "$W/fast" && sha256sum *.pdf)

python3 -I "$HERE/rebuild_and_compare.py" "$W/fast" "$W/fjsp-instances" "$W/FJSP_AGV_MILP" \
        "$HERE/../instances" x "$W/scheduleopt"
