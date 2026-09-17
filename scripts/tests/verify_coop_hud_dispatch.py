"""Execute the real HUD dispatch with draw spies; catch the unbraced else bug."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'SP-Mod-Source-Code-master/cgame/cg_draw.cpp').read_text()
start = source.index('#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)',
                     source.index('// draw status bar and other floating elements'))
end = source.index('#ifdef _XBOX', start + 1)
dispatch = source[start:end]
prefix = r'''
struct Cvar { int integer; } cg_stefxSplitScreen, cg_stefxSplitScreenPlayers,
    cg_stefxSplitScreenP2Entity;
struct Snap { struct { int clientNum; } ps; } snap;
struct { Snap *snap; } cg;
struct { struct { int vidWidth,vidHeight; } glconfig; } cgs;
int g_SPXBPhaseLast;
int draws, fullDraws, topDraws, bottomDraws, viewport;
void XBLF(const char*,...) {}
void CG_STEFX_SetSplitHudViewport(float x,float y,float w,float h) {
    viewport = y == 0 ? 1 : 2;
}
void CG_STEFX_ClearSplitHudViewport() { viewport = 0; }
void CG_Draw2D() {
    ++draws;
    if (viewport==1) ++topDraws;
    else if (viewport==2) ++bottomDraws;
    else ++fullDraws;
}
void Draw() {
'''
suffix = r'''
}
int Check(int split,int players,int expectedSplit) {
    cg_stefxSplitScreen.integer=split;
    cg_stefxSplitScreenPlayers.integer=players;
    draws=fullDraws=topDraws=bottomDraws=0;
    Draw();
    if(viewport!=0) return 1;
    if(expectedSplit) return draws!=2 || fullDraws!=0 || topDraws!=1 || bottomDraws!=1;
    return draws!=1 || fullDraws!=1 || topDraws!=0 || bottomDraws!=0;
}
int main() {
    cg.snap=&snap;cgs.glconfig.vidWidth=1280;cgs.glconfig.vidHeight=720;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
    if(Check(1,2,1)) return 1;
    // Returning to SP must not retain P2's viewport.
    if(Check(0,2,0) || Check(1,1,0) || Check(1,2,1)) return 2;
    cgs.glconfig.vidWidth=cgs.glconfig.vidHeight=0;
    if(Check(1,2,1)) return 3;
#else
    if(Check(1,2,0) || Check(0,1,0)) return 4;
#endif
    return 0;
}
'''
out = root / 'build/research/letterbox_perf'
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')

def run(name, body, defines):
    cpp = out / ('coop_hud_route_' + name + '.cpp')
    exe = cpp.with_suffix('.exe')
    cpp.write_text(prefix + body + suffix)
    subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe', '/nologo', '/MT',
        '/I'+str(vc/'include'), *defines, str(cpp),
        '/Fo'+str(cpp.with_suffix('.obj')), '/Fe'+str(exe), '/link',
        '/LIBPATH:'+str(vc/'lib'), '/LIBPATH:'+str(vc/'PlatformSDK/Lib')], check=True)
    return subprocess.run([str(exe)]).returncode

for name, defines in [('sp', ['/D_XBOX', '/DSTEFX_ELITE_FORCE_SP']),
                      ('xbox_other', ['/D_XBOX']), ('desktop', [])]:
    assert run(name, dispatch, defines) == 0, name

# Demonstrate that the test catches the exact pre-fix third draw, without an
# assertion dialog or touching the running emulator.
else_start = dispatch.index('\telse\n#endif\n')
old = dispatch[:else_start] + '''\telse
#endif
    g_SPXBPhaseLast = 0x45473230;
    CG_Draw2D();
    g_SPXBPhaseLast = 0x45473231;
'''
assert run('old_regression', old, ['/D_XBOX', '/DSTEFX_ELITE_FORCE_SP']) == 1
print('HUD dispatch passed: two split draws, one single-view draw, viewport reset; original third-draw regression reproduced.')
