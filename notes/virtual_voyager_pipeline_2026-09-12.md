# Full Virtual Voyager pipeline — active

The updated user goal is the entire Virtual Voyager pipeline and memory-pressure relief. The earlier testing-build gate is historical and narrower; its local map-load passes do not establish full station, progression or transition coverage. Astrometrics and 480p/16:9 are sub-items of this goal.

| Required area | Current evidence and remaining work |
| --- | --- |
| New Game / mode entry | Approved Campaign/Virtual Voyager layout previously tested. Recheck final staged build, controls and mode persistence. |
| All 20 supplied VV maps | Historical packed-data/load/capture evidence in virtual_voyager_map_checks.md. Revalidate affected maps after pipeline changes; audit dependencies and authored routes, not only spawn views. |
| Travel and progression | Historical transporter, holodeck return and turbolift persistence tests. Inventory source scripts/entities for every travel path and station; prove transitions retain required state. |
| Interactive stations | Astrometrics replacement implemented; current all-entry sweep running. Inventory and verify logs, hazard game, holodeck, transporter, turbolift and any other authored menu names. Verify dismiss/return and normal controls. |
| Save/load | Historical nineteen playable-map roundtrips and normal UI loads. Exercise final build after station use and chained transitions, including state preservation; keep authored brig restrictions. |
| 480p / true 16:9 | Active retail renderer proof shows 640x480, pixel aspect 4/3, horizontal FOV 80 -> 96.41834 and unchanged vertical 64.36645. Native capture is 853x480. Menu/model and 4:3 runtime regression checks remain. |
| Memory pressure | Earlier manual run reached zero physical free and ~220KB zone free. SP model pool reduction already frees 2MiB for zone. Measure peak live allocations, fragmentation and physical reserve during populated maps, stations, saves and repeated travel. Demonstrate actual relief, not merely a larger theoretical budget. |
| PS2 RE / transferable improvements | Original ISO preserved. Extracts and all analysis projects stay in build/research/ps2_re. BORG1 PMP table and LZO decoding validated for all 1,541 entries. Executable loader/allocation analysis and matching-map representation comparison remain in progress. Distinguish content cuts from representation/streaming efficiencies. |
| Final staging | Match ISO, package, XBE/EXE/MAP bundle and manual profile; preserve fixed audio and controls. Record checksums and limitations; close owned emulator processes. |

## Current harness findings

The first corrected-renderer run (vv_astro_wide) was live/error-clear, ISO restored and native capture reviewed, but the astrometrics open counter stayed zero. The authored use command appends genericmenu behind the scripted wait tail; that fixture did not test the terminal. The entry sweep now invokes genericmenu directly. A separate authored-trigger check must have no blocking command tail. No terminal pass is inferred from the failed fixture.

The entry fixture uses the normal D-pad/A key handlers, including page/category navigation, and records navigation failures, missing handles and actually drawn entries. Native captures are tied to stable selected-entry counters. Timing and emulator FPS from these diagnostic tests are not performance qualification.
