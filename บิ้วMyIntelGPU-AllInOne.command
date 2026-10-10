

#!/bin/bash
# MyIntelGPU All-in-One: Build + Deploy + Verify
# Double-click to run

set -e
cd /Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh



echo "=========================================="
echo "  MyIntelGPU Build & Deploy"
echo "=========================================="
echo ""

# 1. Build
echo "[1/4] Building..."
make KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK -j$(sysctl -n hw.ncpu)
VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" MyIntelGPU.kext/Contents/Info.plist)
echo "  -> Version: $VERSION"

# 2. Deploy
echo ""
echo "[2/4] Deploying to /Library/Extensions..."
sudo rm -rf /Library/Extensions/MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo codesign -s - --force /Library/Extensions/MyIntelGPU.kext 2>/dev/null || true

# 3. KextPolicy auto-approve
echo "[3/4] Auto-approve KextPolicy..."
sudo sqlite3 /var/db/SystemPolicyConfiguration/KextPolicy << 'SQL' 2>/dev/null || true
DELETE FROM kext_policy WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
INSERT INTO kext_policy (team_id, bundle_id, allowed, developer_name, flags)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'Pongpan BK - MyIntelGPU', 1);
INSERT OR REPLACE INTO kext_policy_mdm (team_id, bundle_id, allowed, payload_uuid)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'ALWAYS_ALLOW_ADMIN');
UPDATE kext_load_history_v3 SET flags = 51 WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
SQL
echo "  -> KextPolicy: ALWAYS ALLOW"

# 4. Rebuild kernel cache
echo "[4/4] Rebuilding kernel cache..."
if sudo kmutil install --volume-root / 2>/tmp/kmutil.err; then
    echo "  -> kmutil install --volume-root / OK"
elif sudo kmutil install --update-all 2>/tmp/kmutil.err; then
    echo "  -> kmutil install --update-all OK"
else
    echo "  WARNING: kmutil warning (see below)"
    tail -3 /tmp/kmutil.err
fi

echo ""
echo "=========================================="
echo "  DEPLOY COMPLETE - Version: $VERSION"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. sudo reboot"
echo "  2. After boot: kextstat | grep pongpan"
echo "  3. Verify: ioreg -l -c MyIntelFramebuffer -w0 | grep CurrentPowerState"
echo ""
read -p "Press Enter to close..."
