#!/bin/bash
# verify_bridge.sh — one-shot Phase-D verification after any mybridge boot
# Usage: sudo ./verify_bridge.sh [minutes]
MIN=${1:-5}

echo "╔══ PHASE-D BRIDGE VERIFY ($MIN min window) ══"
UP=$(uptime | awk '{print $3}')
echo "║ uptime: $UP"

KEXT=$(kmutil showloaded 2>/dev/null | grep pongpan | grep -oE '2\.0\.[0-9]+')
echo "║ kext: $KEXT"

ARGS=$(nvram boot-args 2>/dev/null)
echo "$ARGS" | grep -q mybridge && echo "║ mode: MIRROR ARMED" || echo "║ mode: dormant (no mybridge)"

TICKS=$(log show --last ${MIN}m --predicate 'process == "kernel" AND eventMessage CONTAINS "[mybridge] tick="' 2>/dev/null | grep -v "log:")
HW=$(echo "$TICKS" | grep -c "HW blit")
CPU=$(echo "$TICKS" | grep -c "CPU copy")
FLIPS=$(echo "$TICKS" | grep -c "flip✓")
FLIPX=$(echo "$TICKS" | grep -c "flip✗")
LAST=$(echo "$TICKS" | tail -1)

echo "╠══ RESULTS ══"
echo "║ HW blit ticks : $HW"
echo "║ CPU copy      : $CPU"
echo "║ flip ok/fail  : $((FLIPS-FLIPX)) / $FLIPX"
echo "║ last line     : $(echo "$LAST" | grep -oE '\(.*\)' | head -1)"

VERDICT="UNKNOWN"
if [ "$HW" -gt 0 ] && [ "$CPU" -eq 0 ] && [ "$FLIPX" -eq 0 ]; then
    VERDICT="🏆 PASS — sustained HW blit + clean flips"
elif [ "$CPU" -gt 0 ] && [ "$HW" -eq 0 ]; then
    VERDICT="⚠️ CPU-copy only (execlist emits failing)"
fi
echo "╠══ VERDICT ══"
echo "║ $VERDICT"
echo "╚═══════════════════════════════"
