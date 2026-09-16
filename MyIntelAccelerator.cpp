/*===========================================================================
 *  MyIntelAccelerator.cpp
 *  MyIntelGPU.kext — IOAccelerator binding (Phase 6c-Debug)
 *
 * See MyIntelAccelerator.hpp for scope. Everything here is fail-safe:
 * any sub-step failing logs and continues — accelerator creation can
 * never take down MyIntelGPU::start().
 *=========================================================================*/

#include "MyIntelAccelerator.hpp"
#include "MyIntelGPU.hpp"
#include "MyIntelObfuscate.h"
#include "MyIntelRing.hpp"
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <stdint.h>
#include <string.h>

#define AccelDebug(fmt, ...) \
    do { IOLog("MyIntelAccelerator: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)

/* IOCFPlugInTypes UUID used by IOAccelerator2D.plugin consumers
 * (same as AppleIntelICLGraphics.kext personality). */
#define kMyIntelAccelCFPlugInUUID  "ACCF0000-0000-0000-0000-000a2789904e"
#define kMyIntelAccelCFPlugInName  "IOAccelerator2D.plugin"

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

    /* Properties BEFORE registerService() so consumers see a complete
     * node on first match. Failure is non-fatal. */
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

    /* 🚨 ล่าความจริงหน้างาน: พ่น Log ทุกครั้งที่มีคนมากดกริ่งเรียก 
     * จะได้เห็นเต็มสองตาว่าตอนกดย่อ/ขยายหน้าต่าง มีตัวเลข Type อะไรยิงเข้ามาก่อกวน */
    AccelDebug("newUserClient: PROBE TRIGGERED! type=0x%X from task=%p", (unsigned int)type, resettingTask);

    if (type == kMyIntelAccelInfoClientType) {
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
        AccelDebug("newUserClient: info client created successfully (task %p)", resettingTask);
        return kIOReturnSuccess;
    }

    /* 🚨 ท่อพักสายชั่วคราว (Pass-through Test): ถ้า WindowServer ยิง Type อื่น (เช่น 0, 1, 2) มาขอเปิดคลาส 
     * เราจะแกล้งทำเป็นยอมรับ เพื่อหลอกไม่ให้ WindowServer ปฏิเสธการทำงานและสั่งเด้งกลับไปใช้ CPU 
     * รอดูหน้างานสดๆ เลยว่าอาการหน่วงแล็กส้นตีนตอนย่อแอปจะหายไปหรือไม่ */
    if (type >= 0 && type <= 0x30) {
        MyIntelAccelClient *testClient = new MyIntelAccelClient;
        if (testClient) {
            if (testClient->initWithTask(resettingTask, securityID, type, properties) &&
                testClient->attach(this) &&
                testClient->start(this)) {
                *handler = testClient;
                AccelDebug("newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x%X (WindowServer Soft-Lock Bypass)", (unsigned int)type);
                return kIOReturnSuccess;
            }
            if (testClient) testClient->release();
        }
    }

    /* ปฏิเสธเสียงแข็งสำหรับคีย์ขยะแปลกปลอมตัวอื่นๆ นอกเหนือช่วงทดสอบ */
    AccelDebug("newUserClient: unsupported type 0x%X from task %p — REJECTING CLEANLY", (unsigned int)type, resettingTask);
    return kIOReturnUnsupported;
}

bool MyIntelAccelerator::publishProperties(void)
{
    bool ok = true;

    /* model — System Profiler GPU naming (real silicon: Raptor Lake-U 0xA7AC) */
    setProperty("model", "Intel Raptor Lake-U Graphics (Core 5 120U)");

    /* IOAccelIndex — standard accelerator ordinal */
    setProperty("IOAccelIndex", 0ULL, 32);

    /* IOSourceVersion — 6-byte OSData (SP reads it) */
    {
        uint8_t srcVer[6] = { 0, 0, 0, 0, 0, 0 };
        OSData *d = OSData::withBytes(srcVer, sizeof(srcVer));
        if (d) {
            setProperty("IOSourceVersion", d);
            d->release();
        }
    }

    /* IOAccelDisplayPipeCapabilities — explicitly DENY display pipe so
     * IOAcceleratorFamily2 (if ever loaded) never calls createDisplayPipe
     * against us. */
    {
        OSDictionary *caps = OSDictionary::withCapacity(2);
        if (caps) {
            caps->setObject("DisplayPipeSupported", kOSBooleanFalse);
            caps->setObject("TransactionsSupported", kOSBooleanFalse);
            setProperty("IOAccelDisplayPipeCapabilities", caps);
            caps->release();
        }
    }

    /* IOCFPlugInTypes — legacy 2D-plugin discovery UUID */
    {
        OSDictionary *plug = OSDictionary::withCapacity(1);
        if (plug) {
            OSString *name = OSString::withCString(kMyIntelAccelCFPlugInName);
            if (name) {
                plug->setObject(kMyIntelAccelCFPlugInUUID, name);
                name->release();
            }
            setProperty("IOCFPlugInTypes", plug);
            plug->release();
        }
    }

    /* PerformanceStatistics — telemetry surface for SP/AGPM/WindowServer.
     * Adding "Counter" (monotonic uint64) + "GPUActivityInPercent" (0..100)
     * satisfies AGPM's poll contract — it stops re-querying on every vsync
     * once these keys are present and stable. */
    {
        OSDictionary *perf = OSDictionary::withCapacity(4);
        if (perf) {
            uint64_t poolBytes = fGPU ? fGPU->getVramPoolSize() : 0;
            OSNumber *total   = OSNumber::withNumber(poolBytes, 64);
            OSNumber *freeN   = OSNumber::withNumber(poolBytes, 64);
            OSNumber *counter = OSNumber::withNumber((uint64_t)0, 64);
            OSNumber *gpuAct  = OSNumber::withNumber((uint64_t)0, 32);
            if (total)   { perf->setObject("vramTotalBytes",        total);   total->release();   }
            if (freeN)   { perf->setObject("vramFreeBytes",         freeN);   freeN->release();   }
            if (counter) { perf->setObject("Counter",               counter); counter->release(); }
            if (gpuAct)  { perf->setObject("GPUActivityInPercent",  gpuAct);  gpuAct->release();  }
            setProperty("PerformanceStatistics", perf);
            perf->release();
        }
    }

    /* IOAccelRevision + IOAccelTypes — WindowServer caches these on first
     * match; without them it re-probes every time the display wakes. */
    setProperty("IOAccelRevision", (uint64_t)0x0001, 32);
    setProperty("IOAccelTypes",    (uint64_t)0x0001, 32);

    /* Media/Metal stub properties removed — not needed for GPU compositing */

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
    AccelDebug("AccelClient clientClose — surface backing persists at provider");
    terminate();
    return kIOReturnSuccess;
}

IOReturn MyIntelAccelClient::externalMethod(uint32_t selector,
                                            IOExternalMethodArguments *arguments,
                                            IOExternalMethodDispatch *dispatch,
                                            OSObject *target,
                                             void *reference)
{
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
                if (ent->backing) { gemBufferDestroy(ent->backing, gpu->getGsm()); ent->backing = NULL; ent->bytes = 0; }
                ent->backing = (MyIntelGEMBuffer*)gemBufferCreate(need, 0, gpu->getGsm(), gpu->getGttTotal(), (void*)gpu->getApertureVA(), gpu->getApertureSize());
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
                if (ent->backing) { gemBufferDestroy(ent->backing, gpu->getGsm()); ent->backing = NULL; ent->bytes = 0; }
                ent->backing = (MyIntelGEMBuffer*)gemBufferCreate(need, 0, gpu->getGsm(), gpu->getGttTotal(), (void*)gpu->getApertureVA(), gpu->getApertureSize());
                if (ent->backing) { ent->w = fShapeW; ent->h = fShapeH; ent->bytes = need; IOLog("MyIntelGPU: SURFACE backing alloc sid=%u %ux%u @ggtt=0x%X\n", fSurfaceID, fShapeW, fShapeH, ent->backing->ggttOffset); }
            }
        }
        return kIOReturnSuccess;
    }
    case 10: { // Flush — bridge blit if mybridge active, else dummy flush
        MyIntelGPU *gpu = fAccel ? fAccel->getGPU() : NULL;
        MyIntelRing *bcs = gpu ? gpu->getRingBCS() : NULL;
        MyIntelRingCallbacks *cb = gpu ? gpu->getRingCallbacks() : NULL;
        MyIntelGEMBuffer *bridge = gpu ? gpu->getBridgeBuf() : NULL;
        MyIntelAccelerator::SurfaceEntry *ent =
            fAccel ? fAccel->surfaceFindOrCreate(fSurfaceID) : NULL;
        // B bridge: WS surface -> scanout buffer (real compositing path)
        if (bridge && ent && ent->backing && ent->backing->cpuAddr &&
            bcs && cb && ringIsInitialized(bcs) && ent->backing->ggttOffset) {
            // Ensure bridge size covers surface; if not, fallback to dummy flush
            uint32_t w = ent->w ? ent->w : fShapeW;
            uint32_t h = ent->h ? ent->h : fShapeH;
            if (w && h && w <= 8192 && h <= 4320) {
                if (bcs && cb && ringIsInitialized(bcs) &&
                    ringEmitSurfacePresent(bcs, bridge->ggttOffset, 7680,
                                           ent->backing->ggttOffset, w * 4,
                                           (uint16_t)w, (uint16_t)h)) {
                    ringEmitFlushDW(bcs, true, false);
                    ringSubmit(bcs, cb);
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
    if (outputCount < sizeof(MyIntelAccelInfo) / sizeof(uint64_t)) {
        return kIOReturnOverrun;
    }

    MyIntelAccelInfo info = {};
    info.accelID       = client->fAccel->getAccelID();

    MyIntelGPU *gpu = client->fAccel->getGPU();
    if (gpu) {
        info.vramPoolMB    = (uint32_t)(gpu->getVramPoolSize() / (1024ULL * 1024));
        info.gttTotalPages = gpu->getGttTotal();
    }

    output[0] = 0;
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
        gemBufferDestroy(e->backing, (uint32_t *)fGPU->getGsm());
        e->backing = NULL;
    }
    e->sid = sid;
    e->colorMode = 0;
    e->w = e->h = 0;
    e->bytes = 0;
    if (fSurfaceLock) IOLockUnlock(fSurfaceLock);
    return e;
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
            gemBufferDestroy(fSurfaces[i].backing, gsm);
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
