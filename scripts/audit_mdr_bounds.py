"""Read-only audit of authored MDR bounds against every skinned frame and LOD.

Uses the active MD4 structures and MC_UnCompress matrix representation.
Does not change assets or decide whether a rendering change is qualified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import numpy as np


def decode_matrices(raw):
    raw = raw.astype(np.float32) - np.float32(32768)
    bones = np.empty(raw.shape[:-1] + (3, 4), np.float32)
    bones[..., :, 3] = raw[..., :3] * np.float32(1 / 64)
    bones[..., :, :3] = (raw[..., 3:] * np.float32(1 / 32766)).reshape(raw.shape[:-1] + (3, 3))
    return bones


def audit(data):
    assert data[:4] == b'RDM5' and struct.unpack_from('<i', data, 4)[0] == 2
    nf, nb, frames_at, nl, lod_at, _, _, end = struct.unpack_from('<8i', data, 72)
    assert 0 < nf and 0 < nb <= 128 and 0 < nl and end == len(data)
    compressed = frames_at < 0
    frames_at = abs(frames_at)
    prefix, stride = (40, 24) if compressed else (56, 48)
    frame_size = prefix + nb * stride
    bounds = np.ndarray((nf, 10), '<f4', data, frames_at,
                        strides=(frame_size, 4)).copy()
    if compressed:
        raw = np.ndarray((nf, nb, 12), '<u2', data, frames_at + prefix,
                         strides=(frame_size, stride, 2))
        bones = decode_matrices(raw)
    else:
        bones = np.ndarray((nf, nb, 3, 4), '<f4', data, frames_at + prefix,
                           strides=(frame_size, 48, 16, 4)).copy()
    assert np.isfinite(bounds).all() and np.isfinite(bones).all()
    box_excess = np.full(nf, -np.inf)
    sphere_excess = np.full(nf, -np.inf)
    vertices = 0
    for _ in range(nl):
        ns, surface_offset, lod_end = struct.unpack_from('<3i', data, lod_at)
        at = lod_at + surface_offset
        for _ in range(ns):
            nv, vo, _, _, _, _, surface_end = struct.unpack_from('<7i', data, at + 140)
            cursor = at + vo
            vertex_weights = []
            for _ in range(nv):
                nw = struct.unpack_from('<i', data, cursor + 20)[0]
                assert 0 < nw <= nb
                weights = [struct.unpack_from('<if3f', data, cursor + 24 + 20*j)
                           for j in range(nw)]
                assert all(0 <= w[0] < nb for w in weights)
                vertex_weights.append(weights)
                cursor += 24 + nw * 20
            width = max(map(len, vertex_weights))
            ids = np.zeros((nv, width), np.int32)
            weights = np.zeros((nv, width), np.float32)
            offsets = np.zeros((nv, width, 3), np.float32)
            for v, items in enumerate(vertex_weights):
                for j, item in enumerate(items):
                    ids[v, j], weights[v, j] = item[:2]
                    offsets[v, j] = item[2:]
            for first in range(0, nf, 32):
                last = min(first + 32, nf)
                xyz = np.zeros((last-first, nv, 3), np.float32)
                for j in range(width):
                    m = bones[first:last, ids[:, j]]
                    # Match DotProduct and the weight accumulation order.
                    transformed = ((m[:, :, :, 0] * offsets[:, j, 0, None]
                        + m[:, :, :, 1] * offsets[:, j, 1, None])
                        + m[:, :, :, 2] * offsets[:, j, 2, None]) + m[:, :, :, 3]
                    xyz += transformed * weights[:, j, None]
                low, high = xyz.min(axis=1), xyz.max(axis=1)
                b = bounds[first:last]
                excess = np.maximum((b[:, :3] - low).max(axis=1),
                                    (high - b[:, 3:6]).max(axis=1))
                radius = np.sqrt(((xyz - b[:, None, 6:9])**2).sum(axis=2)).max(axis=1)
                box_excess[first:last] = np.maximum(box_excess[first:last], excess)
                sphere_excess[first:last] = np.maximum(sphere_excess[first:last], radius - b[:, 9])
            vertices += nv
            at += surface_end
        lod_at += lod_end
    return dict(frames=nf, vertices_across_lods=vertices, compressed=compressed,
                max_box_excess=float(box_excess.max()),
                max_sphere_excess=float(sphere_excess.max()),
                box_bad_frames=int((box_excess > 0.05).sum()),
                sphere_bad_frames=int((sphere_excess > 0.05).sum()))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', type=Path, default=Path('build/release/BaseEF'))
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--limit', type=int)
    args = p.parse_args()
    sources = {}
    for package in ['PAK0.PK3', 'PAK1.PK3', 'PAK2.PK3', 'PAK3.PK3', 'xbox0.pk3']:
        with zipfile.ZipFile(args.base/package) as z:
            for entry in z.infolist():
                if entry.filename.lower().endswith('.mdr'):
                    sources[entry.filename.lower()] = (package, entry.filename)
    results = {}
    cache = {}
    for name, (package, entry) in list(sorted(sources.items()))[:args.limit]:
        with zipfile.ZipFile(args.base/package) as z:
            data = z.read(entry)
        digest = hashlib.sha256(data).hexdigest()
        if digest not in cache:
            cache[digest] = audit(data)
        results[name] = dict(cache[digest], package=package, sha256=digest)
        print(name, results[name]['box_bad_frames'], results[name]['sphere_bad_frames'], flush=True)
        args.output.write_text(json.dumps({'tolerance': 0.05, 'models': results}, indent=2))


if __name__ == '__main__':
    main()
