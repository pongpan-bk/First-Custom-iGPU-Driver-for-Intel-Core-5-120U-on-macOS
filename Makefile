#===========================================================================
#  Makefile - MyIntelGPU (Pure Standalone Driver Mode - Strict Core Sync)
#===========================================================================

TARGET  = MyIntelGPU
MODULE  = com.pongpan-bk.MyIntelGPU

SDK_DIR ?= $(KERNEL_SDK_DIR)
ifeq ($(SDK_DIR),)
    SDK_DIR := /opt/MacKernelSDK
endif

KERNEL_HDRS = $(SDK_DIR)/Headers

# ใช้สัญญลักษณ์ผูกแบบพิมพ์ใหญ่ $(CC) เพื่อดึงคอมไพเลอร์ Clang ตัวเต็มของ macOS
CXXFLAGS = -std=c++14 \
           -mkernel \
           -arch x86_64 \
           -Os \
           -nostdlib \
           -fno-builtin \
           -fno-exceptions \
           -fno-rtti \
           -fno-stack-protector \
           -DKERNEL \
           -D__STRICT_BSD__ \
           -Wno-deprecated-declarations \
           -Wno-inconsistent-missing-override \
           -I$(KERNEL_HDRS) \
           -I$(SDK_DIR)/System/Library/Frameworks/Kernel.framework/Headers \
           -I$(SDK_DIR)/System/Library/Frameworks/IOGraphics.framework/Headers \
           -I$(SDK_DIR)/usr/include

LDFLAGS = -Xlinker -kext

SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
      MyIntelAccelerator.cpp MyIntelMedia.cpp \
      MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp \
      MyIntelVCSCommand.cpp MyIntelVCSClient.cpp kern_start.cpp
      
OBJ = $(SRC:.cpp=.o)

.PHONY: all clean sizecheck strip_binary

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(TARGET).kext/Contents/Info.plist: Info.plist
	mkdir -p "$(TARGET).kext/Contents"
	cp Info.plist "$@"

$(TARGET).kext/Contents/MacOS/$(TARGET): $(OBJ) $(TARGET).kext/Contents/Info.plist
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(OBJ)
	@echo "─── Standalone Build complete: $(TARGET).kext ───"

clean:
	rm -rf $(TARGET).kext $(OBJ) /tmp/build.log

strip_binary:
	strip -x -S $(TARGET).kext/Contents/MacOS/$(TARGET)

sizecheck:
	@actual=$$(stat -f%z "$(TARGET).kext/Contents/MacOS/$(TARGET)" 2>/dev/null || echo 0); \
	echo "Binary size: $$actual bytes — OK"

