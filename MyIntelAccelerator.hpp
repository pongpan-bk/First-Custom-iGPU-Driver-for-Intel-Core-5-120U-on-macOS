#ifndef __MY_INTEL_ACCELERATOR_HPP__
#define __MY_INTEL_ACCELERATOR_HPP__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

// ทำการบอกคอมไพเลอร์ล่วงหน้า (Forward Declaration) ว่ามีคลาส MyIntelGPU อยู่จริง 
// ป้องกันปัญหาการวนลูปเรียกไฟล์หัวข้อ (Circular Include) จนคอมไพเลอร์ฟ้อง Error
class MyIntelGPU;

class MyIntelAccelerator : public IOService {
    OSDeclareDefaultStructors(MyIntelAccelerator)
    
public:
    /* IOKit Service Lifecycle Methods */
    virtual bool init(OSDictionary *dictionary) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free(void) override;

    /* ช่องทางลงทะเบียนรับส่งคำสั่งกับ Userspace App (ตาราง Selector 0-24) */
    virtual IOReturn newUserClient(task_t replyingTask, void *securityID,
                                    UInt32 type, OSDictionary *properties,
                                    IOUserClient **handler) override;

private:
    // พอยเตอร์สำหรับเชื่อมโยงการทำงานกลับไปยังไดรเวอร์ควบคุมฮาร์ดแวร์การ์ดจอหลัก
    MyIntelGPU* fParentGPU;
};

#endif /* __MY_INTEL_ACCELERATOR_HPP__ */
