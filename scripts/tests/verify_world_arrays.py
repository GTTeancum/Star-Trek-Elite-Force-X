"""Check actual immutable triangle packing and native DRAW_ARRAYS packets."""
import ast
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
tree = ast.parse((root/'scripts/tests/verify_world_vertices.py').read_text())
prefix = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
              and any(isinstance(t, ast.Name) and t.id == 'prefix' for t in n.targets))
source = (root/'code/win32/win_qgl_dx8.cpp').read_text(encoding='utf-8')
core = source.split('// STEFX_WORLD_VERTICES_BEGIN',1)[1].split('// STEFX_WORLD_VERTICES_END',1)[0]
core = source[source.index('bool g_stefxWorldBasePass ='):source.index('bool g_stefxWorldBasePass =')+len('bool g_stefxWorldBasePass = false;')] + '\n' + core
suffix = r'''
shader_t shader={1,0,0,"array-test"};shaderStage_t stage;
static DWORD commands[4096];
void fill(int first,int count,int value){for(int v=0;v<count;++v){
 for(int j=0;j<4;++j){tess.xyz[first+v][j]=value+v+j*0.25f;tess.normal[first+v][j]=value-v-j*0.5f;}
 tess.svars.colors[first+v]=0x80010203+value+v;
 for(int b=0;b<2;++b)for(int j=0;j<2;++j)tess.svars.texcoords[b][first+v][j]=value+b+v+j*0.25f;
}}
void batch(const void*a,const void*b=0){STEFX_WorldVerticesBeginBatch();g_stefxWorldBasePass=true;tess.numVertexes=0;
 STEFX_WorldVerticesSurface(a,4);tess.numVertexes=4;
 if(b){STEFX_WorldVerticesSurface(b,4);tess.numVertexes=8;}}
void checkPacket(int count,const GLushort*indices,int n,int t0,int t1){
 memset(commands,0xa5,sizeof(commands));DWORD*end=STEFX_WorldArraysWrite(commands,5);
 assert(commands[0]==D3DPUSH_ENCODE(0x17fc,1)&&commands[1]==5);
 assert(commands[2]==D3DPUSH_ENCODE(0x40001810,s_worldArrayCommandCount));
 assert(end-commands==s_worldArrayCommandCount+3);
 assert(end-commands+29+2<=s_worldArrayCommandCount+60); // streams and END fit reservation
 int emitted=0;
 for(DWORD*p=commands+3;p<end;++p){unsigned int first=*p&0xffffff,vertices=(*p>>24)+1;
  assert(vertices<=255&&vertices%3==0&&first+vertices<=s_worldVertexUsed);
  for(unsigned int v=0;v<vertices;++v){
   assert(emitted<count);stefxWorldVertex_t expected;
   STEFX_WorldPackVertex(expected,indices[emitted],n,t0,t1);
   expected.pad=s_worldVertices[first+v].pad;
   assert(!memcmp(&expected,&s_worldVertices[first+v],sizeof(expected)));++emitted;
  }
 }
 assert(emitted==count&&*end==0xa5a5a5a5);
}
int main(){
 tess.shader=&shader;tess.xstages=&stage;stage.active=1;stage.rgbGen=CGEN_IDENTITY;stage.alphaGen=AGEN_SKIP;
 stage.bundle[0].tcGen=TCGEN_TEXTURE;stage.bundle[1].tcGen=TCGEN_LIGHTMAP;
 backEnd.currentEntity=&tr.worldEntity;backEnd.viewParms.stefxSplitView=1;tr.identityLightByte=255;
 int a,b,c;GLushort ab[]={0,1,2,0,2,3,4,6,5,4,7,6},ba[]={0,2,1,0,3,2,4,5,6,4,6,7};
 for(int mask=0;mask<8;++mask){
  enabled.integer=2;int n=mask&1,t0=mask&2,t1=mask&4;
  batch(&a,&b);fill(0,4,10);fill(4,4,20);
  assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,12,ab,n,t0,t1));assert(s_worldArraysActive&&s_worldStorageMode==2);
  assert(s_worldVertexUsed==12&&g_SPXBWorldArrays[5]==12);checkPacket(12,ab,n,t0,t1);
  unsigned int firstA=s_worldArrayRuns[0].vertexFirst,firstB=s_worldArrayRuns[1].vertexFirst;
  batch(&b,&a);fill(0,4,20);fill(4,4,10);
  assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,12,ba,n,t0,t1));checkPacket(12,ba,n,t0,t1);
  assert(s_worldVertexUsed==12&&s_worldArrayRuns[0].vertexFirst==firstB&&s_worldArrayRuns[1].vertexFirst==firstA);
  enabled.integer=1;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,12,ba,n,t0,t1));enabled.integer=2;
  s_worldVertexUsed=STEFX_WORLD_ARRAY_CAPACITY-3;
  batch(&c);fill(0,4,30);assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ab,n,t0,t1));
  assert(g_SPXBWorldArrays[4]==1&&!s_worldArraysActive);
  batch(&a);fill(0,4,10);assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ab,n,t0,t1));
  // Same vertex data, different topology must fail the byte verifier.
  GLushort changed[]={0,2,1,0,2,3};assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,6,changed,n,t0,t1));
  assert(g_SPXBWorldArrays[6]==1&&!enabled.integer&&!s_worldArraysActive);
  STEFX_WorldVerticesReset();assert(!live&&!s_worldVertexUsed&&!s_worldStorageMode);
 }
 enabled.integer=2;batch(&a,&b);fill(0,8,10);GLushort mixed[]={0,1,4};
 assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,mixed,0,0,0));assert(!live);
 GLushort bad[]={0,1,8};assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3,bad,0,0,0));
 assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,2,ab,0,0,0));
 GLushort repeated[]={0,1,2,4,5,6,0,2,3};assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,9,repeated,0,0,0));
 tess.numVertexes=9;assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,12,ab,0,0,0));
 STEFX_WorldVerticesReset();batch(&a);failAllocation=1;
 assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,6,ab,0,0,0));assert(!live);
 STEFX_WorldVerticesReset();failAllocation=0;
 STEFX_WorldVerticesBeginBatch();g_stefxWorldBasePass=true;tess.numVertexes=0;STEFX_WorldVerticesSurface(&c,3);tess.numVertexes=3;fill(0,3,7);
 GLushort large[6000];for(int i=0;i<6000;++i)large[i]=i%3;
 assert(STEFX_WorldVerticesClaim(GL_TRIANGLES,6000,large,1,1,1));assert(s_worldArrayCommandCount==24);
 checkPacket(6000,large,1,1,1);STEFX_WorldVerticesReset();
 int owners[1000];GLushort fragmented[3000];STEFX_WorldVerticesBeginBatch();g_stefxWorldBasePass=true;tess.numVertexes=0;
 for(int i=0;i<1000;++i){STEFX_WorldVerticesSurface(&owners[i],1);++tess.numVertexes;
  fragmented[i*3]=fragmented[i*3+1]=fragmented[i*3+2]=i;}
 assert(!STEFX_WorldVerticesClaim(GL_TRIANGLES,3000,fragmented,0,0,0));assert(!live);
 puts("World arrays: all formats, exact triangle order, reordered visibility, bounded 255-vertex packets, topology verification, mixed/cross-surface/index guards, capacity/allocation fallback and fenced reset passed.");
}
'''
out = root/'build/research/letterbox_perf'
cpp, exe = out/'world_arrays_test.cpp', out/'world_arrays_test.exe'
cpp.write_text(prefix+core+suffix, encoding='utf-8')
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
               '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'world_arrays_test.obj'),
               '/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
print(result.stdout)
(out/'world_arrays_test.txt').write_text(result.stdout)
