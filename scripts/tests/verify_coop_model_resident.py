"""Actual cache and stream binding: exact attributes, co-op isolation and GPU lifetime."""
import ast,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
tree=ast.parse((root/'scripts/tests/verify_world_vertices.py').read_text())
prefix=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='prefix' for t in n.targets))
prefix=prefix[:prefix.index('struct cvar_t')]
prefix=prefix.replace('#define STEFX_SP_HOSTED_MP 1','#define STEFX_ELITE_FORCE_SP 1\n#define RT_MODEL 1')
prefix=prefix.replace('CGEN_VERTEX };','CGEN_VERTEX, CGEN_LIGHTING_DIFFUSE };')
prefix=prefix.replace('int renderfx;','int renderfx,reType,frame,oldframe;')
prefix=prefix.replace('fading,numVertexes;', 'fading,numVertexes,numIndexes;GLushort indexes[6000];')
# Read flag values from the real renderer header instead of duplicating them.
flags=(root/'code/renderer/tr_types.h').read_text()
prefix+='\n'.join(re.findall(r'^#define\s+RF_\w+\s+0x[0-9a-fA-F]+.*$',flags,re.M))+'\n'
prefix+=r"""
#define D3DVSDT_NONE 0
#define D3DVSDT_FLOAT3 3
#define D3DVSDT_FLOAT2 2
#define D3DVSDT_D3DCOLOR 4
struct md3Surface_t{int numFrames,numVerts,numTriangles;};
struct cvar_t{int integer;const char*string;};cvar_t mode={0,"0"},split={1,"1"},gameMode={0,"coop"};
cvar_t*Cvar_Get(const char*n,const char*d,int){
 if(!strcmp(n,"r_efCoopModelResident")){assert(!strcmp(d,"0"));return &mode;}
 if(!strcmp(n,"stefx_splitScreenMode"))return &gameMode;
 assert(!strcmp(n,"stefx_splitScreen"));return &split;
}
int Q_stricmp(const char*a,const char*b){return _stricmp(a,b);}
void XBLog_WriteCriticalf(const char*,...){}
void XBLog_WriteCritical(const char*){}
int allocations,live,blocks,failed;
void*XPhysicalAlloc(unsigned int n,unsigned int,unsigned int alignment,unsigned int){assert(alignment==4096);++allocations;if(failed)return NULL;++live;return malloc(n);}
void XPhysicalFree(void*p){assert(blocks>0);--live;free(p);}
struct Device{void BlockUntilIdle(){++blocks;}}device;
struct State{Device*device;int colorArrayState;DWORD currentColor;DWORD*drawArray;int drawStride;}state={&device,1,0xffffffff,0,13};State*glw_state=&state;
bool g_stefxWorldBasePass=true;
"""
source=(root/'code/win32/win_qgl_dx8.cpp').read_text()
a=source.index('struct stefxWorldVertex_t');b=source.index('struct stefxWorldVertexKey_t',a)
vertex=source[a:b]
a=source.index('static void STEFX_WorldPackVertex(');b=source.index('static stefxWorldVertex_t *STEFX_WorldArraysClaim',a)
pack=source[a:b]
fn=source[source.index('static void dllDrawElements('):]
stream=fn[fn.index('\tconst unsigned int xyzStride ='):fn.index('\t// Send thru the index data')]
streams='void streams(DWORD*stream,int normals,int tex0,int tex1){DWORD*residentPayload=stream;bool interleavedPayload=false;unsigned int interleavedStride=0;int payloadVertices=tess.numVertexes;'+stream+'}\n'
helper=(root/'code/win32/stefx_coop_model_resident.h').read_text()
suffix=r"""
md3Surface_t surface={1,3,1},surface2={1,3,1};shader_t shader={1,0,0,"prop"};shaderStage_t stage;
entity_t object;
void batch(md3Surface_t*s=&surface){STEFX_CoopModelBeginBatch();tess.numVertexes=0;tess.numIndexes=0;STEFX_CoopModelSurface(s);tess.numVertexes=3;tess.numIndexes=3;for(int i=0;i<3;++i)tess.indexes[i]=i;}
stefxWorldVertex_t*claim(int n=1,int t0=1,int t1=1){return STEFX_CoopModelClaim(GL_TRIANGLES,3,tess.indexes,n,t0,t1);}
int main(){
 tess.shader=&shader;tess.xstages=&stage;stage.active=1;stage.rgbGen=CGEN_LIGHTING_DIFFUSE;stage.alphaGen=AGEN_SKIP;
 stage.bundle[0].tcGen=stage.bundle[1].tcGen=TCGEN_TEXTURE;backEnd.currentEntity=&object;object.e.reType=RT_MODEL;backEnd.viewParms.stefxSplitView=1;
 for(int v=0;v<3;++v){for(int c=0;c<4;++c){tess.xyz[v][c]=v+c*.25f;tess.normal[v][c]=v-c*.5f;}tess.svars.colors[v]=0xffffffff;
 for(int b=0;b<2;++b)for(int c=0;c<2;++c)tess.svars.texcoords[b][v][c]=v+b+c*.75f;}
 batch();assert(!claim()&&!allocations);mode.integer=2;
 split.integer=0;assert(!claim()&&!allocations);split.integer=1;
 gameMode.string="holomatch";assert(!claim()&&!allocations);gameMode.string="coop";
 backEnd.viewParms.stefxSplitView=0;assert(!claim()&&!allocations);backEnd.viewParms.stefxSplitView=1;
 backEnd.projection2D=1;assert(!claim()&&!allocations);backEnd.projection2D=0;
 for(int fmt=0;fmt<8;++fmt){batch();int n=fmt&1,t0=fmt&2,t1=fmt&4;
  stefxWorldVertex_t*p=claim(n,t0,t1);assert(p);unsigned char saved[144];memcpy(saved,p,144);
  assert(claim(n,t0,t1)==p&&!memcmp(saved,p,144));
  DWORD commands[64];state.drawArray=commands;streams((DWORD*)p,n,t0,t1);
  const int slots[]={0,2,3,9,10};const int offsets[]={0,12,24,28,36};DWORD*cmd=commands+17;
  for(int attr=0;attr<5;++attr){if((attr==1&&!n)||(attr==3&&!t0)||(attr==4&&!t1))continue;
   ++cmd;DWORD address=*cmd++;assert(address==((DWORD)p+offsets[attr]));assert((commands[1+slots[attr]]>>8)==48);
   for(int v=0;v<3;++v){const void*want=attr==0?(void*)tess.xyz[v]:attr==1?(void*)tess.normal[v]:attr==2?(void*)&tess.svars.colors[v]:(void*)tess.svars.texcoords[attr-3][v];
    assert(!memcmp((char*)address+48*v,want,(attr<2?3:attr==2?1:2)*4));}}
  assert(cmd==state.drawArray);
 }
 assert(live==1&&allocations==1&&g_SPXBCoopModelResident[3]==8&&g_SPXBCoopModelResident[2]==8&&g_SPXBCoopModelResident[8]==0);
 // Unsupported mutable attributes and animated/multiple-surface batches fall back.
 surface.numFrames=2;assert(!claim());surface.numFrames=1;
 object.e.frame=1;assert(!claim());object.e.frame=0;object.e.oldframe=1;assert(!claim());object.e.oldframe=0;
 const int allowed[]={RF_MORELIGHT,RF_THIRD_PERSON,RF_FIRST_PERSON,RF_DEPTHHACK,RF_NODEPTH,
 RF_NOSHADOW,RF_LIGHTING_ORIGIN,RF_SHADOW_PLANE,RF_WRAP_FRAMES,RF_CAP_FRAMES,
 RF_SETANIMINDEX,RF_STEFX_SPLIT_SLOT0,RF_STEFX_SPLIT_SLOT1,RF_STEFX_SPLIT_SLOT2,
 RF_STEFX_SPLIT_HIDE_SLOT0,RF_STEFX_SPLIT_HIDE_SLOT1};
 int allFlags=0;for(int f=0;f<sizeof(allowed)/sizeof(allowed[0]);++f){
  object.e.renderfx=allowed[f];assert(claim());allFlags|=allowed[f];}
 object.e.renderfx=allFlags;assert(claim());
 const int rejected[]={RF_VOLUMETRIC,RF_ALPHA_FADE,RF_PULSATE,RF_RGB_TINT,
 RF_DISINTEGRATE1,RF_DISINTEGRATE2,RF_DISTORTION,RF_FORKED,RF_TAPERED,RF_GROW,
 RF_G2MINLOD,RF_SHADOW_ONLY,RF_XBOX_NOCULL_BMODEL,(int)0x80000000};
 for(int f=0;f<sizeof(rejected)/sizeof(rejected[0]);++f){
  object.e.renderfx=allFlags|rejected[f];assert(!claim());}
 assert(g_SPXBCoopModelRejects[3]==sizeof(rejected)/sizeof(rejected[0]));object.e.renderfx=0;
 stage.bundle[0].numTexMods=1;assert(!claim());stage.bundle[0].numTexMods=0;
 stage.bundle[1].tcGen=TCGEN_ENVIRONMENT_MAPPED;assert(!claim());stage.bundle[1].tcGen=TCGEN_TEXTURE;
 stage.rgbGen=CGEN_VERTEX;assert(!claim());stage.rgbGen=CGEN_LIGHTING_DIFFUSE;
 tess.fogNum=1;stage.adjustColorsForFog=1;assert(!claim());tess.fogNum=0;stage.adjustColorsForFog=0;
 g_stefxWorldBasePass=false;assert(!claim());g_stefxWorldBasePass=true;
 GLushort ix[3]={0,1,2};assert(!STEFX_CoopModelClaim(GL_TRIANGLES,3,ix,1,1,1));
 tess.indexes[2]=3;assert(!claim());tess.indexes[2]=2;
 STEFX_CoopModelSurface(&surface2);assert(!claim());batch();
 // A distinct constant-color value creates a distinct immutable entry.
 for(int v=0;v<3;++v)tess.svars.colors[v]=0xff224466;assert(claim());assert(g_SPXBCoopModelResident[3]==9);
 // Verifier detects any changed attribute and falls back on this same draw.
 tess.xyz[1][0]+=1;assert(!claim()&&mode.integer==0&&g_SPXBCoopModelResident[8]==1);tess.xyz[1][0]-=1;
 STEFX_CoopModelReset();assert(!live&&blocks==1&&!s_coopModelUsed);mode.integer=2;batch();
 failed=1;assert(!claim()&&allocations==2);assert(!claim()&&allocations==2);failed=0;
 STEFX_CoopModelReset();batch();assert(claim()&&allocations==3);
 s_coopModelUsed=STEFX_COOP_MODEL_CACHE_BYTES/sizeof(stefxWorldVertex_t);batch(&surface2);assert(!claim()&&g_SPXBCoopModelResident[5]==1);
 batch();assert(claim());STEFX_CoopModelReset();assert(!live);
 // Turning the experiment off or leaving co-op releases at frame begin only.
 batch();assert(claim());int oldBlocks=blocks;mode.integer=0;assert(!claim()&&live);
 STEFX_CoopModelConfigure();assert(!live&&blocks==oldBlocks+1&&!s_coopModelAllocAttempted);
 STEFX_CoopModelConfigure();assert(blocks==oldBlocks+1);
 mode.integer=1;batch();assert(claim());gameMode.string="holomatch";assert(!claim()&&live);
 STEFX_CoopModelConfigure();assert(!live);gameMode.string="coop";batch();assert(claim());
 split.integer=0;STEFX_CoopModelConfigure();assert(!live);
 puts("Co-op model residency: all 8 attribute layouts and stream addresses; reuse, SP/MP-mode/HUD gates, animation/mutable-state rejection, byte mismatch fallback, bounded capacity, failed-allocation retry suppression and fenced map reset passed.");
}
"""
out=root/'build/research/letterbox_perf';cpp=out/'coop_model_resident_test.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(prefix+vertex+pack+helper+streams+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/O2','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'coop_model_resident_test.txt').write_text(r.stdout)
