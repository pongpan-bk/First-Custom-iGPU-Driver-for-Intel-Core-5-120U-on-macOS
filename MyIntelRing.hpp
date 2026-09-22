/*===========================================================================
 *  MyIntelRing.cpp
 *  Hackintosh Kext — Ring Buffer Engine Executive (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#include "MyIntelRing.hpp"
#include "MyIntelGPU.hpp"

extern "C" {

bool initHardwareRing(MyIntelGPU* gpu, MyIntelRing* ring, uint32_t size, uint32_t mmioBase) {
    if (!gpu || !ring || size == 0) return false;
    
    ring->ringSize = size;
    ring->head = 0;
    ring->tail = 0;
    ring->space = size - 8; // เผื่อพื้นที่ว่างไว้เล็กน้อยกันบั๊กคิวล้นฮาร์ดแวร์
    
    // จัดสรรและจองพื้นที่หน่วยความจำเพียวกายภาพขนาดคงที่สำหรับคิวคำสั่ง
    ring->ringMemory = IOMemoryDescriptor::withAddressRange(
        (mach_vm_address_t)IOMallocAligned(size, 4096),
        size,
        kIODirectionInOut,
        kernel_task
    );
    
    if (!ring->ringMemory) return false;
    
    ring->ringMemory->prepare();
    ring->virtualAddress = (mach_vm_address_t)ring->ringMemory->getSourceSegment(0, NULL);
    
    // เริ่มต้นเขียนล้างค่าในบัฟเฟอร์คิวคำสั่งให้สะอาดเป็นค่าว่าง (MI_NOOP)
    uint32_t* rawBuffer = (uint32_t*)ring->virtualAddress;
    for (uint32_t i = 0; i < (size / 4); i++) {
        rawBuffer[i] = 0; // MI_NOOP
    }
    
    // ตั้งค่าพิกเตอร์ลงบนรีจิสเตอร์เริ่มต้นของควบคุมประมวลผลการ์ดจอตัวจริง
    gpu->writeRegister32(mmioBase + 0x34, 0); // RING_HEAD
    gpu->writeRegister32(mmioBase + 0x30, 0); // RING_TAIL
    gpu->writeRegister32(mmioBase + 0x38, ((size - 4096) & 0xFFFFF000) | 1); // RING_LEN (เปิดใช้ Ring)
    
    IOLog("MyIntelRing::initHardwareRing - Ring configured at MMIO 0x%X\n", mmioBase);
    return true;
}

void submitBatchToRing(MyIntelRing* ring, uint32_t* commands, uint32_t count) {
    if (!ring || !commands || count == 0) return;
    
    uint32_t* rawRing = (uint32_t*)ring->virtualAddress;
    uint32_t dwordTail = ring->tail / 4;
    uint32_t maxDwords = ring->ringSize / 4;
    
    for (uint32_t i = 0; i < count; i++) {
        rawRing[dwordTail] = commands[i];
        dwordTail = (dwordTail + 1) % maxDwords; // หากเขียนจนสุดหน้ากระดาษให้วนกลับมาเริ่มต้นใหม่ (Wrap-around)
    }
    
    ring->tail = dwordTail * 4;
}

void advanceRingTail(MyIntelGPU* gpu, MyIntelRing* ring, uint32_t mmioBase) {
    if (!gpu || !ring) return;
    
    // ทำการใส่คำสั่งกั้นความจำ (Memory Barrier) เพื่อบังคับให้ CPU ยัดคำสั่งลงแรมให้เสร็จก่อนสะกิดการ์ดจอ
    __asm__ __volatile__("sfence" ::: "memory");
    
    // ส่งข้อมูลพิกัดหางคิวล่าสุดเขียนทับรีจิสเตอร์ควบคุม เพื่อปลุกให้ชิปประมวลผลการ์ดจอเริ่มดึงคำสั่งไปทำงาน
    gpu->writeRegister32(mmioBase + 0x30, ring->tail); // สั่งเลื่อนฮาร์ดแวร์ RING_TAIL
}

}