"""Read a paused XEMU RAM snapshot and compare resident MDR frames to source.

Physical scans locate candidate model records only. Their virtual pointers,
header, frame table and decoded bytes must all validate against source assets.
This is a diagnostic content check, not performance or route qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def restore_frame_bytes(kind, payload, stride):
    """Invert resident byte transforms independently of the C++ decoder."""
    assert 0 <= kind <= 6 and 0 < stride <= 1024 and stride % 2 == 0
    assert len(payload) % stride == 0 and 1 <= len(payload) // stride <= 16
    frames = len(payload) // stride
    if kind >= 4:
        value = bytearray(payload[byte * frames + frame]
                          for frame in range(frames) for byte in range(stride))
    else:
        value = bytearray(payload)
    if kind in (2, 3, 5):
        for i in range(stride, len(value)):
            value[i] = (value[i] ^ value[i-stride] if kind == 2
                        else (value[i] + value[i-stride]) & 255)
    if kind == 6:
        for i in range(stride, len(value), 2):
            word = (int.from_bytes(value[i:i+2], 'little') +
                    int.from_bytes(value[i-stride:i-stride+2], 'little')) & 65535
            value[i:i+2] = word.to_bytes(2, 'little')
    return bytes(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ram', type=Path, required=True)
    parser.add_argument('--registers', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    ram = args.ram.read_bytes()
    assert len(ram) == 64 * 1024 * 1024
    cr3 = int(re.search(r'CR3=([0-9a-fA-F]+)', args.registers.read_text())[1], 16)

    def word(address):
        return struct.unpack_from('<I', ram, address)[0]

    def read(address, size):
        chunks = []
        while size:
            directory = word((cr3 & 0xfffff000) + (address >> 22) * 4)
            if not directory & 1:
                raise ValueError('Unmapped directory')
            if directory & 128:
                physical = (directory & 0xffc00000) + (address & 0x3fffff)
            else:
                page = word((directory & 0xfffff000) + ((address >> 12) & 1023) * 4)
                if not page & 1:
                    raise ValueError('Unmapped page')
                physical = (page & 0xfffff000) + (address & 4095)
            count = min(size, 4096 - (address & 4095))
            if physical + count > len(ram):
                raise ValueError('Outside RAM')
            chunks.append(ram[physical:physical + count])
            address += count
            size -= count
        return b''.join(chunks)

    def decode(data, expected):
        output = bytearray()
        cursor = 0
        while cursor < len(data):
            token = data[cursor]
            cursor += 1
            if token < 128:
                count = token + 1
                assert cursor + count <= len(data)
                output.extend(data[cursor:cursor + count])
                cursor += count
            else:
                assert cursor + 2 <= len(data)
                distance = int.from_bytes(data[cursor:cursor + 2], 'little')
                cursor += 2
                assert 0 < distance <= len(output)
                for _ in range((token & 127) + 3):
                    output.append(output[-distance])
            assert len(output) <= expected
        assert len(output) == expected
        return bytes(output)

    records = {}
    for match in re.finditer(b'models/players/[a-z0-9_/]+\\.mdr\x00', ram):
        at = match.start()
        if at + 120 > len(ram):
            continue
        fields = struct.unpack_from('<14I', ram, at + 64)
        if fields[0] != 5 or fields[11] != 0xfffffff0:
            continue
        name = match[0][:-1].decode('ascii')
        size, header_at, table_at, blob_at = fields[2], fields[7], fields[9], fields[10]
        frame_size, frames = fields[12:14]
        if not (40 < frame_size <= 1024 and 0 < frames <= 65536 and 104 < size < 16000000):
            continue
        header = read(header_at, 104)
        assert header[:4] == b'RDM5', name
        source = (args.assets / name).read_bytes()
        source_frames, bones, offset = struct.unpack_from('<iii', source, 72)
        assert source_frames == frames and frame_size == 40 + bones * 24 and offset == -104, name
        blocks = (frames + 15) // 16
        offsets = struct.unpack('<' + 'I' * (blocks + 1), read(table_at, (blocks + 1) * 4))
        assert offsets[0] == 0 and blob_at + offsets[-1] == header_at + size, name
        decoded = []
        for block in range(blocks):
            assert offsets[block] < offsets[block + 1], name
            data = read(blob_at + offsets[block], offsets[block + 1] - offsets[block])
            expected = min(16, frames - block * 16) * frame_size
            assert data[0] in range(7), name
            value = decode(data[1:], expected) if data[0] else data[1:]
            assert len(value) == expected, name
            value = restore_frame_bytes(data[0], value, frame_size)
            decoded.append(value)
        decoded = b''.join(decoded)
        assert decoded == source[104:104 + frames * frame_size], name
        records[name] = dict(frames=frames, animation_bytes=len(decoded),
                             animation_sha256=hashlib.sha256(decoded).hexdigest(),
                             resident_bytes=size, source_bytes=len(source),
                             saved_bytes=len(source) - size)
    assert records, 'No validated resident block models found'
    args.output.write_text(json.dumps(records, indent=2))
    print('PASS: every animation byte in %d resident model records matches source' % len(records))


if __name__ == '__main__':
    main()
