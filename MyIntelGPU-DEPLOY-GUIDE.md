# 🛠️ MyIntelGPU.kext — คู่มือใช้งานแบบเป็นระเบียบ (สำหรับ Mac)

> Synced 2026-10-10 from `/Users/ppbk/Desktop/MyIntelGPU-DEPLOY-GUIDE.md` (2026-10-06 revision).
> Path note: คำสั่ง build/deploy ด้านล่างอ้าง checkout `IntelReviveGPU-Gen-10-12-on-Hackintosh` —
> ใน checkout นี้ให้แทนด้วย `/Users/ppbk/Documents/GitHub/First-Custom-iGPU-Driver-for-Intel-Core-5-120U-on-macOS`.
>
> สร้าง 2026-08-07 | อัปเดต 2026-08-09: แก้ขั้น kextutil + การตรวจ auxKC (เจอจริงแล้วว่า test-load จาก source dir ทำให้ kext หลุดจาก boot!)
> อัปเดต 2026-10-06: แก้ path ที่ไม่มีจริง (`Default Project/source`) + เพิ่มข้อตรวจ `OSBundleLibraries`
> ⚠️ ตำแหน่งใช้งานจริง: `/Library/Extensions/MyIntelGPU.kext` — **ไม่ใช้ผ่าน OpenCore**

---

## 0.0 📍 Path จริง (อัปเดต 2026-10-06)

คู่มือเดิมชี้ไปที่ path ที่ไม่มีอยู่แล้ว ใช้ค่าเหล่านี้แทน:

| | ค่าเก่า (ไม่มีจริง) | ค่าจริง |
|---|---|---|
| **repo / source** | /Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh 

| **build entry** | `make` | `make BUILD_NUMBER=<N> KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK` |
| **SDK** | — | `/Users/ppbk/MacKernelSDK` (หรือ `/opt/MacKernelSDK` ทั้งคู่มีอยู่) |
| **บิ้ว + deploy ครบ** | — | `บิ้วMyIntelGPU-AllInOne.command` (อยู่ใน repo root) |
| **version ที่ใช้** | — | `3.1.26` (ดูหัวข้อ 2.1) |

> ⚠️ `make` ตรงๆ จะ stamp `CFBundleVersion = <major>.<minor>.<git commit count>`
> ไม่ใช่เวอร์ชันที่ติดตั้งอยู่ ทำให้เลขย้อน/เดินหน้าเอง — ใช้สคริปต์ หรือส่ง `BUILD_NUMBER` เอง

---

## 2.1 ⚠️ `OSBundleLibraries` ต้องตรงกับ macOS (สำคัญมาก)

ค่า `com.apple.kpi.*` ใน `Info.plist` คือ **ABI ของ macOS** ไม่ใช่ Darwin kernel build
`uname -r` บน Sonoma คือ `23.6.0` แต่ kext ต้องประกาศ `22.6.0` (14.x) — **ไม่ใช่ `23.x`**

เครื่องนี้ (macOS 14.8.9 / kernel 23.6.0) ใช้ค่านี้:

```xml
<key>com.apple.kpi.bsd</key>       <string>22.6.0</string>
<key>com.apple.kpi.iokit</key>     <string>22.6.0</string>
<key>com.apple.kpi.libkern</key>   <string>22.6.0</string>
<key>com.apple.iokit.IOGraphicsFamily</key> <string>597</string>
<key>com.apple.iokit.IOPCIFamily</key>      <string>2.9</string>
```

**ทำไมต้องระวัง:** ถ้าค่า kpi ไม่ตรงกับ OS kext จะ **ไม่ match และไม่โหลด** โดยไม่มี error
หน้าจอจะขาว/ค้างหลัง reboot ทั้งที่ `sudo cp` สำเร็จและ `kmutil install` ไม่เตือนอะไรเลย

- `kpi.* = 10.0.0` → ไม่ load (เคยเจอกรณีนี้จริง แก้ Info.plist ทั้งไฟล์ทิ้ง)
- `IOGraphicsFamily = 500.0` → ต่ำกว่า 597 ที่ driver ต้องใช้
- ตรวจก่อน deploy:
  ```bash
  /usr/libexec/PlistBuddy -c "Print :OSBundleLibraries" Info.plist
  ```
- `บิ้วMyIntelGPU-AllInOne.command` มี gate นี้อยู่แล้ว ถ้าค่าใน bundle ต่างจากตัวที่ติดตั้งอยู่
  มันจะหยุดก่อน deploy

---

## 2.2 ⚠️ `Info.plist` ต้องผ่าน `plutil -lint` (เคยพังจริง)

ต้องแก้ XML ให้ถูกก่อน build — `&` ดิบทำให้ทั้งไฟล์ parse ไม่ได้:

```xml
❌ <string>0x03000000&0xFFFF0000</string>
✅ <string>0x03000000&amp;0xFFFF0000</string>
```

ตรวจก่อน build:
```bash
plutil -lint Info.plist     # ต้องได้ "OK"
```

---

## ⛔ 0.1 กฎเหล็ก (IRON RULES — เจอมาแล้วจริง ห้ามละเมิดเด็ดขาด)

> อัปเดต 2026-08-16: สรุปบทเรียนจากความเสียหายหลายรอบ — เขียนเป็นกฎที่ทุกคนต้องยึดก่อนแตะโปรเจกต์นี้

1. **MyIntelGPU.kext ถูกสร้างมาเพื่อ standalone ล้วน — ห้ามฝากงานกับ OpenCore เด็ดขาด**
   - โหลดจาก `/Library/Extensions` (L/E) เท่านั้น ตามคู่มือข้อ 3
   - **ห้าม OC injection** (Kernel:Add / OC/Kexts) — ห้าม copy kext ลง EFI
   - **ห้าม DeviceProperties.Add ที่ GPU node** (PciRoot(0x0)/Pci(0x2,0x0)) — ห้ามสปูฟ device-id / ig-platform-id / framebuffer con0-2 ไปทับ kext
   - เหตุผล: kext match ตาม PCI class `0x03000000` ครอบฮาร์ดแวร์จริงได้เอง → OC spoof = ค่าหลอกที่ขัดกับ kext และไม่มีวันสำเร็จ

2. **ทุกครั้งที่แก้ boot-args ต้องตรวจตัวสะกดให้ละเอียด** — เคยพิมพ์ `yintelfb=1 + myintelgtirq=1` (ตัว `y` + `+`) ทำให้ kext อ่านค่าเพี้ยน → เข้าใจผิดว่า boot ค้าง ทั้งที่จริงแค่ args ผิด
   - เช็คเสมอ: `myintelfb` (ไม่ใช่ `yintelfb`), ห้ามมี `+` คั่นใน boot-args

3. **ห้าม reboot ระหว่างทางถ้ายังไม่ deploy ไป L/E** (กฎเดิมข้อ 0) — test-load จาก source dir แล้ว reboot = auxKC ชี้ path ผิด → kext ไม่โหลด

4. **deploy ทุกครั้งต้องทำ 5 ขั้นครบ**: cp → chown root:wheel → chmod 755 → `kmutil install --volume-root /` → ตรวจ auxKC ชี้ L/E (ข้อ 3)

5. **เช็คข้อเท็จจริงจาก log ก่อนสรุปว่าบูตค้าง** — "จอค้าง" ไม่ได้แปลว่า kext ตายเสมอ ดู `log show` / NVRAM marker / shutdown report ก่อนด่วนสรุป

6. **ห้ามแตะ backup EFI** (`/Volumes/EFI 1`) เด็ดขาด — เป็นเส้นทางกู้คืน (user สั่ง)

7. **แก้ config ทุกครั้งต้อง backup + ตรวจ plutil** — `config.plist.bak-<timestamp>-<อะไรที่แก้>`

8. **หลักการอ้างอิง**: ทำงานแบบเดียวกับ `Wireless USB Big Sur Adapter.app` (install driver ลง L/E + rebuild cache ตรงๆ) — ง่ายกว่า OC มาก เพราะของเรามาไกลกว่าแค่เขียนค่าหลอก

9. **ห้ามรัน `make` เป็น root** — artifact จะเป็นของ root แล้ว `make clean` ครั้งถัดไปพังด้วย Permission denied
   - ซ่อม: `sudo chown -R $(whoami):staff MyIntelGPU.kext *.o MyIntelGPUVersion.h`

10. **`Info.plist` ต้องตรงกับ macOS** — `OSBundleLibraries` ผิด = kext ไม่ load เงียบ ๆ (ข้อ 2.1)
    - เคยเจอจริง: `kpi.* = 10.0.0` บน macOS 14 → ไม่ match → จอขาวหลัง reboot โดยไม่มี error
    - แก้ไฟล์ plist ทุกครั้งต้อง `plutil -lint` (ข้อ 2.2)

---

## 0. ภาพรวม Flow (ทำตามลำดับนี้เสมอ)

```
[แก้โค้ด] → [make build] → [deploy L/E + kmutil install] → [reboot + Allow] → [verify] → [ถ้าตาย: rollback]
```

> ⚠️⚠️ **กฎเหล็ก (เจอมาแล้วจริง): ห้าม reboot ระหว่างทางถ้ายังไม่ได้ deploy ไป L/E**
> ถ้า `kextutil` (test load) ได้รับ approval แล้วคุณ reboot โดย kext ยังอยู่ที่ source dir
> (`/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh/...`) → kernelmanagerd จะ rebuild auxKC
> **ชี้ไปที่ source path (unstaged, อยู่ใน Data volume) ซึ่ง boot activate ไม่ได้**
> → หลัง reboot kext จะไม่โหลดเลย (kextstat ว่าง) + GPU ไม่มี driver → จอขาว/UI ค้าง
> วิธีแก้คืน: ดูหัวข้อ 5.3

---

## 1. Build (ทุกครั้งหลังแก้โค้ด)

```bash
cd /Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh

# ⚠️ ห้ามรัน make เป็น root — artifact จะเป็นของ root แล้ว make clean ครั้งถัดไป
#    จะพังด้วย Permission denied — ซ่อมด้วย: sudo chown -R $(whoami):staff MyIntelGPU.kext *.o MyIntelGPUVersion.h
#
# ⚠️ BUILD_NUMBER ต้องระบุเอง — Makefile:60 default = git commit count
#    ทำให้เลขเวอร์ชันเดินเอง/ย้อนเทียบกับตัวที่ติดตั้ง
BN=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" \
       /Library/Extensions/MyIntelGPU.kext/Contents/Info.plist | awk -F. '{print $3+1}')

plutil -lint Info.plist      # ต้องได้ OK ก่อน build (ข้อ 2.2)
make BUILD_NUMBER="$BN" KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make BUILD_NUMBER="$BN" KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK -j$(sysctl -n hw.ncpu)
codesign -f -s - --timestamp=none MyIntelGPU.kext   # unsigned bundle โหลดไม่ได้

# ถ้า error แก้ก่อน อย่า deploy ต่อ
```

หรือใช้สคริปต์ที่ทำ build + deploy + sign + KextPolicy + kmutil ครบในขั้นเดียว:

```bash
./บิ้วMyIntelGPU-AllInOne.command          # ต่อเลขเองจากตัวที่ติดตั้ง
./บิ้วMyIntelGPU-AllInOne.command 120      # หรือกำหนดเอง
```

> `Makefile.standalone` (อยู่ที่ `/Users/ppbk/Desktop/2026-09-16_0d12ef9/`) เป็นอีกทางที่ให้
> `CFBundleVersion` ตามที่ `Info.plist` เขียนไว้ตรงๆ (ไม่ stamp) ใช้ตอนต้องได้เลข `3.1.26` พอดี

---

## 2. ทดสอบความถูกต้องของ kext (ตรวจ syntax/plist/สิทธิ์ — แต่ห้าม reboot ตามหลัง!)

```bash
# ตรวจว่า kext ถูกต้อง (plist, binary, ownership) — ใช้ kextutil ในโหมด check เท่านั้น:
sudo kextutil -v MyIntelGPU.kext

# ⚠️ ถ้าขึ้น "loaded" = มันโหลดเข้าเคอร์เนลจริง (ใช้ตรวจโค้ดได้)
# ⚠️ ถ้าขึ้น "requires a reboot" / "not approved" = kext ยังไม่ได้รับ Allow
#    → ทำตามข้อ 3 (deploy L/E + kmutil install) ให้เสร็จ ก่อน reboot เท่านั้น!
#    อย่าคิดว่า "reboot แล้วมันจะโหลดให้เอง" — มันจะ stage auxKC ผิด path แทน

# ⚠️⚠️ หลัง kextutil test load: ถ้าจะ reboot โดยยังไม่ deploy จริง
#     ต้องล้าง staged state ก่อน (ไม่งั้น auxKC จะชี้ไป source path):
sudo kmutil clear-staging            # ล้าง staged collection กลับเป็นสภาพเดิม
# แล้วค่อยทำข้อ 3 → reboot

# ดู log ว่าตรวจเจอ GPU และ attach สำเร็จไหม:
sudo dmesg | grep -i "MyIntelGPU" | tail -50

# ถอดออกถ้าอยากเลิกทดสอบ:
sudo kextunload -b com.pongpan-bk.MyIntelGPU      # ⚠️ bundle id ต้องเป็น pongpan-bk ไม่ใช่ com.yourname
```

---

## 3. Deploy จริงที่ L/E (ตัวที่ระบบจะโหลดตอน boot — ทำก่อน reboot เสมอ!)

```bash
# 3.1 สำรองตัวปัจจุบันก่อน (กันพลาด):
sudo cp -R /Library/Extensions/MyIntelGPU.kext /Library/Extensions/MyIntelGPU.kext.bak-$(date +%Y%m%d)

# 3.2 คัดลอกตัวใหม่เข้าไป (เปลี่ยน path ตาม source ของจริง):
sudo cp -R "/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh/MyIntelGPU.kext" /Library/Extensions/MyIntelGPU.kext

# 3.3 ตั้งสิทธิ์ (ต้องทำทุกครั้ง!):
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext

# 3.4 rebuild kext cache — ⚠️ ห้ามใช้ kextcache -i / (ล้มเหลวเงียบ ไม่ rebuild จริง!)
#     ใช้คำสั่งนี้เท่านั้น (พิสูจน์แล้วว่าสำเร็จ):
sudo kmutil install --volume-root /

# 3.5 ⚠️ ตรวจว่า auxKC ชี้ไป L/E จริง (ไม่ใช่ source path!) ก่อน reboot:
ls -la /Library/KernelCollections/AuxiliaryKernelExtensions.kc
strings /Library/KernelCollections/AuxiliaryKernelExtensions.kc | grep "MyIntelGPU"
# ✅ ต้องเห็น: /Library/Extensions/MyIntelGPU.kext
# ❌ ถ้าเห็น /Users/ppbk/Documents/... = ยัง stage ผิด path → ทำ 3.1-3.4 ใหม่
```

---

## 4. Reboot + Allow + Verify

```bash
# 4.1 reboot
sudo reboot

# 4.2 ตอนบูต: กด Allow เมื่อมี hash prompt ขึ้น (ครั้งเดียวต่อ kext ใหม่)
#     ⚠️ ถ้า hash ต่างจากที่คาด = kext ไม่ตรงกับที่วางไว้ — อย่ากด Allow

# 4.3 หลังเข้า Desktop ตรวจผล:
ioreg -l | grep -E "VRAM"                     # ดู VRAM,memSize / VRAM,totalMB
system_profiler SPDisplaysDataType            # ดู VRAM + Accelerator
kmutil showloaded | grep -i "pongpan-bk"      # ดูว่า kext โหลดจริง
sudo dmesg | grep -i "MyIntelGPU" | tail -50  # log ของ kext
```

---

## 5. 🚨 Rollback — บูตไม่ติด / panic / kext ไม่โหลด

### ถ้าเข้าระบบได้ (GUI ขึ้น หรือ SSH ได้):
```bash
# กู้กลับเป็น 3GB safe (ตัวที่พิสูจน์แล้วว่า GUI ทำงาน — caa54b76):
sudo cp -Rs "/Volumes/DATA/rollback-3gb/MyIntelGPU.kext" /Library/Extensions/MyIntelGPU.kext
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo kmutil install --volume-root /
sudo reboot
```

### ⚠️ 5.2 kext ไม่โหลดหลัง reboot / kextstat ว่าง / จอขาว (auxKC stage ผิด path):

> อาการ: หลัง reboot แล้ว `kmutil showloaded | grep pongpan` ว่าง + GUI ขาว/ค้าง
> สาเหตุ: kernelmanagerd จำ "path ที่เคยโหลด kext จาก" ไว้ในตาราง
> `kext_load_history_v3` (ไฟล์ `/var/db/SystemPolicyConfiguration/KextPolicy`) —
> ถ้าเคย test-load จาก source dir (kextutil) record จะ pin ไว้ที่ source path →
> kernelmanagerd เห็น "already approved" + ไม่ re-scan L/E → auxKC ชี้ path ผิด

```bash
# 1) ลบ record ประวัติที่ผูก path เก่า (สำคัญที่สุด — ไม่งั้น reboot กี่รอบก็ไม่เปลี่ยน):
sudo sqlite3 /var/db/SystemPolicyConfiguration/KextPolicy \
  "DELETE FROM kext_load_history_v3 WHERE bundle_id LIKE '%pongpan%';"

# 2) วาง kext ลง L/E ให้ถูกต้อง (ถ้ายังไม่มี):
sudo cp -R "/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh/MyIntelGPU.kext" /Library/Extensions/MyIntelGPU.kext
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext

# 3) ล้าง auxKC เก่า + state + staging (ให้ kernelmanagerd สแกน L/E ใหม่ทั้งหมด):
sudo rm -f /Library/KernelCollections/AuxiliaryKernelExtensions.kc
sudo rm -rf /var/db/KernelExtensionManagement/AuxKC/
sudo kmutil clear-staging          # ใช้ตัวนี้ — `kmutil reset` ไม่มีในเวอร์ชันนี้!

# 4) rebuild auxKC (ถ้าไม่ขึ้นไฟล์ที่ L/E ไม่เป็นไร — reboot จะ build ให้เอง):
sudo kmutil install --volume-root /

# 5) reboot — kernelmanagerd จะ build auxKC จาก L/E ล้วน แล้วโหลด kext:
sudo reboot

# 6) ตรวจหลัง boot (ต้องเห็น /Library/Extensions/... และ kext โหลด):
strings /Library/KernelCollections/AuxiliaryKernelExtensions.kc | grep "MyIntelGPU"
kextstat | grep pongpan
```

### ถ้าบูตค้าง/panic (เข้า macOS ไม่ได้):
- บูต Recovery (OpenCore → เลือก Recovery) → Terminal:
```bash
# mount ระบบ (หา disk ของ macOS เช่น disk3s1 — ใช้ `diskutil list` ดู)
diskutil list
diskutil mount /dev/disk3s1      # เปลี่ยนตามจริง

# กู้ kext กลับเป็นเวอร์ชันที่ใช้ได้:
cp -Rs "/Volumes/MyUSB/rollback-3gb/MyIntelGPU.kext" "/Volumes/Macintosh HD/Library/Extensions/MyIntelGPU.kext"
# หรือลบ kext ทิ้งเลยถ้าอยากกลับไปสภาพเดิม (7MB VRAM แต่บูตได้):
rm -rf "/Volumes/Macintosh HD/Library/Extensions/MyIntelGPU.kext"
```

### ทางเลือกสำรอง (ถ้ามี backup kext เก่า):
```bash
sudo cp -Rs "/Users/pongpan/Desktop/untitled folder 2/L-E-kext-20260801-preReboot4GB/MyIntelGPU.kext.backup-4gb-20260801" /Library/Extensions/MyIntelGPU.kext
```

---

## 6. คำสั่งเสริม

```bash
# เปิด Gatekeeper (รับ app ทุกที่ — บางครั้งจำเป็นสำหรับ installer):
sudo spctl --master-disable

# ล้าง kext cache แบบเต็ม (ถ้า kmutil ติดปัญหา) — ใช้ clear-staging (kmutil reset ไม่มีในเวอร์ชันนี้):
sudo kmutil clear-staging

# ดู kext ทั้งหมดที่โหลด:
kmutil showloaded
```

---

## 7. ⚠️ เรื่อง "build ล่าสุด (fwfix/BAR2, hash E1DABD2F) จะบูตติดไหม?"

- **ยังไม่เคยถูกทดสอบเลย** — เปลี่ยนกลไก VRAM จาก hardcode 3072MB → อ่านจาก BAR2 โดยตรง
- ความเสี่ยง: อาจ panic ตอน boot → เตรียม rollback (ข้อ 5) ไว้ให้พร้อมก่อนลอง
- **วิธีลดความเสี่ยงที่สุด:** ทดสอบด้วย `kextutil -v` (ข้อ 2) ก่อน — ถ้า attach สำเร็จ โอกาสบูตติดสูงขึ้นมาก
- ถ้าอยากได้ความแน่ใจ 100%: ใช้ **3GB safe (`caa54b76`)** ที่พิสูจน์แล้วว่าทำงาน

---

*คู่มือนี้เป็นไฟล์เดียวกับ WORK_MEMORY_2026-08-07.md ข้อ 7 — ใช้แทนกันได้*
