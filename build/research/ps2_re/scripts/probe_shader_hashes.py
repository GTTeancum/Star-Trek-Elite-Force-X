from pathlib import Path
import json,zipfile,re,zlib,struct,collections
base=Path('build/research/ps2_re')
data=(base/'borg1_entries/75be6d3d.bin').read_bytes()
n=struct.unpack_from('<I',data)[0]
hashes={struct.unpack_from('<I',data,4+8*i)[0] for i in range(n)}
names=set()
for path in (Path('build/release/BaseEF')/n for n in ['PAK0.PK3','PAK1.PK3','PAK2.PK3','PAK3.PK3','xbox0.pk3']):
    with zipfile.ZipFile(path) as z:
        for filename in z.namelist():
            if filename.endswith('.shader'):
                text=z.read(filename).decode('latin1')
                names.update(re.findall(r'(?:^|\n)\s*([^\s{}]+)\s*\{',text))
def fnv(s):
    h=2166136261
    for c in s: h=((h^c)*16777619)&0xffffffff
    return h
def djb(s):
    h=0
    for c in s:h=(h*33+c)&0xffffffff
    return h
hits=collections.defaultdict(list)
for name in sorted(names):
    for form,value in [('exact',name),('lower',name.lower()),('upper',name.upper())]:
        b=value.encode('latin1')
        for algo,h in [('crc',zlib.crc32(b)),('crc_inverse',zlib.crc32(b)^0xffffffff),('fnv',fnv(b)),('djb',djb(b))]:
            if h in hashes:hits[algo+'_'+form].append({'name':name,'hash':'%08x'%h})
(base/'shader_name_hash_probe.json').write_text(json.dumps({'candidate_names':len(names),'hits':hits},indent=2))
print('Candidate shader names',len(names),'hits',{k:len(v) for k,v in hits.items()})
