from pathlib import Path
import zipfile,struct,re,json,collections
root=Path.cwd();sources=root/'build/release/BaseEF';out=root/'build/research/virtual_voyager/pipeline';out.mkdir(exist_ok=True)
entries={}
for i in range(4):
 with zipfile.ZipFile(sources/f'PAK{i}.PK3') as z:
  for n in z.namelist():
   low=n.lower()
   if (low.startswith('maps/tour/') or low.startswith('maps/_holodeck_') or low=='maps/_brig.bsp') and low.endswith('.bsp'):
    data=z.read(n);o,l=struct.unpack_from('<II',data,8);text=data[o:o+l].decode('latin1')
    ents=[dict(re.findall(r'"([^"\n]+)"\s*"([^"\n]*)"',b)) for b in re.findall(r'\{([^{}]*)\}',text)]
    entries[low[5:-4]]=dict(source=f'PAK{i}.PK3:{n}',bytes=len(data),entity_count=len(ents),entities=ents)
report={'maps':entries,'interfaces':{},'travel_classes':{},'ui_data':{}}
for name,item in entries.items():
 for index,e in enumerate(item['entities']):
  c=e.get('classname','')
  if c=='target_interface':report['interfaces'].setdefault(e.get('script_targetname',''),[]).append(dict(map=name,index=index,entity=e))
  if any(k in c for k in ['level','teleport','change','transport']):report['travel_classes'].setdefault(c,[]).append(dict(map=name,index=index,entity=e))
with zipfile.ZipFile(sources/'PAK3.PK3') as z:
 for n in z.namelist():
  if n.lower().startswith('ext_data/') and n.lower().endswith('.dat'):
   text=z.read(n).decode('latin1');target=out/Path(n).name;target.write_text(text,encoding="latin1")
   report['ui_data'][n]=dict(bytes=len(text),commands=re.findall(r'(?im)^\s*COMMAND\s+"?([^"\r\n]+)',text))
(out/'authored_pipeline_inventory.json').write_text(json.dumps(report,indent=2))
print('maps',len(entries),'interfaces', {k:len(v) for k,v in report['interfaces'].items()},'travel_classes',{k:len(v) for k,v in report['travel_classes'].items()})
print('ui data',list(report['ui_data']))
