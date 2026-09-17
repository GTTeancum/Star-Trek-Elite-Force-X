"""Count newly observed EF SP firing events; never equate test input with shots."""
import argparse
import json
from pathlib import Path

FIRE_EVENTS = {21: 'primary', 22: 'alternate'}  # Compiled EF SP enums, not JA.


def analyze(rows, intervals):
    result = [dict(slot=slot, observations=0, primary=0, alternate=0,
                   missed_events=0, resets=0, firing_times=[], weapons=set(),
                   ammo_min=None) for slot in range(2)]
    for interval in intervals:
        previous = [None, None]
        for row in rows:
            if row.get('kind') != 'coop_players' or row.get('camera') is not False:
                continue
            if not interval['wall_start'] <= row['t'] < interval['wall_end']:
                continue
            if len(row.get('players', [])) != 2:
                raise ValueError('Missing player observations')
            for slot, player in enumerate(row['players']):
                if not all(key in player for key in ('event_sequence', 'events', 'ammo', 'weapon')):
                    raise ValueError('Recording predates combat evidence; no shot claim possible')
                if len(player['events']) != 2 or len(player['ammo']) != 4:
                    raise ValueError('Wrong EF SP event/ammo layout')
                out = result[slot]
                out['observations'] += 1
                out['weapons'].add(player['weapon'])
                out['ammo_min'] = list(player['ammo']) if out['ammo_min'] is None else [min(a,b) for a,b in zip(out['ammo_min'], player['ammo'])]
                old = previous[slot]
                previous[slot] = player
                if old is None:
                    continue  # Do not count history that predates this interval.
                delta = (player['event_sequence'] - old['event_sequence']) & 0xffffffff
                if (player['client'] != old['client'] or player['entity'] != old['entity'] or
                    player['command_time'] < old['command_time'] or delta > 100000):
                    out['resets'] += 1
                    continue
                out['missed_events'] += max(0, delta - 2)
                for offset in range(min(delta, 2), 0, -1):
                    event = player['events'][(player['event_sequence'] - offset) & 1]
                    if event in FIRE_EVENTS:
                        out[FIRE_EVENTS[event]] += 1
                        out['firing_times'].append(row['t'])
    for out in result:
        out['weapons'] = sorted(out['weapons'])
        out['observed_firing_events'] = out['primary'] + out['alternate']
    if any(not out['observations'] for out in result):
        raise ValueError('No two-player combat observations in confirmed gameplay')
    return dict(players=result,
                both_players_observed_firing=all(out['observed_firing_events'] for out in result),
                full_workload_qualified=False,
                limitations='Asynchronous sampled server event rings; counts are lower bounds, missed events are explicit. Input, ammo, or a few firing events alone do not establish sustained combat, representative routes, FPS acceptance, or retail performance.')


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('flight',type=Path);p.add_argument('progress',type=Path)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=analyze([json.loads(s) for s in a.flight.read_text().splitlines()], json.loads(a.progress.read_text())['gameplay_intervals'])
    result['sources']=[str(a.flight),str(a.progress)]
    a.output.write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='players'},indent=2))
    for player in result['players']:
        print({k:v for k,v in player.items() if k!='firing_times'})
