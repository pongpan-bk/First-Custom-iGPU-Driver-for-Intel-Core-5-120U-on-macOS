/*===========================================================================
 *  MyIntelMedia.hpp
 *  Hackintosh Kext — Phase 7: Media Engine (VDBOX / VEBOX)
 *
 *  จัดการ Media Accelerator Engines สำหรับ Raptor Lake (Gen12.2):
 *    - VDBOX (VCS0-VCS3): H.264, HEVC, AV1, VP9 decode
 *    - VEBOX (VECS0-VECS1): H.264, HEVC encode
 *    - SFC (Scalable Format Converter): NV12→P010, scaling, CSC
 *
 *  อ้างอิง Linux i915:
 *    drivers/gpu/drm/i915/gt/intel_engine_cs.c — engine mmio bases
 *    drivers/gpu/drm/i915/i915_reg.h — VDBOX/VEBOX register macros
 *    drivers/gpu/drm/i915/gt/intel_gpu_commands.h — media pipeline commands
 *///=========================================================================

#ifndef __MY_INTEL_MEDIA_HPP__
#define __MY_INTEL_MEDIA_HPP__

#include <stdint.h>

#pragma mark - Codec Capability Flags

/*
 * ─────────────────────────────────────────────
 *  Codec Capability Bitmask
 *
 *  แต่ละบิตบอกว่า RPL GPU รองรับ codec ไหนบ้าง
 *  ใช้สำหรับสร้าง IOKit property dictionary
 *  ให้ VideoToolbox / AVFoundation เห็น
 *
 *  Raptor Lake (Gen12.2) typically supports:
 *    - H.264:   decode up to 4K, encode up to 4K
 *    - HEVC:    decode 8-bit/10-bit 4K, encode 8-bit/10-bit 4K
 *    - AV1:     decode 8-bit/10-bit 4K (Gen12.2+)
 *    - VP9:     decode 8-bit/10-bit 4K
 *    - VP8:     decode
 *    - MPEG-2:  decode (via MFX legacy)
 *    - VC-1:    decode (via MFX legacy)
 *    - JPEG/MJPEG: decode (via VDBOX)
 * ─────────────────────────────────────────────
 */
typedef enum {
    kMyIntelCodecH264     = (1U << 0),   /* H.264/AVC decode + encode */
    kMyIntelCodecHEVC     = (1U << 1),   /* H.265/HEVC decode + encode */
    kMyIntelCodecAV1      = (1U << 2),   /* AV1 decode only (Gen12+) */
    kMyIntelCodecVP9      = (1U << 3),   /* VP9 decode only */
    kMyIntelCodecVP8      = (1U << 4),   /* VP8 decode */
    kMyIntelCodecJPEG     = (1U << 5),   /* JPEG/MJPEG decode */
    kMyIntelCodecMPEG2    = (1U << 6),   /* MPEG-2 decode (MFX legacy) */
    kMyIntelCodecVC1      = (1U << 7),   /* VC-1 decode (MFX legacy) */
    kMyIntelCodecAVCEncode = (1U << 8),  /* H.264 encode via VEBOX */
    kMyIntelCodecHEVCEncode = (1U << 9), /* HEVC encode via VEBOX */
} MyIntelCodecCaps;

/*
 * Default codec capability mask for Raptor Lake-P (Gen12.2)
 * — 2 VDBOX (decode all) + 1 VEBOX (H.264/HEVC encode)
 */
#define RPL_DEFAULT_CODEC_CAPS  ( \
    kMyIntelCodecH264     | \
    kMyIntelCodecHEVC     | \
    kMyIntelCodecAV1      | \
    kMyIntelCodecVP9      | \
    kMyIntelCodecVP8      | \
    kMyIntelCodecJPEG     | \
    kMyIntelCodecMPEG2    | \
    kMyIntelCodecVC1      | \
    kMyIntelCodecAVCEncode | \
    kMyIntelCodecHEVCEncode \
)

#pragma mark - Max Resolution Limits

/* Maximum decode resolution per codec (RPL Gen12.2) */
#define RPL_H264_MAX_WIDTH     4096
#define RPL_H264_MAX_HEIGHT    4096
#define RPL_HEVC_MAX_WIDTH     8192
#define RPL_HEVC_MAX_HEIGHT    8192
#define RPL_AV1_MAX_WIDTH      8192
#define RPL_AV1_MAX_HEIGHT     8192
#define RPL_VP9_MAX_WIDTH      8192
#define RPL_VP9_MAX_HEIGHT     8192

#pragma mark - Media Engine Instance Info

/*
 * ─────────────────────────────────────────────
 *  MyIntelMediaVdboxInfo
 *
 *  สถานะของ VDBOX engine แต่ละ instance
 *  (ใช้ใน initMediaEngines เพื่อ track ว่า VDBOX
 *   ไหนถูก initialize แล้วบ้าง)
 *
 *  VDBOX index mapping (RPL-P):
 *    Index 0: VCS0  @ 0x1C0000  — MFX0 (H.264/HEVC/AV1/VP9)
 *    Index 1: VCS1  @ 0x1C4000  — MFX1 (same) — ถ้ามี
 *    Index 2: VCS2  @ 0x1D0000  — MFX2 — higher SKU
 *    Index 3: VCS3  @ 0x1D4000  — MFX3
 * ─────────────────────────────────────────────
 */
typedef struct {
    uint32_t  mmioBase;       /*!< VDBOX MMIO base address */
    uint32_t  sfcBase;        /*!< SFC MMIO base (mmioBase + 0x9000) */
    uint32_t  auxInvReg;      /*!< AUX Invalidate register address */
    uint8_t   index;          /*!< Instance index (0-3) */
    bool      hasSfc;         /*!< true if this VDBOX has SFC (Gen12+) */
    bool      initialized;    /*!< true after init */
} MyIntelMediaVdboxInfo;

/*
 * ─────────────────────────────────────────────
 *  MyIntelMediaVeboxInfo
 *
 *  สถานะของ VEBOX engine แต่ละ instance
 *
 *  Index mapping (RPL-P):
 *    Index 0: VECS0 @ 0x1C8000  — VEBOX0 (H.264/HEVC encode)
 *    Index 1: VECS1 @ 0x1D8000  — VEBOX1 — ถ้ามี
 * ─────────────────────────────────────────────
 */
typedef struct {
    uint32_t  mmioBase;       /*!< VEBOX MMIO base address */
    uint32_t  auxInvReg;      /*!< AUX Invalidate register address */
    uint8_t   index;          /*!< Instance index (0-1) */
    bool      initialized;    /*!< true after init */
} MyIntelMediaVeboxInfo;

#pragma mark - SFC (Scalable Format Converter)

/*
 * ─────────────────────────────────────────────
 *  SFC Format IDs (for SFC_FORMAT_INPUT/OUTPUT)
 *
 *  Reference: i915_reg.h — SFC_FORMAT_*
 *
 *  SFC converts between these formats:
 *    Input:  NV12 (YUV 4:2:0 planar), P010 (10-bit)
 *    Output: NV12, P010, RGB32 (rare)
 * ─────────────────────────────────────────────
 */
typedef enum {
    kSfcFormatNV12   = 0,   /* YUV 4:2:0 8-bit planar */
    kSfcFormatP010   = 1,   /* YUV 4:2:0 10-bit planar */
    kSfcFormatYUY2   = 2,   /* YUV 4:2:2 packed */
    kSfcFormatAYUV   = 3,   /* YUV 4:4:4 packed */
    kSfcFormatRGB32  = 4,   /* RGB 8:8:8 */
} MyIntelSfcFormat;

/*
 * SFC chroma siting (for scaling filter)
 */
typedef enum {
    kSfcChromaSitingNone    = 0,
    kSfcChromaSitingHoriz   = 1,   /* MPEG-2 horizontal */
    kSfcChromaSitingVert    = 2,   /* MPEG-2 vertical */
    kSfcChromaSitingBoth    = 3,   /* both */
} MyIntelSfcChromaSiting;

#pragma mark - Media Pipeline Commands

/*
 * ─────────────────────────────────────────────
 *  Media Command Opcodes
 *
 *  Media instructions use INSTR_MEDIA_SUBCLIENT encoding
 *  (bits 31:29 = 010 = 0x40000000 range)
 *
 *  Reference: intel_gpu_commands.h — MEDIA_INSTR macro
 *             MEDIA_INSTR(pipe, op, sub_op, flags)
 *               pipe:    0=MFX, 1=Other
 *               op:      opcode
 *               sub_op:  sub-opcode
 *               flags:   extra flags
 * ─────────────────────────────────────────────
 */
#define MEDIA_INSTR(pipe, op, sub_op, flags) \
    (0x40000000 | ((pipe) << 27) | ((op) << 20) | ((sub_op) << 12) | (flags))

/* MFX_WAIT_CMD — synchronize MFX pipe (GPU command opcode)
   Note: MFX_WAIT (register address) defined in MyIntelGPU.hpp
   Note: MFX_WAIT_DW0_MFX_SYNC_CONTROL_FLAG defined in MyIntelGPU.hpp */
#define MFX_WAIT_CMD                MEDIA_INSTR(1, 0, 0, 0)
#define   MFX_WAIT_DW0_PXP_SYNC_CONTROL_FLAG  (1U << 9)

/* PIPELINE_SELECT (for media) */
#define PIPELINE_SELECT_MEDIA       (1U << 0)

/* MEDIA_VFE_STATE — Media Vector Forward Engine State */
#define MEDIA_VFE_STATE             MEDIA_INSTR(0, 0, 0, 0)

/* MEDIA_INTERFACE_DESCRIPTOR_LOAD */
#define MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INSTR(0, 0, 0, 0)

/* MEDIA_OBJECT — dispatch media kernel */
#define MEDIA_OBJECT                MEDIA_INSTR(0, 0, 0, 0)

/* MEDIA_STATE_FLUSH */
#define MEDIA_STATE_FLUSH           (0x00000004)

/* STATE_BUFFER_OBJECT — indirect data buffer for media */
#define STATE_BUFFER_OBJECT(n)      (0x68000000 | ((n) & 0x1F))

/* STATE_BINDING_TABLE_POINTER — surface binding */
#define STATE_BINDING_TABLE_POINTER 0x78000000

/* 3DSTATE (for media pipeline) */
#define MEDIA_3DSTATE_VFE           (0x00000000)

#pragma mark - IOKit Property Keys

/*
 * ─────────────────────────────────────────────
 *  IOKit Property keys for VideoToolbox / IOGraphics
 *
 *  These properties are published in the IORegistry
 *  so that macOS VideoToolbox / AVFoundation can
 *  discover GPU media capabilities.
 *
 *  Reference: AppleGraphicsControl, IOGraphicsFamily
 *             AppleIntelGraphicsDriver (shim)
 *
 *  Keys pattern:
 *    "HevcDecoding"  → boolean/array of supported formats
 *    "HevcEncoding"  → boolean
 *    "AV1Decoding"   → boolean
 *    "H264Decoding"  → boolean
 *    "H264Encoding"  → boolean
 * ─────────────────────────────────────────────
 */
#define kMyIntelMediaKey_HEVCDecode     "HevcDecoding"
#define kMyIntelMediaKey_HEVCEncode     "HevcEncoding"
#define kMyIntelMediaKey_AV1Decode      "AV1Decoding"
#define kMyIntelMediaKey_H264Decode     "H264Decoding"
#define kMyIntelMediaKey_H264Encode     "H264Encoding"
#define kMyIntelMediaKey_VP9Decode      "VP9Decoding"
#define kMyIntelMediaKey_MediaCapable   "IntelMediaCapable"
#define kMyIntelMediaKey_VdboxCount     "VDBOXCount"
#define kMyIntelMediaKey_VeboxCount     "VEBOXCount"
#define kMyIntelMediaKey_MaxDecodeRes   "MaxDecodeResolution"
#define kMyIntelMediaKey_MaxEncodeRes   "MaxEncodeResolution"

/*
 * ─────────────────────────────────────────────
 *  MFX Pipe Clock Gating Control
 *
 *  VDBOX global registers (base + 0x6000 + offset)
 *  ใช้ disable clock gating สำหรับบางหน่วยที่จำเป็น
 *  ระหว่าง media engine bringup
 *
 *  Reference: i915_reg.h — VDBOX_CGCTL3F10, VDBOX_CGCTL3F18
 *             xe_media.c — media_gt_init
 * ─────────────────────────────────────────────
 */

/* Clock gating register offsets from VDBOX global base (base + 0x6000) */
#define VDBOX_CGCTL_3F10_OFF    0x3F10    /* IECP clock gate */
#define VDBOX_CGCTL_3F18_OFF    0x3F18    /* ALN clock gate */
#define VDBOX_CGCTL_3F1C_OFF    0x3F1C    /* MFXPIPE clock gate */

/* Bit definitions */
#define VDBOX_CGCTL_IECP_DIS    (1U << 22)  /* Disable IECP clock gate */
#define VDBOX_CGCTL_ALN_DIS     (1U << 13)  /* Disable ALN clock gate */
#define VDBOX_CGCTL_MFXPIPE_DIS (1U << 3)   /* Disable MFXPIPE clock gate */

#pragma mark - Inline SFC Helpers

/*
 * ─────────────────────────────────────────────
 *  SFC Lock/Unlock Protocol
 *
 *  ก่อนใช้ SFC ต้อง lock ก่อน:
 *    1. Write 1 to SFC_LOCK
 *    2. Poll SFC_LOCK_ACK until bit set
 *    3. Use SFC
 *    4. Write 0 to SFC_LOCK to release
 *
 *  SFC is shared between VDBOX and VEBOX on same pipe.
 *  VCS0 → SFC0, VCS1 → SFC1, VCS2 → SFC2, VCS3 → SFC3
 *  VECS0 shares SFC0 with VCS0, VECS1 shares SFC1 with VCS1
 *
 *  Reference: i915_reg.h — GEN11_VCS_SFC_FORCED_LOCK
 *             intel_engine_regs.h — SFC_LOCK_STATUS
 * ─────────────────────────────────────────────
 */

/*!
 * @brief  Lock SFC for exclusive access
 *
 * @param readReg   Read MMIO function
 * @param writeReg  Write MMIO function
 * @param context   Context for callbacks
 * @param sfcBase   SFC MMIO base (VDBOX base + 0x9000)
 * @return true = locked
 */
static inline bool sfcLock(
    uint32_t (*readReg)(void *ctx, uint32_t off),
    void     (*writeReg)(void *ctx, uint32_t off, uint32_t val),
    void     *context,
    uint32_t sfcBase)
{
    uint32_t timeout = 1000;  /* 1ms loop */

    /* Request lock */
    writeReg(context, sfcBase + 0x000, 1);  /* SFC_LOCK */

    /* Wait for acknowledge */
    while (timeout--) {
        if (readReg(context, sfcBase + 0x004) & 1) {  /* SFC_LOCK_ACK */
            return true;
        }
    }
    return false;  /* Timeout */
}

/*!
 * @brief  Unlock SFC
 */
static inline void sfcUnlock(
    void (*writeReg)(void *ctx, uint32_t off, uint32_t val),
    void     *context,
    uint32_t sfcBase)
{
    writeReg(context, sfcBase + 0x000, 0);  /* SFC_LOCK = 0 */
}

/*!
 * @brief  Check if SFC is busy (usage bit set)
 */
static inline bool sfcIsBusy(
    uint32_t (*readReg)(void *ctx, uint32_t off),
    void     *context,
    uint32_t sfcBase)
{
    return (readReg(context, sfcBase + 0x008) & 1) != 0;  /* SFC_USAGE */
}

/*
 * ─────────────────────────────────────────────
 *  MFX Pipe Inline Helpers
 * ─────────────────────────────────────────────
 */

/*!
 * @brief  Check if VDBOX is available (not fused off)
 *
 *  VDBOX disable mask register: bits [n] = 1 → VDBOX n disabled
 *
 * @param vdboxMask  Value read from GEN11_GT_VEBOX_VDBOX_DISABLE
 * @param index      VDBOX index (0-3)
 * @return true if available
 */
static inline bool vdboxIsAvailable(uint32_t vdboxDisableMask, uint32_t index)
{
    return !(vdboxDisableMask & (1U << index));
}

/*!
 * @brief  Check if VEBOX is available (not fused off)
 *
 *  VEBOX disable mask register: bits [16+index] = 1 → VEBOX index disabled
 *
 * @param veboxDisableMask  Value read from GEN11_GT_VEBOX_VDBOX_DISABLE
 * @param index             VEBOX index (0-1)
 * @return true if available
 */
static inline bool veboxIsAvailable(uint32_t veboxDisableMask, uint32_t index)
{
    return !(veboxDisableMask & (1U << (16 + index)));
}

/*!
 * @brief  Get VDBOX MMIO base for given index
 */
static inline uint32_t vdboxBaseForIndex(uint32_t index)
{
    static const uint32_t bases[] = {
        0x1C0000,  /* VCS0 */
        0x1C4000,  /* VCS1 */
        0x1D0000,  /* VCS2 */
        0x1D4000,  /* VCS3 */
    };
    if (index >= sizeof(bases) / sizeof(bases[0])) return 0;
    return bases[index];
}

/*!
 * @brief  Get VEBOX MMIO base for given index
 */
static inline uint32_t veboxBaseForIndex(uint32_t index)
{
    static const uint32_t bases[] = {
        0x1C8000,  /* VECS0 */
        0x1D8000,  /* VECS1 */
    };
    if (index >= sizeof(bases) / sizeof(bases[0])) return 0;
    return bases[index];
}

#pragma mark - AV1-Specific Constants

/*
 * AV1 decode capability hints for RPL (Gen12.2):
 *   - AV1 Main profile 8-bit & 10-bit
 *   - Max resolution: 8192x8192
 *   - Max tiles: 32 (8x4)
 *   - Film grain: supported
 *   - CDEF: supported
 *   - Loop restoration: supported
 *   - Global motion: supported
 *
 * Reference: Linux intel_huc.c / intel_gsc_fw.c
 */
#define AV1_MAX_TILE_COLS       8
#define AV1_MAX_TILE_ROWS       4
#define AV1_MAX_SEGMENTS        8
#define AV1_MAX_REF_FRAMES      7    /* +1 current = 8 total frame slots */

/*
 * AV1 OBU (Open Bitstream Unit) types filtered by HW
 * (GPU handles OBU parsing in bitstream — driver tells it where the
 *  sequence/ frame header OBUs are)
 */
#define AV1_OBU_SEQUENCE_HEADER     1
#define AV1_OBU_FRAME               6
#define AV1_OBU_TILE_GROUP          7
#define AV1_OBU_FRAME_HEADER        3

#pragma mark - Media Batch Buffer API

/*
 * ─────────────────────────────────────────────────────────────────
 *  MFX/Media pipeline command macros
 *
 *  These are the instruction words sent to VDBOX/VEBOX ring buffers.
 *  Format: 2 dwords per command (DW0 = opcode + flags, DW1 = data)
 *
 *  Reference: intel_gpu_commands.h — MFX_*, BCS_*
 *             gen12_media_cmd.h
 * ─────────────────────────────────────────────────────────────────
 */
#define MFX_BATCH_WAIT_CMD         0x01  /* Media batch: Stall pipeline, wait for dependencies */
#define   MFX_WAIT_FOR_DECODE      (1U << 17)
#define MFX_PIPELINE_SELECT_CMD    0x02  /* Select decode vs encode pipeline */
#define   MFX_PIPELINE_CODEC_H264  (1U << 0)
#define   MFX_PIPELINE_CODEC_HEVC  (2U << 0)
#define   MFX_PIPELINE_CODEC_VP9   (5U << 0)
#define   MFX_PIPELINE_CODEC_AV1   (12U << 0)
#define MFX_SURFACE_STATE_CMD      0x03  /* Set output surface */
#define MFX_PIPE_BUF_ADDR_CMD      0x04  /* Set buffer addresses */
#define MFX_IND_OBJ_BASE_ADDR_CMD  0x05  /* Indirect object (bitstream) */
#define MFX_BSP_CMD                0x06  /* Bitstream control */
#define MFX_MFD_CMD                0x10  /* Media Function Decode command */
#define   MFD_CMD_OP_H264          0
#define   MFD_CMD_OP_HEVC          4
#define   MFD_CMD_OP_VP9           6
#define   MFD_CMD_OP_AV1           11
#define MFX_VD_PROLOGUE_CMD        0x07  /* VD engine prologue */
#define MFX_VD_EPILOGUE_CMD        0x08  /* VD engine epilogue */
#define MFX_VD_FLUSH_CMD           0x09  /* Flush VD pipeline */
#define BCS_CMD                    0x20  /* BCS (copy/blend) command */
#define VEBOX_CMD                  0x30  /* VEBOX (enhancement) command */

/*
 * ─────────────────────────────────────────────────────────────────
 *  struct MyIntelMediaBatchBuffer
 *
 *  Media batch submission context.
 *  User fills commands into buf[] via mediaBatchAdd*(),
 *  then calls mediaBatchSubmit() to send to hardware.
 *
 *  Typical flow:
 *    mediaBatchBegin(&batch, buf, sizeof(buf));
 *    mediaBatchAddWait(&batch);
 *    mediaBatchAddMfdCmd(&batch, MFD_CMD_OP_H264, ...);
 *    mediaBatchAddEpilogue(&batch);
 *    mediaBatchSubmit(&batch, vcs0Ring);
 * ─────────────────────────────────────────────────────────────────
 */
struct MyIntelMediaBatchBuffer {
    uint32_t *buf;          /* Command buffer */
    uint32_t  size;         /* Max size in dwords */
    uint32_t  offset;       /* Current write offset (in dwords) */
};

static inline void mediaBatchBegin(MyIntelMediaBatchBuffer *b,
                                   uint32_t *buf, uint32_t sizeDwords)
{
    b->buf    = buf;
    b->size   = sizeDwords;
    b->offset = 0;
}

static inline void mediaBatchAddDword(MyIntelMediaBatchBuffer *b, uint32_t dw)
{
    if (b->offset < b->size)
        b->buf[b->offset++] = dw;
}

static inline void mediaBatchAddPair(MyIntelMediaBatchBuffer *b,
                                     uint32_t dw0, uint32_t dw1)
{
    mediaBatchAddDword(b, dw0);
    mediaBatchAddDword(b, dw1);
}

static inline void mediaBatchAddWait(MyIntelMediaBatchBuffer *b)
{
/* MFX_WAIT: MEDIA_INSTR already positions opcode at bits [31:27]. 
 * No << 16 shift needed — just OR the length field. */ 
mediaBatchAddPair(b, MFX_WAIT_CMD | (1 << 0), /* DW0: opcode + length */ 
                  MFX_WAIT_FOR_DECODE);       /* DW1: flags */
} 

static inline void mediaBatchAddSurfaceState(MyIntelMediaBatchBuffer *b,
                                              uint32_t surfOffset)
{
    mediaBatchAddPair(b,
        (MFX_SURFACE_STATE_CMD << 16) | (1 << 0),
        surfOffset);
}

static inline void mediaBatchAddEpilogue(MyIntelMediaBatchBuffer *b)
{
    /* MFX_VD_EPILOGUE_CMD: signals end of decode */
    mediaBatchAddPair(b,
        (MFX_VD_EPILOGUE_CMD << 16) | (1 << 0),
        0);
}

static inline void mediaBatchAddFlush(MyIntelMediaBatchBuffer *b)
{
    /* MFX_VD_FLUSH_CMD: flush pipeline */
    mediaBatchAddPair(b,
        (MFX_VD_FLUSH_CMD << 16) | (1 << 0),
        0);
}

/*
 * @brief Get total size in bytes for submission
 */
static inline uint32_t mediaBatchGetSize(const MyIntelMediaBatchBuffer *b)
{
    return b->offset * sizeof(uint32_t);
}

#endif /* __MY_INTEL_MEDIA_HPP__ */
