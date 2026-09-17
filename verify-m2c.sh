#!/bin/sh
# M2C post-reboot verification (run after the M2C-K reboot)
echo "═══ 1) kext loaded? ═══"
kmutil showloaded | grep -i "pongpan-bk" || echo "!! MyIntelGPU NOT loaded"
echo
echo "═══ 2) M2C sel=9/sel=2 hits in kernel log (triggered by probe5 below) ═══"
sudo dmesg | grep -iE "M2C|DISC" | tail -12
echo
echo "═══ 3) probe5: does Metal return a default device? ═══"
if [ -x /tmp/probe5 ]; then
    /tmp/probe5 2>&1 | tail -6
else
    echo "(/tmp/probe5 missing — rebuild or skip; bundle's createDevice is the trigger)"
fi
echo
echo "═══ 4) bundle load evidence ═══"
log show --last 3m --predicate 'eventMessage CONTAINS "MyIntelGPUMTLDriver"' 2>/dev/null | tail -5
echo
echo "Result ladder: 2) shows 'M2C sel=2 caps 600B name=...' + 3) shows non-nil device = M2C DONE"