/*===========================================================================
 *  MyIntelVCSClient.cpp
 *  Hackintosh Kext — Video Codec User Client Executive (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#include "MyIntelVCSClient.h"
#include "MyIntelGPU.hpp"
#include "MyIntelVCSCommand.h"

OSDefineMetaClassAndStructors(MyIntelVCSClient, IOUserClient)

/* กำหนดตารางสิทธิ์การเชื่อมต่อใช้งาน Selector 0-3 ตามโครงสร้างสัญญาในไฟล์สัญญาผู้ใช้ */
static const IOExternalMethodDispatch sVcsMethods[kMyIntelUserClientMaxSelectors] = {
    { (IOExternalMethodAction)MyIntelVCSClient::actionOpenClient,  0, 0, 0, 0 }, // Selector 0
    { (IOExternalMethodAction)MyIntelVCSClient::actionCloseClient, 0, 0, 0, 0 }, // Selector 1
    { (IOExternalMethodAction)NULL,                               0, 0, 0, 0 }, // Selector 2 (Alloc VRAM)
    { (IOExternalMethodAction)MyIntelVCSClient::actionSubmitDecode, 0, sizeof(struct myintel_vcs_decode_payload), 0, 0 } // Selector 3
};

bool MyIntelVCSClient::initWithTask(task_t owningTask, void *securityID, UInt32 type) {
    if (!IOUserClient::initWithTask(owningTask, securityID, type)) {
        return false;
    }
    fOwningTask = owningTask;
    fClientActive = false;
    fProviderGPU = NULL;
    
    IOLog("MyIntelVCSClient::initWithTask - Connection initialized from Userspace\n");
    return true;
}

bool MyIntelVCSClient::start(IOService *provider) {
    if (!IOUserClient::start(provider)) {
        return false;
    }
    
    fProviderGPU = OSDynamicCast(MyIntelGPU, provider);
    if (!fProviderGPU) {
        return false;
    }
    
    IOLog("MyIntelVCSClient::start - User Client attached to Main GPU Driver\n");
    return true;
}

void MyIntelVCSClient::stop(IOService *provider) {
    IOLog("MyIntelVCSClient::stop - User Client active session torn down\n");
    fClientActive = false;
    IOUserClient::stop(provider);
}

void MyIntelVCSClient::free() {
    IOLog("MyIntelVCSClient::free - Destroying client mapping context\n");
    IOUserClient::free();
}

IOReturn MyIntelVCSClient::externalMethod(UInt32 selector, IOExternalMethodArguments *arguments,
                                         IOExternalMethodDispatch *dispatch, OSObject *target,
                                         void *reference) {
    if (selector >= kMyIntelUserClientMaxSelectors) {
        return kIOReturnBadArgument;
    }
    
    dispatch = (IOExternalMethodDispatch *)&sVcsMethods[selector];
    target = this;
    
    return IOUserClient::externalMethod(selector, arguments, dispatch, target, reference);
}

IOReturn MyIntelVCSClient::actionOpenClient(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments) {
    if (!target) return kIOReturnBadArgument;
    target->fClientActive = true;
    IOLog("MyIntelVCSClient::actionOpenClient - Media session opened successfully\n");
    return kIOReturnSuccess;
}

IOReturn MyIntelVCSClient::actionCloseClient(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments) {
    if (!target) return kIOReturnBadArgument;
    target->fClientActive = false;
    IOLog("MyIntelVCSClient::actionCloseClient - Media session closed by Userspace\n");
    return kIOReturnSuccess;
}

IOReturn MyIntelVCSClient::actionSubmitDecode(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments) {
    if (!target || !target->fClientActive || !target->fProviderGPU) {
        return kIOReturnNotPermitted;
    }
    
    const struct myintel_vcs_decode_payload *payload = (const struct myintel_vcs_decode_payload *)arguments->structureInput;
    if (!payload) return kIOReturnBadArgument;
    
    IOLog("MyIntelVCSClient::actionSubmitDecode - Submitting video bitstream payload size: %u\n", payload->bitstream_size);
    
    // เรียกใช้คำสั่งประมวลผลส่งเข้า VDBOX
    bool success = submitVCSDecodeCmd(target->fProviderGPU, payload);
    return success ? kIOReturnSuccess : kIOReturnIOError;
}
