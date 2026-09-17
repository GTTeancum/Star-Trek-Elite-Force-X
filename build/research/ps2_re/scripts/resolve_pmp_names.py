from pathlib import Path
import hashlib,struct,json,zipfile
base=Path('build/research/ps2_re')
def pmp_hash(name):
    w=struct.unpack('<4I',hashlib.md5(name.lower().encode('latin1')).digest())
    return ((w[0]*w[1]*w[2]*w[3])^sum(w))&0xffffffff
names=set()
for pak in ['PAK0.PK3','PAK1.PK3','PAK2.PK3','PAK3.PK3']:
    with zipfile.ZipFile(Path('build/release/BaseEF')/pak) as z:names.update(z.namelist())
lookup={}
for name in sorted(names):lookup.setdefault('%08x'%pmp_hash(name),[]).append(name)
cd=json.loads((base/'cd_pmp_uncompressed_index.json').read_text())
for e in cd:e['candidate_names']=lookup.get(e['hash'],[])
(base/'cd_pmp_named_index.json').write_text(json.dumps(cd,indent=2))
borg=json.loads((base/'borg1_lzo_validation.json').read_text())
for e in borg:e['candidate_names']=lookup.get(e['hash'],[])
(base/'borg1_named_index.json').write_text(json.dumps(borg,indent=2))
identity=json.loads((base/'ps2_bsp_entity_identity.json').read_text())
checks=[{'name':n,'expected':r['hash'],'actual':'%08x'%pmp_hash(n)} for r in identity for n in r['exact_entity_matches']]
assert checks and all(c['expected']==c['actual'] for c in checks)
report={'formula':'lowercase Latin-1 name; MD5 digest as four little-endian uint32 words; (product(words) XOR sum(words)) modulo 2^32','evidence':'R5900 0x209a38 product/sum assembly; 0x281a78 lowercase; MD5 identification validated against independently entity-matched BSPs','independent_map_checks':checks,'cd_named':sum(bool(e['candidate_names']) for e in cd),'cd_total':len(cd),'borg_named':sum(bool(e['candidate_names']) for e in borg),'borg_total':len(borg),'limits':'Names are candidates when collisions occur; direct MD5 implementation analysis still pending.'}
(base/'pmp_hash_validation.json').write_text(json.dumps(report,indent=2))
print('Independent map checks',len(checks),'all pass; named CD',report['cd_named'],'/',len(cd),'BORG1',report['borg_named'],'/',len(borg))
