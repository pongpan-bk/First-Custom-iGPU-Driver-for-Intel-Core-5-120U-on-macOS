#!/bin/bash
# build_plugin.sh - Build MyIntelGPU Lilu plugin on Mac (Ventura/Sequoia)
# Location: G:\MyIntelGPU-connector\  (Windows) -> ~/Documents/MyIntelGPU-connector/ (Mac)
# Prerequisites: Xcode CLT (xcode-select --install), git

set -e
echo "=== MyIntelGPU Lilu Plugin Build (v3.0.1) ==="
echo "PWD: $(pwd)"
echo "Date: $(date)"

# 1. Clone dependencies if missing
if [ ! -d "../Lilu" ]; then
  echo "[1/6] Cloning Lilu..."
  git clone https://github.com/acidanthera/Lilu.git ../Lilu
else
  echo "[1/6] Lilu exists: $(ls -d ../Lilu)"
fi
if [ ! -d "/opt/MacKernelSDK" ] && [ ! -d "../MacKernelSDK" ]; then
  echo "Cloning MacKernelSDK to /opt..."
  sudo git clone https://github.com/acidanthera/MacKernelSDK.git /opt/MacKernelSDK || git clone https://github.com/acidanthera/MacKernelSDK.git ../MacKernelSDK
fi

# 2. Clean old builds
echo "[2/6] Clean..."
make clean || true

# 3. Build plugin
echo "[3/6] Building plugin (LILU_DIR=../Lilu PRODUCT_NAME=MyIntelGPU MODULE_VERSION=3.0.1)..."
make LILU_DIR=../Lilu PRODUCT_NAME=MyIntelGPU MODULE_VERSION=3.0.1 KERNEL_SDK_DIR=/opt/MacKernelSDK 2>&1 | tee /tmp/myintel-build.log
echo "Build log at /tmp/myintel-build.log"
ls -lh MyIntelGPU.kext/Contents/MacOS/MyIntelGPU || ls -lh MyIntelGPU.kext

# 4. Validate plist
echo "[4/6] Validate Info.plist..."
plutil -lint MyIntelGPU.kext/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" MyIntelGPU.kext/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Print :OSBundleLibraries:as.vit9696.Lilu" MyIntelGPU.kext/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Print :IOKitPersonalities:MyIntelGPU:IOProviderClass" MyIntelGPU.kext/Contents/Info.plist

# 5. Remove old standalone L/E kext (conflicts: class-match 9999)
echo "[5/6] Remove old standalone L/E kext (if exists)..."
if [ -d "/Library/Extensions/MyIntelGPU.kext" ]; then
  echo "Found old L/E kext - removing (sudo)..."
  sudo rm -rf /Library/Extensions/MyIntelGPU.kext
  sudo kmutil clear-staging || true
  sudo kextcache --clear-staging || true
  echo "Old L/E removed. AuxKC will be rebuilt on next reboot."
else
  echo "No L/E kext - clean"
fi

# 6. Install to OC EFI (assume EFI mounted at /Volumes/EFI)
echo "[6/6] Install to EFI..."
EFI_OC="/Volumes/EFI/EFI/OC/Kexts"
if [ -d "$EFI_OC" ]; then
  echo "Copying to $EFI_OC/MyIntelGPU.kext..."
  sudo rm -rf "$EFI_OC/MyIntelGPU.kext"
  sudo cp -R MyIntelGPU.kext "$EFI_OC/MyIntelGPU.kext"
  sudo chown -R root:wheel "$EFI_OC/MyIntelGPU.kext"
  sudo chmod -R 755 "$EFI_OC/MyIntelGPU.kext"
  ls -lh "$EFI_OC/MyIntelGPU.kext/Contents/MacOS/"
  echo "EFI install done."
else
  echo "WARNING: EFI not mounted at /Volumes/EFI. Mount manually:"
  echo "  sudo diskutil mount EFI   # or mount EFI partition"
  echo "  Then: sudo cp -R MyIntelGPU.kext /Volumes/EFI/EFI/OC/Kexts/"
  echo "Staging copy ready at: $(pwd)/MyIntelGPU.kext"
fi

echo ""
echo "=== Build complete ==="
echo "Next: REBOOT and verify:"
echo "  log show --last boot --predicate 'eventMessage CONTAINS \"myigfx\"' | tail -100"
echo "  ioreg -l -n IGPU | grep -E 'device-id|model|built-in'"
echo "  kextstat | grep -E 'Lilu|MyIntelGPU'"
echo "  # Expect: myigfx hooked configRead, device-id 0x9A49, IGPU renamed, built-in present"
echo "  # AppleIntelTGLGraphicsFramebuffer should now match 0x9A49 (TGL GT2 80EU)"
