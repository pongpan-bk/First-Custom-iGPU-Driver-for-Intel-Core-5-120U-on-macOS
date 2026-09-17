/*===========================================================================
 *  MyIntelVCSClient.h
 *  Hackintosh Kext — Video Codec User Client Interface (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MY_INTEL_VCS_CLIENT_H__
#define __MY_INTEL_VCS_CLIENT_H__

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include "myintel_vcs_user.h"

class MyIntelGPU;

class MyIntelVCSClient : public IOUserClient {
    OSDeclareDefaultStructors(MyIntelVCSClient)

public:
    virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;

    /* ตารางดักรับคำสั่งเรียกทำงานจากแอปภายนอก (Selector Dispatch) */
    virtual IOReturn externalMethod(UInt32 selector, IOExternalMethodArguments *arguments,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) override;

protected:
    /* ตรรกะฟังก์ชันภายในคลาสสำหรับประมวลผลวิดีโอ */
    static IOReturn actionOpenClient(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn actionCloseClient(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn actionSubmitDecode(MyIntelVCSClient *target, void *reference, IOExternalMethodArguments *arguments);

private:
    task_t       fOwningTask;
    MyIntelGPU*  fProviderGPU;
    bool         fClientActive;
};

#endif /* __MY_INTEL_VCS_CLIENT_H__ */
