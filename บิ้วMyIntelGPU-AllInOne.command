#!/bin/bash
# MyIntelGPU All-in-One: Build + Deploy + Verify
# Double-click to run
#
# Version numbering: BUILD_NUMBER defaults to (installed version + 1).
# Override with either form:
#     BUILD_NUMBER=120 ./บิ้วMyIntelGPU-AllInOne.command
#     ./บิ้วMyIntelGPU-AllInOne.command 120

set -e
cd /Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh

SRC="MyIntelGPU.kext"
DST="/Library/Extensions/MyIntelGPU.kext"
BACKUP=""

echo "=========================================="
echo "  MyIntelGPU Build & Deploy"
echo "=========================================="
echo ""

if [ "$(id -u)" -eq 0 ]; then
    echo "  WARNING: running as root."
    echo "           Build artifacts become root-owned and the NEXT"
    echo "           'make clean' will fail with Permission denied."
    echo "           Close this and run it as your normal user instead."
    echo ""
    [ -t 0 ] && read -p "Press Enter to continue anyway (or Ctrl-C to stop)..."
fi

# ---------------------------------------------------------------- build number
# The Makefile defaults BUILD_NUMBER to the git commit count (Makefile:60),
# so a bare `make` stamps the same version no matter what you changed.
# Bump past whatever is actually installed instead.
if [ -z "$BUILD_NUMBER" ] && [ -n "$1" ]; then
    BUILD_NUMBER="$1"
fi

if [ -z "$BUILD_NUMBER" ]; then
    CUR=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$DST/Contents/Info.plist" 2>/dev/null || echo "")
    CUR_NUM="${CUR##*.}"
    if [ -n "$CUR" ] && [ "$CUR_NUM" -eq "$CUR_NUM" ] 2>/dev/null; then
        BUILD_NUMBER=$((CUR_NUM + 1))
        echo "[0/5] Version: installed $CUR -> building $CUR_NUM+1"
    else
        BUILD_NUMBER=$(git rev-list --count HEAD 2>/dev/null || echo 1)
        echo "[0/5] Version: no installed bundle found, using $BUILD_NUMBER"
    fi
else
    echo "[0/5] Version: BUILD_NUMBER=$BUILD_NUMBER (explicit)"
fi

# Restore the previous bundle if anything dies between the rm and the install.
# A partially copied DST still exists, so testing only for a missing directory
# would skip the restore and leave a truncated kext in /Library/Extensions.
DEPLOY_DONE=0
restore_on_error() {
    if [ -n "$BACKUP" ] && [ "$DEPLOY_DONE" -eq 0 ]; then
        echo ""
        echo "  !! deploy failed — restoring $BACKUP"
        sudo rm -rf "$DST"
        sudo cp -R "$BACKUP" "$DST"
        sudo chown -R root:wheel "$DST"
        sudo chmod -R 755 "$DST"
        echo "  !! previous bundle is back in place"
    fi
}
trap restore_on_error ERR

# ---------------------------------------------------------------------- build
echo "[1/5] Building..."
make BUILD_NUMBER="$BUILD_NUMBER" KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make BUILD_NUMBER="$BUILD_NUMBER" KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK -j"$(sysctl -n hw.ncpu)"
VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$SRC/Contents/Info.plist")
echo "  -> Version: $VERSION"

if [ "$VERSION" != "4.1.$BUILD_NUMBER" ]; then
    echo "  !! FATAL: bundle says $VERSION but BUILD_NUMBER was $BUILD_NUMBER"
    exit 1
fi

# --------------------------------------------------------------------- deploy
echo ""
echo "[2/5] Backing up current bundle..."
if [ -d "$DST" ]; then
    BACKUP="$HOME/kext-backups/pre-$VERSION-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$BACKUP"
    sudo cp -R "$DST" "$BACKUP/"
    echo "  -> $BACKUP"
else
    echo "  -> nothing installed yet, skipping"
fi

echo "[3/5] Deploying to /Library/Extensions..."
sudo rm -rf "$DST"
sudo cp -R "$SRC" "$DST"
sudo chown -R root:wheel "$DST"
sudo chmod -R 755 "$DST"
# No `|| true` here: an unsigned bundle will not load, and shipping it anyway
# turns a signing problem into a black screen on the next boot.
sudo codesign -s - --force "$DST"
DEPLOY_DONE=1

# -------------------------------------------------------------- kext approval
echo ""
echo "[4/5] Auto-approve KextPolicy..."
sudo sqlite3 /var/db/SystemPolicyConfiguration/KextPolicy << 'SQL' 2>/dev/null || true
DELETE FROM kext_policy WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
INSERT INTO kext_policy (team_id, bundle_id, allowed, developer_name, flags)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'Pongpan BK - MyIntelGPU', 1);
INSERT OR REPLACE INTO kext_policy_mdm (team_id, bundle_id, allowed, payload_uuid)
VALUES ('', 'com.pongpan-bk.MyIntelGPU', 1, 'ALWAYS_ALLOW_ADMIN');
UPDATE kext_load_history_v3 SET flags = 51 WHERE bundle_id = 'com.pongpan-bk.MyIntelGPU';
SQL
echo "  -> KextPolicy: ALWAYS ALLOW"

# ------------------------------------------------------------ kernel cache
echo ""
echo "[5/5] Rebuilding kernel cache..."
if sudo kmutil install --volume-root / 2>/tmp/kmutil.err; then
    echo "  -> kmutil install --volume-root / OK"
elif sudo kmutil install --update-all 2>/tmp/kmutil.err; then
    echo "  -> kmutil install --update-all OK"
else
    echo "  WARNING: kmutil warning (see below)"
    tail -3 /tmp/kmutil.err
fi

trap - ERR

echo ""
echo "=========================================="
echo "  DEPLOY COMPLETE - Version: $VERSION"
echo "=========================================="
echo ""
echo "Still loaded (old version stays until reboot):"
kmutil showloaded 2>/dev/null | grep -i myintel | sed 's/^/  /'
echo ""
echo "Next steps:"
echo "  1. sudo reboot"
echo "  2. After boot, confirm the version actually changed:"
echo "       kmutil showloaded | grep -i myintel"
echo "  3. Check boot timing and health:"
echo "       log show --predicate 'sender == \"MyIntelGPU\"' --last 20m --style syslog"
echo "     Expect: TIMING: start total under 1000 ms, SUBMIT[RCS]/[BCS]/[VCS],"
echo "             ESR/EIR/IPEIR = 0, 'EDID passthrough: injected'"
echo "  4. Confirm the panel is up:"
echo "       system_profiler SPDisplaysDataType | grep Online"
echo ""
echo "Do NOT boot with -myinteldbg when measuring timing; it replays the"
echo "buffered trace and reintroduces the log pressure being measured."
echo ""
# Only block for a keypress when a human double-clicked the file; a scripted or
# piped run would otherwise hang forever waiting on a stdin that never delivers.
# Written as an `if`, not `[ -t 0 ] && read`, so a non-tty run does not leave the
# script's final exit status at 1 and look like a failed deploy.
if [ -t 0 ]; then
    read -p "Press Enter to close..."
fi