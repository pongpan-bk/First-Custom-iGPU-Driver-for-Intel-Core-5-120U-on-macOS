/*===========================================================================
 *  MyIntelEDIDParser.hpp
 *  Hackintosh Kext — EDID Parser for Display Detection (Phase 8)
 *
 *  Parses VESA EDID 1.4 + CEA-861 extension blocks
 *  No C++ STL, no exceptions, no RTTI — IOKit only
 *///=========================================================================

#ifndef __MY_INTEL_EDID_PARSER_HPP__
#define __MY_INTEL_EDID_PARSER_HPP__

#include <IOKit/IOService.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <libkern/libkern.h>

#define MYEDID_BASE_BLOCK_SIZE      128
#define MYEDID_EXT_BLOCK_SIZE       128
#define MYEDID_MAX_BLOCKS           4
#define MYEDID_MAX_MODES            32

#define MYEDID_MANUFACTURER_OFFSET  8
#define MYEDID_PRODUCT_OFFSET       10
#define MYEDID_SERIAL_OFFSET        12
#define MYEDID_WEEK_OFFSET          16
#define MYEDID_YEAR_OFFSET          17
#define MYEDID_VERSION_OFFSET       18
#define MYEDID_REVISION_OFFSET      19
#define MYEDID_INPUT_TYPE_OFFSET    20
#define MYEDID_HSIZE_OFFSET         21
#define MYEDID_VSIZE_OFFSET         22
#define MYEDID_GAMMA_OFFSET         23
#define MYEDID_FEATURES_OFFSET      24
#define MYEDID_COLOR_OFFSET         25
#define MYEDID_EST_TIMING_OFFSET    35
#define MYEDID_STD_TIMING_OFFSET    38
#define MYEDID_DET_TIMING_OFFSET    54
#define MYEDID_EXT_COUNT_OFFSET     126
#define MYEDID_CHECKSUM_OFFSET      127

/* CEA-861 Extension Tags */
#define MYEDID_CEA_TAG_AUDIO        0x01
#define MYEDID_CEA_TAG_VIDEO        0x02
#define MYEDID_CEA_TAG_VENDOR       0x03
#define MYEDID_CEA_TAG_SPEAKER      0x04
#define MYEDID_CEA_TAG_VESA_DTC     0x05
#define MYEDID_CEA_TAG_HDMI_VSDB    0x07
#define MYEDID_CEA_TAG_DTD          0x09

/* Video Input Type bits */
#define MYEDID_DIGITAL_INPUT        0x80
#define MYEDID_DF_1BPC              0x00
#define MYEDID_DF_2BPC              0x10
#define MYEDID_DF_3BPC              0x20
#define MYEDID_DF_4BPC              0x30

/* Established Timing bits (byte 35) */
#define MYEDID_EST_720x400_70       (1U << 0)
#define MYEDID_EST_720x400_88       (1U << 1)
#define MYEDID_EST_640x480_60       (1U << 2)
#define MYEDID_EST_640x480_67       (1U << 3)
#define MYEDID_EST_640x480_72       (1U << 4)
#define MYEDID_EST_640x480_75       (1U << 5)
#define MYEDID_EST_800x600_56       (1U << 6)
#define MYEDID_EST_800x600_60       (1U << 7)
/* byte 36 */
#define MYEDID_EST_800x600_72       (1U << 0)
#define MYEDID_EST_800x600_75       (1U << 1)
#define MYEDID_EST_832x624_75       (1U << 2)
#define MYEDID_EST_1024x768_87      (1U << 3)
#define MYEDID_EST_1024x768_60      (1U << 4)
#define MYEDID_EST_1024x768_70      (1U << 5)
#define MYEDID_EST_1024x768_75      (1U << 6)
#define MYEDID_EST_1280x1024_75     (1U << 7)
/* byte 37 */
#define MYEDID_EST_1152x870_75      (1U << 0)

class MyIntelEDIDParser : public OSObject {
    OSDeclareDefaultStructors(MyIntelEDIDParser)

public:
    virtual bool init(void);
    virtual void free(void);

    /* Parse full EDID (base + extensions) */
    bool parse(const UInt8 *edidData, UInt32 length);

    /* Accessors */
    UInt32 getModeCount(void) const { return fModeCount; }
    bool getModeInfo(UInt32 index, IODisplayModeInformation *info) const;

    const char *getManufacturer(void) const { return fManufacturer; }
    UInt16 getProductID(void) const { return fProductID; }
    UInt32 getSerialNumber(void) const { return fSerialNumber; }
    UInt8 getManufactureWeek(void) const { return fMfgWeek; }
    UInt16 getManufactureYear(void) const { return fMfgYear; }
    UInt8 getEDIDVersion(void) const { return fEDIDVersion; }
    UInt8 getEDIDRevision(void) const { return fEDIDRevision; }
    bool isDigitalInput(void) const { return fDigitalInput; }
    UInt8 getColorDepth(void) const { return fColorDepth; }
    bool hasAudioSupport(void) const { return fHasAudio; }
    bool isDP(void) const { return fIsDP; }
    bool isHDMI(void) const { return fIsHDMI; }
    UInt16 getMaxHorizontalSize(void) const { return fMaxHSize; }
    UInt16 getMaxVerticalSize(void) const { return fMaxVSize; }

    /* Detailed timing access */
    UInt32 getDetailedTimingCount(void) const { return fDetailedTimingCount; }
    bool getDetailedTiming(UInt32 index, UInt32 *pixelClock, UInt16 *hActive, UInt16 *hBlank,
                           UInt16 *vActive, UInt16 *vBlank, UInt8 *hSyncOffset, UInt8 *hSyncWidth,
                           UInt8 *vSyncOffset, UInt8 *vSyncWidth, UInt8 *flags, UInt8 *stereo) const;

    /* Public struct for MyIntelFramebuffer access */
    struct DetailedTiming {
        UInt32 pixelClock;      /* kHz */
        UInt16 hActive;
        UInt16 hBlank;
        UInt16 vActive;
        UInt16 vBlank;
        UInt8 hSyncOffset;
        UInt8 hSyncWidth;
        UInt8 vSyncOffset;
        UInt8 vSyncWidth;
        UInt8 flags;            /* sync polarity, stereo, etc. */
        UInt8 stereo;
    };

    void dumpParsedEDID(void) const;

private:
    /* Parsed data */
    char fManufacturer[4];
    UInt16 fProductID;
    UInt32 fSerialNumber;
    UInt8 fMfgWeek;
    UInt16 fMfgYear;
    UInt8 fEDIDVersion;
    UInt8 fEDIDRevision;
    bool fDigitalInput;
    UInt8 fColorDepth;
    UInt16 fMaxHSize;
    UInt16 fMaxVSize;
    bool fHasAudio;
    bool fIsDP;
    bool fIsHDMI;

    /* Mode list from detailed timing descriptors */
    DetailedTiming fDetailedTimings[MYEDID_MAX_MODES];
    UInt32 fDetailedTimingCount;
    UInt32 fModeCount;

    /* Extension data */
    UInt8 fExtensionCount;
    UInt8 fExtensionData[MYEDID_MAX_BLOCKS][MYEDID_EXT_BLOCK_SIZE];

    /* Helpers */
    bool parseBaseBlock(const UInt8 *block);
    bool parseExtensionBlock(const UInt8 *block, UInt32 index);
    bool parseCEA861Extension(const UInt8 *block, UInt32 length);
    bool parseAudioDataBlock(const UInt8 *data, UInt32 length);
    bool parseVideoDataBlock(const UInt8 *data, UInt32 length);
    bool parseVendorSpecificBlock(const UInt8 *data, UInt32 length);
    bool parseDetailedTiming(const UInt8 *desc, DetailedTiming *timing);
    bool validateChecksum(const UInt8 *block, UInt32 size) const;
    UInt16 decodeManufacturer(const UInt8 *bytes) const;
    void decodeManufacturerString(UInt16 code, char *out) const;
    UInt32 modeInfoFromTiming(const DetailedTiming *t, IODisplayModeInformation *info) const;
    UInt16 readLE16(const UInt8 *ptr) const;
    UInt32 readLE32(const UInt8 *ptr) const;
};

#endif /* __MY_INTEL_EDID_PARSER_HPP__ */