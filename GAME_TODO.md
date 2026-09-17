# Elite Force X — Open Work Only

Last updated: 2026-09-17

## What still needs doing

1. **Paused:** Fix muddy character voices on Xbox.
2. Test single-player menus, settings, and saves on Xbox.
3. Finish campaign playthrough checks and fix the ending.
4. Test Holomatch with one through four Xbox controllers.
5. Test and package the campaign and Holomatch for release.
6. **Paused:** Check that sound from all four players mixes correctly.
7. Darken the background behind objectives, pause screens, and scoreboards.
8. Align the loading wheel and show the loading screen sooner.
9. Finish Virtual Voyager interactions and verify saves, repeated travel, memory margins, and display modes.

## Details

### September 17 stabilization

The user ended speculative co-op optimization and authorized stabilization,
commit and push. The unmeasured PS2-style lighting policy is removed; original
world lightmaps and the existing split-screen economy policy are retained.
No co-op FPS gain is claimed. See [stabilization](notes/stabilization_2026-09-17.md)
and [independent audit](notes/sol_audit_2026-09-17.md).

### 9. Virtual Voyager remains incomplete

Earlier map-load and save checks do not qualify every station, authored route,
repeated transition or final-build display mode. Preserve the supported memory
work; complete the remaining scope in [the pipeline tracker](notes/virtual_voyager_pipeline_2026-09-12.md)
when VV work resumes. Current stabilization is not a full VV completion claim.

### 1. Character voices — paused

3D dialogue still sounds muddy on retail Xbox; radio dialogue sounds fine.
When audio work resumes, also check skipped dialogue and missing mouth movement.
Keep audible output enabled during visible tests. Programmatic XEMU checks do
not establish retail sound quality.

Do not resume audio investigation or tuning without a user request.

Evidence: [audio investigation](notes/vo_3d_diagnosis_2026-09-09.md).

### 2. Single-player menus, settings, and saves

- [ ] Verify normal boot and menu navigation.
- [ ] Check pause-to-Configure and settings Cancel/Accept behavior.
- [ ] Check screen-size Default/Cancel/Accept and Quit/Exit confirmations.
- [ ] Verify saving, overwriting, loading, and returning to gameplay on Xbox.

### 3. Campaign playthrough and ending

- [ ] Test full routes, map transitions, cutscenes, visuals, and stability.
- [ ] Fix Voy20 stopping on a black screen with the HUD instead of showing
      the ending movie and credits in their authored order.
- [ ] Confirm the stasis1 texture and communicator fixes on retail Xbox during
      normal gameplay.

Opening-area checks do not establish complete campaign progression.

Evidence: [map-by-map campaign checks](notes/campaign_checks_2026-09-09.md).

### 4. Holomatch controllers

- [ ] Test one through four physical Xbox controllers through the normal menus.
- [ ] Verify detection and player assignment.
- [ ] Check movement, look direction, actions, Start, and Back.

### 5. Release testing and packaging

- [ ] Verify the current campaign and Holomatch builds on retail Xbox,
      including stability and split-screen visuals.
- [ ] Package the tested builds for testers.
- [ ] Obtain user authorization before committing or publishing a release.

Evidence: [Holomatch qualification](HOLOMATCH_QUALIFICATION.md).

### 6. Four-player sound — paused

- [ ] When audio testing resumes, verify that all four players' sounds mix
      correctly during unmuted gameplay on Xbox.

### 7. Background darkening

- [ ] Darken the whole screen behind objectives and pause screens.
- [ ] Darken only the area within each scoreboard's borders.
- [ ] Restore normal brightness when the overlay is dismissed.

### 8. Loading display

- [ ] Align the animated loading wheel with the wheel baked into the artwork.
- [ ] Show the loading screen early enough that the game does not appear frozen.

## History

Closed items and prior results are retained in
[history](notes/todo_cleanup_2026-09-11.md).

Virtual Voyager testing-build work closed September 12: [completion and evidence](notes/virtual_voyager_testing_2026-09-12.md).
