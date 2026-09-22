# Co-op resolution and pillarbox comparison — September 17

1. Keep the existing co-op display defaults; the combined experiment showed no useful FPS gain.
2. Preserve both long runs, including stalls and evidence of concurrent host GPU work.

## Result

Two 900-host-second Borg1 probes used the original pinned XEMU 0.8.134,
Above Normal priority from launch, native emulator scale, the same ISO/data,
64 MiB guest RAM, audible 0.65, P1 input replay and P2 input mode 3.
No camera bypass, script override, warp, texture change or audio change was used.
No builds ran during either measurement. Both ended live, active, two-player,
error-clear and ISO-restored; owned XEMU processes closed.

| Native XEMU counter measurement | Baseline | Candidate |
| --- | ---: | ---: |
| All retained gameplay average | 5.2747 FPS | 1.8268 FPS |
| Retained gameplay host seconds | 672.64 | 676.03 |
| Overlapping gameplay average | 4.8241 FPS | 1.7910 FPS |
| Overlapping gameplay host seconds | 248.13 | 663.31 |
| Worst approximately ten-second window in overlapping gameplay | 2.3149 FPS | 0 FPS |
| Early overlapping interval before recorded other-game GPU activity | 3.8101 FPS | 2.2788 FPS |

The overlapping server-time envelope is 87,095–148,987 ms. Whole published
gameplay windows trim the endpoints differently; this is not a deterministic
frame-for-frame replay. The observed average difference is -62.87%. The early
comparison envelope is 87,095–100,853 ms, with -40.19% observed difference.
These percentages are NOT isolated causal estimates of the renderer change.

The long-run slow window reaches zero in both runs. Baseline's long stall
overlaps recorded GranTurismo2PC GPU activity at host t521.2–565.7 seconds
(sampled peak84.61%). Candidate also has that process active at t543.8–582.4
(peak83.53%). It had severe stalls before that recorded GPU activity too.
The early comparison conservatively retains windows ending at least30 seconds
before the first such GPU observation, then intersects server-time coverage.
It does not establish absence of other CPU/disk work or identical combat.
Do not attribute all full-run slowdown to the game change or promote this
as a controlled hardware benchmark. It establishes no useful gain here.

P1 first exhausts current weapon energy at baseline t450.18/server142147 and
candidate t821.12/server142414. The full runs are therefore NOT sustained
two-player firing for their entire duration. Both players have independently
observed firing events and movement; later ammo depletion remains visible.
The overlapping comparison includes a small depleted-ammo tail in both runs.

## Opening exclusion and measurement

User visually confirmed 3D at baseline t168.660, recorded in
`baseline_visual_cutoff.json`. The engine still reported the scripted camera
then. Acceptance begins only after complete `gameplay=1 excludedChecks=0`
publications: retained native observations start baseline t222.663 and
candidate t214.089. All boundary-crossing intro windows are excluded.

FPS is native XEMU frame-counter delta divided by host elapsed time, not
guest-clock FPS, inverse MSPF, or an average of individual FPS samples.
The full FPS publication stream has zero dropped publications in both runs.
Slow windows use actual observation endpoints covering80–100% of nominal ten
seconds, following the existing analyzer; these are not instantaneous minima.
Zero-progress intervals remain included. Retail performance is unverified.

## Candidate implementation and decision

`r_efCoopLowRes 1` is an opt-in, default-OFF experiment limited to actual
two-player co-op and compiled inactive in hosted Holomatch. SDK
`SetBackBufferScale(0.8,0.8)` lowers rasterization resolution and scales on
presentation. On a16:9 display the combined gameplay area gets80-pixel side
margins in logical640x480, yielding physical4:3. Pixel-aspect/FOV calculation
is corrected with it; top/bottom split and HUD mapping remain aligned.

The full scaled canvas is512x384. Because480p widescreen uses non-square
display pixels, the central gameplay storage is384x384, displayed as a4:3
area (equivalent to512x384 square pixels). This is NOT two individual4:3
player views. Candidate runtime readback is `(1,1,800,800,80,80,640,480)`.
Theoretical scene pixel coverage is48% of baseline; it is not an FPS prediction.

Backing color/depth allocations and texture pools are unchanged. This test
does NOT bank framebuffer RAM. Lower pixel count did not produce a useful
speedup under these conditions. Leave the experiment off; do not automatically
promote it or infer that all resolution/pillarbox implementations are exhausted.
No release ISO was promoted, and no commit/push was performed in this pass.

Both Release builds passed. The host test executes production guard, aspect
and border functions for SP, hosted MP and other configurations; it passes
default-off, wrong mode/player count,4:3 output and calibration preservation.
Native frame/progress/window analyzer tests also pass. Candidate captures at
14:32:47,14:38:20 and14:41:20 were individually reviewed and show intact world,
weapons, HUDs and black side borders. The t420.9 screenshot failed during a
long stall; retained rather than silently replaced. Baseline captures at
14:12:57,14:15:34,14:18:34 and14:21:34 were individually reviewed.

## Reproduction and immutable evidence

All run preparation, GPU sidecars, analyses and immutable bundles remain under
`build/research/audit_stabilization_20260917`. Exact source run names:

- `coop_resolution_baseline_20260917`, flight suffix `20260917_140755`.
- `coop_resolution_candidate_20260917`, flight suffix `20260917_142737`.

Original flight streams and final dumps remain in `scripts/output`; summaries
and ISO transactions remain in `build/research/letterbox_perf`.
`baseline_result.json`, `candidate_result.json`, `resolution_comparison.json`
and `resolution_identity.json` preserve numeric evidence and identities.
`run_coop_resolution.ps1` reproduces the configuration using unique run names.
`scripts/analyze_coop_resolution.py` reconstructs confirmed gameplay from
streamed publications; `scripts/compare_coop_resolution.py` compares intervals
and exposes the recorded other-game GPU activity.

Baseline SP XBE: `b16e4703f33760aebb015c342285bb189a63fc2b6b64e1c32aabb43b019c7790`.
Candidate SP XBE: `1ca4e6f4aa8248b8aaa5ac7fe6eef7523cf770fcde358d2b795e1b9ba7b2ebdc`.
