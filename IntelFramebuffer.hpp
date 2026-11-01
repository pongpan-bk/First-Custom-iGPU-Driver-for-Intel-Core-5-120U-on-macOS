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
 *  Gen12+ DP/HDMI Link Training Register Offsets
 * ─────────────────────────────────────────────
 *  Reference: i915_reg.h — DDI_BUF_CTL, DP_TP_CTL, DP_TP_STATUS
 *             intel_dp.c — intel_dp_link_training
 *             intel_ddi.c — intel_ddi_prepare_link_retrain
 *
 *  DDI Buffer Control (per DDI):
 *    DDI_A_BUF_CTL = DDI_A_BASE + 0x600
 *    DDI_B_BUF_CTL = DDI_B_BASE + 0x600
 *    DDI_C_BUF_CTL = DDI_C_BASE + 0x600
 *
 *  DP Training Pattern Control (per DDI):
 *    DP_TP_CTL_A   = DDI_A_BASE + 0x604
 *    DP_TP_CTL_B   = DDI_B_BASE + 0x604
 *    DP_TP_CTL_C   = DDI_C_BASE + 0x604
 *
 *  DP Training Pattern Status (per DDI):
 *    DP_TP_STATUS_A = DDI_A_BASE + 0x608
 *    DP_TP_STATUS_B = DDI_B_BASE + 0x608
 *    DP_TP_STATUS_C = DDI_C_BASE + 0x608
 *
 *  DPCD (via AUX channel):
 *    DPCD 0x00000: DPCD Revision
 *    DPCD 0x00001: Max Link Rate
 *    DPCD 0x00002: Max Lane Count
 *    DPCD 0x00003: Max Downspread
 *    DPCD 0x00100-0x00103: Link Rate/Lane Set (per lane)
 *    DPCD 0x00200-0x00203: Lane Status (CR/EQ/Symbol Lock)
 *    DPCD 0x00600: Training Pattern Set
 */

#define DDI_BUF_CTL_OFFSET              0x600
#define DP_TP_CTL_OFFSET                0x604
#define DP_TP_STATUS_OFFSET             0x608

#define DDI_BUF_CTL_ENABLE              (1U << 31)
#define DDI_BUF_CTL_DDI_SELECT_MASK     (0x7U << 28)
#define DDI_BUF_CTL_PORT_REVERSAL       (1U << 16)
#define DDI_BUF_CTL_TRANS_SELECT_MASK   (0x3U << 30)

#define DP_TP_CTL_ENABLE                (1U << 31)
#define DP_TP_CTL_MODE_MASK             (0x3U << 29)
#define DP_TP_CTL_MODE_TP1              (0x0U << 29)  /* Training Pattern 1 */
#define DP_TP_CTL_MODE_TP2              (0x1U << 29)  /* Training Pattern 2 */
#define DP_TP_CTL_MODE_TP3              (0x2U << 29)  /* Training Pattern 3 */
#define DP_TP_CTL_MODE_TP4              (0x3U << 29)  /* Training Pattern 4 */
#define DP_TP_CTL_LINK_TRAIN_MASK       (0x3U << 27)
#define DP_TP_CTL_LINK_TRAIN_OFF        (0x0U << 27)
#define DP_TP_CTL_LINK_TRAIN_CR         (0x1U << 27)  /* Clock Recovery */
#define DP_TP_CTL_LINK_TRAIN_EQ         (0x2U << 27)  /* Equalization */
#define DP_TP_CTL_LINK_TRAIN_SYMBOL     (0x3U << 27)  /* Symbol Lock */
#define DP_TP_CTL_SCRAMBLING_DISABLE    (1U << 26)
#define DP_TP_CTL_ENHANCED_FRAME_EN     (1U << 25)
#define DP_TP_CTL_VOLTAGE_SWING_MASK    (0x3U << 23)
#define DP_TP_CTL_VOLTAGE_SWING_0       (0x0U << 23)  /* Level 0 */
#define DP_TP_CTL_VOLTAGE_SWING_1       (0x1U << 23)  /* Level 1 */
#define DP_TP_CTL_VOLTAGE_SWING_2       (0x2U << 23)  /* Level 2 */
#define DP_TP_CTL_VOLTAGE_SWING_3       (0x3U << 23)  /* Level 3 */
#define DP_TP_CTL_PRE_EMPHASIS_MASK     (0x3U << 21)
#define DP_TP_CTL_PRE_EMPHASIS_0        (0x0U << 21)  /* Level 0 */
#define DP_TP_CTL_PRE_EMPHASIS_1        (0x1U << 21)  /* Level 1 */
#define DP_TP_CTL_PRE_EMPHASIS_2        (0x2U << 21)  /* Level 2 */
#define DP_TP_CTL_PRE_EMPHASIS_3        (0x3U << 21)  /* Level 3 */
#define DP_TP_CTL_LANE_COUNT_MASK       (0x7U << 16)
#define DP_TP_CTL_LANE_COUNT_SHIFT      16

#define DP_TP_STATUS_CR_DONE_LANE0      (1U << 0)
#define DP_TP_STATUS_CR_DONE_LANE1      (1U << 1)
#define DP_TP_STATUS_CR_DONE_LANE2      (1U << 2)
#define DP_TP_STATUS_CR_DONE_LANE3      (1U << 3)
#define DP_TP_STATUS_EQ_DONE_LANE0      (1U << 8)
#define DP_TP_STATUS_EQ_DONE_LANE1      (1U << 9)
#define DP_TP_STATUS_EQ_DONE_LANE2      (1U << 10)
#define DP_TP_STATUS_EQ_DONE_LANE3      (1U << 11)
#define DP_TP_STATUS_SYMBOL_LOCK_LANE0  (1U << 16)
#define DP_TP_STATUS_SYMBOL_LOCK_LANE1  (1U << 17)
#define DP_TP_STATUS_SYMBOL_LOCK_LANE2  (1U << 18)
#define DP_TP_STATUS_SYMBOL_LOCK_LANE3  (1U << 19)
#define DP_TP_STATUS_CR_DONE_MASK       (DP_TP_STATUS_CR_DONE_LANE0 | DP_TP_STATUS_CR_DONE_LANE1 | DP_TP_STATUS_CR_DONE_LANE2 | DP_TP_STATUS_CR_DONE_LANE3)
#define DP_TP_STATUS_EQ_DONE_MASK       (DP_TP_STATUS_EQ_DONE_LANE0 | DP_TP_STATUS_EQ_DONE_LANE1 | DP_TP_STATUS_EQ_DONE_LANE2 | DP_TP_STATUS_EQ_DONE_LANE3)
#define DP_TP_STATUS_SYMBOL_LOCK_MASK   (DP_TP_STATUS_SYMBOL_LOCK_LANE0 | DP_TP_STATUS_SYMBOL_LOCK_LANE1 | DP_TP_STATUS_SYMBOL_LOCK_LANE2 | DP_TP_STATUS_SYMBOL_LOCK_LANE3)

/* DPCD Register Offsets (accessed via AUX) */
#define DPCD_REVISION                   0x00000
#define DPCD_MAX_LINK_RATE              0x00001
#define DPCD_MAX_LANE_COUNT             0x00002
#define DPCD_MAX_DOWNSPREAD             0x00003
#define DPCD_NORP                       0x00004
#define DPCD_DOWNSTREAM_PORT_COUNT      0x00005
#define DPCD_RECEIVE_PORT0_CAP0         0x00006
#define DPCD_RECEIVE_PORT1_CAP0         0x00007
#define DPCD_LINK_RATE_SET              0x00100
#define DPCD_LANE0_1_SET                0x00101
#define DPCD_LANE2_3_SET                0x00102
#define DPCD_DOWNSPREAD_CTRL            0x00103
#define DPCD_MAIN_LINK_CHANNEL_CODING   0x00104
#define DPCD_TRAINING_PATTERN_SET       0x00105
#define DPCD_LANE0_CR_DONE              (1U << 0)
#define DPCD_LANE1_CR_DONE              (1U << 1)
#define DPCD_LANE2_CR_DONE              (1U << 2)
#define DPCD_LANE3_CR_DONE              (1U << 3)
#define DPCD_LANE0_EQ_DONE              (1U << 4)
#define DPCD_LANE1_EQ_DONE              (1U << 5)
#define DPCD_LANE2_EQ_DONE              (1U << 6)
#define DPCD_LANE3_EQ_DONE              (1U << 7)
#define DPCD_LANE0_SYMBOL_LOCK          (1U << 0)
#define DPCD_LANE1_SYMBOL_LOCK          (1U << 1)
#define DPCD_LANE2_SYMBOL_LOCK          (1U << 2)
#define DPCD_LANE3_SYMBOL_LOCK          (1U << 3)

/* Link Rates */
#define DP_LINK_RATE_RBR                0x06  /* 1.62 Gbps */
#define DP_LINK_RATE_HBR                0x0A  /* 2.70 Gbps */
#define DP_LINK_RATE_HBR2               0x14  /* 5.40 Gbps */
#define DP_LINK_RATE_HBR3               0x1E  /* 8.10 Gbps */

/* DDI Buffer Translation Select */
#define DDI_BUF_TRANS_A                 0x00
#define DDI_BUF_TRANS_B                 0x01
#define DDI_BUF_TRANS_C                 0x02
#define DDI_BUF_TRANS_EDP               0x03

/* AUX Channel Control */
#define DDI_AUX_CTL_OFFSET              0x10
#define DDI_AUX_DATA_OFFSET             0x20
#define AUX_CTL_SEND_BUSY               (1U << 31)
#define AUX_CTL_TIMEOUT_MASK            (0xFU << 28)
#define AUX_CTL_RECEIVE_ERROR           (1U << 27)
#define AUX_CTL_TIME_OUT_ERROR          (1U << 26)
#define AUX_CTL_DONE                    (1U << 24)
#define AUX_CTL_PRECHARGE_2US           (0x0U << 16)
#define AUX_CTL_PRECHARGE_4US           (0x1U << 16)
#define AUX_CTL_PRECHARGE_6US           (0x2U << 16)
#define AUX_CTL_PRECHARGE_8US           (0x3U << 16)
#define AUX_CTL_BIT_CLOCK_2X            (1U << 15)
#define AUX_CTL_SYNC_PULSE_WIDTH_MASK   (0x3FU << 8)
#define AUX_CTL_MESSAGE_SIZE_MASK       (0x1FU << 0)

/*
 * ─────────────────────────────────────────────
 *  IntelFramebuffer Class
 * ─────────────────────────────────────────────
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
     *  DP Link Training (Phase 8)
     * ─────────────────────────────────────
     */

    /*!
     * @brief Perform full DP link training sequence on specified DDI
     *
     * Implements the DP 1.4 link training sequence:
     * 1. Read DPCD caps (max link rate, lane count)
     * 2. Clock Recovery (CR) phase - Training Pattern 1
     * 3. Channel Equalization (EQ) phase - Training Pattern 2/3
     * 4. Symbol Lock verification
     *
     * @param ddiIndex  DDI index (0=A, 1=B, 2=C)
     * @return true = link trained successfully
     */
    bool trainDPLink(uint32_t ddiIndex);

    /*!
     * @brief Read DPCD via AUX channel
     *
     * @param ddiIndex  DDI index
     * @param address   DPCD register address
     * @param data      Output buffer
     * @param length    Number of bytes to read
     * @return true = success
     */
    bool auxReadDPCD(uint32_t ddiIndex, uint32_t address, uint8_t *data, uint32_t length);

    /*!
     * @brief Write DPCD via AUX channel
     *
     * @param ddiIndex  DDI index
     * @param address   DPCD register address
     * @param data      Input buffer
     * @param length    Number of bytes to write
     * @return true = success
     */
    bool auxWriteDPCD(uint32_t ddiIndex, uint32_t address, const uint8_t *data, uint32_t length);

    /*!
     * @brief Configure DDI buffer for DP output
     *
     * @param ddiIndex  DDI index
     * @param linkRate  Link rate (DP_LINK_RATE_*)
     * @param laneCount Number of lanes (1/2/4)
     * @return true = success
     */
    bool configureDDIBuffer(uint32_t ddiIndex, uint8_t linkRate, uint8_t laneCount);

    /*!
     * @brief Set training pattern and voltage swing/pre-emphasis
     *
     * @param ddiIndex      DDI index
     * @param pattern       Training pattern (DP_TP_CTL_MODE_*)
     * @param voltageSwing  Voltage swing level (0-3)
     * @param preEmphasis   Pre-emphasis level (0-3)
     * @param laneCount     Number of active lanes
     * @return true = success
     */
    bool setTrainingPattern(uint32_t ddiIndex, uint32_t pattern,
                            uint8_t voltageSwing, uint8_t preEmphasis, uint8_t laneCount);

    /*!
     * @brief Wait for training pattern status (CR/EQ/Symbol Lock)
     *
     * @param ddiIndex      DDI index
     * @param statusMask    Status bits to wait for
     * @param timeoutUS     Timeout in microseconds
     * @return true = status achieved
     */
    bool waitForTrainingStatus(uint32_t ddiIndex, uint32_t statusMask, uint32_t timeoutUS);

    /*!
     * @brief Get DDI base address for register access
     *
     * @param ddiIndex  DDI index (0=A, 1=B, 2=C)
     * @return MMIO base address
     */
    uint32_t getDDIBase(uint32_t ddiIndex) const;

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
