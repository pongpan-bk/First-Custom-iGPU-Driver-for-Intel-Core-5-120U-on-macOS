#!/bin/bash
# ============================================================================
#  07-display-edid.command — EDID, dsize, IODisplayConnectFlags, overrides
# ============================================================================
set -uo pipefail

echo "============================================"
echo "  07-display-edid — EDID / Display"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

ok()  { echo "  [OK] $1"; }
warn(){ echo "  [!!] $1"; }
fail(){ echo "  [XX] $1"; }

# 1. IODisplayConnect live state
echo ""
echo "[1] IODisplayConnect state"
ioreg -lw0 -c IODisplayConnect 2>/dev/null | grep -E '"IODisplayEDID"|"IODisplayConnectFlags"|"DisplayVendorID"|"DisplayProductID"|"ioDisplayLocation"' | sed 's/^/    /'

# 2. AppleDisplay
echo ""
echo "[2] AppleDisplay"
ioreg -lw0 -c AppleDisplay 2>/dev/null | grep -E '"DisplayVendorID"|"DisplayProductID"|"DisplayProductName"|"IODisplayEDID"' | sed 's/^/    /'

# 3. dsize check
echo ""
echo "[3] dsize panel size check"
DSRAW=$(~/Desktop/edid-tools/dsize 2>/dev/null)
if [ -n "$DSRAW" ]; then
  echo "    $DSRAW" | sed 's/^/    /'
  DSIN=$(echo "$DSRAW" | grep -oE -- '-> [0-9.]+' | awk '{print $2}')
  if [ -n "$DSIN" ]; then
    case "$DSIN" in
      15.*|16.*) ok "Panel = ${DSIN}\" — EDID correct" ;;
      30.*|29.*) fail "Panel = ${DSIN}\" — fallback EDID" ;;
      *) warn "Panel = ${DSIN}\"" ;;
    esac
  fi
else
  warn "dsize not found at ~/Desktop/edid-tools/dsize"
fi

# 4. IODisplayConnectFlags
echo ""
echo "[4] IODisplayConnectFlags"
FLG=$(ioreg -lw0 -c IODisplayConnect 2>/dev/null | grep -oE '"IODisplayConnectFlags" = <[0-9a-f]+>' | head -1)
echo "    $FLG"
case "$FLG" in
  *00000000*) warn "Flags=0 — not built-in (0x800)" ;;
  *00000800*|*80000000*) ok "Built-in flag set" ;;
  *) ;;
esac

# 5. Overrides plist
echo ""
echo "[5] EDID overrides plist"
OV="/Library/Displays/Contents/Resources/Overrides/DisplayVendorID-2c82/DisplayProductID-924"
if [ -f "$OV" ]; then
  ok "Override plist found"
  plutil -p "$OV" 2>/dev/null | head -15 | sed 's/^/    /'
else
  warn "No override plist at $OV"
fi

# 6. System Profiler
echo ""
echo "[6] System Profiler — Displays"
system_profiler SPDisplaysDataType 2>/dev/null | grep -E "Resolution|UI Looks|VRAM|Depth" | sed 's/^/    /'

echo ""
echo "============================================"
echo "  Display/EDID check complete"
echo "============================================"
