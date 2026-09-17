from pathlib import Path
import json,struct,zipfile,hashlib
base=Path('build/research/ps2_re')
cd=next(r for r in json.loads((base/'iso_inventory.json').read_text()) if r['path']=='/BASEEF/CD.PMP;1')
entry=next(e for e in json.loads((base/'cd_pmp_named_index.json').read_text()) if 'maps/borg1.bsp' in e['candidate_names'])
with Path('Star Trek - Voyager - Elite Force (USA PS2).iso').open('rb') as f:
    f.seek(cd['lba']*2048+entry['offset']);ps2=f.read(entry['size'])
assert len(ps2)==entry['size']
with zipfile.ZipFile('build/release/BaseEF/PAK0.PK3') as z:pc=z.read('maps/borg1.bsp')
folder=base/'map_comparison';folder.mkdir(exist_ok=True)
(folder/'borg1_ps2.bsp').write_bytes(ps2)
(folder/'borg1_pc.bsp').write_bytes(pc)
names=['entities','shaders','planes','nodes','leafs','leafsurfaces','leafbrushes','models','brushes','brushsides','drawverts','drawindexes','fogs','surfaces','lightmaps','lightgrid','visibility']
rows=[]
for i,name in enumerate(names):
    a,n=struct.unpack_from('<II',ps2,8+8*i);b,m=struct.unpack_from('<II',pc,8+8*i)
    left=ps2[a:a+n];right=pc[b:b+m]
    rows.append({'lump':name,'ps2_bytes':n,'pc_bytes':m,'byte_identical':left==right,'ps2_sha256':hashlib.sha256(left).hexdigest(),'pc_sha256':hashlib.sha256(right).hexdigest()})
report={'name':'maps/borg1.bsp','ps2_bytes':len(ps2),'pc_bytes':len(pc),'identity':'PMP filename hash and independently identical entity lump','lumps':rows}
(folder/'borg1_comparison.json').write_text(json.dumps(report,indent=2))
for r in rows:print(r['lump'],r['ps2_bytes'],r['pc_bytes'],'same',r['byte_identical'])
