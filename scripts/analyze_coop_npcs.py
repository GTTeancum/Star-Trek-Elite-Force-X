"""Describe live Borg and sampled AI engagement; never infer combat from draws."""
import argparse
import collections
import json
import math
import statistics
from pathlib import Path

def analyze(rows, intervals):
    selected=[r for r in rows if r.get('kind')=='coop_npcs' and any(i['wall_start']<=r['t']<i['wall_end'] for i in intervals)]
    if not selected:raise ValueError('No NPC census inside confirmed gameplay')
    players=[r for r in rows if r.get('kind')=='coop_players' and r.get('camera') is False]
    previous={}; actors={}; observations=[]
    for r in selected:
        pp=min(players,key=lambda p:abs(p['t']-r['t'])) if players else None
        if pp is not None and abs(pp['t']-r['t'])>2:pp=None
        live=[n for n in r['npcs'] if n['borg'] and n['health']>0]
        record=dict(t=r['t'],live_borg=len(live),dead_borg=sum(n['borg'] and n['health']<=0 for n in r['npcs']),
                    live_borg_with_target=sum(n['enemy'] is not None for n in live),
                    attack_commands=sum(bool(n['buttons'] & 129) for n in live),rejected=r['rejected'])
        if pp:
            record['players']=[dict(number=p['number'],origin=p['origin'],nearest_live_borg=min((math.dist(p['origin'],n['origin']) for n in live),default=None)) for p in pp['players']]
        observations.append(record)
        for n in r['npcs']:
            if not n['borg']:continue
            key=(n['number'],n['client'],n['npc'])
            a=actors.setdefault(key,dict(number=n['number'],npc_type=n['npc_type'],observations=0,live=0,dead=0,targets=set(),
                firing_events=0,missed_events=0,moving_observations=0,min_health=n['health'],max_health=n['health'],
                behavior=set(),first_origin=n['origin'],last_origin=n['origin'],first_t=r['t'],last_t=r['t']))
            a['observations']+=1;a['live']+=n['health']>0;a['dead']+=n['health']<=0
            a['min_health']=min(a['min_health'],n['health']);a['max_health']=max(a['max_health'],n['health'])
            a['last_origin']=n['origin'];a['last_t']=r['t'];a['behavior'].add(tuple(n['behavior']))
            if n['enemy'] is not None:a['targets'].add(n['enemy'])
            old=previous.get(key);previous[key]=n
            if old and n['command_time']>=old['command_time']:
                a['moving_observations']+=math.dist(old['origin'],n['origin'])>1
                delta=(n['event_sequence']-old['event_sequence'])&0xffffffff
                if delta<100000:
                    a['missed_events']+=max(0,delta-2)
                    a['firing_events']+=sum(n['events'][(n['event_sequence']-k)&1] in (21,22) for k in range(1,min(delta,2)+1))
    for a in actors.values():a['targets']=sorted(a['targets']);a['behavior']=sorted(a['behavior'])
    return dict(census_count=len(selected),intervals=intervals,
        live_borg_range=[min(r['live_borg'] for r in observations),max(r['live_borg'] for r in observations)],
        targeted_borg_range=[min(r['live_borg_with_target'] for r in observations),max(r['live_borg_with_target'] for r in observations)],
        observed_borg_fire_events=sum(a['firing_events'] for a in actors.values()),
        npc_read_host_seconds_median=statistics.median(r['read_host_seconds'] for r in selected),
        actors=list(actors.values()),observations=observations,full_workload_qualified=False,
        limitations='Sparse asynchronous memory observations. Actual event IDs are checked, but missed events, melee and LOS are not reconstructed. A target pointer or attack command alone does not prove a hit. Native captures and representative engagement remain required. Diagnostic census overhead is not FPS qualification.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('flight',type=Path);p.add_argument('progress',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=analyze([json.loads(s) for s in a.flight.read_text().splitlines()],json.loads(a.progress.read_text())['gameplay_intervals'])
    a.output.write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k not in ('actors','observations')},indent=2))
