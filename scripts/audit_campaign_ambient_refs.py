"""Report undefined ambient-set references in original campaign BSP assets.

This is a read-only asset compatibility check, not an audio-quality test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile


def audit(pak):
    with zipfile.ZipFile(pak) as archive:
        sound = archive.read('sound/sound.txt')
        definitions = set(re.findall(
            r'^\s*(?:generalSet|localSet|bmodelSet)\s+(\S+)',
            sound.decode('latin1'), re.M))
        maps = []
        for name in sorted(archive.namelist()):
            if not name.startswith('maps/') or not name.endswith('.bsp'):
                continue
            data = archive.read(name)
            offset, length = struct.unpack_from('<II', data, 8)
            if offset + length > len(data):
                raise ValueError('Invalid entity lump: ' + name)
            entities = data[offset:offset+length].decode('latin1')
            references = set(re.findall(
                r'"soundset"\s+"([^"]+)"', entities, re.I))
            missing = sorted(references - definitions)
            if missing:
                maps.append(dict(map=Path(name).stem, undefined=missing))
        return dict(source=str(pak.resolve()), sound_definitions_sha256=
                    hashlib.sha256(sound).hexdigest(),
                    definition_count=len(definitions), maps_with_undefined_sets=maps)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pak', type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.pak), indent=2))
