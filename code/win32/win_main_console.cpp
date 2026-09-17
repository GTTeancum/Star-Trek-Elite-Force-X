#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/stefx_launch.h"
#include "../client/client.h"
#include "win_local.h"
#include "resource.h"
#include <float.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include "../game/g_public.h"
#include <xonline.h>
#include "glw_win_dx8.h"
#include "../qcommon/xb_settings.h"

#ifdef _XBOX
#include <IO.h>
#include <xtl.h>
#include "../win32/xb_log.h"
#define NEWDECL __cdecl
#if defined(_MSC_VER) && !defined(_M_PPC)
extern "C" void *_ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)
#endif
extern "C" volatile unsigned int g_SPXBBootPhase;
extern "C" volatile unsigned int g_SPXBMainLoopCount;
extern "C" volatile unsigned int g_SPXBClsState;
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBComSubphase;
extern "C" volatile unsigned int g_SPXBComFrameCount;
extern "C" volatile unsigned int g_SPXBMainTailStage;
extern "C" volatile unsigned int g_SPXBDirectMapStatus;
extern "C" volatile unsigned int g_SPXBDirectMapHash;
extern "C" volatile unsigned int g_SPXBDirectMapQueuedCount;
extern "C" volatile unsigned int g_SPXBCGameEntryCurrent;
extern "C" volatile unsigned int g_SPXBCGameEntryExpected;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBPerfLoopMsec;
extern "C" volatile unsigned int g_SPXBPerfInputMsec;
extern "C" volatile unsigned int g_SPXBPerfMenuMsec;
#endif

/* NT kernel prototypes for the early main() probe (file-scope required by VC71) */
extern "C" long __stdcall NtCreateFile(void**, unsigned long, void*, void*,
    void*, unsigned long, unsigned long, unsigned long, unsigned long);
extern "C" long __stdcall NtWriteFile(void*, void*, void*, void*, void*,
    void*, unsigned long, void*);
extern "C" long __stdcall NtClose(void*);

#ifndef FINAL_BUILD
#include "dbg_console_xbox.h"
#endif

#endif

int gLaunchController = 0;

extern int eventHead, eventTail;
extern sysEvent_t eventQue[MAX_QUED_EVENTS];
extern byte		sys_packetReceived[MAX_MSGLEN];

#ifdef _XBOX
bool g_xboxDirectMapBootQueued = false;
static bool s_xboxDirectMapMarkerConsumed = false;
static bool s_xboxDirectMapProbeComplete = false;
static bool s_xboxDirectMapProbeResult = false;
static bool s_xboxMenuMapQueued = false;
static char s_xboxMenuMapName[MAX_QPATH];
static char s_xboxMenuMapMode[16];
static int s_xboxMenuMapPlayers = 1;
static int s_xboxMenuMapHumanPlayers = 1;
static int s_xboxMenuMapVirtualControls = 0;
static int s_xboxMenuMapVirtualControlsP1 = 0;

enum stefxLaunchIntent_t
{
	STEFX_LAUNCH_INTENT_NONE = 0,
	STEFX_LAUNCH_INTENT_FRONTEND,
	STEFX_LAUNCH_INTENT_COOP,
	STEFX_LAUNCH_INTENT_HOLOMATCH
};

static int s_stefxLaunchIntent = STEFX_LAUNCH_INTENT_NONE;
typedef char stefxHolomatchLaunchSetupMustFitLaunchData[
	(sizeof(stefxHolomatchLaunchSetup_t) <= (MAX_LAUNCH_DATA_SIZE - 8)) ? 1 : -1];
static stefxHolomatchLaunchSetup_t s_stefxHolomatchLaunchSetup = {
	STEFX_HOLOMATCH_SETUP_MAGIC,
	STEFX_HOLOMATCH_SETUP_VERSION,
	"hm_borg1",
	4,
	1,
	0,
	15,
	0,
	1,
	1,
	0,
	1,
	0,
	0,
	{
		{ "seven", "blue", 0, 1, 1, 1, 1, 0 },
		{ "munro", "default", 0, 1, 1, 1, 1, 0 },
		{ "foster", "default", 0, 1, 1, 1, 1, 0 },
		{ "tuvok", "default", 0, 1, 1, 1, 1, 0 }
	}
};

static int Sys_STEFXClampHolomatchOption(int value, int minimum, int maximum)
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

static bool Sys_STEFXValidHolomatchAssetToken(const char *value)
{
	const unsigned char *cursor = (const unsigned char *)value;

	if (!cursor || !cursor[0])
	{
		return false;
	}
	while (*cursor)
	{
		if (!((*cursor >= 'a' && *cursor <= 'z') ||
			(*cursor >= 'A' && *cursor <= 'Z') ||
			(*cursor >= '0' && *cursor <= '9') ||
			*cursor == '_' || *cursor == '-' || *cursor == '/'))
		{
			return false;
		}
		++cursor;
	}
	return true;
}

static void Sys_STEFXNormalizeHolomatchPlayerSettings(stefxHolomatchLaunchSetup_t *setup)
{
	int player;

	setup->players = Sys_STEFXClampHolomatchOption(setup->players, 1, STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS);
	setup->humanPlayers = Sys_STEFXClampHolomatchOption(setup->humanPlayers, 1, setup->players);
	setup->mapName[MAX_QPATH - 1] = '\0';
	if (!Sys_STEFXValidHolomatchAssetToken(setup->mapName))
	{
		Q_strncpyz(setup->mapName, "hm_borg1", sizeof(setup->mapName));
	}
	setup->fragLimit = Sys_STEFXClampHolomatchOption(setup->fragLimit, 0, 50);
	setup->timeLimit = Sys_STEFXClampHolomatchOption(setup->timeLimit, 0, 30);
	setup->forceRespawn = setup->forceRespawn ? 1 : 0;
	setup->weaponStay = setup->weaponStay ? 1 : 0;
	setup->fallingDamage = setup->fallingDamage ? 1 : 0;
	setup->teamPlay = setup->teamPlay ? 1 : 0;
	setup->friendlyFire = setup->friendlyFire ? 1 : 0;
	for (player = 0; player < STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS; ++player)
	{
		stefxHolomatchPlayerSetup_t *playerSetup = &setup->player[player];

		playerSetup->modelName[MAX_QPATH - 1] = '\0';
		playerSetup->skinName[MAX_QPATH - 1] = '\0';
		if (!Sys_STEFXValidHolomatchAssetToken(playerSetup->modelName))
		{
			Q_strncpyz(playerSetup->modelName, "munro", sizeof(playerSetup->modelName));
		}
		if (!Sys_STEFXValidHolomatchAssetToken(playerSetup->skinName))
		{
			Q_strncpyz(playerSetup->skinName, "default", sizeof(playerSetup->skinName));
		}
		playerSetup->controlStyle = Sys_STEFXClampHolomatchOption(playerSetup->controlStyle, 0, 8);
		playerSetup->autoswitchMode = Sys_STEFXClampHolomatchOption(playerSetup->autoswitchMode, 0, 2);
		playerSetup->autoaimMode = Sys_STEFXClampHolomatchOption(playerSetup->autoaimMode, 0, 3);
		playerSetup->crosshair = Sys_STEFXClampHolomatchOption(playerSetup->crosshair, 1, 12);
		playerSetup->vibration = playerSetup->vibration ? 1 : 0;
		playerSetup->invertPitch = playerSetup->invertPitch ? 1 : 0;
	}
}

static void Sys_STEFXQueueHolomatchPlayerSettings(const stefxHolomatchLaunchSetup_t *setup)
{
	int player;

	for (player = 0; player < setup->players && player < STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS; ++player)
	{
		const stefxHolomatchPlayerSetup_t *playerSetup = &setup->player[player];

		Cbuf_AddText(va("set hm_model_%d %s\n", player, playerSetup->modelName));
		Cbuf_AddText(va("set hm_skin_%d %s\n", player, playerSetup->skinName));
		Cbuf_AddText(va("set cont_config_%d %d\n", player, playerSetup->controlStyle));
		Cbuf_AddText(va("set cg_autoswitch_%d %d\n", player, playerSetup->autoswitchMode));
		Cbuf_AddText(va("set g_autoaim_%d %d\n", player, playerSetup->autoaimMode));
		Cbuf_AddText(va("set cg_drawCrosshair_%d %d\n", player, playerSetup->crosshair));
		Cbuf_AddText(va("set joy_vibestate_%d %d\n", player, playerSetup->vibration));
		Cbuf_AddText(va("set joy_pitchsensitivity_%d %d\n", player, playerSetup->invertPitch ? -1 : 1));
		XBLF("STEFX_HM_PLAYER_HANDOFF_APPLY: player=%d model='%s' skin='%s' control=%d autoswitch=%d autoaim=%d crosshair=%d vibration=%d invert=%d",
			player + 1, playerSetup->modelName, playerSetup->skinName,
			playerSetup->controlStyle, playerSetup->autoswitchMode, playerSetup->autoaimMode,
			playerSetup->crosshair, playerSetup->vibration, playerSetup->invertPitch);
	}
	Cbuf_AddText(va("set model %s/%s\n", setup->player[0].modelName, setup->player[0].skinName));
	Cbuf_AddText(va("set cg_drawCrosshair %d\n", setup->player[0].crosshair));
	Cbuf_AddText(va("set cg_autoswitch %d\n", setup->player[0].autoswitchMode));
}

static unsigned int Sys_XboxDirectMapHashText(const char *text)
{
	unsigned int hash = 2166136261u;
	if (!text)
	{
		return 0;
	}
	while (*text)
	{
		hash ^= (unsigned char)*text++;
		hash *= 16777619u;
	}
	return hash;
}

static bool Sys_XboxReadFirstLineFromPaths(const char **paths, char *out, int outSize, const char **usedPath)
{
	int pathIndex;

	if (outSize <= 0)
	{
		return false;
	}

	out[0] = '\0';
	if (usedPath)
	{
		*usedPath = NULL;
	}

	for (pathIndex = 0; paths[pathIndex]; ++pathIndex)
	{
		FILE *startupMapFile = fopen(paths[pathIndex], "r");
#if defined(STEFX_ELITE_FORCE_SP)
		XBLF("STEFX: direct-map marker fopen path='%s' result=%s errno=%d",
			paths[pathIndex], startupMapFile ? "ok" : "miss", startupMapFile ? 0 : errno);
#endif
		if (startupMapFile)
		{
			if (fgets(out, outSize, startupMapFile))
			{
				out[strcspn(out, "\r\n\t ")] = '\0';
			}
			fclose(startupMapFile);

			if (out[0])
			{
				if (usedPath)
				{
					*usedPath = paths[pathIndex];
				}
#if defined(STEFX_ELITE_FORCE_SP)
				XBLF("STEFX: direct-map marker read path='%s' value='%s'",
					paths[pathIndex], out);
#endif
				g_SPXBDirectMapStatus = 20;
				g_SPXBDirectMapHash = Sys_XboxDirectMapHashText(out);
				return true;
			}
#if defined(STEFX_ELITE_FORCE_SP)
			XBLF("STEFX: direct-map marker empty path='%s'", paths[pathIndex]);
#endif
		}
	}

	return false;
}

static bool Sys_XboxFileExists(const char *path)
{
	FILE *file = fopen(path, "r");
	if (!file)
	{
		return false;
	}
	fclose(file);
	return true;
}

static bool Sys_XboxNormalBootRequested(void)
{
#if defined(STEFX_ELITE_FORCE_SP)
	if (Sys_XboxFileExists("D:\\ef_sp_normal_boot.txt") ||
		Sys_XboxFileExists("d:\\ef_sp_normal_boot.txt"))
	{
		XBL("STEFX: normal boot requested by D:\\ef_sp_normal_boot.txt\n");
		return true;
	}
#endif
	return false;
}

static bool Sys_XboxDirectMapRequestedUncached(void)
{
	char startupMap[MAX_QPATH];
	const char *startupMapPaths[] = {
#if defined(STEFX_ELITE_FORCE_SP)
		"D:\\ef_sp_level.txt",
		"d:\\ef_sp_level.txt",
#endif
		"D:\\ja_sp_level.txt",
		"d:\\ja_sp_level.txt",
		NULL
	};
	startupMap[0] = '\0';

#if defined(STEFX_SP_HOSTED_MP)
	if (s_stefxLaunchIntent == STEFX_LAUNCH_INTENT_HOLOMATCH)
	{
		return true;
	}
#endif

	if (Sys_XboxNormalBootRequested())
	{
		return false;
	}

	if (Sys_XboxReadFirstLineFromPaths(startupMapPaths, startupMap, sizeof(startupMap), NULL))
	{
		return true;
	}

	return false;
}

static bool Sys_XboxDirectMapRequested(void)
{
	if (!s_xboxDirectMapProbeComplete)
	{
		s_xboxDirectMapProbeResult = Sys_XboxDirectMapRequestedUncached();
		s_xboxDirectMapProbeComplete = true;
		XBLF("STEFX: direct-map startup probe result=%d; cached for process lifetime",
			s_xboxDirectMapProbeResult ? 1 : 0);
	}

	return s_xboxDirectMapProbeResult;
}

bool Sys_IsDirectMapBoot(void)
{
	return !s_xboxDirectMapMarkerConsumed &&
		(g_xboxDirectMapBootQueued || Sys_XboxDirectMapRequested());
}

bool Sys_XboxFrontendLaunchIntent(void)
{
	return s_stefxLaunchIntent == STEFX_LAUNCH_INTENT_FRONTEND;
}

void Sys_ClearDirectMapBoot(void)
{
	if (g_xboxDirectMapBootQueued || !s_xboxDirectMapMarkerConsumed)
	{
		XBL("STEFX: direct-map lifecycle cleared; subsequent disconnect may return to frontend\n");
	}
	g_xboxDirectMapBootQueued = false;
	s_xboxDirectMapMarkerConsumed = true;
}

bool Sys_XboxQueueMenuMap(const char *mapName, const char *mode, int players)
{
	g_SPXBDirectMapStatus = 120;
	if (!mapName || !mapName[0] || s_xboxMenuMapQueued)
	{
		g_SPXBDirectMapStatus = 121;
		return false;
	}

	g_SPXBDirectMapStatus = 122;
	Q_strncpyz(s_xboxMenuMapName, mapName, sizeof(s_xboxMenuMapName));
	g_SPXBDirectMapStatus = 123;
	Q_strncpyz(s_xboxMenuMapMode, mode && mode[0] ? mode : "sp", sizeof(s_xboxMenuMapMode));
	g_SPXBDirectMapStatus = 124;
	if (players < 1)
	{
		players = 1;
	}
	else if (players > 4)
	{
		players = 4;
	}
	s_xboxMenuMapPlayers = players;
	if (!Q_stricmp(s_xboxMenuMapMode, "holomatch"))
	{
		s_xboxMenuMapHumanPlayers = Cvar_VariableIntegerValue("stefx_hmHumanPlayers");
		if (s_xboxMenuMapHumanPlayers < 1)
		{
			s_xboxMenuMapHumanPlayers = players;
		}
		else if (s_xboxMenuMapHumanPlayers > players)
		{
			s_xboxMenuMapHumanPlayers = players;
		}
		s_xboxMenuMapVirtualControls = Cvar_VariableIntegerValue("stefx_hm_split_virtual_controls") ? 1 : 0;
		s_xboxMenuMapVirtualControlsP1 = s_xboxMenuMapVirtualControls &&
			Cvar_VariableIntegerValue("stefx_hm_split_virtual_controls_p1") ? 1 : 0;
	}
	else
	{
		s_xboxMenuMapHumanPlayers = players;
		s_xboxMenuMapVirtualControls = 0;
		s_xboxMenuMapVirtualControlsP1 = 0;
	}
	s_xboxMenuMapQueued = true;
	g_xboxDirectMapBootQueued = true;
	s_xboxDirectMapMarkerConsumed = false;
	g_SPXBDirectMapStatus = 125;
	XBLF("STEFX: queued menu map for outside-frame execution map='%s' mode='%s' players=%d humans=%d",
		s_xboxMenuMapName, s_xboxMenuMapMode, s_xboxMenuMapPlayers, s_xboxMenuMapHumanPlayers);
	if (!Q_stricmp(s_xboxMenuMapMode, "holomatch"))
	{
		XBLF("STEFX_HM_SPLIT_LAUNCH: source=queue map='%s' split=%d players=%d mode='%s' localPlayers=%d humans=%d botViews=%d virtual=%d virtualP1=%d",
			s_xboxMenuMapName,
			1,
			s_xboxMenuMapPlayers,
			s_xboxMenuMapMode,
			s_xboxMenuMapPlayers,
			s_xboxMenuMapHumanPlayers,
			s_xboxMenuMapPlayers - s_xboxMenuMapHumanPlayers,
			s_xboxMenuMapVirtualControls,
			s_xboxMenuMapVirtualControlsP1);
	}
	g_SPXBDirectMapStatus = 126;
	g_SPXBDirectMapQueuedCount++;
	return true;
}

static void Sys_XboxExecuteMenuMap(void)
{
	char mapName[MAX_QPATH];
	char mode[16];
	int players;
	int humanPlayers;
	int virtualControls;
	int virtualControlsP1;

	if (!s_xboxMenuMapQueued)
	{
		return;
	}

	Q_strncpyz(mapName, s_xboxMenuMapName, sizeof(mapName));
	Q_strncpyz(mode, s_xboxMenuMapMode, sizeof(mode));
	players = s_xboxMenuMapPlayers;
	humanPlayers = s_xboxMenuMapHumanPlayers;
	virtualControls = s_xboxMenuMapVirtualControls;
	virtualControlsP1 = s_xboxMenuMapVirtualControlsP1;
	s_xboxMenuMapQueued = false;
	s_xboxMenuMapName[0] = '\0';
	s_xboxMenuMapMode[0] = '\0';
	XBLF("STEFX: executing menu map outside Com_Frame map='%s' mode='%s' players=%d humans=%d",
		mapName, mode, players, humanPlayers);
#if defined(STEFX_ELITE_FORCE_SP)
	/*
	 * The frontend deliberately queues map execution outside its draw frame.
	 * Present the load page before the synchronous map command begins so the
	 * user never watches the retired menu sit inert during the first teardown.
	 */
	Cvar_Set("ui_mapname", mapName);
#if defined(STEFX_SP_HOSTED_MP)
	{
		extern void SP_DrawMPLoadScreen(void);
		XBLog_WriteCritical("STEFX_LOAD_PRESENT: queued Holomatch pre-map draw begin");
		SP_DrawMPLoadScreen();
		re.EndFrame(NULL, NULL);
		XBLog_WriteCritical("STEFX_LOAD_PRESENT: queued Holomatch pre-map draw complete");
	}
#else
	{
		extern void SP_PrecacheEFLoadingTitle(void);
		extern void SP_DrawSPLoadScreen(void);
		Cvar_Set("ui_sp_levelname", "");
		SP_PrecacheEFLoadingTitle();
		XBLog_WriteCritical("STEFX_LOAD_PRESENT: queued SP pre-map draw begin");
		SP_DrawSPLoadScreen();
		re.EndFrame(NULL, NULL);
		XBLog_WriteCritical("STEFX_LOAD_PRESENT: queued SP pre-map draw complete");
	}
#endif
#endif
	if (!Q_stricmp(mode, "holomatch"))
	{
		Cvar_Set("sv_maxclients", "8");
		Cvar_Set("bot_enable", "1");
		Cvar_Set("bot_minplayers", va("%d", players + 3));
		Cvar_Set("g_spSkill", players >= 3 ? "2" : "1");
		Cvar_Set("stefx_hmLocalPlayers", va("%d", players));
		Cvar_Set("stefx_hmHumanPlayers", va("%d", humanPlayers));
		Cvar_Set("r_splitScreenEconomy", players >= 2 ? "1" : "0");
		Cvar_Set("stefx_hm_split_virtual_controls", virtualControls ? "1" : "0");
		Cvar_Set("stefx_hm_split_virtual_controls_p1", virtualControlsP1 ? "1" : "0");
	}
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	if (!Q_stricmp(mode, "virtualvoyager"))
	{
		// Match the original StartTour: discard only tour hub snapshots. Manual
		// saves remain available through Load Game, including earlier tours.
		extern void SG_WipeSavegame(const char *name);
		static const char *tourDecks[] = {
			"deck01", "deck02", "deck03", "deck04", "deck05",
			"deck08", "deck09", "deck10", "deck11", "deck15"
		};
		int deck;
		for (deck = 0; deck < 10; ++deck)
		{
			SG_WipeSavegame(va("hub/tour/%s", tourDecks[deck]));
		}
		XBLog_WriteCritical("STEFX_VIRTUAL_VOYAGER: new tour hub reset; manual saves retained");
	}
#endif
	// Tour mode must be visible during game initialization and ICARUS startup.
	Cvar_Get("cg_virtualVoyager", "0", CVAR_SAVEGAME);
	Cvar_Set("cg_virtualVoyager", !Q_stricmp(mode, "virtualvoyager") ? "1" : "0");
	Cbuf_ExecuteText(EXEC_NOW, va("map %s", mapName));

	/* VM cvar registration during map setup restores these defaults.  Reapply the
	   frontend-selected mode after the synchronous map command has returned. */
	Cvar_Set("stefx_splitScreen", !Q_stricmp(mode, "holomatch") || players >= 2 ? "1" : "0");
	Cvar_Set("stefx_splitScreenPlayers", va("%d", players));
	Cvar_Set("stefx_splitScreenMode", !Q_stricmp(mode, "virtualvoyager") ? "sp" : mode);
	Cvar_Set("stefx_splitScreenP2Entity", "-1");
	Cvar_Set("cg_virtualVoyager", !Q_stricmp(mode, "virtualvoyager") ? "1" : "0");
	if (!Q_stricmp(mode, "holomatch"))
	{
		Cvar_Set("bot_enable", "1");
		Cvar_Set("bot_minplayers", va("%d", players + 3));
		Cvar_Set("g_spSkill", players >= 3 ? "2" : "1");
		Cvar_Set("stefx_hmLocalPlayers", va("%d", players));
		Cvar_Set("stefx_hmHumanPlayers", va("%d", humanPlayers));
		Cvar_Set("r_splitScreenEconomy", players >= 2 ? "1" : "0");
		Cvar_Set("stefx_hm_split_virtual_controls", virtualControls ? "1" : "0");
		Cvar_Set("stefx_hm_split_virtual_controls_p1", virtualControlsP1 ? "1" : "0");
	}
	XBLF("STEFX: menu map outside-frame execution returned map='%s' split=%d players=%d mode='%s'",
		mapName,
		Cvar_VariableIntegerValue("stefx_splitScreen"),
		Cvar_VariableIntegerValue("stefx_splitScreenPlayers"),
		Cvar_VariableString("stefx_splitScreenMode"));
	if (!Q_stricmp(mode, "holomatch"))
	{
		XBLF("STEFX_HM_SPLIT_LAUNCH: source=execute map='%s' split=%d players=%d mode='%s' localPlayers=%d humans=%d botViews=%d virtual=%d virtualP1=%d economy=%d",
			mapName,
			Cvar_VariableIntegerValue("stefx_splitScreen"),
			Cvar_VariableIntegerValue("stefx_splitScreenPlayers"),
			Cvar_VariableString("stefx_splitScreenMode"),
			Cvar_VariableIntegerValue("stefx_hmLocalPlayers"),
			Cvar_VariableIntegerValue("stefx_hmHumanPlayers"),
			players - humanPlayers,
			Cvar_VariableIntegerValue("stefx_hm_split_virtual_controls"),
			Cvar_VariableIntegerValue("stefx_hm_split_virtual_controls_p1"),
			Cvar_VariableIntegerValue("r_splitScreenEconomy"));
	}
}

static bool Sys_XboxQueueDirectMapBoot(void)
{
	char startupMap[MAX_QPATH];
	const char *startupMapSource = NULL;
	bool builtInLaunchIntent = false;
	bool useDevMap = true;
	bool treatAsDirectMap = true;
	const char *startupMapPaths[] = {
#if defined(STEFX_ELITE_FORCE_SP)
		"D:\\ef_sp_level.txt",
		"d:\\ef_sp_level.txt",
#endif
		"D:\\ja_sp_level.txt",
		"d:\\ja_sp_level.txt",
		NULL
	};
	const char *startupCommandPaths[] = {
#if defined(STEFX_ELITE_FORCE_SP)
		"D:\\ef_sp_commands.txt",
		"d:\\ef_sp_commands.txt",
#endif
		"D:\\ja_sp_commands.txt",
		"d:\\ja_sp_commands.txt",
		NULL
	};
	const char *postMapCommandPaths[] = {
#if defined(STEFX_ELITE_FORCE_SP)
		"D:\\ef_sp_postmap_commands.txt",
		"d:\\ef_sp_postmap_commands.txt",
#endif
		NULL
	};
	int startupCommandPathIndex;
	int postMapCommandPathIndex;
	startupMap[0] = '\0';

#if defined(STEFX_SP_HOSTED_MP)
	if (s_stefxLaunchIntent == STEFX_LAUNCH_INTENT_HOLOMATCH)
	{
		Q_strncpyz(startupMap, s_stefxHolomatchLaunchSetup.mapName, sizeof(startupMap));
		startupMapSource = "XBE Holomatch launch intent";
		builtInLaunchIntent = true;
		useDevMap = false;
		Cbuf_AddText("set fs_game BaseEF\n");
		Cbuf_AddText("set stefx_splitScreen 1\n");
		Cbuf_AddText(va("set stefx_splitScreenPlayers %d\n", s_stefxHolomatchLaunchSetup.players));
		Cbuf_AddText("set stefx_splitScreenMode holomatch\n");
		Cbuf_AddText(va("set stefx_hmLocalPlayers %d\n", s_stefxHolomatchLaunchSetup.players));
		Cbuf_AddText(va("set stefx_hmHumanPlayers %d\n", s_stefxHolomatchLaunchSetup.humanPlayers));
		Cbuf_AddText(va("set r_splitScreenEconomy %d\n", s_stefxHolomatchLaunchSetup.players >= 2 ? 1 : 0));
		Cbuf_AddText(va("set stefx_hm_split_virtual_controls %d\n", s_stefxHolomatchLaunchSetup.diagnosticVirtualControls ? 1 : 0));
		Cbuf_AddText(va("set stefx_hm_split_virtual_controls_p1 %d\n", s_stefxHolomatchLaunchSetup.diagnosticVirtualControlsP1 ? 1 : 0));
		/* A direct hardware bootstrap must never inherit input/render proof cvars
		 * from an earlier emulator configuration. P2-P4 are real game bots; P1
		 * must remain exclusively owned by the physical primary controller. */
		Cbuf_AddText("set stefx_splitScreenTestP2Pad 0\n");
		Cbuf_AddText("set stefx_splitScreenTestP2Input 0\n");
		Cbuf_AddText("set stefx_smoke_input 0\n");
		Cbuf_AddText("set stefx_hm_smoke_pin_p1_view 0\n");
		Cbuf_AddText("set stefx_hm_smoke_combat_proof 0\n");
		Cbuf_AddText("set stefx_hm_smoke_mapcycle 0\n");
		Cbuf_AddText("set stefx_hm_split_overlay_proof 0\n");
		Cbuf_AddText("set stefx_hm_split_phaser_proof 0\n");
		Cbuf_AddText("set stefx_hm_split_weapon_proof 0\n");
		Cbuf_AddText("set stefx_hm_launch_source xbe\n");
		Cbuf_AddText("set stefx_splitScreenP2Entity -1\n");
		Sys_STEFXQueueHolomatchPlayerSettings(&s_stefxHolomatchLaunchSetup);
		Cbuf_AddText("set sv_maxclients 8\n");
		Cbuf_AddText(va("set g_gametype %d\n", s_stefxHolomatchLaunchSetup.teamPlay ? 3 : 0));
		Cbuf_AddText(va("set fraglimit %d\n", s_stefxHolomatchLaunchSetup.fragLimit));
		Cbuf_AddText(va("set timelimit %d\n", s_stefxHolomatchLaunchSetup.timeLimit));
		Cbuf_AddText(va("set g_forcerespawn %d\n", s_stefxHolomatchLaunchSetup.forceRespawn ? 20 : 0));
		Cbuf_AddText(va("set stefx_hm_weapon_stay %d\n", s_stefxHolomatchLaunchSetup.weaponStay ? 1 : 0));
		Cbuf_AddText(va("set dmflags %d\n", s_stefxHolomatchLaunchSetup.fallingDamage ? 0 : 8));
		Cbuf_AddText(va("set g_friendlyFire %d\n", s_stefxHolomatchLaunchSetup.friendlyFire ? 1 : 0));
		/* The original PC holodeck entrance intentionally freezes a non-bot for
		 * TIME_INTRO (five seconds).  In local split-screen that looks and feels
		 * like controller rollback while the bot-owned panels are already live.
		 * Console local play starts every 3P/4P viewport immediately instead. */
		Cbuf_AddText("set g_holoIntro 0\n");
		Cbuf_AddText("set bot_enable 1\n");
		/* The direct one-pad hardware build assigns the missing viewport owners to
		 * real game bots.  Do not also append the normal three opponent bots: that
		 * made a 1H/4V proof simulate seven actors even though the requested test
		 * population is P1 plus bot-owned P2-P4.  The same expression naturally
		 * gives a 1H/3V proof exactly two bot-owned secondary viewports. */
		Cbuf_AddText(va("set bot_minplayers %d\n", s_stefxHolomatchLaunchSetup.players));
		Cbuf_AddText("set g_spSkill 2\n");
		XBL("STEFX: applying Holomatch XBE launch intent\n");
		XBLF("STEFX_HM_BOT_POPULATION: source=xbe viewports=%d humans=%d botViews=%d minPlayers=%d extraBots=0",
			s_stefxHolomatchLaunchSetup.players,
			s_stefxHolomatchLaunchSetup.humanPlayers,
			s_stefxHolomatchLaunchSetup.players - s_stefxHolomatchLaunchSetup.humanPlayers,
			s_stefxHolomatchLaunchSetup.players);
		XBL("STEFX_HM_INTRO_POLICY: source=xbe holoIntro=0 immediateLocalInput=1 applies3P=1 applies4P=1\n");
		XBLF("STEFX_HM_SPLIT_LAUNCH: source=xbe map='%s' split=1 players=%d mode='holomatch' localPlayers=%d humans=%d botViews=%d virtual=%d virtualP1=%d economy=%d frag=%d time=%d force=%d stay=%d falling=%d team=%d friendly=%d",
			startupMap,
			s_stefxHolomatchLaunchSetup.players,
			s_stefxHolomatchLaunchSetup.players,
			s_stefxHolomatchLaunchSetup.humanPlayers,
			s_stefxHolomatchLaunchSetup.players - s_stefxHolomatchLaunchSetup.humanPlayers,
			s_stefxHolomatchLaunchSetup.diagnosticVirtualControls ? 1 : 0,
			s_stefxHolomatchLaunchSetup.diagnosticVirtualControlsP1 ? 1 : 0,
			s_stefxHolomatchLaunchSetup.players >= 2 ? 1 : 0,
			s_stefxHolomatchLaunchSetup.fragLimit,
			s_stefxHolomatchLaunchSetup.timeLimit,
			s_stefxHolomatchLaunchSetup.forceRespawn,
			s_stefxHolomatchLaunchSetup.weaponStay,
			s_stefxHolomatchLaunchSetup.fallingDamage,
			s_stefxHolomatchLaunchSetup.teamPlay,
			s_stefxHolomatchLaunchSetup.friendlyFire);
	}
#else
	if (s_stefxLaunchIntent == STEFX_LAUNCH_INTENT_COOP)
	{
		Q_strncpyz(startupMap, "borg1", sizeof(startupMap));
		startupMapSource = "XBE cooperative launch intent";
		builtInLaunchIntent = true;
		useDevMap = false;
		Cbuf_AddText("set stefx_splitScreen 1\n");
		Cbuf_AddText("set stefx_splitScreenPlayers 2\n");
		Cbuf_AddText("set stefx_splitScreenMode coop\n");
		Cbuf_AddText("set r_splitScreenEconomy 1\n");
		Cbuf_AddText("set stefx_splitScreenP2Entity -1\n");
		Cbuf_AddText("set cg_virtualVoyager 0\n");
		XBL("STEFX: applying cooperative XBE launch intent\n");
	}
#endif

	if (!startupMap[0] && Sys_XboxNormalBootRequested())
	{
		g_SPXBDirectMapStatus = 5;
		XBL("JA: direct-map boot: disabled for normal EF story boot\n");
		return false;
	}

	g_SPXBDirectMapStatus = 10;
	if (!startupMap[0])
	{
		Sys_XboxReadFirstLineFromPaths(startupMapPaths, startupMap, sizeof(startupMap), &startupMapSource);
	}

#if defined(STEFX_ELITE_FORCE_SP)
	if (!startupMap[0])
	{
		g_SPXBDirectMapStatus = 30;
		XBL("STEFX: direct-map boot: no explicit level file, normal EF frontend boot\n");
		return false;
	}
#endif

	for (startupCommandPathIndex = 0;
		!builtInLaunchIntent && startupCommandPaths[startupCommandPathIndex];
		++startupCommandPathIndex)
	{
		FILE *startupCommandFile = fopen(startupCommandPaths[startupCommandPathIndex], "r");
#if defined(STEFX_ELITE_FORCE_SP)
		XBLF("STEFX: direct-map startup command fopen path='%s' result=%s errno=%d",
			startupCommandPaths[startupCommandPathIndex], startupCommandFile ? "ok" : "miss",
			startupCommandFile ? 0 : errno);
#endif
		if (startupCommandFile)
		{
			char commandLine[1024];
			g_SPXBDirectMapStatus = 40;
			while (fgets(commandLine, sizeof(commandLine), startupCommandFile))
			{
				commandLine[strcspn(commandLine, "\r\n")] = '\0';
				if (commandLine[0])
				{
					XBLF("JA: direct-map boot: queue startup command '%s' from %s", commandLine, startupCommandPaths[startupCommandPathIndex]);
					Cbuf_AddText(commandLine);
					Cbuf_AddText("\n");
				}
			}
			fclose(startupCommandFile);
		}
	}

	if (!startupMap[0])
	{
		g_SPXBDirectMapStatus = 31;
		XBL("JA: direct-map boot: no ja_sp_level.txt map, normal boot\n");
		return false;
	}

	g_SPXBDirectMapStatus = 50;
	g_SPXBDirectMapHash = Sys_XboxDirectMapHashText(startupMap);
	XBLF("JA: startup map boot: queue %s %s before first Com_Frame source=%s",
		useDevMap ? "devmap" : "map",
		startupMap,
		startupMapSource ? startupMapSource : "<unknown>");
	Cbuf_AddText(va("%s %s\n", useDevMap ? "devmap" : "map", startupMap));
	for (postMapCommandPathIndex = 0;
		!builtInLaunchIntent && postMapCommandPaths[postMapCommandPathIndex];
		++postMapCommandPathIndex)
	{
		FILE *postMapCommandFile = fopen(postMapCommandPaths[postMapCommandPathIndex], "r");
#if defined(STEFX_ELITE_FORCE_SP)
		XBLF("STEFX: direct-map post command fopen path='%s' result=%s errno=%d",
			postMapCommandPaths[postMapCommandPathIndex], postMapCommandFile ? "ok" : "miss",
			postMapCommandFile ? 0 : errno);
#endif
		if (postMapCommandFile)
		{
			char commandLine[1024];
			g_SPXBDirectMapStatus = 55;
			while (fgets(commandLine, sizeof(commandLine), postMapCommandFile))
			{
				commandLine[strcspn(commandLine, "\r\n")] = '\0';
				if (commandLine[0])
				{
					XBLF("JA: direct-map boot: queue post-map command '%s' from %s", commandLine, postMapCommandPaths[postMapCommandPathIndex]);
					Cbuf_AddText(commandLine);
					Cbuf_AddText("\n");
				}
			}
			fclose(postMapCommandFile);
			// D: and d: are fallback spellings of the same Xbox file.
			// Queue the diagnostic command file only once.
			break;
		}
	}
	if (treatAsDirectMap)
	{
		g_xboxDirectMapBootQueued = true;
		g_SPXBDirectMapQueuedCount++;
	}
	g_SPXBDirectMapStatus = 60;
	XBL("JA: direct-map boot: startup/map commands queued for post-init execution\n");
	return true;
}
#endif

void *NEWDECL operator new(size_t size)
{
#if defined(_XBOX)
	void *retaddr = NULL;
#if defined(_MSC_VER) && !defined(_M_PPC)
	retaddr = _ReturnAddress();
#endif
	if (size >= (16 * 1024 * 1024))
	{
		char msg[160];
		_snprintf(msg, sizeof(msg) - 1, "EFALLOC: operator new size=%u phase=%u caller=%p\n", (unsigned int)size, (unsigned int)g_SPXBBootPhase, retaddr);
		msg[sizeof(msg) - 1] = '\0';
		XBLog_Print(msg);
	}
#endif
	return Z_Malloc(size, TAG_NEWDEL, qfalse);
}


void *NEWDECL operator new[](size_t size)
{
#if defined(_XBOX)
	void *retaddr = NULL;
#if defined(_MSC_VER) && !defined(_M_PPC)
	retaddr = _ReturnAddress();
#endif
	if (size >= (16 * 1024 * 1024))
	{
		char msg[160];
		_snprintf(msg, sizeof(msg) - 1, "EFALLOC: operator new[] size=%u phase=%u caller=%p\n", (unsigned int)size, (unsigned int)g_SPXBBootPhase, retaddr);
		msg[sizeof(msg) - 1] = '\0';
		XBLog_Print(msg);
	}
#endif
	return Z_Malloc(size, TAG_NEWDEL, qfalse);
}


void NEWDECL operator delete[](void *ptr)
{
	if (ptr)
		Z_Free(ptr);
}


void NEWDECL operator delete(void *ptr)
{
	if (ptr)
		Z_Free(ptr);
}

/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/
extern void Sys_In_Restart_f(void);
extern void Sys_Net_Restart_f(void);
void Sys_Init( void ) 
{
	Cmd_AddCommand ("in_restart", Sys_In_Restart_f);
	Cmd_AddCommand ("net_restart", Sys_Net_Restart_f);
}

#ifdef XBOX_DEMO
// When we're a demo, we're not running from D:\, so we need some hacks:
char demoBasePath[64];
#endif

char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

#ifdef XBOX_DEMO
	strcpy( cwd, demoBasePath );
#else
	strcpy(cwd, "d:");
#endif

	return cwd;
}

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
void Sys_In_Restart_f( void ) {
}



/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void Sys_Error( const char *error, ... ) {
        va_list                argptr;
        char                text[256];

        va_start (argptr, error);
        vsprintf (text, error, argptr);
        va_end (argptr);

#ifdef _GAMECUBE
        printf(text);
#else
        OutputDebugString(text);
#endif
#ifdef _XBOX
        XBLF("JA: Sys_Error exit: %s", text);
#endif

#if 0 // UN-PORT
        Com_ShutdownZoneMemory();
        Com_ShutdownHunkMemory();
#endif

        exit (1);
}


/*
================
Sys_GetEvent

================
*/
sysEvent_t Sys_GetEvent( void ) {
        sysEvent_t        ev;

        // return if we have data
        if ( eventHead > eventTail ) {
                eventTail++;
                return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
        }

        // check for network packets
        msg_t                netmsg;
        MSG_Init( &netmsg, sys_packetReceived, sizeof( sys_packetReceived ) );

        // return if we have data
        if ( eventHead > eventTail ) {
                eventTail++;
                return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
        }

        // create an empty event to return
        memset( &ev, 0, sizeof( ev ) );
        ev.evTime = Sys_Milliseconds();

        return ev;
}


void Sys_Print(const char *msg)
{
#ifdef _GAMECUBE
	printf(msg);
#elif defined(_XBOX)
	XBLog_Write(msg);
#else
	OutputDebugString(msg);
#endif
}

/*
==============
Sys_Log
==============
*/
void Sys_Log( const char *file, const char *msg ) {
	Sys_Log(file, msg, strlen(msg), strchr(msg, '\n') ? true : false);
}

/*
==============
Sys_Log
==============
*/
void Sys_Log( const char *file, const void *buffer, int size, bool flush ) {
#ifndef FINAL_BUILD
	static bool unableToLog = false;

	// Once we've failed to write to the log files once, bail out.
	// This lets us put release builds on DVD without recompiling.
	if (unableToLog)
		return;

	struct FileInfo
	{
		char name[MAX_QPATH];
		FILE *handle;
	};

	const int LOG_MAX_FILES = 4;
	static FileInfo files[LOG_MAX_FILES];
	static int num_files = 0;

	FileInfo* cur = NULL;
	for (int f = 0; f < num_files; ++f)
	{
		if (!stricmp(file, files[f].name))
		{
			cur = &files[f];
			break;
		}
	}

	if (cur == NULL)
	{
		if (num_files >= LOG_MAX_FILES)
		{
			Sys_Print("Too many log files!\n");
			return;
		}

		cur = &files[num_files++];
		strcpy(cur->name, file);
		cur->handle = NULL;
	}

	char fullname[MAX_QPATH];
	sprintf(fullname, "d:\\%s", cur->name);
	if (!cur->handle)
	{
		cur->handle = fopen(fullname, "wb");
		if (cur->handle == NULL)
		{
			Sys_Print("Unable to open log file!\n");
			unableToLog = true;
			return;
		}
	}

	if (size == 1) fputc(*(char*)buffer, cur->handle);
	else fwrite(buffer, size, 1, cur->handle);

	if (flush)
	{
		fflush(cur->handle);
	}
#endif
}

#ifdef _XBOX
HANDLE Sys_FileStreamMutex = INVALID_HANDLE_VALUE;
#endif

void Win_Init(void)
{
#ifdef _XBOX
	XBL("Win_Init: CreateMutex...\n");
	Sys_FileStreamMutex = CreateMutex(NULL, FALSE, NULL);
	XBLF("Win_Init: mutex handle=0x%x\n", (unsigned)Sys_FileStreamMutex);
#endif
}

/*
=====================

XBE SWITCHING SUPPORT

=====================
*/

#ifdef XBOX_DEMO
// Filled in when we're launched from CDX
LD_DEMO demoLaunchData;
bool demoLaunchDataValid = false;

// If we were launched by the user, or someone has pressed a key
// then the timer should not count down during movies/loading:
bool demoTimerAlways = false;
int demoTimer = 0;
#endif

// Takes a filename (relative to ".") and pre-pends the right path. This
// is needed for demos where the game won't be running from D:
const char *Sys_RemapPath( const char *filename )
{
#ifdef XBOX_DEMO
	return va( "%s\\%s", demoBasePath, filename );
#else
	return va( "D:\\%s", filename );
#endif
}

// Despite what you may think, this function actually just returns
// a value telling you if you *should* quick-boot -- ie skip intro
// cinematics and such. Only supposed to XGetLaunchInfo once per
// boot, so we cache the results.
//
// This function always gets called at startup, so we also use it
// to retrieve the launch info for a demo
#define LAUNCH_MAGIC "J3D1"
bool Sys_QuickStart( void )
{
	static bool retVal = false;
	static bool initialized = false;

	if( initialized )
		return retVal;

	initialized = true;

#ifdef XBOX_DEMO
	// Default to D:\, this gets replaced below if CDX started us
	strcpy( demoBasePath, "D:" );

	gLaunchController = 0;	// Irrelevant in demo
	retVal = false;			// We never come from MP (eg), so always false
	DWORD launchType;

	DWORD result = XGetLaunchInfo( &launchType, (LAUNCH_DATA *) &demoLaunchData );
	if( result == ERROR_SUCCESS && launchType == LDT_TITLE )
	{
		// We were launched by CDX:
		demoLaunchDataValid = true;

		// How we were launched affects timer behavior:
		demoTimerAlways = (demoLaunchData.dwRunmode == XLDEMO_RUNMODE_KIOSKMODE);

		// Need to re-map paths, as D:\\ doesn't work now:
		Q_strncpyz( demoBasePath, demoLaunchData.szLaunchedXBE, sizeof(demoBasePath), qtrue );

		// Find our executable name in the path, and truncate the string there:
		char *pXBE = strstr( demoBasePath, "\\default.xbe" );
		if( !pXBE )
			Com_Error( ERR_FATAL, "Error re-mapping D drive\n" );
		*pXBE = 0;

		// Fix the video path:
		extern char XBOX_VIDEO_PATH[64];
		strcpy( XBOX_VIDEO_PATH, demoBasePath );
#if defined(STEFX_ELITE_FORCE_SP)
		strcat( XBOX_VIDEO_PATH, "\\BaseEF\\video\\" );
#else
		strcat( XBOX_VIDEO_PATH, "\\base\\video\\" );
#endif
	}

	return retVal;
#else
	DWORD launchType;
	LAUNCH_DATA ld;

	if( (XGetLaunchInfo( &launchType, &ld ) != ERROR_SUCCESS) ||
		(launchType != LDT_TITLE) ||
		memcmp(&ld.Data[1], LAUNCH_MAGIC, 4) )
	{
#if defined(STEFX_SP_HOSTED_MP)
		char explicitMap[MAX_QPATH];
		const char *explicitMapPaths[] = {
			"D:\\ef_sp_level.txt",
			"d:\\ef_sp_level.txt",
			"D:\\ja_sp_level.txt",
			"d:\\ja_sp_level.txt",
			NULL
		};
		explicitMap[0] = '\0';
		if (!Sys_XboxNormalBootRequested() &&
			!Sys_XboxReadFirstLineFromPaths(explicitMapPaths, explicitMap, sizeof(explicitMap), NULL))
		{
			/* A production efmp.xbe boot without validated launch data belongs to
			 * the frontend.  Automated and one-pad diagnostics must opt in with an
			 * explicit map/command marker, rather than changing the retail path. */
			XBL("STEFX_HM_DIRECT_BOOT: no handoff or marker; production frontend path\n");
		}
#endif
		return (retVal = false);
	}
	
	gLaunchController = ld.Data[0];
	s_stefxLaunchIntent = ld.Data[6];
	if (s_stefxLaunchIntent == STEFX_LAUNCH_INTENT_HOLOMATCH)
	{
		const stefxHolomatchLaunchSetup_t *setup = (const stefxHolomatchLaunchSetup_t *)&ld.Data[8];
		if (setup->magic == STEFX_HOLOMATCH_SETUP_MAGIC &&
			setup->version == STEFX_HOLOMATCH_SETUP_VERSION &&
			setup->mapName[0] && setup->players >= 1 && setup->players <= 4 &&
			setup->humanPlayers >= 1 && setup->humanPlayers <= setup->players)
		{
			memcpy(&s_stefxHolomatchLaunchSetup, setup, sizeof(s_stefxHolomatchLaunchSetup));
			Sys_STEFXNormalizeHolomatchPlayerSettings(&s_stefxHolomatchLaunchSetup);
			s_stefxHolomatchLaunchSetup.diagnosticVirtualControls =
				s_stefxHolomatchLaunchSetup.diagnosticVirtualControls ? 1 : 0;
			s_stefxHolomatchLaunchSetup.diagnosticVirtualControlsP1 =
				s_stefxHolomatchLaunchSetup.diagnosticVirtualControls &&
				s_stefxHolomatchLaunchSetup.diagnosticVirtualControlsP1 ? 1 : 0;
			XBLF("STEFX_HM_MENU_HANDOFF: accepted version=%d bytes=%u map='%s' players=%d humans=%d botViews=%d frag=%d time=%d force=%d stay=%d falling=%d team=%d friendly=%d virtual=%d virtualP1=%d",
				s_stefxHolomatchLaunchSetup.version,
				(unsigned int)sizeof(s_stefxHolomatchLaunchSetup),
				s_stefxHolomatchLaunchSetup.mapName,
				s_stefxHolomatchLaunchSetup.players,
				s_stefxHolomatchLaunchSetup.humanPlayers,
				s_stefxHolomatchLaunchSetup.players - s_stefxHolomatchLaunchSetup.humanPlayers,
				s_stefxHolomatchLaunchSetup.fragLimit,
				s_stefxHolomatchLaunchSetup.timeLimit,
				s_stefxHolomatchLaunchSetup.forceRespawn,
				s_stefxHolomatchLaunchSetup.weaponStay,
				s_stefxHolomatchLaunchSetup.fallingDamage,
				s_stefxHolomatchLaunchSetup.teamPlay,
				s_stefxHolomatchLaunchSetup.friendlyFire,
				s_stefxHolomatchLaunchSetup.diagnosticVirtualControls,
				s_stefxHolomatchLaunchSetup.diagnosticVirtualControlsP1);
			{
				int player;
				for (player = 0; player < s_stefxHolomatchLaunchSetup.players; ++player)
				{
					const stefxHolomatchPlayerSetup_t *playerSetup = &s_stefxHolomatchLaunchSetup.player[player];
					XBLF("STEFX_HM_PLAYER_HANDOFF: player=%d model='%s' skin='%s' control=%d autoswitch=%d autoaim=%d crosshair=%d vibration=%d invert=%d",
						player + 1, playerSetup->modelName, playerSetup->skinName,
						playerSetup->controlStyle, playerSetup->autoswitchMode, playerSetup->autoaimMode,
						playerSetup->crosshair, playerSetup->vibration, playerSetup->invertPitch);
				}
			}
		}
		else
		{
			/* Do not turn corrupt or version-mismatched menu data into the old
			 * one-pad test bootstrap.  Fail safely into the shared frontend so the
			 * user can make a fresh, explicit Holomatch selection. */
			s_stefxLaunchIntent = STEFX_LAUNCH_INTENT_FRONTEND;
			XBL("STEFX_HM_MENU_HANDOFF: rejected invalid setup; returning to frontend\n");
		}
	}
	XBLF("STEFX: XBE launch handoff controller=%d intent=%d savingDisabled=%d",
		gLaunchController,
		s_stefxLaunchIntent,
		ld.Data[5] == 0x42 ? 1 : 0);

	// Magic number to disable settings/saving
	if( ld.Data[5] == 0x42 )
		Settings.Disable();

	return (retVal = true);
#endif
}

extern int IN_GetMainController(void);
extern void SP_DrawMPLoadScreen(void);

// Takes an extra parameter so that the accepted invite code can pass
// in the XONLINE_ACCEPTED_INVITE to be copied into launch data.
//
// For the demo, the only valid reason is "demo", and pData should be NULL
void Sys_Reboot( const char *reason, const void *pData )
{
#ifdef XBOX_DEMO
	if( pData || !demoLaunchDataValid || Q_stricmp( reason, "demo" ) != 0 )
		Com_Error( ERR_DROP, "Invalid Sys_Reboot call\n" );

	// Kill off the sound and stream threads:
	S_Shutdown();
	extern void Sys_StreamShutdown(void);
	Sys_StreamShutdown();

	// Now return to CDX
	XLaunchNewImage( demoLaunchData.szLauncherXBE, (LAUNCH_DATA *) &demoLaunchData );
	// Should never return!
#else
	LAUNCH_DATA ld;
	const char *path = NULL;
	int controller;

	memset( &ld, 0, sizeof(ld) );
	controller = IN_GetMainController();
	ld.Data[0] = (byte) controller;
	memcpy(&ld.Data[1], LAUNCH_MAGIC, 4);
	Com_Printf("\tController %d Passed\n",controller); 

	if (!Q_stricmp(reason, "multiplayer"))
	{
		path = "d:\\efmp.xbe";
		ld.Data[6] = STEFX_LAUNCH_INTENT_HOLOMATCH;
		SP_DrawMPLoadScreen();

		// Set a magic number if saving is disabled
		if( Settings.IsDisabled() )
			ld.Data[5] = 0x42;

		// Flag that there is no invite in the launch data:
		ld.Data[7] = 0;
		if (pData)
		{
			const stefxHolomatchLaunchSetup_t *setup = (const stefxHolomatchLaunchSetup_t *)pData;
			if (setup->magic == STEFX_HOLOMATCH_SETUP_MAGIC && setup->version == STEFX_HOLOMATCH_SETUP_VERSION)
			{
				memcpy(&ld.Data[8], setup, sizeof(*setup));
				XBLF("STEFX_HM_MENU_HANDOFF: queued version=%d bytes=%u map='%s' players=%d humans=%d botViews=%d frag=%d time=%d force=%d stay=%d falling=%d team=%d friendly=%d virtual=%d virtualP1=%d",
					setup->version, (unsigned int)sizeof(*setup), setup->mapName,
					setup->players, setup->humanPlayers, setup->players - setup->humanPlayers,
					setup->fragLimit, setup->timeLimit,
					setup->forceRespawn, setup->weaponStay, setup->fallingDamage,
					setup->teamPlay, setup->friendlyFire,
					setup->diagnosticVirtualControls ? 1 : 0,
					setup->diagnosticVirtualControlsP1 ? 1 : 0);
				{
					int player;
					for (player = 0; player < setup->players && player < STEFX_HOLOMATCH_MAX_LOCAL_PLAYERS; ++player)
					{
						const stefxHolomatchPlayerSetup_t *playerSetup = &setup->player[player];
						XBLF("STEFX_HM_PLAYER_HANDOFF_QUEUE: player=%d model='%s' skin='%s' control=%d autoswitch=%d autoaim=%d crosshair=%d vibration=%d invert=%d",
							player + 1, playerSetup->modelName, playerSetup->skinName,
							playerSetup->controlStyle, playerSetup->autoswitchMode, playerSetup->autoaimMode,
							playerSetup->crosshair, playerSetup->vibration, playerSetup->invertPitch);
					}
				}
			}
		}
	}
	else if (!Q_stricmp(reason, "singleplayer") ||
		!Q_stricmp(reason, "singleplayer_coop"))
	{
		path = "d:\\default.xbe";
		ld.Data[6] = !Q_stricmp(reason, "singleplayer_coop")
			? STEFX_LAUNCH_INTENT_COOP
			: STEFX_LAUNCH_INTENT_FRONTEND;
		SP_DrawMPLoadScreen();

		if( Settings.IsDisabled() )
			ld.Data[5] = 0x42;
	}
	else if (!Q_stricmp(reason, "invite"))
	{
		path = "d:\\efmp.xbe";
		ld.Data[6] = STEFX_LAUNCH_INTENT_HOLOMATCH;
		SP_DrawMPLoadScreen();

		// Set a magic number if saving is disabled
		if( Settings.IsDisabled() )
			ld.Data[5] = 0x42;

		// Flag that we're including an invite with the launch data:
		ld.Data[7] = 1;

		memcpy( &ld.Data[8], pData, sizeof(XONLINE_ACCEPTED_GAMEINVITE) );
	}
	else
	{
		Com_Error( ERR_FATAL, "Unknown reboot code %s\n", reason );
	}

	// Title should not be doing ANYTHING in the background.
	// Shutting down sound ensures that the sound thread is gone
	S_Shutdown();
	// Similarly, kill off the streaming thread
	extern void Sys_StreamShutdown(void);
	Sys_StreamShutdown();

	// Keep the loading screen up while we reboot!
	glw_state->device->PersistDisplay();

	XBLF("STEFX: XBE handoff reason='%s' path='%s' intent=%d controller=%d",
		reason,
		path,
		(int)ld.Data[6],
		controller);
	XLaunchNewImage(path, &ld);

	// This function should not return!
	Com_Error( ERR_FATAL, "ERROR: XLaunchNewImage returned\n" );
#endif
}

static XONLINE_ACCEPTED_GAMEINVITE acceptedGameInvite;

// Used to check for the presence of an accepted invite for our game on the HD.
// Can only return true on the FIRST call.
bool Sys_InviteExists( void )
{
#ifdef XBOX_DEMO
	return false;
#else
	static bool initialized = false;
	if( initialized )
		return false;
	initialized = true;

	// If we just came from the MP XBE, don't auto-reboot again. That's just silly.
	if( Sys_QuickStart() )
		return false;

	// Try to retrieve an invitation from the HD (this requires that we start XOnline):
	XOnlineStartup( NULL );
	HRESULT hr = XOnlineFriendsGetAcceptedGameInvite( &acceptedGameInvite );
	XOnlineCleanup();

	return (hr == S_OK);
#endif
}

// Reboot to MP to join the game that we have an invite for:
void Sys_JoinInvite( void )
{
	// Aha. Well, XTL seems to blow this away now, so we need to copy it to launch_data. Bleh.
	Sys_Reboot( "invite", &acceptedGameInvite );
	// Never returns!
}

#ifdef XBOX_DEMO
// Timer code for the demo:
static int lastTime = 0;
static bool demoTimerPaused = false;

// Notify the demo timer of a keypress, which resets the timer, and ensures
// that the timer no longer runs during FMV/loading (if it was before)
void Demo_TimerKeypress( void )
{
	demoTimerAlways = false;

	// Reset the timer:
	demoTimer = demoLaunchData.dwTimeout;

	// Stamp the diff-timer
	lastTime = Sys_Milliseconds();
}

// Update the timer, and check to see if we should reboot:
void Demo_TimerUpdate( void )
{
	// Handle first call correctly
	if( !lastTime )
	{
		demoTimer = demoLaunchData.dwTimeout;
		lastTime = Sys_Milliseconds();
	}

	int newTime = Sys_Milliseconds();
	int diffTime = newTime - lastTime;
	lastTime = newTime;

	// If the timer isn't supposed to run, and we're "paused", don't update:
	extern bool in_camera;
	if( !demoTimerAlways && (demoTimerPaused || in_camera) )
		return;

	// If we weren't even launched by CDX in the first place, or were given
	// a zero timeout, do nothing:
	if( !demoLaunchDataValid || !demoLaunchData.dwTimeout )
		return;

	// Time ran out?
	if( demoTimer < diffTime )
		Sys_Reboot( "demo", NULL );

	demoTimer -= diffTime;
}

// Pause/unpause the demo timer when entering/exiting non-interactive state:
void Demo_TimerPause( bool bPaused )
{
	// Always stamp the timer right before we change state:
	Demo_TimerUpdate();

	demoTimerPaused = bPaused;
}

#endif

/*
==================
WinMain

==================
*/
#if defined (_XBOX)
int __cdecl main()
#elif defined (_GAMECUBE)
int main(int argc, char* argv[])
#endif
{
//	Z_SetFreeOSMem();

#ifdef _XBOX
	g_SPXBBootPhase = 2;
	/* Raw NT probe — appends "main_reached" to ef_sp_log.txt before XBLog_Init.
	   If ef_sp_log.txt only has "precrt_ok", a static ctor crashed before main(). */
	{
		struct { unsigned short Len, MaxLen; char *Buf; } oname;
		struct { void *Root; void *Name; unsigned long Attr; } oa;
		struct { union { long Status; void *Ptr; }; unsigned long Info; } iosb;
		static const char path[] = "\\Device\\Harddisk0\\Partition1\\ef_sp_log.txt";
		static const char data[] = "main_reached\n";
		void *h = (void*)-1;
		oname.Buf = (char*)path; oname.Len = sizeof(path)-1; oname.MaxLen = sizeof(path);
		oa.Root = 0; oa.Name = &oname; oa.Attr = 0x40;
		/* FILE_OPEN_IF (3) + FILE_APPEND_DATA — append after precrt_ok line */
		if (NtCreateFile(&h, 0x00100004, &oa, &iosb, 0, 0x80, 0, 3, 0x62) >= 0) {
			NtWriteFile(h, 0, 0, 0, &iosb, (void*)data, sizeof(data)-1, 0);
			NtClose(h);
		}
	}
#endif

	XBLog_Init();
#ifdef _XBOX
	g_SPXBBootPhase = 0x210;
#endif
	XBLF("Log: %s\n", XBLog_GetPath() ? XBLog_GetPath() : "(none)");
#ifdef _XBOX
	g_SPXBBootPhase = 0x211;
#endif
	XBL("main() entered\n");
#ifdef _XBOX
	g_SPXBBootPhase = 0x212;
#endif

#ifdef _XBOX
	/* Match shipping JA MP startup: configure D3D here and let the later input
	 * subsystem own the process-wide XInitDevices call. */
	g_SPXBBootPhase = 0x215;
	/* Shipping JA Xbox reserves a 1 MiB primary push buffer and starts a
	 * kickoff with 128 KiB remaining.  Keep this in the shared engine startup
	 * so campaign, cooperative, and Holomatch use the same retail policy. */
	Direct3D_SetPushBufferSize(1024 * 1024, 128 * 1024);
	XBL("Retail D3D pushbuffer configured: 1048576/131072\n");
#endif

	// get the initial time base
#ifdef _XBOX
	g_SPXBBootPhase = 0x216;
#endif
	Sys_Milliseconds();
#ifdef _XBOX
	g_SPXBBootPhase = 0x217;
#endif

	// Fetch game settings early — path remapping required before renderer start.
#ifdef _XBOX
	g_SPXBBootPhase = 0x218;
#endif
	Sys_QuickStart();
#ifdef _XBOX
	g_SPXBBootPhase = 0x219;
#endif
	XBL("Sys_QuickStart done\n");

#ifdef _XBOX
	g_SPXBBootPhase = 0x21A;
#endif
	Win_Init();
#ifdef _XBOX
	g_SPXBBootPhase = 0x21B;
#endif
	XBL("Win_Init done\n");

#ifdef _XBOX
	g_SPXBBootPhase = 0x21C;
#endif
	XBL("EF: before Com_Init\n");
#ifdef _XBOX
	g_SPXBBootPhase = 0x21E;
#endif
	Com_Init( "" );
#ifdef _XBOX
	g_SPXBBootPhase = 0x21D;
#endif
	XBL("Com_Init done\n");

#if defined(STEFX_SP_HOSTED_MP)
	XBL("G_AllocGentities skipped; official Holomatch game owns entity storage\n");
#else
	extern void G_AllocGentities( void );
	G_AllocGentities();
	XBL("G_AllocGentities done\n");
#endif

#ifdef _XBOX
	const bool xboxDirectMapWarmup = Sys_XboxDirectMapRequested();
	if (xboxDirectMapWarmup)
	{
		// Suppress both the frontend and CL_Frame's legacy marker reader while
		// the first frame initializes the Xbox UI imports. No map command has
		// been placed in the command buffer yet.
		g_xboxDirectMapBootQueued = true;
		XBLog_WriteCritical("STEFX_HW_BOOT: direct-map intent held for UI warmup");
	}
#endif

	// Run one frame to finish loading the renderer, sound, and Xbox UI imports.
	IN_Frame();
	Com_Frame();
#ifdef _XBOX
	g_SPXBPhaseLast = 0x57463230; /* 'WF20' */
#endif
	XBL("First frame done\n");
#ifdef _XBOX
	g_SPXBPhaseLast = 0x57463231; /* 'WF21' */
	XBLog_WriteCritical("STEFX_HW_BOOT: first client frame complete");
	g_SPXBPhaseLast = 0x57463232; /* 'WF22' */
	g_SPXBComSubphase = 30;
	const bool xboxStartupCommandsQueued = Sys_XboxQueueDirectMapBoot();
	g_SPXBComSubphase = 31;
	g_SPXBPhaseLast = 0x57463233; /* 'WF23' */
	if (xboxDirectMapWarmup && !xboxStartupCommandsQueued)
	{
		g_xboxDirectMapBootQueued = false;
	}
	if (xboxStartupCommandsQueued)
	{
		g_SPXBComSubphase = 32;
		XBLog_WriteCritical("STEFX_HW_BOOT: executing deferred startup commands");
		g_SPXBComSubphase = 33;
		Cbuf_Execute();
		g_SPXBComSubphase = 34;
		g_SPXBDirectMapStatus = 70;
		XBLog_WriteCritical("STEFX_HW_BOOT: deferred startup commands complete");
		g_SPXBComSubphase = 35;
	}
	g_SPXBPhaseLast = 0x57463234; /* 'WF24' */
#endif

	// Copy planet bink videos to Z: drive.
	extern void Sys_BinkCopyInit(void);
#ifdef _XBOX
	if (Sys_XboxDirectMapRequested())
	{
		XBL("Sys_BinkCopyInit skipped for direct-map boot\n");
	}
	else if (FILE *autoSmokeMarker = fopen("D:\\ja_sp_autosmoke.txt", "r"))
	{
		fclose(autoSmokeMarker);
		XBL("Sys_BinkCopyInit skipped for SP autosmoke boot\n");
	}
	else
#endif
	{
	#ifdef _XBOX
		g_SPXBPhaseLast = 0x42494e31; /* 'BIN1' */
	#endif
		XBL("Sys_BinkCopyInit begin\n");
		Sys_BinkCopyInit();
		XBL("Sys_BinkCopyInit done\n");
	#ifdef _XBOX
		g_SPXBPhaseLast = 0x42494e32; /* 'BIN2' */
	#endif
	}

	XBL("Entering main game loop\n");
#ifdef _XBOX
	g_SPXBPhaseLast = 0x4d4c5030; /* 'MLP0' */
#endif

	// main game loop
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	int xboxLastLoopStart = 0;
	#endif
	while( 1 ) {
		g_SPXBMainLoopCount++;
		g_SPXBMainTailStage = 0x4D4C3030; /* 'ML00': loop entry */
		#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		const int xboxLoopStart = Sys_Milliseconds();
		if (xboxLastLoopStart != 0)
		{
			g_SPXBPerfLoopMsec = (unsigned int)(xboxLoopStart - xboxLastLoopStart);
		}
		xboxLastLoopStart = xboxLoopStart;
		const int xboxInputStart = Sys_Milliseconds();
		#endif
		IN_Frame();
		g_SPXBMainTailStage = 0x4D4C3031; /* 'ML01': input complete */
		#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBPerfInputMsec = (unsigned int)(Sys_Milliseconds() - xboxInputStart);
		const int xboxMenuStart = Sys_Milliseconds();
		#endif
#ifdef _XBOX
		g_SPXBMainTailStage = 0x4D4C3032; /* 'ML02': before menu-map work */
		Sys_XboxExecuteMenuMap();
		g_SPXBMainTailStage = 0x4D4C3033; /* 'ML03': menu-map work complete */
#endif
		#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBPerfMenuMsec = (unsigned int)(Sys_Milliseconds() - xboxMenuStart);
		#endif
		g_SPXBMainTailStage = 0x4D4C3034; /* 'ML04': before Com_Frame */
		Com_Frame();
		g_SPXBMainTailStage = 0x4D4C3035; /* 'ML05': Com_Frame complete */

		// Poll debug console for new commands
#ifndef FINAL_BUILD
		DebugConsoleHandleCommands();
#endif
		g_SPXBMainTailStage = 0x4D4C3036; /* 'ML06': loop complete */
	}

	return 0;
}


char *Sys_GetClipboardData(void) { return NULL; }

void Sys_StartProcess(char *, qboolean) {}

void Sys_OpenURL(char *, int) {}

void Sys_Quit(void)
{
#ifdef _XBOX
	XBL("JA: Sys_Quit called\n");
	XLaunchNewImage(NULL, NULL);
	XBL("JA: Sys_Quit XLaunchNewImage returned\n");
#endif
}

void Sys_ShowConsole(int, int) {}

void Sys_Mkdir(const char *) {}

int Sys_LowPhysicalMemory(void) { return 0; }

void Sys_FreeFileList(char **filelist)
{
	// All strings in a file list are allocated at once, so we just need to
	// do two frees, one for strings, one for the pointers.
	if ( filelist )
	{
		if ( filelist[0] )
			Z_Free( filelist[0] );

		Z_Free( filelist );
	}
}

#ifdef _JK2MP
char** Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs)
#else
char** Sys_ListFiles(const char *directory, const char *extension, int *numfiles, qboolean wantsubs)
#endif
{
#ifdef _JK2MP
	// MP has extra filter paramter. We don't support that.
	if (filter)
	{
		assert(!"Sys_ListFiles doesn't support filter on console!");
		return NULL;
	}
#endif

	// Hax0red console version of Sys_ListFiles. We mangle our arguments to get a standard filename
	// That file should exist, and contain the list of files that meet this search criteria.
	char	listFilename[MAX_OSPATH];
	char	*listFile = NULL, *curFile, *end;
	int		nfiles;
	char	**retList;
	int		listLen;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean traceShaderList = qfalse;
#endif

	// S00per hack
#ifdef XBOX_DEMO
	const char *basePath = Sys_RemapPath( "base\\" );
	if (strstr(directory, basePath))
		directory += strlen( basePath );
#else
#if defined(STEFX_ELITE_FORCE_SP)
	const char *baseEfPath = "d:\\BaseEF\\";
	if (!Q_stricmpn(directory, baseEfPath, strlen(baseEfPath)))
	{
		directory += strlen(baseEfPath);
	}
	else
#endif
	if (!Q_stricmpn(directory, "d:\\base\\", 8))
	{
		directory += 8;
	}
#endif

	if (!extension)
	{
		extension = "";
	}
	else if (extension[0] == '/' && extension[1] == 0)
	{
		// Passing a slash as extension will find directories
		extension = "dir";
	}
	else if (extension[0] == '.')
	{
		// Skip over leading .
		extension++;
	}

	// Build our filename
	Com_sprintf(listFilename, sizeof(listFilename), "%s\\_console_%s_list_", directory, extension);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	traceShaderList = !Q_stricmp(extension, "shader");
	if (traceShaderList)
	{
		XBLog_Write(va("STEFX: Sys_ListFiles shader dir='%s' list='%s'", directory, listFilename));
	}
#endif
	listLen = FS_ReadFile( listFilename, (void**)&listFile );
	if (listLen <= 0)
	{
		if(listFile) {
			FS_FreeFile(listFile);
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (traceShaderList)
		{
			XBLog_Write(va("STEFX: Sys_ListFiles shader list missing '%s' len=%d", listFilename, listLen));
		}
#endif
		Com_Printf( "WARNING: List file %s not found\n", listFilename );
		if (numfiles)
			*numfiles = 0;
		return NULL;
	}

	// Do a first pass to count number of files in the list
	nfiles = 0;
	curFile = listFile;
	while (true)
	{
		// Find end of line
		end = strchr(curFile, '\r');
		if (end)
		{
			// Should have a \n next -- skip them both
			end += 2;
		}
		else
		{
			end = strchr(curFile, '\n');
			if (end) end++;
			else end = curFile + strlen(curFile);
		}

		// Is the line empty?  If so, we're done.
		if (!curFile || !curFile[0]) break;
		++nfiles;

		// Advance to next line
		curFile = end;
	}

	// Fill in caller's pointer for number of files found
	if (numfiles) *numfiles = nfiles;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceShaderList)
	{
		XBLog_Write(va("STEFX: Sys_ListFiles shader list '%s' len=%d entries=%d", listFilename, listLen, nfiles));
	}
#endif

	// Did we find any files at all?
	if (nfiles == 0)
	{
		FS_FreeFile(listFile);
		return NULL;
	}

	// Allocate a file list, and quick string pool, but use LISTFILES
	retList = (char **) Z_Malloc( ( nfiles + 1 ) * sizeof( *retList ), TAG_LISTFILES, qfalse);
	// Our string pool is actually slightly too large, but it's temporary, and that's better
	// than slightly too small
	char *stringPool = (char *) Z_Malloc( strlen(listFile) + 1, TAG_LISTFILES, qfalse );

	// Now go through the list of files again, and fill in the list to be returned
	nfiles = 0;
	curFile = listFile;
	while (true)
	{
		// Find end of line
		end = strchr(curFile, '\r');
		if (end)
		{
			// Should have a \n next -- skip them both
			*end++ = '\0';
			*end++ = '\0';
		}
		else
		{
			end = strchr(curFile, '\n');
			if (end) *end++ = '\0';
			else end = curFile + strlen(curFile);
		}

		// Is the line empty?  If so, we're done.
		int curStrSize = strlen(curFile);
		if (curStrSize < 1)
		{
			retList[nfiles] = NULL;
			break;
		}

		// Alloc a small copy
		//retList[nfiles++] = CopyString( curFile );
		retList[nfiles++] = stringPool;
		strcpy(stringPool, curFile);
		stringPool += (curStrSize + 1);

		// Advance to next line
		curFile = end;
	}

	// Free the special file's buffer
	FS_FreeFile( listFile );

	return retList;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(_JK2MP)
typedef struct stefx_trace_s {
	qboolean	allsolid;
	qboolean	startsolid;
	float		fraction;
	vec3_t		endpos;
	cplane_t	plane;
	int			surfaceFlags;
	int			contents;
	int			entityNum;
} stefx_trace_t;

typedef struct stefx_game_import_s {
	void	(*Printf)( const char *fmt, ... );
	void	(*WriteCam)( const char *text );
	void	(*Error)( int, const char *fmt, ... );
	int		(*Milliseconds)( void );
	cvar_t	*(*cvar)( const char *var_name, const char *value, int flags );
	void	(*cvar_set)( const char *var_name, const char *value );
	int		(*Cvar_VariableIntegerValue)( const char *var_name );
	void	(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	int		(*argc)( void );
	char	*(*argv)( int n );
	int		(*FS_FOpenFile)( const char *qpath, fileHandle_t *file, fsMode_t mode );
	int		(*FS_Read)( void *buffer, int len, fileHandle_t f );
	int		(*FS_Write)( const void *buffer, int len, fileHandle_t f );
	void	(*FS_FCloseFile)( fileHandle_t f );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	int		(*FS_GetFileList)( const char *path, const char *extension, char *listbuf, int bufsize );
	qboolean	(*AppendToSaveGame)(unsigned long chid, void *data, int length);
	int		(*ReadFromSaveGame)(unsigned long chid, void *pvAddress, int iLength, void **ppvAddressPtr);
	int		(*ReadFromSaveGameOptional)(unsigned long chid, void *pvAddress, int iLength, void **ppvAddressPtr);
	void	(*SendConsoleCommand)( const char *text );
	void	(*DropClient)( int clientNum, const char *reason );
	void	(*SendServerCommand)( int clientNum, const char *fmt, ... );
	void	(*SetConfigstring)( int num, const char *string );
	void	(*GetConfigstring)( int num, char *buffer, int bufferSize );
	void	(*GetUserinfo)( int num, char *buffer, int bufferSize );
	void	(*SetUserinfo)( int num, const char *buffer );
	void	(*GetServerinfo)( char *buffer, int bufferSize );
	void	(*SetBrushModel)( gentity_t *ent, const char *name );
	void	(*trace)( stefx_trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
	int		(*pointcontents)( const vec3_t point, int passEntityNum );
	qboolean	(*inPVS)( const vec3_t p1, const vec3_t p2 );
	qboolean	(*inPVSIgnorePortals)( const vec3_t p1, const vec3_t p2 );
	void	(*AdjustAreaPortalState)( gentity_t *ent, qboolean open );
	qboolean	(*AreasConnected)( int area1, int area2 );
	void	(*linkentity)( gentity_t *ent );
	void	(*unlinkentity)( gentity_t *ent );
	int		(*EntitiesInBox)( const vec3_t mins, const vec3_t maxs, gentity_t **list, int maxcount );
	qboolean	(*EntityContact)( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
	int		*S_Override;
	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );
} stefx_game_import_t;

typedef struct stefx_game_export_s {
	int			apiversion;
	void		(*Init)( const char *mapname, const char *spawntarget, int checkSum, const char *entstring, int levelTime, int randomSeed, int globalTime, SavedGameJustLoaded_e eSavedGameJustLoaded, qboolean qbLoadTransition );
	void		(*Shutdown) (void);
	void		(*WriteLevel) (qboolean qbAutosave);
	void		(*ReadLevel)  (qboolean qbAutosave, qboolean qbLoadTransition);
	qboolean	(*GameAllowedToSaveHere)(void);
	char		*(*ClientConnect)( int clientNum, qboolean firstTime, SavedGameJustLoaded_e eSavedGameJustLoaded );
	void		(*ClientBegin)( int clientNum, usercmd_t *cmd, SavedGameJustLoaded_e eSavedGameJustLoaded);
	void		(*ClientUserinfoChanged)( int clientNum );
	void		(*ClientDisconnect)( int clientNum );
	void		(*ClientCommand)( int clientNum );
	void		(*ClientThink)( int clientNum, usercmd_t *cmd );
	void		(*RunFrame)( int levelTime );
	qboolean	(*ConsoleCommand)( void );
	gentity_t	*gentities;
	int			gentitySize;
	int			num_entities;
} stefx_game_export_t;

static Trace_Functor_t s_stefxTrace;
static stefx_game_export_t *s_stefxEfGame = NULL;
static game_export_t s_stefxJaGame;
static int s_stefxRunFrameLogCount = 0;

extern int SG_Read(unsigned long chid, void *pvAddress, int iLength, void **ppvAddressPtr);
extern int SG_ReadOptional(unsigned long chid, void *pvAddress, int iLength, void **ppvAddressPtr);

static void STEFX_SyncGameExport(void)
{
	if (!s_stefxEfGame)
	{
		s_stefxJaGame.gentities = NULL;
		s_stefxJaGame.gentitySize = 0;
		s_stefxJaGame.num_entities = 0;
		return;
	}

	s_stefxJaGame.gentities = s_stefxEfGame->gentities;
	s_stefxJaGame.gentitySize = s_stefxEfGame->gentitySize;
	s_stefxJaGame.num_entities = s_stefxEfGame->num_entities;
}

static void *STEFX_GameMalloc(int bytes)
{
	return Z_Malloc(bytes, TAG_G_ALLOC, qfalse);
}

static void STEFX_GameFree(void *buf)
{
	if (buf)
	{
		Z_Free(buf);
	}
}

static void STEFX_CopyTraceToEf(stefx_trace_t *dst, const trace_t *src)
{
	if (!dst || !src)
	{
		return;
	}

	dst->allsolid = src->allsolid;
	dst->startsolid = src->startsolid;
	dst->fraction = src->fraction;
	VectorCopy(src->endpos, dst->endpos);
	dst->plane = src->plane;
	dst->surfaceFlags = src->surfaceFlags;
	dst->contents = src->contents;
	dst->entityNum = src->entityNum;
}

static void STEFX_Trace(stefx_trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask)
{
	static int s_traceLogCount = 0;
	trace_t jaTrace;
	memset(&jaTrace, 0, sizeof(jaTrace));
	if (s_traceLogCount < 64)
	{
		g_SPXBPhaseLast = 0x54524144; /* TRAD */
		g_SPXBComSubphase = 1;
	}
	s_stefxTrace(&jaTrace, start, mins, maxs, end, passEntityNum, contentmask, G2_NOCOLLIDE, 0);
	STEFX_CopyTraceToEf(results, &jaTrace);
	if (s_traceLogCount < 64)
	{
		g_SPXBPhaseLast = 0x54524144; /* TRAD */
		g_SPXBComSubphase = 2;
	}
	s_traceLogCount++;
}

static void STEFX_Init(const char *mapname, const char *spawntarget, int checkSum, const char *entstring, int levelTime, int randomSeed, int globalTime, SavedGameJustLoaded_e eSavedGameJustLoaded, qboolean qbLoadTransition)
{
	XBLF("STEFX: adapter before EF Init map='%s' checksum=%d", mapname ? mapname : "(null)", checkSum);
	s_stefxRunFrameLogCount = 0;
	if (s_stefxEfGame && s_stefxEfGame->Init)
	{
		s_stefxEfGame->Init(mapname, spawntarget, checkSum, entstring, levelTime, randomSeed, globalTime, eSavedGameJustLoaded, qbLoadTransition);
	}
	STEFX_SyncGameExport();
	XBLF("STEFX: adapter after EF Init gentities=%p gentitySize=%d num_entities=%d",
		s_stefxJaGame.gentities, s_stefxJaGame.gentitySize, s_stefxJaGame.num_entities);
}

static void STEFX_Shutdown(void)
{
	XBLog_Write("STEFX: adapter Shutdown");
	if (s_stefxEfGame && s_stefxEfGame->Shutdown)
	{
		s_stefxEfGame->Shutdown();
	}
	STEFX_SyncGameExport();
}

static void STEFX_WriteLevel(qboolean qbAutosave)
{
	if (s_stefxEfGame && s_stefxEfGame->WriteLevel)
	{
		s_stefxEfGame->WriteLevel(qbAutosave);
	}
	STEFX_SyncGameExport();
}

static void STEFX_ReadLevel(qboolean qbAutosave, qboolean qbLoadTransition)
{
	if (s_stefxEfGame && s_stefxEfGame->ReadLevel)
	{
		s_stefxEfGame->ReadLevel(qbAutosave, qbLoadTransition);
	}
	STEFX_SyncGameExport();
}

static qboolean STEFX_GameAllowedToSaveHere(void)
{
	return (s_stefxEfGame && s_stefxEfGame->GameAllowedToSaveHere) ? s_stefxEfGame->GameAllowedToSaveHere() : qfalse;
}

static char *STEFX_ClientConnect(int clientNum, qboolean firstTime, SavedGameJustLoaded_e eSavedGameJustLoaded)
{
	char *result = NULL;
	if (s_stefxEfGame && s_stefxEfGame->ClientConnect)
	{
		result = s_stefxEfGame->ClientConnect(clientNum, firstTime, eSavedGameJustLoaded);
	}
	STEFX_SyncGameExport();
	return result;
}

static void STEFX_ClientBegin(int clientNum, usercmd_t *cmd, SavedGameJustLoaded_e eSavedGameJustLoaded)
{
	if (s_stefxEfGame && s_stefxEfGame->ClientBegin)
	{
		s_stefxEfGame->ClientBegin(clientNum, cmd, eSavedGameJustLoaded);
	}
	STEFX_SyncGameExport();
}

static void STEFX_ClientUserinfoChanged(int clientNum)
{
	if (s_stefxEfGame && s_stefxEfGame->ClientUserinfoChanged)
	{
		s_stefxEfGame->ClientUserinfoChanged(clientNum);
	}
	STEFX_SyncGameExport();
}

static void STEFX_ClientDisconnect(int clientNum)
{
	if (s_stefxEfGame && s_stefxEfGame->ClientDisconnect)
	{
		s_stefxEfGame->ClientDisconnect(clientNum);
	}
	STEFX_SyncGameExport();
}

static void STEFX_ClientCommand(int clientNum)
{
	if (s_stefxEfGame && s_stefxEfGame->ClientCommand)
	{
		s_stefxEfGame->ClientCommand(clientNum);
	}
	STEFX_SyncGameExport();
}

static void STEFX_ClientThink(int clientNum, usercmd_t *cmd)
{
#if defined(_XBOX)
	static int s_stefxAdapterClientThinkBudget = 96;
	qboolean logThis = qfalse;

	if (logThis)
	{
		void *efEnt = NULL;
		if (s_stefxEfGame && s_stefxEfGame->gentities && s_stefxEfGame->gentitySize > 0 && clientNum >= 0)
		{
			efEnt = (void *)((byte *)s_stefxEfGame->gentities + s_stefxEfGame->gentitySize * clientNum);
		}
		XBLF("STEFX: adapter ClientThink enter client=%d cmd=%p cmdTime=%d move=(%d,%d,%d) buttons=0x%x weapon=%d efGame=%p efThink=%p efEnt=%p efGentities=%p efGentitySize=%d efNum=%d",
			clientNum,
			cmd,
			cmd ? cmd->serverTime : -1,
			cmd ? cmd->forwardmove : 0,
			cmd ? cmd->rightmove : 0,
			cmd ? cmd->upmove : 0,
			cmd ? cmd->buttons : 0,
			cmd ? cmd->weapon : -1,
			s_stefxEfGame,
			s_stefxEfGame ? s_stefxEfGame->ClientThink : NULL,
			efEnt,
			s_stefxEfGame ? s_stefxEfGame->gentities : NULL,
			s_stefxEfGame ? s_stefxEfGame->gentitySize : 0,
			s_stefxEfGame ? s_stefxEfGame->num_entities : 0);
	}
#endif
	if (s_stefxEfGame && s_stefxEfGame->ClientThink)
	{
		s_stefxEfGame->ClientThink(clientNum, cmd);
	}
	STEFX_SyncGameExport();
#if defined(_XBOX)
	if (logThis)
	{
		XBLF("STEFX: adapter ClientThink exit client=%d cmdTime=%d efGentities=%p efGentitySize=%d efNum=%d jaGentities=%p jaGentitySize=%d jaNum=%d",
			clientNum,
			cmd ? cmd->serverTime : -1,
			s_stefxEfGame ? s_stefxEfGame->gentities : NULL,
			s_stefxEfGame ? s_stefxEfGame->gentitySize : 0,
			s_stefxEfGame ? s_stefxEfGame->num_entities : 0,
			s_stefxJaGame.gentities,
			s_stefxJaGame.gentitySize,
			s_stefxJaGame.num_entities);
		s_stefxAdapterClientThinkBudget--;
	}
#endif
}

static void STEFX_RunFrame(int levelTime)
{
	if (s_stefxRunFrameLogCount < 16)
	{
		XBLF("STEFX: adapter before EF RunFrame count=%d time=%d", s_stefxRunFrameLogCount, levelTime);
	}
	if (s_stefxEfGame && s_stefxEfGame->RunFrame)
	{
		s_stefxEfGame->RunFrame(levelTime);
	}
	STEFX_SyncGameExport();
	if (s_stefxRunFrameLogCount < 16)
	{
		XBLF("STEFX: adapter after EF RunFrame count=%d time=%d entities=%d", s_stefxRunFrameLogCount, levelTime, s_stefxJaGame.num_entities);
	}
	s_stefxRunFrameLogCount++;
}

static void STEFX_ConnectNavs(const char *mapname, int checkSum)
{
	XBLF("STEFX: adapter ConnectNavs no-op map='%s' checksum=%d", mapname ? mapname : "(null)", checkSum);
	STEFX_SyncGameExport();
}

static qboolean STEFX_ConsoleCommand(void)
{
	qboolean result = qfalse;
	if (s_stefxEfGame && s_stefxEfGame->ConsoleCommand)
	{
		result = s_stefxEfGame->ConsoleCommand();
	}
	STEFX_SyncGameExport();
	return result;
}

static void STEFX_GameSpawnRMGEntity(char *s)
{
	XBLF("STEFX: adapter GameSpawnRMGEntity no-op entity='%s'", s ? s : "(null)");
	STEFX_SyncGameExport();
}

static void STEFX_BuildJaExportAdapter(void)
{
	memset(&s_stefxJaGame, 0, sizeof(s_stefxJaGame));
	s_stefxJaGame.apiversion = GAME_API_VERSION;
	s_stefxJaGame.Init = STEFX_Init;
	s_stefxJaGame.Shutdown = STEFX_Shutdown;
	s_stefxJaGame.WriteLevel = STEFX_WriteLevel;
	s_stefxJaGame.ReadLevel = STEFX_ReadLevel;
	s_stefxJaGame.GameAllowedToSaveHere = STEFX_GameAllowedToSaveHere;
	s_stefxJaGame.ClientConnect = STEFX_ClientConnect;
	s_stefxJaGame.ClientBegin = STEFX_ClientBegin;
	s_stefxJaGame.ClientUserinfoChanged = STEFX_ClientUserinfoChanged;
	s_stefxJaGame.ClientDisconnect = STEFX_ClientDisconnect;
	s_stefxJaGame.ClientCommand = STEFX_ClientCommand;
	s_stefxJaGame.ClientThink = STEFX_ClientThink;
	s_stefxJaGame.RunFrame = STEFX_RunFrame;
	s_stefxJaGame.ConnectNavs = STEFX_ConnectNavs;
	s_stefxJaGame.ConsoleCommand = STEFX_ConsoleCommand;
	s_stefxJaGame.GameSpawnRMGEntity = STEFX_GameSpawnRMGEntity;
	STEFX_SyncGameExport();
}
#endif

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame( void ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(_JK2MP)
	s_stefxEfGame = NULL;
	memset(&s_stefxJaGame, 0, sizeof(s_stefxJaGame));
#endif
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
#ifndef _JK2MP
void *Sys_GetGameAPI (void *parms)
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	extern game_export_t *GetGameAPI( game_import_t *import );
	game_import_t *jaImport = (game_import_t *)parms;
#if defined(STEFX_SP_HOSTED_MP)
	extern game_export_t *STEFX_GetHolomatchGameAPI( game_import_t *import );
	XBL("STEFX: SP-hosted Sys_GetGameAPI selecting official EF Holomatch adapter");
	return STEFX_GetHolomatchGameAPI(jaImport);
#else
	stefx_game_import_t efImport;
	game_export_t *rawExport;

	memset(&efImport, 0, sizeof(efImport));
	s_stefxTrace = jaImport->trace;

	efImport.Printf = jaImport->Printf;
	efImport.WriteCam = jaImport->WriteCam;
	efImport.Error = jaImport->Error;
	efImport.Milliseconds = jaImport->Milliseconds;
	efImport.cvar = jaImport->cvar;
	efImport.cvar_set = jaImport->cvar_set;
	efImport.Cvar_VariableIntegerValue = jaImport->Cvar_VariableIntegerValue;
	efImport.Cvar_VariableStringBuffer = jaImport->Cvar_VariableStringBuffer;
	efImport.argc = jaImport->argc;
	efImport.argv = jaImport->argv;
	efImport.FS_FOpenFile = jaImport->FS_FOpenFile;
	efImport.FS_Read = jaImport->FS_Read;
	efImport.FS_Write = jaImport->FS_Write;
	efImport.FS_FCloseFile = jaImport->FS_FCloseFile;
	efImport.FS_ReadFile = jaImport->FS_ReadFile;
	efImport.FS_FreeFile = jaImport->FS_FreeFile;
	efImport.FS_GetFileList = jaImport->FS_GetFileList;
	efImport.AppendToSaveGame = (qboolean (*)(unsigned long, void *, int))jaImport->AppendToSaveGame;
	efImport.ReadFromSaveGame = SG_Read;
	efImport.ReadFromSaveGameOptional = SG_ReadOptional;
	efImport.SendConsoleCommand = jaImport->SendConsoleCommand;
	efImport.DropClient = jaImport->DropClient;
	efImport.SendServerCommand = jaImport->SendServerCommand;
	efImport.SetConfigstring = jaImport->SetConfigstring;
	efImport.GetConfigstring = jaImport->GetConfigstring;
	efImport.GetUserinfo = jaImport->GetUserinfo;
	efImport.SetUserinfo = jaImport->SetUserinfo;
	efImport.GetServerinfo = jaImport->GetServerinfo;
	efImport.SetBrushModel = jaImport->SetBrushModel;
	efImport.trace = STEFX_Trace;
	efImport.pointcontents = jaImport->pointcontents;
	efImport.inPVS = jaImport->inPVS;
	efImport.inPVSIgnorePortals = jaImport->inPVSIgnorePortals;
	efImport.AdjustAreaPortalState = jaImport->AdjustAreaPortalState;
	efImport.AreasConnected = jaImport->AreasConnected;
	efImport.linkentity = jaImport->linkentity;
	efImport.unlinkentity = jaImport->unlinkentity;
	efImport.EntitiesInBox = jaImport->EntitiesInBox;
	efImport.EntityContact = jaImport->EntityContact;
	efImport.S_Override = jaImport->S_Override;
	efImport.Malloc = STEFX_GameMalloc;
	efImport.Free = STEFX_GameFree;

	XBLF("STEFX: Sys_GetGameAPI before EF GetGameAPI import=%p efImport=%p", jaImport, &efImport);
	rawExport = GetGameAPI((game_import_t *)&efImport);
	s_stefxEfGame = (stefx_game_export_t *)rawExport;
	XBLF("STEFX: Sys_GetGameAPI after EF GetGameAPI raw=%p api=%d", rawExport, s_stefxEfGame ? s_stefxEfGame->apiversion : -1);
	STEFX_BuildJaExportAdapter();
	XBLF("STEFX: Sys_GetGameAPI returning JA adapter=%p api=%d efGentitySize=%d",
		&s_stefxJaGame, s_stefxJaGame.apiversion, s_stefxJaGame.gentitySize);
	return &s_stefxJaGame;
#endif
#else
	extern game_export_t *GetGameAPI( game_import_t *import );
	return GetGameAPI((game_import_t *)parms);
#endif
}
#endif

/*
=================
Sys_LoadCgame

Used to hook up a development dll
=================
*/
// void * Sys_LoadCgame( void ) 
#ifndef _JK2MP
#if defined(STEFX_SP_HOSTED_MP)
extern "C" void STEFX_HM_CG_dllEntry( int (QDECL *syscallptr)( int arg,... ) );
extern "C" int STEFX_HM_CG_vmMain( int command, int arg0, int arg1, int arg2,
	int arg3, int arg4, int arg5, int arg6 );
#endif
void * Sys_LoadCgame( int (**entryPoint)(int, ...), int (*systemcalls)(int, ...) )
{
#if defined(STEFX_SP_HOSTED_MP)
	XBLF("STEFX_HM_SP: Sys_LoadCgame official entryOut=%p syscall=%p vmMain=%p dllEntry=%p",
		entryPoint, systemcalls, STEFX_HM_CG_vmMain, STEFX_HM_CG_dllEntry);
	STEFX_HM_CG_dllEntry(systemcalls);
	*entryPoint = (int (*)(int,...))STEFX_HM_CG_vmMain;
	XBLF("STEFX_HM_SP: Sys_LoadCgame official entry assigned=%p", *entryPoint);
	return 0;
#else
	extern void CG_PreInit();
	extern void cg_dllEntry( int (*syscallptr)( int arg,... ) );
	extern int cg_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7 );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLF("STEFX: Sys_LoadCgame entryOut=%p syscall=%p cg_vmMain=%p cg_dllEntry=%p",
		entryPoint, systemcalls, cg_vmMain, cg_dllEntry);
#endif
	cg_dllEntry(systemcalls);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLF("STEFX: Sys_LoadCgame after cg_dllEntry");
#endif
	*entryPoint = (int (*)(int,...))cg_vmMain;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBCGameEntryExpected = (unsigned int)cg_vmMain;
	g_SPXBCGameEntryCurrent = (unsigned int)*entryPoint;
	XBLF("STEFX: Sys_LoadCgame entry assigned=%p", *entryPoint);
#endif
//	CG_PreInit();
	return 0;
#endif
}
#endif

/* VVFIXME: More stubs */
qboolean Sys_FileOutOfDate( LPCSTR psFinalFileName /* dest */, LPCSTR psDataFileName /* src */ )
{
	return qfalse;
}

qboolean Sys_CopyFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, qboolean bOverwrite)
{
	return qfalse;
}

qboolean Sys_CheckCD( void )
{
	return qtrue;
}
