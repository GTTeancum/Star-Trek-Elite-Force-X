from pathlib import Path
import zipfile, json, collections
base=Path('build/research/ps2_re')
with zipfile.ZipFile('build/release/BaseEF/xbox0.pk3') as z:
    maps=collections.defaultdict(dict)
    for entry in z.infolist():
        if entry.filename.startswith('maps/') and entry.filename.endswith('.mle'):
            directory, leaf=entry.filename.rsplit('/',1)
            maps[directory][leaf]={'raw_bytes':entry.file_size,'stored_bytes':entry.compress_size}
    rows=[{'map':name,'lumps':lumps,'total_lump_bytes':sum(v['raw_bytes'] for v in lumps.values())} for name,lumps in sorted(maps.items())]
(base/'xbox_packed_lump_inventory.json').write_text(json.dumps(rows,indent=2))
for row in rows:
    if '/tour/' in row['map'] or row['map']=='maps/borg1':
        print(row['map'],row['total_lump_bytes'], 'indexes', row['lumps'].get('indexes.mle'), 'verts',row['lumps'].get('verts.mle'))
print('Map count:',len(rows))
