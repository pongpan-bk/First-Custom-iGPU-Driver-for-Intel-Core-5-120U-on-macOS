#!/bin/bash
# ============================================================================
#  02-preflight.command — Pre-deploy config & Info.plist validation
# ============================================================================
set -uo pipefail

SRC="/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"
PLIST="$SRC/MyIntelGPU.kext/Contents/Info.plist"

echo "============================================"
echo "  02-preflight — Config & Info.plist"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

ok()  { echo "  [OK] $1"; }
warn(){ echo "  [!!] $1"; }
fail(){ echo "  [XX] $1"; }

# 1. Info.plist validation
echo ""
echo "[1] Info.plist checks"
if [ -f "$PLIST" ]; then
  BID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$PLIST" 2>/dev/null)
  echo "    CFBundleIdentifier: $BID"
  [ -n "$BID" ] && ok "Bundle ID present" || fail "No CFBundleIdentifier"

  VER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$PLIST" 2>/dev/null)
  SHRT=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$PLIST" 2>/dev/null)
  echo "    Version: $SHRT (build $VER)"

  OVR=$(/usr/libexec/PlistBuddy -c "Print :OSBundleRequired" "$PLIST" 2>/dev/null)
  if [ -n "$OVR" ]; then
    fail "OSBundleRequired present — causes 'Invalid Parameter' on macOS 14+"
  else
    ok "No OSBundleRequired"
  fi

  echo "    OSBundleLibraries:"
  /usr/libexec/PlistBuddy -c "Print :OSBundleLibraries" "$PLIST" 2>/dev/null | sed 's/^/      /'

  MIN=$(/usr/libexec/PlistBuddy -c "Print :LSMinimumSystemVersion" "$PLIST" 2>/dev/null)
  echo "    LSMinimumSystemVersion: $MIN"
else
  fail "Info.plist not found: $PLIST"
fi

# 2. Makefile flags audit
echo ""
echo "[2] Makefile CXXFLAGS audit"
MK="$SRC/Makefile"
if [ -f "$MK" ]; then
  echo "    --- Relevant flags ---"
  grep -E "CXXFLAGS|LDFLAGS|-fno-stack|-mkernel|-mmacosx|-fapple-kext|-Xlinker" "$MK" | sed 's/^/      /'
  grep -q "\-fno-stack-protector" "$MK" && ok "-fno-stack-protector present" || fail "Missing -fno-stack-protector"
  grep -q "\-mkernel" "$MK" && ok "-mkernel present" || fail "Missing -mkernel"
  grep -q "\-mmacosx-version-min" "$MK" && ok "macOS version min set" || fail "Missing -mmacosx-version-min"
  grep -q "\-undefined dynamic_lookup" "$MK" && fail "Uses -undefined dynamic_lookup (forbidden)" || ok "No dynamic_lookup"
  grep -q "\-Xlinker -kext" "$MK" && ok "-Xlinker -kext present" || fail "Missing -Xlinker -kext in LDFLAGS"
  grep -q "\-fapple-kext" "$MK" && ok "-fapple-kext present" || warn "Missing -fapple-kext (may need for __TEXT_EXEC)"
else
  fail "Makefile not found"
fi

# 3. EFI config.plist kext entry
echo ""
echo "[3] EFI config.plist kext entry"
EFI="/Volumes/EFI-MAIN/EFI/OC/config.plist"
if [ -f "$EFI" ]; then
  EN=$(/usr/libexec/PlistBuddy -c "Print :Kernel:Add" "$EFI" 2>/dev/null | grep -c MyIntelGPU)
  [ "$EN" -gt 0 ] && ok "MyIntelGPU.kext in Kernel:Add" || fail "MyIntelGPU.kext NOT in Kernel:Add"
  BA=$(/usr/libexec/PlistBuddy -c "Print :NVRAM:Add:7C436110-AB2A-4BBB-A880-FE41995C9F82:boot-args" "$EFI" 2>/dev/null)
  echo "    EFI boot-args: $BA"
else
  warn "EFI not mounted at /Volumes/EFI-MAIN"
fi

echo ""
echo "============================================"
echo "  Preflight complete"
echo "============================================"
