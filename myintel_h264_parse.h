/*===========================================================================
 *  myintel_h264_parse.h — H.264/AVC bitstream parser (header-only)
 *
 *  สร้าง 2026-08-21 — Phase 10: Full MFX Decode
 *
 *  Parse SPS / PPS / slice header จาก Annex-B bitstream จริง
 *  เพื่อป้อนค่าที่ถูกต้องให้ MFX_AVC_IMG_STATE / MFX_AVC_SLICE_STATE /
 *  MFD_AVC_BSD_OBJECT (แทนค่า template ของ Phase 9)
 *
 *  รองรับเฉพาะส่วนที่จำเป็นสำหรับ decode:
 *    - SPS: ขนาดภาพ, profile, log2_max_frame_num, POC type, ref frames,
 *           frame_mbs_only, direct_8x8, cropping
 *    - PPS: entropy mode, pic_order_present, weighted_pred, deblock ctrl,
 *           pic_init_qp, chroma qp offsets, **scaling_list**
 *    - slice header: first_mb, slice_type, frame_num, POC lsb, IDR marking,
 *           qp_delta, deblock, cabac_init_idc
 *
 *  Pure C, ไม่มี kernel deps — ใช้ได้ทั้ง test app และ dylib
 *=========================================================================*/

#ifndef __MYINTEL_H264_PARSE_H__
#define __MYINTEL_H264_PARSE_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── bit reader (MSB-first, ตาม H.264 spec) ─────────────────────── */
typedef struct {
    const uint8_t *p;
    uint32_t       len;
    uint32_t       bytePos;
    int            bitPos;   /* 7..0 */
} H264BitReader;

static inline void h264_br_init(H264BitReader *br, const uint8_t *data, uint32_t len)
{
    br->p = data; br->len = len; br->bytePos = 0; br->bitPos = 7;
}

static inline int h264_br_get(H264BitReader *br)
{
    if (br->bytePos >= br->len) return 0;
    int bit = (br->p[br->bytePos] >> br->bitPos) & 1;
    if (--br->bitPos < 0) { br->bitPos = 7; br->bytePos++; }
    return bit;
}

static inline uint32_t h264_br_u(H264BitReader *br, int n)
{
    uint32_t v = 0;
    for (int i = 0; i < n; i++) v = (v << 1) | (uint32_t)h264_br_get(br);
    return v;
}

/* exp-golomb unsigned (ue(v)) */
static inline uint32_t h264_br_ue(H264BitReader *br)
{
    int zeros = 0;
    while (zeros < 32 && h264_br_get(br) == 0) zeros++;
    if (zeros == 0) return 0;
    return (1u << zeros) - 1 + h264_br_u(br, zeros);
}

/* exp-golomb signed (se(v)) */
static inline int32_t h264_br_se(H264BitReader *br)
{
    uint32_t v = h264_br_ue(br);
    return (v & 1) ? (int32_t)((v + 1) >> 1) : -(int32_t)(v >> 1);
}

/* ยังมี bit เหลือไหม */
static inline bool h264_br_more(H264BitReader *br)
{
    return br->bytePos < br->len;
}

/* ── RBSP extract (ถอด emulation prevention 0x03) ────────────────── */
static inline uint32_t h264_rbsp(const uint8_t *nal, uint32_t nalLen,
                                 uint8_t *out, uint32_t outCap)
{
    uint32_t o = 0, z = 0;
    for (uint32_t i = 0; i < nalLen && o < outCap; i++) {
        if (z == 2 && nal[i] == 0x03) { z = 0; continue; }
        out[o++] = nal[i];
        z = (nal[i] == 0x00) ? z + 1 : 0;
    }
    return o;
}

/* ── SPS ────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t profileIdc, levelIdc, spsId;
    uint32_t chromaFormatIdc;
    uint32_t log2MaxFrameNumMinus4;
    uint32_t picOrderCntType;
    uint32_t log2MaxPicOrderCntLsbMinus4;
    uint32_t deltaPicOrderAlwaysZero;
    uint32_t maxNumRefFrames;
    uint32_t frameMbsOnly;
    uint32_t direct8x8Inference;
    uint32_t mbAdaptive;
    uint32_t width, height;          /* หลัง crop แล้ว */
    uint32_t picWidthInMbs, picHeightInMbs;
    bool     valid;
} H264SPS;

static inline bool h264_parse_sps(const uint8_t *rbsp, uint32_t len, H264SPS *s)
{
    H264BitReader br;
    memset(s, 0, sizeof(*s));
    h264_br_init(&br, rbsp + 1, len - 1);   /* ข้าม nal_header byte */

    s->profileIdc = h264_br_u(&br, 8);
    h264_br_u(&br, 8);                       /* constraint flags + reserved */
    s->levelIdc = h264_br_u(&br, 8);
    s->spsId = h264_br_ue(&br);

    s->chromaFormatIdc = 1;                  /* default 4:2:0 */
    if (s->profileIdc == 100 || s->profileIdc == 110 || s->profileIdc == 122 ||
        s->profileIdc == 244 || s->profileIdc == 44  || s->profileIdc == 83  ||
        s->profileIdc == 86  || s->profileIdc == 118 || s->profileIdc == 128 ||
        s->profileIdc == 138 || s->profileIdc == 139 || s->profileIdc == 134) {
        s->chromaFormatIdc = h264_br_ue(&br);
        if (s->chromaFormatIdc == 3) h264_br_get(&br);   /* separate_colour_plane */
        h264_br_ue(&br);                     /* bit_depth_luma_minus8 */
        h264_br_ue(&br);                     /* bit_depth_chroma_minus8 */
        h264_br_get(&br);                    /* qpprime_y_zero_transform_bypass */
        if (h264_br_get(&br)) {              /* seq_scaling_matrix_present */
            int n = (s->chromaFormatIdc != 3) ? 8 : 12;
            for (int i = 0; i < n; i++) {
                if (h264_br_get(&br)) {      /* scaling_list */
                    // SPS scaling lists are not used by the decoder, skip for now.
                    // The logic below just parses it.
                    int lastScale = 8, nextScale = 8;
                    int size = (i < 6) ? 16 : 64;
                    for (int j = 0; j < size; j++) {
                        if (nextScale != 0) nextScale = (lastScale + h264_br_se(&br) + 256) % 256;
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }

    s->log2MaxFrameNumMinus4 = h264_br_ue(&br);
    s->picOrderCntType = h264_br_ue(&br);
    if (s->picOrderCntType == 0) {
        s->log2MaxPicOrderCntLsbMinus4 = h264_br_ue(&br);
    } else if (s->picOrderCntType == 1) {
        s->deltaPicOrderAlwaysZero = h264_br_get(&br);
        h264_br_se(&br); h264_br_se(&br);
        uint32_t ncc = h264_br_ue(&br);
        for (uint32_t i = 0; i < ncc; i++) h264_br_se(&br);
    }
    s->maxNumRefFrames = h264_br_ue(&br);
    h264_br_get(&br);                        /* gaps_in_frame_num */
    s->picWidthInMbs  = h264_br_ue(&br) + 1;
    uint32_t picHeightInMapUnits = h264_br_ue(&br) + 1;
    s->frameMbsOnly = h264_br_get(&br);
    if (!s->frameMbsOnly) s->mbAdaptive = h264_br_get(&br);
    s->direct8x8Inference = h264_br_get(&br);
    s->picHeightInMbs = picHeightInMapUnits * (2 - s->frameMbsOnly);

    s->width  = s->picWidthInMbs * 16;
    s->height = s->picHeightInMbs * 16;
    if (h264_br_get(&br)) {                  /* frame_cropping */
        uint32_t cl = h264_br_ue(&br), cr = h264_br_ue(&br);
        uint32_t ct = h264_br_ue(&br), cb = h264_br_ue(&br);
        uint32_t subW = (s->chromaFormatIdc == 1) ? 2 : 1;
        uint32_t subH = (s->chromaFormatIdc == 1) ? 2 : 1;
        s->width  -= (cl + cr) * subW;
        s->height -= (ct + cb) * subH * (2 - s->frameMbsOnly);
    }
    s->valid = true;
    return true;
}

// Helper for parsing scaling lists in PPS
static inline void h264_parse_scaling_list(H264BitReader *br, uint8_t *scalingList, int size) {
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < size; j++) {
        if (nextScale != 0) {
            nextScale = (lastScale + h264_br_se(br) + 256) % 256;
        }
        lastScale = (nextScale == 0) ? lastScale : nextScale;
        scalingList[j] = (uint8_t)lastScale;
    }
}

/* ── PPS ────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t ppsId, spsId;
    uint32_t entropyCoding;      /* 0=CAVLC 1=CABAC */
    uint32_t picOrderPresent;
    uint32_t numSliceGroupsMinus1;
    uint32_t weightedPred;
    uint32_t weightedBipredIdc;
    int32_t  picInitQpMinus26;
    int32_t  chromaQpIndexOffset;
    int32_t  secondChromaQpIndexOffset;
    uint32_t deblockCtrlPresent;
    uint32_t transform8x8;
    // Added scaling list fields
    bool     picScalingMatrixPresent;
    uint8_t  scalingList4x4[6][16]; // 6 lists, each 16 values (intra Y, Cb, Cr, inter Y, Cb, Cr)
    uint8_t  scalingList8x8[2][64]; // 2 lists, each 64 values (intra, inter)
    bool     valid;
} H264PPS;

static inline bool h264_parse_pps(const uint8_t *rbsp, uint32_t len,
                                  H264PPS *p, const H264SPS *s)
{
    H264BitReader br;
    memset(p, 0, sizeof(*p)); // Initialize new fields to 0
    p->secondChromaQpIndexOffset = p->chromaQpIndexOffset = 0;
    h264_br_init(&br, rbsp + 1, len - 1);

    p->ppsId = h264_br_ue(&br);
    p->spsId = h264_br_ue(&br);
    p->entropyCoding = h264_br_get(&br);
    h264_br_get(&br);                        /* bottom_field_pic_order */
    p->numSliceGroupsMinus1 = h264_br_ue(&br);
    if (p->numSliceGroupsMinus1 > 0) {
        /* ไม่รองรับ FMO — ข้ามแบบง่าย (clip ทั่วไป = 0) */
    } else {
        /* ไม่มี additional */
    }
    if (p->numSliceGroupsMinus1 > 0) return false;
    h264_br_ue(&br);                         /* num_ref_idx_l0_active_minus1 */
    h264_br_ue(&br);                         /* num_ref_idx_l1_active_minus1 */
    p->weightedPred = h264_br_get(&br);
    p->weightedBipredIdc = h264_br_u(&br, 2);
    p->picInitQpMinus26 = h264_br_se(&br);
    h264_br_se(&br);                         /* pic_init_qs_minus26 */
    p->chromaQpIndexOffset = h264_br_se(&br);
    p->deblockCtrlPresent = h264_br_get(&br);
    h264_br_get(&br);                        /* constrained_intra_pred */
    h264_br_get(&br);                        /* redundant_pic_cnt_present */
    p->secondChromaQpIndexOffset = p->chromaQpIndexOffset;
    if (h264_br_more(&br)) {                 /* ยังไม่จบ — มีส่วนขยาย */
        p->transform8x8 = h264_br_get(&br);
        p->picScalingMatrixPresent = h264_br_get(&br); // Read the flag
        if (p->picScalingMatrixPresent) {
            for (int i = 0; i < ((p->transform8x8 == 0) ? 6 : 8); i++) {
                if (h264_br_get(&br)) { // scaling_list_present_flag
                    if (i < 6) { // 4x4 scaling list
                        h264_parse_scaling_list(&br, p->scalingList4x4[i], 16);
                    } else { // 8x8 scaling list
                        h264_parse_scaling_list(&br, p->scalingList8x8[i - 6], 64);
                    }
                }
            }
        }
        p->secondChromaQpIndexOffset = h264_br_se(&br);
    }
    (void)s;
    p->valid = true;
    return true;
}

/* ── slice header ───────────────────────────────────────────────── */
typedef struct {
    uint32_t firstMbInSlice;
    uint32_t sliceType;          /* 0=P 1=B 2=I (mod 5) */
    uint32_t sliceTypeRaw;
    uint32_t ppsId;
    uint32_t frameNum;
    uint32_t idrPicId;
    uint32_t picOrderCntLsb;
    int32_t  deltaPicOrderCntBottom;
    uint32_t idr;
    uint32_t noOutputOfPriorPics, longTermReference;   /* dec_ref_pic_marking */
    uint32_t cabacInitIdc;
    int32_t  sliceQpDelta;
    uint32_t disableDeblockingIdc;
    int32_t  alphaC0OffsetDiv2, betaOffsetDiv2;
    uint32_t numRefIdxL0Active, numRefIdxL1Active;     /* หลัง override */
    uint32_t sliceHeaderBytes;   /* โดยประมาณ (byte ที่ slice data เริ่ม) */
    uint32_t sliceDataBitOffset; /* bit offset ภายใน slice NAL */
    bool     valid;
} H264SliceHdr;

static inline bool h264_parse_slice(const uint8_t *rbsp, uint32_t len,
                                    H264SliceHdr *sh, const H264SPS *s,
                                    const H264PPS *pps, bool isIdr)
{
    H264BitReader br;
    memset(sh, 0, sizeof(*sh));
    h264_br_init(&br, rbsp + 1, len - 1);   /* ข้าม nal_header byte */

    sh->firstMbInSlice = h264_br_ue(&br);
    sh->sliceTypeRaw = h264_br_ue(&br);
    sh->sliceType = sh->sliceTypeRaw % 5;
    sh->ppsId = h264_br_ue(&br);
    sh->idr = isIdr;

    uint32_t maxFrameNum = 1u << (s->log2MaxFrameNumMinus4 + 4);
    (void)maxFrameNum;
    sh->frameNum = h264_br_u(&br, s->log2MaxFrameNumMinus4 + 4);
    if (!s->frameMbsOnly) {
        if (h264_br_get(&br)) h264_br_get(&br);   /* field_pic + bottom */
    }
    if (isIdr) sh->idrPicId = h264_br_ue(&br);
    if (s->picOrderCntType == 0) {
        sh->picOrderCntLsb = h264_br_u(&br, s->log2MaxPicOrderCntLsbMinus4 + 4);
        if (pps->picOrderPresent) sh->deltaPicOrderCntBottom = h264_br_se(&br);
    } else if (s->picOrderCntType == 1 && !s->deltaPicOrderAlwaysZero) {
        h264_br_se(&br);
        if (pps->picOrderPresent) h264_br_se(&br);
    }
    if (isIdr) {
        sh->noOutputOfPriorPics = h264_br_get(&br);
        sh->longTermReference = h264_br_get(&br);
    } else {
        sh->numRefIdxL0Active = h264_br_ue(&br) + 1;
        if (sh->sliceType == 1) /* B-slice */
            sh->numRefIdxL1Active = h264_br_ue(&br) + 1;
    }
    if (isIdr) {
        if (h264_br_get(&br)) {              /* adaptive_ref_pic_marking */
            for (;;) {
                uint32_t mmco = h264_br_ue(&br);
                if (mmco == 0) break;
                if (mmco == 1 || mmco == 2) h264_br_ue(&br);
                if (mmco == 3 || mmco == 4 || mmco == 6) h264_br_ue(&br);
                if (mmco == 5) { /* mmco5 */ }
            }
        }
    }
    if (pps->entropyCoding && sh->sliceType != 2) {
        sh->cabacInitIdc = h264_br_ue(&br);
    }
    sh->sliceQpDelta = h264_br_se(&br);
    if (sh->sliceType == 2 /* SP */ || sh->sliceType == 4 /* SI */) {
        if (sh->sliceType == 2) h264_br_get(&br);
    }
    if (pps->deblockCtrlPresent) {
        sh->disableDeblockingIdc = h264_br_ue(&br);
        if (sh->disableDeblockingIdc != 1) {
            sh->alphaC0OffsetDiv2 = h264_br_se(&br);
            sh->betaOffsetDiv2 = h264_br_se(&br);
        }
    }
    /* slice_header_bytes โดยประมาณ: ตำแหน่ง byte ปัจจุบัน */
    sh->sliceDataBitOffset = br.bytePos * 8 + (7 - br.bitPos);
    sh->sliceHeaderBytes = br.bytePos + 1;   /* + nal header */
    sh->valid = true;
    return true;
}

/* ── Annex-B NAL iterator ───────────────────────────────────────── */
typedef struct {
    const uint8_t *buf;
    uint32_t       len;
    uint32_t       pos;      /* scan หา start code ถัดไปจากจุดนี้ */
    uint32_t       pending;  /* ตำแหน่ง NAL header ที่รู้แล้ว (=len = ไม่มี) */
} H264NalIter;

static inline void h264_nal_init(H264NalIter *it, const uint8_t *buf, uint32_t len)
{
    it->buf = buf; it->len = len; it->pos = 0; it->pending = len;
}

static inline bool h264_nal_iter_is_same(const H264NalIter *it, const uint8_t *buf, uint32_t len)
{
    return it->buf == buf && it->len == len;
}

/* หา start code ถัดไป (3 หรือ 4 bytes) — คืนค่า offset หรือ len ถ้าไม่เจอ */
static inline uint32_t h264_find_sc(const uint8_t *buf, uint32_t len, uint32_t from)
{
    uint32_t z = 0;
    for (uint32_t i = from; i < len; i++) {
        if (buf[i] == 0) { z++; continue; }
        if (z >= 2 && buf[i] == 1) return i + 1;
        z = 0;
    }
    return len;
}

/* คืนค่า: nal type ผ่าน *type, ตำแหน่ง+ขนาด NAL body (ไม่รวม start code)
 *
 * NOTE: แก้บั๊ก 2026-08-21 — ของเดิม set pos = ตำแหน่ง NAL header ตัวถัดไป
 * แล้วสแกนหา start code ใหม่จากจุดนั้น ทำให้ข้าม NAL สลับตัว (PPS หาย,
 * slice หายครึ่ง) เวอร์ชันนี้จำ pending = NAL header ตัวถัดไปไว้ตรง ๆ */
static inline bool h264_nal_next(H264NalIter *it, const uint8_t **nal,
                                 uint32_t *nalLen, int *type)
{
again:
    ;
    uint32_t start;
    if (it->pending < it->len) {
        start = it->pending;
        it->pending = it->len;
    } else {
        start = h264_find_sc(it->buf, it->len, it->pos);
    }
    if (start >= it->len) { it->pos = it->len; return false; }
    uint32_t nextScHeader = h264_find_sc(it->buf, it->len, start);
    uint32_t bodyEnd = nextScHeader;
    if (bodyEnd >= 4 && it->buf[bodyEnd - 4] == 0 &&
        it->buf[bodyEnd - 3] == 0 && it->buf[bodyEnd - 2] == 0 &&
        it->buf[bodyEnd - 1] == 1) {
        bodyEnd -= 4;
    } else if (bodyEnd >= 3 && it->buf[bodyEnd - 3] == 0 &&
               it->buf[bodyEnd - 2] == 0 && it->buf[bodyEnd - 1] == 1) {
        bodyEnd -= 3;
    }
    while (bodyEnd > start && it->buf[bodyEnd - 1] == 0) bodyEnd--;
    it->pos = nextScHeader;
    it->pending = nextScHeader;           /* NAL header ตัวถัดไป (ถ้ามี) */
    if (bodyEnd <= start) goto again;     /* body ว่าง — ข้าม */
    *nal = it->buf + start;
    *nalLen = bodyEnd - start;
    *type = (*nal)[0] & 0x1F;
    return true;
}

#endif /* __MYINTEL_H264_PARSE_H__ */