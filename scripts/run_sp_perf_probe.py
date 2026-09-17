"""Run a bounded SP map probe with temporary ISO entries and no desktop input."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import psutil
from run_vo_audio_proof import root_entry

ROOT = Path(__file__).resolve().parents[1]


def parse_frame_times(line):
    match = re.search(r'\bft=(\d+(?:/\d+){4,5})(?=\s|$)', line)
    if not match:
        return None
    values = list(map(int, match[1].split('/')))
    elapsed = None
    if len(values) == 6:
        count, elapsed, maximum, p95, p99, over50 = values
        if not elapsed or maximum > elapsed:
            return None
    else:
        count, maximum, p95, p99, over50 = values
    if not count or not (0 <= p95 <= p99 <= maximum and over50 <= count):
        return None
    return {'frames': count, 'max_guest_ms': maximum,
            'elapsed_guest_ms': elapsed,
            'elapsed_guest_fps': count * 1000 / elapsed if elapsed else None,
            'p95_upper_guest_ms': p95, 'p99_upper_guest_ms': p99,
            'frames_over_50_guest_ms': over50}


def read_dump_words(paths, count):
    """A missing/short emulator dump is absent evidence, never zero counters."""
    if not paths:
        return None
    data = paths[-1].read_bytes()
    if len(data) != count * 4:
        return None
    return list(struct.unpack('<%dI' % count, data))


def read_current_fps_ring(data, index_data):
    """Read only the live, paused game's FPS ring, never arbitrary RAM strings."""
    capacity, stride = 64, 576
    if len(data) != capacity * stride or len(index_data) != 4:
        raise ValueError('Missing or incomplete current FPS ring')
    index, = struct.unpack('<I', index_data)
    lines = []
    for serial in range(max(0, index-capacity), index):
        slot = data[(serial % capacity)*stride:((serial % capacity)+1)*stride]
        if b'\0' not in slot:
            raise ValueError('Unterminated current FPS record')
        line = slot.split(b'\0', 1)[0].decode('ascii')
        if not line.startswith('STEFX_HW_FPS_SAMPLE:'):
            raise ValueError('Invalid current FPS record')
        lines.append(line)
    return lines


def parse_long_frames(words):
    if words is None or len(words) != 129:
        return None
    version = words[128] if words[128] in (2, 3) else 1
    stride, limit = {1: (4, 32), 2: (6, 21), 3: (12, 10)}[version]
    if words[0] > limit:
        return None
    records = []
    for i in range(words[0]):
        row = words[1+i*stride:1+(i+1)*stride]
        if row[1] < 250:
            return None
        record = dict(zip(('realtime_ms', 'total_ms', 'server_ms', 'client_ms',
                           'first_commands_ms', 'second_commands_ms'), row))
        if version == 3:
            record['first_command_count'], record['second_command_count'] = row[6:8]
            record['last_command_prefix'] = struct.pack('<4I', *row[8:12]).split(b'\0')[0].decode('ascii', 'replace')
        records.append(record)
    return {'version': version, 'records': records}


def open_iso_for_restore(path):
    # Windows/SMB may release the emulator's file handle just after exit.
    # Keep the recovery manifest pending if the bounded retry still fails.
    for attempt in range(11):
        try:
            return path.open('r+b')
        except PermissionError:
            if attempt == 10:
                raise
            time.sleep(1)


def reject_campaign_override_marker(iso):
    for name in ('ef_sp_smoke_harness.txt', 'ef_sp_input_replay.txt'):
        try:
            root_entry(iso, name)
        except ValueError as error:
            if str(error) == 'Root executable entry not found':
                continue
            raise
        raise ValueError('Campaign probe refuses a pre-existing diagnostic marker: ' + name)


def verify_mode(mode, observed_players, split_evidence):
    if mode == 'sp':
        return observed_players == {1}
    if mode in ('coop', 'mp'):
        players = 2 if mode == 'coop' else 4
        draws = split_evidence.get('hm_draws') or []
        done = split_evidence.get('hm_done') or []
        return (observed_players == {players} and len(draws) == len(done) == 4
            and min(draws[:players]) > 0 and min(done[:players]) >= 2
            and max(done[:players]) - min(done[:players]) <= 1
            and (mode == 'mp' or (split_evidence.get('p2_refdef_valid') == 1
                and draws[2:] == [0, 0] and done[2:] == [0, 0])))
    return False


def verify_mp_workload(result):
    """Require the observed full roster and all four movement-command lanes.

    This endpoint check supplements captures; it does not prove route coverage
    or sustained movement throughout an entire match.
    """
    state = result.get('simulation_evidence') or {}
    bots = result.get('bot_startup_evidence') or []
    commands = result.get('local_command_evidence') or {}
    serial = commands.get('hm_cmd_serial') or []
    forward = commands.get('hm_cmd_forward') or []
    right = commands.get('hm_cmd_right') or []
    return bool(state.get('connected') == 8 and state.get('playing') == 8
        and len(bots) == 32 and bots[13] >= 4 and bots[14] >= 4 and bots[15] == 4
        and len(serial) == len(forward) == len(right) == 4 and min(serial) > 20
        and all(forward[i] != 0 or right[i] != 0 for i in range(4)))


def qualifies_window(result):
    # User's beta gate is an average above 20 for MP, not a minimum of 30
    # in every five-second window. This is guest timing only, not retail proof.
    average = result.get('average_fps') or 0
    timing_pass = (math.isfinite(average) and average > 20 if result.get('mode') == 'mp'
                   else bool(result['fps']) and min(result['fps']) >= result['target_fps'])
    return bool(not result['visual_only'] and not result['diagnostic_only']
        and not result.get('god_mode_requested', False) and not result.get('god_mode', False)
        and result['mode_verified'] and result['final_client_state'] == 7
        and result.get('final_liveness_verified', False)
        and result.get('engine_error_message') == ''
        and not result.get('current_fps_error')
        and (result.get('mode') != 'mp' or (result.get('simulation_verified', False)
            and result.get('mp_workload_verified', False)))
        and result['loaded_map'] == result['map']
        and len(result['fps']) >= 2 and timing_pass)


def add_root_file(iso, name, payload):
    """Append a balanced root directory, retaining every original file entry."""
    iso.seek(32*2048+20)
    header_offset = iso.tell()
    old_header = iso.read(8)
    sector, size = struct.unpack('<II', old_header)
    iso.seek(sector*2048)
    directory = iso.read(size)
    records = {}
    pending, seen = [0], set()
    while pending:
        at = pending.pop()
        if at in seen or at+14 > len(directory):
            raise ValueError('Malformed root directory')
        seen.add(at)
        left, right, file_sector, length, attr, count = struct.unpack_from('<HHIIBB', directory, at)
        entry_name = directory[at+14:at+14+count]
        records[entry_name.lower()] = [entry_name, file_sector, length, attr]
        if left: pending.append(left*4)
        if right: pending.append(right*4)
    file_sector = (iso.seek(0, 2)+2047)//2048
    iso.seek(file_sector*2048)
    iso.write(payload)
    iso.write(b'\0' * ((-len(payload)) % 2048))
    records[name.lower().encode()] = [name.encode(), file_sector, len(payload), 0]
    # XDVDFS compares uppercase ASCII: letters precede underscores. Lowercase
    # sorting makes some files unreachable by the guest's binary tree lookup.
    # Reference: antangelo/xdvdfs, xdvdfs-core/src/layout/name.rs.
    ordered = [records[key] for key in sorted(records, key=bytes.upper)]
    nodes = []
    def build(items):
        if not items: return None
        mid = len(items)//2
        index = len(nodes)
        nodes.append([items[mid], None, None])
        nodes[index][1] = build(items[:mid])
        nodes[index][2] = build(items[mid+1:])
        return index
    build(ordered)
    offsets, size = [], 0
    for record, _, _ in nodes:
        offsets.append(size)
        size += (14+len(record[0])+3)&~3
    if size >= 0x40000:
        raise ValueError('Root directory too large')
    data = bytearray(size)
    for i, (record, left, right) in enumerate(nodes):
        entry_name, file_sector, length, attr = record
        struct.pack_into('<HHIIBB', data, offsets[i],
                         offsets[left]//4 if left is not None else 0,
                         offsets[right]//4 if right is not None else 0,
                         file_sector, length, attr, len(entry_name))
        data[offsets[i]+14:offsets[i]+14+len(entry_name)] = entry_name
    root_sector = (iso.seek(0, 2)+2047)//2048
    iso.seek(root_sector*2048)
    iso.write(data)
    iso.write(b'\0' * ((-len(data)) % 2048))
    iso.seek(header_offset)
    iso.write(struct.pack('<II', root_sector, len(data)))
    iso.flush()
    return header_offset, old_header


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso', type=Path, required=True)
    p.add_argument('--xbe', type=Path, required=True)
    p.add_argument('--symbols', type=Path, required=True)
    p.add_argument('--config', type=Path, required=True)
    p.add_argument('--hdd', type=Path, required=True)
    p.add_argument('--map', required=True)
    p.add_argument('--name', required=True)
    p.add_argument('--seconds', type=int, default=240)
    p.add_argument('--port', type=int, default=4475)
    p.add_argument('--gdb-port', type=int, help='Local guest debugger for diagnostic probes only')
    p.add_argument('--xemu-exe', type=Path, help='Pinned emulator executable for this run')
    p.add_argument('--process-priority', choices=('normal', 'above-normal'), default='normal')
    p.add_argument('--preserve-existing-artifacts', action='store_true',
                   help='Skip generated-file cleanup when preserving transferred evidence')
    p.add_argument('--visual', action='store_true')
    p.add_argument('--first-shot-delay', type=int, default=100)
    p.add_argument('--shot-interval', type=int, default=100)
    p.add_argument('--max-screenshots', type=int, default=4)
    p.add_argument('--diagnostic', action='store_true', help='Collect engine state; not a timing qualification run')
    p.add_argument('--sample-eip-interval', type=float, default=0.0,
                   help='Guest instruction sampling interval; diagnostic runs only')
    p.add_argument('--inspect-sp-facing-triggers', action='store_true')
    p.add_argument('--keep-guest-ram', action='store_true', help='Keep final paused RAM for asset-byte verification')
    p.add_argument('--flight-recorder', action='store_true', help='Read live guest state and preserve stalls; diagnostic only')
    p.add_argument('--mode', choices=['sp', 'coop', 'mp'], default='sp')
    p.add_argument('--command', action='append', default=[])
    p.add_argument('--post-command', action='append', default=[], help='Console commands queued after the normal map load')
    p.add_argument('--input-replay', type=Path, help='Bounded P1 input-only replay; visual or diagnostic SP/co-op probes only')
    p.add_argument('--god', action='store_true', help='Run the existing god command once when the SP client becomes active; diagnostic only')
    p.add_argument('--borg1-slice-warp', action='store_true',
                   help='Use the existing smoke-harness Borg benchmark start; co-op borg1 only')
    a = p.parse_args()
    if a.flight_recorder and not a.diagnostic:
        p.error('--flight-recorder requires --diagnostic')
    if (not math.isfinite(a.sample_eip_interval) or a.sample_eip_interval < 0 or
            (a.sample_eip_interval > 0 and (not a.diagnostic or a.sample_eip_interval < 0.1))):
        p.error('Instruction sampling requires --diagnostic and an interval of at least 0.1 seconds')
    if a.gdb_port and (not a.diagnostic or not 1 <= a.gdb_port <= 65535):
        p.error('Guest debugger requires --diagnostic and a valid local port')
    if a.god and (a.mode != 'sp' or not (a.visual or a.diagnostic)):
        p.error('God mode requires a visual or diagnostic SP probe')
    if a.borg1_slice_warp and (a.mode != 'coop' or a.map != 'borg1' or
                              not (a.visual or a.diagnostic)):
        p.error('Borg1 slice warp requires a visual or diagnostic borg1 co-op probe')
    replay = None
    if a.input_replay:
        if a.mode not in ('sp', 'coop') or not (a.visual or a.diagnostic):
            p.error('Input replay requires a visual or diagnostic SP/co-op probe')
        replay = a.input_replay.read_bytes()
        lines = replay.decode('ascii').splitlines()
        if not lines or lines[0].split() != ['STEFX_INPUT_REPLAY_V1', a.map] or not 1 <= len(lines)-1 <= 32:
            p.error('Invalid input replay header, map, or row count')
        last_end = 0
        for line in lines[1:]:
            row = list(map(int, line.split()))
            if len(row) != 8 or not (last_end <= row[0] < row[1] <= 600000) or any(abs(v)>127 for v in row[2:5]) or any(abs(v)>360 for v in row[5:7]) or row[7]<0 or row[7]&~161:
                p.error('Invalid input replay row')
            last_end = row[1]
    if any('stefx_diag_' in command.lower() for command in a.command + a.post_command) and not (a.visual or a.diagnostic):
        p.error('Diagnostic rendering overrides require --visual or --diagnostic; they cannot qualify performance')
    if not re.fullmatch(r'(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+', a.map) or not re.fullmatch(r'[A-Za-z0-9_]+', a.name):
        p.error('Map and name must contain only letters, digits, and underscores')
    # Validate relocated machine paths before starting an ISO transaction.
    import tomllib
    config = tomllib.loads(a.config.read_text())
    for key in ('bootrom_path', 'flashrom_path', 'eeprom_path'):
        path = Path(config.get('sys', {}).get('files', {}).get(key, ''))
        if not path.is_file():
            p.error('Missing configured %s: %s' % (key, path))
    for path in (a.hdd, a.xbe, a.symbols, a.symbols.with_suffix('.exe'),
                 a.symbols.with_suffix('.xbe')):
        if not path.is_file():
            p.error('Missing probe input or complete symbol-bundle sibling: %s' % path)
    if a.visual:
        screenshot_dir = config.get('general', {}).get('screenshot_dir')
        if not screenshot_dir:
            p.error('Native captures require general.screenshot_dir')
        Path(screenshot_dir).mkdir(parents=True, exist_ok=True)
    for proc in psutil.process_iter(['name', 'cmdline']):
        if (proc.info['name'] or '').lower().startswith('xemu'):
            if any(a.iso.name.lower() in arg.lower() for arg in proc.info['cmdline'] or []):
                raise RuntimeError('ISO is already mounted')
    cleanup = ['pwsh', '-NoProfile', '-File', str(ROOT/'scripts/cleanup_generated.ps1')]
    if not a.preserve_existing_artifacts:
        subprocess.run(cleanup, check=True)
    out = ROOT/'build/research/letterbox_perf'
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out/(a.name + '.iso_transaction.json')
    entries = []
    with a.iso.open('rb') as iso:
        if a.mode in ('sp', 'coop'):
            reject_campaign_override_marker(iso)
        length = iso.seek(0, 2)
        for name, payload in [('default.xbe', a.xbe.read_bytes())]:
            offset = root_entry(iso, name)
            iso.seek(offset)
            entries.append((name, offset, iso.read(8), payload))
    manifest = {'xbe_sha256': hashlib.sha256(entries[0][3]).hexdigest(),
                'map': a.map, 'mode': a.mode, 'seconds': a.seconds, 'visual_only': a.visual,
                'paused_during_harness_setup': True,
                'process_priority': a.process_priority,
                'diagnostic_only': a.diagnostic,
                'original_iso_bytes': length, 'restored': False,
                'entries': [{'name': n, 'offset': off, 'original': old.hex()}
                            for n, off, old, _ in entries]}
    manifest_path.write_text(json.dumps(manifest, indent=2))
    root_restore = None
    try:
        with a.iso.open('r+b') as iso:
            for _, offset, _, payload in entries:
                end = iso.seek(0, 2)
                sector = (end+2047)//2048
                iso.seek(sector*2048)
                iso.write(payload)
                iso.write(b'\0' * ((-len(payload)) % 2048))
                iso.seek(offset)
                iso.write(struct.pack('<II', sector, len(payload)))
            commands = list(a.command)
            if a.god or a.borg1_slice_warp:
                # The existing active-client command hook needs its explicit
                # diagnostic marker. Keep all other marker-gated automation off.
                commands += ['set '+name+' 0' for name in (
                    'stefx_smoke_input', 'stefx_smoke_unlock_player',
                    'stefx_smoke_ready_weapon', 'stefx_smoke_stage_enemy',
                    'stefx_smoke_aim', 'stefx_smoke_wake_ai',
                    'stefx_smoke_fasttime', 'stefx_borg1_slice_warp')]
            if a.borg1_slice_warp:
                commands.append('set stefx_borg1_slice_warp 1')
            if a.mode == 'coop':
                commands = ['set stefx_splitScreen 1', 'set stefx_splitScreenPlayers 2',
                            'set stefx_splitScreenMode coop', 'set r_splitScreenEconomy 1',
                            'set stefx_splitScreenP2Entity -1', 'set cg_virtualVoyager 0'] + commands
            if a.mode == 'mp':
                commands = ['set stefx_splitScreen 1', 'set stefx_splitScreenPlayers 4',
                            'set stefx_splitScreenMode holomatch', 'set stefx_hmLocalPlayers 4',
                            'set stefx_hmHumanPlayers 4', 'set r_splitScreenEconomy 1',
                            'set stefx_hm_split_virtual_controls 0',
                            'set stefx_hm_split_virtual_controls_p1 0',
                            'set stefx_hm_launch_source direct', 'set bot_minplayers 4',
                            'set sv_maxclients 8', 'set g_gametype 0'] + commands
            if a.map or commands or a.post_command or replay or a.god:
                # Record recovery data BEFORE updating the root descriptor.
                iso.seek(32*2048+20)
                root_restore = (32*2048+20, iso.read(8))
                manifest['root_original'] = root_restore[1].hex()
                manifest['commands'] = commands
                manifest['post_commands'] = a.post_command
                manifest['post_command_dispatch'] = 'active_client_after_god' if a.god else 'postmap'
                if replay:
                    manifest['input_replay'] = replay.decode('ascii')
                if a.god:
                    manifest['god_mode_requested'] = True
                    manifest['diagnostic_command_hook'] = True
                if a.borg1_slice_warp:
                    manifest['borg1_slice_warp_requested'] = True
                manifest_path.write_text(json.dumps(manifest, indent=2))
                add_root_file(iso, 'ef_sp_level.txt', (a.map+'\r\n').encode('ascii'))
                if commands:
                    add_root_file(iso, 'ef_sp_commands.txt', ('\n'.join(commands)+'\n').encode('ascii'))
                if a.post_command and not a.god:
                    add_root_file(iso, 'ef_sp_postmap_commands.txt', ('\n'.join(a.post_command)+'\n').encode('ascii'))
                if replay:
                    add_root_file(iso, 'ef_sp_input_replay.txt', replay)
                if a.god or a.borg1_slice_warp:
                    add_root_file(iso, 'ef_sp_smoke_harness.txt', b'1\n')
                if a.god:
                    # Keep god ahead of diagnostic waits/saves. A separate postmap
                    # queue otherwise blocks this appended command until after load.
                    active_commands = ['god'] + list(a.post_command)
                    add_root_file(iso, 'ef_sp_client_active_commands.txt', ('\n'.join(active_commands)+'\n').encode('ascii'))
                    add_root_file(iso, 'ef_sp_client_active_command_time.txt', b'0\n')
                    # Only the client hook should dispatch a command in this run.
                    add_root_file(iso, 'ef_sp_active_command_time.txt', b'2147483647\n')
        command = [sys.executable, str(ROOT/'scripts/ja_xemu_smoke.py'),
                   '--iso', str(a.iso), '--name', a.name, '--duration', str(a.seconds),
                   '--port', str(a.port), '--config-path', str(a.config), '--hdd', str(a.hdd),
                   '--runtime-xbe', str(a.xbe), '--map-file', str(a.symbols),
                   '--proof-mode', a.mode, '--proof-map', a.map, '--extract-xblog-profile', '--start-paused']
        if a.xemu_exe:
            command += ['--xemu-exe', str(a.xemu_exe.resolve())]
        command += ['--process-priority', a.process_priority]
        if a.gdb_port:
            command += ['--gdb-port', str(a.gdb_port)]
        if a.sample_eip_interval:
            command += ['--sample-eip-interval', str(a.sample_eip_interval)]
        if a.flight_recorder:
            command += ['--flight-recorder']
        if a.keep_guest_ram:
            command += ['--keep-guest-ram']
        symbols = a.symbols.read_text()
        match = re.search(r'_g_SPXBMainLoopCount\s+([0-9A-Fa-f]{8})\s', symbols)
        if match:
            command += ['--liveness-address', hex(int(match[1], 16) - 0x3f0000)]
        if a.inspect_sp_facing_triggers:
            if a.mode != 'sp' or not a.diagnostic:
                raise ValueError('Trigger inspection requires an SP diagnostic probe')
            command += ['--inspect-sp-facing-triggers']
        if a.mode == 'sp':
            match = re.search(re.escape('?g_entities@@3PAUgentity_s@@A') + r'\s+([0-9A-Fa-f]{8})\s', symbols)
            if match:
                address = int(match[1], 16) - 0x3f0000
                command += ['--dump-bin-mem', f'0x{address:x}:4:sp_entities_pointer']
        for name, label, count in [('?in_camera@@3_NA', 'camera_active', 4),
                                   ('?client_camera@@3Ucamera_s@@A', 'camera_state', 512),
                                   ('?cg_virtualVoyager@@3UvmCvar_t@@A', 'virtual_voyager_cvar', 16)]:
            match = re.search(re.escape(name)+r'\s+([0-9A-Fa-f]{8})\s', symbols)
            if not match and a.mode == 'mp':
                continue
            if not match:
                raise ValueError('Missing camera symbol: '+name)
            address = int(match[1], 16)-0x3f0000
            command += ['--dump-bin-mem', f'0x{address:x}:{count}:{label}']
        for name, label, count in [('_g_SPXBAstrometricsProof', 'astrometrics_proof', 32),
                                   ('_g_SPXBAstroHarnessProof', 'astro_harness_proof', 16),
                                   ('_g_SPXBViewAspectProof', 'view_aspect_proof', 48),
                                   ('_g_SPXBVvSaveProof', 'vv_save_proof', 16),
                                   ('_g_SPXBVvMissingModels', 'vv_missing_models', 528),
                                   ('_g_SPXBMapPhase', 'map_phase', 4),
                                   ('_g_SPXBLoadingTitleStatus', 'loading_title_status', 4),
                                   ('_g_SPXBMapLast', 'loaded_map', 64),
                                   ('?com_errorMessage@@3PADA', 'engine_error_message', 4096),
                                   ('_g_SPXBCmdTextLast', 'last_command_text', 128),
                                   ('_g_SPXBCinStatus', 'movie_status', 4),
                                   ('_g_SPXBCinBinkFrame', 'movie_frame', 4),
                                   ('_g_SPXBCinOverlayFrames', 'movie_overlay_frames', 4),
                                   ('_g_SPXBClsState', 'client_state', 4),
                                   ('_g_SPXBLogMirror', 'log_mirror', 32768),
                                   ('_g_SPXBFpsProfileMirror', 'current_fps_ring', 36864),
                                   ('_g_SPXBFpsProfileMirrorIndex', 'current_fps_index', 4),
                                   ('_g_SPXBModelRegisterStats', 'model_registration', 32),
                                   ('_g_SPXBBotDefinitionProof', 'bot_definitions', 32),
                                   ('_g_SPXBBotDefinitionText', 'bot_definition_text', 2048),
                                   ('_g_SPXBPhysTotal', 'physical_memory', 8),
                                   ('_g_SPXBIndexStreamProof', 'index_stream_memory', 256),
                                   ('_g_SPXBMemoryObservations', 'memory_observations', 5120),
                                   ('_g_SPXBMemoryObservationSerial', 'memory_observation_serial', 4),
                                   ('_g_SPXBLongFrames', 'long_frames', 516),
                                   ('_g_SPXBStasisUpload', 'stasis_upload', 1728),
                                   ('_g_SPXBStasisDraw', 'stasis_draw', 160),
                                   ('_g_SPXBStasisD3D', 'stasis_d3d', 192),
                                   ('?g_SPXBLoadTimes@@3PAIA', 'load_times', 32),
                                   ('?g_SPXBMapLoadPhases@@3PAIA', 'map_load_phases', 36),
                                   ('?g_SPXBPackedLoadPhases@@3PAIA', 'packed_load_phases', 36),
                                   ('?g_SPXBCoopSpawnStage@@3IC', 'coop_spawn_stage', 4),
                                   ('_g_SPXBScratchLayoutProof', 'scratch_layout', 16),
                                   ('_g_SPXBWorldVertices', 'world_vertices', 32),
                                   ('_g_SPXBWorldArrays', 'world_arrays', 32),
                                   ('_g_SPXBCoopModelPvs', 'coop_model_pvs', 32),
                                   ('_g_SPXBInterleavedVertices', 'interleaved_vertices', 32),
                                   ('_g_SPXBMdrSkinCache', 'mdr_skin_cache', 32),
                                   ('_g_SPXBMdrSkinMismatch', 'mdr_skin_mismatch', 128),
                                   ('_g_SPXBMdrFramePairProtected', 'mdr_frame_pair_protected', 4),
                                   ('_g_SPXBWorldRejects', 'world_rejects', 32),
                                   ('_g_SPXBFxCull', 'fx_cull', 12),
                                   ('_g_SPXBFxMerged', 'fx_merged', 4),
                                   ('_g_SPXBFxPvs', 'fx_pvs', 16),
                                   ('_g_SPXBSplitP2RefdefValid', 'p2_refdef_valid', 4)]:
            match = re.search(re.escape(name)+r'\s+([0-9A-Fa-f]{8})\s', symbols)
            if match:
                address = int(match[1], 16)-0x3f0000
                command += ['--dump-bin-mem', f'0x{address:x}:{count}:{label}']
        if a.mode == 'mp':
            # Official g_local.h level_locals_t prefix: 22 Win32 DWORDs.
            # Includes frame/time/restart and connected/playing counts.
            match = re.search(r'(?<!\S)_level\s+([0-9A-Fa-f]{8})\s', symbols)
            if not match:
                raise ValueError('Missing official simulation symbol: _level')
            address = int(match[1], 16)-0x3f0000
            command += ['--dump-bin-mem', f'0x{address:x}:88:hm_game_state']
            match = re.search(r'(?<!\S)_g_SPXBHMSplitBotProof\s+([0-9A-Fa-f]{8})\s', symbols)
            if not match:
                raise ValueError('Missing bot startup proof symbol')
            address = int(match[1], 16)-0x3f0000
            command += ['--dump-bin-mem', f'0x{address:x}:128:hm_bot_proof']
            for name, label in [('_g_SPXBHMSplitCmdSerial', 'hm_cmd_serial'),
                                ('_g_SPXBHMSplitCmdMoveX', 'hm_cmd_forward'),
                                ('_g_SPXBHMSplitCmdMoveY', 'hm_cmd_right'),
                                ('_g_SPXBHMSplitCmdButtons', 'hm_cmd_buttons')]:
                match = re.search(re.escape(name)+r'\s+([0-9A-Fa-f]{8})\s', symbols)
                if not match:
                    raise ValueError('Missing local-command proof symbol: '+name)
                address = int(match[1], 16)-0x3f0000
                command += ['--dump-bin-mem', f'0x{address:x}:16:{label}']
        if a.mode in ('coop', 'mp'):
            for name, label in [('_g_SPXBHMSplitRenderDrawDelta', 'hm_draws'),
                                ('_g_SPXBHMSplitRenderDoneSerial', 'hm_done')]:
                match = re.search(re.escape(name)+r'\s+([0-9A-Fa-f]{8})\s', symbols)
                if not match:
                    raise ValueError('Missing active renderer proof symbol: '+name)
                address = int(match[1], 16)-0x3f0000
                command += ['--dump-bin-mem', f'0x{address:x}:16:{label}']
        if a.mode == 'sp' and a.diagnostic:
            match = re.search(r'_g_SPXBVvTravelTrace\s+([0-9A-Fa-f]{8})\s', symbols)
            if match:
                address = int(match[1], 16) - 0x3f0000
                command += ['--dump-bin-mem', f'0x{address:x}:272:vv_travel_trace']
        # Log discovery can scan many addresses and monopolize the monitor.
        # Instruction profiling uses only its sampler plus the final named
        # memory dumps, so failed log discovery cannot starve the samples.
        # Both active executables use SP-engine telemetry; 'mp' selects the obsolete codemp ABI.
        if a.diagnostic and not a.sample_eip_interval:
            match = re.search(r'_g_SPXBHeartbeatMagic\s+([0-9A-Fa-f]{8})\s', symbols)
            if not match:
                raise ValueError('Missing current heartbeat symbol')
            address = int(match[1], 16) - 0x3f0000
            if (address & 0xfff) + 24 > 4096:
                raise ValueError('Heartbeat snapshot crosses a guest page')
            # Six words suffice; avoid legacy relocation scans and large ranges.
            # Final FPS records independently confirm each observed heartbeat.
            command += ['--poll-word-addr', hex(address), '--poll-word-count', '6',
                        '--poll-word-label', 'sp_heartbeat', '--poll-word-interval', '15',
                        '--poll-word-start-delay', '40', '--poll-word-remap']
        if a.visual:
            import tomllib
            screenshot_dir = tomllib.loads(a.config.read_text())['general']['screenshot_dir']
            command += ['--xemu-native-screenshots', '--first-shot-delay', str(a.first_shot_delay),
                        '--interval', str(a.shot_interval), '--max-screenshots', str(a.max_screenshots),
                        '--xemu-screenshot-dir', screenshot_dir]
        else:
            command += ['--no-screenshots']
        with (out/(a.name+'.run.log')).open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    finally:
        with open_iso_for_restore(a.iso) as iso:
            if root_restore:
                iso.seek(root_restore[0])
                iso.write(root_restore[1])
            for _, offset, original, _ in entries:
                iso.seek(offset)
                iso.write(original)
            iso.truncate(length)
            iso.flush()
            for _, offset, original, _ in entries:
                iso.seek(offset)
                assert iso.read(8) == original
            if root_restore:
                iso.seek(root_restore[0])
                assert iso.read(8) == root_restore[1]
        manifest['restored'] = True
        manifest_path.write_text(json.dumps(manifest, indent=2))
        # Retain this proof explicitly before bounded smoke output rotates.
        for artifact in (ROOT/'scripts/output').glob(a.name+'_*'):
            if artifact.is_file():
                import shutil
                shutil.copy2(artifact, out/artifact.name)
        if not a.preserve_existing_artifacts:
            subprocess.run(cleanup, check=True)
    summarize_probe(a, out, manifest, manifest_path)


def exact_probe_files(out, name):
    """Select this invocation's timestamp, never a similarly named older run."""
    from pathlib import PureWindowsPath
    reports = re.findall(r'^report=(.+\.report\.txt)$',
                         (out / (name + '.run.log')).read_text(), re.MULTILINE)
    if len(reports) != 1:
        raise ValueError('Expected exactly one completed native probe report')
    filename = PureWindowsPath(reports[0].strip()).name
    prefix = filename[:-len('.report.txt')]
    if not re.fullmatch(re.escape(name) + r'_\d{8}_\d{6}', prefix):
        raise ValueError('Native report does not identify this requested probe')
    files = tuple(out.glob(prefix + '_*'))
    return lambda suffix: [path for path in files if path.name.endswith(suffix)]


def summarize_probe(a, out, manifest, manifest_path):
    probe_files = exact_probe_files(out, a.name)
    # Never let a normal-boot movie or the borg1 wall intro count as a map proof.
    fps_buffers = sorted(probe_files('_final_current_fps_ring.bin'))
    fps_indices = sorted(probe_files('_final_current_fps_index.bin'))
    current_fps_error = None
    try:
        current_fps_lines = read_current_fps_ring(
            fps_buffers[-1].read_bytes() if fps_buffers else b'',
            fps_indices[-1].read_bytes() if fps_indices else b'')
    except (ValueError, UnicodeError) as error:
        current_fps_lines = []
        current_fps_error = str(error)
    (out/(a.name+'.current_profiles.log')).write_text('\n'.join(current_fps_lines)+'\n')
    samples, excluded, observed_players = [], 0, set()
    frame_time_windows = []
    zone_memory_samples = []
    if current_fps_lines:
        for line in current_fps_lines:
            if not line.startswith('STEFX_HW_FPS_SAMPLE:'):
                continue
            memory = re.search(r'\bmem=(\d+)/(\d+)/(\d+)/(\d+)\b', line)
            if memory:
                zone_memory_samples.append(dict(zip(
                    ('used_bytes', 'free_bytes', 'largest_free_block_bytes', 'free_blocks'),
                    map(int, memory.groups()))))
            if not re.search(r'\bgameplay=1 excludedChecks=0$', line):
                excluded += 1
                continue
            match = re.search(r'\bfps=(\d+\.\d+)\b', line)
            if match:
                samples.append(float(match[1]))
                frame_times = parse_frame_times(line)
                if frame_times:
                    frame_time_windows.append(frame_times)
                players = re.search(r'\bplayers=(\d+)\b', line)
                if players:
                    observed_players.add(int(players[1]))
    maps = sorted(probe_files('_final_loaded_map.bin'))
    loaded_map = maps[-1].read_bytes().split(b'\0', 1)[0].decode('ascii', errors='replace') if maps else None
    timings = sorted(probe_files('_final_load_times.bin'))
    load_times = read_dump_words(timings, 8)
    phases = sorted(probe_files('_final_map_load_phases.bin'))
    map_load_phases = read_dump_words(phases, 9)
    phases = sorted(probe_files('_final_packed_load_phases.bin'))
    packed_load_phases = read_dump_words(phases, 9)
    target = 45 if a.mode == 'sp' else (20 if a.mode == 'mp' else 30)
    states = read_dump_words(sorted(probe_files('_final_client_state.bin')), 1)
    client_state = states[0] if states else None
    error_dumps = sorted(probe_files('_final_engine_error_message.bin'))
    engine_error = (error_dumps[-1].read_bytes().split(b'\0', 1)[0].decode('latin1')
                    if error_dumps and error_dumps[-1].stat().st_size == 4096 else None)
    live_before = read_dump_words(sorted(probe_files('_final_liveness_before.bin')), 1)
    live_after = read_dump_words(sorted(probe_files('_final_liveness_after.bin')), 1)
    live_delta = ((live_after[0] - live_before[0]) & 0xffffffff
                  if live_before is not None and live_after is not None else None)
    player_states = sorted(probe_files('_player_state.json'))
    player_state = json.loads(player_states[-1].read_text()) if player_states else None
    split_evidence = {}
    for label in ['p2_refdef_valid']:
        dumps = sorted(probe_files('_final_'+label+'.bin'))
        words = read_dump_words(dumps, 1)
        split_evidence[label] = words[0] if words else None
    mode_verified = verify_mode(a.mode, observed_players, split_evidence)
    if a.mode in ('coop', 'mp'):
        for label in ['hm_draws', 'hm_done']:
            dumps = sorted(probe_files('_final_'+label+'.bin'))
            split_evidence[label] = read_dump_words(dumps, 4)
        mode_verified = verify_mode(a.mode, observed_players, split_evidence)
    result = {'map': a.map, 'loaded_map': loaded_map, 'mode': a.mode,
              'profile_provenance': 'current paused guest FPS ring',
              'current_fps_error': current_fps_error,
              'target_fps': target, 'gameplay_samples': len(samples),
              'excluded_or_untagged_samples': excluded, 'fps': samples,
              'average_fps': sum(samples)/len(samples) if samples else None,
              'minimum_fps': min(samples) if samples else None,
              'below_target': sum(fps < target for fps in samples),
              'frame_time_windows': frame_time_windows,
              'frame_time_summary': ({
                  'observed_frames': sum(item['frames'] for item in frame_time_windows),
                  'max_guest_frame_ms': max(item['max_guest_ms'] for item in frame_time_windows),
                  'frames_over_50_guest_ms': sum(item['frames_over_50_guest_ms'] for item in frame_time_windows)
              } if frame_time_windows else None),
              'load_times_ms': load_times, 'map_load_phases_ms': map_load_phases,
              'model_registration_stats': read_dump_words(sorted(probe_files('_final_model_registration.bin')), 8),
              'physical_memory_total_available_bytes': read_dump_words(sorted(probe_files('_final_physical_memory.bin')), 2),
              'scratch_layout_proof': read_dump_words(sorted(probe_files('_final_scratch_layout.bin')), 4),
              'world_vertices_proof': read_dump_words(sorted(probe_files('_final_world_vertices.bin')), 8),
              'world_arrays_proof': read_dump_words(sorted(probe_files('_final_world_arrays.bin')), 8),
              'interleaved_vertices_proof': read_dump_words(sorted(probe_files('_final_interleaved_vertices.bin')), 8),
              'mdr_skin_cache_proof': read_dump_words(sorted(probe_files('_final_mdr_skin_cache.bin')), 8),
              'mdr_skin_mismatch_proof': read_dump_words(sorted(probe_files('_final_mdr_skin_mismatch.bin')), 32),
              'mdr_frame_pair_protected': read_dump_words(sorted(probe_files('_final_mdr_frame_pair_protected.bin')), 1),
              'world_vertex_rejects': read_dump_words(sorted(probe_files('_final_world_rejects.bin')), 8),
              'fx_cull_proof': read_dump_words(sorted(probe_files('_final_fx_cull.bin')), 3),
              'fx_merge_proof': read_dump_words(sorted(probe_files('_final_fx_merged.bin')), 1),
              'fx_pvs_proof': read_dump_words(sorted(probe_files('_final_fx_pvs.bin')), 4),
              'zone_memory_samples': zone_memory_samples,
              'long_frames': parse_long_frames(read_dump_words(sorted(probe_files('_final_long_frames.bin')), 129)),
              'stasis_upload_stats': read_dump_words(sorted(probe_files('_final_stasis_upload.bin')), 432),
              'stasis_draw_stats': read_dump_words(sorted(probe_files('_final_stasis_draw.bin')), 40),
              'stasis_d3d_stats': read_dump_words(sorted(probe_files('_final_stasis_d3d.bin')), 48),
              'packed_load_phases_ms': packed_load_phases,
              'xbe_sha256': manifest['xbe_sha256'],
              'observed_players': sorted(observed_players), 'split_evidence': split_evidence,
              'final_client_state': client_state,
              'engine_error_message': engine_error,
              'movie_status': read_dump_words(sorted(probe_files('_final_movie_status.bin')), 1),
              'movie_frame': read_dump_words(sorted(probe_files('_final_movie_frame.bin')), 1),
              'final_main_loop_advance': live_delta,
              'final_liveness_verified': live_delta is not None and 0 < live_delta < 100000,
              'player_state': player_state,
              'god_mode_requested': a.god,
              'god_mode': player_state.get('god_mode') if player_state else None,
              'finished_in_game': client_state == 7,  # CA_ACTIVE
              'mode_verified': mode_verified,
              'paused_during_harness_setup': True,
              'visual_only': a.visual, 'diagnostic_only': a.diagnostic, 'retail_verified': False}
    if a.mode == 'mp':
        state = read_dump_words(sorted(probe_files('_final_hm_game_state.bin')), 22)
        result['bot_startup_evidence'] = read_dump_words(
            sorted(probe_files('_final_hm_bot_proof.bin')), 32)
        result['bot_definition_evidence'] = read_dump_words(
            sorted(probe_files('_final_bot_definitions.bin')), 8)
        result['simulation_evidence'] = ({'frame': state[7], 'time': state[8],
            'start_time': state[10], 'restarted': state[17],
            'connected': state[18], 'playing': state[20]} if state else None)
        result['local_command_evidence'] = {
            label: read_dump_words(sorted(probe_files('_final_'+label+'.bin')), 4)
            for label in ('hm_cmd_serial', 'hm_cmd_forward', 'hm_cmd_right', 'hm_cmd_buttons')}
        result['simulation_verified'] = bool(state and state[7] > 20
            and state[8] > state[10] + 1000 and state[17] == 0
            and state[18] >= 4 and state[20] >= 4)
        result['mp_workload_verified'] = verify_mp_workload(result)
    # Keep legacy simulation-clock measurements explicitly labeled. New runs
    # qualify only using unclamped guest elapsed time, with every window covered.
    result['simulation_clock_fps'] = result['fps']
    result['simulation_clock_average_fps'] = result['average_fps']
    elapsed_complete = bool(samples and len(frame_time_windows) == len(samples)
                            and all(item['elapsed_guest_ms'] for item in frame_time_windows))
    result['elapsed_timing_complete'] = elapsed_complete
    result['fps_basis'] = 'guest elapsed time' if elapsed_complete else 'simulation clock; insufficient for acceptance'
    if elapsed_complete:
        result['fps'] = [item['elapsed_guest_fps'] for item in frame_time_windows]
        result['average_fps'] = (sum(item['frames'] for item in frame_time_windows) * 1000
                                 / sum(item['elapsed_guest_ms'] for item in frame_time_windows))
        result['minimum_fps'] = min(result['fps'])
        result['below_target'] = sum(fps < target for fps in result['fps'])
    result['measured_map_window_pass'] = elapsed_complete and qualifies_window(result)
    (out/(a.name+'.summary.json')).write_text(json.dumps(result, indent=2))
    print(manifest_path)


if __name__ == '__main__':
    main()
