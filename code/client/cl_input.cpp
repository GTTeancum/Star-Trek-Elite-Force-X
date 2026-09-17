// cl.input.c  -- builds an intended movement command to send to the server

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"


#include "client.h"
#include "client_ui.h"
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
#include "../win32/xb_log.h"
#include "../win32/win_input.h"
#endif

extern "C" volatile unsigned int g_SPXBUsercmdCount;
extern "C" volatile unsigned int g_SPXBUsercmdTime;
extern "C" volatile unsigned int g_SPXBUsercmdMove;
extern "C" volatile unsigned int g_SPXBUsercmdButtons;
extern "C" volatile unsigned int g_SPXBUsercmdYaw;

#ifdef _XBOX
#include "cl_input_hotswap.h"
#endif

unsigned	frame_msec;
int			old_com_frameTime;
float cl_mPitchOverride = 0.0f;
float cl_mYawOverride = 0.0f;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as argv(1) so it can be matched up with the release.

argv(2) will be set to the time the event happened, which allows exact
control even at low framerates when the down and up events may both get qued
at the same time.

===============================================================================
*/


kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed;
kbutton_t	in_up, in_down;

kbutton_t	in_buttons[9];


qboolean	in_mlooking;


#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static cvar_t *stefx_smokeInput;
static cvar_t *stefx_smokeInputStart;
static cvar_t *stefx_smokeInputEnd;
static cvar_t *stefx_smokeInputForward;
static cvar_t *stefx_smokeInputSide;
static cvar_t *stefx_smokeInputYaw;
static cvar_t *stefx_smokeViewPitch;
static cvar_t *stefx_smokeViewYaw;
static cvar_t *stefx_smokeInputAttackStart;
static cvar_t *stefx_smokeInputAttackEnd;
static cvar_t *stefx_smokeInputAltAttackStart;
static cvar_t *stefx_smokeInputAltAttackEnd;

#define STEFX_SPLIT_MAX_PADS 4
#define STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD 30
#define STEFX_SPLIT_THUMB_DEADZONE 7849
#define STEFX_SPLIT_BUTTON_DPAD_UP (1u << 0)
#define STEFX_SPLIT_BUTTON_DPAD_DOWN (1u << 1)
#define STEFX_SPLIT_BUTTON_DPAD_LEFT (1u << 2)
#define STEFX_SPLIT_BUTTON_DPAD_RIGHT (1u << 3)
#define STEFX_SPLIT_BUTTON_START (1u << 4)
#define STEFX_SPLIT_BUTTON_BACK (1u << 5)
#define STEFX_SPLIT_BUTTON_LEFT_THUMB (1u << 6)
#define STEFX_SPLIT_BUTTON_RIGHT_THUMB (1u << 7)

typedef struct {
	qboolean connected;
	int mainController;
	unsigned int buttons;
	byte analogButtons[8];
	int thumbLX;
	int thumbLY;
	int thumbRX;
	int thumbRY;
	int lastServerTime;
	vec3_t viewangles;
	qboolean viewanglesValid;
	qboolean weaponNextDown;
	qboolean weaponPrevDown;
	qboolean zoomDown;
	qboolean datapadDown;
	qboolean pauseDown;
	qboolean thirdPersonToggleDown;
	qboolean runToggleDown;
	qboolean runEnabled;
	qboolean attackDown;
	float holomatchZoomFov;
	qboolean holomatchZoomValid;
	qboolean synthetic;
} stefxSplitPadState_t;

static stefxSplitPadState_t s_stefxSplitPads[STEFX_SPLIT_MAX_PADS];
static int s_stefxSplitPrimaryPad = -1;
static int s_stefxSplitSecondaryPad = -1;
static qboolean s_stefxSplitRecordingSyntheticPad = qfalse;

static qboolean STEFX_SplitMainControllerValid( int mainController )
{
	return (qboolean)( mainController >= 0 && mainController < STEFX_SPLIT_MAX_PADS );
}

static int STEFX_SplitEffectiveMainController( int rawMainController, qboolean includeSynthetic )
{
	static int s_invalidMainLogBudget = 16;
	int port;
	int firstConnected = -1;
	int firstRealConnected = -1;
	int storedMainController = -1;
	int effectiveMain = -1;
	int mask = 0;
	qboolean rawMainConnected = qfalse;
	qboolean syntheticConnected = qfalse;

	for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
	{
		if ( !s_stefxSplitPads[port].connected
			|| ( !includeSynthetic && s_stefxSplitPads[port].synthetic ) )
		{
			continue;
		}
		mask |= 1 << port;
		if ( firstConnected < 0 )
		{
			firstConnected = port;
		}
		if ( !s_stefxSplitPads[port].synthetic && firstRealConnected < 0 )
		{
			firstRealConnected = port;
		}
		if ( s_stefxSplitPads[port].synthetic )
		{
			syntheticConnected = qtrue;
		}
		if ( port == rawMainController )
		{
			rawMainConnected = qtrue;
		}
		if ( STEFX_SplitMainControllerValid( s_stefxSplitPads[port].mainController ) )
		{
			storedMainController = s_stefxSplitPads[port].mainController;
		}
	}

	if ( STEFX_SplitMainControllerValid( rawMainController ) && rawMainConnected )
	{
		effectiveMain = rawMainController;
	}
	else if ( STEFX_SplitMainControllerValid( storedMainController )
		&& s_stefxSplitPads[storedMainController].connected
		&& ( includeSynthetic || !s_stefxSplitPads[storedMainController].synthetic ) )
	{
		effectiveMain = storedMainController;
	}
	else if ( includeSynthetic
		&& syntheticConnected
		&& firstRealConnected < 0
		&& STEFX_SplitMainControllerValid( rawMainController ) )
	{
		effectiveMain = rawMainController;
	}
	else if ( firstRealConnected >= 0 )
	{
		effectiveMain = firstRealConnected;
	}
	else if ( firstConnected >= 0 )
	{
		effectiveMain = firstConnected;
	}
	else if ( !STEFX_SplitMainControllerValid( effectiveMain ) )
	{
		effectiveMain = 0;
	}

	if ( s_invalidMainLogBudget > 0
		&& Cvar_VariableIntegerValue( "stefx_splitScreen" )
		&& ( rawMainController != effectiveMain
			|| !STEFX_SplitMainControllerValid( rawMainController )
			|| !rawMainConnected ) )
	{
		XBLF( "STEFX_SPLIT_INPUT effective main raw=%d effective=%d mask=0x%x includeSynthetic=%d",
			rawMainController,
			effectiveMain,
			mask,
			includeSynthetic ? 1 : 0 );
		--s_invalidMainLogBudget;
	}

	return effectiveMain;
}

static int STEFX_SplitRealConnectedPadCount( void )
{
	int port;
	int count = 0;

	for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
	{
		if ( s_stefxSplitPads[port].connected && !s_stefxSplitPads[port].synthetic )
		{
			++count;
		}
	}

	return count;
}

static qboolean STEFX_SplitPadHasMeaningfulInput( unsigned int buttons, const byte *analogButtons, int thumbLX, int thumbLY, int thumbRX, int thumbRY )
{
	int i;

	if ( buttons )
	{
		return qtrue;
	}
	if ( analogButtons )
	{
		for ( i = 0; i < 8; ++i )
		{
			if ( analogButtons[i] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
			{
				return qtrue;
			}
		}
	}
	if ( thumbLX <= -STEFX_SPLIT_THUMB_DEADZONE || thumbLX >= STEFX_SPLIT_THUMB_DEADZONE ||
		thumbLY <= -STEFX_SPLIT_THUMB_DEADZONE || thumbLY >= STEFX_SPLIT_THUMB_DEADZONE ||
		thumbRX <= -STEFX_SPLIT_THUMB_DEADZONE || thumbRX >= STEFX_SPLIT_THUMB_DEADZONE ||
		thumbRY <= -STEFX_SPLIT_THUMB_DEADZONE || thumbRY >= STEFX_SPLIT_THUMB_DEADZONE )
	{
		return qtrue;
	}
	return qfalse;
}

static qboolean STEFX_SplitPadUsableForRealInput( int port )
{
	return (qboolean)(
		port >= 0 &&
		port < STEFX_SPLIT_MAX_PADS &&
		s_stefxSplitPads[port].connected &&
		!s_stefxSplitPads[port].synthetic );
}

static int STEFX_SplitRealPadForLocalSlot( int localSlot )
{
	int port;
	int rawMainController = -1;
	int effectiveMainController;
	int candidateSlot = 1;

	if ( localSlot < 0 || localSlot >= STEFX_SPLIT_MAX_PADS )
	{
		return -1;
	}
	for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
	{
		if ( STEFX_SplitPadUsableForRealInput( port ) )
		{
			rawMainController = s_stefxSplitPads[port].mainController;
		}
	}
	effectiveMainController = STEFX_SplitEffectiveMainController( rawMainController, qfalse );
	if ( localSlot == 0 )
	{
		return STEFX_SplitPadUsableForRealInput( effectiveMainController ) ? effectiveMainController : -1;
	}
	for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
	{
		if ( !STEFX_SplitPadUsableForRealInput( port ) || port == effectiveMainController )
		{
			continue;
		}
		if ( candidateSlot == localSlot )
		{
			return port;
		}
		++candidateSlot;
	}
	return -1;
}

int CL_STEFX_SplitScreen_LocalSlotForPad( int port )
{
	int localSlot;

	if ( port < 0 || port >= STEFX_SPLIT_MAX_PADS )
	{
		return -1;
	}
	for ( localSlot = 0; localSlot < STEFX_SPLIT_MAX_PADS; ++localSlot )
	{
		if ( STEFX_SplitRealPadForLocalSlot( localSlot ) == port )
		{
			return localSlot;
		}
	}
	return -1;
}

int CL_STEFX_SplitScreen_PadForLocalSlot( int localSlot )
{
	return STEFX_SplitRealPadForLocalSlot( localSlot );
}

static void STEFX_SplitReleaseObservedPad( int port )
{
	static int s_releaseLogBudget = 16;

	if ( port < 0 || port >= STEFX_SPLIT_MAX_PADS )
	{
		return;
	}

	if ( s_stefxSplitPrimaryPad == port )
	{
		s_stefxSplitPrimaryPad = STEFX_SplitPadUsableForRealInput( s_stefxSplitSecondaryPad ) ? s_stefxSplitSecondaryPad : -1;
		s_stefxSplitSecondaryPad = -1;
		if ( s_releaseLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
		{
			XBLF( "STEFX_SPLIT_INPUT ownership primary released port=%d promoted=%d",
				port,
				s_stefxSplitPrimaryPad );
			--s_releaseLogBudget;
		}
	}
	else if ( s_stefxSplitSecondaryPad == port )
	{
		s_stefxSplitSecondaryPad = -1;
		if ( s_releaseLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
		{
			XBLF( "STEFX_SPLIT_INPUT ownership secondary released port=%d primary=%d",
				port,
				s_stefxSplitPrimaryPad );
			--s_releaseLogBudget;
		}
	}
}

static void STEFX_SplitObserveRealPadActivity( int port, unsigned int buttons, const byte *analogButtons, int thumbLX, int thumbLY, int thumbRX, int thumbRY )
{
	static int s_claimLogBudget = 32;

	if ( !Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		s_stefxSplitPrimaryPad = -1;
		s_stefxSplitSecondaryPad = -1;
		return;
	}
	if ( port < 0 || port >= STEFX_SPLIT_MAX_PADS || s_stefxSplitPads[port].synthetic || s_stefxSplitRecordingSyntheticPad )
	{
		return;
	}
	if ( !STEFX_SplitPadHasMeaningfulInput( buttons, analogButtons, thumbLX, thumbLY, thumbRX, thumbRY ) )
	{
		return;
	}

	if ( !STEFX_SplitPadUsableForRealInput( s_stefxSplitPrimaryPad ) )
	{
		s_stefxSplitPrimaryPad = port;
		if ( s_stefxSplitSecondaryPad == port )
		{
			s_stefxSplitSecondaryPad = -1;
		}
		if ( s_claimLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_INPUT ownership primary claimed port=%d buttons=0x%x LX=%d LY=%d RX=%d RY=%d",
				port,
				buttons,
				thumbLX,
				thumbLY,
				thumbRX,
				thumbRY );
			--s_claimLogBudget;
		}
		return;
	}

	if ( port != s_stefxSplitPrimaryPad && !STEFX_SplitPadUsableForRealInput( s_stefxSplitSecondaryPad ) )
	{
		s_stefxSplitSecondaryPad = port;
		if ( s_claimLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_INPUT ownership secondary claimed port=%d primary=%d buttons=0x%x LX=%d LY=%d RX=%d RY=%d",
				port,
				s_stefxSplitPrimaryPad,
				buttons,
				thumbLX,
				thumbLY,
				thumbRX,
				thumbRY );
			--s_claimLogBudget;
		}
	}
}

static float STEFX_SplitNormalizeThumb( int value )
{
	float normalized;

	if ( value > -STEFX_SPLIT_THUMB_DEADZONE && value < STEFX_SPLIT_THUMB_DEADZONE )
	{
		return 0.0f;
	}

	normalized = value / 32767.0f;
	if ( normalized > 1.0f )
	{
		normalized = 1.0f;
	}
	else if ( normalized < -1.0f )
	{
		normalized = -1.0f;
	}
	return normalized;
}

static signed char STEFX_SplitAxisToMove( float value )
{
	int move = (int)( value * 127.0f );

	if ( move > 127 )
	{
		move = 127;
	}
	else if ( move < -127 )
	{
		move = -127;
	}

	return (signed char)move;
}

static void STEFX_SplitApplyRunState( stefxSplitPadState_t *pad, usercmd_t *cmd )
{
	int maxComponent;
	int sideComponent;

	if ( !pad || !cmd )
	{
		return;
	}

	if ( !pad->runEnabled && ( cmd->forwardmove || cmd->rightmove ) )
	{
		maxComponent = abs( cmd->forwardmove );
		sideComponent = abs( cmd->rightmove );
		if ( sideComponent > maxComponent )
		{
			maxComponent = sideComponent;
		}
		if ( maxComponent > 64 )
		{
			cmd->forwardmove = ClampChar( ( cmd->forwardmove * 64 ) / maxComponent );
			cmd->rightmove = ClampChar( ( cmd->rightmove * 64 ) / maxComponent );
		}
		cmd->buttons |= BUTTON_WALKING;
		return;
	}

	if ( ( cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove ) < ( MOVE_RUN * MOVE_RUN ) )
	{
		cmd->buttons |= BUTTON_WALKING;
	}
}

static qboolean STEFX_SplitRealSecondaryPadConnected( void )
{
	return STEFX_SplitPadUsableForRealInput( s_stefxSplitSecondaryPad );
}

static void STEFX_SplitScreen_UpdateTestP2Pad( int serverTime )
{
	static qboolean s_fakePadConnected = qfalse;
	static int s_fakePadLogBudget = 32;
	byte analogButtons[8];
	unsigned int buttons = 0;
	int phase;
	const int fakePort = 3;
	const int testMode = Cvar_VariableIntegerValue( "stefx_splitScreenTestP2Pad" );

	if ( !testMode || STEFX_SplitRealSecondaryPadConnected() )
	{
		if ( s_fakePadConnected )
		{
			if ( s_stefxSplitPads[fakePort].synthetic )
			{
				CL_STEFX_SplitScreen_RecordPadState( fakePort, qfalse, 0, 0, NULL, 0, 0, 0, 0 );
				s_stefxSplitPads[fakePort].synthetic = qfalse;
			}
			else if ( s_fakePadLogBudget > 0 )
			{
				XBLF( "STEFX_SPLIT_INPUT fakepad release skipped port=%d reason='real pad owns state'",
					fakePort );
				--s_fakePadLogBudget;
			}
			s_fakePadConnected = qfalse;
		}
		return;
	}

	memset( analogButtons, 0, sizeof( analogButtons ) );
	phase = ( serverTime / 700 ) & 3;

	if ( testMode == 2 )
	{
		if ( serverTime >= 2000 && serverTime < 2300 )
		{
			buttons |= STEFX_SPLIT_BUTTON_START; /* Start: test P2 full-screen pause overlay */
		}
	}
	else
	{
		analogButtons[7] = (byte)( ( ( serverTime / 500 ) & 1 ) ? 90 : 0 ); /* Right trigger: attack */
		analogButtons[2] = (byte)( ( ( serverTime / 1600 ) & 3 ) == 1 ? 90 : 0 ); /* X: use */
		analogButtons[3] = (byte)( ( serverTime >= 1900 && serverTime < 2000 ) ? 90 : 0 ); /* Y: centerview */
		analogButtons[5] = (byte)( ( ( serverTime / 1200 ) & 1 ) ? 90 : 0 ); /* White: next weapon */

		if ( ( ( serverTime / 900 ) & 3 ) == 2 )
		{
			buttons |= STEFX_SPLIT_BUTTON_DPAD_UP; /* D-pad up: P2-local zoom */
		}
		if ( serverTime >= 2300 && serverTime < 2400 )
		{
			buttons |= STEFX_SPLIT_BUTTON_LEFT_THUMB; /* Left stick click: P2-local run toggle */
		}
		if ( serverTime >= 2000 && serverTime < 2300 )
		{
			buttons |= STEFX_SPLIT_BUTTON_BACK; /* Back: test P2 full-screen objectives overlay */
		}
		if ( serverTime >= 2200 && serverTime < 2300 )
		{
			buttons |= STEFX_SPLIT_BUTTON_RIGHT_THUMB; /* Right stick click: split keeps both bodies visible */
		}
	}

	s_stefxSplitRecordingSyntheticPad = qtrue;
	CL_STEFX_SplitScreen_RecordPadState(
		fakePort,
		qtrue,
		0,
		buttons,
		analogButtons,
		phase == 1 ? 12000 : ( phase == 3 ? -12000 : 0 ),
		22000,
		9000,
		0 );
	s_stefxSplitRecordingSyntheticPad = qfalse;
	s_stefxSplitPads[fakePort].synthetic = qtrue;
	s_fakePadConnected = qtrue;

	if ( s_fakePadLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_INPUT fakepad mode=%d port=%d time=%d buttons=0x%x A=%u B=%u X=%u Y=%u LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d",
			testMode,
			fakePort,
			serverTime,
			buttons,
			analogButtons[0],
			analogButtons[1],
			analogButtons[2],
			analogButtons[3],
			analogButtons[6],
			analogButtons[7],
			phase == 1 ? 12000 : ( phase == 3 ? -12000 : 0 ),
			22000,
			9000,
			0 );
		--s_fakePadLogBudget;
	}
}

static qboolean STEFX_SplitScreen_BuildTestP2Usercmd( usercmd_t *cmd, const vec3_t currentAngles, const int deltaAngles[3], int serverTime, int *sourcePort, int *weaponDelta, vec3_t outAngles )
{
	static int s_testLogBudget = 32;
	static qboolean s_testAnglesValid = qfalse;
	static int s_testLastServerTime = 0;
	static int s_testLastWeaponBucket = -1;
	static vec3_t s_testAngles;
	int testMode;
	int frameMsec;
	int phase;
	int weaponBucket;

	testMode = Cvar_VariableIntegerValue( "stefx_splitScreenTestP2Input" );
	if ( !testMode )
	{
		return qfalse;
	}

#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	// Diagnostic controls must yield to the campaign's authored presentation.
	// Real controller behavior and ICARUS state are untouched by this fixture.
	extern bool in_camera;
	extern qboolean player_locked;
	if ( in_camera || player_locked )
	{
		memset( cmd, 0, sizeof( *cmd ) );
		cmd->serverTime = serverTime;
		for ( int axis = 0; axis < 3; ++axis )
		{
			cmd->angles[axis] = ANGLE2SHORT( currentAngles[axis] ) - ( deltaAngles ? deltaAngles[axis] : 0 );
		}
		if ( outAngles ) VectorCopy( currentAngles, outAngles );
		if ( sourcePort ) *sourcePort = -2;
		if ( weaponDelta ) *weaponDelta = 0;
		s_testAnglesValid = qfalse;
		s_testLastServerTime = serverTime;
		return qtrue;
	}
#endif

	if ( !s_testAnglesValid )
	{
		VectorCopy( currentAngles, s_testAngles );
		s_testAnglesValid = qtrue;
		s_testLastServerTime = serverTime;
	}

	frameMsec = serverTime - s_testLastServerTime;
	if ( frameMsec < 0 || frameMsec > 100 )
	{
		frameMsec = 16;
	}
	else if ( frameMsec == 0 )
	{
		frameMsec = 1;
	}
	s_testLastServerTime = serverTime;

	s_testAngles[YAW] = AngleNormalize360( s_testAngles[YAW] + 70.0f * ( frameMsec / 1000.0f ) );
	s_testAngles[PITCH] = 0.0f;
	s_testAngles[ROLL] = 0.0f;

	phase = ( serverTime / 700 ) & 3;
	memset( cmd, 0, sizeof( *cmd ) );
	cmd->serverTime = serverTime;
	cmd->forwardmove = 72;
	if ( phase == 1 )
	{
		cmd->rightmove = 48;
	}
	else if ( phase == 3 )
	{
		cmd->rightmove = -48;
	}

	cmd->angles[PITCH] = ANGLE2SHORT( s_testAngles[PITCH] ) - ( deltaAngles ? deltaAngles[PITCH] : 0 );
	cmd->angles[YAW] = ANGLE2SHORT( s_testAngles[YAW] ) - ( deltaAngles ? deltaAngles[YAW] : 0 );
	cmd->angles[ROLL] = 0;

	// Opt-in combat fixture: mode 3 extends movement/weapon cycling with
	// primary-fire bursts. Campaign locks above still force neutral input.
	if ( testMode == 3 && phase != 3 )
	{
		cmd->buttons |= BUTTON_ATTACK;
	}

	if ( outAngles )
	{
		VectorCopy( s_testAngles, outAngles );
	}
	if ( sourcePort )
	{
		*sourcePort = -2;
	}
	if ( weaponDelta )
	{
		*weaponDelta = 0;
		if ( testMode >= 2 )
		{
			weaponBucket = serverTime / 1200;
			if ( weaponBucket != s_testLastWeaponBucket )
			{
				*weaponDelta = 1;
				s_testLastWeaponBucket = weaponBucket;
			}
		}
	}

	if ( s_testLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_INPUT testcmd mode=%d time=%d move=(%d,%d,%d) buttons=0x%x weaponDelta=%d view=(%g,%g,%g)",
			testMode,
			serverTime,
			cmd->forwardmove,
			cmd->rightmove,
			cmd->upmove,
			cmd->buttons,
			weaponDelta ? *weaponDelta : 0,
			s_testAngles[PITCH],
			s_testAngles[YAW],
			s_testAngles[ROLL] );
		--s_testLogBudget;
	}

	return qtrue;
}

void CL_STEFX_SplitScreen_RecordPadState( int port, qboolean connected, int mainController, unsigned int buttons, const byte *analogButtons, int thumbLX, int thumbLY, int thumbRX, int thumbRY )
{
	static int s_recordLogBudget = 48;
	static qboolean s_lastConnected[STEFX_SPLIT_MAX_PADS] = { qfalse, qfalse, qfalse, qfalse };
	static int s_lastMainController[STEFX_SPLIT_MAX_PADS] = { -2, -2, -2, -2 };
	stefxSplitPadState_t *pad;

	if ( port < 0 || port >= STEFX_SPLIT_MAX_PADS )
	{
		return;
	}

	pad = &s_stefxSplitPads[port];
	if ( s_recordLogBudget > 0 &&
		( s_lastConnected[port] != connected || s_lastMainController[port] != mainController ) )
	{
		XBLF( "STEFX_SPLIT_INPUT padstate port=%d connected=%d main=%d buttons=0x%x rawLX=%d rawLY=%d rawRX=%d rawRY=%d",
			port,
			connected ? 1 : 0,
			mainController,
			buttons,
			thumbLX,
			thumbLY,
			thumbRX,
			thumbRY );
		--s_recordLogBudget;
	}
	s_lastConnected[port] = connected;
	s_lastMainController[port] = mainController;

	pad->connected = connected;
	pad->mainController = mainController;

	if ( !connected )
	{
		pad->buttons = 0;
		memset( pad->analogButtons, 0, sizeof( pad->analogButtons ) );
		pad->thumbLX = pad->thumbLY = pad->thumbRX = pad->thumbRY = 0;
		pad->viewanglesValid = qfalse;
		pad->weaponNextDown = qfalse;
		pad->weaponPrevDown = qfalse;
		if ( pad->zoomDown )
		{
			Cvar_Set( "stefx_splitScreenP2Zoom", "0" );
		}
		pad->zoomDown = qfalse;
		pad->datapadDown = qfalse;
		pad->thirdPersonToggleDown = qfalse;
		pad->runToggleDown = qfalse;
		pad->runEnabled = qfalse;
		pad->attackDown = qfalse;
		pad->holomatchZoomFov = 90.0f;
		pad->holomatchZoomValid = qfalse;
		pad->synthetic = qfalse;
		STEFX_SplitReleaseObservedPad( port );
		return;
	}

	pad->buttons = buttons;
	pad->synthetic = s_stefxSplitRecordingSyntheticPad;
	if ( analogButtons )
	{
		memcpy( pad->analogButtons, analogButtons, sizeof( pad->analogButtons ) );
	}
	else
	{
		memset( pad->analogButtons, 0, sizeof( pad->analogButtons ) );
	}
	pad->thumbLX = thumbLX;
	pad->thumbLY = thumbLY;
	pad->thumbRX = thumbRX;
	pad->thumbRY = thumbRY;
	STEFX_SplitObserveRealPadActivity( port, buttons, analogButtons, thumbLX, thumbLY, thumbRX, thumbRY );
}

int CL_STEFX_SplitScreen_PrimaryPadForMainController( void )
{
	if ( !Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		return -1;
	}
	if ( STEFX_SplitPadUsableForRealInput( s_stefxSplitPrimaryPad ) )
	{
		return s_stefxSplitPrimaryPad;
	}
	return -1;
}

qboolean CL_STEFX_SplitScreen_ShouldReservePadForP2( int port, int mainController )
{
	const int effectiveMain = STEFX_SplitEffectiveMainController( mainController, qfalse );
	const int realPadCount = STEFX_SplitRealConnectedPadCount();
	static int s_singlePadLogBudget = 16;
	static int s_waitLogBudget = 16;

	if ( port < 0 || port >= STEFX_SPLIT_MAX_PADS )
	{
		return qfalse;
	}
	if ( Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) >= 3 )
	{
		int localSlot;
		const int localPlayers = Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" );
		for ( localSlot = 1; localSlot < localPlayers && localSlot < STEFX_SPLIT_MAX_PADS; ++localSlot )
		{
			if ( STEFX_SplitRealPadForLocalSlot( localSlot ) == port )
			{
				return qtrue;
			}
		}
		return qfalse;
	}
	if ( port == s_stefxSplitPrimaryPad )
	{
		return qfalse;
	}
	if ( STEFX_SplitPadUsableForRealInput( s_stefxSplitSecondaryPad ) )
	{
		return (qboolean)( port == s_stefxSplitSecondaryPad );
	}
	if ( s_waitLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		XBLF( "STEFX_SPLIT_INPUT reserve p2 suppressed port=%d main=%d effective=%d realPads=%d primary=%d secondary=%d reason='waiting for active second pad'",
			port,
			mainController,
			effectiveMain,
			realPadCount,
			s_stefxSplitPrimaryPad,
			s_stefxSplitSecondaryPad );
		--s_waitLogBudget;
	}
	if ( realPadCount < 2 )
	{
		if ( s_singlePadLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
		{
			XBLF( "STEFX_SPLIT_INPUT reserve p2 suppressed port=%d main=%d effective=%d realPads=%d reason='single real pad remains P1-owned'",
				port,
				mainController,
				effectiveMain,
				realPadCount );
			--s_singlePadLogBudget;
		}
		return qfalse;
	}
	return qfalse;
}

qboolean CL_STEFX_SplitScreen_BuildP2Usercmd( usercmd_t *cmd, const vec3_t currentAngles, const int deltaAngles[3], int serverTime, int *sourcePort, int *weaponDelta, vec3_t outAngles )
{
	static int s_noPadLogBudget = 12;
	static int s_selectLogBudget = 24;
	static int s_cmdLogBudget = 48;
	static int s_weaponLogBudget = 24;
	static int s_utilityLogBudget = 32;
	int port;
	int chosenPort = -1;
	int connectedMask = 0;
	int rawMainController = -1;
	int effectiveMainController;
	stefxSplitPadState_t *pad = NULL;
	float leftX;
	float leftY;
	float rightX;
	float rightY;
	int frameMsec;
	qboolean weaponNextDown;
	qboolean weaponPrevDown;
	qboolean runToggleDown;
	qboolean zoomDown;
	qboolean datapadDown;
	qboolean thirdPersonToggleDown;

	if ( !cmd )
	{
		return qfalse;
	}
	if ( weaponDelta )
	{
		*weaponDelta = 0;
	}
	if ( STEFX_SplitScreen_BuildTestP2Usercmd( cmd, currentAngles, deltaAngles, serverTime, sourcePort, weaponDelta, outAngles ) )
	{
		return qtrue;
	}

	STEFX_SplitScreen_UpdateTestP2Pad( serverTime );

	for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
	{
		if ( s_stefxSplitPads[port].connected )
		{
			connectedMask |= 1 << port;
			rawMainController = s_stefxSplitPads[port].mainController;
		}
	}

	effectiveMainController = STEFX_SplitEffectiveMainController( rawMainController, qtrue );
	if ( STEFX_SplitPadUsableForRealInput( s_stefxSplitSecondaryPad )
		&& s_stefxSplitSecondaryPad != s_stefxSplitPrimaryPad
		&& s_stefxSplitSecondaryPad != effectiveMainController )
	{
		chosenPort = s_stefxSplitSecondaryPad;
	}

	if ( chosenPort < 0 && Cvar_VariableIntegerValue( "stefx_splitScreenTestP2Pad" ) )
	{
		for ( port = 0; port < STEFX_SPLIT_MAX_PADS; ++port )
		{
			if ( s_stefxSplitPads[port].connected
				&& s_stefxSplitPads[port].synthetic
				&& port != effectiveMainController )
			{
				chosenPort = port;
				break;
			}
		}
	}

	if ( chosenPort < 0 )
	{
		if ( s_noPadLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
		{
			XBLF( "STEFX_SPLIT_INPUT no secondary controller mask=0x%x rawMain=%d effectiveMain=%d primary=%d secondary=%d main0=%d main1=%d main2=%d main3=%d",
				connectedMask,
				rawMainController,
				effectiveMainController,
				s_stefxSplitPrimaryPad,
				s_stefxSplitSecondaryPad,
				s_stefxSplitPads[0].mainController,
				s_stefxSplitPads[1].mainController,
				s_stefxSplitPads[2].mainController,
				s_stefxSplitPads[3].mainController );
			--s_noPadLogBudget;
		}
		return qfalse;
	}

	pad = &s_stefxSplitPads[chosenPort];
	if ( s_selectLogBudget > 0 && Cvar_VariableIntegerValue( "stefx_splitScreen" ) )
	{
		XBLF( "STEFX_SPLIT_INPUT selected secondary controller port=%d rawMain=%d effectiveMain=%d mask=0x%x buttons=0x%x A=%u B=%u X=%u Y=%u LT=%u RT=%u rawLX=%d rawLY=%d rawRX=%d rawRY=%d",
			chosenPort,
			pad->mainController,
			effectiveMainController,
			connectedMask,
			pad->buttons,
			pad->analogButtons[0],
			pad->analogButtons[1],
			pad->analogButtons[2],
			pad->analogButtons[3],
			pad->analogButtons[6],
			pad->analogButtons[7],
			pad->thumbLX,
			pad->thumbLY,
			pad->thumbRX,
			pad->thumbRY );
		--s_selectLogBudget;
	}
	if ( !pad->viewanglesValid )
	{
		VectorCopy( currentAngles, pad->viewangles );
		pad->viewanglesValid = qtrue;
		pad->lastServerTime = serverTime;
		pad->runEnabled = (qboolean)Cvar_VariableIntegerValue( "cl_run" );
	}

	frameMsec = serverTime - pad->lastServerTime;
	if ( frameMsec < 0 || frameMsec > 100 )
	{
		frameMsec = 16;
	}
	else if ( frameMsec == 0 )
	{
		frameMsec = 1;
	}
	pad->lastServerTime = serverTime;

	leftX = STEFX_SplitNormalizeThumb( pad->thumbLX );
	leftY = STEFX_SplitNormalizeThumb( pad->thumbLY );
	rightX = STEFX_SplitNormalizeThumb( pad->thumbRX );
	rightY = STEFX_SplitNormalizeThumb( pad->thumbRY );

	pad->viewangles[YAW] = AngleNormalize360( pad->viewangles[YAW] - rightX * 220.0f * ( frameMsec / 1000.0f ) );
	pad->viewangles[PITCH] = AngleNormalize180( pad->viewangles[PITCH] - rightY * 160.0f * ( frameMsec / 1000.0f ) );
	if ( pad->viewangles[PITCH] > 80.0f )
	{
		pad->viewangles[PITCH] = 80.0f;
	}
	else if ( pad->viewangles[PITCH] < -80.0f )
	{
		pad->viewangles[PITCH] = -80.0f;
	}
	pad->viewangles[ROLL] = 0.0f;

	memset( cmd, 0, sizeof( *cmd ) );
	cmd->serverTime = serverTime;
	cmd->forwardmove = STEFX_SplitAxisToMove( leftY );
	cmd->rightmove = STEFX_SplitAxisToMove( leftX );

	runToggleDown = (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_LEFT_THUMB );
	if ( runToggleDown && !pad->runToggleDown )
	{
		pad->runEnabled = (qboolean)!pad->runEnabled;
		XBLF( "STEFX_SPLIT_INPUT run toggle port=%d run=%d",
			chosenPort,
			pad->runEnabled ? 1 : 0 );
	}
	pad->runToggleDown = runToggleDown;
	STEFX_SplitApplyRunState( pad, cmd );

	zoomDown = (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_UP );
	if ( zoomDown != pad->zoomDown )
	{
		Cvar_Set( "stefx_splitScreenP2Zoom", zoomDown ? "1" : "0" );
		if ( s_utilityLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_INPUT p2 zoom %s port=%d buttons=0x%x",
				zoomDown ? "on" : "off",
				chosenPort,
				pad->buttons );
			--s_utilityLogBudget;
		}
	}
	pad->zoomDown = zoomDown;

	datapadDown = (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_BACK );
	if ( datapadDown != pad->datapadDown )
	{
		XBLF( "STEFX_SPLIT_INPUT p2 datapad before overlay call port=%d down=%d catcher=0x%x overlay='%s' paused='%s'",
			chosenPort,
			datapadDown ? 1 : 0,
			(unsigned int)cls.keyCatchers,
			Cvar_VariableString( "stefx_objectivesOverlay" ),
			Cvar_VariableString( "cl_paused" ) );
		if ( datapadDown )
		{
			CL_STEFX_SetObjectivesOverlay(
				CL_STEFX_ObjectivesOverlayActive() ? qfalse : qtrue,
				"p2-split-datapad-toggle" );
		}
		XBLF( "STEFX_SPLIT_INPUT p2 datapad after overlay call port=%d down=%d catcher=0x%x overlay='%s' paused='%s'",
			chosenPort,
			datapadDown ? 1 : 0,
			(unsigned int)cls.keyCatchers,
			Cvar_VariableString( "stefx_objectivesOverlay" ),
			Cvar_VariableString( "cl_paused" ) );
		if ( s_utilityLogBudget > 0 )
		{
			XBLF( "STEFX_SPLIT_INPUT p2 datapad edge port=%d down=%d handled='objectives-overlay'",
				chosenPort,
				datapadDown ? 1 : 0 );
			--s_utilityLogBudget;
		}
	}
	pad->datapadDown = datapadDown;

	{
		qboolean pauseDown = (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_START );
		if ( pauseDown && !pad->pauseDown )
		{
			XBLF( "STEFX_SPLIT_INPUT p2 pause requested port=%d catcher=0x%x objectives=%d paused='%s'",
				chosenPort,
				(unsigned int)cls.keyCatchers,
				CL_STEFX_ObjectivesOverlayActive() ? 1 : 0,
				Cvar_VariableString( "cl_paused" ) );
			if ( CL_STEFX_ObjectivesOverlayActive() )
			{
				CL_STEFX_SetObjectivesOverlay( qfalse, "p2-split-pause-close-objectives" );
			}
			CL_STEFX_RequestPauseMenu( "p2-split-start" );
			XBLF( "STEFX_SPLIT_INPUT p2 pause request queued port=%d catcher=0x%x paused='%s' realtime=%d",
				chosenPort,
				(unsigned int)cls.keyCatchers,
				Cvar_VariableString( "cl_paused" ),
				cls.realtime );
		}
		pad->pauseDown = pauseDown;
	}

	thirdPersonToggleDown = (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_RIGHT_THUMB );
	if ( thirdPersonToggleDown && !pad->thirdPersonToggleDown && s_utilityLogBudget > 0 )
	{
		XBLF( "STEFX_SPLIT_INPUT p2 third-person toggle requested port=%d ignored=1 reason='split-screen forces both player bodies visible'",
			chosenPort );
		--s_utilityLogBudget;
	}
	pad->thirdPersonToggleDown = thirdPersonToggleDown;

	if ( pad->analogButtons[0] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		cmd->upmove = 127;
	}
	else if ( pad->analogButtons[1] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		cmd->upmove = -127;
	}

	if ( pad->analogButtons[7] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		cmd->buttons |= BUTTON_ATTACK;
	}
	if ( pad->analogButtons[6] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		cmd->buttons |= BUTTON_ALT_ATTACK;
	}
	if ( pad->analogButtons[2] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		cmd->buttons |= BUTTON_USE;
	}

	if ( pad->analogButtons[3] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD )
	{
		pad->viewangles[PITCH] = 0.0f;
		if ( pad->zoomDown )
		{
			Cvar_Set( "stefx_splitScreenP2Zoom", "0" );
			pad->zoomDown = qfalse;
			zoomDown = qfalse;
		}
	}

	weaponPrevDown = (qboolean)( pad->analogButtons[4] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD );
	weaponNextDown = (qboolean)( pad->analogButtons[5] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD );
	if ( weaponDelta )
	{
		if ( weaponNextDown && !pad->weaponNextDown )
		{
			*weaponDelta = 1;
		}
		else if ( weaponPrevDown && !pad->weaponPrevDown )
		{
			*weaponDelta = -1;
		}
	}
	if ( s_weaponLogBudget > 0 && weaponDelta && *weaponDelta )
	{
		XBLF( "STEFX_SPLIT_INPUT weapon edge port=%d delta=%d black=%u white=%u",
			chosenPort,
			*weaponDelta,
			pad->analogButtons[4],
			pad->analogButtons[5] );
		--s_weaponLogBudget;
	}
	pad->weaponPrevDown = weaponPrevDown;
	pad->weaponNextDown = weaponNextDown;

	cmd->angles[PITCH] = ANGLE2SHORT( pad->viewangles[PITCH] ) - ( deltaAngles ? deltaAngles[PITCH] : 0 );
	cmd->angles[YAW] = ANGLE2SHORT( pad->viewangles[YAW] ) - ( deltaAngles ? deltaAngles[YAW] : 0 );
	cmd->angles[ROLL] = 0;

	if ( outAngles )
	{
		VectorCopy( pad->viewangles, outAngles );
	}
	if ( sourcePort )
	{
		*sourcePort = chosenPort;
	}

	if ( s_cmdLogBudget > 0 && ( cmd->forwardmove || cmd->rightmove || cmd->upmove || cmd->buttons || rightX || rightY || zoomDown || datapadDown || thirdPersonToggleDown ) )
	{
		XBLF( "STEFX_SPLIT_INPUT cmd port=%d time=%d move=(%d,%d,%d) buttons=0x%x run=%d zoom=%d datapad=%d tpToggle=%d view=(%g,%g,%g) rawLX=%d rawLY=%d rawRX=%d rawRY=%d",
			chosenPort,
			serverTime,
			cmd->forwardmove,
			cmd->rightmove,
			cmd->upmove,
			cmd->buttons,
			pad->runEnabled ? 1 : 0,
			zoomDown ? 1 : 0,
			datapadDown ? 1 : 0,
			thirdPersonToggleDown ? 1 : 0,
			pad->viewangles[PITCH],
			pad->viewangles[YAW],
			pad->viewangles[ROLL],
			pad->thumbLX,
			pad->thumbLY,
			pad->thumbRX,
			pad->thumbRY );
		--s_cmdLogBudget;
	}

	return qtrue;
}

enum stefxHolomatchMoveMode_t
{
	STEFX_HM_MOVE_LEFT_STICK,
	STEFX_HM_MOVE_RIGHT_STICK,
	STEFX_HM_MOVE_DPAD,
	STEFX_HM_MOVE_FACE,
	STEFX_HM_MOVE_DPAD_TANK
};

enum stefxHolomatchLookMode_t
{
	STEFX_HM_LOOK_RIGHT_STICK,
	STEFX_HM_LOOK_LEFT_STICK,
	STEFX_HM_LOOK_FACE,
	STEFX_HM_LOOK_DPAD,
	STEFX_HM_LOOK_NONE
};

enum stefxHolomatchInput_t
{
	STEFX_HM_INPUT_NONE = -1,
	STEFX_HM_INPUT_A = 0,
	STEFX_HM_INPUT_B,
	STEFX_HM_INPUT_X,
	STEFX_HM_INPUT_Y,
	STEFX_HM_INPUT_BLACK,
	STEFX_HM_INPUT_WHITE,
	STEFX_HM_INPUT_LEFT_TRIGGER,
	STEFX_HM_INPUT_RIGHT_TRIGGER,
	STEFX_HM_INPUT_DPAD_UP,
	STEFX_HM_INPUT_DPAD_DOWN,
	STEFX_HM_INPUT_DPAD_LEFT,
	STEFX_HM_INPUT_DPAD_RIGHT,
	STEFX_HM_INPUT_LEFT_THUMB,
	STEFX_HM_INPUT_RIGHT_THUMB,
	STEFX_HM_INPUT_LEFT_STICK_UP,
	STEFX_HM_INPUT_LEFT_STICK_DOWN,
	STEFX_HM_INPUT_LEFT_STICK_LEFT,
	STEFX_HM_INPUT_LEFT_STICK_RIGHT,
	STEFX_HM_INPUT_RIGHT_STICK_UP,
	STEFX_HM_INPUT_RIGHT_STICK_DOWN,
	STEFX_HM_INPUT_RIGHT_STICK_LEFT,
	STEFX_HM_INPUT_RIGHT_STICK_RIGHT
};

typedef struct
{
	int moveMode;
	int lookMode;
	int jump;
	int crouch;
	int use;
	int centerView;
	int attack;
	int altAttack;
	int weaponPrev;
	int weaponNext;
	int zoomIn;
	int zoomOut;
	int runToggle;
} stefxHolomatchControlProfile_t;

/* Xbox-native transpositions of the eight shipping PS2 preset contracts. */
static const stefxHolomatchControlProfile_t s_stefxHolomatchControlProfiles[9] =
{
	{ STEFX_HM_MOVE_LEFT_STICK, STEFX_HM_LOOK_RIGHT_STICK,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B, STEFX_HM_INPUT_X, STEFX_HM_INPUT_Y,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_DPAD_UP, STEFX_HM_INPUT_DPAD_DOWN, STEFX_HM_INPUT_LEFT_THUMB }, // Standard
	{ STEFX_HM_MOVE_RIGHT_STICK, STEFX_HM_LOOK_LEFT_STICK,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_DPAD_DOWN, STEFX_HM_INPUT_DPAD_UP,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B,
		STEFX_HM_INPUT_LEFT_THUMB, STEFX_HM_INPUT_RIGHT_THUMB,
		STEFX_HM_INPUT_Y, STEFX_HM_INPUT_X, STEFX_HM_INPUT_BLACK }, // Standard crossed
	{ STEFX_HM_MOVE_LEFT_STICK, STEFX_HM_LOOK_RIGHT_STICK,
		STEFX_HM_INPUT_Y, STEFX_HM_INPUT_LEFT_THUMB, STEFX_HM_INPUT_X, STEFX_HM_INPUT_DPAD_UP,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B, STEFX_HM_INPUT_DPAD_DOWN }, // Sniper
	{ STEFX_HM_MOVE_RIGHT_STICK, STEFX_HM_LOOK_LEFT_STICK,
		STEFX_HM_INPUT_DPAD_UP, STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_DPAD_DOWN, STEFX_HM_INPUT_Y,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B,
		STEFX_HM_INPUT_LEFT_THUMB, STEFX_HM_INPUT_RIGHT_THUMB,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER, STEFX_HM_INPUT_X }, // Sniper crossed
	{ STEFX_HM_MOVE_DPAD, STEFX_HM_LOOK_FACE,
		STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_RIGHT_STICK_DOWN, STEFX_HM_INPUT_RIGHT_STICK_UP,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_LEFT_STICK_LEFT, STEFX_HM_INPUT_LEFT_STICK_RIGHT,
		STEFX_HM_INPUT_LEFT_STICK_UP, STEFX_HM_INPUT_LEFT_STICK_DOWN,
		STEFX_HM_INPUT_RIGHT_STICK_LEFT }, // Pad mobile
	{ STEFX_HM_MOVE_FACE, STEFX_HM_LOOK_DPAD,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_LEFT_STICK_DOWN, STEFX_HM_INPUT_LEFT_STICK_UP,
		STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_RIGHT_STICK_LEFT, STEFX_HM_INPUT_RIGHT_STICK_RIGHT,
		STEFX_HM_INPUT_RIGHT_STICK_UP, STEFX_HM_INPUT_RIGHT_STICK_DOWN,
		STEFX_HM_INPUT_LEFT_STICK_LEFT }, // Pad mobile crossed
	{ STEFX_HM_MOVE_DPAD_TANK, STEFX_HM_LOOK_NONE,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B, STEFX_HM_INPUT_X, STEFX_HM_INPUT_NONE,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_LEFT_THUMB }, // Old school
	{ STEFX_HM_MOVE_DPAD_TANK, STEFX_HM_LOOK_NONE,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_X, STEFX_HM_INPUT_B,
		STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_NONE, STEFX_HM_INPUT_BLACK }, // Fire below
	{ STEFX_HM_MOVE_LEFT_STICK, STEFX_HM_LOOK_RIGHT_STICK,
		STEFX_HM_INPUT_A, STEFX_HM_INPUT_B, STEFX_HM_INPUT_X, STEFX_HM_INPUT_Y,
		STEFX_HM_INPUT_RIGHT_TRIGGER, STEFX_HM_INPUT_LEFT_TRIGGER,
		STEFX_HM_INPUT_BLACK, STEFX_HM_INPUT_WHITE,
		STEFX_HM_INPUT_DPAD_UP, STEFX_HM_INPUT_DPAD_DOWN, STEFX_HM_INPUT_LEFT_THUMB } // Custom fallback
};

static qboolean STEFX_SplitHolomatchInputPressed( const stefxSplitPadState_t *pad, int input )
{
	const int axisThreshold = 16384;

	if ( !pad || input < STEFX_HM_INPUT_A )
	{
		return qfalse;
	}
	if ( input <= STEFX_HM_INPUT_RIGHT_TRIGGER )
	{
		return (qboolean)( pad->analogButtons[input] > STEFX_SPLIT_ANALOG_BUTTON_THRESHOLD );
	}
	switch ( input )
	{
	case STEFX_HM_INPUT_DPAD_UP: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_UP );
	case STEFX_HM_INPUT_DPAD_DOWN: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_DOWN );
	case STEFX_HM_INPUT_DPAD_LEFT: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_LEFT );
	case STEFX_HM_INPUT_DPAD_RIGHT: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_RIGHT );
	case STEFX_HM_INPUT_LEFT_THUMB: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_LEFT_THUMB );
	case STEFX_HM_INPUT_RIGHT_THUMB: return (qboolean)( pad->buttons & STEFX_SPLIT_BUTTON_RIGHT_THUMB );
	case STEFX_HM_INPUT_LEFT_STICK_UP: return (qboolean)( pad->thumbLY > axisThreshold );
	case STEFX_HM_INPUT_LEFT_STICK_DOWN: return (qboolean)( pad->thumbLY < -axisThreshold );
	case STEFX_HM_INPUT_LEFT_STICK_LEFT: return (qboolean)( pad->thumbLX < -axisThreshold );
	case STEFX_HM_INPUT_LEFT_STICK_RIGHT: return (qboolean)( pad->thumbLX > axisThreshold );
	case STEFX_HM_INPUT_RIGHT_STICK_UP: return (qboolean)( pad->thumbRY > axisThreshold );
	case STEFX_HM_INPUT_RIGHT_STICK_DOWN: return (qboolean)( pad->thumbRY < -axisThreshold );
	case STEFX_HM_INPUT_RIGHT_STICK_LEFT: return (qboolean)( pad->thumbRX < -axisThreshold );
	case STEFX_HM_INPUT_RIGHT_STICK_RIGHT: return (qboolean)( pad->thumbRX > axisThreshold );
	default: return qfalse;
	}
}

static void STEFX_SplitHolomatchAttackRumble( int localSlot, int port, stefxSplitPadState_t *pad, qboolean attackDown )
{
	char vibrationCvar[32];
	int script;

	if ( !pad )
	{
		return;
	}
	if ( attackDown && !pad->attackDown )
	{
		Com_sprintf( vibrationCvar, sizeof( vibrationCvar ), "joy_vibestate_%d", localSlot );
		if ( Cvar_VariableIntegerValue( vibrationCvar ) )
		{
			script = IN_CreateRumbleScript( port, 1, true );
			if ( script >= 0 )
			{
				IN_AddRumbleState( script, 12000, 22000, 70 );
				IN_ExecuteRumbleScript( script );
			}
		}
	}
	pad->attackDown = attackDown;
}

qboolean CL_STEFX_SplitScreen_BuildHolomatchUsercmd( int localSlot, usercmd_t *cmd, const vec3_t currentAngles, const int deltaAngles[3], int serverTime, int *sourcePort, int *weaponDelta, vec3_t outAngles, float *zoomFov )
{
	static int s_lastAssignedPort[STEFX_SPLIT_MAX_PADS] = { -2, -2, -2, -2 };
	static int s_lastControlStyle[STEFX_SPLIT_MAX_PADS] = { -1, -1, -1, -1 };
	static int s_cmdLogBudget[STEFX_SPLIT_MAX_PADS] = { 96, 96, 96, 96 };
	static int s_lastCmdLogTime[STEFX_SPLIT_MAX_PADS] = { -1000, -1000, -1000, -1000 };
	static int s_lastForwardState[STEFX_SPLIT_MAX_PADS] = { 2, 2, 2, 2 };
	static int s_lastRightState[STEFX_SPLIT_MAX_PADS] = { 2, 2, 2, 2 };
	static int s_lastUpMove[STEFX_SPLIT_MAX_PADS] = { 999, 999, 999, 999 };
	static int s_lastButtons[STEFX_SPLIT_MAX_PADS] = { -1, -1, -1, -1 };
	static float s_lastZoomLog[STEFX_SPLIT_MAX_PADS] = { -1.0f, -1.0f, -1.0f, -1.0f };
	stefxSplitPadState_t *pad;
	const stefxHolomatchControlProfile_t *profile;
	int port;
	int frameMsec;
	int controlStyle;
	float leftX;
	float leftY;
	float rightX;
	float rightY;
	int pitchDirection;
	char pitchCvar[32];
	qboolean weaponNextDown;
	qboolean weaponPrevDown;
	qboolean runToggleDown;
	qboolean attackDown;
	qboolean zoomInDown;
	qboolean zoomOutDown;
	char controlCvar[32];

	if ( !cmd || localSlot < 0 || localSlot >= STEFX_SPLIT_MAX_PADS )
	{
		return qfalse;
	}
	if ( weaponDelta )
	{
		*weaponDelta = 0;
	}
	if ( zoomFov )
	{
		*zoomFov = 90.0f;
	}
	port = STEFX_SplitRealPadForLocalSlot( localSlot );
	if ( port != s_lastAssignedPort[localSlot] )
	{
		XBLF( "STEFX_HM_SPLIT_PAD_ASSIGN: slot=%d port=%d previous=%d connectedMask=0x%x",
			localSlot,
			port,
			s_lastAssignedPort[localSlot],
			( STEFX_SplitPadUsableForRealInput( 0 ) ? 1 : 0 ) |
			( STEFX_SplitPadUsableForRealInput( 1 ) ? 2 : 0 ) |
			( STEFX_SplitPadUsableForRealInput( 2 ) ? 4 : 0 ) |
			( STEFX_SplitPadUsableForRealInput( 3 ) ? 8 : 0 ) );
		s_lastAssignedPort[localSlot] = port;
	}
	if ( port < 0 )
	{
		return qfalse;
	}

	pad = &s_stefxSplitPads[port];
	Com_sprintf( controlCvar, sizeof( controlCvar ), "cont_config_%d", localSlot );
	controlStyle = Cvar_VariableIntegerValue( controlCvar );
	if ( controlStyle < 0 || controlStyle >= (int)( sizeof( s_stefxHolomatchControlProfiles ) / sizeof( s_stefxHolomatchControlProfiles[0] ) ) )
	{
		controlStyle = 0;
	}
	profile = &s_stefxHolomatchControlProfiles[controlStyle];
	if ( s_lastControlStyle[localSlot] != controlStyle )
	{
		XBLF( "STEFX_HM_SPLIT_CONTROL_STYLE: slot=%d port=%d style=%d moveMode=%d lookMode=%d jump=%d crouch=%d use=%d attack=%d alt=%d prev=%d next=%d zoom=%d/%d run=%d",
			localSlot, port, controlStyle, profile->moveMode, profile->lookMode,
			profile->jump, profile->crouch, profile->use, profile->attack,
			profile->altAttack, profile->weaponPrev, profile->weaponNext,
			profile->zoomIn, profile->zoomOut, profile->runToggle );
		s_lastControlStyle[localSlot] = controlStyle;
	}
	if ( !pad->viewanglesValid || serverTime < pad->lastServerTime )
	{
		const qboolean reseeded = pad->viewanglesValid;
		VectorCopy( currentAngles, pad->viewangles );
		pad->viewanglesValid = qtrue;
		pad->lastServerTime = serverTime;
		pad->runEnabled = (qboolean)Cvar_VariableIntegerValue( "cl_run" );
		pad->holomatchZoomFov = 90.0f;
		pad->holomatchZoomValid = qtrue;
		pad->weaponNextDown = qfalse;
		pad->weaponPrevDown = qfalse;
		pad->runToggleDown = qfalse;
		pad->attackDown = qfalse;
		if ( reseeded )
		{
			XBLF( "STEFX_HM_SPLIT_PAD_RESEED: slot=%d port=%d serverTime=%d view=(%g,%g,%g)",
				localSlot, port, serverTime,
				pad->viewangles[PITCH], pad->viewangles[YAW], pad->viewangles[ROLL] );
		}
	}
	frameMsec = serverTime - pad->lastServerTime;
	if ( frameMsec < 0 || frameMsec > 100 )
	{
		frameMsec = 16;
	}
	else if ( frameMsec == 0 )
	{
		frameMsec = 1;
	}
	pad->lastServerTime = serverTime;

	leftX = STEFX_SplitNormalizeThumb( pad->thumbLX );
	leftY = STEFX_SplitNormalizeThumb( pad->thumbLY );
	rightX = STEFX_SplitNormalizeThumb( pad->thumbRX );
	rightY = STEFX_SplitNormalizeThumb( pad->thumbRY );

	memset( cmd, 0, sizeof( *cmd ) );
	cmd->serverTime = serverTime;
	switch ( profile->moveMode )
	{
	case STEFX_HM_MOVE_RIGHT_STICK:
		cmd->forwardmove = STEFX_SplitAxisToMove( rightY );
		cmd->rightmove = STEFX_SplitAxisToMove( rightX );
		break;
	case STEFX_HM_MOVE_DPAD:
		cmd->forwardmove = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_UP ) ? 127 :
			( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_DOWN ) ? -127 : 0 );
		cmd->rightmove = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_RIGHT ) ? 127 :
			( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_LEFT ) ? -127 : 0 );
		break;
	case STEFX_HM_MOVE_FACE:
		cmd->forwardmove = STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_Y ) ? 127 :
			( STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_A ) ? -127 : 0 );
		cmd->rightmove = STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_B ) ? 127 :
			( STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_X ) ? -127 : 0 );
		break;
	case STEFX_HM_MOVE_DPAD_TANK:
		cmd->forwardmove = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_UP ) ? 127 :
			( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_DOWN ) ? -127 : 0 );
		cmd->rightmove = 0;
		break;
	default:
		cmd->forwardmove = STEFX_SplitAxisToMove( leftY );
		cmd->rightmove = STEFX_SplitAxisToMove( leftX );
		break;
	}

	switch ( profile->lookMode )
	{
	case STEFX_HM_LOOK_LEFT_STICK:
		rightX = leftX;
		rightY = leftY;
		break;
	case STEFX_HM_LOOK_FACE:
		rightX = STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_B ) ? 1.0f :
			( STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_X ) ? -1.0f : 0.0f );
		rightY = STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_Y ) ? 1.0f :
			( STEFX_SplitHolomatchInputPressed( pad, STEFX_HM_INPUT_A ) ? -1.0f : 0.0f );
		break;
	case STEFX_HM_LOOK_DPAD:
		rightX = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_RIGHT ) ? 1.0f :
			( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_LEFT ) ? -1.0f : 0.0f );
		rightY = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_UP ) ? 1.0f :
			( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_DOWN ) ? -1.0f : 0.0f );
		break;
	case STEFX_HM_LOOK_NONE:
		rightX = 0.0f;
		rightY = 0.0f;
		if ( profile->moveMode == STEFX_HM_MOVE_DPAD_TANK )
		{
			rightX = ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_RIGHT ) ? 1.0f :
				( ( pad->buttons & STEFX_SPLIT_BUTTON_DPAD_LEFT ) ? -1.0f : 0.0f );
		}
		break;
	default:
		break;
	}
	Com_sprintf( pitchCvar, sizeof( pitchCvar ), "joy_pitchsensitivity_%d", localSlot );
	pitchDirection = Cvar_VariableIntegerValue( pitchCvar );
	if ( pitchDirection != -1 )
	{
		pitchDirection = 1;
	}
	pad->viewangles[YAW] = AngleNormalize360( pad->viewangles[YAW] - rightX * 220.0f * ( frameMsec / 1000.0f ) );
	pad->viewangles[PITCH] = AngleNormalize180( pad->viewangles[PITCH] - rightY * (float)pitchDirection * 160.0f * ( frameMsec / 1000.0f ) );
	if ( pad->viewangles[PITCH] > 80.0f )
	{
		pad->viewangles[PITCH] = 80.0f;
	}
	else if ( pad->viewangles[PITCH] < -80.0f )
	{
		pad->viewangles[PITCH] = -80.0f;
	}
	pad->viewangles[ROLL] = 0.0f;

	runToggleDown = STEFX_SplitHolomatchInputPressed( pad, profile->runToggle );
	if ( runToggleDown && !pad->runToggleDown )
	{
		pad->runEnabled = (qboolean)!pad->runEnabled;
		XBLF( "STEFX_HM_SPLIT_PAD_RUN: slot=%d port=%d run=%d", localSlot, port, pad->runEnabled ? 1 : 0 );
	}
	pad->runToggleDown = runToggleDown;
	STEFX_SplitApplyRunState( pad, cmd );

	if ( STEFX_SplitHolomatchInputPressed( pad, profile->jump ) )
	{
		cmd->upmove = 127;
	}
	else if ( STEFX_SplitHolomatchInputPressed( pad, profile->crouch ) )
	{
		cmd->upmove = -127;
	}
	attackDown = STEFX_SplitHolomatchInputPressed( pad, profile->attack );
	if ( attackDown )
	{
		cmd->buttons |= BUTTON_ATTACK;
	}
	if ( STEFX_SplitHolomatchInputPressed( pad, profile->altAttack ) )
	{
		cmd->buttons |= BUTTON_ALT_ATTACK;
	}
	if ( STEFX_SplitHolomatchInputPressed( pad, profile->use ) )
	{
		cmd->buttons |= BUTTON_USE;
	}
	if ( STEFX_SplitHolomatchInputPressed( pad, profile->centerView ) )
	{
		pad->viewangles[PITCH] = 0.0f;
	}
	STEFX_SplitHolomatchAttackRumble( localSlot, port, pad, attackDown );

	weaponPrevDown = STEFX_SplitHolomatchInputPressed( pad, profile->weaponPrev );
	weaponNextDown = STEFX_SplitHolomatchInputPressed( pad, profile->weaponNext );
	zoomInDown = STEFX_SplitHolomatchInputPressed( pad, profile->zoomIn );
	zoomOutDown = STEFX_SplitHolomatchInputPressed( pad, profile->zoomOut );
	if ( !pad->holomatchZoomValid )
	{
		pad->holomatchZoomFov = 90.0f;
		pad->holomatchZoomValid = qtrue;
	}
	if ( zoomInDown != zoomOutDown )
	{
		pad->holomatchZoomFov += ( zoomOutDown ? 1.0f : -1.0f ) * 60.0f * ( frameMsec / 1000.0f );
		if ( pad->holomatchZoomFov < 30.0f )
		{
			pad->holomatchZoomFov = 30.0f;
		}
		else if ( pad->holomatchZoomFov > 90.0f )
		{
			pad->holomatchZoomFov = 90.0f;
		}
	}
	if ( zoomFov )
	{
		*zoomFov = pad->holomatchZoomFov;
	}
	if ( weaponDelta )
	{
		if ( weaponNextDown && !pad->weaponNextDown )
		{
			*weaponDelta = 1;
		}
		else if ( weaponPrevDown && !pad->weaponPrevDown )
		{
			*weaponDelta = -1;
		}
	}
	pad->weaponPrevDown = weaponPrevDown;
	pad->weaponNextDown = weaponNextDown;

	cmd->angles[PITCH] = ANGLE2SHORT( pad->viewangles[PITCH] ) - ( deltaAngles ? deltaAngles[PITCH] : 0 );
	cmd->angles[YAW] = ANGLE2SHORT( pad->viewangles[YAW] ) - ( deltaAngles ? deltaAngles[YAW] : 0 );
	cmd->angles[ROLL] = 0;
	if ( outAngles )
	{
		VectorCopy( pad->viewangles, outAngles );
	}
	if ( sourcePort )
	{
		*sourcePort = port;
	}

	{
		const int forwardState = cmd->forwardmove < 0 ? -1 : ( cmd->forwardmove > 0 ? 1 : 0 );
		const int rightState = cmd->rightmove < 0 ? -1 : ( cmd->rightmove > 0 ? 1 : 0 );
		const qboolean changed = (qboolean)(
			forwardState != s_lastForwardState[localSlot] ||
			rightState != s_lastRightState[localSlot] ||
			cmd->upmove != s_lastUpMove[localSlot] ||
			cmd->buttons != s_lastButtons[localSlot] ||
			( weaponDelta && *weaponDelta ) ||
			fabs( pad->holomatchZoomFov - s_lastZoomLog[localSlot] ) >= 2.0f );
		if ( s_cmdLogBudget[localSlot] > 0 &&
			( changed || serverTime - s_lastCmdLogTime[localSlot] >= 1000 ) )
		{
			XBLF( "STEFX_HM_SPLIT_PAD_CMD: slot=%d port=%d style=%d time=%d move=(%d,%d,%d) buttons=0x%x weaponDelta=%d zoom=%g look=(%g,%g) pitchDirection=%d view=(%g,%g,%g)",
				localSlot,
				port,
				controlStyle,
				serverTime,
				cmd->forwardmove,
				cmd->rightmove,
				cmd->upmove,
				cmd->buttons,
				weaponDelta ? *weaponDelta : 0,
				pad->holomatchZoomFov,
				rightX,
				rightY,
				pitchDirection,
				pad->viewangles[PITCH],
				pad->viewangles[YAW],
				pad->viewangles[ROLL] );
			s_lastCmdLogTime[localSlot] = serverTime;
			s_lastZoomLog[localSlot] = pad->holomatchZoomFov;
			--s_cmdLogBudget[localSlot];
		}
		s_lastForwardState[localSlot] = forwardState;
		s_lastRightState[localSlot] = rightState;
		s_lastUpMove[localSlot] = cmd->upmove;
		s_lastButtons[localSlot] = cmd->buttons;
	}
	return qtrue;
}

static qboolean STEFX_SmokeHarnessEnabled( void )
{
	static qboolean s_checked = qfalse;
	static qboolean s_enabled = qfalse;
	FILE *file;

	if ( s_checked )
	{
		return s_enabled;
	}
	s_checked = qtrue;

	file = fopen( "D:\\ef_sp_smoke_harness.txt", "r" );
	if ( file )
	{
		fclose( file );
		s_enabled = qtrue;
		XBL("STEFX: smoke harness marker enabled client input");
	}

	return s_enabled;
}

static qboolean STEFX_ViewAnglesBad( const vec3_t angles )
{
	return (qboolean)(IS_NAN(angles[0]) || IS_NAN(angles[1]) || IS_NAN(angles[2]));
}

static void STEFX_SeedClientViewAnglesIfInvalid( void )
{
	static int s_logBudget = 8;

	if ( !STEFX_ViewAnglesBad( cl.viewangles ) )
	{
		return;
	}

	VectorCopy( cl.frame.ps.viewangles, cl.viewangles );
	if ( STEFX_ViewAnglesBad( cl.viewangles ) )
	{
		VectorClear( cl.viewangles );
	}

	if ( s_logBudget > 0 )
	{
		XBLF("STEFX: seeded invalid client viewangles from snapshot view=(%g,%g,%g) ps=(%g,%g,%g)",
			cl.viewangles[0], cl.viewangles[1], cl.viewangles[2],
			cl.frame.ps.viewangles[0], cl.frame.ps.viewangles[1], cl.frame.ps.viewangles[2]);
		--s_logBudget;
	}
}

static void STEFX_InitSmokeInputCvars( void )
{
	stefx_smokeInput = Cvar_Get( "stefx_smoke_input", "0", CVAR_TEMP );
	stefx_smokeInputStart = Cvar_Get( "stefx_smoke_input_start", "18000", CVAR_TEMP );
	stefx_smokeInputEnd = Cvar_Get( "stefx_smoke_input_end", "26000", CVAR_TEMP );
	stefx_smokeInputForward = Cvar_Get( "stefx_smoke_input_forward", "90", CVAR_TEMP );
	stefx_smokeInputSide = Cvar_Get( "stefx_smoke_input_side", "0", CVAR_TEMP );
	stefx_smokeInputYaw = Cvar_Get( "stefx_smoke_input_yaw", "0", CVAR_TEMP );
	stefx_smokeViewPitch = Cvar_Get( "stefx_smoke_view_pitch", "9999", CVAR_TEMP );
	stefx_smokeViewYaw = Cvar_Get( "stefx_smoke_view_yaw", "9999", CVAR_TEMP );
	stefx_smokeInputAttackStart = Cvar_Get( "stefx_smoke_input_attack_start", "19000", CVAR_TEMP );
	stefx_smokeInputAttackEnd = Cvar_Get( "stefx_smoke_input_attack_end", "23000", CVAR_TEMP );
	stefx_smokeInputAltAttackStart = Cvar_Get( "stefx_smoke_input_alt_attack_start", "0", CVAR_TEMP );
	stefx_smokeInputAltAttackEnd = Cvar_Get( "stefx_smoke_input_alt_attack_end", "0", CVAR_TEMP );

	XBL("STEFX: smoke input cvars registered; diagnostic input is off by default");
}

static void STEFX_ApplySmokeInput( usercmd_t *cmd )
{
	static int s_logBudget = 48;
	extern bool in_camera;
	const int serverTime = cmd ? cmd->serverTime : 0;
	int startTime;
	int endTime;
	int attackStart;
	int attackEnd;
	int altAttackStart;
	int altAttackEnd;
	int forwardMove;
	int sideMove;
	int yawMove;
	int viewPitch;
	int viewYaw;
	qboolean active;
	qboolean attacking;
	qboolean altAttacking;

	if ( !cmd || in_camera || !STEFX_SmokeHarnessEnabled() || !stefx_smokeInput || !stefx_smokeInput->integer )
	{
		return;
	}

	startTime = stefx_smokeInputStart ? stefx_smokeInputStart->integer : 18000;
	endTime = stefx_smokeInputEnd ? stefx_smokeInputEnd->integer : 26000;
	attackStart = stefx_smokeInputAttackStart ? stefx_smokeInputAttackStart->integer : 19000;
	attackEnd = stefx_smokeInputAttackEnd ? stefx_smokeInputAttackEnd->integer : 23000;
	altAttackStart = stefx_smokeInputAltAttackStart ? stefx_smokeInputAltAttackStart->integer : 0;
	altAttackEnd = stefx_smokeInputAltAttackEnd ? stefx_smokeInputAltAttackEnd->integer : 0;
	forwardMove = stefx_smokeInputForward ? stefx_smokeInputForward->integer : 90;
	sideMove = stefx_smokeInputSide ? stefx_smokeInputSide->integer : 0;
	yawMove = stefx_smokeInputYaw ? stefx_smokeInputYaw->integer : 0;
	viewPitch = stefx_smokeViewPitch ? stefx_smokeViewPitch->integer : 9999;
	viewYaw = stefx_smokeViewYaw ? stefx_smokeViewYaw->integer : 9999;
	active = ( serverTime >= startTime && ( endTime <= 0 || serverTime <= endTime ) );
	if ( !active )
	{
		return;
	}

	if ( viewPitch != 9999 || viewYaw != 9999 )
	{
		if ( viewPitch != 9999 )
		{
			cl.viewangles[PITCH] = (float)viewPitch;
			cmd->angles[PITCH] = ANGLE2SHORT( cl.viewangles[PITCH] );
		}
		if ( viewYaw != 9999 )
		{
			cl.viewangles[YAW] = (float)viewYaw;
			cmd->angles[YAW] = ANGLE2SHORT( cl.viewangles[YAW] );
		}
		if ( s_logBudget > 0 )
		{
			Com_PrintfAlways( "STEFX: smoke view override serverTime=%d pitch=%d yaw=%d final=(%g,%g,%g)\n",
				serverTime,
				viewPitch,
				viewYaw,
				cl.viewangles[0],
				cl.viewangles[1],
				cl.viewangles[2] );
			--s_logBudget;
		}
	}

	if ( !forwardMove && !sideMove )
	{
		forwardMove = 90;
	}
	if ( attackEnd > 0 && attackEnd < startTime )
	{
		attackStart = startTime + 1000;
		attackEnd = ( endTime > attackStart ) ? endTime : attackStart + 4000;
	}

	cmd->forwardmove = ClampChar( cmd->forwardmove + forwardMove );
	cmd->rightmove = ClampChar( cmd->rightmove + sideMove );
	/* Keep the process-local harness deterministic even when an attached pad or
	 * retained binding reports a trigger during the bounded smoke window. */
	cmd->buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK);
	if ( yawMove )
	{
		cl.viewangles[YAW] += (float)yawMove;
		cmd->angles[YAW] = ANGLE2SHORT( cl.viewangles[YAW] );
	}

	attacking = ( serverTime >= attackStart && ( attackEnd <= 0 || serverTime <= attackEnd ) );
	if ( attacking )
	{
		cmd->buttons |= BUTTON_ATTACK;
	}
	altAttacking = ( altAttackStart > 0 && serverTime >= altAttackStart &&
		( altAttackEnd <= 0 || serverTime <= altAttackEnd ) );
	if ( altAttacking )
	{
		cmd->buttons |= BUTTON_ALT_ATTACK;
	}

	if ( s_logBudget > 0 )
	{
		Com_PrintfAlways( "STEFX: smoke input applied serverTime=%d window=%d..%d attackWindow=%d..%d altAttackWindow=%d..%d configuredInput=(%d,%d,%d) move=(%d,%d,%d) attack=%d altAttack=%d buttons=0x%x weapon=%d\n",
			serverTime,
			startTime,
			endTime,
			attackStart,
			attackEnd,
			altAttackStart,
			altAttackEnd,
			forwardMove,
			sideMove,
			yawMove,
			cmd->forwardmove,
			cmd->rightmove,
			cmd->upmove,
			attacking ? 1 : 0,
			altAttacking ? 1 : 0,
			cmd->buttons,
			cmd->weapon );
		s_logBudget--;
	}
}
#endif


#ifdef _XBOX
HotSwapManager swapMan1(HOTSWAP_ID_WHITE);
HotSwapManager swapMan2(HOTSWAP_ID_BLACK);
HotSwapManager swapMan3(HOTSWAP_ID_YELLOW);


void IN_HotSwap1On(void)
{
	swapMan1.SetDown();
}

void IN_HotSwap2On(void)
{
	swapMan2.SetDown();
}

void IN_HotSwap3On(void)
{
	swapMan3.SetDown();
}


void IN_HotSwap1Off(void)
{
	swapMan1.SetUp();
}

void IN_HotSwap2Off(void)
{
	swapMan2.SetUp();
}

void IN_HotSwap3Off(void)
{
	swapMan3.SetUp();
}


void CL_UpdateHotSwap(void)
{
	swapMan1.Update();
	swapMan2.Update();
	swapMan3.Update();
}


bool CL_ExtendSelectTime(void)
{
	return swapMan1.ButtonDown() || swapMan2.ButtonDown() || swapMan3.ButtonDown();
}
#endif


static void IN_UseGivenForce(void)
{
#ifdef _XBOX
	XBLF("STEFX_INPUT: ignored JA useGivenForce command arg='%s'", Cmd_Argv(1));
#endif
}


void IN_MLookDown( void ) {
	in_mlooking = qtrue;
}

void IN_MLookUp( void ) {
	in_mlooking = qfalse;
	if ( !cl_freelook->integer ) {
		IN_CenterView ();
	}
}

void IN_KeyDown( kbutton_t *b ) {
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if ( c[0] ) {
		k = atoi(c);
	} else {
		k = -1;		// typed manually at the console for continuous down
	}

	if ( k == b->down[0] || k == b->down[1] ) {
		return;		// repeating key
	}
	
	if ( !b->down[0] ) {
		b->down[0] = k;
	} else if ( !b->down[1] ) {
		b->down[1] = k;
	} else {
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if ( b->active ) {
		return;		// still down
	}

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	b->downtime = atoi(c);

	b->active = qtrue;
	b->wasPressed = qtrue;
}

void IN_KeyUp( kbutton_t *b ) {
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if ( c[0] ) {
		k = atoi(c);
	} else {
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->active = qfalse;
		return;
	}

	if ( b->down[0] == k ) {
		b->down[0] = 0;
	} else if ( b->down[1] == k ) {
		b->down[1] = 0;
	} else {
		return;		// key up without coresponding down (menu pass through)
	}
	if ( b->down[0] || b->down[1] ) {
		return;		// some other key is still holding it down
	}

	b->active = qfalse;

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if ( uptime ) {
		b->msec += uptime - b->downtime;
	} else {
		b->msec += frame_msec / 2;
	}

	b->active = qfalse;
}



/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState( kbutton_t *key ) {
	float		val;
	int			msec;

	msec = key->msec;
	key->msec = 0;

	if ( key->active ) {
		// still down
		if ( !key->downtime ) {
			msec = com_frameTime;
		} else {
			msec += com_frameTime - key->downtime;
		}
		key->downtime = com_frameTime;
	}

#if 0
	if (msec) {
		Com_Printf ("%i ", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if ( val < 0 ) {
		val = 0;
	}
	if ( val > 1 ) {
		val = 1;
	}

	return val;
}



void IN_UpDown(void) {IN_KeyDown(&in_up);}
void IN_UpUp(void) {IN_KeyUp(&in_up);}
void IN_DownDown(void) {IN_KeyDown(&in_down);}
void IN_DownUp(void) {IN_KeyUp(&in_down);}
void IN_LeftDown(void) {IN_KeyDown(&in_left);}
void IN_LeftUp(void) {IN_KeyUp(&in_left);}
void IN_RightDown(void) {IN_KeyDown(&in_right);}
void IN_RightUp(void) {IN_KeyUp(&in_right);}
void IN_ForwardDown(void) {IN_KeyDown(&in_forward);}
void IN_ForwardUp(void) {IN_KeyUp(&in_forward);}
void IN_BackDown(void) {IN_KeyDown(&in_back);}
void IN_BackUp(void) {IN_KeyUp(&in_back);}
void IN_LookupDown(void) {IN_KeyDown(&in_lookup);}
void IN_LookupUp(void) {IN_KeyUp(&in_lookup);}
void IN_LookdownDown(void) {IN_KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {IN_KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {IN_KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {IN_KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {IN_KeyDown(&in_moveright);}
void IN_MoverightUp(void) {IN_KeyUp(&in_moveright);}

void IN_SpeedDown(void) {IN_KeyDown(&in_speed);}
void IN_SpeedUp(void) {IN_KeyUp(&in_speed);}
void IN_StrafeDown(void) {IN_KeyDown(&in_strafe);}
void IN_StrafeUp(void) {IN_KeyUp(&in_strafe);}

void IN_Button0Down(void) {IN_KeyDown(&in_buttons[0]);}
void IN_Button0Up(void) {IN_KeyUp(&in_buttons[0]);}
void IN_Button1Down(void) {IN_KeyDown(&in_buttons[1]);}
void IN_Button1Up(void) {IN_KeyUp(&in_buttons[1]);}
void IN_Button2Down(void) {IN_KeyDown(&in_buttons[2]);}
void IN_Button2Up(void) {IN_KeyUp(&in_buttons[2]);}
void IN_Button3Down(void) {IN_KeyDown(&in_buttons[3]);}
void IN_Button3Up(void) {IN_KeyUp(&in_buttons[3]);}
void IN_Button4Down(void) {IN_KeyDown(&in_buttons[4]);}
void IN_Button4Up(void) {IN_KeyUp(&in_buttons[4]);}
void IN_Button5Down(void) {IN_KeyDown(&in_buttons[5]);}
void IN_Button5Up(void) {IN_KeyUp(&in_buttons[5]);}
void IN_Button6Down(void) {IN_KeyDown(&in_buttons[6]);}
void IN_Button6Up(void) {IN_KeyUp(&in_buttons[6]);}
void IN_Button7Down(void) {IN_KeyDown(&in_buttons[7]);}
void IN_Button7Up(void) {IN_KeyUp(&in_buttons[7]);}
void IN_Button8Down(void) {IN_KeyDown(&in_buttons[8]);}
void IN_Button8Up(void) {IN_KeyUp(&in_buttons[8]);}


void IN_CenterView (void) {
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.ps.delta_angles[PITCH]);
}


//==========================================================================

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
/*
void CL_AdjustAngles( void ) {
	float	speed;
	
	if ( in_speed.active ) {
		speed = 0.001 * cls.frametime * cl_anglespeedkey->value;
	} else {
		speed = 0.001 * cls.frametime;
	}

	if ( !in_strafe.active ) {
		if ( cl_mYawOverride )
		{
			cl.viewangles[YAW] -= cl_mYawOverride*5.0f*speed*cl_yawspeed->value*CL_KeyState (&in_right);
			cl.viewangles[YAW] += cl_mYawOverride*5.0f*speed*cl_yawspeed->value*CL_KeyState (&in_left);
		}
		else
		{
			cl.viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
			cl.viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
		}
	}

	if ( cl_mPitchOverride )
	{
		cl.viewangles[PITCH] -= cl_mPitchOverride*5.0f*speed*cl_pitchspeed->value * CL_KeyState (&in_lookup);
		cl.viewangles[PITCH] += cl_mPitchOverride*5.0f*speed*cl_pitchspeed->value * CL_KeyState (&in_lookdown);
	}
	else
	{
		cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_lookup);
		cl.viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_lookdown);
	}
}
*/

/*
================
CL_KeyMove

Sets the usercmd_t based on key states
================
*/
void CL_KeyMove( usercmd_t *cmd ) {
	int		movespeed;
	int		forward, side, up;

	//
	// adjust for speed key / running
	// the walking flag is to keep animations consistant
	// even during acceleration and develeration
	//
	if ( in_speed.active ^ cl_run->integer ) {
		movespeed = 127;
		cmd->buttons &= ~BUTTON_WALKING;
	} else {
		cmd->buttons |= BUTTON_WALKING;
		movespeed = 64;
	}

	forward = 0;
	side = 0;
	up = 0;
	if ( in_strafe.active ) {
		side += movespeed * CL_KeyState (&in_right);
		side -= movespeed * CL_KeyState (&in_left);
	}

	side += movespeed * CL_KeyState (&in_moveright);
	side -= movespeed * CL_KeyState (&in_moveleft);


	up += movespeed * CL_KeyState (&in_up);
	up -= movespeed * CL_KeyState (&in_down);

	forward += movespeed * CL_KeyState (&in_forward);
	forward -= movespeed * CL_KeyState (&in_back);

	cmd->forwardmove = ClampChar( forward );
	cmd->rightmove = ClampChar( side );
	cmd->upmove = ClampChar( up );
}

void _UI_MouseEvent( int dx, int dy );

/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent( int dx, int dy, int time ) {
	if ( cls.keyCatchers & KEYCATCH_UI ) {
		_UI_MouseEvent( dx, dy );
	}
	else {
		cl.mouseDx[cl.mouseIndex] += dx;
		cl.mouseDy[cl.mouseIndex] += dy;
	}
}

/*
=================
CL_JoystickEvent

Joystick values stay set until changed
=================
*/
void CL_JoystickEvent( int axis, int value, int time ) {
	if ( axis < 0 || axis >= MAX_JOYSTICK_AXIS ) {
		Com_Error( ERR_DROP, "CL_JoystickEvent: bad axis %i", axis );
	}
	cl.joystickAxis[axis] = value;
}

/*
=================
CL_JoystickMove
=================
*/
void CL_JoystickMove( usercmd_t *cmd )
{
	cmd->rightmove = ClampChar( cmd->rightmove + cl.joystickAxis[AXIS_SIDE] );
	cmd->forwardmove = ClampChar( cmd->forwardmove + cl.joystickAxis[AXIS_FORWARD] );
	cmd->upmove = ClampChar( cmd->upmove + cl.joystickAxis[AXIS_UP] );

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( !cl_run->integer && ( cmd->forwardmove || cmd->rightmove ) )
	{
		int maxComponent = abs( cmd->forwardmove );
		int sideComponent = abs( cmd->rightmove );
		if ( sideComponent > maxComponent )
		{
			maxComponent = sideComponent;
		}
		if ( maxComponent > 64 )
		{
			cmd->forwardmove = ClampChar( ( cmd->forwardmove * 64 ) / maxComponent );
			cmd->rightmove = ClampChar( ( cmd->rightmove * 64 ) / maxComponent );
		}
		cmd->buttons |= BUTTON_WALKING;
		return;
	}
#endif

	// Smarter run-speed detection, taking diagonals into account!
	if( (cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove) < (MOVE_RUN * MOVE_RUN) )
		cmd->buttons |= BUTTON_WALKING;
}

/*
=================
CL_MouseMove
=================
*/
#ifdef _XBOX
void CL_MouseClamp(int *x, int *y)
{
	float ax = Q_fabs(*x);
	float ay = Q_fabs(*y);

	ax = (ax-10)*(3.0f/45.0f) * (ax-10) * (Q_fabs(*x) > 10);
	ay = (ay-10)*(3.0f/45.0f) * (ay-10) * (Q_fabs(*y) > 10);
	if (*x < 0)
		*x = -ax;
	else
		*x = ax;
	if (*y < 0)
		*y = -ay;
	else
		*y = ay;
}
#endif

#if defined(STEFX_SP_HOSTED_MP)
static int s_stefxHolomatchLastFireTime = 0;
#else
extern short cg_crossHairStatus;
#endif

void CL_MouseMove( usercmd_t *cmd )
{
	const float mouseSpeedX = 0.06f;
	const float mouseSpeedY = 0.05f;
	const float m_hoverSensitivity = 0.4f;

	const float	speed = static_cast<float>(frame_msec);
	const float pitch = m_pitch->value;

	// Get raw stick values:
	int ax = cl.mouseDx[cl.mouseIndex];
	int ay = cl.mouseDy[cl.mouseIndex];
	// Run them through our filter:
	CL_MouseClamp(&ax, &ay);

	// Adjust for frame duration and make horizontal slightly faster:
	float mx = ax * speed * mouseSpeedX;
	float my = ay * speed * mouseSpeedY;
	// Save these before we start tuning them, in case user is on a vehicle with overrides:
	float rawMx = mx;
	float rawMy = my;

	// Slow it down when targeting an enemy:
#if defined(STEFX_SP_HOSTED_MP)
	const short crossHairStatus = 0;
#else
	const short crossHairStatus = cg_crossHairStatus;
#endif
	if (crossHairStatus == 1)
	{
		mx *= m_hoverSensitivity;
		my *= m_hoverSensitivity;
	}

	// Switch entries, clear values:
	cl.mouseIndex ^= 1;
	cl.mouseDx[cl.mouseIndex] = 0;
	cl.mouseDy[cl.mouseIndex] = 0;

	// scale by FOV and user settings:
	mx *= cl_sensitivity->value * cl.cgameSensitivity;

	// note the capital Y in the cvarname - scale by FOV and settings:
	my *= cl_sensitivityY->value * cl.cgameSensitivity;

	if (!mx && !my) {
		// If there was a movement but no change in angles then start auto-leveling the camera
		float autolevelSpeed = 0.03f;
#if defined(STEFX_SP_HOSTED_MP)
		const int lastFireTime = s_stefxHolomatchLastFireTime;
#else
		extern int g_lastFireTime;
		const int lastFireTime = g_lastFireTime;
#endif

		if (crossHairStatus != 1 &&							// Not looking at an enemy
			cl.joystickAxis[AXIS_FORWARD] &&					// Moving forward/backward
			cl.frame.ps.groundEntityNum != ENTITYNUM_NONE &&	// Not in the air
			Cvar_VariableIntegerValue("cl_autolevel") &&		// Autolevel is turned on
			lastFireTime < Sys_Milliseconds() - 1000)			// Haven't fired recently
		{
			float normAngle = -SHORT2ANGLE(cl.frame.ps.delta_angles[PITCH]);
			// The adjustment to normAngle below is meant to add or remove some multiple
			// of 360, so that normAngle is within 180 of viewangles[PITCH]. It should
			// be correct.
			int diff = (int)(cl.viewangles[PITCH] - normAngle);
			if (diff > 180)
				normAngle += 360.0f * ((diff+180) / 360);
			else if (diff < -180)
				normAngle -= 360.0f * ((-diff+180) / 360);

			if (Cvar_VariableIntegerValue("cg_thirdperson") == 1)
			{
//				normAngle += 10;	// Removed by BTO, 2003/05/14, I hate it
				autolevelSpeed *= 1.5f;
			}
			if (cl.viewangles[PITCH] > normAngle)
			{
				cl.viewangles[PITCH] -= autolevelSpeed * speed;
				if (cl.viewangles[PITCH] < normAngle) cl.viewangles[PITCH] = normAngle;
			}
			else if (cl.viewangles[PITCH] < normAngle)
			{
				cl.viewangles[PITCH] += autolevelSpeed * speed;
				if (cl.viewangles[PITCH] > normAngle) cl.viewangles[PITCH] = normAngle;
			}
		}
		return;
	}

	// Do yaw - use un-tweaked number if override is active:
	if ( cl_mYawOverride )
		cl.viewangles[YAW] -= cl_mYawOverride * rawMx;
	else
		cl.viewangles[YAW] -= m_yaw->value * mx;

	// Do pitch - use un-tweaked number if override is active:
	const float cl_pitchSensitivity = 0.5f;	// Should be a cvar!
	if ( cl_mPitchOverride )
	{
		if ( pitch > 0 )
			cl.viewangles[PITCH] += cl_mPitchOverride * rawMy * cl_pitchSensitivity;
		else
			cl.viewangles[PITCH] -= cl_mPitchOverride * rawMy * cl_pitchSensitivity;
	}
	else
	{
		cl.viewangles[PITCH] += pitch * my * cl_pitchSensitivity;
	}
}


/*
==============
CL_CmdButtons
==============
*/
void CL_CmdButtons( usercmd_t *cmd ) {
	int		i;

	//
	// figure button bits
	// send a button bit even if the key was pressed and released in
	// less than a frame
	//	
	for (i = 0 ; i < 9 ; i++) {
		if ( in_buttons[i].active || in_buttons[i].wasPressed ) {
			cmd->buttons |= 1 << i;
		}
		in_buttons[i].wasPressed = qfalse;
	}

	if ( cls.keyCatchers ) {
		//cmd->buttons |= BUTTON_TALK;
	}

	// allow the game to know if any key at all is
	// currently pressed, even if it isn't bound to anything
	/*
	if ( kg.anykeydown && !cls.keyCatchers ) {
		cmd->buttons |= BUTTON_ANY;
	}
	*/
}


/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove( usercmd_t *cmd ) {
	int		i;

	// copy the state that the cgame is currently sending
	cmd->weapon = cl.cgameUserCmdValue;

#if !defined(STEFX_ELITE_FORCE_SP)
	if (cl.gcmdSendValue)
	{
		cmd->generic_cmd = cl.gcmdValue;
		cl.gcmdSendValue = qfalse;
	}
	else
	{
		cmd->generic_cmd = 0;
	}
#else
	cl.gcmdSendValue = qfalse;
#endif

	// send the current server time so the amount of movement
	// can be determined without allowing cheating
	cmd->serverTime = cl.serverTime;

	for (i=0 ; i<3 ; i++) {
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
	}
}

/*
=================
CL_CreateCmd
=================
*/
vec3_t cl_overriddenAngles = {0,0,0};
qboolean cl_overrideAngles = qfalse;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
#include "stefx_input_replay.h"
#endif
usercmd_t CL_CreateCmd( void ) {
	usercmd_t	cmd;
	vec3_t		oldAngles;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	vec3_t		stefxProfileAngles;
	int			stefxProfilePort = -1;
	int			stefxProfileWeaponDelta = 0;
	float		stefxProfileZoomFov = 90.0f;
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SeedClientViewAnglesIfInvalid();
#endif
	VectorCopy( cl.viewangles, oldAngles );

	// keyboard angle adjustment
//	CL_AdjustAngles ();
	
	memset( &cmd, 0, sizeof( cmd ) );

	CL_CmdButtons( &cmd );

	// get basic movement from keyboard
	CL_KeyMove (&cmd);

#ifdef XBOX_DEMO
	static int lastStickData = cl.mouseDx[cl.mouseIndex] + cl.mouseDy[cl.mouseIndex] +
							   cl.joystickAxis[AXIS_SIDE] + cl.joystickAxis[AXIS_FORWARD];
	int newStickData = cl.mouseDx[cl.mouseIndex] + cl.mouseDy[cl.mouseIndex] +
					   cl.joystickAxis[AXIS_SIDE] + cl.joystickAxis[AXIS_FORWARD];
	if( lastStickData != newStickData )
	{
		extern void Demo_TimerKeypress( void );
		Demo_TimerKeypress();
	}
	lastStickData = newStickData;
#endif

	// get basic movement from mouse
	CL_MouseMove( &cmd );

	// get basic movement from joystick
	CL_JoystickMove( &cmd );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	STEFX_ApplyInputReplay(&cmd, oldAngles);
#endif

	// check to make sure the angles haven't wrapped
	if ( cl.viewangles[PITCH] - oldAngles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] + 90;
	} else if ( oldAngles[PITCH] - cl.viewangles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] - 90;
	} 

	if ( cl_overrideAngles )
	{
		VectorCopy( cl_overriddenAngles, cl.viewangles );
		cl_overrideAngles = qfalse;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_SeedClientViewAnglesIfInvalid();
#endif
	// store out the final values
	CL_FinishMove( &cmd );

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_ApplySmokeInput( &cmd );
#if defined(STEFX_SP_HOSTED_MP)
	/* P1 still owns exactly one native loopback packet.  Replace that packet's
	 * gameplay fields from the raw physical pad instead of relying on the SP
	 * key-binding table, which can be cleared by the retail PC default.cfg in
	 * PAK2.PK3.  The split bridge continues to begin at client 1. */
	if ( cls.state == CA_ACTIVE &&
		Cvar_VariableIntegerValue( "stefx_splitScreen" ) &&
		!Q_stricmp( Cvar_VariableString( "stefx_splitScreenMode" ), "holomatch" ) &&
		CL_STEFX_SplitScreen_BuildHolomatchUsercmd( 0,
			&cmd,
			oldAngles,
			NULL,
			cl.serverTime,
			&stefxProfilePort,
			&stefxProfileWeaponDelta,
			stefxProfileAngles,
			&stefxProfileZoomFov ) )
	{
		static int s_stefxProfiledP1LogBudget = 160;
		static int s_stefxProfiledP1LastLogTime = -1000;
		static int s_stefxProfiledP1LastForwardState = 2;
		static int s_stefxProfiledP1LastRightState = 2;
		static int s_stefxProfiledP1LastUp = 999;
		static int s_stefxProfiledP1LastButtons = -1;
		static int s_stefxProfiledP1LastWeapon = -1;
		static float s_stefxProfiledP1LastZoom = -1.0f;
		const int forwardState = cmd.forwardmove < 0 ? -1 : ( cmd.forwardmove > 0 ? 1 : 0 );
		const int rightState = cmd.rightmove < 0 ? -1 : ( cmd.rightmove > 0 ? 1 : 0 );
		const qboolean controlChanged = (qboolean)(
			forwardState != s_stefxProfiledP1LastForwardState ||
			rightState != s_stefxProfiledP1LastRightState ||
			cmd.upmove != s_stefxProfiledP1LastUp ||
			cmd.buttons != s_stefxProfiledP1LastButtons ||
			cmd.weapon != s_stefxProfiledP1LastWeapon ||
			stefxProfileWeaponDelta != 0 ||
			fabs( stefxProfileZoomFov - s_stefxProfiledP1LastZoom ) >= 2.0f );

		VectorCopy( stefxProfileAngles, cl.viewangles );
		cmd.weapon = cl.cgameUserCmdValue;
		if ( stefxProfileWeaponDelta > 0 )
		{
			Cbuf_AddText( "weapnext\n" );
		}
		else if ( stefxProfileWeaponDelta < 0 )
		{
			Cbuf_AddText( "weapprev\n" );
		}
		if ( !Cvar_VariableIntegerValue( "stefx_hm_p1_profile_controls" ) )
		{
			Cvar_Set( "stefx_hm_p1_profile_controls", "1" );
		}
		if ( fabs( Cvar_VariableValue( "stefx_hm_p1_profile_zoom_fov" ) - stefxProfileZoomFov ) > 0.05f )
		{
			Cvar_SetValue( "stefx_hm_p1_profile_zoom_fov", stefxProfileZoomFov );
		}
		if ( s_stefxProfiledP1LogBudget > 0 &&
			( controlChanged || cmd.serverTime - s_stefxProfiledP1LastLogTime >= 1000 ) )
		{
			XBLF( "STEFX_HM_P1_PROFILED_CMD: owner=native_loopback slot=0 port=%d style=%d time=%d move=(%d,%d,%d) buttons=0x%x weapon=%d weaponDelta=%d zoom=%g view=(%g,%g,%g)",
				stefxProfilePort,
				Cvar_VariableIntegerValue( "cont_config_0" ),
				cmd.serverTime,
				cmd.forwardmove, cmd.rightmove, cmd.upmove,
				cmd.buttons, cmd.weapon, stefxProfileWeaponDelta,
				stefxProfileZoomFov,
				cl.viewangles[PITCH], cl.viewangles[YAW], cl.viewangles[ROLL] );
			s_stefxProfiledP1LastLogTime = cmd.serverTime;
			s_stefxProfiledP1LastZoom = stefxProfileZoomFov;
			--s_stefxProfiledP1LogBudget;
		}
		s_stefxProfiledP1LastForwardState = forwardState;
		s_stefxProfiledP1LastRightState = rightState;
		s_stefxProfiledP1LastUp = cmd.upmove;
		s_stefxProfiledP1LastButtons = cmd.buttons;
		s_stefxProfiledP1LastWeapon = cmd.weapon;
	}
	else if ( Cvar_VariableIntegerValue( "stefx_hm_p1_profile_controls" ) )
	{
		Cvar_Set( "stefx_hm_p1_profile_controls", "0" );
		Cvar_Set( "stefx_hm_p1_profile_zoom_fov", "90" );
	}
#endif
	{
		++g_SPXBUsercmdCount;
		g_SPXBUsercmdTime = (unsigned int)cmd.serverTime;
		g_SPXBUsercmdMove = ((unsigned int)(unsigned char)cmd.forwardmove) |
			((unsigned int)(unsigned char)cmd.rightmove << 8) |
			((unsigned int)(unsigned char)cmd.upmove << 16) |
			((unsigned int)cmd.weapon << 24);
		g_SPXBUsercmdButtons = (unsigned int)cmd.buttons;
		g_SPXBUsercmdYaw = (unsigned int)cmd.angles[YAW];
	}

	{
		static int s_cmdLogBudget = 80;
		int logButtons = cmd.buttons & ~BUTTON_WALKING;
		qboolean interestingCmd =
			(cmd.forwardmove != 0) ||
			(cmd.rightmove != 0) ||
			(cmd.upmove != 0) ||
			(logButtons != 0) ||
			((int)oldAngles[YAW] != (int)cl.viewangles[YAW]) ||
			((int)oldAngles[PITCH] != (int)cl.viewangles[PITCH]);

		if ( s_cmdLogBudget > 0 && ( interestingCmd || s_cmdLogBudget > 72 ) )
		{
			Com_PrintfAlways("STEFX: CL_CreateCmd state=%d serverTime=%d frame_msec=%u cl_run=%d move=(%d,%d,%d) buttons=0x%x weapon=%d joy=(%d,%d,%d) view=(%g,%g,%g) old=(%g,%g,%g)\n",
				(int)cls.state,
				cmd.serverTime,
				frame_msec,
				cl_run ? cl_run->integer : -1,
				cmd.forwardmove,
				cmd.rightmove,
				cmd.upmove,
				cmd.buttons,
				cmd.weapon,
				cl.joystickAxis[AXIS_SIDE],
				cl.joystickAxis[AXIS_FORWARD],
				cl.joystickAxis[AXIS_UP],
				cl.viewangles[0],
				cl.viewangles[1],
				cl.viewangles[2],
				oldAngles[0],
				oldAngles[1],
				oldAngles[2]);
			s_cmdLogBudget--;
		}
	}
#endif
#if defined(STEFX_SP_HOSTED_MP)
	if (cmd.buttons & (BUTTON_ATTACK | BUTTON_ALT_ATTACK))
	{
		s_stefxHolomatchLastFireTime = Sys_Milliseconds();
	}
#endif

	// draw debug graphs of turning for mouse testing
#ifndef _XBOX
	if ( cl_debugMove->integer ) {
		if ( cl_debugMove->integer == 1 ) {
			SCR_DebugGraph( abs(cl.viewangles[YAW] - oldAngles[YAW]), 0 );
		}
		if ( cl_debugMove->integer == 2 ) {
			SCR_DebugGraph( abs(cl.viewangles[PITCH] - oldAngles[PITCH]), 0 );
		}
	}
#endif

	return cmd;
}


/*
=================
CL_CreateNewCommands

Create a new usercmd_t structure for this frame
=================
*/
void CL_CreateNewCommands( void ) {
	usercmd_t	*cmd;
	int			cmdNum;

	// no need to create usercmds until we have a gamestate
//	if ( cls.state < CA_PRIMED ) {
//		return;
//	}

	frame_msec = com_frameTime - old_com_frameTime;

	// if running less than 5fps, truncate the extra time to prevent
	// unexpected moves after a hitch
	if ( frame_msec > 200 ) {
		frame_msec = 200;
	}
	old_com_frameTime = com_frameTime;


	// generate a command for this frame
	cl.cmdNumber++;
	cmdNum = cl.cmdNumber & CMD_MASK;
	cl.cmds[cmdNum] = CL_CreateCmd ();
	cmd = &cl.cmds[cmdNum];
}

/*
=================
CL_ReadyToSendPacket

Returns qfalse if we are over the maxpackets limit
and should choke back the bandwidth a bit by not sending
a packet this frame.  All the commands will still get
delivered in the next packet, but saving a header and
getting more delta compression will reduce total bandwidth.
=================
*/
qboolean CL_ReadyToSendPacket( void ) {
	// don't send anything if playing back a demo
//	if ( cls.state == CA_CINEMATIC ) 
	if ( cls.state == CA_CINEMATIC || CL_IsRunningInGameCinematic())
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		static int s_stefxReadyCinemaBudget = 12;
		if ( s_stefxReadyCinemaBudget > 0 )
		{
			Com_PrintfAlways( "STEFX: CL_ReadyToSendPacket blocked cinematic state=%d inGameCin=%d cmd=%d realtime=%d\n",
				(int)cls.state,
				CL_IsRunningInGameCinematic() ? 1 : 0,
				cl.cmdNumber,
				cls.realtime );
			--s_stefxReadyCinemaBudget;
		}
#endif
		return qfalse;
	}

	// send every frame for loopbacks
	if (clc.netchan.remoteAddress.type == NA_LOOPBACK)
	{
		return qtrue;
	}

	// if we don't have a valid gamestate yet, only send
	// one packet a second
	if ( cls.state != CA_ACTIVE && cls.state != CA_PRIMED
		&& cls.realtime - clc.lastPacketSentTime < 1000 ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		static int s_stefxReadyThrottleBudget = 12;
		if ( s_stefxReadyThrottleBudget > 0 )
		{
			Com_PrintfAlways( "STEFX: CL_ReadyToSendPacket throttled state=%d remoteType=%d cmd=%d realtime=%d lastSent=%d\n",
				(int)cls.state,
				(int)clc.netchan.remoteAddress.type,
				cl.cmdNumber,
				cls.realtime,
				clc.lastPacketSentTime );
			--s_stefxReadyThrottleBudget;
		}
#endif
		return qfalse;
	}

	return qtrue;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

4	sequence number
2	qport
4	serverid
4	acknowledged sequence number
4	clc.serverCommandSequence
<optional reliable commands>
1	clc_move or clc_moveNoDelta
1	command count
<count * usercmds>

===================
*/
void CL_WritePacket( void ) {
	msg_t		buf;
	byte		data[MAX_MSGLEN];
	int			i, j;
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;
	int			packetNum;
	int			oldPacketNum;
	int			count;

	// don't send anything if playing back a demo
//	if ( cls.state == CA_CINEMATIC ) 
	if ( cls.state == CA_CINEMATIC || CL_IsRunningInGameCinematic())
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		static int s_stefxWriteCinemaBudget = 8;
		if ( s_stefxWriteCinemaBudget > 0 )
		{
			Com_PrintfAlways( "STEFX: CL_WritePacket skipped cinematic state=%d inGameCin=%d cmd=%d realtime=%d\n",
				(int)cls.state,
				CL_IsRunningInGameCinematic() ? 1 : 0,
				cl.cmdNumber,
				cls.realtime );
			--s_stefxWriteCinemaBudget;
		}
#endif
		return;
	}

	MSG_Init( &buf, data, sizeof(data) );

	// write any unacknowledged clientCommands
	for ( i = clc.reliableAcknowledge + 1 ; i <= clc.reliableSequence ; i++ ) {
		MSG_WriteByte( &buf, clc_clientCommand );
		MSG_WriteLong( &buf, i );
		MSG_WriteString( &buf, clc.reliableCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
	}

	// we want to send all the usercmds that were generated in the last
	// few packet, so even if a couple packets are dropped in a row,
	// all the cmds will make it to the server
	if ( cl_packetdup->integer < 0 ) {
		Cvar_Set( "cl_packetdup", "0" );
	} else if ( cl_packetdup->integer > 5 ) {
		Cvar_Set( "cl_packetdup", "5" );
	}
	oldPacketNum = (clc.netchan.outgoingSequence - 1 - cl_packetdup->integer) & PACKET_MASK;
	count = cl.cmdNumber - cl.packetCmdNumber[ oldPacketNum ];
	if ( count > MAX_PACKET_USERCMDS ) {
		count = MAX_PACKET_USERCMDS;
		Com_Printf("MAX_PACKET_USERCMDS\n");
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	{
		static int s_stefxWritePacketBudget = 64;
		if ( s_stefxWritePacketBudget > 0 && ( count > 0 || s_stefxWritePacketBudget > 56 ) )
		{
			usercmd_t *lastcmd = &cl.cmds[cl.cmdNumber & CMD_MASK];
			Com_PrintfAlways( "STEFX: CL_WritePacket state=%d remoteType=%d cmd=%d oldPacket=%d count=%d serverId=%d lastMove=(%d,%d,%d) lastButtons=0x%x lastWeapon=%d realtime=%d\n",
				(int)cls.state,
				(int)clc.netchan.remoteAddress.type,
				cl.cmdNumber,
				oldPacketNum,
				count,
				cl.serverId,
				lastcmd->forwardmove,
				lastcmd->rightmove,
				lastcmd->upmove,
				lastcmd->buttons,
				lastcmd->weapon,
				cls.realtime );
			--s_stefxWritePacketBudget;
		}
	}
#endif
	if ( count >= 1 ) {
		// begin a client move command
		MSG_WriteByte (&buf, clc_move);

		// write the last reliable message we received
		MSG_WriteLong( &buf, clc.serverCommandSequence );

		// write the current serverId so the server
		// can tell if this is from the current gameState
		MSG_WriteLong (&buf, cl.serverId);

		// write the current time
		MSG_WriteLong (&buf, cls.realtime);

		// let the server know what the last messagenum we
		// got was, so the next message can be delta compressed
		// FIXME: this could just be a bit flag, with the message implicit
		// from the unreliable ack of the netchan
		if (cl_nodelta->integer || !cl.frame.valid) {
			MSG_WriteLong (&buf, -1);	// no compression
		} else {
			MSG_WriteLong (&buf, cl.frame.messageNum);
		}

		// write the cmdNumber so the server can determine which ones it
		// has already received
		MSG_WriteLong( &buf, cl.cmdNumber );

		// write the command count
		MSG_WriteByte( &buf, count );

		// write all the commands, including the predicted command
		memset( &nullcmd, 0, sizeof(nullcmd) );
		oldcmd = &nullcmd;
		for ( i = 0 ; i < count ; i++ ) {
			j = (cl.cmdNumber - count + i + 1) & CMD_MASK;
			cmd = &cl.cmds[j];
			MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
			oldcmd = cmd;
		}
	}

	//
	// deliver the message
	//
	packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
	cl.packetTime[ packetNum ] = cls.realtime;
	cl.packetCmdNumber[ packetNum ] = cl.cmdNumber;
	clc.lastPacketSentTime = cls.realtime;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	{
		static int s_stefxTransmitBudget = 64;
		if ( s_stefxTransmitBudget > 0 && ( count > 0 || s_stefxTransmitBudget > 56 ) )
		{
			Com_PrintfAlways( "STEFX: CL_WritePacket transmit size=%d seq=%d packet=%d count=%d remoteType=%d qport=%d\n",
				buf.cursize,
				clc.netchan.outgoingSequence,
				packetNum,
				count,
				(int)clc.netchan.remoteAddress.type,
				clc.netchan.qport );
			--s_stefxTransmitBudget;
		}
	}
#endif
	Netchan_Transmit (&clc.netchan, buf.cursize, buf.data);	
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void ) {
	// don't send any message if not connected
	if ( cls.state < CA_CONNECTED ) {
		return;
	} 

	// don't send commands if paused
	if ( com_sv_running->integer && sv_paused->integer && cl_paused->integer ) {
		return;
	}

	// we create commands even if a demo is playing,
	CL_CreateNewCommands();

	// don't send a packet if the last packet was sent too recently
	if ( !CL_ReadyToSendPacket() ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		static int s_stefxSendBlockedBudget = 32;
		if ( s_stefxSendBlockedBudget > 0 )
		{
			Com_PrintfAlways( "STEFX: CL_SendCmd no packet state=%d cmd=%d remoteType=%d realtime=%d serverTime=%d\n",
				(int)cls.state,
				cl.cmdNumber,
				(int)clc.netchan.remoteAddress.type,
				cls.realtime,
				cl.serverTime );
			--s_stefxSendBlockedBudget;
		}
#endif
		return;
	}

#ifdef _XBOX
	{
		static int s_xboxSendLogs = 0;
		if (s_xboxSendLogs < 16 || cls.state < CA_LOADING)
		{
			Com_PrintfAlways("JA: CL_SendCmd write state=%d cmd=%d serverId=%d remoteType=%d realtime=%d lastSent=%d\n",
				(int)cls.state, cl.cmdNumber, cl.serverId,
				(int)clc.netchan.remoteAddress.type, cls.realtime, clc.lastPacketSentTime);
			++s_xboxSendLogs;
		}
	}
#endif
	CL_WritePacket();
}


/*
============
CL_InitInput
============
*/
void CL_InitInput( void ) {
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	//xbox hot swappable buttons
#ifdef _XBOX
	Cmd_AddCommand ("+hotswap1", IN_HotSwap1On);
	Cmd_AddCommand ("+hotswap2", IN_HotSwap2On);
	Cmd_AddCommand ("+hotswap3", IN_HotSwap3On);
	Cmd_AddCommand ("-hotswap1", IN_HotSwap1Off);
	Cmd_AddCommand ("-hotswap2", IN_HotSwap2Off);
	Cmd_AddCommand ("-hotswap3", IN_HotSwap3Off);
#endif
	Cmd_AddCommand ("useGivenForce", IN_UseGivenForce);
	//buttons
	Cmd_AddCommand ("+attack", IN_Button0Down);//attack
	Cmd_AddCommand ("-attack", IN_Button0Up);
	Cmd_AddCommand ("+force_lightning", IN_Button1Down);//force lightning
	Cmd_AddCommand ("-force_lightning", IN_Button1Up);
	Cmd_AddCommand ("+useforce", IN_Button2Down);	//use current force power
	Cmd_AddCommand ("-useforce", IN_Button2Up);
	Cmd_AddCommand ("+button2", IN_Button2Down);
	Cmd_AddCommand ("-button2", IN_Button2Up);
	Cmd_AddCommand ("+force_drain", IN_Button3Down);//force drain
	Cmd_AddCommand ("-force_drain", IN_Button3Up);
	Cmd_AddCommand ("+button3", IN_Button3Down);
	Cmd_AddCommand ("-button3", IN_Button3Up);
	Cmd_AddCommand ("+walk", IN_Button4Down);//walking
	Cmd_AddCommand ("-walk", IN_Button4Up);
	Cmd_AddCommand ("+use", IN_Button5Down);//use object
	Cmd_AddCommand ("-use", IN_Button5Up);
	Cmd_AddCommand ("+force_grip", IN_Button6Down);//force jump
	Cmd_AddCommand ("-force_grip", IN_Button6Up);
	Cmd_AddCommand ("+altattack", IN_Button7Down);//altattack
	Cmd_AddCommand ("-altattack", IN_Button7Up);
	Cmd_AddCommand ("+forcefocus", IN_Button8Down);//special saber attacks
	Cmd_AddCommand ("-forcefocus", IN_Button8Up);
	//end buttons
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
	cl_debugMove = Cvar_Get ("cl_debugMove", "0", 0);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_InitSmokeInputCvars();
#endif
}

