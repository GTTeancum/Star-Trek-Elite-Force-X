"""Exercise actual menu/settings code with in-memory Xbox APIs and draw spies.

No desktop input or emulator control. Generated C++/binaries/temp files remain
under build/tests/issue1. This does not qualify Xbox rendering or signed saves.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/tests/issue1"
VC = Path("C:/Program Files (x86)/Microsoft Visual Studio 8/VC")


def function(source, name):
    start = re.search(r"^(?:static )?(?:void|qboolean|int) " + name + r"\(", source, re.M).start()
    return source[start:source.index("\n}", start) + 2]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    temp = OUT / "tmp"
    temp.mkdir(exist_ok=True)
    env = dict(os.environ, TMP=str(temp), TEMP=str(temp), PYTHONDONTWRITEBYTECODE="1")
    env["PATH"] = str(VC.parent / "Common7/IDE") + os.pathsep + env.get("PATH", "")
    settings_header = (ROOT / "code/qcommon/xb_settings.h").read_text()
    settings_source = (ROOT / "code/qcommon/xb_settings.cpp").read_text()
    settings_header = re.sub(r"^#include .*\n", "", settings_header, flags=re.M)
    settings_source = re.sub(r"^#include .*\n", "", settings_source, flags=re.M)
    frontend = (ROOT / "code/ui/ui_ef_frontend.cpp").read_text()
    corners = re.search(r"enum \{\n\s*EF_VIDEO_CORNER_TL.*?\n};", frontend, re.S).group()
    state = frontend[frontend.index("static int s_videoCorner"):frontend.index("static char s_menuSmokeTarget")]
    functions = "\n".join(function(frontend, name) for name in (
        "EFFe_LoadControllerState", "EFFe_LoadVideoState", "EFFe_ApplyVideoState",
        "EFFe_CancelVideoState", "EFFe_AdjustVideoCorner", "EFFe_HandleVideoKey",
        "EFFe_HandleControllerKey", "EFFe_DrawVideoScreen"))
    fixture = (ROOT / "scripts/tests/fixtures/issue1_preferences.cpp").read_text()
    body = fixture.replace("// SETTINGS_CODE", settings_header + "\n" + settings_source)
    pad = (ROOT / "code/win32/win_input_console.cpp").read_text()
    body = body.replace("// FRONTEND_CODE", corners + "\n" + state + "\n" + functions +
                        "\n" + function(pad, "IN_STEFX_IsFrontendButton"))
    inputs = (ROOT / "code/client/cl_input.cpp").read_text()
    body = body.replace("// INPUT_CODE", "\n".join(function(inputs, name) for name in
                        ("CL_MouseClamp", "CL_MouseMove")))
    renderer = (ROOT / "code/win32/win_qgl_dx8.cpp").read_text()
    body = body.replace("// SAFE_AREA_CODE", function(frontend, "UI_EFMainMenu_IsCalibratingScreen") +
                        "\n" + "\n".join(function(renderer, name) for name in
                        ("STEFX_SafeAreaClamp", "STEFX_GetSafeArea")))
    for mode in ("sp", "spmp"):
        cpp = OUT / (mode + ".cpp")
        exe = cpp.with_suffix(".exe")
        cpp.write_text(body)
        args = [str(VC / "bin/cl.exe"), "/nologo", "/MT", "/EHsc", "/D_XBOX", "/D_CRT_SECURE_NO_DEPRECATE",
                "/DSTEFX_ELITE_FORCE_SP", "/I" + str(VC / "include")]
        if mode == "spmp":
            args.append("/DSTEFX_SP_HOSTED_MP")
        args += [str(cpp), "/Fo" + str(cpp.with_suffix(".obj")), "/Fe" + str(exe),
                 "/link", "/LIBPATH:" + str(VC / "lib"),
                 "/LIBPATH:" + str(VC / "PlatformSDK/Lib")]
        subprocess.run(args, cwd=OUT, env=env, check=True)
        subprocess.run([str(exe)], cwd=OUT, env=env, check=True)
        # Restore the original invalid corner numbering in the same executable.
        # The draw spy rejects its uninitialized pointer without dereferencing it.
        if mode == "sp":
            cpp.write_text(body.replace("EF_VIDEO_CORNER_TL = 0", "EF_VIDEO_TITLE, EF_VIDEO_CORNER_TL")
                           .replace("cornerColors[EF_VIDEO_CORNER_COUNT]", "cornerColors[4]")
                           .replace("i < EF_VIDEO_CORNER_COUNT", "i < 4"))
            subprocess.run(args, cwd=OUT, env=env, check=True)
            failed = subprocess.run([str(exe), "original-corner-bug"], cwd=OUT, env=env)
            if failed.returncode != 23:
                raise AssertionError("Original out-of-bounds read was not caught")
    print("PASS: SP/MP save roundtrips, both legacy migrations, corruption rejection, "
          "startup restoration, corner cycling/drawing/clamps, Apply/Cancel/Default, "
          "invert-Y/filter persistence, real stick-to-view-angle direction, calibration viewport isolation; "
          "original corner defect reproduced.")


if __name__ == "__main__":
    main()
