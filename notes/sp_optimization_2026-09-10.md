# Single-player performance follow-up

MP optimization is closed under the user's beta threshold. This work concerns
the remaining SP target, preserving authored ICARUS behavior and visual quality.
Audio remains paused.

The corrected Forge4 baseline retains seven pre-death timing windows at 41.11
guest elapsed FPS. Later death-view windows are excluded at both frame boundaries.
It is a bounded opening measurement, not a full-route or retail qualification.
Evidence: `notes/evidence/campaign_20260909/forge4_death_exclusion`.

## Visibility diagnostic cvar candidate

SP `R_MarkLeaves` previously searched the cvar list by name for two diagnostic
visibility overrides on every call. Cache registered handles and read their live
integers instead. This preserves defaults and runtime toggles without changing
PVS, area masks, geometry, LOD, textures, or gameplay. A one-time breadcrumb
records the active override values. MP retains its existing path.

SP-only release build passed. The 180-second test stayed live without an engine
error, but retained gameplay averaged 38.21 guest FPS. The shared-interval
comparison was 38.78 versus 41.11 (-5.7%), with six versus seven whole windows.
This small, non-deterministic pair does not prove causation, but it provides no
support for retaining the candidate. Removed the caching change; the SP restore
build passed (`sp_pvs_cvar_restore_build.log`). Death-view exclusion remains.
Evidence: `notes/evidence/campaign_20260909/rejected_pvs_cvar_cache`.

## World-face tangent candidate

The existing Forge4 instruction profile was a verified god-mode diagnostic,
not a death-view profile. It placed 3.7% of samples in `RB_SurfaceFace`.
That path always converts and normalizes three tangent components per vertex.
Ordinary EF passes do not consume those tangents; Xbox bump/environment passes
do. Candidate: skip this decoding in SP unless the shader or one of its active
stages requires it. Stage inspection includes collapsed bump stages. Preserve
the original decoding for special stages and preserve the MP path.

The first build found that `isSpecular` exists only in the non-Xbox stage layout;
the Xbox specular consumer is commented out. Removed that unavailable field
check. Retry build passed (`sp_face_tangent_retry_build.log`). The 180-second
`perf_sp_forge4_face_tangents` run completed live without an engine error.
It recorded 43.51 guest FPS, but a concurrent OpenRedFaction XEMU process was
observed during the run. Do not use this as clean before/after performance
evidence. Both the raw run and its invalid-comparison assessment are archived
under `notes/evidence/campaign_20260909/face_tangents_contended`.

The 75-second `visual_sp_forge4_face_tangents` native-capture/god diagnostic
completed. Both saved frames were inspected sequentially and compared with the
earlier Forge4 capture: room surfaces, lighting, HUD and weapon look intact;
the closed door opens onto visible combat. This is a bounded visual check, not
all-material fidelity proof. Evidence is in
`notes/evidence/campaign_20260909/face_tangents_visual`.
Clean timing validation remains pending; candidate not yet accepted.
