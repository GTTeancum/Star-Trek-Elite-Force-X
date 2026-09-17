"""Audit all staged Virtual Voyager BSP records inside the final Xbox package."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

import audit_packed_bsp as audit
import build_xbox_patch_pk3 as builder


def verify(base: Path, package: Path) -> dict:
    inventory = json.loads((base / "xbox_virtual_voyager_sources.json").read_text())
    records = []
    with zipfile.ZipFile(package) as pak:
        names = pak.namelist()
        if len(set(names)) != len(names):
            raise ValueError("Duplicate package entries")
        for name, source in inventory["maps"].items():
            path = base / "maps" / (name + ".bsp")
            raw = path.read_bytes()
            if hashlib.sha256(raw).hexdigest() != source["sha256"]:
                raise ValueError(f"Source changed after inventory: {name}")
            lumps = {key: pak.read(f"maps/{name}/{key}.mle")
                     for key in builder.PACKED_BSP_LUMP_NAMES}

            class Packed:
                parse_bsp_lumps = staticmethod(builder.parse_bsp_lumps)

                @staticmethod
                def packed_bsp_lumps(repo, bsp):
                    return lumps, {}

            audit.validate_map(Packed, Path(__file__).resolve().parents[1], path)
            source_lumps = builder.parse_bsp_lumps(raw, path)
            offset, size = source_lumps[0]
            if lumps["entities"].rstrip(b"\0") != raw[offset:offset + size].rstrip(b"\0"):
                raise ValueError(f"Authored entities/scripts changed: {name}")
            lightmaps = pak.read(f"maps/xbox/{name}.lmpdds")
            record_size = struct.unpack_from("<I", lightmaps)[0]
            if record_size != builder.LIGHTMAP_RGB565_DDS_BYTES or (len(lightmaps) - 4) % record_size:
                raise ValueError(f"Invalid lightmap record layout: {name}")
            count = (len(lightmaps) - 4) // record_size
            for index in range(count):
                start = 4 + index * record_size
                if lightmaps[start:start + 4] != b"DDS ":
                    raise ValueError(f"Invalid DDS lightmap: {name}:{index}")
            expected = source_lumps[builder.EF_LUMP_LIGHTMAPS][1] // builder.LIGHTMAP_RGB_BYTES
            if count != expected:
                raise ValueError(f"Lightmap count mismatch: {name}: {count} != {expected}")
            packed_bytes = sum(map(len, lumps.values())) + len(lightmaps)
            records.append({"map": name, "sourceBytes": len(raw), "packedBytes": packed_bytes,
                            "lightmaps": count, "authoredEntitiesPreserved": True,
                            "packedRecordAudit": "passed"})
    return {"package": str(package.resolve()), "mapCount": len(records), "maps": records,
            "sourceBytes": sum(r["sourceBytes"] for r in records),
            "packedBytes": sum(r["packedBytes"] for r in records)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-dir", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = verify(args.base_dir, args.package)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "maps"}, indent=2))
