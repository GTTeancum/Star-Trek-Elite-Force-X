from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from analyze_native_frame_progress import measure


class NativeProgress(unittest.TestCase):
    def test_adjacent_intervals_share_one_counter_span(self):
        rows=[{'t':t,'frame_count':100+t*20} for t in [0,1,2,3,4]]
        out=measure(rows,[{'wall_start':0,'wall_end':2},{'wall_start':2,'wall_end':4}])
        self.assertEqual(out['native_frames'],80)
        self.assertEqual(len(out['blocks']),1)
        self.assertEqual(out['native_frames_per_host_second'],20)

    def test_excluded_gap_is_not_counted(self):
        rows=[{'t':0,'frame_count':0},{'t':1,'frame_count':10},
              {'t':3,'frame_count':1000},{'t':4,'frame_count':1010}]
        out=measure(rows,[{'wall_start':0,'wall_end':1},{'wall_start':3,'wall_end':4}])
        self.assertEqual(out['native_frames'],20)
        self.assertEqual(out['native_frames_per_host_second'],10)

    def test_reset_and_missing_data_rejected(self):
        with self.assertRaises(ValueError):
            measure([{'t':0,'frame_count':10},{'t':1,'frame_count':1}],
                    [{'wall_start':0,'wall_end':1}])
        with self.assertRaises(ValueError):
            measure([], [{'wall_start':0,'wall_end':1}])


if __name__=='__main__': unittest.main()
