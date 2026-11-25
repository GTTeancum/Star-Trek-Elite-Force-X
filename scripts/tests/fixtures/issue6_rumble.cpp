// Host components for verify_issue6_rumble.py. The Xbox portion is a deterministic
// async device double, not a claim about how long a particular controller takes.
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
#define IN_MAX_CONTROLLERS 4
#define KEYCATCH_UI 2
#define CVAR_ARCHIVE 1
#define XBLF(...) ((void)0)
#define XBLog_Write(...) ((void)0)
#define XBLog_WriteCritical(...) ((void)0)
#define XBLog_WriteCriticalf(...) ((void)0)
typedef int qboolean;
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef float vec3_t[3];
const qboolean qtrue = 1, qfalse = 0;
#include "code/client/fffx.h"
#include "code/client/keycodes.h"
#include "code/win32/win_input.h"
#include "SP-Mod-Source-Code-master/game/weapons.h"

struct cvar_t { int integer; };
typedef cvar_t vmCvar_t;
static cvar_t enabled = {1}, paused, other;
cvar_t *cl_paused = &paused;
vmCvar_t ControllerOutNum = {-1};
static int clockMs = 1000, mainPad, catcher, splitScreen, slotOn[] = {1,1,1,1};
static bool fullscreen, freezeAreaBits;
static struct { int time; } hostCg, efCg;
cvar_t *Cvar_Get(const char *name, const char *, int) {
	return strcmp(name, "in_useRumble") == 0 ? &enabled : &other;
}
int Cvar_VariableIntegerValue(const char *name) {
	if (!strcmp(name, "stefx_splitScreen")) return splitScreen;
	for (int n = 0; n < 4; ++n) {
		char expected[32];
		sprintf(expected, "joy_vibestate_%d", n);
		if (!strcmp(name, expected)) return slotOn[n];
	}
	assert(false);
	return 0;
}
void Cvar_SetValue(const char *, int) {}
void Com_Printf(const char *, ...) {}
#define Com_sprintf snprintf
int Sys_Milliseconds() { return clockMs; }
int IN_GetMainController() { return mainPad; }
int Key_GetCatcher() { return catcher; }
qboolean _UI_IsFullscreen() { return fullscreen; }
int CL_STEFX_SplitScreen_LocalSlotForPad(int port) { return (port + 2) % 4; }
void CL_STEFX_SplitScreen_RecordPadState(int,qboolean,int,int,void*,int,int,int,int) {}
bool noControllersConnected, wasPlugged[4];
void IN_PadPlugged(int port) { wasPlugged[port] = true; }
void IN_PadUnplugged(int port) { wasPlugged[port] = false; }
void IN_UpdateGamepad(int) {}
void IN_UIEmptyQueue() {}
bool Sys_QuickStart() { return false; }
bool Sys_IsDirectMapBoot() { return false; }
int gLaunchController, mainControllerDelayedUnplug;
qboolean CurrentStateIsInteractive() { return qtrue; }
void startsetMainController(int n) { mainPad = n; }
struct stefxSplitPadState_t { qboolean attackDown; };

static qboolean s_controllerInvertY, s_controllerVibration;
static struct {
	bool invertAim[2]; int rumble[2], saves;
	bool Save() { ++saves; return true; }
} Settings;
static struct {
	float Cvar_VariableValue(const char *name) { return !strcmp(name, "in_useRumble") ? enabled.integer : -0.022f; }
	void Cvar_SetValue(const char *name, float value) { if (!strcmp(name, "in_useRumble")) enabled.integer = (int)value; }
} ui;
static void EFFe_ReturnToConfigure() {}

struct entityState_t { int number, weapon; };
struct centity_t { entityState_t currentState; int muzzleFlashTime; struct { bool lightningFiring; } pe; };
struct weaponInfo_t { int flashSound, altFlashSound; };
static weaponInfo_t cg_weapons[WP_NUM_WEAPONS];
unsigned int g_SPXBCGameWeaponFireStage, g_SPXBCGameWeaponFireEntity, g_SPXBCGameWeaponFireWeaponAlt;
unsigned int g_SPXBCGamePlayerAltFireCompletions, g_SPXBCGamePlayerPrimaryFireCompletions;
enum { CG_FF_STARTFX, CG_FF_ENSUREFX, CHAN_WEAPON,
	STEFX_CG_FF_STARTFX = CG_FF_STARTFX, STEFX_CG_FF_ENSUREFX = CG_FF_ENSUREFX };
void CG_Error(const char *) { assert(false); }
void cgi_S_StartSound(void *, int, int, int) {}

// Old handles deliberately keep ownership of pending feedback after hot-unplug.
enum { ERROR_SUCCESS = 0, ERROR_BUSY = 170, ERROR_IO_PENDING = 997,
	XDEVICE_TYPE_GAMEPAD = 1, XDEVICE_NO_SLOT = 0 };
struct XINPUT_STATE { DWORD dwPacketNumber; };
struct XINPUT_FEEDBACK {
	struct { DWORD dwStatus; } Header;
	struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } Rumble;
};
struct XDEVICE_PREALLOC_TYPE { int type, count; };
struct MockPad {
	int port, attempts, accepted, appliedL, appliedR;
	bool closed, async, reject, preserveSyncHeader;
	XINPUT_FEEDBACK *pending;
	WORD pendingL, pendingR;
};
typedef MockPad *HANDLE;
static MockPad pads[12], *attached[4];
static int opened;
static DWORD mask, inserts, removes;
void XInitDevices(int, XDEVICE_PREALLOC_TYPE *) {}
DWORD XGetDevices(int) { return mask; }
bool XGetDeviceChanges(int, DWORD *added, DWORD *removed) {
	*added = inserts; *removed = removes;
	inserts = removes = 0;
	return (*added || *removed);
}
HANDLE XInputOpen(int, int port, int, void *) {
	assert(opened < 12 && !attached[port]);
	MockPad *pad = &pads[opened++];
	pad->port = port;
	return attached[port] = pad;
}
void XInputClose(HANDLE pad) {
	assert(!pad->closed && attached[pad->port] == pad);
	pad->closed = true;
	attached[pad->port] = NULL;
}
DWORD XInputSetState(HANDLE pad, XINPUT_FEEDBACK *fb) {
	assert(!pad->closed && !pad->pending);
	for (int i = 0; i < opened; ++i) assert(pads[i].pending != fb);
	++pad->attempts;
	if (pad->reject) {
		pad->reject = false;
		return ERROR_BUSY;  // no header update on immediate submission failure
	}
	++pad->accepted;
	pad->pendingL = fb->Rumble.wLeftMotorSpeed;
	pad->pendingR = fb->Rumble.wRightMotorSpeed;
	if (pad->async) {
		pad->pending = fb;
		return fb->Header.dwStatus = ERROR_IO_PENDING;
	}
	pad->appliedL = pad->pendingL;
	pad->appliedR = pad->pendingR;
	if (pad->preserveSyncHeader) return ERROR_SUCCESS;
	return fb->Header.dwStatus = ERROR_SUCCESS;
}
static void complete(MockPad &pad, DWORD result = ERROR_SUCCESS) {
	assert(pad.pending && pad.pending->Header.dwStatus == ERROR_IO_PENDING);
	assert(pad.pending->Rumble.wLeftMotorSpeed == pad.pendingL);
	assert(pad.pending->Rumble.wRightMotorSpeed == pad.pendingR);
	pad.pending->Header.dwStatus = result;
	pad.pending = NULL;
	if (result == ERROR_SUCCESS) {
		pad.appliedL = pad.pendingL; pad.appliedR = pad.pendingR;
	}
}

// GAME_UNDER_TEST

static void tick(int ms = 1) {
	clockMs += ms;
	efCg.time = clockMs;
	hostCg.time = freezeAreaBits ? 0 : clockMs;
	IN_Frame();
}
static void fire(int weapon = WP_PHASER) {
	centity_t local = {};
	local.currentState.weapon = weapon;
	CG_FireWeapon(&local, qfalse);
}
static void check(bool pass, const char *why) {
	if (!pass) { fprintf(stderr,"FAIL: %s\n", why); exit(1); }
}
static bool isOn(const MockPad &p) { return p.appliedL || p.appliedR; }
static void pending(const MockPad &p, int l, int r, const char *why) {
	check(p.pending && p.pendingL == l && p.pendingR == r, why);
}
static void checkQuiet(MockPad &p) {
	check(!p.pending && !isOn(p), "both motors must be off");
	int count = p.attempts;
	for (int i = 0; i < 12; ++i) tick();
	check(p.attempts == count, "idle frames must not resubmit accepted stops");
}

int main(int argc, char **argv) {
	assert(argc == 2);
	const char *test = argv[1];
	mask = !strcmp(test, "split") ? (1u << 2) | (1u << 3) : 1;
	freezeAreaBits = !strcmp(test, "clock");
	IN_Init();
	tick();
	MockPad &p = *attached[mask == 1 ? 0 : 2];
	if (!strcmp(test, "clock")) {
		fire(); tick();
		check(p.appliedL == 10000 && p.appliedR == 30000, "campaign phaser pulse at original strengths");
		tick(119); check(isOn(p), "pulse must last its original duration");
		tick(2); check(!isOn(p), "120-ms phaser must stop on real time even with frozen snapshot area bits");
		checkQuiet(p);
	} else if (!strcmp(test, "split")) {
		MockPad &p2 = *attached[3];
		splitScreen = 1;
		stefxSplitPadState_t a = {}, b = {};
		p.async = true;
		STEFX_SplitHolomatchAttackRumble(0, 2, &a, qtrue);
		STEFX_SplitHolomatchAttackRumble(1, 3, &b, qtrue);
		tick();
		pending(p, 22000, 12000, "split P1 pulse pending on actual port 2");
		check(p2.appliedL == 22000 && p2.appliedR == 12000, "split P2 pulse on actual port 3");
		slotOn[1] = 0; tick(1);
		check(!isOn(p2), "only disabled local slot must stop");
		check(p.pendingL == 22000, "another player's pending command must not change");
		tick(71); complete(p); tick();
		pending(p, 0, 0, "P1 stops on expiry, despite busy hardware");
		complete(p); checkQuiet(p); checkQuiet(p2);
		mainPad = 2; paused.integer = 1; tick();
		mainPad = 3; paused.integer = 0; tick();
		a.attackDown = qfalse;
		STEFX_SplitHolomatchAttackRumble(0, 2, &a, qtrue); tick();
		pending(p, 22000, 12000, "pausing must not strand a formerly primary port");
		complete(p); tick(71); complete(p); checkQuiet(p);
	} else {
		p.async = true;
		fire(); tick();
		pending(p, 10000, 30000, "campaign primary phaser must submit original two motor strengths");
		if (!strcmp(test, "options")) {
			check(IN_STEFX_IsFrontendButton(A_JOY16) && IN_STEFX_IsFrontendButton(A_JOY15),
				"physical X and A accepted by the game's frontend key filter");
			catcher = KEYCATCH_UI; tick();
			EFFe_LoadControllerState(); EFFe_HandleControllerKey(A_JOY16);
			check(enabled.integer == 1, "X alone must not commit");
			EFFe_HandleControllerKey(A_JOY15);
			check(!enabled.integer && !Settings.rumble[0] && Settings.saves == 1, "A commits off");
			catcher = 0; fire(); tick(); complete(p); tick();
			pending(p, 0, 0, "physical stop must survive leaving Options with vibration off");
			complete(p); checkQuiet(p); fire(); tick(); checkQuiet(p);
			catcher = KEYCATCH_UI;
			EFFe_LoadControllerState(); EFFe_HandleControllerKey(A_JOY16); EFFe_HandleControllerKey(A_JOY13);
			check(!enabled.integer && Settings.saves == 1, "Y discards staged change");
			EFFe_LoadControllerState(); EFFe_HandleControllerKey(A_JOY16); EFFe_HandleControllerKey(A_JOY15);
			check(enabled.integer == 1 && Settings.saves == 2, "X then A re-enables");
			catcher = 0; fire(); tick();
			pending(p, 10000, 30000, "next shot after Options re-enable works");
			complete(p); tick(121); complete(p); checkQuiet(p);
		} else if (!strcmp(test, "hotplug")) {
			removes = inserts = 1; tick();
			MockPad &fresh = *attached[0];
			check(p.closed && &p != &fresh && !isOn(fresh), "replacement pad starts neutral");
			fire(); tick();
			check(fresh.appliedL == 10000 && fresh.appliedR == 30000, "new pad works before old pending retires");
			complete(p); tick(121);
			checkQuiet(fresh);
		} else if (!strcmp(test, "coalesce")) {
			tick(121);                              // old target now zero
			fire(WP_COMPRESSION_RIFLE); tick();     // newest target replaces zero
			complete(p); tick();
			pending(p, 10000, 50000, "send only the newest target, not a stale stop");
			complete(p); tick(201);
			pending(p, 0, 0, "two-state compression rifle stop");
			complete(p); checkQuiet(p);
		} else {
			bool off = !strcmp(test, "off"), menu = !strcmp(test, "menu"), pause = !strcmp(test, "pause");
			bool retry = !strcmp(test, "retry");
			check(off || menu || pause || retry || !strcmp(test, "expiry"), "unknown scenario");
			if (off) enabled.integer = 0;
			if (menu) catcher = KEYCATCH_UI;        // native frontend; legacy fullscreen is false
			if (pause) paused.integer = 1;
			tick(off || menu || pause ? 1 : 121);
			if (off) { fire(); tick(); }           // attempts at new feedback must be ignored
			pending(p, 10000, 30000, "in-flight request must remain untouched");
			complete(p);
			if (retry) {
				p.reject = true; tick();
				check(!p.pending && isOn(p), "injected immediate failure should not turn motors off");
			}
			tick();
			pending(p, 0, 0, "a busy motor-off request must be retried with no new effect");
			if (retry) {
				complete(p, ERROR_BUSY); tick();
				pending(p, 0, 0, "retry async failure, too");
				complete(p, ERROR_BUSY);
				p.async = false; p.preserveSyncHeader = true; tick();
				checkQuiet(p);  // success is final even with a stale async status
			}
			else complete(p);
			enabled.integer = 1; catcher = 0; paused.integer = 0;
			checkQuiet(p);                         // no old script replay after re-enable/resume
			if (off || menu || pause) {
				fire(); tick();
				pending(p, 10000, 30000, "a new shot after re-enable/unpause still works");
				complete(p); tick(121); complete(p); checkQuiet(p);
			}
		}
	}
	IN_Shutdown();
	printf("PASS %s\n", test);
	return 0;
}
