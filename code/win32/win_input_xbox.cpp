// win_input.c -- win32 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

// leave this as first line for PCH reasons...
//
// #include "../server/exe_headers.h"

#include <xtl.h>

#include "../client/client.h"

#include "../qcommon/qcommon.h"
#ifdef _JK2MP
#include "../ui/keycodes.h"
#else
#include "../client/keycodes.h"
#endif

#include "win_local.h"
#include "win_input.h"
#include "xb_log.h"

#define IN_MAX_CONTROLLERS 4

extern "C" volatile unsigned int g_SPXBInputPollCount;
extern "C" volatile unsigned int g_SPXBInputPort;
extern "C" volatile unsigned int g_SPXBInputDigital;
extern "C" volatile unsigned int g_SPXBInputAnalogMask;
extern "C" volatile unsigned int g_SPXBInputLXLY;
extern "C" volatile unsigned int g_SPXBInputRXRY;
extern "C" volatile unsigned int g_SPXBInputPollFailureCount;
extern "C" volatile unsigned int g_SPXBInputPollLastResult;
extern "C" volatile unsigned int g_SPXBInputPollRecoveryCount;
extern "C" volatile unsigned int g_SPXBInputMenuEdgeCount;
extern "C" volatile unsigned int g_SPXBInputMenuEdgeLast;
extern "C" volatile unsigned int g_SPXBInputCommonPressCount;
extern "C" volatile unsigned int g_SPXBInputCommonPressLast;
extern "C" volatile unsigned int g_SPXBInputFrontendQueueCount;
extern "C" volatile unsigned int g_SPXBInputFrontendQueueLast;
extern "C" volatile unsigned int g_SPXBInputDispatchCount;
extern "C" volatile unsigned int g_SPXBInputDispatchLast;
extern "C" volatile unsigned int g_SPXBInputDispatchHandled;
extern "C" volatile unsigned int g_SPXBHMInfoDispatchCount;
extern "C" volatile unsigned int g_SPXBHMInfoDispatchLast;
extern "C" volatile unsigned int g_SPXBHMGameCommandCount;
extern "C" volatile unsigned int g_SPXBHMGameCommandResult;
extern "C" volatile unsigned int g_SPXBHMConsoleCommandCount;
extern "C" volatile unsigned int g_SPXBHMConsoleCommandTag;
extern "C" volatile unsigned int g_SPXBHMScoresDownCount;
extern "C" volatile unsigned int g_SPXBHMScoresUpCount;
extern "C" volatile unsigned int g_SPXBHMScoreDrawCount;
extern "C" volatile unsigned int g_SPXBHMScoreDrawState;
#if defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBRenderCommandHighWater;
extern "C" volatile unsigned int g_SPXBRenderCommandDrops;
extern "C" volatile unsigned int g_SPXBRenderCommandLastDrop;
extern "C" volatile unsigned int g_SPXBRenderCommandCalls;
extern "C" volatile unsigned int g_SPXBRenderCommandIssueCount;
extern "C" volatile unsigned int g_SPXBRenderCommandLastUsed;
extern "C" volatile unsigned int g_SPXBHMScoreStretchCount;
extern "C" volatile unsigned int g_SPXBHMScoreStretchShader;
extern "C" volatile unsigned int g_SPXBHMScoreStretchX;
extern "C" volatile unsigned int g_SPXBHMScoreStretchY;
extern "C" volatile unsigned int g_SPXBHMScoreStretchW;
extern "C" volatile unsigned int g_SPXBHMScoreStretchH;
extern "C" volatile unsigned int g_SPXBHMScoreScaleX;
extern "C" volatile unsigned int g_SPXBHMScoreScaleY;
extern "C" volatile unsigned int g_SPXBHMScoreWhiteShader;
extern "C" volatile unsigned int g_SPXBHMScoreQueuedCount;
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
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceFlags;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceVerts;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceIndexes;
extern "C" volatile unsigned int g_SPXBHMScoreSurfacePasses;
extern "C" volatile unsigned int g_SPXBHMScoreSurfaceState;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitArmed;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitCalls;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitIndexes;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitState;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitTexture;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissor;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissorXY;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissorWH;
#endif

void IN_UIEmptyQueue();
void IN_CheckForNoControllers();

struct inputstate_t
{
	struct controller_t
	{
		HANDLE handle;
		XINPUT_STATE state;
		WORD rumbleLeft, rumbleRight;  // newest request, never driver-owned
		int rumblePacket;             // 1-based packet for this handle, or 0
		bool rumbleImmediate;         // sync success need not update the async status
	};
	controller_t controllers[IN_MAX_CONTROLLERS];
};

inputstate_t *in_state = NULL;
static cvar_t *joy_deadzone = NULL;

// Driver-owned until Header.dwStatus leaves ERROR_IO_PENDING. Storage must
// outlive a handle (and an input restart). A spare lets a newly plugged pad
// run even if its predecessor's cancelled packet has not retired yet.
enum { IN_RUMBLE_PACKETS_PER_PORT = 2 };
static XINPUT_FEEDBACK s_rumblePackets[IN_MAX_CONTROLLERS][IN_RUMBLE_PACKETS_PER_PORT];

static DWORD IN_RumblePacketStatus(const volatile XINPUT_FEEDBACK &packet)
{
	return packet.Header.dwStatus;
}


/*
=========================================================================

JOYSTICK

=========================================================================
*/
//JLF moved here for multiple access (then not used multiple times. oh well.)
extern bool noControllersConnected;
// Process all the insertions and removals, updating handles and such
void IN_ProcessChanges(DWORD dwInsert, DWORD dwRemove)
{
	if (dwInsert || dwRemove)
	{
		XBLF("STEFX: IN_ProcessChanges insert=0x%08x remove=0x%08x\n", dwInsert, dwRemove);
	}

	for(int port = 0; port < IN_MAX_CONTROLLERS; ++port)
	{
		// Close removals.
		if((1 << port) & dwRemove)
		{
			if ( in_state->controllers[port].handle )
			{
				IN_KillRumbleScripts(port);
				XInputClose( in_state->controllers[port].handle );
				in_state->controllers[port].handle = 0;
				in_state->controllers[port].rumblePacket = 0;
			}
#if defined(STEFX_ELITE_FORCE_SP)
			CL_STEFX_SplitScreen_RecordPadState( port, qfalse, IN_GetMainController(), 0, NULL, 0, 0, 0, 0 );
#endif
			IN_PadUnplugged(port);

		}

		// Open insertions.
		if( (1 << port) & dwInsert )
		{
			// Do not send effects from the previous occupant (or the time
			// this port was empty) to this controller.
			IN_KillRumbleScripts(port);
			in_state->controllers[port].rumblePacket = 0;
			in_state->controllers[port].handle = XInputOpen( XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, NULL );
			XBLF("STEFX: controller %d open handle=%p\n", port, in_state->controllers[port].handle);
			IN_PadPlugged(port);
		}
	}

	return;
}

/*********
IN_CheckForNoControllers()
If there are no controllers plugged in, the UI
is notified so it can display an appropriate
message.
*********/
void IN_CheckForNoControllers()
{
	extern bool noControllersConnected;
	if(!noControllersConnected)
	{
		extern bool wasPlugged[4];
		if( !wasPlugged[0] &&
			!wasPlugged[1] &&
			!wasPlugged[2] &&
			!wasPlugged[3] )
		{
			// Tell the UI that there are no controllers connected
		//	VM_Call( uivm, UI_CONTROLLER_UNPLUGGED, true, -1);
			noControllersConnected = true;
		}
	}
}

/*
=========================================================================

  RUMBLE SUPPORT

=========================================================================
*/

void IN_RumbleAdjust(int controller, int left, int right)
{
	assert(controller >= 0 && controller < IN_MAX_CONTROLLERS);
	if (!in_state) return;
	in_state->controllers[controller].rumbleLeft = (WORD)left;
	in_state->controllers[controller].rumbleRight = (WORD)right;
}

static void IN_FlushRumble(int controller)
{
	int localSlot;
	char vibrationCvar[32];
	static unsigned int s_stefxRumbleDisabledMask = 0;

	inputstate_t::controller_t &pad = in_state->controllers[controller];
	if (!pad.handle) return;
	if ((pad.rumbleLeft || pad.rumbleRight) && Cvar_VariableIntegerValue("stefx_splitScreen"))
	{
		localSlot = CL_STEFX_SplitScreen_LocalSlotForPad( controller );
		if ( localSlot >= 0 )
		{
			Com_sprintf( vibrationCvar, sizeof( vibrationCvar ), "joy_vibestate_%d", localSlot );
			if ( !Cvar_VariableIntegerValue( vibrationCvar ) )
			{
				pad.rumbleLeft = pad.rumbleRight = 0;
				if ( !( s_stefxRumbleDisabledMask & ( 1u << localSlot ) ) )
				{
					XBLF( "STEFX_HM_SPLIT_RUMBLE: slot=%d port=%d enabled=0", localSlot, controller );
					s_stefxRumbleDisabledMask |= 1u << localSlot;
				}
			}
			else
			{
				s_stefxRumbleDisabledMask &= ~( 1u << localSlot );
			}
		}
	}

	int slot = pad.rumblePacket - 1;
	XINPUT_FEEDBACK *packets = s_rumblePackets[controller];
	if (slot >= 0)
	{
		const DWORD status = IN_RumblePacketStatus(packets[slot]);
		if (status == ERROR_IO_PENDING) return;  // never overwrite in flight
		if ((pad.rumbleImmediate || status == ERROR_SUCCESS) &&
			packets[slot].Rumble.wLeftMotorSpeed == pad.rumbleLeft &&
			packets[slot].Rumble.wRightMotorSpeed == pad.rumbleRight)
			return;  // already applied to this handle
	}
	else
	{
		for (slot = 0; slot < IN_RUMBLE_PACKETS_PER_PORT; ++slot)
		{
			if (IN_RumblePacketStatus(packets[slot]) != ERROR_IO_PENDING) break;
		}
		if (slot == IN_RUMBLE_PACKETS_PER_PORT) return;
	}

	XINPUT_FEEDBACK &fb = packets[slot];
	fb.Rumble.wLeftMotorSpeed = pad.rumbleLeft;
	fb.Rumble.wRightMotorSpeed = pad.rumbleRight;
	const DWORD result = XInputSetState(pad.handle, &fb);
	pad.rumbleImmediate = (result == ERROR_SUCCESS);
	pad.rumblePacket = (result == ERROR_IO_PENDING || result == ERROR_SUCCESS) ? slot + 1 : 0;
}


/*
=========================================================================

=========================================================================
*/

/*
igBool IN_WindowClose(igWindow *window)
{
	SV_Shutdown ("Server quit\n");
	CL_Shutdown ();
	Com_Shutdown ();
	Sys_Quit ();
	return true;
}
*/

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown( void ) {
	IN_RumbleShutdown();

	if (in_state)
	{
		for (int port = 0; port < IN_MAX_CONTROLLERS; ++port)
		{
			if (!in_state->controllers[port].handle) continue;
			IN_RumbleAdjust(port, 0, 0);
			IN_FlushRumble(port);
			XInputClose(in_state->controllers[port].handle);
		}
	}
	delete in_state;
	in_state = NULL;
}


/*
===========
IN_Init
===========
*/
void IN_Init( void )
{
		g_SPXBInputPollFailureCount = 0;
		g_SPXBInputPollLastResult = 0;
		g_SPXBInputPollRecoveryCount = 0;
		g_SPXBInputMenuEdgeCount = 0;
		g_SPXBInputMenuEdgeLast = 0;
		g_SPXBInputCommonPressCount = 0;
		g_SPXBInputCommonPressLast = 0;
		g_SPXBInputFrontendQueueCount = 0;
		g_SPXBInputFrontendQueueLast = 0;
		g_SPXBInputDispatchCount = 0;
		g_SPXBInputDispatchLast = 0;
		g_SPXBInputDispatchHandled = 0;
		g_SPXBHMInfoDispatchCount = 0;
		g_SPXBHMInfoDispatchLast = 0;
		g_SPXBHMGameCommandCount = 0;
		g_SPXBHMGameCommandResult = 0;
		g_SPXBHMConsoleCommandCount = 0;
		g_SPXBHMConsoleCommandTag = 0;
		g_SPXBHMScoresDownCount = 0;
		g_SPXBHMScoresUpCount = 0;
		g_SPXBHMScoreDrawCount = 0;
		g_SPXBHMScoreDrawState = 0;
#if defined(STEFX_HM_SCORE_DIAGNOSTICS)
		g_SPXBRenderCommandHighWater = 0;
		g_SPXBRenderCommandDrops = 0;
		g_SPXBRenderCommandLastDrop = 0;
		g_SPXBRenderCommandCalls = 0;
		g_SPXBRenderCommandIssueCount = 0;
		g_SPXBRenderCommandLastUsed = 0;
		g_SPXBHMScoreStretchCount = 0;
		g_SPXBHMScoreStretchShader = 0;
		g_SPXBHMScoreStretchX = 0;
		g_SPXBHMScoreStretchY = 0;
		g_SPXBHMScoreStretchW = 0;
		g_SPXBHMScoreStretchH = 0;
		g_SPXBHMScoreScaleX = 0;
		g_SPXBHMScoreScaleY = 0;
		g_SPXBHMScoreWhiteShader = 0;
		g_SPXBHMScoreQueuedCount = 0;
		g_SPXBHMScoreQueuedShader = 0;
		g_SPXBHMScoreBackendMatches = 0;
		g_SPXBHMScoreBackendColor = 0;
		g_SPXBRenderBackendCommandCount = 0;
		g_SPXBRenderBackendStretchCount = 0;
		g_SPXBRenderBackendTerminalId = 0;
		g_SPXBRenderBackendBytes = 0;
		g_SPXBHMScoreBackendGeometry = 0;
		g_SPXBHMScoreBackendGeomShader = 0;
		g_SPXBHMScoreBackendGeomColor = 0;
		g_SPXBRenderBackendDoneCommands = 0;
		g_SPXBRenderBackendDoneStretches = 0;
		g_SPXBRenderBackendDoneTerminal = 0;
		g_SPXBRenderBackendDoneBytes = 0;
		g_SPXBHMScoreBackendDoneGeometry = 0;
		g_SPXBHMScoreBackendDoneShader = 0;
		g_SPXBHMScoreBackendDoneColor = 0;
		g_SPXBHMScoreBatchPending = 0;
		g_SPXBHMScoreSurfaceFlags = 0;
		g_SPXBHMScoreSurfaceVerts = 0;
		g_SPXBHMScoreSurfaceIndexes = 0;
		g_SPXBHMScoreSurfacePasses = 0;
		g_SPXBHMScoreSurfaceState = 0;
		g_SPXBHMScoreSubmitArmed = 0;
		g_SPXBHMScoreSubmitCalls = 0;
		g_SPXBHMScoreSubmitIndexes = 0;
		g_SPXBHMScoreSubmitState = 0;
		g_SPXBHMScoreSubmitTexture = 0;
		g_SPXBHMScoreSubmitScissor = 0;
		g_SPXBHMScoreSubmitScissorXY = 0;
		g_SPXBHMScoreSubmitScissorWH = 0;
#endif

		in_state = new inputstate_t;

		// Initialize support for 4 gamepads
		XDEVICE_PREALLOC_TYPE xdpt[] = {
			{XDEVICE_TYPE_GAMEPAD, 4}
		};

	// Shipping JA initializes peripherals from the input subsystem, after the
	// renderer has been created. XInitDevices may only be called once.
	static bool bInputInitialized = false;
	if (!bInputInitialized) {
		XBLog_WriteCritical("STEFX_HW_BOOT: input XInitDevices begin after renderer init");
		XInitDevices( sizeof(xdpt) / sizeof(XDEVICE_PREALLOC_TYPE), xdpt );
		bInputInitialized = true;
		XBLog_WriteCritical("STEFX_HW_BOOT: input XInitDevices complete after renderer init");
	}

		// Zero all of our data, including handles
		memset(in_state->controllers, 0, sizeof(in_state->controllers));

		// Find out the status of all gamepad ports, then open them
		DWORD deviceMask = XGetDevices( XDEVICE_TYPE_GAMEPAD );
		XBLF("STEFX: IN_Init gamepad mask=0x%08x\n", deviceMask);
		IN_ProcessChanges( deviceMask, 0 );
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: initial controller discovery complete");
#endif

		joy_deadzone = Cvar_Get( "joy_deadzone", "0.18", CVAR_ARCHIVE );
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: input cvars ready");
#endif

		IN_RumbleInit();
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: rumble init complete");
#endif
	}

static inline float _joyAxisConvert(SHORT x)
{
	// Change scale
	float y = x / 32767.0;
	float deadzone = joy_deadzone ? joy_deadzone->value : 0.18f;

	if (deadzone < 0.0f)
	{
		deadzone = 0.0f;
	}
	else if (deadzone > 0.95f)
	{
		deadzone = 0.95f;
	}

	if(fabs(y) < deadzone)
	{
		y = 0.0f;
	}

	return y;
}

// How many controls on the xbox gamepad?
#define IN_NUM_DIGITAL_BUTTONS 8
#define IN_NUM_ANALOG_BUTTONS 8
// Cutoff where the analog buttons are considered to be "pressed"
// This should be smarter.
/* Match the retail Xbox crosstalk contract and the split-player path. */
#define IN_ANALOG_BUTTON_THRESHOLD XINPUT_GAMEPAD_MAX_CROSSTALK

#if defined(STEFX_ELITE_FORCE_SP)
static WORD s_stefxLastMenuButtons[IN_MAX_CONTROLLERS] = { 0, 0, 0, 0 };
static bool s_stefxMenuButtonsPrimed[IN_MAX_CONTROLLERS] = { false, false, false, false };

static void IN_STEFX_UpdateMenuButtonEdge(int port, WORD changed, WORD current, WORD mask, fakeAscii_t button, const char *name)
{
	if (changed & mask)
	{
		const bool pressed = (current & mask) != 0;
		g_SPXBInputMenuEdgeCount++;
		g_SPXBInputMenuEdgeLast = ((unsigned int)button & 0xffffu) |
			(pressed ? 0x00010000u : 0u) |
			(((unsigned int)port & 0xffu) << 24);
		XBLF("STEFX_INPUT_MENU_RAW_DISPATCH port=%d name='%s' mask=0x%04x fakeAscii=%d pressed=%d main=%d state=%d catcher=0x%x",
			port,
			name,
			(unsigned int)mask,
			(int)button,
			pressed ? 1 : 0,
			IN_GetMainController(),
			(int)cls.state,
			(unsigned int)cls.keyCatchers);
		XBLF("STEFX_MENU_INPUT raw port=%d name='%s' fakeAscii=%d pressed=%d buttons=0x%04x state=%d catcher=0x%x",
			port,
			name,
			(int)button,
			pressed ? 1 : 0,
			(unsigned int)current,
			(int)cls.state,
			(unsigned int)cls.keyCatchers);
		IN_CommonJoyPress(port, button, pressed);
	}
}

static void IN_STEFX_UpdateMenuButtons(int port, WORD buttons)
{
	const WORD menuMask = XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK;
	const WORD current = buttons & menuMask;
	WORD changed;

	if (!s_stefxMenuButtonsPrimed[port])
	{
		s_stefxLastMenuButtons[port] = current;
		s_stefxMenuButtonsPrimed[port] = true;
		XBLF("STEFX_INPUT_MENU_PRIME port=%d buttons=0x%04x main=%d state=%d catcher=0x%x",
			port,
			(unsigned int)current,
			IN_GetMainController(),
			(int)cls.state,
			(unsigned int)cls.keyCatchers);
		return;
	}

	changed = (WORD)(current ^ s_stefxLastMenuButtons[port]);
	if (!changed)
	{
		return;
	}

	XBLF("STEFX_INPUT_MENU_RAW port=%d buttons=0x%04x last=0x%04x changed=0x%04x main=%d state=%d catcher=0x%x",
		port,
		(unsigned int)current,
		(unsigned int)s_stefxLastMenuButtons[port],
		(unsigned int)changed,
		IN_GetMainController(),
		(int)cls.state,
		(unsigned int)cls.keyCatchers);

	s_stefxLastMenuButtons[port] = current;
	IN_STEFX_UpdateMenuButtonEdge(port, changed, current, XINPUT_GAMEPAD_START, A_JOY4, "START");
	IN_STEFX_UpdateMenuButtonEdge(port, changed, current, XINPUT_GAMEPAD_BACK, A_JOY1, "BACK");
}
#endif

void IN_UpdateGamepad(int port)
{
	static bool loggedFirstState[IN_MAX_CONTROLLERS] = { false, false, false, false };
	static bool pollFailed[IN_MAX_CONTROLLERS] = { false, false, false, false };
	static int pollFailureLogBudget = 32;
#if defined(STEFX_ELITE_FORCE_SP)
	static int activeStateLogBudget[IN_MAX_CONTROLLERS] = { 16, 16, 16, 16 };
	static int splitSecondaryLogBudget[IN_MAX_CONTROLLERS] = { 16, 16, 16, 16 };
	static bool s_primaryBindingsLogged = false;
	static int s_primaryAnalogSampleLogBudget = 96;
	static int s_primaryAnalogEdgeLogBudget = 96;
#endif
	// Lookup table to convert the digital buttons to fakeAscii_t, in mask order
	const fakeAscii_t digitalXlat[IN_NUM_DIGITAL_BUTTONS] = {
		A_JOY5, // DPAD_UP
		A_JOY7, // DPAD_DOWN
		A_JOY8, // DPAD_LEFT
		A_JOY6, // DPAD_RIGHT
		A_JOY4, // Start
		A_JOY1, // Back
		A_JOY2, // Left stick
		A_JOY3  // Right stick
	};

	// Lookup table to convet the analog buttons to fakeAscii_t, in DX order
	const fakeAscii_t analogXlat[IN_NUM_ANALOG_BUTTONS] = {
		A_JOY15, // A
		A_JOY14, // B
		A_JOY16, // X
		A_JOY13, // Y
		A_JOY10, // Black
		A_JOY9,  // White
		A_JOY11, // Left trigger
		A_JOY12  // Right trigger
	};

	// Get new state
	XINPUT_STATE newState;
	DWORD inputResult;

	memset(&newState, 0, sizeof(newState));
	++g_SPXBInputPollCount;
	g_SPXBInputPort = (unsigned int)port;
	inputResult = XInputGetState( in_state->controllers[port].handle, &newState );
	if (inputResult != ERROR_SUCCESS)
	{
		++g_SPXBInputPollFailureCount;
		g_SPXBInputPollLastResult = (unsigned int)inputResult;
		if (!pollFailed[port] || pollFailureLogBudget > 0)
		{
			XBLog_WriteCriticalf("STEFX_INPUT_POLL_FAILURE: port=%d result=0x%08x failures=%u preservingPacket=%u preservingSticks=(%d,%d,%d,%d)",
				port,
				(unsigned int)inputResult,
				(unsigned int)g_SPXBInputPollFailureCount,
				(unsigned int)in_state->controllers[port].state.dwPacketNumber,
				(int)in_state->controllers[port].state.Gamepad.sThumbLX,
				(int)in_state->controllers[port].state.Gamepad.sThumbLY,
				(int)in_state->controllers[port].state.Gamepad.sThumbRX,
				(int)in_state->controllers[port].state.Gamepad.sThumbRY);
			if (pollFailureLogBudget > 0)
			{
				--pollFailureLogBudget;
			}
		}
		pollFailed[port] = true;
		/* Keep both the legacy input state and the split-player cache at their
		 * last valid sample. Consuming the failed call's output caused random
		 * look snaps and one-frame movement releases on retail hardware. */
		return;
	}
	if (pollFailed[port])
	{
		++g_SPXBInputPollRecoveryCount;
		XBLog_WriteCriticalf("STEFX_INPUT_POLL_RECOVERY: port=%d packet=%u recoveries=%u failures=%u",
			port,
			(unsigned int)newState.dwPacketNumber,
			(unsigned int)g_SPXBInputPollRecoveryCount,
			(unsigned int)g_SPXBInputPollFailureCount);
		pollFailed[port] = false;
	}
	{
		unsigned int analogMask = 0;
		int inputIndex;
		for (inputIndex = 0; inputIndex < IN_NUM_ANALOG_BUTTONS; ++inputIndex)
		{
			if (newState.Gamepad.bAnalogButtons[inputIndex] > IN_ANALOG_BUTTON_THRESHOLD)
			{
				analogMask |= 1u << inputIndex;
			}
		}
		g_SPXBInputDigital = (unsigned int)newState.Gamepad.wButtons;
		g_SPXBInputAnalogMask = analogMask;
		g_SPXBInputLXLY = ((unsigned int)(unsigned short)newState.Gamepad.sThumbLX) |
			((unsigned int)(unsigned short)newState.Gamepad.sThumbLY << 16);
		g_SPXBInputRXRY = ((unsigned int)(unsigned short)newState.Gamepad.sThumbRX) |
			((unsigned int)(unsigned short)newState.Gamepad.sThumbRY << 16);
	}
#if defined(STEFX_ELITE_FORCE_SP)
	CL_STEFX_SplitScreen_RecordPadState( port, qtrue, IN_GetMainController(), newState.Gamepad.wButtons, newState.Gamepad.bAnalogButtons,
		newState.Gamepad.sThumbLX, newState.Gamepad.sThumbLY, newState.Gamepad.sThumbRX, newState.Gamepad.sThumbRY );
	if (Cvar_VariableIntegerValue("stefx_splitScreen"))
	{
		static int s_splitPrimaryClaimLogBudget = 16;
		const int splitPrimary = CL_STEFX_SplitScreen_PrimaryPadForMainController();
		if (splitPrimary >= 0 && splitPrimary != IN_GetMainController())
		{
			if (s_splitPrimaryClaimLogBudget > 0)
			{
				XBLF("STEFX_SPLIT_INPUT main controller reassigned old=%d new=%d activePort=%d state=%d catcher=0x%x",
					IN_GetMainController(),
					splitPrimary,
					port,
					(int)cls.state,
					(unsigned int)cls.keyCatchers);
				--s_splitPrimaryClaimLogBudget;
			}
			IN_SetMainController(splitPrimary);
		}
	}
#endif
	if (!loggedFirstState[port])
	{
		XBLF("STEFX: first gamepad state port=%d buttons=0x%04x A=%u B=%u X=%u Y=%u LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d\n",
			port,
			newState.Gamepad.wButtons,
			newState.Gamepad.bAnalogButtons[0],
			newState.Gamepad.bAnalogButtons[1],
			newState.Gamepad.bAnalogButtons[2],
			newState.Gamepad.bAnalogButtons[3],
			newState.Gamepad.bAnalogButtons[6],
			newState.Gamepad.bAnalogButtons[7],
			newState.Gamepad.sThumbLX,
			newState.Gamepad.sThumbLY,
			newState.Gamepad.sThumbRX,
			newState.Gamepad.sThumbRY);
		loggedFirstState[port] = true;
	}
#if defined(STEFX_ELITE_FORCE_SP)
	if (activeStateLogBudget[port] > 0 &&
		(newState.Gamepad.wButtons ||
		 newState.Gamepad.bAnalogButtons[0] ||
		 newState.Gamepad.bAnalogButtons[1] ||
		 newState.Gamepad.bAnalogButtons[2] ||
		 newState.Gamepad.bAnalogButtons[3] ||
		 newState.Gamepad.bAnalogButtons[6] ||
		 newState.Gamepad.bAnalogButtons[7] ||
		 newState.Gamepad.sThumbLX ||
		 newState.Gamepad.sThumbLY ||
		 newState.Gamepad.sThumbRX ||
		 newState.Gamepad.sThumbRY))
	{
		XBLF("STEFX: active gamepad port=%d buttons=0x%04x A=%u B=%u X=%u Y=%u LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d",
			port,
			newState.Gamepad.wButtons,
			newState.Gamepad.bAnalogButtons[0],
			newState.Gamepad.bAnalogButtons[1],
			newState.Gamepad.bAnalogButtons[2],
			newState.Gamepad.bAnalogButtons[3],
			newState.Gamepad.bAnalogButtons[6],
			newState.Gamepad.bAnalogButtons[7],
			newState.Gamepad.sThumbLX,
			newState.Gamepad.sThumbLY,
			newState.Gamepad.sThumbRX,
			newState.Gamepad.sThumbRY);
		activeStateLogBudget[port]--;
	}
#endif

#if defined(STEFX_ELITE_FORCE_SP)
	if (Cvar_VariableIntegerValue("stefx_splitScreen") && CL_STEFX_SplitScreen_ShouldReservePadForP2(port, IN_GetMainController()))
	{
		IN_STEFX_UpdateMenuButtons(port, newState.Gamepad.wButtons);
		if (splitSecondaryLogBudget[port] > 0)
		{
			XBLF("STEFX_SPLIT_INPUT secondary pad reserved for P2 port=%d rawMain=%d buttons=0x%04x A=%u B=%u X=%u Y=%u LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d state=%d catcher=0x%x",
				port,
				IN_GetMainController(),
				newState.Gamepad.wButtons,
				newState.Gamepad.bAnalogButtons[0],
				newState.Gamepad.bAnalogButtons[1],
				newState.Gamepad.bAnalogButtons[2],
				newState.Gamepad.bAnalogButtons[3],
				newState.Gamepad.bAnalogButtons[6],
				newState.Gamepad.bAnalogButtons[7],
				newState.Gamepad.sThumbLX,
				newState.Gamepad.sThumbLY,
				newState.Gamepad.sThumbRX,
				newState.Gamepad.sThumbRY,
				(int)cls.state,
				(unsigned int)cls.keyCatchers);
			splitSecondaryLogBudget[port]--;
		}
		in_state->controllers[port].state = newState;
		return;
	}
#endif

	// Get old state
	XINPUT_STATE &oldState(in_state->controllers[port].state);

#if defined(STEFX_ELITE_FORCE_SP)
	IN_STEFX_UpdateMenuButtons(port, newState.Gamepad.wButtons);
	if (!s_primaryBindingsLogged &&
		port == IN_GetMainController() &&
		Cvar_VariableIntegerValue("stefx_splitScreen") &&
		cls.state == CA_ACTIVE)
	{
		XBLog_WriteCriticalf("STEFX_HM_P1_BINDINGS: BACK='%s' START='%s' A='%s' B='%s' X='%s' Y='%s' BLACK='%s' WHITE='%s' LT='%s' RT='%s' mPitch=%g stickMode=%d profilePitch=%g",
			Key_GetBinding(A_JOY1) ? Key_GetBinding(A_JOY1) : "",
			Key_GetBinding(A_JOY4) ? Key_GetBinding(A_JOY4) : "",
			Key_GetBinding(A_JOY15) ? Key_GetBinding(A_JOY15) : "",
			Key_GetBinding(A_JOY14) ? Key_GetBinding(A_JOY14) : "",
			Key_GetBinding(A_JOY16) ? Key_GetBinding(A_JOY16) : "",
			Key_GetBinding(A_JOY13) ? Key_GetBinding(A_JOY13) : "",
			Key_GetBinding(A_JOY10) ? Key_GetBinding(A_JOY10) : "",
			Key_GetBinding(A_JOY9) ? Key_GetBinding(A_JOY9) : "",
			Key_GetBinding(A_JOY11) ? Key_GetBinding(A_JOY11) : "",
			Key_GetBinding(A_JOY12) ? Key_GetBinding(A_JOY12) : "",
			Cvar_VariableValue("m_pitch"),
			Cvar_VariableIntegerValue("ui_thumbStickMode"),
			Cvar_VariableValue("joy_pitchsensitivity_0"));
		s_primaryBindingsLogged = true;
	}
#endif

	int buttonIdx;
	bool oldPressed, newPressed;

	// Check all digital buttons first
	for (buttonIdx = 0; buttonIdx < IN_NUM_DIGITAL_BUTTONS; ++buttonIdx)
	{
#if defined(STEFX_ELITE_FORCE_SP)
		if (buttonIdx == 4 || buttonIdx == 5)
		{
			continue;
		}
#endif
		oldPressed = oldState.Gamepad.wButtons & (1 << buttonIdx);
		newPressed = newState.Gamepad.wButtons & (1 << buttonIdx);

		if (oldPressed != newPressed)
		{
#if defined(STEFX_ELITE_FORCE_SP)
			if (buttonIdx == 4 || buttonIdx == 5)
			{
				XBLF("STEFX_INPUT_MENU_DIGITAL_EDGE port=%d buttonIdx=%d fakeAscii=%d pressed=%d state=%d catcher=0x%x",
					port,
					buttonIdx,
					(int)digitalXlat[buttonIdx],
					newPressed ? 1 : 0,
					(int)cls.state,
					(unsigned int)cls.keyCatchers);
			}
#endif
			IN_CommonJoyPress(port, digitalXlat[buttonIdx], newPressed);
		}
	}

	// Now check all analog buttons
	for (buttonIdx = 0; buttonIdx < IN_NUM_ANALOG_BUTTONS; ++buttonIdx)
	{
		oldPressed = oldState.Gamepad.bAnalogButtons[buttonIdx] > IN_ANALOG_BUTTON_THRESHOLD;
		newPressed = newState.Gamepad.bAnalogButtons[buttonIdx] > IN_ANALOG_BUTTON_THRESHOLD;

#if defined(STEFX_ELITE_FORCE_SP)
		if (s_primaryAnalogSampleLogBudget > 0 &&
			port == IN_GetMainController() &&
			Cvar_VariableIntegerValue("stefx_splitScreen") &&
			oldState.Gamepad.bAnalogButtons[buttonIdx] != newState.Gamepad.bAnalogButtons[buttonIdx] &&
			(oldState.Gamepad.bAnalogButtons[buttonIdx] == 0 ||
			 newState.Gamepad.bAnalogButtons[buttonIdx] == 0 ||
			 (oldState.Gamepad.bAnalogButtons[buttonIdx] >> 4) != (newState.Gamepad.bAnalogButtons[buttonIdx] >> 4)))
		{
			XBLog_WriteCriticalf("STEFX_HM_P1_ANALOG_SAMPLE: index=%d key=%d rawOld=%u rawNew=%u threshold=%d binding='%s' state=%d catcher=0x%x",
				buttonIdx,
				(int)analogXlat[buttonIdx],
				(unsigned int)oldState.Gamepad.bAnalogButtons[buttonIdx],
				(unsigned int)newState.Gamepad.bAnalogButtons[buttonIdx],
				IN_ANALOG_BUTTON_THRESHOLD,
				Key_GetBinding(analogXlat[buttonIdx]),
				(int)cls.state,
				(unsigned int)cls.keyCatchers);
			--s_primaryAnalogSampleLogBudget;
		}
#endif

		if (oldPressed != newPressed)
		{
#if defined(STEFX_ELITE_FORCE_SP)
			if (s_primaryAnalogEdgeLogBudget > 0 &&
				port == IN_GetMainController() &&
				Cvar_VariableIntegerValue("stefx_splitScreen"))
			{
				XBLog_WriteCriticalf("STEFX_HM_P1_ANALOG_EDGE: index=%d key=%d pressed=%d rawOld=%u rawNew=%u threshold=%d binding='%s' state=%d catcher=0x%x",
					buttonIdx,
					(int)analogXlat[buttonIdx],
					newPressed ? 1 : 0,
					(unsigned int)oldState.Gamepad.bAnalogButtons[buttonIdx],
					(unsigned int)newState.Gamepad.bAnalogButtons[buttonIdx],
					IN_ANALOG_BUTTON_THRESHOLD,
					Key_GetBinding(analogXlat[buttonIdx]),
					(int)cls.state,
					(unsigned int)cls.keyCatchers);
				--s_primaryAnalogEdgeLogBudget;
			}
#endif
			IN_CommonJoyPress(port, analogXlat[buttonIdx], newPressed);
		}
	}

	// Update joysticks
	_padInfo.joyInfo[0].x = _joyAxisConvert(newState.Gamepad.sThumbLX);
	_padInfo.joyInfo[0].y = _joyAxisConvert(newState.Gamepad.sThumbLY);
	_padInfo.joyInfo[1].x = _joyAxisConvert(newState.Gamepad.sThumbRX);
	_padInfo.joyInfo[1].y = _joyAxisConvert(newState.Gamepad.sThumbRY);
	_padInfo.joyInfo[0].valid = _padInfo.joyInfo[1].valid = true;
	_padInfo.padId = port;

	// Copy state back
	oldState = newState;

	// Update game
	IN_CommonUpdate();
}

extern qboolean CurrentStateIsInteractive();
extern int mainControllerDelayedUnplug;

extern void startsetMainController(int controller);
extern int gLaunchController;
/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
//extern int ignoreInputTime;
extern vmCvar_t ControllerOutNum;
void IN_Frame (void)
{
	static qboolean first = qtrue;
	static int callCount = 0;
	const qboolean xboxTraceInput = qfalse;
	if (xboxTraceInput) XBLF("JA: IN_TIGHT #%d enter in_state=%p", callCount, (void*)in_state);
	if (callCount < 2) {
		XBLog_Write(va("JA: IN_Frame #%d entered, in_state=%p", callCount, (void*)in_state));
	}
	callCount++;
	if (in_state)
	{
		// First, check for changes in device status (removed/inserted pads)
		DWORD dwInsert, dwRemove;
		if( XGetDeviceChanges( XDEVICE_TYPE_GAMEPAD, &dwInsert, &dwRemove ) )
		{
			IN_ProcessChanges(dwInsert, dwRemove);
		}

		if ( first )
		{
			// We only force the controller to be locked when we came from MP:
			extern bool Sys_QuickStart( void );
			bool quickStart = Sys_QuickStart();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			extern bool Sys_IsDirectMapBoot(void);
			if ( Sys_IsDirectMapBoot() )
			{
				if ( quickStart )
				{
					Com_Printf("\tController %d initialized\n", gLaunchController);
					startsetMainController(gLaunchController);
				}
				Cvar_SetValue( "inSplashMenu", 0 );
				Cvar_SetValue( "ControllerOutNum", -1 );
				XBLog_Write("STEFX: direct-map input gate cleared splash/controller lock");
			}
			else
#endif
			if( quickStart )
			{
				Com_Printf("\tController %d initialized\n", gLaunchController);
				startsetMainController(gLaunchController);

				// We're bypassing splash menu!
				Cvar_SetValue( "inSplashMenu", 0 );
			}

			// Only do this check once, no matter what:
			first = qfalse;
		}

		if ( mainControllerDelayedUnplug && CurrentStateIsInteractive() && ControllerOutNum.integer < 0)
			IN_ProcessChanges(0, mainControllerDelayedUnplug);

		// Generate callbacks for each controller that's plugged in
		for (int port = 0; port < IN_MAX_CONTROLLERS; ++port)
		{
			if (xboxTraceInput) XBLF("JA: IN_TIGHT #%d port=%d handle=%p", callCount - 1, port, (void*)in_state->controllers[port].handle);
			if (in_state->controllers[port].handle)
				IN_UpdateGamepad(port);
		}

		if (xboxTraceInput) XBLF("JA: IN_TIGHT #%d before IN_UIEmptyQueue", callCount - 1);
		IN_UIEmptyQueue();
		if (xboxTraceInput) XBLF("JA: IN_TIGHT #%d before IN_RumbleFrame", callCount - 1);
		IN_RumbleFrame();
		// Pump even if no scripts remain, the game is paused, or vibration
		// is disabled: the previous nonzero packet may have just completed.
		for (int port = 0; port < IN_MAX_CONTROLLERS; ++port)
			IN_FlushRumble(port);
		if (xboxTraceInput) XBLF("JA: IN_TIGHT #%d exit", callCount - 1);
	}
}
