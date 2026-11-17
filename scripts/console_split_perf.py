"""Console-only multi-map qualification; no emulator or host input facilities.

prepare-assets and freeze-code never connect to the Xbox. All other commands
require a matching frozen-code manifest before even opening a connection.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/coop_split_20261116'
SOURCE = ROOT / 'build/issue1_20261116/stage'
SDK = Path('C:/XDK_5558/XDK/xbox/bin')
REMOTE = 'E:\\Games\\StarTrekEF-Issue1\\'
MAPS = {'coop': ('borg1', 'stasis1', 'scav2'),
        'holomatch': ('hm_borg1', 'hm_voy1', 'hm_scav1')}
MUSIC = {'borg1': 'borg1', 'stasis1': 'stasis1', 'scav2': 'elite6'}
MARKERS = ('ef_sp_level.txt', 'ef_sp_commands.txt', 'ef_sp_postmap_commands.txt',
           'ef_sp_client_active_commands.txt', 'ef_sp_input_replay.txt')


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def local(path):
    path = Path(path).resolve()
    if not path.is_relative_to(ROOT.resolve()):
        raise ValueError('Project path escapes repository: ' + str(path))
    return path


def save_json(path, value):
    local(path).write_text(json.dumps(value, indent=2), encoding='utf-8')


def prepare_assets():
    out = WORK / 'assets'
    out.mkdir(parents=True, exist_ok=True)
    base = []
    for p in sorted(SOURCE.rglob('*')):
        if not p.is_file():
            continue
        rel = p.relative_to(SOURCE).as_posix()
        low = rel.lower()
        if low in ('default.xbe', 'efmp.xbe', 'xbx_filelist',
                   'baseef/xbox0.pk3', 'baseef/xbox1.pk3') or low.startswith('baseef/music/'):
            continue
        if '/' not in low and low.endswith('.txt'):
            continue  # Never inherit old startup automation.
        base.append(dict(remote=rel, local=str(p.relative_to(ROOT)),
                         bytes=p.stat().st_size, sha256=digest(p)))
    # Keep every supplied movie and the complete original sound bank. Only
    # unrelated native maps/music are omitted from this private bounded stage.
    for mode, maps in MAPS.items():
        for map_name in maps:
            records = list(base)
            names = ('xbox0.pk3',) if mode == 'coop' else ('xbox0.pk3', 'xbox1.pk3')
            for name in names:
                dst = out / (map_name + '_' + name)
                with zipfile.ZipFile(SOURCE/'BaseEF'/name) as src, zipfile.ZipFile(dst, 'w') as target:
                    for info in src.infolist():
                        n = info.filename.lower()
                        if n.startswith('maps/') and not (n.startswith('maps/'+map_name+'/') or
                                n.startswith('maps/xbox/'+map_name+'.') or n.startswith('maps/'+map_name+'.')):
                            continue
                        with src.open(info) as reader, target.open(info, 'w') as writer:
                            shutil.copyfileobj(reader, writer, 1024*1024)
                with zipfile.ZipFile(SOURCE/'BaseEF'/name) as src, zipfile.ZipFile(dst) as target:
                    for info in target.infolist():
                        original = src.getinfo(info.filename)
                        if (info.CRC, info.file_size) != (original.CRC, original.file_size):
                            raise ValueError('Asset content changed: '+info.filename)
                records.append(dict(remote='BaseEF/'+name, local=str(dst.relative_to(ROOT)),
                                     bytes=dst.stat().st_size, sha256=digest(dst)))
            if map_name in MUSIC:
                music = next(p for p in (SOURCE/'BaseEF/music').iterdir()
                             if p.stem.lower() == MUSIC[map_name])
                records.append(dict(remote='BaseEF/music/'+music.name, local=str(music.relative_to(ROOT)),
                                     bytes=music.stat().st_size, sha256=digest(music)))
            save_json(out/(map_name+'.json'), {'mode': mode, 'map': map_name, 'files': records,
                                             'bytes': sum(r['bytes'] for r in records)})
            print('Prepared', map_name, sum(r['bytes'] for r in records), 'bytes', flush=True)


def code_files():
    # Include all tracked source changes plus new source/harness files. The
    # reviewed issue-1 changes are part of this binary identity as well.
    result = subprocess.run(['git', '--no-optional-locks', 'ls-files', '-m', '-o',
                             '--exclude-standard', '-z'], cwd=ROOT, check=True, capture_output=True)
    suffixes = {'.h', '.c', '.cpp', '.py', '.ps1', '.asm', '.vcproj'}
    return sorted({local(ROOT/os.fsdecode(p)) for p in result.stdout.split(b'\0')
                   if p and Path(os.fsdecode(p)).suffix.lower() in suffixes})


def freeze_code():
    bundles = {}
    for kind in ('production', 'verification'):
        files = {}
        for stem in ('default', 'efmp'):
            for ext in ('exe', 'map', 'xbe'):
                p = WORK/kind/(stem+'.'+ext)
                if not p.is_file():
                    raise ValueError('Missing complete cross-build bundle: '+str(p))
                if ext=='xbe':
                    flavor=b'flavor=frame-diagnostics' if kind=='verification' else b'flavor=production'
                    if flavor not in p.read_bytes():
                        raise ValueError('Unexpected runtime build flavor: '+str(p))
                files[str(p.relative_to(ROOT))] = digest(p)
        bundles[kind] = files
    # Both post-work bundles must omit the old visibility diagnostics.
    for kind in ('production', 'verification'):
        for stem in ('default', 'efmp'):
            symbols = (WORK/kind/(stem+'.map')).read_text(encoding='utf-8',errors='replace')
            if 'STEFX_PvsProbeSurfaceWord' in symbols:
                raise ValueError('Unexpected visibility diagnostic code in '+kind+'/'+stem)
            for required in ('_g_SPXBSplitVisibility', '_g_SPXBConsoleWorkload', '_g_SPXBCoopLightMismatch'):
                if required not in symbols:
                    raise ValueError('Missing new console implementation: '+required)
    for maps in MAPS.values():
        for name in maps:
            if not (WORK/'assets'/(name+'.json')).is_file():
                raise ValueError('Prepare all six map asset sets before opening the gate')
    save_json(WORK/'code_complete.json', {'all_six_code_items_complete': True,
        'console_only': True, 'frozen_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        'sources': {str(p.relative_to(ROOT)): digest(p) for p in code_files()}, 'bundles': bundles})
    print('All code and both candidate build pairs frozen. Console test gate is open.', flush=True)


def check_gate():
    gate = json.loads((WORK/'code_complete.json').read_text())
    if gate.get('all_six_code_items_complete') is not True or gate.get('console_only') is not True:
        raise ValueError('Console test gate is closed')
    current = {str(p.relative_to(ROOT)): digest(p) for p in code_files()}
    if current != gate['sources']:
        raise ValueError('Code changed after freeze; console test gate is closed')
    for bundle in gate['bundles'].values():
        for name, expected in bundle.items():
            if digest(local(ROOT/name)) != expected:
                raise ValueError('Frozen binary/symbol identity changed: '+name)
    return gate


def tool(name, *args, timeout=30):
    env = dict(os.environ, TEMP=str(WORK/'tmp'), TMP=str(WORK/'tmp'))
    return subprocess.run([str(SDK/(name+'.exe')), '/x', '10.0.0.231', *args],
        cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)


class Xbdm:
    """Read-only monitor queries. Never attach a debugger or send input."""
    def __init__(self):
        self.sock = socket.create_connection(('10.0.0.231', 731), timeout=8)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.file = self.sock.makefile('rb')
        welcome = self.line()
        if not welcome.startswith('201'):
            self.close()
            raise RuntimeError(welcome)

    def close(self):
        self.file.close()
        self.sock.close()

    def line(self):
        data = self.file.readline(65536)
        if not data:
            raise ConnectionError('Xbox monitor disconnected')
        return data.decode('ascii', 'replace').rstrip('\r\n')

    def query(self, text):
        if not (text.startswith('getmem ') or text.startswith('dirlist ')):
            raise ValueError('Only read-only monitor queries are allowed')
        self.sock.sendall((text+'\r\n').encode('ascii'))
        status = self.line()
        if not status.startswith('202'):
            raise RuntimeError(status)
        lines = []
        while True:
            line = self.line()
            if line == '.':
                return lines
            lines.append(line)

    def read(self, address, size):
        data = bytearray()
        while len(data) < size:
            chunk = min(1024, size-len(data))
            lines = self.query('getmem addr=0x%x length=0x%x' % (address+len(data), chunk))
            raw = ''.join(lines).replace(' ', '').replace('\t', '')
            if not re.fullmatch('[0-9a-fA-F]{%d}' % (chunk*2), raw):
                raise ValueError('Incomplete or inaccessible Xbox memory response')
            data.extend(bytes.fromhex(raw))
        return bytes(data)

    def files(self, directory):
        result = []
        for line in self.query('dirlist name="'+directory+'"'):
            match = re.search(r'name="([^"\\/]+)"', line)
            if not match or match[1] in ('.', '..'):
                continue
            name = directory.rstrip('\\')+'\\'+match[1]
            attr = re.search(r'attributes=(0x[0-9a-fA-F]+|\d+)', line)
            is_dir = 'directory' in line.lower() or (attr and int(attr[1], 0) & 16)
            result.extend(self.files(name) if is_dir else [name])
        return result


class Symbols:
    """Strict linker-PE-XBE section translation; never fall back to PE VAs."""
    def __init__(self, path):
        self.path = local(path)
        pe = path.with_suffix('.exe').read_bytes()
        xbe = path.with_suffix('.xbe').read_bytes()
        u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
        u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
        off = u32(pe, 0x3c)
        if pe[off:off+4] != b'PE\0\0' or xbe[:4] != b'XBEH':
            raise ValueError('Invalid immutable PE/XBE bundle')
        image_base = u32(pe, off+24+28)
        sections = []
        start = off+24+u16(pe, off+20)
        for i in range(u16(pe, off+6)):
            at = start+40*i
            sections.append((pe[at:at+8].split(b'\0')[0], image_base+u32(pe, at+12),
                             max(u32(pe, at+8), u32(pe, at+16))))
        xbase = u32(xbe, 0x104)
        xsections = {}
        self.xbe_data = xbe
        self.file_sections = []
        self.identity_probe = None
        for i in range(u32(xbe, 0x11c)):
            at = u32(xbe, 0x120)-xbase+56*i
            no = u32(xbe, at+20)-xbase
            name = xbe[no:no+32].split(b'\0')[0]
            xsections[name] = (u32(xbe, at+4), u32(xbe, at+8))
            self.file_sections.append((u32(xbe,at+4), u32(xbe,at+8), u32(xbe,at+12), u32(xbe,at+16)))
            if name == b'.text':
                raw = u32(xbe, at+12)
                self.identity_probe = (u32(xbe, at+4), xbe[raw:raw+64])
        if not self.identity_probe:
            raise ValueError('No executable text identity probe')
        self.addresses = {}
        for line in path.read_text(errors='replace').splitlines():
            m = re.match(r'\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\b', line)
            if not m:
                continue
            linked = int(m[2], 16)
            for name, base, size in sections:
                if base <= linked < base+size and name in xsections:
                    target, limit = xsections[name]
                    if linked-base < limit:
                        self.addresses[m[1]] = target+linked-base
                    break

    def get(self, name):
        return self.addresses.get(name)

    def initial_word(self, name):
        address = self.addresses[name]
        for base, size, raw, stored in self.file_sections:
            if base <= address and address+4 <= base+size:
                offset=address-base
                if offset >= stored: return 0  # Loader-zeroed BSS.
                if offset+4 <= stored:
                    return struct.unpack_from('<I',self.xbe_data,raw+offset)[0]
        raise ValueError('No initialized XBE data for '+name)


def backup_saves():
    out = WORK/'save_backup'
    if (out/'manifest.json').exists():
        return
    out.mkdir(exist_ok=True)
    monitor = Xbdm()
    try:
        files = monitor.files('V:\\4c41000b')
    finally:
        monitor.close()
    if not files:
        raise ValueError('Refusing tests without the existing title save inventory')
    records = []
    for remote in files:
        rel = remote[len('V:\\4c41000b\\'):].replace('\\', '/')
        target = local(out/rel)
        target.parent.mkdir(parents=True, exist_ok=True)
        r = tool('xbcp', '/y', 'x'+remote, str(target))
        if r.returncode:
            raise RuntimeError(r.stdout.decode(errors='replace'))
        records.append(dict(remote=remote, local=str(target.relative_to(ROOT)), sha256=digest(target)))
    save_json(out/'manifest.json', records)


def deploy_assets(map_name, out):
    manifest = json.loads((WORK/'assets'/(map_name+'.json')).read_text())
    previous_path = WORK/'deployed_assets.json'
    # The prior issue-1 install is owned by this task. Seed known content so the
    # unchanged 1.2-GB sound bank need not be retransmitted for every map.
    if previous_path.exists():
        previous = json.loads(previous_path.read_text())
    else:
        old = json.loads((ROOT/'build/issue1_20261116/hardware_stage_manifest.json').read_text())
        previous = {x['path']: x['sha256'] for x in old['kept']}
    wanted = {r['remote']: r['sha256'] for r in manifest['files']}
    # The title regenerates this cache; it is not part of the immutable stage.
    tool('xbdel', 'x'+REMOTE+'xbx_filelist')
    for name in sorted(set(previous)-set(wanted)):
        if name not in ('default.xbe', 'efmp.xbe'):
            if '..' in Path(name).parts or ':' in name:
                raise ValueError('Unsafe staged path')
            r = tool('xbdel', 'x'+REMOTE+name.replace('/', '\\'))
            (out/('delete_'+Path(name).name+'.txt')).write_bytes(r.stdout)
    for record in manifest['files']:
        if previous.get(record['remote']) == record['sha256']:
            continue
        src = local(ROOT/record['local'])
        if digest(src) != record['sha256']:
            raise ValueError('Prepared asset identity changed: '+str(src))
        remote = REMOTE+record['remote'].replace('/', '\\')
        r = tool('xbcp', '/y', str(src), 'x'+remote, timeout=300)
        (out/('transfer_'+Path(record['remote']).name+'.txt')).write_bytes(r.stdout)
        if r.returncode:
            raise RuntimeError('Asset transfer failed: '+record['remote'])
        print('Transferred', record['remote'], flush=True)
    save_json(previous_path, wanted)


def commands(mode, profile, mask):
    selected = mask
    verify = profile == 'verify'
    world = 2  # Post-work qualification only; the user declined A/B runs.
    values = {'stefx_console_qualification': 1, 'stefx_menu_smoke': 0,
        'stefx_splitScreen': 1, 'stefx_splitScreenPlayers': 2 if mode=='coop' else 4,
        'stefx_splitScreenMode': mode, 'r_splitScreenEconomy': 1,
        'r_efSplitVisReuse': (2 if verify else 1) if selected&4 else 0,
        'r_efCoopModelPvs': 1 if selected&2 else 0,
        'r_efHmModelPvs': 1 if selected&2 else 0,
        'r_efCoopLight': (2 if verify else 1) if selected&8 else 0,
        'r_efHmLight': (2 if verify else 1) if selected&8 else 0,
        'cg_efCoopFxTrace': (2 if verify else 1) if selected&1 else 0,
        'r_efWorldVertices': world, 'r_efWorldVerticesVerify': int(verify),
        'r_efCoopWorldVertices': 0, 'r_efCoopLowRes': 0,
        'stefx_splitScreenP2Entity': -1, 'cg_virtualVoyager': 0,
        'stefx_splitScreenTestP2Input': 3 if mode=='coop' else 0}
    if profile == 'defaults':
        for name in ('r_efSplitVisReuse','r_efCoopModelPvs','r_efHmModelPvs',
                     'r_efCoopLight','r_efHmLight','cg_efCoopFxTrace','r_efWorldVertices'):
            del values[name]
    if mode == 'holomatch':
        values.update(stefx_hmLocalPlayers=4, stefx_hmHumanPlayers=4,
            stefx_hm_split_virtual_controls=1, stefx_hm_split_virtual_controls_p1=1,
            stefx_hm_launch_source='direct', bot_enable=1, bot_minplayers=0,
            sv_maxclients=8, g_gametype=0, fraglimit=0, timelimit=0)
    return '\n'.join('set %s %s' % pair for pair in values.items())+'\n'


def sample(monitor, symbols, fps_index, log_index):
    probe, expected = symbols.identity_probe
    if monitor.read(probe, len(expected)) != expected:
        raise ValueError('Running title does not match the frozen XBE text')
    result = {'utc': time.time()}
    blocks = {'_g_SPXBConsoleWorkload':80, '_g_SPXBCoopFxTrace':12,
        '_g_SPXBCoopFxMismatch':32, '_g_SPXBCoopModelPvs':8,
        '_g_SPXBSplitVisibility':8, '_g_SPXBCoopLight':16,
        '_g_SPXBCoopLightMismatch':64, '_g_SPXBWorldVertices':8, '_g_SPXBWorldArrays':8,
        '_g_SPXBMainLoopCount':1, '_g_SPXBHMSplitRenderDoneSerial':4,
        '_g_SPXBHMSplitRenderDrawDelta':4, '_g_SPXBHMSplitBotProof':32,
        '_g_SPXBHMSplitStateOriginX':4, '_g_SPXBHMSplitStateOriginY':4,
        '_g_SPXBHMSplitStateOriginZ':4, '_g_SPXBHMSplitStateHealth':4,
        '_g_SPXBPhysAvail':1, '_g_SPXBMapHash':1, '_g_SPXBClsState':1,
        '_g_SPXBSplitBuildDefaults':1}
    # Combine nearby publications to minimize monitor/network interruptions.
    spans=[]
    for address, name, count in sorted((symbols.get(n),n,c) for n,c in blocks.items() if symbols.get(n) is not None):
        end=address+count*4
        if spans and address<=spans[-1][1]+16 and end-spans[-1][0]<=1024:
            spans[-1][1]=max(spans[-1][1],end)
        else:
            spans.append([address,end])
    snapshots=[];workload_address=symbols.get('_g_SPXBConsoleWorkload');workload_coherent=False
    for start,end in spans:
        for attempt in range(3):
            data=monitor.read(start,end-start)
            if workload_address is None or not start<=workload_address<end: break
            before=struct.unpack_from('<I',data,workload_address-start)[0]
            after=struct.unpack('<I',monitor.read(workload_address,4))[0]
            workload_coherent=not (before&1) and before==after
            if workload_coherent: break
        snapshots.append((start,data))
    for name, count in blocks.items():
        address = symbols.get(name)
        if address is None:
            continue
        base,data=next((base,data) for base,data in snapshots if base<=address and address+count*4<=base+len(data))
        raw=data[address-base:address-base+count*4]
        words = list(struct.unpack('<%dI'%count, raw))
        if name == '_g_SPXBConsoleWorkload' and not workload_coherent:
            continue
        result[name] = words
    address = symbols.get('?com_errorMessage@@3PADA')
    if address is not None:
        result['engine_error'] = monitor.read(address,1024).split(b'\0',1)[0].decode('utf-8','replace')
    address = symbols.get('_g_SPXBFpsProfileMirrorIndex')
    ring = symbols.get('_g_SPXBFpsProfileMirror')
    if address is not None and ring is not None:
        current = struct.unpack('<I',monitor.read(address,4))[0]
        if current < fps_index:
            raise ValueError('FPS publication reset during a run')
        result['fps_dropped'] = max(0,current-fps_index-64)
        result['fps'] = []
        for serial in range(max(fps_index,current-64),current):
            raw = monitor.read(ring+(serial&63)*576,576)
            result['fps'].append({'serial':serial,'text':raw.split(b'\0',1)[0].decode('ascii','replace')})
        after = struct.unpack('<I',monitor.read(address,4))[0]
        if after < current or after-max(fps_index,current-64) > 64:
            raise ValueError('FPS ring changed beyond retained capacity while reading')
        fps_index = current
    if log_index is not None:
        position=symbols.get('_g_SPXBLogMirrorPos');ring=symbols.get('_g_SPXBLogMirror')
        current=struct.unpack('<I',monitor.read(position,4))[0]
        if current<log_index: log_index=0
        start=max(log_index,current-32768)
        result['log_dropped']=max(0,current-log_index-32768)
        raw=bytearray();cursor=start
        while cursor<current:
            length=min(current-cursor,32768-(cursor&32767))
            raw.extend(monitor.read(ring+(cursor&32767),length));cursor+=length
        after=struct.unpack('<I',monitor.read(position,4))[0]
        if after-start<=32768:
            result['log_text']=raw.decode('utf-8','replace')
        else:
            result['log_dropped']+=len(raw)
        log_index=current
    result['monitor_block_reads']=len(spans)
    return result, fps_index, log_index


def wait_until_stopped(symbols, timeout=75):
    """Wait for reboot completion, then prove the tested title is unloaded."""
    deadline=time.monotonic()+timeout
    address,expected=symbols.identity_probe
    last_error='tested title still present'
    while time.monotonic()<deadline:
        monitor=None
        try:
            monitor=Xbdm()
            try: unchanged=monitor.read(address,len(expected))==expected
            except ValueError: unchanged=False  # Title address is no longer mapped.
            entries=monitor.query('dirlist name="'+REMOTE.rstrip('\\')+'"')
            if not unchanged: return entries
        except (OSError,RuntimeError,ValueError) as error:
            last_error=str(error)
        finally:
            if monitor: monitor.close()
        time.sleep(2)
    raise RuntimeError('Reboot did not reach an accessible stopped title: '+last_error)


def run(args):
    check_gate()  # Must precede every network/file-transfer operation.
    mode = next(k for k,v in MAPS.items() if args.map in v)
    kind = 'verification' if args.profile=='verify' else 'production'
    if args.profile=='defaults' and (WORK/'selection.json').exists():
        selection=json.loads((WORK/'selection.json').read_text(encoding='utf-8'))
        for name, expected in selection['files'].items():
            if digest(local(ROOT/name))!=expected:
                raise ValueError('Selected bundle identity changed')
        kind='selected'
    stem = 'default' if mode=='coop' else 'efmp'
    log_name = 'ef_sp_log.txt' if mode=='coop' else 'ef_mp_log.txt'
    bundle = WORK/kind
    symbols = Symbols(bundle/(stem+'.map'))
    out = WORK/'runs'/args.label
    if out.exists():
        raise ValueError('Evidence directory already exists; choose a new label')
    out.mkdir(parents=True)
    backup_saves()
    deploy_assets(args.map,out)
    r = tool('xbcp','/y',str(bundle/(stem+'.xbe')),'x'+REMOTE+stem+'.xbe')
    (out/'xbe_transfer.txt').write_bytes(r.stdout)
    if r.returncode:
        raise RuntimeError('XBE transfer failed')
    # Read back the installed binary before resolving any runtime symbols.
    installed = out/'installed.xbe'
    r = tool('xbcp','/y','x'+REMOTE+stem+'.xbe',str(installed))
    if r.returncode or digest(installed)!=digest(bundle/(stem+'.xbe')):
        raise RuntimeError('Installed XBE identity mismatch')
    for marker in MARKERS:
        tool('xbdel','x'+REMOTE+marker)
    payloads = {'ef_sp_level.txt':args.map+'\n', 'ef_sp_commands.txt':commands(mode,args.profile,args.mask)}
    if mode=='holomatch':
        # The original FFA minimum-player manager reserves one spare slot.
        # Add exactly four ordinary bots to the eight-slot local match instead.
        payloads['ef_sp_postmap_commands.txt']=''.join(
            'addbot %s 3 free 0\n'%name for name in ('1_of_12','2_of_3','3_of_6','4_of_12'))
    if mode=='coop':
        # Ordinary process-local movement/fire commands, yielding to authored
        # camera and player locks; never manufacture ammo, enemies or progress.
        payloads['ef_sp_input_replay.txt'] = 'STEFX_INPUT_REPLAY_V1 '+args.map+'\n'+''.join(
            '%d %d 75 %d 0 0 %d %d\n' % (i*15000,(i+1)*15000,35 if i%2 else -35,
                                         35 if i%2 else -35,1 if i%4!=3 else 0) for i in range(32))
    for name,payload in payloads.items():
        p=out/name;p.write_text(payload,encoding='ascii')
        r=tool('xbcp','/y',str(p),'x'+REMOTE+name)
        if r.returncode:
            raise RuntimeError('Startup marker transfer failed: '+name)
    tool('xbdel','x'+REMOTE+log_name)
    save_json(out/'identity.json',{'map':args.map,'mode':mode,'profile':args.profile,
        'bundle':str(bundle.relative_to(ROOT)),'xbe_sha256':digest(installed),
        'seconds':args.seconds,'captures':args.captures,'feature_mask':args.mask,
        'compiled_defaults':symbols.initial_word('_g_SPXBSplitBuildDefaults'),'symbols':symbols.addresses,
        'asset_manifest':json.loads((WORK/'assets'/(args.map+'.json')).read_text())})
    monitor=None;fps_index=0;captured=0;start=time.monotonic();last_report=-20
    last_fps_elapsed=None;last_capture=-60;failure=None
    log_index=0 if args.profile=='verify' else None
    try:
        boot=tool('xbreboot','x'+REMOTE+stem+'.xbe')
        (out/'boot.txt').write_bytes(boot.stdout)
        if boot.returncode:
            raise RuntimeError('Console launch failed')
        with (out/'flight.jsonl').open('w',encoding='utf-8',buffering=1) as flight:
            while time.monotonic()-start<args.seconds:
                elapsed=time.monotonic()-start
                if monitor is None:
                    try: monitor=Xbdm()
                    except (OSError,RuntimeError): time.sleep(2);continue
                try:
                    previous_fps=fps_index
                    record,fps_index,log_index=sample(monitor,symbols,fps_index,log_index)
                    if fps_index>previous_fps: last_fps_elapsed=elapsed
                    if last_fps_elapsed is not None and elapsed-last_fps_elapsed>60:
                        raise RuntimeError('No new frame-time publication for 60 seconds')
                    record['elapsed']=elapsed;flight.write(json.dumps(record)+'\n')
                    if record.get('log_text'):
                        with (out/'runtime_ring.log').open('a',encoding='utf-8') as ring_log:
                            ring_log.write(record['log_text'])
                    if record.get('engine_error'):
                        raise RuntimeError('Engine error: '+record['engine_error'])
                    workload=record.get('_g_SPXBConsoleWorkload')
                    roster_ready=bool(workload and tuple(workload[5:8])==(8,8,4))
                    if mode=='holomatch' and workload and workload[2]>20000 and not roster_ready:
                        raise RuntimeError('Incomplete Holomatch workload: expected four locals plus four bots')
                    for name,index in (('_g_SPXBCoopFxTrace',6),('_g_SPXBCoopLight',6),
                                       ('_g_SPXBSplitVisibility',5),('_g_SPXBWorldVertices',6),('_g_SPXBCoopLight',7)):
                        if name in record and record[name][index]:
                            raise RuntimeError('Correctness failure: '+name)
                    if (args.captures and captured<args.captures and (mode=='coop' or roster_ready) and elapsed-last_capture>=60 and
                            (captured==0 or elapsed>=args.seconds*captured/args.captures)) and any(
                            'gameplay=1 excludedChecks=0' in f['text'] for f in record.get('fps',[])):
                        capture=out/('native_%02d.bmp'%captured)
                        r=tool('xbcapture',str(capture))
                        (out/('capture_%02d.txt'%captured)).write_bytes(r.stdout)
                        if r.returncode: raise RuntimeError('Native capture failed')
                        from PIL import Image
                        Image.open(capture).save(capture.with_suffix('.png'))
                        captured+=1;last_capture=elapsed
                    if elapsed-last_report>=20:
                        print(args.label,round(elapsed,1),'s; FPS publications',fps_index,
                              'loop',record.get('_g_SPXBMainLoopCount'),flush=True)
                        last_report=elapsed
                except (OSError,ConnectionError,ValueError) as error:
                    flight.write(json.dumps({'elapsed':elapsed,'read_error':str(error)})+'\n')
                    if monitor: monitor.close();monitor=None
                    if ((elapsed>min(240,args.seconds-10) and fps_index==0) or
                            (last_fps_elapsed is not None and elapsed-last_fps_elapsed>60)):
                        raise RuntimeError('No readable runtime FPS evidence') from error
                time.sleep(2)
    except Exception as error:
        failure=str(error)
        raise
    finally:
        if monitor: monitor.close()
        cleanup_errors=[];stop_code=None
        if args.profile!='verify':
            try:
                log=tool('xbcp','/y','x'+REMOTE+log_name,str(out/'runtime.log'),timeout=15)
                (out/'log_transfer.txt').write_bytes(log.stdout)
                if log.returncode: cleanup_errors.append('Runtime file log transfer failed')
            except Exception as error: cleanup_errors.append('Runtime log: '+str(error))
        # Delete startup files while the monitor is still connected. A reboot
        # briefly removes XBDM, so SDK operations must not race that interval.
        try:
            for marker in MARKERS: tool('xbdel','x'+REMOTE+marker,timeout=5)
        except Exception as error: cleanup_errors.append('Pre-reboot cleanup: '+str(error))
        try:
            stop=tool('xbreboot');stop_code=stop.returncode
            (out/'stop.txt').write_bytes(stop.stdout)
        except Exception as error: cleanup_errors.append('Reboot: '+str(error))
        endpoint={'elapsed':time.monotonic()-start,'fps_index':fps_index,
            'captured':captured,'stop_returncode':stop_code,'failure':failure,
            'last_fps_elapsed':last_fps_elapsed,'cleanup_errors':cleanup_errors,
            'title_stopped_verified':False,'markers_removed_verified':False}
        save_json(out/'endpoint.json',endpoint)  # Preserve evidence even if recovery fails.
        try:
            entries=wait_until_stopped(symbols)
            endpoint['title_stopped_verified']=True
            remaining=[name for name in MARKERS if any('name="'+name+'"' in line.lower() for line in entries)]
            for marker in remaining: tool('xbdel','x'+REMOTE+marker,timeout=5)
            check=Xbdm()
            try: entries=check.query('dirlist name="'+REMOTE.rstrip('\\')+'"')
            finally: check.close()
            endpoint['markers_removed_verified']=not any(
                'name="'+name+'"' in line.lower() for name in MARKERS for line in entries)
        except Exception as error: cleanup_errors.append('Reboot recovery: '+str(error))
        save_json(out/'endpoint.json',endpoint)
        if not endpoint['title_stopped_verified'] or not endpoint['markers_removed_verified']:
            raise RuntimeError('Console stop/cleanup was not verified; see endpoint.json')
    print('Run ended; native images and workload/FPS evidence require review.',flush=True)


def freeze_selection(args):
    check_gate()
    target=WORK/'selected'
    if target.exists(): raise ValueError('Selected bundle already exists; preserve its evidence')
    # The code is unchanged. Only previously implemented build-time defaults
    # may differ after selecting the measured configuration.
    expected=(args.world_mode<<16)|args.mask
    for stem in ('default','efmp'):
        symbols=Symbols(ROOT/'build/release'/(stem+'.map'))
        if symbols.initial_word('_g_SPXBSplitBuildDefaults')!=expected:
            raise ValueError('Build-time defaults do not match the selected policy')
        if b'flavor=production' not in (ROOT/'build/release'/(stem+'.xbe')).read_bytes():
            raise ValueError('Selected bundle must be a production build')
        source_map=(ROOT/'build/release'/(stem+'.map')).read_text(encoding='utf-8',errors='replace')
        if 'STEFX_PvsProbeSurfaceWord' in source_map:
            raise ValueError('Selected build still contains visibility diagnostics')
    target.mkdir()
    records={}
    for stem in ('default','efmp'):
        for ext in ('exe','map','xbe'):
            dst=target/(stem+'.'+ext)
            shutil.copy2(ROOT/'build/release'/dst.name,dst)
            records[str(dst.relative_to(ROOT))]=digest(dst)
    save_json(WORK/'selection.json',{'mask':args.mask,'world_mode':args.world_mode,'files':records})
    print('Measured policy frozen from unchanged source; defaults runs will use this pair.',flush=True)


def restore():
    check_gate()
    records=json.loads((WORK/'save_backup/manifest.json').read_text())
    monitor=Xbdm()
    try: current=monitor.files('V:\\4c41000b')
    finally: monitor.close()
    original={r['remote'] for r in records}
    for name in set(current)-original:
        r=tool('xbdel','x'+name)
        if r.returncode: raise RuntimeError('Could not remove test-created save: '+name)
    readback=WORK/'save_readback';readback.mkdir(exist_ok=True)
    for i,record in enumerate(records):
        src=local(ROOT/record['local'])
        if digest(src)!=record['sha256']: raise ValueError('Save backup changed')
        r=tool('xbcp','/y',str(src),'x'+record['remote'])
        if r.returncode: raise RuntimeError('Save restoration failed')
        dst=readback/('%02d.bin'%i)
        r=tool('xbcp','/y','x'+record['remote'],str(dst))
        if r.returncode or digest(dst)!=record['sha256']:
            raise RuntimeError('Save restoration readback failed')
    for marker in MARKERS: tool('xbdel','x'+REMOTE+marker)
    monitor=Xbdm()
    try:
        restored=set(monitor.files('V:\\4c41000b'))
        root_entries=monitor.query('dirlist name="'+REMOTE.rstrip('\\')+'"')
    finally: monitor.close()
    if restored!=original:
        raise RuntimeError('Restored save inventory differs from its original backup')
    for line in root_entries:
        match=re.search(r'name="([^"\\/]+)"',line)
        if match and match[1].lower() in MARKERS:
            raise RuntimeError('Test startup marker remains on console: '+match[1])
    save_json(WORK/'restoration.json',{'verified_files':len(records),'markers_removed':True,
                                     'save_inventory_matches':True})


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=['prepare-assets','freeze-code','freeze-selection','run','restore'])
    ap.add_argument('--map',choices=sum((list(v) for v in MAPS.values()),[]))
    ap.add_argument('--profile',choices=['verify','candidate','defaults'],default='candidate')
    ap.add_argument('--mask',type=int,choices=range(16),default=15)
    ap.add_argument('--world-mode',type=int,choices=range(3),default=2)
    ap.add_argument('--label')
    ap.add_argument('--seconds',type=int,default=300)
    ap.add_argument('--captures',type=int,choices=range(4),default=0)
    args=ap.parse_args()
    WORK.mkdir(parents=True,exist_ok=True);(WORK/'tmp').mkdir(exist_ok=True)
    if args.action=='prepare-assets': prepare_assets()
    elif args.action=='freeze-code': freeze_code()
    elif args.action=='restore': restore()
    elif args.action=='freeze-selection': freeze_selection(args)
    else:
        if not args.map or not args.label or not re.fullmatch('[A-Za-z0-9_-]+',args.label):
            ap.error('run requires a map and a unique safe label')
        if not 60<=args.seconds<=600: ap.error('Use a bounded 60–600 second console run')
        run(args)


if __name__=='__main__':
    main()
