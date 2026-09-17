from pathlib import Path
import struct,json,collections
base=Path('build/research/ps2_re')
data=(base/'borg1_entries/75be6d3d.bin').read_bytes()
count=struct.unpack_from('<I',data)[0]
records=[struct.unpack_from('<II',data,4+8*i) for i in range(count)]
# Infer token/string boundary from the minimum string pointer, then validate every record.
first=records[0][1]
string_start=min(x for x in struct.unpack_from('<28I',data,first) if x)
# First record need not reference the first string. Walk candidate token words to find it.
position=first
while position<string_start:
    ptr=struct.unpack_from('<I',data,position)[0]
    if ptr: string_start=min(string_start,ptr)
    position+=4
assert position==string_start
outputs=[]; uses=collections.Counter(); expanded=0
for i,(h,offset) in enumerate(records):
    end=records[i+1][1] if i+1<count else string_start
    assert (end-offset)%4==0
    tokens=[]
    for pos in range(offset,end,4):
        ptr=struct.unpack_from('<I',data,pos)[0]
        if ptr==0:
            tokens.append(None);continue
        assert string_start<=ptr<len(data)
        stop=data.index(b'\0',ptr)
        token=data[ptr:stop].decode('ascii')
        tokens.append(token);uses[ptr]+=1;expanded+=stop-ptr+1
    outputs.append(dict(hash='%08x'%h,offset=offset,tokens=tokens))
report={'classification':'Hash-indexed pretokenized shader definitions; syntax resolved from string offsets', 'record_count':count,'total_bytes':len(data),'table_bytes':first,'token_pointer_bytes':string_start-first,'string_pool_bytes':len(data)-string_start,'string_pool_offset':string_start,'referenced_unique_strings':len(uses),'token_occurrences':sum(uses.values()),'expanded_token_string_bytes':expanded,'first_definition':outputs[0],'limits':'Runtime ownership/lifetime and lookup hash still require executable confirmation. Expanded token bytes are not a comparison with Xbox live memory.'}
(base/'ps2_shader_token_table.json').write_text(json.dumps(report,indent=2))
(base/'ps2_shader_tokens.json').write_text(json.dumps(outputs,indent=2))
print(json.dumps(report,indent=2))
