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

# source-analysis/ — Deep Source Analysis + ผลขุดสมอง OpenCode

> **วันที่วิเคราะห์:** 2026-08-25  
> **วิเคราะห์โดย:** Sisyphus *(surgical structure-scan method — explore agents timeout ทั้ง 2 รอบจึงทำเอง)*

## รายงานวิเคราะห์ซอร์ส (5 ฉบับ)

| ไฟล์ | ครอบคลุม |
| :--- | :--- |
| **01-core-driver.md** | MyIntelGPU class, lifecycle, Phase 0-7 pipeline (mapping i915), BCS tools, boot-args |
| **02-interrupts-power-mmio.md** | Gen11 master IRQ flow, 2.0.229 per-bit selector fix, RC6/forceWake/S3, BAR strategies, register tables |
| **03-ring-execlist-ppgtt.md** | ELSP protocol + descriptor encoding, PPGTT 4-level (PTE 0xC3 bug), MI_FLUSH_DW Gen12 verified form, head-tracking truth |
| **04-vcs-media-decode.md** | VDBOX H.264 pipeline end-to-end, MFX command encoders + QM matrices, DPB rules, userspace contract |
| **05-display-gem-client-build.md** | UserClient selector table 0-24, GEM/PTE defines, Accelerator surfaces, Makefile flags, plist diff, deploy workflow |

---

## ผลขุดสมอง OpenCode จากไดรฟ์ D: (raw APFS carve)

* **วันที่:** 2026-08-25  
* **วิธี:** raw sector read ผ่าน `\\.\PhysicalDrive0` *(ข้าม MacDrive driver ที่พัง)*  
* **พาร์ติชันเป้าหมาย:** P4 offset `402660524032` (~100.91 GiB, GUID `7C3457EF-...` = Apple APFS)

### ทำไมต้อง carve
เนื่องจาก MacDrive MDAPFS filter ไม่ attach volume ทำให้เปิด (open) ไฟล์ตรงๆ ล้มเหลวทั้งหมด บังคับแสดงข้อผิดพลาด ` "A device attached to the system is not functioning"` แม้จะทำการ restart service / reboot / mountvol ก็ไม่หาย จึงจำเป็นต้องเปลี่ยนมาใช้วิธีอ่าน sector ดิบผ่าน disk device แทน

### ไฟล์ในโฟลเดอร์นี้

| ไฟล์ | คืออะไร | คุณค่า |
| :--- | :--- | :--- |
| **conversations_extract.txt** *(12.8MB)* | 13,837 ชิ้นข้อความสนทนาจริงจาก OpenCode sessions บนแมค (text parts + Thai) | ⭐ **สมองตัวจริง** |
| **messages_harvest.txt** *(46MB)* | 58,147 records ดิบจาก leaf pages ของ opencode.db (รวม tool calls, sessionID JSON) | ดิบครบกว่า |
| **sessions_cluster.txt** *(3MB)* | shell history/carved strings โซน storage | คำสั่ง deploy จริง |

> ⚠️ **หมายเหตุ:** `opencode_recovered.db` ถูกลบทิ้งเนื่องจากทำการ dump ตรงจาก offset `488572096512` (119,712 pages) แต่ด้วยกลไก APFS COW ทำหน้าเพจกระจัดกระจาย ส่งผลให้ไฟล์ malformed และใช้งานไม่ได้ จึงต้องหันไปเดโค้ดเลเยอร์ใบไม้ (leaf pages) โดยตรงแทน

---

## วิธีอ่าน conversations_extract.txt

1. แยกบล็อกข้อความด้วยเครื่องหมาย `\n\n@@@@\n\n`
2. รูปแบบบล็อกข้อมูลจะเป็น JSON โครงสร้าง: `{"sessionID":...,"type":"text","text":"..."}` ซึ่งเก็บข้อความโต้ตอบจริงระหว่าง AI และ User
3. ข้อความภาษาไทยถูกเข้ารหัสเป็น **UTF-8** ตามปกติ *(หากเปิดบน Terminal เก่าแล้วแสดงผลเป็น `???` แนะนำให้เปิดด้วย Text Editor ที่รองรับ UTF-8)*

### สคริปต์ที่ใช้ (อยู่ที่ `%TEMP%\opencode\`)
* `rawscan2.py` (signature scan)
* `carve.py` (string carve)
* `final3.py` (SQLite header hunt)
* `harvest.py` (leaf-page record decoder)
