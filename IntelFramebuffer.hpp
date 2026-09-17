/*===========================================================================
 *  MyIntelFramebuffer.hpp
 *  Hackintosh Kext — Framebuffer Output Interface (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MY_INTEL_FRAMEBUFFER_HPP__
#define __MY_INTEL_FRAMEBUFFER_HPP__

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/IOLib.h>

class MyIntelGPU;

class MyIntelFramebuffer : public IOFramebuffer {
    OSDeclareDefaultStructors(MyIntelFramebuffer)
    
public:
    virtual bool init(OSDictionary *dictionary) override;
    virtual IOReturn start(IOService *provider) override;
    
    // ฟังค์ชันบังคับของระบบปฏิบัติการเพื่อจัดแจงรายละเอียดหน้าจอเสมือน
    virtual IOReturn getDisplayModes(IODisplayModeID *allModes) override;
    virtual IOReturn getInformationForDisplayMode(IODisplayModeID mode, IODisplayModeInformation *info) override;
    virtual UInt32 getConnectionCount(void) override;

private:
    MyIntelGPU* fPrimaryGPU;
};

#endif /* __MY_INTEL_FRAMEBUFFER_HPP__ */
