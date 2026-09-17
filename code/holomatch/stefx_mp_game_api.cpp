#include "../game/q_shared.h"
#include "../game/g_public.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/stefx_snapshot_abi.h"
#include "../server/server.h"
#include "../win32/xb_log.h"
#include "stefx_holomatch_bot_bridge.h"

#include <stdarg.h>

extern "C" volatile unsigned int g_SPXBHMGetUsercmdCount;
extern "C" volatile unsigned int g_SPXBHMGetUsercmdTime;
extern "C" volatile unsigned int g_SPXBHMGetUsercmdMove;
extern "C" volatile unsigned int g_SPXBHMGetUsercmdButtons;
extern "C" volatile unsigned int g_SPXBHMClientThinkCount;

extern "C" void QDECL STEFX_HM_Com_Printf( const char *format, ... )
{
	char text[4096];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ) - 1, format, args );
	va_end( args );
	text[sizeof( text ) - 1] = 0;
	::Com_Printf( "%s", text );
}

extern "C" void QDECL STEFX_HM_Com_Error( int level, const char *format, ... )
{
	char text[4096];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ) - 1, format, args );
	va_end( args );
	text[sizeof( text ) - 1] = 0;
	XBLog_WriteCriticalf("STEFX_HM_SP: official Com_Error level=%d message='%s'",
		level, text);
	::Com_Error( level, "%s", text );
}

extern "C" int vmMain( int command, int arg0, int arg1, int arg2,
	int arg3, int arg4, int arg5, int arg6 );
extern "C" void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );

extern void SV_GetUsercmd( int clientNum, usercmd_t *cmd );
extern qboolean SV_GetEntityToken( char *buffer, int bufferSize );

static game_import_t *s_stefxHolomatchImport = NULL;
static game_import_t s_stefxHolomatchImportStorage;
static void *s_stefxHolomatchGentities = NULL;
static int s_stefxHolomatchGentitySize = 0;
static int s_stefxHolomatchNumEntities = 0;
static int s_stefxHolomatchInvalidTraceTraceCount = 0;
static int s_stefxHolomatchBadTraceInputCount = 0;
static int s_stefxHolomatchBadTraceResultCount = 0;
static const char *s_stefxHolomatchEntityParsePoint = NULL;
static game_export_t s_stefxHolomatchExport;

/*
 * The official EF VM and the SP server do not share the same public entity
 * ABI.  EF puts entityShared_t immediately after entityState_t and uses the
 * EF playerState layout; the SP server uses the later JA layout.  Keep the
 * VM's memory private and expose a synchronized SP-shaped view to the engine.
 */
typedef struct stefx_hm_trajectory_s {
	int trType;
	int trTime;
	int trDuration;
	vec3_t trBase;
	vec3_t trDelta;
} stefx_hm_trajectory_t;

typedef struct stefx_hm_entity_state_s {
	int number;
	int eType;
	int eFlags;
	stefx_hm_trajectory_t pos;
	stefx_hm_trajectory_t apos;
	int time;
	int time2;
	vec3_t origin;
	vec3_t origin2;
	vec3_t angles;
	vec3_t angles2;
	int otherEntityNum;
	int otherEntityNum2;
	int groundEntityNum;
	int constantLight;
	int loopSound;
	int modelindex;
	int modelindex2;
	int clientNum;
	int frame;
	int solid;
	int event;
	int eventParm;
	int powerups;
	int weapon;
	int legsAnim;
	int torsoAnim;
} stefx_hm_entity_state_t;

typedef struct stefx_hm_entity_shared_s {
	qboolean linked;
	int linkcount;
	int svFlags;
	int singleClient;
	qboolean bmodel;
	vec3_t mins;
	vec3_t maxs;
	int contents;
	vec3_t absmin;
	vec3_t absmax;
	vec3_t currentOrigin;
	vec3_t currentAngles;
	int ownerNum;
} stefx_hm_entity_shared_t;

typedef struct stefx_hm_entity_prefix_s {
	stefx_hm_entity_state_t s;
	stefx_hm_entity_shared_t r;
	void *client;
	qboolean inuse;
} stefx_hm_entity_prefix_t;

typedef struct stefx_hm_player_state_s {
	int commandTime;
	int pm_type;
	int bobCycle;
	int pm_flags;
	int pm_time;
	vec3_t origin;
	vec3_t velocity;
	int weaponTime;
	int rechargeTime;
	short useTime;
	int introTime;
	int gravity;
	int speed;
	int delta_angles[3];
	int groundEntityNum;
	int legsTimer;
	int legsAnim;
	int torsoTimer;
	int torsoAnim;
	int movementDir;
	int eFlags;
	int eventSequence;
	int events[4];
	int eventParms[4];
	int externalEvent;
	int externalEventParm;
	int externalEventTime;
	int clientNum;
	int weapon;
	int weaponstate;
	vec3_t viewangles;
	int viewheight;
	int damageEvent;
	int damageYaw;
	int damagePitch;
	int damageCount;
	int damageShieldCount;
	int stats[16];
	int persistant[16];
	int powerups[16];
	int ammo[16];
	int ping;
	int entityEventSequence;
} stefx_hm_player_state_t;

#define STEFX_HM_MAX_PS_EVENTS 4

typedef struct stefx_hm_usercmd_s {
	int serverTime;
	byte buttons;
	byte weapon;
	int angles[3];
	signed char forwardmove;
	signed char rightmove;
	signed char upmove;
} stefx_hm_usercmd_t;

#define STEFX_HM_MAX_CLIENT_STATES 128
static byte s_stefxHolomatchMirrorStorage[MAX_GENTITIES * sizeof(gentity_t)];
static playerState_t s_stefxHolomatchMirrorClientStates[STEFX_HM_MAX_CLIENT_STATES];

static gentity_t *STEFX_HolomatchMirrorEntity(int entityNum)
{
	if (entityNum < 0 || entityNum >= MAX_GENTITIES)
	{
		return NULL;
	}
	return (gentity_t *)(s_stefxHolomatchMirrorStorage + sizeof(gentity_t) * entityNum);
}

static int STEFX_HolomatchEntityIndex(const void *entity)
{
	unsigned int base;
	unsigned int address;
	unsigned int stride;
	unsigned int delta;

	if (!entity || !s_stefxHolomatchGentities || s_stefxHolomatchGentitySize <= 0)
	{
		return -1;
	}
	base = (unsigned int)s_stefxHolomatchGentities;
	address = (unsigned int)entity;
	stride = (unsigned int)s_stefxHolomatchGentitySize;
	if (address < base)
	{
		return -1;
	}
	delta = address - base;
	if ((delta % stride) != 0 || (delta / stride) >= (unsigned int)MAX_GENTITIES)
	{
		return -1;
	}
	return (int)(delta / stride);
}

static void *STEFX_HolomatchOfficialEntity(int entityNum)
{
	if (!s_stefxHolomatchGentities || s_stefxHolomatchGentitySize <= 0 ||
		entityNum < 0 || entityNum >= MAX_GENTITIES)
	{
		return NULL;
	}
	return (void *)((byte *)s_stefxHolomatchGentities +
		s_stefxHolomatchGentitySize * entityNum);
}

static void STEFX_HolomatchCopyOfficialPlayerStateToSP(playerState_t *dst,
	const stefx_hm_player_state_t *src)
{
	int i;
	memset(dst, 0, sizeof(*dst));
	dst->commandTime = src->commandTime;
	dst->pm_type = src->pm_type;
	dst->bobCycle = src->bobCycle;
	dst->pm_flags = src->pm_flags;
	dst->pm_time = src->pm_time;
	VectorCopy(src->origin, dst->origin);
	VectorCopy(src->velocity, dst->velocity);
	dst->weaponTime = src->weaponTime;
	dst->rechargeTime = src->rechargeTime;
	dst->useTime = src->useTime;
	dst->gravity = src->gravity;
	dst->speed = src->speed;
	for (i = 0; i < 3; ++i)
	{
		dst->delta_angles[i] = src->delta_angles[i];
	}
	dst->groundEntityNum = src->groundEntityNum;
	dst->legsAnimTimer = src->legsTimer;
	dst->legsAnim = src->legsAnim;
	dst->torsoAnimTimer = src->torsoTimer;
	dst->torsoAnim = src->torsoAnim;
	dst->movementDir = src->movementDir;
	dst->eFlags = src->eFlags;
	dst->eventSequence = src->eventSequence;
	for (i = 0; i < MAX_PS_EVENTS && i < STEFX_HM_MAX_PS_EVENTS; ++i)
	{
		dst->events[i] = src->events[i];
		dst->eventParms[i] = src->eventParms[i];
	}
	dst->externalEvent = src->externalEvent;
	dst->externalEventParm = src->externalEventParm;
	dst->externalEventTime = src->externalEventTime;
	dst->clientNum = src->clientNum;
	dst->weapon = src->weapon;
	dst->weaponstate = src->weaponstate;
	VectorCopy(src->viewangles, dst->viewangles);
	dst->viewheight = src->viewheight;
	dst->damageEvent = src->damageEvent;
	dst->damageYaw = src->damageYaw;
	dst->damagePitch = src->damagePitch;
	dst->damageCount = src->damageCount;
	for (i = 0; i < 16; ++i)
	{
		dst->stats[i] = src->stats[i];
		dst->persistant[i] = src->persistant[i];
		dst->powerups[i] = src->powerups[i];
	}
	for (i = 0; i < MAX_AMMO && i < 16; ++i)
	{
		dst->ammo[i] = src->ammo[i];
	}
	dst->ping = src->ping;
}

static void STEFX_HolomatchCopyOfficialStateToSP(entityState_t *dst,
	const stefx_hm_entity_state_t *src)
{
	memset(dst, 0, sizeof(*dst));
	dst->number = src->number;
	dst->eType = src->eType;
	dst->eFlags = src->eFlags;
	dst->pos.trType = (trType_t)STEFX_OfficialTrajectoryTypeToSP(src->pos.trType);
	dst->pos.trTime = src->pos.trTime;
	dst->pos.trDuration = src->pos.trDuration;
	VectorCopy(src->pos.trBase, dst->pos.trBase);
	VectorCopy(src->pos.trDelta, dst->pos.trDelta);
	dst->apos.trType = (trType_t)STEFX_OfficialTrajectoryTypeToSP(src->apos.trType);
	dst->apos.trTime = src->apos.trTime;
	dst->apos.trDuration = src->apos.trDuration;
	VectorCopy(src->apos.trBase, dst->apos.trBase);
	VectorCopy(src->apos.trDelta, dst->apos.trDelta);
	dst->time = src->time;
	dst->time2 = src->time2;
	VectorCopy(src->origin, dst->origin);
	VectorCopy(src->origin2, dst->origin2);
	VectorCopy(src->angles, dst->angles);
	VectorCopy(src->angles2, dst->angles2);
	dst->otherEntityNum = src->otherEntityNum;
	dst->otherEntityNum2 = src->otherEntityNum2;
	dst->groundEntityNum = src->groundEntityNum;
	dst->constantLight = src->constantLight;
	dst->loopSound = src->loopSound;
	dst->modelindex = src->modelindex;
	dst->modelindex2 = src->modelindex2;
	dst->clientNum = src->clientNum;
	dst->frame = src->frame;
	dst->solid = src->solid;
	dst->event = src->event;
	dst->eventParm = src->eventParm;
	dst->powerups = src->powerups;
	dst->weapon = src->weapon;
	dst->legsAnim = src->legsAnim;
	dst->torsoAnim = src->torsoAnim;
}

static void STEFX_HolomatchCopySPStateToOfficial(stefx_hm_entity_state_t *dst,
	const entityState_t *src)
{
	dst->number = src->number;
	dst->eType = src->eType;
	dst->eFlags = src->eFlags;
	dst->pos.trType = STEFX_SPTrajectoryTypeToOfficial(src->pos.trType);
	dst->pos.trTime = src->pos.trTime;
	dst->pos.trDuration = src->pos.trDuration;
	VectorCopy(src->pos.trBase, dst->pos.trBase);
	VectorCopy(src->pos.trDelta, dst->pos.trDelta);
	dst->apos.trType = STEFX_SPTrajectoryTypeToOfficial(src->apos.trType);
	dst->apos.trTime = src->apos.trTime;
	dst->apos.trDuration = src->apos.trDuration;
	VectorCopy(src->apos.trBase, dst->apos.trBase);
	VectorCopy(src->apos.trDelta, dst->apos.trDelta);
	dst->time = src->time;
	dst->time2 = src->time2;
	VectorCopy(src->origin, dst->origin);
	VectorCopy(src->origin2, dst->origin2);
	VectorCopy(src->angles, dst->angles);
	VectorCopy(src->angles2, dst->angles2);
	dst->otherEntityNum = src->otherEntityNum;
	dst->otherEntityNum2 = src->otherEntityNum2;
	dst->groundEntityNum = src->groundEntityNum;
	dst->constantLight = src->constantLight;
	dst->loopSound = src->loopSound;
	dst->modelindex = src->modelindex;
	dst->modelindex2 = src->modelindex2;
	dst->clientNum = src->clientNum;
	dst->frame = src->frame;
	dst->solid = src->solid;
	dst->event = src->event;
	dst->eventParm = src->eventParm;
	dst->powerups = src->powerups;
	dst->weapon = src->weapon;
	dst->legsAnim = src->legsAnim;
	dst->torsoAnim = src->torsoAnim;
}

static void STEFX_HolomatchSyncOfficialEntityToMirror(int entityNum)
{
	stefx_hm_entity_prefix_t *official;
	gentity_t *mirror;
	stefx_hm_player_state_t *officialPS;

	if (entityNum < 0 || entityNum >= MAX_GENTITIES)
	{
		return;
	}
	official = (stefx_hm_entity_prefix_t *)STEFX_HolomatchOfficialEntity(entityNum);
	if (!official)
	{
		return;
	}
	mirror = STEFX_HolomatchMirrorEntity(entityNum);
	memset(mirror, 0, sizeof(*mirror));
	STEFX_HolomatchCopyOfficialStateToSP(&mirror->s, &official->s);
	mirror->inuse = official->inuse;
	mirror->linked = official->r.linked;
	mirror->svFlags = official->r.svFlags;
	mirror->singleClient = official->r.singleClient;
	mirror->bmodel = official->r.bmodel;
	VectorCopy(official->r.mins, mirror->mins);
	VectorCopy(official->r.maxs, mirror->maxs);
	mirror->contents = official->r.contents;
	VectorCopy(official->r.absmin, mirror->absmin);
	VectorCopy(official->r.absmax, mirror->absmax);
	VectorCopy(official->r.currentOrigin, mirror->currentOrigin);
	VectorCopy(official->r.currentAngles, mirror->currentAngles);
	mirror->owner = NULL;
	if (official->r.ownerNum >= 0 && official->r.ownerNum < MAX_GENTITIES &&
		official->r.ownerNum != ENTITYNUM_NONE)
	{
		mirror->owner = STEFX_HolomatchMirrorEntity(official->r.ownerNum);
	}
	if (official->client && entityNum < STEFX_HM_MAX_CLIENT_STATES)
	{
		officialPS = (stefx_hm_player_state_t *)official->client;
		STEFX_HolomatchCopyOfficialPlayerStateToSP(&s_stefxHolomatchMirrorClientStates[entityNum], officialPS);
		mirror->client = &s_stefxHolomatchMirrorClientStates[entityNum];
	}
}

static void STEFX_HolomatchSyncMirrorToOfficial(int entityNum)
{
	stefx_hm_entity_prefix_t *official;
	gentity_t *mirror;

	if (entityNum < 0 || entityNum >= MAX_GENTITIES)
	{
		return;
	}
	official = (stefx_hm_entity_prefix_t *)STEFX_HolomatchOfficialEntity(entityNum);
	if (!official)
	{
		return;
	}
	mirror = STEFX_HolomatchMirrorEntity(entityNum);
	STEFX_HolomatchCopySPStateToOfficial(&official->s, &mirror->s);
	official->inuse = mirror->inuse;
	official->r.linked = mirror->linked;
	official->r.svFlags = mirror->svFlags;
	official->r.singleClient = mirror->singleClient;
	official->r.bmodel = mirror->bmodel;
	VectorCopy(mirror->mins, official->r.mins);
	VectorCopy(mirror->maxs, official->r.maxs);
	official->r.contents = mirror->contents;
	VectorCopy(mirror->absmin, official->r.absmin);
	VectorCopy(mirror->absmax, official->r.absmax);
	VectorCopy(mirror->currentOrigin, official->r.currentOrigin);
	VectorCopy(mirror->currentAngles, official->r.currentAngles);
	official->r.ownerNum = mirror->owner ? mirror->owner->s.number : ENTITYNUM_NONE;
	/* Engine entity imports update linkage and bounds only.  The official
	 * Holomatch player state remains authoritative; the SP-shaped client is a
	 * read-only projection and omits private EF fields such as introTime and
	 * entityEventSequence. */
}

static void STEFX_HolomatchSyncAllToMirror(void)
{
	int i;
	int count = s_stefxHolomatchNumEntities;
	if (count < 0 || count > MAX_GENTITIES)
	{
		count = MAX_GENTITIES;
	}
	for (i = 0; i < count; ++i)
	{
		STEFX_HolomatchSyncOfficialEntityToMirror(i);
	}
}

static void STEFX_HolomatchRefreshExport(void)
{
	s_stefxHolomatchExport.gentities = s_stefxHolomatchGentities ? STEFX_HolomatchMirrorEntity(0) : NULL;
	s_stefxHolomatchExport.gentitySize = s_stefxHolomatchGentities ? sizeof(gentity_t) : 0;
	s_stefxHolomatchExport.num_entities = s_stefxHolomatchNumEntities;
}

extern "C" void STEFX_HolomatchMarkBot(int clientNum)
{
	stefx_hm_entity_prefix_t *official =
		(stefx_hm_entity_prefix_t *)STEFX_HolomatchOfficialEntity(clientNum);

	if (!official)
	{
		return;
	}
	official->r.svFlags |= SVF_BOT;
	official->inuse = qtrue;
	STEFX_HolomatchSyncOfficialEntityToMirror(clientNum);
	STEFX_HolomatchRefreshExport();
}

const void *STEFX_HolomatchGetOfficialPlayerState(int clientNum)
{
	stefx_hm_entity_prefix_t *official =
		(stefx_hm_entity_prefix_t *)STEFX_HolomatchOfficialEntity(clientNum);

	if (!official || !official->inuse || !official->client)
	{
		return NULL;
	}
	return official->client;
}

int STEFX_HolomatchPrepareSplitWeaponProof(int clientNum, int slot)
{
	enum
	{
		STEFX_HM_STAT_WEAPONS = 2,
		STEFX_HM_WP_PHASER = 1,
		STEFX_HM_WP_COMPRESSION_RIFLE = 2,
		STEFX_HM_WP_IMOD = 3,
		STEFX_HM_WP_SCAVENGER_RIFLE = 4
	};
	static const int proofWeapons[4] = {
		STEFX_HM_WP_PHASER,
		STEFX_HM_WP_COMPRESSION_RIFLE,
		STEFX_HM_WP_IMOD,
		STEFX_HM_WP_SCAVENGER_RIFLE
	};
	static const int proofAmmo[4] = { 50, 128, 60, 100 };
	static int logBudget = 24;
	stefx_hm_entity_prefix_t *official;
	stefx_hm_player_state_t *player;
	int weapon;
	int oldWeapons;
	int oldAmmo;

	if (slot < 0 || slot >= 4)
	{
		return 0;
	}
	official = (stefx_hm_entity_prefix_t *)STEFX_HolomatchOfficialEntity(clientNum);
	if (!official || !official->inuse || !official->client)
	{
		return 0;
	}

	player = (stefx_hm_player_state_t *)official->client;
	weapon = proofWeapons[slot];
	oldWeapons = player->stats[STEFX_HM_STAT_WEAPONS];
	oldAmmo = player->ammo[weapon];
	player->stats[STEFX_HM_STAT_WEAPONS] |= 1 << weapon;
	if (player->ammo[weapon] < proofAmmo[slot] / 2)
	{
		player->ammo[weapon] = proofAmmo[slot];
	}

	if (logBudget > 0 &&
		(oldWeapons != player->stats[STEFX_HM_STAT_WEAPONS] || oldAmmo != player->ammo[weapon]))
	{
		XBLog_WriteCriticalf("STEFX_HM_SPLIT_WEAPON_PROOF: client=%d slot=%d requested=%d weapons=0x%x->0x%x ammo=%d->%d",
			clientNum,
			slot,
			weapon,
			oldWeapons,
			player->stats[STEFX_HM_STAT_WEAPONS],
			oldAmmo,
			player->ammo[weapon]);
		--logBudget;
	}
	return weapon;
}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBHMSplitBotProof[32];
#endif

enum stefxHolomatchGameImportCommand_e
{
	STEFX_HM_G_PRINT = 0,
	STEFX_HM_G_ERROR,
	STEFX_HM_G_MILLISECONDS,
	STEFX_HM_G_CVAR_REGISTER,
	STEFX_HM_G_CVAR_UPDATE,
	STEFX_HM_G_CVAR_SET,
	STEFX_HM_G_CVAR_VARIABLE_INTEGER_VALUE,
	STEFX_HM_G_CVAR_VARIABLE_STRING_BUFFER,
	STEFX_HM_G_ARGC,
	STEFX_HM_G_ARGV,
	STEFX_HM_G_FS_FOPEN_FILE,
	STEFX_HM_G_FS_READ,
	STEFX_HM_G_FS_WRITE,
	STEFX_HM_G_FS_FCLOSE_FILE,
	STEFX_HM_G_SEND_CONSOLE_COMMAND,
	STEFX_HM_G_LOCATE_GAME_DATA,
	STEFX_HM_G_DROP_CLIENT,
	STEFX_HM_G_SEND_SERVER_COMMAND,
	STEFX_HM_G_SET_CONFIGSTRING,
	STEFX_HM_G_GET_CONFIGSTRING,
	STEFX_HM_G_GET_USERINFO,
	STEFX_HM_G_SET_USERINFO,
	STEFX_HM_G_GET_SERVERINFO,
	STEFX_HM_G_SET_BRUSH_MODEL,
	STEFX_HM_G_TRACE,
	STEFX_HM_G_POINT_CONTENTS,
	STEFX_HM_G_IN_PVS,
	STEFX_HM_G_IN_PVS_IGNORE_PORTALS,
	STEFX_HM_G_ADJUST_AREA_PORTAL_STATE,
	STEFX_HM_G_AREAS_CONNECTED,
	STEFX_HM_G_LINKENTITY,
	STEFX_HM_G_UNLINKENTITY,
	STEFX_HM_G_ENTITIES_IN_BOX,
	STEFX_HM_G_ENTITY_CONTACT,
	STEFX_HM_G_BOT_ALLOCATE_CLIENT,
	STEFX_HM_G_BOT_FREE_CLIENT,
	STEFX_HM_G_GET_USERCMD,
	STEFX_HM_G_GET_ENTITY_TOKEN,
	STEFX_HM_G_FS_GETFILELIST,
	STEFX_HM_G_DEBUG_POLYGON_CREATE,
	STEFX_HM_G_DEBUG_POLYGON_DELETE,

	STEFX_HM_BOTLIB_SETUP = 200,
	STEFX_HM_BOTLIB_SHUTDOWN,
	STEFX_HM_BOTLIB_LIBVAR_SET,
	STEFX_HM_BOTLIB_LIBVAR_GET,
	STEFX_HM_BOTLIB_DEFINE,
	STEFX_HM_BOTLIB_START_FRAME,
	STEFX_HM_BOTLIB_LOAD_MAP,
	STEFX_HM_BOTLIB_UPDATE_ENTITY,
	STEFX_HM_BOTLIB_TEST,
	STEFX_HM_BOTLIB_GET_SNAPSHOT_ENTITY,
	STEFX_HM_BOTLIB_GET_CONSOLE_MESSAGE,
	STEFX_HM_BOTLIB_USER_COMMAND,
	STEFX_HM_BOTLIB_AAS_ENTITY_VISIBLE = 300,
	STEFX_HM_BOTLIB_AAS_IN_FIELD_OF_VISION,
	STEFX_HM_BOTLIB_AAS_VISIBLE_CLIENTS,
	STEFX_HM_BOTLIB_AAS_ENTITY_INFO,
	STEFX_HM_BOTLIB_AAS_INITIALIZED,
	STEFX_HM_BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX,
	STEFX_HM_BOTLIB_AAS_TIME,
	STEFX_HM_BOTLIB_AAS_POINT_AREA_NUM,
	STEFX_HM_BOTLIB_AAS_TRACE_AREAS,
	STEFX_HM_BOTLIB_AAS_POINT_CONTENTS,
	STEFX_HM_BOTLIB_AAS_NEXT_BSP_ENTITY,
	STEFX_HM_BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY,
	STEFX_HM_BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY,
	STEFX_HM_BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY,
	STEFX_HM_BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY,
	STEFX_HM_BOTLIB_AAS_AREA_REACHABILITY,
	STEFX_HM_BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA,
	STEFX_HM_BOTLIB_AAS_SWIMMING,
	STEFX_HM_BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,
	STEFX_HM_BOTLIB_EA_SAY = 400,
	STEFX_HM_BOTLIB_EA_SAY_TEAM,
	STEFX_HM_BOTLIB_EA_USE_ITEM,
	STEFX_HM_BOTLIB_EA_DROP_ITEM,
	STEFX_HM_BOTLIB_EA_USE_INV,
	STEFX_HM_BOTLIB_EA_DROP_INV,
	STEFX_HM_BOTLIB_EA_GESTURE,
	STEFX_HM_BOTLIB_EA_COMMAND,
	STEFX_HM_BOTLIB_EA_SELECT_WEAPON,
	STEFX_HM_BOTLIB_EA_TALK,
	STEFX_HM_BOTLIB_EA_ATTACK,
	STEFX_HM_BOTLIB_EA_USE,
	STEFX_HM_BOTLIB_EA_RESPAWN,
	STEFX_HM_BOTLIB_EA_JUMP,
	STEFX_HM_BOTLIB_EA_DELAYED_JUMP,
	STEFX_HM_BOTLIB_EA_CROUCH,
	STEFX_HM_BOTLIB_EA_MOVE_UP,
	STEFX_HM_BOTLIB_EA_MOVE_DOWN,
	STEFX_HM_BOTLIB_EA_MOVE_FORWARD,
	STEFX_HM_BOTLIB_EA_MOVE_BACK,
	STEFX_HM_BOTLIB_EA_MOVE_LEFT,
	STEFX_HM_BOTLIB_EA_MOVE_RIGHT,
	STEFX_HM_BOTLIB_EA_MOVE,
	STEFX_HM_BOTLIB_EA_VIEW,
	STEFX_HM_BOTLIB_EA_END_REGULAR,
	STEFX_HM_BOTLIB_EA_GET_INPUT,
	STEFX_HM_BOTLIB_EA_RESET_INPUT
};

static int QDECL STEFX_HolomatchSyscall( int command, ... )
{
	va_list args;
	va_start(args, command);
	int result = 0;

	if (!s_stefxHolomatchImport)
	{
		va_end(args);
		return 0;
	}
	switch (command)
	{
	case STEFX_HM_G_PRINT:
		{
			const char *text = va_arg(args, const char *);
			if (text && (strstr(text, "bots parsed") ||
				strstr(text, "Missing { in info file") ||
				strstr(text, "Unexpected end of info file") ||
				strstr(text, "Max infos exceeded")))
			{
				XBLog_WriteCriticalf("STEFX_HM_BOT: official print text='%s'", text);
			}
			s_stefxHolomatchImport->Printf("%s", text ? text : "");
		}
		break;
	case STEFX_HM_G_ERROR:
		s_stefxHolomatchImport->Error(ERR_DROP, "%s", va_arg(args, const char *));
		break;
	case STEFX_HM_G_MILLISECONDS:
		result = s_stefxHolomatchImport->Milliseconds();
		break;
	case STEFX_HM_G_CVAR_REGISTER:
		{
			vmCvar_t *vmCvar = (vmCvar_t *)va_arg(args, void *);
			const char *name = va_arg(args, const char *);
			const char *defaultValue = va_arg(args, const char *);
			int flags = va_arg(args, int);
			Cvar_Register(vmCvar, name, defaultValue, flags);
		}
		break;
	case STEFX_HM_G_CVAR_UPDATE:
		Cvar_Update((vmCvar_t *)va_arg(args, void *));
		break;
	case STEFX_HM_G_CVAR_SET:
		{
			const char *name = va_arg(args, const char *);
			const char *value = va_arg(args, const char *);
			s_stefxHolomatchImport->cvar_set(name, value);
		}
		break;
	case STEFX_HM_G_CVAR_VARIABLE_INTEGER_VALUE:
		{
			const char *name = va_arg(args, const char *);
			result = s_stefxHolomatchImport->Cvar_VariableIntegerValue(name);
		}
		break;
	case STEFX_HM_G_CVAR_VARIABLE_STRING_BUFFER:
		{
			const char *name = va_arg(args, const char *);
			char *buffer = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			s_stefxHolomatchImport->Cvar_VariableStringBuffer(name, buffer, bufferSize);
		}
		break;
	case STEFX_HM_G_ARGC:
		result = s_stefxHolomatchImport->argc();
		break;
	case STEFX_HM_G_ARGV:
		{
			int n = va_arg(args, int);
			char *buffer = va_arg(args, char *);
			int bufferLength = va_arg(args, int);
			Q_strncpyz(buffer, s_stefxHolomatchImport->argv(n), bufferLength);
		}
		break;
	case STEFX_HM_G_FS_FOPEN_FILE:
		{
			const char *qpath = va_arg(args, const char *);
			fileHandle_t *file = (fileHandle_t *)va_arg(args, void *);
			fsMode_t mode = (fsMode_t)va_arg(args, int);
			result = s_stefxHolomatchImport->FS_FOpenFile(qpath, file, mode);
		}
		break;
	case STEFX_HM_G_FS_READ:
		{
			void *buffer = va_arg(args, void *);
			int len = va_arg(args, int);
			fileHandle_t file = va_arg(args, fileHandle_t);
			s_stefxHolomatchImport->FS_Read(buffer, len, file);
		}
		break;
	case STEFX_HM_G_FS_WRITE:
		{
			const void *buffer = va_arg(args, const void *);
			int len = va_arg(args, int);
			fileHandle_t file = va_arg(args, fileHandle_t);
			result = s_stefxHolomatchImport->FS_Write(buffer, len, file);
		}
		break;
	case STEFX_HM_G_FS_FCLOSE_FILE:
		{
			fileHandle_t file = va_arg(args, fileHandle_t);
			s_stefxHolomatchImport->FS_FCloseFile(file);
		}
		break;
	case STEFX_HM_G_SEND_CONSOLE_COMMAND:
		{
			int execWhen = va_arg(args, int);
			const char *text = va_arg(args, const char *);
			XBLog_WriteCriticalf("STEFX_HM_BOT: official console command exec=%d text='%s'",
				execWhen, text ? text : "");
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
			if (text && !Q_stricmpn(text, "addbot ", 7))
			{
				++g_SPXBHMSplitBotProof[8];
			}
#endif
			Cbuf_ExecuteText(execWhen, text ? text : "");
		}
		break;
	case STEFX_HM_G_LOCATE_GAME_DATA:
		s_stefxHolomatchGentities = va_arg(args, void *);
		s_stefxHolomatchNumEntities = va_arg(args, int);
		s_stefxHolomatchGentitySize = va_arg(args, int);
		va_arg(args, void *);
		va_arg(args, int);
		STEFX_HolomatchSyncAllToMirror();
		STEFX_HolomatchRefreshExport();
		XBLF("STEFX_HM_SP: G_LOCATE_GAME_DATA official=%p count=%d stride=%d mirror=%p mirrorStride=%d",
			s_stefxHolomatchGentities,
			s_stefxHolomatchNumEntities,
			s_stefxHolomatchGentitySize,
			STEFX_HolomatchMirrorEntity(0),
			(int)sizeof(gentity_t));
		break;
	case STEFX_HM_G_DROP_CLIENT:
		{
			int clientNum = va_arg(args, int);
			const char *reason = va_arg(args, const char *);
			s_stefxHolomatchImport->DropClient(clientNum, reason);
		}
		break;
	case STEFX_HM_G_SEND_SERVER_COMMAND:
		{
			int clientNum = va_arg(args, int);
			const char *commandText = va_arg(args, const char *);
			s_stefxHolomatchImport->SendServerCommand(clientNum, "%s", commandText);
		}
		break;
	case STEFX_HM_G_SET_CONFIGSTRING:
		{
			int num = va_arg(args, int);
			const char *value = va_arg(args, const char *);
			s_stefxHolomatchImport->SetConfigstring(num, value);
		}
		break;
	case STEFX_HM_G_GET_CONFIGSTRING:
		{
			int num = va_arg(args, int);
			char *buffer = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			s_stefxHolomatchImport->GetConfigstring(num, buffer, bufferSize);
		}
		break;
	case STEFX_HM_G_GET_USERINFO:
		{
			int clientNum = va_arg(args, int);
			char *buffer = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			s_stefxHolomatchImport->GetUserinfo(clientNum, buffer, bufferSize);
		}
		break;
	case STEFX_HM_G_SET_USERINFO:
		{
			int clientNum = va_arg(args, int);
			const char *userinfo = va_arg(args, const char *);
			s_stefxHolomatchImport->SetUserinfo(clientNum, userinfo);
		}
		break;
	case STEFX_HM_G_GET_SERVERINFO:
		{
			char *buffer = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			s_stefxHolomatchImport->GetServerinfo(buffer, bufferSize);
		}
		break;
	case STEFX_HM_G_SET_BRUSH_MODEL:
		{
			void *officialEntity = va_arg(args, void *);
			const char *name = va_arg(args, const char *);
			int entityNum = STEFX_HolomatchEntityIndex(officialEntity);
			STEFX_HolomatchSyncOfficialEntityToMirror(entityNum);
			if (entityNum >= 0)
			{
				s_stefxHolomatchImport->SetBrushModel(STEFX_HolomatchMirrorEntity(entityNum), name);
				STEFX_HolomatchSyncMirrorToOfficial(entityNum);
			}
		}
		break;
	case STEFX_HM_G_TRACE:
		{
			trace_t *traceResult = (trace_t *)va_arg(args, void *);
			const float *start = va_arg(args, const float *);
			const float *mins = va_arg(args, const float *);
			const float *maxs = va_arg(args, const float *);
			const float *end = va_arg(args, const float *);
			int passEntityNum = va_arg(args, int);
			int contentmask = va_arg(args, int);
			if ((passEntityNum < 0 || passEntityNum >= MAX_GENTITIES) &&
				s_stefxHolomatchInvalidTraceTraceCount < 32)
			{
				XBLog_WriteCriticalf("STEFX_HM_SP: invalid official trace pass=%d mask=0x%x start=(%g,%g,%g) end=(%g,%g,%g)",
					passEntityNum, contentmask,
					start ? start[0] : 0.0f, start ? start[1] : 0.0f, start ? start[2] : 0.0f,
					end ? end[0] : 0.0f, end ? end[1] : 0.0f, end ? end[2] : 0.0f);
				++s_stefxHolomatchInvalidTraceTraceCount;
			}
			if (s_stefxHolomatchBadTraceInputCount < 32 &&
				((start && (IS_NAN(start[0]) || IS_NAN(start[1]) || IS_NAN(start[2]))) ||
				 (mins && (IS_NAN(mins[0]) || IS_NAN(mins[1]) || IS_NAN(mins[2]))) ||
				 (maxs && (IS_NAN(maxs[0]) || IS_NAN(maxs[1]) || IS_NAN(maxs[2]))) ||
				 (end && (IS_NAN(end[0]) || IS_NAN(end[1]) || IS_NAN(end[2])))))
			{
				XBLog_WriteCriticalf("STEFX_HM_SP: invalid official trace input pass=%d mask=0x%x start=(%g,%g,%g) mins=(%g,%g,%g) maxs=(%g,%g,%g) end=(%g,%g,%g)",
					passEntityNum, contentmask,
					start ? start[0] : 0.0f, start ? start[1] : 0.0f, start ? start[2] : 0.0f,
					mins ? mins[0] : 0.0f, mins ? mins[1] : 0.0f, mins ? mins[2] : 0.0f,
					maxs ? maxs[0] : 0.0f, maxs ? maxs[1] : 0.0f, maxs ? maxs[2] : 0.0f,
					end ? end[0] : 0.0f, end ? end[1] : 0.0f, end ? end[2] : 0.0f);
				++s_stefxHolomatchBadTraceInputCount;
			}
			s_stefxHolomatchImport->trace(traceResult, start, mins, maxs, end,
				passEntityNum, contentmask);
			if (traceResult &&
				(IS_NAN(traceResult->fraction) ||
				 IS_NAN(traceResult->endpos[0]) ||
				 IS_NAN(traceResult->endpos[1]) ||
				 IS_NAN(traceResult->endpos[2])) &&
				s_stefxHolomatchBadTraceResultCount < 32)
			{
				XBLog_WriteCriticalf("STEFX_HM_SP: invalid SP trace result pass=%d mask=0x%x fraction=%g end=(%g,%g,%g) entity=%d",
					passEntityNum, contentmask, traceResult->fraction,
					traceResult->endpos[0], traceResult->endpos[1], traceResult->endpos[2],
					traceResult->entityNum);
				++s_stefxHolomatchBadTraceResultCount;
			}
			if (traceResult && traceResult->fraction < 1.0f &&
				(traceResult->fraction < 0.0f ||
				 traceResult->plane.normal[0] < -1.01f || traceResult->plane.normal[0] > 1.01f ||
				 traceResult->plane.normal[1] < -1.01f || traceResult->plane.normal[1] > 1.01f ||
				 traceResult->plane.normal[2] < -1.01f || traceResult->plane.normal[2] > 1.01f ||
				 IS_NAN(traceResult->plane.normal[0]) ||
				 IS_NAN(traceResult->plane.normal[1]) ||
				 IS_NAN(traceResult->plane.normal[2]) ||
				 IS_NAN(traceResult->plane.dist)) &&
				s_stefxHolomatchBadTraceResultCount < 32)
			{
				XBLog_WriteCriticalf("STEFX_HM_SP: invalid SP trace plane pass=%d fraction=%g normal=(%g,%g,%g) dist=%g entity=%d",
					passEntityNum, traceResult->fraction,
					traceResult->plane.normal[0], traceResult->plane.normal[1], traceResult->plane.normal[2],
					traceResult->plane.dist, traceResult->entityNum);
				++s_stefxHolomatchBadTraceResultCount;
			}
		}
		break;
	case STEFX_HM_G_POINT_CONTENTS:
		{
			const float *point = va_arg(args, const float *);
			int passEntityNum = va_arg(args, int);
			result = s_stefxHolomatchImport->pointcontents(point, passEntityNum);
		}
		break;
	case STEFX_HM_G_IN_PVS:
		{
			const float *point1 = va_arg(args, const float *);
			const float *point2 = va_arg(args, const float *);
			result = s_stefxHolomatchImport->inPVS(point1, point2);
		}
		break;
	case STEFX_HM_G_IN_PVS_IGNORE_PORTALS:
		{
			const float *point1 = va_arg(args, const float *);
			const float *point2 = va_arg(args, const float *);
			result = s_stefxHolomatchImport->inPVSIgnorePortals(point1, point2);
		}
		break;
	case STEFX_HM_G_ADJUST_AREA_PORTAL_STATE:
		{
			void *officialEntity = va_arg(args, void *);
			qboolean open = (qboolean)va_arg(args, int);
			int entityNum = STEFX_HolomatchEntityIndex(officialEntity);
			STEFX_HolomatchSyncOfficialEntityToMirror(entityNum);
			if (entityNum >= 0)
			{
				s_stefxHolomatchImport->AdjustAreaPortalState(STEFX_HolomatchMirrorEntity(entityNum), open);
			}
		}
		break;
	case STEFX_HM_G_AREAS_CONNECTED:
		{
			int area1 = va_arg(args, int);
			int area2 = va_arg(args, int);
			result = s_stefxHolomatchImport->AreasConnected(area1, area2);
		}
		break;
	case STEFX_HM_G_LINKENTITY:
		{
			int entityNum = STEFX_HolomatchEntityIndex(va_arg(args, void *));
			STEFX_HolomatchSyncOfficialEntityToMirror(entityNum);
			if (entityNum >= 0)
			{
				gentity_t *mirror = STEFX_HolomatchMirrorEntity(entityNum);
				s_stefxHolomatchImport->linkentity(mirror);
				STEFX_HolomatchSyncMirrorToOfficial(entityNum);
			}
		}
		break;
	case STEFX_HM_G_UNLINKENTITY:
		{
			int entityNum = STEFX_HolomatchEntityIndex(va_arg(args, void *));
			STEFX_HolomatchSyncOfficialEntityToMirror(entityNum);
			if (entityNum >= 0)
			{
				s_stefxHolomatchImport->unlinkentity(STEFX_HolomatchMirrorEntity(entityNum));
				STEFX_HolomatchSyncMirrorToOfficial(entityNum);
			}
		}
		break;
	case STEFX_HM_G_ENTITIES_IN_BOX:
		{
			const float *mins = va_arg(args, const float *);
			const float *maxs = va_arg(args, const float *);
			int *entityList = va_arg(args, int *);
			int maxcount = va_arg(args, int);
			gentity_t *mirrorList[MAX_GENTITIES];
			int i;
			if (maxcount > MAX_GENTITIES)
			{
				maxcount = MAX_GENTITIES;
			}
			result = s_stefxHolomatchImport->EntitiesInBox(mins, maxs, mirrorList, maxcount);
			for (i = 0; i < result; ++i)
			{
				entityList[i] = mirrorList[i] ? mirrorList[i]->s.number : ENTITYNUM_NONE;
			}
		}
		break;
	case STEFX_HM_G_ENTITY_CONTACT:
		{
			const float *mins = va_arg(args, const float *);
			const float *maxs = va_arg(args, const float *);
			int entityNum = STEFX_HolomatchEntityIndex(va_arg(args, const void *));
			STEFX_HolomatchSyncOfficialEntityToMirror(entityNum);
			result = (entityNum >= 0) ? s_stefxHolomatchImport->EntityContact(mins, maxs,
				STEFX_HolomatchMirrorEntity(entityNum)) : qfalse;
		}
		break;
	case STEFX_HM_G_BOT_ALLOCATE_CLIENT:
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		++g_SPXBHMSplitBotProof[9];
#endif
		result = SV_BotAllocateClient();
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		g_SPXBHMSplitBotProof[10] = (unsigned int)(result + 1);
#endif
		if (result >= 0 && svs.clients)
		{
			client_t *client = &svs.clients[result];
			client->state = CS_ACTIVE;
			client->gentity = STEFX_HolomatchMirrorEntity(result);
			client->rate = 25000;
			client->snapshotMsec = 50;
			client->lastPacketTime = sv.time;
			client->lastConnectTime = sv.time;
		}
		break;
	case STEFX_HM_G_BOT_FREE_CLIENT:
		SV_BotFreeClient(va_arg(args, int));
		break;
	case STEFX_HM_G_GET_USERCMD:
		{
			int clientNum = va_arg(args, int);
			usercmd_t spCommand;
			stefx_hm_usercmd_t *officialCommand;
			SV_GetUsercmd(clientNum, &spCommand);
			++g_SPXBHMGetUsercmdCount;
			g_SPXBHMGetUsercmdTime = (unsigned int)spCommand.serverTime;
			g_SPXBHMGetUsercmdMove = ((unsigned int)(unsigned char)spCommand.forwardmove) |
				((unsigned int)(unsigned char)spCommand.rightmove << 8) |
				((unsigned int)(unsigned char)spCommand.upmove << 16) |
				((unsigned int)spCommand.weapon << 24);
			g_SPXBHMGetUsercmdButtons = (unsigned int)spCommand.buttons;
			officialCommand = (stefx_hm_usercmd_t *)va_arg(args, void *);
			if (officialCommand)
			{
				officialCommand->serverTime = spCommand.serverTime;
				officialCommand->buttons = STEFX_SPButtonsToOfficial(spCommand.buttons);
				officialCommand->weapon = spCommand.weapon;
				officialCommand->angles[0] = spCommand.angles[0];
				officialCommand->angles[1] = spCommand.angles[1];
				officialCommand->angles[2] = spCommand.angles[2];
				officialCommand->forwardmove = spCommand.forwardmove;
				officialCommand->rightmove = spCommand.rightmove;
				officialCommand->upmove = spCommand.upmove;
			}
		}
		break;
	case STEFX_HM_G_GET_ENTITY_TOKEN:
		{
			char *buffer = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			if (s_stefxHolomatchEntityParsePoint)
			{
				const char *token = COM_Parse(&s_stefxHolomatchEntityParsePoint);
				Q_strncpyz(buffer, token, bufferSize);
				result = (s_stefxHolomatchEntityParsePoint || token[0]) ? qtrue : qfalse;
			}
			else
			{
				result = SV_GetEntityToken(buffer, bufferSize);
			}
		}
		break;
	case STEFX_HM_G_FS_GETFILELIST:
		{
			const char *path = va_arg(args, const char *);
			const char *extension = va_arg(args, const char *);
			char *list = va_arg(args, char *);
			int bufferSize = va_arg(args, int);
			result = s_stefxHolomatchImport->FS_GetFileList(path, extension, list, bufferSize);
		}
		break;
	case STEFX_HM_G_DEBUG_POLYGON_CREATE:
		va_arg(args, int);
		va_arg(args, int);
		va_arg(args, void *);
		result = 0;
		break;
	case STEFX_HM_G_DEBUG_POLYGON_DELETE:
		va_arg(args, int);
		break;
	default:
		if (!STEFX_HolomatchBotSyscall(command, args, &result))
		{
			XBLog_WriteCriticalf("STEFX_HM_SP: unhandled official game syscall=%d", command);
		}
		break;
	}

	va_end(args);
	return result;
}

static int STEFX_HolomatchVM( int command, int arg0 = 0, int arg1 = 0, int arg2 = 0 )
{
	return vmMain(command, arg0, arg1, arg2, 0, 0, 0, 0);
}

static qboolean s_stefxHolomatchBotFramesEnabled = qfalse;

static void STEFX_HolomatchInit( const char *mapname, const char *spawntarget,
	int checksum, const char *entstring, int levelTime, int randomSeed, int globalTime,
	SavedGameJustLoaded_e savedGame, qboolean loadTransition )
{
	(void)mapname;
	(void)spawntarget;
	(void)checksum;
	(void)globalTime;
	(void)savedGame;
	(void)loadTransition;
	s_stefxHolomatchEntityParsePoint = entstring;
	// Match official G_InitGame: bot cvars and the bot library exist only
	// when bot_enable was set at initialization. Uninitialized VM cvar
	// handles must never be updated by BotAIStartFrame.
	s_stefxHolomatchBotFramesEnabled = Cvar_VariableIntegerValue("bot_enable") ? qtrue : qfalse;
	STEFX_HolomatchVM(0, levelTime, randomSeed, 0);
	XBLog_WriteCriticalf("STEFX_HM_BOT_FRAME_GATE: initializedEnabled=%d", s_stefxHolomatchBotFramesEnabled);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchShutdown()
{
	STEFX_HolomatchVM(1, 0, 0, 0);
	s_stefxHolomatchBotFramesEnabled = qfalse;
}

void STEFX_HolomatchBotFrame( int levelTime )
{
	if (!s_stefxHolomatchImport || !s_stefxHolomatchBotFramesEnabled)
	{
		return;
	}
	STEFX_HolomatchVM(10, levelTime, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static char *STEFX_HolomatchClientConnect( int clientNum, qboolean firstTime, SavedGameJustLoaded_e savedGame )
{
	char *result = (char *)STEFX_HolomatchVM(2, clientNum, firstTime, savedGame);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
	return result;
}

static void STEFX_HolomatchClientBegin( int clientNum, usercmd_t *cmd, SavedGameJustLoaded_e savedGame )
{
	(void)cmd;
	(void)savedGame;
	STEFX_HolomatchVM(3, clientNum, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchClientUserinfoChanged( int clientNum )
{
	STEFX_HolomatchVM(4, clientNum, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchClientDisconnect( int clientNum )
{
	STEFX_HolomatchVM(5, clientNum, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchClientCommand( int clientNum )
{
	STEFX_HolomatchVM(6, clientNum, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchClientThink( int clientNum, usercmd_t *cmd )
{
	(void)cmd;
	++g_SPXBHMClientThinkCount;
	STEFX_HolomatchVM(7, clientNum, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static void STEFX_HolomatchConnectNavs( const char *mapname, int checksum )
{
	/* The SP host owns this export slot.  Holomatch does not expose the SP
	 * navigator service, and the direct-map bot harness does not consume it. */
	(void)mapname;
	(void)checksum;
}

static void STEFX_HolomatchRunFrame( int levelTime )
{
	STEFX_HolomatchVM(8, levelTime, 0, 0);
	STEFX_HolomatchSyncAllToMirror();
	STEFX_HolomatchRefreshExport();
}

static qboolean STEFX_HolomatchConsoleCommand()
{
	return (qboolean)STEFX_HolomatchVM(9, 0, 0, 0);
}

game_export_t *STEFX_GetHolomatchGameAPI( game_import_t *import )
{
	if (import)
	{
		/* SV_InitGameProgs passes a stack-local import table.  The official VM
		 * retains the adapter between calls, so keep the function pointers in
		 * storage that outlives that stack frame. */
		memcpy(&s_stefxHolomatchImportStorage, import, sizeof(s_stefxHolomatchImportStorage));
		s_stefxHolomatchImport = &s_stefxHolomatchImportStorage;
	}
	else
	{
		s_stefxHolomatchImport = NULL;
	}
	s_stefxHolomatchGentities = NULL;
	s_stefxHolomatchGentitySize = 0;
	s_stefxHolomatchNumEntities = 0;
	s_stefxHolomatchEntityParsePoint = NULL;
	s_stefxHolomatchInvalidTraceTraceCount = 0;
	s_stefxHolomatchBadTraceInputCount = 0;
	s_stefxHolomatchBadTraceResultCount = 0;
	memset(s_stefxHolomatchMirrorStorage, 0, sizeof(s_stefxHolomatchMirrorStorage));
	memset(s_stefxHolomatchMirrorClientStates, 0, sizeof(s_stefxHolomatchMirrorClientStates));
	memset(&s_stefxHolomatchExport, 0, sizeof(s_stefxHolomatchExport));
	STEFX_HolomatchBotReset();

	dllEntry(STEFX_HolomatchSyscall);

	s_stefxHolomatchExport.apiversion = GAME_API_VERSION;
	s_stefxHolomatchExport.Init = STEFX_HolomatchInit;
	s_stefxHolomatchExport.Shutdown = STEFX_HolomatchShutdown;
	s_stefxHolomatchExport.ClientConnect = STEFX_HolomatchClientConnect;
	s_stefxHolomatchExport.ClientBegin = STEFX_HolomatchClientBegin;
	s_stefxHolomatchExport.ClientUserinfoChanged = STEFX_HolomatchClientUserinfoChanged;
	s_stefxHolomatchExport.ClientDisconnect = STEFX_HolomatchClientDisconnect;
	s_stefxHolomatchExport.ClientCommand = STEFX_HolomatchClientCommand;
	s_stefxHolomatchExport.ClientThink = STEFX_HolomatchClientThink;
	s_stefxHolomatchExport.RunFrame = STEFX_HolomatchRunFrame;
	s_stefxHolomatchExport.ConsoleCommand = STEFX_HolomatchConsoleCommand;
	s_stefxHolomatchExport.ConnectNavs = STEFX_HolomatchConnectNavs;
	STEFX_HolomatchRefreshExport();

	XBLog_Write("STEFX: SP-hosted official EF VM game API selected; SP engine services attached");
	return &s_stefxHolomatchExport;
}
