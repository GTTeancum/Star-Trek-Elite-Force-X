"""Compile actual EF SP layouts/enums with Xbox5558; reject the wrong game ABI."""
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/research/letterbox_perf'
source=out/'coop_combat_layout.cpp'
source.write_text('''#include <stddef.h>
#include "g_local.h"
extern "C" unsigned int stefxCombatLayout[] = { sizeof(gentity_t),offsetof(gentity_t,client),offsetof(gclient_t,ps),offsetof(playerState_t,eventSequence),offsetof(playerState_t,events),MAX_PS_EVENTS,offsetof(playerState_t,weapon),offsetof(playerState_t,weaponTime),offsetof(playerState_t,weaponstate),offsetof(playerState_t,ammo),MAX_AMMO,EV_FIRE_WEAPON,EV_ALT_FIRE };
''')
args=['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/c','/FA','/EHsc',
      *['/D'+d for d in ['_XBOX','_LIB','WIN32','GAME_HARD_LINKED','_X86_','NDEBUG','STEFX_ELITE_FORCE_SP']],
      '/I'+str(root/'SP-Mod-Source-Code-master/game'),'/I'+str(root/'code/win32'),
      '/IC:/XDK_5558/XDK/xbox/include',str(source),'/Fo'+str(source.with_suffix('.obj')),
      '/Fa'+str(source.with_suffix('.asm'))]
with (out/'coop_combat_layout_compile.log').open('w') as log:
    subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True)
table=source.with_suffix('.asm').read_text().split('_stefxCombatLayout DD',1)[1].split('_DATA',1)[0]
values=[int(v,16) for v in re.findall(r'\b([0-9a-f]+)H\b',table,re.I)]
assert values==[1056,232,0,112,116,2,148,44,152,380,4,21,22],values
print('Xbox5558 EF SP entity/weapon/ammo/event ABI verified:',values)
