"""Ensure complete FPS classification survives worst-case numeric counters."""
from pathlib import Path
import re
import ast
import sys

root = Path(__file__).resolve().parents[2]
client = (root / 'code/client/cl_main.cpp').read_text()
logger = (root / 'code/win32/xb_log.cpp').read_text()
capacity = int(re.search(r'g_SPXBFpsProfileMirror\[64\]\[(\d+)\]', logger)[1])
formats = re.findall(r'"(STEFX_HW_FPS_SAMPLE:[^"]+)"', client)
assert formats
tree = ast.parse((root/'scripts/ja_xemu_smoke.py').read_text())
patterns = [node.value for node in ast.walk(tree) if isinstance(node, ast.Constant)
            and isinstance(node.value, str) and node.value.startswith(r'\bsample=')
            and 'excludedChecks' in node.value]
assert len(patterns) == 1
for fmt in formats:
    # All counters at their full 32-bit width, including the context suffix.
    record = re.sub(r'%[ud]', '4294967295', fmt).replace('%s', 'direct')
    suffix = re.search(r'"( ft=[^"]+)"', client)[1]
    record += re.sub(r'%[ud]', '4294967295', suffix)
    assert len(record) >= 256, 'Negative control must reproduce old overflow'
    assert len(record) < capacity, (len(record), capacity)
    populated = re.sub(r'gameplay=\d+', 'gameplay=1', record)
    assert re.search(patterns[0], populated), 'Native extractor must accept full frame-time record'
    assert not re.search(patterns[0], populated.replace(' ft=', ' ft=1/')), 'Truncated/malformed statistics must fail'
print(f'FPS record capacity: {len(formats)} formats fit {capacity}-byte slots')
sys.path.insert(0, str(root/'scripts'))
from run_sp_perf_probe import parse_frame_times
parsed = parse_frame_times('ft=100/60/10/60/5 gameplay=1 excludedChecks=0')
assert parsed['frames'] == 100 and parsed['frames_over_50_guest_ms'] == 5
assert parsed['p95_upper_guest_ms'] == 10 and parsed['p99_upper_guest_ms'] == 60
for invalid in ('', 'ft=1/2/3/4', 'ft=0/0/0/0/0', 'ft=1/20/30/20/0', 'ft=1/20/10/20/2'):
    assert parse_frame_times(invalid) is None
print('Frame-time parser accepts complete records and rejects absent/inconsistent statistics')

parsed = parse_frame_times('fps=100.0 ft=100/10000/5000/10/60/5 gameplay=1 excludedChecks=0')
assert parsed['elapsed_guest_fps'] == 10 and parsed['elapsed_guest_ms'] == 10000
assert parse_frame_times('ft=100/10/5000/10/60/5') is None
assert parse_frame_times('ft=100/0/0/0/0/0') is None
print('Elapsed-time rate includes stalls hidden by simulation FPS')
