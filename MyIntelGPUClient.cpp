#include "MyIntelGPUClient.hpp"
#include "MyIntelGEMBuffer.hpp"
#include "MyIntelRing.hpp"   /* emitBcsBlitCopy / emitMiMathProof / emitPipeControlFlush */

OSDefineMetaClassAndStructors(MyIntelGPUClient, IOUserClient)

bool MyIntelGPUClient::initWithTask(task_t owningTask, void * securityToken, UInt32 type) {
    if (!IOUserClient::initWithTask(owningTask, securityToken, type)) return false;
    fClientTask = owningTask;
    fProvider   = NULL;
    fBufferCount = 0;
    for (uint32_t i = 0; i < kMaxClientBuffers; i++) fBuffers[i] = NULL;
    fDirtyRingMD = NULL;
    fDirtyRingMap = NULL;
    fDirtyRingUserVA = 0;
    return true;
}

bool MyIntelGPUClient::start(IOService* provider) {
    fProvider = OSDynamicCast(MyIntelGPU, provider);
    if (!fProvider) return false;
    fProvider->retain();   /* hold provider so kext unload can't free it under us */
    if (!IOUserClient::start(provider)) {
        fProvider->release();
        fProvider = NULL;
        return false;
    }
    return true;
}

void MyIntelGPUClient::stop(IOService* provider) {
    IOLog("MyIntelGPU::[stop] entered (fBufferCount=%u)\n", fBufferCount);
    /* Restore any plane bound to one of our buffers BEFORE destroying it —
     * otherwise the plane keeps scanning freed GGTT and the panel blacks out.
     * (gem_test ProgramPlane / selector 7 leaves plane 1A on the fb buffer.) */
    if (fProvider) {
        for (uint32_t i = 0; i < fBufferCount; i++) {
            if (fBuffers[i] && fBuffers[i]->planeBound) {
                IOLog("MyIntelGPU::[stop] restoring plane for buffer %u\n", i);
                fProvider->restorePlane(fBuffers[i]);
            }
        }
    }
    /* Destroy every GEM buffer this client allocated (leak fix: 4MB/alloc) */
    for (uint32_t i = 0; i < fBufferCount; i++) {
        gemBufferDestroy(fBuffers[i], fProvider ? (uint32_t*)fProvider->fGsm : NULL,
                         &MyIntelGPU::ggttInvalidateTrampoline, fProvider);
        fBuffers[i] = NULL;
    }
    fBufferCount = 0;
    if (fDirtyRingMap) { fDirtyRingMap->release(); fDirtyRingMap = NULL; }
    if (fDirtyRingMD) { fDirtyRingMD->complete(); fDirtyRingMD->release(); fDirtyRingMD = NULL; }
    fDirtyRingUserVA = 0;
    /* PTEs for the destroyed buffers were cleared — flush the GGTT TLB so no
     * stale mapping survives (i915 flushes after every PTE write). */
    if (fProvider) fProvider->ggttInvalidate();
    IOUserClient::stop(provider);
}

void MyIntelGPUClient::free() {
    /* free() is always called when the client object is destroyed — unlike
     * stop() which may be skipped by IOKit depending on retain count.
     * Restore any plane-bound buffer here as the definitive cleanup path. */
    if (fDirtyRingMap) { fDirtyRingMap->release(); fDirtyRingMap = NULL; }
    if (fDirtyRingMD) { fDirtyRingMD->complete(); fDirtyRingMD->release(); fDirtyRingMD = NULL; }
    fDirtyRingUserVA = 0;
    if (fProvider) {
        for (uint32_t i = 0; i < fBufferCount; i++) {
            if (fBuffers[i] && fBuffers[i]->planeBound) {
                IOLog("MyIntelGPU::[free] restoring plane for buffer %u\n", i);
                fProvider->restorePlane(fBuffers[i]);
            }
        }
        fProvider->release();
        fProvider = NULL;
    }
    IOUserClient::free();
}

IOReturn MyIntelGPUClient::clientClose() {
    IOLog("MyIntelGPU::[clientClose] terminating client\n");
    /* IOServiceClose must tear down the client so stop()/free() run and
     * restorePlane() can hand the scanout back to the BIOS framebuffer.
     * The IOUserClient default returns kIOReturnUnsupported, which makes
     * IOServiceClose a silent no-op — the plane then keeps scanning a
     * freed GEM buffer and the panel blacks out. */
    if (!isInactive()) terminate();
    return kIOReturnSuccess;
}

IOReturn MyIntelGPUClient::externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
                                         IOExternalMethodDispatch * dispatch, OSObject * target, void * reference) {
    if (!fProvider) return kIOReturnNotReady;

    switch (selector) {
        case 0: { // AllocGEM: input[0] = size -> output[0] = handle
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            if (fBufferCount >= kMaxClientBuffers) return kIOReturnNoSpace;

            uint32_t size = (uint32_t)arguments->scalarInput[0];

            // GEM_FLAG_CPU_READ (0x1) | GEM_FLAG_CPU_WRITE (0x2) — kernel-side allocation
            MyIntelGEMBuffer* buf = gemBufferCreate(size, GEM_FLAG_CPU_READ | GEM_FLAG_CPU_WRITE,
                                                  (uint32_t*)fProvider->fGsm,
                                                  fProvider->fGttTotal,
                                                  (void*)fProvider->fApertureVA,
                                                  fProvider->fApertureSize,
                                                  &MyIntelGPU::ggttInvalidateTrampoline,
                                                  fProvider);

            if (!buf) return kIOReturnNoMemory;

            fBuffers[fBufferCount++] = buf;   /* register handle (destroyed in stop()) */
            arguments->scalarOutput[0] = (uint64_t)buf;
            IOLog("MyIntelGPU::[IPC] GEM buffer allocated: %u bytes, handle %p\n", size, buf);
            return kIOReturnSuccess;
        }

        case 1: { // MapGTT: input[0] = handle -> output[0] = ggttOffset
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            MyIntelGEMBuffer* buf = (MyIntelGEMBuffer*)arguments->scalarInput[0];
            bool found = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {   /* pointer compare only — never deref */
                if (fBuffers[i] == buf) { found = true; break; }
            }
            if (!found) return kIOReturnBadArgument;
            if (buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;   /* defense-in-depth */
            /* Invalidate-by-contract: MAPPED is set only AFTER PTE writes +
             * barrier + ggttInvalidate() completed inside bind. A buffer
             * without MAPPED must NOT be handed to the plane/engine —
             * the hardware translation cache may still hold stale PTEs. */
            if (!(buf->state & GEM_STATE_MAPPED)) return kIOReturnNotReady;

            arguments->scalarOutput[0] = (uint64_t)buf->ggttOffset;
            return kIOReturnSuccess;
        }

        case 2: { // WriteGEM: structureInput = { handle, payload } -> write 8B payload to buffer CPU addr
            if (!arguments->structureInput ||
                arguments->structureInputSize < sizeof(uint64_t) * 2) return kIOReturnBadArgument;

            const uint64_t *data = (const uint64_t *)arguments->structureInput;
            MyIntelGEMBuffer *target_buf = (MyIntelGEMBuffer *)data[0];
            uint64_t payload = data[1];

            // Security: pointer-compare against per-client registry — never deref a
            // userspace-supplied pointer until it matches an entry we handed out.
            bool verified = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {
                if (fBuffers[i] == target_buf) { verified = true; break; }
            }
            if (!verified) return kIOReturnBadArgument;
            if (target_buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;   /* defense-in-depth */

            if (target_buf->cpuAddr) {
                /* IOMallocAligned backing — direct 8B write stays within the 4K-aligned 4MB alloc */
                uint64_t *kernel_mem_ptr = (uint64_t *)target_buf->cpuAddr;
                *kernel_mem_ptr = payload;
                IOLog("MyIntelGPU::[IPC Ring Check] payload written to GPU memory: %p <- 0x%llx\n",
                      target_buf->cpuAddr, payload);
                return kIOReturnSuccess;
            }

            return kIOReturnError;
        }

        case 3: { // SubmitRing: input[0] = handle -> stage real batch into buffer, submit via ring
            if (arguments->scalarInputCount < 1) return kIOReturnBadArgument;

            MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)arguments->scalarInput[0];

            // Security: pointer-compare against per-client registry — never deref a
            // userspace-supplied pointer until it matches an entry we handed out.
            bool verified = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {
                if (fBuffers[i] == buf) { verified = true; break; }
            }
            if (!verified) return kIOReturnBadArgument;
            if (buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;   /* defense-in-depth */

            /* Stage a REAL batch: [MI_STORE_QWORD_IMM_GEN8 → buf+0x100,
             * MI_NOOP, MI_LRI(1) CS_GPR0=0xCAFEBABE, MI_SRM_GEN8 CS_GPR0→buf+0x108,
             * MI_BATCH_BUFFER_END, NOOP pad]. The BB_START jump is emitted by the
             * ring's kick set 2C (pendingBatchGGTT); the batch writes magic
             * 0x12345678DEADBEEF into GPU memory at offset +0x100 AND loads a
             * value into a scratch command-streamer register (CS_GPR0) then
             * stores it back to +0x108 — the CPU reads both back via selector 4
             * (+0x100, SDI proof) and selector 5 (+0x108, LRI+SRM proof) to
             * PROVE the GPU actually executed the commands (not just fetched).
             *
             * Encoding (verified vs linux i915 gt/intel_gpu_commands.h + selftests/i915_perf.c):
             *   MI_STORE_QWORD_IMM_GEN8 = MI_INSTR(0x20, 3) | REG_BIT(21) = 0x10200003
             *     - bits[7:0]  = length = 3 → total 5 dwords
             *     - bit 21     = qword element (2 data dwords)
             *     - bit 22     = MI_GLOBAL_GTT — 0 = PPGTT (identity map, same as BB_START)
             *   MI_NOOP = 0x00000000 — REQUIRED before the MI_LRI (i915 comment:
             *     "always issue a MI_NOOP before the MI_LOAD_REGISTER_IMM -
             *     otherwise hw simply ignores the register load").
             *   MI_LOAD_REGISTER_IMM(1) = 0x11000001 (3 dw: hdr + reg + value)
             *     - plain LRI, NO bit19 — that bit is only OR'd in LRC ctx-restore
             *       images; i915 selftest emits plain LRI in batches (i915_perf.c:342)
             *   CS_GPR0 = RENDER_RING_BASE(0x2000) + 0x600 = 0x2600 — scratch
             *     register, zero side effects (i915 perf noa_wait uses it exactly
             *     like this: LRI magic → SRM readback).
             *   MI_STORE_REGISTER_MEM_GEN8 = MI_INSTR(0x24, 2) = 0x12000002
             *     (4 dw: hdr + reg + addr_lo + addr_hi; bit22=0 → PPGTT identity)
             *   DW0-DW5   SDI magic → +0x100            (5 + 1 NOOP)
             *   DW6-DW8   LRI CS_GPR0 = 0xCAFEBABE      (3)
             *   DW9-DW12  SRM CS_GPR0 → +0x108          (4)
             *   DW13      MI_BATCH_BUFFER_END           (1)
             *   DW14-15   NOOP pad → 16 dwords = 64B, qword-aligned */
            uint32_t taskType   = 0;
            uint64_t packetData = 0;
            if (arguments->scalarInputCount >= 2) taskType = (uint32_t)arguments->scalarInput[1];
            if (arguments->scalarInputCount >= 3) packetData = arguments->scalarInput[2];

            uint32_t *cmd = (uint32_t *)buf->cpuAddr;
            if (!cmd) return kIOReturnError;
            if (buf->size < 0x200) return kIOReturnError;   /* need room for cmd + magic + reg slots */

            /* Stage per taskType: WriteMagic keeps the legacy 16-dw proof
             * (encoding docs above); the new task types use the MyIntelRing
             * emitters — their layouts are verified vs the i915 clone. */
            uint32_t dwCount = 0;
            switch (taskType) {
            case kMyIntelTaskTypeWriteMagic: {
                /* SDI payload = caller's packetData (scalarInput[2]); 0 keeps
                 * the default magic so legacy gem_test magic checks pass. */
                uint64_t sdiMagic = packetData ? packetData : 0x12345678DEADBEEFULL;
                uint32_t storeAddr  = buf->ggttOffset + 0x100;  /* SDI target within same buffer */
                uint32_t regAddr    = buf->ggttOffset + 0x108;  /* SRM target (8B after SDI magic) */
                cmd[0] = 0x10200003;          /* MI_STORE_QWORD_IMM_GEN8 (5 dwords) */
                cmd[1] = storeAddr;           /* address low — identity PPGTT VA */
                cmd[2] = 0;                   /* address high */
                cmd[3] = (uint32_t)(sdiMagic & 0xFFFFFFFFULL);  /* data low = packetData[31:0] */
                cmd[4] = (uint32_t)(sdiMagic >> 32);            /* data high = packetData[63:32] */
                cmd[5] = 0x00000000;          /* MI_NOOP — REQUIRED before MI_LRI */
                cmd[6] = 0x11000001;          /* MI_LOAD_REGISTER_IMM(1) */
                cmd[7] = 0x00002600;          /* CS_GPR0 (RCS scratch) */
                cmd[8] = 0xCAFEBABE;          /* value loaded into CS_GPR0 */
                cmd[9] = 0x12000002;          /* MI_STORE_REGISTER_MEM_GEN8 */
                cmd[10] = 0x00002600;         /* CS_GPR0 */
                cmd[11] = regAddr;            /* addr_lo — identity PPGTT VA */
                cmd[12] = 0;                  /* addr_hi */
                cmd[13] = 0x05000000;         /* MI_BATCH_BUFFER_END */
                cmd[14] = 0x00000000;         /* NOOP pad */
                cmd[15] = 0x00000000;         /* NOOP pad — 16 dw = 64B qword-aligned */
                dwCount = 16;
                break;
            }
            case kMyIntelTaskTypeBcsBlit: {
                /* packetData = GGTT identity address of the destination buffer
                 * (must be MapGTT'd first); source = this buffer's page.
                 * Copies one page src → dst through the BCS blitter. */
                dwCount = emitBcsBlitCopy(cmd, (uint32_t)(packetData & 0xFFFFFFFFULL),
                                          buf->ggttOffset, GEM_PAGE_SIZE);
                break;
            }
            case kMyIntelTaskTypeMiMath: {
                /* packetData[31:0] = operand A, [63:32] = operand B; sum is
                 * stored at buf+0x108 (ReadReg selector proves ALU executed). */
                uint32_t opA = (uint32_t)(packetData & 0xFFFFFFFFULL);
                uint32_t opB = (uint32_t)(packetData >> 32);
                if (packetData == 0) { opA = 0xCAFEBABE; opB = 1; }
                dwCount = emitMiMathProof(cmd, buf->ggttOffset + 0x108, opA, opB);
                break;
            }
            case kMyIntelTaskTypePipeControl: {
                /* magic = packetData[31:0]; QW_WRITE stores it at buf+0x100
                 * (ReadGEM selector proves the pipeline flush executed). */
                uint32_t magic = (uint32_t)(packetData & 0xFFFFFFFFULL);
                if (magic == 0) magic = 0xCAFEBABE;
                dwCount = emitPipeControlFlush(cmd, buf->ggttOffset + 0x100, magic);
                break;
            }
            case kMyIntelTaskTypeBreadcrumb: {
                /* packetData = seqno to write to HWSP; default 0xBEEF0001 */
                uint32_t seqno = (uint32_t)(packetData & 0xFFFFFFFFULL);
                if (seqno == 0) seqno = 0xBEEF0001;
                MyIntelRing *rcs = fProvider ? fProvider->getRingRCS() : NULL;
                uint32_t hwspGtt = rcs ? rcs->hwspGgtt : 0;
                if (!hwspGtt) hwspGtt = buf->ggttOffset;
                dwCount = emitBreadcrumbSeqno(cmd, hwspGtt, seqno);
                break;
            }
            default:
                return kIOReturnBadArgument;
            }
            if (dwCount == 0) return kIOReturnError;

            if (verified && buf) {
                /* Queue through the RCS execlists path instead of poking
                 * RING_TAIL (0x2030) directly: the ring appends its own
                 * flush+NOOP+BB_START+USER_INTERRUPT set under the engine
                 * lock and submits via ELSP — same path the IRQ workloop
                 * uses. The batch is mapped into the PPGTT identity window
                 * and jumped to via BB_START.
                 *
                 * scalarInput[0] = buffer handle
                 * scalarInput[1] = taskType  (optional, default WriteMagic)
                 * scalarInput[2] = packetData (optional, 0 = default magic) */
                IOLog("MyIntelGPUClient: [RING_SUBMIT] staging handle 0x%llX (GGTT 0x%X) taskType=%u via submitClientTaskViaRing\n",
                      (uint64_t)buf, buf->ggttOffset, taskType);
                return fProvider->submitClientTaskViaRing(buf, taskType, packetData);
            }
            return kIOReturnSuccess;
        }

        case 4: { // ReadGEM: input[0] = handle, input[1] = optional byte offset (default 0x100)
            //    -> output[0] = u64 at buf->cpuAddr + offset
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)arguments->scalarInput[0];

            bool verified = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {
                if (fBuffers[i] == buf) { verified = true; break; }
            }
            if (!verified) return kIOReturnBadArgument;
            if (buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;

            /* Optional byte offset (default 0x100, the WriteMagic slot). The
             * BcsBlit self-copy test reads page 1 at offset 0x1000. */
            uint64_t off = (arguments->scalarInputCount >= 2)
                         ? arguments->scalarInput[1] : 0x100;
            if (off + sizeof(uint64_t) > buf->size) return kIOReturnBadArgument;

            /* GPU wrote via PPGTT identity window — flush stale CPU cache lines
             * before reading so we observe the GPU's write, not the pre-batch
             * contents. */
            uint8_t *p = (uint8_t *)buf->cpuAddr + off;
            __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
            __asm__ volatile("mfence" ::: "memory");
            arguments->scalarOutput[0] = *(volatile uint64_t *)p;
            return kIOReturnSuccess;
        }

        case 5: { // ReadReg: input[0] = handle, input[1] = optional byte offset (default 0x108)
            //    -> output[0] = u64 at buf->cpuAddr + offset
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)arguments->scalarInput[0];

            bool verified = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {
                if (fBuffers[i] == buf) { verified = true; break; }
            }
            if (!verified) return kIOReturnBadArgument;
            if (buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;

            /* Optional byte offset (default 0x108, the CS_GPR0 SRM slot). */
            uint64_t off = (arguments->scalarInputCount >= 2)
                         ? arguments->scalarInput[1] : 0x108;
            if (off + sizeof(uint64_t) > buf->size) return kIOReturnBadArgument;

            /* The GPU stored CS_GPR0 via MI_STORE_REGISTER_MEM — flush stale
             * CPU cache lines before reading, same as selector 4. */
            uint8_t *p = (uint8_t *)buf->cpuAddr + off;
            __asm__ volatile("clflush (%0)" :: "r"(p) : "memory");
            __asm__ volatile("mfence" ::: "memory");
            arguments->scalarOutput[0] = *(volatile uint64_t *)p;
            return kIOReturnSuccess;
        }

        case 6: { // ReadMMIO: input[0] = MMIO register offset
            //    -> output[0] = readReg32(offset)
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            uint32_t reg = (uint32_t)arguments->scalarInput[0];
            arguments->scalarOutput[0] = fProvider->readReg32(reg);
            return kIOReturnSuccess;
        }

        case 7: { // ProgramPlane: input[0] = handle -> bind GEM buffer to Pipe A plane 1
            //    -> output[0] = 1 if PLANE_SURFLIVE latched ggttOffset, else 0
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            MyIntelGEMBuffer *buf = (MyIntelGEMBuffer *)arguments->scalarInput[0];

            // Security: pointer-compare against per-client registry — never deref a
            // userspace-supplied pointer until it matches an entry we handed out.
            bool verified = false;
            for (uint32_t i = 0; i < fBufferCount; i++) {
                if (fBuffers[i] == buf) { verified = true; break; }
            }
            if (!verified) return kIOReturnBadArgument;
            if (buf->magic != GEM_BUFFER_MAGIC) return kIOReturnBadArgument;   /* defense-in-depth */

            arguments->scalarOutput[0] = fProvider->programPlane(buf) ? 1 : 0;
            return kIOReturnSuccess;
        }

        case 8: { // ReadAperture: input[0] = GGTT byte offset -> 32-bit value via CURRENT PTEs
            //    Read-only. MUST NOT call ggttInvalidate() here — changing HW state would
            //    invalidate the pre/post diagnostic comparison.
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            arguments->scalarOutput[0] = fProvider->readAperture32(arguments->scalarInput[0]);
            return kIOReturnSuccess;
        }

        case 9: { // ReadGSM: input[0] = PTE index -> 64-bit PTE from GSM (direct, no TLB path)
            if (arguments->scalarInputCount < 1 || arguments->scalarOutputCount < 1) return kIOReturnBadArgument;

            uint64_t idx = arguments->scalarInput[0];
            if (idx >= fProvider->fGttTotal) return kIOReturnBadArgument;
            volatile uint64_t *gsm64 = (volatile uint64_t *)fProvider->fGsm;
            __asm__ volatile("mfence" ::: "memory");
            arguments->scalarOutput[0] = gsm64[idx];
            return kIOReturnSuccess;
        }

        case 10: { // IOAccelSharedSetupDirtyRing — 24B struct [qword0,qword1,qword2], qword0+4 = capacity
            // lldb proof: MTLIOAccelDevice::lazyInitialize dereferences qword0 at +4 to get ring count.
            // Must return user-valid VA, not kernel VA.
            if (!arguments->structureOutput || arguments->structureOutputSize < 24) return kIOReturnBadArgument;
            // Lazily allocate 1 page for dirty-ring if not already
            if (!fDirtyRingMD) {
                fDirtyRingMD = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut | kIOMemoryBufferPageable, 4096, PAGE_SIZE);
                if (!fDirtyRingMD) return kIOReturnNoMemory;
                IOReturn prep = fDirtyRingMD->prepare();
                if (prep != kIOReturnSuccess) { fDirtyRingMD->release(); fDirtyRingMD = NULL; return prep; }
                // Create mapping in client task — this gives userVA
                fDirtyRingMap = fDirtyRingMD->createMappingInTask(fClientTask, 0, kIOMapAnywhere);
                if (!fDirtyRingMap) { fDirtyRingMD->complete(); fDirtyRingMD->release(); fDirtyRingMD = NULL; return kIOReturnNoMemory; }
                fDirtyRingUserVA = fDirtyRingMap->getVirtualAddress();
                IOMemoryMap *kernMap = fDirtyRingMD->map(kIOMapInhibitCache);
                if (kernMap) {
                    void *kernVA = (void *)kernMap->getVirtualAddress();
                    bzero(kernVA, 4096);
                    *(uint32_t*)((uint8_t*)kernVA + 4) = 64;
                    IOLog("MyIntelGPU::[dirtyRing] allocated userVA=0x%llx cap=%u (kernVA=%p)\n", fDirtyRingUserVA, *(uint32_t*)((uint8_t*)kernVA+4), kernVA);
                    kernMap->release();
                } else {
                    IOLog("MyIntelGPU::[dirtyRing] ERROR kernMap null, userVA=0x%llx\n", fDirtyRingUserVA);
                }
            }
            uint64_t* out = (uint64_t*)arguments->structureOutput;
            out[0] = fDirtyRingUserVA;
            out[1] = 0;
            out[2] = 0;
            arguments->structureOutputSize = 24;
            IOLog("MyIntelGPU::[dirtyRing] selector10 return va=0x%llx 24B\n", fDirtyRingUserVA);
            return kIOReturnSuccess;
        }
        case 11: { // ggttInvalidate (moved from 10) : TLB flush
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            fProvider->ggttInvalidate();
            arguments->scalarOutput[0] = 1;
            return kIOReturnSuccess;
        }

        /* Phase B step-machine: incremental accelerator adoption */
        case 20: { // AccelStepAlloc: input[0]=attachMode(1=this,2=PCI)
            if (!fProvider) return kIOReturnNotReady;
            uint32_t mode = arguments->scalarInputCount >= 1 ? (uint32_t)arguments->scalarInput[0] : 1;
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            arguments->scalarOutput[0] = fProvider->accelStepAlloc(mode) ? 1 : 0;
            return kIOReturnSuccess;
        }
        case 21: { // AccelStepAttach: input[0]=parentSel(0=this,1=PCI,2=IOResources)
            if (!fProvider) return kIOReturnNotReady;
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            uint32_t psel = arguments->scalarInputCount >= 1 ? (uint32_t)arguments->scalarInput[0] : 0;
            arguments->scalarOutput[0] = fProvider->accelStepAttach(psel) ? 1 : 0;
            return kIOReturnSuccess;
        }
        case 22: { // AccelStepStart — DANGER: publishes nubs (freeze watch)
            if (!fProvider) return kIOReturnNotReady;
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            arguments->scalarOutput[0] = fProvider->accelStepStart() ? 1 : 0;
            return kIOReturnSuccess;
        }
        case 23: { // AccelPadProps — Door-A eGPU trick live injection
            if (!fProvider) return kIOReturnNotReady;
            fProvider->accelPadProps();
            return kIOReturnSuccess;
        }
        case 24: { // AccelBatchBlit — hypothesis #10 BATCH+BB_START probe
            if (!fProvider) return kIOReturnNotReady;
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            arguments->scalarOutput[0] = fProvider->accelBatchBlit() ? 1 : 0;
            return kIOReturnSuccess;
        }

        default:
            return kIOReturnBadArgument;
    }
}
