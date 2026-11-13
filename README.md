<div align="center">

<em>"As long as you view macOS as Unix, you will only get what you've always got. I view macOS as Linux, and that's why I get what you see."</em>

# ⚡ MyIntelGPU — Honest-Identity Intel iGPU Driver for macOS

### Custom macOS kernel extension for the Intel Core 5 120U onboard GPU (Device ID `0xA7AC`, Gen 12 / Raptor Lake-U)

[![Build MyIntelGPU.kext](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml/badge.svg)](https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS/actions/workflows/build.yml)
[![Target OS: macOS 12+](https://img.shields.io/badge/macOS-12%2B%20(x86_64)-blue.svg)](https://www.apple.com/macos/)
[![Binary size: ≤220 KB](https://img.shields.io/badge/binary-%E2%89%A4220%20KB-green.svg)](Makefile)
[![Status: Experimental](https://img.shields.io/badge/status-experimental-orange.svg)](#project-status)

**A hardware-level driver ported from Linux i915 register knowledge, engineered to expose the onboard graphics of a CPU Apple never shipped with a Mac — using its real PCI Device ID, with no spoofing and no FakeID.**

</div>

---

## 📖 Overview

**MyIntelGPU** is a semi-standalone kernel extension (kext) that drives the onboard GPU of the **Intel Core 5 120U** (PCI Device `0xA7AC`, Vendor `0x8086`) on macOS. It was built by porting register-level and bus-control knowledge from the Linux `i915` driver stack and re-implementing the macOS side from scratch — IOService matching, GGTT/GSM initialisation, GEM buffer management, command streamers, interrupt plumbing and display diagnostics.

### 🛑 No spoofing. No FakeID.

The defining property of this driver is **honest identity**: the Info.plist matches the real PCI Device ID via standard IOKit personality matching (`IOPCIMatch = 0xa7ac8086`) and claims the device directly. The kext loads from `/Library/Extensions` with its real hardware identity — no FakeID injection, no SMBIOS spoofing to masquerade as another iGPU. That keeps the driver accurate to the silicon it actually controls.

<p align="center">
  <img width="1920" alt="MyIntelGPU loaded on macOS — System Information / boot log" src="https://github.com/user-attachments/assets/a070a163-16f7-4861-9894-cf361eb4cc74" />
  <img width="1920" alt="MyIntelGPU graphics acceleration / decode pipeline proof" src="https://github.com/user-attachments/assets/d9e2b294-c212-4069-9b6b-522bc7d1e252" />
</p>

---

## 🚦 Project Status

> ⚠️ **Experimental software.** This is a hobbyist kernel-level driver for hardware Apple never implemented. It can panic your machine, corrupt state, or simply not load. Read the [disclaimer](#-safety--disclaimer) before installing.

| Area | State |
|---|---|
| Kext loads with honest Device ID matching (IOKit personality matching) | ✅ Working |
| GGTT/GSM init, GEM buffer allocation and binding | ✅ Working |
| RCS / BCS command streamers via Execlists (LRC) | ✅ Working |
| Gen11+ GT interrupt plumbing | ✅ Working |
| Display-state framebuffer snapshot / diagnostics | ✅ Working |
| Hardware video decode (VDBOX / VCS ring, MFX command emission) | 🔶 In progress — userspace harness decodes H.264 with real MFX sequences |
| HEVC / VP9 / AV1 decode | 🔶 Planned |
| Metal acceleration | 🔶 Experimental — `MyIntelGPUMTLDriver.bundle` is embedded as a Metal plugin |
| End-user install via signed GUI installer (.pkg) | ✅ `make pkg` + CI artifact; see [Installation](#-installation-for-owners-of-the-same-model) |

**Tested on:** Intel Core 5 120U (Raptor Lake-U) laptop, macOS 12+ (x86_64). Other CPU models with the same Gen 12 iGPU root are untested.

---

## 🧠 Architectural Deep-Dive & Notable Fixes

### 1. The silicon reality — Alder Lake-P roots

Although the Core 5 120U is marketed as Raptor Lake-U, its GPU subsystem is on the **Alder Lake-P (Gen 12)** architectural root. The driver therefore reuses Gen 12 port/connector tables and the Gen 12 command set unchanged, which is what makes an honest-identity match practical at all.

### 2. The `0x71000013` bit-shift / memory-corruption fix

Video decode commands are submitted straight to the **VCS ring (VDBOX)**, bypassing Metal and VideoToolbox. A 64-bit address must travel through the command buffer unshifted — yet the previous address mask used a `>> 6` shift that corrupted the Luma and scratch-buffer addresses (`kScrIntra` / `kScrDeblock`).

The fix clears the low 6 bits instead, pinning the address to the 64-byte alignment the Gen 12 architecture requires:

```cpp
// Clear only the low 6 bits to pin the Gen 12 GPU address to 64-byte bounds
static constexpr uint64_t kMhwAddressMask = ~0x3FULL;

void writeVcsRingAddress64(uint32_t *commandBuffer, size_t dwIndex, uint64_t gpuAddress) {
    uint64_t alignedAddress = gpuAddress & kMhwAddressMask;
    commandBuffer[dwIndex]     = (uint32_t)(alignedAddress & 0xFFFFFFFFu); // Low DWORD
    commandBuffer[dwIndex + 1] = (uint32_t)(alignedAddress >> 32);         // High DWORD
}
```

**Result:** high-bitrate H.264 streams decode through the hardware VDBOX path instead of spilling onto WindowServer, eliminating the WindowServer poison-frame/stall behaviour previously observed.

### 3. Defensive build discipline

* `-Oz` size-first optimisation, `-ffunction-sections` / `-fdata-sections`, `-Wl,-dead_strip` — dead code is physically stripped from the binary.
* `exported_symbols.txt` whitelist — only ~38 symbols (`_kmod_info`, `_VblankEvent`, `_vcsCmd*`) survive linking; the ~286 C++ mangled symbols are hidden at link time and removed with `strip -x`, shrinking LINKEDIT by ~18 KB.
* Generated `MyIntelGPUVersion.h` keeps the kernel banner in lock-step with the stamped `CFBundleVersion`, so a running kext can never misreport its own version.
* Hard size gate: the build **fails** if the binary exceeds 220,000 bytes.

---

## 📦 Installation (for owners of the same model)

If you have an **Intel Core 5 120U** laptop and macOS 12+ on x86_64, the binary installer is the supported path:

 1. Download **`MyIntelGPU-<version>.pkg`** from the latest green CI run (Actions → *Build MyIntelGPU.kext* → most recent run → **Artifacts** → `MyIntelGPU-pkg-<version>`).
2. Double-click it. The installer:
   * backs up any existing `MyIntelGPU.kext` to `MyIntelGPU.kext.bak-<timestamp>`,
   * installs to `/Library/Extensions` with `root:wheel` / `755`,
   * rebuilds the auxiliary kernel collection (`kmutil install`).
3. Reboot.

Manual install from a CI-built `MyIntelGPU.kext`:

```bash
# Kernel-level code that touches hardware directly — you run this at your own risk.
sudo chown -R root:wheel MyIntelGPU.kext
sudo chmod -R 755 MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo kmutil install --volume-root /
sudo kmutil showloaded | grep MyIntelGPU   # verify
```

---

## 🛠️ Build From Source

### Prerequisites

* **macOS 12+** on x86_64 hardware with the target iGPU (or a CI runner)
* **Xcode Command Line Tools** — `xcode-select --install`
* **MacKernelSDK** — cloned to `~/MacKernelSDK` or `/opt/MacKernelSDK`
  (<https://github.com/acidanthera/MacKernelSDK>)

### Build

```bash
git clone https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS.git
cd First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS
make          # builds MyIntelGPU.kext (with embedded Metal plugin)
make pkg      # builds the installer package MyIntelGPU-<version>.pkg
make version  # shows base / stamped version
make sizecheck
```

The standalone kext embeds `MyIntelGPUMTLDriver.bundle` under `Contents/PlugIns/` — that bundle must travel with the kext or Metal will never resolve the plugin class.

Continuous Integration builds the kext **and** the installer package on every push and on `v*` tags (see [`build.yml`](.github/workflows/build.yml)). Both artifacts (`MyIntelGPU-kext-<version>`, `MyIntelGPU-pkg-<version>`) are downloadable from the run's **Artifacts** section.

---

## 🗂️ Repository Layout

```
Info.plist                  kext personality + IOPCIMatch (0xa7ac8086), version 4.4.213
Makefile                    build, size gate, pkg installer, symbol export control
exported_symbols.txt        whitelist of symbols that survive linking
MyIntelGPU.cpp/.hpp         IOService entry, device matching, IRQ, GGTT bring-up
IntelFramebuffer.cpp        framebuffer service + display-state diagnostics
MyIntelGEMBuffer.cpp        GEM buffers, GGTT binding, Gen 12 TLB invalidation
MyIntelRing.cpp             RCS/BCS ring programming, head-retry logic
MyIntelMedia.cpp            media engine bring-up (VDBOX / VEBOX / SFC)
MyIntelVCS*.cpp/h           VCS ring submission, MFX command emission
myintel_h264_parse.h        pure-C H.264 SPS/PPS/slice parser (no kernel deps)
MyIntelEDIDParser.cpp       EDID parsing for internal panel detection
MyIntelObfuscate.h          source-level constant / string obfuscation utilities
MyIntelGPUMTLDriver.bundle/ Metal plugin (embedded at build time)
test_submit_vcs.m           userspace H.264 decode harness (development proof)
*.command                   ops runbooks (build/verify, VCS harness, audits)
packaging/                  pkg pre/post-install scripts (backup, perms, kmutil)
.github/workflows/build.yml CI: build kext + pkg, size gate, release on tag
```

---

## ❓ FAQ

**Will this work on other Intel iGPUs?**
Only the `0xA7AC` device is matched, by design. Adding another Device ID means re-validating the Gen 12 port tables, command emission and interrupt routing for that silicon.

**Does it need Lilu?**
No. This is a pure native IOService driver — no Lilu plugin shim.

**SIP?**
As with any kext loaded from `/Library/Extensions` on modern macOS, the specifics depend on your macOS version and whether the kext is kmutil-installed. Test on a sacrificial machine first.

**Why do some source constants look odd?**
The shipped kext is produced with `-exported_symbols_list` (≈38 whitelisted symbols, the rest hidden at link time), `strip -x` at link, `-ffunction-sections`/`-fdata-sections` + `-Wl,-dead_strip`, and a 220 KB hard size gate in the Makefile — so the binary exposes almost nothing. `MyIntelObfuscate.h` ships source-level obfuscation helpers, but note: **no source file currently includes it** — string/register obfuscation is not active in the shipped kext today. The PCI match in Info.plist stays literal so IOKit can do its job.

**Can I copy the source into my own driver?**
No — see [License & Redistribution](#-license--redistribution).

---

## ⚠️ Safety & Disclaimer

This is **kernel-level code that drives hardware directly**. Loading it can kernel-panic your machine, leave your GPU in a bad state, or make your installation unbootable. Keep a backup boot volume. **You install and run this entirely at your own risk — there is no warranty of any kind.**

---

## 🔒 License & Redistribution

© 2026 pongpan-bk. **All rights reserved.** The binary installer (`.pkg`) may be installed and used on machines with the target GPU. Redistribution, re-hosting, source copying, de-obfuscation for republication, derivative drivers or rebadged releases are **not permitted** without written permission.

*The source tree is published for transparency and review — not as a grant of rights to copy it.*

---

## 🤖 Credits & Tooling

* **Lead developer & hardware analyst:** [pongpan-bk](https://github.com/pongpan-bk) — Gen 12 iGPU bring-up on macOS, register-level analysis, VCS/VDBOX command path.
* **Core engineering AI agent:** [OpenCode](https://opencode.ai) — exelist/LRC command-queue organisation, register-port control analysis, and interrupt-path calculation (end-to-end synthesis).

---

<div align="center">
  Developed by <b>pongpan-bk</b> · <a href="https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS">Source on GitHub</a>
</div>
