#!/bin/bash
#===========================================================================
#  redeploy.sh — Redeploy MyIntelGPU.kext ลงเครื่องนี้ (local) หลังอัปเกรด OS
#
#  ใช้หลัง: อัปเกรด Ventura (หรือ rebuild ใหม่เมื่อไหร่ก็ได้)
#    ./redeploy.sh
#
#  Flow: build จาก source (stamp version) → backup → ติดตั้ง L/E →
#        rebuild auxKC → verify
#
#  ⚠️  ข้อจำกัดที่รู้: ถ้า csr-active-config ไม่มีบิต 0x80 (ALLOW_UNAUTHENTICATED_ROOT)
#      kmutil จะ fail ตอน rebuild bootKC ("Read-only file system") —
#      auxKC เดิมที่สะอาดยังใช้ได้ แต่ version stamp ใหม่จะเข้า cache ครั้งหน้า
#===========================================================================

set -e
cd "$(dirname "$0")"

TARGET="MyIntelGPU"
BUNDLE_ID="com.pongpan-bk.MyIntelGPU"
DEST="/Library/Extensions/MyIntelGPU.kext"
SDK="${KERNEL_SDK_DIR:-/Users/ppbk/MacKernelSDK}"

echo "════════════════════════════════════════════════════════════"
echo "  MyIntelGPU redeploy (local)"
echo "════════════════════════════════════════════════════════════"

# ── 1. Build (version stamp อัตโนมัติ 1.0.1.N) ───────────────────────────
echo ""
echo "[1/6] Build จาก source (KERNEL_SDK_DIR=${SDK}) ..."
make KERNEL_SDK_DIR="${SDK}" 2>&1 | tail -3

VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "${TARGET}.kext/Contents/Info.plist")
echo "      → version: ${VERSION}"

# ── 2. Backup ตัวเดิม ─────────────────────────────────────────────────────
echo ""
echo "[2/6] สำรอง kext ปัจจุบันใน L/E ..."
BACKUP="${DEST}.bak-$(date +%Y%m%d-%H%M%S)"
if [ -d "${DEST}" ]; then
    sudo cp -R "${DEST}" "${BACKUP}"
    echo "      → ${BACKUP}"
else
    echo "      (ไม่มีตัวเดิม — ข้าม)"
fi

# ── 3. ติดตั้ง L/E + Sign + Auto-Approve ──────────────────────────────────
echo ""
echo "[3/6] ติดตั้งที่ L/E ..."
sudo rm -rf "${DEST}"
sudo cp -R "${TARGET}.kext" "${DEST}"
sudo chown -R root:wheel "${DEST}"
sudo chmod -R 755 "${DEST}"
sudo codesign -s - --force "${DEST}" 2>/dev/null || true
echo "      → ติดตั้งและ Sign เสร็จ"

echo "      → Auto-approve ใน KextPolicy (ไม่ถาม Allow ซ้ำ) ..."
sudo sqlite3 /var/db/SystemPolicyConfiguration/KextPolicy << 'EOF' 2>/dev/null || true
DELETE FROM kext_policy WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
INSERT INTO kext_policy (team_id, bundle_id, allowed, developer_name, flags)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'Pongpan BK - MyIntelGPU', 1);

INSERT OR REPLACE INTO kext_policy_mdm (team_id, bundle_id, allowed, payload_uuid)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'ALWAYS_ALLOW_ADMIN');

UPDATE kext_load_history_v3
SET flags = 51
WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
EOF
echo "      → KextPolicy: ALWAYS ALLOW ✅"

# ── 4. Rebuild auxKC (kmutil — ห้าม kextcache) ────────────────────────────
echo ""
echo "[4/6] rebuild kext cache ..."
if sudo kmutil install --volume-root / 2>/tmp/kmutil.err; then
    echo "      → kmutil install --volume-root / สำเร็จ"
elif sudo kmutil install --update-all 2>/tmp/kmutil.err; then
    echo "      → kmutil install --update-all สำเร็จ"
else
    echo "      ⚠️  kmutil warning (ดูด้านล่าง) — cache เดิมยังใช้ได้"
    tail -3 /tmp/kmutil.err
fi

# ── 5. Verify ──────────────────────────────────────────────────────────────
echo ""
echo "[5/6] verify ..."
echo "   L/E kext version : $(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "${DEST}/Contents/Info.plist")"
echo "   L/E binary hash  : $(shasum "${DEST}/Contents/MacOS/MyIntelGPU" | awk '{print $1}')"
echo "   auxKC มี kext    : $(strings /Library/KernelCollections/AuxiliaryKernelExtensions.kc 2>/dev/null | grep -c 'Iris(R) Plus Graphics' || echo 0)"
if strings /Library/KernelCollections/AuxiliaryKernelExtensions.kc 2>/dev/null | grep -qi "FakeID"; then
    echo "   ⚠️  auxKC ยังมี FakeID หลงเหลือ!"
else
    echo "   auxKC สะอาด     : ✅ (ไม่มี FakeID)"
fi

# ── 6. สรุป ────────────────────────────────────────────────────────────────
echo ""
echo "════════════════════════════════════════════════════════════"
echo "✅ Redeploy เสร็จ — ขั้นต่อไป:"
echo ""
echo "   [1] reboot:  sudo reboot"
echo "   [2] หลังเข้า Desktop ตรวจ:"
echo "       kextstat -b ${BUNDLE_ID}"
echo "       ioreg -l -c ${TARGET} -w0 | grep '\"model\"'"
echo ""
echo "   [ROLLBACK] ถ้าบูตติดแต่พัง:"
echo "       sudo cp -R ${BACKUP} ${DEST} && sudo kmutil install --update-all && sudo reboot"
echo "════════════════════════════════════════════════════════════"
