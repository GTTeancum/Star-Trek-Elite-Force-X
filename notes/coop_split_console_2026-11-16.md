# Cooperative and split-screen console results — November 17, 2026

All six implementation items are complete. Both Xbox Release pairs were built
before the first console test; no game source or binary changed during console
qualification. Runtime testing used only the retail Xbox in devkit mode at
10.0.0.231:731. No emulator, baseline, before/after or world-mode comparison was
run. These are post-work measurements, not evidence of a measured improvement.

The user ended this pass on November 17, describing the observed improvement
as at most 1-2 FPS, and then authorized commit/push. No further optimization or
runtime testing is planned without a new request. That observation is not a
measured before/after result.

1. Remove production visibility diagnostic overhead.
2. Qualify conservative particle collision filtering in co-op.
3. Cull hidden model surfaces separately in each split viewport.
4. Reuse unchanged world visibility safely.
5. Repair and verify combined diffuse-light submission.
6. Qualify the selected Holomatch world submission on the console.

## Post-work production measurements

| Workload | Map | Retained gameplay | Average FPS | Slow complete window: FPS / duration | Longest frame |
|---|---|---:|---:|---:|---:|
| 2P co-op | borg1 | 155.603 s | 19.53 | 14.95 / 10.035 s | 107 ms |
| 2P co-op | scav2 | 251.309 s | 18.41 | 16.39 / 10.065 s | 95 ms |
| 4P + 4 bots | hm_borg1 | 276.169 s | 21.64 | 17.91 / 10.051 s | 115 ms |
| 4P + 4 bots | hm_voy1 | 271.304 s | 20.89 | 18.09 / 10.059 s | 87 ms |
| 4P + 4 bots | hm_scav1 | 276.138 s | 25.44 | 20.43 / 10.082 s | 81 ms |

Each primary run lasted about five minutes including startup. Loading, menus,
authored openings and movie-overlapping publications are excluded. Borg1's
scrolling-text intro contributes no accepted FPS. Every retained primary run
confirms actual movement and firing by all local players; Holomatch additionally
confirms four actual skill-3 bots and the full eight-player server roster.
Co-op retains only complete windows bracketed by observed living players.
Holomatch includes normal deaths and respawns.

FPS is total rendered-frame count divided by the matching hardware
`Sys_Milliseconds` elapsed time from complete six-field `ft=` publications.
The slow window is the lowest adjacent set of complete publications totaling at
least ten seconds, not an exact per-frame rolling ten-second measurement.
Maximum-frame values retain real gameplay stalls. No screenshots were taken
during timing. The console recorder used read-only XBDM memory queries; it did
not attach a debugger or send host/controller input. Small recording overhead
remains part of these post-work measurements.

`production_summary.json` additionally audits player movement and firing only
inside retained intervals, rather than relying on requested input or movement
at spawn. Borg1 records 73/76 horizontal movement observations over one unit and
491/1,272 firing events for P1/P2; Scav2 records 119/124 movements and 993/2,068
shots. All four Holomatch lanes have sustained observed movement and firing.
Each main run has zero lost FPS publications, zero reported fatal engine errors,
verified title shutdown and verified startup-file cleanup. Each has one
inaccessible-memory observation around two seconds into boot, before gameplay.
This is not a claim that every runtime warning is absent; audio is not qualified.

### Stasis1: limited result, excluded from the primary table

`production_stasis1_04` measured **26.67 FPS** over **45.188 seconds**, with
**24.58 FPS** in a **10.047-second** complete window and a **117 ms** longest
frame. P1 moved and fired, but P2's position remained unchanged throughout the
retained interval despite firing 406 shots. P1 subsequently died. This is not a
sustained moving two-player benchmark or a full five-minute stability pass.
The numerical result is preserved as limited evidence, not promoted into the
qualified table. Earlier Stasis attempts were also excluded after early P1 death.

Co-op timings cover bounded starting-area movement/firing loops. Borg1 included
NPC activity; Scav2 contained ten living NPCs but no hostile NPC firing. These
are not full campaign playthroughs or a constant-combat performance guarantee.
The prior P2 HUD health/ammo mirroring remains outside this optimization pass.
No audio, physical-controller, every-camera, save/transition stress or long-term
memory qualification is claimed.

## 1. Production visibility diagnostics

The active retail world/main renderer now requires `STEFX_PVS_DIAGNOSTICS` for
PVS hashes, popcounts, node/parent scans and surface-bitmap tracking. Ordinary
production and frame-verification builds omit that work while keeping errors,
basic FPS publication and normal visibility/rendering. Both frozen symbol maps
omit `STEFX_PvsProbeSurfaceWord`. `-PvsDiagnostics` remains an explicit research
build option and was not used in either tested pair.

## 2. Conservative particle collision filtering

The actual SP cgame filter retains original collision fallback and actual
co-op guards. Invalid or uncertain cases use the original trace. The first
mismatch records the exact query/collider; expensive per-entity research
counters and cycle sampling are omitted from production. Verification completed
**28,508,654** rejected-collider comparisons across Borg1, Stasis1 and Scav2
with zero mismatches. Production uses mode 1 through the selected build mask.

## 3. Per-viewport hidden model culling

Conservative model PVS culling is available to actual co-op and SP-hosted
Holomatch split renderers. It resets per entity/view, unions old/current frame
bounds across LODs, retains MDR fallback coverage and preserves exceptions for
portal/deformed/remapped/depth-disabled/shadow surfaces and uncertain bounds.
Model culling was active in all accepted verification and production runs.
Individual native captures were reviewed for actors, weapons, world geometry,
lighting and effects in each viewport. These are bounded visual checks, not
pixel equivalence for every possible camera or asset.

## 4. Unchanged world visibility reuse

The cache describes markings actually installed in the shared world, keyed by
world identity, visibility stamp, cluster, full area mask and visibility controls.
Map loads, locked visibility, portal views and worst-leaf diagnostics invalidate
or bypass reuse. Per-view frustum traversal still runs. Verification recomputes
normal visibility and compares every node/leaf bit using bounded storage.
**782** identical-key checks passed with zero differences on Holomatch maps.
The three co-op verification routes offered no identical-key opportunities;
there is no direct exercised co-op reuse count to claim for them.

## 5. Combined diffuse-light submission

Original and combined paths now share the same out-of-line field update;
device-readback structures are initialized consistently. Verification compares
CPU/device words and light-enable states, records first-failure detail, restores
the original state and latches the candidate off on a mismatch. Across the six
accepted verification runs, **1,583,588** comparisons passed without mismatch.
The exact cause of the historical failure is not proven; the current corrected
path passes the stated console checks. Production uses combined mode 1 only in
actual split rendering, with detailed verification disabled.

## 6. Holomatch world submission

The existing resident expanded world path, mode 2, remains selected. The user
explicitly removed A/B comparisons, so modes 0 and 1 were not timed and mode 2
is not claimed to be the fastest hardware choice. Verification completed
**96,354,045** exact world-vertex comparisons with zero mismatches across the
three four-player maps. Voyager reaches the bounded cache capacity and uses the
original draw fallback for some surfaces as designed. Each run starts a fresh
title/map; repeated-transition lifetime stress is outside this qualification.

## Build and evidence identity

Both complete verification and production EXE/MAP/XBE pairs are preserved under
`build/coop_split_20261116/{verification,production}` with SHA-256 manifests.
The complete Release cross-build logs are `build_verification.log` and
`build_production_final.log`; each compiled both targets successfully. Python
syntax, PowerShell parsing, whitespace, runtime address translation, binary
flavor/default identity and diagnostic-exclusion checks passed. No host game or
native test executables were run for this goal. No emulator was launched.

Final production identities:

- `default.xbe`: `8c6d3d973b2ee6c3a375428cb6add8721855173740f1ddc2aa7553a3b0020fb0`
- `efmp.xbe`: `c04e03439b96f5dde7aa8fa059a0427c37bc3e6eff816f9ddff8d785cecaa475`

The runtime default word is `0x2000f`: world mode 2, optimization mask 15.
The mask enables particle filtering, model culling, visibility reuse and
combined lighting. Ordinary build-helper defaults still use mask 0; reproduce
the supplied production pair with the explicit selected mask:

```powershell
scripts/build_xbox.ps1 -Target spmp -SkipAssets -SkipStage -ReuseObjects -SplitPerformanceDefaults 15
```

The clean XDK 5558 is used. The separately authorized Bink library was read for
linking; all project-created artifacts remain in this repository. The active
Holomatch executable uses `code/`, not the deprecated `codemp/` tree.
The preceding issue #1 preferences fixes remain included.

The six accepted correctness runs are `verify_borg1_01`, `verify_stasis1_01`,
`verify_scav2_01`, `verify_hm_borg1_02`, `verify_hm_voy1_01` and
`verify_hm_scav1_01`. Their individual captures were inspected; a separate
Scav2 visual run checked a dark close-wall orientation with three more captures.
An extra capture taken after title shutdown showed the XDK dashboard and is
explicitly excluded. Stasis1 started its authored st_06 movie, but audio and
individual movie frames were not inspected.

Primary timing runs are `production_borg1_02`, `production_scav2_01`,
`production_hm_borg1_01`, `production_hm_voy1_01`, `production_hm_scav1_01`.
Each contains immutable binary/asset identity, raw `flight.jsonl`, `analysis.json`,
endpoint and runtime log evidence. See `production_summary.json` for the
retained-interval movement audit and `verification_summary.json` for comparison
counts. Timing used focused native map packages with original content bytes.

## Excluded attempts and harness limits

- Initial Borg1 verification cleanup raced reboot downtime. Its explicit
  reconstructed endpoint and recovery record verify title unload and marker
  removal. Recorder recovery and RAM log streaming were corrected before the
  next test; no game code or binary changed.
- Initial hm_borg1 verification had four locals but no bots. It was excluded.
  The existing startup hook was configured to enable bots and explicitly add
  four standard skill-3 opponents; all accepted Holomatch runs confirm 8/8/4.
- The private Scav1 package omitted native Scav2 data needed by its authored
  transition. Raw BSP fallback exhausted memory; this setup failure is excluded.
  Scav2 was then tested directly with its original native entries and elite6
  music. The full Scav1-to-Scav2 campaign transition is not qualified.
- `production_borg1_01` disabled P1 replay on an initial client-clock reset and
  is excluded. The identical fresh launch `_02` retained the replay and passed
  actual movement/firing checks. Its startup log was read during the excluded
  text intro; no extra log transfer occurred during retained timing.
- Stasis production `_01`/`_02` had early P1 death with the original input;
  `_03` had the same limit with a shorter circle. `_04` moved forward for four
  seconds before firing, then resumed moving/turning. It is limited as described
  above. Only replay data changed, through the existing parser. No health,
  enemies, script state or production code was overridden. Exact fixture data
  and hashes are preserved. The damage cause is not asserted from these records.

The initial implementation/recorder chronology is preserved in
`build/coop_split_20261116/report_history_before_final.md`; frozen recorder/gate
versions remain under `harness_initial`, `harness_reboot_fix` and
`harness_full_roster`. No game source or binary changed after the first gate.

## Delivery and console restoration

Both final executables and all changed native packages/music were installed
and read back with matching SHA-256 hashes. All nine original title save files
were restored byte-for-byte, the save inventory matches its fresh backup, and
diagnostic startup markers are absent. The title is stopped. Evidence is under
`build/coop_split_20261116/final_console` and `restoration.json`.
The final private console stage is bounded to the tested maps plus Scav1's
entry map; it is not a full campaign installation. It retains original asset
bytes, the complete sound bank, required map music, menu/opening movies and
st_06, while omitting unrelated Stasis movies to fit the available space.
Per-map timing above does not claim a separate timing pass of this combined
stage. The binary-only `StarTrekEliteForceX-console-split-2026-11-17-update.zip` is
for an existing complete tester installation and contains no game assets or
diagnostic startup files. The supplied executables match `build/release` and
the frozen production pair.

The user subsequently authorized committing and pushing the completed source
and reports. No GitHub issue mutation or release/asset upload is requested. The
user's `box art.png` and `Thumbs.db` changes remain untouched and uncommitted.
