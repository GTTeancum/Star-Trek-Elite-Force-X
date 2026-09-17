# Virtual Voyager manual testing feedback — 2026-09-12

Superseded by the September12 follow-up: user confirmed audio fixed; texture and portal corrections are implemented and locally tested. See virtual_voyager_followup_2026-09-12.md. The following preserves the earlier report, with the incorrect DDS claim corrected.

User reports smooth movement, no audible sound, and excessive texture blur in the final testing image. These are unresolved testing findings, not a successful audible-output or texture-quality qualification.

Read-only checks of manual XEMU PID26440 found effects/voice volume1.0, music0.25, s_soundStarted1, s_soundMuted0, backend state4. The ring contains successful voice-spatial submissions and ambience requests. This excludes the intentional s_initsound0 diagnostic path but does not prove rendered audio reaches the output device. Do not conflate this silence investigation with the separately paused muddy-3D-voice tuning.

Live r_picmip is1. The package caps ordinary textures at128; inspected Voyager door DDS headers confirm128px maximum dimensions. CORRECTION: the earlier claim that the DDS uploader skips to64px inspected an inactive backend. Active retail DDS upload retains the top mip; the package128px cap is the demonstrated source-detail reduction; no texture setting or asset was changed in the live session. A correction needs memory-budget verification rather than an untested global increase.

The same session also logged Com_Error code1: CM_AdjustAreaPortalState: negative reference count, then orderly server shutdown and return to the main menu. This is a newly discovered gameplay failure despite prior local load/save checks. Preserve original portal behavior while locating the unmatched close; no fix has been established.

Live logs and read-only cvar/package evidence: C:/Users/smmel/.codex/tmp/stefx_vv_manual_20260912_113858/. The user's controls and running emulator were left untouched; background recorder remains attached. Current final ISO was not modified.
