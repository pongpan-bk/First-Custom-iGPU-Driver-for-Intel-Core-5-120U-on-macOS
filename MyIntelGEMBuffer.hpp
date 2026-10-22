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
 *    XELPG_GGTT_PTE_PAT0 = BIT_ULL(52)   — PAT index bit 0
 *    XELPG_GGTT_PTE_PAT1 = BIT_ULL(53)   — PAT index bit 1
 *    XE_PTE_ADDR_MASK   = GENMASK_ULL(51, 12) — Physical address mask
 *
 *  GGTT PTE Format (64-bit):
 *    63  53 52  51      12  11   1  0
 *    ┌───┬───┬──────────┬────┬───┬───┐
 *    │ 0 │PAT│ PhysAddr │ 0  │DM │ V │
 *    └───┴───┴──────────┴────┴───┴───┘
 *      V   = Valid (bit 0)
 *      DM  = Device Memory (bit 1)
 *      PAT = PAT index bits (bits 52-53)
 *      Phys = Physical address >> 12 (bits 12-51)
 */
#define GEM_PTE_VALID           (1ULL << 0)             /* XE_PAGE_PRESENT */
#define GEM_PTE_DM              (1ULL << 1)             /* XE_GGTT_PTE_DM — Device Memory */
#define GEM_PTE_PAT0            (1ULL << 52)            /* XELPG_GGTT_PTE_PAT0 */
#define GEM_PTE_PAT1            (1ULL << 53)            /* XELPG_GGTT_PTE_PAT1 */
#define GEM_PTE_ADDR_MASK       (0xFFFFFFFFFULL << 12)  /* XE_PTE_ADDR_MASK — bits 12-51 */

/*
 * Default cache settings for Gen12+ LLC-coherent:
 *
 * For system memory (CPU-accessible buffers):
 *   PAT index 0 = WB (Write-Back) — LLC-coherent
 *   PTE = GEM_PTE_VALID (no DM flag)
 *
 * For device memory (stolen/VRAM):
 *   PTE = GEM_PTE_VALID | GEM_PTE_DM
 *
 * LLC (Last Level Cache) shared between CPU + GPU on Gen12+:
 *   → WB memory is HW-coherent between CPU and GT
 *   → No CLFLUSH needed for most operations
 */
#define GEM_PTE_LLC             (GEM_PTE_VALID)                        /* WB, LLC-coherent, system memory */
#define GEM_PTE_DEVICE          (GEM_PTE_VALID | GEM_PTE_DM)           /* Device memory (stolen/VRAM) */
#define GEM_PTE_UNCACHED        (GEM_PTE_VALID | GEM_PTE_DM)           /* UC via DM flag */

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
 * @param ggttInvalidFunc callback TLB invalidate bind/unbind
 *
 * @return MyIntelGEMBuffer* — buffer object NULL failed
 * free gemBufferDestroy()
 */
MyIntelGEMBuffer *gemBufferCreate(
    uint32_t    size,
    uint32_t    flags,
    uint32_t   *gsmPtr,
    uint32_t    gttTotal,
    void       *apertureVA,
    uint64_t    apertureSize);

/*!
 * @brief GEM buffer — unbind GGTT + free pages
 *
 * @param buf       buffer object (may be NULL)
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param ggttInvalidFunc callback TLB invalidate
 */
void gemBufferDestroy(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr);

/*!
 * @brief Bind buffer GGTT — PTE page
 *
 * bind (unbind )
 * caller: ggttInvalidate() binding / unbinding
 *
 * @param buf       buffer object
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param gttTotal  GGTT total entries
 * @return true = success
 */
bool gemBufferBindToGGTT(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr,
    uint32_t          gttTotal);

/*!
 * @brief Unbind buffer GGTT — clear PTE
 *
 * @param buf       buffer object
 * @param gsmPtr pointer GTT Stolen Memory (fGsm)
 * @param gttTotal  GGTT total entries
 */
void gemBufferUnbindFromGGTT(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr,
    uint32_t          gttTotal);

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
void gemBufferGGTTInvalidate(
    WriteReg32Func writeReg32Func,
    void          *context,
    uint32_t       ggttInvalidOffset);

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
    uint64_t physAddr,
    uint64_t cacheBits)
{
    /*
     * PTE = (physical_addr >> 12) << 12 | flags
     *
     * XE driver format: physAddr | XE_PAGE_PRESENT
     * Physical address is already page-aligned (4KB),
     * so we just OR in the flags.
     *
     * Reference: xe_ggtt.c xelp_ggtt_pte_flags()
     */
    return (physAddr & GEM_PTE_ADDR_MASK) | cacheBits;
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
