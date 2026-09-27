# source-analysis/ — Deep Source Analysis + ผลขุดสมอง OpenCode

> 2026-08-25 · วิเคราะห์โดย Sisyphus (surgical structure-scan method — explore agents timeout ทั้ง 2 รอบจึงทำเอง)

## รายงานวิเคราะห์ซอร์ส (5 ฉบับ)

| ไฟล์ | ครอบคลุม |
|---|---|
| **01-core-driver.md** | MyIntelGPU class, lifecycle, Phase 0-7 pipeline (mapping i915), BCS tools, boot-args |
| **02-interrupts-power-mmio.md** | Gen11 master IRQ flow, 2.0.229 per-bit selector fix, RC6/forceWake/S3, BAR strategies, register tables |
| **03-ring-execlist-ppgtt.md** | ELSP protocol + descriptor encoding, PPGTT 4-level (PTE 0xC3 bug), MI_FLUSH_DW Gen12 verified form, head-tracking truth |
| **04-vcs-media-decode.md** | VDBOX H.264 pipeline end-to-end, MFX command encoders + QM matrices, DPB rules, userspace contract |
| **05-display-gem-client-build.md** | UserClient selector table 0-24, GEM/PTE defines, Accelerator surfaces, Makefile flags, plist diff, deploy workflow |

## ผลขุดสมอง OpenCode จากไดรฟ์ D: (raw APFS carve)

> วันที่: 2026-08-25 · วิธี: raw sector read ผ่าน `\\.\PhysicalDrive0` (ข้าม MacDrive driver ที่พัง)
> พาร์ติชันเป้าหมาย: P4 offset `402660524032` (~100.91 GiB, GUID `7C3457EF-...` = Apple APFS)

## ทำไมต้อง carve
MacDrive MDAPFS filter ไม่ attach volume → open ไฟล์ตรง fail ทั้งหมด ("A device attached to the system is not functioning")
แม้ restart service/reboot/mountvol ก็ไม่หาย → อ่าน sector ดิบผ่าน disk device แทน

## ไฟล์ในโฟลเดอร์นี้

| ไฟล์ | คืออะไร | คุณค่า |
|---|---|---|
| **conversations_extract.txt** (12.8MB) | 13,837 ชิ้นข้อความสนทนาจริงจาก OpenCode sessions บนแมค (text parts + Thai) | ⭐ สมองตัวจริง |
| **messages_harvest.txt** (46MB) | 58,147 records ดิบจาก leaf pages ของ opencode.db (รวม tool calls, sessionID JSON) | ดิบครบกว่า |
| **sessions_cluster.txt** (3MB) | shell history/carved strings โซน storage | คำสั่ง deploy จริง |

⚠️ `opencode_recovered.db` ถูกลบทิ้ง — dump ตรงจาก offset `488572096512` (119,712 pages) แต่ APFS COW ทำ pages กระจัดกระจาย → malformed ใช้ไม่ได้ จึงหันไป decode leaf pages ตรงๆ แทน

## วิธีอ่าน conversations_extract.txt
- แยกบล็อกด้วย `\n\n@@@@\n\n`
- บล็อกแบบ `{"sessionID":...,"type":"text","text":"..."}` = ข้อความ AI/user จริง
- ภาษาไทยเป็น UTF-8 ปกติ (Terminal เก่าอาจโชว์ ??? — เปิดด้วย editor UTF-8)

## สคริปต์ที่ใช้ (อยู่ที่ `%TEMP%\opencode\`)
rawscan2.py (signature scan) · carve.py (string carve) · final3.py (SQLite header hunt) · harvest.py (leaf-page record decoder)
