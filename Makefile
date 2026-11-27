# Makefile — Pure Thin-Bridge Kext (OpenCore & /L/E Compatible)
KEXT_NAME       := MyIntelGPU
BUNDLE_ID       := com.pongpan-bk.MyIntelGPU
# กฎเหล็ก OpenCore: เลขหลักกลางและหลักท้ายห้ามเกิน 99
KEXT_VERSION    := 5.1.0

KERNEL_SDK_DIR  ?= $(HOME)/Documents/MacKernelSDK
BUILD_DIR       := build/thin-kext
KEXT_BUNDLE     := $(BUILD_DIR)/$(KEXT_NAME).kext
KEXT_MACOS      := $(KEXT_BUNDLE)/Contents/MacOS
KEXT_BIN        := $(KEXT_MACOS)/$(KEXT_NAME)

CXX             := xcrun clang++
# คัดเฉพาะไฟล์ล่ามฮาร์ดแวร์ระดับล่างแท้ๆ (ตัด Command Builder และ Metal Stub ออก)
KEXT_SRCS       := MyIntelGPU.cpp \
                   MyIntelRing.cpp \
                   MyIntelGEMBuffer.cpp \
                   MyIntelMedia.cpp \
                   MyIntelGPUClient.cpp \
                   MyIntelVCSClient.cpp \
                   MyIntelEDIDParser.cpp

KEXT_OBJS       := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(KEXT_SRCS))

CXXFLAGS        := -arch x86_64 -mmacosx-version-min=13.0 \
                   -mkernel -fapple-kext -ffreestanding \
                   -fno-builtin -fno-exceptions -fno-rtti -fno-common \
                   -O2 -Wall -Wextra -Wno-unused-parameter \
                   -I. -I$(KERNEL_SDK_DIR)/Headers

# บังคับ Classic Linker + ปิด Fixup Chains เพื่อให้ทั้ง OpenCore และ kmutil อ่านผ่าน 100%
LDFLAGS         := -arch x86_64 -mmacosx-version-min=13.0 \
                   -Wl,-kext -nostdlib \
                   -Wl,-ld_classic -Wl,-no_fixup_chains \
                   -Wl,-exported_symbols_list,exported_symbols_thin.txt \
                   -lkmodc++ -lkmod -lcc_kext

.PHONY: all clean check-abi

all: $(KEXT_BUNDLE) check-abi

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KEXT_BUNDLE): $(KEXT_OBJS) Info.plist
	@mkdir -p $(KEXT_MACOS)
	$(CXX) $(LDFLAGS) $(KEXT_OBJS) -o $(KEXT_BIN)
	cp Info.plist $(KEXT_BUNDLE)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(KEXT_VERSION)" $(KEXT_BUNDLE)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(KEXT_VERSION)" $(KEXT_BUNDLE)/Contents/Info.plist
	xattr -cr $(KEXT_BUNDLE)
	codesign -fs - --deep $(KEXT_BUNDLE)

# ตรวจสอบอัตโนมัติว่าไม่มี Chained Fixups และไม่มีสัญลักษณ์หลุดไป SystemKC
check-abi: $(KEXT_BUNDLE)
	@echo "=== ตรวจสอบความคลีนของไบนารี ==="
	@! otool -l $(KEXT_BIN) | grep -q "LC_DYLD_CHAINED_FIXUPS" || (echo "❌ ติด Chained Fixups!" && exit 1)
	@! nm -u $(KEXT_BIN) | grep -qE "IOAccelerator|IOFramebuffer" || (echo "❌ มีสัญลักษณ์หลุดไป SystemKC!" && exit 1)
	@stat -f "✅ บิลด์สำเร็จ ขนาดไบนารี: %z bytes (พร้อมใช้ทั้ง OpenCore และ /L/E)" $(KEXT_BIN)

clean:
	rm -rf $(BUILD_DIR) *.o
