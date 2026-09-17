"""Execute real trace dispatch/filter against deterministic collision spies.

Exercises the exact optimized and original entity loops, not a copied filter.
Native mode2 subsequently verifies rejected colliders against Xbox CM itself.
"""
import re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/'SP-Mod-Source-Code-master/cgame/cg_predict.cpp').read_text()
# The mask shortcut depends on this exact engine bridge contract.
cm=(root/'code/qcommon/cm_load_xbox.cpp').read_text()
temp=re.search(r'clipHandle_t\s+CM_TempBoxModel\([^)]*\)\s*\{([^}]+)',cm).group(1)
assert 'CM_TempBoxModelContents( mins, maxs, CONTENTS_BODY )' in temp
for path in ['SP-Mod-Source-Code-master/game/surfaceflags.h','code/game/surfaceflags.h']:
    assert re.search(r'#define\s+CONTENTS_BODY\s+0x2000000\b',(root/path).read_text())

body=src[src.index('static\tint\t\t\tcg_numSolidEntities;'):src.index('/*\n================\nCG_PointContents')]
body=re.sub(r'^#include .*\n','',body,flags=re.M)
bounds=(root/'SP-Mod-Source-Code-master/cgame/stefx_fx_trace_bounds.h').read_text()
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
#define MAX_ENTITIES_IN_SNAPSHOT 64
#define ENTITYNUM_WORLD 62
#define ENTITYNUM_NONE 63
#define ET_PUSH_TRIGGER 2
#define ET_TELEPORT_TRIGGER 3
#define SOLID_BMODEL 0xffffff
#define CONTENTS_BODY 2
#define CONTENTS_SOLID 1
#define qtrue 1
typedef float vec3_t[3];typedef int clipHandle_t;
struct trace_t { float fraction;int allsolid,startsolid,entityNum,contents;float normal[3]; };
struct trajectory_t{vec3_t trBase;};
struct entityState_t{int number,eType,solid,modelindex;trajectory_t pos;};
struct Gent{struct{int solid;}s;};
struct centity_t{entityState_t currentState;vec3_t lerpOrigin,lerpAngles;Gent*gent;};
struct Snap{int numEntities,serverTime;entityState_t entities[64];}snap;
struct {Snap*snap;int clientFrame;}cg;
struct vmCvar_t{int integer;char string[32];};
vmCvar_t cg_stefxSplitScreen,cg_stefxSplitScreenPlayers;
centity_t cg_entities[64];Gent gent[64];vec3_t vec3_origin={0,0,0};
void VectorCopy(const float*a,float*b){memcpy(b,a,12);}
void EvaluateTrajectory(const trajectory_t*t,int,float*out){VectorCopy(t->trBase,out);}
int Q_stricmp(const char*a,const char*b){return _stricmp(a,b);}
int requested=0;const char*gameMode="coop";
void cgi_Cvar_Register(vmCvar_t*v,const char*n,const char*d,int){
 if(!strcmp(n,"cg_efCoopFxTrace")){assert(!strcmp(d,"0"));v->integer=requested;strcpy(v->string,"0");}
 else {strcpy(v->string,gameMode);v->integer=0;}
}
void cgi_Cvar_Update(vmCvar_t*){}
unsigned __int64 STEFX_XboxReadTsc(){return 0;}
unsigned int STEFX_XboxElapsedCycles(unsigned __int64){return 1;}
int logs;extern "C" void XBLog_WriteCritical(const char*){++logs;}
vec3_t modelMins[64],modelMaxs[64];int modelContents[64],calls,forceHit=-1;
clipHandle_t cgi_CM_InlineModel(int i){return i;}
clipHandle_t cgi_CM_TempBoxModel(const float*a,const float*b){VectorCopy(a,modelMins[63]);VectorCopy(b,modelMaxs[63]);modelContents[63]=CONTENTS_BODY;return 63;}
void CM_STEFX_CoopModelBounds(int m,float*a,float*b){VectorCopy(modelMins[m],a);VectorCopy(modelMaxs[m],b);}
void cgi_CM_TransformedBoxTrace(trace_t*t,const float*s,const float*e,const float*mn,const float*mx,int m,int mask,const float*o,const float*) {
 ++calls;memset(t,0,sizeof(*t));t->fraction=1;
 if(!(mask&modelContents[m]))return;
 float enter=0,leave=1;bool inside=true,endInside=true;
 for(int i=0;i<3;++i){float lo=o[i]+modelMins[m][i]-(mx?mx[i]:0)-0.125f,hi=o[i]+modelMaxs[m][i]-(mn?mn[i]:0)+0.125f;
  if(s[i]<lo||s[i]>hi)inside=false;if(e[i]<lo||e[i]>hi)endInside=false;
  float d=e[i]-s[i];if(d==0){if(s[i]<lo||s[i]>hi)return;continue;}
  float a=(lo-s[i])/d,b=(hi-s[i])/d;if(a>b){float c=a;a=b;b=c;}
  if(a>enter)enter=a;if(b<leave)leave=b;if(enter>leave||leave<0||enter>1)return;
 }
 t->startsolid=inside;t->allsolid=inside&&endInside;t->fraction=t->allsolid?0:enter;t->contents=modelContents[m];t->normal[0]=1;
}
void cgi_CM_BoxTrace(trace_t*t,const float*s,const float*e,const float*mn,const float*mx,int,int mask){memset(t,0,sizeof(*t));t->fraction=1;}
'''
# Deliberately corrupt one model's bounds only for the verifier failure case.
prefix=prefix.replace('VectorCopy(modelMins[m],a);VectorCopy(modelMaxs[m],b);',
 'VectorCopy(modelMins[m],a);VectorCopy(modelMaxs[m],b);if(m==forceHit){a[0]+=10000;b[0]+=10000;}')
suffix=r'''
float rng(){return (rand()%40000-20000)*0.01f;}
int main(){
 cg.snap=&snap;snap.numEntities=32;snap.serverTime=1;cg_stefxSplitScreen.integer=1;cg_stefxSplitScreenPlayers.integer=2;
 for(int i=0;i<32;++i){centity_t*c=&cg_entities[i];snap.entities[i].number=i;c->gent=&gent[i];gent[i].s.solid=1;
  c->currentState.number=i;c->currentState.modelindex=i+1;c->currentState.solid=i%2?SOLID_BMODEL:16|(24<<8)|(64<<16);
  for(int j=0;j<3;++j){c->lerpOrigin[j]=c->currentState.pos.trBase[j]=rng();modelMins[i+1][j]=-20;modelMaxs[i+1][j]=20;}
  modelContents[i+1]=CONTENTS_SOLID;
 }
 CG_BuildSolidList();long baselineCalls=0,fastCalls=0;
 for(int n=0;n<40000;++n){vec3_t a,b,mn={-5,-8,-9},mx={5,8,9};for(int j=0;j<3;++j){a[j]=rng();b[j]=n%5?a[j]+rng()*0.03f:a[j];}
  const float*mi=n%2?mn:0,*ma=n%2?mx:0;int mask=1+n%3,skip=n%33;
  trace_t ref,fast,verified;memset(&ref,0,sizeof(ref));ref.fraction=n%7?1:0.25f;fast=verified=ref;
  calls=0;CG_ClipMoveToEntitiesInternal(a,mi,ma,b,skip,mask,&ref,0);baselineCalls+=calls;
  calls=0;CG_ClipMoveToEntitiesInternal(a,mi,ma,b,skip,mask,&fast,1);fastCalls+=calls;
  CG_ClipMoveToEntitiesInternal(a,mi,ma,b,skip,mask,&verified,2);
  assert(!memcmp(&ref,&fast,sizeof(ref))&&!memcmp(&ref,&verified,sizeof(ref))&&!s_fxTraceFailed);
 }
 assert(fastCalls<baselineCalls/2&&g_SPXBCoopFxTrace[5]>1000);
 // Rotated brush models must never use the axis-aligned rejection.
 cg_entities[1].lerpAngles[1]=90;cg_numSolidEntities=1;cg_solidEntities[0]=&cg_entities[1];
 vec3_t a={999,999,999},b={1000,1000,1000};trace_t t;memset(&t,0,sizeof(t));t.fraction=1;
 calls=0;CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,CONTENTS_SOLID,&t,1);assert(calls==1);
 // Rebuilding the list invalidates geometry bounds, including reused map handles.
 s_fxTraceBounds[2].valid=true;CG_BuildSolidList();assert(!s_fxTraceBounds[2].valid);
 // Same-frame origin/model changes must refresh cached world bounds.
 cg_numSolidEntities=1;cg_solidEntities[0]=&cg_entities[1];cg_entities[1].lerpAngles[1]=0;
 unsigned int fills=g_SPXBCoopFxTrace[10];
 memset(&t,0,sizeof(t));t.fraction=1;CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,1,&t,1);
 CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,1,&t,1);assert(g_SPXBCoopFxTrace[10]==fills+1);
 VectorCopy(a,cg_entities[1].currentState.pos.trBase);
 memset(&t,0,sizeof(t));t.fraction=1;CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,1,&t,1);
 assert(t.allsolid&&g_SPXBCoopFxTrace[10]==fills+2);
 cg_entities[1].currentState.modelindex=3;
 memset(&t,0,sizeof(t));t.fraction=1;CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,1,&t,1);
 assert(t.allsolid&&g_SPXBCoopFxTrace[10]==fills+3);cg_entities[1].currentState.modelindex=2;
 // Default, ordinary SP, wrong mode and one-player split all stay on the old path.
 requested=0;CG_STEFX_FxTrace(&t,a,0,0,b,-1,1);assert(!g_SPXBCoopFxTrace[1]);
 s_fxTraceMode.integer=1;cg_stefxSplitScreen.integer=0;CG_STEFX_FxTrace(&t,a,0,0,b,-1,1);assert(!g_SPXBCoopFxTrace[1]);
 cg_stefxSplitScreen.integer=1;cg_stefxSplitScreenPlayers.integer=1;assert(!STEFX_FxTraceMode());
 cg_stefxSplitScreenPlayers.integer=2;strcpy(s_fxTraceGameMode.string,"holomatch");assert(!STEFX_FxTraceMode());strcpy(s_fxTraceGameMode.string,"coop");
 // A false rejection is detected and the original hit is retained immediately.
 cg_numSolidEntities=1;cg_solidEntities[0]=&cg_entities[1];cg_entities[1].lerpAngles[1]=0;
 VectorCopy(cg_entities[1].currentState.pos.trBase,a);VectorCopy(a,b);forceHit=2;memset(s_fxTraceBounds,0,sizeof(s_fxTraceBounds));
 memset(&t,0,sizeof(t));t.fraction=1;CG_ClipMoveToEntitiesInternal(a,0,0,b,-1,1,&t,2);
 assert(t.allsolid&&t.fraction==0&&s_fxTraceFailed&&g_SPXBCoopFxTrace[6]==1&&logs==1&&!STEFX_FxTraceMode());
 float lo[3],hi[3],nan=(float)sqrt(-1.0);a[0]=nan;assert(!STEFX_FxSweptBounds(a,b,0,0,lo,hi));
 printf("40000 original/optimized/verifier trace comparisons; calls %ld -> %ld; isolation, stationary/null hulls, mask rejection, rotated fallback, map invalidation and verifier failure passed.\n",baselineCalls,fastCalls);
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_fx_trace_test.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(prefix+bounds+body+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/O2','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'coop_fx_trace_test.txt').write_text(r.stdout)
