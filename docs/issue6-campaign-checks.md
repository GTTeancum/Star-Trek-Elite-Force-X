# Campaign checks for issue #6

These are evidence-gathering switches for the retail campaign executable
(`default.xbe`), not fixes for the three symptoms. Neither is enabled by
default. For a writable test installation, back up the loose
`BaseEF/default.cfg`, append the indicated `set` line, then restart through
the normal campaign menu. Restore the file after the test. Do not change
existing saves or turn off visibility culling.

**Trails in dark shafts/ceilings.** Save a view where the defect repeats.
Compare the same position, resolution, camera movement, and weapon action
with the switch off, `set stefx_debugFrameClear 1` (cyan), then
`set stefx_debugFrameClear 2` (yellow). These are steady fills behind the
scene, not flashing overlays. Look for `STEFX_FRAME_CLEAR` to confirm the
requested mode ran. If the bad area changes with **both** colors while the
walls do not, the clear color is showing through there: investigate which
surfaces, sky or blending passes should cover it. If the same repeated
image remains with both colors, this test has not established missing
coverage; inspect later draws, including sampled render targets. A clear
does not identify a bad material or a visibility bug by itself.

**Enemies left standing.** Add `set stefx_traceDeaths 1`, launch the campaign
fresh, load a save just before the first large Voyager firefight after the
repairs, and capture the still-standing enemy and its surroundings. The first
32 NPC deaths that use the normal death-animation path per launch emit
`STEFX_DEATH_POSE`. Match the map, game time, model and position to the
pictured enemy. `requested` is the selected animation; `frames` is the
number of frames in its loaded sequence (zero: absent; -1: invalid/unloaded
lookup). `legs` and `torso` are the IDs actually accepted by the game.
Matching IDs with positive frames do **not** prove those frames were drawn.
No matching record is not proof of a broken animation: the actor may not
have taken this death path. No death selection, scripts or despawn timers
are changed by the trace.

**Slow level entry.** Leave both switches **off**. From a fresh launch,
load a save just before a level transition and follow a repeatable route
for the first 20 seconds of player control. Reload that same pre-transition
save without quitting and repeat. Keep the display mode, save, difficulty,
storage device and inputs the same; separate time on the loading screen
from time after control returns. The second run is warm at the process
level, not necessarily at every asset cache. Keep video with the first
controllable frame marked on each pass. Save `ef_sp_log.txt` before the
next launch (title directory on writable installs, otherwise typically
`E:\`). Compare `STEFX_LONG_FRAME` entries and their total, server, client
and command-buffer milliseconds. These are the first **10** completed frames
of at least **250 ms** per process, not a log of every slow frame. A warm
run with no record after the cap, or prolonged sub-250-ms stutter, cannot
be declared smooth. A slow client/server phase narrows the search; it does
not by itself establish background loading or GPU cost.
