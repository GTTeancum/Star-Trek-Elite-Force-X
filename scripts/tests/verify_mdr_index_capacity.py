"""Exercise the active MDR reservation against the actual overflow helper."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/research/letterbox_perf'
anim = (root / 'code/renderer/tr_animation.cpp').read_text()
reservation = re.search(r'RB_CheckOverflow\( surface->numVerts, surface->numTriangles[^;]+;', anim)[0]
surface = (root / 'code/renderer/retail_xbox/tr_surface_retail.cpp').read_text()
start = surface.index('void RB_CheckOverflow(')
helper = surface[start:surface.index('\n}', start)+2]
source = r'''
#include <stdio.h>
#define SHADER_MAX_VERTEXES 100
#define SHADER_MAX_INDEXES 120
#define ERR_DROP 1
#define STEFX_RETAIL_SCOPE
struct {int shader,numVertexes,numIndexes,fogNum;} tess;
struct {int shadowShader;} tr;
int flushes, errors;
void RB_EndSurface(){++flushes;tess.numVertexes=tess.numIndexes=0;}
void RB_BeginSurface(int,int){}
void Com_Error(int,const char*,...){++errors;}
''' + helper + r'''
struct Surface{int numVerts,numTriangles;};
int append(int existing,int triangles,int legacy){
 Surface data={3,triangles};Surface*surface=&data;
 tess.shader=1;tr.shadowShader=2;tess.numVertexes=3;
 tess.numIndexes=existing;flushes=errors=0;
 if(legacy) RB_CheckOverflow(surface->numVerts,surface->numTriangles);
 else {
''' + reservation + r'''
 }
 if(errors)return -1;
 tess.numIndexes+=surface->numTriangles*3;
 return tess.numIndexes;
}
int main(){int bad=0;
 bad+=append(115,2,0)!=6||flushes!=1;
 bad+=append(113,2,0)!=119||flushes!=0;
 bad+=append(114,2,0)!=6||flushes!=1;
 bad+=append(0,40,0)!=-1||errors!=1;
 bad+=append(115,2,1)!=121||flushes!=0; // Original overruns capacity.
 printf("MDR index capacity: 4 boundary cases and original negative control, failures=%d\n",bad);
 return bad;
}
'''
cpp = out / 'mdr_index_capacity.cpp'
cpp.write_text(source)
exe = out / 'mdr_index_capacity.exe'
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe', '/nologo', '/MT',
    '/I'+str(vc/'include'), str(cpp), '/Fo'+str(out/'mdr_index_capacity.obj'),
    '/Fe'+str(exe), '/link', '/LIBPATH:'+str(vc/'lib'),
    '/LIBPATH:'+str(vc/'PlatformSDK/Lib')], check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
(out/'mdr_index_capacity.txt').write_text(result.stdout)
print(result.stdout)
