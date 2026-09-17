"""Exercise the normal travel-menu callbacks and authored deck01 -> deck02 target.

Run only when the serial map suite is terminal and its ISO transaction restored.
This uses the game's opt-in process-local fixture, never host input.
"""
import datetime
import argparse
import json
from pathlib import Path
import subprocess
import struct
import shutil
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--route', choices=['turbolift', 'turbolift-return', 'holodeck-entry', 'holodeck-return', 'transporter'], default='turbolift')
    parser.add_argument('--iso', type=Path, default=Path('build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager.iso'))
    parser.add_argument('--flight-recorder', action='store_true')
    parser.add_argument('--inspect-sp-facing-triggers', action='store_true')
    parser.add_argument('--command', action='append', default=[])
    parser.add_argument('--menu-after-walk', action='store_true', help='Select destination after crossing authored room-entry reset triggers')
    parser.add_argument('--first-shot-delay', type=int, default=30)
    parser.add_argument('--shot-interval', type=int, default=35)
    parser.add_argument('--seconds', type=int, default=130)
    parser.add_argument('--input-replay', type=Path, help='Optional normal in-game movement after arming the destination')
    args = parser.parse_args()
    # All selections execute the original packaged data commands through the UI.
    # Return must follow a successful entry that created the ordinary virtual save.
    routes = {
        'turbolift': ('tour/deck01', 'turbolift', 1, 'tour/deck02'),
        'turbolift-return': ('tour/deck02', 'turbolift', 0, 'tour/deck01'),
        'holodeck-entry': ('tour/deck04', 'holodeck', 0, '_holodeck_garden'),
        'holodeck-return': ('_holodeck_garden', 'endholomenu', 4, 'tour/deck04'),
        'transporter': ('tour/deck04', 'transporter', 0, 'tour/deck01'),
    }
    start, menu, selection, destination = routes[args.route]
    name = 'vv_' + args.route.replace('-', '_') + '_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    command = [sys.executable, str(ROOT/'scripts/run_sp_perf_probe.py'),
        '--iso', str(args.iso),
        '--xbe', 'build/release/default.xbe', '--symbols', 'build/release/default.map',
        '--config', 'build/research/letterbox_perf/xemu.toml',
        '--hdd', 'build/research/audio_probe/hdd.qcow2', '--port', '4491',
        '--xemu-exe', r'C:\Users\smmel\.codex\tmp\stefx_xemu_0134\xemu.exe',
        '--map', start, '--name', name, '--seconds', str(args.seconds),
        '--visual', '--diagnostic', '--first-shot-delay', str(min(args.first_shot_delay, args.seconds - 1)),
        '--shot-interval', str(min(args.shot_interval, args.seconds // 4)), '--max-screenshots', '3',
        '--command', 'set cg_virtualVoyager 1', '--command', 'set stefx_menu_smoke 0',
        '--command', f'set stefx_vv_travel_selection {selection}']
    if args.flight_recorder:
        command += ['--flight-recorder']
    # PAK3 ext_data/sp_turbolift.dat row 1 executes use tour_turbo_02.
    # Original deck01 target_level_change names tour/deck02, target turbolift.
    # Opening/selection uses the same menus and callbacks as ordinary UI use.
    for extra in args.command:
        command += ['--command', extra]
    if args.inspect_sp_facing_triggers:
        command += ['--inspect-sp-facing-triggers']
    if args.input_replay:
        command += ['--input-replay', str(args.input_replay)]
    for post in [('wait 2400' if args.menu_after_walk else 'wait 120' if args.input_replay else 'wait 1200'), f'genericmenu {menu}', ('wait 240' if args.input_replay else 'wait 2400'),
                 'ui_ef_test_vv_travel']:
        command += ['--post-command', post]
    print(name, flush=True)
    result = subprocess.run(command, cwd=ROOT)
    folder = ROOT/'build/research/letterbox_perf'
    summary = json.loads((folder/(name+'.summary.json')).read_text())
    transaction = json.loads((folder/(name+'.iso_transaction.json')).read_text())
    passed = (result.returncode == 0 and transaction.get('restored') is True
              and summary.get('loaded_map') == destination
              and summary.get('finished_in_game') is True
              and summary.get('final_liveness_verified') is True
              and summary.get('engine_error_message') == '')
    proof_files = list(folder.glob(name+'*_final_vv_save_proof.bin'))
    proof = struct.unpack('<4I', proof_files[0].read_bytes()) if len(proof_files) == 1 else None
    if args.route in ('holodeck-entry', 'turbolift'):
        passed = passed and proof is not None and proof[1] >= 1 and proof[3] == 0
    elif args.route in ('holodeck-return', 'turbolift-return'):
        passed = passed and proof is not None and proof[2] >= 1 and proof[3] == 0
    print('Completed save/read proof:', proof)
    evidence = ROOT/'notes/evidence/virtual_voyager_20260911'/name
    evidence.mkdir()
    for source in folder.glob(name+'*'):
        if source.is_file():
            shutil.copy2(source, evidence/source.name)
    captures = []
    log = (folder/(name+'.run.log')).read_text(errors='replace')
    for filename in re.findall(r'^shot=.*?ok=True.*?file=(.+)$', log, re.M):
        source = Path(filename.strip())
        shutil.copy2(source, evidence/source.name)
        captures.append(source.name)
    (evidence/'result.json').write_text(json.dumps({
        'route': args.route, 'start': start, 'destination': destination,
        'runtime_pass': passed, 'save_proof': proof,
        'captures': captures, 'visual_review': 'pending',
        'scope': 'Normal menu callbacks and authored command; not a complete route playthrough'
    }, indent=2))
    print('Travel runtime:', 'PASS' if passed else 'FAIL')
    print('Native menu/capture review and authored-command log verification remain separate.')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
