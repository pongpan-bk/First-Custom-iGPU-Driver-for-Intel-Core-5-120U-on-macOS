/*===========================================================================
 *  myintelvcs_bridge.h
 *  libmyintelvcs.dylib — FFmpeg / Custom Player Bridge (roadmap ข้อ 3)
 *
 *  สร้าง 2026-08-21 — Phase 9
 *
 *  Dynamic Library (.dylib) ส่วนเสริมทำหน้าที่เป็นตัวแปลคำสั่ง (Wrapper)
 *  ระหว่างโปรแกรมเล่นวิดีโอ (FFmpeg hwdec backend / custom player) กับ
 *  MyIntelGPU.kext — ซ่อน IOKit/GEM/MFX command details ไว้ข้างใน:
 *
 *    player → mvcs_decode_h264(annexb chunk) → VDBOX → NV12 frame mapped
 *
 *  Link:  cc player.c -lmyintelvcs -framework IOKit
 *  หรือ dlopen("@rpath/libmyintelvcs.dylib") สำหรับ FFmpeg loader
 *
 *  Author: pongpan-bk | Tooling: Qoder
 *///=========================================================================

#ifndef __MYINTEL_VCS_BRIDGE_H__
#define __MYINTEL_VCS_BRIDGE_H__

#include <stdint.h>
#include <stddef.h>

#include "myintel_vcs_user.h"  /* MYVCS_SURF_FMT_* */

#ifdef __cplusplus
extern "C" {
#endif

extern int mvcs_debug_enabled;

typedef struct mvcs_ctx mvcs_ctx;

/* Decoded frame result (NV12 or P010, mapped read-only-ish into caller address space) */
typedef struct {
    void    *pixels;      /* user VA ของ decoded surface (mapped จาก GEM) */
    size_t   size;        /* mapped size (bytes) */
    uint32_t width;       /* pixels */
    uint32_t height;      /* pixels */
    uint32_t stride;      /* bytes per Y row = width (packed) */
    uint32_t surfFmt;     /* MYVCS_SURF_FMT_NV12 (4) or MYVCS_SURF_FMT_P010 (9) */
    uint32_t bitDepth;    /* 8 or 10 */
    /* private — ใช้กับ mvcs_frame_release() */
    int      _pooled;   /* frame owned by persistent DPB pool */
    uint32_t _srcHandle;
    uint32_t _dstHandle;
    void    *_srcVA;
    size_t   _srcMapSize;
} mvcs_frame_t;

/* ─── Connection lifecycle ────────────────────────────────────── */

/* เปิด connection ไปยัง MyIntelVCS nub; คืน 0 = สำเร็จ */
int  mvcs_open(mvcs_ctx **outCtx);
/* ปิด connection + ปลด GEM buffers ที่ค้างทั้งหมด (kernel ทำให้อัตโนมัติด้วย) */
void mvcs_close(mvcs_ctx *ctx);
/* kern_return_t ของคำสั่งที่ล้มเหลวล่าสุด (debug) */
int  mvcs_last_error(const mvcs_ctx *ctx);

/* ─── Diagnostics ─────────────────────────────────────────────── */

int  mvcs_get_status(mvcs_ctx *ctx, uint64_t *vcsCtl);
int  mvcs_get_context(mvcs_ctx *ctx, uint64_t *mmioBase,
                      uint64_t *ringGgtt, uint64_t *ringSize);

/* ─── GEM Buffer Allocation (roadmap ข้อ 1) ───────────────────── */

/* จองบัฟเฟอร์ GGTT-bound; flags = MYVCS_GEM_SOURCE / MYVCS_GEM_SURFACE */
int  mvcs_gem_create(mvcs_ctx *ctx, uint32_t size, uint32_t flags,
                     uint32_t *outHandle, uint32_t *outGgttOffset,
                     uint32_t *outRealSize);
/* map เข้า user-space (zero-copy DMA pages เดียวกับ GGTT) */
int  mvcs_gem_map(mvcs_ctx *ctx, uint32_t handle, void **outAddr, size_t *outSize);
int  mvcs_gem_unmap(mvcs_ctx *ctx, void *addr, size_t size);
int  mvcs_gem_destroy(mvcs_ctx *ctx, uint32_t handle);

/* ─── Raw command submission (selector 6) ─────────────────────── */

/* ส่ง raw MFX/HCP dwords (ต้อง 16-byte aligned) เข้า VCS ring */
int  mvcs_submit(mvcs_ctx *ctx, const uint32_t *dwords, uint32_t byteCount);

/* ─── High-level decode (roadmap ข้อ 2+3) ─────────────────────── */

/*
 * mvcs_decode_h264 — decode Annex-B chunk 1 เฟรม
 *
 * surfFmt: MYVCS_SURF_FMT_NV12 (4, 8-bit) or MYVCS_SURF_FMT_P010 (9, 10-bit).
 * P010 = 10-bit 4:2:0, doubled surface size (w*h*3 bytes).
 *
 * Return: 0 = สำเร็จ, -1 = argument/pool error, -2 = VDBOX timeout
 *         (frame ใน outFrame จะไม่ถูกแตะเมื่อ return -2)
 * ทำครบทั้ง pipeline ใน call เดียว:
 *   1. จอง Source Buffer + copy bitstream
 *   2. จอง Destination Surface (NV12 w*h*3/2 หรือ P010 w*h*3)
 *   3. สร้าง MFX commands (MFX_PIPE_MODE_SELECT → ... → MI_BATCH_BUFFER_END)
 *   4. Submit ผ่าน selector 6 + รอ VDBOX ทำงาน
 *   5. Map surface กลับมาให้ caller
 *
 * caller ต้อง mvcs_frame_release() เมื่อใช้เฟรมจบ
 */
int  mvcs_decode_h264(mvcs_ctx *ctx,
                      const uint8_t *annexb, uint32_t annexbLen,
                      uint32_t width, uint32_t height,
                      uint32_t surfFmt,
                      mvcs_frame_t *outFrame);

/* ปลด mapping + free GEM buffers ของเฟรม */
void mvcs_frame_release(mvcs_ctx *ctx, mvcs_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* __MYINTEL_VCS_BRIDGE_H__ */
