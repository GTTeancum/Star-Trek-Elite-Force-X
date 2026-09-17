"""Read XEMU 0.8.134's existing video-profiler data without UI or mutation.

ABI verified against the pinned executable's FPS/MSPF string references and
upstream fc9980d2962cbec656253106ea2e121fab1e68d4:
hw/xbox/nv2a/debug.h, pgraph/profile.c, and ui/xui/debug.cc.
MSPF is the interval measured by profile_flip_stall, not reciprocal FPS.
"""
import ctypes
import hashlib
from pathlib import Path
import struct

XEMU_SHA256 = 'f59a9df35f7d3be20da091d14066d4670e97ec9ee27ddf1d9d4b1abf99d2fe72'
# Isolated diagnostic copy produced by make_xemu_lru_probe.py. Only three
# verified sites in the LRU function and the PE checksum differ; layout is
# unchanged. Keep the variant explicit in every emitted identity.
LRU_PROBE_SHA256 = '0541e4828a0e66d5057180b5a2a34a775543fc3b784dabc8f5cd92802b8cfb82'
STATS_RVA = 0x14a0100
CAPACITY, STRIDE, HISTORY_OFFSET, POINTER_OFFSET, SIZE = 300, 184, 200, 55400, 55408
COUNTERS = '''FINISH_VERTEX_BUFFER_DIRTY FINISH_SURFACE_CREATE FINISH_SURFACE_DOWN
FINISH_NEED_BUFFER_SPACE FINISH_FRAMEBUFFER_DIRTY FINISH_PRESENTING FINISH_FLIP_STALL
FINISH_FLUSH FINISH_STALLED CLEAR QUEUE_SUBMIT QUEUE_SUBMIT_AUX PIPELINE_NOTDIRTY
PIPELINE_GEN PIPELINE_BIND PIPELINE_RENDERPASSES BEGIN_ENDS DRAW_ARRAYS INLINE_BUFFERS
INLINE_ARRAYS INLINE_ELEMENTS QUERY SHADER_GEN SHADER_BIND SHADER_BIND_NOTDIRTY
SHADER_UBO_DIRTY SHADER_UBO_NOTDIRTY ATTR_BIND TEX_UPLOAD GEOM_BUFFER_UPDATE_1
GEOM_BUFFER_UPDATE_2 GEOM_BUFFER_UPDATE_3 GEOM_BUFFER_UPDATE_4 GEOM_BUFFER_UPDATE_4_NOTDIRTY
SURF_SWIZZLE SURF_CREATE SURF_DOWNLOAD SURF_UPLOAD SURF_TO_TEX SURF_TO_TEX_FALLBACK
QUEUE_SUBMIT_1 QUEUE_SUBMIT_2 QUEUE_SUBMIT_3 QUEUE_SUBMIT_4 QUEUE_SUBMIT_5'''.split()
assert len(COUNTERS) == 45


def decode_completed_frames(raw, last_sequence=None):
    if len(raw) != SIZE:
        return None
    flip_us, count, fps = struct.unpack_from('<qII', raw)
    pointer, = struct.unpack_from('<I', raw, POINTER_OFFSET)
    if pointer >= CAPACITY or pointer != count % CAPACITY or fps > 10000 or flip_us < 0:
        return None
    if last_sequence is not None and count <= last_sequence:
        return None  # Duplicate, reset, or uint32 wrap: don't invent continuity.
    # The writer may be replacing the oldest slot BEFORE advancing its counter.
    # Exclude that slot even when before/after counter reads match.
    oldest = max(0, count - (CAPACITY - 1))
    start = oldest if last_sequence is None else max(oldest, last_sequence + 1)
    frames = []
    for sequence in range(start, count):
        row = struct.unpack_from('<46i', raw, HISTORY_OFFSET + (sequence % CAPACITY) * STRIDE)
        if any(value < 0 for value in row):
            return None
        frames.append({'sequence': sequence, 'mspf': row[0], 'counters': list(row[1:])})
    return {'frame_count': count, 'increment_fps': fps, 'last_flip_us': flip_us,
            'frames': frames,
            'dropped_records': max(0, oldest - (last_sequence + 1)) if last_sequence is not None else 0}


def read_stable_snapshot(read, last_sequence=None):
    before = read(8, 4)
    raw = read(0, SIZE)
    after = read(8, 4)
    if len(before) != 4 or len(after) != 4 or len(raw) != SIZE:
        return None
    if before != after or before != raw[8:12]:
        return None
    return decode_completed_frames(raw, last_sequence)


class NativeVideoStats:
    def __init__(self, pid):
        import psutil
        self.path = Path(psutil.Process(pid).exe())
        self.sha256 = hashlib.sha256(self.path.read_bytes()).hexdigest()
        if self.sha256 not in (XEMU_SHA256, LRU_PROBE_SHA256):
            raise ValueError('Native video counters require the exact validated XEMU binary')
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        self.api.OpenProcess.restype = ctypes.c_void_p
        self.api.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                              ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.api.CloseHandle.argtypes = [ctypes.c_void_p]
        self.handle = self.api.OpenProcess(0x410, False, pid)  # query + read only
        if not self.handle:
            raise OSError(ctypes.get_last_error(), 'Read-only OpenProcess failed')
        try:
            psapi = ctypes.WinDLL('psapi', use_last_error=True)
            psapi.EnumProcessModules.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                                 ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong)]
            modules = (ctypes.c_void_p * 1024)()
            needed = ctypes.c_ulong()
            if not psapi.EnumProcessModules(self.handle, modules, ctypes.sizeof(modules), ctypes.byref(needed)):
                raise OSError(ctypes.get_last_error(), 'EnumProcessModules failed')
            self.base = modules[0]
            if not self.base:
                raise ValueError('Missing target executable module')
        except Exception:
            self.close()
            raise
        self.last_sequence = None

    def read(self, offset, size):
        if offset < 0 or size <= 0 or offset + size > SIZE:
            return b''
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        ok = self.api.ReadProcessMemory(self.handle, self.base + STATS_RVA + offset,
                                        buffer, size, ctypes.byref(got))
        return buffer.raw if ok and got.value == size else b''

    def snapshot(self):
        result = read_stable_snapshot(self.read, self.last_sequence)
        if result and result['frames']:
            self.last_sequence = result['frames'][-1]['sequence']
        return result

    def identity(self):
        return {'path': str(self.path), 'sha256': self.sha256, 'stats_rva': STATS_RVA,
                'binary_variant': 'original_0.8.134' if self.sha256 == XEMU_SHA256 else 'isolated_lru_guard_probe',
                'module_base': self.base, 'counter_names': COUNTERS,
                'source_revision': 'fc9980d2962cbec656253106ea2e121fab1e68d4',
                'limitations': 'Existing XEMU profiler; asynchronous FPS field, MSPF is not full frame time or retail timing.'}

    def close(self):
        if self.handle:
            self.api.CloseHandle(self.handle)
            self.handle = None
