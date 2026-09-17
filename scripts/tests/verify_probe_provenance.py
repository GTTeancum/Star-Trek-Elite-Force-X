"""A probe must never inherit telemetry from an older, similarly named run."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_sp_perf_probe import exact_probe_files


class ProbeProvenanceTests(unittest.TestCase):
    def test_only_reported_timestamp_is_selected(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            name = 'wide'
            prefix = 'wide_20260910_190453'
            suffix = '_final_current_fps_ring.bin'
            for stem in (prefix, 'wide_check_20260910_190047',
                         'wide_20260910_180000', 'wide_20260910_200000'):
                (out / (stem + suffix)).write_bytes(b'proof')
            (out / 'wide.run.log').write_text('report=scripts\\output\\' + prefix + '.report.txt\n')
            select = exact_probe_files(out, name)
            self.assertEqual(select(suffix), [out / (prefix + suffix)])
            self.assertEqual(select('_final_missing.bin'), [])

    def test_missing_current_file_does_not_fall_back(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / 'wide.run.log').write_text('report=scripts/output/wide_20260910_190453.report.txt\n')
            (out / 'wide_check_20260910_190047_final_liveness_after.bin').write_bytes(b'live')
            self.assertEqual(exact_probe_files(out, 'wide')('_final_liveness_after.bin'), [])

    def test_wrong_missing_or_multiple_reports_are_rejected(self):
        for report in ('', 'report=other_20260910_190453.report.txt\n',
                       'report=wide_check_20260910_190453.report.txt\n',
                       'report=wide_20260910_190453.report.txt\n' * 2):
            with tempfile.TemporaryDirectory() as directory:
                out = Path(directory)
                (out / 'wide.run.log').write_text(report)
                with self.assertRaises(ValueError):
                    exact_probe_files(out, 'wide')


if __name__ == '__main__':
    unittest.main()
