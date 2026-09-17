#ifndef __INTEL_XE_ACCELERATOR_HPP__
#define __INTEL_XE_ACCELERATOR_HPP__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandQueue.h>
#include <IOKit/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/graphics/IOGraphicsAccelerator2.h>

// Forward declaration to prevent circular includes
class IOPCIDevice;
class IOGraphicsAccelerator2;

class IntelXeAccelerator : public IOAccelerator {
    OSDeclareDefaultStructors(IntelXeAccelerator)
    
public:
    /* IOKit Service Lifecycle Methods */
    virtual bool init(OSDictionary *dictionary) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free(void) override;

    /* Native IOAccelerator Methods */
    virtual IOReturn newUserClient(task_t replyingTask, void *securityID,
                                   UInt32 type, OSDictionary *properties,
                                   IOUserClient **handler) override;
    
    virtual IOReturn getAcceleratorInfo(IOAccelInfo *info) override;
    virtual IOReturn createContext(IOAccelContext* context) override;
    virtual IOReturn destroyContext(IOAccelContext* context) override;
    virtual IOReturn createSurface(IOAccelSurface* surface) override;
    virtual IOReturn destroySurface(IOAccelSurface* surface) override;
    
    /* Metal Support */
    virtual IOReturn createMetalDevice(IOGraphicsAccelerator2** device) override;
    
    /* Command Processing */
    virtual IOReturn handleCommand(uint32_t command, void *data = NULL, size_t dataSize = 0);
    
private:
    // Parent PCI device (Intel Xe GPU)
    IOPCIDevice*           fParentGPU;
    
    // Metal acceleration support
    IOGraphicsAccelerator2* fMetalAccelerator;
    
    // Work loop for async operations
    IOWorkLoop*           fWorkLoop;
    
    // Command queue for GPU commands
    IOCommandQueue*       fCommandQueue;
    
    // Interrupt handling
    IOInterruptEventSource* fInterruptSource;
    
    // Memory mappings
    IOMemoryMap*          fBAR0Map;    // GPU registers
    IOMemoryMap*          fConfigMap;  // PCI config space
    
    // Framebuffer memory
    IOBufferMemoryDescriptor* fFramebuffer;
    IOPhysicalAddress    fFramebufferPhys;
    vm_size_t            fFramebufferSize;
    
    // Device information
    UInt32               fDeviceID;
    UInt32               fRevisionID;
    UInt32               fSubsystemID;
    
    /* Initialization Methods */
    bool initializePCI();
    bool initializeMemory();
    bool initializeInterrupts();
    bool initializeMetalSupport();
    bool initializeGPU();
    
    /* Hardware Access Methods */
    UInt32 readConfig32(UInt32 offset);
    void writeConfig32(UInt32 offset, UInt32 value);
    UInt32 readBAR32(UInt32 offset);
    void writeBAR32(UInt32 offset, UInt32 value);
    
    /* GPU Engine Methods */
    bool detectGPU();
    void setupDisplayEngine();
    void setupRenderEngine();
    
    /* Interrupt Handler */
    static void handleInterrupt(OSObject* owner, IOInterruptEventSource* source, int count);
    
    /* Memory Management */
    IOReturn allocateFramebuffer(vm_size_t size);
    void freeFramebuffer();
    
    /* User Client Types */
    enum {
        kIOAcceleratorType2D    = 0,    // 2D acceleration
        kIOAcceleratorType3D    = 1,    // 3D/Metal acceleration
        kIOAcceleratorTypeVideo = 2,    // Video acceleration
        kIOAcceleratorTypeAll   = 3     // All acceleration types
    };
    
    /* Command Definitions */
    enum {
        kAccelCommandFlush      = 0x1000,  // Flush GPU pipeline
        kAccelCommandReset      = 0x1001,  // Reset GPU engine
        kAccelCommandGetInfo    = 0x1002,  // Get GPU information
        kAccelCommandSubmit2D   = 0x2000,  // Submit 2D command
        kAccelCommandSubmit3D   = 0x3000,  // Submit 3D command
        kAccelCommandSubmitCS   = 0x4000   // Submit compute shader
    };
};

#endif /* __INTEL_XE_ACCELERATOR_HPP__ */
