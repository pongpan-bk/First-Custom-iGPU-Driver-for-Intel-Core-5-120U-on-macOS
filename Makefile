KEXT_NAME = MyIntelGPU
KEXT_DIR  = $(KEXT_NAME).kext
OC_OUT    = OpenCore-Ready/EFI/OC/Kexts/$(KEXT_DIR)

SDK_PATH  := $(shell xcrun --sdk macosx --show-sdk-path)
CXX       := clang++

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra \
           -arch x86_64 \
           -mkernel -fno-builtin -fno-exceptions -fno-rtti -fno-common \
           -mkernel \
           -isysroot $(SDK_PATH) \
           -I$(SDK_PATH)/System/Library/Frameworks/Kernel.framework/Headers \
           -D__KERNEL__ -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT

LDFLAGS  = -arch x86_64 \
           -isysroot $(SDK_PATH) \
           -Wl,-kext -nostdlib -lkmodc++ -lkmod -lcc_kext

# รวบรวมไฟล์ .cpp ทั้งหมดในโฟลเดอร์หลัก (ยกเว้นโค้ดฝั่ง Userspace Metal เดิม)
SRCS := $(filter-out MyIntelGPUMTLDriver.cpp, $(wildcard *.cpp))
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean opencore

all: opencore

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KEXT_DIR)/Contents/MacOS/$(KEXT_NAME): $(OBJS)
	@mkdir -p $(KEXT_DIR)/Contents/MacOS
	@rm -rf $(KEXT_DIR)/Contents/PlugIns
	$(CXX) $(LDFLAGS) $(OBJS) -o $@
	@cp Info.plist $(KEXT_DIR)/Contents/Info.plist
	@xattr -cr $(KEXT_DIR)
	@codesign --force --sign - $(KEXT_DIR) 2>/dev/null || true

opencore: $(KEXT_DIR)/Contents/MacOS/$(KEXT_NAME)
	@rm -rf OpenCore-Ready
	@mkdir -p OpenCore-Ready/EFI/OC/Kexts
	@cp -R $(KEXT_DIR) $(OC_OUT)
	@xattr -cr $(OC_OUT)
	@echo "✅ สร้างชุดไดรเวอร์ไร้ Bundle สำหรับ OpenCore เรียบร้อยที่: $(OC_OUT)"

clean:
	rm -rf *.o $(KEXT_DIR) MyIntelGPUMTLDriver.bundle OpenCore-Ready
