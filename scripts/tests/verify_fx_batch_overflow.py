"""Run the two actual generators across a tessellation flush boundary."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
def function(path,name):
    text=path.read_text();start=text.index('static void '+name+'(')
    opening=text.index('{',start);depth=1;end=opening+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
prefix=r'''
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define M_PI 3.14159265358979323846
typedef float vec3_t[3];
#define VectorMA(a,s,b,c) do{for(int q=0;q<3;++q)c[q]=a[q]+s*b[q];}while(0)
#define VectorScale(a,s,b) do{for(int q=0;q<3;++q)b[q]=a[q]*s;}while(0)
#define VectorSubtract(a,b,c) do{for(int q=0;q<3;++q)c[q]=a[q]-b[q];}while(0)
vec3_t vec3_origin={0,0,0};
typedef unsigned char color4ub_t[4];
struct refEntity_t{float origin[3],shaderRGBA_dummy;unsigned char shaderRGBA[4];
 struct{struct{float stscale;}line;struct{float radius,rotation;unsigned char vertRGBA[4][4];}sprite;}stefxData;};
struct entity{refEntity_t e;};entity ent;
struct{entity*currentEntity;struct{struct{float axis[3][3];}or;bool isMirror;}viewParms;}backEnd;
struct{int numVertexes,numIndexes;float xyz[8][4],texCoords[8][1][2];unsigned char vertexColors[8][4];unsigned short indexes[12];unsigned int guard;}tess;
int flushes;
void check(int v,int i){if(tess.numVertexes+v>=8||tess.numIndexes+i>=12){++flushes;tess.numVertexes=tess.numIndexes=0;}}
#define RB_CHECKOVERFLOW(v,i) check(v,i)
void RB_AddQuadStamp(float*,float*,float*,unsigned char*){check(4,6);tess.numVertexes+=4;tess.numIndexes+=6;}
'''
core=function(root/'code/renderer/retail_xbox/tr_surface_retail.cpp','DoLine_Oriented')+'\n'+function(root/'code/renderer/tr_surface.cpp','RB_SurfaceEFAlphaVertPoly')
suffix=r'''
int main(){backEnd.currentEntity=&ent;tess.guard=0x12345678;vec3_t start={0,0,0},end={1,0,0},up={0,1,0};
 tess.numVertexes=6;tess.numIndexes=9;DoLine_Oriented(start,end,up,1);
 assert(flushes==1&&tess.numVertexes==4&&tess.numIndexes==6&&tess.guard==0x12345678);
 for(int i=0;i<6;++i)assert(tess.indexes[i]<4);
 tess.numVertexes=6;tess.numIndexes=9;
 for(int v=0;v<4;++v)for(int c=0;c<4;++c)ent.e.stefxData.sprite.vertRGBA[v][c]=v*4+c+20;
 RB_SurfaceEFAlphaVertPoly();assert(flushes==2&&tess.numVertexes==4&&tess.guard==0x12345678);
 for(int v=0;v<4;++v)assert(!memcmp(tess.vertexColors[v],ent.e.stefxData.sprite.vertRGBA[v],4));
 puts("Expanded FX batching: oriented line and per-vertex-alpha sprite preserve bounds, indices and colors across a batch flush.");}
'''
out=root/'build/research/letterbox_perf';cpp=out/'fx_batch_overflow_test.cpp';cpp.write_text(prefix+core+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC');exe=out/'fx_batch_overflow_test.exe'
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'fx_batch_overflow_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'fx_batch_overflow_test.txt').write_text(r.stdout)
