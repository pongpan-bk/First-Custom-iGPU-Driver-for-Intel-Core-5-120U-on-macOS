#!/bin/bash
# Token-exact search across ALL history (RTC-skew proof)
echo "── A) SURFACE handshake transcript (all history) ──"
log show --style compact --predicate \
 'eventMessage CONTAINS "SURFACE SetIDMode" OR eventMessage CONTAINS "SURFACE SetShapeBacking" OR eventMessage CONTAINS "SURFACE backing"' 2>/dev/null \
 | head -45

echo "── B) DISC sel transcript tail ──"
log show --style compact --predicate \
 'eventMessage CONTAINS "DISC sel"' 2>/dev/null | tail -15

echo "── C) newest WS crash signature ──"
CRASH=$(ls -t /Library/Logs/DiagnosticReports/WindowServer*.ips 2>/dev/null | head -1)
echo "file: $CRASH"
grep -o 'KERN_INVALID_ADDRESS at 0x[0-9a-f]*' "$CRASH" | head -1
grep -c "layer_blit" "$CRASH"
