"""Fault-inject query allocation failure into the actual flare test function."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root/'build/research/audio_probe'
text = (root/'code/renderer/retail_xbox/tr_surface_retail.cpp').read_text()
start = text.index('static bool RB_TestZFlare(')
end = text.index('\n}', start) + 2
body = '#ifdef _XBOX\n' + text[start:end]
source = r'''
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#define _XBOX 1
#define STEFX_RETAIL_SCOPE
#define qfalse false
#define D3D_OK S_OK
#define D3DRS_ZWRITEENABLE 1
#define D3DRS_COLORWRITEENABLE 2
#define GLS_SRCBLEND_ONE 1
#define GLS_DSTBLEND_ONE 2
#define D3DTS_VIEW 0
#define D3DFVF_XYZ 0
#define D3DPT_POINTLIST 0
#define D3DVSDE_VERTEX 0
typedef float vec3_t[3];typedef float vec4_t[4];
struct srfFlare_t {int visible; unsigned char number;};
struct View {float projectionMatrix[16];int viewportWidth,viewportHeight;};
struct Backend {struct {float modelMatrix[16];} ori;View viewParms;} backEnd;
void R_TransformModelToClip(float*,float*,float*,float*,float*clip){clip[0]=clip[1]=clip[2]=0;clip[3]=1;}
void R_TransformClipToWindow(float*,View*,float*,float*w){w[0]=w[1]=5;}
void GL_State(int){} void STEFX_D3D8_SetVertexShaderTracked(int){}
void XBLog_WriteCriticalf(const char*,...){}
struct Stack {void* GetTop(){return 0;}} matrix;
struct Device {
 int submissions,reads,invalidReads;bool allocated;
 HRESULT GetVisibilityTestResult(int,UINT*r,void*){reads++;if(!allocated)invalidReads++;*r=1;return S_OK;}
 void GetRenderState(int,DWORD*r){*r=1;}void SetRenderState(int,DWORD){}
 void SetTransform(int,void*){}void BeginVisibilityTest(){}void Begin(int){}
 void SetVertexData4f(int,float,float,float,float){}void End(){}
 HRESULT EndVisibilityTest(int){submissions++;if(submissions<=3)return E_OUTOFMEMORY;allocated=true;return S_OK;}
} device;
struct glwstate_t {enum {MatrixMode_Model=0};Device*device;Stack*matrixStack[1];} glw;
glwstate_t *glw_state=&glw;
''' + body + r'''
int main(){glw.device=&device;glw.matrixStack[0]=&matrix;
 backEnd.viewParms.viewportWidth=640;backEnd.viewParms.viewportHeight=480;
 srfFlare_t flare;flare.visible=-1;flare.number=113;vec3_t point={0,0,0};
 for(int i=0;i<8;i++)RB_TestZFlare(point,&flare);
 printf("submissions=%d reads=%d invalid_reads=%d visible=%d\n",device.submissions,device.reads,device.invalidReads,flare.visible);
 return device.invalidReads || device.submissions!=8 || device.reads!=4 || flare.visible!=1;
}
'''
cpp=out/'flare_query_failure_test.cpp';cpp.write_text(source)
exe=out/'flare_query_failure_test.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
args=['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
      '/I'+str(vc/'include'),'/I'+str(vc/'PlatformSDK/include'),str(cpp),
      '/Fo'+str(out/'flare_query_failure_test.obj'),'/Fe'+str(exe),
      '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib'),'kernel32.lib']
subprocess.run(args,check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
(out/'flare_query_failure_test.txt').write_text(result.stdout)
print(result.stdout)
old=source.replace('\t\tsurf->visible = -1;', '\t\t/* Original path ignored submission failure. */')
assert old!=source
cpp.write_text(old)
subprocess.run(args,check=True)
negative=subprocess.run([str(exe)],capture_output=True,text=True)
assert negative.returncode==1 and 'invalid_reads=3' in negative.stdout, negative.stdout
with (out/'flare_query_failure_test.txt').open('a') as report:
    report.write('Original failure handling, expected failure: '+negative.stdout)
print('Original failure handling, expected failure: '+negative.stdout)
cpp.write_text(source)
subprocess.run(args,check=True)
