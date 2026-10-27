#!/bin/bash
# ============================================================================
#  09-vcs-decode.command — Build VCS harness + run decode test
# ============================================================================
set -uo pipefail

HARNESS_DIR="/Users/ppbk/Documents/src/VCSMediaHarness"
REPO="/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"
CLIP="$HARNESS_DIR/test_clip.h264"
OUT="/tmp/vcs_test_out.nv12"
BIN="/tmp/vcs_test_harness"
RUN_LOG="/tmp/vcs_harness_run.log"
BUILD_LOG="/tmp/vcs_build.log"

echo "============================================"
echo "  09-vcs-decode — VCS Media Test"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

# 1. Check kext loaded
echo ""
echo "[1/5] kext loaded?"
kmutil showloaded --list-only 2>/dev/null | grep -i myintel | head -3
[ $? -ne 0 ] && echo "  ❌ kext not loaded" && exit 1

# 2. Version lock
echo ""
echo "[2/5] Version lock"
IV=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "/Library/Extensions/MyIntelGPU.kext/Contents/Info.plist" 2>/dev/null)
RV=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$REPO/MyIntelGPU.kext/Contents/Info.plist" 2>/dev/null)
echo "  Installed: $IV | Repo: $RV"
[ "$IV" = "$RV" ] && echo "  ✅ match" || echo "  ⚠️ version mismatch — testing stale kext"

# 3. IORegistry VCS nodes
echo ""
echo "[3/5] VCS IORegistry nodes"
ioreg -l -w0 2>/dev/null | grep -oE '"MyIntelVCSNub"|"MyIntelAccelerator"|"MyIntelGPUClient"|"MyIntelVCSClient"' | sort | uniq -c

# 4. Build harness
echo ""
echo "[4/5] Build harness"
cd "$HARNESS_DIR" || exit 1
rm -f "$BIN"
clang -x objective-c -O2 \
  -framework IOKit -framework CoreFoundation -framework Foundation \
  test_submit_vcs.m MyIntelVCSCommand.cpp \
  -o "$BIN" 2>"$BUILD_LOG" || { echo "  ❌ build fail"; tail -15 "$BUILD_LOG"; exit 1; }
echo "  ✅ Built $BIN ($(stat -f%z "$BIN") bytes)"

# 5. Run decode
echo ""
echo "[5/5] Run decode"
rm -f "$RUN_LOG"
"$BIN" "$CLIP" "$OUT" >"$RUN_LOG" 2>&1
RC=$?
tail -25 "$RUN_LOG"
echo ""
echo "  Exit code: $RC"
if [ -f "$OUT" ]; then
  SZ=$(stat -f%z "$OUT")
  echo "  Output: $SZ bytes"
  MD5=$(md5 -q "$OUT")
  echo "  MD5: $MD5"
  echo "  Golden(ubuntu): $(md5 -q "$REPO/from-kali/ubuntu-igpu-20261016-025703/golden_ubuntu.nv12" 2>/dev/null)"
fi

echo ""
echo "============================================"
echo "  VCS test complete (rc=$RC)"
echo "============================================"
exit $RC
