# Holomatch: raise the slow averages above 30 FPS

Status: active, requested September 11 after the resident-triangle pass. Target
the lowest ten-second confirmed-gameplay averages with four moving local players
and four actual opponent bots. Preserve current visuals and keep audio, SP and
co-op optimization paused. These renderer paths are currently enabled only in
Holomatch, although some techniques may later transfer to the other modes.

## Baseline and measurement

Qualified default XBE SHA256:
`1f155977c67d71bca5587e95db073617ac5b289e25fca6d317f7eee0afedfb81`.
Frozen artifacts: `build/research/letterbox_perf/world_arrays_default_candidate`.
The earlier twenty-minute original-XEMU recording is
`world_arrays_long_20260911_012103`; its gameplay average was 33.12 native FPS.

The earlier 25.51 FPS low used non-overlapping ten-second windows. New analysis
also starts a ten-second window at every native observation, so dips crossing
the original boundaries are included. On the same recording, the lowest is
23.76 FPS at host seconds 409.69–419.33. There are 476 windows at or below 30 out
of 2,111 overlapping windows; these overlap and are not independent events.

`scripts/analyze_native_lows.py` uses actual native-counter differences and
actual observation elapsed time. Window duration is 80–100% of ten seconds;
gameplay gaps are excluded, stalled counters are retained, and resets fail.
Four tests cover these contracts. No interpolation, guest-clock substitution or
reciprocal MSPF is used. Retail performance remains separate.

## Findings and isolation sequence

Six separated slow windows had roughly 494–606 median draw batches and 184–260
vertex uploads per native frame. Six fast windows had roughly 307–347 batches
and 77–104 uploads. The worst window precedes the observed element-cache miss
threshold, but earlier unobserved misses prevent that threshold from proving
actual cache occupancy. The isolated guard comparison is needed to assess its
remaining contribution. These counts are correlations, not timed attribution.

The same twelve windows show zero additional XEMU process read bytes. Total
XEMU CPU consumption was about 3.0–3.4 core equivalents in both groups, while
rendered frames differed sharply. That points away from asset/disk reads for
these particular dips; it does not rule out loading or IO problems elsewhere.
The exported draw-kind diagnostic separates HUD, resident world, dynamic world
base, extra world passes, model and effect calls through `dllDrawElements`.
Its producer-side cycles include waiting for earlier GPU work and must not be
misreported as per-material GPU timings. It is omitted from production builds.

First isolate the known XEMU LRU scan using the separate diagnostic guard copy
with the final default game build, keeping resident world storage enabled. That
is an emulator-only diagnostic and cannot count as an Xbox-code improvement.
Then test the already byte-verified compact dynamic vertex layout alongside
resident triangles, on the original emulator. Earlier compact-layout trials
without resident triangles were confounded by the dominant index-cache issue
and did not establish a reliable gain. Additional changes require measured
mechanism and sustained low-window improvement.

Existing beta and hardware release checks are unchanged. No source commit or
publication is authorized by this optimization request.

## Resident-world cache-guard isolation

`lows_guard_world_20260911_061900` ran for 900 host seconds with the final
default game XBE and the separate diagnostic LRU-guard emulator. Four moving
local players and four bots, final liveness, clear engine error state and ISO
restoration all passed. Both native captures show intact four-player combat.
Gameplay averaged 30.98 native FPS; the lowest rolling window was 15.35 FPS
(host seconds 602.78–612.62). There were 635 overlapping windows at or below
30 out of 1,555. This is diagnostic emulator evidence, not a game-code gain.

Removing that known scan does not eliminate the remaining dips. Different
combat paths and host conditions prevent treating this run as a precise
regression against the earlier overnight baseline. At the worst interval,
XEMU process reads remained unchanged and CPU consumption fell from about
3.2 to 2.6 core equivalents. Backpressure and host contention still need to
be distinguished; neither is established as the cause by those numbers.

The next production-build comparison enables the already byte-verified
compact dynamic vertex layout with resident world triangles on the original
emulator. Separate diagnostic builds will attribute draw submission costs.
The old full-payload reuse-hashing diagnostic is now opt-in (`r_efReuseProbe`)
so it does not distort those measurements; production already omitted it.

## Compact dynamic upload comparison

`lows_compact_world_20260911_063542` completed 900 host seconds on the original
XEMU with resident world mode 2 and compact dynamic uploads enabled. Full
workload, live endpoint, clear engine error and restored ISO passed. Both
native captures were inspected; four-player geometry, HUD and combat effects
remain visible. Gameplay averaged 31.68 native FPS, but the lowest overlapping
window was 8.75 FPS at host seconds 233.04–242.65. Of 1,509 windows, 436 were
at or below 30. This does not qualify the new low-window goal or justify a
default change.

During host seconds 233–243, median native draw count was 453 and vertex
uploads 92. XEMU used about 2.16 CPU core equivalents, versus 3.27 at seconds
670–680 (422 draws and 86 uploads). Both intervals had zero extra process file
read bytes. The two busiest rendering/emulation threads used approximately
0.37/0.38 cores in the dip, versus 0.76/0.76 later. These similar work counts
with reduced thread progress need host scheduling/wait attribution; counts
alone do not establish a game rendering regression. The native frame-history
stream reports 319 missed records, but FPS uses the independent monotonic
native frame counter rather than summing captured history rows.

No compact-upload default change was made. The category diagnostic is being
built separately with `FrameDiagnostics`; it is not an acceptance binary.

## Draw-category diagnosis and next candidate

`lows_draw_kind_diag_20260911_070125` completed 600 seconds with full workload,
live endpoint, clear engine error and restored ISO. Both native captures were
inspected. The diagnostic XBE is
`0ff46fae1e678c46d323ca179f370ab34b6584c33959c909051ca772f6c0e7e2`,
runtime September 11 06:59:26. Its sampled fast/slow quartiles have median
guest frame times 18/38 ms and average model draw counts 79.5/196.0. Model
submission accounts for 31.3%/50.4% of measured draw cycles. This producer-side
time can include GPU waiting; it is not an independent GPU measurement.
Extra world passes contributed essentially no draws in this run.

The live profile mirror independently showed repeated skinning. Sample 951
processed 13,296 MDR vertices, with 9,270 repeated pose vertices, and 5,747,363
guest cycles in skinning. The corresponding log excerpt is saved in
`lows_draw_kind_diag.live_log.txt`. This identifies redundant computation,
without assuming all remaining FPS dips have the same cause.

Two explicit five-second host-PC samples were taken in this diagnostic run.
Both resumed all threads with zero pre-existing suspension counts. Most
observations were in waiting/system code, JIT code or XEMU; neither sample
identified the old saturated-cache loop as a hot function. These samples do
not establish time percentages and make this run unsuitable for acceptance.

The new candidate `r_efMdrSkinCache` defaults to 0. It retains up to 8,192
CPU-skinned vertices (256 KB) within an engine/render frame, keyed by surface,
frame, old frame, vertex count and exact interpolation bits. Hits copy only
position, normal and UV components into the existing tessellation buffer.
Material evaluation, transforms, topology, GPU submission and padding remain
unchanged. Allocation/capacity failures take the original skinning path.
`r_efMdrSkinVerify` recomputes and compares every reused vertex byte, disabling
the candidate on a mismatch. Exported counters record usage and verification.

The compiled helper test passed exact bits, untouched padding, changed pose
and frame keys, allocation failure, vertex/key bounds and mismatch disabling.
Runtime verification and production-build performance comparison are pending;
no gain or new default has been accepted.

Host CPU telemetry in that diagnostic run also identifies a confound in its
worst native-counter window (473.51–483.22 seconds). Host utilization rose to
about 85–89% in several one-second samples while XEMU dropped to 1.2–1.35 CPU
core equivalents. Whole-run medians were approximately 39% host utilization
and 3.17 XEMU cores. No build or thread-PC sampling ran in that interval.
This supports host contention contributing to that dip; it does not explain
every dip or establish which other process caused it. Preserve observed lows
and record host conditions when comparing candidates. Do not stop other user
processes or quietly discard slow windows to pass the goal.

The first production-XBE pose-cache verification (`lows_skin_cache_verify`,
XBE `22383c352dd20b9e45846b1cb79625edaaf5182a8e6323c2d03cfdf0936aeb50`,
runtime 07:19:10) FAILED correctness: a live read showed 745,423 verified
vertices followed by one mismatch and automatic disabling. No performance
comparison or default activation is allowed on this evidence. Additional
failure logging now records source/cached component bits and bone-palette
hashes to diagnose why the existing animation path produced different bytes
for a cache key. This instrumentation has not yet been rebuilt.

The follow-up detailed run `lows_skin_detail_verify_20260911_072924` also
failed after 1,161,983 matching vertices, with one mismatch and automatic
disable. Full workload, liveness and restoration passed. Its mismatch line
rotated out of the general log mirror before inspection, so the next build
retains the first mismatch in a dedicated exported 32-word buffer. Do not
treat either failed run as a successful optimization test.

Source inspection then found a real frame-cache lifetime violation:
`R_STEFX_GetMDRFrame` used FIFO replacement without protecting returned hits.
After filling all 32 slots, fetching cached frame 0 then uncached frame 32
overwrites the buffer still referenced by the frame-0 pointer. `RB_SurfaceAnim`
keeps exactly such a current/old frame pair while constructing bone matrices.
This can make identical requested animation keys yield different actual data.
Whether it accounts for the observed pose mismatch still requires runtime
verification.

The MP-only fix protects the most recently returned cache slot against the
next eviction. The existing decoding and frame bytes are unchanged. A test
compiled from the actual lookup/eviction section reproduces the old overwrite
and verifies 10,000 fixed current/old pairs across two model identities,
equal-frame requests and FIFO wraparound. The new exported protection counter
will confirm the path is exercised. SP/co-op behavior has not been changed;
review this shared-source bug when that work resumes.

The frame-pair fix and persistent mismatch evidence are now being built for
another runtime verification. Pose reuse remains off by default.

`lows_skin_pair_verify_20260911_073850` reproduced the pose mismatch after
2,137,507 matching vertices, with no frame-pair protection yet exercised.
The persistent record identifies frame/oldFrame 137/137, interpolation 0 and
different actual bone-palette hashes (`8824c31e` versus `12c9eabd`). Position
and normal differences are small (several floating-point ULPs); UVs match.
Thus the independently reproduced FIFO bug is not the cause of this mismatch.
Do not conflate those findings. The record is `lows_skin_pair_verify.live_proof.json`.

The pose cache has now been revised to key the actual returned bone-palette
pointer and generation as well as its requested pose. Palette hits retain
their generation; rebuilding/evicting a palette assigns a new one. The
uncompressed immutable frame-bone pointer uses generation zero. Lookup occurs
after the existing palette getter, so it preserves the matrix version the
original path actually uses instead of assuming nominally identical frame
requests imply identical matrices. This revision is not yet built or verified.


## MP checkpoint and user-directed switch to co-op

The palette-version candidate built successfully for SP and MP. Frozen MP XBE
SHA256: da7a7bd2ec7647b343102c30a7faf373f3160bcfe55e85a6fcf22524014dfc22,
production runtime Sep 11 07:49:37. Actual-helper cache tests passed.

Run lows_skin_palette_verify_20260911_075018 FAILED exact-output verification:
a live diagnostic read found 1,039,273 matching vertices followed by one
mismatch and automatic cache disabling. Both palette hashes were d3d13a7b;
frame/oldFrame 62/62, interpolation zero, vertex zero of a 14-vertex surface.
Position/normal differences were small floating-point ULP differences; UVs
matched. Matching palette hashes did not establish identical skinned output.
The cause is unresolved. Frame-pair protection count remained zero.

This is the requested natural stopping point. The user explicitly redirected
work to co-op averaging 30 FPS, using the same dip analysis. Stop further MP
candidate iteration here. Pose caching remains off by default and has no
qualified FPS gain. Do not close the unmet MP low-window target as achieved.
The original qualified resident-world baseline remains separate.

The native capture xemu-2026-09-11-07-53-05.png was inspected: four rendered
views and their HUDs were present. That single image does not qualify exact
output, continuous movement, or visual coverage. Final endpoint and restoration
results are recorded below when the bounded probe finishes.

Final paused proof confirms the same mismatch counters. All four views,
four actual bots and movement-command lanes passed the existing workload gate;
CA_ACTIVE, continued main-loop progress and an empty engine-error buffer were
confirmed. Physical free memory was 2,605,056 bytes. The probe exited and
restored the working ISO. The failed cache remains disabled. Essential final
proofs, launch identity and the inspected capture are preserved under
`notes/evidence/mp_checkpoint_20260911/`, with SHA256 manifest. No FPS gain is
claimed from a correctness-failed verification run.
