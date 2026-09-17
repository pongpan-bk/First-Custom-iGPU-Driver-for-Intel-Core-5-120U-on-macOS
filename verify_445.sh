#!/bin/bash
# verify_445.sh — one-shot post-boot collector for the single-chain test
# Usage: ./verify_445.sh > /tmp/opencode/v445_report.txt 2>&1
echo "════ 2.0.445 SINGLE-CHAIN VERIFICATION ════"
echo "boot: $(sysctl -n kern.boottime | grep -oE '[0-9]+ [0-9:]+ 2026') · up: $(uptime | awk '{print $3,$4}')"
echo "loaded: $(kmutil showloaded 2>/dev/null | grep pongpan | grep -oE '2\.0\.[0-9]+')"
echo ""
echo "── [1] self-open ──"
log show --last 10m --predicate 'process == "kernel"' 2>/dev/null | grep -oE "self-open -> [^ ]*|display connect.*" | head -3
echo ""
echo "── [2] FB subtree (single chain?) ──"
ioreg -n MyIntelFramebuffer -r -d 3 2>/dev/null | grep -E "\+-o|DisplayProductID|busy" | head -12
echo ""
echo "── [3] connect count (ต้อง = 1) ──"
ioreg -n MyIntelFramebuffer -r -d 3 2>/dev/null | grep -c "display0"
echo ""
echo "── [4] EDID on our chain ──"
ioreg -n MyIntelFramebuffer -r -d 3 2>/dev/null | grep -c "IODisplayEDID"
echo ""
echo "── [5] CoreDisplay errors ──"
log show --last 8m --predicate 'process == "WindowServer"' 2>/dev/null | grep -c "Setting main display"
echo "(ต่ำ = ดี)"
echo ""
echo "── [6] presentment ──"
log show --last 8m --predicate 'eventMessage CONTAINS "ndevs" OR eventMessage CONTAINS "IOPresentmentCreate"' 2>/dev/null | grep -oE "ndevs [0-9]+ \([^)]*\)|IOPresentmentCreate[^:]*" | head -5
echo ""
echo "── [7] health ──"
pgrep -x WindowServer >/dev/null && echo "WS ALIVE ✓" || echo "WS DEAD ✗"
ioreg -l 2>/dev/null | grep -oE '"MyIntel(Framebuffer|Accelerator)"=[0-9]+' | tr '\n' ' '
echo ""
find ~/Library/Logs/DiagnosticReports /Library/Logs/DiagnosticReports -newermt "-8 minutes" -name "*.ips" ! -path "*Chrome*" 2>/dev/null | head -3
echo "════ END ════"
