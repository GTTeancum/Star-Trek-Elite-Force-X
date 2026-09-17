from pathlib import Path
import sys,json,struct,re,collections
base=Path('build/research/ps2_re')
sys.path.insert(0,str(base/'python_deps'))
import lzokay
blob=(base/'BORG1.PMP').read_bytes()
rows=json.loads((base/'borg1_lzo_validation.json').read_text())
unknown=[]
for row in rows:
    if row['magic'][:8] in ('43439384','49445033','49424900','52444d35') or row['size']<8192:
        continue
    data=lzokay.decompress(blob[row['offset']:row['offset']+row['stored']],row['size'])
    strings=[s.decode('ascii') for s in re.findall(rb'[ -~]{8,}',data)][:20]
    unknown.append({'index':row['index'],'hash':row['hash'],'raw_bytes':len(data),'prefix':data[:64].hex(),'ascii_samples':strings})
(base/'borg1_unclassified_large_entries.json').write_text(json.dumps(unknown,indent=2))
print('Large unclassified entries:',len(unknown))
for row in sorted(unknown,key=lambda r:r['raw_bytes'],reverse=True)[:8]:
    print(row['index'],row['raw_bytes'],row['ascii_samples'][:3])
