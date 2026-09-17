/*===========================================================================
 *  MyIntelVCS.h
 *  Hackintosh Kext — Video Codec Command Definition (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MY_INTEL_VCS_H__
#define __MY_INTEL_VCS_H__

#include <IOKit/IOLib.h>

/* นิยามชุดคำสั่งสำหรับควบคุมชิปเข้ารหัสและถอดรหัสวิดีโอ (MFX/VDBOX Gen 12) */
#define MI_NOOP                     0x00000000
#define MI_BATCH_BUFFER_START_GEN12 0x18800001
#define MI_FLUSH_DW_GEN12           0x26000003
#define MI_USER_INTERRUPT           0x02000000

/* โครงสร้างแพ็กเก็ตส่งเข้า Userspace สำหรับรับข้อมูลพิกัดภาพยนตร์ถอดรหัส */
struct MyIntelVCSDecPacket {
    uint32_t bitstreamGttAddr;  // ที่อยู่ของไฟล์วิดีโอดิบใน GGTT
    uint32_t bitstreamSize;     // ขนาดความจุของข้อมูลดิบ
    uint32_t surfaceGttAddr;    // ที่อยู่ของภาพปลายทางที่จะนำมาแสดงผลหน้าจอ
    uint32_t commandType;       // ตัวแยกคำสั่ง H.264 / HEVC / VP9
};

#endif /* __MY_INTEL_VCS_H__ */
