#ifndef __MY_INTEL_VCS_CLIENT_H__
#define __MY_INTEL_VCS_CLIENT_H__

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include "MyIntelGEMBuffer.hpp"
#include "myintel_vcs_user.h"

class MyIntelGPU;

/*
 * MyIntelVCSClient — IOUserClient for VCS command submission
 *
 * Provides user-space interface to VCS (Video Codec Service) ring buffer.
 * Apps (FFmpeg/VA-API port) open /dev/myintel_vcs and submit H.264/HEVC
 * decode command streams via externalMethod().
 *
 * External method selectors (see myintel_vcs_user.h):
 *   0 = submitCommandBuffer  — submit VCS commands to ring
 *   1 = waitForCompletion    — wait for VCS decode to finish
 *   2 = getVCSStatus         — query VCS engine status
 *   3 = mapOutputBuffer      — map decoded frame buffer to user-space
 *   4 = unmapOutputBuffer    — unmap decoded frame buffer
 *   5 = getContextInfo       — get VCS context info (codec, resolution, etc.)
 *   6 = submitVCSWorkload    — submit raw MFX/HCP dwords to VCS ring
 *   7 = gemCreate            — Phase 9: allocate GEM buffer (Source/Surface)
 *   8 = gemDestroy           — Phase 9: free GEM buffer
 *   9 = gemGetInfo           — Phase 9: query GEM buffer (ggtt/size/flags)
 *
 * Phase 9 GEM: user-space reserves GGTT-bound buffers via selector 7, then
 * CPU-maps them via clientMemoryForType(MYVCS_GEM_MAP_BASE + handle) for
 * zero-copy bitstream upload / decoded frame readback.
 */

/* Per-client GEM handle table (Phase 9) */
#define VCS_GEM_MAX_HANDLES 32

struct VCSGemSlot {
    MyIntelGEMBuffer *buf;
    bool              active;
};
class MyIntelVCSClient : public IOUserClient {
    OSDeclareDefaultStructors(MyIntelVCSClient)

public:
    virtual bool initWithTask(task_t owningTask, void *securityToken,
                              UInt32 type, OSDictionary *properties) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;
    virtual IOReturn clientClose(void) override;

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch,
                                    OSObject *target, void *reference) override;

    virtual IOReturn clientMemoryForType(uint32_t type, uint32_t *flags,
                                         IOMemoryDescriptor **memory) override;

    static IOReturn submitCommandBuffer(OSObject *target, void *reference,
                                        IOExternalMethodArguments *args);
    static IOReturn waitForCompletion(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args);
    static IOReturn getVCSStatus(OSObject *target, void *reference,
                                 IOExternalMethodArguments *args);
    static IOReturn mapOutputBuffer(OSObject *target, void *reference,
                                    IOExternalMethodArguments *args);
    static IOReturn unmapOutputBuffer(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args);
    static IOReturn getContextInfo(OSObject *target, void *reference,
                                   IOExternalMethodArguments *args);
    static IOReturn submitVCSWorkload(OSObject *target, void *reference,
                                      IOExternalMethodArguments *args);
    static IOReturn gemCreate(OSObject *target, void *reference,
                              IOExternalMethodArguments *args);
    static IOReturn gemDestroy(OSObject *target, void *reference,
                               IOExternalMethodArguments *args);
    static IOReturn gemGetInfo(OSObject *target, void *reference,
                               IOExternalMethodArguments *args);
    /* 2.0.251: engine-level VDBOX reset (recover from ESR halt w/o reboot) */
    static IOReturn engineReset(OSObject *target, void *reference,
                                IOExternalMethodArguments *args);
    /* 2.0.252: read-only engine state dump (LRC head/tail, CSB, scratch) */
    static IOReturn engineDiag(OSObject *target, void *reference,
                               IOExternalMethodArguments *args);

private:
    /* Phase 9 — release every GEM buffer still owned by this client */
    void freeAllGemBuffers(void);

    MyIntelGPU *fProvider;
    task_t      fTask;
    uint32_t    fOpenCount;
    VCSGemSlot  fGemSlots[VCS_GEM_MAX_HANDLES];
};

#endif /* __MY_INTEL_VCS_CLIENT_H__ */
