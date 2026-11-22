
/*
 * UNPUBLISHED -- Rights  reserved  under  the  copyright  laws  of the 
 * United States.  Use  of a copyright notice is precautionary only and 
 * does not imply publication or disclosure.                            
 *                                                                      
 * THIS DOCUMENTATION CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION 
 * OF    VICARIOUS   VISIONS,  INC.    ANY  DUPLICATION,  MODIFICATION, 
 * DISTRIBUTION, OR DISCLOSURE IS STRICTLY PROHIBITED WITHOUT THE PRIOR 
 * EXPRESS WRITTEN PERMISSION OF VICARIOUS VISIONS, INC.
 */

#include "../server/exe_headers.h"

#include "../client/client.h"
#include "win_local.h"
#include "win_file.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/files.h"

#if defined(_WINDOWS)
#include <windows.h>
#elif defined(_XBOX)
#include <xtl.h>
#include "xb_log.h"
#endif

extern void Z_SetNewDeleteTemporary(bool);

#define STREAM_SLOW_READ 0

#include "../client/snd_local_console.h"

#include <deque>

extern HANDLE Sys_FileStreamMutex;
extern int Sys_GetFileCodeSize(int code);
extern const char* Sys_GetFileCodeName(int code);

#define STREAM_MAX_OPEN 48
struct StreamInfo
{
	unsigned int file;
	volatile bool used;
	volatile bool error;
	volatile bool opening;
	volatile bool reading;
	volatile bool looseFile;
};
static StreamInfo* s_Streams = NULL;

enum IORequestType
{
	IOREQ_OPEN,
	IOREQ_READ,
	IOREQ_SHUTDOWN,
};

struct IORequest
{
	IORequestType type;
	streamHandle_t handle;
	DWORD data[3];
};
typedef std::deque<IORequest> requestqueue_t;
requestqueue_t* s_IORequestQueue = NULL;

HANDLE s_Thread = INVALID_HANDLE_VALUE;
HANDLE s_QueueMutex = INVALID_HANDLE_VALUE;
HANDLE s_QueueLen = INVALID_HANDLE_VALUE;


#include "../qcommon/fixedmap.h"

typedef struct
{
	int offset;
	int size;
	char *name;
	unsigned char filenameFlags;
} sound_file_t;

static HANDLE	soundfile	= INVALID_HANDLE_VALUE;
static VVFixedMap< sound_file_t, unsigned int >* soundLookup = NULL;

typedef struct
{
	unsigned int code;
	int size;
	char name[MAX_QPATH];
	char osName[MAX_OSPATH];
	bool directFile;
} loose_sound_file_t;

#define MAX_LOOSE_SOUND_FILES 4096
static loose_sound_file_t s_looseSoundFiles[MAX_LOOSE_SOUND_FILES];
static int s_numLooseSoundFiles = 0;

static void Sys_NormalizeSoundName(const char *name, char *out, int outSize)
{
	Q_strncpyz(out, name, outSize);
	Q_strlwr(out);
	for (int i = 0; out[i]; ++i)
	{
		if (out[i] == '\\')
		{
			out[i] = '/';
		}
	}
}

static loose_sound_file_t *Sys_FindLooseSoundFile(unsigned int code)
{
	for (int i = 0; i < s_numLooseSoundFiles; ++i)
	{
		if (s_looseSoundFiles[i].code == code)
		{
			return &s_looseSoundFiles[i];
		}
	}

	return NULL;
}

static bool Sys_ResolveLooseSoundOSPath(const char *name, char *out, int outSize, int *outSizeBytes)
{
	searchpath_t *search;

	if (!name || !out || outSize <= 0)
	{
		return false;
	}

	out[0] = '\0';
	if (outSizeBytes)
	{
		*outSizeBytes = -1;
	}

	FS_CheckInit();
	for (search = fs_searchpaths; search; search = search->next)
	{
		HANDLE h;
		DWORD high = 0;
		DWORD low;
		char osName[MAX_OSPATH];

		if (!search->dir)
		{
			continue;
		}

		Q_strncpyz(osName, FS_BuildOSPath(search->dir->path, search->dir->gamedir, name), sizeof(osName));
		h = CreateFile(osName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		low = GetFileSize(h, &high);
		CloseHandle(h);
		if (low == INVALID_FILE_SIZE || high != 0)
		{
			continue;
		}

		Q_strncpyz(out, osName, outSize);
		if (outSizeBytes)
		{
			*outSizeBytes = (int)low;
		}
		return true;
	}

	return false;
}

static loose_sound_file_t *Sys_RegisterLooseSoundFile(unsigned int code, const char *name)
{
	loose_sound_file_t *existing = Sys_FindLooseSoundFile(code);
	if (existing)
	{
		return existing;
	}

	if (s_numLooseSoundFiles >= MAX_LOOSE_SOUND_FILES)
	{
		static int s_looseSoundOverflowLogged = 0;
		if (!s_looseSoundOverflowLogged)
		{
			XBLog_Write("STEFX: loose sound registry full; additional sounds will be unavailable");
			s_looseSoundOverflowLogged = 1;
		}
		return NULL;
	}

	char normalized[MAX_QPATH];
	char osName[MAX_OSPATH];
	int directLen = -1;
	Sys_NormalizeSoundName(name, normalized, sizeof(normalized));

	bool directFile = Sys_ResolveLooseSoundOSPath(normalized, osName, sizeof(osName), &directLen);
	int len = directLen;
	if (!directFile)
	{
		fileHandle_t file = 0;
		len = FS_FOpenFileRead(normalized, &file, qfalse);
		if (!file || len <= 0)
		{
			return NULL;
		}
		FS_FCloseFile(file);
		osName[0] = '\0';
	}
	loose_sound_file_t *entry = &s_looseSoundFiles[s_numLooseSoundFiles++];
	entry->code = code;
	entry->size = len;
	Q_strncpyz(entry->name, normalized, sizeof(entry->name));
	Q_strncpyz(entry->osName, osName, sizeof(entry->osName));
	entry->directFile = directFile;

	static int s_looseSoundRegisterLogCount = 0;
	if (s_looseSoundRegisterLogCount < 128)
	{
		XBLog_Write(va("STEFX: loose sound registered code=0x%08x size=%d direct=%d name='%s' os='%s'",
			code, len, entry->directFile ? 1 : 0, entry->name, entry->osName));
		s_looseSoundRegisterLogCount++;
	}

	return entry;
}

static DWORD Sys_ReadLooseSoundViaFS(loose_sound_file_t *loose, DWORD offset, DWORD requested, void *dest, bool *outError)
{
	DWORD bytes = 0;
	fileHandle_t file = 0;
	int len;
	int seekResult = 0;
	static int s_fsSoundReadDetailLogCount = 0;

	if (outError)
	{
		*outError = true;
	}

	if (!loose || !dest)
	{
		return 0;
	}

	len = FS_FOpenFileRead(loose->name, &file, qfalse);
	if (file && len >= (int)(offset + requested))
	{
		if (offset != 0)
		{
			seekResult = FS_Seek(file, offset, FS_SEEK_SET);
		}

		bytes = FS_Read(dest, requested, file);
		if (outError)
		{
			*outError = (bytes != requested);
		}
	}

	if (s_fsSoundReadDetailLogCount < 64)
	{
		XBLog_Write(va("STEFX: loose sound FS read detail name='%s' file=%d len=%d pos=%u seek=%d requested=%u bytes=%u error=%d",
			loose->name,
			file,
			len,
			offset,
			seekResult,
			requested,
			bytes,
			(outError && *outError) ? 1 : 0));
		s_fsSoundReadDetailLogCount++;
	}

	if (file)
	{
		FS_FCloseFile(file);
	}

	return bytes;
}

static DWORD Sys_ReadLooseSoundViaOS(loose_sound_file_t *loose, DWORD offset, DWORD requested, void *dest, bool *outError)
{
	DWORD bytes = 0;
	wfhandle_t h;
	int seekResult = -1;
	static int s_osSoundReadDetailLogCount = 0;

	if (outError)
	{
		*outError = true;
	}
	if (!loose || !dest || !loose->osName[0])
	{
		return 0;
	}

	h = WF_Open(loose->osName, true, false);
	if (h < 0)
	{
		HANDLE rawFile = CreateFile(loose->osName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (rawFile != INVALID_HANDLE_VALUE)
		{
			DWORD rawMoved = SetFilePointer(rawFile, offset, NULL, FILE_BEGIN);
			DWORD rawBytes = 0;
			BOOL rawOk = (rawMoved == offset) && ReadFile(rawFile, dest, requested, &rawBytes, NULL);
			CloseHandle(rawFile);
			if (rawOk && rawBytes == requested)
			{
				if (outError)
				{
					*outError = false;
				}
				if (s_osSoundReadDetailLogCount < 64)
				{
					XBLog_Write(va("STEFX: loose sound raw OS read name='%s' pos=%u requested=%u bytes=%u os='%s'",
						loose->name, offset, requested, rawBytes, loose->osName));
					s_osSoundReadDetailLogCount++;
				}
				return rawBytes;
			}
		}

		if (s_osSoundReadDetailLogCount < 64)
		{
			XBLog_Write(va("STEFX: loose sound OS read open failed name='%s' os='%s' requested=%u rawErr=%lu",
				loose->name, loose->osName, requested, GetLastError()));
			s_osSoundReadDetailLogCount++;
		}
		return 0;
	}

	seekResult = WF_Seek((int)offset, SEEK_SET, h);
	if (seekResult == (int)offset)
	{
		bytes = (DWORD)WF_Read(dest, (int)requested, h);
		if (bytes == requested && outError)
		{
			*outError = false;
		}
	}

	if (s_osSoundReadDetailLogCount < 64)
	{
		XBLog_Write(va("STEFX: loose sound OS read detail name='%s' h=%d pos=%u seek=%d requested=%u bytes=%u error=%d os='%s'",
			loose->name,
			h,
			offset,
			seekResult,
			requested,
			bytes,
			(outError && *outError) ? 1 : 0,
			loose->osName));
		s_osSoundReadDetailLogCount++;
	}

	WF_Close(h);
	return bytes;
}

void Sys_StreamInitialize( void );

static DWORD WINAPI _streamThread(LPVOID)
{
	for (;;)
	{
		IORequest req;
		DWORD bytes = 0;
		StreamInfo* strm;

		// Wait for the IO queue to fill
		WaitForSingleObject(s_QueueLen, INFINITE);
		
		// Grab the next IO request
		WaitForSingleObject(s_QueueMutex, INFINITE);
		assert(!s_IORequestQueue->empty());
		req = s_IORequestQueue->front();
		s_IORequestQueue->pop_front();
		ReleaseMutex(s_QueueMutex);

		int offset = 0;
		sound_file_t* crap;

		// Process request
		switch (req.type)
		{
		case IOREQ_OPEN:

			strm = &s_Streams[req.handle];
			assert(strm->used);
		
			strm->file	= req.data[0];
			strm->looseFile = (soundLookup ? soundLookup->Find(strm->file) : NULL) ? false : true;
			strm->error = (strm->file == -1) ||
				(strm->looseFile && !Sys_FindLooseSoundFile(strm->file));
			strm->opening = false;
			break;
/*
			{
				const char* name = Sys_GetFileCodeName(req.data[0]);

				WaitForSingleObject(Sys_FileStreamMutex, INFINITE);
			
				strm->file = 
					CreateFile(name, GENERIC_READ, 
					FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			
				ReleaseMutex(Sys_FileStreamMutex);
			}

			strm->error = (strm->file == INVALID_HANDLE_VALUE);
			strm->opening = false;
			break;
*/
		case IOREQ_READ:
			{
				strm = &s_Streams[req.handle];
				assert(strm->used);
				bytes = 0;

				crap = soundLookup ? soundLookup->Find(strm->file) : NULL;

				if(crap)
				{
					WaitForSingleObject(Sys_FileStreamMutex, INFINITE);
					offset	= crap->offset + req.data[2];
					strm->error = req.data[2] > (DWORD)crap->size ||
						req.data[1] > (DWORD)crap->size - req.data[2] ||
						SetFilePointer(soundfile, offset, 0, FILE_BEGIN) != offset ||
						!ReadFile(soundfile, (void*)req.data[0], req.data[1], &bytes, NULL) ||
						bytes != req.data[1];
					ReleaseMutex(Sys_FileStreamMutex);
					if (strm->error)
						XBLog_WriteCriticalf("STEFX_AUDIO_BANK: read failed code=0x%08x offset=%d requested=%u read=%u",
							strm->file, offset, req.data[1], bytes);
				}
				else
				{
					loose_sound_file_t *loose = Sys_FindLooseSoundFile(strm->file);
					if (loose)
					{
						if (loose->directFile)
						{
							bool osError = true;
							bytes = Sys_ReadLooseSoundViaOS(loose, req.data[2], req.data[1], (void*)req.data[0], &osError);
							strm->error = osError;
							if (strm->error)
							{
								DWORD directBytes = bytes;
								bool fsError = true;
								DWORD fsBytes = Sys_ReadLooseSoundViaFS(loose, req.data[2], req.data[1], (void*)req.data[0], &fsError);
								if (!fsError)
								{
									bytes = fsBytes;
									strm->error = false;
								}

								static int s_looseSoundFallbackLogCount = 0;
								if (s_looseSoundFallbackLogCount < 32)
								{
									XBLog_Write(va("STEFX: loose sound direct fallback name='%s' requested=%u wfBytes=%u wfError=%d fsBytes=%u fsError=%d",
										loose->name, req.data[1], directBytes, osError ? 1 : 0, fsBytes, fsError ? 1 : 0));
									s_looseSoundFallbackLogCount++;
								}
							}
						}
						else
						{
							bool fsError = true;
							bytes = Sys_ReadLooseSoundViaFS(loose, req.data[2], req.data[1], (void*)req.data[0], &fsError);
							strm->error = fsError;
						}

						static int s_looseSoundReadLogCount = 0;
						if (s_looseSoundReadLogCount < 128)
						{
							XBLog_Write(va("STEFX: loose sound read direct=%d name='%s' pos=%u requested=%u bytes=%u error=%d os='%s'",
								loose->directFile ? 1 : 0, loose->name, req.data[2], req.data[1], bytes, strm->error ? 1 : 0, loose->osName));
							s_looseSoundReadLogCount++;
						}
					}
					else
					{
						strm->error = true;
					}
				}


				/*
				strm->error = 
				(SetFilePointer(strm->file, req.data[2], 0, FILE_BEGIN) != req.data[2] ||
				ReadFile(strm->file, (void*)req.data[0], req.data[1], &bytes, NULL) == 0);
				*/

				strm->reading = false;
			}
			break;

		case IOREQ_SHUTDOWN:
			ExitThread(0);
			break;
		}
	}

	return TRUE;
}


static void _sendIORequest(const IORequest& req)
{
	// Add request to queue
	WaitForSingleObject(s_QueueMutex, INFINITE);
	Z_SetNewDeleteTemporary(true);
	s_IORequestQueue->push_back(req);
	Z_SetNewDeleteTemporary(false);
	ReleaseMutex(s_QueueMutex);

	// Let IO thread know it has one more pending request
	ReleaseSemaphore(s_QueueLen, 1, NULL);
}

void Sys_IORequestQueueClear(void)
{
	WaitForSingleObject(s_QueueMutex, INFINITE);
	delete s_IORequestQueue;
	s_IORequestQueue = new requestqueue_t;
	ReleaseMutex(s_QueueMutex);
}

void Sys_StreamInit(void)
{
	Sys_StreamInitialize();

	// Create array for storing open streams
	s_Streams = (StreamInfo*)Z_Malloc(
		STREAM_MAX_OPEN * sizeof(StreamInfo), TAG_FILESYS, qfalse);
	for (int i = 0; i < STREAM_MAX_OPEN; ++i)
	{
		s_Streams[i].used = false;
		s_Streams[i].looseFile = false;
	}

	// Create queue to hold requests for IO thread
	s_IORequestQueue = new requestqueue_t;

	// Create a thread to service IO
	s_QueueMutex = CreateMutex(NULL, FALSE, NULL);
	s_QueueLen = CreateSemaphore(NULL, 0, STREAM_MAX_OPEN * 3, NULL);
	s_Thread = CreateThread(NULL, 64*1024, _streamThread, 0, 0, NULL);
}

void Sys_StreamShutdown(void)
{
	// Tell the IO thread to shutdown
	IORequest req;
	req.type = IOREQ_SHUTDOWN;
	_sendIORequest(req);

	// Wait for thread to close
	WaitForSingleObject(s_Thread, INFINITE);
	
	// Kill IO thread
	CloseHandle(s_Thread);
	CloseHandle(s_QueueLen);
	CloseHandle(s_QueueMutex);

	// Remove queue of IO requests
	delete s_IORequestQueue;
	
	// Remove streaming table
	Z_Free(s_Streams);
	if (soundLookup)
	{
		sound_file_t *entry;
		while ((entry = soundLookup->Pop()) != NULL)
		{
			if (entry->name) Z_Free(entry->name);
		}
		delete soundLookup;
		soundLookup = NULL;
		CloseHandle(soundfile);
		soundfile = INVALID_HANDLE_VALUE;
	}
}

static streamHandle_t GetFreeHandle(void)
{
	for (streamHandle_t i = 1; i < STREAM_MAX_OPEN; ++i)
	{
		if (!s_Streams[i].used) return i;
	}
	
	// handle 0 is invalid by convention
	return 0;
}

int Sys_StreamOpen(int code, streamHandle_t *handle)
{
	// Find a free handle
	*handle = GetFreeHandle();
	if (*handle == 0)
	{
		return -1;
	}

	// Find the file size
	sound_file_t*	crap	= soundLookup ? soundLookup->Find(code) : NULL;
	loose_sound_file_t *loose = NULL;
	int				size	= -1;
	if(crap)
	{
		size	= crap->size;
	}
	else
	{
		loose = Sys_FindLooseSoundFile(code);
		if (loose)
		{
			size = loose->size;
		}
	}

	if (size < 0)
	{
		*handle = 0;
		return -1;
	}

	// Init stream data
	s_Streams[*handle].used = true;
	s_Streams[*handle].opening = true;
	s_Streams[*handle].reading = false;
	s_Streams[*handle].error = false;
	s_Streams[*handle].looseFile = loose ? true : false;

	// Send an open request to the thread
	IORequest req;
	req.type = IOREQ_OPEN;
	req.handle = *handle;
	req.data[0] = code;
	_sendIORequest(req);

	// Return file size
	return size;
}

bool Sys_StreamRead(void* buffer, int size, int pos, streamHandle_t handle)
{
	assert((unsigned int)buffer % 32 == 0);

	// Handle must be valid.  Do not allow multiple reads.
	if (!s_Streams[handle].used || s_Streams[handle].reading) return false;

	// Ready to read
	s_Streams[handle].reading = true;
	s_Streams[handle].error = false;
	
	// Request IO threading reading
	IORequest req;
	req.type = IOREQ_READ;
	req.handle = handle;
	req.data[0] = (DWORD)buffer;
	req.data[1] = size;
	req.data[2] = pos;
	_sendIORequest(req);
	
	return true;
}

bool Sys_StreamIsReading(streamHandle_t handle)
{
	return s_Streams[handle].used && s_Streams[handle].reading;
}

bool Sys_StreamIsError(streamHandle_t handle)
{
	return s_Streams[handle].used && s_Streams[handle].error;
}

void Sys_StreamClose(streamHandle_t handle)
{
	if (s_Streams[handle].used)
	{
		// Block until read is done
		while (s_Streams[handle].opening || s_Streams[handle].reading);
		
		// Close the file
//		CloseHandle(s_Streams[handle].file);
		s_Streams[handle].used = false;
	}
}

extern char* FS_BuildOSPathUnMapped(const char* name);

unsigned int Sys_GetSoundFileCode(const char* name)
{
	// Get system level path
	char* osname = FS_BuildOSPathUnMapped(name);
	
	// Generate hash for file name
	strlwr(osname);
	unsigned int code = crc32(0, (const byte *)osname, strlen(osname));

	sound_file_t *bankEntry = soundLookup ? soundLookup->Find(code) : NULL;
	if (bankEntry)
	{
		// The bank index stores CRCs only. Voice and effect routing also use
		// the registered path, even when the optional file list is absent.
		if (!bankEntry->name)
		{
			char normalized[MAX_QPATH];
			Sys_NormalizeSoundName(name, normalized, sizeof(normalized));
			bankEntry->name = CopyString(normalized);
		}
		return code;
	}

	Sys_RegisterLooseSoundFile(code, name);

	return code;
}

unsigned int Sys_GetSoundFileCodeFlags(unsigned int code)
{
	sound_file_t*	sf;
	sf	= soundLookup ? soundLookup->Find(code) : NULL;

	if(!sf)
	{
		return 0;
	}
	else
	{
		return sf->filenameFlags;
	}
}

const char *Sys_GetSoundFileCodeName(unsigned int code)
{
	sound_file_t *bankEntry = soundLookup ? soundLookup->Find(code) : NULL;
	if (bankEntry)
		return bankEntry->name;
	loose_sound_file_t *loose = Sys_FindLooseSoundFile(code);
	if (loose)
	{
		return loose->name;
	}

	return Sys_GetFileCodeName(code);
}

int Sys_GetSoundFileCodeSize(unsigned int code)
{
	sound_file_t*	sf;
	sf	= soundLookup ? soundLookup->Find(code) : NULL;

	if(!sf)
	{
		loose_sound_file_t *loose = Sys_FindLooseSoundFile(code);
		if (loose)
		{
			return loose->size;
		}
		return -1;
	}
	else
	{
		return sf->size;
	}

}


#if PROFILE_SOUND
VVFixedMap< char*, unsigned int>* soundCrc	= NULL;
void Sys_LoadSoundCRCFile( void )
{
	FILE*	file;
	file	= fopen("d:\\base\\soundbank\\crclookup.txt", "rb");

	if(!file)
		return;

	int numberOfLines	= 0;
	unsigned int crc	= 0;
	char	name[255];

	// count the number of lines
	while(1)
	{
		if(fscanf(file, "%d %s", &crc, name) == -1)
			break;
		numberOfLines++;
	}

	// allocate memory for the crc lookup
	soundCrc = new VVFixedMap<char*, unsigned int>(numberOfLines);

	// actually read and store the data
	fseek(file, 0, SEEK_SET);
	while(1)
	{
		if(fscanf(file, "%d %s", &crc, name) == -1)
			break;

		char *temp	= (char*)Z_Malloc(strlen(name) + 1, TAG_SND_RAWDATA, qtrue);

		strcpy(temp, name);

		soundCrc->Insert(temp, crc);
	}
	soundCrc->Sort();
	fclose(file);
}

char* Sys_GetSoundName( unsigned int crc )
{
	char* name = *soundCrc->Find(crc);
	return name;
}

#endif // PROFILE_SOUND

extern const char *Sys_RemapPath( const char *filename );

void Sys_StreamInitialize( void )
{
	if (soundLookup)
	{
		return;
	}

	// open the sound file
	soundfile	= CreateFile(
		Sys_RemapPath("BaseEF\\soundbank\\sound.bnk"),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_READONLY | FILE_FLAG_RANDOM_ACCESS,
		NULL );

	if (soundfile == INVALID_HANDLE_VALUE)
	{
		XBLog_Write("EF: Sys_StreamInitialize sound.bnk missing; disabling JA soundbank streaming");
		return;
	}

	// fill in the lookup table
	HANDLE	table	= INVALID_HANDLE_VALUE;

	table	= CreateFile(
		Sys_RemapPath("BaseEF\\soundbank\\sound.tbl"),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL );

	if (table == INVALID_HANDLE_VALUE)
	{
		XBLog_Write("EF: Sys_StreamInitialize sound.tbl missing; disabling JA soundbank streaming");
		CloseHandle(soundfile);
		soundfile = INVALID_HANDLE_VALUE;
		return;
	}

	DWORD	fileSize	= 0;
	fileSize = GetFileSize(
		table,
		NULL);

	const DWORD recordSize = (sizeof(unsigned int) * 3) + 1;
	if (fileSize == INVALID_FILE_SIZE || fileSize == 0 || (fileSize % recordSize) != 0)
	{
		XBLog_Write(va("EF: Sys_StreamInitialize invalid sound.tbl size=%u; disabling JA soundbank streaming", (unsigned int)fileSize));
		CloseHandle(table);
		CloseHandle(soundfile);
		soundfile = INVALID_HANDLE_VALUE;
		return;
	}

	int numberOfRecords	= fileSize / recordSize;
	if (numberOfRecords <= 0 || numberOfRecords > 131072)
	{
		XBLog_Write(va("EF: Sys_StreamInitialize suspicious sound.tbl records=%d size=%u; disabling JA soundbank streaming", numberOfRecords, (unsigned int)fileSize));
		CloseHandle(table);
		CloseHandle(soundfile);
		soundfile = INVALID_HANDLE_VALUE;
		return;
	}

	XBLog_Write(va("EF: Sys_StreamInitialize soundbank records=%d size=%u", numberOfRecords, (unsigned int)fileSize));

	byte*	tempData	= (byte*)Z_Malloc(fileSize, TAG_TEMP_WORKSPACE, true, 32);
	byte*	restore		= tempData;

	DWORD bytesRead = 0;
	DWORD bankBytes = GetFileSize(soundfile, NULL);
	bool valid = bankBytes != INVALID_FILE_SIZE && bankBytes <= 0x7fffffff &&
		tempData && ReadFile(table, tempData, fileSize, &bytesRead, NULL) && bytesRead == fileSize;
	CloseHandle(table);
	for (int index = 0; valid && index < numberOfRecords; ++index)
	{
		const byte *record = tempData + index * recordSize;
		const DWORD offset = *(const DWORD *)(record + 4);
		const DWORD size = *(const DWORD *)(record + 8);
		valid = size >= 12 && offset <= bankBytes && size <= bankBytes - offset;
	}
	if (!valid)
	{
		XBLog_WriteCritical("STEFX_AUDIO_BANK: invalid bank/table pair; using sound files");
		if (tempData) Z_Free(tempData);
		CloseHandle(soundfile);
		soundfile = INVALID_HANDLE_VALUE;
		return;
	}

	soundLookup = new VVFixedMap<sound_file_t, unsigned int>(numberOfRecords);
	for(int i = 0; i < numberOfRecords; i++)
	{
		unsigned int filecode	= *(unsigned int*)tempData;
		tempData += sizeof(unsigned int);
		unsigned int offset		= *(unsigned int*)tempData;
		tempData += sizeof(unsigned int);
		int size				= *(int*)tempData;
		tempData += sizeof(int);
		unsigned char filenameFlags = *(unsigned char*)tempData;
		tempData++;

		sound_file_t sfile;
		sfile.name = NULL;
		sfile.offset	= offset;
		sfile.size		= size;
		sfile.filenameFlags = filenameFlags;

		soundLookup->Insert(sfile, filecode);
	}

	soundLookup->Sort();
	Z_Free(restore);
#if PROFILE_SOUND
	Sys_LoadSoundCRCFile();
#endif
}
