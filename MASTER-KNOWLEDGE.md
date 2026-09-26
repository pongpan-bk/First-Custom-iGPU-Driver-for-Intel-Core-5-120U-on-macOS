# MASTER-KNOWLEDGE — IntelReviveGPU / MyIntelGPU.kext (Gen 10-12 on Hackintosh)

> ⚡ **ไฟล์รวมความรู้ฉบับเดียว — รวบรวมเมื่อ 2026-09-08 (หลัง Mac ล่ม, กู้ข้อมูลจาก Ventoy USB)**
> ที่มา: IntelReviveGPU (Gen 1012+) on Hackintosh (Genesis, 440+ commits), GitHub origin, Linux i915 ground truth,
> Windows 11 native driver registry, WORK_MEMORY ทุกฉบับ, KNOWLEDGE-BASE, Mission C/D design, BCS-DOSSIER, git history, code analysis
>
> **เป้าหมาย:** ให้ไฟล์นี้เป็น "สมองเดียว" ที่พออ่านจบ = รู้ทุกอย่างที่เคยทำ/ค้น/พิสูจน์ + ขั้นต่อไป ต่อได้ทันทีโดยไม่ต้องงมเอกสารเดิม

---

## 📌 สรุปสถานะ ณ 2026-09-08

| รายการ | สถานะ |
|---|---|
| **kext @ L/E** | `com.pongpan-bk.MyIntelGPU` **2.0.1** (UUID `81540B60-01EB-39D4-A2A7-ED8717097446`) — deploy 2026-09-07 15:04 |
| **Build ล่าสุดใน CI** | **2.0.483** (`e8bc0d2`, device-id ครบ 10 ตัว RPL, artifact 73,415 B, run `33780951501`) — ยังไม่ได้ deploy |
| **GPU render** | ❌ GPUActivityInPercent = **0** → AppleSoftwareRenderer (Metal = CPU fallback) |
| **Display** | Web: อยู่ใต้ `.Display_boot` (Apple IONDRV/BIOS plane) — MyIntelFramebuffer=0, MyIntelAccelerator=1 |
| **HW decode** | 🏆 พิสูจน์แล้วใน dev: VDBOX H.264 FHD **406 FPS** (fail=0), multi-client, YouTube 1080p |
| **ความสำเร็จ display (อดีต)** | Git history ยืนยัน: display chain adoption ผ่านถึง **2.0.445 PERFECT PASS** (zero CoreDisplay errors, ProductID 2340) — cwd ที่ deploy (2.0.1) เก่ากว่านั้นมาก |
| **สิ่งที่กำลังติด** | 1) render path ปิดด้วย boot-args → 2) BCS ring queued-never-dispatched → 3) M2C-K (MTLCreateSystemDefaultDevice) ยังไม่ทำ |
| **การ์ดจอ** | Intel Iris Xe **RPL-U `8086:a7ac` rev04**, 80EU — Acer Aspire AL15-52P (Core 5 120U, 10C/12T) |

> 💻 แมคปัจจุบัน: ล่มไปแล้ว → ข้อมูลถูกกู้มาไว้ที่ `/Volumes/Ventoy` + Desktop (ดู §11) — ไฟล์นี้เป็นตัวแทนความรู้ทั้งหมด

---

## 1. Hardware Ground Truth (ยืนยันหลายแหล่ง)

### 1.1 PCI (lspci จาก Ubuntu จริงบนเครื่องเดียวกัน — แม่นสุด)
```
0000:00:02.0 VGA compatible controller [0300]: Intel Raptor Lake-U [8086:a7ac] (rev 04)
  Subsystem: Acer [1025:192e]
  BAR0 = 0x6002000000 (64-bit, non-prefetchable)  [16MB]  ← MMIO regs (mapDeviceMemoryWithIndex(0))
  BAR2 = 0x4000000000 (64-bit, prefetchable)      [256MB] ← Aperture/GMADR (comment "256MB" ใน .hpp ✓)
  I/O ports 0x4000 [64], Expansion ROM 0x000c0000 [128K] (GOP, disabled), IRQ 177, IOMMU group 0
  Kernel driver: i915 (+ module xe มี) — Linux ยืนยันซิลิคอน
```
> ⚠️ **BAR สลับกันระวัง**: `WORK_MEMORY_LATEST.md` (HWiNFO Windows) เขียน BAR0=0x4000000000/256MB สลับกับ BAR2 — **ผิด**
> lspci + iGPU_Report.txt + KNOWLEDGE-BASE ตรงกันหมด: **BAR0=0x6002000000[16M] reg / BAR2=0x4000000000[256M] aperture**
> HWiNFO เรียกว่า "GTTMMADR 256MB" = BAR2 (aperture+GTT area), BAR0 16M = MMIO regs

### 1.2 Sysfs (my_igpu_io_tree.txt — health check)
- i2c busses **5-13 (9 ตัว)** = DDC/DP aux channels
- DRM card1 connectors 6 ตัว: **eDP-1, DP-1..DP-4, HDMI-A-1**
- GT freq files: RP0/RPn/RP1/act/cur/min/max/boost
- renderD128, fb0 (console fb มี stride/mode/virtual_size), boot_display marker

### 1.3 พิกัด (จาก WindowsDriver-REFERENCE 2026-09-03)
- Registry: `HKLM\SYSTEM\CurrentControlSet\Enum\PCI\VEN_8086&DEV_A7AC&SUBSYS_192E1025&REV_04\3&11583659&0&10`
- Windows kernel driver: **igdkmdn64.sys** v32.0.101.5972 (~52.85MB) — `.rdata` 44MB = register/init/PLL tables (ของแท้ที่ต้อง reverse → IOKit)
- RPL device-id family (ทั้งหมดใช้ `iRPLPD_w10_DS`): A7A0 A7A1 A7A8 A7A9 A720 A721 A7AC A7AD A7AA A7AB — **platform = XeLP**
- Windows services: DisplayEnhancementService, IntelGraphicsSoftwareService, IntelAudioService, IntelCollectorService

### 1.4 เฟิร์มแวร์/เครื่องมืออ้างอิง
- Linux i915 register map = ต้นทาง port → kext (`linux_7.0.0.orig.tar.gz` 254MB อยู่ใน Genesis folder)
- GOP.rom (`Intel_iGPU_GOP.rom`) = **0 bytes ว่างเปล่า** (สร้างไม่สำเร็จ — ข้ามไป)
- Windows Intel driver = ไกด์พอร์ต register/ring/PLL logic (อ่าน registry ได้อย่างเดียว ห้ามเขียน)

---

### 1.5 ACPI / DSDT Ground Truth (วิเคราะห์ 2026-09-08 — จาก MaciASL dump จริง)

**ที่มา:** `/Users/ppbk/Desktop/all/ACPI/` = DSDT.aml (600,472 B, `ACRSYS`/`ACRPRDCT`, iASL 1025 v4.0.0) + **SSDT ×34** (SSDT-0..33)
disassemble ครบด้วย `iasl-stable 20200925` (`-e SSDT*.aml` แบบ comma-list) → **externals 238 ตัว → 0** ✅
สำเนา: `all-in-1-myinteligpu.kext-work/ACPI-dump/` (DSDT-full.dsl 4MB + SSDT-6-GFX0-connectors.dsl + PNLF + พวก .aml)

**ข้อค้นพบที่เปลี่ยนความเข้าใจ display:**
- **GFX0 ใน DSDT = แค่ stub** `Device(GFX0){ _ADR 0x00020000 }` — ไม่มี `_DSM`, ไม่มี connector
- **ตัวจริงอยู่ใน `SSDT-6-ACRPRDCT`** (13,289 B): `Scope(_SB.PC00.GFX0)` มี `_DOS` + `_DOD` (dynamic: DID1-DIDF = `SDDL(DDL1..DDL15)`) + **`Device DD1F` (eDP) + `Device DD2F`**:
  - `DD1F._ADR` = `0x1F` ถ้า `EDPV==0` else `DIDX & 0xFFFF`; `_DCS`→`CDDS(DIDX)`; `_BCL` 100 ระดับ (0x00-0x64); `_BCM`→`GFX0.AINT(1, Arg0)` + `BRTL=Arg0`; `_BQC`→`BRTL`
  - `DD2F` เหมือนกันแต่ใช้ `DIDY`, idle `0x1F` เมื่อ `EDPV∈{0,1}`
  - → **eDP ผ่าน ACPI `DD1F`** = ตรงกับ WindowsDriver-REFERENCE เป๊ะ; node path = `PciRoot(0x0)/Pci(0x2,0x0)` = ioreg `GFX0@2` ✅
- **SANV opregion** (SystemMemory 0x43941418, 0x1BC — address per-boot): `ASLB`, `IGDS`, **`CADL`**, `NDID`+`DID1-DIDF`, `LIDS`, `BRTL`/`ALSE`, **`EDPV`**, `HGMD`, `KSV0/1` (HDCP), `VTDS/VTB1-7` (VT-d), `CPEX/M32B/M32L/M64B/M64L`
- **GNVS opregion** (0x43937018, 0x0D1D): `OSYS`, `SMIF/PRM0-3` (SMI), `BTYP`, `DPTF`, fan curves, EC, USB policy
- **SSDT-27-PNLF** (OEM `ZPSS`, compiler `INTL 20260408` = build ใหม่!) = `GFX0.PNLF` `APP0002`/`backlight`, `_UID 0x13`, `_STA 0x0B` **เฉพาะ Darwin** — 141 B ตรงกับ `EFI/OC/ACPI/SSDT-PNLF.aml` (โหลดอยู่)
- EFI OC/ACPI โหลด **13 SSDT** (ALS0, Disable_Network_RP05, EC, GPI0, MCHC, PLUG-ALT, PNLF, RMNE, RTCAWAC, SBUS, USB-Reset, USBX, XOSI) — ทั้งหมด `Enabled=true`
- SSDT-5 = `PEG0-3._DSM` (PCIe slots, ไม่เกี่ยวกับ iGPU); SSDT-18 = `GSCI/GSSE/GSMI` (IGD opregion SMBIOS interface)

**ความหมายต่อ kext:** macOS ไม่อ่าน SANV/EDPV (คนละ ACPI interface กับ Windows/Linux) → kext ต้อง detect display เองผ่าน MMIO,
และมี SSDT-PNLF คอยให้ AppleIntelPanel หา backlight device — **ไม่ต้องพึ่ง ACPI สำหรับ display detect**

**Live ยืนยัน 2026-09-08 (ioreg จริง):** `display0`/`AppleDisplay` อยู่ใต้ `.Display_boot` (**IONDRVFramebuffer**) ไม่ใช่ kext เรา
— `MyIntelAccelerator=1`, **`MyIntelFramebuffer=0`** (ตรง IOKitDiagnostics), DisplayProductID **2340** / Vendor **11394 (0x2C82 BOE)**,
EDID `00 ff ff ff ff ff ff 00 2c 82 24 09...`; live boot-args = `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=13 amfi_get_out_of_my_way=1` (**ไม่มี myintelfb/myintelaccel**)

### 1.6 Live kext properties (ioreg 2026-09-08 — MyIntelGPU node)

```
Class: MyIntelGPU (IOService > IORegistryEntry > OSObject), Bundle com.pongpan-bk.MyIntelGPU
model: "Intel Iris Xe 80EU" · IOPCIDeviceMatch: 0x03000000&0xff000000  ← class-match (attach ไม่ขึ้นกับ device-id)
IOProbeScore: 0x270f (9999) · IOProviderClass: IOPCIDevice · IOUserClientClass: MyIntelGPUClient · IOMatchCategory: IOAccelerator
device-id: ✅ **0xA7AC ยืนยัน 100%** — ioreg GFX0@2 (IOPCIDevice): `compatible = "pci1025,192e","pci8086,a7ac","pciclass,030000"` + `device-id=<aca70000>` ตรง lspci/Windows
  (ค่า 0xA720/0xA728 ที่โผล่บน node MyIntelGPU = property `device-id` จาก personality ที่ IOKit merge ให้ — Universal_Match_204C/B device-id=0x20a70000/0x21a70000 — ไม่ใช่ฮาร์ดแวร์; ฮาร์ดแวร์ดูที่ GFX0@2 IOPCIDevice = a7ac เท่านั้น)
Codec: AV1/H264/HEVC/VP9 decode = True, H264/HEVC encode = True · MaxDecode 8192², MaxEncode 4096²
RCS-GTT: 0x400400c · RCS-MMIO: 0x2000 (RCS ring base register offset จาก BAR0 — ตรง i915 0x02000) · RCS-Status: CREATE OK
VBOXCount/VEBOXCount: True · IntelMediaCapable: True
VRAM memSize: 0x80000000 (2GB) · VRAM totalMB: 0x1000 (4096MB)
```

### 1.7 Audio — HDEF@1F,3 (from IORegistryExplorer dump 2026-09-08)
```
Path: AppleACPIPlatformExpert/PC00/AppleACPIPCI/HDEF@1F,3   (ACPI: _SB/PC00/HDAS@1F0003)
Class: IOPCIDevice · vendor-id 0x8086 · device-id 0x51CA (Intel Smart Sound Technology Audio) · rev 1
subsystem-vendor 0x1025 (Acer) · subsystem-id 0x192e     ← ตรงกับ GFX0 (device 0x2e = 192e)
name "pci8086,51ca" · compatible "pci1025,182e","pci8086,51ca","pciclass,040100","HDAS","HDEF"
layout-id = <07 00 00 00> (7) · alc-layout-id = <0d 00 00 00> (13)  ← บูต arg alcid=13 ตรง alc-layout-id 13
hda-gfx "onboard-1" · AFGLowPowerState=3 · PinConfigurations มี (AppleALC/PinConfigs)
driver-child-bundle: com.apple.driver.AppleHDAController · IOPCIRescued=True (ถูก rescue/remap)
```

## 2. Display — หลักฐาน "จอโผล่" (สำคัญ!)

### 2.1 ICC profile ที่ ColorSync สร้าง (เจอ 2026-09-07)
```
/Library/ColorSync/Profiles/Displays/
  Unknown Display-B533C716-00DC-B94A-F8FB-5B7B06EB9F78.icc   ← 3356B, 23:41, UUID จริง มี vcgt tag
  Unknown Display-FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF.icc  ← 1044B, 21:03, FFFF fallback
```
- **B533C716 = UUID จริง** → ColorSync เห็น EDID + จับ display ตัวจริง (ไม่ใช่ fallback) = "จอโผล่" ในระดับ detect ได้แล้ว
- ชื่อยังเป็น "Unknown Display" เพราะ macOS อ่านชื่อจอไม่ได้ (EDID descriptor ยังเป็น 0xFE)

### 2.2 ioreg display tree (live)
```
GFX0@2 (IOPCIDevice)
├── .Display_boot → IONDRVFramebuffer → display0 → AppleDisplay   ← ตัวที่ macOS ใช้ (BIOS/Apple chain)
│       DisplayProductID=2340, DisplayVendorID=11394 (0x2C82), EDID มี (ดู 2.3)
├── MyIntelGPU (class MyIntelGPU — kext เรา, registered)
├── MyIntelAccelerator (class MyIntelAccelerator — มี 1! busy 1ms)
└── [MyIntelFramebuffer = 0 — ไม่ได้ถูกสร้างใน boot นี้]
```
- MyIntelAccelerator=1 → accelerator ถูกสร้างแล้ว แต่ยังไม่ถูกใช้ (อยู่ในสถานะ idle)
- display0 ใต้ `.Display_boot` = ของ Apple IONDRV (BIOS plane scanout) **ไม่ใช่** ของ kext เรา

### 2.3 EDID — fix "Unknown Display" (บทเรียนสำคัญ!)
- EDID แท้จาก Windows registry (`KDB0924`, checksum 0x01): descriptor 4 มี `0xFE` (ASCII string) แทน `0xFC` (Display Name)
- **Fix คือเปลี่ยน byte 111: `0xFE→0xFC` + checksum คำนวณใหม่ `0x01→0x03`** → macOS จะอ่าน `KD156N2930A02` เป็นชื่อจอ (commit `f3594e0` 2026-08-26, MyIntelFramebuffer.cpp)
- ⚠️ **ใน binary ที่ deploy จริงมี EDID table 3 ชุด: 0x1fc6f=0xFE(เก่า), 0x1fd51=0xFC(แก้แล้ว), 0x1fdd1=0xFC(แก้แล้ว)** — แต่ EDID ที่ live อยู่ยังเป็น 0xFE+checksum 0x01
- **ทำไมยัง "Unknown Display"**: fix ทำงานผ่าน path MyIntelFramebuffer (ต้องมี boot-arg `myintelfb`) — boot นี้ไม่มี → framebuffer ไม่ได้สร้าง → EDID 0xFC ไม่ถูกใช้ เส้นทาง DDC/EDID passthrough (loop ทุก 10s, dmesg "injected KDB0924/KD156N2930A02 onto AppleDisplay") เขียนไม่ถึง node ที่ ColorSync อ่าน
- **วิธีทำจริง (WindowsDriver-REFERENCE §8):** build v475+ → deploy L/E → verify `sudo dmesg | grep MyIntel` เห็น EDID injected → System Profiler ขึ้น `KD156N2930A02`

### 2.4 ประวัติ display ที่เคยสำเร็จ (git history Genesis)
| commit | เหตุการณ์ |
|---|---|
| 40e6a14/4cf4abb/0b37729/1e310f0/12c2a5b/5844970 | display chain adoption ต่อเนื่อง, ProductID 2340, **2.0.445 PERFECT PASS — zero CoreDisplay errors** |
| 0b27446 | Phase 4c-1 VICTORY: myplane=1 scanout owned, GGTT=0x041B0000 |
| d8a2ef6 | 4d-a mirror takeover 10Hz |
| d1b04b7 / 62c7703 | HW BCS blit 60Hz / CPU-copy 60Hz |
| 2594194 | D-final double-buffer flip at vblank |
| 217e8d6 | SURFACE PRESENT Flush(10) |
| 2f25630 | BCS blit user surface |
| 5ad813a / f3594e0 | EDID passthrough / Monitor Name 0xFE→0xFC |
| 047261f | DDC/EDID ColorSync |
| **55a2a432** | **"the one that worked"** — CoreDisplay allocated surfaces ผ่าน FB ของเรา; SIGBUS แก้โดย report fVRAMDescriptor=64MB |
| 0b27446+ | จอดำสาเหตุ (WORK_MEMORY): FB ชน IOMatchCategory กับ .Display_boot → main display offline; EDID ถูก gate หลัง MyIntelFramebuffer path ที่ตาย |

> 📌 **ข้อสรุป display**: โค้ดมีครบและเคยทำงานถึง PERFECT PASS — แต่ cwd 2.0.1 ที่ deploy เก่ากว่า Genesis 2.0.4xx มาก + boot-arg ยังไม่เปิด path

---

## 3. Render Path — ทำไม GPUActivityInPercent = 0 (root cause พร้อมหลักฐาน)

### 3.1 โค้ดมีครบ แต่โดน gate ด้วย boot-args 3 ตัว (วิเคราะห์จาก cwd v2.0.1)

| Boot-arg | Gate ให้อะไร | ผลเมื่อปิด |
|---|---|---|
| `myintelfbinit=1` | `initDisplay()` (การ์ดจอเริ่ม plane, programPlane) | จอไม่แตะ kext — ใช้ BIOS plane ผ่าน Apple |
| `myintelfb` | สร้าง `MyIntelFramebuffer` (IOFramebuffer) | MyIntelFramebuffer=0 — ไม่มี framebuffer ของเรา |
| `myintelaccel=1/2` | สร้าง `MyIntelAccelerator` (IOAccelerator) ให้ family | fallback → AppleSoftwareRenderer |

**boot-args ปัจจุบัน:** `-v keepsyms=1 npci=0x2000 msgbuf=1048576 -no_compat_check alcid=13 amfi_get_out_of_my_way=1` → **ไม่มีทั้ง 3 ตัว** → GPU-only mode

### 3.2 เหตุผลที่ gate ไว้ (กลัว crash)
- `initDisplay` → **จอดำ**: BIOS plane (PLANE_SURF_1A = GGTT page 0, scanout ที่ stolen base `0x4C800000`) — programPlane ผิด/ชนกับ BIOS → black screen
- `MyIntelFramebuffer` → **phantom fb1 + SIGBUS**: `CDDisplay::present_update` โดน (CoreDisplay เขียน fb ที่ VRAM ของเราแต่ report ผิด)
- Accelerator: "minimal no-family-ABI" — ถ้าเปิดตรงๆ จะ panic กับ IOAccelDevice2

### 3.3 โค้ดที่มีอยู่แล้ว (พร้อมเปิดใช้)
- `programPlane()` — stride/64 fix + SURFLIVE latch poll ✅
- `blitSurfaceToFramebuffer()` — XY_SRC_COPY_BLT ✅
- `ringEmitSurfacePresent` — surface present ผ่าน ring ✅
- Phase 5d, Phase 6c, Phase B (deferred t+45s) — pipeline ที่ออกแบบไว้

### 3.4 ลำดับการเปิด (แผน)
```
1. เปิด myintelfbinit=1 → ตรวจ programPlane ไม่ชน BIOS plane (จอไม่ดำ)
2. เปิด myintelfb → framebuffer พร้อม EDID 0xFC + fVRAMDescriptor (บทเรียน 55a2a432)
3. เปิด myintelaccel → accelerator รับ family-ABI (M2C-K ตาม §7)
```
⚠️ เสมอ: ทำทีละตัว + reboot ทดสอบทีละขั้น + rollback รู้ทิศ

---

## 4. Architecture (ชั้นของ kext)

```
MyIntelGPU (IOService) ← PCI provider, match class 0x03000000 (ไม่ต้อง spoof device-id)
  ├── IntelFramebuffer      — MMIO register port layer (จาก Linux i915) + FakeID translateAddress()
  ├── MyIntelAccelerator    — IOAccelerator family, surface manager (sid→GEM backing)
  ├── MyIntelFramebuffer    — IOFramebuffer subclass (modes, EDID, backlight RPL_BLC_PWM_CTL 0xC8250)
  ├── MyIntelRing           — ring buffer / execlist submission (RCS/BCS/VCS) + PPGTT 4-level
  ├── MyIntelGEMBuffer      — Graphics Execution Manager
  ├── MyIntelMedia          — VDBOX/VEBOX detection, clock gating
  ├── MyIntelVCSCommand     — MFX command encoders (Gen11 vs Gen12 bit fields!)
  ├── MyIntelVCSClient      — VCS user client
  └── MyIntelGPUClient      — IOUserClient dispatch (selector 2 WriteGEM, 5 Read, 6 ReadMMIO...)
         ↕ userspace
   libmyintelvcs.dylib (mvcs_* API) → mvcs_bench / vdbox_player / play_vdbox.sh
```

### Data flow ของ GPU execute (พิสูจน์แล้ว v2.0.28+)
```
client → ring → ELSP → PPGTT 4-level → GPU execute → IRQ → readback
```

### Userspace Bridge API (`libmyintelvcs.dylib`) — contract final
| Function | หน้าที่ |
|---|---|
| `mvcs_open/close` | connection lifecycle |
| `mvcs_gem_create/map/unmap/destroy` | GEM zero-copy CPU↔GPU |
| `mvcs_submit` | raw VCS ring dwords |
| `mvcs_decode_h264` | H.264 Annex-B → NV12 frame (**2.5ms @FHD**) |
| `mvcs_frame_release` | DPB pooled bookmark |
| `mvcs_get_status/get_context/last_error` | diagnostics |

Contract: `myintelvcs_bridge.h` (-2 = timeout, frame untouched) · Benchmark gate: fps≥327 & fail=0 (`./bench-regression.sh`) · One-command playback: `./play_vdbox.sh movie.mp4`

---

## 5. Timeline / Milestones

| วันที่ | เหตุการณ์ |
|---|---|
| 2026-06-24 | Release แรก v1.0.0-20260624-28977e2 (VM/QEMU setup era) |
| 06-25 → 07-20 | Genesis era: Phase 1-5 (PCI, BAR 3-strategy, FakeID translate, GGTT) · EFI บน Acer |
| 07-28 | เริ่ม session era ใหม่ (OpenCode) |
| 08-01 | วิจัย VRAM 4GiB (uint32 wrap hypothesis, REPORT-4GB-RESEARCH) |
| 08-07/08 | Phase 1-3 PCI/MMIO/interrupts · IOUserClient bridge + Oracle approve · **gem_test PASS (AllocGEM+MapGTT)** |
| 08-10 | CSB HWSP index fix (2.0.12) → GT reset (2.0.13) → EXECLIST_STATUS decode (2.0.14) |
| 08-11 | **Root cause PPGTT 4-level ≠ Gen12 → fix 5 จุด (2.0.26) → 🎉 GPU EXECUTE สำเร็จ (2.0.28)** |
| 08-13 | 🎉 **F10 VALIDATE 100%** (2.0.142) · BCS0 merge · ReadMMIO sel6 PASS (2.0.185) |
| 08-14 | Phase 2.3 Backlight + Phase 3 Gen12 interrupts/HPD (2.0.200) · Phase 4.1 ProgramPlane PASS บน HW |
| 08-15 | Root cause cache = **CFBundleVersion 4 ส่วน** · myintelfb=1 hang ที่ AUDIO LOOP (ไม่ใช่ GPU!) |
| 08-16 | ⛔ **กฎเหล็ก standalone-only**, ลบ OC injection (2.0.225) · System Profiler ยืนยัน GPU ทำงานเต็มรูปแบบ |
| 08-17 | 🎉 **YouTube 1080p FHD สำเร็จ** · Phase 6c IOAccelerator deployed (2.0.231) |
| 08-20 | Phase 8 Custom VCS Pipeline + Security (2.0.244) |
| 08-21 | Phase 9 GEM user-space + player bridge (2.0.249) · Phase 11 GGTT write path OPEN (2.0.263) |
| 08-22 | Phase 10 VDBOX Harness FHD Proof (harness 5647dbe) |
| 08-23 | E3a/E3c + Metal Phase 1 + VDBOX bridge root cause (หา 3 วัน) (2.0.297) · 🚀 **363 FPS FHD realtime** |
| 08-24 | Mission C Phase 4a design draft (DisplayPipe architecture) |
| 08-25 | **PROOF.md: 406 FPS** (engineReset per-frame fix), multi-client · BCS-DOSSIER |
| 08-26 | EDID fix 0xFE→0xFC (f3594e0) — "Unknown Display" |
| 09-03 | Windows session: CI **2.0.483** ออก (10 device-id) · Metal toolchain 6.2 พิสูจน์บน Windows · **NEXT: M2C-K** |
| 09-07 | Mac ล่ม → setup ใหม่ · kext **2.0.1** deploy @ L/E (เก่ากว่า CI) · ICC "Unknown Display-B533C716" สร้าง 23:41 |

---

## 6. VDBOX / BCS — สถานะ subsystem

### VDBOX (H.264 decode) — ✅ พิสูจน์แล้ว
- **406 FPS FHD, fail=0, 450/450 frames** (engineReset per-frame fix เป็นตัวปลดล็อก)
- YouTube 1080p + multi-client decode ผ่าน
- Profile: 23.5 ms/frame เดิม → 2.5ms @FHD (mvcs_decode_h264)
- Boot-arg: engineReset ทุก first-submit + after-timeout (ห้าม reset ทุกเฟรม — กลืน in-flight batch = hang ~1%)
- Adaptive wait: floor 12ms / ceiling 240ms — return -2 ไม่ success หลอก

### BCS (blit ring) — ⛔ ยังติด "queued-never-dispatched"
- falsified 10 hypotheses แล้ว · เปิด leads ต่อไป:
  - **L1** BATCH+BB_START linkage
  - **L2** GDRST sanitize (H#11: sanitize OK แต่ context image REJECTED)
  - **L3** class matrix
  - **L4** LRC byte-diff vs gen12_xcs_offsets ← **step ถัดไป** (เจอ HWS_PGA = base+0x80)
- Circuit breaker (71f0ea5): 50 emits ไม่ถูก consume → latch execlists OFF + fallback pure CPU-copy

### กฎ ring/head (แพงสุด)
- Ring head ที่ถูกต้อง = **context image head slot** (ไม่ใช่ MMIO RING_HEAD) + mask `head &= (size-1)`
- Indirect BB page math: img+512*4 / img+512*6 (img+512*2 = STATE page — clobber แล้ว hang)

---

## 7. MISSION-D: M2C-K — ขั้นถัดไป (Metal device จริง)

**เป้า:** `MTLCreateSystemDefaultDevice` ≠ nil → ได้ MTLDevice จริง → WindowServer ใช้ HW compositing (WS CPU 27% → <15%)

- แก้ kext ให้ **sel=9** (set API name) + **sel=2** (device caps 600B, dlsym name @+0x48) ตอบถูกต้อง
- เพื่อให้ `IOAccelDeviceCreateWithAPIProperty` คืน non-NULL
- **boot-arg-gated**: `myaccelname=` (default `IOAccelSharedGetConnect`)
- ต้องทำบน macOS (deploy + reboot):

```
STEP 1: deploy kext v2.0.483 (+ M2C-K patch) ลง L/E (5-step IRON RULES)
STEP 2: reboot ด้วย boot-arg myaccelname=IOAccelSharedGetConnect
STEP 3: MTLCreateSystemDefaultDevice != nil → M2C DONE → ไป M2D
        crash → iterate ชื่อ/field (+0x00/+0x08/+0x38/+0x40, ปัจจุบัน 0)
        สงสัย → reboot ไม่มี arg / rollback L/E
STEP 4: verify system_profiler SPDisplaysDataType แสดง renderer เรา (ไม่ใช่ AGXMetalA12)
```

### Metal toolchain (ทำได้บน Windows — พิสูจน์แล้ว)
- `metal.exe` 32023.850 รันบน Windows (MetalToolchain6.2 portable 1.8GB)
- `.metal → AIR → .metallib` ผ่านครบ (valid MTLB header) — **compile shader ได้บน Windows**
- ⚠️ `intelgpu-nt.exe` backend รองรับแค่ KBL(Gen9)/ICL(Gen11) — **ไม่รองรับ Gen12** → ไม่ใช่ปัญหา: macOS runtime (Gen12 IGC) แปลง AIR→Xe ISA ตอนรันไทม์

### 7.1 VDA / Metal Bundle Linkage — ⛔ "Failed to find bundle for accelerator bundle named: MyIntelGPUMTLDriver" (2026-09-08)

**อาการ:** VDA/VideoToolBox หา Metal Driver Bundle ไม่เจอ → `VDADecoderCreateInstance` คืน -12473 (`kVDADecoderFailedToCreateInstanceErr`) → AppleGVA hw decode pipeline ไม่ทำงาน ทั้งที่ MyIntelAccelerator ประกาศชื่อไว้

**ตรวจแล้ว (live):**
1. ✅ kext ตั้งค่า runtime properties ถูกต้อง (MyIntelAccelerator.cpp):
   - `MetalPluginName = MyIntelGPUMTLDriver` · `MetalPluginClassName = MyIntelGPUMTLDevice`
   - `IOGVACodec = "Gen12HP"` · `IOGVAH264/HEVC/AV1/VP9 Decode="1"` · `H264/HEVC Encode="1"`
   - `IOGLBundleName = MyIntelGPUGLDriver` (GL — ยังไม่มี bundle เช่นกัน)
2. ✅ bundle `MyIntelGPUMTLDriver.bundle` มีอยู่จริงใน source (Info.plist ถูก: id `com.custom.MyIntelGPUMTLDriver`, NSPrincipalClass `MyIntelGPUMTLDevice`)
3. ❌ **แต่ bundle ไม่ถูกติดตั้งเลย**:
   - ไม่ฝังใน `/Library/Extensions/MyIntelGPU.kext/Contents/PlugIns/` ← ตำแหน่งมาตรฐาน Apple ใช้ (AppleIntel*.kext/Contents/PlugIns/*MTLDriver.bundle)
   - ไม่มี standalone ใน `/Library/Extensions` หรือ `/System/Library/Extensions`
   - source kext 2.0.476 (CI) **ก็ไม่มี PlugIns** → CI build ไม่ได้ ship bundle ไปด้วย
4. ❌ `MyIntelGPUGLDriver.bundle` (IOGLBundleName ชี้ไป) — ไม่มีที่ไหนเลย

**สาเหตุ:** `MetalPluginClassName`/`MetalPluginName` ถูกประกาศไว้ใน IORegistry แต่ bundle ตัวจริงไม่ได้ถูก deploy เข้าระบบ → CoreVideo/AppleGVA มองหา bundle ตามชื่อไม่เจอ → สร้าง HW decoder instance ไม่ได้

**แนวทางแก้ (ตามที่ user วาง):**
- ฝัง `MyIntelGPUMTLDriver.bundle` ไว้ที่ `MyIntelGPU.kext/Contents/PlugIns/` (เลียนแบบ AppleIntel*.kext layout) → deploy kext ใหม่ (5-step IRON RULES) → reboot → verify MetalPluginClassName โหลดได้ + VDADecoderCreateInstance ≠ -12473
- ตรวจด้วย: `defaults read /Library/Preferences/com.apple.VideoDecoder` หรือ `vda_decoder` probe; IORegistry `MyIntelAccelerator` มี `IOGVACodec` = Gen12HP + `MetalPluginClassName` ครบ
- ยังต้องมี `MTLIOAccelDevice` path (M2C-K sel=2/9 + `IOAccelDeviceCreateWithAPIProperty` ≠ nil) ให้ Metal device เกิดก่อน — VDA decode ต่อจาก Metal device

---

## 8. กฎเหล็ก (IRON RULES — ห้ามละเมิดเด็ดขาด)

### Deploy/OS level
1. **Standalone เท่านั้น** — โหลดจาก `/Library/Extensions` · **ห้าม OC injection** · ห้าม DeviceProperties spoof GPU node (kext match PCI class `0x03000000` เอง)
2. **Deploy 5 ขั้นครบทุกครั้ง**: `cp → chown root:wheel → chmod 755 → kmutil install --volume-root / → ตรวจ auxKC ชี้ L/E`
3. **ห้าม reboot ถ้ายังไม่ deploy L/E** — test-load แล้ว reboot = auxKC stage ผิด path (แก้ sqlite DELETE `kext_load_history_v3` + `kmutil clear-staging`)
4. `kextcache -i /` **fail เงียบ** — ใช้ `kmutil install --volume-root /` เท่านั้น
5. `kmutil reset` ไม่มีใน OS นี้ — ใช้ `kmutil clear-staging`
6. **CFBundleVersion ต้อง 4 ส่วน** ไม่งั้น cache ไม่ rebuild
7. ตรวจ spelling boot-arg เสมอ (เคย `yintelfb` แทน `myintelfb` หลอนทั้ง session) · ห้าม `+` คั่น args
8. "จอค้าง" ≠ kext ตายเสมอ — เช็ค log/NVRAM marker/shutdown report (เคย hang ที่ audio loop)
9. ห้ามแตะ backup EFI — เส้นทางกู้คืน
10. Rollback ที่พิสูจน์: `rollback-3gb` (caa54b76, GUI ทำงาน)
11. **auxKC gotcha**: kmutil install + rm auxKC ทั้ง 2 path + clear-staging ทุกครั้ง
12. **Never hot-reload kext** — แก้จริงต้อง reboot

### Kernel code level
13. No C++ stdlib/exceptions/RTTI — `IOMalloc/IOFree`, `OSDynamicCast`, `OSDeclareDefaultStructors`
14. No `-undefined dynamic_lookup`; link ผ่าน OSBundleLibraries
15. `VblankEvent()` implement local (macOS 15 ลบจาก IOGraphicsFamily) — verify `nm → T _VblankEvent`
16. ไม่ใส่ OSBundleRequired ใน Info.plist · `-fno-stack-protector` (verify: no `__stack_chk`)
17. Binary budget ~105KB (>128KB = bloat)
18. MMIO ผ่าน `OSReadLittle32/OSWriteLittle32` (Intel LE)
19. **ห้ามค่าหลอก** (user rule 2026-08-16): ทุกอย่างต้องมาจาก HW probe/log จริง
20. No os_log → ใช้ IOLog เท่านั้น (unsigned kext fail prelink/auxKC = BOOT FAILURE)
21. version ล่าสุดตาม skill = **2.0.297** (แต่ CI ล่าสุด 2.0.483 — ใช้ CI build เป็นหลัก)

### ระบบ
22. ⛔ CpuTopologyRebuild.kext = boot panic ห้ามใช้
23. M-SPARE EFI args clean rule · EFI "EFI 1" = เส้นทางกู้คืน
24. หลีกเลี่ยง kill -9 ระหว่าง decode (XNU pmap panic risk) — ปิดด้วย mvcs_close

---

## 9. Debugging Encyclopedia (บทเรียนแพงสุด)

| อาการ | Root cause | Fix |
|---|---|---|
| GPU hang ที่ BB_START | PPGTT 4-level ไม่เข้ากัน Gen12 | fix 5 จุด (4d041a3) → milestone 2.0.28 |
| READBACK=0 | PPGTT ไม่เคย map หน้า GEM | per-page phys PTE + TLB invalidate + MI_FLUSH_DW (1c3cfaf) |
| MI_FLUSH_DW เขียนพลาด | Type bit 29 ผิด; DAT(GGTT)=DW1 bit2 ไม่ใช่ DW0 | แก้ encoding (7daa167) |
| MFX Gen12 headers ผิด | BspBuf/QmState/AvcPicIdState 3 fields | แก้ (1bf4e17) + G12 slice wiring (2c97725) |
| Hang ~1% silent corruption | engineReset ทุกเฟรมกลืน in-flight batch | reset first-submit+after-timeout → 406fps fail=0 |
| VDBOX bridge หา 3 วัน | (WORK_MEMORY-snapshot §08-23) | 2.0.297 |
| BCS queued-never-dispatched | ยังเปิด — falsified 10 hypotheses | L1/L2/L3/L4 (ดู §6) |
| Cache ไม่ rebuild | CFBundleVersion ไม่ครบ 4 ส่วน | bump version format |
| kext หายหลัง reboot | auxKC stage ผิด path (KextPolicy pin) | sqlite DELETE kext_load_history_v3 + clear-staging |
| จอดำ (display offline) | FB ชน IOMatchCategory กับ .Display_boot | (WORK_MEMORY) unload/แยก category |
| SIGBUS present_update | VRAM report ผิด (CoreDisplay เขียน fb) | fVRAMDescriptor=64MB (55a2a432) |
| "Unknown Display" | EDID 0xFE แทน 0xFC + ใช้ DDC path แทน | fix byte 111=0xFC + checksum 0x03 (f3594e0) |
| GUI พังที่ 4GiB VRAM | hypothesis: macOS อ่าน VRAM uint32 → wrap 2³² | ใช้ 3072MB / 4095MB probe (REPORT-4GB-RESEARCH) |

### Tools inventory (ใน source)
`accel_steps alloc|attach|start|batchblit|pad`, `accel_probe`, `iosurface_scan`, `[LRCXCS]` kext dump, `mvcs_bench`, `gem_test`, `ggtt_diag`, harness dmcheck/harness, `mhw_vdbox_mfx_hwcmd_g12_X.h` (701KB Gen12 MFX header)

---

## 10. VRAM / GEM

- Stolen memory ~8MB contiguous จาก BIOS (PTE[0..2024]) · Aperture 256MB max mappable
- VRAM pool รายงานถึง **3072MB** (`myintelvram=1`) — ล่าสุดย้ายไปอ่านจาก BAR2 ตรงๆ (build hash E1DABD2F)
- วิจัย 4GiB: HASHES.txt — `caa54b76`=3GiB safe, `61c02b56`=4095MB probe, `4180ac8d`=4GiB-clean
- auxKC rebuild ล้มเหลวเงียบเมื่อ KDK ไม่มี (บทเรียน REPORT-4GB-RESEARCH)

---

## 11. Lineage ของ Source + สิ่งที่เก็บ/กู้ได้ (สำคัญสำหรับการลบ Ventoy)

| สำเนา | ตำแหน่ง (ก่อนล่ม) | สถานะ |
|---|---|---|
| **GitHub main (authoritative)** | github.com/pongpan-bk/IntelReviveGPU-Gen-10-12-on-Hackintosh | ✅ มีตลอด — ตัวจริงสุดท้าย `e8bc0d2` (2.0.483) |
| **บิดาแท้ (Genesis)** | Desktop/IntelReviveGPU (Gen 1012+) on Hackintosh | 🧬 440+ commits, git history ครบ + WORK_MEMORY + MISSION-D + MTLDriver.bundle + Source-Code/ |
| Main-Project (cwd) | Documents/Main-Project/IntelReviveGPU-Gen-10-12-on-Hackintosh-main | เครื่องนี้ทำงานอยู่ — v2.0.1 deploy |
| Ventoy USB | /Volumes/Ventoy/{งาน,สำคัญที่สุด,linux,Default Project,all} | กู้ข้อมูล — หลังรวมไฟล์นี้ + source ไป all-in-1 แล้วลบได้ |
| backups EFI | Ventoy/สำคัญที่สุด + all/ | EFI OC config (ต้องเก็บก่อนลบ!) |

**ของที่ยังอยู่บน Ventoy (ต้องเลื่อนไปก่อนลบ):**
- `/Volumes/Ventoy/งาน/` — linux 7.0.0 source 254MB (i915 register map ต้นทาง)
- `/Volumes/Ventoy/สำคัญที่สุด/` — EFI backups + kext zips
- `/Volumes/Ventoy/linux/` — kernel + DESKTOP-DD7B0I5.txt + EFI
- `/Volumes/Ventoy/Default Project/` — Windows VGA driver + iGPU_Report.txt + WORK_MEMORY_2026-08-07.md
- `/Volumes/Ventoy/all/` — config.plist, bootlog.txt 227KB, MyIntelGPU_Actual_System_Values.txt 3MB, gem_test/ggtt_diag, pcidevices, MyIntelGPU-2.0.220-Beta, deploy scripts

**เส้นทาง GitHub:** `https://github.com/pongpan-bk/IntelReviveGPU-Gen-10-12-on-Hackintosh` (origin/main = e8bc0d2, 483 commits)
⚠️ token ใน origin URL (`ghp_...`) ควร revoke + re-auth ใหม่ (MISSION-D §8)

---

## 12. ถัดไป (Action Plan หลังกู้ข้อมูล)

1. **เขียน master นี้เสร็จ + copy source ไป `all-in-1-myinteligpu.kext-work/source/`** ← กำลังทำ
2. **Deploy CI build ล่าสุด (2.0.483)** — มี device-id ครบ 10 ตัว + EDID fix — แทนที่ 2.0.1 (ซึ่งเก่า)
3. **M2C-K patch** (MISSION-D) → boot-arg `myaccelname=` → MTLDevice จริง → WindowServer accel
4. **เปิด display path ทีละขั้น** (myintelfbinit → myintelfb → myintelaccel) ด้วยบทเรียน §2.4/§9
5. **BCS L4**: byte-diff context image vs gen12_xcs_offsets (จบ BCS queued-never-dispatched)
6. Verify จอขึ้นชื่อ `KD156N2930A02` (ไม่ใช่ Unknown Display) — หลักฐาน = dmesg EDID injected

### มาตรวัดความสำเร็จ
- ✅ `system_profiler SPDisplaysDataType` แสดง renderer เรา (ไม่ใช่ AGXMetalA12)
- ✅ WS CPU < 15% (composition บน GPU ไม่ใช่ CPU)
- ✅ Display ชื่อถูก + ไม่มี CoreDisplay error (PASS ระดับ 2.0.445)
- ✅ VDBOX 406fps fail=0 หลัง reboot (CI build)

---

## 13. Glossary เร็ว
- **GGTT** Global Graphics Translation Table · **PPGTT** per-process GGTT · **ELSP** Execlist Submission Port
- **GUC/HuC/CSR** firmware (Windows driver มี GUC 1883 tokens — power mgmt/execlist)
- **RCS/BCS/VCS/VEBOX** render/blit/video-decode/video-enhancement engines
- **VDBOX/VDENC** video decode/encode hardware · **MFX** Media Framework (Gen12 headers)
- **auxKC** Auxiliary Kernel Collection (kext จาก L/E ถูกฝังที่นี่)
- **XeLP/RPL** Gen12.2 platform (Raptor Lake) — ห้าม spoof เป็น Gen11 (HARD RULE)
- **WOPCM/GUC_STATUS** register ยืนยัน Gen12 (หลักฐานห้าม Gen11)

---

*MASTER-KNOWLEDGE · สร้าง 2026-09-08 · รวบรวมจากทุกแหล่งก่อนลบ Ventoy · Sisyphus (OpenCode)*