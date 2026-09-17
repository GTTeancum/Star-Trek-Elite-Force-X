#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include <string.h>
#include <limits.h>

#include "../game/q_shared.h"
#include "../renderer/tr_types.h"
#include "../qcommon/qcommon.h"
#include "ui_public.h"
#include "ui_shared.h"

#define MAX_PLAYERMODELS 32
#define MAX_DEFERRED_SCRIPT		1024

//
// ui_qmenu.c
//
#define	MAX_EDIT_LINE			256

typedef struct {
	int		cursor;
	int		scroll;
	int		widthInChars;
	char	buffer[MAX_EDIT_LINE];
	int		maxchars;
	int		style;
	int		textEnum;		// Label
	int		textcolor;		// Normal color
	int		textcolor2;		// Highlight color
} uifield_t;

//
// Elite Force qmenu support.  The active Xbox UI API stays JA-derived, but
// EF-owned screens need the original qmenu model instead of parser menus.
//
#define MAX_MENUDEPTH			8
#define MAX_QMENUITEMS			64

#define MTYPE_SLIDER			0
#define MTYPE_LIST				1
#define MTYPE_ACTION			2
#define MTYPE_SPINCONTROL		3
#define MTYPE_SEPARATOR			4
#define MTYPE_FIELD				5
#define MTYPE_RADIOBUTTON		6
#define MTYPE_BITMAP			7
#define MTYPE_TEXT				8
#define MTYPE_SCROLLLIST		9

#define QMF_LEFT_JUSTIFY		0x00000001
#define QMF_GRAYED				0x00000002
#define QMF_NUMBERSONLY			0x00000004
#define QMF_HIGHLIGHT_IF_FOCUS	0x00000008
#define QMF_BLINK				0x00000010
#define QMF_CENTER_JUSTIFY		0x00000020
#define QMF_RIGHT_JUSTIFY		0x00000040
#define QMF_HASMOUSEFOCUS		0x00000080
#define QMF_OWNERDRAW			0x00000100
#define QMF_RBONOFFSTYLE		0x00000200
#define QMF_HIGHLIGHT			0x00000400
#define QMF_MOUSEONLY			0x00000800
#define QMF_HIDDEN				0x00001000
#define QMF_SMALLFONT			0x00002000
#define QMF_INACTIVE			0x00004000
#define QMF_HIGHLIGHTIFFOCUS2	0x00008000

#define QM_GOTFOCUS				1
#define QM_LOSTFOCUS			2
#define QM_ACTIVATED			3

typedef struct _tag_menuframework
{
	const char* title;
	int	cursor;
	int cursor_prev;

	int	nitems;
	void *items[MAX_QMENUITEMS];

	const char *statusbar;

	void (*opening) (void);
	void (*closing) (void);
	void (*draw) (void);
	sfxHandle_t (*key) (int key);

	qboolean	wrapAround;
	qboolean	fullscreen;

	qboolean	initialized;
	float		openingStart;
	float		closingStart;
	float		subSeqStatus[8];
	int			cnt;
	int			descX;
	int			descY;
	int			listX;
	int			listY;
	int			titleX;
	int			titleY;
	int			titleI;
	int			footNoteEnum;
} menuframework_s;

typedef struct
{
	int type;
	const char *name;
	int	id;
	int x, y;
	int left;
	int	top;
	int	right;
	int	bottom;
	menuframework_s *parent;
	int menuPosition;
	unsigned flags;

	const char *statusbar;

	void (*callback)( void *self, int notification );
	void (*statusbarfunc)( void *self );
	void (*ownerdraw)( void *self );
} menucommon_s;


typedef struct
{
	menucommon_s generic;
	uifield_t	field;
} menufield_s;

typedef struct
{
	menucommon_s generic;
	float minvalue;
	float maxvalue;
	float curvalue;
	int focusWidth;
	int focusHeight;
	int color;
	int color2;
	int shader;
	int width;
	int height;
	char *thumbName;
	int thumbShader;
	int thumbWidth;
	int thumbHeight;
	int thumbColor;
	int thumbColor2;
	int thumbGraphicWidth;
	char *picName;
	int picShader;
	int picWidth;
	int picHeight;
	int picX;
	int picY;
	int textX;
	int textY;
	int textEnum;
	int textcolor;
	int textcolor2;
	float range;
} menuslider_s;

typedef struct
{
	menucommon_s generic;
	int	oldvalue;
	int curvalue;
	int	numitems;
	int	top;
	const char **itemnames;
	int		*listnames;
	int width;
	int height;
	int	columns;
	int	seperation;
	int color;
	int color2;
	int textEnum;
	int textX;
	int textY;
	int textcolor;
	int textcolor2;
	byte updated;
} menulist_s;

typedef struct
{
	menucommon_s generic;
	int color;
	int color2;
	int color3;
	int textEnum;
	int textEnum2;
	int textX;
	int textY;
	int textcolor;
	int textcolor2;
	int textcolor3;
	int width;
	int height;
	byte updated;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
} menuseparator_s;

typedef struct
{
	menucommon_s generic;
	int curvalue;
} menuradiobutton_s;

typedef struct
{
	menucommon_s	generic;
	char*			focuspic;
	int				shader;
	int				focusshader;
	int				focusX;
	int				focusY;
	int				focusWidth;
	int				focusHeight;
	int				width;
	int				height;
	int				color;
	int				color2;
	char			*textPtr;
	int				textEnum;
	int				textEnum2;
	int				textX;
	int				textY;
	int				textcolor;
	int				textcolor2;
	int				textStyle;
} menubitmap_s;

typedef struct
{
	menucommon_s generic;
	int				type;
	int				width;
	int				height;
	int				color;
	int				color2;
	int				textEnum;
	int				textX;
	int				textY;
	int				textcolor;
	int				textcolor2;
} menubutton_s;

typedef struct
{
	menucommon_s	generic;
	char*			string;
	int				normaltextEnum;
	int				buttontextEnum;
	int				normaltextEnum2;
	int				buttontextEnum2;
	int				normaltextEnum3;
	int				buttontextEnum3;
	int				style;
	int				color;
	int				color2;
	int				focusX;
	int				focusY;
	int				focusWidth;
	int				focusHeight;
} menutext_s;

extern void		Menu_Cache( void );
extern void		Menu_Focus( menucommon_s *m );
extern void		Menu_AddItem( menuframework_s *menu, void *item );
extern void		Menu_AdjustCursor( menuframework_s *menu, int dir );
extern void		Menu_Center( menuframework_s *menu );
extern void		Menu_Draw( menuframework_s *menu );
extern void		*Menu_ItemAtCursor( menuframework_s *m );
extern sfxHandle_t Menu_ActivateItem( menuframework_s *s, menucommon_s* item );
extern void		Menu_SetStatusBar( menuframework_s *s, const char *string );
extern void		Menu_SlideItem( menuframework_s *s, int dir );
extern void		Menu_SetCursor( menuframework_s *s, int cursor );
extern sfxHandle_t Menu_DefaultKey( menuframework_s *s, int key );
extern void		UI_PushMenu( menuframework_s *menu );
extern void		UI_PopMenu( void );
extern qboolean	UI_EFQmenu_IsActive( void );
extern void		UI_EFQmenu_ClearState( const char *reason );
extern void		UI_EFQmenu_Draw( int realtime );
extern void		UI_EFQmenu_KeyEvent( int key, qboolean down );
extern qboolean	UI_EFQmenu_ConsoleCommand( const char *cmd );
extern qboolean	UI_EFQmenu_RouteMenuName( const char *menuName );

//
// ui_field.c
//
extern void	Field_Clear( uifield_t *edit );
extern void	Field_CharEvent( uifield_t *edit, int ch );
extern void Field_Draw( uifield_t *edit, int x, int y, int width, int size,int color,int color2, qboolean showCursor );


//
// ui_menu.c
//
extern void UI_MainMenu(void);
extern void UI_InGameMenu(const char*holoFlag);
extern void AssetCache(void);
extern void UI_DataPadMenu(void);
extern qboolean UI_EFMainMenu_IsActive(void);
extern void UI_EFMainMenu_Cache(void);
extern void UI_EFMainMenu_InvalidateCache(void);
extern void UI_EFMainMenu_Open(void);
extern void UI_EFMainMenu_OpenNewGame(void);
extern void UI_EFMainMenu_OpenLoadGame(void);
extern void UI_EFMainMenu_OpenLoadGameFromPause(void);
extern void UI_EFMainMenu_OpenSaveGame(void);
extern void UI_EFMainMenu_OpenConfigure(void);
extern void UI_EFMainMenu_OpenConfigureFromPause(void);
extern void UI_EFMainMenu_OpenAudio(void);
extern void UI_EFMainMenu_OpenVideo(void);
extern void UI_EFMainMenu_OpenController(void);
extern void UI_EFMainMenu_OpenCrew(void);
extern void UI_EFMainMenu_StartSplitScreenBaseline(void);
extern void UI_EFMainMenu_StartHolomatchBaseline(void);
extern void UI_EFMainMenu_OpenStub(const char *title, const char *line);
extern void UI_EFMainMenu_Deactivate(void);
extern void UI_EFMainMenu_Draw(int realtime);
extern void UI_EFMainMenu_KeyEvent(int key, qboolean down);
extern qboolean UI_EFMainMenu_WantsControllerInput(void);
extern void UI_EFMainMenu_ControllerKeyEvent(int controller, int key, qboolean down);
extern int UI_EFSave_Count(void);
extern const char *UI_EFSave_Name(int index);
extern const char *UI_EFSave_Comment(int index);
extern const char *UI_EFSave_Map(int index);
extern const char *UI_EFSave_Date(int index);
extern qboolean UI_EFSave_IsCorrupt(int index);
extern qboolean UI_EFSave_Load(int index);
extern qboolean UI_EFSave_CreateNew(void);
extern qboolean UI_EFSave_Overwrite(int index);
extern qboolean UI_EFPauseMenu_IsActive(void);
extern void UI_EFPauseMenu_Cache(void);
extern int UI_EFPropTextWidth(const char *text, int style);
extern void UI_EFDrawPropText(int x, int y, const char *text, int style, int colorIndex);
extern void UI_EFPauseMenu_InvalidateCache(void);
extern void UI_EFPauseMenu_Open(const char *menuID);
extern void UI_EFPauseMenu_Deactivate(void);
extern void UI_EFPauseMenu_Draw(int realtime);
extern void UI_EFPauseMenu_KeyEvent(int key, qboolean down);

//
// ui_connect.c
//
extern void UI_DrawConnect( const char *servername, const char * updateInfoString );
extern void UI_UpdateConnectionString( char *string );
extern void UI_UpdateConnectionMessageString( char *string );


//
// ui_atoms.c
//

#define UI_FADEOUT	0
#define UI_FADEIN	1

typedef struct {
	int					frametime;
	int					realtime;
	int					cursorx;
	int					cursory;
	int					menusp;
	menuframework_s*	activemenu;
	menuframework_s*	stack[MAX_MENUDEPTH];
	
	glconfig_t			glconfig;
	qboolean			debugMode;
	qhandle_t			whiteShader;
	qhandle_t			menuBackShader;
	qhandle_t			cursor;
	float				scalex;
	float				scaley;
	//float				bias;
	qboolean			firstdraw;
} uiStatic_t;

extern void			UI_FillRect( float x, float y, float width, float height, const float *color );
extern void			UI_DrawString( int x, int y, const char* str, int style, vec4_t color );
extern void			UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader ); 
extern void			UI_UpdateScreen( void );
extern void			UI_ForceMenuOff( void );
extern int			UI_RegisterFont(const char *fontName);
extern void			UI_SetColor( const float *rgba );
extern char			*UI_Cvar_VariableString( const char *var_name );

extern uiStatic_t	uis;
extern uiimport_t	ui;


#define MAX_MOVIES 256
#define MAX_MODS 64

typedef struct {
	const char *modName;
	const char *modDescr;
} modInfo_t;

typedef struct {
	char		Name[64];
	int			SkinHeadCount;
//	qhandle_t	SkinHeadIcons[MAX_PLAYERMODELS];
	char		SkinHeadNames[MAX_PLAYERMODELS][16];
	int			SkinTorsoCount;
//	qhandle_t	SkinTorsoIcons[MAX_PLAYERMODELS];
	char		SkinTorsoNames[MAX_PLAYERMODELS][16];
	int			SkinLegCount;
//	qhandle_t	SkinLegIcons[MAX_PLAYERMODELS];
	char		SkinLegNames[MAX_PLAYERMODELS][16];
	char		ColorShader[MAX_PLAYERMODELS][64];
	int			ColorCount;
	char		ColorActionText[MAX_PLAYERMODELS][128];
} playerSpeciesInfo_t;

typedef struct {
	displayContextDef_t uiDC;

	int effectsColor;
	int currentCrosshair;

	modInfo_t modList[MAX_MODS];
	int modIndex;
	int modCount;

	int					playerSpeciesCount;
	playerSpeciesInfo_t	playerSpecies[MAX_PLAYERMODELS];
	int					playerSpeciesIndex;


	char		deferredScript [ MAX_DEFERRED_SCRIPT ];
	itemDef_t*	deferredScriptItem;

	itemDef_t*	runScriptItem;

	qboolean inGameLoad;
	// Used by Force Power allocation screen
	short	forcePowerUpdated;					// Enum of which power had the point allocated
	// Used by Weapon allocation screen
	short	selectedWeapon1;					// 1st weapon chosen
//	char 	selectedWeapon1ItemName[64];		// Item name of weapon chosen
//	int		selectedWeapon1AmmoIndex;			// Holds index to ammo
	short	selectedWeapon2;					// 2nd weapon chosen
//	char 	selectedWeapon2ItemName[64];		// Item name of weapon chosen
//	int		selectedWeapon2AmmoIndex;			// Holds index to ammo
	short	selectedThrowWeapon;				// throwable weapon chosen
//	char 	selectedThrowWeaponItemName[64];	// Item name of weapon chosen
//	int		selectedThrowWeaponAmmoIndex;		// Holds index to ammo

	itemDef_t *weapon1ItemButton;
	qhandle_t litWeapon1Icon;
	qhandle_t unlitWeapon1Icon;
	itemDef_t *weapon2ItemButton;
	qhandle_t litWeapon2Icon;
	qhandle_t unlitWeapon2Icon;

	itemDef_t *weaponThrowButton;
	qhandle_t litThrowableIcon;
	qhandle_t unlitThrowableIcon;
	short		movesTitleIndex;
	char		*movesBaseAnim;
	int			moveAnimTime;
//	int			languageCount;
	int			languageCountIndex;
}	uiInfo_t;

extern uiInfo_t uiInfo;

//
// ui_main.c
//
void _UI_Init( qboolean inGameLoad );
void _UI_DrawRect( float x, float y, float width, float height, float size, const float *color );
void _UI_MouseEvent( int dx, int dy );
void _UI_KeyEvent( int key, qboolean down );
void UI_Report(void);

extern char GoToMenu[];


//
// ui_syscalls.c
//
int				trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits, const char *psAudioFile /* = NULL */);
int				trap_CIN_StopCinematic(int handle); 
void			trap_Cvar_Set( const char *var_name, const char *value );
float			trap_Cvar_VariableValue( const char *var_name );
void			trap_GetGlconfig( glconfig_t *glconfig );
void			trap_Key_ClearStates( void );
int				trap_Key_GetCatcher( void );
qboolean		trap_Key_GetOverstrikeMode( void );
void			trap_Key_SetBinding( int keynum, const char *binding );
void			trap_Key_SetCatcher( int catcher );
void			trap_Key_SetOverstrikeMode( qboolean state );
void			trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void			trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
void			trap_R_SetColor( const float *rgba );
void			trap_R_ClearScene( void );
void			trap_R_AddRefEntityToScene( const refEntity_t *re );
void			trap_R_RenderScene( const refdef_t *fd );
void			trap_S_StopSounds( void );
sfxHandle_t		trap_S_RegisterSound( const char *sample, qboolean compressed );
void			trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
#ifdef _IMMERSION
ffHandle_t		trap_FF_Register( const char *name, int channel = FF_CHANNEL_MENU );
void			trap_FF_Start( ffHandle_t ff );
#endif // _IMMERSION
#ifndef _XBOX
int				PASSFLOAT( float x );
#endif



void _UI_Refresh( int realtime );

//from xboxcommon.h (from MP)
//
// Xbox Error Popup Constants
//
// The error popup system needs a context when it returns a value to know
// what to do. One of these is saved off when we create the popup, and then it's
// queried when we get a response so we know what we're doing.
enum xbErrorPopupType
{
	XB_POPUP_NONE,

	XB_POPUP_DELETE_CONFIRM,
	XB_POPUP_DISKFULL,				// Only at startup
	XB_POPUP_DISKFULL_DURING_SAVE,	// When trying to save a new game
	XB_POPUP_SAVE_COMPLETE,
	XB_POPUP_OVERWRITE_CONFIRM,
	XB_POPUP_LOAD_FAILED,
	XB_POPUP_LOAD_CHECKPOINT_FAILED,
	XB_POPUP_QUIT_CONFIRM,
	XB_POPUP_SAVING,
	XB_POPUP_LOAD_CONFIRM,
	XB_POPUP_TOO_MANY_SAVES,
	XB_POPUP_LOAD_CONFIRM_CHECKPOINT,
	XB_POPUP_CONFIRM_INVITE,
	XB_POPUP_CORRUPT_SETTINGS,
	XB_POPUP_DISKFULL_BOTH,
	XB_POPUP_YOU_ARE_DEAD,
	XB_POPUP_TESTING_SAVE,
	XB_POPUP_CORRUPT_SCREENSHOT,
	XB_POPUP_CONFIRM_NEW_1,
	XB_POPUP_CONFIRM_NEW_2,
	XB_POPUP_CONFIRM_NEW_3,
};
void UI_xboxErrorPopup(xbErrorPopupType popup);
void UI_xboxPopupResponse(void);
void UI_CheckForInvite( void );




#endif
