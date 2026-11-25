#!/usr/bin/env python3
"""Run the packed/raw draw-surface gate, optionally against the shipped 0.9.4 map corpus."""
import argparse
from collections import Counter
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
RELEASE_XBOX0_SHA256 = "a7df7e2a2a606915830f0bd19ec9e80bc73746ce2ebc3e40e3c7d9c369d12657"
BSP_LAYOUTS = {
    "faces": "<IHbII4B4B3h",
    "patches": "<IHbI4B4B6h2B",
    "trisurfs": "<IHbII4B",
}
NODRAW = 0x80
BLACK = b"textures/common/black"


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


TEST_DEFS = r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
enum qboolean { qfalse, qtrue };
enum { SURF_NODRAW = 0x80 };
struct dshader_t { char shader[64]; int surfaceFlags, contentFlags; } material;
struct WorldData { char name[64]; int numShaders; dshader_t *shaders; } s_worldData;
#define Q_stricmp strcasecmp
#define Q_stricmpn strncasecmp
"""

TEST_MAIN = r"""
int main() {
    s_worldData.numShaders = 1;
    s_worldData.shaders = &material;
    assert(!R_EFShouldSkipRawDrawSurface(-1) && !R_EFShouldSkipRawDrawSurface(1));
    unsigned int flags, contents, previous, expected;
    int decisions = 0, restored = 0;
    while (scanf("%63s %63s %u %u %u %u", s_worldData.name, material.shader,
        &flags, &contents, &previous, &expected) == 6) {
        material.surfaceFlags = flags;
        material.contentFlags = contents;
        const unsigned actual = R_EFShouldSkipRawDrawSurface(0);
        if (actual != expected) {
            fprintf(stderr, "%s %s flags=0x%x expected skip=%u got=%u\n",
                s_worldData.name, material.shader, flags, expected, actual);
            return 1;
        }
        ++decisions;
        if (previous && !actual) ++restored;
    }
    printf("Checked %d distinct map/material cases; restored %d previously skipped material cases\n",
        decisions, restored);
    return 0;
}
"""


def corpus(path):
    with path.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != RELEASE_XBOX0_SHA256:
            raise ValueError("--v094-xbox0 requires the checksum-matched public 0.9.4 xbox0.pk3")
    with zipfile.ZipFile(path) as pack:
        maps = sorted(n[:-len("/shaders.mle")] for n in pack.namelist()
                      if n.startswith("maps/") and n.endswith("/shaders.mle"))
        assert len(maps) == len(set(maps)) == 66
        black_faces, restored = Counter(), Counter()
        cases, examined = [], 0
        for name in maps:
            materials = [(n.split(b"\0", 1)[0], flags, contents) for n, flags, contents in
                         struct.iter_unpack("<64sII", pack.read(f"{name}/shaders.mle"))]
            used = Counter()
            for kind, layout in BSP_LAYOUTS.items():
                for record in struct.iter_unpack(layout, pack.read(f"{name}/{kind}.mle")):
                    used[record[1]] += 1
            for material, count in used.items():
                shader, flags, contents = materials[material]
                # Same BSP visibility declaration as Raven's ordinary loader.
                expected = bool(flags & NODRAW) or shader.lower() == b"noshader"
                was_black_exception = shader.lower() == BLACK and name.startswith("maps/borg")
                if shader.lower() == BLACK:
                    black_faces[name] += count
                    assert contents & 1 and not expected
                if was_black_exception:
                    restored[name] += count
                assert shader and not any(c <= 32 for c in shader), shader
                cases.append((name + ".bsp", shader.decode("ascii"), flags, contents,
                              int(expected or was_black_exception), int(expected)))
                examined += count
        assert black_faces == {"maps/borg1": 8, "maps/borg2": 3, "maps/borg3": 4,
                              "maps/borg6": 2, "maps/forge2": 1, "maps/forge4": 1,
                              "maps/scav3": 12}, black_faces
        assert sum(restored.values()) == 17, restored
        print(f"Audited {examined} render-surface entries, {len(maps)} shipped maps, "
              f"{sum(black_faces.values())} opaque black faces, {sum(restored.values())} restored.", flush=True)
        return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.getenv("CXX", "c++"))
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--v094-xbox0", type=Path, help="optional unmodified release xbox0.pk3")
    args = parser.parse_args()
    source = (ROOT / "code/renderer/tr_bsp_xbox.cpp").read_text()
    loader = (ROOT / "code/qcommon/cm_load_xbox.cpp").read_text()
    # Protect the gate used by the real packed map entry as well as the raw adapter.
    assert "R_LoadFacesStream(streamIndexes" in loader and "R_EFLoadRawDrawSurfacesFromBSP(name" in loader
    assert "R_LoadFacesInternal(" in function(source, "void R_LoadFacesStream(")
    assert "ParseFace(parseSurface" in function(source, "static void R_LoadFacesInternal(")
    raw = function(source, "void R_EFLoadRawDrawSurfacesFromBSP(")
    assert "ParseFace(&face" in raw
    for sig in ["static void ParseFace(", "static void ParseMesh (", "static void ParseTriSurf("]:
        body = function(source, sig)
        assert body.index("R_EFShouldSkipRawDrawSurface( ds->shaderNum )") < body.index("R_EFSkipRawDrawSurface(")
    test = function(source, "static qboolean R_EFShouldSkipRawDrawSurface(")
    # Include the original helper if present, so the control fails from the old
    # behavior instead of a missing test-stub symbol when back-tested.
    if "static qboolean R_EFShouldSkipBorgBlackBackingSurface(" in source:
        test = function(source, "static qboolean R_EFShouldSkipBorgBlackBackingSurface(") + "\n" + test
    cases = [
        ("maps/borg2.bsp", "textures/common/black", 0x430, 1, 1, 0),
        ("maps/borg3.bsp", "Textures/Common/Black", 0x430, 1, 1, 0),
        ("maps/borg_custom.bsp", "textures/common/black", 0x430, 1, 1, 0),
        ("maps/forge4.bsp", "textures/common/black", 0x430, 1, 0, 0),
        ("maps/borg6.bsp", "textures/common/black", NODRAW, 1, 1, 1),
        ("maps/borg6.bsp", "textures/borg/static2", 0x40420, 1, 0, 0),
        ("maps/test.bsp", "noshader", 0, 0, 1, 1),
        ("maps/test.bsp", "NoShader", 0, 0, 1, 1),
        ("maps/test.bsp", "textures/test/clip", NODRAW, 1, 1, 1),
    ]
    if args.v094_xbox0:
        cases.extend(corpus(args.v094_xbox0))
    flags = ["-std=c++98", "-Wall", "-Wextra", "-Werror"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
    with tempfile.TemporaryDirectory() as tmp:
        cpp, exe = Path(tmp) / "test.cpp", Path(tmp) / "test"
        cpp.write_text("\n".join((TEST_DEFS, test, TEST_MAIN)))
        subprocess.run([args.cxx, *flags, str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], input="\n".join(" ".join(map(str, row)) for row in cases) + "\n",
                       text=True, check=True)
    print("PASS: draw authored black color/depth surfaces; still skip NODRAW and noshader")


if __name__ == "__main__":
    main()
