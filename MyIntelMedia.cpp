/*
 * Media engines:
 *    1. initMediaEngines        — detect VDBOX/VEBOX availability
 *    2. setMediaClockGating     — disable clock gating
 *    3. publishMediaProperties  — set IORegistry properties
 *    4. startPhase7Media        — orchestrate media startup
 *
 * References:
 *    - Linux i915: drivers/gpu/drm/i915/gt/intel_engine_cs.c
 *    - Linux i915: drivers/gpu/drm/i915/i915_reg.h
 *    - Linux xe:   xe_media.c - media_gt_init
 */

#include "MyIntelGPU.hpp"
#include "MyIntelMedia.hpp"
#include <IOKit/IOLib.h>

/*
 * IODebug — Debug output to kernel log
 * Uses IOLog with function name and line number
 */
#define IODebug(fmt, ...) \
    IOLog("MyIntelGPU:%d " fmt "\n", __LINE__, ##__VA_ARGS__)

#pragma mark -
#pragma mark - initMediaEngines

/*
 * ─────────────────────────────────────────────
 *  bool MyIntelGPU::initMediaEngines(void)
 *
 *  Native Gen12 LP media topology (ADL-P / RPL-U).
 *
 *  The legacy GEN11_GT_VEBOX_VDBOX_DISABLE register (0x138010) is a
 *  Gen9-era *disable* mask: bit = 1 means "fused off". On a7ac it reads
 *  0x00000000, and interpreting that zero with Gen9 semantics ("nothing
 *  disabled") falsely exposes the absent VDBOX2/VDBOX3 slots. A submit
 *  to those phantom engines faults the GT (MMIO fault / hang / black
 *  screen), so this driver no longer derives topology from 0x138010.
 *
 *  This driver currently enumerates the ADL-P/RPL-U topology documented by
 *  i915: VCS0, VCS2, and VECS0. This is a platform whitelist, not a
 *  runtime fuse read; do not expose additional engines until their fuse
 *  semantics and command paths have been validated on the target hardware.
 *  bit = 1 -> supported instance.
 *
 *    bit 0  -> VDBOX[0] VCS0  @ 0x1C0000
 *    bit 1  -> VDBOX[1] VCS2  @ 0x1D0000  (second physical VDBOX)
 *    bit 16 -> VEBOX[0] VECS0 @ 0x1C8000
 *    bit 17 -> VEBOX[1] VECS1 @ 0x1D8000
 *
 *  VDBOX[2] (VCS1 @ 0x1C4000) and VDBOX[3] (VCS3 @ 0x1D4000) are not
 *  instantiated on U-series silicon and stay disabled.
 *
 *  Upstream cross-check: i915 adl_p_info advertises VCS0, VCS2, and VECS0.
 *  Enumeration is not proof of command-submission support; ring status is
 *  published separately after hardware initialization.
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

    /* This implementation only supports the Raptor Lake-U device in Info.plist.
     * 0x138010 is deliberately not treated as a Gen12 fuse-enable mask. */
    const uint32_t gen12SupportedMediaMask = fDeviceID == 0xA7AC
        ? 0x00010003U  /* VDBOX0, VDBOX1 and VEBOX0 */
        : 0U;
    if (gen12SupportedMediaMask == 0) {
        IODebug("MediaEngines: unsupported PCI device 0x%04X — keeping engines disabled",
                fDeviceID);
    }

    fVdboxDisableMask = (~gen12SupportedMediaMask) & 0x0003000FU;
    IODebug("MediaEngines: supported mask = 0x%08X (disable mask = 0x%08X)",
            gen12SupportedMediaMask, fVdboxDisableMask);

    /* ── Detect VDBOX instances (VCS0-VCS3) ── */
    static const uint32_t vdboxBases[] = {
        VCS0_BASE_REAL, VCS2_BASE_REAL, 0x1C4000, 0x1D4000
    };
    fActiveVdboxCount = 0;

    for (int i = 0; i < 4; i++) {
        bool available = (gen12SupportedMediaMask & (1U << i)) != 0;
        fVdboxInstances[i].mmioBase  = vdboxBases[i];
        fVdboxInstances[i].sfcBase   = vdboxBases[i] + 0x9000;
        fVdboxInstances[i].auxInvReg = 0;
        fVdboxInstances[i].index     = (uint8_t)i;
        fVdboxInstances[i].hasSfc    = true;
        fVdboxInstances[i].enumerated = available;

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
        bool available = (gen12SupportedMediaMask & (1U << (16 + i))) != 0;
        fVeboxInstances[i].mmioBase  = veboxBases[i];
        fVeboxInstances[i].auxInvReg = 0;
        fVeboxInstances[i].index     = (uint8_t)i;
        fVeboxInstances[i].enumerated = available;

        if (available) {
            fActiveVeboxCount++;
            IODebug("MediaEngines: VEBOX[%d] @ 0x%06X AVAILABLE", i, veboxBases[i]);
        } else {
            IODebug("MediaEngines: VEBOX[%d] @ 0x%06X DISABLED", i, veboxBases[i]);
        }
    }

    IODebug("MediaEngines: enumerated %u VDBOX + %u VEBOX",
            fActiveVdboxCount, fActiveVeboxCount);
    return (fActiveVdboxCount > 0);
}

#pragma mark -
#pragma mark - setMediaClockGating

/*
 * ─────────────────────────────────────────────
 *  void MyIntelGPU::setMediaClockGating(void)
 *
 *  Disable clock gating on IECP and ALN inside
 *  each present VDBOX instance.
 *
 *  Without this, the media pipeline may stall
 *  when exiting deep C-states (RC6).
 *
 *  Register layout (offsets are relative to the
 *  VDBOX *instance* mmio_base — i915
 *  gt/intel_engine_regs.h:255 VDBOX_CGCTL3F10(base)):
 *    base + 0x3F10  → CGCTL3F10 · IECPUNIT_CLKGATE_DIS bit 22
 *    base + 0x3F18  → CGCTL3F18 · ALNUNIT_CLKGATE_DIS  bit 13
 *
 *  These registers live in the media forcewake
 *  domains (i915 intel_uncore.c:1349), so they
 *  read back 0 unless VDBOX forcewake is held.
 * ─────────────────────────────────────────────
 */
void MyIntelGPU::setMediaClockGating(void)
{
    IODebug("Phase 7: setMediaClockGating()");

    if (!fRegs || !isValidRegs()) return;

    mediaForceWakeGet();

    static const int vdboxInstances[] = { 0, 1 };

    for (size_t n = 0; n < sizeof(vdboxInstances) / sizeof(vdboxInstances[0]); n++) {
        int i = vdboxInstances[n];
        if (!fVdboxInstances[i].enumerated) continue;

        uint32_t base = fVdboxInstances[i].mmioBase;

        uint32_t iecpBefore = readReg32(base + VDBOX_CGCTL_3F10_OFF);
        writeReg32(base + VDBOX_CGCTL_3F10_OFF, iecpBefore | VDBOX_CGCTL_IECP_DIS);
        uint32_t iecpAfter = readReg32(base + VDBOX_CGCTL_3F10_OFF);

        uint32_t alnBefore = readReg32(base + VDBOX_CGCTL_3F18_OFF);
        writeReg32(base + VDBOX_CGCTL_3F18_OFF, alnBefore | VDBOX_CGCTL_ALN_DIS);
        uint32_t alnAfter = readReg32(base + VDBOX_CGCTL_3F18_OFF);

        IODebug("MediaEngines: CGCTL VDBOX%d base=0x%06X "
                "IECP 0x%08X->0x%08X (bit22 %s) "
                "ALN 0x%08X->0x%08X (bit13 %s)",
                i, base,
                iecpBefore, iecpAfter,
                (iecpAfter & VDBOX_CGCTL_IECP_DIS) ? "LATCHED" : "DROPPED",
                alnBefore, alnAfter,
                (alnAfter & VDBOX_CGCTL_ALN_DIS) ? "LATCHED" : "DROPPED");
    }
}

#pragma mark -
#pragma mark - publishMediaProperties

/*
 * ─────────────────────────────────────────────
 *  void MyIntelGPU::publishMediaProperties(void)
 *
 *  Publish enumerated topology and command-ring status separately.
 *
 *  Codec capabilities are intentionally not published here: the current
 *  VCS command path is not integrated with VideoToolbox / AppleGVA.
 * ─────────────────────────────────────────────
 */
void MyIntelGPU::publishMediaProperties(void)
{
    IODebug("Phase 7: publishMediaProperties()");

    setProperty("IntelMediaCapable",     fActiveVdboxCount > 0);
    setProperty("VDBOXCount",            fActiveVdboxCount);
    setProperty("VEBOXCount",            fActiveVeboxCount);
    setProperty("VDBOXEnumeratedCount",  fActiveVdboxCount);
    setProperty("VEBOXEnumeratedCount",  fActiveVeboxCount);
    setProperty("VCS0RingInitialized",   fRingVCS != NULL);
    setProperty("VCS2RingInitialized",   fRingVCS2 != NULL);
    setProperty("VECS0RingInitialized",  false);
    uint32_t mediaStatus = (fActiveVdboxCount > 0)
        ? ((fRingVCS != NULL) ? 2 : 1) : 0;
    setProperty("IntelMediaStatus", (uint64_t)mediaStatus, 32);

    IODebug("MediaEngines: diagnostic topology published; codec API not advertised");
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
