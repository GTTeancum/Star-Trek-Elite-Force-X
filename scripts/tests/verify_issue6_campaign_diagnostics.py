#!/usr/bin/env python3
"""Run the retail frame entry and NPC death probes with host stand-ins (not XDK/gameplay)."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
D3D = (ROOT / "code/win32/win_qgl_dx8.cpp").read_text()
COMBAT = (ROOT / "SP-Mod-Source-Code-master/game/g_combat.cpp").read_text()


def function(source, signature):
    start = source.index(signature)
    end = source.index("\n}", start)
    return source[start:end + 2]


HOST = r"""
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#define _XBOX
#define STEFX_ELITE_FORCE_SP
#define STEFX_RETAIL_RENDERER_ACTIVE
typedef unsigned int DWORD;
typedef bool GLboolean;
typedef int HRESULT;
enum { D3D_OK=0, D3DCLEAR_TARGET=1, MAX_ANIM_FILES=2, MAX_ANIMATIONS=3, ANIM_TOGGLEBIT=1024 };
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
struct D3DVIEWPORT8 { DWORD x,y,w,h; float z0,z1; };
struct D3DRECT { int x1,y1,x2,y2; };
struct Display { int vidWidth,vidHeight; } glConfig={1280,720};
struct cvar_t { int integer; } clearCV,deathCV;
static int clearRegs,deathRegs,margin;
static char line[512];
static int logs;
static void XBLog_WriteCriticalf(const char *format,...) {
  va_list ap; va_start(ap,format); vsnprintf(line,sizeof(line),format,ap); va_end(ap); ++logs;
}
static cvar_t *Cvar_Get(const char *name,const char *value,int flags) {
  assert(!strcmp(value,"0") && flags==0);
  if(!strcmp(name,"stefx_debugFrameClear")) { ++clearRegs;return &clearCV; }
  assert(!strcmp(name,"stefx_traceDeaths")); ++deathRegs;return &deathCV;
}
static void noopPrint(const char *,...) {}
struct GameImport { cvar_t *(*cvar)(const char *,const char *,int); void (*Printf)(const char *,...); } gi={ Cvar_Get,noopPrint };
static void STEFX_D3D8_InvalidateTextureStageCache() {}
static void STEFX_ScratchFrameBegin() {}
static void STEFX_GetSafeArea(int *l,int *t,int *r,int *b){*l=*t=*r=*b=margin;}
struct Device {
  D3DVIEWPORT8 vp;
  int beginResult,clearResult,gets,sets,black,markers; DWORD color;
  int BeginScene() {return beginResult;}
  int GetViewport(D3DVIEWPORT8 *o){++gets; *o=vp;return 0;}
  int SetViewport(const D3DVIEWPORT8 *i){++sets;vp=*i;return 0;}
  int Clear(int count,const D3DRECT *rect,DWORD flags,DWORD c,float z,int stencil){
    assert(flags==D3DCLEAR_TARGET && z==1 && !stencil);
    if(!count){assert(!rect && !c && margin);++black;return 0;}
    assert(count==1 && rect && !vp.x && !vp.y && vp.w==1280 && vp.h==720);
    assert(rect->x1==margin && rect->y1==margin && rect->x2==1280-margin && rect->y2==720-margin);
    color=c; ++markers;return clearResult;
  }
} dev;
struct GLW { Device *device; } world={&dev}, *glw_state=&world;
struct Level { char mapname[16]; int time; } level={"voy",123};
struct animation_t { int numFrames,frameLerp; };
struct AnimSet { animation_t animations[MAX_ANIMATIONS]; } knownAnimFileSets[MAX_ANIM_FILES];
int numKnownAnimFileSets=1;
struct Client {
  struct { int animFileIndex; } clientInfo;
  struct { char legsModelName[32],torsoModelName[32]; } renderInfo;
  struct { int legsAnim,torsoAnim,legsAnimTimer,torsoAnimTimer; } ps;
} client;
struct gentity_t {
  void *NPC; Client *client; struct { int number,legsAnim,torsoAnim,legsAnimTimer,torsoAnimTimer; } s; const char *NPC_type; float currentOrigin[3];
};
#define S_COLOR_RED ""
enum { SETANIM_TORSO=1, SETANIM_LEGS=2, SETANIM_BOTH=3,
       SETANIM_FLAG_OVERRIDE=1, SETANIM_FLAG_HOLD=2, SETANIM_FLAG_RESTART=4,
       BOTH_DEATH1=0, BOTH_DEATH7=MAX_ANIMATIONS-1 };
typedef int qboolean; static const int qfalse=0,qtrue=1;
static cvar_t debug,*g_ICARUSDebug=&debug;
struct AnimName { const char *name; } animTable[MAX_ANIMATIONS];
static int picks;
static int Q_irand(int lo,int hi){ return lo+(picks++%(hi-lo+1)); }
static void PM_SetLegsAnimTimer(gentity_t *,int *t,int value){ *t=value; }
static void PM_SetTorsoAnimTimer(gentity_t *,int *t,int value){ *t=value; }
static qboolean ValidAnimFileIndex(int i){ return i>=0 && i<MAX_ANIM_FILES; }
"""

RUN = r"""
int main() {
  const D3DVIEWPORT8 old={11,12,100,200,0.25f,0.75f}; dev.vp=old;
  assert(dllBeginFrame() && !dev.gets && !dev.sets && !dev.markers && !dev.black && !logs);
  margin=8; assert(dllBeginFrame() && dev.black==1 && !dev.markers);
  clearCV.integer=1; dev.beginResult=-1;
  assert(!dllBeginFrame() && dev.black==1 && !dev.markers);
  dev.beginResult=0; dev.clearResult=-1;
  assert(dllBeginFrame() && dev.black==2 && dev.markers==1 && !logs && !memcmp(&dev.vp,&old,sizeof old));
  dev.clearResult=0; assert(dllBeginFrame() && logs==1 && strstr(line,"mode=1") && dev.color==0xff00ffff);
  assert(dllBeginFrame() && logs==1 && dev.black==4);
  clearCV.integer=2; margin=0; assert(dllBeginFrame() && logs==2 && strstr(line,"mode=2") && dev.color==0xffffff00);
  assert(dev.gets==4 && dev.sets==8 && !memcmp(&dev.vp,&old,sizeof old));
  clearCV.integer=-1; assert(dllBeginFrame() && dev.gets==4 && clearRegs==1);

  gentity_t ent={0,&client,{17,0,0,0,0},"alien",{4,5,6}};
  logs=0; STEFX_TraceDeathPose(&ent,1); assert(!logs && !deathRegs);
  ent.NPC=&ent; STEFX_TraceDeathPose(&ent,1); assert(!logs && deathRegs==1);
  deathCV.integer=1; strcpy(client.renderInfo.legsModelName,"model");
  knownAnimFileSets[0].animations[1].numFrames=7;
  client.ps.legsAnim=ANIM_TOGGLEBIT|1; client.ps.torsoAnim=2;
  STEFX_TraceDeathPose(&ent,1);
  assert(logs==1 && strstr(line,"map=voy time=123 ent=17 npc=alien legsModel=model") &&
         strstr(line,"set=0 requested=1 frames=7 legs=1 torso=2 pos=4,5,6"));
  assert(client.ps.legsAnim==(ANIM_TOGGLEBIT|1) && client.ps.torsoAnim==2);
  STEFX_TraceDeathPose(&ent,2); assert(strstr(line,"frames=0"));
  STEFX_TraceDeathPose(&ent,MAX_ANIMATIONS); assert(strstr(line,"frames=-1"));
  client.clientInfo.animFileIndex=numKnownAnimFileSets; STEFX_TraceDeathPose(&ent,1); assert(strstr(line,"frames=-1"));
  client.clientInfo.animFileIndex=-1;ent.NPC_type=0; STEFX_TraceDeathPose(&ent,1); assert(strstr(line,"npc=-") && strstr(line,"frames=-1"));
  ent.client=0; STEFX_TraceDeathPose(&ent,1); assert(logs==5);
  ent.client=&client; client.clientInfo.animFileIndex=0;
  memset(knownAnimFileSets,0,sizeof knownAnimFileSets);
  client.ps.legsAnim=client.ps.torsoAnim=2;
  normalDeathSelection(&ent);
  assert(picks==1000 && strstr(line,"requested=0 frames=0 legs=2 torso=2"));
  knownAnimFileSets[0].animations[1].numFrames=7; knownAnimFileSets[0].animations[1].frameLerp=40;
  normalDeathSelection(&ent);
  assert(strstr(line,"requested=1 frames=7 legs=1 torso=1"));
  assert(client.ps.legsAnimTimer==280 && client.ps.torsoAnimTimer==280);
  for(int i=0;i<100;i++) STEFX_TraceDeathPose(&ent,1);
  assert(logs==32 && deathRegs==1);
}
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cxx", default=os.getenv("CXX", "c++"))
    ap.add_argument("--sanitize", action="store_true", help="run ASan and UBSan on the host fixture")
    args = ap.parse_args()
    # Hook ordering in the shipping source, not the similarly named JA game tree.
    build = (ROOT / "scripts/build_xbox.ps1").read_text()
    assert '"SP-Mod-Source-Code-master"' in build and '"game\\game.dsp"' in build
    assert "g_combat.cpp" in (ROOT / "SP-Mod-Source-Code-master/game/game.dsp").read_text()
    assert '"..\\win32\\win_qgl_dx8.cpp"' in build
    death = function(COMBAT, "void player_die(")
    assert death.index("NPC_SetAnim(self, SETANIM_BOTH, anim,") < death.index("STEFX_TraceDeathPose( self, anim )")
    retail_end = function((ROOT / "code/renderer/retail_xbox/tr_cmds_retail.cpp").read_text(), "void RE_EndFrame(")
    assert retail_end.index("qglBeginFrame()") < retail_end.index("R_IssueRenderCommands( qtrue )")
    probe = function(D3D, "static void STEFX_TraceUnpaintedPixels(")
    frame = function(D3D, "GLboolean dllBeginFrame(void)")
    trace = function(COMBAT, "static void STEFX_TraceDeathPose(")
    # Run the unmodified normal selection/apply/trace statements. Falling,
    # impulses, event delivery and scripts are deliberately outside this fixture.
    bg = (ROOT / "SP-Mod-Source-Code-master/game/bg_panimate.cpp").read_text()
    npc = (ROOT / "SP-Mod-Source-Code-master/game/NPC.cpp").read_text()
    path = [function(bg, sig) for sig in ("qboolean PM_HasAnimation(", "int PM_PickAnim(", "void PM_SetAnimFinal(")]
    path.append(function(npc, "void NPC_SetAnim(gentity_t\t*ent,int setAnimParts,int anim,int setAnimFlags)"))
    normal = ["void normalDeathSelection(gentity_t *self) { int anim;"]
    for start in ("anim = PM_PickAnim(", "NPC_SetAnim(self, SETANIM_BOTH, anim,", "STEFX_TraceDeathPose( self, anim )"):
        normal.append(death[death.index(start):].split(";", 1)[0] + ";")
    normal.append("}")

    with tempfile.TemporaryDirectory() as d:
        source, binary = Path(d)/"test.cpp", Path(d)/"test"
        flags = ["-std=c++98", "-Wall", "-Wextra", "-Werror"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
        source.write_text("\n".join([HOST, probe, frame, trace, *path, *normal, RUN]))
        subprocess.run([args.cxx, *flags, str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: campaign hooks, frame clear modes/state, rejected lookup, applied pose and 32-record cap")


if __name__ == "__main__":
    main()
