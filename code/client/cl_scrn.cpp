// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"


#include "client.h"
#include "client_ui.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
#include "../win32/xb_perf.h"
extern "C" volatile unsigned int g_SPXBPerfScreenDrawMsec;
extern "C" volatile unsigned int g_SPXBPerfCgameDrawMsec;
extern "C" volatile unsigned int g_SPXBPerfEndFrameMsec;
extern "C" volatile unsigned int g_SPXBPerfFrontendMsec;
extern "C" volatile unsigned int g_SPXBPerfBackendMsec;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBPerfFrameMsec;
extern "C" volatile unsigned int g_SPXBPerfServerMsec;
extern "C" volatile unsigned int g_SPXBPerfClientMsec;
extern "C" volatile unsigned int g_SPXBPerfGameMsec;
extern "C" volatile unsigned int g_SPXBPerfAudioMsec;
extern "C" volatile unsigned int g_SPXBPerfFinishMsec;
extern "C" volatile unsigned int g_SPXBPerfPresentMsec;
extern "C" volatile unsigned int g_SPXBPerfBackendDrawSurfsMsec;
extern "C" volatile unsigned int g_SPXBPerfBackendSwapMsec;
extern "C" volatile unsigned int g_SPXBPerfLoopMsec;
extern "C" volatile unsigned int g_SPXBPerfInputMsec;
extern "C" volatile unsigned int g_SPXBPerfMenuMsec;
extern "C" volatile unsigned int g_SPXBPerfComEventMsec;
extern "C" volatile unsigned int g_SPXBPerfComCommandMsec;
extern "C" volatile unsigned int g_SPXBPerfClientPreambleMsec;
extern "C" volatile unsigned int g_SPXBPerfClientTailMsec;
#endif
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBClTailStage;

// Runtime proof is captured in the flushed hardware log files.
#define STEFX_TEMP_HARDWARE_FPS_COUNTER 0
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
void SCR_DrawVirtualStretchPic(float x, float y, float w, float h,
    float s1, float t1, float s2, float t2, qhandle_t shader)
{
	// RB_SetGL2D uses framebuffer pixels over gameplay and virtual units in fullscreen menus.
	if (cls.state == CA_ACTIVE && cls.glconfig.vidWidth != SCREEN_WIDTH && !_UI_IsFullscreen()) {
		const float sx = (float)cls.glconfig.vidWidth / SCREEN_WIDTH;
		const float sy = (float)cls.glconfig.vidHeight / SCREEN_HEIGHT;
		x *= sx; w *= sx;
		y *= sy; h *= sy;
	}
	re.DrawStretchPic(x, y, w, h, s1, t1, s2, t2, shader);
}
#else
#define SCR_DrawVirtualStretchPic re.DrawStretchPic
#endif

extern console_t con;

qboolean	scr_initialized;		// ready to draw

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_DrawVirtualStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
** SCR_DrawBigChar
** big chars are drawn at 640*480 virtual screen size
*/
void SCR_DrawBigChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -BIGCHAR_HEIGHT ) {
		return;
	}

	ax = x;
	ay = y;
	aw = BIGCHAR_WIDTH;
	ah = BIGCHAR_HEIGHT;

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;
/*
	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
*/
	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	SCR_DrawVirtualStretchPic( ax, ay, aw, ah,
					   fcol, frow, 
					   fcol + size, frow + size2, 
					   cls.charSetShader );

}

/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -SMALLCHAR_HEIGHT ) {
		return;
	}

	row = ch>>4;
	col = ch&15;
/*
	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	re.DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT,
					   fcol, frow, 
					   fcol + size, frow + size, 
					   cls.charSetShader );
*/

	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	re.DrawStretchPic( x * con.xadjust, y * con.yadjust, 
						SMALLCHAR_WIDTH * con.xadjust, SMALLCHAR_HEIGHT * con.yadjust, 
		fcol, frow, 
		fcol + size, frow + size2, 
		cls.charSetShader );

}



/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawBigStringExt( int x, int y, const char *string, float *setColor, qboolean forceColor ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawBigChar( xx+2, y+2, *s );
		xx+=16;
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			s += 2;
			continue;
		}
		SCR_DrawBigChar( xx, y, *s );
		xx+=16;
		s++;
	}
	re.SetColor( NULL );
}


void SCR_DrawBigString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawBigStringExt( x, y, s, color, qfalse );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	SCR_DrawBigStringExt( x, y, s, color, qtrue );
}

#if defined(_XBOX) && STEFX_TEMP_HARDWARE_FPS_COUNTER
static void SCR_DrawHardwareTextLine( int x, int y, const char *text, const vec4_t shadow, const vec4_t color )
{
	int i;

	re.SetColor(shadow);
	for (i = 0; text[i]; ++i)
	{
		SCR_DrawSmallChar(x + 1 + i * SMALLCHAR_WIDTH, y + 1, text[i]);
	}

	re.SetColor(color);
	for (i = 0; text[i]; ++i)
	{
		SCR_DrawSmallChar(x + i * SMALLCHAR_WIDTH, y, text[i]);
	}
}

static void SCR_DrawHardwareFPS( void )
{
	static int sampleStart;
	static int sampleFrames;
	static unsigned int fpsTenths;
	static char text[16] = "FPS --.-";
	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	static int detailNextCapture;
	static char frameText[64];
	static char waitText[64];
	static char loopText[64];
	static char gameText[64];
	static char submitText[64];
	static char frontendText0[64];
	static char frontendText1[64];
	static char drawPhaseText[64];
	static char pushWaitText[64];
	static char ownerGapText[64];
	static char phaseText[64];
	qboolean detailVisible;
	unsigned int comOtherMsec;
	unsigned int frontendGapMsec;
	unsigned int backendGapMsec;
	unsigned int backendOwnedMsec;
	#endif
	const int now = Sys_Milliseconds();
	int elapsed;
	int boxWidth = 8 + (int)strlen(text) * SMALLCHAR_WIDTH;
	int boxHeight = SMALLCHAR_HEIGHT + 6;
	vec4_t background = { 0.0f, 0.0f, 0.0f, 0.70f };
	vec4_t shadow = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };

	if (!sampleStart)
	{
		sampleStart = now;
	}

	++sampleFrames;
	elapsed = now - sampleStart;
	if (elapsed >= 1000)
	{
		fpsTenths = (unsigned int)(((unsigned __int64)sampleFrames * 10000) / elapsed);
		Com_sprintf(text, sizeof(text), "FPS %u.%u", fpsTenths / 10, fpsTenths % 10);
		sampleFrames = 0;
		sampleStart = now;
	}

	if (fpsTenths)
	{
		if (fpsTenths < 200)
		{
			color[1] = 0.25f;
			color[2] = 0.25f;
		}
		else if (fpsTenths < 300)
		{
			color[2] = 0.20f;
		}
		else
		{
			color[0] = 0.35f;
			color[2] = 0.35f;
		}
	}

	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (!detailNextCapture)
	{
		detailNextCapture = now + 2000;
	}
	detailVisible = qtrue;
	if ((now - detailNextCapture) >= 0)
	{
		comOtherMsec = 0;
		if (g_SPXBPerfFrameMsec > g_SPXBPerfServerMsec + g_SPXBPerfClientMsec)
		{
			comOtherMsec = g_SPXBPerfFrameMsec -
				g_SPXBPerfServerMsec - g_SPXBPerfClientMsec;
		}
		Com_sprintf(loopText, sizeof(loopText), "LP %u IN %u MM %u",
			(unsigned int)g_SPXBPerfLoopMsec,
			(unsigned int)g_SPXBPerfInputMsec,
			(unsigned int)g_SPXBPerfMenuMsec);
		Com_sprintf(frameText, sizeof(frameText), "FR %u SV %u CL %u OT %u",
			(unsigned int)g_SPXBPerfFrameMsec,
			(unsigned int)g_SPXBPerfServerMsec,
			(unsigned int)g_SPXBPerfClientMsec,
			comOtherMsec);
		Com_sprintf(gameText, sizeof(gameText), "GM %u FE %u BE %u SD %u",
			(unsigned int)g_SPXBPerfGameMsec,
			(unsigned int)g_SPXBPerfFrontendMsec,
			(unsigned int)g_SPXBPerfBackendMsec,
			(unsigned int)g_SPXBPerfScreenDrawMsec);
		Com_sprintf(submitText, sizeof(submitText), "DR %u SW %u PR %u FN %u",
			(unsigned int)g_SPXBPerfBackendDrawSurfsMsec,
			(unsigned int)g_SPXBPerfBackendSwapMsec,
			(unsigned int)g_SPXBPerfPresentMsec,
			(unsigned int)g_SPXBPerfFinishMsec);
		Com_sprintf(waitText, sizeof(waitText), "EV %u CB %u CP %u CT %u AU %u",
			(unsigned int)g_SPXBPerfComEventMsec,
			(unsigned int)g_SPXBPerfComCommandMsec,
			(unsigned int)g_SPXBPerfClientPreambleMsec,
			(unsigned int)g_SPXBPerfClientTailMsec,
			(unsigned int)g_SPXBPerfAudioMsec);
		Com_sprintf(frontendText0, sizeof(frontendText0), "ST %u ML %u WO %u PO %u",
			(unsigned int)g_SPXBPerfRenderSetupMsec,
			(unsigned int)g_SPXBPerfRenderMarkLeavesMsec,
			(unsigned int)g_SPXBPerfRenderWorldMsec,
			(unsigned int)g_SPXBPerfRenderPolysMsec);
		Com_sprintf(frontendText1, sizeof(frontendText1), "PJ %u EN %u SO %u DG %u",
			(unsigned int)g_SPXBPerfRenderProjectionMsec,
			(unsigned int)g_SPXBPerfRenderEntitiesMsec,
			(unsigned int)g_SPXBPerfRenderSortMsec,
			(unsigned int)g_SPXBPerfRenderDebugMsec);
		Com_sprintf(drawPhaseText, sizeof(drawPhaseText), "DS %u RQ %u PK %u IX %u",
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawStateCyclesCurrent),
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawReserveCyclesCurrent),
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawPackCyclesCurrent),
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawIndexCyclesCurrent));
		Com_sprintf(pushWaitText, sizeof(pushWaitText), "BP %u MX %u O1 %u O10 %u",
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawBeginPushCyclesCurrent),
			STEFX_XboxCyclesToMsec(g_SPXBPerfDrawBeginPushMaxCyclesCurrent),
			(unsigned int)g_SPXBPerfDrawBeginPushOver1MsecCurrent,
			(unsigned int)g_SPXBPerfDrawBeginPushOver10MsecCurrent);
		frontendGapMsec = g_SPXBPerfFrontendMsec > g_SPXBPerfRenderTotalMsec
			? g_SPXBPerfFrontendMsec - g_SPXBPerfRenderTotalMsec
			: 0;
		backendOwnedMsec = g_SPXBPerfBackendDrawSurfsMsec +
			g_SPXBPerfBackendSwapMsec + g_SPXBPerfBackendOtherMsec;
		backendGapMsec = g_SPXBPerfBackendMsec > backendOwnedMsec
			? g_SPXBPerfBackendMsec - backendOwnedMsec
			: 0;
		Com_sprintf(ownerGapText, sizeof(ownerGapText), "RT %u FG %u BO %u BG %u",
			(unsigned int)g_SPXBPerfRenderTotalMsec,
			frontendGapMsec,
			(unsigned int)g_SPXBPerfBackendOtherMsec,
			backendGapMsec);
		detailNextCapture = now + 500;
	}
	Com_sprintf(phaseText, sizeof(phaseText), "PH %08X CT %08X",
		(unsigned int)g_SPXBPhaseLast,
		(unsigned int)g_SPXBClTailStage);
	if (detailVisible)
	{
		boxWidth = 8 + 36 * SMALLCHAR_WIDTH;
		boxHeight = 12 * SMALLCHAR_HEIGHT + 6;
	}
	#endif

	SCR_FillRect(4, 4, boxWidth, boxHeight, background);
	SCR_DrawHardwareTextLine(8, 7, text, shadow, color);
	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (detailVisible)
	{
		SCR_DrawHardwareTextLine(8, 7 + SMALLCHAR_HEIGHT, loopText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 2 * SMALLCHAR_HEIGHT, frameText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 3 * SMALLCHAR_HEIGHT, gameText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 4 * SMALLCHAR_HEIGHT, submitText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 5 * SMALLCHAR_HEIGHT, waitText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 6 * SMALLCHAR_HEIGHT, frontendText0, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 7 * SMALLCHAR_HEIGHT, frontendText1, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 8 * SMALLCHAR_HEIGHT, drawPhaseText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 9 * SMALLCHAR_HEIGHT, pushWaitText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 10 * SMALLCHAR_HEIGHT, ownerGapText, shadow, color);
		SCR_DrawHardwareTextLine(8, 7 + 11 * SMALLCHAR_HEIGHT, phaseText, shadow, color);
	}
	#endif
	re.SetColor(NULL);
}
#endif


/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
** SCR_GetBigStringWidth
*/ 
int	SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * 16;
}

//===============================================================================


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/
#ifndef _XBOX
typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[0] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, 
		w, cl_graphheight->integer, 0, 0, 0, 0, 0 );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, 0 );
	}
}
#endif	// _XBOX
//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

	scr_initialized = qtrue;
}


//=======================================================

void UI_SetActiveMenu( const char* menuname,const char *menuID );
void _UI_Refresh( int realtime );
void UI_DrawConnect( const char *servername, const char * updateInfoString );
#ifdef _XBOX
extern bool g_xboxDirectMapBootQueued;
extern bool Sys_IsDirectMapBoot(void);
#endif

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
#ifdef _XBOX
	static int s_xboxDrawScreenTraceCount = 0;
	static int s_xboxDrawScreenActiveTraceCount = 0;
	const int xboxTraceScreenLate = (cls.state == CA_ACTIVE && cl.serverTime >= 3600 && cl.serverTime <= 5000);
	const int xboxTraceScreen = (cls.state == CA_ACTIVE)
		? (s_xboxDrawScreenActiveTraceCount < 16 || ((s_xboxDrawScreenActiveTraceCount & 255) == 0))
		: (s_xboxDrawScreenTraceCount < 8);
	if (xboxTraceScreen)
	{
		XBLF("JA: SCR_DrawScreenField enter state=%d serverTime=%d stereo=%d", (int)cls.state, cl.serverTime, (int)stereoFrame);
	}
	if (xboxTraceScreenLate)
	{
		XBLF("JA: CL_EARLY SCR_DrawScreenField enter state=%d frame=%d serverTime=%d stereo=%d",
			(int)cls.state, cls.framecount, cl.serverTime, (int)stereoFrame);
	}
#endif

	#ifdef _XBOX
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_DrawScreenField before re.BeginFrame");
	if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: re.BeginFrame...");
	#endif
#ifdef _XBOX
	g_SPXBPhaseLast = 0x53464230; /* 'SFB0' */
#endif
	g_SPXBClTailStage = 0x44533030; /* 'DS00' */
	re.BeginFrame( stereoFrame );
	g_SPXBClTailStage = 0x44533031; /* 'DS01' */
#ifdef _XBOX
	g_SPXBPhaseLast = 0x53464231; /* 'SFB1' */
#endif
#ifdef _XBOX
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_DrawScreenField after re.BeginFrame");
	if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: re.BeginFrame done");
#endif

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
#ifndef _XBOX
	// Xbox no want this
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.SetColor( g_color_table[0] );
			re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, 0 );
			re.SetColor( NULL );
		}
	}
#endif

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
#ifdef _XBOX
	g_SPXBClTailStage = 0x44533130; /* 'DS10' */
	const qboolean xboxUiCatcherActive = (qboolean)((cls.keyCatchers & KEYCATCH_UI) != 0);
	g_SPXBClTailStage = 0x44533131; /* 'DS11' */
	const qboolean stefxObjectivesOverlayClientActive = CL_STEFX_ObjectivesOverlayActive();
	g_SPXBClTailStage = 0x44533132; /* 'DS12' */
	const qboolean stefxMissionFailedOverlayClientActive = CL_STEFX_MissionFailedOverlayActive();
	g_SPXBClTailStage = 0x44533133; /* 'DS13' */
	const qboolean xboxDirectMapBoot = Sys_IsDirectMapBoot() ? qtrue : qfalse;
	g_SPXBClTailStage = 0x44533134; /* 'DS14' */
	const qboolean xboxForceDirectMapGameDraw =
		(xboxDirectMapBoot && cls.state == CA_ACTIVE && !xboxUiCatcherActive && !stefxObjectivesOverlayClientActive && !stefxMissionFailedOverlayClientActive);
	g_SPXBClTailStage = 0x44533135; /* 'DS15' */
	const qboolean stefxObjectivesOverlayActive =
		(qboolean)(stefxObjectivesOverlayClientActive || Cvar_VariableIntegerValue("stefx_objectivesOverlay") != 0);
	g_SPXBClTailStage = 0x44533136; /* 'DS16' */
	static int s_stefxObjectivesScreenTraceBudget = 16;
#else
	const qboolean xboxForceDirectMapGameDraw = qfalse;
	const qboolean stefxObjectivesOverlayActive = qfalse;
#endif
#ifdef _XBOX
	if ((stefxObjectivesOverlayActive || xboxUiCatcherActive) && s_stefxObjectivesScreenTraceBudget > 0)
	{
		XBLog_Write(va("STEFX: SCR objective overlay decision state=%d serverTime=%d uiStarted=%d catcher=0x%x overlay='%s' paused='%s' fullscreen=%d forceMap=%d",
			(int)cls.state,
			cl.serverTime,
			(int)cls.uiStarted,
			(unsigned int)cls.keyCatchers,
			Cvar_VariableString("stefx_objectivesOverlay"),
			Cvar_VariableString("cl_paused"),
			_UI_IsFullscreen() ? 1 : 0,
			xboxForceDirectMapGameDraw ? 1 : 0));
		--s_stefxObjectivesScreenTraceBudget;
	}
#endif
#ifdef _XBOX
	static int stefxCinematicFullscreenBypassLogBudget = 4;
	g_SPXBClTailStage = 0x44533230; /* 'DS20' */
	const qboolean stefxInGameCinematic = CL_IsRunningInGameCinematic();
	g_SPXBClTailStage = 0x44533231; /* 'DS21' */
	const qboolean stefxCinematicStandby = CL_InGameCinematicOnStandBy();
	g_SPXBClTailStage = 0x44533232; /* 'DS22' */
	const qboolean stefxCinematicNeedsDraw =
		(cls.state == CA_CINEMATIC || stefxInGameCinematic || stefxCinematicStandby);
	g_SPXBClTailStage = 0x44533233; /* 'DS23' */
	if (stefxCinematicNeedsDraw && _UI_IsFullscreen() && stefxCinematicFullscreenBypassLogBudget > 0)
	{
		XBLF("STEFX: SCR_DrawScreenField drawing cinematic despite fullscreen UI state=%d catcher=0x%x",
			(int)cls.state, (unsigned int)cls.keyCatchers);
		--stefxCinematicFullscreenBypassLogBudget;
	}
	{
		static int s_stefxCinDecisionBudget = 24;
		if (stefxCinematicNeedsDraw && s_stefxCinDecisionBudget > 0)
		{
			XBLF("STEFX_CIN_DRAW: screen decision state=%d uiStarted=%d catcher=0x%x fullscreen=%d needs=%d forceMap=%d",
				(int)cls.state,
				(int)cls.uiStarted,
				(unsigned int)cls.keyCatchers,
				_UI_IsFullscreen() ? 1 : 0,
				stefxCinematicNeedsDraw ? 1 : 0,
				xboxForceDirectMapGameDraw ? 1 : 0);
			--s_stefxCinDecisionBudget;
		}
	}
#else
	const qboolean stefxCinematicNeedsDraw = qfalse;
#endif
#ifdef _XBOX
	g_SPXBClTailStage = 0x44533330; /* 'DS30' */
	const qboolean stefxUiFullscreen = _UI_IsFullscreen();
	g_SPXBClTailStage = 0x44533331; /* 'DS31' */
#else
	const qboolean stefxUiFullscreen = _UI_IsFullscreen();
#endif
	if ( stefxCinematicNeedsDraw || xboxForceDirectMapGameDraw || !stefxUiFullscreen ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			if (!(cls.keyCatchers & KEYCATCH_UI))
			{
				UI_SetActiveMenu( "mainMenu",NULL );	//			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			}
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			UI_DrawConnect( clc.servername, cls.updateInfoString );
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// We got past the time when the UI needs to prevent swapping
			extern bool connectSwapOverride;
			connectSwapOverride = false;

			// draw the game information screen and loading progress
			g_SPXBClTailStage = 0x44533032; /* 'DS02' */
			CL_CGameRendering( stereoFrame );
			g_SPXBClTailStage = 0x44533033; /* 'DS03' */
			break;
		case CA_ACTIVE:
			#ifdef _XBOX
			g_SPXBClTailStage = 0x44533430; /* 'DS40' */
			#endif
			if (stefxInGameCinematic || stefxCinematicStandby)
			{
				#ifdef _XBOX
				g_SPXBClTailStage = 0x44533431; /* 'DS41' */
				#endif
				SCR_DrawCinematic();				
				#ifdef _XBOX
				g_SPXBClTailStage = 0x44533432; /* 'DS42' */
				#endif
			}
			else
			{
				#ifdef _XBOX
				g_SPXBClTailStage = 0x44533433; /* 'DS43' */
				if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_DrawScreenField before CL_CGameRendering");
				if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: CL_CGameRendering...");
				#endif
				g_SPXBPhaseLast = 0x53464330; /* 'SFC0' */
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
				{
					const int stefxCgameStart = Sys_Milliseconds();
					CL_CGameRendering( stereoFrame );
					g_SPXBPerfCgameDrawMsec =
						(unsigned int)(Sys_Milliseconds() - stefxCgameStart);
				}
#else
				CL_CGameRendering( stereoFrame );
#endif
				g_SPXBClTailStage = 0x44533434; /* 'DS44' */
				g_SPXBPhaseLast = 0x53464331; /* 'SFC1' */
				#ifdef _XBOX
				if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_DrawScreenField after CL_CGameRendering");
				if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: CL_CGameRendering done");
				#endif
			}
			break;
		}
	}

#ifndef _XBOX // on xbox this is rendered right before a flip
	re.ProcessDissolve();
#endif // _XBOX

	// draw downloading progress bar

	// the menu draws next
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( stefxObjectivesOverlayClientActive )
	{
		CL_STEFX_DrawObjectivesOverlay();
		return;
	}
	if ( stefxMissionFailedOverlayClientActive )
	{
		CL_STEFX_DrawMissionFailedOverlay();
		return;
	}
#endif
#ifdef _XBOX
	if (xboxForceDirectMapGameDraw) {
		if (stefxObjectivesOverlayActive)
		{
			XBLog_Write("STEFX: SCR objective overlay blocked by direct-map return");
		}
		if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_DrawScreenField direct-map return before UI");
		if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: skipping UI refresh for direct-map active frame");
		return;
	}
#endif
	#ifdef _XBOX
	if (stefxObjectivesOverlayActive)
	{
		XBLog_Write("STEFX: SCR objective overlay calling _UI_Refresh");
	}
	if (xboxTraceScreen) XBLog_Write("JA: SCR_DrawScreenField: _UI_Refresh...");
	#endif
#ifdef _XBOX
	g_SPXBPhaseLast = 0x53465530; /* 'SFU0' */
#endif
	g_SPXBClTailStage = 0x44533034; /* 'DS04' */
	_UI_Refresh( cls.realtime );
	g_SPXBClTailStage = 0x44533035; /* 'DS05' */
#ifdef _XBOX
	g_SPXBPhaseLast = 0x53465531; /* 'SFU1' */
#endif
#ifdef _XBOX
	if (xboxTraceScreen)
	{
		XBLog_Write("JA: SCR_DrawScreenField: _UI_Refresh done");
		XBLog_Write("JA: SCR_DrawScreenField done");
		if (cls.state == CA_ACTIVE)
		{
			s_xboxDrawScreenActiveTraceCount++;
		}
		else
		{
			s_xboxDrawScreenTraceCount++;
		}
	}
#endif

	// console draws next
//	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
#ifndef _XBOX
	if ( cl_debuggraph->integer || cl_timegraph->integer ) {
		SCR_DrawDebugGraph ();
	}
#endif
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int	recursive;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	int xboxScreenPhaseStart;
#endif

	if ( !scr_initialized ) {
		return;				// not initialized yet
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	static int s_xboxUpdateScreenTraceCount = 0;
	static int s_xboxUpdateScreenActiveTraceCount = 0;
	const int xboxTraceScreenLate = (cls.state == CA_ACTIVE && cl.serverTime >= 3600 && cl.serverTime <= 4600);
	const int xboxTraceScreenTight = xboxTraceScreenLate;
	const int xboxTraceScreen = (cls.state == CA_ACTIVE)
		? (s_xboxUpdateScreenActiveTraceCount < 16 || ((s_xboxUpdateScreenActiveTraceCount & 255) == 0))
		: (s_xboxUpdateScreenTraceCount < 8);
	static int s_xboxUpdateScreenCinematicTraceCount = 0;
	if (cls.state == CA_CINEMATIC && s_xboxUpdateScreenCinematicTraceCount < 24)
	{
		XBLF("STEFX_CIN_DRAW: update enter state=%d ui=%d cgame=%d catcher=0x%x recursive=%d",
			(int)cls.state,
			cls.uiStarted ? 1 : 0,
			cls.cgameStarted ? 1 : 0,
			(unsigned int)cls.keyCatchers,
			recursive);
	}
	if (xboxTraceScreenLate)
	{
		XBLF("JA: CL_EARLY SCR_UpdateScreen enter state=%d frame=%d realtime=%d serverTime=%d recursive=%d stereo=%d",
			(int)cls.state, cls.framecount, cls.realtime, cl.serverTime, recursive, cls.glconfig.stereoEnabled);
	}
#endif

	// load the ref / ui / cgame if needed
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBPhaseLast = 0x53524330; /* 'SRC0' */
#endif
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBClTailStage = 0x53533030; /* 'SS00' */
	#endif
	CL_StartHunkUsers();
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBClTailStage = 0x53533031; /* 'SS01' */
	g_SPXBPhaseLast = 0x53524331; /* 'SRC1' */
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (cls.state == CA_CINEMATIC && s_xboxUpdateScreenCinematicTraceCount < 24)
	{
		XBLF("STEFX_CIN_DRAW: update after hunk state=%d ui=%d cgame=%d catcher=0x%x",
			(int)cls.state,
			cls.uiStarted ? 1 : 0,
			cls.cgameStarted ? 1 : 0,
			(unsigned int)cls.keyCatchers);
	}
	if (xboxTraceScreenLate)
	{
		XBLF("JA: CL_EARLY SCR_UpdateScreen after CL_StartHunkUsers state=%d ui=%d cgame=%d",
			(int)cls.state, cls.uiStarted, cls.cgameStarted);
	}
#endif

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = qtrue;

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceScreenLate)
	{
		XBLF("JA: CL_EARLY SCR_UpdateScreen recursive set recursive=%d", recursive);
	}
#endif

	// if running in stereo, we need to draw the frame twice
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxScreenPhaseStart = Sys_Milliseconds();
#endif
	if ( cls.glconfig.stereoEnabled ) {
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT draw left");
		if (xboxTraceScreen) XBLog_Write("JA: SCR_UpdateScreen: draw left...");
#endif
		SCR_DrawScreenField( STEREO_LEFT );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT draw right");
		if (xboxTraceScreen) XBLog_Write("JA: SCR_UpdateScreen: draw right...");
#endif
		SCR_DrawScreenField( STEREO_RIGHT );
	} else {
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_UpdateScreen before draw center");
		if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT before draw center");
		if (xboxTraceScreen) XBLog_Write("JA: SCR_UpdateScreen: draw center...");
#endif
		#ifdef _XBOX
		g_SPXBPhaseLast = 0x53524332; /* 'SRC2' */
		g_SPXBClTailStage = 0x53533032; /* 'SS02' */
		#endif
		SCR_DrawScreenField( STEREO_CENTER );
		#ifdef _XBOX
		g_SPXBClTailStage = 0x53533033; /* 'SS03' */
		g_SPXBPhaseLast = 0x53524333; /* 'SRC3' */
		#endif
	}
#if defined(_XBOX) && STEFX_TEMP_HARDWARE_FPS_COUNTER
	SCR_DrawHardwareFPS();
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBPerfScreenDrawMsec = (unsigned int)(Sys_Milliseconds() - xboxScreenPhaseStart);
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_UpdateScreen after draw");
	if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT draw done");
	if (xboxTraceScreen) XBLog_Write("JA: SCR_UpdateScreen: draw done");
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_UpdateScreen before re.EndFrame");
	if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT before re.EndFrame");
	if (xboxTraceScreen) XBLog_Write("JA: SCR_UpdateScreen: re.EndFrame...");
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxScreenPhaseStart = Sys_Milliseconds();
#endif
	#ifdef _XBOX
	g_SPXBPhaseLast = 0x53524334; /* 'SRC4' */
	g_SPXBClTailStage = 0x53533034; /* 'SS04' */
	#endif
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	re.EndFrame( &time_frontend, &time_backend );
	g_SPXBPerfFrontendMsec = (unsigned int)time_frontend;
	g_SPXBPerfBackendMsec = (unsigned int)time_backend;
	#else
	if ( com_speeds->integer ) {
		re.EndFrame( &time_frontend, &time_backend );
	} else {
		re.EndFrame( NULL, NULL );
	}
	#endif
	#ifdef _XBOX
	g_SPXBClTailStage = 0x53533035; /* 'SS05' */
	g_SPXBPhaseLast = 0x53524335; /* 'SRC5' */
	#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBPerfEndFrameMsec = (unsigned int)(Sys_Milliseconds() - xboxScreenPhaseStart);
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_UpdateScreen after re.EndFrame");
	if (xboxTraceScreenTight) XBLog_Write("JA: SCR_TIGHT after re.EndFrame");
	if (xboxTraceScreen)
	{
		XBLog_Write("JA: SCR_UpdateScreen: re.EndFrame done");
		if (cls.state == CA_ACTIVE)
		{
			s_xboxUpdateScreenActiveTraceCount++;
		}
		else
		{
			s_xboxUpdateScreenTraceCount++;
		}
	}
#endif

	recursive = 0;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceScreenLate) XBLog_Write("JA: CL_EARLY SCR_UpdateScreen before return");
	if (cls.state == CA_CINEMATIC && s_xboxUpdateScreenCinematicTraceCount < 24)
	{
		XBLF("STEFX_CIN_DRAW: update exit state=%d ui=%d cgame=%d catcher=0x%x",
			(int)cls.state,
			cls.uiStarted ? 1 : 0,
			cls.cgameStarted ? 1 : 0,
			(unsigned int)cls.keyCatchers);
		++s_xboxUpdateScreenCinematicTraceCount;
	}
#endif
}

// this stuff is only used by the savegame (SG) code for screenshots...
//
#ifdef _XBOX

/*
static byte	bScreenData[SG_SCR_WIDTH * SG_SCR_HEIGHT * 4];
static qboolean screenDataValid = qfalse;
void SCR_UnprecacheScreenshot()
{
	screenDataValid = qfalse;
}
*/

void SCR_PrecacheScreenshot()
{
	// No screenshots unless connected to single player local server...
	//
//	char *psInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
//	int iMaxClients = atoi(Info_ValueForKey( psInfo, "sv_maxclients" ));		

	// (no need to check single-player status in voyager, this code base is all singleplayer)
	if ( cls.state != CA_ACTIVE )
	{	
		return;
	}

#ifndef _XBOX
	if (cls.keyCatchers == 0)
	{
		// in-game...
		//
//		SCR_UnprecacheScreenshot();
//		pbScreenData = (byte *)Z_Malloc(SG_SCR_WIDTH * SG_SCR_HEIGHT * 4);		
		S_ClearSoundBuffer();	// clear DMA etc because the following glReadPixels() call can take ages
		re.GetScreenShot( (byte *) &bScreenData, SG_SCR_WIDTH, SG_SCR_HEIGHT);
		screenDataValid = qtrue;
	}
	else
	{
		// we're in the console, or menu, or message input...
		//
	}
#endif

	// save the current screenshot to the user space to be used
	// with a savegame
#ifdef _XBOX
	extern void SaveCompressedScreenshot( void );
	SaveCompressedScreenshot();
#endif

}

/*
byte *SCR_GetScreenshot(qboolean *qValid)
{
	if (!screenDataValid) {
		SCR_PrecacheScreenshot();
	}
	if (qValid) {
		*qValid = screenDataValid;
	}
	return (byte *)&bScreenData;
}

// called from save-game code to set the lo-res loading screen to be the one from the save file...
//
void SCR_SetScreenshot(const byte *pbData, int w, int h)
{
	if (w == SG_SCR_WIDTH && h == SG_SCR_HEIGHT)
	{
		screenDataValid = qtrue;
		memcpy(&bScreenData, pbData, SG_SCR_WIDTH*SG_SCR_HEIGHT*4);
	}
	else
	{
		screenDataValid = qfalse;
		memset(&bScreenData, 0,      SG_SCR_WIDTH*SG_SCR_HEIGHT*4);
	}
}
*/

// This is just a client-side wrapper for the function RE_TempRawImage_ReadFromFile() in the renderer code...
//
/*
byte* SCR_TempRawImage_ReadFromFile(const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip)
{
	return re.TempRawImage_ReadFromFile(psLocalFilename, piWidth, piHeight, pbReSampleBuffer, qbVertFlip);
}
//
// ditto (sort of)...
//
void  SCR_TempRawImage_CleanUp()
{
	re.TempRawImage_CleanUp();
}
*/
#endif
