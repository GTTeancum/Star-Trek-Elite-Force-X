from pathlib import Path
import sys,struct,json,hashlib
sys.path.insert(0,str(Path('build/research/ps2_re/python_deps').resolve()))
import lzokay
root=Path('build/research/ps2_re');b=(root/'BORG1.PMP').read_bytes();n=struct.unpack_from('<I',b)[0];out=root/'borg1_entries';out.mkdir(exist_ok=True);rows=[]
for i in range(n):
 h,o,c,u=struct.unpack_from('<4I',b,4+16*i);d=b[o:o+c];r=dict(index=i,hash='%08x'%h,offset=o,stored=c,size=u)
 try:
  x=lzokay.decompress(d,u);assert len(x)==u
  r.update(decoded=len(x),magic=x[:16].hex(),sha256=hashlib.sha256(x).hexdigest())
  if x[:4] in (b'IBSP',b'RBSP'):
   path=out/('%08x.bsp'%h);path.write_bytes(x);r['file']=str(path)
 except Exception as e:r['error']=str(e)[:100]
 rows.append(r)
(root/'borg1_lzo_validation.json').write_text(json.dumps(rows,indent=2))
print('decoded',sum('decoded'in r for r in rows),'of',n,'maps',[(r['index'],r.get('file')) for r in rows if 'file'in r]);print(rows[:2])
