from pathlib import Path
import copy
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_native_video import summarize
from xemu_native_video_stats import COUNTERS, XEMU_SHA256, LRU_PROBE_SHA256


def row(sequence, mspf):
    counters=[0]*45; counters[COUNTERS.index('BEGIN_ENDS')]=10
    counters[COUNTERS.index('GEOM_BUFFER_UPDATE_1')]=5
    return {'sequence':sequence,'mspf':mspf,'counters':counters}


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.rows=[{'kind':'native_video_setup','sha256':XEMU_SHA256,'counter_names':COUNTERS},
                   {'kind':'native_video','t':40,'frame_count':10,'increment_fps':20,'dropped_records':0,'frames':[row(9,999)]},
                   {'kind':'native_video','t':41,'frame_count':14,'increment_fps':20,'dropped_records':0,'frames':[row(10,10),row(11,20),row(12,30),row(13,40)]}]

    def test_exclusion_order_and_ratios(self):
        s=summarize(self.rows)
        self.assertEqual(s['records'],4)
        self.assertEqual(s['all']['median_mspf'],25)
        self.assertEqual(s['worst'][0]['mspf'],40)
        self.assertEqual(s['fast_quarter']['median_mspf'],10)
        self.assertEqual(s['all']['totals_per_draw']['GEOM_BUFFER_UPDATE_1'],0.5)

    def test_missing_abi_and_duplicate_rejected(self):
        with self.assertRaises(ValueError):summarize(self.rows[1:])
        broken=copy.deepcopy(self.rows);broken[-1]['frames'].append(row(13,40))
        with self.assertRaises(ValueError):summarize(broken)

    def test_guard_copy_must_be_explicit_and_exact(self):
        self.rows[0]['sha256']=LRU_PROBE_SHA256
        with self.assertRaises(ValueError):summarize(self.rows)
        self.rows[0]['binary_variant']='isolated_lru_guard_probe'
        self.assertEqual(summarize(self.rows)['validated_abi']['sha256'],LRU_PROBE_SHA256)
        self.rows[0]['sha256']='unrecognized'
        with self.assertRaises(ValueError):summarize(self.rows)

    def test_gameplay_boundary_batches_are_excluded(self):
        self.rows += [dict(kind='native_video', t=42, frame_count=16,
                           increment_fps=18, dropped_records=0, frames=[row(14,50),row(15,60)]),
                      dict(kind='native_video', t=43, frame_count=17,
                           increment_fps=17, dropped_records=0, frames=[row(16,900)])]
        # The first batch ends in gameplay, but began before the cutoff.
        s=summarize(self.rows,[dict(wall_start=40.5,wall_end=42.5)])
        self.assertEqual(s['records'],2)
        self.assertEqual(s['non_gameplay_or_boundary_records_excluded'],5)
        self.assertEqual(s['all']['median_mspf'],55)
        self.assertEqual(s['native_fps_snapshot_median'],18)
        with self.assertRaises(ValueError):summarize(self.rows,[])

    def test_gameplay_gap_cannot_be_joined(self):
        self.rows.append(dict(kind='native_video', t=45, frame_count=15,
                              increment_fps=5,dropped_records=0,frames=[row(14,999)]))
        s=summarize(self.rows,[dict(wall_start=40,wall_end=41),dict(wall_start=44,wall_end=46)])
        self.assertEqual(s['records'],4)
        self.assertEqual(s['all']['max_mspf'],40)


if __name__=='__main__':unittest.main()
