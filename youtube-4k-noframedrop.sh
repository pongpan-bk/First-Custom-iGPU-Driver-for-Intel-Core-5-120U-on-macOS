#!/bin/bash
# youtube-4k-noframedrop.sh — YouTube 4K smooth-playback toolkit for Core 5 120U Hackintosh
# Honest status: MyIntelGPU VDBOX H.264 HW decode is currently FAIL-NODATA
# (README 2026-10-02: poison=2091, golden diff 97.56%). So this script does NOT
# claim HW decode. It makes 4K watchable TODAY via CPU-efficient path:
#   force H.264 (not VP9/AV1) + correct browser flags + power/display checks
#   + measurable dropped-frame methodology.
#
# Usage:
#   ./youtube-4k-noframedrop.sh --check            # diagnose only (no changes)
#   ./youtube-4k-noframedrop.sh --tune             # diagnose + write tune profile (no sudo)
#   ./youtube-4k-noframedrop.sh --measure [sec]    # local 4K CPU-decode benchmark (needs ffmpeg)
#   ./youtube-4k-noframedrop.sh --help
#
# Exit: 0 = check complete (even with WARNs), 2 = bad usage, 3 = benchmark failed.
set -u

REPORT="${TMPDIR:-/tmp}/youtube-4k-check.txt"
PROFILE="$HOME/youtube-4k-profile.txt"
WARN=0

say()  { printf '%s\n' "$*"; }
warn() { printf 'WARN: %s\n' "$*"; WARN=$((WARN+1)); }
ok()   { printf 'OK: %s\n' "$*"; }
info() { printf 'INFO: %s\n' "$*"; }

usage() {
  sed -n '1,20p' "$0"
  echo "Report: $REPORT"
}

check_os() {
  say "== [1] OS / CPU =="
  sw_vers 2>/dev/null | head -3 || warn "sw_vers missing"
  sysctl -n machdep.cpu.brand_string 2>/dev/null || warn "cpu brand unknown"
  sysctl -n hw.ncpu 2>/dev/null | xargs -I{} echo "ncpu={}" || true
}

check_display() {
  say "== [2] Display (need 3840x2160@60 for real 4K, else downscale hides drops) =="
  if command -v system_profiler >/dev/null 2>&1; then
    system_profiler SPDisplaysDataType 2>/dev/null | grep -iE "Resolution|Refresh|Online|Renderer|Metal" | head -12 || warn "no display info"
  else
    warn "system_profiler missing"
  fi
}

check_kext() {
  say "== [3] MyIntelGPU kext (informational — 4K path below does NOT depend on it) =="
  if kmutil showloaded 2>/dev/null | grep -iq "myintel"; then
    ok "MyIntelGPU loaded: $(kmutil showloaded 2>/dev/null | grep -i myintel | head -1)"
  else
    info "MyIntelGPU not loaded — expected on most boots; 4K tuning still applies (CPU path)"
  fi
  if ioreg -rc MyIntelAccelerator 2>/dev/null | grep -q "MyIntelAccelerator"; then
    ok "MyIntelAccelerator node present"
  else
    info "MyIntelAccelerator absent — VideoToolbox HW from this kext unavailable (see README FAIL-NODATA)"
  fi
  # VideoToolbox Apple-side HW (Intel QuickSync on real Macs; missing here) — report only
  if command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -hide_banner -hwaccels 2>/dev/null | tr '\n' ' ' | sed 's/^/ffmpeg hwaccels: /' || true
  else
    warn "ffmpeg not found — install via 'brew install ffmpeg' for --measure and HW probe"
  fi
}

check_browser() {
  say "== [4] Browsers (YouTube 4K codec path) =="
  for app in "Google Chrome" Safari Firefox "Microsoft Edge" Arc; do
    if [ -d "/Applications/${app}.app" ]; then ok "found ${app}"; else info "not found: ${app}"; fi
  done
  say "--"
  say "Truth: YouTube 4K defaults to VP9/AV1. On this Hackintosh neither is HW-accelerated"
  say "(Apple never shipped Gen12 drivers; our VDBOX VP9/AV1 path is not wired to VideoToolbox)."
  say "Smooth 4K TODAY = force H.264 4K via extension, decode on CPU (10C/12T 120U can do it)."
  say "Extensions: Chrome 'enhanced-h264ify' (block VP9+AV1, allow H.264) — then pick 2160p."
  say "Verify codec: right-click video > 'Stats for nerds' > Mime Type must show avc1, NOT vp09/av01."
}

check_power() {
  say "== [5] Power / contention (top framedrop causes after wrong codec) =="
  pmset -g batt 2>/dev/null | head -3 || info "pmset unavailable"
  say "Close: Xcode builds, Photos analysis, Time Machine backup during test."
  say "Plug in AC. Set display sleep > 30 min for the test window."
}

write_profile() {
  {
    echo "# YouTube 4K no-framedrop profile — $(date)"
    echo "# Machine: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo unknown)"
    echo ""
    echo "## 1. Browser setup (do once)"
    echo "- Chrome: install 'enhanced-h264ify', settings: block VP9=ON, block AV1=ON, allow H.264=ON"
    echo "- chrome://flags: Preferred video codec = H.264 (if present in your build)"
    echo "- Keep ONE 4K tab. Disable other extensions during test."
    echo ""
    echo "## 2. Pre-play checklist"
    echo "- AC power, quit heavy apps, resolution: prefer 1920x1080 display scaling if 4K panel stutters"
    echo "  (downscaled 4K->1080p still looks better than dropped 4K, and CPU load halves)"
    echo ""
    echo "## 3. Measure (the contract — 'Stats for nerds')"
    echo "- Play: https://www.youtube.com/watch?v=aqz-KE-bpKQ (Big Buck Bunny 4K 60fps, H.264 available)"
    echo "- Right-click > Stats for nerds. Watch 60s. PASS = Dropped Frames < 2% of decoded frames."
    echo "- Example PASS: 3600 decoded / 20 dropped. FAIL: hundreds dropped + 'Viewport 3840x2160 dropped'."
    echo "- chrome://media-internals > Players tab confirms 'video_codec: h264', 'hardware_decoder: false' (CPU, expected)."
    echo ""
    echo "## 4. If still dropping"
    echo "- Drop YouTube to 1440p (still H.264) — zero drops at 1440p beats slideshow at 2160p."
    echo "- Safari test: Safari prefers H.264/HEVC via VideoToolbox; compare drops Safari vs Chrome."
    echo "- Do NOT chase VP9/AV1 HW until MyIntelGPU poison=0 + golden diff=0 (see ULW_NOTEPAD.md S3)."
  } > "$PROFILE"
  ok "wrote $PROFILE"
}

do_measure() {
  sec="${1:-15}"
  say "== [6] Local 4K CPU-decode benchmark (${sec}s, H.264 3840x2160) =="
  command -v ffmpeg >/dev/null 2>&1 || { warn "ffmpeg required for --measure"; return 3; }
  f="/tmp/yt4k-test-4k-h264.mp4"
  if [ ! -f "$f" ]; then
    say "generating 4K H.264 sample (one-time, ~30s)..."
    ffmpeg -y -v error -f lavfi -i "testsrc2=size=3840x2160:rate=30:duration=${sec}" \
      -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p "$f" || { warn "sample gen failed"; return 3; }
  fi
  say "decoding (CPU, like YouTube H.264 path)..."
  # time decode; report fps — 120U CPU should sustain >=30fps for 4K H.264
  out=$(ffmpeg -v error -benchmark -i "$f" -f null - 2>&1 | tail -3)
  echo "$out"
  # crude fps extraction
  echo "$out" | grep -q "fps" && ok "decode ran — compare fps >= 30 (smooth) vs < 24 (will drop on YouTube 4K60)" || info "check fps line above"
}

main() {
  case "${1:---check}" in
    --check) : ;;
    --tune) : ;;
    --measure) : ;;
    -h|--help|help) usage; exit 0 ;;
    *) echo "bad arg: $1"; usage; exit 2 ;;
  esac
  : > "$REPORT"
  {
    check_os; echo
    check_display; echo
    check_kext; echo
    check_browser; echo
    check_power
  } | tee "$REPORT"
  if [ "$1" = "--tune" ]; then echo; write_profile; fi
  if [ "$1" = "--measure" ]; then echo; do_measure "${2:-15}"; rc=$?; [ $rc -ne 0 ] && exit $rc; fi
  echo
  if [ "$WARN" -gt 0 ]; then say "RESULT: CHECK-DONE with ${WARN} WARN(s) — see $REPORT"; else say "RESULT: CHECK-DONE clean — see $REPORT"; fi
  say "NEXT: --tune writes $PROFILE; play test URL and read 'Stats for nerds' dropped frames."
}

main "$@"
