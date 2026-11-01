/*
 * MyIntelBootLog.hpp — deferred boot-path kernel logging
 * ──────────────────────────────────────────────────────────────
 * WHY THIS EXISTS
 *
 * Measured on the 4.1.92 Raptor Lake hackintosh (2026-10-04), three
 * consecutive boots of the *same* binary produced:
 *
 *     boot 14:31   start total =   207 ms
 *     boot 14:34   start total =   196 ms
 *     boot 14:40   start total = 12543 ms
 *
 * The slow boot's gap is 12057 ms wide and it sits between two adjacent
 * GEMBuf log records:
 *
 *     14:40:20.944701  GEMBuf: [GGTT] seq=14 bind ... MAPPED
 *     14:40:33.002...  GEMBuf: gemBufferCreate: OK ...
 *
 * There is no code between those two statements other than a return
 * (MyIntelGEMBuffer.cpp: bind at :124, log at :132). MMIO instrumentation
 * in the same run reports worst single read = 12 us and zero fault
 * records, so the register path is not slow either.
 *
 * Conclusion: the stall is *inside kern_log* — XNU unified-log backpressure
 * while ~9000 system messages (softwareupdated, mobileassetd, launchd)
 * were flooding the log during that window. It is not our compute, and it
 * is not a specific slow register.
 *
 * The consequence that matters: with ~394 IOLog call sites on the boot
 * path (306 via IODebug, 88 bare, plus Ring/GEMBuf records) *any single*
 * record can absorb a multi-second stall, and the probability scales with
 * the number of records. Observed: 1 stall in 3 boots at 394 records.
 *
 * THE FIX
 *
 * Boot-path diagnostics are formatted into a static in-memory ring with
 * zero kernel-log traffic, and flushed once — and only when the developer
 * explicitly asks with -myinteldbg=1. That drops the default boot-path
 * log volume from ~394 records to 5:
 *
 *     1x version banner
 *     3x Ring: SUBMIT[<engine>]  (one aggregate record per engine)
 *     1x TIMING: start total
 *     + fault-only records and final EDID outcome (rare, by design)
 *
 * Diagnostics are not lost: every buffered record carries a monotonic
 * "+Nms" stamp relative to the first boot log, so a flushed trace still
 * reconstructs the phase timeline and exposes slow steps directly.
 *
 * os_log was tried previously and reverted (see MyIntelRing.cpp) because
 * os_log sections broke prelink/auxKC and produced a boot failure. This
 * module therefore uses plain memory + IOLog only for the flush.
 */

#ifndef MyIntelBootLog_hpp
#define MyIntelBootLog_hpp

#include <stdint.h>

/*
 * Append one record to the boot ring. Never touches the kernel log, so it
 * cannot stall. Silently stops recording (and counts the loss) once the
 * ring is full — a truncated trace is strictly better than a stalled boot.
 */
void myBootLogf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Emit straight to the kernel log, bypassing the ring. Reserved for the
 * handful of records that must be visible on a default boot: the version
 * banner, per-engine submit summaries, the TIMING line, faults, and the
 * final EDID injection result.
 */
void myBootLogDirect(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Dump the ring. With -myinteldbg=1 the whole trace is replayed to the
 * kernel log; otherwise a single summary line reports how many records
 * were buffered and how many were dropped. Call once, at the end of
 * start() — that is the only point where a multi-record dump is worth the
 * stall risk, and it only happens in explicit debug mode.
 */
void myBootLogFlush(void);

/* Same replay, ignoring the -myinteldbg=1 gate. Used on the start() failure
 * path: a driver that never reaches Phase 7 has no other way to surface the
 * trace, and it has already failed, so log volume no longer matters. */
void myBootLogFlushForce(void);

/* Drop all buffered records and re-arm the relative clock. Called after a
 * flush so post-start() activity (EDID retry timer, client I/O) cannot
 * scribble over a trace that was already reported. */
void myBootLogReset(void);

#endif /* MyIntelBootLog_hpp */