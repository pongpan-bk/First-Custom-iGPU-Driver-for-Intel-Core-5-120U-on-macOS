# MyIntelGPU — Native Intel Gen 12 iGPU Driver for Hackintosh

Standalone macOS IOKit kernel extension that drives Intel Gen 10–12 integrated GPUs
(Xe-LP, e.g. `8086:A7AC` / Core 5 120U) — hardware Apple never shipped a driver for.

**No Lilu. No WhateverGreen. No OpenCore injection.** Pure native driver loaded
from `/Library/Extensions`.

## Status (2026-10-02)

| Layer | Status |
|---|---|
| PCI setup / MMIO (BAR0/BAR2) | ✅ |
| Display / IOFramebuffer | ⚠️ **not ours** — BIOS/GOP + `IONDRVSupport` owns the panel. `MyIntelFB` probes, builds 1920×1080@60, then loses the match and is freed within ~2 ms (`gEnableDisplayFramebuffer=0`) |
| RCS / BCS / VCS rings + execlists submission | ✅ |
| PPGTT identity mapping (GEM buffers) | ✅ |
| Media engines (VDBOX/VEBOX detect via `0x138010`) | ✅ |
| H.264 hardware decode (VDBOX writes pixels) | ⏳ **unverified** — ring submission and `ESR=0` proven, but the last harness run ended `FAIL-NODATA` |
| ExecBatch / WaitBatch / RingStatus client API | ✅ (v4.0.61) |
| Full Metal / VideoToolbox integration | ⏳ next |

## Hardware decode status — NOT proven

Latest harness output, kept verbatim because it contradicts what this file used
to claim:

```
[READBACK] probe=20480 poison(0xA5)=2091 zero=0 other=18389
[READBACK] => FAIL-NODATA
[SRC-COHERENCE] aperture vs CPU: MATCH ✅
```

`poison(0xA5)=2091` means **2091 of the 20480 probed output bytes were never
written** — the surface still holds the fill pattern the harness wrote before
submission. The earlier `=> HW เขียนพิกเซลจริง! decode สำเร็จ` verdict was the
harness's own conclusion, not evidence, and it contradicted the poison count on
the line above it. `zero=0` (no zeroed pixels) and `other=18389` corroborate:
nothing landed. Submission works and `ESR=0` only means the ring retired without
raising an error — it does not mean pixels were produced.

`[SRC-COHERENCE] MATCH` is still meaningful: the aperture really is CPU-coherent,
so the failure is in the MFX command stream or its decode target, not in the
memory path.

Re-run the harness and record `poison=0` before this row may claim decode works.

The kext speaks the Intel MFX command set directly to the VDBOX engine via the VCS
ring — bypassing VideoToolbox entirely (Apple's private IOGPU protocol is
undocumented; Apple never wrote a Gen12 driver, so the KBL/Gen9 spoof route is a
crash generator, not a shortcut).

## Build

```bash
cd /Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh
make BUILD_NUMBER=94 KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make BUILD_NUMBER=94 KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK -j$(sysctl -n hw.ncpu)
```

Two things the Makefile does not tell you:

**`BUILD_NUMBER` is not optional in practice.** `Makefile:60` defaults it to the
git commit count (and `Makefile:64` falls back to `date +%H%M` outside a git
repo). A bare `make` therefore stamps `4.1.<commit-count>`, so the label drifts
every time anything is committed and can even go *backwards* relative to the
installed bundle. Pick the next number past what is installed:

```bash
/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" \
  /Library/Extensions/MyIntelGPU.kext/Contents/Info.plist
```

`บิ้วMyIntelGPU-AllInOne.command` does this for you and refuses to deploy if the
built bundle's version does not match the requested `BUILD_NUMBER`.

**Never run `make` as root.** Artifacts land root-owned, and the next
`make clean` — including the one inside `บิ้วMyIntelGPU-AllInOne.command` —
dies with `Permission denied`. If a root build already happened, repair it:

```bash
sudo chown -R ppbk:staff MyIntelGPU.kext MyIntelGPUVersion.h *.o
```

`MyIntelGPU.kext` and `MyIntelGPUVersion.h` are regenerated on every build
(`Makefile:71`, `FORCE`), so ownership has to be checked after each build.

## Deploy (L/E only — never OpenCore)

```bash
STAMP=$(date +%Y%m%d-%H%M%S)
mkdir -p ~/kext-backups/pre-$STAMP           # the mkdir is required — cp -R
sudo cp -R /Library/Extensions/MyIntelGPU.kext \
  ~/kext-backups/pre-$STAMP/                # will not create the destination
sudo rm -rf /Library/Extensions/MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo codesign -s - --force /Library/Extensions/MyIntelGPU.kext
sudo kmutil install --volume-root /
```

`kextstat` is deprecated; check what is actually loaded with:

```bash
kmutil showloaded | grep -i myintel   # expect com.pongpan-bk.MyIntelGPU (4.1.x)
```

Do not copy this kext onto the OpenCore partition. There is no
`MyIntelGPU.kext` under `/Volumes/EFI-*/EFI/OC/Kexts` and `config.plist` has no
`MyIntelGPU` entry — that is intentional, not a missing install.

## Post-reboot verification

```bash
log show --predicate 'sender == "MyIntelGPU"' --last 20m --style syslog
```

Use the `sender ==` predicate. A broad `eventMessage CONTAINS[...]` filter drops
`Ring:` lines and pulls in unrelated records.

Expected on a healthy boot:

| Check | Expected |
|---|---|
| `TIMING: start total` | **< 1000 ms** (measured 194–197 ms) |
| `SUBMIT[RCS]` / `[BCS]` / `[VCS]` | one line each |
| `ESR` / `EIR` / `IPEIR` | `0x00000000` on all three engines |
| `[bootlog] N records buffered` | present, `0 dropped` |
| `EDID passthrough: injected` | panel matched and patched |
| `system_profiler SPDisplaysDataType` | `Online: Yes` |

Default boot emits only ~6 kernel-log records. `GTFAULT=0x00000000` is not a
fault — it is a zeroed field inside the `SUBMIT[]` lines. Real faults log
`FAULT ESR=...` / `FAULT_GEN12=... VALID`.

Debug flags — **never boot with these when measuring timing**, they re-emit the
buffered trace and reintroduce the log pressure being measured:

| Flag | Effect |
|---|---|
| `myinteldbg=1` | replay the full in-memory boot trace |
| `myintelrcsdbg=1` | per-submit ring CSB traces |
| `myggttdbg=1` | GGTT mapping dump |

> ⚠️ Do NOT inject via OpenCore `Kernel:Add` or spoof device-id in
> `DeviceProperties` — the kext matches the real PCI class itself. The
> `AppleIntelKBLGraphics` Metal/GL plugin binding in `Info.plist` is a known
> crash risk on Gen12 registers (panics: `stack_chk_fail`, `IOGMD: not wired`).

## Test

Harness lives in `Documents/src/VCSMediaHarness/`:

```bash
cd /Users/ppbk/Documents/src/VCSMediaHarness
clang++ -x objective-c++ -std=c++14 -O2 \
  -framework IOKit -framework CoreFoundation -framework Foundation \
  test_submit_vcs.m MyIntelVCSCommand.cpp -o /tmp/vcs_test_harness
/tmp/vcs_test_harness test_clip.h264 /tmp/vcs_test_out.nv12
```

Or run the combined suite: `Desktop/เทส/MyIntelGPU-VCS-Test.command`

## Client API (IOUserClient selectors)

| Sel | Name | Purpose |
|---|---|---|
| 0–11 | legacy GEM/ring/readback | buffer mgmt, submit proof batches |
| 12 | `ExecBatch` | submit caller-authored command stream (i915 execbuffer) |
| 13 | `WaitBatch` | poll HWSP seqno fence |
| 14 | `RingStatus` | head/tail/space/pending telemetry |
| 20–24 | accelerator step machine | IOAccelerator adoption path |

## Architecture

```
Userspace (harness / VideoToolbox-bypass)
    │ IOConnectCall
    ▼
MyIntelVCSClient (IOUserClient) ── GEM buffers, PPGTT identity map
    ▼
MyIntelRing (execlists ELSP) ── BB_START → flush → USER_INTERRUPT
    ▼
VDBOX / VCS0 engine (MFX command stream, Gen12)
```

Source layout: `MyIntelGPU.cpp` (core/rings), `MyIntelVCSClient.cpp` (client),
`MyIntelVCSCommand.cpp` (Gen12 MFX builders), `MyIntelMedia.cpp` (engine detect),
`MyIntelAccelerator.cpp` (IOAccelerator attempt), `MyIntelRing.cpp` (ring/PPGTT).

## Deployment guide

See `เส้นทางทำต่อ.md` (repo root) for the current continuation path and
`Desktop/MyIntelGPU-DEPLOY-GUIDE.md` for full build/deploy/rollback procedures
(iron rules: L/E only, backup before deploy, never reboot mid-deploy).