/*===========================================================================
 *  MyIntelGEMBuffer.cpp
 *  Hackintosh Kext — GEM Memory Buffer Manager (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#include <IOKit/IOLib.h>
#include "MyIntelGEMBuffer.hpp"

OSDefineMetaClassAndStructors(MyIntelGEMBuffer, OSObject)

bool MyIntelGEMBuffer::initWithSize(mach_vm_size_t size) {
    if (!OSObject::init()) {
        return false;
    }
    
    fSize = size;
    fGttOffset = 0;
    
    // จัดสรรหน่วยความจำเพียวกายภาพ (Page-aligned) สำหรับทำ DMA Mapping กับการ์ดจอ
    fMemoryDescriptor = IOMemoryDescriptor::withAddressRange(
        (mach_vm_address_t)IOMallocAligned(size, 4096),
        size,
        kIODirectionInOut,
        kernel_task
    );
    
    if (!fMemoryDescriptor) {
        IOLog("MyIntelGEMBuffer::initWithSize - Failed to allocate IOMemoryDescriptor\n");
        return false;
    }
    
    // ทำการล็อกหน้าหน่วยความจำให้อยู่กับที่ ห้ามระบายลงฮาร์ดดิสก์ (Wire Memory)
    if (fMemoryDescriptor->prepare() != kIOReturnSuccess) {
        IOLog("MyIntelGEMBuffer::initWithSize - Failed to prepare memory descriptor\n");
        fMemoryDescriptor->release();
        fMemoryDescriptor = NULL;
        return false;
    }
    
    IOLog("MyIntelGEMBuffer::initWithSize - Allocated %llu bytes VRAM Buffer\n", size);
    return true;
}

void MyIntelGEMBuffer::free() {
    if (fMemoryDescriptor) {
        fMemoryDescriptor->complete();
        void* rawAddr = (void*)fMemoryDescriptor->getSourceSegment(0, NULL);
        if (rawAddr) {
            IOFreeAligned(rawAddr, fSize);
        }
        fMemoryDescriptor->release();
        fMemoryDescriptor = NULL;
    }
    IOLog("MyIntelGEMBuffer::free - Released GEM VRAM Buffer\n");
    OSObject::free();
}

void MyIntelGEMBuffer::fixupPteEncoding(uint64_t* pteEntry) {
    if (!pteEntry) return;
    
    // แก้ไขบั๊กโครงสร้างสถาปัตยกรรมระดับแกนของ Gen 12 (i915 PPGTT PTE 0xC3 bug)
    // ทำการล้างบิตที่มีปัญหาก่อนส่งคำสั่งเข้าไปยังการ์ดจอ เพื่อป้องกันการเกิดอาการค้างหน้าจอดำตอนโหลดกราฟิก
    uint64_t pte = *pteEntry;
    
    // เคลียร์บิตจำลองการเข้าถึงระดับตารางหน้า (Clear bits 3-4 ที่ทำให้คอนโทรลเลอร์ Gen 12 เอ๋อ)
    pte &= ~(1ULL << 3);
    pte &= ~(1ULL << 4);
    
    // บังคับให้บิตเปิดใช้งานคุณสมบัติความปลอดภัยและการมองเห็นของการ์ดจอคงอยู่ (Valid + Coherent)
    pte |= (1ULL << 0); // Present Bit
    pte |= (1ULL << 1); // Read/Write Bit
    
    *pteEntry = pte;
}
