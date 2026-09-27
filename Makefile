TARGET = MyIntelGPU
CC = cc

# กำหนดพาธไปยังโฟลเดอร์ Lilu (ใช้สำหรับอ้างอิง Headers)
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

# ลิงก์โครงสร้างระบบในระดับแมปส่วนหัว ไม่ต้องคอมไพล์ซอร์สข้ามโฟลเดอร์
LDFLAGS = -Xlinker -kext \
    -nostdlib \
    -lkmod \
    -r

# เอาเฉพาะไฟล์ในโปรเจกต์ของคุณมารวมกัน (ไม่ดึงไฟล์ .cpp ของ Lilu มาปน)
SRC = MyIntelGPU.cpp IntelFramebuffer.cpp MyIntelFramebuffer.cpp \
    MyIntelAccelerator.cpp MyIntelMedia.cpp \
    MyIntelRing.cpp MyIntelGEMBuffer.cpp MyIntelGPUClient.cpp \
    MyIntelVCSCommand.cpp MyIntelVCSClient.cpp

# กำหนดไฟล์วัตถุ .o ให้อยู่เฉพาะในโฟลเดอร์โปรเจกต์ปัจจุบัน
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
	@echo "─── Build complete (Lilu Headers Mode): $(TARGET).kext ───"

clean:
	rm -f *.o
	rm -rf $(TARGET).kext
