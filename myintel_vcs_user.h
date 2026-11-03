/*===========================================================================
 *  myintel_vcs_user.h
 *  User-space ABI for MyIntelVCSClient (IOUserClient)
 *
 *  สร้าง 2026-08-21 — Phase 9: GEM Buffer Allocation + Player Bridge
 *
 *  Single source of truth for the user<->kernel contract:
 *    - externalMethod() selectors
 *    - clientMemoryForType() type encoding for GEM CPU mapping
 *    - GEM buffer flags
 *
 *  Shared by:
 *    - kext       : MyIntelVCSClient.cpp
 *    - test app   : test_submit_vcs.m
 *    - dylib      : myintelvcs_bridge.c (FFmpeg / custom player bridge)
 *
 *  Pure C header — no kernel includes, safe in user-space.
 *///=========================================================================

#ifndef __MYINTEL_VCS_USER_H__
#define __MYINTEL_VCS_USER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ─────────────────────────────────────────────
 *  externalMethod() selectors
 * ─────────────────────────────────────────────
 *
 *  0-6 = Phase 8 (raw command submission)
 *  7-9 = Phase 9 (GEM buffer allocation)
 */
enum {
    kMyVCSSubmitCommandBuffer = 0,  /* in: ggtt, dwords, codec            */
    kMyVCSWaitForCompletion   = 1,  /* in: timeoutMs; wait for VCS0 ring drain */
    kMyVCSGetVCSStatus        = 2,  /* out: VCS_CTL register value        */
    kMyVCSMapOutputBuffer     = 3,  /* in: ggtt, size  out: mapping idx   */
    kMyVCSUnmapOutputBuffer   = 4,  /* in: mapping idx                    */
    kMyVCSGetContextInfo      = 5,  /* out: mmioBase, ringGgtt, ringSize  */
    kMyVCSSubmitVCSWorkload   = 6,  /* structIn: raw MFX/HCP dwords (16B aligned) */

    /* Phase 9 — GEM */
    kMyVCSGemCreate           = 7,  /* in: size, flags out: handle, ggtt, realSize */
    kMyVCSGemDestroy          = 8,  /* in: handle                         */
    kMyVCSGemGetInfo          = 9,  /* in: handle      out: ggtt, size, flags */

    /* 2.0.251 — engine reset/recovery */
    kMyVCSEngineReset         = 10, /* out: esr, ipehr, head, tail, ctl, hwspScratch */

    /* 2.0.252 — read-only engine diagnostics (no reset) */
    kMyVCSEngineDiag          = 11, /* out: esr, ipehr, lrcHead, lrcTail, csbWr, csb0lo,
                                     *      hwspScratch, execStLo, nopid, mmioHead, mmioTail, ctl */

    /* 2.0.260: map a caller-owned virtual range (e.g. an IOSurface's base
     * address) into GGTT + VCS ring PPGTT. The range is resolved inside the
     * CALLING task via IOMemoryDescriptor::withAddressRange(), so a client can
     * only map memory it already owns — never arbitrary physical memory. Pages
     * are bound into GGTT (VDBOX/HCP dwords carry GGTT offsets) and
     * identity-mapped into the ring PPGTT at that same offset.
     * out: ggttOffset (0 = fail). Pair every success with a close() to unbind
     * (the client unbinds all live slots in clientClose()). */
    kMyVCSMapExternalPhys    = 12, /* in: userVA, size | out: ggttOffset */

    /* 2.0.261: Get VideoToolbox decoder configuration */
    kMyVCSGetDecoderConfig   = 13, /* out: decoder config struct for VTRegisterSupplementalVideoDecoderIfAvailable */

    /* 2.0.260: tiled NV12 → linear detile (CPU readback of two GEM handles) */
    kMyVCSDetileOutput       = 14, /* in: srcHandle, dstHandle, width, height, pitch | out: bytesWritten */

    /* 2.0.265: Set HuC firmware GGTT offset for DMA load (must be called before VCS init) */
    kMyVCSSetHuCFirmware     = 15, /* in: handle (GEM), size (bytes) */

    /* Set GuC firmware GGTT offset for DMA load (loads GuC before HuC on Gen12) */
    kMyVCSSetGuCFirmware     = 16, /* in: handle (GEM), size (bytes) */

    /* VCS2 has independent submission, recovery, and diagnostic state. */
    kMyVCSSubmitVCS2Workload = 17, /* structIn: raw MFX/HCP dwords */
    kMyVCSEngineResetVCS2    = 18, /* out: same layout as kMyVCSEngineReset */
    kMyVCSEngineDiagVCS2     = 19, /* out: same layout as kMyVCSEngineDiag */
    kMyVCSGetContextInfoVCS2 = 20, /* out: MMIO base, ring GGTT, size */
    kMyVCSWaitForCompletionVCS2 = 21, /* in: timeoutMs out: LRC ring head */

    /* Phase A: kext-side media session contract (no VideoToolbox glue yet)
     *
     * Begin (22)
     *   in  scalars: [0]=codec (MYVCS_CODEC_*), [1]=width, [2]=height,
     *                [3]=format (MYVCS_SURF_FMT_*)
     *   out scalars: [0]=sessionId (1..MYVCS_SESSION_MAX, 0 = none free)
     *   width/height must be multiples of 16, 16..8192.
     *
     * Submit (23)
     *   in  scalars: [0]=sessionId, [1]=srcHandle, [2]=dstHandle,
     *                [3]=reserved(0)
     *   src/dst handles are informational (GEM handle or any caller cookie);
     *   they are not dereferenced or validated by the kernel.
     *   structureInput: raw MFX/HCP dwords — same rules as selector 6
     *                   (>0 bytes, 16-byte aligned)
     *   out scalars: [0]=fence (monotonic per client connection, starts at 1)
     *   Fence semantics: fence n means "all submissions up to and including n
     *   on this connection are complete". The VCS0 ring is a single ordered
     *   stream, so waiting on any fence <= the latest drains the ring.
     *
     * Wait (24)
     *   in  scalars: [0]=sessionId, [1]=fence (must be <= last fence issued by
     *                this session, else kIOReturnBadArgument),
     *                [2]=timeoutMs (0 = poll once, no wait)
     *   out scalars: [0]=status (MYVCS_SESSION_DONE / MYVCS_SESSION_TIMEOUT),
     *                [1]=lrcHead (LRC image ring head, masked to ring size)
     *
     * End (25)
     *   in  scalars: [0]=sessionId
     *   out: none. Session slot is released; GEM buffers stay caller-owned.
     *
     * All four selectors are VCS0-only. Sessions do not change GEM buffer or
     * submission semantics; they add lifecycle + fencing on top of selector 6.
     */
    kMyVCSMediaSessionBegin  = 22,
    kMyVCSMediaSessionSubmit = 23,
    kMyVCSMediaSessionWait   = 24,
    kMyVCSMediaSessionEnd    = 25,
};

/* Codec identifiers for kMyVCSMediaSessionBegin */
#define MYVCS_CODEC_H264      1
#define MYVCS_CODEC_HEVC      2
/* 3 reserved (VP9) — not accepted until a validated decode path exists */

#define MYVCS_SESSION_MAX     8
#define MYVCS_SESSION_DONE    0
#define MYVCS_SESSION_TIMEOUT 1

/*
 * ─────────────────────────────────────────────
 *  clientMemoryForType() type encoding
 * ─────────────────────────────────────────────
 *
 *  type <  0x100            : legacy output mapping index (mapOutputBuffer)
 *  type >= MYVCS_GEM_MAP_BASE : GEM handle = type - MYVCS_GEM_MAP_BASE
 *
 *  Mapping a GEM handle gives a READ/WRITE user VA over the SAME physical
 *  pages the GGTT PTEs point at (LLC-coherent on Gen12+ → CPU writes are
 *  visible to VDBOX without explicit flush).
 */
#define MYVCS_GEM_MAP_BASE    0x100

/*
 * ─────────────────────────────────────────────
 *  GEM buffer flags (kMyVCSGemCreate scalarInput[1])
 * ─────────────────────────────────────────────
 *
 *  Mirrors GEM_FLAG_* in MyIntelGEMBuffer.hpp. The kext ORs in
 *  CPU+GPU read/write automatically; these hint the intended usage:
 *    MYVCS_GEM_SOURCE    = bitstream buffer  (CPU write, GPU read)
 *    MYVCS_GEM_SURFACE   = decoded surface   (GPU write, CPU read)
 */
#define MYVCS_GEM_SOURCE      (1U << 8)   /* Source Buffer (H.264 bitstream)  */
#define MYVCS_GEM_SURFACE     (1U << 9)   /* Destination Surface (NV12 etc.)  */

/* Max allocation size — must match GEM_MAX_BUFFER_SIZE (256MB) */
#define MYVCS_GEM_MAX_SIZE    (256u * 1024u * 1024u)

/* NV12 surface size helper: Y plane (w*h) + UV plane (w*h/2) */
#define MYVCS_NV12_SIZE(w, h) (((uint32_t)(w) * (uint32_t)(h)) * 3 / 2)

/*
 * P010 surface size helper (10-bit 4:2:0, stored as 16-bit samples):
 *   Y plane   = w * h * 2 bytes
 *   UV plane  = (w/2 * h/2) * 2 (U+V interleaved) * 2 bytes = w * h bytes
 *   Total     = w*h*2 + w*h = w*h*3 bytes  (exactly 2x NV12)
 */
#define MYVCS_P010_SIZE(w, h) (((uint32_t)(w) * (uint32_t)(h)) * 3)

/*
 * MFX/AVC surface format values (Intel Gen12+, MFX_SURFACE_STATE DW3
 * SurfaceFormat field, bits 28-31):
 *   NV12 (8-bit 4:2:0)  = 4  (SURFACE_FORMAT_PLANAR_420_8)
 *   P010 (10-bit 4:2:0) = 9  (SURFACE_FORMAT_P010)
 */
#define MYVCS_SURF_FMT_NV12   4
#define MYVCS_SURF_FMT_P010   9

#ifdef __cplusplus
}
#endif

#endif /* __MYINTEL_VCS_USER_H__ */
