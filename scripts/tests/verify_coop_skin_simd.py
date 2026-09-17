"""Compile the production SIMD kernel against the original Xbox arithmetic."""
from pathlib import Path
import subprocess
root=Path.cwd();out=root/'build/research/letterbox_perf'
source=(root/'code/renderer/tr_animation.cpp').read_text()
loop="\tfor ( j = 0; j < numVerts; j++ ) {"+source.split("\tfor ( j = 0; j < numVerts; j++ ) {",1)[1].split("#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HW_FRAME_DIAGNOSTICS)",1)[0]+"\t\tv = (md4Vertex_t *)&v->weights[v->numWeights];\n\t}\n"
loop="\tv = (md4Vertex_t *) ((byte *)surface + surface->ofsVerts);\n"+loop
dot=(root/'code/game/q_shared.h').read_text().split('inline vec_t DotProduct( const vec3_t v1, const vec3_t v2 ) {',1)[1].split('\n#ifdef _XBOX\ninline vec_t DotProduct',1)[0]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "C:/XDK_5558/XDK/xbox/include/xmmintrin.h"
#define _XBOX 1
typedef unsigned char byte;typedef float vec_t;typedef float vec3_t[3];
#define VectorClear(a) ((a)[0]=(a)[1]=(a)[2]=0)
struct md4Bone_t{float matrix[3][4];};
struct md4Weight_t{int boneIndex;float boneWeight,offset[3];};
struct md4Vertex_t{float normal[3],texCoords[2];int numWeights;md4Weight_t weights[1];};
struct md4Surface_t{int numVerts,ofsVerts;};
struct Tess{float xyz[64][4],normal[64][4],texCoords[64][2][2];}tess,reference;
inline vec_t DotProduct(const vec3_t v1,const vec3_t v2){
'''+dot+r'''
void Original(md4Surface_t*surface,md4Bone_t*bonePtr,int baseVertex){
 int j,k,numVerts=surface->numVerts;md4Vertex_t*v;md4Bone_t*bone;
'''+loop+'}\n'
candidate=source.split('// STEFX_COOP_SKIN_SIMD_KERNEL_BEGIN',1)[1].split('// STEFX_COOP_SKIN_SIMD_KERNEL_END',1)[0]+r'''
void Candidate(md4Surface_t*surface,md4Bone_t*bonePtr,int baseVertex){
 md4Vertex_t*v=(md4Vertex_t*)((byte*)surface+surface->ofsVerts);
 for(int j=0;j<surface->numVerts;++j){
  R_STEFX_SkinVertexSimd(v,bonePtr,tess.xyz[baseVertex+j],tess.normal[baseVertex+j]);
  memcpy(tess.texCoords[baseVertex+j][0],v->texCoords,8);
  v=(md4Vertex_t*)&v->weights[v->numWeights];
 }
}
'''

test=r'''
static unsigned int rng=1234567;
unsigned int Random(){rng=rng*1664525u+1013904223u;return rng;}
float Value(float scale){return ((int)(Random()%20001)-10000)*scale/10000.0f;}
int main(){
 byte storage[65536];md4Surface_t*surface=(md4Surface_t*)storage;byte boneStorage[16*sizeof(md4Bone_t)+16];md4Bone_t*bones=(md4Bone_t*)(boneStorage+4);
 surface->numVerts=64;surface->ofsVerts=sizeof(*surface);
 for(int trial=0;trial<2000;++trial){
  for(int b=0;b<16;++b)for(int r=0;r<3;++r)for(int c=0;c<4;++c)bones[b].matrix[r][c]=Value(c==3?100.0f:3.0f);
  md4Vertex_t*v=(md4Vertex_t*)(storage+surface->ofsVerts);
  for(int j=0;j<64;++j){
   for(int c=0;c<3;++c)v->normal[c]=Value(1.0f);
   v->texCoords[0]=Value(1.0f);v->texCoords[1]=Value(1.0f);v->numWeights=Random()%7;
   for(int k=0;k<v->numWeights;++k){v->weights[k].boneIndex=Random()%16;v->weights[k].boneWeight=(Random()%10001)/10000.0f;for(int c=0;c<3;++c)v->weights[k].offset[c]=Value(50.0f);}
   v=(md4Vertex_t*)&v->weights[v->numWeights];
  }
  memset(&tess,0xa5,sizeof(tess));Original(surface,bones,0);memcpy(&reference,&tess,sizeof(tess));
  memset(&tess,0xa5,sizeof(tess));Candidate(surface,bones,0);
  if(memcmp(&tess,&reference,sizeof(tess))){printf("MISMATCH trial=%d\n",trial);return 1;}
 }
 puts("Production SIMD host test: 128000 vertices matched original loop bit-for-bit, including UVs and untouched padding. No Xbox/FPS claim.");
}
'''
guards=r'''
#define STEFX_HW_FRAME_DIAGNOSTICS 1
struct cvar_t{int integer;const char*string;};
static cvar_t testMode={0,"0"},testSplit={0,"0"},testPlayers={1,"1"},testGame={0,"coop"};
struct Backend{struct View{int stefxSplitView;}viewParms;}backEnd;
static unsigned int g_SPXBCoopMdrSimd[16];static bool s_coopMdrSimdFailed;
cvar_t*Cvar_Get(const char*n,const char*,int){
 if(!strcmp(n,"r_efCoopMdrSimd"))return &testMode;
 if(!strcmp(n,"stefx_splitScreen"))return &testSplit;
 if(!strcmp(n,"stefx_splitScreenPlayers"))return &testPlayers;
 assert(!strcmp(n,"stefx_splitScreenMode"));return &testGame;
}
#define Q_stricmp _stricmp
void XBLog_WriteCriticalf(const char*,...){}
'''+"static int R_STEFX_CoopMdrSimdMode(void) {"+source.split("static int R_STEFX_CoopMdrSimdMode(void) {",1)[1].split("// STEFX_COOP_SKIN_SIMD_KERNEL_BEGIN",1)[0]
prefix+=guards
test=test.replace(' byte storage[65536];',r'''
 assert(R_STEFX_CoopMdrSimdMode()==0);
 testMode.integer=1;assert(R_STEFX_CoopMdrSimdMode()==0);
 testSplit.integer=1;assert(R_STEFX_CoopMdrSimdMode()==0);
 testPlayers.integer=2;assert(R_STEFX_CoopMdrSimdMode()==0);
 backEnd.viewParms.stefxSplitView=1;assert(R_STEFX_CoopMdrSimdMode()==1);
 testGame.string="holomatch";assert(R_STEFX_CoopMdrSimdMode()==0);
 testGame.string="COOP";assert(R_STEFX_CoopMdrSimdMode()==1);
 testMode.integer=2;assert(R_STEFX_CoopMdrSimdMode()==2);
 s_coopMdrSimdFailed=true;assert(R_STEFX_CoopMdrSimdMode()==0);
 s_coopMdrSimdFailed=false;testMode.integer=3;assert(R_STEFX_CoopMdrSimdMode()==0);
 byte storage[65536];''')
cpp=out/'coop_skin_simd_production_test.cpp';exe=out/'coop_skin_simd_production_test.exe';cpp.write_text(prefix+candidate+test)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/O2','/arch:SSE','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_skin_simd_production_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
