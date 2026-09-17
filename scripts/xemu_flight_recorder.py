"""Read-only, bounded live guest-state sampling through XEMU's RAM mapping.

No RAM scan, desktop input, pause, or emulator mutation. The monitor supplies
the RAM mapping and CR3 once; page tables are reread for every sample. Samples
are asynchronous observations, not atomic per-frame timings or FPS acceptance.
"""
import ctypes
import os
import time
import json
import re
import struct
from pathlib import Path


SYMBOLS = ['_g_SPXB'+suffix for suffix in '''
HeartbeatMagic HeartbeatFrame HeartbeatRealtime HeartbeatServerTime MainLoopCount
ComFrameCount SvFrameCount ClFrameCount ClsState ComSubphase ComTailStage ClTailStage
MainTailStage RenderListStage RenderListShader RenderListEntity RenderListTessVerts
RenderListTessIndexes EndSurfaceStage NativeSubmitStage NativeSubmitReserve
NativeSubmitStreams PerfFrameMsec PerfServerMsec PerfClientMsec PerfRenderTotalMsec
PerfRenderWorldMsec PerfRenderEntitiesMsec PerfRenderSortMsec PerfRenderViews
PerfRenderDrawSurfs PerfRenderRefEntities PerfBackendBatches PerfBackendVertexes
PerfBackendIndexes PerfBackendDrawSurfsMsec PerfFinishMsec PerfPresentMsec
PerfSampleActive PerfSampleSerial PerfSubmitCallsCurrent PerfDrawReserveCyclesCurrent
PerfDrawBeginPushCyclesCurrent PerfDrawBeginPushMaxCyclesCurrent PerfDrawBeginPushMaxDwordsCurrent
PerfDrawPackCyclesCurrent PerfDrawStateCyclesCurrent PerfIndexedBlendCallsCurrent
PerfIndexedOpaqueCallsCurrent ScratchDraws ScratchFallbacks ScratchWaitMsec
FileAllocStage FileAllocPathHash
AudioUpdateStage AudioUpdateSerial AudioLoadStage AudioLoadIndex AudioLoadHandle
PhysTotal PhysAvail QALStreamStage PerfAudioMsec HMAudioBackendState HMAudioStartSoundCount
HMAudioLoopCount HMAudioVoiceStartCount HMAudioListenerUpdateMask AudioMemUsed
'''.split()]

# Versioned completed-frame publication in xb_log.cpp, diagnostic builds only.
FRAME_COST_FIELDS = '''sample schema MainLoopCount PerfFrameMsec PerfServerMsec
PerfClientMsec PerfGameMsec PerfFrontendMsec PerfBackendMsec PerfAudioMsec
PerfScreenDrawMsec PerfEndFrameMsec PerfRenderTotalMsec PerfBackendDrawSurfsMsec
PerfBackendBatches PerfBackendVertexes PerfBackendIndexes PerfSubmitCalls
PerfDrawCycles PerfDrawReserveCycles PerfDrawBeginPushCycles PerfDrawSetStreamCycles
PerfDrawStateCycles PerfDrawPackCycles PerfDrawIndexCycles PerfDrawSubmitCycles
PerfFinishMsec PerfPresentMsec PerfDrawBeginPushMaxCycles PerfDrawBeginPushMaxDwords
PerfDrawBeginPushMaxState PerfIndexedOpaqueCalls PerfIndexedBlendCalls
PerfIndexedAlphaTestCalls PerfIndexedBlendIndexes PerfRenderDrawSurfs
PerfRenderRefEntities PerfRenderWorldMsec PerfRenderEntitiesMsec'''.split()


MEMORY_OBSERVATION_FIELDS = '''serial schema guest_ms caller physical_total
physical_free zone_size zone_used zone_overhead zone_peak zone_free free_blocks
largest_free model_md3 model_glm model_gla bsp sound_raw filesys asset_context'''.split()


def read_memory_observations(read, address, serial_address, previous):
    """Read completed zone-mutex publications; report overwritten observations."""
    if address is None or serial_address is None:
        return None
    before = read(serial_address, 4)
    if len(before) != 4:
        return None
    serial, = struct.unpack('<I', before)
    if serial <= previous:
        return None
    # Exclude the oldest slot, which the writer may already be replacing.
    start = max(previous + 1, serial - 62, 1)
    records = []
    for expected in range(start, serial + 1):
        slot = address + (expected - 1) % 64 * 80
        raw = read(slot, 80)
        if len(raw) != 80:
            return None
        words = struct.unpack('<20I', raw)
        if words[0] != expected or words[1] not in (1, 2) or read(slot, 4) != raw[:4]:
            return None
        records.append(dict(zip(MEMORY_OBSERVATION_FIELDS, words)))
    if read(serial_address, 4) != before:
        return None
    return {'serial': serial, 'records': records,
            'dropped_records': start - previous - 1}


def read_coop_light(read, address):
    if address is None:
        return None
    raw = read(address, 64)
    if len(raw) != 64:
        return None
    return dict(zip(['mode','requests','original_set_calls','combined_set_calls',
                     'restores','verified','mismatches','api_failures','failed',
                     'sampled_cycles','sampled_calls','maximum_cycles',
                     'avoided_set_calls','avoided_enable_calls','reserved0','reserved1'],
                    struct.unpack('<16I',raw)))


def read_coop_mdr_simd(read, address):
    if address is None:
        return None
    raw = read(address, 64)
    if len(raw) != 64:
        return None
    words = struct.unpack('<16I', raw)
    return dict(zip(['mode','surfaces','fast_vertices','compared','exact','roundoff',
                     'excessive','maximum_position_error','maximum_normal_error',
                     'maximum_ulp','uv_differences','nonfinite_differences','failed',
                     'first_failure_surface','first_failure_vertex','reserved'],
                    list(words[:7])+list(struct.unpack('<2f',raw[28:36]))+list(words[9:])))


def read_mdr_kernel(read, address, previous):
    if address is None:
        return None
    before = read(address, 4)
    if len(before) != 4:
        return None
    sequence, = struct.unpack('<I', before)
    if not sequence or sequence & 1 or sequence == previous:
        return None
    raw = read(address, 44)
    if len(raw) != 44 or raw[:4] != before or read(address, 4) != before:
        return None
    words = struct.unpack('<11I', raw)
    if words[1] != 1:
        return None
    return dict(zip(['publication','schema','sample','surfaces','vertices','keys',
                     'overflow','repeated_vertices','index_cycles','palette_cycles','skin_cycles'], words))


def read_frame_cost(read, address, previous):
    if address is None:
        return None
    raw = read(address, 4 * len(FRAME_COST_FIELDS))
    if len(raw) != 4 * len(FRAME_COST_FIELDS):
        return None
    words = struct.unpack('<%dI' % len(FRAME_COST_FIELDS), raw)
    if not words[0] or words[0] == previous or words[1] != 1:
        return None
    # Fresh read, not the page cache used for the payload. The writer clears
    # this serial before changing any field and publishes a new serial last.
    if read(address, 4) != raw[:4]:
        return None
    return dict(zip(FRAME_COST_FIELDS, words))


def read_fps_profiles(read, address, index_address, previous):
    """Keep complete FPS windows before the 64-record ring rotates away."""
    if address is None or index_address is None:
        return None
    before = read(index_address, 4)
    if len(before) != 4:
        return None
    index, = struct.unpack('<I', before)
    if index <= previous:
        return None
    # The writer can be replacing the oldest slot before incrementing index.
    start = max(previous, index - 63, 0)
    records = []
    for serial in range(start, index):
        raw = read(address + serial % 64 * 576, 576)
        if len(raw) != 576 or b'\0' not in raw:
            return None
        try:
            line = raw.split(b'\0', 1)[0].decode('ascii')
        except UnicodeError:
            return None
        if not line.startswith('STEFX_HW_FPS_SAMPLE:'):
            return None
        records.append({'publication': serial + 1, 'line': line})
    if read(index_address, 4) != before:
        return None
    return {'index': index, 'records': records, 'dropped_records': start - previous}


def read_coop_cgame(read, address, previous, version=1):
    """One completed client-frame publication; reject partial or racing updates."""
    if address is None:
        return None
    before = read(address, 4)
    if len(before) != 4:
        return None
    sequence, = struct.unpack('<I', before)
    if not sequence or sequence & 1 or sequence == previous:
        return None
    count = 51 if version == 2 else 45
    raw = read(address, count * 4)
    if len(raw) != count * 4 or raw[:4] != before or read(address, 4) != before:
        return None
    words = struct.unpack('<'+str(count)+'I', raw)
    fields = ['publication','client_frame','server_time','total_ms','setup_ms',
              'predict_ms','view_ms','entities_ms','tail_ms','draw_ms','scene_ms',
              'hud_ms','main_loop']
    result = dict(zip(fields, words[:13]))
    result['entity_cycles'] = list(words[13:29])
    result['entity_counts'] = list(words[29:45])
    result['schema'] = version
    if version == 2:
        result.update(zip(['tail_weapon_ms','tail_p2_ms','tail_listener_ms',
                           'tail_powerup_ms','tail_fx_ms','tail_misc_ms'], words[45:51]))
    return result


def read_coop_fx(read, address, previous):
    if address is None:
        return None
    before = read(address, 4)
    if len(before) != 4:
        return None
    sequence, = struct.unpack('<I', before)
    if not sequence or sequence & 1 or sequence == previous:
        return None
    raw = read(address, 294 * 4)
    if len(raw) != 294 * 4 or raw[:4] != before or read(address, 4) != before:
        return None
    words = struct.unpack('<294I', raw)
    if words[1] != 1 or words[5] > 32:
        return None
    fields = ['vtable', 'free_count', 'update_count', 'cull_count', 'draw_count',
              'free_cycles', 'update_cycles', 'cull_cycles', 'draw_cycles']
    return {'publication': sequence, 'schema': words[1], 'client_frame': words[2],
            'server_time': words[3], 'live_effects': words[4],
            'rows': [dict(zip(fields, words[6+i*9:15+i*9])) for i in range(words[5])]}


def read_coop_players(read, address, previous):
    if address is None:
        return None
    before = read(address, 4)
    if len(before) != 4:
        return None
    sequence, = struct.unpack('<I', before)
    if not sequence or sequence & 1 or sequence == previous:
        return None
    raw = read(address, 520 * 4)
    if len(raw) != 520 * 4 or raw[:4] != before or read(address, 4) != before:
        return None
    words = struct.unpack('<520I', raw)
    if words[1] != 1 or words[4] != words[5] or words[7]:
        return None
    cycles = list(words[8::2])
    if sum(cycles) & 0xffffffff != words[6]:
        return None
    return {'publication': sequence, 'schema': words[1], 'client_frame': words[2],
            'server_time': words[3], 'actors': words[4], 'total_cycles': words[6],
            'cycles': cycles, 'visits': list(words[9::2])}


DRAW_KINDS = ['hud', 'resident_world', 'dynamic_world_base', 'world_extra_pass', 'model', 'fx']
DRAW_KIND_FIELDS = ['calls', 'vertices', 'indices', 'draw_cycles', 'begin_push_cycles',
                    'pack_cycles', 'state_cycles', 'reserved_dwords']


def read_draw_kinds(read, address, previous):
    if address is None:
        return None
    raw = read(address, 52 * 4)
    if len(raw) != 52 * 4:
        return None
    words = struct.unpack('<52I', raw)
    if not words[0] or words[0] == previous or words[1] != 1:
        return None
    if read(address, 4) != raw[:4]:
        return None
    return {'sample': words[0], 'schema': words[1], 'MainLoopCount': words[2],
            'frame_guest_ms': words[3],
            'kinds': {kind: dict(zip(DRAW_KIND_FIELDS, words[4+i*8:12+i*8]))
                      for i,kind in enumerate(DRAW_KINDS)}}


def read_model_draws(read, address, previous):
    """Completed frame counts; descriptor names are asynchronous annotations."""
    if address is None:
        return None
    raw = read(address, 516 * 4)
    if len(raw) != 516 * 4:
        return None
    words = struct.unpack('<516I', raw)
    if not words[0] or words[0] == previous or words[1] not in (1, 2):
        return None
    slots = 64 if words[1] == 1 else 257
    if words[1] == 2:
        expanded = read(address, 2060 * 4)
        if len(expanded) != 2060 * 4 or expanded[:16] != raw[:16]:
            return None
        raw = expanded
        words = struct.unpack('<2060I', raw)
    if read(address, 4) != raw[:4]:
        return None
    def name(pointer):
        if not 0x10000 <= pointer <= 0x83ffffc0 or pointer % 4:
            return None
        data = read(pointer, 64)
        if len(data) != 64 or b'\0' not in data:
            return None
        value = data.split(b'\0', 1)[0]
        return value.decode('ascii') if value and all(32 <= c < 127 for c in value) else None
    rows = []
    fields = ['handle', 'shader_pointer', 'calls', 'vertices', 'indices',
              'draw_cycles', 'begin_push_cycles', 'model_pointer']
    for slot in range(slots):
        row = dict(zip(fields, words[4+slot*8:12+slot*8]))
        if not row['calls']:
            continue
        row['overflow'] = slot == slots - 1
        row['model'] = None if slot == slots - 1 else name(row['model_pointer'])
        row['shader'] = None if slot == slots - 1 else name(row['shader_pointer'])
        rows.append(row)
    return {'sample': words[0], 'schema': words[1], 'MainLoopCount': words[2],
            'frame_guest_ms': words[3], 'rows': rows, 'names_atomic': False}


def translate(read_physical, cr3, va):
    """32-bit x86 translation, including 4 MiB pages; absent pages fail closed."""
    def word(address):
        raw = read_physical(address, 4)
        return struct.unpack('<I', raw)[0] if len(raw) == 4 else None
    pde = word((cr3 & 0xfffff000) + (va >> 22) * 4)
    if pde is None or not pde & 1:
        return None
    if pde & 0x80:
        return (pde & 0xffc00000) + (va & 0x3fffff)
    pte = word((pde & 0xfffff000) + ((va >> 12) & 0x3ff) * 4)
    if pte is None or not pte & 1:
        return None
    return (pte & 0xfffff000) + (va & 0xfff)


class Recorder:
    def __init__(self, pid, prefix, map_path, resolve, monitor):
        self.monitor = monitor
        reply = monitor('gpa2hva 0')
        match = re.search(r'Host virtual address for 0x0\s+\([^)]*\) is (?:0x)?([0-9a-fA-F]+)', reply)
        if not match:
            raise RuntimeError('XEMU did not provide its guest RAM host address: '+reply[-300:])
        self.ram = int(match[1], 16)
        regs = monitor('info registers')
        match = re.search(r'CR3=([0-9a-fA-F]+)', regs)
        if not match:
            raise RuntimeError('Guest CR3 unavailable')
        self.cr3 = int(match[1], 16)
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
        self.api.OpenProcess.restype = ctypes.c_void_p
        self.api.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                             ctypes.c_void_p, ctypes.c_size_t,
                                             ctypes.POINTER(ctypes.c_size_t)]
        self.api.CloseHandle.argtypes = [ctypes.c_void_p]
        self.handle = self.api.OpenProcess(0x10 | 0x400, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), 'Read-only OpenProcess failed')
        self.symbols = {name: resolve(name, map_path) for name in SYMBOLS}
        self.symbols = {name: va for name, va in self.symbols.items() if va is not None}
        self.extra = {name: resolve(name, map_path) for name in [
            '_g_SPXBIndexStreamProof',
            '_g_SPXBMemoryObservations', '_g_SPXBMemoryObservationSerial',
            '_g_SPXBLogMirror', '_g_SPXBLogMirrorPos', '_g_SPXBFpsProfileMirror',
            '_g_SPXBFpsProfileMirrorIndex', '_g_SPXBWorldVertices', '_g_SPXBFxCull', '_g_SPXBPerfDrawKindsSnapshot', '_g_SPXBPerfModelDrawsSnapshot',
            '_g_SPXBFxMerged', '_g_SPXBInterleavedVertices', '_g_SPXBWorldArrays', '_g_SPXBPerfFrameSnapshot', '_D3D__Device',
            '?com_errorMessage@@3PADA', '?g_entities@@3PAUgentity_s@@A', '_g_SPXBSplitP2Ent', '?in_camera@@3_NA', '_g_SPXBCoopModelPvs', '_g_SPXBCoopModelArrays', '_g_SPXBCoopWorldStorage', '_g_SPXBCoopHybridIndices', '_g_SPXBCoopModelResident', '_g_SPXBCoopModelRejects', '_g_SPXBCoopCgamePhases', '_g_SPXBCoopCgamePhasesV2', '_g_SPXBCoopFxPhases', '_g_SPXBCoopFxTrace', '_g_SPXBCoopPlayerPhases', '_g_SPXBCoopShadowCache', '_g_SPXBCoopMdrSkin', '_g_SPXBMdrKernelSnapshot', '_g_SPXBCoopMdrMismatch', '_g_SPXBCoopMdrRoundoff', '_g_SPXBCoopMdrSimd', '_g_SPXBCoopLight', '?globals@@3Ugame_export_t@@A', '?level@@3Ulevel_locals_t@@A']}
        self.prefix = prefix
        self.file = open(prefix+'.flight.jsonl', 'w', encoding='utf-8', buffering=1)
        self.emit({'kind': 'setup', 'pid': pid, 'map': str(map_path), 'cr3': self.cr3,
                   'ram_host': self.ram, 'symbols': self.symbols,
                   'diagnostic_only': True, 'atomic': False})
        self.frame = None
        self.index_memory_snapshot = None
        self.memory_observation_serial = 0
        self.last_advance = None
        self.stalls = 0
        self.saved_stall = False
        self.validated = False
        self.frame_cost_serial = 0
        self.draw_kinds_serial = 0
        self.model_draws_serial = 0
        self.mdr_kernel_publication = 0
        self.coop_skin_mismatch_saved = False
        self.fps_profile_serial = 0
        self.fps_profile_next = 0
        self.coop_state_next = 0
        self.coop_npc_next = 0
        self.coop_pvs_next = 0
        self.coop_cgame_publication = 0
        self.coop_fx_publication = 0
        self.coop_player_publication = 0
        self.native_video = None
        self.native_video_next = 0
        try:
            from xemu_native_video_stats import NativeVideoStats
            self.native_video = NativeVideoStats(pid)
            self.emit({'kind': 'native_video_setup', **self.native_video.identity()})
        except Exception as error:
            self.emit({'kind': 'native_video_unavailable', 'reason': str(error)})
        self.host_process = None
        self.host_process_next = 0
        try:
            from xemu_host_process_stats import HostProcessStats
            self.host_process = HostProcessStats(pid)
        except Exception as error:
            self.emit({'kind': 'host_process_unavailable', 'reason': str(error)})

    def emit(self, record):
        self.file.write(json.dumps(record, separators=(',', ':'))+'\n')

    def physical(self, address, size):
        if address < 0 or size <= 0 or address + size > 64*1024*1024:
            return b''
        buffer = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        ok = self.api.ReadProcessMemory(self.handle, self.ram+address, buffer, size, ctypes.byref(got))
        return buffer.raw[:got.value] if ok else b''

    def virtual(self, address, size, pages=None):
        if address is None:
            return b''
        pages = {} if pages is None else pages
        data = bytearray()
        while len(data) < size:
            va = address + len(data)
            page = va & ~0xfff
            if page not in pages:
                pa = translate(self.physical, self.cr3, page)
                pages[page] = self.physical(pa, 4096) if pa is not None else b''
            raw = pages[page]
            if len(raw) != 4096:
                return b''
            amount = min(size-len(data), 4096-(va & 0xfff))
            data.extend(raw[va & 0xfff:(va & 0xfff)+amount])
        return bytes(data)

    def tick(self, elapsed):
        pages, values = {}, {}
        if elapsed >= self.fps_profile_next:
            self.fps_profile_next = elapsed + 1.0
            profiles = read_fps_profiles(self.virtual, self.extra.get('_g_SPXBFpsProfileMirror'),
                                        self.extra.get('_g_SPXBFpsProfileMirrorIndex'), self.fps_profile_serial)
            if profiles is not None:
                self.emit({'kind': 'fps_profiles', 't': elapsed, **profiles})
                self.fps_profile_serial = profiles['index']
        if self.host_process is not None and elapsed >= self.host_process_next:
            self.host_process_next = elapsed + 1.0
            try:
                self.emit({'kind': 'host_process', 't': elapsed, **self.host_process.snapshot()})
            except Exception as error:
                self.emit({'kind': 'host_process_unavailable', 't': elapsed, 'reason': str(error)})
                self.host_process = None
        if self.native_video is not None and elapsed >= self.native_video_next:
            self.native_video_next = elapsed + 0.5
            native = self.native_video.snapshot()
            if native is not None:
                self.emit({'kind': 'native_video', 't': elapsed, **native})
        memory = read_memory_observations(
            self.virtual, self.extra.get('_g_SPXBMemoryObservations'),
            self.extra.get('_g_SPXBMemoryObservationSerial'), self.memory_observation_serial)
        if memory is not None:
            self.memory_observation_serial = memory['serial']
            self.emit({'kind': 'memory_observations', 't': elapsed, **memory,
                       'continuous_extrema': False})
        index_address = self.extra.get('_g_SPXBIndexStreamProof')
        if index_address is not None:
            raw = self.virtual(index_address, 256)
            if (len(raw) == 256 and raw != self.index_memory_snapshot
                    and struct.unpack_from('<I', raw)[0] == 0x49534D31
                    and raw == self.virtual(index_address, 256)):
                self.index_memory_snapshot = raw
                self.emit({'kind': 'index_memory', 't': elapsed,
                           'words': list(struct.unpack('<64I', raw)),
                           'atomic': False, 'stable_double_read': True})
        cost = read_frame_cost(self.virtual, self.extra.get('_g_SPXBPerfFrameSnapshot'),
                               self.frame_cost_serial)
        if cost is not None:
            self.emit({'kind': 'frame_cost', 't': elapsed, 'v': cost})
            self.frame_cost_serial = cost['sample']
        kinds = read_draw_kinds(self.virtual, self.extra.get('_g_SPXBPerfDrawKindsSnapshot'),
                                self.draw_kinds_serial)
        if kinds is not None:
            self.emit({'kind': 'draw_kinds', 't': elapsed, 'v': kinds})
            self.draw_kinds_serial = kinds['sample']
        models = read_model_draws(self.virtual, self.extra.get('_g_SPXBPerfModelDrawsSnapshot'),
                                  self.model_draws_serial)
        if models:
            self.emit({'kind': 'model_draws', 't': elapsed, 'v': models})
            self.model_draws_serial = models['sample']
        kernel = read_mdr_kernel(self.virtual, self.extra.get('_g_SPXBMdrKernelSnapshot'), self.mdr_kernel_publication)
        if kernel:
            self.mdr_kernel_publication = kernel['publication']
            self.emit({'kind': 'mdr_kernel', 't': elapsed, 'v': kernel,
                       'atomic_publication': True, 'cross_record_atomic': False})
        for name, va in self.symbols.items():
            raw = self.virtual(va, 4, pages)
            if len(raw) == 4:
                values[name.removeprefix('_g_SPXB')] = struct.unpack('<I', raw)[0]
        if values.get('HeartbeatMagic') != 1212302931:
            self.emit({'kind': 'unavailable', 't': elapsed, 'reason': 'heartbeat identity'})
            return
        if elapsed >= self.coop_pvs_next and self.extra.get('_g_SPXBCoopModelPvs'):
            raw = self.virtual(self.extra['_g_SPXBCoopModelPvs'], 32)
            if len(raw) == 32:
                self.emit({'kind': 'coop_model_pvs', 't': elapsed,
                           'v': dict(zip(['mode', 'tested_models', 'outside_models',
                                          'eligible_hidden_surfaces', 'unknown_bounds',
                                          'outside_md3', 'outside_mdr', 'skipped_surfaces'],
                                         struct.unpack('<8I', raw))), 'atomic': False})
            address = self.extra.get('_g_SPXBCoopModelArrays')
            if address:
                raw_arrays = self.virtual(address, 32)
                if len(raw_arrays) == 32:
                    self.emit({'kind': 'coop_model_arrays', 't': elapsed,
                               'v': dict(zip(['mode','draws','expanded_vertices','rejected_draws',
                                              'verified_vertices','mismatches','payload_dwords','range_commands'],
                                             struct.unpack('<8I',raw_arrays))), 'atomic': False})
            address = self.extra.get('_g_SPXBCoopHybridIndices')
            if address:
                raw_hybrid = self.virtual(address, 32)
                if len(raw_hybrid) == 32:
                    self.emit({'kind': 'coop_hybrid_indices', 't': elapsed,
                               'v': dict(zip(['mode','compact_draws','source_vertices','indices',
                                              'md3_draws','mdr_draws','brush_expanded','subset_expanded'],
                                             struct.unpack('<8I',raw_hybrid))), 'atomic': False})
            address = self.extra.get('_g_SPXBCoopModelResident')
            if address:
                raw_resident = self.virtual(address, 48)
                if len(raw_resident) == 48:
                    self.emit({'kind': 'coop_model_resident', 't': elapsed,
                               'v': dict(zip(['mode','eligible','hits','fills','stored_vertices',
                                             'capacity_fallbacks','policy_rejects','verified_vertices',
                                             'mismatches','allocation_bytes','allocation_failures','saved_vertices'],
                                             struct.unpack('<12I',raw_resident))), 'atomic': False})
            address = self.extra.get('_g_SPXBCoopWorldStorage')
            cgame_v2 = self.extra.get('_g_SPXBCoopCgamePhasesV2')
            cgame = read_coop_cgame(self.virtual, cgame_v2 or self.extra.get('_g_SPXBCoopCgamePhases'),
                                   self.coop_cgame_publication, version=2 if cgame_v2 else 1)
            if cgame:
                self.coop_cgame_publication = cgame['publication']
                self.emit({'kind': 'coop_cgame_phases', 't': elapsed, 'v': cgame,
                           'atomic_publication': True, 'cross_record_atomic': False})
            trace_address = self.extra.get('_g_SPXBCoopFxTrace')
            if trace_address:
                trace_raw = self.virtual(trace_address, 48)
                if len(trace_raw) == 48:
                    self.emit({'kind': 'coop_fx_trace', 't': elapsed, 'atomic': False,
                               'v': dict(zip(['mode','queries','entity_visits','mask_rejects',
                                              'bounds_rejects','verified_rejects','mismatches',
                                              'transformed_calls','world_cycles','entity_cycles',
                                              'bounds_fills','reserved'],struct.unpack('<12I',trace_raw)))})
            fx = read_coop_fx(self.virtual, self.extra.get('_g_SPXBCoopFxPhases'), self.coop_fx_publication)
            shadow_address = self.extra.get('_g_SPXBCoopShadowCache')
            skin_address = self.extra.get('_g_SPXBCoopMdrSkin')
            mismatch_address = self.extra.get('_g_SPXBCoopMdrMismatch')
            roundoff_address = self.extra.get('_g_SPXBCoopMdrRoundoff')
            simd = read_coop_mdr_simd(self.virtual, self.extra.get('_g_SPXBCoopMdrSimd'))
            light = read_coop_light(self.virtual, self.extra.get('_g_SPXBCoopLight'))
            if light:
                self.emit({'kind': 'coop_light', 't': elapsed, 'atomic': False, 'v': light})
            if simd:
                self.emit({'kind': 'coop_mdr_simd', 't': elapsed, 'atomic': False, 'v': simd})
            if roundoff_address:
                raw_roundoff = self.virtual(roundoff_address, 40)
                if len(raw_roundoff) == 40:
                    words = struct.unpack('<10I',raw_roundoff)
                    self.emit({'kind': 'coop_mdr_roundoff', 't': elapsed, 'atomic': False,
                               'v': dict(zip(['mode','compared','exact','roundoff','excessive',
                                              'maximum_position_error','maximum_normal_error',
                                              'maximum_ulp','uv_differences','nonfinite_differences'],
                                             list(words[:5])+list(struct.unpack('<2f',raw_roundoff[20:28]))+list(words[7:])))})
            if mismatch_address and not self.coop_skin_mismatch_saved:
                raw_mismatch = self.virtual(mismatch_address, 192)
                if len(raw_mismatch) == 192 and raw_mismatch[:4] == b'\x01\x00\x00\x00':
                    self.emit({'kind': 'coop_mdr_mismatch', 't': elapsed,
                               'words': list(struct.unpack('<48I',raw_mismatch)),
                               'first_failure_persistent': True})
                    self.coop_skin_mismatch_saved = True
            if skin_address:
                raw_skin = self.virtual(skin_address, 64)
                if len(raw_skin) == 64:
                    self.emit({'kind': 'coop_mdr_skin', 't': elapsed, 'atomic': False,
                               'v': dict(zip(['mode','requests','hits','fills','reused_vertices',
                                              'capacity_fallbacks','allocation_failures','verified_vertices',
                                              'mismatches','allocation_bytes','pose_hits','pose_fills',
                                              'palette_bytes','resets','failed','view_rejections'],
                                             struct.unpack('<16I',raw_skin)))})
            if shadow_address:
                raw_shadow = self.virtual(shadow_address, 48)
                if len(raw_shadow) == 48:
                    self.emit({'kind': 'coop_shadow_cache', 't': elapsed, 'atomic': False,
                               'v': dict(zip(['mode','queries','hits','misses','stores','admission_rejects',
                                              'verified','mismatches','avoided_fragments','avoided_points',
                                              'cache_bytes','invalidations'], struct.unpack('<12I',raw_shadow)))})
            if fx:
                self.coop_fx_publication = fx['publication']
                self.emit({'kind': 'coop_fx_phases', 't': elapsed, 'v': fx,
                           'atomic_publication': True, 'cross_record_atomic': False})
            players = read_coop_players(self.virtual, self.extra.get('_g_SPXBCoopPlayerPhases'), self.coop_player_publication)
            if players:
                self.coop_player_publication = players['publication']
                self.emit({'kind': 'coop_player_phases', 't': elapsed, 'v': players,
                           'atomic_publication': True, 'cross_record_atomic': False})
            rejects_address = self.extra.get('_g_SPXBCoopModelRejects')
            if rejects_address:
                raw_rejects = self.virtual(rejects_address, 32)
                if len(raw_rejects) == 32:
                    self.emit({'kind': 'coop_model_rejects', 't': elapsed,
                               'v': dict(zip(['provenance','animation','entity','flags',
                                             'shader','stage','color','uv'],
                                            struct.unpack('<8I',raw_rejects))), 'atomic': False})
            if address:
                raw_storage = self.virtual(address, 32)
                if len(raw_storage) == 32:
                    record = {'kind': 'coop_world_storage', 't': elapsed,
                              'v': dict(zip(['mode','borrowed','scratch_buffers','physical_bytes',
                                             'resident_address','resident_capacity_bytes','metadata_bytes','ownership_changes'],
                                            struct.unpack('<8I',raw_storage))), 'atomic': False}
                    for symbol, label in [('_g_SPXBWorldVertices','world_vertices'),
                                          ('_g_SPXBWorldArrays','world_arrays')]:
                        raw_world = self.virtual(self.extra.get(symbol), 32)
                        if len(raw_world) == 32:
                            record[label] = list(struct.unpack('<8I', raw_world))
                    self.emit(record)
            self.coop_pvs_next = elapsed + 1.0
        if elapsed >= self.coop_state_next:
            self.coop_state_next = elapsed + 1.0
            from xemu_coop_state import read_players
            players = read_players(self.virtual,
                self.extra.get('?g_entities@@3PAUgentity_s@@A'), self.extra.get('_g_SPXBSplitP2Ent'))
            if players is not None:
                camera = self.virtual(self.extra.get('?in_camera@@3_NA'), 1)
                self.emit({'kind': 'coop_players', 't': elapsed, 'players': players,
                           'camera': bool(camera[0]) if len(camera) == 1 else None,
                           'mainloop': values.get('MainLoopCount'), 'atomic': False})
        if os.environ.get('STEFX_NPC_CENSUS') == '1' and elapsed >= self.coop_npc_next:
            self.coop_npc_next = elapsed + 3.0
            from xemu_coop_npcs import read_npcs
            census_start = time.perf_counter()
            census = read_npcs(self.virtual, self.extra.get('?g_entities@@3PAUgentity_s@@A'),
                              self.extra.get('?globals@@3Ugame_export_t@@A'),
                              self.extra.get('?level@@3Ulevel_locals_t@@A'))
            if census is not None:
                self.emit({'kind': 'coop_npcs', 't': elapsed,
                           'read_host_seconds': time.perf_counter() - census_start, **census})
        # Read-only SDK 5558 diagnosis. Public PUT/THRESHOLD offsets come from
        # D3D8.h; remaining offsets were checked against the linked 5558
        # MakeRequestedSpace/GpuGet implementations. Never follow MMIO pointers.
        device = self.virtual(self.extra.get('_D3D__Device'), 0x60, pages)
        if len(device) == 0x60:
            d = struct.unpack('<24I', device)
            base, end, put, threshold, cached_get = d[9], d[10], d[0], d[1], d[22]
            if (0x80000000 <= base < end <= 0x84000000 and
                    base <= put <= end and base <= threshold <= end):
                fence = self.virtual(d[12], 4, pages)
                self.emit({'kind': 'd3d_queue', 't': elapsed,
                           'mainloop': values.get('MainLoopCount'),
                           'put': put, 'threshold': threshold, 'base': base, 'end': end,
                           'cached_get': cached_get, 'software_fence': d[11],
                           'completed_fence': struct.unpack('<I', fence)[0] if len(fence) == 4 else None})
        if not self.validated:
            # Validate the independently translated RAM address against XEMU.
            va = self.symbols['_g_SPXBHeartbeatMagic']
            reply = self.monitor('gva2gpa 0x%x' % va)
            match = re.search(r'(?i)gpa\s*:\s*0x([0-9a-f]+)', reply)
            expected = translate(self.physical, self.cr3, va)
            if not match or int(match[1], 16) != expected:
                raise RuntimeError('Flight recorder guest mapping validation failed: '+reply[-250:])
            self.validated = True
        frame = values.get('MainLoopCount')
        if frame != self.frame:
            self.frame, self.last_advance, self.saved_stall = frame, elapsed, False
        age = elapsed-self.last_advance if self.last_advance is not None else 0
        self.emit({'kind': 'sample', 't': elapsed, 'frame_age': age, 'v': values})
        if age >= 2 and not self.saved_stall and self.stalls < 3:
            self.saved_stall = True
            self.stalls += 1
            stem = self.prefix+'_stall%d' % self.stalls
            regs = self.monitor('info registers')
            Path(stem+'.registers.txt').write_text(regs)
            match = re.search(r'ESP=([0-9a-fA-F]+)', regs)
            if match:
                Path(stem+'.stack.bin').write_bytes(self.virtual(int(match[1], 16), 256))
            for name, size in [('_g_SPXBLogMirror',32768),('_g_SPXBLogMirrorPos',4),
                               ('_g_SPXBFpsProfileMirror',36864),('_g_SPXBFpsProfileMirrorIndex',4),
                               ('_g_SPXBWorldVertices',32),('_g_SPXBWorldArrays',32),('_g_SPXBInterleavedVertices',32),('_g_SPXBFxCull',12),
                               ('_g_SPXBFxMerged',4),('?com_errorMessage@@3PADA',4096)]:
                data = self.virtual(self.extra.get(name), size)
                if data:
                    Path(stem+'.'+re.sub(r'[^a-zA-Z0-9]', '', name)+'.bin').write_bytes(data)
            shader = values.get('RenderListShader', 0)
            shader_name = self.virtual(shader, 64).split(b'\0',1)[0].decode('ascii', errors='replace') if 0x10000 <= shader < 0x80000000 else ''
            self.emit({'kind':'stall_capture','t':elapsed,'prefix':stem,'shader':shader_name,'v':values})

    def close(self):
        if self.native_video is not None:
            self.native_video.close()
        self.file.close()
        self.api.CloseHandle(self.handle)

    def capture_registers(self, text, elapsed):
        registers = {name: int(value, 16) for name, value in
                     re.findall(r'\b(EIP|ESP|EBP|EAX|EBX|ECX|EDX|ESI|EDI|CR3)=([0-9a-fA-F]+)', text)}
        if 'CR3' in registers:
            self.cr3 = registers['CR3']
        raw = self.virtual(registers.get('ESP'), 256)
        stack = list(struct.unpack('<64I', raw)) if len(raw) == 256 else []
        self.emit({'kind': 'registers', 't': elapsed, 'r': registers, 'stack': stack})
