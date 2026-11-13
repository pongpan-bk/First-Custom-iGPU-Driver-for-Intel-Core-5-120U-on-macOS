#!/bin/bash
# MyIntelGPU VCS Test Suite — runs the checks matching the current work
# Reads the version of the actually installed kext (CFBundleVersion) and the SHA256
# of the binaries (installed kext + freshly built harness) at runtime on every run,
# so no fixed version/hash is pinned in comments anymore.
# (The old text referenced kext 3.1.41+ / binary c53fafd1 — outdated, never use as evidence.)
# 2026-10-05: harness moved into the repo under proof/ — all paths derive from the script's location

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

HARNESS_DIR="$REPO/proof"
CLIP="$HARNESS_DIR/test_clip.h264"
OUT=/tmp/vcs_test_out.nv12
BIN=/tmp/vcs_test_harness
RUN_LOG="/tmp/vcs_harness_run.log"
KEXT_LOG="/tmp/vcs_kext_log.txt"
BUILD_LOG="/tmp/vcs_build.log"

INSTALLED_PLIST="/Library/Extensions/MyIntelGPU.kext/Contents/Info.plist"
INSTALLED_BIN="/Library/Extensions/MyIntelGPU.kext/Contents/MacOS/MyIntelGPU"
REPO_PLIST="$REPO/MyIntelGPU.kext/Contents/Info.plist"
REPO_BIN="$REPO/MyIntelGPU.kext/Contents/MacOS/MyIntelGPU"
HARNESS_SRC="$HARNESS_DIR/MyIntelVCSCommand.cpp"
REPO_SRC="$REPO/MyIntelVCSCommand.cpp"

read_ver() { /usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$1" 2>/dev/null || echo "(unreadable)"; }
sha256()   { if [ -f "$1" ]; then shasum -a 256 "$1" | awk '{print $1}'; else echo "(missing)"; fi; }

echo "=============================================="
echo "  MyIntelGPU VCS Test Suite"
echo "=============================================="

# ── 1. Check kext loaded (kmutil — kextstat was removed) ──────────
echo ""
echo "[1/7] Kext loaded? (kmutil showloaded)"
KEXT_LINE="$(kmutil showloaded 2>/dev/null | grep -i myintel)"
if [ -z "$KEXT_LINE" ]; then
  echo "  ❌ kext NOT loaded (MyIntelGPU not found via kmutil showloaded)"
  exit 1
fi
echo "  $KEXT_LINE"

# ── 2. Version / provenance lock ─────────────────────────────────
echo ""
echo "[2/7] Version / provenance lock (installed vs repo build)"
INSTALLED_VER="$(read_ver "$INSTALLED_PLIST")"
REPO_VER="$(read_ver "$REPO_PLIST")"
INSTALLED_SHA="$(sha256 "$INSTALLED_BIN")"
REPO_SHA="$(sha256 "$REPO_BIN")"
echo "  installed kext CFBundleVersion : $INSTALLED_VER"
echo "  installed kext binary SHA256   : $INSTALLED_SHA"
echo "  repo build  CFBundleVersion     : $REPO_VER"
echo "  repo build  binary SHA256       : $REPO_SHA"
if [ "$INSTALLED_VER" = "$REPO_VER" ] && [ "$INSTALLED_SHA" = "$REPO_SHA" ]; then
  echo "  ✅ installed kext matches the current source build"
else
  NEWEST="$(printf '%s\n%s\n' "$INSTALLED_VER" "$REPO_VER" | sort -V | tail -1)"
  if [ "$NEWEST" = "$REPO_VER" ] && [ "$INSTALLED_VER" != "$REPO_VER" ]; then
    echo "  ⚠️  WARNING: installed kext version ($INSTALLED_VER) is OLDER than the repo build ($REPO_VER)"
    echo "  ⚠️  WARNING: the loaded kext does NOT match the current source build —"
    echo "  ⚠️  WARNING: the harness will be testing STALE kernel code."
  else
    echo "  ⚠️  WARNING: installed kext ($INSTALLED_VER / $INSTALLED_SHA) does NOT match"
    echo "  ⚠️  WARNING: the current source build ($REPO_VER / $REPO_SHA) — STALE kernel code."
  fi
fi
echo "  (warning only, does not stop the test — still need to know which kext is loaded)"

# ── 3. IORegistry nodes ──────────────────────────────────────────
echo ""
echo "[3/7] IORegistry nodes (VCS/Accel)"
ioreg -l -w0 2>/dev/null | grep -oE '"MyIntelVCSNub"|"MyIntelAccelerator"|"MyIntelGPUClient"|"MyIntelVCSClient"' | sort | uniq -c

# ── 4. Kext log — sender-based predicate so records cannot get lost ──
echo ""
echo '[4/7] Kext log: predicate sender == "MyIntelGPU" (--last 20m)'
log show --last 20m --info --predicate 'sender == "MyIntelGPU"' >"$KEXT_LOG" 2>/dev/null
KEXT_LOG_LINES="$(wc -l <"$KEXT_LOG" | tr -d ' ')"
echo "  raw log: $KEXT_LOG ($KEXT_LOG_LINES lines)"
tail -20 "$KEXT_LOG"

# ── 5. Source-sync check (harness copy vs repo file) ─────────────
echo ""
echo "[5/7] Source-sync check: harness MyIntelVCSCommand.cpp vs repo"
if cmp -s "$HARNESS_SRC" "$REPO_SRC"; then
  echo "  ✅ in sync (identical bytes):"
  echo "      $HARNESS_SRC"
  echo "      $REPO_SRC"
else
  echo "  ⚠️  WARNING: SOURCE DIVERGED — the two MyIntelVCSCommand.cpp files differ:"
  echo "      $HARNESS_SRC"
  echo "      $REPO_SRC"
  echo "  ⚠️  WARNING: the harness will silently test STALE command code."
  cmp "$HARNESS_SRC" "$REPO_SRC" 2>&1 | head -3
fi

# ── 6. Build harness (forced fresh build every run) ───────────────
echo ""
echo "[6/7] Build harness (FORCED fresh build — never reuse /tmp/vcs_test_harness)"
cd "$HARNESS_DIR" || exit 1
rm -f "$BIN"
clang++ -x objective-c++ -std=c++14 -O2 \
  -framework IOKit -framework CoreFoundation -framework Foundation \
  test_submit_vcs.m MyIntelVCSCommand.cpp \
  -o "$BIN" 2>"$BUILD_LOG" || { echo "  ❌ build fail"; tail -5 "$BUILD_LOG"; exit 1; }
echo "  ✅ built fresh at $(date '+%Y-%m-%d %H:%M:%S') ($(stat -f%z "$BIN") bytes)"

# — build ownership: never leave root-owned artifacts behind for the user —
if [ "$(id -u)" -eq 0 ]; then
  OWNER="${SUDO_USER:-}"
  if [ -z "$OWNER" ] || ! id -u "$OWNER" >/dev/null 2>&1; then
    OWNER="ppbk"
  fi
  if chown "$OWNER:staff" "$BIN" 2>/dev/null; then
    echo "  ✅ build ran as root → chowned back to $OWNER:staff"
  else
    echo "  ⚠️ chown $OWNER:staff failed"
  fi
  chown "$OWNER:staff" "$BUILD_LOG" 2>/dev/null || true
else
  echo "  build ran as $(id -un) (not root — skipping chown)"
fi
echo "  ls -ln $BIN →"
ls -ln "$BIN"

HARNESS_SHA="$(sha256 "$BIN")"
echo "  harness binary SHA256 : $HARNESS_SHA"

# ── 7. Run the decode test ───────────────────────────────────────
echo ""
echo "[7/7] Run decode: $CLIP"
echo "  ── provenance recap (all 3 values) ──"
echo "  installed kext version : $INSTALLED_VER"
echo "  installed kext SHA256  : $INSTALLED_SHA"
echo "  harness binary SHA256  : $HARNESS_SHA"

rm -f "$RUN_LOG"
"$BIN" "$CLIP" "$OUT" >"$RUN_LOG" 2>&1
HARNESS_RC=$?
echo "  ── harness tail (last 25 lines) ──"
tail -25 "$RUN_LOG"

echo ""
echo "----------------------------------------------"
echo "  Scenario summary (machine-readable, verbatim):"
if grep -E '^\[(DATA-CHECK|GOLDEN|VERDICT|POISON-RUN|REGISTER-AUDIT)\]' "$RUN_LOG"; then
  :
else
  echo "  (no [DATA-CHECK]/[GOLDEN]/[VERDICT]/[POISON-RUN]/[REGISTER-AUDIT] lines found in the log)"
fi
echo "  raw harness log: $RUN_LOG"
if [ -f "$OUT" ]; then
  ls -la "$OUT" | awk '{print "  NV12 output:", $5, "bytes"}'
else
  echo "  (no NV12 output)"
fi
echo "  harness exit code: $HARNESS_RC (script exits with this code)"
echo "=============================================="
exit "$HARNESS_RC"
