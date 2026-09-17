//Anything above this #include will be ignored by the compiler
#include "../../server/exe_headers.h"

// tr_shade.c

#include "../tr_local.h"
#include "retail_renderer_contract.h"

#ifdef VV_LIGHTING
#include "../../win32/glw_win_dx8.h"
#include "../../win32/win_lighteffects.h"
#endif

#include "../tr_QuickSprite.h"

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#include "../../win32/xb_log.h"
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
#include "../../win32/xb_perf.h"
#endif

STEFX_RETAIL_NAMESPACE_BEGIN

bool styleUpdated[MAX_LIGHT_STYLES];

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

shaderCommands_t	tess;
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
extern bool g_stefxWorldBasePass;
#endif
static qboolean	setArraysOnce;

color4ub_t	styleColors[MAX_LIGHT_STYLES];

extern bool g_bRenderGlowingObjects;

int R_STEFX_FogModeForView( void )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( backEnd.viewParms.stefxSplitThreePlusEconomy )
	{
		return 0;
	}
#endif
	return r_drawfog->integer;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
static int s_stefxShaderTraceDumpedCount;
static qboolean s_stefxShaderTraceStarted;
static cvar_t *s_stefxShaderTrace;

static void R_STEFX_ShaderTraceDumpImage( const shader_t *shader, int pass, int bundleIndex,
	int animationIndex, const image_t *image )
{
	if ( !image )
	{
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_ERROR: null image shader=%d name='%s' pass=%d bundle=%d anim=%d",
			shader ? shader->index : -1, shader ? shader->name : "<null>", pass, bundleIndex,
			animationIndex );
		return;
	}

#ifndef FINAL_BUILD
	XBLog_WriteCriticalf(
		"STEFX_HM_SHADER_IMAGE: shader=%d pass=%d bundle=%d anim=%d ptr=0x%08x tex=%u name='%s' wh=%ux%u format=%d lightmap=%d system=%d mips=%d",
		shader->index, pass, bundleIndex, animationIndex, (unsigned int)image,
		(unsigned int)image->texnum, image->imgName, (unsigned int)image->width,
		(unsigned int)image->height, image->internalFormat, image->isLightmap ? 1 : 0,
		image->isSystem ? 1 : 0, (int)image->mipcount );
#else
	XBLog_WriteCriticalf(
		"STEFX_HM_SHADER_IMAGE: shader=%d pass=%d bundle=%d anim=%d ptr=0x%08x tex=%u wh=%ux%u format=%d lightmap=%d system=%d mips=%d",
		shader->index, pass, bundleIndex, animationIndex, (unsigned int)image,
		(unsigned int)image->texnum, (unsigned int)image->width, (unsigned int)image->height,
		image->internalFormat, image->isLightmap ? 1 : 0, image->isSystem ? 1 : 0,
		(int)image->mipcount );
#endif
}

static void R_STEFX_ShaderTraceDumpBundle( const shader_t *shader, int pass,
	int bundleIndex, const textureBundle_t *bundle )
{
	int animationCount;
	int animationIndex;

	XBLog_WriteCriticalf(
		"STEFX_HM_SHADER_BUNDLE: shader=%d pass=%d bundle=%d image=0x%08x tcgen=%d lightmap=%d vertexLightmap=%d oneShot=%d anims=%d speed=%g texMods=%d vectors=0x%08x",
		shader->index, pass, bundleIndex, (unsigned int)bundle->image, (int)bundle->tcGen,
		(int)bundle->isLightmap, (int)bundle->vertexLightmap, (int)bundle->oneShotAnimMap,
		(int)bundle->numImageAnimations, bundle->imageAnimationSpeed,
		(int)bundle->numTexMods, (unsigned int)bundle->tcGenVectors );

	if ( !bundle->image )
	{
		return;
	}

	animationCount = bundle->numImageAnimations;
	if ( animationCount <= 1 )
	{
		R_STEFX_ShaderTraceDumpImage( shader, pass, bundleIndex, 0, bundle->image );
		return;
	}
	if ( animationCount > 64 )
	{
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_ERROR: invalid animation count shader=%d name='%s' pass=%d bundle=%d anims=%d",
			shader->index, shader->name, pass, bundleIndex, animationCount );
		return;
	}

	for ( animationIndex = 0; animationIndex < animationCount; ++animationIndex )
	{
		R_STEFX_ShaderTraceDumpImage( shader, pass, bundleIndex, animationIndex,
			*((image_t **)bundle->image + animationIndex) );
	}
}

static void R_STEFX_ShaderTraceDumpRegistered( void )
{
	int shaderIndex;

	// Surface batches are a hot path. Register once, but read the live value
	// so console toggles still take effect immediately. Cvar_Get also promotes
	// a user-created value to a registered cvar that survives cvar_restart.
	if ( !s_stefxShaderTrace )
	{
		s_stefxShaderTrace = Cvar_Get( "stefx_hm_shader_trace", "0", 0 );
		XBLog_WriteCriticalf( "STEFX_HM_SHADER_TRACE_GATE: cached live cvar enabled=%d",
			s_stefxShaderTrace->integer );
	}
	if ( !s_stefxShaderTrace->integer )
	{
		return;
	}

	if ( !s_stefxShaderTraceStarted )
	{
		s_stefxShaderTraceStarted = qtrue;
		XBLog_WriteCriticalf( "STEFX_HM_SHADER_TRACE_BEGIN: path='%s' registered=%d max=%d",
			XBLog_GetPath() ? XBLog_GetPath() : "<none>", tr.numShaders, MAX_SHADERS );
	}

	if ( tr.numShaders < s_stefxShaderTraceDumpedCount )
	{
		s_stefxShaderTraceDumpedCount = 0;
		XBLog_WriteCritical( "STEFX_HM_SHADER_TRACE: renderer registration restarted; shader dump reset" );
	}
	if ( tr.numShaders == s_stefxShaderTraceDumpedCount )
	{
		return;
	}

	for ( shaderIndex = s_stefxShaderTraceDumpedCount; shaderIndex < tr.numShaders; ++shaderIndex )
	{
		shader_t *registeredShader = tr.shaders[shaderIndex];
		int pass;

		if ( !registeredShader )
		{
			XBLog_WriteCriticalf( "STEFX_HM_SHADER_TRACE_ERROR: null registered shader index=%d", shaderIndex );
			continue;
		}

		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_DEF: index=%d sorted=%d name='%s' passes=%d sort=%g lm=%d,%d,%d,%d styles=%u,%u,%u,%u default=%d explicit=%d merge=%d bump=%d multiEnv=%d cull=%d fog=%d surface=0x%08x content=0x%08x sky=0x%08x deform=%d remap=0x%08x",
			registeredShader->index, registeredShader->sortedIndex, registeredShader->name,
			(int)registeredShader->numUnfoggedPasses, registeredShader->sort,
			(int)registeredShader->lightmapIndex[0], (int)registeredShader->lightmapIndex[1],
			(int)registeredShader->lightmapIndex[2], (int)registeredShader->lightmapIndex[3],
			(unsigned int)registeredShader->styles[0], (unsigned int)registeredShader->styles[1],
			(unsigned int)registeredShader->styles[2], (unsigned int)registeredShader->styles[3],
			(int)registeredShader->defaultShader, (int)registeredShader->explicitlyDefined,
			(int)registeredShader->entityMergable, (int)registeredShader->isBumpMap,
			registeredShader->multitextureEnv, (int)registeredShader->cullType,
			(int)registeredShader->fogPass, (unsigned int)registeredShader->surfaceFlags,
			(unsigned int)registeredShader->contentFlags, (unsigned int)registeredShader->sky,
			(int)registeredShader->numDeforms, (unsigned int)registeredShader->remappedShader );

		if ( registeredShader->numUnfoggedPasses < 0 ||
			registeredShader->numUnfoggedPasses > MAX_SHADER_STAGES )
		{
			XBLog_WriteCriticalf(
				"STEFX_HM_SHADER_TRACE_ERROR: invalid pass count shader=%d name='%s' passes=%d",
				registeredShader->index, registeredShader->name,
				(int)registeredShader->numUnfoggedPasses );
			continue;
		}

		for ( pass = 0; pass < registeredShader->numUnfoggedPasses; ++pass )
		{
			shaderStage_t *stage = &registeredShader->stages[pass];
			int bundleIndex;
			XBLog_WriteCriticalf(
				"STEFX_HM_SHADER_STAGE: shader=%d pass=%d active=%d index=%d detail=%d env=%d bump=%d rgb=%d alpha=%d fogAdjust=%d fogOverride=%d state=0x%08x color=%u,%u,%u,%u sprite=0x%08x",
				registeredShader->index, pass, (int)stage->active, (int)stage->index,
				(int)stage->isDetail, (int)stage->isEnvironment, (int)stage->isBumpMap,
				(int)stage->rgbGen, (int)stage->alphaGen, (int)stage->adjustColorsForFog,
				(int)stage->mGLFogColorOverride, stage->stateBits,
				(unsigned int)stage->constantColor[0], (unsigned int)stage->constantColor[1],
				(unsigned int)stage->constantColor[2], (unsigned int)stage->constantColor[3],
				(unsigned int)stage->ss );
			if ( !stage->active )
			{
				XBLog_WriteCriticalf(
					"STEFX_HM_SHADER_TRACE_ERROR: inactive permanent stage shader=%d name='%s' pass=%d",
					registeredShader->index, registeredShader->name, pass );
			}
			for ( bundleIndex = 0; bundleIndex < NUM_TEXTURE_BUNDLES; ++bundleIndex )
			{
				R_STEFX_ShaderTraceDumpBundle( registeredShader, pass, bundleIndex,
					&stage->bundle[bundleIndex] );
			}
		}
	}

	XBLog_WriteCriticalf( "STEFX_HM_SHADER_TRACE_DUMP_COMPLETE: first=%d total=%d",
		s_stefxShaderTraceDumpedCount, tr.numShaders );
	s_stefxShaderTraceDumpedCount = tr.numShaders;
}
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
static qboolean R_STEFX_IsSplitEconomyStageSkipped( int stage,
	const shaderStage_t *shaderStage );
static int R_STEFX_SplitEconomyEffectivePassCount( const shaderCommands_t *input );

typedef struct stefxShaderCost_s
{
	unsigned int cycles;
	unsigned int batches;
	unsigned int passes;
	unsigned int indexes;
	unsigned int blendPasses;
	unsigned int alphaTestPasses;
	unsigned int twoSidedPasses;
	unsigned int worldBatches;
	unsigned int entityBatches;
} stefxShaderCost_t;

static stefxShaderCost_t s_stefxShaderCosts[MAX_SHADERS];
static unsigned int s_stefxShaderCostSerial = 0xffffffffu;
static unsigned int s_stefxShaderCostReportedSerial = 0xffffffffu;
static qboolean s_stefxShaderCostActive;

static qboolean R_STEFX_BeginShaderCostSample( void )
{
	const unsigned int serial = (unsigned int)g_SPXBPerfSampleSerial;

	if ( !g_SPXBPerfSampleActive )
	{
		return qfalse;
	}
	if ( serial != s_stefxShaderCostSerial )
	{
		memset( s_stefxShaderCosts, 0, sizeof(s_stefxShaderCosts) );
		s_stefxShaderCostSerial = serial;
		s_stefxShaderCostActive =
			Cvar_VariableIntegerValue( "stefx_splitScreen" ) != 0 &&
			Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) >= 2;
	}

	return s_stefxShaderCostActive;
}

static void R_STEFX_RecordShaderCost( const shaderCommands_t *input, unsigned int cycles )
{
	stefxShaderCost_t *cost;
	unsigned int pass;
	unsigned int fogPasses;
	unsigned int shaderPasses;
	unsigned int shaderIndex;

	if ( !input || !input->shader )
	{
		return;
	}
	shaderIndex = (unsigned int)input->shader->index;
	if ( shaderIndex >= MAX_SHADERS )
	{
		return;
	}

	cost = &s_stefxShaderCosts[shaderIndex];
	shaderPasses = (unsigned int)R_STEFX_SplitEconomyEffectivePassCount( input );
	fogPasses = (input->fogNum && input->shader->fogPass && R_STEFX_FogModeForView() == 1) ? 1u : 0u;
	cost->cycles += cycles;
	++cost->batches;
	cost->passes += shaderPasses + fogPasses;
	cost->indexes += (unsigned int)input->numIndexes * (shaderPasses + fogPasses);
	if ( backEnd.currentEntity == &tr.worldEntity )
	{
		++cost->worldBatches;
	}
	else
	{
		++cost->entityBatches;
	}

	for ( pass = 0; pass < (unsigned int)input->shader->numUnfoggedPasses; ++pass )
	{
		const shaderStage_t *shaderStage = &input->xstages[pass];
		const unsigned int stateBits = shaderStage->stateBits;

		if ( R_STEFX_IsSplitEconomyStageSkipped( (int)pass, shaderStage ) )
		{
			continue;
		}
		if ( !shaderStage->active )
		{
			break;
		}
		if ( pass && r_lightmap->integer &&
			!( shaderStage->bundle[0].isLightmap ||
				shaderStage->bundle[1].isLightmap ||
				shaderStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
		if ( shaderStage->ss && shaderStage->ss->surfaceSpriteType )
		{
			continue;
		}
		if ( stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS) )
		{
			++cost->blendPasses;
		}
		if ( stateBits & GLS_ATEST_BITS )
		{
			++cost->alphaTestPasses;
		}
	}
	if ( input->shader->cullType == CT_TWO_SIDED )
	{
		cost->twoSidedPasses += shaderPasses + fogPasses;
	}
}

void R_STEFX_ReportShaderCosts( void )
{
	const int maxReportedShaders = 5;
	int rank;
	int topShaders[5];
	int topPressureShaders[5];
	unsigned int totalCycles = 0;
	unsigned int totalBatches = 0;
	unsigned int totalPasses = 0;
	unsigned int totalIndexes = 0;
	unsigned int totalBlendPasses = 0;
	unsigned int totalAlphaTestPasses = 0;
	unsigned int totalTwoSidedPasses = 0;
	unsigned int totalWorldBatches = 0;
	unsigned int totalEntityBatches = 0;
	int shaderIndex;

	if ( !g_SPXBPerfSampleActive || !s_stefxShaderCostActive ||
		s_stefxShaderCostSerial == s_stefxShaderCostReportedSerial )
	{
		return;
	}

	for ( rank = 0; rank < maxReportedShaders; ++rank )
	{
		topShaders[rank] = -1;
		topPressureShaders[rank] = -1;
	}
	for ( shaderIndex = 0; shaderIndex < tr.numShaders && shaderIndex < MAX_SHADERS; ++shaderIndex )
	{
		stefxShaderCost_t *cost = &s_stefxShaderCosts[shaderIndex];
		totalCycles += cost->cycles;
		totalBatches += cost->batches;
		totalPasses += cost->passes;
		totalIndexes += cost->indexes;
		totalBlendPasses += cost->blendPasses;
		totalAlphaTestPasses += cost->alphaTestPasses;
		totalTwoSidedPasses += cost->twoSidedPasses;
		totalWorldBatches += cost->worldBatches;
		totalEntityBatches += cost->entityBatches;
	}

	for ( rank = 0; rank < maxReportedShaders; ++rank )
	{
		int candidate = -1;
		unsigned int candidateCycles = 0;
		for ( shaderIndex = 0; shaderIndex < tr.numShaders && shaderIndex < MAX_SHADERS; ++shaderIndex )
		{
			int priorRank;
			qboolean alreadySelected = qfalse;
			for ( priorRank = 0; priorRank < rank; ++priorRank )
			{
				if ( topShaders[priorRank] == shaderIndex )
				{
					alreadySelected = qtrue;
					break;
				}
			}
			if ( !alreadySelected && s_stefxShaderCosts[shaderIndex].cycles > candidateCycles )
			{
				candidate = shaderIndex;
				candidateCycles = s_stefxShaderCosts[shaderIndex].cycles;
			}
		}
		if ( candidate < 0 )
		{
			break;
		}
		topShaders[rank] = candidate;
		{
			const stefxShaderCost_t *cost = &s_stefxShaderCosts[candidate];
			const shader_t *shader = tr.shaders[candidate];
			XBLog_WriteProfile( va(
				"STEFX_HW_SHADER_COST: sample=%u rank=%d shader=%d name='%s' cycles=%u batches=%u passes=%u indexes=%u blend=%u alphaTest=%u twoSided=%u world=%u entity=%u",
				s_stefxShaderCostSerial, rank + 1, candidate,
				shader ? shader->name : "<missing>", cost->cycles, cost->batches,
				cost->passes, cost->indexes, cost->blendPasses, cost->alphaTestPasses,
				cost->twoSidedPasses, cost->worldBatches, cost->entityBatches ) );
		}
	}

	/*
	** CPU time alone can hide the shaders that create the most D3D8 push-buffer
	** pressure.  Rank a second diagnostic list by actual draw passes so a future
	** split-screen quality change can target measured repeated work.  This is
	** frame-diagnostic-only and does not alter the production render path.
	*/
	for ( rank = 0; rank < maxReportedShaders; ++rank )
	{
		int candidate = -1;
		unsigned int candidatePasses = 0;
		unsigned int candidateIndexes = 0;
		for ( shaderIndex = 0; shaderIndex < tr.numShaders && shaderIndex < MAX_SHADERS; ++shaderIndex )
		{
			int priorRank;
			qboolean alreadySelected = qfalse;
			const stefxShaderCost_t *cost = &s_stefxShaderCosts[shaderIndex];
			for ( priorRank = 0; priorRank < rank; ++priorRank )
			{
				if ( topPressureShaders[priorRank] == shaderIndex )
				{
					alreadySelected = qtrue;
					break;
				}
			}
			if ( !alreadySelected &&
				(cost->passes > candidatePasses ||
				(cost->passes == candidatePasses && cost->indexes > candidateIndexes)) )
			{
				candidate = shaderIndex;
				candidatePasses = cost->passes;
				candidateIndexes = cost->indexes;
			}
		}
		if ( candidate < 0 || candidatePasses == 0 )
		{
			break;
		}
		topPressureShaders[rank] = candidate;
		{
			const stefxShaderCost_t *cost = &s_stefxShaderCosts[candidate];
			const shader_t *shader = tr.shaders[candidate];
			XBLog_WriteProfile( va(
				"STEFX_HW_SHADER_PRESSURE: sample=%u rank=%d shader=%d name='%s' cycles=%u batches=%u passes=%u indexes=%u blend=%u alphaTest=%u twoSided=%u world=%u entity=%u",
				s_stefxShaderCostSerial, rank + 1, candidate,
				shader ? shader->name : "<missing>", cost->cycles, cost->batches,
				cost->passes, cost->indexes, cost->blendPasses, cost->alphaTestPasses,
				cost->twoSidedPasses, cost->worldBatches, cost->entityBatches ) );
		}
	}

	XBLog_WriteProfile( va(
		"STEFX_HW_SHADER_COST_TOTAL: sample=%u shaders=%d cycles=%u batches=%u passes=%u indexes=%u",
		s_stefxShaderCostSerial, tr.numShaders, totalCycles, totalBatches, totalPasses,
		totalIndexes ) );
	XBLog_WriteProfile( va(
		"STEFX_HW_SHADER_PRESSURE_TOTAL: sample=%u shaders=%d batches=%u passes=%u indexes=%u blend=%u alphaTest=%u twoSided=%u world=%u entity=%u",
		s_stefxShaderCostSerial, tr.numShaders, totalBatches, totalPasses, totalIndexes,
		totalBlendPasses, totalAlphaTestPasses, totalTwoSidedPasses,
		totalWorldBatches, totalEntityBatches ) );
	s_stefxShaderCostReportedSerial = s_stefxShaderCostSerial;
}
#endif

#ifdef _XBOX
extern "C" volatile unsigned int g_SPXBEndSurfaceStage;
extern "C" volatile unsigned int g_SPXBEndSurfaceShader;
extern "C" volatile unsigned int g_SPXBEndSurfaceShaderIndex;
extern "C" volatile unsigned int g_SPXBEndSurfaceIterator;
extern "C" volatile unsigned int g_SPXBEndSurfacePasses;
extern "C" volatile unsigned int g_SPXBEndSurfaceVerts;
extern "C" volatile unsigned int g_SPXBEndSurfaceIndexes;
extern "C" volatile unsigned int g_SPXBEndSurfaceFog;
#endif

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBHMScoreBatchPending;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceFlags;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceVerts;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceIndexes;
extern "C" volatile unsigned int g_SPXBHMScoreSurfacePasses;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceState;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitArmed;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceMinXY;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceMaxXY;
#endif

/*
================
R_ArrayElementDiscrete

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY R_ArrayElementDiscrete( GLint index ) {
#ifndef _XBOX
	qglColor4ubv( tess.svars.colors[ index ] );
	if ( glState.currenttmu ) {
		qglMultiTexCoord2fARB( 0, tess.svars.texcoords[ 0 ][ index ][0], tess.svars.texcoords[ 0 ][ index ][1] );
		qglMultiTexCoord2fARB( 1, tess.svars.texcoords[ 1 ][ index ][0], tess.svars.texcoords[ 1 ][ index ][1] );
	} else {
		qglTexCoord2fv( tess.svars.texcoords[ 0 ][ index ] );
	}
	qglVertex3fv( tess.xyz[ index ] );
#endif
}

/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements( int numIndexes, const glIndex_t *indexes, void ( APIENTRY *element )(GLint) ) {
	int i;
	glIndex_t last[3];
	qboolean even;

	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}



/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	int		primitives;

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
		return;
	}

#ifdef _XBOX
	if (primitives == 1 || primitives == 3)
	{
//		if (tess.useConstantColor)
//		{
//			qglDisableClientState( GL_COLOR_ARRAY );
//			qglColor4ubv( tess.constantColor );
//		}
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
#if 1	// VVFIXME : Temporary solution to try and increase framerate
//		qglIndexedTriToStrip( numIndexes, indexes );
#endif
	
		return;
	}
#else // _XBOX
	if ( primitives == 1 ) {
		R_DrawStripElements( numIndexes,  indexes, qglArrayElement );
		return;
	}
	
	if ( primitives == 3 ) {
		R_DrawStripElements( numIndexes,  indexes, R_ArrayElementDiscrete );
		return;
	}
#endif // _XBOX

	// anything else will cause no drawing
}




/*
=============================================================

SURFACE SHADERS

=============================================================
*/

/*
=================
R_BindAnimatedImage

=================
*/
e_status CIN_RunCinematic (int handle);	//cl_cin.cpp
void CIN_UploadCinematic(int handle);

// de-static'd because tr_quicksprite wants it
void R_BindAnimatedImage( const textureBundle_t *bundle ) {
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	static cvar_t *diffuseOnly = NULL;
	if (!diffuseOnly) diffuseOnly = Cvar_Get("stefx_diag_diffuse_only", "0", 0);
	if (diffuseOnly->integer && bundle->isLightmap)
	{
		GL_Bind(tr.whiteImage);
		return;
	}
#endif
	int		index;

/*
	if ( bundle->isVideoMap ) {
		CIN_RunCinematic(bundle->videoMapHandle);
		CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}
*/

	if ((r_fullbright->value /*|| tr.refdef.doFullbright */) && bundle->isLightmap)
	{
		STEFX_RETAIL_SCOPE GL_Bind( tr.whiteImage );
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		STEFX_RETAIL_SCOPE GL_Bind( bundle->image );
		return;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX )
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = myftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
		index >>= FUNCTABLE_SIZE2;

		if ( index < 0 ) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	STEFX_RETAIL_SCOPE GL_Bind( *((image_t**)bundle->image + index) );
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
	STEFX_RETAIL_SCOPE GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	int		i;
	vec3_t	temp;

	STEFX_RETAIL_SCOPE GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {
	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP))
	STEFX_WorldVerticesBeginBatch();
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	STEFX_CoopModelBeginBatch();
#endif
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	R_STEFX_ShaderTraceDumpRegistered();
#endif

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = shader->sky ? RB_StageIteratorSky : RB_StageIteratorGeneric;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
//	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
//		tess.shaderTime = tess.shader->clampTime;
//	}

	tess.fading = false;

#ifdef _XBOX
	tess.setTangents = false;
#endif

	tess.registration++;
}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
extern "C" __declspec(dllexport) volatile unsigned int g_SPXBStasisDraw[40] = {0};
#endif

static void DrawMultitextured( shaderCommands_t *input, int stage ) {
	shaderStage_t	*pStage;

	pStage = &tess.xstages[stage];

	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( tess.shader->multitextureEnv );
	}

	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[1] );

	R_BindAnimatedImage( &pStage->bundle[1] );

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	int stasisSlot = -1;
	if (!Q_stricmp(tess.shader->name, "textures/stasis/scum_256")) stasisSlot = 0;
	else if (!Q_stricmp(tess.shader->name, "textures/stasis/m_stasiswall_b")) stasisSlot = 1;
	if (stasisSlot >= 0)
	{
		volatile unsigned int *row = g_SPXBStasisDraw + stasisSlot * 20;
		++row[0];
		row[1] = pStage->bundle[0].image ? pStage->bundle[0].image->texnum : 0;
		row[2] = pStage->bundle[1].image ? pStage->bundle[1].image->texnum : 0;
		row[3] = pStage->bundle[0].tcGen; row[4] = pStage->bundle[1].tcGen;
		row[5] = pStage->bundle[0].isLightmap; row[6] = pStage->bundle[1].isLightmap;
		row[7] = input->numVertexes; row[8] = input->numIndexes;
		row[9] = tess.shader->multitextureEnv; row[10] = r_lightmap->integer;
		row[11] = stage; row[12] = tess.shader->index;
		for (int uv = 0; uv < 2; ++uv)
		{
			union { float f; unsigned int u; } bits;
			bits.f = input->svars.texcoords[0][0][uv]; row[13+uv] = bits.u;
			bits.f = input->svars.texcoords[1][0][uv]; row[15+uv] = bits.u;
		}
	}
#endif
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
	g_stefxWorldBasePass = true;
#endif
	R_DrawElements( input->numIndexes, input->indexes );
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
	g_stefxWorldBasePass = false;
#endif

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	//qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisable( GL_TEXTURE_2D );
#ifdef _XBOX
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
#endif

	GL_SelectTexture( 0 );
}


#ifdef VV_LIGHTING
static void BuildTangentVectors( void ) {

	memset(tess.tangent, 0, sizeof(vec4_t) * SHADER_MAX_VERTEXES);

	for(int i = 0; i < tess.numIndexes; i += 3)
	{
		vec3_t vec1, vec2, du, dv, cp;

		vec1[0] = tess.xyz[tess.indexes[i+1]][0] - tess.xyz[tess.indexes[i]][0];
		vec1[1] = tess.svars.texcoords[0][tess.indexes[i+1]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec1[2] = tess.svars.texcoords[0][tess.indexes[i+1]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		vec2[0] = tess.xyz[tess.indexes[i+2]][0] - tess.xyz[tess.indexes[i]][0];
		vec2[1] = tess.svars.texcoords[0][tess.indexes[i+2]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec2[2] = tess.svars.texcoords[0][tess.indexes[i+2]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		CrossProduct(vec1, vec2, cp);

		if(cp[0] == 0.0f)
			cp[0] = 0.001f;

		du[0] = -cp[1] / cp[0];
		dv[0] = -cp[2] / cp[0];

		vec1[0] = tess.xyz[tess.indexes[i+1]][1] - tess.xyz[tess.indexes[i]][1];
		vec1[1] = tess.svars.texcoords[0][tess.indexes[i+1]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec1[2] = tess.svars.texcoords[0][tess.indexes[i+1]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		vec2[0] = tess.xyz[tess.indexes[i+2]][1] - tess.xyz[tess.indexes[i]][1];
		vec2[1] = tess.svars.texcoords[0][tess.indexes[i+2]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec2[2] = tess.svars.texcoords[0][tess.indexes[i+2]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		CrossProduct(vec1, vec2, cp);

		if(cp[0] == 0.0f)
			cp[0] = 0.001f;

		du[1] = -cp[1] / cp[0];
		dv[1] = -cp[2] / cp[0];

		vec1[0] = tess.xyz[tess.indexes[i+1]][2] - tess.xyz[tess.indexes[i]][2];
		vec1[1] = tess.svars.texcoords[0][tess.indexes[i+1]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec1[2] = tess.svars.texcoords[0][tess.indexes[i+1]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		vec2[0] = tess.xyz[tess.indexes[i+2]][2] - tess.xyz[tess.indexes[i]][2];
		vec2[1] = tess.svars.texcoords[0][tess.indexes[i+2]][0] - tess.svars.texcoords[0][tess.indexes[i]][0];
		vec2[2] = tess.svars.texcoords[0][tess.indexes[i+2]][1] - tess.svars.texcoords[0][tess.indexes[i]][1];

		CrossProduct(vec1, vec2, cp);

		if(cp[0] == 0.0f)
			cp[0] = 0.001f;

		du[2] = -cp[1] / cp[0];
		dv[2] = -cp[2] / cp[0];

		tess.tangent[tess.indexes[i]][0] += du[0];
		tess.tangent[tess.indexes[i]][1] += du[1];
		tess.tangent[tess.indexes[i]][2] += du[2];

		tess.tangent[tess.indexes[i+1]][0] += du[0];
		tess.tangent[tess.indexes[i+1]][1] += du[1];
		tess.tangent[tess.indexes[i+1]][2] += du[2];

		tess.tangent[tess.indexes[i+2]][0] += du[0];
		tess.tangent[tess.indexes[i+2]][1] += du[1];
		tess.tangent[tess.indexes[i+2]][2] += du[2];
	}
	
	for(i = 0; i < tess.numVertexes; i++)
	{
		VectorNormalizeFast(tess.tangent[i]);
	}
}
#endif // VV_LIGHTING

/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
#if !defined(VV_LIGHTING) || defined(STEFX_ELITE_FORCE_SP)
static void ProjectDlightTexture2( void ) {
	int		i, l;
	vec3_t	origin;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	MAC_STATIC float	oldTexCoordsArray[SHADER_MAX_VERTEXES][2];
	MAC_STATIC float	vertCoordsArray[SHADER_MAX_VERTEXES][4];
	unsigned int		colorArray[SHADER_MAX_VERTEXES];
	glIndex_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	radius;
	int		fogging;
	shaderStage_t *dStage;
	vec3_t	posa;
	vec3_t	posb;
	vec3_t	posc;
	vec3_t	dist;
	vec3_t	e1;
	vec3_t	e2;
	vec3_t	normal;
	float	fac,modulate;
	vec3_t	floatColor;
	byte colorTemp[4];

	int		needResetVerts=0;

#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x44320000; /* 'D200': style-2 dynamic-light entry */
#endif

	if ( !backEnd.refdef.num_dlights ) 
	{
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ )
	{
		dlight_t	*dl;

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x44321000 | (unsigned int)(l & 0xff); /* 'D21x': light entry */
#endif

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;

		int		clipall = 63;
		for ( i = 0 ; i < tess.numVertexes ; i++) 
		{
			int		clip;
			VectorSubtract( origin, tess.xyz[i], dist );

			clip = 0;
			if (  dist[0] < -radius ) 
			{
				clip |= 1;
			}
			else if ( dist[0] > radius ) 
			{
				clip |= 2;
			}
			if (  dist[1] < -radius ) 
			{
				clip |= 4;
			}
			else if ( dist[1] > radius ) 
			{
				clip |= 8;
			}
			if (  dist[2] < -radius ) 
			{
				clip |= 16;
			}
			else if ( dist[2] > radius ) 
			{
				clip |= 32;
			}

			clipBits[i] = clip;
			clipall &= clip;
		}
		if ( clipall ) 
		{
			continue;	// this surface doesn't have any of this light
		}
		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

		#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x44322000 | (unsigned int)(l & 0xff); /* 'D22x': build triangles */
		#endif

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) 
		{
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) 
			{
				continue;	// not lighted
			}

			// copy the vertex positions
			VectorCopy(tess.xyz[a],posa);
			VectorCopy(tess.xyz[b],posb);
			VectorCopy(tess.xyz[c],posc);

			VectorSubtract( posa, posb,e1);
			VectorSubtract( posc, posb,e2);
			CrossProduct(e1,e2,normal);
// rjr - removed for hacking 			if ( (!r_dlightBacks->integer && DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f) || // backface
			if ( DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f || // backface
				DotProduct(normal,normal) < 1E-8f) // junk triangle
			{
				continue;
			}
			VectorNormalize(normal);
			fac=DotProduct(normal,origin)-DotProduct(normal,posa);
			if (fac >= radius)  // out of range
			{
				continue;
			}
			modulate = 1.0f-((fac*fac) / (radius*radius));
			fac = 0.5f/sqrtf(radius*radius - fac*fac);

			// save the verts
			VectorCopy(posa,vertCoordsArray[numIndexes]);
			VectorCopy(posb,vertCoordsArray[numIndexes+1]);
			VectorCopy(posc,vertCoordsArray[numIndexes+2]);

			// now we need e1 and e2 to be an orthonormal basis
			if (DotProduct(e1,e1) > DotProduct(e2,e2))
			{
				VectorNormalize(e1);
				CrossProduct(e1,normal,e2);
			}
			else
			{
				VectorNormalize(e2);
				CrossProduct(normal,e2,e1);
			}
			VectorScale(e1,fac,e1);
			VectorScale(e2,fac,e2);

			VectorSubtract( posa, origin,dist);
			texCoordsArray[numIndexes][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posb, origin,dist);
			texCoordsArray[numIndexes+1][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+1][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posc, origin,dist);
			texCoordsArray[numIndexes+2][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+2][1]=DotProduct(dist,e2)+0.5f;

			if ((texCoordsArray[numIndexes][0] < 0.0f && texCoordsArray[numIndexes+1][0] < 0.0f && texCoordsArray[numIndexes+2][0] < 0.0f) ||
				(texCoordsArray[numIndexes][0] > 1.0f && texCoordsArray[numIndexes+1][0] > 1.0f && texCoordsArray[numIndexes+2][0] > 1.0f) ||
				(texCoordsArray[numIndexes][1] < 0.0f && texCoordsArray[numIndexes+1][1] < 0.0f && texCoordsArray[numIndexes+2][1] < 0.0f) ||
				(texCoordsArray[numIndexes][1] > 1.0f && texCoordsArray[numIndexes+1][1] > 1.0f && texCoordsArray[numIndexes+2][1] > 1.0f) )
			{
				continue; // didn't end up hitting this tri
			}
			/* old code, get from the svars = wrong
			oldTexCoordsArray[numIndexes][0]=tess.svars.texcoords[0][a][0];
			oldTexCoordsArray[numIndexes][1]=tess.svars.texcoords[0][a][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.svars.texcoords[0][b][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.svars.texcoords[0][b][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.svars.texcoords[0][c][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.svars.texcoords[0][c][1];
			*/
			oldTexCoordsArray[numIndexes][0]=tess.texCoords[a][0][0];
			oldTexCoordsArray[numIndexes][1]=tess.texCoords[a][0][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.texCoords[b][0][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.texCoords[b][0][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.texCoords[c][0][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.texCoords[c][0][1];

			colorTemp[0] = myftol(floatColor[0] * modulate);
			colorTemp[1] = myftol(floatColor[1] * modulate);
			colorTemp[2] = myftol(floatColor[2] * modulate);
			colorTemp[3] = 255;
			colorArray[numIndexes]=*(unsigned int *)colorTemp;
			colorArray[numIndexes+1]=*(unsigned int *)colorTemp;
			colorArray[numIndexes+2]=*(unsigned int *)colorTemp;

			hitIndexes[numIndexes] = numIndexes;
			hitIndexes[numIndexes+1] = numIndexes+1;
			hitIndexes[numIndexes+2] = numIndexes+2;
			numIndexes += 3;

			if (numIndexes>=SHADER_MAX_VERTEXES-3)
			{
				break; // we are out of space, so we are done :)
			}
		}

		if ( !numIndexes ) {
			continue;
		}

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x44323000 | (unsigned int)(l & 0xff); /* 'D23x': configure overlay */
#endif

		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (R_STEFX_FogModeForView() == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}


		dStage = NULL;
		if (tess.shader)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].bundle[0].image && !tess.shader->stages[i].bundle[0].isLightmap && !tess.shader->stages[i].bundle[0].numTexMods && tess.shader->stages[i].bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].bundle[0].tcGen != TCGEN_FOG) ||
					 (tess.shader->stages[i].bundle[1].image && !tess.shader->stages[i].bundle[1].isLightmap && !tess.shader->stages[i].bundle[1].numTexMods && tess.shader->stages[i].bundle[1].tcGen != TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].bundle[1].tcGen != TCGEN_FOG)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
                    dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}
		if (!needResetVerts)
		{
			needResetVerts=1;
			if (qglUnlockArraysEXT) 
			{
				qglUnlockArraysEXT();
				GLimp_LogComment( "glUnlockArraysEXT\n" );
			}
		}
		qglVertexPointer (3, GL_FLOAT, 16, vertCoordsArray);	// padded for SIMD

		if (dStage)
		{
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44324000 | (unsigned int)(l & 0xff); /* 'D24x': multitexture overlay */
#endif
			GL_SelectTexture( 0 );
			GL_State(0);
			qglTexCoordPointer( 2, GL_FLOAT, 0, oldTexCoordsArray[0] );
			if (dStage->bundle[0].image && !dStage->bundle[0].isLightmap && !dStage->bundle[0].numTexMods && dStage->bundle[0].tcGen != TCGEN_ENVIRONMENT_MAPPED && dStage->bundle[0].tcGen != TCGEN_FOG)
			{
				R_BindAnimatedImage( &dStage->bundle[0] );
			}
			else
			{
				R_BindAnimatedImage( &dStage->bundle[1] );
			}

			GL_SelectTexture( 1 );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
			STEFX_RETAIL_SCOPE GL_Bind( tr.dlightImage );
			GL_TexEnv( GL_MODULATE );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44325000 | (unsigned int)(l & 0xff); /* 'D25x': light texture ready */
#endif

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44326000 | (unsigned int)(l & 0xff); /* 'D26x': pre-draw */
#endif

			R_DrawElements( numIndexes, hitIndexes );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44327000 | (unsigned int)(l & 0xff); /* 'D27x': draw returned */
#endif

			qglDisable( GL_TEXTURE_2D );
			GL_SelectTexture(0);
		}
		else
		{
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44328000 | (unsigned int)(l & 0xff); /* 'D28x': single-texture overlay */
#endif
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

			STEFX_RETAIL_SCOPE GL_Bind( tr.dlightImage );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			if ( dl->additive ) {
				GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			else {
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x44329000 | (unsigned int)(l & 0xff); /* 'D29x': pre-draw */
#endif

			R_DrawElements( numIndexes, hitIndexes );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x4432A000 | (unsigned int)(l & 0xff); /* 'D2Ax': draw returned */
#endif
		}

		if (fogging)
		{
			qglEnable(GL_FOG);
		}

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
	if (needResetVerts)
	{
		qglVertexPointer (3, GL_FLOAT, 16, tess.xyz);	// padded for SIMD
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, tess.numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
	}
#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x443200FF; /* 'D2FF': style-2 dynamic-light complete */
#endif
}

static void ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	glIndex_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	int		fogging;
	vec3_t	floatColor;
	shaderStage_t *dStage;

#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x444C0000; /* 'DL00': dynamic-light entry */
#endif

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x444C1000 | (unsigned int)(l & 0xff); /* 'DL1x': light entry */
#endif

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x444C2000 | (unsigned int)(l & 0xff); /* 'DL2x': build vertices */
#endif

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	dist;
			int		clip;
			float	modulate;

			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], dist );
			
			int l = 1;
			int bestIndex = 0;
			float greatest = tess.normal[i][0];
			if (greatest < 0.0f)
			{
				greatest = -greatest;
			}

			if (VectorCompare(tess.normal[i], vec3_origin))
			{ //damn you terrain!
				bestIndex = 2;
			}
			else
			{
				while (l < 3)
				{
					if ((tess.normal[i][l] > greatest && tess.normal[i][l] > 0.0f) ||
						(tess.normal[i][l] < -greatest && tess.normal[i][l] < 0.0f))
					{
						greatest = tess.normal[i][l];
						if (greatest < 0.0f)
						{
							greatest = -greatest;
						}
						bestIndex = l;
					}
					l++;
				}
			}

			float dUse = 0.0f;
			const float maxScale = 1.5f;
			const float maxGroundScale = 1.4f;
			const float lightScaleTolerance = 0.1f;

			if (bestIndex == 2)
			{
				dUse = origin[2]-tess.xyz[i][2];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxGroundScale)
				{
					dUse = maxGroundScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}

				if (VectorCompare(tess.normal[i], vec3_origin) ||
					tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[1] * scale;
			}
			else if (bestIndex == 1)
			{
				dUse = origin[1]-tess.xyz[i][1];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				dUse = origin[0]-tess.xyz[i][0];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[1] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}

			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if ( dist[bestIndex] > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist[bestIndex] < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist[bestIndex] = Q_fabs(dist[bestIndex]);
				if ( dist[bestIndex] < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist[bestIndex]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = myftol(floatColor[0] * modulate);
			colors[1] = myftol(floatColor[1] * modulate);
			colors[2] = myftol(floatColor[2] * modulate);
			colors[3] = 255;
		}

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x444C3000 | (unsigned int)(l & 0xff); /* 'DL3x': build indexes */
#endif

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x444C4000 | (unsigned int)(l & 0xff); /* 'DL4x': configure overlay */
#endif

		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (R_STEFX_FogModeForView() == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}


		dStage = NULL;
		if (tess.shader && qglActiveTextureARB)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].bundle[0].image && !tess.shader->stages[i].bundle[0].isLightmap && !tess.shader->stages[i].bundle[0].numTexMods) ||
					 (tess.shader->stages[i].bundle[1].image && !tess.shader->stages[i].bundle[1].isLightmap && !tess.shader->stages[i].bundle[1].numTexMods)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
                    dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}

		if (dStage)
		{
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5000 | (unsigned int)(l & 0xff); /* 'DL5x': multitexture overlay */
#endif
			GL_SelectTexture( 0 );
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5100 | (unsigned int)(l & 0xff); /* 'DLQx': texture zero selected */
#endif
			GL_State(0);
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5200 | (unsigned int)(l & 0xff); /* 'DLRx': base state ready */
#endif
			qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );
			if (dStage->bundle[0].image && !dStage->bundle[0].isLightmap && !dStage->bundle[0].numTexMods)
			{
				R_BindAnimatedImage( &dStage->bundle[0] );
			}
			else
			{
				R_BindAnimatedImage( &dStage->bundle[1] );
			}

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5300 | (unsigned int)(l & 0xff); /* 'DLSx': base image ready */
#endif

			GL_SelectTexture( 1 );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
			STEFX_RETAIL_SCOPE GL_Bind( tr.dlightImage );
			GL_TexEnv( GL_MODULATE );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5400 | (unsigned int)(l & 0xff); /* 'DLTx': light texture ready */
#endif

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5500 | (unsigned int)(l & 0xff); /* 'DLUx': pre-draw */
#endif

			R_DrawElements( numIndexes, hitIndexes );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C5600 | (unsigned int)(l & 0xff); /* 'DLVx': draw returned */
#endif

			qglDisable( GL_TEXTURE_2D );
			GL_SelectTexture(0);
		}
		else
		{
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C6000 | (unsigned int)(l & 0xff); /* 'DL`x': single-texture overlay */
#endif
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

			STEFX_RETAIL_SCOPE GL_Bind( tr.dlightImage );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			if ( dl->additive ) {
				GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			else {
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C6100 | (unsigned int)(l & 0xff); /* 'DMax': pre-draw */
#endif

			R_DrawElements( numIndexes, hitIndexes );

#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x444C6200 | (unsigned int)(l & 0xff); /* 'DLbx': draw returned */
#endif
		}

		if (fogging)
		{
			qglEnable(GL_FOG);
		}

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x444C00FF; /* 'DLFF': dynamic-light complete */
#endif
}
#endif // !VV_LIGHTING || STEFX_ELITE_FORCE_SP

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	int			i;

	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		* ( int * )&tess.svars.colors[i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	STEFX_RETAIL_SCOPE GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
}

/*
===============
ComputeColors
===============
*/
#ifdef _XBOX
static void ComputeColors( shaderStage_t *pStage, int forceRGBGen )
{
	int i;
	qboolean killGen = qfalse;
	alphaGen_t forceAlphaGen = (alphaGen_t) pStage->alphaGen;//set this up so we can override below

	if ( tess.shader != tr.projectionShadowShader && tess.shader != tr.shadowShader && 
		( backEnd.currentEntity->e.renderfx & (RF_DISINTEGRATE1|RF_DISINTEGRATE2)))
	{
		RB_CalcDisintegrateColors( tess.svars.colors );
		RB_CalcDisintegrateVertDeform();

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		killGen = qtrue;
	}

	//
	// rgbGen
	//
	if ( !forceRGBGen )
	{
		forceRGBGen = pStage->rgbGen;
	}

	if ( backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC ) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		int			i;
		float		*normal, dot;
		DWORD *color;
		int			numVertexes;

		normal = tess.normal[0];
		color = tess.svars.colors;

		numVertexes = tess.numVertexes;

		for ( i = 0 ; i < numVertexes ; i++, normal += 4, color ++) 
		{
			dot = DotProduct( normal, backEnd.refdef.viewaxis[0] );

			dot *= dot * dot * dot;

			if ( dot < 0.2f ) // so low, so just clamp it
			{
				dot = 0.0f;
			}

			*color = D3DCOLOR_RGBA( (int)(backEnd.currentEntity->e.shaderRGBA[0] * (1-dot)),
				(int)(backEnd.currentEntity->e.shaderRGBA[0] * (1-dot)),
				(int)(backEnd.currentEntity->e.shaderRGBA[0] * (1-dot)),
				(int)(backEnd.currentEntity->e.shaderRGBA[0] * (1-dot)) );
		}

		killGen = qtrue;
	}

	if (killGen)
	{
		goto avoidGen;
	}

	DWORD color;

	switch ( forceRGBGen )
	{
	case CGEN_IDENTITY:
		memset( tess.svars.colors, 0xffffffff, sizeof(DWORD) * tess.numVertexes );
		break;
	default:
	case CGEN_IDENTITY_LIGHTING:
		color = ((tr.identityLightByte & 0xff) << 24 |
			(tr.identityLightByte & 0xff) << 16 |
			(tr.identityLightByte & 0xff) << 8  |
			(tr.identityLightByte & 0xff) << 0);
		memset( tess.svars.colors, color, sizeof(DWORD) * tess.numVertexes );
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( tess.svars.colors );
		break;
	case CGEN_LIGHTING_DIFFUSE_ENTITY:
		RB_CalcDiffuseEntityColor( tess.svars.colors );
		if ( forceAlphaGen == AGEN_IDENTITY && 
			backEnd.currentEntity->e.shaderRGBA[3] == 0xff 
			)
		{
			forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
		}
		break;
	case CGEN_EXACT_VERTEX:
		for( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i] = D3DCOLOR_RGBA( (int)(tess.vertexColors[i][0]),
				(int)(tess.vertexColors[i][1]),
				(int)(tess.vertexColors[i][2]),
				(int)(tess.vertexColors[i][3]) );
		}
		break;
	case CGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i] = D3DCOLOR_RGBA( (int)(pStage->constantColor[0]),
				(int)(pStage->constantColor[1]),
				(int)(pStage->constantColor[2]),
				(int)(pStage->constantColor[3]) );
		}
		break;
	case CGEN_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i] = D3DCOLOR_RGBA( (int)(tess.vertexColors[i][0]),
					(int)(tess.vertexColors[i][1]),
					(int)(tess.vertexColors[i][2]),
					(int)(tess.vertexColors[i][3]));
			}
		}
		else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i] = D3DCOLOR_RGBA( (int)(tess.vertexColors[i][0] * tr.identityLight),
					(int)(tess.vertexColors[i][1] * tr.identityLight),
					(int)(tess.vertexColors[i][2] * tr.identityLight),
					(int)(tess.vertexColors[i][3]));
			}
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i] = D3DCOLOR_XRGB( (int)(255 - tess.vertexColors[i][0]),
					(int)(255 - tess.vertexColors[i][1]),
					(int)(255 - tess.vertexColors[i][2]));
			}
		}
		else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i] = D3DCOLOR_XRGB( (int)((255 - tess.vertexColors[i][0]) * tr.identityLight),
					(int)((255 - tess.vertexColors[i][1]) * tr.identityLight),
					(int)((255 - tess.vertexColors[i][2]) * tr.identityLight));
			}
		}
		break;
	case CGEN_FOG:
		{
			fog_t		*fog;

			fog = tr.world->fogs + tess.fogNum;

			for ( i = 0; i < tess.numVertexes; i++ ) {
				* ( int * )&tess.svars.colors[i] = fog->colorInt;
			}
		}
		break;
	case CGEN_WAVEFORM:
		STEFX_RETAIL_SCOPE RB_CalcWaveColor( &pStage->rgbWave, tess.svars.colors );
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( tess.svars.colors );
		if ( forceAlphaGen == AGEN_IDENTITY && 
			backEnd.currentEntity->e.shaderRGBA[3] == 0xff 
			)
		{
			forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
		}
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( tess.svars.colors );
		break;
	case CGEN_LIGHTMAPSTYLE:
		for ( i = 0; i < tess.numVertexes; i++ ) 
		{
			tess.svars.colors[i] = *(DWORD *)styleColors[pStage->lightmapStyle];
		}
		break;
	}

	//
	// alphaGen
	//
	DWORD rgb;
	switch ( forceAlphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( forceRGBGen != CGEN_IDENTITY &&  forceRGBGen != CGEN_LIGHTING_DIFFUSE ) {
			if ( ( forceRGBGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				forceRGBGen != CGEN_VERTEX ) {
					for ( i = 0; i < tess.numVertexes; i++ ) {
						rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
						tess.svars.colors[i] = rgb | ((255 & 0xff) << 24);
					}
				}
		}
		break;
	case AGEN_CONST:
		if ( forceRGBGen != CGEN_CONST ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
				tess.svars.colors[i] = rgb | ((pStage->constantColor[3] & 0xff) << 24);
			}
		}
		break;
	case AGEN_WAVEFORM:
		STEFX_RETAIL_SCOPE RB_CalcWaveAlpha( &pStage->alphaWave, tess.svars.colors );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( tess.svars.colors );
		break;
	case AGEN_ENTITY:
		if ( forceRGBGen != CGEN_ENTITY ) { //already got it in the CGEN_entity since it does all 4 components
			RB_CalcAlphaFromEntity( tess.svars.colors );
		}
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( tess.svars.colors );
		break;
	case AGEN_VERTEX:
		if ( forceRGBGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
				tess.svars.colors[i] = rgb | ((tess.vertexColors[i][3] & 0xff) << 24);
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
			tess.svars.colors[i] = rgb | (((255 - tess.vertexColors[i][3]) & 0xff) << 24);
		}
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.or.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
				tess.svars.colors[i] = rgb | ((alpha & 0xff) << 24);
			}
		}
		break;
	case AGEN_BLEND:
		if ( forceRGBGen != CGEN_VERTEX ) 
		{
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				rgb = (DWORD)((tess.svars.colors[i]) & 0x00ffffff);
				tess.svars.colors[i] = rgb | ((tess.vertexAlphas[i][pStage->index] & 0xff) << 24);
			}
		}
		break;
	}

avoidGen:
	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( tess.svars.colors );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( tess.svars.colors );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( tess.svars.colors );
			break;
		case ACFF_NONE:
			break;
		}
	}
}
#else // _XBOX
static void ComputeColors( shaderStage_t *pStage, int forceRGBGen )
{
	int			i;
	color4ub_t	*colors = tess.svars.colors;
	qboolean killGen = qfalse;
	alphaGen_t forceAlphaGen = pStage->alphaGen;//set this up so we can override below

	if ( tess.shader != tr.projectionShadowShader && tess.shader != tr.shadowShader && 
			( backEnd.currentEntity->e.renderfx & (RF_DISINTEGRATE1|RF_DISINTEGRATE2)))
	{
		RB_CalcDisintegrateColors( (unsigned char *)tess.svars.colors );
		RB_CalcDisintegrateVertDeform();

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		killGen = qtrue;
	}

	//
	// rgbGen
	//
	if ( !forceRGBGen )
	{
		forceRGBGen = pStage->rgbGen;
	}

	if ( backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC ) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		int			i;
		float		*normal, dot;
		unsigned char *color;
		int			numVertexes;

		normal = tess.normal[0];
		color = tess.svars.colors[0];

		numVertexes = tess.numVertexes;

		for ( i = 0 ; i < numVertexes ; i++, normal += 4, color += 4) 
		{
			dot = DotProduct( normal, backEnd.refdef.viewaxis[0] );

			dot *= dot * dot * dot;

			if ( dot < 0.2f ) // so low, so just clamp it
			{
				dot = 0.0f;
			}

			color[0] = color[1] = color[2] = color[3] = myftol( backEnd.currentEntity->e.shaderRGBA[0] * (1-dot) );
		}

		killGen = qtrue;
	}

	if (killGen)
	{
		goto avoidGen;
	}

	//
	// rgbGen
	//
	switch ( forceRGBGen )
	{
		case CGEN_IDENTITY:
			Com_Memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_LIGHTING_DIFFUSE_ENTITY:
			RB_CalcDiffuseEntityColor( ( unsigned char * ) tess.svars.colors );
			if ( forceAlphaGen == AGEN_IDENTITY && 
				 backEnd.currentEntity->e.shaderRGBA[3] == 0xff 
				)
			{
				forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
			}
			break;
		case CGEN_EXACT_VERTEX:
			Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				*(int *)tess.svars.colors[i] = *(int *)pStage->constantColor;
			}
			break;
		case CGEN_VERTEX:
			if ( tr.identityLight == 1 )
			{
				Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
					tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
					tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
					tess.svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
					tess.svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_FOG:
			{
				fog_t		*fog;

				fog = tr.world->fogs + tess.fogNum;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					* ( int * )&tess.svars.colors[i] = fog->colorInt;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor( &pStage->rgbWave, ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity( ( unsigned char * ) tess.svars.colors );
			if ( forceAlphaGen == AGEN_IDENTITY && 
				 backEnd.currentEntity->e.shaderRGBA[3] == 0xff 
				)
			{
				forceAlphaGen = AGEN_SKIP;	//already got it in this set since it does all 4 components
			}
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_LIGHTMAPSTYLE:
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				*(unsigned *)&colors[i] = *(unsigned *)styleColors[pStage->lightmapStyle];
			}
			break;
	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( forceRGBGen != CGEN_IDENTITY ) {
			if ( ( forceRGBGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				 forceRGBGen != CGEN_VERTEX ) {
				for ( i = 0; i < tess.numVertexes; i++ ) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_CONST:
		if ( forceRGBGen != CGEN_CONST ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = pStage->constantColor[3];
			}
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
		break;
    case AGEN_VERTEX:
		if ( forceRGBGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
        break;
    case AGEN_ONE_MINUS_VERTEX:
        for ( i = 0; i < tess.numVertexes; i++ )
        {
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
        }
        break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.ori.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_BLEND:
		if ( forceRGBGen != CGEN_VERTEX ) 
		{
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				colors[i][3] = tess.vertexAlphas[i][pStage->index]; //rwwRMG - added support
			}
		}
		break;
	}
avoidGen:
	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_NONE:
			break;
		}
	}
}
#endif

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;
    float	*texcoords;

	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

        texcoords = (float *)tess.svars.texcoords[b];
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][1][0];
				texcoords[1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_LIGHTMAP1:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][2][0];
				texcoords[1] = tess.texCoords[i][2][1];
			}
			break;
		case TCGEN_LIGHTMAP2:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][3][0];
				texcoords[1] = tess.texCoords[i][3][1];
			}
			break;
		case TCGEN_LIGHTMAP3:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][4][0];
				texcoords[1] = tess.texCoords[i][4][1];
			}
			break;
		case TCGEN_VECTOR:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[b] );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
//#ifdef VV_LIGHTING
//			tess.shader->stages[tess.currentPass].isEnvironment = qtrue;
//#else
			RB_CalcEnvironmentTexCoords( ( float * ) tess.svars.texcoords[b] );
//#endif
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				STEFX_RETAIL_SCOPE RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].translate,	//scroll unioned
										 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].translate,
									 ( float * ) tess.svars.texcoords[b] );
				break;
			
			case TMOD_STRETCH:
				STEFX_RETAIL_SCOPE RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						               ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				STEFX_RETAIL_SCOPE RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].translate[0],
										( float * ) tess.svars.texcoords[b] );
				break;

			default:
				Com_Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

void ForceAlpha(unsigned char *dstColors, int TR_ForceEntAlpha)
{
	int	i;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = TR_ForceEntAlpha;
	}
}

/*
** RB_IterateStagesGeneric
*/
static vec4_t	GLFogOverrideColors[GLFOGOVERRIDE_MAX] =
{
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_NONE
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_BLACK
	{ 1.0, 1.0, 1.0, 1.0 }	// GLFOGOVERRIDE_WHITE
};

static const float logtestExp2 = (sqrt( -log( 1.0 / 255.0 ) ));
extern bool tr_stencilled; //tr_backend.cpp

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean R_STEFX_IsSplitEconomyStageSkipped( int stage,
	const shaderStage_t *shaderStage )
{
	if ( stage <= 0 || !shaderStage || !backEnd.viewParms.stefxSplitEconomy ||
		backEnd.projection2D )
	{
		return qfalse;
	}

	if ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity )
	{
		if ( backEnd.currentEntity->e.renderfx & RF_DISTORTION )
		{
			return qfalse;
		}
		#if defined(STEFX_SP_HOSTED_MP)
		if ( backEnd.currentEntity->e.renderfx & RF_STEFX_FORCE_ENT_ALPHA )
		{
			return qfalse;
		}
		#endif
		return qtrue;
	}

	return shaderStage->isDetail ? qtrue : qfalse;
}

static qboolean R_STEFX_SkipSplitEconomyStage( int stage,
	const shaderStage_t *shaderStage )
{
	static int s_stefxEntityPassLogBudget = 8;
	static int s_stefxDetailPassLogBudget = 8;

	if ( !R_STEFX_IsSplitEconomyStageSkipped( stage, shaderStage ) )
	{
		return qfalse;
	}

	if ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity )
	{
		if ( s_stefxEntityPassLogBudget > 0 )
		{
			XBLog_WriteCriticalf( "STEFX_SPLIT_ECONOMY: entityPassSkipped shader='%s' stage=%d slot=%d",
				tess.shader ? tess.shader->name : "<none>", stage,
				backEnd.viewParms.stefxSplitSlot );
			--s_stefxEntityPassLogBudget;
		}
	}
	else if ( s_stefxDetailPassLogBudget > 0 )
	{
		XBLog_WriteCriticalf( "STEFX_SPLIT_ECONOMY: detailPassSkipped shader='%s' stage=%d slot=%d",
			tess.shader ? tess.shader->name : "<none>", stage,
			backEnd.viewParms.stefxSplitSlot );
		--s_stefxDetailPassLogBudget;
	}

	return qtrue;
}

static int R_STEFX_SplitEconomyEffectivePassCount( const shaderCommands_t *input )
{
	int effectivePasses = 0;
	int stage;

	if ( !input || !input->shader || !backEnd.viewParms.stefxSplitEconomy ||
		backEnd.projection2D )
	{
		return input ? input->numPasses : 0;
	}

	for ( stage = 0; stage < input->shader->numUnfoggedPasses; ++stage )
	{
		const shaderStage_t *shaderStage = &input->xstages[stage];

		if ( R_STEFX_IsSplitEconomyStageSkipped( stage, shaderStage ) )
		{
			continue;
		}
		if ( !shaderStage->active )
		{
			break;
		}
		if ( stage && r_lightmap->integer &&
			!( shaderStage->bundle[0].isLightmap ||
				shaderStage->bundle[1].isLightmap ||
				shaderStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
		if ( shaderStage->ss && shaderStage->ss->surfaceSpriteType )
		{
			continue;
		}

		++effectivePasses;
	}

	return effectivePasses;
}
#endif

static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	int stage;
	bool	UseGLFog = false;
	bool	FogColorChange = false;
	fog_t	*fog = NULL;

#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x49543030; /* 'IT00': generic pass iteration entered */
#endif

	if (tess.fogNum && tess.shader->fogPass && (tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs) 
		&& R_STEFX_FogModeForView() == 2)
	{	// only gl fog global fog and the "special fog"
		fog = tr.world->fogs + tess.fogNum;

		if (tr.rangedFog)
		{ //ranged fog, used for sniper scope
			float fStart = fog->parms.depthForOpaque;

			if (tr.rangedFog < 0.0f)
			{ //special designer override
				fStart = -tr.rangedFog;
			}
			else
			{
				//the greater tr.rangedFog is, the more fog we will get between the view point and cull distance
				if ((tr.distanceCull-fStart) < tr.rangedFog)
				{ //assure a minimum range between fog beginning and cutoff distance
					fStart = tr.distanceCull-tr.rangedFog;

					if (fStart < 16.0f)
					{
						fStart = 16.0f;
					}
				}
			}

			qglFogi(GL_FOG_MODE, GL_LINEAR);
			
			qglFogf(GL_FOG_START, fStart);
			qglFogf(GL_FOG_END, tr.distanceCull);
		}
		else
		{
			qglFogi(GL_FOG_MODE, GL_EXP2);
			qglFogf(GL_FOG_DENSITY, logtestExp2 / fog->parms.depthForOpaque);
		}
		if ( g_bRenderGlowingObjects )
		{
			const float fogColor[3] = { 0.0f, 0.0f, 0.0f };
			qglFogfv(GL_FOG_COLOR, fogColor );
		}
		else
		{
			qglFogfv(GL_FOG_COLOR, fog->parms.color);
		}
		qglEnable(GL_FOG);
		UseGLFog = true;
	}

	for ( stage = 0; stage < input->shader->numUnfoggedPasses; stage++ )
	{
		shaderStage_t *pStage = &tess.xstages[stage];
		int forceRGBGen = 0;
		int stateBits = 0;

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x49543100 | (unsigned int)(stage & 0xff); /* 'IT1x': stage setup */
#endif

		#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( R_STEFX_SkipSplitEconomyStage( stage, pStage ) )
		{
			continue;
		}
		#endif

		if ( !pStage->active )
		{
			assert(pStage->active);//wtf?
			break;
		}

		// Reject this stage if it's not a glow stage but we are doing a glow pass.
/*
		if ( g_bRenderGlowingObjects && !pStage->glow )
		{
			continue;
		}
*/

#ifdef _XBOX
		tess.currentPass = stage;
#endif

		if ( stage && r_lightmap->integer && !( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}

		stateBits = pStage->stateBits;
		if ( backEnd.projection2D )
		{
			stateBits |= GLS_DEPTHTEST_DISABLE;
		}

		if ( backEnd.currentEntity )
		{
			assert(backEnd.currentEntity->e.renderfx >= 0);

			if ( backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1 )
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_C0;
			}

			if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
			{//want to use RGBGen from ent
				forceRGBGen = CGEN_ENTITY;
			}
		}

		if (pStage->ss && pStage->ss->surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

		if (UseGLFog)
		{
			if (pStage->mGLFogColorOverride)
			{
				qglFogfv(GL_FOG_COLOR, GLFogOverrideColors[pStage->mGLFogColorOverride]);
				FogColorChange = true;
			}
			else if (FogColorChange && fog)
			{
				FogColorChange = false;
				qglFogfv(GL_FOG_COLOR, fog->parms.color);
			}
		}

#ifdef _XBOX
		qglDisable(GL_LIGHTING);
#endif

		if (!input->fading)
		{ //this means ignore this, while we do a fade-out
			ComputeColors( pStage, forceRGBGen );
		}
		ComputeTexCoords( pStage );

		if ( !setArraysOnce )
		{
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors );
		}

#ifdef VV_LIGHTING
		if(pStage->rgbGen == CGEN_LIGHTING_DIFFUSE ||
			pStage->rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY)
		{
            qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer(GL_FLOAT, 16, tess.normal );
		}

		/*if(pStage->isSpecular)
		{
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer(GL_FLOAT, 16, tess.normal );
			if(!tess.setTangents)
                BuildTangentVectors();
			qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
			R_BindAnimatedImage( &pStage->bundle[0] );
			GL_State( stateBits );
			glw_state->lightEffects->RenderSpecular();
			qglDisableClientState( GL_NORMAL_ARRAY );
			continue;
		}*/
		if(pStage->isEnvironment)
		{
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer( GL_FLOAT, 16, tess.normal );
			R_BindAnimatedImage( &pStage->bundle[0] );
			GL_State( stateBits );
			glw_state->lightEffects->RenderEnvironment();
			qglDisableClientState( GL_NORMAL_ARRAY );
			continue;
		}
		if(pStage->isBumpMap)
		{
			qglEnableClientState( GL_NORMAL_ARRAY );
			qglNormalPointer( GL_FLOAT, 16, tess.normal );
			if(!tess.setTangents)
                BuildTangentVectors();
			GL_SelectTexture( 0 );
			R_BindAnimatedImage( &pStage->bundle[0] );
			GL_SelectTexture( 1 );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			R_BindAnimatedImage( &pStage->bundle[1] );
			GL_State( stateBits );
			glw_state->lightEffects->RenderBump();
			qglDisable( GL_TEXTURE_2D );
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
			GL_SelectTexture( 0 );
			qglDisableClientState( GL_NORMAL_ARRAY );
			continue;
		}
#endif // VV_LIGHTING

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image != 0 )
		{
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x49543200 | (unsigned int)(stage & 0xff); /* 'IT2x': multitexture submit */
#endif
			DrawMultitextured( input, stage );
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x49543300 | (unsigned int)(stage & 0xff); /* 'IT3x': multitexture returned */
#endif
		}
		else
		{
			static bool lStencilled = false;

			if ( !setArraysOnce )
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
			}

			//
			// set state
			//
			if ( (tess.shader == tr.distortionShader) || 
				 (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_DISTORTION)) )
			{ //special distortion effect -rww
				//tr.screenImage should have been set for this specific entity before we got in here.
				STEFX_RETAIL_SCOPE GL_Bind( tr.screenImage );
				STEFX_RETAIL_SCOPE GL_Cull(CT_TWO_SIDED);
			}
			else if ( pStage->bundle[0].vertexLightmap && r_vertexLight->integer && r_lightmap->integer )
			{
				STEFX_RETAIL_SCOPE GL_Bind( tr.whiteImage );
			}
			else 
				R_BindAnimatedImage( &pStage->bundle[0] );

			if (tess.shader == tr.distortionShader &&
				glConfig.stencilBits >= 4)
			{ //draw it to the stencil buffer!
				tr_stencilled = true;
				lStencilled = true;
				qglEnable(GL_STENCIL_TEST);
				qglStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

				//don't depthmask, don't blend.. don't do anything
				GL_State(0);
			}
			#if defined(STEFX_SP_HOSTED_MP)
			else if ( backEnd.currentEntity &&
				( backEnd.currentEntity->e.renderfx & RF_STEFX_FORCE_ENT_ALPHA ) )
			{
				ForceAlpha( (unsigned char *)tess.svars.colors,
					backEnd.currentEntity->e.shaderRGBA[3] );
				GL_State( GLS_SRCBLEND_SRC_ALPHA |
					GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
			}
			#endif
			else
			{
				GL_State( stateBits );
			}

			//
			// draw
			//
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x49543400 | (unsigned int)(stage & 0xff); /* 'IT4x': indexed submit */
#endif
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
			g_stefxWorldBasePass = true;
#endif
			R_DrawElements( input->numIndexes, input->indexes );	
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
			g_stefxWorldBasePass = false;
#endif
#ifdef _XBOX
			g_SPXBEndSurfaceStage = 0x49543500 | (unsigned int)(stage & 0xff); /* 'IT5x': indexed submit returned */
#endif

			if (lStencilled)
			{ //re-enable the color buffer, disable stencil test
				lStencilled = false;
				qglDisable(GL_STENCIL_TEST);
				qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
		}

#ifdef VV_LIGHTING
		// Lighting may have been turned on above
		qglDisable(GL_LIGHTING);
		qglDisableClientState( GL_NORMAL_ARRAY );
#endif

#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x49543900 | (unsigned int)(stage & 0xff); /* 'IT9x': stage complete */
#endif
	}
	if (FogColorChange)
	{
		qglFogfv(GL_FOG_COLOR, fog->parms.color);
	}
#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x49543a30; /* 'IT:0': all generic stages complete */
#endif
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	int stage;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	int stefxEffectivePasses;
	static int s_stefxArraySetupLogBudget = 8;
#endif

	input = &tess;

	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473030; /* 'SG00': stage iterator entered */
	#endif
	RB_DeformTessGeometry();
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473031; /* 'SG01': deformation complete */
	#endif

	//
	// log this call
	//
#ifndef _XBOX
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}
#endif

	//
	// set face culling appropriately
	//
	STEFX_RETAIL_SCOPE GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	stefxEffectivePasses = R_STEFX_SplitEconomyEffectivePassCount( input );
	if ( stefxEffectivePasses < tess.numPasses && s_stefxArraySetupLogBudget > 0 )
	{
		XBLog_WriteCriticalf( "STEFX_SPLIT_ECONOMY: arraySetupPasses shader='%s' source=%d effective=%d slot=%d",
			input->shader ? input->shader->name : "<none>", tess.numPasses,
			stefxEffectivePasses, backEnd.viewParms.stefxSplitSlot );
		--s_stefxArraySetupLogBudget;
	}
	if ( stefxEffectivePasses > 1 || input->shader->multitextureEnv )
#else
	if ( tess.numPasses > 1 || input->shader->multitextureEnv )
#endif
	{
		setArraysOnce = qfalse;
		qglDisableClientState (GL_COLOR_ARRAY);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		setArraysOnce = qtrue;

		qglEnableClientState( GL_COLOR_ARRAY);
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );
	}

	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if ( !setArraysOnce )
	{
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglEnableClientState( GL_COLOR_ARRAY );
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473032; /* 'SG02': generic stages complete */
	#endif

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
#if defined(VV_LIGHTING) && !defined(STEFX_ELITE_FORCE_SP)
		qglEnableClientState( GL_NORMAL_ARRAY );
		qglNormalPointer(GL_FLOAT, 16, tess.normal );
		if(!tess.setTangents)
            BuildTangentVectors();
		glw_state->lightEffects->RenderDynamicLights();
		qglDisableClientState( GL_NORMAL_ARRAY );
#else
		if (r_dlightStyle->integer>0)
		{
			ProjectDlightTexture2();
		}
		else
		{
			ProjectDlightTexture();
		}
#endif
	}
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473033; /* 'SG03': dynamic lights complete */
	#endif

	//
	// now do fog
	//
	if (tr.world && (tess.fogNum != tr.world->globalFog || R_STEFX_FogModeForView() != 2) && R_STEFX_FogModeForView() && tess.fogNum && tess.shader->fogPass)
	{
		RB_FogPass();
	}
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473034; /* 'SG04': fog pass complete */
	#endif

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473035; /* 'SG05': array unlock complete */
	#endif

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < tess.shader->numUnfoggedPasses; stage++ )
		{
			if (tess.xstages[stage].ss && tess.xstages[stage].ss->surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(&tess.xstages[stage], input);
			}
		}
	}
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473036; /* 'SG06': surface sprites complete */
	#endif

	//don't disable the hardware fog til after we do surface sprites
	if (R_STEFX_FogModeForView() == 2 &&
		tess.fogNum && tess.shader->fogPass &&
		(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
	{
		qglDisable(GL_FOG);
	}
	#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x53473037; /* 'SG07': iterator complete */
	#endif
}


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	const qboolean stefxShaderCostSample = R_STEFX_BeginShaderCostSample();
	unsigned __int64 stefxShaderCostStart = 0;
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	const qboolean stefxScoreBatch = g_SPXBHMScoreBatchPending ? qtrue : qfalse;
#endif

	input = &tess;

#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x45530000; /* 'ES00': entry */
	g_SPXBEndSurfaceShader = (unsigned int)input->shader;
	g_SPXBEndSurfaceShaderIndex = input->shader ? (unsigned int)input->shader->index : 0xffffffff;
	g_SPXBEndSurfaceIterator = (unsigned int)input->currentStageIteratorFunc;
	g_SPXBEndSurfacePasses = (unsigned int)input->numPasses;
	g_SPXBEndSurfaceVerts = (unsigned int)input->numVertexes;
	g_SPXBEndSurfaceIndexes = (unsigned int)input->numIndexes;
	g_SPXBEndSurfaceFog = (unsigned int)input->fogNum;
#endif

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( stefxScoreBatch ) {
		float minX = input->xyz[0][0];
		float minY = input->xyz[0][1];
		float maxX = minX;
		float maxY = minY;
		int vertexIndex;
		union { float f; unsigned short h[2]; } packed;
		for ( vertexIndex = 1; vertexIndex < input->numVertexes; ++vertexIndex ) {
			if ( input->xyz[vertexIndex][0] < minX ) minX = input->xyz[vertexIndex][0];
			if ( input->xyz[vertexIndex][1] < minY ) minY = input->xyz[vertexIndex][1];
			if ( input->xyz[vertexIndex][0] > maxX ) maxX = input->xyz[vertexIndex][0];
			if ( input->xyz[vertexIndex][1] > maxY ) maxY = input->xyz[vertexIndex][1];
		}
		packed.f = minX;
		g_SPXBHMScoreSurfaceMinXY = (unsigned int)packed.h[1] << 16;
		packed.f = minY;
		g_SPXBHMScoreSurfaceMinXY |= packed.h[1];
		packed.f = maxX;
		g_SPXBHMScoreSurfaceMaxXY = (unsigned int)packed.h[1] << 16;
		packed.f = maxY;
		g_SPXBHMScoreSurfaceMaxXY |= packed.h[1];
		g_SPXBHMScoreSurfaceFlags = 1u;
		g_SPXBHMScoreSurfaceVerts = (unsigned int)input->numVertexes;
		g_SPXBHMScoreSurfaceIndexes = (unsigned int)input->numIndexes;
		g_SPXBHMScoreSurfacePasses = (unsigned int)input->numPasses;
		g_SPXBHMScoreSurfaceState = (input->xstages && input->numPasses > 0) ?
			(unsigned int)input->xstages[0].stateBits : 0xffffffffu;
	}
#endif

	if (input->numIndexes == 0) {
#ifdef _XBOX
		g_SPXBEndSurfaceStage = 0x45530001; /* 'ES01': empty */
#endif
		return;
	}

#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x45530010; /* 'ES10': bounds checks */
#endif
	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		Com_Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		Com_Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

/*
	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}
*/

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
		if ( stefxScoreBatch ) g_SPXBHMScoreBatchPending = 0;
#endif
		return;
	}

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( stefxScoreBatch ) g_SPXBHMScoreSurfaceFlags |= 2u;
#endif

	if ( skyboxportal )
	{
		// world
		if(!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL)) 
		{
			if(tess.currentStageIteratorFunc == RB_StageIteratorSky)
			{	// don't process these tris at all
				return;
			}
		}
		// portal sky
		else
		{
			if(!drawskyboxportal)
			{
				if( !(tess.currentStageIteratorFunc == RB_StageIteratorSky))
				{	// /only/ process sky tris
					return;
				}
			}
		}
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;
	if (tess.fogNum && tess.shader->fogPass && R_STEFX_FogModeForView() == 1)
	{	
		backEnd.pc.c_totalIndexes += tess.numIndexes;
	}

	//
	// call off to shader specific tess end function
	//
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( stefxScoreBatch ) {
		g_SPXBHMScoreSurfaceFlags |= 4u;
		g_SPXBHMScoreSubmitArmed = 1u;
	}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if ( stefxShaderCostSample )
	{
		stefxShaderCostStart = STEFX_XboxReadTsc();
	}
#endif
#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x45530050; /* 'ES50': stage iterator */
#endif
	tess.currentStageIteratorFunc();
#ifdef _XBOX
	g_SPXBEndSurfaceStage = 0x45530051; /* 'ES51': iterator complete */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if ( stefxShaderCostSample )
	{
		R_STEFX_RecordShaderCost( input, STEFX_XboxElapsedCycles( stefxShaderCostStart ) );
	}
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( stefxScoreBatch ) {
		g_SPXBHMScoreSubmitArmed = 0;
		g_SPXBHMScoreBatchPending = 0;
		g_SPXBHMScoreSurfaceFlags |= 8u;
	}
#endif

#ifdef _XBOX
	tess.currentPass = 0;
	g_SPXBEndSurfaceStage = 0x455300FF; /* 'ESFF': complete */
#endif

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer && com_developer->integer ) {
		DrawTris (input);
	}
	if ( r_shownormals->integer && com_developer->integer && com_sv_running->integer ) {
		DrawNormals (input);
	}
	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment( "----------\n" );
}

STEFX_RETAIL_NAMESPACE_END
