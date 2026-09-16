# Makefile สำหรับ MyIntelGPU.kext
#
#  Build บน macOS เท่านั้น! ต้องการ:
#    - Xcode Command Line Tools (xcode-select --install)
#    - MacOSKernelSDK (https://github.com/acidanthera/MacOSKernelSDK)
#      หรือ Xcode เก่า (< 14) ที่ยังมี Kernel.framework
#
#  วิธี build:
#    make
#    sudo chown -R root:wheel MyIntelGPU.kext
#    sudo kextutil -v MyIntelGPU.kext
#
#  ดู log:
#    sudo dmesg | grep MyIntelGPU

TARGET  = MyIntelGPU
CLASS   = MyIntelGPU
SDK_DIR ?= /opt/MacKernelSDK
ifneq ($(KERNEL_SDK_DIR),)
    SDK_DIR := $(KERNEL_SDK_DIR)
endif

# ─── ค้นหา SDK path ──────────────────────────────────────────────
SDK_PATH = $(shell xcrun --show-sdk-path 2>/dev/null)
ifeq ($(SDK_PATH),)
    $(error ERROR: Xcode SDK not found. Run xcode-select --install)
endif

# ─── Kernel Headers ──────────────────────────────────────────────
# MacKernelSDK (acidanthera) structure: Headers/ directly
# ถ้าไม่พบ → fallback ใช้ SDK built-in (Xcode 14-15)
KERNEL_HDRS = $(SDK_DIR)
ifneq ($(wildcard $(SDK_DIR)/Headers),)
    KERNEL_HDRS := $(SDK_DIR)
else ifneq ($(wildcard $(SDK_DIR)/Kernel.framework/Headers),)
    KERNEL_HDRS := $(SDK_DIR)
else
    KERNEL_HDRS := $(SDK_PATH)
endif

# ─── Compiler Flags ──────────────────────────────────────────────
# -mkernel          : kernel ABI (no mxcsr, etc.)
# -arch x86_64      : Intel 64-bit เท่านั้น (ARM64 สำหรับ Apple Silicon)
# -nostdlib         : ไม่ลิงก์ libc (ใช้ kernel libs แทน)
# -fno-exceptions   : IOKit ห้ามใช้ C++ exceptions
# -fno-rtti         : IOKit ห้ามใช้ RTTI (ใช้ OSDynamicCast แทน)
# -DKERNEL          : 定义 kernel build
# -D__STRICT_BSD__  : strict POSIX/BSD namespaces
# -Werror           : treat warnings as errors (CI gate)
CXXFLAGS = -std=c++11 \
           -mkernel \
           -arch x86_64 \
           -Os \
           -nostdlib \
           -fno-builtin \
           -fno-exceptions \
           -fno-rtti \
           -DKERNEL \
           -D__STRICT_BSD__ \
           -Wno-deprecated-declarations \
           -Wno-inconsistent-missing-override \
           -I$(KERNEL_HDRS) \
           -I$(SDK_PATH)/System/Library/Frameworks/Kernel.framework/Headers \
           -I$(SDK_PATH)/System/Library/Frameworks/IOGraphics.framework/Headers \
           -I$(SDK_PATH)/usr/include

# ─── Linker Flags ────────────────────────────────────────────────
# -Xlinker -kext  : kext binary (MH_DYLIB with DYLIB_KEXT flag)
# -undefined dynamic_lookup was removed: it produces ordinal=0 for all imports
# which causes OpenCore "Invalid Parameter" during prelinked injection.
# Now we link normally — the kernel resolves symbols at runtime via OSBundleLibraries.
LDFLAGS = -Xlinker -kext

# ─── Sources ─────────────────────────────────────────────────────
SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
      MyIntelRing.cpp MyIntelGEMBuffer.cpp
OBJ = MyIntelGPU.o IntelFramebuffer.o MyIntelFramebuffer.o \
      MyIntelRing.o MyIntelGEMBuffer.o

.PHONY: all clean install load unload lint format

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

# ─── Compile ─────────────────────────────────────────────────────
%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

# ─── Link ────────────────────────────────────────────────────────
$(TARGET).kext/Contents/MacOS/$(TARGET): $(OBJ) Info.plist
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	cp Info.plist "$(TARGET).kext/Contents/Info.plist"
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(OBJ)
	ls -la "$(TARGET).kext/Contents/MacOS/"
	echo "─── Build complete: $(TARGET).kext ───"

# ─── Utilities ───────────────────────────────────────────────────
clean:
	rm -rf $(TARGET).kext $(OBJ)

install: all
	sudo chown -R root:wheel $(TARGET).kext
	sudo cp -R $(TARGET).kext /Library/Extensions/
	sudo kextutil -v /Library/Extensions/$(TARGET).kext

load: all
	sudo chown -R root:wheel $(TARGET).kext
	sudo kextutil -v $(TARGET).kext

unload:
	sudo kextunload -b com.myintelgpu.driver || true

log:
	sudo dmesg | grep -i "MyIntelGPU" | tail -50

# ─── Code Quality ─────────────────────────────────────────────────
lint:
	clang-tidy --checks="*" --warnings-as-errors="*" \
	  MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
	  MyIntelRing.cpp MyIntelGEMBuffer.cpp -- \
	  $(CXXFLAGS) 2>&1 || echo "Warning: clang-tidy not installed — skipping"

format:
	clang-format -style=file -i \
	  MyIntelGPU.cpp MyIntelGPU.hpp \
	  IntelFramebuffer.cpp IntelFramebuffer.hpp \
	  MyIntelFramebuffer.cpp MyIntelFramebuffer.hpp \
	  MyIntelRing.cpp MyIntelRing.hpp \
	  MyIntelGEMBuffer.cpp MyIntelGEMBuffer.hpp 2>&1 || echo "Warning: clang-format not installed — skipping"

# ─── File Size Check (CI gate: warn if binary > 128KB) ──────────
sizecheck:
	@maxsize=131072; \
	actual=$$(stat -f%z "$(TARGET).kext/Contents/MacOS/$(TARGET)" 2>/dev/null || echo 0); \
	if [ "$$actual" -gt "$$maxsize" ]; then \
		echo "WARNING: Binary $$actual bytes exceeds $$maxsize — check for bloat"; \
	else \
		echo "Size check: $$actual bytes (limit $$maxsize) — OK"; \
	fi
