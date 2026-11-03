/*
 * myintel-decode — Annex-B H.264/HEVC file decoder using MyIntelGPU.kext
 *
 * Layer 1 (Application) of the 4-layer media stack:
 *   myintel-decode (this file)
 *     -> libmyintelvcs (bridge: DDI — parse/pool/submit)          [Layer 2/3]
 *       -> MyIntelVCSClient selectors 6/22-25 (kext DDI)           [Layer 2]
 *         -> VDBOX via VCS ring (Gen12 MFX/HCP packets)            [Layer 4]
 *
 * Usage:
 *   myintel-decode <input> <output.nv12> [--codec h264] [--max-frames N]
 *                                        [--no-detile] [--golden FILE]
 *
 * De-tile supports width==128, height multiple of 32 (verified against
 * golden_ubuntu.nv12). Other geometries: pass --no-detile and de-tile
 * externally.
 *
 * Exit codes: 0 ok, 1 usage/io error, 2 decode error, 3 golden mismatch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "VCSMediaHarness/myintel_vcs_user.h"
#include "VCSMediaHarness/myintelvcs_bridge.h"
#include "VCSMediaHarness/myintel_h264_parse.h"

#define MAX_FILE_BYTES (256u * 1024u * 1024u)

struct nal_span { size_t off, len; uint8_t type; };

static size_t find_start_code(const uint8_t *p, size_t n, size_t from, size_t *scLen)
{
    for (size_t i = from; i + 3 <= n; i++) {
        if (p[i] == 0 && p[i+1] == 0 && p[i+2] == 1) { *scLen = 3; return i; }
        if (i + 4 <= n && p[i] == 0 && p[i+1] == 0 && p[i+2] == 0 && p[i+3] == 1) {
            *scLen = 4; return i;
        }
    }
    return SIZE_MAX;
}

/* Split Annex-B stream into access units. A new AU starts at a VCL NAL
 * whose (frame_num, pic_order_cnt_lsb, is_idr) differs from the previous
 * VCL NAL. Parameter sets / SEI bind to the following AU. Returns AU count. */
static size_t split_access_units(const uint8_t *data, size_t len,
                                 struct nal_span **outNals,
                                 size_t *outNalCount, size_t *outAuOffsets,
                                 size_t maxNals)
{
    struct nal_span *nals = malloc(maxNals * sizeof(*nals));
    size_t nNals = 0;
    size_t pos = 0;
    while (nNals < maxNals) {
        size_t scLen = 0;
        size_t sc = find_start_code(data, len, pos, &scLen);
        if (sc == SIZE_MAX) break;
        size_t nalStart = sc + scLen;
        size_t nextScLen = 0;
        size_t next = find_start_code(data, len, nalStart, &nextScLen);
        nals[nNals].off  = sc;
        nals[nNals].len  = (next == SIZE_MAX) ? len - sc : next - sc;
        nals[nNals].type = data[nalStart] & 0x1F;
        nNals++;
        pos = nalStart;
    }

    H264SPS sps;
    H264PPS pps;
    memset(&sps, 0, sizeof(sps));
    memset(&pps, 0, sizeof(pps));
    bool haveSps = false, havePps = false;
    uint32_t prevFrameNum = 0, prevPoc = 0;
    bool prevIdr = false, prevInit = false;

    size_t nBound = 0;
    outAuOffsets[nBound++] = 0;
    size_t auHasVcl = 0;
    uint8_t rbsp[16384];
    for (size_t i = 0; i < nNals; i++) {
        uint8_t t = nals[i].type;
        int isVcl = (t >= 1 && t <= 5);
        size_t scLen = (nals[i].len >= 4 && !memcmp(data + nals[i].off, "\x00\x00\x00\x01", 4)) ? 4 : 3;
        const uint8_t *nalPtr = data + nals[i].off + scLen;
        uint32_t nalLen = (uint32_t)(nals[i].len - scLen);

        int newAu = 0;
        if (t == 7 || t == 8 || isVcl) {
            uint32_t rbspLen = h264_rbsp(nalPtr, nalLen, rbsp, sizeof(rbsp));
            if (t == 7 && rbspLen > 0) {
                haveSps = h264_parse_sps(rbsp, rbspLen, &sps);
            } else if (t == 8 && rbspLen > 0 && haveSps) {
                havePps = h264_parse_pps(rbsp, rbspLen, &pps, &sps);
            } else if (isVcl && rbspLen > 0 && haveSps && havePps) {
                H264SliceHdr sh;
                memset(&sh, 0, sizeof(sh));
                if (h264_parse_slice(rbsp, rbspLen, &sh, &sps, &pps, t == 5)) {
                    int poc = (sps.picOrderCntType == 0) ? (int)sh.picOrderCntLsb : (int)sh.frameNum;
                    int isIdr = (t == 5);
                    newAu = prevInit && ((int)sh.frameNum != (int)prevFrameNum ||
                                         poc != (int)prevPoc || isIdr != (int)prevIdr);
                    prevFrameNum = sh.frameNum;
                    prevPoc      = (uint32_t)poc;
                    prevIdr      = (bool)isIdr;
                    prevInit     = true;
                } else {
                    newAu = auHasVcl != 0;
                }
            } else if (isVcl) {
                newAu = auHasVcl != 0;
            }
        }

        if (newAu && auHasVcl) {
            outAuOffsets[nBound++] = i;
            auHasVcl = 0;
        }
        if (isVcl) auHasVcl = 1;
    }
    outAuOffsets[nBound] = nNals;      /* sentinel = end */
    *outNals = nals;
    *outNalCount = nNals;
    return nBound;
}

/* NV12 Gen12 4K-tile de-tile for width==128, any height % 32 == 0.
 * Returns malloc'd linear buffer of w*h*3/2 bytes, or NULL. */
static uint8_t *detile_nv12_w128(const uint8_t *buf, size_t bufLen, uint32_t h)
{
    const uint32_t w = 128;
    size_t need = (size_t)w * h * 3 / 2;
    uint8_t *out = malloc(need);
    if (!out) return NULL;
    uint32_t yBytes = w * h;
    if (bufLen < (size_t)yBytes + (w * (h / 2))) { free(out); return NULL; }

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            size_t o = (size_t)(y / 32) * 4096 + (y % 32) * 16 + (x / 16) * 512 + (x % 16);
            out[(size_t)y * w + x] = buf[o];
        }
    }
    for (uint32_t v = 0; v < h / 2; v++) {
        for (uint32_t u = 0; u < w / 2; u++) {
            size_t o = yBytes + (size_t)(v / 32) * 4096 + (v % 32) * 16 + (u / 8) * 512 + (u % 8) * 2;
            size_t s = yBytes + (size_t)v * w + (size_t)u * 2;
            out[s]     = buf[o];
            out[s + 1] = buf[o + 1];
        }
    }
    return out;
}

int main(int argc, char **argv)
{
    const char *inPath = NULL, *outPath = NULL, *goldenPath = NULL;
    const char *codec = "h264";
    uint32_t maxFrames = UINT32_MAX;
    int noDetile = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--codec") && i + 1 < argc) codec = argv[++i];
        else if (!strcmp(argv[i], "--max-frames") && i + 1 < argc) maxFrames = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) goldenPath = argv[++i];
        else if (!strcmp(argv[i], "--no-detile")) noDetile = 1;
        else if (argv[i][0] == '-') { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
        else if (!inPath) inPath = argv[i];
        else if (!outPath) outPath = argv[i];
        else { fprintf(stderr, "unexpected argument %s\n", argv[i]); return 1; }
    }
    if (!inPath || !outPath) {
        fprintf(stderr, "usage: myintel-decode <input> <output.nv12> [--codec h264] [--max-frames N] [--no-detile] [--golden FILE]\n");
        return 1;
    }
    if (strcmp(codec, "h264") != 0) {
        fprintf(stderr, "--codec %s not supported yet (h264 only)\n", codec);
        return 1;
    }

    struct stat st;
    if (stat(inPath, &st) != 0 || st.st_size <= 0 || (size_t)st.st_size > MAX_FILE_BYTES) {
        perror("input"); return 1;
    }
    FILE *f = fopen(inPath, "rb");
    if (!f) { perror("open input"); return 1; }
    uint8_t *data = malloc((size_t)st.st_size);
    if (!data || fread(data, 1, (size_t)st.st_size, f) != (size_t)st.st_size) {
        perror("read input"); fclose(f); return 1;
    }
    fclose(f);

    size_t nalCap = (size_t)st.st_size / 4 + 2;
    size_t *auOffsets = malloc((nalCap + 1) * sizeof(*auOffsets));
    if (!auOffsets) { perror("alloc"); free(data); return 1; }
    struct nal_span *nalsPtr = NULL;
    size_t nNals = 0;
    size_t nAu = split_access_units(data, (size_t)st.st_size, &nalsPtr, &nNals, auOffsets, nalCap);
    const struct nal_span *nals = nalsPtr;
    printf("[INFO] input=%s bytes=%lld nals=%zu access_units=%zu\n",
           inPath, (long long)st.st_size, nNals, nAu);

    mvcs_ctx *ctx = NULL;
    if (mvcs_open(&ctx) != 0) {
        fprintf(stderr, "mvcs_open failed (kext loaded?)\n");
        return 2;
    }

    FILE *of = fopen(outPath, "wb");
    if (!of) { perror("open output"); mvcs_close(ctx); return 1; }

    size_t bytesOut = 0;
    uint32_t frames = 0;
    int rc = 0;
    for (size_t a = 0; a < nAu && frames < maxFrames; a++) {
        size_t b = auOffsets[a], e = auOffsets[a + 1];
        size_t auLen = (size_t)nals[e - 1].off + nals[e - 1].len - (size_t)nals[b].off;
        const uint8_t *au = data + nals[b].off;

        mvcs_frame_t fr;
        memset(&fr, 0, sizeof(fr));
        int drc = mvcs_decode_h264(ctx, au, (uint32_t)auLen, 0, 0, MYVCS_SURF_FMT_NV12, &fr);
        if (drc != 0) {
            fprintf(stderr, "[FAIL] AU %zu: mvcs_decode_h264 rc=%d\n", a, drc);
            rc = 2;
            break;
        }
        uint32_t h = fr.height, w = fr.width;
        const uint8_t *px = fr.pixels;
        size_t pxLen = fr.size;

        if (!noDetile) {
            if (w != 128 || (h % 32) != 0) {
                fprintf(stderr, "[FAIL] de-tile supports width=128, height%%32==0 (got %ux%u); use --no-detile\n", w, h);
                rc = 2;
                break;
            }
            uint8_t *lin = detile_nv12_w128(px, pxLen, h);
            if (!lin) { fprintf(stderr, "de-tile alloc failed\n"); rc = 2; break; }
            fwrite(lin, 1, (size_t)w * h * 3 / 2, of);
            bytesOut += (size_t)w * h * 3 / 2;
            free(lin);
        } else {
            fwrite(px, 1, pxLen, of);
            bytesOut += pxLen;
        }
        frames++;
        printf("[OK] frame %u (AU %zu) %ux%u\n", frames, a, w, h);
        mvcs_frame_release(ctx, &fr);
    }

    fclose(of);
    mvcs_close(ctx);
    printf("[INFO] wrote %s (%zu bytes, %u frames)\n", outPath, bytesOut, frames);

    if (goldenPath && rc == 0) {
        /* compare byte-wise */
        FILE *gf = fopen(goldenPath, "rb");
        FILE *cf = fopen(outPath, "rb");
        if (!gf || !cf) { perror("golden"); return 3; }
        int c, g, mismatch = 0;
        size_t idx = 0;
        while ((c = fgetc(cf)) != EOF && (g = fgetc(gf)) != EOF) {
            if (c != g && mismatch < 5) {
                fprintf(stderr, "[DIFF] byte %zu: out=0x%02X golden=0x%02X\n", idx, c, g);
                mismatch++;
            }
            idx++;
        }
        int extraC = fgetc(cf), extraG = fgetc(gf);
        fclose(gf); fclose(cf);
        if (extraC == EOF && extraG == EOF && mismatch == 0 && idx == bytesOut) {
            printf("[GOLDEN] MATCH (%zu bytes)\n", idx);
        } else {
            printf("[GOLDEN] MISMATCH (compared %zu bytes)\n", idx);
            rc = 3;
        }
    }

    free(nalsPtr);
    free(auOffsets);
    free(data);
    return rc;
}
