"""Measure native co-op frame progress only inside confirmed gameplay windows."""
import argparse
import json
from pathlib import Path

from analyze_sp_host_progress import analyze
from analyze_native_frame_progress import measure
from analyze_native_lows import windows
from analyze_coop_combat import analyze as combat


def summarize(path, cutoff=0.0):
    rows = []
    for line in path.read_text().splitlines():
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            pass  # A live writer may not have completed its last row.
    setup = next(r for r in rows if r['kind'] == 'native_video_setup')
    from xemu_native_video_stats import XEMU_SHA256
    if setup['sha256'] != XEMU_SHA256:
        raise ValueError('Comparison requires the original pinned emulator')
    profiles = '\n'.join(p['line'] for r in rows if r['kind'] == 'fps_profiles'
                         for p in r['records'])
    polls = '\n'.join('xblog t={t} frame={HeartbeatFrame} rt={HeartbeatRealtime} '
                      'st={HeartbeatServerTime} cls={ClsState}'.format(t=r['t'], **r['v'])
                      for r in rows if r['kind'] == 'sample')
    progress = analyze(polls, profiles)
    progress['gameplay_intervals'] = [i for i in progress['gameplay_intervals']
                                     if i['wall_start'] >= cutoff]
    intervals = progress['gameplay_intervals']
    native = [r for r in rows if r['kind'] == 'native_video']
    result = {'source': str(path), 'visual_cutoff': cutoff,
              'progress': progress, 'native_identity': setup,
              'fps_publications_dropped': sum(r['dropped_records'] for r in rows
                                             if r['kind'] == 'fps_profiles')}
    if intervals:
        result['native'] = measure(native, intervals)
        result['lows'] = windows(native, intervals)
        # Keep event continuity across adjacent confirmed intervals.
        joined = []
        for i in intervals:
            if joined and abs(joined[-1]['wall_end'] - i['wall_start']) < .001:
                joined[-1]['wall_end'] = i['wall_end']
            else:
                joined.append(dict(i))
        result['combat'] = combat(rows, joined)
        depleted = next((r for r in rows if r['kind'] == 'coop_players'
                         and r['camera'] is False and r['t'] >= intervals[0]['wall_start']
                         and r['players'][0]['weapon'] in (1, 2)
                         and r['players'][0]['ammo'][1] == 0), None)
        if depleted:
            result['p1_first_energy_empty'] = {
                'wall': depleted['t'], 'command_time': depleted['players'][0]['command_time']}
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('flight', type=Path)
    p.add_argument('--cutoff', type=float, default=0)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = summarize(a.flight, a.cutoff)
    a.output.write_text(json.dumps(result, indent=2))
    print(json.dumps({'native': result.get('native'),
                      'slow_window_fps': result.get('lows', {}).get('minimum_native_fps'),
                      'both_firing': result.get('combat', {}).get('both_players_observed_firing'),
                      'fps_publications_dropped': result['fps_publications_dropped']}, indent=2))
