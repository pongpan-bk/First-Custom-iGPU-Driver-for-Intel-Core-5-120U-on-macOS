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

    fParent              = NULL;
    fInterruptsReady     = false;
    fEnabledPipes        = 0;
    fLastHPDStatus       = 0;
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
    fParent = NULL;
    super::free();
}

#pragma mark -
#pragma mark - Interrupt Register Helpers

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t IntelFramebuffer::pipeBase(uint32_t pipe)
 *
 * MMIO base address Pipe Gen12+ layout:
 *    Pipe A: 0x70000
 *    Pipe B: 0x71000
 *    Pipe C: 0x72000
 *    Pipe D: 0x73000 (Gen12+)
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t IntelFramebuffer::pipeBase(uint32_t pipe)
{
    if (pipe >= FB_MAX_CRTC) {
        return 0;
    }
    return PIPE_A_BASE + (pipe * 0x1000);
}

uint32_t IntelFramebuffer::getPipeIIR(uint32_t pipe)
{
    uint32_t base = pipeBase(pipe);
    if (base == 0) return 0xFFFFFFFF;
    return fParent->readReg32(base + PIPE_IIR_OFFSET);
}

uint32_t IntelFramebuffer::getPipeIER(uint32_t pipe)
{
    uint32_t base = pipeBase(pipe);
    if (base == 0) return 0xFFFFFFFF;
    return fParent->readReg32(base + PIPE_IER_OFFSET);
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

    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        FBLog("ERROR: parent MMIO regs not in kernel space (fRegs=%p)",
              fParent ? (void*)fParent->getRegs() : NULL);
        return;
    }

    /*
     * Step 1a: Clear Misc Display Interrupts (GEN11_DE_MISC_IIR)
     */
    uint32_t miscIIR = fParent->readReg32(GEN11_DE_MISC_IIR);
    if (miscIIR != 0) {
        FBLog("  GEN11_DE_MISC_IIR = 0x%08X (clearing)", miscIIR);
        fParent->writeReg32(GEN11_DE_MISC_IIR, miscIIR);
        (void)fParent->readReg32(GEN11_DE_MISC_IIR);
    }
    fParent->writeReg32(GEN11_DE_MISC_IMR, 0xFFFFFFFF);
    fParent->writeReg32(GEN11_DE_MISC_IER, 0);

    /*
     * Step 1b: Clear & Mask North Hotplug Interrupts (GEN11_DE_HPD_IIR)
     */
    uint32_t hpdIIR = fParent->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        FBLog("  GEN11_DE_HPD_IIR = 0x%08X (clearing)", hpdIIR);
        fParent->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
        (void)fParent->readReg32(GEN11_DE_HPD_IIR);
    }
    fParent->writeReg32(GEN11_DE_HPD_IMR, 0xFFFFFFFF);
    fParent->writeReg32(GEN11_DE_HPD_IER, 0);

    /*
     * Step 1c: Clear & Mask South Display Engine Interrupts (SDE_IIR)
     */
    uint32_t sdeIIR = fParent->readReg32(SDE_IIR);
    if (sdeIIR != 0) {
        FBLog("  SDE_IIR = 0x%08X (clearing)", sdeIIR);
        fParent->writeReg32(SDE_IIR, sdeIIR);
        (void)fParent->readReg32(SDE_IIR);
    }
    fParent->writeReg32(SDE_IMR, 0xFFFFFFFF);
    fParent->writeReg32(SDE_IER, 0);

    /*
     * Step 2: Clear Pipe Interrupts (PIPEA/B/C_IIR)
     */
    for (uint32_t pipe = 0; pipe < FB_MAX_CRTC; pipe++) {
        uint32_t base = pipeBase(pipe);
        if (base == 0) continue;

        uint32_t iir = fParent->readReg32(base + PIPE_IIR_OFFSET);
        if (iir != 0) {
            FBLog("  Pipe %c IIR = 0x%08X (clearing)", 'A' + pipe, iir);
            fParent->writeReg32(base + PIPE_IIR_OFFSET, iir);
        }

        fParent->writeReg32(base + PIPE_IMR_OFFSET, 0xFFFFFFFF);
        fParent->writeReg32(base + PIPE_IER_OFFSET, 0);
    }

    /*
     * Step 3: Disable Master Interrupt
     */
    uint32_t masterCtl = fParent->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl &= ~GEN11_DE_MASTER_ENABLE;
    fParent->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    /*
     * Step 4: Disable Display Interrupt Control
     */
    fParent->writeReg32(DISP_INT_CTL, 0);

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

    uint32_t imr = fParent->readReg32(base + PIPE_IMR_OFFSET);
    imr &= ~GEN11_PIPE_VBLANK;
    fParent->writeReg32(base + PIPE_IMR_OFFSET, imr);

    uint32_t ier = fParent->readReg32(base + PIPE_IER_OFFSET);
    ier |= GEN11_PIPE_VBLANK;
    fParent->writeReg32(base + PIPE_IER_OFFSET, ier);

    uint32_t masterCtl = fParent->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl |= GEN11_DE_MASTER_ENABLE;
    fParent->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    uint32_t dispCtl = fParent->readReg32(DISP_INT_CTL);
    dispCtl |= DISP_INT_ENABLE;
    fParent->writeReg32(DISP_INT_CTL, dispCtl);

    fEnabledPipes |= (1U << pipe);

    FBLog("Vblank enabled on Pipe %c (IER=0x%08X, IMR=0x%08X)",
          'A' + pipe,
          fParent->readReg32(base + PIPE_IER_OFFSET),
          fParent->readReg32(base + PIPE_IMR_OFFSET));
}

void IntelFramebuffer::disableVblankInterrupt(uint32_t pipe)
{
    if (pipe >= FB_MAX_CRTC) {
        return;
    }

    uint32_t base = pipeBase(pipe);
    if (base == 0) return;

    FBLog("Disabling Vblank interrupt on Pipe %c", 'A' + pipe);

    uint32_t imr = fParent->readReg32(base + PIPE_IMR_OFFSET);
    imr |= GEN11_PIPE_VBLANK;
    fParent->writeReg32(base + PIPE_IMR_OFFSET, imr);

    uint32_t ier = fParent->readReg32(base + PIPE_IER_OFFSET);
    ier &= ~GEN11_PIPE_VBLANK;
    fParent->writeReg32(base + PIPE_IER_OFFSET, ier);

    uint32_t iir = fParent->readReg32(base + PIPE_IIR_OFFSET);
    if (iir & GEN11_PIPE_VBLANK) {
        fParent->writeReg32(base + PIPE_IIR_OFFSET, GEN11_PIPE_VBLANK);
    }

    fEnabledPipes &= ~(1U << pipe);

    FBLog("Vblank disabled on Pipe %c", 'A' + pipe);
}

#pragma mark -
#pragma mark - Hotplug Interrupt Enable/Disable

void IntelFramebuffer::enableHotplugInterrupt(uint32_t ddiMask)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return;
    }

    FBLog("Enabling Hotplug interrupts (ddiMask=0x%08X)...", ddiMask);

    /* Enable South hotplug detection pins (PCH DDI detection - DDI A only for internal eDP) */
    uint32_t shotplug = fParent->readReg32(SHOTPLUG_CTL_DDI);
    shotplug |= SHOTPLUG_DDI_A_HPD_ENABLE;
    fParent->writeReg32(SHOTPLUG_CTL_DDI, shotplug);

    /* Unmask & Enable North Display HPD interrupts */
    uint32_t hpdImr = fParent->readReg32(GEN11_DE_HPD_IMR);
    hpdImr &= ~ddiMask;
    fParent->writeReg32(GEN11_DE_HPD_IMR, hpdImr);

    uint32_t hpdIer = fParent->readReg32(GEN11_DE_HPD_IER);
    hpdIer |= ddiMask;
    fParent->writeReg32(GEN11_DE_HPD_IER, hpdIer);

    /* Ensure Master Display Interrupt is enabled */
    uint32_t masterCtl = fParent->readReg32(GEN11_DE_INTERRUPT_CONTROL);
    masterCtl |= GEN11_DE_MASTER_ENABLE;
    fParent->writeReg32(GEN11_DE_INTERRUPT_CONTROL, masterCtl);

    FBLog("Hotplug interrupts armed (North IER=0x%08X)",
          fParent->readReg32(GEN11_DE_HPD_IER));
}

void IntelFramebuffer::disableHotplugInterrupt(void)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return;
    }

    FBLog("Disabling Hotplug interrupts...");

    /* Mask North HPD */
    fParent->writeReg32(GEN11_DE_HPD_IMR, 0xFFFFFFFF);
    fParent->writeReg32(GEN11_DE_HPD_IER, 0);

    uint32_t hpdIIR = fParent->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        fParent->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
    }

    /* Mask South HPD */
    fParent->writeReg32(SDE_IMR, 0xFFFFFFFF);
    fParent->writeReg32(SDE_IER, 0);

    uint32_t sdeIIR = fParent->readReg32(SDE_IIR);
    if (sdeIIR != 0) {
        fParent->writeReg32(SDE_IIR, sdeIIR);
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

    fParent = parent;

    if (!fParent->getRegs() || !fParent->isValidRegs()) {
        FBLog("ERROR: parent MMIO regs not in kernel space (fRegs=%p)",
              (void*)fParent->getRegs());
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

    fInterruptsReady = true;
    FBLog("=== InitInterrupts: DONE ===");
    mygpuProgress("ifb:init-irq-done");
    return true;
}

#pragma mark -
#pragma mark - disableInterrupts

void IntelFramebuffer::disableInterrupts(void)
{
    FBLog("disableInterrupts: START");

    fInterruptsReady = false;
    clearAllInterruptRegisters();
    fEnabledPipes = 0;

    FBLog("disableInterrupts: DONE");
}

#pragma mark -
#pragma mark - Interrupt Handler

void IntelFramebuffer::handleInterrupt(void)
{
    if (!fInterruptsReady || !fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return;
    }

    bool handledAny = false;

    /*
     * 1. Display Engine Misc Interrupts
     */
    uint32_t miscIIR = fParent->readReg32(GEN11_DE_MISC_IIR);
    if (miscIIR != 0) {
        fMiscIrqCount++;
        fParent->writeReg32(GEN11_DE_MISC_IIR, miscIIR);
        handledAny = true;
#ifdef EXTRA_FB_DEBUG
        FBLog("handleInterrupt: DE_MISC_IIR = 0x%08X", miscIIR);
#endif
    }

    /*
     * 2. North Display Engine Hotplug (HPD)
     */
    uint32_t hpdIIR = fParent->readReg32(GEN11_DE_HPD_IIR);
    if (hpdIIR != 0) {
        fHpdIrqCount++;
        fParent->writeReg32(GEN11_DE_HPD_IIR, hpdIIR);
        fLastHPDStatus = hpdIIR;
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
    uint32_t sdeIIR = fParent->readReg32(SDE_IIR);
    if (sdeIIR != 0) {
        fSdeIrqCount++;
        fParent->writeReg32(SDE_IIR, sdeIIR);
        fLastHPDStatus |= sdeIIR;
        handledAny = true;
        FBLog("handleInterrupt: SDE HPD event 0x%08X", sdeIIR);
    }

    /*
     * 4. Per-Pipe Vblank & Status Interrupts
     */
    for (uint32_t pipe = 0; pipe < FB_MAX_CRTC; pipe++) {
        if (!(fEnabledPipes & (1U << pipe))) {
            continue;
        }

        uint32_t base = pipeBase(pipe);
        if (base == 0) continue;

        uint32_t iir = fParent->readReg32(base + PIPE_IIR_OFFSET);
        if (iir == 0) continue;

        /* W1C clear */
        fParent->writeReg32(base + PIPE_IIR_OFFSET, iir);
        handledAny = true;

#ifdef EXTRA_FB_DEBUG
        if (iir & GEN11_PIPE_VBLANK) {
            FBLog("  VBLANK on Pipe %c (IIR=0x%08X)", 'A' + pipe, iir);
        }
        if (iir & GEN11_PIPE_FIFO_UNDERRUN) {
            FBLog("  FIFO UNDERRUN on Pipe %c!", 'A' + pipe);
        }
#endif

        if (iir & GEN11_PIPE_VBLANK) {
            fVblankIrqCount++;
            fParent->notifyVblank();
        }
        if (iir & GEN11_PIPE_FIFO_UNDERRUN) {
            fUnderrunIrqCount++;
        }
    }

    /*
     * 5. Loop clear safety check
     */
    uint32_t sanity = 5;
    while (sanity--) {
        uint32_t checkIIR = fParent->readReg32(GEN11_DE_MISC_IIR);
        uint32_t checkHPD = fParent->readReg32(GEN11_DE_HPD_IIR);
        uint32_t checkSDE = fParent->readReg32(SDE_IIR);
        if (checkIIR == 0 && checkHPD == 0 && checkSDE == 0) break;

        if (checkIIR) fParent->writeReg32(GEN11_DE_MISC_IIR, checkIIR);
        if (checkHPD) fParent->writeReg32(GEN11_DE_HPD_IIR, checkHPD);
        if (checkSDE) fParent->writeReg32(SDE_IIR, checkSDE);
    }

    /* 2.0.223: a bit that re-asserts immediately after the clear loop =
     * level-triggered storm source (rate-limited to first 10 hits). */
    uint32_t stuckMisc = fParent->readReg32(GEN11_DE_MISC_IIR);
    uint32_t stuckHpd  = fParent->readReg32(GEN11_DE_HPD_IIR);
    uint32_t stuckSde  = fParent->readReg32(SDE_IIR);
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
          fSdeIrqCount, fLastHPDStatus, fInterruptsReady ? 1 : 0, fEnabledPipes);
}
