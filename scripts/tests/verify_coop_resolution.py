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
typedef int qboolean;
const int qtrue=1, qfalse=0, CVAR_ARCHIVE=1;
struct cvar_t { int integer; } enabled, leftVar, topVar, rightVar, bottomVar;
int split=1,players=2;
const char *mode="coop";
struct Config { int vidWidth,vidHeight; } glConfig={640,480};
struct State { bool isWidescreen; } state={true}, *glw_state=&state;
cvar_t *s_stefxSafeAreaLeft,*s_stefxSafeAreaTop,*s_stefxSafeAreaRight,*s_stefxSafeAreaBottom;
cvar_t *Cvar_Get(const char *n,const char *,int) {
 if(!strcmp(n,"r_efCoopLowRes")) return &enabled;
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
body = '\n'.join(function(n) for n in ['STEFX_CoopLowResolution(void)',
                  'STEFX_SafeAreaClamp(int', 'STEFX_GetSafeArea(int', 'GLW_GetPixelAspect(void)'])
suffix = r'''
int main() {
 int l,t,r,b;
 enabled.integer=0;
 if(STEFX_CoopLowResolution() || fabs(GLW_GetPixelAspect()-4.0/3.0)>0.0001) return 1;
 enabled.integer=1;
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
 if(!STEFX_CoopLowResolution() || fabs(GLW_GetPixelAspect()-1.0)>0.0001) return 2;
 STEFX_GetSafeArea(&l,&t,&r,&b);
 if(l!=80 || r!=80 || t || b) return 3;
 mode="holomatch";
 if(STEFX_CoopLowResolution()) return 4;
 mode="coop"; players=1;
 if(STEFX_CoopLowResolution()) return 5;
 players=3;
 if(STEFX_CoopLowResolution()) return 6;
 players=2; split=0;
 if(STEFX_CoopLowResolution()) return 7;
 split=1; enabled.integer=2;
 if(STEFX_CoopLowResolution()) return 8;
 enabled.integer=1; state.isWidescreen=false;
 STEFX_GetSafeArea(&l,&t,&r,&b);
 if(l || r || t || b || GLW_GetPixelAspect()!=1.0f) return 9;
#else
 if(STEFX_CoopLowResolution()) return 10;
 STEFX_GetSafeArea(&l,&t,&r,&b);
 if(l || r || t || b) return 11;
#endif
 enabled.integer=0; leftVar.integer=12; rightVar.integer=20;
 STEFX_GetSafeArea(&l,&t,&r,&b);
 if(l!=12 || r!=20) return 12;
 return 0;
}
'''
out = root / 'build/research/audit_stabilization_20260917'
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
print('PASS: production co-op guard, aspect, pillarbox, and ordinary SP/MP isolation')
