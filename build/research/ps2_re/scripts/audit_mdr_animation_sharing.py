"""Find byte-identical player animation regions; no runtime savings assumption."""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

p = argparse.ArgumentParser(__doc__)
p.add_argument('assets', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
sources = {}
# Match the known package layering used by the preceding package inventories.
for package in ('PAK0.PK3', 'PAK1.PK3', 'PAK2.PK3', 'PAK3.PK3', 'xbox0.pk3'):
    path = a.assets / package
    with zipfile.ZipFile(path) as z:
        for name in z.namelist():
            if name.lower().startswith('models/players/') and name.lower().endswith('.mdr'):
                sources[name.lower()] = (path, name)
groups, rejected, block_reuse = {}, [], []
for name, (package, entry) in sorted(sources.items()):
    with zipfile.ZipFile(package) as z:
        data = z.read(entry)
    if len(data) < 104 or data[:4] != b'RDM5':
        rejected.append(name)
        continue
    frames, bones, offset, lods, lod_offset, tags, tag_offset, end = struct.unpack_from('<8i', data, 72)
    frame_bytes = 40 + bones * 24
    if not (frames > 0 and bones > 0 and offset == -104 and
            lod_offset == 104 + frames * frame_bytes and
            104 <= lod_offset <= tag_offset <= end == len(data)):
        rejected.append(name)
        continue
    animation = data[104:lod_offset]
    blocks = {}
    duplicates = 0
    duplicate_bytes = 0
    for at in range(0, len(animation), 16 * frame_bytes):
        payload = animation[at:at + 16 * frame_bytes]
        block_key = hashlib.sha256(payload).digest()
        if block_key in blocks:
            duplicates += 1
            duplicate_bytes += len(payload)
        else:
            blocks[block_key] = len(payload)
    block_reuse.append({'model': name, 'frames': frames, 'bones': bones,
                        'blocks': (frames + 15) // 16, 'duplicate_blocks': duplicates,
                        'duplicate_source_bytes': duplicate_bytes})
    digest = hashlib.sha256(animation).hexdigest()
    key = (digest, frames, bones)
    groups.setdefault(key, []).append({'model': name, 'package': package.name,
                                      'source_bytes': len(data), 'animation_bytes': len(animation)})
shared = [{'sha256': key[0], 'frames': key[1], 'bones': key[2], 'models': rows}
          for key, rows in groups.items() if len(rows) > 1]
shared.sort(key=lambda g: sum(r['animation_bytes'] for r in g['models']), reverse=True)
result = {'candidate_model_count': len(sources), 'validated_model_count': sum(map(len, groups.values())),
          'shared_animation_groups': shared, 'rejected_layouts': rejected,
          'within_model_block_reuse': sorted(block_reuse, key=lambda r: r['duplicate_source_bytes'], reverse=True),
          'limits': 'Package union, not simultaneous runtime residency. Equal SHA256 identifies candidates '
                    'for byte comparison before implementation. Animation regions are source bytes; '
                    'the Xbox already block-compresses eligible models, so these are not RAM savings. '
                    'Borg models already use a separate sharing implementation.'}
a.output.write_text(json.dumps(result, indent=2))
print(json.dumps({k: v for k, v in result.items() if k not in ('shared_animation_groups', 'rejected_layouts', 'within_model_block_reuse')}))
print('shared groups', len(shared))
for group in shared[:8]:
    print(group['frames'], group['bones'], [m['model'] for m in group['models']])
