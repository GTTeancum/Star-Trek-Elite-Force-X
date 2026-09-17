"""Bounded read-only process CPU context; no command lines, UI or process changes."""
import argparse,json,time
from pathlib import Path
import psutil
p=argparse.ArgumentParser();p.add_argument('--seconds',type=int,default=300);p.add_argument('--output',type=Path,required=True);a=p.parse_args();assert 0<a.seconds<=900
start=time.monotonic();previous={};last=time.monotonic()
with a.output.open('x',encoding='utf-8') as out:
 while time.monotonic()-start<a.seconds:
  now=time.monotonic();current={};deltas=[]
  for proc in psutil.process_iter(['pid','name','create_time','cpu_times']):
   info=proc.info;cpu=info['cpu_times']
   if info['pid']==0:continue
   if cpu is None or info['create_time'] is None:continue
   identity=(info['pid'],info['create_time']);total=cpu.user+cpu.system;current[identity]=total
   if identity in previous and total>=previous[identity]:
    cores=(total-previous[identity])/(now-last)
    if cores>.02:deltas.append({'pid':info['pid'],'created':info['create_time'],'name':info['name'],'cpu_cores':cores})
  if previous:
   out.write(json.dumps({'epoch':time.time(),'elapsed':now-start,'window_seconds':now-last,'top_processes':sorted(deltas,key=lambda p:p['cpu_cores'],reverse=True)[:12]})+'\n');out.flush()
  previous=current;last=now;time.sleep(2)
