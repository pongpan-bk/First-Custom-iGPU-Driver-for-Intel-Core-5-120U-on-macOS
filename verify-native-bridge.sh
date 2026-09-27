#!/bin/sh
# verify-native-bridge.sh — no-plugin contract for the pure native Mac driver.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE="$ROOT/MyIntelAccelerator.cpp"
INFO="$ROOT/Info.plist"
FAIL=0

printf '%s\n' '=== MyIntelGPU native bridge contract ==='

# The project intentionally has no userspace GL/Metal bundle. Advertising a
# bundle name that is not shipped makes macOS probe a renderer that cannot
# exist, so the native bridge must not publish those names.
if grep -nE 'setProperty\("(MetalPluginName|MetalPluginClassName|IOGLBundleName)"' "$SOURCE"; then
    printf '%s\n' '[FAIL] active nonexistent renderer/plugin advertisement found'
    FAIL=1
else
    printf '%s\n' '[OK] no active nonexistent renderer/plugin advertisement'
fi

# The kext plist must remain a pure kernel extension and must not gain a
# PlugIns key or an OSBundleRequired requirement.
if grep -nE '<key>(PlugIns|OSBundleRequired)</key>' "$INFO"; then
    printf '%s\n' '[FAIL] Info.plist contains forbidden plugin/root packaging key'
    FAIL=1
else
    printf '%s\n' '[OK] Info.plist remains pure kernel extension metadata'
fi

# A produced standalone bundle must not silently depend on a userspace plugin.
if [ -d "$ROOT/MyIntelGPU.kext/Contents/PlugIns" ]; then
    printf '%s\n' '[FAIL] standalone build unexpectedly contains Contents/PlugIns'
    FAIL=1
else
    printf '%s\n' '[OK] standalone build has no Contents/PlugIns dependency'
fi

if [ "$FAIL" -ne 0 ]; then
    printf '%s\n' 'RESULT: FAIL'
    exit 1
fi

printf '%s\n' 'RESULT: PASS'
