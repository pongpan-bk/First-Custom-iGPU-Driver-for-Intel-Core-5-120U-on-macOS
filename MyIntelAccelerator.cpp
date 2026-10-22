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

    /*     /* model — System Profiler GPU naming (real silicon, this platform) */
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

    /* Probe-only bundle names (boot-arg myintelaccelprobe=1): point at
     * DELIBERATELY NONEXISTENT bundles so Metal.framework's probe fails
     * gracefully and log stream shows who reads them. NEVER point these
     * at real ICL/KBL bundles — they would load and talk to a kernel ABI
     * we don't have (WindowServer crash risk). */
    {
        uint32_t probeArg = 0;
        if (PE_parse_boot_argn("myintelaccelprobe", &probeArg, sizeof(probeArg)) && probeArg != 0) {
            setProperty(DEC_BUFFER_MetalPlugin, "MyIntelGPUMTLDriver");
            setProperty(DEC_BUFFER_IOGLBundle,  "MyIntelGPUGLDriver");
            AccelDebug("Probe mode active: parameters injected");
        }
    }

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
    AccelDebug("AccelClient clientClose");
    terminate();
    return kIOReturnSuccess;
}

IOReturn MyIntelAccelClient::externalMethod(uint32_t selector,
                                            IOExternalMethodArguments *arguments,
                                            IOExternalMethodDispatch *dispatch,
                                            OSObject *target,
                                            void *reference)
{
    switch (selector) {
    case kMyIntelAccelSelectorGetInfo:
        return sGetInfo(this,
                        (const uint64_t *)arguments->scalarInput,
                        (uint32_t)arguments->scalarInputCount,
                        (uint64_t *)arguments->scalarOutput,
                        (uint32_t)arguments->scalarOutputCount);
    case 7:
    case 9:
        // Metal Phase 1 dummy: WindowServer probes selectors 7 then 9 in sequence
        // Return zeroed output to let it proceed past soft-lock
        if (arguments->scalarOutput && arguments->scalarOutputCount > 0) {
            for (uint32_t i = 0; i < arguments->scalarOutputCount; i++) {
                ((uint64_t *)arguments->scalarOutput)[i] = 0;
            }
        }
        if (arguments->structureOutput && arguments->structureOutputSize > 0) {
            memset(arguments->structureOutput, 0, arguments->structureOutputSize);
        }
        AccelDebug("AccelClient selector %u dummy success", (unsigned int)selector);
        return kIOReturnSuccess;
    default:
        // For experimental pass-through types 0x0-0x30, return dummy success
        // instead of unsupported to keep WindowServer from soft-locking
        if (selector <= 0x30) {
            if (arguments->scalarOutput && arguments->scalarOutputCount > 0) {
                for (uint32_t i = 0; i < arguments->scalarOutputCount; i++) {
                    ((uint64_t *)arguments->scalarOutput)[i] = 0;
                }
            }
            if (arguments->structureOutput && arguments->structureOutputSize > 0) {
                memset(arguments->structureOutput, 0, arguments->structureOutputSize);
            }
            AccelDebug("AccelClient selector %u dummy success (generic 0x0-0x30)", (unsigned int)selector);
            return kIOReturnSuccess;
        }
        AccelDebug("AccelClient externalMethod: unknown selector %u", (unsigned int)selector);
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
