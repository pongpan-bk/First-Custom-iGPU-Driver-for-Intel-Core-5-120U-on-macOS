#!/bin/sh
# ============================================================
#  MyIntelGPU-Check — ตรวจผล IRQ storm fix (2.0.200+)
#  one-click: ดับเบิลคลิกบน Desktop -> Terminal เปิด + ตรวจให้
#  หมายเหตุ: macOS 26 ส่ง kext log ไป unified log —
#            ใช้ `log show` แทน `dmesg` (dmesg จะว่างเปล่า)
# ============================================================

FAIL=0

echo "==========================================================="
echo " MyIntelGPU Check - $(date '+%Y-%m-%d %H:%M:%S')"
echo "==========================================================="

# ---- window ตรวจ: 1 ชม.ล่าสุด หรือตั้งแต่ boot (อันที่สั้นกว่า) ----
# ตัด log boot เก่าออก: ถ้า boot ภายใน 1 ชม. ให้เริ่ม window ที่ boot
BOOT_SEC=$(sysctl -n kern.boottime | awk '{print $4}' | tr -d ',')
NOW_SEC=$(date +%s)
if [ -n "$BOOT_SEC" ] && [ "$BOOT_SEC" -gt $((NOW_SEC - 3600)) ]; then
    START_STR=$(date -r "$BOOT_SEC" '+%Y-%m-%d %H:%M:%S')
    echo "Window: ตั้งแต่ boot ครั้งล่าสุด (เริ่ม $START_STR)"
else
    START_STR=$(date -v-1H '+%Y-%m-%d %H:%M:%S')
    echo "Window: 1 ชม.ล่าสุด (เริ่ม $START_STR)"
fi
echo

# ---- 1. Kext version ----
echo "--- 1. Kext version (คาด 2.0.200) --------------------------"
kextstat | grep -i "pongpan-bk" || echo "   (kext ไม่อยู่ใน kextstat!)"
if kextstat | grep -qi "pongpan-bk.MyIntelGPU (2.0.200)"; then
    echo "   PASS: 2.0.200 โหลดแล้ว"
else
    echo "   FAIL: เวอร์ชันไม่ตรง (คาด 2.0.200)"
    FAIL=1
fi
echo

# ---- 2. Happy path ----
echo "--- 2. ลำดับ one-shot (seed -> IRQ -> chain stops) ---------"
sudo log show --start "$START_STR" --style compact \
    --predicate 'eventMessage CONTAINS "MyIntelGPU" AND (eventMessage CONTAINS "kickCommandSet2" OR eventMessage CONTAINS "processEngineInterrupt" OR eventMessage CONTAINS "ARMED")' 2>/dev/null \
    | grep "MyIntelGPU:" | sed 's/^.*(MyIntelGPU) //' | tail -12
N_STOP=$(sudo log show --start "$START_STR" --style compact --predicate 'processID == 0 AND eventMessage CONTAINS "chain stops"' 2>/dev/null | grep -c "chain stops")
N_SUB=$(sudo log show --start "$START_STR" --style compact --predicate 'processID == 0 AND eventMessage CONTAINS "ELSP kicked"' 2>/dev/null | grep -c "ELSP kicked")
if [ "$N_SUB" -ge 1 ] && [ "$N_STOP" -ge 1 ]; then
    echo "   PASS: วงจรครบ (submitted=$N_SUB, chain_stops=$N_STOP)"
else
    echo "   FAIL: ลำดับไม่ครบ (submitted=$N_SUB, chain_stops=$N_STOP)"
    FAIL=1
fi
echo

# ---- 3. Storm check ----
echo "--- 3. Storm check (คาด 0 ทั้งคู่) --------------------------"
N_FLUSH=$(sudo log show --start "$START_STR" --style compact --predicate 'processID == 0 AND eventMessage CONTAINS "flush emit failed"' 2>/dev/null | grep -c "flush emit failed")
N_16320=$(sudo log show --start "$START_STR" --style compact --predicate 'processID == 0 AND eventMessage CONTAINS "tail=16320"' 2>/dev/null | grep -c "tail=16320")
echo "   flush_emit_failed=$N_FLUSH    tail_16320=$N_16320"
if [ "$N_FLUSH" -eq 0 ] && [ "$N_16320" -eq 0 ]; then
    echo "   PASS: ไม่มี storm"
else
    echo "   FAIL: ยังมี storm (window เริ่มจาก boot ล่าสุดแล้ว)"
    FAIL=1
fi
echo

# ---- 4. IRQ ทั้งหมด ----
echo "--- 4. จำนวน processEngineInterrupt (boot ปกติ ~2-3) -------"
N_IRQ=$(sudo log show --start "$START_STR" --style compact --predicate 'processID == 0 AND eventMessage CONTAINS "processEngineInterrupt"' 2>/dev/null | grep -c "processEngineInterrupt")
echo "   IRQ ทั้งหมดตั้งแต boot: $N_IRQ"
if [ "$N_IRQ" -le 3 ]; then
    echo "   PASS: ถูกจำกัด (boot เก่า = เป็นหมื่น)"
else
    echo "   NOTE: IRQ=$N_IRQ เกินคาด - ดูว่า re-kick มาจากไหน"
fi
echo

# ---- Verdict ----
echo "==========================================================="
if [ "$FAIL" -eq 0 ]; then
    echo " VERDICT: PASS - storm dead, one-shot chain หยุดเอง"
else
    echo " VERDICT: FAIL - ดูรายละเอียดข้างบน"
fi
echo "==========================================================="
echo
echo "(กด Enter เพื่อปิด)"
read _x
