"""Native monitor startup must be confirmed paused before issuing cont."""
from pathlib import Path
import sys
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ja_xemu_smoke as smoke

for status in ('VM status: prelaunch', 'VM status: paused'):
    calls, logs = [], []
    def command(sock, cmd, wait):
        calls.append(cmd)
        return status if cmd == 'info status' else ''
    with patch.object(smoke, 'monitor_cmd', command):
        smoke.resume_after_setup(object(), logs.append)
    assert calls == ['info status', 'cont']
    assert logs == ['emulation_setup_verified_paused', 'emulation_started_after_setup']
for status in ('VM status: running', 'unreadable'):
    with patch.object(smoke, 'monitor_cmd', return_value=status) as command:
        try:
            smoke.resume_after_setup(object(), lambda message: None)
        except RuntimeError:
            pass
        else:
            raise AssertionError('Unconfirmed startup was accepted')
        assert command.call_count == 1
print('Paused-start verification: two valid states and two rejected states passed')
