#include <IOKit/IOLib.h>
#include "MyIntelAccelerator.hpp"
#include "MyIntelGPU.hpp"

// จับคู่โครงสร้างแมปคุณสมบัติเมตาคลาสเร่งความเร็วระบบกราฟิกชั้นลึก (IOAccelerator)
OSDefineMetaClassAndStructors(MyIntelAccelerator, IOService)

bool MyIntelAccelerator::init(OSDictionary *dictionary) {
    if (!IOService::init(dictionary)) {
        return false;
    }
    IOLog("MyIntelAccelerator::init - เริ่มต้นเตรียมพร้อมโมดูลเร่งสปีดกราฟิกฮาร์ดแวร์\n");
    return true;
}

bool MyIntelAccelerator::start(IOService *provider) {
    if (!IOService::start(provider)) {
        return false;
    }

    IOLog("MyIntelAccelerator::start - เปิดสถานะระบบเร่งความเร็ว 2D/3D สำหรับชิป Intel Iris Xe\n");

    // ดึงและผูกพอยเตอร์อุปกรณ์การ์ดจอตัวหลักมาเก็บไว้ใช้เชื่อมวงจรอ่านค่าหน่วยความจำวิดีโอ
    fParentGPU = OSDynamicCast(MyIntelGPU, provider);
    if (!fParentGPU) {
        IOLog("MyIntelAccelerator::start - ผิดพลาด: ไม่สามารถอ้างอิงพอยเตอร์ MyIntelGPU หลักได้\n");
        return false;
    }

    // ลงทะเบียนรับบริการส่งต่อคำสั่งเพื่อเตรียมพร้อมให้บริการกับ IOUserClient ของแอปพลิเคชันภายนอก
    registerService();

    return true;
}

void MyIntelAccelerator::stop(IOService *provider) {
    IOLog("MyIntelAccelerator::stop - ปิดช่องทางบริการเร่งความเร็วฮาร์ดแวร์\n");
    IOService::stop(provider);
}

void MyIntelAccelerator::free(void) {
    IOLog("MyIntelAccelerator::free - คืนพื้นที่หน่วยความจำโมดูลคลาสเร่งสปีด\n");
    IOService::free();
}

// ฟังก์ชันสำคัญสำหรับจับคู่สร้างช่องทางเชื่อมต่อสัญญานเรียก (UserClient) จากฝั่งแอปพลิเคชันระบบ
IOReturn MyIntelAccelerator::newUserClient(task_t replyingTask, void *securityID,
                                            UInt32 type, OSDictionary *properties,
                                            IOUserClient **handler) {
    IOLog("MyIntelAccelerator::newUserClient - มีการเชื่อมต่อเข้ามาจาก Userspace (Type: %u)\n", (unsigned int)type);
    
    if (!handler) return kIOReturnBadArgument;
    
    // ตรงนี้จะเป็นจุดที่ระบุเพื่อส่งต่อพอยเตอร์สำหรับประมวลผลคำสั่งชุดวิดีโอ (เช่น MyIntelVCSClient)
    *handler = NULL; 
    return kIOReturnUnsupported;
}
