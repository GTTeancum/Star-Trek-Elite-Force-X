"""Compile the production co-op cache; exercise exact identity and fallback."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
helper=(root/'code/renderer/stefx_coop_mdr_skin.h').read_text()
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
#define MD4_MAX_BONES 128
extern "C" volatile unsigned int g_SPXBMainLoopCount=1;
struct cvar_t { int integer; const char *string; } mode={0,"0"},split={1,"1"},players={2,"2"},game={0,"coop"};
cvar_t *Cvar_Get(const char*n,const char*d,int) {
 if(!strcmp(n,"r_efCoopMdrSkin")){assert(!strcmp(d,"0"));return &mode;}
 if(!strcmp(n,"stefx_splitScreen"))return &split;
 if(!strcmp(n,"stefx_splitScreenPlayers"))return &players;
 return &game;
}
int Q_stricmp(const char*a,const char*b){return stricmp(a,b);}
void XBLog_WriteCriticalf(const char*,...){}
bool failAllocation;int allocations,frees;
void*GetProcessHeap(){return 0;}
void*HeapAlloc(void*,int,size_t n){++allocations;return failAllocation?0:malloc(n);}
void HeapFree(void*,int,void*p){++frees;free(p);}
struct md4Surface_t {int numVerts;};struct md4Header_t{int numBones;};
struct md4Bone_t{float matrix[3][4];};
struct Entity{struct{int frame,oldframe;}e;}entity;
struct{struct{int stefxSplitView;}viewParms;Entity*currentEntity;}backEnd;
struct{int frameCount;}tr;
struct{float xyz[8194][4],normal[8194][4],texCoords[8194][2][2];}tess;
'''
suffix=r'''
int main(){
 backEnd.currentEntity=&entity;backEnd.viewParms.stefxSplitView=1;
 md4Surface_t surface={3},other={3};md4Header_t header={2},otherHeader={2};
 md4Bone_t palette[128]={0},otherPalette[128]={0};bool hit;
 assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit)&&!allocations);
 mode.integer=1;game.string="holomatch";assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 game.string="coop";players.integer=1;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 players.integer=2;split.integer=0;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 split.integer=1;backEnd.viewParms.stefxSplitView=0;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 backEnd.viewParms.stefxSplitView=1;assert(!allocations);
 failAllocation=true;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 failAllocation=false;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit)&&allocations==1);
 R_STEFX_CoopSkinShutdown();
 coopSkinKey_t*k=R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit);assert(k&&!hit);
 unsigned int bits[8]={0x80000000,0x3f800001,0xbf800000,0,0x3eaaaaab,0x7f7fffff,1,0x3f800000};
 for(int trial=0;trial<20000;++trial){
  ++tr.frameCount;k=R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit);assert(k&&!hit);
  bits[1]=bits[1]*1664525u+1013904223u;
  for(int i=0;i<3;++i){memcpy(tess.xyz[i+1],bits,12);memcpy(tess.normal[i+1],bits+3,12);memcpy(tess.texCoords[i+1][0],bits+6,8);}
  R_STEFX_StoreOrVerifyCoopSkin(k,1,false);
  assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,otherPalette,&hit)==k&&hit);
  memset(&tess,0xa5,sizeof(tess));R_STEFX_CopyCoopSkin(k,1);
  for(int i=0;i<3;++i){assert(!memcmp(tess.xyz[i+1],bits,12));assert(!memcmp(tess.normal[i+1],bits+3,12));assert(!memcmp(tess.texCoords[i+1][0],bits+6,8));assert(*(unsigned int*)&tess.xyz[i+1][3]==0xa5a5a5a5);assert(*(unsigned int*)&tess.normal[i+1][3]==0xa5a5a5a5);assert(*(unsigned int*)&tess.texCoords[i+1][1][0]==0xa5a5a5a5);}
  assert(*(unsigned int*)&tess.xyz[0][0]==0xa5a5a5a5&&*(unsigned int*)&tess.xyz[4][0]==0xa5a5a5a5);
  R_STEFX_StoreOrVerifyCoopSkin(k,1,true);assert(!g_SPXBCoopMdrSkin[8]);
 }
 // Same pointer and pose labels do not hide modified palette bytes.
 palette[1].matrix[2][3]=1;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit)&&!hit);
 assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,otherPalette,&hit)==k&&hit);
 assert(R_STEFX_ClaimCoopSkin(&surface,&otherHeader,0,otherPalette,&hit)&&!hit);
 assert(R_STEFX_ClaimCoopSkin(&other,&header,0,otherPalette,&hit)&&!hit);
 ++entity.e.frame;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,otherPalette,&hit)&&!hit);
 ++entity.e.oldframe;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,otherPalette,&hit)&&!hit);
 assert(R_STEFX_ClaimCoopSkin(&surface,&header,.5f,otherPalette,&hit)&&!hit);
 ++g_SPXBMainLoopCount;assert(R_STEFX_ClaimCoopSkin(&surface,&header,.5f,palette,&hit)&&!hit&&s_coopSkinKeys==1);
 ++tr.frameCount;surface.numVerts=8192;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));assert(!R_STEFX_ClaimCoopSkin(&other,&header,0,palette,&hit));
 ++tr.frameCount;surface.numVerts=8193;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit)&&!s_coopSkinVerts);
 ++tr.frameCount;surface.numVerts=1;header.numBones=1;
 for(int n=0;n<128;++n){entity.e.frame=n;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));}
 entity.e.frame=128;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 ++tr.frameCount;header.numBones=128;
 for(int n=0;n<10;++n){entity.e.frame=n;assert(R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));}
 entity.e.frame=10;assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit)&&s_coopSkinBones==1280);
 ++tr.frameCount;header.numBones=1;md4Surface_t surfaces[257];
 for(int n=0;n<257;++n){surfaces[n].numVerts=1;coopSkinKey_t*q=R_STEFX_ClaimCoopSkin(&surfaces[n],&header,0,palette,&hit);assert((q!=0)==(n<256));}
 mode.integer=0;R_STEFX_CoopSkinBeginFrame();assert(!s_coopSkin&&!g_SPXBCoopMdrSkin[9]&&frees==1);
 mode.integer=2;R_STEFX_CoopSkinBeginFrame();k=R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit);
 R_STEFX_StoreOrVerifyCoopSkin(k,1,false);tess.xyz[1][0]=42;R_STEFX_StoreOrVerifyCoopSkin(k,1,true);
 assert(s_coopSkinFailed&&g_SPXBCoopMdrSkin[8]==1&&tess.xyz[1][0]==42);
 R_STEFX_CoopSkinShutdown();assert(!R_STEFX_ClaimCoopSkin(&surface,&header,0,palette,&hit));
 printf("Co-op skin cache: 20000 exact-output comparisons, full palette identity, guards, frame lifetimes, allocation failure, four bounds, cleanup and failure latch passed. bytes=%u\n",sizeof(coopSkinStorage_t));
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_mdr_skin_test.cpp';exe=out/'coop_mdr_skin_test.exe'
cpp.write_text(prefix+helper+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_mdr_skin_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
