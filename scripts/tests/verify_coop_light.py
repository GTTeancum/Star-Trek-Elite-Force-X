"""Compile actual co-op light setup with a recording device; no game execution."""
from pathlib import Path
import subprocess

root=Path.cwd();out=root/'build/research/letterbox_perf'
source=(root/'code/win32/win_qgl_dx8.cpp').read_text()
original='static void dllLightfv('+source.split('static void dllLightfv(',1)[1].split('#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)\n// Co-op only.',1)[0]
helper=source.split('// STEFX_COOP_DIFFUSE_LIGHT_BEGIN',1)[1].split('// STEFX_COOP_DIFFUSE_LIGHT_END',1)[0]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define STEFX_HW_FRAME_DIAGNOSTICS 1
typedef unsigned int DWORD;typedef int BOOL;typedef long HRESULT;
typedef unsigned int GLenum;typedef float GLfloat;
#define TRUE 1
#define FALSE 0
#define SUCCEEDED(x) ((x)>=0)
#define Q_stricmp _stricmp
enum {GL_AMBIENT,GL_DIFFUSE,GL_SPECULAR,GL_POSITION,GL_SPOT_DIRECTION};
struct Color {float r,g,b,a;};struct Vec {float x,y,z;};
struct D3DLIGHT8 {DWORD Type;Color Diffuse,Specular,Ambient;Vec Position,Direction;float Range,Falloff,Attenuation0,Attenuation1,Attenuation2,Theta,Phi;};
struct trRefEntity_t {float ambientLight[3],directedLight[3],lightDir[3];};
struct FakeDevice {
 D3DLIGHT8 light[2];BOOL enabled[2];unsigned int sets,enables,gets;bool corruptSecondGet,failNextSet;
 HRESULT SetLight(DWORD n,const D3DLIGHT8*p){++sets;if(failNextSet){failNextSet=false;return -1;}light[n]=*p;return 0;}
 HRESULT LightEnable(DWORD n,BOOL b){++enables;enabled[n]=b;return 0;}
 HRESULT GetLight(DWORD n,D3DLIGHT8*p){*p=light[n];if(++gets==2&&corruptSecondGet)p->Type^=1;return 0;}
 HRESULT GetLightEnable(DWORD n,BOOL*b){*b=enabled[n];return 0;}
}device;
struct Glw {D3DLIGHT8 dirLight[2];FakeDevice*device;} glw,*glw_state=&glw;
struct Backend {struct View{int stefxSplitView;}viewParms;bool projection2D;} backEnd;
struct cvar_t {int integer;const char*string;};
cvar_t testMode={0,"0"},testSplit={1,"1"},testPlayers={2,"2"},testGame={0,"coop"};
cvar_t*Cvar_Get(const char*n,const char*defaultValue,int){
 if(!strcmp(n,"r_efCoopLight")){assert(!strcmp(defaultValue,"0"));return &testMode;}
 if(!strcmp(n,"stefx_splitScreen"))return &testSplit;
 if(!strcmp(n,"stefx_splitScreenPlayers"))return &testPlayers;
 assert(!strcmp(n,"stefx_splitScreenMode"));return &testGame;
}
float VectorLengthSquared(const float*v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
unsigned int g_SPXBCoopLight[16];bool s_coopLightFailed;
bool g_SPXBPerfSampleActive=true;
unsigned __int64 STEFX_XboxReadTsc(){return 10;}
unsigned int STEFX_XboxElapsedCycles(unsigned __int64){return 20;}
void XBLog_WriteCriticalf(const char*,...){}
'''
test=r'''
static unsigned int seed=7654321;
unsigned int Random(){seed=seed*1664525u+1013904223u;return seed;}
float Value(){return ((int)(Random()%20001)-10000)*0.0125f;}
void Reset(const D3DLIGHT8&before){
 memset(&device,0,sizeof(device));device.light[0]=before;device.enabled[1]=TRUE;
 glw.dirLight[0]=before;glw.device=&device;glw_state=&glw;
 memset(g_SPXBCoopLight,0,sizeof(g_SPXBCoopLight));s_coopLightFailed=false;
 backEnd.viewParms.stefxSplitView=1;backEnd.projection2D=false;
 testSplit.integer=1;testPlayers.integer=2;testGame.string="coop";
}
void Reference(trRefEntity_t*e){
 dllLightfv(0,GL_AMBIENT,e->ambientLight);dllLightfv(0,GL_DIFFUSE,e->directedLight);
 if(VectorLengthSquared(e->lightDir)<=0.0001f){e->lightDir[0]=0;e->lightDir[1]=1;e->lightDir[2]=0;}
 dllLightfv(0,GL_SPOT_DIRECTION,e->lightDir);
}
int main(){
 D3DLIGHT8 before;memset(&before,0,sizeof(before));before.Type=3;before.Direction.x=1;
 trRefEntity_t e;memset(&e,0,sizeof(e));Reset(before);testMode.integer=0;
 assert(!STEFX_CoopDiffuseLight(&e)&&!device.sets);
 testMode.integer=1;testSplit.integer=0;assert(!STEFX_CoopDiffuseLight(&e));
 testSplit.integer=1;testPlayers.integer=1;assert(!STEFX_CoopDiffuseLight(&e));
 testPlayers.integer=2;testGame.string="holomatch";assert(!STEFX_CoopDiffuseLight(&e));
 testGame.string="coop";backEnd.viewParms.stefxSplitView=0;assert(!STEFX_CoopDiffuseLight(&e));
 backEnd.viewParms.stefxSplitView=1;backEnd.projection2D=true;assert(!STEFX_CoopDiffuseLight(&e));
 backEnd.projection2D=false;assert(!STEFX_CoopDiffuseLight(0));
 glw_state=0;assert(!STEFX_CoopDiffuseLight(&e));glw_state=&glw;glw.device=0;assert(!STEFX_CoopDiffuseLight(&e));
 for(int trial=0;trial<30000;++trial){
  float*fields=(float*)&before;for(unsigned int f=1;f<sizeof(before)/4;++f)fields[f]=Value();before.Type=3;
  for(int c=0;c<3;++c){e.ambientLight[c]=Value();e.directedLight[c]=Value();e.lightDir[c]=(trial%7)?Value():0.0f;}
  Reset(before);trRefEntity_t expectedEntity=e;Reference(&expectedEntity);const D3DLIGHT8 expected=device.light[0];
  for(int mode=1;mode<=3;++mode){
   Reset(before);testMode.integer=mode;trRefEntity_t actual=e;assert(STEFX_CoopDiffuseLight(&actual));
   assert(!memcmp(&expected,&device.light[0],sizeof(expected)));
   assert(!memcmp(&expected,&glw.dirLight[0],sizeof(expected)));
   assert(!memcmp(&expectedEntity,&actual,sizeof(actual)));
   assert(device.enabled[0]&&!device.enabled[1]&&!s_coopLightFailed);
   assert(device.sets==(mode==1?1u:mode==2?5u:3u));
   assert(device.enables==(mode==1?2u:mode==2?10u:6u));
   if(mode==2)assert(g_SPXBCoopLight[5]==1&&!g_SPXBCoopLight[6]);
  }
 }
 Reset(before);trRefEntity_t expectedEntity=e;Reference(&expectedEntity);const D3DLIGHT8 expected=device.light[0];
 Reset(before);testMode.integer=2;device.corruptSecondGet=true;assert(STEFX_CoopDiffuseLight(&e));
 assert(s_coopLightFailed&&g_SPXBCoopLight[6]==1&&!memcmp(&expected,&device.light[0],sizeof(expected)));
 unsigned int calls=device.sets;assert(!STEFX_CoopDiffuseLight(&e)&&calls==device.sets);
 Reset(before);testMode.integer=1;device.failNextSet=true;assert(STEFX_CoopDiffuseLight(&e));
 assert(s_coopLightFailed&&g_SPXBCoopLight[7]==1&&!memcmp(&expected,&device.light[0],sizeof(expected)));
 puts("Co-op light: 90000 original/combined/device-state comparisons; guards, zero direction, unchanged fields, verification restoration and API-failure fallback passed.");
}
'''
cpp=out/'coop_light_test.cpp';exe=out/'coop_light_test.exe'
cpp.write_text(prefix+original+'\n#define qglLightfv dllLightfv\n'+helper+test)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/O2','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_light_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
