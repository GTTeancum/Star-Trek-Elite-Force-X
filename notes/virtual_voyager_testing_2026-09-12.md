# Virtual Voyager — ready for testing, September 12, 2026

The approved New Game menu is implemented: CAMPAIGN sits directly below TUTORIAL with matching horizontal alignment, and VIRTUAL / VOYAGER replaces the old ENGAGE panel. Virtual Voyager starts on tour/deck02 through the normal menu.

## Build

- Testing ISO: [StarTrekEliteForceX_virtual_voyager_testing.iso](../build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_testing.iso).
- Hardware testing files: `build/release/default.xbe`, `build/release/efmp.xbe`, and `build/release/BaseEF` now contain the verified pair and asset package. Preserve the complete existing game folder when deploying; these files are not a standalone content distribution.
- ISO SHA-256: `3b09b5214e599856721f4b18c244ba62c22e9bcb301fa69910b429fe01088275`.
- [Component hashes and restored image integrity](../build/research/virtual_voyager/final_delivery_integrity.json).
- The original release asset package was preserved and hash-verified at `build/research/virtual_voyager/pre_virtual_voyager_xbox0.pk3` before promotion.

## Verified requirements

| Requirement | Evidence | Result |
| --- | --- | --- |
| Approved New Game layout and working Virtual Voyager button | [Native menu capture](evidence/virtual_voyager_20260911/menu/new_game.png); [final-image normal boot](evidence/virtual_voyager_20260911/final_image_boot/result.json) | Passed |
| Integrate/optimize all twenty VV BSPs and PAK1–PAK3 assets | [Packed audit](../build/research/virtual_voyager/packed_audit.json); verified 404,903,268-byte xbox0.pk3 payload in final image and release folder | Passed |
| Every VV level loads without apparent visual glitches in checked areas | [Per-map runtime and native visual review](virtual_voyager_map_checks.md); [authoritative evidence audit](../build/research/virtual_voyager/final_map_evidence_audit.json) | Passed |
| Normal save/load support | Nineteen playable maps passed write, overwrite, reload and VV-mode restoration; normal UI slot save/load on deck02 passed separately in overwrite_fixed and ui_load_fixed evidence | Passed |
| Authored travel and persistence | Holodeck entry/return, turbolift hub roundtrip, and transporter deck04-to-deck01; transporter proof(1,1,1,0) | Passed |
| Testing-ready paired image | Actual embedded SP executable booted normally into deck02; live/error-clear, three native captures reviewed; both executable payloads and asset package byte-hash verified after restoring probe files | Passed |

The brig retains its authored cinematic/death sequence and ordinary save restrictions. Runtime verification used XEMU. Full route/station interaction coverage and retail Xbox testing remain for testers; this is not a claim of a complete Virtual Voyager playthrough.

## Fixes included

- Fixed missing/stale NPC model handles after loading a save.
- Fixed save overwrite truncation and propagated save-close failures.
- Fixed model-buffer ownership, repeated bolt-on initialization and fixed-map lookup bounds exposed by map reloads.
- Fixed missing travel-menu labels by using Elite Force's existing bitmap fonts; retained original travel commands, scripts, spawns and timing.
- Packed native Xbox map data and streamed RGB565 lightmaps. The conversion avoids runtime work; it is not a claim of reduced aggregate map-file size.

The transporter investigation ultimately found a fixture sequencing error: the original room-entry trigger clears the selected destination. Selecting after entering the room correctly ran the authored beam-out script and hub transition. No transporter gameplay/script workaround was added. A bounded optional script trace remains disabled unless ICARUS debugging is explicitly enabled.

An earlier artificial main-menu Load invocation during live gameplay stalled and is excluded from the save/load evidence. Normal pause-menu load and cold UI slot load passed. Original PADD/library/tactical station interaction coverage is not established by the travel tests.

Paused audio work and accepted co-op/MP performance work were not reopened. No commit, push or public publication was performed.
