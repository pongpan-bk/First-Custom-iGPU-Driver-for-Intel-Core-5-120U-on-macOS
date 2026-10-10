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

#pragma mark -
#pragma mark - DP Link Training (Phase 8)

/*
 * ─────────────────────────────────────────────────────────────────
 *  uint32_t IntelFramebuffer::getDDIBase(uint32_t ddiIndex)
 *
 *  Returns the MMIO base address for the specified DDI.
 *  DDI A: 0x64000, DDI B: 0x64100, DDI C: 0x64200
 * ─────────────────────────────────────────────────────────────────
 */
uint32_t IntelFramebuffer::getDDIBase(uint32_t ddiIndex) const
{
    static const uint32_t kDDIBases[3] = { DDI_A_BASE, DDI_B_BASE, DDI_C_BASE };
    if (ddiIndex >= 3) return 0;
    return kDDIBases[ddiIndex];
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::auxReadDPCD(uint32_t ddiIndex, uint32_t address,
 *                                     uint8_t *data, uint32_t length)
 *
 *  Read DPCD via AUX channel transaction.
 *  Reference: intel_dp_aux_native_read / intel_ddi_aux_xfer
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::auxReadDPCD(uint32_t ddiIndex, uint32_t address,
                                   uint8_t *data, uint32_t length)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return false;
    }

    uint32_t ddiBase = getDDIBase(ddiIndex);
    if (ddiBase == 0) {
        FBLog("ERROR: auxReadDPCD: invalid DDI index %u", ddiIndex);
        return false;
    }

    if (length > 16) {
        FBLog("ERROR: auxReadDPCD: length %u > 16 (max AUX payload)", length);
        return false;
    }

    FBLog("AUX READ: DDI %u addr=0x%05X len=%u", ddiIndex, address, length);

    /* Set AUX address */
    uint32_t auxDataReg = ddiBase + DDI_AUX_DATA_OFFSET;
    uint32_t auxCtlReg = ddiBase + DDI_AUX_CTL_OFFSET;

    /* Write address to AUX data registers (little-endian) */
    for (uint32_t i = 0; i < 4; i++) {
        fParent->writeReg32(auxDataReg + i * 4,
                           (address >> (i * 8)) & 0xFF);
    }

    /* Configure AUX control for read */
    uint32_t auxCtl = AUX_CTL_PRECHARGE_4US | AUX_CTL_BIT_CLOCK_2X;
    auxCtl |= ((length - 1) & AUX_CTL_MESSAGE_SIZE_MASK);
    auxCtl |= AUX_CTL_SEND_BUSY;

    fParent->writeReg32(auxCtlReg, auxCtl);

    /* Wait for completion */
    uint32_t timeout = 10000; /* ~10ms */
    while (timeout--) {
        uint32_t status = fParent->readReg32(auxCtlReg);
        if (!(status & AUX_CTL_SEND_BUSY)) {
            if (status & AUX_CTL_DONE) {
                /* Read data from AUX data registers */
                for (uint32_t i = 0; i < length; i++) {
                    uint32_t regVal = fParent->readReg32(auxDataReg + (i / 4) * 4);
                    data[i] = (regVal >> ((i % 4) * 8)) & 0xFF;
                }
                FBLog("AUX READ OK: DDI %u addr=0x%05X data[0]=0x%02X",
                      ddiIndex, address, data[0]);
                return true;
            }
            FBLog("AUX READ FAIL: DDI %u addr=0x%05X status=0x%08X",
                  ddiIndex, address, status);
            return false;
        }
        IODelay(1);
    }

    FBLog("AUX READ TIMEOUT: DDI %u addr=0x%05X", ddiIndex, address);
    return false;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::auxWriteDPCD(uint32_t ddiIndex, uint32_t address,
 *                                      const uint8_t *data, uint32_t length)
 *
 *  Write DPCD via AUX channel transaction.
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::auxWriteDPCD(uint32_t ddiIndex, uint32_t address,
                                    const uint8_t *data, uint32_t length)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return false;
    }

    uint32_t ddiBase = getDDIBase(ddiIndex);
    if (ddiBase == 0) {
        FBLog("ERROR: auxWriteDPCD: invalid DDI index %u", ddiIndex);
        return false;
    }

    if (length > 16) {
        FBLog("ERROR: auxWriteDPCD: length %u > 16", length);
        return false;
    }

    FBLog("AUX WRITE: DDI %u addr=0x%05X len=%u data[0]=0x%02X",
          ddiIndex, address, length, data[0]);

    uint32_t auxDataReg = ddiBase + DDI_AUX_DATA_OFFSET;
    uint32_t auxCtlReg = ddiBase + DDI_AUX_CTL_OFFSET;

    /* Write address to AUX data registers */
    for (uint32_t i = 0; i < 4; i++) {
        fParent->writeReg32(auxDataReg + i * 4,
                           (address >> (i * 8)) & 0xFF);
    }

    /* Write payload data */
    for (uint32_t i = 0; i < length; i++) {
        uint32_t regIdx = i / 4;
        uint32_t shift = (i % 4) * 8;
        uint32_t current = fParent->readReg32(auxDataReg + regIdx * 4);
        current = (current & ~(0xFF << shift)) | ((uint32_t)data[i] << shift);
        fParent->writeReg32(auxDataReg + regIdx * 4, current);
    }

    /* Configure AUX control for write (MOT = 0 for write) */
    uint32_t auxCtl = AUX_CTL_PRECHARGE_4US | AUX_CTL_BIT_CLOCK_2X;
    auxCtl |= ((length - 1) & AUX_CTL_MESSAGE_SIZE_MASK);
    auxCtl |= AUX_CTL_SEND_BUSY;

    fParent->writeReg32(auxCtlReg, auxCtl);

    /* Wait for completion */
    uint32_t timeout = 10000;
    while (timeout--) {
        uint32_t status = fParent->readReg32(auxCtlReg);
        if (!(status & AUX_CTL_SEND_BUSY)) {
            if (status & AUX_CTL_DONE) {
                FBLog("AUX WRITE OK: DDI %u addr=0x%05X", ddiIndex, address);
                return true;
            }
            FBLog("AUX WRITE FAIL: DDI %u addr=0x%05X status=0x%08X",
                  ddiIndex, address, status);
            return false;
        }
        IODelay(1);
    }

    FBLog("AUX WRITE TIMEOUT: DDI %u addr=0x%05X", ddiIndex, address);
    return false;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::configureDDIBuffer(uint32_t ddiIndex,
 *                                             uint8_t linkRate, uint8_t laneCount)
 *
 *  Configure DDI buffer for DP output.
 *  Reference: intel_ddi_init_dp_buf_reg
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::configureDDIBuffer(uint32_t ddiIndex,
                                          uint8_t linkRate, uint8_t laneCount)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return false;
    }

    uint32_t ddiBase = getDDIBase(ddiIndex);
    if (ddiBase == 0) return false;

    uint32_t bufCtlReg = ddiBase + DDI_BUF_CTL_OFFSET;

    FBLog("Configuring DDI %u buffer: rate=0x%02X lanes=%u", ddiIndex, linkRate, laneCount);

    uint32_t bufCtl = DDI_BUF_CTL_ENABLE;
    bufCtl |= (ddiIndex << 28) & DDI_BUF_CTL_DDI_SELECT_MASK;
    bufCtl |= DDI_BUF_TRANS_A; /* Translation select - Pipe A */

    /* Set link rate in buffer control */
    switch (linkRate) {
        case DP_LINK_RATE_RBR: bufCtl |= (0x0 << 24); break; /* RBR */
        case DP_LINK_RATE_HBR: bufCtl |= (0x1 << 24); break; /* HBR */
        case DP_LINK_RATE_HBR2: bufCtl |= (0x2 << 24); break; /* HBR2 */
        case DP_LINK_RATE_HBR3: bufCtl |= (0x3 << 24); break; /* HBR3 */
        default: bufCtl |= (0x1 << 24); break;
    }

    fParent->writeReg32(bufCtlReg, bufCtl);
    IODelay(100); /* Wait for buffer enable */

    FBLog("DDI %u buffer enabled: BUF_CTL=0x%08X", ddiIndex,
          fParent->readReg32(bufCtlReg));
    return true;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::setTrainingPattern(uint32_t ddiIndex,
 *                                             uint32_t pattern,
 *                                             uint8_t voltageSwing,
 *                                             uint8_t preEmphasis,
 *                                             uint8_t laneCount)
 *
 *  Set DP training pattern and lane parameters.
 *  Reference: intel_dp_set_tp / intel_dp_program_link_training
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::setTrainingPattern(uint32_t ddiIndex, uint32_t pattern,
                                          uint8_t voltageSwing, uint8_t preEmphasis,
                                          uint8_t laneCount)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return false;
    }

    uint32_t ddiBase = getDDIBase(ddiIndex);
    if (ddiBase == 0) return false;

    uint32_t tpCtlReg = ddiBase + DP_TP_CTL_OFFSET;

    uint32_t tpCtl = DP_TP_CTL_ENABLE;
    tpCtl |= pattern & DP_TP_CTL_MODE_MASK;
    tpCtl |= (voltageSwing << 23) & DP_TP_CTL_VOLTAGE_SWING_MASK;
    tpCtl |= (preEmphasis << 21) & DP_TP_CTL_PRE_EMPHASIS_MASK;
    tpCtl |= ((laneCount - 1) << DP_TP_CTL_LANE_COUNT_SHIFT) & DP_TP_CTL_LANE_COUNT_MASK;

    fParent->writeReg32(tpCtlReg, tpCtl);
    IODelay(100); /* Pattern takes effect */

    FBLog("DDI %u TP_CTL=0x%08X (pat=0x%X vs=%u pe=%u lanes=%u)",
          ddiIndex, tpCtl, pattern, voltageSwing, preEmphasis, laneCount);
    return true;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::waitForTrainingStatus(uint32_t ddiIndex,
 *                                                uint32_t statusMask,
 *                                                uint32_t timeoutUS)
 *
 *  Wait for DP training status bits (CR/EQ/Symbol Lock).
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::waitForTrainingStatus(uint32_t ddiIndex,
                                             uint32_t statusMask,
                                             uint32_t timeoutUS)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        return false;
    }

    uint32_t ddiBase = getDDIBase(ddiIndex);
    if (ddiBase == 0) return false;

    uint32_t tpStatusReg = ddiBase + DP_TP_STATUS_OFFSET;

    FBLog("DDI %u waiting for status 0x%08X (timeout=%u us)",
          ddiIndex, statusMask, timeoutUS);

    while (timeoutUS > 100) {
        uint32_t status = fParent->readReg32(tpStatusReg);
        if ((status & statusMask) == statusMask) {
            FBLog("DDI %u status achieved: TP_STATUS=0x%08X", ddiIndex, status);
            return true;
        }
        IODelay(100);
        timeoutUS -= 100;
    }

    uint32_t finalStatus = fParent->readReg32(tpStatusReg);
    FBLog("DDI %u TIMEOUT waiting for status 0x%08X, got 0x%08X",
          ddiIndex, statusMask, finalStatus);
    return false;
}

/*
 * ─────────────────────────────────────────────────────────────────
 *  bool IntelFramebuffer::trainDPLink(uint32_t ddiIndex)
 *
 *  Full DP Link Training sequence per DP 1.4 spec.
 *  Reference: intel_dp_link_training / intel_dp_start_link_train
 *
 *  Sequence:
 *  1. Read DPCD caps (revision, max link rate, max lane count)
 *  2. Disable DDI buffer
 *  3. Clock Recovery (CR) phase - Training Pattern 1
 *     - Set voltage swing/pre-emphasis per lane from DPCD 0x101/0x102
 *     - Wait for CR_DONE on all lanes
 *  4. Channel Equalization (EQ) phase - Training Pattern 2/3
 *     - Set voltage swing/pre-emphasis per DPCD lane status
 *     - Wait for EQ_DONE on all lanes
 *  5. Symbol Lock - Training Pattern 3/4
 *     - Wait for SYMBOL_LOCK on all lanes
 *  6. Set final link parameters in DPCD
 *  7. Enable DDI buffer with trained parameters
 * ─────────────────────────────────────────────────────────────────
 */
bool IntelFramebuffer::trainDPLink(uint32_t ddiIndex)
{
    if (!fParent || !fParent->getRegs() || !fParent->isValidRegs()) {
        FBLog("ERROR: trainDPLink: parent not ready");
        return false;
    }

    if (ddiIndex >= 3) {
        FBLog("ERROR: trainDPLink: invalid DDI index %u", ddiIndex);
        return false;
    }

    FBLog("=== DP Link Training START: DDI %u ===", ddiIndex);

    /* Step 1: Read DPCD capabilities */
    uint8_t dpcdCaps[16];
    if (!auxReadDPCD(ddiIndex, DPCD_REVISION, dpcdCaps, 8)) {
        FBLog("DP Link Train: Failed to read DPCD caps");
        return false;
    }

    uint8_t maxLinkRate = dpcdCaps[DPCD_MAX_LINK_RATE - DPCD_REVISION];
    uint8_t maxLaneCount = dpcdCaps[DPCD_MAX_LANE_COUNT - DPCD_REVISION] & 0x1F;

    FBLog("DPCD Caps: Rev=0x%02X MaxRate=0x%02X MaxLanes=%u",
          dpcdCaps[0], maxLinkRate, maxLaneCount);

    /* Validate caps */
    if (maxLinkRate < DP_LINK_RATE_RBR || maxLinkRate > DP_LINK_RATE_HBR3) {
        maxLinkRate = DP_LINK_RATE_HBR; /* Safe fallback */
    }
    if (maxLaneCount == 0 || maxLaneCount > 4) {
        maxLaneCount = 4; /* Safe fallback */
    }

    /* Cap at HBR2 for Gen12 (HBR3 requires DSC) */
    if (maxLinkRate > DP_LINK_RATE_HBR2) {
        maxLinkRate = DP_LINK_RATE_HBR2;
    }

    /* Step 2: Disable DDI buffer before training */
    uint32_t ddiBase = getDDIBase(ddiIndex);
    uint32_t bufCtlReg = ddiBase + DDI_BUF_CTL_OFFSET;
    uint32_t bufCtl = fParent->readReg32(bufCtlReg);
    bufCtl &= ~DDI_BUF_CTL_ENABLE;
    fParent->writeReg32(bufCtlReg, bufCtl);
    IODelay(1000); /* Wait for disable */

    /* Step 3: Clock Recovery Phase (Training Pattern 1) */
    FBLog("DP Link Train: Phase 1 - Clock Recovery (TP1)");

    /* Read lane voltage swing/pre-emphasis from DPCD 0x101-0x102 */
    uint8_t laneSettings[4];
    auxReadDPCD(ddiIndex, DPCD_LANE0_1_SET, laneSettings, 4);

    /* Set TP1 with initial voltage swing/pre-emphasis */
    setTrainingPattern(ddiIndex, DP_TP_CTL_MODE_TP1 | DP_TP_CTL_LINK_TRAIN_CR,
                       0, 0, maxLaneCount);

    /* Wait for CR_DONE on all lanes */
    uint32_t crMask = 0;
    for (uint8_t i = 0; i < maxLaneCount; i++) {
        crMask |= (1U << i);
    }
    if (!waitForTrainingStatus(ddiIndex, crMask, 500000)) { /* 500ms timeout */
        FBLog("DP Link Train: CR phase timeout");
        return false;
    }

    /* Step 4: Channel Equalization Phase (Training Pattern 2) */
    FBLog("DP Link Train: Phase 2 - Channel Equalization (TP2)");

    setTrainingPattern(ddiIndex, DP_TP_CTL_MODE_TP2 | DP_TP_CTL_LINK_TRAIN_EQ,
                       0, 0, maxLaneCount);

    uint32_t eqMask = 0;
    for (uint8_t i = 0; i < maxLaneCount; i++) {
        eqMask |= (1U << (8 + i));
    }
    if (!waitForTrainingStatus(ddiIndex, eqMask, 500000)) {
        FBLog("DP Link Train: EQ phase timeout");
        /* Try TP3 */
        setTrainingPattern(ddiIndex, DP_TP_CTL_MODE_TP3 | DP_TP_CTL_LINK_TRAIN_EQ,
                           0, 0, maxLaneCount);
        if (!waitForTrainingStatus(ddiIndex, eqMask, 500000)) {
            FBLog("DP Link Train: EQ phase timeout (TP3 also failed)");
            return false;
        }
    }

    /* Step 5: Symbol Lock (Training Pattern 3) */
    FBLog("DP Link Train: Phase 3 - Symbol Lock (TP3)");

    setTrainingPattern(ddiIndex, DP_TP_CTL_MODE_TP3 | DP_TP_CTL_LINK_TRAIN_SYMBOL,
                       0, 0, maxLaneCount);

    uint32_t symMask = 0;
    for (uint8_t i = 0; i < maxLaneCount; i++) {
        symMask |= (1U << (16 + i));
    }
    if (!waitForTrainingStatus(ddiIndex, symMask, 500000)) {
        FBLog("DP Link Train: Symbol Lock timeout");
        return false;
    }

    /* Step 6: Write final link parameters to DPCD */
    FBLog("DP Link Train: Writing final link parameters");

    uint8_t linkConfig[2];
    linkConfig[0] = maxLinkRate;
    linkConfig[1] = maxLaneCount | (1U << 7); /* Enhanced framing */
    auxWriteDPCD(ddiIndex, DPCD_LINK_RATE_SET, linkConfig, 2);

    /* Step 7: Enable DDI buffer with trained parameters */
    if (!configureDDIBuffer(ddiIndex, maxLinkRate, maxLaneCount)) {
        FBLog("DP Link Train: Failed to configure DDI buffer");
        return false;
    }

    /* Turn off training pattern */
    setTrainingPattern(ddiIndex, DP_TP_CTL_LINK_TRAIN_OFF, 0, 0, maxLaneCount);

    FBLog("=== DP Link Training SUCCESS: DDI %u rate=0x%02X lanes=%u ===",
          ddiIndex, maxLinkRate, maxLaneCount);
    return true;
}
