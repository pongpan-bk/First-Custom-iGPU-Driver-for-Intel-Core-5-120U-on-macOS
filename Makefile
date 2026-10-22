# Makefile MyIntelGPU.kext
#
# Build macOS ! :
#    - Xcode Command Line Tools (xcode-select --install)
#    - MacOSKernelSDK (https://github.com/acidanthera/MacOSKernelSDK)
# Xcode (< 14) Kernel.framework
#
# build:
#    make
#    sudo chown -R root:wheel MyIntelGPU.kext
#    sudo kextutil -v MyIntelGPU.kext
#
# log:
#    sudo dmesg | grep MyIntelGPU

TARGET  = MyIntelGPU
CLASS   = MyIntelGPU
SDK_DIR ?= $(HOME)/MacKernelSDK
ifneq ($(KERNEL_SDK_DIR),)
    SDK_DIR := $(KERNEL_SDK_DIR)
endif
ifeq ($(wildcard $(SDK_DIR)/Headers),)
    SDK_DIR := /opt/MacKernelSDK
endif

# ─── SDK path ──────────────────────────────────────────────
SDK_PATH = $(shell xcrun --show-sdk-path 2>/dev/null)
ifeq ($(SDK_PATH),)
    $(error ERROR: Xcode SDK not found. Run xcode-select --install)
endif

# ─── Kernel Headers ──────────────────────────────────────────────
# MacKernelSDK (acidanthera) structure: Headers/ directly
# → fallback SDK built-in (Xcode 14-15)
KERNEL_HDRS = $(SDK_DIR)
ifneq ($(wildcard $(SDK_DIR)/Headers),)
    KERNEL_HDRS := $(SDK_DIR)
else ifneq ($(wildcard $(SDK_DIR)/Kernel.framework/Headers),)
    KERNEL_HDRS := $(SDK_DIR)
else
    KERNEL_HDRS := $(SDK_PATH)
endif

# ─── Version Stamping ─────────────────────────────────────────────
# ทุก build จะ stamp CFBundleVersion = <major>.<minor>.<build-number> (3 ส่วนเท่านั้น!)
# ⚠️ kext CFBundleVersion รองรับแค่ 3 ส่วน (major 16-bit + minor 8-bit + stage 8-bit)
#    — 4 ส่วน เช่น 2.0.215.216 = INVALID → kmutil ปฏิเสธ kext ตั้งแต่ validate
#    (Error KMErrorDomain Code=30 "CFBundleVersion must be a valid kext version")
#    → kext ไม่เข้า auxKC เลย แก้ยากจนเข้าใจผิดเป็นปัญหา KDK!
# build-number = จำนวน commit ใน git (เลขล้วน, ปลอดภัยกับ kmod version)
# ถ้าไม่มี git → fallback เป็น MMDDHHMM (เดือน/วัน/ชม/นาที)
BASE_VERSION := $(shell /usr/libexec/PlistBuddy -c "Print :CFBundleVersion" Info.plist 2>/dev/null)
GIT_COMMITS  := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)
ifneq ($(GIT_COMMITS),0)
    BUILD_NUMBER := $(GIT_COMMITS)
else
    BUILD_NUMBER := $(shell date +%m%d%H%M)
endif
# ตัด base เหลือ major.minor (2 ส่วนแรก) แล้วต่อ build-number เป็นส่วนที่ 3
STAMPED_VERSION := $(shell echo "$(BASE_VERSION)" | awk -F. '{print $$1"."$$2}').$(BUILD_NUMBER)

# ─── Compiler Flags ──────────────────────────────────────────────
# -mkernel          : kernel ABI (no mxcsr, etc.)
# -arch x86_64 : Intel 64-bit (ARM64 Apple Silicon)
# -nostdlib : libc ( kernel libs )
# -fno-exceptions : IOKit C++ exceptions
# -fno-rtti : IOKit RTTI ( OSDynamicCast )
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
           -fno-stack-protector \
           -fvisibility=hidden \
           -fvisibility-inlines-hidden \
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
      MyIntelAccelerator.cpp \
      MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp
OBJ = MyIntelGPU.o IntelFramebuffer.o MyIntelFramebuffer.o \
      MyIntelAccelerator.o \
      MyIntelRing.o MyIntelGEMBuffer.o MyIntelGPUClient.o

.PHONY: all clean install load unload lint format version strip_binary sizecheck FORCE

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

# FORCE: always out of date — keeps the Info.plist stamp recipe honest
# (the plist target must re-stamp on every build even when source plist
# is untouched, otherwise CFBundleVersion silently stays stale).
# NOTE: must stay BELOW `all:` — as the first regular target it would
# become make's default goal and `make` would rebuild nothing.
FORCE:

# ─── Compile ─────────────────────────────────────────────────────
%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

# ─── Link ────────────────────────────────────────────────────────
$(TARGET).kext/Contents/Info.plist: Info.plist FORCE
	mkdir -p "$(TARGET).kext/Contents"
	cp Info.plist "$@"
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(STAMPED_VERSION)" "$@"
	@echo "   version stamped: $(STAMPED_VERSION)"

$(TARGET).kext/Contents/MacOS/$(TARGET): $(OBJ) $(TARGET).kext/Contents/Info.plist
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(OBJ)
	ls -la "$(TARGET).kext/Contents/MacOS/"
	echo "─── Build complete: $(TARGET).kext  [$(STAMPED_VERSION)] ───"

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
	sudo kextunload -b com.pongpan-bk.MyIntelGPU || true

log:
	sudo dmesg | grep -i "MyIntelGPU" | tail -50

version:
	@echo "Base version : $(BASE_VERSION)"
	@echo "Git commits  : $(GIT_COMMITS)"
	@echo "Stamped      : $(STAMPED_VERSION)"

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
	  MyIntelAccelerator.cpp MyIntelAccelerator.hpp \
	  MyIntelRing.cpp MyIntelRing.hpp \
	  MyIntelGEMBuffer.cpp MyIntelGEMBuffer.hpp 2>&1 || echo "Warning: clang-format not installed — skipping"

# ─── Binary Stripping (Layer 3: Symbol Destruction) ────────────
strip_binary:
	@echo "Stripping symbols from binary..."
	strip -x -S $(TARGET).kext/Contents/MacOS/$(TARGET)
	@ls -la $(TARGET).kext/Contents/MacOS/$(TARGET)
	@echo "Binary stripped. Symbols removed for distribution."

# ─── File Size Check (CI gate: warn if binary > 128KB) ──────────
sizecheck:
	@maxsize=131072; \
	actual=$$(stat -f%z "$(TARGET).kext/Contents/MacOS/$(TARGET)" 2>/dev/null || echo 0); \
	if [ "$$actual" -gt "$$maxsize" ]; then \
		echo "WARNING: Binary $$actual bytes exceeds $$maxsize — check for bloat"; \
	else \
		echo "Size check: $$actual bytes (limit $$maxsize) — OK"; \
	fi
