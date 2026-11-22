"""Compile the retail split-screen face test and compare it with individual vertices."""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/split-face-tests"
VC = Path("C:/Program Files (x86)/Microsoft Visual Studio 8/VC")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    renderer = ROOT / "code/renderer"
    source = (renderer / "retail_xbox/tr_world_retail.cpp").read_text(encoding="latin1")
    start = source.index("struct splitFaceBounds_t")
    end = source.index("\n#endif", start)
    layout = (renderer / "tr_local.h").read_text(encoding="latin1")
    # The first definitions are the Xbox compressed-vertex layout used by the BSP loader.
    definitions = [re.search(r"^#define\s+" + name + r"\b[^\n]*", layout, re.M)[0]
                   for name in ("VERTEX_LM", "NEXT_SURFPOINT")]
    (OUT / "split_face.inc").write_text("\n".join([*definitions, source[start:end]]),
                                        encoding="latin1")
    env = dict(os.environ, TEMP=str(OUT), TMP=str(OUT))
    env["PATH"] = os.pathsep.join(map(str, [VC / "bin", VC.parent / "Common7/IDE", env["PATH"]]))
    exe = OUT / "split_face.exe"
    subprocess.run([
        VC / "bin/cl.exe", "/nologo", "/EHsc", "/MT", "/O2",
        "/I" + str(VC / "include"), "/I" + str(OUT),
        ROOT / "scripts/tests/fixtures/split_face_frustum.cpp",
        "/Fo" + str(exe.with_suffix(".obj")), "/Fe" + str(exe),
        "/link", "/LIBPATH:" + str(VC / "lib"), "/LIBPATH:" + str(VC / "PlatformSDK/Lib"),
    ], cwd=OUT, env=env, check=True)
    subprocess.run([exe], cwd=OUT, env=env, check=True)


if __name__ == "__main__":
    main()
