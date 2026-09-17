//
//  kern_start.cpp
//  MyIntelGPU - Lilu plugin for RPL-U device-id spoof + IGPU fixes
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_myigfx.hpp"

static MYIGFX myigfx;

static const char *bootargOff[] {
    "-myigfxoff"
};

static const char *bootargDebug[] {
    "-myigfxdbg"
};

static const char *bootargBeta[] {
    "-myigfxbeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::MountainLion,
    KernelVersion::Tahoe,
    []() {
        myigfx.init();
    }
};
