# Astra handoff — Star Trek: Elite Force Xbox

Work stopped for transfer on 2026-09-12. The old task's goal is paused, not completed. Owned RE processes were stopped; no owned XEMU remained. Other projects were left alone. No further implementation, RE, builds or tests should run on the old PC. This document transfers context, not a running task.

## Start here

1. Copy the **entire working repository**, including hidden, ignored and untracked files, or use the existing share with exclusive ownership. A Git clone or patch alone is insufficient.
2. Read `notes/handoff_2026-09-12/SETUP.md` and provision the external dependencies.
3. Read this handoff and AGENTS.md. New transfer instructions override historical completion claims.
4. Give the receiving Astra task `notes/handoff_2026-09-12/NEXT_ASTRA_PROMPT.md`, which contains the goal to create.

**PS2 RE and demonstrated memory-footprint reduction come FAR before individual Virtual Voyager systems.** Do not return to station/menu completion until memory pressure is reduced. This is the full pipeline goal, not a one-off astrometrics fix.

## Repository and transfer

- Old root: `Z:\Programming\!archived\Star-Trek-Elite-Force-X`
- UNC: `\\WATSON\Media\Programming\!archived\Star-Trek-Elite-Force-X`
- Branch: `native-d3d8-perf`
- HEAD: `ef7620e166a84642922728c431c8c0912ce4bea1`

The tree contains extensive pre-existing uncommitted work. Preserve everything; no reset/clean or broad rollback. No commit/push was requested or performed for this handoff. No remote task was launched.

`notes/handoff_2026-09-12/` contains git_status.txt, git_head.txt, git_branch.txt, tracked_changes.patch, artifact_manifest.json, observed_state.json, stopped_processes.json and python_versions.json. The binary-capable patch captures tracked changes before handoff documentation, not untracked/ignored files. It is only a recovery supplement. The manifest hashes selected executables/symbols/RE files; large ISOs were recorded by existence/size rather than rehashed. Placeholder strings are not hashes.

Transfer `.git`, `.agents`, `.codex`, source, scripts, tools, notes, SP-Mod-Source-Code-master, third_party_private, build/release, build/research, original PAKs, packages, immutable symbol bundles, EEPROM/HDD/saves and root PS2 ISO. Do not apply Git-ignore exclusions. Check capacity: several candidate ISOs are multiple GB each. Preserve originals until copy verification. External SDK/BIOS/Bink files are listed in SETUP.md and are not all inside the repo.

## User constraints

- No Computer Use, capture app, desktop/window automation or host keyboard/mouse/controller input. Work through source/files/logs/non-interactive commands. Process-local input confined to the targeted emulator/game and XEMU native captures are allowed.
- All PS2 extracts and RE projects stay inside this repo under build/research/ps2_re. Preserve source ISO.
- Audio was confirmed fixed. Do not alter it or resume paused muddy-3D-VO tuning. Keep normal tests audible; don't promote muted diagnostics into manual profiles.
- Co-op accepted as-is; prior co-op/MP optimization tasks closed by user. Do not reopen historical performance goals. Preserve existing work and visual fidelity.
- Target true480p/16:9, native emulator scale, AA off. Preserve user control bindings and canonical default.cfg.
- Work autonomously through harnesses, close owned XEMU after logging, no repeated relaunch loops. Short tests for repeated patterns; longer only for new coverage. Exclude borg1 scrolling-text wall intro from gameplay FPS measurements.
- To-do top stays a numbered one-sentence plain-language shortlist; details below. No EFX IDs or completed/stale items. Refer to task names since numbering changed.

## Goal and order

The copyable goal is in NEXT_ASTRA_PROMPT.md. First complete useful PS2 executable/archive RE and compare corresponding Xbox representations and allocation lifetimes. Implement supported memory savings and demonstrate lower pressure rather than moving budget between pools. Only then finish the entire Virtual Voyager pipeline: locked-in New Game menu, all maps, authored travel/stations/progression, normal save/load and repeated transitions, true480p/16:9 and runtime4:3 regression, no apparent glitches. Stage paired binaries/packages/symbols with precise limits. Emulator checks do not establish retail Xbox qualification.

No PS2-derived memory optimization has yet been implemented and qualified. The full goal is unfinished.

## Memory evidence

Read notes/virtual_voyager_memory_480p_2026-09-12.md and build/research/virtual_voyager/manual_runs/stefx_vv_memory_20260912_115517 (59 records plus final RAM/analysis).

| Measurement | Observed range/peak |
|---|---|
| Physical free | 0–2,220,032 bytes |
| Zone used | 10,788,119–20,230,723 bytes |
| Zone free | 220,571–9,721,433 bytes |
| Largest free block | 211,800–6,818,024 bytes |
| Static texture peak | 7,828 KiB of10 MiB |
| Skin peak in old4-MiB pool | 417 KiB |
| Audio peak | 962,188 bytes |

The audio6-MiB cvar is a ceiling, not a6-MiB allocation. Cutting it does not reclaim5 MiB. SP skin reserve already fell4→2 MiB; this enlarges the zone but does not prove physical free-memory gains. The all27 astrometrics menu probe ended with only389,120 physical bytes free.

Inspect code/qcommon/z_memman_console.cpp. Startup zone budgeting subtracts RETAIL_FINAL_ZONE_RESERVE (7MiB+MODEL_MEM), then accounts for640*480*4 PersistDisplay charge. **ModelMem.AllocateModelSlots() is inside #if 0**: MODEL_MEM arithmetic does not establish live unused model slots. Distinguish reserve budgets, allocation, residency and peaks. Static2.5-MiB temp pool serves saves/Bink: lifetime reuse needs proof; on-demand allocation may fail under save-time pressure. Static texture pool is10 MiB. Audit filesystem/BSP/model transient duplication, ownership, representations and pool lifetime before cutting content.

Distinguish PS2-specific hardware techniques/content reductions from transferable representation/streaming efficiencies. Half the system RAM does not imply a drop-in Xbox saving.

## PS2 RE status

Root ISO: Star Trek - Voyager - Elite Force (USA PS2).iso,1,756,102,656 bytes, preserved. All RE: build/research/ps2_re.

- SLUS_202.27 extracted:3,610,888 bytes; SYSTEM.CNF57 bytes; BORG1.PMP14,650,676 bytes. Hashes in handoff manifest.
- iso_inventory.json lists134 files. Common CD.PMP519,944,924 bytes, SND.PMP298,111,444, BIG.PMP180,979,005 and PMODELS~17.2MB not extracted yet.
- elf_sections.json/exe_strings.json: ELF entry0x100008, no.symtab. TextVA0x100000 size1,772,456; data0x2b3500 size1,628,352; rodata0x440e00 size194,640; bss0x470700 size278,780. Use ELF segment mapping for offsets/addresses.
- BORG1.PMP has count1541 followed by16-byte records [hash,offset,stored size,raw size]. Bounds validated; borg1_lzo_validation.json records all1541 entries successfully decoded with lzokay, exact output lengths and hashes.
- borg1_pmp_probe.json contains superseded preliminary zlib guesses. Do not reuse that diagnosis.
- Decoded magic counts include805 with43 43 93 84 (likely textures, inference),156 IDP3,59 IBI,37 RDM5. No IBSP/RBSP magic found. BSP representation/location unresolved. Most decoded files weren't saved individually; regenerate from preserved PMP.
- filename_hash_probe.json: CRC32/complement, FNV and djb/case variants against PAK names produced zero matches. PMP hash/naming unresolved.

### Ghidra interruption and failed approaches

1. Auto-detected MIPS:LE:64:64-32R6addr was wrong. rejected_auto_r6_decompilation.json has three unreliable functions.
2. Standard MIPS3 MIPS:LE:64:64-32addr:o32 produced nine functions but unsupported PS2 opcodes and false no-return memcpy/memset truncate control flow. limited_mips3_decompilation.json and **current loader_decompilation.json are this limited output**.
3. Correct extension: ghidra-emotionengine-reloaded v2.1.13 for Ghidra11.0.1; processor r5900:LE:32:default, compiler default. Archive/source manifest and expanded extension under tooling; installed isolated extension under tool_home/.ghidra/.ghidra_11.0.1_PUBLIC/Extensions/ghidra-emotionengine-reloaded.
4. Project ghidra/stefx_ps2_r5900 stopped on user request after “Analysis succeeded” and entering ExtractLoader.py. **No completed R5900 JSON or final project save confirmed.** Check project/locks with no live owner; use a fresh repo-local project if needed. Original ELF intact.
5. scripts/run_ghidra_loader.py invokes Java directly because analyzeHeadless.bat/cmd delayed expansion damages the ! in repo path. Update hardcoded Java/Ghidra paths. Retain old loader_decompilation.json separately before rerun.
6. Java Sleigh compilation was extremely slow due tiny XML writes over SMB. Matching Ghidra's native Ghidra/Features/Decompiler/os/win_x86_64/sleigh.exe compiled .slaspec→.sla quickly. Repo-isolated r5900.sla already compiled; ensure it is newer than sources. Local SSD is preferable.

ExtractLoader.py follows roots/direct callees and string xrefs;15 seconds per function. Tentative addresses requiring proper R5900 confirmation:0x1fd050 hunk total,0x1fd088 hunk free,0x1fd208 permanent allocation,0x1fd2d8 temporary allocation;0x2009f0 FSOpen,0x201630 FSRead,0x202510 per-map PMP switch;0x20adc8 decompression wrapper,0x20b398 compressed fetch,0x20b5b8 partial read,0x20b658 PMP read,0x20b868 constructor;0x20f038 world load;0x2834e8 called under hunk pressure (purpose unknown).0x1ffef8 may be normal PK3 hashing rather than PMP hashing.

## Build identities and changes

Paths below start at build/research/virtual_voyager unless specified.

| ISO filename | Matching symbol folder | State |
|---|---|---|
| StarTrekEliteForceX_virtual_voyager_followup.iso | followup_symbols | Last manual profile; user reported astrometrics soft-lock and wrong widescreen |
| StarTrekEliteForceX_virtual_voyager_terminal_wide.iso | terminal_fix/symbols | Earlier active-renderer/menu candidate |
| StarTrekEliteForceX_virtual_voyager_pipeline.iso | terminal_fix/timed_symbols | Latest timed-sweep candidate; not fully qualified/promoted |

manual_profile.json still selects **followup**, not pipeline. Older terminal/terminal_final candidates had an inactive renderer patch; don't use them as widescreen proof.

Latest SP SHA256:0c11186de300d2538632089f36dfd79762549b79f1633b6c42a484581b75c116, diagnostics buildSep12 15:18:29. MP diagnostics15:19:34. build/release/{default,efmp}.xbe match timed_symbols. Latest logs: terminal_fix/build_timed_sweep.log and package_pipeline.log; image has sibling manifest. Both builds passed; full pipeline qualification did not.

Symbols must include matching MAP **and sibling EXE and XBE**. A MAP alone silently resolves link addresses instead of runtime addresses. Never mix candidate binaries and mutable release maps.

Preserved earlier fixes:18 door/floor/wall DDS textures restored up to256px (+325,040 bytes union); EF portal solid-contents guard in code/server/sv_game.cpp; SP skin4→2MiB, MP stays4MiB; allocator bounds checks/lifetime peaks. Audio preserved. Texture detail loss came from package128px cap: active DDS upload-picmip setter is a no-op; earlier inactive-backend diagnosis was wrong. build/release/BaseEF/xbox0.pk3 is restored detail package405,228,308 bytes; xbox0_before_detail_followup.pk3 backup retained.

Latest source edits to inspect, not resume immediately:

- code/ui/ui_ef_astrometrics.inl: bounded32-slot native database (27 actual entries, two groups,max text493 bytes), models register on selection and remain renderer-cached.
- code/ui/ui_ef_qmenu.cpp: routing, D-pad/A fixtures, timed sweep stefx_astro_sweep default off.
- code/win32/win_qgl_dx8.cpp and code/client/cl_cgame.cpp: display/pixel aspect plumbing.
- **Active** code/renderer/retail_xbox/tr_scene_retail.cpp: horizontal FOV/split-screen helper/proof.
- code/renderer/tr_scene.cpp is inactive and restored from scoped backup. Don't repeat edits there.
- scripts/run_sp_perf_probe.py: optional proof reads; scripts/inventory_virtual_voyager_pipeline.py: authored-interface inventory.

terminal_fix/*.before are scoped backups. task_changes.diff is stale and includes inactive-renderer work; don't use for authoritative rollback. Whole-tree patch contains older unrelated work too.

## Exact verification limits

All three latest probes ended live/error-clear with ISO restored and XEMU closed. Exact arrays in observed_state.json. Raw results: build/research/letterbox_perf/<run>*, scripts/output/<run>*, native screenshots, and notes/evidence/virtual_voyager_terminal_20260912.

| Run | Verified | Still unverified |
|---|---|---|
| vv_astro_wide | World640x480,pixel aspect1.33333337,FOV X80→96.41834259,Y64.36644745 unchanged; corridor capture reviewed | Menu opens0 |
| vv_astro_menu_wide | All27 selected/rendered mask0x7ffffff,missing0,31 controller activations,nav failures0; final physical free389,120 | Only entries/02.png stable and individually reviewed; no project/cancel exit proof |
| vv_astro_authored_sweep | Authored use t1384 opens menu; entries0–23 mask0xffffff,activations25,missing/nav failures0; final physical free1,462,272 | Ended before24–26; captures9–23 exist but not individually reviewed; project/cancel0 |

Native853x480 output supports16:9 display. Host aspect_test.cpp/aspect_result.txt passes160 FOVs:4:3 unchanged,wide tan ratio4/3 and isotropic geometry. **Runtime4:3 regression not done.** Isolated terminal_fix/xemu_4x3.toml and eeprom_480p_4x3.bin prepared; don't flip user's main EEPROM.

Proof symbols: _g_SPXBAstrometricsProof[8] = opens,selected mask,missing model mask,project closes,cancel closes,paused after close,catcher after close,last selected. _g_SPXBAstroHarnessProof[4] = rendered mask,controller activations,nav failures,more-text activations. _g_SPXBViewAspectProof[12] = six world then six UI fields:width,height,pixel-aspect float bits,input FOV X,output FOV X,output FOV Y.

## Full pipeline remains after memory gate

Read notes/virtual_voyager_pipeline_2026-09-12.md and build/research/virtual_voyager/pipeline/authored_pipeline_inventory.json:20maps,111 target_level_change,50 trigger_teleport,2 target_teleporter,32 distinct interface IDs. Router covers astrometrics,turbolift,transporter,holodeck. Libraries/PADDs/logs/cargo/hazard game and other interfaces remain incomplete and can hit inherited blank paused UI.

Original behavior in SP-Mod-Source-Code-master/ui/ui_atoms.cpp near920–1020 and ui_turbolift.cpp. PADDs→UI_Padd2Menu;logs→UI_LogMenu;library→UI_AccessingMenu(0);disease/weapon libraries,shooting range,cargo map to PADDs;hazardgame→UI_DischlerGameMenu. Audit script-driven tactical/navigation/engineering interfaces too. Extracted pipeline .dat text is convenient, but original PAKs remain authoritative (newline conversion occurred).

Historical notes/virtual_voyager_map_checks.md, virtual_voyager_testing_2026-09-12.md and virtual_voyager_release_gate.md cover a previous limited testing gate:20 map load/view checks,19 playable-map save roundtrips, normal save/load and some travel. They do **not** establish complete station/route interactions. Old “completed” language doesn't close current goal.

Menu design settled: CAMPAIGN uses old ENGAGE action in a new button directly below TUTORIAL with identical horizontal alignment/style. Old large ENGAGE area becomes VIRTUAL VOYAGER. Preserve difficulty/gender and tutorial behavior.

## Harness safeguards

Existing entry points: scripts/run_sp_perf_probe.py, xemu_flight_recorder.py, ja_xemu_smoke.py, build_virtual_voyager_test_iso.py and terminal_fix wrappers. Audit paths before use; old wrappers can select stale ISO/startup data.

- One probe/ISO transaction at a time; await exit AND restored manifest. Never stack runs.
- No second monitor connection while recorder owns it; it caused invalid snapshots. Read-only memory observations can use recorded setup PID,CR3,ram_host.
- Native capture helper ja_xemu_smoke.xemu_trigger_native_screenshot and stats depend on pinned XEMU binary. Revalidate before changing versions.
- use t1384 appends genericmenu behind existing command tail; long waits postpone it. Timed in-process sweep avoids queue issue, but current run still ended early. Check state before calling it a freeze.
- Some legacy symbol lookups still use mutable release maps; audit readers against complete immutable bundle.
- UI/guest FPS rings may report unrealistic >60; these are not gameplay performance qualification.
- Keep manual profile separate from diagnostics; restore startup transactions and close owned processes on all exits.

## First useful receiver work

Complete correct R5900 loader analysis; resolve PMP lookup/representation and compare BORG1 loading with Xbox. Build a byte-accounted ownership/lifetime table for zone, texture/model pools, renderer buffers and decompression/loading transients. Rank changes by peak saving and fidelity risk, implement a supported change and measure physical free,zone free,largest block and peaks through populated VV maps,repeated travel and saves. Only then resume unfinished stations and final pipeline qualification.
