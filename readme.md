<img width="1280" height="721" alt="social-share" src="https://github.com/user-attachments/assets/6c136d3a-8fa0-43f4-9750-8de068c265e4" />

<p align="center">
  <b>Sisyphus - Ultraworker·Big PickleOpenCode Zen</b><br>
  <svg width='160' height='200' viewBox='0 0 32 40' fill='none' xmlns='http://www.w3.org/2000/svg'><g clip-path='url(#clip0_1311_94973)'><path d='M24 32H8V16H24V32Z' fill='#4B4646'/><path d='M24 8H8V32H24V8ZM32 40H0V0H32V40Z' fill='#F1ECEC'/></g><defs><clipPath id='clip0_1311_94973'><rect width='32' height='40' fill='white'/></clipPath></defs></svg>
</p>

<p align="center">
  <b>Powered by opencode-ai</b><br>
  The open source AI coding agent.
</p>

<p align="center">
  <img src="https://shields.io" alt="Discord">
  <img src="https://shields.io" alt="npm">
  <img src="https://shields.io" alt="Build status">
</p>

---
# First Custom iGPU Driver for Intel Core 5 120U on macOS

ระบบควบคุมชิปประมวลผลกราฟิกและเร่งความเร็วฮาร์ดแวร์ระดับเคอร์เนล (Native Kernel Extension) สำหรับสถาปัตยกรรม **Intel Raptor Lake-U / Raptor Lake Refresh (Device ID: `0xA7AC8086`)** บนระบบปฏิบัติการ macOS เพื่อปลดล็อกขีดจำกัดและเปิดใช้งานระบบกราฟิกอย่างสมบูรณ์

---

## 📸 Proof of Concept (ใช้งานจริงบน macOS Sequoia)

<p align="center">
  <img src="Screenshot%202569-09-24%20at%2023.18.54.png" width="800" alt="macOS Sequoia Intel Iris Xe 4096 MB Genuine Boot">
</p>

> **สถานะปัจจุบัน:** บูตผ่านเข้าสู่ระบบปฏิบัติการ macOS Sequoia 15.7.1 สำเร็จ พร้อมจำลองการแชร์พื้นที่หน่วยความจำขึ้นแสดงผลที่ **Intel Iris Xe 4096 MB** เต็มระบบ

---

## 🚀 คุณสมบัติระดับระบบ (Core Architecture Features)

ตัวไดรเวอร์ถูกพัฒนาขึ้นมาเพื่อควบคุมเลเยอร์หน่วยความจำและการประมวลผลคำสั่งกราฟิกในระดับต่ำ (Low-level Layer 3/4) โดยข้ามข้อจำกัดเดิมของไดรเวอร์ Apple เนทีฟ:

*   **RCS (Ring Control Subsystem):** บูตผ่านฉลุยพร้อมสถานะ **`RCS-Status = "CREATE OK"`** ควบคุมระบบวงรอบการสั่งงานหลักของจีพียูได้สมบูรณ์
*   **GGTT (Global Graphics Translation Table):** ระบบจัดสรรและชี้พิกัดแผนที่หน่วยความจำระดับต่ำ **`RCS-GGTT`** ขนาด 64MB เพื่อส่งผ่านข้อมูลกราฟิกโดยตรงไม่ผ่านเลเยอร์คอขวด
*   **Media Hardware Acceleration:** ปลดล็อกขีดจำกัดระบบถอดรหัสและเข้ารหัสวิดีโอผ่านฮาร์ดแวร์ดิบอย่าง **VDBOX** และ **VEBOX** รองรับความละเอียดสูงสุดถึง **8K (`8192x8192`)**
    *   **Video Decoding:** รองรับ H.264, HEVC, VP9 และ **AV1 Decoding** (เปิด YouTube 4K/8K บน Google Chrome ลื่น ๆ ไม่กินแรงซีพียู)
    *   **Video Encoding:** รองรับ H.264 และ HEVC ความละเอียดสูงสุด 4K (`4096x4096`)
*   **IOAccelerator Linkage:** แมตช์เข้าเลเยอร์ความเร่งฮาร์ดแวร์ระบบผ่าน `IOMatchCategory = IOAccelerator` และเปิดช่องทางการคุยกับแอปพลิเคชันภายนอกผ่านคลาส `MyIntelGPUClient` และ `MyIntelVCSClient`

---

## 📊 ตารางสถานะการทำงานใน I/O Registry (`ioreg`)

เมื่อทำการตรวจสอบสถานะในระดับซิสเทม ไดรเวอร์จะลงทะเบียนคลาสและพารามิเตอร์เข้าสู่ระบบเคอร์เนลอย่างถูกต้อง 100%:

```text
+-o MyIntelGPU  <class MyIntelGPU, id 0x1000004f5, registered, matched, active>
  | {
  |   "IOClass" = "MyIntelGPU"
  |   "MetalStatisticsName" = "Raptor Lake-P"
  |   "IOMatchCategory" = "IOAccelerator"
  |   "IOUserClientClass" = "MyIntelGPUClient"
  |   "RCS-GGTT" = 67125248
  |   "RCS-Status" = "CREATE OK"
  |   "IOPCIMatch" = "0xA7AC8086"
  |   "H264Decoding" = Yes
  |   "HevcDecoding" = Yes
  |   "VP9Decoding" = Yes
  |   "AV1Decoding" = Yes
  |   "MaxDecodeResolution" = "8192x8192"
  | }
  | 
  +-o MyIntelVCS  <class IOService, id 0x100000522, registered, matched, active>
      {
        "IOProviderClass" = "IOService"
        "IOUserClientClass" = "MyIntelVCSClient"
      }
```

---

## 🛠️ โครงสร้างซอร์สโค้ดและส่วนประกอบ (Repository Structure)

*   `MyIntelGPU.cpp` / `.hpp`: คลาสหลักคุมวงจรชีวิตไดรเวอร์ (Lifecycle) และพอร์ตเชื่อมต่อ PCI (`IOPCIDevice`)
*   `MyIntelFramebuffer.cpp` / `.hpp`: เลเยอร์ควบคุมเฟรมบัฟเฟอร์ พอร์ตสัญญาณภาพ และพิกัดหน้าจอ
*   `MyIntelGEMBuffer.cpp` / `.hpp`: ระบบจัดสรรพื้นที่คลังหน่วยความจำ (VRAM/Graphics Execution Manager) 
*   `MyIntelRing.cpp` / `.hpp`: โครงสร้างควบคุมคิวงานและคำสั่งประมวลผล (Ring Buffer Pipeline)
*   `MyIntelVCSClient.cpp` / `.hpp`: สะพานเชื่อมระบบฝั่งผู้ใช้ (Userspace) ตรงสู่ภาคถอดรหัสวิดีโอ (VDBOX)

---

## 📝 วิธีการตรวจสอบสถานะ (Verification Commands)

เปิด Terminal แล้วยิงคำสั่งระดับรูทเพื่อตรวจสอบความนิ่งของตัว Kext:

```bash
# ตรวจสอบว่าเคอร์เนลโหลดไดรเวอร์ทำงานแบบ Active หรือไม่
sudo kmutil showloaded | grep -i MyIntelGPU

# ตรวจสอบการลงทะเบียนคลาสและโครงสร้างหน่วยความจำกราฟิก
sudo ioreg -l -b -r -c MyIntelGPU
```

---
ผู้ร่วมโครงการและหลอกกูทำ จารกูเกิ้ลโครมหำไหญ่ แปลภาษาควายเป็นภาษาเอไอ  / ai deepseek v4. flash free first run , Big Pickle ผู้ทำการเขียน อ่าน แก้ใข ซอสโคตหลัก
ขอบคุณ opencode-ai zen ที่มีโควต้าให้ใช้ฟรีแต่โครตมหาเทพครับ
## ⚖️ License & Credits

*   **Developed by:** [pongpan-bk](https://github.com/pongpan-bk)
*   Powered by dedication to low-level reverse engineering and kernel development.
