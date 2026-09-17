"""Verify release component hashes against actual XDVDFS payloads, read-only."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def locate(stream, path):
    stream.seek(32 * 2048)
    header = stream.read(28)
    if header[:20] != b'MICROSOFT*XBOX*MEDIA':
        raise ValueError('Not an XDVDFS image')
    sector, size = struct.unpack_from('<II', header, 20)
    parts = path.replace('\\', '/').split('/')
    for index, part in enumerate(parts):
        stream.seek(sector * 2048)
        directory = stream.read(size)
        at, seen = 0, set()
        while True:
            if at in seen or at + 14 > len(directory):
                raise ValueError('Invalid directory for ' + path)
            seen.add(at)
            left, right, sector, size, attr, count = struct.unpack_from('<HHIIBB', directory, at)
            if at + 14 + count > len(directory):
                raise ValueError('Truncated directory name')
            name = directory[at + 14:at + 14 + count].upper()
            wanted = part.encode('ascii').upper()
            if wanted == name:
                if bool(attr & 0x10) != (index < len(parts) - 1):
                    raise ValueError('Unexpected entry type: ' + path)
                break
            branch = left if wanted < name else right
            if not branch:
                raise ValueError('Missing ISO component: ' + path)
            at = branch * 4
    return sector * 2048, size


def media_patch_offset(raw, packaged):
    if len(raw) != len(packaged):
        raise ValueError('XBE size mismatch')
    differences = [i for i, (a, b) in enumerate(zip(raw, packaged)) if a != b]
    if len(differences) != 1:
        raise ValueError('Expected exactly one media-enable patch byte')
    offset = differences[0]
    if raw[offset] != 0x7D or packaged[offset] != 0xEB:
        raise ValueError('Unexpected media-enable patch value')
    return offset


def verify(stream, components, release_root=None):
    records = {}
    for name, expected in components.items():
        offset, size = locate(stream, name)
        stream.seek(offset)
        remaining, digest = size, hashlib.sha256()
        while remaining:
            chunk = stream.read(min(remaining, 1024 * 1024))
            if not chunk:
                raise ValueError('Truncated payload: ' + name)
            digest.update(chunk)
            remaining -= len(chunk)
        actual = digest.hexdigest()
        records[name] = {'bytes': size, 'sha256': actual}
        if size != expected['bytes'] or actual.lower() != expected['sha256'].lower():
            if release_root is None or name not in ('default.xbe', 'efmp.xbe'):
                raise ValueError('ISO/release component mismatch: ' + name)
            raw = (release_root / name).read_bytes()
            if len(raw) != expected['bytes'] or hashlib.sha256(raw).hexdigest().lower() != expected['sha256'].lower():
                raise ValueError('Release XBE changed during packaging: ' + name)
            stream.seek(offset)
            packaged = stream.read(size)
            patch = media_patch_offset(raw, packaged)
            records[name].update(sourceSha256=expected['sha256'], mediaEnablePatchOffset=patch)
    return records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--release-root', type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    with args.iso.open('rb') as stream:
        print(json.dumps(verify(stream, manifest['components'], args.release_root), indent=2))
