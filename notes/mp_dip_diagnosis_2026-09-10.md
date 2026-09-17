# Holomatch dips and freeze investigation

Status: unresolved; the performance candidate is not accepted. Diagnose stalls before adding further optimization changes.

Latest: the compact-layout gain did not repeat and remains unqualified. Direct XEMU profiler collection and the resident-triangle experiment are documented in `notes/mp_resident_triangles_2026-09-11.md`; its byte-verification run passed, but the longer performance comparison is pending.

## Compact vertex layout experiment

The captured submission waits justify a bounded layout experiment, not a freeze-fix claim. `r_efInterleavedVertices` defaults to 0 and applies only to MP split-view indexed draws. It packs the enabled position, normal, color, and texture coordinates together, preserving every GPU-consumed bit. Unused fourth position/normal components are omitted; visibility, materials, indexes, and effect selection are unchanged. Persistent world storage still takes precedence when separately enabled.

`scripts/tests/verify_interleaved_vertices.py` compiles the actual packing helper and GPU attribute/stream command writer. All eight formats, constant/array colors, edge vertex counts, guards, signed-zero/NaN bit identity, mismatch detection, and existing planar/world command layouts pass. The existing persistent-world and scratch-fence tests also pass. These are CPU/packet contract checks, not GPU output or speed evidence.

Runtime verification uses `r_efInterleavedVerify 1` and exported `g_SPXBInterleavedVertices`: successful draws, vertices, saved staging bytes, inline fallbacks, verified vertices, mismatches, last stride, eligible draws. Performance comparison must disable verification and use the same production XBE, four moving local players/four bots, two 1 MiB scratch buffers, world storage off, FX culling/batching at 1, and PVS culling off. Change only the interleaving cvar. Keep host progress and guest frame timings separate, retain slow windows, and do not accept the candidate on an average alone. Performance comparison is pending.

Production SP/MP build and freshness checks passed. Frozen candidate: `build/research/letterbox_perf/interleaved_candidate/efmp.xbe`, SHA256 `070ce6c437ec61c57887ce29e8354a069f73e52cbe972a55f6fad8195bbfd4ca`, runtime Sep 10 22:15:29. `interleaved_verify_20260910_221723` completed 180 host seconds with four moving lanes/four active bots, eight playing clients, live endpoint, empty engine error, and ISO restored. It checked 100,520,108 vertices across 1,430,879 packed draws with zero mismatches, including 40,251 inline fallback draws. Saved staging bytes: 621,096,128. Both native captures were inspected and show intact four-view rendering and combat. This verifies data/layout behavior for this bounded run; it does not establish a speed gain or resolve the earlier freeze. Evidence: `notes/evidence/holomatch_20260910/interleaved_verify`. Matched 600-second control/candidate runs are pending.

## Reproduction and provenance

The frozen executable is `build/research/letterbox_perf/world_fx_basepass_candidate/efmp.xbe`, SHA256 `9696532b4a8f6c8e3015324175221c3c98daa414374bac963ce916942708599a`. XEMU 0.8.134, four moving local players, four explicitly added bots, hm_borg1, original two 1 MiB scratch buffers. Persistent world vertices are off; FX culling and conservative batching are on. The newer PVS/animated-batching drafts are not compiled into this executable.

`dip_flight_fx_20260910_210517` completed its planned 600 host seconds. Its ISO transaction reports restored=true. It captured a long frame stall and recovered; this does not resolve the earlier effects-only run that stopped advancing for at least 30 seconds before the emulator exited.

## Captured long stall

Main loop 7685 stopped advancing for an observed 3.689 host seconds. At host t=384.020 the recorder captured EIP 0x002089bc inside DirectSound::CMcpxVoiceClient::SetVolume. The exact frozen PE/XBE section mapping was checked. Disassembly at this address is `mov dword ptr [0xfe820360], edx`, an audio-register write. The saved stack contains return addresses through PlayFromCurrent, Play, alSourcePlay, PlaySingleShot, S_Update_, and S_Update.

The render breadcrumbs were completed: ClTailStage SS05 (screen update returned), RenderListStage DS04 (draw surfaces finished), and NativeSubmitStage ND08 (indexed submission returned). The saved sunny_flare shader is the last submitted shader, not proof that it caused this stall.

This is a 3.689-second FRAME stall sampled inside an audio playback call. One instruction snapshot does not prove that the same instruction or call consumed the whole interval, nor that all dips or the earlier sustained freeze share this cause. The repeated run adds register/stack samples every 0.5 seconds to test that distinction. Audio-quality work remains paused.

## Recurrent rendering slowdown

The first flight run recorded the following one-minute cohorts. These are diagnostic main-loop advances per host wall second, not presentation FPS or a retail qualification. Buffer waits use the engine's guest millisecond counter and must not be equated to host wall time.

| Host interval | Loops/host second | Scratch draws/loop | Scratch buffer wait, guest ms/loop |
| --- | ---: | ---: | ---: |
| 60-120 s | 28.44 | 336.71 | 0.68 |
| 180-240 s | 20.14 | 349.48 | 5.27 |
| 300-360 s | 9.38 | 354.73 | 13.74 |
| 360-420 s | 5.64 | 355.67 | 20.28 |
| 420-480 s | 8.00 | 375.53 | 22.68 |

Many slow observations have NativeSubmitStage ND04: the title has entered Direct3D BeginPush and has not yet obtained command space. Other observations catch buffer-fence waits at the start of a frame. This identifies graphics submission/back-pressure as a concrete investigation target. It does not distinguish expensive GPU work, emulator scheduling, or host contention by itself. Similar draw counts do not guarantee equal vertex count, fill work, or effect cost.

Production-build detailed render counters are inactive (PerfSampleActive=0); zero values are not evidence of zero rendering work. Asynchronous observations are not atomic frame timings, and sampled frame age is a lower bound since the last observed counter advance. Stack-word symbol candidates are not a verified unwound backtrace. Kernel PCs remain unresolved rather than being assigned the nearest title function.

## Tools and checks

`scripts/xemu_flight_recorder.py` reads only the targeted emulator process. It uses its native RAM mapping, fresh guest page-table translation, and an independently checked heartbeat mapping. It does not scan RAM, pause for sampling, or send desktop input. Three address-translation tests pass. `scripts/analyze_xemu_flight.py` translates function addresses by matching PE/XBE section occurrences; known DirectSound/alSourcePlay addresses, kernel exclusion, and unmapped-address rejection were checked against the frozen artifact. The Python tools compile.

The second run, `dip_stack_fx_20260910_212059`, repeats the same settings while adding stack samples and bounded read-only host/audio counter collection. It completed 600 host seconds with final liveness and four-moving-player/four-bot workload verified, no engine error, and the ISO restored. It reproduced a milder slowdown (22.84 loops/host second at 60-120s, 14.19 at 300-360s), with maximum observed frame age 0.569s; it did not reproduce the sustained freeze. Guest elapsed average 30.67 FPS does not override the slower host progress or qualify a gain. Audio memory observed during the latter part of the run ranged from 423,448 to 441,412 bytes, with zero recorded voice-play failures; this does not prove the audio path is free of stalls. Evidence is preserved in notes/evidence/holomatch_20260910/dip_stack_fx. No emulator remains running from this probe.

An external source check of the exact XEMU revision identifies its voice register handling and audio-worker synchronization as possible follow-up locations, not an established emulator bug: [voice processor](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/mcpx/apu/vp/vp.c), [APU worker](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/mcpx/apu/apu.c).


## Completed-frame diagnostic follow-up

The previous goal turn produced new evidence (two stall/slowdown recordings), so it was progress rather than a no-progress turn. The next safe action is a completed-frame cost breakdown, not another speculative optimization.

The existing earlier diagnostic also measured reservation at roughly 90-98% of draw-call cycles in bad samples. Disassembly shows D3DDevice_BeginPush first calls CDevice::LazySetStateVB, then D3DDevice_BeginStateBig. Consequently its total time must not all be described as waiting for free command-buffer space. MakeRequestedSpace separately calls GpuGet and, when necessary, BlockOnTime.

Diagnostic-only changes: completed-frame snapshot v1 (39 words), published with a serial cleared before writing and set last. The read-only flight recorder rejects short reads, zero/changing publication serials, and unknown schemas. Diagnostic sampling is every 250 guest ms; production keeps the original 5000 ms cadence and contains no new snapshot. Six tests pass, including the writer/reader field layout and partial/torn-publication rejection. Existing graphics/audio phase timers are grouped only after their frame completes.

A read-only SDK 5558 queue observation also reads D3D__Device PUT/THRESHOLD and the verified private queue/fence fields. It validates pointer ranges and only follows guest RAM mappings, never MMIO. cached_get is a cached driver observation, not a live hardware GET read; do not infer queue occupancy or a stopped GPU from that field alone.

Build dip_frame_snapshot_build.log completed with SP/MP freshness checks. Frozen diagnostic MP XBE SHA256 f80df3716042c952844024636762bd81f31649c91b0a399408f031c33d872049, runtime Sep 10 2026 21:47:00. It is stored with matching PE/map in build/research/letterbox_perf/dip_frame_snapshot_candidate. The exact failed production binary remains separately preserved. Diagnostic builds also use memory-only logging, so comparison against production changes logging and instrumentation; it cannot isolate either factor or prove a gain by itself.

Started dip_frame_cost_fx for 600 host seconds, same moving four-local-player/four-bot workload, static world off, culling1/batching1/PVS0, original scratch buffers. Broader PVS/animated batching drafts are compiled but explicitly disabled; no new optimization is being evaluated. Completed 600 host seconds, live endpoint and four-moving-player/four-bot workload verified, empty engine error, original scratch layout confirmed, PVS counters all zero, and working ISO restored. Guest elapsed average33.4430/minimum17.6178 FPS is diagnostic only and does not qualify a gain. The completed snapshots, observed wait examples, and full report are saved under notes/evidence/holomatch_20260910/dip_frame_cost_fx.


The live follow-up produced a concrete graphics-drain observation. Completed sample550/mainloop5451: total106 guest ms, server4, client102, audio2, EndFrame91, backend draw surfaces90. Draw cycles67,724,794; BeginPush64,111,511; largest individual BeginPush58,891,360 cycles for708dwords, state0x22. During that same mainloop, host221.027 and221.080 samples both report backend draw surfaces/NativeSubmitStage ND04, issued fence65083 unchanged, completed fence65063 then65073. Graphics completion was progressing while the main thread remained in reservation. This supports GPU submission back-pressure for this dip, not a total GPU deadlock. Sample578/mainloop5666 similarly has152guest ms and pending fences67755/67735 ->67755/67743 during the reservation wait.

Counter-unit correction: PerfRenderTotalMsec/PerfRenderWorldMsec/PerfRenderEntitiesMsec come from RDTSC divided by nominal Xbox733333cycles/ms, whereas frame/audio/backend-surface timers use Sys_Milliseconds. A sampled frame showed nominal-TSC renderer298ms versus90guest ms total. This is not a valid additive frame budget in XEMU. Keep nominal-TSC fields labeled separately; raw same-clock draw/BeginPush cycle fractions remain useful. The completed snapshot prevents mixed-frame publication but cannot reconcile different clock sources. Production FPS acceptance remains based on approved full-window timings, with host progress separately reported.

Push packet capacity was also inspected as a possible corruption source: active SHADER_MAX_INDEXES is6000, not an assumed larger desktop value. The current index packet headers/layout fit the60dword fixed overhead at that bound. No push-packet overflow was established by this review.


A targeted next candidate follows from this evidence: reduce repeated dynamic vertex staging and geometry-buffer updates by packing enabled attributes together, alongside the existing persistent world storage. The current dynamic fallback writes separate position/normal/color/UV arrays. The exact pinned XEMU OpenGL implementation checks dirty memory and may call glBufferSubData separately for the enabled attribute ranges; interleaved ranges can share that update. This is an inference from source, not a measured count or proof that uploads caused the observed stalls. It must be tested against the same frozen baseline with byte-equivalent attributes and unchanged visibility, before claiming a gain. Source: https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/nv2a/pgraph/gl/vertex.c . Do not substitute changing emulator backends for optimizing the Xbox title or count an emulator-setting change as a title FPS gain.
