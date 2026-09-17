# Co-op optimization — September 11, 2026

The user requested a switch at the MP animation-cache checkpoint to co-op,
using the same FPS-dip diagnosis and targeting sustained 30 FPS averages.
This supersedes the earlier optimization pause for co-op. Audio stays paused.
Preserve visual fidelity and all ICARUS-authored behavior.

## Measurement and acceptance

Use the original pinned XEMU, native presented-frame deltas over host time,
matched representative gameplay, and actual two-player movement. Verify both
rendered views, player state, movement and continued simulation at runtime.
Track rolling ten-second lows, frame-cost categories, memory/I/O and host CPU
contention to isolate offenders. Keep real gameplay stalls in the results.
Exclude any interval overlapping borg1's text intro, loading, menus or blocking
fullscreen movies using runtime state, not a fixed delay. Thirty FPS is the
sustained average target; lows must be reported separately. Retail Xbox remains
a separate verification requirement.

## Starting state

The MP pose-cache candidate failed its exact-output verification and remains
off by default. Do not port or enable that unqualified experiment in co-op.
The MP-only frame-pair lifetime fix is independently source-tested but did not
explain the pose mismatch. Review applicability separately before any port.
MP resident-world gains do not automatically establish co-op gains.

The existing probe supports co-op loading and verifies two rendered views.
Its input-replay CLI currently rejects co-op; the replay itself is P1-only.
Existing process-local P2 input is available in cl_input.cpp, but the fake-pad
variant also triggers objectives/zoom and is unsuitable for clean timing.
First prepare and verify a bounded two-player movement fixture, respecting
cinematic ownership, before claiming a current performance baseline. No host
OS input or desktop automation is permitted. Do not treat stationary dual
views as a qualified workload.

## Goal record

The requested work focus is changed in repository memory. The app still holds
the unfinished MP goal: available tools cannot delete/cancel it or replace its
objective, and create_goal rejects an unfinished goal. Do not falsely mark it
achieved or blocked to clear it. Once that goal is removed, create a co-op goal
matching the scope and acceptance above. No token budget was requested.

The MP checkpoint completed: workload and endpoint liveness passed, exact-output
verification failed, and the working ISO was restored. No emulator is being
left running by that probe. P2 user-command consumption is in
SP-Mod-Source-Code-master/game/g_main.cpp (not the JA-derived code/game tree).
Check its presentation/control ownership before enabling the diagnostic P2
movement lane. No co-op runtime baseline has been claimed yet.


## Two-player moving baseline

Added a camera/player-lock guard to the explicitly enabled P2 test-input path;
it emits neutral controls during authored presentation and resumes from current
angles. Real input and script state are untouched. Actual-function compiled
tests passed for disabled mode, camera lock, player lock and angle resumption.
The SP input-only replay is now allowed by diagnostic co-op probes. It remains
P1-only; P2 uses the separate process-local test lane. Production defaults do
not enable either fixture. Both fresh SP and MP production builds passed.

Added read-only SP/co-op player sampling using the compiled Xbox5558 gentity /
gclient layout. Tests reject missing data, invalid slots, identity changes,
nonfinite positions and changed client pointers. The first run exposed an
existing MAP resolver bug: word-boundary matching could not resolve C++ names
starting with '?'. Exact whitespace-delimited matching now handles those names
and C symbols; tests passed. A read-only sidecar collected the current run's
player data without restarting or injecting desktop input.

Production baseline XBE SHA256:
fc1e572136e313946fee0a7ce08d515595d0562098a85863927fb1e857d74455.
Frozen files: build/research/letterbox_perf/coop_movement_baseline/default.*.
Run coop_moving_baseline_20260911_080603, original pinned XEMU, 600 host seconds.
Both players moved through the natural borg1 opening: 340 retained state
observations each, 336 distinct positions each, minimum health 95/100. All
three native captures were inspected. The endpoint verified two rendered
views, CA_ACTIVE, continued main-loop progress and an empty error buffer. The
ISO was restored and both the probe and sidecar exited.

After conservative runtime intro exclusion, native gameplay averaged 14.0928
FPS. Lowest rolling nominal ten-second window: 1.5378 FPS, 14 native frames
across 9.104 observed host seconds (530.775–539.879). This does not meet 30 FPS.
The sampled circular movement fixture covers the opening area, not a complete
co-op campaign or sustained representative combat; no broad qualification is
claimed. Audio has not been assessed or changed.

Two measured slow-window patterns need to be separated:
- At 478.66–488.40, median draws rose to 775 and vertex uploads to 561 versus
  364/190 in an earlier faster window. Buffer fallback counts also increased.
  Host CPU utilization was about 88%; no XEMU file reads occurred.
- The final worst window had median 338.5 draws/185.5 vertex uploads, zero
  scratch fallbacks, 284 accumulated guest milliseconds of scratch-fence waits,
  no file reads, roughly 95% host CPU use and only 1.40 XEMU CPU-core equivalents.
  This supports host contention contributing heavily; draw count alone cannot
  explain that dip. Do not discard it or claim a disk stall.

Physical free memory was only 4 KB at the endpoint, but the zone still held
about 1.9 MB free during gameplay. The engine reserves its large pools early;
physical-free alone is not proof of allocation failure. Any added contiguous
vertex cache requires a deliberate memory plan; do not blindly copy MP's 2 MB
allocation into the SP/co-op executable.

Evidence: notes/evidence/coop_20260911/ (SHA256 manifest; flight losslessly gzipped).

## Next concrete diagnostic

The draw-kind counters previously compiled only for MP now also compile for
SP/co-op FrameDiagnostics builds. Base-material markers are present only in
MP or diagnostic builds; ordinary SP renderer behavior is unchanged. They
separate HUD, world base/extra, model and FX submission work using the existing
completed-frame publication. No optimization/default change has been accepted.
The fresh SP+MP FrameDiagnostics build passed. Run
coop_draw_diagnostic_20260911_082725 completed 600 host seconds on original
XEMU with frozen XBE 19309a5f986583a22d911b8eabe2aac46c12fa1c711fbb066ff60352ea09e0e4.
The endpoint remained live with no engine error; ISO restoration passed.
Both players had 373 retained state observations and 367 distinct positions;
minimum health was 92/98. All three native captures were inspected. The P2
HUD repeats P1 stats; sampled P2 game state differs, so HUD is not health proof.

Complete streamed FPS publications were validated against the paused endpoint.
The diagnostic heartbeat updates faster than FPS publications, so gameplay
classification uses observed complete publication boundaries plus a following
publication guard. It excludes intro/transition overlap and retains stalls.
Native counter deltas inside those intervals averaged 9.8007 FPS. The slowest
nominal ten-second window was 1.7280 FPS (17 frames / 9.83784 host seconds,
t=203.378–213.216). This is a diagnostic run, not an optimized candidate or
production comparison. The user also observed 139 MSPF and 10 FPS; the screenshot
has no exact recorder timestamp, so it is not assigned to an exact frame.

Among 640 coherent gameplay draw samples, the slowest guest-duration quartile
had 478.5 model draws/frame and 70.3% of producer-side draw cycles in the model
category, versus 156.6 and 54.0% in the fastest quartile. World-base draws were
115.6 versus 120.5, so a world-only change is not yet the supported next fix.
These cycles include GPU backpressure and are not per-material GPU timing.

Host CPU contention is also measured: during the worst window, three CPU-context
samples averaged 7.34 core equivalents for dolrecomp.exe versus 1.11 for XEMU.
The user's other processes were not changed. This prevents attributing the
full slowdown to game code. Earlier production-baseline degradation coincided
with that process starting; that earlier timing is correlation, not retrospective
CPU sampling.

Evidence: notes/evidence/coop_20260911/draw_diagnostic/ (SHA256 manifest).
Next: per-model/material completed-frame counters and a bounded overflow bucket,
compiled only for FrameDiagnostics. Reader rejects torn/short/schema-invalid
records and preserves overflow; 13 flight-recorder tests and actual VC71 helper
aggregation/overflow tests passed. Fresh SP+MP diagnostic build in progress:
coop_model_diagnostics_build.log. No optimization default has been changed.
The explicit co-op direction still supersedes the stale app MP goal.


## Model diagnostic and updated goal

The user edited the active app goal: every complete ten-second confirmed co-op
window must exceed 30 native XEMU FPS, sustained full workload, current visuals.
The app goal now matches co-op. It remains active and unmet. The earlier goal
replacement limitation is resolved by the user's edit; do not repeat it.
Qualified MP resident triangles are historical renderer evidence, not a matched
co-op baseline. Current co-op SP binary has no resident-world allocation.

Fresh SP+MP FrameDiagnostics builds passed. Frozen diagnostic SP XBE:
59622e7ce0b1d44f42f88566e4edcdb9c0f51b07952f675becd661d05cd5a602.
Run coop_model_diagnostic_20260911_084811 lasted 420 host seconds, same P1/P2
moving borg1 fixture. Both native captures were inspected. No optimization
was enabled. ISO restoration passed; engine error buffer was empty.

STABILITY FAILED: main-loop counter stopped at 11131 near host 403 seconds;
no native frame progress occurred from 405.891 to 419.891 (14 seconds).
Both final liveness dumps contain 11132, so the mandatory endpoint advance
check failed. Their writes were delayed; the independent flight data also
records the stall, so it cannot be dismissed as only a dump problem.
The preceding complete-publication intervals averaged 18.864 native FPS, but
exclude the terminal stall because no following publication exists. This
partial average must never qualify the run. Preserve/report the terminal
zero-frame interval separately. Do not silently drop it as non-gameplay.

A new dolrecomp.exe process consumed an average 11.07 core equivalents in nine
late CPU-context samples (host t390–420); XEMU received 0.95. Heavy contention
correlates with collapse. No unrelated user processes were changed. Source
and rendering inefficiency still need independent diagnosis under matched load.

Model/material counts reconcile exactly with coherent draw-kind records
(calls, vertices, indices and BeginPush cycles). 560 gameplay pairs survived.
However, the 63-key table overflow bucket contains 78.9% of slow-quartile model
draw cycles (370.6 calls/frame). Therefore the named top materials alone cannot
identify the worst offender. Expand capacity or add model-class totals before
claiming exact material attribution. No counts were dropped silently.

Visibility source findings:
- CG's P2 supplemental scene accepts eligible entities within 4096 units without
  checking PVS. BUT a read-only live sidecar observed only 0–1 additions across
  31 late samples. It is not the main source of these late hundreds of draws.
- CG_AddCEntity disables the existing hidden-character PVS optimization for
  split screen because the scene serves both views.
- SV_STEFX_Borg1RawBspSnapshotBypass unconditionally bypasses ordinary visibility
  on borg1 non-portal snapshots, with budgets of 96 actors and 128 nearby models
  (plus items, missiles, events). This feeds otherwise hidden objects into the
  primary snapshot. Do not simply remove it: verify why it was needed, preserve
  P2 and portal visibility, and avoid regressing authored cinematic behavior.
- A conservative per-view renderer visibility check using whole model bounds
  could eliminate hidden work while retaining shared scene/animation/script
  updates. Point-origin PVS checks alone can clip large models or attachments.
  Exempt uncertain/deformed/through-wall/shadow cases; unknown bounds fail open.

Stall diagnostics: final EIP 0x26750 resolves to VectorLengthSquared; stall2 EIP
0x2c71c resolves to CM_TraceThroughTree+0x21c. Captures are asynchronous; stack
words are not a validated call chain. Last shader gfx/misc/phaserbolt is a stale
renderer marker, not proof the shader caused the stall. No crash cause claimed.

Evidence: notes/evidence/coop_20260911/model_diagnostic/ with SHA256 manifest.
Both probe and CPU/scene sidecars exited. No XEMU test remains running from this
pass. Build artifacts are diagnostic and are not a release-qualified update.

For the stricter ten-second goal, analyze_native_lows.py now reports a separate
complete_window_fps_lower_bound: observed frames divided by the full ten seconds,
not by the shorter polling span. This conservatively omits boundary frames and
cannot inflate a marginal pass. Four native-window tests and streamed gameplay
exclusion tests pass. Existing observed-span metrics remain labeled separately.

Next useful work: resolve visibility inflation in a bounded opt-in candidate,
validate actual model bounds/PVS and exact visible output, then compare matched
moving co-op runs. Keep host CPU context; never claim game-code gain from another
job ending. Audio stays paused. No optimization has been accepted in this pass.


## Per-view model PVS candidate (not yet runtime-qualified)

Added an opt-in renderer experiment r_efCoopModelPvs: default 0, mode 2 observes,
mode 1 skips eligible hidden MD3/MDR surfaces. It is compiled only for SP/co-op,
not the MP renderer. The shared client scene and all animation/ICARUS updates
remain unchanged. Bounds union both animation frames and every loaded MD3 LOD,
then transform through the actual entity axes/origin. Padding is 0.125 local
units plus one world unit; MDR also unions the existing 128-unit world fallback
sphere. Frame-cache bounds are copied before requesting the next packed frame.

Each view queries its own marked BSP leaves with bounded traversal, retaining
partition touches and failing open on invalid bounds, stale/unavailable PVS,
portal/menu views, unsupported entity flags, projected shadows, deformed/remapped
shaders, through-wall shaders, and surface sprites. Only SF_MD3/SF_MDR are filtered.
Cumulative g_SPXBCoopModelPvs counters expose actual mode, tested/outside models,
eligible/skipped surfaces, and unknown bounds; recorder adds asynchronous records.

Actual helper tests compiled with VC71 pass transformed/scaled bounds, grazing,
invalid frames/trees, rotating frame cache, observe-only mode and shader exemptions.
13 recorder tests passed. Disk audit covers 716 MD3 files / 2875 frame records;
all rendered vertices lie within padding (maximum excess about 0.0147 units).
Forty empty tag/hand models have no surfaces; k_chair1 contains an empty trailing
exporter surface and trailing bytes, retained explicitly by the auditor. Current
SHA256 values for all 201 audited MDRs still match, maximum excess 0.02031 units.
These checks establish conservative geometric bounds, not rendered-image or
performance qualification.

Fresh SP+MP FrameDiagnostics build passed; frozen candidate SHA256:
`dfdb97abd62013fb3405466c3da3a40631e514c01556c8ebe5e735b54abdddbf`.
Production default remains disabled (0).

Observe-only mode 2 completed 420 seconds, live endpoint, no engine error,
ISO restored. Confirmed gameplay averaged 19.35 native XEMU FPS. The slowest
observed span was 13.20 FPS over 9.47 seconds; its conservative full-ten-second
lower bound is 12.5 FPS. Both players moved and remained alive in sampled state.
Within confirmed gameplay, 429,171 of 819,646 tested model instances were outside
PVS; 850,012 surfaces were eligible, zero skipped as required in observation mode.
Slow sampled-frame quartile: 533.5 model draws/frame versus 133.3 in the fast
quartile; world draws stayed near 112/frame. Model producer cycles include
backpressure, not isolated GPU material cost.

Inspected both native captures individually (180/300 seconds): both split views
render world/characters/weapons; second capture has depleted P1 weapon and P2
against a pillar. These are limited visual evidence and reveal workload limits,
not full campaign/full combat qualification. Evidence and SHA manifest:
`notes/evidence/coop_20260911/model_pvs_observe/`.

Enabled mode 1 run is active (session61986), CPU context session45130. Same frozen
binary and 420-second fixture already launched before the user's shorter-test
request. It will end through normal endpoint checks/restoration. Subsequent
same-route tests use 270 host seconds, about two minutes after intro; prepared
`coop_model_pvs_default_short.invocation.json` for mode 0. Do not launch until
current ISO transaction is restored. Longer runs are reserved for changed maps,
promising candidates, and final stability. The 30 FPS goal remains unmet.


## Enabled visibility result and frame-cache correctness follow-up

Mode 1 completed 420 seconds: endpoint live, error clear, ISO restored, both
players moving/living in 239 gameplay observations. Native gameplay average
22.78 FPS; worst observed 9.68-second span 16.63 FPS, conservative full-ten-second
lower bound 16.1 FPS. Mode 2 observation was 19.35 / 13.20 respectively.
Mode 1 skipped all 1,021,504 eligible hidden surfaces. Slow sampled-frame quartile
model draws dropped from 533.5 to 335.9/frame; world draws 113.2 versus 120.1.
These are differing trajectories and host workloads, not isolated causal FPS gain.
A 270-second mode 0 control using the same frozen binary is now running
(probe session94224; read-only CPU context session91620).
Both enabled captures were inspected individually; camera poses differ from the
control and cannot establish pixel identity. Production default remains 0.
Evidence: `notes/evidence/coop_20260911/model_pvs_enabled/`.

Read-only source follow-up found the existing packed MDR frame-pair protection
was compiled only for MP. SP/co-op callers R_ACullModel and RB_SurfaceAnim keep
one frame pointer while fetching another, allowing a cache hit on the FIFO
replacement slot to be overwritten by the next miss. The actual lookup/eviction
regression test, compiled as SP, reproduced that overwrite (exit 1). Extended
all four declaration/reset/hit/replacement guards to STEFX_ELITE_FORCE_SP; SP
and MP each then passed 10,000 pairs, two models and replacement wraparound.
This is a verified source-level correctness repair, not measured FPS gain.
The game binary has NOT yet been rebuilt with it; current control uses the old
frozen PVS candidate so the comparison does not mix this additional change.
Build fresh SP+MP after the current control ends, never during FPS measurement.


## Short default control and dynamic triangle candidate

Default mode 0 completed 270 seconds, with about 119 seconds of confirmed
moving two-player gameplay, live endpoint, empty engine error and restored ISO.
Native average 22.81 FPS; minimum observed span 18.98 FPS and full-window lower
bound 18.9 FPS. Comparing whole intervals within shared server time 101444–199789
against enabled PVS yields default 22.93 versus enabled 24.09 native FPS (+1.16).
Different routes/host load still confound causality; the larger observation-mode
comparison overstates the evidence for production gain. PVS stays default 0.
The single native capture was inspected: both views render, P2 faces a pillar,
and P1 has little weapon energy. This remains limited opening-route coverage.
Evidence: `notes/evidence/coop_20260911/model_pvs_default_short/`.

Added r_efCoopModelArrays (default 0, 1 enabled, 2 verifies actual copied bytes).
Only co-op model GL_TRIANGLES draws with valid indices and at most 1000 expanded
vertices qualify. Other draws use the original path. It duplicates the exact
current tessellation vertices into triangle order using the existing fenced
scratch or in-push storage; no new large allocation. Existing shaders, matrices,
material state, texture data and triangle ordering remain unchanged. Native
DRAW_ARRAYS ranges use at most 255 vertices, ending on triangle boundaries.
The same native mechanism bypasses the pinned emulator's element-cache lookup
in the qualified MP resident-triangle change, but this candidate is dynamic,
not resident; it can increase vertex packing/upload work. Performance is unproven.

Actual helper tests pass all eight attribute layouts, constant/per-vertex color,
reordered/repeated indices, packet boundaries, payload guards, every copied DWORD
corruption detection and unsupported-state rejection. These are source-level
checks, not native rendering acceptance. Recorder exposes mode/draws/vertices/
rejections/verified/mismatches/payload/range counts; endpoint dumps added too.
SP+MP FrameDiagnostics build session96253 is active, log coop_model_arrays_build.log.
Includes the separately tested SP/co-op frame-pair repair. Do not run XEMU during
the build. Freeze the successful pair and run a short verify-mode diagnostic
with PVS disabled before any timing comparison. All optimization defaults stay 0.


Extended the model-array test to compile the actual stream-pointer/stride emission
block from dllDrawElements. For every attribute layout it decodes those emitted
registers and walks all referenced vertex components, comparing them with the
original indexed source. This passes at the real 1000-vertex payload cap, as do
packet range/triangle-order tests. Saved actual draw-function snapshot and tests
in model_arrays_candidate evidence with SHA manifest. Prepared 270-second
verify/default/enabled invocations pointing at the not-yet-frozen array candidate;
none has launched. Build96253 remains the only active build; no XEMU test active.


The array runtime-proof analyzer rejects missing execution, wrong modes, any
recorded mismatch, incomplete mode-2 vertex verification, execution while
mode 0 is selected, and counter resets. Synthetic acceptance/rejection cases
passed. Final probe now requests both array and PVS counter dumps as well as
existing MDR frame-pair protection. Build96253 has progressed into the MP
companion with no compiler errors observed; SP compilation finished. No array
runtime verification or FPS gain exists yet. The next pending invocation is
coop_model_arrays_verify_short, mode 2 / PVS 0, 270 seconds, after freezing and
verifying the completed SP+MP build. Do not use current release files mid-build.


Build96253 completed exit 0, SP+MP freshness passed. Frozen full pair in
`build/research/letterbox_perf/coop_model_arrays_candidate/`; default XBE SHA256
960ac8c7d0bc1c16b793c0415f9294e3a045373e47360f800b928a7e91819920,
SP frame-diagnostics runtime 09:56:43; MP runtime 10:00:21. Identity and build log
archived with candidate evidence. Short array mode-2 verification launched;
PVS remains 0. It is correctness/diagnostic data, not FPS acceptance because
mode 2 reads back every copied vertex. No default activation or beta staging.


Short verification probe session27484 and read-only CPU context54342 are active.
At host205.71 seconds mode 2 had submitted 267,517 array draws / 46,291,782
expanded and verified vertices, with zero mismatches and 1,947 oversized/invalid
candidate rejections falling back to original draws. Native profiler confirms
DRAW_ARRAYS activity. The single native180-second capture was inspected:
both split views and model/effect rendering are present, but camera positions
differ from controls and P2 is near a pillar. No pixel-identity, route-coverage
or FPS acceptance claim. Await final liveness/restoration and gameplay-only
counter checks before starting the enabled timing test.


Array mode-2 verification completed 270 seconds: live endpoint, no engine error,
ISO restored, both players moving/living in 96 confirmed-gameplay observations.
Final counters: mode2, 523,748 array draws, 88,776,693 expanded/verified vertices,
zero mismatches, 4,037 rejected candidate draws (original path), 709,922 range
commands. Gameplay-trimmed counters separately verified 72,081,114 vertices
with zero mismatches. Native array draws observed. PVS final mode0/all counters0.
MDR frame-pair protection exercised 1,670 times, confirming SP/co-op uses repair.
The diagnostic's 11.58 native FPS average is NOT optimization acceptance:
mode2 reads back every copied vertex. Evidence archived at model_arrays_verify.
Short mode1 speed run now launched with the same frozen binary, PVS0, 270 seconds;
await liveness/restoration and compare against mode0 on this same repaired build.


Mode1 speed run completed 270 seconds, live/error-clear/restored, both players
moving/living in 102 confirmed-gameplay observations. Native gameplay averaged
12.25 FPS; worst observed 9.92-second window 9.37 FPS, conservative full-window
lower bound 9.3. Mode1 confirmed, verified_vertices stayed 0, no mismatch.
This fails the target and is much slower than the older default control; do not
activate this experiment. Same-build mode0 control has now launched to isolate
expansion from the independently repaired frame cache before removing/replacing
failed work. The one180-second native capture was inspected: both views render
weapons/effects/world; different poses do not prove pixel identity. Evidence
archived in model_arrays_enabled with SHA manifest. Defaults remain 0.

Live control handles: probe87694, CPU context16098 (revalidate before use). Both use frozen960ac8c7... with model arrays0/PVS0; no build is running. Next: finish/analyze this control, compare shared gameplay against enabled mode1, and reject/remove the expansion path if its regression is confirmed. Keep the runtime-exercised frame-pair correctness repair. The qualified MP result used immutable resident world vertices; this failed model experiment instead expands and recopies dynamic vertices. They are different mechanisms, and model producer timing includes queue backpressure from earlier world draws. Investigate remaining costs from measured evidence; do not infer an API bottleneck solely from producer-cycle attribution. Current 30 FPS goal remains unmet.

User-requested pause, Sep 11: pause until GPU usage quiets down. No new tests
or optimization work while paused. Probe87694, CPU recorder16098 and GPU
recorder90289 have all exited 0; control ISO transaction confirms restored:true.
No xemu.exe process was present at the pause checkpoint. Control analysis remains
pending; the performance goal remains unmet.

Host GPU contention was measured during the latter part of the mode0 control:
Vigilante82PC.exe PID33444 used about 84-86% of the same GPU 3D engine as XEMU.
Twenty inspected samples averaged 86.34% (83.84-88.52%); XEMU averaged 2.44%.
Evidence: build/research/letterbox_perf/coop_model_arrays_default_short.gpu_context.jsonl.
This is a current confound, not proof that previous dips had the same cause.
Earlier runs lack matching GPU history. Do not attribute the entire mode1
slowdown to game changes or accept gains from unmatched host GPU loads.
The unrelated application was not changed or stopped. Both experiments remain
at default 0. Recheck host GPU conditions before fresh comparisons on resumption.

Sep 11 resumption checkpoint: user reported GPU freed up. Initial heavy V8 load
was absent; resumed a 270-second same-build mode0 control with GPU recording.
StarWarsDemolitionPC appeared during startup (observed up to 53.33% on the same
GPU engine), then exited. Initial GPU recorder91666 stopped on an invalid
Windows counter sample. Recorder now continues after invalid instances and
records invalid/sample instance counts; recovery92187 completed 60 samples.
The coverage gap remains real and is not filled or treated as zero contention.

Control coop_model_arrays_default_gpuquiet_20260911_122656 completed live,
error-clear, ISO restored; arrays runtime0 with no candidate draws. Confirmed
native gameplay average20.6804 FPS; slowest9.966-second span16.2550 FPS,
conservative complete-ten-second lower bound16.2. Both players moved; minimum
sampled health P1=90/P2=1. Native180 screenshot individually inspected: both
views, world, actors, weapon effects present. Limited repeated opening route
still does not prove sustained full-workload or representative-map acceptance.
Earlier heavily contended same-build default averaged12.0859, worst9.698-second
span7.7335/lowerbound7.5. This difference is not a game-code gain.

Next candidate coop_model_arrays_enabled_gpuquiet was deliberately stopped
before qualification when another application, SpiderMan.exe PID33616, reached
50.83% GPU use during startup. Only our verified XEMU PID19644 was terminated;
probe31915 completed its cleanup and ISO restored:true. Its final liveness and
mode verification are false because of the intentional abort, not evidence of
a new spontaneous game crash. GPU recorder30888 was then stopped; unrelated
applications were untouched. No new emulator or build remains running from
this work. Both experiments remain default0, frame-pair correctness fix retained.

Detailed raw data, qualification limitations and SHA manifests archived in
notes/evidence/coop_20260911/model_arrays_default_contended,
model_arrays_default_gpuquiet, and model_arrays_enabled_gpuquiet.
Performance comparisons await sufficiently stable host GPU conditions; do not
repeatedly launch tests into the observed external game workload. A full GPU
trace is required on the next fresh control/candidate pair. Goal remains unmet.

Next goal continuation: previous turn classified progress (fresh control,
intentional candidate abort, evidence and monitoring repair), not a verified
wait/blocker-only turn. No unrelated processes changed. New initial GPU check
was quiet, so a fresh control was run: coop_model_arrays_default_quiet_pair1,
20260911_123716. It finished live/error-clear/restored with two moving/living
players, avg22.8479 native FPS, lowest9.677-second span14.9834, conservative
complete-ten-second lowerbound14.5. Native180 screenshot inspected individually;
P2 is looking into nearby geometry, so opening-route coverage remains limited.
Archived model_arrays_default_quiet_pair1 with qualification and SHA manifest.

Important correction: long-lived Get-Counter wildcard queries can retain their
initial process instance set, so the old sidecar missed newly launched XEMU
and external games. Fresh independent query identified our XEMU33668 at13.8-
15.6% and SpiderMan2 PID7132 at9.7-26.7%. Thus initial-empty old GPU rows are NOT
proof of an uncontended run. Do not launch enabled_quiet_pair1 from this control
as a qualified A/B baseline. Recorder now reopens the wildcard each iteration,
uses the second warmed sample, records invalid/error/unavailable counts and
refresh_instances:true. A bounded20-second live check succeeded and saw XEMU;
evidence gpu_dynamic_query_verify.jsonl. Future flight streams include a
host_clock epoch_start from the harness's actual time.time origin for precise
GPU/gameplay interval alignment. Existing recordings have no fabricated anchors.

Root-cause profiling improvement: prior mode0 slow-quarter model attribution
hid75.1% of model producer cycles and423.1calls/frame in the63-key overflow bucket.
Model draws rose131.1fast to581.4slow, while dynamic world106.6to113.1. Producer
cycles include prior GPU queue backpressure, not isolated model GPU execution.
Replaced diagnostic-only linear63-key table with256 hashed keys plus explicit
summed overflow, bounded256 probes with no eviction. Snapshot schema2 stores
257 rows; reader remains compatible with schema1 and rejects torn/short schema2
snapshots. Additional diagnostic storage12,352bytes across current+snapshot.
Actual VC71 helper passed adversarial collision/wraparound/full/overflow tests
and guard words;14 recorder tests passed. No game behavior or visual settings
changed. Existing model-array/PVS experiments still default0.

Build25455 is running SP+MP FrameDiagnostics with reuse objects, no staging,
log coop_model_profile_v2_build.log. Revalidate before use; no emulator tests
while building. Next: freeze/hash completed binaries, verify expanded profiler
in a short diagnostic co-op run, identify previously hidden offenders. This
profile build is NOT a performance-qualified release or a claimed FPS gain.

Profile-v2 build25455 completed0, fresh SP+MP identities verified. Frozen full
pair at build/research/letterbox_perf/coop_model_profile_v2_candidate. SP SHA256
e8274ebcdfa20ecca80b7845658c4fc584a06d96cdfb9cc3596e8f70fd9c540a;
MP c0bd92f848ee1e6af1ec2b38333a7081a9d8e68f64a1084f4d7b901a10c64a8c.
Diagnostic coop_model_profile_v2_short_20260911_124651 (270seconds) completed0,
endpointlive/error-clear/ISO restored,104confirmed gameplay movement/living
observations per player. Native180 image individually inspected: two views,
actors, firing effects and world present; not pixel identity/full-route proof.
Schema2 runtime verified, max230named model/material pairs, zerooverflowcalls.
Analyzer independently reconciled model row sums with draw-kind totals.
Slow-quarter model producer costs: actors54.13%, static map props29.30%, moving
brush models8.45%, weapons7.65%. Calls/frame: actors220.42vs51.78fast, props188.49
vs68.43, brush102.09vs18.03. No single material dominates; blite6.7% largest.
These are producer-side costs including queue backpressure, not isolated GPU
costs or CPU-time percentages. Do not reduce visual fidelity from this alone.

Co-op now independently confirmed to execute the same full XEMU element-cache
reverse scan previously proven for MP. Dedicated5-second sampler49633 targeted
only verified XEMU29236, original pinned hash, four busy threads; exited0 with
1280register observations and zero preexisting suspension counts. Thread30032
landed in RVA0x25af40-0x25af55 on107of320observations. Read-only captured RSI cache
header: num_used51200,num_free0,init_callback_rva0x24fab0. Samples epoch
1789145466.15255-1789145471.32391, approximately host223.45-228.62seconds.
Files coop_model_profile_v2_short.host_pcs.jsonl/.identity.json/.lru.json.
This proves the expensive full-cache path is active in co-op; it does not prove
every dip or historical freeze has that cause. No emulator binary was changed.

The diagnostic native average22.2941/lowest observed18.6102/conservative
complete-ten-second lowerbound18.0 are NOT acceptance: sampler interrupts
threads and external GPU contention returned. Refreshed GPU queries with actual
host_clock epoch_start found18gameplay samples, external mean25.80%, max36.36%,
16samplesover10%; SpiderMan PID28692 dominated. Later new PID32844 was also
captured, demonstrating new-process discovery. Missing old query instances must
never be retrospectively interpreted as zero usage. GPU65961 finished0.
Probe70545 finished0 and no XEMU remains. No build or sampler is running.

Archived source/test/build/runtime/counter evidence with SHA manifest under
notes/evidence/coop_20260911/model_profile_v2. Next prepared bounded comparison
invocations: coop_model_profile_v2_control_comparison and
coop_model_profile_v2_arrays_comparison, same frozenv2binary, PVS0, arrays0vs1,
no register sampler. Do not run until a fresh dynamic GPU check permits useful
comparison; monitor changing external processes throughout. This existing
exact-triangle candidate has not yet had a qualified uncontended same-build
comparison. Test it before inventing new resident-memory allocations: SP/co-op
already reserves most64MiB, and stealing texture or zone space can break maps.
Keep both experiments default0 and retain independently verified frame-pair fix.
Goal remains unmet; progress this turn is direct co-op root-cause evidence and
corrected diagnostics, not an FPS fix or a release-readiness claim.

Next goal continuation: previous turn is progress (direct co-op cache-scan proof
and expanded profiling), not no-progress. Fresh GPU probes again saw external
processes at16-42%; no performance A/B was launched. Instead corrected a required
workload limitation: opt-in stefx_splitScreenTestP2Input=3 extends existing
movement/weapon-cycling fixture with primary-fire bursts. Modes0/1/2 retain prior
behavior; authored camera/player locks neutralize mode3 too. Real input defaults
unchanged. Actual helper tests cover old modes, all burst phases and lock paths.

Read-only combat evidence now includes eventSequence/two-event ring, weapon,
weaponTime/weaponState and four ammo counters for each real SP gclient, with a
second eventSequence read rejecting advancement during copy. Offsets/enum IDs
independently compiled with Xbox5558 from SP-Mod-Source-Code-master/game, not
code/game (JA's gentity is1268 instead of1056 and fire IDs28/29 instead of21/22).
New verify_coop_combat_layout.py proves actual layout1056/232/ps0,seq112,events116,
2slots,weapon148,time44,state152,ammo380x4,fire21/alt22. Reader and14flight tests
passed. New analyze_coop_combat.py rejects legacy/no-combat data, counts only new
observed firing events inside confirmed gameplay, excludes initial history and
reports missed ring events/reset identities. Three adversarial tests passed.
It intentionally does not qualify full workload from a few observed shots.

First combat build29315 passed fresh SP+MP; frozen coop_combat_fixture_candidate:
SP211d9fa472f81427ef09fea66ce89d649d1e1897a4861003edf06c7b57787555,
MP5406b882870070626c5076a0e25362f195b8dc350f8f09eb168f751e8cd20635.
coop_combat_arrays_diagnostic_20260911_130623 ran270seconds with mode3P2 and
arrays1/PVS0; live/error-clear/restored,108moving/living observations each.
Native180 capture individually inspected and shows both players firing. Mode1
performed598286draws/103052997expandedvertices within confirmed gameplay;
verified_vertices=0 (mode1 does not read back bytes; earlier mode2 proof applies
only to that verified run). Native totals observed82024index uploads over6627
captured frames vs earlier121705over7566; workloads/frame counts differ, so this
is NOT a causal FPS improvement. Index updates/draw .05477 vs .06521 descriptive.
Five-second sampler82041 targeted pinned XEMU10036 and observed zero full-scan
PCs; no cache header obtained. Absence in this bounded sample does NOT prove the
full-cache path is gone. Sampling plus GPU competition excludes FPS acceptance.
Diagnostic avg15.8294, lowest9.736-second span11.0926/lowerbound10.8; target unmet.

Combat analyzer found P1=99/P2=33new primary events (lower bounds;378/385lost ring
events). P1 switched naturally from rifle to phaser; P2 stayed on rifle2 and last
observed shot was host196.02, then ammo remained1. This exposed a real co-op input
bug: g_main built cmd.weapon from current ps.weapon every frame, applied a cycle
edge once, then discarded the requested target. Actual PM_BeginWeaponChange
lowers for200ms; PM_FinishWeaponChange reads the CURRENT cmd.weapon, which was
already reset to the old weapon. Compiled real cycle/begin/finish functions
reproduced the cancelled switch. New bounded persistent-request helper retains
selection through the transition, accepts subsequent cycle edges, respects
actual/script weapon changes and inventory removal, and resets on player/client
replacement or time rollback. Preserves original P1-preferred fallback on invalid
weapon. Actual-helper tests passed lost-edge reproduction and corrected cases.
No weapon timing, ammo costs or graphics settings changed.

Weapon-request correction build82654 passed fresh SP+MP, frozen pair at
coop_weapon_request_candidate: SP81af3a9288401797366f4668b0a95a05442d47a47761676200bca942d61a3ff3,
MPb38e97c1e04bf952c472d48377bb8605f770b2687aef6522dc17d893b247ce74.
Runtime verification currently active: probe82232, GPU91223, run name
coop_weapon_request_verify,270seconds,mode3P2,arrays0/PVS0. Revalidate handles.
No build running. Check liveness/ISO restore, actual P2 weapon choices and firing
through final gameplay; inspect native180 capture. Archive proof. Full-workload
performance comparisons still need stable GPU and same corrected-build inputs.
Earlier combat evidence archived at notes/evidence/coop_20260911/combat_arrays_diagnostic.
Audio and MP optimization remain paused; goal remains unmet and active.

## P2 overlapping HUD report and correction, Sep 11

User screenshot shows P2's flat split-screen HUD overlapped by a tall full-screen
HUD. CG_DrawActive in EF cg_draw.cpp drew both split viewports and cleared the
viewport, then executed CG_Draw2D a third time. The unbraced else controlled only
the EG20 diagnostic assignment, not the full-screen draw. Braced the entire
single-view branch, preserving both split draws and viewport cleanup.
verify_coop_hud_dispatch.py compiles and executes the real dispatch with draw
spies: split gets exactly one top/one bottom/no full-screen draw, SP gets one
full-screen draw, mode transitions restore viewport state, and non-SP/desktop
preprocessor variants pass. Reintroducing the exact old branch fails as expected.
This removes the duplicated overlay; it does not implement independent P2 HUD
player-state binding (both existing split passes still use the shared snapshot).
That remaining work belongs to the open co-op HUD/layout item.

Prior coop_weapon_request_verify ended live/error-clear with ISO restored.
101 moving/living observations each. Actual server firing events: P1 at least97,
P2 at least67; both equipped weapons1 and2. Thus the retained weapon request now
completes in live co-op. These ring counts are lower bounds (481/775 missed events)
and do not establish full workload. Diagnostic native avg18.8157, lowest sampled
9.787s span15.2235, complete10-second conservative lowerbound14.9. Host GPU
competition persisted, so this is not performance acceptance or a causal gain.
HUD correction SP+MP build active (session2416); native capture verification next.

HUD build2416 passed fresh SP+MP, runtime IDs default13:30:07/efmp13:30:47.
Frozen pair: build/research/letterbox_perf/coop_hud_single_draw_candidate.
SP SHA2489664da0e984dc6353806fe5249563af3dad3ba14f34a8b2006fd212d1bc10;
MP SHA79624a6bbab6985599961be690ff23e0a1131e6a6ac2574b0c07198cc8ef75ec.
Native capture xemu-2026-09-11-13-35-24.png from short borg1 co-op mode3 run
coop_hud_single_draw_verify was inspected individually. It shows one HUD per
viewport with equal scale and no tall duplicate over P2. Programmatic corroboration
at health-bar interior x72..114: P1 orange rows215..226, P2 rows455..466, exactly
240pixels apart. Old capture had additional rows429..453 over P2. The image check
and actual compiled control-flow regression test agree. Both HUDs still show
shared P1 stats; made that remaining requirement explicit in the existing HUD
to-do. No audio change, MP optimization, beta staging or git commit performed.

HUD verification24339 and GPU sidecar96958 finished normally. Probe270seconds,
mode3 combat, arrays0/PVS0, pinned unmodified XEMU. Final liveness and empty
engine error verified; ISO transaction restored. 107 sampled moving/living
observations per player, actual firing lower bounds108/80 with both weapons1/2.
Native gameplay avg17.4452, lowest9.765-second sample13.9271, conservative full
10-second lowerbound13.6. Nineteen aligned GPU samples show external3D mean1.63%,
max1.79%, much less competing host GPU than prior runs. The poor lows therefore
remain real evidence to investigate; do not dismiss this result as the earlier
SpiderMan contention. Different combat routes preclude causal before/after FPS
claims for the HUD correction. Goal remains active/unmet. No tests/builds remain
running from this checkpoint. Complete HUD proof archived as59files+manifest at
notes/evidence/coop_20260911/hud_single_draw. Next optimization work should use
this corrected HUD/weapon-input build and short matched full-combat comparisons.

## Quiet-host model-array comparison and wider diagnostic, Sep 11

Previous goal turn made progress: fixed/proved duplicate HUD; goal still unmet.
coop_hud_arrays_comparison_20260911_133937 is now terminal, live/error-clear,
ISO restored, same HUD/weapon-corrected XBE2489664d as control. P2 mode3 combat,
270seconds, arrays1 vs0/PVS0. Both equipped weapons1/2 and fired (observed lower
bounds91/68;437/675 missed ring events). Native180 capture individually inspected.
Native mean16.40495 vs17.44517 control; low9.836-second span14.23298, conservative
full10 lowerbound14.0 vs13.6. No useful gain. Both host GPU contexts were quiet:
external3D mean1.662/max1.841 across17 gameplay samples vs1.632/1.787 across19.
Combat routes vary despite identical inputs; do not claim a precise causal
regression or qualify sustained representative workload. Modes1/2 remain off.
Pair analysis: coop_hud_arrays_comparison.pair_analysis.json. Median packing
cycles rose1,401,093 to3,299,559; BeginPush cycles3,647,469 to5,423,450. Gameplay
scratch fallbacks262,206 to481,677. Model expansion avoids some index traffic
but introduces substantial copying/inline-buffer pressure.

Revisited prior evidence before implementing more ideas. Tight MDR fallback
and scratch rollover were already tried/rejected in Sep9 notes; do not blindly
reimplement them. Raw CPU pose cache also had an earlier host regression; MP
palette/ULP investigation is paused and has no accepted speedup. The successful
MP immutable world-array path is different from dynamic model expansion.

New opt-in diagnostic extends r_efCoopModelArrays with3(world+model timing) and
4(world+model byte verification). Existing0/1/2 semantics and default0 retained.
Covers indexed triangle batches through SHADER_MAX_INDEXES=6000, while input
vertex count remains capped at1000 and every source index is validated. Both
world and RT_MODEL paths are eligible only in co-op split3D views; HUD excluded.
Payload reservation already uses expanded count, maximum6000*13DWORDs plus
commands, below the1MiB primary and1MiB scratch capacities. No new GPU allocation,
LOD, material, attribute, triangle order, or authored-script change. Purpose:
remove the remaining dynamic world and oversized model index-cache traffic,
then measure whether eliminating that path outweighs dynamic copying cost.
It is NOT accepted optimization and is NOT the MP resident-memory mechanism.

Actual pack/verify/stream/packet helper test passed every attribute combination,
reordered/repeated indices and guard words through6000 indices. Checks exact
per-vertex GPU stream addresses and uninterrupted triangle-list ranges in255
vertex chunks. Existing small-batch corruption tests still exhaustive; larger
batches use strided/final DWORD mutation checks. Mode1/2 limits and world exclusion
retained; modes3/4 bounded6000, invalid counts and modes rejected. Analyzer now
accepts3/4 and requires all expanded bytes verified in mode4. Fresh SP+MP build
currently active session6228, log coop_full_arrays_build.log. No emulator active.

Full-array build6228 completed fresh SP+MP with no compiler errors. Frozen pair
coop_full_arrays_candidate: SPf2c954855db4b959ab73e443bc28bf3ccef63e858e256bb70023b39b4e877e32,
MP5258168d7404871c63c6ccbbbda7eb18f3b523c2896ba97586eee79b228de991.
Mode4 run coop_full_arrays_verify_20260911_135827 finished live/error-clear and
restored its ISO.108 moving/living observations per player; both fired and used
weapons1/2 (event lowerbounds98/80,466/734 ring events missed). Native180 capture
xemu-2026-09-11-14-02-04.png inspected individually: both views, scene, weapons,
effects, single HUDs present. Gameplay-mode4 counters:886,584 draws,
152,036,409 expanded AND verified vertices, zero mismatches, zero rejected draws.
No FPS acceptance from mode4: nativeavg15.8274, low13.8287/9.69s, conservative
full10lower13.4; verification adds substantial reads/comparisons.

Added optional --progress to analyze_native_video.py: excludes full native
profiler batches crossing intro/movie/gameplay interval boundaries. Existing
unfiltered behavior retained. Five tests passed, including initial history,
binary provenance, duplicate frames, boundary overlap and gaps. This extends
intro exclusion to the draw/update summaries; native FPS acceptance already
used filtered frame-counter deltas. New *.native_gameplay_video.json artifacts:
control1989records/indexupdates per draw0.08427/vertexupdates0.66274;
model-only1805records/index0.07048/vertex1.13299;
full verification1797records/index0.000002253/vertex1.25326, slowquartileindex0.
Thus model-only removed little costly index traffic; full-world+model expansion
removes it almost entirely. These counts prove mechanism, not its speed benefit
or equivalence to immutable MP storage. Full-copy bandwidth remains a tradeoff.

Full-array speed run now active: probe8342/GPU14886, name coop_full_arrays_speed,
mode3,270s, same frozen pair/P2mode3/PVS0. No build active. Wait for real terminal
state/ISO restored, then compare native lows, full combat, GPU/CPU conditions and
actual mode/zero verifier reads. Source default remains0; no promotion or retail
claim. Prior quiet model comparison archived56files+manifest in
notes/evidence/coop_20260911/hud_arrays_comparison.

Full-array speed run8342 and GPU14886 both terminal. Run
coop_full_arrays_speed_20260911_140507 completed270seconds, mode3/verification0,
live/error-clear/restored,110 moving/living observations each. Both players
fired and equipped weapons1/2 (event lowerbounds102/71;563/826 missed ring events).
Native180 capture xemu-2026-09-11-14-08-57.png inspected individually; no obvious
world/model/effect/HUD regression. Mean18.22980 native FPS; worst retained9.704s
window15.35512, conservative complete10lower14.9. This fails the requested30+
low-window target. No default enablement and no claimed material speedup.

Gameplay speed-mode counters952,996 expanded draws/159,725,277 vertices,
verificationreads0/rejections0. Native index updates/draw0.000002042, slowquartile
zero. Nineteen GPU samples external3D mean0.00093%,max0.01775%: competing GPU
applications do not explain this remaining slowness. Compare CPU/route context
before precise causal claims; the earlier17.445 control used the prior frozen
build. A same-new-build mode0 invocation is prepared but not run: do not spend
another repeated-route test merely to chase this sub-1FPS descriptive difference.

Speed-mode median packing4,240,350.5 cycles vscontrol1,401,093; BeginPush4,655,276.5
vs3,647,469; end-frame24guestms vs21; server6ms unchanged.584,131 scratch fallbacks
vscontrol262,206. Eliminating index-cache uploads alone does not meet target;
dynamic expansion exchanges that cost for repeated copying/upload pressure.
Keep modes3/4 as disabled diagnostic controls for separating index traffic from
geometry reuse. The successful MP path reused IMMUTABLE resident data, so its
15FPS result cannot be transferred to this copying implementation.

Proofs archived61files+manifest EACH under notes/evidence/coop_20260911/
coop_full_arrays_verify and coop_full_arrays_speed. New analyzer/coverage tests
included. No emulator, build or sidecar remains running; archive16598 is terminal.
Source default0 (PVS also0), SP/MP releases remain frame-diagnostic builds and were
not staged or committed. Audio and MP optimization remain paused. Goal active,
unmet; this turn made progress in mechanism isolation, not FPS acceptance.

Next larger opportunity to examine: immutable world geometry reuse for co-op,
with an explicit memory budget instead of copying MP's extra2MiB allocation.
Com_InitZoneMemory (z_memman_console.cpp) consumes the cached remainder via
GlobalAlloc after reserved pools; do not assume zone-free memory is suitable
contiguous GPU storage. Existing scratch allocates2x1MiB physical buffers. A
possible opt-in budget is one1MiB working slab plus one1MiB resident world slab,
with bounded cached metadata and normal fallback when storage fills. This is
NOT implemented or qualified: one working slab changes fence-wait behavior and
must be measured; account for added SP metadata and map-reset lifetime too.
Reuse existing per-surface/per-stage static guards and byte/topology verification
from MP. Relevant hooks are retail_xbox/tr_shade_retail.cpp,
tr_surface_retail.cpp, tr_local.h and win_qgl_dx8.cpp. Preserve all texture/zone
budgets and shipped MP behavior. Before another big allocation or performance
claim, quantify eligibility/capacity and test actual fences, failed allocation,
map reset, pixel/geometry preservation and native lows. A world-only dynamic
control could distinguish remaining copy cost without promoting full expansion.

## Co-op immutable world storage in the existing scratch budget

Current opt-in candidate borrows scratch slab 1 at frame begin, after a GPU idle
fence, leaving slab 0 as the working buffer. `r_efCoopWorldVertices` defaults to0;
mode1 reserves the slab without caching (control), mode2 uses immutable expanded
world vertices. `r_efCoopWorldVerify` defaults to0, mode1 enables exact attribute
and topology verification on each claim. Dynamic array and model PVS defaults
remain0. No texture/LOD/script/audio changes. MP retains its original cvars,
capacities, allocation/free behavior and eligibility checks.

The physical budget remains2,097,152bytes. SP metadata adds90,056bytes in static
arrays, plus small bookkeeping globals; cache payload capacity1,048,320bytes.
Ownership returns to the two working buffers only after GPU idle. Map reset
clears keys after GPU idle and never frees the borrowed physical allocation.
Capacity/ineligible batches fall back to the existing indexed path. A single
working slab can introduce fence waits; measure instead of assuming a benefit.

Actual-source tests passed: new verify_coop_world_storage covers frame-boundary
ownership, old pending GPU users, repeated full working-slab overwrites with
immutable cache comparison, mode0/1/2, capacity exhaustion/reuse, map reset,
mismatch disabling, SP/non-split fallback, missing device/slab/capacity and failed
physical allocation. Existing world indexed/array tests, scratch rotation tests,
co-op dynamic arrays and HUD dispatch tests also pass. Updated the old world
tests to include the actual base-pass global moved outside their extraction
marker by earlier diagnostic work.

SP+MP frame-diagnostic rebuild is running (session8246) to
build/research/letterbox_perf/coop_resident_slab_build.log. No emulator running.
Next: freeze/hash fresh pair, run short mode2+verify1 on original XEMU with both
players moving/firing/cycling, GPU recorder, natural intro exclusion and native
capture; verify storage/capacity/mismatch/fence evidence before speed mode.
Recorder now samples co-op storage ownership/budget and both world counter arrays
once per second. Not qualified, not enabled by default, goal remains unmet.

First resident-slab verification completed: coop_resident_slab_verify_20260911_143502
(SP2f8be294..., MPb6cbc19d...). Live/error-clear/ISO restored, both moving/firing
(105/74 observed firing events, weapons1/2), native capture14-38-51 inspected:
correct two viewports/single HUDs, no obvious new world/model/effect corruption.
105 gameplay counter observations verified19,608,021 resident vertices with0
mismatches/bad indices. Physical storage stayed2MiB, one ownership transition,
one working slab. Endpoint2048keys/18387vertices: the2K lookup table saturated
before the21840vertex buffer.1728 total capacity fallbacks,1009 inside gameplay.
Diagnostic native18.6379mean, worst15.89197over9.50165s/full10lower15.1; these
include verifier cost and do not qualify timing. Evidence archived59files+
manifest under notes/evidence/coop_20260911/coop_resident_slab_verify.

Bounded candidate corrects that lookup pressure:4096keys and maximum32linear
probes per SP array lookup; safe indexed fallback if the probe budget is hit.
Existing keys are inserted/retrieved within the same bound. MP lookup remains
unchanged. Static metadata now135,112bytes (+45,056 vsfirstslabtest), same2MiB
physical budget. Actual-helper collision test inserts32 colliding keys, rejects
33rd without invalidating existing entries, and reuses all32. Lifetime/storage
and MP array tests pass. Fresh SP/MP build50196 terminal0; log
coop_resident_bounded_build.log. Frozen pair coop_resident_bounded_candidate:
default.xbe f6297a9f250d26aa27e3cc7ec08ef1a97a04f9be7a9644aa5a669b3bae1f460e
efmp.xbe 57d0cb28def53c59af995f7d4523c087a67c5f8cf2dd870bedbce33d9328eb4b

Combined verifier active: probe91265/GPU80050,
coop_resident_bounded_verify_20260911_144428, cache2/verify1/arrays4/PVS0,
270seconds. Reuses eligible world geometry and dynamically expands the remainder,
verifying both paths. Speed invocation uses cache2/verify0/arrays3; same-pair
reserve-only and default-cache controls prepared with arrays3, not yet run.
No build active. Wait for actual terminal/ISO restored before another ISO writer.
Early runtime ownership/budget and requested modes confirmed. Defaults remain0;
no performance promotion. MP/audio stay paused, goal active/unmet.

Combined verification and timing checkpoint (all processes now terminal)

- coop_resident_bounded_verify_20260911_144428: live/error-clear/ISO restored,
  both moving and firing;105 gameplay observations.17,942,340 resident vertices
  verified plus126,090,051 dynamically expanded AND verified vertices.0 mismatches,
  rejected dynamic batches, or resident capacity fallbacks. Native image14-48-06
  inspected individually; no obvious geometry/effect/HUD regression. Diagnostic
  timing16.79575mean/14.0267worst observed9.98096s/full10lower14.0 includes both
  verifiers and is NOT speed acceptance. GPU19samples external3D mean14.477%,
  max15.073%, mostly desktop composition/remoting; do not call this a quiet-host
  match to prior near-zero externalGPU runs.
- coop_resident_bounded_speed_20260911_145129: same frozen f6297a9f... pair,
  cache2/verify0/arrays3/PVS0,270seconds. Live/error-clear/ISO restored;99/101 moving
  observations (102total),91/71 observed firing events, both equipped weapons1/2.
  Native image14-55-10 inspected individually; single HUD per view, no obvious
  new geometry/effect corruption. Verifier counts0.170,430 resident draws and
  701,358 dynamic draws/128,143,332 expanded vertices in retained gameplay,
  no capacity/rejection/mismatch failures. Ownership stayed borrowed once,
  2,097,152bytes of vertex-buffer allocation,135,112bytes static CPU metadata.
  The metadata is ADDITIONAL RAM use even though no new physical vertex slab
  is allocated; do not call this zero memory cost.

Speed result:18.452023 native frames/host second. Worst retained window
152.13551–161.74547hostseconds:131frames/9.60995seconds=13.631700FPS;
conservative complete10secondlower13.1. Goal fails. Do not promote or call this a
material gain over earlier18.2298full-copy test (different build/route/host load).
GPU18samples external3D mean10.2699%,max11.4645%; current host differs from earlier
near-zeroGPU runs. No additional same-route control was run just to chase a
fractional gain on a candidate well below30. Prepared controls remain unused.

The new *.slow_window_costs.json selects the window by NATIVE frame-counter lows,
then joins its sampled renderer data. Model publications are matched to draw-kind
sample/MainLoopCount and checked for identical totals; asynchronous names and
GPU backpressure remain caveats.30 frame samples in the worst interval vs307
elsewhere. Medians:609submitcalls vs467,40,435sourcevertices vs30,311,
96,996indices vs70,308; BeginPush6,268,564cycles vs3,787,806; packing4,359,079 vs
2,968,899; end-frame31guestms vs21. Server6ms vs5; world-traversal1ms both.

Model draws account for the rise:455calls vs315,31,140sourcevertices vs22,081,
79,323indices vs56,289,11.02Mdrawcycles vs6.47M. Resident-world calls87vs79 and
uncached base-world32vs32 are comparatively steady. Model BeginPush5.577Mcycles
vs3.069M; model packing4.103M vs2.695M. The model label on a blocked BeginPush
identifies where it waits, NOT necessarily the shader responsible for earlier
GPU work. Major measured models include Borg upper/lower MDRs, blite/tank/plugin2
MD3props. Keep effect/category limitations in mind; these are submitted model
batches, not a claim that AI or audio caused the dip.

Mechanism: native gameplay index updates/draw0.000002228, vertexupdates/draw
1.03605, DRAW_ARRAYS/draw0.90133. Indexed-cache misses have essentially disappeared
but substantial model copying/submission remains. Gameplay scratch370,106draws,
425,779inline fallbacks and0additional scratch fence-wait milliseconds. Endpoint
scratch wait5856ms occurred outside retained gameplay: do NOT blame it for the
measured dip.

Read-only next-step assessment in audit_coop_ordered_strips.py and
coop_model_ordered_strips_audit.json: inspected effective package LOD0 topology of
19 actual model leaders. Converting only beneficial surfaces to ordered triangle
strips with degenerate connectors could reduce emitted copies25–40% on heavy
Borg upper/lower models (borgbig4upper40.4%,borgthin2upper37.3%,borgthinupper35.3%).
Verified the same ordered nondegenerate triangles and winding for every audited
surface; no asset files or gameplay code changed. Prop savings mostlysmall
(blite9.1%,tank3.6%,plugin2 1.7%), so this is not a blanket10FPS promise. Runtime
native strip/range semantics, flat/provoking-vertex shading, command bounds,
cache/metadata budget, actual payload savings and visual/timing evidence remain
UNIMPLEMENTED/UNVERIFIED. Next work should target measured model costs instead of
more world-cache tuning. Do not restore rejected pose-cache or tight-bounds
experiments blindly; their previous negative results remain relevant.

Proof retention: slab verify60files, boundedverify61files, speed proofs plus
bounded source/test/build snapshot under notes/evidence/coop_20260911. All current
XBE files are diagnostic builds, not staged/released/committed. Default cache,
dynamic arrays and model PVS remain0. HUD guard and P2 weapon-request fixes remain.
No emulator/build/GPU sidecar running. Goal active/unmet; MP/audio still paused.

## One-time renderer review after the user's pause

Read `notes/renderer_review_2026-09-11.md` before continuing implementation.
This review supersedes the proposed next step of wiring ordered strips into the
renderer. The strip header drafted before the pause is still unintegrated and
untested. Review changed no engine code, built no executables and ran no new
emulator tests. Star Trek implementation/testing remains paused at this review
checkpoint so the user can restore the normal reasoning setting.

Priority: test a hybrid submission path that preserves compact indexed MD3/MDR
base passes while retaining the index-cache workaround for varying world, brush
and effect geometry. Animation changes vertices, not necessarily topology; model
array expansion copies roughly2.55x the source model vertex entries in the latest
slow window. Use actual model types/pass provenance, not RT_MODEL alone. Recheck
actual index churn: stable per-material counts are not proof of identical indices.
Next structural options are resident model attributes and compatible object
batching. A fresh short host profile with index misses bypassed should distinguish
upload/driver/guest costs; do not reuse the old cache-scan profile as proof of the
current bottleneck. Detailed evidence, limitations and draft-cache risks are in
the review. No FPS gain or goal completion is claimed.

## Hybrid model indices after the renderer review

Implementation/testing resumed after the review checkpoint. The new
`r_efCoopHybridIndices` experiment remains OFF by default and only changes
co-op array modes 3/4. It keeps full marked MD3/MDR base-pass batches indexed;
world, brush and generated/subset passes retain expansion. Existing attribute
packing, animation, shading and draw state are preserved. The source harness
passed MD3/MDR selection and rejection cases, the existing packing/packet checks,
and all 14 flight-recorder tests. Both SP and MP rebuilt successfully; the new
hybrid symbol exists only in the SP map. No release staging or commit.

Frozen pair: `build/research/letterbox_perf/coop_hybrid_indices_candidate`.
SP SHA256: 34c146b2aed3a8c8134b96f867b64b81e4d98450cdf7e944f1a28bf1e77ba3f4
MP SHA256: 70dfcb269a60ed3d145e9518018f9904cb64d3138d24a04a07bfee84cff77dd6

Enabled trial `coop_hybrid_indices_on_20260911_153941` finished live, error-clear,
with the ISO restored. Native gameplay average 19.1620 FPS; slowest observed
window 143 frames / 9.66362 seconds = 14.7978 FPS (complete 10-second lower bound
14.3). This FAILS the goal. Both players moved throughout the sampled route;
combat/weapon evidence is stored separately. The native 15-43-31 capture was
inspected individually: no obvious new geometry or duplicate-HUD rendering in
that image. Independent P2 HUD state remains open.

The mechanism executed: 575080 eligible model draws used 44114325 source vertex
entries instead of 115384590 expanded entries, avoiding 71270265 copies (61.77%).
These are vertex entries, not measured uploaded bytes. Native index updates per
draw were 0.00155261; vertex-buffer updates per draw 0.600398. Counts do not
measure API duration or prove that rare cache misses are cheap. No 30-FPS claim.

User capture d211ecee matches native frame 7480 exactly: MSPF 78, 599 draws,
194 array draws, 405 indexed draws, 2209 attribute binds, 409 vertex updates,
1 index update and 404 index reuses. Saved `*.user_capture_match.json` preserves
all counters and nearby asynchronous engine publications. The screenshot FPS13
and recorder FPS14 fields update independently. Do not derive FPS from MSPF.
The high draw/upload workload persists; this single frame does not show the
previous continuous index-miss storm. Host-thread profiling is still needed to
distinguish upload/driver waits from other renderer work.

GPU analysis correction: the new helper initially matched `engtype_3D`
case-sensitively, while recorded engine names were lowercase. Raw data was
preserved. Corrected summaries normalize case and exclude ONLY the owned XEMU
PID, not every process named xemu. Enabled gameplay external 3D mean12.3202%,
max16.7529%, 18 samples, no entirely unavailable samples, 57 invalid individual
counter instances. This was not an idle-host run. Do not blame another app for
the game bottleneck or claim matched contention. A same-binary disabled control
is in progress; record its terminal state and results below before comparison.

Disabled control completed: `coop_hybrid_indices_off_20260911_155033`, same
frozen binary and requested fixture, only hybrid mode changed. Live/error-clear,
ISO restored, two moving/living players, hybrid counters all zero. Native
17.4874 FPS mean; slowest 145 frames / 9.83109 seconds = 14.7491 FPS (full10 lower
14.5). GPU external3D mean5.5431%, max13.6219%,18samples,37invalid individual
instances. Enabled-minus-disabled raw mean difference1.6746FPS; lows essentially
unchanged. Different host activity and actual combat paths prevent claiming an
isolated causal gain of1.67FPS. Both fail the goal. Native15-54-11 image inspected
individually; no obvious new rendering corruption, P2 state binding still open.
All trial/analyzer/GPU sessions terminal; proof archive `hybrid_indices/manifest.json`.

The user now explicitly authorizes larger renderer work conditionally isolated
to co-op. Preserve normal SP and qualified MP; co-op is in the SP executable so
runtime mode/split-view checks are required in addition to MP exclusion. Next
larger candidate is resident model geometry, followed by compatible batching.
A dedicated short host-thread profile must distinguish remaining driver/upload
costs; do not count a suspending sampler interval as performance acceptance.

## Dedicated host profile and static model residency implementation

`coop_hybrid_host_profile_20260911_160119` completed live as a dedicated
210-second diagnostic; ISO restored. A single five-second sampler was triggered
by camera=false, at least550submitted draws and native FPSbelow22. No timing from
this run qualifies performance. All1284thread observations had prior suspend
count0. IMPORTANT: selecting the four busiest threads over250ms did not establish
graphics-thread coverage. PE RIP-relative assertion strings identify0x352830 as
DSP instruction execution,0x35e1c0 as DSP memory reading,0x3e51c0 as host main-loop
waiting; the0xb72a00 arithmetic helper is called from that main loop. Do not call
these renderer hot functions or use the sample as permission to resume audio.
Full raw profile, module inventory, disassembly and trigger are preserved under
`notes/evidence/coop_20260911/hybrid_host_profile`. Next host sampling should select
threads by demonstrated graphics execution, not a short CPU-delta ranking alone.

Disk-cache question: inherited files_console.cpp has Z:\jedi.swap, but that
GOB cache is excluded by STEFX_ELITE_FORCE_SP; active loader is loose/PK3. Xbox
X/Y/Z caches can help loading/repeated decompression, not substitute for RAM
attributes during rendering. Existing texture-swap infrastructure also references
Z:\skintextures; a filename in legacy code does not prove use by this fixture.
Host process reads in sampled gameplay: enabled569344bytes/139reads over123.34s;
disabled761856/186 over120.10s. Within the observed slow-window interiors:
86016bytes over8.22s and28672 over9.43s. These are process I/O counters, not isolated
game asset reads or disk latency. They do not prove zero I/O stalls, but provide
no evidence for sustained high-volume streaming as the main rendering dip cause.

New work in `stefx_coop_model_resident.h`: opt-in static MD3 GPU attributes,
256KiB bounded physical allocation +256bounded cache keys (7168bytes on32-bit),
no in-flight eviction. Mode0default; modes1normal/2exact-byte verification.
Single-surface provenance comes from RB_SurfaceMesh after overflow handling and
resets at each material batch. Only static frame0/single-frame surfaces, full
original triangle indices, supported constant/diffuse color and stable UV
attributes; preserves transforms and hardware lighting per draw. Animation,
fading, mutable attributes, unsupported passes/flags and mixed batches fall back.
Actual coop mode and split-view guards exclude normal SP; all hooks/cache code
compile out of SP-hosted MP. Cache release fences its own GPU users on map reset.

Actual-source unit checks passed all8attribute layouts/stream addresses,
co-op isolation, data reuse, verifier corruption fallback, static-policy rejects,
allocation failure retry suppression, capacity fallback and fenced reset.
Existing co-op array/storage tests, MP world cache tests and14recorder tests pass.
SP+MP fresh build is running; no runtime residency verification or FPS gain yet.
This static-prop stage is a foundation for broader model residency/batching and
DOES NOT narrow the30FPS low-window goal or claim that static props alone suffice.


## Static model cache first runtime verification and wider eligibility

Both fresh SP/MP builds completed. Frozen pair: `build/research/letterbox_perf/
coop_model_resident_candidate/identity.json`; SP SHA256 b95cf3f9316f239083bc6762cf16c8d9fd48c778720affe9d7dbfe97ddc247bd,
MP 72f5bc5be947bee908824fb17554a6330bd8419b4f4727d7b356cdc86ebd9707.
`coop_model_resident_verify_20260911_162131` completed live/error-clear and restored
its ISO transaction. Actual gameplay had 41,031 reused submissions, 1,823,568
byte-verified vertices, zero mismatches/failures/capacity fallbacks; only16 entries
and802 stored vertices by the last gameplay sample. The native capture at180s
was inspected individually: both views and combat rendered; independent P2 HUD
state remains open. This was a verifier plus brief host-thread sampler run:
its16.13 native FPS mean/12.88 slowest observed window is diagnostic only, NOT
an enabled/control performance comparison or evidence of a gain/regression.
Raw evidence is archived under `notes/evidence/coop_20260911/model_resident_v1`.
The header archive overlapped the next edit and was recovered verbatim from the
compiled v1 test translation unit; provenance is recorded in its manifest.

The broader host sampler covered35 threads/2240 observations in sequential
one-second groups, all prior suspend counts0. Most observations are waiting;
they are not CPU-time percentages. Thread35084 had8 AMD OpenGL-driver PCs;
35192 had2 driver PCs plus two in a function identified by embedded assertions
as tlb_reset_dirty_range_all. Thread1224 also reached that helper and a function
with vga_draw_graphic assertions. The0x28f2b0 range is APU voice processing,
not a renderer function. This sample establishes some driver/memory-management
coverage but is too sparse to apportion the rendering bottleneck.

V2 explicitly admits visibility, device lighting/depth and animated-texture
binding flags; keeps normalized single-frame geometry, stable generators and
full-index checks. Rejects deform/fade/tint/volumetric/distortion/unknown flags.
The old blanket nonzero-renderfx rejection excluded benign state; source shows
ordinary movers set RF_NOSHADOW. Do not claim its numerical benefit before runtime.
Added8 first-rejection reason counters to make remaining exclusions measurable.
A frame-boundary configure hook fences and releases optional storage when disabled
or leaving actual co-op mode. Tests pass allowed flags/combinations, rejected
flags, all8 layouts, byte mismatch fallback, bounded allocation, disable/mode-exit
lifetime; world ownership and14flight-recorder tests pass. Fresh v2 SP+MP build
is running (`coop_model_resident_v2_build.log`); invocation files are prepared but
no v2 emulator run has started. Experimental defaults remain OFF; goal unmet.


## V2 static residency runtime proof; timing comparison in progress

Both fresh v2 builds completed successfully. Frozen `coop_model_resident_v2_candidate`:
SP410c576d5d73a17f99b2cb38b2207784a8e6b0a31516555477515900db5c7627;
MP71bdb67ed90cab81f7c9fd394c417dbc3b354309df432ef9c6a2ef95311b44bd.
New reject symbol exists in SP and is absent from MP. Full hashes are in identity.json.
Verifier `coop_model_resident_v2_verify_20260911_164209` ended live/error-clear,
ISO restored, both players moving/firing/changing weapons. In confirmed gameplay:
174891 hits,10779942 verified vertices,zero mismatches/allocation failures/capacity
fallbacks; final65entries/4588vertices in256KiB. Source flag widening increased
coverage substantially. First-reject deltas: provenance702037,animation49181,
color37794,UV10016,shader3; all other categories0. Provenance includes non-MD3
world/MDR geometry and mixed batches, so do not call these model-only percentages.
Native screenshot16-45-59 was individually inspected: both views, weapon effects,
world/props render; P2 independent HUD state remains unresolved. The byte verifier
run's18.68 native FPS mean/16.90 worst observed window is not timing acceptance.
External3Dmean5.37%,max8.59%,zero invalid instances; no thread suspension in v2.
Mode1 timing `coop_model_resident_v2_speed` is now active,270seconds, same frozen
pair and other settings. Probe22425/GPU51032; revalidate terminal/restoration before
starting prepared `coop_model_resident_v2_off` mode0 control. No build is active.

Read-only next structural assessment: animated payloads can potentially reuse
already claimed scratch-ring ranges WITHIN the same render frame, without a new
GPU allocation. ScratchClaim is a monotonic bump allocation with inline fallback,
no within-frame reuse; frame begin fences the reused slab and resets its offset.
A bounded per-frame cache can retain only successful scratch allocations, reset
all keys before any slab reuse/ownership switch, and leave inline fallbacks uncached.
It must retain exact current material/format/color/pose keys, single MD3/MDR
surface provenance after overflow, actual co-op guards, and verifier fallback.
Use original planar stream layout for reused scratch payloads, not the48byte
static resident layout. Do not cache expanded/subset/fog/deform/mutable-generator
payloads initially; preserve every transform, lighting and index submission.
This is not yet implemented or qualified. It differs from the rejected MP CPU
skin cache because a hit would skip the GPU payload write too. The MDR palette
cache generation is MP-only and cannot be silently treated as valid in co-op;
verify complete deterministic pose inputs or explicitly track palette lifetime.
Future tests must prove same-frame reuse, next-frame invalidation, scratch-full
fallback, changed poses/materials, exact referenced GPU attributes and both modes.
The larger submission/batching opportunity remains relevant; static residency
alone does not satisfy or redefine the30FPS slow-window goal.


## V2 enabled/control completed; client-phase instrumentation next

Both270-second timed runs used the same frozen v2 pair and original pinned XEMU,
world2/verify0/arrays3/hybrid1/PVS0,full P2mode3 workload. Intro intervals excluded.
Enabled model cache mode1 (`coop_model_resident_v2_speed_20260911_164918`):
20.1271 native FPS mean,worst168frames/9.66606s=17.3804,complete-ten-second
lower bound16.8.196960cache hits saved11161366 vertex rewrites.112movement
observations each,P1/P2 firing108/78,weapon sets1/2;zero mismatches/fallbacks.
Disabled mode0 (`coop_model_resident_v2_off_20260911_165642`):20.7161mean,
worst153/9.53829=16.0406,full-window lower bound15.3;cache counters/allocation0.
108movement observations each,P1/P2 firing111/83,weapon sets1/2. Both live/error-
clear/restored. Native images16-53-00 and17-00-31 individually inspected; both
views and combat render, differing poses/health/workloads prevent pixel identity.
External GPUmean/max:6.49/6.76enabled,6.64/7.51disabled;no invalid instances.
Enabled had189587MDR submissions vs162745disabled. A0.59FPS lower enabled mean
and1.34higher worst-window value do NOT establish a dependable gain or isolated
regression. Keep default0; do not qualify, promote or repeat broad timing soaks.
203source/evidence files retained in notes/evidence/coop_20260911/model_resident_v2;
manifest written after all probe/GPU handles finished. No builds overlapped tests.

A20-observation read-only sidecar in the control used existing phase/counter
symbols and translated RAM from its own recorder setup, with no pause/input.
HUD0-1ms,scene3-5ms,whole sampled client call13-24ms. Actor entity bucket1
median3,520,295guest cycles/18entities; do not convert this into host GPU time.
These asynchronous counters rule against assuming the HUD dominates, but cannot
join all client subphases into the same completed frame. Existing CG_View locals
already time setup,prediction,view,entities,tail,anddraw; they were not exposed in
co-op's flight records. Ordinary CG_DrawActiveFrame constructs the scene once;
do NOT claim it calls the entire client game twice merely because there are two
renderer views.

Added co-op frame-diagnostic-only publication g_SPXBCoopCgamePhases[45]: odd/even
sequence,completed client frame/server time,total/setup/predict/view/entities/
tail/draw/scene/HUD milliseconds,main loop,16entity cycle counts and16counts.
Publishes existing timers after drawing,only for split SP/co-op; compiled out of
SP-hosted MP and non-diagnostic builds. Recorder reads a stable even publication
and rejects odd/truncated/changed/duplicate snapshots.16recorder tests pass,
including torn publication and field-layout checks; diff whitespace clean.
Fresh both-build command now active session82623,coop_cgame_phases_build.log.
This adds measurement,not claimed FPS gain. Next freeze pair and run270seconds
with model cache OFF (other baseline co-op settings retained), then join these
client phases to the native worst windows before choosing the next optimization.
Scratch-backed animated reuse remains a source-supported option, not implemented;
its benefit must not be assumed after the weak static-cache timing result.


## Coherent client phases completed; finer tail trial running

`coop_cgame_phases_20260911_170952` completed, live/error-clear, ISO restored.
Native19.7099 mean; slowest146frames/9.61249s=15.1886FPS, complete10second
lower bound14.6. Both players moved in100/102observations, minimum health97/100.
External GPU2.95%mean/11.85%max, no invalid observations. Native17-13-42image
individually inspected. Worst9client publications: total27,setup1,predict0,
view0,entities7,tail11,draw6,scene5,HUD0ms; other93:total21,entities7,tail9,
draw5,scene4. Actor bucket1 worst median4.55million cycles/21entities vs4.27M/
19elsewhere. These are sparse coherent client observations; other publications
are asynchronous and independent medians do not sum to exact frame totals.
Worst renderer frame65ms,server7,screen26.5,endFrame25.5,backend25,600.5submits.
Both renderer and client costs matter.76files archived with manifest under
notes/evidence/coop_20260911/client_phases before subsequent source edits.

Renamed extended diagnostic publication g_SPXBCoopCgamePhasesV2[51] adds six
same-frame tail timers: P1weapon, P2refdef/weapon (includes test model), listener,
powerup, FX_Add, residual misc. Reader retains45word v1 compatibility and tests
both layouts/torn data;17tests pass. This measures existing calls only and does
not change audio behavior. Bothbuild70393 completed successfully; SP17:26:30,
MP17:27:07. Frozen coop_cgame_tail_candidate/default.xbe SHA256
 a70d0a37b904d4b0f70affad3a45c64ffca36a4584e0ff981001f8ece6d5a5ab;
MP0be5b9d5a76cb147925f81e2a02d23a03f0ae985a65556c233a2edd4e12d8634.
270second trial coop_cgame_tail running session33957, GPU74253. No build
concurrent with it. Settings modelcache0/world2/verify0/arrays3/hybrid1/PVS0,
P2mode3. Wait actual terminal and restored:true before next ISO mutation.


## Effects processing isolated from client tail

coop_cgame_tail_20260911_172958 finished live/error-clear/ISO restored; probe33957
and GPU74253 both exited0. Native16.0291mean,worst74/9.77479=7.57050FPS,
complete10second lowerbound7.4. Both players moved,fired and used weapon1/2
(see world helper combat proof). External3D3.82%mean/4.45%max,invalid0.
Worst10client publications: total26.5,entities8,tail10.5,FX_Add10ms. Other94:
total22,entities7,tail9,FX_Add9. P1weapon worst0.5ms; P2refdef/listener/powerup/
misc each median0. Effects account for nearly all tail cost. Worst renderer
backend37ms/BeginPush16.88millioncycles vs20ms/3.45million other: effects are
one contributor, NOT a complete explanation of the7.57FPSdip. Native17-33-41
capture individually viewed: both views/HUD, moving combat, P2largeauthoredflash.
78files archived notes/evidence/coop_20260911/client_tail/manifest.json before
subsequent edits. Sparse guest-clock phase medians are not host GPU timings.

Next bounded diagnostic adds g_SPXBCoopFxPhases[294] in FX_Util.cpp under SP,
Xbox,frame-diagnostic,!hostedMP guards and runtime split eligibility. Measures
free/update/cull/draw counts+cycles by exact native vtable,32boundedrows with
lastslot overflow; completed-frame odd/even publication. It keeps original
iteration/order,physics,visuals,and effect lifetime. Reader validateslayout,
version,bounds,torn/duplicate data.19flightreader tests pass. Bothbuild71687
running coop_fx_phases_build.log; no test/build overlap. Frozen nextpair/trial
will be coop_fx_phases. No optimization/default promotion yet.


## Spark trail collision root cause and first conditional fix

FX-detail run coop_fx_phases_20260911_174118 completed, live/error-clear/restored,
probe50420/GPU66174 exited0. Native19.6979mean,slowest152/9.48569=16.0241FPS,
complete10lower15.2.105movementobservations/P1moving104/P2moving103;both combat
and weapon1/2 proven. External3D4.67%mean/4.99%max,invalid0. Native17-45-11image
individually viewed. Worst9FXobservations:217liveeffects;8.28million updatecycles,
0.188Mdraw/0.108Mfree/0.022Mcull. FXTrail alone8.21Mupdatecycles for155trails;
other96observations FXTrail6.22M/117.5trails. The trail UpdateOrigin collision
trace is the expensive candidate; rendering/culling/allocation are not dominant
within this FX_Add cost.82evidencefiles archived notes/evidence/coop_20260911/
fx_phases/manifest.json before further edits.

Added default-OFF cg_efCoopFxTrace (1fast/2verify), only called from effect-particle
bounce physics on Xbox EF SP executable, runtime actual coop+2players. Ordinary
SP,player/weapon/server traces and hostedMP retain their paths. Reject encoded
BODY-only boxes when mask excludesBODY; unrotated brush models get conservative
swept-AABB disjoint rejection (+1unit margin); rotated/unknown inputs fallback.
Bounds come from actual collision models,64slots refreshed at solid-list rebuild.
Verifier still performs every original collision test and detects any proposed
skip that would change fraction/allsolid/startsolid; it retains the original hit
and disables optimization immediately on mismatch. No visual/physics reduction.
40,000 executable comparisons of real trace loops passed, including null hulls,
stationary queries, masks, rotation fallback, bounds invalidation, SP/wrong-mode
isolation and deliberately corrupted bounds triggering fallback. Calls in the
synthetic fixture1,213,465 ->406,757; this is NOT measured game FPS gain.

Initialbuild18595 failed linking helper placed in inactive cm_load.cpp; moved
helper to active cm_load_xbox.cpp and removed original-file edit. Retry16030
running coop_fx_trace_build_retry.log. Fresh pair/live Xbox-verifier run pending;
no claim that the30FPS target is met. Flight recorder adds12trace counters.


## Original collision verifier passed; bounds-cache refinement

Frozen coop_fx_trace_candidate SPcef407c367b61f944aa95fc1e2f3bb913d1d34d38cc4422935d5c88ce47e120e;
MP521eeed6ed71a2e57df11c8d8b5101dc225b9dc6c5672b0cc5e9126e1d494b86.
Both retrybuild16030 successful/fresh, SP17:54:43,MP17:55:21. Trial
coop_fx_trace_verify_20260911_175621 finished live/error-clear/restored; probe
83577/GPU66958 both exited0. Confirmed gameplay135635queries,14,686,321verified
rejections,zero mismatches:5,153,336mask+9,532,985bounds. Each query averaged
109.28entity visits; world5073guestcycles,entity111693cycles (cycle counter wrap
handled by consecutive modulo deltas). This is diagnostic verification, NOT a
speed trial:18.804mean/16.052worst native,complete10lower15.6. Both players moved/
fired/changed weapons; native18-00-02capture individually inspected; external
GPU4.65mean/4.84max,invalid0.89files archived fx_trace_v1/manifest.json.

Verification exposed7,072,464boundsfills,52perquery: direct-mapped64entry cache
thrashes among this scene's brush handles. Before timing, changed to one bounded
entry per solid-list slot (256*44=11264bytes), refreshed on list rebuild, model
or actual evaluated origin change. Store validated world bounds to avoid repeated
numeric validation/additions per spark. Rotations and invalid/large coordinates
still fallback. Swept bound margin1unit; deliberately conservative numeric limit
1e6 keeps rounding below it. Original trace-loop comparison still passes40000
cases; added same-frame origin/model invalidation checks. Fresh bothbuild47102
running coop_fx_trace_v2_build.log. No runtime defaults promoted.


V2 build47102 completed successfully; SP18:05:01/MP18:05:37. Frozen
coop_fx_trace_v2_candidate SPc1affe82ab5f02dbca16325e794547c35dc7be8fdf7937cf7332aceeba50d047;
MPa73d646a6cbf3822558cd481e889d436a38aed820f99c3f4b8ca1c3c54f56c02.
Short210second correctness run coop_fx_trace_v2_verify_20260911_180702 is saving
endpoint evidence (probe18911/GPU58520); lastlive209s:9,458,233verifiedrejections,
zero mismatches,86,021queries,110,937boundsfills. Await terminal/restored before
next ISO writer. No FPS acceptance from this verifier. Next timing invocations
coop_fx_trace_v2_speed/off use270seconds and this same pair,mode1/0 respectively.


V2 verifier finished live/error-clear/restored; probe18911/GPU58520 exited0.
210second diagnostic produced7,143,529verified rejects during confirmed gameplay
(9,458,233bylastlivewhole-run observation),zero mismatches.65,030gameplayqueries,
7,208,936entityvisits,67,780boundsfills (~1.04/query instead of52). Original-native
image18-10-45individually inspected. Native15.84mean/14.72worst is diagnosticonly;
external3D10.18mean/12.93max included another XEMU PID25760 temporarily alongside
owned31936. No XEMU process remained on subsequent read-only process check.
Do not attribute that second process's launch without evidence; do not stop it.

An18.53second capture/sampling gap made modulo32cycle deltas ambiguous. Analysis
now omits gaps>3seconds for cycle totals and uses only their covered query count;
verification/operation counts retain all intervals. This is measurement handling,
not a collision mismatch or an assertion of a crash. V2 sampled covered47,102
queries; world5451/entity115212cycles per coveredquery inverificationmode.
Timing invocations remove native screenshots to avoid capture interruptions;
verifier already supplies native visual evidence. They keep frozenpair,270seconds,
fullmovement/firing/weapon switches and intro exclusion. Current enabled timing
coop_fx_trace_v2_speed session85483, GPU70804, mode1. OFFcontrolinvocation ready.
No builds/other ISO writers while timing runs. Goal remains unmet.

## Particle-collision timing comparison completed

Frozen v2 pair, original pinned XEMU, same270-second borg1 fixture and rendering
settings; no screenshots during timing. Confirmed gameplay excludes text/camera
intro. Both probes and GPU sidecars exited0; both endpoints live/error-clear and
ISO restored. No default promotion: cg_efCoopFxTrace remains0.

- Enabled coop_fx_trace_v2_speed_20260911_181521:22.4411nativeFPS mean;
  slowest175frames/9.70216seconds=18.0372FPS; complete10second lower bound17.5.
  117observations,116moving both; minimumhealth85/100. Observed P1/P2 primary
  fire events113/87, both weapons1and2. External3D4.62mean/4.86max,invalid0.
- Disabled coop_fx_trace_v2_off_20260911_182315:20.0225mean;
  slowest160/9.97002=16.0481; complete10second lower bound16.0.
  119observations,117moving both; minimumhealth94/100. P1/P2fire119/95, both
  weapons1and2. External3D4.75mean/4.93max,invalid0. Trace counters all0 as intended.

This pair improves mean2.42FPS and observed worst1.99FPS. Combat outcomes and
worst-window locations differ, so do not treat as a universal gain or30FPS pass.
Enabled153029particlequeries visited16932383entities but only153228full entity
CMcalls (~1/query vs110visited);5993301mask and10785854bounds rejections.
Covered world4443/entity16738guestcycles per query; no omitted cycle intervals.

Coherent client FX costs: enabled worst3ms/other2ms vs OFF worst10ms/other9ms.
Other-window trail medians117updates/1.266Mcycles enabled vs116/6.068M OFF.
The gain comes from rejecting impossible collisions, not reducing effects,
particle lifetimes, update rate, movement, or visual detail. Xbox v2 verifier
previously checked7.14million gameplay rejections with0mismatches; all40000host
trace comparisons and19flight-recorder tests rerun passed. Both fresh builds
already passed and hashes unchanged. Retail and additional-map qualification
remain outstanding.

The enabled worst window still has19msclient work (10msentities,3msFX,6msscene),
25msrenderer backend,~590submissions and7msserver. Independent sampled medians
are not an atomic frame sum. Current next source targets are actor construction
and compatible model submission, not further disk caching or blind LOD cuts.
CG_Player already emits phase IDs around lighting, angles, animation, shadows,
attachments and submission, but these IDs are not duration measurements. Add
coherent per-phase cycle measurements before choosing a new actor shortcut.
RB_RenderDrawSurfList flushes on entity changes unless shader entityMergable;
blindly relaxing that is unsafe because transform, shaderTime, lighting and
renderfx differ by entity. No batching/actor behavior changes implemented here.

Archive helper archive_coop_fx_trace_v2.py retains all three v2 trials and exact
current sources before subsequent production edits. Current archive session4553
must finish successfully before editing these sources. Goal remains active and
unmet. No emulator or timing run remains active; no source build active.

Archive4553 completed exit0:212files retained under notes/evidence/coop_20260911/fx_trace_v2/manifest.json. No pending probe, GPU sidecar or archive session at this checkpoint.

## Actor phases isolate shadow projection

Added diagnostic-only completed actor-phase publication with early-return scope
accounting;20reader tests and controlled-clock nested/early-return/inactive/reset
test passed. Fresh SP/MP build41679passed; frozen coop_player_phases_candidate
SP9445627fff40c617a45bc4131463c6aff5e517632e5e6a9e5e5376bd9490feb1,
MPb957ec3192d3486b315c2aee5b7a40d0500f62ef4cca69748c4f554583d99528.
Trial coop_player_phases_20260911_183703 finished probe54517/GPU8725exit0,
live/error-clear/restored.23.8722native mean,179/9.53018=18.7824worst;complete10
lowerbound17.9. Instrumentation-only: do NOT claim a gain over previous22.44.
External3D1.26mean5.20max,invalid0 differs from previous trials. Other XEMU2304
had OpenRedFaction config at preflight; was left alone, not in gameplay3D samples.
Both players moved/fired with weapons1/2. Shadow impact-mark phase worst3.251M
cycles of5.186Mactor total (20actors), other2.571Mof4.064M (19actors). Ground
trace ~0.36M; animation and tag attachment far smaller. Archive57017 retains
source/evidence under player_phases before next production changes.

Next candidate: bounded exact cache of temporary shadow mark fragments in cgame,
co-op only/defaultOFF. Geometry depends on four projected points and direction;
CG_ImpactMark still computes current UV/color and submits each shadow. Cache must
reset at CG_InitMarkPolys (CG_PreInit and CG_RestartLevel), reject capacity excess,
and verify every hit against original geometry in mode2. No LOD/opacity/lifetime
reductions. Goal remains unmet.

## Exact shadow-fragment cache implemented, live verification pending

Default-OFF cg_efCoopShadowCache:1reuses exact fragments;2compares every hit to
original world projection and keeps original output. Actual coop/split>=2 guards,
compiled out SP-hostedMP. Only temporary shadow-shader marks eligible. Key is all
four input points, projection and original output limits.32LRU entries,64points/
16fragments each;31368staticbytes. Capacity overflow keeps full original384point/
128fragment calculation. CG_InitMarkPolys invalidates on CG_PreInit/restart;
mode2mismatch permanently disables cache for process and retains original output.
UV/color/opacity and every shadow submission remain original. Host20000comparisons
passed including exact output, eviction, mutable caller copies, changed world/
limits, oversized and empty results, age wrap, runtime guards and fail-safe.
20flight-recorder tests passed. Fresh bothbuild90892passed,18:48:23/18:48:59.
Frozen coop_shadow_cache_candidate SPba03420027369732f7312bf9fab802fe1d140c6a83e1bbb6fa5db1f74bd3c915,
MPdb055d48cb3dff533a620ac8bb8c5ad3723a9e7c547dd194bac9cb07a9c87a6b.

Current210second correctness run coop_shadow_cache_verify:probe12829/GPU5227;
mode2,particlefix1,modelcache0/world2/arrays3/hybrid1/PVS0; P2mode3/fullP1input.
One native screenshot requested after180seconds. Speed/off270second invocations
ready and keep screenshots disabled. Wait actual probe terminal/restored before
next ISO writer. No timing/build overlap. No claim of shadow FPS gain or30FPSpass.
Actor instrumentation evidence archive57017completed94files before cache edits.

Shadow verifier coop_shadow_cache_verify_20260911_185026 finished probe12829 and
GPU5227exit0,live/error-clear/restored. Confirmed gameplay30988queries,23928hits
(77.22%),7060misses,5502stores,1558capacityrejects;23928byte comparisons and0
mismatches. Cache31368bytes. Original-native18-54-08capture individually inspected.
53playerobservations,52/51moving,minhealth90/77; P1/P2fire40/33, weapons1/2both.
External3D0.055mean0.104max,invalid0. Diagnostic23.298mean/20.257worst is NOT a
cache speed result (mode2recalculates originals and capture interrupted sampling).

Enabled mode1 trial coop_shadow_cache_speed now running:probe89869/GPU14467,
270seconds,no screenshots,same frozen pair. OFFcontrol invocation ready. Await
actual terminal/restored, analyze native means/lows+combat+cache+GPU, then run
OFFcontrol before archive_coop_shadow_cache.py (allthree trials required).
No build active. No new production edits until allcurrent sources/evidence archived.
No default promotion. Goal active/unmet.

Next read-only renderer lead: existing MDR_SKIN sample837 in
coop_player_phases_20260911_183703_xblog_profiles.log reports444surfaces,16559
vertices,9053repeatedVertices,index203858/palette396505/skin6983379guestcycles.
This is a sampled surface-kernel cost, not yet joined to the worstnativewindow.
tr_animation.cpp already has MP-only CPU-skin cache (8192vertices*32bytes=256KiB,
256keys), exact vertex verifier, and same-mainloop/renderframe invalidation.
Co-op cannot merely enable its cvar: hooks and palette-generation bookkeeping
are compiled only for STEFX_SP_HOSTED_MP. Current co-op bone-palette cache uses
entitypointer identity and16slots; sharedIdentity only enabled forMPthree-plus
view economy. If extending, use a separateactualco-op/defaultOFFguard, preserve
MPbehavior, and explicitly establish bone-palette identity/lifetime; do not
assume repeated surface/frame keys alone prove identical skin results. Existing
pointer+generation keys may limit reuse across views when16palette slots churn.
No MDR skin-cache edits in this turn.

## Shadow timing pair complete; no low-window gain established

Both 270-second probes and GPU sidecars exited0, live/error-clear/restored.
Same frozen ba0342 SP / db055d MP pair, particle trace1/world2/arrays3/hybrid1,
modelresident0, P2mode3 and full P1 replay. Enabled coop_shadow_cache_speed:
25.8299 native mean,188frames/9.53273s=19.7215 worst (full10lower18.8).
OFF coop_shadow_cache_off:24.3587 mean,206/9.76048s=21.1055worst (lower20.6).
Mean improves1.47 but lows do NOT improve; different combat outcomes prevent
claiming uniform benefit. Enabled P1/P2 fire109/78, OFF110/88; weapons1/2both.
Enabled shadow hits48061/61379 (78.30%),0mismatches, cache31368bytes.
Enabled slow-window actor shadow0.897Mcycles vs OFF2.670M; actor counts21vs20.
External3D enabled0.057mean/0.143max,invalid0; OFF0.010mean/0.089max,invalid10
(no unavailable samples). Invalid counters limit host-context completeness.
Archive78784 finished225files at notes/evidence/coop_20260911/shadow_cache;
exact source/test/build/identity and all3trials retained before further edits.
No release defaults promoted. Goal active/unmet.

Next candidate: separate actual-co-op-only CPU skin cache with exact copies of
bone palettes, byte comparison (not hash-only identity), per-frame invalidation,
bounded allocation and original fallback. Keep MP cache code unchanged; renderer
cache must release on mode exit/shutdown. Full geometry verification before FPS.

## Co-op CPU skeletal cache implemented; Xbox verification pending

New code/renderer/stefx_coop_mdr_skin.h, r_efCoopMdrSkin default0 (1fast,2verify),
actual coop/split>=2 and per-view split guards; compiled out of MP. Separate
implementation preserves MP cache unchanged. Matches full palette bytes (including
all bones), header/frame/oldframe/exactlerp and surface identity; cache clears on
main-loop or render-frame change. Copies only xyz3/normal3/uv2 into original tess.
Original transforms/materials/indices/GPUsubmission unchanged. Entire bounded
storage334832bytes via HeapAlloc, failure retains original with one attempt until
reset; released on renderer shutdown or leaving mode at frame begin. Mode2retains
original output and latches cache off for process on byte mismatch.
Host20000exact-output comparisons passed: palette mutation/address changes,
output/padding, modes, allocationfailure, vertex/key/pose/bonebounds and cleanup.
Existing MP cache test and21flight-recorder tests passed. Added coherent11word
MDR kernel publication joined by sample serial to completed-frame costs.
Fresh bothbuild12841running, log coop_mdr_skin_build.log; no emulator/test overlap.
verify/speed/off invocations prepared under coop_mdr_skin_*; freeze fresh pair
before launching correctness210sec, then timing270sec controls. No defaultpromotion.

Both fresh build12841exit0, runtimeSP19:19:36/MP19:22:18. Frozen
coop_mdr_skin_candidate SP310abf8a1a00510c8b3909c164d8ac1ca986eee1b4f9ee9924f20062a1f44880,
MP9b5e4e3f760b7a6d6e627589497e2cf9ff76ca892c687db117637ffd6287d39a.
Cleanup removed3files; correctness210second verifier now launched. No production
edits until this frozen candidate is evaluated/archived. No default promotion.

## First co-op skin verifier failed; investigate before timing

coop_mdr_skin_verify_20260911_192436 probe30726/GPU96928exit0, live/error-clear and
ISO restored. Mode2 latchedoff at t155 after1053050matching vertices and1mismatch.
Allocation334832bytes succeeded. Originaloutput retained; no enabled/off timing.
Both moved; gameplay observed44samples, fire29/32 but onlyweapon2observed in this
short verifier. Native24.88mean/21.90worst is diagnostic only, NOT a gain.
External3D6.37mean6.53max (includes xml-dx8-worker),invalid0. Other processesleftalone.
Native19-28-26capture individually inspected: both viewscombat, no obviousnewmesh
problem; cache had already disabled. Failed evidence/source archive93648finished
109files at notes/evidence/coop_20260911/mdr_skin_v1_failed before edits.

Source review found mode-exit cleanup hook was in inactive tr_cmds.cpp, so cache
allocation remained after failure. Moved hook to active retail_xbox/tr_cmds_retail.cpp;
shutdown hook in active tr_init.cpp remains. This is distinct from vertex mismatch.
Added co-op-only diagnostic persistent48word firstmismatch: original/cached xyz/
normal/UV bits, mesh hashes, actual palette byte comparison/hashes, x87controlword,
MXCSR, basevertices and pose/frame IDs. Header/cache lookup unchanged pendingproof.
Historical MPnotes document samepalette/smallULP failures unresolved; do not assume
frame-pair lifetime bug or floatingpointenvironment explains newfailure withoutdata.
Bothbuild pending in coop_mdr_detail_build.log. Goal active/unmet; defaults0.

Detailed bothbuild44032passed, SP19:34:08/MP19:34:45. Fresh frozenpair
coop_mdr_detail_candidate SPf19e88428e7d7657d518391e24f82bb0ed9316ee828781ca555bc003835ff33c,
MP156ca736505c3556fda14b6e9fc0e3ff36387787346b8083b4758a1acf601130.
Correctness run coop_mdr_detail_verify active:probe63271/GPU46587,210seconds,
mode2 plus prior particle1/shadow1/world2/arrays3/hybrid1. No build overlap.
Read persistent firstfailure via decode_coop_mdr_failure.py NAME --live; do not
speed-test a mismatch. Previous failedcandidate archived, newproduction edits
must wait thisrun's terminal/restoration/archive. No release/default promotion.

## Detailed mismatch retained; bounded numerical audit prepared

Detailed verifier63271/GPU46587exit0, live/error-clear/restored. Firstdifference
at t131.145, vertex0of47, frames616/616, bones14, base0both. Actualpalette memcmp0,
meshhash b767c5a2both, palettehash266e1436both; x87CW0x23fboth, MXCSR0x1fb3both.
XYZ maximum observed difference1.90735e-6, normals1.19209e-7; UVsexact. First55,031
vertices matched before onefailure. Active framehook nowfrees allocation after
failure (bytes0/resets2), confirming the cleanup correction. No exactnesspass.
Native25.52mean/23.28worst is diagnostic only; failedbeforeconfirmedgameplay.
Bothplayers fire32/33, weapons1/2; external3D4.858mean4.953max,invalid0. Native
19-39-45capture individually inspected. Archive61997finished115files under
notes/evidence/coop_20260911/mdr_skin_detail_failed before nextedits.

Disassembled exactfrozenRB_SurfaceAnim via pefile/capstone; source DotProduct
usesinlineSSE, emitted skin loopSSE. Architecturalroundingstate matched; doesnot
prove emulatorinternal/temporary state constant. Read pinnedXEMU primarysource:
https://raw.githubusercontent.com/xemu-project/xemu/fc9980d2962cbec656253106ea2e121fab1e68d4/target/i386/tcg/fpu_helper.c
https://raw.githubusercontent.com/xemu-project/xemu/fc9980d2962cbec656253106ea2e121fab1e68d4/target/i386/ops_sse.h
No emulator bug claimed or binarymodified; no MXCSRworkaround implemented.

Next diagnosis expands observations beyond first tiny mismatch. Diagnostic-only
r_efCoopMdrRoundoffAudit default0, usedwithskinmode2. Always keeps originaltess;
counts bit-exact versus roundoff and maxima. Predeclaredbudget position1e-4model
units, normal1e-6; UVs mustmatchbits, nonfinite differencesfail. Anybudgetexcess
falls through toexistingstrictdisablingverifier. This is NOT exactverification,
not a shippeddefault, and no FPSgain is qualified. Measure full envelope first.
Compiledclassifiertests passed for observedULPs, limits, UVs, signedzero, NaN/
infinity;21reader tests pass. Bothbuild pending in coop_mdr_roundoff_build.log.

Both roundoffbuild85281exit0, SP19:48:22/MP19:48:59. Frozen
coop_mdr_roundoff_candidate SP3af8546b9b8ea0a959e906e370f385518c641fa5c53ccde03c421563c21fde68,
MP397d17b0513f73e72d91c67615de7c02f877280f7295fb178da956bc3c337af4.
Current270sec numericalaudit coop_mdr_roundoff_verify, originaloutputretained;
one nativecapture requested at180. No performanceacceptance for this diagnostic.
No build active, no newproductionedits untilterminal/restored/archive.

Host-only SIMD experiment (no gamecode changes during audit):
build/research/letterbox_perf/prototype_coop_skin_simd.py extracts actual original
skin loop and Xbox inline DotProduct, compares a column-wise SSE matrix kernel.
128000randomized vertices (1-6weights,16bones), UVs and padding matched bits.
Use cleanXDK5558xmmintrin.h; VS2005header generated intrinsic-return warnings,
so rerun with compiler-matched intrinsic header. This is arithmetic evidence,
not runtime/FPSgain. Candidate could reduce repeated DotProduct shuffles/loads;
must beactualco-op-only, originalfallback, Xboxverify, thenmatchedtiming if adopted.
Current roundoffaudit stillrunning; no sourcechanges untilarchive.

Numerical audit79226/GPU77567 finishedexit0, live/error-clear/restored.
Gameplay compared7546653:7546373exact +280roundoff;0excess/UV/nonfinite.
Whole run8283912comparisons,298roundoff; maxposition8.58306884765625e-6,
maxnormal2.384185791015625e-7, maxULP2048(cancellation/nearzero makes ULP alone
misleading). Declared budgets1e-4/1e-6passed; NOTbit-exactequivalence. Cache334832B,
35.21%gameplayhits,0allocfails. P1/P2fire100/78, weapons1/2both; minhealth72/2.
External3D4.727mean4.847max,invalid10limitscompleteness. Diagnostic22.405mean,
19.632worst notgain. Native19-54-06capture individuallyinspected.
Now samefrozenpair enabledtiming coop_mdr_roundoff_speed probe10559/GPU86931,
270seconds,no screenshots; OFFinvocationready. No sourceedits/build duringpair.
Archive_coop_mdr_roundoff.py prepared;requires allthree restoredtransactions.
Hostprototype rerun withcleanXDKintrinsicheaderpassed128000exactvertices.
COFF/capstone staticinspection: original184instructions/704bytes, proposed123/
434bytes,neithercalls. Staticcounts are NOTcycles/FPS; noSIMDgameeditsyet.

## Skeletal cache timing checkpoint — both controls complete

The same frozen pair completed both timing runs with a live endpoint, no engine
error, and restored ISO. Probe10559/GPU86931 and probe1598/GPU86784 all exited0.
No emulator or GPU recorder from this pair remains active.

| Native XEMU measurement | Cache enabled | Cache disabled |
| --- | ---: | ---: |
| Confirmed-gameplay average | 27.306 FPS | 25.822 FPS |
| Slowest observed window | 23.129 FPS over9.512s | 20.257 FPS over9.676s |
| Complete10s conservative lower bound | 22.0 FPS | 19.6 FPS |
| External3D mean / maximum | 4.686 /4.931 | 4.876 /5.072 |
| GPU unavailable / invalid counters | 0 /0 | 0 /0 |

Enabled coop_mdr_roundoff_speed_20260911_195747 reused7,995,681vertices,
226,130hits in591,633requests (38.22%), with334832allocatedbytes,
98capacity fallbacks and0allocation failures. Mode1 does not recompute originals;
its zero mismatch counter is not verification. The preceding original-output
numerical audit establishes only the documented absolute error budget.

Disabled coop_mdr_roundoff_off_20260911_200540 had zero cache activity/allocation.
Both players moved in114of116enabled observations and117of120disabled observations.
Observed primary-fire events P1/P2:114/83enabled versus117/104disabled; both used
weapons1and2 in both trials. These are sampled combat observations, not all events.
Other GPU users were left alone, including xml-dx8-worker.

Observed differences are +1.484FPS average and +2.872FPS slowest window, but the
worst windows are not equal workloads: median MDR surfaces266enabled versus370OFF,
vertices10042versus13786.5, skin cycles3.735Mversus6.185M. Different enemy positions
and combat limit attribution of the full FPS difference to the cache. Serial-joined
kernel counters still identify skinning as substantial work; independent timing
medians must not be added or treated as host GPU timings. The30FPS goal is unmet.

Archived239files, including numerical audit, enabled/OFF evidence, and exact
source snapshots, under notes/evidence/coop_20260911/mdr_roundoff/manifest.json;
archive2483exit0. No production edits or default promotion occurred during the pair.
No SIMD kernel has been integrated yet. Its host prototype remains the next
arithmetic candidate, requiring co-op-only guards, original-output Xbox comparison,
and a fresh matched timing pair before any performance claim.

User cache-drive question: clean XDK5558 declares XMountUtilityDrive; inherited
source has Z: checkpoint/movie uses. A utility-drive asset cache is feasible for
loading or repeated decompression. No disk cache was implemented, and current
skin/render measurements do not establish disk I/O as their bottleneck.

## Co-op SIMD skin kernel — diagnostic candidate

Previous goal turn made progress: completed/compared the numerical-cache trials
and retained their evidence. The next candidate replaces six repeated scalar
DotProduct calls per vertex weight with simultaneous SSE xyz arithmetic. Same
weight accumulation order, full original meshes/materials/UVs, no new allocation.
Implemented only in non-MP Xbox SP code, additionally guarded by actual co-op,
two-plus players and split-view rendering. r_efCoopMdrSimd defaults0;1fast,
2diagnostic original-output comparison. Excess beyond existing1e-4position/
1e-6normal budgets, UV/nonfinite differences latch it off. Small roundoff is
counted separately from exact matches. Mode2 renders original vertices always.
The CPU reuse cache is disabled in the isolated kernel verifier.

Actual production-source host test passed128000 randomized vertices, zero-to-six
weights, unaligned matrix loads, UVs and untouched padding. Mode guards/default0/
failure latch tests also pass;22flight-recorder tests and numerical-budget tests
pass. These are not Xbox runtime or FPS evidence.

Fresh bothbuild50178exit0, runtime SP20:18:17/MP20:18:54. SIMD symbol present only
in default.map, absent from efmp.map. Frozen coop_mdr_simd_candidate:
SP8676ffc5da09b7f6b2ade6470ba337abd4b1a69975a8c8731c7aa43139f8bd91;
MPc7ff1abd7af0854aa11c2ae8163b9352f137f0c82af7181ece3030ba2dd9407d.
Verifier coop_mdr_simd_verify active: probe45010, GPU58362,270seconds,
one native capture. No build or production edits during this trial. Speed/OFF
invocations prepared with reuse cache1 to test the incremental kernel gain.
Wait actual terminal/restored and inspect fidelity before timing. Goal unmet.

SIMD verifier45010/GPU58362 both exited0, endpoint live/error-clear, ISO restored.
Whole run18,949,392comparisons, ALL exact; confirmed gameplay16,372,585comparisons
across437681surfaces, ALL exact. Zero roundoff, excessive, UV or nonfinite
differences; maximum error0. Although the diagnostic permits the existing tiny
roundoff budget, this run did not use it. Original output retained throughout.
Native capture20-23-45 individually inspected: both co-op views in combat.
Both players moved (97/99of100observations), primary fire95/66, weapons1and2both.
Diagnostic native19.605mean/12.728worst is NOT fast-kernel performance.

External3D during verifier12.139mean/13.554max,0invalid/unavailable; a second
XEMU PID30352 overlapped and subsequently exited. Fresh preflight73011exit0
showed about2.88-3.92%3D (csrss/dwm), but a new external XEMU PID34456 then
appeared, from C:/Programming/GitHub/OpenJKDF2ogx/build/xbox/issue1_validation/instance.
No other process was controlled or stopped. Timing with this overlap is
diagnostic and cannot count as a quiet-host30FPS qualification.

Fast SIMD timing now active: coop_mdr_simd_speed, probe90082/GPU45115,
270seconds, no screenshots; same frozen pair, cache1/particle1/shadow1/world2/
arrays3/hybrid1, modelresident0/PVS0, P2input3. OFF invocation prepared with
only SIMD0 changed. No build active. Before OFF, wait actual probe terminal,
restored transaction and GPU sidecar terminal; analyze native lows, combat,
SIMD/cache activity and host contention. Then archive_coop_mdr_simd.py after
all three trials/analyses complete, before further production changes. No
default promotion or30FPS claim. This goal turn made concrete progress by
implementing and runtime-checking the kernel; the performance goal remains active.

## SIMD timing pair complete — no FPS gain demonstrated

Fast probe90082/GPU45115 and OFF probe27283/GPU70918 all exited0. Both games
were live and error-clear at the endpoint, and both ISO transactions restored.
All analyses completed. Archive98980 exited0, retaining248files under
notes/evidence/coop_20260911/mdr_simd/manifest.json before further production work.
No owned test, build, or sidecar is active at this checkpoint.

| Native XEMU measurement | SIMD enabled | SIMD disabled |
| --- | ---: | ---: |
| Confirmed-gameplay average | 21.792 FPS | 24.763 FPS |
| Slowest observed window | 13.613 FPS over9.476s | 20.333 FPS over9.738s |
| Complete10s conservative lower bound | 12.9 FPS | 19.8 FPS |
| External3D mean / maximum | 10.647 /12.368 | 5.817 /12.618 |
| GPU unavailable / invalid counters | 0 /0 | 0 /10 |

These results do not demonstrate an FPS gain. The external OpenJKDF2 emulator
cycled through multiple runs during the pair; do not blame or credit the SIMD
kernel for the full difference. Both P1/P2 moved and used weapons1and2. Observed
primary-fire events were104/89enabled versus108/87OFF. P1 minimum health80versus29
also shows different combat. Preserve the distinction between exact arithmetic
verification and performance acceptance. r_efCoopMdrSimd remains default0.

Fast path processed13,131,887gameplay vertices. Reuse-cache hit rates30.82%enabled
versus30.23%OFF, allocation334832bytes, no allocation failure in either run.
Worst-window median MDR vertices8429enabled versus10668.5OFF, skin cycles3.889M
versus4.996M. Other-gameplay medians8467versus8521vertices, skin cycles3.669Mversus
4.013M. These are descriptive medians under differing host/scene conditions,
not an isolated instruction-cost or FPS experiment. Current frozen Xbox SIMD
function disassembly contains94instructions/341bytes, no calls,22stack operands;
static instruction counts do not establish runtime gain.

## Next renderer lead: repeated light-state updates

Read-only review of the slow-window model tables confirms repeated map props
remain substantial draw work alongside Borg characters. PAK0 MD3s blite/tank/
plugin/plugin2 have one animation frame. Shader/mesh summaries are retained in
coop_static_prop_mesh_review.json and coop_static_prop_shader_review.json.
Several busy materials are single-pass diffuse; other surfaces have animated
or multiple blended passes and cannot be indiscriminately combined.

Important Xbox-specific distinction: active retail_xbox/tr_shade_calc_retail.cpp
RB_CalcDiffuseColor(DWORD*) enables hardware lighting, submits ambient/diffuse/
direction through three qglLightfv calls, then fills white vertex colors. The
non-Xbox overload computes CPU colors; copying that approach would change the
Xbox path and requires independent fidelity work. Static batching is therefore
not yet implemented; per-entity light state and world transforms must be honored.

More direct candidate: each of those three calls reaches dllLightfv in
code/win32/win_qgl_dx8.cpp, which calls SetLight, LightEnable(0,true), and
LightEnable(1,false) every time. No draw occurs between the three calls. Source
search across code finds these as the only direct SetLight/LightEnable sites.
A co-op-only helper can submit the completed directional-light state once,
preserving hardware lighting and the existing enable state. First verify final
device light data/enables against the three-call original (retain original on
failure), instrument call counts/cost, then run a fresh controlled comparison.
No light optimization has been coded yet; no FPS gain is claimed. Avoid a
persistent light-state cache initially unless all invalidation paths are proven.

The older conservative per-view PVS candidate remains default0. Its audited
bounds are substantial correctness evidence, but its old FPS comparison was
confounded. It can be tested with the newer stack after the current root-cause
work; do not mistake its historical1.16FPS descriptive difference for a proven
current gain. The30FPS goal remains unmet and active.

## Combined diffuse-light update candidate

Previous goal turn made progress by completing and retaining the SIMD timing
pair and identifying the actual Xbox hardware-light path. New code now uses
r_efCoopLight (default0):1submits a completed light once;2compares original and
combined CPU/device light data and enable flags, then restores original state;
3runs the original three updates with the same timing hook for a control.
Only Xbox SP compilation, actual co-op with2+players, and a split3D view qualify.
Ordinary SP/MP retain their original calls. Hardware lighting remains enabled;
white vertex colors, transforms, shaders and scene geometry are unchanged.

The candidate performs1SetLight/2LightEnable calls per diffuse setup instead of
3SetLight/6LightEnable calls. It does not cache state across draws or frames.
Zero-direction normalization and untouched light fields are preserved. A device
state mismatch or API failure latches the candidate off and retains/restores the
original path. Diagnostic counters publish requests/calls, comparisons/errors,
and sampled light-setup cycles (verification timing is not FPS evidence).

Actual source host test passed90000 state comparisons, co-op/default/view guards,
zero direction, unused fields, enable flags, mismatch restoration and one-shot
API failure fallback.23flight-recorder tests pass. Host evidence is recorded in
coop_light_host_test_result.json with source hashes; no runtime/FPS inference.

Both build60121exit0; runtime SP20:53:36/MP20:54:17. New helper/counter symbols
present in default.map and absent in efmp.map. Frozen coop_light_candidate:
SP4777b9f79959dfd2c4e608a768f8b15fd2ecc8e48debfb71e13777f41e0f23f1;
MPdc6c3da516f59296d1cf87788e4bfcf1966c8c6035c7da2b1343cfc6f2489569.
Verifier coop_light_verify active: probe43915/GPU33341,270seconds, one native
capture. Initial44comparisons passed before gameplay. Cache1/particle1/shadow1/
world2/arrays3/hybrid1; SIMD0/modelresident0/PVS0; P2input3. No build active.
Speed1/OFF3 invocations and archive_coop_light.py prepared. Wait actual terminal,
restored ISO, and verify device-state/combat results before timing. No production
edits until candidate evidence retained. No default promotion or30FPS claim.

Further read-only state search finds no ApplyStateBlock/CaptureStateBlock/
BeginStateBlock in code/win32 or code/renderer; device Reset exists at
win_qgl_dx8.cpp:3900. This alone does not justify persistent state caching.


## Borg visibility and workload evidence correction

User reports not seeing Borg during gameplay. Read-only inspection of
coop_light_verify_20260911_205545.flight.jsonl finds Borg character submissions
in379of400 model records between the first and last camera-inactive player
observations (131.965 through269.562host seconds). These records are sparse,
asynchronous and can repeat a published sample; they are not379independent
frames or a count of Borg. They include borgThin and borgbig character assets,
not merely Borg wall textures. The native screenshot at20:59:24 shows P1 facing
a wall and P2 firing amid overlapping beams/effects; it does not clearly establish
live hostile Borg engagement. No enemy-removal/AI-disable command appears in
this trial invocation. Existing player-event proof cannot distinguish shooting
walls, friendly fire, idle drones, corpses or attacking Borg.

Do not claim sustained enemy combat from these trials. Next workload validation
needs a compiled-layout runtime NPC census with health, target/activity and
actual engagement, correlated with each player's position and native captures.
Preserve the real map scripts; do not manufacture qualification by bypassing
ICARUS or spawning artificial enemies. These renderer diagnostics retain their
limited diagnostic value, but do not qualify the full combat FPS goal.

Lighting verifier is now terminal: probe43915/GPU33341 exited0, transaction
restored:true. It FAILED after85395comparisons with1mismatch,0API failures and
the failure latch set. Original state restored. Do not launch speed/control
lighting trials for this candidate. Exact differing fields remain unknown;
archive failed evidence and add first-failure detail before retrying.


## Read-only NPC census and audible test correction

Previous goal turn made progress: it identified that player firing proof did not
prove enemy engagement. Added opt-in STEFX_NPC_CENSUS=1 to the flight recorder;
default runs do not pay for this census. Source ABI compiled with clean Xbox5558
from actual EF SP headers, including1056byte entities and612byte NPC records.
Seven reader tests cover live/dead/idle actors, non-Borg teams, invalid/short
reads, nonfinite positions and entity/event replacement.23flight tests passed.
No game binary or AI/script changes were required.

coop_npc_census completed: probe3976 andGPU25922 terminal0; ISO restored:true.
31census observations during confirmed gameplay found15�19living Borg, with
0�4having targets. Nearby actors202/214/311 were seen pursuing players and then
dying;207 disappeared after an observation at3health (death not directly captured).
Targets included bothP1(entity0) andP2(entity183). Two actual Borg firing events
were observed; many events are missed at3second intervals, and melee is not
fully reconstructed. Late loop ends with15living but untargeted Borg elsewhere.
All4native screenshots inspected individually. Source/capture evidence archived
in notes/evidence/coop_20260911/npc_census (71files).

This supports the user's explanation that the nearby room was cleared before
they watched. The circular input route does not maintain combat: do not use its
late cleared-room windows to qualify sustained enemy workload. Next fixture
needs to follow real campaign progression into subsequent encounters, keeping
ICARUS authoritative. Recorded23.18mean/20.47slowest native FPS are diagnostic
only; census/captures add overhead and full-workload qualification is false.

User raised silent output as an observability problem. Found [audio]
volume_limit=0 in the task's XEMU config, with use_dsp=true. Pinned fc9980d
monitor.c applies pow(clamp(volume_limit,0,1),e) as SDL stream gain;0mutes
output, without establishing anything about sound correctness. After NPC
probe/sidecar terminal, changed this config to0.65 (65%slider); retained old
muted config with NPC evidence. Preserve audible output in future visible
tests. This is not authorization to resume muddy3D-voice tuning.

Current short audible-output probe74425 uses frozenlightcandidate with light0,
original pinned XEMU, no NPC census,240seconds. Capture47858 attaches only
the identified XEMU PID35244, copies its APU S16LE stereo48000Hz frames at
SDL handoff and leaves playback enabled. Hook matched source and exact binary
hash/instructions; output metadata distinguishes source samples/SDL acceptance
from speaker delivery and subjective clarity. This instrumentation invalidates
FPS qualification. Wait terminal/restored before another probe or cleanup.


## User acceptance and final teardown

User directed: "Wrap this up. We're going to accept cooperative as-is."
The co-op pass is CLOSED BY USER ACCEPTANCE, not a30FPS pass. No further
co-op work or automatic tests are authorized absent a new request. Former
checklist moved to coop_accepted_as_is_2026-09-11.md; open tracker updated.

Audible probe74425 and capture47858 both terminal0; ISO restored:true.
Captured44622APU blocks, all accepted atSDL handoff, zero hook errors.
Non-silent capture retained under notes/evidence/coop_20260911/audible_output.
Task config volume_limit=0.65 remains; no game audio tuning or production
default promotion. Muted output never proved audio quality.

No goal cancellation tool is available: exposed operations only create/get/
complete/blocked. The numeric app goal is unachieved, and is not falsely marked
complete or blocked. This stop instruction supersedes any automatic continuation;
user may remove the stale goal in the app. Do not resume work from its prompt.
