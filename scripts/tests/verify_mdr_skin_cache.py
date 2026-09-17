"""Compile the real CPU pose-cache helpers and exercise identity and bounds."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root/'code/renderer/tr_animation.cpp').read_text()
helper = source.split('// STEFX_MDR_SKIN_CACHE_BEGIN', 1)[1].split('// STEFX_MDR_SKIN_CACHE_END', 1)[0]
prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _XBOX 1
#define STEFX_SP_HOSTED_MP 1
extern "C" volatile unsigned int g_SPXBMainLoopCount = 1;
struct cvar_t { int integer; }; cvar_t enabled={1}, verify={1};
cvar_t *Cvar_Get(const char*n,const char*,int) { return !strcmp(n,"r_efMdrSkinCache")?&enabled:&verify; }
void XBLog_WriteCriticalf(const char*,...) {}
void XBLog_WriteCritical(const char*) {}
bool failAllocation;
void *GetProcessHeap() { return 0; }
void *HeapAlloc(void*,int,size_t n) { return failAllocation?0:malloc(n); }
struct md4Surface_t { int numVerts; };
struct Entity { struct { int frame, oldframe; } e; } entity;
struct { struct {int stefxSplitView;}viewParms; Entity *currentEntity; } backEnd;
struct {int frameCount;}tr;
struct { float xyz[8194][4], normal[8194][4], texCoords[8194][2][2]; } tess;
'''
suffix = r'''
int main() {
 backEnd.currentEntity=&entity;backEnd.viewParms.stefxSplitView=1;
 md4Surface_t surface={3},other={3};bool hit;int palette,paletteOther;
 failAllocation=true;assert(!R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7));
 failAllocation=false;s_mdrSkinTried=false;
 enabled.integer=0;assert(!R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7));
 enabled.integer=1;backEnd.viewParms.stefxSplitView=0;assert(!R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7));
 backEnd.viewParms.stefxSplitView=1;
 stefxMdrSkinKey_t*k=R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7);assert(k&&!hit);
 unsigned int bits[8]={0x80000000,0x3f800001,0xbf800000,0,0x3eaaaaab,0x7f7fffff,0x00000001,0x3f800000};
 for(int i=0;i<3;++i){memcpy(tess.xyz[i+1],bits,12);memcpy(tess.normal[i+1],bits+3,12);memcpy(tess.texCoords[i+1][0],bits+6,8);}
 R_STEFX_StoreOrVerifyMdrSkin(k,1,false,0);
 assert(R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7)==k&&hit);
 memset(&tess,0xa5,sizeof(tess));R_STEFX_CopyMdrSkin(k,1);
 for(int i=0;i<3;++i){assert(!memcmp(tess.xyz[i+1],bits,12));assert(!memcmp(tess.normal[i+1],bits+3,12));assert(!memcmp(tess.texCoords[i+1][0],bits+6,8));assert(*(unsigned int*)&tess.xyz[i+1][3]==0xa5a5a5a5);assert(*(unsigned int*)&tess.normal[i+1][3]==0xa5a5a5a5);assert(*(unsigned int*)&tess.texCoords[i+1][1][0]==0xa5a5a5a5);}
 assert(*(unsigned int*)&tess.xyz[0][0]==0xa5a5a5a5&&*(unsigned int*)&tess.xyz[4][0]==0xa5a5a5a5);
 R_STEFX_StoreOrVerifyMdrSkin(k,1,true,0);assert(g_SPXBMdrSkinCache[5]==3&&!g_SPXBMdrSkinCache[6]);
 assert(R_STEFX_ClaimMdrSkin(&other,.25f,&hit,&palette,7)&&!hit);
 ++entity.e.frame;assert(R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7)&&!hit);
 ++entity.e.oldframe;assert(R_STEFX_ClaimMdrSkin(&surface,.25f,&hit,&palette,7)&&!hit);
 assert(R_STEFX_ClaimMdrSkin(&surface,.5f,&hit,&palette,7)&&!hit);
 assert(R_STEFX_ClaimMdrSkin(&surface,.5f,&hit,&palette,8)&&!hit);
 assert(R_STEFX_ClaimMdrSkin(&surface,.5f,&hit,&paletteOther,7)&&!hit);
 ++g_SPXBMainLoopCount;assert(R_STEFX_ClaimMdrSkin(&surface,.5f,&hit,&palette,7)&&!hit&&s_mdrSkinKeyCount==1);
 ++tr.frameCount;assert(R_STEFX_ClaimMdrSkin(&surface,.5f,&hit,&palette,7)&&!hit&&s_mdrSkinKeyCount==1);
 ++tr.frameCount;surface.numVerts=8192;assert(R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7)&&!hit);assert(!R_STEFX_ClaimMdrSkin(&other,0,&hit,&palette,7));assert(s_mdrSkinUsed==8192);
 ++tr.frameCount;surface.numVerts=8193;assert(!R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7));assert(s_mdrSkinUsed==0);
 surface.numVerts=0;assert(!R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7));
 surface.numVerts=1;for(int n=0;n<256;++n){entity.e.frame=n;assert(R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7)&&!hit);}entity.e.frame=256;assert(!R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7));
 ++tr.frameCount;k=R_STEFX_ClaimMdrSkin(&surface,0,&hit,&palette,7);R_STEFX_StoreOrVerifyMdrSkin(k,1,false,0);tess.xyz[1][0]=42;R_STEFX_StoreOrVerifyMdrSkin(k,1,true,0);assert(!enabled.integer&&g_SPXBMdrSkinCache[6]==1);
 free(s_mdrSkinVertices);
 puts("MDR skin cache: exact bits, untouched padding, pose/frame keys, allocation failure, both capacity bounds and mismatch disabling passed.");
}
'''
out = root/'build/research/letterbox_perf'
cpp,exe = out/'mdr_skin_cache_test.cpp',out/'mdr_skin_cache_test.exe'
cpp.write_text(prefix+helper+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'mdr_skin_cache_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
