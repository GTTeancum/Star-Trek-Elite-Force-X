
#include "g_local.h"
#include "g_functions.h"
#include "Q3_Interface.h"
#include "g_nav.h"
#include "boltOns.h"
#include "g_roff.h"
#include "g_navigator.h"
#include "anims.h"
#ifdef _XBOX
#include "../../code/win32/xb_log.h"
#endif

extern CNavigator		navigator;

#define	STEPSIZE		18

level_locals_t	level;
game_import_t	gi;
game_export_t	globals;

gentity_t		*g_entities;
gentity_t		*playerEnt = &g_entities[0];

cvar_t	*g_speed;
cvar_t	*g_gravity;
cvar_t	*g_sex;
cvar_t	*g_spskill;
cvar_t	*g_cheats;
cvar_t	*g_developer;
cvar_t	*g_timescale;
cvar_t	*g_knockback;
cvar_t	*g_inactivity;
cvar_t	*g_debugMove;
cvar_t	*g_debugDamage;
cvar_t	*g_weaponRespawn;
cvar_t	*g_subtitles;
cvar_t	*g_language;
cvar_t	*g_ICARUSDebug;
extern cvar_t	*com_buildScript;
cvar_t	*g_skippingcin;
cvar_t	*g_virtualVoyager;

qboolean	stop_icarus = qfalse;

extern char *G_GetLocationForEnt( gentity_t *ent );
void G_RunFrame (int levelTime);
void CG_LoadInterface (void);
void ClearNPCGlobals( void );
void SetClientViewAngle( gentity_t *ent, vec3_t angle );

void ClearPlayerAlertEvents( void );
#define	ALERT_CLEAR_TIME	200
int eventClearTime = 0;

int form_shot_traces = 0;
int form_updateseg_traces = 0;
int form_leaderseg_traces = 0;
int form_closestSP_traces = 0;
int form_clearpath_traces = 0;

extern void NPC_ShowDebugInfo (void);
extern int ffireForgivenessTimer;
extern int ffireLevel;
extern int killPlayerTimer;
extern int loadBrigTimer;
extern const int FFIRE_LEVEL_RETALIATION;
int	teamCount[TEAM_NUM_TEAMS];
int	teamLastEnemyTime[TEAM_NUM_TEAMS];
int	teamEnemyCount[TEAM_NUM_TEAMS];

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
extern qboolean CL_STEFX_SplitScreen_BuildP2Usercmd( usercmd_t *cmd, const vec3_t currentAngles, const int deltaAngles[3], int serverTime, int *sourcePort, int *weaponDelta, vec3_t outAngles );
void NPC_SetAnim( gentity_t *ent, int type, int anim, int priority );
extern void ChangeWeapon( gentity_t *ent, int newWeapon );

extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBComSubphase;
extern "C" volatile unsigned int g_SPXBComSpinCount;
extern "C" volatile unsigned int g_SPXBComMsec;
extern "C" volatile unsigned int g_SPXBComFrameTime;
extern "C" volatile unsigned int g_SPXBComLastTime;
extern "C" volatile unsigned int g_SPXBSVProbePhase;
extern "C" volatile unsigned int g_SPXBSVProbeSubphase;
extern "C" volatile unsigned int g_SPXBSVProbeA;
extern "C" volatile unsigned int g_SPXBSVProbeB;
extern "C" volatile unsigned int g_SPXBSVProbeC;
extern "C" volatile unsigned int g_SPXBSVProbeD;
extern "C" volatile unsigned int g_SPXBHelmetGameP1Ensure;
extern "C" volatile unsigned int g_SPXBHelmetGameP2Ensure;
extern "C" volatile unsigned int g_SPXBHelmetGameP1Slot;
extern "C" volatile unsigned int g_SPXBHelmetGameP2Slot;
extern "C" volatile unsigned int g_SPXBPerfGamePreMsec;
extern "C" volatile unsigned int g_SPXBPerfGameEntitiesMsec;
extern "C" volatile unsigned int g_SPXBPerfGamePostMsec;
extern "C" volatile unsigned int g_SPXBPerfGameEntitiesVisited;
extern "C" volatile unsigned int g_SPXBPerfGameMissiles;
extern "C" volatile unsigned int g_SPXBPerfGameItems;
extern "C" volatile unsigned int g_SPXBPerfGameMovers;
extern "C" volatile unsigned int g_SPXBPerfGameClients;
extern "C" volatile unsigned int g_SPXBPerfGameThinkDue;
extern "C" volatile unsigned int g_SPXBPerfGameScripted;
extern "C" volatile unsigned int g_SPXBPerfGameOther;
extern "C" volatile unsigned int g_SPXBGameDetailTraceEnabled;

#if defined(_DEBUG)
#define STEFX_GAME_TRACE_STAGE(phase, subphase) \
	do { if (g_SPXBGameDetailTraceEnabled) { g_SPXBPhaseLast = (phase); g_SPXBComSubphase = (subphase); g_SPXBSVProbePhase = (phase); g_SPXBSVProbeSubphase = (subphase); } } while (0)
#define STEFX_GAME_TRACE_DETAIL(a, b, c, d) \
	do { if (g_SPXBGameDetailTraceEnabled) { g_SPXBComSpinCount = (a); g_SPXBComMsec = (b); g_SPXBComFrameTime = (c); g_SPXBComLastTime = (d); g_SPXBSVProbeA = (a); g_SPXBSVProbeB = (b); g_SPXBSVProbeC = (c); g_SPXBSVProbeD = (d); } } while (0)
#else
#define STEFX_GAME_TRACE_STAGE(phase, subphase) do { } while (0)
#define STEFX_GAME_TRACE_DETAIL(a, b, c, d) do { } while (0)
#endif

#define STEFX_SPLIT_P2_TARGETNAME "stefx_split_p2"

static int s_stefxSplitP2EntNum = ENTITYNUM_NONE;

static void STEFX_SplitCoopEnsureBaseLoadout( gentity_t *p2 );
static void STEFX_SplitCoopSyncHelmetBoltons( gentity_t *p2 );
static void STEFX_SplitCoopLogHelmetState( const char *context, gentity_t *p2 );
static void STEFX_SplitCoopApplyInitialEVHelmet( gentity_t *p2 );
static void STEFX_SplitCoopMaintainEVHelmets( const char *context );
static gentity_t *STEFX_SplitCoopFindP2( void );
static void STEFX_SplitCoopPlacementFromP1( vec3_t origin, vec3_t angles );
static qboolean STEFX_SplitCoopMapStartsWithEVHelmet( void );

static qboolean STEFX_SplitCoopActive( void )
{
	const int split = gi.Cvar_VariableIntegerValue( "stefx_splitScreen" );
	const int players = gi.Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" );
	return (qboolean)( split && players >= 2 );
}

static qboolean STEFX_SplitCoopP1Female( void )
{
	return (qboolean)( g_sex && g_sex->string && ( g_sex->string[0] == 'f' || g_sex->string[0] == 'F' ) );
}

static const char *STEFX_SplitCoopP2NPCType( void )
{
	return STEFX_SplitCoopP1Female() ? "munro" : "alexandria";
}

static gentity_t *STEFX_SplitCoopP1( void )
{
	if ( !g_entities )
	{
		return NULL;
	}
	return &g_entities[0];
}

static team_t STEFX_SplitCoopP1Team( void )
{
	gentity_t *p1 = STEFX_SplitCoopP1();

	if ( p1 && p1->client )
	{
		return p1->client->playerTeam;
	}
	return TEAM_STARFLEET;
}

static team_t STEFX_SplitCoopP1EnemyTeam( void )
{
	gentity_t *p1 = STEFX_SplitCoopP1();

	if ( p1 && p1->client )
	{
		return p1->client->enemyTeam;
	}
	return TEAM_BORG;
}

static qboolean STEFX_SplitCoopP1Disguised( void )
{
	return (qboolean)( STEFX_SplitCoopP1Team() == TEAM_DISGUISE );
}

static const char *STEFX_SplitCoopNormalHeadName( qboolean female )
{
	return female ? "alexandria/default" : "munro/default";
}

static const char *STEFX_SplitCoopEVHeadName( qboolean female )
{
	return female ? "alexascav/default" : "munroscav/default";
}

static qboolean STEFX_SplitCoopHeadNameIsEV( const char *headName )
{
	return (qboolean)( headName &&
		( !Q_stricmp( headName, "alexascav/default" ) ||
			!Q_stricmp( headName, "munroscav/default" ) ) );
}

static void STEFX_SplitCoopDesiredModelNames( qboolean female, qboolean disguised, const char **legs, const char **torso, const char **head )
{
	if ( disguised )
	{
		if ( female )
		{
			*legs = "impfem/default";
			*torso = "impfem/default";
			*head = "alexascav/default";
		}
		else
		{
			*legs = "imperial/raider";
			*torso = "imperial/raider";
			*head = "munroscav/default";
		}
		return;
	}

	if ( female )
	{
		*legs = "hazardfemale/default";
		*torso = "hazardfemale/default";
		*head = STEFX_SplitCoopNormalHeadName( female );
	}
	else
	{
		*legs = "hazard/default";
		*torso = "hazard/default";
		*head = STEFX_SplitCoopNormalHeadName( female );
	}
}

static void STEFX_SplitCoopApplyP2Model( gentity_t *p2 )
{
	static int s_modelLogBudget = 16;
	const char *legs;
	const char *torso;
	const char *head;
	const qboolean p1Female = STEFX_SplitCoopP1Female();
	const qboolean p2Female = (qboolean)!p1Female;
	const qboolean p1Disguised = STEFX_SplitCoopP1Disguised();
	const team_t p1Team = STEFX_SplitCoopP1Team();
	const team_t p1EnemyTeam = STEFX_SplitCoopP1EnemyTeam();
	qboolean teamChanged = qfalse;

	if ( !p2 || !p2->client )
	{
		return;
	}

	if ( p2->client->playerTeam != p1Team )
	{
		p2->client->playerTeam = p1Team;
		p2->client->ps.persistant[PERS_TEAM] = p1Team;
		teamChanged = qtrue;
	}
	if ( p2->client->enemyTeam != p1EnemyTeam )
	{
		p2->client->enemyTeam = p1EnemyTeam;
		teamChanged = qtrue;
	}

	STEFX_SplitCoopDesiredModelNames( p2Female, p1Disguised, &legs, &torso, &head );
	if ( !Q_stricmp( p2->client->renderInfo.legsModelName, legs )
		&& !Q_stricmp( p2->client->renderInfo.torsoModelName, torso )
		&& !Q_stricmp( p2->client->renderInfo.headModelName, head )
		&& !teamChanged )
	{
		return;
	}

	Q_strncpyz( p2->client->renderInfo.legsModelName, legs, sizeof( p2->client->renderInfo.legsModelName ), qtrue );
	Q_strncpyz( p2->client->renderInfo.torsoModelName, torso, sizeof( p2->client->renderInfo.torsoModelName ), qtrue );
	Q_strncpyz( p2->client->renderInfo.headModelName, head, sizeof( p2->client->renderInfo.headModelName ), qtrue );
	p2->client->clientInfo.infoValid = qfalse;

	if ( s_modelLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP model ent=%d p1Sex='%s' p2Sex='%s' p1Team=%d p1Enemy=%d disguised=%d npc='%s' renderNames=(%s,%s,%s) infoValid=%d models=(%d,%d,%d) deferredRegister=1",
			p2->s.number,
			p1Female ? "female" : "male",
			p2Female ? "female" : "male",
			p1Team,
			p1EnemyTeam,
			p1Disguised ? 1 : 0,
			p2->NPC_type ? p2->NPC_type : "<null>",
			p2->client->renderInfo.legsModelName,
			p2->client->renderInfo.torsoModelName,
			p2->client->renderInfo.headModelName,
			p2->client->clientInfo.infoValid ? 1 : 0,
			p2->client->clientInfo.legsModel,
			p2->client->clientInfo.torsoModel,
			p2->client->clientInfo.headModel );
		--s_modelLogBudget;
	}
}

static void STEFX_SplitCoopSyncOneHelmetBoltOn( gentity_t *p1, gentity_t *p2, const char *boltOnName )
{
	static int s_helmetSyncLogBudget = 48;
	byte p1Slot;
	byte p2Slot;

	if ( !p1 || !p1->client || !p2 || !p2->client || !boltOnName || !boltOnName[0] )
	{
		return;
	}

	p1Slot = G_BoltOnNumberForName( p1, boltOnName );
	p2Slot = G_BoltOnNumberForName( p2, boltOnName );

	if ( p1Slot < MAX_BOLT_ONS && p2Slot >= MAX_BOLT_ONS )
	{
		p2Slot = G_AddBoltOn( p2, boltOnName );
		if ( p2Slot >= MAX_BOLT_ONS )
		{
			p2Slot = G_BoltOnNumberForName( p2, boltOnName );
		}
		if ( p2Slot < MAX_BOLT_ONS )
		{
			p2->activeBoltOn = p2Slot;
		}
		if ( s_helmetSyncLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_COOP helmet sync add p2=%d boltOn='%s' p1Slot=%d p2Slot=%d time=%d",
				p2->s.number,
				boltOnName,
				(int)p1Slot,
				(int)p2Slot,
				level.time );
			--s_helmetSyncLogBudget;
		}
	}
	else if ( p1Slot >= MAX_BOLT_ONS && p2Slot < MAX_BOLT_ONS )
	{
		G_RemoveBoltOn( p2, boltOnName );
		if ( s_helmetSyncLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_COOP helmet sync remove p2=%d boltOn='%s' time=%d",
				p2->s.number,
				boltOnName,
				level.time );
			--s_helmetSyncLogBudget;
		}
	}
}

static void STEFX_SplitCoopSyncHelmetBoltons( gentity_t *p2 )
{
	gentity_t *p1 = &g_entities[0];

	if ( !STEFX_SplitCoopActive() || !p1 || !p1->inuse || !p1->client || !p2 || !p2->inuse || !p2->client )
	{
		return;
	}

	STEFX_SplitCoopSyncOneHelmetBoltOn( p1, p2, "helmet" );
}

static qboolean STEFX_SplitCoopMapStartsWithEVHelmet( void )
{
	if ( !level.mapname[0] )
	{
		return qfalse;
	}

	return (qboolean)( !Q_stricmp( level.mapname, "scav1" ) ||
		!Q_stricmp( level.mapname, "dn1" ) ||
		!Q_stricmp( level.mapname, "forge1" ) ||
		!Q_stricmp( level.mapname, "forge2" ) );
}

static qboolean STEFX_SplitCoopNameContainsNoCase( const char *name, const char *needle )
{
	int needleLen;

	if ( !name || !needle || !needle[0] )
	{
		return qfalse;
	}

	needleLen = strlen( needle );
	while ( *name )
	{
		if ( !Q_stricmpn( name, needle, needleLen ) )
		{
			return qtrue;
		}
		++name;
	}

	return qfalse;
}

static qboolean STEFX_SplitCoopEntityUsesHazardSuit( const gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return qfalse;
	}

	return (qboolean)( STEFX_SplitCoopNameContainsNoCase( ent->client->renderInfo.legsModelName, "hazard" ) ||
		STEFX_SplitCoopNameContainsNoCase( ent->client->renderInfo.torsoModelName, "hazard" ) );
}

static void STEFX_SplitCoopMaintainP1EVHeadModel( const char *context )
{
	static int s_p1EVHeadLogBudget = 48;
	gentity_t *p1 = &g_entities[0];
	const qboolean female = STEFX_SplitCoopP1Female();
	const char *desiredHead = NULL;
	const char *currentHead;
	qboolean shouldUseEVHead;

	if ( !p1 || !p1->inuse || !p1->client || STEFX_SplitCoopP1Disguised() )
	{
		return;
	}

	currentHead = p1->client->renderInfo.headModelName;
	shouldUseEVHead = qfalse;

	if ( shouldUseEVHead )
	{
		desiredHead = STEFX_SplitCoopEVHeadName( female );
	}
	else if ( STEFX_SplitCoopHeadNameIsEV( currentHead ) )
	{
		desiredHead = STEFX_SplitCoopNormalHeadName( female );
	}

	if ( !desiredHead || !desiredHead[0] || !Q_stricmp( currentHead, desiredHead ) )
	{
		return;
	}

	if ( s_p1EVHeadLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP p1 ev-head %s map='%s' female=%d current='%s' desired='%s' hazard=%d time=%d",
			context ? context : "<null>",
			level.mapname,
			female ? 1 : 0,
			currentHead,
			desiredHead,
			STEFX_SplitCoopEntityUsesHazardSuit( p1 ) ? 1 : 0,
			level.time );
		--s_p1EVHeadLogBudget;
	}

	Q_strncpyz( p1->client->renderInfo.headModelName, desiredHead, sizeof( p1->client->renderInfo.headModelName ), qtrue );
	p1->client->clientInfo.infoValid = qfalse;
	gi.SendConsoleCommand( va( "headModel %s\n", desiredHead ) );
}

static byte STEFX_SplitCoopEnsureHelmetOnEntity( gentity_t *ent, const char *context )
{
	static int s_helmetEnsureLogBudget = 96;
	qboolean changed = qfalse;
	byte handSlot;
	byte slot;

	if ( !ent || !ent->inuse || !ent->client )
	{
		return MAX_BOLT_ONS;
	}

	handSlot = G_BoltOnNumberForName( ent, "helmet_lhand" );
	if ( handSlot < MAX_BOLT_ONS )
	{
		G_RemoveBoltOn( ent, "helmet_lhand" );
		changed = qtrue;
	}

	slot = G_BoltOnNumberForName( ent, "helmet" );
	if ( slot >= MAX_BOLT_ONS )
	{
		slot = G_AddBoltOn( ent, "helmet" );
		if ( slot >= MAX_BOLT_ONS )
		{
			slot = G_BoltOnNumberForName( ent, "helmet" );
		}
		changed = qtrue;
	}

	if ( slot < MAX_BOLT_ONS )
	{
		if ( ent->activeBoltOn != slot )
		{
			changed = qtrue;
		}
		ent->activeBoltOn = slot;
	}

	if ( ent == &g_entities[0] )
	{
		++g_SPXBHelmetGameP1Ensure;
		g_SPXBHelmetGameP1Slot = (unsigned int)slot;
	}
	else if ( ent->s.number == s_stefxSplitP2EntNum )
	{
		++g_SPXBHelmetGameP2Ensure;
		g_SPXBHelmetGameP2Slot = (unsigned int)slot;
	}

	if ( s_helmetEnsureLogBudget > 0 && ( changed || ( context && ( !Q_stricmp( context, "p1" ) || !Q_stricmp( context, "p2" ) ) ) ) )
	{
		XBLF( "STEFX_SPLIT_COOP helmet ensure %s ent=%d slot=%d removedHand=%d changed=%d map='%s' time=%d",
			context ? context : "<null>",
			ent->s.number,
			(int)slot,
			(int)handSlot,
			changed ? 1 : 0,
			level.mapname,
			level.time );
		--s_helmetEnsureLogBudget;
	}

	return slot;
}

static void STEFX_SplitCoopApplyInitialEVHelmet( gentity_t *p2 )
{
	static char s_initialHelmetMap[MAX_QPATH] = { 0 };
	static qboolean s_initialHelmetApplied = qfalse;
	gentity_t *p1 = &g_entities[0];

	if ( Q_stricmp( s_initialHelmetMap, level.mapname ) )
	{
		Q_strncpyz( s_initialHelmetMap, level.mapname, sizeof( s_initialHelmetMap ), qtrue );
		s_initialHelmetApplied = qfalse;
	}

	if ( s_initialHelmetApplied || !STEFX_SplitCoopMapStartsWithEVHelmet() )
	{
		return;
	}

	if ( !p1 || !p1->inuse || !p1->client || !p2 || !p2->inuse || !p2->client )
	{
		return;
	}

	STEFX_SplitCoopEnsureHelmetOnEntity( p1, "p1" );
	STEFX_SplitCoopEnsureHelmetOnEntity( p2, "p2" );
	s_initialHelmetApplied = qtrue;
}

static void STEFX_SplitCoopMaintainEVHelmets( const char *context )
{
	gentity_t *p1 = &g_entities[0];
	gentity_t *p2;

	if ( !STEFX_SplitCoopActive() )
	{
		return;
	}

	STEFX_SplitCoopMaintainP1EVHeadModel( context );

	if ( !STEFX_SplitCoopMapStartsWithEVHelmet() )
	{
		return;
	}

	p2 = STEFX_SplitCoopFindP2();
	if ( p1 && p1->inuse && STEFX_SplitCoopEntityUsesHazardSuit( p1 ) )
	{
		STEFX_SplitCoopEnsureHelmetOnEntity( p1, context ? context : "maintain-p1" );
	}
	if ( p2 && p2->inuse && STEFX_SplitCoopEntityUsesHazardSuit( p2 ) )
	{
		STEFX_SplitCoopEnsureHelmetOnEntity( p2, context ? context : "maintain-p2" );
	}
}

static const char *STEFX_SplitCoopBoltOnSlotName( gentity_t *ent, byte slot, int *indexOut )
{
	int boltOnIndex = -1;

	if ( indexOut )
	{
		*indexOut = -1;
	}

	if ( !ent || !ent->client || slot >= MAX_BOLT_ONS )
	{
		return "<invalid-slot>";
	}

	boltOnIndex = ent->client->renderInfo.boltOns[slot].index;
	if ( indexOut )
	{
		*indexOut = boltOnIndex;
	}

	if ( boltOnIndex < 0 || boltOnIndex >= numBoltOns )
	{
		return "<empty>";
	}

	return knownBoltOns[boltOnIndex].name;
}

static void STEFX_SplitCoopLogHelmetState( const char *context, gentity_t *p2 )
{
	static int s_helmetStateLogBudget = 256;
	static int s_lastLogTime = -999999;
	static char s_lastMap[MAX_QPATH] = { 0 };
	static byte s_lastP1Helmet = MAX_BOLT_ONS;
	static byte s_lastP1Hand = MAX_BOLT_ONS;
	static byte s_lastP2Helmet = MAX_BOLT_ONS;
	static byte s_lastP2Hand = MAX_BOLT_ONS;
	gentity_t *p1 = &g_entities[0];
	byte p1Helmet;
	byte p1Hand;
	byte p2Helmet;
	byte p2Hand;
	qboolean changed;
	int p1HelmetIndex;
	int p1HandIndex;
	int p2HelmetIndex;
	int p2HandIndex;
	const char *p1HelmetName;
	const char *p1HandName;
	const char *p2HelmetName;
	const char *p2HandName;

	if ( s_helmetStateLogBudget <= 0 || !STEFX_SplitCoopActive() )
	{
		return;
	}

	if ( !p1 || !p1->inuse || !p1->client )
	{
		return;
	}

	p1Helmet = G_BoltOnNumberForName( p1, "helmet" );
	p1Hand = G_BoltOnNumberForName( p1, "helmet_lhand" );
	p2Helmet = ( p2 && p2->inuse && p2->client ) ? G_BoltOnNumberForName( p2, "helmet" ) : MAX_BOLT_ONS;
	p2Hand = ( p2 && p2->inuse && p2->client ) ? G_BoltOnNumberForName( p2, "helmet_lhand" ) : MAX_BOLT_ONS;
	p1HelmetName = STEFX_SplitCoopBoltOnSlotName( p1, p1Helmet, &p1HelmetIndex );
	p1HandName = STEFX_SplitCoopBoltOnSlotName( p1, p1Hand, &p1HandIndex );
	p2HelmetName = STEFX_SplitCoopBoltOnSlotName( p2, p2Helmet, &p2HelmetIndex );
	p2HandName = STEFX_SplitCoopBoltOnSlotName( p2, p2Hand, &p2HandIndex );
	changed = (qboolean)( Q_stricmp( s_lastMap, level.mapname ) ||
		p1Helmet != s_lastP1Helmet ||
		p1Hand != s_lastP1Hand ||
		p2Helmet != s_lastP2Helmet ||
		p2Hand != s_lastP2Hand );

	if ( !changed && level.time - s_lastLogTime < 5000 )
	{
		return;
	}

	XBLF( "STEFX_SPLIT_COOP helmet state %s time=%d p1Slots=(helmet:%d idx:%d name:%s hand:%d idx:%d name:%s) p2=%d p2Slots=(helmet:%d idx:%d name:%s hand:%d idx:%d name:%s) p1Models=(%s,%s,%s) p2Models=(%s,%s,%s)",
		context ? context : "<null>",
		level.time,
		(int)p1Helmet,
		p1HelmetIndex,
		p1HelmetName,
		(int)p1Hand,
		p1HandIndex,
		p1HandName,
		p2 ? p2->s.number : -1,
		(int)p2Helmet,
		p2HelmetIndex,
		p2HelmetName,
		(int)p2Hand,
		p2HandIndex,
		p2HandName,
		p1->client->renderInfo.legsModelName,
		p1->client->renderInfo.torsoModelName,
		p1->client->renderInfo.headModelName,
		( p2 && p2->client ) ? p2->client->renderInfo.legsModelName : "<null>",
		( p2 && p2->client ) ? p2->client->renderInfo.torsoModelName : "<null>",
		( p2 && p2->client ) ? p2->client->renderInfo.headModelName : "<null>" );
	--s_helmetStateLogBudget;
	s_lastLogTime = level.time;
	Q_strncpyz( s_lastMap, level.mapname, sizeof( s_lastMap ), qtrue );
	s_lastP1Helmet = p1Helmet;
	s_lastP1Hand = p1Hand;
	s_lastP2Helmet = p2Helmet;
	s_lastP2Hand = p2Hand;
}

static qboolean STEFX_SplitCoopP2Valid( void )
{
	if ( s_stefxSplitP2EntNum <= 0 || s_stefxSplitP2EntNum >= MAX_GENTITIES )
	{
		return qfalse;
	}
	if ( !g_entities[s_stefxSplitP2EntNum].inuse || !g_entities[s_stefxSplitP2EntNum].client )
	{
		return qfalse;
	}
	return qtrue;
}

static gentity_t *STEFX_SplitCoopFindP2( void )
{
	gentity_t *ent = NULL;

	if ( STEFX_SplitCoopP2Valid() )
	{
		return &g_entities[s_stefxSplitP2EntNum];
	}

	while ( ( ent = G_Find( ent, FOFS( targetname ), (char *)STEFX_SPLIT_P2_TARGETNAME ) ) != NULL )
	{
		if ( ent->inuse && ent->client )
		{
			s_stefxSplitP2EntNum = ent->s.number;
			gi.cvar_set( "stefx_splitScreenP2Entity", va( "%d", s_stefxSplitP2EntNum ) );
			return ent;
		}
	}

	s_stefxSplitP2EntNum = ENTITYNUM_NONE;
	return NULL;
}

static qboolean STEFX_SplitCoopP1Ready( void )
{
	return (qboolean)( g_entities
		&& g_entities[0].inuse
		&& g_entities[0].client
		&& g_entities[0].health > 0 );
}

// Retain the last spawn boundary even if ordinary log output stops.
__declspec(dllexport) volatile unsigned int g_SPXBCoopSpawnStage = 0;
static void STEFX_CoopSpawnStage( unsigned int stage )
{
	static int logBudget = 24;
	g_SPXBCoopSpawnStage = stage;
	if (logBudget > 0)
	{
		--logBudget;
		XBLog_WriteCriticalf("STEFX_COOP_SPAWN_STAGE: stage=%u time=%d", stage, level.time);
	}
}
static gentity_t *STEFX_SplitCoopSpawnP2( void )
{
	static int s_spawnLogBudget = 16;
	gentity_t *p1;
	gentity_t *spawner;
	gentity_t *p2;
	vec3_t angles;
	vec3_t origin;
	const char *npcType;

	if ( !STEFX_SplitCoopP1Ready() )
	{
		return NULL;
	}

	p2 = STEFX_SplitCoopFindP2();
	if ( p2 )
	{
		return p2;
	}

	p1 = &g_entities[0];
	npcType = STEFX_SplitCoopP2NPCType();
	STEFX_CoopSpawnStage(1);
	STEFX_SplitCoopPlacementFromP1( origin, angles );
	STEFX_CoopSpawnStage(2);

	spawner = G_Spawn();
	STEFX_CoopSpawnStage(3);
	if ( !spawner )
	{
		XBLog_Write( "STEFX_SPLIT_COOP spawn failed: no EF spawner entity" );
		return NULL;
	}

	spawner->classname = "NPC_starfleet";
	spawner->NPC_type = G_NewString( npcType );
	spawner->NPC_targetname = G_NewString( STEFX_SPLIT_P2_TARGETNAME );
	spawner->count = 1;
	spawner->delay = 0;
	spawner->wait = 500;
	spawner->owner = p1;
	spawner->health = p1->max_health > 0 ? p1->max_health : 100;
	VectorCopy( origin, spawner->s.origin );
	VectorCopy( angles, spawner->s.angles );
	G_SetOrigin( spawner, origin );
	gi.linkentity( spawner );

	if ( s_spawnLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP spawn request p1Sex='%s' p2NPC='%s' origin=(%g,%g,%g) angles=(%g,%g,%g)",
			g_sex && g_sex->string ? g_sex->string : "<null>",
			npcType,
			origin[0],
			origin[1],
			origin[2],
			angles[0],
			angles[1],
			angles[2] );
		--s_spawnLogBudget;
	}

	STEFX_CoopSpawnStage(4);
	NPC_Spawn( spawner, spawner, spawner );
	STEFX_CoopSpawnStage(5);
	spawner->e_ThinkFunc = thinkF_G_FreeEntity;
	spawner->nextthink = level.time + FRAMETIME;

	p2 = STEFX_SplitCoopFindP2();
	if ( !p2 )
	{
		gi.cvar_set( "stefx_splitScreenP2Entity", "-1" );
		STEFX_CoopSpawnStage(6);
		return NULL;
	}
	STEFX_CoopSpawnStage(7);

	XBLF( "STEFX_SPLIT_COOP spawn found ent=%d npc='%s' flags=0x%x eFlags=0x%x think=%d nextthink=%d",
		p2->s.number,
		p2->NPC_type ? p2->NPC_type : "<null>",
		p2->flags,
		p2->s.eFlags,
		p2->e_ThinkFunc,
		p2->nextthink );

	VectorCopy( origin, p2->client->ps.origin );
	VectorClear( p2->client->ps.velocity );
	p2->client->ps.pm_time = 0;
	p2->client->ps.pm_flags &= ~( PMF_ALL_TIMES | PMF_RESPAWNED );
	p2->client->renderInfo.lookTarget = ENTITYNUM_NONE;
	G_SetOrigin( p2, origin );
	SetClientViewAngle( p2, angles );
	p2->s.eFlags ^= EF_TELEPORT_BIT;
	gi.linkentity( p2 );

	if ( s_spawnLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP spawn placed ent=%d origin=(%g,%g,%g) angles=(%g,%g,%g)",
			p2->s.number,
			origin[0],
			origin[1],
			origin[2],
			angles[0],
			angles[1],
			angles[2] );
		--s_spawnLogBudget;
	}

	STEFX_CoopSpawnStage(8);
	return p2;
}

static qboolean STEFX_SplitCoopTryPlacement( const vec3_t candidate, vec3_t origin )
{
	trace_t floorTrace;
	trace_t bodyTrace;
	vec3_t start;
	vec3_t end;
	vec3_t mins;
	vec3_t maxs;
	const int clipmask = ( MASK_NPCSOLID & ~CONTENTS_BODY );

	VectorCopy( candidate, start );
	start[2] += 32.0f;
	VectorCopy( candidate, end );
	end[2] -= 96.0f;

	gi.trace( &floorTrace, start, NULL, NULL, end, ENTITYNUM_NONE, clipmask );
	if ( floorTrace.allsolid || floorTrace.startsolid || floorTrace.fraction >= 1.0f || floorTrace.plane.normal[2] < 0.7f )
	{
		return qfalse;
	}

	VectorSet( mins, DEFAULT_MINS_0, DEFAULT_MINS_1, DEFAULT_MINS_2 );
	VectorSet( maxs, DEFAULT_MAXS_0, DEFAULT_MAXS_1, DEFAULT_MAXS_2 );
	VectorCopy( floorTrace.endpos, origin );
	origin[2] -= DEFAULT_MINS_2;
	origin[2] += 0.125f;

	gi.trace( &bodyTrace, origin, mins, maxs, origin, ENTITYNUM_NONE, clipmask );
	if ( bodyTrace.allsolid || bodyTrace.startsolid )
	{
		return qfalse;
	}

	return qtrue;
}

static void STEFX_SplitCoopPlacementFromP1( vec3_t origin, vec3_t angles )
{
	static int s_placementLogBudget = 32;
	static const float offsets[][2] = {
		{ 0.0f, -64.0f },
		{ 48.0f, 16.0f },
		{ -48.0f, 16.0f },
		{ 56.0f, -32.0f },
		{ -56.0f, -32.0f },
		{ 0.0f, 64.0f },
		{ 80.0f, 0.0f },
		{ -80.0f, 0.0f }
	};
	gentity_t *p1 = &g_entities[0];
	vec3_t forward;
	vec3_t right;
	vec3_t candidate;
	int i;

	VectorCopy( p1->client->ps.viewangles, angles );
	angles[PITCH] = 0.0f;
	angles[ROLL] = 0.0f;
	AngleVectors( angles, forward, right, NULL );

	for ( i = 0; i < (int)( sizeof( offsets ) / sizeof( offsets[0] ) ); ++i )
	{
		VectorCopy( p1->currentOrigin, candidate );
		VectorMA( candidate, offsets[i][0], right, candidate );
		VectorMA( candidate, offsets[i][1], forward, candidate );
		if ( STEFX_SplitCoopTryPlacement( candidate, origin ) )
		{
			if ( s_placementLogBudget > 0 )
			{
				XBLF( "STEFX_SPLIT_COOP placement idx=%d p1=(%g,%g,%g) candidate=(%g,%g,%g) origin=(%g,%g,%g)",
					i,
					p1->currentOrigin[0],
					p1->currentOrigin[1],
					p1->currentOrigin[2],
					candidate[0],
					candidate[1],
					candidate[2],
					origin[0],
					origin[1],
					origin[2] );
				--s_placementLogBudget;
			}
			return;
		}
	}

	VectorCopy( p1->currentOrigin, origin );
	VectorMA( origin, 48.0f, right, origin );
	VectorMA( origin, 16.0f, forward, origin );
	origin[2] += 8.0f;
	if ( s_placementLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP placement fallback p1=(%g,%g,%g) origin=(%g,%g,%g)",
			p1->currentOrigin[0],
			p1->currentOrigin[1],
			p1->currentOrigin[2],
			origin[0],
			origin[1],
			origin[2] );
		--s_placementLogBudget;
	}
}

void STEFX_SplitCoopFollowP1ScriptedMove( const char *context )
{
	static int s_followLogBudget = 24;
	gentity_t *p2;
	vec3_t origin;
	vec3_t angles;

	if ( !STEFX_SplitCoopActive() || !STEFX_SplitCoopP1Ready() )
	{
		return;
	}

	p2 = STEFX_SplitCoopFindP2();
	if ( !p2 || !p2->client )
	{
		return;
	}

	STEFX_SplitCoopPlacementFromP1( origin, angles );
	VectorClear( p2->client->ps.velocity );
	p2->client->ps.pm_time = 0;
	p2->client->ps.pm_flags &= ~( PMF_ALL_TIMES | PMF_RESPAWNED );
	p2->client->renderInfo.lookTarget = ENTITYNUM_NONE;
	G_SetOrigin( p2, origin );
	SetClientViewAngle( p2, angles );
	p2->s.eFlags ^= EF_TELEPORT_BIT;
	gi.linkentity( p2 );

	if ( s_followLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP scripted follow context='%s' p1=(%g,%g,%g) p2=%d origin=(%g,%g,%g) angles=(%g,%g,%g)",
			context ? context : "<null>",
			g_entities[0].currentOrigin[0],
			g_entities[0].currentOrigin[1],
			g_entities[0].currentOrigin[2],
			p2->s.number,
			origin[0],
			origin[1],
			origin[2],
			angles[0],
			angles[1],
			angles[2] );
		--s_followLogBudget;
	}
}

static qboolean STEFX_SplitCoopP2Dead( const gentity_t *p2 )
{
	if ( !p2 || !p2->client )
	{
		return qtrue;
	}
	return (qboolean)( p2->health <= 0
		|| p2->client->ps.stats[STAT_HEALTH] <= 0
		|| p2->client->ps.pm_type == PM_DEAD
		|| ( p2->s.eFlags & EF_DEAD ) );
}

static void STEFX_SplitCoopRecoverP2( gentity_t *p2 )
{
	static int s_recoverLogBudget = 16;
	vec3_t origin;
	vec3_t angles;
	int oldHealth;
	int oldPmType;
	int maxHealth;

	if ( !p2 || !p2->client || !STEFX_SplitCoopP1Ready() )
	{
		return;
	}

	oldHealth = p2->health;
	oldPmType = p2->client->ps.pm_type;
	maxHealth = p2->max_health > 0 ? p2->max_health : 100;
	STEFX_SplitCoopPlacementFromP1( origin, angles );

	p2->health = maxHealth;
	p2->client->ps.stats[STAT_MAX_HEALTH] = maxHealth;
	p2->client->ps.stats[STAT_HEALTH] = maxHealth;
	p2->client->ps.pm_type = PM_NORMAL;
	p2->client->ps.pm_flags &= ~( PMF_ALL_TIMES | PMF_RESPAWNED );
	p2->client->ps.pm_time = 0;
	p2->client->ps.weaponTime = 0;
	p2->client->ps.weaponstate = WEAPON_READY;
	p2->client->ps.legsAnimTimer = 0;
	p2->client->ps.torsoAnimTimer = 0;
	VectorClear( p2->client->ps.velocity );
	VectorCopy( origin, p2->client->ps.origin );
	p2->client->respawnTime = level.time;
	p2->client->renderInfo.lookTarget = ENTITYNUM_NONE;

	p2->takedamage = qtrue;
	p2->contents = CONTENTS_BODY;
	p2->clipmask = MASK_NPCSOLID;
	p2->maxs[2] = DEFAULT_MAXS_2;
	p2->s.eFlags &= ~( EF_DEAD | EF_NODRAW );
	p2->s.eFlags |= EF_NPC;
	p2->s.eFlags ^= EF_TELEPORT_BIT;
	p2->flags &= ~FL_NOTARGET;
	p2->s.powerups = 0;
	p2->enemy = NULL;
	p2->lastEnemy = NULL;
	p2->activator = NULL;
	p2->s.loopSound = 0;
	memset( p2->client->ps.powerups, 0, sizeof( p2->client->ps.powerups ) );
	VectorClear( p2->client->damage_from );
	p2->client->damage_armor = 0;
	p2->client->damage_blood = 0;
	p2->client->damage_knockback = 0;

	G_SetOrigin( p2, origin );
	SetClientViewAngle( p2, angles );
	NPC_SetAnim( p2, SETANIM_BOTH, BOTH_STAND1, SETANIM_FLAG_OVERRIDE );
	STEFX_SplitCoopEnsureBaseLoadout( p2 );
	STEFX_SplitCoopApplyP2Model( p2 );
	gi.linkentity( p2 );

	if ( s_recoverLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP recover ent=%d oldHealth=%d newHealth=%d oldPm=%d origin=(%g,%g,%g) angles=(%g,%g,%g)",
			p2->s.number,
			oldHealth,
			p2->health,
			oldPmType,
			origin[0],
			origin[1],
			origin[2],
			angles[0],
			angles[1],
			angles[2] );
		--s_recoverLogBudget;
	}
}

static void STEFX_SplitCoopTestKillP2( gentity_t *p2 )
{
	static int s_lastForcedKillSerial = 0;
	static int s_forceLogBudget = 8;
	int serial;

	if ( !p2 || !p2->client )
	{
		return;
	}

	serial = gi.Cvar_VariableIntegerValue( "stefx_splitScreenTestKillP2" );
	if ( !serial || serial == s_lastForcedKillSerial || STEFX_SplitCoopP2Dead( p2 ) )
	{
		return;
	}

	s_lastForcedKillSerial = serial;
	p2->health = 0;
	p2->client->ps.stats[STAT_HEALTH] = 0;
	p2->client->ps.pm_type = PM_DEAD;
	p2->s.eFlags |= EF_DEAD;
	p2->takedamage = qfalse;

	if ( s_forceLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP test-kill ent=%d serial=%d",
			p2->s.number,
			serial );
		--s_forceLogBudget;
	}
}

static qboolean STEFX_SplitCoopP2ReadyForControl( gentity_t *p2 )
{
	return (qboolean)( p2
		&& p2->inuse
		&& p2->client
		&& !( p2->s.eFlags & EF_NODRAW )
		&& p2->e_ThinkFunc != thinkF_NPC_Begin );
}

static void STEFX_SplitCoopEnsureBaseLoadout( gentity_t *p2 )
{
	static int s_loadoutLogBudget = 16;
	int ammoIndex;
	int oldWeapon;
	int oldWeapons;
	int desiredWeapon;
	qboolean changed = qfalse;

	if ( !p2 || !p2->client )
	{
		return;
	}

	oldWeapon = p2->client->ps.weapon;
	oldWeapons = p2->client->ps.stats[STAT_WEAPONS];
	desiredWeapon = STEFX_SplitCoopP1Disguised() ? WP_SCAVENGER_RIFLE : WP_COMPRESSION_RIFLE;

	p2->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_NONE );
	p2->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_PHASER );
	p2->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_COMPRESSION_RIFLE );
	if ( desiredWeapon == WP_SCAVENGER_RIFLE )
	{
		p2->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SCAVENGER_RIFLE );
	}

	ammoIndex = weaponData[WP_PHASER].ammoIndex;
	if ( ammoIndex >= 0 && ammoIndex < MAX_AMMO && p2->client->ps.ammo[ammoIndex] <= 0 )
	{
		p2->client->ps.ammo[ammoIndex] = ammoData[ammoIndex].max;
		changed = qtrue;
	}

	ammoIndex = weaponData[WP_COMPRESSION_RIFLE].ammoIndex;
	if ( ammoIndex >= 0 && ammoIndex < MAX_AMMO && p2->client->ps.ammo[ammoIndex] <= 0 )
	{
		p2->client->ps.ammo[ammoIndex] = ammoData[ammoIndex].max;
		changed = qtrue;
	}

	ammoIndex = weaponData[WP_SCAVENGER_RIFLE].ammoIndex;
	if ( desiredWeapon == WP_SCAVENGER_RIFLE && ammoIndex >= 0 && ammoIndex < MAX_AMMO && p2->client->ps.ammo[ammoIndex] <= 0 )
	{
		p2->client->ps.ammo[ammoIndex] = ammoData[ammoIndex].max;
		changed = qtrue;
	}

	if ( desiredWeapon == WP_SCAVENGER_RIFLE && p2->client->ps.weapon != WP_SCAVENGER_RIFLE )
	{
		if ( p2->NPC )
		{
			ChangeWeapon( p2, WP_SCAVENGER_RIFLE );
		}
		p2->client->ps.weapon = WP_SCAVENGER_RIFLE;
		p2->client->ps.weaponstate = WEAPON_READY;
		p2->s.weapon = WP_SCAVENGER_RIFLE;
		changed = qtrue;
	}
	else if ( p2->client->ps.weapon <= WP_NONE || p2->client->ps.weapon >= WP_NUM_WEAPONS )
	{
		p2->client->ps.weapon = desiredWeapon;
		p2->s.weapon = desiredWeapon;
		changed = qtrue;
	}
	else if ( p2->s.weapon == WP_NONE )
	{
		p2->s.weapon = p2->client->ps.weapon;
		changed = qtrue;
	}

	if ( p2->NPC )
	{
		ammoIndex = weaponData[p2->client->ps.weapon].ammoIndex;
		if ( ammoIndex >= 0 && ammoIndex < MAX_AMMO )
		{
			p2->NPC->currentAmmo = p2->client->ps.ammo[ammoIndex];
		}
	}

	if ( s_loadoutLogBudget > 0 && ( changed || oldWeapon != p2->client->ps.weapon || oldWeapons != p2->client->ps.stats[STAT_WEAPONS] ) )
	{
		XBLF( "STEFX_SPLIT_COOP loadout ent=%d oldWeapon=%d newWeapon=%d desiredWeapon=%d p1Team=%d oldBits=0x%x newBits=0x%x ammo=(%d,%d,%d,%d)",
			p2->s.number,
			oldWeapon,
			p2->client->ps.weapon,
			desiredWeapon,
			STEFX_SplitCoopP1Team(),
			oldWeapons,
			p2->client->ps.stats[STAT_WEAPONS],
			p2->client->ps.ammo[0],
			p2->client->ps.ammo[1],
			p2->client->ps.ammo[2],
			p2->client->ps.ammo[3] );
		--s_loadoutLogBudget;
	}
}

static void STEFX_SplitCoopTakeControl( gentity_t *p2 )
{
	if ( !p2 || !p2->client )
	{
		return;
	}

	p2->client->playerTeam = STEFX_SplitCoopP1Team();
	p2->client->enemyTeam = STEFX_SplitCoopP1EnemyTeam();
	p2->client->ps.persistant[PERS_TEAM] = p2->client->playerTeam;
	if ( p2->NPC )
	{
		p2->NPC->behaviorState = BS_WAIT;
		p2->NPC->defaultBehavior = BS_WAIT;
		p2->NPC->scriptFlags = 0;
	}
	p2->flags &= ~FL_NOTARGET;
	p2->svFlags &= ~( SVF_NOCLIENT | SVF_BROADCAST );
	p2->s.eFlags &= ~EF_NODRAW;
	p2->client->ps.eFlags &= ~EF_NODRAW;
	p2->e_ThinkFunc = thinkF_NULL;
	p2->nextthink = 0;
	STEFX_SplitCoopEnsureBaseLoadout( p2 );
	STEFX_SplitCoopApplyP2Model( p2 );
	STEFX_SplitCoopApplyInitialEVHelmet( p2 );
	STEFX_SplitCoopSyncHelmetBoltons( p2 );
	STEFX_SplitCoopMaintainEVHelmets( "take-control" );
	STEFX_SplitCoopLogHelmetState( "take-control", p2 );
}

static qboolean STEFX_SplitCoopWeaponSelectable( const gentity_t *p2, int weapon )
{
	if ( !p2 || !p2->client )
	{
		return qfalse;
	}
	if ( weapon < FIRST_WEAPON || weapon > MAX_PLAYER_WEAPONS )
	{
		return qfalse;
	}
	return (qboolean)( p2->client->ps.stats[STAT_WEAPONS] & ( 1 << weapon ) );
}

static int STEFX_SplitCoopChooseUsableWeapon( const gentity_t *p2, int preferredWeapon )
{
	if ( STEFX_SplitCoopWeaponSelectable( p2, preferredWeapon ) && preferredWeapon != WP_NONE )
	{
		return preferredWeapon;
	}
	if ( STEFX_SplitCoopWeaponSelectable( p2, WP_COMPRESSION_RIFLE ) )
	{
		return WP_COMPRESSION_RIFLE;
	}
	if ( STEFX_SplitCoopWeaponSelectable( p2, WP_PHASER ) )
	{
		return WP_PHASER;
	}
	return WP_NONE;
}

static int STEFX_SplitCoopCycleWeapon( const gentity_t *p2, int currentWeapon, int weaponDelta )
{
	int i;
	int weapon;
	const int direction = weaponDelta > 0 ? 1 : -1;

	if ( !p2 || !p2->client || !weaponDelta )
	{
		return currentWeapon;
	}
	if ( currentWeapon == WP_BLUE_HYPO || currentWeapon == WP_RED_HYPO )
	{
		return currentWeapon;
	}

	weapon = currentWeapon;
	for ( i = 0; i <= MAX_PLAYER_WEAPONS; ++i )
	{
		weapon += direction;
		if ( weapon < FIRST_WEAPON || weapon > MAX_PLAYER_WEAPONS )
		{
			weapon = direction > 0 ? FIRST_WEAPON : MAX_PLAYER_WEAPONS;
		}
		if ( STEFX_SplitCoopWeaponSelectable( p2, weapon ) )
		{
			return weapon;
		}
	}

	return currentWeapon;
}

// STEFX_COOP_WEAPON_REQUEST_BEGIN
static int STEFX_SplitCoopWeaponRequest( const gentity_t *p2, int weaponDelta, int time )
{
	static const gentity_t *owner;
	static const gclient_t *client;
	static int lastTime, lastActual, selected;
	if ( !p2 || !p2->client )
	{
		owner = NULL;
		client = NULL;
		return WP_NONE;
	}
	const int actual = p2->client->ps.weapon;
	// Keep a button-edge request until PM_FinishWeaponChange consumes it.
	// Player replacement, time resets and script changes establish a new base.
	if ( owner != p2 || client != p2->client || time < lastTime ||
		actual != lastActual || !STEFX_SplitCoopWeaponSelectable( p2, selected ) )
	{
		selected = actual;
	}
	owner = p2;
	client = p2->client;
	lastTime = time;
	lastActual = actual;
	if ( !STEFX_SplitCoopWeaponSelectable( p2, selected ) || selected == WP_NONE )
	{
		const int preferred = g_entities[0].client ? g_entities[0].client->ps.weapon : WP_NONE;
		selected = STEFX_SplitCoopChooseUsableWeapon( p2, preferred );
	}
	selected = STEFX_SplitCoopCycleWeapon( p2, selected, weaponDelta );
	return selected;
}
// STEFX_COOP_WEAPON_REQUEST_END

static void STEFX_SplitCoopRunFrame( void )
{
	static qboolean s_loggedInactive = qfalse;
	static qboolean s_presentationCatchupDone = qfalse;
	static qboolean s_presentationAnchorValid = qfalse;
	static vec3_t s_presentationP1Origin;
	static vec3_t s_presentationP2Origin;
	static int s_frameLogBudget = 80;
	static int s_stateLogBudget = 64;
	static int s_weaponLogBudget = 24;
	gentity_t *p2;
	usercmd_t cmd;
	vec3_t outAngles;
	int sourcePort = -1;
	int weaponDelta = 0;
	int oldWeapon;
	qboolean hasP2Input;
	int splitCvar;
	int playersCvar;
	int p2Cvar;

	splitCvar = gi.Cvar_VariableIntegerValue( "stefx_splitScreen" );
	playersCvar = gi.Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" );
	p2Cvar = gi.Cvar_VariableIntegerValue( "stefx_splitScreenP2Entity" );
	if ( s_stateLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_COOP state time=%d split=%d players=%d p2Cvar=%d cachedP2=%d p1Ready=%d",
			level.time,
			splitCvar,
			playersCvar,
			p2Cvar,
			s_stefxSplitP2EntNum,
			STEFX_SplitCoopP1Ready() ? 1 : 0 );
		--s_stateLogBudget;
	}

	if ( !STEFX_SplitCoopActive() )
	{
		s_presentationCatchupDone = qfalse;
		s_presentationAnchorValid = qfalse;
		if ( !s_loggedInactive )
		{
			gi.cvar_set( "stefx_splitScreenP2Entity", "-1" );
			s_stefxSplitP2EntNum = ENTITYNUM_NONE;
			s_loggedInactive = qtrue;
		}
		return;
	}
	s_loggedInactive = qfalse;

	if ( level.framenum <= 3 )
	{
		static int s_startupSpawnDeferLogBudget = 3;
		STEFX_GAME_TRACE_STAGE(0x53504C54, 78); /* SPLT */
		STEFX_GAME_TRACE_DETAIL((unsigned int)level.framenum, (unsigned int)level.time, (unsigned int)s_stefxSplitP2EntNum, 0);
		if ( s_startupSpawnDeferLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_COOP deferring P2 spawn/control during startup settle frame=%d time=%d",
				level.framenum,
				level.time );
			--s_startupSpawnDeferLogBudget;
		}
		return;
	}

	p2 = STEFX_SplitCoopFindP2();
	if ( !p2 )
	{
		p2 = STEFX_SplitCoopSpawnP2();
		if ( !p2 )
		{
			return;
		}
		VectorCopy( g_entities[0].currentOrigin, s_presentationP1Origin );
		VectorCopy( p2->currentOrigin, s_presentationP2Origin );
		s_presentationAnchorValid = qtrue;
		s_presentationCatchupDone = qfalse;
	}

	if ( !STEFX_SplitCoopP2ReadyForControl( p2 ) )
	{
		if ( s_frameLogBudget > 72 )
		{
			XBLF( "STEFX_SPLIT_COOP waiting ent=%d eFlags=0x%x think=%d nextthink=%d",
				p2->s.number,
				p2->s.eFlags,
				p2->e_ThinkFunc,
				p2->nextthink );
			--s_frameLogBudget;
		}
		return;
	}

	if ( ( level.framenum & 31 ) == 0 )
	{
		STEFX_SplitCoopLogHelmetState( "run-frame", p2 );
	}

	STEFX_SplitCoopTestKillP2( p2 );
	if ( STEFX_SplitCoopP2Dead( p2 ) )
	{
		STEFX_SplitCoopRecoverP2( p2 );
	}

	STEFX_SplitCoopTakeControl( p2 );
	if ( s_presentationAnchorValid && !s_presentationCatchupDone )
	{
		vec3_t p1Delta;
		vec3_t p2Delta;
		VectorSubtract( g_entities[0].currentOrigin, s_presentationP1Origin, p1Delta );
		VectorSubtract( p2->currentOrigin, s_presentationP2Origin, p2Delta );
		if ( VectorLengthSquared( p1Delta ) > ( 192.0f * 192.0f )
			&& VectorLengthSquared( p2Delta ) < ( 48.0f * 48.0f ) )
		{
			XBLF( "STEFX_SPLIT_COOP presentation catchup p1DeltaSq=%g p2DeltaSq=%g p1=(%g,%g,%g) p2=(%g,%g,%g)",
				VectorLengthSquared( p1Delta ),
				VectorLengthSquared( p2Delta ),
				g_entities[0].currentOrigin[0],
				g_entities[0].currentOrigin[1],
				g_entities[0].currentOrigin[2],
				p2->currentOrigin[0],
				p2->currentOrigin[1],
				p2->currentOrigin[2] );
			STEFX_SplitCoopFollowP1ScriptedMove( "presentation-catchup" );
			s_presentationCatchupDone = qtrue;
		}
	}
	if ( level.framenum <= 3 )
	{
		static int s_startupDeferLogBudget = 3;
		STEFX_GAME_TRACE_STAGE(0x53504C54, 79); /* SPLT */
		STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)level.framenum, (unsigned int)level.time, (unsigned int)p2->e_ThinkFunc);
		if ( s_startupDeferLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_COOP deferring P2 control during startup settle frame=%d time=%d ent=%d",
				level.framenum,
				level.time,
				p2->s.number );
			--s_startupDeferLogBudget;
		}
		return;
	}

	hasP2Input = CL_STEFX_SplitScreen_BuildP2Usercmd( &cmd, p2->client->ps.viewangles, p2->client->ps.delta_angles, level.time, &sourcePort, &weaponDelta, outAngles );
	if ( !hasP2Input )
	{
		memset( &cmd, 0, sizeof( cmd ) );
		cmd.serverTime = level.time;
		cmd.angles[PITCH] = ANGLE2SHORT( p2->client->ps.viewangles[PITCH] ) - p2->client->ps.delta_angles[PITCH];
		cmd.angles[YAW] = ANGLE2SHORT( p2->client->ps.viewangles[YAW] ) - p2->client->ps.delta_angles[YAW];
		cmd.angles[ROLL] = 0;
	}

	oldWeapon = p2->client->ps.weapon;
	cmd.weapon = STEFX_SplitCoopWeaponRequest( p2, weaponDelta, level.time );
	if ( s_weaponLogBudget > 0 && weaponDelta )
	{
		XBLF( "STEFX_SPLIT_COOP weapon cycle ent=%d delta=%d old=%d new=%d bits=0x%x",
			p2->s.number,
			weaponDelta,
			oldWeapon,
			cmd.weapon,
			p2->client->ps.stats[STAT_WEAPONS] );
		--s_weaponLogBudget;
	}

	STEFX_GAME_TRACE_STAGE(0x53504C54, 80); /* SPLT */
	STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)cmd.serverTime, (unsigned int)cmd.buttons, (unsigned int)cmd.weapon);
	ClientThink( p2->s.number, &cmd );
	STEFX_GAME_TRACE_STAGE(0x53504C54, 81); /* SPLT */
	STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)p2->client->ps.stats[STAT_HEALTH], (unsigned int)p2->health, (unsigned int)p2->linked);
	ClientEndFrame( p2 );
	STEFX_GAME_TRACE_STAGE(0x53504C54, 82); /* SPLT */
	STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)p2->client->ps.stats[STAT_HEALTH], (unsigned int)p2->health, (unsigned int)p2->linked);
	PlayerStateToEntityState( &p2->client->ps, &p2->s );
	STEFX_GAME_TRACE_STAGE(0x53504C54, 83); /* SPLT */
	STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)p2->s.eType, (unsigned int)p2->s.eFlags, (unsigned int)p2->linked);
	gi.linkentity( p2 );
	STEFX_GAME_TRACE_STAGE(0x53504C54, 84); /* SPLT */
	STEFX_GAME_TRACE_DETAIL((unsigned int)p2->s.number, (unsigned int)p2->linked, (unsigned int)p2->contents, (unsigned int)p2->health);

	if ( s_frameLogBudget > 0 && ( hasP2Input || s_frameLogBudget > 56 ) )
	{
		XBLF( "STEFX_SPLIT_COOP frame time=%d ent=%d input=%d port=%d npc='%s' origin=(%g,%g,%g) view=(%g,%g,%g) move=(%d,%d,%d) buttons=0x%x weapon=%d health=%d",
			level.time,
			p2->s.number,
			hasP2Input ? 1 : 0,
			sourcePort,
			p2->NPC_type ? p2->NPC_type : "<null>",
			p2->currentOrigin[0],
			p2->currentOrigin[1],
			p2->currentOrigin[2],
			p2->client->ps.viewangles[PITCH],
			p2->client->ps.viewangles[YAW],
			p2->client->ps.viewangles[ROLL],
			cmd.forwardmove,
			cmd.rightmove,
			cmd.upmove,
			cmd.buttons,
			p2->client->ps.weapon,
			p2->health );
		--s_frameLogBudget;
	}
}
#endif
/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=1, e=g_entities,i ; i < globals.num_entities ; i++,e++ ){
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < globals.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

	gi.Printf ("%i teams with %i entities\n", c, c2);
}


/*
============
G_InitCvars

============
*/
void G_InitCvars( void ) {
	// don't override the cheat state set by the system
	g_cheats = gi.cvar ("sv_cheats", "", 0);
	g_developer = gi.cvar ("developer", "", 0);

	// noset vars
	gi.cvar( "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM );
	gi.cvar( "gamedate", __DATE__ , CVAR_ROM );
	g_skippingcin = gi.cvar ("skippingCinematic", "0", CVAR_ROM);

	// latched vars

	// change anytime vars
	g_speed = gi.cvar( "g_speed", "250", 0 );
	g_gravity = gi.cvar( "g_gravity", "800", CVAR_USERINFO );	//using userinfo as savegame flag
	g_sex = gi.cvar ("sex", "male", CVAR_USERINFO | CVAR_ARCHIVE );
	g_spskill = gi.cvar ("g_spskill", "0", CVAR_ARCHIVE | CVAR_USERINFO);	//using userinfo as savegame flag
	g_knockback = gi.cvar( "g_knockback", "1000", 0 );
	g_inactivity = gi.cvar ("g_inactivity", "0", 0);
	g_debugMove = gi.cvar ("g_debugMove", "0", 0);
	g_debugDamage = gi.cvar ("g_debugDamage", "0", 0);
	g_ICARUSDebug = gi.cvar( "g_ICARUSDebug", "0", 0 );
	g_timescale = gi.cvar( "timescale", "1", 0 );
	g_virtualVoyager = gi.cvar( "cg_virtualvoyager", "0", CVAR_NORESTART );

	g_subtitles = gi.cvar ("g_subtitles", "2", CVAR_ARCHIVE);
	g_language = gi.cvar ("g_language", "", CVAR_ARCHIVE | CVAR_NORESTART);
	com_buildScript = gi.cvar ("com_buildscript", "0", 0);
}

/*
============
InitGame

============
*/
extern void Q3_SetPrecacheFile (const char *file);	//q3_interface

// I'm just declaring a global here which I need to get at in NAV_GenerateSquadPaths for deciding if pre-calc'd
//	data is valid, and this saves changing the proto of G_SpawnEntitiesFromString() to include a checksum param which
//	may get changed anyway if a new nav system is ever used. This way saves messing with g_local.h each time -slc
int giMapChecksum;	
SavedGameJustLoaded_e g_eSavedGameJustLoaded;
qboolean g_qbLoadTransition = qfalse;
void InitGame(  const char *mapname, const char *spawntarget, int checkSum, const char *entities, int levelTime, int randomSeed, int globalTime, SavedGameJustLoaded_e eSavedGameJustLoaded, qboolean qbLoadTransition )
{
	int		i;

#ifdef _XBOX
	XBLF("STEFX: InitGame enter map='%s' spawn='%s' checksum=%d entities=%p levelTime=%d randomSeed=%d globalTime=%d",
		mapname ? mapname : "(null)",
		spawntarget ? spawntarget : "(null)",
		checkSum,
		entities,
		levelTime,
		randomSeed,
		globalTime);
#endif
	giMapChecksum = checkSum;
	g_eSavedGameJustLoaded = eSavedGameJustLoaded;
	g_qbLoadTransition = qbLoadTransition;

#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before gi.Printf header");
#endif
	gi.Printf ("------- Game Initialization -------\n");
	gi.Printf ("gamename: %s\n", GAMEVERSION);
	gi.Printf ("gamedate: %s\n", __DATE__);

#ifdef _XBOX
	XBLF("STEFX: InitGame before srand seed=%d", randomSeed);
#endif
	srand( randomSeed );

#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before G_InitCvars");
#endif
	G_InitCvars();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_InitCvars before G_InitMemory");
#endif

	G_InitMemory();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_InitMemory before level memset");
#endif

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.globalTime = globalTime;
	Q_strncpyz( level.mapname, mapname, sizeof(level.mapname) );
	if ( spawntarget != NULL && spawntarget[0] )
	{
		Q_strncpyz( level.spawntarget, spawntarget, sizeof(level.spawntarget) );
	}
	else
	{
		level.spawntarget[0] = 0;
	}


#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before G_InitWorldSession");
#endif
	G_InitWorldSession();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_InitWorldSession before entity alloc");
#endif

	// initialize all entities for this game
	g_entities =  (struct gentity_s *) G_Alloc( MAX_GENTITIES * sizeof(g_entities[0]) );
#ifdef _XBOX
	XBLF("STEFX: InitGame after G_Alloc entities ptr=%p bytes=%d", g_entities, (int)(MAX_GENTITIES * sizeof(g_entities[0])));
#endif
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	globals.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = 1;
	level.clients = (struct gclient_s *) G_Alloc( level.maxclients * sizeof(level.clients[0]) );
#ifdef _XBOX
	XBLF("STEFX: InitGame after client alloc ptr=%p bytes=%d", level.clients, (int)(level.maxclients * sizeof(level.clients[0])));
#endif

	// set client fields on player
	g_entities[0].client = level.clients;

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	globals.num_entities = MAX_CLIENTS;

	//Load bolt-on list
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before G_LoadBoltOns");
#endif
	G_LoadBoltOns();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_LoadBoltOns before G_SquadPathsInit");
#endif
	
	//Sets intial squadpoint data
	G_SquadPathsInit();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_SquadPathsInit before NPC_InitGame");
#endif

	//Set up NPC init data
	NPC_InitGame();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after NPC_InitGame before TIMER_Clear");
#endif
	
	TIMER_Clear();

	//
	//ICARUS INIT START

	gi.Printf("------ ICARUS Initialization ------\n");
	gi.Printf("ICARUS version : %1.2f\n", ICARUS_VERSION);

#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before Interface_Init");
#endif
	Interface_Init( &interface_export );
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after Interface_Init before ICARUS_Init");
#endif
	ICARUS_Init();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after ICARUS_Init");
#endif

	gi.Printf ("-----------------------------------\n");

	//ICARUS INIT END
	//

#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before IT_LoadItemParms");
#endif
	IT_LoadItemParms ();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after IT_LoadItemParms before IS_LoadInfoItemParms");
#endif
	IS_LoadInfoItemParms();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after IS_LoadInfoItemParms before OBJ_LoadObjectives");
#endif

	OBJ_LoadObjectives();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after OBJ_LoadObjectives before OBJ_LoadTactical");
#endif

	OBJ_LoadTactical();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after OBJ_LoadTactical before ClearRegisteredItems");
#endif

	ClearRegisteredItems();

#ifdef _XBOX
	XBLF("STEFX: InitGame before navigator.Load map='%s' checksum=%d", mapname ? mapname : "(null)", checkSum);
#endif
	navCalculatePaths	= ( navigator.Load( mapname, checkSum ) == qfalse );
#ifdef _XBOX
	XBLF("STEFX: InitGame after navigator.Load navCalculatePaths=%d before spawn", navCalculatePaths);
#endif

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString( entities );
#ifdef _XBOX
	XBLF("STEFX: InitGame after G_SpawnEntitiesFromString num_entities=%d before G_FindTeams", globals.num_entities);
#endif

	// general initialization
	G_FindTeams();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after G_FindTeams before SaveRegisteredItems");
#endif

	SaveRegisteredItems();
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame after SaveRegisteredItems");
#endif

	gi.Printf ("-----------------------------------\n");

	//randomize the rand functions
	byte num_calls = (byte)gi.Milliseconds();

	for(i = 0; i < (int)num_calls; i++)
	{
		rand();
	}

	//Calculate all paths
	if ( navCalculatePaths )
	{
#ifdef _XBOX
		XBLog_Write("STEFX: InitGame before NAV_CalculatePaths");
#endif
		NAV_CalculatePaths( mapname, checkSum );
		
		navigator.CalculatePaths();

		if ( navigator.Save( mapname, checkSum ) == qfalse )
		{
			gi.Printf("Unable to save navigations data for map \"%s\" (checksum:%d)\n", mapname, checkSum );
		}
	}

	//Precache the auto-built dialogue .pre file
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before Q3_SetPrecacheFile behaved");
#endif
	if (strrchr(mapname,'/')) {
		Q3_SetPrecacheFile (va("%s/behaved",strrchr(mapname,'/')));
	} else {
		Q3_SetPrecacheFile (va("%s/behaved",mapname));
	}

	//Precache the designer-made extras .pre file
#ifdef _XBOX
	XBLog_Write("STEFX: InitGame before Q3_SetPrecacheFile extra");
#endif
	if (strrchr(mapname,'/')) {
		Q3_SetPrecacheFile (va("%s/extra",strrchr(mapname,'/')));
	} else {
		Q3_SetPrecacheFile (va("%s/extra",mapname));
	}
#ifdef _XBOX
	XBLF("STEFX: InitGame complete num_entities=%d", globals.num_entities);
#endif
}


/*
=================
ShutdownGame
=================
*/
void ShutdownGame( void ) {
	gi.Printf ("==== ShutdownGame ====\n");

	gi.Printf ("... ICARUS_Shutdown\n");
	ICARUS_Shutdown ();	//Shut ICARUS down

	gi.Printf ("... Reference Tags Cleared\n");
	TAG_Init();	//Clear the reference tags

	gi.Printf ("... Navigation Data Cleared\n");
	NAV_Shutdown();

	// write all the client session data so we can get it back
	G_WriteSessionData();
}



//===================================================================

static void G_Cvar_Create( const char *var_name, const char *var_value, int flags ) {
	gi.cvar( var_name, var_value, flags );
}

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
game_export_t *GetGameAPI( game_import_t *import ) {
	gameinfo_import_t	gameinfo_import;

#ifdef _XBOX
	XBLF("STEFX: EF GetGameAPI enter import=%p", import);
#endif
	gi = *import;
#ifdef _XBOX
	XBLog_Write("STEFX: EF GetGameAPI import copied");
#endif

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;

	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;
	globals.GameAllowedToSaveHere = GameAllowedToSaveHere;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ConsoleCommand = ConsoleCommand;

	globals.gentitySize = sizeof(gentity_t);

	gameinfo_import.FS_FOpenFile = gi.FS_FOpenFile;
	gameinfo_import.FS_Read = gi.FS_Read;
	gameinfo_import.FS_FCloseFile = gi.FS_FCloseFile;
	gameinfo_import.FS_ReadFile = gi.FS_ReadFile;
	gameinfo_import.FS_FreeFile = gi.FS_FreeFile;
	gameinfo_import.Cvar_Set = gi.cvar_set;
	gameinfo_import.Cvar_VariableStringBuffer = gi.Cvar_VariableStringBuffer;
	gameinfo_import.Cvar_Create = G_Cvar_Create;
	gameinfo_import.Printf = gi.Printf;
#ifdef _XBOX
	XBLog_Write("STEFX: EF GetGameAPI before GI_Init");
#endif
	GI_Init( &gameinfo_import );
#ifdef _XBOX
	XBLog_Write("STEFX: EF GetGameAPI after GI_Init");
	XBLF("STEFX: EF GetGameAPI return globals=%p api=%d gentitySize=%d", &globals, globals.apiversion, globals.gentitySize);
#endif

	return &globals;
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	gi.Error( ERR_DROP, "%s", text);
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link

/*
-------------------------
Com_Error
-------------------------
*/

void Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	gi.Error( level, "%s", text);
}

/*
-------------------------
Com_Printf
-------------------------
*/

void Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.Printf ("%s", text);
}

#endif

/*
========================================================================

MAP CHANGING

========================================================================
*/


/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

void G_CheckTasksCompleted (gentity_t *ent) 
{
	if ( Q3_TaskIDPending( ent, TID_CHAN_VOICE ) )
	{
		if ( !gi.S_Override[ent->s.number] )
		{//not playing a voice sound
			//return task_complete
#ifdef _XBOX
			if ( ent && ( ent->s.number == 0 || Q3_TaskIDPending( ent, TID_CHAN_VOICE ) ) )
			{
				static int s_stefxVoiceCompleteLogs = 0;
				if (s_stefxVoiceCompleteLogs < 64)
				{
					XBLF("STEFX: G_CheckTasksCompleted voice complete ent=%d class='%s' task=%d time=%d sOverride=%d",
						ent->s.number,
						ent->classname ? ent->classname : "<null>",
						ent->taskID[TID_CHAN_VOICE],
						level.time,
						gi.S_Override ? gi.S_Override[ent->s.number] : -999);
					s_stefxVoiceCompleteLogs++;
				}
			}
#endif
			Q3_TaskIDComplete( ent, TID_CHAN_VOICE );
		}
#ifdef _XBOX
		else
		{
			static int s_stefxVoicePendingLogs = 0;
			if (s_stefxVoicePendingLogs < 64)
			{
				XBLF("STEFX: G_CheckTasksCompleted voice pending ent=%d class='%s' task=%d time=%d sOverride=%d",
					ent->s.number,
					ent->classname ? ent->classname : "<null>",
					ent->taskID[TID_CHAN_VOICE],
					level.time,
					gi.S_Override ? gi.S_Override[ent->s.number] : -999);
				s_stefxVoicePendingLogs++;
			}
		}
#endif
	}

	if ( Q3_TaskIDPending( ent, TID_LOCATION ) )
	{
		char	*currentLoc = G_GetLocationForEnt( ent );

		if ( currentLoc && currentLoc[0] && Q_stricmp( ent->message, currentLoc ) == 0 )
		{//we're in the desired location
			Q3_TaskIDComplete( ent, TID_LOCATION );
		}
		//FIXME: else see if were in other trigger_locations?
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent) 
{
	float	thinktime;
#ifdef _XBOX
	int		entNum = (int)(ent - g_entities);
	qboolean xbLogThink = (qboolean)(level.framenum == 1 && entNum < 8);
#endif

	/*
	if ( ent->NPC == NULL )
	{
		if ( ent->taskManager && !stop_icarus )
		{
			ent->taskManager->Update( );
		}
	}
	*/

	thinktime = ent->nextthink;
#ifdef _XBOX
	if (xbLogThink)
	{
		XBLF("STEFX: G_RunThink enter ent=%d class='%s' thinktime=%d level=%d thinkFunc=%d npc=%p client=%p task=%p",
			entNum,
			ent->classname ? ent->classname : "(null)",
			ent->nextthink,
			level.time,
			ent->e_ThinkFunc,
			ent->NPC,
			ent->client,
			ent->taskManager);
	}
#endif
	if ( thinktime <= 0 ) 
	{
#ifdef _XBOX
		if (xbLogThink)
		{
			XBLF("STEFX: G_RunThink ent=%d skip thinktime<=0", entNum);
		}
#endif
		goto runicarus;
	}
	
	if ( thinktime > level.time ) 
	{
#ifdef _XBOX
		if (xbLogThink)
		{
			XBLF("STEFX: G_RunThink ent=%d skip thinktime future", entNum);
		}
#endif
		goto runicarus;
	}
	
	ent->nextthink = 0;
	if ( ent->e_ThinkFunc == thinkF_NULL )	// actually you don't need this if I check for it in the next function -slc
	{
		//gi.Error ( "NULL ent->think");
#ifdef _XBOX
		if (xbLogThink)
		{
			XBLF("STEFX: G_RunThink ent=%d null think", entNum);
		}
#endif
		goto runicarus;
	}

#ifdef _XBOX
	if (xbLogThink)
	{
		XBLF("STEFX: G_RunThink ent=%d before GEntity_ThinkFunc func=%d", entNum, ent->e_ThinkFunc);
	}
#endif
	GEntity_ThinkFunc( ent );	// ent->think (ent);
#ifdef _XBOX
	if (xbLogThink)
	{
		XBLF("STEFX: G_RunThink ent=%d after GEntity_ThinkFunc inuse=%d nextthink=%d", entNum, ent->inuse, ent->nextthink);
	}
#endif

runicarus:
	if ( ent->inuse )	// GEntity_ThinkFunc( ent ) can have freed up this ent if it was a type flier_child (stasis1 crash)
	{
		if ( ent->NPC == NULL )
		{
			if ( ent->taskManager && !stop_icarus )
			{
#ifdef _XBOX
				if (xbLogThink)
				{
					XBLF("STEFX: G_RunThink ent=%d before taskManager Update", entNum);
				}
#endif
				ent->taskManager->Update( );
#ifdef _XBOX
				if (xbLogThink)
				{
					XBLF("STEFX: G_RunThink ent=%d after taskManager Update", entNum);
				}
#endif
			}
		}
	}
#ifdef _XBOX
	if (xbLogThink)
	{
		XBLF("STEFX: G_RunThink exit ent=%d", entNum);
	}
#endif
}

/*
-------------------------
G_Animate
-------------------------
*/

void G_Animate ( gentity_t *self )
{
	if ( self->s.frame == self->endFrame )
	{
		if ( self->svFlags & SVF_ANIMATING )
		{
			if ( self->loopAnim )
			{
				self->s.frame = self->startFrame;
			}
			else
			{
				self->svFlags &= ~SVF_ANIMATING;
			}

			//Finished sequence - FIXME: only do this once even on looping anims?
			Q3_TaskIDComplete( self, TID_ANIM_BOTH );
		}
		return;
	}

	self->svFlags |= SVF_ANIMATING;

	if ( self->startFrame < self->endFrame )
	{
		if ( self->s.frame < self->startFrame || self->s.frame > self->endFrame )
		{
			self->s.frame = self->startFrame;
		}
		else
		{
			self->s.frame++;
		}
	}
	else if ( self->startFrame > self->endFrame )
	{
		if ( self->s.frame > self->startFrame || self->s.frame < self->endFrame )
		{
			self->s.frame = self->startFrame;
		}
		else
		{
			self->s.frame--;
		}
	}
	else
	{
		self->s.frame = self->endFrame;
	}
}

/*
-------------------------
G_AnimateBoltOn
-------------------------
*/

void G_AnimateBoltOn ( boltOnInfo_t *boltOn )
{
	if ( boltOn->frame == boltOn->endFrame )
	{
		if ( boltOn->loopAnim )
		{
			boltOn->frame = boltOn->startFrame;
		}
		return;
	}

	if ( boltOn->startFrame < boltOn->endFrame )
	{
		if ( boltOn->frame < boltOn->startFrame || boltOn->frame > boltOn->endFrame )
		{
			boltOn->frame = boltOn->startFrame;
		}
		else
		{
			boltOn->frame++;
		}
	}
	else if ( boltOn->startFrame > boltOn->endFrame )
	{
		if ( boltOn->frame > boltOn->startFrame || boltOn->frame < boltOn->endFrame )
		{
			boltOn->frame = boltOn->startFrame;
		}
		else
		{
			boltOn->frame--;
		}
	}
	else
	{
		boltOn->frame = boltOn->endFrame;
	}

/*
-------------------------
G_AnimateBoltOns
-------------------------
*/
}
void G_AnimateBoltOns (gentity_t *self)
{
	//Go through all my boltOns and animate them if needbe
	if ( !self->client )
	{
		if ( self->boltOn.index >= 0 && self->boltOn.index < numBoltOns )
		{
			G_AnimateBoltOn( &self->boltOn );
		}
	}
	else
	{
		for ( int i = 0; i < MAX_BOLT_ONS; i++ )
		{
			if ( self->client->renderInfo.boltOns[i].index >= 0 && self->client->renderInfo.boltOns[i].index < numBoltOns )
			{
				G_AnimateBoltOn( &self->client->renderInfo.boltOns[i] );
			}
		}
	}
}

/*
-------------------------
ResetTeamCounters
-------------------------
*/

void ResetTeamCounters( void )
{
	//clear team enemy counters
	for ( int team = TEAM_FREE; team < TEAM_NUM_TEAMS; team++ )
	{
		teamEnemyCount[team] = 0;
		teamCount[team] = 0;
	}
}

/*
-------------------------
UpdateTeamCounters
-------------------------
*/

void UpdateTeamCounters( gentity_t *ent )
{
	if ( !ent->NPC )
	{
		return;
	}
	if ( !ent->client )
	{
		return;
	}
	if ( ent->health <= 0 )
	{
		return;
	}
	if ( (ent->s.eFlags&EF_NODRAW) )
	{
		return;
	}
	if ( ent->client->playerTeam == TEAM_FREE )
	{
		return;
	}
	//this is an NPC who is alive and visible and is on a specific team

	teamCount[ent->client->playerTeam]++;
	if ( !ent->enemy )
	{
		return;
	}

	//ent has an enemy
	if ( !ent->enemy->client )
	{//enemy is a normal ent
		if ( ent->noDamageTeam == ent->client->playerTeam )
		{//it's on my team, don't count it as an enemy
			return;
		}
	}
	else
	{//enemy is another NPC/player
		if ( ent->enemy->client->playerTeam == ent->client->playerTeam)
		{//enemy is on the same team, don't count it as an enemy
			return;
		}
	}

	//ent's enemy is not on the same team
	teamLastEnemyTime[ent->client->playerTeam] = level.time;
	teamEnemyCount[ent->client->playerTeam]++;
}

extern void NPC_SetAnim(gentity_t	*ent,int type,int anim,int priority);
extern void G_MakeTeamVulnerable( void );
void G_CheckEndLevelTimers( gentity_t *ent )
{
	if ( killPlayerTimer && level.time > killPlayerTimer )
	{
		killPlayerTimer = 0;
		ent->health = 0;
		if ( ent->client && ent->client->ps.stats[STAT_HEALTH] > 0 )
		{
			//simulate death
			ent->client->ps.stats[STAT_HEALTH] = 0;
			//debounce respawn time
			ent->client->respawnTime = level.time + 2000;
			//play the "what have I done?!" anim
			NPC_SetAnim( ent, SETANIM_BOTH, BOTH_GUILT1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
			/*
			NPC_SetAnim( ent, SETANIM_TORSO, BOTH_SIT2TO1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
			NPC_SetAnim( ent, SETANIM_LEGS, LEGS_KNEELDOWN1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
			*/
			ent->client->ps.torsoAnimTimer = -1;
			ent->client->ps.legsAnimTimer = -1;
			//look at yourself
			ent->client->ps.stats[STAT_DEAD_YAW] = ent->client->ps.viewangles[YAW]+180;
			//stop all scripts
			if (Q_stricmpn(level.mapname,"_holo",5)) {
				stop_icarus = qtrue;
			}
			//make the team killable
			G_MakeTeamVulnerable();
		}
	}
	if ( loadBrigTimer && level.time > loadBrigTimer )
	{
		gentity_t *brigent = NULL;

		loadBrigTimer = 0;

		while( (brigent = G_Find(brigent, FOFS(classname), "target_level_change" )) != NULL )
		{
			if ( Q_stricmp("_brig", brigent->message) == 0 )
			{
				break;
			}
		}

		if ( brigent )
		{
			GEntity_UseFunc( brigent, ent, ent );
		}
		else
		{//somehow it was lost, do a manual load
			gi.SendConsoleCommand( "wait;maptransition _brig\n" );
		}

	}
}
/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
	int			msec;
#ifdef _XBOX
	qboolean	xbLogFrame;
	int			xboxProfileStart;
	int			xboxProfileEntitiesStart;
	int			xboxProfilePostStart;
	unsigned int xboxEntitiesVisited = 0;
	unsigned int xboxMissiles = 0;
	unsigned int xboxItems = 0;
	unsigned int xboxMovers = 0;
	unsigned int xboxClients = 0;
	unsigned int xboxThinkDue = 0;
	unsigned int xboxScripted = 0;
	unsigned int xboxOther = 0;
#endif

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;
	msec = level.time - level.previousTime;
#ifdef _XBOX
	xboxProfileStart = gi.Milliseconds();
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_GAME_TRACE_STAGE(0x4752464D, 1); /* GRFM */
	STEFX_GAME_TRACE_DETAIL((unsigned int)level.framenum, (unsigned int)level.time, (unsigned int)msec, (unsigned int)globals.num_entities);
#endif
#ifdef _XBOX
	xbLogFrame = (qboolean)(level.framenum == 1);
	if (xbLogFrame)
	{
		XBLF("STEFX: G_RunFrame enter frame=%d time=%d previous=%d msec=%d entities=%d",
			level.framenum, level.time, level.previousTime, msec, globals.num_entities);
	}
#endif
	
	ResetTeamCounters();
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame after ResetTeamCounters");
	}
#endif
	
	//remember last waypoint, clear current one
	for ( i = 0, ent = &g_entities[0]; i < globals.num_entities ; i++, ent++) 
	{
		if ( ent->waypoint != WAYPOINT_NONE )
		{
			ent->lastWaypoint = ent->waypoint;
			ent->waypoint = WAYPOINT_NONE;
		}
	}
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame after waypoint reset");
	}
#endif

	//Look to clear out old events
	if ( eventClearTime < level.time )
	{
		ClearPlayerAlertEvents();
		eventClearTime = level.time + ALERT_CLEAR_TIME;
	}
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame after alert maintenance");
	}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SplitCoopRunFrame();
#endif
#ifdef _XBOX
	xboxProfileEntitiesStart = gi.Milliseconds();
	g_SPXBPerfGamePreMsec = (unsigned int)(xboxProfileEntitiesStart - xboxProfileStart);
#endif

	//Run the frame for all entities
	for ( i = 0, ent = &g_entities[0]; i < globals.num_entities ; i++, ent++)
	{
		if ( !ent->inuse )
			continue;
#ifdef _XBOX
		xboxEntitiesVisited++;
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 20); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->s.eType, (unsigned int)ent->svFlags, (unsigned int)ent->nextthink);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d begin class=%p targetname=%p eType=%d client=%p svFlags=0x%08x nextthink=%d eventTime=%d free=%d unlink=%d",
				i,
				ent->classname,
				ent->targetname,
				ent->s.eType,
				ent->client,
				ent->svFlags,
				ent->nextthink,
				ent->eventTime,
				ent->freeAfterEvent,
				ent->unlinkAfterEvent);
		}
#endif

		// clear events that are too old
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 21); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->eventTime, (unsigned int)ent->freeAfterEvent, (unsigned int)ent->unlinkAfterEvent);
#endif
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				G_FreeEntity( ent );
				continue;
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				gi.unlinkentity( ent );
			}
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 22); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->freeAfterEvent, (unsigned int)ent->unlinkAfterEvent);
#endif

		// temporary entities don't think
		if ( ent->freeAfterEvent )
			continue;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 23); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->taskManager, (unsigned int)ent->e_ThinkFunc, (unsigned int)ent->nextthink);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d before G_CheckTasksCompleted", i);
		}
#endif
		if ( ent->sequencer && ent->taskManager )
		{
			G_CheckTasksCompleted(ent);
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 24); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->e_ThinkFunc, (unsigned int)ent->nextthink);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d after G_CheckTasksCompleted", i);
		}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 25); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->s.pos.trType, (unsigned int)ent->s.apos.trType);
#endif
		if ( ent->next_roff_time && ent->next_roff_time <= level.time )
		{
			G_Roff( ent );
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 26); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->s.pos.trType, (unsigned int)ent->s.apos.trType);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d after G_Roff", i);
		}
#endif

		if( !ent->client )
		{
			if ( !(ent->svFlags & SVF_SELF_ANIMATING) )
			{//FIXME: make sure this is done only for models with frames?
				//Or just flag as animating?
				if ( ent->s.eFlags & EF_ANIM_ONCE )
				{
					ent->s.frame++;
				}
				else if ( !(ent->s.eFlags & EF_ANIM_ALLFAST) &&
					(ent->s.frame != ent->endFrame || (ent->svFlags & SVF_ANIMATING)) )
				{
					G_Animate( ent );
				}
			}
		}
		else
		{
			G_AnimateBoltOns( ent );
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 28); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->s.frame, (unsigned int)ent->client);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d after animation", i);
		}
#endif

		if ( ent->s.eType == ET_MISSILE ) 
		{
#ifdef _XBOX
			xboxMissiles++;
#endif
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLF("STEFX: G_RunFrame ent=%d before G_RunMissile", i);
			}
#endif
			G_RunMissile( ent );
			continue;
		}

		if ( ent->s.eType == ET_ITEM ) 
		{
#ifdef _XBOX
			xboxItems++;
#endif
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLF("STEFX: G_RunFrame ent=%d before G_RunItem", i);
			}
#endif
			G_RunItem( ent );
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) 
		{
#ifdef _XBOX
			xboxMovers++;
#endif
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLF("STEFX: G_RunFrame ent=%d before G_RunMover", i);
			}
#endif
			G_RunMover( ent );
			continue;
		}

		//The player
		if ( i == 0 ) 
		{
#ifdef _XBOX
			xboxClients++;
#endif
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLog_Write("STEFX: G_RunFrame player before G_CheckEndLevelTimers");
			}
#endif
			G_CheckEndLevelTimers( ent );
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLog_Write("STEFX: G_RunFrame player before NAV_FindPlayerWaypoint");
			}
#endif
			//Recalculate the nearest waypoint for the coming NPC updates
			NAV_FindPlayerWaypoint();
#ifdef _XBOX
			if (xbLogFrame)
			{
				XBLog_Write("STEFX: G_RunFrame player after NAV_FindPlayerWaypoint");
			}
#endif

			if( ent->taskManager && ent->taskManager->HasTasks() && !stop_icarus )
			{
#ifdef _XBOX
				if (xbLogFrame)
				{
					XBLog_Write("STEFX: G_RunFrame player before taskManager Update");
				}
#endif
				ent->taskManager->Update();
#ifdef _XBOX
				if (xbLogFrame)
				{
					XBLog_Write("STEFX: G_RunFrame player after taskManager Update");
				}
#endif
			}

			continue;	// players are ucmd driven
		}

#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d before G_RunThink", i);
		}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 31); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->e_ThinkFunc, (unsigned int)ent->nextthink);
#endif
		const qboolean thinkDue = (qboolean)(ent->nextthink > 0 && ent->nextthink <= level.time);
		const qboolean scripted = (qboolean)(ent->NPC == NULL && ent->taskManager &&
			ent->taskManager->HasTasks() && !stop_icarus);
#ifdef _XBOX
		if (ent->client)
		{
			xboxClients++;
		}
		if (thinkDue)
		{
			xboxThinkDue++;
		}
		if (scripted)
		{
			xboxScripted++;
		}
		if (!ent->client && !thinkDue && !scripted)
		{
			xboxOther++;
		}
#endif
		if ( thinkDue || scripted )
		{
			G_RunThink( ent );	// be aware that ent may be free after returning from here, at least one func frees them
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 32); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->e_ThinkFunc, (unsigned int)ent->nextthink);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d after G_RunThink inuse=%d", i, ent->inuse);
		}
#endif
		ClearNPCGlobals();			//	but these 2 funcs are ok
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 33); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)ent->client, (unsigned int)ent->NPC);
#endif
		if ( ent->NPC && ent->client )
		{
			UpdateTeamCounters( ent );	//	   to call anyway on a freed ent.
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_GAME_TRACE_STAGE(0x4752464D, 34); /* GRFM */
		STEFX_GAME_TRACE_DETAIL((unsigned int)i, (unsigned int)ent->inuse, (unsigned int)teamEnemyCount[0], (unsigned int)teamEnemyCount[1]);
#endif
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLF("STEFX: G_RunFrame ent=%d after UpdateTeamCounters", i);
		}
#endif
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SplitCoopMaintainEVHelmets( "post-entities" );
#endif
#ifdef _XBOX
	xboxProfilePostStart = gi.Milliseconds();
	g_SPXBPerfGameEntitiesMsec = (unsigned int)(xboxProfilePostStart - xboxProfileEntitiesStart);
	g_SPXBPerfGameEntitiesVisited = xboxEntitiesVisited;
#endif

	// perform final fixups on the player
	ent = &g_entities[0];
	if ( ent->inuse ) {
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLog_Write("STEFX: G_RunFrame before ClientEndFrame");
		}
#endif
		ClientEndFrame( ent );
#ifdef _XBOX
		if (xbLogFrame)
		{
			XBLog_Write("STEFX: G_RunFrame after ClientEndFrame");
		}
#endif
	}

	//DEBUG STUFF
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame before debug info");
	}
#endif
	NAV_ShowDebugInfo();
	NPC_ShowDebugInfo();
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame after debug info");
	}
#endif

	//handle the ffire counter
	if ( ffireLevel < FFIRE_LEVEL_RETALIATION )
	{//if we haven't reached the retaliation point, clear the counter
		if ( level.time - teamLastEnemyTime[TEAM_STARFLEET] < 10000 ||//teammates have had an enemy in the last 10 seconds
			level.time - ffireForgivenessTimer > 120000 )//Haven't shot your teammates in the past 2 minutes
		{
			//reset friendly fire counter
			ffireLevel = 0;
		}
	}
#ifdef _XBOX
	if (xbLogFrame)
	{
		XBLog_Write("STEFX: G_RunFrame complete");
	}
	g_SPXBPerfGamePostMsec = (unsigned int)(gi.Milliseconds() - xboxProfilePostStart);
	g_SPXBPerfGameMissiles = xboxMissiles;
	g_SPXBPerfGameItems = xboxItems;
	g_SPXBPerfGameMovers = xboxMovers;
	g_SPXBPerfGameClients = xboxClients;
	g_SPXBPerfGameThinkDue = xboxThinkDue;
	g_SPXBPerfGameScripted = xboxScripted;
	g_SPXBPerfGameOther = xboxOther;
#endif
}



extern qboolean player_locked;

void G_LoadSave_WriteMiscData(void)
{ 
	gi.AppendToSaveGame('LCKD', &player_locked, sizeof(player_locked));
}



void G_LoadSave_ReadMiscData(void)
{
	gi.ReadFromSaveGame('LCKD', &player_locked, sizeof(player_locked), NULL);
}
