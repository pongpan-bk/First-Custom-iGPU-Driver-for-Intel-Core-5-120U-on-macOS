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
#include "MyIntelVCS.h"

/* 2.0.224: early-boot progress marker. Written to NVRAM + IOLog at every
 * startup phase so a hard-locked boot (myintelfb=1, hangs before logd)
 * can be diagnosed after a forced power-off: reboot safe, then run
 * `nvram myintelgpu-progress` to see the last phase reached. */
extern void mygpuProgress(const char *tag);

/* Forward declaration for ring struct — full definition in MyIntelRing.hpp */
struct MyIntelRing;
struct MyIntelRingCallbacks;
class MyIntelVCSClient;

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
#define PP_OFF_DELAYS         0x6120C
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
#define DPLL0_CFGCR0         0x164284  /* ตัวอย่างแอดเดรสสำหรับ Gen 12 Combo PHY */

#pragma mark - Class Declaration

class MyIntelGPU : public IOService {
    OSDeclareDefaultStructors(MyIntelGPU)
    
public:
    /* IOKit Driver Lifecycle Hooks */
    virtual bool init(OSDictionary *dictionary) override;
    virtual IOService* probe(IOService *provider, SInt32 *score) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free(void) override;

    /* MMIO Register Interface Operations */
    virtual uint32_t readRegister32(uint32_t offset);
    virtual void writeRegister32(uint32_t offset, uint32_t value);
    
    /* Translation Helpers for Dynamic Emulation Mode */
    uint32_t translateRegisterOffset(uint32_t fakeOffset);

protected:
    /* Hardware Communication Mappings */
    IOPCIDevice*            fPCIDevice;
    IOMemoryMap*            fMMIOMap;          // ผูกข้อมูบล BAR0 (MMIO Space)
    mach_vm_address_t       fMMIOBaseAddress;   // ตําแหน่งหน่วยความจำฐานที่แมปเสร็จแล้ว
    
    IOMemoryMap*            fApertureMap;      // ผูกข้อมูล BAR2 (Aperture Space)
    mach_vm_address_t       fApertureBaseAddress;

    /* Execution & Asynchronous Event Management Loop */
    IOWorkLoop*             fWorkLoop;
    IOInterruptEventSource* fInterruptSource;
    IOTimerEventSource*     fTimerSource;

    /* Ring Buffers pointers for Phase 5 Multi-Engine Staging */
    MyIntelRing*            fRingRCS;          // Render Engine Command Stream
    MyIntelRing*            fRingBCS;          // Blitter Engine Command Stream
    MyIntelRing*            fRingVCS;          // Video Codec Command Stream
};

#endif /* __MY_INTEL_GPU_HPP__ */
