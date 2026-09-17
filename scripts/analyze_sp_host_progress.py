"""Estimate wall-time progress from polled heartbeats confirmed by final logs.

Diagnostic only: heartbeat publication/poll latency and monitor overhead are
included. This is neither presentation timing nor retail FPS qualification.
"""
import argparse
import json
from pathlib import Path
import re


def analyze(polls, profiles):
    records = {}
    anchors = {}
    for line in profiles.splitlines():
        if not line.startswith('STEFX_HW_FPS_SAMPLE:'):
            continue
        fields = dict(re.findall(r'\b(\w+)=(\d+)\b', line))
        keys = ('sample', 'frame', 'realtime', 'serverTime', 'gameplay', 'excludedChecks')
        if not all(key in fields for key in keys):
            continue
        item = {key: int(fields[key]) for key in keys}
        if item['sample'] in records and records[item['sample']] != item:
            raise ValueError('Conflicting final FPS records')
        records[item['sample']] = item
        anchors[(item['frame'], item['realtime'], item['serverTime'])] = item['sample']
    observations = []
    rejected = 0
    for line in polls.splitlines():
        compact = line.startswith('word_snapshot t=')
        if not compact and not line.startswith('xblog t='):
            continue
        fields = dict(re.findall(r'\b(\w+)=(\S+)', line))
        if compact:
            if fields.get('label') != 'sp_heartbeat':
                continue
            try:
                words = [int(value) for value in fields['values'].split(',')]
            except (KeyError, ValueError):
                words = []
            if len(words) != 6 or words[0] != 0x48424653:
                rejected += 1
                continue
            identity = tuple(words[2:5])
        else:
            if not all(key in fields for key in ('t', 'frame', 'rt', 'st', 'cls')):
                continue
            if fields['cls'] != '7':
                rejected += 1
                continue
            identity = tuple(int(fields[key]) for key in ('frame', 'rt', 'st'))
        serial = anchors.get(identity)
        if serial is None:
            rejected += 1
            continue
        observation = {'wall': float(fields['t']), 'serial': serial,
                       'frame': identity[0], 'rt': identity[1], 'st': identity[2]}
        if observations and observation['serial'] == observations[-1]['serial']:
            continue  # A stale heartbeat is not a new measurement.
        observations.append(observation)
    intervals = []
    for before, after in zip(observations, observations[1:]):
        if after['serial'] <= before['serial'] or after['wall'] <= before['wall']:
            continue
        # Include the starting heartbeat's window conservatively because its
        # publication time precedes the host observation by an unknown delay.
        covered = [records.get(serial) for serial in range(before['serial'], after['serial'] + 1)]
        if any(item is None or item['gameplay'] != 1 or item['excludedChecks'] != 0 for item in covered):
            continue
        frames = after['frame'] - before['frame']
        guest_ms = after['rt'] - before['rt']
        wall = after['wall'] - before['wall']
        if frames <= 0 or guest_ms <= 0:
            continue
        intervals.append({'wall_start': before['wall'], 'wall_end': after['wall'],
                          'server_start': before['st'], 'server_end': after['st'],
                          'frames': frames, 'guest_ms': guest_ms, 'wall_seconds': wall,
                          'frames_per_wall_second': frames / wall,
                          'guest_seconds_per_wall_second': guest_ms / (1000 * wall)})
    wall = sum(item['wall_seconds'] for item in intervals)
    frames = sum(item['frames'] for item in intervals)
    guest_ms = sum(item['guest_ms'] for item in intervals)
    return {'confirmed_observations': len(observations), 'rejected_observations': rejected,
            'gameplay_intervals': intervals,
            'frames_per_wall_second': frames / wall if wall else None,
            'guest_seconds_per_wall_second': guest_ms / (wall * 1000) if wall else None,
            'wall_seconds': wall, 'frames': frames,
            'diagnostic_only': True, 'retail_verified': False,
            'limitations': 'Heartbeat publication/poll latency and monitor overhead; not presentation timing.'}


def compare_progress(baseline, candidate):
    left, right = baseline['gameplay_intervals'], candidate['gameplay_intervals']
    if not left or not right:
        raise ValueError('Both diagnostics need confirmed gameplay intervals')
    start = max(left[0]['server_start'], right[0]['server_start'])
    end = min(left[-1]['server_end'], right[-1]['server_end'])
    def retain(items):
        selected = [item for item in items if item['server_start'] >= start and item['server_end'] <= end]
        if len(selected) < 2:
            raise ValueError('Insufficient complete shared gameplay intervals')
        frames = sum(item['frames'] for item in selected)
        wall = sum(item['wall_seconds'] for item in selected)
        guest_ms = sum(item['guest_ms'] for item in selected)
        return {'intervals': len(selected), 'frames': frames, 'wall_seconds': wall,
                'frames_per_wall_second': frames / wall,
                'guest_seconds_per_wall_second': guest_ms / (1000 * wall),
                'server_start': selected[0]['server_start'], 'server_end': selected[-1]['server_end']}
    a, b = retain(left), retain(right)
    return {'shared_server_interval': [start, end], 'baseline': a, 'candidate': b,
            'candidate_vs_baseline_percent': 100 * (b['frames_per_wall_second'] / a['frames_per_wall_second'] - 1),
            'diagnostic_only': True, 'retail_verified': False,
            'limitations': 'Whole intervals have different boundaries; heartbeat lag and polling overhead remain.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('polls', type=Path)
    parser.add_argument('profiles', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--against', type=Path, help='Equivalent baseline diagnostic analysis JSON')
    args = parser.parse_args()
    result = analyze(args.polls.read_text(), args.profiles.read_text())
    result['sources'] = [str(args.polls), str(args.profiles)]
    if args.against:
        result['comparison_to'] = str(args.against)
        result['comparison'] = compare_progress(json.loads(args.against.read_text()), result)
    rendered = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(rendered + '\n')
    print(rendered)


if __name__ == '__main__':
    main()
