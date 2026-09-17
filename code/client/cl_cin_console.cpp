
/*****************************************************************************
 * name:		cl_cin_console.cpp
 *
 * desc:		video and cinematic playback interface for Xbox (using Bink)
 *
 *****************************************************************************/

#include "client.h"
#include "../win32/win_local.h"
#include "../win32/win_input.h"
#include "../win32/glw_win_dx8.h"
#include "../win32/xb_log.h"
#include "BinkVideo.h"

char XBOX_VIDEO_PATH[64] = "d:\\BaseEF\\video\\";
#define SHADER_VIDEO_PATH "z:\\"

BinkVideo bVideo;	// bink video object
connstate_t	previousState = CA_UNINITIALIZED;	// previous cinematic state

#ifdef _XBOX
extern "C" volatile unsigned int g_SPXBCinPhase;
extern "C" volatile unsigned int g_SPXBCinHandle;
extern "C" volatile unsigned int g_SPXBCinStatus;
extern "C" volatile unsigned int g_SPXBCinLoopCount;
extern "C" volatile char g_SPXBCinArgLast[64];

static qboolean s_xboxBlockingMoviePumpedFrame = qfalse;

static void CIN_XboxCopyArgLast(const char *arg)
{
	int i = 0;
	if (arg)
	{
		while (arg[i] && i < (int)sizeof(g_SPXBCinArgLast) - 1)
		{
			g_SPXBCinArgLast[i] = arg[i];
			++i;
		}
	}
	g_SPXBCinArgLast[i] = 0;
}
#endif

struct CinematicData
{
	char	filename[MAX_OSPATH];	// No path, no extension
	int 	x, y, w, h;				// Dimensions
	int		bits;					// Flags (loop, silent, shader)
};

// We have a fixed lookup table of all cinematics that can be played
// Video handles are just indices into the array. An entry is not
// considered initialized until its width is nonzero
CinematicData cinFiles[] = {
	// Elite Force frontend / story movies.
	{ "eflogo", 0, 0, 0, 0, 0 },
	{ "eflogo_lo", 0, 0, 0, 0, 0 },
	{ "intro", 0, 0, 0, 0, 0 },
	{ "intro_lo", 0, 0, 0, 0, 0 },
	{ "st_01", 0, 0, 0, 0, 0 },
	{ "st_01_lo", 0, 0, 0, 0, 0 },
	{ "st_02", 0, 0, 0, 0, 0 },
	{ "st_02_lo", 0, 0, 0, 0, 0 },
	{ "st_04", 0, 0, 0, 0, 0 },
	{ "st_04_lo", 0, 0, 0, 0, 0 },
	{ "st_05", 0, 0, 0, 0, 0 },
	{ "st_05_lo", 0, 0, 0, 0, 0 },
	{ "st_06", 0, 0, 0, 0, 0 },
	{ "st_06_lo", 0, 0, 0, 0, 0 },
	{ "st_07", 0, 0, 0, 0, 0 },
	{ "st_07_lo", 0, 0, 0, 0, 0 },
	{ "st_08", 0, 0, 0, 0, 0 },
	{ "st_08_lo", 0, 0, 0, 0, 0 },
	{ "st_09", 0, 0, 0, 0, 0 },
	{ "st_09_lo", 0, 0, 0, 0, 0 },
	{ "st_10", 0, 0, 0, 0, 0 },
	{ "st_10_lo", 0, 0, 0, 0, 0 },
	{ "st_11", 0, 0, 0, 0, 0 },
	{ "st_11_lo", 0, 0, 0, 0, 0 },
	{ "st_12", 0, 0, 0, 0, 0 },
	{ "st_12_lo", 0, 0, 0, 0, 0 },
	{ "st_13", 0, 0, 0, 0, 0 },
	{ "st_13_lo", 0, 0, 0, 0, 0 },
	{ "st_14", 0, 0, 0, 0, 0 },
	{ "st_14_lo", 0, 0, 0, 0, 0 },
	{ "st_15", 0, 0, 0, 0, 0 },
	{ "st_15_lo", 0, 0, 0, 0, 0 },
	{ "st_16a", 0, 0, 0, 0, 0 },
	{ "st_16a_lo", 0, 0, 0, 0, 0 },
	{ "st_1718", 0, 0, 0, 0, 0 },
	{ "st_1718_lo", 0, 0, 0, 0, 0 },
	{ "st_19", 0, 0, 0, 0, 0 },
	{ "st_19_lo", 0, 0, 0, 0, 0 },
	{ "st_20", 0, 0, 0, 0, 0 },
	{ "st_20_lo", 0, 0, 0, 0, 0 },
	{ "st_21", 0, 0, 0, 0, 0 },
	{ "st_21_lo", 0, 0, 0, 0, 0 },
	// Attract sequence
	{ "attract", 0, 0, 0, 0, 0 },

	// Planet shaders
	{ "cos", 0, 0, 0, 0, 0 },
	{ "bakura", 0, 0, 0, 0, 0 },
	{ "blenjeel", 0, 0, 0, 0, 0 },
	{ "chandrila", 0, 0, 0, 0, 0 },
	{ "core", 0, 0, 0, 0, 0 },
	{ "ast", 0, 0, 0, 0, 0 },
	{ "dosunn", 0, 0, 0, 0, 0 },
	{ "krildor", 0, 0, 0, 0, 0 },
	{ "narkreeta", 0, 0, 0, 0, 0 },
	{ "ordman", 0, 0, 0, 0, 0 },
	{ "tanaab", 0, 0, 0, 0, 0 },
	{ "tatooine", 0, 0, 0, 0, 0 },
	{ "yalara", 0, 0, 0, 0, 0 },
	{ "zonju", 0, 0, 0, 0, 0 },

	// Others
//	{ "jk0101_sw", 0, 0, 0, 0, 0 },	// Folded into ja01!
//	{ "ja01", 0, 0, 0, 0, 0 },		// Contains the text crawl, so must be localized:
	{ "ja01_e", 0, 0, 0, 0, 0 },
	{ "ja01_f", 0, 0, 0, 0, 0 },
	{ "ja01_d", 0, 0, 0, 0, 0 },
	{ "ja02", 0, 0, 0, 0, 0 },
	{ "ja03", 0, 0, 0, 0, 0 },
	{ "ja04", 0, 0, 0, 0, 0 },
	{ "ja05", 0, 0, 0, 0, 0 },
	{ "ja06", 0, 0, 0, 0, 0 },
	{ "ja07", 0, 0, 0, 0, 0 },
	{ "ja08", 0, 0, 0, 0, 0 },
	{ "ja09", 0, 0, 0, 0, 0 },
	{ "ja10", 0, 0, 0, 0, 0 },
	{ "ja11", 0, 0, 0, 0, 0 },
	{ "ja12", 0, 0, 0, 0, 0 },
};

const int cinNumFiles = sizeof(cinFiles) / sizeof(cinFiles[0]);
static int currentHandle = -1;

// Stupid PC filth
static qboolean qbInGameCinematicOnStandBy = qfalse;
static char	 sInGameCinematicStandingBy[MAX_QPATH];

bool CIN_PlayAllFrames( const char *arg, int x, int y, int w, int h, int systemBits, bool keyBreakAllowed );

/********
CIN_CloseAllVideos
Stops all currently running videos
*********/
void CIN_CloseAllVideos(void)
{
	XBLog_Write("JA: CIN_CloseAllVideos");
	// Stop the current bink video
	bVideo.Stop();
	currentHandle = -1;
}

/********
CIN_StopCinematic

handle	- Not used
return	- FMV status

Stops the current cinematic
*********/
e_status CIN_StopCinematic(int handle)
{
	char cinLog[160];
	_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_StopCinematic handle=%d current=%d prev=%d status=%d", handle, currentHandle, previousState, bVideo.GetStatus());
	cinLog[sizeof(cinLog) - 1] = '\0';
	XBLog_Write(cinLog);
	assert( handle == currentHandle );
	currentHandle = -1;

	if(previousState != CA_UNINITIALIZED)
	{
		cls.state = previousState;
		previousState = CA_UNINITIALIZED;
	}
	if(bVideo.GetStatus() != NS_BV_STOPPED)
	{
		bVideo.Stop();
	}
	return FMV_EOF;
}

/********
CIN_RunCinematic

handle	- Ensure that the supplied cinematic is the one running
return	- FMV status

Fetch and decompress the pending frame
*********/
e_status CIN_RunCinematic (int handle)
{
	char cinLog[256];
	static int runLogBudget = 16;
#ifdef _XBOX
	g_SPXBCinPhase = 300;
	g_SPXBCinHandle = (unsigned int)handle;
	g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
#endif
	if (runLogBudget > 0)
	{
		_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_RunCinematic enter handle=%d current=%d w=%d h=%d status=%d state=%d", handle, currentHandle, (handle >= 0 && handle < cinNumFiles) ? cinFiles[handle].w : -1, (handle >= 0 && handle < cinNumFiles) ? cinFiles[handle].h : -1, bVideo.GetStatus(), cls.state);
		cinLog[sizeof(cinLog) - 1] = '\0';
		XBLog_Write(cinLog);
		runLogBudget--;
	}
	if (handle < 0 || handle >= cinNumFiles || !cinFiles[handle].w)
	{
#ifdef _XBOX
		g_SPXBCinPhase = 301;
#endif
		assert( 0 );
		return FMV_EOF;
	}
#ifdef _XBOX
	g_SPXBCinPhase = 302;
	CIN_XboxCopyArgLast(cinFiles[handle].filename);
#endif

	// If we weren't playing a movie, or playing the wrong one - start up
	if (handle != currentHandle)
	{
		bool shader = cinFiles[handle].bits & CIN_shader;
#ifdef _XBOX
		g_SPXBCinPhase = 310;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
		XBLF("JA: CIN_PHASE RunCinematic start handle=%d current=%d file='%s' shader=%d status=%d",
			handle, currentHandle, cinFiles[handle].filename, shader ? 1 : 0, (int)bVideo.GetStatus());
#endif
		_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_RunCinematic starting file='%s' shader=%d bits=0x%x", cinFiles[handle].filename, shader ? 1 : 0, cinFiles[handle].bits);
		cinLog[sizeof(cinLog) - 1] = '\0';
		XBLog_Write(cinLog);

#ifdef _XBOX
		g_SPXBCinPhase = 311;
#endif
		CIN_StopCinematic(currentHandle);
#ifdef _XBOX
		g_SPXBCinPhase = 312;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
#endif
		if (!bVideo.Start(
				va("%s%s.bik",
					shader ? SHADER_VIDEO_PATH : XBOX_VIDEO_PATH,
					cinFiles[handle].filename),
				cinFiles[handle].x, cinFiles[handle].y,
				cinFiles[handle].w, cinFiles[handle].h))
		{
#ifdef _XBOX
			g_SPXBCinPhase = 313;
			g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
#endif
			XBLog_Write("JA: CIN_RunCinematic BinkVideo::Start failed");
			return FMV_EOF;
		}
#ifdef _XBOX
		g_SPXBCinPhase = 314;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
#endif
		XBLog_Write("JA: CIN_RunCinematic BinkVideo::Start succeeded");
#ifdef _XBOX
		if (!shader)
		{
			extern void CL_STEFX_ExcludeBlockingMovieFromFps(void);
			CL_STEFX_ExcludeBlockingMovieFromFps();
			XBLF("STEFX_FPS_CONTEXT: exclude blocking movie '%s'", cinFiles[handle].filename);
		}
#endif

#ifdef _XBOX
		// Keep these phase writes independent of formatted logging.  If movie
		// startup leaves the CPU in a bad state, the XEMU monitor can still tell
		// us exactly which ordinary setup statement completed.
		g_SPXBCinPhase = 3141;
#endif

		if (cinFiles[handle].bits & CIN_loop)
		{
#ifdef _XBOX
			g_SPXBCinPhase = 3142;
			XBLF("JA: CIN_PHASE RunCinematic before SetLooping true handle=%d status=%d",
				handle, (int)bVideo.GetStatus());
#endif
			bVideo.SetLooping(true);
		}
		else
		{
#ifdef _XBOX
			g_SPXBCinPhase = 3142;
			XBLF("JA: CIN_PHASE RunCinematic before SetLooping false handle=%d status=%d",
				handle, (int)bVideo.GetStatus());
#endif
			bVideo.SetLooping(false);
		}
#ifdef _XBOX
		g_SPXBCinPhase = 3143;
		XBLF("JA: CIN_PHASE RunCinematic after SetLooping handle=%d status=%d",
			handle, (int)bVideo.GetStatus());
#endif

		if (cinFiles[handle].bits & CIN_silent)
		{
#ifdef _XBOX
			g_SPXBCinPhase = 3144;
			XBLF("JA: CIN_PHASE RunCinematic before SetMasterVolume silent handle=%d status=%d",
				handle, (int)bVideo.GetStatus());
#endif
			bVideo.SetMasterVolume(0);
		}
		else
		{
#ifdef _XBOX
			g_SPXBCinPhase = 3144;
			XBLF("JA: CIN_PHASE RunCinematic before SetMasterVolume normal handle=%d status=%d",
				handle, (int)bVideo.GetStatus());
#endif
			bVideo.SetMasterVolume(16384);	//32768);	// Default Bink volume
		}
#ifdef _XBOX
		g_SPXBCinPhase = 3145;
		XBLF("JA: CIN_PHASE RunCinematic after SetMasterVolume handle=%d status=%d",
			handle, (int)bVideo.GetStatus());
#endif

		if (!shader)
		{
#ifdef _XBOX
			g_SPXBCinPhase = 3146;
			XBLF("JA: CIN_PHASE RunCinematic before state switch handle=%d prev=%d status=%d",
				handle, (int)cls.state, (int)bVideo.GetStatus());
#endif
			previousState = cls.state;
			cls.state = CA_CINEMATIC;
#ifdef _XBOX
			g_SPXBCinPhase = 3147;
			XBLF("JA: CIN_PHASE RunCinematic after state switch handle=%d prev=%d state=%d status=%d",
				handle, (int)previousState, (int)cls.state, (int)bVideo.GetStatus());
#endif
		}

#ifdef _XBOX
		g_SPXBCinPhase = 3148;
#endif
		currentHandle = handle;
#ifdef _XBOX
		g_SPXBCinPhase = 315;
		XBLF("JA: CIN_PHASE RunCinematic currentHandle set handle=%d status=%d state=%d",
			handle, (int)bVideo.GetStatus(), (int)cls.state);
#endif
	}

	// Normal case does nothing here
#ifdef _XBOX
	if (runLogBudget > 0)
	{
		XBLF("JA: CIN_PHASE RunCinematic normal path handle=%d current=%d before status status=%d state=%d",
			handle, currentHandle, (int)bVideo.GetStatus(), (int)cls.state);
	}
#endif
	if(bVideo.GetStatus() == NS_BV_STOPPED)
	{
#ifdef _XBOX
		g_SPXBCinPhase = 320;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
#endif
		if (runLogBudget > 0)
		{
			XBLog_Write("JA: CIN_RunCinematic returning EOF");
			runLogBudget--;
		}
		return FMV_EOF;
	}
	else
	{
#ifdef _XBOX
		g_SPXBCinPhase = 321;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
		if (runLogBudget > 0)
		{
			XBLF("JA: CIN_PHASE RunCinematic returning PLAY handle=%d current=%d status=%d state=%d",
				handle, currentHandle, (int)bVideo.GetStatus(), (int)cls.state);
			runLogBudget--;
		}
#endif
		return FMV_PLAY;
	}
}

/********
CIN_PlayCinematic

arg0	- filename of bink video
xpos	- x origin
ypos	- y origin
width	- width of the movie window
height	- height of the movie window
bits	- CIN flags
psAudioFile	- audio file for movie (not used)

Starts playing the given bink video file
*********/
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits, const char *psAudioFile /* = NULL */)
{
	char	arg[MAX_OSPATH];
	char*	nameonly;
	int		handle;

	// get a local copy of the name
	strcpy(arg,arg0);
	char cinLog[256];
	_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_PlayCinematic arg='%s' rect=%d,%d,%d,%d bits=0x%x", arg0 ? arg0 : "<null>", xpos, ypos, width, height, bits);
	cinLog[sizeof(cinLog) - 1] = '\0';
	XBLog_Write(cinLog);

	// remove path, find in list
	nameonly = COM_SkipPath(arg);

	// ja01 contains the text crawl, so we need to add on the right language suffix
	extern DWORD g_dwLanguage;
	if( Q_stricmp(nameonly, "ja01") == 0)
	{
		switch( g_dwLanguage )
		{
			case XC_LANGUAGE_FRENCH:
				strcat(nameonly, "_f");
				break;
			case XC_LANGUAGE_GERMAN:
				strcat(nameonly, "_d");
				break;
			case XC_LANGUAGE_ENGLISH:
			default:
				strcat(nameonly, "_e");
				break;
		}
	}

	for (handle = 0; handle < cinNumFiles; ++handle)
	{
		if (!Q_stricmp(cinFiles[handle].filename, nameonly))
			break;
	}

	// Don't have the requested movie in our table?
	if (handle == cinNumFiles)
	{
		Com_Printf( "ERROR: Movie file %s not found!\n", nameonly );
		_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_PlayCinematic not found name='%s'", nameonly);
		cinLog[sizeof(cinLog) - 1] = '\0';
		XBLog_Write(cinLog);
		return -1;
	}

	// Store off information about the movie in the right place. Don't
	// actually play them movie, CIN_RunCinematic takes care of that.
	cinFiles[handle].x = xpos;
	cinFiles[handle].y = ypos;
	cinFiles[handle].w = width;
	cinFiles[handle].h = height;
	cinFiles[handle].bits = bits;
	currentHandle = -1;
	_snprintf(cinLog, sizeof(cinLog) - 1, "JA: CIN_PlayCinematic handle=%d name='%s'", handle, cinFiles[handle].filename);
	cinLog[sizeof(cinLog) - 1] = '\0';
	XBLog_Write(cinLog);
	return handle;
}

/*********
CIN_SetExtents

handle	- handle to a video
x		- x origin for window
y		- y origin for window
w		- width for window
h		- height for window
*********/
void CIN_SetExtents (int handle, int x, int y, int w, int h)
{
	if (handle < 0 || handle >= cinNumFiles)
		return;

	cinFiles[handle].x = x;
	cinFiles[handle].y = y;
	cinFiles[handle].w = w;
	cinFiles[handle].h = h;

	if (handle == currentHandle)
		bVideo.SetExtents(x,y,w,h);
}


/*********
SCR_DrawCinematic

Externally-called only, and only if cls.state == CA_CINEMATIC (or CL_IsRunningInGameCinematic() == true now)
*********/
void SCR_DrawCinematic (void)
{
#ifdef _XBOX
	static unsigned int s_drawCinematicCalls = 0;
	++s_drawCinematicCalls;
	if (s_drawCinematicCalls <= 16 || (s_drawCinematicCalls % 120) == 0)
	{
		XBLF("STEFX_CIN_DRAW: call=%u standby=%d current=%d status=%d state=%d",
			s_drawCinematicCalls,
			CL_InGameCinematicOnStandBy() ? 1 : 0,
			currentHandle,
			(int)bVideo.GetStatus(),
			(int)cls.state);
	}
#endif
	if (CL_InGameCinematicOnStandBy())
	{
		CIN_PlayAllFrames( sInGameCinematicStandingBy, 0, 0, 640, 480, 0, true );
	}
	else
	{
#ifdef _XBOX
		if (s_xboxBlockingMoviePumpedFrame)
		{
			if (s_drawCinematicCalls <= 16 || (s_drawCinematicCalls % 120) == 0)
			{
				XBLF("STEFX_CIN_DRAW: skip duplicate frame pump current=%d status=%d state=%d",
					currentHandle,
					(int)bVideo.GetStatus(),
					(int)cls.state);
			}
			s_xboxBlockingMoviePumpedFrame = qfalse;
			return;
		}
#endif
		// Run and draw a frame:
		const bool ran = bVideo.Run();
#ifdef _XBOX
		if (s_drawCinematicCalls <= 16 || (s_drawCinematicCalls % 120) == 0 || !ran)
		{
			XBLF("STEFX_CIN_DRAW: runResult=%d current=%d status=%d state=%d",
				ran ? 1 : 0,
				currentHandle,
				(int)bVideo.GetStatus(),
				(int)cls.state);
		}
#endif
	}
}

/*********
SCR_RunCinematic
*********/
void SCR_RunCinematic (void)
{
	// This is called every frame, even when we're not playing a movie
	if (currentHandle >= 0 && currentHandle < cinNumFiles)
	{
		if (CIN_RunCinematic(currentHandle) == FMV_EOF)
		{
#ifdef _XBOX
			XBLF("JA: CIN_PHASE SCR_RunCinematic EOF handle=%d status=%d state=%d",
				currentHandle, (int)bVideo.GetStatus(), (int)cls.state);
#endif
			CIN_StopCinematic(currentHandle);
		}
	}
}

/*********
SCR_StopCinematic
*********/
void SCR_StopCinematic(qboolean bAllowRefusal /* = qfalse */)
{
	CIN_StopCinematic(currentHandle);
}

/*********
CIN_UploadCinematic

handle		- (not used)

This function can be used to render a frame of a movie, if
it needs to be done outside of CA_CINEMATIC. For example,
a menu background or wall texture.
*********/
void CIN_UploadCinematic(int handle)
{
	int w, h;
	byte* data;

	assert( handle == currentHandle );

	if(!bVideo.Ready()) {
		return;
	}
	
	w		= bVideo.GetBinkWidth();
	h		= bVideo.GetBinkHeight();
	data	= (byte*)bVideo.GetBinkData();

	// handle is actually being used to pick from scratchImages in
	// this function - we only have two on Xbox, let's just use one.
	//re.UploadCinematic( w, h, data, handle, 1);
	re.UploadCinematic( w, h, data, 0, 1);
}

/*********
CIN_PlayAllFrames

arg				- bink video filename
x				- x origin for movie
y				- y origin for movie
w				- width of the movie
h				- height of the movie
systemBits		- bit rate for movie
keyBreakAllowed	- if true, button press will end playback

Plays the target movie in full
*********/
bool CIN_PlayAllFrames( const char *arg, int x, int y, int w, int h, int systemBits, bool keyBreakAllowed )
{
	bool retval;
#ifdef _XBOX
	g_SPXBCinPhase = 100;
	g_SPXBCinHandle = (unsigned int)-1;
	g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
	g_SPXBCinLoopCount = 0;
	CIN_XboxCopyArgLast(arg);
	XBLF("JA: CIN_PHASE PlayAllFrames enter arg='%s' rect=%d,%d,%d,%d state=%d keyBreak=%d",
		arg ? arg : "<null>", x, y, w, h, (int)cls.state, keyBreakAllowed ? 1 : 0);
#endif
	Key_ClearStates();
#ifdef _XBOX
	g_SPXBCinPhase = 101;
	XBLF("JA: CIN_PHASE PlayAllFrames after Key_ClearStates arg='%s'", arg ? arg : "<null>");
#endif

	// PC hack
	qbInGameCinematicOnStandBy = qfalse;

#ifdef XBOX_DEMO
	// When run from CDX, we can pause the timer during cutscenes:
	extern void Demo_TimerPause( bool bPaused );
	Demo_TimerPause( true );
#endif

#ifdef _XBOX
	g_SPXBCinPhase = 102;
	XBLF("JA: CIN_PHASE PlayAllFrames before CIN_PlayCinematic arg='%s'", arg ? arg : "<null>");
#endif
	int Handle = CIN_PlayCinematic(arg, x, y, w, h, systemBits, NULL);
#ifdef _XBOX
	g_SPXBCinPhase = 103;
	g_SPXBCinHandle = (unsigned int)Handle;
	g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
	XBLF("JA: CIN_PHASE PlayAllFrames after CIN_PlayCinematic handle=%d status=%d",
		Handle, (int)bVideo.GetStatus());
#endif
	if (Handle != -1)
	{
		unsigned int localLoopCount = 0;
		for (;;)
		{
			int cinematicStatus = CIN_RunCinematic(Handle);
#ifdef _XBOX
			if (localLoopCount < 4 || ((localLoopCount + 1) % 120) == 0)
			{
				XBLF("JA: CIN_PHASE PlayAllFrames loop condition arg='%s' loop=%u handle=%d statusRet=%d videoStatus=%d anykey=%d state=%d",
					arg ? arg : "<null>", localLoopCount + 1, Handle, cinematicStatus,
					(int)bVideo.GetStatus(), kg.anykeydown, (int)cls.state);
			}
#endif
			if (cinematicStatus != FMV_PLAY || (keyBreakAllowed && kg.anykeydown))
			{
				break;
			}
#ifdef _XBOX
			g_SPXBCinPhase = 110;
			++g_SPXBCinLoopCount;
			++localLoopCount;
			g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
			if (g_SPXBCinLoopCount <= 8)
			{
				XBLF("JA: CIN_PHASE loop %u before SCR_UpdateScreen handle=%d status=%d anykey=%d state=%d",
					(unsigned int)g_SPXBCinLoopCount, Handle, (int)bVideo.GetStatus(), kg.anykeydown, (int)cls.state);
			}
			if (localLoopCount == 1 || (localLoopCount % 120) == 0)
			{
				XBLF("STEFX: CIN_PlayAllFrames progress arg='%s' loops=%u handle=%d status=%d anykey=%d state=%d",
					arg ? arg : "<null>", localLoopCount, Handle, (int)bVideo.GetStatus(), kg.anykeydown, (int)cls.state);
			}
			if (cls.state == CA_CINEMATIC && bVideo.GetStatus() != NS_BV_STOPPED)
			{
				const bool pumped = bVideo.Run();
				s_xboxBlockingMoviePumpedFrame = pumped ? qtrue : qfalse;
				if (localLoopCount <= 16 || (localLoopCount % 120) == 0 || !pumped)
				{
					XBLF("STEFX_CIN_DRAW: blocking pump arg='%s' loop=%u pumped=%d status=%d state=%d",
						arg ? arg : "<null>",
						localLoopCount,
						pumped ? 1 : 0,
						(int)bVideo.GetStatus(),
						(int)cls.state);
				}
			}
#endif
			SCR_UpdateScreen	();
#ifdef _XBOX
			g_SPXBCinPhase = 111;
#endif
			IN_Frame			();
#ifdef _XBOX
			g_SPXBCinPhase = 112;
#endif
			Com_EventLoop		();
#ifdef _XBOX
			g_SPXBCinPhase = 113;
#endif
		}
#ifdef _XBOX
//		while (CIN_RunCinematic(Handle) == FMV_PLAY && !(keyBreakAllowed && !kg.anykeydown))
//		{
//			SCR_UpdateScreen	();
//			IN_Frame			();
//			Com_EventLoop		();
//		}
#endif
#ifdef _XBOX
		g_SPXBCinPhase = 120;
		g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
		XBLF("JA: CIN_PHASE PlayAllFrames before stop handle=%d loops=%u localLoops=%u status=%d anykey=%d",
			Handle, (unsigned int)g_SPXBCinLoopCount, localLoopCount, (int)bVideo.GetStatus(), kg.anykeydown);
#endif
		CIN_StopCinematic(Handle);
	}

#ifdef XBOX_DEMO
	Demo_TimerPause( false );
#endif

	retval =(keyBreakAllowed && kg.anykeydown);
#ifdef _XBOX
	g_SPXBCinPhase = 130;
	g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
	XBLF("JA: CIN_PHASE PlayAllFrames before final Key_ClearStates retval=%d status=%d",
		retval ? 1 : 0, (int)bVideo.GetStatus());
#endif
	Key_ClearStates();
#ifdef _XBOX
	g_SPXBCinPhase = retval ? 132 : 131;
	XBLF("JA: CIN_PHASE PlayAllFrames exit retval=%d arg='%s'", retval ? 1 : 0, arg ? arg : "<null>");
#endif

	// Soooper hack! Game ends up running for a couple frames after this cutscene. We don't want it to!
	if( Q_stricmp(arg, "ja08") == 0 )
	{
		// Filth. Don't call Present until this gets cleared.
		extern bool connectSwapOverride;
		connectSwapOverride = true;
	}

	return retval;
}

/*********
CIN_Init
Initializes cinematic system
*********/
void CIN_Init(void)
{
	XBLog_Write("JA: CIN_Init enter");
	// Allocate Memory for Bink System
	bVideo.AllocateXboxMem();
	XBLog_Write("JA: CIN_Init exit");
}

/********
CIN_Shutdown
Shutdown the cinematic system
********/
void CIN_Shutdown(void)
{
	XBLog_Write("JA: CIN_Shutdown enter");
	// Free Memory for the Bink System
	bVideo.FreeXboxMem();
	XBLog_Write("JA: CIN_Shutdown exit");
}


/***** Possible FIXME *****/
/***** The following function may need to be implemented *****/
/***** BEGIN *****/
void CL_PlayCinematic_f(void)
{
	char	*arg;

	arg = Cmd_Argv(1);
#ifdef _XBOX
	XBLF("STEFX: CL_PlayCinematic_f fullscreen EF movie arg='%s'", arg ? arg : "<null>");
#endif
	CIN_PlayAllFrames(arg, 0, 0, 640, 480, 0, true);
}

qboolean CL_CheckPendingCinematic(void)
{
#ifdef _XBOX
	static qboolean s_loggedNoPendingCinematic = qfalse;
	if (!s_loggedNoPendingCinematic)
	{
		XBLog_Write("STEFX: CL_CheckPendingCinematic console path no pending cinematic");
		s_loggedNoPendingCinematic = qtrue;
	}
#endif
	return qfalse;
}

qboolean CL_IsRunningInGameCinematic(void)
{
	return qfalse; //qbPlayingInGameCinematic;
}

void CL_PlayInGameCinematic_f(void)
{
#ifdef _XBOX
	g_SPXBCinPhase = 200;
	g_SPXBCinStatus = (unsigned int)bVideo.GetStatus();
	CIN_XboxCopyArgLast(Cmd_Argv( 1 ));
	XBLF("JA: CIN_PHASE InGameCinematic enter state=%d arg='%s' status=%d",
		(int)cls.state, Cmd_Argv( 1 ), (int)bVideo.GetStatus());
#endif
	if (cls.state == CA_ACTIVE)
	{
		// In some situations (during yavin1 intro) we move to a cutscene directly from
		// a shaking camera - so rumble never gets killed.
#ifdef _XBOX
		g_SPXBCinPhase = 201;
		XBLF("JA: CIN_PHASE InGameCinematic before IN_KillRumbleScripts arg='%s'", Cmd_Argv( 1 ));
#endif
		IN_KillRumbleScripts();
#ifdef _XBOX
		g_SPXBCinPhase = 202;
		XBLF("JA: CIN_PHASE InGameCinematic after IN_KillRumbleScripts");
#endif

		char *arg = Cmd_Argv( 1 );
#ifdef _XBOX
		g_SPXBCinPhase = 203;
		CIN_XboxCopyArgLast(arg);
		XBLF("JA: CIN_PHASE InGameCinematic before PlayAllFrames arg='%s'", arg ? arg : "<null>");
#endif
		CIN_PlayAllFrames(arg, 0, 0, 640, 480, 0, true);
#ifdef _XBOX
		g_SPXBCinPhase = 204;
		XBLF("JA: CIN_PHASE InGameCinematic after PlayAllFrames");
#endif
	}
	else
	{
		qbInGameCinematicOnStandBy = qtrue;
		strcpy(sInGameCinematicStandingBy,Cmd_Argv(1));
#ifdef _XBOX
		g_SPXBCinPhase = 210;
		CIN_XboxCopyArgLast(sInGameCinematicStandingBy);
		XBLF("JA: CIN_PHASE InGameCinematic standby arg='%s' state=%d",
			sInGameCinematicStandingBy, (int)cls.state);
#endif
	}
}

qboolean CL_InGameCinematicOnStandBy(void)
{
	return qbInGameCinematicOnStandBy;
}

// Used by fatal error handler
void MuteBinkSystem( void )
{
	bVideo.SetMasterVolume( 0 );
}
/***** END *****/

