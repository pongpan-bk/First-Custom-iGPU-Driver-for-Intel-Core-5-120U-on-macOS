#!/bin/bash
# ============================================================================
#  04-runtime-health.command — IORegistry, IOAccelerator, PCI apertures
# ============================================================================
set -uo pipefail

echo "============================================"
echo "  04-runtime-health — IOKit / PCI"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

ok()  { echo "  [OK] $1"; }
warn(){ echo "  [!!] $1"; }
fail(){ echo "  [XX] $1"; }

# 1. IORegistry MyIntelGPU node
echo ""
echo "[1] IORegistry — MyIntelGPU node"
IOREG=$(ioreg -r -c MyIntelGPU 2>/dev/null | head -20)
if [ -n "$IOREG" ]; then
  ok "MyIntelGPU node present"
  echo "$IOREG" | grep -E '"IOClass"|"CFBundleIdentifier"|"VRAM|"RCS' | sed 's/^/    /'
else
  fail "MyIntelGPU node NOT in IORegistry"
fi

# 2. IOAccelerator / MyIntelAccelerator
echo ""
echo "[2] MyIntelAccelerator"
ACC=$(ioreg -r -c MyIntelAccelerator 2>/dev/null | head -40)
if [ -n "$ACC" ]; then
  ok "MyIntelAccelerator present"
  echo "$ACC" | grep -E "MetalPluginName|MetalPluginClassName|IOAccelIndex|IOAccelTypes|IOVARendererID|MetalStatisticsName|IOGLBundleName" | sed 's/^/    /'
else
  fail "No MyIntelAccelerator (need myintelaccel=1 boot-arg)"
fi

# 3. PCI device 8086:a7ac
echo ""
echo "[3] PCI device 8086:a7ac"
PCI=$(ioreg -r -c IOPCIDevice 2>/dev/null | grep -i "a7ac" | head -5)
if [ -n "$PCI" ]; then
  ok "GPU device found"
  echo "$PCI" | sed 's/^/    /'
else
  fail "PCI device 8086:a7ac not found"
fi

# 4. PCI apertures (BARs)
echo ""
echo "[4] PCI apertures / BARs"
ioreg -c IOPCIDevice -l -b 2>/dev/null | grep -E "pci8086,a7ac|MyIntelGPU" -A 50 | grep -E "reg|BAR|memSize|RCS-|VRAM" | head -12 | sed 's/^/    /'

# 5. IODisplayConnect / framebuffer
echo ""
echo "[5] IODisplayConnect"
DISP=$(ioreg -lw0 -c IODisplayConnect 2>/dev/null | head -10)
if [ -n "$DISP" ]; then
  ok "IODisplayConnect present"
  ioreg -lw0 -c IODisplayConnect 2>/dev/null | grep -E '"IODisplayEDID"|"DisplayVendorID"|"DisplayProductID"|"IODisplayConnectFlags"' | sed 's/^/    /'
else
  warn "No IODisplayConnect nodes"
fi

# 6. System Profiler
echo ""
echo "[6] System Profiler — Displays"
system_profiler SPDisplaysDataType 2>/dev/null | head -30 | sed 's/^/    /'

echo ""
echo "============================================"
echo "  Runtime health complete"
echo "============================================"
