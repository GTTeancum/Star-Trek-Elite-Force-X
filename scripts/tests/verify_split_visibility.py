"""Compare the Xbox split-screen visibility path against a full BSP walk.

With --maps, also exercise the Xbox BSPs in the local release packages.
All intermediate files are generated inside the repository.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/split-pvs/tests"
RENDERER = ROOT / "code/renderer/retail_xbox"
VC = Path("C:/Program Files (x86)/Microsoft Visual Studio 8/VC")


def sampled_views(trace, players, nodes, leaves, planes):
    def cluster_at(origin):
        index = 0
        while index < len(nodes):
            plane, front, back = nodes[index]
            x, y, z, dist = planes[plane]
            child = front if x*origin[0] + y*origin[1] + z*origin[2] > dist else back
            index = child if child >= 0 else len(nodes) - 1 - child
        return leaves[index - len(nodes)][0]

    views = []
    previous = None
    with trace.open() as source:
        for line in source:
            work = json.loads(line).get("_g_SPXBConsoleWorkload", [])
            if len(work) < 76 or work[0] == previous or work[0] & 1:
                continue
            if work[4] != players or min(work[5:7]) < players or (players == 4 and work[7] != 4):
                continue
            if not all(work[25 + 16*i] for i in range(players)):
                continue
            previous = work[0]
            origins = [struct.unpack("<3f", struct.pack("<3I", *work[14+16*i:17+16*i])) for i in range(players)]
            views.append([cluster_at(origin) for origin in origins])
    assert views
    return views


def export_map(name, players, trace):
    package = "xbox1.pk3" if players == 4 else "xbox0.pk3"
    with zipfile.ZipFile(ROOT / "build/release/BaseEF" / package) as archive:
        def lump(kind):
            return archive.read(f"maps/{name}/{kind}.mle")

        nodes = list(struct.iter_unpack("<ihh12x", lump("nodes")))
        leaves = list(struct.iter_unpack("<hb20x", lump("leafs")))
        vis = lump("visibility")
        planes = list(struct.iter_unpack("<4f", lump("planes"))) if trace else []

    clusters, stride, width, compressed, removed, length, entries = struct.unpack_from("<IIBBBII", vis)
    assert length == clusters * stride
    data = vis[19:]
    if compressed:
        assert width in (1, 2, 3, 4)
        if len(data) == entries * (width + 1) - 1:
            # The package writer can omit the high byte of the last offset.
            # Recover it only when the sorted offsets and row bounds identify
            # exactly one possible value.
            previous = int.from_bytes(data[-2 * width:-width], "little")
            low = int.from_bytes(data[-(width - 1):], "little") if width > 1 else 0
            high = [n for n in range(256) if previous < low + (n << (8 * (width - 1))) < length]
            assert len(high) == 1
            data += bytes(high)
        assert len(data) == entries * (width + 1)
        rows = bytearray([removed]) * length
        for pos in range(0, len(data), width + 1):
            offset = int.from_bytes(data[pos + 1:pos + width + 1], "little")
            rows[offset] = data[pos]
    else:
        # Some packages omit the last padding byte after the last addressable
        # cluster. Every bit consulted by R_MarkLeaves must exist in the BSP.
        assert (clusters - 1) * stride + (clusters + 7) // 8 <= len(data) <= length
        rows = data.ljust(length, b"\0")

    parents = [-1] * (len(nodes) + len(leaves))
    # Match R_SetParent's depth-first visitation, including shared solid leaves.
    stack = [(0, -1)]
    while stack:
        index, parent = stack.pop()
        parents[index] = parent
        if index < len(nodes):
            for child in reversed(nodes[index][1:]):
                child_index = child if child >= 0 else len(nodes) - 1 - child
                stack.append((child_index, index))

    # Spread the simulated cameras across distinct populated map clusters.
    available = sorted({cluster for cluster, _ in leaves if 0 <= cluster < clusters})
    cameras = [available[i * (len(available) - 1) // (players - 1)] for i in range(players)]
    path = OUT / (name + ".bin")
    with path.open("wb") as output:
        output.write(struct.pack("<4i", len(nodes), len(leaves), clusters, stride))
        output.write(struct.pack(f"<{len(nodes)}i", *parents[:len(nodes)]))
        for parent, (cluster, area) in zip(parents[len(nodes):], leaves):
            output.write(struct.pack("<3i", parent, cluster, area))
        output.write(rows)
        output.write(struct.pack(f"<{players}i", *cameras))
        # One view per player per distinct sparse server sample, with open areas.
        # The flight records player origins. It does not record per-frame cameras.
        views = sampled_views(trace, players, nodes, leaves, planes) if trace else []
        output.write(struct.pack("<i", len(views)))
        for view in views:
            output.write(struct.pack(f"<{players}i", *view))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--maps", action="store_true")
    parser.add_argument("--flights", action="store_true",
                        help="also replay the locally archived Nov 16 retail player-origin samples")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    world = (RENDERER / "tr_world_retail.cpp").read_text(encoding="latin1")
    start = world.index("void R_MarkLeaves (mleaf_s *leafOverride)")
    end = world.index("#else // _XBOX", start)
    mark = world[start:end]
    # Count the individual BSP leaves actually inspected by the production loop.
    leaf_test = "cluster = leaf->cluster;"
    assert mark.count(leaf_test) == 2
    pos = mark.rindex(leaf_test)
    mark = mark[:pos] + "++leafTests; " + mark[pos:]
    (OUT / "mark_leaves.inc").write_text(mark, encoding="latin1")
    env = dict(os.environ, TEMP=str(OUT), TMP=str(OUT))
    env["PATH"] = os.pathsep.join(map(str, [VC/"bin", VC.parent/"Common7/IDE", env["PATH"]]))
    for players, names in [(2, ["borg1", "scav2"]), (4, ["hm_borg1", "hm_voy1", "hm_scav1"])]:
        exe = OUT / f"split_visibility_{players}.exe"
        flags = ["/DSTEFX_SP_HOSTED_MP"] if players == 4 else []
        subprocess.run([
            VC/"bin/cl.exe", "/nologo", "/EHsc", "/MT", "/O2", "/DSTEFX_HW_FRAME_DIAGNOSTICS",
            *flags, "/I" + str(VC/"include"), "/I" + str(OUT), "/I" + str(RENDERER),
            ROOT/"scripts/tests/fixtures/split_visibility.cpp",
            "/Fo" + str(exe.with_suffix(".obj")), "/Fe" + str(exe),
            "/link", "/LIBPATH:" + str(VC/"lib"), "/LIBPATH:" + str(VC/"PlatformSDK/Lib"),
        ], cwd=OUT, env=env, check=True)
        maps = []
        if args.maps or args.flights:
            for name in names:
                run = name + ("_02" if name == "borg1" else "_01")
                trace = ROOT / "build/coop_split_20261116/runs" / ("production_" + run) / "flight.jsonl"
                maps.append(export_map(name, players, trace if args.flights else None))
        subprocess.run([exe, *maps], cwd=OUT, env=env, check=True)


if __name__ == "__main__":
    main()
