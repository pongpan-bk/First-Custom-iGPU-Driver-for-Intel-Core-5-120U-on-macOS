#ifndef MyIntelGPUClient_hpp
#define MyIntelGPUClient_hpp

#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "MyIntelGPU.hpp"

class MyIntelGPUClient : public IOUserClient {
    OSDeclareDefaultStructors(MyIntelGPUClient)

private:
    // Per-client handle registry (Oracle #1: raw-pointer deref of userspace input
    // = kernel panic primitive; registry lookup uses pointer COMPARE only, never
    // derefs a user-supplied address). Fixed-size C array because GEM buffers
    // are C structs freed with IOFree(), not OSObjects.
    static const uint32_t kMaxClientBuffers = 16;
    MyIntelGEMBuffer* fBuffers[kMaxClientBuffers];
    uint32_t          fBufferCount;

    MyIntelGPU* fProvider;   // retained in start(), released in free() (Oracle #2: UAF fix)
    task_t      fClientTask;
    // Dirty-ring shared memory for selector 10 (IOAccelSharedSetupDirtyRing)
    IOMemoryDescriptor* fDirtyRingMD;
    IOMemoryMap*        fDirtyRingMap;
    uint64_t            fDirtyRingUserVA;

public:
    virtual bool initWithTask(task_t owningTask, void * securityToken, UInt32 type) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual void free() override;
    virtual IOReturn clientClose() override;

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
                                    IOExternalMethodDispatch * dispatch = NULL,
                                    OSObject * target = NULL, void * reference = NULL) override;
};

#endif /* MyIntelGPUClient_hpp */
