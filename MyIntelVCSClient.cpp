/*
 * MyIntelVCSClient — IOUserClient for VCS command submission
 * Author: pongpan-bk | Tooling: OpenCode (OhMyOpenCode)
 * Phase 8: User-space H.264/HEVC command submission via externalMethod()
 */

#include "MyIntelVCSClient.h"
#include "MyIntelGPU.hpp"
#include "MyIntelRing.hpp"
#include "MyIntelVCSCommand.h"
#include "MyIntelVCS.h"
#include "MyIntelObfuscate.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <string.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(MyIntelVCSClient, IOUserClient)

#define IODebug(fmt, ...) \
    IOLog("%s: [%s:%d] " fmt "\n", DEC_BUFFER_VCSClient, __FUNCTION__, __LINE__, ##__VA_ARGS__)

enum {
    kSubmitCommandBuffer = 0,
    kWaitForCompletion  = 1,
    kGetVCSStatus       = 2,
    kMapOutputBuffer    = 3,
    kUnmapOutputBuffer  = 4,
    kGetContextInfo     = 5,
    kSubmitVCSWorkload  = 6,
    kGemCreate          = 7,
    kGemDestroy         = 8,
    kGemGetInfo         = 9,
    kEngineReset        = 10,   /* 2.0.251: VDBOX engine reset + diag */
};

/*
 * VCS output buffer mapping table
 * Maps GGTT offsets to user-space addresses for decoded frame access
 */
#define VCS_MAX_MAPPINGS 16

struct VCSOutputMapping {
    uint32_t ggttOffset;
    uint32_t size;
    void    *userAddr;
    bool     active;
};

static struct {
    VCSOutputMapping mappings[VCS_MAX_MAPPINGS];
    uint32_t         count;
} sOutputMappings;

bool MyIntelVCSClient::initWithTask(task_t owningTask, void *securityToken,
                                    UInt32 type, OSDictionary *properties)
{
    if (!super::initWithTask(owningTask, securityToken, type, properties)) {
        return false;
    }

    fProvider    = NULL;
    fTask        = owningTask;
    fOpenCount   = 0;

    memset(&sOutputMappings, 0, sizeof(sOutputMappings));
    memset(fGemSlots, 0, sizeof(fGemSlots));

    IODebug("initWithTask: OK");
    return true;
}

bool MyIntelVCSClient::start(IOService *provider)
{
    if (!super::start(provider)) {
        return false;
    }

    // provider = the MyIntelVCS IOService nub, not MyIntelGPU directly.
    // Walk up one level: nub -> parent (MyIntelGPU)
    IOService *parent = OSDynamicCast(IOService, provider->getProvider());
    fProvider = OSDynamicCast(MyIntelGPU, parent);
    if (!fProvider) {
        IODebug("ERROR: provider parent is not MyIntelGPU (provider=%p parent=%p)",
                provider, parent);
        return false;
    }

    /* v3.3.4: retain the provider — same discipline as MyIntelGPUClient::start()
     * so a kext unload / provider stop() cannot free MyIntelGPU (and with it
     * fEngineLock) while this client is still attached. Without this the
     * client's fProvider dangles and every fProvider->… deref (engineReset →
     * vcsFlushPendingQueue → IOLockLock(fEngineLock)) reads recycled memory
     * → GPF (two panics with fEngineLock=0xffff01aa…, a freed-zone echo). */
    fProvider->retain();

    fOpenCount++;
    IODebug("start: VCS client opened (count=%u)", fOpenCount);
    return true;
}

void MyIntelVCSClient::stop(IOService *provider)
{
    IODebug("stop: VCS client closing");

    if (fOpenCount > 0) {
        fOpenCount--;
    }

    super::stop(provider);
}

void MyIntelVCSClient::free()
{
    IODebug("free");
    freeAllGemBuffers();
    if (fProvider) {
        fProvider->release();   /* v3.3.4: match the retain() in start() */
        fProvider = NULL;
    }
    super::free();
}

IOReturn MyIntelVCSClient::clientClose(void)
{
    IODebug("clientClose");
    freeAllGemBuffers();
    terminate();
    return kIOReturnSuccess;
}

/*
 * externalMethod dispatch table
 */
static const IOExternalMethodDispatch sVCSMethodTable[] = {
    /* kSubmitCommandBuffer */
    { MyIntelVCSClient::submitCommandBuffer, 1, 0, 0, 0 },
    /* kWaitForCompletion */
    { MyIntelVCSClient::waitForCompletion, 1, 0, 0, 0 },
    /* kGetVCSStatus */
    { MyIntelVCSClient::getVCSStatus, 0, 0, 1, 0 },
    /* kMapOutputBuffer — in: ggtt, size | out: mapping idx */
    { MyIntelVCSClient::mapOutputBuffer, 2, 0, 1, 0 },
    /* kUnmapOutputBuffer */
    { MyIntelVCSClient::unmapOutputBuffer, 1, 0, 0, 0 },
    /* kGetContextInfo */
    { MyIntelVCSClient::getContextInfo, 0, 0, 3, 0 },
    /* kSubmitVCSWorkload — variable-size structureInput = raw VCS command dwords */
    { MyIntelVCSClient::submitVCSWorkload, 0, kIOUCVariableStructureSize, 0, 0 },
    /* kGemCreate — in: size, flags | out: handle, ggttOffset, realSize */
    { MyIntelVCSClient::gemCreate, 2, 0, 3, 0 },
    /* kGemDestroy — in: handle */
    { MyIntelVCSClient::gemDestroy, 1, 0, 0, 0 },
    /* kGemGetInfo — in: handle | out: ggttOffset, size, flags */
    { MyIntelVCSClient::gemGetInfo, 1, 0, 3, 0 },
    /* kEngineReset — 2.0.251: out: esr, ipehr, head, tail, ctl, hwspScratch, lrcScratch, lrcHead */
    { MyIntelVCSClient::engineReset, 0, 0, 8, 0 },
    /* kEngineDiag — 2.0.252: read-only 12-scalar engine state dump (no reset) */
    { MyIntelVCSClient::engineDiag, 0, 0, 12, 0 },
};

IOReturn MyIntelVCSClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                          IOExternalMethodDispatch *dispatch,
                                          OSObject *target, void *reference)
{
    if (selector >= (sizeof(sVCSMethodTable) / sizeof(sVCSMethodTable[0]))) {
        IODebug("ERROR: invalid selector %u", selector);
        return kIOReturnBadArgument;
    }

    dispatch = (IOExternalMethodDispatch *)&sVCSMethodTable[selector];
    target = this;
    reference = NULL;

    return super::externalMethod(selector, args, dispatch, target, reference);
}

IOReturn MyIntelVCSClient::clientMemoryForType(uint32_t type, uint32_t *flags,
                                               IOMemoryDescriptor **memory)
{
    IODebug("clientMemoryForType: type=%u", type);

    /*
     * Phase 9 — GEM CPU mapping: type = MYVCS_GEM_MAP_BASE + handle.
     * Wraps the SAME physical pages the GGTT PTEs point at (zero-copy:
     * CPU writes bitstream here, VDBOX reads via GGTT; decoded pixels
     * written by VDBOX are read back by CPU). LLC-coherent on Gen12+.
     */
    if (type >= MYVCS_GEM_MAP_BASE) {
        uint32_t handle = type - MYVCS_GEM_MAP_BASE;
        if (handle >= VCS_GEM_MAX_HANDLES || !fGemSlots[handle].active ||
            !fGemSlots[handle].buf) {
            return kIOReturnBadArgument;
        }

        MyIntelGEMBuffer *buf = fGemSlots[handle].buf;

        IOMemoryDescriptor *desc = IOBufferMemoryDescriptor::withAddress(
            buf->cpuAddr,
            buf->size,
            kIODirectionInOut);

        if (!desc) {
            return kIOReturnNoMemory;
        }

        *memory = desc;
        *flags = kIOMapAnywhere;   /* read-write เป็น default ของ mapping */

        IODebug("clientMemoryForType: GEM handle=%u size=%u ggtt=0x%X",
                handle, buf->size, buf->ggttOffset);
        return kIOReturnSuccess;
    }

    if (type >= sOutputMappings.count || !sOutputMappings.mappings[type].active) {
        return kIOReturnBadArgument;
    }

    VCSOutputMapping *map = &sOutputMappings.mappings[type];

    IOBufferMemoryDescriptor *desc = IOBufferMemoryDescriptor::withBytes(
        (void *)((uintptr_t)fProvider->getApertureVA() + map->ggttOffset),
        map->size,
        kIODirectionOutIn,
        false);

    if (!desc) {
        return kIOReturnNoMemory;
    }

    *memory = desc;
    *flags = kIOMapReadOnly | kIOMapAnywhere;

    return kIOReturnSuccess;
}

/*
 * kSubmitCommandBuffer — submit VCS decode command stream to ring
 *
 * args[0] = command buffer GGTT offset (from user-space mapping)
 * args[1] = command size in dwords
 * args[2] = codec type (0x01=H.264, 0x02=HEVC)
 *
 * Returns: kIOReturnSuccess on submission, error on failure
 */
IOReturn MyIntelVCSClient::submitCommandBuffer(OSObject *target, void *reference,
                                               IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    uint32_t ggttOffset = (uint32_t)args->scalarInput[0];
    uint32_t cmdSize    = (uint32_t)args->scalarInput[1];
    uint32_t codec      = (uint32_t)args->scalarInput[2];

    IODebug("submitCommandBuffer: ggtt=0x%X size=%u codec=%u", ggttOffset, cmdSize, codec);

    MyIntelRing *ring = self->fProvider->getVCSRing();
    if (!ring || !ringIsInitialized(ring)) {
        IODebug("ERROR: VCS ring not initialized");
        return kIOReturnNotReady;
    }

    if (cmdSize == 0 || cmdSize > 256) {
        IODebug("ERROR: invalid command size %u", cmdSize);
        return kIOReturnBadArgument;
    }

    /*
     * Emit MI_BATCH_BUFFER_START to point to user command buffer,
     * then MI_BATCH_BUFFER_END as safety net.
     */
    uint32_t *cs = ringBegin(ring, 4);
    if (!cs) {
        IODebug("ERROR: ring full, cannot submit");
        return kIOReturnNoMemory;
    }

    /* MI_BATCH_BUFFER_START | GGTT bit */
    *cs++ = 0x18800000 | (1U << 6);
    *cs++ = ggttOffset;

    /* MI_NOOP padding */
    *cs++ = 0x00000000;
    *cs++ = 0x00000000;

    ringAdvance(ring, cs);
    ringSubmit(ring, self->fProvider->getRingCallbacks());

    return kIOReturnSuccess;
}

/*
 * kWaitForCompletion — poll VCS status register until done
 *
 * args[0] = timeout in milliseconds
 * Returns: scalarOutput[0] = VCS status register value
 */
IOReturn MyIntelVCSClient::waitForCompletion(OSObject *target, void *reference,
                                             IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    uint32_t timeoutMs = (args->scalarInputCount > 0) ? (uint32_t)args->scalarInput[0] : 5000;

    uint32_t status = 0;
    uint32_t elapsed = 0;

    while (elapsed < timeoutMs) {
        status = self->fProvider->readReg32(VCS0_BASE_REAL + 0x34);
        if (status == 0) {
            break;
        }
        IODelay(1000);
        elapsed++;
    }

    args->scalarOutput[0] = status;
    args->scalarOutputCount = 1;

    return (elapsed < timeoutMs) ? kIOReturnSuccess : kIOReturnTimeout;
}

/*
 * kGetVCSStatus — read VCS engine status register
 * Returns: scalarOutput[0] = status value
 */
IOReturn MyIntelVCSClient::getVCSStatus(OSObject *target, void *reference,
                                        IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    uint32_t status = self->fProvider->readReg32(VCS0_BASE_REAL + obf_getRingCtl());

    args->scalarOutput[0] = status;
    args->scalarOutputCount = 1;

    return kIOReturnSuccess;
}

/*
 * kMapOutputBuffer — register a decoded frame buffer for user-space access
 *
 * args[0] = GGTT offset of output buffer
 * args[1] = size in bytes
 * Returns: scalarOutput[0] = mapping index (for clientMemoryForType)
 */
IOReturn MyIntelVCSClient::mapOutputBuffer(OSObject *target, void *reference,
                                           IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self) {
        return kIOReturnNotReady;
    }

    uint32_t ggttOffset = (uint32_t)args->scalarInput[0];
    uint32_t size       = (uint32_t)args->scalarInput[1];

    uint32_t idx = sOutputMappings.count;
    if (idx >= VCS_MAX_MAPPINGS) {
        IODebug("ERROR: output mapping table full");
        return kIOReturnNoSpace;
    }

    sOutputMappings.mappings[idx].ggttOffset = ggttOffset;
    sOutputMappings.mappings[idx].size       = size;
    sOutputMappings.mappings[idx].userAddr   = NULL;
    sOutputMappings.mappings[idx].active     = true;
    sOutputMappings.count = idx + 1;

    IODebug("mapOutputBuffer: idx=%u ggtt=0x%X size=%u", idx, ggttOffset, size);

    args->scalarOutput[0] = idx;
    args->scalarOutputCount = 1;

    return kIOReturnSuccess;
}

/*
 * kUnmapOutputBuffer — release a decoded frame buffer mapping
 *
 * args[0] = mapping index
 */
IOReturn MyIntelVCSClient::unmapOutputBuffer(OSObject *target, void *reference,
                                             IOExternalMethodArguments *args)
{
    uint32_t idx = (uint32_t)args->scalarInput[0];
    if (idx >= sOutputMappings.count) {
        return kIOReturnBadArgument;
    }

    sOutputMappings.mappings[idx].active = false;

    IODebug("unmapOutputBuffer: idx=%u", idx);
    return kIOReturnSuccess;
}

/*
 * kGetContextInfo — return VCS engine info
 *
 * Returns: scalarOutput[0] = VCS0 MMIO base
 *          scalarOutput[1] = ring GGTT offset
 *          scalarOutput[2] = ring size
 */
IOReturn MyIntelVCSClient::getContextInfo(OSObject *target, void *reference,
                                          IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    MyIntelRing *ring = self->fProvider->getVCSRing();

    args->scalarOutput[0] = VCS0_BASE_REAL;
    args->scalarOutput[1] = ring ? ring->ggttOffset : 0;
    args->scalarOutput[2] = ring ? ring->size : 0;
    args->scalarOutputCount = 3;

    return kIOReturnSuccess;
}

/*
 * kSubmitVCSWorkload — submit raw VCS command dwords directly into VCS0 ring
 *
 * structureInput: raw MFX/HCP command dwords (must be 16-byte aligned)
 *
 * Flow: validate → forcewake VDBOX → copy to ring → advance tail → kick GPU
 */
IOReturn MyIntelVCSClient::submitVCSWorkload(OSObject *target, void *reference,
                                             IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    const uint32_t *userCmds = (const uint32_t *)args->structureInput;
    size_t cmdSizeByte = args->structureInputSize;

    if (!userCmds || cmdSizeByte == 0 || (cmdSizeByte % 16) != 0) {
        IODebug("submitVCSWorkload: REJECTED — cmds=%p size=%zu (must be >0, 16-byte aligned)",
                userCmds, cmdSizeByte);
        return kIOReturnBadArgument;
    }

    uint32_t dwordCount = (uint32_t)(cmdSizeByte / 4);

    MyIntelRing *ring = self->fProvider->getVCSRing();
    if (!ring || !ringIsInitialized(ring)) {
        IODebug("submitVCSWorkload: VCS ring not initialized");
        return kIOReturnNotReady;
    }

    if (dwordCount > ringSpace(ring) / 4) {
        IODebug("submitVCSWorkload: REJECTED — %u dwords exceeds ring space %u bytes",
                dwordCount, ringSpace(ring));
        return kIOReturnNoMemory;
    }

    /* Forcewake: wake GT + RENDER domains so VCS ring regs are not gated */
    if (!self->fProvider->forceWakeGet()) {
        IODebug("submitVCSWorkload: forcewake FAILED — engine asleep, aborting");
        return kIOReturnTimeout;
    }

    /* ── PPGTT identity map: map EVERY client GEM buffer into the VCS
     * ring's 4-level PPGTT before kicking.
     *
     * Root cause of Phase-10 READBACK=0 (proved with MI_STORE_DATA_IMM
     * probe): the execlists LRC descriptor uses DESC_LEGACY_64B, so all
     * command data addresses resolve through the context's PPGTT — NOT
     * the GGTT. lrcMapBatchPages() had only ever been called for RCS/BCS
     * batches, so selector-6 workloads ran with every surface/IndObj
     * address unmapped → all VDBOX DMA silently faulted and wrote
     * nothing. Identity map = VA == GGTT offset, so the user-space
     * command stream needs no changes. */
    {
        MyIntelRing *vcsRing = self->fProvider->getVCSRing();
        if (vcsRing && vcsRing->ppgttPTs) {
            uint32_t mapped = 0;
            for (int i = 0; i < VCS_GEM_MAX_HANDLES; i++) {
                if (!self->fGemSlots[i].active || !self->fGemSlots[i].buf) continue;
                MyIntelGEMBuffer *b = self->fGemSlots[i].buf;
                if (!b->pagesPhys || b->pages == 0) continue;
                if (lrcMapBatchPages(vcsRing, b->ggttOffset,
                                     b->pagesPhys, b->pages)) {
                    mapped++;
                } else {
                    IODebug("submitVCSWorkload: PPGTT map FAILED — handle=%d ggtt=0x%X pages=%u",
                            i, b->ggttOffset, b->pages);
                }
            }
            IODebug("submitVCSWorkload: PPGTT identity map — %u/%d GEM buffers",
                    mapped, VCS_GEM_MAX_HANDLES);
        }
    }

    /* Copy user command dwords into VCS ring buffer */
    uint32_t *cs = ringBegin(ring, dwordCount);
    if (!cs) {
        IODebug("submitVCSWorkload: ringBegin returned NULL (ring full?)");
        self->fProvider->forceWakePut();
        return kIOReturnNoMemory;
    }

    memcpy(cs, userCmds, cmdSizeByte);
    ringAdvance(ring, cs + dwordCount);
    ringSubmit(ring, self->fProvider->getRingCallbacks());

    self->fProvider->forceWakePut();

    IODebug("submitVCSWorkload: OK — %u dwords (%zu bytes) pushed to VCS ring",
            dwordCount, cmdSizeByte);
    return kIOReturnSuccess;
}

/*
 * ─────────────────────────────────────────────
 *  2.0.251 — engineReset: VDBOX engine-level reset
 * ─────────────────────────────────────────────
 *  An illegal instruction on VDBOX raises ESR bit0 and PERMANENTLY halts
 *  the engine (HEAD frozen, nothing executes until reset). Before 2.0.251
 *  the only recovery was a full reboot. This mirrors i915
 *  gen6_hw_domain_reset() (intel_reset.c): write the engine's GRDOM bit
 *  into GEN6_GDRST (0x941C) and wait for self-clear. VCS0 domain =
 *  GEN6_GRDOM_MEDIA = bit2 (intel_engine_cs.c [VCS0]=GEN11_GRDOM_MEDIA).
 *
 *  Also rewinds the ring bookkeeping (head=tail) so the next execlist
 *  restore skips over the stale pre-reset stream, and returns the PPHWSP
 *  scratch dword (LRC+0x800) for MI_FLUSH_DW STORE_INDEX probes.
 */
IOReturn MyIntelVCSClient::engineReset(OSObject *target, void *reference,
                                       IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider || !args) {
        return kIOReturnBadArgument;
    }
    if (self->fProvider->isStopping()) {
        return kIOReturnNotReady;
    }

    MyIntelRing *ring = self->fProvider->getVCSRing();
    if (!ring) {
        return kIOReturnNotReady;
    }

    if (!self->fProvider->forceWakeGet()) {
        IODebug("engineReset: forcewake FAILED");
        return kIOReturnTimeout;
    }

    uint32_t base  = ring->mmioBase;
    uint32_t esr   = self->fProvider->readReg32(base + RING_ESR_OFFSET);
    uint32_t ipehr = self->fProvider->readReg32(base + RING_IPEHR_OFFSET);
    uint32_t head  = self->fProvider->readReg32(base + RING_HEAD_REG_OFFSET);
    uint32_t tail  = self->fProvider->readReg32(base + RING_TAIL_REG_OFFSET);
    uint32_t ctl   = self->fProvider->readReg32(base + RING_CTL_REG_OFFSET);

    IODebug("engineReset: pre-reset ESR=0x%08X IPEHR=0x%08X HEAD=0x%X TAIL=0x%X CTL=0x%X",
            esr, ipehr, head, tail, ctl);

    /* GEN6_GDRST with Gen11+ VCS0 domain (MEDIA bit5 | SFC0 bit17) */
    bool ok = false;
    self->fProvider->writeReg32(GEN6_GDRST_REG_OFFSET, VCS0_GDRST_DOMAIN);
    for (int i = 0; i < 1000; i++) {
        if ((self->fProvider->readReg32(GEN6_GDRST_REG_OFFSET) & VCS0_GDRST_DOMAIN) == 0) {
            ok = true;
            break;
        }
        IODelay(10);
    }

    /* Rewind ring: next lrcUpdateRingRegs() restores HEAD=TAIL (idle),
     * so the stale pre-reset command stream is never re-executed. */
    ring->head = ring->tail;

    /* 2.0.255: batches queued while wedged can never complete — without
     * this flush the pending queue stays FULL and every submit returns
     * kIOReturnBusy forever after the reset. */
    self->fProvider->vcsFlushPendingQueue();

    /* 2.0.252: GDRST returns the engine to power-on defaults — execlists
     * mode is OFF again and subsequent EL_CTRL_LOAD kicks are silently
     * ignored (i915 re-runs enable_execlists() after every engine reset).
     * Reprogram: DISABLE_LEGACY_MODE, clear STOP_RING, floor CSB ptr. */
    if (ok) {
        /* Reset ring HEAD/TAIL to 0 — clear stale ring position from
         * previous (possibly faulted) execution. Without this the
         * hardware HEAD remains at the fault address and the next
         * execlist submission re-fetches stale commands. */
        self->fProvider->writeReg32(base + RING_HEAD_REG_OFFSET, 0);
        self->fProvider->readReg32(base + RING_HEAD_REG_OFFSET);   /* posting */
        self->fProvider->writeReg32(base + RING_TAIL_REG_OFFSET, 0);
        self->fProvider->readReg32(base + RING_TAIL_REG_OFFSET);   /* posting */

        self->fProvider->writeReg32(base + RING_MODE_GEN7_OFFSET, 0x00080008u);
        self->fProvider->readReg32(base + RING_MODE_GEN7_OFFSET);
        self->fProvider->writeReg32(base + RING_MI_MODE_REG_OFFSET, 0x00010000u);
        self->fProvider->readReg32(base + RING_MI_MODE_REG_OFFSET);
        self->fProvider->writeReg32(base + RING_CONTEXT_STATUS_PTR_OFFSET,
                                    CONTEXT_STATUS_PTR_RESET);
        self->fProvider->readReg32(base + RING_CONTEXT_STATUS_PTR_OFFSET);
    }

    /* Per-engine HWS state is lost in the domain reset — reprogram it
     * exactly like ringCreate does (HWS PGA ptr + CSB write ptr + STAM). */
    if (ok && ring->hwspGgtt) {
        self->fProvider->writeReg32(base + RING_HWS_PGA_OFFSET, ring->hwspGgtt);
        self->fProvider->readReg32(base + RING_HWS_PGA_OFFSET);   /* posting */
        self->fProvider->writeReg32(base + RING_HWS_STAM_OFFSET, ~0U);
        self->fProvider->readReg32(base + RING_HWS_STAM_OFFSET);  /* posting */
        if (ring->hwspVaddr) {
            ring->hwspVaddr[HWS_CSB_WRITE_DWORD] = HWS_CSB_ENTRIES_GEN11 - 1;
            memset((uint8_t *)ring->hwspVaddr + HWS_CSB_BUF0_DWORD * 4, 0xFF,
                   HWS_CSB_ENTRIES_GEN11 * sizeof(uint64_t));
        }
    }

    uint32_t esr2   = self->fProvider->readReg32(base + RING_ESR_OFFSET);
    uint32_t head2  = self->fProvider->readReg32(base + RING_HEAD_REG_OFFSET);
    /* 2.0.252: MI_FLUSH_DW|STORE_INDEX writes into the RING_HWS_PGA page
     * (i915 status page), NOT the LRC PPHWSP — read the HWSP scratch. */
    uint32_t scratch = ring->hwspVaddr ? ring->hwspVaddr[0x800 / 4] : 0;
    uint32_t lrcScratch = ring->lrcVaddr
        ? ((uint32_t *)ring->lrcVaddr)[0x800 / 4]
        : 0;
    uint32_t lrcHead = (ring->lrcInited && ring->lrcVaddr)
        ? ((uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET))[CTX_RING_HEAD]
        : 0;

    IODebug("engineReset: %s — post-reset ESR=0x%08X HEAD=0x%X hwspScratch[0x800]=0x%08X",
            ok ? "OK" : "TIMEOUT", esr2, head2, scratch);

    self->fProvider->forceWakePut();

    if (args->scalarOutputCount >= 8) {
        args->scalarOutput[0] = esr;
        args->scalarOutput[1] = ipehr;
        args->scalarOutput[2] = head;
        args->scalarOutput[3] = tail;
        args->scalarOutput[4] = ctl;
        args->scalarOutput[5] = scratch;      /* HWSP page +0x800 (STORE_INDEX target) */
        args->scalarOutput[6] = lrcScratch;   /* LRC PPHWSP +0x800 (alt. interpretation) */
        args->scalarOutput[7] = lrcHead;      /* LRC image CTX_RING_HEAD (HW write-back) */
    }
    return ok ? kIOReturnSuccess : kIOReturnTimeout;
}

/*
 * ─────────────────────────────────────────────
 *  engineDiag — 2.0.252 read-only engine state dump (selector 11)
 * ─────────────────────────────────────────────
 * MMIO RING_HEAD/TAIL read 0 in execlists mode — the ground truth for
 * "did the engine consume commands" is the LRC image (HW writes
 * CTX_RING_HEAD back into it live) + the HWSP CSB entries. This selector
 * disturbs nothing, so it can run between probe stages without resetting.
 *
 * scalarOutput:
 *   [0] esr          [1] ipehr        [2] lrcHead (image)  [3] lrcTail (image)
 *   [4] csbWritePtr  [5] csb0 lo      [6] hwspScratch+0x800
 *   [7] execStatusLo [8] nopid(0x94)  [9] mmioHead [10] mmioTail [11] ctl
 */
IOReturn MyIntelVCSClient::engineDiag(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }
    MyIntelRing *ring = self->fProvider->getVCSRing();
    if (!ring) {
        return kIOReturnNotReady;
    }
    if (!self->fProvider->forceWakeGet()) {
        return kIOReturnTimeout;
    }

    uint32_t base = ring->mmioBase;
    uint32_t out[12];
    out[0]  = self->fProvider->readReg32(base + RING_ESR_OFFSET);
    out[1]  = self->fProvider->readReg32(base + RING_IPEHR_OFFSET);
    uint32_t *state = (ring->lrcInited && ring->lrcVaddr)
        ? (uint32_t *)((uint8_t *)ring->lrcVaddr + LRC_STATE_OFFSET) : NULL;
    out[2]  = state ? state[CTX_RING_HEAD] : 0;
    out[3]  = state ? state[CTX_RING_TAIL] : 0;
    out[4]  = ring->hwspVaddr ? ring->hwspVaddr[HWS_CSB_WRITE_DWORD] : 0;
    out[5]  = ring->hwspVaddr ? ring->hwspVaddr[HWS_CSB_BUF0_DWORD] : 0;
    out[6]  = ring->hwspVaddr ? ring->hwspVaddr[0x800 / 4] : 0;
    out[7]  = self->fProvider->readReg32(base + RING_EXECLIST_STATUS_LO_OFFSET);
    out[8]  = self->fProvider->readReg32(base + 0x94);   /* RING_NOPID — LRI test target */
    out[9]  = self->fProvider->readReg32(base + RING_HEAD_REG_OFFSET);
    out[10] = self->fProvider->readReg32(base + RING_TAIL_REG_OFFSET);
    out[11] = self->fProvider->readReg32(base + RING_CTL_REG_OFFSET);

    self->fProvider->forceWakePut();

    uint32_t n = args->scalarOutputCount < 12 ? args->scalarOutputCount : 12;
    for (uint32_t i = 0; i < n; i++) {
        args->scalarOutput[i] = out[i];
    }
    return kIOReturnSuccess;
}

/*
 * ─────────────────────────────────────────────
 *  Phase 9 — GEM Buffer Allocation (user-space ABI)
 * ─────────────────────────────────────────────
 *
 *  จองบัฟเฟอร์ 2 ชนิดตาม roadmap:
 *    - Source Buffer      : H.264 bitstream (CPU write → GPU read)
 *    - Destination Surface: decoded NV12    (GPU write → CPU read)
 *  ทั้งคู่ bind เข้า GGTT จริง → VDBOX DMA ถึงโดยตรง
 */

/*
 * kGemCreate — allocate a GGTT-bound GEM buffer for this client
 *
 * scalarInput[0]  = requested size in bytes (page-aligned up)
 * scalarInput[1]  = flags (MYVCS_GEM_SOURCE / MYVCS_GEM_SURFACE)
 * scalarOutput[0] = handle (index into per-client slot table)
 * scalarOutput[1] = GGTT offset (for MFX command dwords)
 * scalarOutput[2] = actual (aligned) size
 */
IOReturn MyIntelVCSClient::gemCreate(OSObject *target, void *reference,
                                     IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self || !self->fProvider) {
        return kIOReturnNotReady;
    }

    uint32_t size  = (uint32_t)args->scalarInput[0];
    uint32_t flags = (uint32_t)args->scalarInput[1];

    if (size == 0 || size > GEM_MAX_BUFFER_SIZE) {
        IODebug("gemCreate: REJECTED — invalid size %u", size);
        return kIOReturnBadArgument;
    }

    int slot = -1;
    for (int i = 0; i < VCS_GEM_MAX_HANDLES; i++) {
        if (!self->fGemSlots[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        IODebug("gemCreate: REJECTED — handle table full");
        return kIOReturnNoSpace;
    }

    /* CPU+GPU both directions always on: source=CPU write/GPU read,
     * surface=GPU write/CPU read — LLC-coherent, no flush needed */
    flags |= GEM_FLAG_CPU_READ | GEM_FLAG_CPU_WRITE |
             GEM_FLAG_GPU_READ | GEM_FLAG_GPU_WRITE;

    MyIntelGEMBuffer *buf = gemBufferCreate(
        size,
        flags,
        self->fProvider->getGsm(),
        self->fProvider->getGttTotal(),
        (void *)self->fProvider->getApertureVA(),
        self->fProvider->getApertureSize(),
        &MyIntelGPU::ggttInvalidateTrampoline, self->fProvider);

    if (!buf) {
        IODebug("gemCreate: gemBufferCreate FAILED (size=%u)", size);
        return kIOReturnNoMemory;
    }

    /* PTEs just written — invalidate GGTT TLB so VDBOX sees them */
    self->fProvider->ggttInvalidate();

    self->fGemSlots[slot].buf    = buf;
    self->fGemSlots[slot].active = true;

    args->scalarOutput[0] = (uint64_t)slot;
    args->scalarOutput[1] = buf->ggttOffset;
    args->scalarOutput[2] = buf->size;
    args->scalarOutputCount = 3;

    IODebug("gemCreate: OK — handle=%d ggtt=0x%X size=%u flags=0x%X",
            slot, buf->ggttOffset, buf->size, flags);
    return kIOReturnSuccess;
}

/*
 * kGemDestroy — free a GEM buffer owned by this client
 *
 * scalarInput[0] = handle
 */
IOReturn MyIntelVCSClient::gemDestroy(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self) {
        return kIOReturnNotReady;
    }

    uint32_t handle = (uint32_t)args->scalarInput[0];
    if (handle >= VCS_GEM_MAX_HANDLES || !self->fGemSlots[handle].active) {
        return kIOReturnBadArgument;
    }

    MyIntelGEMBuffer *buf = self->fGemSlots[handle].buf;
    self->fGemSlots[handle].active = false;
    self->fGemSlots[handle].buf    = NULL;

    if (buf && self->fProvider) {
        gemBufferDestroy(buf, self->fProvider->getGsm(),
                         &MyIntelGPU::ggttInvalidateTrampoline, self->fProvider);
        self->fProvider->ggttInvalidate();
    }

    IODebug("gemDestroy: handle=%u freed", handle);
    return kIOReturnSuccess;
}

/*
 * kGemGetInfo — query a GEM buffer (GGTT offset for command dwords)
 *
 * scalarInput[0]  = handle
 * scalarOutput[0] = GGTT offset
 * scalarOutput[1] = size
 * scalarOutput[2] = flags
 */
IOReturn MyIntelVCSClient::gemGetInfo(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args)
{
    MyIntelVCSClient *self = OSDynamicCast(MyIntelVCSClient, target);
    if (!self) {
        return kIOReturnNotReady;
    }

    uint32_t handle = (uint32_t)args->scalarInput[0];
    if (handle >= VCS_GEM_MAX_HANDLES || !self->fGemSlots[handle].active ||
        !self->fGemSlots[handle].buf) {
        return kIOReturnBadArgument;
    }

    MyIntelGEMBuffer *buf = self->fGemSlots[handle].buf;

    args->scalarOutput[0] = buf->ggttOffset;
    args->scalarOutput[1] = buf->size;
    args->scalarOutput[2] = buf->flags;
    args->scalarOutputCount = 3;

    return kIOReturnSuccess;
}

/*
 * freeAllGemBuffers — release every slot still active (clientClose/free)
 *
 * กัน client ปิดตัวทิ้ง buffer ค้างใน GGTT (PTE ค้าง = aperture รู่ว)
 */
void MyIntelVCSClient::freeAllGemBuffers(void)
{
    bool any = false;

    for (int i = 0; i < VCS_GEM_MAX_HANDLES; i++) {
        if (fGemSlots[i].active && fGemSlots[i].buf) {
            if (fProvider) {
                gemBufferDestroy(fGemSlots[i].buf, fProvider->getGsm(),
                                 &MyIntelGPU::ggttInvalidateTrampoline, fProvider);
            }
            fGemSlots[i].buf    = NULL;
            fGemSlots[i].active = false;
            any = true;
        }
    }

    if (any && fProvider) {
        fProvider->ggttInvalidate();
        IODebug("freeAllGemBuffers: released all GEM buffers");
    }
}
