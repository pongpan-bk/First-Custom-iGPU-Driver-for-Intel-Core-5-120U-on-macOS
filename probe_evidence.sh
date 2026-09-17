#!/bin/bash
# Mission C Phase 1 — post-reboot evidence capture (one shot)
# Output: source/probe-evidence-<timestamp>.log  (outputs live in project per rule)
OUT="/Users/ppbk/Documents/Default Project/source/probe-evidence-$(date +%Y%m%d-%H%M%S).log"
BT=$(sysctl -n kern.boottime 2>/dev/null | grep -o '{ sec = [0-9]*' | grep -o '[0-9]*')
BTS=$(date -r "$BT" '+%Y-%m-%d %H:%M:%S' 2>/dev/null)
{
echo "=== bootargs ==="; sysctl -n kern.bootargs 2>/dev/null
echo "=== uptime ==="; uptime
echo "=== WindowServer procs (restart count check) ==="; ps -axo pid,lstart,command | grep "[W]indowServer"
echo "=== fresh WS crash reports ==="; find "$HOME/Library/Logs/DiagnosticReports" -name "WindowServer*" -newermt "$BTS" 2>/dev/null
echo "=== accelerator node + props ==="; ioreg -rc MyIntelAccelerator 2>/dev/null | grep -E "MyIntelAccelerator <|MetalPlugin|IOGLBundle|IOAccelRevision|IOAccelTypes" || echo "(node absent)"
echo "=== who opened us (user clients) ==="; ioreg -rc MyIntelAccelerator 2>/dev/null | sed -n '/UserClient/p' | head
echo "=== DISC transcript + handshake since boot ($BTS) ==="
log show --start "$BTS" --style compact --predicate \
 'eventMessage CONTAINS[c] "myintelaccelerator" OR eventMessage CONTAINS[c] "DISC" OR eventMessage CONTAINS[c] "MyIntelGPUGLDriver" OR eventMessage CONTAINS[c] "MyIntelGPUMTLDriver"' 2>/dev/null \
 | grep -vE "Filtering|^Timestamp|com\.apple\.log:" | head -120
} > "$OUT" 2>&1
echo "saved: $OUT ($(wc -l < "$OUT") lines)"
