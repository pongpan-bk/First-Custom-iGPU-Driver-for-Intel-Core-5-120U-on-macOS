/*===========================================================================
 *  MyIntelGPU.cpp
 *  Hackintosh Kext — FakeID Alder Lake → Coffee Lake
 *
 * Implementation MyIntelGPU
 *
 * :
 *    - PCI Initialization (i915_pci_probe counterpart)
 *    - MMIO BAR mapping (i915_driver_mmio_probe counterpart)
 * - Dynamic Register Translation — macOS
 * register I/O real hardware
 *    - GGTT TLB flush + CPU cache coherency
 *
 *  Reference:
 *    Linux i915:
 *      i915_driver.c       → i915_driver_mmio_probe, i915_driver_hw_probe
 *      i915_pci.c          → i915_pci_probe, pciidlist
 *      intel_uncore.c      → intel_uncore_read, intel_uncore_write
 *      i915_gem.c          → GGTT, aperture, GEM object lifecycle
 *      intel_engine_cs.c   → engine MMIO base definition
 *
 *    macOS IOKit:
 *      IODeviceMemory, IOMemoryMap
 *      IOPCIDevice API
 *      OSSynchronizeIO / OSMemoryBarrier / IODelayWriteCombined
 *///=========================================================================

#include "MyIntelGPU.hpp"
#include "IntelFramebuffer.hpp"
#include "MyIntelFramebuffer.hpp"
#include "MyIntelAccelerator.hpp"
#include "MyIntelRing.hpp"
#include "MyIntelGEMBuffer.hpp"   /* GEM_PAGE_SIZE for VRAM pool sizing */
#include "MyIntelObfuscate.h"
#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>
#include <pexpert/pexpert.h>
#include <stdint.h>
#include <string.h>

/* ── Binary-embedded credits — compiled into .kext bytecode.
 * Without source code these strings cannot be removed or altered. */
static const char kMyIntelGPUCredits[] =
    "MyIntelGPU v2.0.240 | First Custom iGPU Driver for Intel Core 5 120U on macOS\n"
    "Author: pongpan-bk | Tooling: OpenCode (OhMyOpenCode)\n"
    "https://github.com/pongpan-bk/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-MacOS\n";

/* 2.0.224: early-boot progress marker (see MyIntelGPU.hpp).
 * Persists the last startup phase to NVRAM so a hard boot hang can be
 * diagnosed after force power-off: `nvram myintelgpu-progress`. Also
 * always IOLog()s so the verbose screen shows it immediately. Degrades
 * silently if IODTNVRAM is not yet published in early boot. */
void mygpuProgress(const char *tag)
{
    IOLog("MyIntelGPU: [progress] %s\n", tag);
    IORegistryEntry *nvram = IORegistryEntry::fromPath("IODTNVRAM", gIOServicePlane);
    if (nvram) {
        OSData *data = OSData::withBytes(tag, (uint32_t)strlen(tag));
        if (data) {
            nvram->setProperty("myintelgpu-progress", data);
            data->release();
        }
        nvram->release();
    }
}

/*
 * _kmod_info — kmutil load kext
 *
 * KMOD_DECL name C identifier (token paste ##)
 * bundle ID "com.myintelgpu.driver" compile error
 *
 * : define kmod_info_t struct KMOD_DECL macro
 *
 * Struct layout (macOS 15.2 SDK):
 *   1 = next             0
 *   2 = info_version     KMOD_INFO_VERSION (=1)
 *   3 = id               -1U (0xFFFFFFFF)
 *   4 = name             "com.myintelgpu.driver"
 *   5 = version          "1.0.0"
 *   6 = reference_count  -1
 *   7 = reference_list   0
 *   8 = address          0
 *   9 = size             0
 *   10 = hdr_size         0
 *   11 = start            myintelgpu_module_start
 *   12 = stop             myintelgpu_module_stop
 */
extern "C" {
    static int myintelgpu_module_start(kmod_info_t *ki, void *data) { return 0; }
    static int myintelgpu_module_stop(kmod_info_t *ki, void *data)  { return 0; }
    kmod_info_t kmod_info = { 0, 1, -1U,                /* next, info_version, id */
        "com.myintelgpu.driver", "1.0.0",              /* name, version */
        -1, 0, 0, 0, 0,                               /* ref_count, ref_list, addr, size, hdr_size */
        myintelgpu_module_start, myintelgpu_module_stop };
}

#ifndef kIOPCIMemorySpace64Bit
#define kIOPCIMemorySpace64Bit 0x1000000000000000ULL
#endif

/*
 * ─────────────────────────────────────────────────────────────────
 *  Gen11/12 GT Distributed Interrupt Registers
 *
 *  VERIFIED against torvalds/linux master (2026-08-11):
 *   - intel_gmd_interrupt_regs.h:  GEN11_GFX_MSTR_IRQ = 0x190010,
 *     GEN11_MASTER_IRQ = BIT(31), GEN11_DISPLAY_IRQ = BIT(16)
 *   - intel_gt_regs.h:  GEN11_GT_INTR_DW(x) = 0x190018 + x*4,
 *     GEN11_RENDER_COPY_INTR_ENABLE = 0x190030,
 *     GEN11_INTR_IDENTITY_REG(x) = 0x190060 + x*4,
 *     GEN11_INTR_DATA_VALID = BIT(31), ENGINE_CLASS bits 18:16,
 *     ENGINE_INSTANCE bits 25:20, ENGINE_INTR low 16,
 *     GEN11_IIR_REG_SELECTOR(x) = 0x190070 + x*4,
 *     GEN11_RCS0_RSVD_INTR_MASK = 0x190090,
 *     GEN11_BCS_RSVD_INTR_MASK = 0x1900a0
 *   - i915_reg.h: GT_RENDER_USER_INTERRUPT = BIT(0),
 *     GT_CS_MASTER_ERROR_INTERRUPT = BIT(3),
 *     GT_CONTEXT_SWITCH_INTERRUPT = BIT(8),
 *     GT_WAIT_SEMAPHORE_INTERRUPT = BIT(11)
 *
 *  NOTE: These live in the global MMIO space (offset 0x190010+), NOT
 *  under any engine mmio_base, and translateAddress() passes them
 *  through untouched (no transTable window covers 0x190000-0x190FFF).
 * ─────────────────────────────────────────────────────────────────
 */
#define GEN11_GFX_MSTR_IRQ            0x190010
#define GEN11_MASTER_IRQ              (1U << 31)
#define GEN11_DISPLAY_IRQ             (1U << 16)
#define GEN11_GT_DW_IRQ(x)            (1U << (x))
#define GEN11_GT_INTR_DW(x)           (0x190018 + (x) * 4)
#define GEN11_RENDER_COPY_INTR_ENABLE 0x190030
#define GEN11_INTR_IDENTITY_REG(x)    (0x190060 + (x) * 4)
#define GEN11_INTR_DATA_VALID         (1U << 31)
#define GEN11_INTR_ENGINE_CLASS_MASK  0x00070000
#define GEN11_INTR_ENGINE_INSTANCE_MASK 0x03F00000
#define GEN11_INTR_ENGINE_INTR_MASK   0x0000FFFF
#define GEN11_IIR_REG_SELECTOR(x)     (0x190070 + (x) * 4)
#define GEN11_RCS0_RSVD_INTR_MASK     0x190090
#define GEN11_BCS_RSVD_INTR_MASK      0x1900a0

#define GT_RENDER_USER_INTERRUPT      (1U << 0)
#define GT_CS_MASTER_ERROR_INTERRUPT  (1U << 3)
#define GT_CONTEXT_SWITCH_INTERRUPT   (1U << 8)
#define GT_WAIT_SEMAPHORE_INTERRUPT   (1U << 11)

/* gen11_gt_irq_postinstall() — masked vs. shared irq mask split.
 *   irqs  = USER | CS_MASTER_ERR | CTX_SWITCH | WAIT_SEM
 *   dmask = irqs<<16 | irqs   (dw0 enable + dw1 enable)
 *   smask = irqs<<16          (shared: only the shared/interrupt-dw1 half) */
#define GEN11_GT_IRQS               (GT_RENDER_USER_INTERRUPT | \
                                     GT_CS_MASTER_ERROR_INTERRUPT | \
                                     GT_CONTEXT_SWITCH_INTERRUPT | \
                                     GT_WAIT_SEMAPHORE_INTERRUPT)
#define GEN11_GT_DMASK              ((GEN11_GT_IRQS << 16) | GEN11_GT_IRQS)
#define GEN11_GT_SMASK              (GEN11_GT_IRQS << 16)

/*
 * Xcode 15.4 (macOS 14 Sonoma) SDK changes:
 *   - OSMemoryBarrier() removed from kernel headers
 *   - kIOMapWriteCombined renamed to kIOMapWriteCombineCache
 *   - kIOMapInhibitCache  renamed to kIOMapCacheInhibit
 *   - getDeviceMemoryWithRegister() now takes 1 arg (UInt8 reg)
 * → use getDeviceMemoryWithIndex()
 *   - createMappingInTask() now requires 4 args
 *
 * Define fallback constants/macros SDK
 */
#ifndef OSMemoryBarrier
#define OSMemoryBarrier()  __sync_synchronize()
#endif

#ifndef kIOMapWriteCombined
#define kIOMapWriteCombined  kIOMapWriteCombineCache
#endif

#ifndef kIOMapInhibitCache
/* Xcode 15.4 removed kIOMapInhibitCache — compute from the existing shift */
#define kIOMapInhibitCache   (1UL << kIOMapCacheShift)
#endif

/*
 * Macro IOKit runtime
 * IOService OSDefineMetaClassAndStructors
 */
#define super IOService
OSDefineMetaClassAndStructors(MyIntelGPU, IOService)

/*
 * IODebug — debug output kernel log
 *
 * IOLog ; comment
 * _EXTRA_DEBUG verbose dev
 *
 * NOTE: os_log was reverted (see MyIntelRing.cpp) — os_log sections broke
 * prelink/auxKC → boot failure. IOLog-only + dmesg capture (msgbuf=1MB).
 */
#define IODebug(fmt, ...) \
    do { IOLog("MyIntelGPU: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)

// #define EXTRA_DEBUG /* log translate address */

/* retry MMIO */
#define BAR_RETRY_COUNT     3

#pragma mark -
#pragma mark - init / free

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::init(OSDictionary *dict)
 *
 * IOService instance:
 * - super::init()
 * - member variables
 * -
 *
 * Linux: i915_driver_create() → devm_drm_dev_alloc()
 * ( Linux struct + drm_device allocator)
 * macOS C++ constructor pattern
 * ─────────────────────────────────────────────────────────────────
 */
bool MyIntelGPU::init(OSDictionary *dict)
{
    /*
 * : init IOService
 * super =
     */
    if (!super::init(dict)) {
        return false;
    }

    /*
 * 0/NULL
 * pointer map
     */
    fPCIDevice    = NULL;
    fMMIOMap      = NULL;
    fRegs         = NULL;
    fApertureMap  = NULL;
    fApertureVA   = NULL;
    fApertureSize = 0;
    fDeviceID     = 0;
    fRevision     = 0;
    fGraphicsVer  = 0;
    fFakeGen      = 0;
    fUseGmdId     = false;
    fGttTotal     = 0;
    fMappableEnd  = 0;
    fGsm          = NULL;
    fStolenBase   = 0;
    fStolenSize   = 0;
    fTransCount   = 0;
    fMMIODesc     = NULL;
    fApertureDesc = NULL;
    fFramebuffer  = NULL;
    fDisplayFramebuffer = NULL;
    fAccelerator  = NULL;

    /* Phase 5 — HW Acceleration */
    fRingRCS       = NULL;
    fRingBCS       = NULL;
    fRingCallbacks = NULL;
    fGemRingRCS    = NULL;
    fGemRingBCS    = NULL;
    fAccelInitialized = false;
    fEngineLock    = NULL;

    /* Phase 6 — Power Management */
    fForceWakeRegs = NULL;
    fFWRefCount    = 0;
    fRC6Enabled    = false;

    /*
 * clear translation table
 * entry 4 fields + name pointer = init
     */
    memset(fTransTable, 0, sizeof(fTransTable));

    obfuscate_init();

    IODebug("init() — OK");

    IOLog("%s", kMyIntelGPUCredits);
    return true;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::free()
 *
 * object :
 * - resources
 * - IOKit stop() free()
 * - init() start()
 * resources (defensive coding)
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::free()
{
    IODebug("free()");

    /*
 * MMIO mapping →
 * ( stop() )
     */
    if (fMMIOMap) {
        fMMIOMap->release();
        fMMIOMap = NULL;
    }
    if (fMMIODesc) {
        fMMIODesc->release();
        fMMIODesc = NULL;
    }
    if (fApertureMap) {
        fApertureMap->release();
        fApertureMap = NULL;
    }
    if (fApertureDesc) {
        fApertureDesc->release();
        fApertureDesc = NULL;
    }
    if (fGSMMap) {
        fGSMMap->release();
        fGSMMap = NULL;
    }
    if (fGSMDesc) {
        fGSMDesc->release();
        fGSMDesc = NULL;
    }
    fGsm = NULL;

    if (fFramebuffer) {
        fFramebuffer->release();
        fFramebuffer = NULL;
    }

    if (fDisplayFramebuffer) {
        fDisplayFramebuffer->release();
        fDisplayFramebuffer = NULL;
    }

    /* Defensive — covers start() abort (stop() never ran) */
    if (fAccelerator) {
        fAccelerator->release();
        fAccelerator = NULL;
    }

    /* Defensive — covers start() abort (stop() never ran) */
    if (fEngineLock) {
        IOLockFree(fEngineLock);
        fEngineLock = NULL;
    }

    /* Defensive — covers start() abort: event sources / workloop may have
     * been created but stop() never ran to release them. */
    if (fInterruptEventSource) {
        fInterruptEventSource->release();
        fInterruptEventSource = NULL;
    }
    if (fDelayedDumpTimer) {
        fDelayedDumpTimer->release();
        fDelayedDumpTimer = NULL;
    }
    if (fWorkLoop) {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }

    super::free();
}

#pragma mark -
#pragma mark - detectHardwareGeneration

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t MyIntelGPU::detectHardwareGeneration()
 *
 * + fFakeGen
 *
 * PCI Device ID → map :
 *    ADL-S (0x468x)  → Gen12 (real), FakeGen=9 (Coffee Lake)
 *    ADL-P (0x469x)  → Gen12 (real), FakeGen=9
 *    RPL-P (0xA7Ax)  → Gen12 (real), FakeGen=9 (same IP as ADL)
 *    CFL  (0x3E9x)   → Gen9  (real), FakeGen=9 (native, no translation)
 *    TGL  (0x9A60)   → Gen12 (real), FakeGen=9
 *    Unknown          → FakeGen=0 (pass-through)
 *
 *  Reference:
 *    include/drm/intel/pciids.h — INTEL_ADLS_IDS, INTEL_ADLP_IDS
 *    i915_pci.c pciidlist[]
 *
 * GMD_ID register (0xD8C) :
 * - Meteor Lake+ has_gmd_id=1
 * - register IP version
 * - fGraphicsVer >= 12.70 → MTL, path
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t MyIntelGPU::detectHardwareGeneration(void)
{
    uint32_t gen = 0;

    /*
 * fPCIDevice valid
 * → config space →
     */
    if (!fPCIDevice) {
        IODebug("ERROR: fPCIDevice is NULL");
        return 0;
    }

    /*
 * 1: PCI Device ID Revision ID
     *
     * PCI config space offset 0x00 = Vendor+Device (2+2 bytes)
     * kIOPCIConfigDeviceID = 0x02
     * kIOPCIConfigRevisionID = 0x08
     */
    fDeviceID = fPCIDevice->configRead16(kIOPCIConfigDeviceID);
    fRevision = fPCIDevice->configRead8(kIOPCIConfigRevisionID);

    IODebug("PCI DeviceID = 0x%04X, Revision = 0x%02X", fDeviceID, fRevision);

    /*
 * 2: Device ID
     *
 * top-level PCI ID ranges — (GT1/GT2/GT3)
 * map revision ID
     */
    switch (fDeviceID >> 8) {
        case 0x46:  /* Alder Lake-S/P (0x468x-0x46Dx) */
            /*
             * Alder Lake = GEN12 (Xe_HPG architecture)
             *
 * Gen12
 * Coffee Lake (Gen9) macOS driver
 * (AppleIntelCFLGraphics)
             *
 * fFakeGen = 9 → translateAddress()
             */
            gen = 12;
            fFakeGen = 9;
            IODebug("Detected Alder Lake (Gen12) → Faking Coffee Lake (Gen9)");
            break;

        case 0x3E:  /* Coffee Lake (0x3E90-0x3E9F) */
        case 0x9B:  /* Coffee Lake-S (0x9BC4, 0x9BC6) */
            /*
 * Coffee Lake — Fake
 * fFakeGen = 9 → translate (fake==real)
             */
            gen = 9;
            fFakeGen = 9;
            IODebug("Detected Coffee Lake (Gen9) — native mode");
            break;

        case 0x9A:  /* Tiger Lake (0x9A60-0x9A70) */
            /*
 * Tiger Lake → fake Coffee Lake
 * engine base (VCS0=0x1c0000)
             */
            gen = 12;
            fFakeGen = 9;
            IODebug("Detected Tiger Lake (Gen12) → Faking Coffee Lake (Gen9)");
            break;

        case 0xA7:  /* Raptor Lake-P (0xA7AC, 0xA7Bx etc.) */
            /*
             * Raptor Lake = same Gen12.2 Xe_HPG IP as Alder Lake
 * engine register offsets ADL
 * → fake Coffee Lake (Gen9)
             */
            gen = 12;
            fFakeGen = 9;
            IODebug("Detected Raptor Lake (Gen12) → Faking Coffee Lake (Gen9)");
            break;

        default:
            /*
 * — pass-through
 * translateAddress() offset
             */
            gen = 0;
            fFakeGen = 0;
#ifdef EXTRA_DEBUG
            IODebug("Unknown device 0x%04X — pass-through mode", fDeviceID);
#endif
            break;
    }

    /*
 * 3: GMD_ID register ( Gen12+)
     *
     * GMD_ID_GRAPHICS (0xD8C):
     *   bit [31:22] = Architecture version (0x00C = 12 for TGL/ADL)
     *   bit [21:14] = Release (0x05 = 5 for ADL)
     *   bit [5:0]   = Stepping (revision)
     *
 * exception: register
 * fFakeGen
     *
     * Reference: intel_device_info.c intel_ipver_early_init()
     */
    if (fRegs && gen >= 12) {
        uint32_t gmdId = 0;

        /*
 * register 0xD8C
 * readReg32() dereference translation
 * ( 0xD8C translation)
         */
        gmdId = readReg32(GMD_ID_GRAPHICS);

        if (gmdId != 0 && gmdId != 0xFFFFFFFF) {
            /*
 * GMD_ID —
 * graphics IP version
             *
             * GMD_ID_ARCH_MASK   = bits 31:22 (>> 22)
             * GMD_ID_RELEASE_MASK = bits 21:14 (>> 14) & 0xFF
             */
            uint32_t arch = (gmdId >> 22) & 0x3FF;   /* Architecture */
            uint32_t rel  = (gmdId >> 14) & 0xFF;     /* Release */

            IODebug("GMD_ID = 0x%08X (arch=%u, rel=%u)", gmdId, arch, rel);

            /*
 * GMD_ID architecture = 0x00C (12)
 * → version
             */
            if (arch >= 12) {
 fGraphicsVer = (arch << 4) | rel; /* 12.5 → 0xC5 */
                fUseGmdId = true;
                IODebug("GMD_ID confirmed: graphics version %u.%u", arch, rel);
            }
        } else {
            /*
 * GMD_ID all-0 all-1 — register
 * device ID fallback
             */
            IODebug("GMD_ID not available — using PCI ID fallback");
        }
    }

    /*
 * fGraphicsVer set GMD_ID
     */
    if (fGraphicsVer == 0) {
        fGraphicsVer = gen;  /* fallback */
    }

    return gen;
}

#pragma mark -
#pragma mark - buildTranslationTable

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::buildTranslationTable()
 *
 * Register Offset
 *
 * :
 * - macOS Intel driver (AppleIntelCFLGraphics) compile
 * register layout Coffee Lake (Gen9)
 * - macOS Ring Tail Register VCS0
 * address = 0x12000 + 0x80 = 0x12080
 * - Alder Lake VCS0 register
 *      address = 0x1C0000 + 0x80 = 0x1C0080
 * - 0x12080 → → GPU crash
 *
 * : translateAddress() offset 0x12080
 * 0x1C0080
 *
 * Engine :
 *    VCS0:   fake 0x12000 → real 0x1C0000  (Video Decode Gen12+)
 *    VECS0:  fake 0x1A000 → real 0x1C8000  (Video Encode Gen12+)
 *    RCS0:   fake 0x02000 → real 0x02000  (Render — RENDER_RING_BASE=0x2000 on all gens)
 *    BCS0:   fake 0x22000 → real 0x22000   (Blitter = same)
 *
 *  Cursor:
 *    Pipe B: fake 0x700C0 → real 0x71080
 *    Pipe A/C: same
 *
 *  Reference:
 *    intel_engine_cs.c engine mmio_bases[]
 *    intel_display_regs.h CURSOR_*_BASE macros
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::buildTranslationTable(void)
{
    /*
      *
     */
    fTransCount = 0;
    memset(fTransTable, 0, sizeof(fTransTable));

    /*
 * fFakeGen == 9 gen != 9
 * → register offset
     *
 * fFakeGen == gen →
 * (engine register )
     */
    if (fFakeGen == fGraphicsVer || fFakeGen == 0) {
        IODebug("No translation needed — native mode");
        return;
    }

    IODebug("Building translation table (FakeGen=%u, RealGen=%u)", fFakeGen, fGraphicsVer);

    /*
 * Entry 0: RCS0 (Render) —
     *
 * entry debug
 * RCS0 offset (0x02000)
     */
    fTransTable[fTransCount].name       = "RCS0";
    fTransTable[fTransCount].fakeBase   = RCS0_BASE_FAKE;
    fTransTable[fTransCount].realBase   = RCS0_BASE_REAL;
    fTransTable[fTransCount].windowSize = ENGINE_WINDOW_SIZE;
 fTransTable[fTransCount].enabled = false; /* */
    fTransCount++;

    /*
 * Entry 2: BCS0 (Blitter) —
     *
 * offset (0x22000)
 * blit buffer / memory copy
     */
    fTransTable[fTransCount].name       = "BCS0 (Blitter)";
    fTransTable[fTransCount].fakeBase   = BCS0_BASE_FAKE;   /* 0x22000 */
    fTransTable[fTransCount].realBase   = BCS0_BASE_REAL;   /* 0x22000 */
    fTransTable[fTransCount].windowSize = ENGINE_WINDOW_SIZE;
 fTransTable[fTransCount].enabled = false; /* */
    fTransCount++;

    /*
 * Entry 3: VECS0 (Video Encode) —
     *
     * Coffee Lake: 0x1A000
     * Alder Lake:  0x1C8000
     *
     * offset delta = 0x1C8000 - 0x1A000 = 0x1AE000
     *
     * register: RING_TAIL, RING_HEAD, RING_CTL, RING_START, HWSTAM
     */
    fTransTable[fTransCount].name       = "VECS0 (Video Encode)";
    fTransTable[fTransCount].fakeBase   = VECS0_BASE_FAKE;   /* 0x1A000 */
    fTransTable[fTransCount].realBase   = VECS0_BASE_REAL;   /* 0x1C8000 */
    fTransTable[fTransCount].windowSize = ENGINE_WINDOW_SIZE;
    fTransTable[fTransCount].enabled    = true;
    fTransCount++;

    /*
 * Entry 4: Cursor Pipe B —
     *
     * Coffee Lake: CUR_BASE Pipe B = 0x700C0
     * Alder Lake:  CUR_BASE Pipe B = 0x71080
     *
 * register 0x700C0:
     *   +0x00 = CURCNTR  (cursor control)
     *   +0x04 = CURBASE  (cursor surface base address)
     *   +0x08 = CURPOS   (cursor position)
     *   +0x0C = CURBSIZE (cursor buffer size)
     *
 * Pipe B cursor offset Alder Lake
 * pipe register block
     *
     * Reference: intel_display_regs.h — CURSOR_A/B/C/D_OFFSET
     *            _CURABASE = 0x70080, _CURBBASE = 0x71080 (TGL+)
     */
    fTransTable[fTransCount].name       = "Cursor Pipe B";
    fTransTable[fTransCount].fakeBase   = CURSOR_B_FAKE;    /* 0x700C0 */
    fTransTable[fTransCount].realBase   = CURSOR_B_REAL;    /* 0x71080 */
 fTransTable[fTransCount].windowSize = 0x40; /* 64 bytes */
    fTransTable[fTransCount].enabled    = true;
    fTransCount++;

    /*
 * Entry 5: PCH (South Display) —
     *
     * Coffee Lake: PCH display registers at 0x48000-0x48FFF
     * Raptor Lake: PCH display registers at 0xC8000-0xC8FFF
     *
 * :
 * - Panel Power Sequencing (PP_CONTROL etc. PCH)
     *   - Backlight PWM (BLC_PWM_CTL = 0x48250 → 0xC8250)
     *   - PCH GPIO, PCH misc
     *
 * macOS PP_CONTROL 0xC50 (global) →
 * macOS PCH base 0x48000 →
     */
    fTransTable[fTransCount].name       = "PCH Display";
    fTransTable[fTransCount].fakeBase   = PCH_DISPLAY_BASE_FAKE;   /* 0x48000 */
    fTransTable[fTransCount].realBase   = PCH_DISPLAY_BASE_REAL;   /* 0xC8000 */
    fTransTable[fTransCount].windowSize = PCH_DISPLAY_WINDOW;      /* 0x1000 */
    fTransTable[fTransCount].enabled    = true;
    fTransCount++;

    /*
 * Entry 6: Cursor Pipe D — Alder Lake Coffee Lake
     *
 * macOS 0x73080 (pipe D cursor offset)
 * pipe D Coffee Lake
 * translation Gen12+
     *
 * FakeID → block pipe D
 * Coffee Lake pipe A, B, C
     *
 * disabled = true + real==fake → pass-through ( mode native)
 * fGraphicsVer >= 12 fFakeGen == 9 → pipe D
     */
    fTransTable[fTransCount].name       = "Cursor Pipe D";
    fTransTable[fTransCount].fakeBase   = CURSOR_D_FAKE;    /* 0x73080 */
    fTransTable[fTransCount].realBase   = CURSOR_D_REAL;    /* 0x73080 */
    fTransTable[fTransCount].windowSize = 0x40;
 fTransTable[fTransCount].enabled = false; /* — pipe D CFL */
    fTransCount++;

    /*
 * Done — debug
     */
    IODebug("Translation table built with %d entries:", fTransCount);
    for (int i = 0; i < fTransCount; i++) {
        IODebug("  [%d] %s: 0x%04X → 0x%04X %s",
                i,
                fTransTable[i].name,
                fTransTable[i].fakeBase,
                fTransTable[i].realBase,
                fTransTable[i].enabled ? "[active]" : "[skipped]");
    }
}

#pragma mark -
#pragma mark - translateAddress (CORE FAKEID LOGIC)

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t MyIntelGPU::translateAddress(uint32_t fakeOffset)
 *
 * FakeID — MMIO register offset
 *
 * offset macOS (Coffee Lake expected address)
 * offset Alder Lake
 *
 *  Algorithm:
 *    for each entry in fTransTable:
 *        if offset >= entry.fakeBase
 *        && offset <  entry.fakeBase + entry.windowSize:
 * if !entry.enabled → return offset ()
 *            realOffset = entry.realBase + (offset - entry.fakeBase)
 *            return realOffset
 * return offset ( entry → pass-through)
 *
 *  Edge Cases:
 * - Register common (0x60000 transcoder,
 *      0x70000 pipe, 0x44400 interrupts) → pass-through
 * - offset windowSize → return offset ("partial match"
 * )
 * - fFakeGen == 0 → return offset (unknown device)
 *
 *  Performance Note:
 * readReg32/writeReg32 ( framebuffer/
 * acceleration). loop 8 entries 1 read → ~8 cmp + branch.
 * x86_64 cost ≈ 2-3 ns →
 *
 * optimize: entry
 * (cursor, VCS0, VECS0, ...) entry = return
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t MyIntelGPU::translateAddress(uint32_t fakeOffset)
{
    /* 🚨 แก้ทาง: บังคับยอมให้คำสั่งช่วงแอดเดรส Global MMIO (0x190000 - 0x19FFFF) 
     * ของชิปยุคใหม่ ทะลุผ่านตัวแปลไปหาฮาร์ดแวร์ตรงๆ โดยไม่โดนสูตรแปลงค่า Coffee Lake เก่าเตะทิ้ง
     *  covers: GT interrupt regs, BT killer, flash cache */
    if (fakeOffset >= 0x190000 && fakeOffset <= 0x19FFFF) {
        return fakeOffset;
    }

    /*
 * FakeGen == 0 () →
 * Fake →
     */
    if (fFakeGen == 0 || fFakeGen == fGraphicsVer) {
        return fakeOffset;
    }

    /*
 * translation
     *
 * offset macOS fakeBase ~ fakeBase+size
 * → realBase + offset
     *
 * : fakeOffset = 0x12080
     *       entry.fakeBase = 0x12000, entry.realBase = 0x1C0000
     *       offset_in_window = 0x12080 - 0x12000 = 0x80
     *       realOffset = 0x1C0000 + 0x80 = 0x1C0080
     *       return 0x1C0080 ✓
     */
    for (int i = 0; i < fTransCount; i++) {
        RegisterTranslationEntry *entry = &fTransTable[i];

        if (!entry->enabled) {
            continue;
        }

        /*
 * offset entry
 * range check mask
 * ( window power of two)
         */
        if (fakeOffset >= entry->fakeBase &&
            fakeOffset <  entry->fakeBase + entry->windowSize) {

            /*
 * offset
             * offset_in_window = fakeOffset - entry->fakeBase
             * realOffset = entry->realBase + offset_in_window
             */
            uint32_t offset_in_window = fakeOffset - entry->fakeBase;
            uint32_t realOffset = entry->realBase + offset_in_window;

#ifdef EXTRA_DEBUG
            IODebug("translate: 0x%04X → 0x%04X (%s, +0x%X in window)",
                    fakeOffset, realOffset, entry->name, offset_in_window);
#endif

            return realOffset;
        }
    }

    /*
 * translation entry
 * offset register "" (common register)
 * 0x70000 (pipe register), 0x60000 (transcoder), 0x44400 (interrupt)
 * →
     */
    return fakeOffset;
}

#pragma mark -
#pragma mark - MMIO Read / Write

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t MyIntelGPU::readReg32(uint32_t offset)
 *
 * 32-bit register MMIO BAR0
 *
 *  Flow:
 *    1. translateAddress(offset) → realOffset (FakeID)
 * 2. fRegs != NULL (mapping )
 * 3. OSMemoryBarrier() — compiler barrier
 * 4. volatile pointer compiler optimize
 *       (volatile ∀ registers)
 *    5. OSSynchronizeIO() — memory barrier guarantee
 * /
 *    6. return value
 *
 *  Memory Barrier Explanation:
 * OSMemoryBarrier(): compiler reorder ()
 *    OSSynchronizeIO():   CPU memory fence (mfence/lfence)
 * DMA
 *
 *  ⚠️ Performance: OSSynchronizeIO() after a read is redundant on x86_64
 *  because the x86/x64 memory model guarantees that reads are not
 *  reordered with other reads (TSO — Total Store Order). A compiler
 *  barrier + volatile access is sufficient.
 *
 *  On ARM64 (Apple Silicon), reads *can* be reordered so a DMB is
 *  needed.  We use a read-specific barrier on ARM64 to avoid the
 *  expensive full mfence on x86_64.
 *
 *  Reference: Linux intel_uncore_read -> __raw_i915_read32 + mb()
 *             Apple IOKit: OSSynchronizeIO() ≈ dmb() on ARM64
 *                                          ≈ mfence on x86_64
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t MyIntelGPU::readReg32(uint32_t offset)
{
    uint32_t realOffset;
    uint32_t value;

    /*
 * offset Fake
     */
    realOffset = translateAddress(offset);

    /*
 * fRegs map address kernel space
     * → return 0xFF (all-ones = "no device")
     *
 * VMware virtual GPU: BAR0 map VA ( 0x2000) → address valid
 * guard isValidRegs() NULL check
     */
    if (!isValidRegs()) {
        IODebug("WARNING: readReg32(0x%04X) — invalid regs VA (fRegs=%p, va=0x%llX)",
                realOffset, fRegs, reinterpret_cast<uint64_t>(fRegs));
        return 0xFFFFFFFF;
    }

    /*
 * Compiler barrier: compiler
 * / memory register read
     */
    OSMemoryBarrier();

    /*
 * register volatile pointer:
 * - volatile → compiler ( cache)
 * - IOMemoryMap::getVirtualAddress() → virtual address BAR0
 * - fRegs uint8_t* +
 * realOffset byte offset
     */
    value = *((volatile uint32_t *)(fRegs + realOffset));

    /*
     * Hardware memory barrier:
     *
     * On x86_64: reads are strongly ordered (TSO model).  The volatile
     * access + compiler barrier (OSMemoryBarrier above) guarantees
     * the read happens in program order.  No full mfence needed.
     *
     * On ARM64: reads CAN be reordered — insert a DMB read barrier
     * to prevent the CPU from reordering this read with subsequent reads.
     *
     * writeReg32() still uses OSSynchronizeIO() to drain the store
     * buffer — that's where the full barrier is truly required.
     */
#if defined(__aarch64__)
    __asm__ volatile("dmb ishld" ::: "memory");
#endif

    return value;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::writeReg32(uint32_t offset, uint32_t value)
 *
 * 32-bit register MMIO BAR0
 *
 *  Flow:
 *    1. translateAddress(offset)
 * 2. fRegs
 *    3. volatile write
 * 4. OSSynchronizeIO() — !
 * write Engine Ring Register:
 * - RING_TAIL → fence → hardware value
 * - GFX_FLSH_CNTL → TLB flush → complete
 * - Interrupt masking → immediate effect
 * 5. (optional) extra IODelayWriteCombined() WC area
 *
 * NB: register write compiler barrier
 * write flush
 * OSSynchronizeIO()
 *
 *  Reference: Linux intel_uncore_write -> __raw_i915_write32 + mb()
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::writeReg32(uint32_t offset, uint32_t value)
{
    uint32_t realOffset;

    /*
 * offset Fake
     */
    realOffset = translateAddress(offset);

    /*
 * Guard: fRegs kernel space
 * (VMware virtual GPU map BAR0 VA ~0x2000 → invalid)
     */
    if (!isValidRegs()) {
        IODebug("WARNING: writeReg32(0x%04X, 0x%08X) — invalid regs VA (fRegs=%p)",
                realOffset, value, fRegs);
        return;
    }

    /*
 * register
 * volatile — CPU buffer
     */
    *((volatile uint32_t *)(fRegs + realOffset)) = value;

    /*
 * Memory Barrier — !
     *
 * write register
 * write transaction PCI bus return
     *
 * : CPU merge write
 * execute out-of-order → register
 * Ring Tail Register
 * ( flush → GPU tail → scheduler )
     *
     * OSSynchronizeIO() = full memory barrier (sfence/mfence)
     */
    OSSynchronizeIO();
}

#pragma mark -
#pragma mark - Aperture (BAR2) Access

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t readAperture32 / writeAperture32
 *
 * Aperture (BAR2 / GMADR) CPU-mappable window GGTT
 * - Coffee Lake: aperture 256MB ( 512MB)
 * - Alder Lake: aperture 256MB-1GB ( config)
 *
 * access WC (Write-Combined) mapping
 * CPU→GPU transfer
 *
 *  Linux counterpart:
 *    io_mapping_map_wc(&ggtt->iomap, offset) → ioremap_wc(bar2)
 *    ggtt->iomap.data = ioremap_wc(ggtt->gmadr.start, aperture_size)
 *
 *  macOS IOKit:
 *    mapDeviceMemoryWithIndex(2) → map BAR2
 * kIOMapWriteCombined WC semantics
 * ( default = cache-disabled (UC) safe )
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t MyIntelGPU::readAperture32(uint64_t apertureOffset)
{
    if (!fApertureVA) {
        IODebug("WARNING: readAperture32(0x%llX) — fApertureVA is NULL", apertureOffset);
        return 0xFFFFFFFF;
    }

    if (apertureOffset >= fApertureSize) {
        IODebug("WARNING: readAperture32(0x%llX) — out of bounds (size=0x%llX)",
                apertureOffset, fApertureSize);
        return 0xFFFFFFFF;
    }

    OSMemoryBarrier();
    uint32_t value = *((volatile uint32_t *)(fApertureVA + apertureOffset));
    OSSynchronizeIO();
    return value;
}

void MyIntelGPU::writeAperture32(uint64_t apertureOffset, uint32_t value)
{
    if (!fApertureVA) {
        IODebug("WARNING: writeAperture32(0x%llX) — fApertureVA is NULL", apertureOffset);
        return;
    }

    if (apertureOffset >= fApertureSize) {
        IODebug("WARNING: writeAperture32(0x%llX) — out of bounds", apertureOffset);
        return;
    }

    *((volatile uint32_t *)(fApertureVA + apertureOffset)) = value;

    /*
     * Write-Combined buffer flush
     *
 * aperture map WC:
 * CPU buffered writes flush
 * → IODelayWriteCombined() flush WC buffer
 * → OSSynchronizeIO()
     *
 * map UC (uncacheable):
 * write bus → flush
 * (≈ 1/10 bandwidth WC)
     *
 * OSSynchronizeIO() simple and safe
     */
    OSSynchronizeIO();

    /*
     * IODelayWriteCombined(address) — flush WC buffer
 * ( x86_64, IODelayWriteCombined WC region
 * flush pending writes)
 * *((volatile uint32_t *)(fApertureVA)) = value;
 * address destination
 * safest = OSSynchronizeIO()
     */
}

#pragma mark -
#pragma mark - GGTT Management

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::ggttInvalidate(void)
 *
 * TLB Invalidate GPU
 *
 * GGTT TLBs (Translation Lookaside Buffers) cache
 * page table entries PTE GSM
 * invalidate hardware PTE
 *
 * Linux i915 ggtt->invalidate() (gt/intel_ggtt.c:197-198, VERIFIED):
 *   intel_uncore_write_fw(uncore, GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);
 *   intel_uncore_read_fw(uncore, GFX_FLSH_CNTL_GEN6);
 *
 *  Register: GFX_FLSH_CNTL_GEN6 = 0x101008 (i915 gt/intel_gt_regs.h:1475)
 *    write GFX_FLSH_CNTL_EN (1<<0) → invalidate
 *    ⚠️ writing 0 = no-op (EN bit must be set) — the old kext wrote 0
 *
 * → dummy read (posting read) → invalidate complete
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::ggttInvalidate(void)
{
    if (!fRegs) {
        return;
    }

    /*
     * Write GFX_FLSH_CNTL_EN (bit0 = 1) — VERIFIED i915 intel_ggtt.c:197
     * The old kext wrote 0, which is a no-op (EN bit never set) — same
     * bug class as the RING_RESET_CTL masked-register lesson.
     */
    writeReg32(GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);

    /*
     * Dummy read — posting read (i915 intel_ggtt.c:198 intel_uncore_read_fw)
     * ensures the TLB invalidate is complete before any subsequent MMIO.
     */
    (void)readReg32(GFX_FLSH_CNTL_GEN6);
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::ggttInitHardware(void)
 *
 * GGTT Hardware Init (Gen8+) — fills fGsm + fGttTotal
 *
 * Flow (Linux i915 — drivers/gpu/drm/i915/gt/intel_ggtt.c):
 *   1. gen8_gmch_probe(): read SNB_GMCH_CTRL (PCI config 0x50)
 *      → gen8_get_total_gtt_size(): ggms = (v>>6) & 0x7
 *        size_bytes = (1 << ggms) << 20
 *        fGttTotal (entries) = size_bytes / 8 (gen8_pte_t = 8 bytes)
 *   2. ggtt_probe_common(): GSM phys = BAR0 phys + 8MB
 *      (GTTMMADR is 16MB on Gen8+, GTTADR offset = 16MB/2)
 *   3. Map GSM as UC (needs_wc_ggtt_mapping() == false on Gen12+)
 *
 * PTE array is NOT cleared — BIOS scanout mappings must survive.
 * gemBufferFindFreeRegion() scans for zero entries before binding.
 * ─────────────────────────────────────────────────────────────────
 */
bool MyIntelGPU::detectStolenMemory(void)
{
    if (fStolenBase != 0 && fStolenSize != 0) {
        return true;    /* already detected (PTE-run in ggttInitHardware) */
    }

    if (!fPCIDevice || !fRegs || !isValidRegs()) {
        IODebug("Stolen: SKIP — PCI/MMIO not ready");
        return false;
    }

    /*
     * Base: GEN6_DSMBASE (MMIO 0x1080C0, 64-bit) & GEN11_BDSM_MASK [63:20].
     * i915: i915_gem_stolen.c → intel_uncore_read64(uncore, GEN6_DSMBASE).
     * translateAddress() passes 0x1080C0 through (not in trans table).
     */
    uint32_t lo = readReg32(GEN6_DSMBASE);
    uint32_t hi = readReg32(GEN6_DSMBASE + 4);
    uint64_t base = (((uint64_t)hi << 32) | lo) & GEN11_BDSM_MASK;

    /*
     * Size: SNB_GMCH_CTRL (PCI 0x50) GMS field bits [15:8].
     * i915 gen8_get_stolen_size(): GMS < 0x10 → GMS << 25 (32MB steps).
     */
    uint16_t gmchCtl = fPCIDevice->configRead16(SNB_GMCH_CTRL);
    uint32_t gms = (gmchCtl >> BDW_GMCH_GMS_SHIFT) & BDW_GMCH_GMS_MASK;
    uint64_t size = 0;
    /* i915 gen8_get_stolen_size(): GMS < 0x10 → 32MB steps; 0x10..0x1F →
     * 512MB + 128MB steps. Old code masked to 5 bits and stopped at 0x14,
     * missing GMS 0x15..0x1F. */
    if (gms < 0x10) {
        size = (uint64_t)gms << 25;
    } else if (gms < 0x20) {
        size = (512ULL << 20) + ((uint64_t)(gms - 0x10) << 27);
    }

    if (base == 0 || size < (1920 * 1080 * 4)) {
        IODebug("Stolen: WARNING — DSMBASE=0x%llX GMS=%u size=%llu MB — detection failed",
                base, gms, size >> 20);
        fStolenBase = 0;
        fStolenSize = 0;
        return false;
    }

    fStolenBase = base;
    fStolenSize = (uint32_t)size;
    IODebug("GGTTDBG: stolenBase=0x%llX stolenSize=%u MB (%u bytes) — DSMBASE/GMS early detect",
            fStolenBase, fStolenSize >> 20, fStolenSize);
    return true;
}

bool MyIntelGPU::ggttInitHardware(void)
{
    if (!fPCIDevice) {
        IODebug("GGTT: SKIP — no PCI device");
        return false;
    }

    /*
     * ── Step 1: GTT size from GMCH_CTRL ──
     * Linux: gen8_gmch_probe() → gen8_get_total_gtt_size()
     */
    uint16_t gmchCtl = fPCIDevice->configRead16(SNB_GMCH_CTRL);
    uint16_t ggms    = (gmchCtl >> BDW_GMCH_GGMS_SHIFT) & BDW_GMCH_GGMS_MASK;

    /* Log FIRST — the old code returned before logging when ggms==0,
     * which is why no GGTT line ever appeared in the boot log. */
    IODebug("GGTT: GMCH_CTRL=0x%04X ggms=%u", gmchCtl, ggms);

    uint32_t gttSizeBytes = 0;
    if (ggms) {
        gttSizeBytes = (1U << ggms) << 20;
    } else if (fGraphicsVer >= 12) {
        /* Gen12 fallback: GGMS==0 → assume minimum 4MB GTT
         * (i915 clamps total to 4GB rather than failing). */
        IODebug("GGTT: WARNING — GGMS=0 on Gen12, assuming 4MB GTT");
        gttSizeBytes = 4U << 20;
    }

    if (gttSizeBytes == 0) {
        IODebug("GGTT: ERROR — invalid GTT size (GMCH_CTRL=0x%04X)", gmchCtl);
        return false;
    }

    fGttTotal = gttSizeBytes / GGTT_PTE_SIZE;   /* PTE entries */

    /*
     * ── Step 2: GSM base = BAR0 phys + 8MB ──
     * Linux: ggtt_probe_common() → pci_resource_start(GEN4_GTTMMADR_BAR)
     *                                + gen6_gttadr_offset()  (16MB/2)
     */
    UInt32 lo = fPCIDevice->configRead32(kIOPCIConfigBaseAddress0);
    UInt32 hi = fPCIDevice->configRead32(kIOPCIConfigBaseAddress0 + 4);
    bool   is64 = ((lo & 0x06) == 0x04);
    UInt64 bar0Phys = is64
        ? ((static_cast<UInt64>(hi & ~0xFU) << 32) | (lo & ~0xFU))
        : (lo & ~0xFU);
    UInt64 gsmPhys = bar0Phys + GEN8_GTTADR_OFFSET;

    IODebug("GGTT: GMCH_CTRL=0x%04X size=%uMB entries=%u GSM phys=0x%llX",
            gmchCtl, gttSizeBytes >> 20, fGttTotal, gsmPhys);

    /*
     * ── Step 3: Map GSM (UC — Gen12+, matching Linux ioremap) ──
     */
#if defined(kIOMapCacheInhibit)
    const IOOptionBits kGSMInhibit = kIOMapCacheInhibit;
#elif defined(kIOMapInhibitCache)
    const IOOptionBits kGSMInhibit = kIOMapInhibitCache;
#else
    const IOOptionBits kGSMInhibit = (1UL << kIOMapCacheShift);
#endif

    fGSMDesc = IOMemoryDescriptor::withPhysicalAddress(
                   static_cast<IOPhysicalAddress>(gsmPhys),
                   gttSizeBytes, kIODirectionInOut);
    if (!fGSMDesc) {
        IODebug("GGTT: ERROR — withPhysicalAddress failed");
        return false;
    }
    fGSMDesc->retain();

    fGSMMap = fGSMDesc->map(kIOMapAnywhere | kGSMInhibit);
    if (!fGSMMap) {
        fGSMMap = fGSMDesc->map(kIOMapAnywhere);
    }
    if (!fGSMMap) {
        IODebug("GGTT: ERROR — GSM map failed");
        fGSMDesc->release();
        fGSMDesc = NULL;
        return false;
    }

    fGsm = reinterpret_cast<volatile uint32_t *>(fGSMMap->getVirtualAddress());

    /*
     * ── Step 3b: GGTT PTE diagnostics (TEMPORARY) ──
     * gemBufferFindFreeRegion() reported "no free region" across all
     * 1M entries — impossible if the BIOS only set framebuffer PTEs.
     * Dump raw PTE values to find out whether GSM maps the real PTE
     * array. NOTE: do NOT zero the GGTT here — the active display
     * plane reads its framebuffer through the GGTT (DSPSURF is a GTT
     * offset), so clearing caused a black screen (reverted).
     */
    {
        volatile uint64_t *gsm64 = (volatile uint64_t *)fGsm;
        IOLog("GGTTDBG: fGsm=%p gttTotal=%u (0x%X)", fGsm, fGttTotal, fGttTotal);
        IOLog("GGTTDBG: PTE[0..7]   = %016llX %016llX %016llX %016llX %016llX %016llX %016llX %016llX",
              gsm64[0], gsm64[1], gsm64[2], gsm64[3],
              gsm64[4], gsm64[5], gsm64[6], gsm64[7]);
        IOLog("GGTTDBG: PTE[2048]   = %016llX  PTE[4096]  = %016llX",
              gsm64[2048], gsm64[4096]);
        IOLog("GGTTDBG: PTE[65536]  = %016llX  PTE[last-4] = %016llX",
              gsm64[65536], gsm64[fGttTotal - 4]);
        uint32_t nonzero = 0;
        for (uint32_t i = 0; i < 65536 && i < fGttTotal; i++) {
            if (gsm64[i] != 0) nonzero++;
        }
        IOLog("GGTTDBG: non-zero in first 65536 PTEs = %u", nonzero);
    }

    /* ── Step 3c: Zero GGTT PTEs beyond the active framebuffer ──
     * Root cause of "no free GGTT region": the GGTT was never zeroed
     * (i915 memsets it at probe; macOS has no driver for this iGPU, so
     * the BIOS/GOP framebuffer PTE run at page 0 is followed by raw
     * stolen-DRAM garbage). gemBufferFindFreeRegion() checks pte == 0,
     * so garbage makes every entry look "used".
     *
     * We must NOT zero the whole GGTT — the active plane reads its
     * framebuffer through the GGTT (DSPSURF is a GTT offset), and
     * clearing it caused a black screen (reverted). Instead: find the
     * end of the contiguous framebuffer PTE run (pte[i] == fbBase +
     * i*0x1000 | VALID) and zero everything after it.
     */
    {
        volatile uint64_t *gsm64 = (volatile uint64_t *)fGsm;
        uint64_t fbBase = gsm64[0] & ~0xFFFULL;
        uint32_t fbEnd  = 1;
        while (fbEnd < fGttTotal) {
            uint64_t expect = (fbBase + ((uint64_t)fbEnd << 12)) | 1ULL;
            if (gsm64[fbEnd] != expect) break;
            fbEnd++;
        }
        IOLog("GGTTDBG: framebuffer PTE run ends at page %u (%u MB)", fbEnd, (fbEnd * 4) / 1024);

        /*
         * Phase 4.2 REMOVED — do NOT overwrite fStolenBase/fStolenSize here.
         * Root cause of the 7MB VRAM report: createVRAMDescriptor() runs in
         * MyIntelFramebuffer::start(), deferred to t+1s — i.e. AFTER this
         * Phase 6 code. Writing fbEnd<<12 (1920×1080×4 = 8,294,400 B = 7.91MB)
         * made the descriptor 7.91MB → VRAM,totalMB = 7. The real stolen size
         * comes from detectStolenMemory() (DSMBASE/GMS), which
         * createVRAMDescriptor's Phase 4.3 fix calls when these are still 0.
         * The PTE scan above is for GGTT zeroing only.
         */
        IOLog("GGTTDBG: fb PTE run base=0x%llX pages=%u — zeroing only, stolen comes from DSMBASE/GMS",
              fbBase, fbEnd);

        if (fbEnd > 0 && fbEnd < fGttTotal) {
            for (uint32_t i = fbEnd; i < fGttTotal; i++) {
                gsm64[i] = 0;
            }
            OSSynchronizeIO();
            IOLog("GGTTDBG: zeroed GGTT pages [%u .. %u)", fbEnd, fGttTotal - 1);
        } else {
            IOLog("GGTTDBG: WARNING — fb scan abnormal (fbEnd=%u), NOT zeroing", fbEnd);
        }
    }

    /* ── Step 4: TLB invalidate ── */
    ggttInvalidate();

    /*
     * Phase 4.4 — VRAM pool capacity = GGTT addressable memory.
     * i915 clamps vm.total to 32 bits (intel_ggtt.c:1531-1536);
     * GGMS=3 → 8MB PTE array → 1M entries → 4GB. Advertising the full
     * GGTT-addressable size is honest — the GGTT can map this much
     * system RAM. WindowServer surface safety is governed by the FB
     * descriptor (createVRAMDescriptor), which maps real stolen DRAM.
     */
    fVramPoolSize = (uint64_t)fGttTotal * GEM_PAGE_SIZE;
    if (fVramPoolSize > (4096ULL << 20)) {
        fVramPoolSize = 4096ULL << 20;
    }
    IODebug("VRAMPOOL: capacity=%llu MB (%u GTT pages) — report target %llu MB",
            fVramPoolSize >> 20, fGttTotal, fVramPoolSize >> 20);

    IODebug("GGTT: OK — fGsm=%p fGttTotal=%u", fGsm, fGttTotal);
    return true;
}

#pragma mark -
#pragma mark - CPU Cache Flush

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::clflushRange(const void *addr, size_t size)
 *
 *  CPU Cache Line Flush
 *
 * CPU buffer WB (Write-Back) cache
 * GPU (dGPU / non-LLC platform)
 *
 *  Linux: drm_clflush_virt_range():
 * - inline asm: clflush [addr]
 * - flush cache line (64 bytes)
 *
 *  x86_64 CLFLUSH instruction:
 * clflush m8 — flush cache line address
 * address-aligned cache line
 *
 * :
 * CLFLUSH serializing instruction → flush
 * memory safe barrier
 *
 * compiler barrier (OSMemoryBarrier)
 * compiler reorder
 *
 * :
 * Gen9+ iGPU LLC (Last Level Cache) CPU
 * → coherency → flush
 * FakeID PAT → flush
 *
 *    Alder Lake = has_llc=1 (LLC shared) → WB access coherenct
 * memory types (WC, UC) → flush
 *
 *  Apple Silicon (ARM64):
 * CLFLUSH — syscall cache maintenance
 *    (dc cvac / dc cvau)
 * macOS for ARM64: IODelayWriteCombined()
 *    memcpy + OSSynchronizeIO()
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::clflushRange(const void *addr, size_t size)
{
    /*
 * validity
     */
    if (!addr || size == 0) {
        return;
    }

    /*
 * Align address cache line boundary (64 bytes)
     *
 * CLFLUSH cache line
 * align: flush = cache line +
 * ( flush )
     */
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)0x3F;   /* align 64 */
    uintptr_t end   = (uintptr_t)addr + size;

    /*
 * compiler barrier — flush
 * store commit
     */
    OSMemoryBarrier();

    /*
 * CLFLUSH cache line range
     *
 * inline assembly:
     *   "clflush (%0)" :: "r"(ptr)
 * → flush cache line address = ptr
     *
 * clflush virtual address; hardware
 * physical cache line
     */
#if defined(__x86_64__)
    for (uintptr_t ptr = start; ptr < end; ptr += 64) {
        __asm__ volatile (
            "clflush (%0)"
            :
            : "r"(ptr)
            : "memory"
        );
    }
#elif defined(__aarch64__)
    /*
 * ARM64 path ( Apple Silicon)
     *
 * syscall cache_invalidate dc cvac
     *
     * macOS kernel: flush_dcache64(addr, size, 0)
 * Apple API public
     *
     * IOMemoryDescriptor::flushProcessorCache() —
 * macOS internal
     *
 * (skeleton)
     */
    // __builtin_arm_dc(0, addr);  /* dc cvac */
#endif

    /*
 * compiler barrier flush
 * compiler store flush instruction
     */
    OSMemoryBarrier();
}

#pragma mark -
#pragma mark - Display / Panel Methods

bool MyIntelGPU::initCDCLK(void)
{
    if (!fRegs) return false;
    uint32_t cdclk = readReg32(CDCLK_CTL);
    IODebug("CDCLK_CTL = 0x%08X", cdclk);
    if (cdclk == 0 || cdclk == 0xFFFFFFFF) {
        IODebug("CDCLK not accessible");
        return false;
    }
    IODebug("CDCLK: frequency valid");
    return true;
}

bool MyIntelGPU::initDPLL(void)
{
    if (!fRegs) return false;
    uint32_t dpllEn = readReg32(DPLL0_ENABLE);
    IODebug("DPLL0_ENABLE = 0x%08X", dpllEn);
    dpllEn |= 0x80000000;
    writeReg32(DPLL0_ENABLE, dpllEn);
    IODebug("DPLL0 enabled");
    return true;
}

bool MyIntelGPU::panelPowerOn(void)
{
    if (!fRegs) return false;
    uint32_t ppStat = readReg32(PP_STATUS);
    IODebug("PP_STATUS = 0x%08X", ppStat);
    uint32_t ppCtl = readReg32(PP_CONTROL);
    IODebug("PP_CONTROL = 0x%08X", ppCtl);
    ppCtl |= 0x80000000;
    writeReg32(PP_CONTROL, ppCtl);
    IODebug("Panel power on requested");
    return true;
}

bool MyIntelGPU::initBacklight(void)
{
    if (!fRegs) return false;
    uint32_t fakeOffset = CFL_BLC_PWM_CTL;
    uint32_t realOffset = translateAddress(fakeOffset);
    if (fakeOffset != realOffset) {
        IODebug("Backlight translated 0x%04X → 0x%04X", fakeOffset, realOffset);
    }
    uint32_t pwm = readReg32(fakeOffset);
    IODebug("BLC_PWM_CTL = 0x%08X", pwm);
    pwm |= 0x80000000;
    writeReg32(fakeOffset, pwm);
    IODebug("Backlight PWM enabled");
    return true;
}

bool MyIntelGPU::setPanelBrightness(uint32_t brightnessPercent)
{
    if (!fRegs || !isValidRegs()) return false;
    if (brightnessPercent > 100) brightnessPercent = 100;

    uint32_t fakeOffset = CFL_BLC_PWM_CTL;
    uint32_t pwmCtl = readReg32(fakeOffset);
    pwmCtl |= 0x80000000; /* Ensure PWM is enabled */
    writeReg32(fakeOffset, pwmCtl);

    /* Compute duty cycle (0..0xFFFF scale) */
    uint32_t maxDuty = 0xFFFF;
    uint32_t dutyVal = (maxDuty * brightnessPercent) / 100;
    uint32_t dataOffset = fakeOffset + 4; /* BLC_PWM_DATA (0x48254 -> 0xC8254) */
    writeReg32(dataOffset, dutyVal);

    IODebug("setPanelBrightness: %u%% (duty=0x%08X at reg 0x%05X)",
            brightnessPercent, dutyVal, translateAddress(dataOffset));
    return true;
}

uint32_t MyIntelGPU::getPanelBrightness(void)
{
    if (!fRegs || !isValidRegs()) return 80;
    uint32_t dataOffset = CFL_BLC_PWM_CTL + 4;
    uint32_t dutyVal = readReg32(dataOffset) & 0xFFFF;
    uint32_t percent = (dutyVal * 100) / 0xFFFF;
    return (percent > 100) ? 100 : percent;
}

void MyIntelGPU::dumpDisplayStatus(void)
{
    if (!fRegs) return;
    IODebug("=== Display Status Dump ===");
    IODebug("  PP_STATUS  = 0x%08X", readReg32(PP_STATUS));
    IODebug("  PP_CONTROL = 0x%08X", readReg32(PP_CONTROL));
    IODebug("  CDCLK_CTL  = 0x%08X", readReg32(CDCLK_CTL));
    IODebug("  DPLL0_EN   = 0x%08X", readReg32(DPLL0_ENABLE));
    IODebug("  TRANS_CONF_A = 0x%08X", readReg32(TRANSCODER_A_BASE + 0x1C));
    IODebug("  PIPE_CONF_A  = 0x%08X", readReg32(PIPE_A_BASE + 0x244));
    IODebug("  CFL PWM     = 0x%08X", readReg32(CFL_BLC_PWM_CTL));
    uint32_t fakeDw = readReg32(PCH_DISPLAY_BASE_FAKE);
    uint32_t realDw = readReg32(PCH_DISPLAY_BASE_REAL);
    IODebug("  PCH fake 0x%04X = 0x%08X", PCH_DISPLAY_BASE_FAKE, fakeDw);
    IODebug("  PCH real 0x%04X = 0x%08X", PCH_DISPLAY_BASE_REAL, realDw);
    /* Plane 1A scanout registers (Gen9 SKL+ layout) */
    IODebug("  PLANE_CTL_1A   = 0x%08X (0x70180)", readReg32(0x70180));
    IODebug("  PLANE_OFFSET_1A= 0x%08X (0x70184)", readReg32(0x70184));
    IODebug("  PLANE_STRIDE_1A= 0x%08X (0x70188)", readReg32(0x70188));
    IODebug("  PLANE_POS_1A   = 0x%08X (0x7018C)", readReg32(0x7018C));
    IODebug("  PLANE_SIZE_1A  = 0x%08X (0x70190)", readReg32(0x70190));
    IODebug("  PLANE_SURF_1A  = 0x%08X (0x7019C) <- scanout GGTT page", readReg32(0x7019C));
    IODebug("  PLANE_SURFLIVE = 0x%08X (0x701AC) <- hw committed surf", readReg32(0x701AC));
    IODebug("===========================");
}

void MyIntelGPU::snapshotDisplayState(void)
{
    if (!fRegs || !isValidRegs()) return;
    fSavedPlaneCtl     = readReg32(PLANE_A_BASE + PLANE_CTL_OFFSET);
    fSavedPlaneStride  = readReg32(PLANE_A_BASE + PLANE_STRIDE_OFFSET);
    fSavedPlaneSize    = readReg32(PLANE_A_BASE + PLANE_SIZE_OFFSET);
    fSavedPlaneSurf    = readReg32(PLANE_A_BASE + PLANE_SURF_OFFSET);
    fSavedPipeConf     = readReg32(PIPE_A_BASE + 0x244);
    fSavedTransConf    = readReg32(TRANSCODER_A_BASE + 0x1C);
    fPlaneSnapshotValid = true;
    IODebug("Display snapshot (BIOS baseline): PLANE_CTL=0x%08X STRIDE=0x%08X SIZE=0x%08X SURF=0x%08X PIPE_CONF=0x%08X TRANS_CONF=0x%08X",
            fSavedPlaneCtl, fSavedPlaneStride, fSavedPlaneSize, fSavedPlaneSurf,
            fSavedPipeConf, fSavedTransConf);
}

void MyIntelGPU::dumpDelayedDisplayState(void)
{
    if (!fRegs || !isValidRegs()) return;
    uint32_t ctl = readReg32(PLANE_A_BASE + PLANE_CTL_OFFSET);
    uint32_t surf = readReg32(PLANE_A_BASE + PLANE_SURF_OFFSET);
    IODebug("Plane 1A after takeover: ENABLE=%s SURF=0x%08X",
            (ctl & 0x80000000) ? "YES" : "NO (DISABLED)", surf);
    if (fPlaneSnapshotValid) {
        IODebug("Compare: CTL 0x%08X → 0x%08X | SURF 0x%08X → 0x%08X | PIPE 0x%08X → 0x%08X | TRANS 0x%08X → 0x%08X",
                fSavedPlaneCtl, ctl, fSavedPlaneSurf, surf,
                fSavedPipeConf, readReg32(PIPE_A_BASE + 0x244),
                fSavedTransConf, readReg32(TRANSCODER_A_BASE + 0x1C));
        if ((fSavedPlaneCtl & 0x80000000) && !(ctl & 0x80000000)) {
            IODebug(">>> PLANE 1A WAS DISABLED after WindowServer takeover — NDRV release theory CONFIRMED");
        } else if ((fSavedPlaneCtl & 0x80000000) && (ctl & 0x80000000)) {
            IODebug(">>> Plane 1A STILL ENABLED — plane alive; desktop must be landing elsewhere (check SURF target)");
        }
        if (fSavedPipeConf != readReg32(PIPE_A_BASE + 0x244)) {
            IODebug(">>> PIPE_CONF_A CHANGED (0x%08X → 0x%08X) — pipe was reprogrammed",
                    fSavedPipeConf, readReg32(PIPE_A_BASE + 0x244));
        }
    }
}

/* Workloop-gated arm — safe to call from start() (non-workloop context).
 * setTimeoutMS: 10000 ms = 10 s (setTimeout() default scale is NANOSECONDS —
 * 10^7 ns = 10 ms, a real bug in 2.0.209). */
IOReturn MyIntelGPU::sDelayedDumpArmAction(OSObject *owner, void *arg0,
                                           void *arg1, void *arg2, void *arg3)
{
    MyIntelGPU *me = OSDynamicCast(MyIntelGPU, owner);
    if (me && me->fDelayedDumpTimer) {
        me->fDelayedDumpTimer->setTimeoutMS(10000);
    }
    return kIOReturnSuccess;
}

/* Mission C — honest EDID passthrough: place real Acer panel EDID on the
 * live display node (.Display_boot/display0/AppleDisplay*) so IODisplay/
 * ColorSync build a real profile instead of the FFFFFFFF fallback.
 * NOTE: node name carries suffix (AppleDisplay-<vendor>-<regid>) —
 * match by PREFIX, not equality. */
bool MyIntelGPU::injectEDIDOnce(void)
{
        /* Real panel EDID: KDB0924 / KD156N2930A02 (15.3\" 1920x1080 DP).
     * Source: Windows registry dump via Extract-EDID.ps1 2026-08-24;
     * checksum 0x01. Supersedes OCR-derived approximation. */
    static const UInt8 edid[128] = {
        0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x2C, 0x82, 0x24, 0x09, 0x00, 0x00, 0x00, 0x00,
        0x27, 0x22, 0x01, 0x04, 0xA5, 0x22, 0x13, 0x78, 0x02, 0x4B, 0x3D, 0x8F, 0x5C, 0x59, 0x91, 0x25,
        0x15, 0x4F, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2A, 0x36, 0x80, 0xA0, 0x70, 0x38, 0x1F, 0x40, 0x30, 0x20,
        0x35, 0x00, 0x58, 0xC2, 0x10, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x4B,
        0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x36, 0x00, 0x00, 0x00, 0xFE,
        0x00, 0x4B, 0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x32, 0x00, 0x01
    };

    IOLog("MyIntelGPU: [EDID] scan attempt #%d\n", fEdidRetries);

    OSData *edidData = OSData::withBytes(edid, sizeof(edid));
    if (!edidData) return false;

    bool legacyDone = false;
    bool oursDone   = false;

    /* Strategy 1: Navigate directly to .Display_boot/display0 */
    IORegistryEntry *display0 = IORegistryEntry::fromPath(
        "IOService:/AppleACPIPlatformExpert/PC00/AppleACPIPCI/GFX0@2/.Display_boot/display0",
        gIOServicePlane);
    if (display0) {
        display0->setProperty("IODisplayEDID", edidData);
        OSNumber *vid = OSNumber::withNumber((uint32_t)0x2C82, 32);
        OSNumber *pid = OSNumber::withNumber((uint32_t)0x0924, 32);
        if (vid) { display0->setProperty("DisplayVendorID", vid); vid->release(); }
        if (pid) { display0->setProperty("DisplayProductID", pid); pid->release(); }

        /* Also set on AppleDisplay child if present */
        IORegistryIterator *childIter = IORegistryIterator::iterateOver(display0, gIOServicePlane);
        if (childIter) {
            IORegistryEntry *child;
            childIter->reset();
            while ((child = childIter->getNextObject()) != NULL) {
                child->setProperty("IODisplayEDID", edidData);
            }
            childIter->release();
        }

        display0->release();
        legacyDone = true;
        IOLog("MyIntelGPU: [EDID] injected KDB0924/KD156N2930A02 onto display0 ✓\n");
    }

    /* Strategy 2: Fallback — recurse OUR OWN provider subtree (PCI GFX nub).
     * .Display_boot/display0/AppleDisplay are descendants of that nub.
     * Global-plane iteration never reaches these deep IONDRV nodes. */
    if (!legacyDone) {
    IORegistryEntry *prov = getProvider();
    IORegistryIterator *iter = prov ?
        IORegistryIterator::iterateOver(prov, gIOServicePlane, kIORegistryIterateRecursively) : NULL;
    bool injected = false;
    if (iter) {
        IORegistryEntry *obj = NULL;
        iter->reset();
        while ((obj = iter->getNextObject()) != NULL) {
                const char *nm = obj->getName(gIOServicePlane);
                if (nm && strncmp(nm, "AppleDisplay", 12) == 0) {
                    obj->setProperty("IODisplayEDID", edidData);
                    OSNumber *vid = OSNumber::withNumber((uint32_t)0x2C82, 32);
                    OSNumber *pid = OSNumber::withNumber((uint32_t)0x0924, 32);
                    if (vid) { obj->setProperty("DisplayVendorID", vid); vid->release(); }
                    if (pid) { obj->setProperty("DisplayProductID", pid); pid->release(); }
                    injected = true;
                    legacyDone = true;
                    IOLog("MyIntelGPU: EDID passthrough: injected KDB0924/KD156N2930A02 onto %s\n", nm);
                    break;
                }
        }
        iter->release();
    }
    if (!injected)
        IOLog("MyIntelGPU: [EDID] AppleDisplay node not found after all passes\n");
    }

    /* Strategy 3: OUR late-registered chain — display0 under
     * MyIntelFramebuffer appears ~t+30s via deferred start; keep
     * rescheduling until it exists and carries our panel EDID. */
    if (fDisplayFramebuffer) {
        IORegistryIterator *ourIter = IORegistryIterator::iterateOver(
            fDisplayFramebuffer, gIOServicePlane);
        uint32_t kids    = 0;
        uint32_t missing = 0;
        if (ourIter) {
            IORegistryEntry *obj = NULL;
            ourIter->reset();
            while ((obj = ourIter->getNextObject()) != NULL) {
                kids++;
                if (!obj->getProperty("IODisplayEDID")) {
                    obj->setProperty("IODisplayEDID", edidData);
                    OSNumber *vid = OSNumber::withNumber((uint32_t)0x2C82, 32);
                    OSNumber *pid = OSNumber::withNumber((uint32_t)0x0924, 32);
                    if (vid) { obj->setProperty("DisplayVendorID", vid); vid->release(); }
                    if (pid) { obj->setProperty("DisplayProductID", pid); pid->release(); }
                    missing++;
                    IOLog("MyIntelGPU: EDID passthrough: injected onto %s (our chain)\n",
                          obj->getName(gIOServicePlane));
                }
            }
            ourIter->release();
        }
        /* An empty subtree means our deferred chain hasn't been born yet —
         * keep rescheduling until display0 exists AND every node carries EDID. */
        oursDone = (kids > 0 && missing == 0);
        if (kids == 0)
            IOLog("MyIntelGPU: [EDID] our FB chain not ready yet (%u children)\n", kids);
        else if (missing)
            IOLog("MyIntelGPU: [EDID] reinjected %u/%u nodes of our chain\n", missing, kids);
    } else {
        oursDone = false;
    }

    edidData->release();
    return (legacyDone && oursDone);
}

void MyIntelGPU::sEdidTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, owner);
    if (!self || self->fStopping) return;

    self->fEdidRetries++;
    if (self->injectEDIDOnce()) {
        IOLog("MyIntelGPU: EDID passthrough: done after %d retries", self->fEdidRetries);
        return; /* stop rescheduling */
    }
    if (self->fEdidRetries < 3600 && sender) /* unlimited: retry every 2s for ~2 hours */
        sender->setTimeoutMS(2000);
}

void MyIntelGPU::armEDIDPassthrough(void)
{
    if (fEdidTimer) {
        IOLog("MyIntelGPU: [EDID] timer already armed\n");
        return;
    }
    fEdidTimer = IOTimerEventSource::timerEventSource(this,
                                                      &MyIntelGPU::sEdidTimerFired);
    if (!fEdidTimer) {
        IOLog("MyIntelGPU: [EDID] timer alloc FAILED\n");
        return;
    }
    IOWorkLoop *wl = getWorkLoop();
    if (!wl || wl->addEventSource(fEdidTimer) != kIOReturnSuccess) {
        IOLog("MyIntelGPU: [EDID] addEventSource FAILED\n");
        fEdidTimer->release();
        fEdidTimer = NULL;
        return;
    }
    fEdidRetries = 0;
    fEdidTimer->setTimeoutMS(2000);
    IOLog("MyIntelGPU: [EDID] passthrough ARMED (2s interval, max %d retries)\n", 45);
}

void MyIntelGPU::armPlaneSelfTest(void)
{
    uint32_t flag = 0;
    if (!PE_parse_boot_argn("myplane", &flag, sizeof(flag)) || !flag) return;

    IOLog("MyIntelGPU: [myplane] scanout self-test ARMED via boot-arg\n");
    fPlaneTimer = IOTimerEventSource::timerEventSource(this,
                                                       &MyIntelGPU::sPlaneTimerFired);
    if (!fPlaneTimer) {
        IOLog("MyIntelGPU: [myplane] timer alloc FAILED\n");
        return;
    }
    IOWorkLoop *wl = getWorkLoop();
    if (!wl || wl->addEventSource(fPlaneTimer) != kIOReturnSuccess) {
        IOLog("MyIntelGPU: [myplane] addEventSource FAILED\n");
        fPlaneTimer->release();
        fPlaneTimer = NULL;
        return;
    }
    /* t+15s: boot + WindowServer + IONDRV must be fully settled before we
     * touch PLANE_* registers; flipping too early races the GOP handoff. */
    fPlaneTimer->setTimeoutMS(15000);
}

void MyIntelGPU::sPlaneTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, owner);
    if (!self || self->fStopping) return;
    if (!self->isValidRegs() || !self->fGsm || !self->fGttTotal) {
        IOLog("MyIntelGPU: [myplane] GGTT/regs not ready — aborting self-test\n");
        return;
    }

    /* One-shot: flip plane 1A to our own GEM-backed framebuffer.
     * programPlane() fills solid blue + clflush + snapshots saved regs. */
    if (!self->fPlaneTestBuf) {
        self->fPlaneTestBuf = gemBufferCreate(
            12 * 1024 * 1024, 0,
            (uint32_t *)self->fGsm, self->fGttTotal,
            (void *)self->fApertureVA, self->fApertureSize);
    }
    if (!self->fPlaneTestBuf) {
        IOLog("MyIntelGPU: [myplane] scanout buffer alloc FAILED\n");
        return;
    }

    bool ok = self->programPlane(self->fPlaneTestBuf);
    IOLog("MyIntelGPU: [myplane] programPlane %s ggtt=0x%08X%s\n",
          ok ? "LATCHED ✓ — WE OWN SCANOUT" : "not-latched",
          self->fPlaneTestBuf->ggttOffset,
          ok ? "" : " (screen stays on IONDRV fb; no harm)");
}

bool MyIntelGPU::flipPlaneTo(MyIntelGEMBuffer *buf)
{
    if (!buf || buf->magic != GEM_BUFFER_MAGIC || !buf->ggttOffset || !buf->cpuAddr) return false;
    if (!fRegs || !isValidRegs()) return false;
    /* Plane already enabled+configured by programPlane(); a double-buffer
     * flip only retargets SURF. No CTL/STRIDE/SIZE writes, no re-snapshot
     * (re-snapshotting would record OUR OWN registers as "saved" state). */
    clflushRange(buf->cpuAddr, 8294400);
    writeReg32(PLANE_A_BASE + PLANE_SURF_OFFSET, buf->ggttOffset);

    /* Poll >=2 vblank periods (16.7ms @60Hz): a SURF write landing right
     * after a vblank commits up to 16.7ms later — 3.4ms reported false miss
     * even though the frame visibly latched. */
    uint32_t live = 0;
    for (int i = 0; i < 2600; i++) {
        IODelay(10);
        live = readReg32(PLANE_A_BASE + PLANE_SURFLIVE_OFFSET);
        if ((live & ~0xFFFU) == (buf->ggttOffset & ~0xFFFU)) return true;
    }
    IODebug("MyIntelGPU: [flip] NOT-LATCHED live=0x%08X want=0x%08X", live, buf->ggttOffset);
    return false;
}

void MyIntelGPU::armFlipTest(void)
{
    uint32_t flag = 0;
    if (!PE_parse_boot_argn("myflip", &flag, sizeof(flag)) || !flag) return;
    if (fPlaneTestBuf) return;   /* myplane already owns the test slot */

    IOLog("MyIntelGPU: [myflip] double-buffer flip test ARMED via boot-arg\n");
    for (int i = 0; i < 2; i++) {
        fFlipBufs[i] = gemBufferCreate(
            12 * 1024 * 1024, 0,
            (uint32_t *)fGsm, fGttTotal,
            (void *)fApertureVA, fApertureSize);
        if (!fFlipBufs[i]) {
            IOLog("MyIntelGPU: [myflip] buffer %d alloc FAILED\n", i);
            return;
        }
        /* Pre-fill distinct frames: programPlane() only ever fills blue, so
         * without this the B-buffer scans uninitialized memory (dark blink). */
        const uint32_t color = (i == 0) ? 0x000000FFu : 0x0000FF00u;  /* XRGB: blue | green */
        uint32_t *px = (uint32_t *)fFlipBufs[i]->cpuAddr;
        for (uint32_t n = 0; n < (8294400u / 4u); n++) px[n] = color;
        clflushRange(fFlipBufs[i]->cpuAddr, 8294400);
    }
    fFlipTimer = IOTimerEventSource::timerEventSource(this,
                                                      &MyIntelGPU::sFlipTimerFired);
    if (!fFlipTimer || !getWorkLoop() ||
        getWorkLoop()->addEventSource(fFlipTimer) != kIOReturnSuccess) {
        IOLog("MyIntelGPU: [myflip] timer setup FAILED\n");
        return;
    }
    fFlipIdx = 0;
    fFlipCount = 0;
    fFlipTimer->setTimeoutMS(15000);
}

void MyIntelGPU::sFlipTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, owner);
    if (!self || self->fStopping) return;

    /* First flip goes through programPlane() to enable/configure the plane
     * and snapshot the IONDRV registers; subsequent flips are SURF-only. */
    MyIntelGEMBuffer *buf = self->fFlipBufs[self->fFlipIdx];
    bool ok = (self->fFlipCount == 0)
        ? self->programPlane(buf)
        : self->flipPlaneTo(buf);
    if (!ok && self->fFlipCount > 0) {
        /* Keep alternating anyway — transient latch misses happen when the
         * vblank window is tight; aborting on first miss hides real stats. */
        IODebug("MyIntelGPU: [myflip] flip #%u miss", self->fFlipCount);
    }
    self->fFlipIdx ^= 1;
    self->fFlipCount++;

    if (self->fFlipCount >= 60) {
        IOLog("MyIntelGPU: [myflip] DONE — %u flips, last latch=%s\n",
              self->fFlipCount, ok ? "ok" : "miss");
        sender->cancelTimeout();
        return;
    }
    sender->setTimeoutMS(500);
}

void MyIntelGPU::armBridgeMirror(void)
{
    uint32_t flag = 0;
    if (!PE_parse_boot_argn("mybridge", &flag, sizeof(flag)) || !flag) return;
    if (fPlaneTestBuf || fFlipBufs[0]) return;   /* one scanout experiment per boot */

    IOLog("MyIntelGPU: [mybridge] mirror takeover ARMED via boot-arg\n");
    fBridgeBuf = gemBufferCreate(
        12 * 1024 * 1024, 0,
        (uint32_t *)fGsm, fGttTotal,
        (void *)fApertureVA, fApertureSize);
    if (!fBridgeBuf) {
        IOLog("MyIntelGPU: [mybridge] buffer alloc FAILED\n");
        return;
    }
    fBridgeTimer = IOTimerEventSource::timerEventSource(this,
                                                        &MyIntelGPU::sBridgeTimerFired);
    if (!fBridgeTimer || !getWorkLoop() ||
        getWorkLoop()->addEventSource(fBridgeTimer) != kIOReturnSuccess) {
        IOLog("MyIntelGPU: [mybridge] timer setup FAILED\n");
        return;
    }
    fBridgeTicks = 0;
    fBridgeTimer->setTimeoutMS(15000);
}

bool MyIntelGPU::createAcceleratorNode(uint32_t mode)
{
    if (fAccelerator) return true;   /* already up */

    IODebug("Phase 6c: Creating MyIntelAccelerator (mode=%u)", mode);
    fAccelerator = MyIntelAccelerator::withGPU(this, mode);
    if (!fAccelerator) {
        IODebug("Phase 6c: withGPU failed (alloc/init)");
        return false;
    }
    IOService *accelParent =
        (mode == kMyIntelAccelAttachUnderPCI) ? (IOService *)fPCIDevice
                                              : (IOService *)this;
    if (!accelParent || !fAccelerator->attachToParent(accelParent, gIOServicePlane)) {
        IODebug("Phase 6c: attachToParent failed — dropping accelerator");
        fAccelerator->release();
        fAccelerator = NULL;
        return false;
    }
    if (!fAccelerator->start(accelParent)) {
        IODebug("Phase 6c: start failed — dropping accelerator");
        fAccelerator->detachFromParent(accelParent, gIOServicePlane);
        fAccelerator->release();
        fAccelerator = NULL;
        return false;
    }
    IODebug("Phase 6c: MyIntelAccelerator registered");
    return true;
}

/* ── Phase B step-machine: split adoption into freeze-safe micro-steps ──
 * Round-1 lesson (timer one-shot): attach+start while WS renders = IOKit
 * gate deadlock, system-wide freeze. Stepping lets us observe between each
 * dangerous transition and stop at the exact failing stage. */
bool MyIntelGPU::accelStepAlloc(uint32_t mode)
{
    if (fAccelerator) { IODebug("accelStepAlloc: node already live"); return true; }
    if (fAccelStepObj) return true;   /* already allocated */
    fAccelStepMode = mode;
    fAccelStepObj = MyIntelAccelerator::withGPU(this, mode);
    IOLog("MyIntelGPU: [step-alloc] mode=%u -> %s\n",
          mode, fAccelStepObj ? "OK (object held)" : "FAILED");
    return fAccelStepObj != NULL;
}

bool MyIntelGPU::accelStepAttach(uint32_t parentSel)
{
    if (!fAccelStepObj) { IODebug("accelStepAttach: no step object"); return false; }
    /* parentSel: 0=this, 1=PCI, 2=IOResources (top council option — fully
     * decoupled from display-pipe topology) */
    IOService *parent = NULL;
    if (parentSel == 1)      parent = fPCIDevice;
    else if (parentSel == 2) {
        IORegistryEntry *re = IORegistryEntry::fromPath("IOResources", gIOServicePlane);
        parent = re ? OSDynamicCast(IOService, re) : NULL;
    }
    else                     parent = this;
    if (!parent) { IOLog("MyIntelGPU: [step-attach] parent resolve FAILED (%u)\n", parentSel); return false; }
    bool ok = fAccelStepObj->attachToParent(parent, gIOServicePlane);
    IOLog("MyIntelGPU: [step-attach] parentSel=%u %s\n", parentSel, ok ? "attached ✓" : "FAILED");
    if (!ok) {
        fAccelStepObj->release();
        fAccelStepObj = NULL;
    }
    return ok;
}

bool MyIntelGPU::accelStepStart(void)
{
    if (!fAccelStepObj) return false;
    IOService *parent = (fAccelStepMode == kMyIntelAccelAttachUnderPCI)
                        ? (IOService *)fPCIDevice : (IOService *)this;
    /* ⚠️ DANGER STEP: start() publishes nubs + registerService — the stage
     * that froze round-1. Run ONLY when observers are ready for a hang. */
    IOLog("MyIntelGPU: [step-start] BEGIN — freeze watch ON\n");
    bool ok = fAccelStepObj->start(parent);
    if (ok) {
        fAccelerator = fAccelStepObj;
        fAccelStepObj = NULL;
        IOLog("MyIntelGPU: [step-start] LIVE — accelerator adopted\n");
    } else {
        IOLog("MyIntelGPU: [step-start] failed — cleaning object");
        fAccelStepObj->detachFromParent(parent, gIOServicePlane);
        fAccelStepObj->release();
        fAccelStepObj = NULL;
    }
    return ok;
}

/* Door-A eGPU trick: live-inject the accelerator personality properties a
 * real GPU driver publishes, so SkyLight/wrangler have something to match.
 * Runs on the ALREADY-REGISTERED node — registry property change fires
 * notifications without needing another registerService cycle. */
bool MyIntelGPU::accelPadProps(void)
{
    if (!fAccelerator) { IOLog("MyIntelGPU: [pad-props] no live node\n"); return false; }
    fAccelerator->setProperty("IOAcceleratorClassName", "IOGraphicsAccelerator2");
    uint32_t accTypes = 0x1u;                       /* bit0 = acceleration capable */
    fAccelerator->setProperty("IOAccelTypes", accTypes, 32);
    uint32_t rendererID = 0x00001002u;              /* generic accel render id */
    fAccelerator->setProperty("IOVARendererID", &rendererID, sizeof(rendererID));
    fAccelerator->setProperty("IOGLBundleName", "MyIntelGPU");
    IOLog("MyIntelGPU: [pad-props] injected IOAcceleratorClassName/IOAccelTypes/"
          "IOVARendererID/IOGLBundleName onto live node\n");
    return true;
}

/* Hypothesis #11 — BCS engine reset: GDRST domain bit2 (@0x941C) then full
 * legacy-off reprogram + HWS/CSB reinit. This is the exact sequence that
 * un-stuck VDBOX decode originally; engines that were never properly
 * sanitized stay gated forever (queued-never-dispatch signature). */
bool MyIntelGPU::accelBCSReset(void)
{
    MyIntelRing *ring = fRingBCS;
    if (!ring || !fRingCallbacks) {
        IOLog("MyIntelGPU: [bcs-reset] missing ring/callbacks\n");
        return false;
    }
    if (!forceWakeGet()) {
        IOLog("MyIntelGPU: [bcs-reset] forcewake FAILED\n");
        return false;
    }

    uint32_t base = ring->mmioBase;
    uint32_t esr0 = readReg32(base + RING_ESR_OFFSET);

    bool ok = false;
    writeReg32(0x941Cu, 0x4u);                  /* GEN11_GRDOM_BLT */
    for (int i = 0; i < 1000; i++) {
        if ((readReg32(0x941Cu) & 0x4u) == 0) { ok = true; break; }
        IODelay(10);
    }

    ring->head = ring->tail;

    if (ok) {
        writeReg32(base + RING_HEAD_REG_OFFSET, 0);
        readReg32(base + RING_HEAD_REG_OFFSET);
        writeReg32(base + RING_TAIL_REG_OFFSET, 0);
        readReg32(base + RING_TAIL_REG_OFFSET);
        writeReg32(base + RING_MODE_GEN7_OFFSET, 0x00080008u);
        readReg32(base + RING_MODE_GEN7_OFFSET);
        writeReg32(base + RING_CONTEXT_STATUS_PTR_OFFSET, CONTEXT_STATUS_PTR_RESET);
        readReg32(base + RING_CONTEXT_STATUS_PTR_OFFSET);
    }
    if (ok && ring->hwspGgtt) {
        writeReg32(base + RING_HWS_PGA_OFFSET, ring->hwspGgtt);
        readReg32(base + RING_HWS_PGA_OFFSET);
        writeReg32(base + RING_HWS_STAM_OFFSET, ~0U);
        readReg32(base + RING_HWS_STAM_OFFSET);
        if (ring->hwspVaddr) {
            ring->hwspVaddr[HWS_CSB_WRITE_DWORD] = HWS_CSB_ENTRIES_GEN11 - 1;
            memset((uint8_t *)ring->hwspVaddr + HWS_CSB_BUF0_DWORD * 4, 0xFF,
                   HWS_CSB_ENTRIES_GEN11 * sizeof(uint64_t));
        }
    }

    forceWakePut();

    if (ok) { ring->head = 0; ring->tail = 0; ring->space = ring->size - 64; }

    IOLog("MyIntelGPU: [bcs-reset] %s — preESR=0x%08X postESR=0x%08X\n",
          ok ? "OK" : "TIMEOUT", esr0,
          ok ? readReg32(base + RING_ESR_OFFSET) : esr0);
    return ok;
}

/* Hypothesis #10 — BCS blit via BATCH+BB_START: the only submission pattern
 * proven on this silicon (gem_test RCS PASS, VDBOX 400fps). Direct-ring BLT
 * placement failed across every class encoding. Batch carries one
 * XY_FAST_COPY_BLT toward the bridge scanout buffer from stolen fb. */
bool MyIntelGPU::accelBatchBlit(void)
{
    /* Self-contained dst: bridge buf exists only under mybridge; probe must
     * work on ANY boot — lazily create a dedicated blit target instead. */
    if (!fBridgeBuf) {
        fBridgeBuf = gemBufferCreate(
            12 * 1024 * 1024, 0,
            (uint32_t *)fGsm, fGttTotal,
            (void *)fApertureVA, fApertureSize);
        if (!fBridgeBuf) {
            IOLog("MyIntelGPU: [batchblit] dst alloc FAILED\n");
            return false;
        }
        IOLog("MyIntelGPU: [batchblit] lazy dst alloc ggtt=0x%08X\n",
              fBridgeBuf->ggttOffset);
    }
    if (!fRingBCS || !fRingCallbacks || !fBridgeBuf) {
        IOLog("MyIntelGPU: [batchblit] missing ring/cb/bridgeBuf\n");
        return false;
    }

    if (!fAccelBatchBuf) {
        fAccelBatchBuf = gemBufferCreate(
            0x1000u, 0,
            (uint32_t *)fGsm, fGttTotal,
            (void *)fApertureVA, fApertureSize);
        if (!fAccelBatchBuf) {
            IOLog("MyIntelGPU: [batchblit] batch alloc FAILED\n");
            return false;
        }
        uint32_t *cs = (uint32_t *)fAccelBatchBuf->cpuAddr;
        cs[0]  = 0x50800008u;                       /* XY_FAST_COPY_BLT 10dw */
        cs[1]  = 7680u;                             /* dst pitch */
        cs[2]  = 0;                                 /* dst x1,y1 */
        cs[3]  = (1080u << 16) | 1920u;             /* y2,x2 */
        cs[4]  = fBridgeBuf->ggttOffset;            /* dst */
        cs[5]  = 0;
        cs[6]  = 0;                                 /* src x1,y1 */
        cs[7]  = 7680u;                             /* src pitch */
        cs[8]  = 0x0u;                              /* src = stolen fb p0 */
        cs[9]  = 0;
        cs[10] = MI_FLUSH_DW;                       /* 0x13000001 */
        cs[11] = MI_BATCH_BUFFER_END;               /* 0x05000000 */
        clflushRange(fAccelBatchBuf->cpuAddr, 0x1000u);
    }

    const uint64_t *bPhys = _gemGetPagesPhys(this, fAccelBatchBuf);
    if (bPhys)
        lrcMapBatchPages(fRingBCS, fAccelBatchBuf->ggttOffset, bPhys,
                         fAccelBatchBuf->size / 4096);

    /* Reset BEFORE submit: engines that were never sanitized stay gated.
     * Fresh BCS + legacy-off reprogram each probe call. */
    accelBCSReset();

    /* BB_START into BCS ring + USER_INTERRUPT + submit (VDBOX pattern) */
    if (!ringEmitBatchStart(fRingBCS, fAccelBatchBuf->ggttOffset)) {
        IOLog("MyIntelGPU: [batchblit] BB_START emit failed\n");
        return false;
    }
    ringEmitUserInterrupt(fRingBCS);
    ringSubmit(fRingBCS, fRingCallbacks);

    /* Execution oracle: LRC image head must advance past previous value */
    uint32_t *st = (uint32_t *)((uint8_t *)fRingBCS->lrcVaddr + LRC_STATE_OFFSET);
    uint32_t h0 = st[CTX_RING_HEAD];
    for (int i = 0; i < 300; i++) {
        IODelay(100);
        uint32_t h1 = st[CTX_RING_HEAD];
        if (h1 != h0) {
            IOLog("MyIntelGPU: [batchblit] EXECUTING — head %u -> %u\n", h0, h1);
            return true;
        }
    }
    IOLog("MyIntelGPU: [batchblit] head NOT advancing — engine still gated\n");
    return false;
}

void MyIntelGPU::armAccelDefer(uint32_t mode)
{
    fAccelDeferMode = mode;
    IOLog("MyIntelGPU: [myacceldefer] deferred accelerator ARMED (mode=%u, t+45s)\n", mode);
    fAccelDeferTimer = IOTimerEventSource::timerEventSource(this,
                                                      &MyIntelGPU::sAccelDeferTimerFired);
    if (!fAccelDeferTimer ||
        !getWorkLoop() ||
        getWorkLoop()->addEventSource(fAccelDeferTimer) != kIOReturnSuccess) {
        IOLog("MyIntelGPU: [myacceldefer] timer setup FAILED\n");
        if (fAccelDeferTimer) { fAccelDeferTimer->release(); fAccelDeferTimer = NULL; }
        return;
    }
    /* t+45s: WindowServer fully composited several seconds of frames on the
     * SW path before the GPU node appears — the whole point of deferral. */
    fAccelDeferTimer->setTimeoutMS(45000);
}

void MyIntelGPU::sAccelDeferTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, owner);
    if (!self || self->fStopping) { sender->cancelTimeout(); return; }

    IOLog("MyIntelGPU: [myacceldefer] creating accelerator NOW (post-stabilization)\n");
    if (self->createAcceleratorNode(self->fAccelDeferMode))
        IOLog("MyIntelGPU: [myacceldefer] node LIVE — watching SkyLight adoption\n");
    else
        IOLog("MyIntelGPU: [myacceldefer] creation FAILED\n");
}

void MyIntelGPU::sBridgeTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, owner);
    if (!self || self->fStopping) { sender->cancelTimeout(); return; }

    const uint32_t FB_BYTES = 8294400u;

    /* Lazy-alloc buffer B */
    if (!self->fBridgeBufB) {
        self->fBridgeBufB = gemBufferCreate(
            12 * 1024 * 1024, 0,
            (uint32_t *)self->fGsm, self->fGttTotal,
            (void *)self->fApertureVA, self->fApertureSize);
        if (self->fBridgeBufB) {
            uint32_t *px = (uint32_t *)self->fBridgeBufB->cpuAddr;
            for (uint32_t n = 0; n < FB_BYTES / 4u; n++) px[n] = 0x0000FF00u;
            self->clflushRange(self->fBridgeBufB->cpuAddr, FB_BYTES);
        }
    }

    /* ── FIRST TICK: bind A + refresh + evidence dump ── */
    if (self->fBridgeTicks == 0) {
        if (!self->programPlane(self->fBridgeBuf)) {
            IOLog("MyIntelGPU: [mybridge] initial bind failed\n");
            sender->setTimeoutMS(1000);
            return;
        }
        memcpy(self->fBridgeBuf->cpuAddr, (const void *)self->fApertureVA, FB_BYTES);
        self->clflushRange(self->fBridgeBuf->cpuAddr, FB_BYTES);
        self->fBridgeShownA = true;

        uint32_t *st = (uint32_t *)((uint8_t *)self->fRingBCS->lrcVaddr + LRC_STATE_OFFSET);
        for (uint32_t row = 0; row < 52; row += 8)
            IOLog("MyIntelGPU: [LRCXCS] dw%02u-%02u: %08X %08X %08X %08X %08X %08X %08X %08X\n",
                  row, row + 7,
                  st[row], st[row+1], st[row+2], st[row+3],
                  st[row+4], st[row+5], st[row+6], st[row+7]);

        IOLog("MyIntelGPU: [mybridge] BOUND ggtt=0x%08X\n",
              self->fBridgeBuf->ggttOffset);
        self->fBridgeTicks++;
        sender->setTimeoutMS(16);
        return;
    }

    MyIntelRing *bcs = self->fRingBCS;

    /* ── CIRCUIT BREAKER latched: pure CPU-copy refresh of shown buffer ── */
    if (self->fBridgeExeclistsOff) {
        MyIntelGEMBuffer *cur = self->fBridgeShownA ? self->fBridgeBuf : self->fBridgeBufB;
        memcpy(cur->cpuAddr, (const void *)self->fApertureVA, FB_BYTES);
        self->clflushRange(cur->cpuAddr, FB_BYTES);
        self->fBridgeTicks++;
        sender->setTimeoutMS(16);
        return;
    }

    /* ── Head sync from CONTEXT IMAGE + stall detection ── */
    uint32_t *st0 = (uint32_t *)((uint8_t *)bcs->lrcVaddr + LRC_STATE_OFFSET);
    uint32_t rawHead = st0[CTX_RING_HEAD] & (bcs->size - 1);
    bool advanced = (rawHead != bcs->head);
    bcs->head = rawHead;
    uint32_t used = (bcs->tail - bcs->head) & (bcs->size - 1);
    if (used > bcs->size - 64) used = 24;   /* wrapped-empty misread guard */
    bcs->space = (used < bcs->size) ? (bcs->size - used) : 0;

    /* ── DOUBLE-BUFFER: write hidden, flip SURF ── */
    MyIntelGEMBuffer *work = self->fBridgeShownA ? self->fBridgeBufB : self->fBridgeBuf;
    bool fed = false;
    const char *why = "";
    if (self->fRingBCS && ringIsInitialized(bcs)) {
        const uint64_t *wPhys = (const uint64_t *)self->_gemGetPagesPhys(self, work);
        if (wPhys) lrcMapBatchPages(bcs, work->ggttOffset, wPhys, work->size / 4096);
        {
            static uint64_t fbPhys[2025];
            static bool fbInit = false;
            if (!fbInit) { for (int i = 0; i < 2025; i++) fbPhys[i] = 0x4c800000ULL + (uint64_t)i * 4096; fbInit = true; }
            lrcMapBatchPages(bcs, 0, fbPhys, 2025);
        }
        if (ringEmitSurfacePresent(bcs, work->ggttOffset, 7680,
                                   0x0, 7680, 1920, 1080)) {
            ringEmitFlushDW(bcs, true, false);
            ringSubmit(bcs, self->fRingCallbacks);
            fed = true;
        } else why = "emit-failed";
    }
    if (!fed) {
        memcpy(work->cpuAddr, (const void *)self->fApertureVA, FB_BYTES);
        self->clflushRange(work->cpuAddr, FB_BYTES);
    }

    bool flipped = fed && self->flipPlaneTo(work);
    if (flipped) self->fBridgeShownA = !self->fBridgeShownA;
    self->fBridgeTicks++;

    /* Circuit breaker: repeated submits without consumption = engine gated.
     * Latch off execlists -> stable CPU-copy mode for rest of boot. */
    if (fed && !advanced) ++self->fBridgeStall;
    else if (advanced) self->fBridgeStall = 0;
    if (self->fBridgeStall >= 50 && !self->fBridgeExeclistsOff) {
        self->fBridgeExeclistsOff = true;
        IOLog("MyIntelGPU: [mybridge] CIRCUIT BREAKER — no consumption, "
              "execlists OFF for this boot\n");
    }

    if ((self->fBridgeTicks % 300) == 0) {
        IOLog("MyIntelGPU: [mybridge] tick=%u (%s%s) why=%s sp=%u h=%u t=%u "
              "stall=%u off=%d\n",
              self->fBridgeTicks,
              fed ? "HW blit" : "CPU copy",
              flipped ? "+flip" : "",
              why, bcs->space, bcs->head, bcs->tail,
              self->fBridgeStall, self->fBridgeExeclistsOff ? 1 : 0);
    }

    sender->setTimeoutMS(16);
}
void MyIntelGPU::sDelayedDumpTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *me = OSDynamicCast(MyIntelGPU, owner);
    if (me) {
        me->delayedDumpTimerFired(sender);
    }
}

void MyIntelGPU::delayedDumpTimerFired(IOTimerEventSource *sender)
{
    fAliveTick++;

    /* 2.0.227: first-IRQ NVRAM marker deferred out of the IRQ path — this
     * ticker runs on the workloop (thread context), safe for registry/NVRAM. */
    if (fFirstIrqSeen && !fFirstIrqMarked) {
        fFirstIrqMarked = true;
        mygpuProgress("irq:first-entry");
    }

    IODebug("ALIVE tick %u (t≈%us, myintelfb=1)", fAliveTick, fAliveTick * 5);
    if (fAliveTick <= 6) {
        char tag[32];
        snprintf(tag, sizeof(tag), "alive:%u", fAliveTick);
        mygpuProgress(tag);
    }

    /* 2.0.223: snapshot IRQ + display diagnostics every tick so the log
     * brackets the exact moment of the myintelfb=1 shutdown. */
    IOLog("MyIntelGPU: ALIVE tick %u: irqTotal=%llu gt0=%llu gt1=%llu "
          "disp=%llu (stormPend=%u) plane-on=%d\n",
          fAliveTick, fIrqTotal, fIrqGt0, fIrqGt1, fIrqDisplay, fStormCount,
          (fPlaneSnapshotValid && (fSavedPlaneCtl & 0x80000000)) ? 1 : 0);
    if (fFramebuffer) {
        fFramebuffer->dumpDiagnostics();
    }
    if (fDisplayFramebuffer) {
        fDisplayFramebuffer->dumpDiagnostics();
    }

    if (fAliveTick == 2) {
        IODebug("=== Delayed Display Status (t+10s, myintelfb=1) ===");
        dumpDelayedDisplayState();
    }

    /* 2.0.227: retry a kick that was skipped under lock contention — the
     * ALIVE ticker runs on the workloop, so a blocking lock here is safe
     * (the stall that 2.0.225 fixed was the IRQ action, not timers). */
    if (fKickPending) {
        IODebug("ALIVE tick %u: retrying pending kick (fKickPending)", fAliveTick);
        kickCommandSet2();
    }

    if (fAliveTick >= 60) {
        IODebug("ALIVE ticker done (t≈300s) — system still logging");
        if (sender) {
            sender->cancelTimeout();
            if (fWorkLoop) {
                fWorkLoop->removeEventSource(sender);
            }
            sender->release();
            fDelayedDumpTimer = NULL;
        }
        return;
    }
    if (sender) {
        sender->setTimeoutMS(5000);
    }
}

void MyIntelGPU::armDelayedDisplayDump(void)
{
    if (fDelayedDumpTimer) return;
    fDelayedDumpTimer = IOTimerEventSource::timerEventSource(
        this, &MyIntelGPU::sDelayedDumpTimerFired);
    if (!fDelayedDumpTimer) {
        IODebug("Delayed dump: no timer source");
        return;
    }
    IOWorkLoop *wl = getWorkLoop();
    if (!wl || wl->addEventSource(fDelayedDumpTimer) != kIOReturnSuccess) {
        IODebug("Delayed dump: addEventSource failed");
        fDelayedDumpTimer->release();
        fDelayedDumpTimer = NULL;
        return;
    }
    wl->runAction(&MyIntelGPU::sDelayedDumpArmAction, this, NULL, NULL, NULL, NULL);
    IODebug("Delayed display dump armed (IOTimerEventSource, t+10s, alive ticks every 5s)");
}

#pragma mark - Deferred Framebuffer Start (Phase 5d hang fix)

/* Workloop-gated arm — safe to call from start() (non-workloop context).
 * setTimeoutMS: 30000 ms = 30 s (setTimeout() default scale is NANOSECONDS —
 * 10^7 ns = 10 ms, same trap as 2.0.209's delayed dump).
 * Evidence 2026-08-26: t+1s start crashed the machine at Apple-logo stage
 * (myintelfb=1 A/B test) — IOFramebuffer registration collided with
 * IONDRVFramebuffer console ownership before logging came alive.
 * t+30s lets WindowServer/desktop settle so any failure is observable. */
IOReturn MyIntelGPU::sFramebufferStartArmAction(OSObject *owner, void *arg0,
                                                void *arg1, void *arg2, void *arg3)
{
    MyIntelGPU *me = OSDynamicCast(MyIntelGPU, owner);
    if (me && me->fFramebufferStartTimer) {
        me->fFramebufferStartTimer->setTimeoutMS(30000);
    }
    return kIOReturnSuccess;
}

void MyIntelGPU::sFramebufferStartTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelGPU *me = OSDynamicCast(MyIntelGPU, owner);
    if (me) {
        me->framebufferStartTimerFired(sender);
    }
}

void MyIntelGPU::framebufferStartTimerFired(IOTimerEventSource *sender)
{
    if (sender) {
        sender->cancelTimeout();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(sender);
        }
        sender->release();
        fFramebufferStartTimer = NULL;
    }

    if (!fDisplayFramebuffer) {
        IODebug("Deferred FB start: no framebuffer to start");
        return;
    }

    mygpuProgress("defer:fire");
    IODebug("Deferred FB start: MyIntelFramebuffer::start() (t+30s, workloop ctx)");
    if (!fDisplayFramebuffer->start(this)) {
        IODebug("  ERROR: MyIntelFramebuffer::start() failed (deferred)");
        mygpuProgress("defer:mfb-FAIL");
        fDisplayFramebuffer->detachFromParent(fPCIDevice, gIOServicePlane);
        fDisplayFramebuffer->release();
        fDisplayFramebuffer = NULL;
        return;
    }
    mygpuProgress("defer:mfb-done");
    IODebug("  MyIntelFramebuffer started successfully (deferred)");
    IODebug("  Starting interrupts (Phase 3 wiring)...");
    safeInitInterrupts();
    mygpuProgress("defer:irq-done");
}

void MyIntelGPU::armDeferredFramebufferStart(void)
{
    if (fFramebufferStartTimer) return;
    fFramebufferStartTimer = IOTimerEventSource::timerEventSource(
        this, &MyIntelGPU::sFramebufferStartTimerFired);
    if (!fFramebufferStartTimer) {
        IODebug("Deferred FB start: no timer source");
        return;
    }
    IOWorkLoop *wl = getWorkLoop();
    if (!wl || wl->addEventSource(fFramebufferStartTimer) != kIOReturnSuccess) {
        IODebug("Deferred FB start: addEventSource failed");
        fFramebufferStartTimer->release();
        fFramebufferStartTimer = NULL;
        return;
    }
    wl->runAction(&MyIntelGPU::sFramebufferStartArmAction, this, NULL, NULL, NULL, NULL);
    IODebug("Deferred framebuffer start armed (IOTimerEventSource, t+30s after Phase 7)");
}

bool MyIntelGPU::programPlane(MyIntelGEMBuffer *buf)
{
    if (!buf || !fRegs || !isValidRegs()) return false;
    if (buf->magic != GEM_BUFFER_MAGIC || !buf->ggttOffset || !buf->cpuAddr) return false;

    const uint32_t hActive = 1920;
    const uint32_t vActive = 1080;
    const uint32_t stride  = hActive * 4;          /* 7680 — 64B aligned */
    const uint32_t fbBytes = stride * vActive;     /* 8,294,400 */
    if (buf->size < fbBytes) {
        IODebug("programPlane: buffer too small (%u < %u)", buf->size, fbBytes);
        return false;
    }

    uint32_t *px = (uint32_t *)buf->cpuAddr;
    for (uint32_t i = 0; i < fbBytes / 4; i++) px[i] = 0x000000FF;  /* solid blue */
    clflushRange(buf->cpuAddr, fbBytes);

    const uint32_t planeBase = PLANE_A_BASE;

    /* Snapshot the previous plane state so restorePlane() can put the
     * scanout back when this buffer is destroyed (a client exiting must not
     * leave the plane pointing at freed GGTT — that blacks out the panel). */
    buf->planeSavedCtl   = readReg32(planeBase + PLANE_CTL_OFFSET);
    buf->planeSavedStride = readReg32(planeBase + PLANE_STRIDE_OFFSET);
    buf->planeSavedSize   = readReg32(planeBase + PLANE_SIZE_OFFSET);
    buf->planeSavedSurf   = readReg32(planeBase + PLANE_SURF_OFFSET);

    uint32_t ctl = buf->planeSavedCtl;
    ctl |= PLANE_CTL_ENABLE;
    writeReg32(planeBase + PLANE_CTL_OFFSET, ctl);

    /* PLANE_STRIDE is a 12-bit field in 64-byte units (i915 skl_program_plane:
     * intel_de_write(PLANE_STRIDE, stride / 64)). Writing raw bytes (7680)
     * truncates to 0x0E00 (3584) = 229,376 B/row, running scanout off the end
     * of the framebuffer -> "top striped, bottom black" + latched plane fault. */
    writeReg32(planeBase + PLANE_STRIDE_OFFSET, stride / 64);
    writeReg32(planeBase + PLANE_SIZE_OFFSET, ((vActive - 1) << 16) | (hActive - 1));
    writeReg32(planeBase + PLANE_SURF_OFFSET, buf->ggttOffset);

    uint32_t live = 0;
    bool latched = false;
    int loops_run = 0;

    for (int i = 0; i < 340; i++) {
        /* Read hardware state immediately on the first pass */
        live = readReg32(planeBase + PLANE_SURFLIVE_OFFSET);

        /* Mask the lower 4KB page offset to match the base GGTT boundary */
        if ((live & ~0xFFFU) == (buf->ggttOffset & ~0xFFFU)) {
            latched = true;
            loops_run = i;
            break;
        }

        /* Micro-delay window to allow hardware display engine to commit the plane */
        IODelay(50);
    }

    if (latched) {
        /* Success Path: Hardware fully committed the surface allocation */
        IODebug("MyIntelGPU: [programPlane] Page flip LATCHED successfully at GGTT=0x%08X (loops=%d)",
                buf->ggttOffset, loops_run);
    } else {
        /* Safety Path: Fallback graceful commit log as per driver original design */
        IODebug("MyIntelGPU: [programPlane] Page flip NOT-LATCHED within 17ms window (stale live=0x%08X)", live);
    }

    /* We wrote the plane registers, so we MUST restore them when the buffer
     * is destroyed — regardless of whether SURFLIVE latched within our poll
     * window (latched can be 0 here yet still commit a frame later; leaving
     * planeBound=false would let the plane scan freed GGTT = black panel). */
    buf->planeBound = true;
    IOLog("MyIntelGPU::[programPlane] planeBound=TRUE buf=%p ggtt=0x%08X\n", buf, buf->ggttOffset);
    return latched;
}

void MyIntelGPU::restorePlane(MyIntelGEMBuffer *buf)
{
    if (!buf || buf->magic != GEM_BUFFER_MAGIC || !buf->planeBound) return;
    if (!fRegs || !isValidRegs()) return;

    IOLog("MyIntelGPU::[restorePlane] buf=%p planeBound=%d surf=0x%08X\n",
          buf, buf->planeBound, buf->planeSavedSurf);
    const uint32_t planeBase = PLANE_A_BASE;
    uint32_t live = 0;
    /* Disable the plane first — clears any latched display-engine fault from
     * the test scanout (i915 skl_disable_plane: CTL=0, SURF=0). Writing the
     * saved registers back while the fault state is latched does not recover
     * the panel; the plane must be taken down and re-enabled. */
    writeReg32(planeBase + PLANE_CTL_OFFSET, 0);
    writeReg32(planeBase + PLANE_SURF_OFFSET, 0);
    for (int i = 0; i < 20; i++) {
        IODelay(10);
        live = readReg32(planeBase + PLANE_SURFLIVE_OFFSET);
        if ((live & ~0xFFFU) == 0) break;
    }
    writeReg32(planeBase + PLANE_STRIDE_OFFSET, buf->planeSavedStride);
    writeReg32(planeBase + PLANE_SIZE_OFFSET,   buf->planeSavedSize);
    writeReg32(planeBase + PLANE_SURF_OFFSET,   buf->planeSavedSurf);
    writeReg32(planeBase + PLANE_CTL_OFFSET,    buf->planeSavedCtl);
    for (int i = 0; i < 20; i++) {
        IODelay(10);
        live = readReg32(planeBase + PLANE_SURFLIVE_OFFSET);
        if ((live & ~0xFFFU) == (buf->planeSavedSurf & ~0xFFFU)) break;
    }
    buf->planeBound = false;
    IODebug("restorePlane: plane 1A restored to CTL=0x%08X SURF=0x%08X SURFLIVE=0x%08X",
            buf->planeSavedCtl, buf->planeSavedSurf, live);
}

bool MyIntelGPU::initDisplay(void)
{
    /*
     * Phase 4.3 gate: pipeline init (CDCLK/DPLL/panel power/backlight) is
     * OPT-IN via boot-arg myintelfbinit=1.
     *
     * Evidence 2026-08-13: with myintelfb=1 alone, writing DPLL0 /
     * PP_CONTROL / BLC_PWM on Gen12 RPL disturbed the BIOS/GOP pipeline
     * and blacked out the panel before WindowServer even started.
     * The BIOS plane (PLANE_SURF_1A = GGTT page 0) already scans stolen
     * base 0x4C800000, which IS our framebuffer aperture — so binding the
     * framebuffer to stolen works WITHOUT touching any display register.
     */
    uint32_t initArg = 0;
    if (!PE_parse_boot_argn("myintelfbinit", &initArg, sizeof(initArg)) || initArg == 0) {
        IODebug("=== Display Init SKIPPED (myintelfbinit absent) — BIOS pipeline untouched ===");
        return true;
    }

    IODebug("=== Display Init Start ===");
    bool ok = true;
    if (!initCDCLK())    { IODebug("CDCLK init FAILED");    ok = false; }
    if (!initDPLL())     { IODebug("DPLL init FAILED");     ok = false; }
    if (!panelPowerOn()) { IODebug("Panel power FAILED");   ok = false; }
    if (!initBacklight()){ IODebug("Backlight init FAILED");ok = false; }
    dumpDisplayStatus();
    IODebug("=== Display Init %s ===", ok ? "OK" : "PARTIAL");
    return ok;
}

#pragma mark -
#pragma mark - Phase 5 — HW Acceleration

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::initHardwareAcceleration(void)
 *
 *  Initialize GPU command streamer engines for basic HW acceleration.
 *
 *  Pipeline:
 *    1. Allocate GEM buffers for RCS + BCS ring buffers
 *    2. Create RCS ring (Render Command Streamer at 0x02200 on Gen12+)
 *    3. Create BCS ring (Blitter Command Streamer at 0x22000)
 *    4. Emit initial ring NOOP commands
 *    5. Submit initial state (rings ready at RING_TAIL=0)
 *
 * Phase 6 (GGTT invalidate) registerService()
 *
 * fail start() initHardwareAcceleration
 * (kext framebuffer-only)
 *
 *  Reference: Linux intel_ring_submission_setup()
 *             xcs_resume()
 *             intel_engine_create_ring()
 *///=========================================================================


void MyIntelGPU::disablePowerGating(void)
{
    if (!fRegs || !isValidRegs()) return;
    if (!fEngineLock) return;

    /* 2.0.215: same serialization as gtResetEngines — PG/RC6 teardown must
     * not race a concurrent ring kick holding the engine lock. */
    IOLockLock(fEngineLock);

    if (!forceWakeGet()) {
        IODebug("ERROR: disablePowerGating - forcewake acquire failed");
        IOLockUnlock(fEngineLock);
        return;
    }

    /* Disable Power Gating at MISCCPCTL_REG (0x9424) bit 0 (PG_DISABLE) */
    uint32_t miscC = readReg32(MISCCPCTL_REG);
    miscC |= MISCCPCTL_PG_DISABLE; /* Set bit 0 = 1 */
    writeReg32(MISCCPCTL_REG, miscC);

    /* Disable RC_CONTROL (0xA090) to ensure RC6 is off */
    writeReg32(RC_CONTROL, 0);

    IODebug("PG/RC6: Disabled (MISCCPCTL=0x%08X RC_CONTROL=0x%08X)",
            readReg32(MISCCPCTL_REG), readReg32(RC_CONTROL));

    forceWakePut();
    IOLockUnlock(fEngineLock);
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::gtResetEngines(void)
 *
 *  Full GT engine reset before execlist init.  Mirrors i915
 *  gt_sanitize() boot path (intel_gt_pm.c) → intel_gt_reset_all_engines()
 *  → gen8_reset_engines() (intel_reset.c):
 *
 *    1. per-engine gen8_engine_reset_prepare(): RING_RESET_CTL handshake —
 *       assert REQUEST_RESET (masked), wait READY_TO_RESET (700ms)
 *    2. gen6_hw_domain_reset(): write GEN6_GDRST = GEN11_GRDOM_FULL (0x1),
 *       poll until GDRST reads 0 (2000ms), loops=2 on ver<12.70 (RPL),
 *       then udelay(50)
 *    3. per-engine gen8_engine_reset_cancel(): deassert REQUEST_RESET
 *
 *  HW rejects execlist contexts unless engines start from a known
 *  reset state (observed EXECLIST_STATUS ACTIVE=0, CSB write ptr=0).
 *///=========================================================================

bool MyIntelGPU::gtResetEngines(void)
{
    if (!fRegs || !isValidRegs()) {
        IODebug("GT Reset: SKIP — MMIO not available");
        return false;
    }

    if (!fEngineLock) {
        IODebug("GT Reset: SKIP — engine lock not available");
        return false;
    }

    /* 2.0.215: serialize the whole handshake against IRQ-workloop kicks and
     * power-state transitions — a concurrent forcewake put or ring kick
     * would drop ELSP/reset writes mid-sequence (E2: WindowServer power
     * flap raced the wake). */
    IOLockLock(fEngineLock);

    /* i915 gt_sanitize() holds FORCEWAKE_ALL across the whole reset sequence
     * (intel_gt_pm.c: forcewake_get before engine prepare, put at the end).
     * RING_RESET_CTL is in the GT power well — without wake held, writes are
     * gated by RC6 and silently dropped, making the reset a no-op. */
    if (!forceWakeGet()) {
        IODebug("GT Reset: SKIP — forcewake acquire failed (reset would be dropped)");
        IOLockUnlock(fEngineLock);
        return false;
    }

    IODebug("GT Reset: begin (GDRST before=0x%08X)", readReg32(GDRST_REG));

    const uint32_t engineBases[2] = { RCS0_BASE_REAL, BCS0_BASE_REAL };
    const char *engineNames[2]    = { "RCS", "BCS" };
    bool allReady = true;

    /* ── 1. per-engine RING_RESET_CTL handshake (gen8_engine_reset_prepare) ── */
    for (int e = 0; e < 2; e++) {
        uint32_t reg = engineBases[e] + RING_RESET_CTL_OFFSET;
        uint32_t ack = readReg32(reg);
        IODebug("GT Reset: %s RING_RESET_CTL=0x%X (req=%u ready=%u cat=%u)",
                engineNames[e], ack,
                (ack >> 0) & 1u, (ack >> 1) & 1u, (ack >> 2) & 1u);

        uint32_t request, mask, want;
        if (ack & RESET_CTL_CAT_ERROR) {
            /* Catastrophic errors bypass the ready sequence (HAS#396813) */
            request = RESET_CTL_CAT_ERROR;
            mask    = RESET_CTL_CAT_ERROR;
            want    = 0;
            ack     = 0;
        } else if (!(ack & RESET_CTL_READY_TO_RESET)) {
            request = RESET_CTL_REQUEST_RESET;
            mask    = RESET_CTL_READY_TO_RESET;
            want    = RESET_CTL_READY_TO_RESET;
            ack     = RESET_CTL_READY_TO_RESET;
        } else {
            IODebug("GT Reset: %s already ready-to-reset", engineNames[e]);
            continue;
        }

        writeReg32(reg, RESET_CTL_MASKED_ENABLE(request));

        bool ok = false;
        for (int i = 0; i < 70000; i++) {          /* 700ms @ 10us */
            uint32_t v = readReg32(reg);
            if ((v & mask) == want) { ok = true; break; }
            IODelay(10);
        }
        if (!ok) {
            IODebug("GT Reset: WARNING — %s reset request timeout "
                    "{req=0x%X, RESET_CTL=0x%X}", engineNames[e],
                    request, readReg32(reg));
            allReady = false;
        } else {
            IODebug("GT Reset: %s ready-to-reset acked (RESET_CTL=0x%X)",
                    engineNames[e], readReg32(reg));
        }
    }

    /* ── 2. GDRST = GEN11_GRDOM_FULL, poll until 0 (gen6_hw_domain_reset) ── */
    int loops = 2;   /* GRAPHICS_VER_FULL < 12.70 → 2 (RPL = 12.55) */
    int err = 1;
    do {
        writeReg32(GDRST_REG, GDRST_GRDOM_FULL);
        err = 1;
        for (int i = 0; i < 200000; i++) {         /* 2000ms @ 10us */
            if ((readReg32(GDRST_REG) & GDRST_GRDOM_FULL) == 0) { err = 0; break; }
            IODelay(10);
        }
    } while (err == 0 && --loops);
    IODelay(50);   /* udelay(50) settle after ack */

    if (err) {
        IODebug("GT Reset: WARNING — GDRST wait failed (GDRST=0x%08X)",
                readReg32(GDRST_REG));
        allReady = false;
    } else {
        IODebug("GT Reset: GDRST acked (GDRST after=0x%08X)", readReg32(GDRST_REG));
    }

    /* ── 3. per-engine deassert REQUEST_RESET (gen8_engine_reset_cancel) ── */
    for (int e = 0; e < 2; e++) {
        uint32_t reg = engineBases[e] + RING_RESET_CTL_OFFSET;
        writeReg32(reg, RESET_CTL_MASKED_DISABLE(RESET_CTL_REQUEST_RESET));
        IODebug("GT Reset: %s REQUEST_RESET cleared (RESET_CTL=0x%X)",
                engineNames[e], readReg32(reg));
    }

    IODebug("GT Reset: done (ok=%s)", allReady ? "yes" : "no");

    forceWakePut();
    IOLockUnlock(fEngineLock);
    return allReady;
}

bool MyIntelGPU::initHardwareAcceleration(void)
{
    IODebug("=== HW Accel Init Start ===");

    if (!fRegs || !isValidRegs()) {
        IODebug("HW Accel: SKIP — MMIO not available");
        return false;
    }

    if (!fGsm || fGttTotal == 0) {
        /* Lazy init — covers the case where Phase 6 ordering failed. */
        IODebug("HW Accel: GGTT not ready — attempting lazy init");
        if (!ggttInitHardware() || !fGsm || fGttTotal == 0) {
            IODebug("HW Accel: SKIP — GGTT not available");
            return false;
        }
    }

    /* Phase B: Disable Power Gating and RC6 early */
    disablePowerGating();

    /*
     * ── Step 0: Allocate Ring Callbacks ──
     */
    fRingCallbacks = (MyIntelRingCallbacks *)IOMalloc(sizeof(MyIntelRingCallbacks));
    if (!fRingCallbacks) {
        IODebug("HW Accel: ERROR — failed to allocate callbacks");
        return false;
    }

    fRingCallbacks->context     = this;
    fRingCallbacks->readReg32   = _readReg32;
    fRingCallbacks->writeReg32  = _writeReg32;
    fRingCallbacks->gemAlloc    = _gemAlloc;
    fRingCallbacks->gemFree     = _gemFree;
    fRingCallbacks->gemGetOffset = _gemGetOffset;
    fRingCallbacks->gemGetVAddr = _gemGetVAddr;
    fRingCallbacks->gemGetPagesPhys = _gemGetPagesPhys;
    fRingCallbacks->fwGet       = _forceWakeGet;
    fRingCallbacks->fwPut       = _forceWakePut;
    fRingCallbacks->inIrqContext = _inIrqContext;

    /*
     * ── Step 1: Allocate GEM buffer for RCS ring ──
     */
    IODebug("HW Accel: Allocating RCS ring buffer...");

    fGemRingRCS = gemBufferCreate(
        GEM_RING_SIZE,
        GEM_FLAG_RING | GEM_FLAG_CPU_WRITE,
        (uint32_t *)fGsm,
        fGttTotal,
        (void *)fApertureVA,
        fApertureSize);

    if (!fGemRingRCS) {
        IODebug("HW Accel: WARNING — RCS ring GEM alloc failed, continuing");
        goto cleanup_callbacks;
    }

    /* Invalidate GGTT TLB after PTE writes */
    ggttInvalidate();

    /*
     * ── Step 2: Create RCS Ring ──
     */
    IODebug("HW Accel: pre-ring FW_ACK_GT=0x%08X GDRST=0x%08X CORE_STATUS=0x%08X",
            readReg32(FORCEWAKE_ACK_GT), readReg32(GDRST_REG),
            readReg32(GT_CORE_STATUS_REG));

    /* GuC ownership probe (i915 intel_guc_reg.h): if GuC fw is running
     * (uKernel state) and WOPCM locked, GuC owns the engines → ELSP
     * execlist submission is ignored by HW. */
    {
        uint32_t gucStatus   = readReg32(GUC_STATUS_REG);
        uint32_t wopcmSize   = readReg32(GUC_WOPCM_SIZE_REG);
        uint32_t wopcmOffset = readReg32(DMA_GUC_WOPCM_OFFSET_REG);
        IODebug("HW Accel: pre-ring GUC_STATUS=0x%08X (MIA_reset=%d bootrom=0x%X ukernel=0x%X mia=0x%X auth=%d)",
                gucStatus,
                (gucStatus & GS_MIA_IN_RESET) != 0,
                (gucStatus & GS_BOOTROM_MASK) >> GS_BOOTROM_SHIFT,
                (gucStatus & GS_UKERNEL_MASK) >> GS_UKERNEL_SHIFT,
                (gucStatus & GS_MIA_MASK) >> GS_MIA_SHIFT,
                (gucStatus & GS_AUTH_STATUS_MASK) >> GS_AUTH_STATUS_SHIFT);
        IODebug("HW Accel: pre-ring WOPCM_SIZE=0x%08X (locked=%d) DMA_OFFSET=0x%08X (valid=%d)",
                wopcmSize, (wopcmSize & GUC_WOPCM_SIZE_LOCKED) != 0,
                wopcmOffset, (wopcmOffset & GUC_WOPCM_OFFSET_VALID) != 0);
    }

    /* Full GT engine reset (i915 gt_sanitize boot path) — HW rejects
     * execlist contexts unless engines start from a known reset state. */
    gtResetEngines();

    IODebug("HW Accel: Creating RCS ring (mmio_base=0x%X)...", RCS0_BASE_REAL);

    fRingRCS = ringCreate(
        kMyIntelEngineRCS,
        RCS0_BASE_REAL,
        GEM_RING_SIZE,
        fRingCallbacks);

    if (!fRingRCS) {
        IODebug("HW Accel: WARNING — RCS ring init failed, continuing");
        goto cleanup_rcs_gem;
    }

    IODebug("HW Accel: RCS ring created OK (ggttOffset=0x%X, vaddr=%p)",
            fRingRCS->ggttOffset, fRingRCS->vaddr);

    /*
     * ── Step 3: Allocate GEM buffer for BCS ring ──
     */
    IODebug("HW Accel: Allocating BCS ring buffer...");

    fGemRingBCS = gemBufferCreate(
        GEM_RING_SIZE,
        GEM_FLAG_RING | GEM_FLAG_CPU_WRITE,
        (uint32_t *)fGsm,
        fGttTotal,
        (void *)fApertureVA,
        fApertureSize);

    if (!fGemRingBCS) {
        IODebug("HW Accel: WARNING — BCS ring GEM alloc failed, continuing without BCS");
        goto skip_bcs;
    }

    ggttInvalidate();

    /*
     * ── Step 4: Create BCS Ring ──
     */
    IODebug("HW Accel: Creating BCS ring (mmio_base=0x%X)...", BCS0_BASE_REAL);

    fRingBCS = ringCreate(
        kMyIntelEngineBCS,
        BCS0_BASE_REAL,
        GEM_RING_SIZE,
        fRingCallbacks);

    if (!fRingBCS) {
        IODebug("HW Accel: WARNING — BCS ring init failed, continuing without BCS");
        goto skip_bcs;
    }

    IODebug("HW Accel: BCS ring created OK (ggttOffset=0x%X, vaddr=%p)",
            fRingBCS->ggttOffset, fRingBCS->vaddr);

skip_bcs:

    /*
     * ── Step 5: Emit initial NOOPs and flush ──
     *
 * ring :
     *    1. MI_FLUSH_DW (flush caches)
     *    2. MI_USER_INTERRUPT (test interrupt)
     *    3. MI_NOOP (pad)
     *
 * submit GPU tail=tail
     */
    if (fRingRCS && ringIsInitialized(fRingRCS)) {
        IODebug("HW Accel: Emitting RCS init commands...");

        /* Flush caches */
        if (!ringEmitFlushDW(fRingRCS, true, true)) {
            IODebug("HW Accel: RCS flush emit failed (non-fatal)");
        }

        /* Pad + interrupt */
        ringEmitNOOP(fRingRCS);
        ringEmitUserInterrupt(fRingRCS);

        /* Submit to GPU */
        ringSubmit(fRingRCS, fRingCallbacks);
        IODebug("HW Accel: RCS init commands submitted (tail=%u)", fRingRCS->tail);
    }

    if (fRingBCS && ringIsInitialized(fRingBCS)) {
        IODebug("HW Accel: Emitting BCS init commands...");

        ringEmitFlushDW(fRingBCS, true, true);
        ringEmitNOOP(fRingBCS);
        ringEmitUserInterrupt(fRingBCS);

        ringSubmit(fRingBCS, fRingCallbacks);
        IODebug("HW Accel: BCS init commands submitted (tail=%u)", fRingBCS->tail);
    }

    /* Ring fetch runs inside the context's PPGTT (LEGACY_64B): map each
     * ring's backing pages into the identity window — otherwise the very
     * first command fetch hits a not-present PTE (ESR bit0, HEAD frozen,
     * element queued forever — case file a272fd1). */
    {
        const struct { void *buf; MyIntelRing *ring; const char *nm; } ringMaps[] = {
            { fGemRingRCS, fRingRCS, "RCS" },
            { fGemRingBCS, fRingBCS, "BCS" },
        };
        for (auto &rm : ringMaps) {
            if (!rm.buf || !rm.ring || !rm.ring->lrcInited) continue;
            const uint64_t *ph = _gemGetPagesPhys(this, rm.buf);
            uint32_t pages = rm.buf ? ((MyIntelGEMBuffer *)rm.buf)->size / 0x1000u : 0;
            if (ph && pages && lrcMapBatchPages(rm.ring, rm.ring->ggttOffset, ph, pages))
                IODebug("HW Accel: %s ring %u pages mapped into PPGTT", rm.nm, pages);
            else
                IODebug("HW Accel: %s ring PPGTT map FAILED (ph=%p pages=%u)", rm.nm, ph, pages);
        }
    }

    fAccelInitialized = true;
    IODebug("=== HW Accel Init %s ===",
            (fRingRCS || fRingBCS) ? "OK" : "FAILED");
    return (fRingRCS != NULL || fRingBCS != NULL);

    /*
     * ── Cleanup on failure ──
     */
cleanup_bcs_gem:
    if (fGemRingBCS) {
        gemBufferDestroy(fGemRingBCS, (uint32_t *)fGsm);
        fGemRingBCS = NULL;
    }
    /* Fall through */

cleanup_rcs_gem:
    if (fRingRCS) {
        ringDestroy(fRingRCS, fRingCallbacks);
        fRingRCS = NULL;
    }
    if (fGemRingRCS) {
        gemBufferDestroy(fGemRingRCS, (uint32_t *)fGsm);
        fGemRingRCS = NULL;
    }
    /* Fall through */

cleanup_callbacks:
    if (fRingCallbacks) {
        IOFree(fRingCallbacks, sizeof(MyIntelRingCallbacks));
        fRingCallbacks = NULL;
    }

    IODebug("=== HW Accel Init FAILED ===");
    /* Non-fatal — kext still runs in framebuffer mode */
    return false;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  Static Callback Trampolines
 *
 *  These bridge C-style function pointers from MyIntelRingCallbacks
 *  to MyIntelGPU instance methods.
 * ─────────────────────────────────────────────────────────────────
 */

uint32_t MyIntelGPU::_readReg32(void *context, uint32_t offset)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    return self ? self->readReg32(offset) : 0xFFFFFFFF;
}

void MyIntelGPU::_writeReg32(void *context, uint32_t offset, uint32_t value)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    if (self) self->writeReg32(offset, value);
}

void *MyIntelGPU::_gemAlloc(void *context, uint32_t size, uint32_t flags)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    if (!self) return NULL;

    /* Cast to volatile-aware pointer for GEM buffer */
    return (void *)gemBufferCreate(
        size,
        flags,
        (uint32_t *)self->fGsm,
        self->fGttTotal,
        (void *)self->fApertureVA,
        self->fApertureSize);
}

void MyIntelGPU::_gemFree(void *context, void *buffer)
{
    if (!buffer) return;

    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    if (!self) return;

    gemBufferDestroy((MyIntelGEMBuffer *)buffer, (uint32_t *)self->fGsm);
}

uint32_t MyIntelGPU::_gemGetOffset(void *context, const void *buffer)
{
    if (!buffer) return 0;

    const MyIntelGEMBuffer *gemBuf = (const MyIntelGEMBuffer *)buffer;
    return gemBuf->ggttOffset;
}

void *MyIntelGPU::_gemGetVAddr(void *context, const void *buffer)
{
    if (!buffer) return NULL;

    const MyIntelGEMBuffer *gemBuf = (const MyIntelGEMBuffer *)buffer;
    return gemBuf->cpuAddr;
}

const uint64_t *MyIntelGPU::_gemGetPagesPhys(void *context, const void *buffer)
{
    if (!buffer) return NULL;

    /* Per-page physical addresses (pagesPhys[]), resolved at buffer create —
     * IOMallocAligned does NOT guarantee physical contiguity, so the PPGTT
     * PTE map must use pagesPhys[i], never physAddr + i*PAGE. */
    const MyIntelGEMBuffer *gemBuf = (const MyIntelGEMBuffer *)buffer;
    return gemBuf->pagesPhys;
}

bool MyIntelGPU::_forceWakeGet(void *context)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    return self ? self->forceWakeGet() : false;
}

void MyIntelGPU::_forceWakePut(void *context)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    if (self) self->forceWakePut();
}

bool MyIntelGPU::_inIrqContext(void *context)
{
    MyIntelGPU *self = static_cast<MyIntelGPU *>(context);
    return self ? self->fInIrqContext : false;
}

#pragma mark -
#pragma mark - Phase 6 — Power Management

/*
 * Phase 6 — Power Management for Raptor Lake (Gen12.2)
 *
 * Extracted from XE driver: xe_pm.c, xe_force_wake.c, xe_gt_idle.c
 *
 * Three-layer architecture:
 *   1. Force Wake — wake GT domain before register access during RC6
 *   2. RC6 (Render Standby) — deep GPU idle state for power saving
 *   3. Display Panel Power — eDP VDD/BL enable sequencing
 *
 * Suspend flow (xe_pm_suspend):
 *   1. Disable C6 idle → force GT awake
 *   2. Save engine state (rings)
 *   3. Panel power off
 *
 * Resume flow (xe_pm_resume):
 *   1. Force wake GT → disable C6
 *   2. Restore engine state
 *   3. Re-enable C6
 *   4. Panel power on
 */

bool MyIntelGPU::forceWakeGet(void)
{
    if (!fRegs || !isValidRegs()) return false;

    /* 2.0.229 (ปฏิบัติการแหกตำรา, CRITICAL #2): IRQ-path fast path — the
     * engine that raised USER IRQ just executed commands, so it is awake by
     * definition. i915 gen11_gt_irq_handler() never calls forcewake; poll
     * the ACK once (no retry rounds, no 1ms inter-round delay) and defer on
     * timeout — the ALIVE ticker retries the pending kick (proven 2.0.228
     * pattern: "SKIP — retry pending"). */
    if (fInIrqContext) {
        uint32_t ackGt     = readReg32(FORCEWAKE_ACK_GT);
        uint32_t ackRender = readReg32(FORCEWAKE_ACK_RENDER);
        if ((ackGt & FW_BIT) && (ackRender & FW_BIT)) {
            return true;
        }
        IODebug("ForceWake: IRQ-path not awake (gt=0x%08X render=0x%08X) — deferring submit",
                ackGt, ackRender);
        return false;
    }

    /* Reference count — only wake on first get */
    if (fFWRefCount++ == 0) {
        /*
         * Wake the two forcewake domains that exist on Gen11+ (i915
         * intel_uncore_fw_domains_init): GT + RENDER.
         *
         * There is NO single MEDIA domain on Gen11+ — FORCEWAKE_MEDIA_GEN9
         * (0xA270 / ACK 0xD88) is Gen9/10-only; Gen11+ media engines use
         * per-VDBOX/VEBOX domains (0xA540+4i / ACK 0xD50+4i). Waking the
         * phantom 0xA270 and polling 0xD88 made the MEDIA ack read 0
         * forever → 50ms timeout → HW accel disabled (boot log 2.0.8:
         * "ForceWake: TIMEOUT ... media=0x00000000" while gt/render were
         * already 0x1 = awake).
         *
         * RCS ring registers (RCS0_BASE = 0x2000) sit in the RENDER domain
         * (i915 __gen12_fw_ranges: 0x2000-0x26ff → FORCEWAKE_RENDER).
         * Mirrors i915 fw_domains_get() + wait_ack_set (FORCEWAKE_KERNEL
         * bit 0): if the domain is already awake (ack bit 0 set — e.g.
         * BIOS/GOP held wake), pass immediately.
         *
         * Write protocol (i915 fw_set): _MASKED_BIT_ENABLE(FORCEWAKE_KERNEL)
         * = value(bit0) | mask(bit16) — same multicast form the kext used.
         */
        /* Hard retry pool (2.0.215): i915 WaRsForcewakeAddDelayForAck treats
         * a single ACK timeout as non-fatal, but a fake success here made
         * ringSubmitExeclists write ELSP while the RENDER well was still
         * asleep → write dropped → desc readback 0x0, HEAD/CSB frozen, ring
         * commands pile up (E2 evidence: myintelfb=1 hung boot). Retry the
         * wake request up to FW_RETRY_MAX rounds; return true ONLY on a
         * confirmed ACK, otherwise fail so the caller aborts instead of
         * programming a sleeping engine. */
        for (int attempt = 0; attempt < FW_RETRY_MAX; attempt++) {
            writeReg32(FORCEWAKE_GT,     FW_BIT | FW_MASK);
            writeReg32(FORCEWAKE_RENDER, FW_BIT | FW_MASK);

            /* Wait for ACK — poll GT + RENDER (i915 __wait_for_ack, 50ms) */
            for (int i = 0; i < FW_ACK_TIMEOUT_US / 10; i++) {
                uint32_t ackGt     = readReg32(FORCEWAKE_ACK_GT);
                uint32_t ackRender = readReg32(FORCEWAKE_ACK_RENDER);
                if ((ackGt & FW_BIT) && (ackRender & FW_BIT)) {
                    IODebug("ForceWake: domains awake (attempt %d: gt=0x%08X render=0x%08X)",
                            attempt + 1, ackGt, ackRender);
                    return true;
                }
                IODelay(10);  /* 10us per iteration */
            }
            IODelay(1000);  /* 1ms between rounds (WaRsForcewakeAddDelayForAck) */
        }

        IODebug("ForceWake: ERROR — ACK timeout after %d attempts "
                "(gt=0x%08X render=0x%08X); refusing fake success — engine "
                "regs would be dropped",
                FW_RETRY_MAX, readReg32(FORCEWAKE_ACK_GT),
                readReg32(FORCEWAKE_ACK_RENDER));
        return false;
    }

    return true;
}

void MyIntelGPU::forceWakePut(void)
{
    if (!fRegs || !isValidRegs()) return;
    if (fFWRefCount == 0) return;

    if (--fFWRefCount == 0) {
        /* Release force wake — clear request bits on GT + RENDER only */
        writeReg32(FORCEWAKE_GT,     FW_MASK);
        writeReg32(FORCEWAKE_RENDER, FW_MASK);
    }
}

bool MyIntelGPU::rc6Enable(void)
{
    if (!fRegs || !isValidRegs()) return false;
    if (!forceWakeGet()) return false;

    /* Set idle hysteresis to ~5 seconds (units of 1280ns)
     * (xe_gt_idle.c xe_gt_idle_enable_c6) */
    writeReg32(RC_IDLE_HYSTERSIS, RC_IDLE_HYST_VALUE);

    /* Enable RC6: HW_ENABLE | TO_MODE | RC6_ENABLE
     * (xe_gt_idle.c xe_gt_idle_enable_c6) */
    writeReg32(RC_CONTROL, RC_CTL_HW_ENABLE | RC_CTL_TO_MODE | RC_CTL_RC6_ENABLE);

    /* Verify */
    uint32_t rcCtl = readReg32(RC_CONTROL);
    fRC6Enabled = (rcCtl & RC_CTL_RC6_ENABLE) != 0;

    forceWakePut();

    IODebug("RC6: %s (RC_CONTROL=0x%08X)", fRC6Enabled ? "enabled" : "FAILED", rcCtl);
    return fRC6Enabled;
}

void MyIntelGPU::rc6Disable(void)
{
    if (!fRegs || !isValidRegs()) return;
    if (!forceWakeGet()) return;

    /* Disable RC6: clear both registers
     * (xe_gt_idle.c xe_gt_idle_disable_c6) */
    writeReg32(RC_CONTROL, 0);
    writeReg32(RC_STATE, 0);

    fRC6Enabled = false;

    forceWakePut();

    IODebug("RC6: disabled");
}

bool MyIntelGPU::gpuSuspend(void)
{
    IODebug("=== GPU Suspend ===");
    mygpuProgress("suspend:enter");

    if (!fRegs || !isValidRegs()) {
        IODebug("GPU Suspend: SKIP — MMIO not available");
        return true;
    }

    /* Step 1: Disable RC6 → force GT awake
     * (xe_pm_suspend → xe_gt_idle_disable_c6) */
    rc6Disable();

    /* Step 2: Save ring tail registers (shadow state)
     * GPU may be in RC6 — rings must be stopped first */
    if (fRingRCS) {
        IODebug("Suspend: RCS ring saved (tail=%u)", ringGetTail(fRingRCS));
    }
    if (fRingBCS) {
        IODebug("Suspend: BCS ring saved (tail=%u)", ringGetTail(fRingBCS));
    }

    /* Step 3: Panel power off
     * (xe_display_pm_suspend) */
    uint32_t ppCtl = readReg32(PP_CONTROL);
    ppCtl &= ~0x80000000;  /* Clear panel power enable */
    writeReg32(PP_CONTROL, ppCtl);
    IODelay(1000);  /* 1ms settle time */

    IODebug("=== GPU Suspend OK ===");
    mygpuProgress("suspend:done");
    return true;
}

bool MyIntelGPU::gpuResume(void)
{
    IODebug("=== GPU Resume ===");
    mygpuProgress("resume:enter");

    if (!fRegs || !isValidRegs()) {
        IODebug("GPU Resume: SKIP — MMIO not available");
        return true;
    }

    /* Step 1: Force wake GT → disable C6
     * (xe_pm_resume → xe_gt_idle_disable_c6) */
    if (!forceWakeGet()) {
        IODebug("Resume: ForceWake FAILED");
        return false;
    }

    /* Step 2: Panel power on
     * (xe_display_pm_resume) */
    panelPowerOn();

    /* Step 3: Restore rings
     * Re-submit tail to kick GPU processing */
    if (fRingRCS) {
        ringSubmit(fRingRCS, fRingCallbacks);
        /* Clear the one-shot gate: work queued before suspend was either
         * submitted by ringSubmit() above or was never staged — a stale
         * workPending=true would make the next kick append a phantom set.
         * Drop the pending batch queue too: entries queued before suspend
         * never had their BB_START emitted, so they are dead work. */
        fRingRCS->workPending = false;
        fRingRCS->pendingCount = 0;
        fRingRCS->pendingHead = 0;
        IODebug("Resume: RCS ring restored (tail=%u)", ringGetTail(fRingRCS));
    }
    if (fRingBCS) {
        ringSubmit(fRingBCS, fRingCallbacks);
        IODebug("Resume: BCS ring restored (tail=%u)", ringGetTail(fRingBCS));
    }

    /* Step 4: Re-enable RC6 for power saving
     * (xe_pm_runtime_resume → xe_gt_idle_enable_c6) */
    rc6Enable();

    forceWakePut();

    IODebug("=== GPU Resume OK ===");
    return true;
}

void MyIntelGPU::dumpPMStatus(void)
{
    if (!fRegs || !isValidRegs()) return;

    IODebug("=== PM Status Dump ===");
    IODebug("  FORCEWAKE_GT   = 0x%08X", readReg32(FORCEWAKE_GT));
    IODebug("  FORCEWAKE_ACK  = 0x%08X", readReg32(FORCEWAKE_ACK_GT));
    IODebug("  RC_CONTROL     = 0x%08X", readReg32(RC_CONTROL));
    IODebug("  RC_STATE       = 0x%08X", readReg32(RC_STATE));
    IODebug("  RC_IDLE_HYST   = 0x%08X", readReg32(RC_IDLE_HYSTERSIS));
    IODebug("  GT_CORE_STATUS = 0x%08X", readReg32(GT_CORE_STATUS_REG));
    IODebug("  PP_CONTROL     = 0x%08X", readReg32(PP_CONTROL));
    IODebug("  PP_STATUS      = 0x%08X", readReg32(PP_STATUS));
    IODebug("  GDRST          = 0x%08X", readReg32(GDRST_REG));
    IODebug("  MISCCPCTL      = 0x%08X", readReg32(MISCCPCTL_REG));
    IODebug("  FWRefCount     = %u", fFWRefCount);
    IODebug("  RC6Enabled     = %s", fRC6Enabled ? "yes" : "no");
    IODebug("========================");
}

#pragma mark -
#pragma mark - start

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::start(IOService *provider)
 *
 * ✅ MAIN ENTRY POINT Kext
 *
 * Pipeline ( i915_driver_probe + i915_driver_hw_probe):
 *
 *  ┌─────────────────────────────────────┐
 *  │ Phase 1: PCI Enable                 │ ← pci_enable_device()
 *  │   setBusMasterEnable(true)          │
 *  │   setMemoryEnable(true)             │
 *  ├─────────────────────────────────────┤
 *  │ Phase 2: BAR0 Mapping (MMIO Regs)   │ ← i915_driver_mmio_probe()
 *  │   mapDeviceMemoryWithIndex(0)       │
 *  │   → fRegs[*]  for MMIO access       │
 *  ├─────────────────────────────────────┤
 *  │ Phase 3: BAR2 Mapping (Aperture)    │ ← i915_ggtt_probe_hw()
 *  │   mapDeviceMemoryWithIndex(2)       │
 *  │   → fApertureVA, fApertureSize      │
 *  ├─────────────────────────────────────┤
 *  │ Phase 4: Detect Generation          │ ← intel_device_info_runtime_init_early()
 * │ → GMD_ID () │
 * │ → fFakeGen = 9 ( ADL) │
 *  ├─────────────────────────────────────┤
 *  │ Phase 5: Build Translation Table    │ ← Per-gen offset table
 *  │   → VCS0, VECS0, Cursor B           │
 *  ├─────────────────────────────────────┤
 *  │ Phase 6: GGTT Basic Init            │ ← i915_ggtt_init_hw()
 *  │   → read GTT_SIZE register          │
 *  │   → GGTT TLB invalidate             │
 *  ├─────────────────────────────────────┤
 *  │ Phase 6b: HW Acceleration Init      │ ← intel_ring_submission_setup()
 *  │   → RCS ring init (0x02000)         │   + GEM buffer alloc
 *  │   → BCS ring init (0x22000)         │
 *  │   → Emit HW init commands           │
 *  ├─────────────────────────────────────┤
 *  │ Phase 7: Register Service           │ ← drm_dev_register()
 *  │   → registerService()              │
 *  └─────────────────────────────────────┘
 *
 *  Reference:
 *    i915_driver.c int i915_driver_probe(struct pci_dev *pdev, ...)
 *      line 841 → calls i915_driver_mmio_probe, i915_driver_hw_probe
 *      line 800 → i915_driver_create — create struct
 *
 *    i915_driver.c i915_driver_mmio_probe(struct drm_i915_private *i915)
 *      line 326 → intel_uncore_init_mmio()
 *                 intel_device_info_runtime_init()
 *
 *    i915_driver.c i915_driver_hw_probe(struct drm_i915_private *i915)
 *      line 471 → i915_ggtt_probe_hw()
 *                 i915_ggtt_init_hw()
 *                 pci_set_master()
 * ─────────────────────────────────────────────────────────────────
 */
bool MyIntelGPU::start(IOService *provider)
{
    /*
     * ============================================================
     *  Phase 0: Super Call + Validation
     * ============================================================
     *
 * super::start() —
 * IOKit →
     */
    if (!super::start(provider)) {
        IODebug("ERROR: super::start() failed");
        return false;
    }

    /*
     * Phase 5 engine-kick lock — serializes kickCommandSet2() between the
     * IRQ workloop thread (processEngineInterrupt) and client threads
     * (submitClientTaskViaRing).  Must exist before armEngineInterrupts()
     * seeds the first kick at Phase 6, and before any client can submit.
     */
    fEngineLock = IOLockAlloc();
    if (!fEngineLock) {
        IODebug("ERROR: IOLockAlloc() failed — aborting start");
        return false;
    }

    /*
 * start() goto return
 * ( Linux i915) cleanup
     *
 * goto fail
     */
    bool        success  = false;
    uint32_t    barCount = 0;
    bool        gEnableDisplayFramebuffer = false; /* Phase 5d gate — see below */

    /*
     * ============================================================
     *  Phase 1: PCI Setup
     * ============================================================
     *
 * IOPCIDevice provider
 * (AppleIntelGraphics IOPCIDevice)
     *
     * Linux-equivalent:
     *   struct pci_dev *pdev = to_pci_dev(dev->dev);
     *   pci_enable_device(pdev);
     *   pci_set_master(pdev);
     */
    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice) {
        IODebug("ERROR: provider is not IOPCIDevice");
        goto fail;
    }

    /*
 * retain() — refcount kext
 * provider release retain
     */
    fPCIDevice->retain();

    /*
     * Enable PCI Memory Space + Bus Mastering
     *
 * command register :
     *   bit 1 = Memory Space Enable (MSE)
     *   bit 2 = Bus Master Enable (BME)
     *
     * Linux:
     *   pci_enable_device(pdev) → set PCI_COMMAND_MEMORY
     *   pci_set_master(pdev)    → set PCI_COMMAND_MASTER
     */
    fPCIDevice->setBusMasterEnable(true);
    fPCIDevice->setMemoryEnable(true);

    IODebug("Phase 1: PCI setup OK");
    mygpuProgress("start:P1");

    /*
     * ─── Fallback: kIOPCIConfigBaseAddress0 may not be defined in MacKernelSDK ───
     */
#ifndef kIOPCIConfigBaseAddress0
#define kIOPCIConfigBaseAddress0  0x10
#endif

    /*
     * ============================================================
     *  Phase 2: BAR0 (MMIO) Mapping
     * ============================================================
     *
     * BAR0 = 64-bit prefetchable memory (MMIO registers)
 * ≈ 2MB (0x200000) Gen9+
     *
     * macOS 15.2 VMware Guest note:
     *   IOPCIDevice::mapDeviceMemoryWithRegister() may fail with
     *   exclusive lock or bad flags.  Use getDeviceMemoryWithRegister()
     *   + IOMemoryDescriptor::map() instead.
     *
     * Descriptor strategies (in order):
     *   1. getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0)
     *   2. getDeviceMemoryWithIndex(0)
     *   3. PCI config space raw physical address fallback
     *
     * Mapping strategies (in order):
     *   1. IOMemoryDescriptor::map(kIOMapAnywhere | inhibit)
     *   2. IOMemoryDescriptor::map(kIOMapAnywhere)
     *   3. createMappingInTask(kernel_task, ...)  — legacy path
     */

    /*
     * ── Step 1: Get BAR0 memory descriptor ──
     */
    fMMIODesc = NULL;

    /* Strategy A — register-based (PCI config offset) */
    fMMIODesc = fPCIDevice->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);

    /* Strategy B — index-based */
    if (!fMMIODesc) {
        IODebug("BAR0: getDeviceMemoryWithRegister(0x%X) failed, "
                "trying getDeviceMemoryWithIndex(0)",
                kIOPCIConfigBaseAddress0);
        fMMIODesc = fPCIDevice->getDeviceMemoryWithIndex(0);
    }

    /* Strategy C — PCI config space raw read */
    if (!fMMIODesc) {
        IODebug("BAR0: IOKit methods failed, reading PCI config space directly");
        UInt32 lo = fPCIDevice->configRead32(kIOPCIConfigBaseAddress0);
        UInt32 hi = fPCIDevice->configRead32(kIOPCIConfigBaseAddress0 + 4);
        bool   is64 = ((lo & 0x06) == 0x04);
        UInt64 phys = is64
            ? ((static_cast<UInt64>(hi & ~0xFU) << 32) | (lo & ~0xFU))
            : (lo & ~0xFU);
        IODebug("BAR0: phys=0x%llX 64bit=%d", phys, is64);
        if (phys) {
            fMMIODesc = IOMemoryDescriptor::withPhysicalAddress(
                static_cast<IOPhysicalAddress>(phys),
                0x200000, kIODirectionInOut);
        }
    }

    if (!fMMIODesc) {
        IODebug("ERROR: Cannot get BAR0 descriptor (all strategies)");
        goto fail;
    }
    fMMIODesc->retain();
    IODebug("BAR0 desc: length=0x%llX", fMMIODesc->getLength());

    /*
     * ── Step 2: Map BAR0 into kernel address space ──
     *
     * Avoid createMappingInTask(kernel_task, ...) on macOS 15+ —
     * kernel_task is restricted → use IOMemoryDescriptor::map().
     */
    {
#if defined(kIOMapCacheInhibit)
        const IOOptionBits kCacheInhibit = kIOMapCacheInhibit;
#elif defined(kIOMapInhibitCache)
        const IOOptionBits kCacheInhibit = kIOMapInhibitCache;
#else
        const IOOptionBits kCacheInhibit = (1UL << kIOMapCacheShift);
#endif
        fMMIOMap = fMMIODesc->map(kIOMapAnywhere | kCacheInhibit);
        if (!fMMIOMap) {
            IODebug("BAR0: map(UC) failed, retrying map(default)");
            fMMIOMap = fMMIODesc->map(kIOMapAnywhere);
        }
        if (!fMMIOMap) {
            IODebug("BAR0: map() failed, retrying createMappingInTask");
            fMMIOMap = fMMIODesc->createMappingInTask(
                kernel_task, kIOMapAnywhere, 0, fMMIODesc->getLength());
        }
    }

    if (!fMMIOMap) {
        IODebug("ERROR: Cannot map BAR0 MMIO (all strategies)");
        goto fail;
    }

    fRegs = reinterpret_cast<volatile uint8_t *>(fMMIOMap->getVirtualAddress());
    IODebug("Phase 2: BAR0 MMIO mapped at 0x%p (size=0x%llX)",
            fRegs, fMMIODesc->getLength());
    mygpuProgress("start:P2");

    /*
     * ============================================================
     *  Phase 3: Detect Generation
     * ============================================================
     *
 * fRegs → detect device + GMD_ID
     *
     * Linux: intel_device_info_runtime_init_early()
     */
    detectHardwareGeneration();

    IODebug("Phase 3: Hardware detected — Gen=%u, FakeGen=%u%s",
            fGraphicsVer, fFakeGen,
            fUseGmdId ? " (GMD_ID)" : "");
    mygpuProgress("start:P3");

    /*
     * ============================================================
     *  Phase 4: BAR2 (Aperture) Mapping
     * ============================================================
     *
     * BAR2 = 64-bit prefetchable memory (GMADR)
 * = Aperture → CPU-mappable window GGTT
     *
     * Linux:
     *   ggtt->gmadr = pci_resource(pdev, 2);
     *   ggtt->iomap = io_mapping_create_wc(ggtt->gmadr.start,
     *                                       ggtt->mappable_end);
     *
     * macOS:
     *   mapDeviceMemoryWithIndex(2)
     *
 * BAR2 ( gen)
 * getLength() == 0 check
     *
 * detect hardware fFakeGen/fGraphicsVer
 * cache strategy (WC vs UC)
     */
    barCount = fPCIDevice->getDeviceMemoryCount();
    IODebug("Device memory count: %u", barCount);

    /*
     * ── Phase 4: BAR2 (GMADR) discovery — config-space verified ──
     *
     * The device-memory array is ordered by BAR presence (each entry's
     * tag = its config-space offset), NOT by BAR number. On this platform
     * (RPL iGPU) the array is [BAR0(0x10,16MB), BAR2(0x18,aperture),
     * stub(0x30,0x40 bytes)] — so getDeviceMemoryWithIndex(2) returns the
     * stub, never the real aperture. Identify BAR2 via config space.
     * (Note: GSM is NOT a separate BAR — it aliases BAR0+8MB.)
     *
     * Nested scope: keeps locals below the goto-fail jump target.
     */
    {
    UInt32 bar2lo = fPCIDevice->configRead32(kIOPCIConfigBaseAddress2);      /* 0x18 */
    UInt32 bar2hi = fPCIDevice->configRead32(kIOPCIConfigBaseAddress2 + 4);  /* 0x1C */
    bool   bar2Is64 = ((bar2lo & 0x06) == 0x04);
    UInt64 bar2Phys = static_cast<UInt64>(bar2lo & ~0xFULL);
    if (bar2Is64) bar2Phys |= (static_cast<UInt64>(bar2hi & ~0xFULL) << 32);
    IODebug("BAR2: config phys=0x%llX is64=%d", bar2Phys, bar2Is64);

    /* Diagnostic: dump every array entry (tag = config offset) */
    for (unsigned i = 0; i < barCount; i++) {
        IODeviceMemory *d = fPCIDevice->getDeviceMemoryWithIndex(i);
        IOPhysicalLength segLen = 0;
        IOPhysicalAddress segPhys = d
            ? d->getPhysicalSegment(0, &segLen, kIOMemoryMapperNone) : 0;
        IODebug("BAR2DBG: idx=%u tag=0x%X phys=0x%llX len=0x%llX",
                i, d ? d->getTag() : 0, segPhys, segLen);
    }

    IOMemoryDescriptor *rawAperture = NULL;
    uint64_t apertureLen = 0;

    /* 1) Prefer the tag-matched descriptor (index-independent) */
    fApertureDesc = fPCIDevice->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2);
    if (fApertureDesc) {
        apertureLen = fApertureDesc->getLength();
        fApertureDesc->retain();
    }

    /* 2) Raw physical fallback — bypasses any broken array entry entirely */
    if (bar2Phys && apertureLen < 0x1000000ULL) {   /* < 16MB → clearly wrong */
        IODebug("BAR2: descriptor len 0x%llX suspicious — using physical fallback", apertureLen);
        apertureLen = 0x10000000ULL;                 /* 256MB (RPL-U GT1) */
        rawAperture = IOMemoryDescriptor::withPhysicalAddress(
                          (IOPhysicalAddress)bar2Phys, apertureLen, kIODirectionInOut);
        if (rawAperture) {
            rawAperture->retain();
            if (fApertureDesc) { fApertureDesc->release(); fApertureDesc = NULL; }
            fApertureDesc = rawAperture;
        }
    }

    /* 3) Map — WC first (it's VRAM), UC as correctness fallback */
    if (fApertureDesc && apertureLen > 0) {
#if defined(kIOMapCacheInhibit)
        const IOOptionBits kApertureInhibit = kIOMapCacheInhibit;
#elif defined(kIOMapInhibitCache)
        const IOOptionBits kApertureInhibit = kIOMapInhibitCache;
#else
        const IOOptionBits kApertureInhibit = (1UL << kIOMapCacheShift);
#endif

        fApertureMap = fApertureDesc->map(kIOMapAnywhere | kIOMapWriteCombined);
        if (!fApertureMap) {
            fApertureMap = fApertureDesc->map(kIOMapAnywhere | kApertureInhibit);
        }
        if (!fApertureMap) {
            fApertureMap = fApertureDesc->map(kIOMapAnywhere);
        }

        if (fApertureMap) {
            fApertureVA = reinterpret_cast<volatile uint8_t *>(
                              fApertureMap->getVirtualAddress());
            fApertureSize = apertureLen;
            OSSynchronizeIO();
            IODebug("Phase 4: BAR2 aperture mapped at 0x%p (size=0x%llX)",
                    fApertureVA, fApertureSize);
            mygpuProgress("start:P4");
        } else {
            IODebug("WARNING: aperture unmappable — continuing (not required for accel)");
        }
    } else {
        IODebug("BAR2: no aperture descriptor — continuing aperture-less");
    }

    if (rawAperture) { rawAperture->release(); }   /* map holds its own ref */
    } /* end nested Phase 4 scope */

    /*
     * ============================================================
     *  Phase 5: Build Translation Table
     * ============================================================
     *
 * VCS0, VECS0, Cursor register
 * translateAddress() readReg32 / writeReg32
     */
    buildTranslationTable();

    IODebug("Phase 5: Translation table ready (%d entries)", fTransCount);
    mygpuProgress("start:P5");

    /*
     * ============================================================
     *  Phase 5b: Display / Panel Init
     * ============================================================
     *
 * Initialize display pipeline eDP:
     *    - CDCLK
     *    - DPLL0
     *    - Panel power sequencing
     *    - Backlight PWM
     *
 * NOTE: gated behind -myintelfb (same as Phase 5d). In GPU-only mode
     *      this kext must NOT touch display registers — writing DPLL0 /
     *      PP_CONTROL / BLC_PWM on Gen12 RPL disturbs the BIOS/NVRAM
     *      display pipeline (IONDRVFramebuffer) and blacks out the panel.
     *      Evidence 2026-08-13: main-flash boot (fFakeGen detected) ran
     *      initDisplay() -> black screen; backup-flash boot (unknown HW)
     *      skipped it -> screen fine.
     */
    gEnableDisplayFramebuffer = false;
    {
        uint32_t fbArg = 0;
        if (PE_parse_boot_argn("myintelfb", &fbArg, sizeof(fbArg))) {
            gEnableDisplayFramebuffer = (fbArg != 0);
        }
    }
    if (gEnableDisplayFramebuffer && fFakeGen != 0) {
        mygpuProgress("start:P5b-display");
        initDisplay();
        mygpuProgress("start:P5b-done");
    } else {
        IODebug("Phase 5b: Skipping display init (gEnableDisplayFramebuffer=%d fFakeGen=%d)",
                gEnableDisplayFramebuffer, fFakeGen);
    }

    /*
     * ============================================================
     *  Phase 5c: IntelFramebuffer — Object Only (No Interrupts Yet)
     * ============================================================
     *
 * Instance IntelFramebuffer
 * initInterrupts() Kernel Hang
 * Interrupt Controller
 * ExitBootServices
     *
     *  Lazy Init:
 * Interrupt enable safeInitInterrupts()
 * trigger setProperties()
 * IOFramebuffer start()
     *
     *  Debug:
 * log → Phase 5c
 * log → initDisplay() Phase
 * log → Phase 6+
     *
     *  Reference:
     *    i915_irq.c — intel_irq_install (called later, not in probe)
     *─────────────────────────────────────────────────────────────────
     */
    IODebug("Phase 5c: Creating IntelFramebuffer (no interrupts)...");
    mygpuProgress("start:P5c");

    fFramebuffer = new IntelFramebuffer;
    if (fFramebuffer) {
        if (!fFramebuffer->init(NULL)) {
            IODebug("ERROR: IntelFramebuffer::init() failed");
            fFramebuffer->release();
            fFramebuffer = NULL;
        }
    } else {
        IODebug("ERROR: Failed to allocate IntelFramebuffer");
    }

    IODebug("Phase 5c: Initialized without Interrupts");
    mygpuProgress("start:P5c-done");

    /*
     * ============================================================
     *  Phase 5d: MyIntelFramebuffer — IOFramebuffer Binding (Phase 4)
     * ============================================================
     *
 * IOFramebuffer subclass macOS framebuffer
 * IORegistry (Phase 4)
     *
 * MyIntelFramebuffer :
 * 1. Extend IOFramebuffer IOService
 * 2. display mode 1920x1080 @ 60Hz
 * 3. VRAM descriptor BAR2
 * 4. publish properties IORegistry
 * 5. Vblank interrupt → VSync (Phase 3)
     */
    /*
     * Phase 5d gate (default OFF):
     * MyIntelFramebuffer is an IOFramebuffer that WindowServer enumerates
     * as a phantom second display (fb1) → CDDisplay::present_update SIGBUS.
     * GPU-only mode: skip creating it unless boot-arg "myintelfb" is set.
     *
     * PE_parse_boot_argn() is the ONLY exported boot-arg API — it matches
     * both a bare "-myintelfb" flag and "myintelfb=1" (writes 1 into the
     * output for a bare flag). Earlier "bare flag not detected" logs were
     * caused by corrupted boot-args (the whole "sudo nvram boot-args=..."
     * command string stored as the value, with a trailing quote), not by
     * PE_parse_boot_argn. PE_boot_args()/PE_get_boot_args() are NOT
     * exported by the kernel (auxKC link fails) — never use them.
     * (boot-arg parsed once above in Phase 5b — gEnableDisplayFramebuffer reused here)
     */
    if (gEnableDisplayFramebuffer) {
    IODebug("Phase 5d: Creating MyIntelFramebuffer (IOFramebuffer)...");
    mygpuProgress("start:P5d");

    fDisplayFramebuffer = new MyIntelFramebuffer;
    if (fDisplayFramebuffer) {
        /*
         * init() — default state
         */
        if (!fDisplayFramebuffer->init(NULL)) {
            IODebug("  ERROR: MyIntelFramebuffer::init() failed");
            fDisplayFramebuffer->release();
            fDisplayFramebuffer = NULL;
        } else {
            /*
             * attachToParent — IORegistry tree
             * provider (IOPCIDevice) parent
             * framebuffer hierarchy GPU
             */
            if (!fDisplayFramebuffer->attachToParent(provider, gIOServicePlane)) {
                IODebug("  WARNING: attachToParent failed — continuing");
            }

            /*
             * Phase 5d hang fix (2.0.212): start()/registerService()/
             * safeInitInterrupts() are DEFERRED to a workloop timer fired
             * ~1s after Phase 7 — see armDeferredFramebufferStart().
             * Running them synchronously here blocked IOKit's serialized
             * matching / display-wrangler thread (ALIVE ticker proved the
             * workloop stayed alive while IOKit was stuck → boot hang).
             * Only new/init/attachToParent stay synchronous — they are
             * in-memory/registry ops that trigger no service matching.
             */
            IODebug("  MyIntelFramebuffer allocated+attached; start() deferred to t+1s (workloop)");
        }
    } else {
        IODebug("  ERROR: Failed to allocate MyIntelFramebuffer");
    }

    IODebug("Phase 5d: IOFramebuffer binding phase complete");
    mygpuProgress("start:P5d-done");
    } else {
        IODebug("Phase 5d: SKIPPED MyIntelFramebuffer (GPU-only mode; pass -myintelfb to enable display)");
    }

    /*
     * ============================================================
     *  Phase 6: GGTT Basic Init
     * ============================================================
     *
     * Read SNB_GMCH_CTRL (PCI 0x50) → GTT PTE array size,
     * map GSM (BAR0 phys + 8MB), fill fGsm/fGttTotal, TLB flush.
     *
     * Linux: i915_ggtt_init_hw() → ggtt_init_hw()
     *                → gen8_gmch_probe() + ggtt_probe_common()
     */
    ggttInitHardware();

    IODebug("Phase 6: GGTT init complete (entries=%u, gsm=%p)",
            fGttTotal, fGsm);
    mygpuProgress("start:P6");

    /*
     * Phase 4.3 evidence — myintelfb=1 only:
     * snapshot the BIOS pipeline AFTER GGTT init but BEFORE WindowServer
     * takes over, then re-dump at t+10s to see whether WindowServer /
     * NDRV release disabled plane 1A (frozen-logo hypothesis).
     */
    if (gEnableDisplayFramebuffer) {
        snapshotDisplayState();
        armDelayedDisplayDump();
    }

    /*
     * ============================================================
     *  Phase 6b: HW Acceleration Init (Ring + GEM)
     * ============================================================
     *
 * RCS + BCS ring buffers, alloc GEM pages, emit init cmds
     *
 * fail start() HW accel init
 * (kext framebuffer-only — boot )
     *
     *  Reference: Linux intel_ring_submission_setup()
     */
    initHardwareAcceleration();

    if (fAccelInitialized) {
        IODebug("Phase 6b: HW Acceleration OK (RCS=%s BCS=%s)",
                fRingRCS ? "yes" : "no",
                fRingBCS ? "yes" : "no");
    } else {
        IODebug("Phase 6b: HW Acceleration not available (framebuffer mode)");
    }
    mygpuProgress("start:P6b");
    armEDIDPassthrough();
    armPlaneSelfTest();
    armFlipTest();
    armBridgeMirror();
    {
        uint32_t deferMode = 0;
        if (PE_parse_boot_argn("myacceldefer", &deferMode, sizeof(deferMode)) && deferMode)
            armAccelDefer(deferMode);
    }

    /*
 * Phase 6b post: arm GT engine interrupts (GPU-only mode only).
     * Ring set 1 already submitted → a RCS0 USER completion IRQ
     * arriving here would kick command set 2 via processEngineInterrupt().
     * Display mode DEFERS the unified master interrupt to
     * safeInitInterrupts() inside the deferred-framebuffer timer (after
     * start() has fully returned). 2.0.225 fix: arming it here during
     * start() wedged the workloop at boot-time loads — the IRQ action
     * runs under the workloop gate, so starting the engine IRQ chain
     * while start() is still on the IOKit boot path stalls the gate and
     * starves the deferred-FB + ALIVE timers (system freeze / inert FB).
     */
    if (!fDisplayFramebuffer) {
        armEngineInterrupts();
    }

    /*
     * ============================================================
     *  Phase 7: Register Service
     * ============================================================
     *
 * registerService() → IOKit service
 * driver ( framebuffer) probe
     *
 * Linux: drm_dev_register() → device node
     *
     * attachToParent(provider, fPCIDevice) —
     * provider tree IOService event
     * (interrupt, power management)
     */
    /*
     * Phase 4.4 — Approach A: VRAM pool report on the GPU node itself.
     *
     * System Profiler reads VRAM,totalMB / VRAM,memSize from the GPU
     * IOService node (service plane) — no IOFramebuffer binding required
     * (WhateverGreen proves this path). This reaches the 4GB report goal
     * even while the myintelfb=1 framebuffer path is inactive (2.0.220:
     * root cause of "VRAM still 7MB" was MyIntelFramebuffer never being
     * created without -myintelfb → publishIORegistryProperties() never ran;
     * GPU node had no VRAM properties at all, so macOS fell back to 7MB).
     *
     * DEFAULT ON since 2.0.220 (no boot-arg required). Opt-out via
     * boot-arg "myintelvram=0" (reverse of the old opt-in gate).
     *
     * Value: fVramPoolSize (set in ggttInitHardware Phase 6, clamped to
     * min(4096MB, fGttTotal*4KB)) — the GTT-addressed VRAM pool.
     */
    {
        uint32_t vramPoolArg = 0;
        bool vramPoolOptOut  = (PE_parse_boot_argn("myintelvram", &vramPoolArg, sizeof(vramPoolArg)) && vramPoolArg == 0);
        if (!vramPoolOptOut && fVramPoolSize > 0) {
            uint64_t vramPoolMB = fVramPoolSize >> 20;
            OSData *mbData      = OSData::withBytes(&vramPoolMB, sizeof(vramPoolMB));
            OSData *bytesData   = OSData::withBytes(&fVramPoolSize, sizeof(fVramPoolSize));
            if (mbData && bytesData) {
                setProperty("VRAM,totalMB", mbData);
                setProperty("VRAM,memSize", bytesData);
                IOLog("MyIntelGPU: VRAM Pool report on GPU node — %llu MB (default ON, opt-out myintelvram=0)\n",
                      vramPoolMB);
            }
            if (mbData)    mbData->release();
            if (bytesData) bytesData->release();
        }
    }

    registerService();

    /*
     * ============================================================
     *  Phase 6c: IOAccelerator Binding (gated, default OFF)
     * ============================================================
     *
     * Publishes an IOAccelerator child service so macOS userspace sees
     * this GPU as an accelerator (system_profiler / ioreg / Metal probe
     * surface). Boot-arg gated exactly like myintelfb (Phase 5b idiom):
     *   absent  -> zero code runs, byte-identical GPU-only behavior
     *   =1      -> attach under this MyIntelGPU node
     *   =2      -> attach under the IOPCIDevice (Apple-style topology)
     *
     * Every sub-step is fail-safe: on any error we log, drop the object
     * and continue — accelerator creation can never fail start().
     */
    {
        uint32_t accelArg = 0;
        if (PE_parse_boot_argn("myintelaccel", &accelArg, sizeof(accelArg)) && accelArg != 0) {
            mygpuProgress("start:P6c-accel");
            createAcceleratorNode(accelArg);
        }
    }

    /*
     * Phase 5d hang fix (2.0.212): arm the deferred framebuffer start AFTER
     * our own registerService() returns — the timer fires ~1s later on the
     * workloop, i.e. once IOKit's serialized matching/display-wrangler thread
     * is no longer blocked inside our start(). See armDeferredFramebufferStart().
     */
    if (fDisplayFramebuffer) {
        armDeferredFramebufferStart();
    }

    /*
 * !
     */
    IODebug("Phase 7: Kext start completed successfully");
    mygpuProgress("start:P7-done");
    success = true;
    goto done;

    /*
     * ============================================================
     *  Error Handler — "Fail" Label
     * ============================================================
     *
 * phase → cleanup partial state
 * stop() IOKit
     *
 * ( goto out_err Linux i915_pci_probe)
     */
fail:
    IODebug("FAILED — cleaning up and returning false");
    mygpuProgress("start:FAIL");
    success = false;

    /*
     * stop() will be called by IOService
 * cleanup stop()
 * stop() (defensive)
     */

done:
    return success;
}

#pragma mark -
#pragma mark - stop

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::stop(IOService *provider)
 *
 * Kext — start()
 *
 * Linux: i915_driver_remove() → cleanup mirror probe
 *         intel_uncore_fini_mmio()
 *         i915_ggtt_driver_release()
 *
 * macOS: IOMemoryMap, release IOPCIDevice
 *
 * : IOKit stop() free()
 * error init → stop()
 * free() defense
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::stop(IOService *provider)
{
    /* 2.0.227: set the stopping gate FIRST — blocks deferred arming
     * (safeInitInterrupts / setProperties / deferred timers) from racing
     * this teardown (review CRITICAL: stop() vs deferred arming). */
    fStopping = true;

    IODebug("stop() — releasing resources");
    mygpuProgress("stop:enter");

    /* 2.0.223: final evidence snapshot before teardown — catches the
     * kext-unload path during the myintelfb=1 shutdown. */
    IOLog("MyIntelGPU: STOP(): irqTotal=%llu gt0=%llu gt1=%llu disp=%llu "
          "(stormPend=%u) aliveTick=%u\n",
          fIrqTotal, fIrqGt0, fIrqGt1, fIrqDisplay, fStormCount, fAliveTick);
    if (fFramebuffer) {
        fFramebuffer->dumpDiagnostics();
    }
    if (fDisplayFramebuffer) {
        fDisplayFramebuffer->dumpDiagnostics();
    }

    /*
     * Disable the engine interrupt source first — no new IRQ workloop
     * runs (and thus no kickCommandSet2 from processEngineInterrupt)
     * while we tear down rings below and free fEngineLock at the end.
     *
     * 2.0.227: remove + release the source here too (review CRITICAL:
     * it was only disabled, never removed from the workloop or released
     * — leaked the source AND left it attached across stop()).
     */
    if (fInterruptEventSource) {
        fInterruptEventSource->disable();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(fInterruptEventSource);
        }
        fInterruptEventSource->release();
        fInterruptEventSource = NULL;
        IODebug("stop(): engine interrupt source removed + released");
    }

    /*
 * Aperture Map
     * (IOMemoryMap::release() → unmap + release)
     */
    if (fApertureMap) {
        fApertureMap->release();
        fApertureMap = NULL;
    }
    if (fApertureDesc) {
        fApertureDesc->release();
        fApertureDesc = NULL;
    }
    fApertureVA = NULL;
    fApertureSize = 0;

    /*
 * MMIO Map
     */
    if (fMMIOMap) {
        fMMIOMap->release();
        fMMIOMap = NULL;
    }
    if (fMMIODesc) {
        fMMIODesc->release();
        fMMIODesc = NULL;
    }
    fRegs = NULL;

    /*
     * GSM Map (GGTT PTE array)
     */
    if (fGSMMap) {
        fGSMMap->release();
        fGSMMap = NULL;
    }
    if (fGSMDesc) {
        fGSMDesc->release();
        fGSMDesc = NULL;
    }
    fGsm = NULL;
    fGttTotal = 0;
    fStolenBase = 0;
    fStolenSize = 0;

    /*
     * MyIntelAccelerator (Phase 6c) — detach + release BEFORE framebuffer
     * and BAR teardown. Same discipline as fDisplayFramebuffer below: do
     * NOT call the child's stop() manually (IOKit drives it during
     * terminate/removeKext; a manual double-stop panics).
     */
    if (fAccelerator) {
        IOService *accelParent =
            OSDynamicCast(IOService, fAccelerator->getParentEntry(gIOServicePlane));
        if (accelParent) {
            fAccelerator->detachFromParent(accelParent, gIOServicePlane);
        }
        fAccelerator->release();
        fAccelerator = NULL;
        IODebug("stop(): accelerator detached+released");
    }

    /*
     * IntelFramebuffer (Interrupt Manager)
     */
    if (fFramebuffer) {
        fFramebuffer->disableInterrupts();
        fFramebuffer->release();
        fFramebuffer = NULL;
    }

    /*
     * MyIntelFramebuffer deferred-start timer (Phase 5d hang fix).
     * If stop() runs before the t+30s timer fires, cancel + release it —
     * otherwise framebufferStartTimerFired() would start() a freed
     * fDisplayFramebuffer (UAF) after stop() released it below.
     */
    if (fFramebufferStartTimer) {
        fFramebufferStartTimer->cancelTimeout();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(fFramebufferStartTimer);
        }
        fFramebufferStartTimer->release();
        fFramebufferStartTimer = NULL;
        IODebug("stop(): deferred FB start timer cancelled");
    }

    /*
     * 2.0.227: fDelayedDumpTimer (ALIVE ticker) — same UAF hazard as the
     * FB start timer: it only self-terminates at aliveTick>=60, so stop()
     * before that left a live recurring timer that would fire on the
     * released fFramebuffer below (review CRITICAL: resource leak).
     */
    if (fDelayedDumpTimer) {
        fDelayedDumpTimer->cancelTimeout();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(fDelayedDumpTimer);
        }
        fDelayedDumpTimer->release();
        fDelayedDumpTimer = NULL;
        IODebug("stop(): ALIVE dump timer cancelled");
    }

    /* myplane=1 self-test teardown: plane may be bound to our GEM buffer —
     * restore IONDRV scanout FIRST, then free the buffer. Reversing the
     * order would leave PLANE_SURF pointing at freed GGTT (black panel). */
    if (fPlaneTestBuf) {
        restorePlane(fPlaneTestBuf);
        gemBufferDestroy(fPlaneTestBuf, (uint32_t *)fGsm);
        fPlaneTestBuf = NULL;
        IODebug("stop(): plane test buffer restored + destroyed");
    }
    if (fAccelDeferTimer) {
        fAccelDeferTimer->cancelTimeout();
        if (fWorkLoop) fWorkLoop->removeEventSource(fAccelDeferTimer);
        fAccelDeferTimer->release();
        fAccelDeferTimer = NULL;
    }
    if (fPlaneTimer) {
        fPlaneTimer->cancelTimeout();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(fPlaneTimer);
        }
        fPlaneTimer->release();
        fPlaneTimer = NULL;
    }
    /* myflip teardown: whichever buffer is on-screen gets the plane restored;
     * BOTH buffers then destroyed. Timer cancelled before any of it. */
    if (fFlipTimer) {
        fFlipTimer->cancelTimeout();
        if (fWorkLoop) {
            fWorkLoop->removeEventSource(fFlipTimer);
        }
        fFlipTimer->release();
        fFlipTimer = NULL;
    }
    for (int i = 0; i < 2; i++) {
        if (fFlipBufs[i]) {
            if (fFlipBufs[i]->planeBound)
                restorePlane(fFlipBufs[i]);
            gemBufferDestroy(fFlipBufs[i], (uint32_t *)fGsm);
            fFlipBufs[i] = NULL;
        }
    }
    if (fBridgeTimer) {
        fBridgeTimer->cancelTimeout();
        if (fWorkLoop) fWorkLoop->removeEventSource(fBridgeTimer);
        fBridgeTimer->release();
        fBridgeTimer = NULL;
    }
    if (fBridgeBufB && fBridgeBufB->planeBound) {
        /* Only the SHOWN buffer needs restore; if B was bound, A is hidden.
         * restorePlane writes back whichever snapshot the bound one holds. */
        restorePlane(fBridgeBufB);
    }
    if (fBridgeBufB) {
        gemBufferDestroy(fBridgeBufB, (uint32_t *)fGsm);
        fBridgeBufB = NULL;
    }
    if (fBridgeBuf) {
        if (fBridgeBuf->planeBound)
            restorePlane(fBridgeBuf);
        gemBufferDestroy(fBridgeBuf, (uint32_t *)fGsm);
        fBridgeBuf = NULL;
    }

    /*
 * MyIntelFramebuffer (IOFramebuffer Binding)
     */
    if (fDisplayFramebuffer) {
        /*
         * NOTE: Do NOT call fDisplayFramebuffer->stop() manually.
         * IOKit calls stop() on child IOServices itself during
         * terminate/removeKext — calling it here caused a double-stop:
         * IOFramebuffer::stop() dereferenced a NULL ivar (offset 0x120)
         * → kernel panic (page fault) on macOS 13.7.
         * Only detach + release.
         */
        fDisplayFramebuffer->detachFromParent(fPCIDevice, gIOServicePlane);
        fDisplayFramebuffer->release();
        fDisplayFramebuffer = NULL;
    }

    /*
 * PCI Device (retain start)
     */
    if (fPCIDevice) {
        fPCIDevice->release();
        fPCIDevice = NULL;
    }

    /*
     * ─────────────────────────────────────
     *  Phase 5 Cleanup — Destroy Rings + GEM Buffers
     * ─────────────────────────────────────
     */
    if (fRingRCS) {
        ringDestroy(fRingRCS, fRingCallbacks);
        fRingRCS = NULL;
    }
    if (fRingBCS) {
        ringDestroy(fRingBCS, fRingCallbacks);
        fRingBCS = NULL;
    }

    /* Free GEM buffers for rings */
    if (fGemRingRCS) {
        gemBufferDestroy(fGemRingRCS, (uint32_t *)fGsm);
        fGemRingRCS = NULL;
    }
    if (fGemRingBCS) {
        gemBufferDestroy(fGemRingBCS, (uint32_t *)fGsm);
        fGemRingBCS = NULL;
    }

    /* Free ring callbacks struct */
    if (fRingCallbacks) {
        IOFree(fRingCallbacks, sizeof(MyIntelRingCallbacks));
        fRingCallbacks = NULL;
    }

    fAccelInitialized = false;

    /*
     * IRQ source is disabled above and accel is torn down — no kick path
     * can run anymore, so the engine lock is safe to free.  (free() also
     * NULL-checks defensively if start() never completed.)
     */
    if (fEngineLock) {
        IOLockFree(fEngineLock);
        fEngineLock = NULL;
    }

    /*
     * 2.0.227: release the lazy workloop (retained in getWorkLoop()).
     * All event sources are removed above, so nothing can re-enter it.
     * NULL it so a defensive free() does not double-release.
     */
    if (fWorkLoop) {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }

    /*
     * reset translation state
     */
    fTransCount = 0;
    fFakeGen = 0;

    /*
 * super (IOService stop)
     */
    super::stop(provider);

    IODebug("stop() — done");
}

#pragma mark -
#pragma mark - Gen11 GT Engine Interrupt Path (GPU-only mode)

/*
 * ─────────────────────────────────────────────────────────────────
 *  IOWorkLoop* MyIntelGPU::getWorkLoop(void)
 * ─────────────────────────────────────────────────────────────────
 */
IOWorkLoop *MyIntelGPU::getWorkLoop(void)
{
    /* 2.0.227: stop() released fWorkLoop — never lazily re-create one
     * during/after teardown (leak + timers on a dead workloop). */
    if (fStopping) {
        return NULL;
    }
    if (!fWorkLoop) {
        fWorkLoop = IOWorkLoop::workLoop();
        if (fWorkLoop) {
            fWorkLoop->retain();
        }
    }
    return fWorkLoop;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::armEngineInterrupts(void)
 *
 *  GPU-only mode (no -myintelfb): MyIntelGPU owns PCI interrupt
 *  vector index 0.  Display mode keeps IntelFramebuffer's own source
 *  (index 0) untouched — this path refuses to double-arm.
 *
 *  Boot-arg "-myintelgtirq" gates the HW enable (safety — default
 *  keeps the polling path active, no registers touched).
 *
 *  Enable chain = gen11_gt_irq_postinstall():
 *    RCS0/BCS_RSVD_INTR_MASK = ~smask  (0x190090 / 0x1900a0)
 *    RENDER_COPY_INTR_ENABLE = dmask   (0x190030)
 *    GFX_MSTR_IRQ            = MASTER  (0x190010)  ← last
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::armEngineInterrupts(void)
{
    bool gEnableGtInterrupts = false;
    PE_parse_boot_argn("myintelgtirq", &gEnableGtInterrupts, sizeof(gEnableGtInterrupts));

    /* 2.0.227: stop() gate — never arm during teardown (review CRITICAL:
     * stop() vs deferred arming race; setProperties can fire from a user
     * thread at any moment). */
    if (fStopping) {
        IODebug("armEngineInterrupts: SKIP (stop() in progress)");
        return;
    }

    /* Arm if explicitly requested via boot-arg OR if display framebuffer is present */
    if (!gEnableGtInterrupts && !fDisplayFramebuffer) {
        IODebug("armEngineInterrupts: SKIPPED (boot-arg -myintelgtirq not set and no display FB)");
        return;
    }

    /* 2.0.227: atomic claim instead of check-then-set — the old
     * `if (fInterruptsArmed) return;` + set-at-end left a window where
     * two callers (setProperties thread + workloop) both passed the check
     * and double-armed (double event source + double HW enable chain).
     * OSCompareAndSwap claims the flag BEFORE the enable chain runs.
     */
    if (!OSCompareAndSwap(false, true, &fInterruptsArmed)) {
        IODebug("armEngineInterrupts: already armed");
        return;
    }

    fInterruptEventSource = IOInterruptEventSource::interruptEventSource(
        this, &MyIntelGPU::sInterruptHandler, fPCIDevice, 0);
    if (!fInterruptEventSource) {
        IODebug("armEngineInterrupts: WARNING — no source for vector 0, trying 1");
        fInterruptEventSource = IOInterruptEventSource::interruptEventSource(
            this, &MyIntelGPU::sInterruptHandler, fPCIDevice, 1);
        if (!fInterruptEventSource) {
            IODebug("armEngineInterrupts: ERROR — no interrupt source (0 or 1)");
            fInterruptsArmed = false;
            return;
        }
    }

    IOWorkLoop *wl = getWorkLoop();
    if (!wl || wl->addEventSource(fInterruptEventSource) != kIOReturnSuccess) {
        IODebug("armEngineInterrupts: ERROR — addEventSource failed");
        fInterruptEventSource->release();
        fInterruptEventSource = NULL;
        fInterruptsArmed = false;
        return;
    }
    fInterruptEventSource->enable();

    /* gen11_gt_irq_postinstall() — masked regs first, master latch last */
    writeReg32(GEN11_RCS0_RSVD_INTR_MASK, ~GEN11_GT_SMASK);
    writeReg32(GEN11_BCS_RSVD_INTR_MASK, ~GEN11_GT_SMASK);
    writeReg32(GEN11_RENDER_COPY_INTR_ENABLE, GEN11_GT_DMASK);
    writeReg32(GEN11_GFX_MSTR_IRQ, GEN11_MASTER_IRQ);
    readReg32(GEN11_GFX_MSTR_IRQ); /* posting read */

    IODebug("armEngineInterrupts: Gen11 Unified Master interrupts ARMED (smask=0x%08X dmask=0x%08X)",
            GEN11_GT_SMASK, GEN11_GT_DMASK);

    /*
     * Set 1 was submitted during initHardwareAcceleration — before the
     * master IRQ latch went live — so its USER_INTERRUPT was dropped.
     * Kick set 2 now: with the latch enabled the completion IRQ must
     * reach processEngineInterrupt (first-kick → FIFO wakeup chain).
     */
    if (fRingRCS && gEnableGtInterrupts) {
        fRingRCS->workPending = true;   /* seed the one-shot set-2 round trip */
        kickCommandSet2();
    }
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::sInterruptHandler(OSObject*, IOInterruptEventSource*, int)
 *
 *  Mirrors i915 gen11_irq_handler():
 *    master_ctl = read(GFX_MSTR_IRQ); if (!master_ctl) return;
 *    write(GFX_MSTR_IRQ, master_ctl);          ← ack served master bits
 *    if (master_ctl & GEN11_GT_DW_IRQ(0)) → processEngineInterrupt(0)
 *    if (master_ctl & GEN11_GT_DW_IRQ(1)) → processEngineInterrupt(1)
 *    if (master_ctl & GEN11_DISPLAY_IRQ)  → fFramebuffer->handleInterrupt()
 *    write(GFX_MSTR_IRQ, GEN11_MASTER_IRQ);    ← re-enable master latch
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::sInterruptHandler(OSObject *target,
                                   IOInterruptEventSource *sender,
                                   int count)
{
    (void)sender;
    (void)count;

    MyIntelGPU *self = OSDynamicCast(MyIntelGPU, target);
    if (!self) {
        return;
    }

    uint32_t master_ctl = self->readReg32(GEN11_GFX_MSTR_IRQ);
    if (!master_ctl) {
        return;
    }

    /* ack: write back the served master bits before servicing */
    self->writeReg32(GEN11_GFX_MSTR_IRQ, master_ctl);
    self->readReg32(GEN11_GFX_MSTR_IRQ); /* posting read */

    self->fIrqTotal++;

    /* 2.0.229: mark IRQ-servicing path so forceWakeGet() and
     * ringSubmitExeclists() take the non-blocking fast path (i915
     * gen11_gt_irq_handler() never forcewakes — the engine that raised
     * USER IRQ is by definition awake). */
    self->fInIrqContext = true;

    if (self->fIrqTotal == 1) {
        /* 2.0.227: defer the NVRAM marker — mygpuProgress() does
         * IORegistryEntry::fromPath + setProperty (registry/NVRAM access)
         * which is unsafe from the IRQ-servicing path (review CRITICAL).
         * Flag it; the ALIVE ticker (workloop context) writes the marker.
         * IOLog is safe here and keeps the immediate visibility. */
        self->fFirstIrqSeen = true;
        IOLog("MyIntelGPU: [progress] irq:first-entry\n");
    }

    if (master_ctl & GEN11_GT_DW_IRQ(0)) {
        self->fIrqGt0++;
        self->processEngineInterrupt(0);
    }
    if (master_ctl & GEN11_GT_DW_IRQ(1)) {
        self->fIrqGt1++;
        self->processEngineInterrupt(1);
    }
    if (master_ctl & GEN11_DISPLAY_IRQ) {
        self->fIrqDisplay++;
        if (self->fFramebuffer) {
            self->fFramebuffer->handleInterrupt();
        }
    }

    /* re-enable the global master latch for the next interrupt */
    self->writeReg32(GEN11_GFX_MSTR_IRQ, GEN11_MASTER_IRQ);
    self->readReg32(GEN11_GFX_MSTR_IRQ); /* posting read */

    /*
     * ── 2.0.223 evidence harness: IRQ storm detection + summary logs ──
     * Storm window = one ALIVE-tick interval (5s, display mode only):
     * if >10k master IRQs arrive within a single tick that is a storm
     * (60Hz vblank is only ~300/tick). Mach timebase APIs are not
     * exported by MacKernelSDK, so we reuse the alive ticker counter.
     */
    if (self->fStormTick != self->fAliveTick) {
        if (self->fStormCount > 10000 && self->fDisplayFramebuffer) {
            IOLog("MyIntelGPU: ** IRQ STORM ** %u master IRQs within one "
                  "5s ALIVE tick (total=%llu gt0=%llu gt1=%llu disp=%llu)\n",
                  self->fStormCount, self->fIrqTotal, self->fIrqGt0,
                  self->fIrqGt1, self->fIrqDisplay);
        }
        self->fStormTick = self->fAliveTick;
        self->fStormCount = 0;
    }
    self->fStormCount++;

    if (self->fDisplayFramebuffer &&
        (self->fIrqTotal < 20 || (self->fIrqTotal % 100000) == 0)) {
        IOLog("MyIntelGPU: IRQ #%llu master=0x%08X (gt0=%llu gt1=%llu disp=%llu)\n",
              self->fIrqTotal, master_ctl, self->fIrqGt0, self->fIrqGt1,
              self->fIrqDisplay);
    }

    self->fInIrqContext = false;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::processEngineInterrupt(uint32_t bank)
 *
 *  Mirrors i915 gen11_gt_irq_handler() + gen11_gt_engine_identity():
 *    1. read GEN11_GT_INTR_DW(bank) — empty ⇒ return
 *    2. write GEN11_IIR_REG_SELECTOR(bank) = intr_dw
 *    3. poll GEN11_INTR_IDENTITY_REG(bank) until DATA_VALID
 *       (max ~100us — i915 udelay(1) per attempt; break early)
 *    4. decode class/instance/intr; RCS0 USER → kickCommandSet2()
 *    5. clear identity by writing the read value back (DATA_VALID)
 *    6. clear GT_INTR_DW(bank) by writing intr_dw back
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::processEngineInterrupt(uint32_t bank)
{
    uint32_t intr_dw = readReg32(GEN11_GT_INTR_DW(bank));
    if (!intr_dw) {
        return;
    }

    /*
     * 2.0.229 (ปฏิบัติการแหกตำรา): mirror i915 gen11_gt_bank_handler()
     * exactly — serve each set bit via its OWN IIR selector write.
     *
     * Old code wrote the whole intr_dw to GEN11_IIR_REG_SELECTOR once and
     * read a single identity → when TWO engines raised interrupts in the
     * same bank (e.g. RCS0 USER + BCS0), the selector pointed at one
     * identity, the other engine's bit was cleared by the final
     * GEN11_GT_INTR_DW write without ever being served → completion IRQ
     * silently dropped → ring work stuck.
     *
     * i915 gen11_gt_bank_handler():
     *   intr_dw = raw_reg_read(GT_INTR_DW);
     *   for_each_set_bit(bit, &intr_dw, 32) {
     *       ident = gen11_gt_engine_identity(bank, bit);  // selector=BIT(bit)
     *       gen11_gt_identity_handler(gt, ident);
     *   }
     *   raw_reg_write(GT_INTR_DW, intr_dw);  // clear AFTER served
     */
    for (uint32_t bit = 0; bit < 32; bit++) {
        if (!(intr_dw & (1u << bit))) {
            continue;
        }

        /* Select THIS bit's IIR entry (i915 writes BIT(bit), not the whole dw) */
        writeReg32(GEN11_IIR_REG_SELECTOR(bank), (1u << bit));

        for (int attempts = 0; attempts < 100; attempts++) {
            uint32_t ident = readReg32(GEN11_INTR_IDENTITY_REG(bank));
            if (ident & GEN11_INTR_DATA_VALID) {
                uint32_t engineClass = (ident & GEN11_INTR_ENGINE_CLASS_MASK) >> 16;
                uint32_t engineInst  = (ident & GEN11_INTR_ENGINE_INSTANCE_MASK) >> 20;
                uint32_t engineIntr  = ident & GEN11_INTR_ENGINE_INTR_MASK;

                IODebug("processEngineInterrupt: bank=%u bit=%u identity=0x%08X (class=%u inst=%u intr=0x%04X)",
                        bank, bit, ident, engineClass, engineInst, engineIntr);

                /* RCS0 = render class 0, instance 0 → USER = ring completion
                 * (set 1 or set 2 — both end in MI_USER_INTERRUPT) */
                if (engineClass == 0 && engineInst == 0 &&
                    (engineIntr & GT_RENDER_USER_INTERRUPT)) {
                    static bool sFirstRcsKick = true;
                    if (sFirstRcsKick) {
                        IODebug("processEngineInterrupt: FIRST RCS0 USER IRQ — kicking set 2");
                        sFirstRcsKick = false;
                    }
                    kickCommandSet2();
                }

                /* clear identity: write DATA_VALID back (i915 same) */
                writeReg32(GEN11_INTR_IDENTITY_REG(bank), ident);
                break;
            }
            IODelay(1); /* 1us per attempt — ~100us worst case, i915 udelay(1) */
        }
    }

    /* clear served bits at the bank level — AFTER all bits served
     * (i915: "Clear must be after shared has been served for engine") */
    writeReg32(GEN11_GT_INTR_DW(bank), intr_dw);
    readReg32(GEN11_GT_INTR_DW(bank)); /* posting read */
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::kickCommandSet2(void)
 *
 *  Asynchronous wakeup after a RCS0 USER completion IRQ.
 *  Appends set 2 (flush + NOOP + USER_INTERRUPT — same shape as
 *  set 1) and re-submits through the execlists LRC path
 *  (lrcUpdateRingRegs + EL_CTRL_LOAD kick).
 *
 *  Work gate: submits only when ring->workPending is set — every set
 *  ends with MI_USER_INTERRUPT, so an unconditional kick would re-arm
 *  itself forever (each completion IRQ → kick → new USER_INTERRUPT →
 *  next IRQ, ~1.2ms chain, CPU never idle). The gate makes the
 *  validation chain one-shot: armEngineInterrupts() seeds
 *  workPending=true for a single set-2 round trip, then the ring
 *  idles until the client queues more work.
 * ─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::kickCommandSet2(void)
{
    if (!fEngineLock) {
        IODebug("kickCommandSet2: SKIP (engine lock not allocated)");
        return;
    }
    /* Workloop-safety (2.0.225): the IRQ action runs under the IOKit
     * workloop gate, so it must never block on this mutex — a contended
     * lock here (start()'s synchronous seed kick) would stall the gate
     * and starve the deferred-FB + ALIVE timers → boot hang. Try-lock
     * and skip: the idle gate in kickRingLocked makes a skipped kick
     * benign (ring stays consistent; the next IRQ retries). */
    if (!IOLockTryLock(fEngineLock)) {
        /* 2.0.227: don't drop the kick silently — a skipped kick leaves
         * workPending=true with no pending IRQ to retry it (review
         * CRITICAL: TryLock skip drops work). Flag it; the ALIVE ticker
         * and the next submitClientTaskViaRing() retry it. */
        fKickPending = true;
        IODebug("kickCommandSet2: SKIP — engine lock busy (non-blocking, retry pending)");
        return;
    }
    kickRingLocked(fRingRCS);
    kickRingLocked(fRingBCS);   /* BCS: blitter tasks (taskType=kMyIntelTaskTypeBcsBlit) */
    IOLockUnlock(fEngineLock);
    fKickPending = false;   /* 2.0.227: kick delivered — clear retry flag */
}

void MyIntelGPU::kickRingLocked(MyIntelRing *ring)
{
    if (!fAccelInitialized || !ring || !fRingCallbacks) {
        IODebug("kickCommandSet2: SKIP (accel=%d ring=%p cb=%p)",
                fAccelInitialized, ring, fRingCallbacks);
        return;
    }
    if (!ringIsInitialized(ring)) {
        IODebug("kickCommandSet2: SKIP (ring not initialized)");
        return;
    }
    if (!ring->workPending) {
        /* Idle gate: no queued work → do not append/submit. Without it
         * the chain never terminates: set N's USER_INTERRUPT → completion
         * IRQ → kick → set N+1 ... forever (~1.2ms, CPU never idle). */
        IODebug("kickCommandSet2: idle — no pending work, SKIP (chain stops)");
        return;
    }

    IODebug("kickCommandSet2: pre head=%u tail=%u space=%u (appending set 2)",
            ring->head, ring->tail, ring->space);

    if (!ringEmitFlushDW(ring, true, true)) {
        /* Storm guard: never re-submit a stuck tail. If the ring is full
         * (space drained to 0, head not advancing) the flush cannot be
         * emitted — bailing here breaks the USER_INTERRUPT -> IRQ -> kick
         * loop that otherwise spins forever at tail=16320 with the CPU
         * pegged (dmesg: "flush emit failed" every ~1.2ms). */
        IODebug("kickCommandSet2: flush emit failed — SKIP submit "
                "(head=%u tail=%u space=%u)",
                ring->head, ring->tail, ring->space);
        return;
    }
    if (!ringEmitNOOP(ring)) {
        IODebug("kickCommandSet2: NOOP emit failed — SKIP submit (space=%u)",
                ring->space);
        return;
    }
    /* Drain the pending batch queue in FIFO order: one BB_START per entry,
     * emitted BEFORE USER_INTERRUPT so the IRQ fires only after the last
     * batch has run and returned (MI_BATCH_BUFFER_END → ring at
     * USER_INT) — completion signal, i915 breadcrumb-after-bb_start parity.
     * Each queued batch is a separate BB_START; the ring executes them
     * sequentially (batch N's END returns to ring at BB_START N+1). */
    uint32_t queueIdx = ring->pendingHead;
    while (ring->pendingCount > 0) {
        MyIntelRing::PendingBatch *pb = &ring->pendingQueue[queueIdx];
        if (pb->ggtt == 0) {
            IODebug("kickCommandSet2: SKIP — queue slot %u empty (count=%u)",
                    queueIdx, ring->pendingCount);
            return;
        }
        IODebug("kickCommandSet2: BB_START → batch ggtt=0x%X taskType=%u packet=0x%llX",
                pb->ggtt, pb->taskType, (unsigned long long)pb->packetData);
        if (!ringEmitBatchStart(ring, pb->ggtt)) {
            IODebug("kickCommandSet2: BB_START emit failed — SKIP submit (space=%u)",
                    ring->space);
            return;
        }
        pb->ggtt = 0;
        pb->taskType = 0;
        pb->packetData = 0;
        queueIdx = (queueIdx + 1) % RING_PENDING_MAX;
        ring->pendingCount--;
    }
    ring->pendingHead = queueIdx;
    if (!ringEmitUserInterrupt(ring)) {
        IODebug("kickCommandSet2: USER_INTERRUPT emit failed — SKIP submit (space=%u)",
                ring->space);
        return;
    }

    ringSubmit(ring, fRingCallbacks);
    ring->workPending = false;   /* work handed to HW; idle until next queue */
    IODebug("kickCommandSet2: submitted (head=%u tail=%u) — ELSP kicked",
            ring->head, ring->tail);
}

kern_return_t MyIntelGPU::submitClientTaskViaRing(void *batchBuffer,
                                                  uint32_t taskType,
                                                  uint64_t packetData)
{
    if (!fAccelInitialized || !fRingCallbacks) {
        IODebug("submitClientTaskViaRing: SKIP (accel=%d cb=%p)",
                fAccelInitialized, fRingCallbacks);
        return kIOReturnNotReady;
    }
    /* BCS blit tasks route to the blitter ring; everything else (WriteMagic,
     * MiMath, PipeControl) runs on RCS. */
    MyIntelRing *ring = (taskType == kMyIntelTaskTypeBcsBlit) ? fRingBCS : fRingRCS;
    if (!ring || !ringIsInitialized(ring)) {
        IODebug("submitClientTaskViaRing: SKIP (ring not initialized, taskType=%u)", taskType);
        return kIOReturnNotReady;
    }
    if (!fEngineLock) {
        IODebug("submitClientTaskViaRing: SKIP (engine lock not allocated)");
        return kIOReturnNotReady;
    }
    if (taskType >= kMyIntelTaskTypeCount) {
        IODebug("submitClientTaskViaRing: SKIP (invalid taskType=%u)", taskType);
        return kIOReturnBadArgument;
    }

    /* The batchBuffer is a client-verified MyIntelGEMBuffer* (registry
     * pointer-compare done client-side) — re-check magic before deref. */
    MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)batchBuffer;
    if (buf == NULL || buf->magic != GEM_BUFFER_MAGIC) {
        IODebug("submitClientTaskViaRing: SKIP (invalid batch buffer)");
        return kIOReturnBadArgument;
    }
    if (buf->pages == 0 || buf->pagesPhys == NULL) {
        IODebug("submitClientTaskViaRing: SKIP (batch has no physical backing)");
        return kIOReturnNotReady;
    }

    /* Map the batch pages into the PPGTT identity window BEFORE the kick,
     * then append to the pending queue. The next kick drains the whole
     * queue in one submission (multi-batch F8b). */
    IOLockLock(fEngineLock);
    if (ring->pendingCount >= RING_PENDING_MAX) {
        IOLockUnlock(fEngineLock);
        IODebug("submitClientTaskViaRing: queue FULL (%u) — client must retry "
                "after completion IRQ (ggtt=0x%X)", RING_PENDING_MAX, buf->ggttOffset);
        return kIOReturnBusy;
    }
    if (!lrcMapBatchPages(ring, buf->ggttOffset, buf->pagesPhys, buf->pages)) {
        IOLockUnlock(fEngineLock);
        IODebug("submitClientTaskViaRing: lrcMapBatchPages FAILED (ggtt=0x%X pages=%u)",
                buf->ggttOffset, buf->pages);
        return kIOReturnError;
    }
    uint32_t slot = (ring->pendingHead + ring->pendingCount) % RING_PENDING_MAX;
    ring->pendingQueue[slot].ggtt       = buf->ggttOffset;
    ring->pendingQueue[slot].taskType   = taskType;
    ring->pendingQueue[slot].packetData = packetData;
    ring->pendingCount++;
    ring->workPending = true;
    IOLockUnlock(fEngineLock);

    kickCommandSet2();
    /* 2.0.227: this thread just released fEngineLock — if an earlier kick
     * was skipped under contention, deliver it now (blocking lock is safe
     * here: client thread, not the workloop gate). */
    if (fKickPending) {
        IODebug("submitClientTaskViaRing: retrying pending kick");
        kickCommandSet2();
    }
    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark - safeInitInterrupts

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool MyIntelGPU::safeInitInterrupts(void)
 *
 * Interrupt System
 *
 * initInterrupts() start() :
 * 1. IODelay(2000) = 2ms HW Register
 * 2. MMIO framebuffer
 * 3. (re-entrant) — init → return true
 *
 * Kernel State
 * setProperties(), handleMessage(), IOFramebuffer::start()
 *
 *  Reference:
 * Linux i915: intel_irq_install() display init
 * i915_probe() — race condition
 *─────────────────────────────────────────────────────────────────
 */
bool MyIntelGPU::safeInitInterrupts(void)
{
    IODebug("safeInitInterrupts: START");

    /* 2.0.227: stop() gate — never touch HW / framebuffer mid-teardown */
    if (fStopping) {
        IODebug("safeInitInterrupts: SKIP (stop() in progress)");
        return false;
    }

    /* Ensure unified master interrupt handler is armed */
    if (!fInterruptsArmed) {
        armEngineInterrupts();
    }

    /*
     * Re-entrant guard — init
     */
    if (fFramebuffer && fFramebuffer->isInterruptsReady()) {
        IODebug("safeInitInterrupts: already initialized");
        return true;
    }

    if (!fFramebuffer) {
        /* 2.0.227: no second armEngineInterrupts() call here — the call
         * above already ran the enable chain (or skipped it via boot-arg
         * gate). A redundant call was the 4065+4078 double-arm. */
        IODebug("safeInitInterrupts: GPU-only mode (no IntelFramebuffer) — engine interrupts");
        return fInterruptsArmed;
    }

    if (!fRegs) {
        IODebug("safeInitInterrupts: ERROR — MMIO not mapped");
        return false;
    }

    if (!isValidRegs()) {
        IODebug("safeInitInterrupts: ERROR — MMIO regs VA invalid (fRegs=%p, no real BAR0?)", fRegs);
        return false;
    }

    /*
 * 2000 microseconds (2 )
 * Hardware Register GPU
 * Kernel
     *
 * IODelay() busy-wait → yield CPU
 * delay (2ms)
     */
    IODebug("safeInitInterrupts: Delaying 2000us...");
    IODelay(2000);

    /*
 * initInterrupts()
     */
    IODebug("safeInitInterrupts: Calling initInterrupts...");
    bool ok = fFramebuffer->initInterrupts(this);

    if (ok) {
        IODebug("safeInitInterrupts: Enabling Vblank interrupt on Pipe A...");
        fFramebuffer->enableVblankInterrupt(0);
    }

    IODebug("safeInitInterrupts: %s", ok ? "OK" : "FAILED");
    return ok;
}

#pragma mark -
#pragma mark - setProperties (Lazy Interrupt Trigger)

/*
 * ─────────────────────────────────────────────────────────────────
 *  IOReturn MyIntelGPU::setProperties(OSObject *properties)
 *
 * Property Changes IORegistry
 *
 * Lazy Trigger safeInitInterrupts():
 * - IOFramebuffer User Space set property
 * → Interrupt System
 * - init interrupt start()
 *
 *  Reference:
 *    IORegistryEntry::setProperties()
 * IOFramebuffer (binding)
 *─────────────────────────────────────────────────────────────────
 */
IOReturn MyIntelGPU::setProperties(OSObject *properties)
{
    IODebug("setProperties: called");

    /*
 * Lazy init interrupts — → init
     *
 * trigger interrupt
 * IOFramebuffer (
 * start() kext )
     */
    if (!fFramebuffer || !fFramebuffer->isInterruptsReady()) {
        IODebug("setProperties: Triggering safeInitInterrupts");
        safeInitInterrupts();
    }

    /*
 * super class IOKit property
     */
    return super::setProperties(properties);
}

#pragma mark -
#pragma mark - notifyVblank

/*
 * ─────────────────────────────────────────────────────────────────
 *  void MyIntelGPU::notifyVblank()
 *
 * Vblank Event MyIntelFramebuffer
 * — IntelFramebuffer::handleInterrupt()
 * Vblank interrupt (Phase 3)
 *
 *  Flow:
 *    HW Interrupt → IntelFramebuffer::handleInterrupt()
 *    → MyIntelGPU::notifyVblank()
 *    → MyIntelFramebuffer::handleVblank()
 * → VblankEvent() → VSync user space
 *─────────────────────────────────────────────────────────────────
 */
void MyIntelGPU::notifyVblank(void)
{
    if (fDisplayFramebuffer) {
        fDisplayFramebuffer->handleVblank();
    }
}

#pragma mark -
#pragma mark - Static IOKit Metadata

/*
 * ─────────────────────────────────────────────────────────────────
 *  IOKit MetaClass Registration
 *
 * Apple's IOKit runtime instance
 * factory method
 *
 * OSDefineMetaClassAndStructors
 *
 * IOKit Matching Dictionary:
 * IOPCIDevice match Info.plist
 * IONameMatch IOPCIMatch key
 *
 *  Example Info.plist snippet:
 *
 *    <key>IOPCIMatch</key>
 *    <string>0x8086 0x4691</string>    // Alder Lake-P GT2
 *
 * match ID:
 *    <string>0x8086 0x4680 0x8086 0x468b 0x8086 0x4691 0x8086 0x4692</string>
 * ─────────────────────────────────────────────────────────────────
 */

/*
 * (extern "C") IOKit library probe
 * custom IOService matching:
 *
 *   static MyIntelGPU *ourInstance = NULL;
 *
 *   MyIntelGPU *MyIntelGPU::probe(IOService *provider, SInt32 *score) {
 *       ...
 *   }
 *
 * standard IOPCIDevice match Info.plist
 * override probe()
 */
