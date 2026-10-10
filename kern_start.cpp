//
//  kern_start.cpp
//  MyIntelGPU - Lilu plugin for RPL-U device-id spoof + IGPU fixes
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include <mach/kmod.h>

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

/*
 * Manual kmod_info_t definition.
 *
 * Xcode-linked Lilu plugins (e.g. WhateverGreen) get the _kmod_info symbol
 * synthesized by ld when linking against libkmod.a (WEG: OTHER_LDFLAGS="-static"
 * + libkmod.a in Frameworks). Our Makefile build does not link libkmod, so we
 * must define the structure ourselves — the same approach that the old
 * composite 2.0.19 kext used (git f753b07), which was accepted and loaded.
 *
 * KMOD_DECL() cannot be used here because the bundle identifier contains dots
 * (com.pongpan-bk.MyIntelGPU) which break the macro's token-paste expansion.
 *
 * Entry points: ADDPR(kern_start)/ADDPR(kern_stop) are provided by Lilu's
 * Library/plugin_start.cpp, which is added to the build in the Makefile.
 */
extern "C" {
    kern_return_t ADDPR(kern_start)(kmod_info_t *, void *);
    kern_return_t ADDPR(kern_stop)(kmod_info_t *, void *);

    kmod_info_t kmod_info = { 0, 1, -1U,                                  /* next, info_version, id */
        "com.pongpan-bk.MyIntelGPU", STAMPED_VERSION,                     /* name, version (ตรง CFBundleVersion ที่ stamp) */
        -1, 0, 0, 0, 0,                                                   /* ref_count, ref_list, addr, size, hdr_size */
        ADDPR(kern_start), ADDPR(kern_stop) };                            /* start, stop */
}
