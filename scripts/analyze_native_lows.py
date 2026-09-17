"""Native XEMU slow windows inside independently confirmed gameplay.

Windows begin at each native observation and end at the last observation no
later than ten seconds afterward. Actual elapsed time is the denominator;
at least 80% coverage is required. Stalled counters remain zero-FPS samples.
No interpolation, MSPF inversion, or guest-clock substitution is used.
"""
import argparse
import bisect
import json
from pathlib import Path
from analyze_native_frame_progress import measure, read_snapshots


def windows(snapshots, intervals, width=10.0):
    if width <= 0:
        raise ValueError('Window duration must be positive')
    blocks = measure(snapshots, intervals)['blocks']
    result = []
    for block in blocks:
        rows = [r for r in snapshots if block['host_start'] <= r['t'] <= block['host_end']]
        times = [r['t'] for r in rows]
        for first in rows:
            end = first['t'] + width
            if end > times[-1]:
                break
            last = rows[bisect.bisect_right(times, end) - 1]
            elapsed = last['t'] - first['t']
            if elapsed < width * .8:
                continue
            count = last['frame_count'] - first['frame_count']
            result.append({'start': first['t'], 'end': last['t'],
                           'host_seconds': elapsed, 'native_frames': count,
                           'native_fps': count / elapsed,
                           'complete_window_end': end,
                           'complete_window_fps_lower_bound': count / width})
    if not result:
        raise ValueError('No complete windows with sufficient observation coverage')
    ordered = sorted(result, key=lambda r:r['native_fps'])
    return {'width_seconds': width, 'overlapping': True,
            'windows': result, 'window_count': len(result),
            'minimum_native_fps': ordered[0]['native_fps'],
            'minimum_complete_window_fps_lower_bound': min(r['complete_window_fps_lower_bound'] for r in result),
            'complete_windows_not_proven_above_30': sum(r['complete_window_fps_lower_bound'] <= 30 for r in result),
            'windows_at_or_below_30': sum(r['native_fps'] <= 30 for r in result),
            'slowest': ordered[:20],
            'limitations': 'native_fps uses the observed 80–100% of nominal duration. complete_window_fps_lower_bound divides only observed frames by the full window duration, conservatively omitting unobserved trailing frames; only windows with the full duration inside confirmed gameplay are included. No claim of instantaneous minima, physical presentation, or retail performance.'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('flight', type=Path)
    p.add_argument('progress', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = windows(read_snapshots(a.flight), json.loads(a.progress.read_text())['gameplay_intervals'])
    result['sources'] = [str(a.flight), str(a.progress)]
    a.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({k:v for k,v in result.items() if k not in ['windows','slowest']}, indent=2))
    print('Worst:', result['slowest'][0])
