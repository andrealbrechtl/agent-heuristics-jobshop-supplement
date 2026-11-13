#!/bin/bash
cd "$(dirname "$0")"
echo "== queues"; tail -2 lb/queue1.log; tail -2 lb/proofs.log; tail -2 runs/ub1.log 2>/dev/null; tail -2 runs/ub2.log 2>/dev/null
echo "== status"; python3 -I update_status.py 2>&1 | tail -40
uptime
