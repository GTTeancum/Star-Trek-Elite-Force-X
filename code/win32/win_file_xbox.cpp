
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

#include "../game/q_shared.h"
#include "win_file.h"
#include "../qcommon/qcommon.h"

#ifdef _XBOX
#include <Xtl.h>
#include "xb_log.h"
#endif

#ifdef _WINDOWS
#include <windows.h>
#endif


struct FileTable
{
	LONG m_bUsed;
	bool m_bErrorsFatal;
	HANDLE m_Handle;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	bool m_bNtHandle;
#endif
};

FileTable* s_FileTable = NULL;
#ifdef STEFX_ELITE_FORCE_SP
const int WF_MAX_OPEN_FILES = 32;
#else
const int WF_MAX_OPEN_FILES = 16;
#endif

static bool WF_ShouldTracePath(const char* name)
{
	if (!name)
	{
		return false;
	}

	return strstr(name, ".mdr") || strstr(name, ".md3") || strstr(name, ".tik") ||
		strstr(name, "default.cfg") ||
		strstr(name, "ext_data\\weapons") || strstr(name, "ext_data/weapons") ||
		strstr(name, "gfx\\2d\\chars") || strstr(name, "gfx/2d/chars") ||
		strstr(name, "\\sound\\") || strstr(name, "/sound/") ||
		strstr(name, "sound\\") || strstr(name, "sound/") ||
		strstr(name, "textures\\borg\\") || strstr(name, "textures/borg/") ||
		strstr(name, "textures\\detail\\") || strstr(name, "textures/detail/") ||
		strstr(name, "real_scripts\\") || strstr(name, "real_scripts/");
}

static int WF_CountUsedHandles(void)
{
	int used = 0;

	if (!s_FileTable)
	{
		return 0;
	}

	for (int i = 0; i < WF_MAX_OPEN_FILES; ++i)
	{
		if (s_FileTable[i].m_bUsed)
		{
			++used;
		}
	}

	return used;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
typedef struct { unsigned short Length; unsigned short MaximumLength; char *Buffer; } WF_NT_STR;
typedef struct { HANDLE RootDirectory; WF_NT_STR *ObjectName; unsigned long Attributes; } WF_NT_OA;
typedef struct { union { long Status; void *Pointer; }; unsigned long Information; } WF_NT_IOSB;

extern "C" long __stdcall NtCreateFile(HANDLE*, unsigned long, WF_NT_OA*, WF_NT_IOSB*,
	LARGE_INTEGER*, unsigned long, unsigned long, unsigned long, unsigned long);
extern "C" long __stdcall NtClose(HANDLE);
extern "C" long __stdcall NtReadFile(HANDLE, HANDLE, void*, void*, WF_NT_IOSB*,
	void*, unsigned long, LARGE_INTEGER*);

static const char *WF_NtDrivePrefix(const char *name, int variant)
{
	if (!name || name[1] != ':')
	{
		return NULL;
	}

	switch (tolower(name[0]))
	{
	case 'd':
	{
		static const char *prefixes[] =
		{
			"\\Device\\CdRom0\\",
			"\\Device\\Cdrom0\\",
			"\\Device\\Harddisk0\\Partition1\\",
			"\\Device\\Harddisk0\\Partition6\\",
			"\\Device\\Harddisk0\\Partition7\\",
			NULL
		};
		return prefixes[variant];
	}
	case 'e':
	{
		static const char *prefixes[] =
		{
			"\\Device\\Harddisk0\\Partition1\\",
			NULL
		};
		return prefixes[variant];
	}
	case 'f':
	{
		static const char *prefixes[] =
		{
			"\\Device\\Harddisk0\\Partition6\\",
			NULL
		};
		return prefixes[variant];
	}
	case 'g':
	{
		static const char *prefixes[] =
		{
			"\\Device\\Harddisk0\\Partition7\\",
			NULL
		};
		return prefixes[variant];
	}
	}

	return NULL;
}

static qboolean WF_BuildNtDevicePath(const char *name, int variant, char *out, int outSize)
{
	const char *prefix = WF_NtDrivePrefix(name, variant);
	const char *rest;
	char normalized[768];
	int i;

	if (!prefix || !out || outSize <= 0)
	{
		return qfalse;
	}

	rest = name + 2;
	while (*rest == '\\' || *rest == '/')
	{
		++rest;
	}

	for (i = 0; rest[i] && i < (int)sizeof(normalized) - 1; ++i)
	{
		normalized[i] = (rest[i] == '/') ? '\\' : rest[i];
	}
	normalized[i] = 0;

	Com_sprintf(out, outSize, "%s%s", prefix, normalized);
	return qtrue;
}

static long WF_NtOpenReadExisting(const char *ntPath, HANDLE *out)
{
	WF_NT_STR name;
	WF_NT_OA oa;
	WF_NT_IOSB iosb;

	name.Buffer = (char *)ntPath;
	name.Length = (unsigned short)strlen(ntPath);
	name.MaximumLength = name.Length + 1;
	oa.RootDirectory = NULL;
	oa.ObjectName = &name;
	oa.Attributes = 0x40;	// OBJ_CASE_INSENSITIVE

	return NtCreateFile(out,
		GENERIC_READ | 0x00100000,	// GENERIC_READ | SYNCHRONIZE
		&oa, &iosb, NULL,
		FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
		1,							// FILE_OPEN
		0x20 | 0x40);				// SYNCHRONOUS_IO_NONALERT | NON_DIRECTORY
}

static qboolean WF_TryNtOpenFallback(const char *name, wfhandle_t handle, bool aligned, bool modelProbe)
{
	HANDLE h = INVALID_HANDLE_VALUE;
	char ntPath[1024];
	long status = -1;
	int variant;

	if (aligned || !name || name[1] != ':')
	{
		return qfalse;
	}

	for (variant = 0; WF_BuildNtDevicePath(name, variant, ntPath, sizeof(ntPath)); ++variant)
	{
		status = WF_NtOpenReadExisting(ntPath, &h);
		if (status >= 0 && h != INVALID_HANDLE_VALUE)
		{
			s_FileTable[handle].m_Handle = h;
			s_FileTable[handle].m_bUsed = true;
			s_FileTable[handle].m_bNtHandle = true;
			s_FileTable[handle].m_bErrorsFatal = (name[0] == 'D' || name[0] == 'd');

			if (modelProbe)
			{
				XBLog_Write(va("STEFX: WF_Open NtCreateFile ok slot=%d used=%d/%d raw='%s' name='%s'",
					handle, WF_CountUsedHandles(), WF_MAX_OPEN_FILES, ntPath, name));
			}
			return qtrue;
		}
	}

	if (modelProbe)
	{
		XBLog_Write(va("STEFX: WF_Open NtCreateFile failed variants=%d lastStatus=0x%08lx name='%s'",
			variant, status, name));
	}
	return qfalse;
}
#endif

void WF_Init(void)
{
	assert(!s_FileTable);

	s_FileTable = new FileTable[WF_MAX_OPEN_FILES];

	for (wfhandle_t i = 0; i < WF_MAX_OPEN_FILES; ++i)
	{
		s_FileTable[i].m_bUsed = false;
		s_FileTable[i].m_Handle = INVALID_HANDLE_VALUE;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		s_FileTable[i].m_bNtHandle = false;
#endif
	}
}

void WF_Shutdown(void)
{
	assert(s_FileTable);
	
	for (wfhandle_t i = 0; i < WF_MAX_OPEN_FILES; ++i)
	{
		if (s_FileTable[i].m_bUsed)
		{
			WF_Close(i);
		}
	}

	delete [] s_FileTable;
	s_FileTable = NULL;
}

static wfhandle_t WF_GetFreeHandle(void)
{
	for (int i = 0; i < WF_MAX_OPEN_FILES; ++i)
	{
		// File opens can run concurrently with archived sound streaming.
		if (InterlockedCompareExchange(&s_FileTable[i].m_bUsed, 1, 0) == 0)
		{
			return i;
		}
	}

	return -1;
}

int WF_Open(const char* name, bool read, bool aligned)
{
	wfhandle_t handle = WF_GetFreeHandle();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean probePath = WF_ShouldTracePath(name);
#endif
	if (handle == -1)
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (probePath)
		{
			XBLog_Write(va("STEFX: WF_Open no free handle used=%d max=%d name='%s'", WF_CountUsedHandles(), WF_MAX_OPEN_FILES, name ? name : "(null)"));
		}
#endif
		return -1;
	}

	s_FileTable[handle].m_Handle = 
		CreateFile(name, read ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE, 
		FILE_SHARE_READ, 0, 
		read ? OPEN_EXISTING : OPEN_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL |(aligned ? FILE_FLAG_NO_BUFFERING : 0) , 0);

	if (s_FileTable[handle].m_Handle != INVALID_HANDLE_VALUE)
	{
		s_FileTable[handle].m_bUsed = true;
		
		// errors are fatal on game partition
		s_FileTable[handle].m_bErrorsFatal = (name[0] == 'D' || name[0] == 'd');
		
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		s_FileTable[handle].m_bNtHandle = false;
		if (probePath)
		{
			XBLog_Write(va("STEFX: WF_Open ok slot=%d used=%d/%d aligned=%d name='%s'", handle, WF_CountUsedHandles(), WF_MAX_OPEN_FILES, aligned ? 1 : 0, name ? name : "(null)"));
		}
#endif
		return handle;
	}
	
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	s_FileTable[handle].m_bNtHandle = false;
	if (probePath)
	{
		DWORD err = GetLastError();
		XBLog_Write(va("STEFX: WF_Open CreateFile failed slot=%d used=%d/%d aligned=%d err=%lu name='%s'", handle, WF_CountUsedHandles(), WF_MAX_OPEN_FILES, aligned ? 1 : 0, err, name ? name : "(null)"));
	}
	if (read && (strstr(name, ".mdr") || strstr(name, ".md3") || strstr(name, ".tik") ||
		strstr(name, "\\sound\\") || strstr(name, "/sound/") ||
		strstr(name, "sound\\") || strstr(name, "sound/") ||
		strstr(name, ".IBI") || strstr(name, ".ibi") ||
		strstr(name, ".pre") || strstr(name, ".PRE") ||
		strstr(name, ".rof") || strstr(name, ".ROF")) &&
		WF_TryNtOpenFallback(name, handle, aligned, probePath))
	{
		return handle;
	}
#endif
	InterlockedExchange(&s_FileTable[handle].m_bUsed, 0);
	return -1;
}

void WF_Close(wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (s_FileTable[handle].m_bNtHandle)
	{
		NtClose(s_FileTable[handle].m_Handle);
	}
	else
#endif
	{
		CloseHandle(s_FileTable[handle].m_Handle);
	}
	s_FileTable[handle].m_Handle = INVALID_HANDLE_VALUE;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	s_FileTable[handle].m_bNtHandle = false;
#endif
	InterlockedExchange(&s_FileTable[handle].m_bUsed, 0);
}

int WF_Read(void* buffer, int len, wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

	DWORD bytes = 0;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (s_FileTable[handle].m_bNtHandle)
	{
		WF_NT_IOSB iosb;
		long status = NtReadFile(s_FileTable[handle].m_Handle, NULL, NULL, NULL,
			&iosb, buffer, len, NULL);
		if (status < 0)
		{
			bytes = 0;
			if (s_FileTable[handle].m_bErrorsFatal)
			{
#if defined(FINAL_BUILD)
				extern void ERR_DiscFail(bool);
				ERR_DiscFail(false);
#else
				assert(0);
#endif
			}
		}
		else
		{
			bytes = iosb.Information;
		}
		return bytes;
	}
#endif
	if (!ReadFile(s_FileTable[handle].m_Handle, buffer, len, &bytes, 0) &&
		s_FileTable[handle].m_bErrorsFatal)
	{
#if defined(FINAL_BUILD)
		extern void ERR_DiscFail(bool);
		ERR_DiscFail(false);
#else
		assert(0);
#endif
	}

	return bytes;
}

int WF_Write(const void* buffer, int len, wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

	DWORD bytes;
	WriteFile(s_FileTable[handle].m_Handle, buffer, len, &bytes, 0);
	return bytes;
}

int WF_Seek(int offset, int origin, wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

	switch (origin)
	{
	case SEEK_CUR: origin = FILE_CURRENT; break;
	case SEEK_END: origin = FILE_END; break;
	case SEEK_SET: origin = FILE_BEGIN; break;
	default: assert(false);
	}

	return SetFilePointer(s_FileTable[handle].m_Handle, offset, 0, origin);
}

int WF_Tell(wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

	return SetFilePointer(s_FileTable[handle].m_Handle, 0, 0, FILE_CURRENT);
}

int WF_Resize(int size, wfhandle_t handle)
{
	assert(handle >= 0 && handle < WF_MAX_OPEN_FILES && 
		s_FileTable[handle].m_bUsed);

	SetFilePointer(s_FileTable[handle].m_Handle, size, NULL, FILE_BEGIN);
	return SetEndOfFile(s_FileTable[handle].m_Handle);
}
