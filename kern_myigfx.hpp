//
//  kern_myigfx.hpp
//  MyIntelGPU
//
//  Lilu plugin — Intel Gen 10-12 iGPU connector
//  Makes macOS recognize unsupported iGPU via device-id faking
//  Based on WhateverGreen architecture (vit9696)
//

#ifndef kern_myigfx_hpp
#define kern_myigfx_hpp

#include <Headers/kern_patcher.hpp>
#include <Headers/kern_iokit.hpp>
#include <Headers/kern_devinfo.hpp>

class MYIGFX {
public:
	/**
	 *  Plugin entry (called from PluginConfiguration lambda)
	 */
	void init();
	void deinit();

private:
	/**
	 *  Private self instance for callbacks
	 */
	static MYIGFX *callbackMYIGFX;

	/**
	 *  Device identification spoofing state
	 */
	bool hasIgpuSpoof {false};
	bool didProcessBuiltin {false};

	/**
	 *  Hooked vtable methods for PCI config reads
	 */
	WIOKit::t_PCIConfigRead16 orgConfigRead16 {nullptr};
	WIOKit::t_PCIConfigRead32 orgConfigRead32 {nullptr};

	/**
	 *  Apply builtin GPU properties + enable device-id faking
	 *
	 *  @param device  IGPU device
	 *  @param info    device information
	 */
	void processBuiltinProperties(IORegistryEntry *device, DeviceInfo *info);

	/**
	 *  Process kext load events
	 *
	 *  @param patcher KernelPatcher instance
	 *  @param index   kinfo handle
	 *  @param address kinfo load address
	 *  @param size    kinfo memory size
	 */
	void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

	/**
	 *  IGPU PCI config device-id faking wrappers (vtable routed)
	 */
	static uint16_t wrapConfigRead16(IORegistryEntry *service, uint32_t space, uint8_t offset);
	static uint32_t wrapConfigRead32(IORegistryEntry *service, uint32_t space, uint8_t offset);

	/**
	 *  Framebuffer kexts we hook (Gen 12 TGL / Gen 11 ICL)
	 */
	static KernelPatcher::KextInfo kextList[];
};

#endif /* kern_myigfx_hpp */