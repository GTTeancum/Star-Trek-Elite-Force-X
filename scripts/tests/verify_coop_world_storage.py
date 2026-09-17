"""Actual SP scratch/cache integration: GPU lifetime, ownership and bounded storage."""
import ast
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
def literal(path, name):
    tree=ast.parse((root/path).read_text())
    return next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and
                any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
prefix=literal('scripts/tests/verify_world_vertices.py','prefix')
prefix=prefix.replace('#define STEFX_SP_HOSTED_MP 1','#define STEFX_ELITE_FORCE_SP 1')
a=prefix.index('struct cvar_t'); prefix=prefix[:a]+r'''
typedef int qboolean;
#define qtrue 1
#define qfalse 0
struct cvar_t{int integer;};cvar_t enabled={0},verify={1},cvScratch={1},cvAB={0};
int split=1;
cvar_t*Cvar_Get(const char*n,const char*d,int){
 if(!strcmp(n,"r_efCoopWorldVertices")){assert(!strcmp(d,"0"));return &enabled;}
 if(!strcmp(n,"r_efCoopWorldVerify")){assert(!strcmp(d,"0"));return &verify;}
 if(!strcmp(n,"r_efScratchVerts"))return &cvScratch;
 assert(!strcmp(n,"r_efScratchAB"));return &cvAB;
}
int Cvar_VariableIntegerValue(const char*n){assert(!strcmp(n,"stefx_splitScreen"));return split;}
void XBLog_WriteCriticalf(const char*,...){}
void XBLog_WriteCritical(const char*){}
int live,blocks,allocations,freed,failAllocation,tick,waits;
void*XPhysicalAlloc(unsigned int bytes,unsigned int,unsigned int,unsigned int){if(failAllocation)return NULL;++live;++allocations;return malloc(bytes);}
void XPhysicalFree(void*p){--live;++freed;free(p);}
int Sys_Milliseconds(){return tick;}
struct Device{unsigned int serial;bool pending[4096];
 Device():serial(0){memset(pending,0,sizeof(pending));}
 void BlockUntilIdle(){++blocks;memset(pending,0,sizeof(pending));}
 bool IsFencePending(DWORD f){assert(f<4096);return pending[f];}
 void BlockOnFence(DWORD f){assert(pending[f]);pending[f]=false;++waits;tick+=7;}
 DWORD InsertFence(){assert(serial+1<4096);pending[++serial]=true;return serial;}
} device;
struct State {Device*device;int colorArrayState;DWORD currentColor;}state={&device,1,0xffffffff};State*glw_state=&state;
bool g_stefxWorldBasePass=false;
'''
source=(root/'code/win32/win_qgl_dx8.cpp').read_text()
a=source.index('#define STEFX_SCRATCH_BUFFER_COUNT');b=source.index('#endif // _XBOX',a)
core='static void STEFX_CoopModelReset(void){}\nstatic void STEFX_CoopModelConfigure(void){}\n'+source[a:b]+source.split('// STEFX_WORLD_VERTICES_BEGIN',1)[1].split('// STEFX_WORLD_VERTICES_END',1)[0]
suffix=r'''
shader_t shader={1,0,0,"coop-world"};shaderStage_t stage;
void batch(const void*a){STEFX_WorldVerticesBeginBatch();g_stefxWorldBasePass=true;tess.numVertexes=0;
 STEFX_WorldVerticesSurface(a,4);tess.numVertexes=4;
 for(int v=0;v<4;++v){for(int j=0;j<4;++j){tess.xyz[v][j]=v+j*.25f;tess.normal[v][j]=v-j*.5f;}
 tess.svars.colors[v]=0xff001234+v;
 for(int b=0;b<2;++b)for(int j=0;j<2;++j)tess.svars.texcoords[b][v][j]=v+b+j*.5f;}}
GLushort ix[6]={0,1,2,0,2,3};
stefxWorldVertex_t*claim(){return STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ix,1,1,1);}
void finish(){STEFX_ScratchFrameEnd();}
int main(){int a,b;
 tess.shader=&shader;tess.xstages=&stage;stage.active=1;stage.rgbGen=CGEN_IDENTITY;stage.alphaGen=AGEN_SKIP;
 stage.bundle[0].tcGen=TCGEN_TEXTURE;stage.bundle[1].tcGen=TCGEN_LIGHTMAP;
 backEnd.currentEntity=&tr.worldEntity;backEnd.viewParms.stefxSplitView=1;tr.identityLightByte=255;
 STEFX_ScratchFrameBegin();assert(live==2&&allocations==2&&!freed&&s_stefxScratchBufferCount==2&&!s_coopWorldBorrowed);
 batch(&a);assert(!claim());assert(STEFX_ScratchClaim(12)==s_stefxScratchBase[1]);finish();
 // Mode changes cannot repurpose the previous frame's slab until frame begin.
 enabled.integer=2;batch(&a);assert(!claim()&&!blocks);
 STEFX_ScratchFrameBegin();assert(blocks==1&&s_coopWorldBorrowed&&s_stefxScratchBufferCount==1&&s_stefxScratchIndex==0);
 batch(&a);stefxWorldVertex_t*p=claim();assert(p==(stefxWorldVertex_t*)s_stefxScratchBase[1]&&live==2&&allocations==2&&!freed);
 assert(s_worldArraysActive&&s_worldVertexUsed==6&&g_SPXBWorldArrays[5]==6);
 unsigned char saved[6*sizeof(stefxWorldVertex_t)];memcpy(saved,p,sizeof(saved));finish();
 for(int i=0;i<10;++i){STEFX_ScratchFrameBegin();assert(s_stefxScratchIndex==0);
  DWORD*w=STEFX_ScratchClaim(s_stefxScratchCapacityDwords);assert(w==s_stefxScratchBase[0]);memset(w,i,s_stefxScratchCapacityDwords*4);
  assert(!STEFX_ScratchClaim(1));batch(&a);assert(claim()==p&&s_worldVertexUsed==6);assert(!memcmp(saved,p,sizeof(saved)));finish();}
 assert(waits==10&&blocks==1&&allocations==2&&!freed);
 assert(g_SPXBCoopWorldStorage[3]==2*1024*1024&&g_SPXBCoopWorldStorage[5]<=1024*1024);
 printf("SP cache metadata: %u bytes; physical allocation: %u bytes\n",g_SPXBCoopWorldStorage[6],g_SPXBCoopWorldStorage[3]);
 // Capacity fallback preserves old resident geometry.
 s_worldVertexUsed=STEFX_WORLD_ARRAY_CAPACITY-3;batch(&b);assert(!claim());batch(&a);assert(claim()==p&&!memcmp(saved,p,sizeof(saved)));
 STEFX_WorldVerticesReset();assert(blocks==2&&!s_worldVertices&&!s_worldVertexUsed&&live==2&&!freed&&s_coopWorldBorrowed);
 batch(&b);assert(claim()==p&&s_worldVertexUsed==6);finish();
 // Reserve-only control is distinct from cache mode, with identical slab budget.
 enabled.integer=1;STEFX_ScratchFrameBegin();batch(&a);assert(!claim()&&s_stefxScratchBufferCount==1);finish();
 enabled.integer=0;STEFX_ScratchFrameBegin();assert(!s_coopWorldBorrowed&&s_stefxScratchBufferCount==2&&!s_worldVertices&&blocks>=3);
 assert(STEFX_ScratchClaim(8)==s_stefxScratchBase[1]);finish();
 enabled.integer=1;STEFX_ScratchFrameBegin();batch(&a);assert(!claim()&&s_coopWorldBorrowed&&!s_worldVertices);finish();
 enabled.integer=2;STEFX_ScratchFrameBegin();batch(&a);assert(claim()==p);finish();
 // SP cannot retain a slab reserved for co-op.
 split=0;STEFX_ScratchFrameBegin();batch(&a);assert(!claim()&&!s_coopWorldBorrowed&&s_stefxScratchBufferCount==2);finish();
 split=1;enabled.integer=2;state.device=NULL;STEFX_CoopWorldConfigure();assert(!s_coopWorldBorrowed);state.device=&device;
 DWORD*savedBase=s_stefxScratchBase[1];s_stefxScratchBase[1]=NULL;STEFX_CoopWorldConfigure();assert(!s_coopWorldBorrowed);s_stefxScratchBase[1]=savedBase;
 unsigned int cap=s_stefxScratchCapacityDwords;s_stefxScratchCapacityDwords=1;STEFX_CoopWorldConfigure();assert(!s_coopWorldBorrowed);s_stefxScratchCapacityDwords=cap;
 STEFX_ScratchFrameBegin();batch(&a);assert(claim()==p);
 // A byte mismatch disables reuse immediately, but ownership returns only after a fence.
 tess.xyz[0][0]+=1;assert(!claim()&&!enabled.integer&&s_coopWorldBorrowed&&g_SPXBWorldArrays[6]==1);finish();
 STEFX_ScratchFrameBegin();assert(!s_coopWorldBorrowed&&s_stefxScratchBufferCount==2&&!s_worldVertices);finish();
 // Force every surface into the same hash chain: at most32 entries may be probed.
 enabled.integer=2;STEFX_ScratchFrameBegin();
 for(int collision=0;collision<32;++collision){batch((void*)(0x10000+collision*0x10000));assert(claim()==p);}
 batch((void*)(0x10000+32*0x10000));assert(!claim()&&s_worldVertexUsed==32*6&&g_SPXBWorldVertices[4]==1);
 for(int collision=0;collision<32;++collision){batch((void*)(0x10000+collision*0x10000));assert(claim()==p);}
 finish();enabled.integer=0;STEFX_ScratchFrameBegin();assert(!s_worldVertices);finish();
 device.BlockUntilIdle();for(int j=0;j<2;++j){XPhysicalFree(s_stefxScratchBase[j]);s_stefxScratchBase[j]=NULL;}
 assert(!live&&freed==2);failAllocation=1;s_stefxScratchInitDone=0;s_stefxScratchReady=0;enabled.integer=2;
 STEFX_ScratchFrameBegin();assert(!s_stefxScratchFrameActive&&!s_coopWorldBorrowed&&!s_stefxScratchReady&&!live);finish();
 puts("Co-op resident storage: default/control/cache modes, fenced ownership, immutable reuse, bounded capacity, no additional physical allocation, map reset, disable/mismatch return, SP and failure fallbacks passed.");
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_world_storage_test.cpp';exe=out/'coop_world_storage_test.exe'
cpp.write_text(prefix+core+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_world_storage_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'coop_world_storage_test.txt').write_text(r.stdout)
