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
                           uint32_t format, uint32_t ggttOffset);
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

bool vcsCmdHcpPipeModeSelect(MyIntelVCSCmdBuf *buf, bool decode, uint32_t codec);
bool vcsCmdHcpSurfaceState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                           uint32_t format, uint32_t ggttOffset);
bool vcsCmdHcpPipeBufAddrState(MyIntelVCSCmdBuf *buf, uint32_t surfaces[8], uint32_t count);
bool vcsCmdHcpIndObjBaseAddrState(MyIntelVCSCmdBuf *buf, uint32_t ggttOffset, uint32_t size);
bool vcsCmdHcpPicState(MyIntelVCSCmdBuf *buf, uint32_t width, uint32_t height,
                       uint32_t picParamGgtt);
bool vcsCmdHcpSliceState(MyIntelVCSCmdBuf *buf, uint32_t sliceCount);
bool vcsCmdHcpRefIdxState(MyIntelVCSCmdBuf *buf, uint32_t frameNum, uint32_t picId);

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
                           uint32_t numRefL1Active, uint32_t deblockCtrlPresent);
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
