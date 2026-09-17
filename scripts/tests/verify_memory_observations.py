"""Validate dropped/racing publication handling without a live emulator."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from xemu_flight_recorder import read_memory_observations


class MemoryObservations(unittest.TestCase):
    def reader(self, serial, corrupt=None, race=False, schema=1, context=0):
        calls = 0

        def read(address, size):
            nonlocal calls
            if address == 4:
                calls += 1
                return struct.pack('<I', serial + int(race and calls > 1))
            slot = (address - 4096) // 80
            expected = serial - ((serial - 1 - slot) % 64)
            words = [expected, schema, 1000 + expected] + [0] * 16 + [context]
            if expected == corrupt:
                words[0] = 0
            return struct.pack('<20I', *words)[:size]
        return read

    def test_fresh_records(self):
        result = read_memory_observations(self.reader(3), 4096, 4, 0)
        self.assertEqual([r['serial'] for r in result['records']], [1, 2, 3])
        self.assertEqual(result['records'][2]['guest_ms'], 1003)
        self.assertEqual(result['dropped_records'], 0)

    def test_unchanged_is_not_a_new_sample(self):
        self.assertIsNone(read_memory_observations(self.reader(3), 4096, 4, 3))

    def test_asset_context_and_unknown_schema(self):
        result = read_memory_observations(self.reader(1, schema=2, context=0xabcdef01), 4096, 4, 0)
        self.assertEqual(result['records'][0]['asset_context'], 0xabcdef01)
        self.assertIsNone(read_memory_observations(self.reader(1, schema=3), 4096, 4, 0))

    def test_wrap_reports_loss(self):
        result = read_memory_observations(self.reader(80), 4096, 4, 0)
        self.assertEqual(result['dropped_records'], 17)
        self.assertEqual([r['serial'] for r in result['records']], list(range(18, 81)))

    def test_in_progress_slot_rejected(self):
        self.assertIsNone(read_memory_observations(self.reader(3, corrupt=2), 4096, 4, 0))

    def test_publication_race_rejected(self):
        self.assertIsNone(read_memory_observations(self.reader(3, race=True), 4096, 4, 0))

    def test_missing_symbol_or_short_read(self):
        self.assertIsNone(read_memory_observations(self.reader(3), None, 4, 0))
        self.assertIsNone(read_memory_observations(lambda a, s: b'', 4096, 4, 0))


if __name__ == '__main__':
    unittest.main()
