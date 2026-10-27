#!/bin/bash
# ============================================================================
#  10-full-audit.command — Comprehensive all-in-one (runs every check)
# ============================================================================
set -uo pipefail
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin"

echo "============================================"
echo "  10-full-audit — Complete Audit"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

DIR="/Users/ppbk/Desktop/เทส/new"

for f in 01-build-verify 02-preflight 03-install-verify 04-runtime-health \
         05-media-engines 06-irq-interrupt 07-display-edid 08-log-forensics; do
  echo ""
  echo "########## $f ##########"
  bash "$DIR/$f.command" 2>&1 | head -60
done

echo ""
echo "============================================"
echo "  Full audit complete"
echo "============================================"
