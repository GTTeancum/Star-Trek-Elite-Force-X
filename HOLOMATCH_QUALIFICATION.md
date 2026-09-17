# Elite Force X Beta Qualification Snapshot

## Additional low-window pass closed by user — September 11

The user closed the additional goal of keeping every ten-second Holomatch
window above30native XEMU FPS. That target was not achieved; the pass is closed
by user direction and no further optimization is deferred. Co-op was also
accepted as-is. Retail, controller and release checks remain open in
[the current to-do](GAME_TODO.md). The completed baseline below is historical
qualification evidence, not proof of the abandoned low-window target.

## Latest check — final optimization push completed September 11

Resident world triangles improved matched hm_borg1 gameplay from 18.88 to 33.81
native XEMU FPS (+14.93), with four moving local players and four active bots.
A twenty-minute candidate soak averaged 33.12 native FPS across confirmed
gameplay and ended live without an engine error or observed sustained freeze.
Both surrounding control runs support the gain. The conservative FX culling
and batching paths remain enabled, with no further LOD or texture reductions.

Final production `efmp.xbe` SHA256:
`1f155977c67d71bca5587e95db073617ac5b289e25fca6d317f7eee0afedfb81`.
Fresh SP and MP builds passed. A 180-second full-workload check on the original
XEMU, without renderer overrides, confirmed the actual defaults and live/error-
clear endpoint; confirmed gameplay averaged 34.87 native FPS. The optimization
item is closed. Existing retail, controller, packaging and release checks remain.

Live sampling isolated an XEMU full-cache scan as a substantial slowdown source.
A separate emulator-only guard gained 13.03 FPS with game resident storage off;
that diagnostic result is not an Xbox-code gain and no emulator was replaced.
Native FPS uses XEMU's frame counter over host time, not guest FPS or reciprocal
MSPF. Retail performance and the cause of every historical freeze remain unproven.
The working test ISO was restored; the beta ISO has not been rebuilt with this XBE.
See [diagnosis, comparisons and retained evidence](notes/mp_resident_triangles_2026-09-11.md).

## Historical guest-clock checks and beta priority — September 10, 2026

The MP optimization item is closed for the tested XEMU workload: hm_borg1 and
hm_voy1 each completed 600 host seconds with four moving players and four verified
bots, averaging 30.57 and 28.62 guest FPS. Both ended live without an engine error.
Retail/host presentation and physical-controller release checks remain open;
there is no deferred 30 FPS optimization pass.

Latest full-workload candidate soak: four moving local players and four verified
bots, no crash observed over 600 host seconds. Retained gameplay averaged 30.57
guest FPS and the final minute averaged 24.41 (baseline 25.56/19.44). The 20+
average is met on hm_borg1 in this run; representative-map consistency and
retail/host presentation remain unverified. Keep the cached shader-trace lookup.
See [MP optimization investigation](notes/mp_optimization_2026-09-10.md).

Stable four-player Holomatch consistently averaging above 20 FPS is the user's
current beta performance threshold. Prioritize stability and tester readiness;
do not delay beta preparation for 30 FPS. Close the performance list item once
verified; do not retain deferred optimization work. A later pass gets a new item
when the user requests it. Check longer matches and representative maps, report slow
windows and stuttering, and distinguish XEMU guest elapsed FPS from visible
host speed. Retail Xbox remains the final performance authority. Historical
30 FPS gates below do not override this updated beta priority. This is a target
change, not evidence that the current build has met it or authorization to publish.

Date: 2026-08-01

## Current Qualification Gate - 2026-08-20

The 2026-08-01 beta evidence below is historical. The active qualification
target is now the shared native D3D8 renderer derived from the shipping retail
Jedi Academy multiplayer Xbox renderer, with separate SP/co-op logic in
`default.xbe` and SP-hosted Holomatch logic in `efmp.xbe`.

Current active build command:

```powershell
scripts\build_xbox.ps1 -Target spmp
```

Important current gates:

- `scripts\build_xbox.ps1 -Target spmp` must run two target-scoped passes:
  first a normal SP pass that refreshes `build\release\default.xbe`, then an
  SP-hosted Holomatch pass that emits `build\release\efmp.xbe`.
- The same `spmp` target must refresh both active release packages:
  `build\release\BaseEF\xbox0.pk3` for SP/co-op and
  `build\release\BaseEF\xbox1.pk3` for Holomatch.
- The `sp` and `spmp` targets assert active PK3 freshness after package
  refresh, rejecting packages older than runtime source or package scripts.
- `scripts\verify_build_xbox_contracts.py` verifies that build-graph and
  package-postcondition contract, and protects the forced `xb_log.cpp` rebuild
  that refreshes the `STEFX_RUNTIME_BUILD_ID` `__DATE__`/`__TIME__` literal.
  It also verifies that `build_xbox.ps1` applies the retail renderer ABI defines
  and filters the legacy frame-path modules replaced by `retail_xbox`.
- `scripts\stage_hardware_pk3_test.ps1 -CheckFreshnessOnly` now verifies the
  build-graph contract, release-XBE source freshness, and both
  `STEFX_RUNTIME_BUILD_ID` identities before staging or XEMU proof refresh. It
  also rejects stale `build\release\BaseEF\xbox0.pk3`/`xbox1.pk3` packages
  when runtime source or package scripts are newer than those retained PK3s.
  Release XBE freshness, release PK3 freshness, and runtime build ID failures
  are reported together so one check shows the full release-artifact blocker
  set.
- New hardware stages must contain `buildScriptContract` provenance in
  `HARDWARE_PATCH_MANIFEST.json`.
- `scripts\verify_hardware_stage.py` is preflight schema v12 and rejects
  missing, failed, stale, or hash-mismatched `buildScriptContract` records.
- `scripts\verify_production_hardware_logs.py` is production schema v19.
- `scripts\qualify_hardware_stage.py` is combined audit schema v63 and expects
  stage v12, production v19, and XEMU refresh manifest schema v3.
- The combined audit structurally validates the retail `jamp.xbe` contract
  ledger, including retail address ranges, confidence, retail/shipping evidence,
  and non-empty per-function contract text.
- The same audit validates active retail-source routing: all 11
  `code/renderer/retail_xbox/*_retail.cpp` modules must include
  `retail_renderer_contract.h`, keep the retail namespace wrapper, remain listed
  in `code/x_exe/x_exe.vcproj`, and retain the build-script retail ABI defines
  plus legacy frame-path replacement filter.
- The same audit also requires schema-v2 SP/Holomatch object-compare reports
  with compare-script provenance, root/link input records, object-pair records,
  and per-current/donor-object file hashes. It invalidates saved reports
  when any active retail renderer source, `code/x_exe/x_exe.vcproj`,
  `scripts/build_xbox.ps1`, or `scripts/compare_retail_renderer_objects.py`
  input is newer than the retained comparison report.
- XEMU refresh manifest schema v3 records the successful release freshness
  preflight command/output before proof reports, including build graph, release
  XBE freshness, release PK3 freshness, and per-XBE runtime build ID identity
  lines for `default.xbe` and `efmp.xbe`.
- Saved XEMU proof is invalidated by newer runtime source, newer retained
  release XBEs, newer retained `BaseEF\xbox0.pk3`/`xbox1.pk3` packages, or
  newer proof-harness/gate scripts:
  `build_xbox.ps1`, `build_xbox_patch_pk3.py`, `check_mp_holomatch_ui.py`,
  `ja_xemu_smoke.py`, `run_sp_xemu_smoke.ps1`, `run_mp_xemu_smoke.ps1`,
  `refresh_xemu_qualification_proof.ps1`, `stage_hardware_pk3_test.ps1`, and
  `verify_build_xbox_contracts.py`.

Current retained artifacts are not fully qualified:

- Fresh 2026-08-20 `build\release\default.xbe` and `build\release\efmp.xbe`
  now exist and have focused 4P Holomatch XEMU/LLE proof below. This does not
  replace the broader SP/co-op regression refresh or staged hardware proof.
- Focused 2026-08-21 SP `borg1` qualification closes the reported black
  surfaces 90 degrees right of the first-control view. BSP tracing identified
  `textures/common/sky`; the canonical package had omitted its retail
  `scripts/voyager.shader` definition and therefore allowed the map material
  to resolve through the default-shader path. The package builder and console
  shader list now retain stock `voyager.shader` alongside the focused
  `borg.shader` blend corrections. The rebuilt 257,135,710-byte `xbox1.pk3`
  has SHA256
  `A4A1D63520DCEDDFA2C5D3C8915CAEB791BE5804C9CCFCABBE8A6A947A77D40B`,
  contains the exact retail `skyParms env/stars 512 -` contract, all six star
  cube DDS faces, and `maps/hm_borg1.aas`. The `borg1` material audit reports
  zero used missing assets and zero used unresolved materials. The
  180-second no-screenshot run
  `scripts/output/sp-borg1-skyfix-live_borg1_20260821_111421.report.txt`
  remained alive at controlled shutdown; its extracted `ef_sp_log.txt`
  contains no shader fallback. The user then inspected the exact right-facing
  surface live in a rerun and confirmed it fixed. This is focused SP visual
  proof, not broader SP/co-op, Holomatch, soak, performance, or hardware
  acceptance.
- Focused 2026-08-24 SP audio work removed the Xbox-only early exits that had
  disabled ambient-set parsing and updates. A 150-second normal `borg1` run
  advanced beyond frame 2,640 without freezing and logged the `borgchasm` set,
  `borgset1.wav`, `yellowringsslow.wav`, `borgregenhum.wav`, `spark5.wav`,
  `spark3.wav`, `step4.wav`, and `step1.wav` reaching playback while Janeway
  VO remained active. The extracted playback mirror was rotated by bounded
  artifact cleanup after its evidence was recorded. The final uncapped build
  is the 4,435,968-byte `default.xbe`, runtime ID `Aug 24 2026 16:43:17`,
  SHA256 `309E39465D7B8912624EA83EBEECDBA82986244CEDF09DE35BE32CC292D2AE0E`.
  Its retained 60-second smoke report is
  `scripts/output/audio_foley_borg1_final_uncapped_smoke_borg1_20260824_164429.report.txt`;
  it remained alive, parsed 103 ambient declarations with 14 precached and
  zero missing sets, and logged no audio playback or missing-stream failure.
  Human audible qualification near a moving Borg remains required.
- Focused 2026-08-24 Voyager Crew qualification now covers the PS2-parity
  category screen, Senior Staff roster, Janeway biography pages one and two,
  Alpha Squad roster, Beta Squad roster, Hazard Suit diagram, and U.S.S.
  Voyager diagram. The exact release pair is the 4,427,776-byte
  `default.xbe`, runtime ID `Aug 24 2026 04:17:36`, SHA256
  `28ED7FD6491C60F84B2F1B6D4678DA62CD54518553865003402174FE37E5C44E`,
  and the 4,247,552-byte `efmp.xbe`, runtime ID `Aug 24 2026 04:17:42`,
  SHA256
  `82939C53C741B06E64F148B8A2BBBA623887405F9C11F024321835FAC69F4FF2`.
  The repacked current-build visual reports are
  `scripts/output/crew_all_screens_final_normal_20260824_043636.report.txt`
  and
  `scripts/output/crew_late_screens_final_normal_20260824_044054.report.txt`;
  their retained frames were inspected chronologically and show complete,
  non-overlapping retail text and art in all eight states. The late-run Xbox
  log proves a 45,632-byte `sp_normaltext.dat` load with 1,035 parsed entries,
  Janeway voice playback from
  `sound/voice/computer/misc/janeway.wav`, and real biography navigation at
  `page=1 pages=2 lines=12`. The 260-second late tour remained alive through
  Voyager. Audible voice quality and all 23 character selections still need
  human qualification; the implemented menu flow and current Janeway runtime
  path are otherwise visually and structurally proven.
- Focused 2026-08-24 Holomatch player-options qualification now covers four
  simultaneously active local-player panels, distinct character/control/
  autoaim/crosshair/vibration/invert selections, and the eight-option layout at
  native 640x480. The current 4,427,776-byte `default.xbe` has runtime ID
  `Aug 24 2026 11:53:40` and SHA256
  `91BB551292704EF63C7AD040E297AE815EA676EBF4E35393CE285B1B8C0B08F5`;
  the current 4,247,552-byte `efmp.xbe` has runtime ID
  `Aug 24 2026 11:55:13` and SHA256
  `692101E55D178A0DBAB8804921730E60B08B3EF4E87348A7220A9772FFE2CA87`.
  The report
  `scripts/output/menu_holomatch_4p_outfit_icons_normal_20260824_115650.report.txt`
  remained alive through controlled shutdown. Its original-resolution frame
  `C:\Games\Emulators\Xemu\JACodex\screenshots\xemu-2026-08-24-11-58-41.png`
  was inspected directly: all four panels are complete and unclipped, and each
  selected `GOLD`/`RED`/`BLUE` outfit now draws the corresponding shipping
  `models/players2/<model>/icon_<skin>.jpg` portrait instead of a fixed Crew bio
  image. Production still requires a real controller for each panel; the
  four-pad override remains keyed only to the bounded menu-smoke target. The
  preceding menu-to-`hm_borg1` engage proof
  `scripts/output/menu_holomatch_4p_options_engage_retry_normal_20260824_114541.report.txt`
  showed four live quadrants and four view weapons through 147 seconds. It is
  functional evidence only: concurrent host projects contaminated XEMU timing,
  and the harness exported no accepted gameplay FPS samples.
- A fresh full non-reuse `spmp` build after the ambient-audio restoration
  produced the 4,435,968-byte `default.xbe`, runtime ID
  `Aug 24 2026 16:51:00`, SHA256
  `CC8602C8E6727890D75F7D26C843A5121864C253DDF89D3B928B68052E25968D`, and
  the 4,255,744-byte `efmp.xbe`, runtime ID `Aug 24 2026 16:52:50`, SHA256
  `683232C66FA0FED593352B7007816A1057A2D213FBADCF2C087087C1E5564D66`.
  `verify_build_xbox_contracts.py` and the build-integrated SP-hosted
  Holomatch architecture/package gate pass. Exact-binary options proof is
  `scripts/output/currentpair_menu_hm_4p_visual_normal_20260824_171013.report.txt`;
  all 14 retained frames were inspected sequentially and show four complete,
  stable, unclipped panels with distinct characters, outfits, and settings.
  Exact-binary menu-to-match proof is
  `scripts/output/currentpair_menu_hm_4p_engage_array_visual_normal_20260824_172112.report.txt`
  with contact sheet
  `scripts/output/currentpair_menu_hm_4p_engage_array_visual_normal_20260824_172112_contact.png`.
  All 13 retained gameplay frames were inspected sequentially across roughly
  155 seconds of live `hm_borg1`: every frame has four coherent, independently
  changing quadrants, all four first-person phasers, correctly rooted beams,
  independent HUDs, textured world geometry, bots, combat, damage, pickups,
  deaths/respawns, and teleport effects. No pane froze or went blank, and no
  pure-black shader surface, exposed self-model polygon, or missing lower-player
  weapon appeared. The process remained alive through controlled shutdown.
  The strict split verifier passes the late-attached 40.3-second `efmp`
  telemetry window: all four clients moved 220-1,138 units with distinct
  commands and attacks, three bots were active, P2-P4 had positive snapshots,
  self filters, and view weapons, all four HUD quadrants were valid, backend
  state was `0x4`, and four listeners were compiled and active. Used memory grew
  by 71,028 bytes; minimum free memory was 12,871,654 bytes and minimum
  largest-free memory was 6,388,704 bytes. Guest/game FPS over the nine accepted
  MP samples averaged 16.3 and ranged from 14.0 to 18.3. Concurrent host work
  taints this timing, and neither this bounded XEMU run nor its approximately
  2-4 FPS expected clean-host allowance is four-player performance acceptance.
- Focused 2026-08-24 `hm_voy1` breadth proof uses the instrumented current
  4,440,064-byte `default.xbe`, runtime ID `Aug 24 2026 19:28:45`, SHA256
  `78C28B05528BDE41E7FB37C08B1A1D5EAB27765B455151894EA93230A87347D7`,
  and 4,255,744-byte `efmp.xbe`, runtime ID `Aug 24 2026 19:28:56`, SHA256
  `79A2411507CDAD90196C2135FBAA816C7D915104910D49A1BC276941C7CD58D0`.
  The report is
  `scripts/output/hm_voy1_visual_late_stage_hm_voy1_20260824_193416.report.txt`
  with contact sheet
  `scripts/output/hm_voy1_visual_late_stage_hm_voy1_20260824_193416_contact.png`.
  All four retained original frames were inspected sequentially. They show
  coherent textured four-way rendering at materially different positions and
  angles, independently changing health/ammo, four first-person phasers,
  active beams, opponents, damage, and all four HUDs. The late telemetry read
  reached frame 737 and game time 38,650 with all four local clients active,
  distinct nonzero commands, and three bots active. The shared native backend
  had completed `RB_EndSurface` and `EndPush`; memory remained healthy at
  9,406,379 bytes free with a 6,547,264-byte largest block. The process was
  alive at controlled shutdown. This run did not reproduce the earlier
  identical-frame `hm_voy1` observation; it closes the focused second-map
  visual concern but does not replace a materially longer soak, physical-pad,
  audible, or retail-hardware gate.
  The same exact XBE pair also has refreshed `hm_borg1` proof in
  `scripts/output/currentpair_hm_borg1_visual_late_hm_borg1_20260824_193914.report.txt`
  and
  `scripts/output/currentpair_hm_borg1_visual_late_hm_borg1_20260824_193914_contact.png`.
  All four original frames were again inspected sequentially: each shows four
  coherent but independently changing Borg-map viewpoints, four view weapons
  and HUDs, sustained phaser fire, damage, opponents, and broad near/far,
  corridor/room, floor/ceiling viewing angles. Late telemetry reached frame
  444 and game time 26,350 with four active, independently commanded local
  clients and two bots already active; the same pair's later `hm_voy1` sample
  above confirms all three requested bots active. Used memory was 10,111,039
  bytes with 13,271,008 bytes free and a 6,744,384-byte largest block. The
  process was alive at controlled shutdown. Together these exact-pair runs
  close the focused representative-map/view-angle XEMU visual gate; longer
  soak, physical-pad, audible, and retail-performance qualification remain
  open.
- The retained SP/Holomatch object-compare reports are stale versus current
  retail renderer source/build inputs and must be regenerated before retail
  contract evidence can pass again.
- The retained hardware stage
  `build\hardware\StarTrekEliteForceX-Beta-20260801` predates the
  `buildScriptContract` manifest field and is rejected by the current preflight.
- The current combined audit is expected to fail until the object comparisons,
  broader XEMU matrix, hardware stage, returned retail logs, and filled
  SP/co-op/Holomatch observation evidence are refreshed.

After a fresh `scripts\build_xbox.ps1 -Target spmp`, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\stage_hardware_pk3_test.ps1 -CheckFreshnessOnly
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\refresh_xemu_qualification_proof.ps1
```

The refresh helper writes `scripts\output\xemu_qualification_proof_refresh_*.json`
and, unless `-SkipAudit` is used, immediately runs
`scripts\qualify_hardware_stage.py` with `--xemu-refresh-report` bound to that
manifest. For a later manual audit rerun, pass the latest refresh manifest
explicitly with `--xemu-refresh-report`. If no explicit XEMU report overrides
are supplied, a standalone audit also auto-selects the newest
`scripts\output\xemu_qualification_proof_refresh_*.json` manifest when one is
present. Schema v63 records that choice in `xemuProofRefreshReport.selectionMode`
as `explicit`, `auto`, `disabled-by-explicit-proof-reports`, or `none`.

## Beta Candidate

- Package: `build/beta/StarTrekEliteForceX-Beta-20260801`.
- XISO: `StarTrekEliteForceX-Beta-20260801.iso`.
- XISO SHA256: `F434561D66B4687F2CF06DA36D5707DBC8BB7F6B1F3A25ED713A5B236BD54C83`.
- Entry point: `default.xbe`.
- SP/co-op personality: `default.xbe`.
- Holomatch personality: `efmp.xbe`.
- Shared runtime: `BaseEF`.
- `codemp/` build/runtime dependency: none.
- Diagnostic or smoke markers in XISO: zero.

Component hashes:

| Component | Bytes | SHA256 |
|---|---:|---|
| `default.xbe` | 4,354,048 | `469F0D271B268B1781BF93753C84E63AE98AE9D11D6A87AF37094D4A0A311B89` |
| `efmp.xbe` | 4,091,904 | `0DF4E39C0BAB86F6D1D47908DF50E690CAE2837C30A2990E95D86D84BA7DFAD7` |
| `BaseEF/xbox0.pk3` | 283,522,258 | `4DBD3F330B7E70861D174AC88F70C528202B917B556109C97A86A9C24C4B2E0B` |
| `BaseEF/xbox1.pk3` | 224,620,678 | `D34C209C70B1D41F3D402AAEE6AF715447F30E4BF2CE48442A76799E07E70870` |
| `BaseEF/soundbank/sound.bnk` | 500,246,186 | `D5D2F4024BC74975065B632E4981096EDBC3E0194D25EBA36F84E4975E690389` |
| `BaseEF/soundbank/sound.tbl` | 103,623 | `11153B73334F267CB118F48FBDF27DA6A17A55AB81A44FA55369DE77234EB247` |

The complete machine-readable manifest is
`build/beta/StarTrekEliteForceX-Beta-20260801/release_manifest.json`.

## Build Evidence

- Full SP build: passed.
- Full SP-hosted Holomatch build: passed.
- Build log: `build_spmp_beta.log`.
- `efmp.xbe` is built from the shared `code/` engine plus
  `code/holomatch/official` game/cgame and bot code.
- The package verifier checked 1021 `code/` source files and found no
  `codemp/` dependency.
- `xbox1.pk3` contains:
  - 33 optimized multiplayer BSPs.
  - 33 patched AAS checksums.
  - 1277 DDS entries: 1149 DXT1 and 128 BGRA32.
  - Zero original JPG/TGA/PNG texture entries.
  - Zero legacy UI scripts.
  - Official bot, weapon, pickup, arena, shader, and loading support assets.
- The runtime stage contains:
  - Zero loose MP map overrides.
  - Zero loose original-image fallbacks for `xbox1.pk3`.
  - Zero loose UI scripts.
  - 7971 shared soundbank records: 7947 Xbox ADPCM and 24 preserved PCM.

## SP And Co-op

- Campaign loading, cinematics, gameplay, and main-menu return pass.
- Campaign loading retains the SP layout and localized quoted level title.
- Two-player co-op retains the canonical full-screen introduction before
  splitting into two live viewports.
- P2 has an independent camera, origin, input, HUD, and weapon view.
- P1/P2 presentation and Borg materials are visually intact.
- Focused proof:
  - `scripts/output/stefx-beta-coop-presentation-final_normal_20260801_082758.report.txt`
  - `scripts/output/stefx-beta-coop-p2input_normal_20260801_083709.report.txt`

## Holomatch

- FFA with bots, weapon pickup/ammo, firing, phaser beam, damage, HUD, loading,
  controls, scoreboard, and audio passes.
- CTF boots into a playable team match with three bots; movement, firing, and
  damage pass.
- Loading screens retain the signed-off MP backdrop/metadata presentation and
  shared SP LCARS wheel.
- Focused proof:
  - `scripts/output/stefx-beta-hm-ffa_hm_borg1_20260801_084654.report.txt`
  - `scripts/output/stefx-beta-hm-ffa-combat_hm_borg1_20260801_085108.report.txt`
  - `scripts/output/stefx-beta-hm-ctf-play_ctf_dn1_20260801_085745.report.txt`
- User signoff remains recorded for HUD, loading screens, controls, phaser,
  footsteps, bot visibility/damage, pickups, movement, stairs, and teleporters.

## Cross-mode Qualification

- XBE roundtrip:
  - `scripts/output/stefx-beta-xbe-roundtrip6_normal_20260801_095353.report.txt`
  - SP menu -> Holomatch -> live FFA -> SP menu remained stable.
- Final uninterrupted mini-soak:
  - `scripts/output/stefx-beta-final-minisoak2_borg2_20260801_101352.report.txt`
  - `scripts/output/stefx-beta-final-minisoak2_borg2_20260801_101352_contact.png`
  - One XEMU session covered SP gameplay, menu return, co-op introduction,
    live split-screen with independent P2 movement, menu return, XBE handoff,
    and more than four minutes of stable Holomatch.
- Marker-free release-disc boot:
  - `scripts/output/stefx-beta-cleaniso-final_normal_20260801_114812.report.txt`
  - `scripts/output/stefx-beta-cleaniso-final_normal_20260801_114812_contact.png`
  - Status: pass; correct six-item shared main menu remained stable.

## Deferred

- XEMU/LLE frame-rate optimization is post-beta. Functional stalls, crashes,
  data corruption, or visual/gameplay regressions remain blockers; raw FPS
  improvement does not.
- Four-player local Holomatch split-screen is in progress as of 2026-08-20, not
  shipping-qualified. The current source launches a 4P local FFA-with-bots
  match from the Holomatch button and adds virtual P1-P4 controls plus P2-P4
  local refdefs for runtime proof. Fresh XEMU build, visual, control, HUD,
  bot-fill, memory, audio, and bounded-soak evidence now exists below; real
  controller assignment, representative-map coverage, long soak, and staged
  hardware proof are still required. Returned `ef_mp_log.txt` files should pass
  `python scripts\verify_holomatch_split_log.py <log>`; the stricter runtime
  qualification should also pass `--min-bots 3 --require-attack
  --require-positive-snapshot-adds --require-fp-filter-slot 1
  --require-fp-filter-slot 2 --require-fp-filter-slot 3 --min-elapsed-seconds
  90 --min-heartbeat-fps 15 --min-largest-free 1048576 --max-used-delta 0`
  after a long enough match. For full staged hardware qualification, pass
  `--require-hm-split-log` to `scripts\verify_production_hardware_logs.py` or
  `scripts\qualify_hardware_stage.py`.
  Latest focused XEMU/LLE proof:
  `scripts/output/hm-split-heartbeat-fps-4p_hm_borg1_20260820_132707.report.txt`
  and
  `scripts/output/hm-split-heartbeat-fps-4p_hm_borg1_20260820_132707_contact.png`.
  That run used 2026-08-20 13:24:43 `default.xbe` and 13:26:02 `efmp.xbe`,
  exited alive, and visually shows all four 4P Holomatch panes rendering live
  `hm_borg1` geometry. The earlier broad P2-P4 HOM failure is not reproduced.
  The production heartbeat mirror now gives accepted log-side FPS samples:
  12 active/gameplay samples averaged 34.1 guest/game-clock FPS with a 22.3
  minimum and 4/12 below 30. Delivered XEMU wall-FPS averaged 25.0 with
  screenshot/poll stalls included, so 4P remains performance-risky and staged
  hardware FPS is still required. Split-camera near-wall views remain open.
  The 2026-08-21 all-slot collision and no-cull diagnostics supersede that
  near-wall interpretation for the broad black-void captures. Collision proof
  reported 758 clusters, five inline models, linked P1-P4 entities, valid
  renderer clusters, and empty eye contents. With culling disabled, an affected
  P4 frame still showed isolated back sides of brushes while the local player
  was airborne at `z=-241`; the blind synthetic controller had reached exposed
  map space. The virtual fallback path now uses a full player-box wall trace
  plus a forward floor probe and logs `STEFX_HM_SPLIT_VIRTUAL_AVOID`; real pad
  input is unchanged. Supporting normal-culling visual proof is
  `scripts/output/hm-virtual-avoid-proof_normal_20260821_010212.report.txt`,
  using 4,395,008-byte `default.xbe` SHA256
  `94B634FA3082DB4CD5A5D5757AB39207A8FB5B64A9DC7BF5AE3F65C1D2FAC55A`
  (runtime ID `Aug 21 2026 01:01:18`) and 4,194,304-byte `efmp.xbe` SHA256
  `6056A7399A6E56524A62D4ED4AB65769FA78762469E559A6F0F817E49FDF2EF2`
  (runtime ID `Aug 21 2026 01:01:22`). All six retained frames were inspected
  sequentially and show four coherent worlds, HUDs, weapons, and effects with
  no isolated-brush black void. The 90-second run exited alive; guest/game FPS
  averaged 26.1 (21.2-33.2) and wall throughput averaged 18.5. The retained
  candidate suppresses periodic virtual jumps while avoidance is active. Its
  exact-binary proof is
  `scripts/output/hm-virtual-avoid-finalproof_normal_20260821_010758.report.txt`,
  using 4,395,008-byte `default.xbe` SHA256
  `A92D119388150237F532BCB3C2728CF45492568E291E6691B432D07D73330662`
  (runtime ID `Aug 21 2026 01:07:03`) and 4,194,304-byte `efmp.xbe` SHA256
  `D6BD5A6A3F6A08D8C1396AB126D64E85CE2CEA5195691154580BD74012CD5403`
  (runtime ID `Aug 21 2026 01:07:08`). All three retained frames from the
  65-second run were inspected sequentially. Every quadrant keeps coherent,
  connected world geometry, HUD, weapon, and effects; the first frame's dark
  P2 opening is a grounded, empty-contents position in valid cluster 598, not
  the prior airborne exposed-space failure. The process exited alive;
  guest/game FPS averaged 26.5 (19.4-30.5) and wall throughput averaged 18.0
  (10.1-31.3). Representative maps, real controllers, long soak, and staged
  hardware remain required.
  Follow-up audio-on XEMU/LLE proof:
  `scripts/output/hm-split-audio-4listeners-4p_hm_borg1_20260820_135837.report.txt`
  used 2026-08-20 13:56:02 `default.xbe` and 13:57:18 `efmp.xbe`, exited
  alive, and proved the Xbox audio backend is initialized in Holomatch
  (`backend=0x00000004`) with four active compiled listeners
  (`listener=0x00040004`) and P2-P4 listener updates
  (`listenerMask=0x0000000e`). The same run logged 168 sound registrations,
  226 starts, 460 loops, 885 respatializes, 20 voice starts, and repeated
  `lipActive` samples. This supersedes the earlier single-listener audio proof.
  Actual audible 4P mix quality, staged hardware behavior, and the known
  voice-line/face-animation issue remain open. The focused split verifier can
  now require this evidence from screenshot-free monitor reports with
  `--audio-only --require-audio-backend --require-audio-listeners
  --min-audio-starts 100 --min-audio-voice-starts 8
  --require-audio-lip-active`, or can apply the same audio flags alongside the
  normal split gates for returned full logs. The production hardware verifier
  can require the same audio subset when `--require-hm-split-log` is combined
  with the matching `--hm-split-require-audio-*` options. Guest/game-clock FPS averaged
  37.3 (35.1 minimum), while delivered XEMU wall-FPS averaged 9.9, so staged
  hardware proof is required before making an audio-on 4P performance call.
  Current combined visual/audio XEMU/LLE proof after the dead split-pane
  fallback and dead virtual-player respawn-input change:
  `scripts/output/hm-split-deadview-fix-4p_hm_borg1_20260820_143939.report.txt`
  and
  `scripts/output/hm-split-deadview-fix-4p_hm_borg1_20260820_143939_contact.png`.
  That run used 2026-08-20 14:37:05 `default.xbe` and 14:38:29 `efmp.xbe`,
  exited alive, and the sampled contact sheet shows all active P2-P4 split
  panes rendering live `hm_borg1` geometry instead of the earlier broad HOM
  failure. It did not catch a local death, so the new
  `STEFX_HM_SPLIT_DEAD_CMD`/`STEFX_HM_SPLIT_DEAD_VIEW` breadcrumbs still need
  a returned death/respawn log or longer soak before the death-path fix is fully
  proven. Audio stayed active in the same visual pass with backend `0x4`,
  listener state `0x00040004`, listener mask `0x0000000e`, 174 registrations,
  269 starts, 595 loops, 1048 respatializes, and 16 voice starts. Guest/game
  FPS averaged 42.4 with a 33.3 minimum and 0/4 below 30; delivered XEMU
  wall-FPS averaged 14.6 with screenshot/poll overhead, so staged hardware
  remains the performance authority.
  Current 4P economy-mode proof after skipping dynamic lights, flares, and
  Xbox world effects only for `r_splitScreenEconomy 1`, plus fast local
  split-player respawn:
  `scripts/output/hm-split-economy-fastrespawn-4p_hm_borg1_20260820_150454.report.txt`
  and
  `scripts/output/hm-split-economy-fastrespawn-4p_hm_borg1_20260820_150454_contact.png`.
  That run used 2026-08-20 15:02:27 `default.xbe` and 15:03:49 `efmp.xbe`
  (`efmp.xbe` SHA256
  `151DAE9953D3BBE2C84A6C9F4FAC6353200BAFB3F122BF7F15F2825E68ED5792`),
  exited alive, and visually shows all four Holomatch panes rendering live
  `hm_borg1` geometry and per-pane HUDs. This supersedes the broad P2-P4 HOM
  failure as current progress proof, but it is not visual signoff: lower panes
  can still expose black void/portal-like gaps from some angles. Guest/game FPS
  averaged 36.6 with a 22.0 minimum and 1/7 samples below 30; delivered XEMU
  wall-FPS averaged 26.0 with 4/6 samples below 30. The same report passes
  the focused audio verifier with backend `0x4`, four listeners, listener mask
  `0x0000000e`, 172 registrations, 620 starts, 1348 loops, 2230 respatializes,
  36 voice starts, and `lipActive=1`. Staged hardware remains required before
  accepting 4P performance or audible mix quality.
  Newest XEMU/LLE self-filter proof after widening the hosted Holomatch
  secondary-pane self-model suppression:
  `scripts/output/hm-split-self-filter-4p_hm_borg1_20260820_151509.report.txt`
  and
  `scripts/output/hm-split-self-filter-4p_hm_borg1_20260820_151509_contact.png`.
  That run used 2026-08-20 15:12:35 `default.xbe`
  (`SHA256 9B49A9143D3C018E539C9E0B7C4B8F6126F9E717DB0D6EA13F9CB0EA2ED6E28E`)
  and 15:13:56 `efmp.xbe`
  (`SHA256 891D164B8162FFE6ECA3C623EF9B5AB526D85BB8B829E2A6EA3F1F7DEBB66FE9`),
  exited alive, and visually shows all four panes rendering live `hm_borg1`
  geometry and per-pane HUDs. The previous broad P2-P4 HOM/void pattern is not
  visible in this sample; some views remain close to dark map walls, so this
  improves the visual proof but does not close full map/angle qualification.
  Guest/game FPS averaged 43.0 with a 33.0 minimum and 0/7 samples below 30;
  delivered XEMU wall-FPS averaged 29.5 with 3/6 samples below 30. The same
  report passes the focused audio verifier with backend `0x4`, four
  compiled/active listeners, listener mask `0x0000000e`, 173 registrations,
  514 starts, 1239 loops, 2202 respatializes, 37 voice starts, and
  `lipActive=1`. Staged hardware remains required before accepting 4P
  performance or audible mix quality.
  Newest XEMU/LLE proof after adding per-pane first-person weapon clones for
  external split views:
  `scripts/output/hm-split-viewweapon-4p_hm_borg1_20260820_153113.report.txt`
  and
  `scripts/output/hm-split-viewweapon-4p_hm_borg1_20260820_153113_contact.png`.
  That run used 2026-08-20 15:28:28 `default.xbe`
  (`SHA256 B62E7049FE508FC56BEFA2C681BE75C0DF9A89E367244CB314E28B2A0689D832`)
  and 15:29:55 `efmp.xbe`
  (`SHA256 9F039285194ACDFFFCA9DD60D479DEB5E4DCFEB80B1CF98B4A5CB7A6B07DFC2E`),
  exited alive, and visually shows all four panes rendering live `hm_borg1`
  geometry, per-pane HUDs, and first-person weapons. This closes the immediate
  P2-P4 no-weapon regression for the current XEMU proof window. Guest/game FPS
  averaged 43.7 with a 31.2 minimum and 0/7 samples below 30; delivered XEMU
  wall-FPS averaged 28.6 with 3/6 samples below 30. The same report passes the
  focused audio verifier with backend `0x4`, four compiled/active listeners,
  listener mask `0x0000000e`, 176 registrations, 543 starts, 1408 loops, 2281
  respatializes, 33 voice starts, and `lipActive=2`. Staged hardware remains
  required before accepting 4P performance or audible mix quality.
  Superseding no-log monitor and visual proof after exporting split proof
  arrays and tightening the virtual attack/state evidence:
  `scripts/output/hm-split-monitor-proof-4p-v3_hm_borg1_20260820_160457.report.txt`
  and
  `scripts/output/hm-split-viewweapon-proof-4p-v3_hm_borg1_20260820_160815.report.txt`
  with contact sheet
  `scripts/output/hm-split-viewweapon-proof-4p-v3_hm_borg1_20260820_160815_contact.png`.
  These runs used 2026-08-20 16:00:43 `default.xbe`
  (`SHA256 277357E319D1B40610E5E8E4FD92201D1357F4CA2A88B67752771A5BCB48946F`)
  and 16:02:17 `efmp.xbe`
  (`SHA256 55A9969954132F3FAF9C7EEBB8A35FB5ADFF2D46ABDE5DD8CB2FE71FE6CB6A13`).
  Both reports pass `verify_holomatch_split_log.py` with required four local
  split players, unique P1-P4 virtual commands including attack, P2-P4
  external refdefs/snapshots, positive P2-P4 view-weapon clones, per-pane
  HUD/status/divider proof, self/first-person model filtering, memory headroom,
  and audio backend/listener/voice/lip activity. The contact sheet visually
  shows live P1-P4 `hm_borg1` quadrants with first-person weapons in every
  pane, closing the returned P2-P4 no-weapon regression for this proof window.
  The monitor run exited alive at 31.3 guest/game FPS average, 24.2 minimum,
  4/9 samples below 30, 22.3 wall-FPS average, 12.1 minimum, 6/8 below 30,
  largest free block minimum 7.26 MiB, `bots=0`, and audio max voice starts
  54/`lipActive=1`. The visual run exited alive at 31.1 guest/game FPS average,
  24.5 minimum, 3/8 below 30, 25.6 wall-FPS average, 12.1 minimum, 4/7 below
  30, largest free block minimum 7.26 MiB, `bots=0`, and audio max voice starts
  67/`lipActive=2`. Bots and accepted staged hardware performance remain open.
  Superseding first-person weapon proof now comes from each local player's
  authoritative official EF player state instead of renderer-side P1 clones.
  The opt-in `stefx_hm_split_weapon_proof 1` diagnostic grants real ownership
  and ammo, then requests P1-P4 weapons 1/2/3/4 through the normal
  `usercmd.weapon` and `PM_BeginWeaponChange`/`PM_FinishWeaponChange` path.
  Evidence is
  `scripts/output/hm-split-distinct-weapons-4p_hm_borg1_20260820_200111.report.txt`
  with contact sheet
  `scripts/output/hm-split-distinct-weapons-4p_hm_borg1_20260820_200111_contact.png`.
  The run used `efmp.xbe` SHA256
  `2A38D2BF44CF39C58671DD89938322A8EF5047491080288425BA188E4DB9ED38`,
  exited alive, kept three bots active, and held the texture allocator exactly
  at 2,985,856 bytes used with a 7,499,904-byte largest free block. Every one
  of the five saved 640x480 frames was reviewed individually: all four panes
  contain complete `hm_borg1` geometry and visibly distinct phaser,
  compression-rifle, IMOD, and scavenger-rifle first-person models. Monitor
  samples repeatedly agree across authoritative state, requested command, and
  cgame submission for weapons 1/2/3/4. P2 and P4 briefly showed their spawn
  phasers after deaths while commands continued to request 2/4, then normal
  weapon selection restored P2; longer respawn/reselection proof remains open
  for P4. The run averaged 18.4 guest/game FPS (13.2 minimum); screenshot and
  monitor stalls reduced delivered wall FPS to 13.0 average, so this is visual
  and ownership proof rather than accepted performance evidence. The official
  bottom HUD was still replicated from P1 in this older proof; the newer
  independent official-HUD proof below supersedes that limitation.
  The exact current binary was then rebuilt with deterministic virtual fire
  release during weapon changes and qualified in
  `scripts/output/hm-split-distinct-weapons-respawn-4p_hm_borg1_20260820_200641.report.txt`
  with contact sheet
  `scripts/output/hm-split-distinct-weapons-respawn-4p_hm_borg1_20260820_200641_contact.png`.
  Its `efmp.xbe` SHA256 is
  `69C98B693743CF9F7F1BC2FD4646E503311C15F8BA335A5C51675A8F76DE89A0`.
  The run exited alive with three bots and fixed 2,985,856-byte texture use;
  all six frames were inspected individually and retain complete non-HOM
  geometry plus the four distinct weapon models. It also caught P2 and P3
  respawns: P2 returned from 3 to 124 health with weapon 2 restored, while P3
  exposed a transient spawn weapon 1/requested weapon 3 sample and then
  returned to authoritative weapon 3 and its IMOD view model. P4 remained
  alive on weapon 4 throughout. Guest/game FPS averaged 17.6 with a 13.6
  minimum and screenshot-heavy wall FPS averaged 9.5; staged hardware and a
  screenshot-free comparison remain required for performance.
  Superseding independent official-HUD proof is
  `scripts/output/hm-split-official-hud-final-4p_hm_borg1_20260820_202836.report.txt`
  with contact sheet
  `scripts/output/hm-split-official-hud-final-4p_hm_borg1_20260820_202836_contact.png`.
  It uses 4,182,016-byte `efmp.xbe` SHA256
  `7E901F31D15902B9D22DA459F770AE2C39718D8E7FF4CDE29F53E83EBB724AFB`
  and runtime ID `Aug 20 2026 20:26:39`. The official cgame HUD helper installs
  each local client's authoritative player state, entity weapon, split refdef,
  weapon-select timing, crosshair state, and low-ammo state, then draws that
  player's status/ammo/crosshair/weapon-select/holdable/score/powerup subset.
  The syscall bridge routes each pass only to its slot; all four records report
  `shared=0` and exact 320x240 quadrant bounds. The old compact top diagnostic
  overlay is absent. All four successful frames were inspected sequentially
  and show complete non-HOM `hm_borg1` geometry, phaser/compression/IMOD/
  scavenger first-person models, and visibly different official health/ammo
  values. P3's death/respawn/IMOD-restoration transition also restored its own
  HUD. The focused verifier passes four players, three bots, unique controls
  and attacks, movement, matching external clients, positive draws, distinct
  weapons, independent HUDs, dividers, 30.1 seconds of heartbeat progress,
  6,416,704-byte minimum largest-free memory, and 117,204-byte memory growth.
  Texture use remained fixed at 2,985,856 bytes with 7,499,904 bytes free.
  The process was alive at controlled shutdown. Guest/game FPS averaged 18.0
  with a 12.5 minimum and polling-heavy wall FPS averaged 13.5; this is exact
  binary visual/behavior proof, not hardware performance acceptance.
  Remaining HUD scope is independent zoom state and per-player pickup/reward/
  attacker announcements. Real controller assignment, audible four-listener
  mix quality, and staged hardware FPS/stability remain open.
  The global-overlay policy now draws vote/warmup/center-text/scoreboard once
  at full-screen coordinates while retaining per-player HUD elements in their
  quadrants. Its exact clean-stage build is the 4,190,208-byte `efmp.xbe` with
  runtime ID `Aug 20 2026 22:01:06` and SHA256
  `CBAF7186CE482F9F1F28B3BFC9AD7EE2ABD2151161C9C52AA07986A25E09D960`.
  Visual proof is
  `scripts/output/hm-split-scoreboard-text-cleanproof2-4p_hm_borg1_20260820_221901.report.txt`
  with contact sheet
  `scripts/output/hm-split-scoreboard-text-cleanproof2-4p_hm_borg1_20260820_221901_contact.png`.
  The only captured frame was inspected sequentially and shows one official
  scoreboard spanning four complete live `hm_borg1` worlds. It contains real
  player rows and the fixed labels `NAME`, `SCORE`, `TIME`, `PING`, `Players`,
  and `FREE FOR ALL`; the prior `?` labels are gone. Direct runtime reads prove
  the clean stage selected retail `PAK2.PK3`: `mp_ingametext.dat` loaded at
  3,117 bytes, all 184 entries parsed, validation state reached `0x1ff`, and
  the named text slots contain their expected strings. `run_sp_xemu_smoke.ps1`
  now exposes `-CleanStageData` to apply release-data cleanup without stripping
  direct-test controls, preventing stale loose seed files from overriding PK3
  authority in diagnostic proofs.
  The companion screenshot-free qualification report is
  `scripts/output/hm-split-scoreboard-text-cleanqual-4p_hm_borg1_20260820_222201.report.txt`.
  It passes `verify_holomatch_split_log.py` with four distinct moving/attacking
  control streams, matching P2-P4 external clients/refdefs/snapshots, positive
  P1-P4 draws and P2-P4 view weapons, four independent HUD quadrants, three
  bots, and a live process at controlled shutdown. Six heartbeat samples
  averaged 18.4 guest/game FPS, ranged from 14.0 to 21.1 FPS, and kept minimum
  largest-free memory at 6,472,928 bytes with 163,927 bytes used-memory growth.
  This accepts the global scoreboard placement and English text path in XEMU;
  it does not accept four-player hardware performance or soak stability.
  The natural front-end/XBE handoff is now proven on the fresh release pair:
  4,395,008-byte `default.xbe`, runtime ID `Aug 20 2026 22:38:08`, SHA256
  `4CFAC4E5C25AF183D1B64DE21DA89E8A79AFC73EF331AB1E6AC08B17C638D22B`;
  and 4,190,208-byte `efmp.xbe`, runtime ID `Aug 20 2026 22:39:49`, SHA256
  `B5C3976DC6E03A5A30280B2D4B6BAA408E80E5F6AF55A08896BA3452A8742F9D`.
  The menu smoke path drove three real `A_CURSOR_DOWN` events and one
  `A_ENTER` through `EFFe_HandleMainKey`, then used the same Holomatch accept
  handler as an ordinary front-end selection. Visual evidence is
  `scripts/output/hm-menu-handoff-visualproof_normal_20260820_225606.report.txt`
  with contact sheet
  `scripts/output/hm-menu-handoff-visualproof_normal_20260820_225606_contact.png`.
  All four source captures were inspected sequentially. Each shows four
  distinct live `hm_borg1` views, independent official HUD values, phaser
  models in every quadrant, and visible beam/flare effects. The fourth capture
  still has a localized black/missing-surface-looking far-wall patch in the
  top-right and a distorted-looking bottom-left floor region. This is not the
  former broad P2-P4 HOM failure, but it remains a visual blocker pending a
  targeted angle repro and representative-map coverage.
  Exact-XBE telemetry proof from the same immutable ISO is
  `scripts/output/hm-menu-handoff-xbetelemetry_normal_20260820_230247.report.txt`.
  It passes
  `verify_holomatch_split_log.py --require-launch-source xbe`, proving the
  launched `efmp` personality received the XBE handoff intent and started four
  local players with synthetic P1-P4 controls. All four clients moved and
  attacked with distinct command profiles; P2-P4 had matching external
  refdefs/snapshots and positive view-weapon submissions; all four render
  passes had positive draw counts and exact independent 320x240 HUDs; three
  bots joined; heartbeats and memory advanced; and the process was alive at
  controlled shutdown. Its `map='normal'` field records the harness's normal
  boot context, while the launched Holomatch BSP shown visually is `hm_borg1`.
  The longer reduced-poll run
  `scripts/output/hm-menu-handoff-perfsoak_normal_20260820_230703.report.txt`
  also passes the strict split and audio gates with `source=xbe`, three bots,
  four independent move/attack streams, all four views/HUDs/weapons, positive
  P2-P4 snapshot adds, first-person filter slots 1-3, and 90.5 seconds of
  measured gameplay. It exited alive with 422,978 bytes of sampled used-memory
  growth, 13,151,855-byte minimum free memory, and 6,430,272-byte minimum
  largest-free memory. Audio held backend `0x4`, four compiled/active listeners,
  listener mask `0x0000000e`, 219 registrations, 2,152 starts, 5,123 loops,
  2,478 respatializes, 158 voice starts, and `lipActive=2`. Audible mix quality
  and the separate missing voice-line/face-animation defect remain open.
  Guest/game FPS averaged 23.5, ranged from 16.0 to 35.9, and was below 30 in
  12/14 samples. Polling-heavy wall FPS averaged 15.8. The user's roughly
  30-FPS uninstrumented XEMU overlay observation remains separate context;
  staged retail hardware is still the performance and long-soak authority.
  The 2026-08-21 guest-phaser correction supersedes the earlier high-origin
  P2-P4 beam behavior without cloning or rebasing P1 renderer entities.
  First-person beam submission now follows each active split player's official
  EF `EF_FIRING` state; the split renderer filters only that pane's own
  third-person world beam. Independent official player-state weapon ownership,
  switching, respawn restoration, and pickup behavior remain the authority.
  The exact current release pair is 4,395,008-byte `default.xbe`, runtime ID
  `Aug 21 2026 02:46:53`, SHA256
  `CFC800C2C84B521D85D6C55185DCD4F421FFCCDE048D2B4326569EAFD9157CD3`,
  and 4,194,304-byte `efmp.xbe`, runtime ID `Aug 21 2026 02:47:00`, SHA256
  `8EEC18D94C36246BE85594258D1B3625FA85403341E0D640E92E50A63CCEFF94`.
  Deterministic visual proof is
  `scripts/output/hm-phaser-all-guests-proof_hm_borg1_20260821_024807.report.txt`.
  Each of its four successful captures was inspected sequentially and shows
  P2-P4 beams beginning at the corresponding first-person phaser muzzle and
  converging on that pane's crosshair. Telemetry reached 1,045 first-person
  beam-line submissions for every guest, tagged uniquely as `0x53504201`,
  `0x53504202`, and `0x53504203`, with own-pane world copies filtered. The
  process remained alive and averaged 25.0 guest/game FPS (24.0 minimum); this
  deliberately static proof is not an accepted performance measurement.
  Normal three-bot regression evidence is
  `scripts/output/hm-post-phaser-normal_hm_borg1_20260821_025108.report.txt`.
  All three successful captures were inspected sequentially and retain the
  corrected P2-P4 muzzle origins during ordinary movement, combat, and
  respawns. It remained alive and averaged 21.2 guest/game FPS (17.3 minimum)
  under heavy screenshot/monitor overhead.
  The reduced-poll 94.6-second normal soak is
  `scripts/output/hm-post-phaser-hom-soak_hm_borg1_20260821_025508.report.txt`.
  It remained alive with four local players and three bots. All 21 successful
  captures were inspected sequentially; every pane keeps coherent `hm_borg1`
  world geometry, independent HUDs, and weapons. The dark P3 view at 45.3
  seconds contains stable patterned doorway/interior geometry and lit lower
  supports, and later P3 views traverse the same corridor cleanly, so it is
  not classified as HOM. This bounded sample did not reproduce the earlier
  broad or localized void pattern, although representative maps and wider
  angle coverage remain required for visual signoff. Audio remained active
  with backend `0x4`, listener mask `0x0000000e`, 114 voice starts by the final
  sample, and observed `lipActive=2`. Guest/game FPS averaged 21.8 (17.2
  minimum); screenshot/poll-heavy wall FPS averaged 15.9. Minimum sampled free
  memory was 13,176,703 bytes, minimum largest-free memory was 6,459,360 bytes,
  and sampled used-memory growth was 192,718 bytes. This is visual/audio/HOM
  soak evidence rather than a strict four-control qualification pass: only
  60.3 seconds landed inside the heartbeat measurement window, and P1 remained
  stationary while P2-P4 moved. Real controller assignment, audible
  four-listener mix quality, representative-map coverage, a materially longer
  strict soak, and staged retail hardware FPS/stability remain open.
  A second-map visual pass on the same exact binaries is
  `scripts/output/hm-post-phaser-representative-dn1_hm_dn1_20260821_030408.report.txt`.
  It remained alive, reached three bots, and kept the texture allocator fixed
  at 3,206,016 bytes used. All five successful captures were inspected
  sequentially. Every quadrant retains coherent `hm_dn1` geometry through
  atrium, corridor, stair, close-wall, combat, and effect views; P2-P4 retain
  first-person weapons, and visible guest beams begin at the correct muzzle.
  No HOM or void appears in this bounded sample. Guest/game FPS averaged 17.2
  (15.3 minimum) under heavy visual polling. This is representative-map visual
  evidence only: the report contains no audio samples, P1's virtual command
  remained stationary, and it does not qualify controls, audio, or performance.
  A 2026-08-24 frame-diagnostics comparison isolated the current four-player
  bottleneck in `hm_borg1`. The baseline visible run is
  `scripts/output/hm_4p_frame_profile_visible_hm_borg1_20260823_235412.report.txt`.
  Across 20 valid samples it averaged 75.2 ms per frame: 5.1 ms server and
  70.0 ms client. Renderer front-end construction averaged 14.8 ms, including
  11.9 ms for entity surfaces, while backend draw-surface execution averaged
  47.0 ms; swap averaged 0.1 ms. The run averaged 14.4 guest/game FPS and 12.1
  delivered wall FPS. This makes mip selection and framebuffer presentation
  poor optimization targets; draw submission and state/push-buffer pressure
  dominate. A stronger split-only model-LOD experiment in
  `scripts/output/hm_4p_lod_profile_visible_hm_borg1_20260824_000002.report.txt`
  improved guest/game FPS only from 14.4 to 15.3 and wall FPS from 12.1 to
  12.3, while no new model-LOD breadcrumb fired. The experiment was removed.
  The shipping JA 1 MiB push-buffer and 128 KiB kickoff contract remains
  unchanged. Material-pass reduction, actor/bot reduction, or deeper batching
  are the remaining plausible large levers; none is accepted yet.
  A follow-up diagnostics run added per-shader cycle attribution and retained
  the top five shader records per sample in
  `scripts/output/hm_4p_shader_cost_extract3_hm_borg1_20260824_003112_xblog_profiles.log`.
  `gfx/misc/sunny_flare` produced as many as 396 tiny blended, two-sided
  submissions in one sample, while the main phaser model's aggregate cost was
  dominated by one isolated push-buffer stall. The resulting narrow economy
  change frustum-culls only wholly offscreen sprite/oriented-sprite effects in
  split viewports. It is gated by `r_splitScreenEconomy`,
  `stefx_splitScreen`, and at least two local players, so it applies to co-op
  and Holomatch split-screen but not one-player rendering. The comparison run
  is `scripts/output/hm_4p_effect_frustum_hm_borg1_20260824_003652.report.txt`;
  it completed with three active bots, reduced ranked `sunny_flare` work from
  880 batches across six baseline samples to 152 across four ranked comparison
  samples, and tightened sampled guest FPS from 8.5-22.0 to 12.0-16.6. Mean
  guest FPS was effectively unchanged at 13.4 versus 13.5, so this is retained
  as a correctness-preserving tail-latency reduction rather than evidence that
  four-player performance is accepted.
  The first 2026-08-24 production rebuild broadened that economy gate to every active
  two-, three-, or four-player local split-screen session in co-op and
  Holomatch; one-player rendering is unchanged. The exact release pair is the
  4,423,680-byte `default.xbe`, runtime ID `Aug 24 2026 00:57:24`, SHA256
  `E2030590BAB8C81DFC5420173E2C6DF4DF247187D3D0A7188FFFDBE8F02C7A71`,
  and 4,243,456-byte `efmp.xbe`, runtime ID `Aug 24 2026 00:59:27`, SHA256
  `F00B21D1D5E5B5549814A6753F3F3C2B8F7327AE3C8EDEC5BA0AD7BF0ABBF2B3`.
  The bounded four-player regression report is
  `scripts/output/hm_4p_split_economy_all_splits_hm_borg1_20260824_010434.report.txt`.
  Its three retained frames were inspected sequentially and show four complete
  `hm_borg1` views with independent HUDs and weapons, visible phaser fire, and
  no black or missing surfaces. Telemetry proves four independent moving input
  streams, view/refdef/snapshot handoff for P2-P4, positive draws in every
  viewport, three bots, and a live process at controlled shutdown. Guest/game
  FPS averaged 10.2, ranged from 8.6 to 11.7, and wall FPS averaged 8.2. This
  is clean functional and visual proof, but emphatically not performance
  acceptance.
  Two follow-up split-only A/B experiments were rejected. Disabling decorative
  surface-sprite passes in
  `scripts/output/hm_4p_no_surface_sprites_hm_borg1_20260824_011555.report.txt`
  averaged 9.9 guest FPS, below the 10.2 baseline. One additional global model
  LOD step in
  `scripts/output/hm_4p_extra_model_lod_hm_borg1_20260824_012019.report.txt`
  averaged 10.6 guest FPS from only four valid samples, with substantially
  noisier wall timing. Neither result justifies another fidelity reduction;
  the diagnostic overrides were not promoted to production behavior.
  The current Holomatch player setup now assigns each connected physical pad
  to its own local-player column and cursor. P2-P4 are no longer pre-readied,
  production launch no longer infers virtual controls from player count, and a
  match can start only when every selected player is both connected and ready.
  Disconnected selected slots display `INSERT CONTROLLER`; virtual controls
  remain available only to explicit diagnostics. The exact production pair is
  the 4,423,680-byte `default.xbe`, runtime ID `Aug 24 2026 01:40:36`, SHA256
  `B0C598E013AB1CDC26CAD54C30FCA44CC33A1A0FAE076E45130CD2D79E1DC52C`,
  and 4,243,456-byte `efmp.xbe`, runtime ID `Aug 24 2026 01:40:43`, SHA256
  `A51BE28B347236B48C5A0C5FBA7646BA906095A54876EDA5E748ED23278C822A`.
  `scripts/output/hm_menu_real_pad_assignment_final_normal_20260824_014204.report.txt`
  completed successfully and remained alive through controlled shutdown. Its
  preserved full-resolution frame is
  `scripts/output/hm_menu_real_pad_assignment_final_normal_20260824_014204_screen.png`;
  visual inspection confirms P1's independent options and three correctly
  blocked absent-controller columns. The code/XBE contract verifier also
  passes. This is current one-pad menu proof, not yet physical four-pad input
  qualification.
  The fresh post-handoff production pair is the 4,423,680-byte `default.xbe`,
  runtime ID `Aug 24 2026 02:25:15`, SHA256
  `EDAEFB96FA55C4FB35EA8F0DE6C167097321946D00BBA7E34A60421EC2B54446`,
  and the 4,243,456-byte `efmp.xbe`, runtime ID `Aug 24 2026 02:25:20`,
  SHA256
  `6D539F35841E928D6F611B3DB15BBCEC99C7D21B1B966ECCF1C500458D1E4CA6`.
  Four guest Xbox pads exercised the production player-options screen in
  `scripts/output/hm_menu_four_guest_xinput_normal_20260824_015016.report.txt`.
  Its retained frame shows all four columns fitting the available 640x480
  real estate with independent character, outfit, control style, weapon
  switch, auto-aim, crosshair, vibration, and invert-pitch values.
  The natural menu-to-`efmp.xbe` visual run is
  `scripts/output/hm_four_player_handoff_telemetry_normal_20260824_022749.report.txt`
  with contact sheet
  `scripts/output/hm_four_player_handoff_telemetry_normal_20260824_022749_contact.png`.
  All nine retained frames were inspected sequentially. Every frame has four
  live, coherent `hm_borg1` views with independent HUDs and first-person
  phasers; no pane becomes blank, no pure-black shader surface appears, and
  P3's dark close-wall views progress through combat, death, and respawn. The
  process was alive at controlled shutdown. Guest/game FPS averaged 14.0,
  ranged from 10.0 to 19.5, and remained below 30 in every sample. This is
  visual and handoff proof, not performance acceptance.
  Runtime setup proof from the same immutable package is
  `scripts/output/hm_four_player_handoff_values_normal_20260824_023040.report.txt`.
  The launched `efmp.xbe` reports `source=xbe`, production controls
  (`virtual=0`, `virtualP1=0`), and the exact menu-selected values for all four
  players: Seven/blue with control style 7; Munro/red with style 8;
  Foster/red with style 4; and Tuvok/red with style 5. Their independent
  autoswitch, auto-aim, crosshair, vibration, and invert values also match the
  source menu selections. The focused handoff verifier passes.
  Four-way control and bounded stability proof is
  `scripts/output/hm_exactpair_unique_controls_hm_borg1_20260824_023624.report.txt`.
  This explicit diagnostic launch uses synthetic P1-P4 inputs while retaining
  the same release `efmp.xbe`. It remained alive for 103 seconds and passed the
  strict split verifier over 75.8 seconds of measured gameplay: all four
  clients had distinct command and angle profiles, attacked, and moved
  433-868 world units; P2-P4 had positive snapshot additions, first-person
  filters, view weapons, and independent refdefs; every viewport had positive
  draw counts and a correctly placed HUD; and three bots became active.
  Sampled used-memory growth was 342,534 bytes, minimum free memory was
  12,987,783 bytes, and minimum largest-free memory was 6,487,168 bytes. The
  low-poll run averaged 10.8 guest/game FPS (9.1-13.6), confirming that the
  current split-only economy changes do not make four-player performance
  acceptable in XEMU. Physical multi-pad gameplay, representative-map breadth,
  a materially longer soak, audible mix quality, and staged-hardware
  performance remain open.
  The current source keeps every production economy reduction behind all three
  gates: `r_splitScreenEconomy`, `stefx_splitScreen`, and at least two local
  players. Co-op and Holomatch launch paths enable the gate for any 2P-4P
  session, while one-player SP explicitly disables it. The gated work consists
  of dynamic-light, flare, and Xbox world-effect suppression; reduced curve
  tessellation; and frustum rejection of wholly offscreen
  sprite/oriented-sprite effects. This scope is
  statically verified in the current release pair, but no newer runtime FPS
  claim supersedes the 10.8-FPS four-player evidence above.
  A same-day production A/B series used
  `scripts/output/hm_current_split_economy_4p_hm_borg1_20260824_045124.report.txt`
  as its 120-second four-player baseline: 10.2 guest/game FPS average, 6.8
  minimum, 7.1 p10, and 14.7 maximum, with three bots, synthetic independent
  controls, audio, and a live process at shutdown. Split-only world-view mip
  bias in
  `scripts/output/hm_split_mip1_4p_hm_borg1_20260824_050157.report.txt`
  regressed the average to 8.1 FPS and p10 to 6.5. An inherited JA Ghoul2
  LOD-bias experiment in
  `scripts/output/hm_split_ghoul_lod2_4p_hm_borg1_20260824_053027.report.txt`
  averaged 10.4 FPS, within noise of baseline; its monitor auto-probe also
  introduced a measurement stall. Later entity-category counters confirmed
  that Elite Force made zero Ghoul2 calls, so the experiment was a no-op and
  its code was removed. Both experiments were removed.
  The bounded frame-diagnostics build in
  `scripts/output/hm_split_frame_profile_4p_hm_borg1_20260824_051857.report.txt`
  averaged 10.0 FPS and remained alive. Samples put the server at 4-10 ms and
  the client at 53-125 ms. Renderer front-end entity surfaces consumed 25-48
  ms while world traversal consumed only 0-4 ms; frames contained four views,
  2,079-4,393 draw surfaces, 337-719 batches, and 564-1,115 submissions.
  Present wait was zero. Per-shader records repeatedly ranked
  `gfx/misc/sunny_flare`, `gfx/misc/spark`, the phaser model, multi-pass
  pickups, and player torso work among the expensive entity paths. This makes
  mip bandwidth and present synchronization poor candidates and identifies
  repeated entity processing and small blended submissions as the current
  bottleneck.
  A final split-only trial preserved the first 16 visible sunny-flare and spark
  instances per viewport and rejected only excess instances. Its controlled
  report is
  `scripts/output/hm_split_dense_effect_cap16_4p_hm_borg1_20260824_053937.report.txt`.
  It remained alive and measured an 8.6 guest/game FPS average, 5.6 minimum,
  and 5.9 p10, but no `denseEffectsCulled` breadcrumb fired: no viewport
  exceeded the entity threshold. The lower FPS is therefore run/scene variance,
  while the attempted budget was a measured no-op and was removed. The shader
  profile's high batch counts arise from pass expansion, not more than 16 source
  entities per viewport. No mip bias, Ghoul2 LOD experiment, or visible-effect
  budget is present in the restored production source; all retained economies
  continue to require an active 2P-4P split session and leave one-player
  rendering unchanged.
  The restored shipping pair was rebuilt after removing those trials. It is the
  4,427,776-byte `default.xbe`, runtime ID `Aug 24 2026 05:43:45`, SHA256
  `FA729D0AC6C6204C90D33041F594FA6A79381C0609C5E4919F18F406972C8C2`, and
  the 4,247,552-byte `efmp.xbe`, runtime ID `Aug 24 2026 05:43:51`, SHA256
  `DFE5A578918E1E61404202E85DDC32F1F8291FBD01EC0A6460B98DE973E88AE4`.
  The SP-hosted code/XBE verifier passes and both binaries are free of the
  rejected mip-level and dense-effect-budget contracts. The next performance
  investigation should target repeated per-viewport entity and draw-pass setup
  and small multi-pass effects before accepting any further visible-quality
  reduction.
  Entity-category diagnostics in
  `scripts/output/hm_split_entity_profile2_4p_hm_borg1_20260824_061810.report.txt`
  confirmed zero Ghoul2 calls. MD3/MDR setup accounted for only about 1-2 ms,
  while the enclosing entity phase reported 15-53 ms. The discrepancy led to
  the split-only filters, which were resolving the same split-player cvars and
  Holomatch mode string for every entity in every viewport. Caching those
  invariant values once per viewport preserved the same filters and economy
  gates but removed the repeated lookups. The controlled diagnostics run in
  `scripts/output/hm_split_cvar_cache_4p_hm_borg1_20260824_062857.report.txt`
  remained alive for 80 seconds, averaged 16.7 guest/game FPS (12.5 minimum,
  12.9 p10, 24.8 maximum), and usually reported a 0 ms entity phase. This is a
  retained split-screen CPU optimization with no quality reduction and no 1P
  effect. The production pair was rebuilt after removing the temporary Ghoul2
  diagnostic field. The 4,427,776-byte `default.xbe` has runtime ID
  `Aug 24 2026 06:38:57` and SHA256
  `5775610F87908BE98EB73C915766D19C79FDCCC69AC494FEC5E90646B8AE61FD`;
  the 4,247,552-byte `efmp.xbe` has runtime ID `Aug 24 2026 06:40:33` and
  SHA256
  `255002BBF2DCB530E2571AEAFCA24D60301232FF1A7AED098A0CEB9BCE0D5767`.
  The SP-hosted code/XBE verifier passes with no `codemp` dependency. Exact-pair
  visual proof is
  `scripts/output/hm_split_cvar_cache_prod_visual_4p_hm_borg1_20260824_064358.report.txt`
  with contact sheet
  `scripts/output/hm_split_cvar_cache_prod_visual_4p_hm_borg1_20260824_064358_contact.png`.
  All three retained captures show four live, distinct `hm_borg1` views with
  independent HUDs and first-person weapons, and no blank pane or pure-black
  shader surface. The process remained alive through controlled shutdown.
  Screenshot-free strict proof is
  `scripts/output/hm_split_cvar_cache_prod_perf_4p_hm_borg1_20260824_065029.report.txt`.
  `verify_holomatch_split_log.py` passes with 17 heartbeats over 70.4 seconds,
  four independently moving and attacking controls, positive P2-P4 snapshot
  additions and view weapons, P2-P4 self filters, three active bots, all four
  HUD quadrants, backend `0x4`, and four compiled/active listeners. Sampled
  guest/game FPS averaged 14.9, ranged from 12.7 to 17.1, and remained below
  30 in every sample. Used memory grew by 286,921 bytes; minimum free memory
  was 12,932,420 bytes and minimum largest-free memory was 6,443,584 bytes.
  This accepts the optimization's bounded four-player behavior and stability
  in XEMU, but not four-player performance or long-soak/hardware readiness.
  A direct-co-op harness attempt in
  `scripts/output/coop_split_cvar_cache_prod_2p_normal_20260824_070006.report.txt`
  did not enter gameplay: it stayed disconnected at frame zero for the full
  run even though the ISO contained `ef_sp_direct_coop.txt`. A bounded retry
  experiment confirmed that the stale launcher is called only from
  `CL_Frame`, which is not entered in that disconnected front-end state; the
  experiment was removed. This report is launcher-failure evidence, not a
  co-op renderer pass, so co-op runtime qualification remains open.
  The clean source was rebuilt after removing that experiment. The current
  4,427,776-byte `default.xbe` has runtime ID `Aug 24 2026 07:13:59` and
  SHA256 `BCFE905C67948B5A1F2896E0A8FA78224CBD3DDFC427EAC2B692A570E4610CCC`;
  the 4,247,552-byte `efmp.xbe` has runtime ID `Aug 24 2026 07:15:32` and
  SHA256 `752695D4E531DBFEAF5AC9B04566552AD29C8E5FC9374ABAEBC497E03607286B`.
  The code/XBE contract verifier passes on this clean pair. Its exact-hash
  runtime refresh remains pending; the runtime evidence above belongs to the
  earlier source-equivalent production pair named there.
  The remaining split-economy decision was then cached directly in each
  `viewParms_t` by `R_STEFX_SetSplitViewport`. Curve LOD, flare suppression,
  world-effect suppression, and entity filtering now consume that per-view
  value instead of independently resolving the same cvars in deeper renderer
  paths. This changes no quality setting: the value is enabled only for an
  active 2P, 3P, or 4P split session and remains false for one-player views.
  The resulting production pair is the 4,427,776-byte `default.xbe`, runtime
  ID `Aug 24 2026 07:20:57`, SHA256
  `957C21EA5C4ED40C495442E76C593BD9D4C39AB310581543A57C7C63DA6257ED`,
  and the 4,247,552-byte `efmp.xbe`, runtime ID
  `Aug 24 2026 07:22:41`, SHA256
  `610339D6209EF3A8DE646552EECB91B348EF12D6FB7DBAA281371BB6E8D219A8`.
  The SP-hosted code/XBE verifier passes with no `codemp` dependency.
  Screenshot-free strict proof is
  `scripts/output/hm_split_viewparm_cache_prod_perf_4p_hm_borg1_20260824_072525.report.txt`.
  `verify_holomatch_split_log.py` passes with 17 heartbeats over 70.5 seconds,
  four independently moving and attacking controls, positive P2-P4 snapshot
  additions and view weapons, P2-P4 self filters, three active bots, all four
  HUD quadrants, backend `0x4`, and four compiled/active listeners. Sampled
  guest/game FPS averaged 16.4, ranged from 13.5 to 19.0, and remained below
  30 in every sample. This is 1.5 FPS, or about 10%, above the prior comparable
  14.9 FPS production average, but still does not meet the performance target.
  Used memory grew by 333,565 bytes; minimum free memory was 12,899,067 bytes
  and minimum largest-free memory was 6,429,312 bytes. Exact-binary visual
  proof is
  `scripts/output/hm_split_viewparm_cache_prod_visual_4p_hm_borg1_20260824_072953.report.txt`
  with contact sheet
  `scripts/output/hm_split_viewparm_cache_prod_visual_4p_hm_borg1_20260824_072953_contact.png`.
  Four sequential native captures show four populated, independently moving
  `hm_borg1` views with textured world geometry, HUDs, first-person weapons,
  phaser effects, and no blank pane or pure-black shader failure. Both runs
  remained alive through controlled shutdown. This accepts the optimization's
  bounded four-player XEMU behavior and visual correctness, but not four-player
  performance, long-soak behavior, or hardware readiness.
  A focused two-player top/bottom pass then corrected two defects exposed by
  the first 2P control run. The Holomatch self-model filter now applies to every
  2P-4P session, removing the moving P2 body polygons, and split projection now
  preserves vertical FOV while deriving horizontal FOV from each viewport's
  aspect ratio. That keeps both first-person weapons inside the 640x240 views.
  The final production pair is the 4,427,776-byte `default.xbe`, runtime ID
  `Aug 24 2026 10:25:46`, SHA256
  `E03E7EECEEF3F283CA72866BDD4D6399FF1F7A30526F53657A0216958F733FB8`,
  and the 4,247,552-byte `efmp.xbe`, runtime ID
  `Aug 24 2026 10:28:00`, SHA256
  `F05D819C770F638961369479920DE2F6C981631C9BFE860C0948E2AB0B51922D`.
  The full non-reuse VS2005/XDK 5558 build and
  `verify_build_xbox_contracts.py` pass.
  Exact-binary visual/control proof is
  `scripts/output/hm_split_2p_topbottom_final_hm_borg1_20260824_103340.report.txt`
  with contact sheet
  `scripts/output/hm_split_2p_topbottom_final_hm_borg1_20260824_103340_contact.png`.
  All four native 640x480 captures were inspected at original resolution. They
  show two coherent top/bottom views, both phaser models, correctly attached
  P2 beams, independent movement, textured geometry, and no P2 body-polygon
  obstruction. The process was alive at controlled shutdown. The player-aware
  split verifier passes with both controls moving and attacking, two distinct
  completed render views, positive P2 snapshot/view-weapon proof, P2 self-model
  filtering, three bots, two active audio listeners, and bounded memory.
  Screenshot capture coincided with heavy host contention, so that run's 14.2
  guest-FPS and 11.7 wall-FPS averages are retained only as raw observations,
  not as a 2P performance estimate.
  The preceding render-equivalent production soak is
  `scripts/output/hm_split_2p_topbottom_soak_hm_borg1_20260824_101527.report.txt`.
  It remained alive for the full 170-second harness interval and supplied 31
  heartbeats spanning 100.4 seconds of guest gameplay. Guest FPS averaged
  33.5, with 24.2 minimum, 29.2 p10, and 45.5 maximum. Wall FPS averaged 25.5,
  with 11.9 minimum, while concurrent host workload periodically reduced XEMU
  to about half speed. Used memory grew by 202,465 bytes; minimum free memory
  was 12,979,051 bytes and minimum largest-free memory was 6,501,856 bytes.
  This confirms that concurrent projects materially taint XEMU speed results;
  raw guest and wall measurements must remain separate, and retail hardware is
  still the performance authority. Compared with the retained strict 4P 15.9
  guest-FPS average, 2P has substantial headroom under cleaner host conditions,
  but neither mode has final hardware performance acceptance.
  The bounded in-memory text mirror still repeats internally impossible startup
  HUD records, such as `players=4` paired with a current 640x240 rectangle that
  the 4P layout cannot produce. The verifier therefore uses current viewport
  and HUD rectangles for 2P geometry, while the native frames prove the sole
  horizontal divider. Structured current-state divider telemetry remains open;
  the contradictory bounded string is not accepted as runtime layout authority.

## Four-Player Notification Overlay Proof - 2026-08-24

- The current Release pair is the 4,440,064-byte `default.xbe`, runtime ID
  `Aug 24 2026 19:52:08`, SHA256
  `A1D6C0EE899038C3729DB5ECDA635A7E8B1DFB973A01615F9B35BD43CBB0E5E2`,
  and the 4,259,840-byte `efmp.xbe`, runtime ID
  `Aug 24 2026 19:52:15`, SHA256
  `C8C63C9A2DF81BB9D51818C346FFD2542C8AE0A95A19DC7CA684F9EF201DB19C`.
  `scripts/build_xbox.ps1 -Target spmp -ReuseObjects -SkipAssets` completed
  successfully. The code-only SP-host architecture verifier and Xbox build
  contract verifier pass, with no active `codemp` dependency.
- Exact-pair bounded visual proof is
  `scripts/output/split_overlay_proof_initial_hm_borg1_20260824_195620.report.txt`
  with contact sheet
  `scripts/output/split_overlay_proof_initial_hm_borg1_20260824_195620_contact.png`.
  All three successful 640x480 native captures were inspected sequentially at
  original resolution. Every capture shows four coherent, independent
  `hm_borg1` viewports with their own HUD and weapon plus a zoom mask, distinct
  pickup notice, reward medal, and attacker portrait/name card in each pane.
  The process remained alive through controlled shutdown.
- Structured telemetry confirms all four overlay paths in the same runtime:
  every slot reported flags `0x10f`, representing zoom, pickup, reward,
  attacker, and bounded proof mode. P1-P4 reported distinct FOV values
  42/50/58/66, pickup items 1/2/3/4, reward types 1/2/4/5, and attacker clients
  1/2/3/0. Three bots were active. This proof cvar is diagnostic-only; normal
  physical-controller and non-proof paths retain production behavior.
- Proof-disabled natural-event regression is
  `scripts/output/split_overlay_natural_regression_hm_borg1_20260824_200300.report.txt`.
  It remained alive through controlled shutdown and supplied six usable
  heartbeats spanning 30.2 seconds of guest gameplay. Natural telemetry
  observed pickups and attacker notices for P1, attacker notices for P2-P4,
  and a reward for P3. The strict split verifier passes with four independently
  moving and attacking controls, positive P2-P4 snapshot additions, four
  correct render/HUD quadrants, P2-P4 view weapons and self filters, three
  bots, and bounded memory. The sampled 16.7 average FPS is an observation,
  not performance acceptance.
- Per-player zoom, pickup, reward, and attacker rendering is qualified for the
  current four-player XEMU/LLE path. Physical 2/3/4-pad assignment, audible
  four-listener output, longer soak behavior, and retail-hardware performance
  remain open and block overall Holomatch completion.

## One-Pad Bot-Viewport Hardware Bootstrap - 2026-08-25

- Direct `efmp.xbe` boot with no menu handoff or smoke marker is hardwired to
  `hm_borg1`, four viewports, one human, and three bot-owned viewports. The
  normal menu path also accepts one connected/ready P1; disconnected selected
  panes are bot viewports. Viewport count and human-client count are separate,
  and the same behavior applies to three-player layouts.
- The first bounded XEMU run exposed an invalid startup transition: normal
  `bot_minplayers` logic populated only one bot every ten seconds, so P1 stayed
  full-screen while the partial secondary HUDs leaked over it. That run is a
  recorded failure and is not qualification evidence.
- The corrected path fills the low bot client slots immediately and holds both
  split rendering and split HUD drawing behind an all-secondary-refdefs-ready
  barrier. With one human and four viewports, clients 1, 2, and 3 are therefore
  literally P2, P3, and P4. The same calculation fills clients 1 and 2 for a
  one-human three-player run.
- The corrected release artifact is the 4,263,936-byte `build/release/efmp.xbe`,
  runtime ID `Aug 25 2026 20:54:49`, SHA256
  `8323B6D816770C3DDD49B30A35466D5A2CBB82667555B2BE756A9A9F3CEB2CBB`.
  `scripts/build_xbox.ps1 -Target spmp -SkipAssets -ReuseObjects`, the
  code-only Holomatch architecture/UI check, and `git diff --check` passed.
- Screenshot-free exact-binary evidence is
  `scripts/output/hm_4view_1human_botviews_final_hm_borg1_20260825_210311.report.txt`.
  It remained alive through controlled shutdown and repeatedly reports four
  armed 320x240 render rectangles. Slots 1-3 are active `bot=1` clients with
  external render clients 1/2/3, valid independent HUDs and view weapons, and
  changing positions/view angles throughout the run. Guest/game FPS averaged
  26.9; delivered XEMU wall-FPS averaged 19.3 while the host was under package
  and cleanup disk contention. No frames were retained, by request, so final
  visual acceptance and performance remain retail-hardware gates rather than
  claims from this log-only run.

## One-Pad Bot-Viewport Retail Hardware Result - 2026-08-25

- The returned `build/release/ef_mp_log.txt` is from the exact test artifact
  above (`Aug 25 2026 20:54:49`). It ran for 80.4 seconds and repeatedly proves
  four abutting 320x240 render rectangles. Clients 1-3 are active `bot=1`
  clients mapped to external render clients 1-3, so the hardware result
  confirms that bots literally owned P2-P4 rather than merely appearing as
  opponents in P1's view.
- Excluding the startup heartbeat, 16 five-second samples span 75.36 seconds.
  Their arithmetic mean is 20.02 FPS, the frame/time-weighted rate is 20.00
  FPS, the median is 19.35 FPS, and the range is 15.9-25.6 FPS. Fifteen of 16
  samples are below 25 FPS and eight are below 20 FPS. This is useful live
  hardware proof, but it does not meet the 25-30 FPS shipping target.
- The log does not show an enabled virtual-controller source, synthetic pad,
  smoke P1 view pin, or forced attack. `attackProof=1` is only the passive
  first-observed-attack log flag. P1 was handled by the real-pad split input
  bridge, while the hardwired player profile selected standard controls,
  concentrated auto-aim, and inverted pitch. The tester nevertheless observed
  intermittent P1 control glitches. Their exact cause is not isolated by this
  log, so clean P1 control and removal of the direct one-pad hardwire remain
  required release gates; this result must not qualify production controls.
- The returned hardware heartbeat format contains no usable memory samples.
  Memory stability therefore remains unqualified by this run.

## Retail P1 Input Discontinuity Hardening - 2026-08-25

- The tester clarified that P1 suffered actual view snaps and intermittent
  loss of held-forward input, not merely unfamiliar inversion or auto-aim.
  Source inspection found that `IN_UpdateGamepad` ignored the return value of
  `XInputGetState` and consumed its output unconditionally. A failed poll could
  therefore feed an invalid look vector or a zero movement vector into the
  split-player cache. The returned pre-fix log cannot prove that a poll failed
  because this path previously had no failure telemetry.
- Both retail UC2 and the local UT Xbox reference consume a new controller
  state only when `XInputGetState` returns `ERROR_SUCCESS`. The active input
  path now follows that contract: failed polls preserve the last valid legacy
  and split-player states, and bounded critical records identify poll failure
  and recovery transitions. Direct `efmp.xbe` bootstrap also explicitly clears
  the synthetic-pad, smoke-input, view-pin, combat/map-cycle, overlay, phaser,
  and weapon-proof cvars before launching Holomatch.
- The hardened release candidate is the 4,263,936-byte
  `build/release/efmp.xbe`, runtime ID `Aug 25 2026 21:30:58`, SHA256
  `A12F8AED7EB60354A18DEA9103EE166304E3959DA7DAA36015466559D3FFC682`.
  The SP/MP release build, code-only architecture/UI check, exported failure
  telemetry symbol check, and `git diff --check` passed. Retail control retest
  is still required; this source-level fix is not yet control qualification.

## Retail P1 Command-Ownership Correction - 2026-08-25

- The retail retest of the `Aug 25 2026 21:30:58` poll-hardened candidate still
  had severely erratic P1 controls. Its updated `build/release/ef_mp_log.txt`
  contains no `STEFX_INPUT_POLL_FAILURE` or recovery record, so all observed
  `XInputGetState` calls succeeded and the poll hypothesis is rejected for
  this reproduction.
- Code-path tracing found two simultaneous owners for client 0. The stock Xbox
  input path sends P1 through `IN_CommonUpdate` -> `CL_CreateCmd` -> the local
  loopback client/server channel. `STEFX_HolomatchRunLocalUsercmds` then sent a
  second P1 command directly to `SV_ClientThink`. The two paths used separate
  view-angle histories and different command times, allowing the direct bridge
  to overwrite held movement and alternate the authoritative view after the
  native command. P2-P4 require the direct split bridge; P1 does not.
- The current correction starts direct split commands at client 1. P1 remains
  exclusively on the native loopback path, while its native command is mirrored
  into the existing split proof arrays. Bounded critical
  `STEFX_HM_P1_NATIVE_CMD` records capture movement/button transitions, command
  time continuity, view/delta angles, and angle jumps for the next retail test.
  `STEFX_HM_INPUT_OWNERSHIP` explicitly records the ownership split at runtime.
- The corrected release candidate is the 4,268,032-byte
  `build/release/efmp.xbe`, runtime ID `Aug 25 2026 21:39:40`, SHA256
  `80EC42A57E32B3DFAD51401577A4C26D265F83118F2ABBF774BC7ED22DF4B0A5`.
  `scripts/build_xbox.ps1 -Target spmp -SkipAssets -ReuseObjects`, the code-only
  SP-hosted architecture/UI check, the Xbox build-contract verifier, embedded
  ownership/native-telemetry marker checks, and `git diff --check` pass. Retail
  retest is still required; this candidate is not yet control qualification.

## Retail P1 Fire And False-Zoom Correction - 2026-08-28

- The retail retest of the ownership-corrected `Aug 25 2026 21:39:40`
  candidate confirmed stable analog-stick control. Its returned
  `build/release/ef_mp_log.txt` contains 72 bounded
  `STEFX_HM_P1_NATIVE_CMD` samples, no `snap=1` record, and no input-poll
  failure or recovery. The only final P1 command button masks are `0x0` and
  `0x10`; no attack bit reached the command despite the tester pressing RT.
  This accepts the duplicate-command correction for the reported movement and
  view discontinuities, but it does not qualify P1 buttons.
- The visible P1 zoom mask was a separate deterministic HUD defect. P1's
  normal split projection is 80 degrees horizontal FOV, while the split HUD
  inferred zoom from `fov_x < 89.5`. The returned log records P1 at
  `fov=80/64.3664`, so an unzoomed P1 was always classified as zoomed. The
  current HUD path preserves and uses `cg.zoomed`, `cg.zoomLocked`,
  `cg.zoomTime`, `cg_zoomFov`, and `cg_fov` from the real P1 gameplay state;
  viewport projection no longer creates a false zoom overlay.
- Both the repository and staged `BaseEF/default.cfg` bind `JOY12` to
  `+attack` and `JOY11` to `+altattack`, ruling out a missing packaged bind.
  The native P1 path instead used an analog-button threshold of 64 while the
  working P2-P4 split path used 30. The local Unreal Tournament Xbox reference
  uses `XINPUT_GAMEPAD_MAX_CROSSTALK` for the same edge decision and documents
  that value as 30. The native path now uses that Xbox contract. Bounded
  critical `STEFX_HM_P1_BINDINGS`, `STEFX_HM_P1_ANALOG_SAMPLE`, and
  `STEFX_HM_P1_ANALOG_EDGE` records expose the runtime binding, raw RT/LT
  pressure, threshold decision, client state, and key catcher before the
  existing final-command telemetry.
- The new hardware candidate is the 4,268,032-byte
  `build/release/efmp.xbe`, runtime ID `Aug 28 2026 18:56:44`, SHA256
  `74D182A438D078F6C740B11AA6988B628DF5681543E4E092CCF09742DDEF108F`.
  The release build, code-only SP-hosted architecture/UI check, Xbox build
  contract verifier and self-test, and embedded diagnostic marker checks pass.
  Retail confirmation that P1 spawns unzoomed and RT fires is still required;
  this candidate is not yet button/control qualification.

## Retail P1 SP-Config Restoration - 2026-08-28

- The retail retest of the `Aug 28 2026 18:56:44` threshold/zoom candidate
  rejects insufficient RT pressure as the remaining fire cause. The returned
  `build/release/ef_mp_log.txt` records key 268 reaching raw pressure 255,
  crossing the 30-pressure threshold, and producing both press and release
  edges. P1 remains exclusively owned by the native loopback input path.
- The same runtime record identifies the actual failure: every audited Xbox
  binding, including `JOY12`/RT, is null. The repository, staged, and canonical
  read-only SP `default.cfg` files are byte-identical (SHA256
  `3C5B05EBF8B732E1D1065A2337BE63E421560E42C2B4E9CFB28A843F9AB38493`)
  and contain `bind JOY12 "+attack"`, `bind JOY11 "+altattack"`,
  `ui_thumbStickMode "0"`, and `m_pitch "-0.022"`. The shipped SP config was
  therefore available, but the active filesystem lookup source was not yet
  known. The first attempted correction re-executed `default.cfg` after the
  persisted layer and removed the direct test profile's forced pitch inversion.
- The replacement hardware candidate is the 4,268,032-byte
  `build/release/efmp.xbe`, runtime ID `Aug 28 2026 20:20:12`, SHA256
  `880551F013BB37CFA67A2BB74DF921749B73236C27526B881ED585AA5A256AA1`.
  The release build, code-only SP-hosted architecture/UI check, Xbox build
  contract verifier and self-test, embedded config/input telemetry marker
  checks, and `git diff --check` pass. Retail confirmation that RT fires and
  Y look follows the non-inverted SP convention is still required; this
  candidate is not yet button/control qualification.
- The retest of that exact `Aug 28 2026 20:20:12` candidate failed all action
  buttons: the tester could move but could not jump, crouch, show scores,
  pause, fire, or use. The returned log proves the intended late config pass
  ran, then audits every tested P1 binding as empty with `m_pitch=+0.022`.
  Controller delivery itself is healthy: RT reached raw pressure 255 and all
  sampled analog buttons generated press/release edges, P1 remained native
  loopback-owned, and analog movement continued. This rejects controller
  polling, thresholding, and the split-command bridge as causes of this failure.
- Package inspection identifies the collision: `BaseEF/PAK2.PK3` contains a
  2,323-byte retail PC `default.cfg` beginning with `unbindall`, while the loose
  1,017-byte Xbox `BaseEF/default.cfg` has the canonical JOY action table and
  negative pitch. Re-executing the ambiguous filename therefore selected the
  PC config again. SP-hosted Holomatch now submits the canonical SP Xbox cvars
  and JOY bind commands directly after every file-backed config layer, scoped
  to `STEFX_SP_HOSTED_MP`; package lookup order can no longer clear P1 actions
  or invert vertical look. The active-game binding audit now includes Back and
  Start as well as face buttons and triggers.
- The replacement hardware candidate is the 4,268,032-byte
  `build/release/efmp.xbe`, runtime ID `Aug 28 2026 20:32:28`, SHA256
  `BFAF1B32EF55662F588137BD391F2B4705C07DC2C83C2F8841022D87195C4DB2`.
  The release build, code-only SP-hosted architecture/UI check, Xbox build
  contract verifier and self-test, embedded collision-safe config/input marker
  checks, Python syntax check, and `git diff --check` pass. Retail confirmation
  of fire, alternate fire, jump, crouch, use, score display, pause, weapon
  cycling, zoom release, and non-inverted Y look remains required.
- The same returned log contains eight aligned settled gameplay FPS samples
  after excluding the loading-only record: mean 20.79, median 21.4, and range
  17.1-23.7 FPS. This
  remains below the 25-30 FPS objective. Aligned frame samples put the median
  inclusive client/render boundary at 34.0 ms and the server at 11.0 ms. The
  production D3D8 build intentionally exposes no detailed frontend/backend or
  GPU-wait counters, but the split is consistent with the earlier bounded
  profiled-renderer result in which backend draw-surface submission dominated.
  The current evidence therefore rejects server simulation and memory capacity
  as the primary target; it does not distinguish CPU submission cost from GPU
  consumption inside the client/render boundary.
- The repeated `Memory is low. Using deferred model.` lines are generated
  because Xbox `Hunk_MemoryRemaining()` currently returns the constant zero,
  not because a real allocator query failed. The analyzer now pairs aligned
  profile samples with matching FPS samples instead of including the loading
  sample, and reports the inclusive client boundary when detailed renderer
  counters are unavailable. The next production build also appends real zone
  used/free/largest-block/free-block values to each five-second heartbeat.
  These source-side diagnostic changes do not replace the immutable
  `Aug 28 2026 20:32:28` control-test XBE above.

## Three/Four-Player Retail Detail-Tier Candidate - 2026-08-28

- The local Unreal Tournament Xbox implementation and retail UC2 source agree
  on the safe sharing boundary: one device, shared resources/simulation, and a
  single final presentation are shared, but distinct cameras still perform
  their own visibility and drawing. UC2 responds by centrally lowering actor
  detail and particles for three- and four-player views. It is therefore an
  idea source for quality policy, not evidence that finished draw results can
  be reused between Holomatch cameras.
- Package inspection found 104 `models/players2/*.mdr` assets in `xbox1.pk3`;
  all 104 contain three authored LODs. A bounded experiment tried the next MDR
  level under the common 3P/4P policy, but both production-logging and
  memory-ring diagnostic XEMU builds stopped at the first complete four-view
  frame. Restoring the established Elite Force MDR LOD0 contract immediately
  restored completed-frame progression. The rejected lower-MDR experiment and
  its `STEFX_HM_QUALITY_LOD` proof were removed.
- The retained detail tier applies one additional LOD step only to non-MDR
  world models while `stefxSplitThreePlusEconomy` is active. That policy is
  common to both three and four players. `RF_FIRST_PERSON` and `RF_DEPTHHACK`
  entities remain explicitly exempt, and one-player/two-player behavior is
  unchanged. `STEFX_HM_QUALITY` records `nonMdrLodBias=1`, `mdrLod=0`, and
  `firstPersonLodBias=0`.
- The same returned hardware record shows clients 4, 5, and 6 entering and
  participating after bot-owned clients 1-3 were already supplying P2-P4.
  This follows directly from the direct profile's old `bot_minplayers 7`
  contract: the test was P1 plus six bots, not P1 plus the three requested
  viewport bots. The current production direct-launch correction now sets minimum
  population equal to the 3P/4P viewport count. It therefore keeps bots
  literally in P2-P3 or P2-P4, removes only the three unintended extra test
  opponents, and emits `STEFX_HM_BOT_POPULATION` with `extraBots=0`. The normal
  menu population policy remains unchanged. XEMU then exposed a second issue:
  the every-frame viewport filler could insert several `addbot` commands before
  the command buffer executed the first one. It now counts those pending
  commands as reserved clients. The screenshot-free diagnostic reports
  `hm_exact2bots_3p_pendingfix_framediag_hm_borg1_20260828_234949.report.txt`
  and
  `hm_exact3bots_4p_pendingfix_framediag_hm_borg1_20260828_235212.report.txt`
  prove exactly two and three bot requests/starts/active clients respectively,
  with slot 0 human and every secondary viewport slot bot-owned.

## Binding-Independent Profiled P1 Candidate - 2026-08-28

- The newest returned `build/release/ef_mp_log.txt` is not from the requested
  `Aug 28 2026 20:32:28` embedded-binding artifact. Its first record still
  identifies `Aug 28 2026 20:20:12`, and its active binding audit again shows
  every P1 action empty with positive `m_pitch`. The file is nevertheless
  conclusive about the failure boundary: raw face buttons and both triggers
  reach full pressure and generate edges, while the final P1 command contains
  only movement and the walking bit.
- Source now removes P1 gameplay's dependency on the mutable SP binding table.
  `CL_CreateCmd` remains the sole owner of P1's normal loopback packet, then
  replaces that packet in-place with slot 0 from
  `CL_STEFX_SplitScreen_BuildHolomatchUsercmd`. This is not a second server
  command. It gives P1 the same control-style, inversion, jump, crouch, use,
  attack, alternate attack, weapon, zoom, run-toggle, vibration, and raw-pad
  contracts used for the other local slots. Map/server-time resets reseed the
  pad angle history so the profiled owner cannot inherit a stale camera.
- While active Holomatch owns P1, ordinary SP face-button/trigger/d-pad events
  are consumed after their raw state has been recorded, preventing duplicate
  bind actions. Back and Start stay global and directly issue the official
  Holomatch `+info` scoreboard and `uimenu` pause commands for any assigned
  local pad, even if `default.cfg` has been cleared. The official P1 cgame view
  consumes the continuous profiled FOV and explicitly clears stale zoom state
  at 90 degrees. Player 0's menu autoswitch value is also copied to the
  official cgame cvar, and the server applies the existing per-slot auto-aim
  contract to the single native P1 command.
- Runtime proof markers are `STEFX_HM_P1_PROFILED_CMD`,
  `STEFX_HM_P1_PROFILE_BIND_BYPASS`, `STEFX_HM_GLOBAL_ACTION`, and
  `STEFX_HM_SPLIT_PAD_RESEED`. Profile command telemetry now records bounded
  state transitions independently per slot instead of exhausting one shared
  budget on continuous movement. The split-log verifier's
  `--require-full-p1-actions` gate requires profiled ownership, binding bypass,
  jump, crouch, primary and alternate fire, use, both weapon directions, zoom
  in and release, scoreboard press/release, pause, and one stable physical P1
  port. `--require-full-control-actions` applies the action requirements across
  every commanded local slot for the later four-pad qualification. Python
  syntax, the strengthened verifier self-test, the SP-hosted code-only
  architecture/UI gate, and `git diff --check` pass. This is compiled into the
  current production candidate but has not been qualified on retail yet; the
  existing `Aug 28 2026 20:32:28` hardware-tested XBE does not contain it.

## Retail Startup Freeze Diagnosis - 2026-08-28

- The newest returned hardware log is from runtime ID
  `Aug 28 2026 20:32:28`. The tester reports that controls are responsive, but
  P1 appeared glued to and snapping back to the spawn point during the first
  few seconds, after which all controls behaved normally.
- This interval is deterministic official Elite Force behavior, not a renewed
  controller or command-ownership failure. P1 spawned at server time 1450.
  The log then records valid P1 movement commands while authoritative slot 0
  remains at `(0,-704,-295.875)` through the 6500 sample. Official
  `ClientBegin` sets every non-bot to `PM_FREEZE` with
  `introTime = level.time + TIME_INTRO` when `g_holoIntro` is enabled, and
  `TIME_INTRO` is exactly 5000 ms. `ClientThink_real` and client prediction
  both zero movement until that deadline. The observed release time therefore
  matches 1450 + 5000 = 6450 ms.
- The console 3P/4P launch now queues `g_holoIntro 0`, so human and bot-owned
  viewports become interactive together instead of P1 alone being frozen.
  `STEFX_HM_INTRO_POLICY` is emitted once by the launch path and again after
  game cvar registration, proving the runtime value and the stock 5000 ms
  interval. The correction is compiled and exercised by the current direct
  production XEMU launch; the next retail run must confirm immediate movement
  from the initial spawn.

## Shared Three/Four-Player Particle Tier - 2026-08-28

- The retail UC2 split-screen manager uses one common low-detail policy for
  3P and 4P, with particle reduction factors of 0.4 and 0.2 respectively.
  Holomatch uses the conservative 3P value for both layouts to honor the
  requirement that every 4P economy rule also apply to 3P.
- While the existing three-plus-player economy flag is active, periodic local
  spawners retain two of each five child bursts. Newly allocated spawner
  children are tagged and removed before simulation/render on the skipped
  ticks. Immediate weapon, projectile, beam, and hit effects are not tagged,
  and 1P/2P behavior is unchanged. Runtime policy is exposed by
  `STEFX_HM_QUALITY_PARTICLES` and the renderer quality record.
- This policy, the non-MDR model LOD step, exact P1-plus-viewport-bot
  population, binding-independent P1 controls, and startup-intro removal are
  present in the combined XEMU diagnostic build. Retail hardware remains the
  performance and final-control authority.

## Shared Three/Four-Player Static Gameplay Portraits - 2026-08-28

- Historical detailed 4P profiles contain 69 non-empty render samples with a
  mean of 6.23 views per frame, a normal range of 4-8, and one populated
  scoreboard sample at 14 views. Every one reports `portals=0`, so portal or
  mirror suppression would not address these extra scenes.
- Official Holomatch `CG_DrawHead` renders every animated attacker and
  scoreboard portrait by clearing the scene, adding a head entity, and calling
  `trap_R_RenderScene`; it already falls back to each client's packaged static
  `modelIcon` whenever 3D icons are disabled. Retail UC2 likewise treats
  portraits as material assets rather than miniature gameplay scenes.
- The shared three-plus-player economy now selects that existing 2D fallback
  for gameplay heads and team flags. It preserves the portrait/flag HUD data
  while eliminating an observed mean 2.23 extra render scenes per sampled
  frame and as many as ten during the scoreboard. The rule is common to 3P and
  4P; 1P/2P retain animated 3D icons. Runtime proof is
  `STEFX_HM_QUALITY_PORTRAITS` with `extraRenderScenes=0`, and the combined
  quality records include `gameplayPortraits=2D`.
- This is present in the combined XEMU-tested candidate. Because the current
  performance runs intentionally captured no images, existing visual proof is
  not replaced and final hardware visual qualification remains open.

## Shared Three/Four-Player Vertex-State Cache - 2026-08-28

- The active native D3D8 wrapper called `SetVertexShader` unconditionally from
  `_updateShader` for every indexed submission, even when the requested
  fixed-function FVF matched the current device state. Historical detailed 4P
  samples contain roughly 300-800 submissions per frame, and their dominant
  measured draw-cycle component is state update. The already-qualified
  texture-stage cache skips 65-67% of its cumulative requests, showing that
  redundant state requests are common in this renderer; that percentage is
  supporting evidence only, not a predicted vertex-state skip rate.
- The local Unreal Tournament Xbox renderer provides an independent idea-net
  precedent: its `SetCachedVertexShader` records a validity bit and the current
  shader handle, avoids identical requests, and invalidates around direct
  device-state changes. Holomatch now uses an equivalent tracked wrapper for
  every active native path that sets a vertex shader or FVF, including ordinary
  draws, flare visibility, backbuffer copying/restoration, HDR, light effects,
  and stencil shadow code. A programmable-shader change therefore updates the
  same tracked value, forcing the next fixed-function request to emit; the cache
  cannot silently retain a stale special shader.
- Identical calls are skipped only while the common three-plus-player economy
  flag is active, so the optimization applies equally to 3P and 4P and does not
  alter 1P/2P submission behavior. `STEFX_HW_VERTEX_SHADER_CACHE` logs cumulative
  requests, emitted calls, skipped calls, and skip percentage every 256 frames;
  the hardware analyzer now reports the latest record and verifies that emitted
  plus skipped equals requests. The renderer's combined quality marker records
  `vertexShaderCache=tracked`.
- UC2's retail deferred D3D8 state performs the same comparison for vertex
  streams, including both the source pointer and stride. Holomatch's four active
  native push-buffer paths all use stream zero with a NULL inline source but
  previously repeated `SetStreamSource` even when the stride was unchanged.
  They now share one tracked call; a special 9/14/16-DWORD stride updates the
  record and therefore forces the next ordinary stride to emit. Skips are again
  restricted to the common 3P/4P policy. `STEFX_HW_STREAM_SOURCE_CACHE` exposes
  independent cumulative accounting, and the analyzer reports it alongside the
  texture-stage and vertex-shader/FVF caches. The combined quality record adds
  `streamSourceCache=tracked`.
- The combined XEMU runs exercise these tracked paths. The earlier profile
  extractor searched for the texture-stage marker but omitted the vertex/FVF
  and stream-source marker names; the records were not overwritten by the
  bounded memory ring. The corrected extractor now retains all three. The final
  production sample balances exactly for both new caches: 568,177 requests,
  102,025 emissions, and 466,152 skips (82%). These counters prove effective
  suppression, although no isolated FPS gain is assigned to either wrapper.

## Historical Shader-Cost Alignment - 2026-08-28

- The six retained bounded shader-cost logs contain 58 samples and 288 top-five
  records. Recurrence, which is more reliable here than individual cycle
  outliers, is concentrated in exactly the work the combined candidate targets:
  Munro torso appears in the top five of 40 samples, hazard legs in 27, the
  phaser model in 22, Munro's gameplay head in 15, and the animated PIP head in
  six. Hypo pickups also recur in 28 samples.
- The attempted player-MDR LOD step was rejected after its XEMU stall. Static
  gameplay portraits remove the separate head/PIP scenes; exact viewport
  bot population removes three unintended offscreen actors from the old
  seven-client test; existing three-plus economy logic already trims secondary
  entity shader passes and distant pickup submissions. The new vertex/FVF and
  stream-source caches then target the CPU state work repeated by the remaining
  300-800 submissions per frame.
- The evidence does not justify broader shader collapse. The dormant generic
  vertex-light collapse has known alpha-semantic risk, and individual world
  shaders occasionally dominate a sample without recurring consistently.
  The next trustworthy decision point is therefore a combined runtime A/B of
  the bounded changes above, not another unmeasured visual sacrifice.

## Rejected Player-Skin Cache Experiment - 2026-08-28

- Elite Force Holomatch lower/upper player bodies use MDR and heads use MD3.
  The inherited JA Ghoul2 path is not part of this runtime workload and is not
  an optimization target.
- A bounded MDR player-skin cache was tested once in XEMU. The match created all
  four participants and submitted all four viewports, but completed-frame
  progress stopped at frame 4. A cache-free build stopped at the same boundary,
  proving the cache was not the cause. The lower-MDR LOD experiment was the
  causal change; restoring MDR to LOD0 restored normal frame progression.
- The MDR cache, its fixed memory pool, its runtime telemetry, its analyzer
  support, and its architecture-check requirements were removed in full.
- Two adjacent D3D push-buffer experiments were also removed: a 4 MiB/1 MiB
  split-only allocation and a reduced per-draw reserve. Neither removal alone
  fixed the frame-4 stall, but the stable retail 1 MiB/128 KiB startup policy
  and conservative `vertex + index + 60` reserve are retained because the
  larger/reduced variants had no qualified benefit.
- The adjacent D3D transform/texture audit did not justify another state
  wrapper. `_updateMatrices` already emits projection, view, and texture
  transforms only when the corresponding matrix stack is dirty; `GL_Bind`
  suppresses repeated texture names, and `_updateTextures` only touches dirty
  stages. The remaining direct transform exceptions are flare visibility and
  light effects, both already removed from the split economy workload. This
  leaves the combined runtime A/B—not another speculative state cache—as the
  next decision gate.

## Exact-Population XEMU Frame-Diagnostic Qualification - 2026-08-28

- The corrected 3P run is
  `scripts/output/hm_exact2bots_3p_pendingfix_framediag_hm_borg1_20260828_234949.report.txt`.
  It remained alive, advanced from completed frame 1405 to 1481 between sampled
  heartbeats, rendered three views, and proves `min=3`, `humans=1`, `bots=2`,
  two requests/allocations/begins/active bots, slot 0 `bot=0`, and slots 1-2
  `bot=1`. Guest/game FPS was 74.3 and 75.1 (74.7 mean).
- The corrected 4P run is
  `scripts/output/hm_exact3bots_4p_pendingfix_framediag_hm_borg1_20260828_235212.report.txt`.
  It remained alive, advanced from completed frame 1270 to 1323 between sampled
  heartbeats, rendered four views, and proves `min=4`, `humans=1`, `bots=3`,
  three requests/allocations/begins/active bots, slot 0 `bot=0`, and slots 1-3
  `bot=1`. Guest/game FPS was 52.2 and 52.7 (52.5 mean).
- Both tests used XEMU 0.8.136/LLE, the retail `d3d8.lib`, frame diagnostics,
  and no screenshots. Monitor polling heavily stalls host wall delivery, so the
  guest heartbeat is the valid engine rate. These bounded runs clear the local
  25-30 FPS target and prove progression/topology, but do not replace a longer
  production soak or retail-hardware FPS qualification.

## Exact-Population Production XEMU Smoke - 2026-08-29

- The final screenshot-free production report is
  `scripts/output/hm_exact3bots_4p_production_final_hm_borg1_20260829_000157.report.txt`.
  It booted production `efmp.xbe` build ID `Aug 28 2026 23:59:41`, remained
  alive through the bounded run, and repeatedly proved `min=4`, `humans=1`,
  `bots=3`, with exactly three requests, allocations, begins, and active bots.
  Slot 0 is human-owned and slots 1-3 are bot-owned. All four views render as
  contiguous 320x240 quadrants of the shared 640x480 target, and live bot
  positions, angles, health, weapons, and server time change between samples.
- The available production guest heartbeat was 48.8 FPS with no below-30
  sample. Monitor memory polling heavily slows wall delivery and yielded only
  one new heartbeat record, so this is a smoke result rather than a statistical
  soak. It corroborates the two-sample 52.5 FPS frame-diagnostic result without
  replacing retail-hardware performance authority.
- Runtime identity is fixed in the report: `default.xbe` is 4,448,256 bytes,
  SHA-256 `CC329619D604B0EAD469E8983636081F345ED1F653FA0DDB54DA4B5E93E1F1E0`;
  `efmp.xbe` is 4,272,128 bytes, SHA-256
  `FEE714665755DB186BE23D2E67759E7A8DC7BCB2EC979E587C3BD59FDA3D44A1`.
  Both identify as production and link the retail D3D8 runtime.
- The extracted cumulative native stage-cache records reach sample 1536 with
  6,828,704 requests, 2,404,665 emitted calls, and 4,424,039 skipped calls
  (64%). The original extractor did not search for the immediately following
  vertex/FVF and stream-source markers; this was an extractor omission, not
  memory-ring overwrite. The corrected extraction is qualified below.
- The post-run SP-hosted architecture/UI check passes across 1,038 source files
  with `codempDependency=false`; `git diff --check` reports only the repository's
  existing LF-to-CRLF warnings. Generated-artifact cleanup removed 3.05 GiB of
  transient ISO/runtime material after the smoke. No screenshots were created.

## Elite Force MDR Bone-Palette Cache Qualification - 2026-08-29

- This optimization is in `code/renderer/tr_animation.cpp`, the Elite Force
  `SF_MDR` animation path. It does not use or modify Ghoul2. The existing MDR
  palette cache keyed entries by the address of `backEnd.currentEntity`, but
  each split viewport copies the same refEntity into a different backend scene
  slice, so the pointer changed and defeated cross-viewport reuse.
- Under the shared 3P/4P economy flag only, cache identity now uses the stable
  Elite Force refEntity number together with the MDR header, current frame, old
  frame, interpolation value, and render time. The cache has 16 bounded slots.
  One-player and two-player rendering retain the original pointer identity and
  behavior. `STEFX_HW_MDR_PALETTE_CACHE` reports requests, actual palette
  builds, reuses, bypasses, and accounting every 256 active MDR render times.
- The exact-population 4P frame-diagnostic report is
  `scripts/output/hm_exact3bots_4p_mdrpalette_framediag_hm_borg1_20260829_001711.report.txt`.
  It remained alive with one human and exactly three bots, rendered four active
  views, and averaged 58.1 guest FPS from three samples (55.1-60.8), with no
  below-30 sample. The earlier exact-population diagnostic averaged 52.5 FPS.
  Its extracted cache window reaches 53,336 palette requests, 9,305 builds, and
  44,031 reuses (82%) with balanced accounting and zero bypasses.
- The first exact-population 3P diagnostic averaged 50.3 FPS (48.1-51.9). An
  immediate repeat, recorded in
  `scripts/output/hm_exact2bots_3p_mdrpalette_repeat_framediag_hm_borg1_20260829_002319.report.txt`,
  averaged 64.8 FPS (58.8-70.8), remained alive, rendered three views, and
  retained exactly one human plus two bots. Both runs reported roughly 80%
  palette reuse with balanced accounting. Because the earlier 74.7 FPS baseline
  contains only two adjacent early-game samples, the mixed result qualifies
  common 3P behavior and stability but not a universal 3P performance gain.
- The retained production report is
  `scripts/output/hm_exact3bots_4p_mdrpalette_production_hm_borg1_20260829_003116.report.txt`.
  Production `efmp.xbe` is 4,272,128 bytes, SHA-256
  `E9B514C695F326BE15C7E4451C41196B6E709B74DA9E0FCFBC45089A8DD098C1`,
  build ID `Aug 29 2026 00:29:08`. It remained alive with the exact 1+3
  topology and four active 320x240 quadrants. Its two production heartbeats
  averaged 58.9 guest FPS (57.6-60.2), with no below-30 sample; the preceding
  production smoke exposed one 48.8 FPS heartbeat.
- At production sample 1536, the MDR cache reports 30,414 requests, 5,226
  builds, and 25,188 reuses (82%), zero bypasses, and exact accounting. The
  corrected extraction also reports 64% texture-stage suppression and balanced
  82% suppression for both vertex/FVF and stream-source calls. The architecture
  audit still passes 1,038 active source files with `codempDependency=false`.
  All runs used XEMU 0.8.136/LLE, retail `d3d8.lib`, and no screenshots.
  Monitor polling perturbs host wall delivery, so guest heartbeat FPS is used.
  The longer no-poll production qualification is recorded below; retail Xbox
  remains the final performance authority.

## Sustained No-Poll 3P/4P XEMU Qualification - 2026-08-29

- The final screenshot-free 4P production report is
  `scripts/output/hm_exact3bots_4p_nopoll_finalproduction_hm_borg1_20260829_011311.report.txt`;
  its extracted ring is the adjacent `_xblog_profiles.log`. The 90-second
  harness run provided 11 post-startup samples spanning 55 seconds of live
  gameplay. It remained alive, held `players=4` in every FPS record, and
  advanced from completed frame 182 to frame 2600. This is the exact direct
  test topology: P1 physical and three bots literally owning P2-P4.
- Production `efmp.xbe` is 4,272,128 bytes, SHA-256
  `1002EA5BBCF86632FFDB8312FFAED7CD9891573F3E6243B579E96A556288E95E`,
  build ID `Aug 29 2026 01:11:41`. The run averaged 47.0 guest FPS, with a
  35.5 minimum, 38.6 p10, 55.4 maximum, and zero of 11 samples below 30. No
  screenshots or live gameplay polling were used.
- Zone use rose from 10,152,885 to 10,388,855 bytes, a bounded 235,970-byte
  change. Minimum free memory was 12,872,482 bytes and the smallest sampled
  largest-free block was 6,507,232 bytes. This rules out memory capacity or
  fragmentation as the current limiter. The production boundary samples put
  median client/render time at 17 ms and server time at 0 ms.
- The matching exact P1-plus-two-bot 3P production report is
  `scripts/output/hm_exact2bots_3p_nopoll_production_hm_borg1_20260829_005020.report.txt`.
  Its eight post-startup samples averaged 69.1 FPS with a 58.1 minimum, while
  frames advanced from 295 to 2773. Median client/render time was 13 ms and
  median server time was 0 ms. The same economy and cache policies therefore
  operate correctly in 3P with substantially more headroom than 4P.
- A bounded detailed attribution run is
  `scripts/output/hm_exact3bots_4p_nopoll_framediag_hm_borg1_20260829_010057.report.txt`.
  Instrumentation makes it unsuitable as the production FPS baseline, but its
  settled samples put the native D3D8 backend at 16 ms median versus 4 ms for
  renderer frontend work and 2 ms for the server. Backend draw-surface work was
  13 ms median. Roughly 394 batches and 421 submissions were present per
  sampled frame; `BeginPush` reserve/wait accounted for about 8.33 million of
  9.93 million median measured draw cycles. Large waits moved among otherwise
  unrelated shaders, which identifies push-buffer/GPU backpressure rather than
  one bad material.
- The production counters remain balanced and active: texture-stage requests
  were suppressed by 63%, vertex/FVF and stream-source requests by 82%, and
  the Elite Force MDR bone-palette cache reused 83% of requests. This cache is
  implemented only in the MDR animation path. No Ghoul2 code participates in
  the workload or the retained optimization.
- Current XEMU performance clears the 25-30 FPS objective without another
  quality reduction. A reusable static-world push-buffer or a reduction in
  backend batch count is the next plausible renderer investigation, but it is
  materially riskier than the retained bounded caches and is not justified as
  a blind change. Retail Xbox remains the shipping performance authority.
- `python scripts/check_mp_holomatch_ui.py --code-only` passes after this run:
  1,038 active SP-hosted source files checked, `codempDependency=false`, shared
  `code/` renderer/audio/input ownership, and `default.xbe <-> efmp.xbe` mode
  handoff. Generated-artifact cleanup removed the 3.05 GiB transient test tree;
  one current XEMU ISO remains. No screenshots were created.

## Current Production Menu, Controls, And Performance Refresh - 2026-08-29

- Current production binaries are `default.xbe` build ID
  `Aug 29 2026 03:45:04`, 4,448,256 bytes, SHA-256
  `C345C51273316616577EB35A8DB66E4BDA05495B05889C12A552B7B5144CCEAA`,
  and `efmp.xbe` build ID `Aug 29 2026 03:45:09`, 4,276,224 bytes, SHA-256
  `31CE4ADD97AD6FD3C6F862932342941213D908C5A744E1C233914BC5DE177FF5`.
  Both are production builds linked to the shared retail D3D8 renderer.
- Normal `default.xbe` menu/handoff proof is
  `scripts/output/hm_production_menu_topology_acceptance_normal_20260829_020351.report.txt`.
  Its extracted ring contains only `source=xbe`, `players=4`, `humans=1`,
  `bots=3`, and `virtual=0/0`. Seven settled samples averaged 58.17 FPS with a
  43.5 minimum and no below-30 sample. This closes removal of the production
  direct-boot hardwire; the retained XEMU ISO was repacked back to this normal
  production boot path after diagnostics.
- Current natural runtime visual proof is
  `scripts/output/hm_current_production_natural_onepad_handoff_normal_20260829_012924.report.txt`.
  Its one retained screenshot was manually inspected and shows four contiguous
  320x240 viewports filling the shared 640x480 target, with coherent distinct
  world views, weapons, independent HUDs, and live bots and no black gaps or
  rendering artifacts. The preceding player-menu proof is
  `scripts/output/hm_current_production_onepad_players_menu_normal_20260829_012643.report.txt`.
  No additional screenshots were captured for the control or performance runs.
- Explicit four-lane diagnostic control proof is
  `scripts/output/hm_current_four_lane_control_retry_hm_borg1_20260829_022217.report.txt`.
  The diagnostic-only P1 virtual path replaces its single native loopback
  command in place, so it cannot fight a second command stream. The verifier
  proves unique movement and view angles for P1-P4, attack from all four,
  changing world positions in every slot, exact 320x240 render rectangles,
  positive draw work, per-slot view weapons, and independent HUDs. A strict
  optional snapshot-entity-count check was omitted because slot 4 remained in
  a valid low-entity PVS while its camera, draw work, weapon, and HUD all
  advanced; it is not a rendering or control invariant.
- The current screenshot-free, no-live-poll 4P performance report is
  `scripts/output/hm_current_4p_bot_perf_hm_borg1_20260829_022656.report.txt`,
  with its adjacent `_xblog_profiles.log`. Eight post-startup samples averaged
  51.6 FPS, ranged from 42.1 to 64.7, and had zero samples below 30. Topology is
  balanced at one human plus three bots with `source=direct` and `virtual=0/0`.
  Minimum free zone memory was 12,899,855 bytes and the smallest largest-free
  block was 6,537,376 bytes.
- The matching 3P report is
  `scripts/output/hm_current_3p_bot_perf_hm_borg1_20260829_023014.report.txt`.
  Seven post-startup samples averaged 62.4 FPS, ranged from 43.1 to 75.4, and
  had zero samples below 30. Topology is one human plus two bots with
  `source=direct` and `virtual=0/0`; minimum free zone memory was 12,958,498
  bytes. This confirms that the shared economy and cache policy applies to 3P
  as well as 4P.
- Both current tests clear the 25-30 FPS XEMU objective. The inclusive
  client/render boundary remains dominant at 18.5 ms median in 4P and 14 ms in
  3P, while median server time is 0 ms. Prior bounded detailed attribution
  places that cost in native D3D8 backend submission, especially `BeginPush`
  reserve/wait backpressure. Memory capacity, server work, and inherited
  Ghoul2 code are not the bottleneck. Elite Force MDR is the only retained
  skeletal-animation optimization path; no Ghoul2 fix participates.
- A fresh bounded frame-diagnostic check is recorded in
  `scripts/output/hm_current_4p_renderer_attribution_hm_borg1_20260829_025728.report.txt`.
  Six settled samples averaged 48.6 FPS (40.6 minimum) while rendering four
  views with one human and three bots. Median server time was 2.5 ms, frontend
  time 4 ms, backend time 10 ms, and draw-surface time 12.5 ms. The median
  frame submitted about 438 batches and 465 native D3D8 draws. Of the measured
  draw cycles, `BeginPush` reserve/wait consumed 1,029,782 versus 422,250 for
  state, 586,488 for vertex packing, 111,189 for indexes, and 23,977 for final
  submission. This confirms push-buffer/GPU backpressure on Elite Force's
  native renderer. It does not justify a blind material simplification because
  the blocking wait can be charged to whichever batch reaches pressure.

## Structured HUD State And Current FPS Refresh - 2026-08-29

- `code/win32/xb_log.cpp` now exports the current split HUD player count and
  both divider rectangles as individual words. `code/client/cl_cgame.cpp`
  publishes every rectangle before incrementing the divider serial, and the
  XEMU harness reads those words instead of inventing a four-player record.
  The code-only gate also rejects the former hardcoded strings.
- Screenshot-free live-memory reports prove each supported split layout from
  the current production XBE pair:
  - `scripts/output/hm_structured_hud_2p_hm_borg1_20260829_031941.report.txt`
    reports one human plus one bot, two 640x240 HUD/status rectangles, and
    `players=2 vertical=(320,0 0x0) horizontal=(0,240 640x0)`.
  - `scripts/output/hm_structured_hud_3p_hm_borg1_20260829_032233.report.txt`
    reports one human plus two bots, a 640x240 top rectangle and two 320x240
    lower rectangles, and
    `players=3 vertical=(320,240 0x240) horizontal=(0,240 640x0)`.
  - `scripts/output/hm_structured_hud_4p_hm_borg1_20260829_032628.report.txt`
    reports one human plus three bots, four distinct 320x240 rectangles, and
    `players=4 vertical=(320,0 0x480) horizontal=(0,240 640x0)`.
  Every divider has zero width or zero height, so it paints no black border.
- Final screenshot-free, no-live-poll performance was measured separately to
  avoid monitor stalls. The current 3P report
  `scripts/output/hm_structured_hud_3p_nopoll_perf_hm_borg1_20260829_033240.report.txt`
  contains seven post-startup samples averaging 54.5 FPS, with a 42.6 minimum
  and zero below 30. The final exact-binary 4P report
  `scripts/output/hm_restored128_4p_nopoll_perf_hm_borg1_20260829_034624.report.txt`
  contains six samples averaging 54.9 FPS, with a 44.1 minimum and zero below
  30. The preceding current-source 4P report averaged 49.4 FPS (36.5 minimum),
  also with zero below 30. All use exact viewport-count bot populations and
  production controls.
- A bounded attempt to merge adjacent additive `gfx/misc/purpleparticle`
  batches is recorded in
  `scripts/output/hm_purplemerge_4p_production_hm_borg1_20260829_031223.report.txt`.
  Its proof marker never fired, so the change had no demonstrated effect and
  was removed. No Ghoul2 code was changed or used; Elite Force MDR/native D3D8
  remains the only relevant renderer path.
- UC2's 1 MiB/32 KiB primary push-buffer policy was tested because Microsoft
  XDK notes say smaller kickoffs can reduce render stalls and the detailed EF
  profile places most measured draw time in `BeginPush`. The screenshot-free
  report
  `scripts/output/hm_kickoff32_4p_nopoll_perf_hm_borg1_20260829_034238.report.txt`
  averaged only 40.4 FPS, reached 26.8 minimum, and put 3/10 samples below 30.
  The candidate was removed and the shared shipping-JA 1 MiB/128 KiB policy
  restored; the exact rebuilt baseline then averaged 54.9 FPS. This reference
  idea is rejected for Elite Force rather than retained on retail-hardware
  speculation.

## Native EF World-Reuse Diagnostic And Production Refresh - 2026-08-29

- A broader adjacent vertex-effect merge was tested after the
  `purpleparticle`-only candidate proved inactive. The 4P diagnostic reported
  50 eligible boundaries and 41 merges, but
  `scripts/output/hm_vertexfxmerge_4p_nopoll_perf_hm_borg1_20260829_040304.report.txt`
  averaged only 40.2 FPS. The change was removed. The exact rebuilt 4P baseline
  in
  `scripts/output/hm_vertexfxmerge_reverted_4p_nopoll_perf_hm_borg1_20260829_041218.report.txt`
  averaged 49.4 FPS (40.7 minimum), and the matching 3P baseline in
  `scripts/output/hm_vertexfxmerge_reverted_3p_nopoll_perf_hm_borg1_20260829_041526.report.txt`
  averaged 64.9 FPS (54.7 minimum).
- A 1 MiB/64 KiB midpoint between the local UT work-in-progress renderer's
  512 KiB/64 KiB policy and Elite Force's retained 1 MiB/128 KiB policy was
  tested in
  `scripts/output/hm_kickoff64_4p_nopoll_perf_hm_borg1_20260829_041946.report.txt`.
  It averaged 44.8 FPS, reached 27.4 minimum, and produced one below-30 sample.
  The candidate was removed; production remains at 1 MiB/128 KiB.
- A separate capacity-only test kept the 128 KiB kickoff but enlarged the
  primary push buffer from 1 MiB to 2 MiB. Although detailed frames can reserve
  slightly more than 1 MiB in aggregate, the exact 4P report
  `scripts/output/hm_push2m128_4p_nopoll_hm_borg1_20260829_051219.report.txt`
  regressed to 44.7 FPS average and 37.9 minimum, versus 50.7/44.6 for the
  preceding 1 MiB build. It was removed. Combined with the failed 32 KiB and
  64 KiB kickoff tests, this supports GPU-consumption backpressure rather than
  a simple primary-buffer capacity shortage.
- A read-only frame-diagnostic counter then hashed the exact native payload of
  Elite Force BSP world submissions only: vertex attributes, indices, native
  D3D8 state, textures, shader/pass, cull mode, and primitive type. It does not
  inspect or modify MDR or Ghoul2. In the 4P report
  `scripts/output/hm_worldreuse_diag_4p_perf_hm_borg1_20260829_045218.report.txt`,
  distinct completed samples found only 573-1,158 reusable dwords out of
  102,908-169,546 candidates (0.56-0.68%). The 3P report
  `scripts/output/hm_worldreuse_diag_3p_perf_hm_borg1_20260829_045455.report.txt`
  ranged from zero reusable dwords to 1,353 of 110,023 (1.23%). The table never
  filled, and diagnostic hashing cost 0.26-0.54 ms on sampled frames. This is
  insufficient coverage for a cross-viewport static world cache, so no cache
  or rendering behavior was added. The measurement remains available only in
  frame-diagnostics builds.
- The final normal release pair was rebuilt from the same source with no frame
  diagnostics. `default.xbe` is 4,448,256 bytes, SHA-256
  `2B0A762CCFF2D84B0C13B840FFFF9507E1D004A1950D2FFD880B1680B491AFC0`, build
  ID `Aug 29 2026 05:14:26`; `efmp.xbe` is 4,280,320 bytes, SHA-256
  `8A90416245FE531EEA216333D19F7CC02A644A86D1DBAF7417F692D6D503C114`, build
  ID `Aug 29 2026 05:14:31`. Both identify as `flavor=production`.
- Screenshot-free, no-live-poll XEMU qualification of that exact production
  `efmp.xbe` passed in both shared economy layouts. The exact final 4P report
  `scripts/output/hm_push2m_reverted_final_4p_hm_borg1_20260829_051543.report.txt`
  contains seven post-startup samples averaging 51.9 FPS, with a 40.1 minimum,
  55.8 maximum, and zero below 30. The matching 3P report
  `scripts/output/hm_push2m_reverted_final_3p_retry_hm_borg1_20260829_051841.report.txt`
  contains five samples averaging 71.1 FPS, with a 64.3 minimum, 82.4 maximum,
  and zero below 30. Both remained alive with P1 plus viewport bots. The shared
  ISO was restored to its normal menu boot afterward; the repack record is
  `scripts/output/repack_sp_normal_20260829_052152.log`.

## Actual-Pass Pressure Diagnostic And Particle-Tier Rejection - 2026-08-29

- A diagnostic-only profiler now ranks the actual shader passes that survive
  the existing three-plus-player stage policy. It is implemented in Elite
  Force's native BSP/MDR D3D8 renderer and does not inspect, call, or modify
  Ghoul2. The 4P report
  `scripts/output/hm_shader_pressure_actual_4p_hm_borg1_20260829_053539.report.txt`
  averaged 48.2 FPS while recording 397-472 batches and 427-502 actual passes
  per sampled frame. The 3P report
  `scripts/output/hm_shader_pressure_actual_3p_hm_borg1_20260829_054001.report.txt`
  averaged 61.6 FPS; one sample recorded 135 independent
  `gfx/misc/spark2` batches. Recurring pressure was led by EF
  `gfx/misc/borgflare`, `gfx/misc/spark2`, HUD characters/ammo, and Borg world
  shaders. The profiler is excluded from production builds.
- That evidence justified one bounded shared 3P/4P cgame experiment: keep one
  of five recurring spawner children instead of the established two of five.
  Immediate projectiles, beams, hit flashes, and other gameplay cues remained
  untouched. The first screenshot-free production pair measured 55.7 FPS
  average/38.7 minimum in 4P and 65.1/51.9 in 3P. Because the comparison was
  mixed, a longer 4P replicate was run; it averaged only 44.3 FPS with a 37.7
  minimum. The one-of-five candidate was rejected rather than retained on
  stochastic first-run evidence.
- The common two-of-five (`spawnerParticles=0.4`) policy was restored, the
  production pair rebuilt, and the XBE strings checked before qualification.
  The exact retained `default.xbe` is 4,448,256 bytes, SHA-256
  `03F1B82AA9779F5325A3C4C50E7C8E4AA79069355CE477135E01B543C7943AEC`,
  build ID `Aug 29 2026 05:57:45`. The exact retained `efmp.xbe` is 4,280,320
  bytes, SHA-256
  `8DA527101A6B64C9D5B938CE1ECEE535040D8AAEA6BED1CF97DB97D7AC0AA910`,
  build ID `Aug 29 2026 05:57:51`. Both are `flavor=production`.
- Final screenshot-free, no-live-poll XEMU/LLE proof of that exact retained
  binary passed both required layouts. The 4P report
  `scripts/output/hm_particles40_restored_4p_hm_borg1_20260829_055907.report.txt`
  contains six settled samples averaging 57.2 FPS, with a 46.7 minimum, 64.8
  maximum, and zero below 30. The 3P report
  `scripts/output/hm_particles40_restored_3p_hm_borg1_20260829_060209.report.txt`
  contains five samples averaging 74.2 FPS, with a 69.6 minimum, 78.7 maximum,
  and zero below 30. Both used P1 plus viewport bots and remained alive through
  controlled shutdown. These clear the XEMU target but do not supersede retail
  hardware as the final performance authority.
- The one retained unified ISO was restored to normal menu boot afterward;
  `scripts/output/repack_sp_normal_20260829_060502.log` records the repack. No
  screenshots were created during this diagnostic, A/B, or final proof cycle.

## Exact EF Trail Experiments And Final Production Refresh - 2026-08-29

- The shader-pressure evidence was followed with an exact extension of the
  retained `spark`/`sunny_flare` merge whitelist to `gfx/misc/spark2`. The
  screenshot-free 4P diagnostic
  `scripts/output/hm_spark2_merge_diag_4p_hm_borg1_20260829_061622.report.txt`
  averaged 56.9 FPS, but the merge marker never fired and a sampled frame still
  contained 64 separate `spark2` batches. Source tracing confirmed why:
  Holomatch produces `spark2` through `LE_TRAIL -> RT_LINE`, not a sprite.
- A second exact candidate admitted only `gfx/misc/spark2` with the `RT_LINE`
  primitive, preserving all depth, distortion, and forced-alpha exclusions.
  `scripts/output/hm_spark2_line_merge_diag_4p_hm_borg1_20260829_062857.report.txt`
  averaged 46.4 FPS with a 38.5 minimum, and again recorded no avoided flush.
  The line exception and its diagnostic hook were removed. Neither experiment
  touched EF MDR models or inherited Ghoul2 code.
- A conservative per-viewport cull was then tested for EF line/trail entities.
  Its bounding sphere enclosed both endpoints plus line width, so only a line
  wholly outside the frustum could qualify. Persistent profile-ring proof in
  `scripts/output/hm_line_cull_prod_4p_profiled_hm_borg1_20260829_064651_xblog_profiles.log`
  reported `offscreenLinesCulled=0` for every sampled slot. The three 4P timing
  runs varied from 44.9 to 55.9 FPS, with the profiled run averaging 49.6; the
  inactive cull and its extraction marker were removed rather than retained.
- The exact post-removal production binaries are `default.xbe` build ID
  `Aug 29 2026 06:49:40`, 4,448,256 bytes, SHA-256
  `741C8783245AAC75CA485FB1C7F910A3CDF53769348B3E9D26464E962601AC1C`,
  and `efmp.xbe` build ID `Aug 29 2026 06:49:46`, 4,280,320 bytes, SHA-256
  `832C9F16D2217E844F6987A1A5FC8F532B2A6995B7C3BAFF7F2A1057707DC621`.
  Both identify as `flavor=production`.
- Final screenshot-free qualification used that exact pair. The 4P report
  `scripts/output/hm_retained_prod_4p_post_rejects_hm_borg1_20260829_065100.report.txt`
  contains eight settled samples averaging 45.4 FPS, with a 41.7 minimum and
  zero below 30. The matching 3P report
  `scripts/output/hm_retained_prod_3p_post_rejects_hm_borg1_20260829_065404.report.txt`
  contains five samples averaging 75.0 FPS, with a 64.1 minimum and zero below
  30. Both used P1 plus exactly the bots owning the remaining viewports.
  `scripts/output/repack_sp_normal_20260829_065656.log` records restoration of
  the single unified ISO to normal menu boot. No screenshots were captured.

## EF MDR, Texture-Bind, And Borg-Flare Rejections - 2026-08-29

- A same-binary MDR experiment forced only Elite Force Holomatch lower-body
  meshes to authored LOD1. Runtime validation recorded 18,929 lower-LOD
  selections and zero rejects, proving the branch was active and structurally
  valid. The disabled 4P run averaged 54.4 FPS; enabling it reduced the average
  to 43.1 FPS. The candidate was removed and the established EF MDR LOD0
  contract restored. This test did not involve Ghoul2.
- A second same-binary experiment cached ordinary fixed-function D3D8 texture
  bindings by texture pointer. It suppressed approximately 15% of binds but
  produced no timing gain: stable 4P samples averaged 39.29 FPS disabled and
  39.04 FPS enabled. The cache and its diagnostic toggle were removed. Exact
  post-removal production proof then averaged 52.9 FPS in 4P and 73.8 FPS in
  3P. The corresponding reports are
  `scripts/output/hm_retained_post_texture_reject_4p_hm_borg1_20260829_080048.report.txt`
  and
  `scripts/output/hm_retained_post_texture_reject_3p_hm_borg1_20260829_080347.report.txt`.
- Shader-pressure data next justified one exact extension of the retained
  sprite merge whitelist to Elite Force's one-pass additive
  `gfx/misc/borgflare`. The same candidate `efmp.xbe` averaged 52.1 FPS with
  the toggle disabled and 53.4 FPS enabled, but the bounded avoided-flush
  marker fired zero times. Because the path performed no merge, the 1.3 FPS
  spread is ordinary run variance. The extension, cvar, logging, and smoke
  switch were removed rather than retained.
- All three candidates were confined to Elite Force MDR or native D3D8 state,
  submission, and shader behavior. Inherited Ghoul2 declarations are not an
  Elite Force workload; no Ghoul2 source was changed or used for an FPS claim.

## Current Post-Rejection Production XEMU Qualification - 2026-08-29

- The exact production binaries rebuilt after all three removals are
  `default.xbe` build ID `Aug 29 2026 08:41:47`, 4,448,256 bytes, SHA-256
  `A778AE3B20E30633DD12D220D8601B0E1DE2BAA46E54F723FD1AA7DB24E549FE`,
  and `efmp.xbe` build ID `Aug 29 2026 08:41:54`, 4,280,320 bytes, SHA-256
  `7600FA44CFB5E3BCDA09FC7119A46F559D580A2015B9FFE98965CA41BD2B889B`.
  Both identify as `flavor=production`; the package validator checked 1,038
  active source files and reported `codempDependency=false`.
- The screenshot-free, no-live-poll 4P report
  `scripts/output/hm_retained_post_borgflare_reject_4p_hm_borg1_20260829_085612.report.txt`
  contains ten settled samples averaging 55.0 FPS, with a 46.4 minimum, 68.9
  maximum, and zero samples below 30. Every population record reports
  `players=4 humans=1 bots=3 source=direct virtual=0/0`, so bots literally own
  P2-P4 for this test profile.
- The matching 3P report
  `scripts/output/hm_retained_post_borgflare_reject_3p_hm_borg1_20260829_085945.report.txt`
  contains eight settled samples averaging 69.1 FPS, with a 54.2 minimum, 80.4
  maximum, and zero samples below 30. Its records report
  `players=3 humans=1 bots=2 source=direct virtual=0/0`, so bots own P2-P3.
- Both layouts exercised the same `three-plus` quality policy. At their latest
  samples, texture-stage suppression was 64% in both; vertex/FVF and stream
  source suppression were 83% in 4P and 82% in 3P; EF MDR palette reuse was
  83% in 4P and 79% in 3P. Both runs remained alive through controlled
  shutdown. These results clear the XEMU 25-30 FPS objective; retail Xbox
  remains the final shipping performance authority.
- The single retained unified ISO was restored from the direct 3P test profile
  to normal menu boot. The repack is recorded in
  `scripts/output/repack_sp_normal_20260829_090459.log`. No screenshots were
  created during the A/B, production qualification, or restoration runs.

## EF D3D8 Stream-Layout Rejection And Production Refresh - 2026-08-29

- A same-XBE experiment targeted only Elite Force's native indexed-draw push
  packet. When the fixed-function FVF was unchanged, it suppressed the exact
  17-DWORD stream-layout descriptor and reduced the matching push reservation.
  The disabled report
  `scripts/output/hm_stream_layout_ab_off_4p_hm_borg1_20260829_121831.report.txt`
  averaged 51.0 FPS with a 43.5 minimum. The enabled report
  `scripts/output/hm_stream_layout_ab_on_4p_hm_borg1_20260829_122152.report.txt`
  averaged 46.2 FPS with a 33.5 minimum. Both had zero samples below 30 and
  used candidate `efmp.xbe` SHA-256
  `696722A94B2B2FCD80D222F835402A64F5BB12DC6E628CBDD0ECC5C02264DBCD`.
  Enabled telemetry reported 884,938 skips out of 1,074,822 requests (82%), so
  the branch was active but performance-negative. The complete experiment was
  removed. It did not touch or use Ghoul2.
- The exact post-removal production pair is `default.xbe` build ID
  `Aug 29 2026 12:31:06`, 4,448,256 bytes, SHA-256
  `83444A6551DB6404D0ADE79DA03B43279876036F04D42FFE313FB38FE8324745`,
  and `efmp.xbe` build ID `Aug 29 2026 12:31:12`, 4,280,320 bytes, SHA-256
  `65A791B6B5225DC86EAFB3F60525B65869594AE2BE197D20AB0A23585427C512`.
  Both identify as `flavor=production`; the package validator checked 1,038
  active source files and reported `codempDependency=false`.
- Exact-production 4P proof is recorded in
  `scripts/output/hm_retained_post_stream_layout_reject_4p_hm_borg1_20260829_125138.report.txt`.
  Its ten settled samples averaged 51.2 FPS, with a 39.0 minimum, 59.2 maximum,
  and zero below 30. Every population record reports
  `players=4 humans=1 bots=3 source=direct virtual=0/0`, so bots own P2-P4.
- Matching 3P proof is recorded in
  `scripts/output/hm_retained_post_stream_layout_reject_3p_hm_borg1_20260829_125459.report.txt`.
  Its nine settled samples averaged 63.2 FPS, with a 49.7 minimum, 68.3 maximum,
  and zero below 30. Every record reports
  `players=3 humans=1 bots=2 source=direct virtual=0/0`, so bots own P2-P3.
- Both production runs used the same `three-plus` policy and created zero
  screenshots. Latest state telemetry showed 83% vertex/FVF and stream-source
  suppression in both layouts; EF MDR palette reuse reached 82% in 4P and 80%
  in 3P. Both processes remained alive through controlled shutdown. These XEMU
  results clear the 25-30 FPS objective, while retail Xbox remains the final
  shipping performance authority.
- The one retained unified ISO was restored to normal menu boot afterward;
  `scripts/output/repack_sp_normal_20260829_125907.log` records the repack.

## Current Production Long Soak And Bot-Viewport Structural Proof - 2026-08-29

- The exact post-rejection production XBE pair received a longer
  screenshot-free, no-live-poll 4P XEMU/LLE soak in
  `scripts/output/hm_current_production_4p_10min_soak_hm_borg1_20260829_130225.report.txt`.
  XEMU remained alive through the controlled end of the 600-second host
  window, during which the guest advanced 241.2 seconds. The final profile
  ring excluded its startup sample and retained 47 settled samples averaging
  39.8 FPS, with a 26.2 minimum, 30.1 p10, 57.1 maximum, and 3/47 samples
  below 30. The population stayed exactly `players=4 humans=1 bots=3`.
  Sampled used memory grew by 437,959 bytes; minimum free memory was
  12,785,236 bytes and the minimum largest-free block was 6,416,352 bytes.
  Because this run used neither screenshots nor live gameplay-log polling, it
  is the current XEMU performance authority for the production pair.
- A separate polling-enabled structural run is
  `scripts/output/hm_current_production_4p_5min_strictlog_hm_borg1_20260829_131559.report.txt`.
  The focused verifier now has an explicit `--bot-viewports` mode so it can
  validate P1-human/P2-P4-bot proof without pretending the server-authoritative
  bot cameras are synthetic local clients. The strict run passes with
  `source=direct`, production controls `virtual=0 virtualP1=0`, one human,
  three active bots, four active and moving player states, four unique final
  render views, exact gapless 320x240 render/HUD/status quadrants, zero-width
  divider geometry, matching external-client routing, positive draw deltas for
  P1-P4, positive P2-P4 view weapons, P1-P4 first-person filters, and P2-P4
  self filters. It also passes 31 heartbeats over 135.3 guest seconds,
  12,762,407-byte minimum free memory, 6,410,880-byte minimum largest-free
  block, and 82,611 bytes of used-memory growth. Its live polling depresses
  delivered wall timing, so none of its FPS figures supersede the no-poll
  soak.
- Both runs created zero screenshots and exercised only Elite Force MDR,
  cgame, BSP, and native D3D8 paths. No Ghoul2 code was changed, called, or
  used as evidence. Retail hardware remains the final FPS, controller-routing,
  audible-mix, and long-soak authority.
- The one retained unified ISO was restored to normal menu boot after these
  tests. `scripts/output/repack_sp_normal_20260829_132551.log` records the
  repack and cleanup pass.
