/*===========================================================================
 *  MyIntelVCS.h
 *  Hackintosh Kext — Video Codec Service (VCS) Register Definitions
 *
 *  สร้าง 2026-08-19 — Phase 7a: VCS Hardware Registers
 *
 *  กำหนด register map สำหรับ Intel Gen12+ VCS (Video Codec Service) engine:
 *    - VCS0 (Video Decode): MMIO base 0x1C0000
 *    - VCS1 (Video Decode): MMIO base 0x1C4000
 *    - VECS0 (Video Encode): MMIO base 0x1C8000
 *
 *  ใช้สำหรับ hardware H.264/HEVC video decode โดยไม่ต้องผ่าน Metal
 *
 *  อ้างอิง:
 *    - Intel PRM Vol 2c — VCS/VDBox registers
 *    - Linux i915: drivers/gpu/drm/i915/gt/intel_engine_regs.h
 *    - Linux i915: drivers/gpu/drm/i915/gt/uc/intel_huc.c
 *    - Intel Media Driver: media_driver/linux/ult/hw/mhw_vdbox_hcp_g12_X.cpp
 *///=========================================================================

#ifndef __MY_INTEL_VCS_H__
#define __MY_INTEL_VCS_H__

#include <stdint.h>

/*
 * ─────────────────────────────────────────────
 *  VCS Engine MMIO Base Addresses
 * ─────────────────────────────────────────────
 *
 *  Gen12+ (Alder Lake / Raptor Lake) video engines:
 *    VCS0  = 0x1C0000  (Video Decode Instance 0)
 *    VCS1  = 0x1C4000  (Video Decode Instance 1)
 *    VECS0 = 0x1C8000  (Video Encode Instance 0)
 *
 *  Note: Coffee Lake (Gen9) VCS was at 0x12000
 *        Gen12+ moved it to 0x1C0000 — this is why translation is needed
 *
 *  Reference: intel_engine_cs.c __engine_mmio_base()
 */
#define VCS0_MMIO_BASE          0x1C0000
#define VCS1_MMIO_BASE          0x1C4000
#define VECS0_MMIO_BASE         0x1C8000

/* Window size for each video engine (4KB) */
#define VCS_ENGINE_WINDOW_SIZE  0x1000

/*
 * ─────────────────────────────────────────────
 *  VCS Ring Registers (offset from engine base)
 * ─────────────────────────────────────────────
 *
 *  Same layout as RCS/BCS:
 *    RING_TAIL    = base + 0x30
 *    RING_HEAD    = base + 0x34
 *    RING_START   = base + 0x38
 *    RING_CTL     = base + 0x3C
 *    RING_MI_MODE = base + 0x9C
 *
 *  Note: These are defined in MyIntelRing.hpp as RING_*_REG_OFFSET
 *        We define them here as well for VCS-specific code
 */
#define VCS_RING_TAIL_REG       0x30
#define VCS_RING_HEAD_REG       0x34
#define VCS_RING_START_REG      0x38
#define VCS_RING_CTL_REG        0x3C
#define VCS_RING_MI_MODE_REG    0x9C
#define VCS_RING_HWS_PGA_REG    0x80    /* Hardware Status Page */

/*
 * ─────────────────────────────────────────────
 *  VCS Status Registers
 * ─────────────────────────────────────────────
 *
 *  Used to check VCS engine status and progress
 */
#define VCS0_MI_MODE            0x1C009C
#define VCS0_MMIO_CTRL          0x1C0094
#define VCS0_CREDENTIALS        0x1C0090
#define VCS0_IRQ_STATUS         0x1C0044  /* VCS0 interrupt status */

/*
 * ─────────────────────────────────────────────
 *  HuC (Video Codec Controller) Registers
 * ─────────────────────────────────────────────
 *
 *  HuC manages firmware for video decode/encode
 *  Must be loaded before VCS can process commands
 *
 *  Reference: intel_huc.h, intel_huc_fw.c
 */
#define HUC_STATUS_REG          0x1C2000  /* HuC status (VCS0 + 0x2000) */
#define HUC_STATUS2_REG         0x1C2300  /* HuC status 2 */
#define HUC_MAILBOX_BASE        0x1C2000  /* HuC mailbox base */
#define HUC_STATUS_DONE         (1U << 0) /* HuC firmware loaded */
#define HUC_AUTHORIZED          (1U << 1) /* HuC authorized by GuC */

/*
 * ─────────────────────────────────────────────
 *  MFX (Media Fixed Function) Registers
 * ─────────────────────────────────────────────
 *
 *  MFX engine handles H.264/AVC decode/encode
 *
 *  Reference: mhw_vdbox_mfx_g12_X.h
 */
#define MFX_STATUS_REG          0x1C0800  /* MFX status */
#define MFX_MODE_REG            0x1C0800  /* MFX mode (decode vs encode) */
#define MFX_SURFACE_STATE_REG   0x1C0804  /* Surface state config */

/* MFX Mode bits */
#define MFX_MODE_CODEC_MASK     0x00000007
#define MFX_MODE_CODEC_H264     0x00000000  /* H.264/AVC */
#define MFX_MODE_CODEC_HEVC     0x00000001  /* HEVC/H.265 */
#define MFX_MODE_CODEC_VP9      0x00000002  /* VP9 */
#define MFX_MODE_DECODE         (1U << 4)   /* Decode mode */
#define MFX_MODE_ENCODE         (0U << 4)   /* Encode mode */

/*
 * ─────────────────────────────────────────────
 *  HCP (HEVC Codec Specific) Registers
 * ─────────────────────────────────────────────
 *
 *  HCP engine handles HEVC/H.265 specific decode/encode
 *  Shares VCS ring with MFX
 *
 *  Reference: mhw_vdbox_hcp_g12_X.h
 */
#define HCP_STATUS_REG          0x1C2800  /* HCP status */
#define HCP_MODE_REG            0x1C2800  /* HCP mode config */

/* HCP Status bits */
#define HCP_STATUS_BUSY         (1U << 0)
#define HCP_STATUS_DONE         (1U << 1)
#define HCP_STATUS_ERROR        (1U << 2)

/*
 * ─────────────────────────────────────────────
 *  Force Wake for VCS Domain
 * ─────────────────────────────────────────────
 *
 *  VCS engine has separate force wake domain
 *  Must be woken before accessing VCS registers during RC6
 *
 *  Reference: xe_force_wake.c, intel_uncore.c
 */
#define FORCEWAKE_VCS           0xA188    /* Same as GT force wake */
#define FORCEWAKE_ACK_VCS       0x130044  /* Acknowledge register */

/* VCS-specific force wake bit (multicast) */
#define FW_VCS_BIT              (1U << 1) /* Bit 1 = VCS domain */

/*
 * ─────────────────────────────────────────────
 *  VCS MMIO Range for Translation
 * ─────────────────────────────────────────────
 *
 *  When VCS is active, MMIO accesses in range 0x1C0000-0x1CFFFF
 *  must be translated from Coffee Lake (0x12000-0x12FFF) to real addresses
 *
 *  The translation window covers:
 *    VCS0:  0x1C0000 - 0x1C3FFF  (16KB)
 *    VCS1:  0x1C4000 - 0x1C7FFF  (16KB)
 *    VECS0: 0x1C8000 - 0x1CBFFF  (16KB)
 */
#define VCS_MMIO_START          0x1C0000
#define VCS_MMIO_END            0x1CFFFF

/*
 * ─────────────────────────────────────────────
 *  VCS Command Buffer Types
 * ─────────────────────────────────────────────
 *
 *  Command buffer types for VCS submission
 */
#define VCS_CMD_H264_DECODE     0x01    /* H.264/AVC decode */
#define VCS_CMD_HEVC_DECODE     0x02    /* HEVC/H.265 decode */
#define VCS_CMD_VP9_DECODE      0x03    /* VP9 decode */
#define VCS_CMD_H264_ENCODE     0x11    /* H.264/AVC encode */
#define VCS_CMD_HEVC_ENCODE     0x12    /* HEVC/H.265 encode */

/*
 * ─────────────────────────────────────────────
 *  VCS Surface Formats
 * ─────────────────────────────────────────────
 *
 *  Video surface pixel formats
 */
#define VCS_SURFACE_NV12        0x00    /* NV12 (YUV 4:2:0 semi-planar) */
#define VCS_SURFACE_P010        0x01    /* P010 (10-bit YUV 4:2:0) */
#define VCS_SURFACE_AYUV        0x02    /* AYUV (YUV 4:4:4) */
#define VCS_SURFACE_Y8          0x03    /* Grayscale (8-bit) */

/*
 * ─────────────────────────────────────────────
 *  VCS Performance Counters
 * ─────────────────────────────────────────────
 *
 *  Optional performance monitoring registers
 */
#define VCS_PERF_REG_BASE       0x1C0400
#define VCS_PERF_CYCLES         (VCS_PERF_REG_BASE + 0x00)
#define VCS_PERF_INST           (VCS_PERF_REG_BASE + 0x04)
#define VCS_PERF_STALL          (VCS_PERF_REG_BASE + 0x08)

/*
 * ─────────────────────────────────────────────
 *  VCS Helper Macros
 * ─────────────────────────────────────────────
 */

/* Check if address is in VCS MMIO range */
#define IS_VCS_ADDRESS(addr) \
    ((addr) >= VCS_MMIO_START && (addr) <= VCS_MMIO_END)

/* Get VCS instance from address (0 or 1) */
#define VCS_INSTANCE(addr) \
    (((addr) - VCS_MMIO_START) >> 14)  /* Divide by 16KB */

/* Align to 8 bytes (qword) — HW requirement for Gen8+ */
#define VCS_ALIGN_8(x)  (((x) + 7) & ~7U)

/* Align to 16 bytes (cache line) */
#define VCS_ALIGN_16(x) (((x) + 15) & ~15U)

/*
 * ─────────────────────────────────────────────
 *  VCS Context Definition
 * ─────────────────────────────────────────────
 *
 *  Structure to hold VCS engine state
 */
typedef struct {
    uint32_t mmioBase;          /* VCS engine MMIO base (0x1C0000) */
    uint32_t instance;          /* VCS instance (0 or 1) */
    bool     initialized;       /* VCS engine initialized */
    bool     hucLoaded;         /* HuC firmware loaded */
    bool     hucAuthorized;     /* HuC authorized by GuC */
    uint32_t currentCodec;      /* Current codec (H264/HEVC/VP9) */
    uint32_t surfaceFormat;     /* Current surface format */
} MyIntelVCSContext;

/*
 * ─────────────────────────────────────────────
 *  VCS API (implemented in MyIntelVCS.cpp)
 * ─────────────────────────────────────────────
 */

/* Initialize VCS engine */
bool vcsEngineInit(MyIntelVCSContext *ctx, uint32_t mmioBase, void *regs);

/* Shutdown VCS engine */
void vcsEngineShutdown(MyIntelVCSContext *ctx, void *regs);

/* Check if VCS engine is ready */
bool vcsEngineReady(const MyIntelVCSContext *ctx, const void *regs);

/* Reset VCS engine */
bool vcsEngineReset(MyIntelVCSContext *ctx, void *regs);

/* Load HuC firmware */
bool vcsLoadHuCFirmware(MyIntelVCSContext *ctx, void *regs,
                        const uint32_t *firmware, uint32_t size);

/* Submit decode command to VCS ring */
bool vcsSubmitDecodeCommand(void *ring, void *callbacks,
                           const uint32_t *commands, uint32_t size);

/* Wait for VCS completion */
bool vcsWaitForCompletion(const MyIntelVCSContext *ctx, const void *regs,
                         uint32_t timeoutMs);

/* Get VCS status */
uint32_t vcsGetStatus(const MyIntelVCSContext *ctx, const void *regs);

#endif /* __MY_INTEL_VCS_H__ */
