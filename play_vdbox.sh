#!/bin/bash
# VDBOX Player launcher - hardware H.264 playback via custom iGPU driver
# Usage: ./play_vdbox.sh movie.mp4 [more/*.mp4]
set -e
FF="$HOME/Downloads/ffmpeg"
[ -x "$FF" ] || FF=$(which ffmpeg)
OUT=/tmp/vdbox_play.h264
for v in "$@"; do
  echo "[VDBOX] converting $v -> raw H.264 ..."
  "$FF" -y -i "$v" -c:v copy -bsf:v h264_mp4toannexb -an "$OUT" 2>/dev/null
  echo "[VDBOX] playing with Intel iGPU HW decode (Core 5 120U VDBOX)"
  sudo "$(dirname "$0")/vdbox_player_bin" "$OUT" && break
done
