
/*===========================================================================
 *  kern_start.cpp
 *  Hackintosh Kext — Standalone Driver Entry Point (Gen 12 Intel Iris Xe)
 *=========================================================================*/

#include <IOKit/IOLib.h>

// ประกาศฟังก์ชันเริ่มต้นและจุดสิ้นสุดสำหรับการโหลดโมดูลระดับ Kernel ดิบ (Standalone Kmod)
extern "C" {
    kern_return_t MyIntelGPU_start(kmod_info_t *ki, void *d);
    kern_return_t MyIntelGPU_stop(kmod_info_t *ki, void *d);
}

kern_return_t MyIntelGPU_start(kmod_info_t *ki, void *d) {
    IOLog("MyIntelGPU::kmod_start - Standalone Driver loaded into Kernel space.\n");
    return KERN_SUCCESS;
}

kern_return_t MyIntelGPU_stop(kmod_info_t *ki, void *d) {
    IOLog("MyIntelGPU::kmod_stop - Standalone Driver unloaded from Kernel space.\n");
    return KERN_SUCCESS;
}
