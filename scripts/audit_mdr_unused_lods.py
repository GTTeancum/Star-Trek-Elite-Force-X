"""Read-only inventory of MDR lower-LOD storage; never rewrites assets."""
import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import struct
import zipfile


def layout(data):
    if len(data) < 104 or data[:4] != b'RDM5' or struct.unpack_from('<i', data, 4)[0] != 2:
        raise ValueError('Unsupported MDR header')
    frames, bones, frame_at, lods, lod_at, tags, tag_at, end = struct.unpack_from('<8i', data, 72)
    if not (frames > 0 and 0 < bones <= 128 and 0 < lods <= 16 and tags >= 0
            and 104 <= abs(frame_at) < lod_at <= tag_at <= end == len(data)):
        raise ValueError('Invalid MDR ranges')
    frame_size = (40 + bones * 24) if frame_at < 0 else (56 + bones * 48)
    if abs(frame_at) + frames * frame_size != lod_at or tag_at + tags * 36 != end:
        raise ValueError('Noncanonical frame or tag tail; needs separate review')
    sizes, cursor = [], lod_at
    for _ in range(lods):
        if cursor + 12 > tag_at:
            raise ValueError('LOD header outside geometry')
        surfaces, surface_at, length = struct.unpack_from('<3i', data, cursor)
        if not (0 < surfaces <= 1024 and 12 <= surface_at < length and cursor + length <= tag_at):
            raise ValueError('Invalid LOD layout')
        sizes.append(length)
        cursor += length
    if cursor != tag_at:
        raise ValueError('Unrecognized data between LODs and tags')
    return {'file_bytes': len(data), 'lods': lods, 'lod_bytes': sizes,
            'unused_lower_lod_bytes': sum(sizes[1:]), 'tag_bytes': tags * 36,
            'frame_bytes': frames * frame_size}


def keep_lod0(data):
    """Return a candidate in memory; frame data, LOD0 and tags stay exact."""
    info = layout(data)
    if info['lods'] == 1:
        return data
    lod_at = struct.unpack_from('<i', data, 88)[0]
    tag_at = struct.unpack_from('<i', data, 96)[0]
    new_tag_at = lod_at + info['lod_bytes'][0]
    result = bytearray(data[:new_tag_at] + data[tag_at:])
    struct.pack_into('<i', result, 84, 1)
    struct.pack_into('<ii', result, 96, new_tag_at, len(result))
    return bytes(result)


def verify_lod0_candidate(original, candidate):
    before, after = layout(original), layout(candidate)
    old_tags = struct.unpack_from('<i', original, 96)[0]
    new_tags = struct.unpack_from('<i', candidate, 96)[0]
    # Only the LOD count, tag offset and file length may change in the header.
    assert original[:84] == candidate[:84] and original[88:96] == candidate[88:96]
    assert after['lods'] == 1 and after['lod_bytes'][0] == before['lod_bytes'][0]
    assert candidate[104:new_tags] == original[104:new_tags]
    assert candidate[new_tags:] == original[old_tags:]
    assert len(original) - len(candidate) == before['unused_lower_lod_bytes']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, default=Path('build/release/BaseEF'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify-lod0-transform', action='store_true',
                        help='Verify in-memory candidate bytes; no package or model files are written.')
    args = parser.parse_args()
    sources, models, rejected = {}, {}, {}
    with ExitStack() as stack:
        for package in ['PAK0.PK3', 'PAK1.PK3', 'PAK2.PK3', 'PAK3.PK3', 'xbox0.pk3']:
            archive = stack.enter_context(zipfile.ZipFile(args.base/package))
            for entry in archive.infolist():
                if entry.filename.lower().endswith('.mdr'):
                    sources[entry.filename.lower()] = (package, archive, entry)
        for name, (package, archive, entry) in sorted(sources.items()):
            data = archive.read(entry)
            try:
                result = layout(data)
                if args.verify_lod0_transform:
                    verify_lod0_candidate(data, keep_lod0(data))
                    result['lod0_transform_verified'] = True
            except ValueError as error:
                rejected[name] = str(error)
                continue
            models[name] = dict(result, package=package, sha256=hashlib.sha256(data).hexdigest())
    report = {'scope': 'SP effective package order; total is across assets, not simultaneous residency',
              'models': models, 'rejected': rejected,
              'unused_lower_lod_bytes': sum(item['unused_lower_lod_bytes'] for item in models.values())}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'Models={len(models)}, rejected={len(rejected)}, lower-LOD bytes={report["unused_lower_lod_bytes"]}')


if __name__ == '__main__':
    main()
