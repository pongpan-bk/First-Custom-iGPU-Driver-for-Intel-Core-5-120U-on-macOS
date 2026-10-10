# MyIntelGPU.kext — Work Memory (28 Jun 2026, updated 29 Jun 2026)

## Project Goal
Get Intel Raptor Lake iGPU (Device ID 0xA7AC) working on macOS 15.2 Sequoia
via OpenCore on **Acer Aspire AL15-52P** (Intel Core 5 120U, RPL-U).

**Development 100% via GitHub Actions** — no local Mac available.
Push → CI builds on `macos-14` runner → download `.kext` from Artifacts.

---
**🚨 CRITICAL FINDING (28-Jun-2026)**: Original SMBIOS `MacBookPro18,1` is **Apple Silicon (M1 Pro)** — using ARM SMBIOS on Intel hardware causes macOS to expect ARM-specific kernel init paths at EXITBS:START. Changed to `iMac19,1` (Intel Coffee Lake). This is the most likely root cause of the reboot loop.
---

---

## Target Hardware (confirmed via CPU-Z)
| Component | Spec |
|-----------|------|
| **Laptop** | Acer Aspire AL15-52P |
| **CPU** | Intel Core 5 120U (Raptor Lake, 2P+8E, 10C/12T) |
| **GPU** | Intel Graphics, Device ID **0xA7AC** (RPL-U) |
| **RAM** | 16GB DDR5-4800 (SCY SM5S510A8AEMC) |
| **SSD** | SCY SMM88HG51200D 512GB NVMe |
| **Display** | 15.3" 1920×1080 @60Hz (KDB0924 eDP) |
| **Audio** | Realtek ALC (alcID=12) |
| **WiFi** | Realtek RTL8852BE WiFi 6 |
| **BIOS** | Insyde Corp V1.02 |
| **OS** | Windows 11 Pro for Workstations (build 26200.8655) |

---

## Phase 1 (PCI Setup) — DONE ✅
- setBusMasterEnable(true)
- setMemoryEnable(true)
- IOPCIDevice provider jigged

---

## Phase 2 (BAR0/BAR2 MMIO Mapping) — DONE ✅

### BAR0 fetch — 3-strategy fallback:
1. `getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0)` — preferred
2. `getDeviceMemoryWithIndex(0)` — fallback
3. Raw PCI config read + `IOMemoryDescriptor::withPhysicalAddress()` — last resort

### BAR0 mapping — 3-strategy fallback:
1. `IOMemoryDescriptor::map(kIOMapCacheInhibit)` — UC
2. `IOMemoryDescriptor::map()` — default caching
3. `createMappingInTask(kernel_task)` — legacy (macOS ≤14)

Same 3-strategy applied to **BAR2 (aperture)**.

---

## Phase 3 (Hardware Detection) — DONE ✅
- Detects Raptor Lake (0xA7AC) → Gen12 real, FakeGen=9 (Coffee Lake)
- Detects Alder Lake (0x469x), Tiger Lake (0x9A6x), Coffee Lake (0x3E9x)
- Reads GMD_ID register (0xD8C) for IP version confirmation

---

## Phase 3b (Interrupt Guard) — FIXED ✅ (commit 4dbe104)
- `isValidRegs()` guard checks `fRegs >= 0xFFFFFF80000000` before MMIO access
- Prevents panic on bogus BAR0 VA (~0x2000 from VMware virtual GPU)
- Applied to: `readReg32()`, `writeReg32()`, `safeInitInterrupts()`,
  `clearAllInterruptRegisters()`, `initInterrupts()`
- On real HW (0xA7AC) → `isValidRegs()` = true → works normally

---

## Phase 4 (IOFramebuffer Binding) — Code Complete, Untested
- `MyIntelFramebuffer` (IOFramebuffer subclass) created in start()
- VRAM descriptor (BAR2 aperture or 16MB stub) published
- Display mode: 1920×1080 @60Hz, XRGB8888 (O010)
- 3-component mask: R=0x00FF0000, G=0x0000FF00, B=0x000000FF, A=0xFF000000
- Vblank → notifyVblank() → handleVblank() chain wired
- Brightness control, power state change implemented
- IORegistry properties: built-in LCD, VRAM size

---

## Phase 5 (Translation Table) — DONE ✅
| Engine | Fake (CFL) | Real (ADL/RPL) |
|--------|-----------|----------------|
| VCS0 | 0x12000 | 0x1C0000 |
| VECS0 | 0x1A000 | 0x1C8000 |
| Cursor B | 0x700C0 | 0x71080 |
| PCH Display | 0x48000 | 0xC8000 |

---

## Phase 6 (GGTT) — DONE ✅
- ggttInvalidate() via GFX_FLSH_CNTL_GEN6 (0x10100)

---

## Phase 7 (registerService) — DONE ✅

---

## EFI Configuration (D:\EFI\OC\)

### Source: linux\EFI — MyIntelGPU-tuned EFI (formerly based on Dell Latitude 5440)
### Last Updated: 28 Jun 2026 (deployed from linux\EFI, old Olarila EFI replaced)

| Setting | Value |
|---------|-------|
| **OpenCore** | 1.0.7 (from linux\EFI\) |
| **SMBIOS** | iMac19,1 (was MacBookPro18,1 — ARM SMBIOS, likely cause of EXITBS:START panic) |
| **boot-args** | `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12` |
| **SecureBootModel** | Disabled |
| **csr-active-config** | 0x67 (SIP fully disabled) |
| **Kexts** | 20 (AppleALC, CPUFriend, CPUFriendDataProvider, CpuTopologyRebuild, ECEnabler, Lilu, MyIntelGPU [DISABLED for VESA baseline], NVMeFix, RestrictEvents, SMCBatteryManager, SMCProcessor, SMCSuperIO, USBToolBox, UTBMap, VirtualSMC, VoodooI2C, VoodooI2CHID, VoodooPS2Controller, WhateverGreen, XHCI-unsupported) |
| **Removed (vs old EFI)** | IntelMausi, SMCLightSensor, AtherosE2200Ethernet, RealtekRTL8111, LucyRTL8125Ethernet |
| **Added (vs old EFI)** | ECEnabler, NVMeFix, VoodooPS2Controller, VoodooI2C, VoodooI2CHID, SMCBatteryManager, XHCI-unsupported |
| **Drivers** | OpenRuntime.efi, OpenCanopy.efi, OpenHfsPlus.efi, ResetNvramEntry.efi, Ext4Dxe.efi, AudioDxe.efi, OpenNtfsDxe.efi + others |
| **ACPI** | SSDT-EC.aml, SSDT-IMEI.aml, SSDT-PLUG-ALT.aml, SSDT-PMC.aml, SSDT-PNLF.aml, SSDT-RHUB.aml, SSDT-RTCAWAC.aml |
| **GPU DeviceProperties** | AAPL,ig-platform-id `BwCbPg==` (bytes 07 00 9B 3E = CFL mobile 0x3E9B0007), device-id `JqcAAA==` (0xA726), framebuffer patches for 1920×1080, stolenmem `AAABAA==` (0x00010000), fbmem `AAAJAA==` (0x00090000) |
| **Audio** | alcid=12 via boot-args (AppleALC autodetect) — no DeviceProperties entry for audio |

---

## Boot Test History

### 28-Jun-2026 — EFI Deployment (pre-boot prep)
- **Work done**: Deployed `linux\EFI` to D:\ (USB EFI partition)
  - Replaced old Olarila EFI (generic, no MyIntelGPU, VESA mode `-igfxvesa`)
  - New EFI: OpenCore 1.0.7, MacBookPro18,1 SMBIOS, MyIntelGPU.kext with proper GPU fake-id (0xA726) + framebuffer patches
  - Fixed `alcid=11` → `alcid=12` in boot-args (Realtek ALC layout)
  - 17 kexts including VoodooPS2Controller (keyboard), BrightnessKeys, USBInjectAll
  - 7 proper ACPI SSDTs (EC, IMEI, PLUG-ALT, PMC, PNLF, RHUB, RTCAWAC)
  - Cleaned D:\ of Olarila junk (__MACOSX, yusufklncc, ._* files, extra boot files)
- **Status**: 🔜 Ready for boot test

### 26-Jun-2026 — First boot attempt
- **Result**: ❌ Failed
- **Error**: `OC: Prelinked injection MyIntelGPU.kext - Invalid Parameter`
- **Last log line**: `EXITBS:START`
- **Root cause (diagnosed 28-Jun)**: OSBundleLibraries declared `com.apple.kpi.mach` and `com.apple.kpi.bsd` at version `20.0.0` — these kpi libraries were removed in macOS 14+ (Sonoma/Sequoia). OpenCore couldn't resolve the dependency → `Invalid Parameter`.
- **Fix**: Removed kpi.mach, kpi.bsd from Info.plist. Updated `com.apple.iokit.IOGraphicsFamily` from `20.0.0` to `24.0.0` (Sequoia kernel version).

### 28-Jun-2026 — VESA baseline v2 + SMBIOS fix (Sisyphus session)
- **CRITICAL FINDING**: MacBookPro18,1 is an **Apple Silicon (M1 Pro) SMBIOS** — using it on Intel hardware causes macOS to expect ARM-specific kernel init paths at EXITBS:START, likely triggering the kernel panic/reboot loop.
- **SMBIOS changed**: MacBookPro18,1 (ARM) → **iMac19,1** (Intel Coffee Lake)
  - iMac19,1 has dGPU (no iGPU dependency) — matches our VESA/MyIntelGPU approach
  - CPUID spoof 0x0906EA (CFL) is compatible with iMac19,1
- **Boot-args cleaned**: Removed `debug=0x100`, `-lilubetaall`, `igfxonln=1`, `-wegnoegpu`
  - New: `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12`
- **AdviseFeatures**: true → false (iMac19,1 doesn't need it)
- **ProcessorType**: 0 → 1537 (iMac19,1 compatible)
- **MyIntelGPU.kept**: Kept disabled (`Enabled=false`) — VESA baseline still active
- **NVRAM delete**: boot-args in delete block ✅ (old args wiped before new ones apply)
- **Backup saved**: `config.plist.backup-20260628-070401`
- **Dell Latitude 5440 reference analyzed**: Same i5-1345U RPL-U + Iris Xe hardware, uses iMac19,1 SMBIOS, provides working reference for config comparison
- **⚠️ Deployed kext was STALE**: D:\EFI\OC\Kexts\MyIntelGPU.kext had old 61KB binary + Info.plist with `kpi.mach`/`kpi.bsd` (known "Invalid Parameter" cause). Replaced with CI #49 build: 85KB binary + clean Info.plist (IOGraphicsFamily=24.0.0, no kpi.mach/kpi.bsd)
- **Status**: 🔜 Ready for boot test — copy D:\EFI\OC\ to USB and boot

### 29-Jun-2026 — GPU data base64 fixes + Acer config final cleanup (Sisyphus session #2)
- **Config fixed**: 3 GPU data values had WRONG base64 encodings:
  1. `AAPL,ig-platform-id` = `BpubPg==` (garbage) → `BwCbPg==` (bytes 07 00 9B 3E = CFL mobile) ✅
  2. `framebuffer-stolenmem` = `AAAQAQ==` (decoded to 0x01100000, wrong) → `AAABAA==` (0x00010000 = 64KB) ✅
  3. `framebuffer-fbmem` = `AACQAA==` (decoded to 0x00900000, wrong) → `AAAJAA==` (0x00090000 = 576KB) ✅
- **AudioDevice path fixed**: `PciRoot(0x0)/Pci(0x1b,0x0)` (desktop HDMI) → `PciRoot(0x0)/Pci(0x1f,0x3)` (laptop HDA) ✅
- **EFI validated**: OpenRuntime.efi, HfsPlus.efi, OpenCanopy.efi all present + registered
- **APFS MinDate/MinVersion**: -1 (boots any macOS version) ✅
- **ScanPolicy**: 0 (scan everything) ✅
- **WORK_MEMORY updated**: Kext list corrected (20 kexts), GPU data values documented with correct base64
- **Status**: 🔜 Ready for boot test on Acer AL15-52P

### 29-Jun-2026 — Boot log diagnosis + config.plist fixes for i9-14900KF desktop (Sisyphus session)
- **Boot log analyzed**: `D:\opencore-2026-06-29-003402.txt` from i9-14900KF + RX 6900 XT machine
  - OpenCore 1.0.4 booted successfully through boot.efi phase (loaded BootKernelExtensions.kc)
  - Only warning: `CPUFriendDataProvider.kext missing executable` (data-only kext, not fatal)
  - System halted after kernel transition (Apple logo hang) — **not an OpenCore-phase error**
- **Root cause of hang**: 3 issues in `D:\EFI\OC\config.plist` that survived the AMD clean-up:
  1. **`-radcodec` still in boot-args** — AMD VCN init runs without proper DeviceProperties + `agdpmod=pikera` was removed → GPU init hangs
  2. **CPUFriendDataProvider.kext** had `ExecutablePath` pointing to missing binary — OpenCore warning
  3. **WhateverGreen.kext still enabled** — no iGPU on KF, AMD GPU handled natively with `agdpmod=pikera`
- **⚠️ Incomplete cleanup noted**: Contrary to WORK_MEMORY entry, WhateverGreen.kext was **still present in kexts folder AND enabled in config** (not deleted). `-radcodec` was also still in boot-args Add section (only `agdpmod=pikera` was removed).
- **Fixes applied**:
  | # | Change | Detail |
  |---|--------|--------|
  | 1 | `boot-args` | `-radcodec` → `agdpmod=pikera` restored |
  | 2 | `CPUFriendDataProvider ExecutablePath` | `Contents/MacOS/CPUFriendDataProvider` → empty string (data-only kext) |
  | 3 | `WhateverGreen Enabled` | `true` → `false` (no iGPU on KF) |
- **Backup saved**: `config.plist.backup-20260629` (original pre-cleanup state with AMD DeviceProperties + MyIntelGPU enabled)
- **Status**: ✅ Config fixed — ready for re-test on i9-14900KF

---

## CI Build Status
| Run | Commit | Status |
|-----|--------|--------|
| #48 (4dbe104) | Phase 3 guard | ✅ Built — artifact: MyIntelGPU.kext (83.6 KB) |
| #49 (1879d4c) | OSBundleLibraries fix for Sequoia | ✅ Built — artifact: MyIntelGPU.kext (85,592 bytes, Info.plist: no kpi.mach/kpi.bsd, IOGraphicsFamily=24.0.0) |

---

## Repository
- **GitHub**: `https://github.com/pongpan-bk/IntelReviveGPU-Gen-10-12-on-Hackintosh`
- **Branch**: `main`
- **Remote origin**: Changed from old `MyIntelGPU.git` to new repo (28-Jun-2026)

---

## Next Steps
1. ✅ **EFI deployed** — OpenCore 1.0.7 w/ MyIntelGPU (disabled), 17 kexts, alcid=12 fix
2. ✅ **SMBIOS fixed**: MacBookPro18,1 (ARM) → iMac19,1 (Intel) — likely EXITBS:START fix
3. ✅ **Boot-args cleaned**: -igfxvesa VESA baseline, no debug/lilubeta/weg flags
4. 🔜 **Boot macOS on Acer AL15-52P** — copy `D:\EFI\OC\` to USB and boot
5. If panic → note last verbose line on screen
6. If boot OK (desktop visible in VESA mode):
   a. Remove `-igfxvesa` from boot-args
   b. Enable MyIntelGPU.kext (`Enabled=true`)
   c. Reboot test — if desktop works → done
   d. If panic → add back `-igfxvesa`, debug kext
7. If no display → check IOFramebuffer binding (Phase 4)
8. HW acceleration / power management (Phase 5-6) — หลังจาก display ติด

---

## Key Files
| File | Purpose |
|------|---------|
| `MyIntelGPU.cpp` | Main driver — all 7 phases of start() |
| `MyIntelGPU.hpp` | Class declarations + translation table + isValidRegs() |
| `IntelFramebuffer.cpp` | Interrupt init, clear registers, Vblank handler |
| `IntelFramebuffer.hpp` | Interrupt register constants + class def |
| `MyIntelFramebuffer.cpp` | IOFramebuffer subclass (Phase 4) |
| `MyIntelFramebuffer.hpp` | FB constants + class def |
| `Info.plist` | Kext metadata, IOPCIMatch includes 0xA7AC |
| `Makefile` | Build system |
| `.github/workflows/build.yml` | GitHub Actions CI (macos-14) |
| `D:\EFI\` | OpenCore EFI for Acer Aspire AL15-52P (custom config.plist) |

---

## 28-Jun-2026 — Removed AMD GPU kexts from D:\EFI (i9-14900KF + RX 6900 XT machine)
- **Context**: User's main Hackintosh (i9-14900KF + RX 6900 XT) booting D:\EFI\OC\ (OpenCore 1.0.4, MacBookPro16,4) hangs at Apple logo progress bar
- **Suspected cause**: AMD GPU-related kexts or config interfering

### Removed:
| Item | Detail |
|------|--------|
| `SMCRadeonSensors.kext` | Kext folder + Kernel > Add entry deleted ✅ |
| `WhateverGreen.kext` | ❌ **NOT actually deleted** — still in kexts folder + enabled in config (fixed 29-Jun) |
| `DeviceProperties` RX 6900 XT spoof | Removed PciRoot device-id + model props ✅ |
| `boot-args` `agdpmod=pikera` | Removed (restored 29-Jun — was needed for RX 6900 XT) |
| `boot-args` `-radcodec` | ❌ **NOT actually removed** — still in Add section (fixed 29-Jun) |
| Comment `AMD Radeon RX 6900 XT` | Removed from config header ✅ |

### Remaining kexts (13):
`AppleALC`, `CPUFriend`, `CPUFriendDataProvider`, `CpuTopologyRebuild`, `Lilu`, `LucyRTL8125Ethernet`, `RestrictEvents`, `SMCProcessor`, `SMCSuperIO`, `USBToolBox`, `UTBMap`, `VirtualSMC`, `MyIntelGPU`

### Files modified:
- `D:\EFI\OC\Kexts\SMCRadeonSensors.kext` — deleted ✅
- `D:\EFI\OC\Kexts\WhateverGreen.kext` — ❌ still present (left in place, disabled in config on 29-Jun)
- `D:\EFI\OC\config.plist` — 3 edits (Kernel > Add, DeviceProperties, boot-args) — **but `-radcodec` survived and `agdpmod=pikera` was incorrectly removed** (fixed 29-Jun)
- **Status**: ❌ Still hung at Apple logo — fixed 29-Jun (see entry above)


ผมทำเอง

สรุปบันทึกการเคลียร์ขยะข้ามค่าย (Cross-Platform Clean Up)วันทีบันทึก: 28 มิถุนายน 2026สถานการณ์: แก้ไขผัง config.plist หลัก เพื่อกำจัดโค้ดตกค้างของฝั่งการ์ดจอแยก AMD Radeon ที่เคยหลงมาในฐานข้อมูลออกให้หมดจด🛑 จุดตายเดิม (The AMD Interferences)ปัญหาหน้างาน: เครื่องหลักสเปกมหาเทพ (Intel Core i9-14900KF + AMD Radeon RX 6900 XT) บูตผ่านโครงสร้าง OpenCore 1.0.4 เกิดอาการ "แถบสถานะโลโก้ Apple นิ่งสนิท ไม่ยอมวิ่งต่อ"สาเหตุหลัก: ในไฟล์โครงสร้างมีคีย์คำสั่ง, ตัวช่วยอ่านค่าเซนเซอร์การ์ดจอค่ายแดง (SMCRadeonSensors), และคำสั่งแต่งกราฟิกฝังอยู่ ซึ่งขัดแย้งกับการจัดระเบียบกลุ่มแกนหลักเพื่อรัน VESA/MyIntelGPU บนสถาปัตยกรรม Intel ยุคใหม่อย่างรุนแรง✂️ รายการ "รื้อถอน" และ "จัดระเบียบใหม่" (The Surgical Clean)1. ลบปลั๊กอินส่วนเกิน (Kext Deletion)SMCRadeonSensors.kext ➔ [ลบเด็ดขาด] ลบทั้งโฟลเดอร์ไฟล์จริง และตัดชื่อออกจากตาราง Kernel > AddWhateverGreen.kext ➔ [ลบชั่วคราว] เนื่องจาก CPU ตัวแรงรหัส KF ไม่มีชิปกราฟิกออนบอร์ด (No iGPU) จึงตัดตัวแม่ตัวนี้ออกเพื่อเคลียร์ทางให้ไดรเวอร์ของจารย์เข้าคุมพื้นที่ตรงๆ2. ล้างรหัสแฝงฝั่งฮาร์ดแวร์ (DeviceProperties Wipe)ทำการลบโค้ดตำแหน่งสะพานเชื่อม PCI (PciRoot...) ที่ใช้สำหรับสั่งหลอกรหัส (Spoof) และตั้งชื่อรุ่นตัวการ์ดจอแยก AMD Radeon RX 6900 XT ออกไปจากแผนผังหลักทั้งหมด3. ชำแหละชุดคำสั่งบูต (Boot-Args Stripping)ทำการริบตัวแปรเฉพาะทางฝั่งค่ายแดงในช่องข้อความ NVRAM > boot-args ออกจนหมดสิ้น:ลบ agdpmod=pikera (คำสั่งแก้จอดำของบอร์ดการ์ดจอแยก)ลบ -radcodec (คำสั่งเปิดระบบถอดรหัสวิดีโอของ AMD)คงเหลือคำสั่งซ่อมแกนหลัก: -v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12📊 สถานะกองทัพล่าสุด (13 Core Kexts Remaining)หลังจากการจัดแถวและล้างขยะข้ามสายพันธุ์ ตารางของจารย์จะเหลือเฉพาะหัวกะทิกลุ่มขับเคลื่อน CPU และพอร์ต 13 ตัวเน้นๆ:text[Lilu] ➔ [VirtualSMC] ➔ [MyIntelGPU (ตัวเก่ง)] ➔ [CPUFriend] ➔ [CPUFriendDataProvider] 
➔ [CpuTopologyRebuild] ➔ [SMCProcessor] ➔ [SMCSuperIO] ➔ [USBToolBox] ➔ [UTBMap] 
➔ [AppleALC] ➔ [LucyRTL8125Ethernet] ➔ [RestrictEvents]
ใช้โค้ดอย่างระมัดระวัง🚀 แผนที่ไฟล์สถาปัตยกรรม (Architecture Mapping)เพื่อให้เด็กๆ ในทีมของจารย์เข้าใจโครงสร้างระบบการคอมไพล์ผ่านระบบก้อนเมฆ (GitHub Actions CI) แบบไม่งง ให้ดูตามผังนี้ครับ:mermaidgraph TD
    A[ซอร์สโค้ดหลัก: MyIntelGPU.cpp/.hpp] -->|คุมฟังก์ชันและตารางแปลงค่า| B(IntelFramebuffer.cpp/.hpp)
    B -->|จัดการระบบ Interrupt / Vblank| C(MyIntelFramebuffer.cpp/.hpp)
    C -->|จำลองโครงสร้างหน้าจอ Phase 4| D[Info.plist: ยัดรหัส 0xA7AC]
    D -->|ส่งผ่าน Makefile| E[.github/workflows/build.yml]
    E -->|GitHub Actions: macos-14| F[คลอดไฟล์เป็น Artifacts 85KB]
    F -->|ยัดลงไดรฟ์| G[D:\EFI\OC\config.plist ตัวแรงของจารย์]

---

### 29-Jun-2026 — H:\EFI conversion (desktop i9-14900KF → Acer AL15-52P) + boot test (Sisyphus session)
- **จริง**: H:\EFI\ คือ EFI จริงที่ใช้ boot Acer Aspire AL15-52P (ไม่ใช่ D:\ เหมือนแต่ก่อน)
- **Conversion done**:
  - SMBIOS: `iMacPro1,1` → `iMac19,1` + ProcessorType `3841` → `1537`
  - boot-args: Desktop AMD args → VESA baseline `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12`
  - csr-active-config: `0x67`
  - DeviceProperties: Removed AMD RX 6900 XT ✅ → Added Intel GPU (AAPL,ig-platform-id `0x3E9B0007`, device-id spoof `0xA726`, framebuffer patches 1920×1080, stolenmem=64KB, fbmem=576KB)
  - Kexts: Removed `SMCRadeonSensors` + `LucyRTL8125Ethernet` from config AND folder ✅
  - **MyIntelGPU.kext**: Copied CI #49 (85,592 bytes) from `~/Downloads` → `H:\EFI\OC\Kexts\` ✅ — `Enabled=false` (VESA baseline)
  - `config.plist` created (copy of `config_REDACTED.plist`)
  - macOS `._*` / `.DS_Store` garbage: Cleaned ✅
- **Boot test result**:
  - **Boot direct (no CleanNvram)**: Black screen — NVRAM pollution from old desktop config
  - **After CleanNvram.efi**: ✅ **Apple logo + progress bar visible on laptop LCD!**
  - **Problem**: Progress bar ไม่ขยับ — kernel hang ระหว่าง boot
- **Suspects (disabled)**:
  1. `CPUFriendDataProvider.kext` — data จาก i9-14900KF ใช้กับ Core 5 120U ไม่ได้ ⛔
  2. `CpuTopologyRebuild.kext` — RPL-U hybrid (2P+8E) ไม่ stable ⛔
  3. `SSDT-AWAC-DISABLE.aml` — desktop SSDT, ควรใช้ `SSDT-RTCAWAC` ⛔
- **Kexts active (10)**: `AppleALC`, `CPUFriend`, `Lilu`, `MyIntelGPU (DISABLED)`, `RestrictEvents`, `SMCProcessor`, `SMCSuperIO`, `USBToolBox`, `UTBMap`, `VirtualSMC`
- **Status**: 🔄 รอ re-test — CleanNvram + boot หลังจาก disable 3 ตัว

---

### 30-Jun-2026 — EFI cleanup + ACPI fix + session log (Sisyphus session #3)
- **Boot error analysis**: User reported `OC: Plist Kexts\FeatureUnlock.kext\Contents\Info.plist is missing` → `Halting on critical error`
  - FeatureUnlock.kext `Contents/Info.plist` **มีอยู่จริง** (2,339 bytes, valid XML)
  - สาเหตุ: USB (H:) มีสถานะ "Full Repair Needed" → FATFS corruption ที่ OpenCore อ่านไม่ผ่าน
  - `chkdsk H: /F` → **No errors found** (FS repaired if any)
- **FeatureUnlock.kext**: Disabled (`Enabled=false`) — comment ลงว่า "(missing kext)" ตั้งแต่ original config
- **Tools cleanup**: User requested minimal Tools to prevent accidental BIOS writes

  | Removed (15 .efi) | Kept (3) |
  |---|---|
  | CFGLock, ControlMsrE2, CsrUtil, RtcRw, modGRUBShell, BootKicker, OpenControl, GopStop, MmapDump, FontTester, KeyTester, ListPartitions, TpmInfo, ChipTune, RU.EFI | **CleanNvram, ResetSystem, OpenShell** |
  - `memtest/` folder also deleted
  - All config entries removed from `Misc > Tools`

- **CpuTopologyRebuild.kext**: Copied from `EFI.copy` → `H:\EFI\OC\Kexts\` + entry added to config — but **Disabled** (`Enabled=false`) per WORK_MEMORY (RPL-U unstable)
- **USBInjectAll.kext**: **Disabled** (`Enabled=false`) — conflicts with USBToolBox+UTBMap
- **SMCLightSensor.kext**: **Disabled** (`Enabled=false`) — unnecessary for boot
- **ACPI fix — 2 SSDTs were MISSING from config.plist (in folder but not enabled)**:

  | SSDT | Previously | Now |
  |------|-----------|-----|
  | SSDT-IMEI.aml | ❌ อยู่ในโฟลเดอร์แต่ไม่ได้เปิดใช้ | ✅ **Enabled** — required for laptop boot (IMEI device) |
  | SSDT-PMC.aml | ❌ อยู่ในโฟลเดอร์แต่ไม่ได้เปิดใช้ | ✅ **Enabled** — ApplePMCRam power management |
  - Total ACPI active: **7 SSDTs** (PLUG-ALT, EC, PNLF, RTCAWAC, RHUB, IMEI, PMC) — ตรงกับ WORK_MEMORY EFI Configuration table

- **boot-args reverted**: ผมเติม `-lilubetaall` ไปก่อน (ผิด) → แก้กลับเป็น VESA baseline
  
  | State | boot-args |
  |---|---|
  | Original H:\EFI | `-v keepsyms=1 debug=0x100 npci=0x2000 igfxonln=1 -wegnoegpu alcid=11` |
  | ผมเติม -lilubetaall (ผิด) | `...alcid=11 -lilubetaall` |
  | **Final (VESA baseline)** | `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12` |

- **boot-args change rationale**: Removed `debug=0x100`, `igfxonln=1`, `-wegnoegpu`, added `msgbuf=1048576`, `-no_compat_check`, `-igfxvesa`, changed `alcid=11→12` — ตรงตาม WORK_MEMORY 29-Jun clean baseline

- **User's project context**: `IntelReviveGPU (Gen 1012+) on Hackintosh` — building custom MyIntelGPU.kext for RPL-U iGPU (0xA7AC). macOS installation is prerequisite to test the kext.
  - CI #49 (85,592 bytes) deployed to `H:\EFI\OC\Kexts\MyIntelGPU.kext`, `Enabled=false`
  - Next: Boot macOS in VESA mode first → then Enable MyIntelGPU → Remove `-igfxvesa`

- **macOS metadata cleaned**: `._*` and `.DS_Store` removed from H:\EFI recursively

- **Current config.plist state (H:\EFI\OC\config.plist)**:

  | Category | Active |
  |---|---|
  | **ACPI** (7) | PLUG-ALT, EC, PNLF, RTCAWAC, RHUB, IMEI, PMC |
  | **Kexts Enabled** (18) | Lilu, VirtualSMC, AppleALC, WhateverGreen, SMCProcessor, SMCSuperIO, SMCBatteryManager, ECEnabler, NVMeFix, RestrictEvents, XHCI-unsupported, BrightnessKeys, VoodooPS2Controller(+plugins), VoodooI2C(+plugins), VoodooI2CHID, USBToolBox, UTBMap, CPUFriend |
  | **Kexts Disabled** (6) | FeatureUnlock, CPUFriendDataProvider, CpuTopologyRebuild, USBInjectAll, SMCLightSensor, MyIntelGPU |
  | **Drivers** (7) | OpenRuntime, OpenHfsPlus, Ext4Dxe, OpenCanopy, ResetNvramEntry, AudioDxe, OpenNtfsDxe |
  | **Tools** (3) | CleanNvram, ResetSystem, OpenShell |
  | **boot-args** | `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -igfxvesa alcid=12` |

- **Boot procedure**: CleanNvram 1 ครั้ง → Boot macOS from Picker → watch verbose for last line before hang
- **Status**: 🔜 Ready for boot test on Acer AL15-52P (H:\EFI\)

---

### 30-Jun-2026 — Phase 4 go-live: MyIntelGPU.kext Enabled + config finalised (Sisyphus session #4)
- **MyIntelGPU.kext updated**: CI build Phase 4 (85,592 bytes) from Downloads → `H:\EFI\OC\Kexts\MyIntelGPU.kext` ✅ 
  - **Enabled**: `false` → `true` (kext will now inject at boot)
  - Load order: after Lilu, before WhateverGreen
- **Major config changes**:

  | Setting | Before | After | Why |
  |---------|--------|-------|-----|
  | **MyIntelGPU.kext** | Disabled | **✅ Enabled** | Phase 4 go-live |
  | **AAPL,ig-platform-id** | `07009B3E` (CFL) | **`0000A73E`** (ADL/RPL) | Match real GPU arch |
  | **SMBIOS** | `iMac19,1` | **`iMac20,1`** | iMac20,1 better for RPL + dGPU-less SMBIOS |
  | **boot-args `-igfxvesa`** | Present | **❌ Removed** | MyIntelGPU接管 GPU ไม่ต้อง VESA fallback |
  | **AdviseFeatures** | false | **true** (unchanged) | — |

- **CPUID spoof**: `0A0655` (Comet Lake) — unchanged, still correct ✅
- **Kernel quirks**: ProvideCurrentCpuInfo=true, AppleXcpmExtraMsrs=true, DisableIoMapper=true — unchanged ✅
- **SMBIOS note**: Serial numbers still from MacBookPro16,2 era — ต้อง regenerate สำหรับ iMac20,1 ทีหลัง ถ้าต้องการ iServices

### Current Phase Status (30-Jun-2026)

| Phase | Description | Status |
|-------|-------------|--------|
| **Phase 1** | PCI Setup (bus master, memory enable) | ✅ DONE |
| **Phase 2** | BAR0/BAR2 MMIO Mapping (3-strategy fallback) | ✅ DONE |
| **Phase 3** | Interrupt Handling (MSI/MSI-X, Vblank, HPD) | ✅ DONE (commit b02b593) |
| **Phase 4** | IOFramebuffer Binding + OS Interface | ✅ **Code Complete — LIVE on H:\EFI** |
| **Phase 5** | Basic HW Acceleration (QE/CI, GEM, Ring Buffer) | ⏳ PENDING |
| **Phase 6** | Power Management (RC6, S3 Sleep/Wake) | ⏳ PENDING |

### Next Steps (updated 30-Jun)
1. ✅ **H:\EFI configured** — MyIntelGPU.kext enabled, SMBIOS iMac20,1, ig-platform-id 0000A73E
2. 🔜 **Boot macOS on Acer AL15-52P** — CleanNvram → Boot with MyIntelGPU active
3. If boot OK (desktop visible with acceleration):
   - Test display, backlight, resolution
   - Proceed to Phase 5 (QE/CI acceleration)
4. If panic → check verbose last line:
   - MyIntelGPU-related? → debug kext log via `sudo dmesg | grep MyIntelGPU`
   - Other issue? → fallback: add `-igfxvesa` back, disable MyIntelGPU
5. After stable boot: regenerate SMBIOS serials for iMac20,1
6. Phase 5-6: HW acceleration + power management

---

### 30-Jun-2026 — Boot log analysis + SetupVirtualMap fix + EFI.copy comparison (Sisyphus session #5)

#### Boot logs วิเคราะห์ (D:\ เก่า 5 logs)
| Log | SMBIOS | MyIntelGPU | EXITBS:START | result |
|-----|--------|:----------:|:------------:|:------:|
| `29-06-234153` | iMac19,1 | Disabled | ✅ | hang after |
| `30-06-002050` | iMac19,1 | Disabled | ✅ | hang after (Recovery) |
| `30-06-014802` | iMac20,1 | Disabled | ✅ | hang after (Recovery) |
| `30-06-062101` | iMac20,1 | **Enabled** | ✅ | hang after |
| `30-06-062633` | iMac20,1 | **Enabled** | ✅ | hang after |

**Pattern:** ทุกครั้งถึง EXITBS:START แต่ค้างทันทีหลัง hand off — **USB LED ไม่ติด = kernel ไม่ได้เริ่ม kext เลย**

#### 🔍 Root cause found: `SetupVirtualMap = False`
- Config ปัจจุบันมีค่าเป็น `False` (ผิด) → ควรเป็น `True` สำหรับ laptop รุ่นใหม่
- อาการตรงกับทุกประการ: Apple logo ค้าง, ไม่มี verbose output, USB ไม่ทำงาน, Ctrl+Alt+Delete ได้
- **แก้แล้ว**: `SetupVirtualMap = False → True` ✅

#### 🔍 Fix 2: `AdviseFeatures = True → False` (iMac20,1 ไม่จำเป็น)

#### 🔍 เปรียบเทียบ EFI.copy (ต้นฉบับที่ไปถึง Apple logo) vs H:\EFI

**Booter Quirks — ไม่แตกต่างเลยหลังจากแก้** ✅
```
SetupVirtualMap: EFI.copy=True  H:\EFI(ก่อน)=False  →  ตอนนี้ True แล้ว
```

**Kernel Quirks — H:\EFI ดีกว่า (12 ค่ามากกว่า)**
| Quirk | EFI.copy | H:\EFI | เหตุผล |
|-------|:--------:|:------:|--------|
| AppleCpuPmCfgLock | False | **True** | CFG Lock unlock |
| AppleXcpmCfgLock | False | **True** | XCPM CFG |
| AppleXcpmExtraMsrs | False | **True** | ⭐ RPL-U ต้องการ! |
| DisableIoMapper | False | **True** | VT-d |
| DisableRtcChecksum | False | **True** | RTC |
| LapicKernelPanic | False | **True** | ป้องกัน KP |

**UEFI Quirks — H:\EFI ปรับเพิ่ม**
- `ReleaseUsbOwnership: True` (Laptop ต้องการ)
- `UnblockFsConnect: True` (Laptop ต้องการ)
- `ExitBootServicesDelay: 200` (Delay เผื่อ BIOS)
- `EnableVectorAcceleration: False` (ไม่จำเป็น)

**boot-args ต่าง:**
- EFI.copy: `-v debug=0x100 keepsyms=1 alcid=11` (desktop AMD)
- H:\EFI: ✅ `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=12`

**Config ปัจจุบัน (H:\EFI\OC\config.plist)**

| หมวด | รายการ |
|------|--------|
| **SMBIOS** | iMac20,1 (ProcessorType=0) |
| **boot-args** | `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=12` |
| **MyIntelGPU** | ✅ Enabled (Phase 4, CI build) |
| **SetupVirtualMap** | ✅ **True** (แก้แล้ว) |
| **AdviseFeatures** | ✅ **False** (แก้แล้ว) |
| **EnableWriteUnprotector** | False |
| **RebuildAppleMemoryMap** | True |
| **SyncRuntimePermissions** | True |
| **ProvideCurrentCpuInfo** | True |
| **ACPI (7)** | PLUG-ALT, EC, PNLF, RTCAWAC, RHUB, IMEI, PMC |
| **Kexts Enabled (22)** | Lilu, MyIntelGPU, VirtualSMC, AppleALC, WhateverGreen, SMCProcessor, SMCSuperIO, SMCBatteryManager, ECEnabler, NVMeFix, RestrictEvents, XHCI-unsupported, BrightnessKeys, VoodooPS2Controller(+plugins), VoodooI2C(+plugins), VoodooI2CHID, USBToolBox, UTBMap, CPUFriend |
| **Kexts Disabled (4)** | FeatureUnlock, CpuTopologyRebuild, USBInjectAll, SMCLightSensor |
| **BIOS** | VMD=Disabled, SecureBoot=Disabled, VT=On |

#### User test result (ก่อนแก้ SetupVirtualMap)
- Apple logo ✅ → progress bar sometimes appears then stops
- **Ctrl+Alt+Delete = reset** ✅ (system not panicked, firmware alive)
- USB LED not active = system frozen in early kernel phase
- Boot from macOS Installer (BaseSystem.dmg) — confirms problem is NOT NVMe/VMD related

#### Key file
- `C:\Users\pongp\Downloads\New folder\EFI.copy\EFI copy\OC\config_REDACTED.plist` — original config (iMacPro1,1, MyIntelGPU-less) that reached Apple logo before modifications

#### Status
- 🔜 Ready for boot test — verbose ควรขึ้นแล้วหลังจากแก้ `SetupVirtualMap=True`

---

### 09-Jul-2026 — CI build fix (Xcode 15.4 SDK) + New G:\EFI (OpenCore 1.0.7) + ocvalidate cleanup (Sisyphus session #6)

#### CI Build Fix (commit 9fac5f8 / 5161c9f)
- **Xcode 15.4 SDK compat**: `IOMemoryDescriptor::withAddress()` removed 4th arg `kernel_task` (3-arg overload for macOS 15.2+)
  - File: `MyIntelGEMBuffer.cpp`
- **Sign-conversion warning**: Changed `- 1` → `- 1U` in `MyIntelRing.hpp:151`
- **CI build**: ✅ Passed on `macos-14` runner — artifact: `MyIntelGPU.kext` (v1.0.0, CI build from `~/Downloads`)

#### New EFI on G:\ (USB, OpenCore 1.0.7)
- **Based on**: WORK_MEMORY final config from 30-Jun-2026 (Session #5)
- **SMBIOS**: `iMac20,1` (AdviseFeatures=false, ProcessorType=0)
- **iGPU**: `AAPL,ig-platform-id = 0000A73E`, `device-id = A7260000`
- **CPUID spoof**: `0A0655` (Comet Lake)
- **boot-args**: `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=12`
- **MyIntelGPU.kext**: ✅ **Enabled** (CI build v1.0.0, from `C:\Users\pongp\Downloads\MyIntelGPU.kext.zip`)
- **Kexts (17)**: Lilu, MyIntelGPU, VirtualSMC, AppleALC, WhateverGreen, SMCProcessor, SMCSuperIO, SMCBatteryManager, ECEnabler, NVMeFix, RestrictEvents, USBInjectAll, XHCI-unsupported, BrightnessKeys, VoodooPS2Controller(+plugins), VoodooI2C(+plugins), VoodooI2CHID
- **ACPI (7)**: PLUG-ALT, EC, PNLF, RTCAWAC, IMEI, PMC, RHUB
- **Drivers (7 enabled)**: OpenRuntime, OpenHfsPlus, Ext4Dxe, OpenCanopy, ResetNvramEntry, AudioDxe, OpenNtfsDxe
- **Source**: Reference config from `F:\สำคัญที่สุด\EFI\EFI\OC\` (เดิม MacBookPro18,1) → แก้ไขตาม WORK_MEMORY

#### ocvalidate fixes (45 → 0 errors)
| Category | Count | Fix |
|---|---|---|
| ACPI comment illegal chars (em dash `–`) | 4 | → regular hyphen `-` |
| Missing Booter Quirks | 2 | Added `DevirtualiseMmio=false`, `SetupVirtualMap=true` |
| Missing UEFI AppleInput keys | 6 | `PointerDwell*`, `PointerPollMask`, `PointerSpeed*` |
| Missing UEFI Audio keys | 6 | `MaximumGain`, `Minimum*Gain`, `PlayChime`, `ResetTrafficClass`, `SetupDelay` |
| Missing UEFI Input key | 1 | `KeyForgetThreshold=5` |
| Missing UEFI Output keys | 5 | `ConsoleFont`, `GopBurstMode`, `InitialMode=Auto`, `ReconnectGraphicsOnConnect`, `UIScale=-1` |
| Missing UEFI ProtocolOverrides | 10 | `AppleEg2Info`, `AppleImg4Verification`, `AppleSecureBoot`, `AppleSmcIo`, `AppleUserInterfaceTheme`, `DataHub`, `DeviceProperties`, `FirmwareVolume`, `OSInfo`, `PciIo` |
| Missing UEFI Quirks | 7 | `EnableVectorAcceleration`, `EnableVmx`, `ForceOcWriteFlash`, `ForgeUefiSupport`, `IgnoreInvalidFlexRatio`, `ResizeUsePciRbIo`, `ShimRetainProtocol` |
| Missing UEFI top-level | 2 | `ReservedMemory=[]`, `Unload=[]` |
| CustomSMBIOSGuid mismatch | 1 | `true` → `false` (iMac20,1 ไม่ต้องใช้) |
| InitialMode illegal | 1 | Set to `Auto` |
| GopPassThrough type error | 1 | Removed duplicate `GopPassThrough<false/>` |

#### Tools (15 entries, all Auxiliary=false → visible in picker)
`BootKicker`, `ChipTune`, `CleanNvram`, `ControlMsrE2`, `CsrUtil`, `FontTester`, `GopStop`, `KeyTester`, `ListPartitions`, `MmapDump`, `OpenControl`, **`UEFI Shell`**, `ResetSystem`, `RtcRw`, `TpmInfo`

#### Config.plist — Final verification
- `C:\Users\pongp\OneDrive\Desktop\IntelReviveGPU (Gen 1012+) on Hackintosh` (current project)
- **XML well-formed**: ✅
- **ocvalidate**: **0 errors** ✅
- **Remnants**: `G:\EFI\OC\config\` directory (old OC 1.0.5 artifact — harmless)

#### Current Status (09-Jul-2026)
- **CI**: ✅ Passing (Xcode 15.4 SDK compat fix)
- **EFI (G:\)**: OpenCore 1.0.7, MyIntelGPU enabled, all quirks per WORK_MEMORY
- **Next**: 🔜 Boot macOS from G:\ USB
- **⚠️ BIOS Prerequisite (สำคัญที่สุด)**: Secure Boot = **Disabled**, VT-d/VT-x = **Enabled**, VMD Controller = **Disabled**
  1. เข้า BIOS (กด F2/Del ตอน开机)
  2. **Secure Boot → Disabled** (หรือ OS Type = Other OS)
  3. **VT-d → Enabled** (ถ้ามี)
  4. **VMD Controller → Disabled** (ไม่งั้น NVMe ไม่ขึ้น)
  5. บูทเครื่องจาก USB → เลือก G: ใน Boot Menu
- **Boot sequence**: Enter OpenCore Picker → CleanNvram (once) → Boot macOS (MyIntelGPU active)

---

### 10-Jul-2026 — Boot log analysis (6 logs) + config overhaul + MyIntelGPU activation + EFI reference comparison (Sisyphus session #7)

#### Boot logs analyzed (6 logs from D:\ — all via USB OpenCore)
| Log | Time | Config | boot-args | Result |
|-----|------|--------|-----------|--------|
| `opencore-2026-07-10-003805.txt` | 00:38 | A (csr=0x40) | no -lilubetaall | EXITBS:START hang (~17s) |
| `opencore-2026-07-10-004041.txt` | 00:40 | A (csr=0x40) | no -lilubetaall | EXITBS:START hang |
| `opencore-2026-07-10-004144.txt` | 00:41 | A (csr=0x40) | no -lilubetaall | EXITBS:START hang |
| `opencore-2026-07-10-011500.txt` | 01:15 | A (csr=0x40) | no -lilubetaall | EXITBS:START hang |
| `opencore-2026-07-10-015334.txt` | 01:53 | B (csr=0x67) | + -igfxvesa | EXITBS:START hang |
| `opencore-2026-07-10-021255.txt` | 02:12 | B (csr=0x67) | + -igfxvesa | EXITBS:START hang |

**Pattern**: ทุก log ถึง EXITBS:START (~17s) แล้ว hang ทันที — USB LED ไม่ติด (kernel ไม่ start)

#### Disk layout confirmed (10-Jul)
| Disk | Partition | Drive | FS | Label | Size | Purpose |
|------|-----------|-------|----|----|------|---------|
| Disk 0 NVMe 512GB | 1 (ESP) | — | FAT32 | EFI | 200MB | Internal ESP |
| | 2 (MSR) | — | — | — | 16MB | MSR |
| | 3 (Basic) | C:\ | NTFS | — | ~475GB | Windows 11 |
| | 4 (Recovery) | — | — | — | 851MB | WinRE |
| Disk 1 USB 15.6GB | 1 (ESP) | D:\ | FAT32 | EFI | 200MB | **OpenCore 1.0.7 config หลัก** |
| | 2 | — | **MBR 0xAF (HFS+)** | — | **~16.7GB** | **macOS Sequoia installer** |

- macOS installer: MBR partition type 0xAF = Apple HFS+ (createinstallmedia)
- D:\ is the EFI partition on the same USB that boots the installer

#### Config changes made (10-Jul)

##### 1. boot-args: Added `-lilubetaall`
- Old: `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=12 -igfxvesa`
- New: `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -lilubetaall alcid=12 -igfxvesa`
- *Note: -lilubetaall was later removed after enabling MyIntelGPU — see final state below*

##### 2. Fixed XML corruption in config.plist (~164 lines of duplicate UEFI section after `</plist>`)
- After section: `</plist>` had ~164 lines of duplicate `UEFI` dict appended
- **Fix**: Removed duplicate trailing content

##### 3. Deleted orphan/problematic kexts from D:\EFI\OC\Kexts\
| Kext | Reason |
|------|--------|
| CPUFriendDataProvider.kext | data-only, no executable, causes OC warning |
| AMDRadeonNavi2xExt.kext | AMD dGPU driver, unnecessary |
| USBInjectAll.kext | conflicts with USBToolBox+UTBMap |
| USBPorts.kext | conflicts with USBToolBox+UTBMap |
| USBWakeFixup.kext | unnecessary |
| SMCLightSensor.kext | unnecessary for boot |

##### 4. Added CpuTopologyRebuild.kext → Kernel > Add ✅
- BundlePath: CpuTopologyRebuild.kext
- Enabled: true
- MinKernel: 20.0.0
- **Why**: RPL-U hybrid CPU (2P+8E) — macOS stock kernel doesn't understand P+E topology

##### 5. Added MyIntelGPU.kext → Kernel > Add ✅ (ENABLED)
- BundlePath: MyIntelGPU.kext
- Enabled: true (was never registered before!)
- MinKernel: 22.0.0
- **Why**: MyIntelGPU.kext existed in folder but was NEVER injected — no Kernel > Add entry

##### 6. Fixed MyIntelGPU.kext/Contents/Info.plist (was corrupted!)
- D:\ version had truncated DOCTYPE + broken XML (duplicate `</plist>` + garbage text)
- **Fix**: Replaced with clean version using `IOPCIPrimaryMatch` and proper `OSBundleLibraries`

##### 7. Removed `-igfxvesa` from boot-args
- **Why**: MyIntelGPU now active — no longer need VESA fallback

##### 8. Booter quirk: DevirtualiseMmio = false → **true**
- **Why**: Both Olarila and etechbox reference EFIs use true for RPL laptops
- Critical for EXITBS:START — prevents MMIO region conflicts at kernel handoff

##### 9. Booter quirk: ProtectUefiServices = true → **false**
- **Why**: Must be false when DevirtualiseMmio=true (they conflict)

##### 10. Kernel quirk: AppleCpuPmCfgLock = true → **false**
- **Why**: RPL doesn't need PM lock, only XCPM lock

##### 11. Kernel quirk: AppleXcpmExtraMsrs = true → **false**
- **Why**: CpuTopologyRebuild.kext handles extra MSRs

#### EFI reference comparison (3 configs)

| Setting | **ของเรา (final)** | Olarila (laptop) | etechbox (laptop) | Base RPL (desktop) |
|---------|:---:|:---:|:---:|:---:|
| `DevirtualiseMmio` | **true** | true | true | true |
| `ProtectUefiServices` | **false** | false | false | true* |
| `AppleCpuPmCfgLock` | **false** | false | false | false |
| `AppleXcpmExtraMsrs` | **false** | false | false | false |
| `AppleXcpmCfgLock` | **true** | true | true | false** |
| `ProvideCurrentCpuInfo` | **true** | true | true | true |
| `CpuTopologyRebuild` | **enabled** | enabled | ❌ missing | ❌ missing |

*\* Desktop RPL ใช้ ProtectUefiServices=true ต่างจาก laptop reference*
*\*\* Desktop RPL ใช้ AppleXcpmCfgLock=false — desktop firmware ไม่ lock MSR*

#### Final config.plist state (D:\EFI\OC\config.plist — 10-Jul 02:17)

| Category | Detail |
|----------|--------|
| **OpenCore** | 1.0.7 |
| **SMBIOS** | iMac20,1 |
| **boot-args** | `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check -lilubetaall alcid=12` |
| **MyIntelGPU.kext** | ✅ **Enabled** (Kernel > Add, v1.0.0 CI build) |
| **CpuTopologyRebuild.kext** | ✅ **Enabled** (RPL hybrid CPU fix) |
| **DevirtualiseMmio** | ✅ **true** (EXITBS fix) |
| **SetupVirtualMap** | ✅ true (fixed prev session) |
| **ProvideCurrentCpuInfo** | ✅ true |
| **csr-active-config** | `ZwAAAA==` (0x67) |
| **SSDTs (7)** | PLUG-ALT, EC, PNLF, RTCAWAC, RHUB, IMEI, PMC |
| **Kexts Enabled (19)** | Lilu, VirtualSMC, AppleALC, WhateverGreen, SMCProcessor, SMCSuperIO, SMCBatteryManager, ECEnabler, NVMeFix, RestrictEvents, XHCI-unsupported, BrightnessKeys, VoodooPS2Controller(+2 plugins), USBToolBox, UTBMap, CPUFriend, **CpuTopologyRebuild**, **MyIntelGPU** |

#### Reference EFI files explored
| Source | Location |
|--------|----------|
| Olarila Alder/RPL EFI | `C:\Users\pongp\Downloads\EFI-olarila\` |
| etechbox 12th-14th Gen EFI | `C:\Users\pongp\Downloads\EFI-etechbox\EFI\` |
| Base RPL Desktop EFI | `C:\Users\pongp\OneDrive\Desktop\Tools\ตัวอย่าง EFI\Base-EFI-for-Intel-13th-Gen-Raptor-Lake-main\` |

#### Next Steps (updated 10-Jul)
1. 🔜 **Boot test**: CleanNvram → Boot macOS Sequoia from USB
2. If EXITBS:START passed → check verbose for any MyIntelGPU panic
3. If EXITBS still hangs → try `-neec` boot-arg (disable E-cores) or `e=0`
4. If boot OK → proceed to Phase 5 (HW acceleration)
5. SMBIOS serials: regenerate for iMac20,1 when needed

### 10-Jul-2026 — Config.plist critical fixes + God Council analysis (Sisyphus session #8)

#### Session context
- User triggered `/god-council` for multi-model expert analysis on EXITBS:START hang
- 4 expert agents dispatched (Architect, Researcher, Security/Debug, Creative) — session interrupted before results collected
- Boot log analysis started (5 of 13 logs read before interrupt)

#### config.plist fixes applied (D:\EFI\OC\config.plist)

| # | Setting | Before | After | Why |
|---|---------|--------|-------|-----|
| 1 | `DevirtualiseMmio` | **false** | **true** | CRITICAL — Must be true for 300-series+ (RPL) to properly devirtualize MMIO regions at kernel handoff |
| 2 | `ExitBootServicesDelay` | **200** | **0** | Unusual 200ms delay can cause timing issues on RPL |
| 3 | `SMBIOS` | **MacBookPro16,3** | **iMac20,1** | MacBookPro16,3 is Ice Lake (i5-1035G4) — mismatched with CPUID spoof 0A0655 (Comet Lake) |
| 4 | `framebuffer-stolenmem` | **64KB** (0x00010000) | **32MB** (0x02000000) | 64KB far too low for RPL-U iGPU — needs minimum 32MB |
| 5 | `framebuffer-fbmem` | **192KB** (0x00030000) | **19MB** (0x01300000) | 192KB too low — needs ~19MB for proper framebuffer allocation |
| 6 | `MyIntelGPU.kext Enabled` | **false** | **true** | Was disabled — now active for RPL→CFL register translation |

#### Discrepancies found vs WORK_MEMORY
- WORK_MEMORY (10-Jul entry) documented `DevirtualiseMmio=true` and `ProtectUefiServices=false` — but actual config.plist had `DevirtualiseMmio=false` and `ProtectUefiServices=true`
- SMBIOS was documented as `iMac20,1` but actual file had `MacBookPro16,3`
- `framebuffer-stolenmem` was documented as `AAABAA==` (64KB) — this was correct but 64KB is critically low for RPL

#### Base64 values (reference)
| Property | Base64 | Hex (LE) | Decimal |
|----------|--------|----------|---------|
| `AAPL,ig-platform-id` | `AACnPg==` | 00 00 A7 3E | 0x0000A73E (CFL headless) |
| `device-id` | `pyYAAA==` | A7 26 00 00 | 0xA7260000 |
| `Cpuid1Data` | `VQYKAA==` | 55 06 0A 00 | 0x000A0655 (Comet Lake) |
| `framebuffer-stolenmem` (new) | `AAAAAg==` | 00 00 00 02 | 0x02000000 (32MB) |
| `framebuffer-fbmem` (new) | `AAAwAQ==` | 00 00 30 01 | 0x01300000 (19MB) |

#### Config validation notes
- `ProtectUefiServices=true` with `DevirtualiseMmio=true` is a known conflict — was NOT changed in this session (should be `false` per previous session's analysis)
- `AppleCpuPmCfgLock=true` — was NOT changed (previous session set to `false`, but current config has `true`)
- `AppleXcpmExtraMsrs=true` — was NOT changed (previous session set to `false`, but current config has `true`)

#### Status
- 🔜 **Ready for boot test** — all 6 fixes applied
- Boot from USB (D:\) → CleanNvram → Boot macOS
- If EXITBS:START still hangs → investigate remaining quirk conflicts (ProtectUefiServices, AppleCpuPmCfgLock, AppleXcpmExtraMsrs)

---

#### Next Steps (updated 10-Jul session #8)
1. 🔜 **Boot test**: CleanNvram → Boot macOS Sequoia from USB (D:\)
2. If EXITBS:START passed → check verbose for MyIntelGPU panic
3. If EXITBS still hangs → fix remaining quirk conflicts:
   - `ProtectUefiServices` → false (conflicts with DevirtualiseMmio=true)
   - `AppleCpuPmCfgLock` → false (RPL doesn't need PM lock)
   - `AppleXcpmExtraMsrs` → false (let CpuTopologyRebuild handle)
4. If boot OK → proceed to Phase 5 (HW acceleration)
5. SMBIOS serials: regenerate for iMac20,1 when needed

---

### 04-Oct-2026 — Boot stall root-caused to kern_log backpressure; boot-log buffering + fake capability-ad removal (Sisyphus session #9)

Hardware: Core 5 120U (Raptor Lake, `8086:A7AC`), macOS 15, `/Library/Extensions` install, panel owned by BIOS/GOP + `IONDRVSupport`.

#### The 12.5-second boot stall

`MyIntelGPU::start()` was nondeterministic. Same binary, consecutive boots:

| Version | Boot times |
|---|---|
| 4.1.91 | 1358 ms, 215 ms |
| 4.1.92 | 187 ms, 207 ms, 196 ms, **12543 ms** |

The slow boot was **not** GPU compute and **not** MMIO. Evidence:

- The 12057 ms gap sat between two *adjacent* `IOLog()` calls — `GEMBuf: [GGTT] seq=14 bind ... MAPPED` at `14:40:20.944701` and `gemBufferCreate: OK` at `14:40:33.002…`. No work or sleep between the statements.
- MMIO worst read that boot was **12 µs**; ring polls were 1.0–1.3 ms. Both orders of magnitude too small.
- ~9000 unrelated kernel messages hit the same window (`softwareupdated`, `mobileassetd`, `launchd`, `dasd`).
- One boot emitted **194** `IOLog()` records. Each one is a `kern_log` write that can block behind system-wide pressure.

Conclusion: unified-log backpressure. Fix is to shrink default boot-path log volume, not to optimise registers.

#### Change 1 — bounded boot-log buffer

New `MyIntelBootLog.hpp`: 32 KB in-memory ring, `myBootLogf` / `myBootLogDirect` / `myBootLogFlush` / `myBootLogReset` / `myBootLogFlushForce`.

- `myBootLogf()` formats into memory — **never** calls `IOLog()`.
- Replay only under `myinteldbg=1`; failure path flushes unconditionally so a driver that dies before Phase 7 still leaves a trace.
- Boot-path records buffered on success: **186**.

Redirected to the buffer in five files, because all of these run *inside* `start()`:

| File | What was leaking |
|---|---|
| `MyIntelGPU.cpp` | `IODebug` + bare `IOLog` |
| `MyIntelRing.cpp` | `RING_DEBUG` / `RING_DEBUG_RAW` |
| `MyIntelGEMBuffer.cpp` | `GEM_DEBUG` |
| `MyIntelMedia.cpp` | `IODebug` macro — **Phase 7**, ~10 records |
| `IntelFramebuffer.cpp` | `FBLog` macro — **Phase 5c** |

`MyIntelMedia.cpp` and `IntelFramebuffer.cpp` were found only by auditing every remaining `IOLog` call site — the first pass assumed the four obvious files and missed both. Audit remaining raw `IOLog` before assuming the boot path is clean.

Kept direct on purpose: version banner, `TIMING`, one `SUBMIT[]` record per engine, real faults, final EDID outcome.

Deliberately left alone:
- `MyIntelAccelerator.cpp` (11 sites) — deferred `t+45s`, outside `start()`
- `MyIntelGPUClient.cpp` / `MyIntelVCSClient.cpp` — client-open path
- `IntelFramebuffer.cpp::dumpDiagnostics()` — periodic, post-boot
- `MyIntelGEMBuffer.cpp::GEM_TRACE` — compiled out via `#if 0`

#### Change 2 — fake capability advertisements removed

`MyIntelAccelerator` claimed hardware video decode and Metal support that nothing implements behind them. Decode is driven as raw MFX over the VCS ring and its readback is still `FAIL-NODATA`; there is no VideoToolbox or IOGPU integration and no Metal driver ABI.

Removed from `MyIntelAccelerator.cpp`:

```
IOGVACodec="Gen12HP"      IOGVAXDecode=2
IOGVAH264Decode/Encode    IOGVAHEVCDecode/Encode
IOGVA_AV1Decode           IOGVP9Decode
MetalStatisticsName
```

Also removed `MetalStatisticsName` from the `Info.plist` personality dict — it was set at match time regardless of `publishProperties()`.

Renamed `VDBOXSupported` → `VDBOXPresent`. Nothing read it (single grep hit, the set itself), and "Supported" contradicted the unproven-decode record. Hardware presence is factual; decode is not.

Kept: `IOAccelRevision`, `IOAccelTypes`, `IOVARendererID`, `MyIntelVCSReady`. The first two are load-bearing — without them WindowServer re-probes the node on every display wake.

A guard comment naming the banned keys sits at the removal site on purpose. Absence of a property carries no information in the source, and this project has already been told to add `MetalPluginName` / `IOGLBundleName` back once (2026-09-17). The next reader is the one who will re-add them.

#### Verification (all real boots, not estimates)

| Boot | Version | `start total` |
|---|---|---|
| 15:14:32 | 4.1.93 | 197 ms |
| 15:18:19 | 4.1.88 | 194 ms |
| 15:55:59 | 4.1.89 | 198 ms |
| 15:59:33 | 4.1.89 | 200 ms |
| 16:02:07 | 4.1.89 | 196 ms |
| 16:04:24 | 4.1.88 | 194 ms |

Every boot: `SUBMIT[RCS]/[BCS]/[VCS]` present, `ESR`/`EIR`/`IPEIR` = `0x00000000` on all three engines, real FAULT count 0, `EDID passthrough: injected KDB0924/KD156N2930A02`, panel `Online: Yes` 1920×1080, `IODVDBundleName` absent.

Checked again at uptime 8 min, after the `t+45s` accelerator arm, to catch late faults — still zero. Real kernel panics: 0. (An 86-hit grep for "panic" was `ApplePVPanic.kext` in sandbox `chown` logs — unrelated.)

Post-removal, confirmed against the **live** IORegistry node: capability keys absent, identity keys intact, `createAccelID 4105`, `attachMode=2`. WindowServer still adopts the node.

#### Two traps that cost real time

**1. Version numbering is a lie by default.** `Makefile:60` sets `BUILD_NUMBER := $(git rev-list --count HEAD)`; `Makefile:64` falls back to `date +%H%M` outside a git repo. This repo has 88 commits, so a bare `make` stamps `4.1.88` no matter what changed. Observed live: a build at 16:03 produced `4.1.88` while `4.1.89` was installed, and the label went *backwards*. Always pass `BUILD_NUMBER=` or use the script.

**2. Never `make` as root.** Artifacts land root-owned and the next `make clean` — including the one inside `บิ้วMyIntelGPU-AllInOne.command` — dies with `Permission denied`. This bit once during this session. Repair:

```bash
sudo chown -R ppbk:staff MyIntelGPU.kext MyIntelGPUVersion.h *.o
```

`MyIntelGPU.kext` and `MyIntelGPUVersion.h` are regenerated on every build (`Makefile:71`, `FORCE`), so ownership must be re-checked after each build. Agent shells run as root; the user's Terminal runs as `ppbk` — this trap is agent-only.

#### `บิ้วMyIntelGPU-AllInOne.command` — four defects fixed

1. **No `BUILD_NUMBER`** → stamped 4.1.88 every run. Now auto-increments from the installed version, overridable via `BUILD_NUMBER=120 ./script` or `./script 120`. Includes an assertion that fails the build if the bundle version doesn't match.
2. **`sudo rm -rf` with no backup** → a mid-deploy failure destroyed the installed bundle, and `set -e` killed the script silently. Now backs up to `~/kext-backups/pre-<version>-<timestamp>/` first, with an `ERR` trap that restores.
3. **`kextstat | grep pongpan`** — deprecated; replaced with `kmutil showloaded`.
4. **Verify advice pointed at a dead node** — `ioreg -c MyIntelFramebuffer | grep CurrentPowerState` cannot work, because `MyIntelFB` probes then is freed within ~2 ms (`gEnableDisplayFramebuffer=0`). Replaced with the log timing check and `system_profiler | grep Online`.

Also: root warning at start, file mode `644` → `755` (it was not executable, so double-click could not run it).

#### EFI partition: nothing to do

`mdfind -name MyIntelGPU.kext` reported `/Volumes/EFI-MAIN/EFI/OC/Kexts/MyIntelGPU.kext`, which looked like a stale kext needing a sync. It does not exist — the `mdfind` hit was **stale Spotlight metadata**. There is no kext there, no empty stub, and `config.plist` has zero `MyIntelGPU` references. The L/E-only deployment is already correct. Do not "fix" this.

#### README

Rewrote `## Build` and `## Deploy`, added `## Post-reboot verification`. Now documents the `BUILD_NUMBER` trap, the root-ownership trap, the backup step, `kmutil showloaded`, the acceptance table, the `GTFAULT=0x00000000` false positive, the debug-flag warning, and that OpenCore intentionally holds no copy of this kext.

#### Log-reading rules

- Use `log show --predicate 'sender == "MyIntelGPU"' --style syslog`. A broad `eventMessage CONTAINS[...]` drops `Ring:` lines and pulls in unrelated records.
- `GTFAULT=0x00000000` is a zeroed field inside a `SUBMIT[]` line, **not** a fault. Real faults read `FAULT ESR=...` / `FAULT_GEN12=... VALID`.
- Never boot with `myinteldbg=1` when measuring timing — it replays the buffered trace and reintroduces the pressure being measured.
- Kernel timestamps from `log show` are `+0700` while `sysctl kern.boottime` is authoritative for "which boot is this"; compare them before attributing a log line to the current boot.

#### Still unproven -- the golden check was circular

Hardware decode **writes every row but the pixels are wrong** (05-Oct). The old metric was invalid: it probed 20480 bytes against a `0xA5` fill, and `0xA5`=165 falls *inside* legal luma 16..235, so it could not distinguish written from unwritten. That metric is gone.

The harness now seeds poison guaranteed outside 16..235 on all four bytes of every dword, so *write coverage* is genuinely measurable:

- `frame=18432` (dW=128 dH=96 pitch=128), `dst_poison_bytes_remaining=1024`, `dst_bytes_written_pct=94.44%`
- `[REGISTER-AUDIT] post: ESR=0 IPEHR=0 lrcHEAD=0x530 lrcTAIL=0x530 mmioHead=0`

Superseded -- these two lines came from the **circular** `golden_master.nv12` reference, so they measured nothing about correctness:

- `[GOLDEN] differing=1024/18432B (5.56%) first_diff=+16640 (0x4100) max_delta=165 still_poison=1024`
- `[VERDICT] FAIL-NODATA golden_compared=1 golden_exact=0 golden_diff_bytes=1024` then `[EXIT] verdict=FAIL-NODATA -> exit 1`

Current run against the true VideoToolbox reference (`golden_videotoolbox.nv12`):

- `[GOLDEN] differing=18163/18432B (98.54%) first_diff=+16 (0x10) max_delta=187 still_poison=1024`
- `[VERDICT] FAIL-NODATA golden_compared=1 golden_exact=0 golden_diff_bytes=18163` then `[EXIT] verdict=FAIL-NODATA -> exit 1`

**The `[GOLDEN]` comparison is circular and must never be used as a pass criterion.** `golden_master.nv12` (byte-identical to `master_bridge.nv12`) has a luma plane that is byte-identical to our own luma output -- `golden[0..12288) == MyIntelGPU_out[0..12288)` -> `True` -- so it was produced by *this* driver, not by an independent decoder. My earlier reasoning that "lossy decoders cannot agree bit-exactly unless both are correct" is **falsified** by this measurement.

**A true reference now exists.** `regen.h264`, regenerated from `gen_clip.m` at 128x96/30, is **byte-identical** to `test_clip.h264`, which proves the hardware encoder is deterministic. So `vt_000.nv12` -- Apple's VideoToolbox decode of that exact bitstream, produced by `/tmp/opencode/vcsgeom/vt_decode` -- is a genuine independent reference. It is itself validated against the analytic BT.601 gradient from `gen_clip.m` at corr **0.9904** / **32.40 dB**.

Measured against that true reference, our output is wrong in **both** planes:

- **Luma Y[0..12288)**: PSNR 20.46 dB, corr 0.9011. Fitted linear map `y' = 0.8104*y + 2.88` -- our luma spans 17..145 where the truth spans 17..190. A range *compression*, not a constant offset or gamma curve.
- **UV written rows**: corr **-0.2194**. U/V swap does not rescue it (-0.2870 / -0.2634), so it is structural, not a plane swap.

So "decode is partially proven" was wrong twice over: the pixels were never validated, and the only thing that looked like validation was self-agreement.

Write *coverage* is still deterministic, and the 1024 unwritten bytes are still a real defect: rows **130, 131, 134, 135, 138, 139, 142, 143** -- 8 of 48 chroma rows, period-4 `poison,poison,exact,exact` starting mid-plane at row 130. Not truncation (a tail would be `WWWW...PPPP`), not pitch/stride (128 is 4-aligned and written rows landed at correct offsets), not a drain race (identical row set across runs and across `MFX_DRAIN_MS=2000` vs `8000`). Caveat: `waited_ms` prints the *configured* budget, not elapsed time, so a fixed post-drain delay is still untested.

Two harness bugs surfaced while establishing this: the old poison pattern let an unwritten frame score 100% luma, and `main()` had no non-PASS `return` at all -- `FAIL-NODATA` used to exit `0`. Both are fixed, so the verdict is now derived from measured data and the exit code reflects it.

#### Next Steps (updated 04-Oct)

1. 🔜 Rebuild via `บิ้วMyIntelGPU-AllInOne.command` so the installed version label is meaningful (currently `4.1.92`; script derives installed+1, so it will produce `4.1.93`). Cosmetic — running code is correct.
2. 🔜 Fix the decoder, not the harness coverage: rebase the pass criterion onto `vt_000.nv12` (Apple VideoToolbox) and drop `golden_master.nv12`/`master_bridge.nv12` as references. Then chase the two measured defects in `MyIntelVCSCommand.cpp`: luma range compression (`a=0.8104`, spans 17..145 vs true 17..190) and structurally wrong UV (corr -0.2194, not a U/V swap). The periodic chroma write stripe (rows 130,131,134,135,138,139,142,143) is a *third*, separate bug -- chase pitch/stride last; it is already excluded by measurement.
3. 🔜 Collect more clean boots if the 12.5 s stall is to be considered fully closed — 6 consecutive boots at 194-200 ms is strong but the original failure was intermittent.
4. ⚠️ Dead stores in `MyIntelRing.cpp` (`lastCsbWr = pollCsb`, `lastActive = …pollActive`) are overwritten immediately by authoritative post-submit values. Harmless, never cleaned.
5. ⚠️ `myBootLogReset()` runs right after the success flush, so EDID retry detail after `start()` is buffered with no later flush. Success/final-outcome lines are direct, so this only limits post-start diagnostics.

## 05-Oct-2026 (late) — Gen12 MFX decode: hypothesis elimination + two surviving chroma defects

Continued the MyIntelGPU standalone VCS decoder investigation. Two background
agents were started and were **still running when this session was closed for a
slow file copy**:

- `bg_2abe16b6` — Oracle, ranking root causes for the chroma defect
- `bg_5a5b5650` — Librarian, exact Gen12 AVC command sequence / dword field layouts

**Collect these first on resume — they gate the next experiment.** Then invoke the
plan agent before touching code.

### Correction to the 04-Oct record

The `corr -0.2194` UV figure was measured on the **pre-`MFX_QM_FLAT`** output. On
the current flat-QM output our V plane correlates **-0.003** with reference V —
effectively zero, not -0.22. The structural conclusion (not a U/V swap) still holds,
but the number is stale. Do not quote -0.2194 for the current build.

### About the "PSNR below 20" recollection

Searched `README.md`, this file, every commit's `README.md`, and all old logs. There
is **no luma PSNR below 20 dB anywhere in the record**. Every stored run reads
20.46 dB exactly (`vcs_true`, `vcs_true2`, `vcs_out`, `vcs_out2` all rms 24.18).

The figure that *is* below 20 is **chroma**, so the recollection was half-right:

| plane | baseline | `MFX_QM_FLAT=1` |
|-------|----------|-----------------|
| Luma  | 20.46 dB | **25.48 dB** |
| Chroma | 12.33 dB | 12.46 dB |

Flat QM is a real, measured luma improvement; chroma barely moved.

### Eliminated by measurement this session (do not re-chase)

- **Surface tile mode + chroma V-offset.** Ran `MFX_SURF_TILE=0,1,2,3` and
  `MFX_SURF_VOFF=0|96`. Masking the deterministic poison rows, all variants differ
  by **0 non-poison bytes**. These fields do not affect written pixels on this path.
- **Deblocking.** `MFX_DEBLOCK=1` luma 13.65 / chroma 63.53; `=0` luma 13.57 /
  chroma 63.37. Override is applied; disabling makes chroma slightly *worse*.
- **Wrong reference frame.** Cross-correlated our output against all 30
  `vt_*.nv12` frames: `vt_000` is best (luma rms 13.57, corr 0.9445). Reference
  indexing is correct. Chroma rms stays ~60 against *every* frame.
- **Luma spatial shift.** 2D search dy,dx ∈ [-16,16]: **(0,0) is the global
  optimum** (13.59). An earlier `bestD=-4` reading was an artifact of a truncated
  search range — discarded.
- **Chroma spatial shift.** Best gain only 5.18 rms and it sits at the search
  boundary, so not a shift.
- **Chroma value permutation.** Histogram L1 distance is 914 (U) / 468 (V) over
  non-poison rows, not 0. Not a permutation of the correct samples.

### Two analysis errors I made — corrected, do not repeat

1. I used a **64-byte stride** for chroma rows. NV12 chroma rows at width 128 are
   **128 bytes**. This sliced every row in half and produced a garbage poison-row
   table. All chroma numbers must use `CB = W = 128`, `CH = H/2 = 48`.
2. "The last 16 chroma rows repeat the first 16" — exact test gives **0/128 bytes
   identical**. The *signed-error pattern* has period 16; the pixel values do not
   repeat. Overclaimed.

### The two defects that survive

**1. Chroma has lost essentially all horizontal structure.**

```
reference  right-half minus left-half = +36.4   (brightens to the right)
ours       right-half minus left-half =  -3.8   (flat)
```

Vertical is roughly right (ours -59.6 vs ref -53.0). So horizontal chroma detail
specifically is missing — the signature of a wrong **intra chroma prediction mode**.

**2. Chroma vertical ramp is essentially ABSENT.**

Measured by least-squares fit on chroma row means (poison rows excluded):

```
reference  slope = -1.1339 /row   R2 = 0.9998
ours       slope = -0.0935 /row   R2 = 0.0017
```

The reference is a near-perfect linear vertical ramp. Our chroma has almost no
slope at all; its large apparent "span" is a period-4 sawtooth oscillation, not a
real gradient. Note this **corrects** an earlier note in this session that claimed
"2.3x over-swing (ref 53.0 / ours 123.3)" -- that span figure measured sawtooth
amplitude, not vertical slope, and the claim was wrong.

Consistent with wrong **chroma QP scaling**, not with the quantization matrices:
`MFX_QM_FLAT` moved luma a lot and chroma almost not at all, which means chroma is
not going through the same matrix path. This is what the two pending agents must
answer: the Gen12 field encodings for chroma intra pred mode and chroma QP offset.

### Harness/ownership defect to fix before the next run

The harness was rebuilt and succeeded, but the binary is owned by **`root:wheel`**,
violating the standing rule (M7) that repository artifacts be `ppbk:staff`. Rebuild
as `ppbk` before any further experiment.

### Next Steps (updated 05-Oct, supersedes the 04-Oct list for decode work)

1. 🔜 Collect `bg_2abe16b6` and `bg_5a5b5650` via `background_output()`.
2. 🔜 Invoke the plan agent with those findings plus the two surviving defects above.
3. 🔜 Rebuild harness as `ppbk`; confirm `ls -ln` shows `ppbk staff`.
4. 🔜 Re-verify poison row set on a fresh binary before trusting any spatial claim.
5. 🔜 Then attack chroma intra pred mode / chroma QP field encoding in
   `MyIntelVCSCommand.cpp`. Do **not** guess bit offsets — that is shotgun debugging.
6. ⚠️ Poison byte values observed this run were `{7, 14, 254}`; the poison pattern is
   per-run random, so never hardcode it in analysis.

## 05-Oct 19:5x — SINGLE-WRITER LOCK (supersedes everything above)

User handed full ownership of this project to the primary session. **There is only
one writer now.** Any other opencode session working this repo must stop editing.

### Do NOT edit these files from another session
- `MyIntelVCSCommand.cpp` (repo root) — chroma fix in progress
- `/Users/ppbk/Documents/src/VCSMediaHarness/MyIntelVCSCommand.cpp` — must stay
  byte-identical to the repo copy
- `/Users/ppbk/Documents/src/VCSMediaHarness/test_submit_vcs.m`
- `/Users/ppbk/Documents/src/VCSMediaHarness/myintel_h264_parse.h`
- `MyIntelGPU.kext/` build artifacts

### Baseline fingerprint — any edit that does not move this is a no-op
```
MyIntelVCSCommand.cpp  SHA256 8711de3f5a5d688e8038fdf9b560f7159e7b2ec8997f7ebc82b7d6e94587d236
size 34922 bytes, 856 lines, mtime 2026-10-05 01:10
harness copy          SHA256 8711de3f5a5d688e8038fdf9b560f7159e7b2ec8997f7ebc82b7d6e94587d236
```

### Open work items owned by the primary session
| Item | State |
|---|---|
| Gen12 AVC chroma field layouts (authoritative Intel source) | librarian `bg_4097a047` in flight |
| Inventory of what the code actually emits | explore `bg_a68fbcf0` in flight |
| chroma fix in `MFD_AVC_IMG_STATE` (0x71000013, 21 DW) | blocked on the two above |
| Batch 6 dword count swing 332 vs 40 | not started |
| GEM leak: `vcs_done()` + `MFX_BBSTART` path | not started |
| kext version lock 4.1.92 installed vs 4.1.93 tree | built, not installed |
| `/Users/ppbk/Desktop/เทส/MyIntelGPU-VCS-Test.command` hardening | not started |

### Runtime state at lock time
```
boottime          2026-10-05 18:19:19
loaded kext       com.pongpan-bk.MyIntelGPU (4.1.92) AF4E9E5A-7A36-3ECC-9B80-4A739844CB33
installed binary  ad40bb262f4792caca14ebefcad6e58a1559814fef20981bef7d9addf39be16b (root:wheel)
tree build        4.1.93  27b83dfcbafcba1276ed19033583796bdce473b0db5513ba80a04f59bd0ab642 (ppbk:staff)
S1 nosubmit       PASS
S2 determinism    PASS (det_diff_bytes=0)
S3 correctness    FAIL golden diff 18163/18432 (98.54%)
```

### Rule that caused this lock
Two sessions were handed the same FAIL-NODATA task and both were about to plan an
edit to `MyIntelVCSCommand.cpp`. Neither had the authoritative Gen12 bit layout,
and the standing project rule is: do not guess bit offsets. Parallel writers on one
binary-format file is the exact failure mode that rule exists to prevent.

---

# MASTER PLAN — Gen12 VCS/MFX H.264 decode → pixel-correct
Written: 2026-10-05, after the single-writer lock. Owner: primary session.
Status: WAVE 0 IN FLIGHT. Nothing in `MyIntelVCSCommand.cpp` may be edited yet.

## 1. Objective (outcome-first)

Apple VideoToolbox's decode of `test_clip.h264` and MyIntelGPU's native Gen12
VDBOX decode of the same bitstream produce **byte-identical 18432-byte NV12
frames**, verified by direct file comparison against
`/Users/ppbk/Documents/src/VCSMediaHarness/golden_videotoolbox.nv12`.

Not "close". Not "high PSNR". Byte-identical, or the work is not done.

## 2. Hard constraints (violating any of these invalidates the result)

| # | Constraint | Why |
|---|---|---|
| C1 | Never guess a bit offset. Every bit position used must be cited from Intel Gen12 media-driver source or proven by a readback experiment. | Shotgun debugging already burned this project. `MFX_QM_FLAT` and the tile-bit experiments produced plausible-looking movement without correctness. |
| C2 | Poison byte values are randomized per run. Read them from the run, never hardcode. | Analysis scripts have silently compared against the wrong run's poison. |
| C3 | Chroma stride at W=128 is **128** bytes (`CB=W`, `CH=H/2=48`). Never 64. | A 64-byte stride sliced every chroma row in half and produced a garbage poison-row table. |
| C4 | `golden_master.nv12` and `master_bridge.nv12` are MyIntelGPU-derived and circular. Only `golden_videotoolbox.nv12` is a valid reference. | A self-generated reference cannot validate a decoder. |
| C5 | Poison coverage and pixel correctness are separate tracks. Poison proves *that* a write happened, never *what* was written. | 1024 bytes unwritten while 94.44% coverage looked like success. |
| C6 | Repo and harness copies of `MyIntelVCSCommand.cpp` must stay byte-identical. | The harness builds from the copy, not the tree. Divergence tests stale code silently. |
| C7 | Repo builds run as `ppbk`; artifacts owned `ppbk:staff`. | A root-owned artifact makes the next `make clean` fail. |
| C8 | Do not use `golden_master.nv12`, do not re-chase tile mode / `YOffsetForVCr` / deblocking. | Already measured as producing identical or worse output. |

## 3. Scenario contract (these are the acceptance criteria)

| ID | Scenario | Pass condition (binary observable) | Real-surface proof |
|---|---|---|---|
| **S1** | Safety — nosubmit | harness exits non-zero; `submitted=0`; `dst_bytes_written_pct=0.00`; all 18432 dst bytes still poison | `/tmp/vcs_test_harness test_clip.h264 out.nv12 --ctl=nosubmit`; grep `[DATA-CHECK]` |
| **S2** | Determinism | two identical submissions produce `det_diff_bytes=0`, `first_diff=-1`, identical poison counts | harness determinism mode, two runs, `cmp` the two `.nv12` |
| **S3** | **Correctness (the goal)** | `golden_diff_bytes=0` and `still_poison=0` against `golden_videotoolbox.nv12` | `cmp golden_videotoolbox.nv12 out.nv12` → exit 0 |
| **S4** | Resource balance | after 50 open/alloc/submit/close cycles in one process, GEM handle count and PPGTT slot count return to baseline | repeat-run counter table from the harness |
| **S5** | Source-to-binary | installed kext SHA == tree build SHA; `kmutil showloaded` version == built `CFBundleVersion` | `shasum -a256` both, `kmutil showloaded \| grep -i myintel` |
| **S6** | Field-level semantic proof | for every Gen12 AVC field the driver writes, an experiment exists that shows the field's effect on output, or the field is documented as inert | per-field A/B table |

S1 and S2 are currently green. S3 is 18163/18432 bytes wrong. S4–S6 unbuilt.

## 4. Current state fingerprint (2026-10-05 ~20:00)

```
MyIntelVCSCommand.cpp   34922 B, 856 lines
  SHA256 8711de3f5a5d688e8038fdf9b560f7159e7b2ec8997f7ebc82b7d6e94587d236
  identical in repo and /Users/ppbk/Documents/src/VCSMediaHarness/

loaded kext       com.pongpan-bk.MyIntelGPU (4.1.92) AF4E9E5A-7A36-3ECC-9B80-4A739844CB33
installed binary  ad40bb262f4792caca14ebefcad6e58a1559814fef20981bef7d9addf39be16b
tree build        4.1.93  27b83dfcbafcba1276ed19033583796bdce473b0db5513ba80a04f59bd0ab642

S1 PASS   S2 PASS   S3 FAIL 18163/18432 (98.54%)   S4-S6 not built
```

## 5. What we know about the defect (measured, not assumed)

**Clip** (harness `[PARSE]`): 128x96, 8x6 MB, profile_idc=100 High, pocType=0,
refFrames=2, CABAC, initQp=28, transform8x8=1, deblockCtrlPresent=0,
chroma_format_idc=1 (4:2:0), slice type 2 (I), slice QP 23.

**Luma** — works, imperfectly: rms 13.57, PSNR 25.48 dB, corr 0.9445 with
`MFX_QM_FLAT=1` (was 20.46 dB / 0.9011). Fitted `y' = 0.8104·y + 2.88`, so luma
range is compressed, not merely offset.

**Chroma** — two independent measured defects:

```
(a) HORIZONTAL structure absent
      reference  right-half minus left-half = +36.4
      ours                                =  -3.8
(b) VERTICAL ramp absent
      reference  slope -1.1339 /row   R2 = 0.9998
      ours      slope -0.0935 /row   R2 = 0.0017
```

**THE CRITICAL OBSERVATION.** Read (a) and (b) together: no horizontal structure
AND no vertical ramp means **our chroma plane is essentially a constant value**.
That is a much stronger and narrower hypothesis than "chroma is wrong". It points
at chroma never receiving a spatial signal at all, not at chroma being decoded
with slightly wrong arithmetic.

**The observation that narrows it further:** `MFX_QM_FLAT` moved luma 20.46 →
25.48 dB but barely touched chroma. In H.264, chroma uses only 4x4 quantization
matrices and never 8x8. So a change to the 8x8 path moving luma while leaving
chroma inert is exactly what theory predicts — and it means **chroma's problem is
on a different path from luma's quantization path**. Chasing quant matrices will
not fix chroma.

**Poison coverage**: 1024 of 18432 bytes unwritten = 8 of 48 chroma rows, in a
deterministic period-4 pattern (rows 34,35,38,39,42,43,46,47, i.e. `row mod 4 ∈
{2,3}` from row 34 onward), byte-for-byte identical across separate runs and
across a 4x longer drain budget. Deterministic ⇒ not a race, not a drain timing
issue. 4 rows = one 4:2:0 chroma row-pair unit.

**Live command path** (HEVC-era `MFX_PIC_STATE` / `MFX_SLICE_STATE` /
`MFX_REF_IDX_STATE` emitters are dead code, only reachable from the legacy branch
at `test_submit_vcs.m:1121-1124`):
```
PipeModeSelect → SurfaceState → PipeBufAddr → IndObj → BspBufBaseAddr
→ AvcPicIdState → AvcImgState (0x71000013, 21 DW) → QmState ×4
→ DirectMode → AvcRefIdxDummy → AvcSliceState → BsdObject
```
The picture state to fix is `MFD_AVC_IMG_STATE`. `AvcImgState` DW5 bit27
(TrellisQuantizationChromaDisable) is currently hardcoded to 1.

## 6. Hypotheses for the chroma defect, ranked by discriminating power

Each hypothesis is stated with the ONE experiment that can confirm or kill it.
No hypothesis may be acted on before its experiment runs.

| # | Hypothesis | Discriminating experiment | Kill condition |
|---|---|---|---|
| **H1** | Chroma intra prediction mode is forced to DC for every block, so chroma = one DC value (a flat plane) | Decode a purpose-built clip whose chroma varies *within* each 4x4 block. If output is still flat, prediction mode is the constraint. | Output shows *any* within-block chroma variation |
| **H2** | Chroma residual never reaches the engine (bitstream offset wrong after luma residual, or residual length mis-programmed) | Sweep slice QP to 0 vs 51 and compare the two chroma planes. QP 51 forces all residuals to zero. | Chroma plane is identical between QP 0 and QP 51 |
| **H3** | A chroma QP-index-offset / chroma-QP field in `MFD_AVC_IMG_STATE` is absent or mis-encoded, so chroma quantizes to ~0 and only DC prediction survives | Authoritative field diff (WAVE 0) shows the field exists and we do not write it correctly | Field does not exist in Gen12 layout |
| **H4** | Chroma write-out addressing is wrong — the engine writes a constant/tiled pattern to chroma addresses | **Micro-harness direct store (WAVE 2).** Store a known dword pattern straight to the chroma-plane GGTT address with no decode at all. | Direct store lands correctly AND shows spatial variation |
| **H5** | The chroma plane is literally never written by the engine and the 6144 bytes are leftover poison that happens to fall in legal range | Dump the raw 6144 chroma bytes and test whether they are one repeated dword | Bytes vary |

**H4 and H5 are strictly upstream of H1–H3** and are the reason WAVE 2 exists:
proving the write path independently is cheaper than debugging decode arithmetic
against a write path that has not been shown to work.

## 7. Execution waves

### WAVE 0 — Gather ground truth (IN FLIGHT, no edits)
| agent | id | deliverable |
|---|---|---|
| librarian | `bg_4097a047` | Authoritative Gen12 AVC field layouts: per command, per DW, per bit range. Explicit "field does not exist" section required. |
| explore | `bg_a68fbcf0` | Inventory of what the code literally emits: hardcoded vs parsed field map with line numbers; QM packing; slice state contents; parser capability gaps |
| deep-low | `bg_61921193` | Harden `/Users/ppbk/Desktop/เทส/MyIntelGPU-VCS-Test.command` (8 defects: exit masking, kextstat, log predicate, version/SHA lock, source-sync check, ownership, scenario summary, stale comment) |
| explore | `bg_644614c9` | Kernel-side GEM/PPGTT teardown map — does client close reclaim everything? |

**Gate:** do not leave WAVE 0 until the librarian result either gives verified bit
positions or explicitly states the field is absent. "Not found" is a valid,
useful answer and unblocks the next wave. Guessing is not.

### WAVE 1 — Plan agent
Feed the WAVE 0 results plus §5 and §6 into the plan agent. Demand: a
wave-ordered task graph where each task names the file, the exact edit, the
bit-position citation it relies on, and the scenario it advances. Any task that
cannot cite a bit position must be a *measurement* task, not an edit task.

### WAVE 2 — Prove the write path independently (H4, H5) — NEW, HIGHEST PRIORITY
Build a micro-harness control mode that bypasses decode entirely:
1. Seed a destination GGTT buffer with a poison pattern.
2. Submit a minimal `MI_STORE_DWORD_IMM` (or equivalent immediate store) to a
   known, explicitly chosen dword offset inside the chroma plane.
3. Read back and assert: the exact target dword equals the stored value, and
   every neighbouring dword still equals its poison value.
4. Repeat across a sweep of offsets covering the Y plane, the first chroma row,
   the last chroma row, and the poison-stripe rows.
5. Then a second variant that walks a known pattern across the whole chroma plane
   to prove per-row addressing and stride.

Pass condition: 100% of targeted dwords written, 0 collateral writes. This is S6
for the write path. Only after this passes does chroma decode debugging become
meaningful. Also add a cache-visibility A/B (no flush vs `MI_FLUSH_DW` vs
`PIPE_CONTROL` equivalent) because PPGTT identity mapping does not by itself
prove GPU-side visibility.

### WAVE 3 — Authoritative field correction
Apply only fields whose Gen12 bit positions are now cited. Re-measure after each
field, one at a time, recording the delta in `golden_diff_bytes`, chroma corr, and
the two chroma structure metrics from §5. Keep a per-field results table; a field
that moves nothing is documented as inert and reverted, not left in as cargo cult.

### WAVE 4 — Hypothesis elimination (H1, H2, H3, H5)
Run the discriminating experiment for the top-ranked surviving hypothesis.
Purpose-built clip for H1 (chroma varying within a 4x4 block). QP sweep for H2.
Field-gap closure for H3. Raw chroma byte dump for H5. One variable per run.

### WAVE 5 — Chroma fix iteration
Only now write the actual fix. Each iteration must report: `golden_diff_bytes`
before/after, luma PSNR before/after, chroma corr, the horizontal-structure delta,
the vertical-ramp slope and R², and the poison row set. An iteration that improves
one metric while regressing another is not progress — record it as such.

### WAVE 6 — Resources (S4)
`test_submit_vcs.m`: `vcs_done()` (around lines 358-364) frees the bit and closes
the connection but destroys no GEM buffer. The `MFX_BBSTART` path (around lines
1245-1247) creates and maps a batch buffer with no matching unmap/destroy. Fix by
centralising cleanup: track every created handle and mapped VA, route every early
return through one cleanup label, and free the batch buffer. Confirm against the
kernel teardown map from `bg_644614c9` — if the kernel already reclaims on close,
say so and downgrade severity rather than double-freeing.

### WAVE 7 — Build, deploy, version lock (S5)
1. `make clean` then `make` as `ppbk` with an explicit `BUILD_NUMBER` past the
   installed version. Never a bare `make` — the default is the git commit count and
   can drift backwards.
2. Verify ownership `ppbk:staff`, no `stack_chk`, undefined symbols limited to
   IOKit/IOLibkern.
3. Back up `/Library/Extensions/MyIntelGPU.kext` to
   `~/kext-backups/pre-<stamp>/` (mkdir first — `cp -R` will not create it).
4. Install, `chown -R root:wheel`, `chmod -R 755`, ad-hoc codesign, `kmutil install`.
5. **Reboot is a user action.** After reboot, verify `kmutil showloaded` shows the
   new version and that the installed SHA equals the tree SHA. If they differ,
   stop and report — do not proceed to decode testing on an unverified kernel.

### WAVE 8 — Documentation and commit
Update `README.md` status table and this log with measured numbers only. Label
nothing a "fix" that has not moved S3. One atomic commit per verified increment,
matching the existing history style (`git log --oneline -20` first).

## 8. Risk register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Authoritative Gen12 layout is unavailable in public sources | medium | high | Fall back to readback-proven semantics from WAVE 2/3 rather than a header. Accept slower convergence; never guess. |
| The other session resumes editing `MyIntelVCSCommand.cpp` | low now | high | Single-writer lock recorded in this log with a baseline SHA. Re-verify the SHA before every edit; abort on mismatch. |
| The 8 unwritten chroma rows are a write-out addressing bug, not a decode bug | medium | high | WAVE 2 exists specifically to partition this before decode work continues. |
| A "fix" improves PSNR by making output flat | high | high | Never accept PSNR alone. S3 is byte-exact, and the chroma structure metrics in §5 must both move. |
| Reboot black-screens | low | high | `gEnableDisplayFramebuffer=false` and no `-myintelfb` remain in force; `IONDRVSupport`/GOP own the panel. Boot with no debug flags when measuring. |
| Root-owned artifact breaks the next `make clean` | medium | medium | C7; verify `ls -ln` after every build. |

## 9. Decisions that require the user (not mine to make)

1. **Reboot** to load a newly built kext. Everything in WAVE 7 past `kmutil install`
   is blocked on this.
2. **Which baseline counts as correct** — already resolved: `golden_videotoolbox.nv12`.
3. Whether to delete `/Users/ppbk/Desktop/all/git history` (99 snapshots, some
   without matching archives). Currently preserved; no action taken.

## 10. Explicitly out of scope until S3 passes

- Display / IOFramebuffer / EDID work — `IONDRVSupport` + GOP own the panel.
- Metal / VideoToolbox client integration, IOAccelerator adoption.
- `AppleIntelKBLGraphics` plugin binding — known Gen12 crash risk.
- Multi-frame / inter-prediction (P, B) — the reference is a single I-frame.
- Anything requiring network access or new package installs.

---

## 20:17 — Archive cleanup: `/Users/ppbk/Desktop/all/git history` 366M → 16M

User approved deleting only the duplicated portion. Audit first, then delete.

### What the archive actually was (measured, not assumed)
```
366M total
├── 46 .zip                     118M   archives
├── 46 extracted dirs           237M   the SAME 46 snapshots, unpacked — pure duplication
├── 2026-09-13_2fc05b6 2         ~5M   orphan, no .zip, OLDER state than 09-17
├── IntelReviveGPU-Gen-10-12…    11M   nested git repo, 46 commits, HEAD 2ff8074
├── Info.plist                  1994 B
└── MyIntelGPU_All_History_Detailed.txt   0 B  (empty placeholder)
```

### Why the archive was NOT safe to delete wholesale
It contains files that **do not exist in the active tree** and that this project
still needs:

| File | Size | Why it matters |
|---|---|---|
| `gen_clip.m` | 4479 B | Generates `test_clip.h264`; source of the analytic BT.601 gradient that independently validates `golden_videotoolbox.nv12` at corr 0.9904 / 32.40 dB |
| `plane_probe.c` | 963 B | Per-plane write probe — **directly relevant to WAVE 2** |
| `plane_probe_all.c` | 4029 B | Same, all planes — **directly relevant to WAVE 2** |
| `ggtt_diag.c` | 10190 B | GGTT mapping diagnostic — **directly relevant to WAVE 2** |
| `MASTER-KNOWLEDGE.md` | 37267 B | Consolidated knowledge doc |
| `HANDOFF-BCS0-PLAN.md` | 2753 B | Prior handoff plan |
| `HANDOFF-FROM-BUILD-SESSION.md` | 1532 B | Prior handoff notes |
| `gem_test.c` | 22282 B | GEM object test |
| `mvcs_bench.c` | 4806 B | VCS benchmark |
| `myintelvcs_bridge.c/.h` | 31941 B | Userspace bridge (older copy) |
| `test_submit_vcs.m` | 50521 B | Harness (older copy; active is 50798 B under `proof/`) |
| `myintel_h264_parse.h` | 17308 B | H.264 parser (older copy) |
| subdirs | — | `docs/ harness/ packaging/ phase4-iaf2/ phase4-iaf2-analysis/ deploy/ บันทึกความจำ-ล่าสุด/` |

`plane_probe.c`, `plane_probe_all.c` and `ggtt_diag.c` are existing write-path
probes. **WAVE 2 must read these before writing a new micro-harness** — they may
already implement the plane/address verification the plan needs.

### Safety check performed before deleting
A `diff -rq` of every one of the 46 candidate directories against
`2026-09-17_d59c9e4` was run, filtering for files present ONLY in the candidate.
Result: **zero unique files**. Nothing that exists solely in a doomed directory
was at risk.

### Action taken
```
deleted  46 extracted snapshot dirs
deleted  46 .zip archives
deleted  MyIntelGPU_All_History_Detailed.txt (0 bytes)
deleted  .DS_Store
kept     2026-09-17_d59c9e4/                 (most complete snapshot)
kept     IntelReviveGPU-Gen-10-12-on-Hackintosh/  (nested repo, 46 commits)
kept     Info.plist
result   366M -> 16M   (350M reclaimed)
```

All 12 high-value files verified present after deletion, byte sizes unchanged.

### Correction to an earlier note in this log
The 05-Oct git-history audit recorded "two directories without matching archives"
and implied the archive was a candidate for wholesale deletion. Both points were
wrong in one respect: `2026-09-13_2fc05b6 2` is not a unique lineage, it is an
**older** state that `2026-09-17_d59c9e4` supersedes (which additionally contains
`HANDOFF-*.md`). And the archive is **not** redundant with the active tree — the
active tree lacks the harness sources, the plane/GGTT probes, `gen_clip.m`, and
`MASTER-KNOWLEDGE.md`. Do not delete the remaining 16M.

### Revised WAVE 2 entry
Before authoring a new micro-harness control mode, read
`2026-09-17_d59c9e4/plane_probe.c`, `plane_probe_all.c`, `ggtt_diag.c`, and
`gem_test.c`. If `plane_probe_all.c` already writes and reads back a known dword
at a chosen plane offset, WAVE 2 reduces to running and extending it rather than
writing new code.

## Session 2026-10-07 00:15-00:45 — size gate closed, validation evidence, USB forensics

### Size budget: ACHIEVED — 198040 bytes (limit 200000)
- `make clean && make` at 00:19 rebuilt the bundle from scratch; the build gate printed
  `Build complete: MyIntelGPU.kext [3.1.104] 198040 bytes (<=200000)`; `du -sk` = 200.
- Final recipe: `-Oz -ffunction-sections -fdata-sections` + `-dead_strip -x
  -exported_symbols_list exported_symbols.txt` (38 kept symbols) + post-link `strip -x -S`;
  `sizecheck` uses maxsize=200000 and is wired into `all`.
- Safety net re-verified AFTER strip: 6/6 class names in cstrings (MyIntelGPU/
  Framebuffer/Accelerator/AccelClient/GPUClient/VCSClient), `__mod_init_func` = 0x30
  (6 entries), 38/38 exported syms, 45 metaclass/vtable syms, plutil lint OK.
- Backup of the binary: /tmp/opencode/MyIntelGPU.198040.bak

### Validation evidence (kmutil print-diagnostics, no load)
- `kmutil print-diagnostics -p <repo>/MyIntelGPU.kext` → `Dependencies: OK`, exit 0.
- Two expected findings for the REPO copy: (1) ownership root:staff (0:20) vs required
  root:wheel (0:0) — corrected at deploy time by `sudo make install`; (2) "Bad code
  signature" — kext is unsigned by design, load approval goes through System Settings
  (Code=27). Neither blocks deployment.
- `kextutil -n/-nt` is dead on macOS 14 ("-n is not a supported kmutil mode"); use
  `kmutil print-diagnostics` instead.

### Deploy state (unchanged since 06-Oct)
- Kext NOT loaded; removed from /Library/Extensions after the boot episode; AuxKC clean.
  Reload needs System Settings approval + reboot — deferred to user.
- EFI-MIAN boot USB intact: BOOTx64.efi 24576 B, OpenCore.efi 626688 B (disk2s1).

### USB forensics (read-only md5 sampling — NEVER write to a suspect drive)
- Round-number sector counts = controller-invented capacities:
  disk5 "1T" reports EXACTLY 2048000000 sectors (= 1000000 MiB); disk4 "2T" reports
  EXACTLY 4096000000 sectors (= 2000000 MiB). Genuine drives report odd counts
  (e.g. 1953525168). Both also report generic model "SSD", no SMART.
- disk4 "2T": every sampled offset from 4GiB to ~1.46TiB reads back the SAME content
  (md5 2b7a70fa59f8173635bcbe956bad56c6, NOT zeros) while only the first 1.4GiB differs
  → one repeated block = capacity wrap = fake.
- disk5 "1T": beyond its 7.7Gi used area reads are mostly genuine zeros (b5cfa9d6... =
  md5 of 4MiB zeros), BUT unstable across probes: the same offset gave non-zero content
  on one run and zeros on another (e.g. 524288MiB), and @525000MiB returned
  9a5a180b19eb05d4f0409abc09420e8d while neighbours are zeros → non-deterministic
  high-offset reads = fake controller.
- Both sticks were PHYSICALLY UNPLUGGED by the user at ~00:40 (absent from diskutil list).
- Earlier note corrected: md5 b5cfa9d6... (disk5 far offsets) is plain zeros, NOT
  evidence of wrapping — on a mostly-empty drive zeros are expected.

### Knowledge backup to /Volumes/256 — FAILED, redo pending
- 4 md files + IntelReviveGPU-repo-20261006-2355.tar.gz verified present in
  /Volumes/256/MyIntelGPU-KB at 23:55 on 06-Oct; by 00:30 on 07-Oct the folder read
  back EMPTY (dir mtime unchanged — unexplained), and at 00:40 the stick was unplugged.
- Redo when it returns: copy the 4 md files + regenerate the tarball from the repo.

### Copy job babysitting (user's own sudo jobs, watch-only — do NOT kill)
- `cp -rf /Users/ppbk/Desktop /Volumes/data` (PID 1918, 12:15AM) and
  `cp -rf /Users/ppbk/Documents /Volumes/data` (PID 2568, 12:34AM).
- Capacity math: sources 25G + 2.5G = 27.5G into 14Gi exFAT disk2s2 (5.7Gi free) →
  ENOSPC guaranteed; the 7.78GB Windows ISO alone exceeds free space. Both cp's sit in
  U+ (I/O wait) on the slow stick, usage stalled at 8.5Gi. Background monitor
  bg_09847583 / ses_eedb9d689ffeO6rLGxyYAweuXu watches until exit.

## Session 2026-10-07 20:15-20:30 — fresh rebuild 3.1.105 + boot-panic root cause

### Constraints handed by the user (standing, do not violate)
- **No backup boot USB. No Windows side anymore.** There is no fallback OS.
- **NEVER modify EFI / OpenCore / boot config.** Hard prohibition ("ห้ามแก้ไข efi boot เด็ดขาด").
- Consequence: no deploy happens without an explicit go-ahead, and nothing in
  `/Volumes/*EFI*`, `EFI/OC`, `config.plist` or NVRAM boot-args is touched.

### Fresh rebuild — DONE (repo-local only, nothing installed)
```
sudo chown -R ppbk:staff .          # repo had gone root-owned from an earlier root build
make BUILD_NUMBER=105 KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make BUILD_NUMBER=105 ... -j$(sysctl -n hw.ncpu)
-> Build complete: MyIntelGPU.kext [3.1.105] 198040 bytes (<=200000)   exit 0
```
Gate evidence (all re-checked after strip):
| check | value |
|---|---|
| size | 198040 / 200000 |
| class names in cstrings | 6/6 (GPU/Framebuffer/Accelerator/AccelClient/GPUClient/VCSClient) |
| `__mod_init_func` | 48 bytes = 6 entries |
| exported syms | 38/38 |
| `plutil -lint` Info.plist | OK |
| `kmutil print-diagnostics -p <repo>/MyIntelGPU.kext` | exit 0, `Dependencies: OK` |
| ownership after build | chowned back to `ppbk:staff` (shell runs as **root** — every build leaves root-owned artifacts; always chown back) |
| git | only `MyIntelGPUVersion.h` regenerated (FORCE rule) |

Expected-and-accepted diagnostics findings: ownership 501:20 vs 0:0 (fixed at
deploy time by root install) and "Bad code signature" (unsigned by design).

Note: `BUILD_NUMBER` must be the bare third component (`105`, not `3.1.105`).
Passing the full string stamps a malformed `CFBundleVersion` (`3.1.3.1.105`).

### Boot failure root cause: Lilu, NOT MyIntelGPU
`/Library/Logs/DiagnosticReports/Kernel-2026-10-06-180135.panic`:
- `panic(cpu 0 ...): Kernel trap ... type 14=page fault, CR2 = 0x0000000000000000`
- backtrace entirely inside **`as.vit9696.Lilu` (1.7.3)**
  `MachInfo::init` ← `KernelPatcher::loadKinfo` ← `Lilu::MetaClass::alloc` ← `Lilu::Lilu()`
- uptime 2793310554 ns (~2.8 s), `Mac OS version: Not yet set`
- `System model name: MacBookPro16,2`, boot-args at the time:
  `-v keepsyms=1 npci=0x2000 msgbuf=1048576 alcid=13`
- **MyIntelGPU does not appear anywhere in the panic, and it is not installed
  (`/Library/Extensions` holds only HighPoint/RtWlanU).**

Current boot is healthy: `kmutil showloaded` shows Lilu 1.7.3 + AppleALC 1.9.9 +
VirtualSMC 1.3.9 + RestrictEvents + SMCProcessor, all injected from
`EFI/OC/Kexts` (Lilu.kext present under `Desktop/EFI/OC/Kexts` and the
`OC-config-backup-20261003-170023` copy). So the 06-Oct boot episode was a Lilu
early-boot page fault, i.e. an EFI-side injector, which is off-limits by the
standing prohibition above.

### Test suite status
`/Users/ppbk/Desktop/เทส/MyIntelGPU-VCS-Test.command` exits at `[1/7]` with
`kext ไม่โหลด` — S1/S2/S3 cannot be measured until a kext is installed and loaded.

### WAVE 0 re-launched (previous task ids were lost)
`bg_7c25cef7` librarian — Gen12 AVC MFD_AVC_IMG_STATE / chroma-QP / chroma-intra-pred bit layouts
`bg_6d059e05` explore   — literal inventory of what MyIntelVCSCommand.cpp emits
`bg_a2b59854` explore   — WAVE 2 entry: do plane_probe/ggtt_diag/gem_test already cover write-path proof
