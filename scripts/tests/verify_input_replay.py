"""Compile the actual input replay with fake clock/input; no emulator or OS input."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/research/letterbox_perf'
header = (root / 'code/client/stefx_input_replay.h').read_text()
source = r'''
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CA_ACTIVE 7
#define BUTTON_ATTACK 1
#define BUTTON_USE 32
#define BUTTON_ALT_ATTACK 128
#define PITCH 0
#define YAW 1
#define STAT_HEALTH 0
#define XBL(x) ((void)0)
void XBLF(const char*,...){}
#define VectorCopy(a,b) memcpy(b,a,sizeof(float)*3)
typedef float vec3_t[3];
struct usercmd_t {int forwardmove,rightmove,upmove,buttons;};
struct {int state;} cls;
struct {int serverTime;vec3_t viewangles;struct {struct {vec3_t origin;int stats[1];} ps;} frame;} cl;
bool in_camera=false;
const char *fixture;
const char *currentMap="stasis1";
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
const char *Cvar_VariableString(const char*){return currentMap;}
FILE *replay_open(const char*,const char*){if(!fixture)return NULL;FILE*f=tmpfile();fputs(fixture,f);rewind(f);return f;}
#define fopen replay_open
''' + header + r'''
#undef fopen
int main(int argc,char**argv){
 int mode=argc>1?argv[1][0]-'0':0;
 fixture="STEFX_INPUT_REPLAY_V1 stasis1\n100 200 127 0 0 20 90 1\n300 400 0 0 0 0 0 32\n";
 if(mode==1)fixture=NULL;
 if(mode==2)fixture="STEFX_INPUT_REPLAY_V1 stasis1\n100 200 999 0 0 0 0 1\n";
 if(mode==3)currentMap="borg1";
 cls.state=CA_ACTIVE;cl.serverTime=1000;vec3_t angles={0,0,0};
 usercmd_t cmd={7,8,9,128};STEFX_ApplyInputReplay(&cmd,angles);
 if(mode>=1)return cmd.forwardmove!=7||cmd.buttons!=128;
 if(cmd.forwardmove||cmd.buttons)return 10;
 cl.serverTime=1100;STEFX_ApplyInputReplay(&cmd,angles);
 if(cmd.forwardmove!=127||cmd.buttons!=1||fabs(cl.viewangles[0]-2)>0.001||fabs(cl.viewangles[1]-9)>0.001)return 11;
 in_camera=true;cl.serverTime=1150;cmd.forwardmove=7;STEFX_ApplyInputReplay(&cmd,angles);
 if(cmd.forwardmove!=7)return 12;
 in_camera=false;cl.serverTime=1200;STEFX_ApplyInputReplay(&cmd,angles);
 if(cmd.forwardmove||cmd.buttons)return 13;
 cl.serverTime=1300;STEFX_ApplyInputReplay(&cmd,angles);if(cmd.buttons!=32)return 14;
 cl.serverTime=1400;cmd.forwardmove=7;cmd.buttons=128;STEFX_ApplyInputReplay(&cmd,angles);
 if(cmd.forwardmove!=7||cmd.buttons!=128)return 15;
 return 0;
}
'''
cpp = out/'input_replay_test.cpp'
exe = out/'input_replay_test.exe'
cpp.write_text(source)
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'input_replay_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
for mode in range(4):
    subprocess.run([str(exe),str(mode)],check=True)
caller = (root/'code/client/cl_input.cpp').read_text()
body = caller[caller.index('usercmd_t CL_CreateCmd'):]
assert body.index('STEFX_ApplyInputReplay(&cmd, oldAngles)') < body.index('if ( cl_overrideAngles )') < body.index('CL_FinishMove( &cmd )')
print('Input replay: timed controls, neutral gaps, camera lock, release, invalid files, authoritative angle ordering passed')
