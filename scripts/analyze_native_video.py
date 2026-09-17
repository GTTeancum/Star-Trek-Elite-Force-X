"""Summarize all captured completed XEMU profiler records, excluding initial backfill.

Counts describe submitted work, not time spent in each API. Correlation is not
causation. MSPF is XEMU's read-increment to flip-stall interval, not reciprocal
presentation FPS and not a GPU-only timer.
"""
import argparse
import collections
import json
from pathlib import Path
import statistics
from xemu_native_video_stats import COUNTERS, XEMU_SHA256, LRU_PROBE_SHA256


def summarize(records, intervals=None):
    setup = next((r for r in records if r.get('kind') == 'native_video_setup'), None)
    valid_hash = setup and (setup.get('sha256') == XEMU_SHA256 or
        (setup.get('sha256') == LRU_PROBE_SHA256 and setup.get('binary_variant') == 'isolated_lru_guard_probe'))
    if not valid_hash or setup.get('counter_names') != COUNTERS:
        raise ValueError('Missing validated native-video ABI identity')
    snapshots = [r for r in records if r.get('kind') == 'native_video']
    if len(snapshots) < 2:
        raise ValueError('Need more than initial native-history backfill')
    frames, seen, dropped = [], set(), 0
    retained_snapshots, excluded = [], 0
    previous_t = snapshots[0]['t']
    # The initial 299 entries can precede gameplay or the observation interval.
    previous = snapshots[0]['frame_count'] - 1
    for snapshot in snapshots[1:]:
        # A batch can straddle the text intro or a movie. Its end timestamp
        # alone cannot establish that every captured frame was gameplay.
        keep = intervals is None or any(i['wall_start'] <= previous_t <= snapshot['t'] <= i['wall_end'] for i in intervals)
        previous_t = snapshot['t']
        if keep:
            retained_snapshots.append(snapshot)
        dropped += snapshot['dropped_records']
        for f in snapshot['frames']:
            sequence = f['sequence']
            if sequence in seen or sequence <= previous:
                raise ValueError('Duplicate or out-of-order native frame history')
            if len(f['counters']) != len(COUNTERS) or f['mspf'] < 0:
                raise ValueError('Invalid native profiler row')
            seen.add(sequence); previous = sequence
            if keep:
                frames.append({'observed_t': snapshot['t'], **f})
            else:
                excluded += 1
    if not frames:
        raise ValueError('No completed frames after initial backfill')

    def group(items):
        medians = {name: statistics.median(f['counters'][i] for f in items)
                   for i, name in enumerate(COUNTERS)}
        draws = sum(f['counters'][COUNTERS.index('BEGIN_ENDS')] for f in items)
        return {'records': len(items), 'median_mspf': statistics.median(f['mspf'] for f in items),
                'max_mspf': max(f['mspf'] for f in items), 'counter_medians': medians,
                'totals_per_draw': {name: sum(f['counters'][COUNTERS.index(name)] for f in items)/draws if draws else None
                                   for name in ['ATTR_BIND','GEOM_BUFFER_UPDATE_1','GEOM_BUFFER_UPDATE_4',
                                                'SHADER_BIND','DRAW_ARRAYS','INLINE_ELEMENTS']}}

    ordered = sorted(frames, key=lambda f: f['mspf'])
    quartile = max(1, len(ordered)//4)
    windows = collections.defaultdict(list)
    for f in frames:
        windows[int(f['observed_t']//60)*60].append(f)
    return {'validated_abi': setup, 'initial_history_excluded': True,
            'gameplay_filtered': intervals is not None,
            'non_gameplay_or_boundary_records_excluded': excluded,
            'records': len(frames), 'dropped_records': dropped,
            'observed_host_interval': [snapshots[0]['t'], snapshots[-1]['t']],
            'native_fps_snapshot_median': statistics.median(r['increment_fps'] for r in retained_snapshots),
            'all': group(frames), 'fast_quarter': group(ordered[:quartile]),
            'slow_quarter': group(ordered[-quartile:]),
            'windows': [{'start_host_s': t, **group(g)} for t,g in sorted(windows.items())],
            'worst': [{'observed_t': f['observed_t'], 'sequence': f['sequence'], 'mspf': f['mspf'],
                       'counters': dict(zip(COUNTERS, f['counters']))} for f in reversed(ordered[-20:])],
            'limitations': 'Observer times bound batches of records; MSPF is not GPU-only time or reciprocal FPS. Counts do not identify API durations; different views/workloads and host contention can confound comparisons. No retail qualification.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flight', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--progress', type=Path, help='Confirmed gameplay intervals; exclude entire boundary-crossing batches')
    args = parser.parse_args()
    records = []
    for line in args.flight.read_text().splitlines():
        try: records.append(json.loads(line))
        except json.JSONDecodeError: pass  # Allow an unfinished final live line.
    intervals = json.loads(args.progress.read_text())['gameplay_intervals'] if args.progress else None
    result = summarize(records, intervals)
    result['source'] = str(args.flight)
    args.output.write_text(json.dumps(result, indent=2))
    for name in ['all','fast_quarter','slow_quarter']:
        g = result[name]
        print(name, 'records',g['records'],'median MSPF',g['median_mspf'],'draws',g['counter_medians']['BEGIN_ENDS'],
              'vertex updates/draw',g['totals_per_draw']['GEOM_BUFFER_UPDATE_1'],
              'index updates/draw',g['totals_per_draw']['GEOM_BUFFER_UPDATE_4'])
