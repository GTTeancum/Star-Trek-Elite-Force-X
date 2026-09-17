# To-do cleanup — September 11, 2026

User explicitly closed the prior shortlist items2(single-player loading/FPS
optimization) and10(the additional Holomatch30FPS low-window pass).
These are closures by user direction, not proof that the previous targets were
achieved. No further performance work is deferred. Co-op was already accepted
as-is. Do not reopen any of these passes without a new user request.

Removed completed/accepted co-op text, historical optimization results and
experiments, duplicated checklists, old build instructions, and the stale
"continue from stasis1" direction from the open tracker. Kept unresolved
campaign routes/ending, physical-controller/hardware checks, release work,
audio issues, Virtual Voyager and requested UI changes. Renumbered the shortlist.

The original tracker is retained below for reference, not as active instructions.

---

# Elite Force X — Open Work Only

Last updated: 2026-09-11 EDT
Status: Co-op accepted as-is; further co-op work stopped. 3D audio remains paused.

Keep this tracker focused on open or explicitly deferred work. Close items when requirements are met or the user explicitly accepts their
current state. Record waived targets separately from verified results. Store implementation history, build
hashes, and test transcripts in linked notes rather than expanding this list.

## What still needs doing

1. **Paused:** Fix muddy character voices on Xbox. Radio voices sound fine.
2. Reduce single-player load times, improve FPS, and cut stuttering.
3. Test single-player menus, settings, saving, loading, and gameplay on Xbox.
4. Check every campaign map for visual problems, crashes, progression, and performance, continuing from stasis1. Verify the rendering fixes on Xbox.
5. Check Holomatch controls with one, two, three, and four Xbox controllers.
6. Test and package both the campaign and Holomatch for release.
7. Check that sound from all four split-screen players mixes correctly when audio work resumes.
8. Implement the Virtual Voyager content (PAK1-PAK3) into the New Game menu. Load Game should be able to load this content.
9. Darken the game behind the objectives, pause screens and scoreboard so their text is easier to read. The first two should darken the whole screen, but the scoreboard should stay within its respective borders.
10. **Paused:** Keep Holomatch's slowest ten-second averages above 30 FPS with four moving players and four bots. This target has not been met.
11. Align loading wheel with static, baked in load wheel in load screens. Ensure that the load screen comes up early enough that the game does not look frozen.

## Single player

### Muddy character voices — paused

**User report, Sep 9:** Updated build still sounds muddy on retail Xbox in the
borg1 opening. Earlier 2D narration and communicator dialogue sounded fine.
XEMU audio checks did not establish retail audio quality.

**Later borg1 observation, Sep 9:** The user reports Munro's line was skipped
and her facial animation did not move during the communicator comparison run.
Record this separately from muddy positional dialogue; the cause is unproven
and audio investigation remains paused.

**Do not investigate or change audio further until the user explicitly requests it.**

- [ ] Resolve positional dialogue quality when work is resumed.
- [ ] Obtain retail listening confirmation; programmatic XEMU checks alone do
      not close this issue.

Evidence: [3D dialogue investigation](notes/vo_3d_diagnosis_2026-09-09.md).

### Test single-player menus and gameplay

Implementation and build checks are recorded; the user-driven pass remains open.

Campaign progress: [map-by-map checks](notes/campaign_checks_2026-09-09.md).

- [ ] Verify normal boot, menu navigation, and representative gameplay.
- [ ] Add a dark overlay behind objectives and pause screens; remove it when returning to gameplay.
- [ ] Verify pause-to-Configure return and audio Cancel/Accept behavior.
- [ ] Verify screen-size Default/Cancel/Accept and Quit/Exit confirmations.
- [ ] Verify save creation, overwrite, loading, and return to gameplay.
- [ ] Fix the campaign ending: Voy20 finishes its conversation but remains on
      a black screen with the HUD. Verify the ending movie and closing credits
      in their authored order; the game stays live, so this is not a confirmed crash.
- [ ] Verify dialogue starts without skipped lines and Borg step/servo audibility
      when audio testing resumes; muddy character voices remain unresolved and paused.

## Cooperative — accepted as-is

**User decision, Sep 11:** Close this work in its current state. No further
co-op optimization or qualification is scheduled. The 30 FPS target was not
met; remaining performance, HUD and menu limitations are accepted. Co-op may
remain clearly labeled unfinished and must not hold up single player or Holomatch.

Details: [accepted state and former checklist](notes/coop_accepted_as_is_2026-09-11.md).

## Holomatch split screen

### Test Xbox controllers

Production assignment and diagnostic multi-view coverage exist; physical-pad
acceptance is still open.

- [ ] Confirm P1's complete action set, non-inverted Y look, Start/Back, and
      immediate movement on the current production binary.
- [ ] Qualify detection and assignment of two, three, and four physical pads
      through the production menu path.

### Check sound with four players

- [ ] Verify the audible four-listener mix during unmuted hardware gameplay
      when audio testing resumes.

### Verify the candidate on Xbox

The final Holomatch optimization pass is closed for the tested XEMU workload.
On hm_borg1, matched gameplay improved from 18.9 to 33.8 native XEMU FPS.
A twenty-minute test with four moving players and four active bots averaged
33.1 native FPS across confirmed gameplay and showed no sustained freeze.
The final build also passed with the new defaults. Retail performance and every
historical freeze cause remain unproven; no further optimization pass is deferred.

- [ ] Stage and measure the candidate on retail hardware.
- [ ] Check the retained split-screen visual settings on Xbox.

Evidence: [slowdown diagnosis and final results](notes/mp_resident_triangles_2026-09-11.md).

Keep EF MDR bodies at the established LOD0 contract. Use the actual EF/native
D3D8 workload; inherited Ghoul2 is not this game's optimization target. Retained
3P/4P policies do not justify additional visual sacrifices without evidence.

Evidence: [Holomatch qualification](HOLOMATCH_QUALIFICATION.md) and
[retained performance/test history](notes/game_todo_history_2026-09-09.md).

## Optimization

### Faster loading and smoother gameplay

**Single-player target:** Improve loading and optimize toward 45+ FPS across campaign levels. Co-op is accepted as-is and its optimization pass is closed. Further Holomatch performance work remains paused until requested. Keep visual fidelity close to its current state. Audio work remains paused.

**Rendering fix awaiting Xbox confirmation, Sep 9:** A screen-capture buffer
was too small and overwrote nearby lightmaps. The corrected allocation removes
stasis1's green stripes in XEMU. A 90-second run preserved all 25 lightmaps.
Janeway's popup also renders cleanly in a diagnostic visual fixture in stasis1;
natural communicator activation still needs checking. Audio work remains paused.
The earlier reported stop was the test harness ending its timed run, not proof
of a crash. Details: [rendering evidence](notes/letterbox_optimization_2026-09-09.md).
**Comparison:** The user confirmed the communicator UI works in borg1 on the
same production candidate. This rules out a universal popup failure, but does
not yet establish that only stasis1 is affected.

Exclude borg1's scrolling-text wall view and any overlapping sample windows
from FPS results. Measure actual gameplay separately from loading and menus.

Two-player co-op was accepted below its former target. XEMU's game clock can run slower than
real time, so its in-game FPS logs can overstate the speed visible on screen.
The pose-cache experiment slowed real-time XEMU progress and has been removed.

- [ ] Measure startup, level-loading, and save-loading times on Xbox; identify
      and reduce the biggest delays.
- [ ] Measure average FPS, slowest moments, and stuttering in single player
      and Holomatch; improve the areas that cause the worst slowdowns.
- [ ] Reduce unnecessary file reads, repeated loading, and memory use where
      measurements show they cause delays or gameplay hitches.
- [ ] Compare before and after on the same scenes and hardware, preserving
      visual quality, stability, and authored gameplay behavior.

Retail Xbox results determine whether changes help. The longer split-screen
match checks above remain the final performance and stability check for that
mode. Audio work remains paused.

## Before release

### Test and package the campaign and Holomatch

- [ ] Broaden `default.xbe` and `efmp.xbe` coverage across loading, menus,
      UI/HUD, representative SP/Holomatch maps, controls, gameplay,
      visuals, stability, memory, and FPS.
- [ ] Stage and qualify the current production XBE pair on retail hardware.
- [ ] Before public release, obtain explicit authorization to commit qualified
      source, then produce clean builds with `sourceTreeDirty: false`.

## Standing constraints

- Preserve ICARUS-authored sequencing, cameras, dialogue waits, and progression.
- Treat retail `jamp.xbe` machine-code contracts as the renderer authority.
- Keep shared renderer/audio/input in `code/`, with separate SP/co-op and
  Holomatch game logic; active builds must not depend on deprecated `codemp/`.
- Build the production pair with `scripts/build_xbox.ps1 -Target spmp`.
- Use XEMU/LLE for emulator qualification; retail Xbox is the final authority.
- Use file-based diagnostics and instrument suspect paths before human tests.
- Run `scripts/cleanup_generated.ps1` around build/package/test cycles.
- Never commit or push without explicit user instruction.

## Evidence and history

- [Holomatch qualification](HOLOMATCH_QUALIFICATION.md)
- [3D dialogue and freeze investigation](notes/vo_3d_diagnosis_2026-09-09.md)
- [Pre-refactor to-do history](notes/game_todo_history_2026-09-09.md) — completed
  implementation notes, retained tests, rejected experiments, and older hashes.
