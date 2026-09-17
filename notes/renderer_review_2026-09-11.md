# One-time co-op renderer review â€” 11 September 2026

The next candidate should be a **hybrid submission path**, not the unfinished
triangle-strip experiment. Keep compact indexed geometry for eligible fixed-topology
MD3/MDR model passes, retain resident world geometry, and apply dynamic expansion
to geometry whose changing index sequences actually need the workaround. Follow
that with persistent model vertex data and, if submission overhead dominates,
compatible object batching. These are ranked proposals, not verified FPS gains.

This was a source/recording review only. No renderer code was changed, no build or
emulator test was started, and no defaults were promoted. The standalone
`code/win32/stefx_coop_model_strips.h` draft from before the pause remains
unintegrated and untested. It is not included by `win_qgl_dx8.cpp` and did not run
in any recorded benchmark. Implementation and tests remain paused at the end of
this one-time review so the temporary reasoning setting can be returned to normal.

**What the evidence establishes**

The reviewed timing recording is
`build/research/letterbox_perf/coop_resident_bounded_speed_20260911_145129.flight.jsonl`.
Its frozen SP executable is SHA256
`f6297a9f250d26aa27e3cc7ec08ef1a97a04f9be7a9644aa5a669b3bae1f460e`.
It used original XEMU 0.8.134, resident-world mode2, dynamic arrays mode3,
verification off, and model PVS off. The means and windows exclude the authored
borg1 text introduction using the saved gameplay intervals.

- Native mean: **18.452 FPS**. Worst observed window: **13.632 FPS**, 131 native
  frames over 9.610 host seconds. The conservative full-ten-second lower bound is
  13.1; this is not an exact instantaneous minimum or an exact full-window count.
- In that window, 30 sampled frames show median **609 draw calls**, **597.5 backend
  batches**, and **455 model draws**. Other retained gameplay has 467/315 total/model
  draw-call medians. The median per-frame draw/batch ratio is **1.0203**: generally
  one submission per batch, not a large multiplier from extra material passes.
- Model submissions contain median **31,140 source vertices and 79,323 indices**.
  Mode3 copies one vertex for every index. It therefore expands this model workload
  to roughly **2.55 times** its original vertex-entry count, before discussing
  attribute formats, host page rounding, or actual transferred bytes.
- Whole-gameplay native counters show approximately **0.00000223 index-buffer
  updates per draw**, but **1.036 vertex-buffer updates per draw**. This proves
  that the index-upload problem was largely bypassed. It does not measure the
  duration or byte volume of vertex uploads, cache lookups, or driver waits.
- Model packing rises from 2.695M to 4.103M guest cycles and model BeginPush from
  3.069M to 5.577M in the slow window. BeginPush is a backpressure observation,
  not proof that the currently named model caused all the work it waits behind.
- Present and Finish medians are both zero guest milliseconds in these snapshots.
  The 31 ms EndFrame figure includes executing queued renderer work; it must not
  be described as 31 ms spent only swapping buffers or waiting for vblank.
- Scratch fence-wait time does not increase inside confirmed gameplay. The final
  5,856 ms cumulative wait includes excluded earlier activity. No evidence here
  supports blaming scratch reuse waits for the retained dip.

The host was not a matched quiet baseline: saved external 3D usage averaged
10.27%, maximum11.46%. The earlier full-expansion run still averaged only18.23FPS
with near-zero measured external GPU use, so competing applications do not explain
the whole remaining problem. In the selected 8.274-second host-observation span,
XEMU accumulated3.05 CPU-core equivalents; two threads each used about80% of one
core and another54%. Their functions are not identified by CPU-time counters.
Only28,672 process read bytes and zero write bytes accumulated in that span; this
does not support a bulk disk-loading explanation for this particular dip.

**The main design correction: stable topology does not require expansion**

[RB_SurfaceAnim](Z:/Programming/!archived/Star-Trek-Elite-Force-X/code/renderer/tr_animation.cpp:761)
copies model triangle indices from the surface and separately skins the positions
and normals. [RB_SurfaceMesh](Z:/Programming/!archived/Star-Trek-Elite-Force-X/code/renderer/retail_xbox/tr_surface_retail.cpp:1372)
does the equivalent for MD3 animation. Moving a vertex does not change which
vertices form the triangle. Base offsets can change when surfaces batch, so that
must be accounted for rather than assumed constant for every draw.

The recorded model/material rows support investigating this distinction:
borgbig4 upper hoses have exactly192 source vertices/864 indices per draw across
301 retained publications; borgThin2 upper hoses have148/648 across249; blite
material has66/108 across337. Count stability is supporting evidence only, not
proof that every index byte is identical.

The pinned XEMU source hashes the **index contents**, not the changing positions,
for its element cache (`xemu_native_source/draw.c`, inline-elements branch).
Thus a changing animation pose can reuse an already cached index sequence.
Blindly expanding all models discards that advantage and increases copying,
working-buffer consumption, inline fallbacks, and vertex processing. Strips would
partly compress that expanded representation while still leaving it larger than
the original indexed model in many cases.

The first short comparison should therefore retain the successful world strategy
and restore compact indexed submission only for verified eligible MD3/MDR base
passes. Keep changing world batches, brush-model batches, dynamic-light index
subsets, and other varying topology on the workaround path. **RT_MODEL is not a
sufficient discriminator:** the engine also represents BSP brush models that way;
use the actual MOD_MESH/MOD_MDR/MOD_BRUSH distinction and pass provenance. Keep
dynamic vertex attributes current regardless of whether indices are reusable.

Do not restore indexed draws indiscriminately: earlier co-op sampling proved that
the saturated XEMU element-cache scan really occurred. A hybrid must demonstrate
bounded/reused model index sequences and no renewed sustained miss storm. The
previous model-only expansion experiment did not test this inverse hybrid.

**The next structural opportunities**

1. **Keep eligible model data resident.** Three high-call props are single-frame
   assets: blite, tank, and plugin2, verified directly in the effective release
   PK3s. Together they account for3,149 of13,042 model calls in the30 slow-window
   publications. Disnode has31 animation frames and cannot be treated as an
   unconditionally static model. Cache immutable model-local geometry, retain
   per-instance transforms/lighting and per-pass texture state, and update only
   genuinely changing attributes. Extend reuse to identical animated poses across
   views/passes only after validating complete keys and GPU lifetimes. This differs
   from the rejected CPU pose cache, which still copied results into tess and
   uploaded them again.

   The actual Xbox diffuse-lighting helper writes white vertex colors and sets
   lighting through device state
   ([source](Z:/Programming/!archived/Star-Trek-Elite-Force-X/code/renderer/retail_xbox/tr_shade_calc_retail.cpp:1395)).
   Consequently, different object lighting does not automatically invalidate all
   stored vertex bytes. Fades, fog-modulated colors, texture-coordinate animation,
   deformations, render flags and other generators still need explicit handling.
   An animated texture alone can change a binding without changing geometry.

   In the pinned OpenGL renderer, `update_memory_buffer` rounds attribute reads
   to pages and updates a shared GL buffer when those pages are dirty. Keep
   immutable and frequently written allocations on distinct pages; otherwise
   unrelated writes can defeat residency. Exact byte comparisons belong in the
   verifier, not an expensive full-data hash on every production cache hit.

   The recorded resident world allocation ends with18,711 vertices and150,192
   bytes of unused payload capacity. This is an observation for this map, not
   guaranteed memory headroom on all maps. Any shared resident model/world pool
   needs bounded allocation, fallback, and explicit GPU ownership. Do not add a
   speculative multi-megabyte model buffer to the64MiB Xbox budget.

2. **Reduce compatible object submissions if per-draw overhead dominates.**
   Source flushes a batch when a non-mergeable entity changes, even when the
   material matches
   ([source](Z:/Programming/!archived/Star-Trek-Elite-Force-X/code/renderer/retail_xbox/tr_backend_retail.cpp:1097)).
   Small props alone make4,805 of13,042 model calls in the reviewed publications.
   Blite emits two material draws per instance; tank emits seven, including
   individual two/three-triangle display and bulb pieces. Compatible batching can
   remove submissions that a topology encoding change cannot.

   Do not simply set entityMergable: different model transforms, light directions,
   shader times, fog, depth ranges and alpha can require different draw state.
   A correct path must represent those instance differences explicitly or prove
   the full state identical. Preserve transparent ordering. Material equality
   alone is not sufficient, and batching every model is not a small safe toggle.

Actors account for roughly69% and map props18% of the model-tagged producer draw
cycles in this window; props account for roughly37% of model calls. These sums
include backpressure. They motivate targeting both the large actor payloads and
the many tiny prop submissions, without asserting either is a measured GPU-time
percentage. Static-prop caching alone is not a credible promise of30FPS lows.

**Why the strip draft should wait**

The draft preserves ordered nondegenerate triangle winding in its intended
algorithm, excludes flat shading/wireframe, bounds its topology table, and uses
full index comparisons rather than trusting a hash. The local pinned XEMU source
joins contiguous DRAW_ARRAYS ranges; that supports the proposed command shape,
but no live or retail strip test has occurred.

It nevertheless has poor priority now. It changes neither object count nor draw
count, and the25â€“40% theoretical savings apply to expanded copies of selected
heavy models, not to the whole frame. It also adds79,584 bytes of static key/arena/
workspace arrays if integrated, before small globals. The cache-miss path builds
a strip **before** checking remaining arena capacity; when storage is exhausted,
an uncached topology can be rebuilt repeatedly and then rejected. Cache budgeting,
negative results/admission policy, input preconditions, map reset, command bounds,
post-draw attribute behavior and rendered fidelity still require work. The asset
audit was per surface; actual runtime material batches can combine surfaces.
Do not mistake that audit for qualification of the draft.

**A bounded next investigation, after the user resumes testing**

- Capture a brief dedicated host-thread profile during a confirmed slow window
  with the index-miss problem bypassed. The older profile identifies the former
  element-cache loop, not the current bottleneck. Distinguish guest emulation,
  dirty-page/vertex uploads, driver submission and actual GPU waits. A suspending
  diagnostic sampler must never be included in an acceptance FPS interval.
- Compare the hybrid and all-expanded modes using the same fresh binary,
  uncontended host context and short full-combat fixture. Preserve the intro
  exclusion. Measure native10-second windows, actual submitted vertex bytes,
  index misses by geometry/pass type, working-buffer fallbacks and stability.
  Do not broaden repeated tests until a mechanism and material gain are shown.
- Use that result to choose persistent model storage versus compatible batching.
  Record whether an apparent improvement merely moved waits from BeginPush to
  another stage. Final qualification must use intended runtime defaults and
  distinguish diagnostic overhead, emulator results and retail Xbox results.

Raising the observed13.63FPS window to30 requires roughly a55% reduction in
elapsed time per frame. No current experiment proves that gain. The review
provides a better-ranked path toward it; the full co-op goal remains unmet.
