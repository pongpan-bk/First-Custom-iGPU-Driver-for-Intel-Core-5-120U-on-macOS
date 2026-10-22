/*===========================================================================
 *  IntelFramebuffer.cpp
 *  Hackintosh Kext — Display Output & Interrupt Management Implementation
 *
 * :
 *    - Interrupt Initialization (clear → mask → enable)
 * - Vblank Interrupt Registration IOInterruptEventSource
 *    - Pipe Management
 *
 * Interrupt Lifecycle:
 * 1. initInterrupts() — Mask + Clear Register
 * 2. installInterruptHandlers() — IOInterruptEventSource
 * 3. enableVblankInterrupt() — Vblank IER
 * 4. handleInterrupt() — Callback Interrupt
 * 5. disableInterrupts() — + unload
 *
 * Linux i915:
 *    drivers/gpu/drm/i915/i915_irq.c — icl_irq_handler, gen8_irq_handler
 *    drivers/gpu/drm/i915/display/intel_display_power.c
 *///=========================================================================

#include "IntelFramebuffer.hpp"

/*
 * Debug Logging Macro — IOLog MyIntelGPU
 */
#define FBLog(fmt, ...) \
    IOLog("IntelFB: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

// #define EXTRA_FB_DEBUG /* log interrupt */

/*
 * Register Macro IOKit Runtime
 */
#define super IOService
OSDefineMetaClassAndStructors(IntelFramebuffer, IOService)

#pragma mark -
#pragma mark - init / free

bool IntelFramebuffer::init(OSDictionary *dict)
{
    if (!super::init(dict)) {
        return false;
    }

    _ox_prnt              = NULL;
    _fx_rdyq     = false;
    _f_m1_pmask        = 0;
    _0x_hpdv       = 0;
    fMiscIrqCount        = 0;
    fHpdIrqCount         = 0;
    fSdeIrqCount         = 0;
    fVblankIrqCount      = 0;
    fUnderrunIrqCount    = 0;

    FBLog("init() — OK");
    return true;
}

void IntelFramebuffer::free()
{
    FBLog("free()");
    _ox_prnt = NULL;
    super::free();
}

#pragma mark -
#pragma mark - Interrupt Register Helpers

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t IntelFramebuffer::pipeBase(uint32_t pipe)
 *
 * MMIO base address Pipe Gen12+ layout:
 *    Pipe A..D: consecutive pages starting at PIPE_A_BASE,
 *    stride = PIPE_A_BASE page step (see pipeBase())
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t IntelFramebuffer::pipeBase(uint32_t pipe)
{
    if (pipe >= FB_MAX_CRTC) {
        return 0;
    }
    return PIPE_A_BASE + (pipe << 12);
}

uint32_t IntelFramebuffer::getPipeIIR(uint32_t pipe)
{
    uint32_t base = pipeBase(pipe);
    if (base == 0) return 0xFFFFFFFF;
    return _ox_prnt->readReg32(base + PIPE_IIR_OFFSET);
}

uint32_t IntelFramebuffer::getPipeIER(uint32_t pipe)
{
    uint32_t base = pipeBase(pipe);
    if (base == 0) return 0xFFFFFFFF;
    return _ox_prnt->readReg32(base + PIPE_IER_OFFSET);
}

#pragma mark -
#pragma mark - Interrupt Clearing

/*
 * ─────────────────────────────────────────────────────────────────
 *  void IntelFramebuffer::clearAllInterruptRegisters(void)
 *
 * Clears and masks all display engine interrupts.
 * ─────────────────────────────────────────────────────────────────
 */
void IntelFramebuffer::clearAllInterruptRegisters(void)
{
    FBLog("Clearing all pending display interrupts...");

    if (!_ox_prnt || !_ox_prnt->getRegs() || !_ox_prnt->isValidRegs()) {
        FBLog("ERROR: parent MMIO regs not in kernel space (fRegs=%p)",
              _ox_prnt ? (void*)_ox_prnt->getRegs() : NULL);
        return;
    }

    /*
     * Step 1a: Clear Misc Display Interrupts (GEN11_DE_MISC_IIR)
     */
    uint32_t _0x_r91 = _ox_prnt->readReg32(GEN11_DE_MISC_IIR);
    if (_0x_r91 != 0) {
        FBLog("  GEN11_DE_MISC_IIR = 0x%08X (clearing)", _0x_r91);
        _ox_prnt->writeReg32(GEN11_DE_MISC_IIR, _0x_r91);
        (void)_ox_prnt->readReg32(GEN11_DE_MISC_IIR);
    }
    _ox_prnt->writeReg32(GEN11_DE_MISC_IMR, 0xFFFFFFFF);
    _ox_prnt->writeReg32(GEN11_DE_MISC_IER, 0);

    /*
     * Step 1b: Clear & Mask North Hotplug Interrupts (GEN11_DE_HPD_IIR)
     */
    uint32_t hpdIIR = _ox_prnt->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        FBLog("  GEN11_DE_HPD_IIR = 0x%08X (clearing)", hpdIIR);
        _ox_prnt->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
        (void)_ox_prnt->readReg32(GEN11_DE_HPD_IIR);
    }
    _ox_prnt->writeReg32(GEN11_DE_HPD_IMR, 0xFFFFFFFF);
    _ox_prnt->writeReg32(GEN11_DE_HPD_IER, 0);

    /*
     * Step 1c: Clear & Mask South Display Engine Interrupts (SDE_IIR)
     */
    uint32_t _0x_r92 = _ox_prnt->readReg32(SDE_IIR);
    if (_0x_r92 != 0) {
        FBLog("  SDE_IIR = 0x%08X (clearing)", _0x_r92);
        _ox_prnt->writeReg32(SDE_IIR, _0x_r92);
        (void)_ox_prnt->readReg32(SDE_IIR);
    }
    _ox_prnt->writeReg32(SDE_IMR, 0xFFFFFFFF);
    _ox_prnt->writeReg32(SDE_IER, 0);

    /*
     * Step 2: Clear Pipe Interrupts (PIPEA/B/C_IIR)
     */
    for (uint32_t pipe = 0; pipe < FB_MAX_CRTC; pipe++) {
        uint32_t base = pipeBase(pipe);
        if (base == 0) continue;

        uint32_t _0x_i71 = _ox_prnt->readReg32(base + PIPE_IIR_OFFSET);
        if (_0x_i71 != 0) {
            FBLog("  Pipe %c IIR = 0x%08X (clearing)", 'A' + pipe, _0x_i71);
            _ox_prnt->writeReg32(base + PIPE_IIR_OFFSET, _0x_i71);
        }

        _ox_prnt->writeReg32(base + PIPE_IMR_OFFSET, 0xFFFFFFFF);
        _ox_prnt->writeReg32(base + PIPE_IER_OFFSET, 0);
    }

    /*
     * Step 3: Disable Master Interrupt
     */
    uint32_t masterCtl = _ox_prnt->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl &= ~GEN11_DE_MASTER_ENABLE;
    _ox_prnt->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    /*
     * Step 4: Disable Display Interrupt Control
     */
    _ox_prnt->writeReg32(DISP_INT_CTL, 0);

    FBLog("All display interrupts cleared and masked");
}

#pragma mark -
#pragma mark - Vblank Interrupt Enable/Disable

void IntelFramebuffer::enableVblankInterrupt(uint32_t pipe)
{
    if (pipe >= FB_MAX_CRTC) {
        FBLog("ERROR: enableVblankInterrupt: invalid pipe %u", pipe);
        return;
    }

    uint32_t base = pipeBase(pipe);
    if (base == 0) return;

    FBLog("Enabling Vblank interrupt on Pipe %c", 'A' + pipe);

    uint32_t _0x_m81 = _ox_prnt->readReg32(base + PIPE_IMR_OFFSET);
    _0x_m81 &= ~GEN11_PIPE_VBLANK;
    _ox_prnt->writeReg32(base + PIPE_IMR_OFFSET, _0x_m81);

    uint32_t ier = _ox_prnt->readReg32(base + PIPE_IER_OFFSET);
    ier |= GEN11_PIPE_VBLANK;
    _ox_prnt->writeReg32(base + PIPE_IER_OFFSET, ier);

    uint32_t masterCtl = _ox_prnt->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl |= GEN11_DE_MASTER_ENABLE;
    _ox_prnt->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    uint32_t dispCtl = _ox_prnt->readReg32(DISP_INT_CTL);
    dispCtl |= DISP_INT_ENABLE;
    _ox_prnt->writeReg32(DISP_INT_CTL, dispCtl);

    _f_m1_pmask |= (1U << pipe);

    FBLog("Vblank enabled on Pipe %c (IER=0x%08X, IMR=0x%08X)",
          'A' + pipe,
          _ox_prnt->readReg32(base + PIPE_IER_OFFSET),
          _ox_prnt->readReg32(base + PIPE_IMR_OFFSET));
}

void IntelFramebuffer::disableVblankInterrupt(uint32_t pipe)
{
    if (pipe >= FB_MAX_CRTC) {
        return;
    }

    uint32_t base = pipeBase(pipe);
    if (base == 0) return;

    FBLog("Disabling Vblank interrupt on Pipe %c", 'A' + pipe);

    uint32_t _0x_m81 = _ox_prnt->readReg32(base + PIPE_IMR_OFFSET);
    _0x_m81 |= GEN11_PIPE_VBLANK;
    _ox_prnt->writeReg32(base + PIPE_IMR_OFFSET, _0x_m81);

    uint32_t ier = _ox_prnt->readReg32(base + PIPE_IER_OFFSET);
    ier &= ~GEN11_PIPE_VBLANK;
    _ox_prnt->writeReg32(base + PIPE_IER_OFFSET, ier);

    uint32_t _0x_i71 = _ox_prnt->readReg32(base + PIPE_IIR_OFFSET);
    if (_0x_i71 & GEN11_PIPE_VBLANK) {
        _ox_prnt->writeReg32(base + PIPE_IIR_OFFSET, GEN11_PIPE_VBLANK);
    }

    _f_m1_pmask &= ~(1U << pipe);

    FBLog("Vblank disabled on Pipe %c", 'A' + pipe);
}

#pragma mark -
#pragma mark - Hotplug Interrupt Enable/Disable

void IntelFramebuffer::enableHotplugInterrupt(uint32_t ddiMask)
{
    if (!_ox_prnt || !_ox_prnt->getRegs() || !_ox_prnt->isValidRegs()) {
        return;
    }

    FBLog("Enabling Hotplug interrupts (ddiMask=0x%08X)...", ddiMask);

    /* Enable South hotplug detection pins (PCH DDI detection - DDI A only for internal eDP) */
    uint32_t shotplug = _ox_prnt->readReg32(SHOTPLUG_CTL_DDI);
    shotplug |= SHOTPLUG_DDI_A_HPD_ENABLE;
    _ox_prnt->writeReg32(SHOTPLUG_CTL_DDI, shotplug);

    /* Unmask & Enable North Display HPD interrupts */
    uint32_t hpdImr = _ox_prnt->readReg32(GEN11_DE_HPD_IMR);
    hpdImr &= ~ddiMask;
    _ox_prnt->writeReg32(GEN11_DE_HPD_IMR, hpdImr);

    uint32_t hpdIer = _ox_prnt->readReg32(GEN11_DE_HPD_IER);
    hpdIer |= ddiMask;
    _ox_prnt->writeReg32(GEN11_DE_HPD_IER, hpdIer);

    /* Ensure Master Display Interrupt is enabled */
    uint32_t masterCtl = _ox_prnt->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl |= GEN11_DE_MASTER_ENABLE;
    _ox_prnt->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    FBLog("Hotplug interrupts armed (North IER=0x%08X)",
          _ox_prnt->readReg32(GEN11_DE_HPD_IER));
}

void IntelFramebuffer::disableHotplugInterrupt(void)
{
    if (!_ox_prnt || !_ox_prnt->getRegs() || !_ox_prnt->isValidRegs()) {
        return;
    }

    FBLog("Disabling Hotplug interrupts...");

    /* Mask North HPD */
    _ox_prnt->writeReg32(GEN11_DE_HPD_IMR, 0xFFFFFFFF);
    _ox_prnt->writeReg32(GEN11_DE_HPD_IER, 0);

    uint32_t hpdIIR = _ox_prnt->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        _ox_prnt->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
    }

    /* Mask South HPD */
    _ox_prnt->writeReg32(SDE_IMR, 0xFFFFFFFF);
    _ox_prnt->writeReg32(SDE_IER, 0);

    uint32_t _0x_r92 = _ox_prnt->readReg32(SDE_IIR);
    if (_0x_r92 != 0) {
        _ox_prnt->writeReg32(SDE_IIR, _0x_r92);
    }

    FBLog("Hotplug interrupts disabled");
}

#pragma mark -
#pragma mark - initInterrupts

bool IntelFramebuffer::initInterrupts(MyIntelGPU *parent)
{
    FBLog("=== InitInterrupts: START ===");
    mygpuProgress("ifb:init-irq-enter");

    if (!parent) {
        FBLog("ERROR: parent is NULL");
        return false;
    }

    _ox_prnt = parent;

    if (!_ox_prnt->getRegs() || !_ox_prnt->isValidRegs()) {
        FBLog("ERROR: parent MMIO regs not in kernel space (fRegs=%p)",
              (void*)_ox_prnt->getRegs());
        return false;
    }

    /*
     * Step 1: Clear all pending interrupts immediately
     */
    FBLog("  Step 1: Clearing pending interrupts...");
    clearAllInterruptRegisters();
    FBLog("  Step 1: Clear OK");

    /*
     * Step 2: Enable Hotplug detection for primary internal DDI (DDI A)
     */
    enableHotplugInterrupt(GEN11_DE_DDI_A_HOTPLUG);

    _fx_rdyq = true;
    FBLog("=== InitInterrupts: DONE ===");
    mygpuProgress("ifb:init-irq-done");
    return true;
}

#pragma mark -
#pragma mark - disableInterrupts

void IntelFramebuffer::disableInterrupts(void)
{
    FBLog("disableInterrupts: START");

    _fx_rdyq = false;
    clearAllInterruptRegisters();
    _f_m1_pmask = 0;

    FBLog("disableInterrupts: DONE");
}

#pragma mark -
#pragma mark - Interrupt Handler

void IntelFramebuffer::handleInterrupt(void)
{
    if (!_fx_rdyq || !_ox_prnt || !_ox_prnt->getRegs() || !_ox_prnt->isValidRegs()) {
        return;
    }

    bool handledAny = false;

    /*
     * 1. Display Engine Misc Interrupts
     */
    uint32_t _0x_r91 = _ox_prnt->readReg32(GEN11_DE_MISC_IIR);
    if (_0x_r91 != 0) {
        fMiscIrqCount++;
        _ox_prnt->writeReg32(GEN11_DE_MISC_IIR, _0x_r91);
        handledAny = true;
#ifdef EXTRA_FB_DEBUG
        FBLog("handleInterrupt: DE_MISC_IIR = 0x%08X", _0x_r91);
#endif
    }

    /*
     * 2. North Display Engine Hotplug (HPD)
     */
    uint32_t hpdIIR = _ox_prnt->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        fHpdIrqCount++;
        _ox_prnt->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
        _0x_hpdv = hpdIIR;
        handledAny = true;
        FBLog("handleInterrupt: HPD event 0x%08X (DDI A=%d, B=%d, C=%d, TC1=%d, TC2=%d)",
              hpdIIR,
              (hpdIIR & GEN11_DE_DDI_A_HOTPLUG) ? 1 : 0,
              (hpdIIR & GEN11_DE_DDI_B_HOTPLUG) ? 1 : 0,
              (hpdIIR & GEN11_DE_DDI_C_HOTPLUG) ? 1 : 0,
              (hpdIIR & GEN11_DE_TC1_HOTPLUG) ? 1 : 0,
              (hpdIIR & GEN11_DE_TC2_HOTPLUG) ? 1 : 0);
    }

    /*
     * 3. South Display Engine Hotplug (PCH SDE)
     */
    uint32_t _0x_r92 = _ox_prnt->readReg32(SDE_IIR);
    if (_0x_r92 != 0) {
        fSdeIrqCount++;
        _ox_prnt->writeReg32(SDE_IIR, _0x_r92);
        _0x_hpdv |= _0x_r92;
        handledAny = true;
        FBLog("handleInterrupt: SDE HPD event 0x%08X", _0x_r92);
    }

    /*
     * 4. Per-Pipe Vblank & Status Interrupts
     */
    for (uint32_t pipe = 0; pipe < FB_MAX_CRTC; pipe++) {
        if (!(_f_m1_pmask & (1U << pipe))) {
            continue;
        }

        uint32_t base = pipeBase(pipe);
        if (base == 0) continue;

        uint32_t _0x_i71 = _ox_prnt->readReg32(base + PIPE_IIR_OFFSET);
        if (_0x_i71 == 0) continue;

        /* W1C clear */
        _ox_prnt->writeReg32(base + PIPE_IIR_OFFSET, _0x_i71);
        handledAny = true;

#ifdef EXTRA_FB_DEBUG
        if (_0x_i71 & GEN11_PIPE_VBLANK) {
            FBLog("  VBLANK on Pipe %c (IIR=0x%08X)", 'A' + pipe, _0x_i71);
        }
        if (_0x_i71 & GEN11_PIPE_FIFO_UNDERRUN) {
            FBLog("  FIFO UNDERRUN on Pipe %c!", 'A' + pipe);
        }
#endif

        if (_0x_i71 & GEN11_PIPE_VBLANK) {
            fVblankIrqCount++;
            _ox_prnt->notifyVblank();
        }
        if (_0x_i71 & GEN11_PIPE_FIFO_UNDERRUN) {
            fUnderrunIrqCount++;
        }
    }

    /*
     * 5. Loop clear safety check
     */
    uint32_t sanity = 5;
    while (sanity--) {
        uint32_t _0x_r93 = _ox_prnt->readReg32(GEN11_DE_MISC_IIR);
        uint32_t checkHPD = _ox_prnt->readReg32(GEN11_DE_HPD_IIR);
        uint32_t _0x_r94 = _ox_prnt->readReg32(SDE_IIR);
        if (_0x_r93 == 0 && checkHPD == 0 && _0x_r94 == 0) break;

        if (_0x_r93) _ox_prnt->writeReg32(GEN11_DE_MISC_IIR, _0x_r93);
        if (checkHPD) _ox_prnt->writeReg32(GEN11_DE_HPD_IIR, checkHPD);
        if (_0x_r94) _ox_prnt->writeReg32(SDE_IIR, _0x_r94);
    }

    /* 2.0.223: a bit that re-asserts immediately after the clear loop =
     * level-triggered storm source (rate-limited to first 10 hits). */
    uint32_t stuckMisc = _ox_prnt->readReg32(GEN11_DE_MISC_IIR);
    uint32_t stuckHpd  = _ox_prnt->readReg32(GEN11_DE_HPD_IIR);
    uint32_t stuckSde  = _ox_prnt->readReg32(SDE_IIR);
    if (stuckMisc != 0 || stuckHpd != 0 || stuckSde != 0) {
        static uint32_t sStuckLogs = 0;
        if (sStuckLogs < 10) {
            FBLog("handleInterrupt: ** IIR STUCK after clear ** misc=0x%08X "
                  "hpd=0x%08X sde=0x%08X — storm source?", stuckMisc, stuckHpd, stuckSde);
            sStuckLogs++;
        }
    }
}

void IntelFramebuffer::dumpDiagnostics(void) const
{
    IOLog("MyIntelGPU: IntelFB diag: vblank=%llu underrun=%llu misc=%llu "
          "hpd=%llu sde=%llu lastHPD=0x%08X ready=%d pipes=0x%02X\n",
          fVblankIrqCount, fUnderrunIrqCount, fMiscIrqCount, fHpdIrqCount,
          fSdeIrqCount, _0x_hpdv, _fx_rdyq ? 1 : 0, _f_m1_pmask);
}
