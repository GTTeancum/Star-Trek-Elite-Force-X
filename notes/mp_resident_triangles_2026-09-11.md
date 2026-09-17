# Holomatch resident-triangle experiment

Status: completed this optimization pass for the tested XEMU workload. A roughly 15 native-XEMU-FPS gain was reproduced, including a live twenty-minute full-workload soak averaging 33.12 native FPS across confirmed gameplay. The fresh production build also passed its normal-default smoke. Resident world mode 2 and conservative FX culling/batching mode 1 are enabled; interleaving, PVS culling and animated extended batching remain off. Retail performance and the cause of every historical sustained freeze are not established. Audio quality, SP and co-op optimization remain paused.

## What the new measurements established

The compact dynamic layout passed byte verification but did not produce a repeatable gain. The first matched comparison improved diagnostic host progress from 16.96 to 20.92 game loops/second; the repeat went from 18.52 to 15.27. Both pairs used four moving local players/four active bots and finished live with no engine error. These measurements are not retail or exact presentation FPS. Do not adopt the layout based on the favorable first run.

The new native reader uses the exact statistics underlying XEMU's Video Debug graph. It verifies executable SHA256 `f59a9df35f7d3be20da091d14066d4670e97ec9ee27ddf1d9d4b1abf99d2fe72`. Binary FPS/MSPF references independently identify stats RVA `0x14a0100`, history offset `0xc8`, record stride `0xb8`, 300 records, and ring-pointer RVA `0x14ad968`. It uses read-only process handles, never UI input or an open debug window. Stable publication checks and omission of the oldest slot avoid reading a slot the writer may be replacing. Initial history is excluded from analysis.

Source interpretation matters: native MSPF measures the host-clock interval from a read-increment event to flip-stall processing. It is neither a GPU-only timer nor reciprocal presentation FPS. Counter values measure work counts, not time in each API. `GEOM_BUFFER_UPDATE_1` counts guest-memory vertex uploads; `_4` counts index-buffer data uploads for index-cache misses. Shader generation and texture uploads were zero in the captured worst control frame, not necessarily every frame of the run. Primary sources at revision `fc9980d2962cbec656253106ea2e121fab1e68d4`: [profiler](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/nv2a/pgraph/profile.c), [draw path](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/nv2a/pgraph/gl/draw.c), [vertex uploads](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/hw/xbox/nv2a/pgraph/gl/vertex.c).

In `native_dip_control_20260910_224947`, all 10,144 captured records after initial backfill were analyzed. The fast quarter had median 20 MSPF and 368 batches; the slow quarter had 79 MSPF and 499 batches. The worst record had 185 MSPF, 742 batches, 588 vertex uploads and 116 index uploads. The compact-layout repeat lowered vertex uploads per draw from 0.529 to 0.311, while index uploads per draw stayed around 0.093. Despite the mechanical reduction in vertex uploads, performance did not improve reliably. Correlation does not establish the time cost of either upload category.

## New implementation

`r_efWorldVertices 2` stores complete triangles for the existing eligible immutable BSP faces/triangle surfaces. It emits bounded native DRAW_ARRAYS ranges instead of building a varying index list each draw. Exact vertex bits, surface order, triangle order, material stages and visible subsets are preserved. 255-vertex chunks keep every range aligned to complete triangles. Unsupported/cross-surface/fragmented batches, allocation failure and capacity exhaustion fall back to the original path. Dynamic attributes retain that path too.

The allocation is 43,680 vertices × 48 bytes (2,096,640 bytes). Modes 1 and 2 share storage exclusively, and changing modes requires a fenced map reset. The unused padding word records source topology for byte verification; it is not sent as a GPU attribute. No cache eviction or overwrite can invalidate in-flight geometry. `g_SPXBWorldArrays` records successful draws, stored expanded vertices, range commands, topology/fragmentation fallbacks, capacity fallbacks, verified vertices, mismatches and actual mode.

Native tests exercise all eight attribute formats, exact triangle order, reordered visible surfaces, topology changes, bounds, capacity/allocation failure, mode changes, fenced reset and the actual emitted packets. Existing indexed-world and dynamic-layout tests still pass.

## Runtime verification

Frozen production XBE: `build/research/letterbox_perf/world_arrays_candidate/efmp.xbe`, SHA256 `60aaf3d4fca90753d4c3c674900e105ebb4889a160627d6119cbf5342951ee81`, runtime Sep 11 00:34:39. Fresh SP and MP builds passed.

`world_arrays_verify_20260911_003546` completed 180 host seconds with the full moving-player/bot workload, live endpoint, empty engine error and restored ISO. It verified 63,032,088 expanded vertices with zero mismatches across 701,222 resident-array draws. Storage held 28,104 expanded vertices; there were no capacity/topology fallbacks. Free physical memory at the endpoint was 2,867,200 bytes. Both native captures were inspected; XEMU's counters confirmed actual array draws. Index uploads per draw averaged 0.0063 in this short run. This is functional/mechanism evidence, not a matched speed comparison.

The longer comparison uses the same frozen XBE, original two 1 MiB scratch buffers, compact layout off, FX culling/batching at 1 and PVS culling off. Only resident-world mode changes from 0 to 2. Do not accept a gain from short startup windows, changed workloads, or guest-clock FPS alone.

Evidence is under `notes/evidence/holomatch_20260910/`: `interleaved_candidate`, `native_dip_control`, `native_dip_candidate`, `world_arrays_verify`, both comparison JSON files and native-counter binary-reference notes. Earlier freeze and frame-cost evidence remains in `notes/mp_dip_diagnosis_2026-09-10.md`.

## First complete resident-array comparison

`world_arrays_control_20260911_004101` and `world_arrays_candidate_run_20260911_005238` both completed 600 host seconds, verified the full eight-participant moving workload, finished live with no engine error, and restored the ISO. These are the same frozen production binary and settings except world mode 0 versus 2.

The whole shared gameplay intervals yielded 15.604 versus 30.136 game loops per host second (+14.533, +93.1%). This is diagnostic progress timing, **not exact presentation FPS or retail performance**. Boundaries differ slightly, and final FPS-ring retention limited the shared server-time range to approximately 196–329 seconds. Guest elapsed-clock averages were 30.880 versus 37.549 FPS. Over-50-ms guest frames fell from 1,077/9,963 to 283/12,051; maximum completed guest frame fell from 482 to 95 ms. Native slow-quarter median MSPF fell from 79 to 40 despite similar median draw counts (463 versus 471). Candidate storage held 28,455 expanded vertices, with no topology/capacity fallback and 2,863,104 physical bytes available at the endpoint.

This first comparison alone was insufficient for acceptance. The reversed control and independent twenty-minute candidate soak reported below subsequently reproduced the gain and tested whether saturation merely delayed the slowdown.

## Specific XEMU cache defect found

The pinned source's `lru_get_one_free` scans the entire active list when no free node remains, then calls eviction. The element cache has 51,200 entries. The same loop is present in the v0.8.136 source. Sources: [pinned LRU](https://github.com/xemu-project/xemu/blob/fc9980d2962cbec656253106ea2e121fab1e68d4/include/qemu/lru.h), [v0.8.136 LRU](https://github.com/xemu-project/xemu/blob/v0.8.136/include/qemu/lru.h).

An isolated native test compiled the exact downloaded LRU and queue headers, instrumenting only the queue iterator to count visits. A miss visited one node before saturation and 51,201 nodes after saturation (51,200 unsuccessful checks plus eviction); a hit visited none in that reverse scan. Smaller capacities reproduced the same linear scaling. Test: `build/research/letterbox_perf/verify_pinned_lru.py`; output: `lru_probe/result.txt`. No emulator binary or installed emulator was modified.

The control recorded 367,790 index-cache misses, reaching 51,200 observed misses by host second 89.64. The first candidate remained below that threshold through ten minutes. Initial history is included and earlier unobserved misses are unknown; the threshold is a pressure indicator, not direct cache-occupancy or stack timing. This is a reproducible algorithmic defect and a strong explanation to test against late-match degradation, not proof that it caused every reported freeze.

Windows WPR CPU sampling could not start (`0xc5585011`, failed to enable profiling policy); a subsequent status check confirmed no recording remained. No permission escalation or desktop interaction was used. A subsequent bounded, emulator-only register sampler established the live loop evidence below.

## Additional diagnostic coverage

`xemu_host_process_stats.py` now records cumulative CPU/thread time, memory/page-fault/IO counters and available host memory for the single target process. It uses read-only process queries and no UI APIs. The repeat included it; the first candidate used a separate bounded sidecar starting after initial gameplay. Its observed working set stayed around 0.8 GiB, with more than 10 GiB host memory available in the inspected snapshots; this does not establish control-run memory behavior. A separate bounded GPU-counter sidecar also recorded only the candidate PID.

The flight recorder now also retains newly published FPS-ring records before the 64-record ring rotates, using before/after index validation and excluding the slot the writer can overwrite. Nine recorder tests pass, including wrap, incomplete reads and torn publication. The long soak and emulator-guard comparison used it; the earlier repeat control did not. The beta package is unchanged.

## Live binary confirmation and reversed control

The repeat control, `world_arrays_control_repeat_20260911_010457`, finished live with the required workload and restored ISO. Its slow-quarter native median was 74 MSPF. Comparing the first candidate against this later control gives 16.643 versus 30.743 game loops/host-second (+14.100).

`analyze_native_frame_progress.py` additionally counts XEMU's own read-increment frame counter within the confirmed, whole gameplay intervals. This is the counter behind the emulator's FPS display; it is not reciprocal MSPF or exact physical-display presentation timing. The first comparison gives 15.765 versus 31.033 native frames/host-second (+15.268); against the reversed control it gives 16.908 versus 30.831 (+13.924). Three tests cover adjacent intervals, excluded gaps, resets and missing data. This is one candidate run compared with controls on both sides, not two independent candidate runs.

Dedicated diagnostic run `lru_runtime_probe_20260911_011617` is excluded from FPS acceptance. Its five-second register capture at host seconds approximately 87.07–92.27 briefly suspended each selected XEMU thread only long enough to copy its context, immediately resuming it before any output or wait. There were no pre-existing suspension counts or sampler errors. The busiest sampled in-image function occupied RVA `0x25ade0–0x25b079`; 133 of thread 11724's 320 observations landed in the reverse scan at `0x25af40–0x25af55`. These observation counts are not CPU-time percentages.

Disassembly shows that exact loop walking the reverse pointers and testing the node's in-use field. Nearby assertion strings identify `/__w/xemu/xemu/include/qemu/lru.h` and `found != NULL`. Read-only inspection of the captured RSI cache pointer found `num_used=51200`, `num_free=0`, and initialization callback RVA `0x24fab0`. That callback copies the 32-byte VertexKey and clears the initialized flag at offset `0x48`, matching the element-cache node layout. This confirms the expensive full-cache scan is active in the pinned live executable. It does not establish the cause of every historical sustained freeze.

The second file named `lru_late_pc` was captured at host seconds 186.79–191.91, after the 180-second gameplay phase ended. It is an endpoint-phase capture and must not be used as evidence that the loop disappeared during gameplay. The diagnostic run itself completed live with an empty engine error and restored ISO.

## Isolated emulator guard experiment

The native source experiment with a free-count guard reduced full-miss reverse visits from 51,201 to one, preserving the observed empty-cache and hit behavior. `make_xemu_lru_probe.py` creates a **separate diagnostic executable**, not an installed-emulator replacement. SHA256 is `0541e4828a0e66d5057180b5a2a34a775543fc3b784dabc8f5cd92802b8cfb82`. Three verified byte sites implement that guard using unreachable alignment padding within the existing LRU function; stack behavior, image layout and unwind metadata are unchanged. The original executable's hash was rechecked unchanged. The exact changes and decoded branches are in `xemu_lru_probe.manifest.json` and `.disassembly.txt`.

The native reader accepts this exact additional hash with an explicit `isolated_lru_guard_probe` identity. Other executable hashes still fail closed. The comparison below disabled the game resident-world optimization to isolate the emulator guard. No speed gain from that run may be attributed to Xbox game code. The copy is diagnostic-only and not for distribution.

The twenty-minute original-emulator world-array run `world_arrays_long_20260911_012103` completed live with four moving local players, four active bots, no engine error and a restored ISO. It crossed 51,200 observed index misses at approximately 663 host seconds and remained live afterward. All 203 FPS publications were retained contiguously and checked against the paused endpoint. The shared gameplay comparison against the repeat control measured 18.880 versus 33.808 native XEMU frames/host-second (+14.927). This uses a broader approximately 64–344-second shared server interval than the first candidate's final-ring-limited comparison.

Across the full twenty-minute recording, all one-minute diagnostic progress windows were 28.79–36.05 game loops/host-second; maximum observed frame age was 0.164 seconds. These asynchronous observations do not prove a physical-display minimum FPS. The native slow-quarter median was 39 MSPF. Resident storage held 28,488 expanded vertices, with no capacity/topology fallbacks; physical memory available at the endpoint was 2,863,104 bytes. Final-ring guest elapsed FPS was 38.729, which is not the native host rate.

The separate emulator-guard run `xemu_lru_guard_control_20260911_014259` then completed ten minutes with resident world storage and interleaving both disabled. The full workload, live endpoint, empty engine error and ISO restoration all passed. Native frame rate over matched gameplay increased from 18.880 to 31.910 (+13.029). Index uploads/draw remained high at 0.0876, comparable to the original control, while slow-quarter median MSPF fell from 74 to 42. This isolates a substantial gain from the emulator guard itself. Both native captures (`xemu-2026-09-11-01-45-40.png`, `xemu-2026-09-11-01-50-23.png`) were inspected and showed intact four-player combat. No emulator binary is included in the game release.

Interpretation warning: the legacy FPS line's `bots=0` is computed as local view count minus local human view count; it describes bot-controlled viewports, not opposing server bots. Actual opponent qualification uses the server's connected/playing roster and 32-word bot-startup evidence. The completed candidate had eight playing participants and four active bots. Do not infer an empty match from that misleading legacy label.

## Final production defaults — passed

Fresh `spmp` compilation refreshed both production XBEs and passed runtime-identity/freshness checks. Final `build/release/efmp.xbe` SHA256: `1f155977c67d71bca5587e95db073617ac5b289e25fca6d317f7eee0afedfb81`, runtime Sep 11 02:00:07. SP runtime is Sep 11 01:57:28. The frozen pair of MP executable/symbol artifacts is in `build/research/letterbox_perf/world_arrays_default_candidate/`.

`world_arrays_defaults_20260911_020250` completed 180 host seconds on the original pinned XEMU with **no `r_ef` overrides**. It verified four moving local players, four active bots, eight playing participants, a live endpoint advancing 34 main loops, empty engine error and restored ISO. Actual resident mode was 2, with 899,430 draws and 28,023 stored expanded vertices; no topology/capacity fallbacks. Culling rejected 1,327,605 out-of-frustum effects and batching merged 531,189 eligible draws. Interleaving/PVS proofs remained zero. Scratch storage was the original two 1 MiB buffers (262,144 DWORDs each).

The native counter measured 34.87 FPS across 134.92 seconds of confirmed gameplay. This short default-mode check is not an additional matched gain claim. Slow-quarter native median was 34 MSPF; maximum observed frame age was 0.107 seconds. The native capture `xemu-2026-09-11-02-05-29.png` was inspected and showed intact four-view combat, materials and HUDs. The longer previously tested candidate averaged 33.12 native FPS across 1,141.41 seconds of confirmed gameplay; the matched comparison remains 18.88 to 33.81 (+14.93 FPS).

To check sustained dips within that long run, all 114 complete non-overlapping ten-second native-counter windows were evaluated: 25.51–41.40 FPS. All 19 complete sixty-second windows were 30.13–36.68 FPS. Window endpoints are trimmed to actual observations and must retain at least 80% of nominal duration; these are window averages, not per-frame minima. Evidence: `world_arrays_long.native_gameplay_windows.json`.

The normal release XBE contains the change. The working test ISO was restored and the existing beta ISO was not rebuilt; neither should be described as containing this final XBE. No source commit, public distribution or emulator replacement occurred. Expanded triangles preserve visual data but can reduce GPU vertex reuse, so retail Xbox performance still needs its existing hardware qualification.

Raw captures, endpoint memory, identities, transactions and analyses are retained under `notes/evidence/holomatch_20260910/`, with `world_arrays_evidence_manifest.json` recording SHA256 provenance. Large flight streams are losslessly compressed as `.flight.jsonl.gz`; decompress before running the existing analyzers. The separate LRU diagnostic executable is not retained in the release evidence or package.
