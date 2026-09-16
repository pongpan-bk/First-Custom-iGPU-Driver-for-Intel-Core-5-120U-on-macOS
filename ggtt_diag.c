/*
 * ggtt_diag.c — Diagnose "top striped, bottom black" after ProgramPlane/restore
 *
 * Reproduces the 12:28 test that broke the panel, but snapshots GGTT state
 * before/after so we can discriminate:
 *   (a) framebuffer content at GGTT 0..0x7E9000 overwritten
 *   (b) GGTT PTE[0..2025] corrupted/remapped
 *   (c) plane register restore race (SURFLIVE not re-latched)
 *
 * New kext selectors (2.0.194):
 *   8  ReadAperture : input[0]=GGTT byte offset -> 32-bit via CURRENT PTEs
 *   9  ReadGSM      : input[0]=PTE index       -> 64-bit PTE from GSM
 *   10 ggttInvalidate: TLB flush, no input
 *
 * Build: clang -o ggtt_diag ggtt_diag.c -framework IOKit -framework CoreFoundation
 * Run:   ./ggtt_diag
 * Output: /var/tmp/ggtt_pre.txt, /var/tmp/ggtt_post_plane_on.txt,
 *         /var/tmp/ggtt_post_restored.txt (survive reboot — /tmp is cleared)
 */

#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KSELECTOR_ALLOC  0
#define KSELECTOR_MAP    1
#define KSELECTOR_WRITE  2
#define KSELECTOR_SUBMIT 3
#define KSELECTOR_READ   4
#define KSELECTOR_READREG 5
#define KSELECTOR_READMMIO 6
#define KSELECTOR_PROGRAMPLANE 7
#define KSELECTOR_READAPERTURE 8
#define KSELECTOR_READGSM 9
#define KSELECTOR_GGTTINVALIDATE 10

#define PLANE_CTL_1A   0x70180
#define PLANE_STRIDE   0x70188
#define PLANE_SIZE     0x70190
#define PLANE_SURF_1A  0x7019C
#define PLANE_SURFLIVE 0x701AC

#define FB_PAGES       2025      /* 1920*1080*4 / 4096 */
#define AP_DUMP_PAGES  64        /* first 256KB of aperture at GGTT 0 */
#define GEM_MAGIC      0x12345678DEADBEEFULL
#define READ_POLL_MS   10
#define READ_POLL_MAX  200

static const char *kMyIntelGPUClassName = "MyIntelGPU";

static io_connect_t conn;

static kern_return_t call_sel(uint32_t sel, const uint64_t *in, uint32_t inCnt,
                              uint64_t *out, uint32_t *outCnt)
{
    return IOConnectCallMethod(conn, sel, in, inCnt, NULL, 0,
                               out, outCnt, NULL, NULL);
}

static uint64_t read_mmio(uint32_t reg)
{
    uint64_t in[1] = { reg }, out[1] = { 0 };
    uint32_t oc = 1;
    if (call_sel(KSELECTOR_READMMIO, in, 1, out, &oc) != KERN_SUCCESS) return 0xDEADBEEF;
    return out[0] & 0xFFFFFFFFULL;
}

static uint64_t read_gsm(uint64_t idx)
{
    uint64_t in[1] = { idx }, out[1] = { 0 };
    uint32_t oc = 1;
    if (call_sel(KSELECTOR_READGSM, in, 1, out, &oc) != KERN_SUCCESS) return 0xDEADBEEFULL;
    return out[0];
}

static uint32_t read_ap(uint64_t off)
{
    uint64_t in[1] = { off }, out[1] = { 0 };
    uint32_t oc = 1;
    if (call_sel(KSELECTOR_READAPERTURE, in, 1, out, &oc) != KERN_SUCCESS) return 0xDEADBEEF;
    return (uint32_t)(out[0] & 0xFFFFFFFFULL);
}

static void dump_state(const char *path, const char *tag)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return; }

    fprintf(f, "=== %s ===\n", tag);

    /* (1) Full PTE dump 0..FB_PAGES-1 with run analysis */
    uint64_t ptes[FB_PAGES];
    for (int i = 0; i < FB_PAGES; i++) ptes[i] = read_gsm(i);
    fprintf(f, "-- PTEs 0..%d --\n", FB_PAGES - 1);
    for (int i = 0; i < FB_PAGES; i++) {
        if (i < 8 || (i >= FB_PAGES - 8) || ptes[i] == 0 || (i > 0 && ptes[i] != ptes[i-1] + 0x1000))
            fprintf(f, "  PTE[%4d] = 0x%016llx\n", i, ptes[i]);
    }
    /* contiguous run summary */
    int runStart = -1, runLen = 0, lastNonzero = -1;
    for (int i = 0; i < FB_PAGES; i++) {
        if (ptes[i]) { if (runStart < 0) runStart = i; runLen++; lastNonzero = i; }
    }
    fprintf(f, "  nonzero PTE count: %d (first=%d last=%d)\n",
            runLen, runStart, lastNonzero);
    /* detect strict fbBase+i*0x1000 contiguity like kext step 3c */
    uint64_t fbBase = ptes[0] & ~0xFFFULL;
    int contigEnd = 1;
    while (contigEnd < FB_PAGES) {
        uint64_t expect = (fbBase + ((uint64_t)contigEnd << 12)) | 1ULL;
        if (ptes[contigEnd] != expect) break;
        contigEnd++;
    }
    fprintf(f, "  kext-contiguous run (PTE[i]==fbBase+i*0x1000|1) ends at: %d\n", contigEnd);

    /* (2) Aperture content at GGTT 0: first 64 pages, sample 1 dword/page */
    fprintf(f, "-- Aperture GGTT 0..0x%X (first dword of each page) --\n", AP_DUMP_PAGES * 0x1000);
    for (int p = 0; p < AP_DUMP_PAGES; p++) {
        uint32_t v = read_ap((uint64_t)p * 0x1000);
        fprintf(f, "  AP[%3d]@0x%06X = 0x%08X\n", p, p * 0x1000, v);
    }

    /* (3) Plane 1A registers */
    fprintf(f, "-- Plane 1A registers --\n");
    fprintf(f, "  PLANE_CTL    (0x70180) = 0x%08llX\n", read_mmio(PLANE_CTL_1A));
    fprintf(f, "  PLANE_STRIDE (0x70188) = 0x%08llX\n", read_mmio(PLANE_STRIDE));
    fprintf(f, "  PLANE_SIZE   (0x70190) = 0x%08llX\n", read_mmio(PLANE_SIZE));
    fprintf(f, "  PLANE_SURF   (0x7019C) = 0x%08llX\n", read_mmio(PLANE_SURF_1A));
    fprintf(f, "  SURFLIVE     (0x701AC) = 0x%08llX\n", read_mmio(PLANE_SURFLIVE));

    fclose(f);
    printf("wrote %s\n", path);
}

int main(void)
{
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                       IOServiceMatching(kMyIntelGPUClassName));
    if (!service) { fprintf(stderr, "FAIL: MyIntelGPU not found\n"); return 2; }

    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: IOServiceOpen 0x%x\n", kr); return 3; }

    /* ── PRE-TEST SNAPSHOT ── */
    dump_state("/var/tmp/ggtt_pre.txt", "PRE-TEST (desktop on BIOS fb)");

    /* ── REPRODUCE 12:28 TEST ──
     * alloc 4MB buffer -> write magic -> BCS blit -> alloc 8.3MB fb ->
     * ProgramPlane -> poll SURFLIVE -> close client (triggers restorePlane) */

    uint64_t sizeBytes = 4 * 1024 * 1024;
    uint64_t inAlloc[1] = { sizeBytes }, outAlloc[1] = { 0 };
    uint32_t oc = 1;
    kr = call_sel(KSELECTOR_ALLOC, inAlloc, 1, outAlloc, &oc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: AllocGEM 0x%x\n", kr); return 4; }
    uint64_t handle = outAlloc[0];
    printf("[alloc] 4MB buffer handle=%p\n", (void *)handle);

    uint64_t inMap[1] = { handle }, outMap[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_MAP, inMap, 1, outMap, &oc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: MapGTT 0x%x\n", kr); return 5; }
    uint64_t ggttOffset = outMap[0];
    printf("[map]   test buffer ggttOffset=0x%llx\n", ggttOffset);

    /* WriteGEM magic into buffer page 0 (structInput path, like gem_test) */
    uint64_t payload = GEM_MAGIC;
    uint64_t inWrite[2] = { handle, payload };
    kr = IOConnectCallMethod(conn, KSELECTOR_WRITE, NULL, 0,
                             inWrite, sizeof(inWrite), NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: WriteGEM 0x%x\n", kr); return 6; }
    printf("[write] magic written\n");

    /* BCS blit: copy page0 -> page1 of the test buffer */
    uint64_t bcsDst = ggttOffset + 0x1000;
    uint64_t inBcs[3] = { handle, 1, bcsDst };
    kr = call_sel(KSELECTOR_SUBMIT, inBcs, 3, NULL, NULL);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: BcsSubmit 0x%x\n", kr); return 7; }
    printf("[blit]  BCS self-copy -> dst ggtt 0x%llx submitted\n", bcsDst);
    usleep(200 * 1000);   /* let blit complete */

    /* alloc fb 8.3MB + ProgramPlane */
    uint64_t fbSize = 1920 * 1080 * 4;
    uint64_t inAlloc2[1] = { fbSize }, outAlloc2[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_ALLOC, inAlloc2, 1, outAlloc2, &oc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: AllocGEM(fb) 0x%x\n", kr); return 8; }
    uint64_t fbHandle = outAlloc2[0];

    uint64_t inMap2[1] = { fbHandle }, outMap2[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_MAP, inMap2, 1, outMap2, &oc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: MapGTT(fb) 0x%x\n", kr); return 9; }
    uint64_t fbGGTT = outMap2[0];
    printf("[fb]    fb ggttOffset=0x%llx\n", fbGGTT);

    uint64_t inProg[1] = { fbHandle }, outProg[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_PROGRAMPLANE, inProg, 1, outProg, &oc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: ProgramPlane 0x%x\n", kr); return 10; }
    printf("[plane] plane 1A -> GGTT 0x%llx (latched=%llu)\n", fbGGTT, outProg[0]);

    int latched = 0;
    for (int i = 0; i < READ_POLL_MAX; i++) {
        uint64_t live = read_mmio(PLANE_SURFLIVE);
        if ((live & ~0xFFFULL) == (fbGGTT & ~0xFFFULL)) { latched = 1; break; }
        usleep(READ_POLL_MS * 1000);
    }
    printf("[plane] SURFLIVE latched=%d\n", latched);

    /* ── POST-TEST SNAPSHOT #1: plane still on fb buffer (before restore) ── */
    dump_state("/var/tmp/ggtt_post_plane_on.txt", "POST-TEST (plane on fb buffer)");

    /* ── Close client -> stop()/free() -> restorePlane() ── */
    printf("[close] IOServiceClose -> restorePlane()...\n");
    IOServiceClose(conn);

    /* re-open for post-restore probes */
    service = IOServiceGetMatchingService(kIOMainPortDefault,
                                          IOServiceMatching(kMyIntelGPUClassName));
    if (!service) { fprintf(stderr, "FAIL: MyIntelGPU not found after close\n"); return 10; }
    kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: re-open 0x%x\n", kr); return 11; }

    /* ── POST-TEST SNAPSHOT #2: after restorePlane (the broken state) ── */
    dump_state("/var/tmp/ggtt_post_restored.txt", "POST-RESTORE (plane back on BIOS fb)");

    /* ── Decisive test: ggttInvalidate — if display recovers -> stale TLB ── */
    uint64_t inInv[0], outInv[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_GGTTINVALIDATE, inInv, 0, outInv, &oc);
    printf("[tlb]   ggttInvalidate kr=0x%x\n", kr);
    usleep(100 * 1000);

    uint64_t inLive[1] = { PLANE_SURFLIVE }, outLive[1] = { 0 };
    oc = 1;
    kr = call_sel(KSELECTOR_READMMIO, inLive, 1, outLive, &oc);
    printf("[tlb]   SURFLIVE after invalidate = 0x%llx\n", outLive[0] & 0xFFFFFFFFULL);

    IOServiceClose(conn);
    printf("DONE — compare /var/tmp/ggtt_pre.txt vs ggtt_post_plane_on.txt vs ggtt_post_restored.txt\n");
    return 0;
}