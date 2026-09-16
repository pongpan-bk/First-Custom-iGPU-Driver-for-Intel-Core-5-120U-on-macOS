/*===========================================================================
 *  MyIntelMedia.cpp
 *  Hackintosh Kext — Phase 7: Media Engines (VDBOX / VEBOX)
 *
 *  Implementation:
 *    1. initMediaEngines  — Detect VDBOX/VEBOX availability
 *    2. setMediaClockGating — Disable clock gating
 *    3. publishMediaProperties — Set IORegistry properties
 *    4. startPhase7Media — Orchestrate Phase 7
 *
 *  References:
 *    - Linux i915: drivers/gpu/drm/i915/gt/intel_engine_cs.c
 *    - Linux i915: drivers/gpu/drm/i915/i915_reg.h
 *    - Linux xe:   xe_media.c — media_gt_init
 *///=========================================================================

#include "MyIntelGPU.hpp"
#include "MyIntelMedia.hpp"
#include <IOKit/IOLib.h>

/*
 * IODebug — Debug output to kernel log
 * Uses IOLog with function name and line number
 */
#define IODebug(fmt, ...) \
    IOLog("MyIntelGPU: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#pragma mark -
#pragma mark - initMediaEngines

/*
 * ─────────────────────────────────────────────
 *  bool MyIntelGPU::initMediaEngines(void)
 *
 *  Detect VDBOX and VEBOX availability by reading
 *  the GEN11_GT_VEBOX_VDBOX_DISABLE register (0x138010).
 *
 *  Also initialize instance tracking structs so that
 *  media pipeline code can reference them later.
 *
 *  VDBOX index mapping (RPL-P):
 *    Index 0: VCS0  @ 0x1C0000  — MFX0
 *    Index 1: VCS1  @ 0x1C4000  — MFX1
 *    Index 2: VCS2  @ 0x1D0000  — MFX2
 *    Index 3: VCS3  @ 0x1D4000  — MFX3
 *
 *  VEBOX index mapping (RPL-P):
 *    Index 0: VECS0 @ 0x1C8000  — VEBOX0
 *    Index 1: VECS1 @ 0x1D8000  — VEBOX1
 *
 *  Disable mask format:
 *    Bits [0:3]    = VDBOX disable (bit n → VCSn disabled)
 *    Bits [16:17]  = VEBOX disable (bit 16+n → VECSn disabled)
 *
 *  @return true if at least one VDBOX is available
 * ─────────────────────────────────────────────
 */
bool MyIntelGPU::initMediaEngines(void)
{
    IODebug("Phase 7: initMediaEngines()");

    if (!fRegs || !isValidRegs()) {
        IODebug("MediaEngines: SKIP - MMIO not available");
        return false;
    }

    /* Read VDBOX/VEBOX disable mask (GEN11_GT_VEBOX_VDBOX_DISABLE = 0x138010) */
    uint32_t vdboxDisableMask = readReg32(0x138010);
    fVdboxDisableMask = vdboxDisableMask;
    IODebug("MediaEngines: VDBOX/VEBOX disable mask = 0x%08X", vdboxDisableMask);

    /* ── Detect VDBOX instances (VCS0-VCS3) ── */
    static const uint32_t vdboxBases[] = { 0x1C0000, 0x1C4000, 0x1D0000, 0x1D4000 };
    fActiveVdboxCount = 0;

    for (int i = 0; i < 4; i++) {
        bool available = !(vdboxDisableMask & (1U << i));
        fVdboxInstances[i].mmioBase  = vdboxBases[i];
        fVdboxInstances[i].sfcBase   = vdboxBases[i] + 0x9000;
        fVdboxInstances[i].auxInvReg = 0;
        fVdboxInstances[i].index     = (uint8_t)i;
        fVdboxInstances[i].hasSfc    = true;
        fVdboxInstances[i].initialized = available;

        if (available) {
            fActiveVdboxCount++;
            IODebug("MediaEngines: VDBOX[%d] @ 0x%06X AVAILABLE", i, vdboxBases[i]);
        } else {
            IODebug("MediaEngines: VDBOX[%d] @ 0x%06X DISABLED", i, vdboxBases[i]);
        }
    }

    /* ── Detect VEBOX instances (VECS0-VECS1) ── */
    static const uint32_t veboxBases[] = { 0x1C8000, 0x1D8000 };
    fActiveVeboxCount = 0;

    for (int i = 0; i < 2; i++) {
        bool available = !(vdboxDisableMask & (1U << (16 + i)));
        fVeboxInstances[i].mmioBase  = veboxBases[i];
        fVeboxInstances[i].auxInvReg = 0;
        fVeboxInstances[i].index     = (uint8_t)i;
        fVeboxInstances[i].initialized = available;

        if (available) {
            fActiveVeboxCount++;
            IODebug("MediaEngines: VEBOX[%d] @ 0x%06X AVAILABLE", i, veboxBases[i]);
        } else {
            IODebug("MediaEngines: VEBOX[%d] @ 0x%06X DISABLED", i, veboxBases[i]);
        }
    }

    IODebug("MediaEngines: Found %u VDBOX + %u VEBOX",
            fActiveVdboxCount, fActiveVeboxCount);
    return (fActiveVdboxCount > 0);
}

#pragma mark -
#pragma mark - setMediaClockGating

/*
 * ─────────────────────────────────────────────
 *  void MyIntelGPU::setMediaClockGating(void)
 *
 *  Disable clock gating on IECP, ALN, and MFXPIPE
 *  units within the VDBOX global register block.
 *
 *  Without this, the media pipeline may stall
 *  when exiting deep C-states (RC6).
 *
 *  Register layout (VDBOX global base = VCS0 + 0x6000):
 *    base + 0x3F10  → CGCTL_IECP  (bit 22 = disable)
 *    base + 0x3F18  → CGCTL_ALN   (bit 13 = disable)
 *    base + 0x3F1C  → CGCTL_MFXPIPE (bit 3 = disable)
 *
 *  Reference: xe_media.c — media_gt_init
 *             i915_reg.h — VDBOX_CGCTL3F10/3F18/3F1C
 * ─────────────────────────────────────────────
 */
void MyIntelGPU::setMediaClockGating(void)
{
    IODebug("Phase 7: setMediaClockGating()");

    if (!fRegs || !isValidRegs()) return;

    /* VDBOX global base = VCS0 base + 0x6000 */
    uint32_t vdboxGlobalBase = 0x1C0000 + 0x6000;

    /* Disable clock gating for IECP */
    uint32_t cg3f10 = readReg32(vdboxGlobalBase + 0x3F10);
    cg3f10 |= (1U << 22);   /* VDBOX_CGCTL_IECP_DIS */
    writeReg32(vdboxGlobalBase + 0x3F10, cg3f10);

    /* Disable clock gating for ALN */
    uint32_t cg3f18 = readReg32(vdboxGlobalBase + 0x3F18);
    cg3f18 |= (1U << 13);   /* VDBOX_CGCTL_ALN_DIS */
    writeReg32(vdboxGlobalBase + 0x3F18, cg3f18);

    /* Disable clock gating for MFXPIPE */
    uint32_t cg3f1c = readReg32(vdboxGlobalBase + 0x3F1C);
    cg3f1c |= (1U << 3);    /* VDBOX_CGCTL_MFXPIPE_DIS */
    writeReg32(vdboxGlobalBase + 0x3F1C, cg3f1c);

    IODebug("MediaEngines: Clock gating disabled "
            "(CGCTL: IECP=0x%08X, ALN=0x%08X, MFXPIPE=0x%08X)",
            cg3f10, cg3f18, cg3f1c);
}

#pragma mark -
#pragma mark - publishMediaProperties

/*
 * ─────────────────────────────────────────────
 *  void MyIntelGPU::publishMediaProperties(void)
 *
 *  Set IORegistry properties so that macOS
 *  VideoToolbox / AVFoundation can discover
 *  the GPU's media capabilities.
 *
 *  Properties published:
 *    IntelMediaCapable     (bool)    — media engine present
 *    HevcDecoding          (bool)    — HEVC decode support
 *    HevcEncoding          (bool)    — HEVC encode support
 *    AV1Decoding           (bool)    — AV1 decode support
 *    H264Decoding          (bool)    — H.264 decode support
 *    H264Encoding          (bool)    — H.264 encode support
 *    VP9Decoding           (bool)    — VP9 decode support
 *    VDBOXCount            (uint32)  — active VDBOX instances
 *    VEBOXCount            (uint32)  — active VEBOX instances
 *    MaxDecodeResolution   (string)  — max decode resolution
 *    MaxEncodeResolution   (string)  — max encode resolution
 *
 *  Reference: AppleGraphicsControl, IOGraphicsFamily
 *             AppleIntelGraphicsDriver shim
 * ─────────────────────────────────────────────
 */
void MyIntelGPU::publishMediaProperties(void)
{
    IODebug("Phase 7: publishMediaProperties()");

    setProperty("IntelMediaCapable",     true);
    setProperty("HevcDecoding",          true);
    setProperty("HevcEncoding",          true);
    setProperty("AV1Decoding",           true);
    setProperty("H264Decoding",          true);
    setProperty("H264Encoding",          true);
    setProperty("VP9Decoding",           true);
    setProperty("VDBOXCount",            fActiveVdboxCount);
    setProperty("VEBOXCount",            fActiveVeboxCount);
    setProperty("MaxDecodeResolution",   "8192x8192");
    setProperty("MaxEncodeResolution",   "4096x4096");

    IODebug("MediaEngines: IORegistry properties published");
}

#pragma mark -
#pragma mark - startPhase7Media

/*
 * ─────────────────────────────────────────────
 *  bool MyIntelGPU::startPhase7Media(void)
 *
 *  Orchestrate Phase 7 Media Engine initialization:
 *    1. initMediaEngines     — detect VDBOX/VEBOX
 *    2. setMediaClockGating  — disable clock gating
 *    3. publishMediaProperties — IORegistry props
 *
 *  Called from MyIntelGPU::start() after Phase 6
 *  power management (force wake / RC6) is established.
 *
 *  @return true if media is usable (at least 1 VDBOX)
 * ─────────────────────────────────────────────
 */
bool MyIntelGPU::startPhase7Media(void)
{
    IODebug("=== Phase 7: Media Engines Start ===");

    if (!initMediaEngines()) {
        IODebug("Phase 7: initMediaEngines FAILED");
        fMediaInitialized = false;
        return false;
    }

    setMediaClockGating();
    publishMediaProperties();

    fMediaInitialized = true;
    IODebug("=== Phase 7: Media Engines %s ===",
            fMediaInitialized ? "OK" : "FAILED");
    return fMediaInitialized;
}
