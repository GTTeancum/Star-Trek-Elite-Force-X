# Stabilization after the Sol audit — September 17

1. Restore the original co-op lightmap path and retain the existing split-screen economy policy.
2. Preserve supported memory work and the accumulated source checkpoint.
3. Verify paired builds and bounded runtime stability without claiming an FPS gain.
4. Commit and push source, tooling and status notes while keeping SDK/game payloads local.

## Changes

Removed the unqualified `r_efCoopPerformance` renderer policy, its lightmap skip,
redundant dynamic-light suppression, declarations and menu/launch setters.
Existing co-op economy behavior remains: it already suppressed dynamic lights
in the normal two-player configuration. World lightmaps load normally again.
The rejected model-array experiment remains removed, primary push buffer remains
1 MiB, and curve subdivision remains 64. No new optimization was introduced.

The earlier fixed audio needs a 24,936-byte DSP image from XDK 5558. Keep the
generated `code/win32/snd_dsp_image.h` local. `scripts/generate_dsp_header.py`
validates the installed SDK image SHA256 and generates the same writable array;
the build calls it before dependency checks. A matching existing header is left
untouched. Audio behavior and image bytes are unchanged. Standalone IDE builds
must run this generator once after installing the licensed XDK.

XEMU smoke launch defaults to Normal. During the stability run the user requested
higher priority on this older PC; the owned co-op XEMU process was changed to
Above Normal after the 330-second observation. A `--process-priority` option now
propagates through both harness entry points, and the VV check uses Above Normal
from startup. This mixed-priority co-op run is not an FPS comparison.
Virtual environments, raw evidence trees, the generated SDK header and redundant
historical whole-tree patch are ignored by Git and preserved on disk.

## Validation

- Existing DSP header matches the SDK exactly; generating a separate fresh header succeeds.
- HUD dispatch regression test passes and reproduces the old third-draw failure.
- Actual lump-stream implementation passes all eight seek/partial-read cases.
- Memory observation reader: seven tests pass.
- Probe provenance: three tests pass.
- Paused-start verifier: two valid and two rejected states pass.
- Actual MDR frame-cache lookup/eviction passes 10,000 pairs for each SP/MP
  configuration and reproduces the historical pointer-overwrite regression.
- Both Release personalities build successfully with frame diagnostics; no
  asset repackaging or legacy emulator staging was performed.
- Complete frozen EXE/MAP/XBE bundles and the pre-build pair remain under
  `build/research/audit_stabilization_20260917`.
- Previous audit's independent guest-snapshot check: 6,619,208 animation bytes
  in four resident MDR models match original assets exactly.

Co-op runtime: `audit_stable_coop_20260917` completed its 480-second bounded
Borg1 run without `exitview` or slice-warp. The authored opening progressed
into two-player gameplay. Both viewports completed 876 frames; P2 refdef is
valid; final state is active/live with an empty engine-error buffer. Native
13:33:37 and 13:35:38 captures were individually reviewed: both views, world,
characters, weapons/effects and HUDs render intact. The ISO transaction restored
and owned XEMU closed. This is local stability evidence, not a full campaign
route, retail qualification or performance comparison.

VV runtime: `audit_stable_vv_20260917` completed its 180-second Deck02 check at
Above Normal priority. The save proof is `(1, 2, 2, 0)`: schema 1, two writes
(including overwrite), two loads and zero signature failures. Final state is
active/live with no engine error. Native 13:40:51 and 13:40:57 captures were
individually reviewed and show intact room, lighting, weapon and HUD. The ISO
restored and owned XEMU closed. Final published free physical memory is 2 MiB.

Co-op's final published free physical value is only 4 KiB. This is not proof
of an allocation failure or sustained duration, and reserved zone capacity is
separate, but the broader memory-pressure issue must remain open. These two
bounded passes do not certify all VV stations/routes, repeated transitions,
all campaigns or retail hardware. No FPS improvement is claimed.

Frozen XBE SHA256:

- SP/co-op: `b16e4703f33760aebb015c342285bb189a63fc2b6b64e1c32aabb43b019c7790`.
- Holomatch: `3591e099c1fe11016ab1fecd99799ce66262553b1c97e45b1bf5c84b36d3a885`.

## Commit scope

This is a checkpoint of accumulated SP/co-op/Holomatch/VV source and diagnostics
since the previous commit, plus this stabilization. It is not a claim that every
historical experiment or every VV interaction has been qualified. Existing
default-off research paths remain default-off. Audio fixes are preserved, not
retuned. No SDK binary, source ISO, game package, emulator HDD, firmware, raw RAM
dump or build output belongs in the commit.
