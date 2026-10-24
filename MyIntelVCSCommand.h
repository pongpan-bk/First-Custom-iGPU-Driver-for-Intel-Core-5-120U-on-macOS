#ifndef __MY_INTEL_VCS_COMMAND_H__
#define __MY_INTEL_VCS_COMMAND_H__

#include <stdint.h>
#include <stdbool.h>  /* bool for C consumers (bridge dylib) */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VCS Command Builder — H.264/HEVC decode command stream
 *
 * Builds MI batch buffer commands for VCS0 ring submission.
 * Each function appends dwords to a command buffer.
 *
 * Reference:
 *   mhw_vdbox_mfx_hwcmd_g12_X.h  — MFX (H.264) commands
 *   mhw_vdbox_hcp_hwcmd_g12_X.h  — HCP (HEVC) commands
 *   codechal_decode_hevc_g12.cpp  — HEVC decode pipeline
 *   codechal_decode_avc_g12.cpp   — H.264 decode pipeline
 */

#define VCS_CMD_MAX_DWORDS  1024

typedef struct {
    uint32_t cmds[VCS_CMD_MAX_DWORDS];
    uint32_t count;
    uint32_t codec;         /* VCS_CMD_H264_DECODE or VCS_CMD_HEVC_DECODE */
} MyIntelVCSCmdBuf;

bool vcsCmdInit(MyIntelVCSCmdBuf *buf, uint32_t codec);
bool vcsCmdEmit(MyIntelVCSCmdBuf *buf, uint32_t dword);
bool vcsCmdEmitBatch(MyIntelVCSCmdBuf *buf, const uint32_t *dwords, uint32_t count);
bool vcsCmdBatchStart(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset);
bool vcsCmdBatchEnd(MyIntelVCSCmdBuf *buf);

bool vcsCmdMfxPipeModeSelect(MyIntelVCSCmdBuf *buf, uint32_t codec, bool decode);
bool vcsCmdMfxSurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                            uint32_t format, uint32_t ggttOffset, bool linear);
bool vcsCmdMfxPipeBufAddrState(MyIntelVCSCmdBuf *buf, uint32_t surfaces[8], uint32_t count);
bool vcsCmdMfxIndObjBaseAddrState(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset, uint32_t size);
bool vcsCmdMfxPicState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                       uint32_t picParamGgtt);
bool vcsCmdMfxSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceCount);
bool vcsCmdMfxRefIdxState(MyIntelVCSCmdBuf *buf, uint32_t frameNum, uint32_t picId);

/* Codec type for HCP/MFX pipe mode select */
#define VCS_CODEC_H264    0
#define VCS_CODEC_HEVC    1
#define VCS_CODEC_AV1     12

/* Parsed HEVC picture parameters for HCP_PIC_STATE (from real SPS/PPS). */
typedef struct {
    uint32_t width, height;
    uint32_t chromaFormatIdc;      /* 1 = 4:2:0, 2 = 4:2:2, 3 = 4:4:4 */
    uint32_t bitDepthLumaMinus8;
    uint32_t bitDepthChromaMinus8;
    uint32_t minCbLog2SizeY;
    uint32_t ctbLog2SizeY;
    uint32_t log2MaxTransformSkipSize;
    uint32_t sampleAdaptiveOffsetEnabled;
    uint32_t pcmEnabled;
    uint32_t cuQpDeltaEnabled;
    uint32_t diffCuQpDeltaDepth;
    uint32_t log2ParallelMergeLevelMinus2;
    uint32_t signDataHidingEnabled;
    uint32_t loopFilterAcrossTilesEnabled;
    uint32_t entropyCodingSyncEnabled;
    uint32_t constrainedIntraPred;
    uint32_t strongIntraSmoothingEnabled;
    uint32_t transformSkipEnabled;
    uint32_t ampEnabled;
    uint32_t transquantBypassEnabled;
} VcsHevcPicParams;

bool vcsCmdG12HcpPicStateEx(MyIntelVCSCmdBuf *buf, const VcsHevcPicParams *pp,
                            uint32_t picParamGgtt);

/* Parsed slice-header values for HCP_SLICE_STATE (from real slice segment). */
typedef struct {
    uint32_t sliceType;          /* 0 = B, 1 = P, 2 = I */
    uint32_t sliceQp;            /* init_qp + slice_qp_delta, 0..51 */
    uint32_t sliceStartCtbX;
    uint32_t sliceStartCtbY;
    uint32_t lastSliceOfPic;
    uint32_t lastSliceOfTile;
    uint32_t lastSliceOfTileColumn;
    uint32_t dependentSliceFlag;
    uint32_t sliceHeaderLength;
    int32_t  sliceCbQpOffset;
    int32_t  sliceCrQpOffset;
    uint32_t saoLumaFlag;
    uint32_t saoChromaFlag;
    uint32_t disableDeblockingFilterFlag;
    uint32_t sliceLoopFilterAcrossSlicesEnabled;
    uint32_t collocatedFromL0Flag;
} VcsHevcSliceParams;

bool vcsCmdG12HcpSliceStateEx(MyIntelVCSCmdBuf *buf, const VcsHevcSliceParams *sp);

bool vcsCmdG12HcpVdControlState(MyIntelVCSCmdBuf *buf, bool initialization,
                                bool memoryImplicitFlush);
bool vcsCmdG12HcpPipeModeSelect(MyIntelVCSCmdBuf *buf, bool decode);
bool vcsCmdG12HcpSurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                               uint32_t format, uint32_t ggttOffset, bool linear);
bool vcsCmdG12HcpSurfaceStateEx(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                                uint32_t format, uint32_t surfaceId);
bool vcsCmdG12HcpPipeBufAddr(MyIntelVCSCmdBuf *buf, uint32_t dstGgtt,
                             uint32_t scratchGgtt, uint32_t refGgtt);
bool vcsCmdG12HcpIndObj(MyIntelVCSCmdBuf *buf, uint32_t ggtt, uint32_t size);
bool vcsCmdG12HcpQmState(MyIntelVCSCmdBuf *buf);
bool vcsCmdG12HcpPicState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                         uint32_t picParamGgtt);
bool vcsCmdG12HcpTileState(MyIntelVCSCmdBuf *buf, uint32_t numTilesX, uint32_t numTilesY);
bool vcsCmdG12HcpRefIdx(MyIntelVCSCmdBuf *buf, uint32_t frameNum, uint32_t picId);
bool vcsCmdG12HcpWeightOffset(MyIntelVCSCmdBuf *buf);
bool vcsCmdG12HcpSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceCount);
bool vcsCmdG12HcpBsdObject(MyIntelVCSCmdBuf *buf, uint32_t sliceOffset,
                           uint32_t sliceSize, uint32_t headerBytes,
                           uint32_t dataBitOffset, bool lastSlice);

/*
 * ─── Gen12 HuC S2L (slice-to-line) front-end commands ──────────────────
 *  HEVC on Gen12 mandates the HuC bitstream-parser before the HCP
 *  long-format picture commands (codechal_decode_hevc_g12.cpp:1387
 *  AddPictureS2LCmds is called unconditionally). Without these the HCP
 *  core runs with no slice data → decoded surface stays all-zero (no fault).
 *
 *  Ground truth: mhw_vdbox_huc_hwcmd_g12_X.{h,cpp} + mhw_vdbox_huc_generic.h
 *    HUC_PIPE_MODE_SELECT      0x75800001   dwSize  3 (DL=1)
 *    HUC_IMEM_STATE            0x75810003   dwSize  5 (DL=3)
 *    HUC_DMEM_STATE            0x75820004   dwSize  6 (DL=4)
 *    HUC_VIRTUAL_ADDR_STATE    0x7584002f   dwSize 49 (DL=47)
 *    HUC_IND_OBJ_BASE_ADDR     0x75850009   dwSize 11 (DL=9)
 *  Address shifts: HUC_GENERAL_STATE_SHIFT=6, HUC_UPPER_BOUND_STATE_SHIFT=12.
 */
bool vcsCmdG12HucPipeModeSelect(MyIntelVCSCmdBuf *buf, bool streamOutEnable);
bool vcsCmdG12HucImemState(MyIntelVCSCmdBuf *buf, uint32_t kernelDescriptor);
bool vcsCmdG12HucDmemState(MyIntelVCSCmdBuf *buf, uint32_t dmemSrcGgtt,
                           uint32_t dmemOffset, uint32_t dataLength);
bool vcsCmdG12HucVirtualAddrState(MyIntelVCSCmdBuf *buf, uint32_t region0Ggtt);
bool vcsCmdG12HucIndObjBaseAddr(MyIntelVCSCmdBuf *buf, uint32_t bitstreamGgtt,
                                uint32_t dataSize);

bool vcsCmdFlushMedia(MyIntelVCSCmdBuf *buf);
bool vcsCmdWaitMedia(MyIntelVCSCmdBuf *buf);

/*
 * ─── VEBOX Encode — H.264 / HEVC encode via VEBOX ──────────────────────
 *  VECS0 @ 0x1C8000, VECS1 @ 0x1D8000
 *  รองรับ H.264 encode (8-bit/10-bit 4K) และ HEVC encode (8-bit/10-bit 4K)
 */
bool vcsCmdVeboxInit(MyIntelVCSCmdBuf *buf);
bool vcsCmdVeboxPictureStart(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                              uint32_t format, uint32_t veboxIndex);
bool vcsCmdVeboxSliceData(MyIntelVCSCmdBuf *buf, uint32_t sliceAddr, uint32_t sliceSize);
bool vcsCmdVeboxPictureEnd(MyIntelVCSCmdBuf *buf);

/*
 * ─── Phase 10 — Gen12-accurate MFX AVC decode sequence ───────────
 *  อ้างอิง layout จริงจาก mhw_vdbox_mfx_hwcmd_g12_X.h (Intel media-driver)
 *  DW0 opcodes คำนวณจาก codegen enums:
 *    PIPE_MODE_SELECT  0x70000004  (5  dwords)
 *    SURFACE_STATE     0x70010005  (6  dwords)
 *    PIPE_BUF_ADDR     0x70020043  (68 dwords)
 *    IND_OBJ_BASE_ADDR 0x70030019  (26 dwords)
 *    BSP_BUF_BASE_ADDR 0x70040008  (10 dwords)
 *    QM_STATE          0x70070010  (18 dwords × 4)
 *    AVC_IMG_STATE     0x71000014  (21 dwords)
 *    AVC_SLICE_STATE   0x7103000A  (11 dwords)
 *    AVC_BSD_OBJECT    0x71280006  (7  dwords)
 *    AVC_DIRECTMODE    0x71020046  (71 dwords)
 */
bool vcsCmdG12PipeModeSelect(MyIntelVCSCmdBuf *buf, bool hevc);
bool vcsCmdG12SurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                           uint32_t pitch);
/* Format-aware surface state: surfFmt = MYVCS_SURF_FMT_NV12 (4) or
 * MYVCS_SURF_FMT_P010 (9). NV12 default via vcsCmdG12SurfaceState. */
bool vcsCmdG12SurfaceStateFmt(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                              uint32_t pitch, uint32_t surfFmt);
bool vcsCmdG12PipeBufAddr(MyIntelVCSCmdBuf *buf, uint32_t dstGgtt,
                          uint32_t scratchGgtt, uint32_t refGgtt);
/* DPB variant: caller supplies 16 pre-masked reference GGTT addresses */
bool vcsCmdG12PipeBufAddrRefs(MyIntelVCSCmdBuf *buf, uint32_t dstGgtt,
                              uint32_t scratchGgtt, const uint32_t refs[16]);
int vcsDpbSanityCheck(const uint32_t refs[16]);
bool vcsCmdG12IndObj(MyIntelVCSCmdBuf *buf, uint32_t ggtt, uint32_t size,
                     uint32_t mvGgtt, uint32_t mvSize);
bool vcsCmdG12BspBufBaseAddr(MyIntelVCSCmdBuf *buf, uint32_t bsdMpcGgtt,
                             uint32_t mprGgtt);
bool vcsCmdG12QmState(MyIntelVCSCmdBuf *buf);
bool vcsCmdG12AvcImgState(MyIntelVCSCmdBuf *buf,
                           uint32_t widthMb, uint32_t heightMb,
                           uint32_t entropy, uint32_t transform8x8,
                           uint32_t direct8x8, uint32_t chromaIdc,
                           uint32_t log2MaxFrameNumM4, uint32_t pocType,
                           uint32_t log2MaxPocLsbM4, uint32_t picOrderPresent,
                           uint32_t deltaPocAlwaysZero,
                           int32_t  picInitQp, uint32_t frameNum,
                           uint32_t numRefFrames, uint32_t numRefL0Active,
                           uint32_t numRefL1Active, uint32_t deblockCtrlPresent,
                           int32_t  chromaQpIndexOffset, int32_t  secondChromaQpIndexOffset);
bool vcsCmdG12AvcPicIdState(MyIntelVCSCmdBuf *buf);
bool vcsCmdG12AvcRefIdxDummy(MyIntelVCSCmdBuf *buf);
bool vcsCmdG12AvcSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceType,
                            uint32_t startMb, uint32_t widthMb,
                            uint32_t heightMb,
                            uint32_t sliceQp, uint32_t cabacInitIdc,
                            uint32_t disableDeblock, int32_t alphaDiv2, int32_t betaDiv2,
                            uint32_t numRefL0Count, uint32_t numRefL1Count,
                            uint32_t nextFirstMb, bool lastSlice);
bool vcsCmdG12BsdObject(MyIntelVCSCmdBuf *buf, uint32_t sliceOffset,
                        uint32_t sliceSize, uint32_t headerBytes,
                        uint32_t dataBitOffset, uint32_t nalType, bool lastSlice);
bool vcsCmdG12DirectMode(MyIntelVCSCmdBuf *buf, uint32_t mvScratchGgtt,
                         uint32_t temporalGgtt);

#ifdef __cplusplus
}
#endif

#endif /* __MY_INTEL_VCS_COMMAND_H__ */
