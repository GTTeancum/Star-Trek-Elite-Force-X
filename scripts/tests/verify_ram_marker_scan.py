"""Overlapping, duplicate, missing, and boundary markers retain exact offsets."""
import random
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ja_xemu_smoke import iter_marker_offsets

cases = [(b'ababa', [b'aba', b'ba', b'aba', b'absent']),
         (b'', [b'a']), (b'last', [b'last', b'st', b't'])]
rng = random.Random(713)
for _ in range(100):
    data = bytes(rng.choice(b'abc') for _ in range(256))
    cases.append((data, [b'a', b'aba', b'bc', b'abc', b'c', b'absent']))
for data, markers in cases:
    expected = [offset for offset in range(len(data))
                if any(data.startswith(marker, offset) for marker in markers)]
    assert list(iter_marker_offsets(data, markers)) == expected
print('PASS: 103 marker scans preserve all overlapping and boundary offsets')
