/*===========================================================================
 *  MyIntelGPU.hpp
 *  Hackintosh Kext — FakeID Alder Lake → Coffee Lake
 *
 * Intel Graphics Driver macOS (IOKit C++)
 * :
 * - PCI BAR0 (MMIO Registers) + BAR2 (Aperture/GMADR)
 * - Dynamic Register Offset Translation: MMIO base engine
 * cursor register macOS ( Coffee Lake)
 * Alder Lake
 * - CPU memory barrier + cache flush GPU coherency
 *
 * Linux i915:
 *    drivers/gpu/drm/i915/i915_pci.c        — PCI ID table + per-gen info
 *    drivers/gpu/drm/i915/intel_device_info.c — GMD_ID runtime detection
 *    drivers/gpu/drm/i915/gt/intel_engine_cs.c — engine MMIO base per gen
 *    drivers/gpu/drm/i915/i915_drv.c        — i915_driver_mmio_probe / hw_probe
 *///=========================================================================

#ifndef __MY_INTEL_GPU_HPP__
#define __MY_INTEL_GPU_HPP__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

/* Phase 5 — GEM Buffer + Ring Buffer includes */
#include "MyIntelGEMBuffer.hpp"

/* 2.0.224: early-boot progress marker. Written to NVRAM + IOLog at every
 * startup phase so a hard-locked boot (myintelfb=1, hangs before logd)
 * can be diagnosed after a forced power-off: reboot safe, then run
 * `nvram myintelgpu-progress` to see the last phase reached. */
extern void mygpuProgress(const char *tag);

/* Forward declaration for ring struct — full definition in MyIntelRing.hpp */
struct MyIntelRing;
struct MyIntelRingCallbacks;

#pragma mark - Constants & Register Offsets

/*
 * ─────────────────────────────────────────────
 * PCI Device IDs —
 * ─────────────────────────────────────────────
 * Coffee Lake ( macOS )
 *    Device IDs: 0x3E91 (U14 GT2), 0x3E92 (U15 GT2),
 *                0x3E9B (U23 GT2), 0x9BC4 (S GT3)
 *
 * Alder Lake ()
 *    Device IDs: 0x4680 (ADL-S GT1), 0x468B (ADL-P GT1),
 *                0x4691 (ADL-P GT2), 0x4692 (ADL-S GT2)
 * ─────────────────────────────────────────────
 */

#define ADL_P_GT2_DEVICE_ID 0x4691 /* Alder Lake-P GT2 () */
#define CFL_GT2_DEVICE_ID 0x3E92 /* Coffee Lake-U15 GT2 () */

/*
 * Client task types — submitClientTaskViaRing() contract (selector 3,
 * scalarInput[1]; packetData = scalarInput[2]). Identifies the staged
 * batch so the ring can log/route pending work (future engines: media,
 * blitter). kMyIntelTaskTypeWriteMagic = proof batch (SDI magic + LRI
 * CS_GPR0 + SRM readback); packetData = u64 magic, 0 = default.
 */
enum {
    kMyIntelTaskTypeWriteMagic = 0,   /* SDI magic + LRI CS_GPR0 + SRM readback (RCS) */
    kMyIntelTaskTypeBcsBlit,          /* BCS FAST_COPY page copy (routes to fRingBCS) */
    kMyIntelTaskTypeMiMath,           /* RCS MI_MATH(4) GPR ALU proof */
    kMyIntelTaskTypePipeControl,      /* RCS PIPE_CONTROL flush proof */
    kMyIntelTaskTypeBreadcrumb,       /* RCS Hardware Breadcrumb Seqno write */
    kMyIntelTaskTypeCount
};

/*
 * MMIO Register Address Engine Base
 * ──────────────────────────────────────────
 * Coffee Lake (GEN9) Alder Lake (GEN12+)
 * RCS0 = 0x02000 RCS0 = 0x02200 ← !
 * VCS0 = 0x12000 VCS0 = 0x1c0000 ← !
 * BCS0 = 0x22000 BCS0 = 0x22000 ()
 * VECS0 = 0x1a000 VECS0 = 0x1c8000 ← !
 * VCS1 = N/A () VCS1 = 0x1c4000
 *    VCS2 = N/A                       VCS2 = 0x1d0000
 *    CCS0 = N/A                       CCS0 = 0x1a000
 *
 * : intel_engine_cs.c __engine_mmio_base()
 *           i915_pci.c  ENGINE_*_MMIO_BASE macros
 * ──────────────────────────────────────────
 */
#define RCS0_BASE_REAL 0x02000 /* Render — RENDER_RING_BASE=0x2000 on ALL gens incl Gen12 (i915_reg.h; mmio_bases entry graphics_ver=1). Was wrongly 0x2200 (DKL PHY reg, not an engine). */
#define RCS0_BASE_FAKE      0x02000

#define VCS0_BASE_REAL 0x1C0000 /* Video Decode Gen12+ () */
#define VCS0_BASE_FAKE 0x12000 /* Coffee Lake () */

#define BCS0_BASE_REAL 0x22000 /* Blitter — */
#define BCS0_BASE_FAKE      0x22000

#define VECS0_BASE_REAL 0x1C8000 /* Video Encode Gen12+ () */
#define VECS0_BASE_FAKE 0x1A000 /* Coffee Lake () */

/*
 * Cursor Register Base —
 * ──────────────────────────────────────────
 *  Coffee Lake:                       Alder Lake:
 * Pipe A CURBASE = 0x70080 Pipe A CURBASE = 0x70080 ()
 * Pipe B CURBASE = 0x700c0 Pipe B CURBASE = 0x71080 ← !
 * Pipe C CURBASE = 0x72080 Pipe C CURBASE = 0x72080 ()
 * Pipe D CURBASE = 0x73080 ()
 * ──────────────────────────────────────────
 */
#define CURSOR_A_FAKE       0x70080
#define CURSOR_A_REAL       0x70080

#define CURSOR_B_FAKE       0x700C0
#define CURSOR_B_REAL 0x71080 /* ! */

#define CURSOR_C_FAKE       0x72080
#define CURSOR_C_REAL       0x72080

#define CURSOR_D_FAKE 0x73080 /* Coffee Lake */
#define CURSOR_D_REAL       0x73080

/*
 * ─────────────────────────────────────────────
 *  Display Register Bases — CFL (Gen9) vs RPL (Gen12.2)
 * ─────────────────────────────────────────────
 *  Pipe/Plane registers:
 *    Pipe A: 0x70000 (same on both)
 *    Pipe B: 0x71000 (same)
 *    Pipe C: 0x72000 (same)
 *
 *  Transcoder registers:
 *    TRANS_A: 0x60000 (same)
 *    TRANS_B: 0x61000 (same)
 *    TRANS_C: 0x62000 (same)
 *
 *  DDI Buffer registers:
 *    DDI A: 0x64000 (same)
 *    DDI B: 0x64100 (same)
 *
 * PCH (South Display) — !
 *    CFL:  PCH registers at 0xCxxx  (I/O space)
 *    RPL:  PCH registers at 0xCxxxx (MMIO space, different base)
 *
 *  Reference: intel_display_regs.h, i915_reg.h
 * ─────────────────────────────────────────────
 */
#define TRANSCODER_A_BASE    0x60000
#define TRANSCODER_B_BASE    0x61000
#define TRANSCODER_C_BASE    0x62000

#define PIPE_A_BASE          0x70000
#define PIPE_B_BASE          0x71000
#define PIPE_C_BASE          0x72000

/* Plane registers (per pipe) */
#define PLANE_A_BASE         0x70100   /* Pipe A primary plane */
#define PLANE_B_BASE         0x71100   /* Pipe B primary plane */
#define PLANE_C_BASE         0x72100   /* Pipe C primary plane */

/* Plane register offsets (SKL+ universal plane layout, relative to
 * PLANE_A_BASE).  VERIFIED on this RPL via gem_test ReadMMIO:
 *   PLANE_CTL_1A 0x70180, PLANE_STRIDE_1A 0x70188, PLANE_SIZE_1A 0x70190,
 *   PLANE_SURF_1A 0x7019C, PLANE_SURFLIVE 0x701AC.
 * Reference: i915 intel_display_reg_defs.h skl_plane_regs. */
#define PLANE_CTL_OFFSET      0x80     /* PLANE_CTL (+0x80) */
#define PLANE_STRIDE_OFFSET   0x88     /* PLANE_STRIDE (+0x88) */
#define PLANE_SIZE_OFFSET     0x90     /* PLANE_SIZE (+0x90) */
#define PLANE_SURF_OFFSET     0x9C     /* PLANE_SURF (+0x9C) — GGTT scanout */
#define PLANE_SURFLIVE_OFFSET 0xAC     /* PLANE_SURFLIVE (+0xAC) — hw latched */

/* PLANE_CTL bits (intel_display_reg_defs.h) */
#define PLANE_CTL_ENABLE      (1U << 31)  /* _PLANE_CTL_ENABLE */

/* DDI (Digital Display Interface) */
#define DDI_A_BASE           0x64000
#define DDI_B_BASE           0x64100
#define DDI_C_BASE           0x64200

/*
 * eDP Panel Power Sequencing — TGL/ADL/RPL (ICL+):
 *   intel_pps_regs.h: PPS_BASE 0x61200, _PP_STATUS 0x61200,
 *   _PP_CONTROL 0x61204, _PP_ON_DELAYS 0x61208, _PP_OFF_DELAYS 0x6120C,
 *   _PP_DIVISOR 0x61210.  (Old CFL PCH offsets 0xC50/0xC54/0xC58/0xC60/0xC64
 *   are wrong for Gen12 — reads return 0.)
 */
#define PP_STATUS            0x61200
#define PP_CONTROL           0x61204
#define PP_ON_DELAYS         0x61208
#define PP_OFF_DELAYS        0x6120C
#define PP_DIVISOR           0x61210

/* Backlight — !
 *  CFL:  0x48250 (PCH backlight)
 *  RPL:  0xC8250 (different offset in display MMIO)
 */
#define CFL_BLC_PWM_CTL      0x48250   /* Coffee Lake backlight */
#define RPL_BLC_PWM_CTL      0xC8250   /* Raptor Lake backlight */

/*
 * CDCLK Control —
 *  CFL:  CDCLK_CTL = 0x130000
 *  TGL:  CDCLK_CTL = 0x46000  (intel_display_regs.h:2769)
 */
#define CDCLK_CTL            0x46000
#define CDCLK_FREQ_SEL_MASK  0x0C000000   /* TGL: REG_GENMASK(27,26) */

/* DPLL — completely different architecture */
/* CFL: DPLL_CRTL1, LCPLL1_CTL */
/* TGL: ICL+ combo PLL regs (intel_display_regs.h) */
#define DPLL0_CFGCR0         0x164284   /* TGL: _TGL_DPLL0_CFGCR0 */
#define DPLL0_CFGCR1         0x164288   /* TGL: _TGL_DPLL0_CFGCR1 */
#define DPLL0_ENABLE         0x46010    /* TGL: ICL_DPLL_ENABLE(0) = _DPLL0_ENABLE */

/*
 * PCH (South Display) translation
 * CFL PCH display registers 0x48000-0x48FFF
 * RPL PCH display registers 0xC8000-0xC8FFF
 */
#define PCH_DISPLAY_BASE_FAKE  0x48000
#define PCH_DISPLAY_BASE_REAL  0xC8000
#define PCH_DISPLAY_WINDOW     0x1000

/*
 * Kernel Virtual Address Boundary
 *
 * x86_64 macOS: kernel virtual addresses 0xffffff8000000000
 * ( fRegs (BAR0 mapping) kernel space
 * 0x2000 VMware virtual GPU BAR0)
 */
#define kMinKernelVA            0xFFFFFF80000000ULL
#define kMaxKernelVA            0xFFFFFFFFFFFFFFFFULL

/*
 * Register Offsets
 */
#define GMD_ID_GRAPHICS     0xD8C      /* Graphics Media Device ID register
 ( Gen12+ / Meteor Lake)
                                          read32(0xD8C) → ver/release */
#define GMD_ID_DISPLAY      0x510A0    /* Display IP version (MTL+)
 verify display gen */

#ifndef FB_MAX_CRTC
#define FB_MAX_CRTC 3
#endif

#define GEN11_GT_INTR_DW0   0x44074    /* GT Interrupt DW0 (shared) */
#define ENGINE_TAIL_REG      0x80      /* Ring Tail Register offset
 ( engine base) */
#define ENGINE_HEAD_REG      0x34      /* Ring Head Register offset */
#define ENGINE_CTL_REG       0x3C      /* Ring Control Register offset */
#define ENGINE_START_REG     0x38      /* Ring Start (Base Address) */

#define GFX_FLSH_CNTL_GEN6  0x101008  /* GGTT TLB Invalidate Register
                                           VERIFIED i915 gt/intel_gt_regs.h:1475
                                           = 0x101008 (NOT 0x10100 — the old
                                           value dropped the trailing 8) */
#define GFX_FLSH_CNTL_EN    (1U << 0)  /* i915 gt/intel_gt_regs.h:1476 —
                                           write 1 to trigger the invalidate;
                                           writing 0 is a NO-OP */

/*
 * GGTT Hardware Init Registers (Gen8+)
 *
 * Reference: Linux i915 — drivers/gpu/drm/i915/gt/intel_ggtt.c
 *   gen8_gmch_probe() / gen8_get_total_gtt_size() / ggtt_probe_common()
 *
 * SNB_GMCH_CTRL (PCI config 0x50, 16-bit):
 *   bits [7:6] (BDW_GMCH_GGMS, 2-bit) = GTT PTE storage size:
 *      0 = invalid, 1..3 → size = (1 << ggms) MB (EXPONENTIAL)
 *   e.g. ggms=3 → 8MB PTE array → 8MB/8 = 1M PTE entries
 *
 *   ⚠️ MASK IS 0x3, NOT 0x7 — bit 8 is the LSB of the GMS (stolen
 *   memory) field (BDW_GMCH_GMS_SHIFT=8). Using 0x7 reads that bit,
 *   so an odd GMS value doubles/corrupts the reported GTT size.
 *   Matches Linux: include/drm/intel/i915_drm.h BDW_GMCH_GGMS_MASK 0x3.
 *
 * GSM base (GTT Stolen Memory page table array):
 *   Gen8+: GTTMMADR (BAR0) is 16MB; GTTADR starts at 8MB offset
 *   phys_gsm = pci_resource_start(GTTMMADR_BAR) + gen6_gttadr_offset()
 *            = BAR0 phys + (16MB / 2) = BAR0 phys + 0x800000
 *
 * Each PTE = 64-bit (8 bytes) on Gen8+.
 * fGttTotal (entries) = gtt_size_bytes / sizeof(gen8_pte_t).
 */
#define SNB_GMCH_CTRL           0x50       /* PCI config: GMCH Graphics Control (16-bit) */
#define BDW_GMCH_GGMS_SHIFT     6          /* GGMS field shift */
#define BDW_GMCH_GGMS_MASK      0x3        /* GGMS field mask (2 bits — per i915, NOT 0x7) */
#define BDW_GMCH_GMS_SHIFT      8          /* GMS (stolen) field shift — must not be read by GGMS */
#define BDW_GMCH_GMS_MASK       0xFF       /* GMS field mask (8 bits, bits [15:8]) — i915 GMS_MASK REG_GENMASK(15,8); gvt BDW_GMCH_GMS_MASK 0xff */
#define GEN6_DSMBASE            0x1080C0   /* Stolen DRAM base (Gen8+; i915 GEN6_DSMBASE) */
#define GEN11_BDSM_MASK         0xFFFFFFFFFFF00000ULL /* bits [63:20] — i915 GEN11_BDSM_MASK */
#define GEN8_GTTADR_OFFSET      0x800000   /* GTT page table offset inside 16MB GTTMMADR */
#define GEN8_GTTADR_BAR0_SIZE   0x1000000  /* Gen8+ GTTMMADR (BAR0) total size = 16MB */
#define GGTT_PTE_SIZE           8          /* gen8_pte_t = 8 bytes */

/*
 * Phase 6 — Power Management Registers (Xe / Raptor Lake)
 *
 *  Registers extracted from xe_gt_regs.h, xe_force_wake.c, xe_gt_idle.c
 *  for Gen12.2 (Raptor Lake) GPU power management.
 *
 *  Three layers:
 *    1. Force Wake — wake GT domain before register access during RC6
 *    2. RC6 (Render Standby) — deep GPU idle state for power saving
 *    3. Display Panel Power — eDP VDD/BL enable sequencing
 */

/* Force Wake — must be accessed before any GT register when in RC6 */
#define FORCEWAKE_GT            0xA188   /* Request: write to wake GT domain (Gen9+: FORCEWAKE_GT_GEN9) */
#define FORCEWAKE_ACK_GT        0x130044 /* Acknowledge: read to confirm wake (FORCEWAKE_ACK_GT_GEN9) */
#define FORCEWAKE_RENDER        0xA278   /* Request: RENDER domain — guards RCS ring regs (Gen9+: FORCEWAKE_RENDER_GEN9) */
#define FORCEWAKE_ACK_RENDER    0x0D84   /* Acknowledge: RENDER domain (FORCEWAKE_ACK_RENDER_GEN9) */
#define FORCEWAKE_MEDIA         0xA270   /* Gen9/10 ONLY — NO single MEDIA domain on Gen11+ (per-VDBOX/VEBOX instead); DO NOT USE here */
#define FORCEWAKE_ACK_MEDIA     0x0D88   /* Acknowledge: MEDIA domain (FORCEWAKE_ACK_MEDIA_GEN9) */

/* Force Wake domain bits (multicast) */
#define FW_BIT                  (1U << 0)   /* Bit 0 = request */
#define FW_MASK                 (1U << 16)  /* Bit 16 = mask */

/* Force Wake timeout */
#define FW_ACK_TIMEOUT_US       50000   /* 50ms max wait for ACK */
#define FW_RETRY_MAX            5       /* 2.0.215: forcewake hard-retry rounds before failing */

/* RC6 (Render Standby) — GPU deep idle */
#define RC_CONTROL              0xA090   /* RC6 enable/control */
#define RC_CTL_HW_ENABLE        (1U << 31)  /* HW-managed RC6 */
#define RC_CTL_TO_MODE          (1U << 28)  /* Target-override mode */
#define RC_CTL_RC6_ENABLE       (1U << 18)  /* Enable RC6 state */
#define RC_STATE                0xA094   /* Current RC state (read-only) */
#define RC_IDLE_HYSTERSIS       0xA0AC   /* Idle hysteresis timer */
#define RC_IDLE_HYST_VALUE      0x3B9ACA  /* ~5 seconds (units of 1280ns) */

/* GT Core Status */
#define GT_CORE_STATUS_REG      0x138060  /* GT core C-state status */
#define GT_C0                   0         /* Active */
#define GT_C6                   3         /* Deepest idle */

/* GPU Reset */
#define GDRST_REG               0x941C   /* Graphics Domain Reset */
#define GDRST_GRDOM_FULL        (1U << 0)  /* Full GPU reset */

/*
 * GuC ownership / WOPCM lock probe — VERIFIED against i915 master
 * drivers/gpu/drm/i915/gt/uc/intel_guc_reg.h (2026):
 *   GUC_STATUS          0xC000
 *     bit0    GS_MIA_IN_RESET
 *     bits1-7 GS_BOOTROM_MASK   (7 bits, BOOTROM_SHIFT=1)
 *     bits8-15 GS_UKERNEL_MASK  (8 bits, UKERNEL_SHIFT=8)
 *     bits16-18 GS_MIA_MASK     (MIA_SHIFT=16)
 *     bits30-31 GS_AUTH_STATUS_MASK (0=?, 1=BAD, 2=GOOD)
 *   GUC_WOPCM_SIZE      0xC050   bit0 = GUC_WOPCM_SIZE_LOCKED
 *   DMA_GUC_WOPCM_OFFSET 0xC340  bit0 = GUC_WOPCM_OFFSET_VALID
 * If GuC is running + WOPCM locked, GuC owns the engines and
 * execlist submission through ELSP will be ignored by HW.
 */
#define GUC_STATUS_REG              0xC000
#define   GS_MIA_IN_RESET           (1U << 0)
#define   GS_BOOTROM_SHIFT          1
#define   GS_BOOTROM_MASK           (0x7FU << GS_BOOTROM_SHIFT)
#define   GS_UKERNEL_SHIFT          8
#define   GS_UKERNEL_MASK           (0xFFU << GS_UKERNEL_SHIFT)
#define   GS_MIA_SHIFT              16
#define   GS_MIA_MASK               (0x07U << GS_MIA_SHIFT)
#define   GS_AUTH_STATUS_SHIFT      30
#define   GS_AUTH_STATUS_MASK       (0x03U << GS_AUTH_STATUS_SHIFT)
#define GUC_WOPCM_SIZE_REG          0xC050
#define   GUC_WOPCM_SIZE_LOCKED     (1U << 0)
#define DMA_GUC_WOPCM_OFFSET_REG    0xC340
#define   GUC_WOPCM_OFFSET_VALID    (1U << 0)

/* Clock Gating */
#define MISCCPCTL_REG           0x9424   /* Misc Clock Power Control */
#define MISCCPCTL_PG_DISABLE    (1U << 0)  /* Disable power gating (i915: MISCCPCTL_DISABLE_PG) */
#define DOP_CLOCK_GATE_RENDER   (1U << 1)  /* DOP clock gate for render */

/* PM Interrupt Mask */
#define PMINTRMSK_REG           0xA168   /* PM Interrupt Mask */
#define ARAT_EXPIRED_MASK       (1U << 9)  /* ARAT expired interrupt */

/*
 * Translation Window Engine
 * ( mask )
 */
#define ENGINE_WINDOW_SIZE  0x1000     /* Engine register block = 4KB */

#pragma mark - Translation Entry Structure

/*
 * RegisterTranslationEntry
 *
 * Dynamic Offset Translation:
 * macOS / register address A ( Coffee Lake)
 * → translateTable[] offset Alder Lake
 *
 * :
 * 1. (address == fakeBase fakeBase + size)
 * 2. → return addr - fakeBase + realBase
 * 3. → return addr (pass-through)
 */
typedef struct {
 const char *name; /* engine/component (debug) */
 uint32_t fakeBase; /* MMIO base macOS (Coffee Lake) */
 uint32_t realBase; /* MMIO base (Alder Lake) */
 uint32_t windowSize; /* register ( 0x1000) */
 bool enabled; /* / */
} RegisterTranslationEntry;

#pragma mark - MyIntelGPU Class

/*
 * MyIntelGPU : IOService
 *
 * Kext — :
 * 1. Probe/Start → 配对 IOPCIDevice
 *   2. PCI Config → setBusMaster, map BAR0 + BAR2
 * 3. MMIO Register Access → / Dynamic Translation
 *   4. GGTT Management → aperture + page table
 *   5. Memory Barrier → OSSynchronizeIO + CLFLUSH
 */
class MyIntelGPU : public IOService {
    OSDeclareDefaultStructors(MyIntelGPU)

    /*!
 * @brief IOUserClient bridge — MyIntelGPUClient needs read access
     * to private GGTT/aperture members (fGsm, fGttTotal, fApertureVA,
     * fApertureSize) to service GEM alloc/map selectors.
     */
    friend class MyIntelFramebuffer;
    friend class MyIntelAccelClient;
    friend class MyIntelAccelerator;

public:

    /*
     * ─────────────────────────────────────
     *  IOKit Lifecycle Methods
     * ─────────────────────────────────────
     */

    /*!
 * @brief provider (IOPCIDevice)
     *
 * i915_pci_probe():
 * - PCI device, enable memory space
 * - device ID
 * - translation table
     *
 * @param provider IOPCIDevice IOService
 * @return true device , false
     */
    virtual bool init(OSDictionary *dict) override;

    /*!
 * @brief Kext
     *
 * Pipeline ( i915_driver_mmio_probe → hw_probe):
     *    1. super::start()
 * 2. IOPCIDevice pointer
     *    3. PCI enable + bus master
     *    4. mapDeviceMemoryWithIndex(0) → BAR0 (MMIO registers)
     *    5. mapDeviceMemoryWithIndex(2) → BAR2 (Aperture/GMADR)
 * 6. Detect generation → translation table
 * 7. GGTT init
     *    8. registerService()
     *
 * @param provider IOPCIDevice IOService
     * @return true = success, false = fail
     */
    virtual bool start(IOService *provider) override;

    /*!
 * @brief Kext (unload / sleep)
     *
     *  Cleanup MMIO mapping, free memory
     */
    virtual void stop(IOService *provider) override;

    /*!
 * @brief Free
     */
    virtual void free() override;


    /*
     * ─────────────────────────────────────
     *  Power Management Helpers
     * ─────────────────────────────────────
     */
    void disablePowerGating(void);

    /*
     * ─────────────────────────────────────
     *  MMIO Register Access
     * ─────────────────────────────────────
     */

    /*!
 * @brief 32-bit register MMIO BAR0
     *
 * :
 * 1. translateAddress(offset) → fake offset real
     *    2. *reinterpret_cast<volatile uint32_t*>(fRegs + realOffset)
     *    3. OSSynchronizeIO() → memory barrier
     *
 * @param offset MMIO register offset ( Coffee Lake)
 * @return register 32-bit
     */
    virtual uint32_t readReg32(uint32_t offset);

    /*!
 * @brief 32-bit register MMIO BAR0
     *
 * :
     *    1. translateAddress(offset) → real offset
     *    2. volatile store
     *    3. OSSynchronizeIO() → write buffer drain
     *
 * @param offset MMIO register offset ( Coffee Lake)
 * @param value
     */
    virtual void writeReg32(uint32_t offset, uint32_t value);

    /*
     * ─────────────────────────────────────
     *  Aperture Access (BAR2 / GMADR)
     * ─────────────────────────────────────
     */

    /*!
 * @brief 32-bit Aperture (BAR2)
     *
     *  Aperture = CPU-mappable window to GGTT.
 * CPU access buffer objects GGTT.
     *
 * @param apertureOffset offset aperture window
     * @return 32-bit value
     */
    virtual uint32_t readAperture32(uint64_t apertureOffset);

    /*!
 * @brief 32-bit Aperture (BAR2) Write-Combined
     *
 * CPU → GPU data transfer (staging buffer, cursors, etc.)
 * flush WC buffer :
 * - IODelayWriteCombined(uint32_t)
     *    - OSSynchronizeIO() (heavy-weight)
     *
 * @param apertureOffset offset aperture window
 * @param value
     */
    virtual void writeAperture32(uint64_t apertureOffset, uint32_t value);

    /*
     * ─────────────────────────────────────
     *  GGTT / Cache Management
     * ─────────────────────────────────────
     */

    /*!
     * @brief  GGTT TLB Invalidate
     *
     * Register: GFX_FLSH_CNTL_GEN6 = 0x101008 (Gen6+)
     *
     * Linux i915 ggtt->invalidate() (gt/intel_ggtt.c:197-198):
     *   intel_uncore_write_fw(uncore, GFX_FLSH_CNTL_GEN6, GFX_FLSH_CNTL_EN);
     *   intel_uncore_read_fw(uncore, GFX_FLSH_CNTL_GEN6);
     * → write GFX_FLSH_CNTL_EN (bit0 = 1), then posting-read.
     *   (Writing 0 = no-op — EN bit must be set.)
     */
    virtual void ggttInvalidate(void);

    /*!
     * @brief  GGTT Hardware Init (Gen8+)
     *
     * Reads GTT PTE storage size from SNB_GMCH_CTRL (PCI config 0x50),
     * computes the GTT Stolen Memory (GSM) base = BAR0 phys + 8MB,
     * maps the PTE array (UC on Gen12+), and fills fGsm/fGttTotal.
     *
     * Linux: gen8_gmch_probe() → gen8_get_total_gtt_size()
     *              → ggtt_probe_common()
     *
     * @return true = GGTT ready, false = continue framebuffer-only
     */
    virtual bool ggttInitHardware(void);

    /*!
     * @brief  Stolen DRAM detection — runs EARLY (Phase 2+, before GGTT).
     *
     * Reads the stolen DRAM base from GEN6_DSMBASE (MMIO 0x1080C0,
     * bits [63:20]) and the size from SNB_GMCH_CTRL GMS (PCI 0x50,
     * bits [15:8] × 32MB for GMS < 0x10).
     *
     * Needed BEFORE ggttInitHardware() because MyIntelFramebuffer is
     * created in Phase 5d while ggttInitHardware runs in Phase 6 —
     * the PTE-run scan there happens too late for createVRAMDescriptor().
     * (Observed: 2026-08-14 myintelfb=1 boot → stub 0x40 aperture →
     * WindowServer SIGBUS in vRotate because WindowServer got 64 bytes.)
     *
     * Linux: i915_gem_stolen.c setup stolen → GEN6_DSMBASE & GEN11_BDSM_MASK
     *
     * @return true = fStolenBase/fStolenSize valid
     */
    virtual bool detectStolenMemory(void);

    /*!
 * @brief CPU Cache Line Flush — non-coherent platforms
     *
 * Linux drm_clflush_virt_range():
 * - CLFLUSH instruction x86_64
 * - flush cache line (aligned)
 * - compiler barrier (OSMemoryBarrier) /
     *
 * cache (WB) GPU
 * ( iGPU Gen9+ has_llc==1 ;
 * discrete Gen12+ DG2/MTL has_llc==0)
     *
 * @param addr virtual address flush
 * @param size (bytes)
     */
    virtual void clflushRange(const void *addr, size_t size);

    /*
     * ─────────────────────────────────────
     *  Display / Panel Initialization
     * ─────────────────────────────────────
     */

    virtual bool initDisplay(void);
    virtual bool panelPowerOn(void);
    virtual bool initCDCLK(void);
    virtual bool initDPLL(void);
    virtual bool initBacklight(void);
    virtual bool setPanelBrightness(uint32_t brightnessPercent);
    virtual uint32_t getPanelBrightness(void);
    virtual void dumpDisplayStatus(void);

    /*!
     * @brief  Snapshot BIOS display pipeline state (plane 1A / pipe A /
     *         transcoder A) — Phase 4.3 evidence gathering.
     *
     * Taken AFTER ggttInitHardware but BEFORE WindowServer starts.
     * Hypothesis (2026-08-14, frozen logo): WindowServer picks our
     * MyIntelFramebuffer for the panel, releases NDRV, and NDRV turns
     * OFF plane 1A on release → panel freezes on the last frame (logo)
     * while the system keeps running. The delayed dump at t+10s tells
     * us whether the plane lost ENABLE after WindowServer takeover.
     */
    virtual void snapshotDisplayState(void);

    /*!
     * @brief  Delayed re-dump of the display pipeline (t+10s) + verdict.
     * Run from an IOTimerEventSource on the workloop (t+10s) so we can
     * observe what WindowServer did to the hardware without blocking
     * start() and without a raw kernel thread (2.0.208 thread variant
     * hard-hung the boot ~3.5s in — regression vs 2.0.207).
     */
    virtual void dumpDelayedDisplayState(void);
    virtual void armDelayedDisplayDump(void);
    virtual void delayedDumpTimerFired(IOTimerEventSource *sender);
    static IOReturn sDelayedDumpArmAction(OSObject *owner, void *arg0,
                                          void *arg1, void *arg2, void *arg3);
    static void sDelayedDumpTimerFired(OSObject *owner, IOTimerEventSource *sender);

    /* Phase 5d hang fix (2.0.212) — deferred framebuffer start.
     * start()/registerService()/safeInitInterrupts() run on the workloop
     * thread ~1s AFTER MyIntelGPU::start() returns (Phase 7 registerService
     * done), so the synchronous IOKit calls no longer block the serialized
     * matching / display-wrangler thread — the deterministic boot hang with
     * myintelfb=1 (ALIVE ticker proved the workloop stayed alive while IOKit
     * was stuck). new/init/attachToParent stay synchronous in Phase 5d. */
    virtual void armDeferredFramebufferStart(void);
    virtual void framebufferStartTimerFired(IOTimerEventSource *sender);
    static IOReturn sFramebufferStartArmAction(OSObject *owner, void *arg0,
                                               void *arg1, void *arg2, void *arg3);
    static void sFramebufferStartTimerFired(OSObject *owner, IOTimerEventSource *sender);

    /* Program Pipe A plane 1 to scan out a GEM buffer (framebuffer test).
     * Returns true if PLANE_SURFLIVE latched buf->ggttOffset. */
    virtual bool programPlane(MyIntelGEMBuffer *buf);

    /* Restore the plane state that programPlane() overwrote, using the
     * snapshot stored in the buffer. Call before destroying a buffer that
     * was bound to a plane — otherwise the plane scans freed GGTT and the
     * panel goes black. */
    virtual void restorePlane(MyIntelGEMBuffer *buf);

    /*
     * ─────────────────────────────────────
     *  FakeID Helpers
     * ─────────────────────────────────────
     */

    /*!
 * @brief PCI Device ID
     *
 * Flow ( intel_device_info_runtime_init_early()):
 * 1. PCI config space → device ID
 * 2. (ADL-S/P, CFL, TGL, ICL, SKL)
 * 3. ADL → fFakeGen = 9 (Coffee Lake)
 * 4. CFL → fFakeGen = 9 (Native, )
 * 5. → fFakeGen = 0 (English mode)
     *
 * GMD_ID register (0xD8C)
 * IP version
     *
 * @return (device ID + revision) 0
     */
    virtual uint32_t detectHardwareGeneration(void);

    /*!
 * @brief Translation Entry Dynamic Offset
     *
 * :
 * - entry engine offset
 * - entry cursor register
 * - fakeBase
     *
 * start() detectHardwareGeneration()
     */
    virtual void buildTranslationTable(void);

    /*!
 * @brief MMIO offset Translation
     *
 * offset macOS (Coffee Lake expected)
 * offset Alder Lake
     *
 * 3 :
 * 1. offset fakeBase entry
 * 2. → return realBase + (offset - fakeBase)
 * 3. → return offset (pass-through)
     *
 * @param fakeOffset offset Coffee Lake
 * @return offset
     */
    virtual uint32_t translateAddress(uint32_t fakeOffset);

 /* Public Accessors child classes (IntelFramebuffer ) */
    IOMemoryMap             *getMMIOMap(void) const { return fMMIOMap; }
    IOMemoryMap             *getApertureMap(void) const { return fApertureMap; }
    volatile uint8_t        *getRegs(void) const { return fRegs; }
    IOPCIDevice             *getPCIDevice(void) const { return fPCIDevice; }

    /* BAR2 (GMADR) Physical descriptor — returned to the framebuffer via getVRAMRange() */
    IODeviceMemory           *getApertureDeviceMemory(void) const {
        return static_cast<IODeviceMemory *>(fApertureDesc);
    }

    /**
 * fRegs kernel virtual address space
 * ( VMware virtual GPU BAR0 map 0x2000)
     */
    bool isValidRegs(void) const {
        uint64_t va = reinterpret_cast<uint64_t>(fRegs);
        return (va >= kMinKernelVA && va <= kMaxKernelVA);
    }

    /*
     * ─────────────────────────────────────
     *  Safe Lazy Interrupt Init
     * ─────────────────────────────────────
     */

    /*!
 * @brief Interrupt ( start() )
     *
 * 2000 microseconds initInterrupts()
 * Hardware Register Kernel
     *
 * Kernel Hang Interrupt Controller
 * start()
     *
     * @return true = success
     */
    bool safeInitInterrupts(void);

    /*!
 * @brief Property Changes ( Lazy Trigger)
     *
 * IOFramebuffer User Space Property
 * → safeInitInterrupts()
     *
 * @param properties Dictionary properties
 * @return kIOReturnSuccess error
     */
    virtual IOReturn setProperties(OSObject *properties) override;

    /*!
     * @brief  Vblank Event Notification
     *
 * IntelFramebuffer::handleInterrupt()
 * Vblank interrupt MyIntelFramebuffer::handleVblank()
 * → VSync event user space
     */
    void notifyVblank(void);

    /*
     * ─────────────────────────────────────
     *  Gen11/12 Distributed Interrupt Path
     * ─────────────────────────────────────
     *
     *  Mirrors i915 gt/intel_gt_irq.c (gen11_irq_handler →
     *  gen11_gt_irq_handler → gen11_gt_engine_identity):
     *   - IOInterruptEventSource owns PCI interrupt index 0 (GPU-only
     *     mode; IntelFramebuffer registers its own source only when the
     *     display path is enabled via -myintelfb).
     *   - sInterruptHandler reads GEN11_GFX_MSTR_IRQ (0x190010):
     *       DISPLAY bit 16 → legacy vblank path
     *       GT_DW_IRQ(0/1)  → engine bank scan (identity decode)
     *   - Enable chain (postinstall order): RCS0/BCS_RSVD_INTR_MASK =
     *     ~smask (0x190090/0x1900a0), RENDER_COPY_INTR_ENABLE = dmask
     *     (0x190030), then GFX_MSTR_IRQ = GEN11_MASTER_IRQ (0x190010).
     *   - Boot-arg "-myintelgtirq" gates the HW enable (safety; default
     *     keeps the polling path active).
     */

    /*! @brief Workloop for the engine interrupt event source (lazy) */
    IOWorkLoop *getWorkLoop(void);

    /*!
 * @brief Arm the Gen11 GT engine interrupt path (GPU-only mode)
     *
 * Creates IOInterruptEventSource on PCI vector index 0 (fallback 1),
     * then runs the gen11_gt_irq_postinstall enable chain.  Gated by
     * boot-arg "-myintelgtirq" (safety — default keeps polling path).
     */
    void armEngineInterrupts(void);

    /*!
 * @brief Emit ring command set 2 (flush + NOOP + USER_INTERRUPT)
     *
 * Runs after the set-1 completion interrupt (RCS0 USER bit).  Appends
     * to the ring at the current tail (set 1 left head==tail==24) and
     * re-submits through the execlists LRC path.
     */
    void kickCommandSet2(void);

    /*!
 * @brief Queue client work through the RCS ring (execlists path)
     *
     * Public entry for MyIntelGPUClient (IOUserClient selector 3).
     * Mirrors the armEngineInterrupts() seed: raises the ring's
     * workPending gate under fEngineLock, then kicks command set 2
     * (flush + NOOP + USER_INTERRUPT → ELSP submit).  Thread-safe
     * against the IRQ workloop (IOInterruptEventSource handler runs in
     * thread context, so a mutex — IOLock — is safe on both sides).
     *
     * @return kIOReturnSuccess  work queued (or already consumed by a
     *         racing IRQ kick — the one-shot gate makes both equal)
     * @return kIOReturnNotReady accel/ring not initialized
     */
    kern_return_t submitClientTaskViaRing(void *batchBuffer, uint32_t taskType, uint64_t packetData);

    /*!
 * @brief Entry point for GT engine interrupt dispatch
     *
 * Called from the IOInterruptEventSource handler after the identity
     * decode (RCS0 = class 0, instance 0).  gt_dw0 is the raw
     * GEN11_GT_INTR_DW(0) snapshot; RCS0 USER bit (bit 0) → kick set 2.
     */
    void processEngineInterrupt(uint32_t gt_dw0);

    /*! @brief Static IOKit interrupt trampoline (IOInterruptEventSource) */
    static void sInterruptHandler(OSObject *target,
                                  IOInterruptEventSource *sender,
                                  int count);

    /*
     * ─────────────────────────────────────
     *  Phase 6 — Power Management
     * ─────────────────────────────────────
     */

    virtual bool forceWakeGet(void);
    virtual void forceWakePut(void);
    virtual bool rc6Enable(void);
    virtual void rc6Disable(void);
    virtual bool gpuSuspend(void);
    virtual bool gpuResume(void);
    virtual void dumpPMStatus(void);

    /*!
     * @brief  Initialize GPU hardware acceleration engines
     *
     *  Create RCS + BCS ring buffers, set up GGTT page tables
     *  and emit initial ring commands.
     *
     *  Called from start() after Phase 6 (GGTT invalidate).
     *
     * @return true = success (non-fatal if false — kext still loads)
     */
    virtual bool initHardwareAcceleration(void);

    /*!
     * @brief  Full GT engine reset before execlist init (i915 gt_sanitize path)
     *
     *  Mirrors i915 gen8_reset_engines: per-engine RING_RESET_CTL handshake
     *  (assert REQUEST, wait READY_TO_RESET) then GDRST = GRDOM_FULL write
     *  and poll-until-zero, then deassert REQUEST.  Called from
     *  initHardwareAcceleration() before ringCreate so HW starts in a
     *  known-reset state (HW rejects execlist contexts otherwise).
     *
     * @return true = reset acked, false = timeout (non-fatal — caller continues)
     */
    bool gtResetEngines(void);

    /*
     * ─────────────────────────────────────
     *  GEM/Ring Callback Trampolines (static)
     * ─────────────────────────────────────
     *
     *  These bridge the C-style function pointers in MyIntelRingCallbacks
     *  to MyIntelGPU instance methods.
     */

    /* MMIO read trampoline */
    static uint32_t _readReg32(void *context, uint32_t offset);
    /* MMIO write trampoline */
    static void _writeReg32(void *context, uint32_t offset, uint32_t value);
    /* GEM alloc trampoline — allocates a GEM buffer in GGTT */
    static void *_gemAlloc(void *context, uint32_t size, uint32_t flags);
    /* GEM free trampoline */
    static void _gemFree(void *context, void *buffer);
    /* Get GGTT offset of gem buffer */
    static uint32_t _gemGetOffset(void *context, const void *buffer);
    /* Get CPU vaddr of gem buffer */
    static void *_gemGetVAddr(void *context, const void *buffer);
    static const uint64_t *_gemGetPagesPhys(void *context, const void *buffer);
    /* Force-wake acquire trampoline */
    static bool _forceWakeGet(void *context);
    /* Force-wake release trampoline */
    static void _forceWakePut(void *context);
    /* IRQ-path query trampoline (2.0.229) */
    static bool _inIrqContext(void *context);

    MyIntelRing *getRingRCS(void) const { return fRingRCS; }
    MyIntelRing *getRingBCS(void) const { return fRingBCS; }
    MyIntelRingCallbacks *getRingCallbacks(void) const { return fRingCallbacks; }
    volatile uint8_t *getApertureVA(void) const { return fApertureVA; }

    /* Phase 9 — GEM user-space accessors (VCS client buffer allocation, kext side) */
    uint32_t *getGsm(void) const { return (uint32_t *)fGsm; }
    uint64_t getApertureSize(void) const { return fApertureSize; }
    uint32_t getGttTotal(void) const { return fGttTotal; }

private:

    /*! @brief kickCommandSet2 body — MUST be called with fEngineLock held.
     *         Public wrapper acquires the lock, then calls this. Kicks the
     *         given ring (RCS or BCS per taskType); the RCS path is
     *         unchanged from before (fRingRCS). */
    void kickRingLocked(MyIntelRing *ring);

    /*
     * ─────────────────────────────────────
     *  Member Variables
     * ─────────────────────────────────────
     */

    /* PCI Device */
 IOPCIDevice *fPCIDevice; /*!< IOPCIDevice (provider) */

    /* MMIO (BAR0) */
 IOMemoryMap *fMMIOMap; /*!< IOMemoryMap BAR0 (64-bit prefetchable) */
 volatile uint8_t *fRegs; /*!< Virtual address MMIO registers */

    /* Aperture (BAR2 / GMADR) */
 IOMemoryMap *fApertureMap; /*!< IOMemoryMap BAR2 (aperture/GMADR) */
 volatile uint8_t *fApertureVA; /*!< Virtual address aperture window */
 uint64_t fApertureSize; /*!< aperture ( 256MB) */

    /* Hardware Info */
 uint32_t fDeviceID; /*!< PCI Device ID () */
    uint32_t                 fRevision;        /*!< PCI Revision ID */
    uint32_t                 fGraphicsVer;     /*!< Graphics IP version (GRAPHICS_VER)
 = 12 ADL, 9 CFL */
    uint8_t                  fFakeGen;         /*!< Fake Generation ID:
 9 = Coffee Lake
 12 = Alder Lake
 0 = , pass-through */
 bool fUseGmdId; /*!< true GMD_ID register
                                                    (Meteor Lake+) */

    /* GGTT Info */
    uint32_t                 fGttTotal;        /*!< GGTT total entries (pages) */
	uint32_t fMappableEnd; /*!< mappable aperture */
    volatile uint32_t       *fGsm;             /*!< GTT Stolen Memory (page table array) */
    IOMemoryDescriptor      *fGSMDesc;         /*!< GSM (GTT PTE array) descriptor */
    IOMemoryMap             *fGSMMap;          /*!< GSM (GTT PTE array) UC mapping */

    /* Phase 4.2 — Stolen Memory (VRAM) Info */
    uint64_t                 fStolenBase;      /*!< Physical base of stolen DRAM (from GGTT PTE[0]) */
    uint32_t                 fStolenSize;      /*!< Stolen DRAM size in bytes (fbEnd * 4096) */

    /* Phase 4.4 — VRAM Pool (GTT-addressed) Info */
    uint64_t                 fVramPoolSize;    /*!< VRAM pool bytes = min(3072MB, fGttTotal*4096) */

    /* Phase 4.3 — display pipeline snapshot (BIOS baseline, myintelfb=1) */
    uint32_t                 fSavedPlaneCtl;   /*!< PLANE_CTL_1A at snapshot */
    uint32_t                 fSavedPlaneStride;/*!< PLANE_STRIDE_1A at snapshot */
    uint32_t                 fSavedPlaneSize;  /*!< PLANE_SIZE_1A at snapshot */
    uint32_t                 fSavedPlaneSurf;  /*!< PLANE_SURF_1A at snapshot (GGTT page) */
    uint32_t                 fSavedPipeConf;   /*!< PIPE_CONF_A at snapshot */
    uint32_t                 fSavedTransConf;  /*!< TRANS_CONF_A at snapshot */
    bool                     fPlaneSnapshotValid; /*!< true after snapshotDisplayState() */
    IOTimerEventSource      *fDelayedDumpTimer;   /*!< t+10s dump + alive ticks (recurring) */
    IOTimerEventSource      *fFramebufferStartTimer; /*!< deferred FB start (t+1s, workloop) */
    IOTimerEventSource      *fEdidTimer;           /*!< EDID passthrough retry (2s x45) */
    int                      fEdidRetries;
    bool                     injectEDIDOnce(void);
    static void              sEdidTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                     armEDIDPassthrough(void);
    /* Phase 4c-1 myplane=1 scanout self-test */
    IOTimerEventSource      *fPlaneTimer;          /*!< one-shot t+15s plane flip */
    MyIntelGEMBuffer        *fPlaneTestBuf;        /*!< bound test buffer (restored+freed in stop) */
    static void              sPlaneTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                     armPlaneSelfTest(void);
    /* Phase 4c-2 myflip=1 double-buffer flip test */
    IOTimerEventSource      *fFlipTimer;           /*!< recurring 500ms A<->B flips */
    MyIntelGEMBuffer        *fFlipBufs[2];         /*!< double-buffer pair (both restored+freed in stop) */
    uint32_t                 fFlipIdx;             /*!< which buffer is on screen */
    uint32_t                 fFlipCount;           /*!< flips done (auto-stop at 60) */
    bool                     flipPlaneTo(MyIntelGEMBuffer *buf);   /*!< no-snapshot SURF flip + latch poll */
    static void              sFlipTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                     armFlipTest(void);
    /* Phase 4d-α mybridge=1 mirror takeover: IONDRV fb -> our GEM scanout */
    IOTimerEventSource      *fBridgeTimer;         /*!< recurring 100ms mirror refresh */
    MyIntelGEMBuffer        *fBridgeBuf;           /*!< mirror buffer A */
    MyIntelGEMBuffer        *fBridgeBufB;          /*!< mirror buffer B (double-buffer) */
    bool                     fBridgeShownA;        /*!< true when A is the scanned-out one */
    uint32_t                 fBridgeStall;         /*!< consecutive emits w/o head advance */
    bool                     fBridgeExeclistsOff;  /*!< circuit breaker latched */
    uint32_t                 fBridgeTicks;         /*!< mirror copies done */
    const char              *fBridgeWhy;           /*!< last blit-skip reason (diag) */
    static void              sBridgeTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                     armBridgeMirror(void);
    MyIntelGEMBuffer        *getBridgeBuf(void) const { return fBridgeBuf; }
    /* Phase B: deferred accelerator adoption — node appears AFTER WindowServer
     * is stable so SW compositor never sees a half-built GPU path at boot */
    IOTimerEventSource      *fAccelDeferTimer;     /*!< one-shot t+45s node create */
    uint32_t                 fAccelDeferMode;      /*!< attach mode (1=this, 2=PCI) */
    static void              sAccelDeferTimerFired(OSObject *owner, IOTimerEventSource *sender);
    void                     armAccelDefer(uint32_t mode);
    bool                     createAcceleratorNode(uint32_t mode);
    /* Phase B step-machine: freeze-safe incremental adoption via ioctls */
    class MyIntelAccelerator *fAccelStepObj;      /*!< alloc'd, not attached */
    uint32_t                 fAccelStepMode;      /*!< remembered attach mode */
    bool                     accelStepAlloc(uint32_t mode);
    bool                     accelStepAttach(uint32_t parentSel); /*!< 0=this,1=PCI,2=IOResources */
    bool                     accelStepStart(void);
    bool                     accelPadProps(void);   /*!< Door-A: eGPU-trick property injection */
    /* Hypothesis #10: BCS blit as BATCH+BB_START (proven VDBOX/gem_test pattern) */
    MyIntelGEMBuffer        *fAccelBatchBuf;       /*!< blit batch buffer (lazy alloc) */
    bool                     accelBatchBlit(void);
    bool                     accelBCSReset(void);  /*!< Hyp#11: GDRST BCS + full reprogram */
    uint32_t                 fAliveTick;          /*!< alive-ticker count (5s per tick) */

    /* 2.0.223 myintelfb=1 evidence harness — IRQ storm + counters */
    uint64_t                 fIrqTotal;           /*!< master IRQs serviced */
    uint64_t                 fIrqGt0;             /*!< GT_DW_IRQ(0) bank seen */
    uint64_t                 fIrqGt1;             /*!< GT_DW_IRQ(1) bank seen */
    uint64_t                 fIrqDisplay;         /*!< GEN11_DISPLAY_IRQ seen */
    uint32_t                 fStormTick;          /*!< ALIVE tick of storm window */
    uint32_t                 fStormCount;         /*!< IRQs in current tick window */

    /* Translation Table */
 RegisterTranslationEntry fTransTable[10]; /*!< register offset */
 int fTransCount; /*!< entry */

 /* Memory Map References — release stop() */
    IOMemoryDescriptor      *fMMIODesc;        /*!< BAR0 IOMemoryDescriptor */
    IOMemoryDescriptor      *fApertureDesc;    /*!< BAR2 IOMemoryDescriptor */

    /* Framebuffer / Interrupt Manager */
    class IntelFramebuffer    *fFramebuffer;       /*!< Phase 3: Interrupt handler */
    class MyIntelFramebuffer  *fDisplayFramebuffer; /*!< Phase 4: IOFramebuffer binding */
    class MyIntelAccelerator  *fAccelerator;       /*!< Phase 6c: IOAccelerator binding (myintelaccel) */

    /* Gen11/12 engine interrupt plumbing (GPU-only mode owns index 0) */
    IOInterruptEventSource   *fInterruptEventSource; /*!< PCI interrupt index 0 */
    IOWorkLoop               *fWorkLoop;             /*!< Event source workloop (lazy) */
    bool                      fInterruptsArmed;      /*!< true after enable chain runs */
    bool                      fStopping;             /*!< 2.0.226: stop() in progress — no deferred arming */
    bool                      fKickPending;          /*!< 2.0.226: kickCommandSet2 skipped (lock busy) — retry needed */
    bool                      fFirstIrqSeen;         /*!< 2.0.226: first master IRQ observed (IRQ path) */
    volatile bool             fInIrqContext;         /*!< 2.0.229: IRQ-servicing path active (no blocking) */
    bool                      fFirstIrqMarked;       /*!< 2.0.226: irq:first-entry NVRAM marker written (thread ctx) */

    /*
     * ─────────────────────────────────────
     *  Phase 5 — HW Acceleration Members
     * ─────────────────────────────────────
     */

    /* Ring Buffer Engines */
    MyIntelRing              *fRingRCS;           /*!< Render Command Streamer ring */
    MyIntelRing              *fRingBCS;           /*!< Blitter Command Streamer ring */

    /* Ring Callbacks — struct for ringCreate/Submit */
    MyIntelRingCallbacks     *fRingCallbacks;     /*!< Dynamically allocated callbacks */

    /* GEM Pool — simple tracking for allocated buffers
     *  (Phase 5: just track RCS + BCS ring buffers) */
    MyIntelGEMBuffer         *fGemRingRCS;        /*!< GEM buffer for RCS ring */
    MyIntelGEMBuffer         *fGemRingBCS;        /*!< GEM buffer for BCS ring */

    /* Phase 5 init state */
    bool                      fAccelInitialized;  /*!< true after initHardwareAcceleration() */

    /* Engine kick lock — serializes kickCommandSet2() between the IRQ
     * workloop thread and client (IOUserClient) threads. Non-recursive
     * IOLock: the public wrapper is the ONLY lock site; never nest. */
    IOLock                   *fEngineLock;        /*!< NULL until start() */

    /*
     * ─────────────────────────────────────
     *  Phase 6 — Power Management Members
     * ─────────────────────────────────────
     */
    volatile uint32_t        *fForceWakeRegs;     /*!< Forced wake MMIO base pointer */
    uint32_t                  fFWRefCount;         /*!< Force wake reference count */
    bool                      fRC6Enabled;         /*!< RC6 currently active */

    /* Accessor for IntelFramebuffer (for interrupt wiring) */
    IntelFramebuffer *getFramebuffer(void) const { return fFramebuffer; }

    /* Phase 4.2 — Stolen VRAM accessors (set during ggttInitHardware) */
    uint64_t getStolenBase(void) const { return fStolenBase; }
    uint32_t getStolenSize(void) const { return fStolenSize; }

    /* Phase 4.4 — VRAM pool accessor (set during ggttInitHardware) */
    uint64_t getVramPoolSize(void) const { return fVramPoolSize; }

    /* GGTT total pages accessor (for accelerator diagnostics) */

    /* Phase 6c — accelerator accessor (NULL unless myintelaccel set) */
    class MyIntelAccelerator *getAccelerator(void) const { return fAccelerator; }
};

#endif /* __MY_INTEL_GPU_HPP__ */
