# BUNDLE โยนจาก build-session (2026-09-15 ~19:05) — เอาไปต่อ Todo #6/#7 ได้เลย

## เป้า + เกณฑ์ผ่าน
- BCS0 [skipped] → [active] (หาย queued-never-dispatched) แล้ว WS < 10%
- วัด WS ตอน idle หลัง reboot เท่านั้น (ภาพ 18:56 = 18.7% แต่ opencode กิน 113% ตอนนั้น — ตัวเลขเพี้ยน)

## สถานะที่syncกันแล้ว
- #1 defines PSMI/FW (Ring.hpp:86-96) ✅ / #2 decl 3 ฟังก์ชัน (hpp ~1119) ✅ / #3-5 impl (cpp 2258-2365) ✅
- #6 hook ใน accelBatchBlit หลัง accelBCSReset (~2426) ก่อน ringEmitBatchStart (~2429) — เรียกแค่ applyBLTDummyCtxWA ตัวเดียว (มัน nested อีก 2 ตัวแล้ว)
- ส่ง `fPCIDevice` (มี member อยู่แล้ว, MyIntelGPU.hpp:1021) ไม่ต้องหา provider ใหม่

## ห้ามสลับกลับ (verify แล้ว)
- RESET_CTL: bit0=REQUEST, bit1=READY (blueprint กลับด้าน)
- EXECLIST_STATUS = base+0x234 (0x22048 = SYNC_2)
- API จริง: readReg32/writeReg32/IODebug (ไม่มี mmio_read32/I915_WRITE32/DBGLOG)
- RING_TAIL = byte offset ใน ring ไม่ใช่ GGTT addr

## ถ้า build พัง `IOService.h not found`
ลอก Makefile จาก `/Users/ppbk/Documents/GitHub/igpu-silicon-reviver-main` (dual-layout: เจอ Headers/IOKit/IOService.h = MKS mode + libkmod.a, ไม่งั้น fallback xcrun; รองรับ KERNEL_SDK_DIR)

## เช็กลิสต์ก่อน reboot (กัน hang แบบรอบ thread-variant)
1. ทุก poll loop (ACK 0xAC, PSMI bit16, READY bit1) ต้องมี timeout — ห้าม while เปล่า
2. masked-register (0xD0) เขียนแบบ mask+data (bit31:16=mask) ห้าม raw write
3. ตรวจ GuC ownership ก่อนสรุปว่า WA ล้มเหลว (GUC_STATUS 0xC000 + WOPCM lock — ถ้า GuC ครอง engines, ELSP จะถูก ignore)
4. Oracle ล้มเหลวแยก 2 เคส: HEAD ไม่ขยับเลย = engine dead / HEAD ขยับแล้ว fault (ESR / RING_FAULT 0xCEC4 VALID bit) = submission ผิด

## ลำดับปิดงาน
hook → make → lsp_diagnostics → reboot → ดู translation log + EXS bit0 + head advance → WS idle → ถ้า BCS ยังตาย กลับไปเช็ก HEAD SYNC จาก MMIO ก่อน emit (final key ของ v2.0.398 ที่เคย LIVE @60Hz)

รายละเอียดเต็ม: MASTER-KNOWLEDGE.md §14
