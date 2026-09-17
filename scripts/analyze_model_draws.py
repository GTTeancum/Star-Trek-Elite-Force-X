"""Attribute completed diagnostic frames to model/material keys, retaining overflow."""
import argparse,json,statistics
from pathlib import Path

def analyze(rows,intervals=None,start=None,end=None):
 if intervals is not None:rows=[r for r in rows if 't' in r and any(i['wall_start']<=r['t']<=i['wall_end'] for i in intervals)]
 if start is not None:rows=[r for r in rows if r.get('t',float('-inf'))>=start]
 if end is not None:rows=[r for r in rows if r.get('t',float('inf'))<=end]
 kinds={r['v']['sample']:r['v'] for r in rows if r.get('kind')=='draw_kinds'}
 models=[r['v'] for r in rows if r.get('kind')=='model_draws']
 paired=[m for m in models if m['sample'] in kinds and m['MainLoopCount']==kinds[m['sample']]['MainLoopCount']]
 if len(paired)<8:raise ValueError('Insufficient coherent gameplay model samples')
 for m in paired:
  k=kinds[m['sample']]['kinds']['model']
  for field in ('calls','vertices','indices','begin_push_cycles'):
   if sum(r[field] for r in m['rows'])!=k[field]:raise ValueError('Model totals disagree with draw-kind publication: '+field)
 ordered=sorted(paired,key=lambda r:r['frame_guest_ms']);n=max(1,len(ordered)//4)
 def group(items):
  totals={}
  for m in items:
   for r in m['rows']:
    key=(r['handle'],r['shader_pointer'],r['model'],r['shader'],r['overflow'])
    v=totals.setdefault(key,dict(handle=key[0],shader_pointer=key[1],model=key[2],shader=key[3],overflow=key[4],calls=0,vertices=0,indices=0,draw_cycles=0,begin_push_cycles=0))
    for f in ('calls','vertices','indices','draw_cycles','begin_push_cycles'):v[f]+=r[f]
  total=sum(r['draw_cycles'] for r in totals.values())
  for r in totals.values():r.update(calls_per_sample=r['calls']/len(items),cycle_fraction=r['draw_cycles']/total if total else 0)
  return dict(samples=len(items),median_guest_frame_ms=statistics.median(m['frame_guest_ms'] for m in items),materials=sorted(totals.values(),key=lambda r:r['draw_cycles'],reverse=True))
 return dict(paired_samples=len(paired),all=group(ordered),fast_quarter=group(ordered[:n]),slow_quarter=group(ordered[-n:]),limitations='Diagnostic producer-side cycles include GPU backpressure. Model/material names are asynchronous annotations. Quartiles use guest duration, not native FPS. Overflow remains explicit.')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('flight',type=Path);p.add_argument('progress',type=Path,nargs='?');p.add_argument('--start',type=float);p.add_argument('--end',type=float);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 intervals=json.loads(a.progress.read_text())['gameplay_intervals'] if a.progress else None
 result=analyze([json.loads(l) for l in a.flight.read_text().splitlines()],intervals,a.start,a.end)
 result['sources']=[str(a.flight),str(a.progress) if a.progress else None];result['host_time_range']={'start':a.start,'end':a.end};a.output.write_text(json.dumps(result,indent=2))
 for g in ('fast_quarter','slow_quarter'):
  print(g,result[g]['samples'],'samples, median guest ms',result[g]['median_guest_frame_ms'])
  for r in result[g]['materials'][:12]:print(r['model'],r['shader'],'calls/frame',round(r['calls_per_sample'],1),'model cycle %',round(r['cycle_fraction']*100,1),'overflow',r['overflow'])
