/*===========================================================================
 *  MyIntelVCSCommand.h
 *  Hackintosh Kext — Video Codec Command Pipeline Setup (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#ifndef __MY_INTEL_VCS_COMMAND_H__
#define __MY_INTEL_VCS_COMMAND_H__

#include <IOKit/IOLib.h>
#include "myintel_vcs_user.h"

class MyIntelGPU;

/**
 * @brief ฟังก์ชันส่งต่อและประมวลผลสัญญานชุดคำสั่งถอดรหัสเข้าสู่โมดูล VDBOX Hardware Engine
 * @param gpu พอยเตอร์ชี้คลาสควบคุมฮาร์ดแวร์การ์ดจอหลัก
 * @param payload โครงสร้างข้อมูลพิกัดความจำของภาพยนตร์ถอดรหัสจากฝั่งผู้ใช้
 */
bool submitVCSDecodeCmd(MyIntelGPU* gpu, const struct myintel_vcs_decode_payload* payload);

#endif /* __MY_INTEL_VCS_COMMAND_H__ */
