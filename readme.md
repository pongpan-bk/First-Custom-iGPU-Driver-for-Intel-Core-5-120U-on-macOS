 ตรวจสอบงานที่ 2: ปลดล็อกท่อคำนวณ 3D เนทีฟเต็มสูบ (RCS0 Render Engine)
========================================================================
--- Kernel Matrix Engine Status ---
Wall Clock adjustment detected - results might be strange while using --end
2026-10-19 04:43:54.533494-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:640] Building translation table (FakeGen=9, RealGen=12)
2026-10-19 04:43:54.533496-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 force-active (debug, PE_parse+fallback missed but myintelrcs=1 expected)
2026-10-19 04:43:54.533498-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-19 04:43:54.533499-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:814] Translation table built with 7 entries:
2026-10-19 04:43:54.533501-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-19 04:43:54.533504-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-19 04:43:54.533506-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-19 04:43:54.533508-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-19 04:43:54.533511-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-19 04:43:54.533513-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-19 04:43:54.533518-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-19 04:43:54.533521-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4225] Phase 5: Translation table ready (7 entries)
2026-10-19 04:43:54.533546-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1350] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-19 04:43:54.652836-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4393] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-19 04:43:54.658683-0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-19 04:43:54.669347-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 04:43:54.696577-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 04:43:54.723887-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 04:43:54.736331-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 04:43:54.736344-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 04:43:54.737542-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 04:43:54.737554-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 04:44:02.717503-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 04:44:02.717513-0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 21:23:25.236921+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:640] Building translation table (FakeGen=9, RealGen=12)
2026-10-19 21:23:25.236923+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 force-active (debug, PE_parse+fallback missed but myintelrcs=1 expected)
2026-10-19 21:23:25.236924+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-19 21:23:25.236926+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:814] Translation table built with 7 entries:
2026-10-19 21:23:25.236927+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-19 21:23:25.236929+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-19 21:23:25.236931+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-19 21:23:25.236933+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-19 21:23:25.236935+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-19 21:23:25.236937+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-19 21:23:25.236939+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:821]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-19 21:23:25.236941+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4225] Phase 5: Translation table ready (7 entries)
2026-10-19 21:23:25.236961+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1350] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-19 21:23:25.345333+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4393] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-19 21:23:25.350956+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-19 21:23:25.361330+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 21:23:25.387444+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 21:23:25.413720+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-10-19 21:23:25.425417+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 21:23:25.425430+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 21:23:25.426530+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 21:23:25.426541+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 21:23:25.427743+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 21:23:25.427755+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 23:55:38.045512+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:639] Building translation table (FakeGen=9, RealGen=12)
2026-10-19 23:55:38.045516+0700  localhost kernel[0]: (MyIntelGPU) RCS0 force-active (myintelrcs=1)
2026-10-19 23:55:38.045517+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-19 23:55:38.045518+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:813] Translation table built with 7 entries:
2026-10-19 23:55:38.045520+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-19 23:55:38.045523+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-19 23:55:38.045527+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-19 23:55:38.045529+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-19 23:55:38.045531+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-19 23:55:38.045534+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-19 23:55:38.045536+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-19 23:55:38.045538+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4428] Phase 5: Translation table ready (7 entries)
2026-10-19 23:55:38.045573+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1349] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-19 23:55:38.175459+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4596] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-19 23:55:38.182311+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-19 23:55:38.263137+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 23:55:38.263150+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 23:55:38.264326+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 23:55:38.264335+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-19 23:55:38.265637+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-19 23:55:38.265648+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 00:30:25.182485+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x40 loops=1
2026-10-20 00:30:25.182497+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=64 CSBwr=2 ACTIVE=1
2026-10-20 00:30:25.291628+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 00:30:25.291642+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 00:30:25.395795+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 00:30:25.395834+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 00:30:25.499067+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 00:30:25.499087+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 00:30:25.601550+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x40 loops=1
2026-10-20 00:30:25.601566+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=64 CSBwr=2 ACTIVE=1
2026-10-20 00:30:25.708305+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x530 loops=1
2026-10-20 00:30:25.708321+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=1328 CSBwr=2 ACTIVE=1
2026-10-20 01:12:14.949459+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:639] Building translation table (FakeGen=9, RealGen=12)
2026-10-20 01:12:14.949463+0700  localhost kernel[0]: (MyIntelGPU) RCS0 force-active (myintelrcs=1)
2026-10-20 01:12:14.949465+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-20 01:12:14.949466+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:813] Translation table built with 7 entries:
2026-10-20 01:12:14.949468+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-20 01:12:14.949471+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-20 01:12:14.949474+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-20 01:12:14.949477+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-20 01:12:14.949479+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-20 01:12:14.949481+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-20 01:12:14.949483+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-20 01:12:14.949486+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4428] Phase 5: Translation table ready (7 entries)
2026-10-20 01:12:14.949510+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1349] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-20 01:12:15.075581+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4596] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-20 01:12:15.081780+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-20 01:12:15.158887+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:12:15.158901+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:12:15.160161+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:12:15.160175+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:12:22.766407+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:12:22.766418+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:15:47.793597+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:639] Building translation table (FakeGen=9, RealGen=12)
2026-10-20 01:15:47.793604+0700  localhost kernel[0]: (MyIntelGPU) RCS0 force-active (myintelrcs=1)
2026-10-20 01:15:47.793607+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-20 01:15:47.793610+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:813] Translation table built with 7 entries:
2026-10-20 01:15:47.793614+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-20 01:15:47.793621+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-20 01:15:47.793626+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-20 01:15:47.793631+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-20 01:15:47.793642+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-20 01:15:47.793646+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-20 01:15:47.793650+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-20 01:15:47.793654+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4428] Phase 5: Translation table ready (7 entries)
2026-10-20 01:15:47.793698+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1349] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-20 01:15:47.931731+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4596] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-20 01:15:47.938577+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-20 01:15:48.019021+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:15:48.019034+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:15:54.476206+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:15:54.476222+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:15:54.477503+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:15:54.477514+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:17:14.163691+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:639] Building translation table (FakeGen=9, RealGen=12)
2026-10-20 01:17:14.163698+0700  localhost kernel[0]: (MyIntelGPU) RCS0 force-active (myintelrcs=1)
2026-10-20 01:17:14.163699+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-20 01:17:14.163701+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:813] Translation table built with 7 entries:
2026-10-20 01:17:14.163704+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-20 01:17:14.163707+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-20 01:17:14.163711+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-20 01:17:14.163714+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-20 01:17:14.163717+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-20 01:17:14.163720+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-20 01:17:14.163723+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-20 01:17:14.163726+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4428] Phase 5: Translation table ready (7 entries)
2026-10-20 01:17:14.163753+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1349] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-20 01:17:14.291070+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4596] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-20 01:17:14.296973+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-20 01:17:14.378175+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:17:14.378187+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:17:14.379442+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:17:14.379453+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:17:14.380724+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:17:14.380740+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:28:47.671378+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:639] Building translation table (FakeGen=9, RealGen=12)
2026-10-20 01:28:47.671382+0700  localhost kernel[0]: (MyIntelGPU) RCS0 force-active (myintelrcs=1)
2026-10-20 01:28:47.671383+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS0 gate myintelrcs=1 -> [active]
2026-10-20 01:28:47.671384+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:813] Translation table built with 7 entries:
2026-10-20 01:28:47.671386+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [0] RCS0: 0x2000 → 0x2000 [active]
2026-10-20 01:28:47.671389+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [1] VCS0 (Video Decode): 0x12000 → 0x1C0000 [active]
2026-10-20 01:28:47.671391+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [2] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-10-20 01:28:47.671393+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [3] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-10-20 01:28:47.671396+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [4] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-10-20 01:28:47.671398+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [5] PCH Display: 0x48000 → 0xC8000 [active]
2026-10-20 01:28:47.671401+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:820]   [6] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-10-20 01:28:47.671404+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4428] Phase 5: Translation table ready (7 entries)
2026-10-20 01:28:47.671426+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1349] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-10-20 01:28:47.803594+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:4596] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-10-20 01:28:47.810472+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: RCS CREATE start mmio=0x2000 (RCS0_BASE_REAL)
2026-10-20 01:28:47.895805+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:28:47.895822+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:28:47.897286+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:28:47.897303+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411E11D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:28:47.899056+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-10-20 01:28:47.899070+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=24 CSBwr=1 ACTIVE=1
2026-10-20 01:44:29.903458+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x40 loops=1
2026-10-20 01:44:29.903467+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=64 CSBwr=2 ACTIVE=1
2026-10-20 01:44:30.011876+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 01:44:30.011888+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 01:44:30.116473+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 01:44:30.116486+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 01:44:30.220159+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x30 loops=1
2026-10-20 01:44:30.220173+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=48 CSBwr=2 ACTIVE=1
2026-10-20 01:44:30.322247+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x40 loops=1
2026-10-20 01:44:30.322258+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=64 CSBwr=2 ACTIVE=1
2026-10-20 01:44:30.430494+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x2 HEAD=0x530 loops=1
2026-10-20 01:44:30.430506+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x40000020041B011D tail=1328 CSBwr=2 ACTIVE=1
--- IORegistry RCS Property Dump ---
    | |   |   |   "RCS-MMIO" = 0x2000
    | |   |   |   "RCS-GGTT" = 0x4004000
    | |   |   |   "RCS-Status" = "CREATE OK"

========================================================================
 🎯 ตรวจสอบงานที่ 3: ระบบภาพดิสเพลย์ 15.3 นิ้ว และ IODisplayConnectFlags
========================================================================
--- Live Display Framebuffer Depth & Connect Flags ---
      "DisplayProductID" = 2340
      "IODisplayEDID" = <00ffffffffffff002c8224090000000027220104a5221378024b3d8f5c599125154f54000000010101010101010101010101010101012a3680a070381f403020350058c21000001a00000000000000000000000000000000001a000000fe004b443135364e32393330413036000000fe004b443135364e323933304130320001>
      "IODisplayConnectFlags" = <00000800>
      "DisplayVendorID" = 11394
--- System Profiler VRAM Pooling & Panel Verification ---
      VRAM (Dynamic, Max): 4096 MB
          Resolution: 1920 x 1080 (1080p FHD - Full High Definition)
          UI Looks like: 1920 x 1080
          Framebuffer Depth: 24-Bit Color (ARGB8888)






# MyIntelGPU — Native Intel Gen 10–12 iGPU Driver for Hackintosh

Standalone macOS IOKit kernel extension that drives Intel Gen 10–12 integrated GPUs
(Xe-LP / Raptor Lake-U, e.g. `8086:A7AC` — Core 5 120U, 80 EU) — hardware Apple
never shipped a driver for.

**No Lilu. No WhateverGreen. No OpenCore injection.** Pure native driver loaded
from `/Library/Extensions`.

> **Version:** 4.4.149 · **Bundle ID:** `com.pongpan-bk.MyIntelGPU`
> **Tested on:** iMac20,1 · macOS Sonoma 14.8.9 (23J631)

## Status

| Layer | Status |
|---|---|
| PCI setup / MMIO (BAR0 regs / BAR2 aperture) | ✅ |
| Display / IOFramebuffer (1920x1080 FHD, backlight) | ✅ — W2-1 cross-connect proven |
| RCS / BCS / VCS rings + execlists submission | ✅ |
| PPGTT identity mapping (GEM buffers) | ✅ |
| Media engines (VDBOX / VEBOX detect via `0x138010`) | ✅ |
| **H.264 hardware decode (VDBOX writes NV12 pixels)** | ✅ **proven** |
| ExecBatch / WaitBatch / RingStatus client API | ✅ |
| GPU render (Metal) | ⏳ opt-in (`myintelaccel=1`), not yet proven |

## Hardware decode proof

```
[READBACK] probe=20480 poison(0xA5)=2091 zero=0 other=18389
[READBACK] => HW เขียนพิกเซลจริง! decode สำเร็จ
[SRC-COHERENCE] aperture vs CPU: MATCH ✅
[PIPELINE] scratch 1531 bytes ← VDBOX เขียน!
[DUMP] wrote 18432 bytes → NV12 128x96
```

The kext speaks the Intel MFX command set directly to the VDBOX engine via the VCS
ring — bypassing VideoToolbox entirely. Apple's private IOGPU protocol is
undocumented, and Apple never wrote a Gen12 driver, so the KBL/Gen9 spoof route is
a crash generator, not a shortcut.

Golden reference dumps from Linux `i915` and the Windows Intel driver live in
`ubuntu-gold/` (register dumps, NV12 frames, MD5 manifest).

## Boot-args

| Flag | Effect |
|---|---|
| `myintelfb=1` | Let `MyIntelFramebuffer` own the real panel (EDID `2c82`/`0924`) |
| `myinteldisplaypipe=1` | Attach the display pipe (W2-1 cross-connect path) |
| `myintelaccel=1` | Opt-in accelerator / IOAccelerator path — **risky on Gen12** |

> ⚠️ `myintelaccel=1` binds the `AppleIntelKBLGraphics` Metal/GL plugin against
> Gen12 registers. Known panic vectors: `stack_chk_fail`, `IOGMD: not wired`.
> Keep a rescue EFI with a safe 2-flag config (`myintelfb` + `myinteldisplaypipe`)
> before testing.

## Build

```bash
make KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK clean
make KERNEL_SDK_DIR=/Users/ppbk/MacKernelSDK -j$(sysctl -n hw.ncpu)
```

## Deploy (L/E only — never OpenCore)

```bash
sudo rm -rf /Library/Extensions/MyIntelGPU.kext
sudo cp -R MyIntelGPU.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/MyIntelGPU.kext
sudo chmod -R 755 /Library/Extensions/MyIntelGPU.kext
sudo codesign -s - --force /Library/Extensions/MyIntelGPU.kext
sudo kmutil install --volume-root /
# reboot, then check:
kextstat | grep pongpan
```

> ⚠️ Do NOT inject via OpenCore `Kernel:Add` or spoof device-id in
> `DeviceProperties` — the kext matches the real PCI class itself.

**Iron rules:** backup → build → deploy → **reboot** → verify. Never reboot
mid-deploy. Keep a bootable rescue path before any risky boot-args change.

## Test

VCS decode harness (proves VDBOX writes real pixels):

```bash
clang++ -x objective-c++ -std=c++14 -O2 \
  -framework IOKit -framework CoreFoundation -framework Foundation \
  test_submit_vcs.m MyIntelVCSCommand.cpp -o /tmp/vcs_test_harness
/tmp/vcs_test_harness test_clip.h264 /tmp/vcs_test_out.nv12
```

## Client API (IOUserClient selectors)

| Sel | Name | Purpose |
|---|---|---|
| 0–11 | legacy GEM/ring/readback | buffer mgmt, submit proof batches |
| 12 | `ExecBatch` | submit caller-authored command stream (i915 execbuffer) |
| 13 | `WaitBatch` | poll HWSP seqno fence |
| 14 | `RingStatus` | head/tail/space/pending telemetry |
| 20–24 | accelerator step machine | IOAccelerator adoption path |

## Architecture

```
Userspace (harness / VideoToolbox-bypass)
    │ IOConnectCall
    ▼
MyIntelVCSClient (IOUserClient) ── GEM buffers, PPGTT identity map
    ▼
MyIntelRing (execlists ELSP) ── BB_START → flush → USER_INTERRUPT
    ▼
VDBOX / VCS0 engine (MFX command stream, Gen12)
```

Source layout: `MyIntelGPU.cpp` (core/rings), `MyIntelVCSClient.cpp` (client),
`MyIntelVCSCommand.cpp` (Gen12 MFX builders), `MyIntelMedia.cpp` (engine detect),
`MyIntelAccelerator.cpp` (IOAccelerator attempt), `MyIntelRing.cpp` (ring/PPGTT),
`MyIntelFramebuffer.cpp` (display / W2-1).

## Hardware ground truth

- **iGPU:** Intel Raptor Lake-U `8086:a7ac` rev 04, 80 EU (Iris Xe), subsystem `1025:192e`
- **BAR0** = 16 MB MMIO regs · **BAR2** = 256 MB aperture / GMADR
- **Display:** eDP panel @ 1920x1080 (FHD), W2-1 cross-connect
  (`kIOFBDependentIDKey = 0x1000004DB`, `kConnectionFlags = 0x800` built-in)

## License & disclaimer

Research / experimental driver. Use at your own risk — kernel extensions can
panic your machine. Always keep a bootable rescue path.
