"""Compare XEMU's own frame counter inside confirmed gameplay intervals.

This uses the counter behind XEMU's FPS overlay, not reciprocal MSPF, guest
time, or exact physical-display presentation timing.
"""
import argparse
import json
from pathlib import Path


def measure(snapshots, intervals):
    blocks = []
    for item in intervals:
        start, end = item['wall_start'], item['wall_end']
        if end <= start:
            raise ValueError('Invalid gameplay interval')
        if blocks and abs(blocks[-1][1] - start) < .001:
            blocks[-1][1] = end
        else:
            blocks.append([start, end])
    selected = []
    for start, end in blocks:
        rows = [r for r in snapshots if start <= r['t'] <= end]
        if len(rows) < 2:
            continue
        if any(b['t'] <= a['t'] or b['frame_count'] < a['frame_count'] for a,b in zip(rows,rows[1:])):
            raise ValueError('Native counter or observation clock reset')
        first, last = rows[0], rows[-1]
        selected.append({'host_start': first['t'], 'host_end': last['t'],
                         'native_frames': last['frame_count'] - first['frame_count'],
                         'host_seconds': last['t'] - first['t']})
    seconds = sum(r['host_seconds'] for r in selected)
    frames = sum(r['native_frames'] for r in selected)
    if not seconds:
        raise ValueError('No usable native observations in gameplay')
    return {'native_frames_per_host_second': frames / seconds,
            'native_frames': frames, 'host_seconds': seconds, 'blocks': selected}


def compare(base_snapshots, base_progress, candidate_snapshots, candidate_progress):
    left, right = base_progress['gameplay_intervals'], candidate_progress['gameplay_intervals']
    if not left or not right:
        raise ValueError('Missing confirmed gameplay intervals')
    start = max(left[0]['server_start'], right[0]['server_start'])
    end = min(left[-1]['server_end'], right[-1]['server_end'])
    def retain(items):
        return [r for r in items if r['server_start'] >= start and r['server_end'] <= end]
    a, b = measure(base_snapshots, retain(left)), measure(candidate_snapshots, retain(right))
    return {'baseline': a, 'candidate': b, 'shared_server_interval': [start, end],
            'gain_native_frames_per_host_second': b['native_frames_per_host_second']-a['native_frames_per_host_second'],
            'retail_verified': False, 'exact_display_presentation': False,
            'limitations': 'Uses the read-increment counter behind XEMU FPS. Whole gameplay boundaries differ slightly; observations trim the ends, and emulator/host contention remains. No MSPF inversion or guest-clock substitution.'}


def read_snapshots(path):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    if not any(r.get('kind') == 'native_video_setup' for r in rows):
        raise ValueError('Missing native ABI identity')
    return [r for r in rows if r.get('kind') == 'native_video']


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('baseline_flight',type=Path)
    p.add_argument('baseline_progress',type=Path)
    p.add_argument('candidate_flight',type=Path)
    p.add_argument('candidate_progress',type=Path)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    result=compare(read_snapshots(a.baseline_flight),json.loads(a.baseline_progress.read_text()),
                   read_snapshots(a.candidate_flight),json.loads(a.candidate_progress.read_text()))
    result['sources']=[str(a.baseline_flight),str(a.baseline_progress),str(a.candidate_flight),str(a.candidate_progress)]
    a.output.write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
