// common.c -- misc functions used in client and server

#include "../game/q_shared.h"
#include "qcommon.h"
#include "../qcommon/sstring.h"	// to get Gil's string class, because MS's doesn't compile properly in here
#include "stv_version.h"

#ifdef _XBOX
#include "../win32/win_file.h"
#include "../ui/ui_splash.h"
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBComFrameCount;
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBComSubphase;
extern "C" volatile unsigned int g_SPXBComTailStage;
extern "C" volatile unsigned int g_SPXBComFrameDepth;
extern "C" volatile unsigned int g_SPXBComCatchCount;
extern "C" volatile unsigned int g_SPXBComSpinCount;
extern "C" volatile unsigned int g_SPXBComMsec;
extern "C" volatile unsigned int g_SPXBComFrameTime;
extern "C" volatile unsigned int g_SPXBComLastTime;
extern "C" volatile unsigned int g_SPXBBootPhase;
extern "C" volatile unsigned int g_SPXBClsState;
extern "C" volatile unsigned int g_SPXBPerfFrameMsec;
extern "C" volatile unsigned int g_SPXBPerfServerMsec;
extern "C" volatile unsigned int g_SPXBPerfClientMsec;
extern "C" volatile unsigned int g_SPXBPerfGameMsec;
extern "C" volatile unsigned int g_SPXBPerfFrontendMsec;
extern "C" volatile unsigned int g_SPXBPerfBackendMsec;
extern "C" volatile unsigned int g_SPXBPerfComEventMsec;
extern "C" volatile unsigned int g_SPXBPerfComCommandMsec;
extern "C" volatile unsigned int g_SPXBCmdExecCount;
extern "C" volatile char g_SPXBCmdLast[128];
// Count, up to 10 twelve-word rows, version at 128. Rows contain six timings,
// command counts for both buffers, then the last command (16 bytes).
// Read only by the external test harness after emulation is paused.
extern "C" { volatile unsigned int g_SPXBLongFrames[129] = {0}; }
extern bool Sys_IsDirectMapBoot(void);
#endif

#include "platform.h"

#define	MAXPRINTMSG	4096

#define MAX_NUM_ARGVS	50

int		com_argc;
char	*com_argv[MAX_NUM_ARGVS+1];

#ifndef _XBOX
static fileHandle_t	logfile;
static fileHandle_t	speedslog;
static fileHandle_t	camerafile;
fileHandle_t	com_journalFile;
fileHandle_t	com_journalDataFile;		// config files are written here
#endif

// Global language setting - this should be used instead of the myriad language
// cvars. Will be one of the Xbox values: XC_LANGUAGE_(ENGLISH|FRENCH|GERMAN)
DWORD	g_dwLanguage;

cvar_t	*com_viewlog;
cvar_t	*com_speeds;
cvar_t	*com_developer;
cvar_t	*com_timescale;
cvar_t	*com_fixedtime;
cvar_t	*com_maxfps;
cvar_t	*com_sv_running;
cvar_t	*com_cl_running;
cvar_t	*com_logfile;		// 1 = buffer log, 2 = flush after each print
cvar_t	*com_showtrace;
cvar_t	*com_terrainPhysics;
cvar_t	*com_version;
cvar_t	*com_buildScript;	// for automated data building scripts
cvar_t	*cl_paused;
cvar_t	*sv_paused;
cvar_t	*com_skippingcin;
cvar_t	*stefx_smokeFastTime;
cvar_t	*stefx_smokeFastTimeMsec;
cvar_t	*com_speedslog;		// 1 = buffer log, 2 = flush after each print
extern cvar_t *inSplashMenu;
extern cvar_t *controllerOut;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean Com_STEFXSmokeHarnessEnabled( void )
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
	}

	return s_enabled;
}
#endif

#ifdef G2_PERFORMANCE_ANALYSIS
cvar_t	*com_G2Report;
#endif


// com_speeds times
int		time_game;
int		time_frontend;		// renderer frontend time
int		time_backend;		// renderer backend time

int		timeInTrace;
int		timeInPVSCheck;
int		numTraces;

int			com_frameTime;
int			com_frameMsec;
int			com_frameNumber = 0;

qboolean	com_errorEntered;
qboolean	com_fullyInitialized = qfalse;

char	com_errorMessage[MAXPRINTMSG];

void Com_WriteConfig_f( void );

//============================================================================

#ifndef _XBOX
static char	*rd_buffer;
static int	rd_buffersize;
static void	(*rd_flush)( char *buffer );

void Com_BeginRedirect (char *buffer, int buffersize, void (*flush)( char *) )
{
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (void)
{
	if ( rd_flush ) {
		rd_flush(rd_buffer);
	}

	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}
#ifndef FINAL_BUILD
#define OUTPUT_TO_BUILD_WINDOW
#endif
#endif	//not xbox

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the apropriate place.

A raw string should NEVER be passed as fmt, because of "%f" type crashers.
=============
*/
void QDECL Com_Printf( const char *fmt, ... ) {
#ifndef FINAL_BUILD
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

#ifndef _XBOX
	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		strcat (rd_buffer, msg);
		return;
	}
#endif

	CL_ConsolePrint( msg );

	// echo to dedicated console and early console
	Sys_Print( msg );

#ifdef OUTPUT_TO_BUILD_WINDOW
	OutputDebugString(msg);
#endif

#ifndef _XBOX
	// logfile
	if ( com_logfile && com_logfile->integer ) {
		if ( !logfile ) {
			logfile = FS_FOpenFileWrite( "qconsole.log" );
			if ( com_logfile->integer > 1 ) {
				// force it to not buffer so we get valid
				// data even if we are crashing
				FS_ForceFlush(logfile);
			}
		}
		if ( logfile ) {
			FS_Write(msg, strlen(msg), logfile);
		}
	}
#endif
#endif
}

void QDECL Com_PrintfAlways( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

#ifndef _XBOX
	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		strcat (rd_buffer, msg);
		return;
	}
#endif

	CL_ConsolePrint( msg );

	// echo to dedicated console and early console
#ifndef FINAL_BUILD
	Sys_Print( msg );

#ifdef OUTPUT_TO_BUILD_WINDOW
	OutputDebugString(msg);
#endif
#endif

#ifndef _XBOX
	// logfile
	if ( com_logfile && com_logfile->integer ) {
		if ( !logfile ) {
			logfile = FS_FOpenFileWrite( "qconsole.log" );
			if ( com_logfile->integer > 1 ) {
				// force it to not buffer so we get valid
				// data even if we are crashing
				FS_ForceFlush(logfile);
			}
		}
		if ( logfile ) {
			FS_Write(msg, strlen(msg), logfile);
		}
	}
#endif
}



/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void QDECL Com_DPrintf( const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	if ( !com_developer || !com_developer->integer ) {
		return;			// don't confuse non-developers with techie stuff...
	}

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	
	Com_Printf ("%s", msg);
}

void Com_WriteCam ( const char *text )
{
#ifndef _XBOX
	static	char	mapname[MAX_QPATH];
	// camerafile
	if ( !camerafile ) 
	{
		extern	cvar_t	*sv_mapname;

		//NOTE: always saves in working dir if using one...
		sprintf( mapname, "maps/%s_cam.map", sv_mapname->string );
		camerafile = FS_FOpenFileWrite( mapname );
	}

	if ( camerafile ) 
	{
		FS_Printf( camerafile, "%s", text );
	}

	Com_Printf( "%s\n", mapname );
#endif
}

void Com_FlushCamFile()
{
#ifndef _XBOX
	if (!camerafile)
	{
		// nothing to flush, right?
		Com_Printf("No cam file available\n");
		return;
	}
	FS_ForceFlush(camerafile);
	FS_FCloseFile (camerafile);
	camerafile = 0;

	static	char	flushedMapname[MAX_QPATH];
	extern	cvar_t	*sv_mapname;
	sprintf( flushedMapname, "maps/%s_cam.map", sv_mapname->string );
	Com_Printf("flushed all cams to %s\n", flushedMapname);
#endif
}

/*
=============
Com_Error

Both client and server can use this, and it will
do the apropriate things.
=============
*/

void SG_WipeSavegame(const char *name);	// pretty sucky, but that's how SoF did it...<g>
void SG_Shutdown();
//void SCR_UnprecacheScreenshot();

void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list		argptr;

#if defined(_WIN32) && defined(_DEBUG)
	if ( code != ERR_DISCONNECT && code != ERR_NEED_CD ) {
//		if (com_noErrorInterrupt && !com_noErrorInterrupt->integer) 
		{
			__asm {
				int 0x03
			}
		}
	}
#endif

	// when we are running automated scripts, make sure we
	// know if anything failed
	if ( com_buildScript && com_buildScript->integer ) {
		code = ERR_FATAL;
	}

	if ( com_errorEntered ) {
		Sys_Error( "recursive error after: %s", com_errorMessage );
	}
	
	com_errorEntered = qtrue;

	//reset some game stuff here
//	SCR_UnprecacheScreenshot();

	va_start (argptr,fmt);
	vsprintf (com_errorMessage,fmt,argptr);
	va_end (argptr);	

#ifdef _XBOX
	XBLog_WriteCriticalf("JA: Com_Error code=%d message=%s", code, com_errorMessage);
#if defined(STEFX_SP_HOSTED_MP)
	XBLog_WriteCriticalf("STEFX_HM_SP: Com_Error code=%d message='%s'",
		code, com_errorMessage);
#endif
#endif

	if ( code != ERR_DISCONNECT ) {
		Cvar_Get("com_errorMessage", "", CVAR_ROM);	//give com_errorMessage a default so it won't come back to life after a resetDefaults
		Cvar_Set("com_errorMessage", com_errorMessage);
	}

	SG_Shutdown();				// close any file pointers
	if ( code == ERR_DISCONNECT ) {
		SV_Shutdown("Disconnect");
		CL_Disconnect();
		CL_FlushMemory();
		CL_StartHunkUsers();
		com_errorEntered = qfalse;
		throw ("DISCONNECTED\n");
	} else if ( code == ERR_DROP ) {
		// If loading/saving caused the crash/error - delete the temp file
	//	SG_WipeSavegame("current");	// delete file

		SV_Shutdown (va("Server crashed: %s\n",  com_errorMessage));
		CL_Disconnect();
		if ( com_cl_running && com_cl_running->integer ) {
			CL_FlushMemory();
			CL_StartHunkUsers();
		}
		Com_Printf (S_COLOR_RED"********************\n"S_COLOR_MAGENTA"ERROR: %s\n"S_COLOR_RED"********************\n", com_errorMessage);
		com_errorEntered = qfalse;
		throw ("DROPPED\n");
	} else if ( code == ERR_NEED_CD ) {
		SV_Shutdown( "Server didn't have CD\n" );
		if ( com_cl_running && com_cl_running->integer ) {
			CL_Disconnect();
			CL_FlushMemory();
			CL_StartHunkUsers();
			com_errorEntered = qfalse;
		} else {
			Com_Printf("Server didn't have CD\n" );
		}
		throw ("NEED CD\n");
	} else {
		CL_Shutdown ();
		SV_Shutdown (va(S_COLOR_RED"Server fatal crashed: %s\n", com_errorMessage));
	}

	Com_Shutdown ();

	Sys_Error ("%s", com_errorMessage);
}


/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Quit_f( void ) {
#ifdef _XBOX
	XBLog_Write("JA: Com_Quit_f entered");
#endif
	// don't try to shutdown if we are in a recursive error
	if ( !com_errorEntered ) {
		SV_Shutdown ("Server quit\n");
		CL_Shutdown ();
		Com_Shutdown ();
	}
	Sys_Quit ();
}



/*
============================================================================

COMMAND LINE FUNCTIONS

+ characters seperate the commandLine string into multiple console
command lines.

All of these are valid:

quake3 +set test blah +map test
quake3 set test blah+map test
quake3 set test blah + map test

============================================================================
*/

#define	MAX_CONSOLE_LINES	32
int		com_numConsoleLines;
char	*com_consoleLines[MAX_CONSOLE_LINES];

/*
==================
Com_ParseCommandLine

Break it up into multiple console lines
==================
*/
void Com_ParseCommandLine( char *commandLine ) {
	com_consoleLines[0] = commandLine;
	com_numConsoleLines = 1;

	while ( *commandLine ) {
		// look for a + seperating character
		// if commandLine came from a file, we might have real line seperators
		if ( *commandLine == '+' || *commandLine == '\n' ) {
			if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
				return;
			}
			com_consoleLines[com_numConsoleLines] = commandLine + 1;
			com_numConsoleLines++;
			*commandLine = 0;
		}
		commandLine++;
	}
}


/*
===================
Com_SafeMode

Check for "safe" on the command line, which will
skip loading of jaconfig.cfg
===================
*/
qboolean Com_SafeMode( void ) {
	int		i;

	for ( i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" )
			|| !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = 0;
			return qtrue;
		}
	}
	return qfalse;
}


/*
===============
Com_StartupVariable

Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
===============
*/
void Com_StartupVariable( const char *match ) {
	int		i;
	char	*s;
	cvar_t	*cv;

	for (i=0 ; i < com_numConsoleLines ; i++) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( strcmp( Cmd_Argv(0), "set" ) ) {
			continue;
		}

		s = Cmd_Argv(1);
		if ( !match || !stricmp( s, match ) ) {
			Cvar_Set( s, Cmd_Argv(2) );
			cv = Cvar_Get( s, "", 0 );
			cv->flags |= CVAR_USER_CREATED;
//			com_consoleLines[i] = 0;
		}
	}
}


/*
=================
Com_AddStartupCommands

Adds command line parameters as script statements
Commands are seperated by + signs

Returns qtrue if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
qboolean Com_AddStartupCommands( void ) {
	int		i;
	qboolean	added;

	added = qfalse;
	// quote every token, so args with semicolons can work
	for (i=0 ; i < com_numConsoleLines ; i++) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		// set commands won't override menu startup
		if ( Q_stricmpn( com_consoleLines[i], "set", 3 ) ) {
			added = qtrue;
		}
		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}


//============================================================================


void Info_Print( const char *s ) {
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

/*
============
Com_StringContains
============
*/
char *Com_StringContains(char *str1, char *str2, int casesensitive) {
	int len, i, j;

	len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (casesensitive) {
				if (str1[j] != str2[j]) {
					break;
				}
			}
			else {
				if (toupper(str1[j]) != toupper(str2[j])) {
					break;
				}
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return NULL;
}

/*
============
Com_Filter
============
*/
int Com_Filter(char *filter, char *name, int casesensitive) {
	char buf[MAX_TOKEN_CHARS];
	char *ptr;
	int i;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?') {
					break;
				}
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (strlen(buf)) {
				ptr = Com_StringContains(name, buf, casesensitive);
				if (!ptr) {
					return qfalse;
				}
				name = ptr + strlen(buf);
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else {
			if (casesensitive) {
				if (*filter != *name) {
					return qfalse;
				}
			}
			else {
				if (toupper(*filter) != toupper(*name)) {
					return qfalse;
				}
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}



/*
=================
Com_InitHunkMemory
=================
*/
#if !defined(_XBOX)
void Com_InitHunkMemory( void )
{
	Hunk_Clear();

//	Cmd_AddCommand( "meminfo", Z_Details_f );
}

// I'm leaving this in just in case we ever need to remember where's a good place to hook something like this in.
//
void Com_ShutdownHunkMemory(void)
{
}


/*
===================
Hunk_SetMark

The server calls this after the level and game VM have been loaded
===================
*/
void Hunk_SetMark( void ) 
{
}



/*
=================
Hunk_ClearToMark

The client calls this before starting a vid_restart or snd_restart
=================
*/
void Hunk_ClearToMark( void ) 
{
	Z_TagFree(TAG_HUNKALLOC);	
//	Z_TagFree(TAG_HUNKMISCMODELS);
}



/*
=================
Hunk_Clear

The server calls this before shutting down or loading a new map
=================
*/
void Hunk_Clear( void ) 
{
	Z_TagFree(TAG_HUNKALLOC);
//	Z_TagFree(TAG_HUNKMISCMODELS);

	extern void CIN_CloseAllVideos();
				CIN_CloseAllVideos();
}
#endif






/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

#define	MAX_PUSHED_EVENTS	64
int			com_pushedEventsHead, com_pushedEventsTail;
sysEvent_t	com_pushedEvents[MAX_PUSHED_EVENTS];

/*
=================
Com_GetRealEvent
=================
*/
sysEvent_t	Com_GetRealEvent( void ) {
	sysEvent_t	ev;

	// get an event from the system
		ev = Sys_GetEvent();

	return ev;
}

/*
=================
Com_PushEvent
=================
*/
void Com_PushEvent( sysEvent_t *event ) {
	sysEvent_t		*ev;
	static int		printedWarning;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

/*
=================
Com_GetEvent
=================
*/
sysEvent_t	Com_GetEvent( void ) {
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}

/*
=================
Com_RunAndTimeServerPacket
=================
*/
void Com_RunAndTimeServerPacket( netadr_t *evFrom, msg_t *buf ) {
	int		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds ();
	}

	SV_PacketEvent( *evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds ();
		msec = t2 - t1;
		if ( com_speeds->integer == 3 ) {
			Com_Printf( "SV_PacketEvent time: %i\n", msec );
		}
	}
}

/*
=================
Com_EventLoop

Returns last event time
=================
*/
int Com_EventLoop( void ) {
	sysEvent_t	ev;
	netadr_t	evFrom;
	byte		bufData[MAX_MSGLEN];
	msg_t		buf;
#ifdef _XBOX
	static int s_xboxEventLoopTraceBudget = 24;
	const qboolean xboxTraceEventLoop = (s_xboxEventLoopTraceBudget > 0);
	if (xboxTraceEventLoop)
	{
		Com_PrintfAlways("JA: Com_EventLoop enter pushed=%d/%d\n", com_pushedEventsHead, com_pushedEventsTail);
	}
#endif

	MSG_Init( &buf, bufData, sizeof( bufData ) );

	while ( 1 ) {
#ifdef _XBOX
		if (xboxTraceEventLoop) Com_PrintfAlways("JA: Com_EventLoop before Com_GetEvent\n");
#endif
		ev = Com_GetEvent();
#ifdef _XBOX
		if (xboxTraceEventLoop)
		{
			Com_PrintfAlways("JA: Com_EventLoop got event type=%d time=%d value=%d value2=%d ptr=%p len=%d\n",
				ev.evType, ev.evTime, ev.evValue, ev.evValue2, ev.evPtr, ev.evPtrLength);
		}
#endif

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
#ifdef _XBOX
			if (xboxTraceEventLoop) Com_PrintfAlways("JA: Com_EventLoop SE_NONE before NS_CLIENT drain\n");
			int xboxClientLoopPackets = 0;
			qboolean xboxClientLoopCapped = qfalse;
#endif
			// manually send packet events for the loopback channel
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
#ifdef _XBOX
				if (xboxClientLoopPackets++ >= 128) {
					xboxClientLoopCapped = qtrue;
					break;
				}
#endif
#ifdef _XBOX
				static int s_xboxClientLoopLogs = 0;
				if (s_xboxClientLoopLogs < 16)
				{
					Com_PrintfAlways("JA: Com_EventLoop dispatch NS_CLIENT loop packet size=%d\n", buf.cursize);
					++s_xboxClientLoopLogs;
				}
#endif
				CL_PacketEvent( evFrom, &buf );
#ifdef _XBOX
				if (xboxTraceEventLoop) Com_PrintfAlways("JA: Com_EventLoop after NS_CLIENT packet\n");
#endif
			}
#ifdef _XBOX
			if (xboxClientLoopCapped) {
				static int s_xboxClientLoopCapLogs = 0;
				if (s_xboxClientLoopCapLogs < 8) {
					Com_PrintfAlways("JA: Com_EventLoop capped NS_CLIENT drain at %d packets\n", xboxClientLoopPackets);
					++s_xboxClientLoopCapLogs;
				}
			}
#endif

#ifdef _XBOX
			if (xboxTraceEventLoop) Com_PrintfAlways("JA: Com_EventLoop before NS_SERVER drain\n");
			int xboxServerLoopPackets = 0;
			qboolean xboxServerLoopCapped = qfalse;
#endif
			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
#ifdef _XBOX
				if (xboxServerLoopPackets++ >= 128) {
					xboxServerLoopCapped = qtrue;
					break;
				}
#endif
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
#ifdef _XBOX
					static int s_xboxServerLoopLogs = 0;
					if (s_xboxServerLoopLogs < 16)
					{
						Com_PrintfAlways("JA: Com_EventLoop dispatch NS_SERVER loop packet size=%d\n", buf.cursize);
						++s_xboxServerLoopLogs;
					}
#endif
					Com_RunAndTimeServerPacket( &evFrom, &buf );
#ifdef _XBOX
					if (xboxTraceEventLoop) Com_PrintfAlways("JA: Com_EventLoop after NS_SERVER packet\n");
#endif
				}
			}
#ifdef _XBOX
			if (xboxServerLoopCapped) {
				static int s_xboxServerLoopCapLogs = 0;
				if (s_xboxServerLoopCapLogs < 8) {
					Com_PrintfAlways("JA: Com_EventLoop capped NS_SERVER drain at %d packets\n", xboxServerLoopPackets);
					++s_xboxServerLoopCapLogs;
				}
			}
#endif

#ifdef _XBOX
			if (xboxTraceEventLoop)
			{
				Com_PrintfAlways("JA: Com_EventLoop return time=%d\n", ev.evTime);
				--s_xboxEventLoopTraceBudget;
			}
#endif
			return ev.evTime;
		}


		switch ( ev.evType ) {
		default:
			Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evTime );
			break;
        case SE_NONE:
            break;
		case SE_KEY:
			CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.evValue );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_CONSOLE:
			Cbuf_AddText( (char *)ev.evPtr );
			Cbuf_AddText( "\n" );
			break;
		case SE_PACKET:
			evFrom = *(netadr_t *)ev.evPtr;
			buf.cursize = ev.evPtrLength - sizeof( evFrom );

			// we must copy the contents of the message out, because
			// the event buffers are only large enough to hold the
			// exact payload, but channel messages need to be large
			// enough to hold fragment reassembly
			if ( (unsigned)buf.cursize > buf.maxsize ) {
				Com_Printf("Com_EventLoop: oversize packet\n");
				continue;
			}
			memcpy( buf.data, (byte *)((netadr_t *)ev.evPtr + 1), buf.cursize );
			if ( com_sv_running->integer ) {
				Com_RunAndTimeServerPacket( &evFrom, &buf );
			} else {
				CL_PacketEvent( evFrom, &buf );
			}
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
		}
	}
}

/*
================
Com_Milliseconds

Can be used for profiling, but will be journaled accurately
================
*/
int Com_Milliseconds (void) {
	sysEvent_t	ev;

	// get events and push them until we get a null event with the current time
	do {

		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );
	
	return ev.evTime;
}

//============================================================================

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void Com_Error_f (void) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f (void) {
	float	s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = atof( Cmd_Argv(1) );

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( ( now - start ) * 0.001 > s ) {
			break;
		}
	}
}

/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void Com_Crash_f( void ) {
	* ( int * ) 0 = 0x12345678;
}

/*
=================
Com_Init
=================
*/
extern void Com_InitZoneMemory();
extern void R_InitWorldEffects();
void Com_Init( char *commandLine ) {
	char	*s;

#ifdef _XBOX
	g_SPXBBootPhase = 0x300;
#endif
	XBLog_Write("JA: Com_Init entered");
#ifdef _XBOX
	g_SPXBBootPhase = 0x301;
#endif
	Com_Printf( "%s %s %s\n", Q3_VERSION, CPUSTRING, __DATE__ );
#ifdef _XBOX
	g_SPXBBootPhase = 0x302;
#endif

	try {
		// Grab the user's langauge preference from the dashboard right away!
		XBLog_Write("JA: XGetLanguage...");
		g_dwLanguage = XGetLanguage();
#ifdef _XBOX
		g_SPXBBootPhase = 0x303;
#endif
		if( g_dwLanguage != XC_LANGUAGE_FRENCH && g_dwLanguage != XC_LANGUAGE_GERMAN )
			g_dwLanguage = XC_LANGUAGE_ENGLISH;
		XBLog_Write("JA: XGetLanguage done");

		// prepare enough of the subsystems to handle
		// cvar and command buffer management
		XBLog_Write("JA: Com_ParseCommandLine...");
		Com_ParseCommandLine( commandLine );
#ifdef _XBOX
		g_SPXBBootPhase = 0x304;
#endif

		XBLog_Write("JA: Swap_Init...");
		Swap_Init ();
#ifdef _XBOX
		g_SPXBBootPhase = 0x305;
#endif
		XBLog_Write("JA: Cbuf_Init...");
		Cbuf_Init ();
#ifdef _XBOX
		g_SPXBBootPhase = 0x306;
#endif

		XBLog_Write("JA: Com_InitZoneMemory...");
		Com_InitZoneMemory();
#ifdef _XBOX
		g_SPXBBootPhase = 0x307;
#endif
		XBLog_Write("JA: Com_InitZoneMemory done");

#ifdef _XBOX
		XBLog_Write("JA: WF_Init...");
		WF_Init();
		g_SPXBBootPhase = 0x308;
		XBLog_Write("JA: WF_Init done");
		XBLog_Write("JA: CL_InitRef...");
		// set up ri
		extern void CL_InitRef( void );
		CL_InitRef();
		g_SPXBBootPhase = 0x309;
		XBLog_Write("JA: CL_InitRef done");
		XBLog_Write("JA: R_Register...");
		// register renderer cvars
		extern void R_Register(void);
		R_Register();
		g_SPXBBootPhase = 0x30A;
		XBLog_Write("JA: R_Register done");
		XBLog_Write("JA: GLimp_Init...");
		// start the gl render layer
		extern void GLimp_Init(void);
		GLimp_Init();
		g_SPXBBootPhase = 0x30B;
		XBLog_Write("JA: GLimp_Init done");
		// put up the license screen
		XBLog_Write("STEFX: SP_DoLicense (EF intro owns frontend movies)...");
		SP_DoLicense();
		g_SPXBBootPhase = 0x30C;
		XBLog_Write("STEFX: SP_DoLicense done");
#endif

		XBLog_Write("JA: Cmd_Init...");
		Cmd_Init ();
#ifdef _XBOX
		g_SPXBBootPhase = 0x30D;
#endif
		XBLog_Write("JA: Cvar_Init...");
		Cvar_Init ();
#ifdef _XBOX
		g_SPXBBootPhase = 0x30E;
#endif

		// get the commandline cvars set
		XBLog_Write("JA: Com_StartupVariable...");
		Com_StartupVariable( NULL );

		// done early so bind command exists
		XBLog_Write("JA: CL_InitKeyCommands...");
		CL_InitKeyCommands();

#ifdef _XBOX
		XBLog_Write("JA: Sys_InitFileCodes...");
		extern void Sys_FilecodeScan_f();
		Sys_InitFileCodes();
		Cmd_AddCommand("filecodes", Sys_FilecodeScan_f);
		XBLog_Write("JA: Sys_InitFileCodes done");

		XBLog_Write("JA: Sys_StreamInit...");
		extern void Sys_StreamInit();
		Sys_StreamInit();
		XBLog_Write("JA: Sys_StreamInit done");

		XBLog_Write("STEFX: Ghoul2 info array init skipped");
#endif

		XBLog_Write("JA: FS_InitFilesystem...");
		FS_InitFilesystem ();	//uses z_malloc
#ifdef _XBOX
		g_SPXBBootPhase = 0x30F;
#endif
		XBLog_Write("JA: FS_InitFilesystem done");
		XBLog_Write("JA: R_InitWorldEffects...");
		R_InitWorldEffects();   // this doesn't do much but I want to be sure certain variables are intialized.
		
		XBLog_Write("JA: exec default.cfg...");
		Cbuf_AddText ("exec default.cfg\n");

		// skip the jaconfig.cfg if "safe" is on the command line
		if ( !Com_SafeMode() ) {
			Cbuf_AddText ("exec jaconfig.cfg\n");
		}

		Cbuf_AddText ("exec autoexec.cfg\n");

		XBLog_Write("JA: Cbuf_Execute (configs)...");
		Cbuf_Execute ();
		XBLog_Write("JA: Config execution done");
#ifdef _XBOX
		XBLog_Write("STEFX_INPUT_AUDIT begin");
		Key_XboxAuditMenuBindings();
		XBLog_Write("STEFX_INPUT_AUDIT end");
		/* Applies to SP and SP-hosted Holomatch alike: PAK2.PK3 ships the
		 * retail PC default.cfg whose unbindall wins the Xbox filesystem
		 * lookup and strips every button binding.  Without this block the
		 * SP build boots with analog look/move only and no usable buttons. */
		/* BaseEF/PAK2.PK3 also contains the retail PC default.cfg, whose
		 * unbindall wins the ambiguous filesystem lookup on Xbox.  Apply the
		 * canonical loose Xbox defaults directly after all file-backed config
		 * layers so package ordering cannot strip P1 actions or flip look Y. */
		Cbuf_AddText ("seta cl_freelook 1\n");
		Cbuf_AddText ("seta cl_run 0\n");
		Cbuf_AddText ("seta ui_thumbStickMode 0\n");
		Cbuf_AddText ("seta m_pitch -0.022\n");
		Cbuf_AddText ("seta sensitivity 2\n");
		Cbuf_AddText ("seta sensitivityY 2\n");
		Cbuf_AddText ("seta joy_deadzone 0.18\n");
		Cbuf_AddText ("unbind JOY0\n");
		Cbuf_AddText ("bind JOY1 datapad\n");
		Cbuf_AddText ("bind JOY2 \"toggle cl_run\"\n");
		Cbuf_AddText ("bind JOY3 \"toggle cg_thirdperson\"\n");
		Cbuf_AddText ("bind JOY4 uimenu\n");
		Cbuf_AddText ("bind JOY5 +zoom\n");
		Cbuf_AddText ("unbind JOY6\n");
		Cbuf_AddText ("unbind JOY7\n");
		Cbuf_AddText ("unbind JOY8\n");
		Cbuf_AddText ("bind JOY9 weapnext\n");
		Cbuf_AddText ("bind JOY10 weapprev\n");
		Cbuf_AddText ("bind JOY11 +altattack\n");
		Cbuf_AddText ("bind JOY12 +attack\n");
		Cbuf_AddText ("bind JOY13 \"centerview; zoomoff\"\n");
		Cbuf_AddText ("bind JOY14 +movedown\n");
		Cbuf_AddText ("bind JOY15 +moveup\n");
		Cbuf_AddText ("bind JOY16 +use\n");
		Cbuf_Execute ();
		XBLog_WriteCriticalf("STEFX_HM_P1_SP_CONFIG: source=embedded-canonical-xbox mPitch=%g stickMode=%d sensitivity=%g sensitivityY=%g deadzone=%g",
			Cvar_VariableValue("m_pitch"),
			Cvar_VariableIntegerValue("ui_thumbStickMode"),
			Cvar_VariableValue("sensitivity"),
			Cvar_VariableValue("sensitivityY"),
			Cvar_VariableValue("joy_deadzone"));
		Key_XboxAuditMenuBindings();
#endif

		// override anything from the config files with command line args
		Com_StartupVariable( NULL );

		// allocate the stack based hunk allocator
		XBLog_Write("JA: Com_InitHunkMemory...");
		Com_InitHunkMemory();
#ifdef _XBOX
		g_SPXBBootPhase = 0x310;
#endif
		XBLog_Write("JA: Com_InitHunkMemory done");

		// if any archived cvars are modified after this, we will trigger a writing
		// of the config file
		cvar_modifiedFlags &= ~CVAR_ARCHIVE;
		
		//
		// init commands and vars
		//
		Cmd_AddCommand ("quit", Com_Quit_f);
		Cmd_AddCommand ("writeconfig", Com_WriteConfig_f );
		
		com_maxfps = Cvar_Get ("com_maxfps", "85", CVAR_ARCHIVE);
		
		com_developer = Cvar_Get ("developer", "0", CVAR_TEMP );
		com_logfile = Cvar_Get ("logfile", "0", CVAR_TEMP );
		com_speedslog = Cvar_Get ("speedslog", "0", CVAR_TEMP );
		
		com_timescale = Cvar_Get ("timescale", "1", CVAR_CHEAT );
		com_fixedtime = Cvar_Get ("fixedtime", "0", CVAR_CHEAT);
		com_showtrace = Cvar_Get ("com_showtrace", "0", CVAR_CHEAT);
		com_terrainPhysics = Cvar_Get ("com_terrainPhysics", "1", CVAR_CHEAT);
		com_viewlog = Cvar_Get( "viewlog", "0", CVAR_TEMP );
		com_speeds = Cvar_Get ("com_speeds", "0", 0);
		
#ifdef G2_PERFORMANCE_ANALYSIS
		com_G2Report = Cvar_Get("com_G2Report", "0", 0);
#endif

		cl_paused	   = Cvar_Get ("cl_paused", "0", CVAR_ROM);
		sv_paused	   = Cvar_Get ("sv_paused", "0", CVAR_ROM);
		com_sv_running = Cvar_Get ("sv_running", "0", CVAR_ROM);
		com_cl_running = Cvar_Get ("cl_running", "0", CVAR_ROM);
		com_skippingcin = Cvar_Get ("skippingCinematic", "0", CVAR_ROM);
		stefx_smokeFastTime = Cvar_Get ("stefx_smoke_fasttime", "0", CVAR_TEMP);
		stefx_smokeFastTimeMsec = Cvar_Get ("stefx_smoke_fasttime_msec", "500", CVAR_TEMP);
		com_buildScript = Cvar_Get( "com_buildScript", "0", 0 );
		
		if ( com_developer && com_developer->integer ) {
			Cmd_AddCommand ("error", Com_Error_f);
			Cmd_AddCommand ("crash", Com_Crash_f );
			Cmd_AddCommand ("freeze", Com_Freeze_f);
		}
		
		s = va("%s %s %s", Q3_VERSION, CPUSTRING, __DATE__ );
		com_version = Cvar_Get ("version", s, CVAR_ROM | CVAR_SERVERINFO );


		// So any controller can skip the logo movies:
		inSplashMenu = Cvar_Get( "inSplashMenu", "1", 0 );
		controllerOut= Cvar_Get( "ControllerOutNum", "-1", 0);

#ifdef XBOX_DEMO
		// Cvar used to hide "QUIT TO DEMOS MENU" options if we weren't started by CDX
		extern bool demoLaunchDataValid;
		if( demoLaunchDataValid )
			Cvar_SetValue( "ui_allowDemoQuit", 1 );
		else
			Cvar_SetValue( "ui_allowDemoQuit", 0 );
#endif

		XBLog_Write("JA: SE_Init...");
		SE_Init();	// Initialize StringEd
		XBLog_Write("JA: SE_Init done");

		XBLog_Write("JA: Sys_Init...");
		Sys_Init();	// this also detects CPU type, so I can now do this CPU check below...
		XBLog_Write("JA: Sys_Init done");

		XBLog_Write("JA: Netchan_Init...");
		Netchan_Init( Com_Milliseconds() & 0xffff );	// pick a port value that should be nice and random
//	VM_Init();
		XBLog_Write("JA: SV_Init...");
		SV_Init();
		XBLog_Write("JA: SV_Init done");

		XBLog_Write("JA: CL_Init...");
		CL_Init();
		XBLog_Write("JA: CL_Init done");

#ifdef _XBOX
		// Experiment. Sound memory never gets freed, move it earlier. This
		// will also let us play movies sooner, if we need to.
		XBLog_Write("JA: CL_StartSound...");
		extern void CL_StartSound(void);
		CL_StartSound();
		XBLog_Write("JA: CL_StartSound done");
#endif

		Sys_ShowConsole( com_viewlog->integer, qfalse );

		// set com_frameTime so that if a map is started on the
		// command line it will still be able to count on com_frameTime
		// being random enough for a serverid
		com_frameTime = Com_Milliseconds();
		XBLog_Write("JA: Com_Init fully initialized");

		// add + commands from command line
#if !defined(_XBOX) || defined(STEFX_ELITE_FORCE_SP)
		if ( !Com_AddStartupCommands() ) {
#ifdef NDEBUG
			// if the user didn't give any commands, run default action
//			if ( !com_dedicated->integer ) 
			{
#ifdef _XBOX
				if (Sys_IsDirectMapBoot())
				{
					XBLog_Write("STEFX: skipping EF frontend intro movies for explicit direct-map boot");
				}
				else
				{
					XBLog_Write("STEFX: normal EF frontend boot queues eflogo and intro movies before PS2 main menu");
					Cbuf_AddText ("cinematic eflogo\n");
					Cbuf_AddText ("cinematic intro\n");
				}
#else
				Cbuf_AddText ("cinematic eflogo\n");
				Cbuf_AddText ("cinematic intro\n");
#endif
			}
#endif	
		}
#endif
		com_fullyInitialized = qtrue;
		Com_Printf ("--- Common Initialization Complete ---\n");
#ifdef _XBOX
		g_SPXBBootPhase = 0x311;
#endif

//HACKERY FOR THE DEUTSCH		
		//if ( (Cvar_VariableIntegerValue("ui_iscensored") == 1) 	//if this was on before, set it again so it gets its flags
		//	)
		//{
		//	Cvar_Get( "ui_iscensored",   "1", CVAR_ARCHIVE|CVAR_ROM|CVAR_INIT|CVAR_CHEAT|CVAR_NORESTART);
		//	Cvar_Set( "ui_iscensored",   "1");	//just in case it was archived
		//	// NOTE : I also create this in UI_Init()
		//	Cvar_Get( "g_dismemberment", "0", CVAR_ARCHIVE|CVAR_ROM|CVAR_INIT|CVAR_CHEAT);
		//	Cvar_Set( "g_dismemberment", "0");	//just in case it was archived
		//}
	}

	catch (const char* reason) {
		Sys_Error ("Error during initialization %s", reason);
	}

#ifdef _XBOX
	//Load these early to keep them at the beginning of memory.  Perhaps
	//here is too early though.  After the license screen would be better.
	extern void SE_CheckForLanguageUpdates(void);
	SE_CheckForLanguageUpdates();
	g_SPXBBootPhase = 0x312;
#endif

}

//==================================================================

void Com_WriteConfigToFile( const char *filename ) {
#ifndef _XBOX
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( !f ) {
		Com_Printf ("Couldn't write %s.\n", filename );
		return;
	}

	FS_Printf (f, "// generated by Star Wars Jedi Academy, do not modify\n");
	Key_WriteBindings (f);
	Cvar_WriteVariables (f);
	FS_FCloseFile( f );
#endif
}


/*
===============
Com_WriteConfiguration

Writes key bindings and archived cvars to config file if modified
===============
*/
void Com_WriteConfiguration( void ) {
	// if we are quiting without fully initializing, make sure
	// we don't write out anything
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( "jaconfig.cfg" );
}


/*
===============
Com_WriteConfig_f

Write the config file to a specific name
===============
*/
void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename );
}

/*
================
Com_ModifyMsec
================
*/


int Com_ModifyMsec( int msec, float &fraction ) 
{
	int		clampTime;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean stefxSmokeFastTimeActive = (qboolean)(stefx_smokeFastTime && stefx_smokeFastTime->integer && Com_STEFXSmokeHarnessEnabled());
#else
	qboolean stefxSmokeFastTimeActive = (qboolean)(stefx_smokeFastTime && stefx_smokeFastTime->integer);
#endif

	fraction=0.0f;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) 
	{
		msec = com_fixedtime->integer;
	} 
	else if ( com_timescale->value ) 
	{
		fraction=(float)msec;
		fraction*=com_timescale->value;
		msec=(int)floor(fraction);
		fraction-=(float)msec;
	}
	
	// don't let it scale below 1 msec
	if ( msec < 1 ) 
	{
		msec = 1;
		fraction=0.0f;
	}

	if ( com_skippingcin->integer || stefxSmokeFastTimeActive ) {
		// we're skipping ahead so let it go a bit faster
		clampTime = 500;
		if ( stefxSmokeFastTimeActive && stefx_smokeFastTimeMsec )
		{
			clampTime = stefx_smokeFastTimeMsec->integer;
			if ( clampTime < 1 )
			{
				clampTime = 1;
			}
			else if ( clampTime > 2000 )
			{
				clampTime = 2000;
			}
		}
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
		fraction=0.0f;
	}

	return msec;
}

/*
=================
Com_Frame
=================
*/
static vec3_t corg;
static vec3_t cangles;
static bool bComma;
void Com_SetOrgAngles(vec3_t org,vec3_t angles)
{
	VectorCopy(org,corg);
	VectorCopy(angles,cangles);
}

#ifdef G2_PERFORMANCE_ANALYSIS
void G2Time_ResetTimers(void);
void G2Time_ReportTimers(void);
#endif

#pragma warning (disable: 4701)	//local may have been used without init (timing info vars)
void Com_Frame( void ) {
#if defined(_XBOX)
	const int xboxPerfFrameStart = Sys_Milliseconds();
	int xboxPerfServerStart = xboxPerfFrameStart;
	int xboxPerfClientStart = xboxPerfFrameStart;
	unsigned int xboxPerfComEventMsec = 0;
	unsigned int xboxPerfComCommandMsec = 0;
	unsigned int xboxFirstCommandMsec = 0, xboxSecondCommandMsec = 0;
	unsigned int xboxCommandStart = 0;
	unsigned int xboxCommandSerial = 0, xboxFirstCommands = 0, xboxSecondCommands = 0;
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComFrameDepth++;
	static unsigned int s_xboxComEntryLogCount = 0;
	qboolean xboxTraceActiveComTail = qfalse;
	int xboxTraceActiveComTailFrame = -1;
	int xboxPerfPhaseStart = xboxPerfFrameStart;
	g_SPXBPhaseLast = 0x43464E30; /* 'CFN0' */
	g_SPXBComTailStage = 0x434D3030; /* 'CM00' */
	g_SPXBComSubphase = 0;
	if (s_xboxComEntryLogCount < 16)
	{
		XBLF("JA: COM_PHASE entry top before try count=%u phase=%08x sub=%u\n", s_xboxComEntryLogCount, g_SPXBPhaseLast, g_SPXBComSubphase);
	}
	s_xboxComEntryLogCount++;
#endif
try
{
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComFrameCount++;
	g_SPXBPhaseLast = 0x434F4D31; /* 'COM1' */
	g_SPXBComSubphase = 1;
#endif
	int		timeBeforeFirstEvents, timeBeforeServer, timeBeforeEvents, timeBeforeClient, timeAfter;
	int		msec, minMsec;
	static int	lastTime;
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	static int frameCount = 0;
	const bool firstFrames = (frameCount < 8);
	static int s_xboxLastComPhaseTime = 0;
	static bool s_xboxTraceComPhase = false;
	g_SPXBComSubphase = 2;
	const int xboxPhaseNow = Sys_Milliseconds();
	g_SPXBComSubphase = 3;
	s_xboxTraceComPhase = firstFrames;
	if (s_xboxTraceComPhase)
	{
		s_xboxLastComPhaseTime = xboxPhaseNow;
		XBLF("JA: COM_PHASE frame=%d enter realtime=%d", frameCount, com_frameTime);
	}
	if (firstFrames) {
		XBLog_Write(va("JA: Com_Frame #%d entered", frameCount));
	}
	frameCount++;
	#endif

	// write config file if anything changed
#ifndef _XBOX
	Com_WriteConfiguration(); 

	// if "viewlog" has been modified, show or hide the log console
	if ( com_viewlog->modified ) {
		Sys_ShowConsole( com_viewlog->integer, qfalse );
		com_viewlog->modified = qfalse;
	}
#endif

	//
	// main event loop
	//
	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds ();
	}

	// we may want to spin here if things are going too fast
	if ( com_maxfps->integer > 0 ) {
		minMsec = 1000 / com_maxfps->integer;
	} else {
		minMsec = 1;
	}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	int xboxFirstEventSpinCount = 0;
	xboxPerfPhaseStart = Sys_Milliseconds();
	g_SPXBComSubphase = 4;
	g_SPXBComSpinCount = 0;
#endif
	do {
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 5;
		g_SPXBComSpinCount = (unsigned int)xboxFirstEventSpinCount;
		if (s_xboxTraceComPhase && xboxFirstEventSpinCount == 0) XBLog_Write("JA: COM_PHASE before first Com_EventLoop");
#endif
		com_frameTime = Com_EventLoop();
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 6;
		g_SPXBComFrameTime = (unsigned int)com_frameTime;
		g_SPXBComLastTime = (unsigned int)lastTime;
		if (s_xboxTraceComPhase && xboxFirstEventSpinCount == 0) XBLog_Write("JA: COM_PHASE after first Com_EventLoop");
#endif
		if ( lastTime > com_frameTime ) {
			lastTime = com_frameTime;		// possible on first frame
		}
		msec = com_frameTime - lastTime;
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 7;
		g_SPXBComMsec = (unsigned int)msec;
		g_SPXBComFrameTime = (unsigned int)com_frameTime;
		g_SPXBComLastTime = (unsigned int)lastTime;
		if ( msec < minMsec && ++xboxFirstEventSpinCount > 1024 )
		{
			com_frameTime = lastTime + minMsec;
			msec = minMsec;
			g_SPXBComSubphase = 8;
			g_SPXBComMsec = (unsigned int)msec;
			g_SPXBComFrameTime = (unsigned int)com_frameTime;
			g_SPXBComSpinCount = (unsigned int)xboxFirstEventSpinCount;
			if (s_xboxTraceComPhase) XBLF("JA: COM_PHASE first event timer stalled; forced msec=%d", msec);
			break;
		}
#endif
	} while ( msec < minMsec );
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPerfComEventMsec += (unsigned int)(Sys_Milliseconds() - xboxPerfPhaseStart);
	g_SPXBComSubphase = 9;
	if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE before first Cbuf_Execute");
	xboxPerfPhaseStart = Sys_Milliseconds();
#endif
#ifdef _XBOX
	xboxCommandStart = (unsigned int)Sys_Milliseconds();
	xboxCommandSerial = g_SPXBCmdExecCount;
#endif
	Cbuf_Execute ();
#ifdef _XBOX
	xboxFirstCommandMsec = (unsigned int)Sys_Milliseconds() - xboxCommandStart;
	xboxFirstCommands = g_SPXBCmdExecCount - xboxCommandSerial;
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	xboxPerfComCommandMsec += (unsigned int)(Sys_Milliseconds() - xboxPerfPhaseStart);
	g_SPXBComTailStage = 0x434D3031; /* 'CM01' */
	g_SPXBComSubphase = 10;
	if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE after first Cbuf_Execute");
#endif

	lastTime = com_frameTime;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComLastTime = (unsigned int)lastTime;
#endif

	// mess with msec if needed
	com_frameMsec = msec;
	float fractionMsec=0.0f;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComTailStage = 0x434D3032; /* 'CM02' */
#endif
	msec = Com_ModifyMsec( msec, fractionMsec);
#ifdef _XBOX
	g_SPXBComTailStage = 0x434D3033; /* 'CM03' */
#endif
	
	//
	// server side
	//
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds ();
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (firstFrames) XBLog_Write("JA: Com_Frame: SV_Frame...");
	g_SPXBComSubphase = 11;
#endif
#if defined(_XBOX)
	g_SPXBComTailStage = 0x434D3034; /* 'CM04': before server */
	xboxPerfServerStart = Sys_Milliseconds();
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE before SV_Frame");
#endif
	SV_Frame (msec, fractionMsec);
#if defined(_XBOX)
	g_SPXBPerfServerMsec = (unsigned int)(Sys_Milliseconds() - xboxPerfServerStart);
	g_SPXBComTailStage = 0x434D3035; /* 'CM05': server complete */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 12;
	if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE after SV_Frame");
#endif


	//
	// client system
	//
#ifdef _XBOX
	extern bool TestDemoTimer();
	extern void PlayDemo();
	if ( TestDemoTimer())
	{
		PlayDemo();
	}
#endif
//	if ( !com_dedicated->integer ) 
	{
		//
		// run event loop a second time to get server to client packets
		// without a frame of latency
		//
		if ( com_speeds->integer ) {
			timeBeforeEvents = Sys_Milliseconds ();
		}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 13;
		if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE before second Com_EventLoop");
		xboxPerfPhaseStart = Sys_Milliseconds();
#endif
		#ifdef _XBOX
		g_SPXBComTailStage = 0x434D3036; /* 'CM06': before second event loop */
		#endif
		Com_EventLoop();
		#ifdef _XBOX
		g_SPXBComTailStage = 0x434D3037; /* 'CM07': second event loop complete */
		#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		xboxPerfComEventMsec += (unsigned int)(Sys_Milliseconds() - xboxPerfPhaseStart);
		g_SPXBComSubphase = 14;
		if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE after second Com_EventLoop");
		xboxPerfPhaseStart = Sys_Milliseconds();
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 15;
		if (s_xboxTraceComPhase) XBLF("JA: COM_PHASE before second Cbuf_Execute state=%d sv=%d direct=%d",
			(int)g_SPXBClsState,
			com_sv_running ? com_sv_running->integer : -1,
			Sys_IsDirectMapBoot() ? 1 : 0);
#endif
#ifdef _XBOX
		g_SPXBComTailStage = 0x434D3038; /* 'CM08': before second command buffer */
		if ( Sys_IsDirectMapBoot() && g_SPXBClsState != (unsigned int)CA_ACTIVE )
		{
			#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
			if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE skip second Cbuf_Execute during direct-map load");
			#endif
		}
		else
#endif
		{
			#ifdef _XBOX
			xboxCommandStart = (unsigned int)Sys_Milliseconds();
			xboxCommandSerial = g_SPXBCmdExecCount;
			#endif
			Cbuf_Execute ();
			#ifdef _XBOX
			xboxSecondCommandMsec = (unsigned int)Sys_Milliseconds() - xboxCommandStart;
			xboxSecondCommands = g_SPXBCmdExecCount - xboxCommandSerial;
			#endif
		}
		#ifdef _XBOX
		g_SPXBComTailStage = 0x434D3039; /* 'CM09': command buffer complete */
		#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		xboxPerfComCommandMsec += (unsigned int)(Sys_Milliseconds() - xboxPerfPhaseStart);
		g_SPXBComSubphase = 16;
		if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE after second Cbuf_Execute");
#endif


		//
		// client side
		//
		if ( com_speeds->integer ) {
			timeBeforeClient = Sys_Milliseconds ();
		}

#if defined(_XBOX)
		xboxPerfClientStart = Sys_Milliseconds();
		g_SPXBComTailStage = 0x434D3130; /* 'CM10': before client */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		if (firstFrames) XBLog_Write("JA: Com_Frame: CL_Frame...");
		g_SPXBComSubphase = 17;
		if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE before CL_Frame");
#endif
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		{
			static int s_xboxActiveComClientBoundaryBudget = 32;
			if (!firstFrames && (s_xboxActiveComClientBoundaryBudget > 0 || (frameCount >= 35 && frameCount < 70)))
			{
				XBLF("JA: COM_ACTIVE before CL_Frame frame=%d msec=%d frac=%g realtime=%d",
					frameCount - 1, msec, fractionMsec, com_frameTime);
			}
		}
	#endif
		CL_Frame (msec, fractionMsec);
#if defined(_XBOX)
		g_SPXBComTailStage = 0x434D3131; /* 'CM11': client complete */
		g_SPXBPerfClientMsec = (unsigned int)(Sys_Milliseconds() - xboxPerfClientStart);
		g_SPXBPerfFrameMsec = (unsigned int)(Sys_Milliseconds() - xboxPerfFrameStart);
		g_SPXBPerfGameMsec = com_speeds->integer ? (unsigned int)time_game : 0;
		g_SPXBPerfFrontendMsec = com_speeds->integer ? (unsigned int)time_frontend : 0;
		g_SPXBPerfBackendMsec = com_speeds->integer ? (unsigned int)time_backend : 0;
		g_SPXBPerfComEventMsec = xboxPerfComEventMsec;
		g_SPXBPerfComCommandMsec = xboxPerfComCommandMsec;
		// Keep rare long frames visible in retail logs. Window averages can
		// otherwise hide the exact frame and whether the server or client stalled.
		if (g_SPXBPerfFrameMsec >= 250)
		{
			static unsigned int s_longFrameLogs = 0;
			if (s_longFrameLogs < 10)
			{
				++s_longFrameLogs;
				const unsigned int slot = 1 + (s_longFrameLogs - 1) * 12;
				g_SPXBLongFrames[slot] = (unsigned int)com_frameTime;
				g_SPXBLongFrames[slot + 1] = g_SPXBPerfFrameMsec;
				g_SPXBLongFrames[slot + 2] = g_SPXBPerfServerMsec;
				g_SPXBLongFrames[slot + 3] = g_SPXBPerfClientMsec;
				g_SPXBLongFrames[slot + 4] = xboxFirstCommandMsec;
				g_SPXBLongFrames[slot + 5] = xboxSecondCommandMsec;
				g_SPXBLongFrames[slot + 6] = xboxFirstCommands;
				g_SPXBLongFrames[slot + 7] = xboxSecondCommands;
				volatile char *lastCommand = (volatile char *)&g_SPXBLongFrames[slot + 8];
				for (unsigned int copy = 0; copy < 16; ++copy)
					lastCommand[copy] = g_SPXBCmdLast[copy];
				g_SPXBLongFrames[128] = 3;
				g_SPXBLongFrames[0] = s_longFrameLogs;
				XBLog_WriteCriticalf("STEFX_LONG_FRAME: realtime=%d total=%u server=%u client=%u commands=%u/%u",
					com_frameTime, g_SPXBPerfFrameMsec,
					g_SPXBPerfServerMsec, g_SPXBPerfClientMsec,
					xboxFirstCommandMsec, xboxSecondCommandMsec);
			}
		}
		XBPerf_EndFrame();
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComSubphase = 18;
		{
			static int s_xboxActiveComClientBoundaryBudget = 32;
			if (!firstFrames && (s_xboxActiveComClientBoundaryBudget > 0 || (frameCount >= 35 && frameCount < 70)))
			{
				XBLF("JA: COM_ACTIVE after CL_Frame frame=%d msec=%d realtime=%d",
					frameCount - 1, msec, com_frameTime);
				if (s_xboxActiveComClientBoundaryBudget > 0)
				{
					--s_xboxActiveComClientBoundaryBudget;
				}
			}
		}
		if (s_xboxTraceComPhase) XBLog_Write("JA: COM_PHASE after CL_Frame");
#endif

		if ( com_speeds->integer ) {
			timeAfter = Sys_Milliseconds ();
		}
	}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (firstFrames) XBLog_Write(va("JA: Com_Frame #%d returning", frameCount-1));
	g_SPXBComSubphase = 19;
	xboxTraceActiveComTail = (g_SPXBClsState == (unsigned int)CA_ACTIVE && frameCount >= 8 && frameCount <= 40);
	xboxTraceActiveComTailFrame = frameCount - 1;
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d before timing/report comFrame=%d realtime=%d",
			xboxTraceActiveComTailFrame, com_frameNumber, com_frameTime);
	}
	if (s_xboxTraceComPhase) XBLF("JA: COM_PHASE frame=%d exit", frameCount - 1);
#endif


	//
	// report timing information
	//
#ifdef _XBOX
	g_SPXBComSubphase = 20;
#endif
	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		Com_Printf("fr:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i tr:%3i pvs:%3i rf:%3i bk:%3i\n", 
					com_frameNumber, all, sv, ev, cl, time_game, timeInTrace, timeInPVSCheck, time_frontend, time_backend);

#ifndef _XBOX
		// speedslog
		if ( com_speedslog && com_speedslog->integer )
		{
			if(!speedslog)
			{
				speedslog = FS_FOpenFileWrite("speeds.log");
				FS_Write("data={\n", strlen("data={\n"), speedslog);
				bComma=false;
				if ( com_speedslog->integer > 1 ) 
				{
					// force it to not buffer so we get valid
					// data even if we are crashing
					FS_ForceFlush(logfile);
				}
			}
			if (speedslog)
			{
				char		msg[MAXPRINTMSG];

				if(bComma)
				{
					FS_Write(",\n", strlen(",\n"), speedslog);
					bComma=false;
				}
				FS_Write("{", strlen("{"), speedslog);
				Com_sprintf(msg,sizeof(msg),
							"%8.4f,%8.4f,%8.4f,%8.4f,%8.4f,%8.4f,",corg[0],corg[1],corg[2],cangles[0],cangles[1],cangles[2]);
				FS_Write(msg, strlen(msg), speedslog);
				Com_sprintf(msg,sizeof(msg),
					"%i,%3i,%3i,%3i,%3i,%3i,%3i,%3i,%3i,%3i}", 
					com_frameNumber, all, sv, ev, cl, time_game, timeInTrace, timeInPVSCheck, time_frontend, time_backend);
				FS_Write(msg, strlen(msg), speedslog);
				bComma=true;
			}
		}
#endif

		timeInTrace = timeInPVSCheck = 0;
	}
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 21;
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d after timing/report comFrame=%d realtime=%d",
			xboxTraceActiveComTailFrame, com_frameNumber, com_frameTime);
	}
#endif

	//
	// trace optimization tracking
	//
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 22;
#endif
	if ( com_showtrace->integer ) {
		extern	int c_traces, c_brush_traces, c_patch_traces;
		extern	int	c_pointcontents;

		/*
		Com_Printf( "%4i non-sv_traces, %4i sv_traces, %4i ms, ave %4.2f ms\n", c_traces - numTraces, numTraces, timeInTrace, (float)timeInTrace/(float)numTraces );
		timeInTrace = numTraces = 0;
		c_traces = 0;
		*/
		
		Com_Printf ("%4i traces  (%ib %ip) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 23;
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d before com_frameNumber++ comFrame=%d realtime=%d",
			xboxTraceActiveComTailFrame, com_frameNumber, com_frameTime);
	}
#endif
	com_frameNumber++;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 24;
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d after com_frameNumber++ comFrame=%d realtime=%d",
			xboxTraceActiveComTailFrame, com_frameNumber, com_frameTime);
	}
#endif
}//try
	catch (const char* reason) {
	#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
		g_SPXBComCatchCount++;
		g_SPXBComFrameDepth--;
#endif
		Com_Printf (reason);
		return;			// an ERR_DROP was thrown
	}

#ifdef G2_PERFORMANCE_ANALYSIS
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d before G2 report/reset", xboxTraceActiveComTailFrame);
	}
#endif
	if (com_G2Report && com_G2Report->integer)
	{
		G2Time_ReportTimers();
	}

	G2Time_ResetTimers();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxTraceActiveComTail)
	{
		XBLF("JA: CL_EARLY COM_TAIL frame=%d after G2 report/reset", xboxTraceActiveComTailFrame);
	}
#endif
#endif

#ifdef XBOX_DEMO
	// This is for the code that auto-reboots back to CDX after a timeout:
	extern void Demo_TimerUpdate( void );
	Demo_TimerUpdate();
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	g_SPXBComSubphase = 27;
	g_SPXBComFrameDepth--;
#endif
}

#pragma warning (default: 4701)	//local may have been used without init

/*
=================
Com_Shutdown
=================
*/
extern void CM_FreeShaderText(void);
void Com_Shutdown (void) {
	CM_ClearMap();

#ifndef _XBOX
	CM_FreeShaderText();

	if (logfile) {
		FS_FCloseFile (logfile);
		logfile = 0;
	}

	if (speedslog) {
		FS_Write("\n};", strlen("\n};"), speedslog);
		FS_FCloseFile (speedslog);
		speedslog = 0;
	}

	if (camerafile) {
		FS_FCloseFile (camerafile);
		camerafile = 0;
	}

	if ( com_journalFile ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = 0;
	}
#endif

#ifdef _XBOX
	extern void Sys_StreamShutdown();
	Sys_StreamShutdown();
	Sys_ShutdownFileCodes();
#endif

//	SE_ShutDown();//close the string packages

	extern void Netchan_Shutdown();
	Netchan_Shutdown();
}

/*
============
ParseTextFile
============
*/

bool Com_ParseTextFile(const char *file, class CGenericParser2 &parser, bool cleanFirst)
{
	fileHandle_t	f;
	int				length = 0;
	char			*buf = 0, *bufParse = 0;

	length = FS_FOpenFileByMode( file, &f, FS_READ );
	if (!f || !length)		
	{
		return false;
	}

	buf = new char [length + 1];
	FS_Read( buf, length, f );
	buf[length] = 0;

	bufParse = buf;
	parser.Parse(&bufParse, cleanFirst);
	delete buf;

	FS_FCloseFile( f );

	return true;
}

void Com_ParseTextFileDestroy(class CGenericParser2 &parser)
{
	parser.Clean();
}

CGenericParser2 *Com_ParseTextFile(const char *file, bool cleanFirst, bool writeable)
{
	fileHandle_t	f;
	int				length = 0;
	char			*buf = 0, *bufParse = 0;
	CGenericParser2 *parse;

	length = FS_FOpenFileByMode( file, &f, FS_READ );
	if (!f || !length)		
	{
		return 0;
	}

	buf = new char [length + 1];
	FS_Read( buf, length, f );
	FS_FCloseFile( f );
	buf[length] = 0;

	bufParse = buf;

	parse = new CGenericParser2;
	if (!parse->Parse(&bufParse, cleanFirst, writeable))
	{
		delete parse;
		parse = 0;
	}

	delete buf;

	return parse;
}
