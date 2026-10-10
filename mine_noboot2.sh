#!/bin/bash
# Post-mortem: failed main-ESP boot (2.0.323 surface build)
echo "── 1) am I alive / which boot ──"
date '+%H:%M:%S'
sysctl -n kern.bootargs 2>/dev/null
uptime | awk -F'up' '{print $2}' | cut -c1-22

echo "── 2) reboot history ──"
last reboot 2>/dev/null | head -6

echo "── 3) OUR driver lines across ALL boots today 21:50+ ──"
log show --start "$(date '+%Y-%m-%d') 21:50:00" --style compact --predicate \
 'eventMessage CONTAINS "(MyIntelGPU)"' 2>/dev/null \
 | grep -E "DISC|SURFACE|newUserClient|Accelerator|NOT loadable|loadable" | head -50

echo "── 4) fresh system .ips ──"
ls -t /Library/Logs/DiagnosticReports/ 2>/dev/null | head -5

echo "── 5) WS launches today 21:50+ ──"
log show --start "$(date '+%Y-%m-%d') 21:50:00" --style compact --predicate \
 'process == "WindowServer"' 2>/dev/null | grep -icE "." 
