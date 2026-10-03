/*===========================================================================
 *  MyIntelFramebuffer.hpp
 *  Hackintosh Kext ? IOFramebuffer Binding (Phase 4)
 *
 *  IOFramebuffer subclass ?????? macOS 15 Sequoia SDK
 *  Pure virtual methods ??????? implement:
 *    - getApertureRange(IOPixelAperture)
 *    - getPixelFormats()
 *    - getInformationForDisplayMode()
 *    - getPixelInformation()
 *    - getPixelFormatsForDisplayMode()
 *///=========================================================================

#ifndef __MY_INTEL_FRAMEBUFFER_HPP__
#define __MY_INTEL_FRAMEBUFFER_HPP__

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/IOPMpowerState.h>
#include "MyIntelGPU.hpp"

#pragma mark - Constants

#define MYFB_MODE_COUNT         1
#define MYFB_DISPLAY_MODE_ID    1

#define MYFB_H_ACTIVE           1920
#define MYFB_V_ACTIVE           1080
#define MYFB_BITS_PER_PIXEL     32
#define MYFB_BYTES_PER_PIXEL    4
#define MYFB_BYTES_PER_ROW      (MYFB_H_ACTIVE * MYFB_BYTES_PER_PIXEL)
#define MYFB_PLANE_STRIDE_64B   (MYFB_BYTES_PER_ROW / 64) /* 7680 / 64 = 120 (0x78) in 64B units */
#define MYFB_BITS_PER_COMP      8
#define MYFB_REFRESH_RATE       60

#define MYFB_BRIGHTNESS_MAX     100
#define MYFB_BRIGHTNESS_DEFAULT 80

#pragma mark - MyIntelFramebuffer Class

class MyIntelFramebuffer : public IOFramebuffer {
    OSDeclareDefaultStructors(MyIntelFramebuffer)

public:
    virtual bool init(OSDictionary *dict) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;

    /* Pure Virtual Methods */
    virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    virtual const char *getPixelFormats(void) override;
    virtual IOReturn getInformationForDisplayMode(
            IODisplayModeID displayMode,
            IODisplayModeInformation *info) override;
    virtual IOReturn getPixelInformation(
            IODisplayModeID displayMode,
            IOIndex depth,
            IOPixelAperture aperture,
            IOPixelInformation *pixelInfo) override;
    virtual UInt64 getPixelFormatsForDisplayMode(
            IODisplayModeID displayMode,
            IOIndex depth) override;

    /* Non-Pure Virtual Overrides */
    virtual UInt32 getDisplayModeCount(void) override;
    virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
    virtual IOReturn getCurrentDisplayMode(
            IODisplayModeID *displayMode,
            IOIndex *depth) override;
    virtual IOReturn setDisplayMode(
            IODisplayModeID displayMode,
            IOIndex depth) override;
    virtual IODeviceMemory *getVRAMRange(void) override;
    virtual IOItemCount getConnectionCount(void) override;
    virtual IOReturn getStartupDisplayMode(
            IODisplayModeID *displayMode,
            IOIndex *depth) override;
    virtual IOReturn getTimingInfoForDisplayMode(
            IODisplayModeID displayMode,
            IOTimingInformation *info) override;
    virtual IOReturn connectFlags(
            IOIndex connectIndex,
            IODisplayModeID displayMode,
            IOOptionBits *flags) override;
    virtual IOReturn getAttributeForConnection(
            IOIndex connectIndex,
            IOSelect key,
            uintptr_t *value) override;
    virtual IOReturn setAttributeForConnection(
            IOIndex connectIndex,
            IOSelect key,
            uintptr_t value) override;
    virtual bool hasDDCConnect(IOIndex connectIndex) override;
    virtual IOReturn getDDCBlock(IOIndex connectIndex, UInt32 blockNumber,
                    IOSelect blockType, IOOptionBits options,
                    UInt8 * data, IOByteCount * length) override;

    /* Public */
    void handleVblank(void);
    void setBrightness(UInt32 brightness);
    IOReturn performPowerStateChange(
            IOIndex connectIndex,
            UInt32 powerState);

    MyIntelGPU *getGPU(void) const { return fGPU; }
    UInt64      getFrameCounter(void) const { return fFrameCounter; }
    UInt32      getBrightness(void) const { return fBrightness; }
    bool        isDisplayOn(void) const { return fDisplayOn; }

    /* 2.0.223: diagnostic counter dump (ALIVE tick / stop()) */
    void dumpDiagnostics(void) const;

private:
    MyIntelGPU          *fGPU;
    IODisplayModeID      fCurrentModeID;
    IOIndex              fCurrentDepth;
    bool                 fDisplayOn;
    UInt64               fFrameCounter;
    AbsoluteTime         fLastVblankTime;
    bool                 fVSyncEnabled;
    UInt32               fBrightness;
    IODeviceMemory      *fVRAMDescriptor;

    /* 2.0.223 evidence counters (WindowServer drive loop detection) */
    UInt32               fAttrGetCount;   /*!< getAttributeForConnection calls */
    UInt32               fAttrSetCount;   /*!< setAttributeForConnection calls */
    UInt32               fPowerOnCount;   /*!< performPowerStateChange ON */
    UInt32               fPowerOffCount;  /*!< performPowerStateChange OFF */
    UInt32               fApertureCount;  /*!< getApertureRange calls */
    IOTimerEventSource  *fLocationTimer;
    static void locationTimerFired(OSObject *owner, IOTimerEventSource *sender);

    void setupDefaultMode(void);
    bool createVRAMDescriptor(void);
    void publishIORegistryProperties(void);
};

#endif /* __MY_INTEL_FRAMEBUFFER_HPP__ */
