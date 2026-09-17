// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"


#include "tr_local.h"
#include "MatComp.h"
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
#include <xmmintrin.h>
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#include "../win32/xb_log.h"
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
#include "../win32/xb_perf.h"
#endif
extern int R_ComputeLOD( trRefEntity_t *ent );

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the 
orientation of the bone in the base frame to the orientation in this
frame.

*/


/*
=============
R_ACullModel
=============
*/
static int R_ACullModel( md4Header_t *header, trRefEntity_t *ent ) {
	vec3_t		bounds[2];
	md4Frame_t	*oldFrame, *newFrame;
	int			i;
	int			frameSize;
	// compute frame pointers

	if (header->ofsFrames<0) // Compressed
	{
		frameSize = (int)( &((md4CompFrame_t *)0)->bones[ tr.currentModel->md4->numBones ] );		
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		newFrame = (md4Frame_t *)R_STEFX_GetMDRFrame(header, ent->e.frame);
		oldFrame = (md4Frame_t *)R_STEFX_GetMDRFrame(header, ent->e.oldframe);
#else
		newFrame = (md4Frame_t *)((byte *)header - header->ofsFrames + ent->e.frame * frameSize );
		oldFrame = (md4Frame_t *)((byte *)header - header->ofsFrames + ent->e.oldframe * frameSize );
#endif
		// HACK! These frames actually are md4CompFrames, but the first fields are the same, 
		// so this will work for this routine.
	}
	else
	{
		frameSize = (int)( &((md4Frame_t *)0)->bones[ tr.currentModel->md4->numBones ] );		
		newFrame = (md4Frame_t *)((byte *)header + header->ofsFrames + ent->e.frame * frameSize );
		oldFrame = (md4Frame_t *)((byte *)header + header->ofsFrames + ent->e.oldframe * frameSize );
	}

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;

			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;

			case CULL_CLIP:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			}
		}
		else
		{
			int sphereCull, sphereCullB;

			sphereCull  = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			if ( newFrame == oldFrame ) {
				sphereCullB = sphereCull;
			} else {
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == CULL_OUT )
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				else if ( sphereCull == CULL_IN )
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
	}
	
	// calculate a bounding box in the current coordinate system
	for (i = 0 ; i < 3 ; i++) {
		bounds[0][i] = oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
	}

	switch ( R_CullLocalBox( bounds ) )
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static int R_STEFX_ACullModel( md4Header_t *header, trRefEntity_t *ent )
{
	int cull;

	cull = R_ACullModel( header, ent );
	if ( cull != CULL_OUT )
	{
		return cull;
	}

	vec3_t center;
	VectorCopy( ent->e.origin, center );
	center[2] += 32.0f;
	cull = R_CullPointAndRadius( center, 128.0f );
	if ( cull == CULL_OUT )
	{
		return CULL_OUT;
	}
	return CULL_CLIP;
}
#endif


/*
=================
R_AComputeFogNum

=================
*/
static int R_AComputeFogNum( md4Header_t *header, trRefEntity_t *ent ) {
	int				i;
	fog_t			*fog;
	md4Frame_t		*frame;
	vec3_t			localOrigin;
	int				frameSize;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}


	if (header->ofsFrames<0) // Compressed
	{
		frameSize = (int)( &((md4CompFrame_t *)0)->bones[ header->numBones ] );		
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		frame = (md4Frame_t *)R_STEFX_GetMDRFrame(header, ent->e.frame);
#else
		frame = (md4Frame_t *)((byte *)header - header->ofsFrames + ent->e.frame * frameSize );
#endif
		// HACK! These frames actually are md4CompFrames, but the first fields are the same, 
		// so this will work for this routine.
	}
	else
	{
		frameSize = (int)( &((md4Frame_t *)0)->bones[ header->numBones ] );		
		frame = (md4Frame_t *)((byte *)header + header->ofsFrames + ent->e.frame * frameSize );
	}

	VectorAdd( ent->e.origin, frame->localOrigin, localOrigin );
	int partialFog = 0;
	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		if ( localOrigin[0] - frame->radius >= fog->bounds[0][0] 
			&& localOrigin[0] + frame->radius <= fog->bounds[1][0] 
			&& localOrigin[1] - frame->radius >= fog->bounds[0][1]
			&& localOrigin[1] + frame->radius <= fog->bounds[1][1] 
			&& localOrigin[2] - frame->radius >= fog->bounds[0][2]
			&& localOrigin[2] + frame->radius <= fog->bounds[1][2] ) 
		{//totally inside it
			return i;
			break;
		}
		if ( ( localOrigin[0] - frame->radius >= fog->bounds[0][0] && localOrigin[1] - frame->radius >= fog->bounds[0][1] && localOrigin[2] - frame->radius >= fog->bounds[0][2] &&
			localOrigin[0] - frame->radius <= fog->bounds[1][0] && localOrigin[1] - frame->radius <= fog->bounds[1][1] && localOrigin[2] - frame->radius <= fog->bounds[1][2] ) || 
			( localOrigin[0] + frame->radius >= fog->bounds[0][0] && localOrigin[1] + frame->radius >= fog->bounds[0][1] && localOrigin[2] + frame->radius >= fog->bounds[0][2] &&
			localOrigin[0] + frame->radius <= fog->bounds[1][0] && localOrigin[1] + frame->radius <= fog->bounds[1][1] && localOrigin[2] + frame->radius <= fog->bounds[1][2] ) ) 
		{//partially inside it
			if ( tr.refdef.fogIndex == i || R_FogParmsMatch( tr.refdef.fogIndex, i ) )
			{//take new one only if it's the same one that the viewpoint is in
				return i;
				break;
			}
			else if ( !partialFog )
			{//first partialFog
				partialFog = i;
			}
		}
	}
	//if all else fails, return the first partialFog
	return partialFog;
}

/*
==============
R_AddAnimSurfaces
==============
*/
void R_AddAnimSurfaces( trRefEntity_t *ent ) {
	md4Header_t		*header;
	md4Surface_t	*surface;
	md4LOD_t		*lod;
	shader_t		*shader = 0;
	shader_t		*cust_shader = 0;
	int				fogNum = 0;
	qboolean		personalModel;
	int				cull;
	int				i, whichLod;
	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

	if ( ent->e.renderfx & RF_CAP_FRAMES) {
		if (ent->e.frame > tr.currentModel->md4->numFrames-1)
			ent->e.frame = tr.currentModel->md4->numFrames-1;
		if (ent->e.oldframe > tr.currentModel->md4->numFrames-1)
			ent->e.oldframe = tr.currentModel->md4->numFrames-1;
	}
	else if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		ent->e.frame %= tr.currentModel->md4->numFrames;
		ent->e.oldframe %= tr.currentModel->md4->numFrames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ( (ent->e.frame >= tr.currentModel->md4->numFrames) 
		|| (ent->e.frame < 0)
		|| (ent->e.oldframe >= tr.currentModel->md4->numFrames)
		|| (ent->e.oldframe < 0) ) 
	{
#ifdef _DEBUG
			VID_Printf (PRINT_ALL, "R_AddAnimSurfaces: no such frame %d to %d for '%s'\n",
#else
			VID_Printf (PRINT_DEVELOPER, "R_AddAnimSurfaces: no such frame %d to %d for '%s'\n",				
#endif
			ent->e.oldframe, ent->e.frame,
			tr.currentModel->name );
			ent->e.frame = 0;
			ent->e.oldframe = 0;
	}

	header = tr.currentModel->md4;

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	cull = R_STEFX_ACullModel( header, ent );
#else
	cull = R_ACullModel ( header, ent );
#endif
	if ( cull == CULL_OUT ) {
		return;
	}

	//
	// compute LOD
	//
	lod = (md4LOD_t *)( (byte *)header + header->ofsLODs );
	whichLod = R_ComputeLOD( ent );
	for ( i = 0; i < whichLod; i++)
	{
		lod = (md4LOD_t*)( (byte *)lod + lod->ofsEnd );
	}
	//
	// set up lighting now that we know we aren't culled
	//
	if ( !personalModel || r_shadows->integer > 1 ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_AComputeFogNum( header, ent );


	//
	// draw all surfaces
	//
	cust_shader = R_GetShaderByHandle( ent->e.customShader );


	surface = (md4Surface_t *)( (byte *)lod + lod->ofsSurfaces );
	for ( i = 0 ; i < lod->numSurfaces ; i++ ) {
		if ( ent->e.customShader ) {
			shader = cust_shader;
		} else if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) {
			skin_t *skin;
			int		j;
			
			skin = R_GetSkinByHandle( ent->e.customSkin );
			
			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
				// the names have both been lowercased
				if ( !strcmp( skin->surfaces[j]->name, surface->name ) ) {
					shader = skin->surfaces[j]->shader;
					break;
				}
			}
		} else {
			shader = R_GetShaderByHandle( surface->shaderIndex );
		}
		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if ( !personalModel
			&& r_shadows->integer == 2 
#ifndef VV_LIGHTING
			&& fogNum == 0
#endif
			&& (ent->e.renderfx & RF_SHADOW_PLANE )
			&& !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) ) 
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (surfaceType_t *)surface, tr.shadowShader, 0, qfalse );
		}

		// projection shadows work fine with personal models
		if ( r_shadows->integer == 3
			&& fogNum == 0
			&& (ent->e.renderfx & RF_SHADOW_PLANE )
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (surfaceType_t *)surface, tr.projectionShadowShader, 0, qfalse );
		}

		// don't add third_person objects if not viewing through a portal
		if ( !personalModel ) {
			R_AddDrawSurf( (surfaceType_t *)surface, shader, fogNum, qfalse );
		}

		surface = (md4Surface_t *)( (byte *)surface + surface->ofsEnd );
	}
}


/*
==============
RB_SurfaceAnim
==============
*/

#define STEFX_MDR_PALETTE_CACHE_SLOTS 16

typedef struct {
	qboolean			valid;
	qboolean			sharedIdentity;
	const trRefEntity_t	*entity;
	int				entityNumber;
	const md4Header_t	*header;
	int				frame;
	int				oldFrame;
	float				backlerp;
	int				renderTime;
	md4Bone_t			bones[MD4_MAX_BONES];
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	unsigned int generation;
#endif
} stefxMdrPaletteCache_t;

static stefxMdrPaletteCache_t s_stefxMdrPaletteCache[STEFX_MDR_PALETTE_CACHE_SLOTS];
static int s_stefxMdrPaletteReplacement;

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static unsigned int s_stefxMdrPaletteRequests;
static unsigned int s_stefxMdrPaletteGeneration;
static unsigned int s_stefxMdrPaletteCurrentGeneration;
static unsigned int s_stefxMdrPaletteBuilds;
static unsigned int s_stefxMdrPaletteReuses;
static unsigned int s_stefxMdrPaletteBypasses;
static unsigned int s_stefxMdrPaletteFrames;
static int s_stefxMdrPaletteLastRenderTime = -1;

static void RB_STEFX_ProfileMdrPaletteCache( void ) {
	if ( backEnd.refdef.time == s_stefxMdrPaletteLastRenderTime ) {
		return;
	}

	s_stefxMdrPaletteLastRenderTime = backEnd.refdef.time;
	++s_stefxMdrPaletteFrames;
	if ( ( s_stefxMdrPaletteFrames & 255u ) == 0u ) {
		const unsigned int reusePercent = s_stefxMdrPaletteRequests
			? ( 100u * s_stefxMdrPaletteReuses ) / s_stefxMdrPaletteRequests
			: 0u;
		XBLog_WriteProfile( va(
			"STEFX_HW_MDR_PALETTE_CACHE: sample=%u players=%d requests=%u emitted=%u skipped=%u skipPct=%u bypass=%u slots=%u",
			s_stefxMdrPaletteFrames,
			Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ),
			s_stefxMdrPaletteRequests, s_stefxMdrPaletteBuilds,
			s_stefxMdrPaletteReuses, reusePercent,
			s_stefxMdrPaletteBypasses,
			(unsigned int)STEFX_MDR_PALETTE_CACHE_SLOTS ) );
	}
}
#endif

static md4Bone_t *RB_GetAnimBonePalette( md4Header_t *header, float frontlerp, float backlerp,
	md4Frame_t *frame, md4Frame_t *oldFrame, md4CompFrame_t *cframe, md4CompFrame_t *coldFrame,
	qboolean compressed ) {
	stefxMdrPaletteCache_t *cache;
	qboolean sharedIdentity = qfalse;
	int i, j;
	md4Bone_t tbone[2];

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	RB_STEFX_ProfileMdrPaletteCache();
	sharedIdentity = backEnd.viewParms.stefxSplitThreePlusEconomy;
#endif

	if ( !backlerp && !compressed ) {
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		++s_stefxMdrPaletteBypasses;
		s_stefxMdrPaletteCurrentGeneration = 0; // immutable uncompressed frame
#endif
		return frame->bones;
	}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	++s_stefxMdrPaletteRequests;
#endif

	cache = NULL;
	for ( i = 0; i < STEFX_MDR_PALETTE_CACHE_SLOTS; ++i ) {
		stefxMdrPaletteCache_t *candidate = &s_stefxMdrPaletteCache[i];
		if ( candidate->valid &&
			candidate->sharedIdentity == sharedIdentity &&
			( sharedIdentity
				? candidate->entityNumber == backEnd.currentEntity->e.number
				: candidate->entity == backEnd.currentEntity ) &&
			candidate->header == header &&
			candidate->frame == backEnd.currentEntity->e.frame &&
			candidate->oldFrame == backEnd.currentEntity->e.oldframe &&
			candidate->backlerp == backlerp &&
			candidate->renderTime == backEnd.refdef.time ) {
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
			++s_stefxMdrPaletteReuses;
			s_stefxMdrPaletteCurrentGeneration = candidate->generation;
#endif
			return candidate->bones;
		}
		if ( !cache && ( !candidate->valid || candidate->renderTime != backEnd.refdef.time ) ) {
			cache = candidate;
		}
	}

	if ( !cache ) {
		cache = &s_stefxMdrPaletteCache[s_stefxMdrPaletteReplacement];
		s_stefxMdrPaletteReplacement = ( s_stefxMdrPaletteReplacement + 1 ) % STEFX_MDR_PALETTE_CACHE_SLOTS;
	}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	++s_stefxMdrPaletteBuilds;
	if (!++s_stefxMdrPaletteGeneration) ++s_stefxMdrPaletteGeneration;
	cache->generation = s_stefxMdrPaletteGeneration;
	s_stefxMdrPaletteCurrentGeneration = cache->generation;
#endif
	cache->valid = qtrue;
	cache->sharedIdentity = sharedIdentity;
	cache->entity = backEnd.currentEntity;
	cache->entityNumber = backEnd.currentEntity->e.number;
	cache->header = header;
	cache->frame = backEnd.currentEntity->e.frame;
	cache->oldFrame = backEnd.currentEntity->e.oldframe;
	cache->backlerp = backlerp;
	cache->renderTime = backEnd.refdef.time;

	if ( compressed ) {
		for ( i = 0; i < header->numBones; ++i ) {
			if ( !backlerp ) {
				MC_UnCompress( cache->bones[i].matrix, cframe->bones[i].Comp );
			} else {
				MC_UnCompress( tbone[0].matrix, cframe->bones[i].Comp );
				MC_UnCompress( tbone[1].matrix, coldFrame->bones[i].Comp );
				for ( j = 0; j < 12; ++j ) {
					((float *)&cache->bones[i])[j] = frontlerp * ((float *)&tbone[0])[j]
						+ backlerp * ((float *)&tbone[1])[j];
				}
			}
		}
	} else {
		for ( i = 0; i < header->numBones * 12; ++i ) {
			((float *)cache->bones)[i] = frontlerp * ((float *)frame->bones)[i]
				+ backlerp * ((float *)oldFrame->bones)[i];
		}
	}

	return cache->bones;
}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
struct stefxMdrPoseProof_t {
	const md4Surface_t *surface;
	int frame, oldFrame;
	float backlerp;
};
static stefxMdrPoseProof_t s_mdrPoseProof[256];
static unsigned int s_mdrProofSerial = 0xffffffffu, s_mdrProofReported = 0xffffffffu;
static unsigned int s_mdrProofKeys, s_mdrProofOverflow, s_mdrProofSurfaces, s_mdrProofVerts;
static unsigned int s_mdrProofRepeatedVerts, s_mdrProofIndexCycles, s_mdrProofPaletteCycles, s_mdrProofSkinCycles;
extern "C" volatile unsigned int g_SPXBMdrKernelSnapshot[11] = {0};

static void R_STEFX_RecordMdrPoseRequest(const md4Surface_t *surface) {
	unsigned int i;
	if (s_mdrProofSerial != g_SPXBPerfSampleSerial) {
		s_mdrProofSerial = g_SPXBPerfSampleSerial;
		s_mdrProofKeys = s_mdrProofOverflow = s_mdrProofSurfaces = s_mdrProofVerts = 0;
		s_mdrProofRepeatedVerts = s_mdrProofIndexCycles = s_mdrProofPaletteCycles = s_mdrProofSkinCycles = 0;
	}
	++s_mdrProofSurfaces;
	s_mdrProofVerts += surface->numVerts;
	for (i = 0; i < s_mdrProofKeys; ++i) {
		const stefxMdrPoseProof_t *key = &s_mdrPoseProof[i];
		if (key->surface == surface && key->frame == backEnd.currentEntity->e.frame &&
			key->oldFrame == backEnd.currentEntity->e.oldframe && key->backlerp == backEnd.currentEntity->e.backlerp) {
			s_mdrProofRepeatedVerts += surface->numVerts;
			return;
		}
	}
	if (s_mdrProofKeys == 256) { ++s_mdrProofOverflow; return; }
	stefxMdrPoseProof_t *key = &s_mdrPoseProof[s_mdrProofKeys++];
	key->surface = surface;
	key->frame = backEnd.currentEntity->e.frame;
	key->oldFrame = backEnd.currentEntity->e.oldframe;
	key->backlerp = backEnd.currentEntity->e.backlerp;
}

void R_STEFX_ReportMdrSkinCost(void) {
	if (!g_SPXBPerfSampleActive || s_mdrProofSerial != g_SPXBPerfSampleSerial || s_mdrProofReported == s_mdrProofSerial) return;
	s_mdrProofReported = s_mdrProofSerial;
	++g_SPXBMdrKernelSnapshot[0];
	g_SPXBMdrKernelSnapshot[1]=1;
	g_SPXBMdrKernelSnapshot[2]=s_mdrProofSerial;
	g_SPXBMdrKernelSnapshot[3]=s_mdrProofSurfaces;
	g_SPXBMdrKernelSnapshot[4]=s_mdrProofVerts;
	g_SPXBMdrKernelSnapshot[5]=s_mdrProofKeys;
	g_SPXBMdrKernelSnapshot[6]=s_mdrProofOverflow;
	g_SPXBMdrKernelSnapshot[7]=s_mdrProofRepeatedVerts;
	g_SPXBMdrKernelSnapshot[8]=s_mdrProofIndexCycles;
	g_SPXBMdrKernelSnapshot[9]=s_mdrProofPaletteCycles;
	g_SPXBMdrKernelSnapshot[10]=s_mdrProofSkinCycles;
	++g_SPXBMdrKernelSnapshot[0];
	XBLog_WriteProfile(va("STEFX_HW_MDR_SKIN: sample=%u surfaces=%u vertices=%u keys=%u overflow=%u repeatedVerts=%u indexCycles=%u paletteCycles=%u skinCycles=%u",
		s_mdrProofSerial, s_mdrProofSurfaces, s_mdrProofVerts, s_mdrProofKeys, s_mdrProofOverflow,
		s_mdrProofRepeatedVerts, s_mdrProofIndexCycles, s_mdrProofPaletteCycles, s_mdrProofSkinCycles));
}
#endif

// STEFX_MDR_SKIN_CACHE_BEGIN
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
// CPU results only: copying a hit into tess leaves submission, transforms,
// material evaluation and GPU resource lifetime on their existing paths.
extern "C" volatile unsigned int g_SPXBMainLoopCount;
extern "C" volatile unsigned int g_SPXBMdrSkinCache[8] = {0};
extern "C" volatile unsigned int g_SPXBMdrSkinMismatch[32] = {0};
struct stefxMdrSkinVertex_t { float xyz[3], normal[3], uv[2]; };
struct stefxMdrSkinKey_t {
	const md4Surface_t *surface;
	int frame, oldFrame;
	unsigned int lerpBits, first, count;
	unsigned int paletteHash;
	const void *palette;
	unsigned int paletteGeneration;
};
enum { STEFX_MDR_SKIN_VERTICES = 8192, STEFX_MDR_SKIN_KEYS = 256 };
static stefxMdrSkinVertex_t *s_mdrSkinVertices;
static stefxMdrSkinKey_t s_mdrSkinKeys[STEFX_MDR_SKIN_KEYS];
static unsigned int s_mdrSkinKeyCount, s_mdrSkinUsed;
static unsigned int s_mdrSkinLoop = ~0u;
static int s_mdrSkinRenderFrame = -1;
static bool s_mdrSkinTried;
static cvar_t *s_mdrSkinCvar, *s_mdrSkinVerify;

static stefxMdrSkinKey_t *R_STEFX_ClaimMdrSkin(md4Surface_t *surface,
	float backlerp, bool *hit, const void *palette, unsigned int paletteGeneration)
{
	*hit = false;
	if (!s_mdrSkinCvar) {
		s_mdrSkinCvar = Cvar_Get("r_efMdrSkinCache", "0", 0);
		s_mdrSkinVerify = Cvar_Get("r_efMdrSkinVerify", "0", 0);
	}
	g_SPXBMdrSkinCache[7] = s_mdrSkinCvar->integer ? 1u : 0u;
	if (!s_mdrSkinCvar->integer || !backEnd.viewParms.stefxSplitView ||
		surface->numVerts <= 0) return NULL;
	++g_SPXBMdrSkinCache[0];
	if (!s_mdrSkinTried) {
		s_mdrSkinTried = true;
		s_mdrSkinVertices = (stefxMdrSkinVertex_t *)HeapAlloc(GetProcessHeap(), 0,
			STEFX_MDR_SKIN_VERTICES * sizeof(stefxMdrSkinVertex_t));
		XBLog_WriteCriticalf("STEFX_MDR_SKIN_CACHE: allocated=%u bytes=%u verify=%d",
			s_mdrSkinVertices ? 1u : 0u,
			STEFX_MDR_SKIN_VERTICES * sizeof(stefxMdrSkinVertex_t), s_mdrSkinVerify->integer);
	}
	if (!s_mdrSkinVertices) { ++g_SPXBMdrSkinCache[4]; return NULL; }
	if (s_mdrSkinLoop != g_SPXBMainLoopCount || s_mdrSkinRenderFrame != tr.frameCount) {
		s_mdrSkinLoop = g_SPXBMainLoopCount;
		s_mdrSkinRenderFrame = tr.frameCount;
		s_mdrSkinKeyCount = s_mdrSkinUsed = 0;
	}
	unsigned int lerpBits;
	memcpy(&lerpBits, &backlerp, sizeof(lerpBits));
	for (unsigned int i = 0; i < s_mdrSkinKeyCount; ++i) {
		stefxMdrSkinKey_t *key = &s_mdrSkinKeys[i];
		if (key->surface == surface && key->palette == palette &&
			key->paletteGeneration == paletteGeneration && key->count == (unsigned int)surface->numVerts &&
			key->frame == backEnd.currentEntity->e.frame &&
			key->oldFrame == backEnd.currentEntity->e.oldframe && key->lerpBits == lerpBits) {
			*hit = true; ++g_SPXBMdrSkinCache[1];
			g_SPXBMdrSkinCache[2] += key->count;
			return key;
		}
	}
	if (s_mdrSkinKeyCount == STEFX_MDR_SKIN_KEYS ||
		(unsigned int)surface->numVerts > STEFX_MDR_SKIN_VERTICES - s_mdrSkinUsed) {
		++g_SPXBMdrSkinCache[4]; return NULL;
	}
	stefxMdrSkinKey_t *key = &s_mdrSkinKeys[s_mdrSkinKeyCount++];
	key->surface = surface; key->frame = backEnd.currentEntity->e.frame;
	key->oldFrame = backEnd.currentEntity->e.oldframe; key->lerpBits = lerpBits;
	key->paletteHash = 0;
	key->palette = palette; key->paletteGeneration = paletteGeneration;
	key->first = s_mdrSkinUsed; key->count = surface->numVerts;
	s_mdrSkinUsed += key->count; g_SPXBMdrSkinCache[3] += key->count;
	return key;
}

static void R_STEFX_CopyMdrSkin(const stefxMdrSkinKey_t *key, int baseVertex)
{
	for (unsigned int j = 0; j < key->count; ++j) {
		const stefxMdrSkinVertex_t *v = &s_mdrSkinVertices[key->first + j];
		memcpy(tess.xyz[baseVertex+j], v->xyz, sizeof(v->xyz));
		memcpy(tess.normal[baseVertex+j], v->normal, sizeof(v->normal));
		memcpy(tess.texCoords[baseVertex+j][0], v->uv, sizeof(v->uv));
	}
}

static void R_STEFX_StoreOrVerifyMdrSkin(const stefxMdrSkinKey_t *key,
	int baseVertex, bool hit, unsigned int paletteHash)
{
	for (unsigned int j = 0; j < key->count; ++j) {
		stefxMdrSkinVertex_t *v = &s_mdrSkinVertices[key->first + j];
		if (hit) {
			++g_SPXBMdrSkinCache[5];
			if (memcmp(tess.xyz[baseVertex+j], v->xyz, sizeof(v->xyz)) ||
				memcmp(tess.normal[baseVertex+j], v->normal, sizeof(v->normal)) ||
				memcmp(tess.texCoords[baseVertex+j][0], v->uv, sizeof(v->uv))) {
				++g_SPXBMdrSkinCache[6]; s_mdrSkinCvar->integer = 0;
				unsigned int original[8], cached[8];
				memcpy(original, tess.xyz[baseVertex+j], 12);
				memcpy(original+3, tess.normal[baseVertex+j], 12);
				memcpy(original+6, tess.texCoords[baseVertex+j][0], 8);
				memcpy(cached, v, sizeof(cached));
				g_SPXBMdrSkinMismatch[1] = (unsigned int)key->surface;
				g_SPXBMdrSkinMismatch[2] = key->frame;
				g_SPXBMdrSkinMismatch[3] = key->oldFrame;
				g_SPXBMdrSkinMismatch[4] = key->lerpBits;
				g_SPXBMdrSkinMismatch[5] = j;
				g_SPXBMdrSkinMismatch[6] = key->paletteHash;
				g_SPXBMdrSkinMismatch[7] = paletteHash;
				for (int component = 0; component < 8; ++component) {
					g_SPXBMdrSkinMismatch[8+component] = original[component];
					g_SPXBMdrSkinMismatch[16+component] = cached[component];
				}
				g_SPXBMdrSkinMismatch[24] = tr.frameCount;
				g_SPXBMdrSkinMismatch[25] = g_SPXBMainLoopCount;
				g_SPXBMdrSkinMismatch[26] = key->count;
				g_SPXBMdrSkinMismatch[0] = 1;
				XBLog_WriteCriticalf("STEFX_MDR_SKIN_CACHE: byte mismatch; disabled surface=%p frame=%d old=%d lerp=%08x vertex=%u palette=%08x/%08x original=%08x/%08x/%08x/%08x/%08x/%08x/%08x/%08x cached=%08x/%08x/%08x/%08x/%08x/%08x/%08x/%08x",
					key->surface, key->frame, key->oldFrame, key->lerpBits, j,
					key->paletteHash, paletteHash,
					original[0],original[1],original[2],original[3],original[4],original[5],original[6],original[7],
					cached[0],cached[1],cached[2],cached[3],cached[4],cached[5],cached[6],cached[7]);
				return;
			}
		} else {
			memcpy(v->xyz, tess.xyz[baseVertex+j], sizeof(v->xyz));
			memcpy(v->normal, tess.normal[baseVertex+j], sizeof(v->normal));
			memcpy(v->uv, tess.texCoords[baseVertex+j][0], sizeof(v->uv));
		}
	}
}
#endif
// STEFX_MDR_SKIN_CACHE_END

#include "stefx_coop_mdr_skin.h"

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
// Retain first mismatch even after the ordinary log mirror rotates.
extern "C" volatile unsigned int g_SPXBCoopMdrMismatch[48]={0};
struct coopSkinProof_t { unsigned int meshHash,cw,mxcsr,base,paletteHash; };
static coopSkinProof_t s_coopSkinProof[COOP_SKIN_KEYS];
// Diagnostic audit only: original output is always retained. Large errors,
// changed UVs or non-finite differences still invoke the disabling verifier.
extern "C" volatile unsigned int g_SPXBCoopMdrRoundoff[10]={0};
// mode, compared, exact, roundoff, excessive, maximum position/normal errors
// as float bits, maximum ULP distance, UV differences, non-finite differences.

// STEFX_COOP_SKIN_ERROR_BUDGET_BEGIN
static unsigned int R_STEFX_SkinErrorBudget(const unsigned int *original,
	const unsigned int *cached,float *positionMax,float *normalMax,unsigned int *ulpMax) {
	unsigned int flags=0;
	for (int c=0;c<8;++c) {
		if (original[c]==cached[c]) continue;
		if (c>=6) { flags|=4; continue; }
		if ((original[c]&0x7f800000u)==0x7f800000u ||
			(cached[c]&0x7f800000u)==0x7f800000u) { flags|=8; continue; }
		float a,b; memcpy(&a,&original[c],4); memcpy(&b,&cached[c],4);
		double error=(double)a-(double)b; if (error<0) error=-error;
		float *maximum=c<3 ? positionMax : normalMax;
		if (error>*maximum) *maximum=(float)error;
		if (error>(c<3 ? 0.0001 : 0.000001)) flags|=(c<3 ? 1u : 2u);
		unsigned int x=original[c]&0x80000000u ? ~original[c] : original[c]|0x80000000u;
		unsigned int y=cached[c]&0x80000000u ? ~cached[c] : cached[c]|0x80000000u;
		unsigned int distance=x>y ? x-y : y-x;
		if (distance>*ulpMax) *ulpMax=distance;
	}
	return flags;
}
// STEFX_COOP_SKIN_ERROR_BUDGET_END

static bool R_STEFX_AuditCoopSkin(const coopSkinKey_t *key,int baseVertex,bool hit) {
	static cvar_t *audit;
	if (!audit) audit=Cvar_Get("r_efCoopMdrRoundoffAudit","0",0);
	if (!audit->integer || s_coopSkinMode->integer!=2) return false;
	g_SPXBCoopMdrRoundoff[0]=1;
	if (!hit) return false; // Existing store, no result is reused in mode2.
	float maxPosition,maxNormal; unsigned int maxUlp=g_SPXBCoopMdrRoundoff[7];
	unsigned int bits=g_SPXBCoopMdrRoundoff[5]; memcpy(&maxPosition,&bits,4);
	bits=g_SPXBCoopMdrRoundoff[6]; memcpy(&maxNormal,&bits,4);
	for (unsigned int j=0;j<key->count;++j) {
		unsigned int original[8];
		memcpy(original,tess.xyz[baseVertex+j],12);
		memcpy(original+3,tess.normal[baseVertex+j],12);
		memcpy(original+6,tess.texCoords[baseVertex+j][0],8);
		const unsigned int *cached=(const unsigned int*)&s_coopSkin->vertices[key->first+j];
		++g_SPXBCoopMdrRoundoff[1];
		if (!memcmp(original,cached,32)) ++g_SPXBCoopMdrRoundoff[2];
		else {
			const unsigned int flags=R_STEFX_SkinErrorBudget(original,cached,&maxPosition,&maxNormal,&maxUlp);
			memcpy(&bits,&maxPosition,4); g_SPXBCoopMdrRoundoff[5]=bits;
			memcpy(&bits,&maxNormal,4); g_SPXBCoopMdrRoundoff[6]=bits;
			g_SPXBCoopMdrRoundoff[7]=maxUlp;
			if (flags) {
				++g_SPXBCoopMdrRoundoff[4];
				if (flags&4) ++g_SPXBCoopMdrRoundoff[8];
				if (flags&8) ++g_SPXBCoopMdrRoundoff[9];
				return false; // Existing exact verifier disables the cache.
			}
			++g_SPXBCoopMdrRoundoff[3];
		}
		++g_SPXBCoopMdrSkin[7];
	}
	return true;
}
static unsigned int R_STEFX_SkinProofHash(const void *data,unsigned int bytes) {
	const unsigned char *p=(const unsigned char*)data;
	unsigned int hash=2166136261u;
	while (bytes--) hash=(hash ^ *p++)*16777619u;
	return hash;
}
static void R_STEFX_ProveCoopSkin(const coopSkinKey_t *key,int baseVertex,bool hit,
	md4Surface_t *surface,const md4Bone_t *palette,const md4Vertex_t *end) {
	unsigned short cw; unsigned int mxcsr;
	__asm { fnstcw cw }
	__asm { stmxcsr mxcsr }
	const coopSkinPose_t *pose=&s_coopSkin->poses[key->pose];
	const void *start=(byte*)surface+surface->ofsVerts;
	coopSkinProof_t current;
	current.meshHash=R_STEFX_SkinProofHash(start,(const byte*)end-(const byte*)start);
	current.paletteHash=R_STEFX_SkinProofHash(palette,pose->count*sizeof(md4Bone_t));
	current.cw=cw; current.mxcsr=mxcsr; current.base=baseVertex;
	coopSkinProof_t *saved=&s_coopSkinProof[key-s_coopSkin->keys];
	if (!hit) { *saved=current; return; }
	if (g_SPXBCoopMdrMismatch[0]) return;
	for (unsigned int j=0;j<key->count;++j) {
		const coopSkinVertex_t *cached=&s_coopSkin->vertices[key->first+j];
		unsigned int original[8];
		memcpy(original,tess.xyz[baseVertex+j],12);
		memcpy(original+3,tess.normal[baseVertex+j],12);
		memcpy(original+6,tess.texCoords[baseVertex+j][0],8);
		if (!memcmp(original,cached,32)) continue;
		g_SPXBCoopMdrMismatch[1]=(unsigned int)surface;
		g_SPXBCoopMdrMismatch[2]=j;
		g_SPXBCoopMdrMismatch[3]=key->count;
		g_SPXBCoopMdrMismatch[4]=saved->meshHash;
		g_SPXBCoopMdrMismatch[5]=current.meshHash;
		g_SPXBCoopMdrMismatch[6]=saved->cw;
		g_SPXBCoopMdrMismatch[7]=current.cw;
		g_SPXBCoopMdrMismatch[8]=saved->mxcsr;
		g_SPXBCoopMdrMismatch[9]=current.mxcsr;
		g_SPXBCoopMdrMismatch[10]=saved->base;
		g_SPXBCoopMdrMismatch[11]=current.base;
		g_SPXBCoopMdrMismatch[12]=saved->paletteHash;
		g_SPXBCoopMdrMismatch[13]=current.paletteHash;
		g_SPXBCoopMdrMismatch[14]=pose->frame;
		g_SPXBCoopMdrMismatch[15]=pose->oldFrame;
		g_SPXBCoopMdrMismatch[16]=pose->lerpBits;
		g_SPXBCoopMdrMismatch[17]=g_SPXBMainLoopCount;
		g_SPXBCoopMdrMismatch[18]=tr.frameCount;
		g_SPXBCoopMdrMismatch[19]=pose->count;
		g_SPXBCoopMdrMismatch[20]=memcmp(&s_coopSkin->bones[pose->first],palette,pose->count*sizeof(md4Bone_t));
		const unsigned int *cachedBits=(const unsigned int*)cached;
		for(int c=0;c<8;++c) { g_SPXBCoopMdrMismatch[21+c]=original[c]; g_SPXBCoopMdrMismatch[29+c]=cachedBits[c]; }
		g_SPXBCoopMdrMismatch[0]=1;
		return;
	}
}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
// Independent, default-off arithmetic path. No persistent geometry or GPU storage.
// mode, surfaces, fast vertices, compared, exact, roundoff, excessive,
// maximum position/normal errors (float bits), maximum ULP, UV/nonfinite
// differences, failure latch, first failing surface/vertex, reserved.
extern "C" volatile unsigned int g_SPXBCoopMdrSimd[16]={0};
static bool s_coopMdrSimdFailed;
static int R_STEFX_CoopMdrSimdMode(void) {
	static cvar_t *mode,*split,*players,*game;
	if (!mode) {
		mode=Cvar_Get("r_efCoopMdrSimd","0",0);
		split=Cvar_Get("stefx_splitScreen","0",0);
		players=Cvar_Get("stefx_splitScreenPlayers","1",0);
		game=Cvar_Get("stefx_splitScreenMode","coop",0);
	}
	int result=0;
	if (!s_coopMdrSimdFailed && split->integer && players->integer>=2 &&
		!Q_stricmp(game->string,"coop") && backEnd.viewParms.stefxSplitView) {
		if (mode->integer==1) result=1;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (mode->integer==2) result=2; // Recompute and retain the original output.
#endif
	}
	if (g_SPXBCoopMdrSimd[0]!=(unsigned int)result)
		XBLog_WriteCriticalf("STEFX_COOP_MDR_SIMD: mode=%d failed=%u",result,s_coopMdrSimdFailed?1u:0u);
	g_SPXBCoopMdrSimd[0]=result;
	return result;
}

// STEFX_COOP_SKIN_SIMD_KERNEL_BEGIN
static void R_STEFX_StoreSkinThree(float *dest,__m128 value) {
	_mm_storel_pi((__m64*)dest,value);
	_mm_store_ss(dest+2,_mm_shuffle_ps(value,value,_MM_SHUFFLE(2,2,2,2)));
}
static void R_STEFX_SkinVertexSimd(const md4Vertex_t *v,const md4Bone_t *palette,
	float *position,float *normal) {
	__m128 pSum=_mm_setzero_ps(),nSum=_mm_setzero_ps();
	const __m128 nx=_mm_set1_ps(v->normal[0]),ny=_mm_set1_ps(v->normal[1]),nz=_mm_set1_ps(v->normal[2]);
	for (int k=0;k<v->numWeights;++k) {
		const md4Weight_t *w=&v->weights[k];
		const md4Bone_t *bone=&palette[w->boneIndex];
		__m128 x=_mm_loadu_ps(bone->matrix[0]),y=_mm_loadu_ps(bone->matrix[1]),
			z=_mm_loadu_ps(bone->matrix[2]),t=_mm_setzero_ps();
		_MM_TRANSPOSE4_PS(x,y,z,t);
		__m128 p=_mm_mul_ps(x,_mm_set1_ps(w->offset[0]));
		p=_mm_add_ps(p,_mm_mul_ps(y,_mm_set1_ps(w->offset[1])));
		p=_mm_add_ps(p,_mm_mul_ps(z,_mm_set1_ps(w->offset[2])));
		p=_mm_add_ps(p,t);
		__m128 n=_mm_mul_ps(x,nx);
		n=_mm_add_ps(n,_mm_mul_ps(y,ny)); n=_mm_add_ps(n,_mm_mul_ps(z,nz));
		const __m128 weight=_mm_set1_ps(w->boneWeight);
		pSum=_mm_add_ps(pSum,_mm_mul_ps(p,weight));
		nSum=_mm_add_ps(nSum,_mm_mul_ps(n,weight));
	}
	R_STEFX_StoreSkinThree(position,pSum);
	R_STEFX_StoreSkinThree(normal,nSum);
}
// STEFX_COOP_SKIN_SIMD_KERNEL_END

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
static void R_STEFX_VerifySkinSimd(const md4Vertex_t *v,const md4Bone_t *palette,
	const md4Surface_t *surface,int vertex,int output) {
	if (s_coopMdrSimdFailed) return;
	float candidate[8]; unsigned int original[8];
	R_STEFX_SkinVertexSimd(v,palette,candidate,candidate+3);
	memcpy(candidate+6,v->texCoords,8);
	memcpy(original,tess.xyz[output],12); memcpy(original+3,tess.normal[output],12);
	memcpy(original+6,tess.texCoords[output][0],8);
	++g_SPXBCoopMdrSimd[3];
	if (!memcmp(original,candidate,32)) { ++g_SPXBCoopMdrSimd[4]; return; }
	float maxPosition,maxNormal; unsigned int bits=g_SPXBCoopMdrSimd[7];
	memcpy(&maxPosition,&bits,4); bits=g_SPXBCoopMdrSimd[8]; memcpy(&maxNormal,&bits,4);
	unsigned int maxUlp=g_SPXBCoopMdrSimd[9];
	const unsigned int flags=R_STEFX_SkinErrorBudget(original,(const unsigned int*)candidate,
		&maxPosition,&maxNormal,&maxUlp);
	memcpy(&bits,&maxPosition,4); g_SPXBCoopMdrSimd[7]=bits;
	memcpy(&bits,&maxNormal,4); g_SPXBCoopMdrSimd[8]=bits; g_SPXBCoopMdrSimd[9]=maxUlp;
	if (!flags) { ++g_SPXBCoopMdrSimd[5]; return; }
	++g_SPXBCoopMdrSimd[6];
	if (flags&4) ++g_SPXBCoopMdrSimd[10];
	if (flags&8) ++g_SPXBCoopMdrSimd[11];
	s_coopMdrSimdFailed=true; g_SPXBCoopMdrSimd[12]=1;
	g_SPXBCoopMdrSimd[13]=(unsigned int)surface; g_SPXBCoopMdrSimd[14]=vertex;
	XBLog_WriteCriticalf("STEFX_COOP_MDR_SIMD: disabled surface=%p vertex=%d flags=%u",surface,vertex,flags);
}
#endif
#endif

void RB_SurfaceAnim( md4Surface_t *surface ) {
	int				j, k;
	float			frontlerp, backlerp;
	int				*triangles;
	int				indexes;
	int				baseIndex, baseVertex;
	int				numVerts;
	md4Vertex_t		*v;
	md4Bone_t		*bonePtr, *bone;
	md4Header_t		*header;
	md4Frame_t		*frame=0;
	md4Frame_t		*oldFrame=0;
	md4CompFrame_t	*cframe=0;
	md4CompFrame_t	*coldFrame=0;
	int				frameSize;
	qboolean		compressed;


	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
		frontlerp = 1;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
		frontlerp = 1.0 - backlerp;
	}
	header = (md4Header_t *)((byte *)surface + surface->ofsHeader);

	if (header->ofsFrames<0) // Compressed
	{
		compressed = qtrue;
		frameSize = (int)( &((md4CompFrame_t *)0)->bones[ header->numBones ] );		
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		cframe = (md4CompFrame_t *)R_STEFX_GetMDRFrame(header, backEnd.currentEntity->e.frame);
		coldFrame = (md4CompFrame_t *)R_STEFX_GetMDRFrame(header, backEnd.currentEntity->e.oldframe);
#else
		cframe = (md4CompFrame_t *)((byte *)header - header->ofsFrames + backEnd.currentEntity->e.frame * frameSize );
		coldFrame = (md4CompFrame_t *)((byte *)header - header->ofsFrames + backEnd.currentEntity->e.oldframe * frameSize );
#endif
	}
	else
	{
		compressed = qfalse;
		frameSize = (int)( &((md4Frame_t *)0)->bones[ header->numBones ] );
		frame = (md4Frame_t *)((byte *)header + header->ofsFrames + 
			backEnd.currentEntity->e.frame * frameSize );
		oldFrame = (md4Frame_t *)((byte *)header + header->ofsFrames + 
			backEnd.currentEntity->e.oldframe * frameSize );
	}



	// Each triangle appends three indexes to tess below.
	RB_CheckOverflow( surface->numVerts, surface->numTriangles * 3 );

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	const qboolean mdrSample = g_SPXBPerfSampleActive ? qtrue : qfalse;
	unsigned __int64 mdrPhaseStart = 0;
	if (mdrSample) { R_STEFX_RecordMdrPoseRequest(surface); mdrPhaseStart = STEFX_XboxReadTsc(); }
#endif
	triangles = (int *) ((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	baseIndex = tess.numIndexes;
	baseVertex = tess.numVertexes;
	for (j = 0 ; j < indexes ; j++) {
		tess.indexes[baseIndex + j] = baseVertex + triangles[j];
	}
	tess.numIndexes += indexes;

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if (mdrSample) { s_mdrProofIndexCycles += STEFX_XboxElapsedCycles(mdrPhaseStart); mdrPhaseStart = STEFX_XboxReadTsc(); }
#endif

	bonePtr = RB_GetAnimBonePalette( header, frontlerp, backlerp, frame, oldFrame,
		cframe, coldFrame, compressed );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if (mdrSample) { s_mdrProofPaletteCycles += STEFX_XboxElapsedCycles(mdrPhaseStart); mdrPhaseStart = STEFX_XboxReadTsc(); }
#endif
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	bool skinHit;
	stefxMdrSkinKey_t *skinKey = R_STEFX_ClaimMdrSkin(surface, backlerp, &skinHit,
		bonePtr, s_stefxMdrPaletteCurrentGeneration);
	if (skinKey && skinHit && !s_mdrSkinVerify->integer) {
		R_STEFX_CopyMdrSkin(skinKey, baseVertex);
		tess.numVertexes += surface->numVerts;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
		if (mdrSample) s_mdrProofSkinCycles += STEFX_XboxElapsedCycles(mdrPhaseStart);
#endif
		return;
	}
	unsigned int skinPaletteHash = 0;
	if (skinKey && s_mdrSkinVerify->integer) {
		const unsigned char *bytes = (const unsigned char *)bonePtr;
		skinPaletteHash = 2166136261u;
		for (unsigned int byteIndex = 0; byteIndex < header->numBones * sizeof(md4Bone_t); ++byteIndex)
			skinPaletteHash = (skinPaletteHash ^ bytes[byteIndex]) * 16777619u;
		if (!skinHit) skinKey->paletteHash = skinPaletteHash;
	}
#endif
	//
	// deform the vertexes by the lerped bones
	//
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	bool coopSkinHit;
	coopSkinKey_t *coopSkinKey=R_STEFX_ClaimCoopSkin(surface,header,backlerp,bonePtr,&coopSkinHit);
	if (coopSkinKey && coopSkinHit && s_coopSkinMode->integer==1) {
		R_STEFX_CopyCoopSkin(coopSkinKey,baseVertex);
		tess.numVertexes+=surface->numVerts;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (mdrSample) s_mdrProofSkinCycles+=STEFX_XboxElapsedCycles(mdrPhaseStart);
#endif
		return;
	}
#endif
	numVerts = surface->numVerts;
	v = (md4Vertex_t *) ((byte *)surface + surface->ofsVerts);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	const int coopSimdMode=R_STEFX_CoopMdrSimdMode();
	if (coopSimdMode) ++g_SPXBCoopMdrSimd[1];
	if (coopSimdMode==1) {
		for (j=0;j<numVerts;++j) {
			R_STEFX_SkinVertexSimd(v,bonePtr,tess.xyz[baseVertex+j],tess.normal[baseVertex+j]);
			memcpy(tess.texCoords[baseVertex+j][0],v->texCoords,8);
			v=(md4Vertex_t*)&v->weights[v->numWeights];
		}
		g_SPXBCoopMdrSimd[2]+=numVerts;
	} else
#endif
	{
	for ( j = 0; j < numVerts; j++ ) {
		vec3_t	tempVert, tempNormal;
		md4Weight_t	*w;

		VectorClear( tempVert );
		VectorClear( tempNormal );
		w = v->weights;
		for ( k = 0 ; k < v->numWeights ; k++, w++ ) {
			bone = bonePtr + w->boneIndex;

			tempVert[0] += w->boneWeight * ( DotProduct( bone->matrix[0], w->offset ) + bone->matrix[0][3] );
			tempVert[1] += w->boneWeight * ( DotProduct( bone->matrix[1], w->offset ) + bone->matrix[1][3] );
			tempVert[2] += w->boneWeight * ( DotProduct( bone->matrix[2], w->offset ) + bone->matrix[2][3] );

			tempNormal[0] += w->boneWeight * DotProduct( bone->matrix[0], v->normal );
			tempNormal[1] += w->boneWeight * DotProduct( bone->matrix[1], v->normal );
			tempNormal[2] += w->boneWeight * DotProduct( bone->matrix[2], v->normal );
		}

		tess.xyz[baseVertex + j][0] = tempVert[0];
		tess.xyz[baseVertex + j][1] = tempVert[1];
		tess.xyz[baseVertex + j][2] = tempVert[2];

		tess.normal[baseVertex + j][0] = tempNormal[0];
		tess.normal[baseVertex + j][1] = tempNormal[1];
		tess.normal[baseVertex + j][2] = tempNormal[2];

		tess.texCoords[baseVertex + j][0][0] = v->texCoords[0];
		tess.texCoords[baseVertex + j][0][1] = v->texCoords[1];

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (coopSimdMode==2) R_STEFX_VerifySkinSimd(v,bonePtr,surface,j,baseVertex+j);
#endif
		v = (md4Vertex_t *)&v->weights[v->numWeights];
	}
	}
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	if (skinKey) R_STEFX_StoreOrVerifyMdrSkin(skinKey, baseVertex, skinHit, skinPaletteHash);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	if (coopSkinKey) {
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (s_coopSkinMode->integer==2) R_STEFX_ProveCoopSkin(coopSkinKey,baseVertex,coopSkinHit,surface,bonePtr,v);
		if (!R_STEFX_AuditCoopSkin(coopSkinKey,baseVertex,coopSkinHit))
#endif
		R_STEFX_StoreOrVerifyCoopSkin(coopSkinKey,baseVertex,coopSkinHit);
	}
#endif
	tess.numVertexes += surface->numVerts;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if (mdrSample) s_mdrProofSkinCycles += STEFX_XboxElapsedCycles(mdrPhaseStart);
#endif
}
