# Tester rebuild — September 22, 2026

1. Close the co-op performance experiment without promoting the resolution candidate.
2. Disable unnecessary research logging in production and retain error diagnostics.
3. Rebuild both Xbox executables and package the existing Virtual Voyager assets for testers.
4. Commit and push the source checkpoint; keep game and SDK payloads local.

## Scope

User explicitly ended optimization and requested logging cleanup, tester rebuild,
commit and push. No new performance target or audio work is opened.
Production builds now skip detailed frame sampling, profile publication and
research ring-marker formatting/writes. The frame-diagnostics build still
supports those tools. Critical errors, startup diagnostics and the basic FPS
publication ring remain. No performance gain is claimed from this cleanup.
The co-op resolution/pillarbox experiment stays default OFF.

## Final experiment disposition

The September 22 baseline recorded 0.3838 native XEMU FPS over 224.06 seconds
of accepted gameplay (86 frames), with no GT2 samples in its GPU record.
The first 900-second candidate never reached accepted gameplay. The longer
2400-second candidate finished in game, live, with an empty engine-error buffer
and its ISO restored. Its continuous recorder failed during loading; the final
64 guest-clock records do not establish native host FPS or a valid comparison.
No recovered native recording was completed. These runs do not prove a causal
resolution benefit or that every possible optimization has been exhausted.
All evidence is preserved under build/research/coop_resolution_20260922 and
build/research/letterbox_perf. No owned emulator remains active.

## Validation and delivery

Existing VV route,
station and retail-hardware limitations remain open. This is a tester candidate,
not a claim of full campaign or retail qualification.

The co-op aspect/mode guards, HUD dispatch regression, probe provenance,
ISO component verifier (six tests), and paired VV ISO replacement tests pass.
Changed Python harness/analyzer modules compile. Source whitespace checks pass.
Both production executables were inspected: research profile and ring-marker
functions compile to a single RET; error and FPS publication functions remain.

Both Release builds passed without FrameDiagnostics. The paired build used
compile fingerprints to reuse unchanged SP objects, then built Holomatch.
Complete matching EXE/MAP/XBE bundles are preserved under
`build/research/tester_20260922/symbols`; build and compiled-logging checks are
beside them. The logging functions are no-ops in both final binaries.

Final executable SHA256:

- SP/co-op: `c74715871b21da8b9a9d94658b2bb1fda80a2d34f7b8b0f2f078a20480b03590`.
- Holomatch: `eee4edfd9c81d61aefc36db8c57dc979629c16022015d1f810d10c177781208e`.

No new emulator or retail run was performed on this tester pair. Earlier
runtime evidence remains tied to its earlier executable hashes.

Tester image: `build/beta/StarTrekEliteForceX-Tester-20260922/StarTrekEliteForceX-Tester-20260922.iso`

All six packaged components match the release files: both executables, xbox0/xbox1
packages and the sound bank/table. The source image is preserved; its directory
scan found no harness markers. The package includes instructions, component
manifest and SHA256SUMS.txt. No game or SDK payload is committed to Git.

ISO SHA256: `fdc3cc4dafe3f81348f7211891b7d17de5329240b453032fbd6654664bc48519`.
