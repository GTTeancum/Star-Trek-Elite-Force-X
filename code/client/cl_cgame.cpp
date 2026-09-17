// cl_cgame.c  -- client system interaction with client game

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"

#ifdef _XBOX
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBClTailStage;
#if defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBHMGameCommandCount;
extern "C" volatile unsigned int g_SPXBHMGameCommandResult;
extern "C" volatile unsigned int g_SPXBHMScoreDrawState;
#if defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBHMScoreStretchCount;
extern "C" volatile unsigned int g_SPXBHMScoreStretchShader;
extern "C" volatile unsigned int g_SPXBHMScoreStretchX;
extern "C" volatile unsigned int g_SPXBHMScoreStretchY;
extern "C" volatile unsigned int g_SPXBHMScoreStretchW;
extern "C" volatile unsigned int g_SPXBHMScoreStretchH;
#endif
#endif
#endif

#include "../ui/ui_shared.h"

#include "../RMG/RM_Headers.h"

#ifdef VV_LIGHTING
#endif
	   		

#include "client.h"
#if defined(STEFX_ELITE_FORCE_SP)
#include "../qcommon/stefx_snapshot_abi.h"
#endif

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
extern qboolean STEFX_HolomatchGetSplitHudState(int slot, int *health, int *weapon, int *score);
extern const void *STEFX_HolomatchGetOfficialPlayerState(int clientNum);
extern "C" void STEFX_HM_CG_AddSplitViewWeapon(int slot, const void *officialPlayerState,
	const float *vieworg, const float *viewaxis, float fovX, int viewTime);
extern "C" void STEFX_HM_CG_DrawSplitPlayerHud(int slot, const void *officialPlayerState,
	const float *vieworg, const float *viewaxis, float fovX, float fovY, int viewTime);
extern "C" void STEFX_HM_CG_DrawSplitGlobalHud(void);
extern qboolean RE_STEFX_SplitScreen_GetLocalRefdef(int slot, refdef_t *refdef);
extern "C" volatile unsigned int g_SPXBHMAudioRegisterSoundCount;
extern "C" volatile unsigned int g_SPXBHMAudioStartSoundCount;
extern "C" volatile unsigned int g_SPXBHMAudioStartLocalCount;
extern "C" volatile unsigned int g_SPXBHMAudioLoopCount;
extern "C" volatile unsigned int g_SPXBHMAudioRespatializeCount;
extern "C" volatile unsigned int g_SPXBHMAudioLastEntChan;
extern "C" volatile unsigned int g_SPXBHMAudioLastHandle;
extern "C" volatile unsigned int g_SPXBHMSplitHudSerial[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudRectX[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudRectY[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudRectW[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudRectH[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusSerial[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusValid[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusHealth[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusWeapon[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusScore[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusRectX[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusRectY[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusRectW[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudStatusRectH[4];
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerSerial;
extern "C" volatile unsigned int g_SPXBHMSplitHudPlayers;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerVerticalX;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerVerticalY;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerVerticalW;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerVerticalH;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerHorizontalX;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerHorizontalY;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerHorizontalW;
extern "C" volatile unsigned int g_SPXBHMSplitHudDividerHorizontalH;

static int s_stefxHolomatchHudDrawSlot = -1;
static int s_stefxHolomatchHudFramePlayers = 0;

static qboolean CL_STEFX_HolomatchAudioProofEnabled(void)
{
	return Cvar_VariableIntegerValue("stefx_hm_audio_proof") ? qtrue : qfalse;
}

static void CL_STEFX_HolomatchAudioProofLog(const char *tag, int ent, int efChan, int spChan, int handle, const float *origin)
{
	static int s_stefxHMAudioProofLogBudget = 96;

	if (!CL_STEFX_HolomatchAudioProofEnabled() || s_stefxHMAudioProofLogBudget <= 0)
	{
		return;
	}

	XBLog_WriteCriticalf("STEFX_HM_AUDIO: %s ent=%d efChan=%d spChan=%d handle=%d origin=(%g,%g,%g)",
		tag ? tag : "event",
		ent,
		efChan,
		spChan,
		handle,
		origin ? origin[0] : 0.0f,
		origin ? origin[1] : 0.0f,
		origin ? origin[2] : 0.0f);
	--s_stefxHMAudioProofLogBudget;
}

static int CL_STEFX_HolomatchSplitHudPlayers(void)
{
	const char *mode;
	int players;
	int slot;

	if (!Cvar_VariableIntegerValue("stefx_splitScreen") ||
		Cvar_VariableIntegerValue("stefx_splitScreenPlayers") < 2)
	{
		return 0;
	}

	mode = Cvar_VariableString("stefx_splitScreenMode");
	if (!mode || Q_stricmp(mode, "holomatch"))
	{
		return 0;
	}

	players = Cvar_VariableIntegerValue("stefx_splitScreenPlayers");
	if (players < 2)
	{
		return 0;
	}
	if (players > 4)
	{
		players = 4;
	}

	/* The renderer deliberately waits until every secondary camera exists
	 * before arming split-screen.  Keep HUD drawing behind the same barrier;
	 * otherwise a bot that joins early paints its quadrant HUD over P1's
	 * still-full-screen camera while the remaining viewport bots are loading. */
	for (slot = 1; slot < players; ++slot)
	{
		refdef_t splitRefdef;
		if (!RE_STEFX_SplitScreen_GetLocalRefdef(slot, &splitRefdef))
		{
			return 0;
		}
	}
	return players;
}

static void CL_STEFX_HolomatchSplitRect(int players, int slot, float *x, float *y, float *w, float *h)
{
	if (!x || !y || !w || !h)
	{
		return;
	}

	*x = 0.0f;
	*y = 0.0f;
	*w = 640.0f;
	*h = 480.0f;
	if (players == 2)
	{
		*y = slot ? 240.0f : 0.0f;
		*h = 240.0f;
	}
	else if (players == 3)
	{
		*y = slot ? 240.0f : 0.0f;
		*h = 240.0f;
		if (slot > 0)
		{
			*x = slot == 2 ? 320.0f : 0.0f;
			*w = 320.0f;
		}
	}
	else if (players >= 4)
	{
		*x = (slot & 1) ? 320.0f : 0.0f;
		*y = (slot & 2) ? 240.0f : 0.0f;
		*w = 320.0f;
		*h = 240.0f;
	}
}

static void CL_STEFX_HolomatchMapHudToSplitSlot(int players, int slot, float *x, float *y, float *w, float *h)
{
	float rectX;
	float rectY;
	float rectW;
	float rectH;
	float xScale;
	float yScale;

	if (!x || !y || !w || !h)
	{
		return;
	}

	CL_STEFX_HolomatchSplitRect(players, slot, &rectX, &rectY, &rectW, &rectH);
	xScale = rectW / 640.0f;
	yScale = rectH / 480.0f;
	*x = rectX + *x * xScale;
	*y = rectY + *y * yScale;
	*w *= xScale;
	*h *= yScale;
}

static void CL_STEFX_DrawHolomatchSplitSmallChar(int x, int y, int ch)
{
	int row;
	int col;
	float frow;
	float fcol;
	float size;
	float size2;

	ch &= 255;
	if (ch == ' ' || y < -SMALLCHAR_HEIGHT)
	{
		return;
	}

	row = ch >> 4;
	col = ch & 15;
	frow = row * 0.0625f;
	fcol = col * 0.0625f;
	size = 0.03125f;
	size2 = 0.0625f;

	re.DrawStretchPic((float)x, (float)y,
		(float)SMALLCHAR_WIDTH, (float)SMALLCHAR_HEIGHT,
		fcol, frow, fcol + size, frow + size2,
		cls.charSetShader);
}

static void CL_STEFX_DrawHolomatchSplitSmallStringColor(int x, int y, const char *text, const float *color)
{
	int i;

	if (!text)
	{
		return;
	}

	re.SetColor(color);
	for (i = 0; text[i]; ++i)
	{
		CL_STEFX_DrawHolomatchSplitSmallChar(x + i * SMALLCHAR_WIDTH, y, text[i]);
	}
	re.SetColor(NULL);
}

static void CL_STEFX_DrawHolomatchSplitStatusOverlay(void)
{
	static int s_stefxHMSplitStatusLogBudget = 24;
	static int s_stefxHMSplitDividerLogBudget = 6;
	static int s_stefxHMSplitLoggedPlayers = 0;
	int players;
	int slot;
	float verticalY;
	float verticalH;

	players = s_stefxHolomatchHudFramePlayers;
	if (players <= 0)
	{
		return;
	}
	if (players != s_stefxHMSplitLoggedPlayers)
	{
		s_stefxHMSplitLoggedPlayers = players;
		s_stefxHMSplitStatusLogBudget = 24;
		s_stefxHMSplitDividerLogBudget = 6;
	}

	verticalY = players == 3 ? 240.0f : 0.0f;
	verticalH = players >= 4 ? 480.0f : (players == 3 ? 240.0f : 0.0f);
	/* The viewport rectangles already meet at x=320/y=240.  Do not paint a
	   separator over either edge: local split-screen uses the full 640x480. */
	g_SPXBHMSplitHudPlayers = (unsigned int)players;
	g_SPXBHMSplitHudDividerVerticalX = 320u;
	g_SPXBHMSplitHudDividerVerticalY = (unsigned int)(int)verticalY;
	g_SPXBHMSplitHudDividerVerticalW = 0u;
	g_SPXBHMSplitHudDividerVerticalH = (unsigned int)(int)verticalH;
	g_SPXBHMSplitHudDividerHorizontalX = 0u;
	g_SPXBHMSplitHudDividerHorizontalY = 240u;
	g_SPXBHMSplitHudDividerHorizontalW = 640u;
	g_SPXBHMSplitHudDividerHorizontalH = 0u;
	++g_SPXBHMSplitHudDividerSerial;
	if (s_stefxHMSplitDividerLogBudget > 0)
	{
		XBLog_WriteCriticalf("STEFX_HM_SPLIT_HUD_DIVIDER: players=%d vertical=(320,%g 0x%g) horizontal=(0,240 640x0)",
			players, verticalY, verticalH);
		--s_stefxHMSplitDividerLogBudget;
	}

	for (slot = 0; slot < players && slot < 4; ++slot)
	{
		int health = 0;
		int weapon = 0;
		int score = 0;
		float x;
		float y;
		float width;
		float height;
		qboolean valid = STEFX_HolomatchGetSplitHudState(slot, &health, &weapon, &score);
		CL_STEFX_HolomatchSplitRect(players, slot, &x, &y, &width, &height);

		++g_SPXBHMSplitHudStatusSerial[slot];
		g_SPXBHMSplitHudStatusValid[slot] = valid ? 1u : 0u;
		g_SPXBHMSplitHudStatusHealth[slot] = (unsigned int)health;
		g_SPXBHMSplitHudStatusWeapon[slot] = (unsigned int)weapon;
		g_SPXBHMSplitHudStatusScore[slot] = (unsigned int)score;
		g_SPXBHMSplitHudStatusRectX[slot] = (unsigned int)(int)x;
		g_SPXBHMSplitHudStatusRectY[slot] = (unsigned int)(int)y;
		g_SPXBHMSplitHudStatusRectW[slot] = (unsigned int)(int)width;
		g_SPXBHMSplitHudStatusRectH[slot] = (unsigned int)(int)height;

		if (s_stefxHMSplitStatusLogBudget > 0)
		{
			XBLog_WriteCriticalf("STEFX_HM_SPLIT_HUD_STATUS: slot=%d players=%d valid=%d health=%d weapon=%d score=%d dst=(%g,%g %gx%g)",
				slot,
				players,
				valid ? 1 : 0,
				health,
				weapon,
				score,
				x,
				y,
				width,
				height);
			--s_stefxHMSplitStatusLogBudget;
		}
	}
}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean CL_STEFX_IsVisibleBrushMoverEntity( const entityState_t *state )
{
	if ( !state )
	{
		return qfalse;
	}
	return state->eType == ET_MOVER && state->solid == SOLID_BMODEL && !(state->eFlags & EF_NODRAW);
}
#endif

#ifdef _IMMERSION
#include "../ff/cl_ff.h"
#include "../ff/ff.h"
#else
#include "fffx.h"
#endif // _IMMERSION
#include "vmachine.h"

vm_t	cgvm;
/*
Ghoul2 Insert Start
*/

#if !defined(G2_H_INC)
	#include "../ghoul2/G2.h"
#endif

/*
Ghoul2 Insert End
*/

//FIXME: Temp
extern void S_UpdateAmbientSet ( const char *name, vec3_t origin );
extern int S_AddLocalSet( const char *name, vec3_t listener_origin, vec3_t origin, int entID, int time );
extern void AS_ParseSets( void );
extern sfxHandle_t AS_GetBModelSound( const char *name, int stage );
extern void	AS_AddPrecacheEntry( const char *name );
extern menuDef_t *Menus_FindByName(const char *p);

extern qboolean R_inPVS( vec3_t p1, vec3_t p2 );

void UI_SetActiveMenu( const char* menuname,const char *menuID );

/*
====================
CL_GetGameState
====================
*/
void CL_GetGameState( gameState_t *gs ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	typedef struct stefxGameState_s {
		int			stringOffsets[1024];
		char		stringData[MAX_GAMESTATE_CHARS];
		int			dataCount;
	} stefxGameState_t;

	stefxGameState_t *efGs = (stefxGameState_t *)gs;
	memset( efGs, 0, sizeof(*efGs) );
	for (int i = 0; i < 1024 && i < MAX_CONFIGSTRINGS; i++)
	{
		efGs->stringOffsets[i] = cl.gameState.stringOffsets[i];
	}
	memcpy( efGs->stringData, cl.gameState.stringData, sizeof(efGs->stringData) );
	efGs->dataCount = cl.gameState.dataCount;
	XBLF("STEFX: CL_GetGameState marshalled EF layout dataCount=%d cs0=%d cs1=%d serverinfo='%s'",
		efGs->dataCount,
		efGs->stringOffsets[0],
		efGs->stringOffsets[1],
		efGs->stringData + efGs->stringOffsets[0]);
#else
	*gs = cl.gameState;
#endif
}

/*
====================
CL_GetGlconfig
====================
*/
void CL_GetGlconfig( glconfig_t *glconfig ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	typedef struct stefxGlconfig_s {
		char		renderer_string[MAX_STRING_CHARS];
		char		vendor_string[MAX_STRING_CHARS];
		char		version_string[MAX_STRING_CHARS];
		char		extensions_string[2 * MAX_STRING_CHARS];
		int			maxTextureSize;
		int			maxActiveTextures;
		int			colorBits, depthBits, stencilBits;
		int			driverType;
		int			hardwareType;
		qboolean	deviceSupportsGamma;
		int			textureCompression;
		qboolean	textureEnvAddAvailable;
		qboolean	textureFilterAnisotropicAvailable;
#if !defined(STEFX_SP_HOSTED_MP)
		qboolean	clampToEdgeAvailable;
#endif
		int			vidWidth, vidHeight;
		float		windowAspect;
		int			displayFrequency;
		qboolean	isFullscreen;
		qboolean	stereoEnabled;
		qboolean	smpActive;
	} stefxGlconfig_t;

	stefxGlconfig_t *efGlconfig = (stefxGlconfig_t *)glconfig;
	memset( efGlconfig, 0, sizeof(*efGlconfig) );
	Q_strncpyz( efGlconfig->renderer_string, cls.glconfig.renderer_string ? cls.glconfig.renderer_string : "", sizeof(efGlconfig->renderer_string) );
	Q_strncpyz( efGlconfig->vendor_string, cls.glconfig.vendor_string ? cls.glconfig.vendor_string : "", sizeof(efGlconfig->vendor_string) );
	Q_strncpyz( efGlconfig->version_string, cls.glconfig.version_string ? cls.glconfig.version_string : "", sizeof(efGlconfig->version_string) );
	Q_strncpyz( efGlconfig->extensions_string, cls.glconfig.extensions_string ? cls.glconfig.extensions_string : "", sizeof(efGlconfig->extensions_string) );

	efGlconfig->maxTextureSize = cls.glconfig.maxTextureSize;
	efGlconfig->maxActiveTextures = cls.glconfig.maxActiveTextures > 0 ? cls.glconfig.maxActiveTextures : 1;
	efGlconfig->colorBits = cls.glconfig.colorBits;
	efGlconfig->depthBits = cls.glconfig.depthBits;
	efGlconfig->stencilBits = cls.glconfig.stencilBits;
	efGlconfig->driverType = 0;		// GLDRV_ICD
	efGlconfig->hardwareType = 0;	// GLHW_GENERIC
	efGlconfig->deviceSupportsGamma = cls.glconfig.deviceSupportsGamma;
	efGlconfig->textureCompression = cls.glconfig.textureCompression != TC_NONE ? 1 : 0;
	efGlconfig->textureEnvAddAvailable = cls.glconfig.textureEnvAddAvailable;
	efGlconfig->textureFilterAnisotropicAvailable = cls.glconfig.textureFilterAnisotropicAvailable;
#if !defined(STEFX_SP_HOSTED_MP)
	efGlconfig->clampToEdgeAvailable = cls.glconfig.clampToEdgeAvailable;
#endif
	efGlconfig->vidWidth = cls.glconfig.vidWidth > 0 ? cls.glconfig.vidWidth : 640;
	efGlconfig->vidHeight = cls.glconfig.vidHeight > 0 ? cls.glconfig.vidHeight : 480;
	efGlconfig->windowAspect = (float)efGlconfig->vidWidth / (float)efGlconfig->vidHeight;
#ifdef _XBOX
    extern float GLW_GetPixelAspect(void);
    efGlconfig->windowAspect *= GLW_GetPixelAspect();
#endif

	efGlconfig->displayFrequency = cls.glconfig.displayFrequency;
	efGlconfig->isFullscreen = cls.glconfig.isFullscreen;
	efGlconfig->stereoEnabled = cls.glconfig.stereoEnabled;
	efGlconfig->smpActive = qfalse;

	XBLF("STEFX: CL_GetGlconfig marshalled EF layout %dx%d aspect=%g maxTex=%d activeTex=%d renderer='%s'",
		efGlconfig->vidWidth,
		efGlconfig->vidHeight,
		efGlconfig->windowAspect,
		efGlconfig->maxTextureSize,
		efGlconfig->maxActiveTextures,
		efGlconfig->renderer_string);
#else
	*glconfig = cls.glconfig;
#endif
}


/*
====================
CL_GetUserCmd
====================
*/
qboolean CL_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	// cmds[cmdNumber] is the last properly generated command

	// can't return anything that we haven't created yet
	if ( cmdNumber > cl.cmdNumber ) {
		Com_Error( ERR_DROP, "CL_GetUserCmd: %i >= %i", cmdNumber, cl.cmdNumber );
	}

	// the usercmd has been overwritten in the wrapping
	// buffer because it is too far out of date
	if ( cmdNumber <= cl.cmdNumber - CMD_BACKUP ) {
		return qfalse;
	}

	*ucmd = cl.cmds[ cmdNumber & CMD_MASK ];

	return qtrue;
}

int CL_GetCurrentCmdNumber( void ) {
	return cl.cmdNumber;
}


/*
====================
CL_GetParseEntityState
====================
*/
/*
qboolean	CL_GetParseEntityState( int parseEntityNumber, entityState_t *state ) {
	// can't return anything that hasn't been parsed yet
	if ( parseEntityNumber >= cl.parseEntitiesNum ) {
		Com_Error( ERR_DROP, "CL_GetParseEntityState: %i >= %i",
			parseEntityNumber, cl.parseEntitiesNum );
	}

	// can't return anything that has been overwritten in the circular buffer
	if ( parseEntityNumber <= cl.parseEntitiesNum - MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	*state = cl.parseEntities[ parseEntityNumber & ( MAX_PARSE_ENTITIES - 1 ) ];
	return qtrue;
}
*/

/*
====================
CL_GetCurrentSnapshotNumber
====================
*/
void	CL_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = cl.frame.messageNum;
	*serverTime = cl.frame.serverTime;
}

/*
====================
CL_GetSnapshot
====================
*/
qboolean	CL_GetSnapshot( int snapshotNumber, snapshot_t *snapshot ) {
	clSnapshot_t	*clSnap;
	int				i, count;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_stefxEngineSnapshotLayoutLogged = 0;
	static int s_stefxGetSnapshotLogBudget = 80;
	if ( !s_stefxEngineSnapshotLayoutLogged )
	{
		XBLF("STEFX: engine snapshot layout snapshot=%d ps=%d entity=%d numOfs=%d entitiesOfs=%d max=%d",
			(int)sizeof(snapshot_t),
			(int)sizeof(playerState_t),
			(int)sizeof(entityState_t),
			(int)((byte *)&(((snapshot_t *)0)->numEntities)),
			(int)((byte *)&(((snapshot_t *)0)->entities)),
			MAX_ENTITIES_IN_SNAPSHOT);
		s_stefxEngineSnapshotLayoutLogged = 1;
	}
#endif

	if ( snapshotNumber > cl.frame.messageNum ) {
		Com_Error( ERR_DROP, "CL_GetSnapshot: snapshotNumber > cl.frame.messageNum" );
	}

	// if the frame has fallen out of the circular buffer, we can't return it
	if ( cl.frame.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	// if the frame is not valid, we can't return it
	clSnap = &cl.frames[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	// if the entities in the frame have fallen out of their
	// circular buffer, we can't return it
	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	// write the snapshot
	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->cmdNum = clSnap->cmdNum;
	snapshot->ps = clSnap->ps;
	count = clSnap->numEntities;
	if ( count > MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_GetSnapshot: truncated %i entities to %i\n", count, MAX_ENTITIES_IN_SNAPSHOT );
		count = MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( s_stefxGetSnapshotLogBudget > 0 )
	{
		XBLF("STEFX: engine CL_GetSnapshot num=%d clMsg=%d clSnapValid=%d clSnapEntities=%d copyCount=%d parseBase=%d parseNow=%d",
			snapshotNumber,
			cl.frame.messageNum,
			(int)clSnap->valid,
			clSnap->numEntities,
			count,
			clSnap->parseEntitiesNum,
			cl.parseEntitiesNum);
		--s_stefxGetSnapshotLogBudget;
	}
#endif
/*
Ghoul2 Insert Start
*/
 	for ( i = 0 ; i < count ; i++ ) 
	{

		int entNum =  ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1) ;		
		snapshot->entities[i] = cl.parseEntities[ entNum ];
	}
/*
Ghoul2 Insert End
*/


	// FIXME: configstring changes and server commands!!!

	return qtrue;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
qboolean CL_STEFX_GetSnapshot( int snapshotNumber, void *snapshotBuffer ) {
	clSnapshot_t	*clSnap;
	stefxSnapshot_t	*snapshot;
	int				i, count;
	int				stefxPlayerMask = 0;
	int				stefxPlayerFlags[3] = { 0, 0, 0 };
	vec3_t			stefxPlayerOrigins[3];
	static int		s_stefxLayoutLogged = 0;
	static int		s_stefxGetSnapshotLogBudget = 80;
	static int		s_stefxLastPresenceSnapshot = -1;
	static int		s_stefxLastPlayerMask = -1;

	memset( stefxPlayerOrigins, 0, sizeof( stefxPlayerOrigins ) );

	if ( snapshotNumber > cl.frame.messageNum ) {
		Com_Error( ERR_DROP, "CL_STEFX_GetSnapshot: snapshotNumber > cl.frame.messageNum" );
	}

	if ( cl.frame.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	clSnap = &cl.frames[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	snapshot = (stefxSnapshot_t *)snapshotBuffer;
	memset( snapshot, 0, sizeof( *snapshot ) );

	if ( !s_stefxLayoutLogged )
	{
		XBLog_WriteCriticalf("STEFX: engine EF snapshot adapter layout snapshot=%d ps=%d entity=%d numOfs=%d entitiesOfs=%d max=%d eventSlots=%d ammoSlots=%d",
			(int)sizeof(stefxSnapshot_t),
			(int)sizeof(stefxPlayerState_t),
			(int)sizeof(stefxEntityState_t),
			(int)((byte *)&(((stefxSnapshot_t *)0)->numEntities)),
			(int)((byte *)&(((stefxSnapshot_t *)0)->entities)),
			STEFX_MAX_ENTITIES_IN_SNAPSHOT,
			MAX_PS_EVENTS,
			MAX_AMMO);
		s_stefxLayoutLogged = 1;
	}

	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	STEFX_CopyJaPlayerStateToEf( &snapshot->ps, &clSnap->ps );

	count = clSnap->numEntities;
	if ( count > STEFX_MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_STEFX_GetSnapshot: truncated %i entities to %i\n", count, STEFX_MAX_ENTITIES_IN_SNAPSHOT );
		count = STEFX_MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;

	if ( s_stefxGetSnapshotLogBudget > 0 )
	{
		const int stefxWpPhaser = 1;
		const int stefxWpCompression = 2;
		const int stefxWpVoyagerHypo = 10;
		XBLF("STEFX: engine EF CL_GetSnapshot num=%d clMsg=%d valid=%d clSnapEntities=%d copyCount=%d parseBase=%d parseNow=%d psWeapon=%d psHealth=%d weaponsServer=0x%x weaponsEF=0x%x phaserServer=%d phaserEF=%d compressionServer=%d compressionEF=%d hypoServer=%d hypoEF=%d psRechargeServer=%d psRechargeEF=%d eventSeq=%d events=(%d,%d,%d,%d)",
			snapshotNumber,
			cl.frame.messageNum,
			(int)clSnap->valid,
			clSnap->numEntities,
			count,
			clSnap->parseEntitiesNum,
			cl.parseEntitiesNum,
			snapshot->ps.weapon,
			snapshot->ps.stats[STAT_HEALTH],
			clSnap->ps.stats[STAT_WEAPONS],
			snapshot->ps.stats[STAT_WEAPONS],
			clSnap->ps.ammo[stefxWpPhaser],
			snapshot->ps.ammo[stefxWpPhaser],
			clSnap->ps.ammo[stefxWpCompression],
			snapshot->ps.ammo[stefxWpCompression],
			clSnap->ps.ammo[stefxWpVoyagerHypo],
			snapshot->ps.ammo[stefxWpVoyagerHypo],
			clSnap->ps.rechargeTime,
			snapshot->ps.rechargeTime,
			snapshot->ps.eventSequence,
			snapshot->ps.events[0], snapshot->ps.events[1],
			snapshot->ps.events[2], snapshot->ps.events[3]);
		--s_stefxGetSnapshotLogBudget;
	}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HM_GAMEPLAY_DIAGNOSTICS)
	{
		const int stefxWpPhaser = 1;
		const int stefxWpCompression = 2;
		int ammoChecksum = 0;
		int ammoIndex;
		int currentAmmo = -1;
		static int s_stefxLastSnapshotWeapon = -999;
		static int s_stefxLastSnapshotWeapons = -1;
		static int s_stefxLastSnapshotPhaserAmmo = -1;
		static int s_stefxLastSnapshotCompressionAmmo = -1;
		static int s_stefxLastSnapshotAmmoChecksum = -1;
		static int s_stefxSnapshotAmmoBudget = 96;
		for ( ammoIndex = 0; ammoIndex < STEFX_PS_MAX_AMMO; ++ammoIndex )
		{
			ammoChecksum = ( ammoChecksum * 33 ) ^ snapshot->ps.ammo[ammoIndex];
		}
		if ( snapshot->ps.weapon >= 0 && snapshot->ps.weapon < STEFX_PS_MAX_AMMO )
		{
			currentAmmo = snapshot->ps.ammo[snapshot->ps.weapon];
		}
		if ( s_stefxSnapshotAmmoBudget > 0 &&
			( snapshot->ps.weapon != s_stefxLastSnapshotWeapon ||
			  snapshot->ps.stats[STAT_WEAPONS] != s_stefxLastSnapshotWeapons ||
			  snapshot->ps.ammo[stefxWpPhaser] != s_stefxLastSnapshotPhaserAmmo ||
			  snapshot->ps.ammo[stefxWpCompression] != s_stefxLastSnapshotCompressionAmmo ||
			  ammoChecksum != s_stefxLastSnapshotAmmoChecksum ) )
		{
			XBLog_WriteCriticalf("STEFX_HM_AMMO: snapshot-change num=%d time=%d psWeapon=%d weapons=0x%x currentAmmo=%d phaser=%d compression=%d imod=%d scav=%d stasis=%d grenade=%d tetrion=%d quantum=%d dread=%d hypo=%d checksum=0x%x health=%d weaponstate=%d",
				snapshotNumber,
				snapshot->serverTime,
				snapshot->ps.weapon,
				snapshot->ps.stats[STAT_WEAPONS],
				currentAmmo,
				snapshot->ps.ammo[stefxWpPhaser],
				snapshot->ps.ammo[stefxWpCompression],
				snapshot->ps.ammo[3],
				snapshot->ps.ammo[4],
				snapshot->ps.ammo[5],
				snapshot->ps.ammo[6],
				snapshot->ps.ammo[7],
				snapshot->ps.ammo[8],
				snapshot->ps.ammo[9],
				snapshot->ps.ammo[10],
				ammoChecksum,
				snapshot->ps.stats[STAT_HEALTH],
				snapshot->ps.weaponstate);
			s_stefxLastSnapshotWeapon = snapshot->ps.weapon;
			s_stefxLastSnapshotWeapons = snapshot->ps.stats[STAT_WEAPONS];
			s_stefxLastSnapshotPhaserAmmo = snapshot->ps.ammo[stefxWpPhaser];
			s_stefxLastSnapshotCompressionAmmo = snapshot->ps.ammo[stefxWpCompression];
			s_stefxLastSnapshotAmmoChecksum = ammoChecksum;
			--s_stefxSnapshotAmmoBudget;
		}
	}
#endif

	for ( i = 0 ; i < count ; i++ )
	{
		int entNum = ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1);
		const entityState_t *sourceState = &cl.parseEntities[ entNum ];
		STEFX_CopyJaEntityStateToEf( &snapshot->entities[i], sourceState );
		if ( snapshot->entities[i].eType == ET_PLAYER )
		{
			int playerNumber = snapshot->entities[i].number;
			if ( playerNumber >= 0 && playerNumber < 3 )
			{
				stefxPlayerMask |= 1 << playerNumber;
				stefxPlayerFlags[playerNumber] = snapshot->entities[i].eFlags;
				VectorCopy( snapshot->entities[i].pos.trBase, stefxPlayerOrigins[playerNumber] );
			}
			static int s_stefxGetSnapshotActorBudget = 256;
			if ( s_stefxGetSnapshotActorBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot actor ent=%d clientNum=%d eType=%d eFlags=0x%x weapon=%d model=%d model2=%d outIndex=%d parseIndex=%d origin=(%g,%g,%g) current=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].clientNum,
					snapshot->entities[i].eType,
					snapshot->entities[i].eFlags,
					snapshot->entities[i].weapon,
					snapshot->entities[i].modelindex,
					snapshot->entities[i].modelindex2,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].origin[0],
					snapshot->entities[i].origin[1],
					snapshot->entities[i].origin[2]);
				--s_stefxGetSnapshotActorBudget;
			}
		}
		if ( CL_STEFX_IsVisibleBrushMoverEntity( sourceState ) )
		{
			static int s_stefxGetSnapshotBModelBudget = 160;
			if ( s_stefxGetSnapshotBModelBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot bmodel ent=%d eType=%d model=%d model2=%d solid=0x%x eFlags=0x%x outIndex=%d parseIndex=%d pos=(%g,%g,%g) apos=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].eType,
					snapshot->entities[i].modelindex,
					snapshot->entities[i].modelindex2,
					snapshot->entities[i].solid,
					snapshot->entities[i].eFlags,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].apos.trBase[0],
					snapshot->entities[i].apos.trBase[1],
					snapshot->entities[i].apos.trBase[2]);
				--s_stefxGetSnapshotBModelBudget;
			}
		}
		if ( snapshot->entities[i].eType > ET_EVENTS )
		{
			static int s_stefxGetSnapshotEventBudget = 128;
			if ( s_stefxGetSnapshotEventBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot event ent=%d eType=%d event=%d weapon=%d outIndex=%d parseIndex=%d origin=(%g,%g,%g) origin2=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].eType,
					snapshot->entities[i].eType - ET_EVENTS,
					snapshot->entities[i].weapon,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].origin2[0],
					snapshot->entities[i].origin2[1],
					snapshot->entities[i].origin2[2]);
				--s_stefxGetSnapshotEventBudget;
			}
		}
	}

	if ( snapshotNumber != s_stefxLastPresenceSnapshot )
	{
		if ( stefxPlayerMask != s_stefxLastPlayerMask || ( snapshotNumber % 120 ) == 0 )
		{
			XBLog_WriteCriticalf("STEFX_VIS: snapshot=%d serverTime=%d entities=%d playerMask=0x%x "
				"local=(%g,%g,%g) bot1Flags=0x%x bot1=(%g,%g,%g) bot2Flags=0x%x bot2=(%g,%g,%g)",
				snapshotNumber,
				snapshot->serverTime,
				snapshot->numEntities,
				stefxPlayerMask,
				stefxPlayerOrigins[0][0], stefxPlayerOrigins[0][1], stefxPlayerOrigins[0][2],
				stefxPlayerFlags[1],
				stefxPlayerOrigins[1][0], stefxPlayerOrigins[1][1], stefxPlayerOrigins[1][2],
				stefxPlayerFlags[2],
				stefxPlayerOrigins[2][0], stefxPlayerOrigins[2][1], stefxPlayerOrigins[2][2]);
		}
		s_stefxLastPresenceSnapshot = snapshotNumber;
		s_stefxLastPlayerMask = stefxPlayerMask;
	}

	return qtrue;
}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
qboolean CL_STEFX_GetSnapshot( int snapshotNumber, void *snapshotBuffer ) {
	clSnapshot_t	*clSnap;
	stefxSnapshot_t	*snapshot;
	int				i, count;
	static int		s_stefxLayoutLogged = 0;
	static int		s_stefxGetSnapshotLogBudget = 80;

	if ( snapshotNumber > cl.frame.messageNum ) {
		Com_Error( ERR_DROP, "CL_STEFX_GetSnapshot: snapshotNumber > cl.frame.messageNum" );
	}

	if ( cl.frame.messageNum - snapshotNumber >= PACKET_BACKUP ) {
		return qfalse;
	}

	clSnap = &cl.frames[snapshotNumber & PACKET_MASK];
	if ( !clSnap->valid ) {
		return qfalse;
	}

	if ( cl.parseEntitiesNum - clSnap->parseEntitiesNum >= MAX_PARSE_ENTITIES ) {
		return qfalse;
	}

	snapshot = (stefxSnapshot_t *)snapshotBuffer;
	memset( snapshot, 0, sizeof( *snapshot ) );

	if ( !s_stefxLayoutLogged )
	{
		XBLF("STEFX: engine EF snapshot adapter layout snapshot=%d ps=%d entity=%d numOfs=%d entitiesOfs=%d max=%d",
			(int)sizeof(stefxSnapshot_t),
			(int)sizeof(stefxPlayerState_t),
			(int)sizeof(entityState_t),
			(int)((byte *)&(((stefxSnapshot_t *)0)->numEntities)),
			(int)((byte *)&(((stefxSnapshot_t *)0)->entities)),
			STEFX_MAX_ENTITIES_IN_SNAPSHOT);
		s_stefxLayoutLogged = 1;
	}

	snapshot->snapFlags = clSnap->snapFlags;
	snapshot->ping = clSnap->ping;
	snapshot->serverTime = clSnap->serverTime;
	memcpy( snapshot->areamask, clSnap->areamask, sizeof( snapshot->areamask ) );
	snapshot->cmdNum = clSnap->cmdNum;
	STEFX_CopyJaPlayerStateToEf( &snapshot->ps, &clSnap->ps );

	count = clSnap->numEntities;
	if ( count > STEFX_MAX_ENTITIES_IN_SNAPSHOT ) {
		Com_DPrintf( "CL_STEFX_GetSnapshot: truncated %i entities to %i\n", count, STEFX_MAX_ENTITIES_IN_SNAPSHOT );
		count = STEFX_MAX_ENTITIES_IN_SNAPSHOT;
	}
	snapshot->numEntities = count;
	snapshot->serverCommandSequence = clSnap->serverCommandNum;

	if ( s_stefxGetSnapshotLogBudget > 0 )
	{
		XBLF("STEFX: engine EF CL_GetSnapshot num=%d clMsg=%d valid=%d clSnapEntities=%d copyCount=%d parseBase=%d parseNow=%d psWeapon=%d psHealth=%d",
			snapshotNumber,
			cl.frame.messageNum,
			(int)clSnap->valid,
			clSnap->numEntities,
			count,
			clSnap->parseEntitiesNum,
			cl.parseEntitiesNum,
			snapshot->ps.weapon,
			snapshot->ps.stats[STAT_HEALTH]);
		--s_stefxGetSnapshotLogBudget;
	}

	for ( i = 0 ; i < count ; i++ )
	{
		int entNum = ( clSnap->parseEntitiesNum + i ) & (MAX_PARSE_ENTITIES-1);
		snapshot->entities[i] = cl.parseEntities[ entNum ];
		if ( snapshot->entities[i].eType == ET_PLAYER )
		{
			static int s_stefxGetSnapshotActorBudget = 256;
			if ( s_stefxGetSnapshotActorBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot actor ent=%d clientNum=%d eType=%d eFlags=0x%x weapon=%d model=%d model2=%d outIndex=%d parseIndex=%d origin=(%g,%g,%g) current=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].clientNum,
					snapshot->entities[i].eType,
					snapshot->entities[i].eFlags,
					snapshot->entities[i].weapon,
					snapshot->entities[i].modelindex,
					snapshot->entities[i].modelindex2,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].origin[0],
					snapshot->entities[i].origin[1],
					snapshot->entities[i].origin[2]);
				--s_stefxGetSnapshotActorBudget;
			}
		}
		if ( CL_STEFX_IsVisibleBrushMoverEntity( &snapshot->entities[i] ) )
		{
			static int s_stefxGetSnapshotBModelBudget = 160;
			if ( s_stefxGetSnapshotBModelBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot bmodel ent=%d eType=%d model=%d model2=%d solid=0x%x eFlags=0x%x outIndex=%d parseIndex=%d pos=(%g,%g,%g) apos=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].eType,
					snapshot->entities[i].modelindex,
					snapshot->entities[i].modelindex2,
					snapshot->entities[i].solid,
					snapshot->entities[i].eFlags,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].apos.trBase[0],
					snapshot->entities[i].apos.trBase[1],
					snapshot->entities[i].apos.trBase[2]);
				--s_stefxGetSnapshotBModelBudget;
			}
		}
		if ( snapshot->entities[i].eType > ET_EVENTS )
		{
			static int s_stefxGetSnapshotEventBudget = 128;
			if ( s_stefxGetSnapshotEventBudget > 0 )
			{
				XBLF("STEFX: engine EF CL_GetSnapshot event ent=%d eType=%d event=%d weapon=%d outIndex=%d parseIndex=%d origin=(%g,%g,%g) origin2=(%g,%g,%g)",
					snapshot->entities[i].number,
					snapshot->entities[i].eType,
					snapshot->entities[i].eType - ET_EVENTS,
					snapshot->entities[i].weapon,
					i,
					entNum,
					snapshot->entities[i].pos.trBase[0],
					snapshot->entities[i].pos.trBase[1],
					snapshot->entities[i].pos.trBase[2],
					snapshot->entities[i].origin2[0],
					snapshot->entities[i].origin2[1],
					snapshot->entities[i].origin2[2]);
				--s_stefxGetSnapshotEventBudget;
			}
		}
	}

	return qtrue;
}
#endif

//bg_public.h won't cooperate in here
#define EF_PERMANENT   0x00080000

qboolean CL_GetDefaultState(int index, entityState_t *state)
{
	if (index < 0 || index >= MAX_GENTITIES)
	{
		return qfalse;
	}

	// Is this safe? I think so. But it's still ugly as sin.
	if (!(sv.svEntities[index].baseline.eFlags & EF_PERMANENT))
//	if (!(cl.entityBaselines[index].eFlags & EF_PERMANENT))
	{
		return qfalse;
	}

	*state = sv.svEntities[index].baseline;
//	*state = cl.entityBaselines[index];

	return qtrue;
}

extern float cl_mPitchOverride;
extern float cl_mYawOverride;
void CL_SetUserCmdValue( int userCmdValue, float sensitivityScale, float mPitchOverride, float mYawOverride ) {
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HM_GAMEPLAY_DIAGNOSTICS)
	static int s_stefxLastUserCmdValue = -999;
	static int s_stefxUserCmdValueBudget = 96;
	if ( userCmdValue != s_stefxLastUserCmdValue && s_stefxUserCmdValueBudget > 0 )
	{
		XBLog_WriteCriticalf("STEFX_HM_AMMO: engine-set-usercmd weapon=%d prev=%d sens=%g pitch=%g yaw=%g",
			userCmdValue,
			s_stefxLastUserCmdValue,
			sensitivityScale,
			mPitchOverride,
			mYawOverride);
		s_stefxLastUserCmdValue = userCmdValue;
		if ( s_stefxUserCmdValueBudget > 0 )
		{
			--s_stefxUserCmdValueBudget;
		}
	}
#endif
	cl.cgameUserCmdValue = userCmdValue;
	cl.cgameSensitivity = sensitivityScale;
	cl_mPitchOverride = mPitchOverride;
	cl_mYawOverride = mYawOverride;
}

extern vec3_t cl_overriddenAngles;
extern qboolean cl_overrideAngles;
void CL_SetUserCmdAngles( float pitchOverride, float yawOverride, float rollOverride ) {
	cl_overriddenAngles[PITCH] = pitchOverride;
	cl_overriddenAngles[YAW] = yawOverride;
	cl_overriddenAngles[ROLL] = rollOverride;
	cl_overrideAngles = qtrue;
}

void CL_AddCgameCommand( const char *cmdName ) {
	Cmd_AddCommand( cmdName, NULL );
}

void CL_CgameError( const char *string ) {
	Com_Error( ERR_DROP, "%s", string );
}


/*
=====================
CL_ConfigstringModified
=====================
*/
void CL_ConfigstringModified( void ) {
	char		*old, *s;
	int			i, index;
	char		*dup;
	gameState_t	oldGs;
	int			len;

	index = atoi( Cmd_Argv(1) );
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
	}
	s = Cmd_Argv(2);

	old = cl.gameState.stringData + cl.gameState.stringOffsets[ index ];
	if ( !strcmp( old, s ) ) {
		return;		// unchanged
	}

	// build the new gameState_t
	oldGs = cl.gameState;

	memset( &cl.gameState, 0, sizeof( cl.gameState ) );

	// leave the first 0 for uninitialized strings
	cl.gameState.dataCount = 1;
		
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( i == index ) {
			dup = s;
		} else {
			dup = oldGs.stringData + oldGs.stringOffsets[ i ];
		}
		if ( !dup[0] ) {
			continue;		// leave with the default empty string
		}

		len = strlen( dup );

		if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
			Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
		}

		// append it to the gameState string buffer
		cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
		memcpy( cl.gameState.stringData + cl.gameState.dataCount, dup, len + 1 );
		cl.gameState.dataCount += len + 1;
	}

	if ( index == CS_SYSTEMINFO ) {
		// parse serverId and other cvars
		CL_SystemInfoChanged();
	}

}


/*
===================
CL_GetServerCommand

Set up argc/argv for the given command
===================
*/
qboolean CL_GetServerCommand( int serverCommandNumber ) {
	char	*s;
	char	*cmd;

	// if we have irretrievably lost a reliable command, drop the connection
	if ( serverCommandNumber <= clc.serverCommandSequence - MAX_RELIABLE_COMMANDS ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: a reliable command was cycled out" );
		return qfalse;
	}

	if ( serverCommandNumber > clc.serverCommandSequence ) {
		Com_Error( ERR_DROP, "CL_GetServerCommand: requested a command not received" );
		return qfalse;
	}

	s = clc.serverCommands[ serverCommandNumber & ( MAX_RELIABLE_COMMANDS - 1 ) ];

	Com_DPrintf( "serverCommand: %i : %s\n", serverCommandNumber, s );

	Cmd_TokenizeString( s );
	cmd = Cmd_Argv(0);

	if ( !strcmp( cmd, "disconnect" ) ) {
		Com_Error (ERR_DISCONNECT,"Server disconnected\n");
	}

	if ( !strcmp( cmd, "cs" ) ) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString( s );
		return qtrue;
	}

	// the clientLevelShot command is used during development
	// to generate 128*128 screenshots from the intermission
	// point of levels for the menu system to use
	// we pass it along to the cgame to make apropriate adjustments,
	// but we also clear the console and notify lines here
	if ( !strcmp( cmd, "clientLevelShot" ) ) {
		// don't do it if we aren't running the server locally,
		// otherwise malicious remote servers could overwrite
		// the existing thumbnails
		if ( !com_sv_running->integer ) {
			return qfalse;
		}
		// close the console
		Con_Close();
		// take a special screenshot next frame
		Cbuf_AddText( "wait ; wait ; wait ; wait ; screenshot levelshot\n" );
		return qtrue;
	}

	// we may want to put a "connect to other server" command here

	// cgame can now act on the command
	return qtrue;
}


/*
====================
CL_CM_LoadMap

Just adds default parameters that cgame doesn't need to know about
====================
*/
#ifdef _XBOX
void CL_CM_LoadMap( const char *mapname ) {
	int		checksum;

	CM_LoadMap( mapname, qtrue, &checksum );
}
#else
void CL_CM_LoadMap( const char *mapname, qboolean subBSP ) {
	int		checksum;

	CM_LoadMap( mapname, qtrue, &checksum, subBSP );
}
#endif // _XBOX

/*
====================
CL_ShutdonwCGame

====================
*/
void CL_ShutdownCGame( void ) {
	cls.cgameStarted = qfalse;

	if ( !cgvm.entryPoint) {
		return;
	}
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	XBLF("STEFX_HM_SP: CL_ShutdownCGame before VM_Call state=%d", (int)cls.state);
#endif
	VM_Call( CG_SHUTDOWN );
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	XBLF("STEFX_HM_SP: CL_ShutdownCGame after VM_Call state=%d", (int)cls.state);
#endif
#ifndef _XBOX	// Not using it
	RM_ShutdownTerrain();
#endif

//	VM_Free( cgvm );
//	cgvm = NULL;
}

//RMG
CCMLandScape *CM_RegisterTerrain(const char *config, bool server);
void RE_InitRendererTerrain( const char *info );
//RMG

extern float tr_distortionAlpha; //tr_shadows.cpp
extern float tr_distortionStretch; //tr_shadows.cpp
extern qboolean tr_distortionPrePost; //tr_shadows.cpp
extern qboolean tr_distortionNegate; //tr_shadows.cpp

float g_oldRangedFog = 0.0f;
/*
====================
CL_CgameSystemCalls

The cgame module is making a system call
====================
*/
void *VM_ArgPtr( int intValue );
void CM_SnapPVS(vec3_t origin,byte *buffer);
#ifdef _XBOX
extern bool Sys_IsDirectMapBoot(void);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static void CL_STEFX_DrawDirectMapLoadScreen( const char *source )
{
	static int s_directMapLoadScreenBudget = 16;
#if defined(STEFX_SP_HOSTED_MP)
	extern void SP_DrawMPLoadScreen( void );
#else
	extern void SP_DrawSPLoadScreen( void );
#endif

	if ( s_directMapLoadScreenBudget > 0 )
	{
		XBLF("STEFX: %s presenting EF loadscreen during direct-map boot state=%d realtime=%d",
			source ? source : "CG_UPDATESCREEN", (int)cls.state, cls.realtime);
		--s_directMapLoadScreenBudget;
	}

#if defined(STEFX_SP_HOSTED_MP)
	XBLog_Write("STEFX: direct-map loadscreen before SP_DrawMPLoadScreen");
	SP_DrawMPLoadScreen();
	XBLog_Write("STEFX: direct-map loadscreen after SP_DrawMPLoadScreen before EndFrame");
#else
	XBLog_Write("STEFX: direct-map loadscreen before SP_DrawSPLoadScreen");
	SP_DrawSPLoadScreen();
	XBLog_Write("STEFX: direct-map loadscreen after SP_DrawSPLoadScreen before EndFrame");
#endif
	re.EndFrame( NULL, NULL );
	XBLog_Write("STEFX: direct-map loadscreen EndFrame done");
}
#endif
//#define	VMA(x) VM_ArgPtr(args[x])
#define	VMA(x) ((void*)args[x])
#define	VMF(x)	((float *)args)[x]

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#if defined(STEFX_SP_HOSTED_MP)
enum stefxCgImport_t
{
	STEFX_CG_PRINT = 0,
	STEFX_CG_ERROR = 1,
	STEFX_CG_MILLISECONDS = 2,
	STEFX_CG_CVAR_REGISTER = 3,
	STEFX_CG_CVAR_UPDATE = 4,
	STEFX_CG_CVAR_SET = 5,
	STEFX_CG_CVAR_VARIABLESTRINGBUFFER = 6,
	STEFX_CG_ARGC = 7,
	STEFX_CG_ARGV = 8,
	STEFX_CG_ARGS = 9,
	STEFX_CG_FS_FOPENFILE = 10,
	STEFX_CG_FS_READ = 11,
	STEFX_CG_FS_WRITE = 12,
	STEFX_CG_FS_FCLOSEFILE = 13,
	STEFX_CG_SENDCONSOLECOMMAND = 14,
	STEFX_CG_ADDCOMMAND = 15,
	STEFX_CG_SENDCLIENTCOMMAND = 16,
	STEFX_CG_UPDATESCREEN = 17,
	STEFX_CG_CM_LOADMAP = 18,
	STEFX_CG_CM_NUMINLINEMODELS = 19,
	STEFX_CG_CM_INLINEMODEL = 20,
	STEFX_CG_CM_LOADMODEL = 21,
	STEFX_CG_CM_TEMPBOXMODEL = 22,
	STEFX_CG_CM_POINTCONTENTS = 23,
	STEFX_CG_CM_TRANSFORMEDPOINTCONTENTS = 24,
	STEFX_CG_CM_BOXTRACE = 25,
	STEFX_CG_CM_TRANSFORMEDBOXTRACE = 26,
	STEFX_CG_CM_MARKFRAGMENTS = 27,
	STEFX_CG_S_STARTSOUND = 28,
	STEFX_CG_S_STARTLOCALSOUND = 29,
	STEFX_CG_S_CLEARLOOPINGSOUNDS = 30,
	STEFX_CG_S_ADDLOOPINGSOUND = 31,
	STEFX_CG_S_UPDATEENTITYPOSITION = 32,
	STEFX_CG_S_RESPATIALIZE = 33,
	STEFX_CG_S_REGISTERSOUND = 34,
	STEFX_CG_S_STARTBACKGROUNDTRACK = 35,
	STEFX_CG_R_LOADWORLDMAP = 36,
	STEFX_CG_R_REGISTERMODEL = 37,
	STEFX_CG_R_REGISTERSKIN = 38,
	STEFX_CG_R_REGISTERSHADER = 39,
	STEFX_CG_R_CLEARSCENE = 40,
	STEFX_CG_R_ADDREFENTITYTOSCENE = 41,
	STEFX_CG_R_ADDPOLYTOSCENE = 42,
	STEFX_CG_R_ADDLIGHTTOSCENE = 43,
	STEFX_CG_R_RENDERSCENE = 44,
	STEFX_CG_R_SETCOLOR = 45,
	STEFX_CG_R_DRAWSTRETCHPIC = 46,
	STEFX_CG_R_MODELBOUNDS = 47,
	STEFX_CG_R_LERPTAG = 48,
	STEFX_CG_GETGLCONFIG = 49,
	STEFX_CG_GETGAMESTATE = 50,
	STEFX_CG_GETCURRENTSNAPSHOTNUMBER = 51,
	STEFX_CG_GETSNAPSHOT = 52,
	STEFX_CG_GETSERVERCOMMAND = 53,
	STEFX_CG_GETCURRENTCMDNUMBER = 54,
	STEFX_CG_GETUSERCMD = 55,
	STEFX_CG_SETUSERCMDVALUE = 56,
	STEFX_CG_R_REGISTERSHADERNOMIP = 57,
	STEFX_CG_MEMORY_REMAINING = 58,
	STEFX_CG_R_REGISTERSHADER3D = 59,
	STEFX_CG_CVAR_SET_NO_MODIFY = 60
};
#else
enum stefxCgImport_t
{
	STEFX_CG_PRINT,
	STEFX_CG_ERROR,
	STEFX_CG_MILLISECONDS,
	STEFX_CG_CVAR_REGISTER,
	STEFX_CG_CVAR_UPDATE,
	STEFX_CG_CVAR_SET,
	STEFX_CG_ARGC,
	STEFX_CG_ARGV,
	STEFX_CG_ARGS,
	STEFX_CG_FS_FOPENFILE,
	STEFX_CG_FS_READ,
	STEFX_CG_FS_WRITE,
	STEFX_CG_FS_FCLOSEFILE,
	STEFX_CG_SENDCONSOLECOMMAND,
	STEFX_CG_ADDCOMMAND,
	STEFX_CG_SENDCLIENTCOMMAND,
	STEFX_CG_UPDATESCREEN,
	STEFX_CG_CM_LOADMAP,
	STEFX_CG_CM_NUMINLINEMODELS,
	STEFX_CG_CM_INLINEMODEL,
	STEFX_CG_CM_TEMPBOXMODEL,
	STEFX_CG_CM_POINTCONTENTS,
	STEFX_CG_CM_TRANSFORMEDPOINTCONTENTS,
	STEFX_CG_CM_BOXTRACE,
	STEFX_CG_CM_TRANSFORMEDBOXTRACE,
	STEFX_CG_CM_MARKFRAGMENTS,
	STEFX_CG_S_STARTSOUND,
	STEFX_CG_S_STARTLOCALSOUND,
	STEFX_CG_S_CLEARLOOPINGSOUNDS,
	STEFX_CG_S_ADDLOOPINGSOUND,
	STEFX_CG_S_UPDATEENTITYPOSITION,
	STEFX_CG_S_RESPATIALIZE,
	STEFX_CG_S_REGISTERSOUND,
	STEFX_CG_S_STARTBACKGROUNDTRACK,
	STEFX_CG_FF_STARTFX,
	STEFX_CG_FF_ENSUREFX,
	STEFX_CG_FF_STOPFX,
	STEFX_CG_FF_STOPALLFX,
	STEFX_CG_R_LOADWORLDMAP,
	STEFX_CG_R_REGISTERMODEL,
	STEFX_CG_R_REGISTERSKIN,
	STEFX_CG_R_REGISTERSHADER,
	STEFX_CG_R_REGISTERSHADERNOMIP,
	STEFX_CG_R_CLEARSCENE,
	STEFX_CG_R_ADDREFENTITYTOSCENE,
	STEFX_CG_R_GETLIGHTING,
	STEFX_CG_R_ADDPOLYTOSCENE,
	STEFX_CG_R_ADDLIGHTTOSCENE,
	STEFX_CG_R_RENDERSCENE,
	STEFX_CG_R_SETCOLOR,
	STEFX_CG_R_DRAWSTRETCHPIC,
	STEFX_CG_R_DRAWSCREENSHOT,
	STEFX_CG_R_MODELBOUNDS,
	STEFX_CG_R_LERPTAG,
	STEFX_CG_R_DRAWROTATEPIC,
	STEFX_CG_R_SCISSOR,
	STEFX_CG_GETGLCONFIG,
	STEFX_CG_GETGAMESTATE,
	STEFX_CG_GETCURRENTSNAPSHOTNUMBER,
	STEFX_CG_GETSNAPSHOT,
	STEFX_CG_GETSERVERCOMMAND,
	STEFX_CG_GETCURRENTCMDNUMBER,
	STEFX_CG_GETUSERCMD,
	STEFX_CG_SETUSERCMDVALUE,
	STEFX_CG_MEMORY_REMAINING,
	STEFX_CG_S_UPDATEAMBIENTSET,
	STEFX_CG_S_ADDLOCALSET,
	STEFX_CG_AS_PARSESETS,
	STEFX_CG_AS_ADDENTRY,
	STEFX_CG_AS_GETBMODELSOUND,
	STEFX_CG_S_GETSAMPLELENGTH
};
#endif

typedef struct stefxRefdef_s {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];
	int			time;
	int			rdflags;
	byte		areamask[MAX_MAP_AREA_BYTES];
} stefxRefdef_t;

#if defined(STEFX_SP_HOSTED_MP)
enum stefxEfRefEntityType_t
{
	STEFX_EF_RT_MODEL = 0,
	STEFX_EF_RT_SPRITE,
	STEFX_EF_RT_ORIENTEDSPRITE,
	STEFX_EF_RT_ALPHAVERTPOLY,
	STEFX_EF_RT_BEAM,
	STEFX_EF_RT_RAIL_CORE,
	STEFX_EF_RT_RAIL_RINGS,
	STEFX_EF_RT_LIGHTNING,
	STEFX_EF_RT_PORTALSURFACE,
	STEFX_EF_RT_LINE,
	STEFX_EF_RT_ORIENTEDLINE,
	STEFX_EF_RT_LINE2,
	STEFX_EF_RT_BEZIER,
	STEFX_EF_RT_CYLINDER,
	STEFX_EF_RT_ELECTRICITY,
	STEFX_EF_RT_MAX
};

typedef struct stefxEfRefEntity_s
{
	int			reType;
	int			renderfx;
	qhandle_t	hModel;
	vec3_t		lightingOrigin;
	float		shadowPlane;
	vec3_t		axis[3];
	qboolean	nonNormalizedAxes;
	vec3_t		origin;
	int			frame;
	vec3_t		oldorigin;
	int			oldframe;
	float		backlerp;
	int			skinNum;
	qhandle_t	customSkin;
	qhandle_t	customShader;
	byte		shaderRGBA[4];
	float		shaderTexCoord[2];
	float		shaderTime;
	union
	{
		struct
		{
			float rotation;
			float radius;
			byte vertRGBA[4][4];
		} sprite;
		struct
		{
			float width;
			float width2;
			float stscale;
		} line;
		struct
		{
			float width;
			vec3_t control1;
			vec3_t control2;
		} bezier;
		struct
		{
			float width;
			float width2;
			float stscale;
			float height;
			float bias;
			qboolean wrap;
		} cylinder;
		struct
		{
			float width;
			float deviation;
			float stscale;
			qboolean wrap;
			qboolean taper;
		} electricity;
	} data;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	int			number;
#endif
} stefxEfRefEntity_t;

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static qhandle_t s_stefxTrackedLowerModel;
static qhandle_t s_stefxTrackedUpperModel;
static qhandle_t s_stefxTrackedHeadModel;
static unsigned int s_stefxTrackedRefCount;
extern "C"
{
	volatile unsigned int g_SPXBHMSplitLowerModel = 0;
	volatile unsigned int g_SPXBHMSplitUpperModel = 0;
	volatile unsigned int g_SPXBHMSplitHeadModel = 0;
	extern volatile unsigned int g_SPXBHMSplitPhaserBridgeFP[4];
	extern volatile unsigned int g_SPXBHMSplitPhaserBridgeWorld[4];
	extern volatile unsigned int g_SPXBHMSplitPhaserBridgeLineFP[4];
	extern volatile unsigned int g_SPXBHMSplitPhaserBridgeLastNumber[4];
}
#endif

static int CL_STEFX_TranslateRenderFx( int efRenderFx )
{
	int renderFx = 0;

	if ( efRenderFx & 0x0002 ) renderFx |= RF_THIRD_PERSON;
	if ( efRenderFx & 0x0004 ) renderFx |= RF_FIRST_PERSON;
	if ( efRenderFx & 0x0008 ) renderFx |= RF_DEPTHHACK;
	if ( efRenderFx & 0x0010 ) renderFx |= RF_STEFX_FULLBRIGHT;
	if ( efRenderFx & 0x0040 ) renderFx |= RF_NOSHADOW;
	if ( efRenderFx & 0x0080 ) renderFx |= RF_LIGHTING_ORIGIN;
	if ( efRenderFx & 0x0100 ) renderFx |= RF_SHADOW_PLANE;
	if ( efRenderFx & 0x0200 ) renderFx |= RF_WRAP_FRAMES;
	if ( efRenderFx & 0x0400 ) renderFx |= RF_CAP_FRAMES;
	if ( efRenderFx & 0x0800 ) renderFx |= RF_STEFX_FORCE_ENT_ALPHA;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	if ( efRenderFx & RF_STEFX_SPLIT_SLOT0 ) renderFx |= RF_STEFX_SPLIT_SLOT0;
	if ( efRenderFx & RF_STEFX_SPLIT_SLOT1 ) renderFx |= RF_STEFX_SPLIT_SLOT1;
	if ( efRenderFx & RF_STEFX_SPLIT_SLOT2 ) renderFx |= RF_STEFX_SPLIT_SLOT2;
#endif

	return renderFx;
}

static qboolean CL_STEFX_CopyRefEntity( refEntity_t *out, const stefxEfRefEntity_t *in )
{
	if ( !out || !in || in->reType < STEFX_EF_RT_MODEL || in->reType >= STEFX_EF_RT_MAX )
	{
		return qfalse;
	}

	memset( out, 0, sizeof( *out ) );
	switch ( in->reType )
	{
	case STEFX_EF_RT_MODEL:
		out->reType = RT_MODEL;
		break;
	case STEFX_EF_RT_SPRITE:
		out->reType = RT_SPRITE;
		break;
	case STEFX_EF_RT_ORIENTEDSPRITE:
		out->reType = RT_EF_ORIENTED_SPRITE;
		break;
	case STEFX_EF_RT_ALPHAVERTPOLY:
		out->reType = RT_EF_ALPHA_VERT_POLY;
		break;
	case STEFX_EF_RT_BEAM:
		out->reType = RT_BEAM;
		break;
	case STEFX_EF_RT_RAIL_CORE:
	case STEFX_EF_RT_RAIL_RINGS:
		out->reType = RT_LINE;
		break;
	case STEFX_EF_RT_LINE:
		out->reType = RT_TEXTURED_LINE;
		break;
	case STEFX_EF_RT_ORIENTEDLINE:
		out->reType = RT_ORIENTED_LINE;
		break;
	case STEFX_EF_RT_LINE2:
		out->reType = RT_TAPERED_LINE;
		break;
	case STEFX_EF_RT_BEZIER:
		out->reType = RT_BEZIER;
		break;
	case STEFX_EF_RT_LIGHTNING:
		out->reType = RT_EF_LIGHTNING;
		break;
	case STEFX_EF_RT_ELECTRICITY:
		out->reType = RT_EF_ELECTRICITY;
		break;
	case STEFX_EF_RT_PORTALSURFACE:
		out->reType = RT_PORTALSURFACE;
		break;
	case STEFX_EF_RT_CYLINDER:
		out->reType = RT_EF_CYLINDER;
		break;
	default:
		return qfalse;
	}

	out->renderfx = CL_STEFX_TranslateRenderFx( in->renderfx );
	out->hModel = in->hModel;
	VectorCopy( in->lightingOrigin, out->lightingOrigin );
	out->shadowPlane = in->shadowPlane;
	AxisCopy( in->axis, out->axis );
	out->nonNormalizedAxes = in->nonNormalizedAxes;
	VectorCopy( in->origin, out->origin );
	out->frame = in->frame;
	VectorCopy( in->oldorigin, out->oldorigin );
	out->oldframe = in->oldframe;
	out->backlerp = in->backlerp;
	out->skinNum = in->skinNum;
	out->customSkin = in->customSkin;
	out->customShader = in->customShader;
	memcpy( out->shaderRGBA, in->shaderRGBA, sizeof( out->shaderRGBA ) );
	memcpy( out->shaderTexCoord, in->shaderTexCoord, sizeof( out->shaderTexCoord ) );
	out->shaderTime = in->shaderTime;
	memcpy( &out->stefxData, &in->data, sizeof( out->stefxData ) );
	VectorSet( out->modelScale, 1.0f, 1.0f, 1.0f );
	out->ghoul2 = NULL;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	out->number = in->number;
#elif defined(_XBOX)
	out->number = 0;
#endif

	switch ( in->reType )
	{
	case STEFX_EF_RT_SPRITE:
	case STEFX_EF_RT_ORIENTEDSPRITE:
	case STEFX_EF_RT_ALPHAVERTPOLY:
		out->radius = in->data.sprite.radius;
		out->rotation = in->data.sprite.rotation;
		break;
	case STEFX_EF_RT_RAIL_CORE:
	case STEFX_EF_RT_RAIL_RINGS:
		out->radius = in->data.line.width > 0.0f ? in->data.line.width : 1.0f;
		break;
	case STEFX_EF_RT_LINE:
	case STEFX_EF_RT_ORIENTEDLINE:
		out->radius = in->data.line.width > 0.0f ? in->data.line.width : 1.0f;
		out->rotation = in->data.line.stscale;
		break;
	case STEFX_EF_RT_LINE2:
		out->radius = in->data.line.width > 0.0f ? in->data.line.width : 1.0f;
		out->backlerp = in->data.line.width2;
		out->rotation = in->data.line.stscale;
		break;
	case STEFX_EF_RT_BEZIER:
		out->radius = in->data.bezier.width > 0.0f ? in->data.bezier.width : 1.0f;
		VectorCopy( in->data.bezier.control1, out->axis[0] );
		VectorCopy( in->data.bezier.control2, out->axis[1] );
		break;
	case STEFX_EF_RT_LIGHTNING:
	case STEFX_EF_RT_ELECTRICITY:
		out->radius = in->data.electricity.width > 0.0f ? in->data.electricity.width : 1.0f;
		out->angles[0] = in->data.electricity.deviation;
		if ( in->data.electricity.taper ) out->renderfx |= RF_TAPERED;
		break;
	case STEFX_EF_RT_CYLINDER:
		out->radius = in->data.cylinder.width;
		out->backlerp = in->data.cylinder.width2;
		VectorMA( in->origin, in->data.cylinder.height, in->axis[0], out->oldorigin );
		break;
	default:
		break;
	}

	return qtrue;
}

static soundChannel_t CL_STEFX_TranslateOfficialSoundChannel( int channel )
{
	enum
	{
		STEFX_OFFICIAL_CHAN_AUTO = 0,
		STEFX_OFFICIAL_CHAN_LOCAL = 1,
		STEFX_OFFICIAL_CHAN_WEAPON = 2,
		STEFX_OFFICIAL_CHAN_VOICE = 3,
		STEFX_OFFICIAL_CHAN_ITEM = 4,
		STEFX_OFFICIAL_CHAN_BODY = 5,
		STEFX_OFFICIAL_CHAN_LOCAL_SOUND = 6,
		STEFX_OFFICIAL_CHAN_ANNOUNCER = 7,
		STEFX_OFFICIAL_CHAN_MENU1 = 8
	};

	switch ( channel )
	{
	case STEFX_OFFICIAL_CHAN_AUTO: return CHAN_AUTO;
	case STEFX_OFFICIAL_CHAN_LOCAL: return CHAN_LOCAL;
	case STEFX_OFFICIAL_CHAN_WEAPON: return CHAN_WEAPON;
	case STEFX_OFFICIAL_CHAN_VOICE: return CHAN_VOICE;
	case STEFX_OFFICIAL_CHAN_ITEM: return CHAN_ITEM;
	case STEFX_OFFICIAL_CHAN_BODY: return CHAN_BODY;
	case STEFX_OFFICIAL_CHAN_LOCAL_SOUND: return CHAN_LOCAL_SOUND;
	case STEFX_OFFICIAL_CHAN_ANNOUNCER: return CHAN_ANNOUNCER;
	case STEFX_OFFICIAL_CHAN_MENU1: return CHAN_LOCAL_SOUND;
	default: return CHAN_AUTO;
	}
}
#endif

static void CL_STEFX_CopyRefdef( refdef_t *out, const stefxRefdef_t *in )
{
	memset( out, 0, sizeof( *out ) );
	if ( !in )
	{
		return;
	}

	out->x = in->x;
	out->y = in->y;
	out->width = in->width;
	out->height = in->height;
	out->fov_x = in->fov_x;
	out->fov_y = in->fov_y;
	VectorCopy( in->vieworg, out->vieworg );
	VectorCopy( in->viewaxis[0], out->viewaxis[0] );
	VectorCopy( in->viewaxis[1], out->viewaxis[1] );
	VectorCopy( in->viewaxis[2], out->viewaxis[2] );
	out->viewContents = 0;
	out->time = in->time;
	out->rdflags = in->rdflags;
	memcpy( out->areamask, in->areamask, sizeof( out->areamask ) );
}

static clipHandle_t CL_SafeCgameInlineModel( const char *tag, int index )
{
	int count = CM_NumInlineModels();
	if ( index < 0 || index >= count )
	{
		XBLF("STEFX: %s CM_InlineModel rejected index=%d count=%d; returning world", tag, index, count);
		return 0;
	}

	return CM_InlineModel( index );
}

static int CL_STEFX_CgameSystemCalls( int *args )
{
	static int s_syscallLogCount = 0;
	if (s_syscallLogCount < 96)
	{
		XBLF("STEFX: EF cgame syscall #%d trap=%d a1=%08x a2=%08x a3=%08x",
			s_syscallLogCount, args[0], args[1], args[2], args[3]);
	}
	s_syscallLogCount++;

	switch ( args[0] )
	{
	case STEFX_CG_PRINT:
#if defined(STEFX_SP_HOSTED_MP)
		XBLog_WriteCritical( (const char *)VMA(1) );
#endif
		Com_Printf( "%s", VMA(1) );
		return 0;
	case STEFX_CG_ERROR:
		Com_Error( ERR_DROP, S_COLOR_RED"%s", VMA(1) );
		return 0;
	case STEFX_CG_MILLISECONDS:
		return Sys_Milliseconds();
	case STEFX_CG_CVAR_REGISTER:
		Cvar_Register( (vmCvar_t *) VMA(1), (const char *) VMA(2), (const char *) VMA(3), args[4] );
		return 0;
	case STEFX_CG_CVAR_UPDATE:
		Cvar_Update( (vmCvar_t *) VMA(1) );
		return 0;
	case STEFX_CG_CVAR_SET:
		Cvar_Set( (const char *) VMA(1), (const char *) VMA(2) );
		return 0;
#if defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_CVAR_VARIABLESTRINGBUFFER:
		Cvar_VariableStringBuffer( (const char *) VMA(1), (char *) VMA(2), args[3] );
		return 0;
#endif
	case STEFX_CG_ARGC:
		return Cmd_Argc();
	case STEFX_CG_ARGV:
		Cmd_ArgvBuffer( args[1], (char *) VMA(2), args[3] );
		return 0;
	case STEFX_CG_ARGS:
		Cmd_ArgsBuffer( (char *) VMA(1), args[2] );
		return 0;
	case STEFX_CG_FS_FOPENFILE:
#if defined(STEFX_SP_HOSTED_MP)
		{
			const char *path = (const char *) VMA(1);
			fileHandle_t *file = (fileHandle_t *) VMA(2);
			int length;
			XBLF("STEFX_HM_SP: cgame fs open enter path='%s' mode=%d", path ? path : "", args[3]);
			length = FS_FOpenFileByMode( path, file, (fsMode_t) args[3] );
			XBLF("STEFX_HM_SP: cgame fs open exit path='%s' len=%d handle=%d",
				path ? path : "", length, file ? *file : -1);
			return length;
		}
#else
		return FS_FOpenFileByMode( (const char *) VMA(1), (int *) VMA(2), (fsMode_t) args[3] );
#endif
	case STEFX_CG_FS_READ:
#if defined(STEFX_SP_HOSTED_MP)
		XBLF("STEFX_HM_SP: cgame fs read enter len=%d handle=%d", args[2], args[3]);
#endif
		FS_Read( VMA(1), args[2], args[3] );
#if defined(STEFX_SP_HOSTED_MP)
		XBLF("STEFX_HM_SP: cgame fs read exit handle=%d", args[3]);
#endif
		return 0;
	case STEFX_CG_FS_WRITE:
		FS_Write( VMA(1), args[2], args[3] );
		return 0;
	case STEFX_CG_FS_FCLOSEFILE:
#if defined(STEFX_SP_HOSTED_MP)
		XBLF("STEFX_HM_SP: cgame fs close enter handle=%d", args[1]);
#endif
		FS_FCloseFile( args[1] );
#if defined(STEFX_SP_HOSTED_MP)
		XBLF("STEFX_HM_SP: cgame fs close exit handle=%d", args[1]);
#endif
		return 0;
	case STEFX_CG_SENDCONSOLECOMMAND:
		Cbuf_AddText( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_ADDCOMMAND:
		CL_AddCgameCommand( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_UPDATESCREEN:
		Com_EventLoop();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( Sys_IsDirectMapBoot() )
		{
			CL_STEFX_DrawDirectMapLoadScreen( "STEFX_CG_UPDATESCREEN" );
			return 0;
		}
#endif
		SCR_UpdateScreen();
		return 0;
	case STEFX_CG_CM_LOADMAP:
		CL_CM_LoadMap( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case STEFX_CG_CM_INLINEMODEL:
		return CL_SafeCgameInlineModel( "EF", args[1] );
#if defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_CM_LOADMODEL:
		XBLF("STEFX: EF cgame requested unsupported CM_LoadModel '%s'; returning world", (const char *) VMA(1));
		return 0;
#endif
	case STEFX_CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( (const float *) VMA(1), (const float *) VMA(2) );
	case STEFX_CG_CM_POINTCONTENTS:
		return CM_PointContents( (float *) VMA(1), args[2] );
	case STEFX_CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( (const float *) VMA(1), args[2], (const float *) VMA(3), (const float *) VMA(4) );
	case STEFX_CG_CM_BOXTRACE:
		CM_BoxTrace( (trace_t *) VMA(1), (const float *) VMA(2), (const float *) VMA(3), (const float *) VMA(4), (const float *) VMA(5), args[6], args[7] );
		return 0;
	case STEFX_CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( (trace_t *) VMA(1), (const float *) VMA(2), (const float *) VMA(3), (const float *) VMA(4), (const float *) VMA(5), args[6], args[7], (const float *) VMA(8), (const float *) VMA(9) );
		return 0;
	case STEFX_CG_CM_MARKFRAGMENTS:
		return re.MarkFragments( args[1], (float(*)[3]) VMA(2), (const float *) VMA(3), args[4], (float *) VMA(5), args[6], (markFragment_t *) VMA(7) );
	case STEFX_CG_S_STARTSOUND:
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (!cls.cgameStarted)
		{
			XBLF("STEFX: EF S_STARTSOUND bridge early started=%d ent=%d chan=%d handle=%d origin=%08x",
				cls.cgameStarted ? 1 : 0,
				args[2],
				args[3],
				args[4],
				args[1]);
		}
#if defined(STEFX_SP_HOSTED_MP)
		{
			soundChannel_t stefxSpChannel = CL_STEFX_TranslateOfficialSoundChannel( args[3] );
			g_SPXBHMAudioStartSoundCount++;
			g_SPXBHMAudioLastEntChan = ((unsigned int)(args[2] & 0xFFFF) << 16) | (unsigned int)(stefxSpChannel & 0xFFFF);
			g_SPXBHMAudioLastHandle = (unsigned int)args[4];
			CL_STEFX_HolomatchAudioProofLog("start", args[2], args[3], stefxSpChannel, args[4],
				args[1] ? (const float *)VMA(1) : NULL);
			if (stefxSpChannel == CHAN_BODY || stefxSpChannel == CHAN_ITEM || stefxSpChannel == CHAN_AUTO)
			{
#if defined(STEFX_HM_GAMEPLAY_DIAGNOSTICS)
				static int s_stefxHostedSoundBridgeBudget = 96;
				if (s_stefxHostedSoundBridgeBudget > 0)
				{
					XBLog_WriteCriticalf("STEFX_HM_SOUND_BRIDGE: before ent=%d efChan=%d spChan=%d handle=%d origin=%08x started=%d state=%d",
						args[2],
						args[3],
						stefxSpChannel,
						args[4],
						args[1],
						cls.cgameStarted ? 1 : 0,
						cls.state);
				}
#endif
				S_StartSound( (float *) VMA(1), args[2], stefxSpChannel, args[4] );
#if defined(STEFX_HM_GAMEPLAY_DIAGNOSTICS)
				if (s_stefxHostedSoundBridgeBudget > 0)
				{
					XBLog_WriteCriticalf("STEFX_HM_SOUND_BRIDGE: after ent=%d efChan=%d spChan=%d handle=%d origin=%08x",
						args[2],
						args[3],
						stefxSpChannel,
						args[4],
						args[1]);
					--s_stefxHostedSoundBridgeBudget;
				}
#endif
				return 0;
			}
			S_StartSound( (float *) VMA(1), args[2], stefxSpChannel, args[4] );
			return 0;
		}
#else
		S_StartSound( (float *) VMA(1), args[2], (soundChannel_t)args[3], args[4] );
		return 0;
#endif
#endif
		S_StartSound( (float *) VMA(1), args[2], (soundChannel_t)args[3], args[4] );
		return 0;
	case STEFX_CG_S_STARTLOCALSOUND:
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (!cls.cgameStarted)
		{
			XBLF("STEFX: EF S_STARTLOCALSOUND bridge early started=%d handle=%d chan=%d",
				cls.cgameStarted ? 1 : 0,
				args[1],
				args[2]);
		}
#endif
#if defined(STEFX_SP_HOSTED_MP)
		g_SPXBHMAudioStartLocalCount++;
		g_SPXBHMAudioLastEntChan = (unsigned int)(CL_STEFX_TranslateOfficialSoundChannel( args[2] ) & 0xFFFF);
		g_SPXBHMAudioLastHandle = (unsigned int)args[1];
		CL_STEFX_HolomatchAudioProofLog("local", 0, args[2], CL_STEFX_TranslateOfficialSoundChannel( args[2] ), args[1], NULL);
		S_StartLocalSound( args[1], CL_STEFX_TranslateOfficialSoundChannel( args[2] ) );
		return 0;
#endif
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case STEFX_CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds();
		return 0;
	case STEFX_CG_S_ADDLOOPINGSOUND:
		if (!cls.cgameStarted)
		{
			return 0;
		}
#if defined(STEFX_SP_HOSTED_MP)
		g_SPXBHMAudioLoopCount++;
		g_SPXBHMAudioLastEntChan = ((unsigned int)(args[1] & 0xFFFF) << 16) | (unsigned int)(CL_STEFX_TranslateOfficialSoundChannel( args[5] ) & 0xFFFF);
		g_SPXBHMAudioLastHandle = (unsigned int)args[4];
		CL_STEFX_HolomatchAudioProofLog("loop", args[1], args[5], CL_STEFX_TranslateOfficialSoundChannel( args[5] ), args[4],
			args[2] ? (const float *)VMA(2) : NULL);
		S_AddLoopingSound( args[1], (const float *) VMA(2), (const float *) VMA(3), args[4], CL_STEFX_TranslateOfficialSoundChannel( args[5] ) );
		return 0;
#endif
		S_AddLoopingSound( args[1], (const float *) VMA(2), (const float *) VMA(3), args[4], (soundChannel_t)args[5] );
		return 0;
	case STEFX_CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], (const float *) VMA(2) );
		return 0;
	case STEFX_CG_S_RESPATIALIZE:
#if defined(STEFX_SP_HOSTED_MP)
		g_SPXBHMAudioRespatializeCount++;
		g_SPXBHMAudioLastEntChan = ((unsigned int)(args[1] & 0xFFFF) << 16);
		CL_STEFX_HolomatchAudioProofLog("respatialize", args[1], 0, 0, 0,
			args[2] ? (const float *)VMA(2) : NULL);
#endif
		S_Respatialize( args[1], (const float *) VMA(2), (float(*)[3]) VMA(3), args[4] );
		return 0;
	case STEFX_CG_S_REGISTERSOUND:
#if defined(STEFX_SP_HOSTED_MP)
		g_SPXBHMAudioRegisterSoundCount++;
		if (CL_STEFX_HolomatchAudioProofEnabled() && g_SPXBHMAudioRegisterSoundCount <= 64)
		{
			XBLog_WriteCriticalf("STEFX_HM_AUDIO: register count=%u name='%s'",
				(unsigned int)g_SPXBHMAudioRegisterSoundCount,
				(const char *)VMA(1));
		}
#endif
		return S_RegisterSound( (const char *) VMA(1) );
	case STEFX_CG_S_STARTBACKGROUNDTRACK:
#ifdef _XBOX
		XBLog_Writef("STEFX: cgame background music syscall stefx intro='%s' loop='%s'",
			(const char *) VMA(1),
			(const char *) VMA(2));
#endif
		S_StartBackgroundTrack( (const char *) VMA(1), (const char *) VMA(2), qfalse );
		return 0;
#if !defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_FF_STARTFX:
		FFFX_START( (ffFX_e) args[1] );
		return 0;
	case STEFX_CG_FF_ENSUREFX:
		FFFX_ENSURE( (ffFX_e) args[1] );
		return 0;
	case STEFX_CG_FF_STOPFX:
		FFFX_STOP( (ffFX_e) args[1] );
		return 0;
	case STEFX_CG_FF_STOPALLFX:
		FFFX_STOPALL;
		return 0;
#endif
	case STEFX_CG_R_LOADWORLDMAP:
		re.LoadWorld( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_R_REGISTERMODEL:
		{
			const char *modelName = (const char *) VMA(1);
			qhandle_t modelHandle;
			if (modelName && (strstr(modelName, ".mdr") || strstr(modelName, "models/players/")))
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: bridge before R_RegisterModel '%s'", modelName);
			}
			modelHandle = re.RegisterModel( modelName );
			if (modelName && (strstr(modelName, ".mdr") || strstr(modelName, "models/players/")))
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: bridge R_RegisterModel '%s' -> %d",
					modelName, modelHandle);
			}
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
			if ( modelName && modelHandle )
			{
				if ( strstr( modelName, "/lower.mdr" ) )
				{
					s_stefxTrackedLowerModel = modelHandle;
					g_SPXBHMSplitLowerModel = (unsigned int)modelHandle;
				}
				else if ( strstr( modelName, "/upper.mdr" ) )
				{
					s_stefxTrackedUpperModel = modelHandle;
					g_SPXBHMSplitUpperModel = (unsigned int)modelHandle;
				}
				else if ( strstr( modelName, "/head.md3" ) )
				{
					s_stefxTrackedHeadModel = modelHandle;
					g_SPXBHMSplitHeadModel = (unsigned int)modelHandle;
				}
			}
#endif
			if (modelName && (strstr(modelName, ".mdr") || strstr(modelName, "models/players/")))
			{
				XBLF("STEFX: EF cgame R_RegisterModel '%s' -> %d", modelName, modelHandle);
			}
			return modelHandle;
		}
	case STEFX_CG_R_REGISTERSKIN:
		{
			const char *skinName = (const char *) VMA(1);
			qhandle_t skinHandle;
			if (skinName && strstr(skinName, "models/players/"))
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: bridge before R_RegisterSkin '%s'", skinName);
			}
			skinHandle = re.RegisterSkin( skinName );
			if (skinName && strstr(skinName, "models/players/"))
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: bridge R_RegisterSkin '%s' -> %d", skinName, skinHandle);
			}
			return skinHandle;
		}
	case STEFX_CG_R_REGISTERSHADER:
		{
			const char *shaderName = (const char *) VMA(1);
			return re.RegisterShader( shaderName );
		}
	case STEFX_CG_R_REGISTERSHADERNOMIP:
		{
			const char *shaderName = (const char *) VMA(1);
			return re.RegisterShaderNoMip( shaderName );
		}
#if defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_R_REGISTERSHADER3D:
		return re.RegisterShader( (const char *) VMA(1) );
#endif
	case STEFX_CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case STEFX_CG_R_ADDREFENTITYTOSCENE:
		{
#if defined(STEFX_SP_HOSTED_MP)
			const stefxEfRefEntity_t *efRefEnt = (const stefxEfRefEntity_t *) VMA(1);
			refEntity_t refEnt;
			static int s_stefxAddRefBridgeBudget = 128;
			static int s_stefxLineBridgeBudget = 96;
			static int s_stefxOrientedSpriteBridgeBudget = 8;
			static int s_stefxAlphaVertPolyBridgeBudget = 8;
			static int s_stefxLightningBridgeBudget = 8;
			static int s_stefxElectricityBridgeBudget = 8;
			static int s_stefxCylinderBridgeBudget = 8;
			if ( !CL_STEFX_CopyRefEntity( &refEnt, efRefEnt ) )
			{
				XBLF("STEFX: EF AddRef rejected pointer=%08x type=%d", (unsigned int)efRefEnt, efRefEnt ? efRefEnt->reType : -1);
				return 0;
			}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if ( ( refEnt.reType == RT_TEXTURED_LINE || refEnt.reType == RT_TAPERED_LINE ) &&
				( refEnt.renderfx & RF_FIRST_PERSON ) )
			{
				int lineSlot = 0;
				if ( refEnt.renderfx & RF_STEFX_SPLIT_SLOT2 )
				{
					lineSlot = ( refEnt.renderfx & RF_STEFX_SPLIT_SLOT1 ) ? 3 : 2;
				}
				else if ( refEnt.renderfx & RF_STEFX_SPLIT_SLOT1 )
				{
					lineSlot = 1;
				}
				++g_SPXBHMSplitPhaserBridgeLineFP[lineSlot];
				g_SPXBHMSplitPhaserBridgeLastNumber[lineSlot] = (unsigned int)refEnt.number;
			}
			if ( refEnt.number >= STEFX_REFENTITY_PHASER_BEAM_BASE &&
				refEnt.number < STEFX_REFENTITY_PHASER_BEAM_BASE + 4 &&
				( refEnt.reType == RT_TEXTURED_LINE || refEnt.reType == RT_TAPERED_LINE ) )
			{
				const int phaserOwner = refEnt.number - STEFX_REFENTITY_PHASER_BEAM_BASE;
				if ( refEnt.renderfx & RF_FIRST_PERSON )
				{
					++g_SPXBHMSplitPhaserBridgeFP[phaserOwner];
				}
				else
				{
					++g_SPXBHMSplitPhaserBridgeWorld[phaserOwner];
				}
			}
			if (s_stefxAddRefBridgeBudget > 0)
			{
				XBLog_Writef("STEFX: EF AddRef marshal efType=%d spType=%d hModel=%d shader=%d renderfx=0x%x origin=(%g,%g,%g) radius=%g",
					efRefEnt->reType,
					refEnt.reType,
					refEnt.hModel,
					refEnt.customShader,
					refEnt.renderfx,
					refEnt.origin[0], refEnt.origin[1], refEnt.origin[2],
					refEnt.radius);
				--s_stefxAddRefBridgeBudget;
			}
			if ( s_stefxLineBridgeBudget > 0 &&
				( efRefEnt->reType == STEFX_EF_RT_LINE ||
				  efRefEnt->reType == STEFX_EF_RT_ORIENTEDLINE ||
				  efRefEnt->reType == STEFX_EF_RT_LINE2 ) )
			{
				XBLog_WriteCriticalf("STEFX_RENDER_LINE bridge efType=%d spType=%d shader=%d rf=0x%x width=%g width2=%g st=%g rgba=(%u,%u,%u,%u) start=(%g,%g,%g) end=(%g,%g,%g)",
					efRefEnt->reType,
					refEnt.reType,
					refEnt.customShader,
					refEnt.renderfx,
					efRefEnt->data.line.width,
					efRefEnt->data.line.width2,
					efRefEnt->data.line.stscale,
					(unsigned int)refEnt.shaderRGBA[0],
					(unsigned int)refEnt.shaderRGBA[1],
					(unsigned int)refEnt.shaderRGBA[2],
					(unsigned int)refEnt.shaderRGBA[3],
					refEnt.origin[0], refEnt.origin[1], refEnt.origin[2],
					refEnt.oldorigin[0], refEnt.oldorigin[1], refEnt.oldorigin[2]);
				--s_stefxLineBridgeBudget;
			}
			switch ( efRefEnt->reType )
			{
			case STEFX_EF_RT_ORIENTEDSPRITE:
				if ( s_stefxOrientedSpriteBridgeBudget > 0 )
				{
					XBLog_WriteCriticalf("STEFX_RENDER_EF orientedSprite spType=%d shader=%d rf=0x%x radius=%g rotation=%g axis1=(%g,%g,%g) axis2=(%g,%g,%g) rgba=(%u,%u,%u,%u)",
						refEnt.reType, refEnt.customShader, refEnt.renderfx,
						efRefEnt->data.sprite.radius, efRefEnt->data.sprite.rotation,
						refEnt.axis[1][0], refEnt.axis[1][1], refEnt.axis[1][2],
						refEnt.axis[2][0], refEnt.axis[2][1], refEnt.axis[2][2],
						(unsigned int)refEnt.shaderRGBA[0], (unsigned int)refEnt.shaderRGBA[1],
						(unsigned int)refEnt.shaderRGBA[2], (unsigned int)refEnt.shaderRGBA[3]);
					--s_stefxOrientedSpriteBridgeBudget;
				}
				break;
			case STEFX_EF_RT_ALPHAVERTPOLY:
				if ( s_stefxAlphaVertPolyBridgeBudget > 0 )
				{
					XBLog_WriteCriticalf("STEFX_RENDER_EF alphaVertPoly spType=%d shader=%d rf=0x%x radius=%g cornerAlpha=(%u,%u,%u,%u)",
						refEnt.reType, refEnt.customShader, refEnt.renderfx,
						efRefEnt->data.sprite.radius,
						(unsigned int)efRefEnt->data.sprite.vertRGBA[0][3],
						(unsigned int)efRefEnt->data.sprite.vertRGBA[1][3],
						(unsigned int)efRefEnt->data.sprite.vertRGBA[2][3],
						(unsigned int)efRefEnt->data.sprite.vertRGBA[3][3]);
					--s_stefxAlphaVertPolyBridgeBudget;
				}
				break;
			case STEFX_EF_RT_LIGHTNING:
				if ( s_stefxLightningBridgeBudget > 0 )
				{
					XBLog_WriteCriticalf("STEFX_RENDER_EF electricity efType=%d spType=%d shader=%d rf=0x%x width=%g deviation=%g st=%g wrap=%d taper=%d start=(%g,%g,%g) end=(%g,%g,%g)",
						efRefEnt->reType, refEnt.reType, refEnt.customShader, refEnt.renderfx,
						efRefEnt->data.electricity.width, efRefEnt->data.electricity.deviation,
						efRefEnt->data.electricity.stscale, efRefEnt->data.electricity.wrap,
						efRefEnt->data.electricity.taper,
						refEnt.origin[0], refEnt.origin[1], refEnt.origin[2],
						refEnt.oldorigin[0], refEnt.oldorigin[1], refEnt.oldorigin[2]);
					--s_stefxLightningBridgeBudget;
				}
				break;
			case STEFX_EF_RT_ELECTRICITY:
				if ( s_stefxElectricityBridgeBudget > 0 )
				{
					XBLog_WriteCriticalf("STEFX_RENDER_EF electricity efType=%d spType=%d shader=%d rf=0x%x width=%g deviation=%g st=%g wrap=%d taper=%d start=(%g,%g,%g) end=(%g,%g,%g)",
						efRefEnt->reType, refEnt.reType, refEnt.customShader, refEnt.renderfx,
						efRefEnt->data.electricity.width, efRefEnt->data.electricity.deviation,
						efRefEnt->data.electricity.stscale, efRefEnt->data.electricity.wrap,
						efRefEnt->data.electricity.taper,
						refEnt.origin[0], refEnt.origin[1], refEnt.origin[2],
						refEnt.oldorigin[0], refEnt.oldorigin[1], refEnt.oldorigin[2]);
					--s_stefxElectricityBridgeBudget;
				}
				break;
			case STEFX_EF_RT_CYLINDER:
				if ( s_stefxCylinderBridgeBudget > 0 )
				{
					XBLog_WriteCriticalf("STEFX_RENDER_EF cylinder spType=%d shader=%d rf=0x%x width=%g width2=%g height=%g st=%g bias=%g wrap=%d origin=(%g,%g,%g)",
						refEnt.reType, refEnt.customShader, refEnt.renderfx,
						efRefEnt->data.cylinder.width, efRefEnt->data.cylinder.width2,
						efRefEnt->data.cylinder.height, efRefEnt->data.cylinder.stscale,
						efRefEnt->data.cylinder.bias, efRefEnt->data.cylinder.wrap,
						refEnt.origin[0], refEnt.origin[1], refEnt.origin[2]);
					--s_stefxCylinderBridgeBudget;
				}
				break;
			default:
				break;
			}
			if ( refEnt.reType == RT_MODEL && refEnt.hModel != 0 &&
				( refEnt.hModel == s_stefxTrackedLowerModel ||
				  refEnt.hModel == s_stefxTrackedUpperModel ||
				  refEnt.hModel == s_stefxTrackedHeadModel ) )
			{
				const char *part = refEnt.hModel == s_stefxTrackedLowerModel ? "lower" :
					( refEnt.hModel == s_stefxTrackedUpperModel ? "upper" : "head" );
				++s_stefxTrackedRefCount;
				if ( s_stefxTrackedRefCount <= 12 || ( s_stefxTrackedRefCount % 1800 ) == 0 )
				{
					XBLog_WriteCriticalf("STEFX_MODEL_BRIDGE part=%s number=%d h=%d frame=%d old=%d back=%g rf=0x%x skin=%d shader=%d origin=(%g,%g,%g) axisLen=(%g,%g,%g) nonNorm=%d",
						part,
						refEnt.number,
						refEnt.hModel,
						refEnt.frame,
						refEnt.oldframe,
						refEnt.backlerp,
						refEnt.renderfx,
						refEnt.customSkin,
						refEnt.customShader,
						refEnt.origin[0], refEnt.origin[1], refEnt.origin[2],
						VectorLength( refEnt.axis[0] ),
						VectorLength( refEnt.axis[1] ),
						VectorLength( refEnt.axis[2] ),
						refEnt.nonNormalizedAxes );
				}
			}
#endif
			re.AddRefEntityToScene( &refEnt );
#else
			const refEntity_t *refEnt = (const refEntity_t *) VMA(1);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (refEnt && refEnt->reType == RT_MODEL && refEnt->hModel >= 2 && refEnt->hModel <= 8)
			{
				static int s_stefxAddRefBridgeBudget = 96;
				if (s_stefxAddRefBridgeBudget > 0)
				{
					XBLog_Writef("STEFX: EF AddRef bridge model ent=%d hModel=%d renderfx=0x%x origin=(%g,%g,%g) axis0=(%g,%g,%g)",
						refEnt->number,
						refEnt->hModel,
						refEnt->renderfx,
						refEnt->origin[0], refEnt->origin[1], refEnt->origin[2],
						refEnt->axis[0][0], refEnt->axis[0][1], refEnt->axis[0][2]);
					--s_stefxAddRefBridgeBudget;
				}
			}
#endif
			re.AddRefEntityToScene( refEnt );
#endif
		}
		return 0;
#if !defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_R_GETLIGHTING:
		return re.GetLighting( (const float * ) VMA(1), (float *) VMA(2), (float *) VMA(3), (float *) VMA(4) );
#endif
	case STEFX_CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], (const polyVert_t *) VMA(3) );
		return 0;
	case STEFX_CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( (const float *) VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case STEFX_CG_R_RENDERSCENE:
		{
			const stefxRefdef_t *efRefdef = (const stefxRefdef_t *) VMA(1);
			refdef_t jaRefdef;
			static int s_stefxRenderSceneLogBudget = 32;
#if defined(STEFX_SP_HOSTED_MP)
			int renderSceneIndex = 32 - s_stefxRenderSceneLogBudget;
#endif
			CL_STEFX_CopyRefdef( &jaRefdef, efRefdef );
			if (s_stefxRenderSceneLogBudget > 0)
			{
#if defined(STEFX_SP_HOSTED_MP)
				XBLog_WriteCriticalf("STEFX: EF RenderScene marshal #%d ef=%08x time=%d rd=0x%x view=(%g,%g,%g) fov=(%g,%g) rect=%d,%d %dx%d",
					renderSceneIndex,
#else
				XBLF("STEFX: EF RenderScene marshal #%d ef=%08x time=%d rd=0x%x view=(%g,%g,%g) fov=(%g,%g) rect=%d,%d %dx%d",
					32 - s_stefxRenderSceneLogBudget,
#endif
					(unsigned int)efRefdef,
					jaRefdef.time,
					jaRefdef.rdflags,
					jaRefdef.vieworg[0], jaRefdef.vieworg[1], jaRefdef.vieworg[2],
					jaRefdef.fov_x, jaRefdef.fov_y,
					jaRefdef.x, jaRefdef.y, jaRefdef.width, jaRefdef.height);
				--s_stefxRenderSceneLogBudget;
			}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
			{
				const int splitPlayers = CL_STEFX_HolomatchSplitHudPlayers();
				int splitSlot;

				for (splitSlot = 1; splitSlot < splitPlayers; ++splitSlot)
				{
					refdef_t splitRefdef;
					const void *officialPlayerState =
						STEFX_HolomatchGetOfficialPlayerState(splitSlot);

					if (!officialPlayerState ||
						!RE_STEFX_SplitScreen_GetLocalRefdef(splitSlot, &splitRefdef))
					{
						continue;
					}
					STEFX_HM_CG_AddSplitViewWeapon(
						splitSlot,
						officialPlayerState,
						splitRefdef.vieworg,
						&splitRefdef.viewaxis[0][0],
						splitRefdef.fov_x,
						splitRefdef.time);
				}
			}
#endif
			re.RenderScene( &jaRefdef );
#if defined(STEFX_SP_HOSTED_MP)
			if (renderSceneIndex >= 0 && renderSceneIndex < 32)
			{
				XBLog_WriteCriticalf("STEFX: EF RenderScene returned #%d", renderSceneIndex);
			}
#endif
		}
		return 0;
	case STEFX_CG_R_SETCOLOR:
		re.SetColor( (const float *) VMA(1) );
		return 0;
	case STEFX_CG_R_DRAWSTRETCHPIC:
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		{
			float x = VMF(1);
			float y = VMF(2);
			float w = VMF(3);
			float h = VMF(4);
#if defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
			if ( g_SPXBHMScoreDrawState & 0x800u )
			{
				union { float f; unsigned int u; } bits;
				unsigned int count = ++g_SPXBHMScoreStretchCount;
				if ( count == 1u )
				{
					g_SPXBHMScoreStretchShader = (unsigned int)args[9];
					bits.f = x; g_SPXBHMScoreStretchX = bits.u;
					bits.f = y; g_SPXBHMScoreStretchY = bits.u;
					bits.f = w; g_SPXBHMScoreStretchW = bits.u;
					bits.f = h; g_SPXBHMScoreStretchH = bits.u;
				}
			}
#endif
#if defined(STEFX_SP_HOSTED_MP)
			{
				const int splitHudPlayers = s_stefxHolomatchHudFramePlayers;
				const int splitSlot = s_stefxHolomatchHudDrawSlot;
				if (splitHudPlayers > 0 && splitSlot >= 0 && splitSlot < splitHudPlayers)
				{
					static int s_stefxHMSplitHudLogBudget = 96;
					const float oldX = x;
					const float oldY = y;
					const float oldW = w;
					const float oldH = h;
					const float s1 = VMF(5);
					const float t1 = VMF(6);
					const float s2 = VMF(7);
					const float t2 = VMF(8);
					const qhandle_t shader = args[9];
					float dstX = oldX;
					float dstY = oldY;
					float dstW = oldW;
					float dstH = oldH;

					float rectX;
					float rectY;
					float rectW;
					float rectH;
					CL_STEFX_HolomatchMapHudToSplitSlot(splitHudPlayers, splitSlot, &dstX, &dstY, &dstW, &dstH);
					CL_STEFX_HolomatchSplitRect(splitHudPlayers, splitSlot, &rectX, &rectY, &rectW, &rectH);
					++g_SPXBHMSplitHudSerial[splitSlot];
					g_SPXBHMSplitHudRectX[splitSlot] = (unsigned int)(int)rectX;
					g_SPXBHMSplitHudRectY[splitSlot] = (unsigned int)(int)rectY;
					g_SPXBHMSplitHudRectW[splitSlot] = (unsigned int)(int)rectW;
					g_SPXBHMSplitHudRectH[splitSlot] = (unsigned int)(int)rectH;
					if (s_stefxHMSplitHudLogBudget > 0)
					{
						XBLog_WriteCriticalf("STEFX_HM_SPLIT_HUD: slot=%d players=%d shared=0 shader=%d src=(%g,%g %gx%g) dst=(%g,%g %gx%g)",
							splitSlot,
							splitHudPlayers,
							shader,
							oldX, oldY, oldW, oldH,
							dstX, dstY, dstW, dstH);
						--s_stefxHMSplitHudLogBudget;
					}
					re.DrawStretchPic(dstX, dstY, dstW, dstH, s1, t1, s2, t2, shader);
					return 0;
				}
			}
#endif
			static int s_stefxStretchPicBudget = 48;
			if ( s_stefxStretchPicBudget > 0 && w >= 600.0f && h >= 400.0f )
			{
				XBLF("STEFX: EF DrawStretchPic large shader=%d xy=(%g,%g) wh=(%g,%g) st=(%g,%g,%g,%g)",
					args[9],
					x, y, w, h,
					VMF(5), VMF(6), VMF(7), VMF(8));
				--s_stefxStretchPicBudget;
			}
			re.DrawStretchPic( x, y, w, h, VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		}
#else
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
#endif
		return 0;
#if !defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_R_DRAWSCREENSHOT:
		return 0;
#endif
	case STEFX_CG_R_MODELBOUNDS:
		re.ModelBounds( args[1], (float *) VMA(2), (float *) VMA(3) );
		return 0;
	case STEFX_CG_R_LERPTAG:
		re.LerpTag( (orientation_t *) VMA(1), args[2], args[3], args[4], VMF(5), (const char *) VMA(6) );
		return 0;
#if !defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_R_DRAWROTATEPIC:
		re.DrawRotatePic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), VMF(9), args[10] );
		return 0;
	case STEFX_CG_R_SCISSOR:
		re.Scissor( VMF(1), VMF(2), VMF(3), VMF(4) );
		return 0;
#endif
	case STEFX_CG_GETGLCONFIG:
		CL_GetGlconfig( (glconfig_t *) VMA(1) );
		return 0;
	case STEFX_CG_GETGAMESTATE:
		CL_GetGameState( (gameState_t *) VMA(1) );
		return 0;
	case STEFX_CG_GETCURRENTSNAPSHOTNUMBER:
		CL_GetCurrentSnapshotNumber( (int *) VMA(1), (int *) VMA(2) );
		return 0;
	case STEFX_CG_GETSNAPSHOT:
		return CL_STEFX_GetSnapshot( args[1], VMA(2) );
	case STEFX_CG_GETSERVERCOMMAND:
		{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			static int s_stefxServerCommandSyscallBudget = 96;
			if ( s_stefxServerCommandSyscallBudget > 0 )
			{
				XBLF("STEFX: engine EF GetServerCommand request seq=%d current=%d",
					args[1], clc.serverCommandSequence);
			}
#endif
			qboolean gotCommand = CL_GetServerCommand( args[1] );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if ( s_stefxServerCommandSyscallBudget > 0 )
			{
				XBLF("STEFX: engine EF GetServerCommand result seq=%d got=%d argv0='%s' argv1='%s' argv2='%s'",
					args[1], gotCommand ? 1 : 0,
					Cmd_Argv(0), Cmd_Argv(1), Cmd_Argv(2));
				--s_stefxServerCommandSyscallBudget;
			}
#endif
			return gotCommand;
		}
	case STEFX_CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case STEFX_CG_GETUSERCMD:
#if defined(STEFX_SP_HOSTED_MP)
		{
			usercmd_t spCommand;
			stefxUsercmd_t *officialCommand = (stefxUsercmd_t *)VMA(2);
			qboolean gotCommand;
			static int s_stefxUsercmdBridgeLogBudget = 32;
			memset(&spCommand, 0, sizeof(spCommand));
			if (officialCommand)
			{
				memset(officialCommand, 0, sizeof(*officialCommand));
			}
			gotCommand = CL_GetUserCmd(args[1], &spCommand);
			if (gotCommand && officialCommand)
			{
				STEFX_CopyJaUsercmdToEf(officialCommand, &spCommand);
			}
			if (s_stefxUsercmdBridgeLogBudget > 0)
			{
				XBLog_WriteCriticalf("STEFX: cgame usercmd bridge cmd=%d got=%d spSize=%d efSize=%d time=%d spButtons=0x%x efButtons=0x%x weapon=%d move=(%d,%d,%d)",
					args[1],
					gotCommand,
					(int)sizeof(spCommand),
					(int)sizeof(*officialCommand),
					spCommand.serverTime,
					spCommand.buttons,
					officialCommand ? officialCommand->buttons : 0,
					spCommand.weapon,
					spCommand.forwardmove,
					spCommand.rightmove,
					spCommand.upmove);
				--s_stefxUsercmdBridgeLogBudget;
			}
			return gotCommand;
		}
#else
		return CL_GetUserCmd( args[1], (usercmd_s *) VMA(2) );
#endif
	case STEFX_CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2), 0.0f, 0.0f );
		return 0;
	case STEFX_CG_MEMORY_REMAINING:
		return Hunk_MemoryRemaining();
#if !defined(STEFX_SP_HOSTED_MP)
	case STEFX_CG_S_UPDATEAMBIENTSET:
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		{
			static int s_stefxAmbientSyscallBudget = 48;
			if ( s_stefxAmbientSyscallBudget > 0 )
			{
				XBLF("STEFX: engine EF S_UpdateAmbientSet name='%s' started=%d",
					(const char *) VMA(1),
					cls.cgameStarted ? 1 : 0);
				--s_stefxAmbientSyscallBudget;
			}
		}
#endif
		if (!cls.cgameStarted)
		{
			return 0;
		}
		S_UpdateAmbientSet( (const char *) VMA(1), (float *) VMA(2) );
		return 0;
	case STEFX_CG_S_ADDLOCALSET:
		{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			static int s_stefxLocalSetSyscallBudget = 48;
			if ( s_stefxLocalSetSyscallBudget > 0 )
			{
				XBLF("STEFX: engine EF S_AddLocalSet enter name='%s' ent=%d time=%d",
					(const char *) VMA(1), args[4], args[5]);
			}
#endif
			int localSetTime = S_AddLocalSet( (const char *) VMA(1), (float *) VMA(2), (float *) VMA(3), args[4], args[5] );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if ( s_stefxLocalSetSyscallBudget > 0 )
			{
				XBLF("STEFX: engine EF S_AddLocalSet done ent=%d result=%d",
					args[4], localSetTime);
				--s_stefxLocalSetSyscallBudget;
			}
#endif
			return localSetTime;
		}
	case STEFX_CG_AS_PARSESETS:
		AS_ParseSets();
		return 0;
	case STEFX_CG_AS_ADDENTRY:
		AS_AddPrecacheEntry( (const char *) VMA(1) );
		return 0;
	case STEFX_CG_AS_GETBMODELSOUND:
		return AS_GetBModelSound( (const char *) VMA(1), args[2] );
	case STEFX_CG_S_GETSAMPLELENGTH:
		return S_GetSampleLengthInMilliSeconds( args[1] );
#else
	case STEFX_CG_CVAR_SET_NO_MODIFY:
		{
			extern void Cvar_SetNoModify( const char *var_name, const char *value );
			Cvar_SetNoModify( (const char *) VMA(1), (const char *) VMA(2) );
		}
		return 0;
#endif
	default:
		Com_Error( ERR_DROP, "Bad EF cgame system trap: %i", args[0] );
		return 0;
	}
}
#endif

int CL_CgameSystemCalls( int *args ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	return CL_STEFX_CgameSystemCalls( args );
#endif
	switch( args[0] ) {
	case CG_PRINT:
		Com_Printf( "%s", VMA(1) );
		return 0;
	case CG_ERROR:
		Com_Error( ERR_DROP, S_COLOR_RED"%s", VMA(1) );
		return 0;
	case CG_MILLISECONDS:
		return Sys_Milliseconds();
	case CG_CVAR_REGISTER:
		Cvar_Register( (vmCvar_t *) VMA(1), (const char *) VMA(2), (const char *) VMA(3), args[4] ); 
		return 0;
	case CG_CVAR_UPDATE:
		Cvar_Update( (vmCvar_t *) VMA(1) );
		return 0;
	case CG_CVAR_SET:
		Cvar_Set( (const char *) VMA(1), (const char *) VMA(2) );
		return 0;
	case CG_ARGC:
		return Cmd_Argc();
	case CG_ARGV:
		Cmd_ArgvBuffer( args[1], (char *) VMA(2), args[3] );
		return 0;
	case CG_ARGS:
		Cmd_ArgsBuffer( (char *) VMA(1), args[2] );
		return 0;
	case CG_FS_FOPENFILE:
		return FS_FOpenFileByMode( (const char *) VMA(1), (int *) VMA(2), (fsMode_t) args[3] );
	case CG_FS_READ:
		FS_Read( VMA(1), args[2], args[3] );
		return 0;
	case CG_FS_WRITE:
		FS_Write( VMA(1), args[2], args[3] );
		return 0;
	case CG_FS_FCLOSEFILE:
		FS_FCloseFile( args[1] );
		return 0;
	case CG_SENDCONSOLECOMMAND:
		Cbuf_AddText( (const char *) VMA(1) );
		return 0;
	case CG_ADDCOMMAND:
		CL_AddCgameCommand( (const char *) VMA(1) );
		return 0;
	case CG_SENDCLIENTCOMMAND:
		CL_AddReliableCommand( (const char *) VMA(1) );
		return 0;
	case CG_UPDATESCREEN:
		// this is used during lengthy level loading, so pump message loop
		Com_EventLoop();	// FIXME: if a server restarts here, BAD THINGS HAPPEN!
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( Sys_IsDirectMapBoot() )
		{
			CL_STEFX_DrawDirectMapLoadScreen( "CG_UPDATESCREEN" );
			return 0;
		}
#endif
		SCR_UpdateScreen();
		return 0;

#ifdef _XBOX
	case CG_RMG_INIT:
	case CG_CM_REGISTER_TERRAIN:
	case CG_RE_INIT_RENDERER_TERRAIN:
		Com_Error( ERR_FATAL, "ERROR: Terrain unsupported on Xbox.\n" );
#else
	case CG_RMG_INIT:
		/*
		if (!com_sv_running->integer)
		{	// don't do this if we are connected locally
			if (!TheRandomMissionManager)
			{
				TheRandomMissionManager = new CRMManager;
			}
			TheRandomMissionManager->SetLandScape( cmg.landScapes[args[1]] );
			TheRandomMissionManager->LoadMission(qfalse);
			TheRandomMissionManager->SpawnMission(qfalse);
			cmg.landScapes[args[1]]->UpdatePatches();
		}
		*/ //this is SP.. I guess we're always the client and server.
//		cl.mRMGChecksum = cm.landScapes[args[1]]->get_rand_seed();
		RM_CreateRandomModels(args[1], (const char *)VMA(2));
		//cmg.landScapes[args[1]]->rand_seed(cl.mRMGChecksum);		// restore it, in case we do a vid restart
		cmg.landScape->rand_seed(cmg.landScape->get_rand_seed());
//		TheRandomMissionManager->CreateMap();
		return 0;
	case CG_CM_REGISTER_TERRAIN:
		return CM_RegisterTerrain((const char *)VMA(1), false)->GetTerrainId();

	case CG_RE_INIT_RENDERER_TERRAIN:
		RE_InitRendererTerrain((const char *)VMA(1));
		return 0;
#endif	// _XBOX

	case CG_CM_LOADMAP:
#ifdef _XBOX
		CL_CM_LoadMap( (const char *) VMA(1) );
#else
		CL_CM_LoadMap( (const char *) VMA(1), args[2] );
#endif
		return 0;
	case CG_CM_NUMINLINEMODELS:
		return CM_NumInlineModels();
	case CG_CM_INLINEMODEL:
		return CL_SafeCgameInlineModel( "JA", args[1] );
	case CG_CM_TEMPBOXMODEL:
		return CM_TempBoxModel( (const float *) VMA(1), (const float *) VMA(2) );//, (int) VMA(3) );
	case CG_CM_POINTCONTENTS:
		return CM_PointContents( (float *)VMA(1), args[2] );
	case CG_CM_TRANSFORMEDPOINTCONTENTS:
		return CM_TransformedPointContents( (const float *) VMA(1), args[2], (const float *) VMA(3), (const float *) VMA(4) );
	case CG_CM_BOXTRACE:
		CM_BoxTrace( (trace_t *) VMA(1), (const float *) VMA(2), (const float *) VMA(3), (const float *) VMA(4), (const float *) VMA(5), args[6], args[7] );
		return 0;
	case CG_CM_TRANSFORMEDBOXTRACE:
		CM_TransformedBoxTrace( (trace_t *) VMA(1), (const float *) VMA(2), (const float *) VMA(3), (const float *) VMA(4), (const float *) VMA(5), args[6], args[7], (const float *) VMA(8), (const float *) VMA(9) );
		return 0;
	case CG_CM_MARKFRAGMENTS:
		return re.MarkFragments( args[1], (float(*)[3]) VMA(2), (const float *) VMA(3), args[4], (float *) VMA(5), args[6], (markFragment_t *) VMA(7) );
	case CG_CM_SNAPPVS:
		CM_SnapPVS((float(*))VMA(1),(byte *) VMA(2));
		return 0;
	case CG_S_STOPSOUNDS:
		S_StopSounds( );
		return 0;

	case CG_S_STARTSOUND:
		// stops an ERR_DROP internally if called illegally from game side, but note that it also gets here 
		//	legally during level start where normally the internal s_soundStarted check would return. So ok to hit this.
		if (!cls.cgameStarted){
			return 0;	
		}
#ifdef _XBOX
		{
			static int stefxSpSoundBridgeBudget = 8;
			qboolean stefxTraceVoice = stefxSpSoundBridgeBudget > 0 &&
				(args[3] == CHAN_VOICE || args[3] == CHAN_VOICE_ATTEN ||
				 args[3] == CHAN_VOICE_GLOBAL || args[3] == CHAN_ANNOUNCER);
			if (stefxTraceVoice)
			{
				--stefxSpSoundBridgeBudget;
				XBLog_WriteCriticalf("STEFX_SP_AUDIO: bridge enter ent=%d chan=%d handle=%d origin=%08x",
					args[2], args[3], args[4], args[1]);
			}
			S_StartSound( (float *) VMA(1), args[2], (soundChannel_t)args[3], args[4] );
			if (stefxTraceVoice)
			{
				XBLog_WriteCriticalf("STEFX_SP_AUDIO: bridge exit ent=%d chan=%d handle=%d",
					args[2], args[3], args[4]);
			}
			return 0;
		}
#else
		S_StartSound( (float *) VMA(1), args[2], (soundChannel_t)args[3], args[4] );
		return 0;
#endif
	case CG_S_UPDATEAMBIENTSET:
		// stops an ERR_DROP internally if called illegally from game side, but note that it also gets here 
		//	legally during level start where normally the internal s_soundStarted check would return. So ok to hit this.
		if (!cls.cgameStarted){
			return 0;
		}
		S_UpdateAmbientSet( (const char *) VMA(1), (float *) VMA(2) );
		return 0;
	case CG_S_ADDLOCALSET:
		return S_AddLocalSet( (const char *) VMA(1), (float *) VMA(2), (float *) VMA(3), args[4], args[5] );
	case CG_AS_PARSESETS:
		AS_ParseSets();
		return 0;
	case CG_AS_ADDENTRY:
		AS_AddPrecacheEntry( (const char *) VMA(1) );
		return 0;
	case CG_AS_GETBMODELSOUND:
		return AS_GetBModelSound( (const char *) VMA(1), args[2] );	
	case CG_S_STARTLOCALSOUND:
		// stops an ERR_DROP internally if called illegally from game side, but note that it also gets here 
		//	legally during level start where normally the internal s_soundStarted check would return. So ok to hit this.
		if (!cls.cgameStarted){
			return 0;
		}
		S_StartLocalSound( args[1], args[2] );
		return 0;
	case CG_S_CLEARLOOPINGSOUNDS:
		S_ClearLoopingSounds();
		return 0;
	case CG_S_ADDLOOPINGSOUND:
		// stops an ERR_DROP internally if called illegally from game side, but note that it also gets here 
		//	legally during level start where normally the internal s_soundStarted check would return. So ok to hit this.
		if (!cls.cgameStarted){
			return 0;
		}
		S_AddLoopingSound( args[1], (const float *) VMA(2), (const float *) VMA(3), args[4], (soundChannel_t)args[5] );
		return 0;
	case CG_S_UPDATEENTITYPOSITION:
		S_UpdateEntityPosition( args[1], (const float *) VMA(2) );
		return 0;
	case CG_S_RESPATIALIZE:
		S_Respatialize( args[1], (const float *) VMA(2), (float(*)[3]) VMA(3), args[4] );
		return 0;
	case CG_S_REGISTERSOUND:
		return S_RegisterSound( (const char *) VMA(1) );
	case CG_S_STARTBACKGROUNDTRACK:
#ifdef STEFX_ELITE_FORCE_SP
#ifdef _XBOX
		XBLog_Writef("STEFX: cgame background music syscall ef intro='%s' loop='%s'",
			(const char *) VMA(1),
			(const char *) VMA(2));
#endif
		S_StartBackgroundTrack( (const char *) VMA(1), (const char *) VMA(2), qfalse);
#else
		S_StartBackgroundTrack( (const char *) VMA(1), (const char *) VMA(2), args[3]);
#endif
		return 0;
	case CG_S_GETSAMPLELENGTH:
		return S_GetSampleLengthInMilliSeconds(  args[1]);
#ifdef _IMMERSION
	case CG_FF_START:
		CL_FF_Start( (ffHandle_t) args[1], (int) args[2] );
		return 0;
	case CG_FF_STOP:
		CL_FF_Stop( (ffHandle_t) args[1], (int) args[2] );
		return 0;
	case CG_FF_STOPALL:
		FF_StopAll();
		return 0;
	case CG_FF_SHAKE:
		FF_Shake( (int) args[1], (int) args[2] );
		return 0;
	case CG_FF_REGISTER:
		return FF_Register( (const char *) VMA(1), (int) args[2] );
	case CG_FF_ADDLOOPINGFORCE:
		CL_FF_AddLoopingForce( (ffHandle_t) args[1], (int) args[2] );
		return 0;
#else
	case CG_FF_STARTFX:
		FFFX_START( (ffFX_e) args[1] );
		return 0;
	case CG_FF_ENSUREFX:
		FFFX_ENSURE( (ffFX_e) args[1] );
		return 0;
	case CG_FF_STOPFX:
		FFFX_STOP( (ffFX_e) args[1] );
		return 0;
	case CG_FF_STOPALLFX:
		FFFX_STOPALL;
		return 0;
#endif // _IMMERSION
#ifdef _XBOX
	case CG_FF_XBOX_SHAKE:
		FF_XboxShake( VMF(1), (int) args[2] );
		return 0;
	case CG_FF_XBOX_DAMAGE:
		FF_XboxDamage( (int) args[1], VMF(2) );
		return 0;
#endif
	case CG_R_LOADWORLDMAP:
		re.LoadWorld( (const char *) VMA(1) );
		return 0; 
	case CG_R_REGISTERMODEL:
		return re.RegisterModel( (const char *) VMA(1) );
	case CG_R_REGISTERSKIN:
		return re.RegisterSkin( (const char *) VMA(1) );
	case CG_R_REGISTERSHADER:
		return re.RegisterShader( (const char *) VMA(1) );
	case CG_R_REGISTERSHADERNOMIP:
		return re.RegisterShaderNoMip( (const char *) VMA(1) );
	case CG_R_REGISTERFONT:
		return re.RegisterFont( (const char *) VMA(1) );
	case CG_R_FONTSTRLENPIXELS:
		return re.Font_StrLenPixels( (const char *) VMA(1), args[2], VMF(3) );
	case CG_R_FONTSTRLENCHARS:
		return re.Font_StrLenChars( (const char *) VMA(1) );
	case CG_R_FONTHEIGHTPIXELS:
		return re.Font_HeightPixels( args[1], VMF(2) );
	case CG_R_FONTDRAWSTRING:
		re.Font_DrawString(args[1],args[2], (const char *) VMA(3), (float*)args[4], args[5], args[6], VMF(7));
		return 0;
	case CG_LANGUAGE_ISASIAN:
		return re.Language_IsAsian();
	case CG_LANGUAGE_USESSPACES:
		return re.Language_UsesSpaces();
	case CG_ANYLANGUAGE_READFROMSTRING:
		return re.AnyLanguage_ReadCharFromString( (const char *) VMA(1), (int *) VMA(2), (qboolean *) VMA(3) );
	case CG_R_SETREFRACTIONPROP:
		tr_distortionAlpha = VMF(1);
		tr_distortionStretch = VMF(2);
		tr_distortionPrePost = (qboolean)args[3];
		tr_distortionNegate = (qboolean)args[4];
		return 0;
	case CG_R_CLEARSCENE:
		re.ClearScene();
		return 0;
	case CG_R_ADDREFENTITYTOSCENE:
		re.AddRefEntityToScene( (const refEntity_t *) VMA(1) );
		return 0;

	case CG_R_INPVS:
		return R_inPVS((float *) VMA(1), (float *) VMA(2));

	case CG_R_GETLIGHTING:
		return re.GetLighting( (const float * ) VMA(1), (float *) VMA(2), (float *) VMA(3), (float *) VMA(4) );
	case CG_R_ADDPOLYTOSCENE:
		re.AddPolyToScene( args[1], args[2], (const polyVert_t *) VMA(3) );
		return 0;
	case CG_R_ADDLIGHTTOSCENE:
		re.AddLightToScene( (const float *) VMA(1), VMF(2), VMF(3), VMF(4), VMF(5) );
		return 0;
	case CG_R_RENDERSCENE:
		re.RenderScene( (const refdef_t *) VMA(1) );
		return 0;
	case CG_R_SETCOLOR:
		re.SetColor( (const float *) VMA(1) );
		return 0;
	case CG_R_DRAWSTRETCHPIC:
		re.DrawStretchPic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), args[9] );
		return 0;
	//case CG_R_DRAWSCREENSHOT:
	//	re.DrawStretchRaw( VMF(1), VMF(2), VMF(3), VMF(4), SG_SCR_WIDTH, SG_SCR_HEIGHT, SCR_GetScreenshot(0), 0, qtrue);
	//	return 0;
	case CG_R_MODELBOUNDS:
		re.ModelBounds( args[1], (float *) VMA(2), (float *) VMA(3) );
		return 0;
	case CG_R_LERPTAG:
		re.LerpTag( (orientation_t *) VMA(1), args[2], args[3], args[4], VMF(5), (const char *) VMA(6) );
		return 0;
	case CG_R_DRAWROTATEPIC:
		re.DrawRotatePic( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), VMF(9), args[10] );
		return 0;
	case CG_R_DRAWROTATEPIC2:
		re.DrawRotatePic2( VMF(1), VMF(2), VMF(3), VMF(4), VMF(5), VMF(6), VMF(7), VMF(8), VMF(9), args[10] );
		return 0;
	case CG_R_SETRANGEFOG:
		if (tr.rangedFog <= 0.0f)
		{
			g_oldRangedFog = tr.rangedFog;
		}
		tr.rangedFog = VMF(1);
		if (tr.rangedFog == 0.0f && g_oldRangedFog)
		{ //restore to previous state if applicable
			tr.rangedFog = g_oldRangedFog;
		}
		return 0;
	case CG_R_LA_GOGGLES:
		re.LAGoggles();
		return 0;
	case CG_R_SCISSOR:
		re.Scissor( VMF(1), VMF(2), VMF(3), VMF(4));
		return 0;
	case CG_GETGLCONFIG:
		CL_GetGlconfig( (glconfig_t *) VMA(1) );
		return 0;
	case CG_GETGAMESTATE:
		CL_GetGameState( (gameState_t *) VMA(1) );
		return 0;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		CL_GetCurrentSnapshotNumber( (int *) VMA(1), (int *) VMA(2) );
		return 0;
	case CG_GETSNAPSHOT:
		return CL_GetSnapshot( args[1], (snapshot_t *) VMA(2) );

	case CG_GETDEFAULTSTATE:
		return CL_GetDefaultState(args[1], (entityState_t *)VMA(2));

	case CG_GETSERVERCOMMAND:
		return CL_GetServerCommand( args[1] );
	case CG_GETCURRENTCMDNUMBER:
		return CL_GetCurrentCmdNumber();
	case CG_GETUSERCMD:
		return CL_GetUserCmd( args[1], (usercmd_s *) VMA(2) );
	case CG_SETUSERCMDVALUE:
		CL_SetUserCmdValue( args[1], VMF(2), VMF(3), VMF(4) );
		return 0;
	case CG_SETUSERCMDANGLES:
		CL_SetUserCmdAngles( VMF(1), VMF(2), VMF(3) );
		return 0;
	case COM_SETORGANGLES:
		Com_SetOrgAngles((float *)VMA(1),(float *)VMA(2));
		return 0;
/*
Ghoul2 Insert Start
*/
		
	case CG_G2_LISTSURFACES:
		G2API_ListSurfaces( (CGhoul2Info *) VMA(1) );
		return 0;

	case CG_G2_LISTBONES:
		G2API_ListBones( (CGhoul2Info *) VMA(1), args[2]);
		return 0;

	case CG_G2_HAVEWEGHOULMODELS:
		return G2API_HaveWeGhoul2Models( *((CGhoul2Info_v *)VMA(1)) );

	case CG_G2_SETMODELS:
		G2API_SetGhoul2ModelIndexes( *((CGhoul2Info_v *)VMA(1)),(qhandle_t *)VMA(2),(qhandle_t *)VMA(3));
		return 0;

/*
Ghoul2 Insert End
*/

	case CG_R_GET_LIGHT_STYLE:
		re.GetLightStyle(args[1], (byte*) VMA(2) );
		return 0;
	case CG_R_SET_LIGHT_STYLE:
		re.SetLightStyle(args[1], args[2] );
		return 0;

	case CG_R_GET_BMODEL_VERTS:
		re.GetBModelVerts( args[1], (float (*)[3])VMA(2), (float *)VMA(3) );
		return 0;
	
	case CG_R_WORLD_EFFECT_COMMAND:
		re.WorldEffectCommand( (const char *) VMA(1) );
		return 0;

	case CG_CIN_PLAYCINEMATIC:
	  return CIN_PlayCinematic( (const char *) VMA(1), args[2], args[3], args[4], args[5], args[6], (const char *) VMA(7));

	case CG_CIN_STOPCINEMATIC:
	  return CIN_StopCinematic(args[1]);

	case CG_CIN_RUNCINEMATIC:
	  return CIN_RunCinematic(args[1]);

#ifndef _XBOX
	case CG_CIN_DRAWCINEMATIC:
	  CIN_DrawCinematic(args[1]);
	  return 0;
#endif

	case CG_CIN_SETEXTENTS:
	  CIN_SetExtents(args[1], args[2], args[3], args[4], args[5]);
	  return 0;

	case CG_Z_MALLOC:
		return (int)Z_Malloc(args[1], (memtag_t) args[2], qfalse);

	case CG_Z_FREE:
		Z_Free((void *) VMA(1));
		return 0;

	case CG_UI_SETACTIVE_MENU:
		UI_SetActiveMenu((const char *) VMA(1),NULL);
		return 0;

	case CG_UI_MENU_OPENBYNAME:
		Menus_OpenByName((const char *) VMA(1));
		return 0;

	case CG_UI_MENU_RESET:
		Menu_Reset();
		return 0;

	case CG_UI_MENU_NEW:
		Menu_New((char *) VMA(1));
		return 0;

	case CG_UI_PARSE_INT:
		PC_ParseInt((int *) VMA(1));
		return 0;

	case CG_UI_PARSE_STRING:
		PC_ParseString((const char **) VMA(1));
		return 0;

	case CG_UI_PARSE_FLOAT:
		PC_ParseFloat((float *) VMA(1));
		return 0;

	case CG_UI_STARTPARSESESSION:
	{
#ifdef _XBOX
		const char *parseName = (const char *) VMA(1);
		Com_PrintfAlways("JA: CG trap UI_STARTPARSE begin file=%s\n", parseName ? parseName : "(null)");
#endif
		int parseLen = PC_StartParseSession((char *) VMA(1),(char **) VMA(2));
#ifdef _XBOX
		Com_PrintfAlways("JA: CG trap UI_STARTPARSE done file=%s len=%d\n", parseName ? parseName : "(null)", parseLen);
#endif
		return parseLen;
	}

	case CG_UI_ENDPARSESESSION:
		PC_EndParseSession((char *) VMA(1));
		return 0;

	case CG_UI_PARSEEXT:
		char **holdPtr;

		holdPtr = (char **) VMA(1);
		*holdPtr = PC_ParseExt();
		return 0;

	case CG_UI_MENUCLOSE_ALL:
		Menus_CloseAll();
		return 0;

	case CG_UI_MENUPAINT_ALL:
		Menu_PaintAll();
		return 0;

	case CG_UI_STRING_INIT:
		String_Init();
		return 0;

	case CG_UI_GETMENUINFO:
		menuDef_t *menu;
		int		*xPos,*yPos,*w,*h,result;

		menu = Menus_FindByName((char *) VMA(1));	// Get menu 
		if (menu)
		{
			xPos = (int *) VMA(2);
			*xPos = (int) menu->window.rect.x;
			yPos = (int *) VMA(3);
			*yPos = (int) menu->window.rect.y;
			w = (int *) VMA(4);
			*w = (int) menu->window.rect.w;
			h = (int *) VMA(5);
			*h = (int) menu->window.rect.h;
			result = qtrue;
		}
		else
		{
			result = qfalse;
		}

		return result;

	case CG_UI_GETITEMTEXT:
		itemDef_t *item;
		menu = Menus_FindByName((char *) VMA(1));	// Get menu 

		if (menu)
		{
			item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, (char *) VMA(2));
			if (item)
			{
				Q_strncpyz( (char *) VMA(3), item->text, 256 );
				result = qtrue;
			}
			else
			{
				result = qfalse;
			}
		}
		else
		{
			result = qfalse;
		}

		return result;

	case CG_UI_GETITEMINFO:
		menu = Menus_FindByName((char *) VMA(1));	// Get menu 

		if (menu)
		{
			qhandle_t *background;

			item = (itemDef_s *) Menu_FindItemByName((menuDef_t *) menu, (char *) VMA(2));
			if (item)
			{
				xPos = (int *) VMA(3);
				*xPos = (int) item->window.rect.x;
				yPos = (int *) VMA(4);
				*yPos = (int) item->window.rect.y;
				w = (int *) VMA(5);
				*w = (int) item->window.rect.w;
				h = (int *) VMA(6);
				*h = (int) item->window.rect.h;

				vec4_t *color;

				color = (vec4_t *) VMA(7);
				if (!color)
				{
					return qfalse;
				}

				(*color)[0] = (float) item->window.foreColor[0];
				(*color)[1] = (float) item->window.foreColor[1];
				(*color)[2] = (float) item->window.foreColor[2];
				(*color)[3] = (float) item->window.foreColor[3];
				background = (qhandle_t *) VMA(8);
				if (!background)
				{
					return qfalse;
				}
				*background = item->window.background;

				result = qtrue;
			}
			else
			{
				result = qfalse;
			}
		}
		else
		{
			result = qfalse;
		}

		return result;
		
	case CG_SP_GETSTRINGTEXTSTRING:
		const char* text;

		assert(VMA(1));	
		text = SE_GetString( (const char *) VMA(1) );

		if (VMA(2))	// only if dest buffer supplied...
		{
			if ( text[0] )
			{
				Q_strncpyz( (char *) VMA(2), text, args[3] );				
			}
			else 
			{
				Com_sprintf( (char *) VMA(2), args[3], "??%s", VMA(1) );
			}
		}
		return strlen(text);
		//break;
	default:
		Com_Error( ERR_DROP, "Bad cgame system trap: %i", args[0] );
	}
	return 0;
}


/*
====================
CL_InitCGame

Should only be called by CL_StartHunkUsers
====================
*/
extern qboolean Sys_LowPhysicalMemory();
void CL_InitCGame( void ) {
	const char			*info;
	const char			*mapname;
	int		t1, t2;

	XBLog_Write("JA: CL_InitCGame entered");

	t1 = Sys_Milliseconds();

	// put away the console
	Con_Close();

	// find the current mapname
	info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
	mapname = Info_ValueForKey( info, "mapname" );
	Com_sprintf( cl.mapname, sizeof( cl.mapname ), "maps/%s.bsp", mapname );
	XBLog_Write("JA: Loading map:");
	XBLog_Write(cl.mapname);

	cls.state = CA_LOADING;
	XBLog_Write("JA: cls.state = CA_LOADING");

	// init for this gamestate
	XBLog_Write("JA: VM_Call(CG_INIT)...");
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	VM_Call( CG_INIT, cl.frame.messageNum, clc.serverCommandSequence );
#else
	VM_Call( CG_INIT, clc.serverCommandSequence );
#endif
	XBLog_Write("JA: VM_Call(CG_INIT) done");

	// we will send a usercmd this frame, which
	// will cause the server to send us the first snapshot
	cls.state = CA_PRIMED;
	XBLog_Write("JA: cls.state = CA_PRIMED");

	t2 = Sys_Milliseconds();

	//Com_Printf( "CL_InitCGame: %5.2f seconds\n", (t2-t1)/1000.0 );
	// have the renderer touch all its images, so they are present
	// on the card even if the driver does deferred loading
	re.EndRegistration();

	// make sure everything is paged in
//	if (!Sys_LowPhysicalMemory()) 
	{
		Com_TouchMemory();
	}

	// clear anything that got printed
	Con_ClearNotify ();
}


/*
====================
CL_GameCommand

See if the current console command is claimed by the cgame
====================
*/
qboolean CL_GameCommand( void ) {
	qboolean result;

	if ( cls.state != CA_ACTIVE ) {
		return qfalse;
	}

	result = VM_Call( CG_CONSOLE_COMMAND );
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	++g_SPXBHMGameCommandCount;
	g_SPXBHMGameCommandResult = result ? 1u : 0u;
#endif
	return result;
}



/*
=====================
CL_CGameRendering
=====================
*/
void CL_CGameRendering( stereoFrame_t stereo ) {
	int stefxSplitHudPlayers = 0;
#if 0
	if ( cls.state == CA_ACTIVE ) {
		static int counter;

		if ( ++counter == 40 ) {
			VM_Debug( 2 );
		}
	}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	static int s_xboxCGameRenderCount = 0;
	const int xboxLogLateFrame = (cls.state == CA_ACTIVE && cl.serverTime >= 3600 && cl.serverTime <= 4600);
	const int xboxLogThisFrame = (cls.state == CA_ACTIVE && (s_xboxCGameRenderCount < 24 || xboxLogLateFrame));
	if (xboxLogThisFrame)
	{
		XBLF("JA: CL_CGameRendering #%d enter state=%d serverTime=%d stereo=%d",
			s_xboxCGameRenderCount, (int)cls.state, cl.serverTime, (int)stereo);
	}
#elif defined(_XBOX)
	const int xboxLogThisFrame = 0;
#endif
	int timei=cl.serverTime;
	if (timei>60)
	{
		timei-=0;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (xboxLogThisFrame)
	{
		XBLog_Write("STEFX: Ghoul2 cgame time tick skipped");
	}
#else
	G2API_SetTime(cl.serverTime,G2T_CG_TIME);
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxLogThisFrame)
	{
		XBLog_Write("JA: CL_CGameRendering: VM_Call(CG_DRAW_ACTIVE_FRAME)...");
	}
#endif
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBClTailStage = 0x564d3031; /* 'VM01' */
	#endif
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	stefxSplitHudPlayers = CL_STEFX_HolomatchSplitHudPlayers();
	s_stefxHolomatchHudFramePlayers = stefxSplitHudPlayers;
	s_stefxHolomatchHudDrawSlot = stefxSplitHudPlayers > 0 ? 0 : -1;
#endif
	VM_Call( CG_DRAW_ACTIVE_FRAME,timei, stereo, qfalse );
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	{
		static int s_stefxHMSplitHudCvarLogBudget = 12;
		const int livePlayersAfterDraw = CL_STEFX_HolomatchSplitHudPlayers();

		if (s_stefxHMSplitHudCvarLogBudget > 0 &&
			livePlayersAfterDraw != stefxSplitHudPlayers)
		{
			XBLog_WriteCriticalf("STEFX_HM_SPLIT_HUD_CVAR: framePlayers=%d liveAfterDraw=%d slot=%d",
				stefxSplitHudPlayers,
				livePlayersAfterDraw,
				s_stefxHolomatchHudDrawSlot);
			--s_stefxHMSplitHudCvarLogBudget;
		}
	}
	if (stefxSplitHudPlayers > 0)
	{
		int slot;

		for (slot = 1; slot < stefxSplitHudPlayers; ++slot)
		{
			refdef_t splitRefdef;
			const void *officialPlayerState = STEFX_HolomatchGetOfficialPlayerState(slot);

			if (!officialPlayerState ||
				!RE_STEFX_SplitScreen_GetLocalRefdef(slot, &splitRefdef))
			{
				continue;
			}
			s_stefxHolomatchHudDrawSlot = slot;
			STEFX_HM_CG_DrawSplitPlayerHud(
				slot,
				officialPlayerState,
				splitRefdef.vieworg,
				&splitRefdef.viewaxis[0][0],
				splitRefdef.fov_x,
				splitRefdef.fov_y,
				splitRefdef.time);
		}

		s_stefxHolomatchHudDrawSlot = -1;
		STEFX_HM_CG_DrawSplitGlobalHud();
	}
	s_stefxHolomatchHudDrawSlot = -1;
	CL_STEFX_DrawHolomatchSplitStatusOverlay();
	s_stefxHolomatchHudFramePlayers = 0;
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBClTailStage = 0x564d3032; /* 'VM02' */
	if (xboxLogThisFrame)
	{
		XBLog_Write("JA: CL_CGameRendering: VM_Call(CG_DRAW_ACTIVE_FRAME) returned");
	}
	s_xboxCGameRenderCount++;
#endif
//	VM_Debug( 0 );
}


/*
=================
CL_AdjustTimeDelta

Adjust the clients view of server time.

We attempt to have cl.serverTime exactly equal the server's view
of time plus the timeNudge, but with variable latencies over
the internet it will often need to drift a bit to match conditions.

Our ideal time would be to have the adjusted time aproach, but not pass,
the very latest snapshot.

Adjustments are only made when a new snapshot arrives, which keeps the
adjustment process framerate independent and prevents massive overadjustment
during times of significant packet loss.
=================
*/

#define	RESET_TIME	300

void CL_AdjustTimeDelta( void ) {
/*
	cl.newSnapshots = qfalse;
	// if the current time is WAY off, just correct to the current value
	if ( cls.realtime + cl.serverTimeDelta < cl.frame.serverTime - RESET_TIME 
		|| cls.realtime + cl.serverTimeDelta > cl.frame.serverTime + RESET_TIME  ) {
		cl.serverTimeDelta = cl.frame.serverTime - cls.realtime;
		cl.oldServerTime = cl.frame.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	}

	// if any of the frames between this and the previous snapshot
	// had to be extrapolated, nudge our sense of time back a little
	if ( cl.extrapolatedSnapshot ) {
		cl.extrapolatedSnapshot = qfalse;
		cl.serverTimeDelta -= 2;
	} else {
		// otherwise, move our sense of time forward to minimize total latency
		cl.serverTimeDelta++;
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
*/
	int		resetTime;
	int		newDelta;
	int		deltaDelta;

	cl.newSnapshots = qfalse;
	
	// if the current time is WAY off, just correct to the current value
	if ( com_sv_running->integer ) {
		resetTime = 100;
	} else {
		resetTime = RESET_TIME;
	}

	newDelta = cl.frame.serverTime - cls.realtime;
	deltaDelta = abs( newDelta - cl.serverTimeDelta );

	if ( deltaDelta > RESET_TIME ) {
		cl.serverTimeDelta = newDelta;
		cl.oldServerTime = cl.frame.serverTime;	// FIXME: is this a problem for cgame?
		cl.serverTime = cl.frame.serverTime;
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<RESET> " );
		}
	} else if ( deltaDelta > 100 ) {
		// fast adjust, cut the difference in half
		if ( cl_showTimeDelta->integer ) {
			Com_Printf( "<FAST> " );
		}
		cl.serverTimeDelta = ( cl.serverTimeDelta + newDelta ) >> 1;
	} else {
		// slow drift adjust, only move 1 or 2 msec

		// if any of the frames between this and the previous snapshot
		// had to be extrapolated, nudge our sense of time back a little
		// the granularity of +1 / -2 is too high for timescale modified frametimes
		if ( com_timescale->value == 0 || com_timescale->value == 1 ) {
			if ( cl.extrapolatedSnapshot ) {
				cl.extrapolatedSnapshot = qfalse;
				cl.serverTimeDelta -= 2;
			} else {
				// otherwise, move our sense of time forward to minimize total latency
				cl.serverTimeDelta++;
			}
		}
	}

	if ( cl_showTimeDelta->integer ) {
		Com_Printf( "%i ", cl.serverTimeDelta );
	}
}

// The UI sets this to a non-negative value, when the level select cheat is used
// to start a level. It will be the rank of neutral force powers that we should have.
// All others will be rank 3. =)
//int levelSelectCheat = -1;

/*
==================
CL_FirstSnapshot
==================
*/
void CL_FirstSnapshot( void ) {
	XBLog_Write("JA: CL_FirstSnapshot entered");

	RE_RegisterMedia_LevelLoadEnd();

	cls.state = CA_ACTIVE;
	XBLog_Write("JA: cls.state = CA_ACTIVE - GAME IS RUNNING");
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( Sys_IsDirectMapBoot() && (cls.keyCatchers & KEYCATCH_UI) )
	{
		XBLF("STEFX: direct-map first snapshot clearing UI catcher=0x%x", (unsigned int)cls.keyCatchers);
		UI_SetActiveMenu(NULL, NULL);
		XBLF("STEFX: direct-map first snapshot UI cleared catcher=0x%x", (unsigned int)cls.keyCatchers);
	}
#endif

	// set the timedelta so we are exactly on this first frame
	cl.serverTimeDelta = cl.frame.serverTime - cls.realtime;
	cl.oldServerTime = cl.frame.serverTime;

	// if this is the first frame of active play,
	// execute the contents of activeAction now
	// this is to allow scripting a timedemo to start right
	// after loading
	if ( cl_activeAction->string[0] ) {
		Cbuf_AddText( cl_activeAction->string );
		Cvar_Set( "activeAction", "" );
	}
	
	Sys_BeginProfiling();

#ifdef _XBOX
	// turn vsync back on - tearing is ugly
	glEnable(GL_VSYNC);
#endif

#ifdef XBOX_DEMO
	// It's convenient, so I call this the "end of loading" for timer pausing
	extern void Demo_TimerPause( bool bPaused );
	Demo_TimerPause( false );

	playerState_t *pState = svs.clients[0].gentity->client;

	// Give us the right weapons, and max ammo:
	extern int demoWeapon1;
	extern int demoWeapon2;
	extern int demoThrowable;
	pState->stats[STAT_WEAPONS] = 1 << WP_SABER;
	Cbuf_ExecuteText( EXEC_APPEND, "give weaponnum 2\n" );
	Cbuf_ExecuteText( EXEC_APPEND, va("give weaponnum %d\n", demoWeapon1) );
	Cbuf_ExecuteText( EXEC_APPEND, va("give weaponnum %d\n", demoWeapon2) );
	Cbuf_ExecuteText( EXEC_APPEND, va("give weaponnum %d\n", demoThrowable) );
	Cbuf_ExecuteText( EXEC_APPEND, "give ammo\n" );
#else
	// Goodies?
	cvar_t*	levelSelectCheat	= Cvar_Get("levelSelectCheat", "-1", CVAR_SAVEGAME);
	if(  levelSelectCheat->integer >= 0 )
	{
		int n = levelSelectCheat->integer;
		//levelSelectCheat = -1;

		// Set all Light powers to level 3:
		Cbuf_ExecuteText( EXEC_APPEND, "setForceHeal 3\nsetMindTrick 3\nsetForceProtect 3\nsetForceAbsorb 3\n" );
		// Set all Dark powers to level 3:
		Cbuf_ExecuteText( EXEC_APPEND, "setForceGrip 3\nsetForceLightning 3\nsetForceRage 3\nsetForceDrain 3\n" );

		// Special case for yavin1b - we need saber powers, but no other neutral
		if( n == 0 )
			Cbuf_ExecuteText( EXEC_APPEND, "setSaberOffense 1\nsetSaberDefense 1\nsetSaberThrow 1\n" );
		else
			Cbuf_ExecuteText( EXEC_APPEND, va("setSaberOffense %d\nsetSaberDefense %d\nsetSaberThrow %d\n", n, n, n) );

		// Set all remaining neutral powers to cheat level:
		Cbuf_ExecuteText( EXEC_APPEND, va("setForcePush %d\nsetForcePull %d\nsetForceSpeed %d\nsetForceJump %d\nsetForceSight %d\n", n, n, n, n, n) );
	}
#endif
}

/*
==================
CL_SetCGameTime
==================
*/
void CL_SetCGameTime( void ) {
#ifdef _XBOX
	static int s_xboxPrimedSetTimeLogCount = 0;
#endif

	// getting a valid frame message ends the connection process
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.state != CA_PRIMED ) {
			return;
		}
#ifdef _XBOX
		if (s_xboxPrimedSetTimeLogCount < 32 || (s_xboxPrimedSetTimeLogCount & 63) == 0)
		{
			XBLF("JA: CL_SetCGameTime primed count=%d newSnapshots=%d frameValid=%d frameMsg=%d serverTime=%d realtime=%d",
				s_xboxPrimedSetTimeLogCount,
				(int)cl.newSnapshots,
				(int)cl.frame.valid,
				cl.frame.messageNum,
				cl.frame.serverTime,
				cls.realtime);
		}
		s_xboxPrimedSetTimeLogCount++;
#endif
		if ( cl.newSnapshots ) {
			cl.newSnapshots = qfalse;
			CL_FirstSnapshot();
		}

		if ( cls.state != CA_ACTIVE ) {
#ifdef _XBOX
			if (s_xboxPrimedSetTimeLogCount < 32 || (s_xboxPrimedSetTimeLogCount & 63) == 0)
			{
				XBLog_Write("JA: CL_SetCGameTime still not active after primed check");
			}
#endif
			return;
		}
	}	

	// if we have gotten to this point, cl.frame is guaranteed to be valid
	if ( !cl.frame.valid ) {
		Com_Error( ERR_DROP, "CL_SetCGameTime: !cl.snap.valid" );
	}

	// allow pause in single player
	static int pauseStart = 0;
	static int pauseServerTimeDelta;
	if ( sv_paused->integer && cl_paused->integer && com_sv_running->integer ) {
		// paused
		if(!pauseStart) {
			pauseServerTimeDelta = cl.serverTimeDelta;
			pauseStart = cls.realtime;
		}
		return;
	}

	if ( cl.frame.serverTime < cl.oldFrameServerTime ) {
		Com_Error( ERR_DROP, "cl.frame.serverTime < cl.oldFrameServerTime" );
	}
	cl.oldFrameServerTime = cl.frame.serverTime;


	// get our current view of time

	// cl_timeNudge is a user adjustable cvar that allows more
	// or less latency to be added in the interest of better 
	// smoothness or better responsiveness.
	cl.serverTime = cls.realtime + cl.serverTimeDelta - cl_timeNudge->integer;

	//If we were paused, subtract out pause time once since we won't yet
	//have an updated server time.  Otherwise the camera gets confused when
	//time leaps forward for a frame and then resets back to normal.
	if(pauseStart && pauseServerTimeDelta == cl.serverTimeDelta) {
		cl.serverTime -= cls.realtime - pauseStart;
	} else {
		pauseStart = 0;
		pauseServerTimeDelta = 0;
	}

	// guarantee that time will never flow backwards, even if
	// serverTimeDelta made an adjustment or cl_timeNudge was changed
	if ( cl.serverTime < cl.oldServerTime ) {
		cl.serverTime = cl.oldServerTime;
	}
	cl.oldServerTime = cl.serverTime;

	// note if we are almost past the latest frame (without timeNudge),
	// so we will try and adjust back a bit when the next snapshot arrives
	if ( cls.realtime + cl.serverTimeDelta >= cl.frame.serverTime - 5 ) {
		cl.extrapolatedSnapshot = qtrue;
	}

	// if we have gotten new snapshots, drift serverTimeDelta
	// don't do this every frame, or a period of packet loss would
	// make a huge adjustment
	if ( cl.newSnapshots ) {
		CL_AdjustTimeDelta();
	}
}
