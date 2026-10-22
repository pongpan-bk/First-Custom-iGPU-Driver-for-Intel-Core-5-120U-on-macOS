<div align="center">

# ⚡ BUILT WITH [OPENCODE](https://opencode.ai)

**This entire driver — i915 register porting, execlists rings, GT interrupts,
kext packaging & CI — was engineered end-to-end with the
[OpenCode](https://opencode.ai) AI coding agent.**

[![Built with OpenCode](https://img.shields.io/badge/Built%20with-OpenCode-7C3AED?style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIyNCIgaGVpZ2h0PSIyNCI+PGNpcmNsZSBjeD0iMTIiIGN5PSIxMiIgcj0iMTAiIGZpbGw9Im5vbmUiIHN0cm9rZT0id2hpdGUiIHN0cm9rZS13aWR0aD0iMiIvPjwvc3ZnPg==)](https://opencode.ai)

</div>

---

# MyIntelGPU — Honest Identity Driver (display-only)

Custom macOS x86_64 kernel extension for **Intel Raptor Lake-U iGPU (Core 5 120U)** — silicon Apple never supported.
Written from scratch by porting i915 register knowledge; loads standalone at `/Library/Extensions` with the REAL PCI device-id, no OpenCore spoofing.

**Author:** [pongpan-bk](https://github.com/pongpan-bk) · **Tooling:** [OpenCode (OhMyOpenCode)](https://opencode.ai)

## What's inside

Display-first driver only (multimedia/VDBOX decode path stripped):

- GGTT/GSM init, VRAM pool, GEM buffers
- RCS/BCS rings with execlists (LRC) submit
- Gen11+ GT interrupt plumbing
- Framebuffer snapshot / display-state diagnostics
- Honest identity: real `device-id`, no FakeID

> Note: `Info.plist` intentionally carries the literal device-id bytes — IOKit personality matching requires them. Source-level constants are obfuscated.

## Build

```bash
git clone https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
cd MyIntelGPU-MacDriver-HonestBridge
make            # needs Xcode CLT + MacKernelSDK (~/MacKernelSDK or /opt/MacKernelSDK)
```

Deploy (own risk — kernel code):

```bash
sudo chown -R root:wheel MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo kmutil install --volume-root /
```
https://www.facebook.com/je4bs
