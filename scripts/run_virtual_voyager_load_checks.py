"""Serial Virtual Voyager load probes using native XEMU captures, without OS input.

Runtime checks and image review are separate: this tool never marks images reviewed.
It stops on a failed load or an unrestored ISO transaction.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', action='append', dest='maps')
    parser.add_argument('--seconds', type=int, default=100)
    parser.add_argument('--walk', action='store_true', help='Walk through the starting doorway using process-local input')
    parser.add_argument('--walk-ms', type=int, default=4000, help='Bounded forward duration before the 90-degree turn; tune to map geometry')
    parser.add_argument('--look-around', action='store_true', help='Continue turning slowly after the bounded walk for varied native capture views')
    parser.add_argument('--save-roundtrip', action='store_true', help='Write, overwrite, and reload a diagnostic slot; require engine completion counters')
    parser.add_argument('--xemu-exe', type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads((ROOT/'build/research/virtual_voyager_sources.json').read_text())['maps']
    maps = args.maps or sorted(inventory, key=lambda name: (not name.startswith('tour/'), name))
    if any(name not in inventory for name in maps):
        parser.error('Every requested map must be in the audited Virtual Voyager inventory')
    if not 250 <= args.walk_ms <= 8000:
        parser.error('Walk duration must be between 250 and 8000 milliseconds')
    if args.seconds < 70:
        parser.error('Allow at least 70 seconds for startup and captures')
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    evidence = ROOT/'notes/evidence/virtual_voyager_20260911'/('loads_'+stamp)
    evidence.mkdir(parents=True)
    work = ROOT/'build/research/letterbox_perf'
    results = []
    xbe = ROOT/'build/release/default.xbe'
    expected_hash = hashlib.sha256(xbe.read_bytes()).hexdigest()
    for index, mapname in enumerate(maps):
        if hashlib.sha256(xbe.read_bytes()).hexdigest() != expected_hash:
            raise RuntimeError('Build changed during the serial qualification; stop and restart explicitly')
        name = 'vv_load_'+mapname.replace('/', '_')+'_'+stamp
        folder = evidence/mapname.replace('/', '_')
        folder.mkdir()
        command = [sys.executable, str(ROOT/'scripts/run_sp_perf_probe.py'),
            '--iso', 'build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager.iso',
            '--xbe', str(xbe), '--symbols', 'build/release/default.map',
            '--config', 'build/research/letterbox_perf/xemu.toml',
            '--hdd', 'build/research/audio_probe/hdd.qcow2',
            '--xemu-exe', str(args.xemu_exe), '--port', '4491', '--map', mapname,
            '--name', name, '--seconds', str(args.seconds), '--visual', '--diagnostic', '--god',
            '--first-shot-delay', str(max(20, args.seconds-80)), '--shot-interval', '25', '--max-screenshots', '3',
            '--command', 'set cg_virtualVoyager 1', '--command', 'set stefx_menu_smoke 0']
        if args.walk:
            replay = folder/'input.txt'
            replay.write_text(f'STEFX_INPUT_REPLAY_V1 {mapname}\n'
                '0 10000 0 0 0 0 0 0\n'
                f'10000 {10000+args.walk_ms} 127 0 0 0 0 0\n'
                + (f'{10000+args.walk_ms} 180000 0 0 0 0 12 0\n' if args.look_around else
                   f'{10000+args.walk_ms} {12000+args.walk_ms} 0 0 0 0 45 0\n'
                   f'{12000+args.walk_ms} 180000 0 0 0 0 0 0\n'))
            command += ['--input-replay', str(replay)]
        if args.save_roundtrip:
            if '_g_SPXBVvSaveProof' not in (ROOT/'build/release/default.map').read_text():
                raise RuntimeError('Roundtrip requires the completed-save/read diagnostic build')
            # The replay intentionally stops on a save-load time reset. Walk first,
            # then save the moved position and exercise the roundtrip.
            for post in [('wait 4000' if args.walk else 'wait 240'), 'save vv_mapcheck', 'wait 240',
                         'save vv_mapcheck', 'wait 240', 'set cg_virtualVoyager 0',
                         'load vv_mapcheck']:
                command += ['--post-command', post]
        print(f'START {index+1}/{len(maps)} {mapname}', flush=True)
        with (folder/'wrapper.log').open('w') as output:
            returncode = subprocess.call(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
        for source in work.glob(name+'*'):
            if source.is_file(): shutil.copy2(source, folder/source.name)
        summary_path = folder/(name+'.summary.json')
        transaction_path = folder/(name+'.iso_transaction.json')
        summary = json.loads(summary_path.read_text()) if summary_path.exists() else {}
        transaction = json.loads(transaction_path.read_text()) if transaction_path.exists() else {}
        logpath = folder/(name+'.run.log')
        log = logpath.read_text(errors='replace') if logpath.exists() else ''
        captures = []
        for filename in re.findall(r'^shot=.*?ok=True.*?file=(.+)$', log, re.M):
            source = Path(filename.strip())
            if source.is_file():
                shutil.copy2(source, folder/source.name)
                captures.append(str(folder/source.name))
        passed = (returncode == 0 and transaction.get('restored') is True
            and summary.get('loaded_map') == mapname
            and summary.get('finished_in_game') is True
            and summary.get('final_liveness_verified') is True
            and summary.get('engine_error_message') == '' and len(captures) == 3)
        save_proof = None
        vv_mode = None
        if args.save_roundtrip:
            proofs = list(folder.glob('*_final_vv_save_proof.bin'))
            modes = list(folder.glob('*_final_virtual_voyager_cvar.bin'))
            if len(proofs) == 1 and proofs[0].stat().st_size == 16:
                save_proof = struct.unpack('<4I', proofs[0].read_bytes())
            if len(modes) == 1 and modes[0].stat().st_size == 16:
                vv_mode = struct.unpack('<4I', modes[0].read_bytes())[-1]
            passed = passed and save_proof is not None and save_proof[0] == 1 and save_proof[1] >= 2 and save_proof[2] >= 1 and save_proof[3] == 0 and vv_mode == 1
        missing_models = None
        missing_paths = list(folder.glob('*_final_vv_missing_models.bin'))
        if '_g_SPXBVvMissingModels' in (ROOT/'build/release/default.map').read_text():
            if len(missing_paths) == 1 and missing_paths[0].stat().st_size == 528:
                missing_models = struct.unpack('<132I', missing_paths[0].read_bytes())[:4]
            passed = passed and missing_models == (1, 0, 0, 0)
        result = {'map': mapname, 'runtime_pass': passed, 'xbe_sha256': expected_hash,
            'missing_model_header': missing_models,
            'save_proof': save_proof, 'restored_vv_mode': vv_mode, 'walk': args.walk, 'walk_ms': args.walk_ms if args.walk else 0, 'look_around': args.look_around,
            'summary': str(summary_path), 'captures': captures, 'visual_review': 'pending',
            'save_load': ('passed' if passed else 'failed') if args.save_roundtrip else 'not tested by this load-only probe', 'diagnostic_invincibility': True}
        results.append(result)
        (evidence/'results.json').write_text(json.dumps(results, indent=2))
        print(('PASS' if passed else 'FAIL')+' '+mapname+' '+str(folder), flush=True)
        if not passed: return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
