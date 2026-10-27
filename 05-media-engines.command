#!/bin/bash
# ============================================================================
#  05-media-engines.command — VDBOX/VEBOX detection, clock gating, VideoToolbox
# ============================================================================
set -uo pipefail

echo "============================================"
echo "  05-media-engines — VDBOX/VEBOX check"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

# 1. VDBOX/VEBOX from kernel log
echo ""
echo "[1] VDBOX/VEBOX detection (kernel log)"
log show --predicate 'sender == "MyIntelGPU"' --last 30m --info 2>/dev/null | grep -iE "VDBOX|VEBOX|VCS|media|clock.*gating|IECP|MFXPIPE" | tail -15 | sed 's/^/    /'
[ $? -ne 0 ] && echo "    (no media engine logs in last 30m)"

# 2. Media properties in IORegistry
echo ""
echo "[2] Media properties (IORegistry)"
ioreg -l 2>/dev/null | grep -iE "AV1Decoding|HEVC|H264|VP9|video.*toolbox|MyIntelMedia|media" | head -10 | sed 's/^/    /'

# 3. VCS/VDBOX nub nodes
echo ""
echo "[3] VCS / MyIntelVCSNub nodes"
ioreg -l -w0 2>/dev/null | grep -oE '"MyIntelVCSNub"|"MyIntelAccelerator"|"MyIntelGPUClient"|"MyIntelVCSClient"' | sort | uniq -c | sed 's/^/    /'

# 4. VideoToolbox frameworks
echo ""
echo "[4] VideoToolbox frameworks"
ls /System/Library/Frameworks/VideoToolbox.framework 2>/dev/null && echo "    VideoToolbox.framework present" || echo "    VideoToolbox not found"
ls /System/Library/Frameworks/CoreMedia.framework 2>/dev/null && echo "    CoreMedia.framework present" || echo "    CoreMedia not found"

echo ""
echo "============================================"
echo "  Media engines check complete"
echo "============================================"
