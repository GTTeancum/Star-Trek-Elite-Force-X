"""Compile real request/cycle and pmove switch helpers; reproduce lost edge requests."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
def function(source,name):
 start=source.rfind('static ',0,source.index(name))
 assert start>=0
 brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
game=(root/'SP-Mod-Source-Code-master/game/g_main.cpp').read_text()
pmove=(root/'SP-Mod-Source-Code-master/game/bg_pmove.cpp').read_text()
# Find exact definitions rather than forward declarations.
start=game.index('static qboolean STEFX_SplitCoopWeaponSelectable(')
game=game[start:]
helpers='\n'.join(function(game,n) for n in ['STEFX_SplitCoopWeaponSelectable(', 'STEFX_SplitCoopChooseUsableWeapon(', 'STEFX_SplitCoopCycleWeapon(', 'STEFX_SplitCoopWeaponRequest('])
helpers+='\n'+function(pmove,'PM_BeginWeaponChange( int')+'\n'+function(pmove,'PM_FinishWeaponChange( void')
prefix=r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
typedef int qboolean;
#define qtrue 1
#define qfalse 0
#define WP_NONE 0
#define WP_PHASER 1
#define WP_COMPRESSION_RIFLE 2
#define WP_BLUE_HYPO 4
#define WP_RED_HYPO 5
#define WP_NUM_WEAPONS 6
#define FIRST_WEAPON 1
#define MAX_PLAYER_WEAPONS 3
#define STAT_WEAPONS 0
#define WEAPON_DROPPING 1
#define WEAPON_RAISING 2
#define EV_CHANGE_WEAPON 7
#define SETANIM_TORSO 0
#define TORSO_DROPWEAP1 0
#define TORSO_RAISEWEAP1 0
#define SETANIM_FLAG_NORMAL 0
#define PW_DISINT_6 0
struct ps_t {int weapon,weaponstate,weaponTime,stats[1],powerups[1];};
struct gclient_t { ps_t ps; struct {int animFileIndex;}clientInfo;};
struct gentity_t {gclient_t*client;int fx_time;struct {int powerups;}s;};
gentity_t g_entities[1];
struct {int time;}cg,level;
struct pmove_t {ps_t*ps;struct {int weapon;}cmd;gentity_t*gent;} movement,*pm=&movement;
void PM_AddEvent(int){}
void PM_SetAnim(pmove_t*,int,int,int){}
int PM_AnimLength(int,int){return 0;}
'''
suffix=r'''
int main(){
 gclient_t c={0};gentity_t p={0};p.client=&c;g_entities[0].client=&c;
 c.ps.weapon=2;c.ps.stats[0]=(1<<1)|(1<<2)|(1<<3);pm->ps=&c.ps;
 // Actual pmove completion reads the current command, not the earlier edge.
 pm->cmd.weapon=STEFX_SplitCoopCycleWeapon(&p,2,-1);
 PM_BeginWeaponChange(pm->cmd.weapon);assert(c.ps.weaponTime==200);
 pm->cmd.weapon=STEFX_SplitCoopCycleWeapon(&p,c.ps.weapon,0);
 PM_FinishWeaponChange();assert(c.ps.weapon==2); // Reproduced old failure.
 c.ps.weaponTime=0;c.ps.weaponstate=0;
 STEFX_SplitCoopWeaponRequest(NULL,0,0);
 pm->cmd.weapon=STEFX_SplitCoopWeaponRequest(&p,-1,1000);
 assert(pm->cmd.weapon==1);PM_BeginWeaponChange(pm->cmd.weapon);
 for(int t=1016;t<=1208;t+=16)assert(STEFX_SplitCoopWeaponRequest(&p,0,t)==1);
 pm->cmd.weapon=STEFX_SplitCoopWeaponRequest(&p,0,1210);
 PM_FinishWeaponChange();assert(c.ps.weapon==1&&c.ps.weaponstate==WEAPON_RAISING);
 assert(STEFX_SplitCoopWeaponRequest(&p,0,1300)==1);
 // Rapid next presses advance the requested selection during the animation.
 assert(STEFX_SplitCoopWeaponRequest(&p,1,1310)==2);
 assert(STEFX_SplitCoopWeaponRequest(&p,1,1320)==3);
 assert(STEFX_SplitCoopWeaponRequest(&p,0,1330)==3);
 c.ps.weapon=2;assert(STEFX_SplitCoopWeaponRequest(&p,0,1340)==2); // script
 assert(STEFX_SplitCoopWeaponRequest(&p,-1,1350)==1);
 c.ps.stats[0]&=~(1<<1);assert(STEFX_SplitCoopWeaponRequest(&p,0,1360)==2);
 c.ps.stats[0]|=1<<1;assert(STEFX_SplitCoopWeaponRequest(&p,-1,1370)==1);
 assert(STEFX_SplitCoopWeaponRequest(&p,0,100)==2); // time reset
 assert(STEFX_SplitCoopWeaponRequest(&p,-1,110)==1);
 gclient_t replacement=c;p.client=&replacement;
 assert(STEFX_SplitCoopWeaponRequest(&p,0,120)==2);
 return 0;
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_weapon_request_test.cpp';exe=cpp.with_suffix('.exe')
cpp.write_text(prefix+helpers+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
print('Actual co-op cycle/request and pmove begin/finish: old lost switch reproduced; retained switch, repeated input, script changes, inventory loss, player/time reset passed')
