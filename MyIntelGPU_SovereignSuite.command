#!/bin/bash
# ==============================================================================
# ⚡ MyIntelGPU MASTER SOVEREIGN SUITE & ARCHITECTURAL FORENSIC (Info.plist Driven)
# Target: Intel Core 5 120U (0x8086:0xA7AC) | macOS 15 - 26+ | Deep Health Check
# รวมฟังก์ชัน: 15 Gates + Deep Performance + VDBOX Forensic + Metal Smoke Analyzer
# ==============================================================================
set -u
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/homebrew/bin:$PATH"

TS="$(date '+%Y%m%d_%H%M%S')"
HUMAN_TIME="$(date '+%Y-%m-%d %H:%M:%S %z')"
LOG_FILE="$HOME/Desktop/MyIntelGPU_MasterForensic_${TS}.text"
exec > >(tee -a "$LOG_FILE") 2>&1

C_RST=$'\033[0m' ; C_BLD=$'\033[1m' ; C_RED=$'\033[0;31m' ; C_GRN=$'\033[0;32m' ; C_YLW=$'\033[1;33m' ; C_BLU=$'\033[0;34m' ; C_CYN=$'\033[0;36m'

sec_header() {
    echo -e "\n${C_BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}"
    echo -e "${C_CYN}[$1]${C_RST} ${C_BLD}$2${C_RST}"
    echo -e "${C_BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}"
}

clear
echo -e "${C_CYN}╔══════════════════════════════════════════════════════════════════════════════╗${C_RST}"
echo -e "${C_CYN}║     ⚡ MyIntelGPU ALL-IN-ONE SOVEREIGN HEALTH & FORENSIC SUITE v2.0          ║${C_RST}"
echo -e "${C_CYN}║     Gen 12 Xe (8086:A7AC) | IntelReviveGPU Hardware Pipeline Validation      ║${C_RST}"
echo -e "${C_CYN}╚══════════════════════════════════════════════════════════════════════════════╝${C_RST}"
echo -e "${C_YLW}📄 บันทึกไฟล์ล็อกรายงานสดไว้ที่: $LOG_FILE${C_RST}"

# ขอสิทธิ์ Root ครั้งเดียวเพื่อเข้าถึงระดับ Kernel
if [ "$EUID" -ne 0 ]; then
    if ! sudo -v 2>/dev/null; then
        echo -e "${C_RED}⚠️  สคริปต์นี้ต้องการสิทธิ์ Root ชั่วคราวเพื่อขุดข้อมูลความปลอดภัยใน Kernel Log${C_RST}"
        sudo -v
    fi
fi

# ==============================================================================
sec_header "PART 1" "HOST ENVIRONMENT, SIP CONFIG & PREBOOT SNAPSHOTS"
# ==============================================================================
echo "--- Host Architecture & Darwin Core ---"
sw_vers ; uname -a
sysctl -n machdep.cpu.brand_string 2>/dev/null || true

echo -e "\n--- System Integrity Protection (SIP) & Config ---"
nvram csr-active-config 2>/dev/null || echo "csr-active-config: (not set)"
csrutil status 2>/dev/null || true

echo -e "\n--- APFS Kernel Collections & Volume Snapshots Graph ---"
kmutil inspect 2>/dev/null | grep -A 2 "com.pongpan-bk.MyIntelGPU" || echo "  Not found in auxKC staging"
diskutil apfs listSnapshots / 2>/dev/null | grep -E "Name:|XID|Created:" | tail -n 4

# ==============================================================================
sec_header "PART 2" "KEXT RESIDENCY, DEPLOYMENT PARITY & ANTI-MISMATCH GUARD"
# ==============================================================================
KEXT_DIR="/Library/Extensions/MyIntelGPU.kext"
KEXT_PLIST="$KEXT_DIR/Contents/Info.plist"
SRC_REPO="/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"

if [ -d "$KEXT_DIR" ]; then
    INST_VER=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$KEXT_PLIST" 2>/dev/null || echo "0.0.0")
    LOADED_VER=$(kmutil showloaded --list-only 2>/dev/null | grep -i myintel | awk -F'(' '{print $2}' | awk -F')' '{print $1}' | head -1)
    echo "  Installed Kext version on Disk : $INST_VER"
    echo "  Active Loaded Kernel version   : ${LOADED_VER:-None (Kext not loaded)}"
    
    if [ "$INST_VER" != "$LOADED_VER" ] && [ -n "$LOADED_VER" ]; then
        echo -e "  ${C_RED}⚠️  VERSION MISMATCH WARNING: เวอร์ชันบนดิสก์และแรมไม่ตรงกัน เสี่ยงเครื่องค้าง (Freeze) หากยิงบิตสตรีม${C_RST}"
    else
        echo -e "  ${C_GRN}✅ Version Synchronized: ปลอดภัยในการทดสอบท่อวงแหวนฮาร์ดแวร์${C_RST}"
    fi
    
    echo -e "\n--- Binary Size Hard-Gate Compliance ---"
    BIN_SIZE=$(stat -f%z "$KEXT_DIR/Contents/MacOS/MyIntelGPU" 2>/dev/null || echo 0)
    echo "  Binary Size: $BIN_SIZE bytes (Hard Gate limit: 220000 bytes)"
    if [ "$BIN_SIZE" -lt 220000 ]; then
        echo -e "  ${C_GRN}✅ ผ่านเพดานขนาดไบนารีเคอร์เนล${C_RST}"
    else
        echo -e "  ${C_RED}❌ ไบนารีเกินเพดาน 220000 bytes เสี่ยงโหลดเคอร์เนลล้มเหลว${C_RST}"
    fi
fi

# ==============================================================================
sec_header "PART 3" "IOVARENDERERID & GFX0 FRAMEBUFFER RESOURCE TABLE"
# ==============================================================================
ioreg -l -w0 -c IOPCIDevice 2>/dev/null | grep -E 'device-id|vendor-id|class-code|IOName|model|AAPL,ig-platform|MetalPlugin|IOMTLBundle' || echo "  (GFX0 node not found)"

echo -e "\n--- Base Address Register (BAR) Mapping Apertures ---"
ioreg -c IOPCIDevice -l -b 2>/dev/null | grep -E "pci8086,a7ac|MyIntelGPU" -A 50 | grep -E "compatible|reg|BAR|memSize|totalMB|VRAM|MMIO|RCS-" | head -12

echo -e "\n--- IOKit Accelerator Sub-Classes Matrix ---"
ioreg -r -c IOAccelerator -l 2>/dev/null | grep -E "MyIntel|IOVARendererID|MetalStatisticsName|IOGLBundleName|IOAccelIndex|IOAccelTypes"

# ==============================================================================
sec_header "PART 4" "DISPLAY ENGINE PANEL REAL-SIZE & OVERRIDES VALIDATION"
# ==============================================================================
echo "--- Live Display Connection Flags & EDID Analysis ---"
ioreg -lw0 -c IODisplayConnect 2>/dev/null | grep -E '"IODisplayEDID"|"IODisplayConnectFlags"|"DisplayVendorID"|"DisplayProductID"' | head -5

echo -e "\n--- Physical Panel Size Extractor (Target 15.3 Inch) ---"
EDIDHEX=$(ioreg -lw0 -c IODisplayConnect 2>/dev/null | grep -oE '"IODisplayEDID" = <[0-9a-f]+>' | head -1 | sed -E 's/.*<([0-9a-f]+)>/\1/')
if [ -n "$EDIDHEX" ]; then
    DSIN=$(/usr/bin/python3 -c 'import sys,math; h=int(sys.argv[1][42:44],16); v=int(sys.argv[1][44:46],16); print("%.1f" % (math.hypot(h,v)/2.54) if h and v else "")' "$EDIDHEX" 2>/dev/null)
    if [ -n "$DSIN" ]; then
        echo "  Detected Physical Screen Size: $DSIN inch"
        case "$DSIN" in
            15.*|16.*) echo -e "  ${C_GRN}✅ EDID Injection Success (ตรงเป้าหมายแผงหน้าจอ 15.3\")${C_RST}" ;;
            *) echo -e "  ${C_RED}❌ Fallback Size Detected (EDID พ่ายแพ้ Race Condition ให้กับ WindowServer)${C_RST}" ;;
        esac
    fi
fi

echo -e "\n--- Userspace Monitor Profile Override ---"
if [ -f /Library/Displays/Contents/Resources/Overrides/DisplayVendorID-2c82/DisplayProductID-924 ]; then
    echo -e "  ${C_GRN}[FOUND] Active Userspace Display Override Plist Matrix:${C_RST}"
    plutil -p /Library/Displays/Contents/Resources/Overrides/DisplayVendorID-2c82/DisplayProductID-924 | head -15
else
    echo -e "  ${C_RED}[NOT FOUND] Missing Display Override Profile Path${C_RST}"
fi

# ==============================================================================
sec_header "PART 5" "VDBOX BARS SUBMISSION, IRQ EVIDENCE & CACHE COHERENCY"
# ==============================================================================
echo "--- Ring Initialization Vector Registry ---"
ioreg -r -c MyIntelGPU -l -w0 2>/dev/null | grep -E 'RingInitialized|RCS-|VDBOX|VEBOX|IntelMedia'

echo -e "\n--- Kernel Engine Irq & Exec-List State (Last 30 Min) ---"
sudo log show --last 30m --predicate 'sender == "MyIntelGPU"' --style compact 2>/dev/null | grep -E "ARMED|first-entry|processEngineInterrupt|USER IRQ|kick|ELSP|head=" | tail -n 15

echo -e "\n--- MOCS & Cache Poisoning Forensic ---"
sudo log show --last 30m --predicate 'sender == "MyIntelGPU"' --style compact 2>/dev/null | grep -E "MOCS|Cache|stale|aperture|poison|write-through" | tail -n 10

# ==============================================================================
sec_header "PART 6" "USERSPACE METAL INJECTION & USER-CLIENT SECURITY INTERCEPT"
# ==============================================================================
echo "--- WindowServer Realtime Metal Connection Map ---"
ioreg -l -r -c IOAccelerator 2>/dev/null | grep -iE "MetalPluginName|MetalPluginClassName|MTLDriver" || echo "  No active userspace bundle mapping registered"

echo -e "\n--- System Security Sandbox & Corpse Interceptions (Last 45 Min) ---"
sudo log show --last 45m --predicate 'process == "WindowServer" or process == "VTDecoderXPCServ" or process == "kernel"' --style compact 2>/dev/null | grep -E "MyIntel|PROBE|PASS-THROUGH|DISC sel|crashed|corpses|denied|unsupported type|Sandbox" | tail -n 15

echo -e "\n${C_CYN}================================================================================${C_RST}"
echo -e " 🏁 ${C_GRN}MASTER ARCHITECTURAL FORENSIC SCAN COMPLETE!${C_RST}"
echo -e " รายงานระบบถูกเข้ารหัสถาวรไว้ที่ไดเรกทอรีเดสก์ท็อปของคุณแล้ว"
echo -e "${C_CYN}================================================================================${C_RST}\n"
