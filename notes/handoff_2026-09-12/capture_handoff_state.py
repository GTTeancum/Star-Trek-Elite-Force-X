from pathlib import Path
import json,subprocess,hashlib,struct,datetime
root=Path.cwd();out=root/'notes/handoff_2026-09-12'
for args,name in [(['git','status','--porcelain=v1'],'git_status.txt'),(['git','diff','--binary'],'tracked_changes.patch'),(['git','rev-parse','HEAD'],'git_head.txt'),(['git','branch','--show-current'],'git_branch.txt')]:
 (out/name).write_bytes(subprocess.check_output(args))
paths=['build/research/ps2_re/SLUS_202.27','build/research/ps2_re/BORG1.PMP','build/research/ps2_re/loader_decompilation.json','build/research/ps2_re/ghidra_loader.log','build/research/ps2_re/scripts/ExtractLoader.py','build/research/ps2_re/scripts/run_ghidra_loader.py','build/release/default.xbe','build/release/efmp.xbe','build/research/virtual_voyager/manual_profile.json']
for folder in ['terminal_fix/symbols','terminal_fix/timed_symbols','followup_symbols']:
 for base in ['default','efmp']:
  for ext in ['map','exe','xbe']: paths.append('build/research/virtual_voyager/'+folder+'/'+base+'.'+ext)
manifest=[]
for rel in paths:
 p=root/rel
 if not p.exists():manifest.append({'path':rel,'exists':False});continue
 b=p.read_bytes();manifest.append({'path':rel,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'mtime':datetime.datetime.fromtimestamp(p.stat().st_mtime).isoformat()})
for rel in ['Star Trek - Voyager - Elite Force (USA PS2).iso','build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_followup.iso','build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_terminal_wide.iso','build/research/virtual_voyager/StarTrekEliteForceX_virtual_voyager_pipeline.iso','build/release/BaseEF/xbox0.pk3']:
 p=root/rel;manifest.append({'path':rel,'exists':p.exists(),'bytes':p.stat().st_size if p.exists() else None,'sha256':'Not rehashed at handoff; see existing build manifests where available'})
(out/'artifact_manifest.json').write_text(json.dumps(manifest,indent=2))
state={};folder=root/'build/research/letterbox_perf';files=list(folder.iterdir())
for name in ['vv_astro_wide','vv_astro_menu_wide','vv_astro_authored_sweep']:
 r={}
 for suffix in ['summary.json','iso_transaction.json']:
  p=folder/(name+'.'+suffix)
  if p.exists():
   d=json.loads(p.read_text());r[suffix]={k:d.get(k) for k in (['restored','map','seconds'] if suffix.startswith('iso') else ['loaded_map','finished_in_game','final_liveness_verified','engine_error_message','physical_memory_total_available_bytes'])}
 for key,fmt in [('astrometrics_proof','8I'),('astro_harness_proof','4I'),('view_aspect_proof','12I')]:
  matches=[p for p in files if p.name.startswith(name+'_') and p.name.endswith('_final_'+key+'.bin')]
  if len(matches)==1:r[key]=struct.unpack('<'+fmt,matches[0].read_bytes())
 state[name]=r
p=root/'build/research/ps2_re/loader_decompilation.json'
if p.exists():
 d=json.loads(p.read_text());state['loader_decompilation']={'language':d.get('language'),'functions':len(d.get('functions',[]))}
(out/'observed_state.json').write_text(json.dumps(state,indent=2));print(json.dumps(state,indent=2))
