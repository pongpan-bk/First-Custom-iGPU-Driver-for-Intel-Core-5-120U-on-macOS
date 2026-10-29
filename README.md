<div align="center">

# ⚡ MYINTELGPU — HONEST IDENTITY DRIVER

### Custom macOS Kernel Extension for Intel Gen 12 / Raptor Lake-U iGPU

[![Built with OpenCode](https://shields.io)](https://opencode.ai)
[![Architecture: Gen12 Alder/Raptor](https://shields.io)](https://github.com)
[![Target OS: macOS](https://shields.io)](https://github.com)

<p align="center">
  <b>A standalone, hardware-level driver ported from Linux i915 register knowledge. Engineered from scratch to run supported graphics endpoints on hardware Apple never implemented.</b>
</p>

</div>

---

## 📖 Overview

**MyIntelGPU** เป็นไดรเวอร์ระบบในระดับเคอร์เนล (Kernel Extension) ขนาดกึ่งสแตนด์อโลน (Semi-Standalone) ที่ถูกสร้างขึ้นมาเพื่อขับเคลื่อนชิปกราฟิกออนบอร์ด **Intel Core 5 120U (Device ID: `0xA7AC`)** บนระบบปฏิบัติการ macOS โดยใช้วิธีพอร์ตองค์ความรู้การจัดการรีจิสเตอร์และการควบคุมบัสจากโครงสร้างไดรเวอร์ `i915` บน Linux Kernel ดั้งเดิม

### 🛑 No More Spoofing / No FakeID
ความโดดเด่นที่สุดของโปรเจกต์นี้คือการทำงานด้วยกลไก **Honest Identity (ตัวตนที่แท้จริง)** ผ่านการทำไฟล์ `Info.plist` ให้เข้าไปจับคู่พอร์ตอุปกรณ์และเคลมสิทธิ์โดยตรง (IOKit Personality Matching) ด้วยค่า PCI Device ID ตรงตามซิลิคอนจริง **ทำให้ตัวไดรเวอร์สามารถโหลดและทำงานได้ทันทีที่ไดเรกทอรี `/Library/Extensions` โดยไม่ต้องพึ่งพาการ Spoof ค่าแต่อย่างใด** ส่งผลให้ระบบทำงานได้เสถียรและแม่นยำสูงสุดขอบเขตฮาร์ดแวร์

---

## 🛠️ โครงสร้างสถาปัตยกรรมภายใน (What's Inside)

ตัวไดรเวอร์ถูกออกแบบโครงสร้างระดับ Bare-Metal แยกออกจากระบบสั่งงานสากล เพื่อควบคุมท่อสัญญาณการประมวลผลและการแสดงผลโดยตรง:

*   **Memory Management:** ระบบจัดสรรและเตรียมความพร้อมหน่วยความจำผ่านกลไก GGTT/GSM Initialization, การตั้งค่า VRAM Pool ขนาดทดสอบ 4096 MB และระบบผูกสิทธิ์สัญญานโหนด GEM Buffers
*   **Command Streamers:** ชุดควบคุมคิววงแหวนคำสั่งประมวลผลหลัก RCS (Render) และ BCS (Blitter) วิ่งตรงผ่านระบบส่งคำสั่งทางลัด Execlists (Logical Ring Context - LRC)
*   **Interrupt Plumbing:** การต่อท่อสัญญาณขัดจังหวะระบบฮาร์ดแวร์หลัก (Gen11+ GT Interrupt Plumbing) แบบสมบูรณ์ไร้รอยต่อ
*   **Display Diagnostics:** โหมดสแกนและดักจับเฟรมบัฟเฟอร์ (Display-State Framebuffer Snapshot) เพื่อสแกนสัญญาณพอร์ตควบคุมหน้าจอภายในของโน้ตบุ๊กโดยเฉพาะ

> ⚠️ **Note:** ตัวแปรสัญลักษณ์คงที่ (Source-level constants) และข้อมูลจำเพาะทางเทคนิคบางส่วนในซอร์สโค้ดถูกบีบอัดและปิดบังเพื่อความปลอดภัยเชิงระบบ โดยไฟล์ `Info.plist` จะทำหน้าที่บรรจุค่าไบนารี Device ID ตรงตัวเพื่อใช้สู้สิทธิ์บนระบบ IOKit ของ macOS

---

## 🔬 Architectural Deep-Dive & Low-Level Fixes

### 1. The Silicon Reality (Alder Lake-P Roots)
จากการสืบค้นประวัติระดับฮาร์ดแวร์ บล็อกสถาปัตยกรรมประมวลผลกราฟิก (Silicon Subsystem) ของชิป **Core 5 120U** ถูกยืนยันว่าทำงานอยู่บนสถาปัตยกรรมรากเหง้าของ **Intel Alder Lake-P (Gen 12)** ส่งผลให้ไดรเวอร์ชุดนี้สามารถแชร์โครงสร้างตารางข้อมูลพอร์ตและคอนเน็กเตอร์ ร่วมกับชุดคำสั่ง Gen 12 แบบดั้งเดิมได้ทั้งหมดอย่างแม่นยำ

### 2. การซ่อมแซมบั๊ก Bit-Shift & Memory Corruption (`0x71000013` Bug)
ในการยิงคำสั่งประมวลผลวิดีโอระดับล่างตรงเข้าหาท่อส่ง **VCS Ring (VDBOX)** ข้ามเลเยอร์คำสั่งของ Metal และ VideoToolbox ตัวแอดเดรสหน่วยความจำ 64 บิตจะต้องส่งค่าผ่านบัฟเฟอร์คำสั่งตรงๆ โดยไม่มีการเลื่อนตำแหน่งบิต 

ไดรเวอร์ตัวนี้ได้ทำการแก้ไขบั๊กตัวจัดการตำแหน่งหน่วยความจำที่ผิดพลาด (`>> 6` Bit-Shift Error) โดยเปลี่ยนมาใช้ระบบล้างเศษบิตล่าง 6 บิต เพื่อตรึงตำแหน่งแอดเดรสภาพ Luma และพื้นที่ Scratch บัฟเฟอร์ (`kScrIntra` / `kScrDeblock`) ให้ลงล็อกขอบเขต 64-byte พอดีตามข้อกำหนดสถาปัตยกรรมฮาร์ดแวร์:

```cpp
// ล้างเฉพาะ 6 บิตล่างเพื่อล็อกขอบเขตแอดเดรสหน่วยความจำกราฟิก Gen 12
static constexpr uint64_t kMhwAddressMask = ~0x3FULL;

void writeVcsRingAddress64(uint32_t *commandBuffer, size_t dwIndex, uint64_t gpuAddress) {
    uint64_t alignedAddress = gpuAddress & kMhwAddressMask;
    commandBuffer[dwIndex]     = (uint32_t)(alignedAddress & 0xFFFFFFFFu); // Low DWORD
    commandBuffer[dwIndex + 1] = (uint32_t)(alignedAddress >> 32);         // High DWORD
}
```

**ผลลัพธ์หลังการแก้ไข:** ระบบสามารถส่งผ่านบัฟเฟอร์วิดีโอความละเอียดสูงพิเศษ (เช่น สตรีมวิดีโอระบบ **8K HDR IMAX** บน YouTube) วิ่งตรงเข้าชิปถอดรหัสฮาร์ดแวร์ 80 EUs ได้ทันทีอย่างราบรื่น โดยไม่ทำให้โปรเซสจัดการหน้าต่างส่วนกลางอย่าง `WindowServer` เกิดอาการโหลดล้นหรือเกิดอาการหน้าจอค้าง (Poison Frame) อีกต่อไป

---

## 🏗️ วิธีการคอมไพล์และการติดตั้ง (Build & Installation)

### เครื่องมือที่จำเป็น (Prerequisites)
การคอมไพล์ซอร์สโค้ดจำเป็นต้องใช้เครื่องมือ **Xcode Command Line Tools** ควบคู่กับชุดพัฒนาเคอร์เนล **MacKernelSDK** (แนะนำให้จัดวางไว้ที่โฟลเดอร์ `~/MacKernelSDK` หรือ `/opt/MacKernelSDK`)

### ขั้นตอนการคอมไพล์ (Compiling)

```bash
git clone https://github.com
cd First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS
make
```


### ขั้นตอนการติดตั้ง (Deployment)
> 🚨 **Disclaimer:** นี่คือโค้ดระดับเคอร์เนลที่จัดการและควบคุมฮาร์ดแวร์โดยตรง การติดตั้งและรันไฟล์มีความเสี่ยงต่อตัวระบบ ผู้ใช้งานโปรดรับความเสี่ยงในการทดสอบด้วยตนเอง

```bash
# 1. ตั้งค่าสิทธิ์ไฟล์ระบบให้ถูกต้องตามข้อกำหนดเคอร์เนล (Root:Wheel)
sudo chown -R root:wheel MyIntelGPU.kext
sudo chmod -R 755 MyIntelGPU.kext

# 2. คัดลอกเข้าไดเรกทอรีส่วนขยายระบบและสั่งอัปเดตระบบแคชเคอร์เนล
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo kmutil install --volume-root /
```

---

## 🤖 Credits & Tooling

*   **Lead Developer & Hardware Analyst:** [pongpan-bk](https://github.com) - ผู้ออกแบบและวางโครงสร้างการเจาะระบบท่อส่งข้อมูลการ์ดจอ Gen 12 บน macOS
*   **Core Engineering AI Agent:** [OpenCode AI](https://opencode.ai) - ระบบผู้ช่วยวิเคราะห์และจัดระเบียบโครงสร้างคำสั่งคิว Execlists, ตัวลงทะเบียนพอร์ตควบคุมตำแหน่งรีจิสเตอร์ และคำนวณสัญญาณท่อขัดจังหวะระบบ (End-to-End Core Synthesis)

---
<div align="center">
  Developed with ❤️ by pongpan-bk · Contact Developer via <a href="https://facebook.com">Facebook</a>
</div>
