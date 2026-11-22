// Stub only platform boundaries; production menu and settings functions are inserted.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <map>
#include <string>
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(23); } } while (0)
typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef int HANDLE;
typedef int BOOL;
struct XCALCSIG_SIGNATURE { DWORD words[5]; };
enum { INVALID_HANDLE_VALUE=-1, ERROR_SUCCESS=0, GENERIC_READ=1, GENERIC_WRITE=2,
       OPEN_ALWAYS=3, OPEN_EXISTING=4, FILE_ATTRIBUTE_NORMAL=0, FALSE=0 };
static BYTE disk[4096];
static DWORD diskSize, diskPos, sigHash;
static bool diskExists, failWrite;
static int saves;
HANDLE XCalculateSignatureBegin(int) { sigHash=2166136261UL; return 2; }
int XCalculateSignatureUpdate(HANDLE, BYTE *p, DWORD size) {
    for(DWORD i=0;i<size;++i) sigHash=(sigHash ^ p[i])*16777619UL;
    return 0;
}
int XCalculateSignatureEnd(HANDLE, XCALCSIG_SIGNATURE *s) {
    for(int i=0;i<5;++i) s->words[i]=sigHash+i;
    return 0;
}
int XCreateSaveGame(const char*,const wchar_t*,int mode,int,char *path,DWORD) {
    if(mode==OPEN_EXISTING && !diskExists) return 1;
    strcpy(path,"memory/"); return 0;
}
HANDLE CreateFile(const char*,int access,int,void*,int,int,void*) {
    if(access==GENERIC_READ && !diskExists) return INVALID_HANDLE_VALUE;
    if(access==GENERIC_WRITE) { diskExists=true; ++saves; }
    diskPos=0; return 1;
}
BOOL WriteFile(HANDLE,const void *data,DWORD n,DWORD *written,void*) {
    if(failWrite) return 0;
    CHECK(diskPos+n <= sizeof(disk)); memcpy(disk+diskPos,data,n);
    diskPos+=n; if(diskSize<diskPos) diskSize=diskPos; *written=n; return 1;
}
BOOL ReadFile(HANDLE,void *data,DWORD n,DWORD *read,void*) {
    if(diskPos+n>diskSize) return 0;
    memcpy(data,disk+diskPos,n); diskPos+=n; *read=n; return 1;
}
DWORD GetFileSize(HANDLE,void*) { return diskSize; }
void CloseHandle(HANDLE) {}
void SetEndOfFile(HANDLE) { diskSize=diskPos; }
BOOL CopyFile(const char*,const char*,int) { return 1; }
void XDeleteSaveGame(const char*,const wchar_t*) { diskExists=false; }
static std::map<std::string,std::string> cvars;
void Cvar_Set(const char *name,const char *value) { cvars[name]=value; }
void Cvar_SetValue(const char *name,float value) { char b[64]; sprintf(b,"%.9g",value); Cvar_Set(name,b); }
float Cvar_VariableValue(const char *name) { return (float)atof(cvars[name].c_str()); }
const char *UI_Cvar_VariableString(const char *name) { return cvars[name].c_str(); }
int Q_stricmp(const char *a,const char *b) { return _stricmp(a,b); }
void Com_Printf(const char*,...) {}
void GLimp_SetGamma(float) {}
void XBLog_WriteCritical(const char*) {}
void XBLog_WriteCriticalf(const char*,...) {}
void XBLF(const char*,...) {}

// SETTINGS_CODE

typedef int qboolean;
typedef int fakeAscii_t;
enum { qfalse=0, qtrue=1, CT_WHITE=0, EF_FRONTEND_FONT_BIG=0, EF_FRONTEND_FONT_MEDIUM=1,
       UI_CENTER=0, A_JOY5=5, A_JOY6, A_JOY7, A_JOY8, A_JOY9, A_JOY13=13, A_JOY14, A_JOY15, A_JOY16, LEFT=100, RIGHT=101,
       UP=102, DOWN=103, ACCEPT=104, BACK=105 };
const float EF_PS2_VIEW_W=1920, EF_PS2_VIEW_H=899;
float colorTable[1][4]={{1,1,1,1}}, s_ps2Gold[4]={1,1,0,1}, s_ps2MapGold[4]={1,1,0,1};
struct { int realtime; } uis;
struct UI {
    void (*Cvar_Set)(const char*,const char*);
    void (*Cvar_SetValue)(const char*,float);
    float (*Cvar_VariableValue)(const char*);
} ui={Cvar_Set,Cvar_SetValue,Cvar_VariableValue};
int EFFe_ClampInt(int v,int lo,int hi) { return v<lo?lo:(v>hi?hi:v); }
bool EFFe_IsBackKey(int k) { return k==BACK; }
bool EFFe_IsAcceptKey(int k) { return k==ACCEPT; }
bool EFFe_IsLeftKey(int k) { return k==LEFT; }
bool EFFe_IsRightKey(int k) { return k==RIGHT; }
bool EFFe_IsUpKey(int k) { return k==UP; }
bool EFFe_IsDownKey(int k) { return k==DOWN; }
void EFFe_ReturnToConfigure() {}
void EFFe_DrawUtilityChrome(const char*,int,int) {}
void EFFe_DrawVideoPrompt() {}
void EFFe_DrawPs2TextColor(float,float,const char*,int,int,const float*,float,float) {}
static int drawCount;
static bool selected[8];
void EFFe_DrawPs2RectColor(float x,float y,float w,float h,const float *color) {
    CHECK(color==s_ps2Gold || color==colorTable[CT_WHITE]);
    CHECK(x>=0 && y>=0 && w>0 && h>0);
    CHECK(x+w<=EF_PS2_VIEW_W+0.01f && y+h<=EF_PS2_VIEW_H+0.01f);
    CHECK(drawCount<8); selected[drawCount++]=(color==colorTable[CT_WHITE]);
}

// FRONTEND_CODE

// Keep input math real; stub the surrounding client/platform state.
struct usercmd_t { int unused; };
enum { PITCH=0, YAW=1, AXIS_FORWARD=0, ENTITYNUM_NONE=1023 };
struct InputCvar { float value; } inputPitch, inputYaw={0.022f}, inputSensitivity={2.0f};
InputCvar *m_pitch=&inputPitch, *m_yaw=&inputYaw;
InputCvar *cl_sensitivity=&inputSensitivity, *cl_sensitivityY=&inputSensitivity;
struct InputState {
    int mouseDx[2], mouseDy[2], mouseIndex, joystickAxis[1];
    float cgameSensitivity, viewangles[3];
    struct { struct { int groundEntityNum, delta_angles[3]; } ps; } frame;
} cl;
int frame_msec=16, g_lastFireTime=0, s_stefxHolomatchLastFireTime=0;
short cg_crossHairStatus=0;
float cl_mYawOverride=0, cl_mPitchOverride=0;
float Q_fabs(float x) { return (float)fabs(x); }
int Cvar_VariableIntegerValue(const char *name) { return (int)Cvar_VariableValue(name); }
int Sys_Milliseconds() { return 10000; }
#define SHORT2ANGLE(x) ((x)*(360.0f/65536.0f))

// INPUT_CODE

void TestLookDirection() {
    for(int override=0;override<2;++override) {
        float angle[2][2];
        for(int invert=0;invert<2;++invert) {
            Cvar_SetValue("m_pitch",-0.022f); EFFe_LoadControllerState();
            if(invert) EFFe_HandleControllerKey(RIGHT);
            EFFe_HandleControllerKey(ACCEPT);
            inputPitch.value=Cvar_VariableValue("m_pitch");
            memset(&cl,0,sizeof(cl)); cl.cgameSensitivity=1;
            cl.mouseDx[0]=32; cl.mouseDy[0]=40;
            cl_mPitchOverride=override ? 0.1f : 0;
            usercmd_t cmd={0}; CL_MouseMove(&cmd);
            angle[invert][PITCH]=cl.viewangles[PITCH];
            angle[invert][YAW]=cl.viewangles[YAW];
        }
        CHECK(angle[0][PITCH]<0 && angle[1][PITCH]>0);
        CHECK(fabs(angle[0][PITCH]+angle[1][PITCH])<0.00001f);
        CHECK(angle[0][YAW]==angle[1][YAW]);
    }
}

enum { EF_SCREEN_MAIN=0, EF_SCREEN_VIDEO=1, CVAR_ARCHIVE=1 };
qboolean s_active=qfalse;
int s_screen=EF_SCREEN_MAIN;
struct { int vidWidth, vidHeight; } glConfig={640,480};
struct GLState { bool isWidescreen; } testGLState={false};
GLState *glw_state=&testGLState;
struct cvar_t { int integer; };
std::map<std::string,cvar_t> integerCvars;
cvar_t *s_stefxSafeAreaLeft, *s_stefxSafeAreaTop, *s_stefxSafeAreaRight, *s_stefxSafeAreaBottom;
cvar_t *Cvar_Get(const char *name,const char*,int) {
    integerCvars[name].integer=Cvar_VariableIntegerValue(name);
    return &integerCvars[name];
}
static bool s_xboxPillarbox;

// SAFE_AREA_CODE

void TestCalibrationViewport() {
    Cvar_SetValue("stefx_safeAreaLeft",8); Cvar_SetValue("stefx_safeAreaTop",6);
    Cvar_SetValue("stefx_safeAreaRight",8); Cvar_SetValue("stefx_safeAreaBottom",6);
    int left,top,right,bottom;
    s_active=qtrue; s_screen=EF_SCREEN_MAIN;
    STEFX_GetSafeArea(&left,&top,&right,&bottom);
    CHECK(left==8 && top==6 && right==8 && bottom==6);
    s_screen=EF_SCREEN_VIDEO;
    STEFX_GetSafeArea(&left,&top,&right,&bottom);
    CHECK(left==0 && top==0 && right==0 && bottom==0);
    s_xboxPillarbox=true;
    STEFX_GetSafeArea(&left,&top,&right,&bottom);
    CHECK(left==80 && top==0 && right==80 && bottom==0);
    s_xboxPillarbox=false;
    CHECK(Cvar_VariableValue("stefx_safeAreaLeft")==8);
    s_active=qfalse;
    STEFX_GetSafeArea(&left,&top,&right,&bottom);
    CHECK(left==8 && top==6 && right==8 && bottom==6);
    glConfig.vidWidth=1280;
    glConfig.vidHeight=720;
    STEFX_GetSafeArea(&left,&top,&right,&bottom);
    CHECK(left==16 && top==9 && right==16 && bottom==9);
    glConfig.vidWidth=640;
    glConfig.vidHeight=480;
}

void InstallSigned(const void *data,DWORD n) {
    memcpy(disk,data,n); XCALCSIG_SIGNATURE sig;
    CHECK(XBSettingsSignBytes(data,n,&sig)); memcpy(disk+n,&sig,sizeof(sig));
    diskSize=n+sizeof(sig); diskExists=true;
}
void TestSettings() {
    Settings=XBSettings(); CHECK(Settings.textureFilter==0 && !Settings.invertAim[0]);
    Cvar_SetValue("m_pitch",-0.022f); Cvar_SetValue("s_effects_volume",0.37f);
    Cvar_SetValue("sensitivity",7); Cvar_SetValue("joy_pitchsensitivity_0",-1);
    Settings.LoadEFPreferences(); CHECK(Settings.Missing());
    CHECK(Cvar_VariableValue("m_pitch")<0);
    Settings.invertAim[0]=true; Settings.textureFilter=1;
    Settings.safeAreaLeft=4; Settings.safeAreaTop=6; Settings.safeAreaRight=8; Settings.safeAreaBottom=12;
    Settings.musicVolume=0.42f; CHECK(Settings.Save());
    Settings=XBSettings(); Settings.LoadEFPreferences();
    CHECK(Settings.invertAim[0] && Settings.textureFilter==1);
#ifdef STEFX_SP_HOSTED_MP
    CHECK(Cvar_VariableValue("m_pitch")<0); // MP's own player settings remain authoritative.
#else
    CHECK(Cvar_VariableValue("m_pitch")>0);
#endif
    CHECK(!strcmp(UI_Cvar_VariableString("r_textureMode"),"GL_LINEAR_MIPMAP_LINEAR"));
    CHECK(Cvar_VariableValue("stefx_safeAreaBottom")==12);
    CHECK(fabs(Cvar_VariableValue("s_effects_volume")-0.37f)<0.0001f);
    CHECK(Cvar_VariableValue("sensitivity")==7 && Cvar_VariableValue("joy_pitchsensitivity_0")==-1);
    CHECK(fabs(Settings.musicVolume-0.42f)<0.0001f);
    XBSettings previous=Settings; previous.version=SETTINGS_VERSION_CALIBRATION;
    InstallSigned(&previous,offsetof(XBSettings,textureFilter));
    Settings=XBSettings(); CHECK(Settings.Load());
    CHECK(Settings.version==SETTINGS_VERSION && Settings.textureFilter==0 && Settings.invertAim[0]);
    CHECK(Settings.safeAreaBottom==12 && Settings.musicVolume==previous.musicVolume);
    XBSettingsLegacy082877 legacy; memset(&legacy,0,sizeof(legacy));
    legacy.version=SETTINGS_VERSION_LEGACY; legacy.invertAim[0]=true; legacy.musicVolume=0.6f;
    InstallSigned(&legacy,sizeof(legacy)); CHECK(Settings.Load());
    CHECK(Settings.safeAreaBottom==0 && Settings.textureFilter==0 && Settings.invertAim[0]);
    CHECK(Settings.musicVolume==0.6f && Settings.version==SETTINGS_VERSION);
    CHECK(Settings.Save()); disk[12]^=0x80; previous=Settings;
    CHECK(!Settings.Load() && Settings.Corrupt()); CHECK(!memcmp(&previous,&Settings,sizeof(Settings)));
    previous.textureFilter=99; InstallSigned(&previous,sizeof(previous));
    CHECK(!Settings.Load()); CHECK(Settings.textureFilter==0);
    failWrite=true; CHECK(!Settings.Save()); failWrite=false;
}
void TestMenus() {
    Settings=XBSettings(); cvars.clear(); Cvar_SetValue("m_pitch",-0.022f);
    EFFe_LoadControllerState(); CHECK(!s_controllerInvertY);
    int before=saves; EFFe_HandleControllerKey(RIGHT); CHECK(s_controllerInvertY);
    EFFe_HandleControllerKey(BACK); CHECK(Cvar_VariableValue("m_pitch")<0 && saves==before);
    EFFe_LoadControllerState(); CHECK(!s_controllerInvertY);
    EFFe_HandleControllerKey(LEFT); EFFe_HandleControllerKey(ACCEPT);
    CHECK(Cvar_VariableValue("m_pitch")>0 && Settings.invertAim[0] && saves==before+1);
    Settings=XBSettings(); CHECK(Settings.Load() && Settings.invertAim[0]);
    Cvar_Set("r_textureMode","GL_LINEAR_MIPMAP_NEAREST"); EFFe_LoadVideoState();
    uis.realtime=250;
    for(int corner=0;corner<4;++corner) {
        CHECK(s_videoCorner==corner); drawCount=0; EFFe_DrawVideoScreen(); CHECK(drawCount==8);
        for(int rect=0;rect<8;++rect) CHECK(selected[rect]==(rect/2==corner));
        EFFe_HandleVideoKey(RIGHT); EFFe_HandleVideoKey(DOWN);
        EFFe_HandleVideoKey(A_JOY16);
    }
    CHECK(s_videoCorner==0); // all four corners cycle back to TL
    CHECK(s_videoSafeLeft==8 && s_videoSafeTop==6 && s_videoSafeRight==0 && s_videoSafeBottom==0);
    for(int i=0;i<100;++i) { EFFe_HandleVideoKey(RIGHT); EFFe_HandleVideoKey(DOWN); }
    CHECK(s_videoSafeLeft==96 && s_videoSafeTop==72);
    CHECK(IN_STEFX_IsFrontendButton(A_JOY9)); // Physical WHITE must reach the raw frontend queue.
    EFFe_HandleVideoKey(A_JOY9); CHECK(s_videoTrilinear);
    before=saves; EFFe_HandleVideoKey(BACK); CHECK(saves==before);
    CHECK(Cvar_VariableValue("stefx_safeAreaLeft")==0);
    CHECK(!strcmp(UI_Cvar_VariableString("r_textureMode"),"GL_LINEAR_MIPMAP_NEAREST"));
    EFFe_LoadVideoState(); CHECK(!s_videoTrilinear);
    EFFe_HandleVideoKey(RIGHT); EFFe_HandleVideoKey(DOWN); EFFe_HandleVideoKey(A_JOY9);
    EFFe_HandleVideoKey(ACCEPT); CHECK(saves==before+1 && Settings.textureFilter==1);
    CHECK(Cvar_VariableValue("stefx_safeAreaLeft")==4 && Cvar_VariableValue("stefx_safeAreaTop")==3);
    Settings=XBSettings(); CHECK(Settings.Load() && Settings.textureFilter==1 && Settings.safeAreaLeft==4);
    EFFe_LoadVideoState(); EFFe_HandleVideoKey(A_JOY14); CHECK(!s_videoTrilinear && !s_videoSafeLeft);
    EFFe_HandleVideoKey(BACK); CHECK(Settings.textureFilter==1 && Settings.safeAreaLeft==4);
    EFFe_LoadVideoState(); CHECK(s_videoTrilinear && s_videoSafeLeft==4);
    EFFe_HandleVideoKey(A_JOY14); EFFe_HandleVideoKey(ACCEPT);
    CHECK(Settings.textureFilter==0 && Settings.safeAreaLeft==0);
    EFFe_LoadVideoState();
    for(int corner=0;corner<4;++corner) {
        EFFe_HandleVideoKey(corner%2 ? LEFT : RIGHT);
        EFFe_HandleVideoKey(corner>=2 ? UP : DOWN);
        CHECK((corner%2 ? s_videoSafeRight : s_videoSafeLeft)==4);
        CHECK((corner>=2 ? s_videoSafeBottom : s_videoSafeTop)==3);
        drawCount=0; EFFe_DrawVideoScreen(); CHECK(drawCount==8);
        EFFe_HandleVideoKey(A_JOY14);
        EFFe_HandleVideoKey(A_JOY16);
    }
}
int main(int argc,char**) {
    if(argc>1) { uis.realtime=250; drawCount=0; EFFe_DrawVideoScreen(); return 0; }
    TestSettings(); TestMenus(); TestLookDirection(); TestCalibrationViewport();
    puts("Preference regression checks passed"); return 0;
}
