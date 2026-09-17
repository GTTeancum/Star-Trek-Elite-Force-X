"""Generate the local DSP include from the user's installed, licensed XDK 5558.

No SDK image bytes belong in source control. Existing matching headers are left
untouched so incremental builds retain their timestamps. Audio data is unchanged.
"""
import argparse
import hashlib
from pathlib import Path
import re

EXPECTED_SHA256 = "a9c372442ccb1af72e50792161b476654e48dad050e041e7b176f37f2caba8d6"


def generate(image: Path, output: Path) -> None:
    data = image.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise ValueError("Expected the unchanged XDK 5558 dsstdfx.bin; refusing another DSP layout")
    if output.exists():
        current = output.read_text()
        match = re.search(r"s_stefxDspEffectsImage\[24936\]\s*=\s*\{(.*?)\};", current, re.S)
        if match and bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", match[1])) == data:
            print("DSP header matches installed XDK 5558 image; unchanged")
            return
    lines = ["#pragma once", "", "// Generated from the installed XDK 5558 dsstdfx.bin. Do not commit.",
             "// Preserve the 5558 reverb/crosstalk node layout used by snd_fx_img.h.",
             "static unsigned char s_stefxDspEffectsImage[%d] = {" % len(data)]
    lines += ["\t" + ",".join("0x%02x" % x for x in data[i:i+16]) + "," for i in range(0, len(data), 16)]
    lines += ["};", ""]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="ascii")
    print("Generated local DSP header from verified XDK 5558 image")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xdk", type=Path, default=Path("C:/XDK_5558/XDK"))
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "code/win32/snd_dsp_image.h")
    args = parser.parse_args()
    generate(args.xdk / "Source/DSound/dsp/dsstdfx.bin", args.output)
