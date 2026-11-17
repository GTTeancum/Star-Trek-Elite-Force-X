# Issue 1: campaign preferences and screen-size crash

Issue: https://github.com/GTTeancum/Star-Trek-Elite-Force-X/issues/1

## Implementation

- Configure > Controller now offers Invert Y using left/right. Accept applies
  the campaign `m_pitch` convention and saves it; Cancel discards the change.
  The right-stick diagram reflects the pending selection.
- Configure > Video offers bilinear/trilinear filtering using White. Accept
  applies and saves the selection; Default selects bilinear. The renderer
  already supports both modes, and the shipped textures contain mipmaps.
- Fixed the screen-size crash: the four-element corner-color array previously
  used enum indices 1 through 4, writing/reading beyond its end. Corner indices
  are now 0 through 3, and drawing/cycling share the declared count.
- Native reboot captures exposed a second calibration defect: the saved viewport
  margins were applied again to the adjustment markers. The renderer now uses
  physical screen coordinates while the calibration screen is active. Ordinary
  menus and gameplay continue to use the saved margins.
- EF's normal frontend boot now loads these preferences after its canonical
  controller defaults. This also restores saved screen calibration. Holomatch
  retains its separate per-player pitch preferences.
- Signed settings version 082902 appends the filter preference. Both prior
  formats (082877 and 082901) migrate with bilinear selected. Invalid signatures,
  sizes, versions and filter values are rejected before updating live settings.
- Added `-BinkLibrary <path-to-binkxbox.lib>` and an early dependency check to
  the build script, since its historical external dependency path is missing.

## Validation and remaining work

Host tests compile actual menu/settings functions with mocked platform APIs.
They cover persistence, both migrations, corruption rejection, Apply/Cancel/
Default, corner drawing/cycling/clamps and preservation of unrelated settings.
They also exercise the actual stick-to-view-angle function (ordinary and
override pitch paths), and the renderer's calibration viewport isolation.
The original corner numbering is deliberately reintroduced and rejected by
the draw spy. These tests do not qualify Xbox graphics or native save signing.

Both final Release Xbox builds pass using XDK 5558. The complete SP-hosted
Holomatch code policy check and `git diff --check` also pass. Build command:
`scripts/build_xbox.ps1 -Target spmp -SkipAssets -SkipStage -ReuseObjects`.
The final follow-up is recorded in `build_spmp_calibration.log`; the earlier
successful pair remains in `symbols_before_calibration_followup/`.

Artifacts are under `build/issue1_20261116/`: build/compile/test logs, original
EXE/MAP/XBE pairs in `baseline/`, and hardware inventory/save backup records.

The initial link failed because the Bink library was absent. The user restored
the explicitly approved
`Z:\Programming\RM4+JadeSrc\Libraries\GX8\bink\binkxbox.lib` location. Its archive
member list validated, and its SHA-256 is
`3cdb8099d7dfb0a6ce03c684b3e27a352ef032c4a97fb156514862a1f611da5d`.
No broader external search was performed.

## Hardware preparation

- Confirmed XBDM at 10.0.0.231:731, version 1.00.5933.1. This is the user's retail
  Xbox running in devkit mode. No debugger attached or remote input used.
- Backed up all nine files in `V:\4c41000b` to `xbox_save_backup/` before any
  preferences test. Existing JA saves share this title ID.
- E: has about 177 MB free. The user allows removal of installed games;
  inventory identifies only `E:\Games\GW2` and `E:\Games\Timesplitters-Stability`.
  Both game folders were removed under that authorization. The focused install
  was deployed to `E:\Games\StarTrekEF-Issue1`; about 208 MB remains free.
- Extracted the complete September 22 tester ISO to `stage/` without modifying
  it. `stage_source_manifest.json` records original files and hashes.
- `hardware_stage/` is a distinct menu/Borg1-only diagnostic asset set,
  1,383,728,019 bytes before the new executable. It retains the original sound
  bank, Borg1 music, opening movies, and all non-map Xbox overrides. Other native
  maps, Holomatch, unused music/movies and the stale filecode cache are omitted.
  Retained PK3 entries were checked for identical CRC and uncompressed size.
  This incomplete diagnostic install is not a release package.
- The final `default.xbe` is staged on the Xbox. The complete extracted stage
  also has the new XBE pair; original/current manifests distinguish their hashes.

For native menu validation, an explicit `stefx_menu_smoke settings` startup
command exercises the actual Configure handlers, inversion Accept/Cancel,
all four corners, trilinear selection, video Accept and Default/Cancel. It
holds each new option for 20 seconds for native `xbcapture` inspection and
logs `STEFX_SETTINGS_TEST` records through the production critical log path.
It deliberately saves settings. After reboot, `settings-readback` opens both
screens without changing saved preferences. Expected video values from the
write pass are trilinear with margins 8,6,8,6; inversion is toggled from the
original value. Restore the backed-up settings after testing and remove the
diagnostic startup commands. The harness is inactive without explicit opt-in.

## Native results and artifacts

The write sequence completed in 226 seconds without a logged engine error.
Native captures of Controller and Video were inspected individually: labels,
glyphs, controller diagram, panel backgrounds, prompts and four corner markers
are visible and readable. All four corners were adjusted in rendered frames;
Accept and Cancel produced the expected runtime values. The signed save grew
from 152 to 172 bytes and carries version 082902, inversion enabled and filter 1.

An independent boot loaded inversion 1, filter 1 and margins 8,6,8,6. Its
read-only menu sequence completed in 183 seconds. Its capture revealed the
double-margin defect described above; `hardware_settings_readback_final/`
contains the final-binary follow-up evidence. That follow-up completed in 182
seconds, loaded the same saved values, and its individually reviewed capture
matches the original pending corner positions (8,6 margins, no second inset).

The 144-second Borg1 scene check used the saved trilinear preference and no
simulated gameplay input. Its native capture shows the authored corridor scene
with Telsia, her weapon, textured walls/floor and the green Borg alcove. No
engine error was logged, and the owned run ended with a successful reboot.
This scene check used the executable before the calibration-only follow-up.
It is not a campaign playthrough, physical-controller test, audio qualification
or performance comparison. Analog inversion is additionally covered by the
actual input-function regression test; Holomatch is build/code-check qualified,
not hardware gameplay qualified in this task.

The final immutable pairs are in `symbols/`. SHA-256:

- `default.xbe`: `ab03378ff7ae91c0896a3650cfce6f2fb470817e5d0de6bf58f6f2f8c2c04071`
- `efmp.xbe`: `dc18449a5eb7f51bd02ebc6f7dda6a96d44270218f6a67cd4f4c36195f7f3c8d`

`StarTrekEliteForceX-Issue1-update.zip` contains the executable pair, install
instructions and checksums for an existing complete tester installation. It
does not contain game assets. The console's focused install remains limited to
menus/Borg1. Diagnostic launch markers have been removed and all owned test runs
have ended. The original preferences were restored; a complete readback confirms
all nine original title save files match their backup SHA-256 hashes exactly.
The final deployed XBE also matches the frozen build hash. See
`restoration_verified.json` and `xbox_final_root.txt`.

No commit, push or GitHub issue mutation has been performed. Existing user
changes to `box art.png` and `Thumbs.db` were left untouched.


## Subsequent console performance build — November 17

The preferences fixes are retained in the newer production executable pair and
binary-only update documented in `coop_split_console_2026-11-16.md`. That report
supersedes this note's final-installed binary identities and focused-stage scope;
the earlier issue-1 artifacts remain preserved as historical evidence. The newer
console work again restored all nine original save files by hash readback. No
commit, push or issue mutation was performed.


## Commit/push authorization — November 17

After ending the performance pass, the user authorized committing and pushing
the completed preferences fixes together with the split-screen changes and
console reports. Earlier no-commit statements describe the pre-authorization
checkpoints. Game assets, build outputs and unrelated user image changes remain
local. No additional runtime testing or release upload is requested.
