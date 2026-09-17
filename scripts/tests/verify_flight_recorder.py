import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from xemu_flight_recorder import Recorder, translate, read_frame_cost, read_fps_profiles, FRAME_COST_FIELDS, read_draw_kinds, read_model_draws
from xemu_flight_recorder import read_coop_cgame, read_coop_fx


class CoopFxPublication(unittest.TestCase):
    def fixture(self, sequence=2, schema=1, count=2):
        words=[sequence,schema,77,9000,400,count]+[0]*(32*9)
        words[6:24]=list(range(11,29))
        return struct.pack('<294I',*words)

    def test_operation_layout(self):
        raw=self.fixture()
        r=read_coop_fx(lambda a,n:raw[:n],100,0)
        self.assertEqual(r['client_frame'],77)
        self.assertEqual(r['live_effects'],400)
        self.assertEqual(r['rows'][0]['vtable'],11)
        self.assertEqual(r['rows'][0]['free_count'],12)
        self.assertEqual(r['rows'][1]['draw_cycles'],28)
        self.assertIsNone(read_coop_fx(lambda a,n:raw[:n],100,2))

    def test_reject_invalid_and_torn(self):
        self.assertIsNone(read_coop_fx(lambda a,n:b'',None,0))
        self.assertIsNone(read_coop_fx(lambda a,n:b'',100,0))
        for raw in (self.fixture(sequence=0), self.fixture(sequence=3),
                    self.fixture(schema=2), self.fixture(count=33), self.fixture()[:-1]):
            self.assertIsNone(read_coop_fx(lambda a,n:raw[:n],100,0))
        for changed in (3,4):
            reads=iter([struct.pack('<I',2),self.fixture(),struct.pack('<I',changed)])
            self.assertIsNone(read_coop_fx(lambda a,n:next(reads),100,0))


class CoopCgamePublication(unittest.TestCase):
    def test_version_two_tail_breakdown_and_short_record(self):
        raw=struct.pack('<51I',2,*range(1,51))
        result=read_coop_cgame(lambda a,n:raw[:n],100,0,version=2)
        self.assertEqual(result['schema'],2)
        self.assertEqual(result['tail_weapon_ms'],45)
        self.assertEqual(result['tail_p2_ms'],46)
        self.assertEqual(result['tail_listener_ms'],47)
        self.assertEqual(result['tail_powerup_ms'],48)
        self.assertEqual(result['tail_fx_ms'],49)
        self.assertEqual(result['tail_misc_ms'],50)
        self.assertIsNone(read_coop_cgame(lambda a,n:raw[:min(n,180)],100,0,version=2))

    def test_completed_frame_and_entity_counters(self):
        words=[12,300,150000,24,2,1,3,8,4,6,5,1,700]+list(range(16))+list(range(16,32))
        raw=struct.pack('<45I',*words)
        result=read_coop_cgame(lambda a,n: raw[:n],100,10)
        self.assertEqual(result['total_ms'],24)
        self.assertEqual(result['hud_ms'],1)
        self.assertEqual(result['main_loop'],700)
        self.assertEqual(result['entity_cycles'],list(range(16)))
        self.assertEqual(result['entity_counts'],list(range(16,32)))
        self.assertIsNone(read_coop_cgame(lambda a,n: raw[:n],100,12))

    def test_absent_partial_and_torn_frames(self):
        self.assertIsNone(read_coop_cgame(lambda a,n: b'',None,0))
        self.assertIsNone(read_coop_cgame(lambda a,n: b'',100,0))
        for seq in (0,3):
            self.assertIsNone(read_coop_cgame(lambda a,n: struct.pack('<I',seq),100,0))
        self.assertIsNone(read_coop_cgame(lambda a,n: struct.pack('<I',2),100,0))
        before=struct.pack('<I',2)
        raw=before+b'\0'*(44*4)
        reads=iter([before,struct.pack('<I',4)+raw[4:]])
        self.assertIsNone(read_coop_cgame(lambda a,n: next(reads),100,0))
        reads=iter([before,raw,struct.pack('<I',3)])
        self.assertIsNone(read_coop_cgame(lambda a,n: next(reads),100,0))



class ModelDraws(unittest.TestCase):
    def test_expanded_schema_and_torn_publication(self):
        words=[12,2,456,80]+[0]*2056
        words[4+255*8:12+255*8]=[7,0x20000,5,20,30,400,80,0x30000]
        words[-8:]=[0xffffffff,0,9,40,60,800,90,0]
        raw=struct.pack('<2060I',*words)
        def read(address,size):
            if address==100:return raw[:size]
            return {0x20000:b'material',0x30000:b'model'}.get(address,b'').ljust(size,b'\0')
        result=read_model_draws(read,100,0)
        self.assertEqual(result['schema'],2)
        self.assertEqual(sum(r['calls'] for r in result['rows']),14)
        self.assertEqual(result['rows'][0]['model'],'model')
        self.assertFalse(result['rows'][0]['overflow'])
        self.assertTrue(result['rows'][1]['overflow'])
        def changed(address,size):
            if size==2060*4:return struct.pack('<I',13)+raw[4:]
            return read(address,size)
        self.assertIsNone(read_model_draws(changed,100,0))
        self.assertIsNone(read_model_draws(lambda a,n: read(a,n-1) if n==2060*4 else read(a,n),100,0))

    def fixture(self):
        words=[11,1,321,90]+[0]*512
        words[4:12]=[7,0x20000,5,20,30,400,80,0x30000]
        words[-8:]=[0xffffffff,0,9,40,60,800,90,0]
        raw=struct.pack('<516I',*words)
        def read(address,size):
            if address==100:return raw[:size]
            return {0x20000:b'material',0x30000:b'model'}.get(address,b'').ljust(size,b'\0')
        return raw,read
    def test_names_and_overflow(self):
        raw,read=self.fixture();r=read_model_draws(read,100,0)
        self.assertEqual(r['MainLoopCount'],321)
        self.assertEqual(r['rows'][0]['model'],'model')
        self.assertEqual(r['rows'][0]['shader'],'material')
        self.assertEqual(sum(x['calls'] for x in r['rows']),14)
        self.assertTrue(r['rows'][1]['overflow'])
        self.assertIsNone(r['rows'][1]['model'])
        self.assertIsNone(read_model_draws(read,100,11))
        self.assertIsNone(read_model_draws(read,None,0))
    def test_torn_short_schema_and_invalid_names(self):
        raw,read=self.fixture()
        self.assertIsNone(read_model_draws(lambda a,n: raw if n>4 else struct.pack('<I',12),100,0))
        self.assertIsNone(read_model_draws(lambda a,n:raw[:n-1],100,0))
        bad=raw[:4]+struct.pack('<I',2)+raw[8:]
        self.assertIsNone(read_model_draws(lambda a,n:bad[:n],100,0))
        r=read_model_draws(lambda a,n:read(a,n) if a==100 else b'x'*n,100,0)
        self.assertIsNone(r['rows'][0]['model'])

class DrawKinds(unittest.TestCase):
    def test_complete_publication_and_old_binary(self):
        words=[7,1,123,44]+list(range(48))
        raw=struct.pack('<52I',*words)
        read=lambda address,size:raw[:size]
        record=read_draw_kinds(read,100,0)
        self.assertEqual(record['kinds']['fx']['reserved_dwords'],47)
        self.assertEqual(record['MainLoopCount'],123)
        self.assertIsNone(read_draw_kinds(read,100,7))
        self.assertIsNone(read_draw_kinds(read,None,0))
    def test_torn_partial_and_unpublished(self):
        raw=struct.pack('<52I',7,1,123,44,*range(48))
        self.assertIsNone(read_draw_kinds(lambda a,n:raw if n>4 else struct.pack('<I',8),100,0))
        self.assertIsNone(read_draw_kinds(lambda a,n:b'\0'*n,100,0))
        self.assertIsNone(read_draw_kinds(lambda a,n:raw[:n-1],100,0))


class Translation(unittest.TestCase):
    def setUp(self):
        self.ram = bytearray(0x10000)
        self.put(0x1004, 0x2001)  # 0x00400000 directory entry
        self.put(0x2000, 0x5001)
        self.put(0x2004, 0x8001)

    def put(self, at, value):
        struct.pack_into('<I', self.ram, at, value)

    def read(self, at, size):
        return bytes(self.ram[at:at+size])

    def test_regular_and_absent(self):
        self.assertEqual(translate(self.read, 0x1000, 0x400123), 0x5123)
        self.assertIsNone(translate(self.read, 0x1000, 0x402000))
        self.assertIsNone(translate(self.read, 0x1000, 0x800000))
        self.assertIsNone(translate(lambda a,n:b'', 0x1000, 0x400000))

    def test_large_page(self):
        self.put(0x1004, 0x00800081)
        self.assertEqual(translate(self.read, 0x1000, 0x456789), 0x856789)

    def test_cross_page_and_remapping(self):
        recorder = Recorder.__new__(Recorder)
        recorder.cr3, recorder.physical = 0x1000, self.read
        self.ram[0x5ffe:0x6000] = b'ab'
        self.ram[0x8000:0x8002] = b'cd'
        self.assertEqual(recorder.virtual(0x400ffe, 4), b'abcd')
        self.put(0x2004, 0x9001)
        self.ram[0x9000:0x9002] = b'ef'
        self.assertEqual(recorder.virtual(0x400ffe, 4), b'abef')
        self.put(0x2004, 0)
        self.assertEqual(recorder.virtual(0x400ffe, 4), b'')


class FpsPublication(unittest.TestCase):
    def fixture(self, index):
        memory = bytearray(64 * 576)
        for serial in range(max(0, index - 64), index):
            line = ('STEFX_HW_FPS_SAMPLE: serial=%d ft=10/5000/500/500/500/10' % (serial + 1)).encode()
            memory[serial % 64 * 576:serial % 64 * 576 + len(line)] = line
        def read(address, size):
            return struct.pack('<I', index) if address == 999999 else memory[address:address + size]
        return memory, read

    def test_initial_and_incremental(self):
        _, read = self.fixture(3)
        self.assertEqual([r['publication'] for r in read_fps_profiles(read, 0, 999999, 0)['records']], [1, 2, 3])
        self.assertEqual(len(read_fps_profiles(read, 0, 999999, 2)['records']), 1)
        self.assertIsNone(read_fps_profiles(read, 0, 999999, 3))
        self.assertIsNone(read_fps_profiles(read, 0, 999999, 4))

    def test_wrap_excludes_slot_writer_can_replace(self):
        memory, read = self.fixture(70)
        memory[6 * 576:7 * 576] = b'x' * 576
        result = read_fps_profiles(read, 0, 999999, 0)
        self.assertEqual(result['dropped_records'], 7)
        self.assertEqual([r['publication'] for r in result['records']], list(range(8, 71)))

    def test_torn_and_partial_reads(self):
        _, read = self.fixture(3)
        calls = 0
        def torn(address, size):
            nonlocal calls
            if address == 999999:
                calls += 1
                if calls > 1:
                    return struct.pack('<I', 4)
            return read(address, size)
        self.assertIsNone(read_fps_profiles(torn, 0, 999999, 0))
        self.assertIsNone(read_fps_profiles(lambda a, n: read(a, n)[:-1], 0, 999999, 0))


class FramePublication(unittest.TestCase):
    def test_completed_and_duplicate(self):
        words = [17, 1] + list(range(len(FRAME_COST_FIELDS) - 2))
        raw = struct.pack('<%dI' % len(words), *words)
        read = lambda address, size: raw[:size]
        result = read_frame_cost(read, 0x1000, 16)
        self.assertEqual(result['sample'], 17)
        self.assertEqual(result['MainLoopCount'], 0)
        self.assertIsNone(read_frame_cost(read, 0x1000, 17))

    def test_rejects_partial_torn_and_wrong_schema(self):
        raw = struct.pack('<%dI' % len(FRAME_COST_FIELDS), 17, 1,
                          *([9] * (len(FRAME_COST_FIELDS) - 2)))
        self.assertIsNone(read_frame_cost(lambda a, n: raw[:n-1], 0x1000, 0))
        for serial in (0, 18):
            read = lambda a, n: struct.pack('<I', serial) if n == 4 else raw
            self.assertIsNone(read_frame_cost(read, 0x1000, 0))
        bad = raw[:4] + struct.pack('<I', 2) + raw[8:]
        self.assertIsNone(read_frame_cost(lambda a, n: bad[:n], 0x1000, 0))

    def test_writer_schema_matches_reader(self):
        import re
        source = (Path(__file__).resolve().parents[2] / 'code/win32/xb_log.cpp').read_text(encoding='utf-8')
        fields = {int(slot): name.removesuffix('Current') for slot, name in
                  re.findall(r'g_SPXBPerfFrameSnapshot\[(\d+)\] = g_SPXB(\w+);', source)}
        for slot, name in enumerate(FRAME_COST_FIELDS[2:], 2):
            self.assertEqual(fields[slot], name)
        self.assertEqual(len(FRAME_COST_FIELDS), 39)


class CoopPlayerPublication(unittest.TestCase):
    def test_complete_and_invalid(self):
        from xemu_flight_recorder import read_coop_players
        words = [2, 1, 10, 500, 3, 3, 77, 0] + [0] * 512
        words[8 + 2 * 0x81] = 77
        words[9 + 2 * 0x81] = 3
        raw = struct.pack('<520I', *words)
        read = lambda a, n: raw[:n]
        result = read_coop_players(read, 123, 0)
        self.assertEqual((result['actors'], result['cycles'][0x81], result['visits'][0x81]), (3, 77, 3))
        self.assertIsNone(read_coop_players(read, 123, 2))
        self.assertIsNone(read_coop_players(read, None, 0))
        self.assertIsNone(read_coop_players(lambda a, n: raw[:n-1], 123, 0))
        for slot, value in [(0, 0), (0, 3), (1, 2), (5, 2), (6, 78), (7, 1)]:
            bad = list(words); bad[slot] = value
            payload = struct.pack('<520I', *bad)
            self.assertIsNone(read_coop_players(lambda a, n: payload[:n], 123, 0))
        for change_on in [2, 3]:
            calls = [0]
            def torn(a, n):
                calls[0] += 1
                return (struct.pack('<I', 4) + raw[4:])[:n] if calls[0] >= change_on else raw[:n]
            self.assertIsNone(read_coop_players(torn, 123, 0))


class MdrKernelPublication(unittest.TestCase):
    def test_completed_reject_duplicate_partial_and_racing(self):
        from xemu_flight_recorder import read_mdr_kernel
        raw=struct.pack('<11I',2,1,100,444,16000,194,0,9000,200,300,400)
        result=read_mdr_kernel(lambda a,n:raw[:n],123,0)
        self.assertEqual((result['sample'],result['skin_cycles']),(100,400))
        self.assertIsNone(read_mdr_kernel(lambda a,n:raw[:n],123,2))
        self.assertIsNone(read_mdr_kernel(lambda a,n:raw[:n],None,0))
        for payload in [raw[:-1],struct.pack('<I',3)+raw[4:],struct.pack('<2I',2,2)+raw[8:]]:
            self.assertIsNone(read_mdr_kernel(lambda a,n:payload[:n],123,0))
        for change_on in [2,3]:
            calls=[0]
            def torn(a,n):
                calls[0]+=1
                return ((struct.pack('<I',4)+raw[4:]) if calls[0]>=change_on else raw)[:n]
            self.assertIsNone(read_mdr_kernel(torn,123,0))


class CoopMdrSimdCounters(unittest.TestCase):
    def test_fields_float_errors_and_short_reads(self):
        from xemu_flight_recorder import read_coop_mdr_simd
        raw=struct.pack('<7I2f7I',2,100,0,1234,1230,4,0,0.00001,0.0000001,32,0,0,0,0,0,0)
        result=read_coop_mdr_simd(lambda a,n:raw[:n],123)
        self.assertEqual((result['compared'],result['roundoff'],result['failed']),(1234,4,0))
        self.assertAlmostEqual(result['maximum_position_error'],0.00001)
        self.assertAlmostEqual(result['maximum_normal_error'],0.0000001)
        self.assertIsNone(read_coop_mdr_simd(lambda a,n:raw[:n],None))
        self.assertIsNone(read_coop_mdr_simd(lambda a,n:raw[:-1],123))


class CoopLightCounters(unittest.TestCase):
    def test_counter_layout_and_short_reads(self):
        from xemu_flight_recorder import read_coop_light
        raw=struct.pack('<16I',1,100,0,100,0,0,0,0,0,12345,10,2345,200,400,0,0)
        result=read_coop_light(lambda a,n:raw[:n],123)
        self.assertEqual((result['combined_set_calls'],result['avoided_enable_calls']),(100,400))
        self.assertEqual((result['sampled_cycles'],result['sampled_calls']),(12345,10))
        self.assertIsNone(read_coop_light(lambda a,n:raw[:n],None))
        self.assertIsNone(read_coop_light(lambda a,n:raw[:-1],123))


if __name__ == '__main__':
    unittest.main()
