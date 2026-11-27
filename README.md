Powered By Opencode-ai

<img width="1280" height="721" alt="opencode-c8152c4bb9ae" src="https://github.com/user-attachments/assets/3ff139b6-5e20-4621-960d-748a194d95fa" />

<div align="center">

# ⚡ MyIntelGPU — Thin-Bridge Intel Gen 12 iGPU Driver for macOS

### Pure-native macOS kernel extension for the Intel Core 5 120U onboard GPU (Device ID `0xA7AC`, Xe-LP / Raptor Lake-U)

[![Build MyIntelGPU Thin Bridge](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml/badge.svg)](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml)
[![Target OS: macOS 13–15](https://img.shields.io/badge/macOS-13--15%20Sequoia%20(x86__64)-blue.svg)](https://www.apple.com/macos/)
[![Binary size: 207 KB](https://img.shields.io/badge/binary-207%20KB%20(%E2%89%A4220%20KB)-brightgreen.svg)](Makefile)
[![Architecture: Thin Kernel Bridge](https://img.shields.io/badge/architecture-Thin%20Kernel%20Bridge%20(Zero%20PlugIns)-purple.svg)](#architectural-deep-dive--notable-fixes)
[![Status: Hardware Verified](https://img.shields.io/badge/VDBOX0%20H.264-bit--exact%20verified-success.svg)](#project-status)

**A clean, hardware-level macOS kernel bridge ported from Linux `i915` register knowledge — driving a Gen 12 Xe-LP iGPU Apple never supported, using its honest PCI Device ID (`0x8086:0xA7AC`), zero FakeID spoofing, and zero mock userspace bundles.**

</div>

---

## 📖 Overview

**MyIntelGPU** (`5.x` Thin-Bridge Edition) is a standalone, Lilu-free `IOService` / `IOPCIDevice` kernel extension that brings up the integrated GPU of the **Intel Core 5 120U** (`8086:A7AC`, 80 EU Iris Xe) on macOS 13 through **macOS 15 (Sequoia)**.

Rather than bloating kernel space with codec parsers or shipping stub Metal bundles that crash `WindowServer`, **MyIntelGPU** is engineered strictly as a **Thin Kernel Bridge ("Hardware Translator Pipe")**:

* **Honest PCI matching (`0xa7ac8086`):** Claims the real hardware directly with `IOProbeScore = 20000` — no FakeID, no WhateverGreen, and no SMBIOS GPU masquerading.
* **Zero `Contents/PlugIns` bloat:** Ships as a single stripped Mach-O kext binary (~207 KB) with `-flto` and `-dead_strip`, clean of `LC_DYLD_CHAINED_FIXUPS` and userspace symbol leaks.
* **Kernel handles DMA & rings; Userspace speaks Gen 12:** The kext manages ForceWake, BAR0/BAR2 MMIO, GGTT + 48-bit PPGTT page tables, zero-copy GEM buffers, Execlists (LRC) submission, and MSI interrupts — while Gen 12 MFX/HCP media command streams are built in userspace (`VCSMediaHarness`, custom FFmpeg backend, and `DYLD_INSERT_LIBRARIES` VideoToolbox shim).

<p align="center">
  <img width="1920" alt="MyIntelGPU loaded on macOS — System Information / boot log" src="https://github.com/user-attachments/assets/a070a163-16f7-4861-9894-cf361eb4cc74" />
  <img width="1920" alt="MyIntelGPU graphics acceleration / decode pipeline proof" src="https://github.com/user-attachments/assets/d9e2b294-c212-4069-9b6b-522bc7d1e252" />
</p>

---

## 🚦 Project Status

> ⚠️ **Experimental kernel software.** This driver talks directly to PCI MMIO, GGTT/PPGTT page tables, and GPU command streamers on hardware Apple never supported. Read [Safety & Disclaimer](#safety--disclaimer) before installing.

| Subsystem | Status | Notes (Verified on macOS 15.8.2 Sequoia) |
|---|---|---|
| Honest PCI ID matching (`0xa7ac8086`) | ✅ Working | Standalone `IOPCIDevice` match; no Lilu / WhateverGreen required |
| Clean Mach-O Kext ABI (`≤220 KB`) | ✅ Working | ~207 KB stripped `x86_64` binary; no `LC_DYLD_CHAINED_FIXUPS`, zero leaked userspace symbols |
| GGTT / 48-bit PPGTT & GEM DMA buffers | ✅ Working | Zero-copy `IOBufferMemoryDescriptor` mapping into userspace via `IOUserClient` |
| RCS / BCS / VCS0 / VCS2 Execlists (LRC) | ✅ Working | Hardware context submission + engine reset recovery; BCS validated 100/100 post-reboot |
| Gen11+ GT MSI interrupts & VBlank bridge | ✅ Working | Native IRQ routing + 15.3" FHD eDP EDID injection & HiDPI display scaling |
| **VDBOX0 H.264 Hardware Decode** | ✅ **Proven** | **Bit-exact** frame0 (`d88b9dc9…`) & 30-frame (`c28f37eb…`) MD5 parity vs Linux `i915` golden dump |
| Userspace FFmpeg & VT Interpose Bridge | ✅ Working | Hardware decode in custom players/FFmpeg + `DYLD_INSERT_LIBRARIES` VideoToolbox shim |
| VDBOX0 HEVC (HCP / P010) & VP9 decode | 🔶 In Progress | Userspace HCP command builders & SPS/PPS/slice parsers under active validation |
| 3D Desktop Compositing (Metal / GL) | ⚪ Intentionally Omitted | macOS lacks a Gen 12 Xe-LP EU shader compiler (`SWSB` ISA); mock bundles are excluded to keep `WindowServer` 100% stable |
| Automated CI (`macos-15-intel`) | ✅ Passing | Builds against pure Acidanthera `MacKernelSDK` in ~25s with automated ABI/symbol/size gates |

**Tested Hardware & OS:**
* **CPU / iGPU:** Intel Core 5 120U (Raptor Lake-U, `8086:A7AC` rev 04, 80 EU Iris Xe, subsystem `1025:192E`)
* **Display:** Built-in 15.3" eDP FHD (`1920x1080`) panel with native HiDPI scaling
* **macOS:** Verified on **macOS 15 (Sequoia, x86_64)**; `MinKernel 22.0.0` (macOS 13+)

---

## 🧠 Architectural Deep-Dive & Notable Fixes

### 1. Silicon reality — Alder Lake-P (Gen 12 Xe-LP) roots & why stock Metal cannot run
Although marketed under Raptor Lake-U, the Core 5 120U graphics block is built on Intel's **Alder Lake-P / Gen 12 Xe-LP** architecture. Apple ended Intel GPU support at Gen 11 (Ice Lake). Because Gen 12 replaced hardware scoreboarding with **Software Scoreboarding (`SWSB`)** and redesigned the EU instruction encoding, Apple's stock Gen 11 userspace stack (`AppleIntelICLGraphicsMTLDriver`) cannot execute shaders on `0xA7AC` silicon.

Instead of shipping fake Metal stubs that trigger `WindowServer` respawn loops, **MyIntelGPU** keeps desktop scanout clean and exposes the real silicon engines (VDBOX media decoders and BCS blitter) directly to userspace through `IOUserClient`.

### 2. Thin kernel bridge Architecture
```text
Userspace Apps (FFmpeg / myintel-play / DYLD VideoToolbox Shim)
    │  Gen12 MFX/HCP Annex-B bitstream & command builders (VCSMediaHarness)
    ▼
MyIntelVCSClient & MyIntelGPUClient (IOUserClient)
    │  Zero-copy GEM allocation, 48-bit PPGTT identity map, Fence wait
    ▼
MyIntelRing (Execlists ELSP submission)
    │  MI_BATCH_BUFFER_START → MI_FLUSH_DW → MI_USER_INTERRUPT
    ▼
Intel Gen 12 Silicon (8086:A7AC — VDBOX0 / VDBOX2 / BCS0)
```
Because codec state machines live entirely in userspace, fixing bitstream edge cases or adding new decoder frontends compiles in seconds — **zero kernel rebuilds and zero reboots required**.

### 3. The `0x71000013` 64-byte address alignment fix
In Gen 12 MFX/HCP pipe buffer commands (`MFX_PIPE_BUF_ADDR_STATE`), 64-bit canonical GPU virtual addresses must travel unshifted in two 32-bit DWORDs with the low 6 control/attributes bits masked cleanly. An early `>> 6` shift corrupted Luma and scratch-buffer pointers (`kScrIntra` / `kScrDeblock`). Clearing only bits `[5:0]` pins addresses to Gen 12's required 64-byte boundary:

```cpp
// Clear only the low 6 bits to align the Gen 12 GPU virtual address to 64-byte bounds
static constexpr uint64_t kMhwAddressMask = ~0x3FULL;

void writeVcsRingAddress64(uint32_t *commandBuffer, size_t dwIndex, uint64_t gpuAddress) {
    uint64_t alignedAddress = gpuAddress & kMhwAddressMask;
    commandBuffer[dwIndex]     = static_cast<uint32_t>(alignedAddress & 0xFFFFFFFFu); // Low DWORD
    commandBuffer[dwIndex + 1] = static_cast<uint32_t>(alignedAddress >> 32);         // High DWORD
}
```

---

## 🛠️ Build from Source

### Prerequisites
* Xcode Command Line Tools (`xcode-select --install`)
* [Acidanthera MacKernelSDK](https://github.com/acidanthera/MacKernelSDK) cloned to `~/MacKernelSDK` (or passed via `KERNEL_SDK_DIR`)

### Build Commands
```bash
git clone https://github.com/acidanthera/MacKernelSDK.git ~/MacKernelSDK
make KERNEL_SDK_DIR=~/MacKernelSDK -j$(sysctl -n hw.ncpu)
make version
```
Or download the prebuilt, ABI-verified `MyIntelGPU-thin-kext.zip` artifact directly from the latest green [GitHub Actions](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml) run.

---

## 📦 Installation

### Recommended on macOS 13–15: `/Library/Extensions` (AuxKC)
When `Info.plist` links IOKit families residing in macOS's `SystemKC` (such as `IOGraphicsFamily` / `IOSurface`), loading via `/Library/Extensions` allows `kmutil` to link symbols cleanly across Kernel Collections:

```bash
sudo rm -rf /Library/Extensions/MyIntelGPU.kext
sudo ditto MyIntelGPU.kext /Library/Extensions/MyIntelGPU.kext
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo kmutil install --volume-root / --update-all
```
*(Requires SIP disabled and `amfi_get_out_of_my_way=1` in boot-args when testing ad-hoc signed builds. Always keep a known-good OpenCore USB rescue stick before updating AuxKC.)*

---

## ⚠️ Safety & Disclaimer

This repository contains experimental, bare-metal kernel research code for unsupported Intel Gen 12 hardware on macOS. Incorrect MMIO writes, page-table faults, or kernel cache updates can cause kernel panics or boot loops. **Use strictly at your own risk and always maintain an external bootable OpenCore recovery USB.**
