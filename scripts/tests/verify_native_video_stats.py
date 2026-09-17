"""Validate profiler ring publication, wrap, gaps and bounded reads."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from xemu_native_video_stats import (SIZE, STRIDE, HISTORY_OFFSET, POINTER_OFFSET,
                                     decode_completed_frames, read_stable_snapshot)


def fixture(count=600):
    data = bytearray(SIZE)
    struct.pack_into('<qII', data, 0, 123456789, count, 20)
    struct.pack_into('<I', data, POINTER_OFFSET, count % 300)
    for sequence in range(max(0, count-300), count):
        struct.pack_into('<46i', data, HISTORY_OFFSET+(sequence % 300)*STRIDE,
                         sequence, *([sequence]*45))
    return data


class NativeVideoTests(unittest.TestCase):
    def test_oldest_slot_excluded_and_ordered(self):
        data = fixture(650)
        # Writer has overwritten its next/oldest slot before publishing count.
        struct.pack_into('<46i', data, HISTORY_OFFSET+50*STRIDE, *([-1]*46))
        value = decode_completed_frames(data)
        self.assertEqual([r['sequence'] for r in value['frames']], list(range(351,650)))
        self.assertTrue(all(r['mspf'] == r['sequence'] for r in value['frames']))

    def test_deduplicate_and_report_overrun(self):
        self.assertEqual(decode_completed_frames(fixture(), 599)['frames'], [])
        value = decode_completed_frames(fixture(), 295)
        self.assertEqual(value['dropped_records'], 5)
        self.assertEqual(value['frames'][0]['sequence'], 301)
        self.assertEqual(len(decode_completed_frames(fixture(), 596)['frames']), 3)
        self.assertIsNone(decode_completed_frames(fixture(), 650))

    def test_startup_and_invalid_snapshots(self):
        self.assertEqual(len(decode_completed_frames(fixture(10))['frames']), 10)
        self.assertEqual(decode_completed_frames(fixture(0))['frames'], [])
        self.assertIsNone(decode_completed_frames(b''))
        data = fixture(); struct.pack_into('<I', data, POINTER_OFFSET, 300)
        self.assertIsNone(decode_completed_frames(data))
        data = fixture(); struct.pack_into('<I', data, POINTER_OFFSET, 1)
        self.assertIsNone(decode_completed_frames(data))

    def test_read_rejects_torn_or_short_publication(self):
        data = fixture()
        reads = []
        def read(offset, size):
            reads.append((offset,size)); return data[offset:offset+size]
        self.assertEqual(read_stable_snapshot(read)['frame_count'], 600)
        self.assertEqual(reads, [(8,4),(0,SIZE),(8,4)])
        replies = iter([struct.pack('<I',599), data, struct.pack('<I',600)])
        self.assertIsNone(read_stable_snapshot(lambda o,s: next(replies)))
        replies = iter([data[8:12], data[:-1], data[8:12]])
        self.assertIsNone(read_stable_snapshot(lambda o,s: next(replies)))


if __name__ == '__main__':
    unittest.main()
