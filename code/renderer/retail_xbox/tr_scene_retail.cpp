//Anything above this #include will be ignored by the compiler
#include "../../server/exe_headers.h"

#include "../tr_local.h"
#include "retail_renderer_contract.h"
#include "../../win32/xb_perf.h"
#include "../../win32/xb_log.h"

#if !defined(G2_H_INC)
	#include "../../ghoul2/G2.h"
#endif
#include "../MatComp.h"

#ifdef _XBOX
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" { volatile unsigned int g_SPXBViewAspectProof[12] = {0}; }
#endif
extern "C" volatile unsigned int g_SPXBSplitSlotActive;
extern "C" volatile unsigned int g_SPXBHMSplitRenderSerial[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderArmedPlayers;
extern "C" volatile unsigned int g_SPXBHMSplitRenderExternal[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderClient[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderRectX[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderRectY[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderRectW[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderRectH[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderViewX[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderViewY[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderViewZ[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderDoneSerial[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderDrawDelta[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderDrawAfter[4];
extern "C" volatile unsigned int g_SPXBHMSplitRenderCluster[4];
#endif

#pragma warning (disable: 4512)	//default assignment operator could not be gened

STEFX_RETAIL_NAMESPACE_BEGIN

static	int			r_firstSceneDrawSurf;

int			r_numdlights;
int			r_firstSceneDlight;

static	int			r_numentities;
static	int			r_firstSceneEntity;
static	int			refEntParent = -1;

static	int			r_numpolys;
static	int			r_firstScenePoly;

static	int			r_numpolyverts;

int					skyboxportal;
int					drawskyboxportal;

/*
====================
R_ToggleSmpFrame

====================
*/
void R_ToggleSmpFrame( void ) {
	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;
	refEntParent = -1;
	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
	refEntParent = -1;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	srfPoly_t	*poly;

	tr.currentEntityNum = TR_WORLDENT;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		STEFX_RETAIL_SCOPE R_AddDrawSurf( (surfaceType_t *)poly, sh, poly->fogIndex, qfalse );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}

	if ( !hShader ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}

	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > MAX_POLYVERTS || r_numpolys >= MAX_POLYS ) {
			Com_Printf (S_COLOR_YELLOW  "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];
		
		memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );

		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex]; 
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}

void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts )
{
	RE_AddPolyToScene( hShader, numVerts, verts, 1 );
}


//=================================================================================


/*
=====================
RE_AddRefEntityToScene

=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent ) {
	if ( !tr.registered ) {
		return;
	}

	assert(!ent || ent->renderfx >= 0);

#ifdef _DEBUG
	if (ent->reType == RT_MODEL)
	{
		assert(ent->hModel || ent->ghoul2 || ent->customShader);
	}
#endif

	if ( r_numentities >= TR_WORLDENT )
	{
#ifndef FINAL_BUILD
		Com_Printf( "WARNING: RE_AddRefEntityToScene: too many entities\n");
#endif
		return;
	}
	if ( ent->reType < 0 || ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		Com_Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
#ifdef _XBOX
	backEndData->entities[r_numentities].visible = -1;
#endif

	if (ent->ghoul2)
	{
		CGhoul2Info_v	&ghoul2 = *((CGhoul2Info_v *)ent->ghoul2);

		if (!ghoul2[0].mModel)
		{
#ifdef _DEBUG
			CGhoul2Info &g2 = ghoul2[0];
#endif
			//DebugBreak();
			Com_Printf("Your ghoul2 instance has no model!\n");
		}
	}

	/*
	if (ent->reType == RT_ENT_CHAIN)
	{
		refEntParent = r_numentities;
		backEndData->entities[r_numentities].e.uRefEnt.uMini.miniStart = r_numminientities - r_firstSceneMiniEntity;
		backEndData->entities[r_numentities].e.uRefEnt.uMini.miniCount = 0;
	}
	else
	{
	*/
		refEntParent = -1;
	//}

	r_numentities++;
}


/************************************************************************************************
 * RE_AddMiniRefEntityToScene                                                                   *
 *    Adds a mini ref ent to the scene.  If the input parameter is null, it signifies the end   *
 *    of the chain.  Otherwise, if there is a valid chain parent, it will be added to that.     *
 *    If there is no parent, it will be added as a regular ref ent.                             *
 *                                                                                              *
 * Input                                                                                        *
 *    ent: the mini ref ent to be added                                                         *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
#if 0 // Jedi Academy mini-entities are not part of the Elite Force renderer ABI.
void RE_AddMiniRefEntityToScene( const miniRefEntity_t *ent ) 
{
#if 0
	refEntity_t		*parent;
#endif

	if ( !tr.registered ) 
	{
		return;
	}
	if (!ent)
	{
		refEntParent = -1;
		return;
	}

#if 1 //i hate you minirefent!
	refEntity_t		tempEnt;

	memcpy(&tempEnt, ent, sizeof(*ent));
	memset(((char *)&tempEnt)+sizeof(*ent), 0, sizeof(tempEnt) - sizeof(*ent));
	RE_AddRefEntityToScene(&tempEnt);
#else

	if ( ent->reType < 0 || ent->reType >= RT_MAX_REF_ENTITY_TYPE ) 
	{
		Com_Error( ERR_DROP, "RE_AddMiniRefEntityToScene: bad reType %i", ent->reType );
	}

	if (!r_numentities || refEntParent == -1 || r_numminientities >= MAX_MINI_ENTITIES)
	{ //rww - add it as a refent also if we run out of minis
//		Com_Error( ERR_DROP, "RE_AddMiniRefEntityToScene: mini without parent ref ent");
		refEntity_t		tempEnt;

		memcpy(&tempEnt, ent, sizeof(*ent));
		memset(((char *)&tempEnt)+sizeof(*ent), 0, sizeof(tempEnt) - sizeof(*ent));
		RE_AddRefEntityToScene(&tempEnt);
		return;
	}

	parent = &backEndData->entities[refEntParent].e;
	parent->uRefEnt.uMini.miniCount++;

	backEndData->miniEntities[r_numminientities].e = *ent;
	r_numminientities++;
#endif
}
#endif

/*
=====================
RE_AddDynamicLightToScene

=====================
*/
void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= MAX_DLIGHTS ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
}

/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}

/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}


#if 0 // Jedi Academy owns this decal pool; Elite Force submits its own scene polys.
enum
{
	DECALPOLY_TYPE_NORMAL,
	DECALPOLY_TYPE_FADE,
	DECALPOLY_TYPE_MAX
};

#define		DECAL_FADE_TIME		1000

decalPoly_t*		RE_AllocDecal		( int type );

static decalPoly_t	re_decalPolys[DECALPOLY_TYPE_MAX][MAX_DECAL_POLYS];

static int			re_decalPolyHead[DECALPOLY_TYPE_MAX];
static int			re_decalPolyTotal[DECALPOLY_TYPE_MAX];

/*
===================
RE_ClearDecals

This is called to remove all decals from the world
===================
*/

void RE_ClearDecals ( void ) 
{
	memset( re_decalPolys, 0, sizeof(re_decalPolys) );
	memset( re_decalPolyHead, 0, sizeof(re_decalPolyHead) );
	memset( re_decalPolyTotal, 0, sizeof(re_decalPolyTotal) );
}

void R_InitDecals ( void )
{
	RE_ClearDecals ( );
}

void RE_FreeDecal ( int type, int index )
{
	if ( !re_decalPolys[type][index].time )
	{
		return;
	}
	
	if ( type == DECALPOLY_TYPE_NORMAL )
	{
		decalPoly_t* fade;

		fade = RE_AllocDecal ( DECALPOLY_TYPE_FADE );

		memcpy ( fade, &re_decalPolys[type][index], sizeof(decalPoly_t) );

		fade->time = tr.refdef.time;
		fade->fadetime = tr.refdef.time + DECAL_FADE_TIME;
	}

	re_decalPolys[type][index].time = 0;

	re_decalPolyTotal[type]--;
}

/*
===================
RE_AllocDecal

Will allways succeed, even if it requires freeing an old active mark
===================
*/
decalPoly_t* RE_AllocDecal( int type ) 
{
	decalPoly_t	*le;
	
	// See if the cvar changed
	if ( re_decalPolyTotal[type] > r_markcount->integer )
	{
		RE_ClearDecals ( );
	}

	le = &re_decalPolys[type][re_decalPolyHead[type]];

	// If it has no time its the first occasion its been used
	if ( le->time )
	{
		if ( le->time != tr.refdef.time ) 
		{
			int i = re_decalPolyHead[type];		

			// since we are killing one that existed before, make sure we 
			// kill all the other marks that belong to the group
			do
			{
				i++;
				if ( i >= r_markcount->integer )
				{
					i = 0;
				}

				// Break out on the first one thats not part of the group
				if ( re_decalPolys[type][i].time != le->time )
				{
					break;
				}

				RE_FreeDecal ( type, i );
			}
			while ( i != re_decalPolyHead[type] );			

			RE_FreeDecal ( type, re_decalPolyHead[type] );
		}
		else
		{
			RE_FreeDecal ( type, re_decalPolyHead[type] );
		}
	}

	memset ( le, 0, sizeof(decalPoly_t) );
	le->time = tr.refdef.time;

	re_decalPolyTotal[type]++;

	// Move on to the next decal poly and wrap around if need be
	re_decalPolyHead[type]++;
	if ( re_decalPolyHead[type] >= r_markcount->integer )
	{
		re_decalPolyHead[type] = 0;
	}

	return le;
}


/*
=================
RE_AddDecalToScene

origin should be a point within a unit of the plane
dir should be the plane normal

temporary marks will not be stored or randomly oriented, but immediately
passed to the renderer.
=================
*/
#define	MAX_DECAL_FRAGMENTS	128
#define	MAX_DECAL_POINTS		384

void RE_AddDecalToScene ( qhandle_t decalShader, const vec3_t origin, const vec3_t dir, float orientation, float red, float green, float blue, float alpha, qboolean alphaFade, float radius, qboolean temporary )
{
	vec3_t			axis[3];
	float			texCoordScale;
	vec3_t			originalPoints[4];
	byte			colors[4];
	int				i, j;
	int				numFragments;
	markFragment_t	markFragments[MAX_DECAL_FRAGMENTS], *mf;
	vec3_t			markPoints[MAX_DECAL_POINTS];
	vec3_t			projection;

	assert(decalShader);

	if ( r_markcount->integer <= 0 && !temporary )
	{
		return;
	}

	if ( radius <= 0 ) 
	{
		Com_Error( ERR_FATAL, "RE_AddDecalToScene:  called with <= 0 radius" );
	}

	// create the texture axis
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orientation );
	CrossProduct( axis[0], axis[2], axis[1] );

	texCoordScale = 0.5 * 1.0 / radius;

	// create the full polygon
	for ( i = 0 ; i < 3 ; i++ ) 
	{
		originalPoints[0][i] = origin[i] - radius * axis[1][i] - radius * axis[2][i];
		originalPoints[1][i] = origin[i] + radius * axis[1][i] - radius * axis[2][i];
		originalPoints[2][i] = origin[i] + radius * axis[1][i] + radius * axis[2][i];
		originalPoints[3][i] = origin[i] - radius * axis[1][i] + radius * axis[2][i];
	}

	// get the fragments
	VectorScale( dir, -20, projection );
	numFragments = R_MarkFragments( 4, (const vec3_t*)originalPoints,
					projection, MAX_DECAL_POINTS, markPoints[0],
					MAX_DECAL_FRAGMENTS, markFragments );

	colors[0] = red * 255;
	colors[1] = green * 255;
	colors[2] = blue * 255;
	colors[3] = alpha * 255;

	for ( i = 0, mf = markFragments ; i < numFragments ; i++, mf++ ) 
	{
		polyVert_t	*v;
		polyVert_t	verts[MAX_VERTS_ON_DECAL_POLY];
		decalPoly_t	*decal;

		// we have an upper limit on the complexity of polygons
		// that we store persistantly
		if ( mf->numPoints > MAX_VERTS_ON_DECAL_POLY ) 
		{
			mf->numPoints = MAX_VERTS_ON_DECAL_POLY;
		}

		for ( j = 0, v = verts ; j < mf->numPoints ; j++, v++ ) 
		{
			vec3_t		delta;

			VectorCopy( markPoints[mf->firstPoint + j], v->xyz );

			VectorSubtract( v->xyz, origin, delta );
			v->st[0] = 0.5 + DotProduct( delta, axis[1] ) * texCoordScale;
			v->st[1] = 0.5 + DotProduct( delta, axis[2] ) * texCoordScale;

			*(int *)v->modulate = *(int *)colors;
		}

		// if it is a temporary (shadow) mark, add it immediately and forget about it
		if ( temporary ) 
		{
			RE_AddPolyToScene( decalShader, mf->numPoints, verts, 1 );
			continue;
		}

		// otherwise save it persistantly
		decal = RE_AllocDecal( DECALPOLY_TYPE_NORMAL );
		decal->time = tr.refdef.time;
		decal->shader = decalShader;
		decal->poly.numVerts = mf->numPoints;
		decal->color[0] = red;
		decal->color[1] = green;
		decal->color[2] = blue;
		decal->color[3] = alpha;
		memcpy( decal->verts, verts, mf->numPoints * sizeof( verts[0] ) );
	}
}

/*
===============
R_AddDecals
===============
*/
static inline void R_AddDecals ( void ) 
{
	int			decalPoly;
	int			type;
	static int  lastMarkCount = -1;

	if ( r_markcount->integer != lastMarkCount )
	{
		if ( lastMarkCount != -1 )
		{
			RE_ClearDecals ( );
		}

		lastMarkCount = r_markcount->integer;
	}

	if ( r_markcount->integer <= 0 )
	{
		return;
	}

	for ( type = DECALPOLY_TYPE_NORMAL; type < DECALPOLY_TYPE_MAX; type ++ )
	{
		decalPoly = re_decalPolyHead[type];

		do
		{
			decalPoly_t* p = &re_decalPolys[type][decalPoly];

			if ( p->time )
			{				
				if ( p->fadetime )
				{
					int t;

					// fade all marks out with time
					t = tr.refdef.time - p->time;
					if ( t < DECAL_FADE_TIME ) 
					{
						float fade;
						int	  j;

						fade = 255.0f * (1.0f - ((float)t / DECAL_FADE_TIME));
						
						for ( j = 0 ; j < p->poly.numVerts ; j++ ) 
						{
							p->verts[j].modulate[3] = fade;
						}

						RE_AddPolyToScene( p->shader, p->poly.numVerts, p->verts, 1 );
					}
					else
					{
						RE_FreeDecal ( type, decalPoly );
					}
				}
				else
				{
					RE_AddPolyToScene( p->shader, p->poly.numVerts, p->verts, 1 );
				}
			}

			decalPoly++;
			if ( decalPoly >= r_markcount->integer )
			{
				decalPoly = 0;
			}
		}
		while ( decalPoly != re_decalPolyHead[type] );
	}
}
#endif


#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#define STEFX_SPLIT_MAX_LOCAL_VIEWS 4
static qboolean s_stefxSplitRefdefValid[STEFX_SPLIT_MAX_LOCAL_VIEWS];
static refdef_t s_stefxSplitRefdef[STEFX_SPLIT_MAX_LOCAL_VIEWS];
static vec3_t s_stefxSplitPvsOrigin[STEFX_SPLIT_MAX_LOCAL_VIEWS];

void RE_STEFX_SplitScreen_SetLocalRefdef( int slot, const refdef_t *refdef, qboolean valid )
{
	if ( slot < 0 || slot >= STEFX_SPLIT_MAX_LOCAL_VIEWS )
	{
		return;
	}
	if ( !valid || !refdef )
	{
		s_stefxSplitRefdefValid[slot] = qfalse;
		return;
	}

	s_stefxSplitRefdef[slot] = *refdef;
	VectorCopy( refdef->vieworg, s_stefxSplitPvsOrigin[slot] );
	s_stefxSplitRefdefValid[slot] = qtrue;
}

qboolean RE_STEFX_SplitScreen_GetLocalRefdef( int slot, refdef_t *refdef )
{
	if ( slot < 0 || slot >= STEFX_SPLIT_MAX_LOCAL_VIEWS ||
		!refdef || !s_stefxSplitRefdefValid[slot] )
	{
		return qfalse;
	}
	*refdef = s_stefxSplitRefdef[slot];
	return qtrue;
}

void RE_STEFX_SplitScreen_SetP2Refdef( const refdef_t *refdef, qboolean valid )
{
	RE_STEFX_SplitScreen_SetLocalRefdef( 1, refdef, valid );
}

void RE_STEFX_SplitScreen_SetP2PvsOrigin( const vec3_t origin )
{
	if ( !origin )
	{
		return;
	}

	VectorCopy( origin, s_stefxSplitPvsOrigin[1] );
}

static qboolean R_STEFX_GetSplitRefdef( int slot, refdef_t *out, vec3_t pvsOrigin )
{
	if ( slot < 0 || slot >= STEFX_SPLIT_MAX_LOCAL_VIEWS || !s_stefxSplitRefdefValid[slot] )
	{
		return qfalse;
	}
	if ( out )
	{
		*out = s_stefxSplitRefdef[slot];
	}
	if ( pvsOrigin )
	{
		VectorCopy( s_stefxSplitPvsOrigin[slot], pvsOrigin );
	}
	return qtrue;
}

static int R_STEFX_ClampSplitPlayers( int players )
{
	if ( players < 1 )
	{
		return 1;
	}
	if ( players > 4 )
	{
		return 4;
	}
	return players;
}

static qboolean R_STEFX_SplitScreenEconomyActive( void )
{
	if ( !Cvar_VariableIntegerValue( "r_splitScreenEconomy" ) )
	{
		return qfalse;
	}
	if ( !Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		return qfalse;
	}
	return R_STEFX_ClampSplitPlayers( Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) ) >= 2 ? qtrue : qfalse;
}

static void R_STEFX_LogSplitDrawSurfCapacity( int slot, int players, int drawAfter )
{
	static unsigned int s_stefxSplitDrawSurfMilestones = 0;
	unsigned int milestone = 0;

	if ( drawAfter >= MAX_DRAWSURFS )
	{
		milestone = 8u;
	}
	else if ( drawAfter >= 0x8000 )
	{
		milestone = 4u;
	}
	else if ( drawAfter >= 0x4000 )
	{
		milestone = 2u;
	}
	else if ( drawAfter >= 0x3000 )
	{
		milestone = 1u;
	}
	if ( milestone && !( s_stefxSplitDrawSurfMilestones & milestone ) )
	{
		s_stefxSplitDrawSurfMilestones |= milestone;
		XBLog_WriteCriticalf( "STEFX_HM_SPLIT_DRAWSURF_CAPACITY: slot=%d players=%d drawAfter=%d capacity=%d milestone=0x%x",
			slot, players, drawAfter, MAX_DRAWSURFS, milestone );
	}
}

static qboolean R_STEFX_ShouldRenderSplitScreen( const refdef_t *fd, int *players )
{
	int requestedPlayers;
	int slot;

	if ( !fd )
	{
		return qfalse;
	}
	if ( !Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		return qfalse;
	}
	if ( fd->rdflags & ( RDF_SKYBOXPORTAL | RDF_NOWORLDMODEL ) )
	{
		return qfalse;
	}
	if ( fd->width <= 1 || fd->height <= 1 )
	{
		return qfalse;
	}

	requestedPlayers = R_STEFX_ClampSplitPlayers( Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) );
	if ( requestedPlayers < 2 )
	{
		return qfalse;
	}
	for ( slot = 1; slot < requestedPlayers; ++slot )
	{
		if ( !s_stefxSplitRefdefValid[slot] )
		{
			return qfalse;
		}
	}

	if ( players )
	{
		*players = requestedPlayers;
	}
	return qtrue;
}

// The cgame supplies FOVs for square framebuffer pixels. Xbox 480p wide
// is anamorphic: widen X once at scene admission, keeping authored vertical
// framing. This also covers ICARUS cameras and menu model previews.
#ifdef _XBOX
extern float GLW_GetPixelAspect(void);
#endif
static float R_STEFX_PixelAspect(void)
{
#ifdef _XBOX
    return GLW_GetPixelAspect();
#else
    return 1.0f;
#endif
}
static float R_STEFX_WidenFov(float fovX, float pixelAspect)
{
    if (pixelAspect == 1.0f) return fovX;
    return atan(tan(fovX * M_PI / 360.0f) * pixelAspect) * 360.0f / M_PI;
}

static float R_STEFX_CalcFovXForViewport( float fovY, int width, int height )
{
	float y;

	if ( width <= 0 || height <= 0 )
	{
		return fovY;
	}

	y = (float)height / tan( fovY / 360.0f * M_PI );
	return atan2( (float)width * R_STEFX_PixelAspect(), y ) * 360.0f / M_PI;
}

static void R_STEFX_SetSplitViewport( trRefdef_t *refdef, viewParms_t *parms, const trRefdef_t *sourceRefdef, const viewParms_t *sourceParms, int slot, int players )
{
	int x;
	int y;
	int w;
	int h;
	int halfW;
	int halfH;

	*refdef = *sourceRefdef;
	*parms = *sourceParms;

	x = sourceRefdef->x;
	y = sourceRefdef->y;
	w = sourceRefdef->width;
	h = sourceRefdef->height;
	players = R_STEFX_ClampSplitPlayers( players );

	if ( players == 2 )
	{
		const int topHeight = sourceRefdef->height / 2;

		y = sourceRefdef->y + ( slot ? topHeight : 0 );
		h = slot ? ( sourceRefdef->height - topHeight ) : topHeight;
	}
	else if ( players == 3 )
	{
		halfH = sourceRefdef->height / 2;
		y = sourceRefdef->y + ( slot ? halfH : 0 );
		h = slot ? ( sourceRefdef->height - halfH ) : halfH;
		if ( slot > 0 )
		{
			halfW = sourceRefdef->width / 2;
			x = sourceRefdef->x + ( slot == 2 ? halfW : 0 );
			w = slot == 2 ? ( sourceRefdef->width - halfW ) : halfW;
		}
	}
	else if ( players >= 4 )
	{
		halfW = sourceRefdef->width / 2;
		halfH = sourceRefdef->height / 2;
		x = sourceRefdef->x + ( ( slot & 1 ) ? halfW : 0 );
		y = sourceRefdef->y + ( ( slot & 2 ) ? halfH : 0 );
		w = ( slot & 1 ) ? ( sourceRefdef->width - halfW ) : halfW;
		h = ( slot & 2 ) ? ( sourceRefdef->height - halfH ) : halfH;
	}

	refdef->x = x;
	refdef->y = y;
	refdef->width = w;
	refdef->height = h;
	refdef->fov_y = sourceRefdef->fov_y;
	refdef->fov_x = R_STEFX_CalcFovXForViewport( refdef->fov_y, w, h );

	parms->viewportX = x;
	parms->viewportY = glConfig.vidHeight - ( y + h );
	parms->viewportWidth = w;
	parms->viewportHeight = h;
	parms->fovX = refdef->fov_x;
	parms->fovY = refdef->fov_y;
	parms->stefxSplitView = qtrue;
	parms->stefxSplitEconomy = R_STEFX_SplitScreenEconomyActive();
	parms->stefxSplitThreePlusEconomy =
		parms->stefxSplitEconomy && players >= 3 ? qtrue : qfalse;
	parms->stefxSplitSlot = slot;

	if ( parms->stefxSplitThreePlusEconomy )
	{
		static qboolean s_stefxThreePlusQualityLogged = qfalse;
		if ( !s_stefxThreePlusQualityLogged )
		{
			s_stefxThreePlusQualityLogged = qtrue;
			XBLog_WriteCriticalf(
				"STEFX_HM_QUALITY: players=%d rendererFog=0 nonMdrLodBias=1 mdrLod=0 firstPersonLodBias=0 impactMarks=0 spawnerParticles=0.4 gameplayPortraits=2D vertexShaderCache=tracked streamSourceCache=tracked policy=three-plus",
				players );
		}
	}
}

static void R_STEFX_LogSplitTiling( const trRefdef_t *sourceRefdef, const viewParms_t *sourceParms, int players )
{
	static int s_lastPlayers = 0;
	static int s_lastSourceX = 0;
	static int s_lastSourceY = 0;
	static int s_lastSourceW = 0;
	static int s_lastSourceH = 0;
	static int s_nextLogMsec = 0;
	trRefdef_t rects[STEFX_SPLIT_MAX_LOCAL_VIEWS];
	viewParms_t rectParms;
	int nowMsec;
	int slot;
	int other;
	int overlapPixels = 0;
	int coveredPixels = 0;
	int sourcePixels;
	int gapPixels;
	qboolean valid = qtrue;

	if ( !sourceRefdef || !sourceParms )
	{
		return;
	}
	players = R_STEFX_ClampSplitPlayers( players );
	if ( players < 2 )
	{
		return;
	}
	nowMsec = Sys_Milliseconds();
	if ( players == s_lastPlayers &&
		sourceRefdef->x == s_lastSourceX &&
		sourceRefdef->y == s_lastSourceY &&
		sourceRefdef->width == s_lastSourceW &&
		sourceRefdef->height == s_lastSourceH &&
		nowMsec < s_nextLogMsec )
	{
		return;
	}

	memset( rects, 0, sizeof( rects ) );
	for ( slot = 0; slot < players; ++slot )
	{
		int right;
		int bottom;

		R_STEFX_SetSplitViewport( &rects[slot], &rectParms,
			sourceRefdef, sourceParms, slot, players );
		right = rects[slot].x + rects[slot].width;
		bottom = rects[slot].y + rects[slot].height;
		coveredPixels += rects[slot].width * rects[slot].height;
		if ( rects[slot].width <= 0 || rects[slot].height <= 0 ||
			rects[slot].x < sourceRefdef->x ||
			rects[slot].y < sourceRefdef->y ||
			right > sourceRefdef->x + sourceRefdef->width ||
			bottom > sourceRefdef->y + sourceRefdef->height )
		{
			valid = qfalse;
		}

		for ( other = 0; other < slot; ++other )
		{
			int overlapLeft = rects[slot].x > rects[other].x ? rects[slot].x : rects[other].x;
			int overlapTop = rects[slot].y > rects[other].y ? rects[slot].y : rects[other].y;
			int overlapRight = right < rects[other].x + rects[other].width ? right : rects[other].x + rects[other].width;
			int overlapBottom = bottom < rects[other].y + rects[other].height ? bottom : rects[other].y + rects[other].height;

			if ( overlapRight > overlapLeft && overlapBottom > overlapTop )
			{
				overlapPixels += ( overlapRight - overlapLeft ) * ( overlapBottom - overlapTop );
			}
		}
	}

	sourcePixels = sourceRefdef->width * sourceRefdef->height;
	gapPixels = sourcePixels - ( coveredPixels - overlapPixels );
	if ( overlapPixels != 0 || gapPixels != 0 )
	{
		valid = qfalse;
	}
	XBLog_WriteCriticalf(
		"STEFX_HM_SPLIT_TILING: players=%d source=(%d,%d %dx%d) sourcePixels=%d coveredPixels=%d overlapPixels=%d gapPixels=%d separators=0 valid=%d",
		players,
		sourceRefdef->x,
		sourceRefdef->y,
		sourceRefdef->width,
		sourceRefdef->height,
		sourcePixels,
		coveredPixels,
		overlapPixels,
		gapPixels,
		valid ? 1 : 0 );

	s_lastPlayers = players;
	s_lastSourceX = sourceRefdef->x;
	s_lastSourceY = sourceRefdef->y;
	s_lastSourceW = sourceRefdef->width;
	s_lastSourceH = sourceRefdef->height;
	s_nextLogMsec = nowMsec + 5000;
}

static void R_STEFX_ApplyExternalSplitView( trRefdef_t *refdef, viewParms_t *parms, const refdef_t *externalRefdef, const vec3_t pvsOrigin )
{
	int i;

	if ( !refdef || !parms || !externalRefdef )
	{
		return;
	}

	refdef->fov_y = externalRefdef->fov_y;
	refdef->fov_x = R_STEFX_CalcFovXForViewport( refdef->fov_y, refdef->width, refdef->height );
	VectorCopy( externalRefdef->vieworg, refdef->vieworg );
	VectorCopy( externalRefdef->viewaxis[0], refdef->viewaxis[0] );
	VectorCopy( externalRefdef->viewaxis[1], refdef->viewaxis[1] );
	VectorCopy( externalRefdef->viewaxis[2], refdef->viewaxis[2] );
	refdef->time = externalRefdef->time;
	refdef->areamaskModified = qtrue;
	for ( i = 0; i < MAX_MAP_AREA_BYTES; ++i )
	{
		refdef->areamask[i] = externalRefdef->areamask[i];
	}

	parms->fovX = refdef->fov_x;
	parms->fovY = refdef->fov_y;
	VectorCopy( refdef->vieworg, parms->or.origin );
	VectorCopy( refdef->viewaxis[0], parms->or.axis[0] );
	VectorCopy( refdef->viewaxis[1], parms->or.axis[1] );
	VectorCopy( refdef->viewaxis[2], parms->or.axis[2] );
	if ( pvsOrigin )
	{
		VectorCopy( pvsOrigin, parms->pvsOrigin );
	}
	else
	{
		VectorCopy( refdef->vieworg, parms->pvsOrigin );
	}
}

#endif

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderWorldEffects(void);
void RE_RenderScene( const refdef_t *fd ) {
	viewParms_t		parms;
	int				startTime;
	static	int		lastTime = 0;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned __int64 xboxSceneStart;
#endif

	if ( !tr.registered ) {
		return;
	}
	GLimp_LogComment( "====== RE_RenderScene =====\n" );

	if ( r_norefresh->integer ) {
		return;
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxSceneStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
	startTime = Sys_Milliseconds()*com_timescale->value;

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		Com_Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = R_STEFX_WidenFov(fd->fov_x, R_STEFX_PixelAspect());
	tr.refdef.fov_y = fd->fov_y;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
    // Proof is sampled read-only by the harness.
    int aspectSlot = (fd->rdflags & RDF_NOWORLDMODEL) ? 6 : 0;
    float pixelAspect = R_STEFX_PixelAspect();
    g_SPXBViewAspectProof[aspectSlot] = fd->width;
    g_SPXBViewAspectProof[aspectSlot + 1] = fd->height;
    g_SPXBViewAspectProof[aspectSlot + 2] = *(unsigned int *)&pixelAspect;
    g_SPXBViewAspectProof[aspectSlot + 3] = *(unsigned int *)&fd->fov_x;
    g_SPXBViewAspectProof[aspectSlot + 4] = *(unsigned int *)&tr.refdef.fov_x;
    g_SPXBViewAspectProof[aspectSlot + 5] = *(unsigned int *)&tr.refdef.fov_y;
    static int aspectLogBudget = 12;
    if (aspectLogBudget > 0) {
        --aspectLogBudget;
        XBLF("STEFX_VIEW_ASPECT: viewport=%dx%d pixelAspect=%g input=%g/%g output=%g/%g flags=%d",
            fd->width, fd->height, R_STEFX_PixelAspect(), fd->fov_x, fd->fov_y,
            tr.refdef.fov_x, tr.refdef.fov_y, fd->rdflags);
    }
#endif



	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.frametime = fd->time - lastTime;
	lastTime = fd->time;

	if (fd->rdflags & RDF_SKYBOXPORTAL)
	{
		skyboxportal = 1;
	}

	if (fd->rdflags & RDF_DRAWSKYBOX)
	{
		drawskyboxportal = 1;
	}
	else
	{
		drawskyboxportal = 0;
	}

	if (tr.refdef.frametime > 500)
	{
		tr.refdef.frametime = 500;
	}
	else if (tr.refdef.frametime < 0)
	{
		tr.refdef.frametime = 0;
	}
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		for (i = 0 ; i < MAX_MAP_AREA_BYTES/4 ; i++) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}


	// derived info

	tr.refdef.floatTime = tr.refdef.time * 0.001f;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];
	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled or if vertex lighting is enabled
	if ( r_dynamiclight->integer == 0 ||
		 r_vertexLight->integer == 1 ) {
		tr.refdef.num_dlights = 0;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	else if ( R_STEFX_SplitScreenEconomyActive() && tr.refdef.num_dlights > 0 ) {
		static int s_stefxSplitEconomyDlightLogBudget = 8;
		if ( s_stefxSplitEconomyDlightLogBudget > 0 ) {
			XBLog_WriteCriticalf( "STEFX_SPLIT_ECONOMY: dlightsSkipped=%d players=%d", tr.refdef.num_dlights, R_STEFX_ClampSplitPlayers( Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) ) );
			--s_stefxSplitEconomyDlightLogBudget;
		}
		tr.refdef.num_dlights = 0;
	}
#endif

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;
	parms.isPortal = qfalse;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	{
		int splitPlayers = 1;
		if ( R_STEFX_ShouldRenderSplitScreen( fd, &splitPlayers ) )
		{
			int slot;
			int splitDrawSurfBase;
			trRefdef_t sourceRefdef = tr.refdef;
			viewParms_t sourceParms = parms;
			static int s_stefxSplitViewportLogBudget = 3;
			const qboolean logSplitViewports = s_stefxSplitViewportLogBudget > 0 ? qtrue : qfalse;
			splitDrawSurfBase = tr.refdef.numDrawSurfs;

			if ( logSplitViewports )
			{
				XBLog_WriteCriticalf( "STEFX_HM_SPLIT_RENDER: armed players=%d source=%d,%d %dx%d gl=%d,%d %dx%d fov=%g/%g",
					splitPlayers,
					sourceRefdef.x,
					sourceRefdef.y,
					sourceRefdef.width,
					sourceRefdef.height,
					sourceParms.viewportX,
					sourceParms.viewportY,
					sourceParms.viewportWidth,
					sourceParms.viewportHeight,
					sourceRefdef.fov_x,
					sourceRefdef.fov_y );
			}
			R_STEFX_LogSplitTiling( &sourceRefdef, &sourceParms, splitPlayers );
#ifdef _XBOX
			g_SPXBHMSplitRenderArmedPlayers = (unsigned int)splitPlayers;
#endif

			for ( slot = 0; slot < splitPlayers; ++slot )
			{
				refdef_t splitRefdef;
				vec3_t splitPvsOrigin;
				qboolean appliedExternal = qfalse;
				int externalClient = slot ? -1 : 0;
				int splitSlotDrawStart;
				R_STEFX_SetSplitViewport( &tr.refdef, &parms, &sourceRefdef, &sourceParms, slot, splitPlayers );
				tr.refdef.numDrawSurfs = splitDrawSurfBase;
				splitSlotDrawStart = tr.refdef.numDrawSurfs;
				g_SPXBSplitSlotActive = (unsigned int)( slot + 1 );
				if ( slot > 0 && R_STEFX_GetSplitRefdef( slot, &splitRefdef, splitPvsOrigin ) )
				{
					R_STEFX_ApplyExternalSplitView( &tr.refdef, &parms, &splitRefdef, splitPvsOrigin );
					tr.refdef.areamaskModified = qtrue;
					tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
					appliedExternal = qtrue;
					externalClient = slot;
				}
				if ( logSplitViewports )
				{
					XBLog_WriteCriticalf( "STEFX_HM_SPLIT_RENDER: slot=%d external=%d externalClient=%d drawBase=%d ref=%d,%d %dx%d gl=%d,%d %dx%d fov=%g/%g view=(%g,%g,%g) pvs=(%g,%g,%g)",
						slot,
						appliedExternal ? 1 : 0,
						externalClient,
						splitDrawSurfBase,
						tr.refdef.x,
						tr.refdef.y,
						tr.refdef.width,
						tr.refdef.height,
						parms.viewportX,
						parms.viewportY,
						parms.viewportWidth,
						parms.viewportHeight,
						tr.refdef.fov_x,
						tr.refdef.fov_y,
						tr.refdef.vieworg[0],
						tr.refdef.vieworg[1],
						tr.refdef.vieworg[2],
						parms.pvsOrigin[0],
						parms.pvsOrigin[1],
						parms.pvsOrigin[2] );
				}
#ifdef _XBOX
				if ( slot >= 0 && slot < 4 )
				{
					++g_SPXBHMSplitRenderSerial[slot];
					g_SPXBHMSplitRenderExternal[slot] = appliedExternal ? 1u : 0u;
					g_SPXBHMSplitRenderClient[slot] = (unsigned int)externalClient;
					g_SPXBHMSplitRenderRectX[slot] = (unsigned int)tr.refdef.x;
					g_SPXBHMSplitRenderRectY[slot] = (unsigned int)tr.refdef.y;
					g_SPXBHMSplitRenderRectW[slot] = (unsigned int)tr.refdef.width;
					g_SPXBHMSplitRenderRectH[slot] = (unsigned int)tr.refdef.height;
					g_SPXBHMSplitRenderViewX[slot] = (unsigned int)((int)tr.refdef.vieworg[0]);
					g_SPXBHMSplitRenderViewY[slot] = (unsigned int)((int)tr.refdef.vieworg[1]);
					g_SPXBHMSplitRenderViewZ[slot] = (unsigned int)((int)tr.refdef.vieworg[2]);
				}
#endif
				STEFX_RETAIL_SCOPE R_RenderView( &parms );
				R_STEFX_LogSplitDrawSurfCapacity( slot, splitPlayers, tr.refdef.numDrawSurfs );
				if ( logSplitViewports )
				{
					XBLog_WriteCriticalf( "STEFX_HM_SPLIT_RENDER_DONE: slot=%d external=%d externalClient=%d drawDelta=%d drawAfter=%d cluster=%d cluster2=%d view=(%g,%g,%g)",
						slot,
						appliedExternal ? 1 : 0,
						externalClient,
						tr.refdef.numDrawSurfs - splitSlotDrawStart,
						tr.refdef.numDrawSurfs,
						tr.viewCluster,
						-1,
						tr.refdef.vieworg[0],
						tr.refdef.vieworg[1],
						tr.refdef.vieworg[2] );
				}
#ifdef _XBOX
				if ( slot >= 0 && slot < 4 )
				{
					++g_SPXBHMSplitRenderDoneSerial[slot];
					g_SPXBHMSplitRenderDrawDelta[slot] =
						(unsigned int)(tr.refdef.numDrawSurfs - splitSlotDrawStart);
					g_SPXBHMSplitRenderDrawAfter[slot] = (unsigned int)tr.refdef.numDrawSurfs;
					g_SPXBHMSplitRenderCluster[slot] = (unsigned int)tr.viewCluster;
				}
#endif
				if ( tr.refdef.numDrawSurfs > splitDrawSurfBase )
				{
					splitDrawSurfBase = tr.refdef.numDrawSurfs;
				}
			}
			g_SPXBSplitSlotActive = 0;

			if ( logSplitViewports )
			{
				--s_stefxSplitViewportLogBudget;
			}
			tr.refdef = sourceRefdef;
			tr.refdef.numDrawSurfs = splitDrawSurfBase;
			parms = sourceParms;
		}
		else
		{
			STEFX_RETAIL_SCOPE R_RenderView( &parms );
		}
	}
#else
	STEFX_RETAIL_SCOPE R_RenderView( &parms );
#endif

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	refEntParent = -1;

	tr.frontEndMsec += Sys_Milliseconds()*com_timescale->value - startTime;

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderTotalMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxSceneStart );
		g_SPXBPerfRenderRefEntities += (unsigned int)tr.refdef.num_entities;
	}
#endif

	RE_RenderWorldEffects();

}

#if 0 //rwwFIXMEFIXME: Disable this before release!!!!!! I am just trying to find a crash bug.
int R_GetRNumEntities(void)
{
	return r_numentities;
}

void R_SetRNumEntities(int num)
{
	r_numentities = num;
}
#endif

STEFX_RETAIL_NAMESPACE_END
