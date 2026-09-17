"""Exercise the actual scratch allocator, capacity checks and GPU fence rotation."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'code/win32/win_qgl_dx8.cpp').read_text()
a=s.index('#define STEFX_SCRATCH_BUFFER_COUNT');b=s.index('#endif // _XBOX',a)
core=s[a:b]
prefix=r'''
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
typedef unsigned long DWORD;typedef int qboolean;
#define qtrue 1
#define qfalse 0
#define MAXULONG_PTR 0xffffffffu
#define PAGE_READWRITE 4
#define PAGE_WRITECOMBINE 8
#define STEFX_SP_HOSTED_MP 1
struct cvar_t {int integer;};
cvar_t cvEnable={1},cvAB={0},cvBuffers={2},cvKB={1024};
cvar_t *Cvar_Get(const char*n,const char*,int){if(!strcmp(n,"r_efScratchVerts"))return &cvEnable;if(!strcmp(n,"r_efScratchAB"))return &cvAB;if(!strcmp(n,"r_efScratchBuffers"))return &cvBuffers;return &cvKB;}
void XBLog_WriteCritical(const char*){}
static void STEFX_WorldVerticesReport(){}
void XBLog_WriteCriticalf(const char*,...){}
int allocCall,failCall,liveCount,tick;void*live[16];
void*XPhysicalAlloc(unsigned int bytes,unsigned int,unsigned int,unsigned int){++allocCall;if(failCall==-1||allocCall==failCall)return NULL;void*p=malloc(bytes);assert(p);live[liveCount++]=p;return p;}
void XPhysicalFree(void*p){int i;for(i=0;i<liveCount&&live[i]!=p;++i){}assert(i<liveCount);live[i]=live[--liveCount];free(p);}
int Sys_Milliseconds(){return tick;}
struct Device{unsigned int serial,blocks;bool pending[4096];
 Device():serial(0),blocks(0){memset(pending,0,sizeof(pending));}
 bool IsFencePending(DWORD f){assert(f<4096);return pending[f];}
 void BlockOnFence(DWORD f){assert(pending[f]);pending[f]=false;++blocks;tick+=7;}
 DWORD InsertFence(){assert(serial+1<4096);pending[++serial]=true;return serial;}
} dev;
struct State{Device*device;} state={&dev};State*glw_state=&state;
'''
suffix=r'''
void reset(int count,int kb,int fail=0){while(liveCount)XPhysicalFree(live[0]);memset(s_stefxScratchBase,0,sizeof(s_stefxScratchBase));memset(s_stefxScratchFence,0,sizeof(s_stefxScratchFence));memset((void*)g_SPXBScratchLayoutProof,0,sizeof(g_SPXBScratchLayoutProof));s_stefxScratchInitDone=0;s_stefxScratchReady=0;s_stefxScratchIndex=0;s_stefxScratchOffset=0;s_stefxScratchFrameActive=0;STEFX_ScratchSelectLayout(2,1024);allocCall=0;failCall=fail;cvBuffers.integer=count;cvKB.integer=kb;cvEnable.integer=1;cvAB.integer=0;dev=Device();tick=0;}
int main(){int n,i;
 for(n=0;n<4;++n){int count=n==1||n==3?3:2;int kb=n>=2?1536:1024;reset(count,kb);STEFX_ScratchFrameBegin();assert(s_stefxScratchReady&&liveCount==count);assert(count*s_stefxScratchCapacityDwords*4u<=4608u*1024u);assert(g_SPXBScratchLayoutProof[0]==(unsigned int)count&&g_SPXBScratchLayoutProof[3]==1);assert(s_stefxScratchCapacityDwords==(kb==1536?384u*1024u:256u*1024u));
  DWORD*start=STEFX_ScratchClaim(s_stefxScratchCapacityDwords-4);assert(start==s_stefxScratchBase[s_stefxScratchIndex]);assert(STEFX_ScratchClaim(1)==start+s_stefxScratchCapacityDwords-4);assert(!STEFX_ScratchClaim(1));assert(!STEFX_ScratchClaim(UINT_MAX));assert(!STEFX_ScratchClaim(UINT_MAX-1));STEFX_ScratchFrameEnd();
  for(i=1;i<12;++i){STEFX_ScratchFrameBegin();assert(s_stefxScratchIndex==(i+1)%count);assert(s_stefxScratchOffset==0);assert(dev.blocks==(unsigned int)(i>=count?i-count+1:0));STEFX_ScratchFrameEnd();}
  cvEnable.integer=0;unsigned int serial=dev.serial;STEFX_ScratchFrameBegin();assert(!s_stefxScratchFrameActive);STEFX_ScratchFrameEnd();assert(dev.serial==serial);
 }
 for(i=1;i<=3;++i){reset(3,1024,i);STEFX_ScratchInit();assert(s_stefxScratchReady&&liveCount==2&&s_stefxScratchBufferCount==2);assert(g_SPXBScratchLayoutProof[2]==1);}
 for(i=1;i<=3;++i){reset(3,1536,i);STEFX_ScratchInit();assert(s_stefxScratchReady&&liveCount==2&&s_stefxScratchCapacityDwords==256u*1024u);assert(g_SPXBScratchLayoutProof[2]==1);}
 for(i=1;i<=2;++i){reset(2,1536,i);STEFX_ScratchInit();assert(s_stefxScratchReady&&liveCount==2&&s_stefxScratchCapacityDwords==256u*1024u);assert(g_SPXBScratchLayoutProof[2]==1);}
 reset(3,1024,-1);STEFX_ScratchFrameBegin();assert(!s_stefxScratchReady&&!s_stefxScratchFrameActive&&!liveCount);STEFX_ScratchFrameEnd();assert(dev.serial==0);
 reset(-200,INT_MAX);STEFX_ScratchInit();assert(s_stefxScratchBufferCount==2&&s_stefxScratchCapacityDwords==256u*1024u);
 reset(2,1024);printf("Scratch layout tests passed: capacity/overflow, 2/3-slot fence reuse, disable, partial allocation cleanup, fallback, invalid settings.\n");return 0;}
'''
out=root/'build/research/letterbox_perf';cpp=out/'scratch_layout_test.cpp';cpp.write_text(prefix+core+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC');exe=out/'scratch_layout_test.exe'
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'scratch_layout_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'scratch_layout_test.txt').write_text(r.stdout)

# The same source must keep SP on its original layout even when MP settings exist.
sp_main=r"""
int main(){reset(3,1536);STEFX_ScratchInit();assert(s_stefxScratchBufferCount==2);assert(s_stefxScratchCapacityDwords==256u*1024u);assert(liveCount==2);reset(2,1024);puts("SP retains original 2x1MiB layout.");return 0;}
"""
cpp.write_text(prefix.replace('#define STEFX_SP_HOSTED_MP 1','')+core+suffix[:suffix.index('int main()')]+sp_main)
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'scratch_layout_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout)
with (out/'scratch_layout_test.txt').open('a') as f:f.write(r.stdout)
