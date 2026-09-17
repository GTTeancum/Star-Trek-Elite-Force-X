from pathlib import Path
import json,struct,zipfile,hashlib,re
base=Path('build/research/ps2_re')
index=json.loads((base/'cd_pmp_uncompressed_index.json').read_text())
cd=next(r for r in json.loads((base/'iso_inventory.json').read_text()) if r['path']=='/BASEEF/CD.PMP;1')
pc={}
for pak in ['PAK0.PK3','PAK1.PK3','PAK2.PK3','PAK3.PK3']:
    with zipfile.ZipFile(Path('build/release/BaseEF')/pak) as z:
        for name in z.namelist():
            if not name.lower().endswith('.bsp'):continue
            with z.open(name) as f:
                h=f.read(144)
                if h[:4]!=b'IBSP':continue
                o,n=struct.unpack_from('<II',h,8)
                f.seek(o);entities=f.read(n)
            pc[name]={'sha':hashlib.sha256(entities).hexdigest(),'prefix':entities[:512].decode('latin1')}
rows=[]
with Path('Star Trek - Voyager - Elite Force (USA PS2).iso').open('rb') as f:
    for entry in index:
        if 'bsp_version' not in entry:continue
        o,n=entry['lumps'][0];f.seek(cd['lba']*2048+entry['offset']+o);e=f.read(n)
        digest=hashlib.sha256(e).hexdigest()
        matches=[name for name,r in pc.items() if r['sha']==digest]
        rows.append({'hash':entry['hash'],'size':entry['size'],'entity_sha256':digest,'exact_entity_matches':matches,'worldspawn_prefix':e[:512].decode('latin1')})
(base/'ps2_bsp_entity_identity.json').write_text(json.dumps(rows,indent=2))
print('Exact entity matches:',sum(bool(r['exact_entity_matches']) for r in rows),'of',len(rows))
for r in rows:
    if any('borg1' in n or 'tour/' in n for n in r['exact_entity_matches']):print(r['hash'],r['size'],r['exact_entity_matches'])
print('First worldspawn:',rows[0]['worldspawn_prefix'])
