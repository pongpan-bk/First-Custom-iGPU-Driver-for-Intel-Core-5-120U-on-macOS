TARGET = MyIntelGPU
CC = cc

CXXFLAGS = -std=c++11 \
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
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Kernel.framework/Headers \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/IOGraphics.framework/Headers \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include

LDFLAGS = -Xlinker -kext

SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
    MyIntelAccelerator.cpp MyIntelMedia.cpp \
    MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp \
    MyIntelVCSCommand.cpp MyIntelVCSClient.cpp

OBJ = $(SRC:.cpp=.o)

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(TARGET).kext/Contents/Info.plist: Info.plist
	mkdir -p "$(TARGET).kext/Contents"
	cp Info.plist "$@"
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion 2.0.483" "$@"
	@echo "   version stamped: 2.0.483"

$(TARGET).kext/Contents/MacOS/$(TARGET): $(addsuffix .o,$(basename $(SRC))) $(TARGET).kext/Contents/Info.plist
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(addsuffix .o,$(basename $(SRC)))
	ls -la "$(TARGET).kext/Contents/MacOS/"
	@echo "─── Build complete: $(TARGET).kext  [2.0.483] ───"

clean:
	rm -rf $(TARGET).kext *.o
