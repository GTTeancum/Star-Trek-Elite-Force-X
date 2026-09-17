# Letterbox and optimization goal — 2026-09-09

Status: in progress. No performance target has been declared achieved.

User benchmark correction: exclude borg1's scrolling-text wall view, including
sample windows that overlap it. The initial 67.0 FPS average / 55.5 minimum
from `letterbox_perf_baseline_borg1_20260909_142743` includes the opening and is
not a valid gameplay acceptance baseline. A runtime-evidenced cutoff is required.
This rule is recorded in the repository's `AGENTS.md` for future sessions.

## Acceptance

- Fix the reported borg1 letterbox persistence and right-edge gap without
  changing authored ICARUS camera sequencing.
- Target 45+ FPS across SP levels, and 30+ FPS in co-op and four-player split
  screen. Report slow samples as well as averages; a fast average must not hide
  sustained drops below the target.
- Improve startup, level/save loading, and stutter where measured bottlenecks
  support a change. Keep visual fidelity close to the current build.
- Keep audio work paused. Preserve the user-confirmed opening crash fixes.
- XEMU supplies programmatic regression evidence; retail Xbox remains the final
  performance authority. Direct map starts are coverage tests, not proof of
  complete campaign traversal.

## Baseline and current changes

The starting SP XBE is SHA-256
`18837272e0e967edf2117cce0971a34d2fb6f7de46eec2d4f897ac943a209e98`.
Bounded baseline copies and tests are under `build/research/letterbox_perf`.
Earlier work remains uncommitted; this goal must not revert those changes.

The active SP camera is `SP-Mod-Source-Code-master/cgame/cg_camera.cpp`,
selected by the build script. The inherited `code/cgame` camera is not the
active EF implementation.

EF's overlay helpers emit framebuffer coordinates. The active retail backend
used a 720-unit projection during widescreen gameplay, compressing 640-pixel
overlays to eight ninths of the display. The candidate uses framebuffer bounds
for active EF overlays, retaining the existing frontend coordinate system.

The candidate advances camera-bar fades before reading alpha/height in the
draw function. New transition-only logging records camera ownership, visible
bars, fade target, and rectangle. This does not force camera shutdown or claim
the reported persistence cause has been established.

`scripts/tests/verify_camera_overlay.py` compiles the actual camera and backend
functions. Its 39 checks pass for fade progression/shutdown, full overlay
coverage, split rectangles, widescreen and normal aspect flags, and frontend
projection. The original functions fail 22 of those checks as a negative control.

## Runtime evidence

`letterbox_visual_aligned_20260909_143914` ran the authored borg1 opening
without input. Four native XEMU captures were individually inspected: the first
shows full-width cinematic bars; the subsequent three show first-person gameplay
with no bars. Final camera memory also has bar alpha, target alpha, and height
all zero. This is emulator evidence; retail acceptance remains outstanding.
The screenshots are retained under `build/research/letterbox_perf/screenshots`.
This visual run is not a timing benchmark.

The runtime FPS classifier excludes complete windows containing camera/text,
menu/loading, or map changes. Native tests cover eight context transitions;
the log analyzer test confirms excluded high-FPS intro samples cannot re-enter
through a legacy-summary fallback.

Two 260-second borg1 runs used the same XBE, SHA-256
`8afe07f785767325f7469a1bba82d6cadb930d370f6141bae6f9c9e397f83c01`,
without screenshots or live polling. Each retained 17 clean gameplay windows:

| Probe | Average FPS | Lowest sample | Total map load |
|---|---:|---:|---:|
| `perf_borg1_cache_off` | 60.36 | 56.1 | 22.110 s |
| `perf_borg1_cache_on` | 61.01 | 57.3 | 22.519 s |

The candidate skips repeated identical vertex shader and stream stride calls.
Its retained counters confirm approximately 65% and 64% fewer calls respectively.
The small FPS difference does not establish a reliable benefit. After these
trials, the candidate switch, extra SP counters, and its test were removed;
the original three/four-player behavior is preserved. Ten native wrapper checks
had covered invalidation, failed calls, switch-off, and the existing split-screen
policy. No visual-quality setting changed. Historical probe snapshots retain
the candidate for reproducibility; the next shipping build must omit it.

These are stationary opening gameplay windows, not all-level traversal or
retail performance qualification. The combined goal remains incomplete.

The coarse CM_LoadMap call accounts for 21.255 seconds of the off run's load.
It also loads lightmaps, shaders, and render geometry, so this is not evidence
that collision calculations alone are slow. A retained nine-word phase array
now separates those stages and reports completion. The instrumented Release
build passed. The co-op probe showed the raw-path array remained incomplete:
this package uses packed map lumps. A separate packed-path array now measures
setup, lightmaps, patches, triangles, faces, flares, collision lumps, and finish.

## Co-op failure and current candidate

`perf_borg1_coop_load_20260909_150340` loaded borg1 with players=2, but its
only retained FPS window ended at serverTime=1500 and was correctly excluded.
The final camera remained near the cinematic start after 260 host seconds.
The emulator process was alive, with the sampled CPU halted in kernel idle;
that alone does not identify the blocked game thread or its cause. No valid
gameplay FPS can be reported. This is a failed co-op progression probe, not a
performance pass. The XBE hash is
`f3b3fe3112a46f08313cce76fd81403fcdfbfd690e2cd45e9e50e8bc3845a15c`.
Further diagnosis must use retained thread/engine state. Audio remains paused.

The probe runner now requires observed player count plus valid P2 camera and
positive draw counts for both views before declaring a co-op map-window pass.
Fourteen tests reject missing view evidence or mismatched player counts.
Four-player passes require positive draw counts for all four views and matched
completion counters (within one view of each other), in addition to players=4
and clean gameplay FPS. The active retail renderer writes these counters after
each R_RenderView; controller qualification remains separate.

Correction: the proposed shader-order change targeted inactive
`code/renderer/tr_shader.cpp`. The active `retail_xbox/tr_shader_retail.cpp`
already checks the cache before text lookup. The inactive edit and its test
were removed. Do not credit this proposal with any runtime change or improvement.
The absence of its exported counters in the linked map revealed the mistake.
Verify active translation units in the actual linker map before further edits.

Despite its misleading name, `perf_borg1_shader_order_20260909_151141` is a
valid packed-load baseline, not a shader optimization trial. XBE SHA-256:
`79e689f67792c43a3dd9b5ec00a65609e0c336c9821d916cf8ee75c6acc67e82`.
It retained 17 gameplay windows averaging 60.38 FPS, minimum 57.2. Total map
load was 21.880 seconds. Packed phases measured 13.328 seconds in patches and
7.575 seconds in faces; other phases totaled only 185 milliseconds.

The active `LumpStream` previously used absolute ZIP seeks for forward gaps,
forcing entry reopening and rereading from byte zero. The candidate uses a
relative forward seek and retains absolute backward seeks. Eight native tests
verify returned bytes, partial reads, backward seeks, errors, and bounds. A
per-stream close log reports relative forward seek counts. Build/measurement
remain in progress; no load-time improvement is yet established.

An offline read of the actual packed borg1 records estimates one complete
vertex-read pass over 244 patches would fall from 50,378,956 bytes read/replayed
to 4,761,360 bytes. For 12,906 faces, it falls from 54,636,708 to 4,770,472 bytes.
These are simulated I/O work totals, not measured load times; collision loading
may skip some records. Evidence: `packed_seek_work_estimate.json` in the bounded
research directory. The shared-header change triggers a full Release rebuild.

Runtime result: `perf_borg1_stream_seek_20260909_152213`, XBE SHA-256
`43aaf8aec14ab72449681c271e6923252efc5ef1821710b9c4b579e8ff29265c`, completed
the authored opening and retained 19 clean gameplay windows (62.06 FPS average,
58.5 minimum). Measured map load fell from 21.880 seconds to 4.346 seconds
(80.1% shorter). Packed patch loading fell from 13.328 to 1.881 seconds and
faces from 7.575 to 1.457 seconds. No geometry, materials, or scripts changed.
This is a demonstrated XEMU load-time improvement, not retail acceptance or
whole-campaign FPS qualification. Co-op diagnostics and Holomatch build follow.

Co-op spawn diagnostics retain a stage counter and at most 24 log lines:
1 before placement, 2 after placement, 3 after spawner allocation, 4 before
NPC_Spawn, 5 after NPC_Spawn, 6 missing player two, 7 found player two, 8 fully
placed. The diagnostic probe mode records engine state and cannot emit a timing
qualification pass. No authored behavior is changed by this instrumentation.

The rebuilt Holomatch executable includes the load change and FPS classifier.
Its stable snapshot is `build/research/letterbox_perf/perf_efmp.xbe`; the clean
SP candidate is `current_default.xbe` in the same directory. These omit the
unproven graphics-state cache extension. Neither is hardware-qualified yet.

The 100-second co-op diagnostic reached spawn stage 8 and continued advancing
frame and server-time counters through the opening. At the endpoint the camera
was still active, so the lack of P2 draw counts is not a gameplay failure. The
earlier stall did not reproduce; do not claim its cause is established or the
whole co-op mode is fixed. A longer gameplay probe is next.

## Co-op model-memory failure identified

The longer `perf_coop_current_20260909_153446` stalled at startup again. A retained
final RAM snapshot was translated using its CR3 page tables and checked against
three independent native memsave outputs (map name, load timings, P2 refdef).
The active renderer counters show one completed view per player, not gameplay
progression. The previous co-op gate used inactive renderer counters; it now
reads the active retail renderer's shared two/four-view arrays.

Recovered log evidence is `perf_coop_current.recovered_log.txt`. It records
`EFALLOC_FATAL: request=302517 tag=12 zone=21319648 used=20828881 free=189126
largest=151600`. Tag 12 is MODEL_MD3 (also used for MDR files); the retained model
probe names `models/players/borgThin4/lower.mdr`. The game then entered disconnect
cleanup. This establishes model-memory exhaustion, not a diagnosis of audio.

The existing lossless Borg frame compactor allocated only from the external
heap. Under pressure it could fall back to larger full-resident model copies.
The candidate permits compact frames/geometry to use zone memory when the heap
cannot supply it, records ownership for correct freeing, and releases a newly
created frame base if its first compact model cannot be allocated. A new
optional allocator holds the recursive zone mutex across fit-check/allocation.
Nine native allocation/ownership tests pass, including failure and cleanup.
Release build passed and the linker map confirms the active helpers. Probe
`perf_coop_model_storage` is running; runtime success remains unproven.

`perf_four_current_20260909_154050` reported players=4 but all four active view
counters were zero. Its 83.94 FPS average is NOT a four-player performance pass.
The four-player launch/render evidence needs diagnosis; do not count it toward
the 30 FPS target. It loaded hm_borg1 in 1.251 seconds on XEMU.

The older candidate ledger documents XEMU's inability to rank GPU synchronization
costs. Current emulator measurements must not be promoted to retail FPS claims.

## Compact model storage runtime result

`perf_coop_model_storage_20260909_155021`, XBE SHA-256
`ffa6fd188613f738d5573fa5a917e9ed51e5f33af9afb49e4a2c56dc40328b23`,
completed the 260-second run and the natural intro. Both active renderer view
counters reached 2630, with 2072/2070 submitted surfaces at the endpoint and a
valid P2 refdef. This is the first valid two-view gameplay result for this
candidate; it does not establish long-session stability or retail acceptance.
Sixteen retained gameplay windows averaged 18.01 FPS (minimum 17.2), all below
30 FPS. Nineteen untagged or intro-overlapping windows were excluded. Map load
was 4.272 seconds. Client time around 45-52 ms dominates the retained gameplay
samples; a diagnostic build will separate frontend/backend/submission costs.
The ISO transaction restored successfully.

The next four-player probe creates four local human clients directly, replacing
the previous one-human/three-bot viewport setup. Qualification requires all four
active renderer draw/completion counters, not merely a players=4 cvar. This
uses process-local game clients and no host input. Timing remains unqualified
until the corrected mode evidence is collected.

The FPS mirror's 256-byte slots could truncate the trailing exclusion fields
as numeric counters grew. The parser rejected those truncated records, but this
lost later gameplay windows. Slots are now 512 bytes (16 KiB additional static
storage); `verify_fps_record_capacity.py` checks both actual format strings with
full-width counters and reproduces failure at the old size. Existing eight
context-window tests, intro-only filtering, and fourteen mode gates pass.
The next diagnostic build includes this telemetry correction. Earlier results
remain limited to their explicitly retained sample windows.

`perf_four_humans_20260909_155944` verifies four active views in hm_borg1:
draw surfaces 556/506/857/371 and completion counters 2398 in every slot.
Eight retained stationary gameplay windows averaged 57.675 FPS, minimum 50.9;
load was 1.196 seconds. XBE SHA-256 is
`d55df8a956e0a7734604653f90b081136c9c40e5f7ffa998f2b629e823ce7579`.
The corrected four-human setup passes the measured XEMU window gate. This is
not moving-match, other-map, controller, or retail acceptance. A separate
native screenshot run exercises process-local virtual movement for all four
clients; it is explicitly visual-only and may overlap compilation.

That visual attempt (`visual_four_moving_20260909_160542`) did not provide valid
game evidence: zero populated log records, short/empty memory dumps, and native
capture `screenshots/xemu-2026-09-09-16-07-58.png` showing a corrupted display.
No four-view or movement acceptance is claimed. The ISO restored successfully.
The probe previously threw on short dumps; it now treats those as absent
evidence, covered by six dump-size cases. Cause of this failed run is unproven.

The SP frame-diagnostics build passed (build time 16:06:58), with a stable copy
`coop_frame_diagnostics.xbe/map`. `diag_coop_render_cost` runs the natural borg1
opening in co-op for 230 seconds with engine polling and cannot claim a timing
qualification pass. This is the next source of rendering-cost evidence.

The diagnostic completed with two-view proof. Final populated frame records
show backend/end-frame around 33-39 ms and frontend 5-7 ms, about 960 batches
and 976-978 submissions, with no measured texture swapping or presentation
wait. Live polling showed some zero counters that disagree with these populated
records; use the final records for this diagnosis. Later FPS records retain the
exclusion suffix after counters exceed the old slot length.

The scratch-ring profiles hit the exact 262144-dword capacity every gameplay
frame and accrue roughly 160000 inline fallbacks per 256 frames. The opt-in
`r_efScratchRollover 1` candidate fences the full buffer, switches to the other
existing buffer, waits for its prior GPU readers, and resumes allocation. It
adds no buffers and changes no vertex payload or image settings. Default stays
0 pending paired qualification. Nine native tests exercise actual claim/fence
code, including oversized requests, disabled fallback, and repeated rollover
with pending readers. The active call site claims scratch before BeginPush,
so rollover never inserts a fence inside an open push reservation. A diagnostic
rebuild passed at 16:18:25, saved as `scratch_rollover.xbe/map`.
`diag_coop_scratch_rollover` is the 230-second natural-opening comparison with
rollover enabled. No performance improvement is yet established.

The rollover diagnostic finished with 15 retained gameplay windows averaging
18.51 FPS, minimum 16.2, versus 20 windows averaging 17.59/minimum 17.0 in the
preceding diagnostic. Median backend cost was 34 versus 36.5 ms. Inline
fallbacks became zero; one 5 ms fence wait was observed. This small diagnostic
gain and worse minimum do not qualify it for default enablement; it remains
opt-in and is not a 30 FPS solution. No retail claim is made.

XEMU exited but the ISO briefly remained unavailable for write restoration.
The pending transaction manifest was used to restore and verify the original
root descriptor, file entries, and 3340857344-byte length; proof was retained
and the summary regenerated. Restoration now retries sharing errors for up
to ten seconds and retains the pending manifest if that fails.

The existing per-shader cost instrumentation is expanded from Holomatch-only
to SP/co-op diagnostic builds. This changes no production rendering. Build
`coop_shader_cost_build.log` is underway. `visual_four_stationary` separately
retries native four-view visual evidence with the previously verified local
human setup; it overlaps compilation and is not a timing qualification run.

The shader-cost build passed at 16:27:07, SHA-256
`0e55ef91735b508ed982d612c3e503ca4b1faaf6d6838442478e0ec834edf11e`.
The active linker map confirms `R_STEFX_ReportShaderCosts` in
`tr_shade_retail.obj`. `diag_coop_shader_cost` is collecting the next profile.

The stationary four-player visual retry rendered four views for roughly 40
seconds, then returned to the main menu (native screenshot 16:29:03). Its
final draw counters are stale gameplay history; they do not prove the match
stayed active through the 120-second run. Earlier four-player FPS measurements
remain short-window evidence only. Their overall window-pass flags are revoked
pending an active endpoint check. Probe summaries now require final client
state CA_ACTIVE (7), with nine gate tests including disconnected/missing state.
The next launch captures the actual log mirror by the supplied linker map and
supports earlier native screenshots to expose the transition. Neither the ISO
active-command files nor its mini-soak marker were present on inspection;
the cause of menu return is still unproven.

`diag_four_menu_return_20260909_163924` establishes the failure: the actual
log mirror records `STEFX_HM_SP: Com_Error code=1 message='G_Spawn: no free
entities'` around server time 42683, followed by disconnect and frontend
initialization. Final client state is 1 (disconnected). Native captures at
16:40:23 and 16:40:31 show four views followed by the main menu. Botlib also
logs that it is being used before setup; causality is not yet established.
The active official G_RunFrame and G_Spawn translation units now log bounded
entity-lifetime summaries and exhaustion details. Limits/lifetimes are not
changed. `hm_entity_lifetime_build.log` is building the diagnostic breadcrumbs.

The SP/co-op texture-stage state cache candidate reuses the existing Holomatch
value cache with an opt-in `r_efTextureStageCache 1`. Default SP/co-op remains
0; Holomatch retains its existing enabled cache. Toggle changes invalidate
cached values, and disabled calls still reach D3D. Native state/toggle/failure/
bounds/invalidation tests pass. Its diagnostic build passed at 16:42:23 and
is retained as `texture_stage_cache.xbe/map`; runtime comparison is pending.
This is distinct from the previously rejected vertex-shader/stride cache.

While the Holomatch lifetime build recompiles shared code, the cache candidate
is undergoing a visual-only co-op run (`visual_coop_texture_cache`, 220 seconds,
cache=1). Its timings are not a comparison because compilation overlaps it.
The newly rebuilt SP production executable is separately retained as
`texture_stage_production.xbe/map` for a subsequent clean on/off comparison;
the opt-in caches remain disabled by default.

The Holomatch lifetime build passed and is retained as
`hm_entity_lifetime.xbe/map`. After the current visual-only co-op probe restores
the ISO, run this MP snapshot with four local humans and final log-mirror
capture to inspect `STEFX_HM_ENTITY_LIFETIME` and `STEFX_HM_ENTITY_FULL`.
No entity lifetime fix has been applied yet.

### Four-player simulation and clean co-op baseline

The retained `diag_hm_entity_lifetime` native log proves that four-player
simulation never advanced: incoming server time reached 41000 while game time
remained 1000, frame remained 0, and `level.restarted` remained 1. Temporary
events accumulated until all 1022 usable entity slots were occupied. Raising
entity limits would conceal the stopped simulation; it is not a fix.
Initialization phase breadcrumbs are being tested in `diag_hm_init_phase` to
locate where the restart flag becomes set. No restart-state workaround has
been applied.

The production cache-OFF co-op run `perf_coop_texture_off` completed with both
views active and client state CA_ACTIVE. Excluding 20 intro/non-gameplay
windows left 22 samples: mean 18.536 FPS, minimum 17.8 FPS, all below 30 FPS.
Map load was 4307 ms. Executable SHA-256:
`be00d89d532f8a46063a6c35dc5221bbfe585567d5b4a02dcdf09c97a2dd1994`.
The ISO transaction was restored and verified. This is XEMU evidence only;
the matching cache-ON comparison is still pending.

`diag_hm_init_phase` completed and its ISO transaction is restored. Native
log mirror records show `initRestart=0/0/0/0/0`, but later game frames still
report `restarted=1`, `time=1000`, `frame=0`. The unexpected state therefore
arises after G_InitGame returns. The next diagnostic adds a bounded VM-call
transition trace; this source change has not yet been built. A focused COFF
relocation check confirmed that g_main and g_client use offset 68 for
`level.restarted`, while g_session uses offset 64 for `level.newSession`;
that specific cross-object layout mismatch is not supported by evidence.

The matching cache-ON run (`perf_coop_texture_on`, 230 seconds) is running
against the identical production snapshot. Do not overlap a build with its
timed portion. No cache default has been changed.

### Submission experiments rejected

The matching cache-ON run completed: 16 retained gameplay windows, mean
17.431 FPS, minimum 16.1 FPS, versus OFF mean 18.536 / minimum 17.8. Both runs
retained two active views and CA_ACTIVE endpoints; both ISO transactions were
restored. Neither meets 30 FPS. This does not demonstrate a cache benefit.

Removed the unqualified SP/co-op texture-stage extension and the earlier
opt-in scratch-rollover experiment from win_qgl_dx8.cpp, restoring that file's
pre-experiment implementation (including existing Holomatch caches). Their
patch and helper checks are retained under build/research/letterbox_perf for
reproduction. Shipping code should not accumulate unsuccessful experiments.
The source cleanup needs a fresh build; prior executable snapshots still
contain the opt-in experiments. `diag_hm_restart_transition` uses its own
already-built snapshot, independently of this source cleanup.

### Four-player restart trigger localized

`diag_hm_restart_transition` completed/restored. Native log mirror proves
`transition=10/1000/0/1`: the first BOTAI_START_FRAME call changes the restart
flag from false to true, while simulation time/frame remain 1000/0.
G_InitGame initializes bot settings/library only when bot_enable is true;
the host adapter previously called BotAIStartFrame unconditionally. That
function updates unregistered VM cvars and calls BotInterbreeding, whose
nonempty bot_interbreedchar path requests tournament mode and ExitLevel.
This is the source-supported explanation of the observed restart transition.

The adapter now latches whether bot initialization was enabled for the map
and skips bot frames otherwise, logging STEFX_HM_BOT_FRAME_GATE. Enabled-bot
behavior remains on the existing path. The build is in progress in
hm_bot_frame_gate_build.log; runtime regression testing is still required.
The unsuccessful submission experiments have been removed and that cleanup
passed a Release build (submission_cleanup_build.log).

### Four-player failure fixed in the reproduced XEMU case

`diag_hm_bot_frame_gate` completed 100 seconds and restored its ISO transaction.
Native endpoint: CA_ACTIVE, four connected and four playing clients, all four
views completed frame serial 4018. Official simulation reached frame 1930,
time 97700 (start 1000), restarted=0. Lifetime logs show slots steady at 190
and no accumulating expired events, versus 1022 slots and frame 0 before the
fix. This supports the disabled-bot startup fix for the reproduced failure.
Executable SHA-256: `fb743de5540b43778cd0c2305bda2d80688342d4d83fef873a79cac9dc1d060c`.

This diagnostic is not a performance qualification. Enabled-bot regression,
longer/moving/combat runs, broader SP map coverage, co-op optimization, and
retail verification remain open. The probe now records the official level
prefix and rejects MP acceptance without advancing simulation and four playing
clients. Mode/endpoint qualification checks passed, including missing/false
simulation evidence rejection. All builds and emulator runs from this cycle
are finished; no ISO transaction remains pending.

### Bot-enabled regression

`diag_hm_bots_enabled` completed/restored using the same fixed executable,
with bot_enable=1 and bot_minplayers=6. Endpoint: six connected/playing
clients (four local humans plus two bots), CA_ACTIVE, all four view serials
3482, simulation frame 1997/time 101050, restarted=0. Slots stayed bounded at
207 in retained lifetime logs. The enabled-bot path therefore remains active
and survives this bounded regression test. This diagnostic run is not an
FPS acceptance test.

`perf_four_moving_fixed` is now a clean 180-second measurement with the
existing process-local movement/firing harness enabled for all four local
players. No host input or desktop automation is involved. Do not overlap a
build or another emulator with this timing run.

### First clean four-player window after simulation fix

`perf_four_moving_fixed` completed/restored: 27 retained gameplay windows,
mean 48.078 FPS, minimum 36.7 FPS; all above 30. Four connected/playing
clients, CA_ACTIVE, all four render serials 6649. Simulation frame 2738,
time 138100, restarted=0. This is a passing XEMU hm_borg1 window, not
all-map or retail acceptance. The process-local movement/firing cvars were
requested; endpoint command counters were not separately dumped, so the
retained evidence does not independently prove each commanded movement.

Corrected an MDR index-capacity reservation: RB_SurfaceAnim appends three
indexes per triangle, but reserved only one. The active overflow helper and
callsite passed four boundary cases plus an original-code negative control
that exceeds capacity. Release SP and Holomatch builds passed in
mdr_index_capacity_build.log. No geometry or material data changed.

A clean SP borg2 start is now running (`perf_sp_borg2_current`, 120 seconds)
against optimized_sp_current.xbe/map, refreshed after the index fix. The
broader SP map inventory remains mostly unmeasured. Do not overlap heavy
builds with this timing run.

The MP probe now also captures the existing per-client command serial,
forward/right movement, and button arrays. Syntax and all four symbols were
verified against the active MP map. This supplies direct command-path evidence
for subsequent movement tests; it does not retroactively add proof to earlier
runs or prove broad traversal by itself.

### Borg2 and a conservative culling candidate

`perf_sp_borg2_current` completed/restored with CA_ACTIVE and one rendered
player: 16 gameplay samples, mean 83.1875 FPS, minimum 73.8. One non-gameplay
window was excluded. This passes the measured map start, not traversal or
retail. The map coverage inventory now records this distinction.

Read-only `audit_mdr_bounds.py` checked all 201 effective campaign MDR assets,
87,836 animation frames, and every LOD. Worst AABB excess was 0.020309 units;
worst sphere excess was 0.025795 units. All frames fit a 0.05-unit tolerance.
The auditor's compressed-matrix decoder matched the actual compiled
MC_UnCompress byte-for-byte for 1,024 matrices, including boundary encodings.
Results: mdr_bounds_audit.json, mdr_bounds_decoder.txt.

A cvar-gated candidate replaces the generic 128-unit fallback sphere with
the union of the two animation-frame boxes plus a 0.125-unit margin. It only
runs after the existing culler rejects the model; the default remains the
existing behavior. No vertices, materials, poses, or script state change.
The Release build passed and is retained as mdr_bounds_candidate.xbe/map.
`perf_coop_mdr_bounds_on` is running a clean 230-second co-op measurement.
Default enablement, paired performance proof, and visual validation remain
pending. Do not overlap heavy work with this timing run.

### Tight MDR fallback did not demonstrate a useful co-op gain

The initial `perf_coop_mdr_bounds_on` run retained 21 gameplay samples, mean
17.981 FPS and minimum 17.1 FPS. It did not improve on the prior approximately
18.5 FPS baseline; this was not a same-executable paired regression claim.
The effect guards passed 18 native cases and built successfully, but were not
present in this measured snapshot. Neither variant is qualified for shipping.

Removed the culling candidate from runtime source, retaining only the proven
index-capacity fix in tr_animation.cpp. Archived the candidate patch and guard
test under build/research/letterbox_perf. The standalone asset auditor and
native decoder check remain usable. The cleanup Release build is running in
mdr_culling_cleanup_build.log; do not benchmark concurrently with it.

The culling cleanup Release build completed successfully for both default.xbe
and efmp.xbe. The co-op culling test ended CA_ACTIVE with two rendered views
and its ISO transaction restored. No emulator or build remains running from
this cycle. The runtime culling behavior is back to its original form; the
MDR index-capacity correction remains. Next optimization work should measure
actual MDR skinning versus submission cost rather than promote the rejected
fallback change.

### MDR skinning profile: repeated work is material

The new diagnostic-only instrumentation separates index copying, bone-palette
preparation, and CPU skinning. It counts exact repeated surface/frame/oldframe/
backlerp requests within the sampled renderer frame, using 256 observation keys
and reporting overflow explicitly. Production paths compile these counters out.
The extractor accepts only complete numeric STEFX_HW_MDR_SKIN records.

`diag_coop_mdr_skin` completed/restored with two active views and CA_ACTIVE.
Retained gameplay samples 34-39 each request 548 surfaces / 20,142 vertices;
207 unique keys, zero observation overflow, 12,162 repeated vertices (60.4%).
That leaves 7,980 unique vertices, or 255,360 bytes if cached as position,
normal, and original UV (8 floats). Sample 39 measured 291,226 index-copy
cycles, 575,656 palette cycles, 8,412,876 skin cycles, and 16,022,754 shader
submission cycles. Sample 38 skin cost was 9,071,617 versus 14,012,316 shader
cycles. These are diagnostic cycle measurements, not FPS qualification.

The asset weight inventory also found 22,749 unit single-weight vertices out
of 59,382 LOD0 vertices across effective campaign models. The repeated-pose
work is the stronger first candidate; do not change LODs or geometry detail.

Next candidate design (not yet implemented): an optional heap-backed pose
cache, roughly 256 KiB of vertex data plus bounded hash metadata, keyed by
surface pointer and exact frame/oldframe/backlerp. Cache raw skinned positions,
normals and original UVs before shader deformation/lighting. Hits must copy
into fresh tess arrays so per-entity effects remain independent. Index copies
and entity/world transforms remain on the original path. Clear keys each
renderer frame, and release/invalidate before model storage is freed (existing
STEFX_ClearMdrFrameCache lifecycle is a candidate hook). Allocation failure
must fall back normally, never become a new fatal allocation. Validate exact
output, capacity limits, changing poses, model invalidation, memory pressure,
and paired runtime performance before enabling by default.

Campaign preflight now rejects ef_sp_smoke_harness.txt rather than allowing
script-override smoke modes into SP/co-op qualification. Actual XDVDFS fixtures
passed marker-present, marker-absent, and malformed-image checks. The current
shared ISO was verified marker-free. Existing normal in-engine +forward/+left
commands can support changing-view SP probes without enabling that marker;
existing g_SPXBUsercmd* telemetry can prove command delivery.

The production restoration build is running in
mdr_skin_profile_production_restore.log (Target spmp, no frame diagnostics).
No emulator remains running. Do not start timing measurements while the build
is active. The tool returned build session 33769; revalidate that handle before
continuing or launching another build.

### Optional MDR pose cache implemented; runtime proof pending

The candidate caches raw local positions, normals and UVs keyed by exact
surface/frame/oldframe/backlerp bits and vertex count. Keys reset each renderer
frame; model-storage cleanup releases the cache. Complete poses are explicitly
committed before becoming hits. Every hit copies into fresh tess arrays, and
index generation, entity transforms, lighting, and shader deformation remain
on their existing paths. Capacity is fixed at 8,192 vertices / 512 hash slots,
276,488 bytes including metadata. The cvar r_efMdrPoseCache defaults to zero.
Heap allocation failure falls back to original rendering without retries each
frame; disabling/re-enabling or model lifecycle reset permits a later retry.

Native tests passed: 534 cache/capacity/exact-copy checks, 13 runtime allocation
and lifecycle checks, and 13 full RB_SurfaceAnim output comparisons covering
weighted vertices, compressed and uncompressed frames, interpolation, changed
poses, different entities/base offsets, later shader mutation and allocation
failure. Full tess output matched the original rendering path byte-for-byte.
The Release build passed (mdr_pose_cache_build.log) and active map symbols
confirm the new functions are linked from tr_animation.obj.

The current clean probe is perf_coop_pose_cache_on (230 seconds), using
mdr_pose_candidate.xbe/map. Runtime allocation success, hit rate, memory,
paired performance and visual checks remain unproven. Do not enable by default
or overlap heavy work with this timing run. The previous production-restore
build finished successfully before the candidate was integrated.
### Pose cache first runtime result and paired control pending

perf_coop_pose_cache_on completed and restored its ISO. Same candidate SHA256
666cb27e0f387ae0a3ba296540e0f7c75e362f63be85cff0fcdc9c35e7454c4e.
Fourteen retained gameplay windows averaged 21.079 FPS, minimum 18.0; all
remain below 30. Twenty intro/non-gameplay windows were excluded. Final
CA_ACTIVE, borg1, and two completed views confirm continued co-op operation.
Cache runtime stats show successful reuse, no capacity fallback, and up to
7,991 vertices retained in observed frames. At renderer frame 7732 cumulative
hitVerts=18,718,182 and computedVerts=13,523,075. This establishes operation,
not a causal performance improvement or retail acceptance.

User supplied an XEMU video-debug screenshot showing FPS 10 and MSPF 88 in
2P co-op. Preserve this as host-visible slowness evidence; guest FPS averages
must not be described as the user's delivered XEMU FPS or retail performance.
The exact relation between native overlay and guest timing remains unverified.

Paired same-executable cache-off control perf_coop_pose_cache_off was started
with identical 230-second map/mode/config/HDD arguments, cvar zero. Running
exec session 70689. Revalidate this session/process before further work; do not
start another emulator, mutate the ISO, or build during the timing run. The
cache still defaults OFF. Do not promote it based on the first result alone.
### Paired pose-cache comparison: guest gain, host discrepancy unresolved

perf_coop_pose_cache_off completed, exited and restored the ISO; exec70689 is
terminal. Cache OFF: 21 retained windows, mean17.438/min16.2, finalCA_ACTIVE,
borg1, two completed views. All windows below30. Same candidate SHA as ON.

New scripts/compare_sp_perf_probes.py calculates rates from original frame and
realtime deltas, deduplicates serials, rejects conflicting/missing windows,
requires complete tagged gameplay windows, and retains only whole windows
within the shared server-time interval. It requires matching binary/map/mode,
clean completed runs and restored ISO transactions. Six regression cases in
scripts/tests/verify_perf_comparison.py passed. Result artifact:
build/research/letterbox_perf/mdr_pose_guest_comparison.json.
Shared interval100957..169585serverms: OFF14windows17.6213FPS(min17.1863),
ON14windows21.1271FPS(min18.0879), +19.8948%. Actual retained boundaries differ
slightly and are recorded; no fractional/interpolated boundary samples used.

This is NOT accepted causal improvement yet: identical230hostsecond runs
finished at guest realtime171073ON vs211332OFF and server169585ON vs196213OFF.
User screenshots also showed worse host-visible FPS for the earlier run.
Load phases differed materially before normal cache use. Sys_Milliseconds
uses Xbox GetTickCount, so guest-clock rates must be separated from host time.
No root cause of clock/progress discrepancy has been established. No default
cache enablement, shipping performance claim, or retail acceptance.

A bounded host/guest diagnostic control diag_coop_pose_host_off is now live
(exec33488), same candidate and230seconds, modecoop, cache0, --diagnostic.
This uses existing native emulator telemetry polls, not desktop input. Polling
perturbs timing, so compare only against an equivalent diagnostic ON run and
never count these as clean gameplay qualification. Revalidate the live handle;
do not overlap heavy builds/emulators/ISO writes until restoration completes.

User asked whether earlier distance-based LOD selection could help2P. Current
code applies extra bias only to non-MDR models in3P/4P; MDR bodies remainLOD0.
Earlier forced lower-bodyLOD1 experiment was4P, regressed54.4to43.1guestFPS,
and was removed. That is not proof against every distance-based2Ppolicy, but
no new LOD change has been made. Preserve fidelity and existingLODcontract
while resolving the pose-cache timing evidence.
### Wall-time measurement verified against final gameplay records

New scripts/analyze_sp_host_progress.py confirms every polled heartbeat tuple
(frame/realtime/serverTime) against the final FPS log before using it. It
requires all intervening windows, including the starting heartbeat window,
to be explicitly gameplay with zero excluded checks. Missing records,
conflicting final records, stale heartbeats, and intro overlap cannot create
passing evidence. Five regression tests passed in verify_host_progress.py.
This remains diagnostic: heartbeat publication/poll latency and monitor
read overhead are included. It is not direct presentation FPS or retail proof.

For diag_coop_pose_host_off,13observations matched final records,0rejected.
Seven retained gameplay intervals span106.1hostseconds:1429frames,13.4684
frames per wall second; guest realtime progressed0.80668seconds/wallsecond.
Artifact build/research/letterbox_perf/mdr_pose_host_off.json. This confirms
that guestFPS alone overstates host-visible progress in this run. No cause
for the ON/OFF discrepancy is established; equivalent ON diagnostic remains
required. The source profile currently points to scripts/output until wrapper
retention completes; regenerate from retained copy afterwards.

AGENTS.md now distinguishes guest-clockFPS from wall-time frame progress and
requires diagnostic labeling. GAME_TODO.md notes co-op remains below target
and the pose cache remains disabled. Audio and authored scripts untouched.

At this note, diagnosticOFF exec33488 remains live but guest emulation is
stopped for final diagnostics. Auto-dumps are progressing and ISO restoration
is still pending. Revalidate same handle; do not start the ON run until the
wrapper finishes and marks restored=true.
DiagnosticOFF has now completed (exec33488 exit0), emulator exited and ISO
restored=true. Host artifact regenerated from retained research profile.
Equivalent diag_coop_pose_host_on230second diagnostic is now started; use its
live exec handle returned by the tool (record below) and wait for restoration
before further builds/ISO work. Same XBE, map, mode, monitor interval and dumps;
only r_efMdrPoseCache changes to1. Do not change measurement methods mid-pair.
Current diagnosticON exec session: 37297. Revalidate before continuing.
### Pose cache rejected on verified real-time progress

DiagnosticON completed (exec37297 exit0), final evidence saved, emulator
exited and ISO restored=true. Both diagnostics used the exact same candidate,
map/mode/duration/config/HDD/native poll settings. All13ONheartbeat tuples
matched final FPS records,0rejected. Seven full gameplay intervals:973frames
in106.1wallseconds=9.1706frames/second; guest clock ratio0.47348.
The shared server interval115723..163580 retains4OFFintervals (124062..162218)
and7ONintervals (115723..163580):12.5700OFF vs9.1706ONframes/wallsecond,
-27.0439%. This is diagnostic and includes monitor/heartbeat timing limits;
it does not establish retail behavior. It does refute accepting this as an
XEMU speedup based only on the earlier +19.9%guest-clock FPS.

The raw-pose runtime experiment has been removed from tr_animation/tr_model,
including allocation, lookup/copy/store and lifecycle hooks. Archived core,
runtime, native tests and reapplication integration.patch are under
build/research/letterbox_perf/rejected_mdr_pose_cache/. The patch contains only
pose integration, so it does not roll back the retained MDR index-capacity fix,
model-storage fallback or diagnostic skin-cost instrumentation. Original
candidate XBE/map and all comparison evidence remain in bounded research.

Post-removal checks passed:39camera checks (originalnegative22fail),9MDR
storage cases,4index-capacity boundary cases plus originalnegativecontrol.
Host analyzer now has7passingcases including complete shared-interval comparison.
SP coverage inventory now records the existing borg1 andborg2 guest-clock
map-start evidence, with host timing/traversal/retail explicitly open. The
legacyborg1summary lacks finished_in_game; inventory keeps that field unknown.

A production Release restoration build is running, exec17403:
scripts/build_xbox.ps1 -Target spmp -ReuseObjects -SkipAssets -SkipStage.
Log: build/research/letterbox_perf/mdr_pose_rejection_restore_build.log.
Do not start emulator timing until it finishes; verify both release artifacts
and removed pose symbols. Afterward continue broader map/moving-view coverage
and measured optimization, with all intro-overlapping FPS windows excluded.
Audio remains paused; no authored ICARUS behavior or model LODs changed.
Restoration build exec17403 finished successfully (exit0). Production buildIDs:
default Sep9 2026 19:02:52; efmp Sep9 2026 19:03:34. Both linker maps contain
zero R_STEFX_FindMdrPose/R_STEFX_ClearMdrPoseCache symbols. Cleanup executed.
No emulator remains running. Source and release artifacts now exclude the
rejected pose cache; preserved index/model-storage/letterbox checks pass.
Next probe started: perf_sp_stasis1_restored,120seconds,SP,clean timing. Uses
new exact snapshot optimization_restored_190252.xbe/map. This broadens map-start
coverage after removal; it is not traversal or retail proof. Record/revalidate
the returned live session before work continues; no overlapping builds or ISO
writes. Clean probe guestFPS must retain its explicit clock-basis limitation.
Current stasis1 probe exec session: 45586.
### Stasis1 below-target window; boundary placement and stutter telemetry

perf_sp_stasis1_restored completed (exec45586 exit0), ISOrestored, final
CA_ACTIVE/stasis1/oneplayer.19retained gameplay windows averaged68.663guestFPS,
minimum35.4,4below45;1windowexcluded. Later samples44.6/38.9/35.4/37.6 with
client17..21ms,server0..7ms andzonefree~1.2MB. Inventory records this as a
below-target map window, not an average-based pass. No traversal/retailproof.

Source inspection found both CL_STEFX_MarkFpsContext calls adjacent atframe
entry despite their both-ends comment. The second call is now after rendering
and cinematic/console work, before FPS reporting. This closes acceptance of
transitions starting during the final frame. Existing opening-intro exclusion
atframeentry remains; do not assume olderwindowlabels handled allendtransitions.
Nativeverify_fps_context now includes actual call-placement checks and the
adjacent-entry negativecontrol,8windowcases,4frame-time cases; allpassed.

Added bounded1KiB frame-time histogram to existing FPS windows. One guestclock
read and histogram update at each completed frame; no allocations/per-frame
logging. ft=count/maxMS/p95upperMS/p99upperMS/over50count precedes the existing
unchanged gameplay/excludedChecks suffix. Histogram resets with eachFPSrecord;
previousframe timestamp continues across windows. Overflowbin255 returns
windowmaximum as a conservative percentileupperbound. Nativecasescovermixed
fast/slowframes,windowreset,unsignedclockwrap,95th/99thrank. Capacitytest verifies
both full-width formats fit512bytes, actualnativeextractor accepts them and
rejectsmalformedft. Probe summaries now retain only accepted gameplayftwindows
and report observedframes,maxguestframems,over50count; legacyabsence staysNone.
Filtering,comparison,mode,hostprogress checks passed. Audio andICARUS untouched.

ProductionReleasebuild exec20943 exit0: default Sep9 2026 19:11:24,
efmp19:12:04; freshnesspassed. Cleanupexecuted. New exactSPsnapshot
frame_time_windows_191124.xbe/map. Nativevisualprobe visual_sp_stasis1_frame_times
isnowrunning,120seconds,SPstasis1,fourcapturesat35/60/85/110hostseconds.
It verifies scenecontent/newtelemetry, notcleanperformance acceptance. Use
returnedexecsession; don'toverlapemulator/ISO/buildwork. Must inspect allfour
nativecaptures individually andverifyftrecords after completion/restoration.
Current stasis1 native visual probe exec session: 2632. Revalidate before further work.
### Stasis1 visual proof and simulation-clock FPS correction

visual_sp_stasis1_frame_times completed (exec2632 exit0), ISOrestored and
finalCA_ACTIVE/stasis1/oneplayer. All4nativecaptures inspected individually:
35s showscombat/projectiles,60s same room withchanged effects,85s enemies near
player/67health,110s largeoverlappingeffects/19health. No obviousnewgeometry
regression observed; this doesnotprove allvisualfidelity. Imagesretainedunder
build/research/letterbox_perf/screenshots/xemu-2026-09-09-19-14-26.png,
19-14-39.png,19-15-04.png,19-15-29.png. Thisisvisualdiagnostic, notcleanFPSproof.

New5fieldftrecords wereextracted andsummarized:19acceptedwindows,6955observed
frames,max3964guestms,6framesover50ms. The43.1simulationFPSwindow had217frames,
max33ms,p95upper30,p99upper32,0over50: sustainedcostthere, notonelargehitch.
Howeverfirstacceptedwindow loggedfps79.9with400framesand3964msmax. This exposed
anothermeasurementproblem: cls.realtime isincremented byCom_ModifyMsec output,
which capsnormalSPframe delta at200ms andcanapplytimescale. Thuslegacyfps is
SIMULATION-clockFPS, notunclampedGetTickCountelapsed. Priorhostratiofields named
guest_seconds_per_wall_second reallyreflectsimulationprogress. Existingnotes
andartifacts arehistorical; donotuse thoseFPS values ashardwareacceptance.
No cause forthe3964msstall isestablished yet; donotassume loggingorassetload.

Framehistogram nowalsosums fullSys_Millisecondselapsed; sixfieldftlayout:
count/elapsedMS/maxMS/p95upperMS/p99upperMS/over50count. Samewindowexclusions.
Gameplaytiming/ICARUS untouched. FPSmirror slots grew512to576bytes (+4KiB total)
sofull-widthrecordsretainclassification. Nativecontext/rank/reset/wraptests,
actualextractor/capacity tests,elapsedparser testsandexistingmodegatespass.
Probe summaries retain simulation_clock_fps separately and onlyaccept when
everyretainedwindow hasvalidelapsedtiming; primaryfps/weightedaverage/minimum
thenuse frames/fullguestelapsed. Missing/legacy5fielddata cannotpassnewgate.
AGENTS.md records thisbenchmarkrule. Realhostpresentationstillneedsseparateproof.

ProductionReleasebuild for thisreportingchange isrunning: exec82546,
logbuild/research/letterbox_perf/unclamped_frame_time_build.log,
Targetspmp ReuseObjects SkipAssets SkipStage. Revalidate beforeemulatorrun.
Nextverify sixfieldft records inactualruntime andsummaryacceptance; thenprofile
thestasiscombatcost. Existing diagnosticmdr_skin_profile.xbe/map isavailable
forboundedshader/MDRcostanalysis butlacksnewmeasurementfields andcannotqualify.
Elapsed-timebuild exec82546 finishedsuccessfully: defaultproduction19:22:00,
efmp19:22:35. Cleanupdone. ExactSPsnapshotunclamped_frame_time_192200.xbe/map.
Clean120sSPstasis1 probe perf_sp_stasis1_elapsed nowstarted. Revalidate its
returnedlivehandle; noheavybuild/otheremulator/ISOwritesuntilrestored. Verify
sixfieldruntimeft,elapsed_timing_complete and fps_basis inresult. Do notclaim
performanceimprovement merelybecausemeasurementmethodchanged.
Current clean elapsed-time stasis1 probe exec session: 50486.
### Full elapsed-time runtime verified; attribution probe prepared

perf_sp_stasis1_elapsed completed(exec50486 exit0), ISOrestored,CA_ACTIVE,
stasis1/oneplayer. elapsed_timing_complete=true, fps_basis=guest elapsed time.
7258retainedframes; weighted69.777FPS,minimum33.5397,1below45window,max4238ms,
7framesover50ms. Legacy simulation average72.43. Specificallysample2 logged
simulation59.4 butft298/8885/4238/18/25/1:298frames/8.885seconds=33.5397FPS.
This proves the unclampedmeasurementpath, notanoptimizationgain. Inventory now
uses thisnewer elapsed-time evidence forstasis1 andkeeps acceptanceopen.

Sourcecg_main defersnon-playerclientmodelregistration untildraw;cg_players
allowsone deferredregistration perclientframe. Do notchange thispolicy or
shiftstallsintoloading withoutmeasuringcause/memory. Addedtiming aroundthetwo
existingCG_RegisterClientModels drawsites(P2/general). Existingafterlogs append
elapsedMs. Newexportg_SPXBModelRegisterStats[8] preservescount,totalms,maxms,
peakentity,peakcg.time,lastentity,lastms,lastcg.time. Timing excludesouterbefore/
afterlogcalls; internalregistration workremainsincluded. Noasset/gameplay/audio
behaviorchanged. Nativeactualhelpertestpassed duration,total,peakattribution,
latestevent,unsignedtimerwrap. Wrappercollects8wordsfrommatchingmapsymbol if
present; legacyabsence staysNone. Countersareprocess-lifetime,notper-mapreset.

Buildrunning exec76623 Targetspmp ReuseObjects SkipAssets SkipStage:
build/research/letterbox_perf/model_registration_timing_build.log. Revalidate,
verifylinkedsymbol, thenrunboundedstasis1initialregistrationprobe(~60seconds)
withnewXBE/mapsnapshot. Comparepeakregistrationduration/game-time withftstall;
noassumption thatregistration isthecause untilruntimecountersproveit.
Registrationtimingbuild exec76623 exit0. Productiondefault19:29:50,
efmp19:30:24. LinkedstatsVA00f27bec fromx_game:cg_players.obj verified.
Cleanupdone; snapshotmodel_registration_192950.xbe/map retained. Started
perf_stasis1_registration60secondcleanSPprobe, intendedearlystallattribution,
notfullmapperformanceproof. Revalidate returnedhandle beforefurtherwork.
Current registration probe exec session: 98463.
### Draw-time registration ruled out; central registration now instrumented

perf_stasis1_registration completed(exec98463 exit0), ISOrestored,CA_ACTIVE.
Elapsedaverage73.182FPS,min45.0607 inthisshortwindow, butmaxframestall4008ms
and7framesover50ms persist; thisisNOTstutteracceptance. Eightregistrationstats
wereallzero. Thatrefutes thedraw-timefallbackcalls asstallcauseforthisrun,
notallmodelregistration. SourceNPC_stats callsCG_RegisterClientModels directly
whenNPCsPrecached;cg_servercmds alsoinvokesit. Thosecalls bypassedfirstprobe.

Moved timing fromtwoCG_Playerfallbacksites tocentralCG_RegisterClientModels,
afterargumentvalidation throughrenderInfo/infoValid/clientinfo setup. Retained
same8wordprocessstats/helper; appendednumericSTEFX_MODEL_REGISTER_TIME after
centralcall andaddedstrictnativeextractor. Noassetloadingpolicy,spawnbehavior,
LOD,audio,orICARUS changes. Nativeaccumulatortestpassedagain. Buildrunning:
central_registration_timing_build.log,Targetspmp/ReuseObjects/SkipAssets/SkipStage.
Revalidate returnedbuildhandle,verifylinkedsymbol thenrepeat60sstasis1probe.

Independentread-onlysourcecheck:activeMDRLODselector forcesLOD0,whilecompact
BorgMDRstorage retainsentireLODtail. UnusedlowerLODs mayoffermemorysavings with
identicalrenderedLOD0, butnoasset/modeldatachangehasbeenimplemented. Audit
alltags/offsets/callersandactualpotentialbytesbeforepursuing; donotconflatewith
previousrejectedlower-detailrenderingexperiment.
Current central-registration build exec session: 50370.
Central-registrationbuild exec50370 exit0. Productiondefault19:38:04,
efmp19:38:39; cleanupdone. Snapshotcentral_registration_193804.xbe/map.
Startedperf_stasis1_central_register60sSPprobe; awaitsamehandle/restoration.
Current central-registration probe exec session: 50062.

### User priorities and stasis1 defects — evening update

SP and four-player Holomatch are mandatory. Co-op is now the lowest tier and
may remain clearly labeled "not finished". The required performance targets
are 45+ FPS across SP and 30+ FPS in four-player Holomatch. This priority update
supersedes the older goal wording that treated co-op as equally required.

The user identified the green striped stasis1 walls/floor as incorrect. Earlier
native captures did not prove correct materials or visual fidelity. Investigate
the effective BSP/shader/image chain before accepting this scene visually.

The apparent death in perf_stasis1_paused_setup was the scheduled 60-second
harness stop: its report records alive_at_end, followed by
emulation_stopped_for_final_diagnostics. Final client state was 7 (active),
stasis1, one player. No XEMU process remained after completion.

Pausing the guest with native QEMU -S until harness setup finished did not
remove the early stall: max 3847 ms, slow window 36.2865 elapsed guest FPS,
weighted mean 69.1291. The run does not pass. The central registration probe
recorded three calls totaling 439 ms, maximum 277 ms; this does not account for
the several-second stall, and direct precache paths are not all covered.

Read-only unused-MDR-LOD audit found 5,375,064 lower-LOD bytes across 201 models,
zero rejected layouts. This is total asset storage, not per-map residency.
No production model or package was changed by this audit.

Material investigation: a ray from the original stasis1 spawn (304,-904,50),
yaw 90/pitch 15, hits floor surface 2921, shader textures/stasis/scum_256.
The original image is dark pebbled gray; the original nearby wall images are
gray-blue. Extracted xbox0 DDS previews preserve those appearances. The actual
ISO's scum_256.dds and m_stasiswall_b.dds are byte-identical to release xbox0,
so stale copies of those two packaged images do not explain the stripes.
The read-only material audit completed with 65 used materials and no missing
assets; it uses a broader package index than the SP runtime and does not prove
runtime binding. Evidence: stasis1_materials.json/txt and iso_*.dds under
build/research/letterbox_perf. Upload offsets, texture residency/binding and
runtime material selection still need investigation. No texture fix claimed.

The exact XDK 5558 Xbox xgraphics.lib XGSetTextureHeader was linked into a
bounded desktop console probe (no UI or input). For 128x128 DXT1, eight mips,
it returns 10936 bytes, exactly matching both DDS payloads. Thus the suspected
fileSize-minus-GPU-size offset mismatch is ruled out for these two files.
The PC XDK library returns the same size. Probe source/executables are
xg_texture_layout* under build/research/letterbox_perf.

A diagnostic-only upload probe now records dimensions, file/GPU byte counts,
texture number/address, source and GPU-payload FNV hashes, and header words for
scum_256 and m_stasiswall_b. It is absent without STEFX_HW_FRAME_DIAGNOSTICS.
Expected hashes from the ISO are in stasis_upload_expected.json. Build session
77854 is compiling the diagnostic pair; revalidate this handle before testing.
Build log: stasis_upload_build.log. No runtime result or fix claimed yet.

Upload probe completed. The first run lost its monitor connection after the
full RAM scan, before small counters were saved. The harness now saves explicit
virtual-memory counters before the large scan. The retry completed and restored
the ISO. Both textures loaded once, 128x128, eight mips, file 11064 bytes, GPU
10936 bytes. Source and upload hashes exactly match the ISO. Their GPU addresses
were 0x836e5b80 and 0x836f9380. Reading the corresponding physical regions from
the native final RAM capture proved both full payloads were still byte-identical
at the end. Evidence: stasis_upload_actual.json, stasis_final_gpu_0/1.bin and
diag_stasis_upload_retry_20260909_201027_final_stasis_upload.bin. This rules out
bad source bytes, initial upload, and later overwrites for these two textures;
it does not prove the correct images/coordinates were used for drawing.

The user also reports broken communicator popups, possibly a regression or
level-specific. CG_DrawTalk uses UI artwork plus CG_Draw3DModel for the head.
No audio investigation is authorized. Correct visual behavior remains open.

Added diagnostic-only g_SPXBStasisDraw[40] to record the actual multitexture
draw bundles and UVs for the two materials (20 words each). The incremental SP
diagnostic build passed. Snapshot stasis_draw_diagnostic.xbe/map/exe includes
the matching PE, needed by native address translation. Native visual/counter
probe diag_stasis_draw_visual is running as session 56108: 45 seconds, four
captures starting at 10 seconds with seven-second spacing. Revalidate before
starting another emulator or restoring release binaries. Current build/release
binaries are diagnostic; preserved central_registration_193804.xbe/map and
central_registration_efmp_1938.xbe/map are the earlier production pair, not a
fresh build of current sources. Restore a production build before hardware use.

diag_stasis_draw_visual completed (session 56108 exit 0), ISO restored. Three
native captures were produced and each inspected individually: 20-17-15,
20-17-16 and 20-17-21 on Sep 9. They reproduce the striped surfaces but do not
contain an active communicator popup. Do not claim popup visual coverage.
The floor draw row records 2757 calls, diffuse tex 39 (the verified scum_256
upload), lightmap tex 15, tcGen 6/2 (texture/lightmap), isLightmap 0/1,
GL_MODULATE 8448 and r_lightmap=0. The m_stasiswall_b row is zero: that material
was not observed through this draw path, so there is no wall binding conclusion.
Next useful diagnostic is the lower-level D3D texture binding/stage state and
submitted texture coordinates for the floor, not another source-image audit.
Existing STEFX_TraceShaderRuntimeDraw in win_qgl_dx8.cpp already checks binding,
stage, matrix and packed vertex contracts but is currently MP-only; safely
reuse or add a narrowly scoped diagnostic for SP. The active popup itself still
needs native capture. No rendering fix has been made or qualified in this turn.

### Stasis1 lighting isolation and borg1 comparison

The D3D diagnostic (`diag_stasis_d3d_20260909_202357`) observed matching
expected/actual texture objects, FVF, texture coordinate indices and pointers.
No binding mismatch was recorded. The diagnostic-only white-lightmap run
(`diag_stasis_diffuse_20260909_202851`) removed the yellow/green stripes in all
three native captures (20:29:45, 20:29:46, 20:29:50). This isolates the defect
to the lightmap path; fullbright is not a proposed fix or qualification mode.

The natural borg1 comparison (`visual_borg1_communicator_compare_20260909_203249`)
completed and restored its ISO transaction. The user confirmed its communicator
UI works, so the popup failure is not universal. Other levels remain unchecked.
The user also reported a skipped Munro line and no facial animation. This is
recorded separately in GAME_TODO.md; audio investigation remains paused.

The user explicitly requested returning to stasis1 and correcting its defects.
A diagnostic build now records source/upload hashes for all 25 lightmaps.
The aligned native XDK swizzle probe produces expected hashes in
build/research/letterbox_perf/stasis_lightmap_expected.txt. The earlier desktop
probe crash was resolved by aligned buffers and running its executable locally;
it was not evidence of an engine crash. Current diagnostic run is
`diag_stasis_lightmaps`, session 79501; revalidate before another emulator run.
An unrelated UT99 xemu process (21660) was observed and left untouched.

### Writable capture buffer overwrite

`diag_stasis_lightmaps_20260909_204229` completed and restored the ISO. All 25
lightmap source and initial upload hashes match the native XDK reference.
The final native RAM capture shows lightmaps 0–9 changed, while 10–24 still
match their upload hashes. Evidence: stasis_lightmaps_residency.json and
stasis_lightmap4_final_swizzled.bin under build/research/letterbox_perf.
Unswizzling the overwritten floor map reproduces the yellow striped pattern.

Found a concrete capacity violation: Upload32 applies the artwork size cap to
*screen, but dllCopyBackBufferToTexEXT enlarges its header for distortion
captures without increasing its allocation. Preserve the declared writable
*screen dimensions and validate the SDK-required byte size before any GPU
capture writes. Diagnostic counters record requested size, capacity, required
bytes and rejected calls. This is a candidate fix, pending native validation.

Build stasis_capture_fix (Sep 9 20:49:36, frame diagnostics) succeeded. Native
run visual_stasis_capture_fix is session 92421 (90 seconds, natural scripts,
no diffuse-only switch, captures every three seconds). The capture helper now
writes final texture residency hashes before deleting its temporary RAM dump.
Python compilation checks passed. Revalidate session before another run.

### Native fix confirmation

`visual_stasis_capture_fix_20260909_205020` completed its 90-second natural
stasis1 run and restored the ISO. Final residency proof checked both audited
world textures and all 25 lightmaps: 27 matches, zero changes from upload.
Native capture 20:51:28 shows correctly lit gray-blue walls and pebbled floor.
The capture counter recorded one 512x256 copy, 524288 required bytes, 699052
allocated bytes, zero rejected copies. The actual caller is RE_InitDissolve's
level-transition capture in code/renderer/tr_draw.cpp (the same target also
serves distortion). The old capped allocation was only 43692 bytes, confirmed
with the exact Xbox5558 XGSetTextureHeader library in xg_capture_capacity.cpp.
This is a 480596-byte write beyond the old allocation's capacity.

A diagnostic-only communicator rendering fixture uses the real CG_DrawTalk /
CG_DrawHead paths with a neutral Janeway face; it does not change audio state,
dialogue or ICARUS timing. It is absent from production builds. Native capture
20:56:51 in visual_stasis_popup_fixture shows the face and popup rendering
cleanly in stasis1. This verifies drawing only, not natural dialogue activation
or lip sync. That run is still collecting final evidence (session 59816).
The probe wrapper rejects stefx_diag_* commands in performance qualification.

Retained native images and residency proof are under
notes/evidence/stasis1_20260909/ (corrected_world.png,
communicator_visual_fixture.png, texture_residency.json).
The popup fixture run completed, restored the ISO, and again reported 27
unchanged textures. It is a visual drawing test, not campaign qualification.
The wrapper's negative test rejected an attempted performance run with the
fixture command before accessing any ISO (expected failure).

Production SP/Holomatch rebuild is running as session 85976, log
build/research/letterbox_perf/stasis_production_build.log. Diagnostic binaries
must not be staged as production. Inspect the completed binaries for absence
of the communicator fixture and diffuse-only override before hardware use.

Production rebuild completed successfully (session 85976 exit 0):
- default.xbe: Sep 9 21:02:31, SHA256
  83c8c7fe02afab2ffa5512985d8d0b1edbc2dc6dfc13069e86c374674b658aa1.
- efmp.xbe: Sep 9 21:05:43, SHA256
  4f61cfe74d89fc3f28a019de28c94a8445bbe6439b5d7275f6d7412a76826909.
Both identify as production, contain the capture-capacity guard, and contain
none of the communicator, diffuse-only or upload diagnostic markers. Current
build/release binaries are now production. Assets were unchanged and hardware
staging was not performed. Full identity report: stasis_production_pair_identity.json.

The first production visual run aborted before guest boot because the native
monitor returned an empty startup-status reply. Its ISO transaction restored;
this is not an in-game crash. Retrying as visual_stasis_production_retry,
session 6295, using the same preserved production SP XBE/map/PE.

### XEMU updater spawned a different session

The user noticed borg1 in XEMU 0.8.136 during the final stasis1 check.
The first production test's own process was PID 27296. Its native stderr
records extraction of xemu.exe and "Restarting to updated executable
C:\Games\Emulators\Xemu\xemu.exe". Child PID 15660 (parent 27296, created
21:07:08) restarted without the test command-line arguments. It loaded
C:\Games\Emulators\Xemu\xemu.toml and that config's separate C:\Programming\GitHub\
Star-Trek-Elite-Force-X\build\xemu\StarTrekEliteForceX_unified_minisoak.iso.
Thus the unexpected borg1 instance WAS spawned by this task's test launch,
not an unrelated user session. The initial explanation was corrected.
The test-specific config already had update checks disabled; the log proves
an updater restart but does not establish why the update was initiated.
Closed only the verified updater child PID 15660. The separate UT99 process
was untouched. Its poor FPS is not a measurement of the verified stasis1 build.
The production stasis1 run on port 4491 produced a correct native screenshot;
its final diagnostics/cleanup are still pending as session 9538.

Final production stasis1 check completed successfully on port 4491 (session
9538 exit 0), with the ISO restored and bounded cleanup complete. Native
production screenshot: notes/evidence/stasis1_20260909/production_world.png.
Production build identities are retained alongside it in production_builds.json.
Current default.xbe and efmp.xbe are production builds. The temporary test ISO
entries were restored; the shared ISO has not been permanently repackaged.
Retail confirmation and natural communicator activation remain open. The
broader letterbox/performance goal remains unfinished; no FPS acceptance is
claimed from these visual runs or from the updater-spawned borg1 instance.


### Stasis1 playability follow-up
- Pinned XEMU 0.8.134 stationary production probe perf_stasis1_capture_fixed_20260909_211527: weighted guest elapsed 73.36 FPS, minimum window 41.26 FPS, maximum frame 3953 ms. This is not presentation FPS or retail qualification; the early stall remains open.
- Normal post-map +forward/+attack probe visual_stasis1_forward_combat_20260909_212204 completed 120 seconds. Native captures show player against a wall firing; final read-only player state origin (359.66, -490.03, 43.72), health 7, command time 87251. It proves continued simulation/input response, not level completion or successful routing.
- Added read-only player-state capture using offsets compiled against active Xbox game structures (sp_player_layout.cpp/.asm in research directory). Added post-map command support without script-override smoke marker or host input.
- Rare long-frame logging under investigation to split total/server/client time. Initial compilation failed because frameCount was diagnostic-only; corrected logger to use realtime and production duration counters.
- Stay on stasis1 until playability/progression/stability are demonstrated. Audio remains paused.

- perf_stasis1_long_frame_20260909_212859 completed and restored ISO. Minimum elapsed gameplay window 42.5 FPS; early maximum frame 2976 ms. Text long-frame records did not survive to final RAM extraction, so absence is not evidence of no stall. Added bounded 129-word retained array (count + up to 32 realtime/total/server/client rows) and final read-only dump for reliable attribution. Production build 21:34:21 includes this instrumentation; perf_stasis1_retained_stall is the current 45-second stasis1-only probe.

- perf_stasis1_retained_stall_20260909_213459 completed/restored. Retained long-frame rows (realtime,total,server,client) in ms: (1842,676,0,675), (8212,4383,0,51), (12782,4077,121,20). The ~4.08s active stall is mostly outside server/client. Added retained slow queued-command records (up to eight, command string and duration), build 21:38:38, now testing perf_stasis1_slow_commands (45 seconds). Source contains an unconditional one-second SG_WriteSavegame popup wait; this is a candidate contribution, not yet attributed. No save behavior changed.

- Slow-command instrumentation candidate (21:38:38) failed before game startup in two probes: perf_stasis1_slow_commands and visual_stasis1_command_retry. All game globals zero; native screenshots show Xbox boot logo. These are invalid timing evidence, not zero-duration commands. Removed that command instrumentation, preserving the working common-frame retained records, and rebuilding via stasis_restore_launch_build.log. Actual cause of pre-game failure not established; test restoration before further diagnosis. No save behavior or audio changes made.

- Restored production build 21:44:41 (command instrumentation removed; cmd.cpp has no remaining diff) launches stasis1 again in visual_stasis1_launch_restored. Native capture screenshots/xemu-2026-09-09-21-47-00.png shows normal combat and corrected gray-blue world. Preserved matching build triplet stasis_restored_launch.xbe/map/exe and SHA manifest. Early stall and full progression still open; do not move to other modes yet.

### Continued stasis1 stall attribution
Previous turn was progress: captured responsive gameplay, narrowed the early stall outside server/client, rejected a command-instrumentation candidate that did not boot, and verified the restored build. Current command-phase probe measures Cbuf_Execute durations only from Com_Frame, preserving command internals and retained array allocation. Retained format v2 has up to 21 six-word rows, version at word128. Parser accepts legacy v1 and rejects invalid counts/short evidence. Another project emulator is active, so diag_stasis1_command_phases is explicitly diagnostic/visual, not FPS qualification. Audio and save behavior unchanged.

- diag_stasis1_command_phases_20260909_215213 retained v2 evidence: early active frame realtime11246 total3780/server120/client22/firstCommands3634/secondCommands0 ms. Thus first command buffer accounts for the stall. Initial load frame total4019 has no command time, as expected for event-driven load. Build 21:56:15 extends the same fixed-size retained allocation to v3: timings, command execution counts, and 16-byte last-command prefix, measured only from Com_Frame. Cmd_ExecuteString internals remain unchanged. Final player origin/view changed without harness post-commands in this diagnostic run; do not treat as a controlled movement route.

- Decisive v3 evidence visual_stasis1_command_identity_20260909_215713: realtime16228 total2982/server126/client40/firstCommands2812/secondCommands0 ms; exactly one command executed, prefix inGameCinematic. It is NOT an autosave stall. Retail PAK0 real_scripts/stasis1/start.IBI explicitly requests SET_VIDEO_PLAY st_06. Runtime log mirror confirms BinkVideo::Start succeeded. ffprobe of seed BaseEF/video/st_06.bik reports Bink video 512x384, 15 FPS, duration4.0s. Thus the blocking authored movie crosses active CL_Frame endpoints and was incorrectly counted as a gameplay frame. Adding a window-exclusion latch on successful fullscreen movie startup; shader videos remain gameplay, authored playback unchanged. No save optimization applied.

- Production build22:02:42 includes the movie-context latch. Updated verify_fps_context.py compiles the actual extracted frame-window implementation: 11 context windows +7 timing cases pass, including blocking movie exclusion and equally long real gameplay stall retention. Window timing data itself remains intact. Runtime visual_stasis1_movie_context is underway; it is a visual/context check, not retail or isolated host FPS qualification.

- VERIFIED runtime movie-context fix: visual_stasis1_movie_context_20260909_220340 completed100s, active stasis1, player health100, ISO restored and cleanup complete. Sample2 includes inGameCinematic and3435ms max but is gameplay=0/excludedChecks=1. Subsequent11 gameplay windows contain4011frames, max60ms,4frames>50ms; weighted guest elapsed72.85FPS, minimum62.83FPS. These are visual/concurrent-emulator measurements, not retail or isolated presentation qualification. This supersedes earlier claims of an unresolved3–4second gameplay stall: that interval was the authored movie and incorrect FPS attribution. Full level progression still unverified; stay on stasis1. Production22:02:42 triplet retained as stasis_movie_context_production.*; evidence copied to notes/evidence/stasis1_20260909/movie_context_*. Shared base ISO and hardware stage are not permanently updated.

### Input-only stasis1 progression probe
Previous turn was progress: corrected blocking-movie FPS attribution and verified runtime exclusion plus subsequent gameplay windows. To exercise progression, added an explicit temporary-file input replay (ef_sp_input_replay.txt), restricted by the probe CLI to visual/diagnostic SP runs. It changes only normal client usercmd input before authoritative cl_overrideAngles and CL_FinishMove, observes camera locks, validates map/time/bounds, stops at completion or map/time reset, and leaves ICARUS untouched. No script-override marker is enabled. Actual-header compiled tests cover timed input, gaps, camera lock, release, absent/invalid/mismatched files, and caller ordering. Initial fixture moves left then backward toward the opening transporter; successful routing is not assumed. Build stasis_input_replay_build.log is underway. Never package the diagnostic input file in release stages.

- Input replay Release build22:18:55 completed. visual_stasis1_transporter_route is the live150s visual probe with scripts/tests/fixtures/stasis1_transporter_input.txt, no script-override marker, native screenshots. CLI negative checks reject use as a qualification run and reject map mismatch before opening an ISO. Existing mode/ISO tests pass with replay-marker contamination rejection added.

- visual_stasis1_transporter_route_20260909_222105 completed150s/restored ISO. Native captures show rear alcove and continued encounter, final player origin(298.22,-1162.54,24.125), health-2: player died, so NOT a progression pass. Replay trace was filtered by production logging; no formatted replay records survived. Adding bounded replay messages to the production log allowlist and runtime extraction before changing route assumptions. No health/position/script changes.

- visual_stasis1_replay_trace_20260909_222903 completed65s, player health100 at(307.65,-1151.28,24.125), but second formatted-record logger filter still discarded replay traces. Added bounded replay-prefix retention there, then built Release successfully. visual_stasis1_replay_records_20260909_223535 completed/restored: actual row transitions prove left movement from(304,-904,24.125) to(215.53,-876.15,80.51), settling(194.35,-831.46,33.10), then backward motion ended(287.35,-1134.88,24.125). Finalhealth100 at(287.35,-1167.79,24.125). Thus route collided/climbed/slid instead of reaching the intended transporter. Trace preserved in notes/evidence/stasis1_20260909/input_route_trace.log. New opening-cover fixture retreats before strafing, then uses normal firing/yaw input to exercise the first battle. This is diagnostic routing, not FPS acceptance or campaign completion. Retail firstbattle.IBI leads to opendoor.IBI; opening-room combat must finish naturally before departure. No ICARUS or audio edits.

- visual_stasis1_opening_cover_20260909_224015 completed180s/restored. Route successfully reached(152.188,-1219.399,28.125), but player died at commandTime90702. This is a combat-route failure, not freeze evidence. Read-only entity inspection compiled exact active Xbox offsets and translated final physical RAM via captured CR3=0xf000; independently cross-checked client pointer, health, origin and commandTime against monitor virtual-memory reads. NPC snapshot preserved as opening_cover_entities.json: playerhealth-2, Chellhealth28, Telsiahealth12; teammates had no pending navigation task, aliens remained active. Multiple enemy spawn positions lie around(64..242,-1204), directly beside the failed cover location. Retail firstbattle.IBI decoded read-only using BlockStream layout: waves are timed, then opendoor runs; preserve this sequencing. Next fixture moves toward front and uses ordinary firing/strafing to test encounter survival. Still no full progression pass. Scratch physical RAM retained only temporarily for this entity diagnostic; original direct-physical-pointer parser was invalid and replaced with validated page-table translation before using any entity findings.

- visual_stasis1_front_cover_20260909_224858 completed180s/restored. Died much earlier at commandTime44992, origin(412.875,-636.430,24.125). Validated paged-memory snapshot proves this replay provoked friendly-fire behavior: Telsia health0/enemyMunro; Munro health-2/enemyblueguy (G_Damage records attacker in targ->enemy on death); Chellhealth100. G_Damage ignores accidental allied NPC hits unless the NPC targets the player. Do not attribute this failure to level instability. Front survival fixture removes all blind firing, retaining normal movement. Next test is survival and authored sequence progression only, not FPS qualification. Entity evidence retained in front_cover_entities.json. No engine/gameplay/audio/script behavior changed for these routing attempts.

- User explicitly requested invincibility using an existing cheat. Confirmed Cmd_God_f toggles FL_GODMODE, Xbox SV_Map_f enables sv_cheats. Stopped the unfinished hostile-aim build and removed its unshipped code; existing production default.xbe remains SHA169ceac2f253ada9e2b1f6c51c6688a7d773f695b765234a980d1f130ed13ab8. Input replay tests pass after removal. Interrupted object files require a normal rebuild before any future packaging; no new XBE was linked from that candidate.
- First attempt visual_stasis1_god_progression_20260909_230838 queued god directly after map. It was too early for CL_ForwardCommandToServer's CA_ACTIVE gate. Final independent memory reads confirm flags0/god_modefalse. However this run DID complete the opening encounter naturally: commandTime177452, Munrohealth10, Chell/Telsiahealth100, no living opening aliens, Chell at(-71,-1172.74,24.047), no teammate navigation waits. Paged snapshot preserved as opening_encounter_completed.json. This proves opening encounter progression, not full level completion.
- Added --god to the probe CLI: visual/diagnostic SP only, temporarily stages the existing client-active god command at time0. Marker-gated fast time, input overrides, weapon readiness, staging, aiming, wake-AI, and warp cvars explicitly disabled; server active-command dispatch gated out. No new cheat implementation or ICARUS changes. Final player read includes compiler-verified flags offset340 and FL_GODMODE16. Qualification rejects both requested and observed god mode; mode/ISO tests and three negative CLI cases pass. visual_stasis1_god_active is the live240s proof with --god and movement-only replay.

### Invincibility command-file lookup correction

The second client-active attempt (`visual_stasis1_god_active_20260909_231634`) exhausted all 20 file-open attempts. Guest RAM confirmed the diagnostic marker enabled, active-command time zero, `sv_cheats=1`, and player flags zero. The command file existed in the image, but exhaustive host traversal concealed its invalid position in the directory search tree.

`add_root_file` sorted lowercase names. XDVDFS uses uppercase ASCII ordering, where S precedes underscore; the active command and command-time filenames therefore selected different branches on Xbox. Primary implementation reference: https://github.com/antangelo/xdvdfs/blob/main/xdvdfs-core/src/layout/name.rs (including its underscore ordering test). The temporary ISO writer now sorts uppercase bytes. A new test follows the actual binary-search branches and failed before this correction, then passed afterward. Existing sector preservation and diagnostic/FPS exclusion tests also passed. Runtime proof is pending in `visual_stasis1_god_lookup_fixed`; no game executable or authored script changes were needed.

God mode runtime proof: `visual_campaign_stasis2_opening_20260909_233714` reports flags=16, god_mode=true, health/entity_health=100, command_time=128706 from guest virtual memory. Readable proof is retained in `notes/evidence/campaign_20260909/stasis2_god_player_state.json`. The stasis1 lookup-fixed run lost the monitor connection during final diagnostics before player-state collection; it is incomplete evidence. Moving the player pointer/client reads before larger dumps preserved the stasis2 result. No claim of full map completion or normal-difficulty performance is made.

### Campaign pass extension and diagnostic-reader cost

The user reports stasis1 stable to the observed point and requests the same treatment through all campaign maps. Retail BSP `target_level_change.mapname` links identify 45 main-path maps; optional brig/holodecks and tutorial are supplementary. Status is tracked in `notes/campaign_checks_2026-09-09.md`, with opening stability separated from route/exit and performance acceptance.

Stasis2's 180-second opening observation finished CA_ACTIVE, both teammates alive, no player task pending, health=100 and FL_GODMODE=0x10. The first movement replay stopped at (-3512.1904,612.9226,32.0617), before the first door plane at X=-3252; Telsia advanced to X=-3112.0107. A visual impression that Munro had crossed the door was corrected using guest memory. The next replay removes the initial sidestep and follows the corridor center.

The post-run RAM scanner repeatedly searched the remaining 64 MiB for every marker after every record. It now merges cached next-marker offsets; matching and record validation remain unchanged. 103 overlap/boundary cases pass. Reprocessing the exact retained corridor RAM yields all 78 records in the same order as the old reader, in 1.125 seconds excluding the dump. This affects post-run analysis only, not game FPS.

Centered stasis2 movement succeeded (`visual_campaign_stasis2_center_20260909_235028`): Munro reached (-2774.9661,640,32.125), beyond the first door and trap trigger, health=100 and god_mode=true at command_time=70183. Three spawned hostile NPCs are present, both teammates have 100 health, and Munro has no pending ICARUS tasks. Captures show the room beyond with intact-looking surfaces. This establishes first-door/trap activation, not encounter victory or level completion. Evidence saved under `notes/evidence/campaign_20260909/stasis2_corridor_*`.

Stasis3 initial walk (`visual_campaign_stasis3_initial_walk_20260909_235443`) finished CA_ACTIVE at (2678.535,-961.906,232.092), command_time=91345, health=100, god=true. The initial door opened and the player entered the larger room; HUD/terminal label and observed surfaces rendered normally. Captive NPCs elsewhere retain health=1; this alone is not evidence of a new damage bug. Full route and rescue/exit progression are not proven.

Voy6 opening (`visual_campaign_voy6_intro_20260909_235924`) remained active for 240 host seconds, command_time=201761, health=100, god=true. The doctor moved between captures; observed sickbay surfaces/HUD remained intact. Entity snapshot shows the characters alive and no player ICARUS task pending. Route and exit remain untested.

Voy7 stationary opening remains at its starting Trek door. Flag16 is TREK_DOOR/MOVER_MUST_FACE, not PLAYER_USE (128); an initial verbal interpretation was corrected before running the fixture. Touch_DoorTrigger requires movement toward the door between distances32 and72, so standing about52 units away explains the closed door. The next fixture uses forward movement only. No engine/script change is warranted from that stationary observation.

Scav1 completed its authored cinematic and transitioned naturally to scav2 (`visual_campaign_scav1_initial_walk_20260910_002145`). Final loaded_map=scav2, CA_ACTIVE, Munro at its authored start (-708,-1984,-103.875), health=100. The long-frame record identifies `maptransition` at realtime108372; no injected command requested scav2. Two sampled cinematic captures have48 solid black rows at both top and bottom across all640 pixels. The initial movement rows fell inside the camera sequence and do not establish manual traversal of scav1. God mode resets at the natural map change (finalflags0); the next isolated map probe re-enables it. Preserve this as transition evidence, not a failed map-name check or proof that god remained active across maps.

Scav3 and scav3b opening checks remained CA_ACTIVE with health100 and god confirmed. They are opening/short-movement evidence, not complete routes. Character-registration stalls remain open: voy7 261ms, voy8 259ms, scav2 ladder325ms; no preloading policy or LOD change made.

Scav4 baseline `visual_campaign_scav4_opening_20260910_005744` failed to enter gameplay. All captured loading images stayed unchanged; final CA_DISCONNECTED=1 and god=false. The virtual-memory log proves map/server load completed, then `AS_ParseSets` found one undefined set and dropped the connection. Original PAK0's scav4 BSP references `klingonhall` on trigger models143/145; original sound/sound.txt defines `klingonhallway` but not `klingonhall`. Read-only audit of the BSPs in original PAK0 found this mismatch only in scav4 (notes/evidence/campaign_20260909/ambient_reference_audit.json). The candidate keeps missing-file parsing fatal and all existing absent-set lookup guards, but logs undefined references instead of dropping an EF SP campaign map. No replacement sound or map/script edit is introduced. Runtime verification pending. This addresses campaign-load compatibility; muddy-VO work remains paused.

Scav4 fix verified in `visual_campaign_scav4_missing_set_fix_20260910_010809`: production SP XBE SHA256 74a0df759998936e18c4de95eddc22bf6fc0b8ab9fe3b3519472a731c9c8b778 (build Sep10 01:07:04) finished CA_ACTIVE=7 on scav4 after150 host seconds. Command time123726, Munro moved from(736,3720,8) to(574.1256,3720,8.125), health100, FL_GODMODE16. This verifies load recovery and short movement, not the route/exit. 7614 retained guest frames include one51ms frame. Diagnostic visuals/god exclude performance acceptance. Final physical memory total67108864, available1613824 bytes; do not casually precache extra NPC models with this small margin. ISO transaction restored successfully. Evidence archived under notes/evidence/campaign_20260909/visual_campaign_scav4_missing_set_fix.

Scavboss reproduced a hard stall in its opening cinematic on production XBE74a0df75. Native guest-virtual counters from the saved RAM: mainLoop451, heartbeat2, frame397, realtime5630, serverTime6330. Last client stageWF03 dereferenced `r_xboxWorldEffects`; its value was0xc8cf6d69, unmapped. Initial suspicion that the overwritten bytes were texture data was disproved by a guest GDB watchpoint. The writer is R_AddWorldSurface at runtime0x000bc412 (linked0x004ac412), OR-ing a surface-visibility bit into `g_SPXBHMPvsProbe[182+116]`. The array has296 words, so index298 overwrites `r_xboxWorldEffects` at0x008f98e0. The diagnostic seen/added bitmaps each support3520 surfaces, but their per-surface writes had no capacity check. Captured watchpoint log and matching XBE/map are retained under notes/evidence/campaign_20260909/visual_campaign_scavboss_opening. A bounds helper now rejects diagnostic bitmap indices beyond109 while preserving normal rendering; The first revision reused word66 for truncated coverage, but tr_main_retail overwrites that word with zFar; the follow-up uses unused word292 after both bitmaps. This diagnostic counter correction does not change bounds protection or rendering. Actual-helper native tests cover boundaries, observed word116, INT limits, and100000 surfaces with surrounding guards. New production XBE556882cb70ab6e67e2d2a469945c66d961b364fcc5d819067bb18882582b7176, build Sep10 01:47:05, is undergoing normal XEMU verification.

Timing evidence correction: the broad physical-RAM string scanner recovered a prior scav5 replay record during the scavboss run. Arbitrary RAM may retain obsolete strings, so its FPS records are not authoritative. run_sp_perf_probe now dumps the current paused guest's exported64-slot FPS ring and its index through virtual memory, validates complete records, and uses only those for FPS/zone summaries. Missing or malformed rings yield no performance acceptance. Raw scans remain diagnostic candidates only. Seven live-ring/wrap cases and four malformed-input cases pass. Prior raw-scan timing values require remeasurement; actual virtual-memory entity/flag/long-frame counters and native screenshots are separate evidence. The shortened counter-write waits were exercised on16 real Scav5 dumps without missing/short output, and immediate/delayed/short/missing write tests pass.

Scavboss bounds fix verified in visual_campaign_scavboss_pvs_bounds_fix_20260910_014818: 210 host seconds, cinematic completed naturally and first-person gameplay resumed. Final native player command time159561, health100, flags16, CA_ACTIVE7. The current FPS ring was read successfully, but this visual/god run does not qualify performance. Full fight and exit remain open. Evidence and entity dump are retained in notes/evidence/campaign_20260909/visual_campaign_scavboss_pvs_bounds_fix. New probes also compare two native guest main-loop counter reads before final pause; an active client state alone is insufficient liveness proof.

The diagnostic counter slot correction built successfully: production SP Sep10 06:17:17, SHA256268829ba3938da4fbd40e4a78476ac209b4b81d91e98928de320ff4bb8cb9c26. The next opening sweep uses this build. Counter word292 follows both 110-word bitmaps and preserves the camera fields66-71. Borg3 on the previous bounds-fixed build was still advancing at final check (58 main-loop increments), but its arena had only about1.66MB free and GlobalMemoryStatus reported zero physical pages available. Full progression remains open.

Voy15 opening sweep exposed a reproducible menu return after the briefing. Native com_errorMessage captured DEFAULT_MODELS failed to register. Hazard lower.mdr allocated 994184 bytes in the zone, then upper.mdr (2759560 bytes) could not allocate. Removing the incorrect zone-only preflight rejection and trying real allocators did not resolve this: visual_campaign_voy15_allocator_preflight returned to the menu and logs confirm all allocation paths failed. This candidate also makes engine errors critical log messages and makes model-file zone fallback nonfatal via Z_TryMalloc. A subsequent candidate precaches mandatory default upper/lower body resources during CG_RegisterGraphics; this changes resource order only, not authored NPC state. Runtime verification is pending; do not claim voy15 fixed. The sequential campaign batch stopped before dn1 and must resume there after resolution.

Voy15 early fallback-body precaching failed in visual_campaign_voy15_required_models: Z_Malloc reported 11064 bytes, tag22 (temporary workspace), and final main-loop delta was zero. That ordering change was removed. It must not be packaged as a fix.

The replacement uses lossless, independently decoded 16-frame MDR blocks for eligible non-Borg player models. It streams two bounded passes instead of retaining an entire raw source beside its replacement. Existing Borg base/patch storage stays intact. All frame bytes, surfaces, tags and LODs are preserved; no ICARUS or audio-quality behavior is changed. The first production build is Sep10 07:22:07, SHA25674350b3fe37f483da72ffc3d48e1f83051afe0830f7e383d7317ae127a6386db. Native codec tests round-tripped every animation byte in58 loose player assets plus synthetic boundary/malformed cases. The original test report omitted one flag byte per block in its savings column; the corrected test includes those bytes (hazard upper savings228972, crewthin upper233233, hazardfemale upper180994).

visual_campaign_voy15_mdr_blocks stayed CA_ACTIVE through210hostseconds with no engine error and final main-loop advance72. Current final zone sample had1534240bytes free (largest1342144); physical available393216. This passes the earlier allocation failure point but the briefing was still active, so a480-second follow-up is running. It does not yet prove scene completion, full route or FPS acceptance. Source header comments changed afterward only; executable code is identical to this tested build.

Voy15 lossless-storage runtime proof: visual_campaign_voy15_mdr_blocks_long ran480hostseconds, completed both authored briefing scenes, cleared the bars and returned first-person control. Final CA_ACTIVE7, no engine error, main-loop advance72, playercommand406281, health100/god16. Native player task slots were all -1. The last current zone sample had1124862bytes free/largest885760; physical available393216. Full route/exit and retail/FPS acceptance remain open. A separate read-only validator translated the final RAM snapshot using its CR3 and reconstructed every animation byte in10 resident MDR model records; all matched original loose assets exactly. Their summed resident saving is1721975bytes before the shared static workspace cost. Evidence: notes/evidence/campaign_20260909/voy15_resident_mdr_roundtrips.json and visual_campaign_voy15_mdr_blocks_long. Borg3 regression is next, then resume dn1 onward.

Borg3 regression on the lossless block build passed the150-second opening check: visual_campaign_borg3_mdr_blocks_regression ended CA_ACTIVE7, no engine error, main-loop advance56, command93611, health100/god16. The current final zone sample had2996484bytes free/largest1778931; physical available20480. Sampled actors, room and HUD looked intact. The sequential opening sweep resumed dn1 throughvoy20, then previously unchecked borg2/holodeck/voy1-5. These remain opening checks; no full-route or FPS claim.

Voy1 movie finding, user-confirmed frozen picture: the150-second opening probe ended during intro.bik, so main-loop delta0 was ambiguous. A330-second run returned naturally to bridge gameplay (final main-loop advance45, no engine error). Captures at160,195and230seconds were identical, proving the visible movie problem persisted while the engine later recovered. The runtime ISO intro.bik is byte-identical to the seed asset (SHA25640e012d12d45b890b3eb23512ac80f658a10d5a29e27991eec96f0c5b0a75afa); ffmpeg decodes changing frames and reports512x384,15FPS,109.066667seconds.

A process-local guest debugger captured the actual movie subimage upload with g_SPXBCinBinkFrame=2. D3DXLoadSurfaceFromMemory returned0x8007000e (E_OUTOFMEMORY) to dllTexSubImage2D. The return was ignored, so playback retained an old texture while Bink continued. Exact old XBE/map and debugger trace are retained in notes/evidence/campaign_20260909/voy1_movie_upload_failure. A new bounded full-size RGBA update path uses a tiled SDK texture lock and XGSwizzleRect directly, checks dimensions/format/backing capacity, waits for GPU use to finish, and logs frame hashes. It avoids D3DX temporary allocation and leaves movie duration/script sequencing unchanged. This candidate is BUILDING, not runtime-verified yet. The campaign sweep stopped atvoy1; voy2-5 remain unchecked.

Forgeboss opening returned to gameplay and remained live, but a late capture shows nearby geometry partly covering the weapon. Keep this flagged for closer visual/proximity review; full fight/exit are not qualified.

Movie upload fix runtime-verified, Sep 10: visual_campaign_voy1_movie_texture_fix ran 330 host seconds with XBE SHA256 9ce69199743d2de4e4084bff500ca31a045040d934b883e9d82a658076431b56 (build 09:27:42). Native captures show changing movie imagery, the Elite Force title, subsequent credits, and natural return to bridge gameplay with letterbox bars cleared. Final CA_ACTIVE=7, main-loop advance=46, movie status=stopped, engine error empty. Evidence is archived under notes/evidence/campaign_20260909/visual_campaign_voy1_movie_texture_fix. This supersedes the preceding BUILDING status. The visual/god run is not FPS or retail acceptance. Full campaign routes and voy2-5 opening checks remain open.

Comparison-tool correction, Sep 10: scripts/compare_sp_perf_probes.py previously read broad RAM-scan logs and derived rates from simulation realtime. It now requires current paused guest FPS-ring provenance, reads only .current_profiles.log, and computes rates from complete six-field ft records. Missing elapsed records, stale provenance, god mode, engine errors and missing liveness reject comparison. Different XBE identities are retained separately so actual code candidates can be compared, while map and mode must match. Nine regression tests passed, including a simulation-clock inflation case and a CLI fixture containing stale raw logs. Earlier results from this comparison tool require remeasurement; no new performance acceptance is claimed.

Voyager opening batch completed Sep 10 on movie-fix build 9ce69199: voy2/voy3/voy4/voy5 ended CA_ACTIVE with final main-loop advances 73/72/72/57 and no engine error. Voy2-4 were stationary entry observations; voy5 was still in its progressing briefing. Captures and runtime records are archived per map in notes/evidence/campaign_20260909. These do not qualify full routes or performance. Next running probe: visual_campaign_forgeboss_proximity, 180 seconds, diagnostic god mode and input-only replay scripts/tests/fixtures/forgeboss_proximity_input.txt; check the nearby geometry/weapon overlap after movement. No ICARUS state edits.

Shared SP/Holomatch refresh started Sep 10: scripts/build_xbox.ps1 -Target spmp -SkipStage -ReuseObjects, log build/research/letterbox_perf/campaign_shared_refresh_build.log. No emulator was active at launch. The prior efmp.xbe/map are preserved under notes/evidence/holomatch_20260910/pre_shared_refresh; prior XBE SHA256 4f61cfe74d89fc3f28a019de28c94a8445bbe6439b5d7275f6d7412a76826909. Build is still running; new Holomatch performance has not been measured. Comparison CLI now also requires verified multiplayer simulation, with positive and negative cases passing. Next: verify completed build identities and package freshness, then current-ring four-player hm_borg1 probe without god or synthetic input. SP campaign routes and retail acceptance remain open.

Shared refresh hit a packaging failure after both XBEs compiled: Resolve-Path.Path included the PowerShell FileSystem provider prefix for the UNC workspace, but FileInfo.FullName did not. Relative-path Substring then threw in the UI overlay copier. Changed five filesystem Resolve-Path uses in build_xbox.ps1 to ProviderPath, including the equivalent menu and stage/package paths. Verified all 54 UI paths resolve under the source root; build contract checks passed. Retry is running with reused objects, log campaign_shared_refresh_retry_build.log. Do not use the failed attempt as completed package provenance.

Packaging retry has passed the former UNC UI-copy failure and entered build_xbox_patch_pk3.py for xbox0.pk3. Both runtime build IDs passed freshness (default 10:10:46, efmp 10:11:53). Before regeneration, the ISO package hashes matched the retained release packages exactly: xbox0 bd7414363112cd98daecafcb902dd9cd949411d24b0e2bc4e79de8a91e316e9a; xbox1 be56a77b50e02d1ba3effd39a8a6db25e658ddb6853db04d7ce57441511c8cd8. Compare refreshed package hashes after build completion and repack the shared ISO if changed. Active build session 69255 remains live; do not launch another build or emulator against partial outputs.

While package generation remains live, started visual_campaign_voy5_briefing_completion for 480 host seconds using the completed default.xbe/map from the retry and unchanged, fingerprinted ISO assets. This is visual/god progression only and cannot qualify performance (the package generator is running concurrently). The package builder does not write the ISO or the completed XBE at this stage. Do not repack or start another emulator until this probe finishes and restores its ISO transaction. The test aims to establish natural briefing completion, which the 150-second opening did not prove.

Regenerated xbox0.pk3 completed and is byte-identical to the current ISO package (SHA256 bd7414363112cd98daecafcb902dd9cd949411d24b0e2bc4e79de8a91e316e9a, 323528660 bytes). No SP package replacement is needed on this evidence. Shared build continues through normal remaining asset stages; Voy5 long diagnostic remains active. Holomatch package identity still needs comparison after its regeneration.

Voy5 briefing completion verified: visual_campaign_voy5_briefing_completion ran 480 host seconds, returned naturally from briefing to HUD with bars cleared, final camera_active=0, CA_ACTIVE, player command_time=415477, god flag16/health100, loop counters26999->27071 (advance72), engine error empty. ISO restored. Archived proof in notes/evidence/campaign_20260909/visual_campaign_voy5_briefing_completion. This visual/god run overlapped asset generation and is not performance acceptance; full route and stasis1 transition remain open. Packaging session69255 still running through standard asset build; no other emulator is active after this probe.

Started visual_campaign_voy17_conversation_completion (450 seconds, visual/god) while the standard asset build remains active. Earlier voy17 evidence covered only a progressing conversation, not completion. Uses completed retry XBE/map and unchanged ISO assets; no authored-state overrides. This is not performance acceptance. Wait for the probe to restore the ISO before repacking or launching another emulator.

Campaign diagnostic timing triage (not acceptance): current-ring, complete-ft archived summaries identify forge4 as the slowest retained opening (minimum guest window36.4, average42.7; visual/god). Its retained active long frame was891ms, with871ms in client and17ms server. Late gameplay windows remain roughly44-46 guest FPS, so the issue is not just initial loading. Last zone free4956264/largest3243904. Prioritize a clean forge4 measurement and client/renderer profiling after mandatory four-player refresh; do not reduce fidelity based on the diagnostic numbers alone. Full triage is build/research/letterbox_perf/campaign_diagnostic_timing_triage.json.

Prepared targeted Forge4 profiling: run_sp_perf_probe.py now forwards --sample-eip-interval to the existing process-local XEMU monitor sampler. Sampling requires --diagnostic and a finite interval >=0.1 seconds, so it cannot qualify performance. Parser guards passed for acceptance-mode, excessive-frequency, NaN and negative requests; existing mode/ISO guards passed. Renderer detail fields in the Forge4 archive were disabled zeros and cannot identify the bottleneck. Use instruction samples with the matching map file for diagnosis after current Voy17 run; no production renderer changes made.

Voy17 completed its authored office/corridor sequence and transitioned naturally to forge1 in visual_campaign_voy17_conversation_completion (450s). Final loaded_map=forge1, CA_ACTIVE, camera_active=0, main-loop advance73, command_time23738, health100, god flag0 after natural transition, no engine error. Last scheduled image was the Forge1 loading screen; final guest memory establishes subsequent active gameplay. Archived proof closes the isolated voy17 scripted progression/exit check, not retail/performance. Started diagnostic_forge4_instruction_profile (180s, diagnostic/god/native captures, 0.2s guest EIP sampling) while standard packaging remains active; no timing acceptance from this run. Active emulator session97782; build69255. Wait for probe ISO restoration before any repack or additional emulator.

First Forge4 instruction diagnostic exposed harness interference: at approximately40hostseconds, automatic XBLog discovery entered repeated address scans and starved instruction sampling (only34 samples after30s remained while scan misses continued). Do not infer a bottleneck from that sparse set. Updated the wrapper so instruction-sampling diagnostics omit automatic log polling/discovery, retaining final named memory dumps and the sampler. Parser/mode guards pass. Let current session97782 finish/restores ISO before retrying with a distinct name; never run overlapping ISO writers. Build69255 remains active.

Explicitly aborted the interference-affected Forge4 diagnostic by stopping only XEMU PID27140 after verifying parent22712 belonged to diagnostic_forge4_instruction_profile. This is a harness-aborted run, not a game crash. Parent probe97782 is cleaning up; verify its terminal result and ISO restored=true before retry. Sampling wrapper now skips automatic XBLog discovery when EIP sampling is requested. No runtime performance result from the aborted run.

Forge4 sampling-only diagnostic completed/restored ISO: 407 observations over host40-180s; title69.5%, unresolved kernel routines30.5%. Largest individual title groups: RB_SurfaceAnim4.9%, dllDrawElements4.2%, RB_SurfaceFace3.7%, SV_AreaEntities_r3.2%; strstr2.2%. No single dominant function. Final forge4 CA_ACTIVE, loop advance39, no engine error. Saved matching linker map, run log and resolved profile under notes/evidence/campaign_20260909/diagnostic_forge4_sampling_only. Timing is diagnostic only (sampling, god mode and concurrent asset build). Need targeted instruction/caller inspection and clean A/B measurements before optimizing; no fidelity reduction justified. Current emulator run is terminal; build69255 remains active on Holomatch assets.

Forge4 sampled-instruction follow-up, Sep 10: preserved matching default.exe disassembly as diagnostic_forge4_sampling_only/skin_disassembly.txt. Of the 20 RB_SurfaceAnim observations, 12 land at guest 0x8bffe/0x8c002, the source-normal loads inside the first per-weight normal DotProduct. Source code/game/q_shared.h already implements float DotProduct with inline SSE; the generated skin loop repeats six reductions and scalar result spills per weight. This establishes existing instruction structure, not a proven optimization or a memory-latency diagnosis. Keep any prospective fused transform local to MDR skinning, preserve arithmetic/geometry behavior, and require clean before/after measurements before retention. No production change made. Build session69255 and its package child11388 were revalidated live with increasing CPU/I/O; four-player clean measurement remains next after packaging finishes.

Performance acceptance guard correction, Sep 10: run_sp_perf_probe.py recorded engine_error_message but its measured_map_window_pass did not reject an error or a missing error dump. The gate now requires an explicitly empty engine error and rejects current_fps_error. verify_perf_mode.py covers nonempty engine error, missing error evidence and invalid timing ring; all checks pass. This is a harness correction, not a game performance improvement. Shared package build69255 remains live; child11388 CPU and I/O continued increasing. Do not launch the clean four-player measurement until the build finishes and package identities are verified.

Public beta priority changed by user, Sep 10: stable four-player Holomatch consistently averaging above 20 FPS is sufficient to move toward testers; do not spend an extended optimization pass chasing 30 FPS before beta. Keep 30 FPS as the later-patch ideal. Updated AGENTS.md, GAME_TODO.md and HOLOMATCH_QUALIFICATION.md. Existing measured_map_window_pass still represents the stricter 30 FPS per-window target, so report beta readiness separately using repeated sustained-run averages and stability evidence; do not present that strict flag as the beta gate or silently lower it. Current package xbox1 hash matches the shared ISO exactly (be56a77b...), executable identities saved under notes/evidence/holomatch_20260910/shared_refresh_identity.json. Build69255 still processing assets. Voy20 ending-completion visual/god probe37918 is running; wait for its ISO restoration before any four-player probe.

Four-player beta soak queued, Sep 10: terminal session81471 waits for verified shared-build process7432 (build session69255) to exit, then requires the final package freshness/success log markers, exact efmp SHA c1cc426456361d7a21baeab903d19cacf4882e660691d5df9f6ff710329578f1 and unchanged xbox1 SHA be56a77b50e02d1ba3effd39a8a6db25e658ddb6853db04d7ce57441511c8cd8 before launching beta_4p_hm_borg1_shared_refresh for600seconds in mode mp. No god, visual polling or synthetic input. Do not start another emulator/ISO writer while this queued job is active. Voy20 probe37918 is terminal0 and ISO restored=true; evidence archived, ending remains black with HUD despite loop advance73/no engine error. Current beta requirement: consistently above20FPS average with stability; close performance item once verified, do not retain deferred30FPS work.

Voy20 ending source follow-up: canonical PAK0 real_scripts/voy20/epilogue.IBI ends with authored fade-out, SET_VIDEO_PLAY st_21, then SET_CLOSINGCREDITS. Saved bounded string evidence in the run archive ending_contract.json. Active code/ui/ui_ef_lifecycle.cpp:UI_EFSP_ConsoleCommand accepts ui_ef_ commands and openmenu, but lacks ui_closingcredits; the latter is emitted by game Q3_Interface.cpp. Runtime command execution was not captured, so do not claim this alone proves the entire black-screen cause or that st_21 played successfully. Preserve the movie/credits order when fixing. Build69255 remains live in soundbank child25180; queued clean four-player session81471 still waits for build completion, and must remain the sole next emulator owner.

Voy20 ending movie package check: current shared ISO BaseEF/video/st_21.bik exists, 2,882,204 bytes, SHA25631b6b57ad21f3b998f45574cd522cbc3cd5e91f83e40598d9e6f36a0a5d6088e, byte-identical to the canonical seed. Saved movie_identity.json beside ending_contract.json. Final diagnostic movie_status1 means stopped (NS_BV_STOPPED); frame counter1 does not establish successful playback. Missing movie file is ruled out, not decode/display failure. No source edits or audio-quality work.

Shared build69255 completed successfully (exit0); final Active release PK3 freshness ok and staging-skipped markers verified. Queued job81471 passed executable/package identities and launched beta_4p_hm_borg1_shared_refresh, 600seconds, four human split views plus four bots, no synthetic controls/god/screenshot polling. It is now the sole active emulator/ISO owner; wait for terminal completion/restoration before other runs. Do not interpret 30FPS strict per-window flag as the revised beta average threshold. No further four-player optimization item should remain once stable repeated20+FPS is established.

User rejected stationary/no-proven-bot workload Sep10. Explicitly stopped owned XEMU2392 (parent2208) for beta_4p_hm_borg1_shared_refresh; wrapper81471 completed/restored. Do NOT count its result as beta evidence. Its bot_minplayers4 with four humans did not establish four bots; earlier claims of four bots were incorrect. Started workload verification session88771, verify_4p_movement_full_bots,180sec visual-only: all four process-local virtual control lanes enabled, bot_minplayers0, four explicit addbot commands (1_of_12,2_of_3,3_of_6,4_of_12), eight slots, frag/time limits0. Verify actual movement/combat and all eight participants before a longer timing run. No retail/performance acceptance from this visual setup check. Wait for session88771 ISO restoration before another emulator.

User closed verify_4p_movement_full_bots after reporting no visible bots. Wrapper88771 terminal0 and ISO restored. Do not count this as workload or performance acceptance. Screenshots prove moving/firing local views but not an AI roster; addbot queuing is insufficient. Obtain runtime client SVF_BOT flags/connected states and command-dispatch evidence before claiming full bots or rerunning a long benchmark.

Bot absence CONFIRMED by completed diagnose_4p_bot_startup (session71492 terminal0/restored). Official level: connected4, playing4. Bot proof[11]=8 add attempts, [12]=0 name-lookup successes, [9]=0 allocations, [13/14]=0 connect/begin. Thus no bots spawned; failure is at G_GetBotInfoByName, not uncertain visibility. Packaged xbox1 scripts/bots.txt contains all four requested names. Eight attempts arise from current case-duplicate command file reads; investigate bot-definition loading/runtime lookup before another workload run. Saved summary, transaction and bot/game/command dumps to notes/evidence/holomatch_20260910/bot_startup_failure. Probe wrapper now captures existing 32-word bot proof without a game rebuild; mode tests pass. No active emulator now.

Bot-definition diagnosis: added bounded g_SPXBBotDefinitionProof[8] plus startup log records in official g_bot.c. Fields: load-call count, bot_enable, file-open count, last length, handle, parsed count, count at lookup (last field reserved). No bot behavior changed. Probe captures this symbol when present. Executable-only shared build session84937 is running with -Target spmp -SkipAssets -SkipStage -ReuseObjects, log bot_definition_diagnostic_build.log. Packages unchanged but source freshness is diagnostic-only until normal package refresh. Do not run another build in parallel. No active emulator. Next: after successful build, repeat short bot startup diagnostic and inspect definitions state before any full benchmark.

Diagnostic build84937 completed0 with build IDs default11:49:21/efmp11:50:13; skipped asset packaging intentionally. diagnose_bot_definitions session46131 completed0/restored. Bot-definition proof=(1,1,3,0,1,12,12,0): enabled, three file-open attempts, final parsed count12 and count-at-lookup12. All eight name lookups still fail. Current ISO loose BaseEF/scripts/bots.txt contains57 definitions including all requested Borg names, matching the source listing. Next inspect actual file/parse contents at load (current log mirror rolled past startup), rather than assuming bot_enable or missing assets. No active emulator/build; no full workload proof. Evidence saved under holomatch_20260910/bot_definition_loading.

Added bounded2KB g_SPXBBotDefinitionText startup record with actual file names/lengths, source prefix and first16 parsed names. Append truncates safely when full; no gameplay/AI behavior changed. Wrapper collects its final contents. Executable-only rebuild15391 is live (bot_definition_text_build.log). Source helper was tightened while build was early; require final freshness success or rerun with reused objects if it detects stale compilation. Next short probe should explain runtime12 definitions vs packaged57. No emulator active.

Root-cause candidate from diagnose_bot_definition_text: scripts/bots.txt opens handle1 length0, while scripts/xpack.bot reads936bytes and yields12 expansion names. ISO base bots.txt is nonempty; base bot names absent because no bytes were parsed. Run77998 completed/restored. Source loose file path requested unbuffered handles for scripts/*.txt and bots/* text; FS_filelength seeks to an unaligned EOF without checking failure. Added these bot text paths to existing buffered whole-file policy (scripts txt/bot/arena, bots/ alongside botfiles/). Needs runtime confirmation; do not declare fixed yet. Executable-only build4559 is running, log bot_buffered_text_build.log. No simultaneous emulator; after build, rerun short bot proof and require57 base definitions plus expansion and successful allocation/connect/begin, then full workload verification. Previous text diagnostic freshness retry22241 passed before run; first15391 failed freshness and was superseded.

Buffered bot-text fix runtime-confirmed: build4559 passed (default12:04:24,efmp12:05:20). verify_bot_buffered_text18697 completed0/restored. Base bots.txt reads3869bytes/57defs; xpack adds12. Existing file-list duplication reads base list again (126total), a separate duplication issue. Bot proof allocation/connect/begin confirms4bots; live bot count[15]=4; official connected/playing=8; game live/no error. Native captures show changed positions and combat, all4 command lanes have nonzero forward/strafe. Short visual run average34.06guestFPS/min19.60 is not acceptance. Archived buffered_text_fix evidence. Started600sec beta_4p_full_bots_moving session30931 with exactsame4humans+4bots, virtual movement/firing, no screenshots/polling/god. Sole active emulator/ISO writer. Require full roster and movement evidence again before counting result against user's20+average threshold; do not chase30 or retain deferred30item once verified.

Workload acceptance guard now requires eight connected/playing, bot startup connect/begin counts>=4 and live bot count4, plus four active nonzero movement-command lanes. verify_mp_workload is explicitly endpoint evidence, not route/sustained-motion proof. Six positive/missing-roster/no-bots/missing-bots/stationary-lane/missing-command cases pass with existing tests. Active probe30931 loaded the prior Python module before this edit; re-evaluate its completed summary through the new helper after it finishes instead of trusting its old strict pass flag. No game source edits/build activity during the soak.


## Letterbox item closed — September 11, 2026

Closed the black-bars / incomplete right-edge coverage item at the user's explicit request (former GAME_TODO.md item 2). Removed it from the open shortlist and detailed open work. This records the user's closure decision; no additional retail validation was performed for this documentation update.
