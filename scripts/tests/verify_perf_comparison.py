"""Guard against intro leakage, missing samples and misleading rate averages."""
import sys
from pathlib import Path
import unittest
import json
import subprocess
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_sp_perf_probes import windows, compare


def row(serial, frame, time, gameplay=1, excluded=0, timing='100/5000/60/20/30/1'):
    return (f'STEFX_HW_FPS_SAMPLE: sample={serial} frame={frame} realtime={time} '
            f'serverTime={time} ft={timing} gameplay={gameplay} excludedChecks={excluded}\n')


class ComparisonTests(unittest.TestCase):
    def test_intro_overlap_is_excluded(self):
        text = row(1, 100, 5000, 0, 10) + row(2, 200, 10000, 1, 1)
        text += row(3, 300, 15000) + row(4, 400, 20000)
        actual = windows(text)
        self.assertEqual([item['start'] for item in actual], [10000, 15000])

    def test_no_invented_window_after_missing_record(self):
        self.assertEqual(windows(row(1, 100, 5000) + row(3, 300, 15000)), [])

    def test_duplicates_are_not_extra_samples(self):
        text = row(1, 100, 5000) + row(2, 200, 10000)
        self.assertEqual(len(windows(text + text)), 1)
        with self.assertRaises(ValueError):
            windows(text + row(2, 201, 10000))

    def test_shared_interval_keeps_only_whole_windows(self):
        left = windows(''.join(row(i, i * 100, i * 5000) for i in range(1, 7)))
        right = windows(''.join(row(i, i * 100, i * 5000 + 500) for i in range(1, 6)))
        result = compare(left, right)
        self.assertEqual(result['left']['retained_server_start'], 10000)
        self.assertEqual(result['left']['retained_server_end'], 25000)
        self.assertEqual(result['right']['retained_server_start'], 5500)
        self.assertEqual(result['right']['retained_server_end'], 25500)

    def test_rate_is_total_frames_over_total_time(self):
        data = windows(row(1, 1, 1000) + row(2, 101, 2000, timing='100/1000/20/20/20/0')
                       + row(3, 151, 4000, timing='50/2000/40/40/40/0'))
        result = compare(data, data)
        self.assertEqual(result['left']['weighted_guest_fps'], 50)
        self.assertEqual(result['left']['minimum_window_fps'], 25)
        self.assertEqual(result['right_vs_left_percent'], 0)

    def test_insufficient_overlap_fails(self):
        data = windows(row(1, 1, 1000) + row(2, 101, 2000))
        with self.assertRaises(ValueError):
            compare(data, data)

    def test_simulation_clock_cannot_inflate_elapsed_fps(self):
        data = windows(row(1, 1, 1000) + row(2, 1001, 1100, timing='100/5000/90/60/80/3'))
        self.assertEqual(data[0]['fps'], 20)
        self.assertEqual(data[0]['frames'], 100)

    def test_incomplete_elapsed_record_is_rejected(self):
        for timing in ['', '100/5000/90/60/80', '100/0/90/60/80/3', '0/5000/90/60/80/3']:
            self.assertEqual(windows(row(1, 1, 1000) + row(2, 101, 6000, timing=timing)), [])

    def test_cli_uses_current_ring_and_rejects_unsafe_provenance(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            summary = dict(xbe_sha256='a' * 64, map='borg3', loaded_map='borg3', mode='sp',
                           mode_verified=True, finished_in_game=True, visual_only=False,
                           diagnostic_only=False, final_liveness_verified=True, final_client_state=7,
                           profile_provenance='current paused guest FPS ring')
            current = ''.join(row(i, i * 100, i * 5000) for i in range(1, 5))
            for name in ['left', 'right']:
                (root / (name + '.summary.json')).write_text(json.dumps(summary))
                (root / (name + '.iso_transaction.json')).write_text('{"restored": true}')
                (root / (name + '.current_profiles.log')).write_text(current)
                (root / (name + '_old_xblog_profiles.log')).write_text('stale and invalid')
            command = [sys.executable, str(Path(__file__).resolve().parents[1] / 'compare_sp_perf_probes.py'),
                       str(root / 'left.summary.json'), str(root / 'right.summary.json')]
            good = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertEqual(json.loads(good.stdout)['left']['weighted_guest_fps'], 20)
            for change in [dict(god_mode=True), dict(final_liveness_verified=False),
                           dict(profile_provenance='raw RAM scan'), dict(engine_error_message='failed')]:
                (root / 'right.summary.json').write_text(json.dumps(dict(summary, **change)))
                self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            (root / 'right.summary.json').write_text(json.dumps(summary))
            (root / 'right.current_profiles.log').unlink()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            (root / 'right.current_profiles.log').write_text(current)
            for name in ['left', 'right']:
                (root / (name + '.summary.json')).write_text(json.dumps(dict(summary, mode='mp')))
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            for name in ['left', 'right']:
                (root / (name + '.summary.json')).write_text(json.dumps(dict(summary, mode='mp', simulation_verified=True)))
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            bots = [0] * 32
            bots[13:16] = [4, 4, 4]
            workload = dict(summary, mode='mp', simulation_verified=True,
                            simulation_evidence=dict(connected=8, playing=8),
                            bot_startup_evidence=bots,
                            local_command_evidence=dict(hm_cmd_serial=[100]*4,
                                hm_cmd_forward=[48]*4, hm_cmd_right=[0]*4))
            for name in ['left', 'right']:
                (root / (name + '.summary.json')).write_text(json.dumps(workload))
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            # A stale pass flag must not disguise an empty or stationary match.
            for change in [dict(bot_startup_evidence=[0]*32),
                           dict(local_command_evidence=dict(hm_cmd_serial=[100]*4,
                                hm_cmd_forward=[48,48,48,0], hm_cmd_right=[0]*4))]:
                (root / 'right.summary.json').write_text(json.dumps(
                    dict(workload, mp_workload_verified=True, **change)))
                self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
