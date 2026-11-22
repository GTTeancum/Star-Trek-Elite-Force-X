"""Execute production selection/aspect/border logic across SP and MP builds."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'code/win32/win_qgl_dx8.cpp').read_text()

def function(name):
    start = source.index(name)
    start = source.rfind('\n', 0, start) + 1
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <string.h>
#include <math.h>
typedef int qboolean, GLint, GLsizei;
void XBLF(const char *,...) {}
const int qtrue=1, qfalse=0, CVAR_ARCHIVE=1;
struct cvar_t { int integer; } leftVar, topVar, rightVar, bottomVar;
static bool s_xboxPillarbox;
static bool calibrating;
int UI_EFMainMenu_IsCalibratingScreen(){return calibrating;}
typedef unsigned int DWORD;
enum { XC_AV_PACK_HDTV=1, XC_VIDEO_FLAGS_WIDESCREEN=1, XC_VIDEO_FLAGS_HDTV_720p=2, XC_VIDEO_FLAGS_HDTV_480p=8 };
static DWORD flags,pack;
DWORD XGetVideoFlags(){return flags;}
DWORD XGetAVPack(){return pack;}
int split=1,players=2;
const char *mode="coop";
struct Config { int vidWidth,vidHeight; } glConfig={640,480};
struct State { bool isWidescreen; } state={true}, *glw_state=&state;
cvar_t *s_stefxSafeAreaLeft,*s_stefxSafeAreaTop,*s_stefxSafeAreaRight,*s_stefxSafeAreaBottom;
cvar_t *Cvar_Get(const char *n,const char *,int) {
 if(!strcmp(n,"stefx_safeAreaLeft")) return &leftVar;
 if(!strcmp(n,"stefx_safeAreaTop")) return &topVar;
 if(!strcmp(n,"stefx_safeAreaRight")) return &rightVar;
 return &bottomVar;
}
int Cvar_VariableIntegerValue(const char *n) {
 return !strcmp(n,"stefx_splitScreen") ? split : players;
}
const char *Cvar_VariableString(const char *) { return mode; }
int Q_stricmp(const char *a,const char *b) { return _stricmp(a,b); }
'''
display=(root/'code/win32/xbox_display.h').read_text().replace('#include <xtl.h>', '')
body = display+'\n'+'\n'.join(function(n) for n in ['STEFX_MultiplayerDisplayRequested(void)',
                  'STEFX_SafeAreaClamp(int', 'STEFX_GetSafeArea(int', 'STEFX_ApplySafeArea(GLint', 'GLW_GetPixelAspect(void)'])
suffix = r'''
int main() {
 int l,t,r,b; pack=XC_AV_PACK_HDTV;flags=10;
 XboxDisplayMode sp=XboxGetDisplayMode(),mp=XboxGetDisplayMode(true);
 if(sp.width!=1280 || sp.height!=720 || !sp.widescreen || !sp.progressive) return 1;
 if(mp.width!=640 || mp.height!=480 || mp.widescreen || !mp.progressive) return 2;
#if defined(STEFX_SP_HOSTED_MP)
 split=0;players=1;mode="holomatch"; if(!STEFX_MultiplayerDisplayRequested()) return 3;
#elif defined(STEFX_ELITE_FORCE_SP)
 if(!STEFX_MultiplayerDisplayRequested()) return 4;
 split=0;players=1;mode="sp";if(STEFX_MultiplayerDisplayRequested()) return 5;
 split=1;mode="holomatch";if(!STEFX_MultiplayerDisplayRequested()) return 6;
#else
 if(STEFX_MultiplayerDisplayRequested()) return 7;
#endif
 flags=11;mp=XboxGetDisplayMode(true);if(!mp.widescreen || mp.width!=640 || !mp.progressive) return 15;
 s_xboxPillarbox=true;state.isWidescreen=false;
 STEFX_GetSafeArea(&l,&t,&r,&b);if(l!=80 || r!=80 || t || b || GLW_GetPixelAspect()!=1) return 8;
 for(int i=0;i<4;++i) {
  int x=(i%2)*320,y=(i/2)*240,w=320,h=240;
  if(!STEFX_ApplySafeArea(x,y,w,h) || x!=80+(i%2)*240 || y!=(i/2)*240 || w!=240 || h!=240) return 16;
 }
 for(int j=0;j<2;++j) {
  int x=0,y=j*240,w=640,h=240;
  if(!STEFX_ApplySafeArea(x,y,w,h) || x!=80 || y!=j*240 || w!=480 || h!=240) return 17;
 }
 leftVar.integer=12;rightVar.integer=20;STEFX_GetSafeArea(&l,&t,&r,&b);if(l!=89 || r!=95) return 9;
#if defined(STEFX_ELITE_FORCE_SP)
 calibrating=true;STEFX_GetSafeArea(&l,&t,&r,&b);if(l!=80 || r!=80) return 10;
 calibrating=false;
#endif
 s_xboxPillarbox=false;STEFX_GetSafeArea(&l,&t,&r,&b);if(l!=12 || r!=20) return 11;
 flags=8;mp=XboxGetDisplayMode(true);if(mp.widescreen || !mp.progressive || mp.width!=640) return 12;
 flags=1;pack=0;mp=XboxGetDisplayMode(true);if(!mp.widescreen || mp.progressive || mp.height!=480) return 13;
 state.isWidescreen=true;glConfig.vidWidth=1280;glConfig.vidHeight=720;
 if(fabs(GLW_GetPixelAspect()-1.0)>0.0001) return 14;
 return 0;
}
'''
out = root / 'build/research/audit_stabilization_20260917'
out.mkdir(parents=True, exist_ok=True)
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
for name, defines in [('sp', ['/DSTEFX_ELITE_FORCE_SP']),
                      ('mp', ['/DSTEFX_ELITE_FORCE_SP','/DSTEFX_SP_HOSTED_MP']),
                      ('other', [])]:
    cpp = out / ('resolution_guard_'+name+'.cpp')
    cpp.write_text(prefix + body + suffix)
    exe = cpp.with_suffix('.exe')
    subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT',
        '/I'+str(vc/'include'), *defines, str(cpp), '/Fo'+str(cpp.with_suffix('.obj')),
        '/Fe'+str(exe), '/link','/LIBPATH:'+str(vc/'lib'),
        '/LIBPATH:'+str(vc/'PlatformSDK/Lib')], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: native 480 multiplayer, dashboard single-player mode, calibration, 4:3 and widescreen borders')
