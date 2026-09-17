"""Stage licensed Virtual Voyager sources for the existing Xbox asset packer.

Only artwork/shaders and tour/holodeck source maps are materialized. Original
PAKs remain authoritative for scripts, models, sound and all other runtime data.
Never extract configs or VM binaries over the Xbox build's local overlays.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

IMAGE_SUFFIXES = {".tga", ".jpg", ".jpeg", ".png"}


def virtual_map(name: str) -> bool:
    return name.startswith("maps/tour/") or name.startswith("maps/_holodeck_") or name == "maps/_brig.bsp"


def extract(base: Path, archives: Path) -> dict:
    maps = {}
    files = {}
    for number in range(4):
        source = archives / f"PAK{number}.PK3"
        with zipfile.ZipFile(source) as pak:
            for entry in pak.infolist():
                name = entry.filename.replace("\\", "/").lower()
                path = PurePosixPath(name)
                if entry.is_dir():
                    continue
                if path.is_absolute() or ".." in path.parts or ":" in name:
                    raise ValueError(f"Unsafe archive path: {entry.filename}")
                if not (path.suffix in IMAGE_SUFFIXES or path.suffix == ".shader" or
                        (virtual_map(name) and path.suffix in {".bsp", ".nav"})):
                    continue
                data = pak.read(entry)
                target = base.joinpath(*path.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                if not target.exists() or target.read_bytes() != data:
                    target.write_bytes(data)
                files[name] = source.name
                if path.suffix == ".bsp":
                    maps[str(path.with_suffix(""))[5:]] = {
                        "archive": source.name, "bytes": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(),
                    }
    report = {"archives": str(archives.resolve()), "maps": dict(sorted(maps.items())),
              "materializedFiles": len(files), "startMap": "tour/deck02"}
    (base / "xbox_virtual_voyager_sources.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-dir", type=Path, required=True)
    parser.add_argument("--archive-dir", type=Path)
    args = parser.parse_args()
    print(json.dumps(extract(args.base_dir, args.archive_dir or args.base_dir), indent=2))
