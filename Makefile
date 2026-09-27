TARGET = MyIntelGPU
CC = cc

# กำหนดพาธไปยังซอร์สโค้ด/เฮดเดอร์ของ Lilu (แนะนำให้โคลนคลัง Lilu ไว้ข้างๆ โฟลเดอร์โปรเจกต์นี้)
LILU_PATH = ../Lilu

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
    -DLILU_CUSTOM_KEXT \
    -Wno-deprecated-declarations \
    -Wno-inconsistent-missing-override \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Kernel.framework/Headers \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/IOGraphics.framework/Headers \
    -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include \
    -I$(LILU_PATH)/Headers \
    -I$(LILU_PATH)/Sources

# ปรับปรุง LDFLAGS ให้เข้ากันได้กับข้อกำหนดสถาปัตยกรรมของ Lilu Plugin
# ขจัดสัญลักษณ์สถิตที่ระบุตำแหน่งตายตัวออกเพื่อให้ระบบปลั๊กอินสับเปลี่ยน API ได้ง่าย
LDFLAGS = -Xlinker -kext \
    -nostdlib \
    -lkmod \
    -r

SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
    MyIntelAccelerator.cpp MyIntelMedia.cpp \
    MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp \
    MyIntelVCSCommand.cpp MyIntelVCSClient.cpp \
    $(LILU_PATH)/Sources/kern_api.cpp \
    $(LILU_PATH)/Sources/kern_util.cpp

OBJ = $(SRC:.cpp=.o)

all: $(TARGET).kext/Contents/MacOS/$(TARGET)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $<

$(TARGET).kext/Contents/Info.plist: Info.plist
	mkdir -p "$(TARGET).kext/Contents"
	cp Info.plist "$@"
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion 2.0.483" "$@"
	@echo "   version stamped: 2.0.483"

$(TARGET).kext/Contents/MacOS/$(TARGET): $(OBJ) $(TARGET).kext/Contents/Info.plist
	mkdir -p "$(TARGET).kext/Contents/MacOS"
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o "$@" $(OBJ)
	ls -la "$(TARGET).kext/Contents/MacOS/"
	@echo "─── Build complete (Lilu Plugin Mode): $(TARGET).kext  ───"

clean:
	rm -f *.o $(LILU_PATH)/Sources/*.o
	rm -rf $(TARGET).kext
