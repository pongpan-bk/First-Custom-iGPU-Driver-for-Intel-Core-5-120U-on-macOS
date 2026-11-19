Powered By Opencode-ai 

<img width="1280" height="721" alt="opencode-c8152c4bb9ae" src="https://github.com/user-attachments/assets/3ff139b6-5e20-4621-960d-748a194d95fa" />




<div align="center">


# ⚡ MyIntelGPU — Honest-Identity Intel iGPU Driver for macOS

### Custom macOS kernel extension for the Intel Core 5 120U onboard GPU (Device ID `0xA7AC`, Gen 12 / Raptor Lake-U)

[![Build MyIntelGPU.kext](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml/badge.svg)](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml)
[![Target OS: macOS 13+](https://img.shields.io/badge/macOS-13%2B%20(x86_64)-blue.svg)](https://www.apple.com/macos/)
[![Binary size: ≤230 KB](https://img.shields.io/badge/binary-%E2%89%A4230%20KB-green.svg)](Makefile)
[![OpenCore: Native EFI](https://img.shields.io/badge/OpenCore-Pure%20Kext%20(No%20Bundle)-purple.svg)](#%EF%B8%8F-option-a-opencore-efi-injection-recommended--pure-kext)
[![Status: Experimental](https://img.shields.io/badge/status-experimental-orange.svg)](#project-status)

**A hardware-level driver ported from Linux i915 register knowledge, engineered to expose the onboard graphics of a CPU Apple never shipped with a Mac — using its real PCI Device ID, with no spoofing, no FakeID, and zero custom userspace bundles.**

</div>

---

## 📖 Overview

**MyIntelGPU** (`Gen12-5.0`) is a standalone, pure-native kernel extension (kext) that drives the onboard GPU of the **Intel Core 5 120U** (PCI Device `0xA7AC`, Vendor `0x8086`) on macOS. It was built by porting register-level and bus-control knowledge from the Linux `i915` driver stack and re-implementing the macOS side from scratch — IOService matching, GGTT/GSM initialisation, GEM buffer management, command streamers, interrupt plumbing, and display diagnostics.

### 🛑 No spoofing. No FakeID. No custom PlugIns bundle.

The defining property of this driver is **honest identity** paired with **pure OpenCore compatibility**:
* **Real PCI matching:** `Info.plist` matches the real PCI Device ID via standard IOKit personality matching (`IOPCIMatch = 0xa7ac8086`, `IOProbeScore = 20000`) and claims the device directly — no FakeID injection and no SMBIOS spoofing to masquerade as another iGPU.
* **Bundle-free native userspace hook:** Instead of shipping an embedded custom `.bundle` under `Contents/PlugIns/`, the personality binds `IOAccelerator` directly to Apple's stock Ice Lake userspace graphics stack (`AppleIntelICLGraphicsMTLDriver`, `AppleIntelICLGraphicsGLDriver`, `AppleIntelICLGraphicsVADriver`, `AppleIntelICLLPGraphicsVAME`). Because those bundles already reside in `/System/Library/Extensions`, **MyIntelGPU.kext can be injected cleanly from OpenCore (`EFI/OC/Kexts`)** without touching `/Library/Extensions` if desired.

<p align="center">
  <img width="1920" alt="MyIntelGPU loaded on macOS — System Information / boot log" src="https://github.com/user-attachments/assets/a070a163-16f7-4861-9894-cf361eb4cc74" />
  <img width="1920" alt="MyIntelGPU graphics acceleration / decode pipeline proof" src="https://github.com/user-attachments/assets/d9e2b294-c212-4069-9b6b-522bc7d1e252" />
</p>

---

## 🚦 Project Status

> ⚠️ **Experimental software.** This is a hobbyist kernel-level driver for hardware Apple never implemented. It can panic your machine, corrupt state, or simply not load. Read the [disclaimer](#%EF%B8%8F-safety--disclaimer) before installing.

| Area | State |
|---|---|
| Kext loads with honest Device ID matching (`0xa7ac8086`) via OpenCore or `/L/E` | ✅ Working |
| GGTT/GSM init, GEM buffer allocation and binding | ✅ Working |
| RCS / BCS command streamers via Execlists (LRC) | ✅ Working |
| Gen11+ GT interrupt plumbing & VBlank bridge | ✅ Working |
| Display-state framebuffer snapshot, EDID parsing & diagnostics | ✅ Working |
| Hardware video decode (VDBOX / VCS ring, userspace MFX harness) | 🔶 In progress — thin kernel bridge + `VCSMediaHarness` decodes H.264 |
| HEVC / VP9 / AV1 decode | 🔶 Planned (`myintel_hevc_parse.h` scaffolding in harness) |
| Metal / GL / VA acceleration hook | 🔶 Experimental — bound directly to stock `AppleIntelICLGraphics*` (`MTLIGAccelDevice`) |
| OpenCore EFI ready-to-merge suite (`make opencore`) | ✅ Working — CI builds Kext + ACPI SSDT + `config-MyIntelGPU.plist` |
| End-user install via GUI installer (`.pkg`) | ✅ `make pkg` + CI artifact; see [Installation](#-installation-for-owners-of-the-same-model) |

**Tested on:** Intel Core 5 120U (Raptor Lake-U) laptop, macOS 13+ (`MinKernel 22.0.0`, x86_64). Other CPU models with the same Gen 12 iGPU root are untested.

---
test on mac os 15

## 🧠 Architectural Deep-Dive & Notable Fixes

### 1. The silicon reality — Alder Lake-P roots

Although the Core 5 120U is marketed as Raptor Lake-U, its GPU subsystem is on the **Alder Lake-P (Gen 12)** architectural root. The driver therefore reuses Gen 12 port/connector tables and the Gen 12 command set unchanged, which is what makes an honest-identity match practical at all.

### 2. Thin kernel bridge & the `0x71000013` bit-shift fix

The kernel extension acts strictly as a thin macOS↔GPU bridge (display + GEM/ring/VCS submit), while codec command builders live in userspace (`VCSMediaHarness/`). Video decode commands are submitted straight to the **VCS ring (VDBOX)**. A 64-bit address must travel through the command buffer unshifted — yet an earlier address mask used a `>> 6` shift that corrupted the Luma and scratch-buffer addresses (`kScrIntra` / `kScrDeblock`).

The fix clears the low 6 bits instead, pinning the address to the 64-byte alignment the Gen 12 architecture requires:

```cpp
// Clear only the low 6 bits to pin the Gen 12 GPU address to 64-byte bounds
static constexpr uint64_t kMhwAddressMask = ~0x3FULL;

void writeVcsRingAddress64(uint32_t *commandBuffer, size_t dwIndex, uint64_t gpuAddress) {
    uint64_t alignedAddress = gpuAddress & kMhwAddressMask;
    commandBuffer[dwIndex]     = (uint32_t)(alignedAddress & 0xFFFFFFFFu); // Low DWORD
    commandBuffer[dwIndex + 1] = (uint32_t)(alignedAddress >> 32);         // High DWORD
}
