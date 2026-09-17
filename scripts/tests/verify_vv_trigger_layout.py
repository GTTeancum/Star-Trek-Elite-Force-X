"""Compile actual EF SP layouts/enums with Xbox5558; reject the wrong game ABI."""
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/research/letterbox_perf'
source=out/'vv_trigger_layout.cpp'
source.write_text('#include <stddef.h>\n#include "g_local.h"\nextern "C" unsigned int stefxTriggerLayout[] = {offsetof(gentity_t,inuse),offsetof(gentity_t,svFlags),offsetof(gentity_t,script_targetname),offsetof(gentity_t,behaviorSet),offsetof(gentity_t,spawnflags),offsetof(gentity_t,movedir),offsetof(gentity_t,nextthink),offsetof(gentity_t,e_ThinkFunc),offsetof(gentity_t,e_TouchFunc),offsetof(gentity_t,e_UseFunc),offsetof(gentity_t,wait),offsetof(gentity_t,delay),offsetof(gentity_t,absmin),offsetof(gentity_t,absmax),offsetof(gentity_t,contents),BSET_USE,sizeof(gentity_t)};\n')
args=['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/c','/FA','/EHsc',
      *['/D'+d for d in ['_XBOX','_LIB','WIN32','GAME_HARD_LINKED','_X86_','NDEBUG','STEFX_ELITE_FORCE_SP']],
      '/I'+str(root/'SP-Mod-Source-Code-master/game'),'/I'+str(root/'code/win32'),
      '/IC:/XDK_5558/XDK/xbox/include',str(source),'/Fo'+str(source.with_suffix('.obj')),
      '/Fa'+str(source.with_suffix('.asm'))]
with (out/'vv_trigger_layout_compile.log').open('w') as log:
    subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True)
table=source.with_suffix('.asm').read_text().split('_stefxTriggerLayout DD',1)[1].split('_DATA',1)[0]
values=[int(v,16) for v in re.findall(r'\b([0-9a-f]+)H\b',table,re.I)]
import json
result=dict(zip(['inuse', 'svFlags', 'script_targetname', 'behaviorSet', 'spawnflags', 'movedir', 'nextthink', 'e_ThinkFunc', 'e_TouchFunc', 'e_UseFunc', 'wait', 'delay', 'absmin', 'absmax', 'contents', 'BSET_USE', 'size'],values))
(out/'vv_trigger_layout.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))

assert result == {'inuse':236,'svFlags':244,'script_targetname':816,'behaviorSet':744,'spawnflags':336,'movedir':428,'nextthink':504,'e_ThinkFunc':508,'e_TouchFunc':524,'e_UseFunc':528,'wait':604,'delay':612,'absmin':280,'absmax':292,'contents':276,'BSET_USE':3,'size':1056}, result
