"""Check temporary XDVDFS root additions and complete DVD-sector writes."""
import io
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_sp_perf_probe import add_root_file
from run_vo_audio_proof import root_entry

iso = io.BytesIO(bytearray(36*2048))
iso.seek(32*2048)
iso.write(b'MICROSOFT*XBOX*MEDIA'+struct.pack('<II', 33, 28))
iso.seek(33*2048)
iso.write(struct.pack('<HHIIBB', 0, 0, 34, 8, 0, 11)+b'default.xbe')
iso.seek(34*2048)
iso.write(b'baseline')
expected = {'default.xbe': b'baseline'}

def lookup(iso, name):
    # Follow only the branch selected by Xbox's uppercase comparison, unlike
    # root_entry's exhaustive traversal, which can hide an incorrectly sorted tree.
    iso.seek(32*2048+20)
    sector, size = struct.unpack('<II', iso.read(8))
    iso.seek(sector*2048)
    directory = iso.read(size)
    at = 0
    while True:
        left, right, sector, size, _, count = struct.unpack_from('<HHIIBB', directory, at)
        found = directory[at+14:at+14+count].upper()
        wanted = name.encode().upper()
        if wanted == found:
            iso.seek(sector*2048)
            return iso.read(size)
        branch = left if wanted < found else right
        assert branch, 'Xbox directory search cannot find ' + name
        at = branch*4

for index in range(24):
    name = 'ef_sp_commands.txt' if index == 0 else f'test_{index}.txt'
    payload = bytes([index])*([7, 2048, 2049][index % 3])
    add_root_file(iso, name, payload)
    expected[name] = payload
    assert len(iso.getvalue()) % 2048 == 0
    for entry, data in expected.items():
        iso.seek(root_entry(iso, entry))
        sector, size = struct.unpack('<II', iso.read(8))
        iso.seek(sector*2048)
        assert iso.read(size) == data, entry
print('PASS: 24 root additions preserve all entries and complete DVD sectors')
for name in ('ef_sp_client_active_commands.txt', 'ef_sp_client_active_command_time.txt',
             'ef_sp_smoke_harness.txt', 'ef_sp_active_command_time.txt'):
    expected[name] = name.encode()
    add_root_file(iso, name, expected[name])
for name, payload in expected.items():
    assert lookup(iso, name) == payload
print('PASS: Xbox binary search finds command and timing files with underscore ordering')
