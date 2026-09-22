# HANDOFF จาก build-fix session (2026-09-15 ~19:00)

## 1. Repo น้องบิลด์ได้แล้ว — ไม่ต้องงม Makefile เอง
`/Users/ppbk/Documents/GitHub/igpu-silicon-reviver-main` → `make` ผ่าน 6/6 ได้ kext x86_64 แล้ว
ถ้า build ฝั่งนี้พังด้วย `IOService.h not found` ให้ลอก `Makefile` ฝั่งนั้น (dual-layout SDK + `KERNEL_SDK_DIR`)

## 2. กับดักที่เจอ (อย่าเหยียบซ้ำ)
- `CFBundleVersion` ต้อง 3 parts (`"1"` เดี่ยวทำให้ stamp เป็น `1..N`)
- CI `macos-13` ตายแล้ว (ค้าง 24 ชม. → cancelled) ใช้ `macos-15-intel` + clone MacKernelSDK ลง `/opt`
- `0x22048` = SYNC_2 ไม่ใช่ EXECLIST_STATUS (ใช้ 0x234) / RESET_CTL bit0=REQUEST bit1=READY — ตามนี้อยู่แล้ว ย้ำกันลืม

## 3. เป้าใหม่จาก user: WS < 10%
ภาพ 18:56 WS=18.7% แต่วัดตอน opencode กิน 113% — **วัด WS ตอน idle หลัง reboot เท่านั้น** (IRON RULE #12)
เส้นทาง = BCS0 [active] → blitter offload (Todo #6 hook + #7 build ของเอ็งต่อได้เลย)

## 4. รายละเอียดเต็ม
`MASTER-KNOWLEDGE.md §14` (เพิ่งยัดไว้) — มี roots, registers, v2.0.398 fix chain, constraints ครบ
