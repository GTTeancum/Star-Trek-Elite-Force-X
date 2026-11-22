// world.c -- world query functions

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"

#include "../qcommon/cm_local.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
#endif

 /*
Ghoul2 Insert Start
*/

#if !defined(GHOUL2_SHARED_H_INC)
	#include "..\game\ghoul2_shared.h"	//for CGhoul2Info_v
#endif
#if !defined(G2_H_INC)
	#include "..\ghoul2\G2.h"
#endif
#if !defined (MINIHEAP_H_INC)
	#include "../qcommon/miniheap.h"
#endif

#ifdef _DEBUG
	#include <float.h>
#endif //_DEBUG
/*
Ghoul2 Insert End
*/
#if MEM_DEBUG
#include "..\smartheap\heapagnt.h"
#define SV_TRACE_PROFILE (0)
#endif

#if 0 //G2_SUPERSIZEDBBOX is not being used
static const float superSizedAdd=64.0f;
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
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
#define STEFX_SV_TRACE_STAGE(phase, subphase) \
	do { g_SPXBPhaseLast = (phase); g_SPXBComSubphase = (subphase); g_SPXBSVProbePhase = (phase); g_SPXBSVProbeSubphase = (subphase); } while (0)
#define STEFX_SV_TRACE_DETAIL(a, b, c, d) \
	do { g_SPXBComSpinCount = (a); g_SPXBComMsec = (b); g_SPXBComFrameTime = (c); g_SPXBComLastTime = (d); g_SPXBSVProbeA = (a); g_SPXBSVProbeB = (b); g_SPXBSVProbeC = (c); g_SPXBSVProbeD = (d); } while (0)

static qboolean STEFX_SVFloatBitsBad(const float *v)
{
	const unsigned int bits = *(const unsigned int *)v;
	const unsigned int absBits = bits & 0x7fffffff;
	const unsigned int expBits = bits & 0x7f800000;

	if (expBits == 0x7f800000)
	{
		return qtrue;
	}
	if (absBits > 0x49800000)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean STEFX_SVVec3Bad(const vec3_t v)
{
	return (qboolean)(STEFX_SVFloatBitsBad(&v[0]) ||
		STEFX_SVFloatBitsBad(&v[1]) ||
		STEFX_SVFloatBitsBad(&v[2]));
}

static qboolean STEFX_SVBoundsBad(const vec3_t mins, const vec3_t maxs)
{
	if (STEFX_SVVec3Bad(mins) || STEFX_SVVec3Bad(maxs))
	{
		return qtrue;
	}
	if (mins[0] > maxs[0] || mins[1] > maxs[1] || mins[2] > maxs[2])
	{
		return qtrue;
	}
	if ((maxs[0] - mins[0]) > 4096.0f ||
		(maxs[1] - mins[1]) > 4096.0f ||
		(maxs[2] - mins[2]) > 4096.0f)
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean STEFX_IsValidSvEntityPtr(const svEntity_t *ent)
{
	const unsigned int base = (unsigned int)&sv.svEntities[0];
	const unsigned int end = base + sizeof(sv.svEntities);
	const unsigned int ptr = (unsigned int)ent;
	const unsigned int stride = sizeof(svEntity_t);

	if (!ent || ptr < base || ptr >= end)
	{
		return qfalse;
	}

	return (qboolean)(((ptr - base) % stride) == 0);
}

static qboolean STEFX_IsValidGEntityPtr(const gentity_t *ent)
{
	const unsigned int base = ge && ge->gentities ? (unsigned int)ge->gentities : 0;
	const unsigned int stride = ge ? (unsigned int)ge->gentitySize : 0;
	const unsigned int ptr = (unsigned int)ent;

	if (!base || !stride || !ent || ptr < base)
	{
		return qfalse;
	}
	if (ptr >= base + stride * MAX_GENTITIES)
	{
		return qfalse;
	}
	return (qboolean)(((ptr - base) % stride) == 0);
}
#endif

/*
================
SV_ClipHandleForEntity

Returns a headnode that can be used for testing or clipping to a
given entity.  If the entity is a bsp model, the headnode will
be returned, otherwise a custom box tree will be constructed.
================
*/
clipHandle_t SV_ClipHandleForEntity( const gentity_t *ent ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_badClipEntityLogCount = 0;
	if (!ent)
	{
		if (s_badClipEntityLogCount < 32)
		{
			XBLF("STEFX: SV_ClipHandleForEntity null entity; using zero box");
		}
		s_badClipEntityLogCount++;
		return CM_TempBoxModel(vec3_origin, vec3_origin);
	}
	if (STEFX_SVBoundsBad(ent->mins, ent->maxs))
	{
		if (s_badClipEntityLogCount < 32)
		{
			XBLF("STEFX: SV_ClipHandleForEntity bad bounds ent=%d bmodel=%d modelindex=%d mins=(%g,%g,%g) maxs=(%g,%g,%g); using zero box",
				ent->s.number, ent->bmodel, ent->s.modelindex,
				ent->mins[0], ent->mins[1], ent->mins[2],
				ent->maxs[0], ent->maxs[1], ent->maxs[2]);
		}
		s_badClipEntityLogCount++;
		return CM_TempBoxModel(vec3_origin, vec3_origin);
	}
	if ( ent->bmodel ) {
		if (ent->s.modelindex <= 0 || ent->s.modelindex >= MAX_SUBMODELS)
		{
			if (s_badClipEntityLogCount < 32)
			{
				XBLF("STEFX: SV_ClipHandleForEntity invalid bmodel ent=%d modelindex=%d contents=0x%x mins=(%g,%g,%g) maxs=(%g,%g,%g); using bbox",
					ent->s.number, ent->s.modelindex, ent->contents,
					ent->mins[0], ent->mins[1], ent->mins[2],
					ent->maxs[0], ent->maxs[1], ent->maxs[2]);
			}
			s_badClipEntityLogCount++;
			return CM_TempBoxModelContents(ent->mins, ent->maxs, ent->contents);
		}
		// explicit hulls in the BSP model
		return CM_InlineModel( ent->s.modelindex );
	}
#else
	if ( ent->bmodel ) {
		// explicit hulls in the BSP model
		return CM_InlineModel( ent->s.modelindex );
	}
#endif

	// create a temp tree from bounding box sizes
	return CM_TempBoxModelContents( ent->mins, ent->maxs, ent->contents );
}

#if defined(STEFX_SP_HOSTED_MP)
void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins,
	const vec3_t maxs, const vec3_t end, int entityNum, int contentmask )
{
	gentity_t *touch;
	clipHandle_t clipHandle;
	const float *origin;
	const float *angles;

	memset(trace, 0, sizeof(*trace));
	trace->fraction = 1.0f;
	VectorCopy(end, trace->endpos);
	trace->entityNum = ENTITYNUM_NONE;

	if (!ge || !ge->gentities || entityNum < 0 || entityNum >= ge->num_entities)
	{
		return;
	}

	touch = SV_GentityNum(entityNum);
	if (!touch || !touch->inuse || !(contentmask & touch->contents))
	{
		return;
	}

	clipHandle = SV_ClipHandleForEntity(touch);
	origin = touch->currentOrigin;
	angles = touch->bmodel ? touch->currentAngles : vec3_origin;
	CM_TransformedBoxTrace(trace, start, end, mins ? mins : vec3_origin,
		maxs ? maxs : vec3_origin, clipHandle, contentmask, origin, angles);

	if (trace->fraction < 1.0f || trace->startsolid || trace->allsolid)
	{
		trace->entityNum = touch->s.number;
	}
}

void SV_BotModelBounds( int modelNum, const vec3_t angles, vec3_t outMins,
	vec3_t outMaxs, vec3_t origin )
{
	clipHandle_t handle;
	vec3_t mins;
	vec3_t maxs;
	float radius;
	int i;

	handle = CM_InlineModel(modelNum);
	CM_ModelBounds(cmg, handle, mins, maxs);
	if (angles && (angles[0] || angles[1] || angles[2]))
	{
		radius = RadiusFromBounds(mins, maxs);
		for (i = 0; i < 3; ++i)
		{
			mins[i] = -radius;
			maxs[i] = radius;
		}
	}
	if (outMins) VectorCopy(mins, outMins);
	if (outMaxs) VectorCopy(maxs, outMaxs);
	if (origin) VectorClear(origin);
}
#endif



/*
===============================================================================

ENTITY CHECKING

To avoid linearly searching through lists of entities during environment testing,
the world is carved up with an evenly spaced, axially aligned bsp tree.  Entities
are kept in chains either at the final leafs, or at the first node that splits
them, which prevents having to deal with multiple fragments of a single entity.

===============================================================================
*/

typedef struct worldSector_s {
	int		axis;		// -1 = leaf node
	float	dist;
	struct worldSector_s	*children[2];
	svEntity_t	*entities;
} worldSector_t;

#define	AREA_DEPTH	8
#define	AREA_NODES	1024

worldSector_t	sv_worldSectors[AREA_NODES];
int			sv_numworldSectors;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static bool s_indexSectors;
static bool s_occupied[AREA_NODES];
static short s_parent[AREA_NODES];
extern "C" volatile unsigned int g_SPXBSplitSectorSkips=0;
static bool SV_IndexSector(worldSector_t *node,int parent) {
    const int id=node-sv_worldSectors;
    s_parent[id]=(short)parent;
    bool occupied=node->entities!=NULL;
    if(node->axis!=-1) {
        bool left=SV_IndexSector(node->children[0],id);
        bool right=SV_IndexSector(node->children[1],id);
        occupied=occupied || left || right;
    }
    return s_occupied[id]=occupied;
}
static void SV_UpdateOccupied(worldSector_t *node) {
    if(!s_indexSectors) return;
    int id=node-sv_worldSectors;
    while(id>=0) {
        node=&sv_worldSectors[id];
        bool occupied=node->entities!=NULL || (node->axis!=-1 &&
            (s_occupied[node->children[0]-sv_worldSectors] || s_occupied[node->children[1]-sv_worldSectors]));
        if(s_occupied[id]==occupied) break;
        s_occupied[id]=occupied;id=s_parent[id];
    }
}
#endif

#ifdef _XBOX
static int STEFX_TruncFloatToInt(float value)
{
	int result = 0;
	int sign = 1;

	if (value < 0.0f)
	{
		sign = -1;
		value = -value;
	}

	while (value >= 1.0f && result < 4096)
	{
		value -= 1.0f;
		result++;
	}

	return result * sign;
}
#endif

/*
===============
SV_CreateworldSector

Builds a uniformly subdivided tree for the given world size
===============
*/
worldSector_t *SV_CreateworldSector( int depth, vec3_t mins, vec3_t maxs ) {
	worldSector_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_worldSectors[sv_numworldSectors];
	sv_numworldSectors++;

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1]) {
		anode->axis = 0;
	} else {
		anode->axis = 1;
	}

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateworldSector (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateworldSector (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld( void ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	s_indexSectors=false;
#endif
	clipHandle_t	h;
	vec3_t			mins, maxs;

	memset( sv_worldSectors, 0, sizeof(sv_worldSectors) );
	sv_numworldSectors = 0;

	// get world map bounds
	h = CM_InlineModel( 0 );
	CM_ModelBounds( cmg, h, mins, maxs );
	SV_CreateworldSector( 0, mins, maxs );
}


/*
===============
SV_UnlinkEntity

===============
*/
void SV_UnlinkEntity( gentity_t *gEnt ) {
	svEntity_t		*ent;
	svEntity_t		*scan;
	worldSector_t	*ws;

	// this should never be called with a freed entity
	if ( !gEnt->inuse ) {
		return;
	}

	ent = SV_SvEntityForGentity( gEnt );

	gEnt->linked = qfalse;

	ws = ent->worldSector;
	if ( !ws ) {
		return;		// not linked in anywhere
	}
	ent->worldSector = NULL;

	if ( ws->entities == ent ) {
		ws->entities = ent->nextEntityInWorldSector;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (!ws->entities) SV_UpdateOccupied(ws);
#endif
		return;
	}

	for ( scan = ws->entities ; scan ; scan = scan->nextEntityInWorldSector ) {
		if ( scan->nextEntityInWorldSector == ent ) {
			scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
			return;
		}
	}

	Com_Printf( "WARNING: SV_UnlinkEntity: not found in worldSector\n" );
}


/*
===============
SV_LinkEntity

===============
*/
#define MAX_TOTAL_ENT_LEAFS		128
void SV_LinkEntity( gentity_t *gEnt ) {
	worldSector_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			cluster;
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			lastLeaf;
	float		*origin, *angles;
	svEntity_t	*ent;
#ifdef _XBOX
	static int	s_xboxLinkLogCount = 0;
	static int	s_stefxActorLinkBudget = 384;
	static int	s_stefxTriggerLinkBudget = 96;
	qboolean	xboxLogThisLink = (s_xboxLinkLogCount < 24);
	qboolean	stefxActorLink = (s_stefxActorLinkBudget > 0 && gEnt &&
		( gEnt->client || ( gEnt->svFlags & SVF_NPC ) || gEnt->s.eType == ET_PLAYER ));
	qboolean	stefxTriggerLink = (s_stefxTriggerLinkBudget > 0 && gEnt &&
		(gEnt->contents & CONTENTS_TRIGGER));

	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity enter ent=%p num=%d inuse=%d contents=0x%x origin=(%g,%g,%g) mins=(%g,%g,%g) maxs=(%g,%g,%g)",
			gEnt,
			gEnt ? gEnt->s.number : -1,
			gEnt ? gEnt->inuse : 0,
			gEnt ? gEnt->contents : 0,
			gEnt ? gEnt->currentOrigin[0] : 0, gEnt ? gEnt->currentOrigin[1] : 0, gEnt ? gEnt->currentOrigin[2] : 0,
			gEnt ? gEnt->mins[0] : 0, gEnt ? gEnt->mins[1] : 0, gEnt ? gEnt->mins[2] : 0,
			gEnt ? gEnt->maxs[0] : 0, gEnt ? gEnt->maxs[1] : 0, gEnt ? gEnt->maxs[2] : 0);
		s_xboxLinkLogCount++;
	}
	if (stefxActorLink)
	{
		XBLog_Write("STEFX: SV_ACTOR_LINK enter");
		--s_stefxActorLinkBudget;
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK enter ent=%d contents=0x%x bmodel=%d modelindex=%d origin=(%g,%g,%g) mins=(%g,%g,%g) maxs=(%g,%g,%g)",
			gEnt->s.number,
			gEnt->contents,
			gEnt->bmodel,
			gEnt->s.modelindex,
			gEnt->currentOrigin[0], gEnt->currentOrigin[1], gEnt->currentOrigin[2],
			gEnt->mins[0], gEnt->mins[1], gEnt->mins[2],
			gEnt->maxs[0], gEnt->maxs[1], gEnt->maxs[2]);
		--s_stefxTriggerLinkBudget;
	}
#endif

	// this should never be called with a freed entity
	if ( !gEnt->inuse ) {
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity skipped not inuse");
		}
		if (stefxActorLink)
		{
			XBLF("STEFX: SV_ACTOR_LINK reject-not-inuse ent=%d", gEnt ? gEnt->s.number : -1);
			--s_stefxActorLinkBudget;
		}
		if (stefxTriggerLink)
		{
			XBLF("STEFX: SV_TRIGGER_LINK reject-not-inuse ent=%d", gEnt ? gEnt->s.number : -1);
			--s_stefxTriggerLinkBudget;
		}
#endif
		return;
	}

	ent = SV_SvEntityForGentity( gEnt );
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity svEntity=%p worldSector=%p", ent, ent ? ent->worldSector : NULL);
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK svEntity=%p worldSector=%p", ent, ent ? ent->worldSector : NULL);
		--s_stefxTriggerLinkBudget;
	}
#endif

	if ( ent->worldSector ) {
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity before unlink old worldSector");
		}
		if (stefxTriggerLink)
		{
			XBLog_Write("STEFX: SV_TRIGGER_LINK before unlink old worldSector");
			--s_stefxTriggerLinkBudget;
		}
#endif
		SV_UnlinkEntity( gEnt );	// unlink from old position
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity after unlink old worldSector");
		}
		if (stefxTriggerLink)
		{
			XBLog_Write("STEFX: SV_TRIGGER_LINK after unlink old worldSector");
			--s_stefxTriggerLinkBudget;
		}
#endif
	}

	// encode the size into the entityState_t for client prediction
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity before solid encode bmodel=%d contents=0x%x", gEnt->bmodel, gEnt->contents);
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK before solid encode bmodel=%d contents=0x%x", gEnt->bmodel, gEnt->contents);
		--s_stefxTriggerLinkBudget;
	}
#endif
	if ( gEnt->bmodel ) {
		gEnt->s.solid = SOLID_BMODEL;		// a solid_box will never create this value
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLF("STEFX: SV_LinkEntity solid bmodel value=0x%x", gEnt->s.solid);
		}
#endif
	} else if ( gEnt->contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) {
		// assume that x/y are equal and symetric
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity before solid i");
		}
#endif
#ifdef _XBOX
		i = STEFX_TruncFloatToInt(gEnt->maxs[0]);
#else
		i = gEnt->maxs[0];
#endif
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLF("STEFX: SV_LinkEntity after solid i=%d", i);
		}
#endif
		if (i<1)
			i = 1;
		if (i>255)
			i = 255;

		// z is not symetric
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity before solid j");
		}
#endif
#ifdef _XBOX
		j = STEFX_TruncFloatToInt(-gEnt->mins[2]);
#else
		j = (-gEnt->mins[2]);
#endif
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLF("STEFX: SV_LinkEntity after solid j=%d", j);
		}
#endif
		if (j<1)
			j = 1;
		if (j>255)
			j = 255;

		// and z maxs can be negative...
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity before solid k");
		}
#endif
#ifdef _XBOX
		k = STEFX_TruncFloatToInt(gEnt->maxs[2] + 32);
#else
		k = (gEnt->maxs[2]+32);
#endif
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLF("STEFX: SV_LinkEntity after solid k=%d", k);
		}
#endif
		if (k<1)
			k = 1;
		if (k>255)
			k = 255;

		gEnt->s.solid = (k<<16) | (j<<8) | i;
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLF("STEFX: SV_LinkEntity solid bbox value=0x%x", gEnt->s.solid);
		}
#endif
	} else {
		gEnt->s.solid = 0;
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity solid zero");
		}
#endif
	}

	// get the position
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLog_Write("STEFX: SV_LinkEntity before origin pointers");
	}
#endif
	origin = gEnt->currentOrigin;
	angles = gEnt->currentAngles;
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity after origin pointers origin=%p angles=%p", origin, angles);
	}
#endif

	// set the abs box
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity before absbox bmodel=%d angles=(%g,%g,%g)",
			gEnt->bmodel, angles[0], angles[1], angles[2]);
	}
#endif
	if ( gEnt->bmodel && (angles[0] || angles[1] || angles[2]) )
	{	// expand for rotation
		float		max;
		int			i;

		max = RadiusFromBounds( gEnt->mins, gEnt->maxs );
		for (i=0 ; i<3 ; i++) {
			gEnt->absmin[i] = origin[i] - max;
			gEnt->absmax[i] = origin[i] + max;
		}
	} else {
		// normal
#ifdef _XBOX
		gEnt->absmin[0] = origin[0] + gEnt->mins[0];
		gEnt->absmin[1] = origin[1] + gEnt->mins[1];
		gEnt->absmin[2] = origin[2] + gEnt->mins[2];
		gEnt->absmax[0] = origin[0] + gEnt->maxs[0];
		gEnt->absmax[1] = origin[1] + gEnt->maxs[1];
		gEnt->absmax[2] = origin[2] + gEnt->maxs[2];
#else
		VectorAdd (origin, gEnt->mins, gEnt->absmin);	
		VectorAdd (origin, gEnt->maxs, gEnt->absmax);
#endif
	}
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity after absbox raw absmin=(%g,%g,%g) absmax=(%g,%g,%g)",
			gEnt->absmin[0], gEnt->absmin[1], gEnt->absmin[2],
			gEnt->absmax[0], gEnt->absmax[1], gEnt->absmax[2]);
	}
#endif

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	gEnt->absmin[0] -= 1;
	gEnt->absmin[1] -= 1;
	gEnt->absmin[2] -= 1;
	gEnt->absmax[0] += 1;
	gEnt->absmax[1] += 1;
	gEnt->absmax[2] += 1;
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity after absbox expanded absmin=(%g,%g,%g) absmax=(%g,%g,%g)",
			gEnt->absmin[0], gEnt->absmin[1], gEnt->absmin[2],
			gEnt->absmax[0], gEnt->absmax[1], gEnt->absmax[2]);
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK after absbox expanded absmin=(%g,%g,%g) absmax=(%g,%g,%g)",
			gEnt->absmin[0], gEnt->absmin[1], gEnt->absmin[2],
			gEnt->absmax[0], gEnt->absmax[1], gEnt->absmax[2]);
		--s_stefxTriggerLinkBudget;
	}
	if (STEFX_SVBoundsBad(gEnt->absmin, gEnt->absmax))
	{
		XBLF("STEFX: SV_LinkEntity bad absbox ent=%d bmodel=%d contents=0x%x; leaving unlinked",
			gEnt ? gEnt->s.number : -1, gEnt ? gEnt->bmodel : 0, gEnt ? gEnt->contents : 0);
		return;
	}
#endif

	// link to PVS leafs
	ent->numClusters = 0;
	ent->lastCluster = 0;
	ent->areanum = -1;
	ent->areanum2 = -1;

	//get all leafs, including solids
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLog_Write("STEFX: SV_LinkEntity before CM_BoxLeafnums");
	}
	if (stefxTriggerLink)
	{
		XBLog_Write("STEFX: SV_TRIGGER_LINK before CM_BoxLeafnums");
		--s_stefxTriggerLinkBudget;
	}
#endif
	num_leafs = CM_BoxLeafnums( gEnt->absmin, gEnt->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &lastLeaf );
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity leafs=%d lastLeaf=%d absmin=(%g,%g,%g) absmax=(%g,%g,%g)",
			num_leafs, lastLeaf,
			gEnt->absmin[0], gEnt->absmin[1], gEnt->absmin[2],
			gEnt->absmax[0], gEnt->absmax[1], gEnt->absmax[2]);
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK leafs=%d lastLeaf=%d", num_leafs, lastLeaf);
		--s_stefxTriggerLinkBudget;
	}
#endif

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if ( !num_leafs ) {
#ifdef _XBOX
		if (xboxLogThisLink)
		{
			XBLog_Write("STEFX: SV_LinkEntity no leafs");
		}
		if (stefxActorLink)
		{
			XBLog_Write("STEFX: SV_ACTOR_LINK no-leafs");
			--s_stefxActorLinkBudget;
		}
		if (stefxTriggerLink)
		{
			XBLog_Write("STEFX: SV_TRIGGER_LINK no-leafs");
			--s_stefxTriggerLinkBudget;
		}
#endif
		return;
	}

	// set areas, even from clusters that don't fit in the entity array
	for (i=0 ; i<num_leafs ; i++) {
		area = CM_LeafArea (leafs[i]);
		if (area != -1)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum != -1 && ent->areanum != area) {
				if (ent->areanum2 != -1 && ent->areanum2 != area && sv.state == SS_LOADING) {
					Com_DPrintf ("Object %i touching 3 areas at %f %f %f\n",
					gEnt->s.number,
					gEnt->absmin[0], gEnt->absmin[1], gEnt->absmin[2]);
				}
				ent->areanum2 = area;
			} else {
				ent->areanum = area;
			}
		}
	}

	// store as many explicit clusters as we can
	ent->numClusters = 0;
	for (i=0 ; i < num_leafs ; i++) {
		cluster = CM_LeafCluster( leafs[i] );
		if ( cluster != -1 ) {
			ent->clusternums[ent->numClusters++] = cluster;
			if ( ent->numClusters == MAX_ENT_CLUSTERS ) {
					break;
			}
		}
	}

	// store off a last cluster if we need to
	if ( i != num_leafs ) {
		ent->lastCluster = CM_LeafCluster( lastLeaf );
	}

	// find the first world sector node that the ent's box crosses
	node = sv_worldSectors;
	while (1)
	{
		if (node->axis == -1)
			break;
		if ( gEnt->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if ( gEnt->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	
	// link it in
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (node->entities && !STEFX_IsValidSvEntityPtr(node->entities))
	{
		XBLF("STEFX: SV_LinkEntity bad sector head node=%p ent=%d head=%p; clearing head",
			node, gEnt ? gEnt->s.number : -1, node->entities);
		node->entities = NULL;
	}
#endif
	ent->worldSector = node;
	ent->nextEntityInWorldSector = node->entities;
	node->entities = ent;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (!ent->nextEntityInWorldSector) SV_UpdateOccupied(node);
#endif

	gEnt->linked = qtrue;
#ifdef _XBOX
	if (xboxLogThisLink)
	{
		XBLF("STEFX: SV_LinkEntity done ent=%d linked=%d clusters=%d area=%d area2=%d",
			gEnt->s.number, gEnt->linked, ent->numClusters, ent->areanum, ent->areanum2);
	}
	if (stefxActorLink)
	{
		XBLog_Write("STEFX: SV_ACTOR_LINK done");
		--s_stefxActorLinkBudget;
	}
	if (stefxTriggerLink)
	{
		XBLF("STEFX: SV_TRIGGER_LINK done ent=%d linked=%d clusters=%d area=%d area2=%d",
			gEnt->s.number, gEnt->linked, ent->numClusters, ent->areanum, ent->areanum2);
		--s_stefxTriggerLinkBudget;
	}
#endif
}

/*
============================================================================

AREA QUERY

Fills in a list of all entities who's absmin / absmax intersects the given
bounds.  This does NOT mean that they actually touch in the case of bmodels.
============================================================================
*/

typedef struct {
	const float	*mins;
	const float	*maxs;
	gentity_t	**list;
	int			count, maxcount;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	bool traceEntities;
#endif
} areaParms_t;


/*
====================
SV_AreaEntities_r

====================
*/
void SV_AreaEntities_r( worldSector_t *node, areaParms_t *ap ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
    if (!ap->traceEntities && s_indexSectors && !s_occupied[node-sv_worldSectors]) {
        ++g_SPXBSplitSectorSkips;return;
    }
#endif
	svEntity_t	*check, *next;
	gentity_t	*gcheck;
	int			count;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_areaLogBudget = 64;
	qboolean logThisArea = (s_areaLogBudget > 0);

	if (logThisArea)
	{
		STEFX_SV_TRACE_STAGE(0x41524541, 1); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, node ? (unsigned int)node->axis : 0xffffffff, ap ? (unsigned int)ap->count : 0, ap ? (unsigned int)ap->maxcount : 0);
		XBLF("STEFX: SV_AreaEntities_r enter node=%p axis=%d count=%d max=%d", node, node ? node->axis : -99, ap ? ap->count : -1, ap ? ap->maxcount : -1);
		--s_areaLogBudget;
	}
#endif

	count = 0;

	for ( check = node->entities  ; check ; check = next ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 20); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, (unsigned int)check, 0, 0);
		}
		if (!STEFX_IsValidSvEntityPtr(check))
		{
			XBLF("STEFX: SV_AreaEntities_r invalid check node=%p sv=%p count=%d", node, check, count);
			break;
		}
#endif
		next = check->nextEntityInWorldSector;
		count++;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 21); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, (unsigned int)check, (unsigned int)next, 0);
		}
		if (next && !STEFX_IsValidSvEntityPtr(next))
		{
			XBLF("STEFX: SV_AreaEntities_r invalid next node=%p sv=%p next=%p count=%d", node, check, next, count);
			next = NULL;
		}
		if (next == check)
		{
			XBLF("STEFX: SV_AreaEntities_r self-cycle guard node=%p sv=%p count=%d", node, check, count);
			next = NULL;
		}
		else if (count > MAX_GENTITIES)
		{
			XBLF("STEFX: SV_AreaEntities_r overflow guard node=%p sv=%p next=%p count=%d", node, check, next, count);
			next = NULL;
		}
#endif

		gcheck = SV_GEntityForSvEntity( check );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 22); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, (unsigned int)gcheck, 0, 0);
		}
		if (!STEFX_IsValidGEntityPtr(gcheck))
		{
			XBLF("STEFX: SV_AreaEntities_r invalid gentity node=%p sv=%p g=%p count=%d", node, check, gcheck, count);
			continue;
		}
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 27); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, (unsigned int)gcheck->s.number, (unsigned int)gcheck->linked, (unsigned int)gcheck->contents);
		}
		if (STEFX_SVBoundsBad(gcheck->absmin, gcheck->absmax))
		{
			XBLF("STEFX: SV_AreaEntities_r bad bounds ent=%d linked=%d contents=0x%x count=%d",
				gcheck ? gcheck->s.number : -1, gcheck ? gcheck->linked : 0, gcheck ? gcheck->contents : 0, count);
			STEFX_SV_TRACE_STAGE(0x41524541, 28); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)next, (unsigned int)ap->count);
			continue;
		}
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 26); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)ap->count, 0);
		}
#endif

		if ( gcheck->absmin[0] > ap->maxs[0]
		|| gcheck->absmin[1] > ap->maxs[1]
		|| gcheck->absmin[2] > ap->maxs[2]
		|| gcheck->absmax[0] < ap->mins[0]
		|| gcheck->absmax[1] < ap->mins[1]
		|| gcheck->absmax[2] < ap->mins[2]) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (ap->traceEntities) {
				STEFX_SV_TRACE_STAGE(0x41524541, 23); /* AREA */
				STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)next, (unsigned int)ap->count);
				STEFX_SV_TRACE_STAGE(0x41524541, 29); /* AREA */
				STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)next, (unsigned int)ap->count);
			}
#endif
			continue;
		}

		if ( ap->count == ap->maxcount ) {
			Com_DPrintf ("SV_AreaEntities: reached maxcount (%d)\n",ap->maxcount);
			return;
		}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 24); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)ap->count, (unsigned int)ap->maxcount);
		}
#endif
		ap->list[ap->count] = gcheck;
		ap->count++;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ap->traceEntities) {
			STEFX_SV_TRACE_STAGE(0x41524541, 25); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)count, gcheck ? (unsigned int)gcheck->s.number : 0xffffffff, (unsigned int)ap->count, 0);
		}
#endif
	}
	
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SV_TRACE_STAGE(0x41524541, 30); /* AREA */
	STEFX_SV_TRACE_DETAIL((unsigned int)node, node ? (unsigned int)node->axis : 0xffffffff, (unsigned int)count, ap ? (unsigned int)ap->count : 0);
#endif
	if (node->axis == -1) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_SV_TRACE_STAGE(0x41524541, 31); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)count, (unsigned int)ap->count, 0);
		if (logThisArea)
		{
			STEFX_SV_TRACE_STAGE(0x41524541, 3); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)count, (unsigned int)ap->count, 0);
		}
#endif
		return;		// terminal node
	}

	// recurse down both sides
	if ( ap->maxs[node->axis] > node->dist ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_SV_TRACE_STAGE(0x41524541, 32); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, (unsigned int)node->children[0]);
		if (logThisArea)
		{
			STEFX_SV_TRACE_STAGE(0x41524541, 4); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, 0);
		}
#endif
		SV_AreaEntities_r ( node->children[0], ap );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_SV_TRACE_STAGE(0x41524541, 34); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, (unsigned int)node->children[0]);
#endif
	}
	if ( ap->mins[node->axis] < node->dist ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_SV_TRACE_STAGE(0x41524541, 33); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, (unsigned int)node->children[1]);
		if (logThisArea)
		{
			STEFX_SV_TRACE_STAGE(0x41524541, 5); /* AREA */
			STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, 0);
		}
#endif
		SV_AreaEntities_r ( node->children[1], ap );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_SV_TRACE_STAGE(0x41524541, 35); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)node, (unsigned int)node->axis, (unsigned int)ap->count, (unsigned int)node->children[1]);
#endif
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SV_TRACE_STAGE(0x41524541, 36); /* AREA */
	STEFX_SV_TRACE_DETAIL((unsigned int)node, node ? (unsigned int)node->axis : 0xffffffff, (unsigned int)ap->count, 0);
#endif
}

/*
================
SV_AreaEntities
================
*/
int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, gentity_t **elist, int maxcount ) {
	areaParms_t		ap;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_areaCallLogBudget = 32;
	qboolean logThisAreaCall = (s_areaCallLogBudget > 0);
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	ap.traceEntities = true;
#else
	// A collision query may visit the same sector for many actors per tick.
	// Retain its entry, exit and invalid-data diagnostics. Detailed builds
	// and single player also publish each candidate's intermediate state.
	static cvar_t *split = Cvar_Get("stefx_splitScreen", "0", 0);
	static cvar_t *players = Cvar_Get("stefx_splitScreenPlayers", "1", 0);
	ap.traceEntities = !split->integer || players->integer < 2;
    if (!ap.traceEntities && !s_indexSectors && sv_numworldSectors>0) {
        SV_IndexSector(sv_worldSectors,-1);s_indexSectors=true;
        XBLog_WriteCritical("STEFX_SPLIT_SECTORS: skipping empty collision subtrees");
    } else if (ap.traceEntities) s_indexSectors=false;
	if (!ap.traceEntities) {
		static bool logged;
		if (!logged) {
			XBLog_WriteCriticalf("STEFX_SPLIT_COLLISION: sector query per-entity probes disabled");
			logged = true;
		}
	}
#endif

	if (logThisAreaCall)
	{
		STEFX_SV_TRACE_STAGE(0x41524541, 0); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)maxcount, 0, 0, 0);
		XBLF("STEFX: SV_AreaEntities enter max=%d mins=(%g,%g,%g) maxs=(%g,%g,%g)",
			maxcount, mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
		--s_areaCallLogBudget;
	}
#endif

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = elist;
	ap.count = 0;
	ap.maxcount = maxcount;

#if SV_TRACE_PROFILE
#if MEM_DEBUG
	{
		int old=dbgMemSetCheckpoint(2003);
		malloc(1);
		dbgMemSetCheckpoint(old);
	}
#endif
#endif
	SV_AreaEntities_r( sv_worldSectors, &ap );

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisAreaCall)
	{
		STEFX_SV_TRACE_STAGE(0x41524541, 6); /* AREA */
		STEFX_SV_TRACE_DETAIL((unsigned int)ap.count, (unsigned int)maxcount, 0, 0);
		XBLF("STEFX: SV_AreaEntities done count=%d max=%d", ap.count, maxcount);
	}
#endif
	return ap.count;
}

/*
===============
SV_SectorList_f
===============
*/
#if 1

void SV_SectorList_f( void ) {
	int				i, c;
	worldSector_t	*sec;
	svEntity_t		*ent;

	for ( i = 0 ; i < AREA_NODES ; i++ ) {
		sec = &sv_worldSectors[i];

		c = 0;
		for ( ent = sec->entities ; ent ; ent = ent->nextEntityInWorldSector ) {
			c++;
		}
		Com_Printf( "sector %i: %i entities\n", i, c );
	}
}

#else

#pragma warning (push, 3)	//go back down to 3 for the stl include
#include <list>
#include <map>
#pragma warning (pop)
using namespace std;

class CBBox
{
public:
	float mMins[3];
	float mMaxs[3];

	CBBox(vec3_t mins,vec3_t maxs)
	{
		VectorCopy(mins,mMins);
		VectorCopy(maxs,mMaxs);
	}
};

static multimap<int,pair<int,list<CBBox> > > entStats;

void SV_AreaEntitiesTree( worldSector_t *node, areaParms_t *ap, int level )
{
	svEntity_t		*check, *next;
	gentity_t		*gcheck;
	int				count;
	list<CBBox>	bblist;

	count = 0;

	for ( check = node->entities  ; check ; check = next )
	{
		next = check->nextEntityInWorldSector;

		gcheck = SV_GEntityForSvEntity( check );

		CBBox bBox(gcheck->absmin,gcheck->absmax);
		bblist.push_back(bBox);
		count++;
	}

	entStats.insert(pair<int,pair<int,list<CBBox> > >(level,pair<int,list<CBBox> >(count,bblist)));
	if (node->axis == -1)
	{
		return;		// terminal node
	}

	// recurse down both sides
	SV_AreaEntitiesTree ( node->children[0], ap, level+1 );
	SV_AreaEntitiesTree ( node->children[1], ap, level+1 );
}

void SV_SectorList_f( void )
{
	areaParms_t		ap;

//	ap.mins = mins;
//	ap.maxs = maxs;
//	ap.list = list;
//	ap.count = 0;
//	ap.maxcount = maxcount;

	entStats.clear();
	SV_AreaEntitiesTree(sv_worldSectors,&ap,0);
	char mess[1000];
	multimap<int,pair<int,list<CBBox> > >::iterator j;
	for(j=entStats.begin();j!=entStats.end();j++)
	{
		sprintf(mess,"**************************************************\n");
		Sleep(5);
		OutputDebugString(mess);
		sprintf(mess,"level=%i, count=%i\n",(*j).first,(*j).second.first);
		Sleep(5);
		OutputDebugString(mess);
		list<CBBox>::iterator k;
		for(k=(*j).second.second.begin();k!=(*j).second.second.end();k++)
		{
			sprintf(mess,"mins=%f %f %f, maxs=%f %f %f\n",
					(*k).mMins[0],(*k).mMins[1],(*k).mMins[2],(*k).mMaxs[0],(*k).mMaxs[1],(*k).mMaxs[2]);
			OutputDebugString(mess);
		}
	}

}
#endif

//===========================================================================


typedef struct {
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	const float	*mins;
	const float *maxs;	// size of the moving object
/*
Ghoul2 Insert Start
*/
	vec3_t		start;
/*
Ghoul2 Insert End
*/
	vec3_t		end;
	int			passEntityNum;
	int			contentmask;
/*
Ghoul2 Insert Start
*/
	EG2_Collision	eG2TraceType;
	int			useLod;
	trace_t		trace;			// make sure nothing goes under here for Ghoul2 collision purposes
/*
Ghoul2 Insert End
*/
} moveclip_t;


/*
====================
SV_ClipMoveToEntities

====================
*/
void SV_ClipMoveToEntities( moveclip_t *clip ) {
	int			i, num;
	gentity_t		*touchlist[MAX_GENTITIES], *touch, *owner, *touchOwner;
	trace_t		trace, oldTrace;
	clipHandle_t	clipHandle;
	const float		*origin, *angles;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_clipLogCount = 0;
	static int s_clipBadEntityLogCount = 0;
	qboolean logThisClip = qtrue;
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisClip)
	{
		STEFX_SV_TRACE_STAGE(0x434C4950, 1); /* CLIP */
		STEFX_SV_TRACE_DETAIL(0, (unsigned int)clip->contentmask, (unsigned int)clip->passEntityNum, 0);
	}
#endif
	num = SV_AreaEntities( clip->boxmins, clip->boxmaxs, touchlist, MAX_GENTITIES);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisClip)
	{
		STEFX_SV_TRACE_STAGE(0x434C4950, 2); /* CLIP */
		STEFX_SV_TRACE_DETAIL(0, (unsigned int)num, (unsigned int)clip->passEntityNum, (unsigned int)clip->contentmask);
	}
#endif

	if ( clip->passEntityNum != ENTITYNUM_NONE ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		gentity_t *passEnt = NULL;
		if (logThisClip)
		{
			STEFX_SV_TRACE_STAGE(0x434C4950, 20); /* CLIP */
			STEFX_SV_TRACE_DETAIL((unsigned int)num, (unsigned int)clip->passEntityNum,
				ge ? (unsigned int)ge->gentitySize : 0, ge ? (unsigned int)ge->gentities : 0);
		}
		if ( clip->passEntityNum < 0 || clip->passEntityNum >= MAX_GENTITIES || !ge || !ge->gentities || ge->gentitySize <= 0 )
		{
			if (s_clipBadEntityLogCount < 64)
			{
				XBLF("STEFX: SV_ClipMoveToEntities invalid passEntityNum=%d ge=%p gentities=%p size=%d num=%d",
					clip->passEntityNum, ge, ge ? ge->gentities : NULL, ge ? ge->gentitySize : 0, num);
			}
			s_clipBadEntityLogCount++;
			owner = NULL;
		}
		else
		{
			passEnt = SV_GentityNum( clip->passEntityNum );
			if (logThisClip)
			{
				STEFX_SV_TRACE_STAGE(0x434C4950, 21); /* CLIP */
				STEFX_SV_TRACE_DETAIL((unsigned int)num, (unsigned int)clip->passEntityNum, (unsigned int)passEnt, 0);
			}
			if (!STEFX_IsValidGEntityPtr(passEnt))
			{
				if (s_clipBadEntityLogCount < 64)
				{
					XBLF("STEFX: SV_ClipMoveToEntities invalid pass entity pass=%d ptr=%p ge=%p gentities=%p size=%d",
						clip->passEntityNum, passEnt, ge, ge ? ge->gentities : NULL, ge ? ge->gentitySize : 0);
				}
				s_clipBadEntityLogCount++;
				owner = NULL;
			}
			else
			{
				owner = passEnt->owner;
			}
		}
		if (logThisClip)
		{
			STEFX_SV_TRACE_STAGE(0x434C4950, 22); /* CLIP */
			STEFX_SV_TRACE_DETAIL((unsigned int)num, (unsigned int)clip->passEntityNum, (unsigned int)owner, 0);
		}
#else
		owner = ( SV_GentityNum( clip->passEntityNum ) )->owner;
#endif
	} else {
		owner = NULL;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisClip)
	{
		STEFX_SV_TRACE_STAGE(0x434C4950, 23); /* CLIP */
		STEFX_SV_TRACE_DETAIL((unsigned int)num, (unsigned int)clip->passEntityNum, (unsigned int)owner, 0);
	}
#endif

	for ( i=0 ; i<num ; i++ ) {
		if (clip->trace.allsolid) {
			return;
		}
		touch = touchlist[i];
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (!STEFX_IsValidGEntityPtr(touch))
		{
			if (s_clipBadEntityLogCount < 64)
			{
				XBLF("STEFX: SV_ClipMoveToEntities invalid touch i=%d num=%d ptr=%p pass=%d",
					i, num, touch, clip->passEntityNum);
			}
			s_clipBadEntityLogCount++;
			continue;
		}
		if (logThisClip)
		{
			STEFX_SV_TRACE_STAGE(0x434C4950, 3); /* CLIP */
			STEFX_SV_TRACE_DETAIL((unsigned int)i, touch ? (unsigned int)touch->s.number : 0xffffffff, touch ? (unsigned int)touch->contents : 0, (unsigned int)clip->contentmask);
		}
#endif
		touchOwner = touch->owner;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (touchOwner && !STEFX_IsValidGEntityPtr(touchOwner))
		{
			if (s_clipBadEntityLogCount < 64)
			{
				XBLF("STEFX: SV_ClipMoveToEntities invalid touch owner i=%d touch=%d owner=%p pass=%d",
					i, touch->s.number, touchOwner, clip->passEntityNum);
			}
			s_clipBadEntityLogCount++;
			touchOwner = NULL;
		}
#endif

		// see if we should ignore this entity
		if ( clip->passEntityNum != ENTITYNUM_NONE ) {
			if (touch->s.number == clip->passEntityNum) {
				continue; // don't clip against the pass entity
			}
			if (touchOwner && touchOwner->s.number == clip->passEntityNum) {
				continue;	// don't clip against own missiles
			}
			if ( owner == touch) {
				continue;	// don't clip against owner
			}
			if ( owner && touchOwner == owner) {
				continue;	// don't clip against other missiles from our owner
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if ( ! ( clip->contentmask & touch->contents ) ) {
			continue;
		}

		// might intersect, so do an exact clip
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (logThisClip)
		{
			STEFX_SV_TRACE_STAGE(0x434C4950, 4); /* CLIP */
			STEFX_SV_TRACE_DETAIL((unsigned int)i, touch ? (unsigned int)touch->s.number : 0xffffffff, touch ? (unsigned int)touch->contents : 0, (unsigned int)clip->contentmask);
		}
#endif
		clipHandle = SV_ClipHandleForEntity (touch);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (logThisClip)
		{
			STEFX_SV_TRACE_STAGE(0x434C4950, 5); /* CLIP */
			STEFX_SV_TRACE_DETAIL((unsigned int)i, (unsigned int)clipHandle, touch ? (unsigned int)touch->s.number : 0xffffffff, touch ? (unsigned int)touch->contents : 0);
		}
#endif

		origin = touch->currentOrigin;
		angles = touch->currentAngles;


		if ( !touch->bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}

#if 0 //G2_SUPERSIZEDBBOX is not being used
		bool shrinkBox=true;

		if (clip->eG2TraceType != G2_SUPERSIZEDBBOX)
		{
			shrinkBox=false;
		}
		else if (trace.entityNum == touch->s.number&&touch->ghoul2.size()&&!(touch->contents & CONTENTS_LIGHTSABER))
		{
			shrinkBox=false;
		}
		if (shrinkBox)
		{
			vec3_t sh_mins;
			vec3_t sh_maxs;
			int j;
			for ( j=0 ; j<3 ; j++ ) 
			{
					sh_mins[j]=clip->mins[j]+superSizedAdd;
					sh_maxs[j]=clip->maxs[j]-superSizedAdd;
			}
			CM_TransformedBoxTrace ( &trace, clip->start, clip->end,
				sh_mins, sh_maxs, clipHandle,  clip->contentmask,
				origin, angles);
		}
		else
#endif
		{
#ifdef __MACOS__
			// compiler bug with const
			CM_TransformedBoxTrace ( &trace, (float *)clip->start, (float *)clip->end,
				(float *)clip->mins, (float *)clip->maxs, clipHandle,  clip->contentmask,
				origin, angles);
#else
			CM_TransformedBoxTrace ( &trace, clip->start, clip->end,
				clip->mins, clip->maxs, clipHandle,  clip->contentmask,
				origin, angles);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (logThisClip)
			{
				STEFX_SV_TRACE_STAGE(0x434C4950, 6); /* CLIP */
				STEFX_SV_TRACE_DETAIL((unsigned int)i, (unsigned int)clipHandle, (unsigned int)trace.entityNum, (unsigned int)trace.contents);
			}
#endif
		//FIXME: when startsolid in another ent, doesn't return correct entityNum 
		//ALSO: 2 players can be standing next to each other and this function will
		//think they're in each other!!!
		}
		oldTrace = clip->trace;

		if ( trace.allsolid ) 
		{
			if(!clip->trace.allsolid)
			{//We didn't come in here all solid, so set the clip->trace's entityNum
				clip->trace.entityNum = touch->s.number;
			}
			clip->trace.allsolid = qtrue;
			trace.entityNum = touch->s.number;
		} 
		else if ( trace.startsolid ) 
		{
			if(!clip->trace.startsolid)
			{//We didn't come in here starting solid, so set the clip->trace's entityNum
				clip->trace.entityNum = touch->s.number;
			}
			clip->trace.startsolid = qtrue;
			trace.entityNum = touch->s.number;
		}

		if ( trace.fraction < clip->trace.fraction ) 
		{
			qboolean	oldStart;

			// make sure we keep a startsolid from a previous trace
			oldStart = clip->trace.startsolid;

			trace.entityNum = touch->s.number;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		}
/*
Ghoul2 Insert Start
*/

#if !defined(STEFX_ELITE_FORCE_SP)
		// decide if we should do the ghoul2 collision detection right here
		if ((trace.entityNum == touch->s.number) && (clip->eG2TraceType != G2_NOCOLLIDE))
		{
			// do we actually have a ghoul2 model here?
			if (touch->ghoul2.size() && !(touch->contents & CONTENTS_LIGHTSABER))
			{
				int			oldTraceRecSize = 0;
				int			newTraceRecSize = 0;
				int			z;

				// we have to do this because sometimes you may hit a model's bounding box, but not actually penetrate the Ghoul2 Models polygons
				// this is, needless to say, not good. So we must check to see if we did actually hit the model, and if not, reset the trace stuff
				// to what it was to begin with

				// set our trace record size
				for (z=0;z<MAX_G2_COLLISIONS;z++)
				{
					if (clip->trace.G2CollisionMap[z].mEntityNum != -1)
					{
						oldTraceRecSize++;
					}
				}

				// if we are looking at an entity then use the player state to get it's angles and origin from
				float radius;
#if 0 //G2_SUPERSIZEDBBOX is not being used
				if (clip->eG2TraceType == G2_SUPERSIZEDBBOX)
				{
					radius=(clip->maxs[0]-clip->mins[0]-2.0f*superSizedAdd)/2.0f;
				}
				else
#endif
				{
					radius=(clip->maxs[0]-clip->mins[0])/2.0f;
				}
				if (touch->client)
				{
					vec3_t world_angles;
					
					world_angles[PITCH] =  0;
					//legs do not *always* point toward the viewangles!
					//world_angles[YAW] =  touch->client->viewangles[YAW];
					world_angles[YAW] =  touch->client->legsYaw;
					world_angles[ROLL] =  0;

					G2API_CollisionDetect(clip->trace.G2CollisionMap, touch->ghoul2,
							world_angles, touch->client->origin, sv.time, touch->s.number, clip->start, clip->end, touch->s.modelScale, G2VertSpaceServer, clip->eG2TraceType, clip->useLod,radius);
				}
				// no, so use the normal entity state
				else
				{
					//use the correct origin and angles!  is this right now?
					G2API_CollisionDetect(clip->trace.G2CollisionMap, touch->ghoul2,
						touch->currentAngles, touch->currentOrigin, sv.time, touch->s.number, clip->start, clip->end, touch->s.modelScale, G2VertSpaceServer, clip->eG2TraceType, clip->useLod,radius);
				}

				// set our new trace record size
 
				for (z=0;z<MAX_G2_COLLISIONS;z++)
				{
					if (clip->trace.G2CollisionMap[z].mEntityNum != -1)
					{
						newTraceRecSize++;
					}
				}

				// did we actually touch this model? If not, lets reset this ent as being hit..
				if (newTraceRecSize == oldTraceRecSize)
				{
					clip->trace = oldTrace;
				}
				else//this trace was valid, so copy the best collision into quake trace place info
				{
					for (z=0;z<MAX_G2_COLLISIONS;z++)
					{
						if (clip->trace.G2CollisionMap[z].mEntityNum==touch->s.number)
						{
							clip->trace.plane.normal[0] = clip->trace.G2CollisionMap[z].mCollisionNormal[0];
							clip->trace.plane.normal[1] = clip->trace.G2CollisionMap[z].mCollisionNormal[1];
							clip->trace.plane.normal[2] = clip->trace.G2CollisionMap[z].mCollisionNormal[2];
							break;
						}
					}
					assert(z<MAX_G2_COLLISIONS); // hmm well ah, weird
					assert(VectorLength(clip->trace.plane.normal)>0.1f);
		}
	}
#endif
#if !defined(STEFX_ELITE_FORCE_SP)
}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisClip)
	{
		STEFX_SV_TRACE_STAGE(0x434C4950, 7); /* CLIP */
	}
	s_clipLogCount++;
#endif
/*
Ghoul2 Insert End
*/

	}
}


/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.
passEntityNum and entities owned by passEntityNum are explicitly not checked.
==================
*/
/*
Ghoul2 Insert Start
*/
void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const int passEntityNum, const int contentmask, const EG2_Collision eG2TraceType, const int useLod ) {
/*
Ghoul2 Insert End
*/
#ifdef _DEBUG
	assert( !_isnan(start[0])&&!_isnan(start[1])&&!_isnan(start[2])&&!_isnan(end[0])&&!_isnan(end[1])&&!_isnan(end[2]));
#endif// _DEBUG

#if SV_TRACE_PROFILE
#if MEM_DEBUG
	{
		int old=dbgMemSetCheckpoint(2002);
		malloc(1);
		dbgMemSetCheckpoint(old);
	}
#endif
#endif

	moveclip_t	clip;
	int			i;
//	int			startMS, endMS;
	float		world_frac;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_svTraceLogCount = 0;
	qboolean logThisTrace = qtrue;
	if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 1); /* TRCE */
		STEFX_SV_TRACE_DETAIL(0, (unsigned int)contentmask, (unsigned int)passEntityNum, (unsigned int)eG2TraceType);
	}
#endif

	/*
	startMS = Sys_Milliseconds ();
	numTraces++;
	*/
	if ( !mins ) {
		mins = vec3_origin;
	}
	if ( !maxs ) {
		maxs = vec3_origin;
	}

#if defined(STEFX_ELITE_FORCE_SP)
	memset ( &clip, 0, sizeof ( moveclip_t ) );
#else
	memset ( &clip, 0, sizeof ( moveclip_t ) - sizeof(clip.trace.G2CollisionMap ));
#endif

	// clip to world
	//NOTE: this will stop not only on static architecture but also entity brushes such as
	//doors, etc.  This prevents us from being able to shorten the trace so that we can
	//ignore all ents past this endpoint... perhaps need to check the entityNum in this
	//BoxTrace or have it not clip against entity brushes here.
	CM_BoxTrace( &clip.trace, start, end, mins, maxs, 0, contentmask );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 2); /* TRCE */
	}
#endif
	clip.trace.entityNum = clip.trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	if ( clip.trace.fraction == 0 ) 
	{// blocked immediately by the world
		*results = clip.trace;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (logThisTrace)
		{
			STEFX_SV_TRACE_STAGE(0x54524345, 6); /* TRCE */
			s_svTraceLogCount++;
		}
#endif
//		goto addtime;		
		return;
	}

	clip.contentmask = contentmask;
/*
Ghoul2 Insert Start
*/	
	VectorCopy( start, clip.start );
	clip.eG2TraceType = eG2TraceType;
	clip.useLod = useLod;
/*
Ghoul2 Insert End
*/
	//Shorten the trace to the size of the trace until it hit the world
	VectorCopy( clip.trace.endpos, clip.end );
	//remember the current completion fraction
	world_frac = clip.trace.fraction;
	//set the fraction back to 1.0 for the trace vs. entities
	clip.trace.fraction = 1.0f;
	
	//VectorCopy( end, clip.end );
	// create the bounding box of the entire move
	// we can limit it to the part of the move not
	// already clipped off by the world, which can be
	// a significant savings for line of sight and shot traces
	clip.passEntityNum = passEntityNum;

#if 0 //G2_SUPERSIZEDBBOX is not being used
	vec3_t superMin;
	vec3_t superMax;  // prison, in boscobel

	if (eG2TraceType==G2_SUPERSIZEDBBOX)
	{
		for ( i=0 ; i<3 ; i++ ) 
		{
				superMin[i]=mins[i]-superSizedAdd;
				superMax[i]=maxs[i]+superSizedAdd;
		}
		clip.mins = superMin;
		clip.maxs = superMax;
	}
	else
#endif
	{
		clip.mins = mins;
		clip.maxs = maxs;
	}

	for ( i=0 ; i<3 ; i++ ) {
		if ( end[i] > start[i] ) {
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		} else {
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	// clip to other solid entities
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 3); /* TRCE */
	}
#endif
	if ( clip.contentmask ) {
		SV_ClipMoveToEntities ( &clip );
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	else if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 8); /* TRCE */
	}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 4); /* TRCE */
	}
#endif

	//scale the trace back down by the previous fraction
	clip.trace.fraction *= world_frac;
	*results = clip.trace;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (logThisTrace)
	{
		STEFX_SV_TRACE_STAGE(0x54524345, 5); /* TRCE */
	}
	s_svTraceLogCount++;
#endif

/*
addtime:
	endMS = Sys_Milliseconds ();

	timeInTrace += endMS - startMS;
*/
}



/*
=============
SV_PointContents
=============
*/
int SV_PointContents( const vec3_t p, int passEntityNum ) {
	gentity_t		*touch[MAX_GENTITIES], *hit;
	int			i, num;
	int			contents, c2;
//	int			startMS, endMS;
	clipHandle_t	clipHandle;
	const float		*angles;

#if MEM_DEBUG
#if SV_TRACE_PROFILE
	{
		int old=dbgMemSetCheckpoint(2001);
		malloc(1);
		dbgMemSetCheckpoint(old);
	}
#endif
#endif

	/*
	startMS = Sys_Milliseconds ();
	numTraces++;
	*/

	// get base contents from world
	contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	num = SV_AreaEntities( p, p, touch, MAX_GENTITIES );

	for ( i=0 ; i<num ; i++ ) {
		hit = touch[i];
		if ( hit->s.number == passEntityNum ) {
			continue;
		}
		// might intersect, so do an exact clip
		clipHandle = SV_ClipHandleForEntity( hit );
		angles = hit->s.angles;
		if ( !hit->bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}

		c2 = CM_TransformedPointContents (p, clipHandle, hit->s.origin, hit->s.angles);

		contents |= c2;
	}

	/*
	endMS = Sys_Milliseconds ();
	timeInTrace += endMS - startMS;
	*/
	return contents;
}


