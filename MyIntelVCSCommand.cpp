#include "MyIntelVCSCommand.h"
#ifdef KERNEL
#include <IOKit/IOLib.h>
#else
/* user-space build (test app / libmyintelvcs.dylib) — kernel IOLib unavailable */
#include <stdio.h>
#include <stdlib.h>
#endif
#include <string.h>

/*
 * MFX Pipe Mode Select — H.264/AVC decode
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_PIPE_MODE_SELECT
 *   DWORD 0: 0x68000004 (opcode)
 *   DWORD 1: Stream-out mode, slice/chroma mode flags
 *   DWORD 2: Codec type, decode/encode mode
 *   DWORD 3: Frame/field mode, MBMV format
 *   DWORD 4: Reserved
 */
#define MFX_PIPE_MODE_SELECT_CMD    0x68000004

bool vcsCmdMfxPipeModeSelect(MyIntelVCSCmdBuf *buf, uint32_t codec, bool decode)
{
    uint32_t dwords[5] = {0};
    dwords[0] = MFX_PIPE_MODE_SELECT_CMD;

    /* DWORD 1: Stream-out disabled for decode, standard mode */
    dwords[1] = 0;

    /* DWORD 2: Codec type + decode mode */
    uint32_t codecVal = 0;
    switch (codec) {
        case 0x01: codecVal = 0; break;  /* H.264 = 0 */
        case 0x02: codecVal = 1; break;  /* HEVC = 1 */
        default:   codecVal = 0; break;
    }
    dwords[2] = (decode ? (1U << 16) : 0) | (codecVal & 0x7);

    /* DWORD 3: Frame mode, default MBMV */
    dwords[3] = 0;

    /* DWORD 4: Reserved */
    dwords[4] = 0;

    return vcsCmdEmitBatch(buf, dwords, 5);
}

/*
 * MFX Surface State — video surface configuration
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_SURFACE_STATE
 *   DWORD 0: 0x68000005 (opcode)
 *   DWORD 1: Surface index + width/height
 *   DWORD 2: Surface format + tiling mode
 *   DWORD 3: Pitch (bytes per row)
 */
#define MFX_SURFACE_STATE_CMD       0x68000005

bool vcsCmdMfxSurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                           uint32_t format, uint32_t ggttOffset)
{
    uint32_t dwords[4] = {0};
    dwords[0] = MFX_SURFACE_STATE_CMD;

    /* DWORD 1: Width (pixels - 1) in lower bits, surface index in upper */
    dwords[1] = ((width - 1) & 0xFFFF);

    /* DWORD 2: Surface format (NV12=1, P010=3) + tiling (linear=0, tileY=4) */
    uint32_t hwFormat = 1; /* NV12 default */
    if (format == 0x01) hwFormat = 3;  /* P010 */
    dwords[2] = (hwFormat & 0xF) | (4 << 16); /* tileY mode for Gen12 */

    /* DWORD 3: Pitch = width * bpp (for NV12: width) */
    dwords[3] = width;

    return vcsCmdEmitBatch(buf, dwords, 4);
}

/*
 * MFX Pipe Buffer Address State — buffer surface list
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_PIPE_BUF_ADDR_STATE
 *   DWORD 0: 0x68000002 (opcode)
 *   DWORD 1: Flags
 *   DWORD 2-9: Surface addresses (GGTT offsets, 8 surfaces)
 */
#define MFX_PIPE_BUF_ADDR_STATE_CMD 0x68000002

bool vcsCmdMfxPipeBufAddrState(MyIntelVCSCmdBuf *buf, uint32_t surfaces[8], uint32_t count)
{
    uint32_t dwords[10] = {0};
    dwords[0] = MFX_PIPE_BUF_ADDR_STATE_CMD;
    dwords[1] = 0;

    uint32_t n = count < 8 ? count : 8;
    for (uint32_t i = 0; i < n; i++) {
        dwords[2 + i] = surfaces[i];
    }

    return vcsCmdEmitBatch(buf, dwords, 10);
}

/*
 * MFX Indirect Object Base Address State — bitstream buffer
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_IND_OBJ_BASE_ADDR_STATE
 *   DWORD 0: 0x68000003 (opcode)
 *   DWORD 1-4: MFW/MBW/IA/MBC object base addresses
 *   DWORD 5-8: MFW/MBW/IA/MBC object upper addresses
 *   DWORD 9: Size
 */
#define MFX_IND_OBJ_BASE_ADDR_STATE_CMD 0x68000003

bool vcsCmdMfxIndObjBaseAddrState(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset, uint32_t size)
{
    uint32_t dwords[10] = {0};
    dwords[0] = MFX_IND_OBJ_BASE_ADDR_STATE_CMD;
    dwords[1] = 0;
    dwords[2] = ggttOffset;    /* IA object base (bitstream) */
    dwords[3] = 0;             /* MBW object base (MB data) */
    dwords[4] = 0;
    dwords[5] = 0;             /* Upper address bits */
    dwords[6] = 0;
    dwords[7] = 0;
    dwords[8] = 0;
    dwords[9] = size;          /* Object size */

    return vcsCmdEmitBatch(buf, dwords, 10);
}

/*
 * MFX Picture State — H.264/HEVC picture parameters
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_PIC_STATE
 *   DWORD 0: 0x68000008 (opcode)
 *   DWORD 1: Width/Height
 *   DWORD 2: Frame number / POC
 *   DWORD 3-4: Additional picture params
 */
#define MFX_PIC_STATE_CMD           0x68000008

bool vcsCmdMfxPicState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                       uint32_t picParamGgtt)
{
    uint32_t dwords[5] = {0};
    dwords[0] = MFX_PIC_STATE_CMD;
    dwords[1] = ((height & 0xFFFF) << 16) | (width & 0xFFFF);
    dwords[2] = 0;             /* Frame number = 0 for first frame */
    dwords[3] = picParamGgtt;  /* GGTT offset to picture parameter buffer */
    dwords[4] = 0;

    return vcsCmdEmitBatch(buf, dwords, 5);
}

/*
 * MFX Slice State — slice-level parameters
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_SLICE_STATE
 *   DWORD 0: 0x68000009 (opcode)
 *   DWORD 1: Slice count + MB offset
 *   DWORD 2: Slice MBA flags
 */
#define MFX_SLICE_STATE_CMD         0x68000009

bool vcsCmdMfxSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceCount)
{
    uint32_t dwords[3] = {0};
    dwords[0] = MFX_SLICE_STATE_CMD;
    dwords[1] = sliceCount;     /* Number of slices */
    dwords[2] = 0;

    return vcsCmdEmitBatch(buf, dwords, 3);
}

/*
 * MFX Reference Index State — reference frame mapping
 *
 * Reference: mhw_vdbox_mfx_hwcmd_g12_X.h — MFX_REF_IDX_STATE
 *   DWORD 0: 0x6800000A (opcode)
 *   DWORD 1: Reference index + frame number
 */
#define MFX_REF_IDX_STATE_CMD       0x6800000A

bool vcsCmdMfxRefIdxState(MyIntelVCSCmdBuf *buf, uint32_t frameNum, uint32_t picId)
{
    uint32_t dwords[2] = {0};
    dwords[0] = MFX_REF_IDX_STATE_CMD;
    dwords[1] = ((picId & 0xFFFF) << 16) | (frameNum & 0xFFFF);

    return vcsCmdEmitBatch(buf, dwords, 2);
}

/*
 * HCP Pipe Mode Select — HEVC-specific mode select
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_PIPE_MODE_SELECT
 *   DWORD 0: 0x68008004 (opcode, different from MFX)
 *   DWORD 1: Stream-out mode
 *   DWORD 2: Decode mode flags
 *   DWORD 3: Frame/field, chroma format
 */
#define HCP_PIPE_MODE_SELECT_CMD    0x68008004

bool vcsCmdHcpPipeModeSelect(MyIntelVCSCmdBuf *buf, bool decode, uint32_t codec)
{
    uint32_t dwords[4] = {0};
    dwords[0] = HCP_PIPE_MODE_SELECT_CMD;
    dwords[1] = 0;
    dwords[2] = decode ? (1U << 16) : 0;
    dwords[2] |= (codec & 0x1F);
    dwords[3] = 0;

    return vcsCmdEmitBatch(buf, dwords, 4);
}

/*
 * HCP Surface State — HEVC surface configuration
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_SURFACE_STATE
 *   DWORD 0: 0x68008005 (opcode)
 *   DWORD 1: Surface index + dimensions
 *   DWORD 2: Surface format + tiling
 *   DWORD 3: Pitch
 */
#define HCP_SURFACE_STATE_CMD       0x68008005

bool vcsCmdHcpSurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                           uint32_t format, uint32_t ggttOffset)
{
    uint32_t dwords[4] = {0};
    dwords[0] = HCP_SURFACE_STATE_CMD;
    dwords[1] = ((height & 0xFFFF) << 16) | (width & 0xFFFF);

    uint32_t hwFormat = 1;
    if (format == 0x01) hwFormat = 3;
    dwords[2] = (hwFormat & 0xF) | (4 << 16);

    dwords[3] = width;

    return vcsCmdEmitBatch(buf, dwords, 4);
}

/*
 * HCP Pipe Buffer Address State — HEVC buffer surface list
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_PIPE_BUF_ADDR_STATE
 *   DWORD 0: 0x68008002 (opcode)
 *   DWORD 1: Flags
 *   DWORD 2-9: Surface addresses (8 surfaces)
 */
#define HCP_PIPE_BUF_ADDR_STATE_CMD 0x68008002

bool vcsCmdHcpPipeBufAddrState(MyIntelVCSCmdBuf *buf, uint32_t surfaces[8], uint32_t count)
{
    uint32_t dwords[10] = {0};
    dwords[0] = HCP_PIPE_BUF_ADDR_STATE_CMD;
    dwords[1] = 0;

    uint32_t n = count < 8 ? count : 8;
    for (uint32_t i = 0; i < n; i++) {
        dwords[2 + i] = surfaces[i];
    }

    return vcsCmdEmitBatch(buf, dwords, 10);
}

/*
 * HCP Indirect Object Base Address State — HEVC bitstream buffer
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_IND_OBJ_BASE_ADDR_STATE
 *   DWORD 0: 0x68008003 (opcode)
 *   DWORD 1-4: Object base addresses
 *   DWORD 5-8: Upper addresses
 *   DWORD 9: Size
 */
#define HCP_IND_OBJ_BASE_ADDR_STATE_CMD 0x68008003

bool vcsCmdHcpIndObjBaseAddrState(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset, uint32_t size)
{
    uint32_t dwords[10] = {0};
    dwords[0] = HCP_IND_OBJ_BASE_ADDR_STATE_CMD;
    dwords[1] = 0;
    dwords[2] = ggttOffset;
    dwords[3] = 0;
    dwords[4] = 0;
    dwords[5] = 0;
    dwords[6] = 0;
    dwords[7] = 0;
    dwords[8] = 0;
    dwords[9] = size;

    return vcsCmdEmitBatch(buf, dwords, 10);
}

/*
 * HCP Picture State — HEVC picture parameters
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_PIC_STATE
 *   DWORD 0: 0x68008008 (opcode)
 *   DWORD 1: Width/Height
 *   DWORD 2: Frame number / POC
 *   DWORD 3: Pic param GGTT offset
 *   DWORD 4: SPS/PPS GGTT offset
 */
#define HCP_PIC_STATE_CMD           0x68008008

bool vcsCmdHcpPicState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                       uint32_t picParamGgtt)
{
    uint32_t dwords[5] = {0};
    dwords[0] = HCP_PIC_STATE_CMD;
    dwords[1] = ((height & 0xFFFF) << 16) | (width & 0xFFFF);
    dwords[2] = 0;
    dwords[3] = picParamGgtt;
    dwords[4] = 0;

    return vcsCmdEmitBatch(buf, dwords, 5);
}

/*
 * HCP Slice State — HEVC slice parameters
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_SLICE_STATE
 *   DWORD 0: 0x68008009 (opcode)
 *   DWORD 1: Slice count
 *   DWORD 2: Slice CABAC/cabac alignment
 */
#define HCP_SLICE_STATE_CMD         0x68008009

bool vcsCmdHcpSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceCount)
{
    uint32_t dwords[3] = {0};
    dwords[0] = HCP_SLICE_STATE_CMD;
    dwords[1] = sliceCount;
    dwords[2] = 0;

    return vcsCmdEmitBatch(buf, dwords, 3);
}

/*
 * HCP Reference Index State — HEVC reference frame mapping
 *
 * Reference: mhw_vdbox_hcp_hwcmd_g12_X.h — HCP_REF_IDX_STATE
 *   DWORD 0: 0x6800800A (opcode)
 *   DWORD 1: Reference index + frame number
 */
#define HCP_REF_IDX_STATE_CMD       0x6800800A

bool vcsCmdHcpRefIdxState(MyIntelVCSCmdBuf *buf, uint32_t frameNum, uint32_t picId)
{
    uint32_t dwords[2] = {0};
    dwords[0] = HCP_REF_IDX_STATE_CMD;
    dwords[1] = ((picId & 0xFFFF) << 16) | (frameNum & 0xFFFF);

    return vcsCmdEmitBatch(buf, dwords, 2);
}

/*
 * ─────────────────────────────────────
 *  Phase 10 — Gen12-accurate MFX AVC decode sequence
 * ─────────────────────────────────────
 *  DW0 opcodes/dwSize อ้างอิง mhw_vdbox_mfx_hwcmd_g12_X.h
 *  (คำนวณจาก codegen enums — ไม่ copy โค้ด Intel)
 *  p4b: แก้ field encoding ตาม layout จริง (สาเหตุ readback=0)
 */

static bool vcsCmdG12MfxWait(MyIntelVCSCmdBuf *buf)
{
    /* DW0: len=0, SubOpcode=0<<16, subtype MFXSINGLEDW=1<<27, type=3<<29 */
    return vcsCmdEmit(buf, 0x68000000u);
}

/* MFX_WAIT — single-dword command; Gen12 ต้องมีทั้งก่อนและหลัง PIPE_MODE_SELECT */

bool vcsCmdG12PipeModeSelect(MyIntelVCSCmdBuf *buf, bool hevc)
{
    if (!vcsCmdG12MfxWait(buf)) return false;
    uint32_t d[5] = {0};
    d[0] = 0x70000003;   /* len=3 → 5 dwords total */
    /* DW1: StandardSelect=AVC(2) bits0-3, CodecSelect=decode(0) bit4,
     *      Pre/PostDeblockOutEnable bits8-9, PicErrorStatusReportEnable bit11,
     *      DecoderModeSelect=VLD(0) bits15-16,
     *      DecoderShortFormatMode=1 (long format) bit17 */
    d[1] = (hevc ? 0u : 2u) | (1u << 9) | (1u << 11) | (1u << 17);
    /* DECODER_SHORT_FORMAT_MODE (bit17): ชื่อฟิลด์ INVERTED กับความหมายจริง!
     * media-driver G12 AddMfxPipeModeSelectCmd เขียน DecoderShortFormatMode =
     * !bShortFormatInUse; codechal_decode_avc ตั้ง bShortFormatInUse=false
     * สำหรับ AVC VLD long format → driver เขียน 1 (comment ฝั่ง encode ยืนยัน:
     * "This bit is set to be long format"). เรา emit long-format commands
     * (MFX_AVC_SLICE_STATE + MFD_AVC_BSD_OBJECT, ไม่มี DPB_STATE) → bit17=1.
     * (ทดสอบค่า 1 ครั้งก่อนพังเพราะ bug headerBytes ยังค้าง — confounded) */
    d[3] = 0;                                  /* DW3: media-driver leaves 0 for decode */
    return vcsCmdEmitBatch(buf, d, 5) && vcsCmdG12MfxWait(buf);
}

bool vcsCmdG12SurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                           uint32_t pitch)
{
    return vcsCmdG12SurfaceStateFmt(buf, width, height, pitch, 4u); /* NV12 */
}

bool vcsCmdG12SurfaceStateFmt(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                              uint32_t pitch, uint32_t surfFmt)
{
    /* 6 dwords — decoded picture surface (NV12 / P010, linear or TileY) */
    uint32_t d[6] = {0};
    d[0] = 0x70010004;   /* len=4 → 6 dwords total */
    d[1] = 0;                                  /* SurfaceId=0 decoded+ref */
    d[2] = (((width - 1) & 0x3FFF) << 4) | (((height - 1) & 0x3FFF) << 18);
    d[3] = (1u << 0)                     |          /* TileWalk=Y (MUST 1 per spec) */
           (1u << 1)                     |          /* TiledSurface=1 (MFX ignores but spec says always 1) [P15] */
           (0u << 2)                     |          /* HalfPitchForChroma=0 */
           (((pitch - 1) & 0x1FFFF) << 3) |         /* SurfacePitch = pitch-1 */
           (1u << 27)                    |          /* InterleaveChroma (NV12/P010) */
           ((surfFmt & 0xF) << 28);                 /* SurfaceFormat: NV12=4, P010=9 */
    d[4] = height & 0x7FFF;                         /* YOffsetForUCb */
    d[5] = height & 0xFFFF;                         /* YOffsetForVCr */
    return vcsCmdEmitBatch(buf, d, 6);
}

bool vcsCmdG12PipeBufAddr(MyIntelVCSCmdBuf *buf, uint32_t dstGgtt,
                          uint32_t scratchGgtt, uint32_t refGgtt)
{
    /* 68 dwords
     * Attribute DWs ของ buffer ที่ใช้จริงต้องมี MOCS index — Gen11+ PTE
     * ไม่มี cache bits (MOCS คุมเดี่ยว); i915 gen12_mocs_table index 2 =
     * WB L3+LLC. ช่องที่ไม่มี buffer/feature ปิด (orig pic, streamout,
     * ILDB×2, scaled ref, slice-size streamout) driver ปล่อย 0 เช่นกัน */
    const uint32_t kMocsWb = 2;
    uint32_t d[68];
    memset(d, 0, sizeof(d));
    d[0] = 0x70020042;   /* len=66 → 68 dwords total */
    /* address fields = bits6-31 → addr>>6 (MHW_VDBOX_MFX_GENERAL_STATE_SHIFT=6,
     * ใช้กับ refs ด้วย — resourceParams.dwLsbNum ตั้งรวมทั้ง command) */
    /* [P5] MFX_NODEBPRE=1: deblock ON → pre-deb output unused, driver leaves
     * it NULL; pointing it at dst risks cross-stage write contention */
#ifdef KERNEL
    const int nodebpre = 0;   /* env gates are user-space test-only */
#else
    const int nodebpre = getenv("MFX_NODEBPRE") != NULL;
#endif
    d[1]  = nodebpre ? 0 : (dstGgtt & ~0x3Fu);   /* DW1  Pre-deblock output [P14: in-place bits6-31] */
    d[3]  = nodebpre ? 0 : kMocsWb;                     /* DW3  Pre-deblock attributes */
    d[4]  = dstGgtt & ~0x3Fu;                      /* DW4  Post-deblock output [P14] */
    d[6]  = kMocsWb;                               /* DW6  Post-deblock attributes */
    /* DW7 original picture = n/a สำหรับ decode */
    /* DW10 streamout = ไม่มี */
    d[13] = scratchGgtt & ~0x3Fu;                  /* DW13 Intra row store scratch [P14] */
    d[15] = kMocsWb;                               /* DW15 Intra RS attributes */
    d[16] = (scratchGgtt + 0x40000) & ~0x3Fu;      /* DW16 Deblock row store scratch [P14] */
    d[18] = kMocsWb;                               /* DW18 Deblock RS attributes */
    /* DW19..DW50 = ReferencePicture0..15 (2 dwords ต่อรูป) — IDR ไม่มี ref จริง
     * แต่ใส่ address ที่ valid ไว้ให้ครบ */
    for (int i = 0; i < 16; i++) {
        d[19 + i * 2] = refGgtt ? (refGgtt & ~0x3Fu) : 0;   /* [P14 in-place] */
    }
    d[51] = kMocsWb;                               /* DW51 Ref picture MOCS (ตัวเดียวรวม 16 ref) */
    d[52] = (scratchGgtt + 0x80000) & ~0x3Fu;      /* DW52 MB error/status buffer [P14] */
    d[54] = kMocsWb;                               /* DW54 MB status attributes */
    return vcsCmdEmitBatch(buf, d, 68);
}

/*
 * DPB sanity: every reference GVA must be 64-byte aligned (bits 5..0 == 0).
 * Returns number of offending slots; 0 = all clear.
 */
int vcsDpbSanityCheck(const uint32_t refs[16])
{
    int bad = 0;
    for (int i = 0; i < 16; i++) {
        if (refs[i] & 0x3F) bad++;
    }
    return bad;
}

/* DPB variant: caller supplies 16 pre-masked, pre-aligned reference GGTT addresses.
 * Same DW layout as vcsCmdG12PipeBufAddr — only the ReferencePicture array differs. */
bool vcsCmdG12PipeBufAddrRefs(MyIntelVCSCmdBuf *buf, uint32_t dstGgtt,
                              uint32_t scratchGgtt, const uint32_t refs[16])
{
    const uint32_t kMocsWb = 2;
    uint32_t d[68];
    memset(d, 0, sizeof(d));
    d[0] = 0x70020042;
#ifdef KERNEL
    const int nodebpre = 0;
#else
    const int nodebpre = getenv("MFX_NODEBPRE") != NULL;
#endif
    d[1]  = nodebpre ? 0 : (dstGgtt & ~0x3Fu);
    d[3]  = nodebpre ? 0 : kMocsWb;
    d[4]  = dstGgtt & ~0x3Fu;
    d[6]  = kMocsWb;
    d[13] = scratchGgtt & ~0x3Fu;
    d[15] = kMocsWb;
    d[16] = (scratchGgtt + 0x40000) & ~0x3Fu;
    d[18] = kMocsWb;
    for (int i = 0; i < 16; i++) {
        d[19 + i * 2] = refs[i] & ~0x3Fu;   /* enforce 64B align at emit time too */
    }
    d[51] = kMocsWb;
    d[52] = (scratchGgtt + 0x80000) & ~0x3Fu;
    d[54] = kMocsWb;
    return vcsCmdEmitBatch(buf, d, 68);
}

bool vcsCmdG12IndObj(MyIntelVCSCmdBuf *buf, uint32_t ggtt, uint32_t size,
                     uint32_t mvGgtt, uint32_t mvSize)
{
    /* 26 dwords — bitstream (IA) + indirect MV objects
     * field เป็น bits12-31 → shift >>12 (4KB granular)
     * DW layout: IA base/high/MOCS/upperBound/upperHigh = DW1-5;
     *            MV base/high/MOCS/upperBound/upperHigh = DW6-10
     * (IT-Coeff DW11-15 / IT-Dblk DW16-20 ใช้เฉพาะ IT mode,
     *  PAK-BSE DW21-25 เฉพาะ encode → 0 ทั้งหมดตาม driver VLD path) */
    const uint32_t kMocsWb = 2;
    uint32_t d[26];
    memset(d, 0, sizeof(d));
    d[0] = 0x70030018;         /* len=24 → 26 dwords total */
    d[1] = ggtt & ~0xFFFu;                 /* IA object base bits12-31 [P14 in-place] */
    /* upper bound = base+size ปัดขึ้น 4KB, เก็บที่ DW4 (bits12-31) */
    d[4] = (ggtt + ((size + 0xFFFu) & ~0xFFFu)) & ~0xFFFu;
    d[3] = kMocsWb;                        /* IA attributes: MOCS bits0-6 */
    d[6] = mvGgtt & ~0xFFFu;               /* MV object base [P14] */
    d[8] = kMocsWb;                        /* MV attributes */
    d[9] = (mvGgtt + ((mvSize + 0xFFFu) & ~0xFFFu)) & ~0xFFFu; /* MV upper bound [P14] */
    return vcsCmdEmitBatch(buf, d, 26);
}

/*
 * MFX_BSP_BUF_BASE_ADDR_STATE — 10 dwords (dwSize=10, SubOpcodeB=4)
 * driver ส่งหลัง IND_OBJ ทุกเฟรม decode: BSD/MPC row store + MPR row store
 * scratch (ขนาด picWidthInMB * 2 * 64 ต่ออัน — เทียบ AllocateVariableResources)
 * ถ้าขาดคำสั่งนี้ VDBOX ไม่รู้จะพัก intermediate data ที่ไหน → decode ไม่ออก
 */
bool vcsCmdG12BspBufBaseAddr(MyIntelVCSCmdBuf *buf, uint32_t bsdMpcGgtt,
                             uint32_t mprGgtt)
{
    const uint32_t kMocsWb = 2;
    uint32_t d[10];
    memset(d, 0, sizeof(d));
    d[0] = 0x70040008;                       /* len=8 → 10 dwords, SubOpB=4, pipeline=2, type=3 */
    d[1] = bsdMpcGgtt & ~0x3Fu;              /* DW1 BSD/MPC row store [P14 in-place] */
    d[3] = kMocsWb;                          /* DW3 BSD/MPC attributes */
    d[4] = mprGgtt & ~0x3Fu;                 /* DW4 MPR row store [P14] */
    d[6] = kMocsWb;                          /* DW6 MPR attributes */
    return vcsCmdEmitBatch(buf, d, 10);
}

/*
 * MFX_QM_STATE — 18 dwords ต่อคำสั่ง (dwSize=18, SubOpcodeB=7); AVC decode
 * ต้องส่ง 4 ตัวตาม driver (AddMfxQmCmd): intra4x4 / inter4x4 / intra8x8 / inter8x8
 * ใช้ default matrix ของ H.264 spec (stream ไม่มี custom scaling list)
 */
static const uint8_t sG12QmIntra8x8[64] = {
     6,10,13,16,18,23,25,27, 10,11,16,18,23,25,27,29,
    13,16,18,23,25,27,29,31, 16,18,23,25,27,29,31,33,
    18,23,25,27,29,31,33,36, 23,25,27,29,31,33,36,38,
    25,27,29,31,33,36,38,40, 27,29,31,33,36,38,40,42,
};
static const uint8_t sG12QmInter8x8[64] = {
     9,13,15,17,19,21,22,24, 13,13,17,19,21,22,24,25,
    15,17,19,21,22,24,25,27, 17,19,21,22,24,25,27,28,
    19,21,22,24,25,27,28,30, 21,22,24,25,27,28,30,32,
    22,24,25,27,28,30,32,33, 24,25,27,28,30,32,33,35,
};

bool vcsCmdG12QmState(MyIntelVCSCmdBuf *buf)
{
    uint32_t d[18];

    /* 4x4: 3 lists × 16 bytes (Y/Cb/Cr) — default H.264 = flat 16 */
    for (uint32_t type = 0; type <= 1; type++) {
        memset(d, 0, sizeof(d));
        d[0] = 0x70070010;                 /* len=16 → 18 dwords, SubOpB=7, pipeline=2, type=3 */
        d[1] = type;                       /* 0=intra4x4, 1=inter4x4 */
        uint8_t *m = (uint8_t *)&d[2];
        for (int i = 0; i < 48; i++) m[i] = 16;    /* 3 lists × 16 */
        if (!vcsCmdEmitBatch(buf, d, 18)) return false;
    }

    /* 8x8: 64 bytes ต่อคำสั่ง */
#ifdef KERNEL
    bool qmFlat = false;
#else
    bool qmFlat = getenv("MFX_QM_FLAT") != NULL;
#endif
    for (uint32_t type = 2; type <= 3; type++) {
        memset(d, 0, sizeof(d));
        d[0] = 0x70070010;                 /* len=16 → 18 dwords */
        d[1] = type;                       /* 2=intra8x8, 3=inter8x8 */
        if (qmFlat) {
            memset(&d[2], 16, 64);
        } else {
            const uint8_t *src = (type == 2) ? sG12QmIntra8x8 : sG12QmInter8x8;
            memcpy(&d[2], src, 64);
        }
        if (!vcsCmdEmitBatch(buf, d, 18)) return false;
    }
    return true;
}

/*
 * MFD_AVC_PICID_STATE — 2 dwords (เทียบ AddMfdAvcPicidCmd)
 * driver ส่งก่อน AVC_IMG_STATE ทุกเฟรม — remapping ปิดสำหรับ decode ปกติ
 */
bool vcsCmdG12AvcPicIdState(MyIntelVCSCmdBuf *buf)
{
    /* 10 dwords (dwSize=10): DW1 remapping flag + DW2-9 PictureIDList (ไม่ใช้,
     * remapping ปิด). Gen12 opcode = 0x71250009 (SubB=5, SubA=1, Op=1, len=9). */
    uint32_t d[10];
    memset(d, 0, sizeof(d));
    d[0] = 0x71250008;   /* len=8 → 10 dwords total */
    d[1] = (1u << 0);              /* PictureIdRemappingDisable=1 */
    return vcsCmdEmitBatch(buf, d, 10);
}

bool vcsCmdG12AvcImgState(MyIntelVCSCmdBuf *buf,
                          uint32_t widthMb, uint32_t heightMb,
                          uint32_t entropy, uint32_t transform8x8,
                          uint32_t direct8x8, uint32_t chromaIdc,
                          uint32_t log2MaxFrameNumM4, uint32_t pocType,
                          uint32_t log2MaxPocLsbM4, uint32_t picOrderPresent,
                          uint32_t deltaPocAlwaysZero,
                          int32_t  picInitQp, uint32_t frameNum,
                          uint32_t numRefFrames, uint32_t numRefL0Active,
                          uint32_t numRefL1Active, uint32_t deblockCtrlPresent)
{
    /* 21 dwords */
    uint32_t d[21];
    memset(d, 0, sizeof(d));
    d[0] = 0x71000013;   /* len=19 → 21 dwords total */
    d[1] = (widthMb * heightMb) & 0xFFFF;                 /* FrameSize (MB) */
    d[2] = ((widthMb - 1) & 0xFF) | (((heightMb - 1) & 0xFF) << 16);
    d[4] = (0u << 0)                          |           /* fieldpicflag=0 */
           (0u << 1)                          |           /* mbaff */
           (1u << 2)                          |           /* framembonlyflag */
           ((transform8x8 & 1) << 3)          |
           ((direct8x8 & 1) << 4)             |
           (0u << 5)                          |           /* constrained ipred */
           (0u << 6)                          |           /* img disposable: IDR = reference -> 0 (driver !ref_pic_flag) */
           ((entropy & 1) << 7)               |           /* entropy coding */
           ((chromaIdc & 3) << 10);
    d[13] = ((uint32_t)picInitQp & 0xFF) |          /* InitialQp bits0-7 */
            ((numRefL0Active & 0x3F) << 8) |         /* NumActiveRefL0 bits8-13 */
            ((numRefL1Active & 0x3F) << 16) |       /* NumActiveRefL1 bits16-21 */
            ((numRefFrames & 0x1F) << 24);          /* NumRefFrames bits24-28 */
    d[14] = (picOrderPresent & 1)             |
            ((deltaPocAlwaysZero & 1) << 1)   |
            ((pocType & 3) << 2)              |
            (((deblockCtrlPresent & 1)) << 15) |  /* DeblockingFilterControlPresentFlag ตาม PPS จริง */
            ((log2MaxFrameNumM4 & 0xFF) << 16) |
            ((log2MaxPocLsbM4 & 0xFF) << 24);
    d[15] = (frameNum & 0xFFFF) << 16;                    /* CurrPicFrameNum */
    /* DW5 bit27: TrellisQuantizationChromaDisable — driver sets TRUE unconditionally */
    d[5] |= (1u << 27);
    return vcsCmdEmitBatch(buf, d, 21);
}

/*
 * MFX_AVC_REF_IDX_STATE — dummy reference สำหรับ I-Frame (10 dwords)
 * driver (AddMfxAvcRefIdx): "Need to add an empty MFX_AVC_REF_IDX_STATE_CMD
 * for dummy reference on I-Frame" → header อย่างเดียว ฟิลด์ภายในศูนย์ทั้งหมด,
 * RefpiclistSelect=0 (list0)
 */
bool vcsCmdG12AvcRefIdxDummy(MyIntelVCSCmdBuf *buf)
{
    uint32_t d[10];
    memset(d, 0, sizeof(d));
    d[0] = 0x71040008;   /* len=8 → 10 dwords, SubOpB=4, SubOpA=0, Op=1(AVC) */
    return vcsCmdEmitBatch(buf, d, 10);
}

bool vcsCmdG12AvcSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceType,
                            uint32_t startMb, uint32_t widthMb,
                            uint32_t heightMb,
                            uint32_t sliceQp, uint32_t cabacInitIdc,
                            uint32_t disableDeblock, int32_t alphaDiv2, int32_t betaDiv2,
                            uint32_t numRefL0Count, uint32_t numRefL1Count,
                            uint32_t nextFirstMb, bool lastSlice)
{
    /* 11 dwords
     * Fix 2026-08-21 เทียบ media-driver mhw_vdbox_mfx_generic.h AddMfxDecodeAvcSlice:
     *  - DW5 NextSlice: last slice → V=frameFieldHeightInMb, H=0 (เดิมเราปล่อย 0/0
     *    = ช่วง MB ที่ decode ว่างเปล่า → VDBOX เขียนแค่ status 4 bytes!)
     *  - DW9 Roundintra=5/ena=1, Roundinter=2 (driver เขียนเสมอ, เดิมเรา 0)
     *  - DW2 NumRef: I-slice = 0; P/B = count (minus1+1) — H.264 slice_type map
     *    {P,B,I,P,I,P,B,I,P,I} เหมือน AvcBsdSliceType[] */
    static const uint32_t sBsdSliceType[10] = {0,1,2,0,2,0,1,2,0,2};
    uint32_t st = sBsdSliceType[sliceType % 10];
    uint32_t d[11];
    memset(d, 0, sizeof(d));
    d[0] = 0x71030009;   /* len=9 → 11 dwords total */
    d[1] = st & 0xF;
    if (st == 0)          /* P-slice */
        d[2] = (numRefL0Count & 0x3F) << 16;
    else if (st == 1)     /* B-slice */
        d[2] = ((numRefL0Count & 0x3F) << 16) | ((numRefL1Count & 0x3F) << 24);
    /* I-slice → d[2] = 0 ตาม driver */
    d[3] = ((uint32_t)(alphaDiv2 & 0xF))        |
           ((uint32_t)(betaDiv2 & 0xF) << 8)    |
           ((sliceQp & 0x3F) << 16)             |
           ((cabacInitIdc & 3) << 24)           |
           ((disableDeblock & 3) << 27);
    d[4] = (startMb & 0x7FFF)                   |
           (((startMb % widthMb) & 0xFF) << 16) |
           (((startMb / widthMb) & 0xFF) << 24);
    if (lastSlice)
        d[5] = (heightMb & 0x1FF) << 16;         /* NextSliceV=frameHeightMB, H=0 */
    else
        d[5] = ((nextFirstMb / widthMb) & 0x1FF) << 16 |
               ((nextFirstMb % widthMb) & 0x1FF);/* NextSlice V<<16 | H bits0-8 */
    d[6] = lastSlice ? (1u << 19) : 0;          /* IsLastSlice */
    d[9] = (5u << 24) | (1u << 27) | (2u << 28); /* Roundintra=5 ena=1 Roundinter=2 */
    return vcsCmdEmitBatch(buf, d, 11);
}

bool vcsCmdG12BsdObject(MyIntelVCSCmdBuf *buf, uint32_t sliceOffset,
                        uint32_t sliceSize, uint32_t headerBytes,
                        uint32_t dataBitOffset, uint32_t nalType, bool lastSlice)
{
    (void)nalType;
    /* 7 dwords */
    uint32_t d[7];
    memset(d, 0, sizeof(d));
    d[0] = 0x71280005;   /* len=5 → 7 dwords total */
    d[1] = sliceSize;                            /* Indirect BSD Data Length */
    d[2] = sliceOffset & 0x1FFFFFFF;             /* start address (offset จาก indirect base) */
    d[3] = (1u << 29);                           /* IntraPredmode 4x4/8x8 luma error control */
    d[4] = (dataBitOffset & 7)                   |   /* First MB bit offset */
           (lastSlice ? (1u << 3) : 0)           |   /* LASTSLICE_FLAG */
           (1u << 7)                             |   /* FixPrevMbSkipped */
           ((headerBytes & 0xFFFF) << 16);           /* First MB byte offset of slice data */
    /* EPB bit4 = 0 ตาม PRM + Xe_LPM_plus driver: 0 = "H/W needs to perform
     * Emulation Byte Removal" (buffer ดิบยังมี 00 00 03 → ต้องให้ HW strip)
     * 1 = ข้อมูลสะอาดแล้วไม่ต้อง strip — เคยตั้ง 1 แล้ว CABAC พังเพราะกิน EPB เป็นข้อมูล */
    d[5] = (1u << 0) | (1u << 1) | (1u << 31);   /* intra concealment control bits */
    d[6] = 0;   /* DW6: driver decode เขียน 0 — NAL type override ทำให้ parser เชื่อ
                 * byte ที่เราป้อนแทนบิตสตรีมจริง */
    return vcsCmdEmitBatch(buf, d, 7);
}

bool vcsCmdG12DirectMode(MyIntelVCSCmdBuf *buf, uint32_t mvScratchGgtt,
                         uint32_t temporalGgtt)
{
    /* 71 dwords — direct MV buffers: DW1..DW32 read (16 refs, 2dw ต่ออัน),
     * DW33 MOCS ของ read, DW34-35 = write buffer (bits6-31), DW36 MOCS,
     * DW37..DW70 = POC list
     * IDR ไม่มี ref → read entries ชี้ buffer เดียวกับ write (valid address)
     * MOCS index 2 = WB L3+LLC เหมือน buffer อื่น */
    const uint32_t kMocsWb = 2;
    uint32_t d[71];
    memset(d, 0, sizeof(d));
    d[0] = 0x71020045;   /* len=69 → 71 dwords total */
    for (int i = 0; i < 16; i++) {
        d[1 + i * 2] = mvScratchGgtt & ~0x3Fu;          /* DW1..DW32 read buffers [P14] */
    }
    d[33] = kMocsWb;                          /* read attributes */
    d[34] = (temporalGgtt ? temporalGgtt : mvScratchGgtt) & ~0x3Fu;  /* write buffer [P14 in-place] */
    d[36] = kMocsWb;                          /* write attributes */
#ifndef KERNEL
    if (temporalGgtt && temporalGgtt != mvScratchGgtt)
        printf("[P14] DMVSEP: read=0x%X write=0x%X\n", mvScratchGgtt, temporalGgtt);
#endif
    return vcsCmdEmitBatch(buf, d, 71);
}

/*
 * ─────────────────────────────────────
 *  Generic command buffer helpers
 * ─────────────────────────────────────
 */

bool vcsCmdInit(MyIntelVCSCmdBuf *buf, uint32_t codec)
{
    if (!buf) return false;
    buf->count = 0;
    buf->codec = codec;
    return true;
}

bool vcsCmdEmit(MyIntelVCSCmdBuf *buf, uint32_t dword)
{
    if (!buf || buf->count >= VCS_CMD_MAX_DWORDS) return false;
    buf->cmds[buf->count++] = dword;
    return true;
}

bool vcsCmdEmitBatch(MyIntelVCSCmdBuf *buf, const uint32_t *dwords, uint32_t count)
{
    if (!buf || !dwords) return false;
    if (buf->count + count > VCS_CMD_MAX_DWORDS) return false;

    for (uint32_t i = 0; i < count; i++) {
        buf->cmds[buf->count++] = dwords[i];
    }
    return true;
}

bool vcsCmdBatchStart(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset)
{
    uint32_t cmd = 0x18800000;  /* MI_BATCH_BUFFER_START */
    cmd |= (1U << 6);          /* GGTT bit */
    return vcsCmdEmit(buf, cmd) && vcsCmdEmit(buf, ggttOffset);
}

bool vcsCmdBatchEnd(MyIntelVCSCmdBuf *buf)
{
    return vcsCmdEmit(buf, 0x05000000);  /* MI_BATCH_BUFFER_END */
}

bool vcsCmdFlushMedia(MyIntelVCSCmdBuf *buf)
{
    uint32_t dwords[4] = {0};
    dwords[0] = 0x13000002;  /* MI_FLUSH_DW len=2 → 4 dwords total */
    dwords[1] = (7U << 24) | (1U << 1);  /* store_data_index=7, MEDIA_FLUSH */
    dwords[2] = 0;
    dwords[3] = 0;
    return vcsCmdEmitBatch(buf, dwords, 4);
}

bool vcsCmdWaitMedia(MyIntelVCSCmdBuf *buf)
{
    return vcsCmdEmit(buf, 0x15000001);  /* MFX_WAIT | EN */
}
