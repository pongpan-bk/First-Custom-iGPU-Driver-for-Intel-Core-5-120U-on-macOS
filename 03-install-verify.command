#!/bin/bash
# ============================================================================
#  03-install-verify.command — Post-install: kext loaded, version, hash
# ============================================================================
set -uo pipefail

SRC="/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"

echo "============================================"
echo "  03-install-verify — Post-install check"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

ok()  { echo "  [OK] $1"; }
warn(){ echo "  [!!] $1"; }
fail(){ echo "  [XX] $1"; }

# 1. Kext loaded
echo ""
echo "[1] Kext loaded?"
LOADED=$(kmutil showloaded --list-only 2>/dev/null | grep -i "MyIntelGPU")
if [ -n "$LOADED" ]; then
  ok "Loaded: $LOADED"
  LVER=$(echo "$LOADED" | sed -E 's/.*\(([0-9.]+)\).*/\1/')
else
  fail "MyIntelGPU kext NOT loaded"
  LVER=""
fi

# 2. Version match: disk vs loaded
echo ""
echo "[2] Version match"
KEXT=/Library/Extensions/MyIntelGPU.kext
DPLIST="$KEXT/Contents/Info.plist"
if [ -f "$DPLIST" ]; then
  DVER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$DPLIST" 2>/dev/null)
  echo "    Installed version: $DVER"
  [ "$DVER" = "$LVER" ] && ok "Loaded version = installed version ($DVER)" || warn "Loaded ($LVER) ≠ installed ($DVER) — reboot needed"
else
  fail "No kext at /Library/Extensions"
fi

# 3. Hash match: installed vs source build
echo ""
echo "[3] Binary hash (installed vs source)"
if [ -f "$KEXT/Contents/MacOS/MyIntelGPU" ]; then
  IHASH=$(shasum -a 256 "$KEXT/Contents/MacOS/MyIntelGPU" 2>/dev/null | cut -c1-16)
  SHASH=$(shasum -a 256 "$SRC/MyIntelGPU.kext/Contents/MacOS/MyIntelGPU" 2>/dev/null | cut -c1-16)
  echo "    Installed: $IHASH"
  echo "    Source:    $SHASH"
  [ "$IHASH" = "$SHASH" ] && ok "Binary matches source build" || warn "Binary differs from source"
else
  fail "Installed binary missing"
fi

# 4. Metal bundle check
echo ""
echo "[4] Metal bundle (PlugIns)"
BUNDLE="$KEXT/Contents/PlugIns/MyIntelGPUMTLDriver.bundle"
if [ -d "$BUNDLE" ]; then
  BID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$BUNDLE/Contents/Info.plist" 2>/dev/null)
  echo "    Bundle ID: $BID"
  [ "$BID" = "com.pongpan-bk.MyIntelGPUMTLDriver" ] && ok "Bundle ID correct" || warn "Bundle ID: $BID"
  BHASH=$(shasum -a 256 "$BUNDLE/Contents/MacOS/MyIntelGPUMTLDriver" 2>/dev/null | cut -c1-16)
  echo "    Hash: $BHASH"
else
  fail "No Metal bundle in PlugIns — Metal will not find driver!"
fi

# 5. auxKC check
echo ""
echo "[5] auxKC"
AUX=/Library/KernelCollections/AuxiliaryKernelExtensions.kc
if [ -f "$AUX" ]; then
  ok "auxKC present ($(ls -la "$AUX" | awk '{print $5}') bytes)"
  kmutil showloaded --list-only 2>/dev/null | grep -i MyIntelGPU | sed 's/^/    /'
else
  warn "No auxKC — kext may be prelinked into system kernel"
fi

echo ""
echo "============================================"
echo "  Install verify complete"
echo "============================================"
