# MyIntelGPU — Native Intel Coffee Lake Gen9 iGPU Driver for macOS

Standalone macOS IOKit kernel extension that drives Intel Coffee Lake (Gen9)
integrated GPUs (UHD 615/620/630) natively — hardware Apple dropped driver support
for after macOS Ventura.

**No Lilu. No WhateverGreen. No OpenCore injection. No fake device ID.**  
Pure native Gen9 driver loaded from `/Library/Extensions`.

> **Version:** 4.4.x · **Bundle ID:** `com.pongpan-bk.MyIntelGPU`  
> **Tested on:** macOS Sequoia 15.x

## Supported Hardware

| PCI ID | GPU | SKU |
|--------|-----|-----|
| `8086:3E91` | Intel UHD 620 | Coffee Lake-U 14W GT2 |
| `8086:3E92` | Intel UHD 620 | Coffee Lake-U 15W GT2 |
| `8086:3E90` | Intel UHD 615 | Coffee Lake-U GT2 |
| `8086:3E96` | Intel UHD 620 | Coffee Lake-U GT2 |
| `8086:3E98` | Intel UHD 620 | Coffee Lake-U GT2 |
| `8086:3E9B` | Intel UHD 620 | Coffee Lake-U 23W GT2 |
| `8086:3EA0` | Intel UHD 620 | Whiskey Lake-U GT2 |
| `8086:9BC4` | Intel UHD 630 | Coffee Lake-S GT3 |
| `8086:9BC8` | Intel UHD 630 | Comet Lake-S GT3 |

## Status

| Layer | Status |
|---|---|
| PCI setup / MMIO (BAR0 regs / BAR2 aperture) | ✅ |
| Display / IOFramebuffer (1920×1080 FHD, backlight) | ✅ |
| RCS / BCS / VCS rings + execlists submission | ✅ |
| PPGTT identity mapping (GEM buffers) | ✅ |
| Media engines (VDBOX / VEBOX detect via `0x138010`) | ✅ |
| **H.264 hardware decode (VDBOX writes NV12 pixels)** | ✅ proven |
| ExecBatch / WaitBatch / RingStatus client API | ✅ |
| GPU render (Metal) | ⏳ opt-in (`myintelaccel=1`), not yet proven |

## Hardware Decode Proof

```
[READBACK] probe=20480 poison(0xA5)=2091 zero=0 other=18389
[READBACK] => HW เขียนพิกเซลจริง! decode สำเร็จ
[SRC-COHERENCE] aperture vs CPU: MATCH ✅
[PIPELINE] scratch 1531 bytes ← VDBOX เขียน!
[DUMP] wrote 18432 bytes → NV12 128x96
```

## Build

```bash
make KERNEL_SDK_DIR=~/MacKernelSDK clean
make KERNEL_SDK_DIR=~/MacKernelSDK -j$(sysctl -n hw.ncpu)
```

Requires: Xcode Command Line Tools + [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)

## Deploy

```bash
sudo rm -rf /Library/Extensions/MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo codesign -s - --force /Library/Extensions/MyIntelGPU.kext
sudo kmutil install --volume-root /
# reboot, then verify:
kextstat | grep pongpan
```

> ⚠️ Do NOT inject via OpenCore `Kernel:Add` or spoof device-id in
> `DeviceProperties` — the kext matches the real PCI class itself.

## Boot-args

| Flag | Effect |
|---|---|
| `myintelfb=1` | Let `MyIntelFramebuffer` own the real panel |
| `myinteldisplaypipe=1` | Attach the display pipe |
| `myintelaccel=1` | Opt-in IOAccelerator path — **experimental** |

> ⚠️ `myintelaccel=1` binds the Metal/GL plugin against Gen9 registers.
> Keep a rescue EFI before testing.

## Architecture

```
Userspace (harness / VideoToolbox-bypass)
    │ IOConnectCall
    ▼
MyIntelVCSClient (IOUserClient) ── GEM buffers, PPGTT identity map
    ▼
MyIntelRing (execlists ELSP) ── BB_START → flush → USER_INTERRUPT
    ▼
VDBOX / VCS0 engine (MFX command stream, Gen9)
```

Source layout:

| File | Role |
|------|------|
| `MyIntelGPU.cpp/hpp` | Core driver — PCI, MMIO, rings, power |
| `IntelFramebuffer.cpp/hpp` | Display pipeline, interrupts |
| `MyIntelFramebuffer.cpp/hpp` | IOFramebuffer subclass |
| `MyIntelAccelerator.cpp/hpp` | IOAccelerator binding |
| `MyIntelMedia.cpp/hpp` | Media engine detection |
| `MyIntelRing.cpp/hpp` | Ring buffer / execlists |
| `MyIntelGEMBuffer.cpp/hpp` | GEM buffer management |
| `MyIntelVCSClient.cpp/hpp` | IOUserClient (GEM/ring selectors) |
| `MyIntelVCSCommand.cpp/h` | Gen9 MFX command builders |
| `MyIntelEDIDParser.cpp/hpp` | EDID parsing |
| `MyIntelGPUClient.cpp/hpp` | Client API selectors 0–24 |

## Client API (IOUserClient Selectors)

| Sel | Name | Purpose |
|---|---|---|
| 0–11 | legacy GEM/ring/readback | buffer mgmt, submit proof batches |
| 12 | `ExecBatch` | submit caller-authored command stream |
| 13 | `WaitBatch` | poll HWSP seqno fence |
| 14 | `RingStatus` | head/tail/space/pending telemetry |
| 20–24 | accelerator step machine | IOAccelerator adoption path |

## License & Disclaimer

Research / experimental driver. Use at your own risk — kernel extensions can
panic your machine. Always keep a bootable rescue path.
