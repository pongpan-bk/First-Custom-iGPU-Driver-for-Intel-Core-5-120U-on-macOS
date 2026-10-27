#!/bin/bash
# ============================================================================
#  06-irq-interrupt.command — MSI-X, GT IRQ, interrupt counters
# ============================================================================
set -uo pipefail

echo "============================================"
echo "  06-irq-interrupt — Interrupt chain"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

# 1. GPU IRQ topology from IORegistry
echo ""
echo "[1] GPU IRQ topology (IORegistry)"
ioreg -l -b -r -c MyIntelGPU 2>/dev/null | grep -E "IOMInterrupt|InterruptControllers|myintelgtirq|MSI" | head -10 | sed 's/^/    /'

# 2. boot-args actual
echo ""
echo "[2] boot-args IRQ flags"
BA=$(nvram boot-args 2>/dev/null | sed 's/^boot-args[[:space:]]*//')
echo "    $BA"
case " $BA " in
  *myintelgtirq*) echo "  [OK] myintelgtirq present" ;;
  *myintelgtirq\=*) echo "  [OK] myintelgtirq= present" ;;
  *) echo "  [!!] myintelgtirq not in boot-args" ;;
esac

# 3. Kernel log IRQ arming
echo ""
echo "[3] IRQ arming (kernel log — last 30m)"
log show --predicate 'sender == "MyIntelGPU"' --last 30m --info 2>/dev/null | grep -iE "ARMED|first-entry|processEngineInterrupt|USER IRQ|GT IRQ|interrupt" | tail -15 | sed 's/^/    /'

# 4. CPU sync & interrupt counters
echo ""
echo "[4] CPU / IRQ counters"
sysctl hw.ncpu hw.activecpu mach_factor hz clockrate 2>/dev/null | sed 's/^/    /'

# 5. Dash-form warning
echo ""
echo "[5] boot-args dash-form check"
case " $BA " in
  *-myintelgtirq*) echo "  [!!] BUG: -myintelgtirq (dash form) will not match PE_parse_boot_argn" ;;
  *) echo "  [OK] No dash-form myintelgtirq" ;;
esac

echo ""
echo "============================================"
echo "  IRQ check complete"
echo "============================================"
