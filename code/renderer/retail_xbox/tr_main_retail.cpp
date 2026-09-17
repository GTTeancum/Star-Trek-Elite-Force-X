// tr_main.c -- main control flow for each frame
//Anything above this #include will be ignored by the compiler
#include "../../server/exe_headers.h"

#include "../tr_local.h"
#include "retail_renderer_contract.h"
#include "../../win32/xb_perf.h"
#include "../../win32/xb_log.h"
// Yeah, this might be kind of bad, but no linux version is planned so far :-) - AReis
// Gee- thanks guys - jdrews, the linux porter...
#ifndef _XBOX
#ifndef __linux__
#include "../win32/glw_win.h"
#endif 
#endif

#ifdef _XBOX
extern "C" volatile unsigned int g_SPXBSplitSlotActive;
extern "C" volatile unsigned int g_SPXBHMSplitLowerModel;
extern "C" volatile unsigned int g_SPXBHMSplitUpperModel;
extern "C" volatile unsigned int g_SPXBHMSplitHeadModel;
extern "C" volatile unsigned int g_SPXBHMSplitFPFilterMask;
extern "C" volatile unsigned int g_SPXBHMSplitSelfFilterMask;
extern "C" volatile unsigned int g_SPXBHMSplitSelfFilterRefNumber[4];
extern "C" volatile unsigned int g_SPXBHMSplitSelfFilterPart[4];
extern "C" volatile unsigned int g_SPXBHMSplitPhaserWorldHidden[4];
extern "C" volatile unsigned int g_SPXBHMPvsProbe[296];
#endif

STEFX_RETAIL_NAMESPACE_BEGIN

trGlobals_t		tr;

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && !defined(STEFX_SP_HOSTED_MP)
// Retain the first sixteen distinct missing-model submissions across reloads.
// Header: version, records, submissions, overflow. Records: handle, entity,
// frame, model count, render flags, and three raw origin floats. Rendering stays unchanged.
extern "C" volatile unsigned int g_SPXBVvMissingModels[132] = {1, 0, 0, 0};
static void STEFX_RecordMissingModel(const trRefEntity_t *ent)
{
    ++g_SPXBVvMissingModels[2];
    unsigned int count = g_SPXBVvMissingModels[1];
    for (unsigned int i = 0; i < count; ++i)
        if (g_SPXBVvMissingModels[4+i*8] == (unsigned int)ent->e.hModel &&
            g_SPXBVvMissingModels[5+i*8] == (unsigned int)ent->e.number) return;
    if (count >= 16) { ++g_SPXBVvMissingModels[3]; return; }
    unsigned int base = 4 + count*8;
    g_SPXBVvMissingModels[base] = ent->e.hModel;
    g_SPXBVvMissingModels[base+1] = ent->e.number;
    g_SPXBVvMissingModels[base+2] = tr.frameCount;
    g_SPXBVvMissingModels[base+3] = tr.numModels;
    g_SPXBVvMissingModels[base+4] = ent->e.renderfx;
    for (int axis = 0; axis < 3; ++axis)
    {
        unsigned int bits;
        memcpy(&bits, &ent->e.origin[axis], sizeof(bits));
        g_SPXBVvMissingModels[base+5+axis] = bits;
    }
    g_SPXBVvMissingModels[1] = count+1;
    XBLF("STEFX_VV_MISSING_MODEL: entity=%d handle=%d models=%d frame=%d name='%s' origin=(%g,%g,%g)",
        ent->e.number, ent->e.hModel, tr.numModels, tr.frameCount,
        tr.currentModel->name, ent->e.origin[0], ent->e.origin[1], ent->e.origin[2]);
}
#endif

#include "stefx_fx_bounds.h"
#include "stefx_coop_model_pvs.h"

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
// STEFX_FX_PVS_BEGIN
extern "C" volatile unsigned int g_SPXBFxPvs[4] = {0};
static bool STEFX_FxTouchesVisibleLeaf(const float bounds[2][3], const mnode_t *root,
    const cplane_t *planes, int planeCount, int visibleFrame) {
    const mnode_t *stack[32];
    int pending = 0, visited = 0;
    stack[pending++] = root;
    while (pending) {
        const mnode_t *node = stack[--pending];
        if (!node || ++visited > 128) { ++g_SPXBFxPvs[2]; return true; }
        ++g_SPXBFxPvs[3];
        if (node->visframe != visibleFrame) continue;
        if (node->contents != -1) return true;
        if (node->planeNum >= (unsigned int)planeCount) { ++g_SPXBFxPvs[2]; return true; }
        const cplane_t &plane = planes[node->planeNum];
        float lo = -plane.dist, hi = -plane.dist;
        for (int axis = 0; axis < 3; ++axis) {
            lo += plane.normal[axis] * bounds[plane.normal[axis] >= 0.0f ? 0 : 1][axis];
            hi += plane.normal[axis] * bounds[plane.normal[axis] >= 0.0f ? 1 : 0][axis];
        }
        // Touching a partition must search both sides. Long beams can touch
        // visible space even when neither endpoint is in a visible leaf.
        if (!(lo > -1.0e9f && hi < 1.0e9f && lo <= hi) || pending > 29) {
            ++g_SPXBFxPvs[2]; return true;
        }
        if (hi >= -1.0f) stack[pending++] = node->children[0];
        if (lo <= 1.0f) stack[pending++] = node->children[1];
    }
    return false;
}
static bool STEFX_CullFxOutsidePvs(const refEntity_t &e) {
    static cvar_t *enabled;
    if (!enabled) enabled = Cvar_Get("r_efFxPvs", "0", 0);
    if (!enabled->integer || !tr.viewParms.stefxSplitView || tr.viewParms.isPortal ||
        !tr.world || !tr.world->nodes || !tr.world->planes || tr.viewCluster < 0 ||
        tr.world->nodes[0].visframe != tr.visCount || !r_drawworld->integer ||
        r_nocull->integer || r_novis->integer || r_lockpvs->integer ||
        (tr.refdef.rdflags & RDF_NOWORLDMODEL) ||
        (e.renderfx & (RF_NODEPTH | RF_DEPTHHACK | RF_DISTORTION))) return false;
    float bounds[2][3];
    if (!STEFX_FxBounds(e, bounds)) return false;
    shader_t *shader = R_GetShaderByHandle(e.customShader);
    if (!shader) return false;
    if (shader->remappedShader) shader = shader->remappedShader;
    // Some world-positioned overlays intentionally draw through walls.
    for (int stage = 0; stage < shader->numUnfoggedPasses; ++stage)
        if (shader->stages[stage].stateBits & GLS_DEPTHTEST_DISABLE) return false;
    ++g_SPXBFxPvs[0];
    if (STEFX_FxTouchesVisibleLeaf(bounds, tr.world->nodes, tr.world->planes,
        tr.world->numplanes, tr.visCount)) return false;
    if (++g_SPXBFxPvs[1] == 1u)
        XBLog_WriteCriticalf("STEFX_FX_PVS: active cluster=%d type=%d", tr.viewCluster, e.reType);
    return true;
}
// STEFX_FX_PVS_END
#endif

static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
#if defined (_XBOX)
	0, 0, 1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
#else
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
#endif
};

void R_AddTerrainSurfaces(void);

#ifndef DEDICATED

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static int R_STEFX_FirstPersonSplitSlotForRenderfx( int renderfx )
{
	if ( !( renderfx & RF_FIRST_PERSON ) )
	{
		return 0;
	}
	if ( renderfx & RF_STEFX_SPLIT_SLOT0 )
	{
		return 1;
	}
#if defined(STEFX_SP_HOSTED_MP)
	if ( ( renderfx & RF_STEFX_SPLIT_SLOT1 ) && ( renderfx & RF_STEFX_SPLIT_SLOT2 ) )
	{
		return 4;
	}
	if ( renderfx & RF_STEFX_SPLIT_SLOT2 )
	{
		return 3;
	}
#endif
	if ( renderfx & RF_STEFX_SPLIT_SLOT1 )
	{
		return 2;
	}
	return 1;
}
#endif

static qboolean R_STEFX_ShouldHideHolomatchSplitSelfModel( const trRefEntity_t *ent, qboolean filterActive, float *xyDistance, float *zDelta, const char **modelPart )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	const char *part;
	unsigned int modelHandle;
	int expectedClientNum;
	float dx;
	float dy;
	float dist2;
	float dz;

	if ( modelPart )
	{
		*modelPart = "unknown";
	}
	if ( xyDistance )
	{
		*xyDistance = 0.0f;
	}
	if ( zDelta )
	{
		*zDelta = 0.0f;
	}
	if ( !filterActive || !ent || g_SPXBSplitSlotActive <= 1 )
	{
		return qfalse;
	}
	if ( ent->e.reType != RT_MODEL || ( ent->e.renderfx & RF_FIRST_PERSON ) || !ent->e.hModel )
	{
		return qfalse;
	}
	expectedClientNum = (int)g_SPXBSplitSlotActive - 1;
	if ( expectedClientNum < 1 )
	{
		return qfalse;
	}
	modelHandle = (unsigned int)ent->e.hModel;
	if ( modelHandle == g_SPXBHMSplitLowerModel )
	{
		part = "lower";
	}
	else if ( modelHandle == g_SPXBHMSplitUpperModel )
	{
		part = "upper";
	}
	else if ( modelHandle == g_SPXBHMSplitHeadModel )
	{
		part = "head";
	}
	else
	{
		part = "local";
	}
	if ( modelPart )
	{
		*modelPart = part;
	}

	dx = ent->e.origin[0] - tr.refdef.vieworg[0];
	dy = ent->e.origin[1] - tr.refdef.vieworg[1];
	dz = tr.refdef.vieworg[2] - ent->e.origin[2];
	dist2 = dx * dx + dy * dy;
	if ( xyDistance )
	{
		*xyDistance = sqrtf( dist2 );
	}
	if ( zDelta )
	{
		*zDelta = dz;
	}

	if ( modelHandle == g_SPXBHMSplitLowerModel ||
		modelHandle == g_SPXBHMSplitUpperModel ||
		modelHandle == g_SPXBHMSplitHeadModel )
	{
		return ( dist2 <= ( 128.0f * 128.0f ) && dz >= -64.0f && dz <= 192.0f ) ? qtrue : qfalse;
	}

	if ( ent->e.number == expectedClientNum )
	{
		return ( dist2 <= ( 256.0f * 256.0f ) && dz >= -96.0f && dz <= 256.0f ) ? qtrue : qfalse;
	}

	return ( dist2 <= ( 96.0f * 96.0f ) && dz >= -96.0f && dz <= 256.0f ) ? qtrue : qfalse;
#else
	(void)ent;
	(void)filterActive;
	(void)xyDistance;
	(void)zDelta;
	(void)modelPart;
	return qfalse;
#endif
}

static qboolean R_STEFX_ShouldHideHolomatchSplitWorldPhaser( const trRefEntity_t *ent, qboolean filterActive )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	int owner;

	if ( !filterActive || !ent || g_SPXBSplitSlotActive < 1 || g_SPXBSplitSlotActive > 4 ||
		( ent->e.renderfx & RF_FIRST_PERSON ) )
	{
		return qfalse;
	}
	if ( ent->e.reType != RT_TEXTURED_LINE && ent->e.reType != RT_TAPERED_LINE )
	{
		return qfalse;
	}
	owner = ent->e.number - STEFX_REFENTITY_PHASER_BEAM_BASE;
	return owner >= 0 && owner < 4 && owner == (int)g_SPXBSplitSlotActive - 1;
#else
	(void)ent;
	(void)filterActive;
	return qfalse;
#endif
}

// entities that will have procedurally generated surfaces will just
// point at this for their sorting surface
surfaceType_t	entitySurface = SF_ENTITY;

/*
=================
R_CullLocalBox

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLocalBox (const vec3_t bounds[2]) {
	int		i, j;
	vec3_t	transformed[8];
	float	dists[8];
	vec3_t	v;
	cplane_t	*frust;
	int			anyBack;
	int			front, back;

	if ( r_nocull->integer==1 ) {
		return CULL_CLIP;
	}

	// transform into world space
	for (i = 0 ; i < 8 ; i++) {
		v[0] = bounds[i&1][0];
		v[1] = bounds[(i>>1)&1][1];
		v[2] = bounds[(i>>2)&1][2];

		VectorCopy( tr.or.origin, transformed[i] );
		VectorMA( transformed[i], v[0], tr.or.axis[0], transformed[i] );
		VectorMA( transformed[i], v[1], tr.or.axis[1], transformed[i] );
		VectorMA( transformed[i], v[2], tr.or.axis[2], transformed[i] );
	}

	// check against frustum planes
	anyBack = 0;
	for (i = 0 ; i < 4 ; i++) {
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for (j = 0 ; j < 8 ; j++) {
			dists[j] = DotProduct(transformed[j], frust->normal);
			if ( dists[j] > frust->dist ) {
				front = 1;
				if ( back ) {
					break;		// a point is in front
				}
			} else {
				back = 1;
			}
		}
		if ( !front ) {
			// all points were behind one of the planes
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if ( !anyBack ) {
		return CULL_IN;		// completely inside frustum
	}

	return CULL_CLIP;		// partially clipped
}

#endif // !DEDICATED

/*
** R_CullLocalPointAndRadius
*/
int R_CullLocalPointAndRadius( const vec3_t pt, float radius )
{
	vec3_t transformed;

	R_LocalPointToWorld( pt, transformed );

	return R_CullPointAndRadius( transformed, radius );
}

/*
** R_CullPointAndRadius
*/
int R_CullPointAndRadius( const vec3_t pt, float radius )
{
	int		i;
	float	dist;
	cplane_t	*frust;
	qboolean mightBeClipped = qfalse;

	if ( r_nocull->integer==1 ) {
		return CULL_CLIP;
	}

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++) 
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct( pt, frust->normal) - frust->dist;
		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius ) 
		{
			mightBeClipped = qtrue;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;		// completely inside frustum
}

#ifndef DEDICATED

/*
=================
R_LocalNormalToWorld

=================
*/
void R_LocalNormalToWorld (const vec3_t local, vec3_t world) {
	world[0] = local[0] * tr.or.axis[0][0] + local[1] * tr.or.axis[1][0] + local[2] * tr.or.axis[2][0];
	world[1] = local[0] * tr.or.axis[0][1] + local[1] * tr.or.axis[1][1] + local[2] * tr.or.axis[2][1];
	world[2] = local[0] * tr.or.axis[0][2] + local[1] * tr.or.axis[1][2] + local[2] * tr.or.axis[2][2];
}

#endif // !DEDICATED
/*
=================
R_LocalPointToWorld

=================
*/
void R_LocalPointToWorld (const vec3_t local, vec3_t world) {
	world[0] = local[0] * tr.or.axis[0][0] + local[1] * tr.or.axis[1][0] + local[2] * tr.or.axis[2][0] + tr.or.origin[0];
	world[1] = local[0] * tr.or.axis[0][1] + local[1] * tr.or.axis[1][1] + local[2] * tr.or.axis[2][1] + tr.or.origin[1];
	world[2] = local[0] * tr.or.axis[0][2] + local[1] * tr.or.axis[1][2] + local[2] * tr.or.axis[2][2] + tr.or.origin[2];
}

#ifndef DEDICATED

float preTransEntMatrix[16];

/*
=================
R_WorldNormalToEntity 

=================
*/
void R_WorldNormalToEntity (const vec3_t worldvec, vec3_t entvec) 
{
	entvec[0] = -worldvec[0] * preTransEntMatrix[0] - worldvec[1] * preTransEntMatrix[4] + worldvec[2] * preTransEntMatrix[8];
	entvec[1] = -worldvec[0] * preTransEntMatrix[1] - worldvec[1] * preTransEntMatrix[5] + worldvec[2] * preTransEntMatrix[9];
	entvec[2] = -worldvec[0] * preTransEntMatrix[2] - worldvec[1] * preTransEntMatrix[6] + worldvec[2] * preTransEntMatrix[10];
}

/*
=================
R_WorldPointToEntity 

=================
*/
/*void R_WorldPointToEntity (vec3_t worldvec, vec3_t entvec)
{
	entvec[0] = worldvec[0] * preTransEntMatrix[0] + worldvec[1] * preTransEntMatrix[4] + worldvec[2] * preTransEntMatrix[8]+preTransEntMatrix[12];
	entvec[1] = worldvec[0] * preTransEntMatrix[1] + worldvec[1] * preTransEntMatrix[5] + worldvec[2] * preTransEntMatrix[9]+preTransEntMatrix[13];
	entvec[2] = worldvec[0] * preTransEntMatrix[2] + worldvec[1] * preTransEntMatrix[6] + worldvec[2] * preTransEntMatrix[10]+preTransEntMatrix[14];
}
*/

/*
=================
R_WorldToLocal

=================
*/
void R_WorldToLocal (vec3_t world, vec3_t local) {
	local[0] = DotProduct(world, tr.or.axis[0]);
	local[1] = DotProduct(world, tr.or.axis[1]);
	local[2] = DotProduct(world, tr.or.axis[2]);
}

/*
==========================
R_TransformModelToClip

==========================
*/
void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		eye[i] = 
			src[0] * modelMatrix[ i + 0 * 4 ] +
			src[1] * modelMatrix[ i + 1 * 4 ] +
			src[2] * modelMatrix[ i + 2 * 4 ] +
			1 * modelMatrix[ i + 3 * 4 ];
	}

	for ( i = 0 ; i < 4 ; i++ ) {
		dst[i] = 
			eye[0] * projectionMatrix[ i + 0 * 4 ] +
			eye[1] * projectionMatrix[ i + 1 * 4 ] +
			eye[2] * projectionMatrix[ i + 2 * 4 ] +
			eye[3] * projectionMatrix[ i + 3 * 4 ];
	}
}

/*
==========================
R_TransformClipToWindow

==========================
*/
void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = ( clip[2] + clip[3] ) / ( 2 * clip[3] );

	window[0] = 0.5f * ( 1.0f + normalized[0] ) * view->viewportWidth;
	window[1] = 0.5f * ( 1.0f + normalized[1] ) * view->viewportHeight;
	window[2] = normalized[2];

	window[0] = (int) ( window[0] + 0.5 );
	window[1] = (int) ( window[1] + 0.5 );
}


/*
==========================
myGlMultMatrix

==========================
*/
void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

/*
=================
R_RotateForEntity

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *ori ) {
//	float	glMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*ori = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, ori->origin );

	VectorCopy( ent->e.axis[0], ori->axis[0] );
	VectorCopy( ent->e.axis[1], ori->axis[1] );
	VectorCopy( ent->e.axis[2], ori->axis[2] );

	preTransEntMatrix[0] = ori->axis[0][0];
	preTransEntMatrix[4] = ori->axis[1][0];
	preTransEntMatrix[8] = ori->axis[2][0];
	preTransEntMatrix[12] = ori->origin[0];

	preTransEntMatrix[1] = ori->axis[0][1];
	preTransEntMatrix[5] = ori->axis[1][1];
	preTransEntMatrix[9] = ori->axis[2][1];
	preTransEntMatrix[13] = ori->origin[1];

	preTransEntMatrix[2] = ori->axis[0][2];
	preTransEntMatrix[6] = ori->axis[1][2];
	preTransEntMatrix[10] = ori->axis[2][2];
	preTransEntMatrix[14] = ori->origin[2];

	preTransEntMatrix[3] = 0;
	preTransEntMatrix[7] = 0;
	preTransEntMatrix[11] = 0;
	preTransEntMatrix[15] = 1;

	myGlMultMatrix( preTransEntMatrix, viewParms->world.modelMatrix, ori->modelMatrix );

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract( viewParms->or.origin, ori->origin, delta );

	// compensate for scale in the axes if necessary
	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0f / axisLength;
		}
	} else {
		axisLength = 1.0f;
	}

	ori->viewOrigin[0] = DotProduct( delta, ori->axis[0] ) * axisLength;
	ori->viewOrigin[1] = DotProduct( delta, ori->axis[1] ) * axisLength;
	ori->viewOrigin[2] = DotProduct( delta, ori->axis[2] ) * axisLength;
}

/*
=================
R_RotateForViewer

Sets up the modelview matrix for a given viewParm
=================
*/
void R_RotateForViewer (void) 
{
	float	viewerMatrix[16];
	vec3_t	origin;

	memset (&tr.or, 0, sizeof(tr.or));
	tr.or.axis[0][0] = 1;
	tr.or.axis[1][1] = 1;
	tr.or.axis[2][2] = 1;
	VectorCopy (tr.viewParms.or.origin, tr.or.viewOrigin);

	// transform by the camera placement
	VectorCopy( tr.viewParms.or.origin, origin );

	viewerMatrix[0] = tr.viewParms.or.axis[0][0];
	viewerMatrix[4] = tr.viewParms.or.axis[0][1];
	viewerMatrix[8] = tr.viewParms.or.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.or.axis[1][0];
	viewerMatrix[5] = tr.viewParms.or.axis[1][1];
	viewerMatrix[9] = tr.viewParms.or.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.or.axis[2][0];
	viewerMatrix[6] = tr.viewParms.or.axis[2][1];
	viewerMatrix[10] = tr.viewParms.or.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.or.modelMatrix );

	tr.viewParms.world = tr.or;

}

/*
** SetFarClip
*/
static void SetFarClip( void )
{
	float	farthestCornerDistance = 0;
	int		i;

	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048.0f;
		return;
	}

	//
	// set far clipping planes dynamically
	//
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		float distance;

		if ( i & 1 )
		{
			v[0] = tr.viewParms.visBounds[0][0];
		}
		else
		{
			v[0] = tr.viewParms.visBounds[1][0];
		}

		if ( i & 2 )
		{
			v[1] = tr.viewParms.visBounds[0][1];
		}
		else
		{
			v[1] = tr.viewParms.visBounds[1][1];
		}

		if ( i & 4 )
		{
			v[2] = tr.viewParms.visBounds[0][2];
		}
		else
		{
			v[2] = tr.viewParms.visBounds[1][2];
		}

		distance = DistanceSquared(tr.viewParms.or.origin, v);

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}
	// Bring in the zFar to the distanceCull distance
	// The sky renders at zFar so need to move it out a little
	// ...and make sure there is a minimum zfar to prevent problems
	tr.viewParms.zFar = Com_Clamp(2048.0f, tr.distanceCull * (1.732), sqrtf( farthestCornerDistance ));

	/*
	if (r_shadows->integer == 2)
	{ //volume caps need an "infinite" far clipping plane. So I'm using this semi-arbitrary massive number.
		tr.viewParms.zFar = 524288.0f;
	}
	*/
}


/*
===============
R_SetupProjection
===============
*/
void R_SetupProjection( void ) {
	float	xmin, xmax, ymin, ymax;
	float	width, height, depth;
	float	zNear, zFar;

	// dynamically compute far clip plane distance
	SetFarClip();

	//
	// set up projection matrix
	//
	zNear	= r_znear->value;
	zFar	= tr.viewParms.zFar;

	ymax = zNear * tan( tr.refdef.fov_y * M_PI / 360.0f );
	ymin = -ymax;

	xmax = zNear * tan( tr.refdef.fov_x * M_PI / 360.0f );
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zFar - zNear;

#if defined (_XBOX)
	tr.viewParms.projectionMatrix[0] = 2 * zNear / width;
	tr.viewParms.projectionMatrix[4] = 0;
	tr.viewParms.projectionMatrix[8] = ( xmax + xmin ) / width;	// normally 0
	tr.viewParms.projectionMatrix[12] = 0;

	tr.viewParms.projectionMatrix[1] = 0;
	tr.viewParms.projectionMatrix[5] = 2 * zNear / height;
	tr.viewParms.projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	tr.viewParms.projectionMatrix[13] = 0;

	tr.viewParms.projectionMatrix[2] = 0;
	tr.viewParms.projectionMatrix[6] = 0;
	tr.viewParms.projectionMatrix[10] = ( zFar + zNear ) / depth;
	tr.viewParms.projectionMatrix[14] = -2 * zFar * zNear / depth;

	tr.viewParms.projectionMatrix[3] = 0;
	tr.viewParms.projectionMatrix[7] = 0;
	tr.viewParms.projectionMatrix[11] = 1;
	tr.viewParms.projectionMatrix[15] = 0;
#else
	tr.viewParms.projectionMatrix[0] = 2 * zNear / width;
	tr.viewParms.projectionMatrix[4] = 0;
	tr.viewParms.projectionMatrix[8] = ( xmax + xmin ) / width;	// normally 0
	tr.viewParms.projectionMatrix[12] = 0;

	tr.viewParms.projectionMatrix[1] = 0;
	tr.viewParms.projectionMatrix[5] = 2 * zNear / height;
	tr.viewParms.projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	tr.viewParms.projectionMatrix[13] = 0;

	tr.viewParms.projectionMatrix[2] = 0;
	tr.viewParms.projectionMatrix[6] = 0;
	tr.viewParms.projectionMatrix[10] = -( zFar + zNear ) / depth;
	tr.viewParms.projectionMatrix[14] = -2 * zFar * zNear / depth;

	tr.viewParms.projectionMatrix[3] = 0;
	tr.viewParms.projectionMatrix[7] = 0;
	tr.viewParms.projectionMatrix[11] = -1;
	tr.viewParms.projectionMatrix[15] = 0;
#endif
}

/*
=================
R_SetupFrustum

Setup that culling frustum planes for the current view
=================
*/
void R_SetupFrustum (void) {
	int		i;
	float	xs, xc;
	float	ang;

	ang = tr.viewParms.fovX / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.or.axis[0], xs, tr.viewParms.frustum[0].normal );
	VectorMA( tr.viewParms.frustum[0].normal, xc, tr.viewParms.or.axis[1], tr.viewParms.frustum[0].normal );

	VectorScale( tr.viewParms.or.axis[0], xs, tr.viewParms.frustum[1].normal );
	VectorMA( tr.viewParms.frustum[1].normal, -xc, tr.viewParms.or.axis[1], tr.viewParms.frustum[1].normal );

	ang = tr.viewParms.fovY / 180 * M_PI * 0.5f;
	xs = sin( ang );
	xc = cos( ang );

	VectorScale( tr.viewParms.or.axis[0], xs, tr.viewParms.frustum[2].normal );
	VectorMA( tr.viewParms.frustum[2].normal, xc, tr.viewParms.or.axis[2], tr.viewParms.frustum[2].normal );

	VectorScale( tr.viewParms.or.axis[0], xs, tr.viewParms.frustum[3].normal );
	VectorMA( tr.viewParms.frustum[3].normal, -xc, tr.viewParms.or.axis[2], tr.viewParms.frustum[3].normal );

	for (i=0 ; i<4 ; i++) {
		tr.viewParms.frustum[i].type = PLANE_NON_AXIAL;
		tr.viewParms.frustum[i].dist = DotProduct (tr.viewParms.or.origin, tr.viewParms.frustum[i].normal);
		SetPlaneSignbits( &tr.viewParms.frustum[i] );
	}
}


/*
=================
R_MirrorPoint
=================
*/
void R_MirrorPoint (vec3_t in, orientation_t *surface, orientation_t *camera, vec3_t out) {
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(local, surface->axis[i]);
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}

void R_MirrorVector (vec3_t in, orientation_t *surface, orientation_t *camera, vec3_t out) {
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(in, surface->axis[i]);
		VectorMA( out, d, camera->axis[i], out );
	}
}


/*
=============
R_PlaneForSurface
=============
*/
void R_PlaneForSurface (surfaceType_t *surfType, cplane_t *plane) {
	srfTriangles_t	*tri;
	srfPoly_t		*poly;
	drawVert_t		*v1, *v2, *v3;
	vec4_t			plane4;

	if (!surfType) {
		memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType) {
	case SF_FACE:
		*plane = ((srfSurfaceFace_t *)surfType)->plane;
		return;
	case SF_TRIANGLES:
		tri = (srfTriangles_t *)surfType;
		v1 = tri->verts + tri->indexes[0];
		v2 = tri->verts + tri->indexes[1];
		v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	case SF_POLY:
		poly = (srfPoly_t *)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	default:
		memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;		
		return;
	}
}

/*
=================
R_GetPortalOrientation

entityNum is the entity that the portal surface is a part of, which may
be moving and rotating.

Returns qtrue if it should be mirrored
=================
*/
qboolean R_GetPortalOrientations( drawSurf_t *drawSurf, int entityNum, 
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, qboolean *mirror ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		transformed;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != TR_WORLDENT ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		STEFX_RETAIL_SCOPE R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.or );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.or.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.or.origin );
	} else {
		plane = originalPlane;
	}

	VectorCopy( plane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// get the pvsOrigin from the entity
		VectorCopy( e->e.oldorigin, pvsOrigin );

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*mirror = qtrue;
			return qtrue;
		}

		// project the origin onto the surface plane to get
		// an origin point we can rotate around
		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );
			
		// now get the camera origin and orientation
		VectorCopy( e->e.oldorigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		// optionally rotate
		if ( e->e.oldframe ) {
			// if a speed is specified
			if ( e->e.frame ) {
				// continuous rotate
				d = (tr.refdef.time/1000.0f) * e->e.frame;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			} else {
				// bobbing rotate, with skinNum being the rotation offset
				d = sin( tr.refdef.time * 0.003f );
				d = e->e.skinNum + d * 4;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			}
		}
		else if ( e->e.skinNum ) {
			d = e->e.skinNum;
			VectorCopy( camera->axis[1], transformed );
			RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
			CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
		}
		*mirror = qfalse;
		return qtrue;
	}

	// if we didn't locate a portal entity, don't render anything.
	// We don't want to just treat it as a mirror, because without a
	// portal entity the server won't have communicated a proper entity set
	// in the snapshot

	// unfortunately, with local movement prediction it is easily possible
	// to see a surface before the server has communicated the matching
	// portal surface entity, so we don't want to print anything here...

	//Com_Printf ("Portal surface without a portal entity\n" );

	return qfalse;
}

static qboolean IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != TR_WORLDENT ) 
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		STEFX_RETAIL_SCOPE R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.or );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.or.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.or.origin );
	} 
	else 
	{
		plane = originalPlane;
	}

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) 
		{
			return qtrue;
		}

		return qfalse;
	}
	return qfalse;
}
#ifndef DEDICATED
/*
** SurfIsOffscreen
**
** Determines if a surface is completely offscreen.
*/
static qboolean SurfIsOffscreen( const drawSurf_t *drawSurf, vec4_t clipDest[128] ) {
	float shortest = 100000000;
	int entityNum;
	int numTriangles;
	shader_t *shader;
	int		fogNum;
	int dlighted;
	vec4_t clip, eye;
	int i;
	unsigned int pointOr = 0;
	unsigned int pointAnd = (unsigned int)~0;

	R_RotateForViewer();

	STEFX_RETAIL_SCOPE R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
	STEFX_RETAIL_SCOPE RB_BeginSurface( shader, fogNum );
	rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	assert( tess.numVertexes < 128 );
	for ( i = 0; i < tess.numVertexes; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( tess.xyz[i], tr.or.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
		pointOr |= pointFlags;
	}

	// trivially reject
	if ( pointAnd )
	{
		return qtrue;
	}

	// determine if this surface is backfaced and also determine the distance
	// to the nearest vertex so we can cull based on portal range.  Culling
	// based on vertex distance isn't 100% correct (we should be checking for
	// range to the surface), but it's good enough for the types of portals
	// we have in the game right now.
	numTriangles = tess.numIndexes / 3;

	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		vec3_t normal;
		float dot;
		float len;

		VectorSubtract( tess.xyz[tess.indexes[i]], tr.viewParms.or.origin, normal );

		len = VectorLengthSquared( normal );			// lose the sqrt
		if ( len < shortest )
		{
			shortest = len;
		}

		if ( ( dot = DotProduct( normal, tess.normal[tess.indexes[i]] ) ) >= 0 )
		{
			numTriangles--;
		}
	}
	if ( !numTriangles )
	{
		return qtrue;
	}

	// mirrors can early out at this point, since we don't do a fade over distance
	// with them (although we could)
	if ( IsMirror( drawSurf, entityNum ) )
	{
		return qfalse;
	}

	if ( shortest > (tess.shader->portalRange*tess.shader->portalRange) )
	{
		return qtrue;
	}

	return qfalse;
}

#endif // DEDICATED
/*
========================
R_MirrorViewBySurface

Returns qtrue if another view has been rendered
========================
*/
qboolean R_MirrorViewBySurface (drawSurf_t *drawSurf, int entityNum) {
	vec4_t			clipDest[128];
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;

	// don't recursively mirror
	if (tr.viewParms.isPortal) {
		Com_DPrintf (S_COLOR_RED "WARNING: recursive mirror/portal found\n" );
		return qfalse;
	}

	if ( r_noportals->integer || (r_fastsky->integer == 1) ) {
		return qfalse;
	}

	// trivially reject portal/mirror
	if ( SurfIsOffscreen( drawSurf, clipDest ) ) {
		return qfalse;
	}

	// save old viewParms so we can return to it after the mirror view
	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.isPortal = qtrue;
	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera, 
		newParms.pvsOrigin, &newParms.isMirror ) ) {
		return qfalse;		// bad portal, no portalentity
	}

	R_MirrorPoint (oldParms.or.origin, &surface, &camera, newParms.or.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );
	
	R_MirrorVector (oldParms.or.axis[0], &surface, &camera, newParms.or.axis[0]);
	R_MirrorVector (oldParms.or.axis[1], &surface, &camera, newParms.or.axis[1]);
	R_MirrorVector (oldParms.or.axis[2], &surface, &camera, newParms.or.axis[2]);

	// OPTIMIZE: restrict the viewport on the mirrored view

	// render the mirror view
	STEFX_RETAIL_SCOPE R_RenderView (&newParms);

	tr.viewParms = oldParms;

	return qtrue;
}

/*
=================
R_SpriteFogNum

See if a sprite is inside a fog volume
=================
*/
int R_SpriteFogNum( trRefEntity_t *ent ) {
	int				i, j;
	fog_t			*fog;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( ent->e.origin[j] - ent->e.radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( ent->e.origin[j] + ent->e.radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

/*
==========================================================================================

DRAWSURF SORTING

==========================================================================================
*/

/*
=================
qsort replacement

=================
*/
#define	SWAP_DRAW_SURF(a,b) temp=((int *)a)[0];((int *)a)[0]=((int *)b)[0];((int *)b)[0]=temp; temp=((int *)a)[1];((int *)a)[1]=((int *)b)[1];((int *)b)[1]=temp;

/* this parameter defines the cutoff between using quick sort and
   insertion sort for arrays; arrays with lengths shorter or equal to the
   below value use insertion sort */

#define CUTOFF 8            /* testing shows that this is good value */

static void shortsort( drawSurf_t *lo, drawSurf_t *hi ) {
    drawSurf_t	*p, *max;
	int			temp;

    while (hi > lo) {
        max = lo;
        for (p = lo + 1; p <= hi; p++ ) {
            if ( p->sort > max->sort ) {
                max = p;
            }
        }
        SWAP_DRAW_SURF(max, hi);
        hi--;
    }
}


/* sort the array between lo and hi (inclusive)
FIXME: this was lifted and modified from the microsoft lib source...
 */

void qsortFast (
    void *base,
    unsigned num,
    unsigned width
    )
{
    char *lo, *hi;              /* ends of sub-array currently sorting */
    char *mid;                  /* points to middle of subarray */
    char *loguy, *higuy;        /* traveling pointers for partition step */
    unsigned size;              /* size of the sub-array */
    char *lostk[30], *histk[30];
    int stkptr;                 /* stack for saving sub-array to be processed */
	int	temp;

	if ( sizeof(drawSurf_t) != 8 ) {
		Com_Error( ERR_DROP, "change SWAP_DRAW_SURF macro" );
	}

    /* Note: the number of stack entries required is no more than
       1 + log2(size), so 30 is sufficient for any array */

    if (num < 2 || width == 0)
        return;                 /* nothing to do */

    stkptr = 0;                 /* initialize stack */

    lo = (char *)base;
    hi = (char *)base + width * (num-1);        /* initialize limits */

    /* this entry point is for pseudo-recursion calling: setting
       lo and hi and jumping to here is like recursion, but stkptr is
       prserved, locals aren't, so we preserve stuff on the stack */
recurse:

    size = (hi - lo) / width + 1;        /* number of el's to sort */

    /* below a certain size, it is faster to use a O(n^2) sorting method */
    if (size <= CUTOFF) {
         shortsort((drawSurf_t *)lo, (drawSurf_t *)hi);
    }
    else {
        /* First we pick a partititioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the
           median of the values, but also that we select one fast.  Using
           the first one produces bad performace if the array is already
           sorted, so we use the middle one, which would require a very
           wierdly arranged array for worst case performance.  Testing shows
           that a median-of-three algorithm does not, in general, increase
           performance. */

        mid = lo + (size / 2) * width;      /* find middle element */
        SWAP_DRAW_SURF(mid, lo);               /* swap it to beginning of array */

        /* We now wish to partition the array into three pieces, one
           consisiting of elements <= partition element, one of elements
           equal to the parition element, and one of element >= to it.  This
           is done below; comments indicate conditions established at every
           step. */

        loguy = lo;
        higuy = hi + width;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
        for (;;) {
            /* lo <= loguy < hi, lo < higuy <= hi + 1,
               A[i] <= A[lo] for lo <= i <= loguy,
               A[i] >= A[lo] for higuy <= i <= hi */

            do  {
                loguy += width;
            } while (loguy <= hi &&  
				( ((drawSurf_t *)loguy)->sort <= ((drawSurf_t *)lo)->sort ) );

            /* lo < loguy <= hi+1, A[i] <= A[lo] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[lo] */

            do  {
                higuy -= width;
            } while (higuy > lo && 
				( ((drawSurf_t *)higuy)->sort >= ((drawSurf_t *)lo)->sort ) );

            /* lo-1 <= higuy <= hi, A[i] >= A[lo] for higuy < i <= hi,
               either higuy <= lo or A[higuy] < A[lo] */

            if (higuy < loguy)
                break;

            /* if loguy > hi or higuy <= lo, then we would have exited, so
               A[loguy] > A[lo], A[higuy] < A[lo],
               loguy < hi, highy > lo */

            SWAP_DRAW_SURF(loguy, higuy);

            /* A[loguy] < A[lo], A[higuy] > A[lo]; so condition at top
               of loop is re-established */
        }

        /*     A[i] >= A[lo] for higuy < i <= hi,
               A[i] <= A[lo] for lo <= i < loguy,
               higuy < loguy, lo <= higuy <= hi
           implying:
               A[i] >= A[lo] for loguy <= i <= hi,
               A[i] <= A[lo] for lo <= i <= higuy,
               A[i] = A[lo] for higuy < i < loguy */

        SWAP_DRAW_SURF(lo, higuy);     /* put partition element in place */

        /* OK, now we have the following:
              A[i] >= A[higuy] for loguy <= i <= hi,
              A[i] <= A[higuy] for lo <= i < higuy
              A[i] = A[lo] for higuy <= i < loguy    */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy-1] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

        if ( higuy - 1 - lo >= hi - loguy ) {
            if (lo + width < higuy) {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - width;
                ++stkptr;
            }                           /* save big recursion for later */

            if (loguy < hi) {
                lo = loguy;
                goto recurse;           /* do small recursion */
            }
        }
        else {
            if (loguy < hi) {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;               /* save big recursion for later */
            }

            if (lo + width < higuy) {
                hi = higuy - width;
                goto recurse;           /* do small recursion */
            }
        }
    }

    /* We have sorted the array, except for any pending sorts on the stack.
       Check if there are any, and do them. */

    --stkptr;
    if (stkptr >= 0) {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;           /* pop subarray from stack */
    }
    else
        return;                 /* all subarrays done */
}


//==========================================================================================

/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf( const surfaceType_t *surface, const shader_t *shader, 
				   int fogIndex, int dlightMap ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	if (STEFX_CoopSkipHiddenSurface(surface, shader)) return;
#endif
	int			index;

	// instead of checking for overflow, we just mask the index
	// so it wraps around
	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;
	// the sort data is packed into a single 32 bit value so it can be
	// compared quickly during the qsorting process
	tr.refdef.drawSurfs[index].sort = (shader->sortedIndex << QSORT_SHADERNUM_SHIFT) 
		| tr.shiftedEntityNum | ( fogIndex << QSORT_FOGNUM_SHIFT ) | (int)dlightMap;
	tr.refdef.drawSurfs[index].surface = (surfaceType_t *)surface;
	tr.refdef.numDrawSurfs++;
}

qboolean R_FogParmsMatch( int fog1, int fog2 )
{
	int i;
	for ( i = 0; i < 2; i++ )
	{
		if ( tr.world->fogs[fog1].parms.color[i] != tr.world->fogs[fog2].parms.color[i] )
		{
			return qfalse;
		}
	}
	return qtrue;
}

/*
=================
R_DecomposeSort
=================
*/
void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader, 
					 int *fogNum, int *dlightMap ) {
	*fogNum = ( sort >> QSORT_FOGNUM_SHIFT ) & 31;
	*shader = tr.sortedShaders[ ( sort >> QSORT_SHADERNUM_SHIFT ) & (MAX_SHADERS-1) ];
	*entityNum = ( sort >> QSORT_ENTITYNUM_SHIFT ) & (MAX_ENTITIES-1);
	*dlightMap = sort & 3;
}

/*
=================
R_SortDrawSurfs
=================
*/
void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader;
	int				fogNum;
	int				entityNum;
	int				dlighted;
	int				i;

	// it is possible for some views to not have any surfaces
	if ( numDrawSurfs < 1 ) {
		// we still need to add it for hyperspace cases
		STEFX_RETAIL_SCOPE R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
		return;
	}

	// if we overflowed MAX_DRAWSURFS, the drawsurfs
	// wrapped around in the buffer and we will be missing
	// the first surfaces, not the last ones
	if ( numDrawSurfs > MAX_DRAWSURFS ) {
		numDrawSurfs = MAX_DRAWSURFS;
#if defined(_DEBUG) && defined(_XBOX)
		Com_Printf(S_COLOR_RED"Draw surface overflow!  Tell Brian.\n");
#endif
	}

#ifndef _XBOX
	// sort the drawsurfs by sort type, then orientation, then shader
	qsortFast (drawSurfs, numDrawSurfs, sizeof(drawSurf_t) );
#endif

	// check for any pass through drawing, which
	// may cause another view to be rendered first
	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		STEFX_RETAIL_SCOPE R_DecomposeSort( (drawSurfs+i)->sort, &entityNum, &shader, &fogNum, &dlighted );

		if ( shader->sort > SS_PORTAL ) {
			break;
		}

		// no shader should ever have this sort type
		if ( shader->sort == SS_BAD ) {
			Com_Error (ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name );
		}

		// if the mirror was completely clipped away, we may need to check another surface
		if ( R_MirrorViewBySurface( (drawSurfs+i), entityNum) ) {
			// this is a debug option to see exactly what is being mirrored
			if ( r_portalOnly->integer ) {
				return;
			}
			break;		// only one mirror view at a time
		}
	}

#ifdef _XBOX
	qsortFast (drawSurfs, numDrawSurfs, sizeof(drawSurf_t) );
#endif

	STEFX_RETAIL_SCOPE R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
}

/*
=============
R_AddEntitySurfaces
=============
*/
void R_AddEntitySurfaces (void) {
	trRefEntity_t	*ent;
	shader_t		*shader;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned __int64 xboxEntityPhaseStart;
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	static int s_stefxSplitFirstPersonFilterBudget = 24;
	static int s_stefxSplitSelfModelFilterBudget = 24;
	static int s_stefxSplitPhaserWorldFilterBudget = 48;
	static int s_stefxSplitEffectCullLogBudget = 16;
	int stefxSplitOffscreenEffectsCulled = 0;
	int stefxSplitDistantEffectsCulled = 0;
	int stefxSplitDistantPickupsCulled = 0;
	qboolean stefxSplitEconomyActive = qfalse;
	qboolean stefxHolomatchSplitActive = qfalse;
	qboolean stefxSplitSelfFilterActive = qfalse;
#endif

	if ( !r_drawentities->integer ) {
		return;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean stefxSpOffscreenFxCull = qfalse;
	{
		const int splitPlayers = Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" );
		const qboolean splitActive = Cvar_VariableIntegerValue( "stefx_splitScreen" ) && splitPlayers >= 2 ? qtrue : qfalse;
		const char *splitMode = splitActive ? Cvar_VariableString( "stefx_splitScreenMode" ) : NULL;

		stefxSplitEconomyActive = tr.viewParms.stefxSplitEconomy;
		// The wholly-offscreen effect cull below was validated for split-screen
		// (880 -> 152 sunny_flare batches).  Extend the same frustum-only test
		// to ordinary one-player views; the split-only distance tier is not
		// extended, so nothing visible on screen changes.
		stefxSpOffscreenFxCull = ( !splitActive &&
			Cvar_VariableIntegerValue( "r_efOffscreenFxCull" ) ) ? qtrue : qfalse;
#if defined(STEFX_SP_HOSTED_MP)
		stefxHolomatchSplitActive = splitActive && splitMode && !Q_stricmp( splitMode, "holomatch" ) ? qtrue : qfalse;
		stefxSplitSelfFilterActive = stefxHolomatchSplitActive && splitPlayers >= 2 ? qtrue : qfalse;
#else
		(void)splitMode;
#endif
	}
#endif

	for ( tr.currentEntityNum = 0; 
	      tr.currentEntityNum < tr.refdef.num_entities; 
		  tr.currentEntityNum++ ) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	s_coopModelOutsidePvs = false;
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( R_STEFX_ShouldHideHolomatchSplitWorldPhaser( ent, stefxHolomatchSplitActive ) ) {
			const unsigned int proofSlot = g_SPXBSplitSlotActive - 1;
			++g_SPXBHMSplitPhaserWorldHidden[proofSlot];
			if ( s_stefxSplitPhaserWorldFilterBudget > 0 ) {
				XBLog_WriteCriticalf( "STEFX_HM_SPLIT_PHASER_FILTER: slot=%u owner=%d entity=%d type=%d start=(%g,%g,%g) end=(%g,%g,%g)",
					proofSlot,
					ent->e.number - STEFX_REFENTITY_PHASER_BEAM_BASE,
					tr.currentEntityNum,
					ent->e.reType,
					ent->e.origin[0], ent->e.origin[1], ent->e.origin[2],
					ent->e.oldorigin[0], ent->e.oldorigin[1], ent->e.oldorigin[2] );
				--s_stefxSplitPhaserWorldFilterBudget;
			}
			continue;
		}
		if ( ( ent->e.renderfx & RF_STEFX_SPLIT_HIDE_SLOT0 ) && g_SPXBSplitSlotActive == 1 ) {
			continue;
		}
		if ( ( ent->e.renderfx & RF_STEFX_SPLIT_HIDE_SLOT1 ) && g_SPXBSplitSlotActive == 2 ) {
			continue;
		}
		{
			float selfXyDistance;
			float selfZDelta;
			const char *selfModelPart;
			if ( R_STEFX_ShouldHideHolomatchSplitSelfModel( ent, stefxSplitSelfFilterActive, &selfXyDistance, &selfZDelta, &selfModelPart ) ) {
				if ( g_SPXBSplitSlotActive > 0 && g_SPXBSplitSlotActive <= 4 ) {
					const unsigned int proofSlot = g_SPXBSplitSlotActive - 1;
					unsigned int partCode = 0;
					if ( selfModelPart && !Q_stricmp( selfModelPart, "lower" ) ) {
						partCode = 1;
					}
					else if ( selfModelPart && !Q_stricmp( selfModelPart, "upper" ) ) {
						partCode = 2;
					}
					else if ( selfModelPart && !Q_stricmp( selfModelPart, "head" ) ) {
						partCode = 3;
					}
					else if ( selfModelPart && !Q_stricmp( selfModelPart, "local" ) ) {
						partCode = 4;
					}
					g_SPXBHMSplitSelfFilterMask |= ( 1u << proofSlot );
					g_SPXBHMSplitSelfFilterRefNumber[proofSlot] = (unsigned int)ent->e.number;
					g_SPXBHMSplitSelfFilterPart[proofSlot] = partCode;
				}
				if ( s_stefxSplitSelfModelFilterBudget > 0 ) {
					XBLog_WriteCriticalf( "STEFX_HM_SPLIT_SELF_FILTER: slot=%u entity=%d refNumber=%d renderfx=0x%x hModel=%d modelPart=%s origin=(%g,%g,%g) view=(%g,%g,%g) xyDist=%g zDelta=%g",
						g_SPXBSplitSlotActive - 1,
						tr.currentEntityNum,
						ent->e.number,
						ent->e.renderfx,
						ent->e.hModel,
						selfModelPart,
						ent->e.origin[0],
						ent->e.origin[1],
						ent->e.origin[2],
						tr.refdef.vieworg[0],
						tr.refdef.vieworg[1],
						tr.refdef.vieworg[2],
						selfXyDistance,
						selfZDelta );
					--s_stefxSplitSelfModelFilterBudget;
				}
				continue;
			}
		}
#endif

		assert(ent->e.renderfx >= 0);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		{
			const int firstPersonSplitSlot = R_STEFX_FirstPersonSplitSlotForRenderfx( ent->e.renderfx );
			if ( firstPersonSplitSlot > 0 && g_SPXBSplitSlotActive > 0 && firstPersonSplitSlot != (int)g_SPXBSplitSlotActive ) {
				if ( g_SPXBSplitSlotActive <= 4 ) {
					g_SPXBHMSplitFPFilterMask |= ( 1u << ( g_SPXBSplitSlotActive - 1 ) );
				}
				if ( s_stefxSplitFirstPersonFilterBudget > 0 ) {
					XBLog_WriteCriticalf( "STEFX_HM_SPLIT_FP_FILTER: slot=%u entity=%d renderfx=0x%x hModel=%d ownerSlot=%d",
						g_SPXBSplitSlotActive - 1,
						tr.currentEntityNum,
						ent->e.renderfx,
						ent->e.hModel,
						firstPersonSplitSlot - 1 );
					--s_stefxSplitFirstPersonFilterBudget;
				}
				continue;
			}
		}
#endif

		ent->needDlights = qfalse;

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		if (tr.viewParms.stefxSplitView && STEFX_FxCullEnabled() &&
			STEFX_CullExtendedFx(ent->e)) continue;
		if (STEFX_CullFxOutsidePvs(ent->e)) continue;
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( ( ( tr.viewParms.stefxSplitView && stefxSplitEconomyActive ) ||
				stefxSpOffscreenFxCull ) &&
			( ent->e.reType == RT_SPRITE || ent->e.reType == RT_ORIENTED_QUAD
#if defined(STEFX_SP_HOSTED_MP)
				|| ent->e.reType == RT_EF_ORIENTED_SPRITE
#endif
			) )
		{
			if (
#if defined(STEFX_SP_HOSTED_MP)
				!STEFX_FxCullEnabled() &&
#endif
				R_CullPointAndRadius( ent->e.origin,
				ent->e.radius > 0.0f ? ent->e.radius : 1.0f ) == CULL_OUT )
			{
				++stefxSplitOffscreenEffectsCulled;
				continue;
			}
			if ( tr.viewParms.stefxSplitThreePlusEconomy &&
				DistanceSquared( ent->e.origin, tr.refdef.vieworg ) >
					512.0f * 512.0f )
			{
				++stefxSplitDistantEffectsCulled;
				continue;
			}
		}
#endif

		// preshift the value we are going to OR into the drawsurf sort
		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in 
		// mirrors, because the true body position will already be drawn
		//
		if ( (ent->e.renderfx & RF_FIRST_PERSON) && tr.viewParms.isPortal) {
			continue;
		}

		// simple generated models, like sprites and beams, are not culled
		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;		// don't draw anything
		case RT_SPRITE:
		case RT_BEAM:
		case RT_ORIENTED_QUAD:
		case RT_LATHE:
		case RT_CLOUDS:
		case RT_ELECTRICITY:
		case RT_LINE:
#if defined(STEFX_SP_HOSTED_MP)
		case RT_TEXTURED_LINE:
		case RT_ORIENTED_LINE:
		case RT_TAPERED_LINE:
		case RT_BEZIER:
		case RT_EF_ORIENTED_SPRITE:
		case RT_EF_ALPHA_VERT_POLY:
		case RT_EF_LIGHTNING:
		case RT_EF_CYLINDER:
		case RT_EF_ELECTRICITY:
#endif
		case RT_CYLINDER:
		case RT_SABER_GLOW:
			// self blood sprites, talk balloons, etc should not be drawn in the primary
			// view.  We can't just do this check for all entities, because md3
			// entities may still want to cast shadows from them
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal) {
				continue;
			}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			xboxEntityPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader, R_SpriteFogNum( ent ), 0 );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if ( g_SPXBPerfSampleActive ) {
				g_SPXBPerfEntitySimpleCycles += STEFX_XboxElapsedCycles( xboxEntityPhaseStart );
				++g_SPXBPerfEntitySimpleCalls;
			}
#endif
			break;

		case RT_MODEL:
			// we must set up parts of tr.or for model culling
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			xboxEntityPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
			STEFX_RETAIL_SCOPE R_RotateForEntity( ent, &tr.viewParms, &tr.or );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	s_coopModelOutsidePvs = STEFX_CoopModelOutsidePvs(ent, tr.currentModel);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
			if ( tr.viewParms.stefxSplitThreePlusEconomy && tr.currentModel &&
				!Q_stricmpn( tr.currentModel->name, "models/powerups/trek/", 21 ) &&
				DistanceSquared( ent->e.origin, tr.refdef.vieworg ) >
					640.0f * 640.0f )
			{
				++stefxSplitDistantPickupsCulled;
				continue;
			}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if ( g_SPXBPerfSampleActive ) {
				g_SPXBPerfEntityModelSetupCycles += STEFX_XboxElapsedCycles( xboxEntityPhaseStart );
				++g_SPXBPerfEntityModelSetupCalls;
			}
#endif
			if (!tr.currentModel) {
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_MESH:
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					xboxEntityPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
					R_AddMD3Surfaces( ent );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					if ( g_SPXBPerfSampleActive ) {
						g_SPXBPerfEntityMeshCycles += STEFX_XboxElapsedCycles( xboxEntityPhaseStart );
						++g_SPXBPerfEntityMeshCalls;
					}
#endif
					break;
				case MOD_BRUSH:
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					xboxEntityPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
					STEFX_RETAIL_SCOPE R_AddBrushModelSurfaces( ent );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					if ( g_SPXBPerfSampleActive ) {
						g_SPXBPerfEntityBrushCycles += STEFX_XboxElapsedCycles( xboxEntityPhaseStart );
						++g_SPXBPerfEntityBrushCalls;
					}
#endif
					break;
#ifdef STEFX_ELITE_FORCE_SP
				case MOD_MDR:
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					xboxEntityPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
					R_AddAnimSurfaces( ent );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
					if ( g_SPXBPerfSampleActive ) {
						g_SPXBPerfEntityAnimCycles += STEFX_XboxElapsedCycles( xboxEntityPhaseStart );
						++g_SPXBPerfEntityAnimCalls;
					}
#endif
					break;
				case MOD_STEFX_MDR_PLACEHOLDER:
					break;
#endif
/*
Ghoul2 Insert Start
*/
				case MOD_MDXM:
  					//g2r
					if (ent->e.ghoul2)
					{
						R_AddGhoulSurfaces( ent);
					}
  					break;
				case MOD_BAD:		// null model axis
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal)
					{
						if (!(ent->e.renderfx & RF_SHADOW_ONLY))
						{
							break;
						}
					}

  					if (ent->e.ghoul2 && G2API_HaveWeGhoul2Models(*((CGhoul2Info_v *)ent->e.ghoul2)))
  					{
  						R_AddGhoulSurfaces( ent);
  						break;
  					}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && !defined(STEFX_SP_HOSTED_MP)
					STEFX_RecordMissingModel(ent);
#endif
					R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, false );
					break;
/*
Ghoul2 Insert End
*/
				default:
					Com_Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;

		default:
			Com_Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#if !defined(STEFX_SP_HOSTED_MP)
	s_coopModelOutsidePvs = false;
#endif
	if ( ( stefxSplitOffscreenEffectsCulled > 0 ||
		stefxSplitDistantEffectsCulled > 0 ||
		stefxSplitDistantPickupsCulled > 0 ) &&
		s_stefxSplitEffectCullLogBudget > 0 )
	{
		XBLog_WriteCriticalf( "STEFX_SPLIT_ECONOMY: offscreenEffectsCulled=%d distantEffectsCulled=%d distantPickupsCulled=%d effectDistance=512 pickupDistance=640 slot=%u",
			stefxSplitOffscreenEffectsCulled,
			stefxSplitDistantEffectsCulled,
			stefxSplitDistantPickupsCulled,
			g_SPXBSplitSlotActive > 0 ? g_SPXBSplitSlotActive - 1 : 0 );
		--s_stefxSplitEffectCullLogBudget;
	}
#endif

}


/*
====================
R_GenerateDrawSurfs
====================
*/
#ifdef _XBOX
extern void R_MarkLeaves(mleaf_s*);
void R_GenerateDrawSurfs( bool isPortal ) {
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned __int64 xboxPhaseStart;
#endif
	// determine which leaves are in the PVS / areamask
	if ( !(tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
		R_MarkLeaves (NULL);
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if ( g_SPXBPerfSampleActive ) {
			g_SPXBPerfRenderMarkLeavesMsec += STEFX_XboxCyclesToMsec(
				STEFX_XboxReadTsc() - xboxPhaseStart );
		}
#endif
	}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
	R_AddWorldSurfaces ();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderWorldMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif

	R_AddPolygonSurfaces();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderPolysMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
#endif

/*
	R_AddTerrainSurfaces();
*/

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
	R_SetupProjection ();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderProjectionMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif

	R_AddEntitySurfaces ();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderEntitiesMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
#endif
}

#else 

void R_GenerateDrawSurfs( void ) {
	R_AddWorldSurfaces ();

	R_AddPolygonSurfaces();

/*
	R_AddTerrainSurfaces(); //rwwRMG - added
*/

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation
	R_SetupProjection ();

	R_AddEntitySurfaces ();
}

#endif

/*
================
R_DebugPolygon
================
*/
void R_DebugPolygon( int color, int numPoints, float *points ) {
	int		i;

	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	// draw solid shade

	qglColor3f( color&1, (color>>1)&1, (color>>2)&1 );
	qglBegin( GL_POLYGON );
	for ( i = 0 ; i < numPoints ; i++ ) {
		qglVertex3fv( points + i * 3 );
	}
	qglEnd();

	// draw wireframe outline
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	qglDepthRange( 0, 0 );
	qglColor3f( 1, 1, 1 );
	qglBegin( GL_POLYGON );
	for ( i = 0 ; i < numPoints ; i++ ) {
		qglVertex3fv( points + i * 3 );
	}
	qglEnd();
	qglDepthRange( 0, 1 );
}

/*
====================
R_DebugGraphics

Visualization aid for movement clipping debugging
====================
*/
void R_DebugGraphics( void ) {
	if ( !r_debugSurface->integer ) {
		return;
	}

	// the render thread can't make callbacks to the main thread
	R_SyncRenderThread();

	STEFX_RETAIL_SCOPE GL_Bind( tr.whiteImage);
	STEFX_RETAIL_SCOPE GL_Cull( CT_FRONT_SIDED );
	CM_DrawDebugSurface( R_DebugPolygon );
}


/*
================
R_RenderView

A view may be either the actual camera view,
or a mirror / remote location
================
*/
void R_RenderView (viewParms_t *parms) {
	int		firstDrawSurf;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned __int64 xboxPhaseStart;
#endif

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		++g_SPXBPerfRenderViews;
		if ( parms->isPortal ) {
			++g_SPXBPerfRenderPortals;
		}
	}
#endif

	tr.viewCount++;

	// set viewParms.world
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
	R_RotateForViewer ();

	R_SetupFrustum ();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderSetupMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
#endif

#ifdef _XBOX
	R_GenerateDrawSurfs(parms->isPortal);
	if (!parms->isPortal) {
		g_SPXBHMPvsProbe[66] = *(unsigned int *)&tr.viewParms.zFar;
		g_SPXBHMPvsProbe[67] = *(unsigned int *)&tr.viewParms.fovX;
		g_SPXBHMPvsProbe[68] = *(unsigned int *)&tr.viewParms.fovY;
		g_SPXBHMPvsProbe[69] = *(unsigned int *)&tr.viewParms.or.origin[0];
		g_SPXBHMPvsProbe[70] = *(unsigned int *)&tr.viewParms.or.origin[1];
		g_SPXBHMPvsProbe[71] = *(unsigned int *)&tr.viewParms.or.origin[2];
	}
#else
	R_GenerateDrawSurfs();
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif
	R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf, tr.refdef.numDrawSurfs - firstDrawSurf );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderSortMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
	}
	xboxPhaseStart = g_SPXBPerfSampleActive ? STEFX_XboxReadTsc() : 0;
#endif

	// draw main system development information (surface outlines, etc)
	R_DebugGraphics();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if ( g_SPXBPerfSampleActive ) {
		g_SPXBPerfRenderDebugMsec += STEFX_XboxCyclesToMsec(
			STEFX_XboxReadTsc() - xboxPhaseStart );
		g_SPXBPerfRenderDrawSurfs +=
			(unsigned int)( tr.refdef.numDrawSurfs - firstDrawSurf );
	}
#endif
}

STEFX_RETAIL_NAMESPACE_END

#endif // !DEDICATED
