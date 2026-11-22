"""Run the Xbox registry's lookup, restart and defragmentation code against colliding names."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/split-cvar-test"
VC = Path("C:/Program Files (x86)/Microsoft Visual Studio 8/VC")


def main():
    source = (ROOT / "code/qcommon/cvar.cpp").read_text(encoding="latin1")

    def section(start, stop):
        return source[source.index(start):source.index(stop, source.index(start))]

    registry = [
        section("cvar_t\t\t*cvar_vars;", "/*\n============\nCvar_VariableValue"),
        section("cvar_t *Cvar_Get( const", "#if defined(STEFX_SP_HOSTED_MP)\nvoid Cvar_SetNoModify"),
        section("void Cvar_Restart_f( void )", "/*\n=====================\nCvar_InfoString"),
        source[source.index("static void Cvar_Realloc("):],
    ]
    headers = r"""
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
typedef int qboolean;
enum {qfalse, qtrue, ERR_FATAL, TAG_SMALL};
enum {CVAR_USER_CREATED=1, CVAR_ROM=2, CVAR_INIT=4, CVAR_NORESTART=8, CVAR_LATCH=16, CVAR_CHEAT=32};
struct cvar_t {
 char *name,*string,*resetString,*latchedString;
 int modified,modificationCount,integer,flags;
 float value;
 cvar_t *next;
};
#define Q_stricmp _stricmp
#define CopyString _strdup
#define Z_Free free
#define Z_Malloc(n,t,c) malloc(n)
#define Com_Error(...) abort()
#define Com_Printf(...) ((void)0)
#define Com_DPrintf(...) ((void)0)
#define XBLog_Write(...) ((void)0)
static int activations;
static void XBLog_Print(const char*) { ++activations; }
"""
    checks = r"""
int main() {
 const char *split="stefx_splitScreen", *players="stefx_splitScreenPlayers";
 char names[5][32]; int found=0;
 for(int i=0;found<5;++i) {
  char name[32]; sprintf(name,"test_%d",i);
  if(Cvar_NameBucket(name)==42) strcpy(names[found++],name);
 }
 cvar_t *vars[5];
 for(int j=0;j<5;++j) {
  assert(!Cvar_FindVar(names[j]));
  vars[j]=Cvar_Get(names[j],"1",j%2 ? CVAR_NORESTART : CVAR_USER_CREATED);
 }
 Cvar_Get(split,"1",CVAR_USER_CREATED);
 Cvar_Get(players,"1",CVAR_USER_CREATED);
 assert(Cvar_FindVar(names[1])==vars[1] && activations==0);
 Cvar_Set(players,"3");
 assert(Cvar_FindVar(names[1])==vars[1] && activations==1);
 for(int j=0;j<5;++j) {
  char upper[32]; strcpy(upper,names[j]); _strupr(upper);
  assert(Cvar_Get(upper,"1",0)==vars[j]);
  vars[j]->flags=j%2 ? CVAR_NORESTART : CVAR_USER_CREATED;
 }
 assert(!Cvar_FindVar("unregistered"));
 cvar_t *late=Cvar_Get("UnRegistered","str",0);
 assert(Cvar_FindVar("UNREGISTERED")==late);
 cvar_t *longName=Cvar_Get("mixed_\xe9","value",0);
 assert(Cvar_FindVar("MIXED_\xe9")==longName);
 char *previous=late->name;
 Cvar_Defrag();
 assert(late->name!=previous && Cvar_FindVar("unregistered")==late);
 Cvar_Restart_f();
 assert(!cvar_splitScreen && !cvar_splitPlayers);
 Cvar_Get(split,"1",0); Cvar_Get(players,"4",0);
 for(int j=0;j<5;++j) {
  if(j%2) assert(Cvar_FindVar(names[j])==vars[j]);
  else {
   assert(!Cvar_FindVar(names[j]));
   cvar_t *fresh=Cvar_Get(names[j],"2",0);
   assert(fresh!=vars[j] && Cvar_FindVar(names[j])==fresh);
  }
 }
 Cvar_Set(split,"0"); g_SPXBSplitCvarIndexUsed=0;
 assert(Cvar_FindVar(names[3])==vars[3] && !g_SPXBSplitCvarIndexUsed);
 Cvar_Set(split,"1"); Cvar_Defrag();
 assert(Cvar_FindVar(names[3])==vars[3] && g_SPXBSplitCvarIndexUsed);
 int count=0;
 for(cvar_t *p=cvar_vars;p;p=p->next) {
  assert(Cvar_FindIndexed(p->name)==p && Cvar_FindLinear(p->name)==p);
  ++count;
 }
 assert(count==9);
 puts("PASS: collisions, case, misses, restart, re-registration, defrag and solo/split transitions");
}
"""
    OUT.mkdir(parents=True, exist_ok=True)
    cpp = OUT / "split_cvar.cpp"
    cpp.write_text("\n".join([headers, *registry, checks]), encoding="latin1")
    env = dict(os.environ, TEMP=str(OUT), TMP=str(OUT))
    env["PATH"] = os.pathsep.join(map(str, [VC / "bin", VC.parent / "Common7/IDE", env["PATH"]]))
    exe = cpp.with_suffix(".exe")
    subprocess.run([
        VC / "bin/cl.exe", "/nologo", "/O2", "/MT", "/I" + str(VC / "include"),
        cpp, "/Fo" + str(cpp.with_suffix(".obj")), "/Fe" + str(exe),
        "/link", "/LIBPATH:" + str(VC / "lib"), "/LIBPATH:" + str(VC / "PlatformSDK/Lib"),
    ], cwd=OUT, env=env, check=True)
    subprocess.run([exe], cwd=OUT, env=env, check=True)


if __name__ == "__main__":
    main()
