
// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"

#ifdef _XBOX
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBMapPhase;
extern "C" volatile unsigned int g_SPXBMapHash;
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBComSubphase;
extern "C" volatile unsigned int g_SPXBComSpinCount;
extern "C" volatile char g_SPXBMapLast[64];

static void SV_InitXboxTrace( unsigned int phase, unsigned int subphase, unsigned int detail )
{
	g_SPXBPhaseLast = phase;
	g_SPXBComSubphase = subphase;
	g_SPXBComSpinCount = detail;
}

static unsigned int SV_InitXboxHashText(const char *text)
{
	unsigned int h = 2166136261u;
	if (!text) {
		return 0;
	}
	while (*text) {
		h ^= (unsigned char)*text++;
		h *= 16777619u;
	}
	return h;
}

static void SV_InitXboxCopyLast(volatile char *dest, unsigned int destSize, const char *src)
{
	unsigned int i = 0;
	if (!dest || destSize == 0) {
		return;
	}
	if (src) {
		while (src[i] && i < destSize - 1) {
			dest[i] = src[i];
			++i;
		}
	}
	dest[i] = 0;
}
#endif

#include "../client/snd_music.h"	// didn't want to put this in snd_local because of rebuild times etc.
#include "../ui/ui_splash.h"
#include "server.h"

#if defined(STEFX_SP_HOSTED_MP)
extern void STEFX_HolomatchHostRunFrame(int levelTime);
extern void STEFX_HolomatchHostAfterGameFrame(int levelTime);
#endif
/*
Ghoul2 Insert Start
*/
#if !defined(TR_LOCAL_H)
	#include "../renderer/tr_local.h"
#endif

#if !defined (MINIHEAP_H_INC)
	#include "../qcommon/miniheap.h"
#endif

void CM_CleanLeafCache(void);
extern void SV_FreeClient(client_t*);

CMiniHeap *G2VertSpaceServer = NULL;
/*
Ghoul2 Insert End
*/


/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
#ifdef _XBOX
	static int s_xboxConfigStringLogCount = 0;
#endif
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( sv.configstrings[ index ] && !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

#ifdef _XBOX
	if ( s_xboxConfigStringLogCount < 96 )
	{
		XBLF("STEFX: SV_SetConfigstring index=%d old=%p new='%s'", index, sv.configstrings[index], val);
		s_xboxConfigStringLogCount++;
	}
#endif

	// change the string in sv
	if ( sv.configstrings[index] )
	{
		Z_Free( sv.configstrings[index] );
	}
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME ) {
		SV_SendServerCommand( NULL, "cs %i \"%s\"\n", index, val );
	}
}



/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i\n", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = 0;
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= STEFX_SERVER_CLIENT_SLOTS ) {
		Com_Error (ERR_DROP, "SV_SetUserinfo: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[ index ].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= STEFX_SERVER_CLIENT_SLOTS ) {
		Com_Error (ERR_DROP, "SV_GetUserinfo: bad index %i\n", index);
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void ) {
	gentity_t			*svent;
	int				entnum;	

	for ( entnum = 0; entnum < ge->num_entities ; entnum++ ) {
		svent = SV_GentityNum(entnum);
		if (!svent->inuse) {
			continue;
		}
		if (!svent->linked) {
			continue;
		}
		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[entnum].baseline = svent->s;
	}
}




/*
===============
SV_Startup

Called when a game is about to begin
===============
*/
void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}

#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_Startup allocating clients");
#endif
	svs.clients = (struct client_s *) Z_Malloc (
		sizeof(client_t) * STEFX_SERVER_CLIENT_SLOTS, TAG_CLIENTS, qtrue );
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_Startup client allocation complete");
#endif
#if defined(STEFX_SP_HOSTED_MP)
	svs.numSnapshotEntities = STEFX_SERVER_CLIENT_SLOTS * 4 * 64;
#else
	svs.numSnapshotEntities = 2 * 4 * 64;
#endif
	svs.initialized = qtrue;

	Cvar_Set( "sv_running", "1" );
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_Startup complete");
#endif
}


#ifdef _XBOX
//Xbox-only memory freeing.
extern void R_ModelFree(void);
extern void Sys_IORequestQueueClear(void);
extern void Music_Free(void);
extern void AS_FreePartial(void);
#if !defined(STEFX_SP_HOSTED_MP)
extern void G_ASPreCacheFree(void);
#endif
extern void Ghoul2InfoArray_Free(void);
extern void Ghoul2InfoArray_Reset(void);
extern void Menu_Reset(void);
extern void G2_FreeRag(void);
#if !defined(STEFX_SP_HOSTED_MP)
extern void ClearAllNavStructures(void);
extern void ClearModelsAlreadyDone(void);
#endif
extern void CL_FreeServerCommands(void);
extern void CL_FreeReliableCommands(void);
extern void CM_Free(void);
extern void ShaderEntryPtrs_Clear(void);
#if !defined(STEFX_SP_HOSTED_MP)
extern void G_FreeRoffs(void);
extern void BG_ClearVehicles(void);
extern void ClearHStringPool(void);
#endif
extern void ClearTheBonePool(void);
#if !defined(STEFX_SP_HOSTED_MP)
extern char cinematicSkipScript[64];
#endif
extern HANDLE s_BCThread;
extern void IN_HotSwap1Off(void);
extern void IN_HotSwap2Off(void);
extern void IN_HotSwap3Off(void);
#if !defined(STEFX_SP_HOSTED_MP)
extern int	cg_saberOnSoundTime[MAX_GENTITIES];
extern char current_speeders;
#endif
extern int zfFaceShaders[3];
extern int tfTorsoShader;
#if !defined(STEFX_SP_HOSTED_MP)
extern bool dontPillarPush;
#endif

void SV_ClearLastLevel(void)
{
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before Menu_Reset()");
	Menu_Reset();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before Z_TagFree(TAG_G_ALLOC)");
	Z_TagFree(TAG_G_ALLOC);
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before Z_TagFree(TAG_UI_ALLOC)");
	Z_TagFree(TAG_UI_ALLOC);
#if !defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before G_FreeRoffs()");
	G_FreeRoffs();
#endif
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before R_ModelFree()");
	R_ModelFree();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before Music_Free()");
	Music_Free();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before Sys_IORequestQueueClear()");
	Sys_IORequestQueueClear();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before AS_FreePartial()");
	AS_FreePartial();
#if !defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before G_ASPreCacheFree()");
	G_ASPreCacheFree();
#endif
#if !defined(STEFX_ELITE_FORCE_SP)
	Ghoul2InfoArray_Free();
	G2_FreeRag();
#endif
#if !defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before ClearAllNavStructures()");
	ClearAllNavStructures();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before ClearModelsAlreadyDone()");
	ClearModelsAlreadyDone();
#endif
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before CL_FreeServerCommands()");
	CL_FreeServerCommands();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before CL_FreeReliableCommands()");
	CL_FreeReliableCommands();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before CM_Free()");
	CM_Free();
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before ShaderEntryPtrs_Clear()");
	ShaderEntryPtrs_Clear();
#if !defined(STEFX_ELITE_FORCE_SP)
	ClearTheBonePool();
#endif
#if !defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before BG_ClearVehicles()");
	BG_ClearVehicles();

	cinematicSkipScript[0] = 0;
#endif

	if (svs.clients)
	{
		XBLog_WriteRingMarker("STEFX_VV_CLEAR: before SV_FreeClient( svs.clients )");
	SV_FreeClient( svs.clients );
	}

#if !defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("STEFX_VV_CLEAR: before ClearHStringPool()");
	ClearHStringPool();
#endif

	// The bink copier thread is so trivial as to not have any communication
	// Rather than polling constantly to clean it up, we just check here.
	// This code should only happen ONCE:
	if (s_BCThread != INVALID_HANDLE_VALUE)
	{
		DWORD status;
		if (GetExitCodeThread( s_BCThread, &status ) && (status != STILL_ACTIVE))
		{
			// Thread is done. Clean up after ourselves:
			CloseHandle( s_BCThread );
			s_BCThread = INVALID_HANDLE_VALUE;
		}
	}

	IN_HotSwap1Off();
	IN_HotSwap2Off();
	IN_HotSwap3Off();

	memset(zfFaceShaders, -1, sizeof(zfFaceShaders));
	tfTorsoShader = -1;

#if !defined(STEFX_SP_HOSTED_MP)
	memset(&cg_saberOnSoundTime, 0, MAX_GENTITIES);
	current_speeders = 0;

	dontPillarPush = false;
#endif
}
#endif

qboolean CM_SameMap(char *server);
qboolean CM_HasTerrain(void);
void Cvar_Defrag(void);

#ifdef _XBOX
// Last map load, retained independently of log-ring rotation. Milliseconds:
// start, preparation, collision, world, game init, settling, total, complete.
__declspec(dllexport) unsigned int g_SPXBLoadTimes[8] = {0};
#endif

// EF-owned load-time screen pulses. The old JA/Xbox YUY2 overlay animation was
// removed because EF loading chrome is drawn through the renderer path instead.
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean s_efLoadingAnimationActive = qfalse;
static int s_efLoadingAnimationDrawCount = 0;
static int s_efLoadingAnimationLogBudget = 16;
#endif

void InitLoadingAnimation( void )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	s_efLoadingAnimationActive = qfalse;
	s_efLoadingAnimationDrawCount = 0;
	s_efLoadingAnimationLogBudget = 16;
	XBLog_Write("STEFX: EF loading animation disabled during map load");
#endif
}

void UpdateLoadingAnimation( void )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( !s_efLoadingAnimationActive )
	{
		return;
	}
#endif
}

void StopLoadingAnimation( void )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLF("STEFX: EF loading animation stop count=%d", s_efLoadingAnimationDrawCount);
	s_efLoadingAnimationActive = qfalse;
#endif
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
================
*/
void SV_SpawnServer( char *iServer, ForceReload_e eForceReload, qboolean bAllowScreenDissolve )
{
	int			i;
	int			checksum;
	char		server[64];
#ifdef _XBOX
	const qboolean hadInitializedServer = svs.initialized ? qtrue : qfalse;
	const unsigned int loadStart = Sys_Milliseconds();
	unsigned int loadPhaseStart = loadStart;
	memset(g_SPXBLoadTimes, 0, sizeof(g_SPXBLoadTimes));
	g_SPXBLoadTimes[0] = loadStart;
#endif

	Q_strncpyz( server, iServer, sizeof(server), qtrue );
#ifdef _XBOX
	g_SPXBMapPhase = 10;
	g_SPXBMapHash = SV_InitXboxHashText(server);
	SV_InitXboxCopyLast(g_SPXBMapLast, sizeof(g_SPXBMapLast), server);
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	// Resolve the localized SP load title while the outgoing filesystem state
	// is still fully available. Renderer/game teardown starts below.
	XBLog_WriteRingMarker("STEFX_VV_RELOAD: before ui_mapname");
	Cvar_Set( "ui_mapname", server );
	XBLog_WriteRingMarker("STEFX_VV_RELOAD: before loading title");
	SP_PrecacheEFLoadingTitle();
	XBLog_WriteRingMarker("STEFX_VV_RELOAD: after loading title");
#endif
#endif
	XBLF("JA: SV_SpawnServer entered map='%s' reload=%d dissolve=%d", server, eForceReload, bAllowScreenDissolve);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	Cvar_Set( "ui_sp_levelname", "" );
#endif

#ifdef XBOX_DEMO
	// Pause the timer if "someone is playing"
	extern void Demo_TimerPause( bool bPaused );
	Demo_TimerPause( true );
#endif

// The following fixes for potential issues only work on Xbox
#if defined(_XBOX) && !defined(STEFX_SP_HOSTED_MP)
	extern qboolean stop_icarus;
	stop_icarus = qfalse;

	//Broken scripts may leave the player locked.  I think that's always bad.
	extern qboolean player_locked;
	player_locked = qfalse;

	//If you quit while in Matrix Mode, this never gets cleared!
	extern qboolean MatrixMode;
	MatrixMode = qfalse;
#endif

#ifdef _XBOX
	// Failsafe to ensure that we don't have rumbling during level load
	extern void IN_KillRumbleScripts( void );
	XBLog_WriteRingMarker("JA: SV_SpawnServer before IN_KillRumbleScripts");
	IN_KillRumbleScripts();
	XBLog_WriteRingMarker("JA: SV_SpawnServer after IN_KillRumbleScripts");
#endif

	XBLog_WriteRingMarker("JA: SV_SpawnServer before RE_RegisterMedia_LevelLoadBegin");
	RE_RegisterMedia_LevelLoadBegin( server, eForceReload, bAllowScreenDissolve );
	XBLog_WriteRingMarker("JA: SV_SpawnServer after RE_RegisterMedia_LevelLoadBegin");


	XBLog_WriteRingMarker("JA: SV_SpawnServer before cl_paused/timescale reset");
	Cvar_SetValue( "cl_paused", 0 );
	Cvar_Set( "timescale", "1" );//jic we were skipping
	XBLog_WriteRingMarker("JA: SV_SpawnServer after cl_paused/timescale reset");

	// shut down the existing game if it is running
	XBLog_WriteRingMarker("JA: SV_SpawnServer before SV_ShutdownGameProgs");
	SV_ShutdownGameProgs(qtrue);
	XBLog_WriteRingMarker("JA: SV_SpawnServer after SV_ShutdownGameProgs");

	Com_Printf ("------ Server Initialization ------\n%s\n", com_version->string);
	XBLog_WriteRingMarker("JA: SV_SpawnServer before Server print");
	Com_Printf ("Server: %s\n",server);	
	XBLog_WriteRingMarker("JA: SV_SpawnServer after Server print");
#if !defined(_XBOX) || !defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteRingMarker("JA: SV_SpawnServer before ui_mapname set");
	Cvar_Set( "ui_mapname", server );
	XBLog_WriteRingMarker("JA: SV_SpawnServer after ui_mapname set");
#endif

#ifndef FINAL_BUILD
//	extern unsigned long texturePointMax;
//	Com_Printf ("Texture pool highwater mark: %u\n", texturePointMax);
#endif

#ifdef _XBOX
	g_SPXBMapPhase = 11;
	// disable vsync during load for speed
	XBLog_WriteRingMarker("JA: SV_SpawnServer before glDisable GL_VSYNC");
	glDisable(GL_VSYNC);
	XBLog_WriteRingMarker("JA: SV_SpawnServer after glDisable GL_VSYNC");
#endif

	// Hope this is correct - InitGame gets called later, which does this,
	// but UI_DrawConnect (in CL_MapLoading) needs it now, to properly
	// mimic CG_DrawInformation:
#if !defined(STEFX_SP_HOSTED_MP)
	extern SavedGameJustLoaded_e g_eSavedGameJustLoaded;
	XBLog_WriteRingMarker("JA: SV_SpawnServer before saved-game load flag set");
	g_eSavedGameJustLoaded = eSavedGameJustLoaded;
	XBLog_WriteRingMarker("JA: SV_SpawnServer after saved-game load flag set");
#else
	XBLog_WriteRingMarker("JA: SV_SpawnServer skipped SP saved-game UI state for Holomatch");
#endif

	// don't let sound stutter and dump all stuff on the hunk
	XBLog_WriteRingMarker("JA: SV_SpawnServer before CL_MapLoading");
#ifdef _XBOX
	g_SPXBMapPhase = 111;
#endif
	CL_MapLoading();
#ifdef _XBOX
	g_SPXBMapPhase = 112;
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer client map-loading handoff complete");
#endif
	XBLog_WriteRingMarker("JA: SV_SpawnServer after CL_MapLoading");

	XBLog_WriteRingMarker("JA: SV_SpawnServer before CM_SameMap");
	if (!CM_SameMap(server))
	{ //rww - only clear if not loading the same map
		XBLog_WriteRingMarker("JA: SV_SpawnServer before CM_ClearMap");
		CM_ClearMap();
		XBLog_WriteRingMarker("JA: SV_SpawnServer after CM_ClearMap");
	}
	XBLog_WriteRingMarker("JA: SV_SpawnServer after CM_SameMap/clear");
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer collision map clear complete");
#endif
#ifndef _XBOX
	else if (CM_HasTerrain())
	{ //always clear when going between maps with terrain
		CM_ClearMap();
	}
#endif

#if !defined(STEFX_ELITE_FORCE_SP)
	// Miniheap never changes sizes, so I just put it really early in mem.
	G2VertSpaceServer->ResetHeap();
#else
	XBLog_Write("STEFX: Ghoul2 server miniheap reset skipped");
#endif

#ifdef _XBOX
	// Deletes all textures
	R_DeleteTextures();
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer server texture delete complete");
#endif
	Hunk_Clear();
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer hunk clear complete");
#endif

#ifdef _XBOX
	if (hadInitializedServer)
	{
#endif
		// Moved up from below to help reduce fragmentation
		if (svs.snapshotEntities)
		{
			Z_Free(svs.snapshotEntities);
			svs.snapshotEntities = NULL;
		}

		// wipe the entire per-level structure
		// Also moved up, trying to do all freeing before new allocs
		for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
			if ( sv.configstrings[i] ) {
				Z_Free( sv.configstrings[i] );
				sv.configstrings[i] = NULL;
			}
		}

#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer prior allocations released");
		SV_ClearLastLevel();
		XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer prior level teardown complete");
#endif

		// Collect all the small allocations done by the cvar system.
		Cvar_Defrag();
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer cvar defrag complete");
	}
	else
	{
		XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer cold boot skipped prior-level cleanup");
	}
#endif

/*
		This is useful for debugging memory fragmentation.  Please don't
	   remove it.
*/
#ifdef _XBOX
	g_SPXBMapPhase = 12;
#if !defined(STEFX_ELITE_FORCE_SP)
	// We've over-freed the info array above, this puts it back into a working state
	Ghoul2InfoArray_Reset();
#else
	XBLog_Write("STEFX: Ghoul2 info array reset skipped");
#endif

#if !defined(STEFX_ELITE_FORCE_SP)
	extern void Z_DumpMemMap_f(void);
	extern void Z_Details_f(void);
	extern void Z_TagPointers(memtag_t);
	Z_DumpMemMap_f();
//	Z_TagPointers(TAG_ALL);
	Z_Details_f();
#else
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer retail memory diagnostics skipped");
#endif
#endif

	InitLoadingAnimation();
	UpdateLoadingAnimation();
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer loading presentation initialized");
#endif

	// init client structures and svs.numSnapshotEntities
	// This is moved down quite a bit, but should be safe. And keeps
	// svs.clients right at the beginning of memory
	if ( !Cvar_VariableIntegerValue("sv_running") ) {
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer starting server state");
#endif
		SV_Startup();
	}

 	// clear out those shaders, images and Models
//	R_InitImages();
//	R_InitShaders();
//	R_ModelInit();

	// allocate the snapshot entities 
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer allocating snapshot entities");
#endif
	svs.snapshotEntities = (entityState_t *) Z_Malloc (sizeof(entityState_t)*svs.numSnapshotEntities, TAG_CLIENTS, qtrue );
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer snapshot allocation complete");
#endif

	Music_SetLevelName(server);
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer music level name set");
#endif

	// toggle the server bit so clients can detect that a
	// server has changed
//!@	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overriden
	// by the game startup or another console command
	Cvar_Set( "nextmap", va("map %s", server) );


	memset (&sv, 0, sizeof(sv));
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer level state cleared");
#endif


	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString("");
	}
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer configstrings allocated");
#endif

	sv.time = 1000;
#if !defined(STEFX_ELITE_FORCE_SP)
	G2API_SetTime(sv.time,G2T_SV_TIME);
#endif

#ifdef _XBOX
	g_SPXBMapPhase = 13;
	XBLog_Write("JA: CL_StartHunkUsers...");
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer restarting client subsystems");
	UpdateLoadingAnimation();
	CL_StartHunkUsers();
	XBLog_WriteCritical("STEFX_HW_BOOT: SV_SpawnServer client subsystem restart complete");
	UpdateLoadingAnimation();
	g_SPXBMapPhase = 14;
#if !defined(STEFX_ELITE_FORCE_SP)
	XBLog_WriteRingMarker("JA: SV_SpawnServer: precache humanoid GLA before BSP load...");
	{
		const qhandle_t normalHumanoid = RE_RegisterModel("models/players/_humanoid/_humanoid.gla");
		char cinematicHumanoid[MAX_QPATH];
		Com_sprintf(cinematicHumanoid, sizeof(cinematicHumanoid),
			"models/players/_humanoid_%s/_humanoid_%s.gla", server, server);
		const qhandle_t cinematicHumanoidHandle = RE_RegisterModel(cinematicHumanoid);
		XBLF("JA: SV_SpawnServer: humanoid GLA handles normal=%d cinematic=%d path='%s'",
			normalHumanoid, cinematicHumanoidHandle, cinematicHumanoid);
	}
#else
	XBLog_Write("STEFX: Ghoul2 humanoid GLA precache skipped");
#endif
	g_SPXBMapPhase = 15;
#if defined(STEFX_ELITE_FORCE_SP)
	XBLog_Write("STEFX: SV_SpawnServer pre-BSP EF loadscreen pulse");
	UpdateLoadingAnimation();
#endif
	XBLF("EF: SV_SpawnServer before CM_LoadMap map='%s' checksum=%d", server, checksum);
	g_SPXBLoadTimes[1] = Sys_Milliseconds() - loadStart;
	loadPhaseStart = Sys_Milliseconds();
	CM_LoadMap( va("maps/%s.bsp", server), qfalse, &checksum );
	g_SPXBLoadTimes[2] = Sys_Milliseconds() - loadPhaseStart;
	g_SPXBMapPhase = 16;
	XBLF("EF: SV_SpawnServer after CM_LoadMap map='%s' checksum=%d", server, checksum);
	UpdateLoadingAnimation();
	g_SPXBMapPhase = 17;
	XBLF("EF: SV_SpawnServer before RE_LoadWorldMap map='%s'", server);
	loadPhaseStart = Sys_Milliseconds();
	RE_LoadWorldMap(va("maps/%s.bsp", server));
	g_SPXBLoadTimes[3] = Sys_Milliseconds() - loadPhaseStart;
	g_SPXBMapPhase = 18;
	XBLF("EF: SV_SpawnServer after RE_LoadWorldMap map='%s'", server);
	UpdateLoadingAnimation();
#else
	CM_LoadMap( va("maps/%s.bsp", server), qfalse, &checksum, qfalse );
#endif

	// set serverinfo visible name
	Cvar_Set( "mapname", server );

	Cvar_Set( "sv_mapChecksum", va("%i",checksum) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// clear physics interaction links
	SV_ClearWorld ();

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// load and spawn all other entities
#ifdef _XBOX
	g_SPXBMapPhase = 19;
#endif
	XBLog_Write("JA: SV_InitGameProgs...");
#ifdef _XBOX
	loadPhaseStart = Sys_Milliseconds();
#endif
	SV_InitGameProgs();
#ifdef _XBOX
	g_SPXBLoadTimes[4] = Sys_Milliseconds() - loadPhaseStart;
	loadPhaseStart = Sys_Milliseconds();
	g_SPXBMapPhase = 20;
#endif
	XBLog_Write("JA: SV_InitGameProgs done");

	// run a few frames to allow everything to settle
#ifdef _XBOX
	g_SPXBMapPhase = 21;
	SV_InitXboxTrace( 0x53565350, 2100, 0 ); /* SVSP */
	XBLog_WriteRingMarker("JA: SV_SpawnServer ring before settle loop log");
	XBLog_WriteRingMarker("JA: SV_SpawnServer before settle RunFrame loop");
	SV_InitXboxTrace( 0x53565350, 2101, 0 ); /* SVSP */
	XBLog_WriteRingMarker("JA: SV_SpawnServer ring after settle loop log");
#endif
	for ( i = 0 ;i < 3 ; i++ ) {
#ifdef _XBOX
		SV_InitXboxTrace( 0x53565350, 2110, (unsigned int)i ); /* SVSP */
		XBLog_WriteRingMarker("JA: SV_SpawnServer ring before settle RunFrame");
		XBLF("JA: SV_SpawnServer settle RunFrame %d before time=%d", i, sv.time);
		SV_InitXboxTrace( 0x53565350, 2111, (unsigned int)i ); /* SVSP */
#endif
#if defined(STEFX_SP_HOSTED_MP)
		STEFX_HolomatchHostRunFrame(sv.time);
#endif
		ge->RunFrame( sv.time );
#if defined(STEFX_SP_HOSTED_MP)
		STEFX_HolomatchHostAfterGameFrame(sv.time);
#endif
#ifdef _XBOX
		SV_InitXboxTrace( 0x53565350, 2112, (unsigned int)i ); /* SVSP */
		XBLog_WriteRingMarker("JA: SV_SpawnServer ring after settle RunFrame");
		XBLF("JA: SV_SpawnServer settle RunFrame %d after time=%d", i, sv.time);
#endif
		sv.time += 100;
#if !defined(STEFX_ELITE_FORCE_SP)
		G2API_SetTime(sv.time,G2T_SV_TIME);
#endif
	}
#ifdef _XBOX
	g_SPXBMapPhase = 22;
	XBLog_WriteRingMarker("JA: SV_SpawnServer before ge->ConnectNavs");
#endif
	ge->ConnectNavs(sv_mapname->string, sv_mapChecksum->integer);
#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer after ge->ConnectNavs");
#endif

	// create a baseline for more efficient communications
#ifdef _XBOX
	g_SPXBMapPhase = 23;
	XBLog_WriteRingMarker("JA: SV_SpawnServer before SV_CreateBaseline");
#endif
	SV_CreateBaseline ();
#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer after SV_CreateBaseline");
#endif

	for (i=0 ; i<STEFX_SERVER_CLIENT_SLOTS ; i++) {
		// clear all time counters, because we have reset sv.time
		svs.clients[i].lastPacketTime = 0;
		svs.clients[i].lastConnectTime = 0;
		svs.clients[i].nextSnapshotTime = 0;

		// send the new gamestate to all connected clients
		if (svs.clients[i].state >= CS_CONNECTED) {
			char	*denied;
#if defined(STEFX_SP_HOSTED_MP)
			qboolean isBot = svs.clients[i].stefxHolomatchBot;
#endif

			// connect the client again
#ifdef _XBOX
			XBLF("JA: SV_SpawnServer before ClientConnect client=%d state=%d", i, svs.clients[i].state);
#endif
#if defined(STEFX_SP_HOSTED_MP)
			denied = ge->ClientConnect( i, qfalse, (SavedGameJustLoaded_e)isBot );
#else
			denied = ge->ClientConnect( i, qfalse, eNO/*qfalse*/ );	// firstTime = qfalse, qbFromSavedGame
#endif
#ifdef _XBOX
			XBLF("JA: SV_SpawnServer after ClientConnect client=%d denied=%s", i, denied ? denied : "(null)");
#endif
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
#if defined(STEFX_SP_HOSTED_MP)
				if (isBot)
				{
					client_t *client = &svs.clients[i];
					usercmd_t command;

					memset(&command, 0, sizeof(command));
					client->state = CS_ACTIVE;
					client->gentity = SV_GentityNum(i);
					client->gentity->s.number = i;
					client->deltaMessage = -1;
					client->nextSnapshotTime = sv.time;
					client->lastPacketTime = sv.time;
					ge->ClientBegin(i, &command, eNO);
					XBLF("STEFX_HM_BOT: reconnected active client=%d flags=0x%x", i,
						client->gentity ? client->gentity->svFlags : 0);
				}
				else
#endif
				{
				svs.clients[i].state = CS_CONNECTED;
				// when we get the next packet from a connected client,
				// the new gamestate will be sent
				}
			}
		}
	}	

	// run another frame to allow things to look at all connected clients
#ifdef _XBOX
	g_SPXBMapPhase = 24;
	XBLog_WriteRingMarker("JA: SV_SpawnServer before post-client RunFrame");
#endif
#if defined(STEFX_SP_HOSTED_MP)
	STEFX_HolomatchHostRunFrame(sv.time);
#endif
	ge->RunFrame( sv.time );
#if defined(STEFX_SP_HOSTED_MP)
	STEFX_HolomatchHostAfterGameFrame(sv.time);
#endif
#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer after post-client RunFrame");
#endif
	sv.time += 100;
#if !defined(STEFX_ELITE_FORCE_SP)
	G2API_SetTime(sv.time,G2T_SV_TIME);
#endif


	// save systeminfo and serverinfo strings
#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer before CS_SYSTEMINFO");
#endif
	SV_SetConfigstring( CS_SYSTEMINFO, Cvar_InfoString( CVAR_SYSTEMINFO ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;

#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer before CS_SERVERINFO");
#endif
	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;
#ifdef _XBOX
	XBLog_WriteRingMarker("JA: SV_SpawnServer after serverinfo configstrings");
#endif

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;
	
	// send a heartbeat now so the master will get up to date info
	svs.nextHeartbeatTime = -9999999;

	Hunk_SetMark();
#ifndef _XBOX
	Z_Validate();
	Z_Validate();
	Z_Validate();
#endif

	StopLoadingAnimation();
#ifdef _XBOX
	g_SPXBMapPhase = 25;
	g_SPXBLoadTimes[5] = Sys_Milliseconds() - loadPhaseStart;
	g_SPXBLoadTimes[6] = Sys_Milliseconds() - loadStart;
	g_SPXBLoadTimes[7] = 1;
	XBLog_WriteCriticalf("STEFX_LOAD_TIMES: map=%s prepare=%u collision=%u world=%u game=%u settle=%u total=%u",
		server, g_SPXBLoadTimes[1], g_SPXBLoadTimes[2], g_SPXBLoadTimes[3],
		g_SPXBLoadTimes[4], g_SPXBLoadTimes[5], g_SPXBLoadTimes[6]);
	XBLog_WriteRingMarker("JA: SV_SpawnServer complete");
#endif

	Com_Printf ("-----------------------------------\n");
}

/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_Init (void) {
	XBLog_Write("JA: SV_Init entered");
	SV_AddOperatorCommands ();

	// serverinfo vars
	Cvar_Get ("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM);
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);

	// systeminfo
	Cvar_Get ("helpUsObi", "0", CVAR_SYSTEMINFO );
	sv_serverid = Cvar_Get ("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );

	// server vars
	sv_fps = Cvar_Get ("sv_fps", "20", CVAR_TEMP );
#ifdef _XBOX
	stefx_maxCatchupTicks = Cvar_Get ("stefx_maxCatchupTicks", "1", CVAR_ARCHIVE );
#endif
	sv_timeout = Cvar_Get ("sv_timeout", "120", CVAR_TEMP );
	sv_zombietime = Cvar_Get ("sv_zombietime", "2", CVAR_TEMP );
	Cvar_Get ("nextmap", "", CVAR_TEMP );
	sv_spawntarget = Cvar_Get ("spawntarget", "", 0 );

	sv_reconnectlimit = Cvar_Get ("sv_reconnectlimit", "3", 0);
	sv_showloss = Cvar_Get ("sv_showloss", "0", 0);
	sv_killserver = Cvar_Get ("sv_killserver", "0", 0);
	sv_mapChecksum = Cvar_Get ("sv_mapChecksum", "", CVAR_ROM);
	sv_testsave = Cvar_Get ("sv_testsave", "0", 0);
	sv_compress_saved_games = Cvar_Get ("sv_compress_saved_games", "1", 0);

#if defined(STEFX_ELITE_FORCE_SP)
	G2VertSpaceServer = NULL;
#ifdef _XBOX
	XBLog_Write("STEFX: Ghoul2 server miniheap disabled");
#endif
#else
	// Only allocated once, no point in moving it around and fragmenting
	// create a heap for Ghoul2 to use for game side model vertex transforms used in collision detection
	{
		static CMiniHeap singleton(132096);
		G2VertSpaceServer = &singleton;
	}
#endif
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( char *message ) {
	int			i, j;
	client_t	*cl;
	
	SV_SendServerCommand( NULL, "print \"%s\"", message );
	SV_SendServerCommand( NULL, "disconnect" );

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for (i=0, cl = svs.clients ; i < STEFX_SERVER_CLIENT_SLOTS ; i++, cl++) {
			if (cl->state >= CS_CONNECTED) {
				// force a snapshot to be sent
				cl->nextSnapshotTime = -1;
				SV_SendClientSnapshot( cl );
			}
		}
	}
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( char *finalmsg ) {
	int i;

	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	//Com_Printf( "----- Server Shutdown -----\n" );

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before final message");
#endif
	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before remove commands");
#endif
	SV_RemoveOperatorCommands();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before game shutdown");
#endif
	SV_ShutdownGameProgs(qfalse);

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before snapshot free");
#endif
	if (svs.snapshotEntities)
	{
		Z_Free(svs.snapshotEntities);
		svs.snapshotEntities = NULL;
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before configstrings free");
#endif
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before clear server");
#endif
	// free current level
	memset( &sv, 0, sizeof( sv ) );

	// free server static data
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before client free");
#endif
	if ( svs.clients ) {
		SV_FreeClient(svs.clients);
		Z_Free( svs.clients );
	}
	memset( &svs, 0, sizeof( svs ) );

	// Ensure we free any memory used by the leaf cache.
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before leaf cache free");
#endif
	CM_CleanLeafCache();

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	XBLog_WriteRingMarker("STEFX_VV_SHUTDOWN: before running reset");
#endif
	Cvar_Set( "sv_running", "0" );

	//Com_Printf( "---------------------------\n" );
}
