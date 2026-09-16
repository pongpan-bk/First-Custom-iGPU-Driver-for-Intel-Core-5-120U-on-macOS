# Makefile MyIntelGPU.kext
#
# Build macOS ! :
#    - Xcode Command Line Tools (xcode-select --install)
#    - MacOSKernelSDK (https://github.com)
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
# ─── Pure Native Driver Mode ────────────────────────────────────────
# No Lilu. kext is a standalone IOService driver only.
# SDK / kernel header resolution (dual layout, per igpu-silicon-reviver):
#   MacKernelSDK : $(SDK)/Headers/IOKit/IOService.h + $(SDK)/Library/x86_64/libkmod.a
#   Xcode/CLT SDK: $(SDK)/System/Library/Frameworks/Kernel.framework/Headers
# VCS source files also pull <stdio.h>/printf, so CLT usr/include is added
# alongside MacKernelSDK headers (MacKernelSDK ships only sys/stdio.h).
SDK_DIR ?= $(HOME)/MacKernelSDK
ifneq ($(KERNEL_SDK_DIR),)
    SDK_DIR := $(KERNEL_SDK_DIR)
endif
ifeq ($(wildcard $(SDK_DIR)/Headers/IOKit/IOService.h),)
    ifneq ($(wildcard /opt/MacKernelSDK/Headers/IOKit/IOService.h),)
        SDK_DIR := /opt/MacKernelSDK
    else
        SDK_DIR := $(shell xcrun --show-sdk-path 2>/dev/null)
    endif
endif
ifeq ($(wildcard $(SDK_DIR)/Headers/IOKit/IOService.h),)
    # Xcode/CLT SDK layout
    USE_MKS :=
    SDK_XCRUN := $(SDK_DIR)
else
    # MacKernelSDK layout
    USE_MKS := 1
    SDK_XCRUN := $(shell xcrun --show-sdk-path 2>/dev/null)
endif

# ─── Version Stamping ─────────────────────────────────────────────
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
STAMPED_VERSION := $(shell echo "$(BASE_VERSION).0.0" | awk -F. '{m=$$1?$$1:1; n=$$2?$$2:0; print m"."n}').$(BUILD_NUMBER)

# ─── Compiler Flags ──────────────────────────────────────────────
ifeq ($(USE_MKS),1)
KERNEL_HEADERS   = -I$(SDK_DIR)/Headers \
                   -I$(SDK_XCRUN)/usr/include
KMOD_LIB         = $(SDK_DIR)/Library/x86_64/libkmod.a
else
KERNEL_HEADERS   = -I$(SDK_XCRUN)/System/Library/Frameworks/Kernel.framework/Headers \
                   -I$(SDK_XCRUN)/usr/include
KMOD_LIB         =
endif
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
           $(KERNEL_HEADERS)

# ─── Linker Flags ────────────────────────────────────────────────
LDFLAGS = -Xlinker -kext

# ─── Sources (Pure Native — 10 files, no Lilu plugin shim) ───────
SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
      MyIntelAccelerator.cpp MyIntelMedia.cpp \
      MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp \
      MyIntelVCSCommand.cpp MyIntelVCSClient.cpp
OBJ = MyIntelGPU.o IntelFramebuffer.o MyIntelFramebuffer.o MyIntelMedia.o \
      MyIntelAccelerator.o MyIntelRing.o MyIntelGEMBuffer.o MyIntelGPUClient.o \
      MyIntelVCSCommand.o MyIntelVCSClient.o

.PHONY: all clean install load unload lint format version strip_binary tools FORCE

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

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
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(OBJ) $(if $(wildcard $(KMOD_LIB)),$(KMOD_LIB))
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

# ─── Code Quality (Pure Native files) ────────────────────────────
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

# ─── Binary Stripping ─────────────────────────────────────────────
strip_binary:
	@echo "Stripping symbols from binary..."
	strip -x -S $(TARGET).kext/Contents/MacOS/$(TARGET)
	@ls -la $(TARGET).kext/Contents/MacOS/$(TARGET)
	@echo "Binary stripped. Symbols removed for distribution."

# ─── File Size Check ──────────────────────────────────────────────
sizecheck:
	@maxsize=131072; \
	actual=$$(stat -f%z "$(TARGET).kext/Contents/MacOS/$(TARGET)" 2>/dev/null || echo 0); \
	if [ "$$actual" -gt "$$maxsize" ]; then \
		echo "WARNING: Binary $$actual bytes exceeds $$maxsize — check for bloat"; \
	else \
		echo "Size check: $$actual bytes (limit $$maxsize) — OK"; \
	fi
