"""Only live FPS slots count; stale bytes outside the ring are irrelevant."""
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_sp_perf_probe import read_current_fps_ring

def make(index):
    data = bytearray(64*576)
    for serial in range(max(0, index-64), index):
        line = ('STEFX_HW_FPS_SAMPLE: sample=%d gameplay=1 excludedChecks=0' % serial).encode()
        start = (serial % 64)*576
        data[start:start+len(line)] = line
    return bytes(data)

for count in (0, 1, 2, 63, 64, 65, 200):
    lines = read_current_fps_ring(make(count), struct.pack('<I', count))
    assert len(lines) == min(count, 64)
    assert lines == ['STEFX_HW_FPS_SAMPLE: sample=%d gameplay=1 excludedChecks=0' % i
                     for i in range(max(0,count-64), count)]
for data, index in [(b'', b''), (make(1)[:-1], struct.pack('<I', 1)),
                    (b'x'*(64*576), struct.pack('<I', 1)),
                    (bytes(64*576), struct.pack('<I', 1))]:
    try:
        read_current_fps_ring(data, index)
        raise AssertionError('Invalid evidence accepted')
    except ValueError:
        pass
print('PASS: seven current-ring/wrap cases and four invalid-evidence cases')
