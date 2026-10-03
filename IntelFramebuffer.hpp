/*===========================================================================
 *  IntelFramebuffer.hpp
 *  Hackintosh Kext — Display Output & Interrupt Management
 *
 * Display Pipeline, Vblank Interrupt,
 * Framebuffer Output MyIntelGPU
 * responsibility
 *
 * MyIntelGPU* parent MMIO ( getRegs(),
 * getMMIOMap(), readReg32/writeReg32) IOPCIDevice
 * Interrupt
 *
 * Linux i915:
 *    drivers/gpu/drm/i915/display/intel_display.c
 *    drivers/gpu/drm/i915/i915_irq.c
 *///=========================================================================

#ifndef __INTEL_FRAMEBUFFER_HPP__
#define __INTEL_FRAMEBUFFER_HPP__

#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include "MyIntelGPU.hpp"

#ifndef FB_MAX_CRTC
#define FB_MAX_CRTC 3
#endif

/*
 * ─────────────────────────────────────────────
 *  Gen12+ Display Interrupt Register Offsets
 * ─────────────────────────────────────────────
 * : i915_reg.h — GEN11_DE_INTERRUPT_*, PIPEA_*
 *
 *  Master Control:
 *    GEN11_DE_INTERRUPT_CONTROL (0x44300):
 *      bit 31: Display Engine Interrupt Master Enable
 *
 *  Display Interrupt Control:
 *    DISP_INT_CTL (0x44200):
 *      bit 0: Enable Display Interrupts
 *
 *  Misc Display Interrupts:
 *    GEN11_DE_MISC_IER (0x44468): Interrupt Enable
 *    GEN11_DE_MISC_IIR (0x4446C): Interrupt Identity (write 1 to clear)
 *    GEN11_DE_MISC_IMR (0x44470): Interrupt Mask
 *
 *  Pipe Interrupts (per pipe):
 *    PIPEA_IER = pipe_a_base + 0x2A0
 *    PIPEA_IIR = pipe_a_base + 0x2A4 (write 1 to clear)
 *    PIPEA_IMR = pipe_a_base + 0x2A8
 *
 *    Vblank interrupt bit: bit 0 (GEN11_PIPE_VBLANK)
 *─────────────────────────────────────────────
 */

#define GEN11_DE_INTERRUPT_CONTROL      0x44300
#define GEN11_DE_MASTER_ENABLE          (1U << 31)

#define DISP_INT_CTL                    0x44200
#define DISP_INT_ENABLE                 (1U << 0)

#define GEN11_DE_MISC_IER               0x44468
#define GEN11_DE_MISC_IIR               0x4446C
#define GEN11_DE_MISC_IMR               0x44470

/* North Display Engine Hotplug (Gen11+) */
#define GEN11_DE_HPD_ISR                0x44470
#define GEN11_DE_HPD_IMR                0x44474
#define GEN11_DE_HPD_IIR                0x44478
#define GEN11_DE_HPD_IER                0x4447C

/* Hotplug bit definitions (Gen 11/12) */
#define GEN11_DE_TC1_HOTPLUG            (1U << 0)
#define GEN11_DE_TC2_HOTPLUG            (1U << 1)
#define GEN11_DE_TC3_HOTPLUG            (1U << 2)
#define GEN11_DE_TC4_HOTPLUG            (1U << 3)
#define GEN11_DE_DDI_A_HOTPLUG          (1U << 16)
#define GEN11_DE_DDI_B_HOTPLUG          (1U << 17)
#define GEN11_DE_DDI_C_HOTPLUG          (1U << 18)
#define GEN11_DE_HPD_ALL_DDI            (GEN11_DE_DDI_A_HOTPLUG | GEN11_DE_DDI_B_HOTPLUG | GEN11_DE_DDI_C_HOTPLUG)

/* South Display Engine / PCH (SDE) Interrupts (Gen 12/PCH) */
#define SDE_ISR                         0xC4000
#define SDE_IMR                         0xC4004
#define SDE_IIR                         0xC4008
#define SDE_IER                         0xC400C
#define SDE_HOTPLUG_MASK_SPT            (1U << 24)

#define SHOTPLUG_CTL_DDI                0xC4030
#define SHOTPLUG_CTL_TC                 0xC4034
#define SHOTPLUG_DDI_A_HPD_ENABLE       (1U << 4)
#define SHOTPLUG_DDI_B_HPD_ENABLE       (1U << 12)
#define SHOTPLUG_DDI_C_HPD_ENABLE       (1U << 20)
#define SHOTPLUG_ALL_DDI_ENABLE         (SHOTPLUG_DDI_A_HPD_ENABLE | SHOTPLUG_DDI_B_HPD_ENABLE | SHOTPLUG_DDI_C_HPD_ENABLE)

#define PIPE_IIR_OFFSET                 0x2A4
#define PIPE_IER_OFFSET                 0x2A0
#define PIPE_IMR_OFFSET                 0x2A8

#define GEN11_PIPE_VBLANK               (1U << 0)
#define GEN11_PIPE_EOF                  (1U << 1)
#define GEN11_PIPE_PSR_STATUS           (1U << 2)
#define GEN11_PIPE_FIFO_UNDERRUN        (1U << 8)

/*
 * ─────────────────────────────────────────────
 *  IntelFramebuffer Class
 * ─────────────────────────────────────────────
 *
 * Responsibilities:
 * 1. initInterrupts() — Interrupt setup & clearing
 * 2. installInterruptHandlers() — IOInterruptEventSource binding
 * 3. Vblank Interrupt Handling
 * 4. Hotplug Detection (HPD) Handling (Gen 12+)
 * 5. Display Pipe Enable/Disable
 *
 * Helper class associated with MyIntelGPU* parent.
 *─────────────────────────────────────────────
 */

class IntelFramebuffer : public IOService {
    OSDeclareDefaultStructors(IntelFramebuffer)

public:

    virtual bool init(OSDictionary *dict) override;
    virtual void free() override;

    /*
     * ─────────────────────────────────────
     *  Setup Methods (called from parent start())
     * ─────────────────────────────────────
     */

    /*!
     * @brief Initialize Parent + Interrupt State
     *
     * Clears interrupt registers and installs IOKit handlers.
     *
     * @param parent MyIntelGPU instance
     * @return true = success
     */
    bool initInterrupts(MyIntelGPU *parent);

    /*!
     * @brief Disable all interrupts during stop / sleep
     */
    void disableInterrupts(void);

    /*!
     * @brief Enable Vblank Interrupt on specified Pipe
     *
     * @param pipe  Pipe index (0=A, 1=B, 2=C)
     */
    void enableVblankInterrupt(uint32_t pipe);

    /*!
     * @brief Disable Vblank Interrupt on specified Pipe
     *
     * @param pipe  Pipe index
     */
    void disableVblankInterrupt(uint32_t pipe);

    /*!
     * @brief Enable Hotplug Detection Interrupt (Gen 12+)
     *
     * @param ddiMask  Bitmask of DDI/TC ports to listen for hotplug events
     */
    void enableHotplugInterrupt(uint32_t ddiMask = GEN11_DE_DDI_A_HOTPLUG);

    /*!
     * @brief Disable Hotplug Detection Interrupt
     */
    void disableHotplugInterrupt(void);

    /*!
     * @brief Handle display interrupts dispatched from MyIntelGPU's master IRQ
     */
    void handleInterrupt(void);

    /*
     * ─────────────────────────────────────
     *  Accessors
     * ─────────────────────────────────────
     */
    MyIntelGPU *getParent(void) const { return fParent; }
    bool isInterruptsReady(void) const { return fInterruptsReady; }
    uint32_t getHotplugStatus(void) const { return fLastHPDStatus; }

    /* 2.0.223: diagnostic counter dump (ALIVE tick / stop()) */
    void dumpDiagnostics(void) const;

private:

    /* Parent */
    MyIntelGPU             *fParent;

    /* Interrupt State */
    bool                    fInterruptsReady;
    uint32_t                fEnabledPipes;
    uint32_t                fLastHPDStatus;

    /* 2.0.223 evidence counters (display IRQ sub-sources) */
    uint64_t                fMiscIrqCount;   /*!< DE_MISC_IIR events */
    uint64_t                fHpdIrqCount;    /*!< DE_HPD_IIR (north) events */
    uint64_t                fSdeIrqCount;    /*!< SDE_IIR (PCH) events */
    uint64_t                fVblankIrqCount; /*!< per-pipe vblank events */
    uint64_t                fUnderrunIrqCount; /*!< per-pipe FIFO underruns */

    /*
     * ─────────────────────────────────────
     *  Internal Helpers
     * ─────────────────────────────────────
     */

    /* Interrupt Status Register (IIR) clearing */
    void clearAllInterruptRegisters(void);

    /* Interrupt Status Register per Pipe */
    uint32_t getPipeIIR(uint32_t pipe);
    uint32_t getPipeIER(uint32_t pipe);

    /* MMIO Base per Pipe */
    static uint32_t pipeBase(uint32_t pipe);
};

#endif /* __INTEL_FRAMEBUFFER_HPP__ */
