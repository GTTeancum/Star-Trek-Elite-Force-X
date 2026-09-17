"""Bounded read-only sidecar for a running probe; never sends desktop input."""
import argparse,json,re,struct,time
from pathlib import Path
from xemu_native_video_stats import NativeVideoStats
from xemu_flight_recorder import Recorder
from xemu_coop_state import read_players
p=argparse.ArgumentParser();p.add_argument('flight',type=Path);p.add_argument('--seconds',type=int,default=450);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert 0<a.seconds<=900
with a.flight.open() as f:setup=json.loads(next(f))
m=Path(setup['map']).read_text()
def va(name):
 match=re.search(r'(?:^|\s)'+re.escape(name)+r'\s+([0-9A-Fa-f]{8})(?:\s|$)',m)
 return int(match[1],16)-0x3f0000 if match else None
r=NativeVideoStats(setup['pid']);g=Recorder.__new__(Recorder);g.api=r.api;g.handle=r.handle;g.ram=setup['ram_host'];g.cr3=setup['cr3']
assert g.virtual(va('_g_SPXBHeartbeatMagic'),4)==struct.pack('<I',1212302931)
try:
 with a.output.open('x') as out, a.flight.open() as stream:
  start=time.monotonic();latest=None
  while time.monotonic()-start<a.seconds:
   for line in stream:
    try:record=json.loads(line)
    except ValueError:continue
    if record.get('kind')=='sample':latest=record['t']
   if g.virtual(va('_g_SPXBHeartbeatMagic'),4)!=struct.pack('<I',1212302931):break
   players=read_players(g.virtual,va('?g_entities@@3PAUgentity_s@@A'),va('_g_SPXBSplitP2Ent'))
   camera=g.virtual(va('?in_camera@@3_NA'),1);free=g.virtual(va('_g_SPXBPhysAvail'),4)
   record={'epoch':time.time(),'flight_t_latest':latest,'players':players,'camera':bool(camera[0]) if len(camera)==1 else None,'physical_free':struct.unpack('<I',free)[0] if len(free)==4 else None,'atomic':False}
   out.write(json.dumps(record)+'\n');out.flush();time.sleep(1)
finally:r.close()
