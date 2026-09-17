from pathlib import Path
import struct,json
base=Path('build/research/ps2_re')
row=next(r for r in json.loads((base/'iso_inventory.json').read_text()) if r['path']=='/BASEEF/CD.PMP;1')
entries=[]
with Path('Star Trek - Voyager - Elite Force (USA PS2).iso').open('rb') as f:
    start=row['lba']*2048;f.seek(start);count=struct.unpack('<I',f.read(4))[0];table=f.read(count*12)
    for i in range(count):
        h,o,size=struct.unpack_from('<3I',table,i*12)
        assert o>=4+count*12 and o+size<=row['bytes'],(i,h,o,size)
        f.seek(start+o);prefix=f.read(min(size,144))
        entry={'index':i,'hash':'%08x'%h,'offset':o,'size':size,'magic':prefix[:16].hex()}
        if len(prefix)>=144 and prefix[:4] in (b'IBSP',b'RBSP'):
            entry['bsp_version']=struct.unpack_from('<I',prefix,4)[0]
            entry['lumps']=[struct.unpack_from('<2I',prefix,8+j*8) for j in range(17)]
            entry['lump_bounds_valid']=all(o+n<=size for o,n in entry['lumps'])
        entries.append(entry)
(base/'cd_pmp_uncompressed_index.json').write_text(json.dumps(entries,indent=2))
print('Valid 12-byte records',len(entries),'BSPs',sum('bsp_version'in e for e in entries))
print(entries[:3])
