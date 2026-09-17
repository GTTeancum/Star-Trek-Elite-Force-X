"""Compare complete gameplay windows over a shared server-time interval.

Rates use complete ft= guest elapsed-time records, not simulation time,
host presentation or retail proof. Read only the current guest FPS ring.
Never prorate a window across an intro or comparison boundary.
"""
import argparse
import json
from pathlib import Path
import re
from run_sp_perf_probe import verify_mp_workload


def windows(text):
    records = {}
    for line in text.splitlines():
        if not line.startswith('STEFX_HW_FPS_SAMPLE:'):
            continue
        fields = dict(re.findall(r'\b(\w+)=(\d+)\b', line))
        required = ('sample', 'frame', 'realtime', 'serverTime', 'gameplay', 'excludedChecks')
        if not all(key in fields for key in required):
            continue
        record = {key: int(fields[key]) for key in required}
        timing = re.search(r'\bft=(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)(?=\s|$)', line)
        record['timing'] = tuple(map(int, timing.groups())) if timing else None
        serial = record['sample']
        if serial in records and records[serial] != record:
            raise ValueError('Conflicting FPS sample serial')
        records[serial] = record
    result = []
    previous = None
    for record in sorted(records.values(), key=lambda item: item['sample']):
        if previous and record['sample'] == previous['sample'] + 1:
            timing = record['timing']
            frames, elapsed = timing[:2] if timing else (0, 0)
            if (record['gameplay'] == 1 and record['excludedChecks'] == 0
                    and elapsed > 0 and frames > 0
                    and record['serverTime'] > previous['serverTime']):
                result.append({'start': previous['serverTime'],
                               'end': record['serverTime'], 'frames': frames,
                               'guest_ms': elapsed, 'fps': frames * 1000 / elapsed})
        previous = record
    return result


def compare(left, right):
    if not left or not right:
        raise ValueError('Both runs need complete tagged gameplay windows')
    start = max(left[0]['start'], right[0]['start'])
    end = min(left[-1]['end'], right[-1]['end'])
    def retained(items):
        selected = [item for item in items if item['start'] >= start and item['end'] <= end]
        if len(selected) < 2:
            raise ValueError('Insufficient complete windows in shared interval')
        frames = sum(item['frames'] for item in selected)
        elapsed = sum(item['guest_ms'] for item in selected)
        return {'windows': len(selected), 'frames': frames, 'guest_ms': elapsed,
                'weighted_guest_fps': frames * 1000 / elapsed,
                'minimum_window_fps': min(item['fps'] for item in selected),
                'retained_server_start': selected[0]['start'],
                'retained_server_end': selected[-1]['end']}
    a, b = retained(left), retained(right)
    return {'shared_server_interval': [start, end], 'left': a, 'right': b,
            'right_vs_left_percent': 100 * (b['weighted_guest_fps'] / a['weighted_guest_fps'] - 1),
            'timing_basis': 'guest elapsed ft records; not host presentation FPS',
            'retail_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('left', type=Path, help='Baseline probe summary JSON')
    parser.add_argument('right', type=Path, help='Candidate probe summary JSON')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    summaries = [json.loads(path.read_text()) for path in (args.left, args.right)]
    for key in ('map', 'mode'):
        if summaries[0][key] != summaries[1][key]:
            raise ValueError('Paired comparison requires matching ' + key)
    for summary in summaries:
        if (not summary['mode_verified'] or not summary['finished_in_game']
                or summary['visual_only'] or summary['diagnostic_only']
                or summary.get('god_mode_requested') or summary.get('god_mode')
                or not summary.get('final_liveness_verified')
                or (summary.get('mode') == 'mp' and
                    (not summary.get('simulation_verified') or not verify_mp_workload(summary)))
                or summary.get('final_client_state') != 7
                or summary.get('loaded_map') != summary.get('map')
                or summary.get('engine_error_message')
                or summary.get('current_fps_error')
                or summary.get('profile_provenance') != 'current paused guest FPS ring'):
            raise ValueError('Both runs must be clean completed gameplay probes')
    raw = []
    for path in (args.left, args.right):
        name = path.name.removesuffix('.summary.json')
        transaction = json.loads(path.with_name(name + '.iso_transaction.json').read_text())
        if not transaction['restored']:
            raise ValueError('Probe ISO transaction remains open')
        profiles = path.with_name(name + '.current_profiles.log')
        if not profiles.is_file():
            raise ValueError('Need current guest FPS ring for each probe')
        raw.append(windows(profiles.read_text()))
    result = compare(*raw)
    result.update({'left_probe': str(args.left), 'right_probe': str(args.right),
                   'left_xbe_sha256': summaries[0]['xbe_sha256'],
                   'right_xbe_sha256': summaries[1]['xbe_sha256']})
    rendered = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(rendered + '\n')
    print(rendered)


if __name__ == '__main__':
    main()
