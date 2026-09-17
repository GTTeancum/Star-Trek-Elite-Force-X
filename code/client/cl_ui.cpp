// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"


#include "client.h"
#include "client_ui.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
#endif

#include "vmachine.h"

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
extern void UI_EFPauseMenu_Open( const char *menuID );
extern qboolean UI_EFPauseMenu_IsActive( void );
#endif

int PC_ReadTokenHandle(int handle, struct pc_token_s *pc_token);

int CL_UISystemCalls( int *args );

//prototypes
//extern qboolean SG_GetSaveImage( const char *psPathlessBaseName, void *pvAddress );
extern int SG_GetSaveGameComment(const char *psPathlessBaseName, char *sComment, char *sMapName);
extern qboolean SG_GameAllowedToSaveHere(qboolean inCamera);
extern void SG_StoreSaveGameComment(const char *sComment);
//extern byte *SCR_GetScreenshot(qboolean *qValid);

#if defined(STEFX_SP_HOSTED_MP)
static qboolean CL_HolomatchGameAllowedToSaveHere(qboolean inCamera)
{
	// Holomatch keeps the SP UI but has no SP game save export. The UI uses
	// qtrue only to determine whether its in-game menu may open.
	return inCamera ? qtrue : qfalse;
}
#endif


/*
====================
Helper functions for User Interface
====================
*/

/*
====================
GetClientState
====================
*/
static connstate_t GetClientState( void ) {
	return cls.state;
}

/*
====================
CL_GetGlConfig
====================
*/
static void UI_GetGlconfig( glconfig_t *config ) {
	*config = cls.glconfig;
}

/*
====================
GetClipboardData
====================
*/
static void GetClipboardData( char *buf, int buflen ) {
	char	*cbd;

	cbd = Sys_GetClipboardData();

	if ( !cbd ) {
		*buf = 0;
		return;
	}

	Q_strncpyz( buf, cbd, buflen );

	Z_Free( cbd );
}

/*
====================
Key_KeynumToStringBuf
====================
*/
// only ever called by binding-display code, therefore returns non-technical "friendly" names 
//	in any language that don't necessarily match those in the config file...
//
void Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) 
{
	const char *psKeyName = Key_KeynumToString( keynum/*, qtrue */);

	// see if there's a more friendly (or localised) name...
	//
	const char *psKeyNameFriendly = SE_GetString( va("KEYNAMES_KEYNAME_%s",psKeyName) );

	Q_strncpyz( buf, (psKeyNameFriendly && psKeyNameFriendly[0]) ? psKeyNameFriendly : psKeyName, buflen );
}

/*
====================
Key_GetBindingBuf
====================
*/
void Key_GetBindingBuf( int keynum, char *buf, int buflen ) {
	char	*value;

	value = Key_GetBinding( keynum );
	if ( value ) {
		Q_strncpyz( buf, value, buflen );
	}
	else {
		*buf = 0;
	}
}

/*
====================
Key_GetCatcher
====================
*/
int Key_GetCatcher( void ) 
{
	return cls.keyCatchers;
}

/*
====================
Key_GetCatcher
====================
*/
void Key_SetCatcher( int catcher ) 
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_stefxKeyCatcherLogBudget = 64;
	if (cls.keyCatchers != catcher && s_stefxKeyCatcherLogBudget > 0)
	{
		XBLog_Write(va("STEFX: Key_SetCatcher change old=0x%x new=0x%x overlay='%s' paused='%s'",
			(unsigned int)cls.keyCatchers,
			(unsigned int)catcher,
			Cvar_VariableString("stefx_objectivesOverlay"),
			Cvar_VariableString("cl_paused")));
		--s_stefxKeyCatcherLogBudget;
	}
#endif
	cls.keyCatchers = catcher;
}

/*
====================
FloatAsInt
====================
*/
int FloatAsInt( float f ) 
{
	int		temp;

	*(float *)&temp = f;

	return temp;
}

static void UI_Cvar_Create( const char *var_name, const char *var_value, int flags ) {
	Cvar_Get( var_name, var_value, flags );
}

static int GetConfigString(int index, char *buf, int size)
{
	int		offset;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		return qfalse;

	offset = cl.gameState.stringOffsets[index];
	if (!offset)
		return qfalse;

	Q_strncpyz( buf, cl.gameState.stringData+offset, size);
 
	return qtrue;
}

/*
====================
CL_ShutdownUI
====================
*/
void CL_ShutdownUI( void ) {
	cls.keyCatchers &= ~KEYCATCH_UI;
	cls.uiStarted = qfalse;
}

void CL_DrawDatapad(int HUDType)
{
	switch(HUDType)
	{
	case DP_OBJECTIVES:
		VM_Call( CG_DRAW_DATAPAD_OBJECTIVES );
		break;
	case DP_WEAPONS:
		VM_Call( CG_DRAW_DATAPAD_WEAPONS );
		break;
	case DP_INVENTORY:
		VM_Call( CG_DRAW_DATAPAD_INVENTORY );
		break;
	case DP_FORCEPOWERS:
		VM_Call( CG_DRAW_DATAPAD_FORCEPOWERS );
		break;
	default:
		break;
	}


}

void UI_Init( int apiVersion, uiimport_t *uiimport, qboolean inGameLoad );

/*
====================
CL_InitUI
====================
*/
void CL_InitUI( void ) {
	uiimport_t	uii;

#ifdef _XBOX
	XBLF("JA: CL_InitUI entered state=%d renderer=%d cgame=%d ui=%d",
		(int)cls.state,
		(int)cls.rendererStarted,
		(int)cls.cgameStarted,
		(int)cls.uiStarted);
#endif

	memset( &uii, 0, sizeof( uii ) );

	uii.Printf = Com_Printf;
	uii.Error = Com_Error;
#ifdef _XBOX
	XBLog_Write("JA: CL_InitUI: import table zeroed; assigning callbacks...");
#endif

	uii.Cvar_Set				= Cvar_Set;
	uii.Cvar_VariableValue		= Cvar_VariableValue;
	uii.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	uii.Cvar_SetValue			= Cvar_SetValue;
	uii.Cvar_Reset				= Cvar_Reset;
	uii.Cvar_Create				= UI_Cvar_Create;
	uii.Cvar_InfoStringBuffer	= Cvar_InfoStringBuffer;

	uii.Draw_DataPad			= CL_DrawDatapad;

	uii.Argc					= Cmd_Argc;
	uii.Argv					= Cmd_ArgvBuffer;
	uii.Cmd_TokenizeString		= Cmd_TokenizeString;

	uii.Cmd_ExecuteText			= Cbuf_ExecuteText;

	uii.FS_FOpenFile			= FS_FOpenFileByMode;
	uii.FS_Read					= FS_Read;
	uii.FS_Write				= FS_Write;
	uii.FS_FCloseFile			= FS_FCloseFile;
	uii.FS_GetFileList			= FS_GetFileList;
	uii.FS_ReadFile				= FS_ReadFile;
	uii.FS_FreeFile				= FS_FreeFile;

	uii.R_RegisterModel			= re.RegisterModel;
	uii.R_RegisterSkin			= re.RegisterSkin;
	uii.R_RegisterShader		= re.RegisterShader;
	uii.R_RegisterShaderNoMip	= re.RegisterShaderNoMip;
	uii.R_RegisterFont			= re.RegisterFont;
#ifndef _XBOX
	uii.R_Font_StrLenPixels		= re.Font_StrLenPixels;
	uii.R_Font_HeightPixels		= re.Font_HeightPixels;
	uii.R_Font_DrawString		= re.Font_DrawString;
#endif
	uii.R_Font_StrLenChars		= re.Font_StrLenChars;
	uii.Language_IsAsian		= re.Language_IsAsian;
	uii.Language_UsesSpaces		= re.Language_UsesSpaces;
	uii.AnyLanguage_ReadCharFromString = re.AnyLanguage_ReadCharFromString;

	//uii.SG_GetSaveImage			= SG_GetSaveImage;
	uii.SG_GetSaveGameComment	= SG_GetSaveGameComment;
	uii.SG_StoreSaveGameComment = SG_StoreSaveGameComment;
#if defined(STEFX_SP_HOSTED_MP)
	uii.SG_GameAllowedToSaveHere= CL_HolomatchGameAllowedToSaveHere;
#else
	uii.SG_GameAllowedToSaveHere= SG_GameAllowedToSaveHere;
#endif

	//uii.SCR_GetScreenshot		= SCR_GetScreenshot;

	//uii.DrawStretchRaw			= re.DrawStretchRaw;
	uii.R_ClearScene			= re.ClearScene;
	uii.R_AddRefEntityToScene	= re.AddRefEntityToScene;
	uii.R_AddPolyToScene		=  re.AddPolyToScene;
	uii.R_AddLightToScene		= re.AddLightToScene;
	uii.R_RenderScene			= re.RenderScene;

	uii.R_ModelBounds			= re.ModelBounds;

	uii.R_SetColor				= re.SetColor;
	uii.R_DrawStretchPic		= re.DrawStretchPic;
	uii.UpdateScreen			= SCR_UpdateScreen;

#ifdef _XBOX
	uii.PrecacheScreenshot		= SCR_PrecacheScreenshot;
#endif

	uii.R_LerpTag				= re.LerpTag;

	uii.S_StartLocalLoopingSound= S_StartLocalLoopingSound;
	uii.S_StartLocalSound		= S_StartLocalSound;
	uii.S_RegisterSound			= S_RegisterSound;

	uii.Key_KeynumToStringBuf	= Key_KeynumToStringBuf;
	uii.Key_GetBindingBuf		= Key_GetBindingBuf;
	uii.Key_SetBinding			= Key_SetBinding;
	uii.Key_IsDown				= Key_IsDown;
	uii.Key_GetOverstrikeMode	= Key_GetOverstrikeMode;
	uii.Key_SetOverstrikeMode	= Key_SetOverstrikeMode;
	uii.Key_ClearStates			= Key_ClearStates;
	uii.Key_GetCatcher			= Key_GetCatcher;
	uii.Key_SetCatcher			= Key_SetCatcher;

	uii.GetClipboardData		= GetClipboardData;

	uii.GetClientState			= GetClientState;

	uii.GetGlconfig				= UI_GetGlconfig;

	uii.GetConfigString			= (void (*)(int, char *, int))GetConfigString;

	uii.Milliseconds			= Sys_Milliseconds;

#ifdef _XBOX
	XBLF("JA: CL_InitUI: callbacks ready Printf=%p Error=%p RegisterShaderNoMip=%p DrawStretchPic=%p UpdateScreen=%p",
		(void*)uii.Printf,
		(void*)uii.Error,
		(void*)uii.R_RegisterShaderNoMip,
		(void*)uii.R_DrawStretchPic,
		(void*)uii.UpdateScreen);
	XBLF("JA: CL_InitUI: calling UI_Init inGameLoad=%d", (int)(cls.state > CA_DISCONNECTED && cls.state <= CA_ACTIVE));
#endif
	UI_Init(UI_API_VERSION, &uii, (cls.state > CA_DISCONNECTED && cls.state <= CA_ACTIVE));
#ifdef _XBOX
	XBLog_Write("JA: CL_InitUI: UI_Init returned");
#endif

//JLF MPSKIPPED
#ifdef _XBOX
	extern void UpdateDemoTimer();
	XBLog_Write("JA: CL_InitUI: UpdateDemoTimer...");
	UpdateDemoTimer();
	XBLog_Write("JA: CL_InitUI done");

#endif

//	uie->UI_Init( UI_API_VERSION, &uii );

}


qboolean UI_GameCommand( void ) {
	if (!cls.uiStarted)
	{
		return qfalse;
	}
	return UI_ConsoleCommand();
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean s_stefxObjectivesOverlayOwnsCatcher = qfalse;
static qboolean s_stefxObjectivesOverlayActive = qfalse;
static qboolean s_stefxPauseMenuRequested = qfalse;

static qboolean CL_STEFX_EnsureUIStarted( const char *source )
{
	XBLF("STEFX: %s ensure UI enter uiStarted=%d state=%d renderer=%d cgame=%d catcher=0x%x",
		source ? source : "UI",
		(int)cls.uiStarted,
		(int)cls.state,
		(int)cls.rendererStarted,
		(int)cls.cgameStarted,
		(unsigned int)cls.keyCatchers);

	if ( cls.uiStarted )
	{
		XBLF("STEFX: %s ensure UI already started catcher=0x%x",
			source ? source : "UI",
			(unsigned int)cls.keyCatchers);
		return qtrue;
	}

	if ( cls.state < CA_CONNECTED || !cls.rendererStarted )
	{
		XBLF("STEFX: %s cannot start UI state=%d renderer=%d cgame=%d",
			source ? source : "UI",
			(int)cls.state,
			(int)cls.rendererStarted,
			(int)cls.cgameStarted);
		return qfalse;
	}

	XBLF("STEFX: %s starting Xbox UI on demand state=%d renderer=%d cgame=%d",
		source ? source : "UI",
		(int)cls.state,
		(int)cls.rendererStarted,
		(int)cls.cgameStarted);
	cls.uiStarted = qtrue;
	CL_InitUI();
	XBLF("STEFX: %s UI start complete uiStarted=%d catcher=0x%x",
		source ? source : "UI",
		(int)cls.uiStarted,
		(unsigned int)cls.keyCatchers);
	return cls.uiStarted ? qtrue : qfalse;
}

void CL_STEFX_SetObjectivesOverlay( qboolean active, const char *source )
{
	const char *tag = source ? source : "objectives-overlay";

	XBLF("STEFX: objectives overlay request source='%s' active=%d uiStarted=%d state=%d catcher=0x%x overlay='%s' paused='%s'",
		tag,
		active ? 1 : 0,
		(int)cls.uiStarted,
		(int)cls.state,
		(unsigned int)cls.keyCatchers,
		Cvar_VariableString( "stefx_objectivesOverlay" ),
		Cvar_VariableString( "cl_paused" ));

	if ( active )
	{
		if ( !CL_STEFX_EnsureUIStarted( tag ) )
		{
			XBLF("STEFX: objectives overlay blocked source='%s' state=%d renderer=%d cgame=%d catcher=0x%x",
				tag,
				(int)cls.state,
				(int)cls.rendererStarted,
				(int)cls.cgameStarted,
				(unsigned int)cls.keyCatchers);
			return;
		}

		s_stefxObjectivesOverlayActive = qtrue;
		Cvar_Set( "stefx_objectivesOverlay", "1" );
		XBLF("STEFX: objectives overlay cvar overlay='%s'",
			Cvar_VariableString( "stefx_objectivesOverlay" ));
		Cvar_Set( "cl_paused", "1" );
		XBLF("STEFX: objectives overlay cvar paused='%s'",
			Cvar_VariableString( "cl_paused" ));
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
		XBLF("STEFX: objectives overlay keycatcher set catcher=0x%x",
			(unsigned int)cls.keyCatchers);
		s_stefxObjectivesOverlayOwnsCatcher = qtrue;
		XBLF("STEFX: objectives overlay open source='%s' catcher=0x%x overlay='%s' paused='%s'",
			tag,
			(unsigned int)cls.keyCatchers,
			Cvar_VariableString( "stefx_objectivesOverlay" ),
			Cvar_VariableString( "cl_paused" ));
		return;
	}

	s_stefxObjectivesOverlayActive = qfalse;
	Cvar_Set( "stefx_objectivesOverlay", "0" );
	Cvar_Set( "cl_paused", "0" );
	if ( s_stefxObjectivesOverlayOwnsCatcher )
	{
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		s_stefxObjectivesOverlayOwnsCatcher = qfalse;
	}
	XBLF("STEFX: objectives overlay close source='%s' catcher=0x%x overlay='%s' paused='%s'",
		tag,
		(unsigned int)cls.keyCatchers,
		Cvar_VariableString( "stefx_objectivesOverlay" ),
		Cvar_VariableString( "cl_paused" ));
}

qboolean CL_STEFX_ObjectivesOverlayActive( void )
{
	return s_stefxObjectivesOverlayActive;
}

qboolean CL_STEFX_MissionFailedOverlayActive( void )
{
	return (qboolean)( ( cls.keyCatchers & KEYCATCH_UI ) != 0
		&& Cvar_VariableIntegerValue( "stefx_missionFailedOverlay" ) != 0 );
}

static const char *CL_STEFX_MissionFailedReasonText( const char *text )
{
	if ( !text || !text[0] )
	{
		return "MISSION OBJECTIVE FAILED";
	}

	if ( text[0] != '@' )
	{
		return text;
	}

	if ( strstr( text, "PLAYER" ) )
	{
		return "YOU HAVE BEEN INCAPACITATED";
	}
	if ( strstr( text, "TOOMANYALLIESDIED" ) )
	{
		return "TOO MANY TEAMMATES HAVE FALLEN";
	}
	if ( strstr( text, "TURNED" ) )
	{
		return "YOU TURNED ON YOUR TEAMMATES";
	}
	if ( strstr( text, "UNKNOWN" ) )
	{
		return "MISSION OBJECTIVE FAILED";
	}

	return "A MISSION-CRITICAL ALLY WAS LOST";
}

static void CL_STEFX_DrawCenteredBigString( int y, const char *text, vec4_t color )
{
	int x;

	if ( !text || !text[0] )
	{
		return;
	}

	x = 320 - ( Q_PrintStrlen( text ) * 16 ) / 2;
	SCR_DrawBigStringColor( x, y, text, color );
}

void CL_STEFX_DrawMissionFailedOverlay( void )
{
	static qboolean s_stefxMissionFailedDrawLogged = qfalse;
	const char *reason;
	vec4_t darkBlue = { 0.015f, 0.015f, 0.229f, 1.0f };
	vec4_t black = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec4_t red = { 1.0f, 0.0f, 0.0f, 1.0f };
	vec4_t gold = { 1.0f, 0.682f, 0.0f, 1.0f };

	if ( !CL_STEFX_MissionFailedOverlayActive() )
	{
		s_stefxMissionFailedDrawLogged = qfalse;
		return;
	}

	if ( !s_stefxMissionFailedDrawLogged )
	{
		XBLF( "STEFX: client drawing full-screen missionfailed overlay catcher=0x%x paused='%s' text='%s'",
			(unsigned int)cls.keyCatchers,
			Cvar_VariableString( "cl_paused" ),
			Cvar_VariableString( "ui_missionfailed_text" ) );
		s_stefxMissionFailedDrawLogged = qtrue;
	}

	re.SetColor( NULL );
	SCR_FillRect( 0.0f, 0.0f, 640.0f, 480.0f, black );
	SCR_FillRect( 50.0f, 10.0f, 540.0f, 80.0f, darkBlue );
	SCR_FillRect( 140.0f, 352.0f, 360.0f, 108.0f, darkBlue );

	re.SetColor( gold );
	re.DrawStretchPic( 128.0f, 348.0f, 384.0f, 3.0f, 0, 0, 0, 0, cls.whiteShader );
	re.DrawStretchPic( 128.0f, 462.0f, 384.0f, 3.0f, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	reason = CL_STEFX_MissionFailedReasonText( Cvar_VariableString( "ui_missionfailed_text" ) );
	CL_STEFX_DrawCenteredBigString( 32, "MISSION FAILED", red );
	CL_STEFX_DrawCenteredBigString( 64, reason, red );
	CL_STEFX_DrawCenteredBigString( 382, "LOAD AUTOSAVE", gold );
	CL_STEFX_DrawCenteredBigString( 414, "LOAD SAVED GAME", gold );
}

void CL_STEFX_DrawObjectivesOverlay( void )
{
	static qboolean s_stefxObjectivesOverlayDrawLogged = qfalse;

	if ( !s_stefxObjectivesOverlayActive )
	{
		return;
	}

	if ( !s_stefxObjectivesOverlayDrawLogged )
	{
		XBLog_Write( "STEFX: client drawing full-screen objectives overlay" );
		s_stefxObjectivesOverlayDrawLogged = qtrue;
	}

	CL_DrawDatapad( DP_OBJECTIVES );
}

void CL_STEFX_RequestPauseMenu( const char *source )
{
	s_stefxPauseMenuRequested = qtrue;
	XBLF( "STEFX: pause menu request queued source='%s' state=%d catcher=0x%x",
		source ? source : "unknown",
		(int)cls.state,
		(unsigned int)cls.keyCatchers );
}

void CL_STEFX_ServiceMenuRequests( void )
{
	static int s_serviceTraceBudget = 16;

	if ( s_serviceTraceBudget > 0 && cls.state == CA_ACTIVE )
	{
		XBLF( "STEFX: pause menu service tick pending=%d state=%d cgameStarted=%d catcher=0x%x serverTime=%d",
			s_stefxPauseMenuRequested ? 1 : 0,
			(int)cls.state,
			(int)cls.cgameStarted,
			(unsigned int)cls.keyCatchers,
			cl.serverTime );
		--s_serviceTraceBudget;
	}

	if ( !s_stefxPauseMenuRequested )
	{
		return;
	}

	s_stefxPauseMenuRequested = qfalse;
	XBLF( "STEFX: pause menu request service state=%d uiStarted=%d cgameStarted=%d catcher=0x%x",
		(int)cls.state,
		(int)cls.uiStarted,
		(int)cls.cgameStarted,
		(unsigned int)cls.keyCatchers );

	if ( cls.state != CA_ACTIVE || !cls.cgameStarted )
	{
		XBLog_Write( "STEFX: pause menu request dropped because game is not active" );
		return;
	}

	if ( CL_STEFX_ObjectivesOverlayActive() )
	{
		CL_STEFX_SetObjectivesOverlay( qfalse, "pause-menu-service-close-objectives" );
	}

	if ( !CL_STEFX_EnsureUIStarted( "pause-menu-service" ) )
	{
		XBLog_Write( "STEFX: pause menu request dropped because UI could not start" );
		return;
	}

	XBLF( "STEFX: pause menu direct open before active=%d catcher=0x%x paused='%s'",
		UI_EFPauseMenu_IsActive() ? 1 : 0,
		(unsigned int)cls.keyCatchers,
		Cvar_VariableString( "cl_paused" ) );
	UI_EFPauseMenu_Open( NULL );
	XBLF( "STEFX: pause menu request serviced direct active=%d catcher=0x%x paused='%s'",
		UI_EFPauseMenu_IsActive() ? 1 : 0,
		(unsigned int)cls.keyCatchers,
		Cvar_VariableString( "cl_paused" ) );
}

void CL_STEFX_ObjectivesOverlay_f( void )
{
	qboolean active = qtrue;

	if ( Cmd_Argc() > 1 )
	{
		active = atoi( Cmd_Argv( 1 ) ) ? qtrue : qfalse;
	}

	CL_STEFX_SetObjectivesOverlay( active, "ef_objectives_overlay" );
}

void CL_STEFX_MissionFailedOverlay_f( void )
{
	XBLF("STEFX: missionfailed overlay command uiStarted=%d cgameStarted=%d state=%d catcher=0x%x",
		cls.uiStarted ? 1 : 0,
		cls.cgameStarted ? 1 : 0,
		(int)cls.state,
		(unsigned int)cls.keyCatchers);
	if ( !CL_STEFX_EnsureUIStarted( "ef_missionfailed_overlay" ) )
	{
		XBLog_Write( "STEFX: missionfailed overlay command blocked because UI could not start" );
		return;
	}

	Cvar_Set( "ui_missionfailed_text", "@SP_INGAME_MISSIONFAILED_PLAYER" );
	UI_SetActiveMenu( "missionfailed_menu", NULL );
	XBLF("STEFX: missionfailed overlay command done catcher=0x%x paused='%s' missionfailed='%s'",
		(unsigned int)cls.keyCatchers,
		Cvar_VariableString( "cl_paused" ),
		Cvar_VariableString( "ui_missionfailed" ));
}
#endif

void CL_GenericMenu_f(void)
{		
	char *arg = Cmd_Argv( 1 );
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	if (!Q_stricmp(Cmd_Argv(0), "endholodeckmenu"))
		arg = "endholomenu";
#endif
	if ( !arg || !arg[0] )
	{
		arg = NULL;
	}

#ifdef _XBOX
	XBLF("STEFX: CL_GenericMenu_f uiStarted=%d cgameStarted=%d state=%d catcher=0x%x arg='%s'",
		cls.uiStarted ? 1 : 0,
		cls.cgameStarted ? 1 : 0,
		(int)cls.state,
		(unsigned int)cls.keyCatchers,
		arg ? arg : "");
#endif
	if (
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		CL_STEFX_EnsureUIStarted( "CL_GenericMenu_f" )
#else
		cls.uiStarted
#endif
	) {
		UI_SetActiveMenu("ingame",arg);
	}
#ifdef _XBOX
	else {
		XBLog_Write("STEFX: CL_GenericMenu_f blocked because UI is not started");
	}
#endif
}


void CL_EndScreenDissolve_f(void)
{
	re.InitDissolve(qtrue);	// dissolve from cinematic to underlying ingame
}

void CL_DataPad_f(void)
{		
#ifdef _XBOX
	XBLF("STEFX: CL_DataPad_f uiStarted=%d cgameStarted=%d state=%d catcher=0x%x",
		cls.uiStarted ? 1 : 0,
		cls.cgameStarted ? 1 : 0,
		(int)cls.state,
		(unsigned int)cls.keyCatchers);
#endif
	if (
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		CL_STEFX_EnsureUIStarted( "CL_DataPad_f" ) &&
#else
		cls.uiStarted &&
#endif
		cls.cgameStarted && (cls.state == CA_ACTIVE) ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		CL_STEFX_SetObjectivesOverlay( qtrue, "CL_DataPad_f" );
#else
		UI_SetActiveMenu("datapad",NULL);
#endif
	}
#ifdef _XBOX
	else {
		XBLog_Write("STEFX: CL_DataPad_f blocked by state gate");
	}
#endif
}

/*
====================
CL_GetGlConfig
====================
*/
static void CL_GetGlconfig( glconfig_t *config ) 
{
	*config = cls.glconfig;
}
/*
int PC_ReadTokenHandle(int handle, pc_token_t *pc_token);
int PC_SourceFileAndLine(int handle, char *filename, int *line);
*/
/*
====================
CL_UISystemCalls

The ui module is making a system call
====================
*/
vm_t	uivm;

#define	VMA(x) ((void*)args[x])
#define	VMF(x)	((float *)args)[x]
int CL_UISystemCalls( int *args ) 
{

	switch( args[0] ) 
	{
	case UI_ERROR:
		Com_Error( ERR_DROP, "%s", VMA(1) );
		return 0;

	case UI_CVAR_REGISTER:
		Cvar_Register( (vmCvar_t *)VMA(1),(const char *) VMA(2),(const char *) VMA(3), args[4] ); 
		return 0;

	case UI_CVAR_SET:
		Cvar_Set( (const char *) VMA(1), (const char *) VMA(2) );
		return 0;

	case UI_CVAR_SETVALUE:
		Cvar_SetValue( (const char *) VMA(1), VMF(2) );
		return 0;

	case UI_CVAR_UPDATE:
		Cvar_Update( (vmCvar_t *) VMA(1) );
		return 0;

	case UI_R_REGISTERMODEL:
		return re.RegisterModel((const char *) VMA(1) );

	case UI_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip((const char *) VMA(1) );

	case UI_GETGLCONFIG:
		CL_GetGlconfig( ( glconfig_t *) VMA(1) );
		return 0;

	case UI_CMD_EXECUTETEXT:
		Cbuf_ExecuteText( args[1], (const char *) VMA(2) );
		return 0;

	case UI_CVAR_VARIABLEVALUE:
		return FloatAsInt( Cvar_VariableValue( (const char *) VMA(1) ) );

	case UI_FS_GETFILELIST:
		return FS_GetFileList( (const char *) VMA(1), (const char *) VMA(2), (char *) VMA(3), args[4] );

	case UI_KEY_SETCATCHER:
		Key_SetCatcher( args[1] );
		return 0;

	case UI_KEY_CLEARSTATES:
		Key_ClearStates();
		return 0;

	case UI_R_SETCOLOR:
		re.SetColor( (const float *) VMA(1) );
		return 0;

	case UI_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;

	case UI_CVAR_VARIABLESTRINGBUFFER:
		Cvar_VariableStringBuffer( (const char *) VMA(1), (char *) VMA(2), args[3] );
		return 0;

  case UI_R_MODELBOUNDS:
		re.ModelBounds( args[1], (float *) VMA(2),(float *) VMA(3) );
		return 0;

	case UI_R_CLEARSCENE:
		re.ClearScene();
		return 0;

//	case UI_KEY_GETOVERSTRIKEMODE:
//		return Key_GetOverstrikeMode();
//		return 0;

//	case UI_PC_READ_TOKEN:
//		return PC_ReadTokenHandle( args[1], VMA(2) );
		
//	case UI_PC_SOURCE_FILE_AND_LINE:
//		return PC_SourceFileAndLine( args[1], VMA(2), VMA(3) );

	case UI_KEY_GETCATCHER:
		return Key_GetCatcher();

	case UI_MILLISECONDS:
		return Sys_Milliseconds();

	case UI_S_REGISTERSOUND:
		return S_RegisterSound((const char *) VMA(1));

	case UI_S_STARTLOCALSOUND:
		S_StartLocalSound( args[1], args[2] );
		return 0;

//	case UI_R_REGISTERFONT:
//		re.RegisterFont( VMA(1), args[2], VMA(3));
//		return 0;

	case UI_CIN_PLAYCINEMATIC:
	  Com_DPrintf("UI_CIN_PlayCinematic\n");
	  return CIN_PlayCinematic((const char *)VMA(1), args[2], args[3], args[4], args[5], args[6], (const char *)VMA(7));

	case UI_CIN_STOPCINEMATIC:
	  return CIN_StopCinematic(args[1]);

	case UI_CIN_RUNCINEMATIC:
	  return CIN_RunCinematic(args[1]);

#ifndef _XBOX
	case UI_CIN_DRAWCINEMATIC:
	  CIN_DrawCinematic(args[1]);
	  return 0;
#endif

	case UI_KEY_SETBINDING:
		Key_SetBinding( args[1], (const char *) VMA(2) );
		return 0;

	case UI_KEY_KEYNUMTOSTRINGBUF:
		Key_KeynumToStringBuf( args[1],(char *) VMA(2), args[3] );
		return 0;

	case UI_CIN_SETEXTENTS:
	  CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
	  return 0;

	case UI_KEY_GETBINDINGBUF:
		Key_GetBindingBuf( args[1], (char *) VMA(2), args[3] );
		return 0;


	default:
		Com_Error( ERR_DROP, "Bad UI system trap: %i", args[0] );

	}

	return 0;
}

