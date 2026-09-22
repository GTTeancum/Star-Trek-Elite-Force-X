"""Compare retained gameplay and expose concurrent host GPU work explicitly."""
import argparse
import json
from pathlib import Path

from analyze_native_frame_progress import compare, read_snapshots
from analyze_native_lows import windows


def gpu_context(path, flight):
    with flight.open() as stream:
        clock = next(json.loads(line)['epoch_start'] for line in stream
                     if '"epoch_start"' in line)
    rows = [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines()]
    activity = [{'t': r['epoch'] - clock, 'percent': e['percent']}
                for r in rows for e in r['engines'] if e['name'] == 'GranTurismo2PC']
    return {'source': str(path), 'samples': len(rows),
            'invalid_samples': sum(bool(r.get('invalid_instances')) for r in rows),
            'other_game_activity': activity,
            'first_other_game_gpu_activity': activity[0]['t'] if activity else None,
            'limitations': 'GPU counters do not establish absence of other CPU/disk activity.'}


def pair(a, b, left, right):
    result = compare(a, left, b, right)
    base = result['baseline']['native_frames_per_host_second']
    cand = result['candidate']['native_frames_per_host_second']
    result['percent_change'] = 100 * (cand / base - 1)
    for name, snapshots in [('baseline', a), ('candidate', b)]:
        blocks = result[name]['blocks']
        lows = windows(snapshots, [{'wall_start': r['host_start'], 'wall_end': r['host_end']}
                                   for r in blocks])
        result[name]['slow_window_fps'] = lows['minimum_native_fps']
        result[name]['slowest_window'] = lows['slowest'][0]
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('baseline', type=Path)
    p.add_argument('candidate', type=Path)
    p.add_argument('baseline_gpu', type=Path)
    p.add_argument('candidate_gpu', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    left = json.loads(args.baseline.read_text())
    right = json.loads(args.candidate.read_text())
    a = read_snapshots(Path(left['source']))
    b = read_snapshots(Path(right['source']))
    ga = gpu_context(args.baseline_gpu, Path(left['source']))
    gb = gpu_context(args.candidate_gpu, Path(right['source']))
    result = {'shared_gameplay': pair(a, b, left['progress'], right['progress']),
              'baseline_gpu': ga, 'candidate_gpu': gb}
    # Predefined guard before the observed external GPU activity. This is a
    # sensitivity comparison, not proof of a controlled host environment.
    def before_activity(progress, gpu):
        end = gpu['first_other_game_gpu_activity']
        return {'gameplay_intervals': [r for r in progress['gameplay_intervals']
                if end is None or r['wall_end'] < end - 30]}
    result['before_recorded_external_gpu_activity'] = pair(
        a, b, before_activity(left['progress'], ga), before_activity(right['progress'], gb))
    result['limitations'] = ('Different combat and views, other host work, and separate '
        'binaries prevent treating the percentage as an isolated renderer effect. '
        'Both runs preserve stalls and exclude the authored opening.')
    args.output.write_text(json.dumps(result, indent=2))
    for label in ['shared_gameplay', 'before_recorded_external_gpu_activity']:
        item = result[label]
        print(label, 'baseline', item['baseline']['native_frames_per_host_second'],
              'candidate', item['candidate']['native_frames_per_host_second'],
              'change', item['percent_change'], 'server interval', item['shared_server_interval'])
