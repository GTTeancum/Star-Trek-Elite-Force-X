"""Conservative bounds tests against generated geometry, including legacy overshoot."""
from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[2]
header=(root/'code/renderer/retail_xbox/stefx_fx_bounds.h').read_text()
core=header[:header.index('static bool STEFX_FxCullEnabled')]+ '\n#endif\n'
types=sorted(set(re.findall(r'\bRT_[A-Z_]+\b',core)))
prefix=r'''
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define _XBOX 1
#define STEFX_SP_HOSTED_MP 1
#define VectorCopy(a,b) memcpy(b,a,12)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VectorMA(a,s,b,c) do{for(int vm=0;vm<3;++vm)(c)[vm]=(a)[vm]+(s)*(b)[vm];}while(0)
float VectorLength(const float*a){return sqrtf(DotProduct(a,a));}
float Distance(const float*a,const float*b){float d[3];for(int i=0;i<3;++i)d[i]=a[i]-b[i];return VectorLength(d);}
struct refEntity_t{int reType;float origin[3],oldorigin[3],axis[3][3],radius,backlerp;
 struct{struct{float width;}line;struct{float radius;}sprite;
 struct{float width,width2,height;}cylinder;struct{float width,deviation;}electricity;}stefxData;};
struct cplane_t{float normal[3],dist;};
'''+ 'enum {'+','.join(types)+'};\n'
suffix=r'''
void inside(const float b[2][3],const float*p,float w){for(int a=0;a<3;++a){assert(p[a]-w>=b[0][a]-0.001f);assert(p[a]+w<=b[1][a]+0.001f);}}
int main(){refEntity_t e;memset(&e,0,sizeof(e));float b[2][3];
 cplane_t planes[4]={{{1,0,0},0},{{-1,0,0},-100},{{0,1,0},-50},{{0,-1,0},-50}};
 e.reType=RT_TEXTURED_LINE;e.origin[0]=-200;e.oldorigin[0]=200;e.radius=10;
 assert(STEFX_FxBounds(e,b)&&!STEFX_FxOutsidePlanes(b,planes)); // off-screen ends, visible middle
 e.origin[1]=e.oldorigin[1]=100;assert(STEFX_FxBounds(e,b)&&STEFX_FxOutsidePlanes(b,planes));
 e.origin[1]=e.oldorigin[1]=54;assert(STEFX_FxBounds(e,b)&&!STEFX_FxOutsidePlanes(b,planes)); // wide edge grazes screen
 // Test every segment center emitted by the actual 33-step Bezier loop.
 e.reType=RT_BEZIER;e.radius=12;
 for(int trial=0;trial<80;++trial){for(int a=0;a<3;++a){e.origin[a]=(trial*13+a*43)%301-150;
 e.oldorigin[a]=(trial*31+a*29)%307-150;e.axis[0][a]=(trial*17+a*59)%809-400;e.axis[1][a]=(trial*47+a*19)%811-400;}
 assert(STEFX_FxBounds(e,b));for(int s=0;s<=33;++s){double t=s/32.0,u=1-t;float p[3];
 for(int a=0;a<3;++a)p[a]=(float)(e.origin[a]*u*u*u+3*e.axis[0][a]*t*u*u+3*e.axis[1][a]*t*t*u+e.oldorigin[a]*t*t*t);
 inside(b,p,6);}}
 // Worst signed random walks in each axis of the sixteen-segment EF bolt.
 e.reType=RT_EF_ELECTRICITY;e.stefxData.electricity.width=8;e.stefxData.electricity.deviation=-2.5f;
 assert(STEFX_FxBounds(e,b));
 for(int signs=0;signs<8;++signs){float p[3];VectorCopy(e.origin,p);float len=Distance(e.origin,e.oldorigin);
 for(int s=0;s<16;++s){for(int a=0;a<3;++a){if(s==15)p[a]=e.oldorigin[a];else{if(s==14)p[a]=(p[a]+e.oldorigin[a])*0.5f;
 p[a]+=(e.oldorigin[a]-e.origin[a]+((signs&(1<<a))?1:-1)*e.stefxData.electricity.deviation*len*(a==2?0.5f:1.0f))/16.0f;}}inside(b,p,4);}}
 e.reType=RT_EF_ORIENTED_SPRITE;e.radius=0;e.stefxData.sprite.radius=50;
 assert(STEFX_FxBounds(e,b));assert(b[1][0]-b[0][0]>100); // uses the actual sprite union, not generic radius
 puts("FX bounds: crossing segments, offscreen rejection, width grazing, all 33 Bezier steps, worst-case electricity walks and oriented sprite dimensions passed.");}
'''
out=root/'build/research/letterbox_perf';cpp=out/'fx_bounds_test.cpp';cpp.write_text(prefix+core+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC');exe=out/'fx_bounds_test.exe'
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'fx_bounds_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(r.stdout);(out/'fx_bounds_test.txt').write_text(r.stdout)
