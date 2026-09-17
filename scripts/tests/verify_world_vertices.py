"""Exercise the actual immutable-storage helper with a simulated GPU lifetime."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
prefix = r'''
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned long DWORD;
typedef unsigned short GLushort;
typedef int GLenum;
#define GL_TRIANGLES 4
#define D3DPUSH_ENCODE(method,count) ((method)|((count)<<18))
#define D3DPUSH_SET_BEGIN_END 0x17fc
#define D3DPUSH_NOINCREMENT_FLAG 0x40000000
#define _XBOX 1
#define STEFX_SP_HOSTED_MP 1
#define SHADER_MAX_VERTEXES 1000
#define SHADER_MAX_INDEXES 6000
#define MAXULONG_PTR 0xffffffffu
#define PAGE_READWRITE 4
#define PAGE_WRITECOMBINE 8
enum { CGEN_IDENTITY, CGEN_IDENTITY_LIGHTING, CGEN_CONST, CGEN_VERTEX };
enum { AGEN_IDENTITY, AGEN_CONST, AGEN_SKIP, AGEN_ENTITY };
enum { TCGEN_IDENTITY, TCGEN_TEXTURE, TCGEN_LIGHTMAP, TCGEN_LIGHTMAP1,
 TCGEN_LIGHTMAP2, TCGEN_LIGHTMAP3, TCGEN_VECTOR, TCGEN_ENVIRONMENT_MAPPED };
struct textureBundle_t { int numTexMods, tcGen; };
struct shaderStage_t { int active,isBumpMap,isEnvironment;void*ss;int adjustColorsForFog,rgbGen,alphaGen;textureBundle_t bundle[2]; };
struct shader_t {int numUnfoggedPasses,numDeforms;void*sky;const char*name;};
struct entity_t {struct {int renderfx;}e;};
struct {entity_t worldEntity;int identityLightByte;}tr;
struct {int projection2D;entity_t*currentEntity;struct{int stefxSplitView;}viewParms;}backEnd;
struct {shader_t*shader;shaderStage_t*xstages;int currentPass,fogNum,dlightBits,fading,numVertexes;
 float xyz[1000][4],normal[1000][4];struct{DWORD colors[1000];float texcoords[2][1000][2];}svars;}tess;
struct cvar_t{int integer;};cvar_t enabled={1},verify={1};
cvar_t*Cvar_Get(const char*n,const char*,int){return !strcmp(n,"r_efWorldVertices")?&enabled:&verify;}
void XBLog_WriteCriticalf(const char*,...){}
int live,blocks,failAllocation;
void*XPhysicalAlloc(unsigned int bytes,unsigned int,unsigned int,unsigned int){if(failAllocation)return NULL;++live;return malloc(bytes);}
void XPhysicalFree(void*p){assert(blocks>0);--live;free(p);}
struct Device {void BlockUntilIdle(){++blocks;}}device;
struct State {Device*device;int colorArrayState;DWORD currentColor;}state={&device,1,0xffffffff};State*glw_state=&state;
extern "C" volatile unsigned int g_SPXBFxCull[3]={0};
extern "C" volatile unsigned int g_SPXBFxMerged=0;
'''
suffix = r'''
shader_t shader={1,0,0,"test"};shaderStage_t stage;
void batch(const void*a,const void*b=0){STEFX_WorldVerticesBeginBatch();g_stefxWorldBasePass=true;tess.numVertexes=0;
 STEFX_WorldVerticesSurface(a,3);tess.numVertexes=3;
 if(b){STEFX_WorldVerticesSurface(b,3);tess.numVertexes=6;}}
void fill(int offset,float value){for(int v=0;v<3;++v){tess.xyz[offset+v][0]=value+v;tess.svars.colors[offset+v]=0xffffffff;}}
int main(){int a,b,c;GLushort ix[6]={0,1,2,3,4,5};
 tess.shader=&shader;tess.xstages=&stage;stage.active=1;stage.rgbGen=CGEN_IDENTITY;stage.alphaGen=AGEN_SKIP;
 stage.bundle[0].tcGen=TCGEN_TEXTURE;backEnd.currentEntity=&tr.worldEntity;backEnd.viewParms.stefxSplitView=1;
 tr.identityLightByte=255;
 batch(&a,&b);fill(0,10);fill(3,20);assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ix,0,1,0));
 assert(s_worldVertexUsed==6&&g_SPXBWorldVertices[2]==2);unsigned short firstA=s_worldIndices[0],firstB=s_worldIndices[3];
 // Different visible ordering reuses the same storage and remaps correctly.
 batch(&b,&a);fill(0,20);fill(3,10);assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ix,0,1,0));
 assert(s_worldVertexUsed==6&&s_worldIndices[0]==firstB&&s_worldIndices[3]==firstA);
 assert(g_SPXBWorldVertices[6]==0);
 batch(&a);fill(0,10);stage.bundle[0].numTexMods=1;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));stage.bundle[0].numTexMods=0;
 stage.rgbGen=CGEN_VERTEX;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));stage.rgbGen=CGEN_IDENTITY;
 tess.dlightBits=1;assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 g_stefxWorldBasePass=false;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));g_stefxWorldBasePass=true;tess.dlightBits=0;
 tess.fogNum=1;assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 stage.adjustColorsForFog=1;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 tess.fogNum=0;assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));stage.adjustColorsForFog=0;
 tess.numVertexes=4;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 batch(&a);fill(0,10);GLushort bad[3]={0,1,3};assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,bad,0,1,0));assert(g_SPXBWorldVertices[7]==1);
 s_worldVertexUsed=STEFX_WORLD_VERTEX_CAPACITY;batch(&c);assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));assert(g_SPXBWorldVertices[4]==1);
 // Capacity exhaustion doesn't invalidate already resident geometry.
 batch(&a);fill(0,10);assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 fill(0,999);assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));assert(g_SPXBWorldVertices[6]==1&&!enabled.integer);
 STEFX_WorldVerticesReset();assert(!live&&blocks==1&&s_worldVertexUsed==0);
 enabled.integer=1;failAllocation=1;batch(&a);assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));assert(!live);
 STEFX_WorldVerticesReset();failAllocation=0;batch(&a);assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,3,ix,0,1,0));
 STEFX_WorldVerticesReset();assert(!live&&blocks==2);
 puts("World vertices: reordered visibility, immutable reuse, dynamic/fog exclusions, mixed-batch fallback, index/capacity bounds, verification failure, allocation failure and fenced map reset passed.");}
'''
out = root/'build/research/letterbox_perf'
cpp = out/'world_vertices_test.cpp'
source=(root/'code/win32/win_qgl_dx8.cpp').read_text()
core=source.split('// STEFX_WORLD_VERTICES_BEGIN',1)[1].split('// STEFX_WORLD_VERTICES_END',1)[0]
core = source[source.index('bool g_stefxWorldBasePass ='):source.index('bool g_stefxWorldBasePass =')+len('bool g_stefxWorldBasePass = false;')] + '\n' + core
cpp.write_text(prefix+core+suffix)
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
exe = out/'world_vertices_test.exe'
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
               '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'world_vertices_test.obj'),
               '/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r = subprocess.run([str(exe)],capture_output=True,text=True,check=True)
print(r.stdout)
(out/'world_vertices_test.txt').write_text(r.stdout)
