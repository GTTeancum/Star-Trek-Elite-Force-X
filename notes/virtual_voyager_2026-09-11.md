# Virtual Voyager integration

User-authorized goal: implement the approved New Game layout, optimize and
package Virtual Voyager content, verify every included level loads without
apparent glitches, and support ordinary saves and loads. This work is active;
no level or save/load qualification has passed yet.

## Approved menu

- TUTORIAL retains its position and shape.
- CAMPAIGN uses the same horizontal alignment and pill shape directly below it;
  it starts borg1 with the selected difficulty and gender.
- The former large ENGAGE rectangle becomes VIRTUAL / VOYAGER and starts
  tour/deck02, as the original game's StartTour does.
- Co-op's separate New Game flow remains unchanged.

## Source findings

- Licensed PAK0Ã¢â‚¬â€œPAK3 exist in build/release/BaseEF and in the complete local seed.
- PAK3 contains ten nested tour decks and six additional holodeck BSPs.
  Base PAK0 contains three more holodeck BSPs and the brig. Include all twenty
  in the load audit, even optional maps not listed by the normal holodeck menu.
- The packer's top-level-only BSP scan omitted maps/tour/. Output naming also
  used only the filename stem, discarding its folder.
- The renderer's lightmap sidecar lookup discarded the tour folder.
- The frontend and outside-frame launch path both reset cg_virtualVoyager.
- cg_virtualVoyager had no CVAR_SAVEGAME flag. Normal save loading needs a
  zero fallback for older campaign saves and saved mode restoration for tours.
- Original StartTour clears hub/tour saves. Review equivalent Xbox behavior
  before finalizing repeated New Game launches; never erase manual saves.
- Authored turbolift and holodeck menus invoke commands from
  ext_data/sp_turbolift.dat and sp_holodeck.dat. Their routing needs qualification;
  do not substitute direct map jumps for script-owned transport behavior.

## Required evidence

- Fresh SP and MP builds with the clean XDK5558 toolchain.
- Packed-BSP conversion checks and full nested path preservation.
- Native XEMU screenshots of the final New Game menu and every VV map.
- Runtime map identity, live frames and clear error records for each map.
- Normal save/load menu round trips, including returning to VV after a restart
  or a campaign load, with tour mode and player state restored.
- Preserve authored ICARUS sequencing, geometry and visual materials.
- Keep output audible; muddy 3D voice tuning remains paused.

## Initial implementation (not yet runtime-verified)

Menu layout/dispatch, nested BSP selection and naming, nested lightmap lookup,
and tour flag persistence are implemented in source. The first SP+MP build is
running in build/research/virtual_voyager_build.log. Source extraction is running
through scripts/extract_virtual_voyager_sources.py, which does not overwrite
controller configs, scripts, or VM binaries.

## First checkpoint

- Fresh initial SP+MP build passed (22:18:43 SP / 22:22:39 MP build IDs).
- Native New Game capture matches the approved arrangement; archived at
  evidence/virtual_voyager_20260911/menu/new_game.png. Capture transaction
  completed and restored the original ISO. The first 100-second capture only
  observed startup movies; the 260-second run reached New Game.
- Twenty VV map source hashes inventoried. The package contains 66 SP/optional
  map conversions including all twenty VV audit targets. Packed-record,
  authored-entity byte preservation and lightmap record audits passed for all20.
- Total VV source BSP bytes90,396,112; packed records plus RGB565 sidecars
  total92,307,795. This is not a disk-size reduction. The pipeline supplies the
  native renderer format and streamed lightmaps, avoiding runtime conversion.
- Menu/save refinements pending second build: permit manual saves on underscore
  holodeck/brig maps only while in VV; reset only reserved tour hub snapshots at
  a new tour; add opt-in process-local menu fixtures for capture and VV launch.
- The second build initially caught a missing SG_WipeSavegame declaration;
  declaration added and the build rerun. No runtime candidate was qualified
  from that failed build.
- The package audit initially misread the sidecar's first DWORD as a count;
  corrected it to the actual per-record size and audited every DDS header.
- Isolated candidate ISO construction is in progress outside build/xemu so
  routine cleanup retains the existing baseline. Check terminal status before
  mounting; build/research/virtual_voyager/iso_build.log.
- In-game turbolift/holodeck routing still needs implementation/verification:
  CL_GenericMenu_f currently routes through ingame/menuID, and these names have
  no specialized route in the active EF frontend. Original data-driven `use`
  commands must be preserved instead of replacing authored travel with `map`.

## Deck02 initial runtime pass

Run vv_deck02_initial_20260911_223757 completed180seconds and restored the
candidate ISO. Engine map identity was tour/deck02, client state7/active,
engine error text empty, final main loop advanced8785→8858. Three native
screenshots were inspected individually; no obvious material/geometry/HUD
fault in the visible starting room. Evidence is archived under
notes/evidence/virtual_voyager_20260911/deck02_initial.

Physical unallocated memory was1,753,088bytes, but the preallocated game zone
still had about8.2MB free (largest block6.7MB). Do not interpret physical-free
alone as total allocator headroom or declare an OOM without evidence.

The current source adds data-driven turbolift/holodeck travel screens, gated
on actual VV mode, to preserve stock use/map/load commands from ext_data.
These are still awaiting runtime verification. Build39750/travel_build.log
contains these changes. Other authored interaction menus remain to be audited.
Normal save/load round trips and19other map loads remain unverified.
## Save stall diagnosis

The first deck02 save/load/travel probe stopped on `save vv_probe_deck02`.
Client state remained active and error text empty, but the main loop stopped:
this was NOT a passing save/load test. Three native captures were inspected;
all showed the same stopped room. Evidence: deck02_save_stall, save_trace,
and save_trace2 under notes/evidence/virtual_voyager_20260911.

The narrowed trace reaches save permission returned true, then stops at
SE_GetString("CON_TEXT_SAVING_GAME"). VVFixedMap::Find used an uninitialized
index for an empty map and unconditionally probed neighboring elements outside
valid bounds for misses. The actual template fails AddressSanitizer on an
empty lookup. A bounded binary search passes empty/singleton/missing-boundary/
gap/all-128-key checks in scripts/tests/verify_fixedmap_lookup.cpp.
The Xbox build containing this fix is pending; no save round trip passed yet.

The active client also lacked `genericmenu` and `endholodeckmenu` registration,
although original target_interface entities and holodeck scripts issue those
commands. EF-SP-only aliases now connect them to the data-driven travel menus.
These routes still need runtime verification. The earlier genericmenu fixture
could not have tested travel successfully, even if saving had returned.

scripts/run_virtual_voyager_load_checks.py now supports serial, short load
checks, stops at runtime failures, archives each transaction and three native
captures, and leaves visual review explicitly pending. It has not run yet.
The Xbox lookup-fix build passed both personalities (SP23:10:39/MP23:13:50).
Run vv_save_lookup_fixed_20260911_231506 confirms SE_GetString now returns,
saving reaches close and the command buffer advances to `load vv_probe_deck02`.
That load stalls; the round trip remains FAILED, not qualified. All three
native captures were reviewed and archived with the failed run diagnostics.

The next build (SP23:20:48/MP23:21:30) adds direct memory-ring load trace lines
and opt-in `ui_ef_test_vv_save`/`ui_ef_test_vv_load` fixtures. These call the
normal UI save-slot creation and load-list APIs, only in frame-diagnostic SP.
The load-only trace is running against the prior save. Do not rebuild until
the probe is terminal and its ISO transaction is restored.

Material-reference audit resolves all seven apparent missing image candidates
to shader names; all ten referenced images exist as DDS in the candidate PK3.
See build/research/virtual_voyager/material_reference_audit.json.

Reload trace 23:35 passed title resolution, game/client shutdown, texture/hunk cleanup and snapshot/configstring frees. It stalls inside SV_ClearLastLevel. Single native capture inspected; still old room, liveness false. Probe terminal, ISO restored. Evidence: reload_trace. Added bounded per-call teardown breadcrumbs for next build.

Model cleanup trace stopped releasing models/weapons2/prifle/prifle_hand.md3: raw HeapAlloc pointer0x01026010, size4308, heap=1. Old cleanup called FS_STEFX_FreeHeapFileBuffer on every heap allocation; its20-byte header probe reaches0x01025ffc before this block. Explicit bFileBuffer ownership now limits the header probe to adopted FS buffers. Plain heap and compact MDR storage go directly to HeapFree, zone storage to Z_Free. Actual release-routine host test rejects the old plain-heap dispatch and passes all three ownership classes plus repeated cleanup after fix. Native reload verification pending. All prior probes terminal/restored; individual native captures inspected.

Ownership runtime probe passed all model cleanup and loaded packed collision/world data again. Next concrete failure: InitGame -> G_LoadBoltOns -> ERROR Too many boltOns (64). Resident game globals were not reset before registering the same config again. Added attachment-table reset before parsing; removed per-model free trace spam. Mode cvar restored to1 after explicit live reset0. One native capture inspected; this remains a FAILED roundtrip. Evidence: model_ownership_runtime.

Reload PASS: vv_bolton_reload_fixed_20260912_000026 returned to tour/deck02, CA_ACTIVE, liveness verified, no engine error; complete reload2348guest ms. cg_virtualVoyager integer1 restored after explicit live reset0. Both native post-reload captures individually reviewed, normal room/HUD and changing tricorder. ISO restored. This validates engine save/read from an earlier executable run, not yet normal UI slot/menu flow. Next vv_ui_save_deck02 invokes the ordinary UI save-slot API.

The first ui_save_deck02 probe was a harness NO-OP: direct-map boot does not start the UI, so bare ui_ef_test_vv_save never reached UI_GameCommand. It is NOT save evidence despite live/error-clear gameplay. Rerunning through existing genericmenu, which ensures UI initialization before dispatching the same fixture. Capture inspected; archive ui_save_deck02 retained as invalid test. Transporter menu route and persistent selected-destination highlight are edited but not compiled yet.

Normal UI save PASS: vv_ui_save_routed_20260912_000940 used genericmenu to initialize UI and called UI_EFSave_CreateNew. It wrote eforce0 for tour/deck02 through the ordinary save-slot API and returned to live/error-clear gameplay. Native capture inspected individually; archive ui_save_routed.

Cold load-menu probe vv_ui_load_cold_20260912_001453 FAILED liveness. Both native captures inspected individually and showed the identical room, not a load menu. Last command was wait 2000; genericmenu ui_ef_loadgame had returned after reading all three save headers, including eforce0. Source inspection shows this direct invocation selects the main-menu route and sets sv_killserver, so it does not exercise the normal pause->Load path. No claim of normal UI load success. Archive ui_load_cold. Next probe explicitly opens ingameMainMenu first, then the normal Load screen and load-slot API. Main-menu shutdown failure remains unresolved.

Transporter route and selected travel destination highlight compiled in the fresh pair: SP Sep12 00:13:23 / MP 00:14:04 (transporter_build.log). Runtime travel qualification remains pending.

Normal pause->Load screen displayed correctly in vv_ui_load_pause_20260912_002305, including eforce0/tour/deck02, but actual load FAILED full-file signature validation. Engine remained live on the original map, and VV flag remained0 after fixture reset; no successful reload occurred. Both captures individually reviewed and archived ui_load_pause. Source/run evidence identifies two defects: postMapCommandPaths tries both D: and d: without stopping on success, so every post command ran twice; repeated save eforce0 overwrote via OPEN_ALWAYS, retaining the previous padded file tail, while the new signature hashed only bytes rewritten. Source now breaks after reading one command-file spelling and uses CREATE_ALWAYS for the save payload, retaining OPEN_ALWAYS for the Xbox save-directory API. Invalid file handles are rejected before writing. Changes await build/runtime overwrite-and-reload proof. Added diagnostic completed-write/read/signature-failure counters so live gameplay after a rejected load cannot be mistaken for success.
The first additional load sweep uses the unchanged00:13:23 XBE. tour/deck01 passed runtime liveness/error checks and all three native captures were inspected individually: correct visible turbolift and HUD, but the rest of the deck was not visually covered. tour/deck03/04/05 follow serially. Do not rebuild before suite termination/restoration.

Additional load sweep loads_20260912_002632 completed all four requested maps (tour/deck01/03/04/05), each live/error-clear with restored ISO. All twelve native captures individually inspected. They cover starting turbolifts only; movement/area review still required. Results and limitations are recorded in virtual_voyager_map_checks.md and the sweep results.json.

scripts/tests/verify_postmap_commands_once.py compiles the actual post-command loop with a case-insensitive file fixture. Before fix: opens2 and repeats both commands, fails. After fix: passes single dispatch, lowercase fallback, no-file handling, and exclusion for normal built-in launches. Evidence save_overwrite/postmap_before.log and postmap_after.log. save_overwrite_build.log is the fresh SP+MP rebuild for the truncation/dispatch fixes and completion counters. Added optional --walk and --save-roundtrip to the serial map checker; roundtrip requires two completed writes, one completed read, zero signature failures, restored VV flag1, live/error-clear game, and native captures. These options have not yet run against Xbox. UI eforce0 from the old double-save remains corrupt until overwritten; do not count it as passing.

Fresh coherent XDK5558 pair built successfully (session1673 terminal0): default Sep12 00:39:25, efmp00:40:07. The targeted vv_overwrite_fixed probe is running (session69224), writing eforce0 twice and loading it after clearing live VV mode. It uses the diagnostic HDD's already-owned eforce0 slot. Expect proof version1/writes2/reads1/signatureFailures0 and restored VV mode1, plus live/error-clear gameplay. Do not rebuild until this probe terminates and its ISO transaction is restored. Main-menu shutdown breadcrumbs are compiled for the separate unresolved route.

Overwrite FIX VERIFIED: vv_overwrite_fixed_20260912_004057 completed proof(version1,writes2,reads1,signatureFailures0), VV mode1 restored after explicit reset0, loaded tour/deck02, live/error-clear. Both native captures individually inspected. Terminal/restored; archive overwrite_fixed.
Cold normal UI load VERIFIED: vv_ui_load_fixed_20260912_004520 initialized normal pause->Load, found existing eforce0, invoked UI_EFSave_Load, and completed proof(1,0,1,0), restored VV mode1. Live/error-clear tour/deck02. Both native post-load captures individually inspected; normal room/HUD. Terminal/restored; archive ui_load_fixed. Earlier load-menu capture ui_load_pause shows slot labels; this newer run had already loaded by its first capture.
Final package builder now accepts paired --default-xbe/--mp-xbe and byte-verifies both appended payloads alongside assets. Synthetic XDVDFS test scripts/tests/verify_virtual_voyager_iso.py passes payload identity, Xbox lookup, sector alignment, preserved source, manifest, and rejection of an unpaired executable. No final image created yet. Read-only baseline_root_audit.json confirms six normal root entries and no diagnostic boot/input markers. Prepared test_virtual_voyager_menu_entry.py for actual normal MAIN->NEW GAME->VV callback; not yet run.
Combined movement/overwrite/reload validation starts with deck01 (session36172) to qualify the replay and completion gates before other levels. Source build remains SP00:39:25/MP00:40:07. No rebuild while probe active.

Corrected movement-before-save run loads_20260912_005309 passed deck01 with proof(1,2,1,0), restored VV1, live/error-clear, and three individually inspected captures showing the bridge beyond the turbolift. Earlier loads_20260912_004851 save test passed but its movement was invalidated by the intentional replay time-reset stop; retain that limitation. The checker now waits4000 command cycles before its first save when --walk is enabled, so movement precedes reload without changing replay or ICARUS behavior.

The remaining19-map combined sweep is RUNNING in session52126, starting tour/deck02. It uses current SP00:39:25/MP00:40:07, --walk --save-roundtrip --seconds110, candidate ISO and diagnostic HDD, port4491. It stops at first failure and archives every completed probe. Poll this same session; do not rebuild or launch another XEMU on this image/HDD until terminal and restored. Every native capture still needs individual review. Actual New Game VV entry probe is prepared but not launched. See virtual_voyager_release_gate.md for remaining explicit gates and known limitations.

The remaining-map scheduler31500 was stopped deliberately after deck03 wall-only captures; its independent deck04 child8276 subsequently finished, all processes are now absent and deck04 transaction restored:true. Deck04 proof(1,2,1,0), VV1, live/error-clear; all three native corridor captures inspected individually and archived manually. Deck03 also passed roundtrip but its three wall-facing captures were insufficient visual coverage. Revised checker adds a bounded process-local yaw turn after movement and moves native captures earlier (30/55/80sec for110sec run). Deck03 repeat session9071 is active; no source rebuild or new ISO writer until terminal/restored. First two repeat captures individually inspected: doorway opening and corridor beyond lift, intact visible textures/HUD.

Deck03 revised repeat loads_20260912_010908 is terminal0/restored, proof(1,2,1,0), VV1 and live/error-clear. All three native captures individually inspected: opening doorway, corridor, reload title. Remaining16-map batch started in session77706, scheduler PID31952, initial deck05 probe PID31608; confirmed live via process inventory. Evidence loads_20260912_011255. Same current XBE pair, revised walk/turn with30/55/80sec captures. Do not start another ISO writer or rebuild until batch stops/finishes and active child transaction restored. All new captures require individual inspection. Menu-entry and authored travel tests remain pending.

Current batch loads_20260912_011255/session77706: deck05 runtime PASS proof(1,2,1,0), VV1/restored; all three captures individually reviewed but narrow door/wall view requires map-specific visual follow-up. Sidecar visual_review.json records limitation and capture hashes. Deck08 is now running. Prepared scripts/check_virtual_voyager_turbolift.py (py_compile passed, NOT RUN) using normal travel menu callbacks, row1 original use tour_turbo_02; independently verified PAK3 deck01 target_level_change points to tour/deck02 with target turbolift. This preparation is not runtime travel evidence.

Batch session77706 continues: deck08 PASS proof(1,2,1,0), VV1/restored, live/error-clear. All three native captures reviewed individually; close wall/door with blue effect, insufficient broader scene coverage, sidecar records follow-up. Deck09 now running. Added optional --walk-ms to checker (250�8000ms, default4000), syntax checked; running Python scheduler retains old loaded code and is unaffected. Deck05 final position versus original PAK3 spawn shows938units forward movement, explaining overshoot; use shorter map-specific walk for its visual follow-up. Do not restart batch or rebuild while current transaction active.

Deck09 in loads_20260912_011255 passed runtime/save proof(1,2,1,0), VV1, restored. VISUAL FAILURE: individually reviewed shots01:20:40 (corridor/NPC/console),01:20:51 (reload title),01:21:16 (corridor with RGB axis marker near NPC). Corrected visual_review.json immediately; do not count no-glitch pass. Retail renderer tr_main_retail.cpp MOD_BAD branch around1912 submits entitySurface, whose default tr_surface_retail.cpp RB_SurfaceEntity draws RB_SurfaceAxis. Need identify actual bad entity/model and source after reload, not hide marker. Final ring logs retain deferred NPC registration (green33,renner37,Kray40,goldf2 106,goldm1 109,Jurot231,Chang234), graphics refresh and G_LoadBoltOns; no precise missing model evidence yet. Batch session77706 still runs deck10; do not rebuild while active. Source review confirmed new tour already wipes10 hub/tour snapshots in win_main_console.cpp; original StartTour parity there needs runtime menu test, no source fix warranted.

Deck10 PASS proof(1,2,1,0), VV1/restored. Three native captures individually inspected (corridor/door junction, reload title, same corridor), sidecar records limited local area but no obvious fault. Batch session77706 now running deck11. For deck09 RGB marker investigation, added bounded SP-only/frame-diagnostic g_SPXBVvMissingModels[132] in retail_xbox/tr_main_retail.cpp at actual MOD_BAD surface submission, plus optional528byte probe dump. Records first16 distinct handle/entity pairs, frame/model count/flags/origin; logs model name once. Does not suppress or alter rendering. SOURCE ONLY, NOT COMPILED/TESTED; do not rebuild until batch terminal/restored. Need repeat deck09 with diagnostic pair, identify true missing model source and fix, then verify visual reload.

Deck11 PASS in batch session77706, restored/proof(1,2,1,0)/VV1/live/error-clear. All three native captures individually reviewed, close wall+blue effect, reload title, same wall; visual follow-up required (sidecar saved). Deck15 started. Missing-model diagnostics remain source-only/uncompiled. No new game fixes yet.

Deck15 PASS proof(1,2,1,0), VV1/restored/live/error-clear. All three native captures individually reviewed: maintenance corridor, door/equipment panels, intact local view. Sidecar and map table updated. Batch session77706 now on7/16 _brig; scheduler31952 remains in charge. All ten tour deck maps have runtime save/load evidence now, but deck09 visual axis failure remains, and decks05/08/11 need wider visual follow-ups. Ten underscore maps remain under test. Do not rebuild until batch ends/restores.

Batch session77706 TERMINAL1 at _brig; ISO restored:true. All three brig captures individually inspected: insubordination mission-failed screen then old deck15 room. Proof(1,0,1,0) confirms zero new saves and load of prior diagnostic slot, so NOT brig roundtrip. PAK0 _brig BSP worldspawn/remove, start pickanumber and NPC poormunro scripts; poormunro.IBI contains SET_MISSIONSTATUSTEXT STAT_INSUBORDINATION and SET_HEALTH on MUNRO. This is authored terminal behavior, not established crash. Need earlier captures and inspect normal save eligibility; do not change authored scripts to pass test. Nine holodecks still untested. Sidecars merged into terminal batch results. Missing-model diagnostic coherent SP+MP build now running session34806, log build/research/virtual_voyager/missing_model_build.log. Wait for terminal0 before deck09 repeat.

Diagnostic pair built (session34806 terminal0): SP01:36:46/MP01:37:26. Deck09 repeat loads_20260912_013817 session88808 terminal0/restored, runtime save proof passes but visual axis reproduced in all-three individually reviewed sequence. Missing-model probe reports6 distinct stale handles: entity109/goldm1 handles201/202/203, entity106/goldf2 handles204/205/206, tr.numModels201,11709 invalid submissions, no overflow. Source CG_RegisterGraphics defers NPC registration without invalidating clientInfo.infoValid restored from save, while CG_Player registers only !infoValid. Added SP-only invalidation of that flag in deferred NPC precache branch; renderer-local models/skins will be registered by existing draw path. No eager loading or marker suppression. Fix building session9953, log npc_reload_build.log; do not test until terminal0. Then repeat deck09 same movement/save and require zero missing-model submissions plus actual restored NPC captures. Brig normal cinematic/death save restrictions confirmed SG_GameAllowedToSaveHere and GameAllowedToSaveHere return !in_camera. Preserve restrictions, capture authored brig early; no claimed brig normal roundtrip.

NPC reload FIX VERIFIED: coherent pair SP01:43:31/MP01:44:16, build session9953 terminal0. Deck09 loads_20260912_014453 session11202 terminal0/restored, live/error-clear, proof(1,2,1,0), VV1. All three native captures individually inspected: NPC survives reload with intact model/gesture/tricorder, no axis. Missing-model header(1,0,0,0), zero records/submissions versus old11709. Fix invalidates restored clientInfo.infoValid for deferred NPCs when graphics registration resets. Remaining nine holodeck batch starts next on this pair. No full objective completion yet.

Active nine-holodeck batch session71665, starting _holodeck_camelot, current fixed SP01:43:31/MP01:44:16. Same candidate ISO/HDD/port4491 and110sec walk/roundtrip. Do not rebuild or start another probe until terminal/restored; inspect every native capture individually. Wider deck05/08/11 views, early brig cinematic capture, actual New Game VV entry, travel/hub returns, and final paired image remain pending.

Nine-holodeck batch evidence loads_20260912_014854/session71665: Camelot PASS runtime/proof(1,2,1,0)/VV1/restored and missing-model header(1,0,0,0). All three native captures individually reviewed: intact stone scenery/barrel/weapon/HUD, health changes show continuing combat. Sidecar records hashes/review. Firingrange now running. Updated checker source so future invocations require zero missing-model diagnostic submissions when symbol exists; current already-running scheduler retains previous code, so inspect these counters manually for this batch. No binary rebuild pending.

Firingrange PASS in active batch71665/loads_20260912_014854: proof(1,2,1,0), VV1/restored, missing model header(1,0,0,0). All three captures individually inspected: loading, then platform/target/sky/HUD. Large dark background checked against original PAK3 BSP shader references rig/sky and stasisfog_black; rig.shader uses clouds2 and sunset, original Stasis.shader black fog(0,0,0). No obvious visible fault. Sidecar and table updated. Garden now running; no new builds.

Garden PASS in active session71665/loads_20260912_014854: runtime/proof(1,2,1,0)/VV1/restored, missing model header(1,0,0,0). All three captures individually reviewed: courtyard architecture, plants, sky, phaser/HUD intact. Sidecar/table updated. Highnoon now running, followed by minigame/proton/proton2/temple/warlord. No new source/build changes in this turn.

Highnoon runtime PASS in active batch71665/loads_20260912_014854: proof(1,2,1,0), VV1/restored, missing-model header(1,0,0,0). All three captures individually inspected: close wall/health62, reload, then death/mission-analysis overlay with hall behind. Needs better alive visual follow-up, not full visual qualification. Diagnostic --god request does not establish continued invulnerability after reload; avoid claiming it did. Sidecar/table updated. Minigame now running, then proton/proton2/temple/warlord. No source/binary changes.

Minigame PASS in active71665/loads_20260912_014854: proof(1,2,1,0), VV1/restored, missing-model(1,0,0,0). All three native captures individually reviewed: arena/characters, then overhead arena/characters/props, no obvious missing geometry/texture/HUD fault. Camera changes observed; not a full minigame interaction test. Sidecar/table updated. Proton now running, then proton2/temple/warlord. No new binary changes.

Proton PASS in active71665/loads_20260912_014854: proof(1,2,1,0), VV1/restored, missing-model(1,0,0,0). All three captures individually reviewed: close gray wall/weapon, reload title, same wall. Wider visual follow-up needed; sidecar/table reflect limitation. Proton2 now running; temple/warlord follow. Planned visual follow-ups can use --walk-ms1000 without another save-roundtrip for maps whose persistence already passed, retaining early live gameplay and diagnostic god mode. This is a plan, not executed evidence. No builds pending.

Proton2 PASS in active71665/loads_20260912_014854: proof(1,2,1,0), VV1/restored, missing-model(1,0,0,0). All three native captures individually inspected: stairwell/railings/wall panels, weapon/HUD intact; limited local route coverage. Sidecar/table updated. Temple now running, Warlord last. No new source/binary changes.

Batch71665 TERMINAL1 at Temple, ISO restored. Temple capture sequence individually reviewed: death at16sec then old Proton2 save. Invalid Temple roundtrip (death prevented new saves); Warlord not yet run. Found harness ordering defect: postmap waits/saves queued before client-active god, whose Cbuf_AddText appended behind them. Changed run_sp_perf_probe.py so --god places god + requested post commands together in client-active file and omits separate postmap file; manifest records dispatch phase. No normal game behavior/source/build changes. Syntax checked. Corrected Temple probe session77426 now running with --walk-ms1000 --save-roundtrip --seconds110. Require successful writes/reload, actual god flag and intact live captures; not verified yet. Terminal prior batch sidecar reviews merged into results.

Temple corrected harness VERIFIED in loads_20260912_021721/session77426 terminal0/restored: proof(1,2,1,0), VV1, missing-model(1,0,0,0), live/error-clear; player health100/entity_flags16/god_mode:true after reload. All three native captures individually reviewed: temple details/weapon/HUD/combat effects intact. Queue-order correction worked; no new engine build needed. Warlord is last untested ordinary holodeck; starts next with corrected harness and1000mswalk. Remaining wider deck05/08/11, Highnoon, Proton views; early brig scene; menu and authored travel; final paired image.
Active Warlord probe session80677, corrected god-first harness, current SP01:43:31/MP01:44:16. Do not rebuild or start another ISO writer until terminal/restored.

Warlord PASS loads_20260912_022124/session80677 terminal0/restored: proof(1,2,1,0), VV1, missing-model(1,0,0,0). All three native captures individually reviewed: narrow walkway/wall, loading, same view; wider follow-up needed. All19 playable tour/holodeck maps now have save/load evidence; brig is authored cinematic/death with ordinary save restrictions. Added --look-around to checker: after bounded forward walk, continuous12degrees/sec yaw inside game-only replay. No engine change; syntax checked. First wider-view test will qualify deck05 using1000mswalk, no repeat save,90seconds; remaining views then follow if it works.
Active wider deck05 visual probe session54515 (--walk-ms1000 --look-around --seconds90, no save repeat), same fixed SP01:43:31/MP01:44:16. Wait terminal/restored and inspect all three varied views before applying to deck08/deck11/Highnoon/Proton/Warlord. No new build.

Deck05 wider attempt loads_20260912_022458/session54515 terminal0/restored, zero missing models, god true. All three native views individually inspected but still near lift/doorway. Runtime position x=-3621.795 fromspawn-3768 shows only146units of1000mswalk; insufficient to clear doorway. Repeat2500mswalk with same slow turn/no-save90sec planned next. Do not claim visual pass or repeat this short route blindly across other maps.

Deck05 second wider run loads_20260912_022828 is terminal with runtime_pass true and no probe/emulator processes remaining. All three native captures individually inspected: corridor beyond lift, wall panels/ceiling lights and weapon/HUD intact. Zero missing-model submissions. Local visual gate passed; complete routes not claimed. Next checks target actual menu/travel callbacks.


Turbolift callback probe vv_turbolift_20260912_023626 terminal0/restored, loaded deck02 from deck01, live/error-clear. All three native captures individually reviewed: deck02 loading then two intact arrival-lift views. Fixture selects normal menu row1 and Engage, whose packaged authored command is use tour_turbo_02. Final log ring no longer retains early menu command; menu visual capture itself still pending. Evidence archived turbolift_20260912_023626. This proves callback-to-destination transition, not full hub-return persistence.

Normal MAIN -> NEW GAME -> VIRTUAL VOYAGER launch test is active in exec session13742 (test_virtual_voyager_menu_entry.py). Current pair unchanged. Native shots scheduled200/230/260sec, duration280. Poll this session and require terminal/restored before any new ISO writer or build. Inspect each capture and final map/VV/liveness/error dumps before passing. Turbolift session56006 terminal0.

Normal MAIN->NEW GAME->VIRTUAL VOYAGER callback PASS: session13742 terminal0, restored:true; loaded tour/deck02, CA_ACTIVE7, VV1, error empty, liveness7104->7175. All three native captures individually inspected: quarters/tricorder/HUD intact. Archive menu_entry_20260912_024100 includes invocation, transaction, raw dumps, reports and images. Actual menu button launch gate passed on current pair; earlier menu layout capture remains visual arrangement evidence. Extended travel fixture with authored holodeck entry/return and transporter selections; syntax checked. Entry requires completed virtual-save write, return requires successful read, both zero signature failures.

Holodeck-entry test ACTIVE session64943, name vv_holodeck_entry_20260912_024701, starts tour/deck04, normal holodeck menu row0 executes authored use _holodeck_garden. Expected garden plus completed virtual-save write. Poll same session; require terminal/restored before return test or other ISO writer. All previous tests terminal. Pair unchanged SP01:43:31/MP01:44:16. Remaining wider visual deck08/11/Highnoon/Proton/Warlord, early brig, holodeck return/transporter/hub persistence, final paired image.

Holodeck entry vv_holodeck_entry_20260912_024701 session64943 terminal0/restored: authored deck04->garden transition and completed virtual-save write passed proof(1,1,0,0). All3 native captures individually inspected. First capture exposes REAL missing text on travel menu (button rectangles only); subsequent garden views intact. Archived holodeck_entry_missing_text. Root cause: EFQ adapter uses inherited JA ergoec/fontdat via Text_Paint, while EF shipped fonts use chars_* atlases and ext_data/fonts.dat. Replaced EFQ text/width with shared existing pause bitmap-font functions, preserving its renderer-restart cache invalidation. Source change compiled/runtime UNVERIFIED. Fresh coherent pair build next; menu and return need fixed-build checks.

Fresh font-fix pair build ACTIVE session8807, log build/research/virtual_voyager/travel_font_build.log. Shared ui_local.h declaration causes wider dependency rebuild; current outputs remain old until completion. Do not run probes or promote until both SP/MP terminal success. Archive audit confirms fonts/ergoec.fontdat absent from PAK0/1/2/3 and candidate xbox0.pk3. Source exports UI_EFPropTextWidth/UI_EFDrawPropText from existing pause bitmap renderer and uses them in EFQ adapters. Next: fixed-build holodeck-entry/menu capture then normal holodeck-return (existing virtual save from prior entry), using scripts/check_virtual_voyager_turbolift.py --route holodeck-entry/holodeck-return. Script now automatically archives future route results/captures (visual review pending). All XEMU probes terminal. Need verify fonts visible and header spacing, plus save/read proof. Preserve existing goal scope and remaining map checks/final image.

Build session8807 remains live: fresh SP XBE written Sep12 02:55:46; MP compilation underway, so pair not ready yet. No compiler errors found in current log scan. Static check of packaged fonts.dat (all768 glyph records) and all22 active travel labels confirms every glyph exists and widths fit470px (longest223px). Evidence build/research/virtual_voyager/travel_font_widths.json. This is sizing evidence only; missing-label fix still needs native capture. Recheck header title spacing on fixed menu (24px bitmap height at y32 approaches separator y52). Do not claim overall menu visual pass before review.

Font pair build session8807 TERMINAL0. Before native check, moved travel title y32->24 because EF big glyph height24 otherwise overlaps separator y52; title now ends48. Incremental paired build for this spacing correction next. No other source changes.

Spacing incremental pair build session10406 terminal0. Launch fixed-build holodeck-entry check, requiring visible menu labels, safe heading spacing, garden arrival and completed virtual save. No further build while probe consumes current pair.

Font/menu FIX VERIFIED in vv_holodeck_entry_20260912_030059 session54712 terminal0/restored. All three native captures individually inspected: complete readable holodeck labels/title with correct spacing, then intact garden. Completed return-save write proof(1,1,0,0), live/error-clear. Current coherent pair file times SP02:59:55/MP03:00:35. Evidence auto-archived with reviewed result.json. Next normal End Holodeck Program callback uses load virtual and must restore deck04; no new source/build change.

Holodeck-return probe ACTIVE session91639, name vv_holodeck_return_20260912_030457. Uses fixed current pair, starts garden, opens endholomenu, selects original row4 End Holodeck Program -> load virtual. Expected deck04 and completed read/signature0. Poll same session and inspect all native captures; requires terminal/restored before another writer. Remaining visual deck08/11/Highnoon/Proton/Warlord, early brig, transporter/hub persistence, final paired image. Do not reopen paused audio/accepted performance work.

Normal holodeck RETURN VERIFIED: vv_holodeck_return_20260912_030457 session91639 terminal0/restored. Original menu row4 load virtual restored deck04 with completed-read proof(1,0,1,0), live/error-clear. All three native captures individually inspected: complete menu including End Holodeck Program, then intact deck04/HUD. Evidence auto-archived and review recorded. Entry+return now verified using normal save/read APIs across separate emulator processes. Current pair unchanged.

Remaining wider visual batch ACTIVE session59909: deck08, deck11, Highnoon, Proton, Warlord; --walk-ms2500 --look-around --seconds90, no repeat saves. Uses corrected god-first harness/current font-fixed pair. All five already passed save roundtrips; this batch targets broader alive local views and zero missing-model counters. Inspect every native capture individually; stop if route coverage is still inadequate instead of assuming all maps share deck05 geometry. Do not rebuild or write ISO while batch active. Remaining afterward: early brig scene, transporter/hub persistence as needed, final paired image/current-build normal boot.

Active batch59909/loads_20260912_030857: deck08 and deck11 runtime PASS/restored. All six captures individually inspected. Deck11 intact corridor/door/HUD now local visual pass. Deck08 corridor intact but first/third views show unexplained blue translucent effect across doorway/wall; do NOT pass visually until source/runtime diagnosis. Source shader-name inventory has no explicit forcefield; initial searches inconclusive, avoid guessing. Reviews stored in per-map visual_review.json (merge after batch terminal). Highnoon now running, Proton/Warlord follow. Added turbolift-return fixture deck02->deck01, require completed read; turbolift outbound now requires completed write. Syntax checked, not run. This will validate normal hub persistence after normal New Tour reset cleared earlier hub slots.

Deck08 blue effect RESOLVED as authored, no code change: original PAK0 scripts/borg.shader textures/borg/static2 declares surfaceparm forcefield/trans/nolightmap, additive ONE/ONE and scrolling texture. Original deck08 surfaces7790/7791 span x[-2320,-2216], y=-3584/-3582,z[-3768,-3678]; actual player(-2268,-3569.8745,-3743.875) is12-14units away. Nearby target_speaker(-2268,-3608,-3712) plays voyfield.wav. Surface coordinate evidence archived with deck08; local visual pass. Highnoon and Proton runtime PASS/restored, all6 captures individually reviewed; courtyard/stairs/NPC/sky and rocky passage/doorway respectively, no apparent local rendering fault. Reviews saved sidecars/table; merge results when batch59909 terminal. Warlord last now running. Pair unchanged.

Wider batch59909 TERMINAL0: all5 runtime-pass/restored, missing-model headers(1,0,0,0). All15 captures individually reviewed, Warlord room/props/panel/HUD intact. All per-map sidecar reviews merged into results.json. All19 playable maps now have normal save/overwrite/reload plus local visual evidence; no full playthrough claim. Brig early cinematic next without god/save/load commands, so original failure behavior remains. Current SP hash94b294ed7937ce9107b85bf15f684a46ba5f88f2757adaed2ee05e43ed36b64b.

Brig early probe ACTIVE session82478 name vv_brig_early_20260912_0323,70seconds native shots8/20/32. No god or save/load injection; preserve original cinematic/death behavior. Poll same session; inspect each capture and final map/error/liveness. Remaining afterbrig: transporter and turbolift outbound/return hub persistence, final paired image with current assets/XBEs and normal boot validation. All earlier probes/builds terminal; do not start another ISO writer while this probe active.

BRIG early visual PASS: vv_brig_early_20260912_0323 session82478 terminal0/restored. All3 native captures individually reviewed: two early room/forcefield/NPC views intact, then authored mission-analysis screen. Final _brig, live/CA_ACTIVE/error-empty, missing models(1,0,0,0). No god/save/load commands, original ICARUS ending preserved. Archive brig_early. All20 maps have runtime/local visual evidence;19 playable maps have normal roundtrips, brig retains normal cinematic/death save restrictions. Next final authored transport/hub route sequence.

Final travel sequence ACTIVE session21489: turbolift deck01->02 (requires hub write), turbolift-return deck02->01 (requires read), transporter deck04->01. First name vv_turbolift_20260912_032757. Each runs serially130sec, stops failure, auto-archives results/captures; inspect all screenshots individually. Current pair unchanged, no rebuild/ISO writer until sequence terminal/restored. Remaining deliverable: final paired ISO from preserved baseline + audited VV package + current SP/MP; verify normal boot/newmenu and promote tested package to release staging so hardware files match. Do not mark complete yet.

Final travel sequence21489: outbound vv_turbolift_20260912_032757 PASS terminal/restored, completed hub write proof(1,1,0,0), deck02 live/error-clear; all3 captures individually reviewed and result updated. Return vv_turbolift_return_20260912_033128 now active, requires completed hub read; transporter follows automatically only if pass. Generalized prepared menu test with --iso/--name/--target and --installed-xbe: final-image test can require byte-identical embedded executable and use it without replacement. Syntax checked, not run. Final image should remain under build/research/virtual_voyager to avoid existing cleanup retention of build/xemu ISOs; preserve baseline. No final image created yet, current candidate still old embedded XBEs between probes.

Turbolift HUB RETURN VERIFIED: vv_turbolift_return_20260912_033128 passed terminal/restored with proof(1,1,1,0), deck01/live/error-clear. All3 native captures individually inspected, readable full turbolift menu and intact deck01/HUD; result updated. Sequence21489 continues final transporter vv_transporter_20260912_033459, deck04 authored bridge target expected deck01. No builds/image writes until terminal. Final image/package promotion still pending.

Travel sequence21489 TERMINAL1/restored at transporter. All3 transporter captures individually inspected: complete readable menu then unchanged deck04 lift. No crash: deck04/live/error-clear, proof(1,0,0,0). This fixture is INCOMPLETE, not proven gameplay bug: PAK3 real_scripts/deck04/bridge.IBI arms whereto SET_USESCRIPT deck04/bridge2. Player must then activate transporter pad trigger (model*11, script_targetname whereto, spawnflags3 PLAYERONLY+FACING, angle180,wait15). bridge2.IBI locks player, EV_BEAM_DOWN, unlocks, uses bridge2 target_level_change to tour/deck01 target transporter (HUB). The test only selected destination while player stood in starting lift. whereto has no targetname, so console use whereto will NOT work (Svcmd_Use_f uses G_UseTargets2 targetname). Need proper process-local movement onto pad/facing180 or faithful bounded integration callback; do not bypass authored scripts/teleport directly. Source pad bounds x[-2800,-2664],y[-3096,-2912],z[-3960,-3864]. Default spawn(-3936,-2778,-3936),angle360. Existing replay can move/turn in-process. Current binaries unchanged. Result archived vv_transporter_20260912_033459 runtime_fail, visualmenu reviewed; no final image yet.

Transporter movement probe ACTIVE session23805 vv_transporter_20260912_034453. No engine changes. Fixture now accepts --input-replay and arms menu early(wait120/240) before replay10sec. Replay file build/research/virtual_voyager/transporter_pad_walk.txt walks east4080ms, south-strafe970ms, east680ms, turns180deg, backs east1200ms onto pad; neutral gaps. Based on original BSP world collision slice and prior actual4sec walk endingx-3035, adjusted toward hallway junctionx-3016. Static route plot/json in build/research/virtual_voyager/deck04_pad_route.* is planning only (constant-height model reports raised pad blocked; normal game stairs must handle it). No teleport, unlock, or direct map command; original pad Touch_Multi and armed bridge2 script must produce destination. Confirm final map, captures and actual position before accepting. Pair unchanged. Wait terminal/restored before next writer.


### Transporter trigger isolation, September 12 04:04
- `vv_transporter_20260912_035622` ended/restored, remained deck04/live/error-clear. All three native captures individually reviewed: intact transporter room/HUD. Runtime entity18 (`whereto`) had the expected pad bounds, facing vector west, touch callback8, use26, contents trigger; its cooldown nextthink109550 proves pad contacts are firing. Its use-script pointer was null. Player stood inside the pad at approximately(-2797,-2964,-3908), facing west.
- Added opt-in read-only `--inspect-sp-facing-triggers` through the probe/harness. Dumps actual SP entity array and scripts for spawnflags3; no game behavior changed. Actual Xbox5558 ABI compiled/verified by `scripts/tests/verify_vv_trigger_layout.py`.
- `vv_transporter_20260912_040028` ended/restored. This intentionally stationary arming diagnostic remains deck04 (route checker reports destination FAIL by design). All three captures individually reviewed: readable selected Bridge menu then original lift. Entity18 use script is correctly `deck04/bridge2`, nextthink0. Menu command and arming work; later beam-out script execution is now the diagnostic target.
- Original bridge2 first clears its own use script, then affects Munro: lock, wait1000, beam event, wait500, unlock, use bridge2. No authored script edits, teleports, trigger bypasses or level unlocks were made.
- Short55-second movement probe `vv_transporter_20260912_040405` launched with existing `g_ICARUSDebug=4` to capture the script sequence nearer activation. Must inspect terminal status and restored transaction before another build/ISO writer.

- Short debug probe040405 ended/restored/live/error-clear, but did NOT move from the original lift; all3 native captures individually reviewed. Input file correctly loaded11rows, only row0 recovered. Final command `use bridge`, camera0, player command_time36702, pad remains armed. This test is not beam-out evidence. `Q3_DebugPrint` funnels into `Com_Printf`, compiled out under FINAL_BUILD, so enabling g_ICARUSDebug cannot supply the missing execution trace in this binary.
- Next useful action: bounded diagnostic breadcrumbs/counters at original ICARUS affect resolution and player lock/use callbacks (gated to transporter script names or explicit diagnostic cvar), then coherent SP/MP build and the previously successful130-second movement route. Preserve authored scripts and map transition logic. Do not infer beam-out success from cooldown alone. All owned probes/builds currently terminal; final image/package promotion still pending.


### Bounded beam-out trace, September12 04:17
- Added opt-in `g_SPXBVvTravelTrace[68]` in g_ICARUS.cpp, bounded16records, enabled only by g_ICARUSDebug>=4 and a bridge2 script call. Events:1 RunScript start(entity,sequencer,time),2 script loaded(entity,length,time),3 RunScript success(entity,0,time),4 affect entity lookup(entity/FFFFFFFF,sequencer,time),5 Affect(owner,sequenceID,type),6 player lock(entity,locked,time),7 Q3_Use(entity,isBridge2,time). Probe maps the exported symbol and saves272bytes. No authored gameplay logic changed.
- Build45016 passed pair. Probe041052 ended/restored/live/error-clear, but replay ended(-2827.94,-2887.83,-3935.875), outside pad. All3captures individually reviewed. Trace filter initially used bare script name while G_ActivateBehavior prefixes Q3_SCRIPT_DIR; zero trace is not evidence of failure to execute.
- Prefix filter corrected to match deck04/bridge2 within full path; coherent pair build22495 passed. New route transporter_pad_walk_deeper.txt extends south strafe430ms; nativecaptures delayed to50/75/100sec to avoid interrupting movement. Probe `vv_transporter_20260912_041719` session4016 ACTIVE. Confirm terminal/restored before next writer. No final image or asset promotion yet.

- IMPORTANT correction:041719 ended/restored, all3captures individually reviewed; actualplayer(-2810.126,-3060.107,-3907.875), facing178.38, pad touchcooldown110000, tracezero. Original deck04 entry trigger model*10 bounds(-3048,-2864,-3960)..(-2988,-2840,-3864), spawnflags1,wait3,targett394. t394 runs deck04/null.IBI, affecting whereto SET_USESCRIPT NULL. Replay crossed this trigger AFTER premature menu selection in the lift. Empty pad script does NOT prove bridge2 executed or failed; earlier inference superseded. Fix test sequence: move into room, then choose destination. No gameplay/script fix justified from these failures.

- RESOLVED transporter qualification:042232 terminal0/restored, deck01/live/error-clear, save/readproof(1,1,1,0), missing-modelheader(1,0,0,0). All3nativecaptures individually reviewed bridge arrival/NPC/HUD intact. Trace proves bridge2 runs ontrigger18, resolves player0, Affectsequence97/type56, locks at20100, unlocks+usesbridge2 at21700. Correct fixture selects destination after normal movement crosses room-entry reset. No authored transport bug or script changes needed. All map/localvisual/save/travel gates now passed; final paired image/normal boot and releaseassetpromotion remain.

Final testing build completed September12: final-image normal boot71099 terminal0/restored; deck02/client7/VV1/error-clear/liveness6953->7026; all3nativecaptures individually reviewed. Final asset promotion and component/root integrity verified; ISO SHA256 3b09b5214e599856721f4b18c244ba62c22e9bcb301fa69910b429fe01088275. Completion audit notes/virtual_voyager_testing_2026-09-12.md. Former TODO7 closed; local repo memory updated.
