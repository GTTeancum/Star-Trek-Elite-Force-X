// snd_mem.c: sound caching


// leave this as first line for PCH reasons...
//
// #include "../server/exe_headers.h"

#include "snd_local_console.h"

#ifdef _XBOX
#include <xtl.h>
#include "../win32/xb_log.h"
extern HANDLE Sys_FileStreamMutex;
extern void S_XboxOnSoundLoaded(sfx_t *sfx);
extern void QAL_XboxSetBufferDebugName(ALuint buffer, const char *name);
extern void *FS_STEFX_AllocExternalSoundBuffer(int len);
extern qboolean FS_STEFX_FreeHeapFileBuffer(void *buffer);
extern "C" volatile unsigned int g_SPXBAudioLoadStage;
extern "C" volatile unsigned int g_SPXBAudioLoadIndex;
extern "C" volatile unsigned int g_SPXBAudioLoadHandle;
#endif

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

#ifndef WAVE_FORMAT_XBOX_ADPCM
#define WAVE_FORMAT_XBOX_ADPCM 0x0069
#endif

#define SND_MAX_LOADS 48
static sfx_t** s_LoadList = NULL;
static int s_LoadListSize = 0;
qboolean gbInsideLoadSound = qfalse;	// Needed to link VVFIXME

extern int Sys_GetFileCode(const char *name);

static void S_FreeRawSoundData(void *data)
{
	if (!data)
	{
		return;
	}
#ifdef _XBOX
	if (FS_STEFX_FreeHeapFileBuffer(data))
	{
		return;
	}
#endif
	Z_Free(data);
}

//Drain sound main memory into ARAM.
void S_DrainRawSoundData(void)
{
	extern int s_soundStarted;
	if (!s_soundStarted) return;

	do
	{
		S_UpdateLoading();

#ifdef _GAMECUBE
		extern void ERR_DiscFail(bool);
		ERR_DiscFail(true);
#endif
	} 
	while (s_LoadListSize);
}



/*
============
GetWavInfo
============
*/
wavinfo_t GetWavInfo(byte *data)
{
	wavinfo_t info;
	memset(&info, 0, sizeof(wavinfo_t));

	if (!data) return info;

#ifdef _GAMECUBE
	if (*(short*)&data[14] != 0)
	{
		// invalid type, abort
		return info;
	}

	info.format = AL_FORMAT_MONO4;
	info.width = 4;
	info.size = ((*(int*)&data[20]) >> 1) + 96;
	info.rate = *(int*)&data[8];
#else
	if (strncmp((char *)&data[0], "RIFF", 4) ||
		strncmp((char *)&data[8], "WAVE", 4))
	{
		// invalid type, abort
		return info;
	}

	int dataofs = 12; // done with riff chunk
	int fmtFound = 0;
	int dataFound = 0;
	unsigned short formatTag = 0;
	unsigned short channels = 0;
	unsigned int rate = 0;
	unsigned int byteRate = 0;
	unsigned short bits = 0;
	unsigned int dataSize = 0;
	unsigned int dataStart = 0;

	while (dataofs > 0 && dataofs < 4096)
	{
		char chunkName[5];
		memcpy(chunkName, &data[dataofs], 4);
		chunkName[4] = 0;
		unsigned int chunkSize = *(unsigned int *)&data[dataofs + 4];
		byte *chunkData = &data[dataofs + 8];

		if (!strncmp(chunkName, "fmt ", 4))
		{
			formatTag = *(unsigned short *)&chunkData[0];
			channels = *(unsigned short *)&chunkData[2];
			rate = *(unsigned int *)&chunkData[4];
			byteRate = *(unsigned int *)&chunkData[8];
			bits = *(unsigned short *)&chunkData[14];
			fmtFound = 1;
		}
		else if (!strncmp(chunkName, "data", 4))
		{
			dataSize = chunkSize;
			dataStart = dataofs + 8;
			dataFound = 1;
		}

		if (fmtFound && dataFound)
		{
			break;
		}

		dataofs += 8 + ((chunkSize + 1) & ~1);
	}

	if (!fmtFound || !dataFound || !channels || !rate || !bits)
	{
		return info;
	}

	info.rate = rate;
	info.width = bits;
	info.channels = channels;
	info.waveFormatTag = formatTag;
	info.dataofs = dataStart;
	info.size = dataSize;
	info.byteRate = byteRate;

	if (formatTag == WAVE_FORMAT_PCM)
	{
		if (channels == 1 && bits == 8) info.format = AL_FORMAT_MONO8;
		else if (channels == 1 && bits == 16) info.format = AL_FORMAT_MONO16;
		else if (channels == 2 && bits == 8) info.format = AL_FORMAT_STEREO8;
		else if (channels == 2 && bits == 16) info.format = AL_FORMAT_STEREO16;
		else
		{
			memset(&info, 0, sizeof(info));
			return info;
		}

		info.samples = dataSize / (channels * (bits / 8));
	}
	else if (formatTag == WAVE_FORMAT_XBOX_ADPCM)
	{
		info.format = channels == 1 ? AL_FORMAT_MONO4 : AL_FORMAT_STEREO4;
		info.samples = dataSize * 2;
	}
	else
	{
		memset(&info, 0, sizeof(info));
		return info;
	}

	static int s_wavInfoLogCount = 0;
	if (s_wavInfoLogCount < 32)
	{
		Com_PrintfAlways("STEFX: WAV info tag=0x%x channels=%d bits=%d rate=%d byteRate=%d size=%d dataofs=%d format=0x%x\n",
			formatTag, channels, bits, rate, byteRate, dataSize, dataStart, info.format);
		s_wavInfoLogCount++;
	}
#endif

	return info;
}

// adjust filename for foreign languages and WAV/MP3 issues. 
//
unsigned int Sys_GetSoundFileCode(const char* name);
int Sys_GetSoundFileCodeSize(unsigned int filecode);
const char *Sys_GetSoundFileCodeName(unsigned int code);

extern "C"
{
char* C_MP3_GetUnpackedSize(void *pvData, int iDataLen, int *piUnpackedSize, int bStereoDesired);
char* C_MP3_UnpackRawPCM(void *pvData, int iDataLen, int *piUnpackedSize, void *pbUnpackBuffer, int bStereoDesired);
char* C_MP3_GetHeaderData(void *pvData, int iDataLen, int *piRate, int *piWidth, int *piChannels, int bStereoDesired);
}

static qboolean S_DecodeMP3Sound(sfx_t *sfx, byte *sourceData, int sourceBytes, wavinfo_t *info)
{
	int unpackedBytes = 0;
	int actualBytes = 0;
	int rate = 0;
	int widthBytes = 0;
	int channels = 0;
	char *error;
	const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
	if (!name)
	{
		name = "<unknown>";
	}

#ifdef _XBOX
	WaitForSingleObject(Sys_FileStreamMutex, INFINITE);
#endif
	error = C_MP3_GetUnpackedSize(sourceData, sourceBytes, &unpackedBytes, qfalse);
	if (error || unpackedBytes <= 0)
	{
#ifdef _XBOX
		ReleaseMutex(Sys_FileStreamMutex);
#endif
		static int s_mp3SizeErrorLogCount = 0;
		if (s_mp3SizeErrorLogCount < 64)
		{
			Com_PrintfAlways("STEFX: MP3 size failed '%s' bytes=%d err='%s'\n",
				name, sourceBytes, error ? error : "<none>");
			s_mp3SizeErrorLogCount++;
		}
		return qfalse;
	}

	error = C_MP3_GetHeaderData(sourceData, sourceBytes, &rate, &widthBytes, &channels, qfalse);
	if (error || rate <= 0 || widthBytes <= 0 || channels <= 0)
	{
#ifdef _XBOX
		ReleaseMutex(Sys_FileStreamMutex);
#endif
		static int s_mp3HeaderErrorLogCount = 0;
		if (s_mp3HeaderErrorLogCount < 64)
		{
			Com_PrintfAlways("STEFX: MP3 header failed '%s' bytes=%d err='%s'\n",
				name, sourceBytes, error ? error : "<none>");
			s_mp3HeaderErrorLogCount++;
		}
		return qfalse;
	}
#ifdef _XBOX
	ReleaseMutex(Sys_FileStreamMutex);
#endif

	byte *decoded = (byte *)Z_Malloc(unpackedBytes + 2304, TAG_SND_RAWDATA, qtrue, 32);
	if (!decoded)
	{
		return qfalse;
	}

	actualBytes = unpackedBytes;
#ifdef _XBOX
	WaitForSingleObject(Sys_FileStreamMutex, INFINITE);
#endif
	error = C_MP3_UnpackRawPCM(sourceData, sourceBytes, &actualBytes, decoded, qfalse);
#ifdef _XBOX
	ReleaseMutex(Sys_FileStreamMutex);
#endif
	if (error || actualBytes <= 0)
	{
		static int s_mp3DecodeErrorLogCount = 0;
		if (s_mp3DecodeErrorLogCount < 64)
		{
			Com_PrintfAlways("STEFX: MP3 decode failed '%s' bytes=%d err='%s'\n",
				name, sourceBytes, error ? error : "<none>");
			s_mp3DecodeErrorLogCount++;
		}
		Z_Free(decoded);
		return qfalse;
	}

	memset(info, 0, sizeof(*info));
	info->rate = rate;
	info->width = widthBytes * 8;
	info->channels = channels;
	info->waveFormatTag = WAVE_FORMAT_PCM;
	info->dataofs = 0;
	info->size = actualBytes;
	info->samples = actualBytes / (channels * widthBytes);
	if (channels == 1 && widthBytes == 1) info->format = AL_FORMAT_MONO8;
	else if (channels == 1 && widthBytes == 2) info->format = AL_FORMAT_MONO16;
	else if (channels == 2 && widthBytes == 1) info->format = AL_FORMAT_STEREO8;
	else if (channels == 2 && widthBytes == 2) info->format = AL_FORMAT_STEREO16;
	else
	{
		Z_Free(decoded);
		memset(info, 0, sizeof(*info));
		return qfalse;
	}

	S_FreeRawSoundData(sfx->pSoundData);
	sfx->pSoundData = decoded;

	static int s_mp3DecodeLogCount = 0;
	if (s_mp3DecodeLogCount < 128)
	{
		Com_PrintfAlways("STEFX: MP3 decoded '%s' src=%d pcm=%d rate=%d width=%d channels=%d format=0x%x\n",
			name, sourceBytes, actualBytes, rate, widthBytes, channels, info->format);
		s_mp3DecodeLogCount++;
	}

	return qtrue;
}

static int S_TrySoundFileCode(const char *name, const char *ext)
{
	char tryName[MAX_QPATH];
	Q_strncpyz(tryName, name, sizeof(tryName));

	int len = strlen(tryName);
	if (len < 4)
	{
		return -1;
	}

	tryName[len - 3] = ext[0];
	tryName[len - 2] = ext[1];
	tryName[len - 1] = ext[2];
	for (int i = 0; i < len; i++)
	{
		if (tryName[i] == '\\')
		{
			tryName[i] = '/';
		}
	}

	unsigned int code = Sys_GetSoundFileCode(tryName);
	if (Sys_GetSoundFileCodeSize(code) == -1)
	{
		return -1;
	}

	static int s_soundResolveLogCount = 0;
	if (s_soundResolveLogCount < 96)
	{
		Com_PrintfAlways("STEFX: sound resolved '%s' code=0x%x size=%d\n",
			tryName, code, Sys_GetSoundFileCodeSize(code));
		s_soundResolveLogCount++;
	}

	return code;
}

static int S_LoadSound_FileNameAdjuster(char *psFilename)
{
	char tryName[MAX_QPATH];
	char englishName[MAX_QPATH];
	const char *originalExt;
	int code;

	Q_strncpyz(tryName, psFilename, sizeof(tryName));
	Q_strlwr(tryName);

	int len = strlen(tryName);
	if (len < 4)
	{
		return -1;
	}

	originalExt = tryName + len - 3;

	char *psVoice = strstr(tryName,"chars");
	if (psVoice)
	{
		// account for foreign voices...
		//		
		extern DWORD g_dwLanguage;
		if (g_dwLanguage == XC_LANGUAGE_GERMAN)
			strncpy(psVoice, "chr_d", 5);	// Same number of letters as "chars"
		else if (g_dwLanguage == XC_LANGUAGE_FRENCH)
			strncpy(psVoice, "chr_f", 5);	// Same number of letters as "chars"
		else
			psVoice = NULL;					// Flag that we didn't substitute
	}

	Q_strncpyz(englishName, tryName, sizeof(englishName));

	code = S_TrySoundFileCode(tryName, "wxb");
	if (code != -1) return code;

#ifdef _XBOX
	code = S_TrySoundFileCode(tryName, "wav");
	if (code != -1) return code;
#endif

	if (Q_stricmp(originalExt, "wxb"))
	{
		code = S_TrySoundFileCode(tryName, originalExt);
		if (code != -1) return code;
	}

#ifndef _XBOX
	code = S_TrySoundFileCode(tryName, "wav");
	if (code != -1) return code;
#endif

	code = S_TrySoundFileCode(tryName, "mp3");
	if (code != -1) return code;

	if (psVoice)
	{
		//hmmm, not found, ok, maybe we were trying a foreign noise ("arghhhhh.mp3" that doesn't matter?) but it
		// was missing?   Can't tell really, since both types are now in sound/chars. Oh well, fall back to English for now...
		char *englishVoice = strstr(englishName, "chr_");
		if (englishVoice)
		{
			// yep, so fallback to re-try the english...
			//
			strncpy(englishVoice, "chars", 5);

			code = S_TrySoundFileCode(englishName, "wxb");
			if (code != -1) return code;

#ifdef _XBOX
			code = S_TrySoundFileCode(englishName, "wav");
			if (code != -1) return code;
#endif

			if (Q_stricmp(originalExt, "wxb"))
			{
				code = S_TrySoundFileCode(englishName, originalExt);
				if (code != -1) return code;
			}

#ifndef _XBOX
			code = S_TrySoundFileCode(englishName, "wav");
			if (code != -1) return code;
#endif

			code = S_TrySoundFileCode(englishName, "mp3");
			if (code != -1) return code;
		}
	}

	static int s_soundMissingLogCount = 0;
	if (s_soundMissingLogCount < 96)
	{
		Com_PrintfAlways("STEFX: sound missing base='%s'\n", tryName);
		s_soundMissingLogCount++;
	}

	return -1;
}

/*
==============
S_GetFileCode
==============
*/
int S_GetFileCode( const char* sSoundName )
{
	char	sLoadName[MAX_QPATH];

	// make up a local filename to try wav/mp3 substitutes...
	//	
	Q_strncpyz(sLoadName, sSoundName, sizeof(sLoadName));	
	Q_strlwr( sLoadName );

	// make sure we have an extension...
	//
	if (sLoadName[strlen(sLoadName) - 4] != '.')
	{
		strcat(sLoadName, ".xxx");
	}

	return S_LoadSound_FileNameAdjuster(sLoadName);
}

/*
============
S_UpdateLoading
============
*/
void S_UpdateLoading(void) {
	#ifdef _XBOX
	g_SPXBAudioLoadStage = 0x414C3030; /* 'AL00': entered */
	#endif
	for ( int i = 0; i < SND_MAX_LOADS; ++i )
	{
		#ifdef _XBOX
		g_SPXBAudioLoadIndex = (unsigned int)i;
		g_SPXBAudioLoadHandle = s_LoadList[i] ? (unsigned int)s_LoadList[i]->iStreamHandle : 0xffffffff;
		g_SPXBAudioLoadStage = 0x414C3130; /* 'AL10': before stream poll */
		#endif
		if ( s_LoadList[i] &&
			(s_LoadList[i]->iFlags & SFX_FLAG_LOADING) &&
			!Sys_StreamIsReading(s_LoadList[i]->iStreamHandle) )
		{
			#ifdef _XBOX
			g_SPXBAudioLoadStage = 0x414C3230; /* 'AL20': before load completion */
			#endif
			S_EndLoadSound(s_LoadList[i]);
			#ifdef _XBOX
			g_SPXBAudioLoadStage = 0x414C3231; /* 'AL21': load completion returned */
			#endif
			s_LoadList[i] = NULL;
			--s_LoadListSize;
		}
	}
	#ifdef _XBOX
	g_SPXBAudioLoadStage = 0x414C4646; /* 'ALFF': complete */
	#endif
}

/*
==============
S_BeginLoadSound
==============
*/
qboolean S_StartLoadSound( sfx_t *sfx )
{
	assert(sfx->iFlags & SFX_FLAG_UNLOADED);

	// Stream completion is retired once per frame by S_Update.  Doing that
	// reentrantly while submitting a new sound can deadlock the stream mutex.
	// If the queue is full, leave this sound unloaded for a later-frame retry.
	if (s_LoadListSize >= SND_MAX_LOADS)
	{
#ifdef _XBOX
		static int s_xboxDeferredLoadLogBudget = 32;
		if (s_xboxDeferredLoadLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_LOAD: deferred active=%d limit=%d code=0x%x name='%s'",
				s_LoadListSize, SND_MAX_LOADS, sfx->iFileCode,
				name ? name : "<unknown>");
			--s_xboxDeferredLoadLogBudget;
		}
#endif
		return qfalse;
	}

	sfx->iFlags &= ~SFX_FLAG_UNLOADED;
	
	// Valid file?
	if (sfx->iFileCode == -1)
	{
		sfx->iFlags |= SFX_FLAG_RESIDENT | SFX_FLAG_DEFAULT;
		return qfalse;
	}

#if PROFILE_SOUND
	extern char* Sys_GetSoundName( unsigned int crc );
	char* name = Sys_GetSoundName(sfx->iFileCode);
	int time	= Sys_Milliseconds();
	Com_Printf("SOUND: %s at %d\n", name, time);
#endif

	// Open the file
#ifdef _XBOX
	{
		static int s_xboxStreamStageLogBudget = 48;
		if (s_xboxStreamStageLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_LOAD: open begin active=%d code=0x%x name='%s'",
				s_LoadListSize, sfx->iFileCode, name ? name : "<unknown>");
			--s_xboxStreamStageLogBudget;
		}
	}
#endif
	sfx->iSoundLength = Sys_StreamOpen(sfx->iFileCode, &sfx->iStreamHandle);
#ifdef _XBOX
	{
		static int s_xboxStreamOpenLogBudget = 48;
		if (s_xboxStreamOpenLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_LOAD: open end active=%d handle=%d length=%d code=0x%x name='%s'",
				s_LoadListSize, sfx->iStreamHandle, sfx->iSoundLength,
				sfx->iFileCode, name ? name : "<unknown>");
			--s_xboxStreamOpenLogBudget;
		}
	}
#endif
	if ( sfx->iSoundLength <= 0 )
	{
#ifdef _XBOX
		const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
		Com_PrintfAlways("STEFX: S_StartLoadSound open failed name='%s' code=0x%x len=%d\n",
			name ? name : "<unknown>", sfx->iFileCode, sfx->iSoundLength);
#endif
		sfx->iFlags |= SFX_FLAG_RESIDENT | SFX_FLAG_DEFAULT;
		return qfalse;
	}
#ifdef _XBOX
	{
		const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
		const char *category = "sfx";
		int *budget = NULL;
		static int voiceBudget = 48;
		static int weaponBudget = 48;
		static int ambientBudget = 32;
		static int foleyBudget = 48;
		static int otherBudget = 32;

		if (sfx->iFlags & SFX_FLAG_VOICE)
		{
			category = "vo";
			budget = &voiceBudget;
		}
		else if (name && (strstr(name, "weapons/") || strstr(name, "weapon/") ||
			strstr(name, "weapons\\") || strstr(name, "weapon\\")))
		{
			category = "weapon";
			budget = &weaponBudget;
		}
		else if (name && (strstr(name, "ambient") || strstr(name, "world/") || strstr(name, "world\\")))
		{
			category = "ambient";
			budget = &ambientBudget;
		}
		else if (name && (strstr(name, "foot") || strstr(name, "step") ||
			strstr(name, "pain") || strstr(name, "fall")))
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
			Com_PrintfAlways("STEFX_DN3_PROOF: audio load category=%s name='%s' code=0x%x len=%d flags=0x%x\n",
				category, name ? name : "<unknown>", sfx->iFileCode, sfx->iSoundLength, sfx->iFlags);
			--(*budget);
		}
	}
#endif

#ifdef _GAMECUBE
	// Allocate a buffer to read into...
	SND_PrepareForSoundLoad(sfx, sfx->iSoundLength + 64);
	sfx->pSoundData = Z_Malloc(sfx->iSoundLength + 64, TAG_SND_RAWDATA,
			qtrue, 32);
#else
	// Allocate a buffer to read into...
	SND_PrepareForSoundLoad(sfx, sfx->iSoundLength);
#ifdef _XBOX
	if ((sfx->iFlags & SFX_FLAG_VOICE) && sfx->iSoundLength >= (256 * 1024))
	{
		sfx->pSoundData = FS_STEFX_AllocExternalSoundBuffer(sfx->iSoundLength);
	}
	else
#endif
	{
		sfx->pSoundData = Z_Malloc(sfx->iSoundLength, TAG_SND_RAWDATA, qtrue, 32);
	}
#endif

#ifdef _XBOX
	// AL_MEMORY_USED does not include every byte held by the zone allocator,
	// and the zone can become fragmented even while the configured sound-pool
	// limit says that another sound should fit.  A full-quality VO line is a
	// large contiguous allocation, so make one bounded cache purge and retry
	// after an actual allocation failure.  SND_FreeOldestSound deliberately
	// preserves buffers still attached to channels and never frees this sfx.
	if (!sfx->pSoundData)
	{
		const int freed = SND_FreeOldestSound(sfx);
		const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
		XBLog_WriteCriticalf("STEFX_AUDIO_CACHE: zone retry incoming=%d freed=%d code=0x%x name='%s'",
			sfx->iSoundLength, freed, sfx->iFileCode,
			name ? name : "<unknown>");
#ifdef _GAMECUBE
		sfx->pSoundData = Z_Malloc(sfx->iSoundLength + 64, TAG_SND_RAWDATA,
			qtrue, 32);
#else
		sfx->pSoundData = Z_Malloc(sfx->iSoundLength, TAG_SND_RAWDATA, qtrue, 32);
#endif
	}
#endif

	// Setup the background read
#ifdef _XBOX
	{
		static int s_xboxStreamReadLogBudget = 48;
		if (s_xboxStreamReadLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_LOAD: read begin active=%d handle=%d length=%d code=0x%x name='%s'",
				s_LoadListSize, sfx->iStreamHandle, sfx->iSoundLength,
				sfx->iFileCode, name ? name : "<unknown>");
			--s_xboxStreamReadLogBudget;
		}
	}
#endif
	if ( !sfx->pSoundData || 
			!Sys_StreamRead(sfx->pSoundData, sfx->iSoundLength, 0, 
				sfx->iStreamHandle) )
	{
#ifdef _XBOX
		const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
		XBLog_WriteCriticalf("STEFX_AUDIO_LOAD: queue failed allocation=%d handle=%d length=%d code=0x%x name='%s'",
			sfx->pSoundData ? 1 : 0, sfx->iStreamHandle, sfx->iSoundLength,
			sfx->iFileCode, name ? name : "<unknown>");
#endif
		if(sfx->pSoundData) {
			S_FreeRawSoundData(sfx->pSoundData);
		}
		Sys_StreamClose(sfx->iStreamHandle);
		sfx->iFlags |= SFX_FLAG_RESIDENT | SFX_FLAG_DEFAULT;
		return qfalse;
	}
	sfx->iFlags |= SFX_FLAG_LOADING;
#ifdef _XBOX
	{
		static int s_xboxStreamQueuedLogBudget = 48;
		if (s_xboxStreamQueuedLogBudget > 0)
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			XBLog_WriteRingMarkerf("STEFX_AUDIO_LOAD: read queued active=%d handle=%d code=0x%x name='%s'",
				s_LoadListSize, sfx->iStreamHandle, sfx->iFileCode,
				name ? name : "<unknown>");
			--s_xboxStreamQueuedLogBudget;
		}
	}
#endif

	// add sound to load list
	for (int i = 0; i < SND_MAX_LOADS; ++i)
	{
		if (!s_LoadList[i])
		{
			s_LoadList[i] = sfx;
			++s_LoadListSize;
			break;
		}
	}

	return qtrue;
}

/*
==============
S_EndLoadSound
==============
*/
qboolean S_EndLoadSound( sfx_t *sfx )
{
	wavinfo_t	info;
	byte*		data;
	ALuint		Buffer;

	assert(sfx->iFlags & SFX_FLAG_LOADING);
	sfx->iFlags &= ~SFX_FLAG_LOADING;

	// was the read successful?
	if (Sys_StreamIsError(sfx->iStreamHandle))
	{
#if defined(FINAL_BUILD)
		extern void ERR_DiscFail(bool);
		ERR_DiscFail(false);
#endif
		Sys_StreamClose(sfx->iStreamHandle);
		S_FreeRawSoundData(sfx->pSoundData);
		sfx->iFlags |= SFX_FLAG_RESIDENT | SFX_FLAG_DEFAULT;
		return qfalse;
	}

	Sys_StreamClose(sfx->iStreamHandle);
	SND_TouchSFX(sfx);

	sfx->iLastTimeUsed = Com_Milliseconds()+1;	// why +1? Hmmm, leave it for now I guess	

	// loading a WAV, presumably...
	data = (byte*)sfx->pSoundData;
	info = GetWavInfo( data );

	if (info.size == 0)
	{
#ifdef _XBOX
		if ( sfx->iFlags & SFX_FLAG_VOICE )
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			Com_PrintfAlways("STEFX: S_EndLoadSound wav parse empty name='%s' code=0x%x raw=%d\n",
				name ? name : "<unknown>", sfx->iFileCode, sfx->iSoundLength);
		}
#endif
		if (!S_DecodeMP3Sound(sfx, data, sfx->iSoundLength, &info))
		{
			S_FreeRawSoundData(sfx->pSoundData);
			sfx->iFlags |= SFX_FLAG_RESIDENT | SFX_FLAG_DEFAULT;
			return qfalse;
		}

		data = (byte*)sfx->pSoundData;
	}
	
	sfx->iSoundLength = info.size;
	if (info.byteRate > 0)
	{
		sfx->iSoundDurationMs = (int)(((__int64)info.size * 1000) / info.byteRate);
	}
	else if (info.samples > 0 && info.rate > 0)
	{
		sfx->iSoundDurationMs = (int)(((__int64)info.samples * 1000) / info.rate);
	}
	else
	{
		sfx->iSoundDurationMs = 0;
	}

	// make sure we have enough space for the sound
	SND_update(sfx);

	// Clear Open AL Error State
	alGetError();

	// Generate AL Buffer
	alGenBuffers(1, &Buffer);

	// Copy audio data to AL Buffer
	// NOTE: pass the WHOLE file, header included.  Unlike PC OpenAL (which
	// takes raw samples and needs data + dataofs), the Xbox AL shim's
	// alBufferData calls _wavFindDataOffset() on this pointer to locate the
	// data chunk itself, and REJECTS an ADPCM buffer whose offset is 0.
	// Passing data + info.dataofs makes every ADPCM sound fail to load.
	alBufferData(Buffer, info.format, data, 
		sfx->iSoundLength, info.rate);
	if (alGetError() != AL_NO_ERROR)
	{
#ifdef _XBOX
		if ( sfx->iFlags & SFX_FLAG_VOICE )
		{
			const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
			Com_PrintfAlways("STEFX: S_EndLoadSound alBufferData failed name='%s' code=0x%x tag=0x%x fmt=0x%x size=%d rate=%d\n",
				name ? name : "<unknown>", sfx->iFileCode, info.waveFormatTag, info.format, sfx->iSoundLength, info.rate);
		}
#endif
		S_FreeRawSoundData(sfx->pSoundData);
		sfx->iFlags |= SFX_FLAG_UNLOADED;
		return qfalse;
	}
#ifdef _XBOX
	{
		const char *name = Sys_GetSoundFileCodeName(sfx->iFileCode);
		const char *category = "sfx";

		QAL_XboxSetBufferDebugName(Buffer, name ? name : "<unknown>");

		if (sfx->iFlags & SFX_FLAG_VOICE)
		{
			category = "vo";
		}
		else if (name && (strstr(name, "weapons/") || strstr(name, "weapon/") ||
			strstr(name, "weapons\\") || strstr(name, "weapon\\")))
		{
			category = "weapon";
		}
		else if (name && (strstr(name, "ambient") || strstr(name, "world/") || strstr(name, "world\\")))
		{
			category = "ambient";
		}
		else if (name && (strstr(name, "foot") || strstr(name, "step") ||
			strstr(name, "pain") || strstr(name, "fall")))
		{
			category = "foley";
		}

		Com_PrintfAlways("STEFX_DN3_PROOF: audio buffer category=%s name='%s' code=0x%x tag=0x%x fmt=0x%x size=%d rate=%d byteRate=%d durationMs=%d buffer=%d\n",
			category, name ? name : "<unknown>", sfx->iFileCode, info.waveFormatTag, info.format, sfx->iSoundLength, info.rate, info.byteRate, sfx->iSoundDurationMs, Buffer);
	}
#endif
	
	sfx->Buffer = Buffer;

#if defined(_GAMECUBE)
	{
		static int s_xboxRawSoundFreeCount = 0;
		if (s_xboxRawSoundFreeCount < 16 || (s_xboxRawSoundFreeCount & 63) == 0)
		{
			Com_PrintfAlways("JA: S_EndLoadSound freeing raw sound copy count=%d bytes=%d\n",
				s_xboxRawSoundFreeCount, sfx->iSoundLength);
		}
		s_xboxRawSoundFreeCount++;
	}
	Z_Free(sfx->pSoundData);
	sfx->pSoundData = NULL;
#endif
#if defined(_XBOX)
	{
		static int s_xboxRawSoundTransferCount = 0;
		if (s_xboxRawSoundTransferCount < 32 || (s_xboxRawSoundTransferCount & 127) == 0)
		{
			Com_PrintfAlways("JA: S_EndLoadSound transferred raw sound to QAL count=%d bytes=%d buffer=%d\n",
				s_xboxRawSoundTransferCount, sfx->iSoundLength, Buffer);
		}
		s_xboxRawSoundTransferCount++;
	}
	sfx->pSoundData = NULL;
#endif
	sfx->iFlags |= SFX_FLAG_RESIDENT;
#ifdef _XBOX
	S_XboxOnSoundLoaded(sfx);
#endif

	return qtrue;
}

/*
============
S_InitLoad
============
*/
void S_InitLoad(void) {
	s_LoadList = new sfx_t*[SND_MAX_LOADS];
	memset(s_LoadList, 0, SND_MAX_LOADS * sizeof(sfx_t*));
	s_LoadListSize = 0;
}

/*
============
S_CloseLoad
============
*/
void S_CloseLoad(void) {
	delete [] s_LoadList;
}

