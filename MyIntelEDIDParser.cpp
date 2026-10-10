/*===========================================================================
 *  MyIntelEDIDParser.cpp
 *  Hackintosh Kext — EDID Parser Implementation (Phase 8)
 *///=========================================================================

#include "MyIntelEDIDParser.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/graphics/IODisplay.h>
#include <libkern/libkern.h>

#define EDIDLog(fmt, ...) \
    IOLog("MyIntelEDID: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define super IOService
OSDefineMetaClassAndStructors(MyIntelEDIDParser, IOService)

#pragma mark - init / free

bool MyIntelEDIDParser::init(void)
{
    if (!super::init()) {
        return false;
    }

    fManufacturer[0] = '\0';
    fProductID = 0;
    fSerialNumber = 0;
    fMfgWeek = 0;
    fMfgYear = 0;
    fEDIDVersion = 0;
    fEDIDRevision = 0;
    fDigitalInput = false;
    fColorDepth = 0;
    fMaxHSize = 0;
    fMaxVSize = 0;
    fHasAudio = false;
    fIsDP = false;
    fIsHDMI = false;
    fExtensionCount = 0;
    fDetailedTimingCount = 0;
    fModeCount = 0;

    memset(fDetailedTimings, 0, sizeof(fDetailedTimings));
    memset(fExtensionData, 0, sizeof(fExtensionData));

    EDIDLog("init() — OK");
    return true;
}

void MyIntelEDIDParser::free(void)
{
    EDIDLog("free()");
    super::free();
}

#pragma mark - Public API

bool MyIntelEDIDParser::parse(const UInt8 *edidData, UInt32 length)
{
    if (!edidData || length < MYEDID_BASE_BLOCK_SIZE) {
        EDIDLog("ERROR: Invalid EDID data (length=%u)", length);
        return false;
    }

    EDIDLog("Parsing EDID: %u bytes", length);

    /* Parse base block (first 128 bytes) */
    if (!parseBaseBlock(edidData)) {
        EDIDLog("ERROR: Base block parse failed");
        return false;
    }

    /* Parse extension blocks if present */
    if (fExtensionCount > 0) {
        UInt32 extOffset = MYEDID_BASE_BLOCK_SIZE;
        for (UInt8 i = 0; i < fExtensionCount && extOffset < length; i++) {
            if (extOffset + MYEDID_EXT_BLOCK_SIZE <= length) {
                bcopy(edidData + extOffset, fExtensionData[i], MYEDID_EXT_BLOCK_SIZE);
                parseExtensionBlock(fExtensionData[i], i);
                extOffset += MYEDID_EXT_BLOCK_SIZE;
            }
        }
    }

    /* Build mode list from detailed timings */
    fModeCount = fDetailedTimingCount;
    EDIDLog("Parse complete: %u detailed timings, %u extension blocks",
            fDetailedTimingCount, fExtensionCount);

    return true;
}

bool MyIntelEDIDParser::getModeInfo(UInt32 index, IODisplayModeInformation *info) const
{
    if (!info || index >= fModeCount) {
        return false;
    }

    return modeInfoFromTiming(&fDetailedTimings[index], info);
}

void MyIntelEDIDParser::dumpParsedEDID(void) const
{
    EDIDLog("=== Parsed EDID ===");
    EDIDLog("Manufacturer: %s (0x%04X)", fManufacturer, decodeManufacturer((const UInt8*)fManufacturer));
    EDIDLog("Product ID: 0x%04X", fProductID);
    EDIDLog("Serial: 0x%08X", fSerialNumber);
    EDIDLog("Mfg Date: Week %u, Year %u", fMfgWeek, fMfgYear);
    EDIDLog("EDID Version: %u.%u", fEDIDVersion, fEDIDRevision);
    EDIDLog("Digital Input: %s", fDigitalInput ? "Yes" : "No");
    EDIDLog("Color Depth: %u bpc", fColorDepth);
    EDIDLog("Max Size: %u x %u mm", fMaxHSize, fMaxVSize);
    EDIDLog("Audio Support: %s", fHasAudio ? "Yes" : "No");
    EDIDLog("Interface: %s%s%s", fIsDP ? "DP" : "", fIsHDMI ? "HDMI" : "",
            (!fIsDP && !fIsHDMI) ? "Unknown" : "");
    EDIDLog("Detailed Timings: %u", fDetailedTimingCount);

    for (UInt32 i = 0; i < fDetailedTimingCount; i++) {
        const DetailedTiming *t = &fDetailedTimings[i];
        EDIDLog("  Mode %u: %ux%u @ %u Hz (pclk=%u kHz)",
                i, t->hActive, t->vActive,
                (t->pixelClock * 1000) / ((t->hActive + t->hBlank) * (t->vActive + t->vBlank)),
                t->pixelClock);
    }
}

#pragma mark - Private Helpers

bool MyIntelEDIDParser::parseBaseBlock(const UInt8 *block)
{
    if (!validateChecksum(block, MYEDID_BASE_BLOCK_SIZE)) {
        EDIDLog("WARNING: Base block checksum invalid");
    }

    /* Header: 00 FF FF FF FF FF FF 00 */
    if (block[0] != 0x00 || block[1] != 0xFF || block[2] != 0xFF ||
        block[3] != 0xFF || block[4] != 0xFF || block[5] != 0xFF ||
        block[6] != 0xFF || block[7] != 0x00) {
        EDIDLog("ERROR: Invalid EDID header");
        return false;
    }

    /* Manufacturer ID (bytes 8-9) - 3 chars encoded in 16 bits */
    UInt16 mfgCode = readLE16(&block[MYEDID_MANUFACTURER_OFFSET]);
    decodeManufacturerString(mfgCode, fManufacturer);

    /* Product ID (bytes 10-11) */
    fProductID = readLE16(&block[MYEDID_PRODUCT_OFFSET]);

    /* Serial Number (bytes 12-15) */
    fSerialNumber = readLE32(&block[MYEDID_SERIAL_OFFSET]);

    /* Manufacture Week/Year (bytes 16-17) */
    fMfgWeek = block[MYEDID_WEEK_OFFSET];
    fMfgYear = 1990 + block[MYEDID_YEAR_OFFSET];

    /* EDID Version/Revision (bytes 18-19) */
    fEDIDVersion = block[MYEDID_VERSION_OFFSET];
    fEDIDRevision = block[MYEDID_REVISION_OFFSET];

    /* Video Input Type (byte 20) */
    fDigitalInput = (block[MYEDID_INPUT_TYPE_OFFSET] & MYEDID_DIGITAL_INPUT) != 0;
    UInt8 df = block[MYEDID_INPUT_TYPE_OFFSET] & 0x30;
    fColorDepth = (df == MYEDID_DF_1BPC) ? 6 : (df == MYEDID_DF_2BPC) ? 8 :
                  (df == MYEDID_DF_3BPC) ? 10 : (df == MYEDID_DF_4BPC) ? 12 : 8;

    /* Max Horizontal/Vertical Size (bytes 21-22) - cm */
    fMaxHSize = block[MYEDID_HSIZE_OFFSET];
    fMaxVSize = block[MYEDID_VSIZE_OFFSET];

    /* Established Timings (bytes 35-37) - we skip these, use detailed timings */

    /* Standard Timings (bytes 38-53) - 8 entries, 2 bytes each - skip for now */

    /* Detailed Timing Descriptors (bytes 54-125) - 4 descriptors, 18 bytes each */
    for (UInt8 i = 0; i < 4; i++) {
        const UInt8 *desc = &block[MYEDID_DET_TIMING_OFFSET + i * 18];
        if (desc[0] == 0 && desc[1] == 0) {
            /* Monitor descriptor (not timing) - skip for now */
            continue;
        }
        if (fDetailedTimingCount < MYEDID_MAX_MODES) {
            if (parseDetailedTiming(desc, &fDetailedTimings[fDetailedTimingCount])) {
                fDetailedTimingCount++;
            }
        }
    }

    /* Extension block count (byte 126) */
    fExtensionCount = block[MYEDID_EXT_COUNT_OFFSET];

    EDIDLog("Base block: Mfg=%s Prod=0x%04X Ver=%u.%u Ext=%u Modes=%u",
            fManufacturer, fProductID, fEDIDVersion, fEDIDRevision,
            fExtensionCount, fDetailedTimingCount);

    return true;
}

bool MyIntelEDIDParser::parseExtensionBlock(const UInt8 *block, UInt32 index)
{
    if (!validateChecksum(block, MYEDID_EXT_BLOCK_SIZE)) {
        EDIDLog("WARNING: Extension block %u checksum invalid", index);
    }

    UInt8 tag = block[0];
    UInt8 rev = block[1];

    EDIDLog("Extension %u: Tag=0x%02X Rev=%u", index, tag, rev);

    if (tag == 0x02 && rev == 0x03) {
        /* CEA-861 Extension */
        return parseCEA861Extension(block, MYEDID_EXT_BLOCK_SIZE);
    }

    return true;
}

bool MyIntelEDIDParser::parseCEA861Extension(const UInt8 *block, UInt32 length)
{
    /* CEA-861 header: tag=0x02, rev, dtd_offset, flags */
    UInt8 dtdOffset = block[2];
    /* flags = block[3]: bit 0 = underscan, bit 1 = basic audio, bit 2 = YCbCr 4:4:4, bit 3 = YCbCr 4:2:2 */

    fHasAudio = (block[3] & 0x02) != 0;

    /* Data blocks start at offset 4, end at dtdOffset */
    UInt8 *dataPtr = (UInt8 *)&block[4];
    UInt32 dataLen = dtdOffset - 4;

    while (dataLen > 0) {
        UInt8 blockTag = (dataPtr[0] >> 5) & 0x07;
        UInt8 blockLen = dataPtr[0] & 0x1F;

        if (blockLen == 0 || dataLen < (1 + blockLen)) {
            break;
        }

        switch (blockTag) {
            case MYEDID_CEA_TAG_AUDIO:
                parseAudioDataBlock(dataPtr + 1, blockLen);
                break;
            case MYEDID_CEA_TAG_VIDEO:
                parseVideoDataBlock(dataPtr + 1, blockLen);
                break;
            case MYEDID_CEA_TAG_VENDOR:
                parseVendorSpecificBlock(dataPtr + 1, blockLen);
                break;
            default:
                break;
        }

        dataPtr += 1 + blockLen;
        dataLen -= 1 + blockLen;
    }

    /* Parse DTDs in extension if any */
    if (dtdOffset < length) {
        const UInt8 *dtdPtr = &block[dtdOffset];
        UInt32 remaining = length - dtdOffset;
        while (remaining >= 18) {
            if (dtdPtr[0] != 0 || dtdPtr[1] != 0) {
                if (fDetailedTimingCount < MYEDID_MAX_MODES) {
                    parseDetailedTiming(dtdPtr, &fDetailedTimings[fDetailedTimingCount]);
                    fDetailedTimingCount++;
                }
            }
            dtdPtr += 18;
            remaining -= 18;
        }
    }

    return true;
}

bool MyIntelEDIDParser::parseAudioDataBlock(const UInt8 *data, UInt32 length)
{
    /* Audio Data Block: each SAD is 3 bytes */
    for (UInt32 i = 0; i + 2 < length; i += 3) {
        UInt8 format = (data[i] >> 3) & 0x0F;
        UInt8 channels = data[i] & 0x07;
        UInt8 freq = data[i + 1];
        /* byte 2 = byte 2 of SAD */
        (void)channels;
        (void)freq;
        if (format >= 1 && format <= 14) { /* LPCM, AC3, MPEG1, MP3, MPEG2, AAC, DTS, ATRAC, One Bit, Dolby Digital+, DTS-HD, MAT, DST, WMA Pro */
            fHasAudio = true;
        }
    }
    return true;
}

bool MyIntelEDIDParser::parseVideoDataBlock(const UInt8 *data, UInt32 length)
{
    /* Video Data Block: each VIC is 1 byte */
    (void)data;
    (void)length;
    return true;
}

bool MyIntelEDIDParser::parseVendorSpecificBlock(const UInt8 *data, UInt32 length)
{
    /* HDMI VSDB: IEEE OUI 0x000C03 (HDMI) */
    if (length >= 3 && data[0] == 0x03 && data[1] == 0x0C && data[2] == 0x00) {
        fIsHDMI = true;
    }
    /* DisplayPort VSDB: IEEE OUI 0x001101 (VESA) */
    if (length >= 3 && data[0] == 0x01 && data[1] == 0x11 && data[2] == 0x00) {
        fIsDP = true;
    }
    return true;
}

bool MyIntelEDIDParser::parseDetailedTiming(const UInt8 *desc, DetailedTiming *timing)
{
    /* Detailed Timing Descriptor (18 bytes):
     *  0-1: Pixel Clock (10 kHz units, little-endian)
     *  2-3: H Active (lo 8) | H Blank (lo 8)
     *  4:   H Active (hi 4) | H Blank (hi 4)
     *  5-6: V Active (lo 8) | V Blank (lo 8)
     *  7:   V Active (hi 4) | V Blank (hi 4)
     *  8-9: H Sync Offset (lo 8) | H Sync Width (lo 8)
     *  10:  H Sync Offset (hi 4) | H Sync Width (hi 4)
     *  11-12: V Sync Offset (lo 4) | V Sync Width (lo 4) | V Sync Offset (hi 2) | V Sync Width (hi 2)
     *  13-14: H Image Size (mm) | V Image Size (mm)
     *  15:  H Image Size (hi 4) | V Image Size (hi 4)
     *  16:  H Border | V Border
     *  17:  Flags
     */

    timing->pixelClock = readLE16(desc) * 10; /* Convert to kHz */

    timing->hActive = desc[2] | ((desc[4] & 0xF0) << 4);
    timing->hBlank = desc[3] | ((desc[4] & 0x0F) << 8);

    timing->vActive = desc[5] | ((desc[7] & 0xF0) << 4);
    timing->vBlank = desc[6] | ((desc[7] & 0x0F) << 8);

    timing->hSyncOffset = desc[8] | ((desc[10] & 0xF0) << 4);
    timing->hSyncWidth = desc[9] | ((desc[10] & 0x0F) << 8);

    timing->vSyncOffset = (desc[11] & 0xF0) | ((desc[12] & 0xF0) >> 4);
    timing->vSyncWidth = (desc[11] & 0x0F) | (desc[12] & 0x0F);

    timing->flags = desc[17];
    timing->stereo = 0;

    EDIDLog("DTD: %ux%u pclk=%u kHz Hblank=%u Vblank=%u flags=0x%02X",
            timing->hActive, timing->vActive, timing->pixelClock,
            timing->hBlank, timing->vBlank, timing->flags);

    return true;
}

bool MyIntelEDIDParser::validateChecksum(const UInt8 *block, UInt32 size) const
{
    UInt8 sum = 0;
    for (UInt32 i = 0; i < size; i++) {
        sum += block[i];
    }
    return sum == 0;
}

UInt16 MyIntelEDIDParser::decodeManufacturer(const UInt8 *bytes) const
{
    return (bytes[0] << 8) | bytes[1];
}

void MyIntelEDIDParser::decodeManufacturerString(UInt16 code, char *out) const
{
    /* EISA ID: 3 chars, 5 bits each
     * byte 0: bits 7-3 = char 0, bits 2-0 = char 1 bits 4-2
     * byte 1: bits 7-5 = char 1 bits 1-0, bits 4-0 = char 2
     */
    out[0] = 'A' + ((code >> 10) & 0x1F) - 1;
    out[1] = 'A' + (((code >> 5) & 0x1F) | ((code & 0x07) << 2)) - 1; /* Fix: combine properly */
    out[2] = 'A' + (code & 0x1F) - 1;
    out[3] = '\0';

    /* Correct decoding:
     * Char 1 = bits 14-10 (5 bits)
     * Char 2 = bits 9-5 (5 bits)
     * Char 3 = bits 4-0 (5 bits)
     */
    out[0] = 'A' + ((code >> 10) & 0x1F) - 1;
    out[1] = 'A' + ((code >> 5) & 0x1F) - 1;
    out[2] = 'A' + (code & 0x1F) - 1;
    out[3] = '\0';
}

UInt16 MyIntelEDIDParser::readLE16(const UInt8 *ptr) const
{
    return (UInt16)ptr[0] | ((UInt16)ptr[1] << 8);
}

UInt32 MyIntelEDIDParser::readLE32(const UInt8 *ptr) const
{
    return (UInt32)ptr[0] | ((UInt32)ptr[1] << 8) |
           ((UInt32)ptr[2] << 16) | ((UInt32)ptr[3] << 24);
}

UInt32 MyIntelEDIDParser::modeInfoFromTiming(const DetailedTiming *t, IODisplayModeInformation *info) const
{
    if (!info || !t) return false;

    bzero(info, sizeof(IODisplayModeInformation));
    info->nominalWidth = t->hActive;
    info->nominalHeight = t->vActive;
    info->refreshRate = ((t->pixelClock * 1000ULL) << 16) /
                        ((t->hActive + t->hBlank) * (t->vActive + t->vBlank));
    info->maxDepthIndex = 0;
    info->flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
    if (t == &fDetailedTimings[0]) {
        info->flags |= kDisplayModeDefaultFlag;
    }

    return true;
}