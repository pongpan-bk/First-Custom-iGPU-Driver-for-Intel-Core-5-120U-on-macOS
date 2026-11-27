#!/bin/zsh
set -u

REPORT="/Users/ppbk/Desktop/MyIntelGPU_HeavyStress_$(date +%Y%m%d_%H%M%S).txt"
REPO="/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"
DECODE_BIN="$REPO/build/myintel-decode/myintel-decode"
FFMPEG_BIN="/Users/ppbk/Documents/untitled folder/src/VCSMediaHarness/ffmpeg"
BCS_BIN="$REPO/build/bcs_stress"
WORK_DIR="/tmp/myintel_heavy_stress_$$"

mkdir -p "$WORK_DIR"
trap 'rm -rf "$WORK_DIR"' EXIT

log_line() {
  print -r -- "$1" | tee -a "$REPORT"
}

section() {
  log_line ""
  log_line "======================================================================"
  log_line "  $1"
  log_line "======================================================================"
}

: > "$REPORT"
log_line "MyIntelGPU Heavy Stress Test Suite"
log_line "Timestamp : $(date '+%Y-%m-%d %H:%M:%S %z')"
log_line "macOS     : $(sw_vers -productVersion) ($(sw_vers -buildVersion)) / $(uname -m)"

section "1. KEXT, IOREGISTRY & BUNDLE CONTRACT CHECK"
kextstat | grep -E "MyIntelGPU|AppleIntel|IOAccelerator" | tee -a "$REPORT" || true
ioreg -r -c MyIntelGPU -l | grep -E "MyIntelGPU|RingInitialized|RCS-Status|IntelMediaStatus|IOVARendererID|MetalPlugin|IOMTLBundle|IOPCIMatch" | tee -a "$REPORT" || true
ioreg -r -c MyIntelAccelerator -l | grep -E "MyIntelAccelerator|VDBOX|VEBOX|IntelMediaCapable|IOAccelRevision|IOVARendererID" | tee -a "$REPORT" || true

for p in \
  "/Library/Extensions/MyIntelGPU.kext" \
  "/Library/GPUBundles/MyIntelGPUMTLDriver.bundle" \
  "/System/Library/Extensions/AppleIntelICLLPGraphicsVAME.bundle" \
  "/System/Library/Extensions/AppleIntelICLGraphicsVADriver.bundle" \
  "/System/Library/Extensions/AppleIntelICLGraphicsGLDriver.bundle"
do
  if [[ -e "$p" ]]; then
    ls -ld "$p" | tee -a "$REPORT"
    codesign -dv --verbose=2 "$p" 2>&1 | head -n 8 | tee -a "$REPORT"
  else
    log_line "[MISSING] $p"
  fi
done

section "2. METAL DEVICE & WINDOWSERVER GPU MAPPING PROBE"
cat <<'SWIFT' > "$WORK_DIR/mtl_probe.swift"
import Foundation
import Metal

let devices = MTLCopyAllDevices()
let defDev = MTLCreateSystemDefaultDevice()
print("MTLCopyAllDevicesCount=\(devices.count)")
for (idx, d) in devices.enumerated() {
    print("  [\(idx)] name=\(d.name) registryID=\(d.registryID) lowPower=\(d.isLowPower) headless=\(d.isHeadless) removable=\(d.isRemovable)")
}
print("MTLSystemDefaultDevice=\(defDev?.name ?? "nil")")
SWIFT
swiftc -parse-as-library "$WORK_DIR/mtl_probe.swift" -o "$WORK_DIR/mtl_probe" 2>>"$REPORT" && "$WORK_DIR/mtl_probe" | tee -a "$REPORT" || log_line "[WARN] Metal probe failed or returned 0 devices"

WS_PID=$(pgrep -x WindowServer || true)
log_line "WindowServer PID: ${WS_PID:-none}"
if [[ -n "$WS_PID" ]]; then
  vmmap "$WS_PID" 2>/dev/null | grep -E "Metal|IOAccelerator|Intel|MyIntel|GPUBundle" | head -n 30 | tee -a "$REPORT" || true
fi

section "3. USERCLIENT OPEN/CLOSE CHURN STRESS (200 CYCLES)"
cat <<'OBJC' > "$WORK_DIR/uc_churn.m"
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <mach/mach_time.h>

static double elapsed_ms(uint64_t t0, uint64_t t1) {
    static mach_timebase_info_data_t info = {0, 0};
    if (info.denom == 0) mach_timebase_info(&info);
    return (double)(t1 - t0) * (double)info.numer / (double)info.denom / 1e6;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        int iterations = (argc > 1) ? atoi(argv[1]) : 200;
        const char *services[] = {"MyIntelGPU", "MyIntelAccelerator", "MyIntelGPUVCS"};
        for (int s = 0; s < 3; s++) {
            int ok = 0, fail = 0;
            uint64_t t0 = mach_absolute_time();
            for (int i = 0; i < iterations; i++) {
                io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(services[s]));
                if (!svc) { fail++; continue; }
                io_connect_t conn = IO_OBJECT_NULL;
                kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
                if (kr != KERN_SUCCESS) {
                    kr = IOServiceOpen(svc, mach_task_self(), 5, &conn);
                }
                if (kr == KERN_SUCCESS && conn != IO_OBJECT_NULL) {
                    IOServiceClose(conn);
                    ok++;
                } else {
                    fail++;
                }
                IOObjectRelease(svc);
            }
            uint64_t t1 = mach_absolute_time();
            printf("Service=%-18s ok=%d fail=%d total_ms=%.2f avg_ms=%.4f\n",
                   services[s], ok, fail, elapsed_ms(t0, t1), elapsed_ms(t0, t1) / iterations);
        }
    }
    return 0;
}
OBJC
clang -fobjc-arc "$WORK_DIR/uc_churn.m" -framework Foundation -framework IOKit -o "$WORK_DIR/uc_churn" 2>>"$REPORT" && "$WORK_DIR/uc_churn" 200 | tee -a "$REPORT" || log_line "[WARN] uc_churn build/run failed"

section "4. VCS / MYINTEL-DECODE MULTI-RESOLUTION & SUSTAINED BURST"
if [[ -x "$DECODE_BIN" ]]; then
  if [[ -x "$FFMPEG_BIN" ]]; then
    "$FFMPEG_BIN" -y -f lavfi -i testsrc2=size=1920x1080:rate=30 -t 2 -c:v libx264 -pix_fmt yuv420p "$WORK_DIR/sample_1080p.mp4" >/dev/null 2>&1 || true
    "$FFMPEG_BIN" -y -f lavfi -i testsrc2=size=1280x720:rate=30 -t 2 -c:v libx264 -pix_fmt yuv420p "$WORK_DIR/sample_720p.mp4" >/dev/null 2>&1 || true
  fi
  for sample in "$WORK_DIR/sample_720p.mp4" "$WORK_DIR/sample_1080p.mp4"; do
    if [[ -f "$sample" ]]; then
      log_line "--- Testing sample: $sample ---"
      for mode_flag in "" "--detile-wide"; do
        t_start=$(python3 -c 'import time; print(time.perf_counter())')
        rc=0
        for run_idx in {1..15}; do
          if [[ -n "$mode_flag" ]]; then
            "$DECODE_BIN" "$mode_flag" -i "$sample" -o "$WORK_DIR/out_${run_idx}.yuv" >/dev/null 2>&1 || { rc=$?; break; }
          else
            "$DECODE_BIN" -i "$sample" -o "$WORK_DIR/out_${run_idx}.yuv" >/dev/null 2>&1 || { rc=$?; break; }
          fi
          rm -f "$WORK_DIR/out_${run_idx}.yuv"
        done
        t_end=$(python3 -c 'import time; print(time.perf_counter())')
        elapsed=$(python3 -c "print(f'{($t_end - $t_start):.3f}')")
        log_line "mode=${mode_flag:-default} 15x_runs rc=$rc elapsed=${elapsed}s"
      done
    fi
  done
else
  log_line "[SKIP] myintel-decode not found at $DECODE_BIN"
fi

section "5. BCS BLITTER STRESS (IF AVAILABLE)"
if [[ -x "$BCS_BIN" ]]; then
  "$BCS_BIN" 500 2>&1 | tee -a "$REPORT" || log_line "[WARN] bcs_stress exited non-zero"
else
  log_line "[SKIP] bcs_stress binary not found at $BCS_BIN"
fi

section "6. SANDBOX / MACF / KERNEL ERROR & CRASH REPORT AUDIT (LAST 30m)"
log show --last 30m --predicate 'process == "kernel" AND (composedMessage CONTAINS[c] "MyIntelGPU" OR composedMessage CONTAINS[c] "MyIntelGPUClient" OR composedMessage CONTAINS[c] "Sandbox" OR composedMessage CONTAINS[c] "Fence")' 2>/dev/null | tail -n 80 | tee -a "$REPORT" || true

log_line ""
log_line "--- Recent Panic / WindowServer / Spin / Hang Reports ---"
ls -lt /Library/Logs/DiagnosticReports 2>/dev/null | grep -E "Kernel|WindowServer|VTDecoder|mediaanalysis" | head -n 15 | tee -a "$REPORT" || true

section "DONE"
log_line "Full report saved to: $REPORT"