//
//  kern_myigfx.cpp
//  MyIntelGPU
//
//  Lilu plugin — Intel Gen 10-12 iGPU connector
//  Makes macOS recognize unsupported iGPU via device-id faking
//  Based on WhateverGreen architecture (vit9696)
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_devinfo.hpp>

#include "kern_myigfx.hpp"

// ประกาศไอดี Kext ปลายทางตามมิติประเภทขนาด size_t สำหรับ Lilu
static size_t kextTglId   { 0 };
static size_t kextIclLpId { 1 };
static size_t kextIclHpId { 2 };

MYIGFX *MYIGFX::callbackMYIGFX = nullptr;

// จัดพารามิเตอร์ลงช่อง Aggregate Initialization รูปแบบ C++11 ดั้งเดิม 
// เรียงลำดับ: ชื่อ Kext, เส้นทางอาเรย์ (nullptr), จำนวนเส้นทาง (0), ปีกกา patches ว่าง {}, ปีกกาพิกัด {}, สถานะโหลด, และไอดีเก็บค่าปลายทาง
KernelPatcher::KextInfo MYIGFX::kextList[] {
	{ "com.apple.driver.AppleIntelTGLGraphicsFramebuffer",    nullptr, 0, {}, {}, KernelPatcher::KextInfo::Unloaded, kextTglId },
	{ "com.apple.driver.AppleIntelICLLPGraphicsFramebuffer",  nullptr, 0, {}, {}, KernelPatcher::KextInfo::Unloaded, kextIclLpId },
	{ "com.apple.driver.AppleIntelICLHPGraphicsFramebuffer",  nullptr, 0, {}, {}, KernelPatcher::KextInfo::Unloaded, kextIclHpId }
};

void MYIGFX::init() {
	callbackMYIGFX = this;

	DBGLOG("myigfx", "plugin init");

	// Try to process the builtin GPU immediately — the IGPU may already be
	// visible in the IORegistry at plugin load time (WEG-style early pass).
	auto devInfo = DeviceInfo::create();
	if (devInfo) {
		if (devInfo->videoBuiltin) {
			DBGLOG("myigfx", "builtin video present at init, processing");
			processBuiltinProperties(devInfo->videoBuiltin, devInfo);
			didProcessBuiltin = true;
		} else {
			DBGLOG("myigfx", "no builtin video at init, waiting for kext load");
		}
		DeviceInfo::deleter(devInfo);
	}

	// ลงทะเบียนรับสิทธิ์ผ่านตัวแปรสถาปัตยกรรมหลัก "lilu" แทนโมเดลสไตล์เก่า
	lilu.onKextLoad(kextList, arrsize(kextList),
		[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
			callbackMYIGFX->processKext(patcher, index, address, size);
		});
}

void MYIGFX::deinit() {
}

void MYIGFX::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (didProcessBuiltin) return;

	// WEG-style: obtain the device tree info and process the builtin GPU.
	auto devInfo = DeviceInfo::create();
	if (devInfo) {
		if (devInfo->videoBuiltin) {
			DBGLOG("myigfx", "processKext index %zu, found builtin video, processing", index);
			processBuiltinProperties(devInfo->videoBuiltin, devInfo);
			didProcessBuiltin = true;
		} else {
			DBGLOG("myigfx", "processKext index %zu, no builtin video yet, retry on next kext load", index);
		}
		DeviceInfo::deleter(devInfo);
	}
}

void MYIGFX::processBuiltinProperties(IORegistryEntry *device, DeviceInfo *info) {
	auto name = device->getName();

	// There could be only one IGPU, and it must be named IGPU for AppleGVA to function properly.
	if (!name || strcmp(name, "IGPU") != 0)
		WIOKit::renameDevice(device, "IGPU");

	WIOKit::awaitPublishing(device);

	auto obj = OSDynamicCast(IOService, device);
	if (obj) {
		uint32_t realDevice = WIOKit::readPCIConfigValue(obj, WIOKit::kIOPCIConfigDeviceID);
		uint32_t acpiDevice = 0, fakeDevice = 0;

		// The fake device-id is provided via the IORegistry "device-id" property.
		if (!WIOKit::getOSDataValue(obj, "device-id", acpiDevice))
			DBGLOG("myigfx", "missing IGPU device-id (no spoof target configured)");

		// User may request to fake device-id even if it is supported (WEG L424).
		if (realDevice != acpiDevice) {
			DBGLOG("myigfx", "user requested device-id fake 0x%04X", acpiDevice);
			fakeDevice = acpiDevice;
		}

		// Update vtable I/O functions so the fake device-id is returned to matching.
		if (fakeDevice && obj->getProperty("no-gfx-spoof") == nullptr) {
			if (fakeDevice != realDevice) {
				hasIgpuSpoof = true;
				KernelPatcher::routeVirtual(obj, WIOKit::PCIConfigOffset::ConfigRead16, wrapConfigRead16, &orgConfigRead16);
				KernelPatcher::routeVirtual(obj, WIOKit::PCIConfigOffset::ConfigRead32, wrapConfigRead32, &orgConfigRead32);
				DBGLOG("myigfx", "hooked configRead methods, spoofing 0x%04X -> 0x%04X", realDevice, fakeDevice);
			}
		}
	} else {
		SYSLOG("myigfx", "invalid IGPU device type");
	}

	// Ensure built-in.
	if (!device->getProperty("built-in")) {
		DBGLOG("myigfx", "fixing built-in");
		uint8_t builtBytes[] { 0x00 };
		device->setProperty("built-in", builtBytes, sizeof(builtBytes));
	}
}

uint16_t MYIGFX::wrapConfigRead16(IORegistryEntry *service, uint32_t space, uint8_t offset) {
	auto result = callbackMYIGFX->orgConfigRead16(service, space, offset);
	if (offset == WIOKit::kIOPCIConfigDeviceID && service != nullptr) {
		auto name = service->getName();
		if (!name) return result;
		// แก้ไขปัญหาเปรียบเทียบข้อมูลด้วยการใช้พิกัดตำแหน่งอาเรย์ตัวอักษรทีละช่อง [index] แทนการเช็คผ่านพอยเตอร์ตรงๆ
		bool doSpoof = (callbackMYIGFX->hasIgpuSpoof && name[0] == 'I' && name[1] == 'G' && name[2] == 'P' && name[3] == 'U');
		if (doSpoof) {
			uint32_t device;
			if (WIOKit::getOSDataValue(service, "device-id", device) && device != result) {
				DBGLOG("myigfx", "configRead16 %s reported 0x%04x instead of 0x%04x", name, static_cast<uint16_t>(device), result);
				return static_cast<uint16_t>(device);
			}
		}
	}

	return result;
}

uint32_t MYIGFX::wrapConfigRead32(IORegistryEntry *service, uint32_t space, uint8_t offset) {
	auto result = callbackMYIGFX->orgConfigRead32(service, space, offset);
	if ((offset == WIOKit::kIOPCIConfigDeviceID || offset == WIOKit::kIOPCIConfigVendorID) && service != nullptr) {
		auto name = service->getName();
		if (!name) return result;
		// แก้ไขปัญหาเปรียบเทียบข้อมูลด้วยการใช้พิกัดตำแหน่งอาเรย์ตัวอักษรทีละช่อง [index] เช่นเดียวกันกับด้านบน
		bool doSpoof = (callbackMYIGFX->hasIgpuSpoof && name[0] == 'I' && name[1] == 'G' && name[2] == 'P' && name[3] == 'U');
		if (doSpoof) {
			uint32_t device;
			if (WIOKit::getOSDataValue(service, "device-id", device) && device != (result & 0xFFFF)) {
				device = (result & 0xFFFF) | (device << 16);
				DBGLOG("myigfx", "configRead32 %s reported 0x%08x instead of 0x%08x", name, device, result);
				return device;
			}
		}
	}

	return result;
}
