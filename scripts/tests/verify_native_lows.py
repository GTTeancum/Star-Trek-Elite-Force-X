import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_native_lows import windows

class NativeLows(unittest.TestCase):
    def test_stall_is_not_dropped(self):
        rows=[{'t':float(t),'frame_count':min(t,12)*40} for t in range(31)]
        r=windows(rows,[{'wall_start':0,'wall_end':30}])
        self.assertEqual(r['minimum_native_fps'],0)
        self.assertEqual(r['window_count'],21)
    def test_does_not_bridge_excluded_time(self):
        rows=[{'t':float(t),'frame_count':t*35} for t in range(41)]
        r=windows(rows,[{'wall_start':0,'wall_end':12},{'wall_start':25,'wall_end':40}])
        self.assertEqual(r['minimum_native_fps'],35)
        self.assertTrue(all(w['end']<=12 or w['start']>=25 for w in r['windows']))
    def test_nonuniform_observations_use_elapsed_time(self):
        rows=[{'t':t*.7,'frame_count':t*21} for t in range(50)]
        r=windows(rows,[{'wall_start':0,'wall_end':34.3}])
        self.assertAlmostEqual(r['minimum_native_fps'],30)
        self.assertAlmostEqual(r['minimum_complete_window_fps_lower_bound'],29.4)
        self.assertEqual(r['complete_windows_not_proven_above_30'],r['window_count'])
        self.assertTrue(all(8<=w['host_seconds']<=10 for w in r['windows']))
    def test_reset_fails(self):
        rows=[{'t':float(t),'frame_count':t*35 if t<15 else 0} for t in range(31)]
        with self.assertRaises(ValueError):
            windows(rows,[{'wall_start':0,'wall_end':30}])

if __name__=='__main__': unittest.main()
