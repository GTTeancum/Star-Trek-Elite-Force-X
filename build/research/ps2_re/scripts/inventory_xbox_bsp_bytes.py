from pathlib import Path
import zipfile, struct, json
root = Path.cwd()
rows=[]
for path in sorted((root/'build/release/BaseEF').glob('*.PK3')) + [root/'build/release/BaseEF/xbox0.pk3']:
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            if not entry.filename.lower().endswith('.bsp'):
                continue
            with archive.open(entry) as stream:
                header=stream.read(144)
            if len(header)<144 or header[:4]!=b'IBSP':
                continue
            lumps=[struct.unpack_from('<ii', header,8+8*i) for i in range(17)]
            rows.append(dict(package=str(path.relative_to(root)),name=entry.filename,raw_bytes=entry.file_size,stored_bytes=entry.compress_size,version=struct.unpack_from('<i',header,4)[0],lump_bytes=[n for o,n in lumps],bounds_valid=all(o>=0 and n>=0 and o+n<=entry.file_size for o,n in lumps)))
output=root/'build/research/ps2_re/xbox_bsp_byte_inventory.json'
output.write_text(json.dumps(rows,indent=2))
print('Inventoried',len(rows),'BSPs; all bounds valid:',bool(rows) and all(r['bounds_valid'] for r in rows))
for r in rows:
    if 'tour/' in r['name'].lower() or 'borg1' in r['name'].lower():
        print(r['name'], r['raw_bytes'], 'bytes')
