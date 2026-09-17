# Borg1 3D voice diagnosis — 2026-09-09

## Current result: opening freezes confirmed fixed on retail; audio unresolved

User confirmation, Sep 9: borg1 no longer freezes with the latest fixes. The
opening-freeze item is closed. The user reports that 3D dialogue still sounds
muddy on retail Xbox and has explicitly paused further audio work. The XEMU
audio results below do not establish a retail audio-quality fix.

The final Release XBE includes the voice clarity fix, atomic file-handle
ownership, and failed flare-query handling. Its SHA-256 is
`18837272e0e967edf2117cce0971a34d2fb6f7de46eec2d4f897ac943a209e98`.
Both final 270-second captures used this unchanged executable, normal XEMU DSP
and HRTF settings, and the authored borg1 opening. All five dialogue matches
passed the unchanged confidence, ordering, spectral, clipping, and routing
gates. No invalid-free diagnostic or fatal allocation occurred.

| Final proof | Game progress | Player presence changes vs source | Clipping |
| --- | ---: | --- | --- |
| `archive_final.acceptance.json` | 159939 ms | -0.474 / -1.113 / -0.841 dB | none |
| `banked_verified.acceptance.json` | 166205 ms | -0.365 / -1.179 / -0.651 dB | none |

The exact tested XBE is at `build/release/default.xbe` and is installed in
`build/xemu/StarTrekEliteForceX_unified_minisoak.iso`. The embedded XBE hash was
read back and verified; `installed_release.json` records the update. The ISO is
3340857344 bytes. Temporary fixture changes were restored, and cleanup ran
before/after packaging. Retail hardware has not been re-tested with this build.

## Freeze investigation and intermediate builds

The archived-MP3 setup now reproduces the stall in XEMU. The original frozen
RAM dump (`archive_stall_ram.bin`) contains a self-linked zone free block at
`0x01b78bc0`; the CPU loops in `Z_LargestFreeBlock_NoLock`, called by renderer
memory reporting. This is downstream heap damage, not a DirectSound stall.
The Release diagnostic caught the offending duplicate free on a repeat run:

```
STEFX_ZONE_INVALID_FREE ptr=01B94174 header=01b94170 caller=000FDC3E tag=0
```

In that diagnostic build, caller `0x000fdc3e` is the return from `Z_Free` inside
`unzClose`. The FS handle selector returned an unused slot without reserving it;
opening an archive yields, allowing the main thread and audio streaming thread
to select the same slot and subsequently close/free the same archive object.
The lower-level WF selector had the same reservation race.

The fix atomically reserves both tables before opening, releases failed opens,
retains the FS reservation during zero-length path retries, and publishes a
closed slot as free only after clearing its old state. A Release invalid-header
diagnostic now logs the caller before a duplicate free can corrupt the list.
No ICARUS behavior was changed.

The native concurrency test compiles the actual source selectors and exercises
160,000 operations across eight threads: zero ownership failures with the fix;
the original selectors produce 92,061 ownership collisions in the negative
control. `scripts/tests/verify_file_handle_reservation.py` reproduces this test.
The unchanged production XBE, SHA-256
`210f9e7ac9d7ef92d99e197230653650c1973251e1f44bacdc502b7226f9bca0`,
passes the archived-audio qualification. The fixture temporarily hides only
`BaseEF/sound` and `BaseEF/soundbank` in the ISO; the XBE is unmodified and both
directory entries are restored afterward. `archive_production.acceptance.json`
records five matched dialogue lines, no clipping, player presence changes of
-0.444/-1.062/-0.803 dB, correct source reuse, no invalid free, and game progress
to 145103 ms. A separate 270-second archived run of the bank-disabled diagnostic
variant reached 161657 ms and passed the audio measurements; its final log ring
had rotated out the source-routing records, so it is not labeled a full gate pass.
The subsequent banked run (`banked_production`) exposed a separate renderer
fault and therefore **failed** qualification. Its saved RAM and trap stack locate
the original access violation at `D3DDevice_GetVisibilityTestResult+0x1f`, called
by retail `RB_TestZFlare` for query 113. The query-page pointer is null; the flare
has `visible=0`. The XDK 5558 `EndVisibilityTest` implementation returns
`0x8007000e` when allocating that page fails, but the caller ignored its result
and attempted readback on the next frame. This unchecked source path is present
in the Aug 21 retail renderer import, before the recent audio edits; the loader's
initial `visible=-1` assignment is intact. No claim about Claude's authorship or
the exact allocation-pressure trigger follows from this history.

The renderer now restores the unsubmitted sentinel on failed submission and
retries before reading a result, with bounded before/after HRESULT logging.
The fault-injection test compiles the actual `RB_TestZFlare` function, forces
three allocation failures, then allows successful queries: eight submissions,
four valid readbacks, zero invalid readbacks, and eventual visible output.
`scripts/tests/verify_flare_query_failure.py` is the reproducible test.
The new Release XBE has SHA-256
`18837272e0e967edf2117cce0971a34d2fb6f7de46eec2d4f897ac943a209e98`.
`archive_final.acceptance.json` passes all gates for this exact executable.
The `banked_final` run remained stable through 151641 ms of game time and passed
tone/clipping checks, but its acknowledgement correlation was 0.549494 versus
the unchanged conservative 0.55 gate. Its measurement is retained in
`banked_final_ack.json`; this run is not labeled a full acceptance pass.
The unchanged-executable repeat, `banked_verified`, passed every original gate;
its acknowledgement correlation is 0.679. The earlier borderline result remains
retained rather than being overwritten or accepted by lowering the threshold.

The user's Sep 9 retail test froze during the scrolling-text transition. The
preserved log is `build/research/audio_probe/retail_freeze_20260909.log`.
It identifies the tested Sep 9 11:20:05 Release build, confirms successful DSP
download/listener setup, but reports `sound.bnk missing`. Janeway's narration
uses archived MP3 fallback, while the successful XEMU proof used banked WAVs.
The last heartbeat is serverTime=65994; no player spatial-voice event appears
before the log ends. This log does not identify the stalled call, and missing
assets alone are not yet a proven freeze cause. Earlier XEMU acceptance must
not be read as hardware qualification or proof for the archived-MP3 path.


## Original voice-clarity implementation and earlier proof

The Release SP executable now initializes the DSP graph unconditionally and
renders positional mono dialogue with equal-power stereo speaker gains. It
retains listener-relative position, changing left/right balance, channel reference
distances, distance muting, voice volume and playback completion. Dialogue avoids
HRTF spectral filtering; other positional effects retain HRTF. Source reuse
switches back to the correct buffer type. Multiple listeners divide the gain to
bound the sum; default 2D-buffer headroom remains active. Initial gain and spatial
state are applied before playback to avoid a stale-volume first frame.

Dialogue now uses stereo panning instead of HRTF front/back and elevation timbre
cues. Height still affects distance and pan. ICARUS scripts, sequencing, dialogue
durations and cameras were not changed. Communicator/narration routing remains 2D.

Validated executable: `build/release/default.xbe`, SHA-256
`1a5297b82021602908796b339575c52ebfd29bc4ac3fd57887ad8f6b3e2ea64b`.
The Release build and build identity/freshness checks passed using
`scripts/build_xbox.ps1 -Target sp -SkipAssets -SkipStage -ReuseObjects`.

| Player line | Before: DSP on/full HRTF | Fixed Release |
| --- | ---: | ---: |
| I've been cut off | -6.55 dB | -0.42 dB |
| Acknowledged | -6.10 dB | -1.05 dB |
| Tertiary what | -6.88 dB | -0.80 dB |

Janeway measured 0.00 dB and Tuvok -0.69 dB. The entire 236.74-second capture
contained zero clipped samples. XEMU DSP emulation and HRTF were enabled; all
44,389 frames came from GP/EP output. The saved HDD boot log identifies the Sep 9
build and successful DSP download/listener setup. Runtime logs show successful
speaker-gain updates and restoration of HRTF for spark/door effects. Game profiles
continued to serverTime=141583 after the complete opening dialogue sequence.

The native C++ test passed 20,448 spatial cases: azimuth rotation, left/right
symmetry, height, distance attenuation, maximum-distance mute and one to four
listeners' summed gain bounds. It uses the actual production gain calculation.
This is mathematical coverage, not four-player runtime qualification. Runtime
qualification covers SP borg1; retail Xbox output and performance remain untested.

Reproduce the automated capture and acceptance checks:

```powershell
python scripts/run_vo_audio_proof.py --iso build/xemu/StarTrekEliteForceX_unified_minisoak.iso --xbe build/release/default.xbe --exe C:/Games/Emulators/Xemu/JACodex/xemu.exe --config build/research/audio_probe/xemu.toml --output build/research/audio_probe/voice_fix_v2.raw --seconds 240
python scripts/verify_vo_audio_proof.py --capture build/research/audio_probe/voice_fix_v2.raw --assets build/release/BaseEF --xbe build/release/default.xbe
```

The verifier checks executable hash, normal DSP/HRTF configuration, five ordered
dialogue matches, <2 dB coloration, zero clipping, effect mode restoration,
continued game progress and ISO restoration. Proof is in `voice_fix_v2.*`.
The shared ISO was restored to its original executable; the fixed deliverable
is the Release XBE above. No commits or hardware staging occurred. The protected
controller configuration was untouched.

The first candidate capture failed because the harness held a writer open on
the network ISO. That run and its stale HDD-log fragment are invalid. The harness
now closes the writer before launch and rejects DVD-open failures/early exits.
The successful v2 run is the qualification evidence.

## Earlier diagnosis (superseded by the resolution above)

## Conclusion

The tested assets and their Xbox conversion are sound. Two distinct problems
are involved: the missing effects graph breaks 3D routing in a clean DSP-emulated
boot, while directional HRTF filtering produces substantial voice coloration even
with the graph loaded successfully. Claude's graph repair is justified, but it
is not a demonstrated complete fix for muddy dialogue. Light HRTF reduces the
coloration only modestly.

These are programmatic XEMU results, not a claim of retail-hardware qualification.
The user's observation is retail Xbox, borg1 opening: narration and Tuvok's
communicator are clear; the cinematic player is muddy. The captured ISO selects
Alexa. Both Alexa and Munro source conversions were independently checked.

## Measurements

Presence is output/source gain in 2–6 kHz minus gain in 200–1,000 Hz. Negative
numbers indicate speech-band loss relative to bass. Mixed scene ambience biases
these values; waveform correlation checks alignment, not intelligibility.

| Configuration | I've been cut off | Acknowledged | Tertiary what |
| --- | ---: | ---: | ---: |
| Original ISO, XEMU default VP output | -9.80 dB | -9.43 dB | -8.57 dB |
| Current binary, DSP graph forced on, full HRTF | -6.55 dB | -6.10 dB | -6.88 dB |
| Same graph, light HRTF | -4.68 dB | -5.10 dB | -5.89 dB |
| Graph forced off, full HRTF, DSP emulation on | No accepted match | No accepted match | No accepted match |
| Same graph-on binary, emulator HRTF filter bypassed | +1.29 dB | +0.79 dB | +1.79 dB |

The graph-off run continued through the scene: the runtime voice-start records
exist and Tuvok's 2D line matches at correlation 0.576, with -0.66 dB relative
presence. Player correlations are only 0.073, 0.225 and 0.197 (threshold 0.35).
Thus missing player matches are not simply a failure to reach the dialogue.

The HRTF bypass is an emulator diagnostic, not a proposed Xbox fix. It also
raises levels: 0.0188% and 0.0083% of samples in the first two matched player
segments clip; the third does not. It establishes a major filter contribution,
but neither subjective quality nor production gain/panning behavior is qualified.
The full and light runs had no clipping. The full graph-on run passed the
previously reported ~51-second freeze and played all three lines.

The original-ISO baseline differs in executable version and monitor point.
Do not attribute its difference from the current graph-on run solely to the
image download. The subsequent graph-on/off, light and bypass controls use the
same current base binary and explicit GP/EP output.

## Independent asset validation

- All six English opening player recordings, male and female, were re-extracted
  from the original PAK archives and decoded independently with FFmpeg.
  The loose MP3s match those archive entries byte for byte. Xbox WAV conversions
  correlate above 0.99995 with the originals; relative presence changes are below
  0.04 dB. This substantially excludes conversion as the muddy-voice cause.
- The local voice bank audit examined 3,913 records: 3,907 mono 44.1 kHz Xbox
  ADPCM, four stereo 44.1 kHz, two mono 22.05 kHz. No malformed RIFF chunks or
  partial ADPCM blocks were found. All opening lines are mono 44.1 kHz.
- All 20 cin/01 bank records match corresponding loose WAVs byte for byte.
  This validates the local stage, not the identity of files on the retail Xbox.
- The Xbox whole-WAV upload path correctly finds the data chunk in the backend.

## Assessment of Claude's changes

The embedded 24,936-byte image exactly matches the XDK 5558 standard image;
reverb index 0 and crosstalk index 1 are correct. Local XDK documentation requires
this graph for 3D sound. Supplying it is a valid repair. The default remains
s_dspImage=0, so this repair is not active by default. A release-folder autoexec
setting is not proof of what is installed: the inspected shared ISO lacks that
loose autoexec and contains a different XBE from the current release file.

The claim in snd_dsp_image.h that a later voice writes through returned pointers
into the original image is unsupported. XDK 5558's WMAStream sample downloads
an image at main.cpp:142 and frees the input buffer at line 145. A freeze does
not prove the proposed write or lifetime requirement. A write during download
would be a different hypothesis; this investigation does not establish its cause.

Light HRTF retains azimuth processing and loses elevation filtering. It improved
these measurements by only about 1–2 dB. The remaining investigation should
focus on the actual listener-relative emitter position and resulting filter
selection before choosing an engine-side spatialization policy. The actor's
position can use its origin instead of eyePoint; that is a hypothesis, not a
proven position bug. ICARUS sequencing and dialogue waits were left intact.

## Evidence and reproducibility

Preserve build/research/audio_probe/ until superseded. It contains raw captures,
*_analysis.json, original_asset_comparison.json, original PAK source excerpts,
control measurements, executable snapshots, controlled_variant_manifest.json,
*.variant.json restoration records, saved HDD boot-log fragments, and isolated
HDD/EEPROM/config. prior/ preserves Claude's September 7 reports. Diagnostic
XBEs must not be distributed as release candidates.

scripts/capture_xemu_vo.py launches its own hidden XEMU, checks executable hash
and instruction signature, and uses a read-only process-local hook to copy the
48 kHz stereo S16LE APU output before clearing. It records the monitor point of
every frame. All explicitly DSP-enabled runs use point 4 (GP/EP). SDL volume is
muted only in the isolated configuration, downstream of capture. No desktop
input, UI automation, host audio capture, or ICARUS overrides were used.

scripts/analyze_vo_capture.py uses FFmpeg and normalized waveform alignment,
then measures spectral bands and clipping. Its controls passed: a delayed
unchanged reference matches at 1.000 seconds, correlation 0.99999993, presence
error -0.00031 dB; a deliberate 1.2 kHz low-pass measures -25.17 dB; silence is
rejected. Python syntax and git diff whitespace checks passed.

A validity correction matters: the RAM scanner initially reported the literal
'STEFX_AUDIO_DSP: source=disabled bytes=0' as evidence of execution. That same
text resides in the binary regardless of branch selection. The scanner now
rejects that literal. Saved HDD boot-log fragments and running guest instruction
readbacks validate diagnostic settings instead. The early default-only experiment
and its supposed disabled-branch conclusion are not used as controlled evidence.

Matching XEMU source explains the two emulator controls:
[output selection](https://github.com/xemu-project/xemu/blob/fc24584ce88f0915ad7f04775bb7712c2e3f49ee/hw/xbox/mcpx/apu/dsp/gp_ep.c)
and [HRTF filter application](https://github.com/xemu-project/xemu/blob/fc24584ce88f0915ad7f04775bb7712c2e3f49ee/hw/xbox/mcpx/apu/vp/vp.c).
The initial default configuration omitted use_dsp and sampled the VP path.
Only controls with explicit DSP emulation can evaluate the downloaded graph.

At the end of this earlier diagnosis, no production audio changes or commits had been made by Codex. Tests temporarily
replace only the shared ISO's default.xbe region and restore the original bytes
in a finally block, with hashes recorded. The protected controller cfg was not
modified. Generated-artifact cleanup runs before and after each capture cycle.
