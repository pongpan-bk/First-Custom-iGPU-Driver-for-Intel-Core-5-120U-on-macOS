KEXT_NAME       := MyIntelGPU
KEXT_VERSION    := 5.1.0
KERNEL_SDK_DIR  ?= $(HOME)/Documents/MacKernelSDK

BUILD_DIR       := build/thin-objs
KEXT_BUNDLE     := $(KEXT_NAME).kext
KEXT_MACOS      := $(KEXT_BUNDLE)/Contents/MacOS
KEXT_BIN        := $(KEXT_MACOS)/$(KEXT_NAME)

CXX             := xcrun clang++
# หากรันครั้งแรกแล้วยังมีฟังก์ชันใน MyIntelGPU.cpp เรียกหาไฟล์เดิม ให้เติมชื่อไฟล์นั้นไว้ก่อนแล้วค่อยทยอยตัดสายใน .cpp ทีละจุด
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
                   -fno-stack-protector \
                   -O2 -Wall -Wno-unused-parameter \
                   -I. -I$(KERNEL_SDK_DIR)/Headers

LDFLAGS         := -arch x86_64 -mmacosx-version-min=13.0 \
                   -L$(KERNEL_SDK_DIR)/Library/x86_64 \
                   -Wl,-kext -nostdlib \
                   -Wl,-ld_classic -Wl,-no_fixup_chains \
                   -Wl,-exported_symbols_list,exported_symbols.txt \
                   -lkmodc++ -lkmod -lcc_kext

.PHONY: all clean

all: $(KEXT_BUNDLE)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KEXT_BUNDLE): $(KEXT_OBJS) Info.plist
	@rm -rf $(KEXT_BUNDLE)
	@mkdir -p $(KEXT_MACOS)
	$(CXX) $(LDFLAGS) $(KEXT_OBJS) -o $(KEXT_BIN)
	cp Info.plist $(KEXT_BUNDLE)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(KEXT_VERSION)" $(KEXT_BUNDLE)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(KEXT_VERSION)" $(KEXT_BUNDLE)/Contents/Info.plist
	codesign -fs - --deep $(KEXT_BUNDLE)

clean:
	rm -rf $(BUILD_DIR) $(KEXT_BUNDLE) *.o
