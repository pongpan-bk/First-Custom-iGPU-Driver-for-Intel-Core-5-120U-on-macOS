#include <IOKit/IOLib.h>
#include "MyIntelGEMBuffer.hpp"

// ผูกตัวคลาสเข้ากับโมดูลฐานขับเคลื่อนระบบความปลอดภัยของ IOKit Runtime
OSDefineMetaClassAndStructors(MyIntelGEMBuffer, OSObject)

bool MyIntelGEMBuffer::initWithSize(mach_vm_size_t size) {
    if (!OSObject::init()) {
        return false;
    }
    
    fSize = size;
    fGttOffset = 0; // เคลียร์พิกัดค่าเริ่มต้นเพื่อเตรียมทำการแมปเข้าตำแหน่งตารางจดบันทึก
    
    // ทำการสร้าง Allocation Descriptor เพื่อจำลองพื้นที่ RAM เครื่องมาทำหน้าที่เป็น VRAM จำลอง
    fMemoryDescriptor = IOMemoryDescriptor::withAddressRange(
        (mach_vm_address_t)IOMallocPageable(size),
        size,
        kIODirectionInOut,
        kernel_task
    );
    
    if (!fMemoryDescriptor) {
        IOLog("MyIntelGEMBuffer::initWithSize - เกิดข้อผิดพลาด! ไม่สามารถจองเนื้อที่ VRAM จำลองขนาด %qd bytes ได้\n", size);
        return false;
    }
    
    // สั่งจองและเตรียมตารางเพื่อเขียนโครงสร้างที่อยู่ทางกายภาพ (Physical Mapping)
    fMemoryDescriptor->prepare();
    
    IOLog("MyIntelGEMBuffer::initWithSize - สำเร็จ จัดสรรหน่วยความจำและบล็อกความจุเรียบร้อย\n");
    return true;
}

void MyIntelGEMBuffer::fixupPteEncoding(uint64_t* pteEntry) {
    if (!pteEntry) return;
    
    /* 
     * ─── แก้ไขบั๊กทางเทคนิค Gen12 PPGTT 4-Level ─────────────────────────────
     * บนฮาร์ดแวร์ Alder Lake / Raptor Lake ตัวเข้ารหัสโครงสร้างหน้าหน่วยความจำ (PTE Entry) 
     * ของไดรเวอร์ macOS ดั้งเดิม (Coffee Lake base) มักจะตีความบิตสถานะ 0xC3 คลาดเคลื่อน 
     * ส่งผลให้เกิดอาการหน้าจอค้างทันทีหลังจากผ่านเฟสโหลดไดรเวอร์ (Hard Lockup)
     * สคริปต์แก้ไขจุดนี้จะทำหน้าที่ขยับบิตปรับแต่งตรรกะระบบแคช (Cache Coherency Mask) 
     * ให้เข้ากับสเปกของลินุกซ์ i915 kernel driver
     * ───────────────────────────────────────────────────────────────────
     */
    uint64_t rawPte = *pteEntry;
    
    // ล้างรูปแบบโครงสร้างข้อมูลบิตแคชแบบเก่าที่ไม่สอดคล้องกันออกไป
    rawPte &= ~(1ULL << 3); // ปรับบิตควบคุม PWT (Page-level Write-Through)
    rawPte |= (1ULL << 7);  // เปิดบิตควบคุม PAT เพื่อบังคับให้ GPU เขียนอ่านแบบซิงโครนัสเสถียร
    
    *pteEntry = rawPte;
}

void MyIntelGEMBuffer::free() {
    IOLog("MyIntelGEMBuffer::free - กำลังคืนพื้นที่การจัดสรรตารางหน่วยความจำวิดีโอย่อย\n");
    if (fMemoryDescriptor) {
        fMemoryDescriptor->complete();
        fMemoryDescriptor->release();
        fMemoryDescriptor = nullptr;
    }
    OSObject::free();
}
