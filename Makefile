# Makefile — Intel iGPU Silicon Reviver (GPU-only core)
#
# Builds MyIntelGPU.kext from the 6 GPU source files. No media/VCS components.
#
# Prerequisites:
#    - Xcode Command Line Tools (xcode-select --install); its SDK ships the
#      Kernel.framework headers used for kernel kext builds.
#    - acidanthera/MacOSKernelSDK is optional; used only when the installed
#      SDK drops the kernel headers (set SDK_DIR to point at it).
#
# Build:
#    make
#    sudo chown -R root:wheel MyIntelGPU.kext
#    sudo kextutil -v MyIntelGPU.kext
#
# Log:
#    sudo dmesg | grep MyIntelGPU

TARGET  = MyIntelGPU
CLASS   = MyIntelGPU

# ─── SDK / kernel header resolution ─────────────────────────
# Prefer MacOSKernelSDK if present, else fall back to the Xcode/CLT SDK.
#   KERNEL_SDK_DIR   latest check   : if MacOSKernelSDK path is supplied, or
#   $(HOME)/MacKernelSDK             : common local checkout location
#   /opt/MacKernelSDK                : where CI/homebrew style installs go
#   SDK_PATH kernel headers          : built-in SDK fallback
SDK_DIR ?= $(HOME)/MacKernelSDK
ifneq ($(KERNEL_SDK_DIR),)
    SDK_DIR := $(KERNEL_SDK_DIR)
endif
ifeq ($(wildcard $(SDK_DIR)/Headers),)
    SDK_DIR := /opt/MacKernelSDK
endif
ifeq ($(wildcard $(SDK_DIR)/Headers),)
    SDK_DIR := $(shell xcrun --show-sdk-path 2>/dev/null)
endif

# ─── Version Stamping ─────────────────────────────────────────
# Stamp CFBundleVersion = <major>.<minor>.<build-number> (3 parts only).
# kext CFBundleVersion only accepts 3 parts (16.8.8 bits); a 4-part value is
# invalid and kmutil rejects the kext at validate time (KMErrorDomain 30).
# build-number = git commit count; falls back to MMDDHHMM when not a repo.
BASE_VERSION := $(shell /usr/libexec/PlistBuddy -c "Print :CFBundleVersion" Info.plist 2>/dev/null)
GIT_COMMITS  := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)
ifneq ($(GIT_COMMITS),0)
    BUILD_NUMBER := $(GIT_COMMITS)
else
    BUILD_NUMBER := $(shell date +%m%d%H%M)
endif
STAMPED_VERSION := $(shell echo "$(BASE_VERSION)" | awk -F. '{print $$1"."$$2}').$(BUILD_NUMBER)

# ─── Sources (GPU-only) ─────────────────────────────────────
SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
      MyIntelAccelerator.cpp MyIntelRing.cpp MyIntelGEMBuffer.cpp
OBJ = $(SRC:.cpp=.o)

# ─── Compiler / linker flags ────────────────────────────────
KERNEL_HEADERS   = -I$(SDK_DIR)/System/Library/Frameworks/Kernel.framework/Headers \
                   -I$(SDK_DIR)/System/Library/Frameworks/IOKit.framework/Headers \
                   -I$(SDK_DIR)/usr/include
CFLAGS  = -std=c++11 -mkernel -arch x86_64 -Os -nostdlib -fno-builtin \
          -fno-exceptions -fno-rtti -fno-stack-protector -DKERNEL -D__STRICT_BSD__ \
          -Wno-deprecated-declarations -Wno-inconsistent-missing-override \
          $(KERNEL_HEADERS)
LDFLAGS = -Xlinker -kext

all: kext

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

kext: $(OBJ)
	rm -rf "$(TARGET).kext"
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	cp Info.plist "$(TARGET).kext/Contents/Info.plist"
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $(STAMPED_VERSION)" "$(TARGET).kext/Contents/Info.plist"
	@echo "   version stamped: $(STAMPED_VERSION)"
	$(CXX) $(CFLAGS) $(LDFLAGS) -o "$(TARGET).kext/Contents/MacOS/$(TARGET)" $(OBJ)
	@echo "─── Build complete: $(TARGET).kext  [$(STAMPED_VERSION)] ───"

version:
	@echo "$(STAMPED_VERSION)"

clean:
	rm -rf "$(TARGET).kext" $(OBJ)

.PHONY: all kext version clean
