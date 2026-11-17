"""Analyze existing Xbox-only records; no network, execution or input."""
from pathlib import Path
import argparse
import json
import re


def signed(value):
    return value if value < 0x80000000 else value-0x100000000


def analyze(path):
    identity=json.loads((path/'identity.json').read_text(encoding='utf-8'))
    records=[json.loads(line) for line in (path/'flight.jsonl').read_text(encoding='utf-8').splitlines()]
    slots=2 if identity['mode']=='coop' else 4
    positions=[set() for _ in range(slots)]
    fires=[[] for _ in range(slots)]
    health=[[] for _ in range(slots)]
    ammo=[[] for _ in range(slots)]
    weapons=[set() for _ in range(slots)]
    phases=[];rosters=set();npcs=[];engaged=[];npc_fires=[];samples=0
    alive_samples=[];roster_samples=[];previous_server_time=None
    for record in records:
        state=record.get('_g_SPXBConsoleWorkload')
        if state and state[0] and state[1]==1 and not state[0]&1:
            samples+=1;rosters.add(tuple(state[5:8]));npcs.append(state[8]);engaged.append(state[9]);npc_fires.append(state[11])
            alive_samples.append((state[2],all(state[12+i*16+13] and signed(state[12+i*16+1])>0 for i in range(slots))))
            roster_samples.append((state[2],tuple(state[5:8])==(8,8,4)))
            for slot in range(slots):
                p=state[12+slot*16:28+slot*16]
                if not p[13]:continue
                positions[slot].add(tuple(p[2:5]));fires[slot].append(p[7]);health[slot].append(signed(p[1]))
                ammo[slot].append(signed(p[6]));weapons[slot].add(p[5])
        for publication in record.get('fps',[]):
            text=publication['text']
            server_match=re.search(r'\bserverTime=(\d+)\b',text)
            server_end=int(server_match[1]) if server_match else None
            server_start=previous_server_time;previous_server_time=server_end
            m=re.search(r'\bft=(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)\b',text)
            qualified=bool(re.search(r'\bgameplay=1\b',text) and re.search(r'\bexcludedChecks=0\b',text))
            if not m or not qualified:
                phases.append(None);continue
            count,elapsed,maximum,p95,p99,over50=map(int,m.groups())
            if count<=0 or elapsed<=0 or maximum>elapsed or not 0<=p95<=p99<=maximum or over50>count:
                raise ValueError('Malformed complete FPS publication: '+text)
            phases.append(dict(serial=publication['serial'],frames=count,ms=elapsed,max_ms=maximum,
                               p95_upper_ms=p95,p99_upper_ms=p99,over50=over50,fps=count*1000/elapsed,
                               server_start=server_start,server_end=server_end))
    engine_valid=[x for x in phases if x]
    # Co-op cannot be qualified from a surviving P1 plus a static dead P2 view.
    # Keep complete publications bracketed by observed living-player samples;
    # stop at the first observed co-op death after both players became active.
    alive_start=alive_end=None
    if slots==2:
        for stamp,alive in alive_samples:
            if alive:
                if alive_start is None: alive_start=stamp
                alive_end=stamp
            elif alive_start is not None: break
        phases=[x if x and alive_start is not None and x['server_start'] is not None and
                x['server_end'] is not None and alive_start<=x['server_start']<=x['server_end']<=alive_end
                else None for x in phases]
    roster_start=roster_end=None
    if slots==4:
        for stamp,complete in roster_samples:
            if complete:
                if roster_start is None: roster_start=stamp
                roster_end=stamp
            elif roster_start is not None: break
        phases=[x if x and roster_start is not None and x['server_start'] is not None and
                x['server_end'] is not None and roster_start<=x['server_start']<=x['server_end']<=roster_end
                else None for x in phases]
    valid=[x for x in phases if x]
    frame_count=sum(x['frames'] for x in valid);milliseconds=sum(x['ms'] for x in valid)
    # Complete adjacent publication windows spanning at least ten seconds.
    # Boundary resolution is a whole publication, never invented per-frame data.
    windows=[]
    for start in range(len(phases)):
        elapsed=count=0;last=None
        for part in phases[start:]:
            if not part or last is not None and part['serial']!=last+1:break
            elapsed+=part['ms'];count+=part['frames'];last=part['serial']
            if elapsed>=10000:
                windows.append(dict(first=phases[start]['serial'],last=last,ms=elapsed,
                                    frames=count,fps=count*1000/elapsed));break
    players=[dict(slot=i,distinct_positions=len(positions[i]),
        firing_events=max(fires[i])-min(fires[i]) if fires[i] else None,
        min_health=min(health[i]) if health[i] else None,
        min_ammo=min(ammo[i]) if ammo[i] else None,weapons=sorted(weapons[i])) for i in range(slots)]
    return dict(map=identity['map'],mode=identity['mode'],profile=identity['profile'],
        xbe_sha256=identity['xbe_sha256'],timing_with_captures=bool(identity['captures']),
        gameplay_frames=frame_count,gameplay_ms=milliseconds,
        engine_eligible_publications=len(engine_valid),
        coop_alive_observation_interval=[alive_start,alive_end] if slots==2 else None,
        holomatch_full_roster_interval=[roster_start,roster_end] if slots==4 else None,
        workload_boundary='Complete windows bracketed by observed living co-op players or a full 8-player/4-bot Holomatch roster; Holomatch includes normal deaths/respawns',
        gameplay_fps=frame_count*1000/milliseconds if milliseconds else None,
        slowest_complete_publication_window=min(windows,key=lambda x:x['fps']) if windows else None,
        window_boundary='Adjacent complete FPS publications totaling at least 10 seconds; not exact rolling 10-second windows',
        max_frame_ms=max((x['max_ms'] for x in valid),default=None),
        frames_over_50ms=sum(x['over50'] for x in valid),
        fps_publications=len(valid),fps_dropped=sum(r.get('fps_dropped',0) for r in records),
        read_errors=sum('read_error' in r for r in records),
        engine_errors=[r['engine_error'] for r in records if r.get('engine_error')],
        workload_samples=samples,players=players,observed_rosters=sorted(rosters),
        max_living_npcs=max(npcs,default=None),max_engaged_npcs=max(engaged,default=None),
        npc_firing_events=max(npc_fires)-min(npc_fires) if npc_fires else None,
        all_players_moved_and_fired=all(p['distinct_positions']>2 and (p['firing_events'] or 0)>0 for p in players),
        full_holomatch_roster_observed=(8,8,4) in rosters if slots==4 else None,
        endpoint=json.loads((path/'endpoint.json').read_text(encoding='utf-8')),
        final_counters=next(({k:v for k,v in r.items() if k.startswith('_g_SPXB') and 'ConsoleWorkload' not in k}
                             for r in reversed(records) if '_g_SPXBMainLoopCount' in r),{}))


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('run',type=Path)
    args=ap.parse_args();root=Path(__file__).resolve().parents[1];path=args.run.resolve()
    if not path.is_relative_to(root):raise ValueError('Evidence must remain inside repository')
    result=analyze(path)
    (path/'analysis.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='final_counters'},indent=2))


if __name__=='__main__':main()
