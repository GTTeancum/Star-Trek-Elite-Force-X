
#include "../game/q_shared.h"
#include "qcommon.h"
#include "files.h"
#ifdef _XBOX
#include "platform.h"
#include "../win32/xb_log.h"
#endif
#include "../win32/win_file.h"
#ifndef STEFX_ELITE_FORCE_SP
#include "../zlib/zlib.h"
#endif


//#define GOB_PROFILE


static	cvar_t		*fs_openorder;

#if defined(STEFX_ELITE_FORCE_SP)
#define MAX_FILEHASH_SIZE 1024
#define MAX_PAKFILES 1024
#define STEFX_ZIP_SEEK_SCRATCH_SIZE (8 * 1024)
static byte s_stefxZipSeekScratch[STEFX_ZIP_SEEK_SCRATCH_SIZE];
#endif

// Zlib Tech Ref says decompression should use about 44kb.  I'll
// go with 64kb as a safety factor...
#define ZI_STACKSIZE (64*1024)

static char* zi_stackTop = NULL;
static char* zi_stackBase = NULL;



//GOB stuff
//===========================================================================

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
extern "C" volatile unsigned int g_SPXBShaderScanMagic;
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBFileAllocStage;
extern "C" volatile unsigned int g_SPXBFileAllocPathHash;
extern "C" volatile unsigned int g_SPXBFileAllocPathPtr;
extern "C" volatile unsigned int g_SPXBFileAllocLength;
extern "C" volatile unsigned int g_SPXBFileAllocTag;
extern "C" volatile unsigned int g_SPXBFSWholeCloseStage;
extern "C" volatile unsigned int g_SPXBFSWholeCloseHandle;
#define STEFX_FS_MODEL_PHASE(stage) \
	( g_SPXBPhaseLast = 0xF0000000u | ( ( (unsigned int)(stage) & 0xffu ) << 16 ) | \
		( g_SPXBPhaseLast & 0xffffu ) )

static qboolean STEFX_ShouldTraceAssetOpen(const char *filename)
{
	return filename &&
		(strstr(filename, ".mdr") || strstr(filename, ".md3") ||
		 strstr(filename, ".skin") || strstr(filename, "animation.cfg") ||
		 strstr(filename, "ext_data/boltOns.cfg") || strstr(filename, "ext_data\\boltOns.cfg") ||
		 strstr(filename, "ext_data/items") || strstr(filename, "ext_data\\items") ||
		 strstr(filename, "ext_data/weapons") || strstr(filename, "ext_data\\weapons") ||
		 strstr(filename, "ext_data/NPCs") || strstr(filename, "ext_data\\NPCs") ||
		 strstr(filename, "ext_data/addon") || strstr(filename, "ext_data\\addon") ||
		 strstr(filename, "maps/") || strstr(filename, "maps\\") ||
		 strstr(filename, "scripts/") || strstr(filename, "scripts\\") ||
		 strstr(filename, "textures/borg/") || strstr(filename, "textures\\borg\\") ||
		 strstr(filename, "gfx/2d/chars") || strstr(filename, "gfx\\2d\\chars") ||
		 strstr(filename, "real_scripts/") || strstr(filename, "real_scripts\\"));
}

static qboolean STEFX_IsWholeFileImage(const char *filename)
{
	const char *ext = filename ? strrchr(filename, '.') : NULL;

	return ext &&
		(!Q_stricmp(ext, ".tga") ||
		 !Q_stricmp(ext, ".jpg") ||
		 !Q_stricmp(ext, ".jpeg") ||
		 !Q_stricmp(ext, ".dds"));
}

static qboolean STEFX_IsPlayerAnimationCfg(const char *filename)
{
	return filename &&
		(strstr(filename, "animation.cfg") &&
		 (strstr(filename, "models/players/") || strstr(filename, "models\\players\\")));
}

static qboolean STEFX_IsMapBSP(const char *filename)
{
	const char *ext = filename ? strrchr(filename, '.') : NULL;

	return ext && !Q_stricmp(ext, ".bsp") &&
		(strstr(filename, "maps/") || strstr(filename, "maps\\"));
}

static qboolean STEFX_IsBoltOnsCfg(const char *filename)
{
	return filename &&
		(!Q_stricmp(filename, "ext_data/boltOns.cfg") ||
		 !Q_stricmp(filename, "ext_data\\boltOns.cfg"));
}

static qboolean STEFX_IsCriticalWholeFileRead(const char *filename)
{
	const char *ext;

	if (!filename)
	{
		return qfalse;
	}

	ext = strrchr(filename, '.');
	return !Q_stricmp(filename, "default.cfg") ||
		!Q_stricmp(filename, "ext_data/fonts.dat") ||
		!Q_stricmp(filename, "ext_data\\fonts.dat") ||
		STEFX_IsBoltOnsCfg(filename) ||
		STEFX_IsMapBSP(filename) ||
		(ext && !Q_stricmp(ext, ".dat") &&
			 (strstr(filename, "ext_data/") || strstr(filename, "ext_data\\"))) ||
		(ext && !Q_stricmp(ext, ".cfg") &&
		 (strstr(filename, "ext_data/NPCs") || strstr(filename, "ext_data\\NPCs"))) ||
		(ext && !Q_stricmp(ext, ".npc") &&
		 (strstr(filename, "ext_data/") || strstr(filename, "ext_data\\"))) ||
		(ext && !Q_stricmp(ext, ".nav") &&
		 (strstr(filename, "maps/") || strstr(filename, "maps\\"))) ||
		(ext && !Q_stricmp(ext, ".sqd") &&
		 (strstr(filename, "maps/") || strstr(filename, "maps\\"))) ||
		strstr(filename, "gfx/2d/chars") || strstr(filename, "gfx\\2d\\chars");
}

static qboolean STEFX_ShouldTryStdioWholeFileRead(const char *filename)
{
	const char *ext = filename ? strrchr(filename, '.') : NULL;

	if (filename &&
		(!Q_stricmpn(filename, "botfiles/", 9) ||
		 !Q_stricmpn(filename, "botfiles\\", 9) ||
		 !Q_stricmpn(filename, "bots/", 5) ||
		 !Q_stricmpn(filename, "bots\\", 5)))
	{
		return qtrue;
	}

	// Bot definitions are byte-sized text reads. Unbuffered loose-file handles
	// cannot reliably seek to their non-sector-aligned end or read parser buffers.
	if (filename && ext &&
		(!Q_stricmpn(filename, "scripts/", 8) || !Q_stricmpn(filename, "scripts\\", 8)) &&
		(!Q_stricmp(ext, ".txt") || !Q_stricmp(ext, ".bot") || !Q_stricmp(ext, ".arena")))
	{
		return qtrue;
	}

	if (STEFX_IsCriticalWholeFileRead(filename))
	{
		return qtrue;
	}

	return ext &&
		(!Q_stricmp(ext, ".mdr") ||
		 !Q_stricmp(ext, ".md3") ||
		 !Q_stricmp(ext, ".tik") ||
		 !Q_stricmp(ext, ".aas") ||
		 !Q_stricmp(ext, ".IBI") ||
		 !Q_stricmp(ext, ".pre") ||
		 !Q_stricmp(ext, ".rof"));
}

#define STEFX_PRECACHE_FILES_MAX 128
#define STEFX_PRECACHE_BYTES_MAX (16 * 1024 * 1024)

typedef struct stefxPrecacheFile_s
{
	char name[MAX_QPATH];
	byte *data;
	int len;
} stefxPrecacheFile_t;

static stefxPrecacheFile_t s_stefxPrecacheFiles[STEFX_PRECACHE_FILES_MAX];
static int s_stefxPrecacheFileCount = 0;
static int s_stefxPrecacheBytes = 0;

static memtag_t STEFX_WholeFileTag(const char *qpath)
{
	const char *ext = qpath ? strrchr(qpath, '.') : NULL;

	if (STEFX_IsMapBSP(qpath))
	{
		return TAG_BSP;
	}

	if (ext && (!Q_stricmp(ext, ".mdr") || !Q_stricmp(ext, ".md3")))
	{
		return TAG_MODEL_MD3;
	}

	return TAG_FILESYS;
}

static unsigned int STEFX_FileAllocHash(const char *text)
{
	unsigned int hash = 2166136261u;

	while (text && *text)
	{
		unsigned char c = (unsigned char)*text++;
		if (c == '\\')
		{
			c = '/';
		}
		if (c >= 'A' && c <= 'Z')
		{
			c = (unsigned char)(c - 'A' + 'a');
		}
		hash ^= c;
		hash *= 16777619u;
	}

	return hash;
}

typedef struct stefxHeapFileHeader_s
{
	unsigned int magic;
	void *base;
	int len;
	int allocSize;
	int allocator;
} stefxHeapFileHeader_t;

#define STEFX_HEAP_FILE_MAGIC 0x48464246u /* 'HFBF' */
#define STEFX_FILE_ALLOC_HEAP 1
#define STEFX_FILE_ALLOC_VIRTUAL 2

static stefxHeapFileHeader_t *STEFX_GetHeapFileHeader(const void *buffer)
{
	stefxHeapFileHeader_t *header;

	if (!buffer)
	{
		return NULL;
	}

	header = ((stefxHeapFileHeader_t *)buffer) - 1;
	if (header->magic != STEFX_HEAP_FILE_MAGIC || !header->base)
	{
		return NULL;
	}

	return header;
}

qboolean FS_STEFX_IsHeapFileBuffer(const void *buffer)
{
	return STEFX_GetHeapFileHeader(buffer) ? qtrue : qfalse;
}

static byte *STEFX_AllocHeapFileBuffer(int len, const char *qpath)
{
	extern void *Z_TryMalloc(int size, memtag_t tag, int alignment);
	memtag_t tag;
	byte *base;
	byte *payload;
	stefxHeapFileHeader_t *header;
	unsigned int payloadAddress;
	int allocSize;
	int allocator = STEFX_FILE_ALLOC_HEAP;
	static int s_heapFileLogBudget = 96;

	if (len < 0)
	{
		return NULL;
	}

	tag = STEFX_WholeFileTag(qpath);
	g_SPXBFileAllocStage = 0x10;
	g_SPXBFileAllocTag = (unsigned int)tag;
	allocSize = len + 1 + sizeof(stefxHeapFileHeader_t) + 31;
	STEFX_FS_MODEL_PHASE( 0x5A );
	base = NULL;
	// Large map and model images outlive this read when the renderer adopts the
	// buffer.  The Xbox process heap can become too fragmented for the next MDR
	// even though enough page-backed memory remains, so use the same VirtualAlloc
	// path for both persistent asset classes before falling back to HeapAlloc.
	if (tag == TAG_BSP || tag == TAG_MODEL_MD3)
	{
		g_SPXBFileAllocStage = 0x11;
		base = (byte *)VirtualAlloc(NULL, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (base)
		{
			allocator = STEFX_FILE_ALLOC_VIRTUAL;
		}
	}
	if (!base)
	{
		g_SPXBFileAllocStage = 0x12;
		base = (byte *)HeapAlloc(GetProcessHeap(), 0, allocSize);
	}
	g_SPXBFileAllocStage = 0x13;
	STEFX_FS_MODEL_PHASE( 0x5B );
	if (!base)
	{
		byte *zoneFallback = NULL;
		XBLog_Write(va("STEFX: FS whole-file heap unavailable file='%s' len=%d tag=%d bytes=%d",
			qpath ? qpath : "(null)", len, (int)tag, allocSize));
		if (tag == TAG_MODEL_MD3 || tag == TAG_BSP)
		{
			g_SPXBFileAllocStage = 0x14;
			zoneFallback = tag == TAG_MODEL_MD3
				? (byte *)Z_TryMalloc(len + 1, tag, 32)
				: (byte *)Z_Malloc(len + 1, tag, qfalse, 32);
			if (zoneFallback)
			{
				XBLog_WriteCriticalf("STEFX: FS whole-file zone fallback file='%s' len=%d tag=%d payload=%p",
					qpath ? qpath : "(null)", len, (int)tag, zoneFallback);
				return zoneFallback;
			}
			XBLog_WriteCriticalf("STEFX: FS whole-file zone fallback failed file='%s' len=%d tag=%d",
				qpath ? qpath : "(null)", len, (int)tag);
		}
		return NULL;
	}

	payloadAddress = ((unsigned int)(base + sizeof(stefxHeapFileHeader_t) + 31)) & ~31u;
	payload = (byte *)payloadAddress;
	header = ((stefxHeapFileHeader_t *)payload) - 1;
	header->magic = STEFX_HEAP_FILE_MAGIC;
	header->base = base;
	header->len = len;
	header->allocSize = allocSize;
	header->allocator = allocator;
	g_SPXBFileAllocStage = 0x15;

	if (s_heapFileLogBudget > 0 &&
		(len >= (256 * 1024) || tag == TAG_MODEL_MD3 || tag == TAG_BSP || STEFX_ShouldTraceAssetOpen(qpath)))
	{
		XBLog_Write(va("STEFX: FS whole-file alloc file='%s' len=%d tag=%d bytes=%d allocator=%d payload=%p",
			qpath ? qpath : "(null)", len, (int)tag, allocSize, allocator, payload));
		--s_heapFileLogBudget;
	}

	return payload;
}

// Large full-quality dialogue WAVs need a contiguous temporary read buffer.
// Keeping that buffer out of the fixed zone leaves the zone available as the
// last-resort home for late MDR files when the Xbox page/heap allocators are
// already tight.  The normal heap-file header lets every existing owner test
// and release this allocation without a parallel tracking table.
void *FS_STEFX_AllocExternalSoundBuffer(int len)
{
	byte *base;
	byte *payload;
	stefxHeapFileHeader_t *header;
	unsigned int payloadAddress;
	const int allocSize = len + sizeof(stefxHeapFileHeader_t) + 31;
	int allocator = STEFX_FILE_ALLOC_VIRTUAL;

	if (len <= 0)
	{
		return NULL;
	}

	base = (byte *)VirtualAlloc(NULL, allocSize,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!base)
	{
		allocator = STEFX_FILE_ALLOC_HEAP;
		base = (byte *)HeapAlloc(GetProcessHeap(), 0, allocSize);
	}
	if (!base)
	{
		XBLog_Write(va("STEFX_AUDIO_CACHE: external allocation failed bytes=%d", len));
		return NULL;
	}

	payloadAddress = ((unsigned int)(base + sizeof(stefxHeapFileHeader_t) + 31)) & ~31u;
	payload = (byte *)payloadAddress;
	header = ((stefxHeapFileHeader_t *)payload) - 1;
	header->magic = STEFX_HEAP_FILE_MAGIC;
	header->base = base;
	header->len = len;
	header->allocSize = allocSize;
	header->allocator = allocator;
	return payload;
}

qboolean FS_STEFX_FreeHeapFileBuffer(void *buffer)
{
	stefxHeapFileHeader_t *header = STEFX_GetHeapFileHeader(buffer);

	if (!header)
	{
		return qfalse;
	}

	header->magic = 0;
	if (header->allocator == STEFX_FILE_ALLOC_VIRTUAL)
	{
		VirtualFree(header->base, 0, MEM_RELEASE);
	}
	else
	{
		HeapFree(GetProcessHeap(), 0, header->base);
	}
	return qtrue;
}

static int STEFX_FindPrecachedFile(const char *filename)
{
	int i;

	if (!filename)
	{
		return -1;
	}

	for (i = 0; i < s_stefxPrecacheFileCount; ++i)
	{
		if (!Q_stricmp(s_stefxPrecacheFiles[i].name, filename))
		{
			return i;
		}
	}

	return -1;
}

static int STEFX_ReadPrecachedFile(const char *filename, void **buffer)
{
	int index = STEFX_FindPrecachedFile(filename);
	byte *copy;

	if (index < 0)
	{
		return -1;
	}

	if (!buffer)
	{
		return s_stefxPrecacheFiles[index].len;
	}

	copy = (byte *)Z_Malloc(s_stefxPrecacheFiles[index].len + 1,
		STEFX_WholeFileTag(filename), qfalse, 32);
	if (!copy)
	{
		return -1;
	}
	memcpy(copy, s_stefxPrecacheFiles[index].data, s_stefxPrecacheFiles[index].len);
	copy[s_stefxPrecacheFiles[index].len] = 0;
	*buffer = copy;
	if (filename && (strstr(filename, "real_scripts/") || strstr(filename, "real_scripts\\")))
	{
		XBLog_Write(va("STEFX: FS precache hit '%s' len=%d", filename, s_stefxPrecacheFiles[index].len));
	}
	return s_stefxPrecacheFiles[index].len;
}

void FS_STEFX_PrecacheFile(const char *qpath)
{
	fileHandle_t h = 0;
	int len;
	byte *buf;

	FS_CheckInit();

	if (!qpath || !qpath[0] || STEFX_FindPrecachedFile(qpath) >= 0)
	{
		return;
	}

	if (s_stefxPrecacheFileCount >= STEFX_PRECACHE_FILES_MAX)
	{
		XBLog_Write(va("STEFX: FS precache file cap full, skipping '%s' count=%d/%d bytes=%d",
			qpath, s_stefxPrecacheFileCount, STEFX_PRECACHE_FILES_MAX, s_stefxPrecacheBytes));
		return;
	}

	len = FS_FOpenFileRead(qpath, &h, qfalse);
	if (h == 0 || len <= 0)
	{
		if (h)
		{
			FS_FCloseFile(h);
		}
		XBLog_Write(va("STEFX: FS precache miss '%s' len=%d handle=%d", qpath, len, h));
		return;
	}

	if (len > STEFX_PRECACHE_BYTES_MAX - s_stefxPrecacheBytes)
	{
		FS_FCloseFile(h);
		XBLog_Write(va("STEFX: FS precache byte cap full, skipping '%s' len=%d bytes=%d/%d",
			qpath, len, s_stefxPrecacheBytes, STEFX_PRECACHE_BYTES_MAX));
		return;
	}

	buf = (byte *)Z_Malloc(len + 1, STEFX_WholeFileTag(qpath), qfalse, 32);
	if (!buf)
	{
		FS_FCloseFile(h);
		XBLog_Write(va("STEFX: FS precache zone alloc failed '%s' len=%d", qpath, len));
		return;
	}
	FS_Read(buf, len, h);
	FS_FCloseFile(h);
	buf[len] = 0;

	Q_strncpyz(s_stefxPrecacheFiles[s_stefxPrecacheFileCount].name, qpath, sizeof(s_stefxPrecacheFiles[s_stefxPrecacheFileCount].name));
	s_stefxPrecacheFiles[s_stefxPrecacheFileCount].data = buf;
	s_stefxPrecacheFiles[s_stefxPrecacheFileCount].len = len;
	++s_stefxPrecacheFileCount;
	s_stefxPrecacheBytes += len;

	XBLog_Write(va("STEFX: FS precache ok '%s' len=%d count=%d/%d bytes=%d/%d",
		qpath, len, s_stefxPrecacheFileCount, STEFX_PRECACHE_FILES_MAX,
		s_stefxPrecacheBytes, STEFX_PRECACHE_BYTES_MAX));
}

void FS_STEFX_ClearPrecache(const char *reason)
{
	int i;
	int oldCount = s_stefxPrecacheFileCount;
	int oldBytes = s_stefxPrecacheBytes;

	for (i = 0; i < s_stefxPrecacheFileCount; ++i)
	{
		if (s_stefxPrecacheFiles[i].data)
		{
			Z_Free(s_stefxPrecacheFiles[i].data);
		}
		s_stefxPrecacheFiles[i].name[0] = '\0';
		s_stefxPrecacheFiles[i].data = NULL;
		s_stefxPrecacheFiles[i].len = 0;
	}

	s_stefxPrecacheFileCount = 0;
	s_stefxPrecacheBytes = 0;

	if (oldCount || oldBytes)
	{
		XBLog_Write(va("STEFX: FS precache clear reason='%s' count=%d bytes=%d",
			reason ? reason : "(none)", oldCount, oldBytes));
	}
}

static int STEFX_ReadLooseFileWithStdio(const char *filename, void **buffer)
{
	searchpath_t *search;
	const char *openName = filename;
	char lowerOpenName[MAX_QPATH];
	int casePass;

	if (!STEFX_ShouldTryStdioWholeFileRead(filename) && !STEFX_IsWholeFileImage(filename))
	{
		return -1;
	}

	for (casePass = 0; casePass < 2; ++casePass)
	{
		const char *caseOpenName = openName;
		if (casePass == 1)
		{
			Q_strncpyz(lowerOpenName, openName, sizeof(lowerOpenName));
			Q_strlwr(lowerOpenName);
			if (!strcmp(lowerOpenName, openName))
			{
				continue;
			}
			caseOpenName = lowerOpenName;
		}

		for (search = fs_searchpaths; search; search = search->next)
		{
			char osname[MAX_OSPATH];
			FILE *fp;
			long len;
			size_t bytesRead;

			if (!search->dir)
			{
				continue;
			}

			Q_strncpyz(osname, FS_BuildOSPath(search->dir->path, search->dir->gamedir, caseOpenName), sizeof(osname));
			fp = fopen(osname, "rb");
			if (!fp)
			{
				continue;
			}

			if (fseek(fp, 0, SEEK_END) != 0)
			{
				fclose(fp);
				continue;
			}
			len = ftell(fp);
			if (len < 0 || fseek(fp, 0, SEEK_SET) != 0)
			{
				fclose(fp);
				continue;
			}

			if (!buffer)
			{
				fclose(fp);
				XBLog_Write(va("STEFX: FS stdio fallback length file='%s' open='%s' os='%s' len=%ld caseRetry=%d",
					filename, caseOpenName, osname, len, casePass));
				return (int)len;
			}

			byte *buf = (byte *)Z_Malloc((int)len + 1,
				STEFX_WholeFileTag(filename), qfalse, 32);
			if (!buf)
			{
				fclose(fp);
				continue;
			}
			bytesRead = fread(buf, 1, (size_t)len, fp);
			fclose(fp);

			if (bytesRead != (size_t)len)
			{
				Z_Free(buf);
				XBLog_Write(va("STEFX: FS stdio fallback short read file='%s' open='%s' os='%s' len=%ld read=%u",
					filename,
					caseOpenName,
					osname,
					len,
					(unsigned int)bytesRead));
				continue;
			}

			buf[len] = 0;
			*buffer = buf;
			XBLog_Write(va("STEFX: FS stdio fallback open file='%s' open='%s' os='%s' len=%ld caseRetry=%d",
				filename, caseOpenName, osname, len, casePass));
			return (int)len;
		}
	}

	XBLog_Write(va("STEFX: FS stdio fallback miss file='%s'", filename ? filename : "(null)"));
	return -1;
}
#endif

#ifndef STEFX_ELITE_FORCE_SP
struct gi_handleTable
{
	wfhandle_t file;
	bool used;
};

static gi_handleTable *gi_handles = NULL;
static int gi_cacheHandle = 0;

static GOBFSHandle gi_open(GOBChar* name, GOBAccessType type)
{
	if (type != GOBACCESS_READ) return (GOBFSHandle)0xFFFFFFFF;

	int f;
	for (f = 0; f < MAX_FILE_HANDLES; ++f)
	{
		if (!gi_handles[f].used) break;
	}

	if (f == MAX_FILE_HANDLES) return (GOBFSHandle)0xFFFFFFFF;
	
	gi_handles[f].file = WF_Open(name, true, strstr(name, "assets.gob") ? true : false);
	if (gi_handles[f].file < 0) return (GOBFSHandle)0xFFFFFFFF;
	gi_handles[f].used = true;

	return (GOBFSHandle)f;
}

static GOBBool gi_close(GOBFSHandle* handle)
{
	WF_Close(gi_handles[(int)*handle].file);
	gi_handles[(int)*handle].used = false;
	return GOB_TRUE;
}

static GOBInt32 gi_read(GOBFSHandle handle, GOBVoid* buffer, GOBInt32 size)
{
	return WF_Read(buffer, size, gi_handles[(int)handle].file);
}

static GOBInt32 gi_seek(GOBFSHandle handle, GOBInt32 offset, GOBSeekType type)
{
	int _type;
	switch (type) {
	case GOBSEEK_START: _type = SEEK_SET; break;
	case GOBSEEK_CURRENT: _type = SEEK_CUR; break;
	case GOBSEEK_END: _type = SEEK_END; break;
	default: assert(0); _type = SEEK_SET; break;
	}

	return WF_Seek(offset, _type, gi_handles[(int)handle].file);
}

static GOBVoid* gi_alloc(GOBUInt32 size)
{
	return Z_Malloc(size, TAG_FILESYS, qfalse, 32);
}

static GOBVoid gi_free(GOBVoid* ptr)
{
	Z_Free(ptr);
}

static GOBBool cache_open(GOBUInt32 size)
{
	for (gi_cacheHandle = 0; gi_cacheHandle < MAX_FILE_HANDLES; ++gi_cacheHandle)
	{
		if (!gi_handles[gi_cacheHandle].used) break;
	}

	if (gi_cacheHandle == MAX_FILE_HANDLES) return GOB_FALSE;
	
	gi_handles[gi_cacheHandle].file = WF_Open("z:\\jedi.swap", false, true);
	if (gi_handles[gi_cacheHandle].file < 0) return GOB_FALSE;

	if (!WF_Resize(size, gi_handles[gi_cacheHandle].file))
	{
		WF_Close(gi_handles[gi_cacheHandle].file);
		return GOB_FALSE;
	}

	gi_handles[gi_cacheHandle].used = true;

	return GOB_TRUE;
}

static GOBBool cache_close(GOBVoid)
{
	WF_Close(gi_handles[gi_cacheHandle].file);
	gi_handles[gi_cacheHandle].used = false;
	return GOB_TRUE;
}

static GOBInt32 cache_read(GOBVoid* buffer, GOBInt32 size)
{
	return WF_Read(buffer, size, gi_handles[gi_cacheHandle].file);
}

static GOBInt32 cache_write(GOBVoid* buffer, GOBInt32 size)
{
	return WF_Write(buffer, size, gi_handles[gi_cacheHandle].file);
}

static GOBInt32 cache_seek(GOBInt32 offset)
{
	return WF_Seek(offset, SEEK_SET, gi_handles[gi_cacheHandle].file);
}

static voidpf zi_alloc(voidpf opaque, uInt items, uInt size)
{
	voidpf ret = zi_stackTop;
	
	zi_stackTop += items * size;
	assert(zi_stackTop < zi_stackBase + ZI_STACKSIZE);

	return ret;
}

static void zi_free(voidpf opaque, voidpf address)
{
}

static GOBInt32 gi_decompress_zlib(GOBVoid* source, GOBUInt32 sourceLen, 
	GOBVoid* dest, GOBUInt32* destLen)
{
	// Copied and modified version of zlib's uncompress()...

	z_stream stream;
	int err;

	stream.next_in = (Bytef*)source;
	stream.avail_in = (uInt)sourceLen;

	stream.next_out = (Bytef*)dest;
	stream.avail_out = (uInt)*destLen;
	if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

	stream.zalloc = zi_alloc;
	stream.zfree = zi_free;
	zi_stackTop = zi_stackBase;

	err = inflateInit(&stream);
	if (err != Z_OK) return err;

	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		inflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}
	*destLen = stream.total_out;

	err = inflateEnd(&stream);
	return err;
}

GOBInt32 gi_decompress_null(GOBVoid* source, GOBUInt32 sourceLen, 
	GOBVoid* dest, GOBUInt32* destLen)
{
	if (sourceLen > *destLen) return -1;
	*destLen = sourceLen;

	memcpy(dest, source, sourceLen);
	return 0;
}

#ifdef GOB_PROFILE
static GOBVoid gi_profileread(GOBUInt32 code)
{
	code = LittleLong(code);
	Sys_Log("gob-prof.dat", &code, sizeof(code), true);
}
#endif

//===========================================================================

#endif // !STEFX_ELITE_FORCE_SP




static void FS_CheckUsed(fileHandle_t f)
{
	if (!fsh[f].used)
	{
		Com_Error( ERR_FATAL, "Filesystem call attempting to use invalid handle\n" );
	}
}


int FS_filelength( fileHandle_t f )
{
	FS_CheckInit();
	FS_CheckUsed(f);
	
#if defined(STEFX_ELITE_FORCE_SP)
	if (fsh[f].zipFile)
	{
		return fsh[f].fileSize;
	}
	else
	{
		int pos = WF_Tell(fsh[f].whandle);
		WF_Seek(0, SEEK_END, fsh[f].whandle);
		int end = WF_Tell(fsh[f].whandle);
		WF_Seek(pos, SEEK_SET, fsh[f].whandle);

		return end;
	}
#else
	if (fsh[f].gob)
	{
		GOBUInt32 cur, end, crap;
		GOBSeek(fsh[f].ghandle, 0, GOBSEEK_CURRENT, &cur);
		GOBSeek(fsh[f].ghandle, 0, GOBSEEK_END, &end);
		GOBSeek(fsh[f].ghandle, cur, GOBSEEK_START, &crap);
		
		return end;
	}
	else
	{
		int pos = WF_Tell(fsh[f].whandle);
		WF_Seek(0, SEEK_END, fsh[f].whandle);
		int end = WF_Tell(fsh[f].whandle);
		WF_Seek(pos, SEEK_SET, fsh[f].whandle);

		return end;
	}
#endif
}


void FS_FCloseFile( fileHandle_t f )
{
	FS_CheckInit();
	FS_CheckUsed(f);

#if defined(STEFX_ELITE_FORCE_SP)
	const qboolean traceWholeClose =
		(g_SPXBFSWholeCloseStage == 0x46534301u && g_SPXBFSWholeCloseHandle == (unsigned int)f) ? qtrue : qfalse;
	if (traceWholeClose)
	{
		g_SPXBFSWholeCloseStage = 0x46534302u; /* FSC2: close entered */
	}
	if (fsh[f].zipFile)
	{
		if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534303u; /* FSC3: current zip member close */
		unzCloseCurrentFile(fsh[f].handleFiles.file.z);
		if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534304u; /* FSC4: zip member closed */
		unzClose(fsh[f].handleFiles.file.z);
		if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534305u; /* FSC5: zip container closed */
	}
	else
	{
		if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534306u; /* FSC6: loose handle close */
		WF_Close(fsh[f].whandle);
		if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534307u; /* FSC7: loose handle closed */
	}

	// Publish the slot as free only after all old owner state is cleared.
	memset(&fsh[f], 0, (byte *)&fsh[f].used - (byte *)&fsh[f]);
	fsh[f].whandle = 0;
	InterlockedExchange((LONG *)&fsh[f].used, qfalse);
	if (traceWholeClose) g_SPXBFSWholeCloseStage = 0x46534308u; /* FSC8: slot cleared */
#else
	if (fsh[f].gob)
		GOBClose(fsh[f].ghandle);
	else
		WF_Close(fsh[f].whandle);

	fsh[f].used = qfalse;
#endif
}


fileHandle_t FS_FOpenFileWrite( const char *filename )
{
	FS_CheckInit();
	
	fileHandle_t f = FS_HandleForFile();

	char* osname = FS_BuildOSPath( filename );
	fsh[f].whandle = WF_Open(osname, false, false);
	if (fsh[f].whandle >= 0)
	{
		fsh[f].used = qtrue;
#ifndef STEFX_ELITE_FORCE_SP
		fsh[f].gob = qfalse;
#endif
		fsh[f].zipFile = qfalse;
		return f;
	}

	InterlockedExchange((LONG *)&fsh[f].used, qfalse);
	return 0;
}


/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/

static int FS_FOpenFileReadOS( const char *filename, fileHandle_t f )
{
#if defined(STEFX_ELITE_FORCE_SP)
	searchpath_t *search;
	const char *openName = filename;
	char lowerOpenName[MAX_QPATH];
	int casePass;
	qboolean traceDefaultCfg = !Q_stricmp(filename, "default.cfg");

#if defined(_XBOX)
	if (traceDefaultCfg)
	{
		XBLog_Write("STEFX: FS default.cfg loose lookup begin");
	}
#endif

	for (casePass = 0; casePass < 2; ++casePass)
	{
		const char *caseOpenName = openName;
		if (casePass == 1)
		{
			Q_strncpyz(lowerOpenName, openName, sizeof(lowerOpenName));
			Q_strlwr(lowerOpenName);
			if (!strcmp(lowerOpenName, openName))
			{
				continue;
			}
			caseOpenName = lowerOpenName;
		}

		for (search = fs_searchpaths; search; search = search->next)
		{
			if (!search->dir)
			{
				continue;
			}

			char* osname = FS_BuildOSPath(search->dir->path, search->dir->gamedir, caseOpenName);
#if defined(_XBOX)
			if (traceDefaultCfg)
			{
				XBLog_Write(va("STEFX: FS default.cfg loose try open='%s' os='%s' caseRetry=%d", caseOpenName, osname, casePass));
			}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			qboolean bufferedSoundRead = !Q_stricmpn(caseOpenName, "sound/", 6) || !Q_stricmpn(caseOpenName, "sound\\", 6);
			qboolean bufferedWholeFileRead = STEFX_ShouldTryStdioWholeFileRead(caseOpenName) ||
				STEFX_IsWholeFileImage(caseOpenName) ||
				strstr(caseOpenName, ".skin") ||
				strstr(caseOpenName, "animation.cfg");
			fsh[f].whandle = WF_Open(osname, true, (bufferedSoundRead || bufferedWholeFileRead) ? false : true);
#else
			fsh[f].whandle = WF_Open(osname, true, false);
#endif
			if (fsh[f].whandle >= 0)
			{
				int len;
				fsh[f].used = qtrue;
				fsh[f].zipFile = qfalse;
				fsh[f].fileSize = 0;
				Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));
				len = FS_filelength(f);
				fsh[f].fileSize = len;
				if (len <= 0 &&
					(STEFX_IsWholeFileImage(caseOpenName) ||
					 STEFX_IsCriticalWholeFileRead(caseOpenName)))
				{
					XBLog_Write(va("STEFX: FS loose asset rejected zero length file='%s' open='%s' os='%s' len=%d caseRetry=%d",
						filename, caseOpenName, osname, len, casePass));
					// Keep the FS slot reserved while trying another path.
					WF_Close(fsh[f].whandle);
					fsh[f].whandle = -1;
					continue;
				}
				if (traceDefaultCfg)
				{
					XBLog_Write(va("STEFX: FS default.cfg loose OK len=%d caseRetry=%d", len, casePass));
				}
				if (STEFX_ShouldTraceAssetOpen(filename))
				{
					if (STEFX_IsPlayerAnimationCfg(filename))
					{
						XBLog_Write(va("STEFX: FS player animation open file='%s' len=%d caseRetry=%d",
							filename, len, casePass));
					}
					else
					{
						XBLog_Write(va("STEFX: FS loose asset open file='%s' open='%s' os='%s' len=%d caseRetry=%d",
							filename, caseOpenName, osname, len, casePass));
					}
				}
				return len;
			}
		}
	}

#if defined(_XBOX)
	if (traceDefaultCfg)
	{
		XBLog_Write("STEFX: FS default.cfg loose lookup miss");
	}
#endif
	return -1;
#else
	qboolean allowConsoleList = qfalse;
	int fileCode;
#if defined(_XBOX)
	allowConsoleList = ( strstr(filename, "_console_") && strstr(filename, "_list_") );
#endif
	fileCode = allowConsoleList ? 0 : Sys_GetFileCode(filename);
	if (allowConsoleList || fileCode != -1)
	{
		char* osname = FS_BuildOSPath( filename );
		fsh[f].whandle = WF_Open(osname, true, false);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (strstr(filename, ".mdr"))
		{
			XBLog_Write(va("STEFX: FS OS MDR open file='%s' code=%d os='%s' wh=%d", filename, fileCode, osname, fsh[f].whandle));
		}
#endif
		if (fsh[f].whandle >= 0)
		{
			fsh[f].used = qtrue;
			fsh[f].gob = qfalse;
			return FS_filelength(f);
		}
	}
	return -1;
#endif
}


/*
===================
FS_BuildGOBPath

Qpath may have either forward or backwards slashes
===================
*/
#ifndef STEFX_ELITE_FORCE_SP
static char *FS_BuildGOBPath(const char *qpath )
{
	static char path[2][MAX_OSPATH];
	static int toggle;
	
	toggle ^= 1;		// flip-flop to allow two returns without clash

	if (qpath[0] == '\\' || qpath[0] == '/')
	{
		Com_sprintf( path[toggle], sizeof( path[0] ), ".%s", qpath );
	}
	else
	{
		Com_sprintf( path[toggle], sizeof( path[0] ), ".\\%s", qpath );
	}

//	FS_ReplaceSeparators( path[toggle], '\\' );
	FS_ReplaceSeparators( path[toggle] );
	
	return path[toggle];
}


static int FS_FOpenFileReadGOB( const char *filename, fileHandle_t f )
{
	char* gobname = FS_BuildGOBPath( filename );
	if (GOBOpen(gobname, &fsh[f].ghandle) == GOBERR_OK)
	{
		fsh[f].used = qtrue;
		fsh[f].gob = qtrue;
		int len = FS_filelength(f);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (strstr(filename, ".mdr") || strstr(filename, ".md3") ||
			strstr(filename, ".skin") || strstr(filename, "animation.cfg"))
		{
			if (STEFX_IsPlayerAnimationCfg(filename))
			{
				XBLog_Write(va("STEFX: FS player animation GOB file='%s' len=%d", filename, len));
			}
			else
			{
				XBLog_Write(va("STEFX: FS GOB model open file='%s' gob='%s' len=%d", filename, gobname, len));
			}
		}
#endif
		return len;
	}
	return -1;
}
#endif

#if defined(STEFX_ELITE_FORCE_SP)
static long FS_HashFileName( const char *fname, int hashSize )
{
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0')
	{
		letter = tolower(fname[i]);
		if (letter == '.')
		{
			break;
		}
		if (letter == '\\' || letter == PATH_SEP)
		{
			letter = '/';
		}
		hash += (long)(letter) * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (hashSize - 1);
	return hash;
}

static pack_t *FS_LoadZipFile( char *zipfile )
{
	fileInPack_t	*buildBuffer;
	pack_t			*pack;
	unzFile			uf;
	int				err;
	unz_global_info gi;
	char			filename_inzip[MAX_ZPATH];
	unz_file_info	file_info;
	int				i, len;
	long			hash;
	int				fs_numHeaderLongs;
	int				*fs_headerLongs;
	char			*namePtr;

	fs_numHeaderLongs = 0;

	XBLog_Write(va("STEFX: FS PK3 load begin '%s'", zipfile));
	uf = unzOpen(zipfile);
	XBLog_Write(va("STEFX: FS PK3 unzOpen '%s' handle=%p", zipfile, uf));
	if (!uf)
	{
		XBLog_Write(va("STEFX: FS PK3 open failed '%s'", zipfile));
		return NULL;
	}
	err = unzGetGlobalInfo(uf, &gi);
	if (err != UNZ_OK)
	{
		XBLog_Write(va("STEFX: FS PK3 global info failed '%s' err=%d", zipfile, err));
		unzClose(uf);
		return NULL;
	}
	XBLog_Write(va("STEFX: FS PK3 global info '%s' entries=%lu", zipfile, gi.number_entry));

	fs_packFiles += gi.number_entry;

	len = 0;
	unzGoToFirstFile(uf);
	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK)
		{
			break;
		}
		if (file_info.size_filename > MAX_QPATH)
		{
			Com_Error(ERR_FATAL, "ERROR: filename length > MAX_QPATH ( strlen(%s) = %d) \n", filename_inzip, file_info.size_filename);
		}
		len += strlen(filename_inzip) + 1;
		unzGoToNextFile(uf);
	}

	buildBuffer = (fileInPack_t *)Z_Malloc(gi.number_entry * sizeof(fileInPack_t) + len, TAG_FILESYS, qtrue);
	namePtr = ((char *)buildBuffer) + gi.number_entry * sizeof(fileInPack_t);
	fs_headerLongs = (int*)Z_Malloc(gi.number_entry * sizeof(int), TAG_FILESYS, qtrue);

	for (i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1)
	{
		if (i > gi.number_entry)
		{
			break;
		}
	}

	pack = (pack_t*)Z_Malloc(sizeof(pack_t) + i * sizeof(fileInPack_t *), TAG_FILESYS, qtrue);
	memset(pack, 0, sizeof(pack_t) + i * sizeof(fileInPack_t *));
	pack->hashSize = i;
	pack->hashTable = (fileInPack_t **)(((char *)pack) + sizeof(pack_t));
	Q_strncpyz(pack->pakFilename, zipfile, sizeof(pack->pakFilename));
	pack->handle = uf;
	pack->numfiles = gi.number_entry;

	unzGoToFirstFile(uf);
	for (i = 0; i < gi.number_entry; i++)
	{
		err = unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK)
		{
			break;
		}
		if (file_info.uncompressed_size > 0)
		{
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong(file_info.crc);
		}
		Q_strlwr(filename_inzip);
		hash = FS_HashFileName(filename_inzip, pack->hashSize);
		buildBuffer[i].name = namePtr;
		strcpy(buildBuffer[i].name, filename_inzip);
		namePtr += strlen(filename_inzip) + 1;
		unzGetCurrentFileInfoPosition(uf, &buildBuffer[i].pos);
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile(uf);
	}

	pack->checksum = Com_BlockChecksum(fs_headerLongs, 4 * fs_numHeaderLongs);
	pack->checksum = LittleLong(pack->checksum);
	Z_Free(fs_headerLongs);

	pack->buildBuffer = buildBuffer;
	XBLog_Write(va("STEFX: FS loaded PK3 '%s' files=%d checksum=%d", zipfile, pack->numfiles, pack->checksum));
	return pack;
}

static int QDECL paksort( const void *a, const void *b )
{
	char *aa = *(char **)a;
	char *bb = *(char **)b;

	return stricmp(aa, bb);
}

#if defined(STEFX_ELITE_FORCE_SP)
static qboolean FS_TryAddPK3( const char *path, const char *dir, const char *pakName )
{
	searchpath_t	*search;
	pack_t			*pak;
	char			*pakfile;
	wfhandle_t		testHandle;

	pakfile = FS_BuildOSPath(path, dir, pakName);
	testHandle = WF_Open(pakfile, true, false);
	if (testHandle < 0)
	{
		return qfalse;
	}
	WF_Close(testHandle);

	XBLog_Write(va("STEFX: FS PK3 candidate '%s'", pakfile));
	pak = FS_LoadZipFile(pakfile);
	if (!pak)
	{
		XBLog_Write(va("STEFX: FS PK3 not loaded '%s'", pakfile));
		return qfalse;
	}

	search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t), TAG_FILESYS, qtrue);
	search->pack = pak;
	search->dir = 0;
	search->next = fs_searchpaths;
	fs_searchpaths = search;
	return qtrue;
}

static void FS_AddKnownPK3Files( const char *path, const char *dir )
{
	int i;
	int loaded;
	char pakName[32];

	loaded = 0;
	XBLog_Write("STEFX: FS PK3 explicit startup probe begin");

	for (i = 0; i <= 32; ++i)
	{
		Com_sprintf(pakName, sizeof(pakName), "pak%d.pk3", i);
		if (FS_TryAddPK3(path, dir, pakName))
		{
			++loaded;
		}
	}

	for (i = 0; i <= 32; ++i)
	{
		Com_sprintf(pakName, sizeof(pakName), "asset%d.pk3", i);
		if (FS_TryAddPK3(path, dir, pakName))
		{
			++loaded;
		}
	}

	if (FS_TryAddPK3(path, dir, "xbox0.pk3"))
	{
		++loaded;
	}

#if defined(STEFX_SP_HOSTED_MP)
	if (FS_TryAddPK3(path, dir, "xbox1.pk3"))
	{
		++loaded;
	}
#endif

	XBLog_Write(va("STEFX: FS PK3 explicit startup probe done loaded=%d", loaded));
}
#endif

static void FS_AddGameDirectory( const char *path, const char *dir )
{
	int				i;
	searchpath_t	*search;
	pack_t			*pak;
	char			*pakfile;
	int				numfiles;
	char			**pakfiles;
	char			*sorted[MAX_PAKFILES];

	XBLog_Write(va("STEFX: FS add game directory path='%s' dir='%s'", path, dir));
	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	search = (searchpath_t *)Z_Malloc(sizeof(searchpath_t), TAG_FILESYS, qtrue);
	search->dir = (directory_t*)Z_Malloc(sizeof(*search->dir), TAG_FILESYS, qtrue);
	search->pack = 0;
	Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
	Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	XBLog_Write("STEFX: FS PK3 scan active; loose files remain first for EF SP opens");

#if defined(STEFX_ELITE_FORCE_SP)
	FS_AddKnownPK3Files(path, dir);
	return;
#endif

	pakfile = FS_BuildOSPath(path, dir, "");
	pakfile[strlen(pakfile) - 1] = 0;
	XBLog_Write(va("STEFX: FS scanning PK3 directory '%s'", pakfile));
#ifdef _JK2MP
	pakfiles = Sys_ListFiles(pakfile, ".pk3", NULL, &numfiles, qfalse);
#else
	pakfiles = Sys_ListFiles(pakfile, ".pk3", &numfiles, qfalse);
#endif
	XBLog_Write(va("STEFX: FS PK3 scan result dir='%s' count=%d list=%p", pakfile, numfiles, pakfiles));

	if (!pakfiles)
	{
		return;
	}

	if (numfiles > MAX_PAKFILES)
	{
		numfiles = MAX_PAKFILES;
	}
	for (i = 0; i < numfiles; i++)
	{
		sorted[i] = pakfiles[i];
	}

	qsort(sorted, numfiles, sizeof(sorted[0]), paksort);

	for (i = 0; i < numfiles; i++)
	{
		pakfile = FS_BuildOSPath(path, dir, sorted[i]);
		XBLog_Write(va("STEFX: FS PK3 candidate '%s'", pakfile));
		pak = FS_LoadZipFile(pakfile);
		if (!pak)
		{
			XBLog_Write(va("STEFX: FS skipped invalid PK3 '%s'", pakfile));
			continue;
		}
		search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t), TAG_FILESYS, qtrue);
		search->pack = pak;
		search->dir = 0;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}

	Sys_FreeFileList(pakfiles);
}

static qboolean FS_PK3FileExists( const char *filename )
{
	searchpath_t	*search;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	long			hash;
	int				chainCount;
	int				i;
	int				searchCount;
	const qboolean tracePackedProbe = strstr(filename, "/misc.mle") != NULL;

	if (tracePackedProbe)
	{
		XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists begin file='%s' root=%p", filename, fs_searchpaths));
	}

	searchCount = 0;
	for (search = fs_searchpaths; search && searchCount < 2048; search = search->next, ++searchCount)
	{
		if (!search->pack)
		{
			continue;
		}

		pak = search->pack;
		if (tracePackedProbe)
		{
			XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists path=%d search=%p pack=%p file='%s' files=%d hashSize=%d table=%p",
				searchCount, search, pak, pak->pakFilename, pak->numfiles, pak->hashSize, pak->hashTable));
		}
		if (pak->hashSize <= 0 || !pak->hashTable || pak->numfiles < 0)
		{
			XBLog_WriteCritical(va("STEFX: FS invalid PK3 index file='%s' pk3='%s' files=%d hashSize=%d table=%p",
				filename, pak->pakFilename, pak->numfiles, pak->hashSize, pak->hashTable));
			continue;
		}
		hash = FS_HashFileName(filename, pak->hashSize);
		pakFile = pak->hashTable[hash];
		if (tracePackedProbe)
		{
			XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists bucket path=%d hash=%ld head=%p", searchCount, hash, pakFile));
		}
		chainCount = 0;
		while (pakFile && chainCount++ < pak->numfiles)
		{
			if (!FS_FilenameCompare(pakFile->name, filename))
			{
				if (tracePackedProbe)
				{
					XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists found path=%d chain=%d entry='%s'", searchCount, chainCount, pakFile->name));
				}
				return qtrue;
			}
			pakFile = pakFile->next;
		}
		if (tracePackedProbe)
		{
			XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists miss path=%d chain=%d tail=%p", searchCount, chainCount, pakFile));
		}

		if (pakFile)
		{
			XBLog_WriteCritical(va("STEFX: FS PK3 hash cycle file='%s' pk3='%s' hash=%ld files=%d",
				filename, pak->pakFilename, hash, pak->numfiles));
			for (i = 0; i < pak->numfiles; ++i)
			{
				if (!FS_FilenameCompare(pak->buildBuffer[i].name, filename))
				{
					return qtrue;
				}
			}
		}
	}
	if (search)
	{
		XBLog_WriteCritical(va("STEFX: FS search path cycle file='%s' count=%d node=%p", filename, searchCount, search));
	}
	if (tracePackedProbe)
	{
		XBLog_WriteCritical(va("STEFX_RETAIL_LUMPS: PK3 exists end file='%s' found=0 paths=%d", filename, searchCount));
	}

	return qfalse;
}

#if defined(STEFX_ELITE_FORCE_SP)
qboolean FS_PK3PatchFileExists( const char *filename )
{
	return FS_PK3FileExists(filename);
}
#endif

static int FS_FOpenFileReadPK3( const char *filename, fileHandle_t f )
{
	int chainCount;
	int i;
	searchpath_t	*search;
	pack_t			*pak;
	fileInPack_t	*pakFile;
	long			hash;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean traceDefaultCfg = !Q_stricmp(filename, "default.cfg");

	if (traceDefaultCfg)
	{
		XBLog_Write("STEFX: FS default.cfg PK3 lookup begin");
	}
#endif

	for (search = fs_searchpaths; search; search = search->next)
	{
		if (!search->pack)
		{
			continue;
		}

		pak = search->pack;
		hash = FS_HashFileName(filename, pak->hashSize);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (traceDefaultCfg)
		{
			XBLog_Write(va("STEFX: FS default.cfg PK3 scan pk3='%s' hash=%ld files=%d", pak->pakFilename, hash, pak->numfiles));
		}
#endif
		pakFile = pak->hashTable[hash];
		chainCount = 0;
		while (pakFile && chainCount++ < pak->numfiles)
		{
			if (!FS_FilenameCompare(pakFile->name, filename))
			{
				goto foundPakFile;
			}
			pakFile = pakFile->next;
		}

		if (pakFile)
		{
			XBLog_WriteCritical(va("STEFX: FS PK3 open hash cycle file='%s' pk3='%s' hash=%ld files=%d",
				filename, pak->pakFilename, hash, pak->numfiles));
			for (i = 0; i < pak->numfiles; ++i)
			{
				if (!FS_FilenameCompare(pak->buildBuffer[i].name, filename))
				{
					pakFile = &pak->buildBuffer[i];
					goto foundPakFile;
				}
			}
		}
		continue;

foundPakFile:
		{
			unzFile z = unzReOpen(pak->pakFilename, pak->handle);
			unz_s *zfi;
			int len;

			if (!z)
			{
#if defined(STEFX_ELITE_FORCE_SP)
				XBLog_Write(va("STEFX: FS PK3 reopen failed file='%s' pk3='%s'", filename, pak->pakFilename));
				return -1;
#else
				Com_Error(ERR_FATAL, "Couldn't reopen %s", pak->pakFilename);
#endif
			}
			if (unzSetCurrentFileInfoPosition(z, pakFile->pos) != UNZ_OK ||
				unzOpenCurrentFile(z) != UNZ_OK)
			{
				unzClose(z);
				return -1;
			}

			zfi = (unz_s *)z;
			len = zfi->cur_file_info.uncompressed_size;
			fsh[f].handleFiles.file.z = z;
			fsh[f].handleFiles.unique = qtrue;
			fsh[f].used = qtrue;
			fsh[f].zipFile = qtrue;
			fsh[f].fileSize = len;
			fsh[f].zipFilePos = pakFile->pos;
			Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (traceDefaultCfg)
			{
				XBLog_Write(va("STEFX: FS default.cfg PK3 OK pk3='%s' len=%d", pak->pakFilename, len));
			}
#endif
			if (STEFX_ShouldTraceAssetOpen(filename))
			{
				if (STEFX_IsPlayerAnimationCfg(filename))
				{
					XBLog_Write(va("STEFX: FS player animation PK3 file='%s' len=%d", filename, len));
				}
				else
				{
					XBLog_Write(va("STEFX: FS PK3 asset open file='%s' pk3='%s' len=%d", filename, pak->pakFilename, len));
				}
			}
			return len;
		}
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceDefaultCfg)
	{
		XBLog_Write("STEFX: FS default.cfg PK3 lookup miss");
	}
#endif
	return -1;
}

static int FS_ReturnPath( const char *zname, char *zpath, int *depth )
{
	int len, at, newdep;

	newdep = 0;
	zpath[0] = 0;
	len = 0;
	at = 0;

	while (zname[at] != 0)
	{
		if (zname[at] == '/' || zname[at] == '\\')
		{
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy(zpath, zname);
	zpath[len] = 0;
	*depth = newdep;

	return len;
}
#endif


/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
int FS_FOpenFileRead( const char *filename, fileHandle_t *file, qboolean uniqueFILE )
{
	FS_CheckInit();
	qboolean traceDefaultCfg = !Q_stricmp(filename ? filename : "", "default.cfg");
	
	if ( file == NULL ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileRead: NULL 'file' parameter passed\n" );
	}

	if ( !filename ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed\n" );
	}

	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	if ( strstr( filename, ".." ) || strstr( filename, "::" ) ) {
		*file = 0;
		return -1;
	}

	*file = FS_HandleForFile();
	fsh[*file].handleFiles.unique = uniqueFILE;

	int len;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceDefaultCfg)
	{
		XBLog_Write(va("STEFX: FS default.cfg read begin handle=%d unique=%d", *file, (int)uniqueFILE));
	}
#endif
	
#if defined(STEFX_ELITE_FORCE_SP)
	len = FS_FOpenFileReadOS(filename, *file);
	if (len < 0)
	{
		len = FS_FOpenFileReadPK3(filename, *file);
	}
#else
	if (fs_openorder->integer == 0)
	{
		// Release mode -- read from GOB first
		len = FS_FOpenFileReadGOB(filename, *file);
		if (len < 0) len = FS_FOpenFileReadOS(filename, *file);
	}
	else
	{
		// Debug mode -- external files override GOB
		len = FS_FOpenFileReadOS(filename, *file);
		if (len < 0) len = FS_FOpenFileReadGOB(filename, *file);
	}
#endif

	if (len >= 0)
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (traceDefaultCfg)
		{
			XBLog_Write(va("STEFX: FS default.cfg read success len=%d handle=%d", len, *file));
		}
#endif
		return len;
	}

	Com_DPrintf ("Can't find %s\n", filename);
	
	InterlockedExchange((LONG *)&fsh[*file].used, qfalse);
	*file = 0;
	return -1;
}


/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read( void *buffer, int len, fileHandle_t f )
{
	FS_CheckInit();
	FS_CheckUsed(f);

	if ( !f )
	{
		return 0;
	}
	
	if ( f <= 0  || f >= MAX_FILE_HANDLES )
	{
		Com_Error( ERR_FATAL, "FS_Read: Invalid handle %d\n", f );
	}

#if defined(STEFX_ELITE_FORCE_SP)
	if (fsh[f].zipFile)
	{
		return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, len);
	}
	else
	{
		return WF_Read(buffer, len, fsh[f].whandle);
	}
#else
	if (fsh[f].gob)
	{
		GOBUInt32 size = GOBRead(buffer, len, fsh[f].ghandle);
		if (size == GOB_INVALID_SIZE)
		{
#if defined(FINAL_BUILD)
			extern void ERR_DiscFail(bool);
			ERR_DiscFail(false);
#else
			Com_Error( ERR_FATAL, "Failed to read from GOB" );
#endif
		}
		return size;
	}
	else
	{
		return WF_Read(buffer, len, fsh[f].whandle);
	}
#endif
}

/*
	MP has FS_Read2 which is supposed to do some extra logic.
	We don't care, and just call FS_Read()
*/
int FS_Read2( void *buffer, int len, fileHandle_t f )
{
	return FS_Read(buffer, len, f);
}

/*
=================
FS_Write
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t f )
{
	FS_CheckInit();
	FS_CheckUsed(f);

	if ( !f )
	{
		return 0;
	}
	
	if ( f <= 0  || f >= MAX_FILE_HANDLES )
	{
		Com_Error( ERR_FATAL, "FS_Read: Invalid handle %d\n", f );
	}

#if defined(STEFX_ELITE_FORCE_SP)
	if (fsh[f].zipFile)
	{
		Com_Error( ERR_FATAL, "FS_Write: Cannot write to PK3 files %d\n", f );
	}
	else
	{
		return WF_Write(buffer, len, fsh[f].whandle);
	}
#else
	if (fsh[f].gob)
	{
		Com_Error( ERR_FATAL, "FS_Write: Cannot write to GOB files %d\n", f );
	}
	else
	{
		return WF_Write(buffer, len, fsh[f].whandle);
	}
#endif

	return 0;
}


/*
=================
FS_Seek

=================
*/
#if defined(STEFX_ELITE_FORCE_SP)
static qboolean FS_ResetZipCurrentFile(fileHandle_t f)
{
	unzCloseCurrentFile(fsh[f].handleFiles.file.z);

	if (unzSetCurrentFileInfoPosition(fsh[f].handleFiles.file.z, fsh[f].zipFilePos) != UNZ_OK)
	{
#if defined(_XBOX)
		XBLog_Write(va("STEFX: FS PK3 seek reset position failed file='%s'", fsh[f].name));
#endif
		return qfalse;
	}

	if (unzOpenCurrentFile(fsh[f].handleFiles.file.z) != UNZ_OK)
	{
#if defined(_XBOX)
		XBLog_Write(va("STEFX: FS PK3 seek reopen failed file='%s'", fsh[f].name));
#endif
		return qfalse;
	}

	return qtrue;
}

static int FS_DiscardZipBytes(fileHandle_t f, int bytesToDiscard)
{
	int skipped = 0;

	while (bytesToDiscard > 0)
	{
		const int block = bytesToDiscard > STEFX_ZIP_SEEK_SCRATCH_SIZE ? STEFX_ZIP_SEEK_SCRATCH_SIZE : bytesToDiscard;
		const int read = FS_Read(s_stefxZipSeekScratch, block, f);

		if (read <= 0)
		{
#if defined(_XBOX)
			XBLog_Write(va("STEFX: FS PK3 seek discard failed file='%s' wanted=%d skipped=%d read=%d",
				fsh[f].name, bytesToDiscard, skipped, read));
#endif
			return -1;
		}

		skipped += read;
		bytesToDiscard -= read;
	}

	return skipped;
}

static int FS_SeekZipFile(fileHandle_t f, long offset, int origin)
{
	const int current = unztell(fsh[f].handleFiles.file.z);
	int target;

	if (origin == FS_SEEK_CUR && current < 0)
	{
#if defined(_XBOX)
		XBLog_Write(va("STEFX: FS PK3 seek tell failed file='%s' offset=%ld", fsh[f].name, offset));
#endif
		return -1;
	}

	switch (origin)
	{
	case FS_SEEK_SET:
		target = offset;
		break;
	case FS_SEEK_CUR:
		target = current + offset;
		break;
	case FS_SEEK_END:
		target = fsh[f].fileSize + offset;
		break;
	default:
		Com_Error(ERR_FATAL, "Bad origin in FS_Seek\n");
		return -1;
	}

	if (target < 0 || target > fsh[f].fileSize)
	{
#if defined(_XBOX)
		XBLog_Write(va("STEFX: FS PK3 seek out of range file='%s' offset=%ld origin=%d current=%d size=%d target=%d",
			fsh[f].name, offset, origin, current, fsh[f].fileSize, target));
#endif
		return -1;
	}

	if (target < current || origin == FS_SEEK_SET || origin == FS_SEEK_END)
	{
		if (!FS_ResetZipCurrentFile(f))
		{
			return -1;
		}

		if (target == 0)
		{
			return 0;
		}

		return FS_DiscardZipBytes(f, target) < 0 ? -1 : target;
	}

	if (target == current)
	{
		return current;
	}

	return FS_DiscardZipBytes(f, target - current) < 0 ? -1 : target;
}
#endif

int FS_Seek( fileHandle_t f, long offset, int origin )
{
	FS_CheckInit();
	FS_CheckUsed(f);

#if defined(STEFX_ELITE_FORCE_SP)
	if (fsh[f].zipFile)
	{
		return FS_SeekZipFile(f, offset, origin);
	}
	else
	{
		int _origin;
		switch( origin ) {
		case FS_SEEK_CUR: _origin = SEEK_CUR; break;
		case FS_SEEK_END: _origin = SEEK_END; break;
		case FS_SEEK_SET: _origin = SEEK_SET; break;
		default:
			_origin = SEEK_CUR;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek\n" );
			break;
		}

		return WF_Seek(offset, _origin, fsh[f].whandle);
	}
#else
	if (fsh[f].gob)
	{
		int _origin;
		switch( origin ) {
		case FS_SEEK_CUR: _origin = GOBSEEK_CURRENT; break;
		case FS_SEEK_END: _origin = GOBSEEK_END; break;
		case FS_SEEK_SET: _origin = GOBSEEK_START; break;
		default:
			_origin = GOBSEEK_CURRENT;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek\n" );
			break;
		}

		GOBUInt32 pos;
		GOBSeek(fsh[f].ghandle, offset, _origin, &pos);
		return pos;
	}
	else
	{
		int _origin;
		switch( origin ) {
		case FS_SEEK_CUR: _origin = SEEK_CUR; break;
		case FS_SEEK_END: _origin = SEEK_END; break;
		case FS_SEEK_SET: _origin = SEEK_SET; break;
		default:
			_origin = SEEK_CUR;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek\n" );
			break;
		}

		return WF_Seek(offset, _origin, fsh[f].whandle);
	}
#endif
}


/*
=================
FS_Access
=================
*/
qboolean FS_Access( const char *filename )
{
#if defined(STEFX_ELITE_FORCE_SP)
	searchpath_t *search;

	FS_CheckInit();

	for (search = fs_searchpaths; search; search = search->next)
	{
		if (!search->dir)
		{
			continue;
		}

		char* osname = FS_BuildOSPath(search->dir->path, search->dir->gamedir, filename);
		wfhandle_t whandle = WF_Open(osname, true, false);
		if (whandle >= 0)
		{
			WF_Close(whandle);
			return qtrue;
		}
	}

	return FS_PK3FileExists(filename);
#else
	GOBBool status;
	
	FS_CheckInit();

	char* gobname = FS_BuildGOBPath( filename );
	if (GOBAccess(gobname, &status) != GOBERR_OK || status != GOB_TRUE)
	{
		return Sys_GetFileCode( filename ) != -1;
	}

	return qtrue;
#endif
}


/*
======================================================================================

CONVENIENCE FUNCTIONS FOR ENTIRE FILES

======================================================================================
*/

#ifdef _JK2MP
int	FS_FileIsInPAK(const char *filename, int *pChecksum)
#else
int	FS_FileIsInPAK(const char *filename)
#endif
{
	FS_CheckInit();

	if ( !filename ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed\n" );
	}

#ifdef _JK2MP
	*pChecksum = 0;
#endif

#if defined(STEFX_ELITE_FORCE_SP)
	return FS_PK3FileExists(filename) ? 1 : -1;
#else
	GOBBool exists;
	GOBAccess(const_cast<GOBChar*>(filename), &exists);
	return exists ? 1 : -1;
#endif
}

/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile( const char *qpath, void **buffer )
{
	FS_CheckInit();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	const qboolean traceMapBsp = STEFX_IsMapBSP(qpath);
	const char *traceExt = qpath ? strrchr(qpath, '.') : NULL;
	const qboolean traceShaderRead = traceExt && !Q_stricmp(traceExt, ".shader");
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533030; /* 'FS00' */
	}
	if (traceMapBsp)
	{
		XBLog_WriteCritical(va("STEFX_HW_BOOT: FS_ReadFile map begin '%s'", qpath));
	}
#endif
	
	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name\n" );
	}

	// stop sounds from repeating
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FS_MODEL_PHASE( 0x50 );
#endif
	S_ClearSoundBuffer();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FS_MODEL_PHASE( 0x51 );
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533031; /* 'FS01' */
	}
	if (traceMapBsp)
	{
		XBLog_WriteCritical("STEFX_HW_BOOT: FS_ReadFile map sound buffer clear complete");
	}
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	{
		STEFX_FS_MODEL_PHASE( 0x52 );
		int cachedLen = STEFX_ReadPrecachedFile(qpath, buffer);
		STEFX_FS_MODEL_PHASE( 0x53 );
		if (cachedLen >= 0)
		{
			if (traceShaderRead)
			{
				g_SPXBShaderScanMagic = 0x46533032; /* 'FS02' */
			}
			return cachedLen;
		}
	}
#endif

	fileHandle_t h;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FS_MODEL_PHASE( 0x54 );
#endif
	int len = FS_FOpenFileRead( qpath, &h, qfalse );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FS_MODEL_PHASE( 0x55 );
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533033; /* 'FS03' */
	}
	if (traceMapBsp)
	{
		XBLog_WriteCritical(va("STEFX_HW_BOOT: FS_ReadFile map open returned len=%d handle=%d", len, h));
	}
#endif
	if ( h == 0 )
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		len = STEFX_ReadLooseFileWithStdio(qpath, buffer);
		if (len >= 0)
		{
			return len;
		}
#endif
		if ( buffer ) *buffer = NULL;
		return -1;
	}
	
	if ( !buffer )
	{
		FS_FCloseFile(h);
		return len;
	}

	// assume temporary....
	byte* buf;
	int bytesRead;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceMapBsp)
	{
		XBLog_WriteCritical(va("STEFX_HW_BOOT: FS_ReadFile map allocating %d bytes", len + 1));
	}
	STEFX_FS_MODEL_PHASE( 0x56 );
	g_SPXBFileAllocStage = 1;
	g_SPXBFileAllocPathHash = STEFX_FileAllocHash(qpath);
	g_SPXBFileAllocPathPtr = (unsigned int)qpath;
	g_SPXBFileAllocLength = (unsigned int)len;
	const memtag_t wholeFileTag = STEFX_WholeFileTag(qpath);
	g_SPXBFileAllocTag = (unsigned int)wholeFileTag;
	g_SPXBFileAllocStage = 2;
	if (len >= (256 * 1024) || wholeFileTag == TAG_MODEL_MD3 || wholeFileTag == TAG_BSP)
	{
		buf = STEFX_AllocHeapFileBuffer(len, qpath);
	}
	else
	{
		g_SPXBFileAllocStage = 3;
		buf = (byte *)Z_Malloc(len + 1, wholeFileTag, qfalse, 32);
	}
	g_SPXBFileAllocStage = 4;
	STEFX_FS_MODEL_PHASE( 0x57 );
#else
	buf = (byte*)Z_Malloc( len+1, TAG_TEMP_WORKSPACE, qfalse, 32);
#endif
	if (!buf)
	{
		FS_FCloseFile(h);
		*buffer = NULL;
		return -1;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533034; /* 'FS04' */
	}
	if (traceMapBsp)
	{
		XBLog_WriteCritical(va("STEFX_HW_BOOT: FS_ReadFile map allocation complete buffer=%p", buf));
	}
#endif
	buf[len]='\0';

//	Z_Label(buf, qpath);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FS_MODEL_PHASE( 0x58 );
	g_SPXBFileAllocStage = 5;
#endif
	bytesRead = FS_Read(buf, len, h);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBFileAllocStage = 6;
	STEFX_FS_MODEL_PHASE( 0x59 );
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533035; /* 'FS05' */
	}
	if (traceMapBsp)
	{
		XBLog_WriteCritical(va("STEFX_HW_BOOT: FS_ReadFile map read complete expected=%d actual=%d", len, bytesRead));
	}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (STEFX_ShouldTraceAssetOpen(qpath))
	{
		XBLog_Write(va("STEFX: FS whole-file read file='%s' len=%d read=%d handle=%d zip=%d",
			qpath, len, bytesRead, (int)h, fsh[h].zipFile ? 1 : 0));
	}
	if (bytesRead != len)
	{
		qboolean wasHeapFile = FS_STEFX_FreeHeapFileBuffer(buf);
		qboolean wasZipFile = fsh[h].zipFile ? qtrue : qfalse;
		FS_FCloseFile(h);

		if (!wasHeapFile)
		{
			Z_Free(buf);
		}

		if (!wasZipFile && STEFX_ShouldTryStdioWholeFileRead(qpath))
		{
			int retryLen = STEFX_ReadLooseFileWithStdio(qpath, buffer);
			XBLog_Write(va("STEFX: FS whole-file short read retry file='%s' len=%d read=%d retryLen=%d success=%d",
				qpath, len, bytesRead, retryLen, (retryLen >= 0 && buffer && *buffer) ? 1 : 0));
			if (retryLen >= 0)
			{
				return retryLen;
			}
		}

		XBLog_Write(va("STEFX: FS whole-file short read failed file='%s' len=%d read=%d",
			qpath, len, bytesRead));
		*buffer = NULL;
		return -1;
	}
#endif

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBFileAllocStage = 7;
	g_SPXBFSWholeCloseHandle = (unsigned int)h;
	g_SPXBFSWholeCloseStage = 0x46534301u; /* FSC1: whole-file close requested */
#endif
	FS_FCloseFile( h );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBFSWholeCloseStage = 0x46534309u; /* FSC9: whole-file close returned */
	g_SPXBFileAllocStage = 8;
	if (traceShaderRead)
	{
		g_SPXBShaderScanMagic = 0x46533036; /* 'FS06' */
	}
#endif

	*buffer = buf;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (traceMapBsp)
	{
		XBLog_WriteCritical("STEFX_HW_BOOT: FS_ReadFile map complete");
	}
#endif
	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer )
{
	FS_CheckInit();

	if ( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile( NULL )" );
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (FS_STEFX_FreeHeapFileBuffer(buffer))
	{
		return;
	}
#endif

	Z_Free( buffer );
}


int	FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode )
{
	int r;
	qboolean sync;

	FS_CheckInit();

	if ( !f ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileByMode: NULL file handle for '%s'", qpath ? qpath : "(null)" );
		return -1;
	}

	sync = qfalse;
	r = 0;

	switch( mode ) {
	case FS_READ:
		r = FS_FOpenFileRead( qpath, f, qtrue );
		break;
	case FS_WRITE:
		*f = FS_FOpenFileWrite( qpath );
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
		*f = FS_FOpenFileWrite( qpath );
		if ( *f ) {
			WF_Seek( 0, SEEK_END, fsh[*f].whandle );
		}
		break;
	default:
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		XBLF("STEFX: FS_FOpenFileByMode bad mode=%d qpath='%s'", mode, qpath ? qpath : "(null)");
#endif
		Com_Error( ERR_FATAL, "FSH_FOpenFile: bad mode" );
		return -1;
	}

	if ( *f ) {
		fsh[*f].handleSync = sync;
		fsh[*f].fileSize = r;
	}

	return r;
}


int	FS_FTell( fileHandle_t f )
{
	FS_CheckInit();
	FS_CheckUsed(f);

#if defined(STEFX_ELITE_FORCE_SP)
	if (fsh[f].zipFile)
	{
		return unztell(fsh[f].handleFiles.file.z);
	}
	else
	{
		return WF_Tell(fsh[f].whandle);
	}
#else
	if (fsh[f].gob)
	{
		GOBUInt32 pos;
		GOBSeek(fsh[f].ghandle, 0, GOBSEEK_CURRENT, &pos);
		return pos;
	}
	else
	{
		return WF_Tell(fsh[f].whandle);
	}
#endif
}

/*
================
FS_Startup
================
*/
void FS_Startup( const char *gameName )
{
	int f;

	Com_Printf( "----- FS_Startup -----\n" );
#ifdef _XBOX
	XBLog_Write("STEFX_FILE_OWNERSHIP: atomic FS/WF handle reservation enabled");
#endif

	fs_openorder = Cvar_Get( "fs_openorder", "0", 0 );
	fs_debug = Cvar_Get( "fs_debug", "0", 0 );
	fs_copyfiles = Cvar_Get( "fs_copyfiles", "0", CVAR_INIT );
	fs_cdpath = Cvar_Get ("fs_cdpath", Sys_DefaultCDPath(), CVAR_INIT );	
	fs_basepath = Cvar_Get ("fs_basepath", Sys_DefaultBasePath(), CVAR_INIT );
	fs_gamedirvar = Cvar_Get ("fs_game", gameName, CVAR_INIT|CVAR_SERVERINFO );
	fs_restrict = Cvar_Get ("fs_restrict", "", CVAR_INIT );
	Q_strncpyz( fs_gamedir, fs_gamedirvar->string[0] ? fs_gamedirvar->string : gameName, sizeof( fs_gamedir ) );
#ifdef _XBOX
	XBLog_Write(va("EF: FS_Startup basepath='%s' gamedir='%s' fs_game='%s'", fs_basepath->string, fs_gamedir, fs_gamedirvar->string));
#endif

	for (f = 0; f < MAX_FILE_HANDLES; ++f)
	{
		memset(&fsh[f], 0, sizeof(fsh[f]));
	}

#if defined(STEFX_ELITE_FORCE_SP)
	if (fs_cdpath->string[0])
	{
		FS_AddGameDirectory(fs_cdpath->string, gameName);
	}

	FS_AddGameDirectory(fs_basepath->string, gameName);

	if (fs_gamedirvar->string[0] &&
		!Q_stricmp(gameName, BASEGAME) &&
		Q_stricmp(fs_gamedirvar->string, gameName))
	{
		if (fs_cdpath->string[0])
		{
			FS_AddGameDirectory(fs_cdpath->string, fs_gamedirvar->string);
		}
		FS_AddGameDirectory(fs_basepath->string, fs_gamedirvar->string);
	}

	XBLog_Write(va("STEFX: FS loose/PK3 startup complete packFiles=%d", fs_packFiles));
#else
	gi_handles = new gi_handleTable[MAX_FILE_HANDLES];
	for (f = 0; f < MAX_FILE_HANDLES; ++f)
	{
		gi_handles[f].used = false;
	}

	zi_stackBase = (char*)Z_Malloc(ZI_STACKSIZE, TAG_FILESYS, qfalse);

	GOBMemoryFuncSet mem;
	mem.alloc = gi_alloc;
	mem.free = gi_free;
	
	GOBFileSysFuncSet file;
	file.close = gi_close;
	file.open = gi_open;
	file.read = gi_read;
	file.seek = gi_seek;
	file.write = NULL;
	
	GOBCacheFileFuncSet cache;
	cache.close = cache_close;
	cache.open = cache_open;
	cache.read = cache_read;
	cache.seek = cache_seek;
	cache.write = cache_write;

	GOBCodecFuncSet codec = {
		2, // codecs
		{
			{ // Codec 0 - zlib
				'z', GOB_INFINITE_RATIO, // tag, ratio (ratio is meaningless for decomp)
				NULL,
				gi_decompress_zlib,
			},
			{ // Codec 1 - null
				'0', GOB_INFINITE_RATIO, // tag, ratio (ratio is meaningless for decomp)
				NULL,
				gi_decompress_null,
			},
		}
	};

	if (
#ifdef _XBOX
		GOBInit(&mem, &file, &codec, &cache)
#else
		GOBInit(&mem, &file, &codec, NULL)
#endif
		!= GOBERR_OK)
	{
		Com_Error( ERR_FATAL, "Could not initialize GOB" );
	}

	char* archive = FS_BuildOSPath( "assets" );
	if (GOBArchiveOpen(archive, GOBACCESS_READ, GOB_FALSE, GOB_TRUE) != GOBERR_OK)
	{
#if defined(FINAL_BUILD)
		extern void ERR_DiscFail(bool);
		ERR_DiscFail(false);
#else
		Cvar_Set("fs_openorder", "1");
#endif
	}
	
	GOBSetCacheSize(1);
	GOBSetReadBufferSize(32 * 1024);

#ifdef GOB_PROFILE
	GOBProfileFuncSet profile = {
		gi_profileread
	};
	GOBSetProfileFuncs(&profile);
	GOBStartProfile();
#endif
#endif

	Com_Printf( "----------------------\n" );
}

/*
============================

DIRECTORY SCANNING FUCNTIONS

============================
*/

#define	MAX_FOUND_FILES	0x1000

/*
==================
FS_AddFileToList
==================
*/
static int FS_AddFileToList( char *name, char *list[MAX_FOUND_FILES], int nfiles ) {
	int		i;

	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}
	for ( i = 0 ; i < nfiles ; i++ ) {
		if ( !stricmp( name, list[i] ) ) {
			return nfiles;		// allready in list
		}
	}
//	list[nfiles] = CopyString( name );
	list[nfiles] = (char *) Z_Malloc( strlen(name) + 1, TAG_LISTFILES, qfalse );
	strcpy(list[nfiles], name);
	nfiles++;

	return nfiles;
}

/*
===============
FS_ListFiles

Returns a uniqued list of files that match the given criteria
from all search paths
===============
*/
char **FS_ListFiles( const char *path, const char *extension, int *numfiles )
{
	char			*netpath;
	int				numSysFiles;
	char			**sysFiles;
	char			*name;
	int				nfiles = 0;
	char			**listCopy;
	char			*list[MAX_FOUND_FILES];
	int				i;
#if defined(STEFX_ELITE_FORCE_SP)
	searchpath_t	*search;
	int				pathLength;
	int				extensionLength;
	int				length, pathDepth;
	pack_t			*pak;
	fileInPack_t	*buildBuffer;
	char			zpath[MAX_QPATH];
#endif

	FS_CheckInit();

	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}
	if ( !extension ) {
		extension = "";
	}

#if defined(STEFX_ELITE_FORCE_SP)
	pathLength = strlen(path);
	extensionLength = strlen(extension);
	FS_ReturnPath(path, zpath, &pathDepth);

	for (search = fs_searchpaths; search; search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++)
			{
				int zpathLen, depth;

				name = buildBuffer[i].name;
				zpathLen = FS_ReturnPath(name, zpath, &depth);

				if ((depth - pathDepth) > 2 || pathLength > zpathLen || Q_stricmpn(name, path, pathLength))
				{
					continue;
				}

				length = strlen(name);
				if (length < extensionLength)
				{
					continue;
				}
				if (stricmp(name + length - extensionLength, extension))
				{
					continue;
				}

				nfiles = FS_AddFileToList(name + pathLength + 1, list, nfiles);
			}
		}
		else if (search->dir)
		{
			netpath = FS_BuildOSPath(search->dir->path, search->dir->gamedir, path);
#ifdef _JK2MP
			sysFiles = Sys_ListFiles(netpath, extension, NULL, &numSysFiles, qfalse);
#else
			sysFiles = Sys_ListFiles(netpath, extension, &numSysFiles, qfalse);
#endif
			for (i = 0; i < numSysFiles; i++)
			{
				name = sysFiles[i];
				nfiles = FS_AddFileToList(name, list, nfiles);
			}
			Sys_FreeFileList(sysFiles);
		}
	}
#else
	// We don't do any fancy searchpath magic here, it's all in the meta-file
	// that Sys_ListFiles will return
	netpath = FS_BuildOSPath( path );
#ifdef _JK2MP
	sysFiles = Sys_ListFiles( netpath, extension, NULL, &numSysFiles, qfalse );
#else
	sysFiles = Sys_ListFiles( netpath, extension, &numSysFiles, qfalse );
#endif
	for ( i = 0 ; i < numSysFiles ; i++ ) {
		// unique the match
		name = sysFiles[i];
		nfiles = FS_AddFileToList( name, list, nfiles );
	}
	Sys_FreeFileList( sysFiles );
#endif

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ), TAG_LISTFILES, qfalse);
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}


/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList( char **filelist )
{
	int		i;

	FS_CheckInit();

	if ( !filelist ) {
		return;
	}

	for ( i = 0 ; filelist[i] ; i++ ) {
		Z_Free( filelist[i] );
	}

	Z_Free( filelist );
}

/*
===============
FS_AddFileToListBuf
===============
*/
static int FS_AddFileToListBuf( char *name, char *listbuf, int bufsize, int nfiles )
{
	char	*p;

	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}

	if (name[0] == '/' || name[0] == '\\') {
		name++;
	}

	p = listbuf;
	while ( *p ) {
		if ( !stricmp( name, p ) ) {
			return nfiles;		// already in list
		}
		p += strlen( p ) + 1;
	}

	if ( ( p + strlen( name ) + 2 - listbuf ) > bufsize ) {
		return nfiles;		// list is full
	}

	strcpy( p, name );
	p += strlen( p ) + 1;
	*p = 0;

	return nfiles + 1;
}

/*
================
FS_GetFileList

Returns a uniqued list of files that match the given criteria
from all search paths
================
*/
int	FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize )
{
	int				nfiles = 0;
	int				i;
	char			*netpath;
	int				numSysFiles;
	char			**sysFiles;
	char			*name;
#if defined(STEFX_ELITE_FORCE_SP)
	searchpath_t	*search;
	int				pathLength;
	int				extensionLength;
	int				length, pathDepth;
	pack_t			*pak;
	fileInPack_t	*buildBuffer;
	char			zpath[MAX_QPATH];
#endif

	FS_CheckInit();

	if ( !path ) {
		return 0;
	}
	if ( !extension ) {
		extension = "";
	}

	// Prime the file list buffer
	listbuf[0] = '\0';
#if defined(STEFX_ELITE_FORCE_SP)
	pathLength = strlen(path);
	extensionLength = strlen(extension);
	FS_ReturnPath(path, zpath, &pathDepth);

	for (search = fs_searchpaths; search; search = search->next)
	{
		if (search->pack)
		{
			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for (i = 0; i < pak->numfiles; i++)
			{
				int zpathLen, depth;

				name = buildBuffer[i].name;
				zpathLen = FS_ReturnPath(name, zpath, &depth);

				if ((depth - pathDepth) > 2 || pathLength > zpathLen || Q_stricmpn(name, path, pathLength))
				{
					continue;
				}

				length = strlen(name);
				if (length < extensionLength)
				{
					continue;
				}
				if (stricmp(name + length - extensionLength, extension))
				{
					continue;
				}

				nfiles = FS_AddFileToListBuf(name + pathLength + 1, listbuf, bufsize, nfiles);
			}
		}
		else if (search->dir)
		{
			netpath = FS_BuildOSPath(search->dir->path, search->dir->gamedir, path);
#ifdef _JK2MP
			sysFiles = Sys_ListFiles(netpath, extension, NULL, &numSysFiles, qfalse);
#else
			sysFiles = Sys_ListFiles(netpath, extension, &numSysFiles, qfalse);
#endif
			for (i = 0; i < numSysFiles; i++)
			{
				name = sysFiles[i];
				nfiles = FS_AddFileToListBuf(name, listbuf, bufsize, nfiles);
			}
			Sys_FreeFileList(sysFiles);
		}
	}
#else
	netpath = FS_BuildOSPath( path );
#ifdef _JK2MP
	sysFiles = Sys_ListFiles( netpath, extension, NULL, &numSysFiles, qfalse );
#else
	sysFiles = Sys_ListFiles( netpath, extension, &numSysFiles, qfalse );
#endif
	for ( i = 0 ; i < numSysFiles ; i++ ) {
		// unique the match
		name = sysFiles[i];
		nfiles = FS_AddFileToListBuf( name, listbuf, bufsize, nfiles );
	}
	Sys_FreeFileList( sysFiles );
#endif

	return nfiles;
}

/*
=================
 Filesytem STUBS
=================
*/

qboolean FS_ConditionalRestart(int checksumFeed)
{
	return qfalse;
}

void FS_ClearPakReferences(int flags)
{
	return;
}

const char *FS_LoadedPakNames(void)
{
	return "";
}

const char *FS_ReferencedPakNames(void)
{
	return "";
}

void FS_SetRestrictions(void)
{
	return;
}

#ifdef _JK2MP
void FS_Restart(int checksumFeed)
#else
void FS_Restart(void)
#endif
{
	return;
}

qboolean FS_FileExists(const char *file)
{
#if defined(STEFX_ELITE_FORCE_SP)
	return FS_Access(file);
#else
	assert(!"FS_FileExists not implemented on Xbox");
	return qfalse;
#endif
}

void FS_UpdateGamedir(void)
{
	return;
}

void FS_PureServerSetReferencedPaks(const char *pakSums, const char *pakNames)
{
	return;
}

void FS_PureServerSetLoadedPaks(const char *pakSums, const char *pakNames)
{
	return;
}

const char *FS_ReferencedPakChecksums(void)
{
	return "";
}

const char *FS_LoadedPakChecksums(void)
{
	return "";
}
