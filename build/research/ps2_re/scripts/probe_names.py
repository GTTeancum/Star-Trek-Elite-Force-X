from pathlib import Path
import zipfile,json,zlib,collections
root=Path('build/research/ps2_re');rows=json.loads((root/'borg1_lzo_validation.json').read_text());hashes={int(x['hash'],16):x for x in rows};names=[]
for i in range(4):
 with zipfile.ZipFile(Path('build/release/BaseEF')/f'PAK{i}.PK3') as z:names+=z.namelist()
names=list(set(names));hits=collections.defaultdict(list)
def fnv(s):
 h=2166136261
 for c in s:h=((h^c)*16777619)&0xffffffff
 return h
def djb(s):
 h=0
 for c in s:h=(h*33+c)&0xffffffff
 return h
for n in names:
 for form,t in [('exact',n),('lower',n.lower()),('upper',n.upper()),('backslash',n.lower().replace('/','\\'))]:
  b=t.encode('latin1')
  for algo,h in [('crc',zlib.crc32(b)),('crc_invert',zlib.crc32(b)^0xffffffff),('fnv',fnv(b)),('djb',djb(b))]:
   if h in hashes:hits[algo+'_'+form].append((n,'%08x'%h))
(root/'filename_hash_probe.json').write_text(json.dumps(hits,indent=2))
print({k:len(v) for k,v in hits.items()})
for k,v in hits.items():print(k,v[:4])
