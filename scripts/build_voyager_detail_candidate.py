"""Rebuild only the selected Voyager detail textures, preserving every other entry."""
from pathlib import Path
import json,zipfile,hashlib,sys,argparse
sys.path.insert(0,str(Path(__file__).resolve().parent))
from build_xbox_patch_pk3 import build_dds, texture_size_for_path
root=Path(__file__).resolve().parents[1]
def main():
 out=root/'build/research/virtual_voyager/xbox0_detail_candidate.pk3'
 if out.exists():raise RuntimeError('Candidate already exists')
 manifest=json.loads((root/'build/research/virtual_voyager/xbox0.pk3.manifest.json').read_text())
 changes={};report=[]
 for t in manifest['textures']:
  n=t['path']
  if not n.startswith('textures/voyager/') or not any(x in Path(n).stem for x in ['door','floor','wall']):continue
  if max(t['sourceWidth'],t['sourceHeight'])<=128:continue
  source=root/'build/release/BaseEF'/t['source']
  data,info=build_dds(source,256,alpha_format='dxt5',generate_mipmaps=t['mipCount']>1)
  assert info['format']==t['format']
  changes[n]=data;report.append({'path':n,'before':t,'after':info})
 with zipfile.ZipFile(root/'build/release/BaseEF/xbox0.pk3') as src,zipfile.ZipFile(out,'w',compression=zipfile.ZIP_STORED) as dst:
  assert len(src.namelist())==len(set(src.namelist()))
  for entry in src.infolist():dst.writestr(entry,changes.get(entry.filename,src.read(entry)))
 with zipfile.ZipFile(root/'build/release/BaseEF/xbox0.pk3') as src,zipfile.ZipFile(out) as dst:
  assert src.namelist()==dst.namelist()
  for n in src.namelist():assert dst.read(n)==changes.get(n,src.read(n)),n
 result={'changed':len(changes),'extra_payload_bytes':sum(x['after']['bytes']-x['before']['bytes'] for x in report),'unchanged_entries_verified':True,'textures':report,'sha256':hashlib.sha256(out.read_bytes()).hexdigest()}
 out.with_suffix('.audit.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
 print(json.dumps({k:v for k,v in result.items() if k!='textures'},indent=2))
if __name__=='__main__':main()
