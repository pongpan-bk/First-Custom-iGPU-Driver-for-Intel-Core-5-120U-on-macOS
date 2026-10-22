#include "MyIntelGPUClient.hpp"
#include "MyIntelGEMBuffer.hpp"
#include "MyIntelRing.hpp"

OSDefineMetaClassAndStructors(MyIntelGPUClient, IOUserClient)

bool MyIntelGPUClient::initWithTask(task_t owningTask, void * securityToken, UInt32 type) {
    if (!IOUserClient::initWithTask(owningTask, securityToken, type)) return false;
    fClientTask = owningTask;
    fProvider   = NULL;
    fBufferCount = 0;
    for (uint32_t i = 0; i < kMaxClientBuffers; i++) fBuffers[i] = NULL;
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
        gemBufferDestroy(fBuffers[i], fProvider ? (uint32_t*)fProvider->fGsm : NULL);
        fBuffers[i] = NULL;
    }
    fBufferCount = 0;
    /* PTEs for the destroyed buffers were cleared — flush the GGTT TLB so no
     * stale mapping survives (i915 flushes after every PTE write). */
    if (fProvider) fProvider->ggttInvalidate();
    IOUserClient::stop(provider);
}

void MyIntelGPUClient::free() {
    /* free() is always called when the client object is destroyed — unlike
     * stop() which may be skipped by IOKit depending on retain count.
     * Restore any plane-bound buffer here as the definitive cleanup path. */
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
                                                  fProvider->fApertureSize);

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

            // gemBufferCreate() already bound this buffer to GGTT — just report the offset (idempotent).
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

        /* selector 3 (SubmitRing) removed — batch-staging path cut with the
           media/VDBOX subsystem; GEM readback selectors below stay for display debug */
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

        case 10: { // ggttInvalidate: TLB flush (GFX_FLSH_CNTL_GEN6 bit0=1) — if display recovers, cause was stale TLB
            if (arguments->scalarOutputCount < 1) return kIOReturnBadArgument;
            fProvider->ggttInvalidate();
            arguments->scalarOutput[0] = 1;
            return kIOReturnSuccess;
        }

        default:
            return kIOReturnBadArgument;
    }
}
