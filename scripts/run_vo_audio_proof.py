"""Temporarily boot a built XBE in the existing ISO, capture audio, and restore it."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import psutil


def root_entry(iso, name):
    iso.seek(32 * 2048)
    header = iso.read(2048)
    if header[:20] != b"MICROSOFT*XBOX*MEDIA":
        raise ValueError("Not an XDVDFS image")
    sector, size = struct.unpack_from("<II", header, 20)
    iso.seek(sector * 2048)
    directory = iso.read(size)
    todo, seen = [0], set()
    while todo:
        at = todo.pop()
        if at in seen or at + 14 > size:
            raise ValueError("Invalid root directory")
        seen.add(at)
        left, right, file_sector, file_size, attr, count = struct.unpack_from("<HHIIBB", directory, at)
        entry_name = directory[at+14:at+14+count].decode("ascii")
        if entry_name.lower() == name.lower():
            return sector * 2048 + at + 4
        if left: todo.append(left * 4)
        if right: todo.append(right * 4)
    raise ValueError("Root executable entry not found")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ["iso", "xbe", "exe", "config", "output"]:
        p.add_argument("--"+key, type=Path, required=True)
    p.add_argument("--seconds", type=int, default=220)
    p.add_argument("--map", type=Path)
    a = p.parse_args()
    for process in psutil.process_iter(["name", "cmdline"]):
        if (process.info["name"] or "").lower() == "xemu.exe":
            if any(a.iso.name.lower() in arg.lower() for arg in (process.info["cmdline"] or [])):
                raise RuntimeError("Test ISO is mounted by another emulator")
    data = a.xbe.read_bytes()
    cleanup = ["pwsh", "-NoProfile", "-File", str(Path(__file__).with_name("cleanup_generated.ps1"))]
    subprocess.run(cleanup, check=True)
    with a.iso.open("r+b") as f:
        entry = root_entry(f, "default.xbe")
        f.seek(entry)
        original = f.read(8)
        f.seek(0, 2)
        length = f.tell()
        appended = (length + 2047) & ~2047
        manifest = {"xbe": str(a.xbe.resolve()), "xbe_sha256": hashlib.sha256(data).hexdigest(),
                    "bytes": len(data), "iso_entry": entry, "original_entry_hex": original.hex(),
                    "original_iso_bytes": length, "appended_sector": appended // 2048,
                    "iso_restored": False}
        record = a.output.with_suffix(".variant.json")
        record.write_text(json.dumps(manifest, indent=2))
    try:
        with a.iso.open("r+b") as f:
            f.seek(appended)
            f.write(data)
            f.seek(entry)
            f.write(struct.pack("<II", appended // 2048, len(data)))
        # Close the writer before XEMU opens the network-backed DVD image.
        subprocess.run([sys.executable, str(Path(__file__).with_name("capture_xemu_vo.py")),
            "--exe", str(a.exe), "--iso", str(a.iso), "--config", str(a.config),
            "--output", str(a.output), "--seconds", str(a.seconds), "--extract-log"] +
            (["--map", str(a.map)] if a.map else []), check=True)
    finally:
        with a.iso.open("r+b") as f:
            f.seek(entry)
            f.write(original)
            f.truncate(length)
            f.flush()
            f.seek(entry)
            assert f.read(8) == original
        manifest["iso_restored"] = True
        record.write_text(json.dumps(manifest, indent=2))
        subprocess.run(cleanup, check=True)



if __name__ == "__main__":
    main()
