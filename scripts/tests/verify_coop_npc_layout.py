"""Compile actual EF SP layouts/enums with Xbox5558; reject the wrong game ABI."""
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/research/letterbox_perf'
source=out/'coop_npc_layout.cpp'
source.write_text('#include <stddef.h>\n#include "g_local.h"\nextern "C" unsigned int stefxNpcLayout[] = { offsetof(gentity_t,client),offsetof(gentity_t,inuse),offsetof(gentity_t,currentOrigin),offsetof(gentity_t,classname),offsetof(gentity_t,targetname),offsetof(gentity_t,health),offsetof(gentity_t,NPC),offsetof(gentity_t,enemy),offsetof(gentity_t,NPC_type),offsetof(gentity_t,flags),offsetof(gentity_t,svFlags),offsetof(gentity_t,takedamage),offsetof(gclient_t,playerTeam),offsetof(gclient_t,enemyTeam),offsetof(gNPC_t,behaviorState),offsetof(gNPC_t,defaultBehavior),offsetof(gNPC_t,tempBehavior),offsetof(gNPC_t,enemyLastSeenTime),offsetof(gNPC_t,enemyLastVisibility),offsetof(gNPC_t,aiFlags),offsetof(gNPC_t,scriptFlags),offsetof(gNPC_t,shotTime),offsetof(gNPC_t,nextBStateThink),offsetof(gNPC_t,last_ucmd),offsetof(game_export_t,num_entities),offsetof(level_locals_t,time),offsetof(usercmd_t,buttons),offsetof(usercmd_t,forwardmove),offsetof(usercmd_t,rightmove),sizeof(gentity_t),sizeof(gNPC_t),sizeof(gclient_t),MAX_GENTITIES,TEAM_BORG,BUTTON_ATTACK,BUTTON_ALT_ATTACK };\n')
args=['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/c','/FA','/EHsc',
      *['/D'+d for d in ['_XBOX','_LIB','WIN32','GAME_HARD_LINKED','_X86_','NDEBUG','STEFX_ELITE_FORCE_SP']],
      '/I'+str(root/'SP-Mod-Source-Code-master/game'),'/I'+str(root/'code/win32'),
      '/IC:/XDK_5558/XDK/xbox/include',str(source),'/Fo'+str(source.with_suffix('.obj')),
      '/Fa'+str(source.with_suffix('.asm'))]
with (out/'coop_npc_layout_compile.log').open('w') as log:
    subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True)
table=source.with_suffix('.asm').read_text().split('_stefxNpcLayout DD',1)[1].split('_DATA',1)[0]
values=[int(v,16) for v in re.findall(r'\b([0-9a-f]+)H\b',table,re.I)]
assert len(values)==36,values
result=dict(zip(['gentity_t.client', 'gentity_t.inuse', 'gentity_t.currentOrigin', 'gentity_t.classname', 'gentity_t.targetname', 'gentity_t.health', 'gentity_t.NPC', 'gentity_t.enemy', 'gentity_t.NPC_type', 'gentity_t.flags', 'gentity_t.svFlags', 'gentity_t.takedamage', 'gclient_t.playerTeam', 'gclient_t.enemyTeam', 'gNPC_t.behaviorState', 'gNPC_t.defaultBehavior', 'gNPC_t.tempBehavior', 'gNPC_t.enemyLastSeenTime', 'gNPC_t.enemyLastVisibility', 'gNPC_t.aiFlags', 'gNPC_t.scriptFlags', 'gNPC_t.shotTime', 'gNPC_t.nextBStateThink', 'gNPC_t.last_ucmd', 'game_export_t.num_entities', 'level_locals_t.time', 'usercmd_t.buttons', 'usercmd_t.forwardmove', 'usercmd_t.rightmove', 'entity_size', 'npc_size', 'client_size', 'max_entities', 'team_borg', 'attack', 'alt_attack'],values))
import json
(out/'coop_npc_layout.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))

assert result == {'gentity_t.client': 232, 'gentity_t.inuse': 236, 'gentity_t.currentOrigin': 304, 'gentity_t.classname': 332, 'gentity_t.targetname': 480, 'gentity_t.health': 540, 'gentity_t.NPC': 848, 'gentity_t.enemy': 584, 'gentity_t.NPC_type': 860, 'gentity_t.flags': 340, 'gentity_t.svFlags': 244, 'gentity_t.takedamage': 548, 'gclient_t.playerTeam': 1812, 'gclient_t.enemyTeam': 1816, 'gNPC_t.behaviorState': 128, 'gNPC_t.defaultBehavior': 132, 'gNPC_t.tempBehavior': 136, 'gNPC_t.enemyLastSeenTime': 48, 'gNPC_t.enemyLastVisibility': 8, 'gNPC_t.aiFlags': 72, 'gNPC_t.scriptFlags': 472, 'gNPC_t.shotTime': 80, 'gNPC_t.nextBStateThink': 548, 'gNPC_t.last_ucmd': 552, 'game_export_t.num_entities': 64, 'level_locals_t.time': 12, 'usercmd_t.buttons': 4, 'usercmd_t.forwardmove': 24, 'usercmd_t.rightmove': 25, 'entity_size': 1056, 'npc_size': 612, 'client_size': 3364, 'max_entities': 1024, 'team_borg': 2, 'attack': 1, 'alt_attack': 128}
