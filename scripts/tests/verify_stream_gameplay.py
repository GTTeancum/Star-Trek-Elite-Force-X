import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from analyze_stream_gameplay import analyze

def event(n,t,gameplay=1,excluded=0):
 return {'kind':'fps_profiles','t':t,'records':[{'publication':n,'line':f'STEFX_HW_FPS_SAMPLE: sample={n} frame={n*100} realtime={n*5000} serverTime={n*5000} gameplay={gameplay} excludedChecks={excluded}'}]}
rows=[event(1,10,0,10),event(2,20),event(3,30),event(4,40),event(5,50,0,1),event(6,60),event(7,70),event(8,80)]
x=analyze(rows)['gameplay_intervals'];assert [(i['wall_start'],i['wall_end']) for i in x]==[(20,30),(60,70)]
assert not analyze([event(2,20),event(4,40),event(5,50)])['gameplay_intervals']
x=analyze([event(1,10),event(2,100),event(3,110)])['gameplay_intervals'];assert x[0]['wall_end']-x[0]['wall_start']==90
assert not analyze([event(1,10),event(2,20)])['gameplay_intervals']
try:analyze([event(1,10),event(1,11,0)])
except ValueError:pass
else:raise AssertionError('Conflicting publication accepted')
a=event(1,10);a['records'][0]['publication']=2
try:analyze([a])
except ValueError:pass
else:raise AssertionError('Mismatched publication accepted')
print('Stream gameplay: intro, transition/movie overlap, missing and conflicting records, trailing guard and long stalls passed')
