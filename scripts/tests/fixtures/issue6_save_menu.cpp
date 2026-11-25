// Normal save flows, driven from the real campaign Save Game screen and A handler.
// Only the ef-save-menu runner's temporary host harness uses these platform shims.
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <map>
#include <string>
#include <vector>
#include <strings.h>

#define _XBOX 1
using qboolean = int;
using byte = unsigned char;
using DWORD = unsigned long;
using HANDLE = void *;
using LPCSTR = const char *;
using LPCWSTR = const unsigned short *;
const int qtrue = 1, qfalse = 0, FALSE = 0, EXEC_APPEND = 2, ERR_FATAL = 1;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define S_COLOR_RED ""
#define stricmp strcasecmp
#define Q_stricmp strcasecmp
#define Com_sprintf snprintf
#define XBLF(...) ((void)0)
#define XBLog_WriteRingMarkerf(...) ((void)0)
#define XBLog_WriteRingMarker(...) ((void)0)
#define ZeroMemory(p,n) memset(p, 0, n)
void Com_Error(int, const char *, ...) { std::abort(); }
void Com_DPrintf(const char *, ...) {}
void Com_Printf(const char *, ...) {}
void Q_strncpyz(char *, const char *, int, qboolean = qfalse);
// GAME_STRING_COPY
void Q_strcat(char *p, size_t cap, const char *s) { assert(strlen(p)+strlen(s)<cap); strcat(p,s); }

// The release XDK uses 32-bit time_t and 16-bit wchar_t.
using XboxTime = int32_t;
static XboxTime clockTick = 1700000000;
static XboxTime xboxTime(XboxTime *v) { return *v = ++clockTick; }
static struct tm *xboxLocalTime(const XboxTime *p) { std::time_t t = *p; return std::localtime(&t); }
static size_t xboxMbstowcs(wchar_t *p, const char *s, size_t n) {
    unsigned short *dest = reinterpret_cast<unsigned short *>(p);
    size_t i = 0;
    for (; i < n && s[i]; ++i) dest[i] = (unsigned char)s[i];
    if (i < n) dest[i] = 0;
    return i;
}
#define time_t XboxTime
#define time xboxTime
#define localtime xboxLocalTime
#define mbstowcs xboxMbstowcs
// GAME_SIZES
static_assert(sizeof(savedata_t::currentSaveFileMap) >= iSG_MAPCMD_SIZE, "MPCM must fit");
static_assert(sizeof(savedata_t::currentSaveFileComments) >= iSG_COMMENT_SIZE, "COMM must fit");

using Bytes = std::vector<byte>;
using Chunks = std::map<int, Bytes>;
static std::map<std::string, Chunks> slots;
static std::map<std::string, std::string> cvars;
static std::vector<std::string> directory, checkpoints;
static std::string currentSlot, payloadPath, queued;
static int enumerators, acquired, released, payloadOpens, indexInDir, freeSpaceChecks;
enum { ERROR_SUCCESS=0, ERROR_FILE_NOT_FOUND=2, ERROR_NO_MORE_FILES=18, ERROR_NOT_READY=21 };
static DWORD lastSearchError;
static bool storageUnavailable;
DWORD GetLastError() { return lastSearchError; }
int Cvar_VariableIntegerValue(const char *k) { return atoi(cvars[k].c_str()); }
void Cvar_Set(const char *k,const char *v) { cvars[k]=v; }
void Cvar_SetValue(const char *k,int v) { cvars[k]=std::to_string(v); }
int trap_Cvar_VariableValue(const char *k) { return Cvar_VariableIntegerValue(k); }
void XBLog_WriteCriticalf(const char *fmt, ...) {
    va_list args; va_start(args, fmt); char line[256];
    vsnprintf(line,sizeof(line),fmt,args); va_end(args); checkpoints.push_back(line);
}
struct XGAME_FIND_DATA { wchar_t szSaveGameName[256]; };
static void getEntry(XGAME_FIND_DATA *row) {
    const std::string &n = directory.at(indexInDir);
    assert(n.size() < 256); std::copy(n.begin(),n.end(),row->szSaveGameName); row->szSaveGameName[n.size()]=0;
}
HANDLE XFindFirstSaveGame(const char *root,XGAME_FIND_DATA *row) {
    assert(std::string(root)=="U:\\"); if (storageUnavailable || directory.empty()) {
        lastSearchError = storageUnavailable ? ERROR_NOT_READY : ERROR_FILE_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }
    assert(!enumerators); ++acquired; ++enumerators; indexInDir=0; getEntry(row); return (HANDLE)1;
}
int XFindNextSaveGame(HANDLE h,XGAME_FIND_DATA *row) {
    assert(h==(HANDLE)1 && enumerators==1);
    if (++indexInDir >= int(directory.size())) { lastSearchError=ERROR_NO_MORE_FILES; return 0; }
    getEntry(row); return 1;
}
int XFindClose(HANDLE h) {
    assert(h==(HANDLE)1 && enumerators==1); --enumerators; ++released; lastSearchError=ERROR_SUCCESS; return 1;
}
unsigned long SG_BlocksLeft() { ++freeSpaceChecks; return 1042; }
int SG_SaveGameSize() { return 15; }
enum { OPEN_EXISTING=1, OPEN_ALWAYS, CREATE_ALWAYS, GENERIC_WRITE, GENERIC_READ, FILE_SHARE_READ,
       FILE_ATTRIBUTE_NORMAL, XCALCSIG_FLAG_SAVE_GAME, iSAVEGAME_VERSION=2 };
DWORD XCreateSaveGame(const char *root, LPCWSTR name,int disposition,int,char *out,size_t capacity) {
    assert(std::string(root)=="U:\\");
    std::string slot; for (; *name; ++name) slot.push_back((char)*name);
    if (disposition==OPEN_ALWAYS) {
        if (!slots.count(slot)) directory.push_back(slot);
        slots[slot];
    } else if (disposition!=OPEN_EXISTING || !slots.count(slot)) return 2;
    currentSlot=slot; snprintf(out,capacity,"U:\\%s\\",slot.c_str()); return ERROR_SUCCESS;
}
HANDLE CreateFile(const char *path,int access,int,int,int mode,int,int) {
    assert(std::string(path)=="U:\\"+currentSlot+"\\JK3SG.xsv");
    if (access==GENERIC_WRITE) { assert(mode==CREATE_ALWAYS); ++payloadOpens; slots[currentSlot].clear(); }
    else assert(access==GENERIC_READ && mode==OPEN_EXISTING);
    payloadPath=path; return (HANDLE)2;
}
static struct { DWORD dwFileLength, Signature; } sg_validationHeader,sg_validationHeaderRead;
static HANDLE sg_Handle,sg_sigHandle,sg_sigHandleRead;
static int sg_BufferSize,sg_CurrentBufferPos,fhSaveGame,giSaveGameVersion;
static bool bSavingCheckpoint,gbSGWriteFailed;
static qboolean gFullCompressionOn,g_WriteFieldBufferToFile,bypassFieldCompression,qbSGReadIsTestOnly;
const char *CHECK_POINT_STRING="Z:\\Checkpoint.xsv";
static char saveGameComment[iSG_COMMENT_SIZE];
DWORD XCalculateSignatureGetSize(int) { return sizeof(DWORD); }
HANDLE XCalculateSignatureBegin(int) { return (HANDLE)3; }
void XCalculateSignatureEnd(HANDLE h,DWORD *) { assert(h==(HANDLE)3); }
int CopyFile(const char *,const char *,int) { return 1; }
DWORD GetFileSize(HANDLE,void *) { return 100; }
int ReadFile(HANDLE,void *,DWORD n,DWORD *done,void *) { *done=n; sg_validationHeader.dwFileLength=100; return 1; }
void WriteFile(HANDLE,const void *,DWORD n,DWORD *done,void *) { *done=n; }
int SG_Close() { sg_Handle=nullptr; return qtrue; }
int SG_CloseWrite() { return SG_Close(); }
void SG_WipeSavegame(const char *) { assert(!"normal save should not wipe"); }
const char *GetString_FailedToOpenSaveGame(const char *,int) { return "unavailable"; }
void SG_Append(int id,const void *p,int n) {
    const byte *b=(const byte *)p; slots.at(currentSlot)[id]=Bytes(b,b+n);
}
int SG_Read(int id,void *p,int n) {
    const auto &data=slots.at(currentSlot).at(id); assert((int)data.size()==n);
    if (p) memcpy(p,data.data(),n); return n;
}
struct Cvar { int integer; const char *string; };
static Cvar skillTest={1,"1"},mapConfig={0,"borg1"},*sv_testsave=&skillTest,*sv_mapname=&mapConfig;
static struct { const char *configstrings[1]; int time, timeResidual; } sv={{""},1,0};
const int CS_SERVERINFO=0;
const char *Info_ValueForKey(const char *,const char *) { return sv_mapname->string; }
int Sys_Milliseconds() { static int ticks; return ticks+=1000; }
int SG_GameAllowedToSaveHere(int) { return 1; }
void SG_WriteCvars() {}
void WriteGame(int) {}
void CM_WritePortalState() {}
void SG_WriteServerConfigStrings() {}
struct MockGame { void WriteLevel(int) {} } world,*ge=&world;
byte *sg_FullBuffer;
void TempFree() {}
struct itemDef_t {};
qboolean Script_RunDeferred(itemDef_t *,const char **) { return qtrue; }
void ui_resetSaveGameList() {}
// GAME_SAVE_CODE

static savedata_t s_savedata[MAX_SAVELOADFILES];
static struct { char listBuf[LISTBUFSIZE]; int saveFileCnt, currentLine; } s_savegame;
static qboolean s_saveListIncomplete,ui_ShowDeleteActive;
const char *SE_GetString(const char *,const char *) { return "New Game"; }
void storeSGDataDatetoCvar() {}
void storeSGDataTimetoCvar() {}
void storeSGDataDiffLeveltoCvar() {}
struct MockUI {
    int (*SG_GetSaveGameComment)(const char *,char *,char *);
    void (*SG_StoreSaveGameComment)(const char *);
    void Cmd_ExecuteText(int mode,const char *s) { assert(mode==EXEC_APPEND && queued.empty()); queued=s; }
    void Printf(const char *,...) {}
    int Key_GetCatcher() { return 0; }
} ui={SG_GetSaveGameComment,SG_StoreSaveGameComment};
// GAME_MENU_CODE
enum efFrontendScreen_t { EF_SCREEN_AUDIO, EF_SCREEN_VIDEO, EF_SCREEN_CONTROLLER, EF_SCREEN_NEWGAME,
     EF_SCREEN_COOP_NEW, EF_SCREEN_LOADGAME, EF_SCREEN_SAVEGAME };
static efFrontendScreen_t s_screen;
static int s_cursor,s_saveCount,s_loadCount;
static qboolean s_loggedDraw,s_saveOverwritePending,s_active,s_returnToPause;
void EFFe_LoadAudioState() {}
void EFFe_LoadVideoState() {}
void EFFe_LoadControllerState() {}
void EFFe_SyncNewGameState() {}
const char *EFFe_ScreenName(int) { return "Save Game"; }
bool EFFe_IsBackKey(int k) { return k=='B'; }
bool EFFe_IsUpKey(int k) { return k=='U'; }
bool EFFe_IsDownKey(int k) { return k=='D'; }
bool EFFe_IsAcceptKey(int k) { return k=='A'; }
void EFFe_MoveCursor(int n,int d) { s_cursor=(s_cursor+d+n)%n; }
void EFFe_ReturnToPause() {}
void UI_ForceMenuOff() {}
// GAME_FRONTEND_CODE

static void enterSave() { EFFe_SetScreen(EF_SCREEN_SAVEGAME,0); assert(queued.empty()); }
static void dispatch(const std::string &name) {
    assert(queued=="save "+name+"\n"); queued.clear();
    assert(SG_WriteSavegame(name.c_str(),qfalse));
    assert(!sg_Handle && slots.at(name).at('COMM').size()==64 && slots.at(name).at('MPCM').size()==64);
    assert(slots.at(name).at('CMTM').size()==4);
}
static bool saw(const std::string &stage) {
    for (const auto &s:checkpoints) if (s.find(stage)!=std::string::npos) return true;
    return false;
}
int main() {
    Cvar_Set("g_spskill","1");
    enterSave(); assert(s_saveCount==0 && !acquired && !released && !payloadOpens);
    EFFe_HandleSaveGameKey('A'); dispatch("eforce0"); assert(checkpoints.empty());
    // Fresh Settings-only profile. Opening/refreshing the screen never opens a payload for write.
    directory.assign(1,"Settings"); slots.clear();
    Cvar_Set("stefx_saveTrace","1");
    enterSave(); assert(s_saveCount==0 && acquired==released && acquired==1 && payloadOpens==1);
    EFFe_HandleSaveGameKey('A'); dispatch("eforce0"); assert(acquired==released && acquired==2);
    assert(saw("operation=menu stage=list.begin") && saw("operation=menu stage=list.end"));
    assert(saw("operation=new-slot stage=list.begin") && saw("operation=new-slot stage=list.end"));
    assert(saw("operation=new-slot stage=space.begin") && saw("operation=new-slot stage=space.end"));
    assert(saw("operation=new stage=queue.begin") && saw("operation=new stage=queue.end"));
    for (auto stage:{"container","payload","serialize","close"}) {
        assert(saw(std::string("stage=")+stage+".begin") && saw(std::string("stage=")+stage+".end"));
    }
    assert(!slots.count("Settings") && directory.front()=="Settings");
    enterSave(); assert(s_saveCount==1 && std::string(UI_EFSave_Map(0))=="borg1");
    assert(std::string(UI_EFSave_Comment(0))=="NORMAL" && !s_savedata[0].corrupt);
    const Chunks first=slots.at("eforce0");
    EFFe_HandleSaveGameKey('A'); dispatch("eforce1"); assert(slots.at("eforce0")==first);
    enterSave(); assert(s_saveCount==2); EFFe_HandleSaveGameKey('D'); // newest first
    assert(std::string(UI_EFSave_Name(s_cursor-1))=="eforce1");
    const int scans=acquired; EFFe_HandleSaveGameKey('A'); assert(s_saveOverwritePending && queued.empty());
    EFFe_HandleSaveGameKey('A'); dispatch("eforce1"); assert(acquired==scans && acquired==released);
    assert(slots.at("eforce0")==first && slots.size()==2 && freeSpaceChecks>=5);
    // Ordinary user-supplied description and clearing it must preserve fixed-width records.
    SG_StoreSaveGameComment("Training"); SG_WriteComment(qfalse,"eforce1");
    assert(std::string((const char *)slots.at("eforce1").at('COMM').data())=="Training");
    SG_StoreSaveGameComment(""); SG_WriteComment(qfalse,"eforce1");
    const Bytes &comment=slots.at("eforce1").at('COMM');
    assert(comment.size()==64 && std::string((const char *)comment.data())=="@MENUS_JEDI");
    assert(std::all_of(comment.begin()+strlen("@MENUS_JEDI"),comment.end(),[](byte c){return c==0;}));
    // Storage can briefly be unavailable. Never infer that eforce0 is free on a failed search.
    const auto priorSlots=slots; storageUnavailable=true;
    enterSave(); EFFe_HandleSaveGameKey('A');
    assert(queued.empty() && slots==priorSlots && acquired==released);
    storageUnavailable=false;
    // A completely occupied, ordinary 100-slot directory must close the search and not overwrite.
    int stamp=1700000000; slots.clear(); directory.clear();
    for (int n=0;n<MAX_SAVELOADFILES;++n) {
        auto name="eforce"+std::to_string(n); directory.push_back(name); slots[name]=first;
        ++stamp; memcpy(slots[name]['CMTM'].data(),&stamp,sizeof(stamp));
    }
    enterSave(); assert(s_saveCount==MAX_SAVELOADFILES);
    EFFe_HandleSaveGameKey('A'); assert(queued.empty() && acquired==released);
    printf("PASS: real Save Game menu and A paths; clean/Settings-only first save, new slot, "
           "confirmed overwrite, unavailable storage, 100 occupied slots, metadata round trip, handles and opt-in stages.\n");
}
