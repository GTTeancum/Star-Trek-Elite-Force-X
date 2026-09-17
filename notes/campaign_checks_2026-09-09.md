# Campaign checks

User requested a pass through every campaign map on September 9, 2026.

Each map needs visual inspection, loading/stability and memory checks, authored progression and exit checks, and a separate performance run. God mode is allowed for progression diagnostics. Audio work remains paused. Direct map loading establishes only that map's isolated behavior; it does not prove campaign transitions or saved inventory. Retail Xbox remains the performance authority.

**Remaining coverage:** Opening and initial-movement checks reached the later campaign maps. Full routes, exits and retail confirmation remain open; the former instruction to restart from the Stasis maps is stale. See GAME_TODO.md for current priorities.

**Timing correction:** Earlier FPS records extracted by scanning all guest RAM can include stale text from a previous map. Those numbers require remeasurement and do not establish performance acceptance. New probes read the current game's FPS ring directly. Native player-state reads, captures, and the reproduced load/crash findings are separate evidence.

**Forge4 timing rejection, Sep 10:** The clean 180-second run averaged 45.48
guest FPS (minimum window 33.84), but the final player health was -6. It stayed
live without an engine error, yet death-view overlap makes this unsuitable for
gameplay acceptance. Evidence is retained in
`notes/evidence/campaign_20260909/perf_sp_forge4_clean_rejected`.
The measurement fix adds SP death/invalid-snapshot overlap exclusion to the
existing frame-boundary timing latch; it does not change gameplay or ICARUS.

**Death exclusion verified:** Release build succeeded and the 180-second repeat
reproduced death (health -2). The log reports exclusion starting at server time
40495; later roughly 48 FPS death windows have `gameplay=0` and nonzero
`excludedChecks`. Seven retained pre-death windows average 41.11 guest FPS;
17 windows were excluded. The engine ended live and the ISO was restored.
This verifies the measurement correction, not a full route or the 45 FPS target.
Evidence: `notes/evidence/campaign_20260909/forge4_death_exclusion`.

| Map | Current evidence | Still needed |
| --- | --- | --- |
| borg1 | Letterbox candidate checked in XEMU; intro excluded from FPS windows. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| borg2 | 150-second starting-area observation stayed active with no engine error; final counter advanced61. Sampled corridor, lighting, weapon and HUD look intact. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| holodeck | 150-second opening stayed active in the Borg simulation. Sampled corridor, effects, weapon and HUD look intact. Final execution evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy1 | Movie upload fix verified in XEMU: changing movie imagery and credits, natural return to bridge gameplay with bars cleared; CA_ACTIVE, main loop advanced 46, no engine error. Visual/god diagnostic only; full route and retail performance remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy2 | 150-second spawn-area check stayed active with no engine error; final counter advanced 73. Sampled doorway and HUD look intact. Door traversal, full route, exit and performance remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy3 | 150-second stationary entry check stayed active with no engine error; final counter advanced 72. Sampled entry surfaces and HUD look intact. Movement, full route, exit and performance remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy4 | 150-second opening check stayed active with no engine error; final counter advanced 72. Sampled corridor, characters, phaser and HUD look intact. Movement, full route, exit and performance remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy5 | Eight-minute diagnostic completed the briefing naturally and cleared the bars. Camera inactive, player command time 415477, final loop advance 72, no engine error. Full route, exit and performance remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| stasis1 | Texture allocation fix checked; opening encounter completed once; user reports stable to this point. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| stasis2 | Opening and first door/trap trigger checked in XEMU; player and teammates alive; god confirmed. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| stasis3 | 120-second initial walk checked in XEMU; first door crossed; god and 100 player health confirmed. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy6 | 240-second opening check finished in active gameplay; characters alive; god confirmed. Route and exit not tested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy7 | First door and corridor reached with normal movement; active gameplay and god confirmed. One 272 ms guest frame observed; performance acceptance remains open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy8 | Opening released player control; short movement out of the lift checked; active gameplay and god confirmed. Full route remains open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scav1 | Authored cinematic completed and transitioned naturally to scav2; sampled bars span the full width. God reset on transition, as confirmed in memory. | Broader visual review and Xbox confirmation. |
| scav2 | Climbed the entry ladder and reached the room above; height gain and movement confirmed in memory. Player has 100 health and god active; full route remains open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scav3 | 150-second opening and short movement check stayed active with 100 health and god enabled. Sampled corridor textures and HUD look intact; full route remains open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scav3b | 120-second entry-area check stayed in active gameplay with 100 health and god enabled. HUD and sampled room textures look intact; route and exit not tested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scav4 | Fixed the load abort caused by an undefined ambient-set reference in the original map. A 150-second XEMU run reached active gameplay and moved 162 units; god and 100 health confirmed. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scav5 | Opening and room look-around finished in active gameplay with god/100 health. Sampled surfaces and HUD look intact; an open doorway is visible. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| scavboss | Fixed a visibility-diagnostic bitmap overflow that corrupted a renderer pointer. A 210-second XEMU run passed the old freeze, completed the cinematic naturally, and reached first-person gameplay; current guest command time reached 159561 ms with god and 100 health. Full fight and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy9 | Transporter-room scene completed naturally and returned player control with bars cleared in a 150-second XEMU check. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| borg3 | Lossless-storage regression check stayed active for150seconds with intact sampled characters, room and HUD. Final execution and memory evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| borg4 | 150-second spawn-area check stayed active; final main-loop counter advanced 71 frames, with god and 100 health. Sampled terminal, room textures and HUD look intact. Route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| borg5 | 150-second opening observation reached first-person gameplay and stayed active; final counter advanced 59 frames with god/100 health. Sampled platform, machinery and HUD look intact. Route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| borg6 | 150-second spawn-area check stayed active; final main-loop counter advanced 71 frames with god/100 health. Sampled damaged corridor and HUD look intact. Route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy13 | Natural Tuvok communicator portrait/text displayed and cleared correctly; NPCs continued through the door. Final counter advanced 72 frames after a 150-second check; god/100 health confirmed. Route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy14 | 150-second spawn-corridor observation; sampled surfaces and HUD look intact. Final execution and memory readings are retained. Movement through the corridor, route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy15 | Eight-minute XEMU check completed both briefing scenes naturally and returned player control with bars cleared. No engine error; runtime animation bytes match original assets. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn1 | 150-second opening stayed active with no engine error; final counter advanced73, god and100health confirmed. Teammate progressed and sampled room/weapon/HUD look intact. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn2 | 150-second opening stayed active with no engine error; final counter advanced72. Sampled corridor, teammate, weapon and HUD look intact. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn3 | 150-second entry-door observation stayed active with no engine error; final counter advanced65. Sampled door, weapon and HUD look intact. Door traversal, route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn4 | Opening encounter progressed during150seconds; enemies cleared from view and teammate stayed active. Sampled models, corridor and HUD look intact. Final execution evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn5 | 150-second opening stayed active; final counter advanced71, god and100health confirmed. Sampled teammates, entry machinery and HUD look intact. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn6 | Opening cinematic completed naturally and returned to first-person gameplay with bars cleared. Sampled team and corridor look intact. Final execution evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| dn8 | 150-second starting-area observation stayed active. Sampled industrial surfaces, weapon and HUD look intact. Final execution evidence retained; movement, route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy16 | 150-second opening reached gameplay and displayed the mission update. Sampled room and HUD look intact. Final execution evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy17 | Completed the authored conversation and transitioned naturally to forge1 during 450 seconds. Final forge1 camera inactive, player health 100 and god reset on transition; no engine error. Retail confirmation and performance remain open. | Broader visual review, performance and retail confirmation. |
| forge1 | Opening cinematic completed naturally and returned gameplay with bars cleared. Sampled lava-area entrance, team, corridor and weapon look intact. Final execution evidence retained; full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| forge2 | 150-second starting-area observation stayed active with no engine error; final counter advanced72. Sampled entry surfaces, lighting, weapon and HUD look intact. Passage traversal, route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| forge3 | 150-second entry-area observation stayed active. Sampled corridor surfaces, weapon and HUD look intact. Final execution evidence retained; movement, route and exit remain untested. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| forge4 | Opening combat remained live in the instruction-sampling diagnostic; final loop advance 39, no engine error. Profile saved for targeted optimization. Full route, exit and performance acceptance remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| forge5 | 150-second starting-area observation stayed active with no engine error; final counter advanced70. Sampled room, lighting, weapon and HUD look intact. Full route and exit remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| forgeboss | Movement diagnostic cleared the weapon overlap after moving 332 units from spawn. Later weapon looks intact beside a wall. Original spawn overlap, room entrance, full fight, exit and performance remain open. | Route, exit/transition, visuals, memory, performance, retail confirmation. |
| voy20 | 450-second diagnostic: conversation progressed, then black screen with HUD persisted from 180 seconds onward. Game remained live (loop advance 73), CA_ACTIVE, no engine error. Ending/credits not complete; active UI lacks the authored ui_closingcredits command handler. Not performance acceptance. | Route, exit/transition, visuals, memory, performance, retail confirmation. |

Tutorial and optional holodecks/brig are supplementary checks after the main campaign. Map order comes from retail BSP `target_level_change.mapname` links, excluding optional branches.
