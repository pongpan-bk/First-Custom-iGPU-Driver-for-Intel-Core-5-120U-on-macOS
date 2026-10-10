# Ultrawork Notepad — Gen12 MFX H.264 decode correctness
Started: 2026-10-05T18:25+07:00 (recreated after reboot; original /tmp notepad lost)

## Goal
Make MyIntelGPU's standalone native Gen12 VCS/MFX H.264 decoder produce pixels
matching the independent Apple VideoToolbox reference.
Canonical log: WORK_MEMORY_2026-06-25.md (session notes appended there).
Reference:   /Users/ppbk/Documents/src/VCSMediaHarness/golden_videotoolbox.nv12

## Plan
1. Collect bg_d8dbd47b (librarian: Gen12 AVC chroma field layouts)   <- IN FLIGHT
2. Collect bg_909315e4 (oracle: rank chroma root cause)              <- IN FLIGHT
3. Invoke plan agent with those findings
4. Fix chroma intra pred mode / chroma QP field encoding in MyIntelVCSCommand.cpp

## Now
Agents running. Fresh harness built as ppbk (exit 0). Signature reproduced.

## Todo
- [ ] background_output(bg_d8dbd47b)
- [ ] background_output(bg_909315e4)
- [ ] plan agent
- [ ] implement + verify chroma fix (nosubmit / determinism / golden)

## Scenarios (contract)
S1 nosubmit  : --ctl=nosubmit exits non-zero, no dst write    (safety, already green)
S2 determinism: --ctl=determinism PASS-DETERMINISTIC           (already green)
S3 correctness: golden_videotoolbox byte compare -> currently FAIL 97.56% differ.
   This is the real goal. Poison remaining must reach 0 AND golden diff 0.

## Findings (durable, reproduced on fresh hw 05-Oct)
Luma: rms 13.57, PSNR 25.48 dB, corr 0.9445 with MFX_QM_FLAT=1 (baseline 20.46).
Chroma: U corr -0.2748 / V corr +0.2974. Poison rows {34,35,38,39,42,43,46,47}
  = 1024 bytes, mod4 pattern [2,3,2,3,2,3,2,3], deterministic across runs.
Two surviving chroma defects:
  (a) HORIZONTAL structure lost: ref right-half-minus-left = +36.4, ours = -3.8
  (b) VERTICAL ramp ABSENT: least-squares on row means ->
      ref slope -1.1339/row R2=0.9998 ; ours slope -0.0935/row R2=0.0017

ELIMINATED - do not re-chase: tile mode, YOffsetForVCr, deblocking,
luma shift (2D search optimum is (0,0)), chroma shift, chroma permutation
(histogram L1 914/468 != 0), U/V swap, wrong reference frame, byte-wrap,
signed/unsigned chroma.

## Learnings (pitfalls - cost me real time)
- /tmp is wiped on reboot. Keep durable state in the REPO, not /tmp.
- Harness CLI: clip = argv[1], output = argv[2]. With no args it silently
  runs 1920x1080 with no bitstream and writes nothing.
- /tmp/opencode was root:wheel mode 755. Running as ppbk, fopen(outPath) failed
  SILENTLY -> no [DUMP]. Symptom looked like a harness bug; it was filesystem
  permissions. Always chown scratch dirs before blaming the harness.
- NV12 chroma row stride at W=128 is 128 bytes (CB=W, CH=H/2), NOT 64. I used 64
  and produced a garbage poison-row table.
- "Last 16 chroma rows repeat first 16" is FALSE (0/128 identical). The signed
  ERROR pattern has period 16; pixel values do not repeat.
- NEVER measure gradient from min/max span when a sawtooth is present. Fit slope
  and check R2. My "2.3x over-swing (53 vs 123)" claim was exactly this error.
- corr -0.2194 in the 04-Oct record is STALE (pre-QM_FLAT). Current V corr +0.2974.
- Poison byte values are randomized per run. Never hardcode.

## 05-Oct (cont) — git-history audit: no prior chroma work exists
Checked /Users/ppbk/Desktop/all/git history (99 dated snapshots, Sep 2026).
- Distinct MyIntelVCSCommand.cpp sizes: 33265 (x32), 2272 (x16). Current tree is
  34922 bytes / 856 lines — LARGER than every snapshot, so current work is the most
  advanced and was NOT lost.
- The Sept snapshots contain only a 4-dword placeholder decode
  (MI_NOOP + BATCH_BUFFER_START + USER_INTERRUPT) with ZERO chroma/QP handling.
  They never produced pixels. So there is NO earlier chroma fix to recover;
  the chroma investigation starts from scratch in the current tree.
- Standing approval granted by user: proceed autonomously in this project.

## Live G12 command path (corrected understanding)
The vcsCmdMfxPicState / vcsCmdMfxSliceState / vcsCmdMfxRefIdxState family is DEAD
CODE (HEVC-era, only reached in the old branch at test_submit_vcs.m:1121-1124).
The live Gen12 path emits:
  PipeModeSelect, SurfaceState, PipeBufAddr, IndObj, BspBufBaseAddr,
  AvcPicIdState, **AvcImgState** (0x71000013, 21 dwords), QmState x4,
  DirectMode, AvcRefIdxDummy, AvcSliceState, BsdObject.
=> The picture-state command to fix is MFD_AVC_IMG_STATE, NOT MFX_PIC_STATE.

Clip SPS/PPS (verified by harness [PARSE] output):
  SPS: 128x96, 8x6 MB, profile=100, pocType=0, refFrames=2, chromaFormatIdc=1 (4:2:0)
  PPS: entropy=CABAC, initQp=28, transform8x8=1, deblockCtrlPresent=0
  SLICE: type=2 (I), frameNum=0, qp=23
Note profile=100 (high) so chroma_format_idc IS explicitly coded in the SPS and the
parser handles it correctly (defaults 1, gated on the standard high-profile list).
transform8x8=1 is passed through to AvcImgState DW4 bit3.
AvcImgState DW5 bit27 (TrellisQuantizationChromaDisable) is hardcoded to 1.

## Scenario baseline (verified 05-Oct, fresh ppbk-built harness)
S1 nosubmit   : submitted=0, dst_written_pct=0.00, dst_poison_remaining=18432,
                exit 1  -> PASS (safety intact, nothing written)
S2 determinism: det_diff_bytes=0 differing=0 first_diff=-1,
                poison_run1=1024 poison_run2=1024 -> frames bit-identical,
                just incomplete. PASS for determinism.
S3 correctness: golden diff 17982/18432 (97.56%) -> FAIL, this is the goal.
