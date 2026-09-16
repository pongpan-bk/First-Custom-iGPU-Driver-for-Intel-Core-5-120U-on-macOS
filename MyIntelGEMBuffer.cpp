/*===========================================================================
 *  MyIntelGEMBuffer.cpp
 *  Hackintosh Kext — GEM Buffer Object Manager (Phase 5 - Gen12 Fix)
 *
 * work:
 *    - IOMallocAligned → pages (per-page phys resolved into pagesPhys[])
 *    - PTE write → GSM (GTT Stolen Memory)
 *    - GGTT TLB invalidate → writeReg32(GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN)
 *
 * Physical memory allocation kext:
 * IOMallocAligned(size, alignment) —
 * Physical Address kvtophys() IOGetPhysicalAddress
 * ( API MacKernelSDK → IOMemoryDescriptor )
 *
 * Strategy:
 *    1. IOMallocAligned() → cpu virtual address
 *    2. IOMemoryDescriptor::withAddress(kernel_task) → map physical
 *    3. physical address IOMemoryDescriptor::prepare() + getPhysicalSegment()
 *
 * : Linux i915 — i915_gem_gtt.c
 *///=========================================================================

#include "MyIntelGEMBuffer.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/libkern.h>

/*
 * ─────────────────────────────────────────────
 *  Debug Logging
 * ─────────────────────────────────────────────
 * IOLog + prefix "GEMBuf" grep
 */
#define GEM_DEBUG(str, ...) \
    do { IOLog("GEMBuf: " str "\n", ##__VA_ARGS__); } while(0)

#if 0
#define GEM_TRACE(str, ...) \
    do { IOLog("GEMBuf: " str "\n", ##__VA_ARGS__); } while(0)
#else
#define GEM_TRACE(str, ...) do { } while(0)
#endif

/*
 * ─────────────────────────────────────────────
 *  Forward Declarations (internal)
 * ─────────────────────────────────────────────
 */
static bool     gemBufferAllocPages(MyIntelGEMBuffer *buf);
static void     gemBufferFreePages(MyIntelGEMBuffer *buf);
static bool     gemBufferResolvePages(MyIntelGEMBuffer *buf);

/*
 * ─────────────────────────────────────────────
 *  Public API
 * ─────────────────────────────────────────────
 */

MyIntelGEMBuffer *gemBufferCreate(
    uint32_t    size,
    uint32_t    flags,
    uint32_t   *gsmPtr,
    uint32_t    gttTotal,
    void       *apertureVA,
    uint64_t    apertureSize)
{
    GEM_TRACE("gemBufferCreate: size=%u flags=0x%X", size, flags);

    /* Validate size */
    if (size == 0 || size > GEM_MAX_BUFFER_SIZE) {
        GEM_DEBUG("ERROR: invalid size %u (max %u)", size, GEM_MAX_BUFFER_SIZE);
        return NULL;
    }

    /* Allocate buffer struct (from kernel heap) */
    MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)IOMalloc(sizeof(MyIntelGEMBuffer));
    if (!buf) {
        GEM_DEBUG("ERROR: failed to allocate GEM buffer struct");
        return NULL;
    }

    /* Zero-initialize */
    bzero(buf, sizeof(MyIntelGEMBuffer));

    buf->magic     = GEM_BUFFER_MAGIC;
    buf->size      = gemBufferAlignSize(size);
    buf->pages     = gemBufferPageCount(size);
    buf->flags     = flags | GEM_FLAG_CPU_WRITE;
    buf->cpuAddr   = NULL;
    buf->physAddr  = 0;
    buf->ggttOffset = 0;

    /* Allocate physical pages */
    if (!gemBufferAllocPages(buf)) {
        GEM_DEBUG("ERROR: gemBufferAllocPages failed");
        IOFree(buf, sizeof(MyIntelGEMBuffer));
        return NULL;
    }

    /* Bind to GGTT (if GGTT is available) */
    if (gsmPtr && gttTotal > 0) {
        if (!gemBufferBindToGGTT(buf, gsmPtr, gttTotal)) {
            GEM_DEBUG("WARNING: GGTT bind failed — CPU-only buffer");
            /* Continue — buffer still usable from CPU side */
        }
    }

    /* CPU-side mapping: already via cpuAddr from IOMallocAligned */

    GEM_DEBUG("gemBufferCreate: OK — size=%u pages=%u cpuAddr=%p physAddr=0x%llX ggttOffset=0x%X",
              buf->size, buf->pages, buf->cpuAddr, buf->physAddr, buf->ggttOffset);

    return buf;
}

void gemBufferDestroy(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr)
{
    if (!buf || buf->magic != GEM_BUFFER_MAGIC) {
        return;
    }

    GEM_TRACE("gemBufferDestroy: buf=%p size=%u", buf, buf->size);

    /* Unbind from GGTT first */
    if (gsmPtr && buf->ggttOffset != 0) {
        gemBufferUnbindFromGGTT(buf, gsmPtr, 0);
    }

    /* Free physical pages */
    gemBufferFreePages(buf);

    /* Invalidate magic */
    buf->magic = 0;

    /* Free struct */
    IOFree(buf, sizeof(MyIntelGEMBuffer));

    GEM_TRACE("gemBufferDestroy: OK");
}

bool gemBufferBindToGGTT(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr,
    uint32_t          gttTotal)
{
    if (!buf || !gsmPtr) return false;
    if (buf->magic != GEM_BUFFER_MAGIC) return false;
    if (buf->physAddr == 0) return false;

    GEM_TRACE("gemBufferBindToGGTT: buf=%p pages=%u", buf, buf->pages);

    /* bind → unbind */
    if (buf->ggttOffset != 0) {
        gemBufferUnbindFromGGTT(buf, gsmPtr, gttTotal);
    }

    /* free region GGTT */
    uint32_t pageIdx = gemBufferFindFreeRegion(gsmPtr, gttTotal, buf->pages);
    if (pageIdx == 0) {
        GEM_DEBUG("ERROR: no free GGTT region (%u pages)", buf->pages);
        return false;
    }

    buf->ggttOffset = pageIdx * GEM_PAGE_SIZE;

    /*
     * Write PTEs — one per page
     */
    volatile uint64_t *gsm64 = (volatile uint64_t *)gsmPtr;

    for (uint32_t i = 0; i < buf->pages; i++) {
        /* Per-page PTE — use pagesPhys[], NEVER physAddr + i*PAGE */
        uint64_t pte = gemBufferMakePTE(buf->pagesPhys[i], GEM_PTE_LLC);

        /* writeq() — atomic 64-bit write on x86_64 */
        gsm64[pageIdx + i] = pte;
    }

    /* One barrier for the whole batch */
    OSSynchronizeIO();

    GEM_TRACE("gemBufferBindToGGTT: OK — ggttOffset=0x%X phys=0x%llX",
              buf->ggttOffset, buf->physAddr);

    return true;
}

void gemBufferUnbindFromGGTT(
    MyIntelGEMBuffer *buf,
    uint32_t         *gsmPtr,
    uint32_t          gttTotal)
{
    if (!buf || !gsmPtr) return;
    if (buf->ggttOffset == 0) return;

    GEM_TRACE("gemBufferUnbindFromGGTT: ggttOffset=0x%X", buf->ggttOffset);

    uint32_t startPage = buf->ggttOffset >> GEM_PAGE_SHIFT;
    gemBufferClearPTEs(gsmPtr, startPage, buf->pages);

    buf->ggttOffset = 0;
}

/* 🚨 แก้ปมส้นตีน: อัปเกรดตัวล้างแคช (TLB Invalidate) ให้รองรับสถาปัตยกรรม Gen 12 อ้างอิงตาม Linux XE driver */
void gemBufferGGTTInvalidate(
    WriteReg32Func writeReg32Func,
    void          *context,
    uint32_t       ggttInvalidOffset)
{
    if (!writeReg32Func) return;

    GEM_TRACE("gemBufferGGTTInvalidate: Gen12 Force-Flush Triggered offset=0x%X", ggttInvalidOffset);
    
    /* โค้ดลีนุกซ์ XE/i915 สำหรับชิปยุคใหม่: การล้างตาราง GGTT ต้องยิงแฟล็กสลับบิต 
     * บังคับเปิดบิตเปิดฟังก์ชัน (Bit 0 = 1) เพื่อถล่มแคชระบบแอนิเมชันให้ยอมโหลดใหม่ */
    writeReg32Func(context, ggttInvalidOffset, 1); 
    
    /* ยิงซ้ำเพื่อเสถียรภาพสถาปัตยกรรม (Double-Wall Sync) */
    OSSynchronizeIO();
}

uint32_t gemBufferFindFreeRegion(
    const uint32_t *gsmPtr,
    uint32_t        gttTotal,
    uint32_t        pages)
{
    if (!gsmPtr || gttTotal == 0 || pages == 0 || pages > gttTotal) {
        return 0;
    }

    const volatile uint64_t *gsm64 = (const volatile uint64_t *)gsmPtr;

    uint32_t consecutive = 0;
    uint32_t startPage   = 0;

    /* Skip page 0 (reserved for HW), leave 4 guard pages at end */
    uint32_t maxPages = gttTotal - 4;

    for (uint32_t i = 1; i < maxPages; i++) {
        if (gsm64[i] == 0) {
            if (consecutive == 0) {
                startPage = i;
            }
            consecutive++;

            if (consecutive >= pages) {
                return startPage;
            }
        } else {
            consecutive = 0;
        }
    }

    GEM_DEBUG("WARNING: no free GGTT region found (searched %u entries, need %u pages)",
              maxPages, pages);
    return 0;
}

/* 🚨 ซ่อมส่วนท้ายไฟล์ที่ถูกตัดขาดให้สมบูรณ์เรียบร้อย */
void gemBufferClearPTEs(
    uint32_t *gsmPtr,
    uint32_t  startPage,
    uint32_t  pages)
{
    if (!gsmPtr || pages == 0) return;

    volatile uint64_t *gsm64 = (volatile uint64_t *)gsmPtr;

    for (uint32_t i = 0; i < pages; i++) {
        gsm64[startPage + i] = 0;
    }
    
    /* บังคับเคลียร์กระแสข้อมูลฝั่งซีพียูหลังล้างค่าตาราง */
    OSSynchronizeIO();
}

/*
 * ─────────────────────────────────────────────
 *  Internal Helpers
 * ─────────────────────────────────────────────
 */

static bool gemBufferAllocPages(MyIntelGEMBuffer *buf)
{
    if (!buf || buf->size == 0) return false;

    /*
     * IOMallocAligned — physically contiguous, page-aligned
     *
     * alignment = GEM_PAGE_SIZE (4KB) physical pages
     * aligned GGTT page boundary
     *
     * : IOMallocAligned kext
     * memory leak user space
     */
    buf->cpuAddr = IOMallocAligned(buf->size, GEM_PAGE_SIZE);
    if (!buf->cpuAddr) {
        GEM_DEBUG("ERROR: IOMallocAligned failed (size=%u)", buf->size);
        return false;
    }

    /* Zero-initialize */
    bzero(buf->cpuAddr, buf->size);

    /* Resolve per-page physical addresses */
    if (!gemBufferResolvePages(buf)) {
        GEM_DEBUG("ERROR: gemBufferResolvePages failed");
        IOFreeAligned(buf->cpuAddr, buf->size);
        buf->cpuAddr = NULL;
        return false;
    }

    GEM_TRACE("gemBufferAllocPages: cpu=%p phys=0x%llX size=%u",
              buf->cpuAddr, buf->physAddr, buf->size);
    return true;
}

static void gemBufferFreePages(MyIntelGEMBuffer *buf)
{
    if (!buf || !buf->cpuAddr) return;

    if (buf->pagesPhys) {
        IOFree(buf->pagesPhys, buf->pages * sizeof(uint64_t));
        buf->pagesPhys = NULL;
    }

    IOFreeAligned(buf->cpuAddr, buf->size);
    buf->cpuAddr = NULL;
    buf->physAddr = 0;
}

/*
 * ─────────────────────────────────────────────
 *  Physical Address Resolution
 * ─────────────────────────────────────────────
 *
 *  macOS kext: IOMallocAligned returns kernel virtual address.
 *  physical address IOMemoryDescriptor
 *
 *  IOMallocAligned is NOT guaranteed to return physically contiguous
 *  pages, so each page is resolved individually and stored in
 *  buf->pagesPhys[]. PTE mapping MUST use these per-page addresses —
 *  never physAddr + i*PAGE.
 *
 *  per page:
 *    1. IOMemoryDescriptor::withAddress(kernel_task) — wrap VA
 *    2. IOMemoryDescriptor::prepare() — resolve pages
 *    3. IOMemoryDescriptor::getPhysicalSegment() — get PA
 *    4. IOMemoryDescriptor::complete() — release
 */

static bool gemBufferResolvePages(MyIntelGEMBuffer *buf)
{
    if (!buf || !buf->cpuAddr || buf->pages == 0) return false;

    buf->pagesPhys = (uint64_t *)IOMalloc(buf->pages * sizeof(uint64_t));
    if (!buf->pagesPhys) {
        GEM_DEBUG("ERROR: pagesPhys alloc failed (%u pages)", buf->pages);
        return false;
    }

    mach_vm_address_t va = reinterpret_cast<mach_vm_address_t>(buf->cpuAddr);

    for (uint32_t i = 0; i < buf->pages; i++) {
        IOMemoryDescriptor *desc = IOMemoryDescriptor::withAddress(
                                        (void *)(va + (i * GEM_PAGE_SIZE)),
                                        GEM_PAGE_SIZE,
                                        kIODirectionInOut);
        if (!desc) {
            GEM_DEBUG("ERROR: IOMemoryDescriptor::withAddress failed (page %u)", i);
            IOFree(buf->pagesPhys, buf->pages * sizeof(uint64_t));
            buf->pagesPhys = NULL;
            return false;
        }

        IOReturn ret = desc->prepare(kIODirectionInOut);
        if (ret != kIOReturnSuccess) {
            GEM_DEBUG("ERROR: prepare() failed (page %u, 0x%X)", i, ret);
            desc->release();
            IOFree(buf->pagesPhys, buf->pages * sizeof(uint64_t));
            buf->pagesPhys = NULL;
            return false;
        }

        IOByteCount offset = 0;
        uint64_t physAddr = desc->getPhysicalSegment(offset, NULL,
                                                      kIODirectionInOut);
        if (physAddr == 0) {
            GEM_DEBUG("ERROR: getPhysicalSegment returned 0 (page %u)", i);
            desc->complete(kIODirectionInOut);
            desc->release();
            IOFree(buf->pagesPhys, buf->pages * sizeof(uint64_t));
            buf->pagesPhys = NULL;
            return false;
        }

        desc->complete(kIODirectionInOut);
        desc->release();

        buf->pagesPhys[i] = physAddr;
    }

    /* physAddr kept as page 0 address (for compat/logging) */
    buf->physAddr = buf->pagesPhys[0];

    GEM_TRACE("gemBufferResolvePages: %u pages resolved (phys0=0x%llX)",
              buf->pages, buf->physAddr);
    return true;
}
