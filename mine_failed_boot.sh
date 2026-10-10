#!/bin/bash
# Mine the failed-boot logs: DISC transcript + handshake evidence
echo "── 1) live state ──"
sysctl -n kern.bootargs 2>/dev/null
uptime | awk -F'up' '{print $2}' | cut -c1-20
kmutil showloaded 2>/dev/null | grep myintelgpu | awk '{print $NF}'

echo "── 2) recent reboots ──"
last reboot 2>/dev/null | head -5

echo "── 3) GOLD: DISC/handshake lines (any time today) ──"
log show --start "$(date -v-6H '+%Y-%m-%d %H:%M:%S')" --style compact --predicate \
 'eventMessage CONTAINS[c] "myintelaccelerator" OR eventMessage CONTAINS[c] "DISC" OR eventMessage CONTAINS[c] "MyIntelGPUGLDriver"' 2>/dev/null \
 | grep -vE "Filtering|^Timestamp|com\.apple\.log:" | head -60

echo "── 4) WS crash .ips ใหม่สุด ──"
ls -t "$HOME/Library/Logs/DiagnosticReports/" 2>/dev/null | grep -i window | head -4
