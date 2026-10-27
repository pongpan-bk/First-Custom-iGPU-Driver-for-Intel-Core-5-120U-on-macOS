#!/bin/bash
# ============================================================================
#  08-log-forensics.command — Kernel log dump, IPC audit, sandbox check
# ============================================================================
set -uo pipefail

echo "============================================"
echo "  08-log-forensics — Kernel / IPC logs"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

sudo -v 2>/dev/null && echo "(sudo OK)" || echo "(sudo FAILED — logs may be empty)"

# 1. MyIntelGPU kernel log (1 hour)
echo ""
echo "[1] MyIntelGPU kernel log (last 1h)"
TMP=$(mktemp)
sudo log show --last 1h --predicate 'sender == "MyIntelGPU"' --style compact > "$TMP" 2>/dev/null
LINES=$(wc -l < "$TMP" | tr -d ' ')
echo "    $LINES lines captured"

echo ""
echo "  --- EDID inject ---"
grep -iE "EDID|injected|KDB0924" "$TMP" | tail -5 | sed 's/^/    /'

echo ""
echo "  --- Ring/ELSP ---"
grep -iE "kick|ELSP|head|idle|drained" "$TMP" | tail -8 | sed 's/^/    /'

echo ""
echo "  --- HW Accel/Media ---"
grep -iE "HW Accel|VDBOX|VEBOX|VCS|VDBOX\[" "$TMP" | tail -8 | sed 's/^/    /'

echo ""
echo "  --- Errors ---"
grep -iE "error|fail|panic|Invalid" "$TMP" | tail -6 | sed 's/^/    /'

rm -f "$TMP"

# 2. IPC security audit (30 min)
echo ""
echo "[2] IPC security audit (WindowServer/VTDecoder — 30m)"
sudo log show --last 30m --predicate 'process == "WindowServer" or process == "VTDecoderXPCService" or process == "kernel"' --style compact 2>/dev/null \
  | grep -iE "MyIntel|PROBE|PASS-THROUGH|DISC sel|crashed|corpses|denied|unsupported type" | tail -10 | sed 's/^/    /'
echo "    (empty = no issues)"

# 3. Sandbox audit
echo ""
echo "[3] Sandbox / MyIntelGPU deny (30m)"
sudo log show --last 30m --predicate 'process == "kernel" and composedMessage contains "Sandbox" and composedMessage contains "MyIntelGPU"' --style compact 2>/dev/null | tail -4 | sed 's/^/    /'
echo "    (empty = no denies)"

echo ""
echo "============================================"
echo "  Log forensics complete"
echo "============================================"
