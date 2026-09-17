# Cooperative accepted as-is — September 11, 2026

The user ended the optimization push and accepted cooperative mode in its
current state. Further co-op work is closed by that decision, not by achieving
the former performance target. Do not resume automatically or defer another
performance pass. Reopen only on request.

Every ten-second gameplay window exceeding 30 native XEMU FPS was NOT achieved.
The circular borg1 fixture clears nearby attackers and ceases to provide a
sustained combat workload. Broader campaign and retail qualification, HUD and
menu limitations remain accepted rather than represented as fixed. Co-op may
remain clearly labeled unfinished and must not hold up SP/Holomatch release work.

No production defaults were promoted, no beta was published and no commit or
push was made. XEMU test output was muted by volume_limit=0; restored to0.65.
The short audio diagnostic captured44622 APU blocks with44622 successful SDL
handoffs, no rejected handoffs and no hook errors. This proves sample delivery
to SDL, not speaker delivery, perceptual clarity or retail sound quality.
Muddy3D VO work remains paused.

Evidence: [co-op investigation](coop_optimization_2026-09-11.md).

## Former checklist — historical, no longer active

## Cooperative

**Current focus, Sep 11:** Improve sustained two-player gameplay to a 30 FPS
average. Diagnose the slowest windows, fix measured bottlenecks, and compare
matched before/after runs. Exclude the borg1 text intro, loading, menus and
fullscreen movies; retain actual gameplay stalls. Preserve current visuals.

Current borg1 opening baseline: **14.1 FPS average** with two moving players;
30 FPS remains unmet. Broader map and combat coverage is still needed.
- [ ] Fix the worst measured bottlenecks and verify sustained 30 FPS averages.
- [ ] Report slow windows, stability, memory and retail verification separately.

Evidence: [co-op optimization](notes/coop_optimization_2026-09-11.md).

**Earlier release priority, Sep 9:** Lowest tier. Single player and four-player
Holomatch are mandatory. Co-op may remain unfinished with a clear “not finished”
disclaimer; its remaining work must not hold up those required modes.

### Finish co-op menus and on-screen displays

- [ ] Complete character selection, difficulty selection, new game, and load game.
- [ ] Finish the co-op HUD layout and show each player's own health and ammo.

### Restore co-op logs and automatic tests

- [ ] Restore persistent SP/co-op file logging on retail hardware and capture
      the reported approximately 15 FPS case.
- [ ] Move the direct-co-op XEMU bootstrap out of disconnected-only `CL_Frame`
      so automated two-player renderer qualification can enter gameplay.

### Test co-op cutscenes and gameplay

- [ ] Verify a natural cinematic-to-gameplay transition.
- [ ] Qualify controls, gameplay, visuals, stability, memory, and FPS on
      XEMU/LLE and retail hardware.
- [ ] Verify unmuted dialogue, ship ambience, and Borg servo/footstep events
      when audio testing resumes. Mouth animation alone does not prove sound.
