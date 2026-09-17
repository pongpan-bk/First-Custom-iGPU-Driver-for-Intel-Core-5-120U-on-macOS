/*===========================================================================
 *  MyIntelRing.hpp
 *  Hackintosh Kext — Ring Buffer Stream Controller (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MY_INTEL_RING_HPP__
#define __MY_INTEL_RING_HPP__

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

// Forward Declaration เพื่ออ้างอิงกลับไปยังคลาสควบคุมฮาร์ดแวร์หลักโดยไม่เกิด Circular Include
class MyIntelGPU;

/* 
 * โครงสร้างข้อมูลควบคุมตำแหน่งและขอบเขตวงรอบคิวส่งงาน (Ring Container)
 * จัดการพิกัดตัวชี้คิว (Head/Tail Tracking) และควบคุมขนาดบัฟเฟอร์ระดับฮาร์ดแวร์ 32-บิต
 */
struct MyIntelRing {
    IOMemoryDescriptor* ringMemory;       // ตัวจัดสรรและจองพื้นที่เซกเมนต์หน่วยความจำบัฟเฟอร์วงรอบ
    mach_vm_address_t   virtualAddress;   // ที่อยู่หน่วยความจำเสมือนฝั่ง Kernel (CPU ใช้เขียนคำสั่งเข้าคิว)
    uint32_t            gttAddress;       // ที่อยู่กายภาพบนตารางหน้าจอแสดงผล GPU (GGTT Mapping)
    
    uint32_t            ringSize;         // ขนาดความจุของหน้ากระดาษคิวคำสั่ง (มาตรฐานฮาร์ดแวร์กำหนดไว้ที่ 16KB)
    uint32_t            head;             // ตำแหน่งที่ตัวการ์ดจออ่านคำสั่งไปถึงล่าสุด (Hardware Head Tracking)
    uint32_t            tail;             // ตำแหน่งตัวชี้ที่ไดรเวอร์ป้อนคำสั่งต่อท้ายล่าสุด (Tail Advance)
    uint32_t            space;            // บิตคำนวณและตรวจสอบขนาดพื้นที่ว่างในวงรอบคิวคำสั่งปัจจุบัน
};

/* 
 * ชุดโครงสร้างฟังก์ชันระบบตรวจสอบย้อนกลับ (Callback Interfaces)
 * ใช้สำหรับส่งสัญญาณขัดจังหวะ (Interrupts) แจ้งเตือนเมื่อคิวทำงานครบรอบ หรือระบบรันคำสั่งพิเศษเสร็จสิ้น
 */
struct MyIntelRingCallbacks {
    void (*onRingWrapAround)(MyIntelRing* ring);
    void (*onBreadcrumbComplete)(uint32_t seqno);
};

/* 
 * ───────────────────────────────────────────────────────────────────────────
 * สัญญาฟังก์ชันการทำงานส่วนนอก (C-Style Linkage Interface Functions)
 * ป้องกันปัญหาระบบคอมไพเลอร์ Clang แปลงชื่อฟังก์ชันสลับกันตอนลิงก์ไฟล์ kext
 * ───────────────────────────────────────────────────────────────────────────
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief สั่งเตรียมความพร้อมและจองพื้นที่หน่วยความจำฮาร์ดแวร์คิวประมวลผลตามขนาดที่กำหนด
 * @param gpu พอยเตอร์ชี้ไปยังไดรเวอร์ควบคุมการ์ดจอหลัก
 * @param ring พอยเตอร์คิววงรอบที่ต้องการจัดสรรพื้นที่
 * @param size ขนาดความจุของคิวคำสั่ง (Bytes)
 * @param mmioBase ค่าแอดเดรสฐานของเครื่องยนต์กราฟิกที่ต้องการผูกระบบควบคุม
 */
bool initHardwareRing(MyIntelGPU* gpu, MyIntelRing* ring, uint32_t size, uint32_t mmioBase);

/**
 * @brief จัดการเขียนชุดคำสั่งคอมมานด์บอร์ดลงหน้าบัฟเฟอร์คิวคำสั่งวงรอบหลัก
 * @param ring พอยเตอร์คิววงรอบที่ต้องการป้อนคำสั่งเข้า
 * @param commands พอยเตอร์อาร์เรย์ชุดคำสั่งเลขฐานสิบหก 32-บิต
 * @param count จำนวนคำสั่งทั้งหมดในแพ็กเก็ตที่ต้องการเขียน
 */
void submitBatchToRing(MyIntelRing* ring, uint32_t* commands, uint32_t count);

/**
 * @brief คำสั่งเลื่อนและอัปเดตรีจิสเตอร์ตำแหน่งหางคิว (Tail Register Entry) เพื่อส่งสัญญาณสะกิด GPU ให้ดึงคำสั่งไปเริ่มประมวลผล
 * @param gpu พอยเตอร์ชี้ไปยังไดรเวอร์ควบคุมการ์ดจอหลัก
 * @param ring พอยเตอร์คิววงรอบที่คำสั่งเขียนเสร็จสิ้นแล้ว
 * @param mmioBase แอดเดรสรีจิสเตอร์ของเครื่องยนต์กราฟิกจริง เช่น RCS0_BASE_REAL หรือ VCS0_BASE_REAL
 */
void advanceRingTail(MyIntelGPU* gpu, MyIntelRing* ring, uint32_t mmioBase);

#ifdef __cplusplus
}
#endif

#endif /* __MY_INTEL_RING_HPP__ */
