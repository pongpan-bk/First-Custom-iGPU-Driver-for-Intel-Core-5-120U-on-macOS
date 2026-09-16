#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

#define GEN9_DEV_ID      0x3E9B // Coffee Lake (Fake Device ID)
#define GEN12_TGL_ID     0x9A49 // Tiger Lake
#define GEN12_ADL_ID     0x46A6 // Alder Lake
#define DSMBASE_REG      0x1080C0
#define GTT_SIZE_4MB     0x400000

struct RegisterMapping {
    uint32_t realGenReg;
    uint32_t fakeGenReg;
};

class MyIntelGPU : public IOService {
    OSDeclareDefaultStructors(MyIntelGPU);

private:
    IOPCIDevice* pciDevice{nullptr};
    IOMemoryMap* mmioMap{nullptr};
    volatile uint32_t* mmioBase{nullptr};
    bool isGen12{false};
    uint32_t stolenMemorySize{0};
    uint64_t dsmBase{0};

    RegisterMapping translationTable[16];
    size_t translationCount{0};

public:
    virtual bool start(IOService* provider) override {
        if (!IOService::start(provider)) return false;

        pciDevice = OSDynamicCast(IOPCIDevice, provider);
        if (!pciDevice) return false;

        pciDevice->retain();
        pciDevice->open(this);

        if (!detectHardwareGeneration()) {
            IOLog("MyIntelGPU: Hardware does not require Gen12 translation.\n");
            return true;
        }

        buildTranslationTable();
        detectStolenMemory();
        ggttInitHardware();

        IOLog("MyIntelGPU: Successfully initialized Gen12 to Gen9 translation layer.\n");
        return true;
    }

    virtual void stop(IOService* provider) override {
        if (mmioMap) {
            mmioMap->release();
            mmioMap = nullptr;
        }
        if (pciDevice) {
            pciDevice->close(this);
            pciDevice->release();
            pciDevice = nullptr;
        }
        IOService::stop(provider);
    }

    bool detectHardwareGeneration() {
        uint16_t vendorId = pciDevice->configRead16(0x02);
        uint16_t deviceId = pciDevice->configRead16(0x00);

        if (vendorId != 0x8086) return false;

        // ตรวจสอบ Device ID ให้ครอบคลุมตามรายการ IOPCIMatch ใน Info.plist
        switch (deviceId) {
            case 0x20A7: case 0x21A7: case 0xA0A7: case 0xA1A7:
            case 0xA2A7: case 0xA3A7: case 0xA8A7: case 0xA9A7:
            case 0xAAA7: case 0xABA7: case 0x26A7: case 0xA7AC:
            case GEN12_TGL_ID: case GEN12_ADL_ID:
                isGen12 = true;
                IOLog("MyIntelGPU: Detected Intel Gen12 GPU (0x%04X). Spoofing as Gen9 (0x%04X).\n", deviceId, GEN9_DEV_ID);
                return true;
            default:
                return false;
        }
    }

    void buildTranslationTable() {
        translationTable[0] = { 0x02030, 0x02050 }; // RCS Ring Register translation
        translationTable[1] = { 0x12030, 0x12050 }; // VCS Ring Register translation
        translationCount = 2;
    }

    uint32_t readReg32(uint32_t offset) {
        for (size_t i = 0; i < translationCount; i++) {
            if (translationTable[i].fakeGenReg == offset) {
                offset = translationTable[i].realGenReg;
                break;
            }
        }
        return mmioBase ? mmioBase[offset / 4] : 0;
    }

    void writeReg32(uint32_t offset, uint32_t value) {
        for (size_t i = 0; i < translationCount; i++) {
            if (translationTable[i].fakeGenReg == offset) {
                offset = translationTable[i].realGenReg;
                break;
            }
        }
        if (mmioBase) {
            mmioBase[offset / 4] = value;
        }
    }

    uint32_t readAperture32(uint32_t offset) {
        return readReg32(offset);
    }

    void writeAperture32(uint32_t offset, uint32_t value) {
        writeReg32(offset, value);
    }

    void ggttInitHardware() {
        IOLog("MyIntelGPU: Initializing GGTT mapping.\n");
        writeReg32(0x100000, GTT_SIZE_4MB);
    }

    void detectStolenMemory() {
        dsmBase = pciDevice->configRead32(0x5C) & 0xFFFFF000;
        stolenMemorySize = readReg32(DSMBASE_REG);
        IOLog("MyIntelGPU: DSMBASE = 0x%llX, Stolen Memory = %u MB\n", dsmBase, stolenMemorySize / (1024 * 1024));
    }
};

OSDefineMetaClassAndStructors(MyIntelGPU, IOService);