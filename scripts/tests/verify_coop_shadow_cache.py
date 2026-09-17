"""Exercise actual bounded cache and mode guard, including verifier fallback."""
import re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=(root/'SP-Mod-Source-Code-master/cgame/cg_marks.cpp').read_text()
guard=source[source.index('#define STEFX_COOP_SHADOW_CACHE 1'):source.index('#endif')]
guard=re.sub(r'^#include.*\n','',guard,flags=re.M)
header=(root/'SP-Mod-Source-Code-master/cgame/stefx_shadow_cache.h').read_text()
assert 'temporary && markShader==cgs.media.shadowMarkShader' in source
assert 's_shadowCache.Reset(); s_shadowFrame=-1;' in source
for file,function in [('cg_main.cpp','void CG_PreInit()'),('cg_snapshot.cpp','void CG_RestartLevel( void )')]:
    text=(root/'SP-Mod-Source-Code-master/cgame'/file).read_text()
    assert 'CG_InitMarkPolys();' in text[text.index(function):text.index(function)+1600]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef float vec3_t[3];
struct markFragment_t {int firstPoint,numPoints;};
struct vmCvar_t{int integer;char string[32];};
vmCvar_t cg_stefxSplitScreen,cg_stefxSplitScreenPlayers;
struct{int clientFrame;}cg;
int requested;const char*gameMode="coop";
void cgi_Cvar_Register(vmCvar_t*v,const char*n,const char*d,int){
 if(!strcmp(n,"cg_efCoopShadowCache")){assert(!strcmp(d,"0"));v->integer=requested;strcpy(v->string,"0");}
 else {v->integer=0;strcpy(v->string,gameMode);}
}
void cgi_Cvar_Update(vmCvar_t*v){if(v->string[0]=='0')v->integer=requested;else strcpy(v->string,gameMode);}
int Q_stricmp(const char*a,const char*b){return _stricmp(a,b);}
int calls,shape,world;
int original(int n,const vec3_t*q,const vec3_t projection,int maxP,vec3_t out,int maxF,markFragment_t*f){
 ++calls;assert(n==4);
 int count=shape==1?17:shape==2?1:shape==3?0:2;
 if(count>maxF)count=maxF;
 for(int i=0;i<count;++i){f[i].firstPoint=3*i;f[i].numPoints=3;
  for(int j=0;j<9;++j)out[9*i+j]=q[(j/3)%4][j%3]+projection[j%3]+(float)(world+i+maxP);}
 if(shape==2){f[0].numPoints=65;for(int k=0;k<195;++k)out[k]=(float)k;}
 return count;
}
'''
body=r'''
int main(){
 cg_stefxSplitScreen.integer=1;cg_stefxSplitScreenPlayers.integer=2;
 assert(!STEFX_ShadowCacheMode());requested=1;++cg.clientFrame;assert(STEFX_ShadowCacheMode()==1);
 cg_stefxSplitScreen.integer=0;assert(!STEFX_ShadowCacheMode());cg_stefxSplitScreen.integer=1;
 cg_stefxSplitScreenPlayers.integer=1;assert(!STEFX_ShadowCacheMode());cg_stefxSplitScreenPlayers.integer=2;
 gameMode="holomatch";++cg.clientFrame;assert(!STEFX_ShadowCacheMode());gameMode="coop";++cg.clientFrame;
 assert(STEFX_ShadowCacheMode()==1);
 vec3_t polygon[4]={{0}},projection={0,0,-20};float out[384*3],expected[384*3];markFragment_t f[128],ef[128];
 volatile unsigned int stats[12]={0};s_shadowCache.Reset();
 for(int i=0;i<20000;++i){
  int key=rand()%48;polygon[0][0]=(float)key;projection[2]=(i%13)?-20.f:-15.f;
  int n=s_shadowCache.Call(i%3?1:2,polygon,projection,384,out,128,f,original,stats);
  int en=original(4,polygon,projection,384,expected,128,ef);
  assert(n==en&&!memcmp(f,ef,n*sizeof(*f))&&!memcmp(out,expected,18*sizeof(float))&&!s_shadowCache.failed);
  out[0]=999999;f[0].numPoints=1; // caller mutation must not alter cached geometry
 }
 assert(stats[2]&&stats[3]&&stats[4]&&stats[6]&&!stats[7]);
 s_shadowCache.Reset();int c=calls;
 s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);
 s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);assert(calls==c+1);
 ++world;s_shadowCache.Reset();s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);assert(calls==c+2);
 // Changed renderer output limits cannot reuse results from other limits.
 s_shadowCache.Call(1,polygon,projection,383,out,128,f,original,stats);assert(calls==c+3);
 // Capacity excess falls back on every call without reducing renderer limits.
 for(shape=1;shape<=2;++shape){s_shadowCache.Reset();c=calls;
  s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);
  s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);assert(calls==c+2);}
 shape=3;s_shadowCache.Reset();c=calls;
 assert(!s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats));
 assert(!s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats));assert(calls==c+1);
 // Forced age wrap invalidates all old entries.
 s_shadowCache.serial=~0u;s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);assert(stats[11]);
 shape=0;s_shadowCache.Reset();s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);
 ++world;int n=s_shadowCache.Call(2,polygon,projection,384,out,128,f,original,stats);
 int en=original(4,polygon,projection,384,expected,128,ef);
 assert(n==en&&!memcmp(out,expected,18*sizeof(float))&&s_shadowCache.failed&&stats[7]==1&&!STEFX_ShadowCacheMode());
 s_shadowCache.Reset();c=calls;s_shadowCache.Call(1,polygon,projection,384,out,128,f,original,stats);assert(calls==c+1&&s_shadowCache.failed);
 printf("20000 exact cached/original comparisons passed; isolation, eviction, caller mutation, world reset, capacity fallback, empty results, wrap and verifier fail-safe passed. Cache bytes=%u\n",(unsigned)sizeof(s_shadowCache));
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_shadow_cache_test.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(prefix+header+guard+body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/O2','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],check=True,capture_output=True,text=True);print(r.stdout);(out/'coop_shadow_cache_test.txt').write_text(r.stdout)
