// leave this at the top of all UI_xxxx files for PCH reasons.
#include "../server/exe_headers.h"

#include "ui_local.h"
#include "../qcommon/stefx_launch.h"
#ifdef _XBOX
#include "../qcommon/xb_settings.h"
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBDirectMapStatus;
extern int CL_STEFX_SplitScreen_LocalSlotForPad(int port);
extern int CL_STEFX_SplitScreen_PadForLocalSlot(int localSlot);
#endif

#define EF_FRONTEND_BUTTON_COUNT 6
#define EF_FRONTEND_CONFIGURE_COUNT 3
#define EF_FRONTEND_NEWGAME_COUNT 9
#define EF_FRONTEND_COOP_COUNT 2
#define EF_FRONTEND_COOP_NEW_COUNT 8
#define EF_FRONTEND_HOLOMATCH_COUNT 4
#define EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT 7
#define EF_FRONTEND_HOLOMATCH_MAP_COUNT 19
#define EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS 4
#define EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS 8
#define EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS 4
#define EF_FRONTEND_HOLOMATCH_CONTROL_STYLES 9
#define EF_FRONTEND_HOLOMATCH_AUTOSWITCH_MODES 3
#define EF_FRONTEND_HOLOMATCH_AUTOAIM_MODES 4
#define EF_FRONTEND_HOLOMATCH_CROSSHAIRS 12
#define EF_FRONTEND_CREW_CATEGORY_COUNT 5
#define EF_FRONTEND_CREW_GROUP_COUNT 3
#define EF_FRONTEND_CREW_MEMBER_COUNT 23
#define EF_FRONTEND_CREW_BIO_LINES 12
#define EF_FRONTEND_FONT_COUNT 3
#define EF_FRONTEND_FONT_CHARS 256
#define EF_FRONTEND_FONT_BUFFER 20000
#define EF_FRONTEND_BUTTON_TEXT_BUFFER 14000
#define EF_FRONTEND_NORMAL_TEXT_BUFFER 55000
#define EF_FRONTEND_MNT_MAX 1073
#define EF_FRONTEND_CREW_LAST_NORMAL_TEXT 192
#define EF_FRONTEND_MBT_MAX 250
#define EF_SPLITSCREEN_BASELINE_MAP "borg1"
#define EF_HOLOMATCH_BASELINE_MAP "hm_borg1"

#define EF_FRONTEND_FONT_TINY 0
#define EF_FRONTEND_FONT_MEDIUM 1
#define EF_FRONTEND_FONT_BIG 2

// The PS2 reference capture is a 1920x899 game viewport inside the PCSX2
// window.  Keep these conversions explicit so the layout can be audited back
// to source pixels.
#define EF_PS2_VIEW_W 1920.0f
#define EF_PS2_VIEW_H 899.0f
#define EF_PS2_X(x) ((float)(x) * (640.0f / EF_PS2_VIEW_W))
#define EF_PS2_Y(y) ((float)(y) * (480.0f / EF_PS2_VIEW_H))
#define EF_PS2_W(w) EF_PS2_X(w)
#define EF_PS2_H(h) EF_PS2_Y(h)
#define EF_ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define EF_PROP_HEIGHT 16
#define EF_PROP_BIG_HEIGHT 24
#define EF_PROP_TINY_HEIGHT 10
#define EF_PROP_GAP_WIDTH 2
#define EF_PROP_GAP_TINY_WIDTH 1
#define EF_PROP_GAP_BIG_WIDTH 3
#define EF_PROP_SPACE_WIDTH 4
#define EF_PROP_SPACE_TINY_WIDTH 3
#define EF_PROP_SPACE_BIG_WIDTH 6


typedef enum {
	EF_SCREEN_MAIN,
	EF_SCREEN_NEWGAME,
	EF_SCREEN_LOADGAME,
	EF_SCREEN_SAVEGAME,
	EF_SCREEN_COOP,
	EF_SCREEN_COOP_NEW,
	EF_SCREEN_HOLOMATCH,
	EF_SCREEN_HOLOMATCH_ADVANCED,
	EF_SCREEN_HOLOMATCH_PLAYERS,
	EF_SCREEN_CREW,
	EF_SCREEN_CREW_ROSTER,
	EF_SCREEN_CREW_BIO,
	EF_SCREEN_CREW_HAZARD_SUIT,
	EF_SCREEN_CREW_VOYAGER,
	EF_SCREEN_CONFIGURE,
	EF_SCREEN_AUDIO,
	EF_SCREEN_VIDEO,
	EF_SCREEN_CONTROLLER,
	EF_SCREEN_STUB
} efFrontendScreen_t;

enum {
	EF_PROMPT_SELECT_MID,
	EF_PROMPT_SELECT_HIGH,
	EF_PROMPT_BACK_HIGH,
	EF_PROMPT_ACCEPT_HIGH,
	EF_PROMPT_CANCEL_HIGH,
	EF_PROMPT_LOAD_SELECT,
	EF_PROMPT_LOAD_BACK,
	EF_PROMPT_SWITCH_CORNERS,
	EF_PROMPT_DEFAULT,
	EF_PROMPT_ACCEPT,
	EF_PROMPT_CANCEL
};

enum {
	EF_UTILITY_LEFT,
	EF_UTILITY_TOP,
	EF_UTILITY_TOP_RIGHT,
	EF_UTILITY_RIGHT,
	EF_UTILITY_BOTTOM_LEFT,
	EF_UTILITY_BOTTOM_RIGHT
};

enum {
	EF_NEWGAME_TITLE,
	EF_NEWGAME_LEFT_PANEL,
	EF_NEWGAME_DIFFICULTY_HEADER,
	EF_NEWGAME_EASY,
	EF_NEWGAME_NORMAL,
	EF_NEWGAME_CHALLENGING,
	EF_NEWGAME_DIFFICULT,
	EF_NEWGAME_GENDER_HEADER,
	EF_NEWGAME_FEMALE,
	EF_NEWGAME_MALE,
	EF_NEWGAME_WARP_CORE,
	EF_NEWGAME_TUTORIAL,
	EF_NEWGAME_ENGAGE_BLOCK
};

enum {
	EF_LOADGAME_LEFT_FRAME,
	EF_LOADGAME_RIGHT_FRAME,
	EF_LOADGAME_BOTTOM_LEFT,
	EF_LOADGAME_BOTTOM_RIGHT
};

enum {
	EF_CONFIGURE_TITLE,
	EF_CONFIGURE_AUDIO,
	EF_CONFIGURE_VIDEO,
	EF_CONFIGURE_CONTROLLER
};

enum {
	EF_AUDIO_TITLE,
	EF_AUDIO_EFFECTS,
	EF_AUDIO_MUSIC,
	EF_AUDIO_VOICE,
	EF_AUDIO_SOUND
};

enum {
	EF_VIDEO_TITLE,
	EF_VIDEO_CORNER_TL,
	EF_VIDEO_CORNER_TR,
	EF_VIDEO_CORNER_BL,
	EF_VIDEO_CORNER_BR,
	EF_VIDEO_INSTRUCTIONS
};

enum {
	EF_CONTROLLER_TITLE,
	EF_CONTROLLER_FRAME_TOP_LEFT,
	EF_CONTROLLER_FRAME_TOP_RIGHT,
	EF_CONTROLLER_FRAME_LEFT_UPPER,
	EF_CONTROLLER_FRAME_LEFT_MIDDLE,
	EF_CONTROLLER_FRAME_LEFT_LOWER,
	EF_CONTROLLER_FRAME_RIGHT_UPPER,
	EF_CONTROLLER_FRAME_RIGHT_LOWER,
	EF_CONTROLLER_FRAME_BOTTOM_LEFT,
	EF_CONTROLLER_FRAME_BOTTOM_RIGHT,
	EF_CONTROLLER_LEFT_CALLOUTS,
	EF_CONTROLLER_CENTER_PAD,
	EF_CONTROLLER_ANALOG_LEFT,
	EF_CONTROLLER_ANALOG_RIGHT,
	EF_CONTROLLER_RIGHT_CALLOUTS
};

enum {
	EF_MBT_NEWGAME = 1,
	EF_MBT_LOADGAME = 2,
	EF_MBT_SETUP = 3,
	EF_MBT_EXPLOREVGER = 4,
	EF_MBT_CREDITS = 5,
	EF_MBT_QUIT = 6,
	EF_MBT_VOYAGERCREW = 7
};

typedef struct {
	float x;
	float y;
	int textEnum;
	const char *fallbackLabel;
	const char *fallbackDescription;
	const char *commandName;
	qboolean enabled;
	int color;
	int color2;
} efFrontendButton_t;

typedef struct {
	const char *listName;
	const char *portraitName;
	const char *voiceName;
	int normalTextStart;
	int normalTextCount;
} efFrontendCrewMember_t;

typedef struct {
	const char *title;
	const char *emblemName;
	int firstMember;
	int memberCount;
} efFrontendCrewGroup_t;

typedef struct {
	qboolean cached;
	qhandle_t whiteShader;
	qhandle_t buttonRight;
	qhandle_t buttonLeftEnd;
	qhandle_t fullButton;
	qhandle_t circle;
	qhandle_t quadrants;
	qhandle_t ps2MainTopLeftChrome;
	qhandle_t ps2UtilityBottomLeftChrome;
	qhandle_t ps2UtilityTopRightChrome;
	qhandle_t ps2ControllerTopRightChrome;
	qhandle_t ps2ControllerBottomLeftChrome;
	qhandle_t ps2LoadTopRightChrome;
	qhandle_t ps2LoadBottomLeftChrome;
	qhandle_t pauseCornerUpper;
	qhandle_t pauseCornerUpper2;
	qhandle_t bracketCorner;
	qhandle_t cornerLove;
	qhandle_t cornerLove2;
	qhandle_t lgTopLeft;
	qhandle_t lgTopRight;
	qhandle_t lgLowLeft;
	qhandle_t lgLowRight;
	qhandle_t panelCorner;
	qhandle_t monBar;
	qhandle_t monBar2;
	qhandle_t slider;
	qhandle_t leftArrow;
	qhandle_t rightArrow;
	qhandle_t xboxA;
	qhandle_t xboxB;
	qhandle_t xboxX;
	qhandle_t xboxY;
	qhandle_t xboxWhite;
	qhandle_t xboxBlack;
	qhandle_t xboxLT;
	qhandle_t xboxRT;
	qhandle_t xboxBack;
	qhandle_t xboxStart;
	qhandle_t xboxLStick;
	qhandle_t xboxRStick;
	qhandle_t xboxDpad[4];
	qhandle_t xboxController;
	qhandle_t warpCore;
	qhandle_t cursor;
	qhandle_t crosshairs[EF_FRONTEND_HOLOMATCH_CROSSHAIRS];
	qhandle_t holomatchPortraits[EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS][3];
	qhandle_t crewFederation;
	qhandle_t crewHazard;
	qhandle_t crewSuit;
	qhandle_t crewSuitPower;
	qhandle_t crewSuitComm;
	qhandle_t crewSuitDirectional;
	qhandle_t crewSuitEnergy;
	qhandle_t crewSuitWave;
	qhandle_t crewSuitScanner;
	qhandle_t crewSuitPouch;
	qhandle_t crewSuitBuffer;
	qhandle_t crewVoyager;
	qhandle_t crewVoyagerPhaser;
	qhandle_t crewVoyagerTorpedo;
	qhandle_t crewVoyagerVentral;
	qhandle_t crewVoyagerMidHull;
	qhandle_t crewVoyagerNacelles;
	qhandle_t crewVoyagerBussard;
	qhandle_t crewVoyagerRcs;
	qhandle_t crewVoyagerBridge;
	qhandle_t crewPortraits[EF_FRONTEND_CREW_MEMBER_COUNT];
	sfxHandle_t crewVoices[EF_FRONTEND_CREW_MEMBER_COUNT];
} efFrontendAssets_t;


typedef struct {
	qboolean loaded;
	qhandle_t shader[EF_FRONTEND_FONT_COUNT];
	int propMap[EF_FRONTEND_FONT_COUNT][EF_FRONTEND_FONT_CHARS][3];
} efFrontendFonts_t;

static efFrontendAssets_t s_assets;
static efFrontendFonts_t s_fonts;
static qboolean s_active = qfalse;
static qboolean s_loggedDraw = qfalse;
static efFrontendScreen_t s_screen = EF_SCREEN_MAIN;
static int s_cursor = 0;
static int s_newgameDifficulty = 1;
static int s_newgameGenderMale = 1;
static qboolean s_newgameGenderTouched = qfalse;
static qboolean s_loadForCoop = qfalse;
static int s_loadCount = 0;
static int s_saveCount = 0;
static qboolean s_saveOverwritePending = qfalse;
static qboolean s_returnToPause = qfalse;
static int s_holomatchPlayers = 4;
static int s_holomatchMap = 0;
static int s_holomatchPointLimit = 0;
static int s_holomatchTimeLimit = 15;
static int s_holomatchForceRespawn = 0;
static int s_holomatchWeaponStay = 1;
static int s_holomatchFallingDamage = 1;
static int s_holomatchTeamPlay = 0;
static int s_holomatchFriendlyFire = 1;
static qhandle_t s_holomatchLevelshots[EF_FRONTEND_HOLOMATCH_MAP_COUNT];
static int s_holomatchPlayerCursors[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 0, 0, 0, 0 };
static int s_holomatchPlayerCharacters[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 7, 10, 9, 2 };
static int s_holomatchPlayerOutfits[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 0, 0, 0 };
static int s_holomatchPlayerControlStyles[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 0, 0, 0, 0 };
static int s_holomatchPlayerAutoswitch[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 1, 1, 1 };
static int s_holomatchPlayerAutoaim[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 1, 1, 1 };
static int s_holomatchPlayerCrosshairs[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 0, 0, 0, 0 };
static int s_holomatchPlayerVibration[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 1, 1, 1 };
static int s_holomatchPlayerInvertPitch[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 1, 1, 1 };
static qboolean s_holomatchPlayerReady[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { qfalse, qfalse, qfalse, qfalse };
static int s_crewGroup = 0;
static int s_crewMember = 0;
static int s_crewBioPage = 0;
static float s_audioEffects = 1.0f;
static float s_audioMusic = 0.25f;
static float s_audioVoice = 1.0f;
static float s_audioOriginalEffects = 1.0f;
static float s_audioOriginalMusic = 0.25f;
static float s_audioOriginalVoice = 1.0f;
static qboolean s_audioTouched = qfalse;
static int s_videoCorner = 0;
static int s_videoSafeLeft = 0;
static int s_videoSafeTop = 0;
static int s_videoSafeRight = 0;
static int s_videoSafeBottom = 0;
static int s_videoOriginalLeft = 0;
static int s_videoOriginalTop = 0;
static int s_videoOriginalRight = 0;
static int s_videoOriginalBottom = 0;
static char s_menuSmokeTarget[32];
static int s_menuSmokeStage = 0;
static int s_menuSmokeNextRealtime = 0;
static int s_menuSmokeMainDownRemaining = 0;
static int s_menuSmokeDownRemaining = 0;
static qboolean s_holomatchMenuVirtualPads = qfalse;
static char s_stubTitle[64];
static char s_stubLine[96];
static char s_fontBuffer[EF_FRONTEND_FONT_BUFFER];
static char s_buttonTextBuffer[EF_FRONTEND_BUTTON_TEXT_BUFFER];
static char *s_buttonText[EF_FRONTEND_MBT_MAX][2];
static qboolean s_buttonTextLoaded = qfalse;
static char s_normalTextBuffer[EF_FRONTEND_NORMAL_TEXT_BUFFER];
static char *s_normalText[EF_FRONTEND_MNT_MAX];
static qboolean s_normalTextLoaded = qfalse;
static int s_normalTextCount = 0;
static char s_crewBioLines[EF_FRONTEND_CREW_BIO_LINES][256];

static const char *s_configureItems[EF_FRONTEND_CONFIGURE_COUNT] = { "AUDIO", "VIDEO", "CONTROLLER" };
static const char *s_newgameItems[EF_FRONTEND_NEWGAME_COUNT] = { "EASY", "NORMAL", "CHALLENGING", "DIFFICULT", "FEMALE", "MALE", "TUTORIAL", "CAMPAIGN", "VIRTUAL VOYAGER" };
static const char *s_holomatchMapNames[EF_FRONTEND_HOLOMATCH_MAP_COUNT] = {
	"hm_borg1", "hm_kln1", "hm_for1", "hm_noon", "hm_voy2", "hm_dn1", "hm_scav1",
	"hm_voy1", "hm_borg2", "hm_dn2", "hm_cam", "hm_borg3", "hm_altar", "hm_blastradius",
	"hm_borgattack", "hm_for2", "hm_raven", "hm_temple", "hm_voy3"
};
static const char *s_holomatchMapTitles[EF_FRONTEND_HOLOMATCH_MAP_COUNT] = {
	"ASSIMILATION", "BRAVERY", "MUTINY", "SHOWDOWN", "DANGEROUS CARGO", "THE MACHINE", "WASTE DISPOSAL",
	"ISOLATION", "RESISTANCE", "HANGAR", "SIEGE", "DATA MATRIX", "ALTAR", "BLAST RADIUS",
	"BORG ATTACK", "FORGE 2", "USS RAVEN", "TEMPLE", "VOYAGER 3"
};

typedef struct {
	int crewMember;
	const char *modelName;
} efFrontendHolomatchCharacter_t;

static const efFrontendHolomatchCharacter_t s_holomatchCharacters[EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS] = {
	{ 7, "seven" },
	{ 10, "munro" },
	{ 9, "foster" },
	{ 2, "tuvok" }
};

static const char *s_holomatchOutfitNames[] = { "GOLD", "RED", "BLUE" };
static const char *s_holomatchOutfitCvars[] = { "default", "red", "blue" };
static const char *s_holomatchControlStyleNames[EF_FRONTEND_HOLOMATCH_CONTROL_STYLES] = {
	"STANDARD", "STD CROSSD", "SNIPER", "SNP CROSSD", "PAD MOBILE",
	"PM CROSSD", "OLDSCHOOL", "FIRE BELOW", "CUSTOM"
};
static const char *s_holomatchAutoswitchNames[EF_FRONTEND_HOLOMATCH_AUTOSWITCH_MODES] = {
	"OFF", "SAFE", "UNSAFE"
};
static const char *s_holomatchAutoaimNames[EF_FRONTEND_HOLOMATCH_AUTOAIM_MODES] = {
	"WIDESPREAD", "CONCENTRATED", "VERTICAL ONLY", "NONE"
};

// These IDs are the retail EF sp_normaltext.dat positions used by ui_crew.cpp.
static const efFrontendCrewMember_t s_crewMembers[EF_FRONTEND_CREW_MEMBER_COUNT] = {
	{ "CPTN. JANEWAY",   "menu/bios/janeway.tga",      "sound/voice/computer/misc/janeway.mp3",    16, 8 },
	{ "CMDR. CHAKOTAY",  "menu/bios/chakotay.tga",     "sound/voice/computer/misc/chakotay.mp3",   24, 7 },
	{ "LT. CMDR. TUVOK", "menu/bios/tuvok.tga",        "sound/voice/computer/misc/tuvok.mp3",      31, 7 },
	{ "LT. TORRES",      "menu/bios/torres.tga",       "sound/voice/computer/misc/torres.mp3",     38, 7 },
	{ "ENSIGN PARIS",    "menu/bios/paris.tga",        "sound/voice/computer/misc/paris.mp3",      45, 8 },
	{ "ENSIGN KIM",      "menu/bios/kim.tga",          "sound/voice/computer/misc/kim.mp3",        53, 7 },
	{ "DOCTOR",          "menu/bios/doctor.tga",       "sound/voice/computer/misc/emhdoctor.mp3",  60, 7 },
	{ "SEVEN OF NINE",   "menu/bios/seven.tga",        "sound/voice/computer/misc/seven.mp3",      67, 7 },
	{ "NEELIX",          "menu/bios/neelix.tga",       "sound/voice/computer/misc/neelix.mp3",     74, 7 },
	{ "LT. FOSTER",      "menu/bios/foster.tga",       "sound/voice/computer/misc/foster.mp3",     81, 8 },
	{ "ENSIGN MUNRO",    "menu/bios/munro.tga",        "sound/voice/computer/misc/munro.mp3",      89, 8 },
	{ "CREWMAN CHANG",   "menu/bios/chang.tga",        "sound/voice/computer/misc/chang.mp3",      97, 8 },
	{ "CREWMAN BIESSMAN", "menu/bios/biessman.tga",    "sound/voice/computer/misc/biessman.mp3",  105, 8 },
	{ "CREWMAN MURPHY",  "menu/bios/telsia.tga",       "sound/voice/computer/misc/telsia.mp3",    113, 8 },
	{ "CREWMAN CHELL",   "menu/bios/chell.tga",        "sound/voice/computer/misc/chell.mp3",     121, 8 },
	{ "CREWMAN JUROT",   "menu/bios/jurot.tga",        "sound/voice/computer/misc/jurot.mp3",     129, 8 },
	{ "CREWMAN CUERVO",  "menu/bios/oviedo.tga",       "sound/voice/computer/misc/oviedo.mp3",    137, 8 },
	{ "CREWMAN LATHROP", "menu/bios/kenn.tga",         "sound/voice/computer/misc/kenn.mp3",      145, 8 },
	{ "CREWMAN ODELL",   "menu/bios/odell.tga",        "sound/voice/computer/misc/odell.mp3",     153, 8 },
	{ "CREWMAN CSATLOS", "menu/bios/csatlos.tga",      "sound/voice/computer/misc/csatlos.mp3",   161, 8 },
	{ "CREWMAN JAWORSKI", "menu/bios/jaworski.tga",    "sound/voice/computer/misc/jaworski.mp3", 169, 8 },
	{ "CREWMAN NELSON",  "menu/bios/nelson.tga",       "sound/voice/computer/misc/nelson.mp3",   177, 8 },
	{ "CREWMAN LAIRD",   "menu/bios/mackey.tga",       "sound/voice/computer/misc/laird.mp3",    185, 8 }
};

static const efFrontendCrewGroup_t s_crewGroups[EF_FRONTEND_CREW_GROUP_COUNT] = {
	{ "SENIOR STAFF", "menu/suit/federation.tga", 0, 9 },
	{ "ALPHA SQUAD",  "menu/common/hazlogo.tga",   9, 7 },
	{ "BETA SQUAD",   "menu/common/hazlogo.tga",  16, 7 }
};

static void EFFe_DrawFrame(const char *title, qboolean backPrompt);

#define EF_PS2_RGBA(r, g, b) { (float)(r) / 255.0f, (float)(g) / 255.0f, (float)(b) / 255.0f, 1.0f }

static vec4_t s_ps2ButtonPurple = EF_PS2_RGBA(105, 83, 145);
static vec4_t s_ps2ButtonSelected = EF_PS2_RGBA(155, 135, 189);
static vec4_t s_ps2StripPurple = EF_PS2_RGBA(121, 69, 111);
static vec4_t s_ps2TopPurple = EF_PS2_RGBA(73, 53, 83);
static vec4_t s_ps2LightBrown = EF_PS2_RGBA(171, 103, 59);
static vec4_t s_ps2DarkBrown = EF_PS2_RGBA(97, 49, 17);
static vec4_t s_ps2Gold = EF_PS2_RGBA(213, 176, 44);
static vec4_t s_ps2MapGold = EF_PS2_RGBA(193, 193, 46);
static vec4_t s_ps2SelectedText = EF_PS2_RGBA(219, 217, 223);
static vec4_t s_ps2DeepPurple = EF_PS2_RGBA(75, 15, 137);
static vec4_t s_ps2BrightPurple = EF_PS2_RGBA(163, 124, 230);
static vec4_t s_ps2MutedPurple = EF_PS2_RGBA(132, 94, 181);
static vec4_t s_ps2AudioGray = EF_PS2_RGBA(143, 143, 143);
static vec4_t s_ps2DialogGray = EF_PS2_RGBA(102, 102, 102);
static vec4_t s_ps2ControllerBody = EF_PS2_RGBA(37, 24, 48);
static vec4_t s_ps2ControllerGrip = EF_PS2_RGBA(30, 22, 38);



static efFrontendButton_t s_buttons[EF_FRONTEND_BUTTON_COUNT] = {
	{EF_PS2_X(491), EF_PS2_Y(253), EF_MBT_NEWGAME,     "NEW GAME",     "START A NEW GAME",         "ui_ef_newgame",   qtrue, CT_DKPURPLE1, CT_LTPURPLE1},
	{EF_PS2_X(491), EF_PS2_Y(356), EF_MBT_LOADGAME,    "LOAD GAME",    "LOAD A SAVED GAME",        "ui_ef_loadgame",  qtrue, CT_DKPURPLE1, CT_LTPURPLE1},
	{EF_PS2_X(491), EF_PS2_Y(459), 0,                  "COOPERATIVE",  "START COOPERATIVE PLAY",   "ui_ef_coop",      qtrue, CT_DKPURPLE1, CT_LTPURPLE1},
	{EF_PS2_X(491), EF_PS2_Y(562), 0,                  "HOLOMATCH",    "ENTER HOLOMATCH",          "ui_ef_holomatch", qtrue, CT_DKPURPLE1, CT_LTPURPLE1},
	{EF_PS2_X(491), EF_PS2_Y(665), 0,                  "CONFIGURE",    "CONFIGURE OPTIONS",        "ui_ef_configure", qtrue, CT_DKPURPLE1, CT_LTPURPLE1},
	{EF_PS2_X(491), EF_PS2_Y(768), EF_MBT_VOYAGERCREW, "VOYAGER CREW", "VIEW CREW BIOGRAPHIES",    "ui_ef_crew",      qtrue, CT_DKPURPLE1, CT_LTPURPLE1}
};

static void EFFe_Adjust(float *x, float *y, float *w, float *h)
{
	(void)x;
	(void)y;
	(void)w;
	(void)h;
}

static void EFFe_DrawPicSTColor(float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader, const float *color)
{
#ifdef _XBOX
	static int s_frontendDrawPicLogBudget = 48;
#endif

	if (!shader)
	{
		return;
	}

	ui.R_SetColor(color);
	EFFe_Adjust(&x, &y, &w, &h);
#ifdef _XBOX
	if (s_frontendDrawPicLogBudget > 0)
	{
		XBLF("STEFX: EF main menu drawpic shader=%d rgba=(%g,%g,%g,%g) rect=(%g,%g %gx%g) st=(%g,%g %g,%g) drawFn=%p",
			shader, color[0], color[1], color[2], color[3], x, y, w, h, s0, t0, s1, t1, (void*)ui.R_DrawStretchPic);
		--s_frontendDrawPicLogBudget;
	}
#endif
	ui.R_DrawStretchPic(x, y, w, h, s0, t0, s1, t1, shader);
}

static void EFFe_DrawPicColor(float x, float y, float w, float h, qhandle_t shader, const float *color)
{
	float s0;
	float s1;
	float t0;
	float t1;

	if (w < 0)
	{
		w = -w;
		s0 = 1.0f;
		s1 = 0.0f;
	}
	else
	{
		s0 = 0.0f;
		s1 = 1.0f;
	}

	if (h < 0)
	{
		h = -h;
		t0 = 1.0f;
		t1 = 0.0f;
	}
	else
	{
		t0 = 0.0f;
		t1 = 1.0f;
	}

	EFFe_DrawPicSTColor(x, y, w, h, s0, t0, s1, t1, shader, color);
}

static void EFFe_DrawPic(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	EFFe_DrawPicColor(x, y, w, h, shader, colorTable[colorIndex]);
}

static void EFFe_ClearFontMaps(void)
{
	int f;
	int i;

	for (f = 0; f < EF_FRONTEND_FONT_COUNT; f++) {
		for (i = 0; i < EF_FRONTEND_FONT_CHARS; i++) {
			s_fonts.propMap[f][i][0] = 0;
			s_fonts.propMap[f][i][1] = 0;
			s_fonts.propMap[f][i][2] = -1;
		}
	}
}

static qboolean EFFe_ParseFontMap(const char **cursor, int fontIndex, const char *debugName)
{
	char *token;
	int ch;
	int component;

	token = COM_ParseExt(cursor, qtrue);
	if (!token[0] || Q_stricmp(token, "{")) {
#ifdef _XBOX
		XBLF("STEFX: EF frontend font parse fail map='%s' expected_open token='%s'",
			debugName, token ? token : "<null>");
#endif
		return qfalse;
	}

	for (ch = 0; ch < EF_FRONTEND_FONT_CHARS; ch++) {
		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "{")) {
#ifdef _XBOX
			XBLF("STEFX: EF frontend font parse fail map='%s' char=%d expected_char_open token='%s'",
				debugName, ch, token ? token : "<null>");
#endif
			return qfalse;
		}

		for (component = 0; component < 3; component++) {
			token = COM_ParseExt(cursor, qtrue);
			if (!token[0]) {
#ifdef _XBOX
				XBLF("STEFX: EF frontend font parse fail map='%s' char=%d component=%d empty",
					debugName, ch, component);
#endif
				return qfalse;
			}
			s_fonts.propMap[fontIndex][ch][component] = atoi(token);
		}

		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "}")) {
#ifdef _XBOX
			XBLF("STEFX: EF frontend font parse fail map='%s' char=%d expected_char_close token='%s'",
				debugName, ch, token ? token : "<null>");
#endif
			return qfalse;
		}
	}

	token = COM_ParseExt(cursor, qtrue);
	if (!token[0] || Q_stricmp(token, "}")) {
#ifdef _XBOX
		XBLF("STEFX: EF frontend font parse fail map='%s' expected_close token='%s'",
			debugName, token ? token : "<null>");
#endif
		return qfalse;
	}

	return qtrue;
}

static void EFFe_LoadFonts(void)
{
	fileHandle_t file;
	int len;
	const char *cursor;
	qboolean ok;

	EFFe_ClearFontMaps();
	s_fonts.shader[EF_FRONTEND_FONT_TINY] = ui.R_RegisterShaderNoMip("gfx/2d/chars_tiny.tga");
	s_fonts.shader[EF_FRONTEND_FONT_MEDIUM] = ui.R_RegisterShaderNoMip("gfx/2d/chars_medium.tga");
	s_fonts.shader[EF_FRONTEND_FONT_BIG] = ui.R_RegisterShaderNoMip("gfx/2d/chars_big.tga");

	len = ui.FS_FOpenFile("ext_data/fonts.dat", &file, FS_READ);
	if (!file) {
#ifdef _XBOX
		XBLF("STEFX: EF frontend font file missing len=%d tiny=%d med=%d big=%d",
			len, s_fonts.shader[0], s_fonts.shader[1], s_fonts.shader[2]);
#endif
		s_fonts.loaded = qfalse;
		return;
	}

	if (len <= 0 || len >= EF_FRONTEND_FONT_BUFFER) {
#ifdef _XBOX
		XBLF("STEFX: EF frontend font file bad len=%d max=%d",
			len, EF_FRONTEND_FONT_BUFFER);
#endif
		ui.FS_FCloseFile(file);
		s_fonts.loaded = qfalse;
		return;
	}

	memset(s_fontBuffer, 0, sizeof(s_fontBuffer));
	ui.FS_Read(s_fontBuffer, len, file);
	ui.FS_FCloseFile(file);

	cursor = s_fontBuffer;
	ok = EFFe_ParseFontMap(&cursor, EF_FRONTEND_FONT_TINY, "tiny");
	if (ok) {
		ok = EFFe_ParseFontMap(&cursor, EF_FRONTEND_FONT_MEDIUM, "medium");
	}
	if (ok) {
		ok = EFFe_ParseFontMap(&cursor, EF_FRONTEND_FONT_BIG, "big");
	}

	s_fonts.loaded = ok;
#ifdef _XBOX
	XBLF("STEFX: EF frontend fonts loaded=%d len=%d tiny=%d med=%d big=%d sampleA=%d/%d/%d",
		s_fonts.loaded ? 1 : 0,
		len,
		s_fonts.shader[0],
		s_fonts.shader[1],
		s_fonts.shader[2],
		s_fonts.propMap[EF_FRONTEND_FONT_MEDIUM]['A'][0],
		s_fonts.propMap[EF_FRONTEND_FONT_MEDIUM]['A'][1],
		s_fonts.propMap[EF_FRONTEND_FONT_MEDIUM]['A'][2]);
#endif
}

static void EFFe_LanguageFilename(const char *baseName, const char *baseExtension, char *finalName)
{
	char language[MAX_QPATH];
	fileHandle_t file;

	ui.Cvar_VariableStringBuffer("g_language", language, sizeof(language));
	if (language[0] == '\0' || Q_stricmp("ENGLISH", language) == 0)
	{
		Com_sprintf(finalName, MAX_QPATH, "%s.%s", baseName, baseExtension);
		return;
	}

	Com_sprintf(finalName, MAX_QPATH, "%s_%s.%s", baseName, language, baseExtension);
	ui.FS_FOpenFile(finalName, &file, FS_READ);
	if (file == 0)
	{
		Com_sprintf(finalName, MAX_QPATH, "%s.%s", baseName, baseExtension);
	}
	else
	{
		ui.FS_FCloseFile(file);
	}
}

static char *EFFe_ParseQuoted(char **cursor)
{
	char *p;
	char *start;
	char *out;

	if (!cursor || !*cursor)
	{
		return NULL;
	}

	p = *cursor;
	for (;;)
	{
		while (*p && *p <= ' ')
		{
			p++;
		}
		if (p[0] == '/' && p[1] == '/')
		{
			while (*p && *p != '\n')
			{
				p++;
			}
			continue;
		}
		if (p[0] == '/' && p[1] == '*')
		{
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/'))
			{
				p++;
			}
			if (*p)
			{
				p += 2;
			}
			continue;
		}
		break;
	}

	while (*p && *p != '"')
	{
		p++;
	}
	if (!*p)
	{
		*cursor = NULL;
		return NULL;
	}

	p++;
	start = p;
	out = p;
	while (*p && *p != '"')
	{
		if (*p == '\\' && p[1])
		{
			p++;
		}
		*out++ = *p++;
	}
	if (*p == '"')
	{
		p++;
	}
	*out = '\0';
	*cursor = p;
	return start;
}

static int EFFe_LoadTextFile(const char *baseName, const char *extension, char *dest, int destSize)
{
	char filename[MAX_QPATH];
	char *fileBuffer;
	int len;
	int copyLen;

	EFFe_LanguageFilename(baseName, extension, filename);
	len = ui.FS_ReadFile(filename, (void **)&fileBuffer);
	if (len < 0 || !fileBuffer)
	{
#ifdef _XBOX
		XBLF("STEFX: EF frontend text missing file='%s' len=%d", filename, len);
#endif
		return -1;
	}

	copyLen = len;
	if (copyLen >= destSize)
	{
		copyLen = destSize - 1;
	}
	memcpy(dest, fileBuffer, copyLen);
	dest[copyLen] = '\0';
	ui.FS_FreeFile(fileBuffer);
#ifdef _XBOX
	XBLF("STEFX: EF frontend text loaded file='%s' len=%d copied=%d", filename, len, copyLen);
#endif
	return copyLen;
}

static void EFFe_LoadButtonText(void)
{
	char *cursor;
	char *token;
	int i;

	if (s_buttonTextLoaded)
	{
		return;
	}

	memset(s_buttonText, 0, sizeof(s_buttonText));
	s_buttonText[0][0] = "";
	s_buttonText[0][1] = NULL;

	if (EFFe_LoadTextFile("ext_data/sp_buttontext", "dat", s_buttonTextBuffer, sizeof(s_buttonTextBuffer)) >= 0)
	{
		cursor = s_buttonTextBuffer;
		i = 1;
		while (i < EF_FRONTEND_MBT_MAX)
		{
			token = EFFe_ParseQuoted(&cursor);
			if (!token)
			{
				break;
			}
			s_buttonText[i][0] = ((token[0] == '/') && (token[1] == '\0')) ? NULL : token;
			token = EFFe_ParseQuoted(&cursor);
			if (!token)
			{
				break;
			}
			s_buttonText[i][1] = ((token[0] == '/') && (token[1] == '\0')) ? NULL : token;
			i++;
		}
#ifdef _XBOX
		XBLF("STEFX: EF frontend button text parsed count=%d max=%d", i, EF_FRONTEND_MBT_MAX);
#endif
	}

	s_buttonTextLoaded = qtrue;
}

static void EFFe_LoadNormalText(void)
{
	char *cursor;
	char *token;
	int i;
	int loadedLen;

	if (s_normalTextLoaded && s_normalTextCount > EF_FRONTEND_CREW_LAST_NORMAL_TEXT)
	{
		return;
	}

	memset(s_normalText, 0, sizeof(s_normalText));
	s_normalText[0] = "";
	s_normalTextCount = 0;
	s_normalTextLoaded = qfalse;
	loadedLen = EFFe_LoadTextFile("ext_data/sp_normaltext", "dat", s_normalTextBuffer, sizeof(s_normalTextBuffer));
	if (loadedLen >= 0)
	{
		cursor = s_normalTextBuffer;
		i = 1;
		while (i < EF_FRONTEND_MNT_MAX)
		{
			token = EFFe_ParseQuoted(&cursor);
			if (!token)
			{
				break;
			}
			s_normalText[i++] = ((token[0] == '/') && (token[1] == '\0')) ? NULL : token;
		}
		s_normalTextCount = i;
		s_normalTextLoaded = i > EF_FRONTEND_CREW_LAST_NORMAL_TEXT ? qtrue : qfalse;
#ifdef _XBOX
		XBLog_WriteCriticalf("STEFX_CREW_TEXT: loadedLen=%d count=%d max=%d crewReady=%d title='%s' name='%s'",
			loadedLen, i, EF_FRONTEND_MNT_MAX, s_normalTextLoaded ? 1 : 0,
			(i > 16 && s_normalText[16]) ? s_normalText[16] : "",
			(i > 17 && s_normalText[17]) ? s_normalText[17] : "");
#endif
	}
#ifdef _XBOX
	else
	{
		static int s_crewTextLoadFailureLogBudget = 4;
		if (s_crewTextLoadFailureLogBudget > 0)
		{
			XBLog_WriteCriticalf("STEFX_CREW_TEXT: load failed len=%d", loadedLen);
			--s_crewTextLoadFailureLogBudget;
		}
	}
#endif
}

static const char *EFFe_NormalText(int index, const char *fallback)
{
	if (index > 0 && index < EF_FRONTEND_MNT_MAX && s_normalText[index] && s_normalText[index][0])
	{
		return s_normalText[index];
	}
	return fallback ? fallback : "";
}

static const char *EFFe_ButtonText(const efFrontendButton_t *button, int column)
{
	if (button && button->textEnum > 0 && button->textEnum < EF_FRONTEND_MBT_MAX &&
		column >= 0 && column < 2 && s_buttonText[button->textEnum][column] && s_buttonText[button->textEnum][column][0])
	{
		return s_buttonText[button->textEnum][column];
	}

	if (column == 1)
	{
		return button && button->fallbackDescription ? button->fallbackDescription : "";
	}
	return button && button->fallbackLabel ? button->fallbackLabel : "";
}

static void EFFe_Cache(void)
{
	int i;

	if (s_assets.cached)
	{
		return;
	}

	s_assets.whiteShader = ui.R_RegisterShader("white");
	s_assets.buttonRight = ui.R_RegisterShaderNoMip("menu/new/bar1.tga");
	s_assets.buttonLeftEnd = ui.R_RegisterShaderNoMip("menu/common/barbuttonleft.tga");
	s_assets.fullButton = ui.R_RegisterShaderNoMip("menu/common/full_button2.tga");
	s_assets.circle = ui.R_RegisterShaderNoMip("menu/common/circle.tga");
	s_assets.quadrants = ui.R_RegisterShaderNoMip("menu/special/quadrants.jpg");
	s_assets.ps2MainTopLeftChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_main_lcars_top_left.tga");
	s_assets.ps2UtilityBottomLeftChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_utility_bottom_left.tga");
	s_assets.ps2UtilityTopRightChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_utility_top_right.tga");
	s_assets.ps2ControllerTopRightChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_controller_top_right.tga");
	s_assets.ps2ControllerBottomLeftChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_controller_bottom_left.tga");
	s_assets.ps2LoadTopRightChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_load_top_right.tga");
	s_assets.ps2LoadBottomLeftChrome = ui.R_RegisterShaderNoMip("menu/common/ps2_load_bottom_left.tga");
	s_assets.pauseCornerUpper = ui.R_RegisterShaderNoMip("menu/common/corner_ll_47_7.tga");
	s_assets.pauseCornerUpper2 = ui.R_RegisterShaderNoMip("menu/common/corner_ul_47_7.tga");
	s_assets.bracketCorner = ui.R_RegisterShaderNoMip("menu/common/corner_ul_16_18.tga");
	s_assets.cornerLove = ui.R_RegisterShaderNoMip("menu/common/corner_love.tga");
	s_assets.cornerLove2 = ui.R_RegisterShaderNoMip("menu/common/corner_love_2.tga");
	s_assets.lgTopLeft = ui.R_RegisterShaderNoMip("menu/common/lg_topleft.tga");
	s_assets.lgTopRight = ui.R_RegisterShaderNoMip("menu/common/lg_topright.tga");
	s_assets.lgLowLeft = ui.R_RegisterShaderNoMip("menu/common/lg_lowleft.tga");
	s_assets.lgLowRight = ui.R_RegisterShaderNoMip("menu/common/lg_lowright.tga");
	s_assets.panelCorner = ui.R_RegisterShaderNoMip("menu/lcarscontrols/round11.tga");
	s_assets.monBar = ui.R_RegisterShaderNoMip("menu/common/mon_bar.tga");
	s_assets.monBar2 = ui.R_RegisterShaderNoMip("menu/common/monbar_2.tga");
	s_assets.slider = ui.R_RegisterShaderNoMip("menu/common/slider.tga");
	s_assets.leftArrow = ui.R_RegisterShaderNoMip("menu/common/left_arrow.tga");
	s_assets.rightArrow = ui.R_RegisterShaderNoMip("menu/common/right_arrow.tga");
	s_assets.xboxA = ui.R_RegisterShaderNoMip("menu/common/xbox_a.tga");
	s_assets.xboxB = ui.R_RegisterShaderNoMip("menu/common/xbox_b.tga");
	s_assets.xboxX = ui.R_RegisterShaderNoMip("menu/common/xbox_x.tga");
	s_assets.xboxY = ui.R_RegisterShaderNoMip("menu/common/xbox_y.tga");
	s_assets.xboxWhite = ui.R_RegisterShaderNoMip("menu/common/xbox_white.tga");
	s_assets.xboxBlack = ui.R_RegisterShaderNoMip("menu/common/xbox_black.tga");
	s_assets.xboxLT = ui.R_RegisterShaderNoMip("menu/common/xbox_lt.tga");
	s_assets.xboxRT = ui.R_RegisterShaderNoMip("menu/common/xbox_rt.tga");
	s_assets.xboxBack = ui.R_RegisterShaderNoMip("menu/common/xbox_back.tga");
	s_assets.xboxStart = ui.R_RegisterShaderNoMip("menu/common/xbox_start.tga");
	s_assets.xboxLStick = ui.R_RegisterShaderNoMip("menu/common/xbox_lstick.tga");
	s_assets.xboxRStick = ui.R_RegisterShaderNoMip("menu/common/xbox_rstick.tga");
	s_assets.xboxDpad[0] = ui.R_RegisterShaderNoMip("menu/common/xbox_dpad_up.tga");
	s_assets.xboxDpad[1] = ui.R_RegisterShaderNoMip("menu/common/xbox_dpad_down.tga");
	s_assets.xboxDpad[2] = ui.R_RegisterShaderNoMip("menu/common/xbox_dpad_left.tga");
	s_assets.xboxDpad[3] = ui.R_RegisterShaderNoMip("menu/common/xbox_dpad_right.tga");
	s_assets.xboxController = ui.R_RegisterShaderNoMip("menu/common/xbox_controller_s.tga");
	s_assets.warpCore = ui.R_RegisterShaderNoMip("menu/common/warpcore2.jpg");
	s_assets.cursor = ui.R_RegisterShaderNoMip("menu/common/cursor.tga");
	for (i = 0; i < EF_FRONTEND_HOLOMATCH_CROSSHAIRS; ++i)
	{
		char crosshairName[32];
		Com_sprintf(crosshairName, sizeof(crosshairName), "gfx/2d/crosshair%c", 'a' + i);
		s_assets.crosshairs[i] = ui.R_RegisterShaderNoMip(crosshairName);
	}
	for (i = 0; i < EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS; ++i)
	{
		int outfit;
		for (outfit = 0; outfit < EF_ARRAY_LEN(s_holomatchOutfitCvars); ++outfit)
		{
			char iconName[MAX_QPATH];
			Com_sprintf(iconName, sizeof(iconName), "models/players2/%s/icon_%s.jpg",
				s_holomatchCharacters[i].modelName, s_holomatchOutfitCvars[outfit]);
			s_assets.holomatchPortraits[i][outfit] = ui.R_RegisterShaderNoMip(iconName);
		}
	}
	s_assets.crewFederation = ui.R_RegisterShaderNoMip("menu/suit/federation.tga");
	s_assets.crewHazard = ui.R_RegisterShaderNoMip("menu/common/hazlogo.tga");
	s_assets.crewSuit = ui.R_RegisterShaderNoMip("menu/suit/breakout_suit.tga");
	s_assets.crewSuitPower = ui.R_RegisterShaderNoMip("menu/suit/power_conv.tga");
	s_assets.crewSuitComm = ui.R_RegisterShaderNoMip("menu/suit/combadge.tga");
	s_assets.crewSuitDirectional = ui.R_RegisterShaderNoMip("menu/suit/direct_log_circ.tga");
	s_assets.crewSuitEnergy = ui.R_RegisterShaderNoMip("menu/suit/energy_pack.tga");
	s_assets.crewSuitWave = ui.R_RegisterShaderNoMip("menu/suit/multi_wavegen.tga");
	s_assets.crewSuitScanner = ui.R_RegisterShaderNoMip("menu/suit/pass_acscan.tga");
	s_assets.crewSuitPouch = ui.R_RegisterShaderNoMip("menu/suit/pouches.tga");
	s_assets.crewSuitBuffer = ui.R_RegisterShaderNoMip("menu/suit/trans_buff.tga");
	s_assets.crewVoyager = ui.R_RegisterShaderNoMip("menu/special/voy_1.tga");
	s_assets.crewVoyagerPhaser = ui.R_RegisterShaderNoMip("menu/voyager/phaser_strip.tga");
	s_assets.crewVoyagerTorpedo = ui.R_RegisterShaderNoMip("menu/voyager/photon_launch.tga");
	s_assets.crewVoyagerVentral = ui.R_RegisterShaderNoMip("menu/voyager/bottom_strip.tga");
	s_assets.crewVoyagerMidHull = ui.R_RegisterShaderNoMip("menu/voyager/mid_hull.tga");
	s_assets.crewVoyagerNacelles = ui.R_RegisterShaderNoMip("menu/voyager/warpnac.tga");
	s_assets.crewVoyagerBussard = ui.R_RegisterShaderNoMip("menu/voyager/bussard.tga");
	s_assets.crewVoyagerRcs = ui.R_RegisterShaderNoMip("menu/voyager/rcs.tga");
	s_assets.crewVoyagerBridge = ui.R_RegisterShaderNoMip("menu/voyager/bridge.tga");
	for (i = 0; i < EF_FRONTEND_CREW_MEMBER_COUNT; ++i)
	{
		s_assets.crewPortraits[i] = ui.R_RegisterShaderNoMip(s_crewMembers[i].portraitName);
		s_assets.crewVoices[i] = ui.S_RegisterSound(s_crewMembers[i].voiceName);
	}

	EFFe_LoadButtonText();
	EFFe_LoadNormalText();
	EFFe_LoadFonts();

	s_assets.cached = qtrue;
#ifdef _XBOX
	XBLF("STEFX: EF main menu cache done white=%d quad=%d leftCap=%d rightBar=%d warp=%d fontSmall=%d crewFed=%d crewHaz=%d crewSuit=%d crewVoy=%d crewVoice0=%d",
		s_assets.whiteShader, s_assets.quadrants, s_assets.buttonLeftEnd, s_assets.buttonRight, s_assets.warpCore, s_fonts.shader[1],
		s_assets.crewFederation, s_assets.crewHazard, s_assets.crewSuit, s_assets.crewVoyager, s_assets.crewVoices[0]);
	ui.Printf("STEFX: EF main menu cache done white=%d quad=%d leftCap=%d rightBar=%d warp=%d fontSmall=%d crewFed=%d crewHaz=%d crewSuit=%d crewVoy=%d crewVoice0=%d\n",
		s_assets.whiteShader, s_assets.quadrants, s_assets.buttonLeftEnd, s_assets.buttonRight, s_assets.warpCore, s_fonts.shader[1],
		s_assets.crewFederation, s_assets.crewHazard, s_assets.crewSuit, s_assets.crewVoyager, s_assets.crewVoices[0]);
#endif
}

void UI_EFMainMenu_Cache(void)
{
	EFFe_Cache();
}

void UI_EFMainMenu_InvalidateCache(void)
{
	memset(&s_assets, 0, sizeof(s_assets));
	memset(&s_fonts, 0, sizeof(s_fonts));
	memset(s_holomatchLevelshots, 0, sizeof(s_holomatchLevelshots));
	memset(s_buttonText, 0, sizeof(s_buttonText));
	memset(s_normalText, 0, sizeof(s_normalText));
	s_buttonTextLoaded = qfalse;
	s_normalTextLoaded = qfalse;
	s_normalTextCount = 0;
	s_loggedDraw = qfalse;
#ifdef _XBOX
	XBLog_Write("STEFX: EF main menu renderer cache invalidated");
#endif
}

static float EFFe_TextWidthScaled(const char *text, int fontIndex, float scale)
{
	int gap;
	int spaceWidth;
	float width = 0.0f;
	const unsigned char *s = (const unsigned char *)text;

	if (!text || scale <= 0.0f)
	{
		return 0.0f;
	}

	gap = (fontIndex == EF_FRONTEND_FONT_TINY) ? EF_PROP_GAP_TINY_WIDTH :
		((fontIndex == EF_FRONTEND_FONT_BIG) ? EF_PROP_GAP_BIG_WIDTH : EF_PROP_GAP_WIDTH);
	spaceWidth = (fontIndex == EF_FRONTEND_FONT_TINY) ? EF_PROP_SPACE_TINY_WIDTH :
		((fontIndex == EF_FRONTEND_FONT_BIG) ? EF_PROP_SPACE_BIG_WIDTH : EF_PROP_SPACE_WIDTH);

	while (*s)
	{
		int w = (*s == ' ') ? spaceWidth : s_fonts.propMap[fontIndex][*s][2];
		if (w != -1) {
			width += (float)(w + gap) * scale;
		}
		s++;
	}

	return (width > 0.0f) ? (width - (float)gap * scale) : 0.0f;
}

static int EFFe_TextWidth(const char *text, int fontIndex)
{
	return (int)(EFFe_TextWidthScaled(text, fontIndex, 1.0f) + 0.5f);
}

static void EFFe_DrawTextScaledXYColor(float x, float y, const char *text, int fontIndex, int style, const float *color, float xScale, float yScale)
{
	const unsigned char *s;

	if (!text || !s_fonts.loaded || !s_fonts.shader[fontIndex] || xScale <= 0.0f || yScale <= 0.0f)
	{
		return;
	}

	if (style & UI_CENTER)
	{
		x -= EFFe_TextWidthScaled(text, fontIndex, xScale) * 0.5f;
	}
	else if (style & UI_RIGHT)
	{
		x -= EFFe_TextWidthScaled(text, fontIndex, xScale);
	}

	ui.R_SetColor(color);
	s = (const unsigned char *)text;
	while (*s)
	{
		int ch = *s++;
		int sx = s_fonts.propMap[fontIndex][ch][0];
		int sy = s_fonts.propMap[fontIndex][ch][1];
		int sw = s_fonts.propMap[fontIndex][ch][2];
		int gap = (fontIndex == EF_FRONTEND_FONT_TINY) ? EF_PROP_GAP_TINY_WIDTH :
			((fontIndex == EF_FRONTEND_FONT_BIG) ? EF_PROP_GAP_BIG_WIDTH : EF_PROP_GAP_WIDTH);
		int spaceWidth = (fontIndex == EF_FRONTEND_FONT_TINY) ? EF_PROP_SPACE_TINY_WIDTH :
			((fontIndex == EF_FRONTEND_FONT_BIG) ? EF_PROP_SPACE_BIG_WIDTH : EF_PROP_SPACE_WIDTH);
		float rawH, s0, t0, s1, t1, w, h, drawX, drawY;

		if (ch == ' ') {
			sw = spaceWidth;
		}
		if (sw == -1) {
			continue;
		}
		rawH = (fontIndex == EF_FRONTEND_FONT_TINY) ? (float)EF_PROP_TINY_HEIGHT :
			((fontIndex == EF_FRONTEND_FONT_BIG) ? (float)EF_PROP_BIG_HEIGHT : (float)EF_PROP_HEIGHT);
		w = (float)sw * xScale;
		h = rawH * yScale;
		s0 = (float)sx / 256.0f;
		t0 = (float)sy / 256.0f;
		s1 = (float)(sx + sw) / 256.0f;
		t1 = (float)(sy + (int)rawH) / 256.0f;
		if (ch != ' ') {
			drawX = x;
			drawY = y;
#ifdef _XBOX
			{
				static int s_frontendDrawTextLogBudget = 48;
				if (s_frontendDrawTextLogBudget > 0)
				{
					XBLF("STEFX: EF main menu drawtext ch=%d font=%d shader=%d rect=(%g,%g %gx%g) st=(%g,%g %g,%g) drawFn=%p",
						ch, fontIndex, s_fonts.shader[fontIndex], drawX, drawY, w, h, s0, t0, s1, t1, (void*)ui.R_DrawStretchPic);
					--s_frontendDrawTextLogBudget;
				}
			}
#endif
			ui.R_DrawStretchPic(drawX, drawY, w, h, s0, t0, s1, t1, s_fonts.shader[fontIndex]);
		}
		x += (float)(sw + gap) * xScale;
	}
	ui.R_SetColor(NULL);
}

static void EFFe_DrawTextScaledColor(float x, float y, const char *text, int fontIndex, int style, const float *color, float scale)
{
	EFFe_DrawTextScaledXYColor(x, y, text, fontIndex, style, color, scale, scale);
}

static void EFFe_DrawTextScaled(float x, float y, const char *text, int fontIndex, int style, int colorIndex, float scale)
{
	EFFe_DrawTextScaledColor(x, y, text, fontIndex, style, colorTable[colorIndex], scale);
}

static void EFFe_DrawText(float x, float y, const char *text, int fontIndex, int style, int colorIndex)
{
	EFFe_DrawTextScaled(x, y, text, fontIndex, style, colorIndex, 1.0f);
}

static void EFFe_DrawPs2PicColor(float x, float y, float w, float h, qhandle_t shader, const float *color);
static void EFFe_DrawPs2PicFlipXColor(float x, float y, float w, float h, qhandle_t shader, const float *color);
static void EFFe_DrawPs2TextColor(float x, float y, const char *text, int fontIndex, int style, const float *color, float xScale, float yScale);

static float EFFe_FontPs2Height(int fontIndex, float yScale)
{
	float rawH;

	rawH = (fontIndex == EF_FRONTEND_FONT_TINY) ? (float)EF_PROP_TINY_HEIGHT :
		((fontIndex == EF_FRONTEND_FONT_BIG) ? (float)EF_PROP_BIG_HEIGHT : (float)EF_PROP_HEIGHT);
	return rawH * yScale * (EF_PS2_VIEW_H / 480.0f);
}

static float EFFe_CenteredPs2TextY(float y, float h, int fontIndex, float yScale)
{
	return y + (h - EFFe_FontPs2Height(fontIndex, yScale)) * 0.5f;
}

static void EFFe_DrawButton(const efFrontendButton_t *button, int index)
{
	const float *fillColor = (index == s_cursor) ? s_ps2ButtonSelected : s_ps2ButtonPurple;
	const float *textColor = (index == s_cursor) ? s_ps2SelectedText : colorTable[CT_BLACK];
	float buttonH = EF_PS2_H(86);
	float buttonY = button->y / (480.0f / EF_PS2_VIEW_H);

	if (index >= 0 && index < EF_FRONTEND_BUTTON_COUNT)
	{
		EFFe_DrawPs2PicColor(401.0f, buttonY, 112.0f, 86.0f, s_assets.buttonLeftEnd, fillColor);
		EFFe_DrawPicColor(button->x, button->y, EF_PS2_W(394), buttonH, s_assets.whiteShader, fillColor);
		EFFe_DrawPs2TextColor(515.0f, EFFe_CenteredPs2TextY(buttonY, 86.0f, EF_FRONTEND_FONT_BIG, 1.0f),
			EFFe_ButtonText(button, 0),
			EF_FRONTEND_FONT_BIG,
			UI_LEFT,
			textColor,
			0.82f,
			1.00f);
	}
}

static void EFFe_DrawQuadrantLabel(float x, float y, const char *text)
{
	EFFe_DrawTextScaledXYColor(x, y, text, EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.835f, 0.78f);
}

static void EFFe_DrawPs2Pic(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	EFFe_DrawPic(EF_PS2_X(x), EF_PS2_Y(y), EF_PS2_W(w), EF_PS2_H(h), shader, colorIndex);
}

static void EFFe_DrawPs2PicColor(float x, float y, float w, float h, qhandle_t shader, const float *color)
{
	EFFe_DrawPicColor(EF_PS2_X(x), EF_PS2_Y(y), EF_PS2_W(w), EF_PS2_H(h), shader, color);
}

static void EFFe_DrawPs2PicSTColor(float x, float y, float w, float h, float s0, float t0, float s1, float t1, qhandle_t shader, const float *color)
{
	EFFe_DrawPicSTColor(EF_PS2_X(x), EF_PS2_Y(y), EF_PS2_W(w), EF_PS2_H(h), s0, t0, s1, t1, shader, color);
}

static void EFFe_DrawPs2PicFlipX(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	EFFe_DrawPic(EF_PS2_X(x), EF_PS2_Y(y), -EF_PS2_W(w), EF_PS2_H(h), shader, colorIndex);
}

static void EFFe_DrawPs2PicFlipXColor(float x, float y, float w, float h, qhandle_t shader, const float *color)
{
	EFFe_DrawPicColor(EF_PS2_X(x), EF_PS2_Y(y), -EF_PS2_W(w), EF_PS2_H(h), shader, color);
}

static void EFFe_DrawPs2PicFlipY(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	EFFe_DrawPic(EF_PS2_X(x), EF_PS2_Y(y), EF_PS2_W(w), -EF_PS2_H(h), shader, colorIndex);
}

static void EFFe_DrawPs2PicFlipYColor(float x, float y, float w, float h, qhandle_t shader, const float *color)
{
	EFFe_DrawPicColor(EF_PS2_X(x), EF_PS2_Y(y), EF_PS2_W(w), -EF_PS2_H(h), shader, color);
}

static void EFFe_DrawPs2PicFlipXY(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	EFFe_DrawPic(EF_PS2_X(x), EF_PS2_Y(y), -EF_PS2_W(w), -EF_PS2_H(h), shader, colorIndex);
}

static void EFFe_DrawPs2PicFlipXYColor(float x, float y, float w, float h, qhandle_t shader, const float *color)
{
	EFFe_DrawPicColor(EF_PS2_X(x), EF_PS2_Y(y), -EF_PS2_W(w), -EF_PS2_H(h), shader, color);
}

static void EFFe_DrawPs2Rect(float x, float y, float w, float h, int colorIndex)
{
	EFFe_DrawPs2Pic(x, y, w, h, s_assets.whiteShader, colorIndex);
}

static void EFFe_DrawPs2RectColor(float x, float y, float w, float h, const float *color)
{
	EFFe_DrawPs2PicColor(x, y, w, h, s_assets.whiteShader, color);
}

/*
** Draw a callout from authored PS2-space endpoints without requiring a
** raster line asset.  Axis-aligned runs stay single crisp quads; diagonal
** runs are tessellated into overlapping white-shader quads so the same
** vector coordinates survive the anisotropic 1920x899-to-640x480 transform.
*/
static void EFFe_DrawPs2LineColor(float x1, float y1, float x2, float y2, float thickness, const float *color)
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	float adx = dx < 0.0f ? -dx : dx;
	float ady = dy < 0.0f ? -dy : dy;
	float half = thickness * 0.5f;

	if (ady < 0.01f)
	{
		EFFe_DrawPs2RectColor(x1 < x2 ? x1 : x2, y1 - half, adx + thickness, thickness, color);
		return;
	}
	if (adx < 0.01f)
	{
		EFFe_DrawPs2RectColor(x1 - half, y1 < y2 ? y1 : y2, thickness, ady + thickness, color);
		return;
	}

	{
		int i;
		int steps = (int)(((adx > ady ? adx : ady) / 3.0f) + 1.0f);
		for (i = 0; i <= steps; ++i)
		{
			float t = (float)i / (float)steps;
			EFFe_DrawPs2RectColor(x1 + dx * t - half, y1 + dy * t - half,
				thickness, thickness, color);
		}
	}
}

static void EFFe_DrawPs2PanelBracket(float x, float y, float w, qboolean rightSide, qboolean lowerHalf)
{
	const float bracketH = 240.0f;
	const float capH = 60.0f;
	const float stemStart = 31.0f;
	const float stemW = 24.0f;
	const float upperStemExtra = 2.0f;
	float stemX = rightSide ? (x + w - stemW) : x;

	if (lowerHalf)
	{
		if (rightSide)
		{
			EFFe_DrawPs2PicFlipXYColor(x, y + bracketH - capH, w, capH, s_assets.panelCorner, s_ps2StripPurple);
		}
		else
		{
			EFFe_DrawPs2PicFlipYColor(x, y + bracketH - capH, w, capH, s_assets.panelCorner, s_ps2StripPurple);
		}
		EFFe_DrawPs2RectColor(stemX, y, stemW, bracketH - stemStart, s_ps2StripPurple);
	}
	else
	{
		if (rightSide)
		{
			EFFe_DrawPs2PicFlipXColor(x, y, w, capH, s_assets.panelCorner, s_ps2StripPurple);
		}
		else
		{
			EFFe_DrawPs2PicColor(x, y, w, capH, s_assets.panelCorner, s_ps2StripPurple);
		}
		EFFe_DrawPs2RectColor(stemX, y + stemStart, stemW, bracketH - stemStart + upperStemExtra, s_ps2StripPurple);
	}
}

static const char *EFFe_ScreenName(efFrontendScreen_t screen)
{
	switch (screen)
	{
	case EF_SCREEN_MAIN:
		return "main";
	case EF_SCREEN_NEWGAME:
		return "newgame";
	case EF_SCREEN_LOADGAME:
		return "loadgame";
	case EF_SCREEN_SAVEGAME:
		return "savegame";
	case EF_SCREEN_COOP:
		return "coop";
	case EF_SCREEN_COOP_NEW:
		return "coop-new";
	case EF_SCREEN_HOLOMATCH:
		return "holomatch";
	case EF_SCREEN_HOLOMATCH_ADVANCED:
		return "holomatch-advanced";
	case EF_SCREEN_HOLOMATCH_PLAYERS:
		return "holomatch-players";
	case EF_SCREEN_CREW:
		return "crew";
	case EF_SCREEN_CREW_ROSTER:
		return "crew-roster";
	case EF_SCREEN_CREW_BIO:
		return "crew-bio";
	case EF_SCREEN_CREW_HAZARD_SUIT:
		return "crew-hazard-suit";
	case EF_SCREEN_CREW_VOYAGER:
		return "crew-voyager";
	case EF_SCREEN_CONFIGURE:
		return "configure";
	case EF_SCREEN_AUDIO:
		return "audio";
	case EF_SCREEN_VIDEO:
		return "video";
	case EF_SCREEN_CONTROLLER:
		return "controller";
	case EF_SCREEN_STUB:
		return "stub";
	default:
		return "unknown";
	}
}

static qboolean EFFe_IsAcceptKey(int key)
{
	return key == A_ENTER || key == A_KP_ENTER || key == A_MOUSE1 || key == A_JOY15;
}

static qboolean EFFe_IsBackKey(int key)
{
	return key == A_ESCAPE || key == A_MOUSE2 || key == A_JOY13 || key == A_JOY14 || key == A_BACKSPACE;
}

static qboolean EFFe_IsUpKey(int key)
{
	return key == A_CURSOR_UP || key == A_JOY5;
}

static qboolean EFFe_IsDownKey(int key)
{
	return key == A_CURSOR_DOWN || key == A_JOY7;
}

static qboolean EFFe_IsLeftKey(int key)
{
	return key == A_CURSOR_LEFT || key == A_JOY8;
}

static qboolean EFFe_IsRightKey(int key)
{
	return key == A_CURSOR_RIGHT || key == A_JOY6;
}

static float EFFe_Clamp01(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	if (value > 1.0f)
	{
		return 1.0f;
	}
	return value;
}

static int EFFe_ClampInt(int value, int minimum, int maximum)
{
	if (value < minimum)
	{
		return minimum;
	}
	if (value > maximum)
	{
		return maximum;
	}
	return value;
}

static void EFFe_DrawPromptLine(float x, float y, qhandle_t icon, const char *label)
{
	if (icon)
	{
		EFFe_DrawPic(x, y, 18.0f, 18.0f, icon, CT_WHITE);
	}
	EFFe_DrawTextScaledXYColor(x + 27.0f, y - 1.0f, ":", EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.55f, 0.78f);
	EFFe_DrawTextScaledXYColor(x + 43.0f, y - 1.0f, label, EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.55f, 0.78f);
}

static void EFFe_DrawPs2PromptIcon(float x, float y, qhandle_t icon)
{
	if (icon)
	{
		EFFe_DrawPs2Pic(x, y, 60.0f, 58.0f, icon, CT_WHITE);
	}
}

static void EFFe_DrawPromptLabel(float x, float y, const char *label)
{
	EFFe_DrawPs2TextColor(x, y, ":", EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.68f, 1.02f);
	EFFe_DrawPs2TextColor(x + 44.0f, y, label, EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.68f, 1.02f);
}

static void EFFe_DrawPanelCode(float x, float y, const char *code)
{
	EFFe_DrawPs2TextColor(x, y, code, EF_FRONTEND_FONT_TINY, UI_RIGHT, colorTable[CT_BLACK], 0.82f, 1.04f);
}

static void EFFe_DrawPromptTopSelectOnly(void)
{
	EFFe_DrawPs2PromptIcon(1385.0f, 68.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1460.0f, 80.0f, "Select");
}

static void EFFe_DrawPromptTopSelectBack(void)
{
	EFFe_DrawPs2PromptIcon(1385.0f, 40.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1460.0f, 52.0f, "Select");
	EFFe_DrawPs2PromptIcon(1385.0f, 96.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1460.0f, 108.0f, "Back");
}

static void EFFe_DrawPromptTopAcceptCancel(void)
{
	EFFe_DrawPs2PromptIcon(1385.0f, 40.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1460.0f, 52.0f, "Accept");
	EFFe_DrawPs2PromptIcon(1385.0f, 96.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1460.0f, 108.0f, "Cancel");
}

static void EFFe_DrawControllerAcceptCancelPrompt(void)
{
	EFFe_DrawPs2PromptIcon(1215.0f, 58.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1288.0f, 70.0f, "Accept");
	EFFe_DrawPs2PromptIcon(1510.0f, 58.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1583.0f, 70.0f, "Cancel");
}

static void EFFe_DrawLoadPrompt(void)
{
	EFFe_DrawPs2PromptIcon(690.0f, 586.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(765.0f, 598.0f, "Select");
	EFFe_DrawPs2PromptIcon(995.0f, 586.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1070.0f, 598.0f, "Back");
}

static void EFFe_DrawSavePrompt(void)
{
	EFFe_DrawPs2PromptIcon(690.0f, 586.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(765.0f, 598.0f, s_saveOverwritePending ? "Overwrite" : "Save");
	EFFe_DrawPs2PromptIcon(995.0f, 586.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1070.0f, 598.0f, s_saveOverwritePending ? "Cancel" : "Back");
}

static void EFFe_DrawVideoPrompt(void)
{
	EFFe_DrawPs2PromptIcon(775.0f, 610.0f, s_assets.xboxX);
	EFFe_DrawPromptLabel(850.0f, 622.0f, "Switch Corners");
	EFFe_DrawPs2PromptIcon(570.0f, 690.0f, s_assets.xboxB);
	EFFe_DrawPromptLabel(645.0f, 702.0f, "Default");
	EFFe_DrawPs2PromptIcon(895.0f, 690.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(975.0f, 702.0f, "Accept");
	EFFe_DrawPs2PromptIcon(1215.0f, 690.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1300.0f, 702.0f, "Cancel");
}

static void EFFe_DrawPs2TextColor(float x, float y, const char *text, int fontIndex, int style, const float *color, float xScale, float yScale)
{
	EFFe_DrawTextScaledXYColor(EF_PS2_X(x), EF_PS2_Y(y), text, fontIndex, style, color, xScale, yScale);
}

static void EFFe_DrawTitleText(float x, float y, const char *title)
{
	EFFe_DrawPs2TextColor(x, y, title, EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2Gold, 1.06f, 1.20f);
}

static void EFFe_DrawMenuText(float x, float y, const char *text, const float *color)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_BIG, UI_LEFT, color, 0.82f, 1.00f);
}

static void EFFe_DrawSmallMenuText(float x, float y, const char *text, const float *color)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_MEDIUM, UI_LEFT, color, 0.92f, 1.03f);
}

static void EFFe_DrawHeaderText(float x, float y, const char *text)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2SelectedText, 0.82f, 1.00f);
}

static void EFFe_DrawMainTopLeftChrome(void)
{
	EFFe_DrawPs2Pic(181.0f, 23.0f, 449.0f, 318.0f, s_assets.ps2MainTopLeftChrome, CT_WHITE);
	EFFe_DrawPs2RectColor(373.0f, 154.0f, 256.0f, 30.0f, s_ps2TopPurple);
}

static void EFFe_DrawMainTopChrome(const char *title, qboolean backPrompt)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);

	EFFe_DrawMainTopLeftChrome();
	EFFe_DrawPs2RectColor(640.0f, 154.0f, 35.0f, 30.0f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(685.0f, 169.0f, 255.0f, 15.0f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(948.0f, 154.0f, 520.0f, 30.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1475.0f, 154.0f, 249.0f, 30.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(641.0f, 195.0f, 34.0f, 32.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(685.0f, 195.0f, 255.0f, 15.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(948.0f, 196.0f, 520.0f, 31.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(1475.0f, 196.0f, 249.0f, 31.0f, s_ps2LightBrown);
	EFFe_DrawPicColor(EF_PS2_X(181), EF_PS2_Y(341), EF_PS2_W(192), EF_PS2_H(393), s_assets.whiteShader, s_ps2LightBrown);
	EFFe_DrawPicColor(EF_PS2_X(181), EF_PS2_Y(742), EF_PS2_W(192), EF_PS2_H(138), s_assets.whiteShader, s_ps2DarkBrown);
	EFFe_DrawTitleText(435.0f, 66.0f, title);
	if (backPrompt)
	{
		EFFe_DrawPromptTopSelectBack();
	}
	else
	{
		EFFe_DrawPromptTopSelectOnly();
	}
}

static void EFFe_DrawLoadFrame(void)
{
	EFFe_DrawPs2RectColor(199.0f, 49.0f, 1238.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(199.0f, 49.0f, 77.0f, 262.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(199.0f, 317.0f, 77.0f, 376.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(199.0f, 699.0f, 77.0f, 107.0f, s_ps2MutedPurple);
	EFFe_DrawPs2PicColor(198.0f, 806.0f, 120.0f, 90.0f, s_assets.ps2LoadBottomLeftChrome, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1445.0f, 49.0f, 175.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2PicColor(1620.0f, 49.0f, 101.0f, 142.0f, s_assets.ps2LoadTopRightChrome, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1644.0f, 192.0f, 77.0f, 63.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1644.0f, 261.0f, 77.0f, 576.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(318.0f, 837.0f, 792.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1117.0f, 837.0f, 527.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1644.0f, 837.0f, 77.0f, 34.0f, s_ps2MutedPurple);
}

static void EFFe_DrawUtilityCodes(const char *title)
{
	const char *codeTop;
	const char *codeMid;
	const char *codeStrip;
	const char *codeBottom;

	codeTop = "5-0987";
	codeMid = "16116";
	codeStrip = "28430";
	codeBottom = "1701-8";
	if (title && !Q_stricmp(title, "ELITE FORCE : AUDIO"))
	{
		codeTop = "1176";
		codeMid = "9214";
		codeStrip = "2510-81";
		codeBottom = "1001001";
	}
	else if (title && !Q_stricmp(title, "ELITE FORCE : ADJUST SCREEN SIZE"))
	{
		codeTop = "207";
		codeMid = "44909";
		codeStrip = "357";
		codeBottom = "456730-1";
	}

	EFFe_DrawPanelCode(452.0f, 62.0f, codeTop);
	EFFe_DrawPanelCode(452.0f, 325.0f, codeMid);
	EFFe_DrawPanelCode(452.0f, 382.0f, codeStrip);
	EFFe_DrawPanelCode(452.0f, 778.0f, codeBottom);
}

static void EFFe_DrawUtilityChrome(const char *title, qboolean acceptCancel, qboolean drawTopPrompts)
{
	const float *leftTallColor = acceptCancel ? s_ps2StripPurple : s_ps2LightBrown;

	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);

	EFFe_DrawPs2RectColor(236.0f, 47.0f, 235.0f, 222.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(236.0f, 276.0f, 235.0f, 99.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(236.0f, 379.0f, 235.0f, 357.0f, leftTallColor);
	EFFe_DrawPs2Pic(235.0f, 742.0f, 318.0f, 119.0f, s_assets.ps2UtilityBottomLeftChrome, CT_WHITE);
	EFFe_DrawPs2RectColor(236.0f, 742.0f, 228.0f, 50.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(326.0f, 792.0f, 223.0f, 66.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(496.0f, 189.0f, 1051.0f, 31.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1552.0f, 189.0f, 110.0f, 31.0f, s_ps2MutedPurple);
	EFFe_DrawPs2Pic(1551.0f, 189.0f, 144.0f, 42.0f, s_assets.ps2UtilityTopRightChrome, CT_WHITE);
	EFFe_DrawPs2RectColor(1552.0f, 189.0f, 110.0f, 31.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1566.0f, 234.0f, 128.0f, 540.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(558.0f, 792.0f, 794.0f, 66.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1359.0f, 792.0f, 335.0f, 66.0f, s_ps2LightBrown);

	EFFe_DrawUtilityCodes(title);

	if (title && !Q_stricmp(title, "ELITE FORCE : ADJUST SCREEN SIZE"))
	{
		EFFe_DrawPs2TextColor(531.0f, 89.0f, title, EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2Gold, 1.05f, 1.20f);
	}
	else
	{
		EFFe_DrawTitleText(531.0f, 89.0f, title);
	}
	if (!drawTopPrompts)
	{
		return;
	}
	if (acceptCancel)
	{
		EFFe_DrawPromptTopAcceptCancel();
	}
	else
	{
		EFFe_DrawPromptTopSelectBack();
	}
}

static void EFFe_DrawMainChildChrome(const char *title, qboolean backPrompt)
{
	EFFe_DrawMainTopChrome(title, backPrompt);
}

static void EFFe_DrawRoundButton(float capX, float x, float y, float w, float h, const char *label, const float *fill, const float *textColor)
{
	float capW = x - capX;
	if (capW < 126.0f)
	{
		capW = 126.0f;
	}
	EFFe_DrawPs2PicColor(capX, y, capW, h, s_assets.buttonLeftEnd, fill);
	EFFe_DrawPs2PicColor(x, y, w, h, s_assets.buttonRight, fill);
	if (label && label[0])
	{
		EFFe_DrawMenuText(x + 25.0f, EFFe_CenteredPs2TextY(y, h, EF_FRONTEND_FONT_BIG, 1.0f), label, textColor);
	}
}

static void EFFe_DrawRectButton(float x, float y, float w, float h, const char *label, const float *fill, const float *textColor)
{
	EFFe_DrawPs2RectColor(x, y, w, h, fill);
	if (label && label[0])
	{
		EFFe_DrawMenuText(x + 32.0f, EFFe_CenteredPs2TextY(y, h, EF_FRONTEND_FONT_BIG, 1.0f), label, textColor);
	}
}

static void EFFe_DrawRightRoundButton(float x, float y, float w, float h, const char *label, const float *fill, const float *textColor)
{
	EFFe_DrawPs2RectColor(x, y, w - 54.0f, h, fill);
	EFFe_DrawPs2PicColor(x, y, w, h, s_assets.buttonRight, fill);
	if (label && label[0])
	{
		EFFe_DrawMenuText(x + 34.0f, EFFe_CenteredPs2TextY(y, h, EF_FRONTEND_FONT_BIG, 1.0f), label, textColor);
	}
}

static void EFFe_DrawNewGameLayout(qboolean cooperative)
{
	int i;
	const char *difficulty[4] = { "EASY", "NORMAL", "CHALLENGING", "DIFFICULT" };
	const char *gender[2] = { "FEMALE", "MALE" };
	const char *characters[2] = { "ALEXANDRIA", "MUNRO" };
	const char **choices = cooperative ? characters : gender;

	EFFe_DrawMainChildChrome(cooperative ? "COOPERATIVE : NEW GAME" : "ELITE FORCE : NEW GAME", qtrue);
	EFFe_DrawPs2RectColor(181.0f, 341.0f, 192.0f, 393.0f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(181.0f, 742.0f, 192.0f, 138.0f, s_ps2DarkBrown);
	EFFe_DrawPanelCode(358.0f, 391.0f, "45");
	EFFe_DrawPanelCode(358.0f, 453.0f, "7688200");
	EFFe_DrawPanelCode(358.0f, 798.0f, "9955");

	EFFe_DrawRectButton(415.0f, 238.0f, 513.0f, 83.0f, "GAME DIFFICULTY", s_ps2DeepPurple, s_ps2SelectedText);
	for (i = 0; i < 4; i++)
	{
		qboolean selected = (s_cursor == i);
		qboolean active = (s_newgameDifficulty == i);
		EFFe_DrawRightRoundButton(415.0f, 332.0f + (float)i * 74.0f, 513.0f, 66.0f,
			difficulty[i],
			selected ? s_ps2ButtonSelected : (active ? s_ps2BrightPurple : s_ps2ButtonPurple),
			selected || active ? s_ps2SelectedText : colorTable[CT_BLACK]);
	}

	EFFe_DrawRectButton(415.0f, 631.0f, 513.0f, 85.0f, cooperative ? "PLAYER 1" : "GENDER", s_ps2DeepPurple, s_ps2SelectedText);
	for (i = 0; i < 2; i++)
	{
		qboolean selected = (s_cursor == i + 4);
		qboolean active = (s_newgameGenderMale ? i == 1 : i == 0);
		EFFe_DrawRightRoundButton(415.0f, 725.0f + (float)i * 75.0f, 513.0f, 66.0f,
			choices[i],
			selected || active ? s_ps2BrightPurple : s_ps2ButtonPurple,
			selected || active ? s_ps2SelectedText : colorTable[CT_BLACK]);
	}

	EFFe_DrawPs2PicSTColor(954.0f, 237.0f, 168.0f, 639.0f, 0.0f, 0.0f, 0.5f, 1.0f, s_assets.warpCore, colorTable[CT_WHITE]);
	EFFe_DrawRoundButton(1197.0f, 1292.0f, 313.0f, 425.0f, 94.0f,
		cooperative ? (s_newgameGenderMale ? "P2 : ALEXANDRIA" : "P2 : MUNRO") : "TUTORIAL",
		s_cursor == 6 ? s_ps2ButtonSelected : s_ps2ButtonPurple,
		colorTable[CT_BLACK]);
	if (!cooperative)
	{
		EFFe_DrawRoundButton(1197.0f, 1292.0f, 429.0f, 425.0f, 94.0f,
			"CAMPAIGN", s_cursor == 7 ? s_ps2ButtonSelected : s_ps2ButtonPurple, colorTable[CT_BLACK]);
	}
	EFFe_DrawPs2PicColor(1115.0f, 552.0f, 606.0f, 66.0f, s_assets.buttonRight, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(1115.0f, 585.0f, 606.0f, 22.0f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(1115.0f, 607.0f, 606.0f, 12.0f, colorTable[CT_BLACK]);
	EFFe_DrawPs2RectColor(1292.0f, 618.0f, 429.0f, 205.0f,
		s_cursor == (cooperative ? 7 : 8) ? s_ps2ButtonSelected : s_ps2ButtonPurple);
	if (cooperative)
	{
		EFFe_DrawPs2TextColor(1327.0f, 737.0f, "ENGAGE", EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_BLACK], 0.82f, 1.00f);
	}
	else
	{
		EFFe_DrawPs2TextColor(1327.0f, 695.0f, "VIRTUAL", EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_BLACK], 0.82f, 1.00f);
		EFFe_DrawPs2TextColor(1327.0f, 758.0f, "VOYAGER", EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_BLACK], 0.82f, 1.00f);
	}
}

static void EFFe_DrawNewGameScreen(void)
{
	EFFe_DrawNewGameLayout(qfalse);
}

static void EFFe_DrawLoadGameScreen(void)
{
	int i;
	int first;
	char comment[34];
	char detail[64];
	const char *title = s_loadForCoop ? "COOPERATIVE : LOAD GAME" : "ELITE FORCE : LOAD GAME";

	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawLoadFrame();
	EFFe_DrawPs2TextColor(330.0f, 105.0f, title, EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2Gold, 0.92f, 1.08f);

	if (s_loadCount <= 0)
	{
		EFFe_DrawPs2RectColor(535.0f, 260.0f, 850.0f, 300.0f, s_ps2DialogGray);
		EFFe_DrawPs2RectColor(546.0f, 271.0f, 828.0f, 278.0f, colorTable[CT_BLACK]);
		EFFe_DrawPs2TextColor(960.0f, 370.0f, "NO SAVED GAMES FOUND", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2MapGold, 0.74f, 1.00f);
	}
	else
	{
		first = s_cursor - 2;
		if (first < 0)
		{
			first = 0;
		}
		if (first > s_loadCount - 6)
		{
			first = s_loadCount - 6;
		}
		if (first < 0)
		{
			first = 0;
		}
		for (i = first; i < s_loadCount && i < first + 6; ++i)
		{
			const float *fill = i == s_cursor ? s_ps2ButtonSelected : s_ps2ButtonPurple;
			const float *textColor = UI_EFSave_IsCorrupt(i) ? s_ps2AudioGray : colorTable[CT_BLACK];
			Q_strncpyz(comment, UI_EFSave_Comment(i), sizeof(comment));
			Com_sprintf(detail, sizeof(detail), "%s  %s", UI_EFSave_Map(i), UI_EFSave_IsCorrupt(i) ? "CORRUPT" : UI_EFSave_Name(i));
			EFFe_DrawRightRoundButton(360.0f, 215.0f + (float)(i - first) * 93.0f, 1190.0f, 76.0f,
				comment[0] ? comment : UI_EFSave_Name(i), fill, textColor);
			EFFe_DrawPs2TextColor(405.0f, 267.0f + (float)(i - first) * 93.0f, detail,
				EF_FRONTEND_FONT_TINY, UI_LEFT, i == s_cursor ? s_ps2SelectedText : s_ps2MapGold, 0.82f, 1.0f);
		}
	}
	EFFe_DrawLoadPrompt();
}

static void EFFe_DrawSaveGameScreen(void)
{
	int i;
	int first;
	int total = s_saveCount + 1;
	char comment[34];
	char detail[80];

	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawLoadFrame();
	EFFe_DrawPs2TextColor(330.0f, 105.0f, "ELITE FORCE : SAVE GAME", EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2Gold, 0.92f, 1.08f);

	first = s_cursor - 2;
	if (first < 0)
	{
		first = 0;
	}
	if (first > total - 6)
	{
		first = total - 6;
	}
	if (first < 0)
	{
		first = 0;
	}

	for (i = first; i < total && i < first + 6; ++i)
	{
		const float *fill = i == s_cursor ? s_ps2ButtonSelected : s_ps2ButtonPurple;
		const float *textColor = colorTable[CT_BLACK];
		if (i == 0)
		{
			Q_strncpyz(comment, "CREATE NEW SAVE", sizeof(comment));
			Q_strncpyz(detail, "FIRST AVAILABLE SAVE SLOT", sizeof(detail));
		}
		else
		{
			int saveIndex = i - 1;
			Q_strncpyz(comment, UI_EFSave_Comment(saveIndex), sizeof(comment));
			if (!comment[0])
			{
				Q_strncpyz(comment, UI_EFSave_Name(saveIndex), sizeof(comment));
			}
			if (s_saveOverwritePending && i == s_cursor)
			{
				Com_sprintf(detail, sizeof(detail), "PRESS A AGAIN TO OVERWRITE  %s", UI_EFSave_Name(saveIndex));
			}
			else
			{
				Com_sprintf(detail, sizeof(detail), "%s  %s", UI_EFSave_Map(saveIndex), UI_EFSave_Name(saveIndex));
			}
			if (UI_EFSave_IsCorrupt(saveIndex))
			{
				textColor = s_ps2AudioGray;
			}
		}

		EFFe_DrawRightRoundButton(360.0f, 215.0f + (float)(i - first) * 93.0f, 1190.0f, 76.0f,
			comment, fill, textColor);
		EFFe_DrawPs2TextColor(405.0f, 267.0f + (float)(i - first) * 93.0f, detail,
			EF_FRONTEND_FONT_TINY, UI_LEFT, i == s_cursor ? s_ps2SelectedText : s_ps2MapGold, 0.82f, 1.0f);
	}

	EFFe_DrawSavePrompt();
}

static void EFFe_DrawCoopScreen(void)
{
	int i;
	EFFe_DrawFrame("ELITE FORCE : COOPERATIVE", qtrue);
	for (i = 0; i < EF_FRONTEND_COOP_COUNT; ++i)
	{
		EFFe_DrawButton(&s_buttons[i], i);
	}
}

static void EFFe_DrawCoopNewScreen(void)
{
	EFFe_DrawNewGameLayout(qtrue);
}

static void EFFe_DrawHolomatchBottomChrome(const float *leftColor)
{
	EFFe_DrawPs2PicColor(102.0f, 728.0f, 378.0f, 116.0f, s_assets.ps2UtilityBottomLeftChrome, leftColor);
	EFFe_DrawPs2RectColor(102.0f, 728.0f, 279.0f, 116.0f, leftColor);
	EFFe_DrawPs2RectColor(480.0f, 780.0f, 948.0f, 64.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1431.0f, 780.0f, 399.0f, 64.0f, s_ps2LightBrown);
}

static void EFFe_DrawHolomatchPrompts(qboolean acceptCancel)
{
	EFFe_DrawPs2PromptIcon(1464.0f, 66.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1536.0f, 78.0f, acceptCancel ? "Accept" : "Select");
	EFFe_DrawPs2PromptIcon(1464.0f, 120.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1536.0f, 132.0f, acceptCancel ? "Cancel" : "Back");
}

static void EFFe_DrawHolomatchSetupChrome(void)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawPs2RectColor(102.0f, 47.0f, 279.0f, 280.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(102.0f, 332.0f, 279.0f, 39.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(102.0f, 375.0f, 279.0f, 349.0f, s_ps2LightBrown);
	EFFe_DrawHolomatchBottomChrome(s_ps2DarkBrown);

	EFFe_DrawPs2RectColor(423.0f, 187.0f, 1407.0f, 39.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(423.0f, 230.0f, 177.0f, 315.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(423.0f, 548.0f, 171.0f, 82.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(600.0f, 548.0f, 666.0f, 82.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1278.0f, 548.0f, 552.0f, 82.0f, s_ps2MutedPurple);

	EFFe_DrawTitleText(453.0f, 91.0f, "ELITE FORCE : HOLOMATCH");
	EFFe_DrawHolomatchPrompts(qfalse);
	EFFe_DrawPanelCode(375.0f, 58.0f, "111611");
	EFFe_DrawPanelCode(375.0f, 344.0f, "81454");
	EFFe_DrawPanelCode(375.0f, 390.0f, "11");
	EFFe_DrawPanelCode(375.0f, 755.0f, "345-5");
}

static void EFFe_DrawHolomatchAdvancedChrome(void)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawPs2RectColor(102.0f, 47.0f, 279.0f, 322.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(102.0f, 373.0f, 279.0f, 351.0f, s_ps2StripPurple);
	EFFe_DrawHolomatchBottomChrome(s_ps2DarkBrown);

	EFFe_DrawPs2RectColor(408.0f, 185.0f, 1422.0f, 41.0f, s_ps2MutedPurple);
	EFFe_DrawPs2PicColor(1656.0f, 185.0f, 174.0f, 43.0f, s_assets.ps2UtilityTopRightChrome, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1680.0f, 234.0f, 150.0f, 526.0f, s_ps2MutedPurple);

	EFFe_DrawTitleText(453.0f, 91.0f, "ELITE FORCE : ADVANCED");
	EFFe_DrawHolomatchPrompts(qtrue);
	EFFe_DrawPanelCode(375.0f, 58.0f, "7617");
	EFFe_DrawPanelCode(375.0f, 344.0f, "4396");
	EFFe_DrawPanelCode(375.0f, 390.0f, "81453");
	EFFe_DrawPanelCode(375.0f, 755.0f, "93433");
}

static void EFFe_DrawHolomatchRoundRow(float x, float y, float w, float h, const char *label, const float *fill, const float *textColor)
{
	const float capW = 66.0f;
	EFFe_DrawPs2PicColor(x, y, capW, h, s_assets.buttonLeftEnd, fill);
	EFFe_DrawPs2PicColor(x + 39.0f, y, w - 39.0f, h, s_assets.buttonRight, fill);
	EFFe_DrawMenuText(x + 90.0f, EFFe_CenteredPs2TextY(y, h, EF_FRONTEND_FONT_BIG, 1.0f), label, textColor);
}

static void EFFe_DrawHolomatchScreen(void)
{
	qhandle_t levelshot;
	char value[64];
	const float *playersFill;
	const float *levelFill;

	EFFe_DrawHolomatchSetupChrome();
	playersFill = s_cursor == 0 ? s_ps2ButtonSelected : s_ps2ButtonPurple;
	levelFill = s_cursor == 1 ? s_ps2ButtonSelected : s_ps2ButtonPurple;
	EFFe_DrawRectButton(627.0f, 241.0f, 447.0f, 62.0f, "PLAYERS", playersFill,
		s_cursor == 0 ? s_ps2SelectedText : colorTable[CT_BLACK]);
	Com_sprintf(value, sizeof(value), "%d", s_holomatchPlayers);
	EFFe_DrawPs2TextColor(672.0f, 319.0f, value, EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2MapGold, 0.82f, 1.0f);
	EFFe_DrawRectButton(627.0f, 375.0f, 447.0f, 62.0f, "LEVEL", levelFill,
		s_cursor == 1 ? s_ps2SelectedText : colorTable[CT_BLACK]);
	EFFe_DrawPs2TextColor(672.0f, 453.0f, s_holomatchMapTitles[s_holomatchMap], EF_FRONTEND_FONT_BIG,
		UI_LEFT, s_cursor == 1 ? s_ps2MapGold : s_ps2SelectedText, 0.82f, 1.0f);

	if (!s_holomatchLevelshots[s_holomatchMap])
	{
		s_holomatchLevelshots[s_holomatchMap] = ui.R_RegisterShaderNoMip(va("levelshots/%s", s_holomatchMapNames[s_holomatchMap]));
	}
	levelshot = s_holomatchLevelshots[s_holomatchMap];
	EFFe_DrawPs2PicColor(1170.0f, 231.0f, 72.0f, 58.0f, s_assets.panelCorner, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(1170.0f, 260.0f, 24.0f, 240.0f, s_ps2StripPurple);
	EFFe_DrawPs2PicFlipYColor(1170.0f, 490.0f, 72.0f, 58.0f, s_assets.panelCorner, s_ps2StripPurple);
	EFFe_DrawPs2PicFlipXColor(1698.0f, 231.0f, 72.0f, 58.0f, s_assets.panelCorner, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(1746.0f, 260.0f, 24.0f, 240.0f, s_ps2StripPurple);
	EFFe_DrawPs2PicFlipXYColor(1698.0f, 490.0f, 72.0f, 58.0f, s_assets.panelCorner, s_ps2StripPurple);
	EFFe_DrawPs2Pic(1200.0f, 248.0f, 543.0f, 282.0f, levelshot, CT_WHITE);
	EFFe_DrawPs2TextColor(1314.0f, 579.0f, s_holomatchMapNames[s_holomatchMap], EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_BLACK], 0.9f, 1.0f);
	EFFe_DrawHolomatchRoundRow(489.0f, 659.0f, 585.0f, 65.0f, "ADVANCED",
		s_cursor == 2 ? s_ps2ButtonSelected : s_ps2ButtonPurple, colorTable[CT_BLACK]);
	EFFe_DrawRectButton(1224.0f, 644.0f, 564.0f, 121.0f, "ENGAGE",
		s_cursor == 3 ? s_ps2ButtonSelected : s_ps2ButtonPurple, colorTable[CT_BLACK]);
}

static const char *EFFe_OnOff(int value)
{
	return value ? "ON" : "OFF";
}

static void EFFe_DrawHolomatchAdvancedScreen(void)
{
	const char *labels[EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT] = {
		"POINT LIMIT", "TIME LIMIT", "FORCE RESPAWN", "WEAPON STAY",
		"FALLING DAMAGE", "TEAM PLAY", "FRIENDLY FIRE"
	};
	int i;
	const float rowY[EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT] = {
		244.0f, 317.0f, 390.0f, 465.0f, 538.0f, 612.0f, 687.0f
	};
	char values[EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT][32];

	Q_strncpyz(values[0], s_holomatchPointLimit ? va("%d POINTS", s_holomatchPointLimit) : "UNLIMITED", sizeof(values[0]));
	Q_strncpyz(values[1], s_holomatchTimeLimit ? va("%d MINUTES", s_holomatchTimeLimit) : "UNLIMITED", sizeof(values[1]));
	Q_strncpyz(values[2], EFFe_OnOff(s_holomatchForceRespawn), sizeof(values[2]));
	Q_strncpyz(values[3], EFFe_OnOff(s_holomatchWeaponStay), sizeof(values[3]));
	Q_strncpyz(values[4], EFFe_OnOff(s_holomatchFallingDamage), sizeof(values[4]));
	Q_strncpyz(values[5], EFFe_OnOff(s_holomatchTeamPlay), sizeof(values[5]));
	Q_strncpyz(values[6], EFFe_OnOff(s_holomatchFriendlyFire), sizeof(values[6]));

	EFFe_DrawHolomatchAdvancedChrome();
	for (i = 0; i < EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT; ++i)
	{
		const float *fill = s_cursor == i ? s_ps2ButtonSelected : s_ps2ButtonPurple;
		const float *valueColor = (i == 6 && !s_holomatchTeamPlay) ? s_ps2AudioGray : s_ps2SelectedText;
		float y = rowY[i];
		if (i == 6 && !s_holomatchTeamPlay)
		{
			fill = s_ps2DialogGray;
		}
		EFFe_DrawHolomatchRoundRow(471.0f, y, 654.0f, 64.0f, labels[i], fill,
			s_cursor == i ? s_ps2SelectedText : colorTable[CT_BLACK]);
		EFFe_DrawPs2TextColor(1167.0f, EFFe_CenteredPs2TextY(y, 64.0f, EF_FRONTEND_FONT_BIG, 1.0f),
			values[i], EF_FRONTEND_FONT_BIG, UI_LEFT,
			s_cursor == i && i < 2 ? s_ps2MapGold : valueColor, 0.82f, 1.0f);
	}
}

static const char *EFFe_HolomatchOutfitName(int outfit)
{
	return s_holomatchOutfitNames[(outfit + EF_ARRAY_LEN(s_holomatchOutfitNames)) % EF_ARRAY_LEN(s_holomatchOutfitNames)];
}

static int EFFe_HolomatchCharacterIndexForCrewMember(int crewMember)
{
	int i;
	for (i = 0; i < EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS; ++i)
	{
		if (s_holomatchCharacters[i].crewMember == crewMember)
		{
			return i;
		}
	}
	return 0;
}

static float EFFe_HolomatchPlayerTextScale(const char *text, float preferredScale, float maxPs2Width)
{
	float width = EFFe_TextWidthScaled(text, EF_FRONTEND_FONT_MEDIUM, preferredScale);
	float maxWidth = EF_PS2_W(maxPs2Width);
	if (width > maxWidth && width > 0.0f)
	{
		return preferredScale * maxWidth / width;
	}
	return preferredScale;
}

static qboolean EFFe_HolomatchMenuSmokeUsesVirtualPads(void)
{
	return s_holomatchMenuVirtualPads;
}

static void EFFe_DrawHolomatchPlayerOption(float x, float centerX, float y, const char *label,
	const char *value, qhandle_t valuePic, qboolean selected)
{
	const float *valueColor = selected ? s_ps2MapGold : s_ps2ButtonSelected;
	float labelScale = EFFe_HolomatchPlayerTextScale(label, 0.79f, 350.0f);
	float valueScale = EFFe_HolomatchPlayerTextScale(value, 0.79f, 320.0f);
	EFFe_DrawPs2TextColor(centerX, y, label, EF_FRONTEND_FONT_MEDIUM, UI_CENTER,
		selected ? colorTable[CT_WHITE] : s_ps2SelectedText, labelScale, 1.00f);
	if (value && value[0])
	{
		EFFe_DrawPs2TextColor(centerX, y + 31.0f, value, EF_FRONTEND_FONT_MEDIUM, UI_CENTER,
			valueColor, valueScale, 1.00f);
	}
	if (valuePic)
	{
		EFFe_DrawPs2Pic(centerX - 20.0f, y + 23.0f, 40.0f, 40.0f, valuePic, CT_WHITE);
	}
	EFFe_DrawPs2Pic(x + 29.0f, y + 22.0f, 32.0f, 38.0f, s_assets.leftArrow, CT_WHITE);
	EFFe_DrawPs2Pic(x + 386.0f, y + 22.0f, 32.0f, 38.0f, s_assets.rightArrow, CT_WHITE);
}

static int EFFe_HolomatchPadForPlayer(int player)
{
	if (EFFe_HolomatchMenuSmokeUsesVirtualPads() &&
		player >= 0 && player < s_holomatchPlayers)
	{
		return player;
	}
#ifdef _XBOX
	return CL_STEFX_SplitScreen_PadForLocalSlot(player);
#else
	return player;
#endif
}

static int EFFe_HolomatchHumanPlayerCount(void)
{
	int player;
	int humanPlayers = 0;

	for (player = 0; player < s_holomatchPlayers; ++player)
	{
		if (EFFe_HolomatchPadForPlayer(player) >= 0)
		{
			++humanPlayers;
		}
	}
	return humanPlayers;
}

static void EFFe_DrawHolomatchPlayersScreen(void)
{
	static const char *labels[EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS] = {
		"CHARACTER", "OUTFIT", "CONTROL STYLE", "SWITCH WEAPON",
		"AUTO-AIM", "CROSSHAIR", "VIBRATION", "INVERT PITCH"
	};
	int player;
	int option;

	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	for (player = 0; player < EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS; ++player)
	{
		float x = 77.0f + (float)player * 458.0f;
		float centerX = x + 229.0f;
		int character = s_holomatchPlayerCharacters[player];
		int characterIndex = EFFe_HolomatchCharacterIndexForCrewMember(character);
		int outfit = (s_holomatchPlayerOutfits[player] + EF_ARRAY_LEN(s_holomatchOutfitCvars)) %
			EF_ARRAY_LEN(s_holomatchOutfitCvars);
		qhandle_t portrait = s_assets.holomatchPortraits[characterIndex][outfit];
		qboolean configured = player < s_holomatchPlayers;
		qboolean connected = configured && EFFe_HolomatchPadForPlayer(player) >= 0;
		if (!portrait)
		{
			portrait = s_assets.crewPortraits[character];
		}

		EFFe_DrawPs2RectColor(x, 20.0f, 447.0f, 10.0f, s_ps2ButtonPurple);
		EFFe_DrawPs2RectColor(x, 20.0f, 10.0f, 858.0f, s_ps2ButtonPurple);
		EFFe_DrawPs2RectColor(x + 437.0f, 20.0f, 10.0f, 858.0f, s_ps2ButtonPurple);
		EFFe_DrawPs2RectColor(x, 274.0f, 447.0f, 10.0f, s_ps2ButtonPurple);
		EFFe_DrawPs2RectColor(x, 868.0f, 447.0f, 10.0f, s_ps2ButtonPurple);
		if (!configured)
		{
			continue;
		}
		if (!connected)
		{
			EFFe_DrawPs2TextColor(centerX, 383.0f, "BOT", EF_FRONTEND_FONT_BIG, UI_CENTER, colorTable[CT_WHITE], 0.76f, 1.00f);
			EFFe_DrawPs2TextColor(centerX, 426.0f, "VIEWPORT", EF_FRONTEND_FONT_BIG, UI_CENTER, colorTable[CT_WHITE], 0.76f, 1.00f);
			continue;
		}

		EFFe_DrawPs2Pic(110.0f + (float)player * 458.0f, 38.0f, 382.0f, 190.0f,
			portrait, CT_WHITE);
		EFFe_DrawPs2Pic(x + 43.0f, 224.0f, 48.0f, 46.0f, s_assets.xboxA, CT_WHITE);
		EFFe_DrawPs2TextColor(x + 95.0f, 232.0f, "Ready",
			EF_FRONTEND_FONT_MEDIUM, UI_LEFT,
			s_holomatchPlayerReady[player] ? s_ps2MapGold : colorTable[CT_WHITE], 0.70f, 0.92f);
		if (player == 0)
		{
			EFFe_DrawPs2Pic(x + 247.0f, 224.0f, 48.0f, 46.0f, s_assets.xboxY, CT_WHITE);
			EFFe_DrawPs2TextColor(x + 299.0f, 232.0f, "Back", EF_FRONTEND_FONT_MEDIUM, UI_LEFT,
				colorTable[CT_WHITE], 0.70f, 0.92f);
		}

		for (option = 0; option < EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS; ++option)
		{
			const char *value = NULL;
			qhandle_t valuePic = 0;
			float y = 296.0f + (float)option * 70.0f;
			switch (option)
			{
			case 0: value = s_crewMembers[character].listName; break;
			case 1: value = EFFe_HolomatchOutfitName(s_holomatchPlayerOutfits[player]); break;
			case 2: value = s_holomatchControlStyleNames[s_holomatchPlayerControlStyles[player]]; break;
			case 3: value = s_holomatchAutoswitchNames[s_holomatchPlayerAutoswitch[player]]; break;
			case 4: value = s_holomatchAutoaimNames[s_holomatchPlayerAutoaim[player]]; break;
			case 5: valuePic = s_assets.crosshairs[s_holomatchPlayerCrosshairs[player]]; break;
			case 6: value = s_holomatchPlayerVibration[player] ? "ON" : "OFF"; break;
			case 7: value = s_holomatchPlayerInvertPitch[player] ? "ON" : "OFF"; break;
			}
			EFFe_DrawHolomatchPlayerOption(x, centerX, y, labels[option], value, valuePic,
				option == s_holomatchPlayerCursors[player]);
		}
	}
}

static void EFFe_DrawCrewPrompts(const char *acceptLabel)
{
	/* Keep both rows wholly above the 151-unit roster rule. */
	EFFe_DrawPs2PromptIcon(1435.0f, 18.0f, s_assets.xboxA);
	EFFe_DrawPromptLabel(1510.0f, 30.0f, acceptLabel);
	EFFe_DrawPs2PromptIcon(1435.0f, 74.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1510.0f, 86.0f, "Back");
}

static void EFFe_DrawCrewCategoryChrome(void)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawPs2RectColor(102.0f, 47.0f, 279.0f, 280.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(102.0f, 332.0f, 279.0f, 39.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(102.0f, 375.0f, 279.0f, 349.0f, s_ps2StripPurple);
	EFFe_DrawHolomatchBottomChrome(s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(423.0f, 187.0f, 1407.0f, 39.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1680.0f, 230.0f, 150.0f, 526.0f, s_ps2MutedPurple);
	EFFe_DrawTitleText(453.0f, 91.0f, "ELITE FORCE : CREW");
	EFFe_DrawCrewPrompts("Select");
	EFFe_DrawPanelCode(375.0f, 58.0f, "881");
	EFFe_DrawPanelCode(375.0f, 344.0f, "2445");
	EFFe_DrawPanelCode(375.0f, 390.0f, "431108");
	EFFe_DrawPanelCode(375.0f, 755.0f, "7617");
}

static void EFFe_DrawCrewScreen(void)
{
	static const char *labels[EF_FRONTEND_CREW_CATEGORY_COUNT] = {
		"SENIOR STAFF", "ALPHA SQUAD", "BETA SQUAD", "HAZARD SUIT", "U.S.S. VOYAGER"
	};
	int i;

	EFFe_DrawCrewCategoryChrome();
	for (i = 0; i < 3; ++i)
	{
		EFFe_DrawHolomatchRoundRow(735.0f, 255.0f + (float)i * 121.0f, 714.0f, 101.0f, labels[i],
			i == s_cursor ? s_ps2ButtonSelected : s_ps2ButtonPurple,
			i == s_cursor ? s_ps2SelectedText : colorTable[CT_BLACK]);
	}
	for (i = 3; i < EF_FRONTEND_CREW_CATEGORY_COUNT; ++i)
	{
		EFFe_DrawRectButton(557.0f + (float)(i - 3) * 548.0f, 613.0f, 507.0f, 104.0f, labels[i],
			i == s_cursor ? s_ps2ButtonSelected : s_ps2ButtonPurple,
			i == s_cursor ? s_ps2SelectedText : colorTable[CT_BLACK]);
	}
}

static void EFFe_DrawCrewRosterChrome(const char *title, const char *acceptLabel)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawTitleText(150.0f, 57.0f, title);
	EFFe_DrawCrewPrompts(acceptLabel);
	EFFe_DrawPs2RectColor(38.0f, 151.0f, 817.0f, 31.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(860.0f, 151.0f, 376.0f, 31.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1241.0f, 151.0f, 602.0f, 31.0f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(38.0f, 844.0f, 1120.0f, 31.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1163.0f, 844.0f, 680.0f, 31.0f, s_ps2LightBrown);
}

static void EFFe_DrawCrewMemberList(void)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	int i;
	for (i = 0; i < group->memberCount; ++i)
	{
		int member = group->firstMember + i;
		EFFe_DrawRectButton(42.0f, 194.0f + (float)i * 64.0f, 489.0f, 54.0f,
			s_crewMembers[member].listName,
			i == s_crewMember ? s_ps2ButtonSelected : s_ps2ButtonPurple,
			i == s_crewMember ? s_ps2SelectedText : colorTable[CT_BLACK]);
	}
}

static void EFFe_DrawCrewRosterScreen(void)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	char title[96];
	qhandle_t emblem = s_crewGroup == 0 ? s_assets.crewFederation : s_assets.crewHazard;

	Com_sprintf(title, sizeof(title), "ELITE FORCE : %s", group->title);
	EFFe_DrawCrewRosterChrome(title, "Select");
	EFFe_DrawCrewMemberList();
	EFFe_DrawPs2TextColor(1575.0f, 218.0f, group->title, EF_FRONTEND_FONT_BIG, UI_CENTER,
		s_ps2MapGold, 0.92f, 1.08f);
	EFFe_DrawPs2Pic(900.0f, 302.0f, 620.0f, 410.0f, emblem, CT_WHITE);
}

static int EFFe_WrapCrewBio(const char *text)
{
	char word[128];
	char line[256];
	const char *src = text;
	int count = 0;
	line[0] = '\0';
	memset(s_crewBioLines, 0, sizeof(s_crewBioLines));

	while (src && *src && count < EF_FRONTEND_CREW_BIO_LINES)
	{
		int wordLen = 0;
		char trial[256];
		while (*src == ' ') ++src;
		while (*src && *src != ' ' && wordLen < (int)sizeof(word) - 1) word[wordLen++] = *src++;
		word[wordLen] = '\0';
		if (!word[0]) break;
		Com_sprintf(trial, sizeof(trial), "%s%s%s", line, line[0] ? " " : "", word);
		if (EFFe_TextWidthScaled(trial, EF_FRONTEND_FONT_MEDIUM, 0.86f) > EF_PS2_W(1210.0f) && line[0])
		{
			Q_strncpyz(s_crewBioLines[count++], line, sizeof(s_crewBioLines[0]));
			Q_strncpyz(line, word, sizeof(line));
		}
		else
		{
			Q_strncpyz(line, trial, sizeof(line));
		}
	}
	if (line[0] && count < EF_FRONTEND_CREW_BIO_LINES)
	{
		Q_strncpyz(s_crewBioLines[count++], line, sizeof(s_crewBioLines[0]));
	}
	return count;
}

static void EFFe_DrawCrewBioScreen(void)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	int member = group->firstMember + s_crewMember;
	const efFrontendCrewMember_t *crew = &s_crewMembers[member];
	char title[96];
	int metadataCount = crew->normalTextCount - 2;
	int bioTextIndex = crew->normalTextStart + crew->normalTextCount - 1;
	int lineCount;
	int firstLine;
	int i;

	EFFe_LoadNormalText();
#ifdef _XBOX
	{
		static qboolean s_loggedCrewBioText = qfalse;
		if (!s_loggedCrewBioText)
		{
			XBLog_WriteCriticalf("STEFX_CREW_BIO_DRAW: member=%d start=%d entries=%d loaded=%d count=%d meta0='%s' bioPrefix='%.48s'",
				member, crew->normalTextStart, crew->normalTextCount,
				s_normalTextLoaded ? 1 : 0, s_normalTextCount,
				EFFe_NormalText(crew->normalTextStart + 1, "<missing>"),
				EFFe_NormalText(bioTextIndex, "<missing>"));
			s_loggedCrewBioText = qtrue;
		}
	}
#endif
	Com_sprintf(title, sizeof(title), "ELITE FORCE : %s", group->title);
	EFFe_DrawCrewRosterChrome(title, "Next Page");
	EFFe_DrawCrewMemberList();
	for (i = 0; i < metadataCount; ++i)
	{
		EFFe_DrawPs2TextColor(555.0f, 196.0f + (float)i * 36.0f,
			EFFe_NormalText(crew->normalTextStart + 1 + i, ""), EF_FRONTEND_FONT_MEDIUM,
			UI_LEFT, colorTable[CT_WHITE], 0.86f, 1.00f);
	}
	EFFe_DrawPs2Pic(1520.0f, 198.0f, 260.0f, 169.0f, s_assets.crewPortraits[member], CT_WHITE);
	lineCount = EFFe_WrapCrewBio(EFFe_NormalText(bioTextIndex, ""));
	firstLine = s_crewBioPage * 7;
	for (i = firstLine; i < lineCount && i < firstLine + 7; ++i)
	{
		EFFe_DrawPs2TextColor(555.0f, 415.0f + (float)(i - firstLine) * 47.0f,
			s_crewBioLines[i], EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.86f, 1.00f);
	}
}

static void EFFe_DrawCrewStaticChrome(const char *title)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawTitleText(350.0f, 58.0f, title);
	EFFe_DrawPs2PromptIcon(1435.0f, 72.0f, s_assets.xboxY);
	EFFe_DrawPromptLabel(1510.0f, 84.0f, "Back");
	/* Measured directly from the 1096x811 retail PS2 crew-page capture. */
	EFFe_DrawPs2RectColor(38.5f, 25.5f, 224.3f, 48.0f, s_ps2TopPurple);
	EFFe_DrawPs2PicColor(38.5f, 69.8f, 261.1f, 108.7f, s_assets.pauseCornerUpper, s_ps2TopPurple);
	EFFe_DrawPs2RectColor(131.4f, 154.1f, 436.2f, 29.9f, s_ps2TopPurple);
	EFFe_DrawPs2RectColor(583.4f, 155.2f, 38.5f, 28.8f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(635.9f, 169.6f, 299.6f, 14.4f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(946.0f, 155.2f, 613.0f, 28.8f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1571.4f, 155.2f, 292.5f, 28.8f, s_ps2LightBrown);
	EFFe_DrawPs2PicColor(38.5f, 197.3f, 261.1f, 136.4f, s_assets.pauseCornerUpper2, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(131.4f, 197.3f, 436.2f, 28.8f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(583.4f, 197.3f, 38.5f, 28.8f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(635.9f, 197.3f, 299.6f, 12.2f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(946.0f, 197.3f, 613.0f, 26.6f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(1571.4f, 197.3f, 292.5f, 26.6f, s_ps2LightBrown);
}

static void EFFe_DrawCrewHazardSuitScreen(void)
{
	const float *componentColor = colorTable[CT_LTGOLD1];
	const float *lineColor = colorTable[CT_WHITE];

	EFFe_DrawCrewStaticChrome("ELITE FORCE : HAZARD SUIT");
	EFFe_DrawPs2RectColor(38.5f, 311.5f, 224.3f, 22.2f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(38.5f, 342.5f, 224.3f, 391.3f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(38.5f, 743.8f, 224.3f, 136.4f, s_ps2DarkBrown);
	EFFe_DrawPanelCode(268.0f, 288.0f, "451-05");
	EFFe_DrawPanelCode(268.0f, 343.0f, "452");
	EFFe_DrawPanelCode(268.0f, 747.0f, "57258");
	EFFe_DrawPs2PicColor(440.0f, 357.0f, 1536.0f, 480.0f, s_assets.crewSuit, s_ps2MutedPurple);

	/* Component markers share the suit's exact 3.0x/1.875x authored transform. */
	EFFe_DrawPs2PicColor(623.0f, 531.4f, 96.0f, 60.0f, s_assets.crewSuitPower, componentColor);
	EFFe_DrawPs2PicColor(677.0f, 450.8f, 48.0f, 30.0f, s_assets.crewSuitComm, componentColor);
	EFFe_DrawPs2PicColor(1034.0f, 559.5f, 96.0f, 30.0f, s_assets.crewSuitDirectional, componentColor);
	EFFe_DrawPs2PicColor(1181.0f, 537.0f, 96.0f, 30.0f, s_assets.crewSuitEnergy, componentColor);
	EFFe_DrawPs2PicColor(569.0f, 424.5f, 192.0f, 30.0f, s_assets.crewSuitWave, componentColor);
	EFFe_DrawPs2PicColor(485.0f, 559.5f, 96.0f, 60.0f, s_assets.crewSuitScanner, componentColor);
	EFFe_DrawPs2PicColor(1277.0f, 591.4f, 48.0f, 60.0f, s_assets.crewSuitPouch, componentColor);
	EFFe_DrawPs2PicColor(1226.0f, 533.3f, 48.0f, 30.0f, s_assets.crewSuitBuffer, componentColor);

	/* Vector reconstruction of the retail callout masks at measured bounds. */
	EFFe_DrawPs2LineColor(585.1f, 307.1f, 764.4f, 307.1f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(585.1f, 307.1f, 585.1f, 421.8f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(711.5f, 307.1f, 711.5f, 421.8f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(478.2f, 474.4f, 633.9f, 531.4f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(759.2f, 416.8f, 706.0f, 459.6f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(409.9f, 572.0f, 487.5f, 572.0f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(409.9f, 572.0f, 409.9f, 631.9f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(951.2f, 572.0f, 1028.8f, 572.0f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(951.2f, 572.0f, 951.2f, 631.9f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1370.3f, 495.5f, 1198.2f, 534.8f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1264.8f, 539.8f, 1400.3f, 569.6f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1468.5f, 601.9f, 1298.1f, 641.2f, 4.0f, lineColor);
	EFFe_DrawPs2TextColor(780.0f, 283.0f, "Multi-Phasic Wave Generators", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(307.0f, 388.0f, "Power", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(307.0f, 426.0f, "Converter", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(780.0f, 399.0f, "Comm Badge", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(319.0f, 638.0f, "Passive/Active", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(319.0f, 677.0f, "Scanners", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(867.0f, 638.0f, "Directional", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(867.0f, 677.0f, "Logistics", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1381.0f, 472.0f, "Energy Pack", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1416.0f, 542.0f, "Transporter Buffer", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1477.0f, 594.0f, "Equipment Pouch", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(829.0f, 786.0f, "FRONT", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2Gold, 0.86f, 1.00f);
	EFFe_DrawPs2TextColor(1370.0f, 786.0f, "BACK", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2Gold, 0.86f, 1.00f);
}

static void EFFe_DrawCrewVoyagerScreen(void)
{
	const float *componentColor = colorTable[CT_LTGOLD1];
	const float *lineColor = colorTable[CT_WHITE];

	EFFe_DrawCrewStaticChrome("ELITE FORCE : U.S.S. VOYAGER");
	EFFe_DrawPs2RectColor(38.5f, 311.5f, 224.3f, 22.2f, s_ps2DarkBrown);
	EFFe_DrawPs2RectColor(38.5f, 342.5f, 224.3f, 25.5f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(38.5f, 660.7f, 224.3f, 73.1f, s_ps2StripPurple);
	EFFe_DrawPs2RectColor(38.5f, 743.8f, 224.3f, 136.4f, s_ps2DarkBrown);
	EFFe_DrawPanelCode(268.0f, 325.0f, "4396");
	EFFe_DrawPanelCode(268.0f, 704.0f, "1411");
	EFFe_DrawPanelCode(268.0f, 769.0f, "431108");
	/*
	** The retail capture scales voy_1's visible 586x114-pixel hull to exactly
	** 985x186 capture pixels.  Keep that measured 0.98154x/0.96567x transform
	** here; drawing the 1024x256 source at 1:1 UI pixels clipped the bow and
	** made every system overlay drift away from its callout.
	*/
	EFFe_DrawPs2PicColor(103.8f, 409.3f, 3015.3f, 463.5f, s_assets.crewVoyager, s_ps2MutedPurple);

	/* Gold system overlays share the measured hull transform above. */
	EFFe_DrawPs2PicColor(307.0f, 490.8f, 753.8f, 29.0f, s_assets.crewVoyagerPhaser, componentColor);
	EFFe_DrawPs2PicColor(819.4f, 563.3f, 47.1f, 29.0f, s_assets.crewVoyagerTorpedo, componentColor);
	EFFe_DrawPs2PicColor(987.2f, 619.3f, 47.1f, 29.0f, s_assets.crewVoyagerVentral, componentColor);
	EFFe_DrawPs2PicColor(1066.7f, 525.2f, 94.2f, 14.5f, s_assets.crewVoyagerMidHull, componentColor);
	EFFe_DrawPs2PicColor(1399.5f, 563.3f, 753.8f, 57.9f, s_assets.crewVoyagerNacelles, componentColor);
	EFFe_DrawPs2PicColor(1308.2f, 570.5f, 94.2f, 57.9f, s_assets.crewVoyagerBussard, componentColor);
	EFFe_DrawPs2PicColor(940.1f, 523.4f, 94.2f, 29.0f, s_assets.crewVoyagerRcs, componentColor);
	EFFe_DrawPs2PicColor(751.7f, 442.0f, 94.2f, 57.9f, s_assets.crewVoyagerBridge, componentColor);

	/* Vector reconstruction of the eight retail callouts. */
	EFFe_DrawPs2LineColor(475.0f, 447.0f, 760.0f, 447.0f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(501.1f, 490.0f, 501.1f, 592.5f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(501.1f, 592.5f, 473.0f, 597.5f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(826.6f, 576.4f, 826.6f, 616.2f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(826.6f, 616.2f, 760.3f, 618.1f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1008.0f, 646.3f, 1008.0f, 752.7f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1008.0f, 752.7f, 967.0f, 752.7f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1093.1f, 529.9f, 1093.1f, 736.1f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1093.1f, 736.1f, 1098.0f, 750.4f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1364.7f, 591.9f, 1364.7f, 666.6f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1364.7f, 666.6f, 1452.7f, 674.2f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1699.3f, 401.3f, 1767.2f, 401.3f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1767.2f, 401.3f, 1772.7f, 412.5f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(1772.7f, 412.5f, 1772.7f, 584.2f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(965.3f, 345.9f, 971.6f, 345.9f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(971.6f, 345.9f, 972.0f, 361.3f, 4.0f, lineColor);
	EFFe_DrawPs2LineColor(972.0f, 361.3f, 972.0f, 523.0f, 4.0f, lineColor);
	EFFe_DrawPs2TextColor(480.0f, 306.0f, "RCS (Maneuvering) Thrusters", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(307.0f, 424.0f, "Bridge", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(63.0f, 572.0f, "Forward Phaser", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(538.0f, 598.0f, "Forward", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(359.0f, 638.0f, "Photon Torpedo", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(499.0f, 684.0f, "Launchers", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(422.0f, 748.0f, "Ventral Phaser Strip", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1253.0f, 383.0f, "Warp Nacelles in", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1253.0f, 425.0f, "lowered Position", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1466.0f, 664.0f, "Bussard", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1466.0f, 707.0f, "Collectors", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1063.0f, 755.0f, "Mid-Hull", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
	EFFe_DrawPs2TextColor(1063.0f, 797.0f, "Phaser Strip", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.90f, 1.00f);
}

static void EFFe_DrawStubScreen(void)
{
	const char *title = s_stubTitle[0] ? s_stubTitle : "ELITE FORCE";
	const char *line = s_stubLine[0] ? s_stubLine : "THIS MENU IS NOT AVAILABLE YET";

	EFFe_DrawMainChildChrome(title, qtrue);
	EFFe_DrawPs2RectColor(535.0f, 360.0f, 850.0f, 260.0f, s_ps2DialogGray);
	EFFe_DrawPs2RectColor(546.0f, 371.0f, 828.0f, 238.0f, colorTable[CT_BLACK]);
	EFFe_DrawPs2TextColor(960.0f, 455.0f, "COMING SOON", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2MapGold, 0.82f, 1.00f);
	EFFe_DrawPs2TextColor(960.0f, 520.0f, line, EF_FRONTEND_FONT_MEDIUM, UI_CENTER, colorTable[CT_WHITE], 0.94f, 1.08f);
}

static void EFFe_DrawConfigureScreen(void)
{
	EFFe_DrawUtilityChrome("ELITE FORCE : CONFIGURE", qfalse, qtrue);
	EFFe_DrawRoundButton(723.0f, 829.0f, 294.0f, 513.0f, 104.0f, "AUDIO",
		s_cursor == 0 ? s_ps2ButtonSelected : s_ps2ButtonPurple,
		s_cursor == 0 ? s_ps2SelectedText : colorTable[CT_BLACK]);
	EFFe_DrawRoundButton(723.0f, 829.0f, 425.0f, 513.0f, 104.0f, "VIDEO",
		s_cursor == 1 ? s_ps2ButtonSelected : s_ps2ButtonPurple,
		s_cursor == 1 ? s_ps2SelectedText : colorTable[CT_BLACK]);
	EFFe_DrawRoundButton(723.0f, 829.0f, 556.0f, 513.0f, 104.0f, "CONTROLLER",
		s_cursor == 2 ? s_ps2ButtonSelected : s_ps2ButtonPurple,
		s_cursor == 2 ? s_ps2SelectedText : colorTable[CT_BLACK]);
}

static void EFFe_DrawSlider(float x, float y, float w, float h, float value, qboolean selected)
{
	int i;
	float fillW;
	float knobX;

	EFFe_DrawPs2RectColor(x, y, w, h, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(x + 4.0f, y + 4.0f, w - 8.0f, h - 8.0f, colorTable[CT_BLACK]);
	fillW = (w - 10.0f) * EFFe_Clamp01(value);
	EFFe_DrawPs2RectColor(x + 5.0f, y + 5.0f, fillW, h - 10.0f, selected ? colorTable[CT_WHITE] : s_ps2AudioGray);
	EFFe_DrawPs2PicColor(x + 5.0f, y + 5.0f, w - 10.0f, h - 10.0f, s_assets.monBar2, s_ps2MutedPurple);
	for (i = 1; i < 4; i++)
	{
		EFFe_DrawPs2RectColor(x + (w * (float)i / 4.0f), y + 5.0f, 4.0f, h - 10.0f, s_ps2MutedPurple);
	}
	knobX = x + 5.0f + fillW - 22.0f;
	if (knobX < x + 8.0f)
	{
		knobX = x + 8.0f;
	}
	if (knobX > x + w - 42.0f)
	{
		knobX = x + w - 42.0f;
	}
	EFFe_DrawPs2PicColor(knobX, y - 4.0f, 48.0f, h + 8.0f, s_assets.slider, selected ? s_ps2BrightPurple : s_ps2DeepPurple);
}

static void EFFe_DrawAudioRow(float y, const char *label, float value, int index)
{
	qboolean selected;
	selected = (s_cursor == index);
	EFFe_DrawPs2RectColor(553.0f, y, 475.0f, 76.0f, selected ? s_ps2ButtonSelected : s_ps2ButtonPurple);
	EFFe_DrawMenuText(578.0f, EFFe_CenteredPs2TextY(y, 76.0f, EF_FRONTEND_FONT_BIG, 1.0f),
		label, selected ? s_ps2SelectedText : colorTable[CT_BLACK]);
	if (index < 3)
	{
		EFFe_DrawSlider(1065.0f, y, 425.0f, 76.0f, value, selected);
	}
}

static void EFFe_DrawAudioScreen(void)
{
	EFFe_DrawUtilityChrome("ELITE FORCE : AUDIO", qtrue, qtrue);
	EFFe_DrawAudioRow(281.0f, "EFFECTS VOLUME", s_audioEffects, 0);
	EFFe_DrawAudioRow(378.0f, "MUSIC VOLUME", s_audioMusic, 1);
	EFFe_DrawAudioRow(476.0f, "VOICE VOLUME", s_audioVoice, 2);
	EFFe_DrawAudioRow(573.0f, "SOUND QUALITY", 0.0f, 3);
	EFFe_DrawPs2TextColor(1082.0f,
		EFFe_CenteredPs2TextY(573.0f, 76.0f, EF_FRONTEND_FONT_MEDIUM, 1.20f),
		"Stereo", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 1.08f, 1.20f);
}

static void EFFe_DrawVideoScreen(void)
{
	float left;
	float top;
	float right;
	float bottom;
	const float *cornerColors[4];
	int i;
	qboolean bright;

	left = (float)s_videoSafeLeft * (EF_PS2_VIEW_W / 640.0f);
	top = (float)s_videoSafeTop * (EF_PS2_VIEW_H / 480.0f);
	right = EF_PS2_VIEW_W - (float)s_videoSafeRight * (EF_PS2_VIEW_W / 640.0f);
	bottom = EF_PS2_VIEW_H - (float)s_videoSafeBottom * (EF_PS2_VIEW_H / 480.0f);
	bright = ((uis.realtime / 250) & 1) ? qtrue : qfalse;
	for (i = 0; i < 4; ++i)
	{
		cornerColors[i] = (i == s_videoCorner && bright) ? colorTable[CT_WHITE] : s_ps2Gold;
	}

	EFFe_DrawUtilityChrome("ELITE FORCE : ADJUST SCREEN SIZE", qtrue, qfalse);
	EFFe_DrawPs2RectColor(left, top, 300.0f, 12.0f, cornerColors[EF_VIDEO_CORNER_TL]);
	EFFe_DrawPs2RectColor(left, top, 12.0f, 225.0f, cornerColors[EF_VIDEO_CORNER_TL]);
	EFFe_DrawPs2RectColor(right - 300.0f, top, 300.0f, 12.0f, cornerColors[EF_VIDEO_CORNER_TR]);
	EFFe_DrawPs2RectColor(right - 12.0f, top, 12.0f, 225.0f, cornerColors[EF_VIDEO_CORNER_TR]);
	EFFe_DrawPs2RectColor(left, bottom - 12.0f, 300.0f, 12.0f, cornerColors[EF_VIDEO_CORNER_BL]);
	EFFe_DrawPs2RectColor(left, bottom - 225.0f, 12.0f, 225.0f, cornerColors[EF_VIDEO_CORNER_BL]);
	EFFe_DrawPs2RectColor(right - 300.0f, bottom - 12.0f, 300.0f, 12.0f, cornerColors[EF_VIDEO_CORNER_BR]);
	EFFe_DrawPs2RectColor(right - 12.0f, bottom - 225.0f, 12.0f, 225.0f, cornerColors[EF_VIDEO_CORNER_BR]);
	EFFe_DrawPs2TextColor(1024.0f, 309.0f, "INSTRUCTIONS", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2MapGold, 0.83f, 1.03f);
	EFFe_DrawPs2TextColor(1024.0f, 402.0f, "Use the directional buttons to adjust", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, colorTable[CT_WHITE], 1.29f, 1.55f);
	EFFe_DrawPs2TextColor(1024.0f, 476.0f, "the position of the flashing corner.", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, colorTable[CT_WHITE], 1.29f, 1.55f);
	EFFe_DrawVideoPrompt();
}

static void EFFe_DrawControllerText(float x, float y, const char *text)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.68f, 1.02f);
}

static void EFFe_DrawControllerSmallText(float x, float y, const char *text)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_MEDIUM, UI_LEFT, colorTable[CT_WHITE], 0.80f, 0.94f);
}

static void EFFe_DrawControllerAnalogText(float x, float y, const char *text)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_BIG, UI_LEFT, colorTable[CT_WHITE], 0.50f, 0.62f);
}

static void EFFe_DrawControllerGoldText(float x, float y, const char *text, int style)
{
	EFFe_DrawPs2TextColor(x, y, text, EF_FRONTEND_FONT_BIG, style, s_ps2MapGold, 0.82f, 1.00f);
}

static void EFFe_DrawControllerGlyph(float x, float y, float w, float h, qhandle_t icon)
{
	if (icon)
	{
		EFFe_DrawPs2Pic(x, y, w, h, icon, CT_WHITE);
	}
}

static void EFFe_DrawControllerBinding(float x, float y, float w, float h, qhandle_t icon, const char *text)
{
	EFFe_DrawControllerGlyph(x, y, w, h, icon);
	EFFe_DrawControllerText(x + 70.0f, y + 7.0f, text);
}

static void EFFe_DrawControllerFrame(void)
{
	EFFe_DrawPs2RectColor(110.0f, 148.0f, 1284.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1404.0f, 148.0f, 284.0f, 34.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(110.0f, 182.0f, 120.0f, 300.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(110.0f, 488.0f, 120.0f, 349.0f, s_ps2MutedPurple);
	EFFe_DrawPs2PicColor(110.0f, 790.0f, 232.0f, 105.0f, s_assets.ps2ControllerBottomLeftChrome, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(110.0f, 790.0f, 120.0f, 47.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(230.0f, 845.0f, 935.0f, 36.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1175.0f, 845.0f, 633.0f, 36.0f, s_ps2MutedPurple);
	EFFe_DrawPs2PicColor(1688.0f, 148.0f, 120.0f, 126.0f, s_assets.ps2ControllerTopRightChrome, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1688.0f, 246.0f, 120.0f, 180.0f, s_ps2MutedPurple);
	EFFe_DrawPs2RectColor(1688.0f, 432.0f, 120.0f, 449.0f, s_ps2MutedPurple);
}

static void EFFe_DrawControllerScreen(void)
{
	EFFe_DrawPic(0.0f, 0.0f, 640.0f, 480.0f, s_assets.whiteShader, CT_BLACK);
	EFFe_DrawPs2TextColor(155.0f, 68.0f, "ELITE FORCE : CONTROLLER", EF_FRONTEND_FONT_BIG, UI_LEFT, s_ps2Gold, 1.06f, 1.20f);
	EFFe_DrawControllerAcceptCancelPrompt();
	EFFe_DrawControllerFrame();
	EFFe_DrawPs2TextColor(960.0f, 208.0f, "< STANDARD >", EF_FRONTEND_FONT_BIG, UI_CENTER, s_ps2MapGold, 0.86f, 1.00f);

	EFFe_DrawPs2PicSTColor(754.0f, 260.0f, 413.0f, 255.0f, 0.0f, 0.0f, 1.0f, 381.0f / 512.0f,
		s_assets.xboxController, colorTable[CT_WHITE]);

	EFFe_DrawControllerBinding(260.0f, 205.0f, 46.0f, 54.0f, s_assets.xboxA, "JUMP");
	EFFe_DrawControllerBinding(260.0f, 283.0f, 46.0f, 54.0f, s_assets.xboxB, "CROUCH");
	EFFe_DrawControllerBinding(260.0f, 361.0f, 50.0f, 50.0f, s_assets.xboxDpad[0], "ZOOM");
	EFFe_DrawControllerBinding(260.0f, 439.0f, 52.0f, 52.0f, s_assets.xboxBlack, "PREV WEAPON");
	EFFe_DrawControllerBinding(260.0f, 517.0f, 52.0f, 52.0f, s_assets.xboxWhite, "NEXT WEAPON");
	EFFe_DrawControllerBinding(260.0f, 595.0f, 55.0f, 35.0f, s_assets.xboxBack, "DATAPAD");
	EFFe_DrawControllerBinding(260.0f, 673.0f, 55.0f, 35.0f, s_assets.xboxStart, "MENU");

	EFFe_DrawControllerBinding(1260.0f, 205.0f, 58.0f, 43.0f, s_assets.xboxRT, "FIRE");
	EFFe_DrawControllerBinding(1260.0f, 283.0f, 58.0f, 43.0f, s_assets.xboxLT, "ALT. FIRE");
	EFFe_DrawControllerBinding(1260.0f, 361.0f, 46.0f, 54.0f, s_assets.xboxY, "CENTER VIEW");
	EFFe_DrawControllerBinding(1260.0f, 439.0f, 46.0f, 54.0f, s_assets.xboxX, "USE");
	EFFe_DrawControllerBinding(1260.0f, 517.0f, 44.0f, 44.0f, s_assets.xboxLStick, "TOGGLE RUN");
	EFFe_DrawControllerBinding(1260.0f, 595.0f, 44.0f, 44.0f, s_assets.xboxRStick, "TOGGLE VIEW");

	EFFe_DrawControllerGlyph(590.0f, 590.0f, 44.0f, 44.0f, s_assets.xboxLStick);
	EFFe_DrawControllerGlyph(590.0f, 665.0f, 44.0f, 44.0f, s_assets.xboxLStick);
	EFFe_DrawControllerSmallText(645.0f, 555.0f, "Analog Left:");
	EFFe_DrawControllerAnalogText(655.0f, 590.0f, "FORWARD");
	EFFe_DrawControllerAnalogText(655.0f, 628.0f, "BACK");
	EFFe_DrawControllerAnalogText(655.0f, 670.0f, "STEP LEFT");
	EFFe_DrawControllerAnalogText(655.0f, 708.0f, "STEP RIGHT");
	EFFe_DrawControllerGlyph(1005.0f, 590.0f, 44.0f, 44.0f, s_assets.xboxRStick);
	EFFe_DrawControllerGlyph(1005.0f, 665.0f, 44.0f, 44.0f, s_assets.xboxRStick);
	EFFe_DrawControllerSmallText(1060.0f, 555.0f, "Analog Right:");
	EFFe_DrawControllerAnalogText(1070.0f, 590.0f, "LOOK DOWN");
	EFFe_DrawControllerAnalogText(1070.0f, 628.0f, "LOOK UP");
	EFFe_DrawControllerAnalogText(1070.0f, 670.0f, "TURN LEFT");
	EFFe_DrawControllerAnalogText(1070.0f, 708.0f, "TURN RIGHT");
}

static void EFFe_LoadAudioState(void)
{
	s_audioEffects = EFFe_Clamp01(ui.Cvar_VariableValue("s_effects_volume"));
	s_audioMusic = EFFe_Clamp01(ui.Cvar_VariableValue("s_music_volume"));
	s_audioVoice = EFFe_Clamp01(ui.Cvar_VariableValue("s_voice_volume"));
	s_audioOriginalEffects = s_audioEffects;
	s_audioOriginalMusic = s_audioMusic;
	s_audioOriginalVoice = s_audioVoice;
	s_audioTouched = qfalse;
#ifdef _XBOX
	XBLF("STEFX: EF audio settings loaded effects=%g music=%g voice=%g",
		s_audioEffects, s_audioMusic, s_audioVoice);
#endif
}

static void EFFe_SetAudioCvars(float effects, float music, float voice)
{
	ui.Cvar_SetValue("s_effects_volume", effects);
	ui.Cvar_SetValue("s_music_volume", music);
	ui.Cvar_SetValue("s_voice_volume", voice);
}

static void EFFe_ApplyAudioState(qboolean persist)
{
	EFFe_SetAudioCvars(s_audioEffects, s_audioMusic, s_audioVoice);
#ifdef _XBOX
	if (persist)
	{
		Settings.effectsVolume = s_audioEffects;
		Settings.musicVolume = s_audioMusic;
		Settings.voiceVolume = s_audioVoice;
		if (!Settings.Save())
		{
			XBLog_WriteCritical("STEFX: EF audio settings persistence failed");
		}
	}
	XBLF("STEFX: EF audio settings apply effects=%g music=%g voice=%g persist=%d",
		s_audioEffects, s_audioMusic, s_audioVoice, persist ? 1 : 0);
#else
	(void)persist;
#endif
	s_audioTouched = qfalse;
}

static void EFFe_CancelAudioState(void)
{
	s_audioEffects = s_audioOriginalEffects;
	s_audioMusic = s_audioOriginalMusic;
	s_audioVoice = s_audioOriginalVoice;
	EFFe_SetAudioCvars(s_audioEffects, s_audioMusic, s_audioVoice);
	s_audioTouched = qfalse;
#ifdef _XBOX
	XBLF("STEFX: EF audio settings cancel restore effects=%g music=%g voice=%g",
		s_audioEffects, s_audioMusic, s_audioVoice);
#endif
}

static void EFFe_LoadVideoState(void)
{
	s_videoSafeLeft = EFFe_ClampInt((int)ui.Cvar_VariableValue("stefx_safeAreaLeft"), 0, 96);
	s_videoSafeTop = EFFe_ClampInt((int)ui.Cvar_VariableValue("stefx_safeAreaTop"), 0, 72);
	s_videoSafeRight = EFFe_ClampInt((int)ui.Cvar_VariableValue("stefx_safeAreaRight"), 0, 96);
	s_videoSafeBottom = EFFe_ClampInt((int)ui.Cvar_VariableValue("stefx_safeAreaBottom"), 0, 72);
	s_videoOriginalLeft = s_videoSafeLeft;
	s_videoOriginalTop = s_videoSafeTop;
	s_videoOriginalRight = s_videoSafeRight;
	s_videoOriginalBottom = s_videoSafeBottom;
	s_videoCorner = EF_VIDEO_CORNER_TL;
#ifdef _XBOX
	XBLF("STEFX_SAFE_AREA: loaded left=%d top=%d right=%d bottom=%d",
		s_videoSafeLeft, s_videoSafeTop, s_videoSafeRight, s_videoSafeBottom);
#endif
}

static void EFFe_ApplyVideoState(void)
{
	ui.Cvar_SetValue("stefx_safeAreaLeft", (float)s_videoSafeLeft);
	ui.Cvar_SetValue("stefx_safeAreaTop", (float)s_videoSafeTop);
	ui.Cvar_SetValue("stefx_safeAreaRight", (float)s_videoSafeRight);
	ui.Cvar_SetValue("stefx_safeAreaBottom", (float)s_videoSafeBottom);
#ifdef _XBOX
	Settings.safeAreaLeft = s_videoSafeLeft;
	Settings.safeAreaTop = s_videoSafeTop;
	Settings.safeAreaRight = s_videoSafeRight;
	Settings.safeAreaBottom = s_videoSafeBottom;
	if (!Settings.Save())
	{
		XBLog_WriteCritical("STEFX_SAFE_AREA: settings persistence failed");
	}
	XBLF("STEFX_SAFE_AREA: accepted left=%d top=%d right=%d bottom=%d",
		s_videoSafeLeft, s_videoSafeTop, s_videoSafeRight, s_videoSafeBottom);
#endif
}

static void EFFe_CancelVideoState(void)
{
	s_videoSafeLeft = s_videoOriginalLeft;
	s_videoSafeTop = s_videoOriginalTop;
	s_videoSafeRight = s_videoOriginalRight;
	s_videoSafeBottom = s_videoOriginalBottom;
#ifdef _XBOX
	XBLF("STEFX_SAFE_AREA: cancelled left=%d top=%d right=%d bottom=%d",
		s_videoSafeLeft, s_videoSafeTop, s_videoSafeRight, s_videoSafeBottom);
#endif
}

static void EFFe_AdjustVideoCorner(int horizontal, int vertical)
{
	const int horizontalStep = 4;
	const int verticalStep = 3;

	if (horizontal)
	{
		if (s_videoCorner == EF_VIDEO_CORNER_TL || s_videoCorner == EF_VIDEO_CORNER_BL)
		{
			s_videoSafeLeft = EFFe_ClampInt(s_videoSafeLeft + horizontal * horizontalStep, 0, 96);
		}
		else
		{
			s_videoSafeRight = EFFe_ClampInt(s_videoSafeRight - horizontal * horizontalStep, 0, 96);
		}
	}
	if (vertical)
	{
		if (s_videoCorner == EF_VIDEO_CORNER_TL || s_videoCorner == EF_VIDEO_CORNER_TR)
		{
			s_videoSafeTop = EFFe_ClampInt(s_videoSafeTop + vertical * verticalStep, 0, 72);
		}
		else
		{
			s_videoSafeBottom = EFFe_ClampInt(s_videoSafeBottom - vertical * verticalStep, 0, 72);
		}
	}
#ifdef _XBOX
	XBLF("STEFX_SAFE_AREA: adjust corner=%d left=%d top=%d right=%d bottom=%d",
		s_videoCorner, s_videoSafeLeft, s_videoSafeTop, s_videoSafeRight, s_videoSafeBottom);
#endif
}

static void EFFe_AdjustAudio(int direction)
{
	float delta;

	delta = direction > 0 ? 0.1f : -0.1f;
	if (s_cursor == 0)
	{
		s_audioEffects = EFFe_Clamp01(s_audioEffects + delta);
	}
	else if (s_cursor == 1)
	{
		s_audioMusic = EFFe_Clamp01(s_audioMusic + delta);
	}
	else if (s_cursor == 2)
	{
		s_audioVoice = EFFe_Clamp01(s_audioVoice + delta);
	}
	s_audioTouched = qtrue;
	EFFe_ApplyAudioState(qfalse);
	s_audioTouched = qtrue;
}

static void EFFe_SetNewGameGender(qboolean male)
{
	s_newgameGenderMale = male ? 1 : 0;
	s_newgameGenderTouched = qtrue;
	if (male)
	{
		ui.Cvar_Set("legsmodel", "hazard/default");
		ui.Cvar_Set("torsomodel", "hazard/default");
		ui.Cvar_Set("headmodel", "munro/default");
		ui.Cvar_Set("sex", "male");
	}
	else
	{
		ui.Cvar_Set("legsmodel", "hazardfemale/default");
		ui.Cvar_Set("torsomodel", "hazardfemale/default");
		ui.Cvar_Set("headmodel", "alexandria/default");
		ui.Cvar_Set("sex", "female");
	}
#ifdef _XBOX
	XBLF("STEFX: EF new game gender set male=%d", s_newgameGenderMale);
#endif
}

static void EFFe_SetNewGameDifficulty(int difficulty)
{
	if (difficulty < 0)
	{
		difficulty = 0;
	}
	if (difficulty > 3)
	{
		difficulty = 3;
	}
	s_newgameDifficulty = difficulty;

	if (difficulty == 0)
	{
		ui.Cvar_SetValue("g_spskill", 0.0f);
		ui.Cvar_Set("handicap", "100");
	}
	else if (difficulty == 1)
	{
		ui.Cvar_SetValue("g_spskill", 1.0f);
		ui.Cvar_Set("handicap", "100");
	}
	else if (difficulty == 2)
	{
		ui.Cvar_SetValue("g_spskill", 2.0f);
		ui.Cvar_Set("handicap", "100");
	}
	else
	{
		ui.Cvar_SetValue("g_spskill", 2.0f);
		ui.Cvar_Set("handicap", "50");
	}
#ifdef _XBOX
	XBLF("STEFX: EF new game difficulty set index=%d item='%s'", difficulty, s_newgameItems[difficulty]);
#endif
}

static void EFFe_SyncNewGameState(void)
{
	char sex[32];
	int skill;
	float handicap;

	skill = (int)ui.Cvar_VariableValue("g_spskill");
	handicap = ui.Cvar_VariableValue("handicap");
	if (skill <= 0)
	{
		s_newgameDifficulty = 0;
	}
	else if (skill == 1)
	{
		s_newgameDifficulty = 1;
	}
	else if (handicap > 50.0f)
	{
		s_newgameDifficulty = 2;
	}
	else
	{
		s_newgameDifficulty = 3;
	}

	if (!s_newgameGenderTouched)
	{
		s_newgameGenderMale = 1;
		return;
	}

	ui.Cvar_VariableStringBuffer("sex", sex, sizeof(sex));
	s_newgameGenderMale = Q_stricmp(sex, "female") && Q_stricmp(sex, "f");
}

static void EFFe_StartMap(const char *mapName, qboolean virtualVoyager = qfalse)
{
#ifdef _XBOX
	XBLF("STEFX: EF new game start map='%s' difficulty=%d genderMale=%d catcher=0x%x", mapName ? mapName : "", s_newgameDifficulty, s_newgameGenderMale, ui.Key_GetCatcher());
	ui.Printf("STEFX_MENU_NEWGAME_START map='%s' cursor=%d difficulty=%d genderMale=%d catcher=0x%x\n",
		mapName ? mapName : "",
		s_cursor,
		s_newgameDifficulty,
		s_newgameGenderMale,
		ui.Key_GetCatcher());
#endif
	EFFe_SetNewGameDifficulty(s_newgameDifficulty);
	EFFe_SetNewGameGender(s_newgameGenderMale ? qtrue : qfalse);
	ui.Cvar_Set("stefx_splitScreen", "0");
	ui.Cvar_Set("stefx_splitScreenPlayers", "1");
	ui.Cvar_Set("stefx_splitScreenMode", "sp");
	ui.Cvar_Set("r_splitScreenEconomy", "0");
	ui.Cvar_Set("stefx_splitScreenP2Entity", "-1");
	s_active = qfalse;
	UI_ForceMenuOff();
	ui.Cvar_SetValue("cg_virtualVoyager", virtualVoyager ? 1.0f : 0.0f);
	if (mapName && mapName[0])
	{
#ifdef _XBOX
		extern bool Sys_XboxQueueMenuMap(const char *mapName, const char *mode, int players);
		Sys_XboxQueueMenuMap(mapName, virtualVoyager ? "virtualvoyager" : "sp", 1);
#else
		ui.Cmd_ExecuteText(EXEC_APPEND, va("map %s\n", mapName));
#endif
	}
}

static void EFFe_StartCoopNewGame(void)
{
#ifdef _XBOX
	XBLF("STEFX_COOP_MENU_LAUNCH: map='%s' players=2 difficulty=%d p1='%s' p2='%s'",
		EF_SPLITSCREEN_BASELINE_MAP,
		s_newgameDifficulty,
		s_newgameGenderMale ? "munro" : "alexandria",
		s_newgameGenderMale ? "alexandria" : "munro");
#endif
	EFFe_SetNewGameDifficulty(s_newgameDifficulty);
	EFFe_SetNewGameGender(s_newgameGenderMale ? qtrue : qfalse);
	ui.Cvar_Set("stefx_splitScreen", "1");
	ui.Cvar_Set("stefx_splitScreenPlayers", "2");
	ui.Cvar_Set("stefx_splitScreenMode", "coop");
	ui.Cvar_Set("r_splitScreenEconomy", "1");
	ui.Cvar_Set("stefx_splitScreenP2Entity", "-1");
	ui.Cvar_SetValue("cg_virtualVoyager", 0.0f);
	s_active = qfalse;
	UI_ForceMenuOff();
#ifdef _XBOX
	{
		extern bool Sys_XboxQueueMenuMap(const char *mapName, const char *mode, int players);
		Sys_XboxQueueMenuMap(EF_SPLITSCREEN_BASELINE_MAP, "coop", 2);
	}
#else
	ui.Cmd_ExecuteText(EXEC_APPEND, "map " EF_SPLITSCREEN_BASELINE_MAP "\n");
#endif
}

static const efFrontendHolomatchCharacter_t *EFFe_HolomatchCharacterForCrewMember(int crewMember)
{
	return &s_holomatchCharacters[EFFe_HolomatchCharacterIndexForCrewMember(crewMember)];
}

static void EFFe_FillHolomatchPlayerSettings(stefxHolomatchLaunchSetup_t *setup)
{
	int player;

	for (player = 0; player < STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS; ++player)
	{
		const efFrontendHolomatchCharacter_t *character =
			EFFe_HolomatchCharacterForCrewMember(s_holomatchPlayerCharacters[player]);
		stefxHolomatchPlayerSetup_t *playerSetup = &setup->player[player];

		Q_strncpyz(playerSetup->modelName, character->modelName, sizeof(playerSetup->modelName));
		Q_strncpyz(playerSetup->skinName,
			s_holomatchOutfitCvars[s_holomatchPlayerOutfits[player]], sizeof(playerSetup->skinName));
		playerSetup->controlStyle = s_holomatchPlayerControlStyles[player];
		playerSetup->autoswitchMode = s_holomatchPlayerAutoswitch[player];
		playerSetup->autoaimMode = s_holomatchPlayerAutoaim[player];
		playerSetup->crosshair = s_holomatchPlayerCrosshairs[player] + 1;
		playerSetup->vibration = s_holomatchPlayerVibration[player] ? 1 : 0;
		playerSetup->invertPitch = s_holomatchPlayerInvertPitch[player] ? 1 : 0;
	}
}

static void EFFe_ApplyHolomatchPlayerSettings(const stefxHolomatchLaunchSetup_t *setup)
{
	int player;
	char cvarName[32];
	char cvarValue[32];
	for (player = 0; player < setup->players && player < STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS; ++player)
	{
		const stefxHolomatchPlayerSetup_t *playerSetup = &setup->player[player];
		Com_sprintf(cvarName, sizeof(cvarName), "hm_model_%d", player);
		ui.Cvar_Set(cvarName, playerSetup->modelName);
		Com_sprintf(cvarName, sizeof(cvarName), "hm_skin_%d", player);
		ui.Cvar_Set(cvarName, playerSetup->skinName);
		Com_sprintf(cvarName, sizeof(cvarName), "cont_config_%d", player);
		Com_sprintf(cvarValue, sizeof(cvarValue), "%d", playerSetup->controlStyle);
		ui.Cvar_Set(cvarName, cvarValue);
		Com_sprintf(cvarName, sizeof(cvarName), "cg_autoswitch_%d", player);
		Com_sprintf(cvarValue, sizeof(cvarValue), "%d", playerSetup->autoswitchMode);
		ui.Cvar_Set(cvarName, cvarValue);
		Com_sprintf(cvarName, sizeof(cvarName), "g_autoaim_%d", player);
		Com_sprintf(cvarValue, sizeof(cvarValue), "%d", playerSetup->autoaimMode);
		ui.Cvar_Set(cvarName, cvarValue);
		Com_sprintf(cvarName, sizeof(cvarName), "cg_drawCrosshair_%d", player);
		Com_sprintf(cvarValue, sizeof(cvarValue), "%d", playerSetup->crosshair);
		ui.Cvar_Set(cvarName, cvarValue);
		Com_sprintf(cvarName, sizeof(cvarName), "joy_vibestate_%d", player);
		ui.Cvar_Set(cvarName, playerSetup->vibration ? "1" : "0");
		Com_sprintf(cvarName, sizeof(cvarName), "joy_pitchsensitivity_%d", player);
		ui.Cvar_Set(cvarName, playerSetup->invertPitch ? "-1" : "1");
#ifdef _XBOX
		XBLF("STEFX_HM_PLAYER_CVARS: player=%d model='%s' skin='%s' control=%d autoswitch=%d autoaim=%d crosshair=%d vibration=%d invert=%d",
			player + 1, playerSetup->modelName, playerSetup->skinName,
			playerSetup->controlStyle, playerSetup->autoswitchMode, playerSetup->autoaimMode,
			playerSetup->crosshair, playerSetup->vibration, playerSetup->invertPitch);
#endif
	}
	ui.Cvar_Set("model", va("%s/%s", setup->player[0].modelName, setup->player[0].skinName));
	Com_sprintf(cvarValue, sizeof(cvarValue), "%d", setup->player[0].crosshair);
	ui.Cvar_Set("cg_drawCrosshair", cvarValue);
	Com_sprintf(cvarValue, sizeof(cvarValue), "%d", setup->player[0].autoswitchMode);
	ui.Cvar_Set("cg_autoswitch", cvarValue);
}

static void EFFe_StartHolomatch(void)
{
	stefxHolomatchLaunchSetup_t setup;
	char number[16];
	int botMinPlayers = s_holomatchPlayers + 3;

	memset(&setup, 0, sizeof(setup));
	setup.magic = STEFX_HOLOMATCH_SETUP_MAGIC;
	setup.version = STEFX_HOLOMATCH_SETUP_VERSION;
	Q_strncpyz(setup.mapName, s_holomatchMapNames[s_holomatchMap], sizeof(setup.mapName));
	setup.players = s_holomatchPlayers;
	setup.humanPlayers = EFFe_HolomatchHumanPlayerCount();
	setup.fragLimit = s_holomatchPointLimit;
	setup.timeLimit = s_holomatchTimeLimit;
	setup.forceRespawn = s_holomatchForceRespawn;
	setup.weaponStay = s_holomatchWeaponStay;
	setup.fallingDamage = s_holomatchFallingDamage;
	setup.teamPlay = s_holomatchTeamPlay;
	setup.friendlyFire = s_holomatchFriendlyFire;
	setup.diagnosticVirtualControls = EFFe_HolomatchMenuSmokeUsesVirtualPads() ? 1 : 0;
	setup.diagnosticVirtualControlsP1 = setup.diagnosticVirtualControls;
	EFFe_FillHolomatchPlayerSettings(&setup);

#ifdef _XBOX
	XBLF("STEFX_HM_MENU_LAUNCH: map='%s' viewports=%d humans=%d botViews=%d frag=%d time=%d force=%d stay=%d falling=%d team=%d friendly=%d virtual=%d virtualP1=%d",
		setup.mapName, setup.players, setup.humanPlayers, setup.players - setup.humanPlayers,
		setup.fragLimit, setup.timeLimit, setup.forceRespawn,
		setup.weaponStay, setup.fallingDamage, setup.teamPlay, setup.friendlyFire,
		setup.diagnosticVirtualControls, setup.diagnosticVirtualControlsP1);
#endif

#if defined(_XBOX) && !defined(STEFX_SP_HOSTED_MP)
	{
		extern void Sys_Reboot(const char *reason, const void *pData);
		Sys_Reboot("multiplayer", &setup);
	}
#else
	EFFe_ApplyHolomatchPlayerSettings(&setup);
	Com_sprintf(number, sizeof(number), "%d", setup.players);
	ui.Cvar_Set("stefx_splitScreen", "1");
	ui.Cvar_Set("stefx_splitScreenPlayers", number);
	ui.Cvar_Set("stefx_splitScreenMode", "holomatch");
	ui.Cvar_Set("stefx_hmLocalPlayers", number);
	Com_sprintf(number, sizeof(number), "%d", setup.humanPlayers);
	ui.Cvar_Set("stefx_hmHumanPlayers", number);
	ui.Cvar_Set("r_splitScreenEconomy", setup.players >= 2 ? "1" : "0");
	ui.Cvar_Set("stefx_hm_split_virtual_controls", setup.diagnosticVirtualControls ? "1" : "0");
	ui.Cvar_Set("stefx_hm_split_virtual_controls_p1", setup.diagnosticVirtualControlsP1 ? "1" : "0");
	ui.Cvar_Set("stefx_hm_launch_source", "menu");
	ui.Cvar_Set("stefx_splitScreenP2Entity", "-1");
	ui.Cvar_Set("sv_maxclients", "8");
	ui.Cvar_Set("g_gametype", setup.teamPlay ? "3" : "0");
	Com_sprintf(number, sizeof(number), "%d", setup.fragLimit);
	ui.Cvar_Set("fraglimit", number);
	Com_sprintf(number, sizeof(number), "%d", setup.timeLimit);
	ui.Cvar_Set("timelimit", number);
	ui.Cvar_Set("g_forcerespawn", setup.forceRespawn ? "20" : "0");
	ui.Cvar_Set("stefx_hm_weapon_stay", setup.weaponStay ? "1" : "0");
	ui.Cvar_Set("dmflags", setup.fallingDamage ? "0" : "8");
	ui.Cvar_Set("g_friendlyFire", setup.friendlyFire ? "1" : "0");
	ui.Cvar_Set("bot_enable", "1");
	Com_sprintf(number, sizeof(number), "%d", botMinPlayers);
	ui.Cvar_Set("bot_minplayers", number);
	ui.Cvar_Set("g_spSkill", "2");
	ui.Cvar_SetValue("cg_virtualVoyager", 0.0f);
	s_active = qfalse;
	UI_ForceMenuOff();
#ifdef _XBOX
	{
		extern bool Sys_XboxQueueMenuMap(const char *mapName, const char *mode, int players);
		Sys_XboxQueueMenuMap(setup.mapName, "holomatch", setup.players);
	}
#else
	ui.Cmd_ExecuteText(EXEC_APPEND, va("map %s\n", setup.mapName));
#endif
#endif
}

static void EFFe_SetScreen(efFrontendScreen_t screen, int cursor)
{
	s_screen = screen;
	s_cursor = cursor;
	s_loggedDraw = qfalse;
	if (s_screen == EF_SCREEN_AUDIO)
	{
		EFFe_LoadAudioState();
	}
	else if (s_screen == EF_SCREEN_VIDEO)
	{
		EFFe_LoadVideoState();
	}
	else if (s_screen == EF_SCREEN_NEWGAME)
	{
		EFFe_SyncNewGameState();
	}
	else if (s_screen == EF_SCREEN_COOP_NEW)
	{
		EFFe_SyncNewGameState();
	}
	else if (s_screen == EF_SCREEN_LOADGAME)
	{
		s_loadCount = UI_EFSave_Count();
		if (s_cursor >= s_loadCount)
		{
			s_cursor = s_loadCount > 0 ? s_loadCount - 1 : 0;
		}
	}
	else if (s_screen == EF_SCREEN_SAVEGAME)
	{
		s_saveCount = UI_EFSave_Count();
		s_saveOverwritePending = qfalse;
		if (s_cursor > s_saveCount)
		{
			s_cursor = s_saveCount;
		}
	}
#ifdef _XBOX
	XBLF("STEFX: EF frontend screen set screen='%s' cursor=%d", EFFe_ScreenName(s_screen), s_cursor);
	ui.Printf("STEFX_MENU_SCREEN_SET screen='%s' cursor=%d catcher=0x%x\n",
		EFFe_ScreenName(s_screen),
		s_cursor,
		ui.Key_GetCatcher());
#endif
}

static void EFFe_OpenScreenInternal(efFrontendScreen_t screen, int cursor, const char *reason,
	qboolean preserveGame, qboolean returnToPause)
{
#ifdef _XBOX
	XBLF("STEFX: EF frontend open screen='%s' active=%d reason='%s' preserveGame=%d returnToPause=%d realtime=%d catcher=0x%x", EFFe_ScreenName(screen), s_active ? 1 : 0, reason ? reason : "", preserveGame ? 1 : 0, returnToPause ? 1 : 0, uis.realtime, ui.Key_GetCatcher());
#endif
	UI_EFQmenu_ClearState(reason ? reason : "ef-frontend-open");
	UI_EFPauseMenu_Deactivate();
	EFFe_Cache();
	if (!preserveGame)
	{
		ui.Cvar_Set("sv_killserver", "1");
	}
	else
	{
		ui.Cvar_Set("cl_paused", "1");
	}
	ui.Key_SetCatcher(KEYCATCH_UI);
	s_active = qtrue;
	s_returnToPause = returnToPause;
	EFFe_SetScreen(screen, cursor);
}

static void EFFe_OpenScreen(efFrontendScreen_t screen, int cursor, const char *reason)
{
	EFFe_OpenScreenInternal(screen, cursor, reason, qfalse, qfalse);
}

static void EFFe_ReturnToMain(void)
{
	s_returnToPause = qfalse;
	EFFe_SetScreen(EF_SCREEN_MAIN, 0);
}

static void EFFe_ReturnToPause(void)
{
#ifdef _XBOX
	XBLF("STEFX_MENU_RETURN: return to pause from screen='%s'", EFFe_ScreenName(s_screen));
#endif
	s_active = qfalse;
	s_screen = EF_SCREEN_MAIN;
	s_returnToPause = qfalse;
	UI_EFPauseMenu_Open(NULL);
}

static void EFFe_ReturnToConfigure(void)
{
	EFFe_SetScreen(EF_SCREEN_CONFIGURE, 0);
}

static void EFFe_DrawChildScreen(void)
{
	switch (s_screen)
	{
	case EF_SCREEN_NEWGAME:
		EFFe_DrawNewGameScreen();
		break;
	case EF_SCREEN_LOADGAME:
		EFFe_DrawLoadGameScreen();
		break;
	case EF_SCREEN_SAVEGAME:
		EFFe_DrawSaveGameScreen();
		break;
	case EF_SCREEN_COOP:
		EFFe_DrawCoopScreen();
		break;
	case EF_SCREEN_COOP_NEW:
		EFFe_DrawCoopNewScreen();
		break;
	case EF_SCREEN_HOLOMATCH:
		EFFe_DrawHolomatchScreen();
		break;
	case EF_SCREEN_HOLOMATCH_ADVANCED:
		EFFe_DrawHolomatchAdvancedScreen();
		break;
	case EF_SCREEN_HOLOMATCH_PLAYERS:
		EFFe_DrawHolomatchPlayersScreen();
		break;
	case EF_SCREEN_CREW:
		EFFe_DrawCrewScreen();
		break;
	case EF_SCREEN_CREW_ROSTER:
		EFFe_DrawCrewRosterScreen();
		break;
	case EF_SCREEN_CREW_BIO:
		EFFe_DrawCrewBioScreen();
		break;
	case EF_SCREEN_CREW_HAZARD_SUIT:
		EFFe_DrawCrewHazardSuitScreen();
		break;
	case EF_SCREEN_CREW_VOYAGER:
		EFFe_DrawCrewVoyagerScreen();
		break;
	case EF_SCREEN_CONFIGURE:
		EFFe_DrawConfigureScreen();
		break;
	case EF_SCREEN_AUDIO:
		EFFe_DrawAudioScreen();
		break;
	case EF_SCREEN_VIDEO:
		EFFe_DrawVideoScreen();
		break;
	case EF_SCREEN_CONTROLLER:
		EFFe_DrawControllerScreen();
		break;
	case EF_SCREEN_STUB:
		EFFe_DrawStubScreen();
		break;
	default:
		break;
	}
}
static void EFFe_DrawFrame(const char *title, qboolean backPrompt)
{
	EFFe_DrawMainTopChrome(title, backPrompt);

	EFFe_DrawPanelCode(359.0f, 301.0f, "81453");
	EFFe_DrawPanelCode(359.0f, 351.0f, "9343");
	EFFe_DrawPanelCode(359.0f, 744.0f, "431108");

	EFFe_DrawPs2PanelBracket(916.0f, 250.0f, 63.0f, qfalse, qfalse);
	EFFe_DrawPs2PanelBracket(916.0f, 619.0f, 63.0f, qfalse, qtrue);
	EFFe_DrawPs2PanelBracket(1655.0f, 251.0f, 66.0f, qtrue, qfalse);
	EFFe_DrawPs2PanelBracket(1655.0f, 619.0f, 66.0f, qtrue, qtrue);
	EFFe_DrawPs2Pic(970.0f, 253.0f, 700.0f, 607.0f, s_assets.quadrants, CT_WHITE);
	EFFe_DrawPs2RectColor(918.0f, 542.0f, 803.0f, 4.0f, s_ps2LightBrown);
	EFFe_DrawPs2RectColor(1342.0f, 253.0f, 4.0f, 607.0f, s_ps2LightBrown);

	EFFe_DrawPs2TextColor(1082.0f, 306.0f, "Dominion", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(997.0f, 371.0f, "Bajoran", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(997.0f, 417.0f, "Wormhole", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1522.0f, 315.0f, "Voyager", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1375.0f, 372.0f, "Borg", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1375.0f, 418.0f, "Space", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	/*
	** Text height is expressed in native UI pixels after the PS2 position is
	** converted, so the original 518/519 top row touched the 542-546 rule.
	** Keep both rows on common baselines with equal visual clearance.
	*/
	EFFe_DrawPs2TextColor(1045.0f, 488.0f, "Gamma", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1045.0f, 568.0f, "Alpha", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1639.0f, 488.0f, "Delta", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1639.0f, 568.0f, "Beta", EF_FRONTEND_FONT_MEDIUM, UI_CENTER, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1010.0f, 661.0f, "Ferengi Alliance", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(970.0f, 715.0f, "Cardassia", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(970.0f, 770.0f, "Federation", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1534.0f, 648.0f, "Romulan", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1534.0f, 700.0f, "Empire", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1534.0f, 741.0f, "Klingon", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
	EFFe_DrawPs2TextColor(1534.0f, 793.0f, "Empire", EF_FRONTEND_FONT_MEDIUM, UI_LEFT, s_ps2MapGold, 0.96f, 1.06f);
}

static void EFFe_ActivateButton(const efFrontendButton_t *button)
{
	if (!button || !button->enabled)
	{
		return;
	}

#ifdef _XBOX
	XBLF("STEFX: EF frontend activate label='%s' command='%s' catcher=0x%x",
		EFFe_ButtonText(button, 0),
		button->commandName ? button->commandName : "",
		ui.Key_GetCatcher());
#endif

	if (!button->commandName || !button->commandName[0])
	{
		return;
	}

	if (!UI_EFQmenu_ConsoleCommand(button->commandName))
	{
#ifdef _XBOX
		XBLF("STEFX: EF frontend command unhandled label='%s' command='%s'",
			EFFe_ButtonText(button, 0),
			button->commandName);
#endif
		ui.Key_SetCatcher(KEYCATCH_UI);
	}
}
static void EFFe_MoveCursor(int count, int delta)
{
	if (count <= 0)
	{
		return;
	}
	s_cursor = (s_cursor + count + delta) % count;
}

static void EFFe_HandleMainKey(int key)
{
	if (EFFe_IsUpKey(key))
	{
		do
		{
			EFFe_MoveCursor(EF_FRONTEND_BUTTON_COUNT, -1);
		}
		while (!s_buttons[s_cursor].enabled);
	}
	else if (EFFe_IsDownKey(key))
	{
		do
		{
			EFFe_MoveCursor(EF_FRONTEND_BUTTON_COUNT, 1);
		}
		while (!s_buttons[s_cursor].enabled);
	}
	else if (EFFe_IsAcceptKey(key))
	{
		if (s_buttons[s_cursor].enabled)
		{
#ifdef _XBOX
			XBLF("STEFX: EF frontend command label='%s' command='%s'", EFFe_ButtonText(&s_buttons[s_cursor], 0), s_buttons[s_cursor].commandName);
			ui.Printf("STEFX_MENU_MAIN_ACCEPT cursor=%d label='%s' command='%s'\n",
				s_cursor,
				EFFe_ButtonText(&s_buttons[s_cursor], 0),
				s_buttons[s_cursor].commandName ? s_buttons[s_cursor].commandName : "");
#endif
			EFFe_ActivateButton(&s_buttons[s_cursor]);
		}
	}
}

static void EFFe_HandleNewGameKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_ReturnToMain();
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_NEWGAME_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_NEWGAME_COUNT, 1);
		return;
	}
	if (EFFe_IsLeftKey(key) || EFFe_IsRightKey(key))
	{
		if (s_cursor <= 3)
		{
			EFFe_SetNewGameDifficulty(s_cursor);
		}
		else if (s_cursor == 4 || s_cursor == 5)
		{
			EFFe_SetNewGameGender(s_cursor == 5 ? qtrue : qfalse);
		}
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}

#ifdef _XBOX
	ui.Printf("STEFX_MENU_NEWGAME_ACCEPT cursor=%d item='%s'\n",
		s_cursor,
		(s_cursor >= 0 && s_cursor < EF_FRONTEND_NEWGAME_COUNT) ? s_newgameItems[s_cursor] : "<bad>");
#endif

	if (s_cursor <= 3)
	{
		EFFe_SetNewGameDifficulty(s_cursor);
	}
	else if (s_cursor == 4 || s_cursor == 5)
	{
		EFFe_SetNewGameGender(s_cursor == 5 ? qtrue : qfalse);
	}
	else if (s_cursor == 6)
	{
		EFFe_StartMap("tutorial");
	}
	else if (s_cursor == 7)
	{
		EFFe_StartMap("borg1");
	}
	else if (s_cursor == 8)
	{
		EFFe_StartMap("tour/deck02", qtrue);
	}
}

static void EFFe_HandleCoopKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		if (s_returnToPause)
		{
			EFFe_ReturnToPause();
		}
		else
		{
			EFFe_ReturnToMain();
		}
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_COOP_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_COOP_COUNT, 1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}

#ifdef _XBOX
	XBLF("STEFX_COOP_MENU: accept item='%s'", s_cursor == 0 ? "new-game" : "load-game");
#endif
	if (s_cursor == 0)
	{
		EFFe_SetScreen(EF_SCREEN_COOP_NEW, 0);
	}
	else
	{
		s_loadForCoop = qtrue;
		EFFe_SetScreen(EF_SCREEN_LOADGAME, 0);
	}
}

static void EFFe_HandleCoopNewKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_SetScreen(EF_SCREEN_COOP, 0);
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		do
		{
			EFFe_MoveCursor(EF_FRONTEND_COOP_NEW_COUNT, -1);
		} while (s_cursor == 6);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		do
		{
			EFFe_MoveCursor(EF_FRONTEND_COOP_NEW_COUNT, 1);
		} while (s_cursor == 6);
		return;
	}
	if (EFFe_IsLeftKey(key) || EFFe_IsRightKey(key) || EFFe_IsAcceptKey(key))
	{
		if (s_cursor <= 3)
		{
			EFFe_SetNewGameDifficulty(s_cursor);
		}
		else if (s_cursor <= 5)
		{
			EFFe_SetNewGameGender(s_cursor == 5 ? qtrue : qfalse);
		}
		else if (s_cursor == 7 && EFFe_IsAcceptKey(key))
		{
			EFFe_StartCoopNewGame();
		}
	}
}

static void EFFe_HandleLoadGameKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		if (s_returnToPause)
		{
			EFFe_ReturnToPause();
		}
		else if (s_loadForCoop)
		{
			EFFe_SetScreen(EF_SCREEN_COOP, 1);
		}
		else
		{
			EFFe_ReturnToMain();
		}
		return;
	}
	if (s_loadCount <= 0)
	{
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(s_loadCount, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(s_loadCount, 1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}

#ifdef _XBOX
	XBLF("STEFX_SAVELOAD_MENU: accept coop=%d index=%d name='%s' corrupt=%d",
		s_loadForCoop ? 1 : 0, s_cursor, UI_EFSave_Name(s_cursor), UI_EFSave_IsCorrupt(s_cursor));
#endif
	if (UI_EFSave_IsCorrupt(s_cursor))
	{
		return;
	}
	ui.Cvar_Set("stefx_splitScreen", s_loadForCoop ? "1" : "0");
	ui.Cvar_Set("stefx_splitScreenPlayers", s_loadForCoop ? "2" : "1");
	ui.Cvar_Set("stefx_splitScreenMode", s_loadForCoop ? "coop" : "sp");
	ui.Cvar_Set("r_splitScreenEconomy", s_loadForCoop ? "1" : "0");
	ui.Cvar_Set("stefx_splitScreenP2Entity", "-1");
	ui.Cvar_SetValue("cg_virtualVoyager", 0.0f);
	if (UI_EFSave_Load(s_cursor))
	{
		s_active = qfalse;
		UI_ForceMenuOff();
	}
}

static void EFFe_HandleSaveGameKey(int key)
{
	int total = s_saveCount + 1;

	if (EFFe_IsBackKey(key))
	{
		if (s_saveOverwritePending)
		{
			s_saveOverwritePending = qfalse;
#ifdef _XBOX
			XBLF("STEFX_SAVELOAD_MENU: overwrite cancelled cursor=%d", s_cursor);
#endif
		}
		else
		{
			EFFe_ReturnToPause();
		}
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		s_saveOverwritePending = qfalse;
		EFFe_MoveCursor(total, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		s_saveOverwritePending = qfalse;
		EFFe_MoveCursor(total, 1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}

	if (s_cursor == 0)
	{
#ifdef _XBOX
		XBLF("STEFX_SAVELOAD_MENU: create-new accepted existing=%d", s_saveCount);
#endif
		if (UI_EFSave_CreateNew())
		{
			s_active = qfalse;
			s_returnToPause = qfalse;
			UI_ForceMenuOff();
		}
		return;
	}

	if (!s_saveOverwritePending)
	{
		s_saveOverwritePending = qtrue;
#ifdef _XBOX
		XBLF("STEFX_SAVELOAD_MENU: overwrite armed index=%d name='%s'", s_cursor - 1, UI_EFSave_Name(s_cursor - 1));
#endif
		return;
	}

#ifdef _XBOX
	XBLF("STEFX_SAVELOAD_MENU: overwrite confirmed index=%d name='%s'", s_cursor - 1, UI_EFSave_Name(s_cursor - 1));
#endif
	if (UI_EFSave_Overwrite(s_cursor - 1))
	{
		s_active = qfalse;
		s_returnToPause = qfalse;
		UI_ForceMenuOff();
	}
}

static int EFFe_CycleOption(const int *values, int count, int current, int direction)
{
	int i;
	for (i = 0; i < count; ++i)
	{
		if (values[i] == current)
		{
			return values[(i + count + direction) % count];
		}
	}
	return values[0];
}

static void EFFe_AdjustHolomatchAdvanced(int direction)
{
	static const int pointLimits[] = { 0, 5, 10, 20, 30, 50 };
	static const int timeLimits[] = { 0, 5, 10, 15, 20, 30 };
	if (s_cursor == 0)
	{
		s_holomatchPointLimit = EFFe_CycleOption(pointLimits, EF_ARRAY_LEN(pointLimits), s_holomatchPointLimit, direction);
	}
	else if (s_cursor == 1)
	{
		s_holomatchTimeLimit = EFFe_CycleOption(timeLimits, EF_ARRAY_LEN(timeLimits), s_holomatchTimeLimit, direction);
	}
	else if (s_cursor == 2)
	{
		s_holomatchForceRespawn = !s_holomatchForceRespawn;
	}
	else if (s_cursor == 3)
	{
		s_holomatchWeaponStay = !s_holomatchWeaponStay;
	}
	else if (s_cursor == 4)
	{
		s_holomatchFallingDamage = !s_holomatchFallingDamage;
	}
	else if (s_cursor == 5)
	{
		s_holomatchTeamPlay = !s_holomatchTeamPlay;
	}
	else if (s_cursor == 6 && s_holomatchTeamPlay)
	{
		s_holomatchFriendlyFire = !s_holomatchFriendlyFire;
	}
#ifdef _XBOX
	XBLF("STEFX_HM_MENU_RULE: cursor=%d direction=%d frag=%d time=%d force=%d stay=%d falling=%d team=%d friendly=%d",
		s_cursor, direction, s_holomatchPointLimit, s_holomatchTimeLimit, s_holomatchForceRespawn,
		s_holomatchWeaponStay, s_holomatchFallingDamage, s_holomatchTeamPlay, s_holomatchFriendlyFire);
#endif
}

static void EFFe_HandleHolomatchKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_ReturnToMain();
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_HOLOMATCH_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_HOLOMATCH_COUNT, 1);
		return;
	}
	if (EFFe_IsLeftKey(key) || EFFe_IsRightKey(key))
	{
		int direction = EFFe_IsRightKey(key) ? 1 : -1;
		if (s_cursor == 0)
		{
			s_holomatchPlayers += direction;
			if (s_holomatchPlayers < 1) s_holomatchPlayers = 4;
			if (s_holomatchPlayers > 4) s_holomatchPlayers = 1;
		}
		else if (s_cursor == 1)
		{
			s_holomatchMap = (s_holomatchMap + EF_FRONTEND_HOLOMATCH_MAP_COUNT + direction) % EF_FRONTEND_HOLOMATCH_MAP_COUNT;
		}
#ifdef _XBOX
		XBLF("STEFX_HM_MENU_SETUP: players=%d mapIndex=%d map='%s'", s_holomatchPlayers, s_holomatchMap, s_holomatchMapNames[s_holomatchMap]);
#endif
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}
	if (s_cursor == 2)
	{
		EFFe_SetScreen(EF_SCREEN_HOLOMATCH_ADVANCED, 0);
	}
	else if (s_cursor == 3)
	{
		int player;
		s_holomatchMenuVirtualPads =
			(qboolean)(!Q_stricmp(s_menuSmokeTarget, "holomatch-players-options"));
		for (player = 0; player < EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS; ++player)
		{
			s_holomatchPlayerCursors[player] = 0;
			s_holomatchPlayerReady[player] = qfalse;
		}
		EFFe_SetScreen(EF_SCREEN_HOLOMATCH_PLAYERS, 0);
	}
}

static void EFFe_HandleHolomatchAdvancedKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_SetScreen(EF_SCREEN_HOLOMATCH, 2);
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_HOLOMATCH_ADVANCED_COUNT, 1);
		return;
	}
	if (EFFe_IsLeftKey(key) || EFFe_IsRightKey(key) || EFFe_IsAcceptKey(key))
	{
		EFFe_AdjustHolomatchAdvanced(EFFe_IsLeftKey(key) ? -1 : 1);
	}
}

static void EFFe_AdjustHolomatchPlayer(int player, int direction)
{
	int option;

	if (player < 0 || player >= s_holomatchPlayers || player >= EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS)
	{
		return;
	}
	option = s_holomatchPlayerCursors[player];
	if (option == 0)
	{
		int characterIndex;
		for (characterIndex = 0; characterIndex < EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS; ++characterIndex)
		{
			if (s_holomatchCharacters[characterIndex].crewMember == s_holomatchPlayerCharacters[player])
			{
				break;
			}
		}
		if (characterIndex >= EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS)
		{
			characterIndex = 0;
		}
		characterIndex = (characterIndex + EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS + direction) %
			EF_FRONTEND_HOLOMATCH_PLAYABLE_CHARACTERS;
		s_holomatchPlayerCharacters[player] = s_holomatchCharacters[characterIndex].crewMember;
	}
	else if (option == 1)
	{
		s_holomatchPlayerOutfits[player] = (s_holomatchPlayerOutfits[player] + 3 + direction) % 3;
	}
	else if (option == 2)
	{
		s_holomatchPlayerControlStyles[player] =
			(s_holomatchPlayerControlStyles[player] + EF_FRONTEND_HOLOMATCH_CONTROL_STYLES + direction) %
			EF_FRONTEND_HOLOMATCH_CONTROL_STYLES;
	}
	else if (option == 3)
	{
		s_holomatchPlayerAutoswitch[player] =
			(s_holomatchPlayerAutoswitch[player] + EF_FRONTEND_HOLOMATCH_AUTOSWITCH_MODES + direction) %
			EF_FRONTEND_HOLOMATCH_AUTOSWITCH_MODES;
	}
	else if (option == 4)
	{
		s_holomatchPlayerAutoaim[player] =
			(s_holomatchPlayerAutoaim[player] + EF_FRONTEND_HOLOMATCH_AUTOAIM_MODES + direction) %
			EF_FRONTEND_HOLOMATCH_AUTOAIM_MODES;
	}
	else if (option == 5)
	{
		s_holomatchPlayerCrosshairs[player] =
			(s_holomatchPlayerCrosshairs[player] + EF_FRONTEND_HOLOMATCH_CROSSHAIRS + direction) %
			EF_FRONTEND_HOLOMATCH_CROSSHAIRS;
	}
	else if (option == 6)
	{
		s_holomatchPlayerVibration[player] = !s_holomatchPlayerVibration[player];
	}
	else if (option == 7)
	{
		s_holomatchPlayerInvertPitch[player] = !s_holomatchPlayerInvertPitch[player];
	}
	s_holomatchPlayerReady[player] = qfalse;
	#ifdef _XBOX
	XBLF("STEFX_HM_PLAYER_SETUP: player=%d option=%d direction=%d character=%d name='%s' outfit=%d control=%d autoswitch=%d autoaim=%d crosshair=%d vibration=%d invert=%d",
		player + 1, option, direction, s_holomatchPlayerCharacters[player],
		s_crewMembers[s_holomatchPlayerCharacters[player]].listName, s_holomatchPlayerOutfits[player],
		s_holomatchPlayerControlStyles[player], s_holomatchPlayerAutoswitch[player],
		s_holomatchPlayerAutoaim[player], s_holomatchPlayerCrosshairs[player] + 1,
		s_holomatchPlayerVibration[player], s_holomatchPlayerInvertPitch[player]);
	#endif
}

static qboolean EFFe_HolomatchAllPlayersReady(void)
{
	int player;
	if (EFFe_HolomatchPadForPlayer(0) < 0 || !s_holomatchPlayerReady[0])
	{
		return qfalse;
	}
	for (player = 0; player < s_holomatchPlayers; ++player)
	{
		if (EFFe_HolomatchPadForPlayer(player) >= 0 && !s_holomatchPlayerReady[player])
		{
			return qfalse;
		}
	}
	return qtrue;
}

static void EFFe_HandleHolomatchPlayerKey(int player, int key)
{
	if (player < 0 || player >= s_holomatchPlayers || player >= EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS)
	{
		return;
	}
	if (EFFe_IsBackKey(key))
	{
		if (player == 0)
		{
			s_holomatchMenuVirtualPads = qfalse;
			EFFe_SetScreen(EF_SCREEN_HOLOMATCH, 3);
		}
		else if (s_holomatchPlayerReady[player])
		{
			s_holomatchPlayerReady[player] = qfalse;
#ifdef _XBOX
			XBLF("STEFX_HM_PLAYER_READY: player=%d ready=0 localPlayers=%d", player + 1, s_holomatchPlayers);
#endif
		}
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		s_holomatchPlayerCursors[player] =
			(s_holomatchPlayerCursors[player] + EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS - 1) %
			EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS;
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		s_holomatchPlayerCursors[player] =
			(s_holomatchPlayerCursors[player] + 1) % EF_FRONTEND_HOLOMATCH_PLAYER_OPTIONS;
		return;
	}
	if (EFFe_IsLeftKey(key) || EFFe_IsRightKey(key))
	{
		EFFe_AdjustHolomatchPlayer(player, EFFe_IsRightKey(key) ? 1 : -1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}
	s_holomatchPlayerReady[player] = !s_holomatchPlayerReady[player];
#ifdef _XBOX
	XBLF("STEFX_HM_PLAYER_READY: player=%d ready=%d localPlayers=%d", player + 1,
		s_holomatchPlayerReady[player] ? 1 : 0, s_holomatchPlayers);
#endif
	if (s_holomatchPlayerReady[player] && EFFe_HolomatchAllPlayersReady())
	{
#ifdef _XBOX
		XBLF("STEFX_HM_PLAYER_READY: launch-all-ready localPlayers=%d", s_holomatchPlayers);
#endif
		EFFe_StartHolomatch();
	}
}

static void EFFe_HandleHolomatchPlayersKey(int key)
{
	EFFe_HandleHolomatchPlayerKey(0, key);
}

static void EFFe_PlayCrewVoice(void)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	int member = group->firstMember + s_crewMember;
	sfxHandle_t voice = s_assets.crewVoices[member];
	if (!voice)
	{
		voice = ui.S_RegisterSound(s_crewMembers[member].voiceName);
		s_assets.crewVoices[member] = voice;
	}
	if (voice)
	{
		ui.S_StartLocalSound(voice, CHAN_LOCAL_SOUND);
	}
#ifdef _XBOX
	XBLF("STEFX_CREW_VOICE: group='%s' member=%d name='%s' file='%s' handle=%d played=%d",
		group->title, member, s_crewMembers[member].listName, s_crewMembers[member].voiceName,
		voice, voice ? 1 : 0);
	ui.Printf("STEFX_CREW_VOICE group='%s' member=%d name='%s' file='%s' handle=%d played=%d\n",
		group->title, member, s_crewMembers[member].listName, s_crewMembers[member].voiceName,
		voice, voice ? 1 : 0);
#endif
}

static void EFFe_HandleCrewKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_ReturnToMain();
		return;
	}
	if (EFFe_IsUpKey(key) || EFFe_IsLeftKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_CREW_CATEGORY_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key) || EFFe_IsRightKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_CREW_CATEGORY_COUNT, 1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}
	if (s_cursor < EF_FRONTEND_CREW_GROUP_COUNT)
	{
		s_crewGroup = s_cursor;
		s_crewMember = 0;
		EFFe_SetScreen(EF_SCREEN_CREW_ROSTER, 0);
	}
	else if (s_cursor == 3)
	{
		EFFe_SetScreen(EF_SCREEN_CREW_HAZARD_SUIT, 0);
	}
	else
	{
		EFFe_SetScreen(EF_SCREEN_CREW_VOYAGER, 0);
	}
}

static void EFFe_HandleCrewRosterKey(int key)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	if (EFFe_IsBackKey(key))
	{
		EFFe_SetScreen(EF_SCREEN_CREW, s_crewGroup);
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		s_crewMember = (s_crewMember + group->memberCount - 1) % group->memberCount;
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		s_crewMember = (s_crewMember + 1) % group->memberCount;
		return;
	}
	if (EFFe_IsAcceptKey(key))
	{
		s_crewBioPage = 0;
		EFFe_PlayCrewVoice();
		EFFe_SetScreen(EF_SCREEN_CREW_BIO, s_crewMember);
	}
}

static void EFFe_HandleCrewBioKey(int key)
{
	const efFrontendCrewGroup_t *group = &s_crewGroups[s_crewGroup];
	int member = group->firstMember + s_crewMember;
	const efFrontendCrewMember_t *crew = &s_crewMembers[member];
	int lineCount;
	int pageCount;
	if (EFFe_IsBackKey(key))
	{
		EFFe_SetScreen(EF_SCREEN_CREW_ROSTER, s_crewMember);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}
	EFFe_LoadNormalText();
	lineCount = EFFe_WrapCrewBio(EFFe_NormalText(crew->normalTextStart + crew->normalTextCount - 1, ""));
	pageCount = (lineCount + 6) / 7;
	if (pageCount < 1) pageCount = 1;
	s_crewBioPage = (s_crewBioPage + 1) % pageCount;
#ifdef _XBOX
	XBLog_WriteCriticalf("STEFX_CREW_BIO_PAGE: group='%s' member='%s' page=%d pages=%d lines=%d",
		group->title, crew->listName, s_crewBioPage, pageCount, lineCount);
#endif
}

static void EFFe_HandleCrewStaticKey(int key)
{
	if (EFFe_IsBackKey(key) || EFFe_IsAcceptKey(key))
	{
		EFFe_SetScreen(EF_SCREEN_CREW, s_screen == EF_SCREEN_CREW_HAZARD_SUIT ? 3 : 4);
	}
}

static void EFFe_HandleConfigureKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		if (s_returnToPause)
		{
			EFFe_ReturnToPause();
		}
		else
		{
			EFFe_ReturnToMain();
		}
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_CONFIGURE_COUNT, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(EF_FRONTEND_CONFIGURE_COUNT, 1);
		return;
	}
	if (!EFFe_IsAcceptKey(key))
	{
		return;
	}

#ifdef _XBOX
	XBLF("STEFX: EF configure activate index=%d item='%s'", s_cursor, s_configureItems[s_cursor]);
#endif
	if (s_cursor == 0)
	{
		EFFe_SetScreen(EF_SCREEN_AUDIO, 0);
	}
	else if (s_cursor == 1)
	{
		EFFe_SetScreen(EF_SCREEN_VIDEO, 0);
	}
	else
	{
		EFFe_SetScreen(EF_SCREEN_CONTROLLER, 0);
	}
}

static void EFFe_HandleAudioKey(int key)
{
	if (EFFe_IsBackKey(key))
	{
		EFFe_CancelAudioState();
		EFFe_ReturnToConfigure();
		return;
	}
	if (EFFe_IsAcceptKey(key))
	{
		EFFe_ApplyAudioState(qtrue);
		EFFe_ReturnToConfigure();
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_MoveCursor(4, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_MoveCursor(4, 1);
		return;
	}
	if (EFFe_IsLeftKey(key))
	{
		EFFe_AdjustAudio(-1);
		return;
	}
	if (EFFe_IsRightKey(key))
	{
		EFFe_AdjustAudio(1);
		return;
	}
}

static void EFFe_HandleVideoKey(int key)
{
	if (key == A_JOY16)
	{
		s_videoCorner = (s_videoCorner + 1) & 3;
#ifdef _XBOX
		XBLF("STEFX: EF video screen-size switch corner=%d", s_videoCorner);
#endif
		return;
	}
	if (key == A_JOY14)
	{
		s_videoSafeLeft = 0;
		s_videoSafeTop = 0;
		s_videoSafeRight = 0;
		s_videoSafeBottom = 0;
#ifdef _XBOX
		XBLF("STEFX_SAFE_AREA: default corner=%d", s_videoCorner);
#endif
		return;
	}
	if (EFFe_IsBackKey(key))
	{
		EFFe_CancelVideoState();
#ifdef _XBOX
		XBLF("STEFX_SAFE_AREA: close-cancel key=%d corner=%d", key, s_videoCorner);
#endif
		EFFe_ReturnToConfigure();
		return;
	}
	if (EFFe_IsAcceptKey(key))
	{
		EFFe_ApplyVideoState();
		EFFe_ReturnToConfigure();
		return;
	}
	if (EFFe_IsLeftKey(key))
	{
		EFFe_AdjustVideoCorner(-1, 0);
		return;
	}
	if (EFFe_IsRightKey(key))
	{
		EFFe_AdjustVideoCorner(1, 0);
		return;
	}
	if (EFFe_IsUpKey(key))
	{
		EFFe_AdjustVideoCorner(0, -1);
		return;
	}
	if (EFFe_IsDownKey(key))
	{
		EFFe_AdjustVideoCorner(0, 1);
	}
}

static void EFFe_HandleControllerKey(int key)
{
	if (EFFe_IsBackKey(key) || EFFe_IsAcceptKey(key))
	{
#ifdef _XBOX
		XBLF("STEFX: EF controller menu close key=%d", key);
#endif
		EFFe_ReturnToConfigure();
	}
}

static void EFFe_RunMenuSmoke(int realtime)
{
#ifdef _XBOX
	char target[32];
	int desiredCursor;
	int newGameCursor;
	qboolean targetCoop;
	qboolean targetHolomatch;
	qboolean targetCrew;
	qboolean targetSubmenu;

	ui.Cvar_VariableStringBuffer("stefx_menu_smoke", target, sizeof(target));
	if (!target[0] || !Q_stricmp(target, "0"))
	{
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	targetCoop = (qboolean)(!Q_stricmp(target, "coop") ||
		!Q_stricmp(target, "coop-new") ||
		!Q_stricmp(target, "coop-load") ||
		!Q_stricmp(target, "coop-load-accept"));
	targetHolomatch = (qboolean)(!Q_stricmp(target, "holomatch") ||
		!Q_stricmp(target, "holomatch-advanced") ||
		!Q_stricmp(target, "holomatch-players") ||
		!Q_stricmp(target, "holomatch-players-options") ||
		!Q_stricmp(target, "holomatch-engage"));
	targetCrew = (qboolean)(!Q_stricmp(target, "crew") ||
		!Q_stricmp(target, "crew-roster") ||
		!Q_stricmp(target, "crew-bio") ||
		!Q_stricmp(target, "crew-bio-page2") ||
		!Q_stricmp(target, "crew-hazard") ||
		!Q_stricmp(target, "crew-voyager") ||
		!Q_stricmp(target, "crew-tour"));
	targetSubmenu = (qboolean)(!Q_stricmp(target, "coop-new") ||
		!Q_stricmp(target, "coop-load") ||
		!Q_stricmp(target, "coop-load-accept") ||
		!Q_stricmp(target, "holomatch-advanced") ||
		!Q_stricmp(target, "holomatch-players") ||
		!Q_stricmp(target, "holomatch-players-options") ||
		!Q_stricmp(target, "holomatch-engage") ||
		!Q_stricmp(target, "crew-roster") ||
		!Q_stricmp(target, "crew-bio") ||
		!Q_stricmp(target, "crew-bio-page2") ||
		!Q_stricmp(target, "crew-hazard") ||
		!Q_stricmp(target, "crew-voyager") ||
		!Q_stricmp(target, "crew-tour"));
	desiredCursor = targetCoop ? 2 : (targetHolomatch ? 3 : (targetCrew ? 5 : 0));
	newGameCursor = !Q_stricmp(target, "virtualvoyager") ? 8 :
		(!Q_stricmp(target, "tutorial") ? 6 : (!Q_stricmp(target, "engage") ? 7 : 0));
	if (Q_stricmp(target, "tutorial") && Q_stricmp(target, "engage") &&
		Q_stricmp(target, "newgame") && Q_stricmp(target, "virtualvoyager") &&
		!targetCoop && !targetHolomatch && !targetCrew)
	{
		ui.Printf("STEFX_MENU_SMOKE invalid target='%s'\n", target);
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	if (!s_menuSmokeStage || Q_stricmp(target, s_menuSmokeTarget))
	{
		Q_strncpyz(s_menuSmokeTarget, target, sizeof(s_menuSmokeTarget));
		s_menuSmokeStage = 1;
		// Drive diagnostic traversal by rendered frames.  Menu transitions can
		// rebase the UI clock while sv_killserver is processed.
		s_menuSmokeNextRealtime = 0;
		s_menuSmokeMainDownRemaining = desiredCursor;
		s_menuSmokeDownRemaining = newGameCursor;
		ui.Printf("STEFX_MENU_SMOKE begin target='%s' mainCursor=%d subCursor=%d screen='%s' cursor=%d realtime=%d\n",
			s_menuSmokeTarget,
			desiredCursor,
			newGameCursor,
			EFFe_ScreenName(s_screen),
			s_cursor,
			realtime);
		return;
	}

	if (realtime < s_menuSmokeNextRealtime)
	{
		return;
	}

	if (s_menuSmokeStage == 1)
	{
		int acceptDelay;

		if (s_screen != EF_SCREEN_MAIN)
		{
			ui.Printf("STEFX_MENU_SMOKE wait-main screen='%s' cursor=%d realtime=%d\n",
				EFFe_ScreenName(s_screen),
				s_cursor,
				realtime);
			s_menuSmokeNextRealtime = realtime + 250;
			return;
		}
		if (s_menuSmokeMainDownRemaining > 0)
		{
			ui.Printf("STEFX_MENU_SMOKE main-down remainingBefore=%d cursor=%d realtime=%d\n",
				s_menuSmokeMainDownRemaining,
				s_cursor,
				realtime);
			EFFe_HandleMainKey(A_CURSOR_DOWN);
			s_menuSmokeMainDownRemaining--;
			s_menuSmokeNextRealtime = 0;
			return;
		}
		if (s_cursor != desiredCursor)
		{
			ui.Printf("STEFX_MENU_SMOKE main-align target='%s' cursorBefore=%d cursorAfter=%d realtime=%d\n",
				s_menuSmokeTarget,
				s_cursor,
				desiredCursor,
				realtime);
				s_cursor = desiredCursor;
		}
		acceptDelay = !Q_stricmp(target, "holomatch") ? (int)ui.Cvar_VariableValue("stefx_menu_smoke_accept_delay") : 0;
		if (acceptDelay > 0)
		{
			if (acceptDelay > 10000)
			{
				acceptDelay = 10000;
			}
			ui.Printf("STEFX_MENU_SMOKE selection-hold target='%s' cursor=%d delay=%d realtime=%d\n",
				s_menuSmokeTarget,
				s_cursor,
				acceptDelay,
				realtime);
			s_menuSmokeStage = 4;
			s_menuSmokeNextRealtime = realtime + acceptDelay;
			return;
		}
		ui.Printf("STEFX_MENU_SMOKE main-accept target='%s' cursor=%d realtime=%d\n", s_menuSmokeTarget, s_cursor, realtime);
		EFFe_HandleMainKey(A_ENTER);
		if (!targetSubmenu && (targetCoop || targetHolomatch || targetCrew))
		{
			ui.Cvar_Set("stefx_menu_smoke", "0");
			s_menuSmokeStage = 0;
			s_menuSmokeTarget[0] = '\0';
			s_menuSmokeMainDownRemaining = 0;
			return;
		}
		if (targetSubmenu)
		{
			s_menuSmokeStage = 5;
			// Opening a child menu can rebase the UI clock while sv_killserver is
			// processed.  Stage 5 already waits for the exact destination screen.
			s_menuSmokeNextRealtime = 0;
			return;
		}
		s_menuSmokeStage = 2;
		s_menuSmokeNextRealtime = realtime + 500;
		return;
	}

	if (s_menuSmokeStage == 4)
	{
		if (s_screen != EF_SCREEN_MAIN)
		{
			ui.Printf("STEFX_MENU_SMOKE abort-before-main-accept screen='%s' cursor=%d realtime=%d\n",
				EFFe_ScreenName(s_screen),
				s_cursor,
				realtime);
			ui.Cvar_Set("stefx_menu_smoke", "0");
			s_menuSmokeStage = 0;
			s_menuSmokeTarget[0] = '\0';
			s_menuSmokeMainDownRemaining = 0;
			return;
		}
		if (s_cursor != desiredCursor)
		{
			ui.Printf("STEFX_MENU_SMOKE main-realign target='%s' cursorBefore=%d cursorAfter=%d realtime=%d\n",
				s_menuSmokeTarget,
				s_cursor,
				desiredCursor,
				realtime);
			s_cursor = desiredCursor;
		}
		ui.Printf("STEFX_MENU_SMOKE main-accept target='%s' cursor=%d realtime=%d\n", s_menuSmokeTarget, s_cursor, realtime);
		EFFe_HandleMainKey(A_ENTER);
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	if (s_menuSmokeStage == 5)
	{
		int submenuCursor;
		efFrontendScreen_t expectedScreen;

		if (targetCoop)
		{
			expectedScreen = EF_SCREEN_COOP;
			submenuCursor = (!Q_stricmp(s_menuSmokeTarget, "coop-load") ||
				!Q_stricmp(s_menuSmokeTarget, "coop-load-accept")) ? 1 : 0;
		}
		else if (targetHolomatch)
		{
			expectedScreen = EF_SCREEN_HOLOMATCH;
			submenuCursor = !Q_stricmp(s_menuSmokeTarget, "holomatch-advanced") ? 2 : 3;
			if (!Q_stricmp(s_menuSmokeTarget, "holomatch-players-options"))
			{
				s_holomatchPlayers = EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS;
			}
		}
		else
		{
			expectedScreen = EF_SCREEN_CREW;
			submenuCursor = !Q_stricmp(s_menuSmokeTarget, "crew-hazard") ? 3 :
				(!Q_stricmp(s_menuSmokeTarget, "crew-voyager") ? 4 : 0);
		}
		if (s_screen != expectedScreen)
		{
			ui.Printf("STEFX_MENU_SMOKE wait-submenu target='%s' screen='%s' cursor=%d realtime=%d\n",
				s_menuSmokeTarget,
				EFFe_ScreenName(s_screen),
				s_cursor,
				realtime);
			s_menuSmokeNextRealtime = realtime + 250;
			return;
		}
		if (!Q_stricmp(s_menuSmokeTarget, "crew-tour"))
		{
			s_cursor = 0;
			s_menuSmokeDownRemaining = 0;
			s_menuSmokeStage = 8;
			s_menuSmokeNextRealtime = realtime + 30000;
			ui.Printf("STEFX_MENU_SMOKE crew-tour screen='%s' phase=categories realtime=%d\n",
				EFFe_ScreenName(s_screen), realtime);
			return;
		}
		s_cursor = submenuCursor;
		ui.Printf("STEFX_MENU_SMOKE submenu-accept target='%s' screen='%s' cursor=%d realtime=%d\n",
			s_menuSmokeTarget,
			EFFe_ScreenName(s_screen),
			s_cursor,
			realtime);
		if (targetCoop)
		{
			EFFe_HandleCoopKey(A_ENTER);
		}
		else if (targetHolomatch)
		{
			EFFe_HandleHolomatchKey(A_ENTER);
		}
		else
		{
			EFFe_HandleCrewKey(A_ENTER);
		}
		if (!Q_stricmp(s_menuSmokeTarget, "crew-bio") ||
			!Q_stricmp(s_menuSmokeTarget, "crew-bio-page2") ||
			!Q_stricmp(s_menuSmokeTarget, "holomatch-engage") ||
			!Q_stricmp(s_menuSmokeTarget, "coop-load") ||
			!Q_stricmp(s_menuSmokeTarget, "coop-load-accept"))
		{
			s_menuSmokeStage = 6;
			s_menuSmokeNextRealtime = 0;
			return;
		}
		if (!Q_stricmp(s_menuSmokeTarget, "holomatch-players-options"))
		{
			s_menuSmokeStage = 7;
			s_menuSmokeNextRealtime = realtime + 500;
			return;
		}
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	if (s_menuSmokeStage == 8)
	{
		const char *phase = "done";
		switch (s_menuSmokeDownRemaining)
		{
		case 0:
			s_crewGroup = 0;
			s_crewMember = 0;
			s_cursor = 0;
			EFFe_HandleCrewKey(A_ENTER);
			phase = "senior-roster";
			break;
		case 1:
			EFFe_HandleCrewRosterKey(A_ENTER);
			phase = "senior-bio";
			break;
		case 2:
			EFFe_HandleCrewBioKey(A_ENTER);
			phase = "senior-bio-page-2";
			break;
		case 3:
			s_crewGroup = 1;
			s_crewMember = 0;
			EFFe_SetScreen(EF_SCREEN_CREW_ROSTER, 0);
			phase = "alpha-roster";
			break;
		case 4:
			s_crewGroup = 2;
			s_crewMember = 0;
			EFFe_SetScreen(EF_SCREEN_CREW_ROSTER, 0);
			phase = "beta-roster";
			break;
		case 5:
			EFFe_SetScreen(EF_SCREEN_CREW_HAZARD_SUIT, 0);
			phase = "hazard-suit";
			break;
		case 6:
			EFFe_SetScreen(EF_SCREEN_CREW_VOYAGER, 0);
			phase = "voyager";
			break;
		default:
			ui.Printf("STEFX_MENU_SMOKE crew-tour complete realtime=%d\n", realtime);
			ui.Cvar_Set("stefx_menu_smoke", "0");
			s_menuSmokeStage = 0;
			s_menuSmokeTarget[0] = '\0';
			s_menuSmokeMainDownRemaining = 0;
			s_menuSmokeDownRemaining = 0;
			return;
		}
		ui.Printf("STEFX_MENU_SMOKE crew-tour screen='%s' phase=%s realtime=%d\n",
			EFFe_ScreenName(s_screen), phase, realtime);
		s_menuSmokeDownRemaining++;
		s_menuSmokeNextRealtime = realtime + 10000;
		return;
	}

	if (s_menuSmokeStage == 7)
	{
		static const int controlSteps[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 7, 8, 4, 5 };
		static const int autoaimSteps[EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS] = { 1, 0, 3, 2 };
		qboolean engageAfterOptions = (qboolean)(ui.Cvar_VariableValue("stefx_menu_smoke_options_engage") != 0.0f);
		int player;
		int step;

		if (s_screen != EF_SCREEN_HOLOMATCH_PLAYERS)
		{
			s_menuSmokeNextRealtime = realtime + 250;
			return;
		}
		for (player = 0; player < EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS; ++player)
		{
			s_holomatchPlayerCursors[player] = 1;
			EFFe_AdjustHolomatchPlayer(player, 1);
			s_holomatchPlayerCursors[player] = 2;
			for (step = 0; step < controlSteps[player]; ++step)
			{
				EFFe_AdjustHolomatchPlayer(player, 1);
			}
			s_holomatchPlayerCursors[player] = 3;
			for (step = 0; step <= player % 2; ++step)
			{
				EFFe_AdjustHolomatchPlayer(player, 1);
			}
			s_holomatchPlayerCursors[player] = 4;
			for (step = 0; step < autoaimSteps[player]; ++step)
			{
				EFFe_AdjustHolomatchPlayer(player, 1);
			}
			s_holomatchPlayerCursors[player] = 5;
			for (step = 0; step <= player; ++step)
			{
				EFFe_AdjustHolomatchPlayer(player, 1);
			}
			if (player & 1)
			{
				s_holomatchPlayerCursors[player] = 6;
				EFFe_AdjustHolomatchPlayer(player, 1);
			}
			s_holomatchPlayerCursors[player] = 7;
			EFFe_AdjustHolomatchPlayer(player, 1);
		}
		s_cursor = 0;
		for (player = 0; player < EF_FRONTEND_HOLOMATCH_LOCAL_PLAYERS; ++player)
		{
			s_holomatchPlayerCursors[player] = 4;
		}
		ui.Printf("STEFX_MENU_SMOKE options-proof target='%s' players=%d screen='%s' realtime=%d\n",
			s_menuSmokeTarget, s_holomatchPlayers, EFFe_ScreenName(s_screen), realtime);
		if (engageAfterOptions)
		{
			for (player = 0; player < s_holomatchPlayers; ++player)
			{
				s_holomatchPlayerReady[player] = qfalse;
			}
			ui.Cvar_Set("stefx_menu_smoke_options_engage", "0");
			for (player = 0; player < s_holomatchPlayers; ++player)
			{
				EFFe_HandleHolomatchPlayerKey(player, A_ENTER);
			}
			ui.Printf("STEFX_MENU_SMOKE options-engage players=%d realtime=%d\n",
				s_holomatchPlayers, realtime);
		}
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	if (s_menuSmokeStage == 6)
	{
		if (!Q_stricmp(s_menuSmokeTarget, "coop-load") ||
			!Q_stricmp(s_menuSmokeTarget, "coop-load-accept"))
		{
			qboolean acceptLoad = (qboolean)!Q_stricmp(s_menuSmokeTarget, "coop-load-accept");
			if (s_screen != EF_SCREEN_LOADGAME)
			{
				ui.Printf("STEFX_MENU_SMOKE wait-coop-load screen='%s' cursor=%d realtime=%d\n",
					EFFe_ScreenName(s_screen), s_cursor, realtime);
				s_menuSmokeNextRealtime = realtime + 250;
				return;
			}
			if (acceptLoad && s_loadCount > 0)
			{
				ui.Cvar_Set("stefx_menu_smoke", "0");
				EFFe_HandleLoadGameKey(A_ENTER);
				s_menuSmokeStage = 0;
				s_menuSmokeTarget[0] = '\0';
				s_menuSmokeMainDownRemaining = 0;
				return;
			}
			ui.Printf("STEFX_MENU_SMOKE coop-load-ready screen='%s' saves=%d cooperative=%d realtime=%d\n",
				EFFe_ScreenName(s_screen), s_loadCount, s_loadForCoop ? 1 : 0, realtime);
		}
		else if (!Q_stricmp(s_menuSmokeTarget, "crew-bio") || !Q_stricmp(s_menuSmokeTarget, "crew-bio-page2"))
		{
			if (s_screen != EF_SCREEN_CREW_ROSTER)
			{
				s_menuSmokeNextRealtime = realtime + 250;
				return;
			}
			EFFe_HandleCrewRosterKey(A_ENTER);
			if (!Q_stricmp(s_menuSmokeTarget, "crew-bio-page2"))
			{
				EFFe_HandleCrewBioKey(A_ENTER);
			}
		}
		else
		{
			int player;
			if (s_screen != EF_SCREEN_HOLOMATCH_PLAYERS)
			{
				s_menuSmokeNextRealtime = realtime + 250;
				return;
			}
			for (player = 0; player < s_holomatchPlayers; ++player)
			{
				EFFe_HandleHolomatchPlayerKey(player, A_ENTER);
			}
		}
		ui.Printf("STEFX_MENU_SMOKE final-accept target='%s' screen='%s' realtime=%d\n",
			s_menuSmokeTarget, EFFe_ScreenName(s_screen), realtime);
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
		return;
	}

	if (s_menuSmokeStage == 2)
	{
		if (s_screen != EF_SCREEN_NEWGAME)
		{
			ui.Printf("STEFX_MENU_SMOKE wait-newgame screen='%s' cursor=%d realtime=%d\n",
				EFFe_ScreenName(s_screen),
				s_cursor,
				realtime);
			s_menuSmokeNextRealtime = realtime + 250;
			return;
		}
		if (s_menuSmokeDownRemaining > 0)
		{
			ui.Printf("STEFX_MENU_SMOKE newgame-down remainingBefore=%d cursor=%d realtime=%d\n",
				s_menuSmokeDownRemaining,
				s_cursor,
				realtime);
			EFFe_HandleNewGameKey(A_CURSOR_DOWN);
			s_menuSmokeDownRemaining--;
			s_menuSmokeNextRealtime = realtime + 160;
			return;
		}
		if (s_cursor != newGameCursor)
		{
			ui.Printf("STEFX_MENU_SMOKE newgame-align target='%s' cursorBefore=%d cursorAfter=%d realtime=%d\n",
				s_menuSmokeTarget,
				s_cursor,
				newGameCursor,
				realtime);
			s_cursor = newGameCursor;
		}
		s_menuSmokeStage = 3;
		s_menuSmokeNextRealtime = realtime + 250;
		return;
	}

	if (s_menuSmokeStage == 3)
	{
		if (s_screen != EF_SCREEN_NEWGAME)
		{
			ui.Printf("STEFX_MENU_SMOKE abort-before-accept screen='%s' cursor=%d realtime=%d\n",
				EFFe_ScreenName(s_screen),
				s_cursor,
				realtime);
			ui.Cvar_Set("stefx_menu_smoke", "0");
			s_menuSmokeStage = 0;
			s_menuSmokeMainDownRemaining = 0;
			return;
		}
		if (!Q_stricmp(s_menuSmokeTarget, "newgame"))
		{
			ui.Printf("STEFX_MENU_SMOKE newgame-capture-ready cursor=%d\n", s_cursor);
			ui.Cvar_Set("stefx_menu_smoke", "0");
			s_menuSmokeStage = 0;
			return;
		}
		ui.Printf("STEFX_MENU_SMOKE newgame-accept target='%s' cursor=%d item='%s' realtime=%d\n",
			s_menuSmokeTarget,
			s_cursor,
			(s_cursor >= 0 && s_cursor < EF_FRONTEND_NEWGAME_COUNT) ? s_newgameItems[s_cursor] : "<bad>",
			realtime);
		EFFe_HandleNewGameKey(A_ENTER);
		ui.Cvar_Set("stefx_menu_smoke", "0");
		s_menuSmokeStage = 0;
		s_menuSmokeTarget[0] = '\0';
		s_menuSmokeMainDownRemaining = 0;
	}
#else
	(void)realtime;
#endif
}

qboolean UI_EFMainMenu_IsActive(void)
{
	return s_active;
}

void UI_EFMainMenu_Open(void)
{
	EFFe_OpenScreen(EF_SCREEN_MAIN, 0, "ef-main-open");
#ifdef _XBOX
	ui.Printf("STEFX: EF main menu open realtime=%d catcher=0x%x\n", uis.realtime, ui.Key_GetCatcher());
#endif
}

void UI_EFMainMenu_OpenNewGame(void)
{
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	extern void Sys_Reboot(const char *reason, const void *pData);
	Sys_Reboot("singleplayer", NULL);
#else
	EFFe_OpenScreen(EF_SCREEN_NEWGAME, 0, "ef-newgame-open");
#endif
}

void UI_EFMainMenu_OpenLoadGame(void)
{
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	extern void Sys_Reboot(const char *reason, const void *pData);
	Sys_Reboot("singleplayer", NULL);
#else
	s_loadForCoop = qfalse;
	EFFe_OpenScreen(EF_SCREEN_LOADGAME, 0, "ef-loadgame-open");
#endif
}

void UI_EFMainMenu_OpenLoadGameFromPause(void)
{
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	UI_EFMainMenu_OpenLoadGame();
#else
	s_loadForCoop = qfalse;
	EFFe_OpenScreenInternal(EF_SCREEN_LOADGAME, 0, "ef-pause-loadgame-open", qtrue, qtrue);
#endif
}

void UI_EFMainMenu_OpenSaveGame(void)
{
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
#ifdef _XBOX
	XBLog_Write("STEFX_SAVELOAD_MENU: save screen ignored for Holomatch personality");
#endif
#else
	EFFe_OpenScreenInternal(EF_SCREEN_SAVEGAME, 0, "ef-pause-savegame-open", qtrue, qtrue);
#endif
}

void UI_EFMainMenu_OpenConfigure(void)
{
	EFFe_OpenScreen(EF_SCREEN_CONFIGURE, 0, "ef-configure-open");
}

void UI_EFMainMenu_OpenConfigureFromPause(void)
{
	EFFe_OpenScreenInternal(EF_SCREEN_CONFIGURE, 0, "ef-pause-configure-open", qtrue, qtrue);
}

void UI_EFMainMenu_OpenAudio(void)
{
	EFFe_OpenScreen(EF_SCREEN_AUDIO, 0, "ef-audio-open");
}

void UI_EFMainMenu_OpenVideo(void)
{
	EFFe_OpenScreen(EF_SCREEN_VIDEO, 0, "ef-video-open");
}

void UI_EFMainMenu_OpenController(void)
{
	EFFe_OpenScreen(EF_SCREEN_CONTROLLER, 0, "ef-controller-open");
}

void UI_EFMainMenu_OpenCrew(void)
{
	EFFe_OpenScreen(EF_SCREEN_CREW, 0, "ef-crew-open");
}

void UI_EFMainMenu_StartSplitScreenBaseline(void)
{
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	extern void Sys_Reboot(const char *reason, const void *pData);
	Sys_Reboot("singleplayer", NULL);
#else
	EFFe_OpenScreen(EF_SCREEN_COOP, 0, "ef-coop-open");
#endif
}

void UI_EFMainMenu_StartHolomatchBaseline(void)
{
	EFFe_OpenScreen(EF_SCREEN_HOLOMATCH, 0, "ef-holomatch-open");
}

void UI_EFMainMenu_OpenStub(const char *title, const char *line)
{
	Q_strncpyz(s_stubTitle, title ? title : "ELITE FORCE", sizeof(s_stubTitle));
	Q_strncpyz(s_stubLine, line ? line : "THIS MENU IS NOT AVAILABLE YET", sizeof(s_stubLine));
	EFFe_OpenScreen(EF_SCREEN_STUB, 0, "ef-stub-open");
}

void UI_EFMainMenu_Deactivate(void)
{
	if (!s_active)
	{
		return;
	}

	s_active = qfalse;
	s_screen = EF_SCREEN_MAIN;
#ifdef _XBOX
	XBLF("STEFX: EF frontend deactivate catcher=0x%x", ui.Key_GetCatcher());
	ui.Printf("STEFX: EF frontend deactivate catcher=0x%x\n", ui.Key_GetCatcher());
#endif
}

void UI_EFMainMenu_Draw(int realtime)
{
	int i;
	static unsigned int s_drawFrames = 0;

	if (!s_active)
	{
		return;
	}

	if (s_screen == EF_SCREEN_MAIN)
	{
		EFFe_DrawFrame("ELITE FORCE : MAIN MENU", qfalse);
		for (i = 0; i < EF_FRONTEND_BUTTON_COUNT; i++)
		{
			EFFe_DrawButton(&s_buttons[i], i);
		}
	}
	else
	{
		EFFe_DrawChildScreen();
	}

	if (!s_loggedDraw)
	{
#ifdef _XBOX
		XBLF("STEFX: EF frontend first draw screen='%s' realtime=%d cursor=%d", EFFe_ScreenName(s_screen), realtime, s_cursor);
		ui.Printf("STEFX: EF frontend first draw screen='%s' realtime=%d cursor=%d\n", EFFe_ScreenName(s_screen), realtime, s_cursor);
#endif
		s_loggedDraw = qtrue;
	}
	EFFe_RunMenuSmoke(realtime);
	++s_drawFrames;
	if ((s_drawFrames % 300) == 0)
	{
#ifdef _XBOX
		XBLF("STEFX: EF frontend heartbeat screen='%s' frames=%u realtime=%d cursor=%d", EFFe_ScreenName(s_screen), s_drawFrames, realtime, s_cursor);
		ui.Printf("STEFX: EF frontend heartbeat screen='%s' frames=%u realtime=%d cursor=%d\n", EFFe_ScreenName(s_screen), s_drawFrames, realtime, s_cursor);
#endif
	}
}

qboolean UI_EFMainMenu_WantsControllerInput(void)
{
	return (qboolean)(s_active && s_screen == EF_SCREEN_HOLOMATCH_PLAYERS);
}

void UI_EFMainMenu_ControllerKeyEvent(int controller, int key, qboolean down)
{
	int player;

	if (!UI_EFMainMenu_WantsControllerInput() || !down)
	{
		return;
	}
#ifdef _XBOX
	player = CL_STEFX_SplitScreen_LocalSlotForPad(controller);
#else
	player = controller;
#endif
	if (player < 0 || player >= s_holomatchPlayers)
	{
#ifdef _XBOX
		XBLF("STEFX_HM_MENU_PAD: ignored port=%d slot=%d players=%d key=%d", controller, player,
			s_holomatchPlayers, key);
#endif
		return;
	}
#ifdef _XBOX
	XBLF("STEFX_HM_MENU_PAD: port=%d slot=%d option=%d key=%d", controller, player,
		s_holomatchPlayerCursors[player], key);
#endif
	EFFe_HandleHolomatchPlayerKey(player, key);
}

void UI_EFMainMenu_KeyEvent(int key, qboolean down)
{
	if (!s_active || !down)
	{
		return;
	}

#ifdef _XBOX
	ui.Printf("STEFX_MENU_KEY screen='%s' key=%d cursorBefore=%d catcher=0x%x\n",
		EFFe_ScreenName(s_screen),
		key,
		s_cursor,
		ui.Key_GetCatcher());
#endif

	switch (s_screen)
	{
	case EF_SCREEN_MAIN:
		EFFe_HandleMainKey(key);
		break;
	case EF_SCREEN_NEWGAME:
		EFFe_HandleNewGameKey(key);
		break;
	case EF_SCREEN_LOADGAME:
		EFFe_HandleLoadGameKey(key);
		break;
	case EF_SCREEN_SAVEGAME:
		EFFe_HandleSaveGameKey(key);
		break;
	case EF_SCREEN_COOP:
		EFFe_HandleCoopKey(key);
		break;
	case EF_SCREEN_COOP_NEW:
		EFFe_HandleCoopNewKey(key);
		break;
	case EF_SCREEN_HOLOMATCH:
		EFFe_HandleHolomatchKey(key);
		break;
	case EF_SCREEN_HOLOMATCH_ADVANCED:
		EFFe_HandleHolomatchAdvancedKey(key);
		break;
	case EF_SCREEN_HOLOMATCH_PLAYERS:
		EFFe_HandleHolomatchPlayersKey(key);
		break;
	case EF_SCREEN_CREW:
		EFFe_HandleCrewKey(key);
		break;
	case EF_SCREEN_CREW_ROSTER:
		EFFe_HandleCrewRosterKey(key);
		break;
	case EF_SCREEN_CREW_BIO:
		EFFe_HandleCrewBioKey(key);
		break;
	case EF_SCREEN_CREW_HAZARD_SUIT:
	case EF_SCREEN_CREW_VOYAGER:
		EFFe_HandleCrewStaticKey(key);
		break;
	case EF_SCREEN_CONFIGURE:
		EFFe_HandleConfigureKey(key);
		break;
	case EF_SCREEN_AUDIO:
		EFFe_HandleAudioKey(key);
		break;
	case EF_SCREEN_VIDEO:
		EFFe_HandleVideoKey(key);
		break;
	case EF_SCREEN_CONTROLLER:
		EFFe_HandleControllerKey(key);
		break;
	case EF_SCREEN_STUB:
		if (EFFe_IsAcceptKey(key) || EFFe_IsBackKey(key))
		{
			EFFe_ReturnToMain();
		}
		break;
	default:
		break;
	}

#ifdef _XBOX
	ui.Printf("STEFX_MENU_KEY_DONE screen='%s' key=%d cursorAfter=%d catcher=0x%x active=%d\n",
		EFFe_ScreenName(s_screen),
		key,
		s_cursor,
		ui.Key_GetCatcher(),
		s_active ? 1 : 0);
#endif
}
