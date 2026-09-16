/*===========================================================================
 *  mvcs_bench.c — CLI decode bench + regression gate for libmyintelvcs
 *
 *  Usage:
 *    ./mvcs_bench file.h264 [frames] [width] [height] [fpsGate]
 *
 *  Defaults: frames=all, W=1920 H=1080, gate=0 (no gate)
 *  Exit: 0 ok+gate-pass, 1 runtime error, 2 gate-fail
 *///=========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "myintelvcs_bridge.h"

static double nowMs(void)
{
    return (double)clock_gettime_nsec_np(CLOCK_MONOTONIC) / 1e6;
}

static unsigned char *readFile(const char *path, size_t *outLen)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *outLen = (size_t)sz;
    return buf;
}

/* Split next frame: a frame = NALs up to (not incl.) the next VCL NAL that
 * follows an already-collected VCL NAL. Returns bytes consumed; *frameLen set. */
static size_t nextFrame(const unsigned char *p, size_t len, size_t off,
                        const unsigned char **frame, size_t *frameLen)
{
    size_t start = off, nal = off;
    int sawVcl = 0;
    *frame = NULL; *frameLen = 0;

    while (nal + 3 <= len) {
        while (nal + 2 < len && !(p[nal] == 0 && p[nal+1] == 0 && p[nal+2] == 1)) {
            if (nal + 3 < len && p[nal] == 0 && p[nal+1] == 0 && p[nal+2] == 0 && p[nal+3] == 1) break;
            nal++;
        }
        if (nal + 2 >= len) break;
        size_t scLen = (p[nal]==0 && nal+3<len && p[nal+1]==0 && p[nal+2]==0 && p[nal+3]==1) ? 4 : 3;
        uint8_t t = p[nal + scLen] & 0x1F;
        int isVcl = (t >= 1 && t <= 5);
        if (isVcl && sawVcl) {
            *frame = p + start;
            *frameLen = nal - start;
            return nal;
        }
        if (isVcl) sawVcl = 1;
        nal += scLen;
    }
    if (sawVcl && len > start) {
        *frame = p + start;
        *frameLen = len - start;
        return len;
    }
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.h264 [frames] [W] [H] [fpsGate]\n", argv[0]);
        return 1;
    }
    uint32_t W = argc > 3 ? (uint32_t)atoi(argv[3]) : 1920;
    uint32_t H = argc > 4 ? (uint32_t)atoi(argv[4]) : 1080;
    double gate = argc > 5 ? atof(argv[5]) : 0.0;
    long maxFrames = argc > 2 ? atol(argv[2]) : 0;

    size_t len = 0;
    unsigned char *data = readFile(argv[1], &len);
    if (!data) { fprintf(stderr, "[BENCH] cannot read %s\n", argv[1]); return 1; }

    mvcs_ctx *ctx = NULL;
    if (mvcs_open(&ctx)) { fprintf(stderr, "[BENCH] mvcs_open failed (kext loaded?)\n"); return 1; }

    uint64_t mmio=0, rggtt=0, rsz=0;
    mvcs_get_context(ctx, &mmio, &rggtt, &rsz);
    printf("[BENCH] ctx ok mmio=%#llx ringGgtt=%#llx ringSize=%llu | %s %ux%u\n",
           (unsigned long long)mmio, (unsigned long long)rggtt,
           (unsigned long long)rsz, argv[1], W, H);

    size_t off = 0;
    long n = 0, fail = 0;
    double totalMs = 0.0;

    while (len > 0 && (maxFrames <= 0 || n < maxFrames)) {
        const unsigned char *fr = NULL;
        size_t frLen = 0;
        off = nextFrame(data, len, off, &fr, &frLen);
        if (!off || !fr || !frLen) break;
        if (n == 0)
            printf("[BENCH] first frame %zu bytes\n", frLen);

        mvcs_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        double t0 = nowMs();
        int rc = mvcs_decode_h264(ctx, fr, (uint32_t)frLen, W, H, MYVCS_SURF_FMT_NV12, &frame);
        double t1 = nowMs();
        if (rc != 0) {
            fail++;
            fprintf(stderr, "[BENCH] frame %ld rc=%d lastErr=%d (%zu bytes)\n", n, rc, mvcs_last_error(ctx), frLen);
            if (fail >= 3) { fprintf(stderr, "[BENCH] abort after 3 fails\n"); break; }
        } else {
            mvcs_frame_release(ctx, &frame);
            totalMs += (t1 - t0);
            n++;
            if (n % 50 == 0) printf("[BENCH] %ld frames avg=%.2fms\n", n, totalMs / (double)n);
        }
    }

    double fps = (totalMs > 0) ? (double)n * 1000.0 / totalMs : 0.0;
    printf("\n[BENCH] RESULT ok=%ld fail=%ld avg=%.2fms fps=%.1f\n", n, fail, n ? totalMs/(double)n : 0.0, fps);

    mvcs_close(ctx);
    free(data);

    if (n == 0) return 1;
    if (gate > 0 && fps < gate) { printf("[BENCH] GATE FAIL: %.1f < %.1f\n", fps, gate); return 2; }
    if (gate > 0) printf("[BENCH] GATE PASS\n");
    return 0;
}
