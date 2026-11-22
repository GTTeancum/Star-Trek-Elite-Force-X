"""Check shipped single-player lightmaps and world-surface addressing against the PC archives."""
import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import struct
from zipfile import ZipFile


def lump(bsp, index):
    start, size = struct.unpack_from("<II", bsp, 8 + index * 8)
    return memoryview(bsp)[start:start + size]


def verify(bsp, package, name):
    raw = lump(bsp, 14)
    atlas_pixels = 128 * 128
    count, remainder = divmod(len(raw), atlas_pixels * 3)
    if not count or remainder:
        raise ValueError(f"{name}: invalid source atlas size")

    # Check the index and vertex range the Xbox loader uses for each planar or curved world surface.
    surfaces = list(struct.iter_unpack("<8i72x", lump(bsp, 13)))
    checked = set()
    used_vertices = set()
    for kind, table, record in [(1, "faces", "<IHxI4x4xB9x"), (2, "patches", "<IHxI4xB17x")]:
        for index, shader, span, encoded in struct.iter_unpack(record, package.read(f"maps/{name}/{table}.mle")):
            material, _, actual_kind, first, verts, _, _, atlas = surfaces[index]
            if (shader, kind, span >> 12, span & 4095, encoded - 4) != (material, actual_kind, first, verts, atlas):
                raise ValueError(f"{name}: surface {index} points to a different material, vertex range or atlas")
            if index in checked or atlas >= count:
                raise ValueError(f"{name}: invalid surface {index} atlas {atlas}")
            checked.add(index)
            if atlas >= 0:
                used_vertices.update(range(first, first + verts))
    if len(checked) != sum(s[2] in (1, 2) for s in surfaces):
        raise ValueError(f"{name}: missing world surfaces")

    pc_uv = list(struct.iter_unpack("<20x2f16x", lump(bsp, 10)))
    gpu_uv = list(struct.iter_unpack("<2f60x", package.read(f"maps/{name}/verts.mle")))
    if len(pc_uv) != len(gpu_uv):
        raise ValueError(f"{name}: vertex count differs")
    # Non-lightmapped surfaces may contain uninitialized UVs in the original BSP.
    for i in used_vertices:
        s, t = pc_uv[i]
        u, v = gpu_uv[i]
        if (s, t) != (u / 65536, v / 65536):
            raise ValueError(f"{name}: lightmap coordinates differ at vertex {i}")

    data = package.read(f"maps/xbox/{name}.lmpdds")
    record_size = struct.unpack_from("<I", data)[0]
    if record_size != 128 + atlas_pixels * 2 or len(data) != 4 + count * record_size:
        raise ValueError(f"{name}: unexpected DDS count or size")
    for atlas in range(count):
        offset = 4 + atlas * record_size
        if (data[offset:offset + 4] != b"DDS " or struct.unpack_from("<II", data, offset + 12) != (128, 128)
                or struct.unpack_from("<5I", data, offset + 88) != (16, 0xF800, 0x07E0, 0x001F, 0)):
            raise ValueError(f"{name}: incompatible DDS header at atlas {atlas}")
        start = atlas * atlas_pixels * 3
        colors = struct.iter_unpack("BBB", raw[start:start + atlas_pixels * 3])
        words = struct.iter_unpack("<H", data[offset + 128:offset + record_size])
        for pixel, ((r, g, b), (actual,)) in enumerate(zip(colors, words)):
            expected = ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)
            if actual != expected:
                raise ValueError(f"{name} atlas {atlas} pixel {pixel}: BSP {(r, g, b)} "
                                 f"expects RGB565 {expected:#06x}, got {actual:#06x}")
    print(f"{name}: {count} atlases, {count * atlas_pixels} texels, "
          f"{len(checked)} surfaces, {len(used_vertices)} lightmapped UV pairs")
    return count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_ef", type=Path, help="Directory containing the original PAK*.PK3 files")
    parser.add_argument("xbox_pk3", type=Path)
    args = parser.parse_args()
    with ExitStack() as stack:
        sources = {}
        for path in sorted(args.base_ef.glob("[Pp][Aa][Kk][0-9].[Pp][Kk]3")):
            pak = stack.enter_context(ZipFile(path))
            for member in pak.namelist():
                if member.lower().endswith(".bsp"):
                    sources.setdefault(member.lower(), []).append((pak, member))
        package = stack.enter_context(ZipFile(args.xbox_pk3))
        maps = {path[10:-7] for path in package.namelist()
                if path.startswith("maps/xbox/") and path.endswith(".lmpdds")}
        manifest = json.loads(package.read("xbox_patch_manifest.json"))
        sizes = {row["map"]: row["originalBspBytes"] for row in manifest["bspOptimizations"]}
        if not maps or maps != set(sizes):
            raise ValueError("Lightmap inventory differs from the package manifest")
        total = 0
        for name in sorted(maps):
            # A retail update can contain a different BSP revision of the same name.
            revisions = {pak.read(member) for pak, member in sources[f"maps/{name}.bsp"]
                         if pak.getinfo(member).file_size == sizes[name]}
            if len(revisions) != 1:
                raise ValueError(f"{name}: could not identify the shipped BSP revision")
            total += verify(revisions.pop(), package, name)
        print(f"Passed {len(maps)} maps, {total} atlases, {total * 128 * 128:,} texels")
