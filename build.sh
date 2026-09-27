#!/bin/bash
#===========================================================================
#  build.sh — ONE-CLICK build for MyIntelGPU.kext
#
#    ./build.sh                build + version bump + verify gates + zip
#    ./build.sh --install      ... then deploy to /Library/Extensions + auxKC
#    ./build.sh --efi          ... then also copy into OpenCore EFI
#    ./build.sh --pkg          ... then also build the installer .pkg
#
#  Proven local build host (MASTER-KNOWLEDGE.md §11):
#    /opt/MacKernelSDK is absent -> use the Command Line Tools SDK
#    make KERNEL_SDK_DIR=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
#
#  Hard rules baked in (learned the expensive way — do not "simplify" these away):
#   * auxKC identity key = CFBundleVersion, NOT the binary hash.
#     Changing the binary without bumping the version = silent no-op on auxKC.
#     => the version bump below is mandatory, not cosmetic.
#   * ld64.lld binaries are rejected by kmutil ("expected no platform").
#     => always link with the Apple CLT linker (plain `cc`), never -fuse-ld=lld.
#   * `kextcache -i /` and `kmutil install --volume-root /` are SILENT NO-OPS
#     when the KDK does not match the running OS. The only command that really
#     rebuilds auxKC is the explicit `kmutil create -n aux ... --allow-missing-kdk`
#     followed by `kcditto`.
#   * `.bundle.bundle` in a personality bundle name = the 3.1.39 regression
#     (Apple's loader appends .bundle itself). Gate G12 hard-fails on it.
#   * Backups live outside /Library/Extensions. A leftover "*.kext.bak-<ts>"
#     beside the live kext is still in the kext search path carrying the same
#     CFBundleIdentifier => ambiguous auxKC => "Invalid Parameter" at load.
#   * NEVER reboot from this script. Reboot is the user's decision.
#===========================================================================

set -uo pipefail
cd "$(dirname "$0")" || exit 1

# ── config ───────────────────────────────────────────────────────────────────
TARGET="MyIntelGPU"
BUNDLE_ID="com.pongpan-bk.MyIntelGPU"
KEXT="MyIntelGPU.kext"
BINARY="$KEXT/Contents/MacOS/$TARGET"
PLIST_SRC="Info.plist"
PLIST_OUT="$KEXT/Contents/Info.plist"
BUILD_DIR="build"
DIST_DIR="$BUILD_DIR/dist"
LOG="$BUILD_DIR/build.log"
L_E="/Library/Extensions/$KEXT"
AUXKC="/Library/KernelCollections/AuxiliaryKernelExtensions.kc"
BKC="/System/Library/KernelCollections/BootKernelExtensions.kc"
SKEKC="/System/Library/KernelCollections/SystemKernelExtensions.kc"

DO_BUMP=1
DO_PKG=0
DO_INSTALL=0
DO_EFI=0
EFI_MOUNT=""
SDK_OVERRIDE=""
DIAG=0
FAILURES=0
WARNINGS=0

# Backups go to the standing archive location, never /Library/Extensions: a
# leftover "*.kext.bak-<ts>" sitting beside the live kext carries the SAME
# CFBundleIdentifier, and the auxKC builder rejects that as "Invalid Parameter"
# and refuses to update. Override with MYINTELGPU_BACKUP_ROOT only if needed.
BACKUP_ROOT="${MYINTELGPU_BACKUP_ROOT:-$HOME/Desktop/all}"

# ── output helpers ───────────────────────────────────────────────────────────
if [ -t 1 ]; then C_OK=$'\033[32m'; C_BAD=$'\033[31m'; C_WARN=$'\033[33m'
                   C_INF=$'\033[36m'; C_DIM=$'\033[2m'; C_OFF=$'\033[0m'
else C_OK=""; C_BAD=""; C_WARN=""; C_INF=""; C_DIM=""; C_OFF=""; fi

say()  { printf '%s\n' "$*"; }
head1() { printf '\n%s\n' "════════════════════════════════════════════════════════════"; }
step() { printf '\n%s▸ %s%s\n' "$C_INF" "$*" "$C_OFF"; }
ok()   { printf '  %s✔%s %s\n' "$C_OK" "$C_OFF" "$*"; }
bad()  { printf '  %s✖%s %s\n' "$C_BAD" "$C_OFF" "$*"; FAILURES=$((FAILURES+1)); }
warn() { printf '  %s!%s %s\n' "$C_WARN" "$C_OFF" "$*"; WARNINGS=$((WARNINGS+1)); }
info() { printf '  %s·%s %s\n' "$C_DIM" "$C_OFF" "$*"; }
die()  { printf '\n%s✖ %s%s\n' "$C_BAD" "$*" "$C_OFF" >&2; exit 1; }

usage() {
  cat <<'USAGE'
One-click build for MyIntelGPU.kext

  ./build.sh [options]

  (no options)        bump CFBundleVersion, clean build, run verify gates, emit zip
  --install           also install to /Library/Extensions and rebuild auxKC (no reboot)
  --efi [mountpoint]  also copy the kext into <mountpoint>/EFI/OC/Kexts
  --pkg               also build dist/MyIntelGPU-<ver>.pkg installer
  --no-bump           keep the current CFBundleVersion (auxKC will NOT refresh)
  --sdk PATH          force a kernel SDK dir instead of auto-detect
  --diagnostics N     pass MYINTELGPU_DIAGNOSTICS=N to the compiler (default 0)
  -h, --help          this text

Exit code 0 = built and every gate passed.
USAGE
}

# ── arg parse ────────────────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
  case "$1" in
    --install)    DO_INSTALL=1 ;;
    --pkg)        DO_PKG=1 ;;
    --efi)        DO_EFI=1
                  if [ $# -ge 2 ] && [ "${2#-}" = "$2" ]; then EFI_MOUNT="$2"; shift; fi ;;
    --no-bump)    DO_BUMP=0 ;;
    --sdk)        [ $# -ge 2 ] || die "--sdk needs a path"; SDK_OVERRIDE="$2"; shift ;;
    --diagnostics) [ $# -ge 2 ] || die "--diagnostics needs a value"
                  DIAG="$2"; shift ;;
    -h|--help)    usage; exit 0 ;;
    *)            printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

# ── 0. preflight ─────────────────────────────────────────────────────────────
head1
say "  MyIntelGPU.kext — one-click build"
head1

PLB=/usr/libexec/PlistBuddy
[ -x "$PLB" ] || die "PlistBuddy missing (install Xcode Command Line Tools)"

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then :; else C_OK=""; C_BAD=""; C_WARN=""; C_INF=""; C_DIM=""; C_OFF=""; fi

info "host     : $(scutil --get ComputerName 2>/dev/null || hostname)"
info "os       : $(sw_vers -productName) $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
info "cc       : $(cc --version 2>/dev/null | head -1)"
info "repo     : $(git rev-parse --short HEAD 2>/dev/null || echo 'not a git repo')"

case "$(uname -m)" in
  arm64) warn "this kext is x86_64-only; cross-building on Apple Silicon still works but cannot be tested locally" ;;
esac

# ── 1. kernel SDK resolution ─────────────────────────────────────────────────
step "Resolving kernel SDK"

sdk_ok() { [ -d "$1/System/Library/Frameworks/Kernel.framework/Headers" ]; }

SDK=""
if [ -n "$SDK_OVERRIDE" ]; then
  SDK="$SDK_OVERRIDE"
else
  DEVP="$(xcode-select -p 2>/dev/null)"
  for cand in \
      "${KERNEL_SDK_DIR:-}" \
      "/opt/MacKernelSDK" \
      "$HOME/MacKernelSDK" \
      "$PWD/../MacKernelSDK" \
      "$PWD/.sdk/MacKernelSDK" \
      "${DEVP}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk" \
      "${DEVP}/SDKs/MacOSX.sdk"
  do
    if [ -n "$cand" ] && sdk_ok "$cand"; then SDK="$cand"; break; fi
  done
fi

if [ -z "$SDK" ]; then
  warn "no kernel headers found locally — fetching MacKernelSDK into .sdk/"
  mkdir -p "$PWD/.sdk"
  if curl -fsSL https://github.com/acidanthera/MacKernelSDK/archive/refs/heads/master.tar.gz \
       -o "$BUILD_DIR/MacKernelSDK.tar.gz"; then
    tar -xzf "$BUILD_DIR/MacKernelSDK.tar.gz" -C "$PWD/.sdk" --strip-components=1 \
      && mv "$PWD/.sdk/MacKernelSDK-master" "$PWD/.sdk/MacKernelSDK" 2>/dev/null
    tar -xzf "$BUILD_DIR/MacKernelSDK.tar.gz" -C "$BUILD_DIR" --strip-components=1 \
      && mv "$BUILD_DIR/MacKernelSDK-master" "$PWD/.sdk/MacKernelSDK" 2>/dev/null
  fi
  for cand in "$PWD/.sdk/MacKernelSDK"; do
    if sdk_ok "$cand"; then SDK="$cand"; break; fi
  done
fi

sdk_ok "$SDK" || die "no usable kernel SDK (need .../Kernel.framework/Headers).
       Install Xcode CLT:  xcode-select --install
       or pass --sdk /path/to/MacKernelSDK"
ok "SDK: $SDK"
[ "$SDK" = "/opt/MacKernelSDK" ] || info "using CLT SDK, not /opt/MacKernelSDK (proven path on this host)"

# ── 2. version bump (auxKC identity) ─────────────────────────────────────────
step "Version"
OLD_VER="$($PLB -c 'Print :CFBundleVersion' "$PLIST_SRC" 2>/dev/null)"
SHORT_VER="$($PLB -c 'Print :CFBundleShortVersionString' "$PLIST_SRC" 2>/dev/null)"
[ -n "$OLD_VER" ] || die "cannot read CFBundleVersion from $PLIST_SRC"

if [ "$DO_BUMP" = "1" ]; then
  NEW_VER="$(printf '%s' "$OLD_VER" | awk -F. '{printf "%s.%s.%d", $1, $2, $3+1}')"
  $PLB -c "Set :CFBundleVersion $NEW_VER" "$PLIST_SRC" >/dev/null \
    || die "failed to write CFBundleVersion"
  ok "CFBundleVersion $OLD_VER -> $NEW_VER   (short $SHORT_VER)"
  info "auxKC keys off this string — the bump is what makes the new binary actually load"
else
  NEW_VER="$OLD_VER"
  warn "--no-bump: CFBundleVersion stays $OLD_VER"
  [ "$DO_INSTALL" = "1" ] && warn "installing without a bump can leave auxKC serving the previous binary"
fi

mkdir -p "$DIST_DIR"

# ── 3. build ─────────────────────────────────────────────────────────────────
step "Build"
if command -v make >/dev/null 2>&1; then :; else die "make not found"; fi

MAKE_ARGS=(KERNEL_SDK_DIR="$SDK" MYINTELGPU_DIAGNOSTICS="$DIAG")

# Stale .o files are a proven cause of "Invalid Parameter" at load time, so the
# build is always from clean. `make clean` must be its own invocation: naming a
# goal suppresses the default `all` goal.
say "  ${C_DIM}make clean${C_OFF}"
make clean >/dev/null 2>&1

say "  ${C_DIM}make ${MAKE_ARGS[*]}${C_OFF}"
if ! make "${MAKE_ARGS[@]}" >"$LOG" 2>&1; then
  printf '\n%s\n' "──── build failed — last 40 lines of $LOG ────"
  tail -40 "$LOG"
  printf '%s\n' "───────────────────────────────────────────────"
  die "build failed (full log: $LOG)"
fi

NWARN="$(grep -c 'warning:' "$LOG" 2>/dev/null || true)"
NWARN="${NWARN:-0}"
NOBJ="$(ls -1 ./*.o 2>/dev/null | wc -l | tr -d ' ')"
[ -f "$BINARY" ] || die "make exited 0 but $BINARY does not exist — check $LOG"
ok "compiled $NOBJ objects -> $BINARY"
if [ "$NWARN" -gt 0 ]; then
  info "compiler warnings: $NWARN (pre-existing, not gating)"
  grep 'warning:' "$LOG" | sed -n '1,5p' | while IFS= read -r w; do info "$w"; done
fi

# ── 4. verify gates ──────────────────────────────────────────────────────────
step "Verify gates"
G=0
gate() { G=$((G+1)); }

gate; [ -f "$BINARY" ] && ok "G$G  binary present" || { bad "G$G  binary missing: $BINARY"; }
FT="$(file -b "$BINARY" 2>/dev/null)"
gate; printf '%s' "$FT" | grep -q 'kext bundle' && printf '%s' "$FT" | grep -q 'x86_64' \
  && ok "G$G  Mach-O type: $FT" || bad "G$G  wrong Mach-O type: $FT"

gate; plutil -lint "$PLIST_OUT" >/dev/null 2>&1 \
  && ok "G$G  Info.plist lints clean" || bad "G$G  Info.plist is malformed"

NMDEF="$(nm "$BINARY" 2>/dev/null | grep -E ' [tT] _VblankEvent$')"
gate; [ -n "$NMDEF" ] \
  && ok "G$G  VblankEvent defined locally (${NMDEF%% *})" \
  || bad "G$G  VblankEvent not defined in the binary"

gate; nm -u "$BINARY" 2>/dev/null | grep -q '_VblankEvent' \
  && bad "G$G  VblankEvent is UNDEFINED — macOS 15+ removed it; needs a local body" \
  || ok "G$G  VblankEvent is not an undefined symbol"

gate; nm "$BINARY" 2>/dev/null | grep -q 'stack_chk' \
  && bad "G$G  __stack_chk present — needs -fno-stack-protector" \
  || ok "G$G  no __stack_chk symbols"

BADDY="$(nm -u "$BINARY" 2>/dev/null | grep -E '_dyld_|__mh_dylib|_malloc$|_printf$' | tr '\n' ' ')"
gate; [ -z "$BADDY" ] \
  && ok "G$G  no userspace/dyld undefined symbols (kext-safe link)" \
  || bad "G$G  userspace symbols leaked in: $BADDY"

SIZE="$(stat -f%z "$BINARY" 2>/dev/null || echo 0)"
gate; if [ "$SIZE" -gt 0 ] && [ "$SIZE" -le 262144 ]; then ok "G$G  size ${SIZE} B (budget ok)"
        else bad "G$G  size ${SIZE} B outside 0..256K budget"; fi

OUT_VER="$($PLB -c 'Print :CFBundleVersion' "$PLIST_OUT" 2>/dev/null)"
gate; [ "$OUT_VER" = "$NEW_VER" ] \
  && ok "G$G  bundle CFBundleVersion = $OUT_VER (matches source = auxKC identity ok)" \
  || bad "G$G  bundle version $OUT_VER != source $NEW_VER"

# the 3.1.29/3.1.39 regression: Apple appends .bundle itself
BB_N="$(strings "$BINARY" 2>/dev/null | grep -c '\.bundle\.bundle' || true)"
BB_N="${BB_N:-0}"
gate; [ "${BB_N:-0}" -eq 0 ] \
  && ok "G$G  no '.bundle.bundle' in binary (the 3.1.39 dlopen-fail regression)" \
  || bad "G$G  '.bundle.bundle' x$BB_N in binary — consumer dlopen will fail"

PL_BB="$(grep -ac '\.bundle\.bundle' "$PLIST_OUT" 2>/dev/null || true)"
PL_BB="${PL_BB:-0}"
gate; [ "${PL_BB:-0}" -eq 0 ] \
  && ok "G$G  no '.bundle.bundle' in Info.plist personality names" \
  || bad "G$G  '.bundle.bundle' x$PL_BB in Info.plist — fix the bundle names"

BNAMES="$($PLB -c 'Print :IOKitPersonalities' "$PLIST_OUT" 2>/dev/null \
         | grep -aE '(^|[A-Za-z])BundleName|MetalPluginName' | sed 's/^[[:space:]]*//')"
BADN="$(printf '%s\n' "$BNAMES" | grep -aE '\.bundle$' | tr '\n' ' ')"
gate; [ -z "$BADN" ] \
  && ok "G$G  personality bundle names are suffix-less (loader appends .bundle itself)" \
  || bad "G$G  bundle name carries a .bundle suffix: ${BADN}
              loader appends .bundle -> resolves to '*.bundle.bundle' -> dlopen fails
              -> AppleGVA/IOAccel/Metal consumer never attaches -> software render.
              Reference on this machine, /System/Library/Extensions/AppleIntelKBLGraphics.kext:
                MetalPluginName = AppleIntelKBLGraphicsMTLDriver   (no suffix)"

if $PLB -c 'Print :OSBundleRequired' "$PLIST_OUT" >/dev/null 2>&1; then
  gate; warn "G$G  OSBundleRequired present — NOT auto-removed on purpose: every proven-good
              build of this kext (3.1.35 / 3.1.38 / 3.1.40) shipped with it and loaded.
              Treat the generic 'remove OSBundleRequired' advice as stale for this project."
else
  gate; ok "G$G  OSBundleRequired absent"
fi

if otool -l "$BINARY" 2>/dev/null | grep -q LC_KEXT_BUNDLE; then
  gate; ok "G$G  LC_KEXT_BUNDLE present"
else
  gate; info "G$G  LC_KEXT_BUNDLE absent (MH_KEXT_BUNDLE filetype set instead — normal for this build)"
fi

UUID="$(dwarfdump --uuid "$BINARY" 2>/dev/null | awk '{print $2}')"
gate; [ -n "$UUID" ] && ok "G$G  UUID $UUID" || info "G$G  UUID unavailable (no dSYM)"

SHA_MD5="$(md5 -q "$BINARY" 2>/dev/null || true)"
gate; [ -n "$SHA_MD5" ] \
  && ok "G$G  md5 $SHA_MD5   sha256 $(shasum -a 256 "$BINARY" 2>/dev/null | cut -c1-16)…" \
  || bad "G$G  cannot hash the binary"

# ── 5. artifacts ─────────────────────────────────────────────────────────────
step "Artifacts"
rm -f "$DIST_DIR/$KEXT.zip"
rm -f "$DIST_DIR/$KEXT-"*.pkg
if ditto -c -k --sequesterRsrc --keepParent "$KEXT" "$DIST_DIR/$KEXT.zip" 2>/dev/null; then
  ok "zip  $DIST_DIR/$KEXT.zip ($(stat -f%z "$DIST_DIR/$KEXT.zip") B)"
else
  bad "zip creation failed"
fi

if [ "$DO_PKG" = "1" ]; then
  rm -rf "$BUILD_DIR/pkgroot" "$BUILD_DIR/pkgscripts"
  mkdir -p "$BUILD_DIR/pkgroot/Library/Extensions" "$BUILD_DIR/pkgscripts"
  cp -R "$KEXT" "$BUILD_DIR/pkgroot/Library/Extensions/"
  cp packaging/postinstall "$BUILD_DIR/pkgscripts/postinstall" 2>/dev/null
  if pkgbuild --root "$BUILD_DIR/pkgroot" --scripts "$BUILD_DIR/pkgscripts" \
       --identifier "$BUNDLE_ID" --version "$NEW_VER" --ownership recommended \
       "$DIST_DIR/$KEXT-$NEW_VER.pkg" >"$BUILD_DIR/pkgbuild.log" 2>&1; then
    ok "pkg  $DIST_DIR/$KEXT-$NEW_VER.pkg ($(stat -f%z "$DIST_DIR/$KEXT-$NEW_VER.pkg") B)"
    info "its postinstall only does the generic kextcache path; for a real refresh use ./build.sh --install"
  else
    bad "pkgbuild failed (see $BUILD_DIR/pkgbuild.log)"
  fi
fi

# ── 6. optional: OpenCore EFI ────────────────────────────────────────────────
if [ "$DO_EFI" = "1" ]; then
  step "OpenCore EFI"
  MOUNT="$EFI_MOUNT"
  if [ -z "$MOUNT" ]; then
    for m in /Volumes/EFI /Volumes/EFI-MAIN /Volumes/ESP /Volumes/*; do
      if [ -d "$m/EFI/OC" ]; then MOUNT="$m"; break; fi
    done
  fi
  if [ -n "$MOUNT" ] && [ -d "$MOUNT/EFI/OC/Kexts" ]; then
    sudo rm -rf "$MOUNT/EFI/OC/Kexts/$KEXT"
    sudo cp -R "$KEXT" "$MOUNT/EFI/OC/Kexts/$KEXT"
    sudo chown -R root:wheel "$MOUNT/EFI/OC/Kexts/$KEXT"
    ok "copied to $MOUNT/EFI/OC/Kexts/$KEXT"
    warn "OpenCore overwrites NVRAM boot-args from config.plist — edit config, don't expect a merge"
  else
    bad "no OpenCore EFI found (looked for EFI/OC/Kexts). Mount it and rerun: ./build.sh --efi /Volumes/<ESP>"
  fi
fi

# ── 7. optional: install + real auxKC rebuild ────────────────────────────────
if [ "$DO_INSTALL" = "1" ]; then
  step "Install to /Library/Extensions"
  AUX_BEFORE=""; [ -f "$AUXKC" ] && AUX_BEFORE="$(stat -f%m "$AUXKC")"

  if [ -d "$L_E" ]; then
    STRAY="$(find /Library/Extensions -maxdepth 1 -name "$KEXT.bak*" 2>/dev/null)"
    if [ -n "$STRAY" ]; then
      printf '%s\n' "$STRAY" | while IFS= read -r s; do bad "stale backup bundle beside the live kext: $s"; done
      die "move these out of /Library/Extensions before installing (duplicate CFBundleIdentifier):
       sudo mv /Library/Extensions/$KEXT.bak-* \"$BACKUP_ROOT/\""
    fi
    mkdir -p "$BACKUP_ROOT" || die "cannot create backup dir $BACKUP_ROOT"
    BAK="$BACKUP_ROOT/$KEXT.bak-$(date +%Y%m%d-%H%M%S)"
    sudo cp -R "$L_E" "$BAK" && ok "backed up live kext -> $BAK"
  fi

  # rsync -a carries the source modes across; no chmod, so nothing here can
  # silently rewrite permissions the way chmod -R 755 did.
  sudo rsync -a --delete "$KEXT/" "$L_E/" || die "rsync into $L_E failed — live kext untouched"
  sudo chown -R root:wheel "$L_E"
  sudo touch /Library/Extensions
  ok "installed $L_E ($(sudo $PLB -c 'Print :CFBundleVersion' "$L_E/Contents/Info.plist"))"

  step "Rebuild auxKC (explicit kmutil create — kextcache/install are no-ops on KDK mismatch)"
  KCARGS=()
  for k in /Library/Extensions/*.kext; do KCARGS+=(-p "$k"); done
  if sudo kmutil create -n aux -B "$BKC" -S "$SKEKC" -A "$AUXKC" \
        "${KCARGS[@]}" -M --allow-missing-kdk >"$BUILD_DIR/kmutil.log" 2>&1; then
    sudo kcditto 2>/dev/null || warn "kcditto failed — Preboot may lag"
    AUX_AFTER="$(stat -f%m "$AUXKC" 2>/dev/null)"
    if [ -n "$AUX_BEFORE" ] && [ "$AUX_AFTER" = "$AUX_BEFORE" ]; then
      bad "auxKC mtime unchanged — the new binary will NOT be loaded"
    else
      ok "auxKC rebuilt (mtime $(date -r "$AUXKC" '+%H:%M:%S'))"
    fi
  else
    bad "kmutil create failed — see $BUILD_DIR/kmutil.log"
    tail -5 "$BUILD_DIR/kmutil.log" | while IFS= read -r l; do info "$l"; done
  fi

  say ""
  say "  Next step is YOUR call:  sudo reboot"
  say "  After boot:  kmutil showloaded | grep pongpan   ·   ioreg -l -c $TARGET -w0 | grep model"
fi

# ── 8. verdict ───────────────────────────────────────────────────────────────
head1
if [ "$FAILURES" -eq 0 ]; then
  say "  ✅ BUILD OK — MyIntelGPU $NEW_VER  ·  ${SIZE} B  ·  md5 ${SHA_MD5}"
  [ "$WARNINGS" -gt 0 ] && say "     ($WARNINGS warning(s), $NWARN compiler warning(s) — none gating)"
  say ""
  say "     kext    : $KEXT"
  say "     log     : $LOG"
  say "     dist    : $DIST_DIR/$KEXT.zip"
  [ "$DO_PKG" = "1" ] && say "     pkg     : $DIST_DIR/$KEXT-$NEW_VER.pkg"
  [ "$DO_INSTALL" = "0" ] && say ""
  [ "$DO_INSTALL" = "0" ] && say "     deploy : sudo ./build.sh --install      (this script never reboots)"
  head1
  exit 0
else
  say "  ❌ $FAILURES gate(s) FAILED — do NOT deploy this binary"
  say "     log: $LOG"
  head1
  exit 1
fi
