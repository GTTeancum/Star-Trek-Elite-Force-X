"""Exercise actual packing and emitted GPU attributes for every enabled format."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root/'code/win32/win_qgl_dx8.cpp').read_text(encoding='utf-8')
helper = source.split('// STEFX_INTERLEAVED_VERTICES_BEGIN',1)[1].split('// STEFX_INTERLEAVED_VERTICES_END',1)[0]
packet = source.split('\t// Write the vertex shader\n#define CMD_STREAM_STRIDEANDTYPE0',1)[1].split('\n\t// Send thru the index data',1)[0]
packet = '\n#define CMD_STREAM_STRIDEANDTYPE0' + packet
prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long DWORD;
#define _XBOX 1
#define STEFX_SP_HOSTED_MP 1
#define SHADER_MAX_VERTEXES 1000
struct cvar_t {int integer;}; cvar_t enabled={1}, verify={1};
cvar_t*Cvar_Get(const char*n,const char*,int){return !strcmp(n,"r_efInterleavedVertices")?&enabled:&verify;}
struct {int projection2D;struct {int stefxSplitView;}viewParms;}backEnd;
struct {int numVertexes;float xyz[1000][4],normal[1000][4];
 struct {DWORD colors[1000];float texcoords[2][1000][2];}svars;}tess;
struct {int colorArrayState;DWORD currentColor;DWORD*drawArray;int drawStride;}state;
typedef void dummy; // no SDK or rendering device needed
#define glw_state (&state)
#define D3DPUSH_ENCODE(method,count) ((method)|((count)<<18))
#define D3DVSDT_FLOAT3 0x32
#define D3DVSDT_FLOAT2 0x22
#define D3DVSDT_D3DCOLOR 0x40
#define D3DVSDT_NONE 0
static DWORD storage[13016], commands[64];
'''
emit = r'''
void checkPacket(DWORD*payload,stefxPackedLayout_t l,int normals,int tex0,int tex1,int path){
 DWORD*worldPayload=path==1?payload:0;DWORD*residentPayload=worldPayload;int payloadVertices=tess.numVertexes;
 bool interleavedPayload=path==0;
 unsigned int interleavedStride=l.stride;
 DWORD*stream=payload;
 unsigned int xyzStride=worldPayload?48u:(interleavedPayload?interleavedStride:16u);
 unsigned int colorStride=worldPayload?48u:(interleavedPayload?interleavedStride:4u);
 unsigned int texStride=worldPayload?48u:(interleavedPayload?interleavedStride:8u);
 state.drawArray=commands;
'''
suffix = r'''
 assert(state.drawArray<=commands+60);
 unsigned int compactStride=path==1?48:l.stride;
 assert((commands[1]>>8)==(path==2?16:compactStride));
 assert((commands[4]>>8)==(path==2?4:compactStride));
 if(normals)assert((commands[3]>>8)==(path==2?16:compactStride));
 if(tex0)assert((commands[10]>>8)==(path==2?8:compactStride));
 if(tex1)assert((commands[11]>>8)==(path==2?8:compactStride));
 for(DWORD*p=commands+17;p<state.drawArray;p+=2){
  unsigned int offset=0,method=p[0]&0xffff;
  switch(method){
   case 0x1720:offset=0;break;
   case 0x1728:offset=path==2?tess.numVertexes*16:12;break;
   case 0x172c:offset=path==1?24:(path==2?tess.numVertexes*(16+(normals?16:0)):l.color);break;
   case 0x1744:offset=path==1?28:(path==2?tess.numVertexes*(20+(normals?16:0)):l.tex0);break;
   case 0x1748:offset=path==1?36:(path==2?tess.numVertexes*(20+(normals?16:0)+(tex0?8:0)):l.tex1);break;
   default:assert(false);
  }
  assert(p[1]==(((DWORD)payload+offset)&0x7fffffff));
 }
}
int main(){
 assert(sizeof(DWORD)==4);
 for(int v=0;v<1000;++v){
  for(int j=0;j<4;++j){DWORD x=0x3f800001+v*19+j;memcpy(&tess.xyz[v][j],&x,4);
   x=0xbf000001+v*7+j;memcpy(&tess.normal[v][j],&x,4);}
  tess.svars.colors[v]=0x12345678u+v;
  for(int b=0;b<2;++b)for(int j=0;j<2;++j){DWORD x=0x7f000001+v*3+b+j;memcpy(&tess.svars.texcoords[b][v][j],&x,4);}
 }
 DWORD zero=0x80000000,nan=0x7fc01234;
 memcpy(&tess.xyz[0][0],&zero,4);memcpy(&tess.xyz[0][1],&nan,4);
 int counts[]={0,1,3,999,1000};
 for(int mask=0;mask<8;++mask)for(int colors=0;colors<2;++colors)for(int ci=0;ci<5;++ci){
  int normals=mask&1,tex0=mask&2,tex1=mask&4;
  tess.numVertexes=counts[ci];state.colorArrayState=colors;state.currentColor=0x80abcdef;
  stefxPackedLayout_t l=STEFX_PackedLayout(normals,tex0,tex1);
  assert(l.stride%4==0&&l.stride>=16&&l.stride<=44);
  assert(l.stride==16+(normals?12:0)+(tex0?8:0)+(tex1?8:0));
  memset(storage,0xa5,sizeof(storage));DWORD*payload=storage+8;
  STEFX_PackInterleaved(payload,l,normals,tex0,tex1);
  assert(STEFX_VerifyInterleaved(payload,l,normals,tex0,tex1));
  for(int k=0;k<8;++k){assert(storage[k]==0xa5a5a5a5);assert(payload[l.stride/4*tess.numVertexes+k]==0xa5a5a5a5);}
  for(int v=0;v<tess.numVertexes;++v){
   const unsigned char*p=(const unsigned char*)payload+l.stride*v;
   assert(!memcmp(p,tess.xyz[v],12));
   if(normals)assert(!memcmp(p+12,tess.normal[v],12));
   DWORD color=colors?tess.svars.colors[v]:state.currentColor;
   assert(!memcmp(p+l.color,&color,4));
   if(tex0)assert(!memcmp(p+l.tex0,tess.svars.texcoords[0][v],8));
   if(tex1)assert(!memcmp(p+l.tex1,tess.svars.texcoords[1][v],8));
  }
  for(int path=0;path<3;++path)checkPacket(payload,l,normals,tex0,tex1,path);
  if(tess.numVertexes){payload[0]^=1;assert(!STEFX_VerifyInterleaved(payload,l,normals,tex0,tex1));}
 }
 backEnd.viewParms.stefxSplitView=1;tess.numVertexes=1000;assert(STEFX_InterleavedWanted());
 backEnd.projection2D=1;assert(!STEFX_InterleavedWanted());backEnd.projection2D=0;
 backEnd.viewParms.stefxSplitView=0;assert(!STEFX_InterleavedWanted());
 puts("Interleaved vertices: all 8 formats, array/constant color, edge counts, bit identity, guards, mismatch detection and actual GPU stream packets passed.");
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'interleaved_vertices_test.cpp';exe=out/'interleaved_vertices_test.exe'
cpp.write_text(prefix+helper+emit+packet+suffix,encoding='utf-8')
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'interleaved_vertices_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
