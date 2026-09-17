// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"
#include "fx_public.h"
#define	FOCUS_DISTANCE	512
#ifdef _XBOX
#include "../../code/win32/xb_log.h"
#define CG_XBOX_ACTIVE_LOG(msg) do { if (xboxDrawLog) XBLog_Write(msg); } while (0)
extern void CM_SnapPVS(vec3_t origin, byte *buffer);
extern qboolean player_locked;
extern void CGCam_Disable( void );
extern void RE_STEFX_SplitScreen_SetP2Refdef( const refdef_t *refdef, qboolean valid );
extern void RE_STEFX_SplitScreen_SetP2PvsOrigin( const vec3_t origin );
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBClTailStage;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBCoopCgamePhasesV2[51];
extern void CG_STEFX_BeginPlayerProfile(void);
extern void CG_STEFX_EndPlayerProfile(void);
extern "C" volatile unsigned int g_SPXBMainLoopCount;
#define STEFX_COOP_CGAME_DETAIL 1
#endif
extern "C" volatile unsigned int g_SPXBSplitP2Ent;
extern "C" volatile unsigned int g_SPXBSplitP2TraceFrac1000;
extern "C" volatile unsigned int g_SPXBSplitP2ViewX;
extern "C" volatile unsigned int g_SPXBSplitP2ViewY;
extern "C" volatile unsigned int g_SPXBSplitP2ViewZ;
extern "C" volatile unsigned int g_SPXBSplitP2PsX;
extern "C" volatile unsigned int g_SPXBSplitP2PsY;
extern "C" volatile unsigned int g_SPXBSplitP2PsZ;
extern "C" volatile unsigned int g_SPXBSplitP2CurX;
extern "C" volatile unsigned int g_SPXBSplitP2CurY;
extern "C" volatile unsigned int g_SPXBSplitP2CurZ;
extern "C" volatile unsigned int g_SPXBSplitP2AnglesPitch;
extern "C" volatile unsigned int g_SPXBSplitP2AnglesYaw;
extern "C" volatile unsigned int g_SPXBSplitP2RefdefValid;
extern "C" volatile unsigned int g_SPXBSplitCameraMode;
extern "C" volatile unsigned int g_SPXBSplitP1TraceFrac1000;
extern "C" volatile unsigned int g_SPXBSplitP1LocalX1000;
extern "C" volatile unsigned int g_SPXBSplitP1LocalY1000;
extern "C" volatile unsigned int g_SPXBSplitP1LocalZ1000;
extern "C" volatile unsigned int g_SPXBSplitP2LocalX1000;
extern "C" volatile unsigned int g_SPXBSplitP2LocalY1000;
extern "C" volatile unsigned int g_SPXBSplitP2LocalZ1000;
extern "C" volatile unsigned int g_SPXBSplitLocalDiffX1000;
extern "C" volatile unsigned int g_SPXBSplitLocalDiffY1000;
extern "C" volatile unsigned int g_SPXBSplitLocalDiffZ1000;

static float s_stefxLastP1ThirdPersonTraceFraction = -1.0f;

qboolean STEFX_XboxSuppressPlayerPresentation( void );

static qboolean CG_STEFX_SplitScreenActive( void )
{
	static int s_activeLogBudget = 64;
	if ( s_activeLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_VIEW state time=%d split=%d players=%d p2=%d snap=%d client=%d camera=%d locked=%d",
			cg.time,
			cg_stefxSplitScreen.integer,
			cg_stefxSplitScreenPlayers.integer,
			cg_stefxSplitScreenP2Entity.integer,
			cg.snap ? 1 : 0,
			cg.snap ? cg.snap->ps.clientNum : -1,
			in_camera ? 1 : 0,
			player_locked ? 1 : 0 );
		--s_activeLogBudget;
	}
	return (qboolean)( cg_stefxSplitScreen.integer && cg_stefxSplitScreenPlayers.integer >= 2 );
}

static void CG_STEFX_CalcEyeOrigin( const playerState_t *ps, vec3_t eye )
{
	VectorCopy( ps->origin, eye );
	eye[2] += ps->viewheight;
}

static void CG_STEFX_CalcViewDeltaInAxis( const vec3_t eye, const vec3_t viewOrg, const vec3_t axis[3], vec3_t worldDelta, vec3_t localDelta )
{
	VectorSubtract( viewOrg, eye, worldDelta );
	localDelta[0] = DotProduct( worldDelta, axis[0] );
	localDelta[1] = DotProduct( worldDelta, axis[1] );
	localDelta[2] = DotProduct( worldDelta, axis[2] );
}

static void CG_STEFX_AngleSubtractVec( const vec3_t a, const vec3_t b, vec3_t out )
{
	out[PITCH] = AngleSubtract( a[PITCH], b[PITCH] );
	out[YAW] = AngleSubtract( a[YAW], b[YAW] );
	out[ROLL] = AngleSubtract( a[ROLL], b[ROLL] );
}

static unsigned int CG_STEFX_Fixed1000( float value )
{
	return (unsigned int)(int)( value * 1000.0f );
}

static void CG_STEFX_ApplyP1FirstPersonRelativeViewToP2( const gentity_t *p2, refdef_t *p2Refdef, vec3_t outViewAngles )
{
	vec3_t p1Eye;
	vec3_t p1LocalDelta;
	vec3_t p1WorldDelta;
	vec3_t p1AngleDelta;
	vec3_t p2Eye;
	vec3_t p2Axis[3];

	CG_STEFX_CalcEyeOrigin( &cg.predicted_player_state, p1Eye );
	CG_STEFX_CalcViewDeltaInAxis( p1Eye, cg.refdef.vieworg, cg.refdef.viewaxis, p1WorldDelta, p1LocalDelta );
	CG_STEFX_AngleSubtractVec( cg.refdefViewAngles, cg.predicted_player_state.viewangles, p1AngleDelta );

	VectorCopy( p2->client->ps.viewangles, outViewAngles );
	outViewAngles[PITCH] = AngleNormalize180( outViewAngles[PITCH] + p1AngleDelta[PITCH] );
	outViewAngles[YAW] = AngleNormalize360( outViewAngles[YAW] + p1AngleDelta[YAW] );
	outViewAngles[ROLL] = AngleNormalize180( outViewAngles[ROLL] + p1AngleDelta[ROLL] );
	AnglesToAxis( outViewAngles, p2Axis );

	CG_STEFX_CalcEyeOrigin( &p2->client->ps, p2Eye );
	VectorCopy( p2Eye, p2Refdef->vieworg );
	VectorMA( p2Refdef->vieworg, p1LocalDelta[0], p2Axis[0], p2Refdef->vieworg );
	VectorMA( p2Refdef->vieworg, p1LocalDelta[1], p2Axis[1], p2Refdef->vieworg );
	VectorMA( p2Refdef->vieworg, p1LocalDelta[2], p2Axis[2], p2Refdef->vieworg );
}

static void CG_STEFX_CalcThirdPersonViewForPlayer( const playerState_t *ps, const vec3_t origin, const vec3_t inputAngles, int traceEntNum, vec3_t outViewOrg, vec3_t outViewAngles, float *traceFraction )
{
	vec3_t		forward, right, up;
	vec3_t		view;
	vec3_t		eye;
	vec3_t		focusAngles;
	trace_t trace;
	static vec3_t	mins = { -4, -4, -4 };
	static vec3_t	maxs = { 4, 4, 4 };
	vec3_t		focusPoint;
	float		focusDist;
	float		forwardScale, sideScale;

	VectorCopy( origin, eye );
	eye[2] += ps->viewheight;

	VectorCopy( inputAngles, outViewAngles );
	VectorCopy( outViewAngles, focusAngles );

	// if dead, look at killer
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		focusAngles[YAW] = ps->stats[STAT_DEAD_YAW];
		outViewAngles[YAW] = ps->stats[STAT_DEAD_YAW];
	}

	if ( focusAngles[PITCH] > 45 ) {
		focusAngles[PITCH] = 45;		// don't go too far overhead
	}
	AngleVectors( focusAngles, forward, NULL, NULL );

	VectorMA( eye, FOCUS_DISTANCE, forward, focusPoint );

	VectorCopy( eye, view );

	view[2] += 8;

	outViewAngles[PITCH] *= 0.5;

	AngleVectors( outViewAngles, forward, right, up );

	forwardScale = cos( cg_thirdPersonAngle.value / 180 * M_PI );
	sideScale = sin( cg_thirdPersonAngle.value / 180 * M_PI );
	VectorMA( view, -cg_thirdPersonRange.value * forwardScale, forward, view );
	VectorMA( view, -cg_thirdPersonRange.value * sideScale, right, view );

	// trace a ray from the origin to the viewpoint to make sure the view isn't
	// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

	CG_Trace( &trace, eye, mins, maxs, view, traceEntNum, MASK_SOLID );

	if ( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, view );
		view[2] += (1.0 - trace.fraction) * 32;
		// try another trace to this position, because a tunnel may have the ceiling
		// close enogh that this is poking out

		CG_Trace( &trace, eye, mins, maxs, view, traceEntNum, MASK_SOLID );
		VectorCopy( trace.endpos, view );
	}

	if ( traceFraction )
	{
		*traceFraction = trace.fraction;
	}

	VectorCopy( view, outViewOrg );

	// select pitch to look at focus point from vieword
	VectorSubtract( focusPoint, outViewOrg, focusPoint );
	focusDist = sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1 ) {
		focusDist = 1;	// should never happen
	}
	outViewAngles[PITCH] = -180 / M_PI * atan2( focusPoint[2], focusDist );
	outViewAngles[YAW] -= cg_thirdPersonAngle.value;
}

static void CG_STEFX_LogSplitCameraCompare( const gentity_t *p2, const refdef_t *p2Refdef, const vec3_t p2ViewAngles, qboolean thirdPersonMode, float p2TraceFraction )
{
	static int s_logBudget = 160;
	centity_t *p1Cent;
	centity_t *p2Cent;
	vec3_t p1Eye;
	vec3_t p2Eye;
	vec3_t p1WorldDelta;
	vec3_t p2WorldDelta;
	vec3_t p1ViewLocal;
	vec3_t p2ViewLocal;
	vec3_t p1PlayerAxis[3];
	vec3_t p2PlayerAxis[3];
	vec3_t p1PlayerLocal;
	vec3_t p2PlayerLocal;
	vec3_t localDiff;

	if ( !p2 || !p2->client || !p2Refdef )
	{
		return;
	}

	p1Cent = &cg_entities[cg.predicted_player_state.clientNum];
	p2Cent = &cg_entities[p2->s.number];

	CG_STEFX_CalcEyeOrigin( &cg.predicted_player_state, p1Eye );
	CG_STEFX_CalcEyeOrigin( &p2->client->ps, p2Eye );
	CG_STEFX_CalcViewDeltaInAxis( p1Eye, cg.refdef.vieworg, cg.refdef.viewaxis, p1WorldDelta, p1ViewLocal );
	CG_STEFX_CalcViewDeltaInAxis( p2Eye, p2Refdef->vieworg, p2Refdef->viewaxis, p2WorldDelta, p2ViewLocal );

	AngleVectors( cg.predicted_player_state.viewangles, p1PlayerAxis[0], p1PlayerAxis[1], p1PlayerAxis[2] );
	AngleVectors( p2->client->ps.viewangles, p2PlayerAxis[0], p2PlayerAxis[1], p2PlayerAxis[2] );
	p1PlayerLocal[0] = DotProduct( p1WorldDelta, p1PlayerAxis[0] );
	p1PlayerLocal[1] = DotProduct( p1WorldDelta, p1PlayerAxis[1] );
	p1PlayerLocal[2] = DotProduct( p1WorldDelta, p1PlayerAxis[2] );
	p2PlayerLocal[0] = DotProduct( p2WorldDelta, p2PlayerAxis[0] );
	p2PlayerLocal[1] = DotProduct( p2WorldDelta, p2PlayerAxis[1] );
	p2PlayerLocal[2] = DotProduct( p2WorldDelta, p2PlayerAxis[2] );
	VectorSubtract( p2PlayerLocal, p1PlayerLocal, localDiff );

	g_SPXBSplitCameraMode = thirdPersonMode ? 1u : 0u;
	g_SPXBSplitP1TraceFrac1000 = ( s_stefxLastP1ThirdPersonTraceFraction >= 0.0f ) ? (unsigned int)( s_stefxLastP1ThirdPersonTraceFraction * 1000.0f ) : 0u;
	g_SPXBSplitP1LocalX1000 = CG_STEFX_Fixed1000( p1PlayerLocal[0] );
	g_SPXBSplitP1LocalY1000 = CG_STEFX_Fixed1000( p1PlayerLocal[1] );
	g_SPXBSplitP1LocalZ1000 = CG_STEFX_Fixed1000( p1PlayerLocal[2] );
	g_SPXBSplitP2LocalX1000 = CG_STEFX_Fixed1000( p2PlayerLocal[0] );
	g_SPXBSplitP2LocalY1000 = CG_STEFX_Fixed1000( p2PlayerLocal[1] );
	g_SPXBSplitP2LocalZ1000 = CG_STEFX_Fixed1000( p2PlayerLocal[2] );
	g_SPXBSplitLocalDiffX1000 = CG_STEFX_Fixed1000( localDiff[0] );
	g_SPXBSplitLocalDiffY1000 = CG_STEFX_Fixed1000( localDiff[1] );
	g_SPXBSplitLocalDiffZ1000 = CG_STEFX_Fixed1000( localDiff[2] );

	if ( s_logBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_CAM mode=%s time=%d p1Ent=%d p2Ent=%d thirdCvar=%d range=%g angle=%g p1Trace=%g p2Trace=%g fov=(%g,%g)->(%g,%g)",
			thirdPersonMode ? "third" : "first",
			cg.time,
			cg.predicted_player_state.clientNum,
			p2->s.number,
			cg_thirdPerson.integer,
			cg_thirdPersonRange.value,
			cg_thirdPersonAngle.value,
			s_stefxLastP1ThirdPersonTraceFraction,
			p2TraceFraction,
			cg.refdef.fov_x,
			cg.refdef.fov_y,
			p2Refdef->fov_x,
			p2Refdef->fov_y );

		XBLF( "STEFX_SPLIT_CAM P1 ps=(%g,%g,%g) lerp=(%g,%g,%g) eye=(%g,%g,%g) view=(%g,%g,%g) aim=(%g,%g,%g) cam=(%g,%g,%g) dEye=(%g,%g,%g) localView=(%g,%g,%g) localAim=(%g,%g,%g)",
			cg.predicted_player_state.origin[0], cg.predicted_player_state.origin[1], cg.predicted_player_state.origin[2],
			p1Cent->lerpOrigin[0], p1Cent->lerpOrigin[1], p1Cent->lerpOrigin[2],
			p1Eye[0], p1Eye[1], p1Eye[2],
			cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2],
			cg.predicted_player_state.viewangles[0], cg.predicted_player_state.viewangles[1], cg.predicted_player_state.viewangles[2],
			cg.refdefViewAngles[0], cg.refdefViewAngles[1], cg.refdefViewAngles[2],
			p1WorldDelta[0], p1WorldDelta[1], p1WorldDelta[2],
			p1ViewLocal[0], p1ViewLocal[1], p1ViewLocal[2],
			p1PlayerLocal[0], p1PlayerLocal[1], p1PlayerLocal[2] );

		XBLF( "STEFX_SPLIT_CAM P2 ps=(%g,%g,%g) cur=(%g,%g,%g) lerp=(%g,%g,%g) eye=(%g,%g,%g) view=(%g,%g,%g) aim=(%g,%g,%g) cam=(%g,%g,%g) dEye=(%g,%g,%g) localView=(%g,%g,%g) localAim=(%g,%g,%g) localDiff=(%g,%g,%g)",
			p2->client->ps.origin[0], p2->client->ps.origin[1], p2->client->ps.origin[2],
			p2->currentOrigin[0], p2->currentOrigin[1], p2->currentOrigin[2],
			p2Cent->lerpOrigin[0], p2Cent->lerpOrigin[1], p2Cent->lerpOrigin[2],
			p2Eye[0], p2Eye[1], p2Eye[2],
			p2Refdef->vieworg[0], p2Refdef->vieworg[1], p2Refdef->vieworg[2],
			p2->client->ps.viewangles[0], p2->client->ps.viewangles[1], p2->client->ps.viewangles[2],
			p2ViewAngles[0], p2ViewAngles[1], p2ViewAngles[2],
			p2WorldDelta[0], p2WorldDelta[1], p2WorldDelta[2],
			p2ViewLocal[0], p2ViewLocal[1], p2ViewLocal[2],
			p2PlayerLocal[0], p2PlayerLocal[1], p2PlayerLocal[2],
			localDiff[0], localDiff[1], localDiff[2] );

		--s_logBudget;
	}
}

static void CG_STEFX_UpdateSplitP2Refdef( void )
{
	static int s_logBudget = 48;
	int p2EntNum;
	gentity_t *p2;
	refdef_t p2Refdef;
	vec3_t angles;
	vec3_t viewAngles;
	vec3_t target;
	vec3_t pvsOrigin;
	qboolean zoomActive;
	qboolean thirdPersonMode;
	float traceFraction;

	RE_STEFX_SplitScreen_SetP2Refdef( NULL, qfalse );
	g_SPXBSplitP2Ent = 0xffffffffu;
	g_SPXBSplitP2TraceFrac1000 = 0;
	g_SPXBSplitP2RefdefValid = 0;
	g_SPXBSplitP2PsX = 0;
	g_SPXBSplitP2PsY = 0;
	g_SPXBSplitP2PsZ = 0;
	g_SPXBSplitP2CurX = 0;
	g_SPXBSplitP2CurY = 0;
	g_SPXBSplitP2CurZ = 0;
	g_SPXBSplitP2AnglesPitch = 0;
	g_SPXBSplitP2AnglesYaw = 0;
	g_SPXBSplitCameraMode = 0;
	g_SPXBSplitP1TraceFrac1000 = 0;
	g_SPXBSplitP1LocalX1000 = 0;
	g_SPXBSplitP1LocalY1000 = 0;
	g_SPXBSplitP1LocalZ1000 = 0;
	g_SPXBSplitP2LocalX1000 = 0;
	g_SPXBSplitP2LocalY1000 = 0;
	g_SPXBSplitP2LocalZ1000 = 0;
	g_SPXBSplitLocalDiffX1000 = 0;
	g_SPXBSplitLocalDiffY1000 = 0;
	g_SPXBSplitLocalDiffZ1000 = 0;

	if ( !CG_STEFX_SplitScreenActive() )
	{
		return;
	}

	p2EntNum = cg_stefxSplitScreenP2Entity.integer;
	g_SPXBSplitP2Ent = (unsigned int)p2EntNum;
	if ( p2EntNum <= 0 || p2EntNum >= MAX_GENTITIES || !g_entities[p2EntNum].inuse || !g_entities[p2EntNum].client )
	{
		if ( s_logBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_VIEW no p2 refdef ent=%d active=%d players=%d",
				p2EntNum,
				cg_stefxSplitScreen.integer,
				cg_stefxSplitScreenPlayers.integer );
			--s_logBudget;
		}
		return;
	}

	p2 = &g_entities[p2EntNum];
	if ( STEFX_XboxSuppressPlayerPresentation() )
	{
		return;
	}
	zoomActive = (qboolean)( cg_stefxSplitScreenP2Zoom.integer != 0 );
	thirdPersonMode = (qboolean)( cg.renderingThirdPerson != 0 );
	traceFraction = -1.0f;
	g_SPXBSplitP2PsX = (unsigned int)(int)p2->client->ps.origin[0];
	g_SPXBSplitP2PsY = (unsigned int)(int)p2->client->ps.origin[1];
	g_SPXBSplitP2PsZ = (unsigned int)(int)p2->client->ps.origin[2];
	g_SPXBSplitP2CurX = (unsigned int)(int)p2->currentOrigin[0];
	g_SPXBSplitP2CurY = (unsigned int)(int)p2->currentOrigin[1];
	g_SPXBSplitP2CurZ = (unsigned int)(int)p2->currentOrigin[2];
	VectorCopy( p2->client->ps.origin, target );
	target[2] += p2->client->ps.viewheight;
	VectorCopy( p2->client->ps.viewangles, angles );
	g_SPXBSplitP2AnglesPitch = (unsigned int)(int)angles[PITCH];
	g_SPXBSplitP2AnglesYaw = (unsigned int)(int)angles[YAW];

	p2Refdef = cg.refdef;
	if ( thirdPersonMode )
	{
		CG_STEFX_CalcThirdPersonViewForPlayer( &p2->client->ps, p2->client->ps.origin, p2->client->ps.viewangles, p2EntNum, p2Refdef.vieworg, viewAngles, &traceFraction );
	}
	else
	{
		CG_STEFX_ApplyP1FirstPersonRelativeViewToP2( p2, &p2Refdef, viewAngles );
	}
	g_SPXBSplitP2TraceFrac1000 = (traceFraction >= 0.0f) ? (unsigned int)( traceFraction * 1000.0f ) : 0;
	VectorCopy( p2Refdef.vieworg, pvsOrigin );
	g_SPXBSplitP2ViewX = (unsigned int)(int)p2Refdef.vieworg[0];
	g_SPXBSplitP2ViewY = (unsigned int)(int)p2Refdef.vieworg[1];
	g_SPXBSplitP2ViewZ = (unsigned int)(int)p2Refdef.vieworg[2];
	viewAngles[ROLL] = 0.0f;
	AnglesToAxis( viewAngles, p2Refdef.viewaxis );
	if ( zoomActive )
	{
		p2Refdef.fov_x = 45.0f;
	}
	p2Refdef.time = cg.time;
	memcpy( p2Refdef.areamask, cg.snap->areamask, sizeof( p2Refdef.areamask ) );
	RE_STEFX_SplitScreen_SetP2Refdef( &p2Refdef, qtrue );
	RE_STEFX_SplitScreen_SetP2PvsOrigin( pvsOrigin );
	g_SPXBSplitP2RefdefValid = 1;
	CG_STEFX_LogSplitCameraCompare( p2, &p2Refdef, viewAngles, thirdPersonMode, traceFraction );
	if ( !thirdPersonMode )
	{
		CG_STEFX_AddSplitViewWeapon( &p2->client->ps, &cg_entities[p2EntNum], &p2Refdef, viewAngles, 1 );
	}

	if ( s_logBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_VIEW p2 refdef ent=%d npc='%s' mode=%s target=(%g,%g,%g) view=(%g,%g,%g) pvs=(%g,%g,%g) angles=(%g,%g,%g) cameraAngles=(%g,%g,%g) fovX=%g zoom=%d trace=%g",
			p2EntNum,
			p2->NPC_type ? p2->NPC_type : "<null>",
			thirdPersonMode ? "third" : "first",
			target[0],
			target[1],
			target[2],
			p2Refdef.vieworg[0],
			p2Refdef.vieworg[1],
			p2Refdef.vieworg[2],
			pvsOrigin[0],
			pvsOrigin[1],
			pvsOrigin[2],
			angles[0],
			angles[1],
			angles[2],
			viewAngles[0],
			viewAngles[1],
			viewAngles[2],
			p2Refdef.fov_x,
			zoomActive ? 1 : 0,
			traceFraction );
		--s_logBudget;
	}
}

qboolean STEFX_XboxSuppressPlayerPresentation( void )
{
	const qboolean introScrollActive = (cg.scrollTextTime != 0) ? qtrue : qfalse;
	const qboolean lockedTextOverlayActive =
		(cg.captionTextTime != 0
		|| cg.gameTextTime != 0
		|| cg.gameNextTextTime > cg.time
		|| cg.LCARSTextTime > cg.time) ? qtrue : qfalse;

	return (in_camera || introScrollActive || (player_locked && lockedTextOverlayActive)) ? qtrue : qfalse;
}

static qboolean STEFX_XboxSmokeHarnessPresent( void )
{
	static int s_smokeHarnessPresent = 0;
	static int s_smokeHarnessMissLogCount = 0;
	const char *paths[] = {
		"D:\\ef_sp_commands.txt",
		"E:\\ef_sp_commands.txt",
		NULL
	};
	int i;

	if ( s_smokeHarnessPresent )
	{
		return qtrue;
	}

	for ( i = 0; paths[i]; ++i )
	{
		FILE *f = fopen( paths[i], "r" );
		if ( f )
		{
			fclose( f );
			s_smokeHarnessPresent = 1;
			XBLF("STEFX: CG smoke harness file detected '%s'", paths[i]);
			break;
		}
	}

	if ( !s_smokeHarnessPresent && s_smokeHarnessMissLogCount < 4 )
	{
		XBLF("STEFX: CG smoke harness file not visible miss=%d unlockCvar=%d inputCvar=%d",
			s_smokeHarnessMissLogCount,
			cg_stefxSmokeUnlockPlayer.integer,
			cg_stefxSmokeInput.integer);
		++s_smokeHarnessMissLogCount;
	}

	return s_smokeHarnessPresent ? qtrue : qfalse;
}

static qboolean STEFX_XboxSmokeCameraUnlockActive( int serverTime )
{
	static int s_smokeCameraGateLogCount = 0;
	const int startTime = cg_stefxSmokeInputStart.integer > 0 ? cg_stefxSmokeInputStart.integer : 71000;
	const qboolean cvarRequested =
		(cg_stefxSmokeUnlockPlayer.integer != 0 || cg_stefxSmokeInput.integer != 0) ? qtrue : qfalse;
	const qboolean fileRequested = STEFX_XboxSmokeHarnessPresent();
	const qboolean active = cvarRequested && fileRequested && serverTime >= startTime;

	if ( in_camera
		&& s_smokeCameraGateLogCount < 32
		&& ( s_smokeCameraGateLogCount < 4 || serverTime >= startTime - 1000 ) )
	{
		XBLF("STEFX: CG smoke camera gate serverTime=%d start=%d active=%d unlockCvar=%d inputCvar=%d file=%d weapon=%d",
			serverTime,
			startTime,
			active ? 1 : 0,
			cg_stefxSmokeUnlockPlayer.integer,
			cg_stefxSmokeInput.integer,
			fileRequested ? 1 : 0,
			cg.snap ? cg.snap->ps.weapon : -1);
		++s_smokeCameraGateLogCount;
	}

	return active;
}
#endif

float cg_zoomFov;

#define CG_CAM_ABOVE	2

/*
=============================================================================

  MODEL TESTING

The viewthing and gun positioning tools from Q2 have been integrated and
enhanced into a single model testing facility.

Model viewing can begin with either "testmodel <modelname>" or "testgun <modelname>".

The names must be the full pathname after the basedir, like 
"models/weapons/v_launch/tris.md3" or "players/male/tris.md3"

Testmodel will create a fake entity 100 units in front of the current view
position, directly facing the viewer.  It will remain immobile, so you can
move around it to view it from different angles.

Testgun will cause the model to follow the player around and supress the real
view weapon model.  The default frame 0 of most guns is completely off screen,
so you will probably have to cycle a couple frames to see it.

"nextframe", "prevframe", "nextskin", and "prevskin" commands will change the
frame or skin of the testmodel.  These are bound to F5, F6, F7, and F8 in
q3default.cfg.

If a gun is being tested, the "gun_x", "gun_y", and "gun_z" variables will let
you adjust the positioning.

Note that none of the model testing features update while the game is paused, so
it may be convenient to test with deathmatch set to 1 so that bringing down the
console doesn't pause the game.

=============================================================================
*/

/*
=================
CG_TestModel_f

Creates an entity in front of the current position, which
can then be moved around
=================
*/
void CG_TestModel_f (void) {
	vec3_t		angles;

	memset( &cg.testModelEntity, 0, sizeof(cg.testModelEntity) );
	if ( cgi_Argc() < 2 ) {
		return;
	}

	Q_strncpyz (cg.testModelName, CG_Argv( 1 ), MAX_QPATH );
	cg.testModelEntity.hModel = cgi_R_RegisterModel( cg.testModelName );

	if ( cgi_Argc() == 3 ) {
		cg.testModelEntity.backlerp = atof( CG_Argv( 2 ) );
		cg.testModelEntity.frame = 1;
		cg.testModelEntity.oldframe = 0;
	}
	if (! cg.testModelEntity.hModel ) {
		CG_Printf( "Can't register model\n" );
		return;
	}

	VectorMA( cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin );

	angles[PITCH] = 0;
	angles[YAW] = 180 + cg.refdefViewAngles[1];
	angles[ROLL] = 0;

	AnglesToAxis( angles, cg.testModelEntity.axis );
	cg.testGun = qfalse;
}

/*
=================
CG_TestGun_f

Replaces the current view weapon with the given model
=================
*/
void CG_TestGun_f (void) {
	CG_TestModel_f();
	cg.testGun = qtrue;
	cg.testModelEntity.renderfx = RF_MINLIGHT | RF_DEPTHHACK | RF_FIRST_PERSON;
}


void CG_TestModelNextFrame_f (void) {
	cg.testModelEntity.frame++;
	CG_Printf( "frame %i\n", cg.testModelEntity.frame );
}

void CG_TestModelPrevFrame_f (void) {
	cg.testModelEntity.frame--;
	if ( cg.testModelEntity.frame < 0 ) {
		cg.testModelEntity.frame = 0;
	}
	CG_Printf( "frame %i\n", cg.testModelEntity.frame );
}

void CG_TestModelNextSkin_f (void) {
	cg.testModelEntity.skinNum++;
	CG_Printf( "skin %i\n", cg.testModelEntity.skinNum );
}

void CG_TestModelPrevSkin_f (void) {
	cg.testModelEntity.skinNum--;
	if ( cg.testModelEntity.skinNum < 0 ) {
		cg.testModelEntity.skinNum = 0;
	}
	CG_Printf( "skin %i\n", cg.testModelEntity.skinNum );
}

static void CG_AddTestModel (void) {
	int		i;

	// re-register the model, because the level may have changed	
/*	cg.testModelEntity.hModel = cgi_R_RegisterModel( cg.testModelName );
	if (! cg.testModelEntity.hModel ) {
		CG_Printf ("Can't register model\n");
		return;
	}
*/
	// if testing a gun, set the origin reletive to the view origin
	if ( cg.testGun ) {
		VectorCopy( cg.refdef.vieworg, cg.testModelEntity.origin );
		VectorCopy( cg.refdef.viewaxis[0], cg.testModelEntity.axis[0] );
		VectorCopy( cg.refdef.viewaxis[1], cg.testModelEntity.axis[1] );
		VectorCopy( cg.refdef.viewaxis[2], cg.testModelEntity.axis[2] );

		// allow the position to be adjusted
		for (i=0 ; i<3 ; i++) {
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[0][i] * cg_gun_x.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[1][i] * cg_gun_y.value;
			cg.testModelEntity.origin[i] += cg.refdef.viewaxis[2][i] * cg_gun_z.value;
		}
	}

	cgi_R_AddRefEntityToScene( &cg.testModelEntity );
}



//============================================================================


/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/
void CG_CalcVrect (void) {
	int		size;

	// the intermission should allways be full screen
	if ( cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION ) {
		size = 100;
	} else {
		// bound normal viewsize
		if (cg_viewsize.integer < 30) {
			cgi_Cvar_Set ("cg_viewsize","30");
			size = 30;
		} else if (cg_viewsize.integer > 100) {
			cgi_Cvar_Set ("cg_viewsize","100");
			size = 100;
		} else {
			size = cg_viewsize.integer;
		}

	}
	cg.refdef.width = cgs.glconfig.vidWidth * size * 0.01;
	cg.refdef.width &= ~1;

	cg.refdef.height = cgs.glconfig.vidHeight * size * 0.01;
	cg.refdef.height &= ~1;

	cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) * 0.5;
	cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) * 0.5;
}

//==============================================================================


/*
===============
CG_OffsetThirdPersonView

===============
*/
static void CG_OffsetThirdPersonView( void ) {
#ifdef _XBOX
	float traceFraction;

	CG_STEFX_CalcThirdPersonViewForPlayer( &cg.predicted_player_state, cg.refdef.vieworg, cg.refdefViewAngles, cg.predicted_player_state.clientNum, cg.refdef.vieworg, cg.refdefViewAngles, &traceFraction );
	s_stefxLastP1ThirdPersonTraceFraction = traceFraction;
#else
	vec3_t		forward, right, up;
	vec3_t		view;
	vec3_t		focusAngles;
	trace_t		trace;
	static vec3_t	mins = { -4, -4, -4 };
	static vec3_t	maxs = { 4, 4, 4 };
	vec3_t		focusPoint;
	float		focusDist;
	float		forwardScale, sideScale;

	cg.refdef.vieworg[2] += cg.predicted_player_state.viewheight;

	VectorCopy( cg.refdefViewAngles, focusAngles );

	// if dead, look at killer
	if ( cg.predicted_player_state.stats[STAT_HEALTH] <= 0 ) {
		focusAngles[YAW] = cg.predicted_player_state.stats[STAT_DEAD_YAW];
		cg.refdefViewAngles[YAW] = cg.predicted_player_state.stats[STAT_DEAD_YAW];
	}

	if ( focusAngles[PITCH] > 45 ) {
		focusAngles[PITCH] = 45;		// don't go too far overhead
	}
	AngleVectors( focusAngles, forward, NULL, NULL );

	VectorMA( cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint );

	VectorCopy( cg.refdef.vieworg, view );

	view[2] += 8;

	cg.refdefViewAngles[PITCH] *= 0.5;

	AngleVectors( cg.refdefViewAngles, forward, right, up );

	forwardScale = cos( cg_thirdPersonAngle.value / 180 * M_PI );
	sideScale = sin( cg_thirdPersonAngle.value / 180 * M_PI );
	VectorMA( view, -cg_thirdPersonRange.value * forwardScale, forward, view );
	VectorMA( view, -cg_thirdPersonRange.value * sideScale, right, view );

	// trace a ray from the origin to the viewpoint to make sure the view isn't
	// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

	CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predicted_player_state.clientNum, MASK_SOLID );

	if ( trace.fraction != 1.0 ) {
		VectorCopy( trace.endpos, view );
		view[2] += (1.0 - trace.fraction) * 32;
		// try another trace to this position, because a tunnel may have the ceiling
		// close enogh that this is poking out

		CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predicted_player_state.clientNum, MASK_SOLID );
		VectorCopy( trace.endpos, view );
	}


	VectorCopy( view, cg.refdef.vieworg );

	// select pitch to look at focus point from vieword
	VectorSubtract( focusPoint, cg.refdef.vieworg, focusPoint );
	focusDist = sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1 ) {
		focusDist = 1;	// should never happen
	}
	cg.refdefViewAngles[PITCH] = -180 / M_PI * atan2( focusPoint[2], focusDist );
	cg.refdefViewAngles[YAW] -= cg_thirdPersonAngle.value;
#endif
}


#define MIN_CAMERA_HEIGHT	75
#define MIN_CAMERA_PITCH	40
#define MAX_CAMERA_PITCH	90

//----------------------------------------------
static void CG_OffsetThirdPersonOverheadView( void ) {
	vec3_t		view, angs;
	trace_t		trace;
	static vec3_t	mins = { -4, -4, -4 };
	static vec3_t	maxs = { 4, 4, 4 };

	VectorCopy( cg.refdef.vieworg, view );

	// Move straight up from the player, making sure to always go at least the min camera height, 
	//	otherwise, the camera will clip into the head of the player.
	if ( cg_thirdPersonRange.value < MIN_CAMERA_HEIGHT )
	{
		view[2] += MIN_CAMERA_HEIGHT;
	}
	else
	{
		view[2] += cg_thirdPersonRange.value;
	}

	// Now adjust the camera angles, but we shouldn't adjust the viewAngles...only the viewAxis
	VectorCopy( cg.refdefViewAngles, angs );
	angs[PITCH] = cg_thirdPersonAngle.integer;

	// Simple clamp to weed out any really obviously nasty angles
	if ( angs[PITCH] < MIN_CAMERA_PITCH )
	{
		angs[PITCH] = MIN_CAMERA_PITCH;
	}
	else if ( angs[PITCH] > MAX_CAMERA_PITCH )
	{
		angs[PITCH] = MAX_CAMERA_PITCH;
	}

	// Convert our new desired camera angles and store them where they will get used by the engine 
	//	when setting up the actual camera view.
	AnglesToAxis( angs, cg.refdef.viewaxis );
	cg.refdefViewAngles[PITCH] = 0;
	g_entities[0].client->ps.delta_angles[PITCH] = 0;
	
	// Trace a ray from the origin to the viewpoint to make sure the view isn't
	//	in a solid block.
	CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predicted_player_state.clientNum, MASK_SOLID );

	if ( trace.fraction != 1.0 ) 
	{
		VectorCopy( trace.endpos, cg.refdef.vieworg );
	}
	else
	{
		VectorCopy( view, cg.refdef.vieworg );
	}
}

// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset( void ) {
	int		timeDelta;
	
	// smooth out stair climbing
	timeDelta = cg.time - cg.stepTime;
	if ( timeDelta < STEP_TIME ) {
		cg.refdef.vieworg[2] -= cg.stepChange 
			* (STEP_TIME - timeDelta) / STEP_TIME;
	}
}

/*
===============
CG_OffsetFirstPersonView

===============
*/
static void CG_OffsetFirstPersonView( void ) {
	float			*origin;
	float			*angles;
	float			bob;
	float			ratio;
	float			delta;
	float			speed;
	float			f;
	vec3_t			predictedVelocity;
	int				timeDelta;
	
	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		return;
	}

	origin = cg.refdef.vieworg;
	angles = cg.refdefViewAngles;

	// if dead, fix the angle and don't add any kick
	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		angles[ROLL] = 40;
		angles[PITCH] = -15;
		angles[YAW] = cg.snap->ps.stats[STAT_DEAD_YAW];
		origin[2] += cg.predicted_player_state.viewheight;
		return;
	}

	// add angles based on weapon kick
	VectorAdd (angles, cg.kick_angles, angles);

	// add angles based on damage kick
	if ( cg.damageTime ) {
		ratio = cg.time - cg.damageTime;
		if ( ratio < DAMAGE_DEFLECT_TIME ) {
			ratio /= DAMAGE_DEFLECT_TIME;
			angles[PITCH] += ratio * cg.v_dmg_pitch;
			angles[ROLL] += ratio * cg.v_dmg_roll;
		} else {
			ratio = 1.0 - ( ratio - DAMAGE_DEFLECT_TIME ) / DAMAGE_RETURN_TIME;
			if ( ratio > 0 ) {
				angles[PITCH] += ratio * cg.v_dmg_pitch;
				angles[ROLL] += ratio * cg.v_dmg_roll;
			}
		}
	}

	// add pitch based on fall kick
#if 0
	ratio = ( cg.time - cg.landTime) / FALL_TIME;
	if (ratio < 0)
		ratio = 0;
	angles[PITCH] += ratio * cg.fall_value;
#endif

	// add angles based on velocity
	VectorCopy( cg.predicted_player_state.velocity, predictedVelocity );

	delta = DotProduct ( predictedVelocity, cg.refdef.viewaxis[0]);
	angles[PITCH] += delta * cg_runpitch.value;
	
	delta = DotProduct ( predictedVelocity, cg.refdef.viewaxis[1]);
	angles[ROLL] -= delta * cg_runroll.value;

	// add angles based on bob

	// make sure the bob is visible even at low speeds
	speed = cg.xyspeed > 200 ? cg.xyspeed : 200;

	delta = cg.bobfracsin * cg_bobpitch.value * speed;
	if (cg.predicted_player_state.pm_flags & PMF_DUCKED)
		delta *= 3;		// crouching
	angles[PITCH] += delta;
	delta = cg.bobfracsin * cg_bobroll.value * speed;
	if (cg.predicted_player_state.pm_flags & PMF_DUCKED)
		delta *= 3;		// crouching accentuates roll
	if (cg.bobcycle & 1)
		delta = -delta;
	angles[ROLL] += delta;

//===================================

	// add view height
	origin[2] += cg.predicted_player_state.viewheight;

	// smooth out duck height changes
	timeDelta = cg.time - cg.duckTime;
	if ( timeDelta < DUCK_TIME) {
		cg.refdef.vieworg[2] -= cg.duckChange * (DUCK_TIME - timeDelta) / DUCK_TIME;
	}

	// add bob height
	bob = cg.bobfracsin * cg.xyspeed * cg_bobup.value;
	if (bob > 6) {
		bob = 6;
	}

	origin[2] += bob;


	// add fall height
	delta = cg.time - cg.landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		f = delta / LAND_DEFLECT_TIME;
		cg.refdef.vieworg[2] += cg.landChange * f;
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		delta -= LAND_DEFLECT_TIME;
		f = 1.0 - ( delta / LAND_RETURN_TIME );
		cg.refdef.vieworg[2] += cg.landChange * f;
	}

	// add step offset
	CG_StepOffset();

	if(cg.snap->ps.leanofs != 0)
	{
		vec3_t	right;
		//add leaning offset
		//FIXME: when crouching, this bounces up and down?!
		cg.refdefViewAngles[2] += (float)cg.snap->ps.leanofs/2;
		AngleVectors(cg.refdefViewAngles, NULL, right, NULL);
		VectorMA(cg.refdef.vieworg, (float)cg.snap->ps.leanofs, right, cg.refdef.vieworg);
	}

	// pivot the eye based on a neck length
#if 0
	{
#define	NECK_LENGTH		8
	vec3_t			forward, up;
 
	cg.refdef.vieworg[2] -= NECK_LENGTH;
	AngleVectors( cg.refdefViewAngles, forward, NULL, up );
	VectorMA( cg.refdef.vieworg, 3, forward, cg.refdef.vieworg );
	VectorMA( cg.refdef.vieworg, NECK_LENGTH, up, cg.refdef.vieworg );
	}
#endif

	{
		centity_t	*playerCent = &cg_entities[0];

		if ( playerCent && playerCent->gent && playerCent->gent->client )
		{
			VectorCopy( cg.refdef.vieworg, playerCent->gent->client->renderInfo.eyePoint );
			VectorCopy( cg.refdefViewAngles, playerCent->gent->client->renderInfo.eyeAngles );
		}
	}
}

//======================================================================

void CG_ZoomDown_f( void )
 { 
	// Ignore zoom requests when yer paused
	if ( cg_paused.integer || in_camera )
	{
		return;
	}
	
	//Captain Proton has no zoom technology 
	if ( cg.weaponSelect == WP_PROTON_GUN )
	{
		cg.zoomLocked = qfalse;
		cg.zoomed = qfalse;
		return;
	}

	// The zoom hasn't been started yet, so do it now
	if ( !cg.zoomed )
	{
		cg.zoomLocked = qfalse;
		cg.zoomed = qtrue;
		cg_zoomFov = cg_fov.value;
		cg.zoomTime = cg.time;
		cgi_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.zoomStart );
		return;
	}

	// Can only snap out of the zoom mode if it has already been locked (CG_ZoomUp_f has been called)
	if ( cg.zoomLocked )
	{
		// Snap out of zoom mode
		cg.zoomed = qfalse;
		cg.zoomTime = cg.time;
		cgi_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.zoomEnd );
	}
}
 
void CG_ZoomUp_f( void )
 { 
	// Ignore zoom requests when yer paused
	if ( cg_paused.integer || in_camera )
	{
		return;
	}

	if ( cg.zoomed ) {
		// Freeze the zoom mode
		cg.zoomLocked = qtrue;
	}
}

void CG_ZoomOff_f( void )
{
	if ( cg_paused.integer || in_camera )
	{
		return;
	}

	if ( cg.zoomed )
	{
		cg.zoomLocked = qfalse;
		cg.zoomed = qfalse;
		cg.zoomTime = cg.time;
		cgi_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.zoomEnd );
	}
}


/*
====================
CG_CalcFovFromX

Calcs Y FOV from given X FOV
====================
*/
#define	WAVE_AMPLITUDE	1
#define	WAVE_FREQUENCY	0.4

qboolean CG_CalcFOVFromX( float fov_x ) 
{
	float	x;
//	float	phase;
//	float	v;
//	int		contents;
	float	fov_y;
	qboolean	inwater;

	x = cg.refdef.width / tan( fov_x / 360 * M_PI );
	fov_y = atan2( cg.refdef.height, x );
	fov_y = fov_y * 360 / M_PI;

	// there's a problem with this, it only takes the leafbrushes into account, not the entity brushes,
	//	so if you give slime/water etc properties to a func_door area brush in order to move the whole water 
	//	level up/down this doesn't take into account the door position, so warps the view the whole time
	//	whether the water is up or not. Fortunately there's only one slime area in Trek that you can be under,
	//	so lose it...
#if 0
/*
	// warp if underwater
	contents = CG_PointContents( cg.refdef.vieworg, -1 );
	if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ){
		phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		v = WAVE_AMPLITUDE * sin( phase );
		fov_x += v;
		fov_y -= v;
		inwater = qtrue;
	}
	else {
		inwater = qfalse;
	}
*/
#else
	inwater = qfalse;
#endif


	// set it
	cg.refdef.fov_x = fov_x;
	cg.refdef.fov_y = fov_y;

	return (inwater);
}

#ifdef _XBOX
static qboolean STEFX_XboxBadFloat( float value )
{
	return (value != value || value < -1000000.0f || value > 1000000.0f);
}

static qboolean STEFX_XboxBadVec3( const vec3_t value )
{
	return (STEFX_XboxBadFloat(value[0]) || STEFX_XboxBadFloat(value[1]) || STEFX_XboxBadFloat(value[2]));
}

static void STEFX_XboxRepairRefdef( const char *stage )
{
	static int s_logBudget = 48;
	qboolean badRefdef;
	playerState_t *ps;
	vec3_t repairAngles;
	int viewheight;
	float repairFov;

	ps = &cg.predicted_player_state;
	badRefdef =
		(cg.refdef.width <= 0 || cg.refdef.height <= 0 ||
		cg.refdef.fov_x < 1.0f || cg.refdef.fov_y < 1.0f ||
		STEFX_XboxBadFloat(cg.refdef.fov_x) || STEFX_XboxBadFloat(cg.refdef.fov_y) ||
		STEFX_XboxBadVec3(cg.refdef.vieworg) || STEFX_XboxBadVec3(cg.refdefViewAngles) ||
		STEFX_XboxBadVec3(cg.refdef.viewaxis[0]));

	if (!badRefdef)
	{
		return;
	}

	if (s_logBudget > 0)
	{
		XBLF("STEFX: cgame refdef repair needed stage=%s time=%d rect=%d,%d %dx%d fov=(%g,%g) view=(%g,%g,%g) angles=(%g,%g,%g) predOrg=(%g,%g,%g) predView=(%g,%g,%g) snap=%p",
			stage ? stage : "<null>",
			cg.time,
			cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height,
			cg.refdef.fov_x, cg.refdef.fov_y,
			cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2],
			cg.refdefViewAngles[0], cg.refdefViewAngles[1], cg.refdefViewAngles[2],
			ps->origin[0], ps->origin[1], ps->origin[2],
			ps->viewangles[0], ps->viewangles[1], ps->viewangles[2],
			(void*)cg.snap);
	}

	if (cg.refdef.width <= 0)
	{
		cg.refdef.width = cgs.glconfig.vidWidth > 0 ? cgs.glconfig.vidWidth : 640;
	}
	if (cg.refdef.height <= 0)
	{
		cg.refdef.height = cgs.glconfig.vidHeight > 0 ? cgs.glconfig.vidHeight : 480;
	}

	if (STEFX_XboxBadVec3(ps->origin) && cg.snap)
	{
		ps = &cg.snap->ps;
	}

	if (STEFX_XboxBadVec3(cg.refdef.vieworg) ||
		(VectorCompare(cg.refdef.vieworg, vec3_origin) && !VectorCompare(ps->origin, vec3_origin)))
	{
		VectorCopy(ps->origin, cg.refdef.vieworg);
		viewheight = ps->viewheight;
		if (viewheight < 8 || viewheight > 96)
		{
			viewheight = 54;
		}
		cg.refdef.vieworg[2] += viewheight;
	}

	VectorCopy(ps->viewangles, repairAngles);
	if (STEFX_XboxBadVec3(repairAngles))
	{
		VectorClear(repairAngles);
		repairAngles[YAW] = 90.0f;
	}
	VectorCopy(repairAngles, cg.refdefViewAngles);
	AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);

	if (cg.refdef.fov_x < 1.0f || cg.refdef.fov_y < 1.0f ||
		STEFX_XboxBadFloat(cg.refdef.fov_x) || STEFX_XboxBadFloat(cg.refdef.fov_y))
	{
		repairFov = cg_fov.value;
		if (STEFX_XboxBadFloat(repairFov) || repairFov < 1.0f || repairFov > 160.0f)
		{
			repairFov = 80.0f;
		}
		CG_CalcFOVFromX(repairFov);
	}

	if (s_logBudget > 0)
	{
		XBLF("STEFX: cgame refdef repaired stage=%s time=%d rect=%d,%d %dx%d fov=(%g,%g) view=(%g,%g,%g) angles=(%g,%g,%g) axis0=(%g,%g,%g)",
			stage ? stage : "<null>",
			cg.time,
			cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height,
			cg.refdef.fov_x, cg.refdef.fov_y,
			cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2],
			cg.refdefViewAngles[0], cg.refdefViewAngles[1], cg.refdefViewAngles[2],
			cg.refdef.viewaxis[0][0], cg.refdef.viewaxis[0][1], cg.refdef.viewaxis[0][2]);
		--s_logBudget;
	}
}
#endif
/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/

static qboolean	CG_CalcFov( void ) {
	float	fov_x;
	float	f;

	if ( cg.predicted_player_state.pm_type == PM_INTERMISSION ) {
		// if in intermission, use a fixed value
		fov_x = 80;
	} else {
		// user selectable
		fov_x = cg_fov.value;
		if ( fov_x < 1 ) {
			fov_x = 1;
		} else if ( fov_x > 160 ) {
			fov_x = 160;
		}

		// Disable zooming when in third person
		if ( cg.zoomed && !cg.renderingThirdPerson )
		{
			if ( !cg.zoomLocked )
			{
				// Interpolate current zoom level
				cg_zoomFov = cg_fov.value - ((float)( cg.time - cg.zoomTime ) / ZOOM_IN_TIME + ZOOM_START_PERCENT) 
									* ( cg_fov.value - MAX_ZOOM_FOV );

				// Clamp zoomFov
				if ( cg_zoomFov < MAX_ZOOM_FOV )
					cg_zoomFov = MAX_ZOOM_FOV;
				else if ( cg_zoomFov > cg_fov.value )
					cg_zoomFov = cg_fov.value;
				else
				{//still zooming
					static zoomSoundTime = 0;

					if ( zoomSoundTime < cg.time )
					{
						cgi_S_StartSound( cg.refdef.vieworg, ENTITYNUM_WORLD, CHAN_LOCAL, cgs.media.zoomLoop );
						zoomSoundTime = cg.time + 300;
					}
				}
			}

			fov_x = cg_zoomFov;
		} else {
			f = ( cg.time - cg.zoomTime ) / ZOOM_OUT_TIME;
			if ( f > 1.0 ) {
				fov_x = fov_x;
			} else {
				fov_x = cg_zoomFov + f * ( fov_x - cg_zoomFov );
			}
		}
	}

	return ( CG_CalcFOVFromX( fov_x ) );
}



/*
===============
CG_DamageBlendBlob

===============
*/
static void CG_DamageBlendBlob( void ) 
{
	int			t;
	int			maxTime;
	refEntity_t		ent;

	if ( !cg.damageValue ) {
		return;
	}

	// ragePro systems can't fade blends, so don't obscure the screen
	if ( cgs.glconfig.hardwareType == GLHW_RAGEPRO ) {
		return;
	}

	maxTime = DAMAGE_TIME;
	t = cg.time - cg.damageTime;
	if ( t <= 0 || t >= maxTime ) {
		return;
	}

	memset( &ent, 0, sizeof( ent ) );
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_FIRST_PERSON;

	VectorMA( cg.refdef.vieworg, 8, cg.refdef.viewaxis[0], ent.origin );
	VectorMA( ent.origin, cg.damageX * -8, cg.refdef.viewaxis[1], ent.origin );
	VectorMA( ent.origin, cg.damageY * 8, cg.refdef.viewaxis[2], ent.origin );

	ent.radius = cg.damageValue * 3 * ( 1.0 - ((float)t / maxTime) );
	ent.customShader = cgs.media.borgEyeFlareShader;
	ent.shaderRGBA[0] = 180 * ( 1.0 - ((float)t / maxTime) );
	ent.shaderRGBA[1] = 50 * ( 1.0 - ((float)t / maxTime) );
	ent.shaderRGBA[2] = 50 * ( 1.0 - ((float)t / maxTime) );
	ent.shaderRGBA[3] = 255;

	cgi_R_AddRefEntityToScene( &ent );
}


/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
static qboolean CG_CalcViewValues( void ) {
	playerState_t	*ps;

	memset( &cg.refdef, 0, sizeof( cg.refdef ) );

	// calculate size of 3D view
	CG_CalcVrect();

	ps = &cg.predicted_player_state;

	// intermission view
	if ( ps->pm_type == PM_INTERMISSION ) {
		VectorCopy( ps->origin, cg.refdef.vieworg );
		VectorCopy( ps->viewangles, cg.refdefViewAngles );
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
		return CG_CalcFov();
	}

	cg.bobcycle = ( ps->bobCycle & 128 ) >> 7;
	cg.bobfracsin = fabs( sin( ( ps->bobCycle & 127 ) / 127.0 * M_PI ) );
	cg.xyspeed = sqrt( ps->velocity[0] * ps->velocity[0] +
		ps->velocity[1] * ps->velocity[1] );


	VectorCopy( ps->origin, cg.refdef.vieworg );
	VectorCopy( ps->viewangles, cg.refdefViewAngles );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	s_stefxLastP1ThirdPersonTraceFraction = -1.0f;
#endif

	// add error decay
	if ( cg_errorDecay.value > 0 ) {
		int		t;
		float	f;

		t = cg.time - cg.predictedErrorTime;
		f = ( cg_errorDecay.value - t ) / cg_errorDecay.value;
		if ( f > 0 && f < 1 ) {
			VectorMA( cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg );
		} else {
			cg.predictedErrorTime = 0;
		}
	}

	if ( cg.renderingThirdPerson ) {
		// back away from character
		if ( cg_thirdPerson.integer == CG_CAM_ABOVE)
		{			
			CG_OffsetThirdPersonOverheadView();
		}
		else
		{
			CG_OffsetThirdPersonView();
		}
	} else {
		// offset for local bobbing and kicks
		CG_OffsetFirstPersonView();
	}

	// shake the camera if necessary
	CGCam_UpdateShake( cg.refdef.vieworg, cg.refdefViewAngles );

	// Doing this when the camera is directly above will look bad
	if ( cg_thirdPerson.integer != CG_CAM_ABOVE )
	{
		// position eye reletive to origin
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
	}

	if ( cg.hyperspace ) {
		cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
	}

	// field of view
	return CG_CalcFov();
}


/*
=====================
CG_PowerupTimerSounds
=====================
*/
static void CG_PowerupTimerSounds( void ) {
/*	int		i;
	int		t;

	// powerup timers going away
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		t = cg.snap->ps.powerups[i];
		if ( t <= cg.time ) {
			continue;
		}
		if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
			continue;
		}
		if ( ( t - cg.time ) / POWERUP_BLINK_TIME != ( t - cg.oldTime ) / POWERUP_BLINK_TIME ) {
			cgi_S_StartSound( NULL, cg.snap->ps.clientNum, CHAN_ITEM, cgs.media.wearOffSound );
		}
	}
*/
}

//=========================================================================

/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/
extern void CG_BuildSolidList( void );
#ifdef _XBOX
extern int g_stefxCgRenderSceneMsec;
extern int g_stefxCgDraw2DMsec;
extern "C" volatile unsigned int g_SPXBCgEntTypeCycles[16];
extern "C" volatile unsigned int g_SPXBCgEntTypeCounts[16];
extern "C" volatile unsigned int g_SPXBCgHiddenPlayersSkipped;
#endif
void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView ) {
	qboolean	inwater = qfalse;
#ifdef _XBOX
	g_SPXBClTailStage = 0x43473030; /* 'CG00' */
	static int s_xboxLastProfileTime = -100000;
	const int xboxProfileStart = cgi_Milliseconds();
	int xboxProfileSetup = 0;
	int xboxProfilePredict = 0;
	int xboxProfileView = 0;
	int xboxProfileEntities = 0;
	int xboxProfileTail = 0;
	int xboxProfileDraw = 0;
	int xboxProfileMark = xboxProfileStart;
#if STEFX_COOP_CGAME_DETAIL
	int xboxTailParts[6] = {0};
	int xboxTailPartStart = 0;
#endif
	const int xboxDrawLog = (serverTime >= 3400 && serverTime <= 4600);
	const qboolean stefxHardwareFirstFrame = ( cg.clientFrame == 0 );
	if (xboxDrawLog)
	{
		XBLF("JA: CL_EARLY EF CG_DrawActiveFrame enter serverTime=%d stereo=%d snap=%p next=%p info='%s'",
			serverTime, (int)stereoView, (void*)cg.snap, (void*)cg.nextSnap, cg.infoScreenText);
	}
#endif

	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before set time");
	cg.time = serverTime;
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after set time");

	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_BuildSolidList");
	g_SPXBPhaseLast = 0x45465330; /* 'EFS0' */
	CG_BuildSolidList();
	g_SPXBPhaseLast = 0x45465331; /* 'EFS1' */
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_BuildSolidList");
	// update cvars
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_UpdateCvars");
	CG_UpdateCvars();
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_UpdateCvars");

	// if we are only updating the screen as a loading
	// pacifier, don't even try to read snapshots
	if ( cg.infoScreenText[0] != 0 ) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame infoScreenText path");
		CG_DrawInformation();
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame infoScreenText return");
		return;
	}

	// any looped sounds will be respecified as entities
	// are added to the render list
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before S_ClearLoopingSounds");
	cgi_S_ClearLoopingSounds();
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after S_ClearLoopingSounds");

	// clear all the render lists
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before R_ClearScene");
	cgi_R_ClearScene();
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after R_ClearScene");

	// set up cg.snap and possibly cg.nextSnap
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_ProcessSnapshots");
	g_SPXBPhaseLast = 0x45465030; /* 'EFP0' */
	CG_ProcessSnapshots();
	g_SPXBClTailStage = 0x43473031; /* 'CG01' */
	g_SPXBPhaseLast = 0x45465031; /* 'EFP1' */
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_ProcessSnapshots");
#ifdef _XBOX
	xboxProfileSetup = cgi_Milliseconds() - xboxProfileMark;
	xboxProfileMark = cgi_Milliseconds();
#endif
#if defined(STEFX_ELITE_FORCE_SP)
	if ( stefxHardwareFirstFrame )
	{
		XBLog_WriteCriticalf( "STEFX_HW_CHECKPOINT: first frame snapshots complete snap=%p ents=%d",
			cg.snap, cg.snap ? cg.snap->numEntities : -1 );
	}
#endif

	// if we haven't received any snapshots yet, all
	// we can draw is the information screen
	if ( !cg.snap ) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame no snap path");
		CG_DrawInformation();
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame no snap return");
		return;
	}
#ifdef _XBOX
	if (xboxDrawLog)
	{
		XBLF("JA: CL_EARLY EF CG_DrawActiveFrame snap ready time=%d ents=%d client=%d weapon=%d health=%d",
			cg.snap->serverTime, cg.snap->numEntities, cg.snap->ps.clientNum, cg.snap->ps.weapon, cg.snap->ps.stats[STAT_HEALTH]);
	}
#endif

	// let the client system know what our weapon and zoom settings are
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before SetUserCmdValue");
	cgi_SetUserCmdValue( cg.weaponSelect, cg.refdef.fov_y / 75.0 );
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after SetUserCmdValue");

	// this counter will be bumped for every valid scene we generate
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before clientFrame++");
	cg.clientFrame++;
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after clientFrame++");

	// update cg.predicted_player_state
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_PredictPlayerState");
	g_SPXBPhaseLast = 0x45464430; /* 'EFD0' */
	CG_PredictPlayerState();
	g_SPXBClTailStage = 0x43473032; /* 'CG02' */
	g_SPXBPhaseLast = 0x45464431; /* 'EFD1' */
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_PredictPlayerState");
#ifdef _XBOX
	xboxProfilePredict = cgi_Milliseconds() - xboxProfileMark;
	xboxProfileMark = cgi_Milliseconds();
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( stefxHardwareFirstFrame )
	{
		XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame prediction complete" );
	}
#endif
#ifdef _XBOX
	if ( in_camera && STEFX_XboxSmokeCameraUnlockActive( serverTime ) )
	{
		static int s_stefxSmokeCameraDisableLogCount = 0;
		if ( s_stefxSmokeCameraDisableLogCount < 16 )
		{
			XBLF("STEFX: CG smoke camera disable serverTime=%d start=%d cgTime=%d weapon=%d unlockCvar=%d inputCvar=%d",
				serverTime,
				cg_stefxSmokeInputStart.integer,
				cg.time,
				cg.snap ? cg.snap->ps.weapon : -1,
				cg_stefxSmokeUnlockPlayer.integer,
				cg_stefxSmokeInput.integer);
			++s_stefxSmokeCameraDisableLogCount;
		}
		CGCam_Disable();
	}
#endif
#ifdef _XBOX
	if (xboxDrawLog)
	{
		XBLF("STEFX: cgame predict state time=%d predOrg=(%g,%g,%g) predView=(%g,%g,%g) snapOrg=(%g,%g,%g) snapView=(%g,%g,%g) weapon=%d health=%d viewheight=%d",
			cg.time,
			cg.predicted_player_state.origin[0], cg.predicted_player_state.origin[1], cg.predicted_player_state.origin[2],
			cg.predicted_player_state.viewangles[0], cg.predicted_player_state.viewangles[1], cg.predicted_player_state.viewangles[2],
			cg.snap->ps.origin[0], cg.snap->ps.origin[1], cg.snap->ps.origin[2],
			cg.snap->ps.viewangles[0], cg.snap->ps.viewangles[1], cg.snap->ps.viewangles[2],
			cg.predicted_player_state.weapon,
			cg.predicted_player_state.stats[STAT_HEALTH],
			cg.predicted_player_state.viewheight);
	}
#endif

	// decide on third person view
	cg.renderingThirdPerson = cg_thirdPerson.integer || (cg.snap->ps.stats[STAT_HEALTH] <= 0);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( CG_STEFX_SplitScreenActive() )
	{
		static int s_splitThirdPersonLogBudget = 24;
		cg.zoomed = qfalse;
		if ( s_splitThirdPersonLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_VIEW split camera mode active=%d players=%d p2=%d serverTime=%d camera=%d thirdCvar=%d renderingThird=%d",
				cg_stefxSplitScreen.integer,
				cg_stefxSplitScreenPlayers.integer,
				cg_stefxSplitScreenP2Entity.integer,
				serverTime,
				in_camera ? 1 : 0,
				cg_thirdPerson.integer,
				cg.renderingThirdPerson ? 1 : 0 );
			--s_splitThirdPersonLogBudget;
		}
	}
#endif

	if ( in_camera )
	{
		// The camera takes over the view
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CGCam_RenderScene");
		g_SPXBPhaseLast = 0x45465630; /* 'EFV0' */
		CGCam_RenderScene();		
		g_SPXBClTailStage = 0x43473033; /* 'CG03' */
		g_SPXBPhaseLast = 0x45465631; /* 'EFV1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CGCam_RenderScene");
	}
	else
	{		
		//Finish any fading that was happening
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CGCam_UpdateFade");
		CGCam_UpdateFade();
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CGCam_UpdateFade");
		// build cg.refdef
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_CalcViewValues");
		g_SPXBPhaseLast = 0x45465630; /* 'EFV0' */
		inwater = CG_CalcViewValues();
		g_SPXBClTailStage = 0x43473033; /* 'CG03' */
		g_SPXBPhaseLast = 0x45465631; /* 'EFV1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_CalcViewValues");
	}
#ifdef _XBOX
	xboxProfileView = cgi_Milliseconds() - xboxProfileMark;
	xboxProfileMark = cgi_Milliseconds();
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( stefxHardwareFirstFrame )
	{
		XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame view complete" );
	}
#endif
#if defined(_XBOX) && defined(STEFX_XBOX_SURVIVAL_HACKS)
	STEFX_XboxRepairRefdef("post-view");
#endif

	//This is done from the vieworg to get origin for non-attenuated sounds
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before S_UpdateAmbientSet");
	cgi_S_UpdateAmbientSet( CG_ConfigString( CS_AMBIENT_SET ), cg.refdef.vieworg );
	g_SPXBClTailStage = 0x43473430; /* 'CG40' */
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after S_UpdateAmbientSet");

	// first person blend blobs, done after AnglesToAxis
	if ( !cg.renderingThirdPerson ) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_DamageBlendBlob");
		CG_DamageBlendBlob();
		g_SPXBClTailStage = 0x43473431; /* 'CG41' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_DamageBlendBlob");
	}

	// build the render lists
	if ( !cg.hyperspace ) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_AddPacketEntities");
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( stefxHardwareFirstFrame )
		{
			XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame packet entities begin" );
		}
#endif
		g_SPXBPhaseLast = 0x45464530; /* 'EFE0' */
#if STEFX_COOP_CGAME_DETAIL
		CG_STEFX_BeginPlayerProfile();
#endif
		CG_AddPacketEntities();			// adter calcViewValues, so predicted player state is correct
#if STEFX_COOP_CGAME_DETAIL
		CG_STEFX_EndPlayerProfile();
#endif
		g_SPXBClTailStage = 0x43473432; /* 'CG42' */
		g_SPXBPhaseLast = 0x45464531; /* 'EFE1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_AddPacketEntities");
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( stefxHardwareFirstFrame )
		{
			XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame packet entities complete" );
		}
#endif
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_AddMarks");
		CG_AddMarks();
		g_SPXBClTailStage = 0x43473433; /* 'CG43' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_AddMarks");
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_AddLocalEntities");
		CG_AddLocalEntities();
		g_SPXBClTailStage = 0x43473434; /* 'CG44' */
		g_SPXBClTailStage = 0x43473034; /* 'CG04' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_AddLocalEntities");
	}
#ifdef _XBOX
	xboxProfileEntities = cgi_Milliseconds() - xboxProfileMark;
	xboxProfileMark = cgi_Milliseconds();
#if STEFX_COOP_CGAME_DETAIL
	xboxTailPartStart = xboxProfileMark;
#endif
#endif

	// Don't draw the in-view weapon when in camera mode
#ifdef _XBOX
	{
		static int s_stefxViewWeaponDecisionLogCount = 0;
		const qboolean stefxSuppressPlayerPresentation = STEFX_XboxSuppressPlayerPresentation();
		if (s_stefxViewWeaponDecisionLogCount < 8)
		{
			XBLF("STEFX: CG_ViewWeapon decision inCamera=%d playerLocked=%d suppress=%d pano=%d snapWeapon=%d predictedWeapon=%d third=%d drawGun=%d zoomed=%d testGun=%d scrollTime=%d captionTime=%d gameTextTime=%d gameTextUntil=%d lcarsUntil=%d",
				in_camera ? 1 : 0,
				player_locked ? 1 : 0,
				stefxSuppressPlayerPresentation ? 1 : 0,
				cg_pano.integer,
				cg.snap ? cg.snap->ps.weapon : -1,
				cg.predicted_player_state.weapon,
				cg.renderingThirdPerson ? 1 : 0,
				cg_drawGun.integer,
				cg.zoomed ? 1 : 0,
				cg.testGun ? 1 : 0,
				cg.scrollTextTime,
				cg.captionTextTime,
				cg.gameTextTime,
				cg.gameNextTextTime,
				cg.LCARSTextTime);
			if (s_stefxViewWeaponDecisionLogCount < 160)
			{
				++s_stefxViewWeaponDecisionLogCount;
			}
		}
	}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( !STEFX_XboxSuppressPlayerPresentation() && !cg_pano.integer )
#else
	if ( !in_camera && !cg_pano.integer )
#endif
	{
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_AddViewWeapon");
		g_SPXBPhaseLast = 0x45465730; /* 'EFW0' */
		CG_AddViewWeapon( &cg.predicted_player_state );
		g_SPXBPhaseLast = 0x45465731; /* 'EFW1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_AddViewWeapon");
	}

	// finish up the rest of the refdef
#if STEFX_COOP_CGAME_DETAIL
	xboxTailParts[0] = cgi_Milliseconds() - xboxTailPartStart;
	xboxTailPartStart = cgi_Milliseconds();
#endif
	if ( cg.testModelEntity.hModel ) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_AddTestModel");
		CG_AddTestModel();
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_AddTestModel");
	}
	
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before refdef finalize");
	cg.refdef.time = cg.time;
	memcpy( cg.refdef.areamask, cg.snap->areamask, sizeof( cg.refdef.areamask ) );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( in_camera )
	{
		static int s_stefxCameraPvsLogs = 0;
		CM_SnapPVS( cg.refdef.vieworg, cg.refdef.areamask );
		if ( s_stefxCameraPvsLogs < 12 )
		{
			XBLF("STEFX: CG camera SnapPVS time=%d view=(%g,%g,%g) rdflags=0x%x fov=(%g,%g)",
				cg.time,
				cg.refdef.vieworg[0], cg.refdef.vieworg[1], cg.refdef.vieworg[2],
				cg.refdef.rdflags,
				cg.refdef.fov_x, cg.refdef.fov_y);
			++s_stefxCameraPvsLogs;
		}
	}
	CG_STEFX_UpdateSplitP2Refdef();
#endif
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after refdef finalize");

	// update audio positions
#if STEFX_COOP_CGAME_DETAIL
	xboxTailParts[1] = cgi_Milliseconds() - xboxTailPartStart;
	xboxTailPartStart = cgi_Milliseconds();
#endif
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before S_Respatialize");
	cgi_S_Respatialize( cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater );
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after S_Respatialize");

	// warning sounds when powerup is wearing off
#if STEFX_COOP_CGAME_DETAIL
	xboxTailParts[2] = cgi_Milliseconds() - xboxTailPartStart;
	xboxTailPartStart = cgi_Milliseconds();
#endif
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_PowerupTimerSounds");
	CG_PowerupTimerSounds();
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_PowerupTimerSounds");

	// make sure the lagometerSample and frame timing isn't done twice when in stereo
#if STEFX_COOP_CGAME_DETAIL
	xboxTailParts[3] = cgi_Milliseconds() - xboxTailPartStart;
#endif
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before frametime");
	if ( stereoView != STEREO_RIGHT ) {
		cg.frametime = cg.time - cg.oldTime;
		cg.oldTime = cg.time;
	}
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after frametime");

	//Add all effects
#if STEFX_COOP_CGAME_DETAIL
	xboxTailPartStart = cgi_Milliseconds();
#endif
	if (cg.frametime >= 0) {
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before FX_Add");
		g_SPXBPhaseLast = 0x45465430; /* 'EFT0' */
		FX_Add();
		g_SPXBClTailStage = 0x43473035; /* 'CG05' */
		g_SPXBPhaseLast = 0x45465431; /* 'EFT1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after FX_Add");
	}
#ifdef _XBOX
	xboxProfileTail = cgi_Milliseconds() - xboxProfileMark;
#if STEFX_COOP_CGAME_DETAIL
	xboxTailParts[4] = cgi_Milliseconds() - xboxTailPartStart;
	xboxTailParts[5] = xboxProfileTail;
	for (int tailPart = 0; tailPart < 5; ++tailPart) xboxTailParts[5] -= xboxTailParts[tailPart];
	if (xboxTailParts[5] < 0) xboxTailParts[5] = 0; // millisecond rounding
#endif
	xboxProfileMark = cgi_Milliseconds();
#endif

	if ( cg_pano.integer ) {	// let's grab a panorama!
		cg.levelShot = qtrue;  //hide the 2d
		VectorClear(cg.refdefViewAngles);		
		cg.refdefViewAngles[YAW] = -360 * cg_pano.integer/cg_panoNumShots.integer;	//choose angle
		AnglesToAxis( cg.refdefViewAngles, cg.refdef.viewaxis );
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_DrawActive pano");
		g_SPXBPhaseLast = 0x45465230; /* 'EFR0' */
		CG_DrawActive( stereoView );
		g_SPXBClTailStage = 0x43473037; /* 'CG07' */
		g_SPXBPhaseLast = 0x45465231; /* 'EFR1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_DrawActive pano");
		cg.levelShot = qfalse;
	} 	else {
		// actually issue the rendering calls
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame before CG_DrawActive");
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( stefxHardwareFirstFrame )
		{
			XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame render begin" );
		}
#endif
		g_SPXBPhaseLast = 0x45465230; /* 'EFR0' */
		CG_DrawActive( stereoView );
		g_SPXBClTailStage = 0x43473037; /* 'CG07' */
		g_SPXBPhaseLast = 0x45465231; /* 'EFR1' */
		CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame after CG_DrawActive");
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( stefxHardwareFirstFrame )
		{
			XBLog_WriteCritical( "STEFX_HW_CHECKPOINT: first frame render complete" );
		}
#endif
	}
#ifdef _XBOX
	xboxProfileDraw = cgi_Milliseconds() - xboxProfileMark;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	if (cg_stefxSplitScreen.integer && cg_stefxSplitScreenPlayers.integer >= 2) {
		// Publish the existing phase timers together after both HUD passes.
		// An external reader rejects odd/changing sequence values instead of
		// combining timings or entity counts from different client frames.
		++g_SPXBCoopCgamePhasesV2[0];
		g_SPXBCoopCgamePhasesV2[1] = cg.clientFrame;
		g_SPXBCoopCgamePhasesV2[2] = serverTime;
		g_SPXBCoopCgamePhasesV2[3] = cgi_Milliseconds() - xboxProfileStart;
		g_SPXBCoopCgamePhasesV2[4] = xboxProfileSetup;
		g_SPXBCoopCgamePhasesV2[5] = xboxProfilePredict;
		g_SPXBCoopCgamePhasesV2[6] = xboxProfileView;
		g_SPXBCoopCgamePhasesV2[7] = xboxProfileEntities;
		g_SPXBCoopCgamePhasesV2[8] = xboxProfileTail;
		g_SPXBCoopCgamePhasesV2[9] = xboxProfileDraw;
		g_SPXBCoopCgamePhasesV2[10] = g_stefxCgRenderSceneMsec;
		g_SPXBCoopCgamePhasesV2[11] = g_stefxCgDraw2DMsec;
		g_SPXBCoopCgamePhasesV2[12] = g_SPXBMainLoopCount;
		for (int slot = 0; slot < 16; ++slot) {
			g_SPXBCoopCgamePhasesV2[13+slot] = g_SPXBCgEntTypeCycles[slot];
			g_SPXBCoopCgamePhasesV2[29+slot] = g_SPXBCgEntTypeCounts[slot];
		}
		for (int part = 0; part < 6; ++part) g_SPXBCoopCgamePhasesV2[45+part] = xboxTailParts[part];
		++g_SPXBCoopCgamePhasesV2[0];
	}
#endif
	if (serverTime - s_xboxLastProfileTime >= 10000)
	{
		XBLog_WriteCriticalf(
			"STEFX_HW_PROFILE: cgame total=%d setup=%d predict=%d view=%d entities=%d tail=%d draw=%d scene=%d hud=%d entCounts=%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u entKcy=%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u plSkip=%u serverTime=%d frame=%d",
			cgi_Milliseconds() - xboxProfileStart,
			xboxProfileSetup,
			xboxProfilePredict,
			xboxProfileView,
			xboxProfileEntities,
			xboxProfileTail,
			xboxProfileDraw,
			g_stefxCgRenderSceneMsec,
			g_stefxCgDraw2DMsec,
			g_SPXBCgEntTypeCounts[0], g_SPXBCgEntTypeCounts[1],
			g_SPXBCgEntTypeCounts[2], g_SPXBCgEntTypeCounts[3],
			g_SPXBCgEntTypeCounts[4], g_SPXBCgEntTypeCounts[5],
			g_SPXBCgEntTypeCounts[6], g_SPXBCgEntTypeCounts[7],
			g_SPXBCgEntTypeCounts[8], g_SPXBCgEntTypeCounts[9],
			g_SPXBCgEntTypeCounts[10], g_SPXBCgEntTypeCounts[11],
			g_SPXBCgEntTypeCycles[0] / 1000u, g_SPXBCgEntTypeCycles[1] / 1000u,
			g_SPXBCgEntTypeCycles[2] / 1000u, g_SPXBCgEntTypeCycles[3] / 1000u,
			g_SPXBCgEntTypeCycles[4] / 1000u, g_SPXBCgEntTypeCycles[5] / 1000u,
			g_SPXBCgEntTypeCycles[6] / 1000u, g_SPXBCgEntTypeCycles[7] / 1000u,
			g_SPXBCgEntTypeCycles[8] / 1000u, g_SPXBCgEntTypeCycles[9] / 1000u,
			g_SPXBCgEntTypeCycles[10] / 1000u, g_SPXBCgEntTypeCycles[11] / 1000u,
			g_SPXBCgHiddenPlayersSkipped,
			serverTime,
			cg.clientFrame);
		s_xboxLastProfileTime = serverTime;
	}
#endif
	CG_XBOX_ACTIVE_LOG("JA: CL_EARLY EF CG_DrawActiveFrame done");
}

#ifdef _XBOX
#undef CG_XBOX_ACTIVE_LOG
#endif
