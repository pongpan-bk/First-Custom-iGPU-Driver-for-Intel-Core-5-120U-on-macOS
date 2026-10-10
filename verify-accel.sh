#!/bin/sh
# verify-accel.sh — Milestone 1 post-boot verification (run AFTER reboot)
# Checks: boot-arg pushed, deferral fired, accelerator node live, WS state.
echo "=== [1] boot-args มี myacceldefer? ==="
nvram boot-args
echo
echo "=== [2] boot log: myacceldefer flow ==="
log show --last 2h --predicate 'eventMessage CONTAINS "myacceldefer"' 2>/dev/null \
  | grep -E "ARMED|creating accelerator|LIVE|FAILED" | head -6
echo
echo "=== [3] MyIntelAccelerator instance ใน ioreg ==="
ioreg -rc MyIntelAccelerator 2>/dev/null | grep -cE '"IOClass" = "MyIntelAccelerator"'
ioreg -rc MyIntelAccelerator 2>/dev/null | grep -E '"IOClass"|"CFBundleIdentifier"|attachMode' | head -6
echo
echo "=== [4] WindowServer CPU (nomal < 20%) ==="
ps aux | grep -E "WindowServer" | grep -v grep | awk '{printf "CPU=%%%s  %s\n", $3, $11}'
echo
echo "=== [5] system_profiler: Metal renderer ==="
system_profiler SPDisplaysDataType 2>/dev/null | grep -iE "chipset|metal|renderer" | head -6
echo
echo "=== [6] ฟรีซถึงไหม? แค่ boot ถึงตรงนี้ = ยังไม่ฟรีซ ==="
echo "Done. ถ้า [3] > 0 แปลว่า accelerator เกิดแล้ว — ต่อ Path C ได้"