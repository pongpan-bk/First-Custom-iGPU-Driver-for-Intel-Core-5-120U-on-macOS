/*===========================================================================
 *  myintel_vcs_user.h
 *  Hackintosh Kext — Userspace Interface Contract (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MYINTEL_VCS_USER_H__
#define __MYINTEL_VCS_USER_H__

#if defined(__cplusplus)
#include <IOKit/IOTypes.h>
#else
#include <stdint.h>
#endif

/* 
 * ───────────────────────────────────────────────────────────────────────────
 * โครงสร้างข้อมูลสำหรับรับส่งคำสั่งถอดรหัสวิดีโอ (VDBOX H.264 Pipeline)
 * ───────────────────────────────────────────────────────────────────────────
 */
struct myintel_vcs_decode_payload {
    uint64_t bitstream_gtt_offset;  /* ตำแหน่งของไฟล์วิดีโอดิบบนหน่วยความจำการ์ดจอ (GGTT) */
    uint32_t bitstream_size;        /* ขนาดของข้อมูลไฟล์วิดีโอดิบ (Bytes) */
    
    uint64_t surface_gtt_offset;    /* ตำแหน่งพิกัดพื้นที่เฟรมภาพที่เตรียมนำมาวาดบนหน้าจอ */
    uint32_t surface_width;         /* ความกว้างของวิดีโอ (Pixels) */
    uint32_t surface_height;        /* ความสูงของวิดีโอ (Pixels) */
    uint32_t surface_pitch;         /* ขนาดความกว้างแนวขวางของหน้าพื้นผิวแรม (Stride) */
    
    uint32_t codec_type;            /* ชนิดตัวแปลงสัญญาณ (0 = H.264, 1 = HEVC, 2 = VP9) */
    uint32_t flags;                 /* บิตคำสั่งเสริมพิเศษสำหรับการประมวลผล */
    uint64_t fence_seqno_out;       /* หมายเลขคิวแจ้งเตือนความคืบหน้าส่งกลับฝั่ง Userspace */
};

/* 
 * ───────────────────────────────────────────────────────────────────────────
 * นิยามตัวเลข Selector สำหรับ IOUserClient (ตารางเรียกใช้คำสั่งจาก Userspace)
 * ───────────────────────────────────────────────────────────────────────────
 */
enum {
    kMyIntelUserClientOpen         = 0,  /* เปิดใช้งานช่องทางเชื่อมต่อไดรเวอร์การ์ดจอ */
    kMyIntelUserClientClose        = 1,  /* ปิดช่องทางเชื่อมต่อและเคลียร์สิทธิ์การทำงาน */
    kMyIntelUserClientAllocBuffer  = 2,  /* คำสั่งจองพื้นที่หน่วยความจำความเร็วสูง (GEM) */
    kMyIntelUserClientSubmitDecode = 3,  /* สั่งยิงคำสั่งแกะรหัสไฟล์วิดีโอเข้าสู่คิวฮาร์ดแวร์ VDBOX */
    kMyIntelUserClientGetProgress  = 4,  /* ดึงข้อมูลสถานะและตัวเลขลำดับคิวทำงานล่าสุด */
    kMyIntelUserClientMaxSelectors = 5
};

#endif /* __MYINTEL_VCS_USER_H__ */
