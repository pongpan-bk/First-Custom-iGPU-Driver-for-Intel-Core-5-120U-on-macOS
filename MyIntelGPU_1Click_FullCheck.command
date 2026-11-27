#!/bin/bash
# ==============================================================================
# ⚡ MyIntelGPU — 1-CLICK FULL CHECK (15 gates / 12 sections — Info.plist Driven)
# Target: Intel Core 5 120U (0x8086:0xA7AC) | MyIntelGPU.kext | macOS 15
#
# Gates: 01 PCI · 02 Repo · 03 APFS · 04 Kext · 05 AuxKC · 06 Size
#        07 GPUBundles guard · 08 Info.plist Bundles · 09 Perms+CodeSign
#        10 Accelerator · 11 IRQ · 12 EDID · 13 Info.plist Metal Contract
#        14 BCS blitter · 15 VCS H.264 harness
#
# Output: ~/Desktop/MyIntelGPU_FullCheck_YYYYMMDD_HHMMSS.text  (UTF-8, ANSI-stripped)
# Exit:   0 = all gates PASS · 1 = any FAIL · 2 = only WARN
# ==============================================================================
set -u

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/opt/homebrew/bin:$PATH"
TS="$(date '+%Y%m%d_%H%M%S')"
HUMAN_TIME="$(date '+%Y-%m-%d %H:%M:%S %z')"
DESKTOP_DIR="$HOME/Desktop"
mkdir -p "$DESKTOP_DIR"

FINAL_LOG="$DESKTOP_DIR/MyIntelGPU_FullCheck_${TS}.text"
HEAD_TMP="$(mktemp /tmp/myintel_fc_head.XXXXXX)"
BODY_TMP="$(mktemp /tmp/myintel_fc_body.XXXXXX)"
FIX_TMP="$(mktemp /tmp/myintel_fc_fix.XXXXXX)"
PROBE_SRC="/tmp/myintel_fc_probe_${TS}.m"
PROBE_BIN="/tmp/myintel_fc_probe_${TS}"
WORK_DIR="$(mktemp -d /tmp/myintel_fc_work.XXXXXX)"

cleanup() {
    rm -f "$HEAD_TMP" "$BODY_TMP" "$FIX_TMP" "$PROBE_SRC" "$PROBE_BIN" 2>/dev/null || true
    rm -rf "$WORK_DIR" 2>/dev/null || true
    if [ -n "${SUDO_KEEPALIVE_PID:-}" ]; then
        kill "$SUDO_KEEPALIVE_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ── Terminal UI ───────────────────────────────────────────────────────────────
C_RST=$'\033[0m'
C_BLD=$'\033[1m'
C_RED=$'\033[0;31m'
C_GRN=$'\033[0;32m'
C_YLW=$'\033[1;33m'
C_BLU=$'\033[0;34m'
C_CYN=$'\033[0;36m'

run_timed() {
    local sec="$1"
    shift
    /usr/bin/perl -e 'alarm shift; exec @ARGV' "$sec" "$@"
}

sec_log() {
    printf "\n================================================================================\n" >> "$BODY_TMP"
    printf "[%s] %s\n" "$1" "$2" >> "$BODY_TMP"
    printf "================================================================================\n" >> "$BODY_TMP"
    printf "\n${C_BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}\n"
    printf "${C_CYN}[%s]${C_RST} ${C_BLD}%s${C_RST}\n" "$1" "$2"
    printf "${C_BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RST}\n"
}

add_fix() {
    printf '%s\n' "$1" >> "$FIX_TMP"
}

clear
printf "%s\n" "${C_CYN}╔══════════════════════════════════════════════════════════════════════════╗${C_RST}"
printf "%s\n" "${C_CYN}║   ⚡ MyIntelGPU 1-Click Full Check — 15 gates (Info.plist Driven)         ║${C_RST}"
printf "%s\n" "${C_CYN}║      Gen 12 Xe (8086:A7AC) | macOS 15 | read-only diagnostics            ║${C_RST}"
printf "%s\n" "${C_CYN}╚══════════════════════════════════════════════════════════════════════════╝${C_RST}"
printf "\n%s\n\n" "${C_YLW}ไฟล์รายงานจะถูกบันทึกไว้ที่: $FINAL_LOG${C_RST}"

SUDO=""
printf "%s\n" "${C_YLW}ขอสิทธิ์ sudo ครั้งเดียวเพื่อตรวจ AuxKC, WindowServer VMMap และ Kernel Logs...${C_RST}"
if sudo -v 2>/dev/null; then
    SUDO="sudo"
    ( while true; do sudo -n true; sleep 25; kill -0 "$$" || exit; done ) 2>/dev/null &
    SUDO_KEEPALIVE_PID=$!
    printf "%s\n\n" "${C_GRN}✓ ได้รับสิทธิ์ sudo — เริ่มสแกน${C_RST}"
else
    printf "%s\n\n" "${C_YLW}⚠ รันแบบไม่ใช้ sudo (คำสั่งที่ต้องใช้ root จะถูกข้าม)${C_RST}"
fi

# ── Gate results ──────────────────────────────────────────────────────────────
SC_01_PCI="UNKNOWN"
SC_02_REPO="UNKNOWN"
SC_03_APFS="UNKNOWN"
SC_04_KEXT="UNKNOWN"
SC_05_AUXKC="UNKNOWN"
SC_06_SIZE="UNKNOWN"
SC_07_BOOTSAFE="UNKNOWN"
SC_08_PLUGINS="UNKNOWN"
SC_09_SIGN="UNKNOWN"
SC_10_ACCEL="UNKNOWN"
SC_11_IRQ="UNKNOWN"
SC_12_EDID="UNKNOWN"
SC_13_MTL="UNKNOWN"
SC_14_BCS="UNKNOWN"
SC_15_VCS="UNKNOWN"

# ==============================================================================
# [01/12] HOST, SECURITY, PCI IDENTITY, BOOT-ARGS
# ==============================================================================
sec_log "01/12" "HOST · SECURITY · PCI HONEST-IDENTITY (8086:A7AC) · BOOT-ARGS"
{
    echo "--- Host & OS ---"
    sw_vers
    uname -a
    sysctl -n machdep.cpu.brand_string 2>/dev/null || true
    sysctl kern.boottime 2>/dev/null || true
    uptime
    echo ""
    echo "--- Security & Boot-Args ---"
    csrutil status 2>/dev/null || true
    nvram csr-active-config 2>/dev/null || echo "csr-active-config: (not set)"
    nvram boot-args 2>/dev/null || echo "boot-args: (not set)"
    echo ""
    echo "--- PCI Graphics Device (IOPCIDevice, GFX0) ---"
    ioreg -l -w0 -n GFX0 -r 2>/dev/null \
        | grep -E 'device-id|vendor-id|class-code|subsystem|IOName|model|AAPL,ig-platform|MetalPlugin|IOMTLBundle' \
        || echo "  (GFX0 node not found)"
    echo ""
    echo "--- A7AC property neighbourhood ---"
    ioreg -l -w0 2>/dev/null | grep -i -B 3 -A 12 'aca70000' | head -n 40 \
        || echo "  (no 0xA7AC device-id anywhere in the registry)"
} >> "$BODY_TMP" 2>&1

if ioreg -l -w0 2>/dev/null | grep -qi '"device-id" = <aca70000>'; then
    if ioreg -l -w0 2>/dev/null | grep -E -qi '"device-id" = <(3e9b|a59b|9b3e)'; then
        SC_01_PCI="WARN (พบ 0xA7AC ปนกับค่า Spoof/FakeID)"
    else
        SC_01_PCI="PASS (Honest-Identity 0x8086:0xA7AC ไม่พึ่ง FakeID)"
    fi
else
    SC_01_PCI="FAIL (ไม่พบ PCI 0xA7AC ใน IOPCIDevice)"
    add_fix "ตรวจว่า iGPU ถูกมองเห็นที่ PciRoot(0x0)/Pci(0x2,0x0) และ kext ผูกกับ 0xa7ac8086"
fi

# ==============================================================================
# [02/12] REPO SYNC · VERSION PARITY · APFS GHOST INODES
# ==============================================================================
sec_log "02/12" "REPO SYNC · VERSION PARITY · APFS GHOST INODE SCAN"
SCRIPT_SELF_DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd || echo "$HOME/Desktop")"
REPO_DIR=""
for cand in \
    "$HOME/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh" \
    "$SCRIPT_SELF_DIR" \
    "$SCRIPT_SELF_DIR/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS" \
    "$HOME/Desktop/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS" \
    "$HOME/Downloads/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS" \
    "$HOME/Documents/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS"; do
    if [ -f "$cand/MyIntelGPU.cpp" ] && [ -f "$cand/Makefile" ]; then
        REPO_DIR="$cand"
        break
    fi
done

KEXT_DIR="/Library/Extensions/MyIntelGPU.kext"
KEXT_PLIST="$KEXT_DIR/Contents/Info.plist"
[ ! -f "$KEXT_PLIST" ] && [ -n "$REPO_DIR" ] && [ -f "$REPO_DIR/Info.plist" ] && KEXT_PLIST="$REPO_DIR/Info.plist"
KEXT_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$KEXT_PLIST" 2>/dev/null || echo 'com.pongpan-bk.MyIntelGPU')"
KEXT_EXE="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$KEXT_PLIST" 2>/dev/null || echo 'MyIntelGPU')"
KEXT_BIN="$KEXT_DIR/Contents/MacOS/$KEXT_EXE"

INST_VER="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$KEXT_DIR/Contents/Info.plist" 2>/dev/null || echo 'NONE')"
REPO_VER="NONE"
REPO_HDR_VER="NONE"

# Extract plugin & class bindings dynamically from Info.plist
PLIST_KV="$(/usr/bin/python3 - "$KEXT_PLIST" << 'PY_PLIST_EOF' 2>/dev/null || true
import plistlib, sys
p = sys.argv[1]
with open(p, "rb") as f:
    d = plistlib.load(f)
pers = d.get("IOKitPersonalities") or {}
pname, pdict = next(iter(pers.items()), ("NONE", {}))
def g(k):
    v = pdict.get(k, "")
    return str(v) if v is not None else ""
print("PERS_NAME=" + pname)
print("MTL_NAME=" + (g("MetalPluginName") or g("IOMTLBundleName")))
print("IOMTL_NAME=" + g("IOMTLBundleName"))
print("MTL_CLASS=" + g("MetalPluginClassName"))
print("VA_NAME=" + g("IOVABundleName"))
print("DVD_NAME=" + g("IODVDBundleName"))
print("GL_NAME=" + g("IOGLBundleName"))
print("VAREND_ID=" + g("IOVARendererID"))
PY_PLIST_EOF
)"

pkv() { printf '%s\n' "$PLIST_KV" | sed -n "s/^$1=//p" | head -n 1; }
INFO_PERS_NAME="$(pkv PERS_NAME)"
INFO_MTL_NAME="$(pkv MTL_NAME)"
INFO_IOMTL_NAME="$(pkv IOMTL_NAME)"
INFO_MTL_CLASS="$(pkv MTL_CLASS)"
INFO_VA_NAME="$(pkv VA_NAME)"
INFO_DVD_NAME="$(pkv DVD_NAME)"
INFO_GL_NAME="$(pkv GL_NAME)"
INFO_VAREND_ID="$(pkv VAREND_ID)"

{
    echo "Repo path            : ${REPO_DIR:-NOT FOUND}"
    echo "Installed CFBundleVer: $INST_VER"
    echo "Active Info.plist    : $KEXT_PLIST (Personality: ${INFO_PERS_NAME:-NONE})"
    if [ -n "$REPO_DIR" ] && [ -d "$REPO_DIR" ]; then
        echo ""
        echo "--- Git State ---"
        ( cd "$REPO_DIR" && git rev-parse --abbrev-ref HEAD && git log -n 3 --oneline && git status -s ) 2>/dev/null || true
        if [ -f "$REPO_DIR/MyIntelGPUVersion.h" ]; then
            REPO_HDR_VER="$(sed -n 's/.*MYINTELGPU_BUNDLE_VERSION "\(.*\)".*/\1/p' "$REPO_DIR/MyIntelGPUVersion.h" | head -n 1)"
            echo "Version header       : ${REPO_HDR_VER:-UNREADABLE}"
        fi
        if [ -d "$REPO_DIR/MyIntelGPU.kext" ]; then
            REPO_VER="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$REPO_DIR/MyIntelGPU.kext/Contents/Info.plist" 2>/dev/null || echo NONE)"
        elif [ -d "$REPO_DIR/build/MyIntelGPU.kext" ]; then
            REPO_VER="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$REPO_DIR/build/MyIntelGPU.kext/Contents/Info.plist" 2>/dev/null || echo NONE)"
        else
            REPO_VER="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$REPO_DIR/Info.plist" 2>/dev/null || echo NONE)"
        fi
        echo "Repo kext/plist ver  : $REPO_VER"
        echo ""
        echo "--- Build Artifacts (.o) ---"
        ls -la "$REPO_DIR"/*.o 2>/dev/null | head -n 20 || echo "  (no .o in repo root)"
    fi
    echo ""
    echo "--- APFS Ghost Dentry Scan (readdir + lstat) ---"
    if [ -n "$REPO_DIR" ] && [ -d "$REPO_DIR" ]; then
        /usr/bin/python3 - "$REPO_DIR" << 'PY_APFS_EOF' 2>&1 || true
import os, sys
root = sys.argv[1]
bad = []
scanned = 0
for cur, dirs, files in os.walk(root):
    if '/.git' in cur or cur.endswith('/.git'):
        dirs[:] = []
        continue
    scanned += 1
    try:
        names = os.listdir(cur)
    except OSError as e:
        bad.append("DIR_ERR:%s:%s" % (cur, e))
        continue
    for n in names:
        p = os.path.join(cur, n)
        try:
            os.lstat(p)
        except OSError as e:
            bad.append("GHOST_INODE:%s -> %s" % (p, e))
print("scanned_dirs=%d" % scanned)
if not bad:
    print("CLEAN (0 ghost dentries)")
for b in bad:
    print(b)
PY_APFS_EOF
    else
        echo "SKIP (repo not found)"
    fi
} >> "$BODY_TMP" 2>&1

ver_num() {
    printf '%s' "$1" | awk -F. '{printf "%d%03d%03d", ($1+0), ($2+0), ($3+0)}'
}

if [ -z "$REPO_DIR" ]; then
    SC_02_REPO="WARN (ไม่พบโฟลเดอร์รีโพ — ตรวจได้เฉพาะ kext ที่ติดตั้ง v$INST_VER)"
elif [ "$REPO_VER" = "$INST_VER" ]; then
    SC_02_REPO="PASS (เวอร์ชันตรงกัน v$INST_VER)"
elif [ "$REPO_VER" != "NONE" ] && [ "$INST_VER" != "NONE" ] && [ "$(ver_num "$INST_VER")" -gt "$(ver_num "$REPO_VER")" ]; then
    SC_02_REPO="WARN (installed v$INST_VER ใหม่กว่า repo v$REPO_VER — คาดได้หลังดีพลอย; rebuild เพื่อยืนยัน source parity)"
    add_fix "ถ้าต้องการ source parity: ./บิ้วMyIntelGPU-AllInOne.command"
elif [ "$REPO_VER" != "NONE" ] && [ "$INST_VER" != "NONE" ] && [ "$(ver_num "$REPO_VER")" -gt "$(ver_num "$INST_VER")" ]; then
    SC_02_REPO="WARN (tree ใหม่กว่า installed: repo v$REPO_VER > installed v$INST_VER — รอ build+deploy เมื่อต้องการอัปเดต)"
    add_fix "หากต้องการซิงก์เวอร์ชัน: รัน ./บิ้วMyIntelGPU-AllInOne.command แล้ว reboot"
else
    SC_02_REPO="WARN (เทียบเวอร์ชันไม่ได้: repo=$REPO_VER installed=$INST_VER)"
fi

GHOST_LINES="$(sed -n '/--- APFS Ghost Dentry Scan/,$p' "$BODY_TMP" | grep -c 'GHOST_INODE:\|DIR_ERR:' | tr -d ' ')"
if [ "${GHOST_LINES:-0}" -gt 0 ]; then
    SC_03_APFS="WARN (พบ ghost dentry $GHOST_LINES รายการ — อยู่ใน build tree)"
    add_fix "ล้าง ghost dentry: mv \"$REPO_DIR/build\" \"$REPO_DIR/build.ghost-\$(date +%s)\" && mkdir -p \"$REPO_DIR/build\""
else
    SC_03_APFS="PASS (ไม่พบ ghost inode / ENOENT ในรีโพ)"
fi

# ==============================================================================
# [03/12] KEXT LOAD · AUXKC UUID SYNC · BINARY SIZE GATE
# ==============================================================================
sec_log "03/12" "KEXT LOAD · AUXKC UUID SYNC · 220 KB SIZE GATE"
LOADED_LINE="$($SUDO kmutil showloaded --list-only --bundle-identifier "$KEXT_ID" 2>/dev/null | grep "$KEXT_ID" || kextstat 2>/dev/null | grep "$KEXT_ID" || true)"
DISK_UUID="$(dwarfdump --uuid "$KEXT_BIN" 2>/dev/null | awk '{print $2}' | head -n 1)"
LOADED_UUID="$(printf '%s' "$LOADED_LINE" | grep -oE '[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}' | head -n 1)"
LOADED_VER="$(printf '%s\n' "$LOADED_LINE" | awk -v id="$KEXT_ID" '{for (i = 1; i < NF; i++) if ($i == id) {v=$(i+1); gsub(/[()]/, "", v); print v; exit}}')"
BIN_SIZE="$(stat -f%z "$KEXT_BIN" 2>/dev/null || echo 0)"

{
    echo "--- Loaded Kext ---"
    echo "${LOADED_LINE:-NOT LOADED}"
    echo "Loaded Ver    : ${LOADED_VER:-UNKNOWN} | Installed Ver: $INST_VER | ID: $KEXT_ID"
    echo "Binary path   : $KEXT_BIN"
    echo "Binary size   : $BIN_SIZE bytes (hard gate <= 220000)"
    echo "Disk UUID     : ${DISK_UUID:-UNKNOWN}"
    echo "Loaded UUID   : ${LOADED_UUID:-UNKNOWN}"
    echo "Exported syms : $(nm -gU "$KEXT_BIN" 2>/dev/null | wc -l | tr -d ' ')"
    md5 "$KEXT_BIN" 2>/dev/null || true
    echo ""
    echo "--- auxKC / kmutil inspect ---"
    run_timed 12 $SUDO kmutil inspect 2>/dev/null | grep -A 4 -B 1 "$KEXT_ID" || echo "  (no kmutil match / needs root)"
    echo ""
    echo "--- APFS boot snapshots ---"
    diskutil apfs listSnapshots / 2>/dev/null | grep -E 'Name:|XID|Created:' | head -n 20 || echo "  (none listed)"
} >> "$BODY_TMP" 2>&1

if [ -n "$LOADED_VER" ] && [ "$LOADED_VER" = "$INST_VER" ]; then
    SC_04_KEXT="PASS (โหลดใน Kernel: v$LOADED_VER)"
elif [ -n "$LOADED_LINE" ]; then
    SC_04_KEXT="WARN (Kernel โหลด v${LOADED_VER:-?} แต่ติดตั้ง v$INST_VER — ต้อง reboot ให้ AuxKC ใหม่มีผล)"
    add_fix "sudo reboot แล้วรัน FullCheck อีกรอบ"
else
    SC_04_KEXT="FAIL (kext ไม่ได้โหลด)"
    add_fix "sudo kmutil install --volume-root / --update-all && sudo reboot"
fi

if [ -n "$LOADED_UUID" ] && [ -n "$DISK_UUID" ] && [ "$LOADED_UUID" = "$DISK_UUID" ]; then
    SC_05_AUXKC="PASS (Kernel UUID == Disk UUID)"
elif [ -n "$LOADED_UUID" ] && [ -n "$DISK_UUID" ]; then
    SC_05_AUXKC="WARN (UUID ไม่ตรง: kernel=$LOADED_UUID disk=$DISK_UUID — ต้อง reboot ให้ AuxKC ใหม่มีผล)"
    add_fix "sudo kmutil install --volume-root / --update-all && sudo reboot"
else
    SC_05_AUXKC="WARN (อ่าน UUID ไม่ครบ — ตรวจด้วยตาในหัวข้อ 03)"
fi

if [ "${BIN_SIZE:-0}" -gt 0 ] && [ "${BIN_SIZE:-0}" -le 220000 ]; then
    SC_06_SIZE="PASS ($BIN_SIZE / 220,000 bytes)"
elif [ "${BIN_SIZE:-0}" -gt 220000 ]; then
    SC_06_SIZE="FAIL ($BIN_SIZE bytes เกิน 220,000)"
    add_fix "ลดขนาดไบนารี: ตรวจ exported_symbols.txt / dead_strip; เพดานคือ 220000 bytes"
else
    SC_06_SIZE="FAIL (ไม่พบไบนารี $KEXT_BIN)"
fi

# ==============================================================================
# [04/12] INFO.PLIST TARGET BUNDLES · GPUBUNDLES GUARD · PERMS · CODESIGN
# ==============================================================================
sec_log "04/12" "INFO.PLIST GRAPHICS BUNDLES · /Library/GPUBundles GUARD · PERMS · CODESIGN"

resolve_bundle_path() {
    local raw="${1:-}"
    [ -z "$raw" ] && return 1
    local b="${raw%.bundle}"
    for loc in \
        "$KEXT_DIR/Contents/PlugIns/${b}.bundle" \
        "/Library/GPUBundles/${b}.bundle" \
        "/Library/Extensions/${b}.bundle" \
        "/System/Library/Extensions/${b}.bundle"; do
        if [ -d "$loc" ]; then
            printf '%s' "$loc"
            return 0
        fi
    done
    return 1
}

ACTIVE_MTL_PLUG="$(resolve_bundle_path "$INFO_MTL_NAME" || true)"
ACTIVE_VA_PLUG="$(resolve_bundle_path "$INFO_VA_NAME" || true)"
ACTIVE_DVD_PLUG="$(resolve_bundle_path "$INFO_DVD_NAME" || true)"
ACTIVE_GL_PLUG="$(resolve_bundle_path "$INFO_GL_NAME" || true)"

BAD_OWN="$(find "$KEXT_DIR" ! -user 0 -o ! -group 0 2>/dev/null | head -n 10)"
KSEAL="MISSING"
if codesign --verify --deep --strict "$KEXT_DIR" >/dev/null 2>&1; then KSEAL="VALID"; else KSEAL="BROKEN"; fi

BSEAL="SYSTEM_SSV"
if [ -n "$ACTIVE_MTL_PLUG" ]; then
    case "$ACTIVE_MTL_PLUG" in
        /System/*) BSEAL="APPLE_SYSTEM_SSV" ;;
        *)
            if codesign --verify --strict "$ACTIVE_MTL_PLUG" >/dev/null 2>&1; then BSEAL="VALID"; else BSEAL="BROKEN"; fi
            ;;
    esac
else
    BSEAL="MISSING"
fi

{
    echo "--- Info.plist Personality Bindings (${INFO_PERS_NAME:-NONE}) ---"
    echo "  IOMTLBundleName      : ${INFO_IOMTL_NAME:--}"
    echo "  MetalPluginName      : ${INFO_MTL_NAME:--} -> ${ACTIVE_MTL_PLUG:-NOT FOUND ON DISK}"
    echo "  MetalPluginClassName : ${INFO_MTL_CLASS:--}"
    echo "  IOVABundleName       : ${INFO_VA_NAME:--} -> ${ACTIVE_VA_PLUG:-NOT FOUND ON DISK}"
    echo "  IODVDBundleName      : ${INFO_DVD_NAME:--} -> ${ACTIVE_DVD_PLUG:-NOT FOUND ON DISK}"
    echo "  IOGLBundleName       : ${INFO_GL_NAME:--} -> ${ACTIVE_GL_PLUG:-NOT FOUND ON DISK}"
    echo "  IOVARendererID       : ${INFO_VAREND_ID:--}"
    echo ""
    echo "--- /Library/GPUBundles (must stay empty of unverified custom bundles) ---"
    ls -la /Library/GPUBundles 2>/dev/null || echo "  (directory does not exist — boot-safe)"
    echo ""
    echo "--- Kext Contents ---"
    find "$KEXT_DIR/Contents" -maxdepth 4 2>/dev/null | sort
    echo ""
    echo "Target Metal bundle  : ${ACTIVE_MTL_PLUG:-NONE}"
    echo "Target bundle seal   : $BSEAL"
    echo "Kext code signature  : $KSEAL"
    echo ""
    echo "--- Ownership (files not 0:0 inside MyIntelGPU.kext) ---"
    echo "${BAD_OWN:-ALL FILES 0:0 root:wheel}"
    echo ""
    echo "--- CodeSign Detail (MyIntelGPU.kext) ---"
    codesign -dvvv "$KEXT_DIR" 2>&1 | head -n 14 || true
    echo "[verify kext --deep --strict]"
    codesign --verify --deep --strict "$KEXT_DIR" 2>&1 | head -n 12 || true
} >> "$BODY_TMP" 2>&1

if [ -e "/Library/GPUBundles/MyIntelGPUMTLDriver.bundle" ]; then
    SC_07_BOOTSAFE="WARN (พบ custom bundle ค้างใน /Library/GPUBundles — เสี่ยงกระทบ WindowServer ตอน login)"
    add_fix "ล้าง custom bundle ที่ไม่ใช้แล้ว: sudo rm -rf /Library/GPUBundles/MyIntelGPUMTLDriver.bundle"
else
    SC_07_BOOTSAFE="PASS (/Library/GPUBundles ว่าง — boot-safe)"
fi

MISSING_BUNDLES=()
[ -z "$ACTIVE_MTL_PLUG" ] && MISSING_BUNDLES+=("MTL:${INFO_MTL_NAME:-unset}")
[ -n "$INFO_VA_NAME" ]  && [ -z "$ACTIVE_VA_PLUG" ]  && MISSING_BUNDLES+=("VA:$INFO_VA_NAME")
[ -n "$INFO_DVD_NAME" ] && [ -z "$ACTIVE_DVD_PLUG" ] && MISSING_BUNDLES+=("DVD:$INFO_DVD_NAME")
[ -n "$INFO_GL_NAME" ]  && [ -z "$ACTIVE_GL_PLUG" ]  && MISSING_BUNDLES+=("GL:$INFO_GL_NAME")

if [ -d "$KEXT_DIR/Contents/PlugIn" ]; then
    SC_08_PLUGINS="WARN (พบโฟลเดอร์สะกดผิด Contents/PlugIn ค้างใน kext)"
    add_fix "ลบหรือเปลี่ยนชื่อ Contents/PlugIn ใน $KEXT_DIR แล้วเซ็น kext ใหม่"
elif [ "${#MISSING_BUNDLES[@]}" -eq 0 ] && [ -n "$ACTIVE_MTL_PLUG" ]; then
    SC_08_PLUGINS="PASS (Info.plist -> ${INFO_MTL_NAME}.bundle + ชุด VA/DVD/GL พบครบในระบบ)"
elif [ -n "$ACTIVE_MTL_PLUG" ]; then
    SC_08_PLUGINS="WARN (พบ ${INFO_MTL_NAME}.bundle แต่ขาด bundle เสริม: ${MISSING_BUNDLES[*]})"
else
    SC_08_PLUGINS="FAIL (ไม่พบ ${INFO_MTL_NAME:-MetalPluginName}.bundle ที่ระบุใน Info.plist)"
    add_fix "ตรวจสอบค่า MetalPluginName / IOMTLBundleName ใน $KEXT_PLIST ให้ตรงกับ bundle ที่มีใน /System/Library/Extensions"
fi

if [ -z "$BAD_OWN" ] && [ "$KSEAL" = "VALID" ] && [ "$BSEAL" != "BROKEN" ]; then
    SC_09_SIGN="PASS (0:0 root:wheel + kext seal VALID + target=$BSEAL)"
else
    SC_09_SIGN="FAIL (owner/seal ไม่ผ่าน: owner=${BAD_OWN:-ok} kext=$KSEAL target=$BSEAL)"
    if [ "$KSEAL" = "BROKEN" ]; then
        add_fix "ซ่อมลายเซ็น kext: sudo codesign -s - --force --deep \"$KEXT_DIR\" && sudo kmutil install --volume-root / --update-all"
    fi
    if [ -n "$BAD_OWN" ]; then
        add_fix "sudo chown -R 0:0 \"$KEXT_DIR\" && sudo chmod -R 755 \"$KEXT_DIR\""
    fi
fi

# ==============================================================================
# [05/12] IOREGISTRY NODE MATRIX
# ==============================================================================
sec_log "05/12" "IOREGISTRY NODE MATRIX (GPU / ACCEL / VCS / FRAMEBUFFER)"
{
    echo "--- Node presence ---"
    for cls in MyIntelGPU MyIntelAccelerator MyIntelGPUVCS IONDRVFramebuffer; do
        n="$(ioreg -r -c "$cls" -l -w0 2>/dev/null | grep -c '^+-o' || true)"
        echo "  $cls : ${n:-0} instance(s)"
    done
    echo ""
    echo "--- MyIntelGPU ---"
    ioreg -r -c MyIntelGPU -l -w0 2>/dev/null || echo "  (none)"
    echo ""
    echo "--- MyIntelAccelerator ---"
    ioreg -r -c MyIntelAccelerator -l -w0 2>/dev/null || echo "  (none)"
    echo ""
    echo "--- MyIntelGPUVCS ---"
    ioreg -r -n MyIntelGPUVCS -l -w0 2>/dev/null || echo "  (none)"
} >> "$BODY_TMP" 2>&1

# ==============================================================================
# [06/12] LIVE USERSPACE PROBE: IOKit handshake + Info.plist Bundle/Class Contract
# ==============================================================================
sec_log "06/12" "LIVE PROBE: IOKit HANDSHAKE · INFO.PLIST BUNDLE LOAD & CLASS CONTRACT"
cat << 'OBJC_PROBE_EOF' > "$PROBE_SRC"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <IOKit/IOKitLib.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <string.h>

static int accelNodes = 0;
static int accelOpenType5 = 0;
static int gpuPlistSync = 0;
static int accelPlistSync = 1;

static void inspectClass(const char *clsName, NSString *expPlugin, NSString *expClass) {
    CFMutableDictionaryRef match;
    if (strcmp(clsName, "MyIntelGPUVCS") == 0)
        match = IOServiceNameMatching(clsName);
    else
        match = IOServiceMatching(clsName);

    io_iterator_t iter = 0;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter);
    if (kr != KERN_SUCCESS || !iter) {
        printf("  [IOKit] %-20s : query error kr=0x%x\n", clsName, kr);
        return;
    }
    int idx = 0;
    io_service_t svc;
    while ((svc = IOIteratorNext(iter)) != 0) {
        idx++;
        uint64_t eid = 0;
        IORegistryEntryGetRegistryEntryID(svc, &eid);
        io_name_t nm = {0}, ioCls = {0};
        IORegistryEntryGetName(svc, nm);
        IOObjectGetClass(svc, ioCls);
        CFTypeRef bId = IORegistryEntryCreateCFProperty(svc, CFSTR("CFBundleIdentifier"), kCFAllocatorDefault, 0);
        CFTypeRef mPlg = IORegistryEntryCreateCFProperty(svc, CFSTR("MetalPluginName"), kCFAllocatorDefault, 0);
        CFTypeRef mCls = IORegistryEntryCreateCFProperty(svc, CFSTR("MetalPluginClassName"), kCFAllocatorDefault, 0);

        NSString *plgStr = mPlg ? [(__bridge id)mPlg description] : @"-";
        NSString *clsStr = mCls ? [(__bridge id)mCls description] : @"-";

        printf("  [IOKit] %-20s : #%d class=%s name=%s id=0x%llx bundle=%s mtlPlugin=%s mtlClass=%s\n",
               clsName, idx, ioCls, nm, eid,
               bId ? [[(__bridge id)bId description] UTF8String] : "-",
               plgStr.UTF8String, clsStr.UTF8String);

        if (strcmp(clsName, "MyIntelGPU") == 0) {
            if ([plgStr isEqualToString:expPlugin] && [clsStr isEqualToString:expClass])
                gpuPlistSync = 1;
        }
        if (strcmp(clsName, "MyIntelAccelerator") == 0) {
            accelNodes++;
            if (!([plgStr isEqualToString:expPlugin] && [clsStr isEqualToString:expClass]))
                accelPlistSync = 0;
        }

        if (strcmp(clsName, "MyIntelGPU") == 0 || strcmp(clsName, "MyIntelAccelerator") == 0 ||
            strcmp(clsName, "MyIntelGPUVCS") == 0) {
            for (uint32_t t = 0; t <= 5; t++) {
                io_connect_t conn = 0;
                kern_return_t okr = IOServiceOpen(svc, mach_task_self(), t, &conn);
                if (okr == KERN_SUCCESS && conn) {
                    printf("    -> IOServiceOpen(type=%u) : OPEN OK\n", t);
                    if (strcmp(clsName, "MyIntelAccelerator") == 0 && t == 5) accelOpenType5 = 1;
                    IOServiceClose(conn);
                } else {
                    printf("    -> IOServiceOpen(type=%u) : kr=0x%x\n", t, okr);
                }
            }
        }
        if (bId) CFRelease(bId);
        if (mPlg) CFRelease(mPlg);
        if (mCls) CFRelease(mCls);
        IOObjectRelease(svc);
    }
    IOObjectRelease(iter);
    if (idx == 0) printf("  [IOKit] %-20s : 0 instances (NOT REGISTERED)\n", clsName);
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSString *targetBundlePath = (argc > 1) ? [NSString stringWithUTF8String:argv[1]] : @"";
        NSString *expPlugin        = (argc > 2) ? [NSString stringWithUTF8String:argv[2]] : @"";
        NSString *expClass         = (argc > 3) ? [NSString stringWithUTF8String:argv[3]] : @"";

        printf("=== [6A] IOKit Services & Info.plist Parity (Expected: %s / %s) ===\n",
               expPlugin.UTF8String, expClass.UTF8String);
        const char *targets[] = { "MyIntelGPU", "MyIntelAccelerator", "IOAccelerator",
                                  "MyIntelFramebuffer", "IntelFramebuffer", "MyIntelGPUVCS" };
        for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++)
            inspectClass(targets[i], expPlugin, expClass);

        printf("[GATE] ACCEL_NODES=%d\n", accelNodes);
        printf("[GATE] ACCEL_UC5=%d\n", accelOpenType5);
        printf("[GATE] GPU_PLIST_SYNC=%d\n", gpuPlistSync);
        printf("[GATE] ACCEL_PLIST_SYNC=%d\n", accelPlistSync);

        printf("\n=== [6B] Info.plist Target Metal Bundle Load + Class Probe ===\n");
        int dl_ok = 0, cls_ok = 0, init_accel = 0, init_limits = 0;
        if (targetBundlePath.length > 0 && [[NSFileManager defaultManager] fileExistsAtPath:targetBundlePath]) {
            NSBundle *b = [NSBundle bundleWithPath:targetBundlePath];
            NSString *exe = [b executablePath];
            NSString *principal = [b objectForInfoDictionaryKey:@"NSPrincipalClass"];
            printf("  [Info.plist Bundle] %s\n", targetBundlePath.UTF8String);
            printf("    executable      : %s\n", exe ? exe.UTF8String : "(dyld shared cache / nil)");
            printf("    bundle principal: %s | Info.plist target class: %s\n",
                   principal ? principal.UTF8String : "(not set in bundle — uses Info.plist MetalPluginClassName)",
                   expClass.UTF8String);

            NSError *loadErr = nil;
            if ([b loadAndReturnError:&loadErr]) {
                printf("    [NSBundle load] : OK\n");
                dl_ok = 1;
            } else if (exe && dlopen(exe.UTF8String, RTLD_NOW | RTLD_LOCAL)) {
                printf("    [dlopen]        : OK\n");
                dl_ok = 1;
            } else {
                printf("    [load]          : FAIL -> %s\n",
                       loadErr ? loadErr.localizedDescription.UTF8String : (dlerror() ?: "unknown"));
            }

            if (dl_ok && expClass.length > 0) {
                Class c = NSClassFromString(expClass);
                printf("    [resolved class]: %s\n", c ? class_getName(c) : "NOT FOUND");
                if (c) {
                    cls_ok = 1;
                    NSMutableArray *chain = [NSMutableArray array];
                    for (Class cur = c; cur != Nil; cur = class_getSuperclass(cur)) {
                        [chain addObject:[NSString stringWithUTF8String:class_getName(cur)]];
                    }
                    printf("    [inheritance]   : %s\n", [[chain componentsJoinedByString:@" -> "] UTF8String]);
                    if ([c instancesRespondToSelector:NSSelectorFromString(@"initWithAcceleratorPort:")]) init_accel = 1;
                    if ([c instancesRespondToSelector:NSSelectorFromString(@"initLimits")]) init_limits = 1;
                    printf("    [selectors]     : initWithAcceleratorPort=%d initLimits=%d\n", init_accel, init_limits);
                }
            }
        } else {
            printf("  [Info.plist Bundle] NOT FOUND: %s\n", targetBundlePath.UTF8String);
        }

        printf("[GATE] DLOPEN=%d\n", dl_ok);
        printf("[GATE] PLUGIN_CLASS=%d\n", cls_ok);
        printf("[GATE] PLUGIN_INIT_ACCEL=%d\n", init_accel);
        printf("[GATE] PLUGIN_INIT_LIMITS=%d\n", init_limits);

        printf("\n=== [6C] Metal Framework ===\n");
        NSArray<id<MTLDevice>> *devs = MTLCopyAllDevices();
        printf("  MTLCopyAllDevicesCount=%lu\n", (unsigned long)devs.count);
        for (id<MTLDevice> d in devs)
            printf("  [MTLDevice] name=%s registryID=0x%llx\n", d.name.UTF8String, d.registryID);
        id<MTLDevice> sys = MTLCreateSystemDefaultDevice();
        printf("  MTLDefaultDevice=%s\n", sys ? sys.name.UTF8String : "nil");
        printf("[GATE] MTL_DEVICES=%lu\n", (unsigned long)devs.count);
    }
    return 0;
}
OBJC_PROBE_EOF

PROBE_OUT=""
if xcrun clang -O2 -fobjc-arc -framework Foundation -framework Metal -framework IOKit \
        "$PROBE_SRC" -o "$PROBE_BIN" >> "$BODY_TMP" 2>&1; then
    codesign -fs - "$PROBE_BIN" >> "$BODY_TMP" 2>&1 || true
    PROBE_OUT="$(run_timed 20 "$PROBE_BIN" "${ACTIVE_MTL_PLUG:-}" "${INFO_MTL_NAME:-}" "${INFO_MTL_CLASS:-}" 2>&1)"
    printf '%s\n' "$PROBE_OUT" >> "$BODY_TMP"
else
    echo "WARN: probe did not compile (Xcode CLT missing?)" >> "$BODY_TMP"
fi

gv() { printf '%s\n' "$PROBE_OUT" | sed -n "s/^\[GATE\] $1=//p" | head -n 1; }
ACCEL_NODES="$(gv ACCEL_NODES)"
ACCEL_UC5="$(gv ACCEL_UC5)"
GPU_PLIST_SYNC="$(gv GPU_PLIST_SYNC)"
ACCEL_PLIST_SYNC="$(gv ACCEL_PLIST_SYNC)"
DL_OPEN="$(gv DLOPEN)"
PLUG_CLASS="$(gv PLUGIN_CLASS)"
PLUG_ACCEL="$(gv PLUGIN_INIT_ACCEL)"
PLUG_LIMITS="$(gv PLUGIN_INIT_LIMITS)"
MTL_DEVS="$(gv MTL_DEVICES)"

if [ "${ACCEL_NODES:-0}" -ge 1 ] 2>/dev/null; then
    if [ "${ACCEL_UC5:-0}" -eq 1 ] 2>/dev/null; then
        SC_10_ACCEL="PASS (Accelerator registered; user client type 5 opens)"
    else
        SC_10_ACCEL="WARN (Accelerator registered แต่ user client type 5 เปิดไม่ได้)"
        add_fix "ตรวจ MyIntelAccelerator::newUserClient ว่าเปิด type 5 ตามที่ควร"
    fi
else
    SC_10_ACCEL="FAIL (ไม่พบ MyIntelAccelerator node)"
    add_fix "ตรวจ boot-arg myintelaccel=2 (หรือ =1) ใน NVRAM / config.plist"
fi

if [ "${DL_OPEN:-0}" -eq 1 ] && [ "${PLUG_CLASS:-0}" -eq 1 ] && [ "${PLUG_ACCEL:-0}" -eq 1 ] && [ "${PLUG_LIMITS:-0}" -eq 1 ]; then
    if [ "${GPU_PLIST_SYNC:-1}" -eq 0 ]; then
        SC_13_MTL="WARN (${INFO_MTL_NAME}/${INFO_MTL_CLASS} โหลดผ่านครบ แต่โหนด MyIntelAccelerator ยังประกาศชื่อปลั๊กอินไม่ตรง Info.plist)"
        add_fix "ซิงก์ชื่อปลั๊กอินใน MyIntelAccelerator::publishProperties() ให้ตรงกับ Info.plist (${INFO_MTL_NAME} / ${INFO_MTL_CLASS}) หรือตั้ง boot-arg mymtlplugin=${INFO_MTL_NAME} mymtlclass=${INFO_MTL_CLASS}"
    elif [ "${MTL_DEVS:-0}" -ge 1 ] 2>/dev/null; then
        SC_13_MTL="PASS (${INFO_MTL_NAME}/${INFO_MTL_CLASS} ครบสัญญา + Metal enumerate $MTL_DEVS device)"
    else
        SC_13_MTL="PASS (Info.plist -> ${INFO_MTL_NAME}/${INFO_MTL_CLASS} โหลดผ่าน + selectors ครบ; MTLCopyAllDevices=0 ตามโหมด Honest-ID)"
    fi
elif [ "${DL_OPEN:-0}" -eq 1 ]; then
    SC_13_MTL="WARN (${INFO_MTL_NAME} โหลดได้ แต่คลาส/selector ไม่ตรง: class=${INFO_MTL_CLASS}:$PLUG_CLASS initAccel=$PLUG_ACCEL initLimits=$PLUG_LIMITS)"
    add_fix "ตรวจค่า MetalPluginClassName ใน Info.plist ให้ตรงกับคลาสจริงใน ${INFO_MTL_NAME}.bundle"
else
    SC_13_MTL="FAIL (โหลด ${INFO_MTL_NAME:-MetalPluginName}.bundle ตาม Info.plist ไม่สำเร็จ)"
    add_fix "ตรวจสอบพาธ ${ACTIVE_MTL_PLUG:-not-found} ของ ${INFO_MTL_NAME:-unset} ใน Info.plist"
fi

# ==============================================================================
# [07/12] DISPLAY / EDID
# ==============================================================================
sec_log "07/12" "DISPLAY ENGINE · FRAMEBUFFER · EDID INJECTION"
{
    system_profiler SPDisplaysDataType 2>/dev/null || true
    echo ""
    echo "--- IODisplayConnect / EDID ---"
    ioreg -lw0 -r -c IODisplayConnect 2>/dev/null \
        | grep -E 'DisplayVendorID|DisplayProductID|IODisplayEDID|IODisplayPrefsKey' || echo "  (no IODisplayConnect match)"
} >> "$BODY_TMP" 2>&1

EDID_HEX="$(ioreg -lw0 -r -c IODisplayConnect 2>/dev/null | sed -n 's/.*"IODisplayEDID" = <\([0-9a-fA-F]*\)>.*/\1/p' | head -n 1)"
if [ -n "$EDID_HEX" ] && [ "${#EDID_HEX}" -ge 256 ]; then
    SC_12_EDID="PASS (IODisplayEDID ${#EDID_HEX} hex chars = $(( ${#EDID_HEX} / 2 )) bytes)"
elif [ -n "$EDID_HEX" ]; then
    SC_12_EDID="WARN (IODisplayEDID สั้นผิดปกติ: ${#EDID_HEX} hex chars)"
else
    SC_12_EDID="WARN (ไม่พบ IODisplayEDID — จออาจใช้ fallback timing)"
fi

# ==============================================================================
# [08/12] WINDOWSERVER / COMPOSITOR
# ==============================================================================
sec_log "08/12" "WINDOWSERVER VMMAP · SOFTWARE COMPOSITE AUDIT"
WS_PID="$(pgrep -x WindowServer | head -n 1)"
{
    echo "WindowServer PID: ${WS_PID:-NOT FOUND}"
    if [ -n "$WS_PID" ] && [ -n "$SUDO" ]; then
        echo "--- Metal / accelerator mappings ---"
        run_timed 15 $SUDO vmmap -w "$WS_PID" 2>/dev/null \
            | grep -E 'MyIntel|MTL|Metal|IOAccelerator|AppleIntel' | head -n 40 || true
    fi
    echo ""
    echo "--- Unsupported sw composite (last 15m) ---"
    SW_CNT="$(run_timed 10 log show --last 15m --style compact \
        --predicate 'process == "WindowServer" AND eventMessage CONTAINS "Unsupported sw composite"' 2>/dev/null \
        | grep -c 'Unsupported sw composite' || true)"
    echo "count=${SW_CNT:-0}"
} >> "$BODY_TMP" 2>&1

# ==============================================================================
# [09/12] VDBOX / VCS H.264 HARNESS
# ==============================================================================
sec_log "09/12" "VDBOX / VCS H.264 (harness build + canonical verify-decode gate)"
VCS_BIN="$WORK_DIR/vcs_test_harness"
VD_SH="${REPO_DIR:-}/tools/verify-decode.sh"

{
    echo "=== [9A] harness link check (test_submit_vcs.m + MyIntelVCSCommand.cpp) ==="
    if [ -z "$REPO_DIR" ] || [ ! -f "$REPO_DIR/test_submit_vcs.m" ]; then
        echo "SKIP: test_submit_vcs.m not found"
    elif xcrun clang -O2 -fobjc-arc -framework Foundation -framework IOKit \
            -I"$REPO_DIR" -I"$REPO_DIR/VCSMediaHarness" \
            "$REPO_DIR/test_submit_vcs.m" "$REPO_DIR/MyIntelVCSCommand.cpp" \
            -o "$VCS_BIN" 2>&1; then
        echo "build: OK ($(stat -f%z "$VCS_BIN") bytes)"
    else
        echo "build: FAILED"
    fi
    echo ""
    echo "=== [9B] canonical H.264 golden gate (tools/verify-decode.sh) ==="
    if [ -f "$VD_SH" ]; then
        echo "script mode: $(stat -f%Lp "$VD_SH")  (invoking via /bin/sh)"
        OUT="$WORK_DIR/verify" /bin/sh "$VD_SH" 2>&1 | tail -n 16
        echo "verify_decode_rc=${PIPESTATUS[0]:-unknown}"
    else
        echo "SKIP: tools/verify-decode.sh not found"
    fi
    echo ""
    echo "=== [9C] note on the raw harness dump ==="
    echo "test_submit_vcs.m dumps the surface without the myintel-decode detile walk;"
    echo "its md5 is NOT a bit-exactness oracle. Use verify-decode.sh for the verdict."
} >> "$BODY_TMP" 2>&1

VD_RC="$(sed -n 's/^verify_decode_rc=//p' "$BODY_TMP" | tail -n 1)"
VD_PASS="$(grep -c '^PASS  h264-' "$BODY_TMP" 2>/dev/null | tr -d ' ')"

if [ "${VD_RC:-x}" = "0" ] && [ "${VD_PASS:-0}" -ge 2 ]; then
    if grep -q 'build: OK' "$BODY_TMP" 2>/dev/null; then
        SC_15_VCS="PASS (frame0 + 30f bit-exact via verify-decode.sh; harness links)"
    else
        SC_15_VCS="WARN (decode bit-exact แต่ harness link ไม่ผ่าน — ดูหัวข้อ 09A)"
        add_fix "harness ต้อง link คู่กับ MyIntelVCSCommand.cpp และ -I\$REPO -I\$REPO/VCSMediaHarness"
    fi
elif [ -f "$VD_SH" ]; then
    SC_15_VCS="FAIL (verify-decode.sh ไม่ผ่าน: rc=${VD_RC:-unknown}, PASS lines=${VD_PASS:-0})"
    add_fix "รันเอง: /bin/sh \"$REPO_DIR/tools/verify-decode.sh\""
else
    SC_15_VCS="WARN (ไม่มี tools/verify-decode.sh ในรีโพที่ตรวจพบ)"
fi

# ==============================================================================
# [10/12] BCS BLITTER STRESS (bounded)
# ==============================================================================
sec_log "10/12" "BCS BLITTER (selector 25 BcsCopyRect) — bounded 100 iterations"
BCS_BIN="$WORK_DIR/bcs_stress"
{
    if [ -n "$REPO_DIR" ] && [ -f "$REPO_DIR/tools/bcs_stress.c" ] && xcrun clang -O2 "$REPO_DIR/tools/bcs_stress.c" \
            -o "$BCS_BIN" -framework IOKit -framework CoreFoundation 2>&1; then
        codesign -fs - "$BCS_BIN" 2>&1 || true
        echo "--- run: bcs_stress 100 (30s watchdog) ---"
        run_timed 30 "$BCS_BIN" 100 2>&1 | tail -n 25
        echo "bcs_rc=${PIPESTATUS[0]:-unknown}"
    else
        echo "SKIP: tools/bcs_stress.c missing or did not compile"
    fi
} >> "$BODY_TMP" 2>&1

if grep -q 'bcs_rc=0' "$BODY_TMP" 2>/dev/null; then
    SC_14_BCS="PASS (100/100 submissions + content match)"
elif grep -q '\[stress\] submissions' "$BODY_TMP" 2>/dev/null; then
    SC_14_BCS="FAIL (bcs_stress exit != 0 — ดูหัวข้อ 10)"
    add_fix "BCS ring อาจค้าง: อย่ารันเกิน 100 รอบ; ถ้าค้างให้ reboot แล้วรัน tools/verify-pipes.sh"
else
    SC_14_BCS="SKIP (bcs_stress ไม่ได้รัน)"
fi

# ==============================================================================
# [11/12] KERNEL LOG FORENSICS + IRQ EVIDENCE
# ==============================================================================
sec_log "11/12" "KERNEL LOG FORENSICS · IRQ EVIDENCE · PANIC REPORTS"
BOOTARGS="$(nvram boot-args 2>/dev/null || echo "")"
{
    echo "boot-args: $BOOTARGS"
    echo ""
    echo "--- Ring / media init evidence from IORegistry ---"
    ioreg -r -c MyIntelGPU -l -w0 2>/dev/null | grep -E 'RingInitialized|RCS-|VDBOX|VEBOX|VECS|IntelMedia' || true
    echo ""
    echo "--- Kernel messages: MyIntel* (last 2h, tail 120) ---"
    run_timed 12 log show --last 2h --style compact \
        --predicate 'senderImagePath CONTAINS "MyIntel" OR eventMessage CONTAINS "MyIntel"' 2>/dev/null \
        | tail -n 120 || true
    echo ""
    echo "--- Security / CodeSign rejections (last 2h) ---"
    run_timed 12 log show --last 2h --style compact \
        --predicate 'eventMessage CONTAINS "Invalid ownership" OR eventMessage CONTAINS "map_with_linking_np" OR (eventMessage CONTAINS "AMFI" AND eventMessage CONTAINS "MyIntel")' 2>/dev/null \
        | tail -n 40 || true
    echo ""
    echo "--- Recent panic / WindowServer reports ---"
    ls -lt /Library/Logs/DiagnosticReports/*panic* /Library/Logs/DiagnosticReports/*WindowServer* 2>/dev/null | head -n 10 || echo "  (none)"
} >> "$BODY_TMP" 2>&1

GTIRQ=0
printf '%s' "$BOOTARGS" | grep -q 'myintelgtirq' && GTIRQ=1
VCS0INIT="$(ioreg -r -c MyIntelGPU -l -w0 2>/dev/null | grep -q '"VCS0RingInitialized" = Yes' && echo Yes || echo No)"

if [ "$GTIRQ" -eq 1 ] && [ "$VCS0INIT" = "Yes" ]; then
    SC_11_IRQ="PASS (myintelgtirq present + VCS0 ring initialized)"
elif [ "$VCS0INIT" = "Yes" ]; then
    SC_11_IRQ="WARN (ring init OK แต่ไม่พบ myintelgtirq ใน boot-args)"
else
    SC_11_IRQ="WARN (อ่าน IRQ/VBlank ไม่ได้จาก userspace — ดู log ในหัวข้อ 11)"
fi

# ==============================================================================
# [12/12] SCORECARD + WRITE REPORT
# ==============================================================================
sec_log "12/12" "EXECUTIVE SCORECARD · WRITING REPORT"
FIX_COUNT="$(wc -l < "$FIX_TMP" | tr -d ' ')"
FAIL_COUNT=0
WARN_COUNT=0

for st in "$SC_01_PCI" "$SC_02_REPO" "$SC_03_APFS" "$SC_04_KEXT" "$SC_05_AUXKC" \
          "$SC_06_SIZE" "$SC_07_BOOTSAFE" "$SC_08_PLUGINS" "$SC_09_SIGN" \
          "$SC_10_ACCEL" "$SC_11_IRQ" "$SC_12_EDID" "$SC_13_MTL" "$SC_14_BCS" "$SC_15_VCS"; do
    case "$st" in
        FAIL*) FAIL_COUNT=$((FAIL_COUNT + 1)) ;;
        WARN*) WARN_COUNT=$((WARN_COUNT + 1)) ;;
    esac
done

{
    echo "################################################################################"
    echo "⚡ MYINTELGPU — 1-CLICK FULL CHECK REPORT (INFO.PLIST DRIVEN)"
    echo "Generated At : $HUMAN_TIME"
    echo "Local Repo   : ${REPO_DIR:-NOT FOUND}"
    echo "Installed Ver: $INST_VER  |  Info.plist Target: ${INFO_MTL_NAME:-NONE} (${INFO_MTL_CLASS:-NONE})"
    echo "Summary      : $FAIL_COUNT FAIL · $WARN_COUNT WARN · $((15 - FAIL_COUNT - WARN_COUNT)) PASS"
    echo "################################################################################"
    echo ""
    echo "==================== [ สรุปผลตรวจ 15 ด่าน ] ===================="
    printf "  01. PCI Honest-Identity (0xA7AC)  : %s\n" "$SC_01_PCI"
    printf "  02. Repo & Version Parity         : %s\n" "$SC_02_REPO"
    printf "  03. APFS Ghost Inodes             : %s\n" "$SC_03_APFS"
    printf "  04. Kext Loaded in Kernel         : %s\n" "$SC_04_KEXT"
    printf "  05. AuxKC UUID Sync               : %s\n" "$SC_05_AUXKC"
    printf "  06. Binary Size Gate (<=220KB)    : %s\n" "$SC_06_SIZE"
    printf "  07. /Library/GPUBundles Guard     : %s\n" "$SC_07_BOOTSAFE"
    printf "  08. Info.plist Graphics Bundles   : %s\n" "$SC_08_PLUGINS"
    printf "  09. Ownership + Kext CodeSign     : %s\n" "$SC_09_SIGN"
    printf "  10. MyIntelAccelerator Node       : %s\n" "$SC_10_ACCEL"
    printf "  11. IRQ / Ring Evidence           : %s\n" "$SC_11_IRQ"
    printf "  12. EDID Injection                : %s\n" "$SC_12_EDID"
    printf "  13. Info.plist Metal Contract     : %s\n" "$SC_13_MTL"
    printf "  14. BCS Blitter (BcsCopyRect)     : %s\n" "$SC_14_BCS"
    printf "  15. VCS H.264 Harness             : %s\n" "$SC_15_VCS"
    echo "================================================================"
    echo ""
    if [ "$FIX_COUNT" -gt 0 ]; then
        echo "==================== [ คำสั่งซ่อมที่ตรงกับผลรอบนี้ ] ===================="
        awk '{printf "  [%d] %s\n", NR, $0}' "$FIX_TMP"
        echo "========================================================================"
    else
        echo "✅ ทุกด่านผ่านเกณฑ์ครบ 15/15!"
    fi
} > "$HEAD_TMP"

cat "$HEAD_TMP" "$BODY_TMP" | LC_ALL=C sed $'s/\033\\[[0-9;]*m//g' > "$FINAL_LOG"

printf "\n"
cat "$HEAD_TMP"
printf "\n${C_GRN}📄 รายงานถูกบันทึกที่:${C_RST}\n"
printf "${C_BLD}   %s${C_RST}\n\n" "$FINAL_LOG"

afplay /System/Library/Sounds/Glass.aiff >/dev/null 2>&1 &
open -R "$FINAL_LOG" >/dev/null 2>&1 || true

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
elif [ "$WARN_COUNT" -gt 0 ]; then
    exit 2
fi
exit 0
