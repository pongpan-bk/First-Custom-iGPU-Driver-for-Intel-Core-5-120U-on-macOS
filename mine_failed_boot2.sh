#!/bin/bash
# Refined mining: exact tokens only + failed-boot window analysis
echo "── A) exact DISC transcript (kernel+any process, today 20:00+) ──"
log show --start "$(date '+%Y-%m-%d') 20:00:00" --style compact --predicate \
 'eventMessage CONTAINS "DISC sel" OR eventMessage CONTAINS "newUserClient" OR eventMessage CONTAINS "AccelClient"' 2>/dev/null \
 | grep -vE "Filtering|^Timestamp" | head -40

echo "── B) WindowServer launches during failed-boot window (21:35-21:47) ──"
log show --start "$(date '+%Y-%m-%d') 21:35:00" --end "$(date '+%Y-%m-%d') 21:47:00" --style compact --predicate \
 'eventMessage CONTAINS[c] "windowserver" AND (eventMessage CONTAINS[c] "launch" OR eventMessage CONTAINS[c] "crash" OR eventMessage CONTAINS[c] "exit")' 2>/dev/null \
 | head -15

echo "── C) kernel panics / early-boot stops in that window ──"
log show --start "$(date '+%Y-%m-%d') 21:35:00" --end "$(date '+%Y-%m-%d') 21:47:00" --style compact --predicate \
 'processID == 0 AND (eventMessage CONTAINS[c] "panic" OR eventMessage CONTAINS[c] "MyIntelGPU")' 2>/dev/null \
 | head -25

echo "── D) system-level crash reports ──"
ls -t /Library/Logs/DiagnosticReports/ 2>/dev/null | head -6
