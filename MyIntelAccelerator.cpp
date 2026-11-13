/*
 * IOAccelerator binding implementation. See MyIntelAccelerator.hpp for scope.
 * Fail-safe by construction: any sub-step failing logs and continues —
 * accelerator creation can never take down MyIntelGPU::start().
 */

#include "MyIntelAccelerator.hpp"
#include "MyIntelGPU.hpp"

#include "MyIntelRing.hpp"
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <stdint.h>
#include <string.h>

#define AccelDebug(fmt, ...) \
    do { IOLog("MyIntelAccelerator:%d " fmt "\n", __LINE__, ##__VA_ARGS__); } while(0)



#define super IOAccelerator
OSDefineMetaClassAndStructors(MyIntelAccelerator, IOAccelerator)

#pragma mark - MyIntelAccelerator

MyIntelAccelerator *MyIntelAccelerator::withGPU(MyIntelGPU *gpu, IOOptionBits attachMode)
{
    if (!gpu) {
        return NULL;
    }
    MyIntelAccelerator *accel = new MyIntelAccelerator;
    if (!accel) {
        return NULL;
    }
    if (!accel->init(NULL)) {
        accel->release();
        return NULL;
    }
    accel->fGPU        = gpu;
    accel->fAttachMode = attachMode;
    accel->fAccelID    = 0;
    accel->fAccelIDValid = false;
    accel->fSurfaceLock = IOLockAlloc();
    if (!accel->fSurfaceLock) {
        accel->release();
        return NULL;
    }
    return accel;
}

bool MyIntelAccelerator::start(IOService *provider)
{
    if (!super::start(provider)) {
        AccelDebug("super::start failed");
        return false;
    }

    /* Publish diagnostic identity before registerService(). */
    if (!publishProperties()) {
        AccelDebug("publishProperties failed (continuing)");
    }

    /* Family-issued accelerator ID. Semantics are closed-source on
     * IOGraphicsFamily 597 — treat as opaque: log result, never fail
     * the service on error. */
    IOReturn ar = IOAccelerator::createAccelID(0, &fAccelID);
    if (ar == kIOReturnSuccess) {
        fAccelIDValid = true;
        AccelDebug("createAccelID OK -> id=%d", (int)fAccelID);
    } else {
        fAccelIDValid = false;
        AccelDebug("createAccelID FAILED 0x%X (continuing)", (unsigned int)ar);
    }

    registerService();
    AccelDebug("started (attachMode=%u accelID=%s%d)",
               (unsigned int)fAttachMode,
               fAccelIDValid ? "" : "invalid/", (int)fAccelID);
    return true;
}

void MyIntelAccelerator::stop(IOService *provider)
{
    surfaceReleaseAll();
    if (fAccelIDValid) {
        IOReturn ar = IOAccelerator::releaseAccelID(0, fAccelIDValid ? fAccelID : 0);
        AccelDebug("releaseAccelID id=%d -> 0x%X", (int)fAccelID, (unsigned int)ar);
        fAccelIDValid = false;
    }
    fGPU = NULL;
    super::stop(provider);
}

void MyIntelAccelerator::free()
{
    if (fSurfaceLock) {
        IOLockFree(fSurfaceLock);
        fSurfaceLock = NULL;
    }
    super::free();
}

IOReturn MyIntelAccelerator::newUserClient(task_t resettingTask, void *securityID,
                                           UInt32 type, OSDictionary *properties,
                                           IOUserClient **handler)
{
    if (!handler) {
        return kIOReturnBadArgument;
    }
    *handler = NULL;

    AccelDebug("newUserClient: type=0x%X from task=%p", (unsigned int)type, resettingTask);

    if (type == kMyIntelAccelInfoClientType ||
        type == kMyIntelAccelMetalClientType ||
        type == kMyIntelAccelSharedClientType) {
        MyIntelAccelClient *client = new MyIntelAccelClient;
        if (!client) {
            return kIOReturnNoMemory;
        }
        if (!client->initWithTask(resettingTask, securityID, type, properties)) {
            client->release();
            return kIOReturnError;
        }
        if (!client->attach(this)) {
            client->release();
            return kIOReturnError;
        }
        if (!client->start(this)) {
            client->detach(this);
            client->release();
            return kIOReturnError;
        }

        *handler = client;
        AccelDebug("newUserClient: client type=0x%X created successfully (task %p)",
                   (unsigned int)type, resettingTask);
        return kIOReturnSuccess;
    }

    AccelDebug("newUserClient: unsupported type 0x%X from task %p",
               (unsigned int)type, resettingTask);
    return kIOReturnUnsupported;
}

bool MyIntelAccelerator::publishProperties(void)
{
    bool ok = true;

    /* model — System Profiler GPU naming (real silicon: Raptor Lake-U 0xA7AC) */
    setProperty("model", "Intel Iris Xe");

    /* Metal plugin discovery: the loader reads these from the IOAccelerator
     * node, not from the IOPCIDevice personality. Name must match the bundle
     * shipped in Contents/PlugIns (MyIntelGPUMTLDriver.bundle). */
    setProperty("MetalPluginName", "MyIntelGPUMTLDriver");
    setProperty("MetalPluginClassName", "MyIntelGPUMTLDevice");
    setProperty("MetalStatisticsName", "Intel Iris Xe 80EU");

    return ok;
}

/* =========================================================================
 *  MyIntelAccelClient — diagnostic user client
 *  Selector 0 (kMyIntelAccelSelectorGetInfo): returns MyIntelAccelInfo.
 *  No pointer arguments from userspace — fixed-size output only.
 *======================================================================= */

OSDefineMetaClassAndStructors(MyIntelAccelClient, IOUserClient)

bool MyIntelAccelClient::initWithTask(task_t owningTask, void *securityToken,
                                      UInt32 type, OSDictionary *properties)
{
    if (!IOUserClient::initWithTask(owningTask, securityToken, type, properties)) {
        AccelDebug("AccelClient initWithTask: super failed");
        return false;
    }
    fAccel = NULL;
    fClientType = type;
    fClientTask = owningTask;
    fDirtyRingMD = NULL;
    fDirtyRingMap = NULL;
    fDirtyRingUserVA = 0;
    fSurfaceID = 0;
    fColorMode = 0;
    fShapeW = fShapeH = 0;
    return true;
}

bool MyIntelAccelClient::start(IOService *provider)
{
    fAccel = OSDynamicCast(MyIntelAccelerator, provider);
    if (!fAccel) {
        AccelDebug("AccelClient start: provider is not MyIntelAccelerator");
        return false;
    }
    return IOUserClient::start(provider);
}

IOReturn MyIntelAccelClient::clientClose(void)
{
    // [step 1: release SurfaceBacking]
    if (fAccel && fSurfaceID) {
        fAccel->surfaceReleaseByID(fSurfaceID);
    }

    // [step 2: clear the dirty Ring tracking]
    if (fDirtyRingMap) { fDirtyRingMap->release(); fDirtyRingMap = NULL; }
    if (fDirtyRingMD) { fDirtyRingMD->complete(); fDirtyRingMD->release(); fDirtyRingMD = NULL; }
    fDirtyRingUserVA = 0;

    AccelDebug("AccelClient clientClose — surface %u cleaned up", fSurfaceID);
    terminate();
    return kIOReturnSuccess; // confirm native success grant
}

/* ── M2C: Metal device-class client (type 5) + shared-client probe (type 6) ──
 * IOAccelDeviceCreateWithAPIProperty (private, IOAccelerator.framework):
 *   type-5 IOServiceOpen -> sel=9 (16B API name) -> sel=2 (600B caps struct) ->
 *   dlsym(RTLD_SELF=-3, name @ caps+0x18) — NULL => factory returns NULL (device nil).
 * The dlsym name MUST resolve inside IOAccelerator's dependency scope, so default
 * to a real exported IOAccelerator symbol; override per boot via `myaccelname=`.
 * Field layout (from lldb disasm of IOAccelDeviceCreateWithAPIProperty):
 *   +0x00 qword -> obj->0x20   +0x08 u32 count -> obj->0x30 (0 = skip ptr array)
 *   +0x38 qword -> obj->0x34   +0x40 u32 -> obj->0x3c   +0x18 C string -> dlsym
 * NOTE: live lldb regs show structOut=rbp-0x280 (&outSize var at rbp-0x2b0);
 * dlsym reads rbp-0x268 = structOut+0x18 (NOT +0x48 — off-by-0x30 bug, fixed).
 */

static char gMyAccelName[64] = "IOAccelSharedGetConnect";  // default export candidate
static bool gMyAccelNameInit = false;

static void myAccelNameInitOnce(void)
{
    if (gMyAccelNameInit) return;
    char b[64];
    bool has = PE_parse_boot_argn("myaccelname", b, sizeof(b) - 1);
    if (has && b[0] != '\0') {
        strlcpy(gMyAccelName, b, sizeof(gMyAccelName));
    }
    gMyAccelNameInit = true;
}

IOReturn MyIntelAccelClient::setupDirtyRing(IOExternalMethodArguments *arguments)
{
    if (!arguments || !arguments->structureOutput || arguments->structureOutputSize < 24)
        return kIOReturnBadArgument;

    if (!fDirtyRingMD) {
        fDirtyRingMD = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut | kIOMemoryBufferPageable, 4096, PAGE_SIZE);
        if (!fDirtyRingMD) return kIOReturnNoMemory;
        if (fDirtyRingMD->prepare() != kIOReturnSuccess) {
            fDirtyRingMD->release();
            fDirtyRingMD = NULL;
            return kIOReturnNoMemory;
        }
        fDirtyRingMap = fDirtyRingMD->createMappingInTask(fClientTask, 0, kIOMapAnywhere);
        if (!fDirtyRingMap) {
            fDirtyRingMD->complete();
            fDirtyRingMD->release();
            fDirtyRingMD = NULL;
            return kIOReturnNoMemory;
        }
        fDirtyRingUserVA = fDirtyRingMap->getVirtualAddress();
        IOMemoryMap *kernMap = fDirtyRingMD->map(kIOMapInhibitCache);
        if (kernMap) {
            void *kernVA = (void *)kernMap->getVirtualAddress();
            bzero(kernVA, 4096);
            *(uint32_t *)((uint8_t *)kernVA + 4) = 64;
            kernMap->release();
        } else {
            IOLog("MyIntelAccelerator::[dirtyRing] ERROR kernMap null, userVA=0x%llx type=%u\n",
                  fDirtyRingUserVA, (unsigned)fClientType);
        }
        IOLog("MyIntelAccelerator::[dirtyRing] allocated userVA=0x%llx cap=64 type=%u\n",
              fDirtyRingUserVA, (unsigned)fClientType);
    }

    uint64_t *out = (uint64_t *)arguments->structureOutput;
    out[0] = fDirtyRingUserVA;
    out[1] = 0;
    out[2] = 0;
    arguments->structureOutputSize = 24;
    IOLog("MyIntelAccelerator::[dirtyRing] sel10 return va=0x%llx 24B type=%u\n",
          fDirtyRingUserVA, (unsigned)fClientType);
    return kIOReturnSuccess;
}

IOReturn MyIntelAccelClient::externalMethod(uint32_t selector,
                                            IOExternalMethodArguments *arguments,
                                            IOExternalMethodDispatch *dispatch,
                                            OSObject *target,
                                             void *reference)
{
    if (!arguments) {
        return kIOReturnBadArgument;
    }

    if (fClientType == kMyIntelAccelInfoClientType) {
        if (selector != kMyIntelAccelSelectorGetInfo) {
            return kIOReturnUnsupported;
        }
        return sGetInfo(this, arguments->scalarInput, arguments->scalarInputCount,
                        arguments->scalarOutput, arguments->scalarOutputCount);
    }

    if (fClientType == kMyIntelAccelMetalClientType ||
        fClientType == kMyIntelAccelSharedClientType) {
        myAccelNameInitOnce();

        /* IOAccelSharedSetupDirtyRing uses selector 10 and a 24-byte output. */
        if (selector == 10 && arguments->structureOutput &&
            arguments->structureOutputSize >= 24) {
            return setupDirtyRing(arguments);
        }

        switch (selector) {
        case 9: { /* set API property name — 16B structIn e.g. "Metal\0..." */
            AccelDebug("DISC M2C type=%u sel=9 apiName stIn=%lu stOut=%lu",
                       (unsigned int)fClientType,
                       (unsigned long)arguments->structureInputSize,
                       (unsigned long)arguments->structureOutputSize);
            return kIOReturnSuccess;
        }
        case 2: { /* get device caps — 600B structOut (IOAccelDeviceCreateWithAPIProperty) */
            if (!arguments->structureOutput || arguments->structureOutputSize < 600)
                return kIOReturnBadArgument;
            uint8_t *caps = (uint8_t *)arguments->structureOutput;
            bzero(caps, arguments->structureOutputSize);
            strlcpy((char *)(caps + 0x18), gMyAccelName, 64);
            AccelDebug("DISC M2C type=%u sel=2 caps 600B name=%s",
                       (unsigned int)fClientType, gMyAccelName);
            return kIOReturnSuccess;
        }
        default:
            AccelDebug("DISC M2C type=%u sel=%u in=%u out=%u stIn=%lu stOut=%lu -> SUCCESS",
                       (unsigned int)fClientType,
                       (unsigned int)selector,
                       (unsigned int)arguments->scalarInputCount,
                       (unsigned int)arguments->scalarOutputCount,
                       (unsigned long)arguments->structureInputSize,
                       (unsigned long)arguments->structureOutputSize);
            return kIOReturnSuccess;
        }
    }

    if (fClientType != kMyIntelAccelInfoClientType) {
        return kIOReturnUnsupported;
    }

    /* Mission C Phase 1: log-only — responses below must stay unchanged (crash oracle) */
    AccelDebug("DISC sel=%u in=%u out=%u stIn=%lu stOut=%lu",
               (unsigned int)selector,
               (unsigned int)arguments->scalarInputCount,
               (unsigned int)arguments->scalarOutputCount,
               (unsigned long)arguments->structureInputSize,
               (unsigned long)arguments->structureOutputSize);
    for (uint32_t discI = 0; discI < arguments->scalarInputCount && discI < 8; discI++) {
        AccelDebug("DISC   in[%u]=0x%llX", (unsigned int)discI,
                   (unsigned long long)arguments->scalarInput[discI]);
    }
    if (arguments->structureInput != NULL && arguments->structureInputSize > 0) {
        const uint8_t *discP = (const uint8_t *)arguments->structureInput;
        uint32_t discN = (uint32_t)(arguments->structureInputSize < 16 ?
                                    arguments->structureInputSize : 16);
        char discHex[3 * 16];
        uint32_t discPos = 0;
        for (uint32_t discI = 0; discI < discN; discI++) {
            discHex[discPos++] = "0123456789ABCDEF"[discP[discI] >> 4];
            discHex[discPos++] = "0123456789ABCDEF"[discP[discI] & 0xF];
            discHex[discPos++] = ' ';
        }
        discHex[discPos] = '\0';
        AccelDebug("DISC   st[%lu]=%s",
                   (unsigned long)arguments->structureInputSize, discHex);
    }

    switch (selector) {
    /* ── Surface methods ── */
    case 0: { // ReadLockOptions
        if (arguments->scalarOutput && arguments->scalarOutputCount > 0)
            arguments->scalarOutput[0] = kIOAccelSurfaceLockInDontCare;
        return kIOReturnSuccess;
    }
    case 1:
        return kIOReturnSuccess;
    case 2: { // GetState — idle
        if (arguments->scalarOutput && arguments->scalarOutputCount > 0)
            arguments->scalarOutput[0] = kIOAccelSurfaceStateIdleBit;
        return kIOReturnSuccess;
    }
    case 3: { // WriteLockOptions
        if (arguments->scalarOutput && arguments->scalarOutputCount > 0)
            arguments->scalarOutput[0] = kIOAccelSurfaceLockInDontCare;
        return kIOReturnSuccess;
    }
    case 4:
        return kIOReturnSuccess;
    case 5: { // Read — copy from backing
        if (!arguments->structureOutput || arguments->structureOutputSize == 0)
            return kIOReturnBadArgument;
        MyIntelAccelerator::SurfaceEntry *entry =
            fAccel ? fAccel->surfaceFindOrCreate(fSurfaceID) : NULL;
        if (!entry || !entry->backing || !entry->backing->cpuAddr)
            return kIOReturnNotReady;
        uint32_t copyN = arguments->structureOutputSize;
        if (copyN > entry->bytes) copyN = (uint32_t)entry->bytes;
        memcpy(arguments->structureOutput, entry->backing->cpuAddr, copyN);
        return kIOReturnSuccess;
    }
    case 6: { // SetShapeBacking — store region + alloc backing
        if (arguments->structureInput == NULL || arguments->structureInputSize < sizeof(IOAccelDeviceRegion))
            return kIOReturnBadArgument;
        IOAccelDeviceRegion region;
        memcpy(&region, arguments->structureInput, sizeof(region));
        fShapeW = (uint32_t)(region.bounds.w < 0 ? -(int32_t)region.bounds.w : (int32_t)region.bounds.w);
        fShapeH = (uint32_t)(region.bounds.h < 0 ? -(int32_t)region.bounds.h : (int32_t)region.bounds.h);
        AccelDebug("SURFACE SetShapeBacking W=%u H=%u sid=%u", fShapeW, fShapeH, fSurfaceID);
        if (fSurfaceID && fShapeW && fShapeH && fShapeW <= 8192 && fShapeH <= 4320 && fAccel && fAccel->getGPU()) {
            MyIntelGPU *gpu = fAccel->getGPU();
            auto *ent = fAccel->surfaceFindOrCreate(fSurfaceID);
            uint32_t need = fShapeW * fShapeH * 4;
            if (ent && (!ent->backing || ent->bytes != need)) {
                if (ent->backing) { gemBufferDestroy(ent->backing, gpu->getGsm(), &MyIntelGPU::ggttInvalidateTrampoline, gpu); ent->backing = NULL; ent->bytes = 0; }
                ent->backing = (MyIntelGEMBuffer*)gemBufferCreate(need, 0, gpu->getGsm(), gpu->getGttTotal(), (void*)gpu->getApertureVA(), gpu->getApertureSize(), &MyIntelGPU::ggttInvalidateTrampoline, gpu);
                if (ent->backing) { ent->w = fShapeW; ent->h = fShapeH; ent->bytes = need; IOLog("MyIntelGPU: SURFACE backing alloc sid=%u %ux%u @ggtt=0x%X\n", fSurfaceID, fShapeW, fShapeH, ent->backing->ggttOffset); }
            }
        }
        return kIOReturnSuccess;
    }
    case 7: { // kIOAccelSurfaceSetIDMode
        if (arguments->scalarInputCount < 2) return kIOReturnBadArgument;
        fSurfaceID = (uint32_t)arguments->scalarInput[0];
        fColorMode = (uint32_t)arguments->scalarInput[1];
        AccelDebug("SURFACE SetIDMode surface=%u mode=0x%X", fSurfaceID, fColorMode);
        return kIOReturnSuccess;
    }
    case 8: // SetScale
        return kIOReturnSuccess;
    case 9: { // SetShape — also alloc backing for large canvas (WS 8192x4320 path)
        // WS may call SetShape with uint32_t w,h in scalarInput; try to capture
        if (arguments->scalarInputCount >= 2) {
            uint32_t w = (uint32_t)arguments->scalarInput[0];
            uint32_t h = (uint32_t)arguments->scalarInput[1];
            if (w && h && w <= 8192 && h <= 4320) { fShapeW = w; fShapeH = h; }
        }
        if (fSurfaceID && fShapeW && fShapeH && fAccel && fAccel->getGPU()) {
            MyIntelGPU *gpu = fAccel->getGPU();
            auto *ent = fAccel->surfaceFindOrCreate(fSurfaceID);
            uint32_t need = fShapeW * fShapeH * 4;
            if (ent && (!ent->backing || ent->bytes != need)) {
                if (ent->backing) { gemBufferDestroy(ent->backing, gpu->getGsm(), &MyIntelGPU::ggttInvalidateTrampoline, gpu); ent->backing = NULL; ent->bytes = 0; }
                ent->backing = (MyIntelGEMBuffer*)gemBufferCreate(need, 0, gpu->getGsm(), gpu->getGttTotal(), (void*)gpu->getApertureVA(), gpu->getApertureSize(), &MyIntelGPU::ggttInvalidateTrampoline, gpu);
                if (ent->backing) { ent->w = fShapeW; ent->h = fShapeH; ent->bytes = need; IOLog("MyIntelGPU: SURFACE backing alloc sid=%u %ux%u @ggtt=0x%X\n", fSurfaceID, fShapeW, fShapeH, ent->backing->ggttOffset); }
            }
        }
        return kIOReturnSuccess;
    }
    case 10: { // Flush — dual RCS/BCS dispatch: RCS primary for 3D/GPGPU, BCS fallback for blit
        // IRON RULE: RING_CTL_SIZE_SHIFT must stay 11, RCS base 0x2000 not 0x22000
        MyIntelGPU *gpu = fAccel ? fAccel->getGPU() : NULL;
        MyIntelRing *rcs = gpu ? gpu->getRingRCS() : NULL;
        MyIntelRing *bcs = gpu ? gpu->getRingBCS() : NULL;
        MyIntelRingCallbacks *cb = gpu ? gpu->getRingCallbacks() : NULL;
        MyIntelGEMBuffer *bridge = gpu ? gpu->getBridgeBuf() : NULL;
        MyIntelAccelerator::SurfaceEntry *ent =
            fAccel ? fAccel->surfaceFindOrCreate(fSurfaceID) : NULL;
        // B bridge: WS surface -> scanout buffer (real compositing path)
        if (bridge && ent && ent->backing && ent->backing->cpuAddr &&
            cb && ent->backing->ggttOffset) {
            // Ensure bridge size covers surface; if not, fallback to dummy flush
            uint32_t w = ent->w ? ent->w : fShapeW;
            uint32_t h = ent->h ? ent->h : fShapeH;
            if (w && h && w <= 8192 && h <= 4320) {
                // RCS primary: 3D/GPGPU Flush via render engine
                if (rcs && ringIsInitialized(rcs)) {
                    if (ringEmitSurfacePresent(rcs, bridge->ggttOffset, 7680,
                                               ent->backing->ggttOffset, w * 4,
                                               (uint16_t)w, (uint16_t)h)) {
                        ringEmitFlushDW(rcs, true, false);
                        ringSubmit(rcs, cb);
                        IOLog("MyIntelGPU: RCS Flush w=%u h=%u ggtt=0x%X\n", w, h, ent->backing->ggttOffset);
                        return kIOReturnSuccess;
                    } else {
                        IOLog("MyIntelGPU: RCS emit FAILED w=%u h=%u -> fallback BCS\n", w, h);
                    }
                } else {
                    IOLog("MyIntelGPU: RCS not ready (rcs=%p init=%d) -> fallback BCS\n", rcs, rcs ? ringIsInitialized(rcs) : 0);
                }
                // BCS fallback: blit path for WindowServer compositing
                if (bcs && ringIsInitialized(bcs) &&
                    ringEmitSurfacePresent(bcs, bridge->ggttOffset, 7680,
                                           ent->backing->ggttOffset, w * 4,
                                           (uint16_t)w, (uint16_t)h)) {
                    ringEmitFlushDW(bcs, true, false);
                    ringSubmit(bcs, cb);
                    IOLog("MyIntelGPU: BCS Flush w=%u h=%u\n", w, h);
                    return kIOReturnSuccess;
                }
                // HW blit unavailable — CPU fallback
                if (ent->backing->cpuAddr && bridge->cpuAddr) {
                    uint32_t bytes = w * h * 4;
                    if (bytes > bridge->size) bytes = bridge->size;
                    if (bytes > ent->bytes) bytes = (uint32_t)ent->bytes;
                    /* Format probe: first 4 pixels of WS surface — reveals
                     * byte order (BGRA/RGBA), alpha presence, values. 1x/boot */
                    static bool probed = false;
                    if (!probed && ent->bytes >= 16) {
                        uint32_t *px = (uint32_t *)ent->backing->cpuAddr;
                        IOLog("MyIntelGPU: [SURF-FMT] px0=%08X px1=%08X px2=%08X px3=%08X\n",
                              px[0], px[1], px[2], px[3]);
                        probed = true;
                    }
                    memcpy(bridge->cpuAddr, ent->backing->cpuAddr, bytes);
                }
                return kIOReturnSuccess;
            }
        }
        // Dummy flush: RCS primary, BCS fallback
        if (rcs && ringIsInitialized(rcs)) {
            if (!ringEmitFlushDW(rcs, true, true)) return kIOReturnError;
            ringEmitNOOP(rcs);
            ringEmitUserInterrupt(rcs);
            ringSubmit(rcs, cb);
            return kIOReturnSuccess;
        }
        if (!bcs || !cb || !ringIsInitialized(bcs)) return kIOReturnNotReady;
        if (!ringEmitFlushDW(bcs, true, true)) return kIOReturnError;
        ringEmitNOOP(bcs);
        ringEmitUserInterrupt(bcs);
        ringSubmit(bcs, cb);
        return kIOReturnSuccess;
    }
    case 11: // QueryLock
        return kIOReturnSuccess;
    case 12: // ReadLock
        return kIOReturnSuccess;
    case 13: // ReadUnlock
        return kIOReturnSuccess;
    case 14: // WriteLock
        return kIOReturnSuccess;
    case 15: // WriteUnlock
        return kIOReturnSuccess;
    case 16: // Control
        return kIOReturnSuccess;
    default:
        AccelDebug("SURFACE unknown sel=%u", selector);
        return kIOReturnUnsupported;
    }
}

IOReturn MyIntelAccelClient::sGetInfo(MyIntelAccelClient *client,
                                      const uint64_t *input, uint32_t inputCount,
                                      uint64_t *output, uint32_t outputCount)
{
    if (!client || !client->fAccel) {
        return kIOReturnNotReady;
    }
    if (!output) {
        return kIOReturnBadArgument;
    }
    if (outputCount < sizeof(MyIntelAccelInfo) / sizeof(uint64_t)) {
        return kIOReturnOverrun;
    }

    MyIntelAccelInfo info = {};
    info.accelID       = (uint32_t)client->fAccel->getAccelID();

    MyIntelGPU *gpu = client->fAccel->getGPU();
    if (gpu) {
        info.vramPoolMB    = (uint32_t)(gpu->getVramPoolSize() / (1024ULL * 1024));
        info.gttTotalPages = gpu->getGttTotal();
    }

    memcpy(output, &info, sizeof(info));
    return kIOReturnSuccess;
}

/* ── Mission C Phase 3b: persistent surface table (provider-owned) ── */

MyIntelAccelerator::SurfaceEntry *
MyIntelAccelerator::surfaceFindOrCreate(uint32_t sid)
{
    if (fSurfaceLock)
        IOLockLock(fSurfaceLock);
    for (int i = 0; i < kMaxSurfaces; i++) {
        if (fSurfaces[i].sid == sid && fSurfaces[i].backing) {
            if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
            return &fSurfaces[i];
        }
    }
    for (int i = 0; i < kMaxSurfaces; i++) {
        if (fSurfaces[i].backing == NULL) {
            fSurfaces[i].sid   = sid;
            fSurfaces[i].colorMode = 0;
            fSurfaces[i].w = fSurfaces[i].h = 0;
            fSurfaces[i].bytes = 0;
            if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
            return &fSurfaces[i];
        }
    }
    /* full: round-robin evict oldest slot */
    SurfaceEntry *e = &fSurfaces[fSurfEvict % kMaxSurfaces];
    fSurfEvict++;
    if (e->backing && fGPU) {
        gemBufferDestroy(e->backing, (uint32_t *)fGPU->getGsm(), &MyIntelGPU::ggttInvalidateTrampoline, fGPU);
        e->backing = NULL;
    }
    e->sid = sid;
    e->colorMode = 0;
    e->w = e->h = 0;
    e->bytes = 0;
    if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
    return e;
}

void MyIntelAccelerator::surfaceReleaseByID(uint32_t sid)
{
    if (!fGPU || !sid) return;

    if (fSurfaceLock) IOLockLock(fSurfaceLock);
    uint32_t *gsm = (uint32_t *)fGPU->getGsm();

    for (int i = 0; i < kMaxSurfaces; i++) {
        if (fSurfaces[i].sid == sid && fSurfaces[i].backing) {
            IOLog("MyIntelAccelerator: Releasing sid=%u\n", sid);
            gemBufferDestroy(fSurfaces[i].backing, gsm, &MyIntelGPU::ggttInvalidateTrampoline, fGPU);
            fSurfaces[i].backing = NULL;
            fSurfaces[i].sid = 0;
            fSurfaces[i].bytes = 0;
            break; // found, removed — done
        }
    }

    if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
}

void MyIntelAccelerator::surfaceReleaseAll(void)
{
    if (!fGPU)
        return;
    if (fSurfaceLock)
        IOLockLock(fSurfaceLock);
    uint32_t *gsm = (uint32_t *)fGPU->getGsm();
    for (int i = 0; i < kMaxSurfaces; i++) {
        if (fSurfaces[i].backing) {
            gemBufferDestroy(fSurfaces[i].backing, gsm, &MyIntelGPU::ggttInvalidateTrampoline, fGPU);
            fSurfaces[i].backing = NULL;
            fSurfaces[i].sid = 0;
            fSurfaces[i].bytes = 0;
        }
    }
    if (fSurfaceLock)
        IOLockUnlock(fSurfaceLock);
}

/* Mission C — blit composited surface to display scanout via BCS ring */
void MyIntelAccelerator::blitSurfaceToFramebuffer(
    mach_vm_address_t src_user_addr, uint32_t width, uint32_t height, uint32_t stride)
{
    MyIntelRing *bcs = fGPU ? fGPU->getRingBCS() : NULL;
    MyIntelRingCallbacks *cb = fGPU ? fGPU->getRingCallbacks() : NULL;
    if (!bcs || !cb || !ringIsInitialized(bcs)) return;

    IOMemoryDescriptor *srcDesc = IOMemoryDescriptor::withAddressRange(
        src_user_addr, (IOByteCount)(height * stride),
        kIODirectionOut, current_task());
    if (!srcDesc) return;

    if (srcDesc->prepare() != kIOReturnSuccess) {
        srcDesc->release();
        return;
    }

    IOPhysicalAddress src_phys = srcDesc->getPhysicalSegment(0, NULL);
    if (!src_phys) {
        srcDesc->complete();
        srcDesc->release();
        return;
    }

    if (fSurfaceLock) IOLockLock(fSurfaceLock);

    uint32_t *cs = ringBegin(bcs, 12);
    if (cs) {
        cs[0] = 0x50430006;
        cs[1] = 0xCC000000 | stride;
        cs[2] = 0;
        cs[3] = (height << 16) | width;
        cs[4] = 0x00000000;
        cs[5] = stride;
        cs[6] = 0;
        cs[7] = (uint32_t)src_phys;
        cs[8] = 0x26001001;
        cs[9] = 0;
        cs[10] = 0;
        cs[11] = 0;
        ringAdvance(bcs, cs);

        ringSubmit(bcs, cb);
    }

    srcDesc->complete();
    srcDesc->release();

    if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
}
