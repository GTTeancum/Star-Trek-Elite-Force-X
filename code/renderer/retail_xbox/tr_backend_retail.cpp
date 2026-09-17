//Anything above this #include will be ignored by the compiler
#include "../../server/exe_headers.h"

#include "../tr_local.h"
#include "retail_renderer_contract.h"
#include "../../win32/xb_perf.h"

#ifndef DEDICATED
#if !defined __TR_WORLDEFFECTS_H
	#include "../tr_WorldEffects.h"
#endif
#endif

#ifdef _XBOX
#include "../../win32/glw_win_dx8.h"
#include "../../win32/win_highdynamicrange.h"
#include "../../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBClTailStage;
extern "C" volatile unsigned int g_SPXBRenderListStage;
extern "C" volatile unsigned int g_SPXBRenderListIndex;
extern "C" volatile unsigned int g_SPXBRenderListCount;
extern "C" volatile unsigned int g_SPXBRenderListSurfaceType;
extern "C" volatile unsigned int g_SPXBRenderListSort;
extern "C" volatile unsigned int g_SPXBRenderListShader;
extern "C" volatile unsigned int g_SPXBRenderListEntity;
extern "C" volatile unsigned int g_SPXBRenderListTessVerts;
extern "C" volatile unsigned int g_SPXBRenderListTessIndexes;
extern "C" volatile unsigned int g_SPXBCinRawStage;
extern "C" volatile unsigned int g_SPXBCinRawFrames;
extern "C" volatile unsigned int g_SPXBCinRawSourceSize;
extern "C" volatile unsigned int g_SPXBCinRawUploadSize;
extern "C" volatile unsigned int g_SPXBCinRawFirstPixel;
#if defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBHMScoreQueuedCount;
extern "C" volatile unsigned int g_SPXBHMScoreStretchX;
extern "C" volatile unsigned int g_SPXBHMScoreStretchY;
extern "C" volatile unsigned int g_SPXBHMScoreStretchW;
extern "C" volatile unsigned int g_SPXBHMScoreStretchH;
extern "C" volatile unsigned int g_SPXBHMScoreQueuedShader;
extern "C" volatile unsigned int g_SPXBHMScoreBackendMatches;
extern "C" volatile unsigned int g_SPXBHMScoreBackendColor;
extern "C" volatile unsigned int g_SPXBRenderBackendCommandCount;
extern "C" volatile unsigned int g_SPXBRenderBackendStretchCount;
extern "C" volatile unsigned int g_SPXBRenderBackendTerminalId;
extern "C" volatile unsigned int g_SPXBRenderBackendBytes;
extern "C" volatile unsigned int g_SPXBHMScoreBackendGeometry;
extern "C" volatile unsigned int g_SPXBHMScoreBackendGeomShader;
extern "C" volatile unsigned int g_SPXBHMScoreBackendGeomColor;
extern "C" volatile unsigned int g_SPXBRenderBackendDoneCommands;
extern "C" volatile unsigned int g_SPXBRenderBackendDoneStretches;
extern "C" volatile unsigned int g_SPXBRenderBackendDoneTerminal;
extern "C" volatile unsigned int g_SPXBRenderBackendDoneBytes;
extern "C" volatile unsigned int g_SPXBHMScoreBackendDoneGeometry;
extern "C" volatile unsigned int g_SPXBHMScoreBackendDoneShader;
extern "C" volatile unsigned int g_SPXBHMScoreBackendDoneColor;
extern "C" volatile unsigned int g_SPXBHMScoreBatchPending;
extern "C" volatile unsigned int g_SPXBHMScoreShaderFlags;
extern "C" volatile unsigned int g_SPXBHMScoreImageTex;
extern "C" volatile unsigned int g_SPXBHMScoreWhiteTex;
extern "C" volatile unsigned int g_SPXBHMScoreImageWH;
#endif
#endif

extern qboolean Menus_AnyFullScreenVisible( void );
extern qboolean tr_distortionPrePost;
extern qboolean tr_distortionNegate;
extern void RB_CaptureScreenImage( void );
extern void RB_DistortionFill( void );

STEFX_RETAIL_NAMESPACE_BEGIN

#ifdef _XBOX
static qboolean R_XboxSplitScreenActive( void )
{
	return Cvar_VariableIntegerValue( "stefx_splitScreen" ) ? qtrue : qfalse;
}
#endif

backEndData_t	*backEndData;
backEndState_t	backEnd;

bool tr_stencilled = false;
static void RB_DrawGlowOverlay();
static void RB_BlurGlowTexture();

// Whether we are currently rendering only glowing objects or not.
bool g_bRenderGlowingObjects = false;

// Whether the current hardware supports dynamic glows/flares.
bool g_bDynamicGlowSupported = false;

extern void R_RotateForViewer(void);
extern void R_SetupFrustum(void);

static const float s_flipMatrix[16] = {
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

#ifndef DEDICATED

/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		Com_Printf (S_COLOR_YELLOW  "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
#ifndef _XBOX
		image->frameUsed = tr.frameCount;
#endif
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

#ifdef _XBOX
void GL_InvalidateCurrentTexture( void )
{
	if ( glState.currenttmu >= 0 && glState.currenttmu < 2 )
	{
		glState.currenttextures[glState.currenttmu] = -1;
	}
}

void GL_InvalidateTextureUnit( int unit )
{
	if ( unit >= 0 && unit < 2 )
	{
		glState.currenttextures[unit] = -1;
	}
}
#endif

//bind 3D texture -rww
void GL_Bind3D( image_t *image )
{
	int texnum;

	if ( !image ) {
		Com_Printf (S_COLOR_YELLOW  "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
#ifndef _XBOX
		image->frameUsed = tr.frameCount;
#endif
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_3D, texnum);
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" );
	}
	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" );
	}
	else if ( unit == 2 )
	{
		qglActiveTextureARB( GL_TEXTURE2_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE2_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE2_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE2_ARB )\n" );
	}
	else if ( unit == 3 )
	{
		qglActiveTextureARB( GL_TEXTURE3_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE3_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE3_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE3_ARB )\n" );
	}
	else {
		Com_Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}
	glState.faceCulling = cullType;
	if (backEnd.projection2D){	//don't care, we're in 2d when it's always disabled
		return;	
	}

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
#ifdef _XBOX
	case GL_NONE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_NONE );
		break;
#endif
	default:
		Com_Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				Com_Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				Com_Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		case GLS_ATEST_GE_C0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.75f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = GL_DEPTH_BUFFER_BIT;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	if ( backEnd.viewParms.stefxSplitView )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}
#endif

	// clear relevant buffers
	if ( r_measureOverdraw->integer || r_shadows->integer == 2 || tr_stencilled )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
		tr_stencilled = false;
	}

	if (skyboxportal)
	{
		if ( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL )
		{	// portal scene, clear whatever is necessary
			if (r_fastsky->integer || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL) )
			{	// fastsky: clear color
				// try clearing first with the portal sky fog color, then the world fog color, then finally a default
				clearBits |= GL_COLOR_BUFFER_BIT;
				//rwwFIXMEFIXME: Clear with fog color if there is one
				qglClearColor ( 0.5, 0.5, 0.5, 1.0 );
			} 
		}
	}
	else
	{
		if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && !g_bRenderGlowingObjects )
		{
			clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
			qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
			qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
		}
	}

	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && r_DynamicGlow->integer && !g_bRenderGlowingObjects )
	{
		if (tr.world && tr.world->globalFog != -1)
		{ //this is because of a bug in multiple scenes I think, it needs to clear for the second scene but it doesn't normally.
			const fog_t		*fog = &tr.world->fogs[tr.world->globalFog];

			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(fog->parms.color[0],  fog->parms.color[1], fog->parms.color[2], 1.0f );
		}
	}

	// If this pass is to just render the glowing objects, don't clear the depth buffer since
	// we're sharing it with the main scene (since the main scene has already been rendered). -AReis
	if ( g_bRenderGlowingObjects )
	{
		clearBits &= ~GL_DEPTH_BUFFER_BIT;
	}

	if (clearBits)
	{
		qglClear( clearBits );
	}

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.or.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}

#define	MAC_EVENT_PUMP_MSEC		5

//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoordFloat(vec3_t worldCoord, float *x, float *y)
{
	int	xcenter, ycenter;
	vec3_t	local, transformed;
	vec3_t	vfwd;
	vec3_t	vright;
	vec3_t	vup;
	float xzi;
	float yzi;

	xcenter = glConfig.vidWidth / 2;
	ycenter = glConfig.vidHeight / 2;

#ifdef _XBOX
	if(R_XboxSplitScreenActive())
		ycenter = 240 / 2;
#endif

	//AngleVectors (tr.refdef.viewangles, vfwd, vright, vup);
#ifdef _XBOX
	if(R_XboxSplitScreenActive()) {
		VectorCopy(backEnd.refdef.viewaxis[0], vfwd);
		VectorCopy(backEnd.refdef.viewaxis[1], vright);
		VectorCopy(backEnd.refdef.viewaxis[2], vup);

		VectorSubtract (worldCoord, backEnd.refdef.vieworg, local);
	}
	else {
#endif
	VectorCopy(tr.refdef.viewaxis[0], vfwd);
	VectorCopy(tr.refdef.viewaxis[1], vright);
	VectorCopy(tr.refdef.viewaxis[2], vup);

	VectorSubtract (worldCoord, tr.refdef.vieworg, local);
#ifdef _XBOX
	}
#endif

	transformed[0] = DotProduct(local,vright);
	transformed[1] = DotProduct(local,vup);
	transformed[2] = DotProduct(local,vfwd);		

	// Make sure Z is not negative.
	if(transformed[2] < 0.01)
	{
		return false;
	}

	xzi = xcenter / transformed[2] * (90.0/tr.refdef.fov_x);
	yzi = ycenter / transformed[2] * (90.0/tr.refdef.fov_y);

	*x = xcenter + xzi * transformed[0];
	*y = ycenter - yzi * transformed[1];

	return true;
}

//used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoord( vec3_t worldCoord, int *x, int *y )
{
	float	xF, yF;
	bool retVal = R_WorldCoordToScreenCoordFloat( worldCoord, &xF, &yF );
	*x = (int)xF;
	*y = (int)yF;
	return retVal;
}

/*
==================
RB_RenderDrawSurfList
==================
*/
//number of possible surfs we can postrender.
//note that postrenders lack much of the optimization that the standard sort-render crap does,
//so it's slower.
#define MAX_POST_RENDERS	128

typedef struct
{
	int			fogNum;
	int			entNum;
	int			dlighted;
	int			depthRange;
	drawSurf_t	*drawSurf;
	shader_t	*shader;
	qboolean	eValid;
} postRender_t;

static postRender_t g_postRenders[MAX_POST_RENDERS];
static int g_numPostRenders = 0;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#if defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBFxMerged = 0;
static qboolean R_STEFX_FxMergeReject(int reason, const shader_t *shader, const refEntity_t &e) {
	static unsigned int reasons[4];
	if (++reasons[reason] == 1u) {
		const shaderStage_t &s = shader->stages[0];
		XBLog_WriteCriticalf("STEFX_FX_MERGE_REJECT: reason=%d shader=%s type=%d fx=%x time=%f passes=%d normal=%d tangent=%d rgb=%d alpha=%d state=%x mods=%d cull=%d",
			reason, shader->name, e.reType, e.renderfx, e.shaderTime, shader->numUnfoggedPasses,
			shader->needsNormal, shader->needsTangent, s.rgbGen, s.alphaGen, s.stateBits, s.bundle[0].numTexMods, shader->cullType);
	}
	return qfalse;
}
// STEFX_FX_BATCH_CONTRACT_BEGIN
static qboolean R_STEFX_CanMergeExtendedFx(const shader_t *shader, int entityNum, bool animated) {
	const refEntity_t &e = backEnd.refdef.entities[entityNum].e;
	if (shader->numUnfoggedPasses != 1 || shader->numDeforms || shader->needsNormal ||
		shader->needsTangent || shader->sky || shader->remappedShader || e.shaderTime != 0.0f ||
		(e.renderfx & ~(RF_THIRD_PERSON | RF_FIRST_PERSON))) return R_STEFX_FxMergeReject(0, shader, e);
	const shaderStage_t &s = shader->stages[0];
	if (!s.active || s.ss || s.isBumpMap || s.isEnvironment ||
		s.bundle[1].image || (!animated && s.bundle[0].numTexMods) || s.bundle[0].tcGen != TCGEN_TEXTURE ||
		(s.rgbGen != CGEN_VERTEX && s.rgbGen != CGEN_EXACT_VERTEX &&
		 s.rgbGen != CGEN_IDENTITY && s.rgbGen != CGEN_IDENTITY_LIGHTING && s.rgbGen != CGEN_CONST &&
		 !(animated && s.rgbGen == CGEN_WAVEFORM && s.rgbWave.func != GF_RAND)) ||
		(s.alphaGen != AGEN_VERTEX && s.alphaGen != AGEN_IDENTITY && s.alphaGen != AGEN_SKIP && s.alphaGen != AGEN_CONST &&
		 !(animated && s.alphaGen == AGEN_WAVEFORM && s.alphaWave.func != GF_RAND)) ||
		(s.stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) != (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE) ||
		(s.stateBits & (GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE | GLS_ATEST_BITS))) return R_STEFX_FxMergeReject(1, shader, e);
	// All accepted entities use the same zero time offset. These texture
	// operations and deterministic waves therefore evaluate identically in a
	// combined batch. Entity translation and random wave state stay separate.
	for (int m = 0; m < s.bundle[0].numTexMods; ++m) {
		const texModInfo_t &mod = s.bundle[0].texMods[m];
		switch (mod.type) {
		case TMOD_NONE: case TMOD_TRANSFORM: case TMOD_TURBULENT:
		case TMOD_SCROLL: case TMOD_SCALE: case TMOD_ROTATE: break;
		case TMOD_STRETCH:
			if (mod.wave.func != GF_RAND) break;
		default: return R_STEFX_FxMergeReject(1, shader, e);
		}
	}
	// These generators emit world-space vertices and bake entity color into
	// each vertex. Do not merge immediate-mode beams or stateful legacy bolts.
	switch (e.reType) {
	case RT_SPRITE: case RT_ORIENTED_QUAD: case RT_LINE:
	case RT_TEXTURED_LINE: case RT_ORIENTED_LINE: case RT_TAPERED_LINE:
	case RT_BEZIER: case RT_EF_ORIENTED_SPRITE: case RT_EF_ALPHA_VERT_POLY:
	case RT_EF_LIGHTNING: case RT_EF_ELECTRICITY:
		return shader->cullType == CT_TWO_SIDED ? qtrue : R_STEFX_FxMergeReject(2, shader, e);
	default: return R_STEFX_FxMergeReject(3, shader, e);
	}
}
// STEFX_FX_BATCH_CONTRACT_END
#endif
static qboolean R_STEFX_CanMergeSplitVertexFx( const shader_t *shader, int entityNum )
{
	const trRefEntity_t *entity;

	if ( !backEnd.viewParms.stefxSplitEconomy || !shader ||
		entityNum < 0 || entityNum == TR_WORLDENT || entityNum >= backEnd.refdef.num_entities )
	{
		return qfalse;
	}

#if defined(STEFX_SP_HOSTED_MP)
	static cvar_t *extended;
	if (!extended) extended = Cvar_Get("r_efFxBatch", "1", 0);
	if (extended->integer && R_STEFX_CanMergeExtendedFx(shader, entityNum, extended->integer > 1)) return qtrue;
	// Preserve the existing, proven two-material exception as the fallback.
#endif

	/* These definitions are one-pass, additive, two-sided, vertex-colored
	 * EF sprite shaders.  Keep the exception name-exact and type-exact so lines,
	 * trails, depth hacks, and unrelated translucent entities retain ordering. */
	if ( Q_stricmp( shader->name, "gfx/misc/sunny_flare" ) != 0 &&
		Q_stricmp( shader->name, "gfx/misc/spark" ) != 0 )
	{
		return qfalse;
	}

	entity = &backEnd.refdef.entities[entityNum];
	if ( entity->e.reType != RT_SPRITE
#if defined(STEFX_SP_HOSTED_MP)
		&& entity->e.reType != RT_EF_ORIENTED_SPRITE
#endif
		)
	{
		return qfalse;
	}
	if ( entity->e.renderfx & ( RF_NODEPTH | RF_DEPTHHACK | RF_DISTORTION ) )
	{
		return qfalse;
	}
#if defined(STEFX_SP_HOSTED_MP)
	if ( entity->e.renderfx & RF_STEFX_FORCE_ENT_ALPHA )
	{
		return qfalse;
	}
#endif
	return qtrue;
}
#endif

//get the "average" (ideally center) position of a surface on the tess.
//this is a kind of lame method because I can't think correctly right now.
static inline bool R_AverageTessXYZ(vec3_t dest)
{
	int i = 1;
	float bd = 0.0f;
	float d = 0.0f;
	int b = -1;
	vec3_t v;

	while (i < tess.numVertexes)
	{
		VectorSubtract(tess.xyz[i], tess.xyz[i], v);
		d = VectorLength(v);
		if (b == -1 || d < bd)
		{
			b = i;
			bd = d;
		}
		i++;
	}
	if (b != -1)
	{
		VectorSubtract(tess.xyz[0], tess.xyz[b], v);

		VectorScale(v, 0.5f, dest);
		VectorAdd(dest, tess.xyz[0], dest);

		return true;
	}

	return false;
}

void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	int				depthRange, oldDepthRange;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean		stefxMergeVertexFx;
#endif
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
	float			originalTime;
	trRefEntity_t	*curEnt;
	postRender_t	*pRender;
	bool			didShadowPass = false;
#ifdef _XBOX
	g_SPXBRenderListStage = 0x524C0000; /* 'RL00': list entry */
	g_SPXBRenderListIndex = 0xffffffff;
	g_SPXBRenderListCount = (unsigned int)numDrawSurfs;
	g_SPXBRenderListSurfaceType = 0xffffffff;
	g_SPXBRenderListSort = 0;
	g_SPXBRenderListShader = 0;
	g_SPXBRenderListEntity = 0xffffffff;
	g_SPXBRenderListTessVerts = (unsigned int)tess.numVertexes;
	g_SPXBRenderListTessIndexes = (unsigned int)tess.numIndexes;
#endif
#ifdef __MACOS__
	int				macEventTime;

	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size

	// we don't want to pump the event loop too often and waste time, so
	// we are going to check every shader change
	macEventTime = Sys_Milliseconds()*com_timescale->value + MAC_EVENT_PUMP_MSEC;
#endif

	if (g_bRenderGlowingObjects)
	{ //only shadow on initial passes
		didShadowPass = true;
	}

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
#ifdef _XBOX
	g_SPXBRenderListStage = 0x524C0001; /* 'RL01': begin view */
#endif
	RB_BeginDrawingView ();
#ifdef _XBOX
	g_SPXBRenderListStage = 0x524C0002; /* 'RL02': view ready */
#endif

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = (unsigned int) -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++)
	{
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0010; /* 'RL10': surface entry */
		g_SPXBRenderListIndex = (unsigned int)i;
		g_SPXBRenderListSurfaceType = (unsigned int)*drawSurf->surface;
		g_SPXBRenderListSort = drawSurf->sort;
		g_SPXBRenderListTessVerts = (unsigned int)tess.numVertexes;
		g_SPXBRenderListTessIndexes = (unsigned int)tess.numIndexes;
#endif
		if ( drawSurf->sort == oldSort )
		{
			// fast path, same as previous sort
#ifdef _XBOX
			g_SPXBRenderListStage = 0x524C0011; /* 'RL11': fast surface dispatch */
#endif
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#ifdef _XBOX
			g_SPXBRenderListStage = 0x524C0012; /* 'RL12': fast surface complete */
#endif
			continue;
		}
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0020; /* 'RL20': decompose sort */
#endif
		STEFX_RETAIL_SCOPE R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0021; /* 'RL21': sort decomposed */
		g_SPXBRenderListShader = (unsigned int)shader;
		g_SPXBRenderListEntity = (unsigned int)entityNum;
#endif

#ifndef _XBOX	// GLOWXXX
		// If we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if ( g_bRenderGlowingObjects && !shader->hasGlow )
		{
			shader = oldShader;
			entityNum = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}
#endif
		oldSort = drawSurf->sort;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		stefxMergeVertexFx = R_STEFX_CanMergeSplitVertexFx( shader, entityNum );
#if defined(STEFX_SP_HOSTED_MP)
		// Both sides must satisfy the same contract; a normal sprite must not
		// absorb a previous depth-hacked, transformed or forced-alpha entity.
		stefxMergeVertexFx = stefxMergeVertexFx && !fogNum && !dlighted &&
			shader == oldShader && R_STEFX_CanMergeSplitVertexFx(oldShader, oldEntityNum);
		if (stefxMergeVertexFx && entityNum != oldEntityNum) ++g_SPXBFxMerged;
#endif
#endif

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (entityNum != TR_WORLDENT &&
			g_numPostRenders < MAX_POST_RENDERS)
		{
			if ( ( backEnd.refdef.entities[entityNum].e.renderfx & RF_DISTORTION )
			#if defined(STEFX_SP_HOSTED_MP)
				|| ( backEnd.refdef.entities[entityNum].e.renderfx & RF_STEFX_FORCE_ENT_ALPHA )
			#endif
				)
			{ //must render last
				curEnt = &backEnd.refdef.entities[entityNum];
				pRender = &g_postRenders[g_numPostRenders];

				g_numPostRenders++;

				depthRange = 0;
				//figure this stuff out now and store it
				if ( curEnt->e.renderfx & RF_NODEPTH )
				{
					depthRange = 2;
				}
				else if ( curEnt->e.renderfx & RF_DEPTHHACK )
				{
					depthRange = 1;
				}
				pRender->depthRange = depthRange;

				//It is not necessary to update the old* values because
				//we are not updating now with the current values.
				depthRange = oldDepthRange;

				//store off the ent num
				pRender->entNum = entityNum;

				//remember the other values necessary for rendering this surf
				pRender->drawSurf = drawSurf;
				pRender->dlighted = dlighted;
				pRender->fogNum = fogNum;
				pRender->shader = shader;

				/*
				if (shader == tr.distortionShader)
				{
					pRender->eValid = qfalse;
				}
				else
				*/
				{
					pRender->eValid = qtrue;
				}

				//assure the info is back to the last set state
				shader = oldShader;
				entityNum = oldEntityNum;
				fogNum = oldFogNum;
				dlighted = oldDlighted;

				oldSort = -20; //invalidate this thing, cause we may want to postrender more surfs of the same sort

				//continue without bothering to begin a draw surf
				continue;
			}
		}

		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				&& !stefxMergeVertexFx
#endif
			) )
		{
			if (oldShader != NULL) {
#ifdef __MACOS__	// crutch up the mac's limited buffer queue size
				int		t;

				t = Sys_Milliseconds()*com_timescale->value;
				if ( t > macEventTime ) {
					macEventTime = t + MAC_EVENT_PUMP_MSEC;
					Sys_PumpEvents();
				}
#endif
#ifdef _XBOX
				g_SPXBRenderListStage = 0x524C0030; /* 'RL30': flush batch */
				g_SPXBRenderListTessVerts = (unsigned int)tess.numVertexes;
				g_SPXBRenderListTessIndexes = (unsigned int)tess.numIndexes;
#endif
				RB_EndSurface();
#ifdef _XBOX
				g_SPXBRenderListStage = 0x524C0031; /* 'RL31': batch flushed */
#endif

/*
				if (!didShadowPass && shader && shader->sort > SS_BANNER)
				{
					RB_ShadowFinish();
					didShadowPass = true;
				}
*/
			}
#ifdef _XBOX
			g_SPXBRenderListStage = 0x524C0032; /* 'RL32': begin batch */
#endif
			STEFX_RETAIL_SCOPE RB_BeginSurface( shader, fogNum );
#ifdef _XBOX
			g_SPXBRenderListStage = 0x524C0033; /* 'RL33': batch ready */
#endif
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = 0;

			if ( entityNum != TR_WORLDENT ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				STEFX_RETAIL_SCOPE R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights ) {
					STEFX_RETAIL_SCOPE R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
				}

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = 2;
				}
				else if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = 1;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				STEFX_RETAIL_SCOPE R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			qglLoadMatrixf( backEnd.ori.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				switch ( depthRange ) {
					default:
					case 0:
						qglDepthRange (0, 1);	
						break;

					case 1:
						qglDepthRange (0, .3);	
						break;

					case 2:
						qglDepthRange (0, 0);
						break;
				}

				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0050; /* 'RL50': surface dispatch */
#endif
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0051; /* 'RL51': surface complete */
#endif
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	//assert(entityNum < MAX_GENTITIES);

	if (oldShader != NULL) {
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0060; /* 'RL60': final batch flush */
		g_SPXBRenderListTessVerts = (unsigned int)tess.numVertexes;
		g_SPXBRenderListTessIndexes = (unsigned int)tess.numIndexes;
#endif
		RB_EndSurface();
#ifdef _XBOX
		g_SPXBRenderListStage = 0x524C0061; /* 'RL61': final batch complete */
#endif
	}

#ifdef _CRAZY_ATTRIB_DEBUG
	qglPopAttrib();
	glState.glStateBits = -1;
#endif

	if (tr_stencilled && ::tr_distortionPrePost)
	{ //ok, cap it now
		::RB_CaptureScreenImage();
		::RB_DistortionFill();
	}

	//render distortion surfs (or anything else that needs to be post-rendered)
#ifdef _XBOX
	g_SPXBRenderListStage = 0x524C0070; /* 'RL70': post-render pass */
#endif
	if (g_numPostRenders > 0)
	{
		int lastPostEnt = -1;

		while (g_numPostRenders > 0)
		{
			g_numPostRenders--;
			pRender = &g_postRenders[g_numPostRenders];

			STEFX_RETAIL_SCOPE RB_BeginSurface( pRender->shader, pRender->fogNum );

			backEnd.currentEntity = &backEnd.refdef.entities[pRender->entNum];

			backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
			// we have to reset the shaderTime as well otherwise image animations start
			// from the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// set up the transformation matrix
			STEFX_RETAIL_SCOPE R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

			// set up the dynamic lighting if needed
			if ( backEnd.currentEntity->needDlights )
			{
				STEFX_RETAIL_SCOPE R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			qglLoadMatrixf( backEnd.ori.modelMatrix );

			depthRange = pRender->depthRange;
			switch ( depthRange )
			{
				default:
				case 0:
					qglDepthRange (0, 1);	
					break;

				case 1:
					qglDepthRange (0, .3);	
					break;

				case 2:
					qglDepthRange (0, 0);
					break;
			}

			if (!pRender->eValid)
			{
			}
			else if ((backEnd.refdef.entities[pRender->entNum].e.renderfx & RF_DISTORTION) &&
				lastPostEnt != pRender->entNum)
			{ //do the capture now, we only need to do it once per ent
				int x, y;
				int rad = backEnd.currentEntity->e.radius;

				// Hack - prevent this from using
				if( rad > SCREEN_IMAGE_MAX_HEIGHT )
				{
#ifndef FINAL_BUILD
					Com_Printf( "WARNING: Shrinking screenImage\n" );
#endif
					rad = SCREEN_IMAGE_MAX_HEIGHT;
				}

				//We are going to just bind this, and then the CopyTexImage is going to
				//stomp over this texture num in texture memory.
				STEFX_RETAIL_SCOPE GL_Bind( tr.screenImage );

				if (R_WorldCoordToScreenCoord( backEnd.currentEntity->e.origin, &x, &y ))
				{
					int cX, cY;
					cX = glConfig.vidWidth-x-(rad/2);
					cY = glConfig.vidHeight-y-(rad/2);

#ifdef _XBOX
					cY = 240 - y - (rad / 2);
#endif

					if (cX+rad > glConfig.vidWidth)
					{ //would it go off screen?
						cX = glConfig.vidWidth-rad;
					}
					else if (cX < 0)
					{ //cap it off at 0
						cX = 0;
					}

#ifdef _XBOX
					if(R_XboxSplitScreenActive())
					{
                        if (cY+rad > 240)
						{
							cY = 240 - rad;
						}
						else if (cY < 0)
						{
							cY = 0;
						}
					}
					else {
#endif
					if (cY+rad > glConfig.vidHeight)
					{ //would it go off screen?
						cY = glConfig.vidHeight-rad;
					}
					else if (cY < 0)
					{ //cap it off at 0
						cY = 0;
					}
#ifdef _XBOX 
					}
#endif

					//now copy a portion of the screen to this texture
#ifdef _XBOX
					if(R_XboxSplitScreenActive())
						qglCopyBackBufferToTexEXT(rad, rad, cX, (backEnd.refdef.y + 240) - cY, (cX + rad), (backEnd.refdef.y + 240) - (cY + rad));
					else
                        qglCopyBackBufferToTexEXT(rad, rad, cX, (480 - cY), (cX + rad), (480 - (cY + rad)));
#else
					qglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, cX, cY, rad, rad, 0);
#endif

					lastPostEnt = pRender->entNum;
				}
			}

			rb_surfaceTable[ *pRender->drawSurf->surface ]( pRender->drawSurf->surface );
			RB_EndSurface();
		}
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

#if 0
	RB_DrawSun();
#endif
	if (tr_stencilled && !::tr_distortionPrePost)
	{ //draw in the stencil buffer's cutout
		::RB_DistortionFill();
	}
/*
	if (!didShadowPass)
	{
		// darken down any stencil shadows
		RB_ShadowFinish();
		didShadowPass = true;
	}
*/
#ifdef _XBOX
	if (Cvar_VariableIntegerValue("r_hdreffect"))
	{
//		HDREffect.Render();
	}
#endif

	// add light flares on lights that aren't obscured

	// rww - 9-13-01 [1-26-01-sof2]
//	RB_RenderFlares();

#ifdef __MACOS__
	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size
#endif
#ifdef _XBOX
	g_SPXBRenderListStage = 0x524C00FF; /* 'RLFF': list complete */
#endif
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
#ifdef _XBOX
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;
	const qboolean widescreen = ( glw_state && glw_state->isWidescreen ) ? qtrue : qfalse;

	// set 2D virtual screen size
	// The game-side split HUD already emits coordinates in the full 640x480
	// screen space. Applying the active 3D viewport here scales and offsets the
	// HUD a second time, so retain the original Elite Force 2D viewport.
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	// FakeGL's D3D viewport supplies the screen-space Y inversion. Retain the
	// shipping Xbox orthographic ordering while using Elite Force's full-screen
	// 2D viewport; reversing these bounds mirrors every HUD command vertically.
	#if defined(STEFX_ELITE_FORCE_SP)
	// EF's CG_AdjustFrom640 and CG_FillRect2 submit framebuffer coordinates,
	// including camera bars and fades. JA's 720-unit widescreen projection
	// compresses those coordinates and leaves the last ninth uncovered.
	if (!::Menus_AnyFullScreenVisible() && cls.state == CA_ACTIVE) {
		qglOrtho (0, glConfig.vidWidth, 0, glConfig.vidHeight, 0, 1);
		static int s_efOverlayProjectionLogs = 0;
		if (s_efOverlayProjectionLogs < 2) {
			XBLog_WriteCriticalf("STEFX_OVERLAY_PROJECTION: pixels=%dx%d widescreen=%d",
				glConfig.vidWidth, glConfig.vidHeight, widescreen ? 1 : 0);
			++s_efOverlayProjectionLogs;
		}
	}
	#else
	if(widescreen && !::Menus_AnyFullScreenVisible() && cls.state == CA_ACTIVE)
		qglOrtho (0, 720, 0, 480, 0, 1);
	#endif
	else
		qglOrtho (0, 640, 0, 480, 0, 1);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = Sys_Milliseconds()*com_timescale->value;
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001;
}
#else
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 640, 480, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = Sys_Milliseconds()*com_timescale->value;
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}
#endif


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) 
{
	static byte *s_paddedUpload = NULL;
	static int s_paddedUploadBytes = 0;
	static unsigned int s_rawFrameCount = 0;
	static unsigned int s_rawUploadCount = 0;
	static unsigned int s_rawRetainedCount = 0;
	int uploadCols = 1;
	int uploadRows = 1;
	int uploadBytes;
	int row;
	const byte *uploadData = data;
	qboolean textureSizeChanged;
	float s0, t0, s1, t1;

	g_SPXBCinRawStage = 0x52570000; /* 'RW00': entry */

	if ( !tr.registered || !data || cols <= 0 || rows <= 0 ||
		client < 0 || client >= NUM_SCRATCH_IMAGES ) {
		g_SPXBCinRawStage = 0x5257FFFF; /* 'RWFF': rejected */
		return;
	}

	g_SPXBCinRawSourceSize = ((unsigned int)cols << 16) | ((unsigned int)rows & 0xffff);
	g_SPXBCinRawFirstPixel = *(const unsigned int *)data;
	g_SPXBCinRawStage = 0x52570001; /* 'RW01': accepted */

	while ( uploadCols < cols ) uploadCols <<= 1;
	while ( uploadRows < rows ) uploadRows <<= 1;
	textureSizeChanged = ( uploadCols != tr.scratchImage[client]->width ||
		uploadRows != tr.scratchImage[client]->height ) ? qtrue : qfalse;

	// The EF reel is 512x384.  Xbox textures are power-of-two, so retain the
	// exact decoded rows in a 512x512 upload and crop with texture coordinates.
	// Retained frames reuse the texture and must not rebuild this 1 MiB staging
	// image; that redundant copy was the remaining periodic movie hitch.
	if ( ( uploadCols != cols || uploadRows != rows ) &&
		( dirty || textureSizeChanged ) ) {
		uploadBytes = uploadCols * uploadRows * 4;
		if ( uploadBytes > s_paddedUploadBytes ) {
			if ( s_paddedUpload ) {
				Z_Free( s_paddedUpload );
			}
			s_paddedUpload = (byte *)Z_Malloc( uploadBytes, TAG_BINK, qtrue, 32 );
			s_paddedUploadBytes = uploadBytes;
		}
		if ( !s_paddedUpload ) {
			Com_Error( ERR_DROP, "RE_StretchRaw: failed to allocate %i movie upload bytes", uploadBytes );
			return;
		}
		memset( s_paddedUpload, 0, uploadBytes );
		for ( row = 0; row < rows; ++row ) {
			memcpy( s_paddedUpload + row * uploadCols * 4,
				data + row * cols * 4, cols * 4 );
		}
		uploadData = s_paddedUpload;
	}
	g_SPXBCinRawUploadSize = ((unsigned int)uploadCols << 16) | ((unsigned int)uploadRows & 0xffff);
	g_SPXBCinRawStage = 0x52570002; /* 'RW02': upload buffer ready */

	R_SyncRenderThread();
	g_SPXBCinRawStage = 0x52570003; /* 'RW03': renderer synchronized */

	// Cinematics own stage zero.  Explicitly disable the lightmap stage so a
	// prior 3D draw cannot modulate the movie texture.
	STEFX_RETAIL_SCOPE GL_SelectTexture( 1 );
	qglDisable( GL_TEXTURE_2D );
	STEFX_RETAIL_SCOPE GL_TexEnv( GL_MODULATE );
	STEFX_RETAIL_SCOPE GL_SelectTexture( 0 );
	qglEnable( GL_TEXTURE_2D );
	STEFX_RETAIL_SCOPE GL_TexEnv( GL_REPLACE );
	STEFX_RETAIL_SCOPE GL_Bind( tr.scratchImage[client] );
	g_SPXBCinRawStage = 0x52570004; /* 'RW04': scratch texture bound */

	if ( textureSizeChanged ) {
		tr.scratchImage[client]->width = uploadCols;
		tr.scratchImage[client]->height = uploadRows;
		// FakeGL accepts conventional row-major RGBA source data here, converts it,
		// then swizzles it into the Xbox texture.
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, uploadCols, uploadRows,
			0, GL_RGBA, GL_UNSIGNED_BYTE, uploadData );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
		g_SPXBCinRawStage = 0x52570005; /* 'RW05': initial upload complete */
	} else if ( dirty ) {
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, uploadCols, uploadRows,
			GL_RGBA, GL_UNSIGNED_BYTE, uploadData );
		g_SPXBCinRawStage = 0x52570006; /* 'RW06': update complete */
	} else {
		g_SPXBCinRawStage = 0x52570007; /* 'RW07': retained texture reused */
	}

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}
	STEFX_RETAIL_SCOPE GL_State( GLS_DEPTHTEST_DISABLE );
	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	s0 = 0.5f / uploadCols;
	t0 = 0.5f / uploadRows;
	s1 = ( cols - 0.5f ) / uploadCols;
	t1 = ( rows - 0.5f ) / uploadRows;

	qglBeginEXT( GL_TRIANGLE_STRIP, 4, 0, 0, 4, 0 );
	qglTexCoord2f( s0, t0 ); qglVertex2f( x, y );
	qglTexCoord2f( s1, t0 ); qglVertex2f( x + w, y );
	qglTexCoord2f( s0, t1 ); qglVertex2f( x, y + h );
	qglTexCoord2f( s1, t1 ); qglVertex2f( x + w, y + h );
	qglEnd();
	g_SPXBCinRawStage = 0x52570008; /* 'RW08': quad submitted */

	STEFX_RETAIL_SCOPE GL_SelectTexture( 0 );
	STEFX_RETAIL_SCOPE GL_TexEnv( GL_MODULATE );

	++s_rawFrameCount;
	if ( dirty ) {
		++s_rawUploadCount;
	} else {
		++s_rawRetainedCount;
	}
	g_SPXBCinRawFrames = s_rawFrameCount;
	g_SPXBCinRawStage = 0x52570009; /* 'RW09': complete */
	if ( s_rawFrameCount <= 4 || (s_rawFrameCount % 120) == 0 ) {
		XBLF("STEFX: retail RE_StretchRaw frame=%u uploads=%u retained=%u source=%dx%d upload=%dx%d rect=%d,%d,%d,%d format=RGBA",
			s_rawFrameCount, s_rawUploadCount, s_rawRetainedCount,
			cols, rows, uploadCols, uploadRows, x, y, w, h);
	}
}

void RE_UploadCinematic (int cols, int rows, const byte *data, int client, qboolean dirty) {
/*
	STEFX_RETAIL_SCOPE GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->width = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->height = rows;
#ifdef _XBOX
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5, cols, rows, 0, GL_RGB_SWIZZLE_EXT, GL_UNSIGNED_BYTE, data );
#else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
#endif
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );	
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
#ifdef _XBOX
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGB_SWIZZLE_EXT, GL_UNSIGNED_BYTE, data );
#else
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
#endif
		}
	}
*/
}

const void *RB_Scissor( const void *data )
{
	const scissorCommand_t *cmd = (const scissorCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	if ( cmd->x >= 0 ) {
		qglScissor( cmd->x, glConfig.vidHeight - cmd->y - cmd->h, cmd->w, cmd->h );
	} else {
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}

	return (const void *)(cmd + 1);
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	qboolean stefxScoreCommand = qfalse;
#endif

	cmd = (const stretchPicCommand_t *)data;

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	++g_SPXBRenderBackendStretchCount;
	if ( cmd->x >= 12.5f && cmd->x <= 13.5f &&
		cmd->y >= 104.5f && cmd->y <= 105.5f &&
		cmd->w >= 589.5f && cmd->w <= 590.5f &&
		cmd->h >= 23.5f && cmd->h <= 24.5f ) {
		image_t *scoreImage = NULL;
		stefxScoreCommand = qtrue;
		++g_SPXBHMScoreBackendGeometry;
		g_SPXBHMScoreBackendGeomShader = (unsigned int)cmd->shader;
		g_SPXBHMScoreBackendGeomColor = *(unsigned int *)backEnd.color2D;
		g_SPXBHMScoreShaderFlags = cmd->shader ? 1u : 0u;
		if ( cmd->shader ) {
			if ( cmd->shader->defaultShader ) g_SPXBHMScoreShaderFlags |= 2u;
			if ( cmd->shader->explicitlyDefined ) g_SPXBHMScoreShaderFlags |= 4u;
			if ( cmd->shader->stages && cmd->shader->numUnfoggedPasses > 0 ) {
				g_SPXBHMScoreShaderFlags |= 8u;
				scoreImage = &cmd->shader->stages[0].bundle[0].image[0];
			}
		}
		if ( scoreImage ) {
			g_SPXBHMScoreShaderFlags |= 16u;
			if ( scoreImage == tr.whiteImage ) g_SPXBHMScoreShaderFlags |= 32u;
			g_SPXBHMScoreImageTex = (unsigned int)scoreImage->texnum;
			g_SPXBHMScoreImageWH = ((unsigned int)scoreImage->width & 0xffffu) |
				((unsigned int)scoreImage->height << 16);
		} else {
			g_SPXBHMScoreImageTex = 0;
			g_SPXBHMScoreImageWH = 0;
		}
		g_SPXBHMScoreWhiteTex = tr.whiteImage ? (unsigned int)tr.whiteImage->texnum : 0;
	}
	if ( g_SPXBHMScoreQueuedCount == 1u &&
		(unsigned int)cmd->shader == g_SPXBHMScoreQueuedShader ) {
		union { float f; unsigned int u; } bits;
		bits.f = cmd->x;
		if ( bits.u == g_SPXBHMScoreStretchX ) {
			bits.f = cmd->y;
			if ( bits.u == g_SPXBHMScoreStretchY ) {
				bits.f = cmd->w;
				if ( bits.u == g_SPXBHMScoreStretchW ) {
					bits.f = cmd->h;
					if ( bits.u == g_SPXBHMScoreStretchH ) {
						++g_SPXBHMScoreBackendMatches;
						g_SPXBHMScoreBackendColor = *(unsigned int *)backEnd.color2D;
					}
				}
			}
		}
	}
#endif

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		STEFX_RETAIL_SCOPE RB_BeginSurface( shader, 0 );
	}

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( stefxScoreCommand ) {
		g_SPXBHMScoreBatchPending = 1u;
	}
#endif

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawRotatePic
=============
*/
const void *RB_RotatePic ( const void *data ) 
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;
	image = &shader->stages[0].bundle[0].image[0];

	if ( image ) {
		if ( !backEnd.projection2D ) {
			RB_SetGL2D();
		}

		qglColor4ubv( backEnd.color2D );
		qglPushMatrix();

		qglTranslatef(cmd->x+cmd->w,cmd->y,0);
		qglRotatef(cmd->a, 0.0, 0.0, 1.0);
		
		STEFX_RETAIL_SCOPE GL_Bind( image );
#ifdef _XBOX
		qglBeginEXT (GL_QUADS, 4, 0, 0, 4, 0);
#else
		qglBegin (GL_QUADS);
#endif
		qglTexCoord2f( cmd->s1, cmd->t1);
		qglVertex2f( -cmd->w, 0 );
		qglTexCoord2f( cmd->s2, cmd->t1 );
		qglVertex2f( 0, 0 );
		qglTexCoord2f( cmd->s2, cmd->t2 );
		qglVertex2f( 0, cmd->h );
		qglTexCoord2f( cmd->s1, cmd->t2 );
		qglVertex2f( -cmd->w, cmd->h );
		qglEnd();
		
		qglPopMatrix();
	}

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawRotatePic2
=============
*/
const void *RB_RotatePic2 ( const void *data ) 
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;

	if ( shader->numUnfoggedPasses )
	{
		image = &shader->stages[0].bundle[0].image[0];

		if ( image )
	{
			if ( !backEnd.projection2D )
			{
				RB_SetGL2D();
			}

			// Get our current blend mode, etc.
			GL_State( shader->stages[0].stateBits );

			qglColor4ubv( backEnd.color2D );
			qglPushMatrix();

			// rotation point is going to be around the center of the passed in coordinates
			qglTranslatef( cmd->x, cmd->y, 0 );
			qglRotatef( cmd->a, 0.0, 0.0, 1.0 );
		
			STEFX_RETAIL_SCOPE GL_Bind( image );
#ifdef _XBOX
			qglBeginEXT( GL_QUADS, 4, 0, 0, 4, 0);
#else
			qglBegin( GL_QUADS );
#endif
				qglTexCoord2f( cmd->s1, cmd->t1);
				qglVertex2f( -cmd->w * 0.5f, -cmd->h * 0.5f );

				qglTexCoord2f( cmd->s2, cmd->t1 );
				qglVertex2f( cmd->w * 0.5f, -cmd->h * 0.5f );

				qglTexCoord2f( cmd->s2, cmd->t2 );
				qglVertex2f( cmd->w * 0.5f, cmd->h * 0.5f );

				qglTexCoord2f( cmd->s1, cmd->t2 );
				qglVertex2f( -cmd->w * 0.5f, cmd->h * 0.5f );
			qglEnd();
		
			qglPopMatrix();

			// Hmmm, this is not too cool
			GL_State( GLS_DEPTHTEST_DISABLE |
				  GLS_SRCBLEND_SRC_ALPHA |
				  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
		}
	}

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

#ifdef _XBOX
	g_SPXBRenderListStage = 0x44530000; /* 'DS00': draw-surfs entry */
#endif

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
#ifdef _XBOX
		g_SPXBRenderListStage = 0x44530001; /* 'DS01': flush pending 2D */
		g_SPXBRenderListTessVerts = (unsigned int)tess.numVertexes;
		g_SPXBRenderListTessIndexes = (unsigned int)tess.numIndexes;
#endif
		RB_EndSurface();
#ifdef _XBOX
		g_SPXBRenderListStage = 0x44530002; /* 'DS02': pending 2D flushed */
#endif
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

#ifdef _XBOX
	g_SPXBRenderListStage = 0x44530003; /* 'DS03': dispatch list */
	g_SPXBRenderListCount = (unsigned int)cmd->numDrawSurfs;
#endif
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
#ifdef _XBOX
	g_SPXBRenderListStage = 0x44530004; /* 'DS04': draw-surfs complete */
#endif

	// Dynamic Glow/Flares:
	/*
		The basic idea is to render the glowing parts of the scene to an offscreen buffer, then take
		that buffer and blur it. After it is sufficiently blurred, re-apply that image back to
		the normal screen using a additive blending. To blur the scene I use a vertex program to supply
		four texture coordinate offsets that allow 'peeking' into adjacent pixels. In the register
		combiner (pixel shader), I combine the adjacent pixels using a weighting factor. - Aurelio
	*/

	// Render dynamic glowing/flaring objects.
#ifndef _XBOX	// GLOWXXX
	if ( !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && g_bDynamicGlowSupported && r_DynamicGlow->integer )
	{
		// Copy the normal scene to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage ); 
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );    

		// Just clear colors, but leave the depth buffer intact so we can 'share' it.
		qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		qglClear( GL_COLOR_BUFFER_BIT ); 

		// Render the glowing objects.
		g_bRenderGlowingObjects = true;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );  
		g_bRenderGlowingObjects = false;

		qglFinish();

		// Copy the glow scene to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.screenGlow ); 
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
		
		// Resize the viewport to the blur texture size.
		const int oldViewWidth = backEnd.viewParms.viewportWidth;
		const int oldViewHeight = backEnd.viewParms.viewportHeight;
		backEnd.viewParms.viewportWidth = r_DynamicGlowWidth->integer;
		backEnd.viewParms.viewportHeight = r_DynamicGlowHeight->integer;
		SetViewportAndScissor();

		// Blur the scene.
		RB_BlurGlowTexture();

		// Copy the finished glow scene back to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
		
		// Set the viewport back to normal.
		backEnd.viewParms.viewportWidth = oldViewWidth;
		backEnd.viewParms.viewportHeight = oldViewHeight;
		SetViewportAndScissor();
		qglClear( GL_COLOR_BUFFER_BIT ); 

		// Draw the glow additively over the screen.
		RB_DrawGlowOverlay(); 
	}
#endif	// _XBOX

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	qglDrawBuffer( cmd->buffer );

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	image_t	*image;
	float	x, y, w, h;
//	int		start, end;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

//	start = Sys_Milliseconds()*com_timescale->value;


	int i=0;
	   				 R_Images_StartIteration();
	while ( (image = R_Images_GetNextIteration()) != NULL)
	{
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->width / 512.0;
			h *= image->height / 512.0;
		}

		STEFX_RETAIL_SCOPE GL_Bind( image );
#ifdef _XBOX
		qglBeginEXT (GL_QUADS, 4, 0, 0, 4, 0);
#else
		qglBegin (GL_QUADS);
#endif
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
		i++;
	}

	qglFinish();

//	end = Sys_Milliseconds()*com_timescale->value;
//	Com_Printf ("%i msec to draw all images\n", end - start );
}


/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	const int xboxPerfSample = g_SPXBPerfSampleActive ? 1 : 0;
	int xboxFinishStart = 0;
#endif

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
#ifndef _XBOX
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = (unsigned char *)Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		Hunk_FreeTempMemory( stencilReadback );
	}
#endif

    if ( !glState.finishCalled ) {
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxPerfSample) {
			xboxFinishStart = Sys_Milliseconds();
		}
#endif
        qglFinish();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxPerfSample) {
			g_SPXBPerfFinishMsec += (unsigned int)(Sys_Milliseconds() - xboxFinishStart);
		}
#endif
	}

    GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

    GLimp_EndFrame();

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

const void	*RB_WorldEffects( const void *data ) 
{
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;
#ifdef _XBOX
	g_SPXBClTailStage = 0x57453030; /* 'WE00' */
#endif

	// Always flush the tess buffer
	if ( tess.shader && tess.numIndexes ) 
	{
		#ifdef _XBOX
		g_SPXBClTailStage = 0x57453031; /* 'WE01' */
		#endif
		RB_EndSurface();
		#ifdef _XBOX
		g_SPXBClTailStage = 0x57453032; /* 'WE02' */
		#endif
	}
	#ifdef _XBOX
	g_SPXBClTailStage = 0x57453033; /* 'WE03' */
	#endif
	RB_RenderWorldEffects();
	#ifdef _XBOX
	g_SPXBClTailStage = 0x57453034; /* 'WE04' */
	#endif

	if(tess.shader)
	{
		#ifdef _XBOX
		g_SPXBClTailStage = 0x57453035; /* 'WE05' */
		#endif
		STEFX_RETAIL_SCOPE RB_BeginSurface( tess.shader, tess.fogNum );
		#ifdef _XBOX
		g_SPXBClTailStage = 0x57453036; /* 'WE06' */
		#endif
	}

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
====================
*/
extern const void *R_DrawWireframeAutomap(const void *data); //tr_world.cpp
void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;
#ifdef _XBOX
	g_SPXBClTailStage = 0x42453030; /* 'BE00' */
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	const unsigned char *commandBase = (const unsigned char *)data;
	g_SPXBRenderBackendCommandCount = 0;
	g_SPXBRenderBackendStretchCount = 0;
	g_SPXBRenderBackendTerminalId = 0xffffffffu;
	g_SPXBRenderBackendBytes = 0;
	g_SPXBHMScoreBackendGeometry = 0;
	g_SPXBHMScoreBackendGeomShader = 0;
	g_SPXBHMScoreBackendGeomColor = 0;
	g_SPXBHMScoreBatchPending = 0;
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	const int xboxPerfSample = g_SPXBPerfSampleActive ? 1 : 0;
	int xboxCommandStart;
	int xboxCommandId;
#endif

	t1 = Sys_Milliseconds()*com_timescale->value;

	while ( 1 ) {
#ifdef _XBOX
		g_SPXBClTailStage = 0x42450000 | (*(const unsigned int *)data & 0xffff); /* 'BE'+command */
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
		++g_SPXBRenderBackendCommandCount;
		g_SPXBRenderBackendBytes = (unsigned int)((const unsigned char *)data - commandBase);
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxPerfSample) {
			xboxCommandStart = Sys_Milliseconds();
			xboxCommandId = *(const int *)data;
		}
#endif
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_SCISSOR:
			data = RB_Scissor( data );
			break;
		case RC_ROTATE_PIC:
			data = RB_RotatePic( data );
			break;
		case RC_ROTATE_PIC2:
			data = RB_RotatePic2( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if (xboxPerfSample) {
				g_SPXBPerfBackendDrawSurfsMsec += (unsigned int)(Sys_Milliseconds() - xboxCommandStart);
			}
#endif
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if (xboxPerfSample) {
				g_SPXBPerfBackendSwapMsec += (unsigned int)(Sys_Milliseconds() - xboxCommandStart);
			}
#endif
			break;
		case RC_WORLD_EFFECTS:
			data = RB_WorldEffects( data );
			break;
		case RC_END_OF_LIST:
		default:
#ifdef _XBOX
			g_SPXBClTailStage = 0x42454646; /* 'BEFF' */
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
			g_SPXBRenderBackendTerminalId = *(const unsigned int *)data;
			g_SPXBRenderBackendDoneCommands = g_SPXBRenderBackendCommandCount;
			g_SPXBRenderBackendDoneStretches = g_SPXBRenderBackendStretchCount;
			g_SPXBRenderBackendDoneTerminal = g_SPXBRenderBackendTerminalId;
			g_SPXBRenderBackendDoneBytes = g_SPXBRenderBackendBytes;
			g_SPXBHMScoreBackendDoneGeometry = g_SPXBHMScoreBackendGeometry;
			g_SPXBHMScoreBackendDoneShader = g_SPXBHMScoreBackendGeomShader;
			g_SPXBHMScoreBackendDoneColor = g_SPXBHMScoreBackendGeomColor;
#endif
			// stop rendering on this thread
			t2 = Sys_Milliseconds()*com_timescale->value;
			backEnd.pc.msec = t2 - t1;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if (xboxPerfSample) {
				g_SPXBPerfBackendSurfaces = (unsigned int)backEnd.pc.c_surfaces;
				g_SPXBPerfBackendVertexes = (unsigned int)backEnd.pc.c_vertexes;
				g_SPXBPerfBackendIndexes = (unsigned int)backEnd.pc.c_indexes;
				g_SPXBPerfBackendTotalIndexes = (unsigned int)backEnd.pc.c_totalIndexes;
				g_SPXBPerfBackendBatches = (unsigned int)backEnd.pc.c_shaders;
			}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
			R_STEFX_ReportShaderCosts();
			extern void R_STEFX_ReportMdrSkinCost(void);
			R_STEFX_ReportMdrSkinCost();
#endif
			return;
		}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (xboxPerfSample &&
			xboxCommandId != RC_DRAW_SURFS &&
			xboxCommandId != RC_SWAP_BUFFERS) {
			g_SPXBPerfBackendOtherMsec += (unsigned int)(Sys_Milliseconds() - xboxCommandStart);
		}
#endif
	}

}

#ifndef _XBOX	// GLOWXXX

// What Pixel Shader type is currently active (regcoms or fragment programs).
GLuint g_uiCurrentPixelShaderType = 0x0;

// Begin using a Pixel Shader.
void BeginPixelShader( GLuint uiType, GLuint uiID )
{
	switch ( uiType )
	{
		// Using Register Combiners, so call the Display List that stores it.
		case GL_REGISTER_COMBINERS_NV:
		{
			// Just in case...
			if ( !qglCombinerParameterfvNV )
				return;

			// Call the list with the regcom in it.
			qglEnable( GL_REGISTER_COMBINERS_NV );
			qglCallList( uiID );

			g_uiCurrentPixelShaderType = GL_REGISTER_COMBINERS_NV;
		}
		return;

		// Using Fragment Programs, so call the program.
		case GL_FRAGMENT_PROGRAM_ARB:
		{
			// Just in case...
			if ( !qglGenProgramsARB )
				return;

			qglEnable( GL_FRAGMENT_PROGRAM_ARB );
			qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, uiID );

			g_uiCurrentPixelShaderType = GL_FRAGMENT_PROGRAM_ARB;
		}
		return;
	}
}

// Stop using a Pixel Shader and return states to normal.
void EndPixelShader()
{
	if ( g_uiCurrentPixelShaderType == 0x0 )
		return;

	qglDisable( g_uiCurrentPixelShaderType );
}

// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).
extern bool g_bTextureRectangleHack;

static inline void RB_BlurGlowTexture()
{
	qglDisable (GL_CLIP_PLANE0);
	STEFX_RETAIL_SCOPE GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST );

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	/////////////////////////////////////////////////////////
	// Setup vertex and pixel programs.
	/////////////////////////////////////////////////////////

	// NOTE: The 0.25 is because we're blending 4 textures (so = 1.0) and we want a relatively normalized pixel
	// intensity distribution, but this won't happen anyways if intensity is higher than 1.0.
	float fBlurDistribution = r_DynamicGlowIntensity->value * 0.25f;
	float fBlurWeight[4] = { fBlurDistribution, fBlurDistribution, fBlurDistribution, 1.0f };

	// Enable and set the Vertex Program.
	qglEnable( GL_VERTEX_PROGRAM_ARB );
	qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, tr.glowVShader );

	// Apply Pixel Shaders.
	if ( qglCombinerParameterfvNV )
	{
		BeginPixelShader( GL_REGISTER_COMBINERS_NV, tr.glowPShader );

		// Pass the blur weight to the regcom.
		qglCombinerParameterfvNV( GL_CONSTANT_COLOR0_NV, (float*)&fBlurWeight );
	}
	else if ( qglProgramEnvParameter4fARB )
	{
		BeginPixelShader( GL_FRAGMENT_PROGRAM_ARB, tr.glowPShader );

		// Pass the blur weight to the Fragment Program.
		qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, fBlurWeight[0], fBlurWeight[1], fBlurWeight[2], fBlurWeight[3] );
	}

	/////////////////////////////////////////////////////////
	// Set the blur texture to the 4 texture stages.
	/////////////////////////////////////////////////////////

	// How much to offset each texel by.
	float fTexelWidthOffset = 0.1f, fTexelHeightOffset = 0.1f;

	GLuint uiTex = tr.screenGlow;  

	qglActiveTextureARB( GL_TEXTURE3_ARB );  
	qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
	
	qglActiveTextureARB( GL_TEXTURE2_ARB ); 
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_2D );  
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex ); 
	
	/////////////////////////////////////////////////////////
	// Draw the blur passes (each pass blurs it more, increasing the blur radius ).
	/////////////////////////////////////////////////////////
	
	//int iTexWidth = backEnd.viewParms.viewportWidth, iTexHeight = backEnd.viewParms.viewportHeight;
	int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight; 
	
	for ( int iNumBlurPasses = 0; iNumBlurPasses < r_DynamicGlowPasses->integer; iNumBlurPasses++ )       
	{
		// Load the Texel Offsets into the Vertex Program.
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, -fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 1, -fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 2, fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 3, fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );

		// After first pass put the tex coords to the viewport size.
		if ( iNumBlurPasses == 1 )
		{
			// OK, very weird, but dependent on which texture rectangle extension we're using, the
			// texture either needs to be always texure correct or view correct...
			if ( !g_bTextureRectangleHack ) 
			{
				iTexWidth = backEnd.viewParms.viewportWidth;
				iTexHeight = backEnd.viewParms.viewportHeight;
			}

			uiTex = tr.blurImage;
			qglActiveTextureARB( GL_TEXTURE3_ARB );  
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
			qglActiveTextureARB( GL_TEXTURE2_ARB ); 
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );			
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
			qglActiveTextureARB(GL_TEXTURE0_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex ); 

			// Copy the current image over.
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );     
			qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		}

		// Draw the fullscreen quad.
		qglBegin( GL_QUADS ); 
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, iTexHeight );  
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, 0 ); 
			qglVertex2f( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, iTexHeight );
			qglVertex2f( backEnd.viewParms.viewportWidth, 0 ); 
		qglEnd();

		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );       
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );    

		// Increase the texel offsets.
		// NOTE: This is possibly the most important input to the effect. Even by using an exponential function I've been able to
		// make it look better (at a much higher cost of course). This is cheap though and still looks pretty great. In the future 
		// I might want to use an actual gaussian equation to correctly calculate the pixel coefficients and attenuates, texel
		// offsets, gaussian amplitude and radius...
		fTexelWidthOffset += r_DynamicGlowDelta->value;
		fTexelHeightOffset += r_DynamicGlowDelta->value;
	}

	// Disable multi-texturing.
	qglActiveTextureARB( GL_TEXTURE3_ARB );   
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB( GL_TEXTURE2_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );
	qglEnable( GL_TEXTURE_2D );

	qglDisable( GL_VERTEX_PROGRAM_ARB );
	EndPixelShader();
	
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglDisable( GL_BLEND );
	qglEnable( GL_DEPTH_TEST );

	glState.currenttmu = 0;	//this matches the last one we activated
}

// Draw the glow blur over the screen additively.
static inline void RB_DrawGlowOverlay()
{
	qglDisable (GL_CLIP_PLANE0);
	STEFX_RETAIL_SCOPE GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST ); 

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );

	// For debug purposes.
	if ( r_DynamicGlow->integer != 2 )
	{
		// Render the normal scene texture.
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage ); 
		qglBegin(GL_QUADS);    
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglTexCoord2f( 0, glConfig.vidHeight ); 
			qglVertex2f( 0, 0 );

			qglTexCoord2f( 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, 0 );
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 );
		qglEnd();
	}

	// One and Inverse Src Color give a very soft addition, while one one is a bit stronger. With one one we can
	// use additive blending through multitexture though.
	if ( r_DynamicGlowSoft->integer )
	{
		qglBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
	}
	else
	{
		qglBlendFunc( GL_ONE, GL_ONE );
	}
	qglEnable( GL_BLEND );  

	// Now additively render the glow texture.
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );     
	qglBegin(GL_QUADS);    
		qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );  
		qglTexCoord2f( 0, r_DynamicGlowHeight->integer ); 
		qglVertex2f( 0, 0 );

		qglTexCoord2f( 0, 0 );
		qglVertex2f( 0, glConfig.vidHeight );

		qglTexCoord2f( r_DynamicGlowWidth->integer, 0 );
		qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

		qglTexCoord2f( r_DynamicGlowWidth->integer, r_DynamicGlowHeight->integer );
		qglVertex2f( glConfig.vidWidth, 0 );
	qglEnd();

	qglDisable( GL_TEXTURE_RECTANGLE_EXT );
	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR );
	qglDisable( GL_BLEND );

	// NOTE: Multi-texture wasn't that much faster (we're obviously not bottlenecked by transform pipeline),
	// and besides, soft glow looks better anyways.
/*	else
	{
		int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;
		if ( GL_TEXTURE_RECTANGLE_EXT == GL_TEXTURE_RECTANGLE_NV ) 
		{
			iTexWidth = r_DynamicGlowWidth->integer;
			iTexHeight = r_DynamicGlowHeight->integer;
		}

		qglActiveTextureARB( GL_TEXTURE1_ARB ); 
		qglDisable( GL_TEXTURE_2D ); 
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.screenGlow ); 
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );  

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

		qglBegin(GL_QUADS);    
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, iTexHeight );  
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, glConfig.vidHeight );  
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, 0 ); 
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, 0 ); 
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 ); 
		qglEnd();

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
	}*/

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
}
#endif	//XBOX

STEFX_RETAIL_NAMESPACE_END

#endif //!DEDICATED
