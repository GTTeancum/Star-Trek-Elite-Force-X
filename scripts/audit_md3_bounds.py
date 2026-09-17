"""Check every disk MD3 vertex/frame against authored frame boxes (read-only)."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
import numpy as np

def audit(data):
    if data[:4]!=b'IDP3' or struct.unpack_from('<i',data,4)[0]!=15:raise ValueError('Unsupported MD3 format')
    _,nf,nt,ns,skins,frame_at,tags_at,surface_at,end=struct.unpack_from('<9i',data,72)
    if not (0<=nf and 0<=ns and 108<=end<=len(data)):raise ValueError('Invalid MD3 header')
    if ns==0:return dict(frames=nf,vertices=0,max_box_excess=0,within_local_padding=True,no_render_geometry=True)
    bounds=np.ndarray((nf,6),'<f4',data,frame_at,strides=(56,4)).copy()
    if not np.isfinite(bounds).all() or not (bounds[:,:3]<=bounds[:,3:]).all():raise ValueError('Invalid bounds')
    worst=-float('inf');vertices=0
    for _ in range(ns):
        flags,snf,nsh,nv,ntri,tri,shader,st,xyz,size=struct.unpack_from('<10i',data,surface_at+68)
        if nv==0 and ntri==0 and size==0 and _==ns-1:break # empty trailing exporter surface
        if snf!=nf or nv<0 or size<=0:raise ValueError('Invalid surface')
        if not nf or not nv:
            surface_at+=size;continue
        points=np.ndarray((nf,nv,3),'<i2',data,surface_at+xyz,strides=(nv*8,8,2)).astype(np.float32)/64
        low,high=points.min(axis=1),points.max(axis=1)
        worst=max(worst,float(np.maximum(bounds[:,:3]-low,high-bounds[:,3:]).max()))
        vertices+=nf*nv;surface_at+=size
    if surface_at>end:raise ValueError('Invalid surface chain')
    return dict(frames=nf,vertices=vertices,max_box_excess=worst if vertices else 0,within_local_padding=worst<=.125,no_render_geometry=vertices==0,trailing_declared_bytes=end-surface_at)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',type=Path,default=Path('build/release/BaseEF'));p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    sources={};archives={}
    for package in ['PAK0.PK3','PAK1.PK3','PAK2.PK3','PAK3.PK3','xbox0.pk3']:
        z=archives[package]=zipfile.ZipFile(a.base/package)
        for e in z.infolist():
            if e.filename.lower().endswith('.md3'):sources[e.filename.lower()]=(package,e.filename)
    results={};cache={}
    for name,(package,entry) in sorted(sources.items()):
        data=archives[package].read(entry);sha=hashlib.sha256(data).hexdigest()
        if sha not in cache:
            try:cache[sha]=audit(data)
            except Exception as exc:cache[sha]=dict(within_local_padding=False,error=str(exc),frames=0)
        results[name]=dict(cache[sha],package=package,sha256=sha)
    for z in archives.values():z.close()
    result=dict(models=results,all_within_padding=all(r['within_local_padding'] for r in results.values()),local_padding=.125)
    a.output.write_text(json.dumps(result,indent=2));print('MD3 models',len(results),'frames',sum(r['frames'] for r in results.values()),'failures',sum(not r['within_local_padding'] for r in results.values()))
