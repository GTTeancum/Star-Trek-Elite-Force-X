import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_sp_host_progress import analyze, compare_progress


def profile(i, excluded=0):
    return (f'STEFX_HW_FPS_SAMPLE: sample={i} frame={100*i} realtime={5000*i} '
            f'serverTime={5000*i} gameplay=1 excludedChecks={excluded}\n')


def poll(i, t, frame=None):
    return (f'xblog t={t} frame={100*i if frame is None else frame} '
            f'rt={5000*i} st={5000*i} cls=7\n')


class HostProgressTests(unittest.TestCase):
    def test_compact_heartbeat_confirmed_by_final_gameplay_records(self):
        polls = ''.join(f'word_snapshot t={10*i} label=sp_heartbeat values=1212302931,{i},{100*i},{5000*i},{5000*i},200\n' for i in (1, 3))
        result = analyze(polls, ''.join(profile(i) for i in range(1, 4)))
        self.assertEqual(result['frames_per_wall_second'], 10)
        self.assertIsNone(analyze(polls, profile(1) + profile(2, 1) + profile(3))['frames_per_wall_second'])

    def test_compact_bad_magic_short_or_unconfirmed_rejected(self):
        for values in ('0,1,100,5000,5000,200', '1212302931,1,100',
                       '1212302931,1,999,5000,5000,200'):
            result = analyze(f'word_snapshot t=10 label=sp_heartbeat values={values}', profile(1))
            self.assertEqual(result['rejected_observations'], 1)

    def test_frame_rate_and_guest_speed_are_separate(self):
        result = analyze(poll(1, 10) + poll(3, 30), ''.join(profile(i) for i in range(1, 4)))
        self.assertEqual(result['frames_per_wall_second'], 10)
        self.assertEqual(result['guest_seconds_per_wall_second'], .5)

    def test_intro_anywhere_in_interval_rejected(self):
        for excluded in range(1, 4):
            result = analyze(poll(1, 10) + poll(3, 30),
                             ''.join(profile(i, int(i == excluded)) for i in range(1, 4)))
            self.assertIsNone(result['frames_per_wall_second'])

    def test_missing_record_is_not_gameplay_proof(self):
        result = analyze(poll(1, 10) + poll(3, 30), profile(1) + profile(3))
        self.assertEqual(result['gameplay_intervals'], [])

    def test_unconfirmed_telemetry_rejected(self):
        result = analyze(poll(1, 10) + poll(2, 20, 999), profile(1) + profile(2))
        self.assertEqual(result['rejected_observations'], 1)
        self.assertIsNone(result['frames_per_wall_second'])

    def test_stale_heartbeat_not_duplicated(self):
        result = analyze(poll(1, 10) + poll(1, 15) + poll(2, 20), profile(1) + profile(2))
        self.assertEqual(result['confirmed_observations'], 2)
        self.assertEqual(result['frames_per_wall_second'], 10)

    def test_comparison_uses_whole_shared_intervals(self):
        profiles = ''.join(profile(i) for i in range(1, 6))
        baseline = analyze(''.join(poll(i, i * 10) for i in range(1, 6)), profiles)
        candidate = analyze(''.join(poll(i, i * 5) for i in range(2, 6)), profiles)
        result = compare_progress(baseline, candidate)
        self.assertEqual(result['baseline']['intervals'], 3)
        self.assertEqual(result['baseline']['server_start'], 10000)
        self.assertEqual(result['candidate_vs_baseline_percent'], 100)

    def test_comparison_rejects_missing_evidence(self):
        with self.assertRaises(ValueError):
            compare_progress({'gameplay_intervals': []}, {'gameplay_intervals': []})


if __name__ == '__main__':
    unittest.main()
