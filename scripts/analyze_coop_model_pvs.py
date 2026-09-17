"""Summarize asynchronous cumulative co-op PVS evidence inside gameplay only."""
import argparse,json
from pathlib import Path

def summarize(rows,intervals):
    samples=[r for r in rows if r.get('kind')=='coop_model_pvs']
    totals={key:0 for key in ['tested_models','outside_models','eligible_hidden_surfaces','unknown_bounds','outside_md3','outside_mdr','skipped_surfaces']}
    spans=[]
    for interval in intervals:
        s=[r for r in samples if interval['wall_start']<=r['t']<=interval['wall_end']]
        if len(s)<2:continue
        first,last=s[0],s[-1]
        if any(r['v']['mode']!=first['v']['mode'] for r in s):raise ValueError('Mode changed inside measurement')
        delta={k:last['v'][k]-first['v'][k] for k in totals}
        if any(v<0 for v in delta.values()):raise ValueError('Counter reset')
        for k,v in delta.items():totals[k]+=v
        spans.append(dict(start=first['t'],end=last['t'],mode=first['v']['mode'],delta=delta))
    if not spans:raise ValueError('No gameplay PVS observations')
    return dict(spans=spans,totals=totals,modes=sorted(set(s['mode'] for s in spans)),limitations='Asynchronous cumulative counter deltas trimmed to each gameplay interval; no claim of per-frame timing or exact output identity. Mode 2 observes without skipping, mode 1 skips; production default is 0 until qualified.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('flight',type=Path);p.add_argument('progress',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=summarize([json.loads(l) for l in a.flight.read_text().splitlines()],json.loads(a.progress.read_text())['gameplay_intervals']);result['sources']=[str(a.flight),str(a.progress)];a.output.write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='spans'},indent=2))
