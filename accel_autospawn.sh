#!/bin/bash
# accel_autospawn.sh — auto-spawn MyIntelAccelerator after boot
# Guards: (1) only when myintelfb in boot-args (2) skip if node already alive
LOG=/var/log/myintel_autospawn.log
echo "[$(date '+%F %T')] autospawn start" >> "$LOG"

# Gate 1: only on FB-enabled configs
ARGS=$(nvram boot-args 2>/dev/null)
if [[ "$ARGS" != *myintelfb* ]]; then
    echo "[$(date '+%F %T')] no myintelfb in boot-args — skip" >> "$LOG"
    exit 0
fi

# Wait for desktop stability + deferred FB start (t+30s after Phase 7 ≈ t+40s total)
sleep 50

# Gate 2: idempotent — skip if accelerator already alive
COUNT=$(ioreg -l 2>/dev/null | grep -c '"MyIntelAccelerator"=1')
if [ "$COUNT" -ge 1 ]; then
    echo "[$(date '+%F %T')] accel node already alive — skip" >> "$LOG"
    exit 0
fi

cd "/Users/ppbk/Documents/Default Project/source" || exit 1
./accel_steps alloc   >> "$LOG" 2>&1
sleep 1
./accel_steps attach 0 >> "$LOG" 2>&1
sleep 1
./accel_steps start   >> "$LOG" 2>&1

FINAL=$(ioreg -l 2>/dev/null | grep -o '"MyIntelAccelerator"=[0-9]*' | head -1)
echo "[$(date '+%F %T')] autospawn done — $FINAL" >> "$LOG"
