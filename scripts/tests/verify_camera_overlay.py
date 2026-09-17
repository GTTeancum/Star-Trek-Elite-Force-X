"""Compile actual camera/renderer functions to check fade and pixel contracts."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/research/letterbox_perf"
OUT.mkdir(parents=True, exist_ok=True)


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index("\n}", start) + 2]


camera = (ROOT / "SP-Mod-Source-Code-master/cgame/cg_camera.cpp").read_text()
renderer = (ROOT / "code/renderer/retail_xbox/tr_backend_retail.cpp").read_text()
source = r'''
#include <stdio.h>
#include <string.h>
#include <math.h>
#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
#define BAR_DURATION 1000.0f
#define CAMERA_BAR_FADING 8
#define qtrue 1
#define qfalse 0
#define CA_ACTIVE 1
#define GL_PROJECTION 0
#define GL_MODELVIEW 1
#define GL_CULL_FACE 2
#define GL_CLIP_PLANE0 3
#define GLS_DEPTHTEST_DISABLE 1
#define GLS_SRCBLEND_SRC_ALPHA 2
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA 4
typedef int qboolean; typedef float vec4_t[4];
struct Camera {float bar_time,bar_alpha,bar_alpha_source,bar_alpha_dest;
 float bar_height,bar_height_source,bar_height_dest; int info_state;} client_camera;
struct Rect {int x,y,width,height;};
struct CG {int time; Rect refdef;} cg;
bool in_camera;int calls;float drawnHeight;
void XBLog_WriteCriticalf(const char*,...){}
void CG_FillRect2(float,float,float,float h,const float*){calls++;drawnHeight=h;}
void CGCam_DrawFades(){}
struct Backend {int projection2D;struct {int time;float floatTime;}refdef;}backEnd;
struct Config {int vidWidth,vidHeight;}glConfig;
struct Glw {int isWidescreen;}glw,*glw_state=&glw;
struct Client {int state;}cls;
struct Cvar {float value;}timescale,*com_timescale=&timescale;
bool menu;float orthoWidth,orthoHeight;
bool Menus_AnyFullScreenVisible(){return menu;}
void qglViewport(int,int,int,int){}void qglScissor(int,int,int,int){}
void qglMatrixMode(int){}void qglLoadIdentity(){}void qglDisable(int){}
void GL_State(int){}int Sys_Milliseconds(){return 0;}
void qglOrtho(float,float w,float,float h,float,float){orthoWidth=w;orthoHeight=h;}
'''
source += function(camera, "void CGCam_UpdateBarFade( void )") + "\n"
source += function(camera, "void CGCam_DrawWideScreen( void )") + "\n"
source += function(renderer, "void\tRB_SetGL2D (void)") + "\n"
source += r'''
int main(){int failures=0,checks=0;
 // A zero-alpha transition must still advance, even without camera updates.
 cg.refdef.width=640;cg.refdef.height=480;cg.time=500;
 client_camera.info_state=CAMERA_BAR_FADING;client_camera.bar_alpha_dest=1;
 client_camera.bar_height_dest=48;CGCam_DrawWideScreen();checks++;
 if(calls!=2 || fabs(drawnHeight-24)>0.01f)failures++;
 // Once the script disables the camera, draw owns the remaining fade.
 client_camera.bar_time=500;client_camera.bar_alpha_source=.5f;
 client_camera.bar_alpha_dest=0;client_camera.bar_height_source=24;
 client_camera.bar_height_dest=0;client_camera.info_state=CAMERA_BAR_FADING;
 cg.time=1000;calls=0;CGCam_DrawWideScreen();checks++;
 if(calls!=2 || fabs(drawnHeight-12)>0.01f)failures++;
 cg.time=1600;calls=0;CGCam_DrawWideScreen();checks++;
 if(calls || client_camera.bar_alpha || client_camera.bar_height)failures++;
 // Full pixel-space overlays cover every resolution, aspect, and quadrant.
 int sizes[][2]={{640,480},{720,480},{1280,720}};
 for(int i=0;i<3;i++)for(int wide=0;wide<2;wide++){
  glConfig.vidWidth=sizes[i][0];glConfig.vidHeight=sizes[i][1];
  glw.isWidescreen=wide;cls.state=CA_ACTIVE;menu=false;RB_SetGL2D();checks++;
  if(orthoWidth!=glConfig.vidWidth || orthoHeight!=glConfig.vidHeight)failures++;
  for(int slot=0;slot<4;slot++){float x=(slot%2)*glConfig.vidWidth*.5f;
   float w=glConfig.vidWidth*.5f;checks++;
   if(fabs((x+w)/orthoWidth*glConfig.vidWidth-(x+w))>.01f)failures++;}
  menu=true;RB_SetGL2D();checks++;
  if(orthoWidth!=640 || orthoHeight!=480)failures++;
 }
 printf("camera_overlay checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
'''
cpp = OUT / "camera_overlay_test.cpp"
exe = OUT / "camera_overlay_test.exe"
cpp.write_text(source)
vc = Path("C:/Program Files (x86)/Microsoft Visual Studio 8/VC")
command = ["C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe", "/nologo", "/MT", "/EHsc",
           "/I" + str(vc / "include"), str(cpp), "/Fo" + str(OUT / "camera_overlay_test.obj"),
           "/Fe" + str(exe), "/link", "/LIBPATH:" + str(vc / "lib"),
           "/LIBPATH:" + str(vc / "PlatformSDK/Lib")]
subprocess.run(command, check=True)
result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
(OUT / "camera_overlay_test.txt").write_text(result.stdout)
print(result.stdout)
old_camera = subprocess.check_output(
    ['git', 'show', 'HEAD:SP-Mod-Source-Code-master/cgame/cg_camera.cpp'],
    cwd=ROOT, text=True)
old_renderer = subprocess.check_output(
    ['git', 'show', 'HEAD:code/renderer/retail_xbox/tr_backend_retail.cpp'],
    cwd=ROOT, text=True)
negative = source.replace(function(camera, 'void CGCam_DrawWideScreen( void )'),
                          function(old_camera, 'void CGCam_DrawWideScreen( void )'))
negative = negative.replace(function(renderer, 'void\tRB_SetGL2D (void)'),
                            function(old_renderer, 'void\tRB_SetGL2D (void)'))
cpp.write_text(negative)
subprocess.run(command, check=True, capture_output=True)
result = subprocess.run([str(exe)], capture_output=True, text=True)
assert result.returncode != 0, 'Original implementation unexpectedly passed'
(OUT / 'camera_overlay_negative.txt').write_text(result.stdout)
print('Original implementation:', result.stdout)
cpp.write_text(source)
