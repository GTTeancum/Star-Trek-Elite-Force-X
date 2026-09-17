# Jedi Academy Xbox Build - Project Notes

## Stabilization authorized — 2026-09-17

User accepted the audit and explicitly authorized necessary stabilization,
commit and push. Read `notes/stabilization_2026-09-17.md` for final results.
Both builds pass; frozen SP b16e4703... and MP 3591e099... bundles are preserved.
480-second natural-intro co-op and Deck02 two-save/two-load checks finish live,
error-clear and ISO-restored. Co-op's published physical reserve is still only
4 KiB; these bounded checks do not close memory pressure or full VV stability.
User subsequently requested higher process priority: use the harness option
`--process-priority above-normal` on this PC; no FPS gain was claimed.
The unmeasured co-op lighting policy has been removed, restoring world
lightmaps; prior split-screen economy already suppresses dynamic lights and
is unchanged. No new FPS experiment is authorized by this checkpoint.
Keep SDK DSP bytes local: the build generates `snd_dsp_image.h` from the
installed XDK 5558 with a pinned hash, preserving the existing audio bytes.
Full VV and retail qualification remain open. Earlier no-commit statements
are historical; this explicit request authorizes this stabilization checkpoint.

## Independent audit — 2026-09-17

Read `notes/sol_audit_2026-09-17.md` before resuming this work. The user requested
an audit, not a handoff continuation, and clarified that the concern was Sol's
inconsistent conclusions, not a newly reported crash. Latest prior instruction:
co-op only, clear wins only, no speculative candidate hunt. The lighting change
was authorized but still has NO measured on/off FPS benefit. The 0.918x array
result is limited guest-clock evidence from differing gameplay intervals, not
a precise native-XEMU FPS regression. Arrays are removed; push buffer is 1 MiB.
Preserve supported memory work: audit independently verified all 6,619,208
animation bytes in four models from the saved candidate guest RAM. This does
not close repeated-transition/save qualification or full Virtual Voyager work.
No new build/runtime test or game-code change occurred during the audit.
Do not mix current release, restored minisoak and older VV manual-profile builds.

## RECEIVING PC RESUMED — 2026-09-13

User explicitly authorized continuing the handoff on RedRocket (LRPC). Read
`notes/receiver_redrocket_2026-09-13.md` for current setup, evidence and active
work. The main handoff predates additional TARDIS RE and the unqualified index
streaming candidate; read `build/research/ps2_re/RECEIVER_PROGRESS.md` too.
Memory reduction remains the gate before VV station/UI work. Preserve transferred
evidence with `run_sp_perf_probe.py --preserve-existing-artifacts`; do not run
automatic generated-file cleanup. Use complete immutable symbol bundles.
The old-PC stop below remains historical and still applies to that PC.

## STOPPED FOR TRANSFER — 2026-09-12

User stopped all work and is offloading to another PC/Astra task. Read HANDOFF_ASTRA.md and notes/handoff_2026-09-12/SETUP.md first; receiving goal is in notes/handoff_2026-09-12/NEXT_ASTRA_PROMPT.md. Old goal is paused and unfinished; owned RE/emulator work is stopped. Do not auto-resume on the old PC. Priority remains PS2 RE and demonstrated memory-footprint reduction before individual Virtual Voyager systems. Historical completion/manual-session statements below do not supersede this handoff.

## Active goal expansion - PS2 executable RE, 2026-09-12
- PRIORITY OVERRIDE: PS2 RE and demonstrated memory-footprint reduction come FAR before individual Virtual Voyager systems. Pause station/menu implementation and interaction qualification until the footprint is lowered. Do not spend further runs on astrometrics completeness now. Existing fixes/evidence remain staged; memory-focused harnesses are authorized.
- SCOPE CORRECTION: finish the full Virtual Voyager pipeline and relieve memory pressure, not a one-off terminal fix. Astrometrics/widescreen are sub-items. Cover New Game/launch, every supplied VV map, authored travel and stations, progression, save/load, asset residency and repeated transitions; use PS2 RE to guide supported, fidelity-preserving memory improvements. Previous testing-build completion excludes full interactions and does not close this goal.

- User requires all extracted PS2 files and RE working artifacts, including analysis projects, to stay inside this repository. Use build/research/ps2_re; do not place extracted copies or Ghidra projects in external temp directories.
- User explicitly authorizes reverse engineering the PS2 executable and supplied root ISO Star Trek - Voyager - Elite Force (USA PS2).iso. Preserve source ISO; extract copies and compare maps, memory, loading and rendering for transferable efficiency ideas. This is part of the new active goal alongside astrometrics/widescreen harness validation; no user participation needed. Audio remains fixed/untouched.

## Latest Virtual Voyager follow-up — 2026-09-12
- LATEST MANUAL TEST: astrometrics terminal soft-lock and incorrect gameplay widescreen remain OPEN; previous local passes do not qualify these. See the manual-test section of notes/virtual_voyager_followup_2026-09-12.md. Preserve active user session/logging.
- User says audio is fixed: do not investigate or alter audio. Old silence findings are superseded; muddy 3D VO remains separately paused.
- Follow-up implemented and locally tested: restore 18 Voyager door/floor/wall DDS textures from source (up to 256px), correct EF non-solid portal open/close accounting, reduce SP model texture reserve from 4MiB to 2MiB, add release allocator bounds checks and compact pool diagnostics. Holomatch keeps 4MiB.
- IMPORTANT CORRECTION: active retail JkaFakeglSetDDSUploadPicmip is a no-op. The earlier 64px DDS upload diagnosis inspected an inactive backend. Actual loss was package conversion's 128px cap; r_picmip is unchanged.
- Both Xbox builds pass. Host portal regression reproduces old failure and passes 10,000 cycles with fix; allocator release bounds checks pass. New image passes deck02 save/load and deck04 transporter to populated deck01; captures individually reviewed, final engine errors empty, live, ISO restored. These short follow-ups do not repeat all 20 previous map checks or establish retail hardware qualification.
- Bridge check: zone free minimum 6,450,350 bytes, largest free block minimum 4,492,581 bytes; final static textures 7,999,872/10,485,760 and skin peak 223,616/2,097,152 bytes with zero swaps/fetches. Reduced reserve makes 2MiB available to the zone, not necessarily physical free RAM.
- Release xbox0.pk3 now uses tested detail package; baseline backup preserved. Next manual launch MUST read build/research/virtual_voyager/manual_profile.json: followup ISO, matching immutable map/EXE/XBE bundle, original controller binding, 480p/16:9/native scale/HDTV, AA off. Emulator closed. Do not reuse old launcher unchanged.
- Symbol resolution requires sibling EXE AND XBE alongside MAP; a map-only bundle silently returns linked rather than runtime addresses. First deck02 continuous flight stream excluded for that reason; final dumps and independent corrected allocator read valid. Transporter recorder is valid.
- See notes/virtual_voyager_followup_2026-09-12.md for exact scope and evidence. No commit/push/publication.

## Virtual Voyager testing build completed — 2026-09-12
- Approved menu, PAK1–PAK3/native BSP package, all20 map local visual/load checks,
  nineteen playable-map save roundtrips and normal UI save/load are verified.
- Final paired testing ISO: build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_testing.iso.
- Normal final-image New Game -> Virtual Voyager boot passed; release/BaseEF/xbox0.pk3
  promoted with verified baseline backup. See notes/virtual_voyager_testing_2026-09-12.md.
- Authored transporter room-entry trigger clears destination: select after entering.
  No script workaround needed. Holodeck return and turbolift hub persistence passed.
- Brig retains cinematic/death save restrictions. XEMU local checks do not establish
  retail Xbox qualification or a complete route/station interaction playthrough.
- Former TODO7 is closed; remaining items renumbered. Do not reopen paused audio
  or accepted co-op/MP work. No commit/push/publication performed.

## Performance Benchmark Rules — User Instruction, 2026-09-09
- To-do format required by user: keep the top numbered shortlist to one short,
  plain-language sentence per item for at-a-glance reading. Put implementation
  details, evidence and verification checklists in matching sections below.
  Preserve this format when closing, renumbering or cleaning up items.
- LATEST TRACKER CLEANUP, Sep 11: user closed former shortlist#2(SP loading/FPS
  optimization) and#10(additional Holomatch30FPS low-window pass). No deferred
  optimization pass remains. This does not prove prior numeric targets were
  met. Do not reopen from historical notes/goals. Current shortlist renumbered;
  use task names to avoid confusing old/new numbers. Co-op remains accepted
  as-is. See notes/todo_cleanup_2026-09-11.md for the former tracker.
- LATEST USER DECISION, Sep 11: wrap up; cooperative is accepted AS-IS.
  Further co-op optimization and testing are stopped. This overrides older
  active co-op goals/checklists below. The30FPS target was NOT achieved; user
  accepts its limitations. Do not automatically resume, defer another pass,
  promote experiments, or switch to other game work. Wait for a user request.
  No commit/push/publication. See notes/coop_accepted_as_is_2026-09-11.md.
  Keep XEMU output audible0.65; muddy3D voice tuning remains paused.
- Sep 11 co-op CPU skeletal-cache verifier FAILED after1,053,050 matching
  vertices, latched off with original output retained. No FPS qualification.
  Separate default-OFF path uses exact full palette bytes and334832boundedbytes.
  Failed source/evidence archived under mdr_skin_v1_failed and mdr_skin_detail_failed.
  Detailed failure had equal mesh/palette bytes and recorded FPU controls.
  Later numerical audit passed declared position1e-4/normal1e-6 error limits;
  this is NOT bit-exact verification and does not identify the rounding cause.
  Same-binary cache ON27.31mean/23.13worst versus OFF25.82mean/20.26worst
  native FPS, with different worst-window model workloads. Goal remains unmet.
  All three roundoff audit/timing runs terminal, error-clear and ISO restored;
  239 files archived in notes/evidence/coop_20260911/mdr_roundoff.
  SIMD kernel now implemented, defaultOFF/actualco-op guards. Xbox verifier
  compared18,949,392vertices, all exact (16,372,585 during gameplay), no failures.
  Verifier/fast/OFF all terminal, live/error-clear and restored;248files archived
  in notes/evidence/coop_20260911/mdr_simd. SIMD ON21.79mean/13.61worst versus
  OFF24.76mean/20.33worst, with materially higher external GPU load ON and
  different combat. No demonstrated FPS gain; remains defaultOFF. No owned
  SIMD test active. Co-op light candidate now compiled: r_efCoopLight default0,
  1combined,2device-state verification/restoration,3instrumented original control.
  Host90000comparisons passed; fresh bothbuild60121 passed. Xbox verifier
  probe43915/GPU33341 now terminal, ISO restored. Xbox verification FAILED
  after85395comparisons with1mismatch,0API failures; original state restored
  and candidate latched off. Do not speed-test until exact differing fields
  are captured and resolved. Archive failed candidate before production edits.
  Xbox diffuse lighting is HARDWARE, not PC CPU vertex-color lighting; preserve
  that distinction in any static-prop batching work. See latest notes.
  Frame-begin hook belongs in active retail_xbox/tr_cmds_retail.cpp.
- Sep 11 shadow-cache A/B archived in notes/evidence/coop_20260911/shadow_cache.
  Enabled25.83mean/19.72worst versus OFF24.36mean/21.11worst native FPS:
  reduced CPU projection cost, but NO demonstrated low-window FPS gain.
  Both runs live/error-clear/restored with two moving/firing players; host
  counter invalid instances in OFF limit the context comparison. See notes.
  Next candidate is co-op-only exact CPU skeletal-vertex reuse; default OFF.
- Sep 11 actor profiling isolates ground-shadow polygon projection:3.25Mof5.19M
  actor cycles in the slow window. New co-op-only/defaultOFF shadow cache uses
  exact geometry keys,31KBbounded memory,original fallback and byte verifier.
  23,928gameplay cache-hit comparisons passed with0mismatches; timing pair
  completed and archived. See latest notes before
  further edits. Goal unmet; preserve audio pause and ordinarySP/qualifiedMP.
- Sep 11 co-op client-tail cost is spark-trail updates: worst sample155trails,
  8.21million guest cycles; drawing/freeing/culling effects comparatively small.
  New default-OFF cg_efCoopFxTrace filters impossible particle collisions only,
  behind actual co-op/2player guards.40k host trace-loop comparisons passed;
  Xbox verification passed7.14M gameplay skips; same-pair timing22.44vs20.02mean, worst18.04vs16.05. Details in notes/coop_optimization_2026-09-11.md.
  Goal remains unmet; preserve original SP/MP paths and audio pause.
- Sep 11 user authorizes larger co-op renderer changes behind conditional paths.
  Gate new behavior on actual co-op mode and split rendering; compile it out of
  SP-hosted MP where practical. Preserve ordinary SP and qualified Holomatch
  paths and visual fidelity. Co-op shares the SP executable, so a compile-time
  SP guard alone does not isolate normal single player. No new approval needed
  for this larger work; preserve audio pause and verify both fresh builds.
- Sep 11 hybrid indices experiment: full MD3/MDR base passes indexed, world/
  brush expansion retained. Same-binary short trials: enabled19.16FPS mean,
  slowest14.80; disabled17.49mean, slowest14.75. Host load and combat differ.
  No proven low-window gain; all co-op experiments remain OFF by default.
  User's 78MSPF capture matches native frame7480 with599draws,409vertex uploads,
  one index upload. Counters do not measure driver/API time. Next larger work
  targets model residency and compatible batching; profile the remaining host
  bottleneck rather than assigning all producer waits to their current model.
- Sep 11 co-op resident-slab/combined-array experiment remains OFF by default.
  It passed geometry verification but timed 18.45 native FPS, worst observed
  window 13.63 FPS (complete 10-second lower bound 13.1): the 30 FPS goal is unmet.
  The worst window has 455 model draws vs 315 elsewhere, while world costs are
  comparatively steady. Focus subsequent work on measured model costs.
  Bounded cache metadata adds 135112 bytes of RAM despite reusing the existing
  2 MiB vertex slabs. Current host GPU context differs from earlier quiet runs.
  Read notes/coop_optimization_2026-09-11.md for exact evidence and the read-only
  ordered-strip topology assessment; strips are NOT implemented or qualified.
- Sep 11 co-op HUD regression: keep the full-screen CG_Draw2D call AND its
  diagnostic breadcrumbs braced inside the single-view else. Falling through
  after the two split HUD passes draws a third, stretched HUD over P2.
  Verify with scripts/tests/verify_coop_hud_dispatch.py. Independent P2 HUD
  player-state binding remains open; viewport separation alone does not fix it.
- Sep 11 resumed after user reported GPU freed up. Check host GPU contention
  before comparisons and record GPU counters throughout each short test.
  The earlier pause is lifted; the co-op performance goal remains unmet.
  See notes/coop_optimization_2026-09-11.md for the prior contention evidence.
- Co-op workload correction, Sep 11: P2 test-input mode1 only moves. Mode3
  adds firing and weapon cycling; verify actual server firing events, weapon
  changes and ammo for BOTH players. An attack command alone is not shooting
  proof. Older movement-only runs cannot qualify sustained full combat.
  Preserve the P2 weapon-request latch through pmove's delayed switch; resetting
  cmd.weapon to the equipped weapon each frame cancels the requested change.
- Sep 11 test-length update: shorten repeated same-route borg1 diagnostics; use
  roughly two minutes of confirmed gameplay after the excluded text intro.
  Start with 270 host seconds and inspect actual gameplay coverage. Reserve
  longer runs for changed levels, promising candidates, and final stability
  qualification. Repeated loops are not additional route or combat coverage.
- Latest goal edit, Sep 11: optimize co-op until every complete ten-second
  confirmed-gameplay window averages above 30 native XEMU FPS under sustained
  full workload, preserving current visuals and verifying runtime defaults and
  stability. This supersedes the sustained-average-only co-op criterion below.
  Compare like-for-like co-op workloads; the qualified MP resident-triangle
  baseline is renderer evidence, not a matched co-op performance baseline.
  The app goal now reflects co-op; no goal-object replacement remains pending.
- Latest user direction, Sep 11: stop the current MP pass at the animation-cache
  candidate's pass/fail checkpoint, then focus on co-op. Target a sustained
  30 FPS average during actual two-player gameplay. Use native frame-counter
  deltas and rolling slow-window analysis to isolate offenders, compare matched
  workloads, and preserve visual fidelity and ICARUS sequencing. Report lows
  separately; this is not a requirement that every ten-second window exceed 30.
  Audio stays paused. This supersedes the older co-op pause/priority below for
  current optimization work. The unmet MP low-window target is paused, not passed.
- Sep 11 new optimization goal: raise the lowest ten-second confirmed-gameplay
  averages above 30 native XEMU FPS in sustained four-player Holomatch tests.
  Keep four moving local players and four actual bots, preserve current visuals,
  and isolate root causes of the slow windows. This is a new requested pass;
  the earlier completed pass remains historical. Audio, SP and co-op stay paused.
- Sep 10 workload requirement: four-player Holomatch tests MUST include actual
  movement and the full bot count. Stationary empty matches do not qualify.
  Verify participant types/counts and movement at runtime, not only requested
  cvars. The eight-slot setup is four local players plus four bots; bot_minplayers
  counts total participants and is not a requested number of bots. Process-local
  virtual input is authorized for this workload; keep it out of release defaults.
- Public beta priority update, Sep 10: stable four-player Holomatch consistently
  averaging above 20 FPS is sufficient to move toward public beta testing.
  Close the performance list item once that is verified. Do not retain deferred
  30 FPS work; open a new item only when the user requests it. Report slow windows/stutter separately and
  keep XEMU guest timing distinct from host presentation and retail Xbox.
  This supersedes the four-player 30 FPS beta gate below; SP and co-op priorities
  otherwise remain unchanged. Public distribution still requires user authorization.
- Exclude borg1's scrolling-text opening, where the camera is looking at a
  wall, from FPS benchmarks. It inflates results and must never count toward
  gameplay performance acceptance.
- Exclude any FPS sample window that overlaps that opening, not just samples
  whose final frame occurs during it. Use runtime scene/text-state evidence
  to establish the cutoff; do not assume a fixed host-time delay.
- Separate loading, menus, and other non-gameplay time from gameplay FPS.
  Report averages and slow samples for the retained gameplay window.
- Blocking fullscreen movies can begin and end between two active CL_Frame
  checks. Latch their overlapping FPS window as excluded at successful movie
  startup; do not count a four-second movie as one gameplay frame. Keep shader
  videos and real gameplay stalls in gameplay measurements. Stasis1's authored
  opening requests st_06; preserve that movie and ICARUS sequencing.
- Label XEMU guest-clock FPS separately from frames per host wall second.
  The guest clock can run slower than wall time; guest FPS alone does not
  prove the speed visible in XEMU. Native monitor polling is diagnostic and
  must be identified as such, with intro-overlap exclusion still enforced.
- Legacy `fps=` records use `cls.realtime`, a simulation clock affected by
  time scaling and the 200 ms frame clamp. New acceptance must use complete
  six-field `ft=` records (count/elapsed/max/p95 upper/p99 upper/over50),
  calculating FPS from count and elapsed `Sys_Milliseconds` time. Neither
  guest elapsed FPS nor simulation FPS alone proves host presentation speed.
- Current combined letterbox/optimization goal: 45+ FPS across SP levels
  and 30+ FPS in four-player Holomatch, with visual fidelity close to its
  current state. SP and four-player Holomatch are mandatory release priorities.
  User priority update, Sep 9: co-op is the lowest tier and may remain clearly
  labeled "not finished"; its 30+ FPS target is aspirational and must not block
  the required modes. Retail Xbox is the final performance authority.
- Further 3D audio work remains paused until the user explicitly resumes it.
- Latest focus, Sep 9: stay on stasis1 until it is playable. A correct opening
  screenshot alone does not establish gameplay progression or stability.

## Holomatch Slowdown Diagnosis — 2026-09-11
- Do not assume late-match XEMU slowdowns are proportional to draw count or
  caused by texture quality. The pinned 0.8.134 binary's element-cache miss path
  scans all 51,200 entries after saturation. Live thread PCs and the full cache
  header confirmed this path; a separate cache-guard experiment gained about
  13 native XEMU FPS with game resident-world storage disabled. The same source
  loop is present in 0.8.136. Evidence: `notes/mp_resident_triangles_2026-09-11.md`.
- Keep the original XEMU pin unchanged. The separately hashed LRU-guard copy is
  diagnostic-only; identify its variant in results and do not attribute its
  gains to Xbox game code or include it in a beta package.
- Resident expanded world triangles gained about 15 native XEMU FPS on the
  original emulator and completed a 20-minute full-workload soak. Preserve the
  exact vertex/material/topology checks, bounded allocation and fenced map
  reset. Retail performance remains a separate check; expanded triangles can
  change GPU vertex reuse even when the image is identical.
- Use XEMU's native frame-counter deltas inside confirmed gameplay intervals
  for its displayed-FPS comparison. Never invert MSPF or substitute guest time.
  Stream the FPS publication ring during long tests so early windows are not
  lost; the final ring alone retains only 64 records.
- The legacy FPS line's `bots` field counts bot-controlled local viewports,
  not opposing server bots. Four human views legitimately log `bots=0` while
  four opponents are active. Verify the actual eight-player server roster,
  bot startup/AI evidence and movement-command lanes.
- The special host-PC sampler briefly suspends selected XEMU threads and must
  be confined to a dedicated diagnostic interval, never FPS acceptance. It is
  not desktop input or UI control. Ordinary flight recording remains read-only.

## Writable Texture Capacity — 2026-09-09
- Preserve the declared dimensions of `*screen`; it is writable GPU capture
  storage, not artwork to shrink with texture-quality caps. The stasis1
  transition copies 512x256 pixels into it. Shrinking its allocation while
  resizing only its texture header overwrote neighboring lightmaps.
- Validate the SDK-required capture size against the backing allocation before
  submitting GPU writes. Evidence: `notes/letterbox_optimization_2026-09-09.md`.
- Communicator rendering fixtures are diagnostic-only and do not qualify
  natural dialogue activation, audio, lip sync, or campaign performance.

## Current Holomatch Override - 2026-07-22
- Active Holomatch development starts from the working Elite Force SP engine in `code\`.
- Build Holomatch with `scripts\build_xbox.ps1 -Target spmp`.
- The active Holomatch development artifact is `build\release\efmp.xbe`; current qualification packages it into the shared XEMU ISO and the single bounded hardware stage.
- Runtime data is staged under `BaseEF`; the active Holomatch package is `BaseEF\xbox1.pk3`.
- `codemp\` is historical/deprecated for this project phase and must not be a build, link, include, or runtime dependency for SP-hosted `efmp.xbe`.
- `default.xbe` remains the SP/co-op executable and must not be overwritten by Holomatch work.
- Current Holomatch tests boot straight to `hm_borg1` with bots; menus are future work.
- The authoritative current status files are `GAME_TODO.md` and `HOLOMATCH_QUALIFICATION.md`.
- Use XEMU/LLE for the current unified SP/co-op/Holomatch qualification. The
  unmodified retail Jedi Academy multiplayer XBE crashes CXBX-R, so CXBX-R is
  not a compatibility or regression authority for the JA-derived renderer and
  must not be used to qualify that path. Retail Xbox hardware is the final
  performance authority.

## Development Workflow
- **Programmer:** Codex (AI) — all code changes are made by Codex
- **Compile & Test:** User compiles and runs when Codex asks
- **Never commit or push** without explicit user instruction

## ICARUS Script Authority
- Never write production code that supersedes, bypasses, compensates for, or re-times behavior authored by ICARUS scripts.
- Treat ICARUS task sequencing, spawns, AI state, cameras, dialogue waits, and map progression as authoritative. Fix contract violations in the responsible engine subsystem (such as audio, rendering, input, asset loading, or task-state reporting) while leaving the authored script behavior intact.
- Synthetic test behavior that intentionally overrides script-owned state must remain isolated behind an explicit diagnostic-only harness and must never be enabled during campaign qualification or packaged in a release build.

## Testing Environment
- **Hardware:** Retail Xbox only (no dev kit, no debugger attach)
- **Emulator:** XEMU/LLE for active qualification. CXBX-R is historical-only for this JA-derived renderer and is not a regression authority.
- **Target build:** Release only — all other configurations (Debug, FinalBuild, DemoDebug, DemoRelease, DemoFinal, SHDebug, etc.) are removed from all .vcproj and .sln files
- **Diagnostic tool:** Debug log output — the SP codebase has logging strings that write to a log file; this is the **only** practical way to diagnose runtime issues
- All new code paths must be instrumented with log output before asking the user to test
- Do not assume a crash cause — log before and after suspect calls so the log tells us where execution stopped
- **Canonical controller config:** `C:\Games\Emulators\CXBX\Star-Trek-Elite-Force-X\BaseEF\default.cfg` is read-only to Codex. It may be inspected and compared, but must never be edited, overwritten, generated, staged into, or used as a package output target.

## Repository Structure
- **SP (Single Player):** `code/` — `JediAcademy.sln`
- **Historical MP (deprecated):** `codemp/` — `JKA_mp.sln`
- **XDK:** clean, unmodified 5558 at `C:\XDK_5558\XDK\`; do not use the modified 5849 tree for active builds.

## Toolchain
- **Compiler:** VS2005 (Microsoft Visual Studio 8)
- **XDK Version:** 5558
- **Platform:** Win32 (renamed from Xenon/Xbox in all vcproj/sln files)
- **MASM:** `C:\Program Files (x86)\Microsoft Visual Studio 8\VC\bin\ml.exe`
- **imagebld:** `C:\XDK\xbox\bin\imagebld.exe`

---

## SP Build Status — COMPLETE ✅

Release config builds and produces XBE:
- `Release` → `ja-release.exe` + `ja-release.xbe`

### Key SP Fixes Applied
- `Xenon` → `Win32` platform rename in all vcproj/sln files
- D3D9 → D3D8 headers in `win32/glw_win_dx8.h`, `win_lighteffects.h`, `win_highdynamicrange.h`, `win_qgl_dx8.cpp`
- Bink/RAD stub headers: `client/bink.h`, `client/RAD.h`
- `BinkVideo.cpp/h`: `IDirect3DTexture9` → `IDirect3DTexture8`, `s32` → `S32`, OpenFlags stubbed
- wchar_t casts in `sv_savegame.cpp`, `xb_settings.cpp`, `ui_main.cpp`
- ~200 for-loop variable scope fixes across cgame, game, renderer, ghoul2, server files
- `G2_misc.cpp`: multiple hoisted `int i` fixes

### SP Runtime Fixes Applied
- `Sys_InitFileCodes`: changed from `Com_Error(ERR_DROP)` to warning on failure (filecode cache is non-critical)
- `Com_Error(ERR_DROP)`: guarded `CL_FlushMemory`/`CL_StartHunkUsers` with `com_cl_running` check (prevents crash during early-init errors)
- `win_qgl_dx8.cpp`: NULL device guards on `dllBeginFrame`, `dllMaterialfv`, QGL_Init `SetMaterial`, `GLW_Shutdown` `Release`
- `patchxbe.py`: added empty PE section removal (`.rsrc`) from MP version — prevents imagebld failures
- XBLog breadcrumbs: GOB init/open, Sys_StreamInit, TheGhoul2InfoArray, Com_Frame first 3 frames, granular CL_Disconnect steps

### SP Stub Files (`code/x_exe/`)
Two files must exist in `code/x_exe/`:

**`xbox_asm_stubs.asm`** — provides:
- `__ftol2_sse` — x87 truncation
- `__ftol2` — same
- `___CxxFrameHandler3` — returns ExceptionContinueSearch
- `__except_handler4` — buffer security stub
- `_WinMainCRTStartup` — Xbox startup entry point

**`xbox_crt_stubs.cpp`** — provides:
- `_strcmpi` → forwards to `_stricmp`

### SP Linker Settings (Release)
- Xbox system libraries come from the unmodified XDK 5558 tree at `C:\XDK_5558\XDK\xbox\lib`.
- Keep the active Xbox renderer link entirely on one XDK version; do not mix 5558 headers or objects with 5849 libraries.
- `IgnoreDefaultLibraryNames`: msvcrt/libcmt variants
- `/FORCE:MULTIPLE`
- `BufferSecurityCheck="FALSE"`
- `d3d8.lib` (retail runtime). `d3d8i.lib` is diagnostic-only and regressed the measured gameplay baseline.
- Stub obj `.\Release\exe\xbox_asm_stubs.obj` listed explicitly in AdditionalDependencies
- `EntryPointSymbol="_WinMainCRTStartup"`

### SP XBE Generation (post-build, baked into x_exe.vcproj)
Calls `patchxbe.py $(ProjectDir) .\Release\ja-release.exe .\Release\ja-release.xbe`

**`patchxbe.py`** does four things:
1. Strips `KERNEL32.DLL` from the PE import table (imagebld rejects Win32 DLL imports)
2. Patches PE subsystem field to Xbox (14)
3. Runs `C:\XDK\xbox\bin\imagebld.exe` with test signing flags
4. Injects D3D8 and XGRAPHC library version entries into the XBE for CXBX-Reloaded HLE

Pre-link event assembles stubs automatically — Rebuild works without manual intervention.

### XBE Metadata
- **Title ID:** `0x4C41000B`
- **LAN Key:** `4C41000B4C41000B4C41000B4C41000B`
- **Title Name:** Jedi Knight: Jedi Academy
- **Stack Size:** `0x40000`

---

## Historical MP Build Status — Deprecated

This section documents the old inherited MP tree only. It is not the active Holomatch build path, and `codemp\` must remain unnecessary for SP-hosted `efmp.xbe`.

### Projects in `codemp/JKA_mp.sln`
| Project | Status |
|---|---|
| goblib | ✅ 0 errors |
| x_botlib | ✅ 0 errors |
| x_ui | ✅ 0 errors |
| x_jk2game | ✅ 0 errors |
| x_jk2cgame | ✅ 0 errors |
| x_exe | ✅ Release builds (`jamp-release.exe` + `jamp-release.xbe`) |

### Key MP Fixes Applied (source files in `codemp/`)
- `Xbox` → `Win32` platform rename (36-254 replacements per file)
- `client/cl_data.h`: added return type to `operator=(const ClientManager&)`
- `renderer/modelmem.h`: hoisted `int i` before for loop
- `ui/ui_main.c`: `const baseClass` → `const int baseClass` (lines 1461, 10986); hoisted `int i` before post-loop assert
- `botlib/l_precomp.cpp`: `ctime((const long*)` → `ctime((const time_t*)`
- `qcommon/xb_settings.cpp`: `(LPCWSTR)` casts on `XCreateSaveGame`/`XDeleteSaveGame` calls; `(wchar_t*)` cast on `mbstowcs`
- `client/snd_dma_console.cpp`: hoisted `int i` before for loop
- `renderer/tr_font.cpp`: hoisted `iFontToFind` and `it` before their for loops
- `renderer/tr_shade.cpp`: hoisted `int i`
- `win32/win_highdynamicrange.cpp`: hoisted `int xx`
- `win32/win_lighteffects.cpp`: hoisted `int i`
- `win32/win_qgl_dx8.cpp`: fixed `for(int i=` scope
- `xbox/xblive.cpp`: hoisted `int i`
- `qcommon/huffman.cpp`: hoisted `int i`
- Various for-loop scope fixes across game/cgame files

### MP Stub Files (`codemp/x_exe/`)
Same `xbox_asm_stubs.asm` and `xbox_crt_stubs.cpp` as SP — copied from `code/x_exe/`.
The asm stub also includes `__except_handler4` (required by MP's `win_shared.cpp`).

### MP Runtime Fixes Applied
- Same `Sys_InitFileCodes` non-fatal fix as SP
- Same `Com_Error(ERR_DROP)` early-init guard as SP (`CL_FlushMemory` skipped when `com_cl_running` not set)
- Same D3D NULL device guards in `win_qgl_dx8.cpp` as SP (4 crash points)

### MP XBE — COMPLETE ✅
`patchxbe.py` in `codemp/x_exe/` handles KERNEL32 stripping, empty section removal (.rsrc), subsystem patch, and imagebld. Pre-link event assembles stubs. Linker uses `/FIXED:NO` to generate .reloc section (required by imagebld). `EmbedManifest="false"` set. `EntryPointSymbol="WinMainCRTStartup"` (no leading underscore — linker decorates it).

### Holocron FFA + Jedi Master Port (from JO MP)
Code complete — needs compile test. Changes:
- `q_shared.h`: uncommented `isJediMaster`, `holocronsCarried[]`, `holocronCantTouch`, `holocronCantTouchTime`, `holocronBits` in playerState_t; uncommented `isJediMaster` in both entityState_t variants
- `g_main.c`: removed "not supported" blocks, uncommented `g_MaxHolocronCarry` cvar
- `g_combat.c`: uncommented G_GetJediMaster, G_ThereIsAMaster, JM death/scoring, friendly fire prevention
- `g_client.c`: uncommented `isJediMaster = qtrue` on saber pickup, `= qfalse` on spawn
- `g_active.c`: uncommented G_UpdateJediMasterBroadcasts body
- `g_misc.c`: ported HolocronRespawn, HolocronPopOut, HolocronTouch, HolocronThink from JO; fixed SP_misc_holocron (removed assert(0), #ifndef _XBOX guards, uncommented isJediMaster)
- `w_force.c`: uncommented HolocronUpdate, holocron init, holocron force regen, JM force grants
- `bg_misc.c`: uncommented isJediMaster and holocronBits in BG_PlayerStateToEntityState
- `gameinfo.txt`: added "Holocron FFA" (1) and "Jedi Master" (2) to both gametype lists

### MP XBLog Integration
- `Com_Printf` restructured: XBLog_Write always runs even in Release (original was `#ifdef _DEBUG` guarded)
- `win_main_console.cpp`: full boot sequence breadcrumbs (JAMP: prefix)
- `common.cpp`: breadcrumbs throughout Com_Init

---

## Roadmap
1. ✅ SP — Release builds and produces XBE; XBLog wired to `E:\ja_log.txt` with breadcrumbs throughout boot
2. ✅ MP — Release builds and produces XBE; XBLog wired to `E:\ja_log.txt` with breadcrumbs (`JAMP:` prefix)
3. ✅ Holocron FFA + Jedi Master gametypes ported from JO to JA MP
4. 📋 Test SP on retail Xbox (check `E:\ja_log.txt` for boot progress)
5. 📋 Test MP on retail Xbox
5. 📋 Jedi Outcast single player build
6. 📋 Re-theme JA SP/MP UI to more closely match PC version's UI theme
7. 📋 Port JA's .skin segment selection to JO SP — cosmetic customization of Kyle using alternate .skin files in the same model folder (same GLM, falls back to base skin if segments missing)
8. 📋 Add outside-file support — files placed outside GOBs using the same folder structure should be read by the filesystem and should supersede matching GOB-contained files

## Testing Notes
- `ef_sp_input_replay.txt` is an explicit diagnostic-only, process-local input
  fixture. Never include it in a release ISO or hardware stage. The probe adds
  it temporarily for visual/diagnostic SP runs and restores the original ISO.
  It supplies player input before authoritative camera overrides; it must not
  alter ICARUS tasks, entity positions, health, or authored progression.
- Use XEMU/LLE for current unified SP/co-op/Holomatch proof.
- Retail Xbox remains the final target.
- Current logs are `E:\ef_sp_log.txt` for SP and `D:\ef_mp_log.txt`, falling back to `E:\ef_mp_log.txt`, for SP-hosted Holomatch. XBLog flushes every write; last line = crash point.
- Run `scripts\cleanup_generated.ps1` before and after emulator runs and after every completed build/package/test cycle. `scripts\run_sp_xemu_smoke.ps1` does this automatically. Use `-Aggressive` after replacing a beta stage so obsolete package trees cannot accumulate.
- Keep one current XEMU ISO under `build\xemu`; per-run ISO copies and staging trees are temporary artifacts. Use `-KeepStage` only for a specific diagnostic that needs inspection afterward.
- Smoke output retention is bounded. Preserve a proof explicitly in project notes or a committed artifact before allowing newer runs to rotate it out.

## JO MP Source Reference
- JO MP source is at `D:\Programming\GitHub\jedioutcast-master\CODE-mp\`
- Used for porting Holocron/Jedi Master gametypes to JA MP

---

## Key Technical Notes

### d3d8.lib vs d3d8i.lib
The active build links XDK 5558's retail `d3d8.lib`. Use `d3d8i.lib` only for a bounded diagnostic that needs its counters, then restore `d3d8.lib`: the instrumented runtime reduced the measured gameplay baseline and is not a shipping candidate.

### XDK Tools Location
The active build tools are under `C:\XDK_5558\XDK\xbox\bin\`, including `imagebld.exe`, `xsasm.exe`, and `xbcp.exe`.

### patchxbe.py Location
`code/x_exe/patchxbe.py` — active SP and SP-hosted Holomatch XBE post-processor. Do not wire active Holomatch through `codemp/x_exe`.

### Symbol Naming in MASM
`.model flat` does NOT prepend underscores to PUBLIC names. Write the exact linker symbol name:
- C name `foo` → linker symbol `_foo` → MASM `PUBLIC _foo`
- C name `_foo` → linker symbol `__foo` → MASM `PUBLIC __foo`

### $(IntDir) in AdditionalDependencies
`$(IntDir)` does not expand in `AdditionalDependencies`. Use hardcoded relative paths like `.\\Debug\\exe\\xbox_asm_stubs.obj`.

### XDK Include Paths
Required in all MP vcprojs: `C:\XDK\xbox\include;C:\XDK\include`
Both `platform.h` and `xboxcommon.h` include `xtl.h` which lives in `C:\XDK\xbox\include\`.

### Solution Format
MP solution (`JKA_mp.sln`) is VS2003 Format Version 7.00. VS2005 loads it fine once all vcprojs use Win32 platform.

## Invincibility for progression diagnostics — User instruction, 2026-09-09
- The user explicitly authorized the existing god-mode cheat for progression tests.
- `scripts/run_sp_perf_probe.py --god` queues the existing `god` command once via the client-active diagnostic hook. Verify the guest entity FL_GODMODE flag; queuing `god` immediately after `map` is too early and is rejected before CA_ACTIVE.
- God-mode runs remain explicitly visual/diagnostic, not FPS or normal-difficulty acceptance. Keep ICARUS behavior unchanged and the temporary cheat-command files out of release packages.

## Campaign-wide checks — User instruction, 2026-09-09
- The user reports stasis1 stable to the observed point and requests the same treatment through every campaign map. Continue across the campaign; preserve the distinction between opening stability and full route/exit completion.
- Track readable per-map status in `notes/campaign_checks_2026-09-09.md`. Use native XEMU captures and guest-memory/log checks; god mode is authorized for progression diagnostics, not performance acceptance. Do not resume paused audio work.
- Invincibility is now runtime-confirmed: `visual_campaign_stasis2_opening_20260909_233714` reads entity flags `0x10`, god_mode=true, health=100, command_time=128706 directly from guest virtual memory. The temporary XDVDFS directory writer must sort uppercase ASCII, not lowercase, or command/timing filenames become unreachable by Xbox binary search. Regression covered by `scripts/tests/verify_perf_iso.py`.

## Campaign probe evidence — 2026-09-10
- Read current FPS records from the exported guest-virtual FPS ring and its index. Scanning arbitrary physical RAM can recover stale records from earlier maps and must not qualify performance.
- CA_ACTIVE alone does not prove continued execution. The probe compares two main-loop counter reads before final pause; keep this liveness requirement in acceptance.
- Preserve bounds on both visibility-diagnostic surface bitmaps. Each has 110 words (3520 surfaces); larger surface indices must still render normally while skipping bitmap writes. An unchecked write froze scavboss by corrupting a neighboring renderer pointer. Diagnostic overflow count uses word292; words66-71 belong to camera data.
- Current campaign progress, including tight memory on borg3, is in notes/campaign_checks_2026-09-09.md. Opening checks do not establish complete routes or exits.

## Lossless MDR storage — 2026-09-10
- Voy15 could not allocate its default player model after the briefing. Trying alternate allocators alone failed; early default-model precaching caused a later workspace OOM and was removed.
- Eligible non-Borg MDR models now use streamed lossless 16-frame blocks; all original animation bytes, surfaces, tags and LODs are retained. Borg base/patch storage remains unchanged.
- Eight-minute XEMU voy15 test completed both briefing scenes naturally and returned control, no engine error and final main-loop advance72. Native guest-memory reconstruction matched every animation byte in10 resident models to source, saving1721975bytes of model storage before shared workspace. This is opening/scene stability evidence, not full-route, FPS or retail acceptance.
- Evidence and implementation details are in notes/letterbox_optimization_2026-09-09.md and notes/evidence/campaign_20260909/voy15_resident_mdr_roundtrips.json.

## Movie texture upload failure — 2026-09-10
- Voy1's apparently frozen BIK was an ignored D3DXLoadSurfaceFromMemory E_OUTOFMEMORY (0x8007000e), captured at movie frame 2 through the process-local debugger. The movie clock continued while an old texture remained visible.
- Full-size compatible RGBA movie updates now use the existing texture's tiled lock and XGSwizzleRect, with format/dimension/capacity checks and GPU synchronization, avoiding a temporary surface allocation. Preserve movie timing and ICARUS sequencing.
- XBE 9ce69199743d2de4e4084bff500ca31a045040d934b883e9d82a658076431b56 passed a 330-second XEMU visual/god check: changing movie frames and credits, then natural bridge gameplay, main-loop advance 46 and no engine error. This is not retail or performance acceptance. See notes/evidence/campaign_20260909/visual_campaign_voy1_movie_texture_fix.
