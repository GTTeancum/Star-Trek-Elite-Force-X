"""Summarize diagnostic producer-side draw costs; these are not GPU timings."""
import argparse
import json
import statistics
from pathlib import Path
from xemu_flight_recorder import DRAW_KINDS, DRAW_KIND_FIELDS


def summarize(records):
    costs={r['v']['sample']:r['v'] for r in records if r.get('kind')=='frame_cost'}
    kinds=[r['v'] for r in records if r.get('kind')=='draw_kinds']
    paired=[(k,costs[k['sample']]) for k in kinds if k['sample'] in costs and
            k['MainLoopCount']==costs[k['sample']]['MainLoopCount']]
    if len(paired)<8:
        raise ValueError('Insufficient coherent paired frame samples')
    ordered=sorted(paired,key=lambda p:p[0]['frame_guest_ms'])
    count=max(1,len(ordered)//4)
    def group(items):
        totals={kind:{field:sum(k['kinds'][kind][field] for k,c in items)
                      for field in DRAW_KIND_FIELDS} for kind in DRAW_KINDS}
        calls=sum(t['calls'] for t in totals.values())
        cycles=sum(t['draw_cycles'] for t in totals.values())
        return {'samples':len(items), 'median_guest_frame_ms':statistics.median(k['frame_guest_ms'] for k,c in items),
                'median_server_ms':statistics.median(c['PerfServerMsec'] for k,c in items),
                'median_client_ms':statistics.median(c['PerfClientMsec'] for k,c in items),
                'kinds':{kind:{**t,'call_fraction':t['calls']/calls if calls else 0,
                               'draw_cycle_fraction':t['draw_cycles']/cycles if cycles else 0,
                               'calls_per_sample':t['calls']/len(items)} for kind,t in totals.items()}}
    return {'paired_samples':len(paired),'all':group(ordered),
            'fast_quarter':group(ordered[:count]),'slow_quarter':group(ordered[-count:]),
            'limitations':'Diagnostic build only. Indexed entry path, including resident array dispatch; excludes separate immediate-mode entry points. Producer-side cycles include GPU backpressure from earlier draws and are not per-material GPU timings. Quartiles use sampled guest frame duration, not FPS acceptance.'}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('flight',type=Path)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--progress',type=Path,help='Restrict diagnostic samples to independently confirmed gameplay intervals')
    p.add_argument('--start',type=float,help='Restrict samples to host seconds at or after this value')
    p.add_argument('--end',type=float,help='Restrict samples to host seconds at or before this value')
    a=p.parse_args()
    rows=[json.loads(line) for line in a.flight.read_text().splitlines()]
    original_count=len(rows)
    if a.progress:
        intervals=json.loads(a.progress.read_text())['gameplay_intervals']
        rows=[r for r in rows if 't' in r and any(i['wall_start']<=r['t']<=i['wall_end'] for i in intervals)]
    if a.start is not None:
        rows=[r for r in rows if r.get('t',float('-inf'))>=a.start]
    if a.end is not None:
        rows=[r for r in rows if r.get('t',float('inf'))<=a.end]
    result=summarize(rows)
    result['source']=str(a.flight)
    result['gameplay_progress_source']=str(a.progress) if a.progress else None
    result['host_time_range']={'start':a.start,'end':a.end}
    result['excluded_records']=original_count-len(rows)
    a.output.write_text(json.dumps(result,indent=2))
    for group in ['fast_quarter','slow_quarter']:
        g=result[group]
        print(group,g['samples'],'samples, median guest frame',g['median_guest_frame_ms'])
        for kind,t in g['kinds'].items():
            print(kind,'calls/frame',round(t['calls_per_sample'],1),'draw cycles %',round(t['draw_cycle_fraction']*100,1),'beginPush cycles',t['begin_push_cycles'])
