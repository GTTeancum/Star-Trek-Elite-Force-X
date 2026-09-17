# Holomatch performance investigation

## Full match baseline

The 600-host-second `beta_4p_full_bots_moving` run ended live, with no engine
error. The endpoint confirms eight playing clients, four actual bots, and
advancing movement commands for all four local players. The preceding visual
check captured movement and combat. Requested bot commands alone are not proof.

The retained 64 complete timing windows cover 322.726 guest seconds and 8,250
frames: **25.56 guest elapsed FPS overall**. The final twelve windows average
**19.44 FPS**, and 17 five-second windows fall below 20 FPS (lowest 15.09).
This is not yet consistently above 20. Keep the beta item open for that reason,
not because it misses the superseded 30 FPS ideal. Guest speed does not establish
host presentation speed or retail Xbox performance.

Evidence: [full workload soak](evidence/holomatch_20260910/full_bots_moving_soak/assessment.json).
Executable SHA256: `cbd9b065cba95d9e8d243866101d12f98956b97bafeb8bbb9c5a2dac83faa271`.

## Where to optimize first

1. **Rendering and animation.** The late recorded long frames spend 240–324 ms
   in the client versus 5–13 ms in the server. The short instruction profile
   attributes 12 of 165 samples (7.3%) to `RB_SurfaceAnim`. Bone palettes already
   have a cache; vertex processing still repeats for visible models across
   views. Earlier rejected pose-cache experiments must not be reinstated without
   understanding their failure and demonstrating a measured improvement.
2. **Repeated configuration searches.** Case-insensitive string comparison
   routines account for 16 of 165 samples (9.7%); another four land in cvar code.
   These are samples, not a call-stack attribution. One confirmed source-level
   example is `RB_BeginSurface` calling `R_STEFX_ShaderTraceDumpRegistered` for
   each batch, which performs a name-based cvar lookup even with tracing disabled.
   Cache registered cvar handles or move frame-invariant checks out of batch
   loops, with runtime toggle behavior preserved and before/after measurement.
3. **World-state projection.** The MP adapter copies every entity after each
   `ClientThink`, as well as after bot and game frames. Each entity projection
   clears the mirror and clears its entity state again. This is redundant work,
   but the short profile does not establish it as a leading cost. Do not defer
   synchronization blindly: collision imports and snapshots consume the mirrors.

The cgame constructs its scene once; the renderer then draws four viewports.
There is no evidence that four complete cgame simulations are being run.
Visibility traversal and rendering remain per view. Lowering visual quality
is not the first step.

## Profile limits

The separate diagnostic samples native XEMU register reports, which perturb
execution and cannot qualify FPS. It was initially paused for debugger setup;
only host times 100–150 seconds were analyzed, after resuming and booting.
The sample contains 165 observations, with 27.3% in unresolved kernel routines.
It is directional evidence, not a precise breakdown of frame cost, and does not
capture the late-match slowdown from the longer soak.

Evidence: [instruction profile](evidence/holomatch_20260910/mp_profile/instruction_profile.txt).
Audio work remains paused.

## First candidate: shader-trace lookup

`tr_shade_retail.cpp` now registers and retains the shader-trace cvar handle,
then reads its live integer in the surface-batch path. The D3D bridge already
uses this approach; the retail surface entry point still used a linear search
through the cvar list. Trace toggles remain live. `Cvar_Get` registers any
user-created value so `cvar_restart` resets it rather than removing its storage.
A one-time startup breadcrumb identifies the new path. No visual setting changes.

Release build passed (efmp September 10, 12:30:14). The 600-second
`beta_4p_cached_trace_moving` comparison completed live with four moving local
players and four verified bots (eight connected/playing, no engine error).
The ISO was restored. Candidate identity and build transcript are in
[candidate evidence](evidence/holomatch_20260910/cached_trace_candidate/candidate.json).
The benchmark harness qualification tests passed before the candidate run.

The paired-comparison tool now independently checks the bot roster and all four
movement lanes instead of accepting `simulation_verified` alone. Its nine tests
pass, including rejection of empty/stationary matches carrying a stale success
flag. This also permits checking the original soak from its raw evidence even
though its summary predates the added workload flag.

Candidate retained gameplay: **30.57 guest FPS**, final twelve windows **24.41**,
minimum five-second window **14.80**, eight windows below 20. Baseline overall
was 25.56, final twelve 19.44. The shared-server-time comparison is 24.72 versus
31.09 (+25.8%); whole-window boundaries differ slightly. Keep the candidate.
This is one pair of moving combat runs, not deterministic replay, so it supports
the candidate but does not attribute the entire difference to this single edit.
It meets the 20+ average on hm_borg1 in this run, not a new 30 FPS requirement.
Representative-map consistency and retail/host presentation remain unverified.
See [paired comparison](evidence/holomatch_20260910/cached_trace_candidate/comparison.json).

The same executable is now undergoing a 600-host-second hm_voy1 breadth run,
`beta_4p_cached_trace_voy1`, with identical four-player movement and four explicit
bot commands. This run is pending; launch configuration does not verify its
actual roster, map progression, or speed. No further visual reductions applied.

The probe's MP timing gate now follows the user's above-20 average threshold,
instead of requiring every window to reach the historical 30 FPS target. Slow
windows remain reported. This is only a measured guest-window gate; it is not
retail or across-map acceptance. Threshold, missing/invalid average, workload,
and paired-comparison tests pass. The already-running hm_voy1 process loaded
the old reporting code; assess its completed evidence under the updated rule
without treating its old minimum-window pass field as the current beta gate.

## Second-map result and optimization closure

hm_voy1 completed its 600-host-second run live with no engine error, eight
playing clients, four actual bots, and all four movement lanes verified.
Average **28.62 guest FPS**, last twelve windows **27.39**, minimum five-second
window **12.79**, three windows below 20. ISO restoration completed. The current
above-20 average gate passes. Evidence: [hm_voy1 assessment](evidence/holomatch_20260910/voy1_full_workload/assessment.json).

Close the MP optimization list item per the user's instruction: both tested
full-workload maps average above 20, including their final minutes, and neither
ten-minute run crashed. This does not assert every map is qualified or establish
host presentation/retail speed. Keep hardware validation as a separate release
check, with no deferred 30 FPS optimization pass. The overall SP/letterbox goal
remains active.

## Beta packaging follow-up

The user corrected the return to SP optimization. Focus remains Holomatch beta readiness; do not resume SP optimization from the older automatic goal. The two completed full-workload MP results remain valid for their recorded executable hashes, and the optimization item remains closed under the revised 20+ average threshold.

Release preflight found stale XBE/PK3 inputs. Current efmp.xbe before refresh hashes to 50149f5d0bc3227026ff8225c713a326c783d28f36e801c67555c3ee9b45180f, different from the qualified d8348053e323170e1c34946be9097224257c352d0184b2f4349359fa63c85d71. Do not transfer qualification to new bytes without verification. The unaccepted SP tangent experiment was removed while preserving the earlier flare-query fix; its repeat wrapper has finished and its ISO transaction reports restored=true.

Started release/package refresh with scripts/build_xbox.ps1 -Target spmp -SkipStage -ReuseObjects. Log: build/research/letterbox_perf/beta_release_refresh_20260910.log. This is release preparation, not a new SP optimization pass. Completion, package freshness, diagnostic exclusion, and final executable provenance still need checking. Nothing has been published.

Package preparation also corrected scripts/package_beta.ps1: removed obsolete four-player-future and co-op-qualified claims, linked current evidence with explicit hash scope, and replaced hard-link output with an independent ISO copy so diagnostic writes cannot mutate a packaged beta. Added output containment and source/output identity checks. PowerShell parser and git diff whitespace checks passed. End-to-end packaging remains pending the active asset build.

The beta packager now verifies actual XDVDFS component bytes against the manifest via scripts/verify_beta_iso_components.py. It fails on any mismatch instead of describing current build files as though they were necessarily inside an older ISO. Exact equality is required (including XBEs); a media-patched XBE must be reconciled explicitly, not silently accepted. Four fixture tests cover valid nested lookup, stale content, missing component, and truncated payload; all pass. Real package verification remains pending asset-build completion.

Direct ISO payload inspection confirms the restored working ISO still holds default Sep 9 13:18:59 and efmp Sep 7 12:12:19; current refreshed release XBEs are Sep 10 13:45:09 and 13:46:09. Recorded in build/research/letterbox_perf/beta_prepackage_identity_20260910.json. Repack the clean release ISO after assets finish, using run_sp_xemu_smoke.ps1 -RepackOnly -CleanReleaseIso (no SP gameplay run). extract-xiso applies an expected one-byte HDD/media patch; final manifest needs actual packaged hashes plus verified source linkage, following stage_hardware_pk3_test.ps1, before the strict new verifier can qualify that package. Do not bypass a mismatch.

Actual extract-xiso roundtrip passed for both refreshed XBEs. The verifier now records packaged sha256 separately from sourceSha256 and accepts only the existing one-byte 7D-to-EB media patch contract; unrelated differences or changed source hashes fail. Proof: build/research/letterbox_perf/beta_xbe_roundtrip_20260910.json. Six fixture tests pass, including source mutation and unexpected patch rejection. The temporary executable-only ISO was removed after verification; this is packaging proof, not runtime/performance qualification.

Release refresh completed successfully (exit 0). Final sound bank: 597835610 bytes, 8429 records, sorted unique CRC keys, contiguous ranges. CheckFreshnessOnly passed for both XBEs, both PK3s, and runtime build identities; cleanup completed. Started clean ISO repack with run_sp_xemu_smoke.ps1 -Repack -RepackOnly -CleanReleaseIso -KeepIso -Name beta_clean_release_20260910. Repack log: build/research/letterbox_perf/beta_clean_repack_20260910.log. RepackOnly performs no emulator gameplay launch.

Clean repack and local beta packaging completed successfully. Candidate: build/beta/StarTrekEliteForceX-Beta-20260910/StarTrekEliteForceX-Beta-20260910.iso (2275999744 bytes; SHA256 D706AB34FC4E65E31EFA5410FDB97640885747B315FBABC31DC6CC77A8DD52D4). It is an independent copy, with zero diagnostic markers and verified component provenance. No public publication has occurred.

The final packaged Holomatch executable (SHA256 0d88e554403af027b4086d6fab983e6a330e7f9128c82e643689260e8d20ff39) is undergoing beta_final_packaged_4p: 600 host seconds on hm_borg1, four local movement lanes and four explicitly requested bots. Runtime roster, retained timing, and restoration must still be verified. The clean ISO exposed a harness assumption that ef_sp_level.txt already existed; the initial attempt failed before ISO writes. The probe now adds the map marker through its existing temporary root-directory transaction. The root-directory regression test passed before retry. The user's slower 5400 RPM storage extended packaging; avoid duplicate jobs or treating slow progress as a failure.

## Resumed final package check and performance direction

User paused tests to reduce concurrent machine load, then explicitly authorized continuation. The suspended processes had exited by continuation; no unrelated/reused PID was resumed. Recovered the working ISO from its transaction record and verified its complete SHA256 matches the clean beta ISO (D706AB34FC4E65E31EFA5410FDB97640885747B315FBABC31DC6CC77A8DD52D4). The interrupted beta_final_packaged_4p run is invalid for acceptance. A fresh 600-second beta_final_packaged_4p_resumed run is active with the packaged executable, four movement lanes and four requested bots.

User performance direction: if further FPS gains are needed, target substantial bottlenecks capable of 5–10 FPS gains, preserving current visual fidelity. Do not delay beta for marginal improvements when the full four-player/four-bot workload clears the above-20 average threshold. Profile major repeated rendering/animation work before proposing new optimizations; this direction does not reopen a deferred 30 FPS item or authorize returning to SP. Concurrent unrelated test processes were visible on the host during the fresh run; guest-clock results must not be described as isolated host-presentation performance.


## Final optimization push — explicitly requested

The user now requests one final Holomatch optimization push. Target a substantial measured improvement (roughly 5–10 FPS where feasible), preserve visual fidelity, and use the final packaged full-workload run as the baseline. This supersedes merely finishing packaging; it does not resume SP or audio. Investigate major rendering/submission and animation costs with existing diagnostic counters before choosing a candidate. Retain only improvements established by matched four-player/four-bot moving tests; previous rejected caches are not evidence of an available gain.


Fresh packaged baseline completed: beta_final_packaged_4p_resumed, hm_borg1, 600 host seconds. Eight playing/connected clients, four actual bots and all four movement lanes verified; final main loop live, engine error empty, ISO restored. Average 29.0711 guest elapsed FPS; last twelve windows 21.1089; minimum 15.4977, eight windows below20. Evidence and matching symbols: notes/evidence/holomatch_20260910/final_push_baseline. This verifies the packaged MP executable on this workload, not retail or isolated host speed. Started executable-only FrameDiagnostics build for the explicitly requested final optimization push; existing beta ISO remains intact.

Final-push baseline also exposes native submission pressure: latest STEFX_HW_SCRATCH_RING sample10496 has4339193draws,666826fallbacks, peak262144dwords (exact1MiB capacity),2247fencewaits totaling44872guestms. Both retained hm_borg1/hm_voy1 endpoints have about4.9MBfree physical memory. This supports inspecting scratch capacity/fence latency as a larger candidate, not accepting a larger allocation or claiming GPU throughput will increase. Keep original two1MiB buffers as the control; validate memory pressure and actual frame progress for any alternative.

Prepared a bounded scratch-layout candidate while the frozen original diagnostic runs. MP startup cvars r_efScratchBuffers (2 or3) and r_efScratchKB (1024 or1536;1536 only with2buffers) allow 2x1MiB control,3x1MiB latency trial, or2x1.5MiB capacity trial. Maximum3MiBtotal, exactly1MiB above original. Defaults remain2x1MiB; SP does not consult new settings. Partial allocation failure releases all new blocks before retrying original layout. Native actual-helper tests pass: capacity/overflow rejection,2/3-slot fence reuse, disabled ring, partial failures at each allocation, fallback, invalid settings. No performance gain or GPU correctness established yet. Diagnostic build succeeded; final_push_mp_frame_cost is running the frozen original layout with four-player/four-bot workload for600hostseconds.

Frame diagnostic finished/restored with8playing clients,4bots,4moving lanes, live endpoint and empty engine error. Preserved under notes/evidence/holomatch_20260910/final_push_frame_cost. Native draw-cycle groups put reservation at85,918,797/95,744,732cycles (89.7%) in sample68 and155,364,661/159,349,461 (97.5%) in sample70. Do not reconcile all legacy subsystem timing fields as one perfectly aligned frame budget. Expensive shader attribution includes waiting charged to the next draw; do not remove artwork to fix that wait. First selected candidate: two1.5MiB scratch buffers (same two-fence rotation, extra1MiB capacity). Production candidate rebuild is active in final_push_scratch_candidate_build.log.

User captures show75MSPF/9FPS and145MSPF/4FPS with scoreboard open during the diagnostic. Include death/scoreboard stalls in evaluation. Do not describe guest29FPS as visible29FPS. Fixed the diagnostic wrapper logger selection: both active executables have the SP-engine boot/heartbeat ABI; selecting legacy mp logger left polling unresolved. Frozen native-monitor bundles must include matching .exe with .xbe/.map to translate section addresses. Current diagnostic lacked polled host proof; final guest-memory/frame-profile extraction remains usable. Host-progress analyzer tests pass; next production-control/candidate comparisons must verify actual heartbeat attachment and host-time progress.


Wide scratch diagnostic did not qualify: the user observed a freeze; the 480-second native capture failed, monitor reads became empty, and every final named dump was missing. No layout, liveness, roster, or FPS proof survived. XEMU exited through the harness and the ISO transaction reports restored. Evidence: notes/evidence/holomatch_20260910/final_push_wide_scratch_failed. Do not infer whether the buffer trial or monitor caused the stall. Production defaults and the separate beta ISO remain unchanged.

Replaced this wrapper's diagnostic logger discovery with six heartbeat words every 15 seconds after a 40-second delay, translating the live guest page each time. This avoids legacy broad relocation scans and large telemetry reads. Host-progress analysis accepts compact snapshots only when their exact frame/realtime/serverTime identity matches the final live FPS ring, and excludes intervals containing any non-gameplay record. Nine analyzer tests pass, including bad magic, short reads, unconfirmed values, and intro overlap. Started final_push_control_compact with the same production executable and original 2 x 1024 KiB buffer layout, four moving players and four explicitly requested bots. No performance result claimed.


Compact-monitor control completed: 600 host seconds on hm_borg1, same production candidate executable with original 2 x 1024 KiB layout verified ([2,262144,0,1]). Four actual bots and four movement lanes verified, eight playing clients, final main loop advances, engine error empty, 4,964,352 physical bytes available. Guest elapsed average 27.9756 FPS, minimum 15.7434, six windows below 20. Confirmed compact heartbeats show 16.2561 frames per host second over 512.67 seconds (8,334 frames), with guest time advancing at 0.5885 of wall time. This is monitor-diagnostic game progress, not exact presentation timing or retail FPS. No freeze reproduced; cause of the failed wider trial remains unresolved. Evidence: notes/evidence/holomatch_20260910/final_push_control_compact. Next short candidate check verifies actual startup buffer selection before committing another full performance comparison.


A 90-second compact-monitor candidate check confirmed the requested wider buffers really were allocated: scratch layout [2,393216,0,1], with no allocation fallback. Four-player/four-bot workload and final liveness verified, empty engine error. Native capture at 70 seconds succeeded; four viewports and active combat visible. This short visual check does not qualify performance or resolve the earlier long-run freeze. Started final_push_wide_compact for the matched 600-second comparison, without screenshots, on the same executable and monitor cadence as the original-layout control.


The full wider-buffer run finished live with four moving players and four bots, empty engine error, and actual layout [2,393216,0,1]. Free physical memory was 3,911,680 bytes. Exact-run results: 28.4926 guest FPS versus 27.9756 control; diagnostic host progress 17.5403 versus 16.2561 frames/second. Shared whole gameplay intervals give 17.6830 versus 15.9388 (+10.94%, about 1.74 frames/second), with imperfect interval boundaries. This does not meet the requested 5–10 FPS improvement, so the wider layout is not adopted. Inline fallbacks dropped from 844,490 to 53,699, but fence waiting increased from 41,242 ms over 10,496 frames to 72,516 ms over 11,264. One final combined experiment now allows three 1.5 MiB buffers (4.5 MiB total, +2.5 MiB over control), testing both capacity and reuse latency. Production defaults remain two 1 MiB buffers. Native tests cover the larger bound and partial-allocation fallback to the original layout.

Fixed another harness provenance defect before accepting the comparison: prefix globbing could select an older similarly named run (wide_compact_check instead of wide_compact). The summarizer now requires exactly one matching timestamped report and only reads that invocation's files. It does not fall back when current files are missing. Three regression tests passed; both comparison summaries and host analyses were regenerated from their exact timestamps. The regenerated control was unchanged. No incorrect candidate result was accepted.

## Persistent world vertices and effect work — user selected options 1 and 2

Read-only assessment completed; the user explicitly selected implementation of options 1 and 2. The current goal seeks a measured 10–15+ FPS gain in moving four-player Holomatch with four actual bots, preserving visual fidelity. Do not substitute guest-clock FPS for host progress or claim the gain before measurement. SP/co-op/audio remain outside this work.

Implementation in progress: an approximately 1 MiB immutable interleaved vertex allocation keyed by world surface and material stage, with dynamic visible-index remapping. This preserves existing material batches across changing visible subsets. Faces and triangle surfaces with static attributes are eligible; curves, deformations, dynamic texture coordinates, vertex-light styles, fog and dynamic-light batches retain the dynamic path. Allocation exhaustion falls back without eviction, and map reset waits for GPU completion before freeing storage. Runtime controls r_efWorldVertices, r_efWorldVerticesVerify, r_efFxCull and r_efFxBatch currently default off pending qualification. Separate exported counters and bounded logs establish actual use and verification failures.

Effect candidate adds conservative world bounds for EF lines, tapered lines, Bezier curves (including the inherited 33/32 endpoint), EF lightning/electricity/cylinders and sprite variants. Stateful legacy bolts and immediate-mode legacy beams remain unchanged. Safe additive one-pass vertex-colored generated effects may batch only when both adjacent entities meet the contract. Expanded batching exposed missing oriented-line overflow checking and a per-vertex sprite color offset captured before a possible flush; both are corrected.

Native helper tests pass: immutable surface reuse under changed visibility order; mixed/dynamic/fog fallback; capacity and index bounds; allocation failure; mismatched-vertex rejection; fenced reset; crossing beams and edge grazing; all 33 Bezier steps; bounded worst-case electricity walks. Full build and emulator qualification are still pending. Existing beta package is not refreshed or published.

World/FX first runtime checks: world_fx_verify (XBE 41f9c03db645adc50ca2b396ce044d2bf8c95a08a14bca96d2574c87fa456a68) finished live with four moving players/four bots and clear engine error, but persistent draw count and merge count were both zero. Culling was active. Two native captures were inspected; this was not combined optimization proof. world_fx_gate_verify then finished live with 239809 merge-eligibility events and 599610/924106 culls; persistent draws still zero. Guest elapsed average 39.61 in a short visual run is not a matched performance gain. Exact new reject counters show 424989 draws blocked by the deformation/fog/dlight category and 26604 with unsupported surface coverage; do not call zero mismatches a passing vertex comparison when no persistent draws occurred.

Current correction explicitly brackets the ordinary base-material draw, allowing immutable base attributes to coexist with later dynamic-light passes. Fog-modulated vertex colors still fall back; GPU fog state remains independent. Unit tests exercise both base and later-lighting phases. Private immutable-storage implementation now lives between STEFX_WORLD_VERTICES_BEGIN/END markers in win_qgl_dx8.cpp (not a shared header), so further implementation-only changes do not invalidate all translation units. Scratch lifetime tests still pass, including unchanged SP allocation. Build world_fx_basepass_build.log is active; qualification of this corrected version remains pending. All feature defaults remain off; no beta package change.


The corrected static base-pass check completed: world_fx_basepass_verify_20260910_202433, XBE 9696532b4a8f6c8e3015324175221c3c98daa414374bac963ce916942708599a. Persistent draws329534, stored surface-stage entries3082, vertices15194, reused surfaces3059102, capacity fallbacks0, byte mismatches0, bad indices0. All four movement lanes and four actual bots verified; endpoint live and engine error empty. Physical memory available3919872bytes. Native screenshot xemu-2026-09-10-20-26-22.png was inspected with intact four-view geometry and effects. The short run used expensive byte verification and is not performance acceptance. Matched600-second world_fx_control uses this same frozen executable with all three features off; candidate will enable all three with byte verification off. Original2x1MiB scratch layout remains fixed.


Matched disabled-feature control world_fx_control_20260910_202812 completed600hostseconds with8playing clients,4actualbots,4movinglanes, live endpoint, empty engine error, and restored ISO. All new world/cull counters zero; legacy merge-eligibility events399747. Guest elapsed average25.8372FPS/minimum13.1555; confirmed diagnostic host progress15.1069frames/second over527.97seconds (7976frames), with slowest retained15-second interval5.19frames/second. Physicalfree4960256bytes. Evidence saved in notes/evidence/holomatch_20260910/world_fx_control. Enabled-feature candidate is running on the identical XBE; no comparison accepted yet.


Combined candidate world_fx_candidate_20260910_203949 finished live/full workload but regressed: guest23.6964FPS; host14.0639 overall; comparable whole intervals13.7512 versus15.3472 control (-10.40%). Worst guest window5.3579FPS. Static storage active1321265draws,12091434reuses,15858vertices,no capacity fallback, no bad indices; byte verification was off. FX2593367tested/1708372culled/580040merge eligibility events. Do not adopt or close performance work. Evidence in notes/evidence/holomatch_20260910/world_fx_candidate.

Effects-only isolation world_fx_effects_only_20260910_205138 failed early. Heartbeat frame5052/realtime156315/serverTime150376 was unchanged at host251.28,266.42,281.47seconds, then XEMU exited rc0 at283seconds of planned600. No final guest dump or liveness/workload qualification survived; this is not the scheduled final diagnostic pause. Do not infer an exception cause from rc0 or dsound tail warnings. Evidence saved under notes/evidence/holomatch_20260910/world_fx_effects_only.

User now prioritizes isolating the specific cause of FPS dips and the freeze over further optimization candidates. Stop new optimization work. Draft PVS bounds rejection and animated FX batching mode2 were prepared and native-tested, but have NOT been built or run; they remain off and are not part of the frozen executable used in dip_flight_fx. That run uses the exact failed effects-only settings and XBE9696532b..., with read-only flight recording and0.5second guest EIP sampling. This is diagnosis, not FPS acceptance. Recorder obtains the native RAM host address from XEMU, walks current guest page tables without RAM scans, validates a known heartbeat mapping, samples existing phase counters, and preserves registers/stack/logs after2seconds without main-loop advance. Three address-translation tests pass.


The dip flight recorder captured a 3.689-second frame stall sampled inside DirectSound SetVolume during sound playback after rendering. Recurrent slowdown separately correlates with BeginPush reservation and rising scratch-buffer fence waits. Do not collapse these into one asserted root cause or call the freeze fixed. Detailed provenance, interpretation limits, and cohort measurements: notes/mp_dip_diagnosis_2026-09-10.md. The frozen candidate remains unaccepted; no new optimization draft was built.


Completed-frame diagnostic f80df371... ran600hostseconds with full workload/liveness and restored ISO. Sample550:106guestms frame,90ms backend rendering,2ms audio; one BeginPush accounts for most draw cycles. Read-only queue counters show GPU completion advancing while the same mainloop remains in BeginPush. This isolates a graphics-back-pressure dip separately from audio stalls. Six snapshot/translation tests pass. No gain accepted. The next targeted candidate is compact interleaved dynamic vertex staging alongside persistent world storage, motivated by current planar uploads and the pinned XEMU renderer's per-attribute dirty-buffer handling; it remains an untested hypothesis. See notes/mp_dip_diagnosis_2026-09-10.md for clock-unit caveats and exact evidence.
