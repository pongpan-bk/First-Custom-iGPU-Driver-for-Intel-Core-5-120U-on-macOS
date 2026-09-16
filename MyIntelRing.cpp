/*===========================================================================
 *  MyIntelRing.cpp
 *  Hackintosh Kext — Ring Buffer Engine (Phase 5)
 *
 *  Implementation:
 *    1. ringCreate — alloc GEM buffer + program engine registers
 *    2. ringBegin / ringAdvance — command emission
 *    3. ringSubmit — RING_TAIL write = kick GPU
 *    4. ringEmit* — MI command helpers
 *
 *  References:
 *    - Linux i915: intel_ring_submission.c xcs_resume()
 *                  intel_ring.h intel_ring_begin/advance
 *    - i915_reg.h: RING_TAIL, RING_HEAD, RING_START, RING_CTL
 *///=========================================================================

#include "MyIntelRing.hpp"
#include <IOKit/IOLib.h>

/*
 * ─────────────────────────────────────────────
 *  Debug Logging
 * ─────────────────────────────────────────────
 * IOLog() → kernel console (verbose screen + dmesg).
 *
 * NOTE: os_log was tried (subsystem com.myintelgpu.driver) but the os_log
 * sections made the unsigned kext fail prelink/auxKC → BOOT FAILURE.
 * Reverted to IOLog-only. Capture via dmesg (boot-args msgbuf=1048576
 * keeps a 1MB kernel message buffer, ~survives long enough after boot).
 */
#define RING_DEBUG(str, ...) \
    do { IOLog("Ring[%s]: " str "\n", \
         (ring && ring->magic == RING_MAGIC) \
            ? (ring->engineType == kMyIntelEngineRCS ? "RCS" : \
               ring->engineType == kMyIntelEngineBCS ? "BCS" : "?") \
            : "NEW", \
         ##__VA_ARGS__); } while(0)

#define RING_DEBUG_RAW(str, ...) \
    do { IOLog("Ring: " str "\n", ##__VA_ARGS__); } while(0)

#if 0
#define RING_TRACE(str, ...) RING_DEBUG(str, ##__VA_ARGS__)
#else
#define RING_TRACE(str, ...) do { } while(0)
#endif

/*
 * ─────────────────────────────────────────────
 *  Internal Helpers
 * ─────────────────────────────────────────────
 */

/* Get engine name string */
static const char *engineName(MyIntelEngineType type)
{
    switch (type) {
        case kMyIntelEngineRCS:  return "RCS";
        case kMyIntelEngineBCS:  return "BCS";
        case kMyIntelEngineVCS:  return "VCS";
        case kMyIntelEngineVECS: return "VECS";
        default:                 return "???";
    }
}

/* Forward declarations (defined below ringCreate) */
static bool lrcBuildContext(MyIntelRing *ring);
static bool lrcBuildContextXcs(MyIntelRing *ring);
static void lrcUpdateRingRegs(MyIntelRing *ring);

/*
 * ─────────────────────────────────────────────
 *  ringCreate
 * ─────────────────────────────────────────────
 */
MyIntelRing *ringCreate(
    MyIntelEngineType    engineType,
    uint32_t             mmioBase,
    uint32_t             ringSize,
    MyIntelRingCallbacks *cb)
{
    if (!cb || !cb->gemAlloc || !cb->gemFree || !cb->gemGetOffset ||
        !cb->gemGetVAddr || !cb->readReg32 || !cb->writeReg32) {
        RING_DEBUG_RAW("ERROR: ringCreate — invalid callbacks");
        return NULL;
    }

    RING_DEBUG_RAW("ringCreate: engine=%s mmioBase=0x%X size=%u",
                   engineName(engineType), mmioBase, ringSize);

    /* Validate ring size (power of 2, within bounds) */
    if (ringSize < RING_MIN_SIZE || ringSize > RING_MAX_SIZE ||
        (ringSize & (ringSize - 1)) != 0) {
        RING_DEBUG_RAW("ERROR: invalid ring size %u (must be power of 2, "
                       "%u-%u)", ringSize, RING_MIN_SIZE, RING_MAX_SIZE);
        return NULL;
    }

    /* Allocate ring struct */
    MyIntelRing *ring = (MyIntelRing *)IOMalloc(sizeof(MyIntelRing));
    if (!ring) {
        RING_DEBUG_RAW("ERROR: failed to allocate ring struct");
        return NULL;
    }
    bzero(ring, sizeof(MyIntelRing));

    ring->magic       = RING_MAGIC;
    ring->engineType  = engineType;
    ring->mmioBase    = mmioBase;
    ring->size        = ringSize;
    ring->head        = 0;
    ring->tail        = 0;
    ring->emit        = 0;
    ring->space       = ringSize - RING_MIN_FREE_SPACE;
    ring->lastTail    = 0;
    ring->wraparound  = 0;
    ring->initialized = false;

    /*
     * Step 1: Allocate GEM buffer for ring
     *
     * Use gemAlloc callback — MyIntelGPU will allocate a GEM buffer
     * in the GGTT aperture via gemBufferCreate
     */
    void *gemBuf = cb->gemAlloc(cb->context, ringSize,
                                 GEM_FLAG_RING | GEM_FLAG_CPU_WRITE);
    if (!gemBuf) {
        RING_DEBUG_RAW("ERROR: gemAlloc failed for ring buffer (size=%u)", ringSize);
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    ring->gemBuf     = gemBuf;  /* Store for later cleanup in ringDestroy */
    ring->ggttOffset = cb->gemGetOffset(cb->context, gemBuf);
    ring->vaddr      = (uint32_t *)cb->gemGetVAddr(cb->context, gemBuf);

    if (!ring->vaddr || ring->ggttOffset == 0) {
        RING_DEBUG_RAW("ERROR: gem buffer has no vaddr/ggttOffset");
        cb->gemFree(cb->context, gemBuf);
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    RING_DEBUG_RAW("ringCreate: buffer @ ggtt=0x%X vaddr=%p size=%u",
                   ring->ggttOffset, ring->vaddr, ringSize);

    /*
     * Step 2: Program engine registers via MMIO
     *
     * Sequence (from Linux xcs_resume):
     *   1. RING_CTL = 0 (disable ring first)
     *   2. RING_HEAD = 0
     *   3. RING_TAIL = 0
     *   4. RING_START = ggttOffset
     *   5. RING_CTL = size_bits | RING_VALID
     *   6. RING_MI_MODE = clear STOP_RING
     *   7. RING_TAIL = tail (0)
     *
     * Force-wake the GT for the whole sequence: without it, RING_*
     * writes on Gen11+ are dropped while the RENDER power well is
     * down (RC6) → RING_CTL readback stays 0x00000000.
     */
    bool fwHeld = (cb->fwGet != NULL) && cb->fwGet(cb->context);
    if (!fwHeld) {
        /*
         * Fail-fast: without forcewake the RING_* writes on Gen11+ are
         * gated by the power well (RC6) and silently dropped — RING_CTL
         * readback stays 0. Writing registers anyway is futile and leaves
         * the engine in an undefined state; abort instead.
         */
        RING_DEBUG_RAW("ERROR: forcewake acquire failed — cannot program engine regs");
        cb->gemFree(cb->context, gemBuf);
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    uint32_t base = mmioBase;

    /* 0. Engine reset handshake (i915 gen8+ path — intel_engine_regs.h RING_RESET_CTL)
     *    RING_RESET_CTL = base + 0xd0:
     *      bit0 REQUEST_RESET, bit1 READY_TO_RESET, bit2 CAT_ERROR
     *    i915 (intel_gt.c gen6_reset_engines): assert REQUEST, wait READY,
     *    then release. Without this, Gen8+ (incl RPL/A7AC) engine stays in
     *    reset-affect state and RING_CTL_VALID never sets. N.B. on Gen8+ the
     *    legacy ring-format setup is GEM_BUG_ON(ver>=8) in i915, so VALID is
     *    expected to be HW/lifecycle-managed (execlists mode readback shows
     *    the run state); record what we measure. */
    uint32_t resetCtl = cb->readReg32(cb->context, base + RING_RESET_CTL_OFFSET);
    RING_DEBUG_RAW("ringCreate: RING_RESET_CTL before = 0x%X (req=%u ready=%u cat=%u)",
                   resetCtl,
                   (resetCtl >> 0) & 1u, (resetCtl >> 1) & 1u, (resetCtl >> 2) & 1u);

    /* If CAT_ERROR is sticky or REQUEST is already asserted, clear via write 0
     * first (i915 gen6_reset_engines asserts REQUEST then waits READY). Do NOT
     * assert a full reset here: we narrow to wake the engine for ring work. */
    if (resetCtl & (1u << 0)) {
        cb->writeReg32(cb->context, base + RING_RESET_CTL_OFFSET, 0);
        cb->readReg32(cb->context, base + RING_RESET_CTL_OFFSET);
    }

    /* 1. Stop + disable ring */
    cb->writeReg32(cb->context, base + RING_CTL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_CTL_REG_OFFSET); /* posting read */

    /* 2. Zero head + tail */
    cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET);
    cb->writeReg32(cb->context, base + RING_TAIL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_TAIL_REG_OFFSET);

    /* 🌟 สับลูปปั่นค่ารีไตร HEAD 50ms ปลุก Silicon Intel ให้กลืนค่าศูนย์จริงด้วยมือจารเอง! */
    int head_retry = 50000; // 50ms (ใช้ delay ไมโครวินาที)
    while (head_retry-- > 0) {
        cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET, 0);
        if ((cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET) & 0xFFFFFFFC) == 0) break;
        IODelay(1);
    }

    if (head_retry <= 0) {
        RING_DEBUG_RAW("peetalog::[Hardware Error] RING_HEAD write failed to stick! Engine stuck in deep RC6");
        if (fwHeld) cb->fwPut(cb->context);
        cb->gemFree(cb->context, gemBuf);
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    /*
     * ── Step 0: Allocate GEM buffer for HWS Page ──
     *
     * i915 (intel_ring_submission.c) allocates a separate HWSP buffer
     * and writes its GGTT offset to RING_HWS_PGA.
     */
    void *hwspBuf = cb->gemAlloc(cb->context, GEM_PAGE_SIZE,
                                 GEM_FLAG_RING | GEM_FLAG_CPU_WRITE);
    if (hwspBuf) {
        ring->hwspGemBuf = hwspBuf;
        ring->hwspGgtt = cb->gemGetOffset(cb->context, hwspBuf);
        ring->hwspVaddr = (uint32_t *)cb->gemGetVAddr(cb->context, hwspBuf);
        cb->writeReg32(cb->context, base + RING_HWS_PGA_OFFSET, ring->hwspGgtt);
        cb->readReg32(cb->context, base + RING_HWS_PGA_OFFSET); /* posting read */

        /* Gen11+ CSB: floor RING_CONTEXT_STATUS_PTR to HWSP with 12 entries —
         * full reset_csb_pointers() (i915 intel_execlists_submission.c:2804):
         *   1. MMIO 0xffff<<16 | rv<<8 | rv (rv=11)  → RING_CONTEXT_STATUS_PTR
         *   2. WRITE_ONCE(hwsp[0x2f], rv)            → HWSP write ptr = 11
         *   3. memset(csb_status, -1, (rv+1)*8)      → fill 12 u64 entries = 0xFF
         *   4. wmb() + second MMIO write (paranoia)
         * The HW then mirrors CSB entries to HWSP dword 0x10 (8B each) and
         * advances the write ptr at dword 0x2f. */
        cb->writeReg32(cb->context, base + RING_CONTEXT_STATUS_PTR_OFFSET,
                       CONTEXT_STATUS_PTR_RESET);
        cb->readReg32(cb->context, base + RING_CONTEXT_STATUS_PTR_OFFSET); /* posting read */
        ring->hwspVaddr[HWS_CSB_WRITE_DWORD] = HWS_CSB_ENTRIES_GEN11 - 1;
        memset((uint8_t *)ring->hwspVaddr + HWS_CSB_BUF0_DWORD * 4, 0xFF,
               HWS_CSB_ENTRIES_GEN11 * sizeof(uint64_t));
        OSSynchronizeIO();
        cb->writeReg32(cb->context, base + RING_CONTEXT_STATUS_PTR_OFFSET,
                       CONTEXT_STATUS_PTR_RESET);
        cb->readReg32(cb->context, base + RING_CONTEXT_STATUS_PTR_OFFSET); /* posting read */

        /* 🌟 HWS Status Page Interrupt Mask (mask all) + TLB flush */
        cb->writeReg32(cb->context, base + RING_HWS_STAM_OFFSET, ~0U);
        cb->readReg32(cb->context, base + RING_HWS_STAM_OFFSET);
        /* flush_cs_tlb via RING_INSTPM: enable sync flush + TLB invalidation */
        cb->writeReg32(cb->context, base + RING_INSTPM_OFFSET,
                       0x02000200); /* INSTPM_SYNC_FLUSH | INSTPM_TLB_INVALIDATE masked enable */
        cb->readReg32(cb->context, base + RING_INSTPM_OFFSET); /* posting read */
        RING_DEBUG_RAW("ringCreate: HWS Page @ ggtt=0x%X (STAM+TLB)", ring->hwspGgtt);
    } else {
        RING_DEBUG_RAW("ringCreate: WARNING — failed to allocate HWS Page");
        /* Continue without HWSP, head-tracking will be via polling */
    }

    /* 3. Set ring start address (GGTT offset) */
    cb->writeReg32(cb->context, base + RING_START_REG_OFFSET, ring->ggttOffset);
    cb->readReg32(cb->context, base + RING_START_REG_OFFSET);

    /* ⛔ NO MMIO PPGTT setup on Gen12 — REMOVED (2.0.18)
     * i915 sets GFX_PPGTT_ENABLE (RING_MODE_GEN7 bit9) + RING_PP_DIR_DCLV/BASE
     * ONLY in the legacy Gen6/7 ring-submission path:
     *   - intel_ring_submission.c:174 (GEM_BUG_ON(GRAPHICS_VER>=8) — never runs on Gen12)
     *   - gen6_ppgtt.c:70 (Gen6 only)
     * On Gen8+, PPGTT is mandatory and page tables are carried PER-CONTEXT in the
     * LRC context image (PDP/PML4 regs), NOT via these MMIO regs. Writing
     * GFX_PPGTT_ENABLE + PP_DIR_BASE=0 (empty placeholder) put HW into full-PPGTT
     * translation against a null page table → ring fetch faults before the first
     * command is read (observed: HEAD=0, ACTIVE=0, ESR=0x1, IPEHR=0x980F0400). */

    /* Gen12/late-Gen11 execlists: RING_MODE_GEN7 bit15 GFX_RUN_LIST_ENABLE
     * selects execlists mode. Rule-check capability by reading + reporting —
     * do not clear it; legacy ring programming is not supported on Gen8+. */
    {
        uint32_t modeGen7 = cb->readReg32(cb->context, base + RING_MODE_GEN7_OFFSET);
        RING_DEBUG_RAW("ringCreate: RING_MODE_GEN7 = 0x%X (RUN_LIST=%u)",
                       modeGen7, (modeGen7 >> 15) & 1u);
    }

    /* ⛔ GFX_RUN_LIST_ENABLE (bit15) write REMOVED (2.0.18)
     * i915 enable_execlists() (intel_execlists_submission.c:2936-2940) picks:
     *   ver >= 11 → REG_MASKED_FIELD_ENABLE(GEN11_GFX_DISABLE_LEGACY_MODE)  [0x00080008]
     *   ver <  11 → REG_MASKED_FIELD_ENABLE(GFX_RUN_LIST_ENABLE)            [0x80008000]
     * On Gen11+ (this RPL = Gen12.2) bit15 is NOT written at all; the run-list is
     * the only mode once DISABLE_LEGACY_MODE (written below) is set — matching i915. */

    /* Gen11+ execlists: i915 enable_execlists() also sets bit3
     * GEN11_GFX_DISABLE_LEGACY_MODE (masked _MASKED_BIT_ENABLE → 0x00080008)
     * — switches the engine out of the legacy ring (GGTT) mode. Without it,
     * submit through SQ_CONTENTS/EL_CTRL_LOAD is ignored. */
    {
        cb->writeReg32(cb->context, base + RING_MODE_GEN7_OFFSET, 0x00080008u);
        cb->readReg32(cb->context, base + RING_MODE_GEN7_OFFSET); /* posting read */
        uint32_t after = cb->readReg32(cb->context, base + RING_MODE_GEN7_OFFSET);
        RING_DEBUG_RAW("ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x%X (bit3=%u)",
                       after, (after >> 3) & 1u);
    }

    /* Ground truth of the execlist submission ports (read-only) —
     * EXECLIST_STATUS_LO/HI (base+0x234/238) shows whether the engine
     * accepted any context into its run queue; SQ_CONTENTS (base+0x510)
     * mirrors the 64-bit context descriptor last written to ELSP. */
    {
        uint32_t execStatusLo = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_LO_OFFSET);
        uint32_t execStatusHi = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_HI_OFFSET);
        uint32_t sqContents0  = cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET);
        uint32_t sqContents1  = cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET + 4);
        RING_DEBUG_RAW("ringCreate: EXECLIST_STATUS_LO=0x%08X HI=0x%08X (active=%u pend=%u load=%u)",
                       execStatusLo, execStatusHi,
                       (execStatusLo >> 0) & 1u, (execStatusLo >> 1) & 1u, (execStatusLo >> 2) & 1u);
        RING_DEBUG_RAW("ringCreate: EXECLIST_SQ_CONTENTS[0]=0x%08X [1]=0x%08X (desc=0x%016llX)",
                       sqContents0, sqContents1,
                       ((uint64_t)sqContents1 << 32) | sqContents0);
    }


    /* 4. Enable ring with size
     *    RING_CTL bits 13:11 = (size / 4KB) - 1
     *    bit 0 = RING_VALID
     */
    {
        uint32_t sizeInPages = ringSize >> GEM_PAGE_SHIFT; /* GEM_PAGE_SHIFT = 12 */
        uint32_t ctlValue = ((sizeInPages - 1) << RING_CTL_SIZE_SHIFT) | RING_CTL_VALID;

        /* Gen12+ (execlists mode) never sets RING_CTL_VALID via MMIO — the
         * ring is activated by the LRC context image instead. This poll is
         * only a quick legacy-path check: bound it to 50 iterations (~50µs,
         * was 5ms) so ringCreate doesn't burn 5ms at boot waiting for a bit
         * that never sets on this HW. The result is informational only. */
        cb->writeReg32(cb->context, base + RING_CTL_REG_OFFSET, ctlValue);

        int valid_retry = 50;
        bool is_valid = false;
        while (valid_retry-- > 0) {
            if (cb->readReg32(cb->context, base + RING_CTL_REG_OFFSET) & RING_CTL_VALID) {
                is_valid = true;
                break;
            }
            IODelay(1);
        }

        if (!is_valid) {
            /*
             * Gen12+ HW never sets VALID via MMIO in execlists mode (i915
             * GEM_BUG_ON(ver>=8) for legacy ring submission). Readback 0 is
             * EXPECTED here — the ring is activated by the LRC context image
             * written to the execlist (CTX_RING_CTL | RING_VALID), not by
             * RING_CTL. Log + continue; the submit path checks lrcInited.
             */
            RING_DEBUG_RAW("ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing");
        } else {
            RING_TRACE("RING_CTL = 0x%08X (VALID set via MMIO — legacy path active)", ctlValue);
        }
    }

    /* 5. Clear STOP_RING — mainline i915 (enable_execlists) uses the masked
     *    form _MASKED_BIT_DISABLE(STOP_RING) = 0x00010000 (mask bit16, clear
     *    bit0); a plain RMW write is a no-op on Gen11+ (register is masked). */
    {
        cb->writeReg32(cb->context, base + RING_MI_MODE_REG_OFFSET, 0x00010000u);
        cb->readReg32(cb->context, base + RING_MI_MODE_REG_OFFSET);
    }

    /* 6. Initial tail = 0 (ring is empty) */
    ring->head = 0;
    ring->tail = 0;
    ring->emit = 0;
    ring->space = ringSize - RING_MIN_FREE_SPACE;
    ring->lastTail = 0;
    ring->workPending = false;

    cb->writeReg32(cb->context, base + RING_TAIL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_TAIL_REG_OFFSET);

    /* 6.5 PPGTT identity-map — MUST exist BEFORE lrcBuildContext (PDP0
     *     phys is patched into LRC value slots CTX_PDP0_UDW/LDW (0x31/0x33) at build time).
     *     Failure degrades to no-batch mode: ring still runs, BB_START
     *     submissions will fail at lrcMapBatchPages instead. */
    lrcAllocPPGTT(ring, cb);

    /* 7. Execlists LRC context object (Gen12) — 4 pages = 1 PPHWSP + state.
     *    The LRI image (lrcBuildContext) carries RING_VALID + head/tail/start
     *    => the execlist submit path activates the ring, no MMIO RING_CTL.
     *    If LRC build fails, keep lrcInited=false so ringSubmit falls back
     *    to the legacy RING_TAIL path. */
    ring->lrcGemBuf = cb->gemAlloc(cb->context, LRC_CONTEXT_SIZE,
                                   GEM_FLAG_RING | GEM_FLAG_CPU_WRITE);
    if (ring->lrcGemBuf != NULL) {
        ring->lrcGgtt = cb->gemGetOffset(cb->context, ring->lrcGemBuf);
        ring->lrcVaddr = (uint32_t *)cb->gemGetVAddr(cb->context, ring->lrcGemBuf);
        RING_DEBUG_RAW("ringCreate: LRC context @ ggtt=0x%X vaddr=%p size=0x%X",
                       ring->lrcGgtt, ring->lrcVaddr, LRC_CONTEXT_SIZE);
        if (ring->engineType == kMyIntelEngineBCS ||
            ring->engineType == kMyIntelEngineVCS ||
            ring->engineType == kMyIntelEngineVECS) {
            /* 2.0.250: i915 gen12 uses the XCS register image (52 dw,
             * gen12_xcs_offsets) for EVERY engine except RENDER — VCS was
             * previously built with the 185-dword RCS image, which carries
             * render-only regs (RCS_INDIRECT_CTX, R_PWR_CLK_STATE, GPR
             * block) that don't exist on VDBOX. */
            if (lrcBuildContextXcs(ring)) {
                ring->lrcInited = true;

                /* Context runs in 4-level PPGTT (LEGACY_64B): the indirect
                 * BB fetch walks PPGTT, but only batch pages were mapped.
                 * Map ALL LRC pages into the identity window or the restore
                 * fetch faults on not-present (ESR bit0, head frozen). */
                const uint64_t *lrcPhys =
                    cb->gemGetPagesPhys(cb->context, ring->lrcGemBuf);
                if (lrcPhys &&
                    lrcMapBatchPages(ring, ring->lrcGgtt, lrcPhys,
                                     LRC_CONTEXT_SIZE / GEM_PAGE_SIZE)) {
                    RING_DEBUG_RAW("ringCreate: LRC %u pages mapped into PPGTT",
                                   LRC_CONTEXT_SIZE / GEM_PAGE_SIZE);
                } else {
                    RING_DEBUG_RAW("ringCreate: WARNING — LRC PPGTT map failed "
                                   "(indirect BB will fault)");
                }
            } else {
                RING_DEBUG_RAW("ringCreate: WARNING — XCS LRC image build failed, using legacy submit");
            }
        } else if (lrcBuildContext(ring)) {
            ring->lrcInited = true;
        } else {
            RING_DEBUG_RAW("ringCreate: WARNING — LRC image build failed, using legacy submit");
        }
    } else {
        RING_DEBUG_RAW("ringCreate: WARNING — LRC alloc failed, using legacy submit");
    }

    if (fwHeld) cb->fwPut(cb->context);

    ring->initialized = true;

    RING_DEBUG("ringCreate: OK — ring ready at GGTT=0x%X vaddr=%p",
               ring->ggttOffset, ring->vaddr);

    return ring;
}

/*
 * ─────────────────────────────────────────────
 *  ringDestroy
 * ─────────────────────────────────────────────
 */
void ringDestroy(MyIntelRing *ring, MyIntelRingCallbacks *cb)
{
    if (!ring) return;

    RING_TRACE("ringDestroy:");

    if (ring->magic == RING_MAGIC && ring->initialized && cb && cb->writeReg32) {
        /* Disable ring — hold forcewake so the writes are not gated */
        bool fwHeld = (cb->fwGet != NULL) && cb->fwGet(cb->context);
        uint32_t base = ring->mmioBase;
        cb->writeReg32(cb->context, base + RING_CTL_REG_OFFSET, 0);
        cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET, 0);
        cb->writeReg32(cb->context, base + RING_TAIL_REG_OFFSET, 0);
        cb->writeReg32(cb->context, base + RING_START_REG_OFFSET, 0);
        if (fwHeld) cb->fwPut(cb->context);
    }

    /* Free HWSP buffer */
    if (ring->hwspGemBuf && cb && cb->gemFree) {
        cb->gemFree(cb->context, ring->hwspGemBuf);
        ring->hwspGemBuf = NULL;
        ring->hwspGgtt = 0;
        ring->hwspVaddr = NULL;
    }

    /* Free the execlists LRC context object */
    if (ring->lrcGemBuf && cb && cb->gemFree) {
        cb->gemFree(cb->context, ring->lrcGemBuf);
        ring->lrcGemBuf = NULL;
        ring->lrcGgtt = 0;
        ring->lrcVaddr = NULL;
        ring->lrcInited = false;
    }

    /* Free the GEM buffer backing this ring — plug memory leak */
    if (ring->gemBuf && cb && cb->gemFree) {
        cb->gemFree(cb->context, ring->gemBuf);
        ring->gemBuf = NULL;
    }

    ring->magic = 0;
    IOFree(ring, sizeof(MyIntelRing));
}

/*
 * ─────────────────────────────────────────────
 *  ringReset
 * ─────────────────────────────────────────────
 */
bool ringReset(MyIntelRing *ring, MyIntelRingCallbacks *cb)
{
    if (!ring || ring->magic != RING_MAGIC || !cb) return false;

    RING_DEBUG("ringReset:");

    uint32_t base = ring->mmioBase;

    bool fwHeld = (cb->fwGet != NULL) && cb->fwGet(cb->context);

    /* Stop ring: HEAD = TAIL (empty the ring), CTL = 0 */
    cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET,
                   cb->readReg32(cb->context, base + RING_TAIL_REG_OFFSET));
    cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET);

    cb->writeReg32(cb->context, base + RING_CTL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_CTL_REG_OFFSET);

    /* Clear head/tail */
    cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET, 0);
    cb->writeReg32(cb->context, base + RING_TAIL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET);

    /* Verify head is 0 — retry loop (50ms) for power-well wake (mirrors ringCreate) */
    int head_retry = 50000;
    while (head_retry-- > 0) {
        cb->writeReg32(cb->context, base + RING_HEAD_REG_OFFSET, 0);
        if ((cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET) & 0xFFFFFFFC) == 0) break;
        IODelay(1);
    }
    if (head_retry <= 0) {
        RING_DEBUG("WARNING: RING_HEAD write failed to stick after reset (deep RC6?)");
    }

    /* Re-initialize */
    cb->writeReg32(cb->context, base + RING_START_REG_OFFSET, ring->ggttOffset);

    uint32_t sizeInPages = ring->size >> GEM_PAGE_SHIFT;
    uint32_t ctlValue = ((sizeInPages - 1) << RING_CTL_SIZE_SHIFT) | RING_CTL_VALID;
    cb->writeReg32(cb->context, base + RING_CTL_REG_OFFSET, ctlValue);

    /* Poll RING_CTL VALID (5ms) — mirrors ringCreate */
    int valid_retry = 5000;
    bool is_valid = false;
    while (valid_retry-- > 0) {
        if (cb->readReg32(cb->context, base + RING_CTL_REG_OFFSET) & RING_CTL_VALID) {
            is_valid = true;
            break;
        }
        IODelay(1);
    }
    if (!is_valid) {
        RING_DEBUG("WARNING: RING_CTL_VALID not set after reset! Timeout 5ms");
        if (fwHeld) cb->fwPut(cb->context);
        return false;
    }

    /* Clear STOP_RING — Gen11+ RING_MI_MODE is a masked register
     * (bits 16:31 = mask, bits 0:15 = value); a plain RMW write is a
     * no-op because the mask bits are 0. 0x00010000 =
     * _MASKED_BIT_DISABLE(STOP_RING): mask bit16, clear bit0 — same
     * form as ringInit(). */
    cb->writeReg32(cb->context, base + RING_MI_MODE_REG_OFFSET, 0x00010000u);
    cb->readReg32(cb->context, base + RING_MI_MODE_REG_OFFSET);

    /* Reset state */
    ring->head = 0;
    ring->tail = 0;
    ring->emit = 0;
    ring->space = ring->size - RING_MIN_FREE_SPACE;
    ring->lastTail = 0;
    ring->workPending = false;

    /* Write tail */
    cb->writeReg32(cb->context, base + RING_TAIL_REG_OFFSET, 0);
    cb->readReg32(cb->context, base + RING_TAIL_REG_OFFSET);

    if (fwHeld) cb->fwPut(cb->context);

    RING_DEBUG("ringReset: OK");
    return true;
}

/*
 * ─────────────────────────────────────────────
 *  ringBegin — Reserve dwords in ring buffer
 * ─────────────────────────────────────────────
 *
 *  Reference: Linux intel_ring_begin()
 *    - Check if enough space in ring
 *    - If emit + size*4 wraps past end, emit NOOPs to align
 *    - Return pointer to write position
 *
 *  Returns NULL if not enough space (caller must flush/wait)
 */
uint32_t *ringBegin(MyIntelRing *ring, uint32_t dwords)
{
    if (!ring || ring->magic != RING_MAGIC) {
        return NULL;
    }
    if (dwords == 0) {
        return NULL;
    }

    uint32_t neededBytes = dwords * sizeof(uint32_t);

    /* Sync head from HW (Gen12 execlists): MMIO RING_HEAD reads 0 in
     * execlists mode — the real head lives in the LRC image
     * (state[CTX_RING_HEAD]), advanced by HW as it consumes commands
     * (mirrors i915 intel_ring_update_head()). Reclaiming that space
     * keeps ring->space from draining one-way to 0 (the root of the
     * tail=16320 accumulation in the kick loop). */
    if (ring->lrcInited && ring->lrcVaddr) {
        uint32_t *state = (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET);
        uint32_t hwHead = state[CTX_RING_HEAD];
        if (hwHead < ring->size) {
            uint32_t used = (ring->tail - ring->head) & (ring->size - 1);
            /* head==tail means DRAINED (i915 ring semantics); without this
             * branch space froze at its last value once HW caught up → deadlock */
            if (hwHead == ring->tail) {
                uint32_t fullSpace = ring->size - RING_MIN_FREE_SPACE;
                if (used != 0 || ring->space != fullSpace) {
                    ring->head = hwHead;
                    ring->space = fullSpace;
                    RING_DEBUG("ringBegin: ring drained (head==tail=%u) — space recovered to %u",
                               hwHead, ring->space);
                }
            } else {
                uint32_t reclaimed = (hwHead - ring->head) & (ring->size - 1);
                if (reclaimed != 0 && reclaimed <= used) {
                    ring->head = hwHead;
                    ring->space += reclaimed;
                    RING_DEBUG("ringBegin: HW head advanced %u bytes (head %u -> %u, space=%u)",
                               reclaimed, (hwHead - reclaimed) & (ring->size - 1),
                               hwHead, ring->space);
                }
            }
        }
    }

    if (neededBytes > ring->space) {
        RING_DEBUG("WARNING: ring full (need %u bytes, space %u, head=%u tail=%u)",
                   neededBytes, ring->space, ring->head, ring->tail);
        return NULL;
    }

    /* Check for wraparound: if emit + neededBytes > size, pad with NOOPs */
    uint32_t emitEnd = ring->emit + neededBytes;
    if (emitEnd > ring->size) {
        /* Fill remaining space with NOOPs */
        uint32_t padBytes = ring->size - ring->emit;
        uint32_t padDwords = padBytes / sizeof(uint32_t);
        uint32_t *pad = (uint32_t *)((uint8_t *)ring->vaddr + ring->emit);
        for (uint32_t i = 0; i < padDwords; i++) {
            *pad++ = MI_NOOP;
        }
        /* Write barrier so GPU sees the NOOPs */
        OSSynchronizeIO();

        /* Wrap emit to beginning */
        ring->emit = 0;
        ring->wraparound++;

        RING_TRACE("ringBegin: wrapped around (wraparound=%u)", ring->wraparound);

        /* Re-check after wrap */
        emitEnd = neededBytes;
        if (emitEnd > ring->size) {
            RING_DEBUG("ERROR: command too large for ring (%u bytes > %u)",
                       neededBytes, ring->size);
            return NULL;
        }
    }

    /* Return pointer to write position */
    ring->space -= neededBytes;
    return (uint32_t *)((uint8_t *)ring->vaddr + ring->emit);
}

/*
 * ─────────────────────────────────────────────
 *  ringAdvance — Update tail after writing commands
 * ─────────────────────────────────────────────
 *
 *  Reference: Linux intel_ring_advance()
 *    - Update ring->tail based on emit and cs pointer
 *
 *  @param cs  Pointer to one past the last written dword
 *             (same semantics as Linux intel_ring_advance)
 */
void ringAdvance(MyIntelRing *ring, uint32_t *cs)
{
    if (!ring || ring->magic != RING_MAGIC || !cs) return;

    /* Calculate bytes written: (cs - vaddr) */
    uint32_t newEmit = (uint32_t)((uintptr_t)cs - (uintptr_t)ring->vaddr);

    /* Must be within ring [0, size) */
    if (newEmit >= ring->size) {
        RING_DEBUG("ERROR: ringAdvance: cs=%p vaddr=%p newEmit=%u size=%u",
                   cs, ring->vaddr, newEmit, ring->size);
        return;
    }

    /*
     * Align tail to 8 bytes (qword) — HW requirement for Gen8+
     *
     * ⚠️ Gen8+ GPU requires RING_TAIL to be qword-aligned (bits 2:0 = 0).
     * TAIL_ADDR = GENMASK(20,3) = 0x001FFFF8 — only bits [20:3] matter.
     * If we don't align, the GPU may hang or mis-execute commands.
     *
     * Also: wrap both emit and tail to stay within [0, size).
     * Without wrapping, emit can reach ring->size (exactly at buffer end)
     * and tail can overflow past size when rounding up — both cause OOB.
     *
     * Linux: GEM_BUG_ON(!IS_ALIGNED(rq->ring->emit, 8));
     */
    ring->emit = ringWrap(ring, newEmit);
    ring->tail = ringWrap(ring, (ring->emit + 7) & ~7U);

    RING_TRACE("ringAdvance: emit=%u tail=%u (qword-aligned)", ring->emit, ring->tail);
}

/*
 * ─────────────────────────────────────────────
 *  lrcBuildContext — Build the Gen12 RCS LRC image
 * ─────────────────────────────────────────────
 *
 *  Fills the LRC context object (LRC_CONTEXT_SIZE = 4 pages) with the
 *  gen12_rcs_offsets register stream that i915 __lrc_init_regs() emits
 *  (intel_lrc.c:455-549). Layout per page:
 *    page 0 = PPHWSP (unused, left zeroed — only the 0x800 scratch area counts),
 *    LRC_STATE_OFFSET (0x1000) = register state (= ce->lrc_reg_state).
 *
 *  The register state is a mini-batch of MI_LOAD_REGISTER_IMM commands
 *  (i915 set_offsets(), intel_lrc.c:49-106). dword 0 MUST be MI_NOOP
 *  before the first MI_LRI (intel_gpu_commands.h:149). Register addresses
 *  are ABSOLUTE = engine mmio_base + (offset << 2) (set_offsets:95); every
 *  Gen11+ LRI header carries MI_LRI_LRM_CS_MMIO (bit19) — even the
 *  R_PWR_CLK_STATE LRI(1) that is NOT posted (set_offsets:80-81).
 *
 *  Emission = gen12_rcs_offsets verbatim (185 dwords = LRC_STATE_DWORDS):
 *    dw 0       : MI_NOOP
 *    dw 1-27    : LRI(13, POSTED)  CTX_* ring regs (0x244..0x2b4)
 *    dw 28-32   : NOP(5)
 *    dw 33-51   : LRI(9,  POSTED)  timestamp + PDP3..PDP0 (PDP0 = PML4)
 *    dw 52-58   : LRI(3,  POSTED)  0x1b0 0x5a8 0x5ac
 *    dw 59-64   : NOP(6)
 *    dw 65-67   : LRI(1,  flags=0) 0x0c8 R_PWR_CLK_STATE (NOT posted)
 *    dw 68-80   : NOP(13)
 *    dw 81-183  : LRI(51, POSTED)  0x588 x6, 0x028 0x09c 0x0c0 0x178 0x17c
 *                  0x358 0x170 0x150 0x154 0x158 0x41c, 0x600..0x67c, 0x068 0x084
 *    dw 184     : NOP(1)
 *
 *  Value slots are patched below by the CTX_* dword indices (= bspec off + 1,
 *  intel_lrc_reg.h) — the same indices lrcUpdateRingRegs() re-patches at
 *  submit time, so the ring regs are refreshed without rebuilding the image.
 */
static bool lrcBuildContext(MyIntelRing *ring)
{
    /* lrc_reg_state = vaddr + LRC_STATE_OFFSET (dwords) */
    uint32_t *state = (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET);

    /* Zero the whole state page first — zeroed dwords decode as MI_NOOP,
     * which the HW skips (i915 __lrc_init_regs() memsets the reg state on
     * inhibit). */
    bzero(state, LRC_STATE_DWORDS * sizeof(uint32_t));

    /* state[0] = MI_NOOP (required before first MI_LRI) */
    state[0] = MI_NOOP;

    /* ── Block 1: LRI(13,POSTED) — the CTX_* ring registers (dw 1-27) ──
     * gen12_rcs_offsets head (RING_CONTEXT_CONTROL..RING_CMD_CCTL).
     * Header: MI_LRI(13)|POSTED|LRM_CS_MMIO. Pairs: (reg<-, value). We set
     * value=0 at build; lrcUpdateRingRegs() patches the CTX_* value slots
     * at submit time.
     */
    {
        static const uint32_t regOffsets[13] = {
            0x244, /* RING_CONTEXT_CONTROL */
            0x034, /* RING_HEAD */
            0x030, /* RING_TAIL */
            0x038, /* RING_START */
            0x03C, /* RING_CTL */
            0x168, /* RING_MODE */
            0x140, /* RING_INSTPM */
            0x110, /* RING_BB_STATE (i915_reg.h: RING_IMR=0x150, NOT 0x110) */
            0x1C0, /* BB_PER_CTX_PTR */
            0x1C4, /* BB_STATE */
            0x1C8, /* BB_ADDR */
            0x180, /* RING_SECURE_BASE */
            0x2B4, /* RING_CMD_CCTL */
        };
        uint32_t idx = 1; /* dword 1 = first LRI header */
        state[idx++] = MI_LRI(13) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 13; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i]; /* absolute reg addr */
            state[idx++] = 0;                              /* value — patched below */
        }
    }

    /* ── Block 2: LRI(9,POSTED) — timestamp + PPGTT PDPs (dw 33-51) ──
     * gen12_rcs_offsets mid: 0x3a8 (RING_CTX_TIMESTAMP), then PDP3..PDP0.
     * 4-level PPGTT: PDP0 holds the PML4 page phys (ASSIGN_CTX_PML4); the
     * other PDP descriptors are ignored (intel_lrc.c). Value slots land at
     * CTX_PDP0_UDW=0x31 (pair 7) / CTX_PDP0_LDW=0x33 (pair 8).
     */
    {
        static const uint32_t regOffsets[9] = {
            0x3A8, /* RING_CTX_TIMESTAMP */
            0x28C, /* PDP3_UDW */
            0x288, /* PDP3_LDW */
            0x284, /* PDP2_UDW */
            0x280, /* PDP2_LDW */
            0x27C, /* PDP1_UDW */
            0x278, /* PDP1_LDW */
            0x274, /* PDP0_UDW — 4-level: PML4 phys */
            0x270, /* PDP0_LDW — 4-level: PML4 phys */
        };
        uint32_t idx = 33;
        state[idx++] = MI_LRI(9) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 9; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i];
            state[idx++] = 0;
        }
    }

    /* ── Block 3: LRI(3,POSTED) — indirect ctx regs (dw 52-58) ── */
    {
        static const uint32_t regOffsets[3] = {
            0x1B0, /* RING_INDIRECT_CTX */
            0x5A8, /* RING_INDIRECT_CTX_OFFSET */
            0x5AC, /* RING_INDIRECT_CTX_CTL */
        };
        uint32_t idx = 52;
        state[idx++] = MI_LRI(3) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 3; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i];
            state[idx++] = 0;
        }
    }

    /* ── Block 4: LRI(1) — R_PWR_CLK_STATE (dw 65-67), NOT posted ──
     * gen12_rcs_offsets emits this as LRI(1, 0) — no MI_LRI_FORCE_POSTED,
     * but Gen11+ still ORs MI_LRI_LRM_CS_MMIO (set_offsets()). Value slot
     * = CTX_R_PWR_CLK_STATE (0x43). i915 reads the current value from HW
     * (intel_read_ctx_r_pwr_clk); we have no HW state → 0 (default).
     */
    {
        uint32_t idx = 65;
        state[idx++] = MI_LRI(1) | MI_LRI_LRM_CS_MMIO; /* no FORCE_POSTED */
        state[idx++] = ring->mmioBase + 0x0C8;         /* R_PWR_CLK_STATE */
        state[idx++] = 0;
    }

    /* ── Block 5: LRI(51,POSTED) — remaining regs (dw 81-183) ──
     * gen12_rcs_offsets tail: 0x588 x6, 0x028..0x41c, 0x600..0x67c, 0x068
     * 0x084. Value slots: CTX_MI_MODE (0x61, pair of 0x09c RING_MI_MODE),
     * CTX_BB_OFFSET (0x71, pair of 0x158 RING_BB_OFFSET).
     */
    {
        static const uint32_t regOffsets[51] = {
            0x588, 0x588, 0x588, 0x588, 0x588, 0x588, /*  0- 5 */
            0x028, 0x09C, 0x0C0, 0x178, 0x17C, 0x358, /*  6-11 */
            0x170, 0x150, 0x154, 0x158, 0x41C,       /* 12-16 */
            0x600, 0x604, 0x608, 0x60C, 0x610, 0x614, 0x618, 0x61C,
            0x620, 0x624, 0x628, 0x62C, 0x630, 0x634, 0x638, 0x63C,
            0x640, 0x644, 0x648, 0x64C, 0x650, 0x654, 0x658, 0x65C,
            0x660, 0x664, 0x668, 0x66C, 0x670, 0x674, 0x678, 0x67C,
            0x068, 0x084, /* 49-50 */
        };
        uint32_t idx = 81;
        state[idx++] = MI_LRI(51) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 51; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i];
            state[idx++] = 0;
        }
    }

    /* dw 184 = NOP(1), left zeroed. Total emitted = LRC_STATE_DWORDS (185). */

    /* ── Value patches — mirrors i915 __lrc_init_regs() (inhibit=true) ── */
    /* For the FIRST load the HW requires CTX_CONTEXT_CONTROL to inhibit
     * the context (i915 init_common_regs with inhibit=true):
     *   ENABLE(INHIBIT_SYN_CTX_SWITCH) | DISABLE(RESTORE_INHIBIT) | RESTORE_INHIBIT
     */
    state[CTX_CONTEXT_CONTROL] = CTX_CONTROL_INHIBIT_INIT;
    state[CTX_RING_HEAD]       = 0;
    state[CTX_RING_TAIL]       = 0;
    state[CTX_RING_START]      = ring->ggttOffset;
    /* RING_CTL: size field at bits 13:11 = (pages-1) << RING_CTL_SIZE_SHIFT.
     * ⚠️ Do NOT use i915's RING_CTL_SIZE(size) = size - PAGE_SIZE (≡ shift 12) —
     * on this RPL silicon it decodes as 28KB and hangs. Same encoding as the
     * legacy MMIO path, empirically validated. */
    state[CTX_RING_CTL] = ((ring->size >> GEM_PAGE_SHIFT) - 1) << RING_CTL_SIZE_SHIFT
                        | RING_CTL_VALID;

    /* init_common_regs(): CTX_TIMESTAMP = ce->stats.runtime.last (0 on first load) */
    state[CTX_TIMESTAMP] = 0;

    /* init_ppgtt_regs() 4-level: ASSIGN_CTX_PML4 — PML4 page phys, UDW/LDW
     * (i915 init_ppgtt_regs intel_lrc.c, "other PDP descriptors are ignored").
     * UDW slot = upper 32 bits of the raw page phys. */
    state[CTX_PDP0_UDW] = ring->ppgttPml4Hi;
    state[CTX_PDP0_LDW] = ring->ppgttPml4Lo;

    /* __reset_stop_ring(): CTX_MI_MODE |= STOP_RING << 16 (masked write,
     * i915 mode = RING_MI_MODE_STOP_RING; clears the stop-ring bit) */
    state[CTX_MI_MODE] = (RING_MI_MODE_STOP_RING << 16);

    /* init_common_regs(): bb_offset slot = 0 */
    state[CTX_BB_OFFSET] = 0;

    OSSynchronizeIO();

    RING_DEBUG_RAW("lrcBuildContext: state@+0x%X ggtt=0x%X size=0x%X CTL=0x%X CC=0x%X",
                   LRC_STATE_OFFSET, ring->ggttOffset, ring->size,
                   state[CTX_RING_CTL], state[CTX_CONTEXT_CONTROL]);
    return true;
}

/*
 * ─────────────────────────────────────────────
 *  lrcBuildContextXcs — Build the Gen12 XCS (BCS) LRC image
 * ─────────────────────────────────────────────
 *  Fills the LRC context object with the gen12_xcs_offsets register stream
 *  (intel_lrc.c:227-257) — VERBATIM, 52 dwords = LRC_STATE_DWORDS_XCS:
 *    dw 0       : MI_NOOP
 *    dw 1-27    : LRI(13, POSTED)  0x244 0x034 0x030 0x038 0x03c 0x168
 *                  0x140 0x110 0x1c0 0x1c4 0x1c8 0x180 0x2b4
 *    dw 28-32   : NOP(5)
 *    dw 33-51   : LRI(9,  POSTED)  0x3a8 0x28c 0x288 0x284 0x280 0x27c
 *                  0x278 0x274 0x270   ← PDP3..PDP0 (PDP0 = PML4, 4-level)
 *  Blocks 1+2 are byte-identical to gen12_rcs_offsets (same CTX_* value-slot
 *  dword indices 0x03..0x33) — only blocks 3-5 are absent (XCS has no
 *  indirect-ctx regs, no R_PWR_CLK_STATE, no GPR block). Value patches
 *  mirror lrcBuildContext() for the slots that exist; CTX_MI_MODE (0x61)
 *  and CTX_BB_OFFSET (0x71) do NOT exist here.
 */
static bool lrcBuildContextXcs(MyIntelRing *ring)
{
    uint32_t *state = (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET);
    bzero(state, LRC_STATE_DWORDS_XCS * sizeof(uint32_t));

    state[0] = MI_NOOP;

    /* Block 1: LRI(13,POSTED) — identical reg list to RCS block 1 */
    {
        static const uint32_t regOffsets[13] = {
            0x244, /* RING_CONTEXT_CONTROL */
            0x034, /* RING_HEAD */
            0x030, /* RING_TAIL */
            0x038, /* RING_START */
            0x03C, /* RING_CTL */
            0x168, /* RING_MODE */
            0x140, /* RING_INSTPM */
            0x110, /* RING_BB_STATE */
            0x1C0, /* BB_PER_CTX_PTR */
            0x1C4, /* BB_STATE */
            0x1C8, /* BB_ADDR */
            0x180, /* RING_SECURE_BASE */
            0x2B4, /* RING_CMD_CCTL */
        };
        uint32_t idx = 1;
        state[idx++] = MI_LRI(13) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 13; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i];
            state[idx++] = 0;
        }
    }

    /* Block 2: LRI(9,POSTED) — timestamp + PDP3..PDP0 (PDP0 = PML4 phys) */
    {
        static const uint32_t regOffsets[9] = {
            0x3A8, /* RING_CTX_TIMESTAMP */
            0x28C, /* PDP3_UDW */
            0x288, /* PDP3_LDW */
            0x284, /* PDP2_UDW */
            0x280, /* PDP2_LDW */
            0x27C, /* PDP1_UDW */
            0x278, /* PDP1_LDW */
            0x274, /* PDP0_UDW — 4-level: PML4 phys */
            0x270, /* PDP0_LDW — 4-level: PML4 phys */
        };
        uint32_t idx = 33;
        state[idx++] = MI_LRI(9) | MI_LRI_FORCE_POSTED | MI_LRI_LRM_CS_MMIO;
        for (uint32_t i = 0; i < 9; i++) {
            state[idx++] = ring->mmioBase + regOffsets[i];
            state[idx++] = 0;
        }
    }

    /* Value patches — subset of lrcBuildContext() (slots that exist in XCS) */
    state[CTX_CONTEXT_CONTROL] = CTX_CONTROL_INHIBIT_INIT;
    state[CTX_RING_HEAD]       = 0;
    state[CTX_RING_TAIL]       = 0;
    state[CTX_RING_START]      = ring->ggttOffset;
    state[CTX_RING_CTL]        = ((ring->size >> GEM_PAGE_SHIFT) - 1) << RING_CTL_SIZE_SHIFT
                               | RING_CTL_VALID;
    state[CTX_TIMESTAMP]       = 0;
    state[CTX_PDP0_UDW]        = ring->ppgttPml4Hi;
    state[CTX_PDP0_LDW]        = ring->ppgttPml4Lo;

    OSSynchronizeIO();

    /* ── Indirect / per-context BB activation (gen12 XCS slot map) ──
     * i915 lrc_setup_indirect_ctx/bb_per_ctx target dword slots inside the
     * first LRI block: dw19 = BB_PER_CTX_PTR val, dw20 = BB_STATE val,
     * dw22 = BB_ADDR val. Without them (all zero) the element queues but
     * never dispatches - head frozen@80 signature. Regions: page2 =
     * INDIRECT_BB (BB_END-only minimal batch), page3 = PER_CTX_BB. */
    {
        /* Indirect BB page2: gen12 WA sequence + BB_END.
         * Each WA is MI_LOADs that restore GPR/timestamp state; without
         * them the XCS context image is considered incomplete and the
         * element never dispatches (h=0 fault signature). */
        /* Page math: img is dword*; each page = 512 dwords. State page =
         * page1 (img+512*2 elements). Indirect BB = page2 = img+512*4.
         * Earlier build used img+512*2 -> WROTE THE BATCH OVER THE STATE
         * PAGE (this exact dump proved it) -> restore parsed garbage. */
        uint32_t *img = (uint32_t *)((uint8_t *)ring->lrcVaddr);
        bzero(img + 512 * 4, 4096);
        bzero(img + 512 * 6, 4096);
        {
            uint32_t *cs = img + 512 * 4;
            /* gen12_emit_timestamp_wa: 3x MI_LOAD via GPR0 */
            uint32_t gpr0 = ring->mmioBase + 0x600;
            uint32_t tsReg = ring->mmioBase + 0x3a8;
            uint32_t tsSlot = ring->lrcGgtt + LRC_STATE_OFFSET + CTX_TIMESTAMP * 4;
            *cs++ = MI_LOAD_REGISTER_MEM_GEN8 | MI_SRM_LRM_GLOBAL_GTT | MI_LRI_LRM_CS_MMIO;
            *cs++ = gpr0;
            *cs++ = tsSlot; *cs++ = 0;
            *cs++ = MI_LOAD_REGISTER_REG | MI_LRR_SOURCE_CS_MMIO | MI_LRI_LRM_CS_MMIO;
            *cs++ = gpr0; *cs++ = tsReg;
            *cs++ = MI_LOAD_REGISTER_REG | MI_LRR_SOURCE_CS_MMIO | MI_LRI_LRM_CS_MMIO;
            *cs++ = gpr0; *cs++ = tsReg;
            *cs++ = MI_BATCH_BUFFER_END;
        }
        img[512 * 3 + 0] = MI_BATCH_BUFFER_END;

        // DISABLE indirect BB for now — test if HW dispatches without it
        // (gen12 XCS may not require it; our BB with only BB_END may be confusing HW)
        state[19] = 0; state[21] = 0; state[23] = 0;
    }

    RING_DEBUG_RAW("lrcBuildContextXcs: state@+0x%X ggtt=0x%X size=0x%X CTL=0x%X CC=0x%X (%u dw)",
                   LRC_STATE_OFFSET, ring->ggttOffset, ring->size,
                   state[CTX_RING_CTL], state[CTX_CONTEXT_CONTROL], LRC_STATE_DWORDS_XCS);
    return true;
}


/*
 * ─────────────────────────────────────────────
 *  lrcAllocPPGTT — Allocate + populate the identity-map PPGTT
 * ─────────────────────────────────────────────
 *  Layout (4-level, DESC_LEGACY_64B): PML4 → PDP → PD → 128 PT pages.
 *  131 GGTT pages = 1 PML4 + 1 PDP + 1 PD + 128 PT pages (page 0=PML4,
 *  page 1=PDP, page 2=PD, pages 3..130=PT). All entries are u64:
 *  PML4[0] → PDP page phys, PDP[0] → PD page phys, PD[i] → PT page i+3 phys
 *  (each PT page = 512 PTEs × 4KB = 2MB window → total 256MB identity map
 *  covering the GGTT aperture, batch VA == GGTT offset).
 *
 *  Root wiring is filled once here; PTEs are filled lazily by
 *  lrcMapBatchPages() at batch submit time.
 *
 *  MUST run BEFORE lrcBuildContext() — PML4 phys is patched into the LRC
 *  value slots CTX_PDP0_UDW/LDW (0x31/0x33) at build time.
 */
bool lrcAllocPPGTT(MyIntelRing *ring, const MyIntelRingCallbacks *cb)
{
    if (!ring || !cb || !cb->gemAlloc || !cb->gemGetPagesPhys) {
        RING_DEBUG_RAW("lrcAllocPPGTT: missing callbacks — PPGTT disabled");
        return false;
    }

    ring->ppgttGemBuf = cb->gemAlloc(cb->context,
                                     PPGTT_TOTAL_PAGES * GEM_PAGE_SIZE,
                                     GEM_FLAG_RING | GEM_FLAG_CPU_WRITE);
    if (ring->ppgttGemBuf == NULL) {
        RING_DEBUG_RAW("lrcAllocPPGTT: gemAlloc(%u pages) FAILED — PPGTT disabled",
                       PPGTT_TOTAL_PAGES);
        return false;
    }

    ring->ppgttGgtt  = cb->gemGetOffset(cb->context, ring->ppgttGemBuf);
    ring->ppgttVaddr = (uint64_t *)cb->gemGetVAddr(cb->context, ring->ppgttGemBuf);
    if (ring->ppgttVaddr == NULL) {
        RING_DEBUG_RAW("lrcAllocPPGTT: gemGetVAddr FAILED — PPGTT disabled");
        return false;
    }

    /* Zero ALL pages first — zeroed entries are not-present (safe); PML4/PDP
     * upper dwords beyond the used slots must read as not-present too. */
    bzero(ring->ppgttVaddr, PPGTT_TOTAL_PAGES * GEM_PAGE_SIZE);

    /* Page map: 0=PML4, 1=PDP, 2=PD, 3..130=PT (u64 entries, 512/page) */
    ring->ppgttPdp = ring->ppgttVaddr + 512;          /* page 1 = PDP */
    ring->ppgttPTs = ring->ppgttVaddr + 3 * 512;      /* page 3 = PT page 0 */

    const uint64_t *pagesPhys = cb->gemGetPagesPhys(cb->context, ring->ppgttGemBuf);
    if (pagesPhys == NULL) {
        RING_DEBUG_RAW("lrcAllocPPGTT: gemGetPagesPhys FAILED — PPGTT disabled");
        return false;
    }

    /* PDP0 regs = PML4 page phys (4-level: ASSIGN_CTX_PML4 — i915
     * init_ppgtt_regs writes the raw page phys, no flags) */
    ring->ppgttPml4Lo = (uint32_t)(pagesPhys[0] & 0xFFFFFFFFu);
    ring->ppgttPml4Hi = (uint32_t)(pagesPhys[0] >> 32);

    /* PDE/PTE flags:
     * PDE = PRESENT|RW (i915 PPAT_CACHED_PDE=0)
     * PTE = PRESENT|RW|_PAGE_PAT(0x80) — i915 gen8_pte_encode default WB
     *       (PPAT_CACHED). Old encode 0x3 left the cache field at PPAT[000];
     *       fetch tolerated it, stores silently vanished. */
    ring->ppgttPdeEncode = GEN8_PAGE_PRESENT | GEN8_PAGE_RW;
    uint32_t pteEnc = GEN8_PAGE_PRESENT | GEN8_PAGE_RW | 0x80;   /* = 0xC3 (i915-faithful) */
#ifndef KERNEL
    /* A/B variants, user-space test builds only: MFX_PTEVAR=2 -> USER+0x20 */
    if (const char *pev = getenv("MFX_PTEVAR")) {
        if (strtoul(pev, NULL, 0) == 2)
            pteEnc = GEN8_PAGE_PRESENT | GEN8_PAGE_RW | (1u << 2) | (0x02u << 4);
    }
#endif
    ring->ppgttPteEncode = pteEnc;

    /* 4-level root wiring (u64 entries = phys | PRESENT|RW): */
    ring->ppgttVaddr[0] = (uint64_t)pagesPhys[1] | ring->ppgttPdeEncode;  /* PML4[0] → PDP */
    ring->ppgttPdp[0]   = (uint64_t)pagesPhys[2] | ring->ppgttPdeEncode;  /* PDP[0]  → PD  */
    for (uint32_t i = 0; i < PPGTT_PT_PAGES; i++) {
        ring->ppgttVaddr[2 * 512 + i] = (uint64_t)pagesPhys[3 + i]        /* PD[i] → PT i */
                                      | ring->ppgttPdeEncode;
    }

    OSSynchronizeIO();

    RING_DEBUG_RAW("lrcAllocPPGTT: OK — %u pages @ ggtt=0x%X vaddr=%p PDP0=0x%X%08X",
                   PPGTT_TOTAL_PAGES, ring->ppgttGgtt, ring->ppgttVaddr,
                   ring->ppgttPml4Hi, ring->ppgttPml4Lo);
    return true;
}

/*
 * ─────────────────────────────────────────────
 *  lrcMapBatchPages — Write PTEs for a batch buffer (identity map)
 * ─────────────────────────────────────────────
 *  Maps pageCount physical pages at VA == batchGGTT (identity: batch VA =
 *  GGTT offset). Called at submit time, under the engine lock, BEFORE the
 *  kick that emits BB_START.
 */
bool lrcMapBatchPages(MyIntelRing *ring, uint32_t batchGGTT,
                      const uint64_t *pagesPhys, uint32_t pageCount)
{
    if (!ring || !ring->ppgttPTs || !pagesPhys || pageCount == 0) {
        return false;
    }

    /* Guard: batch must fit inside the 256MB identity window */
    const uint64_t end = (uint64_t)batchGGTT + (uint64_t)pageCount * GEM_PAGE_SIZE;
    if (end > PPGTT_WINDOW_SIZE) {
        RING_DEBUG_RAW("lrcMapBatchPages: batch 0x%X+%u pages exceeds 256MB window",
                       batchGGTT, pageCount);
        return false;
    }

    for (uint32_t i = 0; i < pageCount; i++) {
        const uint32_t va     = batchGGTT + i * (uint32_t)GEM_PAGE_SIZE;
        const uint32_t ptIdx  = va >> 21;          /* 2MB per PT page */
        const uint32_t pteIdx = (va >> 12) & 511;  /* PTE within PT page */
        const uint64_t physAddr = pagesPhys[i] & ~0xFFFull;   /* page-granular clear */
        ring->ppgttPTs[ptIdx * 512 + pteIdx] =
            physAddr | ring->ppgttPteEncode;
    }

    OSSynchronizeIO();

    /* [PPGTT-AUDIT] one-shot dump: hierarchy + first mapped PTEs vs phys[] */
    {
        const uint32_t ptIdx0 = batchGGTT >> 21;
        RING_DEBUG_RAW("PPGTT-AUDIT: ggtt=0x%X n=%u | PML4[0]=%llX PDP[0]=%llX PD[%u]=%llX",
                       batchGGTT, pageCount,
                       ring->ppgttVaddr[0], ring->ppgttPdp[0],
                       ptIdx0, ring->ppgttVaddr[2 * 512 + ptIdx0]);
        for (uint32_t i = 0; i < pageCount && i < 3; i++) {
            const uint32_t va  = batchGGTT + i * (uint32_t)GEM_PAGE_SIZE;
            const uint64_t pte = ring->ppgttPTs[(va >> 21) * 512 + ((va >> 12) & 511)];
            RING_DEBUG_RAW("PPGTT-AUDIT: VA=0x%X PTE=%llX (phys=%llX flags=%s)",
                           va, pte, pagesPhys[i],
                           (pte & 0xFFFFFFFFFFFFF000ull) == (pagesPhys[i] & 0xFFFFFFFFFFFFF000ull)
                               ? "match" : "MISMATCH");
        }
    }

    return true;
}

/*
 * ─────────────────────────────────────────────
 *  lrcUpdateRingRegs — Patch LRC ring regs before submit
 * ─────────────────────────────────────────────
 *  Mirrors i915 lrc_update_regs(): refresh head/tail/start/ctl inside the
 *  context image so the execlist load picks up the current ring state.
 */
static void lrcUpdateRingRegs(MyIntelRing *ring)
{
    uint32_t *state = (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET);

    state[CTX_RING_HEAD]  = ring->head;
    state[CTX_RING_TAIL]  = ring->tail;
    state[CTX_RING_START] = ring->ggttOffset;
    state[CTX_RING_CTL]   = ((ring->size >> GEM_PAGE_SHIFT) - 1) << RING_CTL_SIZE_SHIFT
                          | RING_CTL_VALID;

    OSSynchronizeIO();
}

/*
 * ─────────────────────────────────────────────
 *  ringSubmitExeclists — Gen12+ execlist submission
 * ─────────────────────────────────────────────
 *
 *  Mirrors i915 write_desc() + execlists_submit_ports() on Gen12 (no ELSP):
 *    1. lrc_update_regs() — patch CTX_RING_HEAD/TAIL/START/CTL in LRC
 *    2. write descriptor LOW dword first  → RING_EXECLIST_SQ_CONTENTS (0x510)
 *    3. write descriptor HIGH dword       → SQ_CONTENTS + 4 (0x514)
 *    4. EL_CTRL_LOAD (bit0)               → RING_EXECLIST_CONTROL  (0x550)
 *    5. posting read + dump CSB + EXECLIST_STATUS for the log
 *
 *  Descriptor (low dword) = LRCA (GGTT offset, bits 12-31, NO shift)
 *    | DESC_VALID | DESC_PRIVILEGE | DESC_LEGACY_64B | DESC_FORCE_RESTORE.
 */
void ringSubmitExeclists(MyIntelRing *ring, MyIntelRingCallbacks *cb)
{
    if (!ring || ring->magic != RING_MAGIC || !cb || !cb->writeReg32) {
        return;
    }
    if (!ring->lrcInited || !ring->lrcVaddr) {
        RING_DEBUG_RAW("ringSubmitExeclists: LRC not initialized — falling back to legacy submit");
        ringSubmit(ring, cb);
        return;
    }

    lrcUpdateRingRegs(ring);

    uint32_t base = ring->mmioBase;

    /* Descriptor low dword: LRCA = GGTT offset page-aligned. FLAGS below. */
    uint32_t descLo = (ring->lrcGgtt & DESC_GTT_ADDRESS_MASK)
                    | DESC_VALID
                    | DESC_PRIVILEGE
                    | DESC_LEGACY_64B
                    | DESC_FORCE_RESTORE;
    /* Descriptor high dword: engine class in bits 63:61 (i915
     * GEN11_ENGINE_CLASS_SHIFT=61, intel_lrc.h:105) + instance bits 53:48
     * + sw ctx id bits 47:37 (GEN11_SW_CTX_ID_SHIFT=37, nonzero; i915 uses
     * 1+tag → we use 1). hi dword = bits 63:32:
     *   class shift 61-32=29, sw_ctx_id shift 37-32=5.
     * HW classes (intel_engine_types.h): RENDER=0 VIDEO_DECODE=2 COPY=3
     * VIDEO_ENHANCE=4 — 2.0.250: VCS previously sent class=0 (render),
     * fixed to VIDEO_DECODE per i915 execlists->ccid (gen11..12.54). */
    uint32_t hwClass = 0u;
    switch (ring->engineType) {
        case kMyIntelEngineVCS:  hwClass = 2u; break;  /* VIDEO_DECODE */
        case kMyIntelEngineBCS:  hwClass = 3u; break;  /* COPY_ENGINE */
        case kMyIntelEngineVECS: hwClass = 4u; break;  /* VIDEO_ENHANCE */
        default:                 hwClass = 0u; break;  /* RENDER (RCS) */
    }
    /* A/B probe (case file a272fd1): mybcstest=N overrides BCS class so one
     * boot tests an alternate encoding — 1=VIDEO_DECODE, 2=VIDEO_ENHANCE,
     * 4/5=OTHER/COMPUTE. Default stays 3 (COPY) when arg absent or zero. */
    if (ring->engineType == kMyIntelEngineBCS) {
        uint32_t clsProbe = 0;
        if (PE_parse_boot_argn("mybcstest", &clsProbe, sizeof(clsProbe)) && clsProbe && clsProbe <= 5)
            hwClass = clsProbe;
    }
    uint32_t descHi = (hwClass << (61 - 32))   /* class, instance = 0 */
                    | (1u << (37 - 32));        /* sw ctx id = 1 */

    bool fwHeld = (cb->fwGet != NULL) && cb->fwGet(cb->context);

    /* 2.0.215: forceWakeGet now fails (returns false) on persistent ACK
     * timeout instead of faking success — a dropped ELSP write leaves
     * desc=0x0/HEAD=0 (E2 hung-boot signature). Abort the submit and leave
     * the ring stalled rather than program a sleeping engine. */
    if (!fwHeld) {
        RING_DEBUG_RAW("ringSubmitExeclists: ABORT — forcewake acquire failed; ELSP write would be dropped");
        return;
    }

    /* write_desc(): LOW dword first, then HIGH (Gen12 SQ_CONTENTS order) */
    cb->writeReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET, descLo);
    cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET); /* posting */
    cb->writeReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET + 4, descHi);
    cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET + 4); /* posting */

    /* Kick: EL_CTRL_LOAD — i915 execlists_submit_ports() on Gen12 */
    cb->writeReg32(cb->context, base + RING_EXECLIST_CONTROL_OFFSET, EL_CTRL_LOAD);
    cb->readReg32(cb->context, base + RING_EXECLIST_CONTROL_OFFSET); /* posting read */

    /* ── POST-KICK POLL: 2.0.15 probe read immediately after EL_CTRL_LOAD
     * always saw CSBwr=0/ACTIVE=0/HEAD=0 even though CSB[00] contained a
     * HW-written 0x03FF8000_00000001 (ctx-to-valid) — the engine needs a few
     * microseconds to load the LRC and start fetching. Poll up to ~10ms and
     * break as soon as the engine shows real progress (ACTIVE, CSB write
     * pointer advanced from its reset value 11, or HEAD moving). */
    /* 2.0.229: IRQ-path submit — the engine that raised USER IRQ is awake,
     * so the poll loop below (10 × 1ms = 10ms block) must not run in the
     * workloop gate. i915 gen11_gt_irq_handler() never polls after submit;
     * completion arrives via the USER_INTERRUPT → IRQ chain. Do a single
     * immediate read for the boot log instead. */
    bool inIrq = (cb->inIrqContext != NULL) && cb->inIrqContext(cb->context);

    uint32_t pollActive = 0, pollCsb = 0, pollHead = 0;
    int pollLoops = 0;
    if (!inIrq) {
        for (pollLoops = 0; pollLoops < 10; pollLoops++) {
            IODelay(1000); /* 1ms */
            uint32_t elSt = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_LO_OFFSET);
            pollActive = elSt & EXECLIST_STATUS_ELEMENT_ACTIVE;
            pollCsb = ring->hwspVaddr ? ring->hwspVaddr[HWS_CSB_WRITE_DWORD] : 0;
            pollHead = cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET) & 0xFFFFFFFC;
            if (pollActive || pollCsb != (HWS_CSB_ENTRIES_GEN11 - 1) || pollHead != 0) {
                break;
            }
        }
    } else {
        uint32_t elSt = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_LO_OFFSET);
        pollActive = elSt & EXECLIST_STATUS_ELEMENT_ACTIVE;
        pollCsb = ring->hwspVaddr ? ring->hwspVaddr[HWS_CSB_WRITE_DWORD] : 0;
        pollHead = cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET) & 0xFFFFFFFC;
    }
    RING_DEBUG_RAW("ringSubmitExeclists: post-kick poll ACTIVE=%d CSBwr=0x%X HEAD=0x%X loops=%d",
                   (pollActive != 0), pollCsb, pollHead, pollLoops + 1);

    if (fwHeld) cb->fwPut(cb->context);

    ring->lastTail = ring->tail;

    /* Post-submit ground truth for the boot log: CSB (HWSP) + EXECLIST_STATUS */
    {
        uint32_t csbWrite = ring->hwspVaddr ? ring->hwspVaddr[HWS_CSB_WRITE_DWORD] : 0;
        uint32_t execLo   = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_LO_OFFSET);
        uint32_t execHi   = cb->readReg32(cb->context, base + RING_EXECLIST_STATUS_HI_OFFSET);
        uint64_t descRead = ((uint64_t)cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET + 4) << 32)
                          |  cb->readReg32(cb->context, base + RING_EXECLIST_SQ_CONTENTS_OFFSET);
        RING_DEBUG_RAW("ringSubmitExeclists: desc=0x%016llX tail=%u CSBwr=%u ACTIVE=%u",
                       descRead, ring->tail, csbWrite, execLo & EXECLIST_STATUS_ELEMENT_ACTIVE);
        RING_DEBUG_RAW("ringSubmitExeclists: EXECLIST_STATUS_HI=0x%08X",
                       execHi);

        /* Engine fault diagnosis (i915 intel_engine_regs.h + intel_gt_regs.h):
         *  - HEAD advancing ⇒ engine fetched/executed ring commands
         *  - HEAD == 0 after kick ⇒ submit not executed (fault before run → GGTT/LRCA)
         *  - EIR/ESR != 0 ⇒ fault during execution → IPEIR/IPEHR hold faulting
         *    instruction (gen8+); GT fault reg (0xCEC4 Gen12) for GGTT faults. */
        uint32_t ringHead = cb->readReg32(cb->context, base + RING_HEAD_REG_OFFSET);
        uint32_t ringTail = cb->readReg32(cb->context, base + RING_TAIL_REG_OFFSET);
        uint32_t ringEir  = cb->readReg32(cb->context, base + RING_EIR_OFFSET);
        uint32_t ringEsr  = cb->readReg32(cb->context, base + RING_ESR_OFFSET);
        uint32_t ringIpeir = cb->readReg32(cb->context, base + RING_IPEIR_OFFSET);
        uint32_t ringIpehr = cb->readReg32(cb->context, base + RING_IPEHR_OFFSET);
        uint32_t gtFault   = cb->readReg32(cb->context, GEN12_RING_FAULT_REG_OFFSET);
        RING_DEBUG_RAW("ringSubmitExeclists: HEAD=0x%X TAIL=0x%X EIR=0x%08X ESR=0x%08X IPEIR=0x%08X IPEHR=0x%08X",
                       ringHead & 0xFFFFFFFC, ringTail & 0xFFFFFFFC,
                       ringEir, ringEsr, ringIpeir, ringIpehr);
        RING_DEBUG_RAW("ringSubmitExeclists: FAULT_GEN12=0x%08X%s",
                       gtFault, (gtFault & RING_FAULT_VALID_BIT) ? " VALID" : "");

        /* Raw CSB ring dump (Gen11+: u64 entries at HWSP dword 0x10, 12 slots,
         * write ptr at HWSP dword 0x2f). HW writes context-switch events here;
         * 0xFFFFFFFF/0 pattern + write ptr reset ⇒ engine rejected the context. */
        if (ring->hwspVaddr) {
            uint32_t *hwsp = ring->hwspVaddr;
            for (int i = 0; i < HWS_CSB_ENTRIES_GEN11; i++) {
                uint32_t lo = hwsp[HWS_CSB_BUF0_DWORD + (i * 2)];
                uint32_t hi = hwsp[HWS_CSB_BUF0_DWORD + (i * 2) + 1];
                RING_DEBUG_RAW("ringSubmitExeclists: CSB[%02d]=0x%08X_%08X",
                               i, hi, lo);
            }
        }
    }
}

/*
 * ─────────────────────────────────────────────
 *  ringSubmit — Write RING_TAIL register = kick GPU
 * ─────────────────────────────────────────────
 *
 *  On Gen12+ with a built LRC (lrcInited) this routes to
 *  ringSubmitExeclists() — the RING_TAIL MMIO path below is the legacy
 *  Gen8-and-earlier fallback and is not honored by Gen12 silicon.
 *
 *  Reference: Linux i9xx_submit_request()
 *    ENGINE_WRITE(engine, RING_TAIL, intel_ring_set_tail(ring, request->tail));
 */
void ringSubmit(MyIntelRing *ring, MyIntelRingCallbacks *cb)
{
    if (!ring || ring->magic != RING_MAGIC) {
        return;
    }
    if (!cb || !cb->writeReg32) {
        RING_DEBUG("ERROR: ringSubmit: no writeReg32 callback");
        return;
    }
    if (!ring->initialized) {
        return;
    }

    /* Gen12+: submit through the execlists LRC path (SQ_CONTENTS + EL_CTRL_LOAD).
     * Legacy RING_TAIL write is NOT supported on Gen8+ (i915 GEM_BUG_ON
     * GRAPHICS_VER >= 8) — the ring runs only when loaded via a context. */
    if (ring->lrcInited) {
        ringSubmitExeclists(ring, cb);
        return;
    }

    /*
     * Write RING_TAIL with the current tail byte offset.
     *
     * ⚠️ Gen8+ HW requires RING_TAIL to be qword-aligned (bits 2:0 = 0).
     * ring->tail is already qword-aligned from ringAdvance, but we
     * mask it here defensively to prevent GPU hangs from any stray
     * unaligned value that might have been set directly.
     *
     * Reference: Intel PRM Vol 2c — RING_TAIL register definition
     *   "Bits 2:0 of this register are reserved. SW must initialize
     *    them to 0. Writes to these bits are ignored."
     */
    uint32_t tailValue = ring->tail & ~0x7U;

    /*
     * Write barrier: ensure all ring writes are visible to GPU
     * before we tell it to read them
     */
    OSSynchronizeIO();

    bool fwHeld = (cb->fwGet != NULL) && cb->fwGet(cb->context);

    cb->writeReg32(cb->context, ring->mmioBase + RING_TAIL_REG_OFFSET, tailValue);

    /*
     * Posting read — ensure the MMIO write reaches the GPU
     * before we proceed (prevents reordering)
     */
    cb->readReg32(cb->context, ring->mmioBase + RING_TAIL_REG_OFFSET);

    if (fwHeld) cb->fwPut(cb->context);

    ring->lastTail = tailValue;

    RING_TRACE("ringSubmit: TAIL=%u size=%u", tailValue, ring->size);
}

/*
 * ─────────────────────────────────────────────
 *  High-Level Command Emission Helpers
 * ─────────────────────────────────────────────
 */

void ringResetSoftware(MyIntelRing *ring)
{
    if (!ring || ring->magic != RING_MAGIC) {
        return;
    }

    ring->head  = 0;
    ring->tail  = 0;
    ring->emit  = 0;
    ring->space = ring->size - RING_MIN_FREE_SPACE;

    /* HW restores head/tail from the LRC image on FORCE_RESTORE — zero them
     * there too or it refetches stale bytes from the old position */
    if (ring->lrcInited && ring->lrcVaddr) {
        uint32_t *state = (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET);
        state[CTX_RING_HEAD] = 0;
        state[CTX_RING_TAIL] = 0;
        OSSynchronizeIO();
    }

    RING_DEBUG("ringResetSoftware: cursors zeroed, space=%u", ring->space);
}

bool ringEmitNOOP(MyIntelRing *ring)
{
    uint32_t *cs = ringBegin(ring, 1);
    if (!cs) return false;
    *cs++ = MI_NOOP;
    ringAdvance(ring, cs);
    return true;
}

bool ringEmitUserInterrupt(MyIntelRing *ring)
{
    uint32_t *cs = ringBegin(ring, 1);
    if (!cs) return false;
    *cs++ = MI_USER_INTERRUPT;
    ringAdvance(ring, cs);
    return true;
}

/* Gen9 XY_FAST_COPY_BLT 10-dw — same encoding as the proven
 * emitBcsBlitCopy() path, generalized for arbitrary pitch/rect. */
bool ringEmitSurfacePresent(MyIntelRing *bcs,
                            uint32_t dstGgtt, uint32_t dstPitchBytes,
                            uint32_t srcGgtt, uint32_t srcPitchBytes,
                            uint16_t wPx, uint16_t hPx)
{
    if (!bcs || !ringIsInitialized(bcs))
        return false;
    if ((dstGgtt & (PAGE_SIZE - 1)) || (srcGgtt & (PAGE_SIZE - 1)))
        return false;

    uint32_t *cs = ringBegin(bcs, 10);
    if (!cs) return false;

    cs[0] = GEN9_XY_FAST_COPY_BLT_10DW;
    cs[1] = BLT_DEPTH_32 | dstPitchBytes;   /* dst pitch */
    cs[2] = 0;                              /* dst x1,y1 */
    cs[3] = ((uint32_t)hPx << 16) | wPx;    /* dst y2,x2 */
    cs[4] = dstGgtt;
    cs[5] = 0;                              /* dst instance */
    cs[6] = 0;                              /* src x1,y1 */
    cs[7] = srcPitchBytes;                  /* src pitch */
    cs[8] = srcGgtt;
    cs[9] = 0;                              /* src instance */

    ringAdvance(bcs, cs + 10);

    RING_DEBUG_RAW("ringEmitSurfacePresent: src=0x%X dst=0x%X %ux%u",
                   srcGgtt, dstGgtt, wPx, hPx);
    return true;
}

bool ringEmitBatchStart(MyIntelRing *ring, uint32_t va)
{
    /* i915 gen8_emit_bb_start parity — 4 dwords, qword aligned:
     *   MI_ARB_ON_OFF|MI_ARB_ENABLE + BB_START_GEN8|non-secure + lo + hi
     * Non-secure bit (1<<8): without it the parser rejects the jump. */
    uint32_t *cs = ringBegin(ring, 4);
    if (!cs) return false;

    *cs++ = MI_ARB_ON_OFF | MI_ARB_ENABLE;
    *cs++ = MI_BATCH_BUFFER_START_GEN8 | MI_BATCH_BUFFER_START_NONSEC;
    *cs++ = va;                     /* lower 32 bits (identity → GGTT offset) */
    *cs++ = 0;                      /* upper 32 bits (32-bit PPGTT) */

    ringAdvance(ring, cs);
    RING_DEBUG_RAW("ringEmitBatchStart: va=0x%X (4 dwords)", va);
    return true;
}

bool ringEmitFlushDW(MyIntelRing *ring, bool flushGFX, bool flushMedia)
{
    /*
     * MI_FLUSH_DW — Gen12 xcs form, VERIFIED against torvalds/linux
     * master 2026-08-11 (gt/gen8_engine_cs.c gen8_emit_flush_xcs() +
     * gt/intel_gpu_commands.h):
     *   cmd = MI_FLUSH_DW + 1 | MI_FLUSH_DW_OP_STOREDW | MI_FLUSH_DW_STORE_INDEX
     *       = 0x13000002 | (1<<14) | (1<<21) = 0x13214002  (4 dwords)
     *   dword0 = cmd        — flags live in dword0, NOT dword1
     *   dword1 = LRC_PPHWSP_SCRATCH_ADDR (0x800, PPHWSP offset; STORE_INDEX
     *            makes it a PPHWSP offset, not a GGTT address)
     *   dword2 = 0          — upper store address
     *   dword3 = 0          — value written to PPHWSP scratch
     *
     * flushGFX/flushMedia have NO encoding on Gen12 MI_FLUSH_DW — there
     * are no GFX/MEDIA/LLC "flush flag" bits (the old kext dword1 encoding
     * 0x07000007 was invented and desynced the command parser). Params kept
     * for API compatibility.
     */
    (void)flushGFX;
    (void)flushMedia;

    uint32_t *cs = ringBegin(ring, 4);
    if (!cs) return false;

    *cs++ = MI_FLUSH_DW_GEN12;
    *cs++ = LRC_PPHWSP_SCRATCH_ADDR;
    *cs++ = 0;  /* upper store address */
    *cs++ = 0;  /* store value */

    ringAdvance(ring, cs);
    return true;
}

bool ringEmitRaw(MyIntelRing *ring, const uint32_t *cmds, uint32_t dwords)
{
    if (!cmds || dwords == 0) return false;

    uint32_t *cs = ringBegin(ring, dwords);
    if (!cs) return false;

    for (uint32_t i = 0; i < dwords; i++) {
        *cs++ = cmds[i];
    }

    ringAdvance(ring, cs);
    return true;
}

/*
 * ─────────────────────────────────────────────
 *  Batch-Content Emitters (F10 compute/blit proofs)
 * ─────────────────────────────────────────────
 *
 * These write into a CLIENT batch buffer (not the ring itself): the ring
 * BB_STARTs into the buffer, which holds the full command stream ending in
 * MI_BATCH_BUFFER_END. Same command set the client stages manually today
 * (MyIntelGPUClient.cpp:166-190), factored per taskType so the client can
 * pick the proof without duplicating i915 encodings.
 */

uint32_t emitBcsBlitCopy(uint32_t *dst, uint32_t dstAddr,
                         uint32_t srcAddr, uint32_t bytes)
{
    if (!dst || (bytes & (PAGE_SIZE - 1)) != 0 || bytes == 0) return 0;
    if ((dstAddr & (PAGE_SIZE - 1)) != 0 || (srcAddr & (PAGE_SIZE - 1)) != 0) return 0;

    /* i915 emit_copy (intel_migrate.c:591-599), instance = 0 */
    uint32_t height = bytes >> PAGE_SHIFT;
    dst[0] = GEN9_XY_FAST_COPY_BLT_10DW;
    dst[1] = BLT_DEPTH_32 | PAGE_SIZE;          /* dst pitch */
    dst[2] = 0;                                 /* dst x1,y1 */
    dst[3] = (height << 16) | (PAGE_SIZE / 4);  /* dst x2,y2 = h<<16 | w */
    dst[4] = dstAddr;
    dst[5] = 0;                                 /* instance */
    dst[6] = 0;                                 /* src x1,y1 */
    dst[7] = PAGE_SIZE;                         /* src pitch */
    dst[8] = srcAddr;
    dst[9] = 0;                                 /* instance */
    dst[10] = MI_BATCH_BUFFER_END;
    dst[11] = MI_NOOP;                          /* pad → 12 dw, qword aligned */

    RING_DEBUG_RAW("emitBcsBlitCopy: src=0x%X dst=0x%X bytes=%u (h=%u w=%u)",
                   srcAddr, dstAddr, bytes, height, PAGE_SIZE / 4);
    return 12;
}

uint32_t emitMiMathProof(uint32_t *dst, uint32_t storeAddr,
                         uint32_t operandA, uint32_t operandB)
{
    if (!dst) return 0;

    /* CS_GPR0 = RCS base (0x2000) + 0x600, CS_GPR1 = +0x608 — see
     * MI_CS_GPR0_RCS in MyIntelRing.hpp. */
    const uint32_t gpr0 = MI_CS_GPR0_RCS;
    const uint32_t gpr1 = gpr0 + 8;

    uint32_t n = 0;
    dst[n++] = MI_LOAD_REGISTER_IMM(1);         /* LRI CS_GPR0 = A */
    dst[n++] = gpr0;
    dst[n++] = operandA;
    dst[n++] = MI_LOAD_REGISTER_IMM(1);         /* LRI CS_GPR1 = B */
    dst[n++] = gpr1;
    dst[n++] = operandB;
    dst[n++] = MI_MATH(4);                      /* 1 header + 4 ALU dwords */
    dst[n++] = MI_MATH_LOAD(MI_MATH_REG_SRCA, MI_MATH_REG(0));   /* SRCA ← GPR0 */
    dst[n++] = MI_MATH_LOAD(MI_MATH_REG_SRCB, MI_MATH_REG(1));   /* SRCB ← GPR1 */
    dst[n++] = MI_MATH_ADD;                     /* ACCU = SRCA + SRCB */
    dst[n++] = MI_MATH_STORE(MI_MATH_REG(0), MI_MATH_REG_ACCU);  /* GPR0 ← ACCU */
    dst[n++] = MI_STORE_REGISTER_MEM_GEN8;      /* readback GPR0 → storeAddr */
    dst[n++] = gpr0;
    dst[n++] = storeAddr;
    dst[n++] = 0;                               /* addr hi */
    dst[n++] = MI_BATCH_BUFFER_END;

    RING_DEBUG_RAW("emitMiMathProof: A=0x%X B=0x%X store=0x%X (expect sum 0x%X)",
                   operandA, operandB, storeAddr, operandA + operandB);
    return n;
}

uint32_t emitPipeControlFlush(uint32_t *dst, uint32_t storeAddr, uint32_t magic)
{
    if (!dst) return 0;

    /* PC1 — flush: gen12_emit_fini_breadcrumb_rcs parity. HDC_PIPELINE_FLUSH
     * is a DWORD0 flag (gen12_emit_pipe_control bit_group_0); the cache set
     * goes in DWORD1. gen8_engine_cs.c:844 + :825-831. */
    dst[0] = GFX_OP_PIPE_CONTROL(6) | PIPE_CONTROL0_HDC_PIPELINE_FLUSH;
    dst[1] = PIPE_CONTROL_GEN12_RCS_FLUSH;
    dst[2] = 0;                                 /* offset — no post-sync write */
    dst[3] = 0;
    dst[4] = 0;
    dst[5] = 0;

    /* PC2 — QW_WRITE store (breadcrumb): __gen8_emit_write_rcs
     * (gen8_engine_cs.h:76-87): d1 = flags|QW_WRITE, d2 = addr, d4 = value. */
    dst[6]  = GFX_OP_PIPE_CONTROL(6);
    dst[7]  = PIPE_CONTROL_FLUSH_ENABLE | PIPE_CONTROL_CS_STALL | PIPE_CONTROL_QW_WRITE;
    dst[8]  = storeAddr;
    dst[9]  = 0;                                /* addr hi */
    dst[10] = magic;                            /* value stored */
    dst[11] = 0;
    dst[12] = MI_USER_INTERRUPT;
    dst[13] = MI_BATCH_BUFFER_END;
    dst[14] = MI_NOOP;                          /* pad → 15 dw */

    RING_DEBUG_RAW("emitPipeControlFlush: store=0x%X magic=0x%X", storeAddr, magic);
    return 15;
}

uint32_t emitBreadcrumbSeqno(uint32_t *dst, uint32_t hwspGttAddr, uint32_t seqno)
{
    if (!dst) return 0;
    uint32_t seqnoStoreAddr = HWS_SEQNO_ADDRESS(hwspGttAddr);

    /* PC1 — HDC pipeline flush */
    dst[0] = GFX_OP_PIPE_CONTROL(6) | PIPE_CONTROL0_HDC_PIPELINE_FLUSH;
    dst[1] = PIPE_CONTROL_GEN12_RCS_FLUSH;
    dst[2] = 0;
    dst[3] = 0;
    dst[4] = 0;
    dst[5] = 0;

    /* PC2 — Store seqno to HWSP (offset 0x80) with GLOBAL_GTT */
    dst[6]  = GFX_OP_PIPE_CONTROL(6);
    dst[7]  = PIPE_CONTROL_FLUSH_ENABLE | PIPE_CONTROL_CS_STALL | PIPE_CONTROL_QW_WRITE | PIPE_CONTROL_GLOBAL_GTT_IVB;
    dst[8]  = seqnoStoreAddr;
    dst[9]  = 0;
    dst[10] = seqno;
    dst[11] = 0;
    dst[12] = MI_USER_INTERRUPT;
    dst[13] = MI_BATCH_BUFFER_END;
    dst[14] = MI_NOOP;

    RING_DEBUG_RAW("emitBreadcrumbSeqno: hwspStore=0x%X seqno=0x%X", seqnoStoreAddr, seqno);
    return 15;
}
