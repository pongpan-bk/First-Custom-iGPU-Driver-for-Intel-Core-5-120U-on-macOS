กูเลิกทำแล้วววววววววว

<div align="center">

# ⚡ MYINTELGPU — HONEST IDENTITY DRIVER

### Custom macOS Kernel Extension for Intel Gen 12 / Raptor Lake-U iGPU

[![Built with OpenCode](https://img.shields.io/badge/Built%20with-OpenCode-7C3AED?style=for-the-badge&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIyNCIgaGVpZ2h0PSIyNCI+PGNpcmNsZSBjeD0iMTIiIGN5PSIxMiIgcj0iMTAiIGZpbGw9Im5vbmUiIHN0cm9rZT0id2hpdGUiIHN0cm9rZS13aWR0aD0iMiIvPjwvc3ZnPg==)](https://opencode.ai)
[![Architecture](https://shields.io)](https://github.com/pongpan-bk)
[![Platform](https://shields.io)](https://github.com/pongpan-bk)

<p align="center">
  <b>A standalone, hardware-level driver ported from i915 register knowledge. Engineered from scratch to run supported graphics endpoints on hardware Apple never implemented.</b>
</p>

</div>

---

## 📖 Overview

**MyIntelGPU** เป็นไดรเวอร์ (Kernel Extension) ขนาดกึ่งสแตนด์อโลนสำหรับกราฟิกออนบอร์ด **Intel Core 5 120U (Device ID: `0xA7AC`)** ที่รันบนระบบปฏิบัติการ macOS พัฒนาขึ้นโดยการพอร์ตองค์ความรู้การจัดการรีจิสเตอร์จากระบบบัสลินุกซ์ (i915 Driver Base) 

ความพิเศษของไดรเวอร์ตัวนี้คือการรันด้วย **Honest Identity (ตัวตนที่แท้จริง)** โดยตัวไฟล์ `Info.plist` จะทำการเคลมสิทธิ์และจับคู่พอร์ตอุปกรณ์กราฟิก (IOKit Personality Matching) ด้วยค่า PCI Device ID ของตัวมันเองโดยตรง **ทำให้ระบบโหลดทำงานได้เองที่ `/Library/Extensions` โดยไม่ต้องพึ่งพาการทำ FakeID หรือโกงค่าผ่าน OpenCore Device Properties** ให้ระบบแปรปรวน

---

## 🛠️ What's Inside

โครงสร้างสถาปัตยกรรมภายในประกอบด้วยระบบควบคุม Display-First และการจัดการวงแหวนคำสั่งระดับ Bare-Metal:

- **Memory Management:** ระบบจัดสรรหน่วยความจำภายใน GGTT/GSM Initialization, การจัดการ VRAM Pool ขนาด 4096 MB และการผูกค่าสิทธิ์โหนด GEM Buffers
- **Command Streamers:** ระบบควบคุมคิววงแหวนคำสั่งหลัก RCS (Render) และ BCS (Blitter) ผ่านกลไกการส่งคำสั่งตรงแบบ Execlists (Logical Ring Context - LRC)
- **Interrupt Plumbing:** ระบบท่อส่งสัญญาณขัดจังหวะฮาร์ดแวร์ Gen11+ GT Interrupt Plumbing แบบสมบูรณ์
- **Diagnostics:** ระบบตรวจสอบและดักจับเฟรมบัฟเฟอร์ (Display-State Framebuffer Snapshot) สำหรับการสแกนสัญญาณจอโน้ตบุ๊กผ่านพอร์ตลอจิกภายใน

> ⚠️ **Note:** ตัวแปรสัญญลักษณ์คงที่ (Source-level constants) บางส่วนภายในซอร์สโค้ดถูกบีบอัดและปิดบังเพื่อความปลอดภัยเชิงระบบ และไฟล์ `Info.plist` บรรจุค่าไบนารี Device ID ตรงตัวสำหรับการสู้สิทธิ์ IOKit

---

## 🔬 Architectural Deep-Dive & Low-Level Fixes

### 1. The Silicon Reality (Alder Lake-P Roots)
จากการสืบค้นประวัติระดับ Bare-Metal บล็อกสถาปัตยกรรมประมวลผลกราฟิก (Silicon Subsystem) ของชิป Core 5 120U ถูกยืนยันว่าทำงานอยู่บนฐานรากของ **Intel Alder Lake-P (Gen 12)** ทำให้ไดรเวอร์ตัวนี้สามารถแชร์ตารางแมปข้อมูลพอร์ตและคอนเน็กเตอร์ร่วมกับสถาปัตยกรรม Gen 12 ดั้งเดิมได้ทั้งหมด

### 2. The Bit-Shift & Memory Corruption Repair (`0x71000013` Bug)
ในการส่งคำสั่งควบคุมระดับล่างตรงเข้าหาท่อส่ง **VCS Ring (VDBOX)** ข้ามระบบคำสั่งส่วนหน้าของ Metal และ VideoToolbox ตัวแอดเดรสหน่วยความจำ 64 บิตจะต้องส่งค่าผ่านบัฟเฟอร์คำสั่งตรงๆ โดยไม่มีการเลื่อนตำแหน่งบิต 

ไดรเวอร์ตัวนี้ได้ทำการแก้ไขบั๊กตัวจัดการตำแหน่งหน่วยความจำที่ผิดพลาด (`>> 6` Bit-Shift Error) โดยเปลี่ยนมาใช้ระบบล้างเศษบิตล่าง 6 บิตเพื่อตรึงตำแหน่งแอดเดรสภาพ Luma และพื้นที่ Scratch (`kScrIntra` / `kScrDeblock`) ให้ลงล็อกขอบเขต 64-byte อย่างแม่นยำตามกลไกบัสฮาร์ดแวร์แท้:

```cpp
// ล้างเฉพาะ 6 บิตล่างเพื่อล็อกขอบเขตสถาปัตยกรรมหน่วยความจำกราฟิก Gen 12
static constexpr uint64_t kMhwAddressMask = ~0x3FULL;

void writeVcsRingAddress64(uint32_t *commandBuffer, size_t dwIndex, uint64_t gpuAddress) {
    uint64_t alignedAddress = gpuAddress & kMhwAddressMask;
    commandBuffer[dwIndex]     = (uint32_t)(alignedAddress & 0xFFFFFFFFu); // Low DWORD
    commandBuffer[dwIndex + 1] = (uint32_t)(alignedAddress >> 32);         // High DWORD
}
```
ผลลัพธ์นี้ช่วยแก้ไขปัญหาภาพเป็นพิษ (Poison Frame) ส่งผลให้ท่อส่งประมวลผลสามารถดันงานภาพระดับความละเอียดสูง (เช่น **8K HDR IMAX** บน YouTube) วิ่งตรงเข้าชิปการ์ดจอได้ทันที โดยที่กระบวนการระบบส่วนกลางอย่าง `WindowServer` ไม่เกิดอาการโหลดล้นหลอด

---

## 🏗️ Build & Installation

### Prerequisite
การคอมไพล์ซอร์สโค้ดจำเป็นต้องใช้เครื่องมือ **Xcode Command Line Tools** ควบคู่กับชุดพัฒนา **MacKernelSDK** (จัดวางไว้ที่โฟลเดอร์ `~/MacKernelSDK` หรือ `/opt/MacKernelSDK`)

### Compiling
```bash
git clone https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
cd MyIntelGPU-MacDriver-HonestBridge
make
```

### Deployment
> 🚨 **Disclaimer:** นี่คือโค้ดระดับ Kernel ที่จัดการฮาร์ดแวร์โดยตรง การติดตั้งและรันไฟล์มีความเสี่ยงต่อระบบ ผู้ใช้งานโปรดรับความเสี่ยงด้วยตนเอง

```bash
# ตั้งค่าสิทธิ์ไฟล์ระบบให้ถูกต้อง (Root:Wheel)
sudo chown -R root:wheel MyIntelGPU.kext
sudo chmod -R 755 MyIntelGPU.kext

# คัดลอกเข้าไดเรกทอรีส่วนขยายระบบและสั่งอัปเดตแคช
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo kmutil install --volume-root /
```

---

## 🤖 Credits & Tooling

- **Lead Developer:** [pongpan-bk](https://github.com/pongpan-bk)
- **Core Engineering AI Agent:** [OpenCode AI](https://opencode.ai) — ระบบจัดทำและจัดระเบียบโครงสร้างคำสั่งคิว Execlists, ตัวลงทะเบียน Register พอร์ตควบคุม และท่อส่งสัญญาณ Interrupt ตั้งแต่เริ่มต้นจนจบกระบวนการ (End-to-End Core Synthesis)

---
<div align="center">
  Developed with ❤️ by pongpan-bk · Contact Developer: <a href="https://facebook.com">Facebook</a>
</div>

https://www.facebook.com/je4bs
