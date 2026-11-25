// Driven by verify_campaign_corpse_pose.py. Only host services are stubbed;
// paths.inc contains the production game and cgame functions, not copies here.
#include <cstdlib>
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cassert>
static int pick;
static int xdk_rand15() { return (32768*pick+6)/7; } // XDK rand(), not glibc RAND_MAX
#define rand xdk_rand15
#define random ef_host_random
#include "game/b_local.h"
#include "game/g_functions.h"
#include "game/anims.h"
#include "cgame/cg_local.h"
#include "cgame/cg_media.h"
#include "game/Q3_Interface.h"
#undef random
#undef rand

static bool pvs, fov, los, freed;
static gentity_t entities[2], tempEnt;
gentity_t *g_entities = entities;
static gclient_t victimClient, shooterClient;
static gNPC_t ai;
static centity_t entity;
static snapshot_t snap;
game_import_t gi;
level_locals_t level;
cg_t cg;
cgs_t cgs;
gentity_t *NPC;
gNPC_t *NPCInfo;
gclient_t *client;
usercmd_t ucmd;
vmCvar_t cg_debugAnim, cg_animSpeed, cg_noPlayerAnims, cg_debugPosition;
vmCvar_t cg_stefxCullHiddenPlayers, cg_stefxSplitScreen;
static cvar_t icarusDebug;
cvar_t *g_ICARUSDebug = &icarusDebug;
int numKnownAnimFileSets = 1;
animFileSet_t knownAnimFileSets[MAX_ANIM_FILES];
stringID_table_t animTable[MAX_ANIMATIONS+1];
static int g_SPXBCgHiddenPlayersSkipped;

void Com_Printf(const char *, ...) {}
void CG_Printf(const char *, ...) {}
void CG_Error(const char *, ...) { assert(false); }
void CG_PlayerAnimSounds(animsounds_t *, int, int) {}
void Q3_TaskIDClear(int *id) { *id=-1; }
qboolean Q3_TaskIDPending(gentity_t *, taskID_t) { return qfalse; }
void Q3_TaskIDComplete(gentity_t *, taskID_t) { assert(false); }
void G_FreeEntity(gentity_t *) { freed=true; }
void ClientThink(int, usercmd_t *) {}
qboolean InFOV(gentity_t *, gentity_t *, int, int) { return fov?qtrue:qfalse; }
qboolean NPC_ClearLOS(gentity_t *, vec3_t) { return los?qtrue:qfalse; }
static bool SV_inPVS(const vec3_t, const vec3_t) { return pvs; }
gentity_t *G_TempEntity(vec3_t, int) { return &tempEnt; }
void G_AddEvent(gentity_t *, int, int) {}
void G_Sound(gentity_t *, int) {}
int G_SoundIndex(char *) { return 1; }
static int targetUses;
void G_UseTargets(gentity_t *e,gentity_t *) { if(e->target && !strcmp(e->target,"counter2")) ++targetUses; }
void G_UseTargets2(gentity_t *,gentity_t *,const char *) {}
void G_CheckVictoryScript(gentity_t *) {}
void G_SetMissionStatusText(gentity_t *, int) {}
void G_MakeTeamVulnerable() {}
void LookAtKiller(gentity_t *,gentity_t *,gentity_t *) {}
void TossClientItems(gentity_t *) {}
qboolean OnSameTeam(gentity_t *,gentity_t *) { return qfalse; }
void G_AlertTeam(gentity_t *,gentity_t *,float,float) {}
void G_DeathAlert(gentity_t *,gentity_t *) {}
void G_ActivateBehavior(gentity_t *,int) {}
entlist_t ICARUS_EntList;
struct HostScriptPool { void DeleteSequencer(CSequencer *) {} } scriptPool;
#define iICARUS (&scriptPool)
static char *strupr(char *s) { for(char *p=s;*p;++p) *p=toupper((unsigned char)*p); return s; }
void OBJ_SetPendingObjectives(gentity_t *) {}
void AngleVectors(const vec3_t, vec3_t forward, vec3_t, vec3_t) { VectorSet(forward,1,0,0); }
float VectorNormalize2(const vec3_t v, vec3_t out) {
	const float len=sqrt(DotProduct(v,v)); if(len>0) VectorScale(v,1/len,out); return len;
}
void EvaluateTrajectory(const trajectory_t *tr,int,vec3_t v) { VectorCopy(tr->trBase,v); }
static int contents(const vec3_t,int) { return 0; }
static void linked(gentity_t *) {}
static void hitRaider(trace_t *t,const vec3_t,const vec3_t,const vec3_t,const vec3_t end,int pass,int mask) {
	assert(pass==0 && mask==MASK_SHOT); t->entityNum=1; VectorCopy(end,t->endpos);
}
static vec3_t muzzle, forward = {1,0,0};
weaponData_t weaponData[WP_NUM_WEAPONS];
void AddSightEvent(gentity_t *,vec3_t,float,alertEventLevel_e) {}
void AddSoundEvent(gentity_t *,vec3_t,float,alertEventLevel_e) {}
qboolean W_CheckBorgAdapt(gentity_t *,int,gentity_t *target,vec3_t) {
	assert(target->client->playerTeam!=TEAM_BORG); return qtrue;
}
void CG_CheckEvents(centity_t *) {}
static void draw(centity_t *);
cvar_t *g_debugDamage=&icarusDebug, *debugNPCFreeze=&icarusDebug;
int teamLastEnemyTime[TEAM_NUM_TEAMS];
gentity_t *g_lastClientDamaged=NULL;
qboolean stop_icarus=qfalse;
int Q_stricmp(const char *a, const char *b) { return strcasecmp(a,b); }
void G_FriendlyFireReaction(gentity_t *,gentity_t *,qboolean) {}
void G_ApplyKnockback(gentity_t *, vec3_t, float) { assert(false); }
void GEntity_PainFunc(gentity_t *,gentity_t *,int) {}
void NPC_ExecuteBState(gentity_t *) {}
qboolean NPC_UpdateAngles(qboolean,qboolean) { return qtrue; }
void NPC_ApplyRoff() {}
int CTaskManager::Update() { assert(false); return 0; }
void Com_Error(int,const char *,...) { assert(false); }
#define OTHER_DEATH(name) void name(gentity_t *,gentity_t *,gentity_t *,int,int) { assert(false); }
OTHER_DEATH(funcBBrushDie) OTHER_DEATH(misc_model_breakable_die) OTHER_DEATH(ExplodeDeath_Wait)
OTHER_DEATH(func_usable_die) OTHER_DEATH(mine_die) OTHER_DEATH(flier_die) OTHER_DEATH(turret_die)
OTHER_DEATH(bot_rocket_die) OTHER_DEATH(reaver_vat_die)

// Compile the shipping Xbox SP branches, after the original headers have
// been imported using Linux CRT types. Diagnostics are not under test here.
#define _XBOX
#define STEFX_ELITE_FORCE_SP
#define XBLF(...) ((void)0)
#define XBLog_WriteCriticalf(...) ((void)0)
static cvar_t split;
static cvar_t *cv(const char *key,const char *,int) {
	return !strcmp(key,"stefx_splitScreen")?&split:&icarusDebug;
}
static int cvi(const char *key) { return !strcmp(key,"stefx_splitScreen")?split.integer:0; }

#include "paths.inc"

static int leg, torso;
static float legMix, torsoMix;
static void draw(centity_t *cent) {
	int oldLeg, oldTorso;
	CG_PlayerAnimation(cent,&oldLeg,&leg,&legMix,&oldTorso,&torso,&torsoMix);
	cent->gent->client->renderInfo.legsFrame=cent->pe.legs.frame;
}
static void timeAt(int t) { cg.time=level.time=t; }
static void fresh(team_t team) {
	memset(&entity,0,sizeof entity);
	memset(entities,0,sizeof entities);
	memset(&ai,0,sizeof ai);
	memset(&victimClient,0,sizeof victimClient);
	memset(&shooterClient,0,sizeof shooterClient);
	gentity_t &e=entities[1];
	entities[0].client=&shooterClient;
	e.NPC=&ai; e.client=&victimClient; e.health=75; e.s.number=1;
	e.s.legsAnim=BOTH_STAND1; e.s.eType=ET_PLAYER; e.e_DieFunc=dieF_player_die;
	e.NPC_type=(char*)"raider"; shooterClient.playerTeam=TEAM_FREE;
	e.client->playerTeam=team; e.client->ps.legsAnim=e.client->ps.torsoAnim=BOTH_STAND1;
	e.client->ps.stats[STAT_HEALTH]=e.health; e.takedamage=qtrue;
	e.client->renderInfo.legsFrame=-1; e.s.groundEntityNum=ENTITYNUM_WORLD;
	entity.currentState.clientNum=MAX_CLIENTS;
	entity.currentState.number=e.s.number;
	entity.gent=&e;
	gi.linkentity=linked; gi.pointcontents=contents; gi.trace=hitRaider; gi.cvar=cv; gi.Cvar_VariableIntegerValue=cvi;
	shooterClient.ps.ammo[AMMO_PHASER]=1; weaponData[WP_PHASER].range=1024;
	cg.snap=&snap; cg_animSpeed.integer=cg_stefxCullHiddenPlayers.integer=1;
	pvs=false; freed=false; fov=los=false; SetNPCGlobals(&e); targetUses=0;
	// voy4's os1 is a named Hirogen with a spawn behavior and a death target,
	// NOT a death script. NPC_Begin initializes both ICARUS pointers.
	e.script_targetname=(char*)"os1"; e.target=(char*)"counter2";
	e.behaviorSet[BSET_SPAWN]=(char*)"voy4/genericscavenger_run";
	e.sequencer=(CSequencer*)&ai; e.taskManager=(CTaskManager*)&ai;
	ICARUS_EntList.clear(); ICARUS_AssociateEnt(&e);
	assert(ICARUS_EntList.count("OS1")==1);
	timeAt(100000);
}
static void die(int mod=MOD_PHASER) {
	gentity_t *e=&entities[1];
	if (mod==MOD_PHASER || mod==MOD_PHASER_ALT) {
		e->health=e->client->ps.stats[STAT_HEALTH]=4;
		WP_FirePhaser(&entities[0],mod==MOD_PHASER_ALT?qtrue:qfalse);
	} else {
		G_Damage(e,&entities[0],&entities[0],NULL,NULL,79,DAMAGE_NO_ARMOR,mod);
	}
	assert(e->client->ps.pm_type==PM_DEAD && !e->takedamage && e->contents==CONTENTS_CORPSE);
	assert(e->client->ps.legsAnimTimer==-1 && e->client->ps.torsoAnimTimer==-1);
	if(!e->behaviorSet[BSET_DEATH]) {
		assert(!e->sequencer && !e->taskManager && !ICARUS_EntList.count("OS1"));
	} else {
		assert(e->sequencer && e->taskManager && ICARUS_EntList.count("OS1"));
	}
	assert(targetUses==(mod==MOD_KNOCKOUT?0:1));
}
static void pulse() { NPC_Think(&entities[1]); }
static void enterSnapshot() {
	entityState_t state=entities[1].s;
	state.clientNum=MAX_CLIENTS;
	state.legsAnim=victimClient.ps.legsAnim; state.torsoAnim=victimClient.ps.torsoAnim;
	entity.currentValid=qfalse;
	CG_SetEntityNextState(&entity,&state); CG_TransitionEntity(&entity);
	assert(entity.currentValid);
}
static void modelTests(bool stock, int* nums) {
	animation_t *a=knownAnimFileSets[0].animations;
	memset(a,0,sizeof knownAnimFileSets[0].animations);
	a[BOTH_STAND1].numFrames=1; a[BOTH_STAND1].loopFrames=-1;
	a[BOTH_STAND1].frameLerp=a[BOTH_STAND1].initialLerp=100;
	for(int n=0;n<11;++n) {
		animation_t &fall=a[BOTH_DEATH1+n]; animation_t &rest=a[BOTH_DEAD1+n];
		fall.firstFrame=nums[n*3]; fall.numFrames=nums[n*3+1]; fall.loopFrames=-1;
		fall.frameLerp=fall.initialLerp=nums[n*3+2]; rest=fall;
		rest.firstFrame+=fall.numFrames-1; rest.numFrames=1;
	}
	for(int n=0;n<11;++n) {
		const int id=BOTH_DEATH1+n, rest=BOTH_DEAD1+n, first=a[id].firstFrame;
		const int last=a[rest].firstFrame, duration=a[id].initialLerp+a[id].numFrames*a[id].frameLerp;
		team_t teams[]={TEAM_KLINGON,TEAM_MALON,TEAM_HIROGEN,TEAM_SCAVENGERS};
		fresh(teams[n%4]);
		pick=n<7?n:(n%2?0:6);
		if(n>=7) {
			victimClient.ps.pm_time=100; victimClient.ps.pm_flags|=PMF_TIME_KNOCKBACK;
			VectorSet(victimClient.ps.velocity,(n<9?200:-200),0,20);
		}
		die();
		assert((victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==id);
		// Player/view is in a different PVS: no client frame is published.
		for(int dt=0;dt<duration;dt+=50) {
			timeAt(100000+dt); assert(!present(&entity)); pulse();
			assert(!ai.timeOfDeath && (victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==id);
		}
		timeAt(100000+duration); pulse();
		assert(ai.timeOfDeath==(stock?0:level.time+15000));
		assert((victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==(stock?id:rest));
		// Re-enter the snapshot/PVS well after the real fall should be over.
		timeAt(104000+duration); pvs=true; enterSnapshot();
		assert(present(&entity)); assert(leg==(stock?first:last) && torso==leg);
		assert(legMix==0 && torsoMix==0); // no interpolation masking this frame
		timeAt(level.time+50); present(&entity);
		assert(leg==last && torso==last); // stock does not stay upright on a continuous draw
		assert(victimClient.ps.legsAnimTimer==-1 && victimClient.ps.torsoAnimTimer==-1);
		pulse(); assert(ai.timeOfDeath);
		const int health=NPC->health, hits=shooterClient.ps.persistant[PERS_ACCURACY_HITS];
		WP_FirePhaser(&entities[0],qfalse); // corpse isn't a second kill/target trigger
		assert(NPC->health==health && NPC->contents==CONTENTS_CORPSE && !NPC->takedamage);
		assert(targetUses==1 && shooterClient.ps.persistant[PERS_ACCURACY_HITS]==hits);
		timeAt(ai.timeOfDeath-1); pulse(); assert(NPC->e_ThinkFunc!=thinkF_NPC_RemoveBody);
		timeAt(ai.timeOfDeath); pulse(); assert(NPC->e_ThinkFunc==thinkF_NPC_RemoveBody);
		NPC->currentOrigin[0]=256; NPC_RemoveBody(NPC); assert(!freed);
		NPC->currentOrigin[0]=257; fov=los=true; NPC_RemoveBody(NPC); assert(!freed);
		los=false; NPC_RemoveBody(NPC); assert(freed);

		// Continuous visibility: render a normal fall, not just a late join.
		fresh(TEAM_KLINGON); pvs=true; enterSnapshot(); present(&entity);
		timeAt(level.time+50); present(&entity);
		if(n>=7) {
			victimClient.ps.pm_time=100; victimClient.ps.pm_flags|=PMF_TIME_KNOCKBACK;
			VectorSet(victimClient.ps.velocity,(n<9?200:-200),0,20);
		}
		die();
		const int start=level.time;
		bool sawMiddle=false;
		for(int dt=0;dt<=duration+150;dt+=25) {
			timeAt(start+dt); pulse(); present(&entity);
			if(leg>first && leg<last) sawMiddle=true;
		}
		assert(sawMiddle && leg==last && torso==last && ai.timeOfDeath);
	}
	pick=0;
	// A scripted, undying living "deadguy", and a living actor waiting on
	// a map signal are not deaths. A real death with a script retains it.
	fresh(TEAM_MALON); ai.nextBStateThink=300000; NPC->flags|=FL_UNDYING; victimClient.ps.legsAnim=BOTH_DEAD2;
	G_Damage(NPC,&entities[0],&entities[0],NULL,NULL,79,DAMAGE_NO_ARMOR,MOD_PHASER);
	timeAt(200000); pulse(); assert(NPC->health==1 && !ai.timeOfDeath && victimClient.ps.legsAnim==BOTH_DEAD2);
	fresh(TEAM_MALON); ai.nextBStateThink=300000; timeAt(200000); pulse(); assert(victimClient.ps.legsAnim==BOTH_STAND1);
	fresh(TEAM_MALON); NPC->behaviorSet[BSET_DEATH]=(char*)"common/stumbledie";
	NPC->taskManager=(CTaskManager*)&ai; NPC->sequencer=(CSequencer*)&ai;
	ai.nextBStateThink=300000; // do not enter the script interpreter stub
	die(); timeAt(200000); pulse(); assert(!ai.timeOfDeath && NPC->taskManager && NPC->sequencer);
	NPC->taskManager=NULL; NPC->sequencer=NULL; pulse(); assert(!ai.timeOfDeath);
	assert((victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==BOTH_DEATH1);
	fresh(TEAM_MALON); die(MOD_KNOCKOUT); assert(ai.timeOfDeath==110000);
	timeAt(109999); pulse(); assert(ai.timeOfDeath==110000);
	assert((victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==BOTH_DEATH1);
	fresh(TEAM_HIROGEN); die(MOD_PHASER_ALT); assert(ai.timeOfDeath==102000);
	timeAt(102000); pulse(); assert(NPC->e_ThinkFunc==thinkF_NPC_RemoveBody);
	fresh(TEAM_STARFLEET); die(); timeAt(200000); pulse(); assert(!ai.timeOfDeath);
	fresh(TEAM_HIROGEN); die(); victimClient.ps.powerups[PW_DISINT_2]=200500;
	timeAt(200000); pulse(); assert(!ai.timeOfDeath);
	fresh(TEAM_MALON); die(); a[BOTH_DEATH1].loopFrames=0;
	timeAt(200000); pulse(); assert(!ai.timeOfDeath); a[BOTH_DEATH1].loopFrames=-1;
	fresh(TEAM_KLINGON); split.integer=1; die(); timeAt(200000); pulse();
	assert(!ai.timeOfDeath); split.integer=0;
	// Missing or nonmatching authored rest pose: never guess another frame.
	fresh(TEAM_MALON); die(); int expected=a[BOTH_DEAD1].firstFrame;
	a[BOTH_DEAD1].firstFrame=0; timeAt(200000); pulse(); assert(!ai.timeOfDeath);
	a[BOTH_DEAD1].firstFrame=expected; victimClient.ps.torsoAnim=BOTH_STAND1;
	pulse(); assert(!ai.timeOfDeath); assert((victimClient.ps.legsAnim&~ANIM_TOGGLEBIT)==BOTH_DEATH1);
	if(!stock) {
		fresh(TEAM_MALON); die(); victimClient.clientInfo.animFileIndex=-1;
		timeAt(200000); pulse(); assert(!ai.timeOfDeath);
		victimClient.clientInfo.animFileIndex=0; victimClient.ps.legsAnim=MAX_ANIMATIONS;
		pulse(); assert(!ai.timeOfDeath);
	}
}
int main(int argc,char **argv) {
	assert(argc==35); int rows[33]; for(int i=0;i<33;i++) rows[i]=atoi(argv[i+2]);
	modelTests(atoi(argv[1])!=0,rows);
	printf("11/11 hidden and continuously rendered deaths, cleanup gates and script/special controls passed\n");
}
