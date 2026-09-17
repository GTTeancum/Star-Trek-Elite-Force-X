"""Compile actual P2 diagnostic input; prove campaign locks and opt-in behavior."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
text=(root/'code/client/cl_input.cpp').read_text()
start=text.index('static qboolean STEFX_SplitScreen_BuildTestP2Usercmd')
end=text.index('\n}',start)+2
helper=text[start:end]
prefix=r'''#include <string.h>
#include <math.h>
#include <assert.h>
#define STEFX_ELITE_FORCE_SP 1
#define PITCH 0
#define YAW 1
#define ROLL 2
#define BUTTON_ATTACK 1
#define qtrue 1
#define qfalse 0
#define ANGLE2SHORT(x) ((int)((x)*65536.0f/360.0f)&65535)
#define VectorCopy(a,b) memcpy(b,a,sizeof(float)*3)
void XBLF(const char*,...){}
typedef int qboolean;
typedef float vec3_t[3];
struct usercmd_t {int serverTime,angles[3],forwardmove,rightmove,upmove,buttons;};
bool in_camera=false;
qboolean player_locked=0;
int mode=0;
int Cvar_VariableIntegerValue(const char*){return mode;}
float AngleNormalize360(float x){return fmodf(x+360.0f,360.0f);}
'''
suffix=r'''
int main(){
 usercmd_t cmd; memset(&cmd,0x55,sizeof(cmd)); usercmd_t original=cmd;
 vec3_t angles={10,20,30},out={0,0,0}; int delta[3]={1,2,3},port=42,weapon=9;
 assert(!STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,100,&port,&weapon,out));
 assert(!memcmp(&cmd,&original,sizeof(cmd)));
 mode=1;in_camera=true;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,200,&port,&weapon,out));
 assert(!cmd.forwardmove&&!cmd.rightmove&&!cmd.upmove&&!cmd.buttons&&cmd.serverTime==200);
 for(int i=0;i<3;++i)assert(cmd.angles[i]==ANGLE2SHORT(angles[i])-delta[i]&&out[i]==angles[i]);
 assert(port==-2&&weapon==0);
 in_camera=false;player_locked=1;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,300,&port,&weapon,out));
 assert(!cmd.forwardmove&&!cmd.buttons);
 player_locked=0;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,100000,&port,&weapon,out));
 assert(cmd.forwardmove==72&&out[YAW]>20&&out[YAW]<21);
 in_camera=true;angles[YAW]=130;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,NULL,100001,NULL,NULL,NULL));
 assert(cmd.angles[YAW]==ANGLE2SHORT(130)&&!cmd.forwardmove);
 in_camera=false;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,NULL,100010,NULL,NULL,out));
 assert(cmd.forwardmove==72&&out[YAW]>130&&out[YAW]<131);
 // Existing movement modes never manufacture fire. Combat mode is explicit.
 for(mode=1;mode<=3;++mode){
  for(int phase=0;phase<4;++phase){
   int time=112000+phase*700;
   assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,time,&port,&weapon,out));
   assert(cmd.forwardmove==72);
   assert(cmd.buttons==((mode==3&&phase!=3)?BUTTON_ATTACK:0));
  }
 }
 mode=3;in_camera=true;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,120000,&port,&weapon,out));
 assert(!cmd.buttons&&!cmd.forwardmove&&!weapon);
 in_camera=false;player_locked=1;
 assert(STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,120700,&port,&weapon,out));
 assert(!cmd.buttons&&!cmd.forwardmove&&!weapon);
 player_locked=0;mode=0;memset(&cmd,0x55,sizeof(cmd));original=cmd;
 assert(!STEFX_SplitScreen_BuildTestP2Usercmd(&cmd,angles,delta,121000,&port,&weapon,out));
 assert(!memcmp(&cmd,&original,sizeof(cmd)));
 return 0;
}
'''
out=root/'build/research/letterbox_perf'; cpp=out/'coop_test_input.cpp'; exe=out/'coop_test_input.exe'; cpp.write_text(prefix+helper+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_test_input.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
print('P2 diagnostic input: opt-in, camera lock, player lock, neutral input and angle resumption passed')
