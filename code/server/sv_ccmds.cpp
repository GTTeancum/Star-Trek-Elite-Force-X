// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"



#include "server.h"
#include "../qcommon/stefx_save_menu.h"
#include "..\game\weapons.h"
#include "..\game\g_items.h"
#include "..\game\statindex.h"
#if defined(STEFX_ELITE_FORCE_SP)
#include "../qcommon/stefx_snapshot_abi.h"
#endif

#ifdef _XBOX
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBMapPhase;
extern "C" volatile unsigned int g_SPXBMapHash;
extern "C" volatile char g_SPXBMapLast[64];

static unsigned int SV_XboxHashText(const char *text)
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

static void SV_XboxCopyLast(volatile char *dest, unsigned int destSize, const char *src)
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


/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

qboolean qbLoadTransition = qfalse;

/*
==================
SV_SetPlayer

Returns the player
==================
*/
static client_t *SV_SetPlayer( void ) {
	client_t	*cl;

	cl = &svs.clients[0];
	if ( !cl->state ) {
		Com_Printf( "Client is not active\n" );
		return NULL;
	}
	return cl;
}


//=========================================================
// don't call this directly, it should only be called from SV_Map_f() or SV_MapTransition_f()
//
static void SV_Map_( ForceReload_e eForceReload ) 
{
	char	*map;
//	char	expanded[MAX_QPATH];

	map = Cmd_Argv(1);
#ifdef _XBOX
	g_SPXBMapPhase = 1;
	g_SPXBMapHash = SV_XboxHashText(map);
	SV_XboxCopyLast(g_SPXBMapLast, sizeof(g_SPXBMapLast), map);
	XBLog_Write(va("JA: SV_Map_: requested '%s'", map ? map : "(null)"));
#endif
	if ( !*map ) {
#ifdef _XBOX
		g_SPXBMapPhase = 2;
		XBLog_Write("JA: SV_Map_: empty map arg, return");
#endif
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	if (strchr (map, '\\') ) {
		Com_Printf ("Can't have mapnames with a \\\n");
		return;
	}

#ifndef _XBOX	// Could check for maps/%s/brushes.mle or something...
	Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
	if ( FS_ReadFile (expanded, NULL) == -1 ) {
		Com_Printf ("Can't find map %s\n", expanded);
		extern	cvar_t	*com_buildScript;
		if (com_buildScript && com_buildScript->integer)
		{//yes, it's happened, someone deleted a map during my build...
			Com_Error( ERR_FATAL, "Can't find map %s\n", expanded );
		}
		return;
	}
#endif

	if (map[0]!='_')
	{
		SG_WipeSavegame("Checkpoint");
	}

#ifdef _XBOX
	g_SPXBMapPhase = 3;
	XBLog_Write("JA: SV_Map_: calling SV_SpawnServer");
#endif
	SV_SpawnServer( map, eForceReload, qtrue );	// start up the map
#ifdef _XBOX
	g_SPXBMapPhase = 4;
	XBLog_Write("JA: SV_Map_: SV_SpawnServer returned");
#endif
}



// Save out some player data for later restore if this is a spawn point with KEEP_PREV (spawnflags&1) set...	
//
// (now also called by auto-save code to setup the cvars correctly
void SV_Player_EndOfLevelSave(void)						   
{
	int	i;	

	// I could just call GetClientState() but that's in sv_bot.cpp, and I'm not sure if that's going to be deleted for
	//	the single player build, so here's the guts again...
	//
	client_t* cl = &svs.clients[0];	// 0 because only ever us as a player	

	if (cl 
		&&
		cl->gentity && cl->gentity->client	// crash fix for voy4->brig transition when you kill Foster. 
											//	Shouldn't happen, but does sometimes...
		)
	{
		Cvar_Set( sCVARNAME_PLAYERSAVE, "");	// default to blank

//		clientSnapshot_t*	pFrame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];
#if defined(STEFX_SP_HOSTED_MP)
		const playerState_t *pState = cl->gentity->client;
#else
		const stefxPlayerState_t *pState = (const stefxPlayerState_t *)cl->gentity->client;
#endif
		const char *s = va("%i %i %i %i %i %f %f %f",
							pState->stats[STAT_HEALTH],
							pState->stats[STAT_ARMOR],
							pState->stats[STAT_WEAPONS],
							pState->weapon,
							pState->weaponstate,
							pState->viewangles[0],
							pState->viewangles[1],
							pState->viewangles[2]
							);
		Cvar_Set( sCVARNAME_PLAYERSAVE, s );

		for (i=0;i< STEFX_PS_MAX_AMMO; i++)
		{
			Cvar_Set( va("playerammo%d", i), va("%i", pState->ammo[i]) );
		}

#if !defined(STEFX_SP_HOSTED_MP)
		for (i=0;i< MAX_WEAPONS; i++)
		{
			Cvar_Set( va("borgadapt%d", i), va("%i", pState->borgAdaptHits[i]) );
		}
#endif
	}
}


// Restart the server on a different map
//
//extern void	SCR_PrecacheScreenshot();  //scr_scrn.cpp
static void SV_MapTransition_f(void)
{		
	char	*spawntarget;

//	SCR_PrecacheScreenshot();
	SV_Player_EndOfLevelSave();

	spawntarget = Cmd_Argv(2);
	if ( *spawntarget != NULL ) 
	{
		Cvar_Set( "spawntarget", spawntarget );
	}
	else
	{
		Cvar_Set( "spawntarget", "" );
	}

	SV_Map_( eForceReload_NOTHING );
}

/*
==================
SV_Map_f

Restart the server on a different map, but clears a cvar so that typing "map blah" doesn't try and preserve
player weapons/ammo/etc from the previous level that you haven't really exited (ie ignores KEEP_PREV on spawn points)
==================
*/
//void SCR_UnprecacheScreenshot();	//scr_scrn.cpp
static void SV_Map_f( void ) 
{
	char commandName[MAX_QPATH];
	Q_strncpyz(commandName, Cmd_Argv(0), sizeof(commandName));

	Cvar_Set( sCVARNAME_PLAYERSAVE, "");
	Cvar_Set( "spawntarget", "" );
	Cvar_Set("tier_storyinfo", "0");
	Cvar_Set("tiers_complete", "");
//	SCR_UnprecacheScreenshot();

	ForceReload_e eForceReload = eForceReload_NOTHING;	// default for normal load

	if ( !Q_stricmp( commandName, "devmapbsp") ) {
		eForceReload = eForceReload_BSP;
	}
	else
	if ( !Q_stricmp( commandName, "devmapmdl") ) {
		eForceReload = eForceReload_MODELS;
	}
	else
	if ( !Q_stricmp( commandName, "devmapall") ) {
		eForceReload = eForceReload_ALL;
	}

	SV_Map_( eForceReload );
#ifdef _XBOX
	g_SPXBMapPhase = 5;
#endif

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( !Q_stricmpn( commandName, "devmap", 6 ) ) {
		Cvar_Set( "helpUsObi", "1" );
#if defined(STEFX_ELITE_FORCE_SP)
		Cvar_Set( "sv_cheats", "1" );
#endif
	} else {
#ifdef _XBOX
		Cvar_Set( "helpUsObi", "1" );
#if defined(STEFX_ELITE_FORCE_SP)
		Cvar_Set( "sv_cheats", "1" );
#endif
#else
		Cvar_Set( "helpUsObi", "0" );
#if defined(STEFX_ELITE_FORCE_SP)
		Cvar_Set( "sv_cheats", "0" );
#endif
#endif
	}
#ifdef _XBOX
	g_SPXBMapPhase = 6;
#endif
}

/*
==================
SV_LoadTransition_f
==================
*/
void SV_LoadTransition_f(void)
{
	char	*map;
	char	*spawntarget;

	map = Cmd_Argv(1);
	if ( !*map ) {
		return;
	}

	qbLoadTransition = qtrue;

//	SCR_PrecacheScreenshot();
	SV_Player_EndOfLevelSave();

	//Save the full current state of the current map so we can return to it later
	SG_WriteSavegame( va("hub/%s", sv_mapname->string), qfalse );

	//set the spawntarget if there is one
	spawntarget = Cmd_Argv(2);
	if ( *spawntarget != NULL ) 
	{
		Cvar_Set( "spawntarget", spawntarget );
	}
	else
	{
		Cvar_Set( "spawntarget", "" );
	}

	if ( !SV_TryLoadTransition( map ) )
	{//couldn't load a savegame
		SV_Map_( eForceReload_NOTHING );
	}
	qbLoadTransition = qfalse;
}
//===============================================================

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int			i, j, l;
	client_t	*cl;
	const char		*s;
	int			ping;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("map: %s\n", sv_mapname->string );

	Com_Printf ("num score ping name            lastmsg address               qport rate\n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for (i=0,cl=svs.clients ; i < STEFX_SERVER_CLIENT_SLOTS ; i++,cl++)
	{
		if (!cl->state)
			continue;
		Com_Printf ("%3i ", i);
		Com_Printf ("%5i ", cl->gentity->client->persistant[PERS_SCORE]);

		if (cl->state == CS_CONNECTED)
			Com_Printf ("CNCT ");
		else if (cl->state == CS_ZOMBIE)
			Com_Printf ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");

		Com_Printf ("%7i ", sv.time - cl->lastPacketTime );

		s = NET_AdrToString( cl->netchan.remoteAddress );
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf (" %5i", cl->rate);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
}

/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	cl = SV_SetPlayer();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}

//===========================================================

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_AddCommand ("devmapbsp", SV_Map_f);
	Cmd_AddCommand ("devmapmdl", SV_Map_f);
	Cmd_AddCommand ("devmapsnd", SV_Map_f);
	Cmd_AddCommand ("devmapall", SV_Map_f);
	Cmd_AddCommand ("maptransition", SV_MapTransition_f);
	Cmd_AddCommand ("load", SV_LoadGame_f);
	Cmd_AddCommand ("loadtransition", SV_LoadTransition_f);
	Cmd_AddCommand ("save", SV_SaveGame_f);
#if defined(STEFX_EF_MENU_SAVE_COMMAND)
	Cmd_AddCommand (STEFX_EF_MENU_SAVE_COMMAND, SV_EFMenuSaveGame_f);
#endif
	Cmd_AddCommand ("wipe", SV_WipeGame_f);

//#ifdef _DEBUG
//	extern void UI_Dump_f(void);
//	Cmd_AddCommand ("ui_dump", UI_Dump_f);
//#endif
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("serverrecord");
	Cmd_RemoveCommand ("serverstop");
	Cmd_RemoveCommand ("sectorlist");
#endif
}

