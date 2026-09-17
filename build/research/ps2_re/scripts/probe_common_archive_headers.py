from pathlib import Path
import json,struct
base=Path('build/research/ps2_re')
rows=json.loads((base/'iso_inventory.json').read_text())
iso=Path('Star Trek - Voyager - Elite Force (USA PS2).iso')
results=[]
with iso.open('rb') as f:
    for name in ['CD.PMP','BIG.PMP','PMODELS.PMP']:
        matches=[r for r in rows if r['path'].split('/')[-1].split(';')[0]==name]
        for row in matches:
            start=row['lba']*2048
            f.seek(start);header=f.read(16)
            count=struct.unpack_from('<I',header)[0]
            result={'path':row['path'],'bytes':row['bytes'],'lba':row['lba'],'header':header.hex(),'count':count}
            if 4+count*16<=row['bytes']:
                f.seek(start+4);table=f.read(count*16)
                entries=[dict(zip(['hash','offset','stored','raw'],struct.unpack_from('<4I',table,i*16))) for i in range(count)]
                result['all_bounds_valid']=all(e['offset']>=4+count*16 and e['offset']+e['stored']<=row['bytes'] for e in entries)
                result['largest_raw']=sorted(entries,key=lambda e:e['raw'],reverse=True)[:8]
                (base/(name.lower()+'.index.json')).write_text(json.dumps(entries,indent=2))
            results.append(result)
(base/'common_archive_headers.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
