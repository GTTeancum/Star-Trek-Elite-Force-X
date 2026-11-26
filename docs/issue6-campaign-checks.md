# Campaign checks for issue #6

These checks cover the retail campaign executable (`default.xbe`). The
optional evidence-gathering switches are off by default. For a writable
test installation, back up the loose
`BaseEF/default.cfg`, append the indicated `set` line, then restart through
the normal campaign menu. Restore the file after the test. Do not change
existing saves or turn off visibility culling.

**Freeze on the first manual save.** Standard Start-to-pause captures two
optional thumbnails before it enumerates save slots. A failed surface
allocation or conversion now leaves a thumbnail unavailable rather than
continuing with a missing surface. The preview writer checks the native file
layout, signs in small chunks, releases acquired resources on failure, and
does not reuse stale scratch previews. The legacy default is used if present;
the loose 0.9.4 package may instead show no thumbnail. Slot contents, save
format and game state are unchanged. This is a verified failure path, not a
confirmed explanation for why the report only reproduced with no slots.

For console verification, back up saves and use an isolated, initially empty
save area. With normal logging first, run the standard New Game route at
480p and 720p: press Start, enter Save Game, create slot 1, reload slot 1,
then create slot 2 and confirm an overwrite. Keep a fresh empty area for
each repetition. Separate failures before Pause, at Save Game enumeration,
and after confirming a slot. Repeat with diagnostics only if needed; a
checkpoint last seen in RAM can precede a blocking log flush and does not
on its own identify the next game operation as the stall. The Linux tests
exercise the real source callbacks with mocked D3D/files, not the hardware.

**Rumble and overwrite controls.** On the same unmodified Xbox candidate, try
the reporter's third-party controller/adapter and an original Xbox controller
as a control. Fire one shot and release the trigger; note whether the motor
stops without a second shot or a menu transition. In the campaign controller
settings, use **X** to stage the Vibration change, **A** to keep it and **Y**
to discard it. Check enable, disable, pause/resume and a new shot, then restart
to verify the accepted setting persists. These changes apply to campaign,
not Holomatch. The rumble regression uses the real firing/dispatch path
with simulated feedback, not controller hardware.

With an existing slot, select it and verify the **Overwrite** prompt and the
**Y / Cancel** hint stay clear of each other at 480p and 720p; cancel once
without touching the save, then confirm an overwrite on an isolated test slot.
Check a full list, where the old footer crowded the last visible row.
Linux source/host regressions:
`python3 scripts/tests/verify_issue6_rumble.py` and
`python3 scripts/tests/verify_issue6_frontend.py`. Passing them does not
qualify the console's display or motor behavior.

**Trails in dark shafts/ceilings.** The first screenshot matches the lift
shaft in `borg2` (Incursion). Its green lift marking uses a shader unique to
that map in the v0.9.4 campaign package. Two solid, opaque black world faces
cap the shaft at Z=-766 and Z=1210. The Xbox BSP loader discarded them by
map prefix and texture name, leaving their pixels unpainted in frames that
do not clear color. They have no lightmap; changing lighting gain does not
restore the missing draws.

The second [reported screenshot](https://github.com/GTTeancum/Star-Trek-Elite-Force-X/issues/6#issuecomment-7133922903)
matches a separate eight-sided pit in the same map: face 8485 at Z=-342.
The pose was fitted against the shipped wall and trim textures, excluding the
trail, HUD and weapon from feature matching. Of 178 correspondences held out
of the final fit, 167 reprojected within three pixels, with median error
0.79 pixels at 1280x720. The reconstructed eye is approximately
(1084, -1965, 252), looking down about 70 degrees. All three BSP clusters
that reference the cap can be seen from the camera's cluster.

The loader now keeps authored drawable geometry regardless of material
color or map name, and still skips `SURF_NODRAW` and `noshader`. The
v0.9.4 package audit covers all 66 maps: 17 of 31 black faces had been
removed across Borg1, Borg2, Borg3 and Borg6. The older backing workaround
began with two Borg6 surfaces; it was not a BSP visibility rule.

The companion offline regression takes the checksum-matched, unmodified
public release package (NumPy required). Add the original second image path
(Pillow required) to test it directly:

```sh
python scripts/tests/verify_campaign_pit_projection.py --v094-xbox0 /path/to/xbox0.pk3 --issue6-second-image /path/to/second.jpg
```

In the second capture, 178 of 179 sampled bright pixels fall on the restored
pit floor. Six interior rays see no static world triangle behind it when the
old skip is applied; 12 separate wall/floor control rays are unchanged. In
Borg6, 18 rays on both sides of the authored `static2` doorway hit its
forcefield before and after restoring the two black pit surfaces. This tests
geometry and draw eligibility, not the animated effect or console pixels.

To compare visually, revisit both Borg2 shafts in 480p and 720p. Look up
and down, then move the camera and fire to expose trails. With the ordinary
clear setting, the caps should be steady black. Check the Borg1 scripted
views and the original Borg6 forcefield scenes too, so restored backing
does not obscure live effects. These host tests cannot certify
Xbox rendering.
The optional `set stefx_debugFrameClear 1` (cyan) and `2` (yellow) probes
can distinguish remaining uncovered pixels from later draws. Use the same
camera pose and look for `STEFX_FRAME_CLEAR` to confirm the probe ran.

**Enemies left standing.** Add `set stefx_traceDeaths 1`, launch the campaign
fresh, take the turbolift from the completed warp-core repair in `voy3` to the
first Cargo Bay fight in `voy4` (Defense), and capture the still-standing enemy and its surroundings. The first
32 NPC deaths that use the normal death-animation path per launch emit
`STEFX_DEATH_POSE`. Match the map, game time, model and position to the
pictured enemy. `requested` is the selected animation; `frames` is the
number of frames in its loaded sequence (zero: absent; -1: invalid/unloaded
lookup). `legs` and `torso` are the IDs actually accepted by the game.
Matching IDs with positive frames do **not** prove those frames were drawn.
No matching record is not proof of a broken animation: the actor may not
have taken this death path. The trace itself does not change death selection,
scripts or timers.

The regression reproduces an offscreen ordinary scavenger-family death:
without a rendered last frame the original server cannot start its cleanup
timer, and re-entering view can present an upright opening frame. The
candidate settles to the authored final pose only after the full fall, when
a matching resting pose exists. Nearby and visible bodies still follow the
existing removal gates; scripted deaths, special animations and split
screen keep their original route. Check both offscreen and continuously
visible kills in the Cargo Bay. The host fixture reproduces an upright
return, not a permanently standing body during continuous play.

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
