/*
 * MFX Decode Test App (Gen12 AVC, parsed bitstream).
 *
 * Upgraded from the template-command version:
 *    1. Open a real .h264/.264/.annexb file (Annex-B)
 *    2. Parse real SPS / PPS / slice header (myintel_h264_parse.h)
 *    3. Allocate GEM buffers: Source (bitstream) + Destination (NV12)
 *       + Scratch (row stores) + Reference + Direct-MV/temporal
 *    4. Emit the full Gen12 MFX sequence per frame (first frame = IDR):
 *         PIPE_MODE_SELECT → SURFACE_STATE → PIPE_BUF_ADDR → IND_OBJ
 *         → AVC_IMG_STATE → DIRECTMODE → (SLICE_STATE + BSD_OBJECT)/slice
 *         → FLUSH_MEDIA
 *    5. Submit via Selector 6 (kSubmitVCSWorkload) so VDBOX decodes for real
 *    6. Read the Destination Surface back to prove HW wrote the pixels
 *
 *  Usage:
 *    ./test_submit_vcs [file.h264] [out.nv12]
 *      no args = original dummy mode (Phase 8 regression)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include "MyIntelVCSCommand.h"
#include "MyIntelVCS.h"
#include "myintel_vcs_user.h"
#include "myintel_h264_parse.h"

/* ─── Annex-B NAL scanner ─────────────────────────────────────── */

typedef struct {
    uint32_t sps, pps, idr, nonidr, other;
    size_t   firstNalOffset;   /* offset of first start code (bitstream head) */
    int      found;
} NalScan;

/* find the next start code at p[i..] (3- or 4-byte) ; returns the start-code length */
static size_t startCodeAt(const uint8_t *p, size_t len, size_t i)
{
    if (i + 2 >= len) return 0;
    if (p[i] == 0 && p[i + 1] == 0) {
        if (p[i + 2] == 1) return 3;
        if (i + 3 < len && p[i + 2] == 0 && p[i + 3] == 1) return 4;
    }
    return 0;
}

static void scanAnnexB(const uint8_t *data, size_t len, NalScan *out)
{
    memset(out, 0, sizeof(*out));
    out->firstNalOffset = 0;

    for (size_t i = 0; i + 2 < len; ) {
        size_t sc = startCodeAt(data, len, i);
        if (!sc) { i++; continue; }

        uint8_t nalType = data[i + sc] & 0x1F;
        if (!out->found) { out->firstNalOffset = i; out->found = 1; }

        switch (nalType) {
            case 7:  out->sps++;    break;
            case 8:  out->pps++;    break;
            case 5:  out->idr++;    break;
            case 1:
            case 2:
            case 3:
            case 4:  out->nonidr++; break;
            default: out->other++;  break;
        }
        i += sc;
    }
}

/* ─── GEM helpers (selector 7 + clientMemoryForType) ──────────── */

static kern_return_t gemCreate(io_connect_t conn, uint32_t size, uint32_t flags,
                               uint32_t *handle, uint32_t *ggtt, uint32_t *realSize)
{
    uint64_t in[2]  = { size, flags };
    uint64_t out[3] = { 0 };
    uint32_t outCnt = 3;
    kern_return_t kr = IOConnectCallMethod(conn, kMyVCSGemCreate,
                                           in, 2, NULL, 0,
                                           out, &outCnt, NULL, NULL);
    if (kr == KERN_SUCCESS) {
        *handle   = (uint32_t)out[0];
        *ggtt     = (uint32_t)out[1];
        *realSize = (uint32_t)out[2];
    }
    return kr;
}

static kern_return_t gemMap(io_connect_t conn, uint32_t handle,
                            void **addr, size_t *size)
{
    mach_vm_address_t va = 0;
    mach_vm_size_t sz = 0;
    kern_return_t kr = IOConnectMapMemory(conn, MYVCS_GEM_MAP_BASE + handle,
                                          mach_task_self(), &va, &sz,
                                          kIOMapAnywhere);
    if (kr == KERN_SUCCESS) { *addr = (void *)va; *size = (size_t)sz; }
    return kr;
}

static void gemUnmap(uint32_t handle, void *addr, size_t size)
{
    if (addr) vm_deallocate(mach_task_self(), (vm_address_t)addr, size);
    (void)handle;
}

static void gemDestroy(io_connect_t conn, uint32_t handle)
{
    uint64_t in[1] = { handle };
    IOConnectCallMethod(conn, kMyVCSGemDestroy, in, 1, NULL, 0,
                        NULL, NULL, NULL, NULL);
}

/* ─── main ────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    const char *bitPath = (argc > 1) ? argv[1] : NULL;
    const char *outPath = (argc > 2) ? argv[2] : NULL;
    const uint32_t W = 1920, H = 1080;

    printf("═══ test_submit_vcs — Phase 10 Full MFX Decode ═══\n");

    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceNameMatching("MyIntelGPUVCS"));
    if (!service) { printf("FATAL: MyIntelGPUVCS nub not found\n"); return 1; }
    printf("[OK] MyIntelGPUVCS nub found (0x%X)\n", service);

    io_connect_t conn = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) { printf("FATAL: IOServiceOpen failed: 0x%X\n", kr); return 1; }
    printf("[OK] IOServiceOpen (conn=0x%X)\n", conn);

    uint64_t sOut[4] = {0};
    uint32_t sOutCnt = 1;
    kr = IOConnectCallMethod(conn, kMyVCSGetVCSStatus, NULL, 0, NULL, 0, sOut, &sOutCnt, NULL, NULL);
    printf("[STATUS] VCS_CTL=0x%llX kr=0x%X\n", sOut[0], kr);

    sOutCnt = 3; memset(sOut, 0, sizeof(sOut));
    kr = IOConnectCallMethod(conn, kMyVCSGetContextInfo, NULL, 0, NULL, 0, sOut, &sOutCnt, NULL, NULL);
    printf("[CONTEXT] mmioBase=0x%llX ringGgtt=0x%llX ringSize=%llX kr=0x%X\n",
           sOut[0], sOut[1], sOut[2], kr);

    /* ── load real bitstream (if any) ── */
    uint8_t *bit = NULL;
    long     bitLen = 0;
    NalScan  nal = {0};

    if (bitPath) {
        FILE *f = fopen(bitPath, "rb");
        if (!f) { printf("FATAL: open %s failed\n", bitPath); goto done; }
        fseek(f, 0, SEEK_END);
        bitLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (bitLen <= 0) { printf("FATAL: %s empty\n", bitPath); fclose(f); goto done; } // Added comment to prevent accidental modification of goto label
        if (bitLen > 1024 * 1024) bitLen = 1024 * 1024; /* cap 1MB per test run */
        bit = (uint8_t *)malloc((size_t)bitLen);
        bitLen = (long)fread(bit, 1, (size_t)bitLen, f);
        fclose(f);

        scanAnnexB(bit, (size_t)bitLen, &nal);
        printf("[BITSTREAM] %s: %ld bytes\n", bitPath, bitLen);
        printf("[NAL] SPS=%u PPS=%u IDR=%u nonIDR=%u other=%u firstNAL@%zu\n",
               nal.sps, nal.pps, nal.idr, nal.nonidr, nal.other, nal.firstNalOffset);
        if (!nal.found) {
            printf("WARN: no Annex-B start code found — sending the file as raw\n");
        }
    } else {
        printf("[BITSTREAM] no file — dummy mode (Phase 8 regression)\n");
    }

    /* ── Phase 10: parse SPS / PPS / slice header of the first frame (IDR) ── */
    H264SPS      sps;
    H264PPS      pps;
    H264SliceHdr sh;
    /* [P13] multi-slice frame support: IDR @77+@220 = 2 slices/frame — submit every slice
     * otherwise VDBOX waits for MBs that never arrive → frame never finishes → status/pixel silent forever */
#define MAX_FRAME_SLICES 4
    struct {
        uint32_t firstMb, nalOff, nalLen, hdrBytes, bitOff;
        uint32_t qpDelta, cabacIdc, deblockIdc, alphaDiv2, betaDiv2;
        uint32_t refL0, refL1, sliceType;
    } fsl[MAX_FRAME_SLICES];
    /* Declarations to avoid jump bypasses initialization errors */
    uint32_t nFsl;
    bool     haveSps, havePps, haveSlice;
    uint32_t sliceNalOff, sliceNalType;
    uint32_t dW, dH;
    uint32_t cropW, cropH, pitch, surfSize;
    uint32_t srcHandle, srcGgtt, srcSize;
    uint32_t dstHandle, dstGgtt, dstSize;
    uint32_t scrHandle, scrGgtt, scrSize;
    uint32_t refHandle, refGgtt, refSize;
    uint32_t mvHandle, mvGgtt, mvSize;
    void    *srcVA, *dstVA, *scrVA, *refVA, *mvVA;
    size_t   srcMapSz, dstMapSz, scrMapSz, refMapSz, mvMapSz;
    uint32_t wantSrc;
    uint32_t cmdBytes, padDwords;
    
    nFsl = 0;
    haveSps = havePps = haveSlice = false;
    sliceNalOff = 0; sliceNalType = 0;
    dW = W; dH = H;
    // (Other initializations for variables used before the goto jump)

    memset(&sps, 0, sizeof(sps)); memset(&pps, 0, sizeof(pps)); memset(&sh, 0, sizeof(sh));

    if (bit) {
        static uint8_t rbspBuf[1024];
        H264NalIter it;
        h264_nal_init(&it, bit, (uint32_t)bitLen);
        const uint8_t *nalp; uint32_t nlen; int ntype;
        while (h264_nal_next(&it, &nalp, &nlen, &ntype)) {
            if (ntype == 7 && !haveSps) {
                uint32_t rl = h264_rbsp(nalp, nlen, rbspBuf, sizeof(rbspBuf));
                haveSps = h264_parse_sps(rbspBuf, rl, &sps);
            } else if (ntype == 8 && haveSps && !havePps) {
                uint32_t rl = h264_rbsp(nalp, nlen, rbspBuf, sizeof(rbspBuf));
                havePps = h264_parse_pps(rbspBuf, rl, &pps, &sps);
            } else if (ntype == 5 && haveSps && havePps && nFsl < MAX_FRAME_SLICES) {
                H264SliceHdr shTmp;
                memset(&shTmp, 0, sizeof(shTmp));
                uint32_t rl = h264_rbsp(nalp, nlen, rbspBuf, sizeof(rbspBuf));
                if (h264_parse_slice(rbspBuf, rl, &shTmp, &sps, &pps, true)) {
                    fsl[nFsl].firstMb    = shTmp.firstMbInSlice;
                    fsl[nFsl].nalOff     = (uint32_t)(nalp - bit);
                    fsl[nFsl].nalLen     = nlen;
                    fsl[nFsl].hdrBytes   = shTmp.sliceHeaderBytes;
                    fsl[nFsl].bitOff     = shTmp.sliceDataBitOffset % 8;
                    fsl[nFsl].qpDelta    = shTmp.sliceQpDelta;
                    fsl[nFsl].cabacIdc   = shTmp.cabacInitIdc;
                    fsl[nFsl].deblockIdc = shTmp.disableDeblockingIdc;
                    fsl[nFsl].alphaDiv2  = shTmp.alphaC0OffsetDiv2;
                    fsl[nFsl].betaDiv2   = shTmp.betaOffsetDiv2;
                    fsl[nFsl].refL0      = shTmp.numRefIdxL0Active;
                    fsl[nFsl].refL1      = shTmp.numRefIdxL1Active;
                    fsl[nFsl].sliceType  = shTmp.sliceType;
                    if (nFsl == 0) {          /* first slice keeps the legacy interface */
                        sh = shTmp;
                        sliceNalOff  = fsl[0].nalOff;
                        sliceNalType = (uint32_t)ntype;
                        haveSlice = true;
                    }
                    nFsl++;
                }
            } else if (nFsl > 0) {
                break;   /* non-slice NAL after slices began = end of frame */
            }
        }

        if (haveSps && havePps && haveSlice) {
            dW = sps.picWidthInMbs * 16;
            dH = sps.picHeightInMbs * 16;
            printf("[PARSE] SPS: %ux%u crop→%ux%u (%ux%u MB) profile=%u pocType=%u refFrames=%u\n",
                   sps.picWidthInMbs * 16, sps.picHeightInMbs * 16, sps.width, sps.height,
                   sps.picWidthInMbs, sps.picHeightInMbs, sps.profileIdc,
                   sps.picOrderCntType, sps.maxNumRefFrames);
            printf("[PARSE] PPS: entropy=%s initQp=%d transform8x8=%u deblockCtrl=%u\n",
                   pps.entropyCoding ? "CABAC" : "CAVLC", 26 + pps.picInitQpMinus26,
                   pps.transform8x8, pps.deblockCtrlPresent);
            printf("[PARSE] SLICE: type=%u firstMb=%u frameNum=%u qp=%d hdrBytes=%u bitOff=%u nalOff=%u\n",
                   sh.sliceType, sh.firstMbInSlice, sh.frameNum,
                   26 + pps.picInitQpMinus26 + sh.sliceQpDelta,
                   sh.sliceHeaderBytes, sh.sliceDataBitOffset % 8, sliceNalOff);
        } else {
            printf("[PARSE] incomplete SPS(%d)/PPS(%d)/slice(%d) -> using old template mode\n",
                   haveSps, havePps, haveSlice);
        }
    }

    /* [FHD-CROP] dW/dH = MB aligned (1088) for the buffer allocation, but SurfaceState needs the true crop (1080) */
    cropW = (haveSps && sps.valid) ? sps.width  : dW;
    cropH = (haveSps && sps.valid) ? sps.height : dH;
    pitch    = (dW + 127) & ~127u;          /* MFX: pitch align 128B (uses aligned dW) */
    surfSize = pitch * dH * 3 / 2;          /* allocate for 1088 to be safe */

    srcHandle = 0; srcGgtt = 0; srcSize = 0;
    dstHandle = 0; dstGgtt = 0; dstSize = 0;
    scrHandle = 0; scrGgtt = 0; scrSize = 0;   /* row stores + MB status */
    refHandle = 0; refGgtt = 0; refSize = 0;   /* reference picture */
    mvHandle  = 0; mvGgtt  = 0; mvSize  = 0;   /* direct MV + temporal */
    srcVA = NULL; dstVA = NULL; scrVA = NULL; refVA = NULL; mvVA = NULL;
    srcMapSz = 0; dstMapSz = 0; scrMapSz = 0; refMapSz = 0; mvMapSz = 0;

    wantSrc = bit ? (uint32_t)bitLen : 4096;

    kr = gemCreate(conn, wantSrc, MYVCS_GEM_SOURCE, &srcHandle, &srcGgtt, &srcSize);
    printf("[GEM] Source Buffer: handle=%u ggtt=0x%X size=%u kr=0x%X (%s)\n",
           srcHandle, srcGgtt, srcSize, kr, kr == KERN_SUCCESS ? "OK" : "FAIL");
    if (kr != KERN_SUCCESS) goto done;

    kr = gemCreate(conn, surfSize, MYVCS_GEM_SURFACE, &dstHandle, &dstGgtt, &dstSize);
    printf("[GEM] Destination Surface: handle=%u ggtt=0x%X size=%u (%ux%u pitch=%u) kr=0x%X (%s)\n",
           dstHandle, dstGgtt, dstSize, dW, dH, pitch, kr, kr == KERN_SUCCESS ? "OK" : "FAIL");
    if (kr != KERN_SUCCESS) goto done;

    kr = gemCreate(conn, 0x100000, MYVCS_GEM_SOURCE, &scrHandle, &scrGgtt, &scrSize);
    printf("[GEM] Scratch (row stores): handle=%u ggtt=0x%X size=%u kr=0x%X (%s)\n",
           scrHandle, scrGgtt, scrSize, kr, kr == KERN_SUCCESS ? "OK" : "FAIL");
    if (kr != KERN_SUCCESS) goto done;

    kr = gemCreate(conn, surfSize, MYVCS_GEM_SURFACE, &refHandle, &refGgtt, &refSize);
    printf("[GEM] Reference Surface: handle=%u ggtt=0x%X size=%u kr=0x%X (%s)\n",
           refHandle, refGgtt, refSize, kr, kr == KERN_SUCCESS ? "OK" : "FAIL");
    if (kr != KERN_SUCCESS) goto done;

    kr = gemCreate(conn, 0x40000, MYVCS_GEM_SOURCE, &mvHandle, &mvGgtt, &mvSize);
    printf("[GEM] DirectMV/Temporal: handle=%u ggtt=0x%X size=%u kr=0x%X (%s)\n",
           mvHandle, mvGgtt, mvSize, kr, kr == KERN_SUCCESS ? "OK" : "FAIL");
    if (kr != KERN_SUCCESS) goto done;

    /* map into user-space then copy the real bitstream into the Source Buffer */
    kr = gemMap(conn, srcHandle, &srcVA, &srcMapSz);
    printf("[MAP] Source VA=%p size=%zu kr=0x%X\n", srcVA, srcMapSz, kr);
    if (kr != KERN_SUCCESS) goto done;

    kr = gemMap(conn, dstHandle, &dstVA, &dstMapSz);
    printf("[MAP] Destination VA=%p size=%zu kr=0x%X\n", dstVA, dstMapSz, kr);
    if (kr != KERN_SUCCESS) goto done;

    kr = gemMap(conn, scrHandle, &scrVA, &scrMapSz);
    if (kr != KERN_SUCCESS) goto done;
    kr = gemMap(conn, refHandle, &refVA, &refMapSz);
    if (kr != KERN_SUCCESS) goto done;
    kr = gemMap(conn, mvHandle, &mvVA, &mvMapSz);
    if (kr != KERN_SUCCESS) goto done;

    if (bit) {
        memcpy(srcVA, bit, (size_t)bitLen);
        printf("[UPLOAD] copy bitstream %ld bytes → Source Buffer (GGTT 0x%X)\n",
               bitLen, srcGgtt);
    } else {
        memset(srcVA, 0, srcMapSz);
    }
    /* poison the whole surface with 0xA5 — three cases at read-back:
     *   still 0xA5  = VDBOX never wrote (state/address wrong)
     *   became 0    = VDBOX really wrote but decoded black (coherency OK)
     *   other       = decode succeeded */
    memset(dstVA, 0xA5, dstMapSz);
    memset(scrVA, 0, scrMapSz);   /* row stores, BSP/MPR, and MB status must start zeroed */
    memset(refVA, 0xC3, refMapSz);
    memset(mvVA,  0x3C, mvMapSz);

    /* ── Address-space probe v3 (2.0.251): staged experiments + engineReset ──
     * MI_FLUSH_DW encoding matching real i915 (0x33004006) still got bounced by VDBOX (ESR=1)
     * → isolate the cause step by step, resetting the engine between steps (selector 10), no reboot:
     *   S1: NOOP+BB_END          — baseline: is ring fetch + ctx restore working?
     *   S2: MI_FLUSH_DW STORE_INDEX → PPHWSP scratch (same breadcrumb shape as the real one)
     *   S3: MI_FLUSH_DW WriteImmData, DAT(DW1 bit2)=1 → GGTT (gen125.xml spec!)
     *   S4: MI_FLUSH_DW WriteImmData, DAT=0 → identity-map PPGTT
     *   WARNING (5whys lesson): "USE_GTT" is NOT a DW0 bit — it is DW1 bit2 per gen125.xml
     *      old encoding (0x13004006 + addr without bit2) = accidental PPGTT write */
    {
        uint32_t pr[16];
        uint32_t pn;
        uint64_t ro[8];
        uint32_t roCnt;

        #define DO_RESET(tag) do { \
            roCnt = 8; \
            kr = IOConnectCallScalarMethod(conn, kMyVCSEngineReset, NULL, 0, ro, &roCnt); \
            printf("[RESET %s] kr=0x%X pre: ESR=0x%08X IPEHR=0x%08X HEAD=0x%X TAIL=0x%X CTL=0x%X | post scratch=0x%08X lrcScratch=0x%08X\n", \
                   tag, kr, (uint32_t)ro[0], (uint32_t)ro[1], (uint32_t)ro[2], (uint32_t)ro[3], (uint32_t)ro[4], (uint32_t)ro[5], (uint32_t)ro[6]); \
        } while (0)

        #define DO_SUBMIT(tag) do { \
            while (pn & 3) pr[pn++] = 0; /* MI_NOOP pad — kext rejects non 16B-aligned size (kr=0xE00002C2) */ \
            sOutCnt = 0; \
            kr = IOConnectCallMethod(conn, kMyVCSSubmitVCSWorkload, NULL, 0, pr, pn * 4, sOut, &sOutCnt, NULL, NULL); \
            printf("[PROBE %s] submit kr=0x%X\n", tag, kr); \
            usleep(100000); \
        } while (0)

        /* [S0] reset: clear any stuck halt + diag */
        DO_RESET("S0");

        /* [S1] safe baseline: MI_NOOP×12 + BB_END — must not error, period */
        pn = 0;
        for (int i = 0; i < 12; i++) pr[pn++] = 0;   /* MI_NOOP */
        pr[pn++] = 0x05000000;                        /* MI_BATCH_BUFFER_END */
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        DO_SUBMIT("S1 noop");
        DO_RESET("S1");
        if (ro[0] == 0) printf("[S1] => NOOP/BB_END survived ESR=0 — ring fetch + ctx restore OK\n");
        else printf("[S1] => ESR=0x%X even for NOOP! — context restore broken\n", (uint32_t)ro[0]);

        /* [S2] MI_FLUSH_DW|STOREDW|STORE_INDEX → PPHWSP scratch 0x800 */
        pn = 0;
        pr[pn++] = 0x13204002;   /* MI_FLUSH_DW|STOREDW|STORE_INDEX(1<<21), len 4 */
        pr[pn++] = 0x800;        /* PPHWSP scratch offset */
        pr[pn++] = 0;
        pr[pn++] = 0x77777777;
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        pr[pn++] = 0x05000000;
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        DO_SUBMIT("S2 hwsp");
        DO_RESET("S2");
        if (ro[5] == 0x77777777) printf("[S2] => MI_FLUSH_DW STORE_INDEX worked! MI_FLUSH_DW is legal on VDBOX\n");
        else printf("[S2] => scratch=0x%08X (expected 0x77777777) ESR=0x%X\n", (uint32_t)ro[5], (uint32_t)ro[0]);

        /* [S3 DEBUG] two passes to isolate which one fails */
        /* [S3-A] write dst only to dstGgtt */
        ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
        ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
        printf("[S3-A] dstGgtt=0x%08X scrGgtt=0x%08X\n", dstGgtt, scrGgtt);
        pn = 0;
        /* MI_FLUSH_DW Gen12.5: len=3 → 5 dwords, DAT(DW1 bit2)=1 → GGTT */
        pr[pn++] = 0x13004003; pr[pn++] = dstGgtt | 0x4; pr[pn++] = 0; pr[pn++] = 0x1357BDF0; pr[pn++] = 0;
        pr[pn++] = 0x05000000;
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        DO_SUBMIT("S3-A dst");
        DO_RESET("S3-A");
        uint32_t ma = ((volatile uint32_t *)dstVA)[0];
        uint32_t mb = ((volatile uint32_t *)scrVA)[0];
        printf("[S3-A] dst=0x%08X scr=0x%08X (expect dst=0x1357BDF0 scr=0x5A5A5A5A)\n", ma, mb);

        /* [S3-B] write only scr to scrGgtt */
        ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
        ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
        pn = 0;
        /* MI_FLUSH_DW Gen12.5: len=3 → 5 dwords, DAT(DW1 bit2)=1 → GGTT */
        pr[pn++] = 0x13004003; pr[pn++] = scrGgtt | 0x4; pr[pn++] = 0; pr[pn++] = 0x2468ACE0; pr[pn++] = 0;
        pr[pn++] = 0x05000000;
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        DO_SUBMIT("S3-B scr");
        DO_RESET("S3-B");
        ma = ((volatile uint32_t *)dstVA)[0];
        mb = ((volatile uint32_t *)scrVA)[0];
        printf("[S3-B] dst=0x%08X scr=0x%08X (expect dst=0xA5A5A5A5 scr=0x2468ACE0)\n", ma, mb);

        /* [S3-C] both again */
        ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
        ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
        pn = 0;
        pr[pn++] = 0x13004003; pr[pn++] = dstGgtt | 0x4; pr[pn++] = 0; pr[pn++] = 0x1357BDF0; pr[pn++] = 0;
        pr[pn++] = 0x13004003; pr[pn++] = scrGgtt | 0x4; pr[pn++] = 0; pr[pn++] = 0x2468ACE0; pr[pn++] = 0;
        pr[pn++] = 0x05000000;
        pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
        DO_SUBMIT("S3-C both");
        DO_RESET("S3-C");
        ma = ((volatile uint32_t *)dstVA)[0];
        mb = ((volatile uint32_t *)scrVA)[0];
        printf("[S3-C] dst=0x%08X scr=0x%08X (expect 0x1357BDF0/0x2468ACE0)\n", ma, mb);

        if (ma == 0x1357BDF0 && mb == 0x2468ACE0) {
            printf("[S3] => GGTT write path OK! — only remaining suspect is MFX state/pipeline\n");
        } else {
            /* which one failed? */
            printf("[S3-DIAG] A(dst only): dst=%s  B(scr only): scr=%s  C(both): dst=%s scr=%s\n",
                   ma == 0x1357BDF0 ? "OK" : "FAIL",
                   mb == 0x2468ACE0 ? "OK" : "FAIL",
                   "check above", "check above");

            DO_RESET("S3");
            /* [S4] PPGTT: same shape as S3 but DAT(bit2)=0 → PPGTT identity-map */
            ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
            ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
            pn = 0;
            pr[pn++] = 0x13004003; pr[pn++] = dstGgtt; pr[pn++] = 0; pr[pn++] = 0x1357BDF0; pr[pn++] = 0;
            pr[pn++] = 0x13004003; pr[pn++] = scrGgtt; pr[pn++] = 0; pr[pn++] = 0x2468ACE0; pr[pn++] = 0;
            pr[pn++] = 0x05000000;
            pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0; pr[pn++] = 0;
            DO_SUBMIT("S4 ppgtt");
            ma = ((volatile uint32_t *)dstVA)[0];
            mb = ((volatile uint32_t *)scrVA)[0];
            printf("[S4] dst=0x%08X scr=0x%08X (expect 0x1357BDF0/0x2468ACE0)\n", ma, mb);
            if (ma == 0x1357BDF0 && mb == 0x2468ACE0)
                printf("[S4] => PPGTT write OK — USE_GTT was the problem! use the PPGTT address instead\n");
            else {
                DO_RESET("S4");
                printf("[S4] => no write in any flavor — the problem is context/execution level\n");
            }
        }
        /* [S5] NULL-submit (BBE-only): who writes zeros @+4..7 — infra or MFX?
         * pre-poison dword[0..1] of both buffers, submit an empty stream, read back
         * [1] became zero = the writer is not in the MFX commands at all
         * A/B: only with MFX_S5=1 (default OFF — the reset before MFX may affect VDBOX) */
        if (getenv("MFX_S5")) {
            ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
            ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
            ((volatile uint32_t *)dstVA)[1] = 0xA5A5A5A5;
            ((volatile uint32_t *)scrVA)[1] = 0x5A5A5A5A;
            pn = 0;
            pr[pn++] = 0x05000000;   /* MI_BATCH_BUFFER_END */
            DO_SUBMIT("S5 null");
            printf("[S5-null] dst[0]=%08X dst[1]=%08X | scr[0]=%08X scr[1]=%08X\n",
                   ((volatile uint32_t *)dstVA)[0], ((volatile uint32_t *)dstVA)[1],
                   ((volatile uint32_t *)scrVA)[0], ((volatile uint32_t *)scrVA)[1]);
        }

        /* restore poison so readback classification stays accurate */
        ((volatile uint32_t *)dstVA)[0] = 0xA5A5A5A5;
        ((volatile uint32_t *)scrVA)[0] = 0x5A5A5A5A;
        DO_RESET("final");
        #undef DO_RESET
        #undef DO_SUBMIT
    }

    /* build the command sequence */
    MyIntelVCSCmdBuf cmdBuf;
    vcsCmdInit(&cmdBuf, VCS_CMD_H264_DECODE);

    if (haveSlice) {
        /* Gen12 MFX sequence, complete set (first IDR frame) */
        /* long-format BSD: DW2 points at the NAL header.
         * FirstMbByteOffsetOfSliceDataOrSliceHeader = NAL header + slice header size (byte where slice data begins)
         *   → match media-driver dwOffset (non-Intel entrypoint). Must be sliceHeaderBytes, not 1!
         * FirstMacroblockMbBitOffset = bit offset of the first MB (0-7) inside that byte */

        /* [P4] MI_FORCE_WAKEUP (Gen12): opcode 29 << 23 = 0x0E800000, 2 dwords
         * DW1: bit9=MfxPowerWellControl + bit0=ForceMediaSlice0Awake(VDBOX0)
         *      + MaskBits[31:16]: bit25(mfx)+bit16(slice0) → 0x02010201
         * A/B: only with MFX_WAKEUP=1 — wake VDBOX before PIPE_MODE_SELECT like the real driver does */
        if (getenv("MFX_WAKEUP")) {
            uint32_t fw[2] = { 0x0E800000, 0x02010201 };
            vcsCmdEmitBatch(&cmdBuf, fw, 2);
            printf("[P4] MI_FORCE_WAKEUP injected (slice0+MFX well)\n");
        }
        vcsCmdG12PipeModeSelect(&cmdBuf, false);
        vcsCmdG12SurfaceState(&cmdBuf, cropW, cropH, pitch);
        vcsCmdG12PipeBufAddr(&cmdBuf, dstGgtt, scrGgtt, refGgtt);
        /* [P14] MFX_IAOFF=n: shift IA base by n pages — tests whether HW truly reads the bitstream
         * (identical result with the page pointed elsewhere = fetch path not working) */
        uint32_t iaOff = getenv("MFX_IAOFF") ? strtoul(getenv("MFX_IAOFF"), NULL, 0) * 0x1000u : 0;
        vcsCmdG12IndObj(&cmdBuf, srcGgtt + iaOff, (uint32_t)bitLen, mvGgtt + iaOff, 0x40000);
        /* BSP_BUF: BSD/MPC + MPR row store scratch (picWidthMB*2*64 each) —
         * driver sends this every frame; use the 0xC0000/0xC1000 range of the 1MB scratch */
        vcsCmdG12BspBufBaseAddr(&cmdBuf, scrGgtt + 0xC0000, scrGgtt + 0xC1000);
        vcsCmdG12AvcPicIdState(&cmdBuf);
        vcsCmdG12AvcImgState(&cmdBuf,
            sps.picWidthInMbs, sps.picHeightInMbs,
            pps.entropyCoding, pps.transform8x8, sps.direct8x8Inference,
            sps.chromaFormatIdc, sps.log2MaxFrameNumMinus4, sps.picOrderCntType,
            sps.log2MaxPicOrderCntLsbMinus4, pps.picOrderPresent,
            sps.deltaPicOrderAlwaysZero, 26 + pps.picInitQpMinus26,
            sh.frameNum, 0u, sh.numRefIdxL0Active,
            sh.numRefIdxL1Active, pps.deblockCtrlPresent,
            pps.chromaQpIndexOffset, pps.secondChromaQpIndexOffset);
        /* QM_STATE × 4 (intra4x4/inter4x4/intra8x8/inter8x8, default matrix) */
        /* [P14] MFX_NOQM=1: skip QM x4 — driver only sends them when pic_scaling_matrix_present_flag is set */
        if (getenv("MFX_NOQM")) {
            printf("[P14] NOQM: skipped 4x QM_STATE\n");
        } else {
            vcsCmdG12QmState(&cmdBuf);
        }
        /* [P14] MFX_DMVSEP=1: split the write buffer's upper half — avoids read==write perfect aliasing */
        vcsCmdG12DirectMode(&cmdBuf, mvGgtt,
            getenv("MFX_DMVSEP") ? (mvGgtt + 0x20000) : mvGgtt);
        /* driver: dummy REF_IDX for I-Frame (empty command) — A/B: only with MFX_REFIDX=1 */
        if (getenv("MFX_REFIDX"))
            vcsCmdG12AvcRefIdxDummy(&cmdBuf);
        /* [P13-EMIT] multi-slice: emit SliceState+BSD for every slice of the frame
         * only the last slice gets lastSlice; previous slices' NextSlice points at the next slice's firstMb */
        const char *sq = getenv("MFX_SLICEQP");
        int sqOv = sq ? (int)strtol(sq, NULL, 0) : -1;
        const char *db = getenv("MFX_DEBLOCK");
        int dbOv = db ? (int)strtol(db, NULL, 0) : -1;
        uint32_t nEmit = getenv("MFX_ONESLICE") ? 1u : nFsl;
        for (uint32_t si = 0; si < nEmit; si++) {
            bool isLast = (si == nEmit - 1);
            uint32_t nextFirstMb = isLast ? 0 : fsl[si + 1].firstMb;
            uint32_t effQp = (sqOv >= 0) ? (uint32_t)sqOv : (26 + pps.picInitQpMinus26 + fsl[si].qpDelta);
            if (sqOv >= 0) printf("[P15] SLICEQP override %u (was %u)\n", effQp, 26 + pps.picInitQpMinus26 + fsl[si].qpDelta);
            uint32_t effDb = (dbOv >= 0) ? (uint32_t)dbOv : fsl[si].deblockIdc;
            if (dbOv >= 0) printf("[P15] DEBLOCK override %u (was %u)\n", effDb, fsl[si].deblockIdc);
            vcsCmdG12AvcSliceState(&cmdBuf, fsl[si].sliceType, fsl[si].firstMb,
                sps.picWidthInMbs, sps.picHeightInMbs,
                effQp,
                fsl[si].cabacIdc, effDb,
                fsl[si].alphaDiv2, fsl[si].betaDiv2,
                fsl[si].refL0, fsl[si].refL1, nextFirstMb, isLast);
            /* [P16] BSD_OBJECT DW4 override sweep (env):
             *  MFX_BSDABS=<bits>  → byteOffset=bits>>3, bitOffset=bits&7
             *  MFX_BSDHDR=<bytes> → force FirstMbByteOffset
             *  MFX_BSDBIT=<0..7>  → force FirstMacroblockMbBitOffset */
            uint32_t bsHdr = fsl[si].hdrBytes, bsBit = fsl[si].bitOff;
            {
                const char *ea = getenv("MFX_BSDABS");
                const char *eh = getenv("MFX_BSDHDR");
                const char *eb = getenv("MFX_BSDBIT");
                /* per-slice: slice N byte offset = base + N (header delta = 8 bits exactly) */
                if (ea) { uint32_t ab = (uint32_t)strtol(ea, NULL, 0); bsHdr = (ab >> 3) + si; bsBit = ab & 7u; }
                if (eh) bsHdr = (uint32_t)strtol(eh, NULL, 0) + si;
                if (eb) bsBit = (uint32_t)strtol(eb, NULL, 0);
                if (ea || eh || eb)
                    printf("[P16] BSD override: hdr=%u bit=%u abs=%u\n", bsHdr, bsBit, bsHdr*8+bsBit);
            }
            vcsCmdG12BsdObject(&cmdBuf, fsl[si].nalOff, fsl[si].nalLen,
                bsHdr, bsBit, sliceNalType, isLast);
            printf("[P13-LOOP] Fed Slice %u/%u: firstMb=%u nextFirstMb=%u off=%u len=%u hdr=%u bit=%u last=%d\n",
                   si + 1, nFsl, fsl[si].firstMb, nextFirstMb,
                   fsl[si].nalOff, fsl[si].nalLen, bsHdr, bsBit, isLast);
        }
        printf("[SEQ] G12 Multi-slice sequence injected: total_slices=%u img=%ux%u MB\n",
               nFsl, sps.picWidthInMbs, sps.picHeightInMbs);
    } else {
        /* ── Phase 9 template (regression path) ── */
        vcsCmdMfxPipeModeSelect(&cmdBuf, VCS_CMD_H264_DECODE, true);
        vcsCmdMfxSurfaceState(&cmdBuf, W, H, VCS_SURFACE_NV12, dstGgtt, true);
        uint32_t surfaces[4] = { dstGgtt, srcGgtt, 0, 0 };
        vcsCmdMfxPipeBufAddrState(&cmdBuf, surfaces, 4);
        vcsCmdMfxIndObjBaseAddrState(&cmdBuf, srcGgtt, bit ? (uint32_t)bitLen : 0);
        vcsCmdMfxPicState(&cmdBuf, W, H, 0);
        uint32_t slices = (nal.idr + nal.nonidr) ? (nal.idr + nal.nonidr) : 1;
        if (slices > 8) slices = 8;
        vcsCmdMfxSliceState(&cmdBuf, slices);
    }
    /* trim the trailing flush of the stream */
    vcsCmdBatchEnd(&cmdBuf);
    cmdBytes = cmdBuf.count * 4;
    padDwords = (16 - (cmdBytes % 16)) / 4;
    for (uint32_t i = 0; i < padDwords; i++) vcsCmdEmit(&cmdBuf, 0x00000000);
    cmdBytes = cmdBuf.count * 4;

    printf("[CMDBUF] %u dwords (%u bytes, 16B aligned) src=0x%X dst=0x%X\n",
           cmdBuf.count, cmdBytes, srcGgtt, dstGgtt);
    printf("[CMDBUF] First 8:");
    for (uint32_t i = 0; i < 8 && i < cmdBuf.count; i++) printf(" 0x%08X", cmdBuf.cmds[i]);
    printf("\n");
    /* dump everything — confirms the fixed dwords (MOCS/NextSlice/BSD DW6) really land in the stream */
    for (uint32_t i = 0; i < cmdBuf.count; i += 8) {
        printf("[DW%03u]", i);
        for (uint32_t j = i; j < i + 8 && j < cmdBuf.count; j++)
            printf(" %08X", cmdBuf.cmds[j]);
        printf("\n");
    }

    /* forensic snapshot before the MFX submit — diff finds write positions at dword granularity */
    uint32_t preDst[64], preScr[64];
    for (int i = 0; i < 64; i++) {
        preDst[i] = ((volatile uint32_t *)dstVA)[i];
        preScr[i] = ((volatile uint32_t *)scrVA)[i];
    }

    /* [P7] MFX_BBSTART=1: wrap the MFX command sequence in a Batch Buffer + ring holding just BB_START
     * (every real stack submits media through a BB; inline-in-ring may be ignored by VDBOX) */
    if (getenv("MFX_BBSTART")) {
        uint32_t bbHandle = 0, bbGgtt = 0, bbSize = 0;
        void *bbVA = NULL; size_t bbMapSz = 0;
        kr = gemCreate(conn, 0x1000, MYVCS_GEM_SOURCE, &bbHandle, &bbGgtt, &bbSize);
        printf("[P7] batch GEM handle=%u ggtt=0x%X kr=0x%X\n", bbHandle, bbGgtt, kr);
        if (kr == KERN_SUCCESS && gemMap(conn, bbHandle, &bbVA, &bbMapSz) == KERN_SUCCESS) {
            /* breadcrumb verdict: MI_FLUSH_DW writes a magic before/after MFX — magic appears = batch was executed
             * MFX_BBMODE=1: all-MI batch (crumb+BBE) | 2: crumb+MFX+crumb2 | default: crumb+MFX */
            const char *bmv = getenv("MFX_BBMODE");
            uint32_t bbmode = bmv ? (uint32_t)strtoul(bmv, NULL, 0) : 0;
            uint32_t w = 0;
            /* [scr0] MFX_ALIGN4K=1: round the store target to a 4KB page boundary — tests page-granular store */
            const char *alv = getenv("MFX_ALIGN4K");
            uint32_t crumbAddr = alv ? ((scrGgtt + 0x1000) & ~0xFFFu)
                                     : (scrGgtt + 0x100);
            ((volatile uint32_t *)bbVA)[w++] = 0x13004003;
            ((volatile uint32_t *)bbVA)[w++] = crumbAddr | 0x4;
            ((volatile uint32_t *)bbVA)[w++] = 0;
            ((volatile uint32_t *)bbVA)[w++] = 0xC0DEC0DE;
            ((volatile uint32_t *)bbVA)[w++] = 0;
            if (bbmode != 1) {
                /* [P9] MICROSCOPE: insert a breadcrumb after every command — shows the last point HW reached */
                /* [P14-FIX] copy the ENTIRE stream in one go — do not rely on a fixed boundary table
                 * (the old table {…309} dropped slice2 out of the batch + NOQM read past count!)
                 * MFX_UPTO_DW=n: clamp raw dword index for bisect */
                uint32_t uptoDw = cmdBuf.count;
                /* strip the stream tail honestly: trailing zeros -> BBE
                 * (cmdBuf has zero-pad after BBE which breaks naive conditionals) */
                while (uptoDw && cmdBuf.cmds[uptoDw - 1] == 0) uptoDw--;
                if (uptoDw && cmdBuf.cmds[uptoDw - 1] == 0x05000000u) uptoDw--;
                const char *udw = getenv("MFX_UPTO_DW");
                if (udw) {
                    uint32_t v = (uint32_t)strtoul(udw, NULL, 0);
                    if (v && v < uptoDw) uptoDw = v;
                    printf("[P14] UPTO_DW=%u (full=%u)\n", uptoDw, cmdBuf.count);
                }
                memcpy((void *)((volatile uint32_t *)bbVA + w),
                       cmdBuf.cmds, uptoDw * sizeof(uint32_t));
                w += uptoDw;
                if (bbmode == 2) {
                    ((volatile uint32_t *)bbVA)[w++] = 0x13004003;
                    ((volatile uint32_t *)bbVA)[w++] = (scrGgtt + 0x200) | 0x4;
                    ((volatile uint32_t *)bbVA)[w++] = 0;
                    ((volatile uint32_t *)bbVA)[w++] = 0xFACEF00D;
                    ((volatile uint32_t *)bbVA)[w++] = 0;
                }
            }
            uint32_t total = w;
            ((volatile uint32_t *)bbVA)[total] = 0x0A000000;   /* MI_BATCH_BUFFER_END */
            { volatile uint32_t *v = (volatile uint32_t *)bbVA; for (uint32_t k = 0; k < total + 1; k++) (void)v[k]; } /* write drain */
            /* ring stream: BB_START + addr + BBE + NOOP pad
             * Gen11+ XML: DL=bits[7:0](=1), ASI=bit8 (1=PPGTT), SLB=bit22, op=49 */
            uint32_t rs[8] = {0};
            rs[0] = getenv("MFX_ASI") ? 0x18800001 : 0x18800101;   /* ASI: 1=PPGTT(default) 0=GGTT */
            rs[1] = bbGgtt & ~3u;
            rs[2] = 0x05000000;              /* BBE */
            uint32_t pn7 = 4;                /* 16B aligned */
            kr = IOConnectCallMethod(conn, kMyVCSSubmitVCSWorkload, NULL, 0,
                                     rs, pn7 * sizeof(uint32_t), sOut, &sOutCnt, NULL, NULL);
            printf("[P7] BB_START submit kr=0x%X (%s)\n", kr, kr == KERN_SUCCESS ? "SUCCESS" : "FAIL");
            goto wait_and_readback;
        }
    }

    /* submit through Selector 6 */
    printf("[SUBMIT] selector 6, %u bytes...\n", cmdBytes);
    sOutCnt = 0;
    kr = IOConnectCallMethod(conn, kMyVCSSubmitVCSWorkload, NULL, 0,
                             cmdBuf.cmds, cmdBytes, sOut, &sOutCnt, NULL, NULL);
    printf("[SUBMIT] result=0x%X (%s)\n", kr, kr == KERN_SUCCESS ? "SUCCESS" : "FAILED");

wait_and_readback:
    /* ── [ENGINE-DIAG] VDBOX error state captured IMMEDIATELY after submit ──
     * selector 11 (kMyVCSEngineDiag): read-only, no reset.
     * dout[0]=ESR dout[1]=IPEHR dout[2]=lrcHead dout[3]=lrcTail dout[4]=csbWr
     * dout[5]=csb0lo dout[6]=hwspScratch dout[7]=execStLo dout[8]=nopid
     * dout[9]=mmioHead dout[10]=mmioTail dout[11]=ctl */
    {
        uint64_t dout[12];
        uint32_t doutCnt = 12;
        kern_return_t dkr = IOConnectCallMethod(conn, kMyVCSEngineDiag, NULL, 0, NULL, 0,
                                                dout, &doutCnt, NULL, NULL);
        printf("[ENGINE-DIAG] kr=0x%X n=%u\n", dkr, doutCnt);
        printf("[ENGINE-DIAG] ESR=0x%08X IPEHR=0x%08X\n",
               (uint32_t)dout[0], (uint32_t)dout[1]);
        printf("[ENGINE-DIAG] lrcHead=0x%08X lrcTail=0x%08X csbWr=0x%08X csb0lo=0x%08X\n",
               (uint32_t)dout[2], (uint32_t)dout[3], (uint32_t)dout[4], (uint32_t)dout[5]);
        printf("[ENGINE-DIAG] hwspScratch=0x%08X execStLo=0x%08X nopid=0x%08X mmioHead=0x%08X mmioTail=0x%08X ctl=0x%08X\n",
               (uint32_t)dout[6], (uint32_t)dout[7], (uint32_t)dout[8],
               (uint32_t)dout[9], (uint32_t)dout[10], (uint32_t)dout[11]);
        if ((uint32_t)dout[0] & 1)
            printf("[ENGINE-DIAG] => ESR bit0 SET: engine HALTED on instruction error!\n");
    }

    if (kr == KERN_SUCCESS) {
        for (int i = 0; i < 20; i++) {
            sOutCnt = 1; memset(sOut, 0, sizeof(sOut));
            IOConnectCallMethod(conn, kMyVCSGetVCSStatus, NULL, 0, NULL, 0, sOut, &sOutCnt, NULL, NULL);
            printf("[POLL %d] VCS_CTL=0x%llX\n", i, sOut[0]);
            if (sOut[0] != 0) { printf("[HW] VCS active!\n"); break; }
            usleep(10000);
        }
    }

    /* [P2] tunable timing: MFX_WAIT_MS (default 300) — is decode slower than the poll? */
    {
        const char *wenv = getenv("MFX_WAIT_MS");
        uint32_t waitMs = wenv ? (uint32_t)strtoul(wenv, NULL, 0) : 300;
        printf("\n[WAIT] %ums giving VDBOX time to work...\n", waitMs);
        usleep(waitMs * 1000);
    }
    sOutCnt = 1; memset(sOut, 0, sizeof(sOut));
    IOConnectCallMethod(conn, kMyVCSGetVCSStatus, NULL, 0, NULL, 0, sOut, &sOutCnt, NULL, NULL);
    printf("[FINAL] VCS_CTL=0x%llX\n", sOut[0]);

    /* ── [ENGINE-DIAG post-wait] second snapshot after 300ms — late halt? ── */
    {
        uint64_t dout[12];
        uint32_t doutCnt = 12;
        kern_return_t dkr = IOConnectCallMethod(conn, kMyVCSEngineDiag, NULL, 0, NULL, 0,
                                                dout, &doutCnt, NULL, NULL);
        printf("[ENGINE-DIAG post-wait] kr=0x%X ESR=0x%08X IPEHR=0x%08X lrcHead=0x%08X lrcTail=0x%08X ctl=0x%08X\n",
               dkr, (uint32_t)dout[0], (uint32_t)dout[1],
               (uint32_t)dout[2], (uint32_t)dout[3], (uint32_t)dout[11]);
        if ((uint32_t)dout[0] & 1)
            printf("[ENGINE-DIAG post-wait] => ESR bit0 SET: engine halted with error during decode!\n");
    }

    /* [P3] forensic diff: every dword that changed vs pre-submit (first 64 dwords of dst/scr) */
    {
        int nDst = 0, nScr = 0;
        for (int i = 0; i < 64; i++) {
            uint32_t nowD = ((volatile uint32_t *)dstVA)[i];
            uint32_t nowS = ((volatile uint32_t *)scrVA)[i];
            if (nowD != preDst[i]) { printf("[FORENSICS] dst+%03X %08X → %08X\n", i * 4, preDst[i], nowD); nDst++; }
            if (nowS != preScr[i]) { printf("[FORENSICS] scr+%03X %08X → %08X\n", i * 4, preScr[i], nowS); nScr++; }
        }
        printf("[FORENSICS] dst changed=%d scr changed=%d (first 64 dwords)\n", nDst, nScr);
        { static const char *nm[11] = {"PMS","Surf","BufAddr","IndObj","Bsp","PicId","Img","QM4","DirectMode","Slice","BSD"};
          for (int i = 0; i < 11; i++) {
              uint32_t m = ((volatile uint32_t *)scrVA)[0x400 * (i+1)];
              printf("[P9] %-11s scr+0x%04X = %08X %s\n", nm[i], 0x1000*(i+1), m,
                     m == (0xBEEF0000u | (uint32_t)(i+1)) ? "REACHED ✅" : "—");
          } }

    }

    /* read the Destination Surface back to verify the decode */
    if (dstVA) {
        const uint8_t *px = (const uint8_t *)dstVA;
        uint32_t poisonCnt = 0, zeroCnt = 0, otherCnt = 0;
        size_t probe = dstMapSz < 262144 ? dstMapSz : 262144;
        for (size_t i = 0; i < probe; i++) {
            if (px[i] == 0xA5) poisonCnt++;
            else if (px[i] == 0) zeroCnt++;
            else otherCnt++;
        }
        printf("[READBACK] probe=%zu poison(0xA5)=%u zero=%u other=%u\n",
               probe, poisonCnt, zeroCnt, otherCnt);
        if (poisonCnt == probe) {
            printf("[READBACK] => VDBOX never wrote the surface (wrong state/address, or decode never started)\n");
        } else if (otherCnt > 0 && otherCnt > zeroCnt) {
            printf("[READBACK] => HW wrote real pixels! decode succeeded\n");
        } else {
            printf("[READBACK] => VDBOX overwrote the poison but got 0/black (decode ran, output wrong)\n");
        }
        printf("[READBACK] first 16:");
        for (int i = 0; i < 16; i++) printf(" %02X", px[i]);
        printf("\n");

        /* coherence check: read through the GGTT aperture (selector 3)
         * if the aperture sees pixels but the direct map sees 0 = CPU cache stale
         * (GPU wrote via LLC/DRAM but the CPU read the old L3 line) */
        uint64_t mapIn[2]  = { dstGgtt, surfSize };
        uint64_t mapOut[1] = { 0 };
        uint32_t mapOutCnt = 1;
        kr = IOConnectCallMethod(conn, kMyVCSMapOutputBuffer, mapIn, 2, NULL, 0,
                                 mapOut, &mapOutCnt, NULL, NULL);
        if (kr == KERN_SUCCESS) {
            mach_vm_address_t apVA = 0;
            mach_vm_size_t apSz = 0;
            kr = IOConnectMapMemory(conn, (uint32_t)mapOut[0], mach_task_self(),
                                    &apVA, &apSz, kIOMapAnywhere | kIOMapReadOnly);
            if (kr == KERN_SUCCESS && apVA) {
                const uint8_t *ap = (const uint8_t *)apVA;
                uint32_t apNZ = 0;
                size_t apProbe = apSz < probe ? apSz : probe;
                for (size_t i = 0; i < apProbe; i++) if (ap[i]) apNZ++;
                printf("[READBACK-APERTURE] probe=%zu nonzero=%u (%s)\n",
                       apProbe, apNZ,
                        apNZ ? "aperture sees pixels — CPU cache stale!" :
                               "aperture is 0 too — HW never really wrote");
                printf("[READBACK-APERTURE] first 16:");
                for (int i = 0; i < 16; i++) printf(" %02X", ap[i]);
                printf("\n");
                vm_deallocate(mach_task_self(), (vm_address_t)apVA, apSz);
            } else {
                printf("[READBACK-APERTURE] map failed kr=0x%X\n", kr);
            }
        } else {
            printf("[READBACK-APERTURE] mapOutputBuffer failed kr=0x%X\n", kr);
        }

        /* [H1] discriminator: does the VDBOX see the bitstream through GGTT the same as the CPU map?
         * the CPU memcpy into srcVA must appear via the aperture if PTE/map agree */
        {
            uint64_t mapInS[2]  = { srcGgtt, srcMapSz };
            uint64_t mapOutS[1] = { 0 };
            uint32_t mapOutSCnt = 1;
            kern_return_t krS = IOConnectCallMethod(conn, kMyVCSMapOutputBuffer, mapInS, 2,
                                                    NULL, 0, mapOutS, &mapOutSCnt, NULL, NULL);
            if (krS == KERN_SUCCESS) {
                mach_vm_address_t apSVA = 0;
                mach_vm_size_t apSSz = 0;
                if (IOConnectMapMemory(conn, (uint32_t)mapOutS[0], mach_task_self(),
                                       &apSVA, &apSSz, kIOMapAnywhere | kIOMapReadOnly) == KERN_SUCCESS && apSVA) {
                    const uint8_t *apS = (const uint8_t *)apSVA;
                    const uint8_t *cpu = (const uint8_t *)srcVA;
                    size_t cmpSz = apSSz < srcMapSz ? apSSz : srcMapSz;
                    size_t probe2 = cmpSz;
                    size_t diffs = 0; ssize_t firstDiff = -1;
                    for (size_t i = 0; i < probe2; i++) {
                        if (apS[i] != cpu[i]) { if (firstDiff < 0) firstDiff = (ssize_t)i; diffs++; }
                    }
                    printf("[SRC-COHERENCE] aperture vs CPU (%zu bytes): %s\n",
                           probe2, diffs ? "MISMATCH!" : "MATCH ✅");
                    if (firstDiff >= 0)
                        printf("[SRC-COHERENCE] first diff @+%zd: ap=%02X cpu=%02X\n",
                               firstDiff, apS[firstDiff], cpu[firstDiff]);
                    vm_deallocate(mach_task_self(), (vm_address_t)apSVA, apSSz);
                } else {
                    printf("[SRC-COHERENCE] map failed kr=0x%X\n", kr);
                }
            } else {
                printf("[SRC-COHERENCE] mapOutputBuffer failed kr=0x%X\n", krS);
            }
        }

        if (outPath && otherCnt > 0) {
            FILE *o = fopen(outPath, "wb");
            if (o) {
                size_t dumpSz = dstMapSz < surfSize ? dstMapSz : surfSize;
                fwrite(dstVA, 1, dumpSz, o);
                fclose(o);
                printf("[DUMP] wrote %zu bytes → %s (NV12 %ux%u pitch=%u)\n",
                       dumpSz, outPath, dW, dH, pitch);
            }
        }
    }

    /* pipeline progress: which buffers did VDBOX write? */
    {
        struct { const char *name; const uint8_t *va; size_t sz; uint8_t pat; }
        bufs[3] = {
            { "scratch", (const uint8_t *)scrVA, scrMapSz, 0x5A },
            { "ref    ", (const uint8_t *)refVA, refMapSz, 0xC3 },
            { "directMV", (const uint8_t *)mvVA,  mvMapSz, 0x3C },
        };
        for (int b = 0; b < 3; b++) {
            if (!bufs[b].va) continue;
            uint32_t changed = 0;
            size_t lim = bufs[b].sz < 524288 ? bufs[b].sz : 524288;
            size_t chOff[4] = {0,0,0,0}; uint8_t chVal[4] = {0,0,0,0};
            int nch = 0;
            for (size_t i = 0; i < lim; i++) {
                if (bufs[b].va[i] != bufs[b].pat) {
                    changed++;
                    if (nch < 4) {
                        /* capture the first offset of each changed dword */
                        if (nch == 0 || i >= chOff[nch-1] + 4) {
                            chOff[nch] = i & ~(size_t)3;
                            chVal[nch] = bufs[b].va[i];
                            nch++;
                        }
                    }
                }
            }
            printf("[PIPELINE] %s: %u/%zu bytes changed%s\n",
                   bufs[b].name, changed, lim, changed ? " <- VDBOX wrote!" : "");
            if (nch > 0) {
                printf("[PIPELINE]   changed dwords:");
                for (int k = 0; k < nch; k++)
                    printf(" @+0x%zX=%02X", chOff[k], chVal[k]);
                printf("\n");
            }
        }
        /* MB status / error region (scratch + 0x80000, per DW52 of PIPE_BUF_ADDR) */
        if (scrVA && scrMapSz > 0x80040) {
            const uint32_t *st = (const uint32_t *)((const uint8_t *)scrVA + 0x80000);
            printf("[PIPELINE] MB-status @+0x80000 (64 dwords):\n");
            for (int i = 0; i < 64; i++) {
                if ((i & 7) == 0) printf("  +%03X:", i * 4);
                printf(" %08X", st[i]);
                if ((i & 7) == 7) printf("\n");
            }
        }
        if (scrVA && scrMapSz >= 0xC2100) {
            const uint32_t *bsp = (const uint32_t *)((const uint8_t *)scrVA + 0xC0000);
            const uint32_t *mpr = (const uint32_t *)((const uint8_t *)scrVA + 0xC1000);
            printf("[PIPELINE] BSP @+0xC0000:");
            for (int i = 0; i < 16; i++) printf(" %08X", bsp[i]);
            printf("\n[PIPELINE] MPR @+0xC1000:");
            for (int i = 0; i < 16; i++) printf(" %08X", mpr[i]);
            printf("\n");
        }
    }

    /* ── cleanup ─ */
    gemUnmap(srcHandle, srcVA, srcMapSz);
    gemUnmap(dstHandle, dstVA, dstMapSz);
    gemUnmap(scrHandle, scrVA, scrMapSz);
    gemUnmap(refHandle, refVA, refMapSz);
    gemUnmap(mvHandle,  mvVA,  mvMapSz);
    srcVA = dstVA = NULL;
    gemDestroy(conn, srcHandle);
    gemDestroy(conn, dstHandle);
    gemDestroy(conn, scrHandle);
    gemDestroy(conn, refHandle);
    gemDestroy(conn, mvHandle);

done:
    if (bit) free(bit);
    IOServiceClose(conn);
    printf("[DONE]\n");
    return 0;
}
