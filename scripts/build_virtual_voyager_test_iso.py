"""Create an isolated test ISO with a verified Xbox asset package replacement."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

import psutil
from verify_beta_iso_components import locate


def entry_offset(stream, path):
    stream.seek(32 * 2048 + 20)
    sector, size = struct.unpack("<II", stream.read(8))
    parts = path.split("/")
    for index, part in enumerate(parts):
        directory_offset = sector * 2048
        stream.seek(directory_offset)
        directory = stream.read(size)
        at, seen = 0, set()
        while True:
            if at in seen or at + 14 > len(directory):
                raise ValueError("Invalid ISO directory")
            seen.add(at)
            left, right, sector, size, attr, count = struct.unpack_from("<HHIIBB", directory, at)
            name = directory[at + 14:at + 14 + count].decode("ascii").lower()
            if name == part.lower():
                if bool(attr & 16) != (index < len(parts) - 1):
                    raise ValueError("Wrong ISO entry type")
                break
            branch = left if part.lower() < name else right
            if not branch:
                raise ValueError("Missing ISO entry: " + path)
            at = branch * 4
    return directory_offset + at + 4


def build(source, output, package, default_xbe=None, mp_xbe=None):
    if bool(default_xbe) != bool(mp_xbe):
        raise ValueError("Provide both fresh SP and Holomatch executables")
    executables = {}
    if source.resolve() == output.resolve() or output.exists():
        raise ValueError("Output must be a new, separate test image")
    for process in psutil.process_iter(["name", "cmdline"]):
        if (process.info["name"] or "").lower().startswith("xemu"):
            command = " ".join(process.info["cmdline"] or []).lower()
            if source.name.lower() in command or output.name.lower() in command:
                raise ValueError("Source or destination image is mounted")
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_suffix(".partial.iso")
    if partial.exists():
        raise ValueError("Unresolved partial image exists")
    shutil.copyfile(source, partial)
    digest = hashlib.sha256()
    with partial.open("r+b") as stream:
        pointer = entry_offset(stream, "BaseEF/xbox0.pk3")
        stream.seek(pointer)
        original = stream.read(8)
        sector = (stream.seek(0, 2) + 2047) // 2048
        stream.seek(sector * 2048)
        size = 0
        with package.open("rb") as data:
            while block := data.read(1024 * 1024):
                stream.write(block)
                digest.update(block)
                size += len(block)
        stream.write(b"\0" * ((-size) % 2048))
        stream.seek(pointer)
        stream.write(struct.pack("<II", sector, size))
        stream.flush()
        offset, actual_size = locate(stream, "BaseEF/xbox0.pk3")
        stream.seek(offset)
        verified = hashlib.sha256()
        remaining = actual_size
        while remaining:
            block = stream.read(min(remaining, 1024 * 1024))
            if not block:
                raise ValueError("Truncated candidate package")
            verified.update(block)
            remaining -= len(block)
        if size != actual_size or digest.digest() != verified.digest():
            raise ValueError("Candidate asset identity mismatch")
    if default_xbe:
        with partial.open("r+b") as stream:
            for name, path in (("default.xbe", default_xbe), ("efmp.xbe", mp_xbe)):
                payload = path.read_bytes()
                if payload[:4] != b"XBEH":
                    raise ValueError("Invalid Xbox executable: " + str(path))
                pointer = entry_offset(stream, name)
                sector = (stream.seek(0, 2) + 2047) // 2048
                stream.seek(sector * 2048)
                stream.write(payload)
                stream.write(b"\0" * ((-len(payload)) % 2048))
                stream.seek(pointer)
                stream.write(struct.pack("<II", sector, len(payload)))
                stream.flush()
                offset, actual_size = locate(stream, name)
                stream.seek(offset)
                actual = stream.read(actual_size)
                if actual != payload:
                    raise ValueError("Executable identity mismatch: " + name)
                executables[name] = {"source": str(path.resolve()), "bytes": len(payload),
                    "sha256": hashlib.sha256(payload).hexdigest(), "verified": True}
    partial.rename(output)
    report = {"source": str(source.resolve()), "output": str(output.resolve()),
              "package": str(package.resolve()), "packageBytes": size,
              "packageSha256": digest.hexdigest(), "originalEntry": original.hex(),
              "replacementVerified": True, "sourceUnmodified": True, "executables": executables}
    output.with_suffix(".manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--default-xbe", type=Path)
    parser.add_argument("--mp-xbe", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.source, args.output, args.package, args.default_xbe, args.mp_xbe), indent=2))
