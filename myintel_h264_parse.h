/*
 * H.264/AVC bitstream parser (header-only).
 *
 * Parse SPS / PPS / slice header from a real Annex-B bitstream
 *  to feed correct values to MFX_AVC_IMG_STATE / MFX_AVC_SLICE_STATE /
 *  MFD_AVC_BSD_OBJECT (replacing the Phase 9 templates)
 *
 *  Only the sections required for decode are supported:
 *    - SPS: picture size, profile, log2_max_frame_num, POC type, ref frames,
 *           frame_mbs_only, direct_8x8, cropping
 *    - PPS: entropy mode, pic_order_present, weighted_pred, deblock ctrl,
 *           pic_init_qp, chroma qp offsets, **scaling_list**
 *    - slice header: first_mb, slice_type, frame_num, POC lsb, IDR marking,
 *           qp_delta, deblock, cabac_init_idc
 *
 *  Pure C, no kernel deps — usable from both the test app and the dylib
 */

#ifndef __MYINTEL_H264_PARSE_H__
#define __MYINTEL_H264_PARSE_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── bit reader (MSB-first, per H.264 spec) ──────────────────────── */
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

/* any bits remaining? */
static inline bool h264_br_more(H264BitReader *br)
{
    return br->bytePos < br->len;
}

/* ── RBSP extract (strip emulation prevention 0x03) ─────────────── */
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
    uint32_t width, height;          /* after cropping */
    uint32_t picWidthInMbs, picHeightInMbs;
    bool     valid;
} H264SPS;

static inline bool h264_parse_sps(const uint8_t *rbsp, uint32_t len, H264SPS *s)
{
    H264BitReader br;
    memset(s, 0, sizeof(*s));
    h264_br_init(&br, rbsp + 1, len - 1);   /* skip nal_header byte */

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
    uint32_t constrainedIntraPred;
    uint32_t redundantPicCntPresent;
    uint32_t numRefIdxL0DefaultActiveMinus1;
    uint32_t numRefIdxL1DefaultActiveMinus1;
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
    p->picOrderPresent = h264_br_get(&br);  /* bottom_field_pic_order_in_frame_present_flag */
    p->numSliceGroupsMinus1 = h264_br_ue(&br);
    if (p->numSliceGroupsMinus1 > 0) {
        /* FMO not supported — simple skip (typical clip = 0) */
    } else {
        /* no additional data */
    }
    if (p->numSliceGroupsMinus1 > 0) return false;
    p->numRefIdxL0DefaultActiveMinus1 = h264_br_ue(&br);
    p->numRefIdxL1DefaultActiveMinus1 = h264_br_ue(&br);
    p->weightedPred = h264_br_get(&br);
    p->weightedBipredIdc = h264_br_u(&br, 2);
    p->picInitQpMinus26 = h264_br_se(&br);
    h264_br_se(&br);                         /* pic_init_qs_minus26 */
    p->chromaQpIndexOffset = h264_br_se(&br);
    p->deblockCtrlPresent = h264_br_get(&br);
    p->constrainedIntraPred = h264_br_get(&br);
    p->redundantPicCntPresent = h264_br_get(&br);
    p->secondChromaQpIndexOffset = p->chromaQpIndexOffset;
    if (h264_br_more(&br)) {                 /* not finished — extension present */
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
    uint32_t numRefIdxL0Active, numRefIdxL1Active;     /* after override */
    uint32_t redundantPicCnt, directSpatialMvPred, nalRefIdc;
    uint32_t sliceHeaderBytes;   /* approx (byte where slice data begins) */
    uint32_t sliceDataBitOffset; /* bit offset inside the slice NAL */
    bool     valid;
} H264SliceHdr;

static inline bool h264_parse_slice(const uint8_t *rbsp, uint32_t len,
                                    H264SliceHdr *sh, const H264SPS *s,
                                    const H264PPS *pps, bool isIdr)
{
    H264BitReader br;
    memset(sh, 0, sizeof(*sh));
    /* H.264 default num_ref_idx_lX_active_minus1 = 0 -> active count = 1.
     * IDR/I slices have no override_flag, so without this the driver reports
     * zero active refs; iHD picture state writes num_ref_idx_lX_active_minus1+1. */
    sh->numRefIdxL0Active = 1;
    sh->numRefIdxL1Active = 1;
    h264_br_init(&br, rbsp + 1, len - 1);   /* skip nal_header byte */

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
    /* [FIX 2026-11-01] field order per spec 7.3.3: redundant_pic_cnt -> direct_spatial
     * -> num_ref_idx override -> ref_pic_list_modification -> pred_weight_table
     * -> dec_ref_pic_marking -> cabac_init_idc -> slice_qp_delta
     * old bug: IDR read dec_ref_pic_marking twice (ate 1 bit of slice_qp_delta)
     * -> slice_qp_delta decoded as -2 instead of -5 -> QP 26 instead of 23 -> image distorted 17% */
    sh->nalRefIdc = (rbsp[0] >> 5) & 3;
    if (pps->redundantPicCntPresent) sh->redundantPicCnt = h264_br_ue(&br);
    if (sh->sliceType == 1) sh->directSpatialMvPred = h264_br_get(&br);   /* B */
    if (sh->sliceType != 2 && sh->sliceType != 4) {          /* P / SP / B */
        if (h264_br_get(&br)) {                              /* num_ref_idx_active_override_flag */
            sh->numRefIdxL0Active = h264_br_ue(&br) + 1;
            if (sh->sliceType == 1) sh->numRefIdxL1Active = h264_br_ue(&br) + 1;
        } else {
            sh->numRefIdxL0Active = pps->numRefIdxL0DefaultActiveMinus1 + 1;
            sh->numRefIdxL1Active = pps->numRefIdxL1DefaultActiveMinus1 + 1;
        }
        /* ref_pic_list_modification() */
        for (int lx = 0; lx < ((sh->sliceType == 1) ? 2 : 1); lx++) {
            if (h264_br_get(&br)) {
                for (;;) {
                    uint32_t idc = h264_br_ue(&br);
                    if (idc == 3) break;
                    h264_br_ue(&br);
                }
            }
        }
        /* pred_weight_table() — 4:2:0 => chroma_array_type != 0 */
        if ((pps->weightedPred && sh->sliceType != 1) ||
            (pps->weightedBipredIdc == 1 && sh->sliceType == 1)) {
            int nl[2] = { (int)sh->numRefIdxL0Active,
                          (sh->sliceType == 1) ? (int)sh->numRefIdxL1Active : 0 };
            h264_br_ue(&br);                                 /* luma_log2_weight_denom */
            h264_br_ue(&br);                                 /* chroma_log2_weight_denom */
            for (int l = 0; l < 2; l++) {
                for (int i = 0; i < nl[l]; i++) {
                    if (h264_br_get(&br)) { for (int j = 0; j < 4; j++) h264_br_se(&br); }
                    if (h264_br_get(&br)) { for (int j = 0; j < 4; j++) h264_br_se(&br); }
                }
            }
        }
    }
    /* dec_ref_pic_marking() — read exactly once (IDR = 2 bits, non-IDR = adaptive flag) */
    if (sh->nalRefIdc != 0) {
        if (isIdr) {
            sh->noOutputOfPriorPics = h264_br_get(&br);
            sh->longTermReference   = h264_br_get(&br);
        } else if (h264_br_get(&br)) {                       /* adaptive_ref_pic_marking_mode_flag */
            for (;;) {
                uint32_t mmco = h264_br_ue(&br);
                if (mmco == 0) break;
                if (mmco == 1 || mmco == 2 || mmco == 4) h264_br_ue(&br);
                if (mmco == 3 || mmco == 6) { h264_br_ue(&br); h264_br_ue(&br); }
            }
        }
    }
    if (pps->entropyCoding && sh->sliceType != 2 && sh->sliceType != 4) {
        sh->cabacInitIdc = h264_br_ue(&br);
    }
    sh->sliceQpDelta = h264_br_se(&br);
    if (sh->sliceType == 3) h264_br_get(&br);                /* SP: sp_for_switch_flag */
    if (sh->sliceType == 3 || sh->sliceType == 4) h264_br_se(&br);   /* slice_qs_delta */
    if (pps->deblockCtrlPresent) {
        sh->disableDeblockingIdc = h264_br_ue(&br);
        if (sh->disableDeblockingIdc != 1) {
            sh->alphaC0OffsetDiv2 = h264_br_se(&br);
            sh->betaOffsetDiv2 = h264_br_se(&br);
        }
    }
    /* slice_header_bytes approximation: current byte position */
    sh->sliceDataBitOffset = br.bytePos * 8 + (7 - br.bitPos);
    sh->sliceHeaderBytes = br.bytePos + 1;   /* + nal header */
    sh->valid = true;
    return true;
}

/* ── Annex-B NAL iterator ───────────────────────────────────────── */
typedef struct {
    const uint8_t *buf;
    uint32_t       len;
    uint32_t       pos;      /* scan for the next start code from here */
    uint32_t       pending;  /* position of the known NAL header (=len = none) */
} H264NalIter;

static inline void h264_nal_init(H264NalIter *it, const uint8_t *buf, uint32_t len)
{
    it->buf = buf; it->len = len; it->pos = 0; it->pending = len;
}

static inline bool h264_nal_iter_is_same(const H264NalIter *it, const uint8_t *buf, uint32_t len)
{
    return it->buf == buf && it->len == len;
}

/* find the next start code (3 or 4 bytes) — returns offset, or len if not found */
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

/* returns: nal type via *type, position+size of NAL body (excluding start code)
 *
 * NOTE: 2026-08-21 bugfix — previously pos was set to the position of the next NAL header
 * then start-code scan re-ran from there, which skipped every other NAL (PPS lost,
 * half the slices lost). This version pins pending = next NAL header directly) */
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
    uint32_t nextSc = h264_find_sc(it->buf, it->len, start);
    /* h264_find_sc() returns the offset after the 0x01 byte of the next start code
     * so we must back up 1 byte to keep 0x01 (and the leading zeros) out of
     * the NAL body — previously nalLen came out 3-4 bytes long and BSD decode
     * read the start code as CABAC data */
    uint32_t bodyEnd = (nextSc > start) ? (nextSc - 1) : nextSc;
    while (bodyEnd > start && it->buf[bodyEnd - 1] == 0) bodyEnd--;
    it->pos = nextSc;
    it->pending = nextSc;                 /* next NAL header (if any) */
    if (bodyEnd <= start) goto again;     /* empty body — skip */
    *nal = it->buf + start;
    *nalLen = bodyEnd - start;
    *type = (*nal)[0] & 0x1F;
    return true;
}

#endif /* __MYINTEL_H264_PARSE_H__ */