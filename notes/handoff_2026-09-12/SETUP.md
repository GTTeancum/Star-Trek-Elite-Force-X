# New-PC setup and transfer checklist

Windows x64 is the supported existing harness environment. These are instructions for the receiving machine; work on the old PC is stopped.

## Download and install

| Tool | Version and purpose | Download |
|---|---|---|
| Git for Windows | Current stable; source/diffs | [Official site](https://gitforwindows.org/) |
| ripgrep | Windows x64 source search | [Official releases](https://github.com/BurntSushi/ripgrep/releases) |
| PowerShell7 | pwsh wrappers; retain Windows PowerShell | [Microsoft instructions](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-powershell-on-windows) |
| Python | 64-bit3.12, matching Python312 workflow | [Python3.12.10 Windows installer](https://www.python.org/downloads/release/python-31210/) |
| Java JDK | Temurin21; old installation21.0.10+7 | [Adoptium JDK21](https://adoptium.net/temurin/releases/?version=21) |
| Ghidra | Pin11.0.1 for existing project/extension | [NSA release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_11.0.1_build) |
| PS2 processor extension | emotionengine-reloaded v2.1.13 for11.0.1 | [Matching ZIP](https://github.com/chaoticgd/ghidra-emotionengine-reloaded/releases/download/v2.1.13/ghidra_11.0.1_PUBLIC_20240202_ghidra-emotionengine-reloaded.zip), [documentation](https://github.com/chaoticgd/ghidra-emotionengine-reloaded) |
| XEMU | Pin0.8.134; capture/stat hooks are binary-dependent | [Official release](https://github.com/xemu-project/xemu/releases/tag/v0.8.134) |
| Visual Studio2022 C++ tools | Desktop development with C++,MSVC,Windows SDK; host regression programs | [Microsoft downloads](https://visualstudio.microsoft.com/downloads/) (select2022/older downloads where necessary) |

The matching extension ZIP, source manifest and expanded/installed copy already travel in build/research/ps2_re/tooling and tool_home. Prefer preserved matching artifacts. Gradle is unnecessary for the prebuilt extension. PCSX2,Docker,WSL and desktop-control plugins are not required. FFmpeg may be needed for future media conversion; current work preserves prepared audio/assets and the SkipAssets build doesn't require a media reconversion pass.

## Copy/provision from the existing authorized installations

Transfer package prepared on 2026-09-13 at `Z:\C_DRIVE_NEW_PC`. Read its README.md and INSTALL_ON_NEW_PC.ps1. It contains the clean XDK5558, complete existing VS8 supporting tree with private matching MASM CRT, Bink library, MCPX and BIOS. Repo-contained game content, HDD and EEPROM are intentionally excluded. REGISTRY_NOT_REQUIRED.txt explains why this command-line workflow needs no registry import. Public-download tools above remain separate.

These are not supplied by Git or modern Visual Studio; no public replacement download was verified.

| Dependency | Old source path | Purpose |
|---|---|---|
| Clean Xbox XDK5558 | C:\XDK_5558\XDK, entire installation | Xbox CL/Link/Lib,headers/libs,xsasm,image tools |
| Visual Studio2005/8 assembler | C:\Program Files (x86)\Microsoft Visual Studio 8\VC\bin\ml.exe and supporting installation | Explicit build-script preflight dependency |
| Xbox Bink library | Z:\Programming\RM4+JadeSrc\Libraries\GX8\bink\binkxbox.lib,412,598bytes | External hardcoded linker dependency |
| MCPX | C:\Games\Emulators\Xemu\MCPX\mcpx_1.0.bin,512bytes | Boot ROM |
| Debug BIOS | C:\Games\Emulators\Xemu\BIOS\xbox-4627_debug.bin,1,048,576bytes | Working firmware |
| Emulator disk/EEPROM | repo build/research/audio_probe/hdd.qcow2 and eeprom.bin | Existing saves,wide/480p settings |
| Game content | Whole repo,including root PS2 ISO,third_party_private,BaseEF PAK0–3,xbox0.pk3 | Source and prepared packages |

Use clean5558, **not modified C:\XDK5849**. Copy dependent runtimes or install from existing licensed media; copying one EXE may not be enough. VS2022 does not substitute for the Xbox compiler or the explicit VS8 MASM path. Bink can be copied inside the new repo and its linker path updated; don't replace it with a different platform's library. Keep private SDK/game material private.

Inspect the QCOW backing chain before relocation (compatible qemu-img info --backing-chain if available). Copy every backing file and preserve/rebase paths safely. The chain was **not checked at handoff**. Don't replace with a blank disk or discard saves.

## Python

From the new repo root, create a fresh environment; do not trust a venv copied from another machine:

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r notes/handoff_2026-09-12/requirements.txt
.\.venv\Scripts\python.exe -c "import PIL, psutil, numpy, pycdlib, elftools, capstone, lzokay; print('imports OK')"
```

Use the installed Python executable if py launcher is unavailable. Requirements reflect installed versions, not proof every module was exercised. lzokay also exists in repo build/research/ps2_re/python_deps. Ghidra scripts use bundled Jython; ExtractLoader.py is not a standalone Python3 script.

## Relocate paths before running

Old paths:

- Repo Z:\Programming\!archived\Star-Trek-Elite-Force-X, also \\WATSON\Media\Programming\!archived\Star-Trek-Elite-Force-X.
- Python C:\Users\smmel\AppData\Local\Programs\Python\Python312\python.exe.
- XEMU C:\Users\smmel\.codex\tmp\stefx_xemu_0134\xemu.exe.
- Ghidra Z:\Programming\ghidra_11.0.1_PUBLIC.
- Java C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot\bin\java.exe.

Search active scripts/configs for these before use. Don't bulk-rewrite archival evidence. Key files:

- scripts/build_xbox.ps1: XDK5558,VS8 MASM,Bink directory near1404,staging path.
- build/research/ps2_re/scripts/run_ghidra_loader.py: Java and Utility.jar; run from repo root. Keep -Duser.home repo-local for extension/project work.
- build/research/virtual_voyager/manual_profile.json,xemu_480p_16x9.toml,terminal_fix/run_probe.py,probe_command.json and wrappers: repo,Python,XEMU,firmware,disk,capture/log paths,image/symbol pair.
- scripts/run_sp_perf_probe.py,ja_xemu_smoke.py and helpers: default paths,binary-sensitive hooks,legacy mutable symbol paths.

Keep extracts/projects inside the new repo. Local SSD is preferable: SMB compilation and5400RPM storage affected prior runtimes. Check full transfer size, not just ISO size.

## Ghidra recovery

Use11.0.1 and matching extension, processor r5900:LE:32:default, compiler default. Isolated extension lives under build/research/ps2_re/tool_home/.ghidra/.ghidra_11.0.1_PUBLIC/Extensions. The wrapper uses Java directly to avoid cmd handling of ! paths. Retain generic output before rerun. R5900 project stopped during postscript, without confirmed final save: inspect safely or import into a fresh repo-local project. The existing nine-function loader_decompilation.json is NOT the R5900 result.

If Sleigh recompiles slowly, use matching Ghidra's Ghidra/Features/Decompiler/os/win_x86_64/sleigh.exe to compile extension .slaspec into .sla; verify output timestamps. Don't repeat known-wrong generic imports. Precompiled repo-local r5900.sla is already present.

## Build and emulator preflight — receiver only

RE can proceed before Xbox SDK setup is complete. Preserve transferred binaries/symbol bundles before building. Resolve all external paths first. Typical build:

```powershell
pwsh -NoProfile -File scripts/build_xbox.ps1 -Target spmp -SkipAssets -SkipStage -FrameDiagnostics
```

Prior incremental builds also used -ReuseObjects; omit it for first new-machine build to avoid assuming cache portability. Don't use -Clean casually. SkipAssets preserves prepared package/audio; SkipStage avoids old C:\Games\Emulators\CXBX\Star-Trek-Elite-Force-X staging destination. Put intended Python on PATH for helpers. VS2022 serves host tests, not Xbox compilation.

Preserve these existing XEMU settings, merging into sections rather than duplicating them:

```toml
[display.ui]
aspect_ratio = "16x9"
[display.window]
startup_size = "1280x720"
[display.quality]
surface_scale = 1
[sys]
avpack = "hdtv"
[audio]
use_dsp = true
volume_limit = 0.65
```

aspect_ratio belongs to display.ui, not display.window. Main EEPROM flags0x90000 and checksum were validated. Update firmware,disk,games_dir,screenshot and DVD paths. Diagnostic config contains a stale DVD path overridden by probe --iso; launching config alone may load wrong build. Manual and diagnostic controller bindings differ; new-PC GUID may differ. Never promote an uncontrolled/muted diagnostic as manual profile.

Before trusting new-PC runs: confirm paired ISO/build ID and MAP+EXE+XBE,64-MiB emulated RAM,native scale/wide settings,valid recorder/capture,exclusive monitor ownership,ISO transaction restoration and owned emulator shutdown. Host performance measurements aren't interchangeable across PCs.
