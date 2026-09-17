"""Run an unchanged XBE against archived sounds, then restore the ISO fixture."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import psutil


def find_entry(stream, sector, size, name):
    stream.seek(sector * 2048)
    directory = stream.read(size)
    todo, seen = [0], set()
    while todo:
        at = todo.pop()
        if at in seen or at + 14 > size:
            raise ValueError('Invalid directory')
        seen.add(at)
        left, right, data_sector, length, attr, count = struct.unpack_from('<HHIIBB', directory, at)
        actual = directory[at+14:at+14+count]
        if actual.decode('ascii').lower() == name.lower():
            return sector * 2048 + at + 14, data_sector, length, actual
        if left: todo.append(left * 4)
        if right: todo.append(right * 4)
    raise ValueError('Missing fixture directory: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['iso', 'xbe', 'exe', 'config', 'output']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=270)
    parser.add_argument('--map', type=Path)
    args = parser.parse_args()
    for process in psutil.process_iter(['name', 'cmdline']):
        if (process.info['name'] or '').lower() == 'xemu.exe' and any(
                args.iso.name.lower() in arg.lower() for arg in (process.info['cmdline'] or [])):
            raise RuntimeError('Test ISO is already mounted')
    with args.iso.open('rb') as stream:
        stream.seek(32 * 2048)
        header = stream.read(2048)
        assert header[:20] == b'MICROSOFT*XBOX*MEDIA'
        sector, size = struct.unpack_from('<II', header, 20)
        _, sector, size, _ = find_entry(stream, sector, size, 'BaseEF')
        entries = [find_entry(stream, sector, size, name) for name in ['sound', 'soundbank']]
    # Adjacent spellings retain the directory's ordering while hiding only
    # the converted sound trees. The PK3 archive assets stay available.
    replacements = [b'sounc', b'soundbanj']
    try:
        with args.iso.open('r+b') as stream:
            for entry, replacement in zip(entries, replacements):
                stream.seek(entry[0])
                assert stream.read(len(entry[3])) == entry[3]
                stream.seek(entry[0])
                stream.write(replacement)
        command = [sys.executable, str(Path(__file__).with_name('run_vo_audio_proof.py'))]
        for key in ['iso', 'xbe', 'exe', 'config', 'output', 'seconds']:
            command += ['--' + key, str(getattr(args, key))]
        if args.map:
            command += ['--map', str(args.map)]
        subprocess.run(command, check=True)
    finally:
        with args.iso.open('r+b') as stream:
            for offset, _, _, name in entries:
                stream.seek(offset)
                stream.write(name)
            stream.flush()
            for offset, _, _, name in entries:
                stream.seek(offset)
                assert stream.read(len(name)) == name
        args.output.with_suffix('.directory_restore.json').write_text(json.dumps({
            'restored': True, 'hidden_during_test': ['BaseEF/sound', 'BaseEF/soundbank'],
            'xbe_modified': False}, indent=2))


if __name__ == '__main__':
    main()
