/*===========================================================================
 *  MyIntelRing.hpp
 *  Hackintosh Kext — Ring Buffer Engine (Phase 5)
 *
 * GPU Command Ring Buffer Gen12+:
 *    - RCS (Render Command Streamer) — 3D/Graphics
 *    - BCS (Blitter Command Streamer) — Memory Copy/Fill
 *
 * ring :
 * - Ring buffer GGTT aperture (CPU = GGTT VA → GPU = GGTT offset)
 *    - MMIO register pair: RING_HEAD, RING_TAIL, RING_START, RING_CTL
 *    - Command emission: ringBegin() → write MI_* commands → ringAdvance()
 *    - Submission: ringSubmit() → write RING_TAIL register
 *
 * Linux i915:
 *    drivers/gpu/drm/i915/gt/intel_ring_submission.c — xcs_resume()
 *    drivers/gpu/drm/i915/gt/intel_ring.h — intel_ring_begin/advance
 *    drivers/gpu/drm/i915/gt/intel_engine_regs.h — RING_TAIL/HEAD/START/CTL
 *///=========================================================================

#ifndef __MY_INTEL_RING_HPP__
#define __MY_INTEL_RING_HPP__

#include <stdint.h>
#include <libkern/libkern.h>
#include "MyIntelGEMBuffer.hpp"   /* GEM_PAGE_SHIFT, GEM_FLAG_* */

/*
 * ─────────────────────────────────────────────
 *  Engine MMIO Base Offsets (from MyIntelGPU.hpp)
 * ─────────────────────────────────────────────
 *
 *  RCS0_BASE = 0x02200  (Render — Gen12+/TGL/ADL/RPL; Gen11 and earlier used 0x02000)
 *  BCS0_BASE = 0x22000  (Blitter — same)
 *  VCS0_BASE = 0x1C0000 (Video — differs from CFL; use translateAddress)
 *  VECS0_BASE = 0x1C8000 (Video Encode — differs)
 *
 *  Register offsets from engine base (Xe / Gen12+):
 *    RING_TAIL    = base + 0x30  (write tail to submit)
 *    RING_HEAD    = base + 0x34  (read/clear head)
 *    RING_START   = base + 0x38  (GGTT address of ring buffer)
 *    RING_CTL     = base + 0x3C  (size + valid bit)
 *    RING_MI_MODE = base + 0x9C  (stop ring bit)
 */

/*
 * Ring register offsets (from engine mmio base)
 *
 * Reference: xe_engine_regs.h / intel_engine_regs.h
 *   RING_TAIL    = base + 0x30   TAIL_ADDR = GENMASK(20,3)  — 8-byte aligned
 *   RING_HEAD    = base + 0x34   HEAD_ADDR = GENMASK(20,2)  — 4-byte aligned
 *   RING_START   = base + 0x38
 *   RING_CTL     = base + 0x3C   RING_CTL_SIZE(size) = (size) - PAGE_SIZE
 *   RING_MI_MODE = base + 0x9C
 */
#define RING_TAIL_REG_OFFSET        0x30
#define RING_HEAD_REG_OFFSET        0x34
#define RING_START_REG_OFFSET       0x38
#define RING_CTL_REG_OFFSET         0x3C
/* Error/fault registers — i915 gt/intel_engine_regs.h (lines 59-61, 81-83,
 * verified against torvalds/linux master 2026-08-10):
 *   RING_IPEIR = base+0x64, RING_IPEHR = base+0x68  (gen8+ instruction pointer
 *   error identity/header — captures the faulting instruction),
 *   RING_EIR = base+0xb0 (error identity), RING_ESR = base+0xb8 (error status) */
#define RING_IPEIR_OFFSET           0x64
#define RING_IPEHR_OFFSET           0x68
#define RING_EIR_OFFSET             0xB0

/* 2.0.255: Gen11+ redefined ALL GRDOM bits except FULL(bit0)/RENDER(bit1) —
 * old GEN6_GRDOM_MEDIA(bit2) == GEN11_GRDOM_BLT here, so pre-2.0.255 resets
 * hit the copy engine, never VDBOX. VCS0 = MEDIA(bit5); i915 resets the
 * paired SFC together with VCS engines -> include SFC0(bit17). */
#define GEN6_GDRST_REG_OFFSET       0x941C
#define GEN11_GRDOM_MEDIA           (1u << 5)
#define GEN11_GRDOM_SFC0            (1u << 17)
#define VCS0_GDRST_DOMAIN           (GEN11_GRDOM_MEDIA | GEN11_GRDOM_SFC0)
#define RING_ESR_OFFSET             0xB8
#define RING_HWS_PGA_OFFSET         0x80    /* Hardware Status Page */
#define RING_HWS_STAM_OFFSET        0x98    /* Interrupt Mask for Status Page */
#define RING_MI_MODE_REG_OFFSET     0x9C
#define RING_INSTPM_OFFSET          0xC0    /* Instruction PM Register (TLB Flush) */
#define RING_PP_DIR_DCLV_OFFSET     0x220   /* PPGTT Directory DCLV */
#define RING_PP_DIR_BASE_OFFSET     0x228   /* PPGTT Directory Base Address */
#define RING_MODE_GEN7_OFFSET       0x29C   /* Gen7+ Ring Mode (PPGTT Enable) */

/* Engine reset handshake register — i915 gt/intel_engine_regs.h RING_RESET_CTL
 * (masked register: bit31:16 = write mask, bit15:0 = data — i915 uses
 * REG_MASKED_FIELD_ENABLE(x) = (x<<16)|x and REG_MASKED_FIELD_DISABLE(x) = (x<<16)|0) */
#define RING_RESET_CTL_OFFSET       0xD0    /* RING_RESET_CTL(base) = base + 0xd0 */
#define RESET_CTL_REQUEST_RESET     (1U << 0)  /* bit0 REQUEST_RESET */
#define RESET_CTL_READY_TO_RESET    (1U << 1)  /* bit1 READY_TO_RESET */
#define RESET_CTL_CAT_ERROR         (1U << 2)  /* bit2 CAT_ERROR */
#define RESET_CTL_MASKED_ENABLE(x)  ((x) << 16 | (x))   /* masked-field assert */
#define RESET_CTL_MASKED_DISABLE(x) ((x) << 16 | 0)     /* masked-field deassert */

/* Execlist submission ports — i915 gt/intel_engine_regs.h (lines 178-234, verified) */
#define RING_ELSP_OFFSET                0x230   /* RING_ELSP(base) = base + 0x230 */
#define RING_EXECLIST_STATUS_LO_OFFSET  0x234   /* RING_EXECLIST_STATUS_LO(base) = base + 0x234 */
#define RING_EXECLIST_STATUS_HI_OFFSET  0x238   /* HI = base + 0x234 + 4 */
#define RING_CONTEXT_CONTROL_OFFSET     0x244   /* RING_CONTEXT_CONTROL(base) = base + 0x244 */
#define RING_CONTEXT_STATUS_PTR_OFFSET  0x3A0   /* RING_CONTEXT_STATUS_PTR(base) = base + 0x3a0 */
#define RING_EXECLIST_SQ_CONTENTS_OFFSET 0x510  /* RING_EXECLIST_SQ_CONTENTS(base) = base + 0x510 */
#define RING_EXECLIST_CONTROL_OFFSET    0x550   /* RING_EXECLIST_CONTROL(base) = base + 0x550 */
/* GT-level fault register (absolute, not per-engine) — i915 gt/intel_gt_regs.h:
 * GEN12_RING_FAULT_REG = 0xcec4 (line 1041); RING_FAULT_VALID = bit0 (line 332).
 * GEN8_RING_FAULT_REG (0x4094) is intentionally NOT read on GRAPHICS_VER>=12:
 * i915 reads only GEN12_RING_FAULT_REG for Gen12+ (intel_gt.c:373-383); 0x4094
 * holds stale/garbage without a VALID bit and was removed as a false positive. */
#define GEN12_RING_FAULT_REG_OFFSET     0xCEC4
#define RING_FAULT_VALID_BIT            (1U << 0)
/* EXECLIST_CONTROL bits (i915 intel_engine_regs.h line 233-234) */
#define GEN12_CTX_VALID                 (1U << 0)
#define GEN12_EL_SUBMIT_REQ             (1U << 1)
/* EXECLIST_STATUS_LO bits — user-confirmed semantics from i915:
 * bit0 ELEMENT_ACTIVE, bit4 SINGLE_CONTEXT, bits28-31 ACTIVE_QUEUED */
#define EXECLIST_STATUS_ELEMENT_ACTIVE  (1U << 0)
#define EXECLIST_STATUS_SINGLE_CONTEXT  (1U << 4)
#define EXECLIST_STATUS_ACTIVE_QUEUED   (0xFUL << 28)

/* EXECLIST_CONTROL bits — i915 intel_engine_regs.h line 235 */
#define EL_CTRL_LOAD                    (1U << 0)   /* trigger load of SQ_CONTENTS descriptors */

/* RING_MODE_GEN7 execlists enable bits — i915 enable_execlists() */
#define RING_MODE_GEN7_RUN_LIST         (1U << 15)  /* GFX_RUN_LIST_ENABLE — gen8-10 path */
#define RING_MODE_GEN7_DISABLE_LEGACY   (1U << 3)   /* GEN11_GFX_DISABLE_LEGACY_MODE — gen11+ path */

/*
 * ─────────────────────────────────────────────
 *  Execlists CSB (Context Status Buffer) — Gen11+
 * ─────────────────────────────────────────────
 *  Lives in the RING_HWS_PGA (0x80) HWSP page. Values VERIFIED against
 *  torvalds/linux @ b9b3e33b70b71e516930117e21de3ad2a7723747
 *  (gt/intel_engine.h:201-205):
 *    I915_HWS_CSB_BUF0_INDEX = 0x10   (CSB entries, u64 each, at HWSP byte 0x40)
 *    ICL_HWS_CSB_WRITE_INDEX = 0x2f   (Gen11+ write ptr, HWSP byte 0xbc)
 *    I915_HWS_CSB_WRITE_INDEX = 0x1f  (pre-Gen11)
 *  NOTE: the write pointer is an INDEPENDENT index — it is NOT
 *  buf0 + entries*2. Old (wrong) values were 0x50 / 0x68, which pointed at
 *  HWSP dwords the HW never touches → "CSB write ptr = 0" symptom.
 *  RING_CONTEXT_STATUS_PTR (base+0x3a0) is floored on reset via
 *  reset_csb_pointers(): 0xffff<<16 | rv<<8 | rv, rv = entries-1.
 */
#define HWS_CSB_BUF0_DWORD      0x10            /* I915_HWS_CSB_BUF0_INDEX (byte 0x40) */
#define HWS_CSB_ENTRIES_GEN11   12              /* GEN11_CSB_ENTRIES (intel_lrc_reg.h:69) */
#define HWS_CSB_WRITE_DWORD     0x2f            /* ICL_HWS_CSB_WRITE_INDEX (byte 0xbc) */
#define HWS_SEQNO_DWORD         0x20            /* I915_HWS_SEQNO_INDEX (byte 0x80) */
#define HWS_SEQNO_ADDRESS(hwsp) ((hwsp) + (HWS_SEQNO_DWORD * 4))
#define CONTEXT_STATUS_PTR_RESET \
    ((0xffffU << 16) | ((HWS_CSB_ENTRIES_GEN11 - 1) << 8) | (HWS_CSB_ENTRIES_GEN11 - 1))

/*
 * ─────────────────────────────────────────────
 *  LRC (Logical Ring Context) image — Gen12 RCS
 * ─────────────────────────────────────────────
 *  i915 intel_lrc_reg.h / intel_lrc.c. The context object is 4 pages:
 *  page 0 = PPHWSP (full 4KB), register state starts at LRC_STATE_OFFSET
 *  = LRC_STATE_PN(1) * PAGE_SIZE = 0x1000 (mainline intel_lrc.h:27).
 *  The state is a mini batch of MI_LOAD_REGISTER_IMM commands; dword 0 of the
 *  state MUST be MI_NOOP before the first MI_LRI (intel_gpu_commands.h:149).
 *  Layout = gen12_rcs_offsets verbatim (intel_lrc.c:455-549), 185 dwords:
 *    dw  0       : MI_NOOP
 *    dw  1-27    : LRI(13,POSTED)  0x244 0x034 0x030 0x038 0x03c 0x168
 *                  0x140 0x110 0x1c0 0x1c4 0x1c8 0x180 0x2b4
 *    dw  28-32   : NOP(5)
 *    dw  33-51   : LRI(9,POSTED)   0x3a8 0x28c 0x288 0x284 0x280 0x27c
 *                  0x278 0x274 0x270   ← PDP3..PDP0 (PDP0 = PML4, 4-level)
 *    dw  52-58   : LRI(3,POSTED)   0x1b0 0x5a8 0x5ac
 *    dw  59-64   : NOP(6)
 *    dw  65-67   : LRI(1, flags=0) 0x0c8   ← NOT posted (no FORCE_POSTED)
 *    dw  68-80   : NOP(13)
 *    dw  81-183  : LRI(51,POSTED)  0x588 x6, 0x028 0x09c 0x0c0 0x178 0x17c
 *                  0x358 0x170 0x150 0x154 0x158 0x41c, 0x600..0x67c, 0x068 0x084
 *    dw  184     : MI_NOOP
 *  Value-slot dword indices (= i915 CTX_* = bspec off + 1) — see defines below.
 */
#define LRC_STATE_OFFSET        0x1000          /* = LRC_STATE_PN(1) * PAGE_SIZE */
#define LRC_CONTEXT_SIZE        0x4000          /* 4 pages — gen12 RCS context_size (PPHWSP+state) */
#define LRC_STATE_DWORDS        185             /* gen12_rcs_offsets total (dw 0..184) */

/* XCS (BCS/VCS/VECS) context state size — gen12_xcs_offsets total (52 dw):
 *   dw 0       : MI_NOOP
 *   dw 1-27    : LRI(13,POSTED)  identical to RCS block 1 (0x244..0x2b4)
 *   dw 28-32   : NOP(5)
 *   dw 33-51   : LRI(9,POSTED)   identical to RCS block 2 (0x3a8 + PDP3..PDP0)
 * No blocks 3-5 (no indirect-ctx regs, no R_PWR_CLK_STATE, no GPR block).
 * CTX_* value-slot indices are therefore the SAME as RCS for blocks 1-2
 * (0x03..0x33) — lrcUpdateRingRegs() + lrcAllocPPGTT() patch them unchanged. */
#define LRC_STATE_DWORDS_XCS    52              /* gen12_xcs_offsets total (dw 0..51) */

/* Value-slot dword indices (= i915 CTX_* intel_lrc_reg.h = bspec off + 1).
 * Block 1 (dw 1-27): */
#define CTX_CONTEXT_CONTROL     0x03            /* value slot of 0x244 (RING_CONTEXT_CONTROL) */
#define CTX_RING_HEAD           0x05            /* value slot of 0x034 */
#define CTX_RING_TAIL           0x07            /* value slot of 0x030 */
#define CTX_RING_START          0x09            /* value slot of 0x038 */
#define CTX_RING_CTL            0x0b            /* value slot of 0x03C */
#define CTX_BB_STATE            0x11            /* value slot of 0x110 */
/* Block 2 (dw 33-51): */
#define CTX_TIMESTAMP           0x23            /* value slot of 0x3a8 (RING_CTX_TIMESTAMP) */
#define CTX_PDP3_UDW            0x25            /* value slot of 0x28c */
#define CTX_PDP3_LDW            0x27            /* value slot of 0x288 */
#define CTX_PDP2_UDW            0x29            /* value slot of 0x284 */
#define CTX_PDP2_LDW            0x2b            /* value slot of 0x280 */
#define CTX_PDP1_UDW            0x2d            /* value slot of 0x27c */
#define CTX_PDP1_LDW            0x2f            /* value slot of 0x278 */
#define CTX_PDP0_UDW            0x31            /* value slot of 0x274 — 4-level: PML4 phys UDW */
#define CTX_PDP0_LDW            0x33            /* value slot of 0x270 — 4-level: PML4 phys LDW */
/* Block 4 (dw 65-67): */
#define CTX_R_PWR_CLK_STATE     0x43            /* value slot of 0x0c8 */
/* Block 5 (dw 81-183): */
#define CTX_MI_MODE             0x61            /* value slot of 0x09c (RING_MI_MODE) */
#define CTX_BB_OFFSET           0x71            /* value slot of 0x158 (RING_BB_OFFSET) */

/* CTX_CONTEXT_CONTROL value on first load — i915 init_common_regs(inhibit=true):
 *   ENABLE(INHIBIT_SYN_CTX_SWITCH) | DISABLE(RESTORE_INHIBIT) | RESTORE_INHIBIT
 * = 0x00080008 | 0x00010000 | 0x00000001 */
#define CTX_CONTROL_INHIBIT_INIT 0x00090009u

/* MI_LOAD_REGISTER_IMM — i915 intel_gpu_commands.h lines 149-160:
 *   MI_LOAD_REGISTER_IMM(x) = MI_INSTR(0x22, 2*(x)-1); Gen11+ ALWAYS ORs
 *   MI_LRI_LRM_CS_MMIO (bit19); POSTED = bit12 */
#define MI_LRI(count)               (0x11000000u | (2u * (count) - 1))
#define MI_LRI_FORCE_POSTED         (1u << 12)
#define MI_LRI_LRM_CS_MMIO          (1u << 19)

/*
 * ─────────────────────────────────────────────
 *  Context descriptor (64-bit, low dword) — i915 intel_lrc.h
 * ─────────────────────────────────────────────
 *  bits 0-11 flags, bits 12-31 LRCA = GGTT offset of context object (page-aligned,
 *  NO shift — Jar's `(ggtt>>12)<<11` was wrong), bits 32+ ctx id etc.
 *  lrc_descriptor(): LRCA | GEN8_CTX_VALID | GEN8_CTX_PRIVILEGE | mode<<3,
 *  and lrc_update_regs() ORs CTX_DESC_FORCE_RESTORE (bit 2).
 */
#define DESC_VALID                  (1u << 0)
#define DESC_FORCE_RESTORE          (1u << 2)
#define DESC_ADDRESSING_SHIFT       3            /* GEN8_CTX_ADDRESSING_MODE_SHIFT */
#define DESC_LEGACY_32B             (1u << 3)    /* INTEL_LEGACY_32B_CONTEXT << 3 */
#define DESC_LEGACY_64B             (3u << 3)    /* INTEL_LEGACY_64B_CONTEXT << 3 */
#define DESC_PRIVILEGE              (1u << 8)
#define DESC_GTT_ADDRESS_MASK       0xFFFFF000u  /* CTX_GTT_ADDRESS_MASK = GENMASK(31,12) */

/* RING_CTL bit definitions
 * RING_CTL field (bits 13:11) = (size / 4KB) - 1 = sizeInPages - 1
 * With (sizeInPages-1) << RING_CTL_SIZE_SHIFT = (sizeInPages-1) << 11
 *   → bits 13:11 encode (pages-1) per i915 (RING_CTL_SIZE_SHIFT = 11)
 * ⚠️ DO NOT use shift 12 — HW decodes bits 13:11, so << 12 shifts the
 *   field one bit too far (16KB ring would decode as 28KB, HW walks
 *   past the buffer → hang on first submit).
 */
#define RING_CTL_VALID              (1U << 0)
#define RING_CTL_SIZE_SHIFT         11      /* (sizeInPages - 1) << 11 = size field at bits 13:11 */

/* RING_MI_MODE bit definitions */
#define RING_MI_MODE_STOP_RING      (1U << 0)   /* Write 1 = stop */
#define RING_MI_MODE_SOFT_RESET     (1U << 1)

/*
 * ─────────────────────────────────────────────
 *  Engine Type IDs
 * ─────────────────────────────────────────────
 */
typedef enum {
    kMyIntelEngineRCS = 0,      /* Render Command Streamer */
    kMyIntelEngineBCS = 1,      /* Blitter Command Streamer */
    kMyIntelEngineVCS = 2,      /* Video Command Streamer */
    kMyIntelEngineVECS = 3,     /* Video Enhancement */
    kMyIntelEngineCount
} MyIntelEngineType;

/*
 * ─────────────────────────────────────────────
 *  MyIntelRing Structure
 * ─────────────────────────────────────────────
 *
 *  Simple struct + C-style functions
 *  No C++ constructor/destructor (IOKit constraints)
 */
struct MyIntelRing {
    /* Engine Info */
    MyIntelEngineType engineType;   /* RCS / BCS */
    uint32_t          mmioBase;     /* Engine MMIO base offset (e.g. 0x02000) */

    /* Ring Buffer (backed by GGTT page) */
    uint32_t         *vaddr;        /* CPU virtual address (cached write pointer) */
    uint32_t          ggttOffset;   /* GGTT offset of ring buffer */
    uint32_t          size;         /* ring buffer size in bytes (power of 2) */
    void             *gemBuf;       /* GEM buffer handle — needed to free in ringDestroy */

    /* Hardware Status Page (HWSP) for Head Tracking */
    uint32_t          hwspGgtt;     /* GGTT offset of HWSP */
    void             *hwspGemBuf;   /* GEM buffer handle for HWSP */
    uint32_t         *hwspVaddr;    /* CPU VA of HWSP — CSB entries live here (dword 0x50) */

    /* Execlists LRC context object (4 pages: PPHWSP + register state) */
    void             *lrcGemBuf;    /* GEM buffer handle */
    uint32_t          lrcGgtt;      /* GGTT offset (page-aligned) = LRCA in descriptor */
    uint32_t         *lrcVaddr;     /* CPU VA of context object */
    bool              lrcInited;    /* LRC image built + ready for submission */

    /* PPGTT identity-map (4-level: PML4 → PDP → PD → 128 PTs, 256MB window).
     * Allocated once at ringCreate; PML4 phys patched into LRC slots
     * CTX_PDP0_UDW=0x31 / CTX_PDP0_LDW=0x33 at build time.
     * Layout: page 0 = PML4, page 1 = PDP, page 2 = PD, pages 3..130 = PT.
     * All entries are u64 (gen8_pte_t — Gen8+ HW reads 8 bytes/entry). */
    void             *ppgttGemBuf;    /* GEM buffer handle (131 GGTT pages) */
    uint32_t          ppgttGgtt;      /* GGTT offset of PPGTT allocation */
    uint64_t         *ppgttVaddr;     /* CPU VA of PML4 page (page 0) */
    uint64_t         *ppgttPdp;       /* CPU VA of PDP page (page 1) */
    uint64_t         *ppgttPTs;       /* CPU VA of PT page 0 (page 3) */
    uint32_t          ppgttPml4Lo;    /* PDP0 LDW = lower 32 bits of PML4 phys */
    uint32_t          ppgttPml4Hi;    /* PDP0 UDW = upper 32 bits of PML4 phys */
    uint32_t          ppgttPdeEncode; /* PDE flags (PRESENT|RW = 0x3) */
    uint32_t          ppgttPteEncode; /* PTE flags (PRESENT|RW = 0x3) */

    /* Max pending client batches queued before the next kick drains them */
#define RING_PENDING_MAX            8

    /* Pending client batch queue — F8b multi-batch: submitClientTaskViaRing()
     * appends up to RING_PENDING_MAX entries; kickCommandSet2Locked() drains
     * the whole queue in order (one BB_START per entry, then USER_INTERRUPT).
     * Ring-buffer semantics: pendingHead = next entry to consume,
     * pendingCount = queued entries (slot = (head + i) % RING_PENDING_MAX). */
    struct PendingBatch {
        uint32_t          ggtt;        /* Batch GGTT offset for BB_START (0 = none) */
        uint32_t          taskType;    /* Client task type (kMyIntelTaskType_*) */
        uint64_t          packetData;  /* Client packet data (task payload) */
    };
    PendingBatch      pendingQueue[RING_PENDING_MAX];
    uint32_t          pendingHead;    /* index of next entry to drain */
    uint32_t          pendingCount;   /* number of queued entries */

    /* Ring State (byte offsets within ring) */
    uint32_t          head;         /* RING_HEAD shadow (read position, updated by HW) */
    uint32_t          tail;         /* RING_TAIL shadow (write position, updated by us) */
    uint32_t          emit;         /* Current command write position (within ring) */
    uint32_t          space;        /* Available space in bytes */

    /* Status */
    bool              initialized;  /* Ring has been initialized */
    bool              workPending;  /* Client work queued → submit on next IRQ kick */
    uint32_t          lastTail;     /* Last tail written via RING_TAIL register */
    uint32_t          wraparound;   /* Ring wraparound count (debug) */

    /* Hardware Breadcrumbs / Seqno Tracking */
    uint32_t          currentSeqno;   /* Last submitted hardware seqno */
    uint32_t          completedSeqno; /* Last completed hardware seqno */

    /* Magic */
    uint32_t          magic;
};

/* Magic for validation */
#define RING_MAGIC                  0x52494E47   /* "RING" */

/* Default ring size = 16KB (4096 dwords) */
#define RING_DEFAULT_SIZE           0x4000
#define RING_MIN_SIZE               0x1000       /* 4KB min */
#define RING_MAX_SIZE               0x80000      /* 512KB max */

/* Reserve space for request overhead (MI_NOOP + MI_USER_INTERRUPT + MI_FLUSH) */
#define RING_MIN_FREE_SPACE         64

/*
 * ─────────────────────────────────────────────
 *  Ring Helper Macros (used by ringBegin/Advance)
 * ─────────────────────────────────────────────
 */

/*!
 * @brief  Wrap a ring offset within ring buffer size (power-of-2 trick)
 *         len: ring->size; pos: the byte offset
 *         returns pos & (len - 1) when len is a power of 2
 */
static inline uint32_t ringWrap(const MyIntelRing *ring, uint32_t offset)
{
    return offset & (ring->size - 1);
}

/*!
 * @brief  Direction from a to b (positive = forward, negative = backward)
 *         used to check if tail is behind head
 */
static inline int32_t ringDirection(const MyIntelRing *ring, uint32_t a, uint32_t b)
{
    return (int32_t)((b - a) & (ring->size - 1));
}

/*!
 * @brief  Available space in ring buffer (bytes)
 *
 *  XE driver formula (xe_lrc_ring_space):
 *    ((head - tail - 1) & (size - 1)) + 1
 *
 *  When empty (head == tail): returns size (all space available)
 *  When 1 byte used: returns size - 1
 *  ... when full: returns 1 (minimum space, ring can hold size-1 bytes)
 *
 *  ⚠️ Previous implementation used ringDirection()-1 which underflowed
 *  to UINT32_MAX when ring was empty (head == tail), causing the ring
 *  to appear to have 4GB of space — corrupting ring state.
 *
 *  Reference: xe_lrc.c xe_lrc_ring_space()
 */
static inline uint32_t ringSpace(const MyIntelRing *ring)
{
    return ((ring->head - ring->tail - 1) & (ring->size - 1)) + 1;
}

/*!
 * @brief  Check if ring tail wraps past head (ring is full)
 */
static inline bool ringIsFull(const MyIntelRing *ring)
{
    return ring->tail == (ring->head ^ ring->size);
}

/*
 * ─────────────────────────────────────────────
 *  MI Commands (Gen8+ / Gen12)
 * ─────────────────────────────────────────────
 *
 *  Encoding:
 *    Bits 31:29 = command type (0=MI, 1=2D, 2=3D)
 *    Bits 28:23 = MI opcode
 *    Bits 22:0  = operands / flags
 */

/* MI_NOOP = 0x00000000 */
#define MI_NOOP                     0x00000000

/* MI_USER_INTERRUPT = (0x02 << 23) = 0x01000000 */
#define MI_USER_INTERRUPT           0x01000000

/* MI_FLUSH_DW — i915 gt/intel_gpu_commands.h, VERIFIED against
 * torvalds/linux master 2026-08-11:
 *   MI_FLUSH_DW = MI_INSTR(0x26, 1) = 0x13000001 (gen6 3-dword base; the
 *   length field lives in bits 5:0 = dwords-2, so "1" = 3 dwords).
 * Gen12 xcs form (i915 gen12_emit_flush_xcs, gt/gen8_engine_cs.c):
 *   cmd = MI_FLUSH_DW + 1 | MI_FLUSH_DW_OP_STOREDW | MI_FLUSH_DW_STORE_INDEX
 *       = 0x13000002 | (1<<14) | (1<<21) = 0x13214002   (4 dwords)
 *   dword1 = LRC_PPHWSP_SCRATCH_ADDR = 0x800 (offset within the context
 *            PPHWSP; STORE_INDEX makes it a PPHWSP offset, not a GGTT addr)
 *   dword2 = 0 (upper addr), dword3 = 0 (value written to PPHWSP scratch)
 * ⚠️ There are NO GFX/MEDIA/LLC "flush flag" bits in MI_FLUSH_DW dword1 —
 * the old kext encoding (0x07000007 dword1, invented flags) was wrong and
 * desynced the command parser. */
#define MI_FLUSH_DW                 (0x13000000 | 1)   /* MI_INSTR(0x26, 1) */
#define MI_FLUSH_DW_OP_STOREDW      (1U << 14)
#define MI_FLUSH_DW_STORE_INDEX     (1U << 21)
#define MI_FLUSH_DW_GEN12           (MI_FLUSH_DW + 1 | MI_FLUSH_DW_OP_STOREDW | MI_FLUSH_DW_STORE_INDEX)  /* 0x13214002 */
#define MI_FLUSH_DW_MEDIA_FLUSH     (1U << 1)
#define LRC_PPHWSP_SCRATCH_ADDR     0x800u             /* i915 intel_lrc.h LRC_PPHWSP_PN(0)*PAGE + 0x800 */

/* MFX_WAIT — Insert wait for pending media operations (Gen12+ VCS) */
#define MFX_WAIT                   0x15000000
#define MFX_WAIT_EN                (1U << 0)
#define MI_FLUSH_DW_MEDIA_ONLY     (MI_FLUSH_DW | MI_FLUSH_DW_MEDIA_FLUSH)

/* MI_BATCH_BUFFER_START — VERIFIED against i915 gt/intel_gpu_commands.h:188:
 *   MI_BATCH_BUFFER_START_GEN8 = MI_INSTR(0x31, 1) = 0x18800001
 * ⚠️ bit8 is the NON-SECURE bit (gen8_emit_bb_start: flags &
 * I915_DISPATCH_SECURE ? 0 : BIT(8)), NOT "2nd-level" as this header
 * previously claimed. Address is a PPGTT VA in the identity map, so no
 * GGTT-selection bits are set (pre-Gen8 semantics). */
#define MI_BATCH_BUFFER_START_GEN8   0x18800001   /* MI_INSTR(0x31, 1) */
#define MI_BATCH_BUFFER_START_NONSEC (1U << 8)    /* non-secure batch */
#define MI_BATCH_BUFFER_END          0x05000000

/* ── PPGTT identity-map (4-level / DESC_LEGACY_64B) ─────────────────────
 * VERIFIED against i915 gt/intel_gtt.h + gen8_ppgtt.c + intel_lrc.c:
 *   GEN8_PAGE_PRESENT=bit0, GEN8_PAGE_RW=bit1 (intel_gtt.h:152-153)
 *   Gen12 PTE PAT bits: bit3=PAT0, bit4=PAT1, bit7=PAT2, bit11=LM
 *   (intel_gtt.h:92-95); TGL_CACHELEVEL LLC → pat_index 0 → NO PAT bits.
 *   PPAT_CACHED_PDE = 0 (WB LLC, intel_gtt.h:136) → cached PDE/PTE = phys|3.
 * Layout: 1 PML4 + 1 PDP + 1 PD + 128 PT pages (131 total). All entries are
 * u64 (gen8_pte_t — Gen8+ HW reads 8B per entry; the 32-bit entry world
 * ended at Gen7). PDP0 regs (0x270/0x274) hold the PML4 page phys —
 * ASSIGN_CTX_PML4 (intel_lrc.c init_ppgtt_regs, "other PDP descriptors are
 * ignored"). 4-level walk: PML4[VA47:39] → PDP[VA38:30] → PD[VA29:21] →
 * PT[VA20:12] → 4KB page. VA<256MB → PML4[0]/PDP[0]/PD[0..127]. */
#define GEN8_PAGE_PRESENT           (1U << 0)
#define GEN8_PAGE_RW                (1U << 1)
#define GEN12_PPGTT_PTE_PAT0        (1U << 3)
#define GEN12_PPGTT_PTE_PAT1        (1U << 4)
#define GEN12_PPGTT_PTE_PAT2        (1U << 7)
#define GEN12_PPGTT_PTE_LM          (1U << 11)
#define PPAT_CACHED_PDE             0               /* WB LLC (intel_gtt.h:136) */
#define PPGTT_PML4_PAGES            1
#define PPGTT_PDP_PAGES             1
#define PPGTT_PD_PAGES              1
#define PPGTT_PT_PAGES              128
#define PPGTT_TOTAL_PAGES           (PPGTT_PML4_PAGES + PPGTT_PDP_PAGES + PPGTT_PD_PAGES + PPGTT_PT_PAGES)   /* 131 */
#define PPGTT_WINDOW_SIZE           (PPGTT_PT_PAGES * 512 * 4096u)      /* 256MB */

/* PDP0 pairs live in gen12_rcs_offsets block 2 (0x274 = PDP0_UDW,
 * 0x270 = PDP0_LDW). Value slots CTX_PDP0_UDW/LDW = 0x31/0x33 — see the
 * LRC value-slot defines above (block 2, dw 33-51). */
#define PDP0_UDW_REG_OFFSET         0x274
#define PDP0_LDW_REG_OFFSET         0x270

/* MI_LOAD_REGISTER_IMM(x) = MI_INSTR(0x22, 2*(x)-1) — i915 gt/intel_gpu_commands.h:155.
 * 1 reg → (0x22 << 23) | 1 = 0x11000001, 3 dwords: hdr + reg + value.
 * NOTE (i915 comment :149): always issue a MI_NOOP BEFORE the MI_LRI, otherwise
 * HW simply ignores the register load under certain conditions.
 * In a plain batch (not LRC ctx restore) i915 emits the LRI WITHOUT bit19
 * (MI_LRI_LRM_CS_MMIO) — see selftests/i915_perf.c:342 — so absolute MMIO
 * offsets apply. bit19 is only OR'd in the LRC image path (MyIntelRing.cpp:745). */
#define MI_LOAD_REGISTER_IMM(n)     (0x11000000u | ((2u * (n) - 1) & 0x7F))

/* MI_STORE_REGISTER_MEM = MI_INSTR(0x24,1) = 0x12000001 (3 dw: hdr+reg+addr32)
 * MI_STORE_REGISTER_MEM_GEN8 = MI_INSTR(0x24,2) = 0x12000002 (4 dw: hdr+reg+addr_lo+addr_hi)
 *   — i915 gt/intel_gpu_commands.h:161-163 */
#define MI_STORE_REGISTER_MEM       (0x12000000u | 1)
#define MI_STORE_REGISTER_MEM_GEN8  (0x12000000u | 2)
#define MI_SRM_LRM_GLOBAL_GTT       (1U << 22)   /* bit22 = 0 → PPGTT, 1 → GGTT */
#define MI_LOAD_REGISTER_MEM_GEN8   0x14800002u  /* MI_INSTR(0x29,2) */
#define MI_LOAD_REGISTER_REG        0x15000001u  /* MI_INSTR(0x2A,1) */
#define MI_LRR_SOURCE_CS_MMIO       (1U << 18)

/* CS_GPR0 (RCS) = RENDER_RING_BASE (0x2000) + 0x600 — i915 gt/intel_engine_regs.h:238
 * GEN8_RING_CS_GPR(base, n) = base + 0x600 + n*8. Safe scratch register:
 * i915 perf (noa_wait) writes a magic value into these GPRs via MI_LRI and
 * reads them back via MI_SRM to prove the batch executed (i915_selftest
 * i915_perf.c:341-399). Zero side effects on rendering. */
#define MI_CS_GPR0_RCS              (0x2000 + 0x600)   /* 0x2600 */

/* MI_ARB_ON_OFF = (0x08 << 23) = 0x04000000 */
#define MI_ARB_ON_OFF               (0x04000000)
#define MI_ARB_DISABLE              (0 << 0)   /* bit 0 = 0: disable */
#define MI_ARB_ENABLE               (1 << 0)   /* bit 0 = 1: enable */

/* MI_REPORT_HEAD = (0x07 << 23) = 0x03800000 */
#define MI_REPORT_HEAD              0x03800000

/* MI_MATH = (0x1A << 23) = 0x0D000000 */
/* MI_SEMAPHORE_WAIT = (0x1C << 23) = 0x0E000000 */

/*
 * ── MI_MATH (GPR ALU) — i915 gt/intel_gpu_commands.h:344-363, VERIFIED
 * against torvalds/linux master 2026-08-11. Full usage example in
 * gt/selftest_rps.c:117-128 (the 1024-iteration rps burn loop):
 *   MI_MATH(4) → LOAD(SRCA, GPR(COUNT)) → LOAD(SRCB, GPR(INC)) → ADD →
 *   STORE(GPR(COUNT), ACCU)
 * MI_MATH(x) = MI_INSTR(0x1a, (x)-1) → MI_MATH(4) = 0x0D000003 (1 header
 * dw + 4 ALU dwords). GPRs = CS_GPR(base,n) = base+0x600+n*8; RCS base
 * 0x2000 → CS_GPR0 = 0x2600 (MI_CS_GPR0_RCS above). */
#define MI_MATH(x)                      (0x0D000000u | ((x) - 1))
#define MI_MATH_INSTR(opcode, op1, op2) (((opcode) << 20) | ((op1) << 10) | (op2))
#define MI_MATH_LOAD(op1, op2)          MI_MATH_INSTR(0x080, op1, op2)   /* GPR→ALU src */
#define MI_MATH_LOADINV(op1, op2)       MI_MATH_INSTR(0x480, op1, op2)
#define MI_MATH_ADD                     MI_MATH_INSTR(0x100, 0x0, 0x0)
#define MI_MATH_STORE(op1, op2)         MI_MATH_INSTR(0x180, op1, op2)   /* ALU accu→GPR */
#define MI_MATH_REG(x)                  (x)
#define MI_MATH_REG_SRCA                0x20
#define MI_MATH_REG_SRCB                0x21
#define MI_MATH_REG_ACCU                0x31
#define MI_CS_GPR(base, n)              ((base) + 0x600 + (n) * 8)

/*
 * ── XY_FAST_COPY_BLT (BCS blitter) — i915 gt/intel_gpu_commands.h:269,278
 * + gt/intel_migrate.c emit_copy():591-599, VERIFIED against torvalds/linux
 * master 2026-08-11.
 *   GEN9_XY_FAST_COPY_BLT_CMD = (2<<29 | 0x42<<22) = 0x50800000
 *   → | (10-2) = 0x50800008   (10-dword linear copy)
 *   BLT_DEPTH_32 = (3<<24)     (32bpp linear)
 *   d0: hdr|(10-2)  d1: BLT_DEPTH_32|PAGE_SIZE(dst pitch)  d2: 0 (dst x1,y1)
 *   d3: (size>>12)<<16 | PAGE_SIZE/4 (height<<16|width)   d4: dst_offset
 *   d5: instance   d6: 0 (src x1,y1)   d7: PAGE_SIZE (src pitch)
 *   d8: src_offset   d9: instance
 * i915 emits src/dst offsets as object offsets into the same VMA and lets
 * the migrator resolve the base — we pass absolute GGTT identity offsets
 * (our PPGTT identity window == GGTT offset, same as BB_START addressing),
 * instance = 0 (single BCS). */
#define GEN9_XY_FAST_COPY_BLT_CMD       ((2u << 29) | (0x42u << 22))     /* 0x50800000 */
#define GEN9_XY_FAST_COPY_BLT_10DW      (GEN9_XY_FAST_COPY_BLT_CMD | (10 - 2))  /* 0x50800008 */
#define BLT_DEPTH_32                    (3u << 24)                        /* 0x03000000 */

/*
 * ── PIPE_CONTROL — i915 gt/intel_gpu_commands.h:287-318 + gt/gen8_engine_cs.c
 * gen12_emit_fini_breadcrumb_rcs():819-853, VERIFIED against torvalds/linux
 * master 2026-08-11.
 *   GFX_OP_PIPE_CONTROL(len) = (0x3<<29)|(0x3<<27)|(0x2<<24)|(len-2)
 *     → GFX_OP_PIPE_CONTROL(6) = 0x7A000004 (6-dword form, gen8+).
 *   __gen8_emit_pipe_control (gen8_engine_cs.h:53-62): memset 6 dw;
 *     batch[0] = GFX_OP_PIPE_CONTROL(6)|bit_group_0; batch[1] = bit_group_1;
 *     batch[2] = offset; batch[3..5] = 0.
 *   gen12 flush (gen12_emit_fini_breadcrumb_rcs): PIPE_CONTROL0_HDC_PIPELINE_FLUSH
 *     (REG_BIT(9)) goes in DWORD0 (bit_group_0); the cache-flush flag set goes
 *     in DWORD1 (bit_group_1): CS_STALL|TLB_INVALIDATE|TILE_CACHE_FLUSH|
 *     RENDER_TARGET_CACHE_FLUSH|DEPTH_CACHE_FLUSH|DC_FLUSH_ENABLE|FLUSH_ENABLE
 *     (+FLUSH_L3 for graphics ver < 12.70). The QW_WRITE store (breadcrumb)
 *     is a second PIPE_CONTROL: d1 = FLUSH_ENABLE|CS_STALL|QW_WRITE, d2 =
 *     store addr, d4 = value (gen8_engine_cs.h __gen8_emit_write_rcs:76-87). */
#define GFX_OP_PIPE_CONTROL(len)        (((0x3u << 29) | (0x3u << 27) | (0x2u << 24)) | ((len) - 2))
#define PIPE_CONTROL_COMMAND_CACHE_INVALIDATE   (1u << 29)  /* gen11+ */
#define PIPE_CONTROL_TILE_CACHE_FLUSH           (1u << 28)  /* gen11+ */
#define PIPE_CONTROL_FLUSH_L3                   (1u << 27)
#define PIPE_CONTROL_AMFS_FLUSH                 (1u << 25)  /* gen12+ */
#define PIPE_CONTROL_GLOBAL_GTT_IVB             (1u << 24)  /* gen7+ — addr is GGTT */
#define PIPE_CONTROL_MMIO_WRITE                 (1u << 23)
#define PIPE_CONTROL_STORE_DATA_INDEX           (1u << 21)
#define PIPE_CONTROL_CS_STALL                   (1u << 20)
#define PIPE_CONTROL_GLOBAL_SNAPSHOT_RESET      (1u << 19)
#define PIPE_CONTROL_TLB_INVALIDATE             (1u << 18)
#define PIPE_CONTROL_PSD_SYNC                   (1u << 17)  /* gen11+ */
#define PIPE_CONTROL_MEDIA_STATE_CLEAR          (1u << 16)
#define PIPE_CONTROL_WRITE_TIMESTAMP            (3u << 14)
#define PIPE_CONTROL_QW_WRITE                   (1u << 14)
#define PIPE_CONTROL_POST_SYNC_OP_MASK          (3u << 14)
#define PIPE_CONTROL_DEPTH_STALL                (1u << 13)
#define PIPE_CONTROL_WRITE_FLUSH                (1u << 12)
#define PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH  (1u << 12)  /* gen6+ */
#define PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE (1u << 11) /* MBZ on ILK */
#define PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE   (1u << 10)  /* GM45+ */
#define PIPE_CONTROL_INDIRECT_STATE_DISABLE     (1u << 9)
#define PIPE_CONTROL0_HDC_PIPELINE_FLUSH        (1u << 9)   /* gen12 — DWORD0 */
#define PIPE_CONTROL_NOTIFY                     (1u << 8)
#define PIPE_CONTROL_FLUSH_ENABLE               (1u << 7)   /* gen7+ */
#define PIPE_CONTROL_DC_FLUSH_ENABLE            (1u << 5)
#define PIPE_CONTROL_VF_CACHE_INVALIDATE        (1u << 4)
#define PIPE_CONTROL_CONST_CACHE_INVALIDATE     (1u << 3)
#define PIPE_CONTROL_STATE_CACHE_INVALIDATE     (1u << 2)
#define PIPE_CONTROL_STALL_AT_SCOREBOARD        (1u << 1)
#define PIPE_CONTROL_DEPTH_CACHE_FLUSH          (1u << 0)

/* gen12 RCS flush flag set (gen8_engine_cs.c gen12_emit_fini_breadcrumb_rcs
 * :825-831, graphics ver 12 < 12.70 → include FLUSH_L3) */
#define PIPE_CONTROL_GEN12_RCS_FLUSH    (PIPE_CONTROL_CS_STALL | \
                                         PIPE_CONTROL_TLB_INVALIDATE | \
                                         PIPE_CONTROL_TILE_CACHE_FLUSH | \
                                         PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH | \
                                         PIPE_CONTROL_DEPTH_CACHE_FLUSH | \
                                         PIPE_CONTROL_DC_FLUSH_ENABLE | \
                                         PIPE_CONTROL_FLUSH_ENABLE | \
                                         PIPE_CONTROL_FLUSH_L3)

/*
 * ─────────────────────────────────────────────
 *  Ring Helper Function Typedefs
 * ─────────────────────────────────────────────
 *
 *  These are function pointers that MyIntelGPU will provide
 *  to the ring code for MMIO access + GGTT management
 */

/* Read 32-bit MMIO register */
typedef uint32_t (*ReadReg32Func)(void *context, uint32_t offset);

/* Write 32-bit MMIO register */
/* Guard: MyIntelGEMBuffer.hpp may already define this */
#ifndef MY_INTEL_WRITE_REG_32_FUNC
#define MY_INTEL_WRITE_REG_32_FUNC
typedef void (*WriteReg32Func)(void *context, uint32_t offset, uint32_t value);
#endif /* MY_INTEL_WRITE_REG_32_FUNC */

/* Allocate a GEM buffer in GGTT */
typedef void *(*GEMAllocFunc)(void *context, uint32_t size, uint32_t flags);

/* Free a GEM buffer from GGTT */
typedef void (*GEMFreeFunc)(void *context, void *buffer);

/* Get GGTT offset of a GEM buffer */
typedef uint32_t (*GEMGetOffsetFunc)(void *context, const void *buffer);

/* Get CPU virtual address of a GEM buffer */
typedef void *(*GEMGetVAddrFunc)(void *context, const void *buffer);

/* Get per-page physical addresses of a GEM buffer (IOMallocAligned is not
 * physically contiguous — PTE mapping MUST use pagesPhys[i], never
 * physAddr + i*PAGE). May be NULL for CPU-only buffers. */
typedef const uint64_t *(*GEMGetPagesPhysFunc)(void *context, const void *buffer);

/* Acquire GT force-wake before engine register programming (may be NULL) */
typedef bool (*ForceWakeGetFunc)(void *context);
/* Release GT force-wake after engine register programming (may be NULL) */
typedef void (*ForceWakePutFunc)(void *context);
/* TRUE when called from the IRQ-servicing path (may be NULL) */
typedef bool (*InIrqContextFunc)(void *context);

/*
 * Callbacks struct — passed to ring init
 *
 * fwGet/fwPut are optional. When provided, the ring code holds GT
 * force-wake during engine register programming: without it, RING_*
 * writes on Gen11+ are dropped by the RENDER power well while the GT
 * is in RC6 (RING_CTL readback 0). Mirrors i915 xcs_resume().
 *
 * inIrqContext (2.0.229) — when non-NULL and TRUE, ringSubmitExeclists
 * must not block: i915 gen11_gt_irq_handler() never forcewakes nor polls
 * (the engine that raised USER IRQ is by definition awake).
 */
struct MyIntelRingCallbacks {
    void            *context;       /* MyIntelGPU instance */
    ReadReg32Func    readReg32;
    WriteReg32Func   writeReg32;
    GEMAllocFunc     gemAlloc;
    GEMFreeFunc      gemFree;
    GEMGetOffsetFunc gemGetOffset;
    GEMGetVAddrFunc  gemGetVAddr;
    GEMGetPagesPhysFunc gemGetPagesPhys;
    ForceWakeGetFunc fwGet;
    ForceWakePutFunc fwPut;
    InIrqContextFunc inIrqContext;
};

/*
 * ─────────────────────────────────────────────
 *  Ring API
 * ─────────────────────────────────────────────
 */

/*!
 * @brief  Create + initialize a ring buffer engine
 *
 * :
 *    1. Allocate ring buffer (GEM) — size = RING_DEFAULT_SIZE (16KB)
 *    2. Bind to GGTT
 *    3. Program RING_START = ring->ggttOffset
 *    4. Clear RING_HEAD = 0, RING_TAIL = 0
 *    5. Program RING_CTL = (size/4KB - 1) << 11 | RING_VALID
 *    6. Clear STOP_RING bit in RING_MI_MODE
 *    7. Write RING_TAIL = ring->tail
 *
 * @param engineType  kMyIntelEngineRCS or kMyIntelEngineBCS
 * @param mmioBase    Engine MMIO base (0x02000 for RCS, 0x22000 for BCS)
 * @param ringSize    Ring buffer size (RING_DEFAULT_SIZE = 16KB)
 * @param cb          Callbacks struct for MMIO + GEM access
 * @return MyIntelRing* NULL failed
 */
MyIntelRing *ringCreate(
    MyIntelEngineType    engineType,
    uint32_t             mmioBase,
    uint32_t             ringSize,
    MyIntelRingCallbacks *cb);

/*!
 * @brief  Destroy ring — stop + clear registers + free buffer
 *
 * @param ring  Ring object (may be NULL)
 * @param cb    Callbacks struct
 */
void ringDestroy(MyIntelRing *ring, MyIntelRingCallbacks *cb);

/*!
 * @brief  Reset ring to empty state (clear + re-init registers)
 *
 * reset ring
 * GPU hang first init
 *
 * @param ring  Ring object
 * @param cb    Callbacks struct
 * @return true = success
 */
bool ringReset(MyIntelRing *ring, MyIntelRingCallbacks *cb);

/*!
 * @brief  Begin command emission — reserve dwords in ring
 *
 *  Check available space, return pointer to write commands.
 * space → return NULL (caller ringFlush )
 *
 *  Reference: Linux intel_ring_begin()
 *
 * @param ring    Ring object
 * @param dwords  Number of dwords (uint32_t) to reserve
 * @param cb      Callbacks struct (may be NULL — only needed for MMIO in future)
 * @return uint32_t* — pointer to write dwords, NULL
 */
uint32_t *ringBegin(MyIntelRing *ring, uint32_t dwords);

/*!
 * @brief  Advance ring tail after writing commands
 *
 *  Update ring->emit + ring->tail based on how many dwords
 *  were written since ringBegin().
 *
 *  Reference: Linux intel_ring_advance()
 *
 * @param ring  Ring object
 * @param cs    Pointer returned by ringBegin() + dwords emitted
 */
void ringAdvance(MyIntelRing *ring, uint32_t *cs);

/*!
 * @brief  Submit ring — write RING_TAIL register to kick GPU
 *
 * ringAdvance() GPU
 *
 *  Reference: Linux i9xx_submit_request()
 *
 * @param ring  Ring object
 * @param cb    Callbacks struct
 */
void ringSubmit(MyIntelRing *ring, MyIntelRingCallbacks *cb);

/*!
 * @brief  Submit ring via execlists (Gen12+) — write LRC descriptor to
 *         RING_EXECLIST_SQ_CONTENTS then kick RING_EXECLIST_CONTROL|EL_CTRL_LOAD.
 *         Mirrors i915 write_desc() + execlists_submit_ports() on Gen12.
 *
 * @param ring  Ring object (LRC must be built: ring->lrcInited == true)
 * @param cb    Callbacks struct
 */
void ringSubmitExeclists(MyIntelRing *ring, MyIntelRingCallbacks *cb);

/*!
 * @brief  Reset ring software cursors + LRC image head/tail after engine reset.
 *         Must be called under the same lock as the reset itself. Without it the
 *         software space counter and HW fetch point diverge → ring-full deadlock.
 *
 * @param ring  Ring object (may be NULL — no-op)
 */
void ringResetSoftware(MyIntelRing *ring);

/*!
 * @brief  Get ring's current tail (byte offset within ring)
 *
 * @param ring  Ring object
 * @return tail in bytes
 */
static inline uint32_t ringGetTail(const MyIntelRing *ring)
{
    return ring->tail;
}

/*!
 * @brief  Get ring's current head (byte offset, updated by HW)
 *
 * @param ring  Ring object
 * @return head in bytes
 */
static inline uint32_t ringGetHead(const MyIntelRing *ring)
{
    return ring->head;
}

/*!
 * @brief  Check if ring has pending commands (tail != head)
 *
 * @param ring  Ring object
 * @return true if ring is not empty
 */
static inline bool ringIsBusy(const MyIntelRing *ring)
{
    return ring->tail != ring->head;
}

/*!
 * @brief  Check if ring is initialized
 */
static inline bool ringIsInitialized(const MyIntelRing *ring)
{
    return ring && ring->magic == RING_MAGIC && ring->initialized;
}

/*
 * ─────────────────────────────────────────────
 *  High-Level Command Emission Helpers
 * ─────────────────────────────────────────────
 */

/*!
 * @brief  Emit a single MI_NOOP
 *
 * @param ring  Ring object
 * @return true = success
 */
bool ringEmitNOOP(MyIntelRing *ring);

/*!
 * @brief  Emit MI_USER_INTERRUPT (for vblank / command completion)
 *
 * @param ring  Ring object
 * @return true = success
 */
bool ringEmitUserInterrupt(MyIntelRing *ring);

/* Gen9 XY_FAST_COPY_BLT (10-dw form, linear 32bpp) — present surface to scanout */
bool ringEmitSurfacePresent(MyIntelRing *bcs,
                            uint32_t dstGgtt, uint32_t dstPitchBytes,
                            uint32_t srcGgtt, uint32_t srcPitchBytes,
                            uint16_t wPx, uint16_t hPx);

/*!
 * @brief  Emit MI_FLUSH_DW — flush GPU caches
 *
 * @param ring  Ring object
 * @param flushGFX   GFX cache flush
 * @param flushMedia Media cache flush
 * @return true = success
 */
bool ringEmitFlushDW(MyIntelRing *ring, bool flushGFX, bool flushMedia);

/*!
 * @brief  Emit raw dwords into ring (for custom commands)
 *
 * @param ring    Ring object
 * @param cmds    Array of dwords
 * @param dwords  Number of dwords
 * @return true = success
 */
bool ringEmitRaw(MyIntelRing *ring, const uint32_t *cmds, uint32_t dwords);

/*!
 * @brief  Emit GEN9_XY_FAST_COPY_BLT (10-dw) + MI_BATCH_BUFFER_END into a
 *         client batch buffer. BCS blit proof — copies a page of GPU
 *         memory src→dst (identity PPGTT addresses), i915 emit_copy layout.
 *
 * @param dst  Dword buffer (batch cpuAddr); must hold >= 12 dwords
 * @param dstAddr  Destination GGTT identity address (page aligned)
 * @param srcAddr  Source GGTT identity address (page aligned)
 * @param bytes    Copy size (PAGE_SIZE multiple)
 * @return dwords written (12 = 10-dw copy + BB_END + NOOP pad), 0 = error
 */
uint32_t emitBcsBlitCopy(uint32_t *dst, uint32_t dstAddr,
                         uint32_t srcAddr, uint32_t bytes);

/*!
 * @brief  Emit MI_MATH proof batch: LRI(1) CS_GPR0 + LRI(1) CS_GPR1,
 *         MI_MATH(4) [LOAD SRCA←GPR0, LOAD SRCB←GPR1, ADD, STORE GPR0←ACCU],
 *         MI_SRM_GEN8 CS_GPR0→storeAddr, MI_BATCH_BUFFER_END.
 *         Proves the RCS ALU executed (GPR0 snapshot = A+B).
 *
 * @param dst        Dword buffer (batch cpuAddr); must hold >= 17 dwords
 * @param storeAddr  GGTT identity address for the MI_SRM readback
 * @param operandA   Value loaded into CS_GPR0 (default 0xCAFEBABE)
 * @param operandB   Value loaded into CS_GPR1 (default 1)
 * @return dwords written (17 = 16 + BB_END), 0 = error
 */
uint32_t emitMiMathProof(uint32_t *dst, uint32_t storeAddr,
                         uint32_t operandA, uint32_t operandB);

/*!
 * @brief  Emit PIPE_CONTROL flush proof: PC1 (gen12 HDC_PIPELINE_FLUSH +
 *         RCS cache flush set), PC2 (QW_WRITE store of magic to storeAddr),
 *         MI_USER_INTERRUPT, MI_BATCH_BUFFER_END. Proves the RCS executed
 *         a full pipeline flush (gen12_emit_fini_breadcrumb_rcs parity).
 *
 * @param dst        Dword buffer (batch cpuAddr); must hold >= 15 dwords
 * @param storeAddr  GGTT identity address for the QW_WRITE (page/8 aligned)
 * @param magic      Value stored by the QW_WRITE (default 0xCAFEBABE)
 * @return dwords written (15 = 6+6+1+1 + NOOP pad), 0 = error
 */
uint32_t emitPipeControlFlush(uint32_t *dst, uint32_t storeAddr, uint32_t magic);

/*!
 * @brief  Emit a Gen12 hardware breadcrumb seqno to the HWSP page.
 *         Writes seqno to HWSP (offset 0x80 / dword 0x20) with PIPE_CONTROL
 *         flush + MI_USER_INTERRUPT.
 *
 * @param dst          Dword buffer (batch cpuAddr); must hold >= 15 dwords
 * @param hwspGttAddr  GGTT address of the HWSP page
 * @param seqno        Hardware seqno to write
 * @return dwords written (15), 0 = error
 */
uint32_t emitBreadcrumbSeqno(uint32_t *dst, uint32_t hwspGttAddr, uint32_t seqno);

/*!
 * @brief  Allocate + populate the identity-map PPGTT (PD + 128 PTs).
 *         Must run BEFORE lrcBuildContext (PDP0 phys goes into LRC slots).
 *
 * @param ring  Ring object
 * @param cb    Callbacks (gemAlloc/gemGetPagesPhys/gemGetOffset/gemGetVAddr)
 * @return true = success
 */
bool lrcAllocPPGTT(MyIntelRing *ring, const MyIntelRingCallbacks *cb);

/*!
 * @brief  Map a batch buffer's physical pages into the PPGTT window at
 *         VA = GGTT offset (identity). Writes PTEs from pagesPhys[].
 *
 * @param ring       Ring object
 * @param batchGGTT  GGTT offset of batch (its PPGTT VA in identity map)
 * @param pagesPhys  Per-page physical addresses of the batch buffer
 * @param pageCount  Number of pages
 * @return true = success
 */
bool lrcMapBatchPages(MyIntelRing *ring, uint32_t batchGGTT,
                      const uint64_t *pagesPhys, uint32_t pageCount);

/*!
 * @brief  Emit MI_BATCH_BUFFER_START (4 dwords, i915 gen8_emit_bb_start).
 *         [ARB_ENABLE][BB_START_GEN8|NONSEC][lo][hi] — qword aligned.
 *
 * @param ring  Ring object
 * @param va    Batch PPGTT VA (= GGTT offset in identity map)
 * @return true = success
 */
bool ringEmitBatchStart(MyIntelRing *ring, uint32_t va);

#endif /* __MY_INTEL_RING_HPP__ */
