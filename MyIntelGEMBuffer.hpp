/*===========================================================================
 *  MyIntelGEMBuffer.hpp
 *  Hackintosh Kext — GEM Buffer Object Manager (Phase 5)
 *
 * Buffer Objects GGTT:
 * - allocateBuffer(): physical pages + GGTT PTE
 * - bindToGGTT(): PTE GSM (GTT Stolen Memory)
 * - unbindFromGGTT(): PTE + TLB invalidate
 * - freeBuffer(): pages
 *
 *  GGTT PTE Format (Xe / Gen12+):
 *    63  53 52  51      12  11   1  0
 *   ┌───┬───┬──────────┬────┬───┬───┐
 *   │ 0 │PAT│ PhysAddr │ 0  │DM │ V │
 *   └───┴───┴──────────┴────┴───┴───┘
 *     bit 0     = Valid / Present (XE_PAGE_PRESENT)
 *     bit 1     = Device Memory (XE_GGTT_PTE_DM)
 *     bits 12-51 = Physical Address >> 12 (XE_PTE_ADDR_MASK)
 *     bit 52    = PAT index bit 0 (XELPG_GGTT_PTE_PAT0)
 *     bit 53    = PAT index bit 1 (XELPG_GGTT_PTE_PAT1)
 *
 *  Reference: Linux XE driver — xe_gtt_defs.h, xe_ggtt.c
 *///=========================================================================

#ifndef __MY_INTEL_GEM_BUFFER_HPP__
#define __MY_INTEL_GEM_BUFFER_HPP__

#include <libkern/libkern.h>
#include <stdint.h>

/*
 * ─────────────────────────────────────────────
 *  Constants
 * ─────────────────────────────────────────────
 */

/* GGTT Page Size = 4KB */
#define GEM_PAGE_SHIFT          12
#define GEM_PAGE_SIZE           (1ULL << GEM_PAGE_SHIFT)   /* 4096 */
#define GEM_PAGE_MASK           (GEM_PAGE_SIZE - 1)

/* Max buffer size — Phase 4.4 raised from 16MB to 256MB (64K pages)
 * so ML/OpenVINO workloads can use the GTT-addressed VRAM pool.
 * Not higher: gemBufferResolvePages() resolves one IOMemoryDescriptor
 * per page (3 IOKit calls), so 256MB ≈ 200K calls ≈ ~1s one-time cost. */
#define GEM_MAX_BUFFER_SIZE     (256 * 1024 * 1024)
#define GEM_MAX_BUFFER_PAGES    (GEM_MAX_BUFFER_SIZE / GEM_PAGE_SIZE)

/* Default ring buffer size = 16KB (4 pages) */
#define GEM_RING_SIZE           0x4000
#define GEM_RING_PAGES          (GEM_RING_SIZE / GEM_PAGE_SIZE)

/* Default batch buffer size = 64KB (16 pages) */
#define GEM_BATCH_SIZE          0x10000
#define GEM_BATCH_PAGES         (GEM_BATCH_SIZE / GEM_PAGE_SIZE)

/*
 * ─────────────────────────────────────────────
 *  PTE Flag Bits (Gen12+ / Xe)
 * ─────────────────────────────────────────────
 *
 *  Reference: Linux XE driver — xe_gtt_defs.h
 *    XE_PAGE_PRESENT    = BIT_ULL(0)     — Valid/Present bit
 *    XE_GGTT_PTE_DM     = BIT_ULL(1)     — Device Memory flag
 *    XELPG_GGTT_PTE_PAT0 = BIT_ULL(52)   — PAT index bit 0 (XELPG ONLY)
 *    XELPG_GGTT_PTE_PAT1 = BIT_ULL(53)   — PAT index bit 1 (XELPG ONLY)
 *    XE_PTE_ADDR_MASK   = GENMASK_ULL(51, 12) — Physical address mask
 *
 *  ⚠️ Platform note: RPL-U / Iris Xe (0xA7AC) is Xe-LP-derived; the
 *  XELPG PAT-bit layout (bits 52-53) is NOT verified for this part.
 *  We assert PRESENT only and let hardware use PAT index 0 (cache mode
 *  programmed by firmware) until PRM confirms PAT encoding. Do NOT set
 *  GEM_PTE_PAT0/PAT1 for system-memory GGTT PTEs.
 *
 *  GGTT PTE Format (64-bit):
 *    63       51      12  11   1  0
 *    ┌────────┬──────────┬────┬───┬───┐
 *    │ 0      │ PhysAddr │ 0  │DM │ V │
 *    └────────┴──────────┴────┴───┴───┘
 *      V   = Valid/Present (bit 0)
 *      DM  = Device Memory (bit 1)
 *      Phys = Physical address bits 12-51
 */
#define GEM_PTE_PRESENT         (1ULL << 0)             /* XE_PAGE_PRESENT */
#define GEM_PTE_DM              (1ULL << 1)             /* XE_GGTT_PTE_DM — Device Memory */
#define GEM_PTE_ADDR_MASK       0x000FFFFFFFFFF000ULL   /* GENMASK_ULL(51,12) — bits 12-51 */

/*
 * Default cache settings for Gen12+ LLC-coherent (PAT index 0 = WB):
 *
 * For system memory (CPU-accessible buffers):
 *   PTE = GEM_PTE_PRESENT (no DM flag) → PAT index 0, LLC-coherent WB
 *
 * For device memory (stolen/VRAM):
 *   PTE = GEM_PTE_PRESENT | GEM_PTE_DM
 *
 * LLC (Last Level Cache) shared between CPU + GPU on Gen12+:
 *   → WB memory is HW-coherent between CPU and GT
 *   → No CLFLUSH needed for most operations
 */
#define GEM_PTE_SYSTEM_DEFAULT  (GEM_PTE_PRESENT)       /* system RAM, PAT 0 (WB) */
#define GEM_PTE_DEVICE_DEFAULT  (GEM_PTE_PRESENT | GEM_PTE_DM)  /* device memory */

/*
 * ─────────────────────────────────────────────
 *  GEM Buffer Object Flags
 * ─────────────────────────────────────────────
 */
#define GEM_FLAG_CPU_READ        (1U << 0)
#define GEM_FLAG_CPU_WRITE       (1U << 1)
#define GEM_FLAG_GPU_READ        (1U << 2)
#define GEM_FLAG_GPU_WRITE       (1U << 3)
#define GEM_FLAG_RING            (1U << 4)   /* Ring buffer (pinned) */
#define GEM_FLAG_BATCH           (1U << 5)   /* Batch buffer */
#define GEM_FLAG_PINNED          (1U << 6)   /* Permanently pinned in GGTT */

/*
 * GGTT lifecycle state (set/cleared by bind/unbind + plane/engine owners):
 *   MAPPED    — PTEs written + barrier + TLB invalidate completed; safe for
 *               plane/engine to consume. Set ONLY inside bind, after the
 *               invalidate-by-contract sequence.
 *   SCANOUT   — a plane is actively scanning this buffer (programPlane).
 *   IN_FLIGHT — engine batch/context references it (retire before unbind).
 *   RETIRING  — destroy requested but still referenced; unbind deferred.
 */
#define GEM_STATE_MAPPED        (1U << 1)
#define GEM_STATE_SCANOUT       (1U << 2)
#define GEM_STATE_IN_FLIGHT     (1U << 3)
#define GEM_STATE_RETIRING      (1U << 4)

/*
 * ─────────────────────────────────────────────
 *  MyIntelGEMBuffer Structure
 * ─────────────────────────────────────────────
 *
 *  POD struct — no constructor/destructor (IOKit C++ constraints)
 * helper functions
 */
typedef struct {
 uint32_t size; /* buffer (bytes) */
 uint32_t pages; /* pages */

    uint32_t    ggttOffset;     /* GGTT offset (offset in GGTT address space) */
                                /* = pageIndex * GEM_PAGE_SIZE */
    uint32_t    flags;          /* GEM_FLAG_* */
    uint32_t    state;          /* GEM_STATE_* lifecycle (MAPPED/SCANOUT/...) */
    uint32_t    bindSeq;        /* GGTT telemetry seq of last successful bind —
                                 * [PLANE] logs carry it so a parser can
                                 * correlate present/retire with bind/invalidate
                                 * ordering (0 = never bound) */

    /* Physical pages */
    void       *cpuAddr;        /* Kernel virtual address */
    uint64_t    physAddr;       /* Physical address of page 0 — IOMallocAligned
                                 * is NOT guaranteed physically contiguous, so
                                 * per-page addresses live in pagesPhys[] */
    uint64_t   *pagesPhys;      /* Per-page physical addresses (pages entries,
                                 * NULL until resolved). PTE mapping MUST use
                                 * these — NEVER physAddr + i*PAGE */

    /* Plane scanout snapshot (set by MyIntelGPU::programPlane) — used to
     * restore the previous plane state when this buffer is destroyed, so a
     * client exiting cannot leave the plane pointing at freed GGTT (which
     * blacks out the panel). Original PLANE_A_BASE + *_OFFSET values. */
    bool        planeBound;     /* true if this buffer was bound to a plane */
    uint32_t    planeSavedCtl;  /* original PLANE_CTL  (pre-program) */
    uint32_t    planeSavedStride;/* original PLANE_STRIDE */
    uint32_t    planeSavedSize;  /* original PLANE_SIZE   */
    uint32_t    planeSavedSurf;  /* original PLANE_SURF   */

    /* Debug */
    uint32_t    magic;          /* Magic number for validation */
} MyIntelGEMBuffer;

/* Magic number for buffer validation */
#define GEM_BUFFER_MAGIC        0x47454D42   /* "GEMB" */

/*!
 * @brief  Invoke GGTT TLB invalidate via writeReg32 callback
 *
 * @param writeReg32Func  function pointer (MyIntelGPU::writeReg32)
 * @param ggttInvalidOffset  register offset = GFX_FLSH_CNTL_GEN6 (0x101008)
 */
#ifndef MY_INTEL_WRITE_REG_32_FUNC
#define MY_INTEL_WRITE_REG_32_FUNC
typedef void (*WriteReg32Func)(void *context, uint32_t offset, uint32_t value);
#endif /* MY_INTEL_WRITE_REG_32_FUNC */

/*!
 * @brief GGTT TLB invalidate callback — invalidate-by-contract.
 *
 * Invoked by gemBufferBindToGGTT()/gemBufferUnbindFromGGTT()/gemBufferDestroy()
 * AFTER the PTE batch write + OSSynchronizeIO() barrier, so NO PTE mutation
 * can escape without a following TLB invalidate — callers must never need to
 * remember to flush manually (owner layer wires this to
 * MyIntelGPU::ggttInvalidateTrampoline, which runs the full
 * write GFX_FLSH_CNTL_EN + posting-read protocol).
 *
 * The callback may be NULL only when gsmPtr is NULL (CPU-only buffer, no
 * PTE written); passing NULL with a real gsmPtr logs a stale-TLB warning.
 */
typedef void (*GGTTInvalidateFunc)(void *context);
void gemBufferGGTTInvalidate(
    WriteReg32Func writeReg32Func,
    void          *context,
    uint32_t       ggttInvalidOffset);

/*
 * ─────────────────────────────────────────────
 *  Buffer Object Helper Functions
 * ─────────────────────────────────────────────
 *
 * : create → destroy
 * C-style function ( exceptions / RTTI)
 */

/*!
 * @brief GEM buffer — allocate pages + bind GGTT ( aperture )
 *
 * @param size buffer (byte) — align GEM_PAGE_SIZE
 * @param flags     GEM_FLAG_* (CPU/GPU access flags)
 * @param gsmPtr pointer GTT Stolen Memory (fGsm) PTE write
 * @param gttTotal  GGTT total entries (fGttTotal) — limit
 * @param apertureVA Virtual address BAR2 aperture ( NULL)
 * @param apertureSize aperture (byte)
 * @param invalidateFn GGTT TLB invalidate callback (invalidate-by-contract —
 *        invoked after PTE writes + barrier inside bind; may be NULL only
 *        when gsmPtr is NULL)
 * @param invalidateCtx context passed to invalidateFn
 *
 * @return MyIntelGEMBuffer* — buffer object NULL failed
 * free gemBufferDestroy()
 */
MyIntelGEMBuffer *gemBufferCreate(
    uint32_t          size,
    uint32_t          flags,
    uint32_t         *gsmPtr,
    uint32_t          gttTotal,
    void             *apertureVA,
    uint64_t          apertureSize,
    GGTTInvalidateFunc invalidateFn,
    void             *invalidateCtx);

/*!
 * @brief GEM buffer — unbind GGTT + free pages
 *
 * @param buf       buffer object (may be NULL)
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param invalidateFn GGTT TLB invalidate callback (invalidate-by-contract —
 *        invoked after PTE clear + barrier inside unbind)
 * @param invalidateCtx context passed to invalidateFn
 */
void gemBufferDestroy(
    MyIntelGEMBuffer  *buf,
    uint32_t          *gsmPtr,
    GGTTInvalidateFunc invalidateFn,
    void              *invalidateCtx);

/*!
 * @brief Bind buffer GGTT — PTE page
 *
 * bind (unbind )
 * invalidate-by-contract: PTE writes + OSSynchronizeIO() + invalidateFn()
 * completed before this returns — caller must NOT need a manual
 * ggttInvalidate() afterwards.
 *
 * @param buf       buffer object
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param gttTotal  GGTT total entries
 * @param invalidateFn GGTT TLB invalidate callback (may be NULL only with
 *        NULL gsmPtr)
 * @param invalidateCtx context passed to invalidateFn
 * @return true = success
 */
bool gemBufferBindToGGTT(
    MyIntelGEMBuffer  *buf,
    uint32_t          *gsmPtr,
    uint32_t           gttTotal,
    GGTTInvalidateFunc invalidateFn,
    void              *invalidateCtx);

/*!
 * @brief Unbind buffer GGTT — clear PTE
 *
 * invalidate-by-contract: PTE clears + OSSynchronizeIO() + invalidateFn()
 * completed before this returns.
 *
 * @param buf       buffer object
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param gttTotal  GGTT total entries
 * @param invalidateFn GGTT TLB invalidate callback (may be NULL only with
 *        NULL gsmPtr)
 * @param invalidateCtx context passed to invalidateFn
 */
void gemBufferUnbindFromGGTT(
    MyIntelGEMBuffer  *buf,
    uint32_t          *gsmPtr,
    uint32_t           gttTotal,
    GGTTInvalidateFunc invalidateFn,
    void              *invalidateCtx);

/*!
 * @brief Scan GGTT PTE table free page block
 *
 * (Simple linear scan — Phase 5 optimize)
 *
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param gttTotal  GGTT total entries
 * @param pages pages
 * @return page index (0 = invalid/full) 0
 */
uint32_t gemBufferFindFreeRegion(
    const uint32_t *gsmPtr,
    uint32_t        gttTotal,
    uint32_t        pages);

/*!
 * @brief Mark GGTT page range free (clear PTEs)
 *
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param startPage index page
 * @param pages pages
 */
void gemBufferClearPTEs(
    uint32_t *gsmPtr,
    uint32_t  startPage,
    uint32_t  pages);

static inline uint64_t gemBufferMakePTE(
    uint64_t dmaAddr,
    uint64_t cacheBits)
{
    /*
     * PTE = (dma_addr >> 12) << 12 | flags
     *
     * XE driver format: dmaAddr | XE_PAGE_PRESENT
     * Physical address is already page-aligned (4KB),
     * so we just OR in the flags.
     *
     * Reference: xe_ggtt.c xelp_ggtt_pte_flags()
     *
     * NOTE: caller passes the DMA address as seen by the GT (from
     * IOMemoryDescriptor::getPhysicalSegment), not a raw CPU physAddr —
     * identical unless VT-d/IOMMU remaps.
     */
    uint64_t pte = (dmaAddr & GEM_PTE_ADDR_MASK) | cacheBits;

#if DEBUG
    /* Validation (debug builds only — zero cost in release) */
    if ((dmaAddr & GEM_PAGE_MASK) != 0) {
        IOLog("GEMBuf: PTE ERROR — dmaAddr 0x%llX not 4KB-aligned\n", dmaAddr);
    }
    if ((dmaAddr & ~GEM_PTE_ADDR_MASK) != 0) {
        IOLog("GEMBuf: PTE ERROR — dmaAddr 0x%llX has bits outside mask 0x%llX\n",
              dmaAddr, GEM_PTE_ADDR_MASK);
    }
    if ((cacheBits & GEM_PTE_ADDR_MASK) != 0) {
        IOLog("GEMBuf: PTE ERROR — cacheBits 0x%llX overlaps address field\n", cacheBits);
    }
#endif
    return pte;
}

/*!
 * @brief Align size page boundary
 */
static inline uint32_t gemBufferAlignSize(uint32_t size)
{
    return (size + GEM_PAGE_MASK) & ~GEM_PAGE_MASK;
}

/*!
 * @brief  Convert size to page count
 */
static inline uint32_t gemBufferPageCount(uint32_t size)
{
    return gemBufferAlignSize(size) >> GEM_PAGE_SHIFT;
}

#endif /* __MY_INTEL_GEM_BUFFER_HPP__ */
