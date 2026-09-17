#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/accelerator/IOAccelDevice.h>

#include "IntelXeAccelerator.hpp"
#include "IntelXeGPU.hpp"

// ใช้ native IOAccelerator แทน IOService
OSDefineMetaClassAndStructors(IntelXeAccelerator, IOAccelerator)

bool IntelXeAccelerator::init(OSDictionary *dictionary) {
    if (!IOAccelerator::init(dictionary)) {
        return false;
    }
    
    IOLog("IntelXeAccelerator::init - Initializing Intel Xe Graphics Accelerator\n");
    
    // Initialize member variables
    fParentGPU = NULL;
    fMetalAccelerator = NULL;
    fWorkLoop = NULL;
    fCommandQueue = NULL;
    fInterruptSource = NULL;
    
    fFramebuffer = NULL;
    fFramebufferPhys = 0;
    fFramebufferSize = 0;
    
    fDeviceID = 0;
    fRevisionID = 0;
    fSubsystemID = 0;
    
    return true;
}

bool IntelXeAccelerator::start(IOService *provider) {
    if (!IOAccelerator::start(provider)) {
        return false;
    }

    IOLog("IntelXeAccelerator::start - Starting Intel Xe 2D/3D Acceleration Engine\n");

    // Get parent GPU device (IOPCIDevice)
    fParentGPU = OSDynamicCast(IOPCIDevice, provider);
    if (!fParentGPU) {
        IOLog("IntelXeAccelerator::start - ERROR: Cannot get parent PCI device\n");
        return false;
    }
    
    // Get device information
    fDeviceID = fParentGPU->configRead32(0x02);
    fRevisionID = fParentGPU->configRead32(0x08) & 0xFF;
    fSubsystemID = fParentGPU->configRead32(0x2C);
    
    IOLog("IntelXeAccelerator: Device ID: 0x%04X, Revision: 0x%02X, Subsystem: 0x%04X\n",
          fDeviceID, fRevisionID, fSubsystemID);
    
    // Create work loop for async operations
    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop) {
        IOLog("IntelXeAccelerator::start - ERROR: Cannot create work loop\n");
        return false;
    }
    
    // Initialize PCI resources
    if (!initializePCI()) {
        IOLog("IntelXeAccelerator::start - ERROR: PCI initialization failed\n");
        return false;
    }
    
    // Initialize memory (framebuffer)
    if (!initializeMemory()) {
        IOLog("IntelXeAccelerator::start - ERROR: Memory initialization failed\n");
        return false;
    }
    
    // Initialize interrupts (optional)
    if (!initializeInterrupts()) {
        IOLog("IntelXeAccelerator::start - WARNING: Interrupt initialization failed (non-fatal)\n");
    }
    
    // Initialize Metal support
    if (!initializeMetalSupport()) {
        IOLog("IntelXeAccelerator::start - WARNING: Metal support initialization failed\n");
        // Continue without Metal support
    }
    
    // Initialize GPU engine
    if (!initializeGPU()) {
        IOLog("IntelXeAccelerator::start - ERROR: GPU engine initialization failed\n");
        return false;
    }
    
    // Create command queue for GPU commands
    fCommandQueue = IOCommandQueue::commandQueue(this, fWorkLoop);
    if (!fCommandQueue) {
        IOLog("IntelXeAccelerator::start - WARNING: Cannot create command queue\n");
    } else {
        fWorkLoop->addEventSource(fCommandQueue);
    }
    
    // Register with IORegistry as accelerator service
    setProperty("IOAccelerator", kOSBooleanTrue);
    setProperty("IOAcceleratorType", "GPU");
    setProperty("VendorID", fParentGPU->configRead32(0x00), 32);
    setProperty("DeviceID", fDeviceID, 32);
    setProperty("RevisionID", fRevisionID, 32);
    setProperty("Model", "Intel Iris Xe Graphics (Raptor Lake-P)");
    
    // Register service for user clients
    registerService();
    
    IOLog("IntelXeAccelerator::start - Accelerator started successfully\n");
    return true;
}

void IntelXeAccelerator::stop(IOService *provider) {
    IOLog("IntelXeAccelerator::stop - Shutting down acceleration engine\n");
    
    // Clean up command queue
    if (fCommandQueue && fWorkLoop) {
        fWorkLoop->removeEventSource(fCommandQueue);
        fCommandQueue->release();
        fCommandQueue = NULL;
    }
    
    // Clean up work loop
    if (fWorkLoop) {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }
    
    // Clean up Metal accelerator
    if (fMetalAccelerator) {
        fMetalAccelerator->release();
        fMetalAccelerator = NULL;
    }
    
    // Clean up interrupts
    if (fInterruptSource && fWorkLoop) {
        fInterruptSource->disable();
        fWorkLoop->removeEventSource(fInterruptSource);
        fInterruptSource->release();
        fInterruptSource = NULL;
    }
    
    // Clean up memory mappings
    if (fBAR0Map) {
        fBAR0Map->release();
        fBAR0Map = NULL;
    }
    
    if (fConfigMap) {
        fConfigMap->release();
        fConfigMap = NULL;
    }
    
    // Free framebuffer
    if (fFramebuffer) {
        fFramebuffer->release();
        fFramebuffer = NULL;
        fFramebufferPhys = 0;
        fFramebufferSize = 0;
    }
    
    IOAccelerator::stop(provider);
}

void IntelXeAccelerator::free(void) {
    IOLog("IntelXeAccelerator::free - Releasing accelerator resources\n");
    
    // Additional cleanup if needed
    IOAccelerator::free();
}

// Native IOAccelerator method for creating user clients
IOReturn IntelXeAccelerator::newUserClient(task_t replyingTask, void *securityID,
                                           UInt32 type, OSDictionary *properties,
                                           IOUserClient **handler) {
    IOLog("IntelXeAccelerator::newUserClient - User space connection request (Type: %u)\n", 
          (unsigned int)type);
    
    if (!handler) {
        return kIOReturnBadArgument;
    }
    
    // Create appropriate user client based on type
    IOUserClient *client = NULL;
    IOReturn result = kIOReturnSuccess;
    
    switch (type) {
        case kIOAcceleratorType2D:
            // Create 2D acceleration client
            // client = IntelXe2DClient::withTask(replyingTask);
            IOLog("IntelXeAccelerator::newUserClient - 2D acceleration client requested\n");
            result = kIOReturnUnsupported;
            break;
            
        case kIOAcceleratorType3D:
            // Create 3D/Metal client
            // client = IntelXe3DClient::withTask(replyingTask);
            IOLog("IntelXeAccelerator::newUserClient - 3D/Metal acceleration client requested\n");
            result = kIOReturnUnsupported;
            break;
            
        case kIOAcceleratorTypeVideo:
            // Create video acceleration client
            // client = IntelXeVideoClient::withTask(replyingTask);
            IOLog("IntelXeAccelerator::newUserClient - Video acceleration client requested\n");
            result = kIOReturnUnsupported;
            break;
            
        default:
            IOLog("IntelXeAccelerator::newUserClient - Unknown client type: %u\n", 
                  (unsigned int)type);
            result = kIOReturnUnsupported;
            break;
    }
    
    if (client) {
        // Initialize the client
        if (client->initWithTask(replyingTask, securityID, type, properties)) {
            client->attach(this);
            client->start(this);
            *handler = client;
            result = kIOReturnSuccess;
        } else {
            client->release();
            result = kIOReturnError;
        }
    } else {
        *handler = NULL;
    }
    
    return result;
}

// Native accelerator info method (required by IOAccelerator)
IOReturn IntelXeAccelerator::getAcceleratorInfo(IOAccelInfo *info) {
    if (!info) {
        return kIOReturnBadArgument;
    }
    
    // Fill accelerator information structure
    memset(info, 0, sizeof(IOAccelInfo));
    
    info->version = kIOAcceleratorVersion2;
    info->acceleratorType = kIOAcceleratorTypeGPU;
    info->vendorID = 0x8086;  // Intel
    info->deviceID = fDeviceID;
    info->revisionID = fRevisionID;
    
    if (fParentGPU) {
        info->busID = fParentGPU->getBusNumber();
        info->deviceID = fParentGPU->getDeviceNumber();
        info->functionID = fParentGPU->getFunctionNumber();
    }
    
    // Framebuffer information
    info->framebufferAddress = fFramebufferPhys;
    info->framebufferSize = fFramebufferSize;
    
    // Capabilities
    info->caps = kIOAccelCap2D | kIOAccelCap3D | kIOAccelCapVideo;
    
    // Add Metal capability if supported
    if (fMetalAccelerator) {
        info->caps |= kIOAccelCapMetal;
    }
    
    // Feature flags
    info->features = 0;
    
    // Maximum dimensions
    info->maxWidth = 8192;    // 8K
    info->maxHeight = 4320;   // 8K
    info->maxDepth = 1;       // 2D only
    
    // Color formats
    info->supportedColorSpaces = kIOAccelColorSpaceRGB;
    
    return kIOReturnSuccess;
}

// Helper function to handle GPU commands
IOReturn IntelXeAccelerator::handleCommand(uint32_t command, void *data, size_t dataSize) {
    IOLog("IntelXeAccelerator::handleCommand - Processing command: 0x%08X\n", command);
    
    // TODO: Implement actual command processing
    switch (command) {
        case kAccelCommandFlush:
            // Flush GPU pipeline
            IOLog("IntelXeAccelerator::handleCommand - Flush command\n");
            break;
            
        case kAccelCommandReset:
            // Reset GPU engine
            IOLog("IntelXeAccelerator::handleCommand - Reset command\n");
            break;
            
        case kAccelCommandGetInfo:
            // Get GPU information
            IOLog("IntelXeAccelerator::handleCommand - GetInfo command\n");
            break;
            
        default:
            IOLog("IntelXeAccelerator::handleCommand - Unknown command: 0x%08X\n", command);
            return kIOReturnUnsupported;
    }
    
    return kIOReturnSuccess;
}
