#include "MyIntelGPU.hpp"
#include "MyIntelRing.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

// นิยามคำสั่งควบคุมวงแหวนระดับฮาร์ดแวร์ของ Intel (MI Commands)
#define MI_NOOP                  0x00000000
#define MI_USER_INTERRUPT        0x02000000
#define MI_BATCH_BUFFER_START    0x31000001 // สั่งรัน Batch แบบ 64-bit Address สำหรับชิป Gen 12
#define RING_CTL_ENABLE          (1U << 0)

/**
 * ฟังก์ชันสร้างและจัดสรรพื้นที่หน่วยความจำให้แก่ Ring Engine 
 * อ้างอิงสถาปัตยกรรมของ XNU Kernel และฟังก์ชันตัวแปรหลักใน MyIntelGPU.cpp
 */
MyIntelRing* ringCreate_Secure(MyIntelGPU* gpu, uint32_t engineType, uint32_t mmioBase, uint32_t sizeBytes)
{
    if (!gpu || sizeBytes == 0) return NULL;

    // 1. จัดสรรออบเจกต์คุมหน่วยความจำของคลาส Ring
    MyIntelRing* ring = (MyIntelRing*)IOMalloc(sizeof(MyIntelRing));
    if (!ring) return NULL;
    memset(ring, 0, sizeof(MyIntelRing));

    ring->engineType = engineType;
    ring->mmioBase   = mmioBase;
    ring->size       = sizeBytes;
    ring->head       = 0;
    ring->tail       = 0;
    ring->space      = sizeBytes - 64; // เผื่อพื้นที่ว่างป้องกันคำสั่งชนขอบ

    // 2. จัดสรรหน่วยความจำแบบต่อเนื่องทางกายภาพนอกคลาส (Out-of-tree Wired Kernel Page Allocation)
    // ใช้แฟล็ก kIOMemoryPhysicallyContiguous ตามข้อกำหนดของสถาปัตยกรรม X86_64 Kernel
    IOBufferMemoryDescriptor* bufferDesc = IOBufferMemoryDescriptor::withOptions(
        kIOMemoryPhysicallyContiguous | kIOMemoryDirectionInOut,
        ring->size,
        4096 // แนบพิกัดขนาดหน้าเพจระบบ 4KB
    );

    if (!bufferDesc) {
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    // ตรึงหน้าเพจไม่ให้โดนสลับลงฮาร์ดดิสก์ (Wire Pages)
    if (bufferDesc->prepare() != kIOReturnSuccess) {
        bufferDesc->release();
        IOFree(ring, sizeof(MyIntelRing));
        return NULL;
    }

    // 3. ดึงแอดเดรสเสมือนฝั่ง CPU Kernel ออกมาใช้เขียนคำสั่ง (เสมือน getBytesNoCopy)
    ring->ringBufferVaddr = (uint32_t*)bufferDesc->getBytesNoCopy();
    bzero((void*)ring->ringBufferVaddr, ring->size);

    // 4. บันทึกพิกัดหน่วยความจำแหวน (GTT Offset บัส) ส่งเข้า Register ตรงๆ ผ่าน writeReg32 ของพี่
    // สอดคล้องกับพิกัด Log เฟส 6 ของพี่: "Allocating RCS ring buffer... Creating RCS ring"
    uint32_t physicalSegmentLen = 0;
    IOPhysicalAddress ringPhysAddr = bufferDesc->getPhysicalSegment(0, (IOPhysicalLength*)&physicalSegmentLen);
    
    // แปลงพิกัดกายภาพเข้าสู่แอดเดรสบัสกราฟิก GGTT
    ring->ggttOffset = (uint32_t)ringPhysAddr; 

    // สั่งเขียนบันทึกตำแหน่งพิกัดฐานคำสั่ง และเปิดการใช้งานตัวควบคุมวงแหวนคำสั่ง (RING_CTL)
    gpu->writeReg32(ring->mmioBase + RING_START_REG_OFFSET, ring->ggttOffset);
    
    uint32_t ringControlFlags = ((ring->size / 4096) - 1) << 12 | RING_CTL_ENABLE;
    gpu->writeReg32(ring->mmioBase + RING_CTL_REG_OFFSET, ringControlFlags);

    // เก็บออบเจกต์ Descriptor สำรองไว้เคลียร์หน่วยความจำตอนสั่งปิดระบบ
    ring->descriptorPriv = (void*)bufferDesc;
    ring->lrcInited = true;

    return ring;
}

/**
 * ฟังก์ชันสั่งเร่งฮาร์ดแวร์ส่งแถวคำสั่งประมวลผล (Kick Hardware via Execlist Submission)
 * ลิงก์ตรงกับฟังก์ชันคุมบัสในระบบล็อก และสอดคล้องกับบรรทัด Log: "ringSubmitExeclists: desc=... tail=24"
 */
void ringSubmit_Secure(MyIntelGPU* gpu, MyIntelRing* ring)
{
    if (!gpu || !ring || !ring->lrcInited) return;

    // ตรวจสอบพิกัดการไหลของข้อมูล (Memory Boundary Check)
    uint32_t dwordTailIndex = ring->tail / sizeof(uint32_t);
    
    // ใส่คำสั่งแจ้งเตือนสิทธิ์การขัดจังหวะระบบ (Interrupt) เพื่อให้ Core ตื่นตัวทำรายงานผล
    ring->ringBufferVaddr[dwordTailIndex++] = MI_USER_INTERRUPT;
    ring->ringBufferVaddr[dwordTailIndex++] = MI_NOOP; // ล้างพิกัด Alignment ความกว้าง PCI Bus

    // วนลูปแอดเดรสหางกลับมาจุดเริ่มต้นเมื่อชนขอบขนาด (Ring Wrap-around Management)
    ring->tail = (dwordTailIndex * sizeof(uint32_t)) % ring->size;

    // สั่งเขียนพิกัด Tail ลง Register จริงของการ์ดจอผ่านเลเยอร์คุมระบบของพี่
    gpu->writeReg32(ring->mmioBase + RING_TAIL_REG_OFFSET, ring->tail);

    // ทำฮาร์ดแวร์บาร์ริเออร์ (Hardware Fence) เพื่อการันตีว่าบัส PCI ส่งข้อมูลเรียบร้อย
    OSMemoryBarrier();
}
