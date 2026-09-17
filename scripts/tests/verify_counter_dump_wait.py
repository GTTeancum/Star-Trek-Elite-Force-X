"""Counter dumps tolerate delayed writes and flag incomplete evidence."""
from pathlib import Path
import re
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ja_xemu_smoke as smoke

original = smoke.monitor_cmd, smoke.time.monotonic, smoke.time.sleep
try:
    for mode in ('immediate', 'delayed', 'short', 'missing'):
        with TemporaryDirectory() as directory:
            now = [0.0]
            pending = []
            logs = []
            def monitor(sock, command, wait):
                assert wait == 0.2
                path = Path(re.search(r'"([^"]+)"$', command)[1])
                if mode == 'immediate':
                    path.write_bytes(b'1234')
                elif mode == 'delayed':
                    pending.append(path)
                elif mode == 'short':
                    path.write_bytes(b'12')
                return '(qemu)'
            def sleep(seconds):
                now[0] += seconds
                if pending and now[0] >= 0.3:
                    pending.pop().write_bytes(b'1234')
            smoke.monitor_cmd = monitor
            smoke.time.monotonic = lambda: now[0]
            smoke.time.sleep = sleep
            smoke.dump_virtual_memory_binary(object(), str(Path(directory)/'probe'),
                                             ['0x1000:4:counter'], logs.append)
            incomplete = any('incomplete=' in line for line in logs)
            assert incomplete == (mode in ('short', 'missing')), (mode, logs)
            assert now[0] <= 4.2
            if mode == 'immediate':
                assert now[0] == 0
    print('PASS: immediate/delayed counter writes and bounded short/missing evidence')
finally:
    smoke.monitor_cmd, smoke.time.monotonic, smoke.time.sleep = original
