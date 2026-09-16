/*===========================================================================
 *  MyIntelFramebuffer.cpp
 *  Hackintosh Kext — IOFramebuffer Binding Implementation (Phase 4)
 *  macOS 15 Sequoia SDK
 *///=========================================================================

#include "MyIntelFramebuffer.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/pwr_mgt/IOPMpowerState.h>

// Constants missing from macOS 15.2 SDK headers
#ifndef kIOFBBrightness
#define kIOFBBrightness         'brgt'
#endif
#ifndef kIOFBPowerStateOn
#define kIOFBPowerStateOn       1
#endif

#define FBLog(fmt, ...) \
    IOLog("MyIntelFB: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

// Experiment #1 (2026-08-26): file-scope KDB0924 EDID for DDC path
// Source: Windows registry dump via Extract-EDID.ps1 2026-08-24; checksum 0x01
static const UInt8 gKDB0924_EDID[128] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x2C, 0x82, 0x24, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x27, 0x22, 0x01, 0x04, 0xA5, 0x22, 0x13, 0x78, 0x02, 0x4B, 0x3D, 0x8F, 0x5C, 0x59, 0x91, 0x25,
    0x15, 0x4F, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2A, 0x36, 0x80, 0xA0, 0x70, 0x38, 0x1F, 0x40, 0x30, 0x20,
    0x35, 0x00, 0x58, 0xC2, 0x10, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x4B,
    0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x36, 0x00, 0x00, 0x00, 0xFC,
    0x00, 0x4B, 0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x32, 0x00, 0x03
};

/*
 * E2 instrumentation: log attribute keys — throttled to first 200 calls
 * per key to avoid kernel log buffer flooding on frequent WindowServer polls.
 */
static void logAttrKey(const char *op, IOIndex connectIndex, IOSelect key)
{
    /* Throttle: skip noisy keys after boot settles (fAttrGetCount > 200) */
    static uint32_t sLogCount = 0;
    if (sLogCount > 200) return;
    sLogCount++;

    char c0 = (char)((key >> 24) & 0xFF), c1 = (char)((key >> 16) & 0xFF),
         c2 = (char)((key >> 8) & 0xFF), c3 = (char)(key & 0xFF);
    bool printable = (c0 >= 0x20 && c0 <= 0x7E) && (c1 >= 0x20 && c1 <= 0x7E) &&
                     (c2 >= 0x20 && c2 <= 0x7E) && (c3 >= 0x20 && c3 <= 0x7E);
    if (printable)
        FBLog("%s(conn=%lu) key='%c%c%c%c' (0x%08X)", op, (long)connectIndex,
              c0, c1, c2, c3, (unsigned int)key);
    else
        FBLog("%s(conn=%lu) key=0x%08X (non-printable)", op, (long)connectIndex,
              (unsigned int)key);
}

#define super IOFramebuffer
OSDefineMetaClassAndStructors(MyIntelFramebuffer, IOFramebuffer)

#pragma mark - init / free

bool MyIntelFramebuffer::init(OSDictionary *dict)
{
    if (!super::init(dict)) return false;

    fGPU            = NULL;
    fCurrentModeID  = MYFB_DISPLAY_MODE_ID;
    fCurrentDepth   = 0;
    fDisplayOn      = true;
    fFrameCounter   = 0;
    fLastVblankTime = 0;
    fVSyncEnabled   = false;
    fBrightness     = MYFB_BRIGHTNESS_DEFAULT;
    fVRAMDescriptor = NULL;
    fAttrGetCount   = 0;
    fAttrSetCount   = 0;
    fPowerOnCount   = 0;
    fPowerOffCount  = 0;
    fApertureCount  = 0;
    fLocationTimer  = nullptr;

    FBLog("init() OK - 1920x1080 @ 60Hz");
    return true;
}

void MyIntelFramebuffer::free()
{
    FBLog("free()");
    if (fLocationTimer) {
        fLocationTimer->release();
        fLocationTimer = nullptr;
    }
    if (fVRAMDescriptor) {
        fVRAMDescriptor->release();
        fVRAMDescriptor = NULL;
    }
    fGPU = NULL;
    super::free();
}

#pragma mark - start / stop

bool MyIntelFramebuffer::start(IOService *provider)
{
    FBLog("=== start() ===");
    mygpuProgress("mfb:start-enter");
    if (!super::start(provider)) {
        FBLog("super::start failed");
        return false;
    }

    fGPU = OSDynamicCast(MyIntelGPU, provider);
    if (!fGPU) {
        FBLog("provider is not MyIntelGPU");
        return false;
    }
    fGPU->retain();

    setupDefaultMode();
    mygpuProgress("mfb:mode-set");
    createVRAMDescriptor();
    mygpuProgress("mfb:vram-desc");
    publishIORegistryProperties();
    mygpuProgress("mfb:publish");

    registerService(kIOServiceAsynchronous);
    mygpuProgress("mfb:register-service");

    /* Connection chain (display0/IODisplayConnect/AppleDisplay) is created
     * by IOFramebuffer::open() → displaysOnline → makeDisplayConnects when
     * we self-open below — spawning it here raced that path into ghost
     * duplicate nodes during CoreDisplay reconfiguration. */

    IOReturn oerr = open();
    FBLog("self-open -> 0x%x", oerr);
    if (oerr == kIOReturnSuccess) {
        OSString *loc = OSString::withCString("0");
        if (loc) { setProperty("ioDisplayLocation", loc); loc->release(); FBLog("ioDisplayLocation=0 on FB"); }
        IORegistryIterator *it = IORegistryIterator::iterateOver(this, gIOServicePlane);
        if (it) {
            IORegistryEntry *ch;
            while ((ch = it->getNextObject())) {
                if (OSDynamicCast(IODisplayConnect, ch)) {
                    OSString *l2 = OSString::withCString("0");
                    if (l2) { ch->setProperty("ioDisplayLocation", l2); l2->release(); FBLog("ioDisplayLocation=0 on display0"); }
                    break;
                }
            }
            it->release();
        }
    }

    IOWorkLoop *workLoop = getWorkLoop();
    bool createdWL = false;
    if (!workLoop) {
        workLoop = IOWorkLoop::workLoop();
        createdWL = true;
    }
    if (workLoop) {
        fLocationTimer = IOTimerEventSource::timerEventSource(this, locationTimerFired);
        if (fLocationTimer) {
            workLoop->addEventSource(fLocationTimer);
            fLocationTimer->setTimeoutMS(2500);
            FBLog("location timer armed 2.5s%s", createdWL ? " (new WL)" : "");
        }
        if (createdWL) workLoop->release();
    }

    FBLog("start() OK");
    mygpuProgress("mfb:start-done");
    return true;
}

void MyIntelFramebuffer::locationTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    MyIntelFramebuffer *self = OSDynamicCast(MyIntelFramebuffer, owner);
    if (!self) return;
    bool found = false;
    IORegistryIterator *it = IORegistryIterator::iterateOver(self, gIOServicePlane);
    if (it) {
        IORegistryEntry *ch;
        while ((ch = it->getNextObject())) {
            if (OSDynamicCast(IODisplayConnect, ch)) {
                {
                    OSNumber *flagsNum = OSNumber::withNumber((unsigned long long)0, 32);
                    if (flagsNum) {
                        ch->setProperty("IODisplayConnectFlags", flagsNum);
                        flagsNum->release();
                    }
                    IOLog("MyIntelFB: IODisplayConnectFlags=0 set on display0 (timer)\n");
                }
                IORegistryIterator *it2 = IORegistryIterator::iterateOver(ch, gIOServicePlane);
                if (it2) {
                    IORegistryEntry *gc;
                    while ((gc = it2->getNextObject())) {
                        if (OSDynamicCast(IODisplay, gc) || strcmp(gc->getName(), "AppleDisplay")==0) {
                            OSString *loc = OSString::withCString("0");
                            if (loc) { gc->setProperty("ioDisplayLocation", loc); loc->release(); IOLog("MyIntelFB: ioDisplayLocation=0 on AppleDisplay (timer)\n"); }
                            found = true;
                            break;
                        }
                    }
                    it2->release();
                }
                break;
            }
        }
        it->release();
    }
    if (!found) {
        IOLog("MyIntelFB: location timer AppleDisplay not found, retry 2s\n");
        sender->setTimeoutMS(2000);
    }
}

void MyIntelFramebuffer::stop(IOService *provider)
{
    FBLog("stop()");
    if (fLocationTimer) {
        fLocationTimer->cancelTimeout();
        if (IOWorkLoop *wl = fLocationTimer->getWorkLoop()) wl->removeEventSource(fLocationTimer);
        else if (IOWorkLoop *workLoop = getWorkLoop()) workLoop->removeEventSource(fLocationTimer);
        fLocationTimer->release();
        fLocationTimer = nullptr;
    }
    PMstop();  /* Unregister from power management plane (paired with PMinit in start) */
    if (fVRAMDescriptor) {
        fVRAMDescriptor->release();
        fVRAMDescriptor = NULL;
    }
    if (fGPU) {
        fGPU->release();
        fGPU = NULL;
    }
    super::stop(provider);
}

#pragma mark - Pure Virtual Methods

IODeviceMemory *MyIntelFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    fApertureCount++;
    FBLog("getApertureRange(aperture=%lu)", (long)aperture);
    if (!fVRAMDescriptor) return NULL;

    /*
     * IOPixelAperture 0 = main frame buffer (required)
     * IOPixelAperture 1 = cursor aperture (optional, Gen8+ uses HW cursor)
     * For now, return the same VRAM range for all apertures.
     *
     * On Gen12+ the cursor is handled by the display engine directly
     * via CURBASE/CURPOS registers — no separate aperture needed.
     */
    fVRAMDescriptor->retain();
    return fVRAMDescriptor;
}

const char *MyIntelFramebuffer::getPixelFormats(void)
{
    /*
     * Return multiple pixel formats macOS can choose from.
     * "O010" = XRGB8888 (32-bit, no alpha), preferred
     * "Q010" = ARGB8888 (32-bit with alpha)
     * Formats are listed in priority order — macOS picks the first
     * one it supports.
     *
     * Reference: IOFramebuffer.h — IO32BitDirectPixels
     */
    FBLog("getPixelFormats -> \"O010Q010\"");
    return "O010Q010";
}

IOReturn MyIntelFramebuffer::getInformationForDisplayMode(
        IODisplayModeID displayMode,
        IODisplayModeInformation *info)
{
    if (!info) return kIOReturnBadArgument;
    if (displayMode != MYFB_DISPLAY_MODE_ID)
        return kIOReturnUnsupportedMode;

    bzero(info, sizeof(IODisplayModeInformation));
    info->nominalWidth    = MYFB_H_ACTIVE;
    info->nominalHeight   = MYFB_V_ACTIVE;
    info->refreshRate     = MYFB_REFRESH_RATE << 16; // 16.16 fixed point
    info->maxDepthIndex   = 0;
    info->flags           = kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag;

    FBLog("getInformationForDisplayMode: %ux%u @ %uHz",
          (uint32_t)info->nominalWidth, (uint32_t)info->nominalHeight,
          (uint32_t)info->refreshRate);
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::getPixelInformation(
        IODisplayModeID displayMode,
        IOIndex depth,
        IOPixelAperture aperture,
        IOPixelInformation *pixelInfo)
{
    (void)depth;
    (void)aperture;

    FBLog("getPixelInformation(mode=%u depth=%lu aperture=%lu)",
          (uint32_t)displayMode, (long)depth, (long)aperture);
    if (!pixelInfo) return kIOReturnBadArgument;
    if (displayMode != MYFB_DISPLAY_MODE_ID)
        return kIOReturnUnsupportedMode;

    bzero(pixelInfo, sizeof(IOPixelInformation));

    pixelInfo->pixelType        = kIORGBDirectPixels;
    pixelInfo->componentCount   = 3;
    pixelInfo->bitsPerPixel     = MYFB_BITS_PER_PIXEL;
    pixelInfo->bytesPerRow      = MYFB_BYTES_PER_ROW;
    pixelInfo->bytesPerPlane    = 0;
    pixelInfo->bitsPerComponent = MYFB_BITS_PER_COMP;
    pixelInfo->componentMasks[0] = 0x00FF0000;
    pixelInfo->componentMasks[1] = 0x0000FF00;
    pixelInfo->componentMasks[2] = 0x000000FF;
    pixelInfo->componentMasks[3] = 0xFF000000;
    pixelInfo->flags            = 0;
    strlcpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof(pixelInfo->pixelFormat));
    pixelInfo->activeWidth  = MYFB_H_ACTIVE;
    pixelInfo->activeHeight = MYFB_V_ACTIVE;

    return kIOReturnSuccess;
}

UInt64 MyIntelFramebuffer::getPixelFormatsForDisplayMode(
        IODisplayModeID displayMode,
        IOIndex depth)
{
    (void)displayMode;
    (void)depth;
    FBLog("getPixelFormatsForDisplayMode -> 0 (obsolete)");
    return 0; // Obsolete per IOFramebuffer.h — must return 0
}

#pragma mark - Non-Pure Virtual Overrides

UInt32 MyIntelFramebuffer::getDisplayModeCount(void)
{
    FBLog("getDisplayModeCount -> %u", (uint32_t)MYFB_MODE_COUNT);
    return MYFB_MODE_COUNT;
}

IOReturn MyIntelFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    FBLog("getDisplayModes called");
    if (!allDisplayModes) return kIOReturnBadArgument;
    allDisplayModes[0] = MYFB_DISPLAY_MODE_ID;
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::getCurrentDisplayMode(
        IODisplayModeID *displayMode,
        IOIndex *depth)
{
    if (!displayMode || !depth) return kIOReturnBadArgument;
    *displayMode = fCurrentModeID;
    *depth       = fCurrentDepth;
    FBLog("getCurrentDisplayMode -> mode=%u depth=%lu",
          (uint32_t)fCurrentModeID, (long)fCurrentDepth);
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::setDisplayMode(
        IODisplayModeID displayMode,
        IOIndex depth)
{
    FBLog("setDisplayMode: mode=%u depth=%lu", (uint32_t)displayMode, (long)depth);
    if (displayMode != MYFB_DISPLAY_MODE_ID)
        return kIOReturnUnsupportedMode;
    if (fCurrentModeID == displayMode && fCurrentDepth == depth)
        return kIOReturnSuccess;

    fCurrentModeID = displayMode;
    fCurrentDepth  = depth;

    if (fGPU) {
        fGPU->initDisplay();
    }
    return kIOReturnSuccess;
}

IODeviceMemory *MyIntelFramebuffer::getVRAMRange(void)
{
    FBLog("getVRAMRange called");
    if (!fVRAMDescriptor) return NULL;
    fVRAMDescriptor->retain();
    return fVRAMDescriptor;
}

IOReturn MyIntelFramebuffer::getAttributeForConnection(
        IOIndex connectIndex,
        IOSelect key,
        uintptr_t *value)
{
    (void)connectIndex;
    fAttrGetCount++;
    logAttrKey("getAttribute", connectIndex, key);
    switch (key) {
        case kIOFBBrightness:
            if (fGPU) {
                fBrightness = fGPU->getPanelBrightness();
            }
            if (value) *value = fBrightness;
            FBLog("kIOFBBrightness -> %u", (uint32_t)fBrightness);
            return kIOReturnSuccess;

        case kConnectionFlags:
            if (value) *value = kIOConnectionBuiltIn;
            FBLog("kConnectionFlags -> 0x800 (built-in eDP, was 0x0)");
            return kIOReturnSuccess;

        case kConnectionEnable:
            /* E3b: report enabled so updateOnline() sees an online display. */
            if (value) *value = 1;
            FBLog("kConnectionEnable ('enab') -> 1");
            return kIOReturnSuccess;

        case kConnectionCheckEnable:
            /* E3b: robustness (V1 — not a blocker); return enabled. */
            if (value) *value = 1;
            FBLog("kConnectionCheckEnable ('cena') -> 1");
            return kIOReturnSuccess;

        case kConnectionPower:
            /* E3b: report ON so the wrangler does not power-cycle. */
            if (value) *value = kIOFBPowerStateOn;
            FBLog("kConnectionPower ('powr') -> %u (on)", (unsigned int)kIOFBPowerStateOn);
            return kIOReturnSuccess;

        case kConnectionEnableAudio:
        case kConnectionAudioStreaming:
            /* 2.0.229 (H2, ปฏิบัติการแหกตำรา): this GPU has no display
             * audio engine bound (no AudioDxe/AppleHDA path); claiming
             * success here makes WindowServer wait on audio that never
             * comes. Report unsupported so the audio subsystem skips the
             * connection instead of stalling. */
            return kIOReturnUnsupported;

        case kConnectionSupportsHLDDCSense:
            FBLog("kConnectionSupportsHLDDCSense -> supported (DDC path active)");
            return kIOReturnSuccess;
        case kConnectionSupportsLLDDCSense:
            return kIOReturnUnsupported;

        case kConnectionSyncEnable:
            /* Sync is handled by the display engine HW — report enabled. */
            if (value) *value = 1;
            return kIOReturnSuccess;

        case kConnectionDisplayParameterCount:
            /* No tunable display params — return 0 stops CoreDisplay
             * from iterating param[0..N] every display wake. */
            if (value) *value = 0;
            return kIOReturnSuccess;

        default: {
            IOReturn r = super::getAttributeForConnection(connectIndex, key, value);
            FBLog("default attr -> 0x%X", (unsigned int)r);
            return r;
        }
    }
}

bool MyIntelFramebuffer::hasDDCConnect(IOIndex connectIndex)
{
    bool present = (connectIndex == 0);
    FBLog("hasDDCConnect idx=%u -> %d", (unsigned)connectIndex, present);
    return present;
}

IOReturn MyIntelFramebuffer::getDDCBlock(IOIndex connectIndex, UInt32 blockNumber,
                IOSelect blockType, IOOptionBits options,
                UInt8 * data, IOByteCount * length)
{
    (void)blockType;
    (void)options;
    if (connectIndex != 0) return kIOReturnBadArgument;
    if (!data || !length) return kIOReturnBadArgument;
    if (blockNumber != 0 && blockNumber != 1) {
        FBLog("getDDCBlock block %u -> no extension", (unsigned)blockNumber);
        return kIOReturnUnsupported;
    }
    if (*length < sizeof(gKDB0924_EDID)) {
        FBLog("getDDCBlock buffer too small %lu", (unsigned long)*length);
        return kIOReturnNoMemory;
    }
    bcopy(gKDB0924_EDID, data, sizeof(gKDB0924_EDID));
    *length = sizeof(gKDB0924_EDID);
    FBLog("getDDCBlock block %u -> 128B KDB0924 OK", (unsigned)blockNumber);
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::setAttributeForConnection(
        IOIndex connectIndex,
        IOSelect key,
        uintptr_t value)
{
    (void)value;
    fAttrSetCount++;
    logAttrKey("setAttribute", connectIndex, key);
    switch (key) {
        case kIOFBBrightness:
            fBrightness = (value > MYFB_BRIGHTNESS_MAX) ? MYFB_BRIGHTNESS_MAX : (UInt32)value;
            FBLog("brightness = %u", (uint32_t)fBrightness);
            if (fGPU) {
                fGPU->setPanelBrightness(fBrightness);
            }
            return kIOReturnSuccess;
        case kConnectionEnableAudio:
        case kConnectionAudioStreaming:
            /* 2.0.229 (H2): mirror getAttributeForConnection — no display
             * audio engine on this GPU; reject audio enable requests. */
            return kIOReturnUnsupported;
        default: {
            IOReturn r = super::setAttributeForConnection(connectIndex, key, value);
            FBLog("default attr -> 0x%X", (unsigned int)r);
            return r;
        }
    }
}

#pragma mark - Public Methods

/*
 * VblankEvent — macOS 15 removed this symbol from IOGraphicsFamily.
 * Must be defined locally in the kext, otherwise OpenCore prelinked
 * injection fails with "Invalid Parameter" (unresolved U symbol).
 */
extern "C" void VblankEvent(IOFramebuffer *that, UInt32 count, UInt64 time)
{
    (void)that;
    (void)count;
    (void)time;
    return;
}

void MyIntelFramebuffer::handleVblank(void)
{
    fFrameCounter++;
    clock_get_uptime(&fLastVblankTime);
    VblankEvent(this, (UInt32)fFrameCounter, fLastVblankTime);
}

void MyIntelFramebuffer::setBrightness(UInt32 brightness)
{
    fBrightness = (brightness > MYFB_BRIGHTNESS_MAX) ? MYFB_BRIGHTNESS_MAX : brightness;
    if (fGPU) {
        fGPU->setPanelBrightness(fBrightness);
    }
}

IOReturn MyIntelFramebuffer::performPowerStateChange(
        IOIndex connectIndex,
        UInt32 powerState)
{
    (void)connectIndex;
    if (powerState == kIOFBPowerStateOn) {
        fPowerOnCount++;
        fDisplayOn = true;
        FBLog("power ON");
        if (fGPU) {
            fGPU->gpuResume();
        }
    } else {
        fPowerOffCount++;
        FBLog("power OFF");
        if (fGPU) {
            fGPU->gpuSuspend();
        }
        fDisplayOn = false;
    }
    return kIOReturnSuccess;
}

void MyIntelFramebuffer::dumpDiagnostics(void) const
{
    IOLog("MyIntelGPU: MyIntelFB diag: frame=%llu attrGet=%u attrSet=%u "
          "powerOn=%u powerOff=%u aperture=%u displayOn=%d mode=%u\n",
          fFrameCounter, fAttrGetCount, fAttrSetCount, fPowerOnCount,
          fPowerOffCount, fApertureCount, fDisplayOn ? 1 : 0,
          (unsigned int)fCurrentModeID);
}

#pragma mark - Internal Helpers

void MyIntelFramebuffer::setupDefaultMode(void)
{
    fCurrentModeID = MYFB_DISPLAY_MODE_ID;
    fCurrentDepth  = 0;
}

bool MyIntelFramebuffer::createVRAMDescriptor(void)
{
    if (!fGPU) return false;

    /*
     * Phase 4.2 — Map stolen DRAM as the VRAM aperture.
     *
     * The physical base and size of stolen memory are detected in
     * MyIntelGPU::ggttInitHardware() by scanning GGTT PTE[0]:
     *   stolenBase = PTE[0] & ~0xFFF   (e.g. 0x4C800000)
     *   stolenSize = contiguous PTE run × 4096  (e.g. 64 MB)
     *
     * CRITICAL: size reported to WindowServer via getApertureRange()
     * MUST equal the physical mapping size — not larger, not smaller.
     * Reporting a size bigger than what we map causes WindowServer to
     * allocate display surfaces beyond the end of the mapping →
     * CDDisplay::present_update SIGBUS / KERN_PROTECTION_FAILURE.
     *
     * The BIOS/GOP plane (PLANE_SURF_1A=0x00000000) already scans from
     * GGTT page 0 = stolenBase, so mapping stolen here aligns our VRAM
     * descriptor with the active scanout buffer — no flicker, no gap.
     */
    uint64_t stolenBase = fGPU->getStolenBase();
    uint32_t stolenSize = fGPU->getStolenSize();

    if (stolenBase == 0 || stolenSize < (1920 * 1080 * 4)) {
        /*
         * Phase 4.3 fix — MyIntelFramebuffer is created in Phase 5d but
         * ggttInitHardware (PTE-run scan) runs in Phase 6, so the stolen
         * fields are still zero here → without this call we would fall
         * back to the 0x40-byte stub and WindowServer would SIGBUS.
         * detectStolenMemory() reads DSMBASE/GMS directly (no GGTT).
         */
        fGPU->detectStolenMemory();
        stolenBase = fGPU->getStolenBase();
        stolenSize = fGPU->getStolenSize();
    }

    if (stolenBase != 0 && stolenSize >= (1920 * 1080 * 4)) {
        /* Real stolen DRAM mapping — safe for WindowServer surfaces */
        fVRAMDescriptor = IODeviceMemory::withRange(
                              (IOPhysicalAddress)stolenBase,
                              (IOPhysicalLength)stolenSize);
        if (!fVRAMDescriptor) {
            FBLog("ERROR: could not map stolen VRAM 0x%llX size %u MB — trying stub",
                  stolenBase, stolenSize >> 20);
        } else {
            FBLog("VRAM: stolen map 0x%llX size %u MB (%u bytes) — Phase 4.2 REAL",
                  stolenBase, stolenSize >> 20, stolenSize);
        }
    }

    if (!fVRAMDescriptor) {
        /*
         * Fallback stub: 0x40 bytes at a safe physical address.
         * Keeps the framebuffer display-safe (no WindowServer surface
         * can be allocated against 64 bytes) — same as pre-Phase-4.2.
         * This path is taken only when stolen base detection fails.
         */
        fVRAMDescriptor = IODeviceMemory::withRange(
                              (IOPhysicalAddress)0x4000, 0x40);
        FBLog("VRAM: stub 0x40 bytes (stolen detection failed) — display-safe fallback");
    }

    if (!fVRAMDescriptor) {
        FBLog("ERROR: could not allocate framebuffer VRAM descriptor");
        return false;
    }
    FBLog("VRAM descriptor: phys=0x%llX length=0x%llX bytes",
          fVRAMDescriptor->getPhysicalAddress(),
          fVRAMDescriptor->getLength());
    return true;
}


void MyIntelFramebuffer::publishIORegistryProperties(void)
{
    uint64_t vramSize = 0;
    if (fVRAMDescriptor && fVRAMDescriptor->getLength() > 0) {
        vramSize = fVRAMDescriptor->getLength();
    }

    uint64_t vramSizeAuto = (vramSize > 0) ? vramSize : 0x40000000;
    uint32_t vramArg = 0;
    if (PE_parse_boot_argn("myintelvram", &vramArg, sizeof(vramArg)) && vramArg != 0) {
        uint64_t poolSize = fGPU->getVramPoolSize();
        if (poolSize > 0) {
            vramSizeAuto = poolSize;
            IOLog("MyIntelGPU: VRAM Pool report ENABLED — %llu MB (GTT pool, myintelvram=1)\n",
                  vramSizeAuto >> 20);
        }
    }
    uint64_t vramSizeAutoMB = vramSizeAuto / 1024 / 1024;

    OSData *mbData = OSData::withBytes(&vramSizeAutoMB, sizeof(vramSizeAutoMB));
    OSData *bytesData = OSData::withBytes(&vramSizeAuto, sizeof(vramSizeAuto));
    if (mbData && bytesData) {
        setProperty("VRAM,totalMB", mbData);
        setProperty("VRAM,memSize", bytesData);
        mbData->release();
        bytesData->release();
        IOLog("MyIntelGPU: VRAM Injection set to AUTO. Detected Size: %llu MB\n", vramSizeAutoMB);
    }

    static const UInt8 kdbKDB0924_EDID[128] = {
        0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x2C, 0x82, 0x24, 0x09, 0x00, 0x00, 0x00, 0x00,
        0x27, 0x22, 0x01, 0x04, 0xA5, 0x22, 0x13, 0x78, 0x02, 0x4B, 0x3D, 0x8F, 0x5C, 0x59, 0x91, 0x25,
        0x15, 0x4F, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2A, 0x36, 0x80, 0xA0, 0x70, 0x38, 0x1F, 0x40, 0x30, 0x20,
        0x35, 0x00, 0x58, 0xC2, 0x10, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x4B,
        0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x36, 0x00, 0x00, 0x00, 0xFC,
        0x00, 0x4B, 0x44, 0x31, 0x35, 0x36, 0x4E, 0x32, 0x39, 0x33, 0x30, 0x41, 0x30, 0x32, 0x00, 0x03
    };
    OSData *edidData = OSData::withBytes(kdbKDB0924_EDID, sizeof(kdbKDB0924_EDID));
    if (edidData) {
        setProperty("IODisplayEDID", edidData);
        edidData->release();
        FBLog("EDID KDB0924 injected");
    }

    setProperty("built-in", kOSBooleanTrue);

    OSString *modeStr = OSString::withCString("1920x1080 @ 60Hz");
    if (modeStr) { setProperty("display-mode", modeStr); modeStr->release(); }

    OSString *fmtStr = OSString::withCString("XRGB8888 (O010)");
    if (fmtStr) { setProperty("pixel-format", fmtStr); fmtStr->release(); }

    OSString *dispType = OSString::withCString("LCD");
    if (dispType) { setProperty("display-type", dispType); dispType->release(); }

    setProperty("IOFBIntegrated", kOSBooleanTrue);

    uint32_t fbMemSize = 1920 * 1080 * 4;
    OSNumber *fbMemNum = OSNumber::withNumber(fbMemSize, 32);
    if (fbMemNum) { setProperty("IOFBMemorySize", fbMemNum); fbMemNum->release(); }

    OSDictionary *fbConfig = OSDictionary::withCapacity(4);
    if (fbConfig) {
        OSNumber *idNum = OSNumber::withNumber((unsigned long long)1, 32);
        OSNumber *aidNum = OSNumber::withNumber((unsigned long long)0, 32);
        OSNumber *dmNum = OSNumber::withNumber((unsigned long long)1, 32);
        OSNumber *dfNum = OSNumber::withNumber((unsigned long long)3, 32);
        if (idNum)  { fbConfig->setObject("ID", idNum);   idNum->release(); }
        if (aidNum) { fbConfig->setObject("AID", aidNum);  aidNum->release(); }
        if (dmNum)  { fbConfig->setObject("DM", dmNum);    dmNum->release(); }
        if (dfNum)  { fbConfig->setObject("DF", dfNum);    dfNum->release(); }
        setProperty("IOFBConfig", fbConfig);
        fbConfig->release();
    }

    OSDictionary *modeDict = OSDictionary::withCapacity(6);
    if (modeDict) {
        OSNumber *mid = OSNumber::withNumber((unsigned long long)1, 32);
        OSNumber *mdm = OSNumber::withNumber((unsigned long long)1, 32);
        OSNumber *mtm = OSNumber::withNumber((unsigned long long)0, 32);
        OSNumber *maid = OSNumber::withNumber((unsigned long long)0, 32);
        OSNumber *mdf = OSNumber::withNumber((unsigned long long)3, 32);
        OSNumber *mpi = OSNumber::withNumber((unsigned long long)0, 32);
        if (mid)  { modeDict->setObject("ID", mid);   mid->release(); }
        if (mdm)  { modeDict->setObject("DM", mdm);   mdm->release(); }
        if (mtm)  { modeDict->setObject("TM", mtm);   mtm->release(); }
        if (maid) { modeDict->setObject("AID", maid); maid->release(); }
        if (mdf)  { modeDict->setObject("DF", mdf);   mdf->release(); }
        if (mpi)  { modeDict->setObject("PI", mpi);   mpi->release(); }

        OSDictionary *fbModes = OSDictionary::withCapacity(1);
        if (fbModes) {
            OSString *key = OSString::withCString("1");
            if (key) {
                fbModes->setObject(key, modeDict);
                key->release();
            }
            setProperty("IOFBModes", fbModes);
            fbModes->release();
        }
        modeDict->release();
    }

    FBLog("IORegistry properties published (setProperty bypass)");
}

IOItemCount MyIntelFramebuffer::getConnectionCount(void)
{
    FBLog("getConnectionCount() called -> returning 1 connection");
    return 1;
}

IOReturn MyIntelFramebuffer::getStartupDisplayMode(
        IODisplayModeID *displayMode,
        IOIndex *depth)
{
    if (!displayMode || !depth) return kIOReturnBadArgument;
    *displayMode = MYFB_DISPLAY_MODE_ID;
    *depth = fCurrentDepth;
    FBLog("getStartupDisplayMode -> mode %u depth %lu", (unsigned int)*displayMode, (long)*depth);
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::getTimingInfoForDisplayMode(
        IODisplayModeID displayMode,
        IOTimingInformation *info)
{
    if (!info) return kIOReturnBadArgument;
    if (displayMode != MYFB_DISPLAY_MODE_ID) return kIOReturnUnsupportedMode;
    bzero(info, sizeof(*info));
    info->appleTimingID = kIOTimingIDInvalid;
    info->flags = kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag;
    FBLog("getTimingInfoForDisplayMode mode %u -> timing valid", (unsigned int)displayMode);
    return kIOReturnSuccess;
}

IOReturn MyIntelFramebuffer::connectFlags(
        IOIndex connectIndex,
        IODisplayModeID displayMode,
        IOOptionBits *flags)
{
    FBLog("connectFlags() called for index: %d mode: %u",
          (int)connectIndex, (unsigned int)displayMode);

    if (connectIndex != 0 || flags == NULL) {
        return kIOReturnBadArgument;
    }

    *flags = kIOConnectionBuiltIn;
    FBLog("connectFlags -> 0x%X (built-in eDP)", (unsigned int)*flags);
    return kIOReturnSuccess;
}
