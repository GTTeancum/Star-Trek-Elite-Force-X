"""Prevent a requested split-screen mode from being mistaken for rendered proof."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_sp_perf_probe import verify_mode, read_dump_words
from tempfile import TemporaryDirectory
import struct
import run_sp_perf_probe as probe

window = dict(visual_only=False, diagnostic_only=False, mode_verified=True,
    final_client_state=7, final_liveness_verified=True, engine_error_message='',
    loaded_map='hm_borg1', map='hm_borg1', fps=[51, 52], average_fps=51.5, target_fps=30)
assert probe.qualifies_window(window)
assert not probe.qualifies_window(dict(window, mode='mp'))
assert not probe.qualifies_window(dict(window, mode='mp', simulation_verified=False))
assert not probe.qualifies_window(dict(window, mode='mp', simulation_verified=True))
assert probe.qualifies_window(dict(window, mode='mp', simulation_verified=True, mp_workload_verified=True))
beta_window = dict(window, mode='mp', simulation_verified=True,
                   mp_workload_verified=True, fps=[15, 30], average_fps=22.5)
assert probe.qualifies_window(beta_window)  # slow windows are reported, not a 30 FPS gate
for average in (None, 0, 19.99, 20, float('nan'), float('inf')):
    assert not probe.qualifies_window(dict(beta_window, average_fps=average))
assert probe.qualifies_window(dict(beta_window, average_fps=20.01))
bot_proof = [0] * 32
bot_proof[13:16] = [4, 4, 4]
workload = dict(simulation_evidence=dict(connected=8, playing=8),
    bot_startup_evidence=bot_proof, local_command_evidence=dict(
        hm_cmd_serial=[50]*4, hm_cmd_forward=[90]*4, hm_cmd_right=[0]*4))
assert probe.verify_mp_workload(workload)
assert not probe.verify_mp_workload(dict(workload, simulation_evidence=dict(connected=4, playing=4)))
assert not probe.verify_mp_workload(dict(workload, bot_startup_evidence=[0]*32))
assert not probe.verify_mp_workload(dict(workload, bot_startup_evidence=None))
assert not probe.verify_mp_workload(dict(workload, local_command_evidence=dict(
    hm_cmd_serial=[50]*4, hm_cmd_forward=[90,90,90,0], hm_cmd_right=[0]*4)))
assert not probe.verify_mp_workload(dict(workload, local_command_evidence={}))
for field, value in [('final_client_state', 1), ('final_client_state', None),
                     ('final_liveness_verified', False), ('final_liveness_verified', None),
                     ('engine_error_message', 'ERR_DROP: allocation failed'),
                     ('engine_error_message', None), ('current_fps_error', 'invalid ring'),
                     ('god_mode_requested', True), ('god_mode', True),
                     ('visual_only', True), ('diagnostic_only', True),
                     ('mode_verified', False), ('loaded_map', 'borg1'),
                     ('fps', [51]), ('fps', [51, 29])]:
    assert not probe.qualifies_window(dict(window, **{field: value}))

class DelayedRelease:
    def __init__(self, failures):
        self.failures, self.calls = failures, 0
    def open(self, mode):
        assert mode == 'r+b'
        self.calls += 1
        if self.calls <= self.failures:
            raise PermissionError('sharing violation')
        return self

original_sleep = probe.time.sleep
probe.time.sleep = lambda _: None
try:
    delayed = DelayedRelease(3)
    assert probe.open_iso_for_restore(delayed) is delayed and delayed.calls == 4
    locked = DelayedRelease(100)
    try:
        probe.open_iso_for_restore(locked)
        raise AssertionError('Persistent lock must not be treated as restored')
    except PermissionError:
        assert locked.calls == 11
finally:
    probe.time.sleep = original_sleep

with TemporaryDirectory() as directory:
    dump = Path(directory) / 'memory.bin'
    assert read_dump_words([], 4) is None
    for size in (0, 1, 15, 17):
        dump.write_bytes(b'\0' * size)
        assert read_dump_words([dump], 4) is None
    dump.write_bytes(struct.pack('<4I', 10, 20, 30, 40))
    assert read_dump_words([dump], 4) == [10, 20, 30, 40]

evidence = dict(p2_refdef_valid=1, hm_draws=[42,39,0,0], hm_done=[200,200,0,0])
assert verify_mode('sp', {1}, {})
assert not verify_mode('sp', set(), {})
assert not verify_mode('sp', {1, 2}, evidence)
assert verify_mode('coop', {2}, evidence)
assert not verify_mode('coop', {1}, evidence)
assert not verify_mode('coop', {2}, {})
assert not verify_mode('coop', {2}, dict(evidence, p2_refdef_valid=0))
assert not verify_mode('coop', {2}, dict(evidence, hm_draws=[42,0,0,0]))
assert not verify_mode('mp', {4}, evidence)
four = dict(hm_draws=[123, 113, 110, 125], hm_done=[200, 200, 200, 199])
assert verify_mode('mp', {4}, four)
assert not verify_mode('mp', {4}, dict(four, hm_draws=[123, 113, 0, 125]))
assert not verify_mode('mp', {4}, dict(four, hm_done=[200, 200, 200, 150]))
assert not verify_mode('mp', {4}, dict(four, hm_done=[1, 1, 1, 1]))
assert not verify_mode('mp', {2}, four)
print('PASS: 14 mode qualification cases')
print('PASS: 6 complete/missing/short/oversized memory dump cases')
print('PASS: transient and persistent ISO sharing violations')
print('PASS: window gates, including invincibility and return-to-menu rejection')

# Use actual XDVDFS parsing: absent marker, present marker, malformed image.
import io
for name in (b'default.xbe', b'ef_sp_smoke_harness.txt', b'ef_sp_input_replay.txt'):
    marker = name != b'default.xbe'
    data=bytearray(41*2048)
    data[32*2048:32*2048+20]=b'MICROSOFT*XBOX*MEDIA'
    struct.pack_into('<II',data,32*2048+20,40,14+len(name))
    struct.pack_into('<HHIIBB',data,40*2048,0,0,0,1,0,len(name))
    data[40*2048+14:40*2048+14+len(name)]=name
    try:
        probe.reject_campaign_override_marker(io.BytesIO(data))
        assert not marker
    except ValueError as error:
        assert marker and 'pre-existing diagnostic marker' in str(error)
try:
    probe.reject_campaign_override_marker(io.BytesIO(b'invalid'))
    raise AssertionError('Malformed images must not be treated as marker-free')
except ValueError as error:
    assert 'Not an XDVDFS' in str(error)
print('PASS: campaign marker presence, absence, and malformed-image checks')

# Invalid sampling requests must stop before touching an ISO or launching XEMU.
import subprocess
sampling_base = [sys.executable, str(Path(probe.__file__)), '--iso', 'unused',
                 '--xbe', 'unused', '--symbols', 'unused', '--config', 'unused',
                 '--hdd', 'unused', '--map', 'forge4', '--name', 'unused']
for sampling_flags in [ ['--sample-eip-interval', '0.2'],
                       ['--diagnostic', '--sample-eip-interval', '0.01'],
                       ['--diagnostic', '--sample-eip-interval', 'nan'],
                       ['--diagnostic', '--sample-eip-interval', '-1'] ]:
    sampling_result = subprocess.run(sampling_base + sampling_flags, capture_output=True, text=True)
    assert sampling_result.returncode == 2
    assert 'Instruction sampling requires' in sampling_result.stderr
print('PASS: instruction sampling remains diagnostic and bounded')
