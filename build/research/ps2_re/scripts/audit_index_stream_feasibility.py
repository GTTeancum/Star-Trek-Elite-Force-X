"""Read-only index stream feasibility: exact packed ranges, no geometry changes."""
from pathlib import Path
import zipfile,struct,json
base=Path('build/research/ps2_re')
rows=[]
with zipfile.ZipFile('build/release/BaseEF/xbox0.pk3') as z:
    maps=sorted({n.rsplit('/',1)[0] for n in z.namelist() if n.startswith('maps/') and n.endswith('/misc.mle')})
    for name in maps:
        length=z.getinfo(name+'/indexes.mle').file_size
        phases=[]
        for kind,width in [('trisurfs',19),('faces',29)]:
            data=z.read(name+'/'+kind+'.mle')
            assert len(data)%width==0,(name,kind,len(data))
            end=0; backward=0; maximum=0; total=0
            for offset in range(0,len(data),width):
                # packed code(4), shader(2), fog(1), verts(4), indexes(4)
                packed=struct.unpack_from('<I',data,offset+11)[0]
                start=(packed>>12)*2; count=(packed&4095)*2
                assert start+count<=length,(name,kind,start,count,length)
                backward+=start<end
                end=start+count; maximum=max(maximum,count); total+=count
            phases.append(dict(kind=kind,surfaces=len(data)//width,max_surface_index_bytes=maximum,referenced_bytes=total,backward_reads=backward))
        scratch=max(p['max_surface_index_bytes'] for p in phases)
        rows.append(dict(map=name,index_lump_bytes=length,phases=phases,max_surface_scratch_bytes=scratch,theoretical_input_buffer_reduction_bytes=length-scratch))
(base/'packed_index_stream_feasibility.json').write_text(json.dumps(rows,indent=2))
print('Validated index ranges for',len(rows),'maps')
for r in rows:
    if '/tour/' in r['map']:
        print(r['map'],'whole',r['index_lump_bytes'],'scratch',r['max_surface_scratch_bytes'],'backwards',sum(p['backward_reads'] for p in r['phases']))
