# Historical to-do snapshot — 2026-09-09

Preserved before the readability refactor. This is historical evidence, not the active tracker; build hashes and status claims below may be superseded. See [GAME_TODO.md](../GAME_TODO.md) for current open work.

# Elite Force X Game To-Do

Detailed Holomatch test evidence and current qualification status live in
`HOLOMATCH_QUALIFICATION.md`. Completed work is retained in Git history rather
than this active list.

## Standard Single Player

- [ ] Resolve muddy 3D dialogue on retail Xbox — **PAUSED at user request;
  do not investigate or change audio further until explicitly requested.**
  Sep 9 retail retest: sound still sounds muddy with the updated build. The
  borg1 opening's positional dialogue remains unresolved despite passing XEMU
  captured-audio checks; earlier 2D narration/communicator dialogue sounded fine.
- [ ] Fix letterbox bars remaining in borg1. Sep 9 retail retest and supplied
  screenshot show the bars still present; the bars also stop short of the
  right edge, leaving a strip of the scene visible. Unresolved; recorded for
  future work.
- [ ] Confirm the Sep 9 freeze fixes on retail hardware separately from audio
  quality; the latest feedback does not explicitly confirm crash resolution.
  XEMU reproduced
  a duplicate archive free during borg1's opening with converted sounds absent;
  FS/WF handles now reserve slots atomically, and failed flare-query submissions
  are retried before readback. The final Release XBE passes both soundbank and
  archived-MP3 progression and captured-audio checks. Evidence and final
  qualification status: `notes/vo_3d_diagnosis_2026-09-09.md`.

- [x] Restore a clean, production-personality boot with no forced map or test
  harness and package one current XEMU image at
  `build/xemu/StarTrekEliteForceX_standard.iso`. The 2,283,077,632-byte image
  has SHA256
  `666F36945A4EC163B048E871621F36B98C6375E26C092763C9CE6A2552C70406`.
- [x] Restore the complete 42-movie retail BIK set through the direct retail
  Bink reader; no XMV conversion is used. The retained-RGBA presentation path
  was visually clean in the bounded XEMU proof, with only the minor occasional
  stutter accepted for the standard candidate. An immutable, screenshot-free
  210-second run of the exact standard ISO used normal boot with no commands or
  injected input. Decoder telemetry advanced through the startup movies, then
  reached phase `131` at 143.3 seconds: the explicit normal, non-key-aborted
  `CIN_PlayAllFrames` exit. Status `1` is `NS_BV_STOPPED`; the frontend main
  loop then advanced from 41 to 1,998 iterations and XEMU remained alive at
  209.7 seconds. The retained final record is
  `scripts/output/final_standard_normalboot_normal_20260830_143244.report.txt`.
- [x] Raise standard-SP player textures to the 128-pixel cap and encode every
  runtime sound as Xbox ADPCM. Stock WAV sources take precedence over MP3s;
  MP3 decoding is used only where the retail/runtime source set has no matching
  WAV. Holomatch's separate 64-pixel player cap is unchanged. The final
  soundbank contains no preserved PCM records, and every loose WAV fallback is
  format tag `0x0069` (XBADPCM). The same executable keeps an unloaded one-shot voice
  channel pending, retries a full async queue, and wakes the channel when its
  buffer becomes resident, preventing ICARUS from treating a never-started
  facial line as complete. Audible quality and no-skip behavior still require
  the final user-driven pass below.
- [x] Restore functional in-game Save Game and Load Game frontends. Save Game
  now creates the first free `eforce0`-`eforce99` slot or confirms an overwrite;
  Load Game preserves the live server until a save is selected, and both flows
  return cleanly through the pause menu instead of killing the game or leaving
  the pause input owner active. The shared SP/Holomatch release build passes.
- [x] Finish the standard frontend/pause implementation audit. Configure now
  preserves and returns to a live paused game; audio Cancel restores the
  opening mix while Accept persists it; Quit Game and Exit Program have working
  confirmations; and Adjust Screen Size now edits and persists one shared
  four-corner safe area. Its zero-margin default maps every D3D viewport and
  scissor to the full display, so adjacent 3P/4P viewports retain zero-width
  seams instead of black gutters. Existing settings migrate without losing the
  prior controller/audio/video fields. The combined production build and clean
  package validators pass.
- [ ] On the final clean image, manually qualify menu navigation, gameplay,
  pause-to-Configure return, audio Cancel/Accept, screen-size
  Default/Cancel/Accept, Quit Game confirmation, save/load, dialogue quality,
  and moving Borg step/servo audibility. Borg
  animation-event diagnostics are present in the active Elite Force cgame/game
  sources, and every referenced Borg movement asset is present in the staged
  soundbank, but audible behavior still needs a user-driven gameplay pass.

## Cooperative

- [ ] Complete the cooperative menu flow:
  - character selection
  - difficulty selection
  - new game
  - load game
- [ ] Implement distinct one-player and two-player cooperative HUD layouts.
- [ ] Restore persistent SP/co-op file logging on retail hardware and capture
  the current approximately 15 FPS case.
- [ ] Move the direct-co-op XEMU test bootstrap out of disconnected-only
  `CL_Frame` so automated two-player renderer qualification can enter gameplay.
- [ ] Qualify a natural cinematic-to-gameplay transition in cooperative play.
- [ ] Qualify representative cooperative controls, gameplay, visuals, stability,
  memory use, and FPS on XEMU/LLE and staged retail hardware.
- [ ] Verify dialogue audio while testing unmuted. Mouth texture animation is
  visually confirmed; audible VO still needs human confirmation.
- [ ] Verify restored SP/co-op ambience and foley while testing unmuted,
  including ship hums and moving Borg servo/footstep events.

## Holomatch Split Screen

- [x] Replace the bounded XEMU text-mirror HUD/divider assumptions with
  structured current-state telemetry. The runtime now publishes the live player
  count, per-slot HUD/status rectangles, and exact zero-area vertical and
  horizontal divider geometry; the harness no longer hardcodes `players=4`.
  Screenshot-free XEMU passes prove the correct 2P top/bottom, asymmetric 3P,
  and four-quadrant 4P layouts from the current production XBE pair.
- [ ] Qualify real local controller assignment and detection with two, three,
  and four physical pads. Production menu/gameplay assignment is implemented;
  virtual P1-P4 controls remain diagnostic-only.
- [x] Qualify four-player local Holomatch with bots across representative maps
  and viewing angles.
- [x] Add the one-pad hardware-test bootstrap: direct `efmp.xbe` boot enters
  `hm_borg1` with P1 human and bot clients 1-3 literally driving P2-P4. The
  same bot-viewport fill and all-ready render barrier apply to three-player.
- [x] Remove the production direct one-pad/bot-viewport hardwire. Normal
  `default.xbe` now reaches the Holomatch player screen and performs the
  validated `efmp.xbe` handoff with `source=xbe`, one human, three bots, and
  both virtual-control flags off. Direct launch and virtual P1-P4 commands are
  retained only as explicit diagnostics. The diagnostic P1 path now replaces
  its one native loopback command in place instead of injecting a competing
  second command; a current four-lane XEMU run proves distinct movement, view
  angles, attack, camera origins, HUDs, and draw work for all four slots.
- [ ] Finish retail P1 control qualification on the current production binary.
  The source-side controller correction uses the raw PS2-profiled builder,
  bypasses packaged PC bindings, clears stale zoom state, and routes Start/Back
  directly. `g_holoIntro 0` also removes the official five-second spawn freeze
  from explicit 3P/4P test launches. Confirm the complete action set,
  non-inverted Y look, and immediate movement on hardware; then qualify two,
  three, and four physical pads through the production menu path.
- [x] Qualify per-player zoom, pickup notices, reward notices, and attacker
  announcements.
- [ ] Verify the audible four-listener mix while testing unmuted.
- [ ] Run materially longer four-player soak tests and measure FPS, memory, and
  stability on XEMU/LLE and staged retail hardware.
- [ ] Decide whether four-player Holomatch meets shipping performance or needs
  scoped visual/rendering sacrifices. Any accepted economy behavior must apply
  only while two or more local split-screen views are active in co-op or
  Holomatch; one-player rendering must remain unchanged. Current observations
  are not final acceptance proof. The first one-pad retail run sustained 20.0
  FPS over 75.36 seconds after startup (15.9 minimum, 25.6 maximum), below the
  25-30 FPS target despite feeling reasonably fluid to the tester. The later
  control-diagnostic run produced eight aligned settled samples averaging
  20.79 FPS (17.1 minimum, 23.7 maximum). Its median inclusive client/render
  boundary was 34.0 ms versus 11.0 ms for the server, consistent with the earlier
  detailed backend/submission bottleneck. Xbox `Hunk_MemoryRemaining()` is
  hardcoded to zero, so its `Memory is low` model-deferral messages are not
  evidence of actual memory exhaustion. The next production heartbeat will
  log real zone used/free/largest-block values. The local UT work-in-progress
  and retail UC2 references confirm that independent cameras still require
  independent visibility/draw work; the reusable retail pattern is a shared
  device/resources plus a centralized lower 3P/4P detail tier. XEMU rejected
  extending the lower-detail policy to Elite Force MDR player bodies: both the
  production-logging and memory-ring diagnostic builds stopped at the first
  complete four-view frame. Restoring the established MDR LOD0 contract
  restored normal frame progression immediately. The retained 3P/4P policy
  applies one LOD step only to non-MDR world models and still exempts
  first-person/depth-hacked weapon models. The returned hardware log also
  proves that the direct one-pad
  profile's `bot_minplayers 7` eventually added clients 4-6 on top of the
  bot-owned P2-P4 viewports. The current production candidate sets the direct
  3P/4P test population to the viewport count itself. An additional XEMU-found
  defect let the every-frame viewport filler insert several `addbot` commands
  before any command had executed; pending commands are now reserved in the
  count. Screenshot-free diagnostic runs prove exactly two bots for 3P and
  three for 4P, literally occupying P2-P3 or P2-P4 with no offscreen extra
  opponent. The normal menu population policy is unchanged. The shared
  three-plus-player
  tier now also retains two of every five periodic spawner particle bursts,
  following UC2's conservative 3P policy while preserving immediate weapon,
  projectile, beam, and hit effects. The same 0.4 policy applies to 3P and 4P;
  1P/2P remain unchanged. This policy is present in the XEMU-qualified combined
  build and still needs visual and FPS qualification on hardware. Historical
  detailed samples
  also expose 5-8 renderer views in ordinary 4P frames and 14 while the
  scoreboard is populated, despite reporting zero portal views. Those extra
  scenes come from animated 3D HUD/scoreboard heads. The shared 3P/4P tier now
  uses the existing static model-icon fallback for gameplay portraits and team
  flags, eliminating those tiny secondary `R_RenderScene` calls without
  removing the HUD information; 1P/2P retain the animated 3D icons. This is in
  the combined XEMU-tested build and requires final visual/FPS qualification on
  hardware.
  The native D3D8 wrapper also used to call `SetVertexShader` for every indexed
  submission even when the fixed-function FVF was unchanged. Historical 4P
  profiles show roughly 300-800 submissions per sampled frame, and the existing
  texture-stage cache proves 65-67% of its state requests are redundant. The
  local UT Xbox renderer independently uses the same explicit vertex-state
  caching pattern. A tracked wrapper in the current production candidate now
  covers every active native
  shader/FVF change and skips identical consecutive requests only under the
  shared 3P/4P economy policy. It logs cumulative requests, emissions, skips,
  and skip percentage. Retail UC2's deferred state also compares stream source
  and stride before emission; the Holomatch push-buffer path previously repeated
  its inline stream-zero stride on every submission. The same tracked 3P/4P
  policy now covers all four native inline-stride call sites and logs an
  independent skip rate. The combined XEMU runs exercised these paths. The
  earlier extractor omitted the vertex/FVF and stream-source marker names;
  those records were not lost to the memory ring. Corrected extraction proves
  balanced production accounting at sample 1536: each cache skipped 466,152 of
  568,177 requests (82%). Across 58 retained bounded
  shader-cost samples, recurring
  top-five work is dominated by player bodies, the phaser, gameplay heads, and
  pickups, so the combined exact-bot-count, non-MDR LOD, static-portrait,
  existing entity-pass/distance, and state-cache changes are aligned with the
  measured workload. Broader shader collapse remains rejected until an A/B can
  justify its known alpha/visual risk. The model-format audit confirms that
  Holomatch lower/upper player bodies are EF MDR models, while heads are MD3.
  The inherited JA Ghoul2 path is not an Elite Force Holomatch workload and is
  excluded from further optimization work. A bounded MDR player-skin cache was
  also rejected and removed. A cache-free rerun still stopped at completed
  frame 4, proving that cache was not causal; the lower MDR LOD experiment was.
  Restoring MDR LOD0 then produced progressing exact-population runs: 3P guest
  FPS 74.3-75.1 and 4P guest FPS 52.2-52.7 in XEMU frame diagnostics. A final
  screenshot-free production smoke of the exact three-bot 4P topology remained
  alive, advanced live bot state in all four tiled 320x240 views, and reported
  a 48.8 FPS guest heartbeat with no below-30 sample. Its cumulative native
  texture-stage cache skipped 4,424,039 of 6,828,704 requests (64%) by sample
  1536. An Elite Force MDR bone-palette cache correction is now also retained
  in `code/renderer/tr_animation.cpp`: the old backend-entity pointer key could
  not recognize the same refEntity copied into separate viewport scene slices.
  Only under the common 3P/4P economy policy, the cache instead uses the stable
  EF refEntity number together with the MDR header, current/old frames,
  interpolation, and render time; 1P/2P keep the original pointer identity.
  No Ghoul2 code participates. A screenshot-free exact-population 4P diagnostic
  run averaged 58.1 guest FPS versus the earlier 52.5 diagnostic, and the final
  production run averaged 58.9 FPS (57.6-60.2) versus the earlier production
  smoke's single 48.8 sample. Production palette accounting at sample 1536
  skipped 25,188 of 30,414 rebuild requests (82%) with zero bypasses. The first
  3P run varied down to 50.3 FPS, while an immediate repeat averaged 64.8
  (58.8-70.8); this proves stable operation and common policy coverage but does
  not support a universal 3P gain over the earlier two-sample 74.7 early-game
  result. Longer screenshot-free, no-poll production runs provide the
  sustained comparison. On the production source rebuilt as the current
  `Aug 29 2026 03:45` XBE pair, exact P1-plus-two-bot 3P averaged 54.5 FPS
  (42.6 minimum, zero below 30) on the same retained source, while the final
  exact-binary P1-plus-three-bot 4P run averaged 54.9 FPS (44.1 minimum, zero
  below 30).
  Both runs had virtual controls off, healthy cache accounting, about 12.9 MB
  free zone memory, and no screenshots or live gameplay polling. An earlier
  longer pair averaged 69.1 FPS for 3P and 47.0 FPS for 4P; together the runs
  clear the 25-30 FPS XEMU target without claiming emulator variance as a
  universal gain. The current 4P median inclusive client/render time was
  18.5 ms versus 0 ms for the server. The
  bounded detailed run further assigns the client cost to the native D3D8
  backend (16 ms median versus 4 ms frontend), with `BeginPush` reserve/wait
  consuming most measured draw cycles. The current bottleneck is therefore
  D3D8 render submission/GPU backpressure, not memory capacity, server work, or
  Ghoul2. No further visual sacrifice is justified by XEMU performance at this
  point. A bounded `purpleparticle` merge experiment was rejected and removed
  because its proof marker never fired. A UC2-derived 32 KiB push-buffer
  kickoff was also rejected and removed after regressing 4P from the retained
  128 KiB policy's 49.4-54.9 FPS range to 40.4 FPS with three below-30
  samples. No unproven renderer change was retained. These remain emulator
  results; retail Xbox is the final performance authority.

  Subsequent bounded work rejected three more native D3D8 candidates instead
  of retaining speculative changes. A generalized adjacent vertex-effect merge
  fired 41 times but reduced 4P to 40.2 FPS versus the restored 49.4 FPS
  baseline. A 64 KiB push-buffer kickoff averaged 44.8 FPS and reached 27.4.
  A capacity-only 2 MiB/128 KiB test also regressed 4P to 44.7 FPS versus 50.7
  with 1 MiB/128 KiB, so the production policy remains unchanged. Finally,
  exact hashing of EF
  BSP world submissions measured only 0.56-0.68% cross-viewport payload reuse
  in 4P and 0-1.23% in 3P, too little to justify a static world cache. This
  diagnostic is frame-build-only and does not touch MDR or Ghoul2. The normal
  production pair was rebuilt afterward and qualified screenshot-free with no
  live polling: exact P1-plus-three-bot 4P averaged 51.9 FPS (40.1 minimum) and
  P1-plus-two-bot 3P averaged 71.1 FPS (64.3 minimum), with no below-30 samples.
  The exact `efmp.xbe` is build ID `Aug 29 2026 05:14:31`, SHA-256
  `8A90416245FE531EEA216333D19F7CC02A644A86D1DBAF7417F692D6D503C114`.
  The unified ISO has been restored to normal menu boot.

  A follow-up frame-diagnostic profiler ranked Elite Force's actual surviving
  shader passes rather than inherited declared stage counts. It is confined to
  the EF BSP/MDR/native D3D8 and Holomatch cgame paths; no Ghoul2 path is
  measured or modified. In one 3P sample, `gfx/misc/spark2` alone produced 135
  separate batches, motivating a bounded common 3P/4P recurring-spawner test.
  Reducing the existing two-of-five particle-child tier to one-of-five gave a
  mixed first pair (55.7 FPS 4P, 65.1 FPS 3P) and a longer 4P replicate fell to
  44.3 FPS, so the candidate was rejected. The established shared 0.4 tier was
  restored and rebuilt. Screenshot-free qualification of the exact retained
  production XBE then averaged 57.2 FPS in 4P (46.7 minimum) and 74.2 FPS in
  3P (69.6 minimum), with zero samples below 30. The current `efmp.xbe` build
  ID is `Aug 29 2026 05:57:51`, SHA-256
  `8DA527101A6B64C9D5B938CE1ECEE535040D8AAEA6BED1CF97DB97D7AC0AA910`.
  The unified ISO was restored to normal menu boot afterward. Retail hardware
  remains the final 25-30 FPS authority.

  Two follow-up attempts to reduce Elite Force `gfx/misc/spark2` submission
  pressure were also rejected. Adding `spark2` to the existing exact sprite
  merge could not fire because Holomatch emits that effect as
  `LE_TRAIL -> RT_LINE`; the 4P diagnostic still showed 64 separate `spark2`
  batches and no avoided flush. Restricting the exception to that exact line
  primitive also recorded no avoided flush and averaged 46.4 FPS, so neither
  merge change remains. A separate conservative sphere cull for EF line/trail
  segments was instrumented into the production profile ring. Every sampled
  viewport reported `offscreenLinesCulled=0` on `hm_borg1`, proving the added
  cull work was inactive; it too was removed without a 3P rollout. No Ghoul2
  code was used or modified by any candidate.

  The exact retained production pair was rebuilt after those removals.
  `default.xbe` is 4,448,256 bytes, SHA-256
  `741C8783245AAC75CA485FB1C7F910A3CDF53769348B3E9D26464E962601AC1C`,
  build ID `Aug 29 2026 06:49:40`; `efmp.xbe` is 4,280,320 bytes, SHA-256
  `832C9F16D2217E844F6987A1A5FC8F532B2A6995B7C3BAFF7F2A1057707DC621`,
  build ID `Aug 29 2026 06:49:46`. The final screenshot-free 4P report averaged
  45.4 FPS (41.7 minimum) and the matching 3P report averaged 75.0 FPS (64.1
  minimum), both with zero below-30 samples and exact P1-plus-viewport-bot
  populations. This reinforces the observed bot-trajectory variance while
  keeping both layouts above the XEMU target. The single retained ISO was
  restored to normal menu boot; retail hardware remains the shipping FPS
  authority.

  Three additional bounded Elite Force renderer candidates were rejected on
  measured evidence. Forcing lower-body MDR LOD1 was active for 18,929
  selections with zero validation rejects, but reduced the same-binary 4P run
  from 54.4 to 43.1 FPS; MDR therefore remains at LOD0. A fixed-function
  texture-pointer cache suppressed about 15% of ordinary binds but measured
  39.29 FPS disabled versus 39.04 enabled, so it was removed. Extending the
  exact sprite merge whitelist to the one-pass `gfx/misc/borgflare` shader
  measured 52.1 FPS disabled and 53.4 enabled, but its avoided-flush marker
  fired zero times; the apparent difference was run variance and the extension
  was removed. These tests used Elite Force MDR/native D3D8 only. No Ghoul2
  code was changed, measured, or retained.

  The exact post-rejection production pair is now `default.xbe` build ID
  `Aug 29 2026 08:41:47`, 4,448,256 bytes, SHA-256
  `A778AE3B20E30633DD12D220D8601B0E1DE2BAA46E54F723FD1AA7DB24E549FE`,
  and `efmp.xbe` build ID `Aug 29 2026 08:41:54`, 4,280,320 bytes, SHA-256
  `7600FA44CFB5E3BCDA09FC7119A46F559D580A2015B9FFE98965CA41BD2B889B`.
  Screenshot-free XEMU/LLE qualification of that exact `efmp.xbe` averaged
  55.0 FPS in 4P (46.4 minimum) and 69.1 FPS in 3P (54.2 minimum), with zero
  samples below 30. Telemetry proves `players=4 humans=1 bots=3` and
  `players=3 humans=1 bots=2`; both use the common `three-plus` quality path.
  The reports are
  `scripts/output/hm_retained_post_borgflare_reject_4p_hm_borg1_20260829_085612.report.txt`
  and
  `scripts/output/hm_retained_post_borgflare_reject_3p_hm_borg1_20260829_085945.report.txt`.
  The single unified ISO was restored to normal menu boot; the repack record is
  `scripts/output/repack_sp_normal_20260829_090459.log`. Retail Xbox remains the
  final 25-30 FPS authority.

  A subsequent Elite Force native-D3D8 experiment tried suppressing the exact
  17-DWORD fixed-function stream-layout descriptor when the current FVF was
  unchanged. The same candidate `efmp.xbe` skipped 82% of descriptors, proving
  the branch was active, but regressed screenshot-free 4P from 51.0 FPS average
  and 43.5 minimum to 46.2 average and 33.5 minimum. The cache state, runtime
  cvar, telemetry extension, packet-size adjustment, and smoke switch were all
  removed. This experiment was confined to Elite Force indexed submission; no
  Ghoul2 code was changed, called, or measured.

  Exact post-removal production was rebuilt and packaged. `default.xbe` is
  build ID `Aug 29 2026 12:31:06`, 4,448,256 bytes, SHA-256
  `83444A6551DB6404D0ADE79DA03B43279876036F04D42FFE313FB38FE8324745`;
  `efmp.xbe` is build ID `Aug 29 2026 12:31:12`, 4,280,320 bytes, SHA-256
  `65A791B6B5225DC86EAFB3F60525B65869594AE2BE197D20AB0A23585427C512`.
  The package validator again checked 1,038 active files and reported
  `codempDependency=false`. Screenshot-free XEMU qualification of that exact
  `efmp.xbe` averaged 51.2 FPS in 4P (39.0 minimum, 59.2 maximum) and 63.2 FPS
  in 3P (49.7 minimum, 68.3 maximum), with zero samples below 30. Runtime
  population stayed exact at P1 plus bots owning P2-P4/P2-P3. Both layouts
  retained the common `three-plus` path; vertex/FVF and stream-source state
  suppression reached 83%, while EF MDR palette reuse reached 82% in 4P and
  80% in 3P. The single unified ISO was restored to normal menu boot in
  `scripts/output/repack_sp_normal_20260829_125907.log`. Retail Xbox remains
  the final performance authority.

  A longer screenshot-free XEMU/LLE soak of this exact production pair is now
  recorded in
  `scripts/output/hm_current_production_4p_10min_soak_hm_borg1_20260829_130225.report.txt`.
  The process remained alive for the full 600-second host window while the
  guest advanced 241.2 seconds. Excluding the startup sample, the retained
  47-sample profile ring averaged 39.8 FPS, with a 26.2 minimum, 30.1 p10,
  57.1 maximum, and 3/47 samples below 30. Every population record stayed at
  one human plus three bots. Used memory increased by 437,959 bytes; minimum
  free memory was 12,785,236 bytes and the minimum largest-free block was
  6,416,352 bytes. This is the current timing authority because it used no
  screenshots and no live gameplay-log polling.

  A separate five-minute structural run is recorded in
  `scripts/output/hm_current_production_4p_5min_strictlog_hm_borg1_20260829_131559.report.txt`.
  `verify_holomatch_split_log.py --bot-viewports` passes it with production
  controls (`virtual=0/0`), P1 as the sole human, authoritative bot states in
  P2-P4, four moving states and distinct completed render views, exact gapless
  320x240 quadrants, independent HUD/status routing, positive draw work in all
  four panes, P2-P4 view weapons, first-person/self filters, 31 heartbeats over
  135.3 guest seconds, 12,762,407-byte minimum free memory, 6,410,880-byte
  minimum largest-free block, and 82,611 bytes of sampled used-memory growth.
  Live polling intentionally disqualifies this companion run as an FPS
  comparison. Both runs created zero screenshots. The verifier's explicit
  bot-viewport mode distinguishes server-authoritative bot cameras from
  physical/synthetic local clients, whose secondary views require snapshot
  merge proof. No Ghoul2 code participates in either runtime path.
  `scripts/output/repack_sp_normal_20260829_132551.log` records the final
  restoration of the one retained ISO to normal menu boot.

## Shared Renderer And Release Qualification

- [ ] Broaden `default.xbe` and `efmp.xbe` qualification across loading, menus,
  UI/HUD, representative SP/co-op/Holomatch maps, controls, gameplay, visual
  correctness, stability/soak behavior, memory use, and FPS.
- [ ] Stage and qualify the current production XBE pair on retail hardware.
- [ ] Before public release, commit the qualified source and produce clean builds
  with `sourceTreeDirty: false`.

## Standing Constraints

- Treat retail `jamp.xbe` machine-code contracts as the renderer authority.
- Keep shared renderer, audio, and input infrastructure in `code\` while
  preserving separate SP/co-op and Holomatch game logic.
- Keep active builds and runtime packages independent of deprecated `codemp\`.
- Build the production pair with `scripts\build_xbox.ps1 -Target spmp`.
- Use XEMU/LLE for emulator qualification and retail Xbox hardware as the final
  performance authority.
- Keep runtime diagnostics file-based and instrument new suspect paths before
  requesting human tests.
- Run `scripts\cleanup_generated.ps1` around completed build/package/test cycles.
