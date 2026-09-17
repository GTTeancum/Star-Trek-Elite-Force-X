/*****************************************************************************
 * name:		snd_dma.c
 *
 * desc:		main control for any streaming sound output device
 *
 *
 *****************************************************************************/
// leave this as first line for PCH reasons...
//
// #include "../server/exe_headers.h"

#include "snd_local_console.h"
#include "snd_music.h"

// #include "../../toolbox/zlib/zlib.h"

#include "../client/client.h"
#include "../qcommon/fixedmap.h"

#ifdef _XBOX
#include <Xtl.h>
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBHMAudioBackendState;
extern "C" volatile unsigned int g_SPXBHMAudioBeginRegistrationCount;
extern "C" volatile unsigned int g_SPXBHMAudioListenerState;
extern "C" volatile unsigned int g_SPXBHMAudioVoiceStartCount;
extern "C" volatile unsigned int g_SPXBAudioMemUsed;
extern "C" volatile unsigned int g_SPXBHMAudioLipActiveCount;
extern "C" volatile unsigned int g_SPXBHMAudioListenerUpdateMask;
extern "C" volatile unsigned int g_SPXBAudioUpdateStage;
extern "C" volatile unsigned int g_SPXBAudioUpdateSerial;
extern "C" volatile unsigned int g_SPXBAudioFaceUpdateCount;
extern "C" volatile unsigned int g_SPXBAudioFaceLipDataUpdateCount;
extern "C" volatile unsigned int g_SPXBAudioFaceFallbackUpdateCount;
extern "C" volatile unsigned int g_SPXBAudioFaceLastEntity;
extern "C" volatile unsigned int g_SPXBAudioFaceLastVolume;
extern "C" volatile unsigned int g_SPXBAudioVoiceRequestCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceQueuedLoadCount;
extern "C" volatile unsigned int g_SPXBAudioVoicePlaySuccessCount;
extern "C" volatile unsigned int g_SPXBAudioVoicePlayFailureCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceLoadRetryCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceLoadRetrySuccessCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceLoadedWakeCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceEarlyStopCount;
extern "C" volatile unsigned int g_SPXBAudioVoiceLastRequestCode;
extern "C" volatile unsigned int g_SPXBAudioVoiceLastPlayCode;
extern "C" volatile unsigned int g_SPXBAudioVoiceLastStopCode;
extern "C" volatile unsigned int g_SPXBAudioVoiceLastStopAge;
#endif

#ifdef _GAMECUBE
typedef const char* LPCSTR;
#endif

// Maps CRCs to offsets
struct LipFileInfo
{
	unsigned long crc;
	unsigned long offset;
};
static VVFixedMap< unsigned int, unsigned int >* s_lipSyncMap = NULL;
static char *s_lipSyncData = NULL;

static void S_Play_f(void);
#ifndef _JK2MP
static void S_PlayEx_f(void);
#endif
static void S_SoundList_f(void);
static void S_Music_f(void);

void S_Update_();
void S_StopAllSounds(void);
static void S_UpdateBackgroundTrack( void );
unsigned int S_HashName( const char *name );
static int SND_FreeSFXMem(sfx_t *sfx);
#ifdef _XBOX
extern qboolean FS_STEFX_FreeHeapFileBuffer(void *buffer);
extern void QAL_SetSourceVoice(ALuint source, bool isVoice);
#endif

#if defined(STEFX_SP_HOSTED_MP)
static vec3_t s_stefxHolomatchEntityOrigins[MAX_GENTITIES];
static qboolean s_stefxHolomatchEntityOriginValid[MAX_GENTITIES];
#endif

/*static void S_FreeAllSFXMem(void);
static void S_UnCacheDynamicMusic( void );
*/
//extern unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned long len);
#include "../zlib/zlib.h"

extern int Sys_GetFileCodeSize(int code);
extern unsigned int Sys_GetSoundFileCode(const char* name);
extern int RE_RegisterMedia_GetLevel(void);

extern void Sys_StreamInit(void);
extern void Sys_StreamShutdown(void);

qboolean SND_RegisterAudio_Clean(void);
void S_KillEntityChannel(int entnum, int chan);

#ifdef _XBOX
extern "C"
{
char* C_MP3_GetUnpackedSize(void *pvData, int iDataLen, int *piUnpackedSize, int bStereoDesired);
char* C_MP3_GetHeaderData(void *pvData, int iDataLen, int *piRate, int *piWidth, int *piChannels, int bStereoDesired);
}
#endif

//////////////////////////
//
// vars for bgrnd music track...
//
typedef struct
{	
	//
	// disk-load stuff
	//
	char		sLoadedDataName[MAX_QPATH];
	int			iFileCode;
	int			iFileSeekTo;
	bool		bLoaded;
	//
	// remaining dynamic fields...
	//
	int			iXFadeVolumeSeekTime;
	int			iXFadeVolumeSeekTo;	// when changing this, set the above timer to Sys_Milliseconds(). 
									//	Note that this should be thought of more as an up/down bool rather than as a 
									//	number now, in other words set it only to 0 or 255. I'll probably change this
									//	to actually be a bool later.
	int			iXFadeVolume;		// 0 = silent, 255 = max mixer vol, though still modulated via overall music_volume 
	float		fSmoothedOutVolume;
	qboolean	bActive;			// whether playing or not
	qboolean	bExists;			// whether was even loaded for this level (ie don't try and start playing it)
	//
	// new dynamic fields...
	//
	qboolean		bTrackSwitchPending;
	qboolean		bLooping;
	MusicState_e	eTS_NewState;
	float			fTS_NewTime;
	//
	// Generic...
	//
	int				s_backgroundSize;
	int				s_backgroundBPS;

	void Rewind()
	{
		iFileSeekTo = 0;
	}

	void SeekTo(float fTime)
	{
		iFileSeekTo = (int)((float)(s_backgroundBPS) * fTime);
	}

	float TotalTime(void)
	{
		return (float)(s_backgroundSize) / (float)(s_backgroundBPS);
	}

	float PlayTime(void)
	{
		ALfloat playTime;
		alGetStreamf(AL_TIME, &playTime);
		return playTime;
	}

	float ElapsedTime(void)
	{
		return fmod(PlayTime(), TotalTime());
	}
} MusicInfo_t;

static void S_SetDynamicMusicState( MusicState_e musicState );

#define fDYNAMIC_XFADE_SECONDS (1.f)

static MusicInfo_t	tMusic_Info[eBGRNDTRACK_NUMBEROF]	= {0};
static qboolean		bMusic_IsDynamic					= qfalse;
static MusicState_e	eMusic_StateActual					= eBGRNDTRACK_EXPLORE;	// actual state, can be any enum
static MusicState_e	eMusic_StateRequest					= eBGRNDTRACK_EXPLORE;	// requested state, can only be explore, action, boss, or silence
static char			sMusic_BackgroundLoop[MAX_QPATH]	= {0};	// only valid for non-dynamic music
static char			sInfoOnly_CurrentDynamicMusicSet[64];	// any old reasonable size, only has to fit stuff like "kejim_post"
//
//////////////////////////

// =======================================================================
// Internal sound data & structures
// =======================================================================
 
// only begin attenuating sound volumes when outside the FULLVOLUME range
#define		SOUND_FULLVOLUME	256

#define		SOUND_ATTENUATE		0.0008f
#define		VOICE_ATTENUATE		0.004f

// This number has dramatic affects on volume.
#define		SOUND_REF_DIST_BASE	1500.f

#define		SOUND_UPDATE_TIME	100

const float	SOUND_FMAXVOL=1.0;
const int	SOUND_MAXVOL=255;

int					s_soundStarted;
qboolean			s_soundMuted;
int					s_loopEnabled;
int					s_updateTime;

#ifdef _XBOX
static qboolean s_xboxSilentAudio = qfalse;
static qboolean s_xboxLipDataLoaded = qfalse;
static int s_xboxSilentVoiceStartsLogged = 0;
static int s_xboxSilentVoiceUpdatesLogged = 0;
static void S_XboxUpdateSilentVoiceVolumes(void);
static int S_XboxFallbackVoiceVolume(const channel_t *ch, int now);
static int S_XboxNormalizeFaceVolume(int volume);
extern HANDLE Sys_FileStreamMutex;
#endif

struct listener_t
{
	ALuint handle;
	ALfloat pos[3];
	ALfloat orient[6];
	int entnum;
};

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
#define SND_MAX_LISTENERS 4
#else
#define SND_MAX_LISTENERS 1
#endif
static listener_t s_listeners[SND_MAX_LISTENERS];
static int s_numListeners;

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static int S_STEFX_HolomatchRequestedListeners(void)
{
	const char *mode;
	int players;

	if (!Cvar_VariableIntegerValue("stefx_splitScreen"))
	{
		return 1;
	}
	mode = Cvar_VariableString("stefx_splitScreenMode");
	if (!mode || Q_stricmp(mode, "holomatch"))
	{
		return 1;
	}
	players = Cvar_VariableIntegerValue("stefx_hmLocalPlayers");
	if (players <= 0)
	{
		players = Cvar_VariableIntegerValue("stefx_splitScreenPlayers");
	}
	if (players < 1)
	{
		players = 1;
	}
	if (players > SND_MAX_LISTENERS)
	{
		players = SND_MAX_LISTENERS;
	}
	return players;
}
#endif

static int			s_numChannels;			// Number of AL Sources == Num of Channels

#ifdef _XBOX
#	define	MAX_CHANNELS_2D 64
#	define	MAX_CHANNELS_3D 64
#else
#	define	MAX_CHANNELS_2D 30
#	define	MAX_CHANNELS_3D 30
#endif

#define MAX_CHANNELS (MAX_CHANNELS_2D + MAX_CHANNELS_3D)
static channel_t*   s_channels;

#define	MAX_SFX 3072	//2048
#define INVALID_CODE 0
static sfx_t* s_sfxBlock;
static int* s_sfxCodes;

static bool s_registered = false;
static int s_defaultSound = 0;

typedef struct 
{ 
	int				volume;
	vec3_t			origin;
	sfx_t			*sfx;
	int				entnum;
	int				entchannel;
	bool			bProcessed;
	bool			bMarked;
} loopSound_t;

#define	MAX_LOOP_SOUNDS 32
static int numLoopSounds;
static loopSound_t* loopSounds;

// The game and cgame retain this pointer in their import tables. Keep its
// address valid even when sound initialization is disabled or fails.
static int s_entityWavVolStorage[MAX_GENTITIES];
int* s_entityWavVol = s_entityWavVolStorage;

cvar_t		*s_effects_volume;
cvar_t		*s_music_volume;
cvar_t		*s_voice_volume;
cvar_t		*s_testsound;
cvar_t		*s_allowDynamicMusic;
cvar_t		*s_show;
cvar_t		*s_separation;
cvar_t		*s_CPUType;
cvar_t		*s_debugdynamic;
cvar_t		*s_soundpoolmegs;
//cvar_t		*s_language;	// note that this is distinct from "g_language"



void S_SoundInfo_f(void) {	
	Com_Printf("----- Sound Info -----\n" );

	if (!s_soundStarted) {
		Com_Printf ("sound system not started\n");
	} else {
		if ( s_soundMuted ) {
			Com_Printf ("sound system is muted\n");
		}
	}
	S_DisplayFreeMemory();
	Com_Printf("----------------------\n" );
}

void TrashSounds_f( void )
{
	SND_FreeOldestSound( NULL );
}

static void S_LoadLipSyncTables(void)
{
	extern DWORD g_dwLanguage;
	const char *langSuffix;
	void *buffer;

	switch( g_dwLanguage )
	{
#ifndef XBOX_DEMO	// Demo has no foreign audio
		case XC_LANGUAGE_FRENCH:
			langSuffix = "_f";
			break;
		case XC_LANGUAGE_GERMAN:
			langSuffix = "_d";
			break;
#endif
		case XC_LANGUAGE_ENGLISH:
		default:
			langSuffix = "_e";
			break;
	}

	int len = FS_ReadFile(va("lipdata%s.idx", langSuffix), &buffer);
	if( len == -1 )
	{
#ifdef _XBOX
		s_xboxLipDataLoaded = qfalse;
		Com_Printf("STEFX: Xbox lip-sync index lipdata%s.idx missing; continuing without lip-sync metadata.\n",
			langSuffix);
		return;
#else
		Com_Error(ERR_DROP, "ERROR: No lip sync index file\n");
#endif
	}
	int numLipFiles = len / sizeof(LipFileInfo);
	LipFileInfo *lbuf = (LipFileInfo *)buffer;

	Z_PushNewDeleteTag( TAG_LIPSYNC );
	s_lipSyncMap = new VVFixedMap< unsigned int, unsigned int >(numLipFiles);
	Z_PopNewDeleteTag();

	for( int i = 0; i < numLipFiles; ++i )
		s_lipSyncMap->Insert(lbuf[i].offset, lbuf[i].crc);
	FS_FreeFile(buffer);

	len = FS_ReadFile(va("lipdata%s.dat", langSuffix), &buffer);
	if( len == -1 )
	{
#ifdef _XBOX
		delete s_lipSyncMap;
		s_lipSyncMap = NULL;
		s_xboxLipDataLoaded = qfalse;
		Com_Printf("STEFX: Xbox lip-sync data lipdata%s.dat missing; continuing without lip-sync metadata.\n",
			langSuffix);
		return;
#else
		Com_Error(ERR_DROP, "ERROR: No lip sync data file\n");
#endif
	}

	Z_PushNewDeleteTag( TAG_LIPSYNC );
	s_lipSyncData = new char[len];
	Z_PopNewDeleteTag();

	memcpy(s_lipSyncData, buffer, len);
	FS_FreeFile(buffer);

#ifdef _XBOX
	s_xboxLipDataLoaded = qtrue;
	Com_Printf("JA: Xbox lip-sync tables loaded suffix=%s files=%d bytes=%d\n",
		langSuffix, numLipFiles, len);
#endif
}

/*
================
S_Init
================
*/
void S_Init( void ) {
	ALCcontext *ALCContext = NULL;
	ALCdevice *ALCDevice = NULL;
	cvar_t	*cv;

	Com_Printf("\n------- sound initialization -------\n");

	s_effects_volume = Cvar_Get ("s_effects_volume", "1.0", CVAR_ARCHIVE);
	s_voice_volume= Cvar_Get ("s_voice_volume", "1.0", CVAR_ARCHIVE);
	s_music_volume = Cvar_Get ("s_music_volume", "0.25", CVAR_ARCHIVE);

	s_separation = Cvar_Get ("s_separation", "0.5", CVAR_ARCHIVE);
	s_allowDynamicMusic = Cvar_Get ("s_allowDynamicMusic", "1", CVAR_ARCHIVE);

	s_show = Cvar_Get ("s_show", "0", CVAR_CHEAT);
	s_testsound = Cvar_Get ("s_testsound", "0", CVAR_CHEAT);
	s_debugdynamic = Cvar_Get("s_debugdynamic","0", CVAR_CHEAT);

	s_CPUType = Cvar_Get("sys_cpuid","",0);
	s_soundpoolmegs = Cvar_Get("s_soundpoolmegs", "6", CVAR_ARCHIVE);

//	s_language = Cvar_Get("s_language","english",CVAR_ARCHIVE | CVAR_NORESTART);

#ifdef _XBOX
	cv = Cvar_Get("s_initsound", "1", CVAR_ROM);
	if ( !cv->integer ) {
		s_xboxSilentAudio = qtrue;
		s_soundMuted = qtrue;
		s_soundStarted = 0;
		g_SPXBHMAudioBackendState = 0;
		Com_Printf("STEFX: Xbox audio disabled by s_initsound=0; no device, banks, lip-sync, ambient, or music state initialized.\n");
		Com_Printf("------------------------------------\n");
		return;
	}
	s_xboxSilentAudio = qfalse;
	g_SPXBHMAudioBackendState = 1;
#else
	cv = Cvar_Get("s_initsound", "1", CVAR_ROM);
	if ( !cv->integer ) {
		s_soundStarted = 0;	// needed in case you set s_initsound to 0 midgame then snd_restart (div0 err otherwise later)
		Com_Printf ("not initializing.\n");
		Com_Printf("------------------------------------\n");
		return;
	}
#endif

	AS_Init();

	Cmd_AddCommand("play", S_Play_f);
#ifndef _JK2MP
	Cmd_AddCommand("playex", S_PlayEx_f);
#endif
	Cmd_AddCommand("music", S_Music_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);
	Cmd_AddCommand("soundstop", S_StopAllSounds);
	Cmd_AddCommand("trashsounds", TrashSounds_f);

	// clear out the lip synching override array
	memset(s_entityWavVol, 0, sizeof(int) * MAX_GENTITIES);

#ifdef _XBOX
	Com_Printf("JA: Xbox audio alcOpenDevice begin silent=%d\n", (int)s_xboxSilentAudio);
#else
	Com_Printf("JA: audio alcOpenDevice begin\n");
#endif
	ALCDevice = alcOpenDevice((ALubyte*)"DirectSound3D");
	if (!ALCDevice)
	{
		Com_Printf("JA: Xbox audio alcOpenDevice failed\n");
#ifdef _XBOX
		g_SPXBHMAudioBackendState = 0xE001;
#endif
		return;
	}
#ifdef _XBOX
	g_SPXBHMAudioBackendState = 2;
#endif
	Com_Printf("JA: Xbox audio alcOpenDevice ok device=%p\n", ALCDevice);

	//Create context(s)
	Com_Printf("JA: Xbox audio alcCreateContext begin\n");
	ALCContext = alcCreateContext(ALCDevice, NULL);
	if (!ALCContext)
	{
		Com_Printf("JA: Xbox audio alcCreateContext failed\n");
#ifdef _XBOX
		g_SPXBHMAudioBackendState = 0xE002;
#endif
		return;
	}
#ifdef _XBOX
	g_SPXBHMAudioBackendState = 3;
#endif
	Com_Printf("JA: Xbox audio alcCreateContext ok context=%p\n", ALCContext);

	//Set active context
	Com_Printf("JA: Xbox audio alcMakeContextCurrent begin\n");
	alcMakeContextCurrent(ALCContext);		
	if (alcGetError(ALCDevice) != ALC_NO_ERROR)
	{
		Com_Printf("JA: Xbox audio alcMakeContextCurrent failed\n");
#ifdef _XBOX
		g_SPXBHMAudioBackendState = 0xE003;
#endif
		return;
	}
	Com_Printf("JA: Xbox audio alcMakeContextCurrent ok\n");

	s_channels = new channel_t[MAX_CHANNELS];
	
	s_sfxBlock = new sfx_t[MAX_SFX];
	s_sfxCodes = new int[MAX_SFX];
	memset(s_sfxCodes, INVALID_CODE, sizeof(int) * MAX_SFX);

	loopSounds = new loopSound_t[MAX_LOOP_SOUNDS];

	S_StopAllSounds();

	s_soundStarted = 1;
	s_soundMuted = 1;
	s_loopEnabled = 0;
	s_updateTime = 0;
#ifdef _XBOX
	g_SPXBHMAudioBackendState = 4;
#endif

#ifdef _XBOX
	if (s_xboxSilentAudio)
	{
		s_numChannels = 0;
		Com_Printf("JA: Xbox silent audio metadata allocated: sfx=%d channels=%d entities=%d\n",
			MAX_SFX, MAX_CHANNELS, MAX_GENTITIES);
		Com_Printf("------------------------------------\n");
		S_InitLoad();
		S_LoadLipSyncTables();
		return;
	}
#endif

	S_SoundInfo_f();

	memset(s_channels, 0, sizeof(channel_t) * MAX_CHANNELS);
	s_numChannels = 0;
	
	// create music channel
	alGenStream();

	Com_Printf("------------------------------------\n");

	S_InitLoad();
	S_LoadLipSyncTables();
}

// only called from snd_restart. QA request...
//
void S_ReloadAllUsedSounds(void)
{
	if (s_soundStarted && !s_soundMuted )
	{
		// new bit, reload all soundsthat are used on the current level->..
		//
		for (int i = 0; i < MAX_SFX; ++i)
		{
			if (s_sfxCodes[i] == INVALID_CODE || s_sfxCodes[i] == s_defaultSound) continue;

			sfx_t *sfx = &s_sfxBlock[i];

			if ((sfx->iFlags & SFX_FLAG_UNLOADED) && 
				!(sfx->iFlags & (SFX_FLAG_DEFAULT | SFX_FLAG_DEMAND)))
			{
				S_StartLoadSound(sfx);
			}
		}
	}
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown( void )
{
	ALCcontext	*ALCContext;
	ALCdevice	*ALCDevice;
	int			i;

	memset(s_entityWavVol, 0, sizeof(int) * MAX_GENTITIES);

	if ( !s_soundStarted ) {
		return;
	}

	alDeleteStream();

	// Release all the AL Sources (including Music channel (Source 0))
	for (i = 0; i < s_numChannels; i++)
	{
		alDeleteSources(1, &(s_channels[i].alSource));
	}

	S_FreeAllSFXMem();
	S_UnCacheDynamicMusic();
	
	// Release listeners
	for (i = 0; i < s_numListeners; ++i)
	{
		alDeleteListeners(1, &s_listeners[i].handle);
	}
	s_numListeners = 0;
	
	delete [] s_channels;
	delete [] s_sfxBlock;
	delete [] s_sfxCodes;
	delete [] loopSounds;

	// Get active context
	ALCContext = alcGetCurrentContext();
	// Get device for active context
	ALCDevice = alcGetContextsDevice(ALCContext);
	// Release context(s)
	alcDestroyContext(ALCContext);
	// Close device
	alcCloseDevice(ALCDevice);
	
	s_numChannels = 0;
	s_soundStarted = 0;

	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("music");
	Cmd_RemoveCommand("stopsound");
	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");
	AS_Free();
	S_CloseLoad();
}



/*
	Mutes / Unmutes all OpenAL sound
*/
void S_AL_MuteAllSounds(qboolean bMute)
{
	if (!s_soundStarted) return;
	if (bMute) alGain(0.f);
	else alGain(1.f);
}

void S_SetVolume(float volume)
{
	if (!s_soundStarted) return;
	alGain(volume);
	alUpdate();
}





// =======================================================================
// Load a sound
// =======================================================================
/*
==================
S_FixMusicFileExtension
==================
*/
#ifdef _XBOX
static qboolean S_XboxMusicCandidateExists(const char *name)
{
	fileHandle_t handle = 0;
	int len = FS_FOpenFileRead(name, &handle, qtrue);
	if (handle)
	{
		FS_FCloseFile(handle);
		return len > 0 ? qtrue : qfalse;
	}

	return qfalse;
}

static qboolean S_XboxMusicNameHasExtension(const char *name)
{
	const char *slash = strrchr(name, '/');
	const char *backslash = strrchr(name, '\\');
	const char *dot = strrchr(name, '.');
	const char *lastSep = slash > backslash ? slash : backslash;
	return (dot && (!lastSep || dot > lastSep)) ? qtrue : qfalse;
}

static qboolean S_XboxMusicNameIsMP3(const char *name)
{
	const char *dot = strrchr(name, '.');
	return (dot && !Q_stricmp(dot, ".mp3")) ? qtrue : qfalse;
}

static qboolean S_XboxMusicNameIsWAV(const char *name)
{
	const char *dot = strrchr(name, '.');
	return (dot && !Q_stricmp(dot, ".wav")) ? qtrue : qfalse;
}

static void S_XboxMusicSetExtension(char *out, int outSize, const char *name, const char *ext)
{
	Q_strncpyz(out, name, outSize);
	if (S_XboxMusicNameHasExtension(out))
	{
		char stripped[MAX_QPATH];
		COM_StripExtension(out, stripped);
		Q_strncpyz(out, stripped, outSize);
	}
	Q_strcat(out, outSize, ".");
	Q_strcat(out, outSize, ext);
}
#endif

char* S_FixMusicFileName(const char* name)
{
	static char xname[MAX_QPATH];

#if defined(_XBOX)
	if (!name || !name[0])
	{
		xname[0] = 0;
		return xname;
	}

	Q_strncpyz(xname, name, sizeof(xname));
	for (int i = 0; xname[i]; ++i)
	{
		if (xname[i] == '\\')
		{
			xname[i] = '/';
		}
	}

	char wavName[MAX_QPATH];
	S_XboxMusicSetExtension(wavName, sizeof(wavName), xname, "wav");
	if ((S_XboxMusicNameIsMP3(xname) || !S_XboxMusicNameHasExtension(xname) || S_XboxMusicNameIsWAV(xname)) &&
		S_XboxMusicCandidateExists(wavName))
	{
		Q_strncpyz(xname, wavName, sizeof(xname));
		static int s_xboxMusicWAVLogCount = 0;
		if (s_xboxMusicWAVLogCount < 64)
		{
			Com_Printf("STEFX: Xbox music resolved WAV '%s'\n", xname);
			s_xboxMusicWAVLogCount++;
		}
		return xname;
	}

	if (S_XboxMusicNameHasExtension(xname) && S_XboxMusicCandidateExists(xname))
	{
		static int s_xboxMusicExactLogCount = 0;
		if (s_xboxMusicExactLogCount < 64)
		{
			Com_Printf("STEFX: Xbox music resolved exact '%s'\n", xname);
			s_xboxMusicExactLogCount++;
		}
		return xname;
	}

	char mp3Name[MAX_QPATH];
	S_XboxMusicSetExtension(mp3Name, sizeof(mp3Name), xname, "mp3");
	if (S_XboxMusicCandidateExists(mp3Name))
	{
		Q_strncpyz(xname, mp3Name, sizeof(xname));
		static int s_xboxMusicMP3LogCount = 0;
		if (s_xboxMusicMP3LogCount < 64)
		{
			Com_Printf("STEFX: Xbox music resolved MP3 '%s'\n", xname);
			s_xboxMusicMP3LogCount++;
		}
		return xname;
	}

	char wxbName[MAX_QPATH];
	S_XboxMusicSetExtension(wxbName, sizeof(wxbName), xname, "wxb");
	Q_strncpyz(xname, wxbName, sizeof(xname));
	return xname;
#elif defined(_WINDOWS)
	const char* ext = "wav";
#elif defined(_GAMECUBE)
	const char* ext = "adp";
#endif

#if !defined(_XBOX)
	Q_strncpyz(xname, name, sizeof(xname));
	if (xname[strlen(xname) - 4] != '.')
	{
		strcat(xname, ".");
		strcat(xname, ext);
	}
	else
	{
		int len = strlen(xname);
		xname[len-3] = ext[0];
		xname[len-2] = ext[1];
		xname[len-1] = ext[2];
	}
#endif

#ifdef _GAMECUBE
	if (!strncmp("music/", xname, 6) ||
		!strncmp("music\\", xname, 6))
	{
		char chan_name[MAX_QPATH];

		/*
		ALint is_stereo;
		alGeti(AL_STEREO, &is_stereo);

		sprintf(chan_name,"music-%s/%s", 
			is_stereo ? "stereo" : "mono", &xname[6]);
		strcpy(xname, chan_name);
		*/

		sprintf(chan_name,"music-stereo/%s", &xname[6]);
		strcpy(xname, chan_name);
	}
#endif
	
	return xname;
}


/*
==================
S_HashName
==================
*/
unsigned int S_HashName( const char *name ) {
	if (!name) {
		Com_Error (ERR_FATAL, "S_HashName: NULL\n");
	}
	if (!name[0]) {
		Com_Error (ERR_FATAL, "S_HashName: empty name\n");
	}

	if (strlen(name) >= MAX_QPATH) {
		Com_Error (ERR_FATAL, "Sound name too long: %s", name);
	}

	char sSoundNameNoExt[MAX_QPATH];
	COM_StripExtension(name,sSoundNameNoExt);

	Q_strlwr(sSoundNameNoExt);
	for (int i = 0; i < strlen(sSoundNameNoExt); ++i)
	{
		if (sSoundNameNoExt[i] == '\\') sSoundNameNoExt[i] = '/';
	}

	return crc32(0, (const byte *)sSoundNameNoExt, strlen(sSoundNameNoExt));
}

/*
===================
S_DisableSounds

Disables sounds until the next S_BeginRegistration.
This is called when the hunk is cleared and the sounds
are no longer valid.
===================
*/
void S_DisableSounds( void ) {
	if (!s_soundStarted) return;
	S_StopAllSounds();
	SND_RegisterAudio_Clean(); // unregister sounds
	s_soundMuted = qtrue;
}

void S_SetLoopState( qboolean s ) {
	if (!s_soundStarted) return;
	s_loopEnabled = s;
}

void S_CreateSources( void ) {
	int i;

	// Remove any old sources
	for (i = 0; i < s_numChannels; ++i)
	{
		alDeleteSources(1, &s_channels[i].alSource);
	}
	s_numChannels = 0;

	// Create as many AL Sources (up to Max) as possible
	int limit = MAX_CHANNELS_2D + MAX_CHANNELS_3D / s_numListeners;
	for (i = 0; i < limit; i++)
	{
		if (i < MAX_CHANNELS_2D)
		{
			alGenSources2D(1, &s_channels[i].alSource);
			s_channels[i].b2D = true;
		}
		else
		{
			alGenSources3D(1, &s_channels[i].alSource);
			s_channels[i].b2D = false;
		}

		if (alGetError() != AL_NO_ERROR)
		{
			// Reached limit of sources
			break;
		}

		if (!s_channels[i].b2D)
		{
			alSourcef(s_channels[i].alSource, AL_REFERENCE_DISTANCE, SOUND_REF_DIST_BASE);
		}

		s_numChannels++;
	}

	assert(s_numChannels > MAX_CHANNELS_2D);
}

/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration( void )
{
	if (!s_soundStarted) return;

#ifdef _XBOX
	g_SPXBHMAudioBeginRegistrationCount++;
	if (s_xboxSilentAudio)
	{
		s_soundMuted = qtrue;
		if (!s_registered)
		{
			s_defaultSound = S_RegisterSound("sound/null.wav");
			s_registered = true;
		}
		Com_Printf("JA: Xbox silent S_BeginRegistration complete lipData=%d\n", s_xboxLipDataLoaded);
		return;
	}
#endif

	int i;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	int num_listeners = S_STEFX_HolomatchRequestedListeners();
#else
	int num_listeners = 1;
#endif

	// Turn sound back on.
	s_soundMuted = qfalse;
	g_SPXBHMAudioListenerState = ((unsigned int)SND_MAX_LISTENERS << 16) | (unsigned int)num_listeners;

	// Create listeners
	assert(num_listeners <= SND_MAX_LISTENERS);
	if (num_listeners < s_numListeners)
	{
		// remove some listeners
		for (i = num_listeners; i < s_numListeners; ++i)
		{
			alDeleteListeners(1, &s_listeners[i].handle);
		}

		s_numListeners = num_listeners;
	
		S_CreateSources();
	}
	else if (num_listeners > s_numListeners)
	{
		// add some listeners
		for (i = s_numListeners; i < num_listeners; ++i)
		{
			memset(&s_listeners[i], 0, sizeof(listener_t));
			s_listeners[i].entnum = i;
			s_listeners[i].orient[2] = -1;
			s_listeners[i].orient[4] = 1;
			alGenListeners(1, &s_listeners[i].handle);
			alListenerfv(s_listeners[i].handle, AL_POSITION, s_listeners[i].pos);
			alListenerfv(s_listeners[i].handle, AL_ORIENTATION, s_listeners[i].orient);
		}
		
		s_numListeners = num_listeners;
		
		S_CreateSources();
	}

	S_SetLoopState(qtrue);

	if (!s_registered) {
		s_defaultSound = S_RegisterSound("sound/null.wav");
		S_LoadSound(s_defaultSound);
		s_registered = true;
	}

#ifdef _XBOX
	{
		static int s_xboxRealBeginRegistrationLogCount = 0;
		if (!s_xboxSilentAudio && s_xboxRealBeginRegistrationLogCount < 8)
		{
			Com_Printf("JA: Xbox real S_BeginRegistration listeners=%d channels=%d default=%d registered=%d\n",
				s_numListeners, s_numChannels, s_defaultSound, s_registered);
			s_xboxRealBeginRegistrationLogCount++;
		}
	}
#endif
}

/*
==================
S_LookupSfx
==================
*/
sfxHandle_t S_LookupSfx(int hash) 
{
	for (int i = 0; i < MAX_SFX; ++i)
	{
		if (s_sfxCodes[i] == hash)
		{
			return i;
		}
	}
	return -1;
}

/*
==================
S_AllocSfx
==================
*/
sfxHandle_t S_AllocSfx(int hash) 
{
	for (int i = 0; i < MAX_SFX; ++i)
	{
		if (s_sfxCodes[i] == INVALID_CODE)
		{
			s_sfxCodes[i] = hash;
			return i;
		}
	}
	return -1;
}

extern void	COM_StripExtension( const char *in, char *out );
extern char *FS_BuildOSPathUnMapped( const char *qpath );

// Convert pathname to filecode
int Lip_GetFileCode(const char* name)
{
	// Get system level path
	char* osname = FS_BuildOSPathUnMapped(name);

	// Generate hash for file name
	strlwr(osname);
	unsigned int code = crc32(0, (const unsigned char *)osname, strlen(osname));

	return code;
}

static void	S_LoadLips(sfx_t* thesfx, const char* name)
{
	// Sanity checks
	if( !s_lipSyncData || !s_lipSyncMap )
		return;

	// get the lipfile name -> turn it into a crc
	char lipfile[MAX_QPATH];
	COM_StripExtension(name,lipfile);
	strcat(lipfile,".lip");
	unsigned int code = Lip_GetFileCode( lipfile );

	// Lookup in the fixed map
	unsigned int *pOffset = s_lipSyncMap->Find(code);
	if( !pOffset )
		return;

	// OK. Set the pointer to the right data
	thesfx->pLipSyncData = s_lipSyncData + *pOffset;
}



/*
==================
S_RegisterSound

Creates a default buzz sound if the file can't be loaded
==================
*/
sfxHandle_t	S_RegisterSound(const char *name)
{
	sfx_t *sfx;
	unsigned int hash;
	sfxHandle_t handle;

	if (!s_soundStarted) {
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH || !name[0] ) {		
		Com_Printf( S_COLOR_RED"Sound name exceeds MAX_QPATH - %s\n", name );
		return s_defaultSound;
	}

	/* Temporary fix for levels that try to precache music and play them as sfx */
	if (strstr(name, "MUSIC"))
	{
		Com_Printf( "WARNING: Trying to play music file %s through S_StartSound!\n", name );
		return s_defaultSound;
	}

	hash = S_HashName( name );
	handle = S_LookupSfx(hash);
	
	if (handle < 0)
	{
		handle = S_AllocSfx(hash);

		if (handle < 0)
			Com_Error (ERR_DROP, "No free sound channels");

		sfx = &s_sfxBlock[handle];
		memset(sfx, 0, sizeof(sfx_t));

		if (strlen(name) < 5 || name[0] == '*') sfx->iFileCode = -1;
		else sfx->iFileCode = S_GetFileCode(name);

		sfx->iFlags |= SFX_FLAG_UNLOADED;
	}
	else
	{
		sfx = &s_sfxBlock[handle];
	}

	SND_TouchSFX(sfx);

	if ( sfx->iFileCode == -1 ) sfx->iFlags |= SFX_FLAG_DEFAULT;

	sfx->pLipSyncData = NULL;

	char fixedName[MAX_QPATH];
	Q_strncpyz( fixedName, name, sizeof(fixedName) );
	Q_strlwr( fixedName );

	char *psVoice = strstr(fixedName, "chars");
	if( !psVoice )
	{
		psVoice = strstr(fixedName, "sound/voice/");
	}
	if( psVoice )
	{
		// Need to replace "chars" with "chr_f" or "chr_d" if we're in a foreign
		// language, or the crc won't match the one generated by lipthing2:
#ifndef XBOX_DEMO
		extern DWORD g_dwLanguage;
		if( g_dwLanguage == XC_LANGUAGE_FRENCH )
			strncpy( psVoice, "chr_f", 5 );
		else if( g_dwLanguage == XC_LANGUAGE_GERMAN )
			strncpy( psVoice, "chr_d", 5 );
#endif

		sfx->iFlags |= SFX_FLAG_VOICE;
		sfx->iFlags |= SFX_FLAG_DEMAND;

		// load up the lip sync data
		S_LoadLips(sfx, fixedName);
#ifdef _XBOX
		{
			static int s_xboxVoiceRegistersLogged = 0;
			if (s_xboxVoiceRegistersLogged < 96)
			{
				Com_Printf("STEFX: S_RegisterSound voice handle=%d flags=0x%x fileCode=0x%x lip=%d name='%s'\n",
					handle, sfx->iFlags, sfx->iFileCode, sfx->pLipSyncData ? 1 : 0, fixedName);
				s_xboxVoiceRegistersLogged++;
			}
		}
#endif
	}

	if ( sfx->iFlags & SFX_FLAG_DEFAULT )
	{
		sfx->iFlags |= SFX_FLAG_RESIDENT;
		return s_defaultSound;
	}

	//can be uncommented for debugging if soundname is used
	//also uncomment sSoundName from sfx_t
	//sfx->sSoundName = CopyString(name);

	return handle;
}


//=============================================================================
channel_t *S_FindFurthestChannel(void)
{
	int			ch_idx;
	channel_t	*ch;
	channel_t	*ch_firstToDie = NULL;
	int			li_idx;
	listener_t	*li;
	int			longestDist = -1;
	int			dist;

	for (li_idx = 0, li = s_listeners; li_idx < s_numListeners; ++li_idx, ++li)
	{
		for (ch_idx = MAX_CHANNELS_2D, ch = s_channels + ch_idx; 
		ch_idx < s_numChannels; ch_idx++, ch++)
		{				
			dist = 
				((li->pos[0] - ch->origin[0]) * (li->pos[0] - ch->origin[0])) +
				((li->pos[1] - ch->origin[1]) * (li->pos[1] - ch->origin[1])) +
				((li->pos[2] - ch->origin[2]) * (li->pos[2] - ch->origin[2]));
			
			if (dist > longestDist)
			{
				longestDist = dist;
				ch_firstToDie = ch;
			}
		}
	}

	return ch_firstToDie;
}

static bool IsListenerEnt(int entnum)
{
	for (int i = 0; i < s_numListeners; ++i)
	{
		if (s_listeners[i].entnum == entnum) return true;
	}
	return false;
}

/*
=================
S_PickChannel
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel, bool is2D, sfx_t* sfx)
{
	int			ch_idx;
	channel_t	*ch, *ch_firstToDie;
	bool	foundChan = false;

	if ( entchannel < 0 ) 
	{
		Com_Error (ERR_DROP, "S_PickChannel: entchannel<0");
	}

	// Check for replacement sound, or find the best one to replace

    ch_firstToDie = s_channels;
	unsigned int age = 0xFFFFFFFF;

	/*
	** Reuse an exact entity/channel/sound match before consuming another
	** hardware voice.  The prior free-first ordering let short repeated UI
	** sounds fan out across every source; normal successive dialogue uses a
	** different sfx and is therefore unaffected.
	*/
	for (ch_idx = 0, ch = s_channels + ch_idx; ch_idx < s_numChannels; ch_idx++, ch++)
	{
		if (sfx && ch->thesfx == sfx &&
			ch->entnum == entnum && ch->entchannel == entchannel &&
			ch->entchannel != CHAN_AMBIENT && ch->b2D == is2D &&
			!IsListenerEnt(ch->entnum))
		{
			ch_firstToDie = ch;
			foundChan = true;
			break;
		}
	}

	for (ch_idx = 0, ch = s_channels + ch_idx; !foundChan && ch_idx < s_numChannels; ch_idx++, ch++)
	{
		// Special check to prevent 2d voices from being played
		// twice in 2 player games...
		if (is2D && ch->b2D && 
			sfx == ch->thesfx && 
			ch->bPlaying && 
			(sfx->iFlags & SFX_FLAG_VOICE))
		{
			return NULL;
		}
		
		// See if the channel is free
		if (!ch->thesfx && is2D == ch->b2D && ch->iLastPlayTime < age)
		{
			ch_firstToDie = ch;
			age = ch->iLastPlayTime;
			foundChan = true;
		}
	}

	if (!foundChan)
	{
		for (ch_idx = 0, ch = s_channels + ch_idx; ch_idx < s_numChannels; ch_idx++, ch++)
		{
			if ( (ch->entnum == entnum) && 
				(ch->entchannel == entchannel) && 
				(ch->entchannel != CHAN_AMBIENT) && 
				(!IsListenerEnt(ch->entnum)) &&
				(ch->b2D == is2D) &&
				(!ch_firstToDie->thesfx || 
				!(ch_firstToDie->thesfx->iFlags & SFX_FLAG_LOADING)) ) 
			{
				// Same entity and same type of sound effect (entchannel)
				ch_firstToDie = ch;
				foundChan = true;
				break;
			}
		}
	}

	if (!foundChan)
	{
		if (is2D)
		{
			// Find random sound effect
			ch_firstToDie = s_channels + (rand() % MAX_CHANNELS_2D);
		}
		else
		{
			// Find sound effect furthest from listeners
			ch_firstToDie = S_FindFurthestChannel();
		}
	}

	assert(ch_firstToDie->b2D == is2D);

	if (ch_firstToDie->thesfx && ch_firstToDie->thesfx->iFlags & SFX_FLAG_LOADING)
	{
		// If the sound is loading, just give up...
#ifdef _XBOX
		if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
			entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
			(sfx && (sfx->iFlags & SFX_FLAG_VOICE)) ||
			(ch_firstToDie->thesfx && (ch_firstToDie->thesfx->iFlags & SFX_FLAG_VOICE)))
		{
			static int s_xboxVoiceLoadingDropLogs = 0;
			if (s_xboxVoiceLoadingDropLogs < 64)
			{
				Com_Printf("STEFX_VOICE_TRACE: pick drop loading voice ent=%d chan=%d oldEnt=%d oldChan=%d oldCode=0x%x newCode=0x%x\n",
					entnum, entchannel, ch_firstToDie->entnum, ch_firstToDie->entchannel,
					ch_firstToDie->thesfx ? ch_firstToDie->thesfx->iFileCode : 0,
					sfx ? sfx->iFileCode : 0);
				s_xboxVoiceLoadingDropLogs++;
			}
		}
#endif
		return NULL;
	}
	
	if (ch_firstToDie->bPlaying)
	{
#ifdef _XBOX
		if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
			entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
			(sfx && (sfx->iFlags & SFX_FLAG_VOICE)))
		{
			static int s_xboxVoiceChannelReclaims = 0;
			if (s_xboxVoiceChannelReclaims < 64)
			{
				Com_Printf("STEFX: S_PickChannel reclaim voice ent=%d chan=%d oldEnt=%d oldChan=%d oldCode=0x%x newCode=0x%x\n",
					entnum, entchannel, ch_firstToDie->entnum, ch_firstToDie->entchannel,
					ch_firstToDie->thesfx ? ch_firstToDie->thesfx->iFileCode : 0,
					sfx ? sfx->iFileCode : 0);
				s_xboxVoiceChannelReclaims++;
			}
			alSourceStop(ch_firstToDie->alSource);
			ch_firstToDie->bPlaying = false;
		}
		else
		{
			// We have an insane amount of channels on the Xbox
			// and stopping one is a blocking operation.  Let's
			// just assume that no one will care if a sound is
			// dropped when over 100 are already playing...
			return NULL;
		}
#else
		// Stop sound
		alSourceStop(ch_firstToDie->alSource);
		ch_firstToDie->bPlaying = false;
#endif
	}

	// Reset channel variables
#ifdef _XBOX
	if (ch_firstToDie->thesfx &&
		(ch_firstToDie->entchannel == CHAN_VOICE || ch_firstToDie->entchannel == CHAN_VOICE_ATTEN ||
		 ch_firstToDie->entchannel == CHAN_VOICE_GLOBAL || ch_firstToDie->entchannel == CHAN_ANNOUNCER ||
		 (ch_firstToDie->thesfx->iFlags & SFX_FLAG_VOICE)))
	{
		static int s_xboxVoiceResetLogs = 0;
		if (s_xboxVoiceResetLogs < 64)
		{
			Com_Printf("STEFX_VOICE_TRACE: pick reset voice oldEnt=%d oldChan=%d oldCode=0x%x oldPlaying=%d oldLoading=%d newEnt=%d newChan=%d newCode=0x%x\n",
				ch_firstToDie->entnum, ch_firstToDie->entchannel, ch_firstToDie->thesfx->iFileCode,
				ch_firstToDie->bPlaying ? 1 : 0,
				(ch_firstToDie->thesfx->iFlags & SFX_FLAG_LOADING) ? 1 : 0,
				entnum, entchannel, sfx ? sfx->iFileCode : 0);
			s_xboxVoiceResetLogs++;
		}
	}
#endif
	alSourcei(ch_firstToDie->alSource, AL_BUFFER, 0);
	ch_firstToDie->thesfx = NULL;
	ch_firstToDie->bLooping = false;

	/*
	This code can be used to increase the volume of 2D voices, but it
	makes things sound a little weird because Raven is playing 3D sounds
	where there should be 2D sounds. If we can get all sounds that should
	be 3D, to be 3D this will help
	extern void SetHeadroom( int source, float value);
	if(is2D)
	{
		if(	entchannel == CHAN_VOICE		|| // i don't think 
			entchannel == CHAN_ANNOUNCER	||
			entchannel == CHAN_VOICE_ATTEN	||
			entchannel == CHAN_VOICE_GLOBAL )
		{
			SetHeadroom(ch_firstToDie->alSource, 0.0f); // no more attenuation
		}
		else
		{
			SetHeadroom(ch_firstToDie->alSource, 6.0f); // dsound default for 2d sounds
		}
	}
	*/

    return ch_firstToDie;
}



// =======================================================================
// Start a sound effect
// =======================================================================

#ifdef _XBOX
extern const char *Sys_GetSoundFileCodeName(unsigned int code);

static qboolean S_XboxIsVoiceChannel(soundChannel_t channel)
{
	return channel == CHAN_VOICE || channel == CHAN_VOICE_ATTEN ||
		channel == CHAN_VOICE_GLOBAL || channel == CHAN_ANNOUNCER;
}

// Scripted dialogue is serial per speaker.  The game task can advance on the
// frame where the outgoing source reports silence, before the sound update has
// detached its buffer.  Retire that stale channel before allocating the next
// XBADPCM dialogue line so both voice buffers do not have to coexist.
static void S_XboxRetirePriorVoiceForReplacement(int entityNum,
	soundChannel_t incomingChannel, sfx_t *incoming)
{
	if (!incoming ||
		!(S_XboxIsVoiceChannel(incomingChannel) || (incoming->iFlags & SFX_FLAG_VOICE)))
	{
		return;
	}

	for (int i = 0; i < s_numChannels; ++i)
	{
		channel_t *ch = &s_channels[i];
		sfx_t *outgoing = ch->thesfx;
		if (!outgoing || outgoing == incoming || ch->entnum != entityNum ||
			!(S_XboxIsVoiceChannel((soundChannel_t)ch->entchannel) ||
			  (outgoing->iFlags & SFX_FLAG_VOICE)))
		{
			continue;
		}

		if (ch->bPlaying)
		{
			alSourceStop(ch->alSource);
		}
		alSourcei(ch->alSource, AL_BUFFER, 0);
		ch->thesfx = NULL;
		ch->bPlaying = false;
		ch->bLooping = false;
		if (ch->entnum >= 0 && ch->entnum < MAX_GENTITIES)
		{
			s_entityWavVol[ch->entnum] = 0;
		}

		qboolean stillInUse = qfalse;
		for (int j = 0; j < s_numChannels; ++j)
		{
			if (s_channels[j].thesfx == outgoing)
			{
				stillInUse = qtrue;
				break;
			}
		}
		int freed = 0;
		if (!stillInUse && (outgoing->iFlags & SFX_FLAG_RESIDENT) &&
			!(outgoing->iFlags & SFX_FLAG_DEFAULT))
		{
			freed = SND_FreeSFXMem(outgoing);
		}

		XBLog_WriteCriticalf("STEFX_VOICE_CACHE: retired ent=%d oldCode=0x%x newCode=0x%x freed=%d",
			entityNum, outgoing->iFileCode, incoming->iFileCode, freed);
	}
}

static qboolean S_XboxCoalesceTutorialTextTick(int entityNum, soundChannel_t entchannel, sfx_t *sfx)
{
	static unsigned int s_xboxTutorialTickLastCode = 0;
	static int s_xboxTutorialTickLastEntity = -1;
	static int s_xboxTutorialTickLastChannel = -1;
	static int s_xboxTutorialTickLastRequestTime = -1000;
	const char *name;
	const int now = Sys_Milliseconds();
	int i;

	if (!sfx)
	{
		return qfalse;
	}

	name = Sys_GetSoundFileCodeName(sfx->iFileCode);
	if (!name ||
		(!strstr(name, "sound/interface/tedtext.wav") &&
		 !strstr(name, "sound\\interface\\tedtext.wav")))
	{
		return qfalse;
	}

	// The tutorial may issue the same UI tick several times in one frame.  The
	// first request has not necessarily claimed a channel yet, so a channel-only
	// test lets the initial burst allocate multiple sources before coalescing can
	// see it.  Remember the accepted request itself and reject only an identical
	// near-simultaneous duplicate.
	if (s_xboxTutorialTickLastCode == sfx->iFileCode &&
		s_xboxTutorialTickLastEntity == entityNum &&
		s_xboxTutorialTickLastChannel == (int)entchannel &&
		now - s_xboxTutorialTickLastRequestTime >= 0 &&
		now - s_xboxTutorialTickLastRequestTime < 75)
	{
		static int s_xboxTutorialRequestCoalesceLogBudget = 32;
		if (s_xboxTutorialRequestCoalesceLogBudget > 0)
		{
			XBLog_WriteRingMarkerf("STEFX_AUDIO_REPEAT: coalesced request ent=%d chan=%d age=%d name='%s'",
				entityNum, entchannel, now - s_xboxTutorialTickLastRequestTime, name);
			--s_xboxTutorialRequestCoalesceLogBudget;
		}
		return qtrue;
	}

	s_xboxTutorialTickLastCode = sfx->iFileCode;
	s_xboxTutorialTickLastEntity = entityNum;
	s_xboxTutorialTickLastChannel = (int)entchannel;
	s_xboxTutorialTickLastRequestTime = now;

	for (i = 0; i < s_numChannels; ++i)
	{
		channel_t *candidate = &s_channels[i];
		if (candidate->thesfx != sfx || candidate->entnum != entityNum ||
			candidate->entchannel != entchannel)
		{
			continue;
		}

		if ((sfx->iFlags & SFX_FLAG_LOADING) ||
			(candidate->bPlaying && now - (int)candidate->iLastPlayTime < 75))
		{
			static int s_xboxTutorialTickCoalesceLogBudget = 32;
			if (s_xboxTutorialTickCoalesceLogBudget > 0)
			{
				XBLog_WriteRingMarkerf("STEFX_AUDIO_REPEAT: coalesced ent=%d chan=%d source=%u loading=%d age=%d name='%s'",
					entityNum, entchannel, candidate->alSource,
					(sfx->iFlags & SFX_FLAG_LOADING) ? 1 : 0,
					candidate->bPlaying ? now - (int)candidate->iLastPlayTime : -1,
					name);
				--s_xboxTutorialTickCoalesceLogBudget;
			}
			return qtrue;
		}
	}

	return qfalse;
}
#endif

static void SetChannelOrigin(channel_t *ch, const vec3_t origin, int entityNum)
{
	if (origin) 
	{
		ch->origin[0] = origin[0];
		ch->origin[1] = origin[1];
		ch->origin[2] = origin[2];
	}
	else
	{
		vec3_t pos;

#if defined(STEFX_SP_HOSTED_MP)
		if (entityNum >= 0 && entityNum < MAX_GENTITIES &&
			s_stefxHolomatchEntityOriginValid[entityNum])
		{
			VectorCopy(s_stefxHolomatchEntityOrigins[entityNum], pos);
		}
		else
		{
			VectorClear(pos);
		}
#else
		extern void G_EntityPosition( int i, vec3_t ret );
		G_EntityPosition(entityNum, pos);
#endif
		
		ch->origin[0] = pos[0];
		ch->origin[1] = pos[1];
		ch->origin[2] = pos[2];
	}

	ch->bOriginDirty = true;
}

/*
====================
S_StartAmbientSound

Starts an ambient, 'one-shot" sound.
====================
*/

void S_StartAmbientSound( const vec3_t origin, int entityNum, unsigned char volume, sfxHandle_t sfxHandle )
{
	channel_t	*ch;
	/*const*/ sfx_t *sfx;

	if( volume == 0)
		return;

	if ( !s_soundStarted ) {
		return;
	}

	if ( s_soundMuted ) {
#ifdef _XBOX
		return;
#else
		return;
#endif
	}
	if ( sfxHandle < 0 || sfxHandle > MAX_SFX || s_sfxCodes[sfxHandle] == INVALID_CODE ) {
		return;
	}
	if ( !origin && ( entityNum < 0 || entityNum > MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartAmbientSound: bad entitynum %i", entityNum );
	}

	sfx = &s_sfxBlock[sfxHandle];
	if (sfx->iFlags & SFX_FLAG_UNLOADED){
		S_StartLoadSound(sfx);
	}	
	SND_TouchSFX(sfx);

	// pick a channel to play on
	bool is2D = false;
	for (int i = 0; i < s_numListeners; ++i)
	{
		if ((entityNum == s_listeners[i].entnum && !origin) ||
			(origin &&
			origin[0] == s_listeners[i].pos[0] &&
			origin[1] == s_listeners[i].pos[1] &&
			origin[2] == s_listeners[i].pos[2]))
		{
			is2D = true;
			break;
		}
	}

	ch = S_PickChannel( entityNum, CHAN_AMBIENT, is2D, NULL );
	if (!ch) {
		return;
	}
	
	if (!is2D)
	{
		SetChannelOrigin(ch, origin, entityNum);
	}

	ch->master_vol = volume;
	ch->fLastVolume = -1;
	ch->entnum = entityNum;
	ch->entchannel = CHAN_AMBIENT;
	ch->thesfx = sfx;
}

/*
====================
S_MuteSound

Mutes sound on specified channel for specified entity.
This seems to be implemented quite incorrectly on PC. I
think the following is what this function should do...

Perhaps we should actually be changing the volume on all
the channels that meet our criteria, but for now we'll just
kill the sounds and hope it does what is expected.
====================
*/
void S_MuteSound(int entityNum, int entchannel) 
{
	S_KillEntityChannel( entityNum, entchannel );

/*
	if (!s_soundStarted) {
		return;
	}

	//I guess this works.
	channel_t *ch = S_PickChannel( entityNum, entchannel );

	if (!ch)
	{
		return;
	}

	ch->master_vol = 0;
	ch->entnum = 0;
	ch->entchannel = 0;
	ch->thesfx = 0;
	ch->startSample = 0;

	ch->leftvol = 0;
	ch->rightvol = 0;
*/
}

/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
#include "../game/g_local.h"
extern int Sys_GetSoundFileCodeSize(unsigned int code);
extern unsigned int Sys_GetSoundFileCodeFlags(unsigned int code);
extern const char *Sys_GetSoundFileCodeName(unsigned int code);

#ifdef _XBOX
static const char *S_XboxLevelMapName(void)
{
#if defined(STEFX_SP_HOSTED_MP)
	const char *mapname = Cvar_VariableString("mapname");
	return mapname ? mapname : "";
#else
	return level.mapname;
#endif
}

static qboolean S_XboxDN3ProofMapActive(void)
{
	const char *mapname = NULL;

	if (S_XboxLevelMapName()[0] && !Q_stricmp(S_XboxLevelMapName(), "dn3"))
	{
		return qtrue;
	}

	mapname = Cvar_VariableString("mapname");
	if (mapname && mapname[0] && !Q_stricmp(mapname, "dn3"))
	{
		return qtrue;
	}

	mapname = Cvar_VariableString("cl_mapname");
	if (mapname && mapname[0] && !Q_stricmp(mapname, "dn3"))
	{
		return qtrue;
	}

	return qfalse;
}

static qboolean S_XboxIsFoleySound(soundChannel_t entchannel, const sfx_t *sfx)
{
	const char *name;

	if (!sfx)
	{
		return qfalse;
	}

	name = (sfx->iFileCode == -1) ? NULL : Sys_GetSoundFileCodeName(sfx->iFileCode);
	return entchannel == CHAN_BODY || entchannel == CHAN_ITEM ||
		(name && (strstr(name, "foot") || strstr(name, "step") ||
			strstr(name, "borgservo") || strstr(name, "borgfall"))) ? qtrue : qfalse;
}

static void S_XboxDN3ProofLogSoundStart(int entityNum, soundChannel_t entchannel, sfxHandle_t sfxHandle, const sfx_t *sfx)
{
	if (!S_XboxDN3ProofMapActive() || !sfx)
	{
		return;
	}

	const char *name = (sfx->iFileCode == -1) ? NULL : Sys_GetSoundFileCodeName(sfx->iFileCode);
	if (!name)
	{
		name = "<unknown>";
	}

	const char *category = "sfx";
	int *budget = NULL;
	static int voiceBudget = 48;
	static int weaponBudget = 48;
	static int foleyBudget = 48;
	static int ambientBudget = 32;
	static int otherBudget = 32;

	if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
		entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
		(sfx->iFlags & SFX_FLAG_VOICE))
	{
		category = "vo";
		budget = &voiceBudget;
	}
	else if (entchannel == CHAN_WEAPON || strstr(name, "weapons/") || strstr(name, "weapon/"))
	{
		category = "weapon";
		budget = &weaponBudget;
	}
	else if (entchannel == CHAN_AMBIENT || strstr(name, "ambient") || strstr(name, "world/"))
	{
		category = "ambient";
		budget = &ambientBudget;
	}
	else if (entchannel == CHAN_BODY || entchannel == CHAN_ITEM ||
		entchannel == CHAN_LOCAL_SOUND || strstr(name, "foot") || strstr(name, "step") ||
		strstr(name, "pain") || strstr(name, "fall"))
	{
		category = "foley";
		budget = &foleyBudget;
	}
	else
	{
		budget = &otherBudget;
	}

	if (*budget <= 0)
	{
		return;
	}

	Com_Printf("STEFX_DN3_PROOF: audio start category=%s name='%s' ent=%d chan=%d handle=%d fileCode=0x%x flags=0x%x resident=%d levelMap='%s' map='%s' cl_map='%s'\n",
		category,
		name,
		entityNum,
		entchannel,
		sfxHandle,
		sfx->iFileCode,
		sfx->iFlags,
		(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
		S_XboxLevelMapName(),
		Cvar_VariableString("mapname"),
		Cvar_VariableString("cl_mapname"));
	--(*budget);
}
#endif

void S_StartSound(const vec3_t origin, int entityNum, soundChannel_t entchannel, sfxHandle_t sfxHandle ) 
{
	channel_t	*ch;
	/*const*/ sfx_t *sfx;
#ifdef _XBOX
	qboolean stefxTraceVoiceStart = qfalse;
	qboolean stefxIsVoiceRequest = qfalse;
	static int stefxTraceVoiceStartBudget = 8;
	if (stefxTraceVoiceStartBudget > 0 &&
		(entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
		 entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER))
	{
		stefxTraceVoiceStart = qtrue;
		--stefxTraceVoiceStartBudget;
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start entry ent=%d chan=%d handle=%d origin=%p started=%d muted=%d",
			entityNum, entchannel, sfxHandle, origin, s_soundStarted ? 1 : 0, s_soundMuted ? 1 : 0);
	}
#endif

	if ( !s_soundStarted ) {
#ifdef _XBOX
		if (stefxTraceVoiceStart)
			XBLog_WriteCriticalf("STEFX_SP_AUDIO: start return sound-not-started");
#endif
		return;
	}

	if ( s_soundMuted ) {
#ifdef _XBOX
		if (!s_xboxSilentAudio) {
			if (stefxTraceVoiceStart)
				XBLog_WriteCriticalf("STEFX_SP_AUDIO: start return muted");
			return;
		}
#else
		return;
#endif
	}

	if ( sfxHandle < 0 || sfxHandle > MAX_SFX || s_sfxCodes[sfxHandle] == INVALID_CODE ) {
#ifdef _XBOX
		if (stefxTraceVoiceStart)
			XBLog_WriteCriticalf("STEFX_SP_AUDIO: start return invalid handle=%d code=0x%x",
				sfxHandle, (sfxHandle >= 0 && sfxHandle <= MAX_SFX) ? s_sfxCodes[sfxHandle] : INVALID_CODE);
#endif
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum > MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	sfx = &s_sfxBlock[sfxHandle];
#ifdef _XBOX
	if (S_XboxCoalesceTutorialTextTick(entityNum, entchannel, sfx))
	{
		return;
	}
	stefxIsVoiceRequest = (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
		entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
		(sfx->iFlags & SFX_FLAG_VOICE)) ? qtrue : qfalse;
	if (stefxIsVoiceRequest)
	{
		++g_SPXBAudioVoiceRequestCount;
		g_SPXBAudioVoiceLastRequestCode = (unsigned int)sfx->iFileCode;
	}
	if (S_XboxIsFoleySound(entchannel, sfx))
	{
		static int s_xboxFoleyRequestLogBudget = 64;
		if (s_xboxFoleyRequestLogBudget > 0)
		{
			const char *foleyName = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_FOLEY: request ent=%d chan=%d handle=%d code=0x%x flags=0x%x resident=%d name='%s'",
				entityNum, entchannel, sfxHandle, sfx->iFileCode, sfx->iFlags,
				(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
				foleyName ? foleyName : "<unknown>");
			--s_xboxFoleyRequestLogBudget;
		}
	}
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start sfx resolved code=0x%x flags=0x%x buffer=%u",
			sfx->iFileCode, sfx->iFlags, sfx->Buffer);
	S_XboxDN3ProofLogSoundStart(entityNum, entchannel, sfxHandle, sfx);
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start dn3 proof returned");
#endif

#ifdef _XBOX
	if (!s_xboxSilentAudio)
	{
		static int s_xboxRealSoundStartsLogged = 0;
		if (s_xboxRealSoundStartsLogged < 48 &&
			(entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
			 entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
			 (sfx->iFlags & SFX_FLAG_VOICE)))
		{
			Com_Printf("JA: Xbox real sound start request ent=%d chan=%d handle=%d flags=0x%x fileCode=0x%x state=0x%x\n",
				entityNum, entchannel, sfxHandle, sfx->iFlags, sfx->iFileCode, sfx->Buffer);
			s_xboxRealSoundStartsLogged++;
		}
	}
#endif

#ifdef _XBOX
	if (s_xboxSilentAudio)
	{
		if (entchannel == CHAN_AUTO && (sfx->iFlags & SFX_FLAG_VOICE))
			entchannel = CHAN_VOICE;

		if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN || entchannel == CHAN_VOICE_GLOBAL)
		{
			channel_t *silent = NULL;
			channel_t *freeSilent = NULL;
			for (int i = 0; i < MAX_CHANNELS; ++i)
			{
				channel_t *candidate = &s_channels[i];
				if (candidate->thesfx && candidate->entnum == entityNum &&
					(candidate->entchannel == CHAN_VOICE ||
					 candidate->entchannel == CHAN_VOICE_ATTEN ||
					 candidate->entchannel == CHAN_VOICE_GLOBAL))
				{
					silent = candidate;
					break;
				}
				if (!freeSilent && !candidate->thesfx)
					freeSilent = candidate;
			}
			if (!silent)
				silent = freeSilent ? freeSilent : &s_channels[0];

			memset(silent, 0, sizeof(*silent));
			silent->entnum = entityNum;
			silent->entchannel = entchannel;
			silent->thesfx = sfx;
			silent->bPlaying = true;
			silent->iLastPlayTime = Sys_Milliseconds();
			if (entityNum >= 0 && entityNum < MAX_GENTITIES)
				s_entityWavVol[entityNum] = -1;

			if (s_xboxSilentVoiceStartsLogged < 32)
			{
				int lipLength = sfx->pLipSyncData ? *(int*)sfx->pLipSyncData : 0;
				Com_Printf("JA: Xbox silent voice start ent=%d chan=%d handle=%d lip=%d samples=%d durationMs=%d\n",
					entityNum, entchannel, sfxHandle, sfx->pLipSyncData ? 1 : 0, lipLength, sfx->iSoundDurationMs);
				s_xboxSilentVoiceStartsLogged++;
			}
		}
		return;
	}
#endif

#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start size lookup enter code=0x%x", sfx->iFileCode);
#endif
	int soundFileSize = Sys_GetSoundFileCodeSize(sfx->iFileCode);
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start size lookup exit size=%d", soundFileSize);
#endif
	if(soundFileSize == -1) {
#ifdef _XBOX
		if (S_XboxIsFoleySound(entchannel, sfx))
		{
			static int s_xboxFoleyMissingLogBudget = 32;
			if (s_xboxFoleyMissingLogBudget > 0)
			{
				const char *foleyName = Sys_GetSoundFileCodeName(sfx->iFileCode);
				XBLog_WriteRingMarkerf("STEFX_FOLEY: missing stream code=0x%x name='%s'",
					sfx->iFileCode, foleyName ? foleyName : "<unknown>");
				--s_xboxFoleyMissingLogBudget;
			}
		}
#endif
		return;
	}

	int flags = Sys_GetSoundFileCodeFlags(sfx->iFileCode);
#ifdef _XBOX
	if (S_XboxDN3ProofMapActive())
	{
		const char *soundName = Sys_GetSoundFileCodeName(sfx->iFileCode);
		const char *category = "sfx";
		int *budget = NULL;
		static int voiceBudget = 48;
		static int weaponBudget = 48;
		static int foleyBudget = 48;
		static int ambientBudget = 32;
		static int otherBudget = 32;

		if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
			entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
			(sfx->iFlags & SFX_FLAG_VOICE))
		{
			category = "vo";
			budget = &voiceBudget;
		}
		else if (entchannel == CHAN_WEAPON ||
			(soundName && (strstr(soundName, "weapons/") || strstr(soundName, "weapon/") ||
				strstr(soundName, "weapons\\") || strstr(soundName, "weapon\\"))))
		{
			category = "weapon";
			budget = &weaponBudget;
		}
		else if (entchannel == CHAN_AMBIENT ||
			(soundName && (strstr(soundName, "ambient") || strstr(soundName, "world/") || strstr(soundName, "world\\"))))
		{
			category = "ambient";
			budget = &ambientBudget;
		}
		else if (entchannel == CHAN_BODY || entchannel == CHAN_ITEM ||
			entchannel == CHAN_LOCAL_SOUND ||
			(soundName && (strstr(soundName, "foot") || strstr(soundName, "step") ||
				strstr(soundName, "pain") || strstr(soundName, "fall"))))
		{
			category = "foley";
			budget = &foleyBudget;
		}
		else
		{
			budget = &otherBudget;
		}

		if (*budget > 0)
		{
			Com_Printf("STEFX_DN3_PROOF: audio start category=%s name='%s' ent=%d chan=%d handle=%d fileCode=0x%x flags=0x%x resident=%d levelMap='%s' map='%s' cl_map='%s'\n",
				category,
				soundName ? soundName : "<unknown>",
				entityNum,
				entchannel,
				sfxHandle,
				sfx->iFileCode,
				sfx->iFlags,
				(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
				S_XboxLevelMapName(),
				Cvar_VariableString("mapname"),
				Cvar_VariableString("cl_mapname"));
			--(*budget);
		}
	}

	if (S_XboxDN3ProofMapActive() &&
		(entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
		 entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
		 (sfx->iFlags & SFX_FLAG_VOICE)))
	{
		const char *soundName = Sys_GetSoundFileCodeName(sfx->iFileCode);
		Com_Printf("STEFX_DN3_PROOF: audio start category=vo name='%s' ent=%d chan=%d handle=%d fileCode=0x%x flags=0x%x bankSize=%d resident=%d\n",
			soundName ? soundName : "<unknown>",
			entityNum,
			entchannel,
			sfxHandle,
			sfx->iFileCode,
			sfx->iFlags,
			Sys_GetSoundFileCodeSize(sfx->iFileCode),
			(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0);
	}
#endif

	if (sfx->iFlags & SFX_FLAG_UNLOADED){
#ifdef _XBOX
		if (stefxTraceVoiceStart)
			XBLog_WriteCriticalf("STEFX_SP_AUDIO: start load enter");
		S_XboxRetirePriorVoiceForReplacement(entityNum, entchannel, sfx);
#endif
		S_StartLoadSound(sfx);
#ifdef _XBOX
		if (stefxTraceVoiceStart)
			XBLog_WriteCriticalf("STEFX_SP_AUDIO: start load exit flags=0x%x", sfx->iFlags);
		if (stefxIsVoiceRequest && (sfx->iFlags & SFX_FLAG_UNLOADED))
		{
			++g_SPXBAudioVoiceQueuedLoadCount;
		}
#endif
	}
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start touch enter");
#endif
	SND_TouchSFX(sfx);
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start touch exit");
#endif

	// pick a channel to play on
	bool is2D = false;
	for (int i = 0; i < s_numListeners; ++i)
	{
		if ((entityNum == s_listeners[i].entnum && !origin) ||
			(origin &&
			origin[0] == s_listeners[i].pos[0] &&
			origin[1] == s_listeners[i].pos[1] &&
			origin[2] == s_listeners[i].pos[2]))
		{
			is2D = true;
			break;
		}
	}

	if(entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER)
		is2D	= true;

#ifdef _XBOX
	// Diagnostic: s_voice2D 1 forces dialogue onto the non-positional path,
	// bypassing 3D processing (HRTF, distance, panning) while leaving the
	// file, decoder and mixer identical.  Discriminates 'asset is dull' from
	// '3D playback dulls it'.  0 = normal behaviour.
	{
		static cvar_t *s_voice2DCvar = NULL;
		if (!s_voice2DCvar) s_voice2DCvar = Cvar_Get("s_voice2D", "0", 0);
		if (s_voice2DCvar && s_voice2DCvar->integer &&
			(entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN))
		{
			is2D = true;
		}
	}
#endif


	// super hack so we can hear the explosionson t2_wedge
	if( !stricmp(S_XboxLevelMapName(), "t2_wedge") && (flags & (1 <<SFF_TIEEXPLODE)))
		is2D	= true;

	if (entchannel == CHAN_VOICE)
	{
		// Make howlers and sand_creature VOICE effects use the normal fall-off (they will still be affected
		// by the Voice Volume)
		if ((flags & (1 << SFF_SAND_CREATURE)) || 
				(flags & (1 << SFF_HOWLER)))
		{
			entchannel = CHAN_VOICE_ATTEN;
		}
	}
	if (entchannel == CHAN_WEAPON)
	{
		// Check if we are playing a 'charging' sound, if so, stop it now ..
		ch = s_channels + 1;
		for (int i = 1; i < s_numChannels; i++, ch++)
		{
			unsigned int weaponSoundFlags;

			if(ch->thesfx)
				weaponSoundFlags = Sys_GetSoundFileCodeFlags(ch->thesfx->iFileCode);

			if ((ch->entnum == entityNum) && (ch->entchannel == CHAN_WEAPON) && (ch->thesfx) && (weaponSoundFlags & (1 <<SFF_ALTCHARGE)))
			{
				// Stop this sound
				alSourceStop(ch->alSource);
				alSourcei(ch->alSource, AL_BUFFER, NULL);
				ch->bPlaying = false;
				ch->thesfx = NULL;
				break;
			}
		}
	}
	else
	{
		ch = s_channels + 1;
		for (int i = 1; i < s_numChannels; i++, ch++)
		{
			unsigned int fallSoundFlags;

			if(ch->thesfx)
				fallSoundFlags = Sys_GetSoundFileCodeFlags(ch->thesfx->iFileCode);
		
			if ((ch->entnum == entityNum) && (ch->thesfx) && 
					(fallSoundFlags & (1 << SFF_FALLING)))
			{
				// Stop this sound
				alSourceStop(ch->alSource);
				alSourcei(ch->alSource, AL_BUFFER, NULL);
				ch->bPlaying = false;
				ch->thesfx = NULL;
				break;
			}
		}
	}
	
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start pick channel enter is2D=%d", is2D ? 1 : 0);
#endif
	ch = S_PickChannel( entityNum, entchannel, is2D, sfx );
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start pick channel exit channel=%p", ch);
#endif
	if (!ch) {
#ifdef _XBOX
		if (S_XboxIsFoleySound(entchannel, sfx))
		{
			static int s_xboxFoleyNoChannelLogBudget = 32;
			if (s_xboxFoleyNoChannelLogBudget > 0)
			{
				const char *foleyName = Sys_GetSoundFileCodeName(sfx->iFileCode);
				XBLog_WriteRingMarkerf("STEFX_FOLEY: no channel ent=%d chan=%d flags=0x%x resident=%d name='%s'",
					entityNum, entchannel, sfx->iFlags,
					(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
					foleyName ? foleyName : "<unknown>");
				--s_xboxFoleyNoChannelLogBudget;
			}
		}
		if (entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN ||
			entchannel == CHAN_VOICE_GLOBAL || entchannel == CHAN_ANNOUNCER ||
			(sfx->iFlags & SFX_FLAG_VOICE))
		{
			Com_Printf("STEFX: S_StartSound no channel ent=%d chan=%d handle=%d code=0x%x flags=0x%x resident=%d loading=%d\n",
				entityNum, entchannel, sfxHandle, sfx->iFileCode, sfx->iFlags,
				(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
				(sfx->iFlags & SFX_FLAG_LOADING) ? 1 : 0);
		}
#endif
		return;
	}

	if (!is2D)
	{
		SetChannelOrigin(ch, origin, entityNum);
	}
		
	if (entchannel == CHAN_AUTO && (sfx->iFlags & SFX_FLAG_VOICE)) {
		entchannel = CHAN_VOICE; // Compensate of the incompetance of others. Yeah. ;)
//		entchannel = CHAN_VOICE_ATTEN; // Super hack to put Rancor noises on a different channel E3!
	}

	ch->master_vol = SOUND_MAXVOL;	//FIXME: Um.. control?
	ch->fLastVolume = -1;
	ch->entnum = entityNum;
	ch->entchannel = entchannel;
	ch->thesfx = sfx;
	ch->bPlaying = false;
	ch->bLooping = false;
	ch->iLastPlayTime = 0;

	if (entchannel < CHAN_AMBIENT && IsListenerEnt(ch->entnum))
	{
		ch->master_vol = SOUND_MAXVOL * SOUND_FMAXVOL;	//this won't be attenuated so let it scale down
	}
	if ( entchannel == CHAN_VOICE || entchannel == CHAN_VOICE_ATTEN || entchannel == CHAN_VOICE_GLOBAL ) 
	{
		s_entityWavVol[ ch->entnum ] = -1;	//we've started the sound but it's silent for now
#ifdef _XBOX
		g_SPXBHMAudioVoiceStartCount++;
		static int s_xboxVoiceAssignLogs = 0;
		if (s_xboxVoiceAssignLogs < 128)
		{
			const char *soundName = Sys_GetSoundFileCodeName(sfx->iFileCode);
			Com_Printf("STEFX_VOICE_TRACE: assigned ent=%d chan=%d source=%u code=0x%x flags=0x%x resident=%d buffer=%u name='%s'\n",
				ch->entnum, ch->entchannel, ch->alSource, sfx->iFileCode, sfx->iFlags,
				(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0, sfx->Buffer,
				soundName ? soundName : "<unknown>");
			s_xboxVoiceAssignLogs++;
		}
#endif
	}
#ifdef _XBOX
	if (stefxTraceVoiceStart)
		XBLog_WriteCriticalf("STEFX_SP_AUDIO: start exit ent=%d chan=%d source=%u", ch->entnum, ch->entchannel, ch->alSource);
#endif
}

/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound( sfxHandle_t sfxHandle, int channelNum ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	// Play a 2D sound -- doesn't matter which listener we use
	S_StartSound (NULL, 0, (soundChannel_t)channelNum, sfxHandle );
}


/*
==================
S_StartLocalLoopingSound
==================
*/
void S_StartLocalLoopingSound( sfxHandle_t sfxHandle) {
	vec3_t nullVec = {0,0,0};

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	// Play a 2D sound -- doesn't matter which listener we use
	S_AddLoopingSound( 0, nullVec, nullVec, sfxHandle, CHAN_AMBIENT );

}

// Kill an voice sounds from an entity
void S_KillEntityChannel(int entnum, int chan)
{
	int i;
	channel_t *ch;

	if ( !s_soundStarted ) {
		return;
	}

	// This code is only used by the UI stopVoice command now, this check
	// screws that usage up.
//	if ( entnum < s_numListeners && chan == CHAN_VOICE ) {
//		// don't kill player death sounds
//		return;
//	}

	ch = s_channels;
	for (i = 0; i < s_numChannels; i++, ch++)
	{
		if (ch->bPlaying &&
			ch->entnum == entnum &&
			ch->entchannel == chan)
		{
#ifdef _XBOX
			if (ch->thesfx &&
				(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
				 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
				 (ch->thesfx->iFlags & SFX_FLAG_VOICE)))
			{
				static int s_xboxVoiceKillLogs = 0;
				if (s_xboxVoiceKillLogs < 64)
				{
					Com_Printf("STEFX_VOICE_TRACE: kill entity channel ent=%d chan=%d code=0x%x playing=%d loading=%d\n",
						entnum, chan, ch->thesfx->iFileCode, ch->bPlaying ? 1 : 0,
						(ch->thesfx->iFlags & SFX_FLAG_LOADING) ? 1 : 0);
					s_xboxVoiceKillLogs++;
				}
			}
#endif
			alSourceStop(ch->alSource);
			ch->bPlaying = false;
	
			alSourcei(ch->alSource, AL_BUFFER, 0);
			if(ch->thesfx)
				ch->thesfx->pLipSyncData = NULL;
			ch->thesfx = NULL;
			ch->bLooping = false;
		}
	}
}

/*
==================
S_StopLoopingSound

Stops all active looping sounds on a specified entity.
Sort of a slow method though, isn't there some better way?
==================
*/
void S_StopLoopingSound( int entnum )
{
	if (!s_soundStarted) {
		return;
	}

	int i = 0;

	while (i < numLoopSounds)
	{
		if (loopSounds[i].entnum == entnum)
		{
			int x = i+1;
			while (x < numLoopSounds)
			{
				memcpy(&loopSounds[x-1], &loopSounds[x], sizeof(loopSounds[x]));
				x++;
			}
			numLoopSounds--;
		}
		i++;
	}
}

// returns length in milliseconds of supplied sound effect...  (else 0 for bad handle now)
//
extern int Sys_GetSoundFileCodeSize(unsigned int code);
float S_GetSampleLengthInMilliSeconds( sfxHandle_t sfxHandle)
{
	sfx_t *sfx;

	if (!s_soundStarted)
	{	//we have no sound, so let's just make a reasonable guess
		return 512;
	}

	if ( s_sfxCodes[sfxHandle] == INVALID_CODE ) {
		return 0.0f;
	}

	sfx = &s_sfxBlock[sfxHandle];
	if (sfx->iSoundDurationMs > 0)
	{
		return (float)sfx->iSoundDurationMs;
	}

	int size = Sys_GetSoundFileCodeSize(sfx->iFileCode);
	if (size < 0) return 0;

	// Until the async loader has parsed the WAV header, the encoded file size is
	// not a trustworthy duration.  Keep the legacy fallback bounded and base it
	// on the original 22.05 kHz, 16-bit mono assumption (44,100 bytes/second).
	return (float)(((__int64)size * 1000) / 44100);
}

/*
==================
S_LoadSound
==================
*/
void S_LoadSound( sfxHandle_t sfxHandle ) 
{
	/*const*/ sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle > MAX_SFX || s_sfxCodes[sfxHandle] == INVALID_CODE ) {
		return;
	}

	sfx = &s_sfxBlock[sfxHandle];

	if (sfx->iFlags & SFX_FLAG_UNLOADED){
		S_StartLoadSound(sfx);

		extern void S_DrainRawSoundData(void);
		S_DrainRawSoundData();
	}
}

/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
void S_ClearSoundBuffer( void ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}
}


/*
==================
S_StopAllSounds
==================
*/
void S_StopSounds(void)
{
	int i; //, j;
	channel_t *ch;

	if ( !s_soundStarted ) {
		return;
	}

	// stop looping sounds
	S_ClearLoopingSounds();

	// clear all the s_channels
	ch = s_channels;
	for (i = 0; i < s_numChannels; i++, ch++)
	{
		if (ch->bPlaying)
		{
			alSourceStop(ch->alSource);
			ch->bPlaying = false;
		}
	
		// free lip sync data
		if(ch->thesfx)
			ch->thesfx->pLipSyncData = NULL;

		alSourcei(ch->alSource, AL_BUFFER, 0);
		ch->thesfx = NULL;
		ch->bLooping = false;
	}

	// clear out the lip synching override array
	memset(s_entityWavVol, 0, sizeof(int) * MAX_GENTITIES);

	S_ClearSoundBuffer ();
}

/*
==================
S_StopAllSounds
 and music
==================
*/
void S_StopAllSounds(void) {
	if ( !s_soundStarted ) {
		return;
	}
	// stop the background music
	S_StopBackgroundTrack();

	S_StopSounds();
}

void S_StopAllSoundsExceptMusic(void) {
	if ( !s_soundStarted ) {
		return;
	}

	S_StopSounds();
}

/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/

/*
==================
S_ClearLoopingSounds

==================
*/
void S_ClearLoopingSounds( void )
{
	if ( !s_soundStarted ) {
		return;
	}

	int i;

	for (i = 0; i < MAX_LOOP_SOUNDS; i++)
	{
		loopSounds[i].bProcessed = false;
		loopSounds[i].bMarked = false;
		loopSounds[i].sfx = NULL;
	}

	numLoopSounds = 0;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
==================
*/
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle, soundChannel_t chan ) {
	/*const*/ sfx_t *sfx;

  	if ( !s_soundStarted || s_soundMuted || !s_loopEnabled ) {
		return;
	}
	if ( numLoopSounds >= MAX_LOOP_SOUNDS ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle > MAX_SFX || s_sfxCodes[sfxHandle] == INVALID_CODE ) {
		return;
	}

	sfx = &s_sfxBlock[sfxHandle];
	if (sfx->iFlags & SFX_FLAG_UNLOADED){
		S_StartLoadSound(sfx);
		if (sfx->iFlags & SFX_FLAG_UNLOADED) {
			return;
		}
	}
	SND_TouchSFX(sfx);

	loopSounds[numLoopSounds].origin[0] = origin[0];
	loopSounds[numLoopSounds].origin[1] = origin[1];
	loopSounds[numLoopSounds].origin[2] = origin[2];

	loopSounds[numLoopSounds].sfx = sfx;
	loopSounds[numLoopSounds].volume = SOUND_MAXVOL;
	loopSounds[numLoopSounds].entnum = entityNum;
	loopSounds[numLoopSounds].entchannel = chan;
	numLoopSounds++;
}


/*
==================
S_AddAmbientLoopingSound
==================
*/
void S_AddAmbientLoopingSound( const vec3_t origin, unsigned char volume, sfxHandle_t sfxHandle ) 
{
	/*const*/ sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted || !s_loopEnabled ) {
		return;
	}
	if ( numLoopSounds >= MAX_LOOP_SOUNDS ) {
		return;
	}

	if (volume == 0)
		return;

	if ( sfxHandle < 0 || sfxHandle > MAX_SFX || s_sfxCodes[sfxHandle] == INVALID_CODE ) {
		return;
	}

	sfx = &s_sfxBlock[sfxHandle];
#ifdef _XBOX
	{
		static int s_xboxAmbientLoopQueueLogBudget = 24;
		if (s_xboxAmbientLoopQueueLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AMBIENT: queue loop handle=%d code=0x%x flags=0x%x resident=%d volume=%d name='%s'",
				sfxHandle, sfx->iFileCode, sfx->iFlags,
				(sfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0, (int)volume,
				name ? name : "<unknown>");
			--s_xboxAmbientLoopQueueLogBudget;
		}
	}
#endif
	if (sfx->iFlags & SFX_FLAG_UNLOADED){
		S_StartLoadSound(sfx);
		if (sfx->iFlags & SFX_FLAG_UNLOADED) {
			return;
		}
	}
	SND_TouchSFX(sfx);

	loopSounds[numLoopSounds].origin[0] = origin[0];
	loopSounds[numLoopSounds].origin[1] = origin[1];
	loopSounds[numLoopSounds].origin[2] = origin[2];
	
	loopSounds[numLoopSounds].sfx = sfx;	
	loopSounds[numLoopSounds].volume = volume / 2;
	loopSounds[numLoopSounds].entnum = -1;
	loopSounds[numLoopSounds].entchannel = CHAN_AMBIENT;
	numLoopSounds++;
}




/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
#if defined(STEFX_SP_HOSTED_MP)
	if (entityNum >= 0 && entityNum < MAX_GENTITIES && origin)
	{
		VectorCopy(origin, s_stefxHolomatchEntityOrigins[entityNum]);
		s_stefxHolomatchEntityOriginValid[entityNum] = qtrue;
	}
#endif
	if ( !s_soundStarted ) {
		return;
	}

	channel_t *ch;
	int i;

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}

	if (entityNum == 0)
		return;
	
	ch = s_channels;
	for (i = 0; i < s_numChannels; i++, ch++)
	{
		if ((ch->bPlaying) && 
			(ch->entnum == entityNum) &&
			(!ch->b2D))
		{
			if (ch->origin[0] != origin[0] ||
				ch->origin[1] != origin[1] ||
				ch->origin[2] != origin[2])
			{
				ch->origin[0] = origin[0];
				ch->origin[1] = origin[1];
				ch->origin[2] = origin[2];
				ch->bOriginDirty = true;
			}
		}
	}
}


/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
void S_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], qboolean inwater )
{
	if ( !s_soundStarted || s_soundMuted ) {
		return; 
	}

	int index = 0;

#ifdef _XBOX
	g_SPXBHMAudioListenerState = ((unsigned int)SND_MAX_LISTENERS << 16) | (unsigned int)s_numListeners;
#endif

	if ( index >= s_numListeners ) {
		return;
	}

	listener_t *li = &s_listeners[index];

	li->entnum = entityNum;
	
	li->pos[0] = head[0];
	li->pos[1] = head[1];
	li->pos[2] = head[2];
	alListenerfv(li->handle, AL_POSITION, li->pos);
	
	li->orient[0] = axis[0][0];
	li->orient[1] = axis[0][1];
	li->orient[2] = axis[0][2];
	li->orient[3] = axis[2][0];
	li->orient[4] = axis[2][1];
	li->orient[5] = axis[2][2];
	
	alListenerfv(li->handle, AL_ORIENTATION, li->orient);
#ifdef _XBOX
	{
		static int s_xboxRespatializeLogs = 0;
		if (Cvar_VariableIntegerValue("stefx_hm_audio_proof") && s_xboxRespatializeLogs < 16)
		{
			Com_Printf("STEFX_HM_AUDIO_BACKEND: respatialize ent=%d listener=%d activeListeners=%d compiledListeners=%d pos=(%g,%g,%g) inwater=%d\n",
				entityNum,
				index,
				s_numListeners,
				SND_MAX_LISTENERS,
				head[0],
				head[1],
				head[2],
				inwater ? 1 : 0);
			s_xboxRespatializeLogs++;
		}
	}
#endif
}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
void S_STEFX_SplitScreen_SetLocalListener(int slot, int entityNum, const vec3_t head, const vec3_t axis[3], qboolean valid)
{
	listener_t *li;
	static int s_xboxSplitListenerLogBudget = 24;

	if (slot <= 0 || slot >= SND_MAX_LISTENERS)
	{
		return;
	}
	if (!valid || !head || !axis)
	{
		g_SPXBHMAudioListenerUpdateMask &= ~(1u << slot);
		return;
	}
	if (!s_soundStarted || s_soundMuted || s_xboxSilentAudio || slot >= s_numListeners)
	{
		return;
	}

	li = &s_listeners[slot];
	li->entnum = entityNum;
	li->pos[0] = head[0];
	li->pos[1] = head[1];
	li->pos[2] = head[2];
	alListenerfv(li->handle, AL_POSITION, li->pos);

	li->orient[0] = axis[0][0];
	li->orient[1] = axis[0][1];
	li->orient[2] = axis[0][2];
	li->orient[3] = axis[2][0];
	li->orient[4] = axis[2][1];
	li->orient[5] = axis[2][2];
	alListenerfv(li->handle, AL_ORIENTATION, li->orient);

	g_SPXBHMAudioListenerUpdateMask |= (1u << slot);
	g_SPXBHMAudioListenerState = ((unsigned int)SND_MAX_LISTENERS << 16) | (unsigned int)s_numListeners;

	if (Cvar_VariableIntegerValue("stefx_hm_audio_proof") && s_xboxSplitListenerLogBudget > 0)
	{
		Com_Printf("STEFX_HM_AUDIO_LISTENER: slot=%d ent=%d activeListeners=%d compiledListeners=%d mask=0x%x pos=(%g,%g,%g)\n",
			slot,
			entityNum,
			s_numListeners,
			SND_MAX_LISTENERS,
			(unsigned int)g_SPXBHMAudioListenerUpdateMask,
			head[0],
			head[1],
			head[2]);
		--s_xboxSplitListenerLogBudget;
	}
}
#endif

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( void ) {
#ifdef _XBOX
	++g_SPXBAudioUpdateSerial;
	g_SPXBAudioUpdateStage = 0x41553030; /* 'AU00': entered */
#endif
	if ( !s_soundStarted ) {
	#ifdef _XBOX
		g_SPXBAudioUpdateStage = 0x41554630; /* 'AUF0': sound not started */
	#endif
		return;
	}

	// don't update too often
	int now = Sys_Milliseconds();
	if (now - s_updateTime < SOUND_UPDATE_TIME) {
	#ifdef _XBOX
		g_SPXBAudioUpdateStage = 0x41554631; /* 'AUF1': rate limited */
	#endif
		return;
	}
	s_updateTime = now;
	
	if ( s_soundMuted ) {
#ifdef _XBOX
		if (s_xboxSilentAudio)
		{
			S_XboxUpdateSilentVoiceVolumes();
			g_SPXBAudioUpdateStage = 0x41554632; /* 'AUF2': silent update complete */
			return;
		}
#endif
		alUpdate();
	#ifdef _XBOX
		g_SPXBAudioUpdateStage = 0x41554633; /* 'AUF3': muted update complete */
	#endif
		return;
	}

	// finish up any pending loads
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553130; /* 'AU10': before load retirement */
	#endif
	S_UpdateLoading();
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553131; /* 'AU11': load retirement complete */
	#endif

	// update the music stream
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553230; /* 'AU20': before music */
	#endif
	S_UpdateBackgroundTrack();
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553231; /* 'AU21': music complete */
	#endif

	// mix some sound
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553330; /* 'AU30': before channel mix */
	#endif
	S_Update_();
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553331; /* 'AU31': channel mix complete */
	#endif

	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41553430; /* 'AU40': before OpenAL update */
	#endif
	alUpdate();
	#ifdef _XBOX
	g_SPXBAudioUpdateStage = 0x41554646; /* 'AUFF': complete */
	#endif
}

static void UpdatePosition(channel_t *ch)
{
	if ( !ch->b2D )
	{
		if ( ch->bLooping && ch->bPlaying )
		{
			loopSound_t	*loop = &loopSounds[ch->loopChannel];
			if ( loop->origin[0] != ch->origin[0] ||
				loop->origin[1] != ch->origin[1] ||
				loop->origin[2] != ch->origin[2] )
			{
				ch->origin[0] = loop->origin[0];
				ch->origin[1] = loop->origin[1];
				ch->origin[2] = loop->origin[2];
				ch->bOriginDirty = true;
			}
		}
		
		if ( ch->bOriginDirty )
		{
			alSourcefv(ch->alSource, AL_POSITION, ch->origin);
			ch->bOriginDirty = false;
		}
	}
}

static void UpdateGain(channel_t *ch)
{
	float v = 0.f;

	if ( ch->bLooping && ch->bPlaying )
	{
		loopSound_t	*loop = &loopSounds[ch->loopChannel];
		if ( loop->volume != ch->master_vol )
		{
			ch->master_vol = loop->volume;
		}
	}
	
	if ( ch->entchannel == CHAN_ANNOUNCER ||
		ch->entchannel == CHAN_VOICE || 
		ch->entchannel == CHAN_VOICE_ATTEN || 
		ch->entchannel == CHAN_VOICE_GLOBAL )
	{
		v = ((float)(ch->master_vol) * s_voice_volume->value) / 255.0f;
	}
	else if ( ch->entchannel == CHAN_MUSIC )
	{
		v = ((float)(ch->master_vol) * s_music_volume->value) / 255.f;
	}
	else
	{
		v = ((float)(ch->master_vol) * s_effects_volume->value) / 255.f;
	}
	
	if ( ch->fLastVolume != v)
	{
		alSourcef(ch->alSource, AL_GAIN, v);
		ch->fLastVolume = v;
	}
}

static void UpdatePlayState(channel_t *ch)
{
	if (!ch->bPlaying) return;

	if (ch->bLooping)
	{
		// Looping sound
		loopSound_t	*loop = &loopSounds[ch->loopChannel];
		
		if (loop->bProcessed == false && loop->sfx != NULL &&
			(loop->sfx == ch->thesfx ||
			(loop->sfx->iFlags & SFX_FLAG_DEFAULT)))
		{
			// Playing
			loop->bProcessed = true;
		}
		else
		{
			// Sound no longer needed
			alSourceStop(ch->alSource);
			alSourcei(ch->alSource, AL_BUFFER, 0);
			ch->thesfx = NULL;
			ch->bPlaying = false;
		}
	}
	else
	{
		// Single shot sound
		ALint state;
		alGetSourcei(ch->alSource, AL_SOURCE_STATE, &state);
		if (ch->thesfx &&
			(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
			 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
			 (ch->thesfx->iFlags & SFX_FLAG_VOICE)) &&
			ch->thesfx->iSoundDurationMs > 0)
		{
			const int age = Sys_Milliseconds() - (int)ch->iLastPlayTime;
			if (age >= ch->thesfx->iSoundDurationMs + 150)
			{
				static int s_xboxVoiceDurationStopsLogged = 0;
				if (s_xboxVoiceDurationStopsLogged < 64)
				{
					Com_Printf("STEFX: Xbox voice duration stop ent=%d chan=%d code=0x%x age=%d duration=%d alState=%d\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, age, ch->thesfx->iSoundDurationMs, state);
					s_xboxVoiceDurationStopsLogged++;
				}
				alSourceStop(ch->alSource);
				alSourcei(ch->alSource, AL_BUFFER, 0);
				if (ch->entnum >= 0 && ch->entnum < MAX_GENTITIES)
				{
					s_entityWavVol[ch->entnum] = 0;
				}
				ch->thesfx = NULL;
				ch->bPlaying = false;
				return;
			}
		}
		if (state == AL_STOPPED)
		{
			const int stopAge = Sys_Milliseconds() - (int)ch->iLastPlayTime;
			if (ch->thesfx &&
				(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
				 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
				 (ch->thesfx->iFlags & SFX_FLAG_VOICE)))
			{
#ifdef _XBOX
				g_SPXBAudioVoiceLastStopCode = (unsigned int)ch->thesfx->iFileCode;
				g_SPXBAudioVoiceLastStopAge = (unsigned int)stopAge;
				if (stopAge < 250)
				{
					++g_SPXBAudioVoiceEarlyStopCount;
				}
#endif
				static int s_xboxVoiceAlStopsLogged = 0;
				if (s_xboxVoiceAlStopsLogged < 64)
				{
					Com_Printf("STEFX: Xbox voice AL stopped ent=%d chan=%d code=0x%x age=%d duration=%d\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode,
						Sys_Milliseconds() - (int)ch->iLastPlayTime, ch->thesfx->iSoundDurationMs);
					s_xboxVoiceAlStopsLogged++;
				}
			}
			alSourcei(ch->alSource, AL_BUFFER, 0);
			ch->thesfx = NULL;
			ch->bPlaying = false;
		}
	}
}

static void UpdateAttenuation(channel_t *ch)
{
	if (!ch->b2D)
	{
		switch (ch->entchannel)
		{
		case CHAN_VOICE:
			alSourcef(ch->alSource, AL_REFERENCE_DISTANCE, 2000.0f); // bring up 3d voice volumes
			break;
		case CHAN_VOICE_ATTEN:
			alSourcef(ch->alSource, AL_REFERENCE_DISTANCE, 300.0f);
			break;
		case CHAN_LESS_ATTEN:
		case CHAN_ANNOUNCER:
		case CHAN_LOCAL_SOUND:
		case CHAN_BODY:
		case CHAN_VOICE_GLOBAL:
		case CHAN_LOCAL:
			alSourcef(ch->alSource, AL_REFERENCE_DISTANCE, 1500.0f);
			break;
		default:
			alSourcef(ch->alSource, AL_REFERENCE_DISTANCE, 300.0f);
			break;
		}
	}
}

static void PlaySingleShot(channel_t *ch)
{
	const qboolean isVoice = (ch && ch->thesfx &&
		(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
		 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
		 (ch->thesfx->iFlags & SFX_FLAG_VOICE))) ? qtrue : qfalse;

#ifdef _XBOX
	if (isVoice)
	{
		static int s_xboxVoicePrePlayLogs = 0;
		if (s_xboxVoicePrePlayLogs < 128)
		{
			const char *soundName = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
			Com_Printf("STEFX_VOICE_TRACE: pre play ent=%d chan=%d source=%u code=0x%x flags=0x%x resident=%d buffer=%u duration=%d name='%s'\n",
				ch->entnum, ch->entchannel, ch->alSource, ch->thesfx->iFileCode,
				ch->thesfx->iFlags, (ch->thesfx->iFlags & SFX_FLAG_RESIDENT) ? 1 : 0,
				ch->thesfx->Buffer, ch->thesfx->iSoundDurationMs,
				soundName ? soundName : "<unknown>");
			s_xboxVoicePrePlayLogs++;
		}
	}
#endif
	if (!ch || !ch->thesfx || ch->thesfx->Buffer == 0)
	{
#ifdef _XBOX
		static int s_xboxZeroBufferPlayLogBudget = 64;
		if (s_xboxZeroBufferPlayLogBudget > 0)
		{
			const char *soundName = (ch && ch->thesfx) ?
				Sys_GetSoundFileCodeName(ch->thesfx->iFileCode) : NULL;
			XBLog_WriteCriticalf("STEFX_AUDIO_PLAY: rejected zero buffer ent=%d chan=%d code=0x%x flags=0x%x voice=%d name='%s'",
				ch ? ch->entnum : -1,
				ch ? ch->entchannel : -1,
				(ch && ch->thesfx) ? ch->thesfx->iFileCode : 0,
				(ch && ch->thesfx) ? ch->thesfx->iFlags : 0,
				isVoice ? 1 : 0,
				soundName ? soundName : "<unknown>");
			--s_xboxZeroBufferPlayLogBudget;
		}
		if (isVoice)
		{
			++g_SPXBAudioVoicePlayFailureCount;
			if (ch->entnum >= 0 && ch->entnum < MAX_GENTITIES)
			{
				s_entityWavVol[ch->entnum] = 0;
			}
		}
#endif
		if (ch)
		{
			ch->thesfx = NULL;
			ch->bPlaying = false;
		}
		return;
	}

	#ifdef _XBOX
	QAL_SetSourceVoice(ch->alSource, isVoice != qfalse);
	#endif
	alSourcei(ch->alSource, AL_LOOPING, AL_FALSE);
	
	UpdateAttenuation(ch);
	UpdatePosition(ch);
	UpdateGain(ch);
	
	// Attach buffer to source
	alSourcei(ch->alSource, AL_BUFFER, ch->thesfx->Buffer);
	
	// Clear error state, and check for successful Play call
	alGetError();
	alSourcePlay(ch->alSource);
	ALenum playError = alGetError();
	if (playError == AL_NO_ERROR)
	{
		ch->bPlaying = true;
		ch->iLastPlayTime = Sys_Milliseconds();
#ifdef _XBOX
		if (isVoice)
		{
			++g_SPXBAudioVoicePlaySuccessCount;
			g_SPXBAudioVoiceLastPlayCode = (unsigned int)ch->thesfx->iFileCode;
		}
		if (S_XboxIsFoleySound((soundChannel_t)ch->entchannel, ch->thesfx))
		{
			static int s_xboxFoleyPlayLogBudget = 64;
			if (s_xboxFoleyPlayLogBudget > 0)
			{
				const char *foleyName = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
				XBLog_WriteRingMarkerf("STEFX_FOLEY: playing ent=%d chan=%d source=%u buffer=%u name='%s'",
					ch->entnum, ch->entchannel, ch->alSource, ch->thesfx->Buffer,
					foleyName ? foleyName : "<unknown>");
				--s_xboxFoleyPlayLogBudget;
			}
		}
		if (isVoice)
		{
			static int s_xboxVoicePlayStartLogs = 0;
			if (s_xboxVoicePlayStartLogs < 64)
			{
				const char *soundName = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
				XBLog_WriteCriticalf("STEFX_VOICE_PLAY: start ent=%d chan=%d source=%u code=0x%x duration=%d buffer=%d lip=%d faceStart=%d name='%s'",
					ch->entnum, ch->entchannel, ch->alSource, ch->thesfx->iFileCode,
					ch->thesfx->iSoundDurationMs, ch->thesfx->Buffer,
					ch->thesfx->pLipSyncData ? 1 : 0,
					S_XboxNormalizeFaceVolume( 1 ),
					soundName ? soundName : "<unknown>");
				s_xboxVoicePlayStartLogs++;
			}
		}
#endif
	}
#ifdef _XBOX
	else
	{
		if (S_XboxIsFoleySound((soundChannel_t)ch->entchannel, ch->thesfx))
		{
			static int s_xboxFoleyPlayFailLogBudget = 32;
			if (s_xboxFoleyPlayFailLogBudget > 0)
			{
				const char *foleyName = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
				XBLog_WriteRingMarkerf("STEFX_FOLEY: play failed ent=%d chan=%d error=0x%x buffer=%u name='%s'",
					ch->entnum, ch->entchannel, playError, ch->thesfx->Buffer,
					foleyName ? foleyName : "<unknown>");
				--s_xboxFoleyPlayFailLogBudget;
			}
		}
		if (isVoice)
		{
			++g_SPXBAudioVoicePlayFailureCount;
			static int s_xboxVoicePlayFailLogs = 0;
			if (s_xboxVoicePlayFailLogs < 64)
			{
				XBLog_WriteCriticalf("STEFX_VOICE_PLAY: failed ent=%d chan=%d code=0x%x error=0x%x buffer=%d",
					ch->entnum, ch->entchannel, ch->thesfx->iFileCode, playError, ch->thesfx->Buffer);
				s_xboxVoicePlayFailLogs++;
			}
			if (ch->entnum >= 0 && ch->entnum < MAX_GENTITIES)
			{
				s_entityWavVol[ch->entnum] = 0;
			}
		}
		ch->thesfx = NULL;
		ch->bPlaying = false;
	}
#endif
}

#ifdef _XBOX
void S_XboxOnSoundLoaded(sfx_t *sfx)
{
	if (!sfx || !(sfx->iFlags & SFX_FLAG_RESIDENT))
	{
		return;
	}

	for (int i = 0; i < s_numChannels; ++i)
	{
		channel_t *ch = &s_channels[i];
		if (ch->thesfx != sfx || ch->bPlaying || ch->bLooping)
		{
			continue;
		}

		if (sfx->Buffer == 0)
		{
			continue;
		}

		if (ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
			ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
			(sfx->iFlags & SFX_FLAG_VOICE))
		{
			++g_SPXBAudioVoiceLoadedWakeCount;
			static int s_xboxLoadedWakeLogged = 0;
			if (s_xboxLoadedWakeLogged < 64)
			{
				Com_Printf("STEFX: S_XboxOnSoundLoaded wake ent=%d chan=%d code=0x%x flags=0x%x buffer=%d\n",
					ch->entnum, ch->entchannel, sfx->iFileCode, sfx->iFlags, sfx->Buffer);
				s_xboxLoadedWakeLogged++;
			}
		}

		PlaySingleShot(ch);
	}

	if (sfx->iFlags & SFX_FLAG_VOICE)
	{
		qboolean found = qfalse;
		for (int i = 0; i < s_numChannels; ++i)
		{
			channel_t *ch = &s_channels[i];
			if (ch->thesfx == sfx)
			{
				found = qtrue;
				break;
			}
		}
		if (!found)
		{
			static int s_xboxLoadedNoWaiterLogged = 0;
			if (s_xboxLoadedNoWaiterLogged < 64)
			{
				const char *soundName = Sys_GetSoundFileCodeName(sfx->iFileCode);
				Com_Printf("STEFX_VOICE_TRACE: loaded voice has no channel code=0x%x flags=0x%x buffer=%u name='%s'\n",
					sfx->iFileCode, sfx->iFlags, sfx->Buffer,
					soundName ? soundName : "<unknown>");
				s_xboxLoadedNoWaiterLogged++;
			}
		}
	}
}
#endif

void UpdateLoopingSounds()
{
	// Look for non-processed loops that are ready to play
	for (int j = 0; j < numLoopSounds; j++)
	{
		loopSound_t	*loop = &loopSounds[j];
		
		{
			// merge all loops with the same sfx into a single loop
			float num = 1;
			for (int k = j+1; k < numLoopSounds; ++k)
			{
				if (loopSounds[k].sfx == loop->sfx &&
					loopSounds[k].entnum == loop->entnum)
				{
					loop->origin[0] += loopSounds[k].origin[0];
					loop->origin[1] += loopSounds[k].origin[1];
					loop->origin[2] += loopSounds[k].origin[2];
					loop->volume += loopSounds[k].volume;
					loopSounds[k].bProcessed = true;
					num += 1;
				}
			}

			loop->origin[0] /= num;
			loop->origin[1] /= num;
			loop->origin[2] /= num;
			loop->volume /= (int)num;
		}

		if (loop->bProcessed == false && (loop->sfx->iFlags & SFX_FLAG_RESIDENT))
		{
			// play the loop
			bool is2D = false;
			for (int i = 0; i < s_numListeners; ++i)
			{
				if (loop->entnum == s_listeners[i].entnum ||
					(loop->origin[0] == s_listeners[i].pos[0] &&
					loop->origin[1] == s_listeners[i].pos[1] &&
					loop->origin[2] == s_listeners[i].pos[2]))
				{
					is2D = true;
					break;
				}
			}

			channel_t *ch = S_PickChannel(0, 0, is2D, NULL);
			if (!ch) continue;

			ch->master_vol = loop->volume;
			ch->fLastVolume = -1;
			ch->entnum = loop->entnum;
			ch->entchannel = loop->entchannel;
			ch->thesfx = loop->sfx;
			ch->loopChannel = j;
			ch->bLooping = true;
			
			ch->origin[0] = loop->origin[0];
			ch->origin[1] = loop->origin[1];
			ch->origin[2] = loop->origin[2];
			ch->bOriginDirty = true;

			#ifdef _XBOX
			QAL_SetSourceVoice(ch->alSource, false);
			#endif
			alSourcei(ch->alSource, AL_LOOPING, AL_TRUE);
			alSourcei(ch->alSource, AL_BUFFER, ch->thesfx->Buffer);
			UpdateAttenuation(ch);
			UpdatePosition(ch);
			UpdateGain(ch);
			
			alGetError();
			alSourcePlay(ch->alSource);
			ALenum loopPlayError = alGetError();
			if (loopPlayError == AL_NO_ERROR)
			{
				ch->bPlaying = true;
				ch->iLastPlayTime = Sys_Milliseconds();
			#ifdef _XBOX
				{
					static int s_xboxLoopPlayLogBudget = 24;
					if (s_xboxLoopPlayLogBudget > 0)
					{
						const char *name = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
						XBLog_WriteRingMarkerf("STEFX_AMBIENT: loop playing ent=%d chan=%d source=%u buffer=%u volume=%d name='%s'",
							ch->entnum, ch->entchannel, ch->alSource, ch->thesfx->Buffer,
							ch->master_vol, name ? name : "<unknown>");
						--s_xboxLoopPlayLogBudget;
					}
				}
			#endif
			}
		#ifdef _XBOX
			else
			{
				static int s_xboxLoopFailLogBudget = 16;
				if (s_xboxLoopFailLogBudget > 0)
				{
					const char *name = Sys_GetSoundFileCodeName(ch->thesfx->iFileCode);
					XBLog_WriteRingMarkerf("STEFX_AMBIENT: loop play failed error=0x%x source=%u buffer=%u name='%s'",
						loopPlayError, ch->alSource, ch->thesfx->Buffer, name ? name : "<unknown>");
					--s_xboxLoopFailLogBudget;
				}
			}
		#endif
		}
	}
}

static void SyncChannelLoops(void)
{
	channel_t		*ch;
	int				i, j;

	// Try to match up channels with looping sounds
	// (The order of sounds in loopSounds can change
	// frame to frame.)
	ch = s_channels;
	for ( i = 0; i < s_numChannels ; i++, ch++ )
	{
		if ( ch->bPlaying && ch->bLooping )
		{
			for ( j = 0; j < numLoopSounds; ++j )
			{
				if ( ch->thesfx == loopSounds[j].sfx && 
					!loopSounds[j].bMarked )
				{
					ch->loopChannel = j;
					loopSounds[j].bMarked = true;
					break;
				}
			}
		}
	}
}

static int S_XboxNormalizeFaceVolume( int volume )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	// Elite Force reserves extension skins 1-4 for blink/frown states.
	// Its four talking mouth textures occupy extension skins 5-8.
	if ( volume >= 1 && volume <= 4 )
	{
		return volume + 4;
	}
#endif
	return volume;
}

static void S_XboxStoreFaceVolume( int entityNum, int volume, qboolean fromLipData )
{
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES )
	{
		return;
	}

	s_entityWavVol[entityNum] = volume;
	++g_SPXBAudioFaceUpdateCount;
	if ( fromLipData )
	{
		++g_SPXBAudioFaceLipDataUpdateCount;
	}
	else
	{
		++g_SPXBAudioFaceFallbackUpdateCount;
	}
	g_SPXBAudioFaceLastEntity = (unsigned int)entityNum;
	g_SPXBAudioFaceLastVolume = (unsigned int)volume;
}

void _UpdateLipSyncData( channel_t*	ch)
{
	int		samples;
	int		currentTime;
	int		timePlayed;
	int		length;
	char*	data;

	if (ch->thesfx->pLipSyncData == NULL)
	{
		//Com_Printf("Missing lip-sync info: %s\n", ch->thesfx->sSoundName);
		return;
	}

	// Get current time
	currentTime = Sys_Milliseconds();

	// Calculate how much time has passed since the sample was started
	timePlayed = currentTime - ch->iLastPlayTime;

	// There is a new computed lip-sync value every 1000 samples - so find out how many samples
	// have been played and lookup the value in the lip-sync table
	samples = (timePlayed * 22050) / 1000;

	// Get the number of total samples in this sound
	length	= *(int*)ch->thesfx->pLipSyncData;

	// Get a ptr to the lipsync data
	data	= (char*)((int*)ch->thesfx->pLipSyncData + 1);

	if ((ch->thesfx->pLipSyncData) && (samples < length))
	{
		int p = samples / 500 ;
		short t;

		if(p%2 == 0 )
		{
			t = data[p/2];
			t = t >> 4;
			if(t == 0)
				t = -1;

			S_XboxStoreFaceVolume( ch->entnum,
				S_XboxNormalizeFaceVolume( t ), qtrue );	// want left 4 bits
		}
		else
		{
			t = data[p/2] & 0x0f;
			if(t == 0)
				t = -1;
			S_XboxStoreFaceVolume( ch->entnum,
				S_XboxNormalizeFaceVolume( t ), qtrue );	// want right 4 bits
		}
		//Com_Printf("%s,  total samples = %d, current sample = %d, lip index = %d, lip type = %d \n", ch->thesfx->sSoundName, length, samples, p/2, s_entityWavVol[ ch->entnum ] );
	}

}

#ifdef _XBOX
static int S_XboxFallbackVoiceVolume(const channel_t *ch, int now)
{
	static const int pattern[] = { 1, 2, 4, 3, 2, 1, 3, 4 };
	int elapsed;
	int frame;

	if ( !ch )
	{
		return -1;
	}

	elapsed = now - (int)ch->iLastPlayTime;
	if ( elapsed < 0 )
	{
		elapsed = 0;
	}

	frame = ( elapsed / 90 ) % ( sizeof( pattern ) / sizeof( pattern[0] ) );
	return S_XboxNormalizeFaceVolume( pattern[frame] );
}

static void S_XboxUpdateSilentVoiceVolumes(void)
{
	if (!s_entityWavVol || !s_channels)
		return;

	memset(s_entityWavVol, 0, sizeof(int) * MAX_GENTITIES);

	int now = Sys_Milliseconds();
	for (int i = 0; i < MAX_CHANNELS; ++i)
	{
		channel_t *ch = &s_channels[i];
		if (!ch->thesfx || !ch->bPlaying)
			continue;

		if (ch->entchannel != CHAN_VOICE &&
			ch->entchannel != CHAN_VOICE_ATTEN &&
			ch->entchannel != CHAN_VOICE_GLOBAL &&
			ch->entchannel != CHAN_ANNOUNCER &&
			!(ch->thesfx->iFlags & SFX_FLAG_VOICE))
			continue;

		if (ch->entnum < 0 || ch->entnum >= MAX_GENTITIES)
			continue;

		if (ch->thesfx->pLipSyncData)
		{
			int length = *(int*)ch->thesfx->pLipSyncData;
			int samples = ((now - (int)ch->iLastPlayTime) * 22050) / 1000;
			if (samples >= length)
			{
				s_entityWavVol[ch->entnum] = 0;
				ch->bPlaying = false;
				ch->thesfx = NULL;
				continue;
			}
			_UpdateLipSyncData(ch);
		}
		else
		{
			int elapsed = now - (int)ch->iLastPlayTime;
			int duration = ch->thesfx->iSoundDurationMs > 0 ? ch->thesfx->iSoundDurationMs : 1200;
			if (elapsed > duration + 150)
			{
				static int s_xboxSilentVoiceStopsLogged = 0;
				if (s_xboxSilentVoiceStopsLogged < 64)
				{
					Com_Printf("STEFX: Xbox silent voice duration stop ent=%d chan=%d code=0x%x elapsed=%d duration=%d\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, elapsed, duration);
					s_xboxSilentVoiceStopsLogged++;
				}
				s_entityWavVol[ch->entnum] = 0;
				ch->bPlaying = false;
				ch->thesfx = NULL;
				continue;
			}
			S_XboxStoreFaceVolume( ch->entnum,
				S_XboxFallbackVoiceVolume(ch, now), qfalse );
		}

		if (s_xboxSilentVoiceUpdatesLogged < 48)
		{
			Com_Printf("JA: Xbox silent voice update ent=%d chan=%d vol=%d age=%d lip=%d\n",
				ch->entnum, ch->entchannel, s_entityWavVol[ch->entnum],
				now - (int)ch->iLastPlayTime, ch->thesfx->pLipSyncData ? 1 : 0);
			s_xboxSilentVoiceUpdatesLogged++;
		}
	}
}
#endif

void S_Update_(void)
{
	channel_t		*ch;
	int				i;
#ifdef _XBOX
	int				xboxActiveVoiceChannels = 0;
#endif

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}
	
	memset(s_entityWavVol, 0, sizeof(int) * MAX_GENTITIES);

	SyncChannelLoops();

	ch = s_channels;
	for ( i = 0; i < s_numChannels ; i++, ch++ )
	{	
		if ( !ch->thesfx ) continue;

		if ( ch->thesfx->iFlags & SFX_FLAG_UNLOADED )
		{
			// Keep one-shot sounds pending until their async load completes.
			// Marking them playing here causes UpdatePlayState to discard the
			// channel before PlaySingleShot ever attaches the loaded buffer.
#ifdef _XBOX
			if (!ch->bLooping)
			{
				static int s_xboxPendingUnloadLogged = 0;
				if (s_xboxPendingUnloadLogged < 64 &&
					(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
					 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
					 (ch->thesfx->iFlags & SFX_FLAG_VOICE)))
				{
					Com_Printf("STEFX: S_Update pending unloaded ent=%d chan=%d code=0x%x flags=0x%x\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, ch->thesfx->iFlags);
					s_xboxPendingUnloadLogged++;
				}
			}
#endif
			if (!ch->bLooping)
			{
				const qboolean isVoice =
					(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
					 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
					 (ch->thesfx->iFlags & SFX_FLAG_VOICE)) ? qtrue : qfalse;
				if (isVoice)
				{
					++g_SPXBAudioVoiceLoadRetryCount;
				}
				S_StartLoadSound(ch->thesfx);
				if (isVoice && !(ch->thesfx->iFlags & SFX_FLAG_UNLOADED))
				{
					++g_SPXBAudioVoiceLoadRetrySuccessCount;
					Com_Printf("STEFX_VOICE_TRACE: retry accepted ent=%d chan=%d code=0x%x flags=0x%x\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, ch->thesfx->iFlags);
				}
				// S_Override is the original game/ICARUS contract for a pending
				// voice task. An asynchronous Xbox buffer load remains part of
				// that voice's lifetime even before samples are available.
				if (isVoice && ch->entnum >= 0 && ch->entnum < MAX_GENTITIES)
				{
					s_entityWavVol[ch->entnum] = -1;
				}
				ch->bPlaying = false;
			}
			alSourceStop(ch->alSource);
			if (ch->thesfx->iFlags & (SFX_FLAG_UNLOADED | SFX_FLAG_LOADING))
			{
				continue;
			}
		}
		
		if ( ch->entchannel == CHAN_VOICE || 
			ch->entchannel == CHAN_VOICE_ATTEN || 
			ch->entchannel == CHAN_VOICE_GLOBAL ||
			ch->entchannel == CHAN_ANNOUNCER ||
			(ch->thesfx->iFlags & SFX_FLAG_VOICE) )
		{
			if ( ch->entnum >= 0 && ch->entnum < MAX_GENTITIES )
			{
				if(ch->bPlaying)
				{
#ifdef _XBOX
					++xboxActiveVoiceChannels;
#endif
					if ( ch->thesfx->pLipSyncData )
					{
						_UpdateLipSyncData(ch);
#ifdef _XBOX
						{
							static int s_stefxLipTraceBudget = 8;
							if (s_stefxLipTraceBudget > 0)
							{
								Com_Printf("STEFX_LIPTRACE: source=lipdata ent=%d chan=%d vol=%d code=0x%x flags=0x%x age=%d duration=%d playing=%d\n",
									ch->entnum,
									ch->entchannel,
									s_entityWavVol[ch->entnum],
									ch->thesfx->iFileCode,
									ch->thesfx->iFlags,
									Sys_Milliseconds() - (int)ch->iLastPlayTime,
									ch->thesfx->iSoundDurationMs,
									ch->bPlaying ? 1 : 0);
								--s_stefxLipTraceBudget;
							}
						}
#endif
					}
#ifdef _XBOX
					else
					{
						S_XboxStoreFaceVolume( ch->entnum,
							S_XboxFallbackVoiceVolume(ch, Sys_Milliseconds()), qfalse );
						{
							static int s_stefxFallbackLipTraceBudget = 8;
							if (s_stefxFallbackLipTraceBudget > 0)
							{
								Com_Printf("STEFX_LIPTRACE: source=fallback ent=%d chan=%d vol=%d code=0x%x flags=0x%x age=%d duration=%d playing=%d\n",
									ch->entnum,
									ch->entchannel,
									s_entityWavVol[ch->entnum],
									ch->thesfx->iFileCode,
									ch->thesfx->iFlags,
									Sys_Milliseconds() - (int)ch->iLastPlayTime,
									ch->thesfx->iSoundDurationMs,
									ch->bPlaying ? 1 : 0);
								--s_stefxFallbackLipTraceBudget;
							}
						}
						if (s_xboxSilentVoiceUpdatesLogged < 8)
						{
							Com_Printf("STEFX: Xbox voice fallback lip ent=%d chan=%d vol=%d sound='%s'\n",
								ch->entnum, ch->entchannel, s_entityWavVol[ch->entnum],
								ch->thesfx ? va("code=0x%08x", ch->thesfx->iFileCode) : "<null>");
							s_xboxSilentVoiceUpdatesLogged++;
						}
					}
#endif
				}
				else
				{
					s_entityWavVol[ch->entnum] = -1;
				}
			}

		}

		UpdatePosition(ch);
		UpdateGain(ch);

		if ( ch->bPlaying )
		{
			UpdatePlayState(ch);
		}
		else
		{
#ifdef _XBOX
			if ( !(ch->thesfx->iFlags & SFX_FLAG_RESIDENT) )
			{
				static int s_xboxPendingNonResidentLogged = 0;
				if (s_xboxPendingNonResidentLogged < 64 &&
					(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
					 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
					 (ch->thesfx->iFlags & SFX_FLAG_VOICE)))
				{
					Com_Printf("STEFX: S_Update waiting nonresident ent=%d chan=%d code=0x%x flags=0x%x buffer=%d\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, ch->thesfx->iFlags, ch->thesfx->Buffer);
					s_xboxPendingNonResidentLogged++;
				}
				continue;
			}
#else
			if ( !(ch->thesfx->iFlags & SFX_FLAG_RESIDENT) ) continue;
#endif
#ifdef _XBOX
			if (ch->thesfx &&
				(ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE_ATTEN ||
				 ch->entchannel == CHAN_VOICE_GLOBAL || ch->entchannel == CHAN_ANNOUNCER ||
				 (ch->thesfx->iFlags & SFX_FLAG_VOICE)))
			{
				static int s_xboxPendingPlayLogged = 0;
				if (s_xboxPendingPlayLogged < 64)
				{
					Com_Printf("STEFX: S_Update pending play ent=%d chan=%d code=0x%x flags=0x%x buffer=%d\n",
						ch->entnum, ch->entchannel, ch->thesfx->iFileCode, ch->thesfx->iFlags, ch->thesfx->Buffer);
					s_xboxPendingPlayLogged++;
				}
			}
#endif
			PlaySingleShot(ch);
		}
	}

	UpdateLoopingSounds();
#ifdef _XBOX
	g_SPXBHMAudioLipActiveCount = (unsigned int)xboxActiveVoiceChannels;
	{
		ALint audioBytes = 0;
		alGeti(AL_MEMORY_USED, &audioBytes);
		g_SPXBAudioMemUsed = (unsigned int)audioBytes;
	}
#endif
}



/*
===============================================================================

console functions

===============================================================================
*/

static void S_Play_f( void ) {
	int 	i;
	sfxHandle_t	h;
	char name[256];
	
	i = 1;
	while ( i<Cmd_Argc() ) {
		if ( !strrchr(Cmd_Argv(i), '.') ) {
			Com_sprintf( name, sizeof(name), "%s.wav", Cmd_Argv(1) );
		} else {
			Q_strncpyz( name, Cmd_Argv(i), sizeof(name) );
		}
		h = S_RegisterSound( name );
	if( h ) {
			S_StartLocalSound( h, CHAN_LOCAL_SOUND );
		}
		i++;
	}
}

/*
 * Crazy expanded play function:
 * playex <file name> xOffset yOffset zOffset channel
 */
#ifndef _JK2MP
static void S_PlayEx_f( void ) {
	sfxHandle_t	h;
	char		name[256] = { 0 };
	vec3_t		origin;
	int			entchannel;

	if (Cmd_Argc() < 6)
		return;

	Q_strncpyz( name, Cmd_Argv(1), sizeof(name) );
	h = S_RegisterSound( name );
	if (!h)
		return;

#if defined(STEFX_SP_HOSTED_MP)
	if (s_stefxHolomatchEntityOriginValid[0])
	{
		VectorCopy(s_stefxHolomatchEntityOrigins[0], origin);
	}
	else
	{
		VectorClear(origin);
	}
#else
	extern void G_EntityPosition( int i, vec3_t ret );
	G_EntityPosition(0, origin);
#endif

	origin[0] += atof(Cmd_Argv(2));
	origin[1] += atof(Cmd_Argv(3));
	origin[2] += atof(Cmd_Argv(4));

	entchannel = atoi(Cmd_Argv(5));

	S_StartSound(origin, 0, (soundChannel_t)entchannel, h);
}
#endif

static void S_Music_f( void ) {
	int		c;

	c = Cmd_Argc();

	if ( c == 2 ) {
		S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(1), qfalse );
	} else if ( c == 3 ) {
		S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2), qfalse );		
	} else {
		Com_Printf ("music <musicfile> [loopfile]\n");
		return;
	}
}

void S_SoundList_f( void ) {
}


/*
===============================================================================

background music functions

===============================================================================
*/

// fixme: need to move this into qcommon sometime?, but too much stuff altered by other people and I won't be able
//	to compile again for ages if I check that out...
//
// DO NOT replace this with a call to FS_FileExists, that's for checking about writing out, and doesn't work for this.
//
qboolean S_MusicFileExists( const char *psFilename )
{
	fileHandle_t fhTemp;

	char* pLoadName = S_FixMusicFileName(psFilename);
	
	FS_FOpenFileRead (pLoadName, &fhTemp, qtrue);	// qtrue so I can fclose the handle without closing a PAK
	if (!fhTemp) 
		return qfalse;
	
	FS_FCloseFile(fhTemp);
	static int s_xboxMusicExistsLogCount = 0;
	if (s_xboxMusicExistsLogCount < 96)
	{
		Com_Printf("STEFX: S_MusicFileExists '%s' -> '%s'\n", psFilename ? psFilename : "<null>", pLoadName);
		s_xboxMusicExistsLogCount++;
	}
	return qtrue;
}

static void S_StopBackgroundTrack_Actual( MusicInfo_t *pMusicInfo ) 
{
	pMusicInfo->bLoaded = false;
	pMusicInfo->Rewind();
	alStreamStop();
}

static void FreeMusic( MusicInfo_t *pMusicInfo )
{
	pMusicInfo->sLoadedDataName[0] = '\0';
}

// called only by snd_shutdown (from snd_restart or app exit)
//
static void S_UnCacheDynamicMusic( void )
{
	for (int i = eBGRNDTRACK_DATABEGIN; i != eBGRNDTRACK_DATAEND; i++)
	{
		FreeMusic( &tMusic_Info[i]);
	}
}

static qboolean S_StartBackgroundTrack_Actual( MusicInfo_t *pMusicInfo, qboolean qbDynamic, const char *intro, const char *loop )
{
	Q_strncpyz( sMusic_BackgroundLoop, loop, sizeof( sMusic_BackgroundLoop ));	

	char* name = S_FixMusicFileName(intro);
	Com_Printf("STEFX_DN3_PROOF: music request intro='%s' fixed='%s' loop='%s' dynamic=%d\n",
		intro ? intro : "<null>",
		name ? name : "<null>",
		loop ? loop : "<null>",
		qbDynamic ? 1 : 0);
#ifdef _XBOX
	Com_Printf("STEFX_DN3_PROOF: music request intro='%s' fixed='%s' loop='%s' dynamic=%d levelMap='%s' map='%s' cl_map='%s'\n",
		intro ? intro : "<null>",
		name ? name : "<null>",
		loop ? loop : "<null>",
		qbDynamic ? 1 : 0,
		S_XboxLevelMapName(),
		Cvar_VariableString("mapname"),
		Cvar_VariableString("cl_mapname"));
#endif

	if ( !intro[0] ) {
		S_StopBackgroundTrack_Actual( pMusicInfo );
		return qfalse;
	}

	// new bit, if file requested is not same any loaded one (if prev was in-mem), ditch it...
	//
	if (Q_stricmp(name, pMusicInfo->sLoadedDataName))
	{
		FreeMusic( pMusicInfo );
	}

	//
	// open up a wav file and get all the info
	//
	fileHandle_t handle;
	int len = FS_FOpenFileRead( name, &handle, qtrue );
	Com_Printf("STEFX: Xbox music file open '%s' len=%d handle=%d\n",
		name ? name : "<null>", len, handle);
	if ( !handle ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", name );
		S_StopBackgroundTrack_Actual( pMusicInfo );
		return qfalse;
	}
	
#if defined(_XBOX)
	if (S_XboxMusicNameIsMP3(name))
	{
		void *mp3Data = NULL;
		int mp3Len = len;
		if (mp3Len > 0)
		{
			mp3Data = HeapAlloc(GetProcessHeap(), 0, mp3Len);
		}
		if (mp3Len <= 0 || !mp3Data)
		{
			FS_FCloseFile(handle);
			Com_Printf(S_COLOR_YELLOW "WARNING: couldn't allocate MP3 metadata buffer for %s (%d bytes)\n",
				name, mp3Len);
			S_StopBackgroundTrack_Actual(pMusicInfo);
			return qfalse;
		}

		int mp3BytesRead = FS_Read(mp3Data, mp3Len, handle);
		FS_FCloseFile(handle);
		if (mp3BytesRead != mp3Len)
		{
			Com_Printf(S_COLOR_YELLOW "WARNING: short MP3 metadata read for %s (%d/%d bytes)\n",
				name, mp3BytesRead, mp3Len);
			HeapFree(GetProcessHeap(), 0, mp3Data);
			S_StopBackgroundTrack_Actual(pMusicInfo);
			return qfalse;
		}

		int unpackedBytes = 0;
		int rate = 0;
		int widthBytes = 0;
		int channels = 0;
		WaitForSingleObject(Sys_FileStreamMutex, INFINITE);
		char *headerError = C_MP3_GetHeaderData(mp3Data, mp3Len, &rate, &widthBytes, &channels, qtrue);
		char *sizeError = C_MP3_GetUnpackedSize(mp3Data, mp3Len, &unpackedBytes, qtrue);
		ReleaseMutex(Sys_FileStreamMutex);
		HeapFree(GetProcessHeap(), 0, mp3Data);

		if (headerError || sizeError || unpackedBytes <= 0 || rate <= 0 || widthBytes <= 0 || channels <= 0)
		{
			Com_Printf(S_COLOR_YELLOW "WARNING: Invalid MP3 music %s header='%s' size='%s'\n",
				name, headerError ? headerError : "<ok>", sizeError ? sizeError : "<ok>");
			S_StopBackgroundTrack_Actual(pMusicInfo);
			return qfalse;
		}

		pMusicInfo->s_backgroundSize = unpackedBytes;
		pMusicInfo->s_backgroundBPS = rate * widthBytes * channels;
		pMusicInfo->iFileCode = Sys_GetSoundFileCode(name);

		static int s_xboxMusicLoadLogCount = 0;
		if (s_xboxMusicLoadLogCount < 64)
		{
			Com_Printf("STEFX: Xbox MP3 music loaded '%s' compressed=%d pcm=%d rate=%d width=%d channels=%d code=0x%x metadata=process_heap\n",
				name, mp3Len, unpackedBytes, rate, widthBytes, channels, pMusicInfo->iFileCode);
			s_xboxMusicLoadLogCount++;
		}
	}
	else
#endif
#if defined(_XBOX) || defined(_WINDOWS)
	{
	// read enough of the file to get the header...
	byte buffer[4096];
	memset(buffer, 0, sizeof(buffer));
	FS_Read(buffer, sizeof(buffer), handle);
	FS_FCloseFile( handle );

	wavinfo_t info = GetWavInfo(buffer);
	if ( info.size == 0 ) {
		Com_Printf(S_COLOR_YELLOW "WARNING: Invalid format in music file %s\n", name);
		S_StopBackgroundTrack_Actual( pMusicInfo );
		return qfalse;
	}
	
	pMusicInfo->s_backgroundSize = info.size;
	if (info.waveFormatTag == WAVE_FORMAT_XBOX_ADPCM)
	{
		pMusicInfo->s_backgroundBPS = info.byteRate > 0 ? info.byteRate : ((info.rate * info.channels) / 2);
	}
	else
	{
		pMusicInfo->s_backgroundBPS = info.rate * info.width / 8;
	}
	if (info.waveFormatTag != WAVE_FORMAT_XBOX_ADPCM && info.format == AL_FORMAT_STEREO4)
	{
		pMusicInfo->s_backgroundBPS <<= 1;
	}
	pMusicInfo->iFileCode = Sys_GetFileCode(name);
	Com_Printf("STEFX: Xbox WAV music loaded '%s' data=%d bps=%d rate=%d width=%d format=0x%x tag=0x%x byteRate=%d code=0x%x\n",
		name,
		info.size,
		pMusicInfo->s_backgroundBPS,
		info.rate,
		info.width,
		info.format,
		info.waveFormatTag,
		info.byteRate,
		pMusicInfo->iFileCode);
	}
#elif defined(_GAMECUBE)
	FS_FCloseFile( handle );
	pMusicInfo->s_backgroundSize = len;
	pMusicInfo->s_backgroundBPS = 48000 * 4 / 8 * 2;
	pMusicInfo->iFileCode = Sys_GetFileCode(name);
#endif
	
	Q_strncpyz(pMusicInfo->sLoadedDataName, intro, sizeof(pMusicInfo->sLoadedDataName));
	pMusicInfo->bLoaded = true;
	Com_Printf("STEFX: Xbox music loaded state name='%s' loadedName='%s' code=0x%x loop=%d size=%d bps=%d\n",
		name ? name : "<null>",
		pMusicInfo->sLoadedDataName,
		pMusicInfo->iFileCode,
		pMusicInfo->bLooping ? 1 : 0,
		pMusicInfo->s_backgroundSize,
		pMusicInfo->s_backgroundBPS);
	
	return qtrue;
}

static void S_SwitchDynamicTracks( MusicState_e eOldState, MusicState_e eNewState, qboolean bNewTrackStartsFullVolume )
{
	// copy old track into fader...
	//
	tMusic_Info[ eBGRNDTRACK_FADE ] = tMusic_Info[ eOldState ];
	tMusic_Info[ eBGRNDTRACK_FADE ].iXFadeVolumeSeekTime= Sys_Milliseconds();
	tMusic_Info[ eBGRNDTRACK_FADE ].iXFadeVolumeSeekTo	= 0;
	//
	// ... and deactivate...
	//
	tMusic_Info[ eOldState ].bActive = qfalse;
	//
	// set new track to either full volume or fade up...
	//
	tMusic_Info[eNewState].bActive				= qtrue;
	tMusic_Info[eNewState].iXFadeVolumeSeekTime	= Sys_Milliseconds();
	tMusic_Info[eNewState].iXFadeVolumeSeekTo	= 255;
	tMusic_Info[eNewState].iXFadeVolume			= bNewTrackStartsFullVolume ? 255 : 0;
	
	eMusic_StateActual = eNewState;

	// sanity check
	if (tMusic_Info[eNewState].iFileSeekTo >= tMusic_Info[eNewState].s_backgroundSize)
	{
		tMusic_Info[eNewState].iFileSeekTo = 0;
	}
}

// called by both the config-string parser and the console-command state-changer...
//
// This either changes the music right now (copying track structures etc), or leaves the new state as pending
//	so it gets picked up by the general music player if in a transition that can't be overridden...
//
static void S_SetDynamicMusicState( MusicState_e eNewState )
{
	if (eMusic_StateRequest != eNewState)
	{
		eMusic_StateRequest  = eNewState;

		if (s_debugdynamic->integer)
		{
			LPCSTR	psNewStateString = Music_BaseStateToString( eNewState, qtrue );
					psNewStateString = psNewStateString?psNewStateString:"<unknown>";

			Com_Printf( S_COLOR_MAGENTA "S_SetDynamicMusicState( Request: \"%s\" )\n", psNewStateString );
		}		
	}
}


static void S_HandleDynamicMusicStateChange( void )
{
	if (eMusic_StateRequest != eMusic_StateActual)
	{
		// check whether or not the new request can be honoured, given what's currently playing...
		//
		if (Music_StateCanBeInterrupted( eMusic_StateActual, eMusic_StateRequest ))
		{
			switch (eMusic_StateRequest)
			{
				case eBGRNDTRACK_EXPLORE:	// ... from action or silence
				{
					switch (eMusic_StateActual)
					{
						case eBGRNDTRACK_ACTION:	// action->explore
						{
							// find the transition track to play, and the entry point for explore when we get there,
							//	and also see if we're at a permitted exit point to switch at all...
							//
							float fPlayingTimeElapsed = tMusic_Info[ eMusic_StateActual ].ElapsedTime();

							// supply:
							//
							// playing point in float seconds
							// enum of track being queried
							//
							// get:
							//
							// enum of transition track to switch to
							// float time of entry point of new track *after* transition

							MusicState_e	eTransition;
							float			fNewTrackEntryTime = 0.0f;
							if (Music_AllowedToTransition( fPlayingTimeElapsed, eBGRNDTRACK_ACTION, &eTransition, &fNewTrackEntryTime))
							{
								tMusic_Info[eTransition].Rewind();
								tMusic_Info[eTransition].bTrackSwitchPending	= qtrue;
								tMusic_Info[eTransition].bLooping				= qfalse;
								tMusic_Info[eTransition].eTS_NewState			= eMusic_StateRequest;
								tMusic_Info[eTransition].fTS_NewTime			= fNewTrackEntryTime;

								S_SwitchDynamicTracks( eMusic_StateActual, eTransition, qfalse );	// qboolean bNewTrackStartsFullVolume
							}
						}
						break;						

						case eBGRNDTRACK_SILENCE:	// silence->explore
						{
							tMusic_Info[ eMusic_StateRequest ].Rewind();
							S_SwitchDynamicTracks( eMusic_StateActual, eMusic_StateRequest, qfalse );	// qboolean bNewTrackStartsFullVolume
						}
						break;

						default:	// trying to transition from some state I wasn't aware you could transition from (shouldn't happen), so ignore
						{
							assert(0); 
							S_SwitchDynamicTracks( eMusic_StateActual, eBGRNDTRACK_SILENCE, qfalse );	// qboolean bNewTrackStartsFullVolume
						}
						break;
					}
				}
				break;

				case eBGRNDTRACK_SILENCE:	// from explore or action
				{
					switch (eMusic_StateActual)
					{
						case eBGRNDTRACK_ACTION:	// action->silence
						case eBGRNDTRACK_EXPLORE:	// explore->silence
						{
							// find the transition track to play, and the entry point for explore when we get there,
							//	and also see if we're at a permitted exit point to switch at all...
							//
							float fPlayingTimeElapsed = tMusic_Info[ eMusic_StateActual ].ElapsedTime();

							MusicState_e	eTransition;
							float			fNewTrackEntryTime = 0.0f;
							if (Music_AllowedToTransition( fPlayingTimeElapsed, eMusic_StateActual, &eTransition, &fNewTrackEntryTime))
							{
								tMusic_Info[eTransition].Rewind();
								tMusic_Info[eTransition].bTrackSwitchPending	= qtrue;
								tMusic_Info[eTransition].bLooping				= qfalse;
								tMusic_Info[eTransition].eTS_NewState			= eMusic_StateRequest;
								tMusic_Info[eTransition].fTS_NewTime			= 0.0f;	//fNewTrackEntryTime;  irrelevant when switching to silence

								S_SwitchDynamicTracks( eMusic_StateActual, eTransition, qfalse );	// qboolean bNewTrackStartsFullVolume
							}
						}
						break;

						default:		// some unhandled type switching to silence
							assert(0);	// fall through since boss case just does silence->switch anyway

						case eBGRNDTRACK_BOSS:	// boss->silence
						{
							tMusic_Info[eBGRNDTRACK_SILENCE].Rewind();
							S_SwitchDynamicTracks( eMusic_StateActual, eBGRNDTRACK_SILENCE, qfalse );	// qboolean bNewTrackStartsFullVolume
						}
						break;
					}
				}
				break;

				case eBGRNDTRACK_ACTION:	// anything->action
				{
					switch (eMusic_StateActual)
					{
						case eBGRNDTRACK_SILENCE:	// silence->action
						{
							tMusic_Info[ eMusic_StateRequest ].Rewind();
							S_SwitchDynamicTracks( eMusic_StateActual, eMusic_StateRequest, qfalse );	// qboolean bNewTrackStartsFullVolume
						}
						break;

						default:	// !silence->action
						{
							float fEntryTime = Music_GetRandomEntryTime( eMusic_StateRequest );
							tMusic_Info[ eMusic_StateRequest ].SeekTo(fEntryTime);
							S_SwitchDynamicTracks( eMusic_StateActual, eMusic_StateRequest, qfalse );	// qboolean bNewTrackStartsFullVolume
						}
						break;
					}
				}
				break;

				case eBGRNDTRACK_BOSS:
				{	
					tMusic_Info[eMusic_StateRequest].Rewind();
					S_SwitchDynamicTracks( eMusic_StateActual, eMusic_StateRequest, qfalse );	// qboolean bNewTrackStartsFullVolume
				}
				break;

				case eBGRNDTRACK_DEATH:
				{	
					tMusic_Info[eMusic_StateRequest].Rewind();
					S_SwitchDynamicTracks( eMusic_StateActual, eMusic_StateRequest, qtrue );	// qboolean bNewTrackStartsFullVolume
				}
				break;

				default: assert(0); break;	// unknown new mode request, so just ignore it
			}
		}
	}
}



static char gsIntroMusic[MAX_QPATH]={0};
static char gsLoopMusic [MAX_QPATH]={0};

void S_RestartMusic( void ) 
{
	if (s_soundStarted && !s_soundMuted )
	{
		S_StartBackgroundTrack( gsIntroMusic, gsLoopMusic, qfalse );	// ( default music start will set the state to EXPLORE )
		S_SetDynamicMusicState( eMusic_StateRequest );					// restore to prev state
	}
}


// Basic logic here is to see if the intro file specified actually exists, and if so, then it's not dynamic music,
//	When called by the cgame start it loads up, then stops the playback (because of stutter issues), so that when the
//	actual snapshot is received and the real play request is processed the data has already been loaded so will be quicker.
//
void S_StartBackgroundTrack( const char *intro, const char *loop, qboolean bCalledByCGameStart )
{
	bMusic_IsDynamic = qfalse;

	if (!s_soundStarted)
	{	//we have no sound, so don't even bother trying
		Com_Printf("STEFX: S_StartBackgroundTrack skipped: sound not started intro='%s' loop='%s'\n",
			intro ? intro : "<null>",
			loop ? loop : "<null>");
		return;
	}

	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}

	Q_strncpyz(gsIntroMusic,intro, sizeof(gsIntroMusic));
	Q_strncpyz(gsLoopMusic, loop,  sizeof(gsLoopMusic));

	char sName[MAX_QPATH];
	Q_strncpyz(sName,intro,sizeof(sName));

#ifdef _XBOX
	COM_DefaultExtension( sName, sizeof( sName ), ".wav" );
#else
	COM_DefaultExtension( sName, sizeof( sName ), ".wxb" );
#endif

	Com_Printf("STEFX: S_StartBackgroundTrack intro='%s' loop='%s' default='%s' cgameStart=%d allowDynamic=%d dynAvail=%d\n",
		intro,
		loop,
		sName,
		bCalledByCGameStart ? 1 : 0,
		s_allowDynamicMusic ? s_allowDynamicMusic->integer : -1,
		Music_DynamicDataAvailable(intro) ? 1 : 0);

	// if dynamic music not allowed, then just stream the explore music instead of playing dynamic...
	//
	if (!s_allowDynamicMusic->integer && Music_DynamicDataAvailable(intro))	// "intro", NOT "sName" (i.e. don't use version with ".wxb" extension)
	{
		LPCSTR psMusicName = Music_GetFileNameForState( eBGRNDTRACK_DATABEGIN );
		if (psMusicName && S_MusicFileExists( psMusicName ))
		{
			Q_strncpyz(sName,psMusicName,sizeof(sName));
		}
	}

	// conceptually we always play the 'intro'[/sName] track, intro-to-loop transition is handled in UpdateBackGroundTrack().
	//
	if ( (strstr(sName,"/") && S_MusicFileExists( sName )) )	// strstr() check avoids extra file-exists check at runtime if reverting from streamed music to dynamic since literal files all need at least one slash in their name (eg "music/blah")
	{
		Com_DPrintf("S_StartBackgroundTrack: Found/using non-dynamic music track '%s'\n", sName);
		tMusic_Info[eBGRNDTRACK_NONDYNAMIC].bLooping = qtrue;
		S_StartBackgroundTrack_Actual( &tMusic_Info[eBGRNDTRACK_NONDYNAMIC], bMusic_IsDynamic, sName, sName );
	}
	else
	{
		if (Music_DynamicDataAvailable(intro))	// "intro", NOT "sName" (i.e. don't use version with ".wxb" extension)
		{
			int i;
			extern const char *Music_GetLevelSetName(void);
			Q_strncpyz(sInfoOnly_CurrentDynamicMusicSet, Music_GetLevelSetName(), sizeof(sInfoOnly_CurrentDynamicMusicSet));
			for (i = eBGRNDTRACK_DATABEGIN; i != eBGRNDTRACK_DATAEND; i++)
			{
				qboolean bOk = qfalse;
				LPCSTR psMusicName = Music_GetFileNameForState( (MusicState_e) i);
				if (psMusicName && (!Q_stricmp(tMusic_Info[i].sLoadedDataName, psMusicName) || S_MusicFileExists( psMusicName )) )
				{
					bOk = S_StartBackgroundTrack_Actual( &tMusic_Info[i], qtrue, psMusicName, loop );
				}
				
				tMusic_Info[i].bExists = bOk;

				if (!tMusic_Info[i].bExists)
				{
					FreeMusic( &tMusic_Info[i] );
				}
			}

			//
			// default all tracks to OFF first (and set any other vars)
			//
			for (i=0; i<eBGRNDTRACK_NUMBEROF; i++)
			{
				tMusic_Info[i].bActive				= qfalse;
				tMusic_Info[i].bTrackSwitchPending	= qfalse;
				tMusic_Info[i].bLooping				= qtrue;
				tMusic_Info[i].fSmoothedOutVolume	= 0.25f;
			}

			tMusic_Info[eBGRNDTRACK_DEATH].bLooping		= qfalse;

			if (tMusic_Info[eBGRNDTRACK_EXPLORE].bExists &&
				tMusic_Info[eBGRNDTRACK_ACTION ].bExists
				)
			{
				Com_DPrintf("S_StartBackgroundTrack: Found dynamic music tracks\n");
				bMusic_IsDynamic = qtrue;

				//
				// ... then start the default music state...
				//
				eMusic_StateActual = eMusic_StateRequest = eBGRNDTRACK_EXPLORE;

				MusicInfo_t *pMusicInfo = &tMusic_Info[ eMusic_StateActual ];

				pMusicInfo->bActive				= qtrue;
				pMusicInfo->iXFadeVolumeSeekTime= Sys_Milliseconds();
				pMusicInfo->iXFadeVolumeSeekTo	= 255;
				pMusicInfo->iXFadeVolume		= 0;			
			}
			else
			{
				Com_Printf( S_COLOR_RED "Dynamic music did not have both 'action' and 'explore' versions, inhibiting...\n");
				S_StopBackgroundTrack();
			}
		}
		else
		{
			if (sName[0]!='.')	// blank name with ".wxb" or whatever attached - no error print out
			{
				Com_Printf( S_COLOR_RED "Unable to find music \"%s\" as explicit track or dynamic music entry!\n",sName);
				S_StopBackgroundTrack();
			}
		}
	}	

	if (bCalledByCGameStart)
	{
		S_StopBackgroundTrack();
	}
}

void S_StopBackgroundTrack( void )
{
	for (int i=0; i<eBGRNDTRACK_NUMBEROF; i++)
	{
		S_StopBackgroundTrack_Actual( &tMusic_Info[i] );
	}
}



// qboolean return is true only if we're changing from a streamed intro to a dynamic loop...
//
static qboolean S_UpdateBackgroundTrack_Actual( MusicInfo_t *pMusicInfo, qboolean bFirstOrOnlyMusicTrack, float fDefaultVolume) 
{
	float fMasterVol = fDefaultVolume; // s_musicVolume->value;
	static int s_xboxMusicUpdateLogCount = 0;

	if (bMusic_IsDynamic)
	{
		// step xfade volume...
		//
		if ( pMusicInfo->iXFadeVolume != pMusicInfo->iXFadeVolumeSeekTo )
		{
			int iFadeMillisecondsElapsed = Sys_Milliseconds() - pMusicInfo->iXFadeVolumeSeekTime;

			if (iFadeMillisecondsElapsed > (fDYNAMIC_XFADE_SECONDS * 1000))
			{
				pMusicInfo->iXFadeVolume = pMusicInfo->iXFadeVolumeSeekTo;
			}
			else
			{
				pMusicInfo->iXFadeVolume = (int) (255.0f * ((float)iFadeMillisecondsElapsed/(fDYNAMIC_XFADE_SECONDS * 1000.0f)));
				if (pMusicInfo->iXFadeVolumeSeekTo == 0)	// bleurgh
					pMusicInfo->iXFadeVolume = 255 - pMusicInfo->iXFadeVolume;
			}
		}
		fMasterVol *= (float)((float)pMusicInfo->iXFadeVolume / 255.0f);
	}

	if ( pMusicInfo->bLoaded == false ) {
		return qfalse;
	}

	pMusicInfo->fSmoothedOutVolume = (pMusicInfo->fSmoothedOutVolume + fMasterVol)/2.0f;

	alStreamf(AL_GAIN, pMusicInfo->fSmoothedOutVolume);

	// don't bother playing anything if musicvolume is 0
	if ( pMusicInfo->fSmoothedOutVolume <= 0 ) {
		return qfalse;
	}

#ifdef _XBOX
	if (s_xboxMusicUpdateLogCount < 48)
	{
		ALint state = 0;
		alGetStreami(AL_SOURCE_STATE, &state);
		Com_Printf("STEFX: Xbox music update loadedName='%s' code=0x%x looping=%d state=%d vol=%g defaultVol=%g seek=%d play=%g total=%g\n",
			pMusicInfo->sLoadedDataName,
			pMusicInfo->iFileCode,
			pMusicInfo->bLooping ? 1 : 0,
			state,
			pMusicInfo->fSmoothedOutVolume,
			fDefaultVolume,
			pMusicInfo->iFileSeekTo,
			pMusicInfo->PlayTime(),
			pMusicInfo->TotalTime());
		s_xboxMusicUpdateLogCount++;
	}
#endif

	// start playing if necessary
	if ( pMusicInfo->bLooping )
	{
		ALint state;
		alGetStreami(AL_SOURCE_STATE, &state);
		if ( state != AL_PLAYING )
		{
			alStreamPlay(pMusicInfo->iFileSeekTo, 
				pMusicInfo->iFileCode, 
				pMusicInfo->bLooping);
		}
	}

	if ( pMusicInfo->PlayTime() >= pMusicInfo->TotalTime() ) 
	{
		// loop the music, or play the next piece if we were on the intro...
		//	(but not for dynamic, that can only be used for loop music)
		//
		if (bMusic_IsDynamic)	// needs special logic for this, different call
		{
			pMusicInfo->Rewind();
		}
		else
		{
			// for non-dynamic music we need to check if "sMusic_BackgroundLoop" is an actual filename,
			//	or if it's a dynamic music specifier (which can't literally exist), in which case it should set
			//	a return flag then exit...
			//
			char sTestName[MAX_QPATH*2];// *2 so COM_DefaultExtension doesn't do an ERR_DROP if there was no space
			//	for an extension, since this is a "soft" test				
			Q_strncpyz( sTestName, sMusic_BackgroundLoop, sizeof(sTestName));
#ifdef _XBOX
			COM_DefaultExtension(sTestName, sizeof(sTestName), ".wav");
#else
			COM_DefaultExtension(sTestName, sizeof(sTestName), ".mp3");
#endif
			
			if (S_MusicFileExists( sTestName ))
			{
				// Restart the music
				alStreamStop();
				alStreamPlay(pMusicInfo->iFileSeekTo, 
					pMusicInfo->iFileCode, 
					pMusicInfo->bLooping);
			}
			else
			{
				// proposed file doesn't exist, but this may be a dynamic track we're wanting to loop, 
				//	so exit with a special flag...
				//
				return qtrue;
			}
		}
	}

	return qfalse;
}


// used to be just for dynamic, but now even non-dynamic music has to know whether it should be silent or not...
//
static LPCSTR S_Music_GetRequestedState(void)
{
// This doesn't do anything in MP - just return NULL
#ifndef _JK2MP
	int iStringOffset = cl.gameState.stringOffsets[CS_DYNAMIC_MUSIC_STATE];
	if (iStringOffset)
	{
		LPCSTR psCommand = cl.gameState.stringData+iStringOffset; 

		return psCommand;
	}
#endif

	return NULL;
}


// scan the configstring to see if there's been a state-change requested...
// (note that even if the state doesn't change it still gets here, so do a same-state check for applying)
//
// then go on to do transition handling etc...
//
static void S_CheckDynamicMusicState(void)
{
	LPCSTR psCommand = S_Music_GetRequestedState();

	if (psCommand)
	{
		MusicState_e eNewState;

		if ( !Q_stricmpn( psCommand, "silence", 7) )
		{				
			eNewState = eBGRNDTRACK_SILENCE;
		}
		else if ( !Q_stricmpn( psCommand, "action", 6) )
		{
			eNewState = eBGRNDTRACK_ACTION;
		}
		else if ( !Q_stricmpn( psCommand, "boss", 4) )
		{
			// special case, boss music is optional and may not be defined...
			//
			if (tMusic_Info[ eBGRNDTRACK_BOSS ].bExists)
			{
				eNewState = eBGRNDTRACK_BOSS;
			}
			else
			{
				// ( leave it playing current track )
				//
				eNewState = eMusic_StateActual;
			}
		}
		else if ( !Q_stricmpn( psCommand, "death", 5) )
		{
			// special case, death music is optional and may not be defined...
			//
			if (tMusic_Info[ eBGRNDTRACK_DEATH ].bExists)
			{
				eNewState = eBGRNDTRACK_DEATH;
			}
			else
			{
				// ( leave it playing current track, typically either boss or action )
				//
				eNewState = eMusic_StateActual;
			}
		}
		else
		{
			// seems a reasonable default...
			//
			eNewState = eBGRNDTRACK_EXPLORE;
		}
		
		S_SetDynamicMusicState( eNewState );
	}

	S_HandleDynamicMusicStateChange();
}

static void S_UpdateBackgroundTrack( void )
{
	if (bMusic_IsDynamic)
	{
		S_CheckDynamicMusicState();
		
		if (eMusic_StateActual != eBGRNDTRACK_SILENCE)
		{
			MusicInfo_t *pMusicInfoCurrent = &tMusic_Info[ (eMusic_StateActual == eBGRNDTRACK_FADE)?eBGRNDTRACK_EXPLORE:eMusic_StateActual ];
			MusicInfo_t *pMusicInfoFadeOut = &tMusic_Info[ eBGRNDTRACK_FADE ];

			if ( pMusicInfoCurrent->bLoaded )
			{
				float fRemainingTimeInSeconds = 1000000;

				if (pMusicInfoFadeOut->bActive)
				{
					S_UpdateBackgroundTrack_Actual( pMusicInfoFadeOut, qfalse, s_music_volume->value );	// inactive-checked internally

					//
					// only do this for the fader!...
					//
					if (pMusicInfoFadeOut->iXFadeVolume == 0)
					{
						pMusicInfoFadeOut->bActive = qfalse;

						// play if we have a file
						if (pMusicInfoCurrent->iFileCode)
						{
							alStreamPlay(pMusicInfoCurrent->iFileSeekTo, 
								pMusicInfoCurrent->iFileCode,
								pMusicInfoCurrent->bLooping);
							
							pMusicInfoCurrent->iXFadeVolumeSeekTime = Sys_Milliseconds();
						}
						else
						{
							alStreamStop();
						}
					}
				}
				else
				{
					S_UpdateBackgroundTrack_Actual( pMusicInfoCurrent, qtrue, s_music_volume->value );
					fRemainingTimeInSeconds = pMusicInfoCurrent->TotalTime() - pMusicInfoCurrent->ElapsedTime();
				}
				
				if ( fRemainingTimeInSeconds < fDYNAMIC_XFADE_SECONDS*2 )
				{
					// now either loop current track, switch if finishing a transition, or stop if finished a death...
					//
					if (pMusicInfoCurrent->bTrackSwitchPending)
					{
						pMusicInfoCurrent->bTrackSwitchPending = qfalse;	// ack
						tMusic_Info[ pMusicInfoCurrent->eTS_NewState ].SeekTo(pMusicInfoCurrent->fTS_NewTime);
						S_SwitchDynamicTracks( eMusic_StateActual, pMusicInfoCurrent->eTS_NewState, qfalse);	// qboolean bNewTrackStartsFullVolume
					}
					else
					{
						// normal looping, so set rewind current track, set volume to 0 and fade up to full (unless death track playing, then stays quiet)
						//	(while fader copy of end-section fades down)
						//
						// copy current track to fader...
						//
						*pMusicInfoFadeOut = *pMusicInfoCurrent;	// struct copy
						pMusicInfoFadeOut->iXFadeVolumeSeekTime	= Sys_Milliseconds();
						pMusicInfoFadeOut->iXFadeVolumeSeekTo	= 0;
						//
						pMusicInfoCurrent->Rewind();
						pMusicInfoCurrent->iXFadeVolumeSeekTime	= Sys_Milliseconds();
						pMusicInfoCurrent->iXFadeVolumeSeekTo	= (eMusic_StateActual == eBGRNDTRACK_DEATH) ? 0: 255;
						pMusicInfoCurrent->iXFadeVolume			= 0;
					}
				}
			}
		}
		else
		{
			// special case, when foreground music is shut off but fader still running to fade off previous track...
			//
			MusicInfo_t *pMusicInfoFadeOut = &tMusic_Info[ eBGRNDTRACK_FADE ];
			if (pMusicInfoFadeOut->bActive)
			{
				S_UpdateBackgroundTrack_Actual( pMusicInfoFadeOut, qtrue, s_music_volume->value );
				if (pMusicInfoFadeOut->iXFadeVolume == 0)
				{
					pMusicInfoFadeOut->bActive = qfalse;
					alStreamStop();
				}
			}	
		}
	}
	else
	{
		// standard / non-dynamic one-track music...
		//
		LPCSTR psCommand = S_Music_GetRequestedState();	// special check just for "silence" case...
		qboolean bShouldBeSilent = (psCommand && !Q_stricmp(psCommand,"silence"));
		float fDesiredVolume = bShouldBeSilent ? 0.0f : s_music_volume->value;
		//
		// internal to this code is a volume-smoother...
		//
		qboolean bNewTrackDesired = S_UpdateBackgroundTrack_Actual(&tMusic_Info[eBGRNDTRACK_NONDYNAMIC], qtrue, fDesiredVolume);

		if (bNewTrackDesired)
		{
			S_StartBackgroundTrack( sMusic_BackgroundLoop, sMusic_BackgroundLoop, qfalse );
		}
	}
}

// Called from MusicFree in snd_music to prevent pending state changes from
// crashing after the level finishes loading, but before new music level data
// has been read. Not sure if this fixes the bug, but it seems good.
void S_AvertMusicDisaster(void)
{
	eMusic_StateRequest = eMusic_StateActual;
}


int SND_GetMemoryUsed(void)
{
	ALint used = 0;
	alGeti(AL_MEMORY_USED, &used);
	return used;
}

static int SND_GetSoundPoolLimitBytes(void)
{
	int megs = s_soundpoolmegs ? s_soundpoolmegs->integer : 6;

	if (megs <= 0)
	{
		megs = 6;
	}

	return megs * 1024 * 1024;
}

int SND_PrepareForSoundLoad(sfx_t *pIncoming, int incomingBytes)
{
	if (incomingBytes <= 0)
	{
		return 0;
	}

	const int before = SND_GetMemoryUsed();
	const int limit = SND_GetSoundPoolLimitBytes();
	int freed = 0;
	if (before + incomingBytes > limit)
	{
		freed = SND_FreeOldestSound(pIncoming);
	}

#ifdef _XBOX
	if (freed > 0 || before + incomingBytes > limit)
	{
		const char *name = pIncoming ? Sys_GetSoundFileCodeName(pIncoming->iFileCode) : NULL;
		XBLog_WriteCriticalf("STEFX_AUDIO_CACHE: reserve before=%d incoming=%d limit=%d freed=%d after=%d code=0x%x name='%s'",
			before, incomingBytes, limit, freed, SND_GetMemoryUsed(),
			pIncoming ? pIncoming->iFileCode : 0,
			name ? name : "<unknown>");
	}
#endif
	return freed;
}

void SND_update(sfx_t *sfx) 
{
	while ( SND_GetMemoryUsed() > SND_GetSoundPoolLimitBytes())
	{
		int iBytesFreed = SND_FreeOldestSound(sfx);
		if (iBytesFreed == 0)
			break;	// sanity
	}
}

// free any allocated sfx mem...
//
// now returns # bytes freed to help with z_malloc()-fail recovery
//
static int SND_FreeSFXMem(sfx_t *sfx)
{
	int iOrgMem = SND_GetMemoryUsed();
	int iZoneFreed = 0;

	alGetError();
	if (sfx->Buffer)
	{
		alDeleteBuffers(1, &(sfx->Buffer));
		sfx->Buffer = 0;
	}

	if (sfx->pSoundData)
	{
		Com_DPrintf("JA: SND_FreeSFXMem freeing raw sound copy bytes=%d\n", sfx->iSoundLength);
#ifdef _XBOX
		if (FS_STEFX_FreeHeapFileBuffer(sfx->pSoundData))
		{
			iZoneFreed += sfx->iSoundLength;
		}
		else
#endif
		{
			iZoneFreed += Z_Free(sfx->pSoundData);
		}
		sfx->pSoundData = NULL;
	}

	sfx->iFlags &= ~(SFX_FLAG_RESIDENT | SFX_FLAG_LOADING);
	sfx->iFlags |= SFX_FLAG_UNLOADED;

	return iZoneFreed + iOrgMem - SND_GetMemoryUsed();
}

void S_DisplayFreeMemory() 
{
}

void SND_TouchSFX(sfx_t *sfx)
{
	sfx->iLastTimeUsed		= Com_Milliseconds()+1;
	sfx->iLastLevelUsedOn	= (short)RE_RegisterMedia_GetLevel();
}

static qboolean SND_SFXInUseByChannel(sfx_t *sfx)
{
	for (int iChannel = 0; iChannel < s_numChannels; iChannel++)
	{
		if (s_channels[iChannel].thesfx == sfx)
		{
			return qtrue;
		}
	}

	return qfalse;
}

// currently this is only called during snd_shutdown or snd_restart
//
static void S_FreeAllSFXMem(void)
{
	for (int i = 0; i < MAX_SFX; ++i)
	{
		if (s_sfxCodes[i] != INVALID_CODE && i != s_defaultSound)
		{
			SND_FreeSFXMem(&s_sfxBlock[i]);
		}
	}
}

// returns number of bytes freed up...
//
// new param is so we can be usre of not freeing ourselves (without having to rely on possible uninitialised timers etc)
// Super new version just throws out ALL sound. Bwa ha ha!
int SND_FreeOldestSound(sfx_t *pButNotThisOne /* = NULL */) 
{	
	int iBytesFreed = 0;
	sfx_t *sfx;

	// start on 1 so we never dump the default sound...
	//

	Com_Printf(" Trashing sounds.\n");
	
	for (int i = 0; i < MAX_SFX; ++i)
	{
		if (s_sfxCodes[i] == INVALID_CODE || i == s_defaultSound) continue;

		sfx = &s_sfxBlock[i];

		if (sfx != pButNotThisOne)
		{
			// Don't throw out the default sound, or sounds that are not in memory
			//
			if (!(sfx->iFlags & SFX_FLAG_DEFAULT) && 
				(sfx->iFlags & SFX_FLAG_RESIDENT))
			{
				// new bit, we can't throw away any sfx_t struct in use by a channel, 
				// else the paint code will crash...
				//
				int iChannel;
				for (iChannel=0; iChannel<s_numChannels; iChannel++)
				{
					channel_t *ch = & s_channels[iChannel];

					if (ch->thesfx == sfx)
						break;	// damn, being used
				}
				if (iChannel == s_numChannels)
				{
					// this sfx_t struct wasn't used by any channels, so we can lose it...
					//			
					iBytesFreed += SND_FreeSFXMem( &s_sfxBlock[i] );
				}
			}
		}
	}

	return iBytesFreed;
}
int SND_FreeOldestSound(void)
{
	return SND_FreeOldestSound(NULL);	// I had to add a void-arg version of this because of link issues, sigh
}


// just before we drop into a level, ensure the audio pool is under whatever the maximum
//	pool size is (but not by dropping out sounds used by the current level)...
//
// returns qtrue if at least one sound was dropped out, so z_malloc-fail recovery code knows if anything changed
//
qboolean SND_RegisterAudio_Clean(void)
{
	if ( !s_soundStarted ) {
		return qfalse;
	}

	qboolean bAtLeastOneSoundDropped = qfalse;

	Com_DPrintf( "SND_RegisterAudio_Clean():\n");

	extern void S_DrainRawSoundData(void);
	S_DrainRawSoundData();

	{
		for (int i = 0;	i < MAX_SFX; ++i)
		{
			if (s_sfxCodes[i] == INVALID_CODE || i == s_defaultSound) continue;

			sfx_t *sfx = &s_sfxBlock[i];

			if (sfx->iFlags & (SFX_FLAG_RESIDENT | SFX_FLAG_DEMAND))
			{
				qboolean bDeleteThis = qtrue;
				//if (bDeleteThis)
				{
					int iChannel;
					for (iChannel=0; iChannel<s_numChannels; iChannel++)
					{
						if (s_channels[iChannel].thesfx == sfx)
						{
							bDeleteThis = false;
							break;
						}
					}
					
					if (bDeleteThis)
					{
						if (!(sfx->iFlags & SFX_FLAG_DEFAULT) &&
							(sfx->iFlags & SFX_FLAG_RESIDENT) && 
							SND_FreeSFXMem(sfx))
						{
							bAtLeastOneSoundDropped = qtrue;
						}
						if (sfx->iFlags & SFX_FLAG_DEMAND)
						{
							s_sfxCodes[i] = INVALID_CODE;
						}
					}
				}
			}
		}
	}

	Com_DPrintf( "SND_RegisterAudio_Clean(): Ok\n");	

	return bAtLeastOneSoundDropped;
}

qboolean SND_RegisterAudio_LevelLoadEnd(qboolean bDeleteEverythingNotUsedThisLevel /* 99% qfalse */)
{
	if ( !s_soundStarted ) {
		return qfalse;
	}

#ifdef _XBOX
	if (s_xboxSilentAudio)
	{
		return qfalse;
	}
#endif

	qboolean bAtLeastOneSoundDropped = qfalse;
	int iLoadedAudioBytes = SND_GetMemoryUsed();
	const int iMaxAudioBytes = SND_GetSoundPoolLimitBytes();
	const int iCurrentLevel = RE_RegisterMedia_GetLevel();
	int iFreedSounds = 0;
	int iFreedBytes = 0;
	int iSkippedChannels = 0;

	Com_DPrintf( "SND_RegisterAudio_LevelLoadEnd():\n");

	extern void S_DrainRawSoundData(void);
	S_DrainRawSoundData();

	for (int i = 0; i < MAX_SFX && (iLoadedAudioBytes > iMaxAudioBytes || bDeleteEverythingNotUsedThisLevel); ++i)
	{
		if (s_sfxCodes[i] == INVALID_CODE || i == s_defaultSound) continue;

		sfx_t *sfx = &s_sfxBlock[i];

		if ((sfx->iFlags & (SFX_FLAG_DEFAULT | SFX_FLAG_LOADING)) ||
			!(sfx->iFlags & SFX_FLAG_RESIDENT))
		{
			continue;
		}

		qboolean bDeleteThis = bDeleteEverythingNotUsedThisLevel ?
			(sfx->iLastLevelUsedOn != iCurrentLevel) :
			(sfx->iLastLevelUsedOn < iCurrentLevel);

		if (!bDeleteThis)
		{
			continue;
		}

		if (SND_SFXInUseByChannel(sfx))
		{
			iSkippedChannels++;
			continue;
		}

		const int iBytesFreed = SND_FreeSFXMem(sfx);
		if (iBytesFreed > 0)
		{
			iFreedBytes += iBytesFreed;
			iFreedSounds++;
			bAtLeastOneSoundDropped = qtrue;
		}

		if (sfx->iFlags & SFX_FLAG_DEMAND)
		{
			s_sfxCodes[i] = INVALID_CODE;
		}

		iLoadedAudioBytes = SND_GetMemoryUsed();
	}

#ifdef _XBOX
	if (iFreedSounds || iLoadedAudioBytes > iMaxAudioBytes || iSkippedChannels)
	{
		Com_Printf("STEFX: audio cache level-end level=%d loaded=%d cap=%d freed=%d freedBytes=%d skippedChannels=%d force=%d overBudget=%d\n",
			iCurrentLevel,
			iLoadedAudioBytes,
			iMaxAudioBytes,
			iFreedSounds,
			iFreedBytes,
			iSkippedChannels,
			bDeleteEverythingNotUsedThisLevel ? 1 : 0,
			(iLoadedAudioBytes > iMaxAudioBytes) ? 1 : 0);
	}
#endif

	Com_DPrintf( "SND_RegisterAudio_LevelLoadEnd(): Ok\n");

	return bAtLeastOneSoundDropped;
}

qboolean S_FileExists( const char *psFilename )
{
	// This is only really used for music. Need to swap .mp3 with .wxb on Xbox
	char *fixedName = S_FixMusicFileName(psFilename);

	// VVFIXME : This can be done better?
	fileHandle_t fhTemp;

	FS_FOpenFileRead (fixedName, &fhTemp, qtrue);	// qtrue so I can fclose the handle without closing a PAK
	if (!fhTemp) 
		return qfalse;
	
	FS_FCloseFile(fhTemp);
	return qtrue;
}

void S_Precache( const char *name )
{
	const sfxHandle_t handle = S_RegisterSound( name );

#ifdef _XBOX
	// Dialogue is intentionally demand-loaded.  Keeping the registration gives
	// ICARUS a stable handle and preserves lip-sync metadata, while deferring the
	// XBADPCM buffer until S_StartSound actually requests the line.
	// Loading every scripted line during map precache exhausts the 64 MiB Xbox
	// address space before late Borg player-model allocations can complete.
	if (handle >= 0 && handle <= MAX_SFX &&
		s_sfxCodes[handle] != INVALID_CODE &&
		(s_sfxBlock[handle].iFlags & SFX_FLAG_DEMAND))
	{
		static int s_xboxDemandPrecacheLogBudget = 64;
		if (s_xboxDemandPrecacheLogBudget > 0)
		{
			const sfx_t *sfx = &s_sfxBlock[handle];
			const char *soundName = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_PRECACHE: deferred demand handle=%d code=0x%x flags=0x%x name='%s'",
				handle, sfx->iFileCode, sfx->iFlags,
				soundName ? soundName : "<unknown>");
			--s_xboxDemandPrecacheLogBudget;
		}
		return;
	}
#endif

	S_LoadSound( handle );
}

const char	*basicSounds[] = 
{
	"death1.wav",
	"death2.wav",
	"death3.wav",
	"jump1.wav",
	"pain25.wav",
	"pain50.wav",
	"pain75.wav",
	"pain100.wav",
	"gurp1.wav",
	"gurp2.wav",
	"drown.wav",
	"gasp.wav",
	"land1.wav",
	"falling1.wav"
};

const int numBasicSounds = sizeof(basicSounds) / sizeof(basicSounds[0]);

void S_LoadCommonSounds( void )
{
#if defined(STEFX_SP_HOSTED_MP)
	Com_Printf("STEFX_HM_SP: campaign common-sound precache skipped; EF cgame owns match media\n");
	return;
#else
	int i;

	S_Precache( "sound/weapons/saber/saberon.wav" );
	S_Precache( "sound/weapons/saber/saberonquick.wav" );
	S_Precache( "sound/weapons/saber/saberoff.wav" );
	S_Precache( "sound/weapons/saber/saberoffquick.wav" );
	S_Precache( "sound/weapons/saber/saberspinoff.wav" );

	for ( i = 1; i < 4; i++ )
		S_Precache( va("sound/weapons/saber/saberhit%d.wav", i) );
	for ( i = 1; i < 4; i++ )
		S_Precache( va("sound/weapons/saber/saberhitwall%d.wav", i) );
	for ( i = 1; i < 10; i++ )
		S_Precache( va("sound/weapons/saber/saberhup%d.wav", i) );

	S_Precache( "sound/weapons/saber/saber_catch.wav" );

	for ( i = 1; i < 4; i++ )
		S_Precache( va("sound/weapons/saber/saberbounce%d.wav", i) );
	for ( i = 1; i < 10; i++ )
		S_Precache( va("sound/weapons/saber/saberblock%d.wav", i) );

	// Which player sounds do we need?
	char *jaden;
	extern cvar_t *g_sex;
	if( g_sex->string[0] == 'f' || g_sex->string[0] == 'F' )
		jaden = "jaden_fmle";
	else
		jaden = "jaden_male";

	for( i = 0; i < numBasicSounds; ++i )
		S_Precache( va("sounds/chars/%s/misc/%s", jaden, basicSounds[i]) );
#endif
}
