# Virtual Voyager follow-up — 2026-09-12

The requested texture, portal and memory fixes are implemented, built and staged for the next manual test. Audio was left alone following the user's confirmation that it is fixed. 480p, anamorphic 16:9, native emulator resolution and no AA remain selected.

## Changes

- Restored 18 selected Voyager door, floor and wall textures from authored sources, with a 256px maximum instead of 128px. The entire changed-texture union adds 325,040 bytes. Mip chains remain; this is a source-detail correction, not removal of distance filtering. Every other package entry is unchanged. Some original artwork is low resolution and remains so.
- Corrected SV_AdjustAreaPortalState for ordinary Elite Force SP: EF clears solid contents before explicitly opening a portal, so the old solid-only guard discarded the opening and later accepted its closing. The EF path now honors that ordering; the other game path retains its guard. Shared-area reference counting and its negative-count error remain intact. This fixes a reproduced API mismatch; the exact entity in the earlier manual crash was not captured.
- Reduced the SP model texture reserve from 4MiB to 2MiB before zone initialization, making 2MiB available to the existing game heap. This also applies to the shared SP/co-op binary. Holomatch keeps its 4MiB reserve. It is not a claim of 2MiB additional physical free RAM. Static texture reserve remains 10MiB.
- Added release-build static texture bounds checks, a single-model-texture capacity check, lifetime skin peak tracking and short, untruncated pool diagnostics.

Correction to the earlier diagnosis: the active retail DDS upload retains the top mip; JkaFakeglSetDDSUploadPicmip is a no-op there. Earlier inspection of an inactive backend incorrectly attributed an additional 128-to-64px loss to r_picmip. The demonstrated reduction was package conversion. r_picmip was not changed.

## Verification

Both SP and SP-hosted MP Xbox builds completed successfully. A host regression extracted the actual portal function: the legacy guard reproduces the unmatched close, while the fixed path passes 10,000 toggle cycles, independent shared-area openings and no-portal entities. The actual static allocator class passes FINAL_BUILD alignment, exact-capacity, overflow and reset checks.

Texture package verification confirms all unmodified entries are byte-identical. Decoded source comparison improves error for 17 of 18 replacements; floor4lit has a small DXT-error increase (13.25 to13.87) while preserving higher source dimensions and matching its base texture. This metric is not a substitute for visual review.

- deck02: 160-second check, authored usable toggles, save/load roundtrip; proof (1,1,1,0), final live/in-game, empty engine error. Two native captures individually reviewed: intact quarters, doors, weapon and HUD. Static peak8,062,336 bytes; skin peak63,360 bytes, zero swaps/fetches. Initial continuous recorder had incorrect map-only symbol translation and is excluded; final smoke dumps and a separately corrected live allocator read are valid.
- deck04 to deck01: 180-second process-local transporter approach and normal menu/authored travel. Destination reached, proof (1,1,0,0) for transition save/read, final live/in-game, empty engine error. This is not a full manual save roundtrip (deck02 supplies that check). Three native captures individually reviewed: intact populated bridge, consoles, crew, weapon and HUD. Zone free minimum6,450,350 bytes, largest block minimum4,492,581 bytes. Final static7,999,872/10,485,760 bytes; lifetime skin peak223,616/2,097,152; swaps/fetches/readBytes/writeBytes all zero. Correctly mapped continuous recorder archived.

Both ISO transactions restored successfully. One transporter stall snapshot was recorded during the run; final liveness and destination passage succeeded, so this is not reported as a persistent freeze. These short tests qualify the changed paths locally; they do not repeat all20 earlier map checks, prove every interaction, or establish retail Xbox performance/stability. Raw640x480 captures are anamorphic; the configured display presents16:9.

## Staged output

- ISO: build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_followup.iso
- Next launch: build/research/virtual_voyager/manual_profile.json, retaining the user's controller binding and dedicated480p/16:9 config. Emulator is closed.
- Symbols: followup_symbols contains matching MAP, EXE and XBE for both binaries. All three are required for correct runtime address translation.
- Release BaseEF/xbox0.pk3 promoted after tests; backup xbox0_before_detail_followup.pk3 preserved.
- Final ISO component hashes rechecked after transaction restoration. Package SHA256: 4708e01ab7d66cdcf467477b97d3500891e527b1f813a81ea0fc89d07e95aae6.
- SP XBE SHA256: 0b0b90e032cfd171988e2353cd19be011db734668839af703f14cd63cd261aad.
- MP XBE SHA256: 62b40e000980cbb34de94a78a97ce8072d6110bbdbc04820ce30a7e19f3b134b.

Evidence: notes/evidence/virtual_voyager_followup_20260912 and notes/evidence/virtual_voyager_20260911/vv_transporter_20260912_125111. No commit, push or publication.

## Manual test supersedes local qualification — September12, 14:20

User encountered an apparent freeze while activating Astrometrics Control on tour/deck08. Process3204 remains alive and main-loop/frame counters advance while serverTime remains49341. Captured Com_Error buffers are empty. Log records ingame menuID=astrometrics routing into the fallback that pauses cl_paused. There is no astrometrics route in the EF Xbox qmenu bridge, although the original PC UI implements it. Treat as an open terminal/menu soft-lock, not a proven engine process crash; do not claim Virtual Voyager interactions complete.

User also reports no widescreen. EEPROM0x90000 and runtime overlay widescreen=1 confirm selection, but EF CG_CalcFOVFromX derives vertical FOV from640x480 pixel dimensions; the retail projection consumes those angles without aspect correction. The EF glconfig bridge also publishes width/height as windowAspect. Correct gameplay aspect handling remains open; earlier statements that16:9 was fully ready were too broad. Preserve480p and investigate gameplay/cinematic/overlay paths separately before changing code.

Initial evidence preserved under notes/evidence/virtual_voyager_followup_20260912/manual_report. Live session recorder remains active at C:/Users/smmel/.codex/tmp/stefx_vv_followup_20260912_141150. No user input injected and no emulator closure performed.
