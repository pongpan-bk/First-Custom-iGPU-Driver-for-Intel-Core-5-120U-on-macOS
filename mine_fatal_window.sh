#!/bin/bash
# Extract OUR driver's log lines from the failed-boot window (21:37-21:47)
echo "── A) MyIntelGPU lines in fatal window ──"
log show --start "$(date '+%Y-%m-%d') 21:37:00" --end "$(date '+%Y-%m-%d') 21:47:30" --style compact --predicate \
 'processID == 0 AND eventMessage CONTAINS "(MyIntelGPU)"' 2>/dev/null \
 | grep -E "DISC|newUserClient|AccelClient|Accelerator|Phase 6c|probe" | head -40

echo "── B) DISC sel lines anywhere today ──"
log show --start "$(date '+%Y-%m-%d') 20:00:00" --style compact --predicate \
 'eventMessage CONTAINS "DISC sel"' 2>/dev/null | head -20

echo "── C) fresh WS crash signature check ──"
CRASH=$(ls -t /Library/Logs/DiagnosticReports/WindowServer-2026-08-23-214*.ips 2>/dev/null | head -1)
if [ -n "$CRASH" ]; then
  echo "file: $CRASH"
  grep -o '"exception"[^}]*}' "$CRASH" | head -2
  grep -o 'layer_blit[a-z_]*' "$CRASH" | sort | uniq -c
fi
