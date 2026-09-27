/*
 * gem_test.c — MyIntelGPU IOUserClient userspace test
 *
 * Contract (matches MyIntelGPUClient::externalMethod):
 *   selector 0 = AllocGEM   : scalarInput[0]  = size (bytes)
 *                             scalarOutput[0] = handle (opaque GEM ptr)
 *   selector 1 = MapGTT     : scalarInput[0]  = handle
 *                             scalarOutput[0] = ggttOffset (GGTT page offset)
 *   selector 2 = WriteGEM   : structInput[0]  = handle, structInput[1] = payload
 *                             (writes payload to buffer CPU addr, kernel-side)
 *   selector 3 = SubmitRing : scalarInput[0] = handle,
 *                             scalarInput[1] = taskType (optional, 0=WriteMagic),
 *                             scalarInput[2] = packetData (optional, 0=default magic)
 *                             (stages real MI_STORE_QWORD_IMM + MI_LRI + MI_SRM batch;
 *                              packetData becomes the SDI payload written to buf+0x100)
 *   selector 4 = ReadGEM    : scalarInput[0]  = handle
 *                             scalarOutput[0] = u64 magic read back at buf+0x100
 *   selector 5 = ReadReg    : scalarInput[0]  = handle
 *                             scalarOutput[0] = u64 value read back at buf+0x108
 *                             (CS_GPR0 snapshot — proves MI_LRI loaded the register
 *                              and MI_SRM stored it, i.e. register writes execute)
 *   selector 6 = ReadMMIO   : scalarInput[0]  = MMIO register offset
 *                             scalarOutput[0] = readReg32(offset) zero-extended
 *                             (arbitrary display/GT register read — used to probe
 *                              plane registers e.g. PLANE_CTL_1A 0x70180)
 *
 * Test flow: single-batch proof ([3]-[8]) then multi-batch queue proof
 * ([9]-[11]: MULTI_BATCH buffers, each with a distinct packetData, all
 * submitted back-to-back; every buffer's magic + GPR snapshot is polled).
 *
 * Build:
 *   clang -o gem_test gem_test.c -framework IOKit -framework CoreFoundation
 *
 * Run:
 *   ./gem_test [size_mb]      (default 4 MB)
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

#define GEM_MAGIC        0x12345678DEADBEEFULL
#define REG_VALUE        0xCAFEBABEULL
#define READ_POLL_MS     10
#define READ_POLL_MAX    200   /* 2 s total before declaring timeout */

static const char *kMyIntelGPUClassName = "MyIntelGPU";

int main(int argc, char **argv)
{
    uint32_t sizeMB = 4;
    if (argc > 1) {
        sizeMB = (uint32_t)atoi(argv[1]);
        if (sizeMB == 0 || sizeMB > 256) {
            fprintf(stderr, "size must be 1..256 MB\n");
            return 1;
        }
    }
    uint64_t sizeBytes = (uint64_t)sizeMB * 1024 * 1024;

    /* 1. Find MyIntelGPU service */
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                       IOServiceMatching(kMyIntelGPUClassName));
    if (!service) {
        fprintf(stderr, "FAIL: MyIntelGPU service not found (kext not loaded?)\n");
        return 2;
    }
    printf("[1] MyIntelGPU service found\n");

    /* 2. Open user client */
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: IOServiceOpen = 0x%x\n", kr);
        return 3;
    }
    printf("[2] IOServiceOpen OK (conn=%u)\n", conn);

    /* 3. Selector 0 — AllocGEM */
    uint64_t inAlloc[1]  = { sizeBytes };
    uint64_t outAlloc[1] = { 0 };
    uint32_t outCnt      = 1;
    kr = IOConnectCallMethod(conn, KSELECTOR_ALLOC,
                             inAlloc, 1, NULL, 0,
                             outAlloc, &outCnt, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: AllocGEM = 0x%x\n", kr);
        IOServiceClose(conn);
        return 4;
    }
    uint64_t handle = outAlloc[0];
    printf("[3] AllocGEM OK: size=%llu bytes, handle=%p\n", sizeBytes, (void *)handle);
    if (handle == 0) {
        fprintf(stderr, "FAIL: null handle\n");
        IOServiceClose(conn);
        return 5;
    }

    /* 4. Selector 1 — MapGTT (idempotent: reports existing ggttOffset) */
    uint64_t inMap[1]   = { handle };
    uint64_t outMap[1]  = { 0 };
    outCnt              = 1;
    kr = IOConnectCallMethod(conn, KSELECTOR_MAP,
                             inMap, 1, NULL, 0,
                             outMap, &outCnt, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: MapGTT = 0x%x\n", kr);
        IOServiceClose(conn);
        return 6;
    }
    uint64_t ggttOffset = outMap[0];
    printf("[4] MapGTT OK: ggttOffset=0x%llx (%llu pages)\n",
           ggttOffset, ggttOffset / 4096);

    /* 5. Selector 2 — WriteGEM: push 8-byte payload into buffer CPU addr (kernel-side) */
    uint64_t payload = 0x12345678DEADBEEFULL;
    uint64_t inWrite[2] = { handle, payload };
    kr = IOConnectCallMethod(conn, KSELECTOR_WRITE,
                             NULL, 0, inWrite, sizeof(inWrite),
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: WriteGEM = 0x%x\n", kr);
        IOServiceClose(conn);
        return 8;
    }
    printf("[5] WriteGEM OK: payload 0x%llx written to buffer CPU addr\n", payload);

    /* 6. Selector 3 — SubmitRing: stage real MI_STORE_QWORD_IMM batch, submit via ring.
     *    taskType=0 (WriteMagic), packetData=0 (default magic). */
    uint64_t inSubmit[3] = { handle, 0 /* kMyIntelTaskTypeWriteMagic */, 0 /* default magic */ };
    kr = IOConnectCallMethod(conn, KSELECTOR_SUBMIT,
                             inSubmit, 3, NULL, 0,
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: SubmitRing = 0x%x\n", kr);
        IOServiceClose(conn);
        return 9;
    }
    printf("[6] SubmitRing OK: batch staged at ggtt=0x%llx (ring kick via execlists)\n", ggttOffset);

    /* 7. Poll selector 4 — ReadGEM until the GPU-written magic appears (or timeout) */
    uint64_t inRead[1] = { handle };
    uint64_t outRead[1] = { 0 };
    int readOK = 0;
    for (int i = 0; i < READ_POLL_MAX; i++) {
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_READ,
                                 inRead, 1, NULL, 0,
                                 outRead, &outCnt, NULL, NULL);
        if (kr == KERN_SUCCESS && outRead[0] == GEM_MAGIC) {
            readOK = 1;
            printf("[7] ReadGEM OK: magic 0x%llx at buf+0x100 after %d polls — GPU EXECUTED the batch\n",
                   outRead[0], i + 1);
            break;
        }
        usleep(READ_POLL_MS * 1000);
    }
    if (!readOK) {
        fprintf(stderr, "FAIL: magic not observed after %d polls (last kr=0x%x, val=0x%llx)\n",
                READ_POLL_MAX, kr, outRead[0]);
        IOServiceClose(conn);
        return 10;
    }

    /* 8. Poll selector 5 — ReadReg until CS_GPR0's snapshot (0xCAFEBABE) appears.
     *    Proves the MI_LRI register load executed on the GPU (not just SDI writes). */
    uint64_t outReg[1] = { 0 };
    int regOK = 0;
    for (int i = 0; i < READ_POLL_MAX; i++) {
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_READREG,
                                 inRead, 1, NULL, 0,
                                 outReg, &outCnt, NULL, NULL);
        if (kr == KERN_SUCCESS && outReg[0] == REG_VALUE) {
            regOK = 1;
            printf("[8] ReadReg OK: CS_GPR0 snapshot 0x%llx at buf+0x108 after %d polls — MI_LRI + MI_SRM EXECUTED\n",
                   outReg[0], i + 1);
            break;
        }
        usleep(READ_POLL_MS * 1000);
    }
    if (!regOK) {
        fprintf(stderr, "FAIL: CS_GPR0 snapshot not observed after %d polls (last kr=0x%x, val=0x%llx)\n",
                READ_POLL_MAX, kr, outReg[0]);
        IOServiceClose(conn);
        return 10;
    }

    /* 9. Multi-batch: allocate MULTI_BATCH buffers, submit all with distinct
     *    packetData (proves F8b queue carries unique SDI payload per entry),
     *    then poll every buffer's magic + GPR snapshot. */
#define MULTI_BATCH 4
    uint64_t multiHandle[MULTI_BATCH] = {0};
    uint64_t multiGgtt[MULTI_BATCH]   = {0};
    uint64_t multiMagic[MULTI_BATCH]  = {0};
    for (int b = 0; b < MULTI_BATCH; b++) {
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_ALLOC,
                                 inAlloc, 1, NULL, 0,
                                 outAlloc, &outCnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: MultiAlloc[%d] = 0x%x\n", b, kr);
            IOServiceClose(conn);
            return 11;
        }
        multiHandle[b] = outAlloc[0];
        inMap[0] = multiHandle[b];
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_MAP,
                                 inMap, 1, NULL, 0,
                                 outMap, &outCnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: MultiMap[%d] = 0x%x\n", b, kr);
            IOServiceClose(conn);
            return 11;
        }
        multiGgtt[b] = outMap[0];
        /* Unique nonzero magic per batch: hi dword tag 0xFEED0000, lo dword = index */
        multiMagic[b] = (0xFEED0000ULL << 32) | (uint64_t)(b + 1);
        uint64_t inMulti[3] = { multiHandle[b], 0, multiMagic[b] };
        kr = IOConnectCallMethod(conn, KSELECTOR_SUBMIT,
                                 inMulti, 3, NULL, 0,
                                 NULL, NULL, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: MultiSubmit[%d] = 0x%x\n", b, kr);
            IOServiceClose(conn);
            return 11;
        }
        printf("[9.%d] MultiSubmit OK: batch ggtt=0x%llx packetData=0x%llx queued\n",
               b, multiGgtt[b], multiMagic[b]);
    }

    /* 10. Poll every batch's SDI magic — all MULTI_BATCH must complete */
    for (int b = 0; b < MULTI_BATCH; b++) {
        int ok = 0;
        inRead[0] = multiHandle[b];
        for (int i = 0; i < READ_POLL_MAX; i++) {
            outCnt = 1;
            kr = IOConnectCallMethod(conn, KSELECTOR_READ,
                                     inRead, 1, NULL, 0,
                                     outRead, &outCnt, NULL, NULL);
            if (kr == KERN_SUCCESS && outRead[0] == multiMagic[b]) {
                ok = 1;
                printf("[10.%d] ReadGEM OK: magic 0x%llx at buf+0x100 after %d polls — batch %d EXECUTED\n",
                       b, outRead[0], i + 1, b);
                break;
            }
            usleep(READ_POLL_MS * 1000);
        }
        if (!ok) {
            fprintf(stderr, "FAIL: batch %d magic not observed (last kr=0x%x, val=0x%llx)\n",
                    b, kr, outRead[0]);
            IOServiceClose(conn);
            return 11;
        }
    }

    /* 11. Poll every batch's CS_GPR0 snapshot (MI_LRI + MI_SRM executed per batch) */
    for (int b = 0; b < MULTI_BATCH; b++) {
        int ok = 0;
        inRead[0] = multiHandle[b];
        for (int i = 0; i < READ_POLL_MAX; i++) {
            outCnt = 1;
            kr = IOConnectCallMethod(conn, KSELECTOR_READREG,
                                     inRead, 1, NULL, 0,
                                     outReg, &outCnt, NULL, NULL);
            if (kr == KERN_SUCCESS && outReg[0] == REG_VALUE) {
                ok = 1;
                printf("[11.%d] ReadReg OK: CS_GPR0 0x%llx at buf+0x108 after %d polls\n",
                       b, outReg[0], i + 1);
                break;
            }
            usleep(READ_POLL_MS * 1000);
        }
        if (!ok) {
            fprintf(stderr, "FAIL: batch %d GPR snapshot not observed (last kr=0x%x, val=0x%llx)\n",
                    b, kr, outReg[0]);
            IOServiceClose(conn);
            return 11;
        }
    }

    /* 13. BcsBlit self-copy: taskType=1, packetData = dst GGTT (page 1 of this
     *      buffer). Client stages FAST_COPY: src = page 0 (the staged batch cmd
     *      page), dst = page 1. After exec page1[0:8] == batch dwords
     *      [0x50800008 (FAST_COPY hdr), 0x03001000 (BLT_DEPTH_32|PAGE_SIZE)].
     *      ReadGEM(offset=0x1000) proves the blitter actually moved the page. */
    printf("[13] BcsBlit self-copy: page0 -> page1 (dst ggtt=0x%llx)...\n",
           ggttOffset + 0x1000);
    uint64_t bcsDst = ggttOffset + 0x1000;
    uint64_t inBcs[3] = { handle, 1 /* kMyIntelTaskTypeBcsBlit */, bcsDst };
    kr = IOConnectCallMethod(conn, KSELECTOR_SUBMIT,
                             inBcs, 3, NULL, 0,
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: BcsSubmit = 0x%x\n", kr);
        IOServiceClose(conn);
        return 12;
    }
    uint64_t expectPage1 = 0x0300100050800008ULL;
    uint64_t outBcs[1] = { 0 };
    int bcsOK = 0;
    for (int i = 0; i < READ_POLL_MAX; i++) {
        uint64_t inReadOff[2] = { handle, 0x1000 };
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_READ,
                                 inReadOff, 2, NULL, 0,
                                 outBcs, &outCnt, NULL, NULL);
        if (kr == KERN_SUCCESS && outBcs[0] == expectPage1) {
            bcsOK = 1;
            printf("[13] ReadGEM(off=0x1000) OK: page1[0]=0x%llx after %d polls — FAST_COPY EXECUTED\n",
                   outBcs[0], i + 1);
            break;
        }
        usleep(READ_POLL_MS * 1000);
    }
    if (!bcsOK) {
        fprintf(stderr, "FAIL: BCS page1 not copied (last kr=0x%x, val=0x%llx, expect=0x%llx)\n",
                kr, outBcs[0], expectPage1);
        IOServiceClose(conn);
        return 12;
    }

    /* 14. MiMath: taskType=2, packetData = (B<<32)|A. Client stages LRI GPR0=A,
     *      LRI GPR1=B, MI_MATH ADD, SRM GPR0 -> buf+0x108. ReadReg(offset=0x108)
     *      proves the ALU executed: expect low 32 bits = A+B (SRM writes 4 bytes;
     *      high dword at buf+0x10C is untouched, so mask).
     *      NOTE: use a DISTINCT operand, not 0xcafebabe — test [8]/[11] already
     *      left 0xcafebabe at buf+0x108, so a stale read is indistinguishable
     *      from a no-op math. 0xC0DEC0DE+2 = 0xC0DEC0E0 (no carry, unique). */
    uint64_t mmA = 0xC0DEC0DEULL, mmB = 2ULL;
    uint64_t mmSum = (mmA + mmB) & 0xFFFFFFFFULL;
    printf("[14] MiMath: A=0x%llx B=0x%llx -> expect 0x%llx at buf+0x108...\n",
           mmA, mmB, mmSum);
    uint64_t inMm[3] = { handle, 2 /* kMyIntelTaskTypeMiMath */, (mmB << 32) | mmA };
    kr = IOConnectCallMethod(conn, KSELECTOR_SUBMIT,
                             inMm, 3, NULL, 0,
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: MiMathSubmit = 0x%x\n", kr);
        IOServiceClose(conn);
        return 13;
    }
    uint64_t outMm[1] = { 0 };
    int mmOK = 0;
    for (int i = 0; i < READ_POLL_MAX; i++) {
        uint64_t inReadOff[2] = { handle, 0x108 };
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_READREG,
                                 inReadOff, 2, NULL, 0,
                                 outMm, &outCnt, NULL, NULL);
        if (kr == KERN_SUCCESS && (outMm[0] & 0xFFFFFFFFULL) == mmSum) {
            mmOK = 1;
            printf("[14] ReadReg(off=0x108) OK: 0x%llx after %d polls — MI_MATH ADD EXECUTED\n",
                   outMm[0], i + 1);
            break;
        }
        usleep(READ_POLL_MS * 1000);
    }
    if (!mmOK) {
        fprintf(stderr, "FAIL: MiMath sum not observed (last kr=0x%x, val=0x%llx, expect=0x%llx)\n",
                kr, outMm[0], mmSum);
        IOServiceClose(conn);
        return 13;
    }

    /* 15. PipeControl: taskType=3, magic = packetData[31:0]. Client stages
     *      PIPE_CONTROL HDC flush + QW_WRITE(magic) -> buf+0x100. ReadGEM
     *      proves the pipeline flush + post-sync write executed. */
    uint64_t pcMagic = 0xDEADBEEFULL;
    printf("[15] PipeControl: QW_WRITE 0x%llx -> buf+0x100...\n", pcMagic);
    uint64_t inPc[3] = { handle, 3 /* kMyIntelTaskTypePipeControl */, pcMagic };
    kr = IOConnectCallMethod(conn, KSELECTOR_SUBMIT,
                             inPc, 3, NULL, 0,
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: PipeControlSubmit = 0x%x\n", kr);
        IOServiceClose(conn);
        return 14;
    }
    uint64_t outPc[1] = { 0 };
    int pcOK = 0;
    uint64_t inReadPc[1] = { handle };   /* must use original buffer, not last multiHandle */
    for (int i = 0; i < READ_POLL_MAX; i++) {
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_READ,
                                 inReadPc, 1, NULL, 0,
                                 outPc, &outCnt, NULL, NULL);
        if (kr == KERN_SUCCESS && outPc[0] == pcMagic) {
            pcOK = 1;
            printf("[15] ReadGEM OK: 0x%llx after %d polls — PIPE_CONTROL QW_WRITE EXECUTED\n",
                   outPc[0], i + 1);
            break;
        }
        usleep(READ_POLL_MS * 1000);
    }
    if (!pcOK) {
        fprintf(stderr, "FAIL: PipeControl magic not observed (last kr=0x%x, val=0x%llx, expect=0x%llx)\n",
                kr, outPc[0], pcMagic);
        IOServiceClose(conn);
        return 14;
    }

    /* 16. ReadMMIO: probe plane scanout registers via selector 6.
     *      PLANE_CTL_1A (0x70180), PLANE_SURF_1A (0x7019C), PLANE_SURFLIVE
     *      (0x701AC) — proves the client can read arbitrary display MMIO. */
    {
        uint64_t regs[3] = { 0x70180, 0x7019C, 0x701AC };
        const char *names[3] = { "PLANE_CTL_1A", "PLANE_SURF_1A", "PLANE_SURFLIVE" };
        printf("[16] ReadMMIO plane probe:\n");
        for (int i = 0; i < 3; i++) {
            uint64_t inReg[1] = { regs[i] };
            uint64_t outReg[1] = { 0 };
            outCnt = 1;
            kr = IOConnectCallMethod(conn, KSELECTOR_READMMIO,
                                     inReg, 1, NULL, 0,
                                     outReg, &outCnt, NULL, NULL);
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "FAIL: ReadMMIO(0x%llx) = 0x%x\n", regs[i], kr);
                IOServiceClose(conn);
                return 15;
            }
            printf("      %s (0x%llx) = 0x%08llx\n", names[i], regs[i], outReg[0] & 0xFFFFFFFFULL);
        }
    }

    /* 17. ProgramPlane: allocate a dedicated 1920x1080x4 framebuffer
     *      (8,294,400 B — bigger than the default 4MB test buffer), bind it
     *      to Pipe A plane 1 via selector 7, then poll PLANE_SURFLIVE
     *      (0x701AC) until the HW latches ggttOffset — proves the plane now
     *      scans out OUR framebuffer instead of the BIOS stub. */
    {
        uint64_t fbSize = 1920 * 1080 * 4;   /* 8,294,400 B */
        uint64_t inAlloc2[1] = { fbSize };
        uint64_t outAlloc2[1] = { 0 };
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_ALLOC,
                                 inAlloc2, 1, NULL, 0,
                                 outAlloc2, &outCnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: AllocGEM(fb) = 0x%x\n", kr);
            IOServiceClose(conn);
            return 17;
        }
        uint64_t fbHandle = outAlloc2[0];
        printf("[17] AllocGEM(fb): %llu bytes, handle=%p\n", fbSize, (void *)fbHandle);

        uint64_t inMap2[1] = { fbHandle };
        uint64_t outMap2[1] = { 0 };
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_MAP,
                                 inMap2, 1, NULL, 0,
                                 outMap2, &outCnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: MapGTT(fb) = 0x%x\n", kr);
            IOServiceClose(conn);
            return 17;
        }
        uint64_t fbGGTT = outMap2[0];
        printf("[17] MapGTT(fb): ggttOffset = 0x%llx\n", fbGGTT);

        uint64_t inProg[1] = { fbHandle };
        uint64_t outProg[1] = { 0 };
        outCnt = 1;
        kr = IOConnectCallMethod(conn, KSELECTOR_PROGRAMPLANE,
                                 inProg, 1, NULL, 0,
                                 outProg, &outCnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL: ProgramPlane = 0x%x\n", kr);
            IOServiceClose(conn);
            return 17;
        }
        printf("[17] ProgramPlane: plane 1A -> GGTT 0x%llx (latched=%llu)\n",
               fbGGTT, outProg[0]);

        int latched = 0;
        uint64_t liveVal = 0;
        for (int i = 0; i < READ_POLL_MAX; i++) {
            uint64_t inLive[1] = { 0x701AC };
            uint64_t outLive[1] = { 0 };
            outCnt = 1;
            kr = IOConnectCallMethod(conn, KSELECTOR_READMMIO,
                                     inLive, 1, NULL, 0,
                                     outLive, &outCnt, NULL, NULL);
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "FAIL: ReadMMIO(SURFLIVE) = 0x%x\n", kr);
                IOServiceClose(conn);
                return 17;
            }
            liveVal = outLive[0] & 0xFFFFFFFFULL;
            if ((liveVal & ~0xFFFULL) == (fbGGTT & ~0xFFFULL)) { latched = 1; break; }
            usleep(READ_POLL_MS * 1000);
        }
        if (!latched) {
            fprintf(stderr, "FAIL: SURFLIVE never latched (last=0x%llx, expect=0x%llx)\n",
                    liveVal, fbGGTT);
            IOServiceClose(conn);
            return 17;
        }
        printf("[17] PLANE_SURFLIVE = 0x%08llx — plane 1A SCANNING our framebuffer\n", liveVal);
    }

    /* 18. Sanity checks */
    int pass = 1;
    if (ggttOffset == 0) {
        fprintf(stderr, "FAIL: ggttOffset == 0 (not bound to GGTT)\n");
        pass = 0;
    }
    if ((ggttOffset & 0xFFF) != 0) {
        fprintf(stderr, "FAIL: ggttOffset 0x%llx not 4K-aligned\n", ggttOffset);
        pass = 0;
    }

    IOServiceClose(conn);
    printf(pass ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return pass ? 0 : 7;
}
