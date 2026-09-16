/*===========================================================================
 *  MyIntelAccelerator.hpp
 *  MyIntelGPU.kext — IOAccelerator binding (Phase 6c)
 *
 * Publishes an IOAccelerator service under the GPU so macOS userspace
 * (system_profiler / ioreg / Metal probing) sees this device as an
 * accelerator. Deliberately minimal and fail-safe: no family ABI
 * (IOAccelDevice2) — that requires closed IOGraphicsAccelerator2
 * provider virtuals and would panic against a plain IOAccelerator.
 *
 * Gated by boot-arg "myintelaccel" (see MyIntelGPU.cpp Phase 6c):
 *   absent   -> nothing created (byte-identical GPU-only behavior)
 *   =1       -> attach under MyIntelGPU node
 *   =2       -> attach under the IOPCIDevice (Apple-style topology)
 *=========================================================================*/

#ifndef __MY_INTEL_ACCELERATOR_HPP__
#define __MY_INTEL_ACCELERATOR_HPP__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/graphics/IOAccelSurfaceConnect.h>
#include "MyIntelGEMBuffer.hpp"

class MyIntelGPU;

/* Attach topology modes (boot-arg myintelaccel value) */
#define kMyIntelAccelAttachUnderGPU   1
#define kMyIntelAccelAttachUnderPCI   2

/* Private user-client connect type (above kIOAccelNumClientTypes space).
 * Diagnostic-only client: returns a fixed-size info struct. */
#define kMyIntelAccelInfoClientType   0x100

/* Diagnostic info returned by selector 0 of MyIntelAccelClient */
struct MyIntelAccelInfo {
    uint32_t accelID;        /* family-issued IOAccelID (0 if invalid) */
    uint32_t vramPoolMB;     /* GPU VRAM pool size in MB */
    uint32_t gttTotalPages;  /* GGTT total pages */
    uint32_t engines;        /* bit0 = RCS, bit1 = BCS present */
};

/* Selector index for MyIntelAccelClient::externalMethod */
#define kMyIntelAccelSelectorGetInfo  0

class MyIntelAccelerator : public IOAccelerator {
    OSDeclareDefaultStructors(MyIntelAccelerator)

public:
    /*! @brief Factory — alloc + init only. Caller attaches and starts.
     *  @param gpu        owning MyIntelGPU (NOT retained — parent owns us)
     *  @param attachMode kMyIntelAccelAttachUnderGPU / UnderPCI */
    static MyIntelAccelerator *withGPU(MyIntelGPU *gpu, IOOptionBits attachMode);

    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;

    /*! @brief User-client factory.
     *  kMyIntelAccelInfoClientType -> diagnostic client.
     *  Standard IOGraphicsAccelerator2 numeric types (0-9, 0x20, 0x21)
     *  return kIOReturnUnsupported (logged — evidence of who probes). */
    virtual IOReturn newUserClient(task_t resettingTask, void *securityID,
                                   UInt32 type, OSDictionary *properties,
                                   IOUserClient **handler) override;

    IOOptionBits getAttachMode(void) const { return fAttachMode; }
    /* Family-issued accel ID (0 if createAccelID failed) */
    IOAccelID getAccelID(void) const { return fAccelIDValid ? fAccelID : 0; }
    MyIntelGPU *getGPU(void) const { return fGPU; }

    /* Mission C Phase 3b — persistent surface backing.
     * Survives client open/close so WindowServer's reopen loop
     * doesn't churn 135MB GEM allocations. */
    struct SurfaceEntry {
        uint32_t sid;
        uint32_t colorMode;
        uint32_t w;
        uint32_t h;
        MyIntelGEMBuffer *backing;
        uint64_t bytes;
    };
    SurfaceEntry *surfaceFindOrCreate(uint32_t sid);
    void surfaceReleaseAll(void);
    void blitSurfaceToFramebuffer(mach_vm_address_t src_user_addr,
                                  uint32_t width, uint32_t height, uint32_t stride);

private:
    /*! @brief Publish the ICL-style property vocabulary (see .cpp). */
    bool publishProperties(void);

    MyIntelGPU *fGPU;          /* not retained — parent holds us */
    IOOptionBits fAttachMode;
    IOAccelID    fAccelID;
    bool         fAccelIDValid;

    static const int kMaxSurfaces = 16;
    SurfaceEntry fSurfaces[16];
    int          fSurfEvict;   /* round-robin eviction when full */
    IOLock      *fSurfaceLock; /* guards table ops across WS threads */
};

/*! @brief Read-only diagnostic user client for the accelerator node.
 *  One selector (kMyIntelAccelSelectorGetInfo), fixed-size output,
 *  no pointer arguments from userspace — no arbitrary derefs. */
class MyIntelAccelClient : public IOUserClient {
    OSDeclareDefaultStructors(MyIntelAccelClient)

public:
    virtual bool initWithTask(task_t owningTask, void *securityToken,
                              UInt32 type, OSDictionary *properties) override;
    virtual bool start(IOService *provider) override;
    virtual IOReturn clientClose(void) override;
    virtual IOReturn externalMethod(uint32_t selector,
                                    IOExternalMethodArguments *arguments,
                                    IOExternalMethodDispatch *dispatch,
                                    OSObject *target,
                                    void *reference) override;

    /* Static action for the dispatch table (referenced at file scope) */
    static IOReturn sGetInfo(MyIntelAccelClient *client,
                             const uint64_t *input, uint32_t inputCount,
                             uint64_t *output, uint32_t outputCount);

private:
    MyIntelAccelerator *fAccel;  /* provider, not retained */

    uint32_t fSurfaceID;  /* last SetIDMode target */
    uint32_t fColorMode;
    uint32_t fShapeW;
    uint32_t fShapeH;
};

#endif /* __MY_INTEL_ACCELERATOR_HPP__ */
