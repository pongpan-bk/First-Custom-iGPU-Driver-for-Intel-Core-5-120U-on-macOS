#include "MyIntelGPU.hpp"
#include "MyIntelRing.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

/**
 * ฟังก์ชันทำลายและคืนหน่วยความจำแหวนคำสั่งให้แก่แกนระบบปฏิบัติการ XNU 
 * ถูกเรียกใช้โดยตรงจากฟังก์ชัน stop() ของพี่ใน MyIntelGPU.cpp ป้องกันปัญหาหน่วยความจำรั่ว (Memory Leak)
 */
void ringDestroy_Secure(MyIntelGPU* gpu, MyIntelRing* ring)
{
    if (!ring) return;

    // 1. สั่งสั่งปิดการทำงานตัวควบคุมวงแหวนในฮาร์ดแวร์ก่อนล้างหน่วยความจำ (Disable HW Engine)
    if (gpu && gpu->isValidRegs()) {
        gpu->writeReg32(ring->mmioBase + RING_CTL_REG_OFFSET, 0);
        gpu->writeReg32(ring->mmioBase + RING_START_REG_OFFSET, 0);
        gpu->writeReg32(ring->mmioBase + RING_TAIL_REG_OFFSET, 0);
        OSSynchronizeIO(); // ล้างคำสั่งค้างท่อในบัส PCI
    }

    // 2. ดึง Descriptor และสั่งปลดขีดจำกัตหน่วยความจำ (Unwire + Release Descriptor)
    IOBufferMemoryDescriptor* bufferDesc = (IOBufferMemoryDescriptor*)ring->descriptorPriv;
    if (bufferDesc) {
        bufferDesc->complete(); // ปลดล็อกหน้าเพจกลับสู่สถานะปกติ
        bufferDesc->release();  // ทำลายคืนหน่วยความจำให้ระบบ
        ring->descriptorPriv = NULL;
    }

    // 3. คืนพื้นที่หน่วยความจำโครงสร้างออบเจกต์ Ring สู่ระบบ
    IOFree(ring, sizeof(MyIntelRing));
    IOLog("MyIntelGPU: [RingTeardown] คืนพื้นที่หน่วยความจำของโครงสร้าง Ring Buffer สำเร็จ\n");
}
