// Native frontend integration fixture. Use via verify_issue6_frontend.py.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <vector>
#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif
#include "platform.inc" // existing memory-backed file, signature and cvar boundaries
#include "settings.inc"
#include "client/keycodes.h"
typedef int qboolean, qhandle_t, sfxHandle_t, fileHandle_t;
typedef float vec4_t[4];
enum { qfalse=0, qtrue=1, UI_LEFT=0, UI_CENTER=2, UI_RIGHT=4, MAX_QPATH=64, KEYCATCH_UI=2,
       CHAN_LOCAL_SOUND=1, FS_READ=0, EXEC_APPEND=1, CT_WHITE, CT_BLACK, CT_DKPURPLE1, CT_LTPURPLE1, CT_LTGOLD1 };
#include "qcommon/stefx_launch.h"
static vec4_t colorTable[32];
static int slotCount, overwriteIndex, createCount, catcher;
static std::map<std::string, int> handles;
struct Quad { float x,y,w,h; int glyph,shader; };
static std::vector<Quad> draws;
static int Shader(const char *p) {
    if (!handles.count(p)) { int id=int(handles.size())+1; handles[p]=id; }
    return handles[p];
}
void Q_strncpyz(char *d,const char *s,size_t n) { snprintf(d,n,"%s",s); }
const char *va(const char *fmt,...) {
    static char s[1024]; va_list ap; va_start(ap,fmt); vsnprintf(s,sizeof(s),fmt,ap); va_end(ap); return s;
}
#define Com_sprintf snprintf
char *COM_ParseExt(const char**,qboolean) { abort(); }
#define XBLF(...) ((void)0)
void XBLog_Write(const char*) {}
volatile unsigned int g_SPXBUIPauseOpenCount, g_SPXBUIPauseActive;
extern "C" { volatile unsigned int g_SPXBDirectMapStatus; }
int CL_STEFX_SplitScreen_LocalSlotForPad(int p) { return p; }
int CL_STEFX_SplitScreen_PadForLocalSlot(int p) { return p; }
bool Sys_XboxQueueMenuMap(const char*,const char*,int) { abort(); }
void Sys_Reboot(const char*,const void*) { abort(); }
void UI_ForceMenuOff() { catcher=0; }
void UI_EFPauseMenu_Open(const char*); void UI_EFPauseMenu_Deactivate();
qboolean UI_EFPauseMenu_IsActive();
qboolean UI_EFQmenu_IsActive() { return qfalse; }
void UI_EFQmenu_ClearState(const char*) {}
qboolean UI_EFQmenu_ConsoleCommand(const char*);
int UI_EFSave_Count() { return slotCount; }
const char *UI_EFSave_Name(int i) { static char n[32]; CHECK(i>=0 && i<slotCount); snprintf(n,sizeof(n),"eforce%d",i); return n; }
const char *UI_EFSave_Comment(int i) {
    CHECK(i>=0 && i<slotCount);
    return i%2 ? "A LONG SAVED GAME TITLE OF 33 CHR" : "NORMAL";
}
const char *UI_EFSave_Map(int) { return "BORG1"; }
bool UI_EFSave_IsCorrupt(int) { return false; }
qboolean UI_EFSave_CreateNew(qboolean nativeMenuSubmission = qfalse) { assert(nativeMenuSubmission); ++createCount; return qtrue; }
qboolean UI_EFSave_Overwrite(int i, qboolean nativeMenuSubmission = qfalse) { assert(nativeMenuSubmission); overwriteIndex=i; return qtrue; }
qboolean UI_EFSave_Load(int) { abort(); }
struct { int realtime; } uis;
struct UI {
    static void R_SetColor(const float*) {}
    static void R_DrawStretchPic(float x,float y,float w,float h,float s,float,float,float,int shader) {
        draws.push_back({x,y,w,h,int(s*256),shader});
    }
    static int R_RegisterShader(const char*s) { return Shader(s); }
    static int R_RegisterShaderNoMip(const char*s) { return Shader(s); }
    static int S_RegisterSound(const char*) { return 1; }
    static void S_StartLocalSound(int,int) {}
    static int FS_FOpenFile(const char*,int*f,int) { *f=0; return -1; }
    static void FS_FCloseFile(int) {} static void FS_Read(void*,int,int) { abort(); }
    static int FS_ReadFile(const char*,void**p) { *p=NULL; return -1; }
    static void FS_FreeFile(void*) {}
    static void Cvar_Set(const char*n,const char*v) { ::Cvar_Set(n,v); }
    static void Cvar_SetValue(const char*n,float v) { ::Cvar_SetValue(n,v); }
    static float Cvar_VariableValue(const char*n) { return ::Cvar_VariableValue(n); }
    static void Cvar_VariableStringBuffer(const char*n,char*d,size_t len) { Q_strncpyz(d,UI_Cvar_VariableString(n),len); }
    static int Key_GetCatcher() { return catcher; } static void Key_SetCatcher(int v) { catcher=v; }
    static void Printf(const char*,...) {} static void Cmd_ExecuteText(int,const char*) { abort(); }
    static qboolean SG_GameAllowedToSaveHere(qboolean) { return qtrue; }
} ui;
#include "frontend.inc"
void EFQ_KeepMainMenuDumb(const char*,const char*) { abort(); }
void EFQ_OpenConfirmation(qboolean) { abort(); }
#include "qmenu_command.inc"

namespace pause_ui {
enum { EF_MENU_BUTTON_MED_WIDTH=130, EF_MENU_BUTTON_MED_HEIGHT=18 };
int s_cursor,s_loggedTextDraws,s_activeSystem;
qboolean s_active,s_loggedDraw;
void EFPause_LoadTextTables() {} void EFPause_Cache() {} void EFPause_LoadPropFonts() {}
void EFPause_SetActiveSystem(int,int) {}
#include "pause_route.inc"
}
void UI_EFPauseMenu_Open(const char*s) { pause_ui::UI_EFPauseMenu_Open(s); }
void UI_EFPauseMenu_Deactivate() { pause_ui::UI_EFPauseMenu_Deactivate(); }
qboolean UI_EFPauseMenu_IsActive() { return pause_ui::UI_EFPauseMenu_IsActive(); }
void UI_EFPauseMenu_KeyEvent(int k,qboolean d) { pause_ui::UI_EFPauseMenu_KeyEvent(k,d); }
// Legacy/parser menu is neither owner on these routes.
struct menuDef_t { struct { int flags; } window; };
enum { WINDOW_IGNORE_ESCAPE=1 };
int Menu_Count() { return 0; } menuDef_t *Menu_GetFocused() { abort(); }
void UI_EFQmenu_KeyEvent(int,qboolean) { abort(); } void UpdateDemoTimer() { abort(); }
bool Menus_AnyFullScreenVisible() { abort(); } void Menus_CloseAll() { abort(); }
void Menu_HandleKey(menuDef_t*,int,qboolean) { abort(); }
void trap_Key_SetCatcher(int) { abort(); } int trap_Key_GetCatcher() { abort(); } void trap_Key_ClearStates() { abort(); }
#include "key_route.inc"

static void Tap(int key) { _UI_KeyEvent(key,qtrue); _UI_KeyEvent(key,qfalse); }
static void Draw() { draws.clear(); UI_EFMainMenu_Draw(1500); }
static void Font(int width) { // Identifiable glyph UVs and variable metrics; no game files.
    s_fonts.loaded=qtrue;
    for(int f=0;f<3;++f) for(int c=0;c<256;++c) {
        s_fonts.propMap[f][c][0]=c;
        s_fonts.propMap[f][c][2]=f==EF_FRONTEND_FONT_TINY ? 3 : width+(c%5);
    }
}
static std::vector<Quad> ByShader(int sh) {
    std::vector<Quad> result;
    for (const Quad &d : draws) if (d.shader==sh) result.push_back(d);
    return result;
}
static void CheckPrompt(const char*a,const char*b,float& ax,float& yx) {
    Draw();
    auto A=ByShader(s_assets.xboxA),Y=ByShader(s_assets.xboxY),rows=ByShader(s_assets.buttonRight);
    CHECK(A.size()==1 && Y.size()==1 && A[0].y==Y[0].y);
    ax=A[0].x; yx=Y[0].x;
    auto tiny=ByShader(s_fonts.shader[EF_FRONTEND_FONT_TINY]),big=ByShader(s_fonts.shader[EF_FRONTEND_FONT_BIG]);
    std::string footer; float lastA=0,lastB=0;
    for (const Quad &d : big) if (d.y>=A[0].y) {
        footer+=char(d.glyph); (d.x<yx ? lastA : lastB)=d.x+d.w;
    }
    CHECK(footer==std::string(":")+a+":"+b);
    CHECK(yx-lastA>=29.9f && ax>92 && lastB<548 && A[0].y+A[0].h<446);
    CHECK(rows.size()==size_t(std::min(slotCount+(s_screen==EF_SCREEN_SAVEGAME),6)));
    for (const Quad &row : rows) {
        float nameBottom=0,detailTop=1000;
        for(const Quad &d : big) if(d.y>=row.y && d.y<row.y+row.h) {
            nameBottom=std::max(nameBottom,d.y+d.h); CHECK(d.x+d.w<=row.x+row.w-8);
        }
        for(const Quad &d : tiny) if(d.y>=row.y && d.y<row.y+row.h) detailTop=std::min(detailTop,d.y);
        CHECK(nameBottom && detailTop<1000 && detailTop-nameBottom>=2);
        CHECK(A[0].y-(row.y+row.h)>=16);
    }
}
static void CheckSave(int width,int count) {
    slotCount=count; createCount=0; overwriteIndex=-1;
    UI_EFPauseMenu_Open("ingameMainMenu"); Tap(A_JOY7); Tap(A_JOY15); // Pause > Save Game
    CHECK(s_screen==EF_SCREEN_SAVEGAME && !UI_EFPauseMenu_IsActive() && s_cursor==0);
    Font(width);
    float ax,yx,nextA,nextY; CheckPrompt("Save","Back",ax,yx);
    if (!count) { Tap(A_JOY15); CHECK(createCount==1 && overwriteIndex==-1 && !s_active); return; }
    Tap(A_JOY5); Tap(A_JOY15); // wrap to final slot, arm
    CHECK(s_saveOverwritePending && s_cursor==count && overwriteIndex==-1);
    CheckPrompt("Overwrite","Cancel",nextA,nextY); CHECK(ax==nextA && yx==nextY);
    Tap(A_JOY13); CHECK(!s_saveOverwritePending && s_screen==EF_SCREEN_SAVEGAME && overwriteIndex==-1);
    CheckPrompt("Save","Back",nextA,nextY);
    Tap(A_JOY15); Tap(A_JOY15); CHECK(overwriteIndex==count-1 && !createCount && !s_active);
}
static void OpenController(bool fromMain) {
    if (fromMain) { UI_EFMainMenu_Open(); for(int i=0;i<4;++i) Tap(A_JOY7); Tap(A_JOY15); }
    CHECK(s_screen==EF_SCREEN_CONFIGURE); Tap(A_JOY7); Tap(A_JOY7); Tap(A_JOY15);
    CHECK(s_screen==EF_SCREEN_CONTROLLER);
}
static bool Shown(const char *s) {
    Draw(); std::string text;
    for(const Quad &d : ByShader(s_fonts.shader[EF_FRONTEND_FONT_BIG])) text+=char(d.glyph);
    return text.find(s)!=std::string::npos; // spaces don't emit glyphs
}
static void CheckControllerLayout(int width) {
    Font(width); Draw();
    float vibrationStart=0, vibrationEnd=0, invertStart=0, invertEnd=0;
    for(const Quad &d : ByShader(s_fonts.shader[EF_FRONTEND_FONT_BIG])) {
        if(d.y<405) continue;
        if(d.x>370) { if(!vibrationStart) vibrationStart=d.x; vibrationEnd=d.x+d.w; }
        else { if(!invertStart) invertStart=d.x; invertEnd=d.x+d.w; }
    }
    auto x=ByShader(s_assets.xboxX).back(), left=ByShader(s_assets.xboxDpad[2]).back(), right=ByShader(s_assets.xboxDpad[3]).back();
    CHECK(invertStart>=left.x+left.w+5 && invertEnd<=right.x-5);
    CHECK(vibrationStart>=x.x+x.w+4 && vibrationEnd<=560);
}
static void InstallSigned(const void *p, DWORD len) {
    XCALCSIG_SIGNATURE sig;
    CHECK(XBSettingsSignBytes(p,len,&sig) && len+sizeof(sig)<=sizeof(disk));
    memcpy(disk,p,len); memcpy(disk+len,&sig,sizeof(sig)); diskExists=true; diskSize=len+sizeof(sig);
}
static void CheckLegacy() {
    XBSettings prev; prev.version=SETTINGS_VERSION_CALIBRATION; prev.rumble[0]=0; prev.rumble[1]=1;
    InstallSigned(&prev,offsetof(XBSettings,textureFilter)); Cvar_SetValue("in_useRumble",1);
    Settings=XBSettings(); Settings.LoadEFPreferences();
    CHECK(Settings.version==SETTINGS_VERSION && Settings.rumble[1]==1 && Cvar_VariableValue("in_useRumble")==0);
    XBSettingsLegacy082877 old={}; old.version=SETTINGS_VERSION_LEGACY; old.rumble[0]=1; old.rumble[1]=0;
    InstallSigned(&old,sizeof(old)); Settings.LoadEFPreferences();
    CHECK(Settings.version==SETTINGS_VERSION && Settings.rumble[1]==0 && Cvar_VariableValue("in_useRumble")==1);
}
int main() {
    Cvar_SetValue("in_useRumble",1); Cvar_SetValue("m_pitch",-0.022f); Settings=XBSettings();
    OpenController(true); Font(11); int before=saves;
#ifdef STEFX_SP_HOSTED_MP
    CHECK(!Shown("VIBRATION:")); Settings.rumble[0]=0;
    Tap(A_JOY16); Tap(A_JOY15); CHECK(Settings.rumble[0]==0 && Cvar_VariableValue("in_useRumble")==1);
    Settings=XBSettings(); Settings.LoadEFPreferences(); CHECK(Settings.rumble[0]==0 && Cvar_VariableValue("in_useRumble")==1);
    puts("PASS: Holomatch ignores campaign vibration on menu accept and startup.");
#else
    CHECK(Shown("VIBRATION:ON")); Tap(A_JOY16); CHECK(Shown("VIBRATION:OFF"));
    for(int w=8;w<=18;w+=5) CheckControllerLayout(w);
    CHECK(Cvar_VariableValue("in_useRumble")==1 && saves==before); Tap(A_JOY13);
    OpenController(false); CHECK(Shown("VIBRATION:ON")); Tap(A_JOY16); Tap(A_JOY15);
    CHECK(Settings.rumble[0]==0 && Settings.rumble[1]==1 && Cvar_VariableValue("in_useRumble")==0 && saves==before+1);
    Cvar_SetValue("in_useRumble",1); Settings=XBSettings(); Settings.LoadEFPreferences();
    CHECK(Settings.rumble[0]==0 && Cvar_VariableValue("in_useRumble")==0 && Settings.rumble[1]==1);
    OpenController(true); CHECK(Shown("VIBRATION:OFF")); Tap(A_JOY16); Tap(A_JOY15);
    CHECK(Cvar_VariableValue("in_useRumble")==1 && Settings.rumble[0]==1 && Cvar_VariableValue("m_pitch")<0);
    CheckLegacy();
    for(int w=8;w<=18;w+=5) { CheckSave(w,0); CheckSave(w,1); CheckSave(w,5); CheckSave(w,10); }
    UI_EFPauseMenu_Open("ingameMainMenu"); Tap(A_JOY7); Tap(A_JOY7); Tap(A_JOY15);
    CHECK(s_screen==EF_SCREEN_LOADGAME); float ax,yx; CheckPrompt("Select","Back",ax,yx);
    puts("PASS: Main > Configure > Controller (X / Y / A, reload, re-enable, both legacy formats); Pause > Save Game (new / arm / Y / confirm) and Load; 0, 1, 5, 10 slots; 3 glyph profiles; 30 logical px between Overwrite and Y, >16 px above footer.");
#endif
}
