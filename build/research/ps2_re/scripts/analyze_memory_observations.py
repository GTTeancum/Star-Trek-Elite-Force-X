"""Summarize fresh guest memory observations against their paired EXE/MAP/XBE."""
import argparse
import bisect
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'scripts'))
from ja_xemu_smoke import pe_sections, xbe_sections


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('flight', type=Path)
    parser.add_argument('map', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--assets', type=Path, help='PK3 directory for phase/model hash candidates')
    args = parser.parse_args()
    contexts = {}
    if args.assets:
        names = set()
        for package in args.assets.iterdir():
            if package.suffix.lower() != '.pk3':
                continue
            with zipfile.ZipFile(package) as archive:
                for name in archive.namelist():
                    if name.lower().endswith('.mdr'):
                        names.update((name, name.lower()))
        for name in sorted(names):
            for phase in ('preflight', 'model-slot-failed', 'load-start', 'load-done'):
                value = 2166136261
                for byte in (phase + '|' + name).encode('ascii'):
                    value = ((value ^ byte) * 16777619) & 0xffffffff
                contexts.setdefault(value, []).append({'phase': phase, 'model': name})
    pe_base, pe = pe_sections(args.map.with_suffix('.exe').read_bytes())
    xbe_data = args.map.with_suffix('.xbe').read_bytes()
    xbe = xbe_sections(xbe_data)
    functions = []
    for line in args.map.read_text(errors='replace').splitlines():
        match = re.match(r'\s+\w{4}:\w{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s+(.*)', line)
        if not match:
            continue
        name, linked, owner = match.groups()
        linked = int(linked, 16)
        for section, va, offset, size in pe:
            relative = linked - pe_base - va
            if 0 <= relative < size:
                for xs, xv, xl in xbe:
                    if xs == section and relative < xl:
                        functions.append((xv + relative, name, owner))
    functions.sort()
    addresses = [row[0] for row in functions]
    records, drops = [], 0
    for line in args.flight.open():
        entry = json.loads(line)
        if entry.get('kind') != 'memory_observations':
            continue
        drops += entry['dropped_records']
        for source in entry['records']:
            row = dict(source, host_observed_seconds=entry['t'])
            if row.get('asset_context'):
                row['asset_context_matches'] = contexts.get(row['asset_context'], [])
            index = bisect.bisect_right(addresses, row['caller']) - 1
            if index >= 0:
                address, name, owner = functions[index]
                row['nearest_preceding_function'] = name
                row['function_offset'] = row['caller'] - address
                row['object'] = owner
            records.append(row)
    if not records:
        raise SystemExit('No fresh observations; refusing to substitute legacy polled values')
    serials = [row['serial'] for row in records]
    if serials != sorted(set(serials)):
        raise SystemExit('Non-monotonic or duplicate publications')
    result = {
        'flight': str(args.flight), 'map': str(args.map),
        'xbe_sha256': hashlib.sha256(xbe_data).hexdigest(),
        'observation_count': len(records), 'dropped_records': drops,
        'asset_candidates_directory': str(args.assets) if args.assets else None,
        'continuous_extrema': False,
        'limits': 'Sample locations are nearest preceding linker functions, not stack unwinds. '
                  'Zone tags exclude externally allocated asset buffers. '
                  'Repeated host polls are not additional memory measurements. '
                  'This diagnostic adds 5124 static bytes before linker/page rounding.',
        'minimum_physical_observation': min(records, key=lambda row: row['physical_free']),
        'minimum_zone_observation': min(records, key=lambda row: row['zone_free']),
        'records': records,
    }
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({key: value for key, value in result.items() if key != 'records'}, indent=2))


if __name__ == '__main__':
    main()
