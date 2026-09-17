// tr_models.c -- model loading and caching

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"

#include "tr_local.h"
#include "MatComp.h"
#include "../qcommon/sstring.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
#include "mdr_frame_codec.h"
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
extern "C" volatile unsigned int g_SPXBWeaponModelTraceStage;
extern "C" volatile unsigned int g_SPXBWeaponModelTracePathHash;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceDiskLen;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceDiskSuccess;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceIdent;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceVersion;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceSize;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceLoaded;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceHandle;
extern "C" volatile unsigned int g_SPXBWeaponModelTraceFailCode;
extern "C" volatile unsigned int g_SPXBModelProbeStage;
extern "C" volatile unsigned int g_SPXBModelProbePathHash;
extern "C" volatile unsigned int g_SPXBModelProbeNamePtr;
extern "C" volatile unsigned int g_SPXBModelProbeFileLen;
extern "C" volatile unsigned int g_SPXBPhaseLast;
#define STEFX_RENDER_MODEL_PHASE(stage) \
	( g_SPXBPhaseLast = 0xF0000000u | ( ( (unsigned int)(stage) & 0xffu ) << 16 ) | \
		( g_SPXBPhaseLast & 0xffffu ) )
#endif

#define	LL(x) x=LittleLong(x)
#ifndef MD4_IDENT
#define MD4_IDENT			(('5'<<24)+('M'<<16)+('D'<<8)+'R')
#endif

void RE_LoadWorldMap_Actual( const char *name, world_t &worldData, int index ); //should only be called for sub-bsp instances

static qboolean R_LoadMD3 (model_t *mod, int lod, void *buffer, const char *name, qboolean &bAlreadyCached );
#ifdef STEFX_ELITE_FORCE_SP
static qboolean R_LoadMDR (model_t *mod, void *buffer, const char *name, qboolean &bAlreadyCached );
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
extern qboolean FS_STEFX_IsHeapFileBuffer(const void *buffer);
extern qboolean FS_STEFX_FreeHeapFileBuffer(void *buffer);
#endif

/*
Ghoul2 Insert Start
*/

typedef	struct modelHash_s
{
	char		name[MAX_QPATH];
	qhandle_t	handle;
	struct		modelHash_s	*next;

}modelHash_t;

#define FILE_HASH_SIZE		1024
static	modelHash_t 		*mhHashTable[FILE_HASH_SIZE];


/*
Ghoul2 Insert End
*/



// This stuff looks a bit messy, but it's kept here as black box, and nothing appears in any .H files for other 
//	modules to worry about. I may make another module for this sometime.
//
typedef pair<int,int> StringOffsetAndShaderIndexDest_t;
typedef vector <StringOffsetAndShaderIndexDest_t> ShaderRegisterData_t;
struct CachedEndianedModelBinary_s
{
	void	*pModelDiskImage;
	int		iAllocSize;		// may be useful for mem-query, but I don't actually need it
#ifdef _XBOX
	qboolean bHeapAllocated;
	qboolean bFileBuffer; // Adopted FS buffer has its own allocation header.
#if defined(STEFX_ELITE_FORCE_SP)
	qboolean bMdrCompacted;
	qboolean bMdrNeedsRegistration;
	const byte *pMdrFrameBase;
	int iMdrFrameBaseCount;
	int iMdrFrameSize;
	int iMdrFrameCount;
	int iMdrFramePatchOffsetsOffset;
	int iMdrFramePatchesOffset;
#endif
#endif
	ShaderRegisterData_t ShaderRegisterData;

	int		iLastLevelUsedOn;

	CachedEndianedModelBinary_s()
	{
		pModelDiskImage = 0;
		iLastLevelUsedOn    = -1;
		iAllocSize = 0;
#ifdef _XBOX
		bHeapAllocated = qfalse;
		bFileBuffer = qfalse;
#if defined(STEFX_ELITE_FORCE_SP)
		bMdrCompacted = qfalse;
		bMdrNeedsRegistration = qfalse;
		pMdrFrameBase = NULL;
		iMdrFrameBaseCount = 0;
		iMdrFrameSize = 0;
		iMdrFrameCount = 0;
		iMdrFramePatchOffsetsOffset = 0;
		iMdrFramePatchesOffset = 0;
#endif
#endif
		ShaderRegisterData.clear();
	}
};
typedef struct CachedEndianedModelBinary_s CachedEndianedModelBinary_t;
typedef map <sstring_t,CachedEndianedModelBinary_t>	CachedModels_t;
													CachedModels_t *CachedModels = NULL;	// the important cache item.

#ifdef _XBOX
#if defined(STEFX_ELITE_FORCE_SP)
static qboolean STEFX_TryStreamMdrFile(const char *name, void **buffer, qboolean *cached);
static void STEFX_ClearMdrFrameCache(void);
static void STEFX_FreeMdrFrameBases(void);
static void STEFX_ResetMdrModelRegistry(void);
#endif
static void RE_RegisterModels_FreeDiskImage(CachedEndianedModelBinary_t &cachedModel)
{
	if (!cachedModel.pModelDiskImage)
	{
		return;
	}

	if (cachedModel.bFileBuffer)
	{
		// Only adopted FS buffers carry the header read by this function.
		// Probing before a plain HeapAlloc block can cross an unmapped page.
		FS_STEFX_FreeHeapFileBuffer(cachedModel.pModelDiskImage);
	}
	else if (cachedModel.bHeapAllocated)
	{
		HeapFree(GetProcessHeap(), 0, cachedModel.pModelDiskImage);
	}
	else
	{
		Z_Free(cachedModel.pModelDiskImage);
	}

	cachedModel.pModelDiskImage = NULL;
	cachedModel.bHeapAllocated = qfalse;
	cachedModel.bFileBuffer = qfalse;
#if defined(STEFX_ELITE_FORCE_SP)
	cachedModel.bMdrCompacted = qfalse;
	cachedModel.bMdrNeedsRegistration = qfalse;
	cachedModel.pMdrFrameBase = NULL;
	cachedModel.iMdrFrameBaseCount = 0;
	cachedModel.iMdrFrameSize = 0;
	cachedModel.iMdrFrameCount = 0;
	cachedModel.iMdrFramePatchOffsetsOffset = 0;
	cachedModel.iMdrFramePatchesOffset = 0;
	STEFX_ClearMdrFrameCache();
#endif
}
#endif

void RE_RegisterModels_StoreShaderRequest(const char *psModelFileName, const char *psShaderName, const int *piShaderIndexPoke)
{
	char sModelName[MAX_QPATH];

	Q_strncpyz(sModelName,psModelFileName,sizeof(sModelName));
	Q_strlwr  (sModelName);

	CachedEndianedModelBinary_t &ModelBin = (*CachedModels)[sModelName];

	if (ModelBin.pModelDiskImage == NULL)
	{	
		assert(0);	// should never happen, means that we're being called on a model that wasn't loaded
	}
	else
	{
		const int iNameOffset =		  psShaderName		- (char *)ModelBin.pModelDiskImage;
		const int iPokeOffset = (char*) piShaderIndexPoke	- (char *)ModelBin.pModelDiskImage;

		ModelBin.ShaderRegisterData.push_back( StringOffsetAndShaderIndexDest_t( iNameOffset,iPokeOffset) );
	}
}


static const byte FakeGLAFile[] = 
{
0x32, 0x4C, 0x47, 0x41, 0x06, 0x00, 0x00, 0x00, 0x2A, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x01, 0x00, 0x00, 0x00,
0x14, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x18, 0x01, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00,
0x26, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x4D, 0x6F, 0x64, 0x56, 0x69, 0x65, 0x77, 0x20,
0x69, 0x6E, 0x74, 0x65, 0x72, 0x6E, 0x61, 0x6C, 0x20, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74,
0x00, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD,
0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFD, 0xBF, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F,
0x00, 0x80, 0x00, 0x80, 0x00, 0x80
};

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean STEFX_IsWeaponDiskModelName(const char *name)
{
	return (name && (strstr(name, "models/weapons2/") || strstr(name, "models\\weapons2\\")));
}

static unsigned int STEFX_ModelTraceHash(const char *name)
{
	unsigned int hash = 5381;
	const unsigned char *p = (const unsigned char *)name;

	while (p && *p)
	{
		hash = ((hash << 5) + hash) ^ *p++;
	}

	return hash;
}
#endif

// returns qtrue if loaded, and sets the supplied qbool to true if it was from cache (instead of disk)
//   (which we need to know to avoid LittleLong()ing everything again (well, the Mac needs to know anyway)...
//
qboolean RE_RegisterModels_GetDiskFile( const char *psModelFileName, void **ppvBuffer, qboolean *pqbAlreadyCached)
{
	char sModelName[MAX_QPATH];
	int len;

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x40 );
#endif
	Q_strncpyz(sModelName,psModelFileName,sizeof(sModelName));
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x41 );
#endif
	Q_strlwr  (sModelName);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x42 );
#endif

	CachedEndianedModelBinary_t &ModelBin = (*CachedModels)[sModelName];
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x43 );
#endif

	if (ModelBin.pModelDiskImage == NULL)
	{
		// didn't have it cached, so try the disk...
		//

			// special case intercept first...
			//
			if (!strcmp(sDEFAULT_GLA_NAME ".gla" , psModelFileName))
			{
				// return fake params as though it was found on disk...
				//
				void *pvFakeGLAFile = Z_Malloc( sizeof(FakeGLAFile), TAG_FILESYS, qfalse );
				memcpy(pvFakeGLAFile, &FakeGLAFile[0],  sizeof(FakeGLAFile));
				*ppvBuffer = pvFakeGLAFile;
				*pqbAlreadyCached = qfalse;	// faking it like this should mean that it works fine on the Mac as well
				return qtrue;	
			}

		if ( ppvBuffer )
		{
			*ppvBuffer = NULL;
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			STEFX_RENDER_MODEL_PHASE( 0x44 );
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ppvBuffer && STEFX_TryStreamMdrFile(sModelName, ppvBuffer, pqbAlreadyCached))
			return qtrue;
#endif
		len = FS_ReadFile( psModelFileName, ppvBuffer );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_RENDER_MODEL_PHASE( 0x45 );
		if ( len <= 0 && strcmp( sModelName, psModelFileName ) )
		{
			if ( ppvBuffer )
			{
				*ppvBuffer = NULL;
			}
			STEFX_RENDER_MODEL_PHASE( 0x46 );
			int lowerLen = FS_ReadFile( sModelName, ppvBuffer );
			STEFX_RENDER_MODEL_PHASE( 0x47 );
			if ( strstr( psModelFileName, "models/players/" ) ||
				strstr( psModelFileName, "models\\players\\" ) ||
				STEFX_IsWeaponDiskModelName( psModelFileName ) )
			{
				XBLog_WriteCriticalf( "STEFX_MODEL_BOOT: disk lower retry original='%s' lower='%s' len=%d success=%d buffer=%p",
					psModelFileName,
					sModelName,
					lowerLen,
					(ppvBuffer && *ppvBuffer) ? 1 : 0,
					ppvBuffer ? *ppvBuffer : NULL );
			}
			if ( lowerLen > 0 || (ppvBuffer && *ppvBuffer) )
			{
				len = lowerLen;
			}
		}
#endif
		*pqbAlreadyCached = qfalse;

		const bool bSuccess = !!(*ppvBuffer);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( strstr( psModelFileName, "models/players/" ) ||
			strstr( psModelFileName, "models\\players\\" ) ||
			STEFX_IsWeaponDiskModelName( psModelFileName ) )
		{
			XBLog_WriteCriticalf( "STEFX_MODEL_BOOT: disk fetch '%s' cacheKey='%s' len=%d success=%d buffer=%p",
				psModelFileName,
				sModelName,
				len,
				bSuccess ? 1 : 0,
				*ppvBuffer );
		}
		if ( STEFX_IsWeaponDiskModelName( psModelFileName ) )
		{
			g_SPXBWeaponModelTraceStage = 1;
			g_SPXBWeaponModelTracePathHash = STEFX_ModelTraceHash( psModelFileName );
			g_SPXBWeaponModelTraceDiskLen = (unsigned int)len;
			g_SPXBWeaponModelTraceDiskSuccess = bSuccess ? 1 : 0;
			g_SPXBWeaponModelTraceFailCode = bSuccess ? 0 : 1;
		}
#endif

		return bSuccess;
	}
	else
	{
		*ppvBuffer = ModelBin.pModelDiskImage;
		*pqbAlreadyCached = qtrue;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (STEFX_IsWeaponDiskModelName(psModelFileName))
		{
			g_SPXBWeaponModelTraceStage = 2;
			g_SPXBWeaponModelTracePathHash = STEFX_ModelTraceHash( psModelFileName );
			g_SPXBWeaponModelTraceDiskSuccess = 1;
			g_SPXBWeaponModelTraceFailCode = 0;
			XBLF("STEFX: model disk cache hit '%s' size=%d heap=%d buffer=%p",
				psModelFileName,
				ModelBin.iAllocSize,
				ModelBin.bHeapAllocated ? 1 : 0,
				ModelBin.pModelDiskImage);
		}
#endif
		return qtrue;
	}
}


// if return == true, no further action needed by the caller...
//
void *RE_RegisterModels_Malloc(int iSize, void *pvDiskBufferIfJustLoaded, const char *psModelFileName, qboolean *pqbAlreadyFound, memtag_t eTag)
{
	char sModelName[MAX_QPATH];

	Q_strncpyz(sModelName,psModelFileName,sizeof(sModelName));
	Q_strlwr  (sModelName);

	CachedEndianedModelBinary_t &ModelBin = (*CachedModels)[sModelName];

	if (ModelBin.pModelDiskImage == NULL)
	{
		// ... then this entry has only just been created, ie we need to load it fully...
		//
		// new, instead of doing a Z_Malloc and assigning that we just morph the disk buffer alloc
		//	then don't thrown it away on return - cuts down on mem overhead
		//
		// ... groan, but not if doing a limb hierarchy creation (some VV stuff?), in which case it's NULL
		//			
#ifndef _XBOX
		if ( pvDiskBufferIfJustLoaded )
		{
			Z_MorphMallocTag( pvDiskBufferIfJustLoaded, eTag );
		}
		else
#endif
		{
#ifdef _XBOX
			if (eTag == TAG_MODEL_MD3 || eTag == TAG_MODEL_GLM || eTag == TAG_MODEL_GLA)
			{
				int nameLen = strlen(sModelName);
#if defined(STEFX_ELITE_FORCE_SP)
				if (pvDiskBufferIfJustLoaded &&
					nameLen >= 4 &&
					!stricmp(&sModelName[nameLen - 4], ".mdr"))
				{
					ModelBin.bHeapAllocated = FS_STEFX_IsHeapFileBuffer(pvDiskBufferIfJustLoaded);
					ModelBin.bFileBuffer = ModelBin.bHeapAllocated;
#ifdef _XBOX
					if (strstr(sModelName, "models/players/"))
					{
						XBLF("STEFX: RE_RegisterModels_Malloc adopted MDR disk buffer model='%s' size=%d tag=%d heap=%d",
							sModelName, iSize, eTag, ModelBin.bHeapAllocated ? 1 : 0);
					}
#endif
				}
				else
#endif
				{
					pvDiskBufferIfJustLoaded = HeapAlloc(GetProcessHeap(), 0, iSize);
					if (!pvDiskBufferIfJustLoaded)
					{
#if defined(STEFX_ELITE_FORCE_SP)
						XBLF("STEFX: RE_RegisterModels_Malloc heap failed model='%s' size=%d tag=%d; falling back to zone",
							sModelName, iSize, eTag);
						ModelBin.bHeapAllocated = qfalse;
						pvDiskBufferIfJustLoaded = Z_Malloc(iSize, eTag, qfalse);
						if (!pvDiskBufferIfJustLoaded)
						{
							XBLF("STEFX: RE_RegisterModels_Malloc zone fallback failed model='%s' size=%d tag=%d; returning bad model",
								sModelName, iSize, eTag);
							ModelBin.pModelDiskImage = NULL;
							ModelBin.iAllocSize = 0;
							ModelBin.bHeapAllocated = qfalse;
							*pqbAlreadyFound = qfalse;
							return NULL;
						}
#else
						pvDiskBufferIfJustLoaded = Z_Malloc(iSize, eTag, qfalse);
						ModelBin.bHeapAllocated = qfalse;
#endif
					}
					else
					{
						ModelBin.bHeapAllocated = qtrue;
					}
				}
			}
			else
#endif
			{
			pvDiskBufferIfJustLoaded =  Z_Malloc(iSize,eTag, qfalse );
			}
		}

		ModelBin.pModelDiskImage= pvDiskBufferIfJustLoaded;
		ModelBin.iAllocSize		= iSize;
		*pqbAlreadyFound		= qfalse;
	}
	else
	{
#ifdef _XBOX
		if (eTag == TAG_MODEL_GLA && strstr(sModelName, "_humanoid"))
		{
			Com_PrintfAlways("JA: RE_RegisterModels_Malloc cache hit '%s' size=%d tag=%d\n",
				sModelName, ModelBin.iAllocSize, eTag);
		}
#endif
		// if we already had this model entry, then re-register all the shaders it wanted...
		//
		const int iEntries = ModelBin.ShaderRegisterData.size();
		for (int i=0; i<iEntries; i++)
		{
			int iShaderNameOffset	= ModelBin.ShaderRegisterData[i].first;
			int iShaderPokeOffset	= ModelBin.ShaderRegisterData[i].second;

			const char *const psShaderName	 =		   &((char*)ModelBin.pModelDiskImage)[iShaderNameOffset];
				  int  *const piShaderPokePtr= (int *) &((char*)ModelBin.pModelDiskImage)[iShaderPokeOffset];

			shader_t *sh = R_FindShader( psShaderName, lightmapsNone, stylesDefault, qtrue );
	            
			if ( sh->defaultShader ) 
			{
				*piShaderPokePtr = 0;
			} else {
				*piShaderPokePtr = sh->index;
			}
		}
		*pqbAlreadyFound = qtrue;	// tell caller not to re-Endian or re-Shader this binary
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (ModelBin.bMdrNeedsRegistration)
		{
			*pqbAlreadyFound = qfalse;
			ModelBin.bMdrNeedsRegistration = qfalse;
		}
#endif
	}

	ModelBin.iLastLevelUsedOn = RE_RegisterMedia_GetLevel();

	return ModelBin.pModelDiskImage;
}


// dump any models not being used by this level if we're running low on memory...
//
static int GetModelDataAllocSize(void)
{
	return	Z_MemSize( TAG_MODEL_MD3) +
			Z_MemSize( TAG_MODEL_GLM) +
			Z_MemSize( TAG_MODEL_GLA);
}

static int GetCachedModelDataAllocSize(int *heapBytes, int *zoneBytes, int *modelCount)
{
	int totalBytes = 0;

	if (heapBytes)
	{
		*heapBytes = 0;
	}
	if (zoneBytes)
	{
		*zoneBytes = 0;
	}
	if (modelCount)
	{
		*modelCount = 0;
	}
	if (!CachedModels)
	{
		return 0;
	}

	for (CachedModels_t::iterator itModel = CachedModels->begin(); itModel != CachedModels->end(); ++itModel)
	{
		CachedEndianedModelBinary_t &cachedModel = (*itModel).second;
		if (!cachedModel.pModelDiskImage || cachedModel.iAllocSize <= 0)
		{
			continue;
		}

		totalBytes += cachedModel.iAllocSize;
		if (modelCount)
		{
			++(*modelCount);
		}
#ifdef _XBOX
		if (cachedModel.bHeapAllocated)
		{
			if (heapBytes)
			{
				*heapBytes += cachedModel.iAllocSize;
			}
		}
		else
#endif
		{
			if (zoneBytes)
			{
				*zoneBytes += cachedModel.iAllocSize;
			}
		}
	}

	return totalBytes;
}
extern cvar_t *r_modelpoolmegs;
//
// return qtrue if at least one cached model was freed (which tells z_malloc()-fail recovery code to try again)
//
extern qboolean gbInsideRegisterModel;
qboolean RE_RegisterModels_LevelLoadEnd(qboolean bDeleteEverythingNotUsedThisLevel /* = qfalse */)
{	
	qboolean bAtLeastoneModelFreed = qfalse;

	if (gbInsideRegisterModel)
	{
		Com_DPrintf( "(Inside RE_RegisterModel (z_malloc recovery?), exiting...\n");
	}
	else
	{
		int iHeapModelBytes = 0;
		int iZoneCachedModelBytes = 0;
		int iCachedModelCount = 0;
		int iLoadedModelBytes	=	GetCachedModelDataAllocSize(&iHeapModelBytes, &iZoneCachedModelBytes, &iCachedModelCount);
		const int iMaxModelBytes=	r_modelpoolmegs->integer * 1024 * 1024;

		qboolean bEraseOccured = qfalse;
		int iFreedModelBytes = 0;
		int iFreedModelCount = 0;
#ifdef _XBOX
		const int iZoneTaggedModelBytes = GetModelDataAllocSize();
#endif
		for (CachedModels_t::iterator itModel = CachedModels->begin(); itModel != CachedModels->end() && ( bDeleteEverythingNotUsedThisLevel || iLoadedModelBytes > iMaxModelBytes ); bEraseOccured?itModel:++itModel)
		{			
			bEraseOccured = qfalse;

			CachedEndianedModelBinary_t &CachedModel = (*itModel).second;
			const int iCachedAllocSize = CachedModel.iAllocSize;

			qboolean bDeleteThis = qfalse;

			if (bDeleteEverythingNotUsedThisLevel)
			{
				bDeleteThis = (CachedModel.iLastLevelUsedOn != RE_RegisterMedia_GetLevel());
			}
			else
			{
				bDeleteThis = (CachedModel.iLastLevelUsedOn < RE_RegisterMedia_GetLevel());
			}

			// if it wasn't used on this level, dump it...
			//
			if (bDeleteThis)
			{
	#ifdef _DEBUG
//				LPCSTR psModelName = (*itModel).first.c_str();
//				VID_Printf( PRINT_DEVELOPER, "Dumping \"%s\"", psModelName);
//				VID_Printf( PRINT_DEVELOPER, ", used on lvl %d\n",CachedModel.iLastLevelUsedOn);
	#endif				

				if (CachedModel.pModelDiskImage) {
#ifdef _XBOX
					RE_RegisterModels_FreeDiskImage(CachedModel);
#else
					Z_Free(CachedModel.pModelDiskImage);	
#endif
					//CachedModel.pModelDiskImage = NULL;	// REM for reference, erase() call below negates the need for it.
					bAtLeastoneModelFreed = qtrue;
					iFreedModelBytes += iCachedAllocSize;
					++iFreedModelCount;
				}

				itModel = CachedModels->erase(itModel);
				bEraseOccured = qtrue;

				iLoadedModelBytes = GetCachedModelDataAllocSize(&iHeapModelBytes, &iZoneCachedModelBytes, &iCachedModelCount);
			}
		}
#ifdef _XBOX
		if (iFreedModelCount > 0 || iLoadedModelBytes > iMaxModelBytes)
		{
			XBLF("STEFX: model cache level-end level=%d cached=%d heap=%d zoneCache=%d zoneTagged=%d cap=%d freed=%d freedBytes=%d force=%d overBudget=%d",
				RE_RegisterMedia_GetLevel(),
				iLoadedModelBytes,
				iHeapModelBytes,
				iZoneCachedModelBytes,
				iZoneTaggedModelBytes,
				iMaxModelBytes,
				iFreedModelCount,
				iFreedModelBytes,
				bDeleteEverythingNotUsedThisLevel ? 1 : 0,
				iLoadedModelBytes > iMaxModelBytes ? 1 : 0);
		}
#endif
	}

	//VID_Printf( PRINT_DEVELOPER, "RE_RegisterModels_LevelLoadEnd(): Ok\n");	

	return bAtLeastoneModelFreed;	
}

void RE_RegisterModels_Info_f( void )
{	
	int iTotalBytes = 0;
	if(!CachedModels) {
		Com_Printf ("%d bytes total (%.2fMB)\n",iTotalBytes, (float)iTotalBytes / 1024.0f / 1024.0f);
		return;
	}

	int iModels = CachedModels->size();
	int iModel  = 0;

	for (CachedModels_t::iterator itModel = CachedModels->begin(); itModel != CachedModels->end(); ++itModel,iModel++)
	{	
		CachedEndianedModelBinary_t &CachedModel = (*itModel).second;

		VID_Printf( PRINT_ALL, "%d/%d: \"%s\" (%d bytes)",iModel,iModels,(*itModel).first.c_str(),CachedModel.iAllocSize );

		#ifdef _DEBUG
		VID_Printf( PRINT_ALL, ", lvl %d\n",CachedModel.iLastLevelUsedOn);
		#endif

		iTotalBytes += CachedModel.iAllocSize;
	}
	VID_Printf( PRINT_ALL, "%d bytes total (%.2fMB)\n",iTotalBytes, (float)iTotalBytes / 1024.0f / 1024.0f);
}


static void RE_RegisterModels_DeleteAll(void)
{
	for (CachedModels_t::iterator itModel = CachedModels->begin(); itModel != CachedModels->end(); )
	{
		CachedEndianedModelBinary_t &CachedModel = (*itModel).second;

		if (CachedModel.pModelDiskImage) {
#ifdef _XBOX
			RE_RegisterModels_FreeDiskImage(CachedModel);
#else
			Z_Free(CachedModel.pModelDiskImage);					
#endif
		}

		itModel = CachedModels->erase(itModel);			
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_FreeMdrFrameBases();
#endif

	extern void RE_AnimationCFGs_DeleteAll(void);
	RE_AnimationCFGs_DeleteAll();
}


static int giRegisterMedia_CurrentLevel=0;
static qboolean gbAllowScreenDissolve = qtrue;
#ifdef _XBOX
extern bool g_xboxDirectMapBootQueued;
extern bool Sys_IsDirectMapBoot(void);
#endif
//
// param "bAllowScreenDissolve" is just a convenient way of getting hold of a bool which can be checked by the code that
//	issues the InitDissolve command later in RE_RegisterMedia_LevelLoadEnd()
//
void RE_RegisterMedia_LevelLoadBegin(const char *psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve)
{
	gbAllowScreenDissolve = bAllowScreenDissolve;

	tr.numBSPModels = 0;

	// for development purposes we may want to ditch certain media just before loading a map...
	//
	switch (eForceReload)
	{
		case eForceReload_BSP:

			CM_DeleteCachedMap(qtrue);
			R_Images_DeleteLightMaps();
			break;

		case eForceReload_MODELS:

			RE_RegisterModels_DeleteAll();
			break;

		case eForceReload_ALL:

			// BSP...
			//
			CM_DeleteCachedMap(qtrue);
			R_Images_DeleteLightMaps();
			//
			// models...
			//
			RE_RegisterModels_DeleteAll();
			break;
	}

	// at some stage I'll probably want to put some special logic here, like not incrementing the level number
	//	when going into a map like "brig" or something, so returning to the previous level doesn't require an 
	//	asset reload etc, but for now...
	//
	// only bump level number if we're not on the same level. 
	//	Note that this will hide uncached models, which is perhaps a bad thing?...
	//
	static char sPrevMapName[MAX_QPATH]={0};
	if (Q_stricmp( psMapName,sPrevMapName ))
	{
		Q_strncpyz( sPrevMapName, psMapName, sizeof(sPrevMapName) );
		giRegisterMedia_CurrentLevel++;
	}
}

int RE_RegisterMedia_GetLevel(void)
{
	return giRegisterMedia_CurrentLevel;
}

extern qboolean SND_RegisterAudio_LevelLoadEnd(qboolean bDeleteEverythingNotUsedThisLevel);

void RE_RegisterMedia_LevelLoadEnd(void)
{
	RE_RegisterModels_LevelLoadEnd(qfalse);
	RE_RegisterImages_LevelLoadEnd();
	SND_RegisterAudio_LevelLoadEnd(qfalse);

#ifdef _XBOX
	if (Sys_IsDirectMapBoot())
	{
		gbAllowScreenDissolve = qfalse;
	}
#endif
	if (gbAllowScreenDissolve)
	{
		RE_InitDissolve(qfalse);
	}

	S_RestartMusic();
	
	extern qboolean gbAlreadyDoingLoad;
					gbAlreadyDoingLoad = qfalse;
}




/*
** R_GetModelByHandle
*/
model_t	*R_GetModelByHandle( qhandle_t index ) {
	model_t		*mod;

	// out of range gets the defualt model
	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel( void ) {
	model_t		*mod;

	if ( tr.numModels == MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = (model_t*) Hunk_Alloc( sizeof( *tr.models[tr.numModels] ), qtrue );
	mod->index= tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

/*
Ghoul2 Insert Start
*/

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname, const int size ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (size-1);
	return hash;
}

void RE_InsertModelIntoHash(const char *name, model_t *mod)
{
	int			hash;
	modelHash_t	*mh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	// insert this file into the hash table so we can look it up faster later
	mh = (modelHash_t*)Hunk_Alloc( sizeof( modelHash_t ), qtrue );

	mh->next = mhHashTable[hash];
	mh->handle = mod->index;
	strcpy(mh->name, name);
	mhHashTable[hash] = mh;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static void STEFX_InsertModelHandleAliasIntoHash(const char *name, qhandle_t handle)
{
	int			hash;
	modelHash_t	*mh;

	if (!name || handle <= 0)
	{
		return;
	}

	hash = generateHashValue(name, FILE_HASH_SIZE);
	mh = (modelHash_t*)Hunk_Alloc( sizeof( modelHash_t ), qtrue );
	mh->next = mhHashTable[hash];
	mh->handle = handle;
	strcpy(mh->name, name);
	mhHashTable[hash] = mh;
}
#endif

#ifdef STEFX_ELITE_FORCE_SP
static qboolean STEFX_IsMdrModelName(const char *name)
{
	const char *ext = name ? strrchr(name, '.') : NULL;
	return (ext && !Q_stricmp(ext, ".mdr"));
}

#if defined(_XBOX)
static qhandle_t RE_RegisterModel_Actual( const char *name );
#endif

static qboolean STEFX_IsGhoul2ModelName(const char *name)
{
	const char *ext = name ? strrchr(name, '.') : NULL;
	return (ext && (!Q_stricmp(ext, ".glm") || !Q_stricmp(ext, ".gla")));
}

#if defined(_XBOX)
static qboolean STEFX_ShouldUseMdrMemoryPlaceholder(const char *name, int size);
static void STEFX_LogMdrMemoryStats(const char *phase, const char *name, int fileLen, int requestSize, int realSize, int alignPad, int fit);

static qboolean STEFX_IsBorgPlayerModelName(const char *name)
{
	return (name && (strstr(name, "models/players/borg") || strstr(name, "models\\players\\borg")));
}

static qboolean STEFX_IsPlayerModelName(const char *name)
{
	return (name && (strstr(name, "models/players/") || strstr(name, "models\\players\\")));
}

static const char *STEFX_ModelPartToken(const char *name)
{
	if (!name)
	{
		return NULL;
	}
	if (strstr(name, "/lower.") || strstr(name, "\\lower."))
	{
		return "lower.";
	}
	if (strstr(name, "/upper.") || strstr(name, "\\upper."))
	{
		return "upper.";
	}
	if (strstr(name, "/head.") || strstr(name, "\\head."))
	{
		return "head.";
	}
	return NULL;
}

static const char *STEFX_DefaultPlayerMdrFallbackName(const char *name)
{
	const char *part = STEFX_ModelPartToken(name);
	if (!STEFX_IsPlayerModelName(name) || !STEFX_IsMdrModelName(name) || !part)
	{
		return NULL;
	}

	if (!Q_stricmp(part, "lower."))
	{
		return "models/players/hazard/lower.mdr";
	}
	if (!Q_stricmp(part, "upper."))
	{
		return "models/players/hazard/upper.mdr";
	}

	return NULL;
}

static qboolean STEFX_IsDefaultPlayerMdrFallbackName(const char *name)
{
	const char *fallback;

	fallback = STEFX_DefaultPlayerMdrFallbackName(name);
	return (qboolean)(fallback && !Q_stricmp(name, fallback));
}

#define STEFX_BORG_MDR_FRAME_GROUPS 5
#define STEFX_BORG_MDR_FRAME_CACHE_SLOTS 32
#define STEFX_BORG_MDR_MAX_FRAME_BYTES 1024
#define STEFX_MDR_BLOCK_FRAMES 16
static byte s_stefxMdrBlockRaw[STEFX_MDR_BLOCK_FRAMES * STEFX_BORG_MDR_MAX_FRAME_BYTES];
static byte s_stefxMdrBlockPacked[sizeof(s_stefxMdrBlockRaw) * 2 + 32];
static int s_stefxMdrBlockDictionary[4096];
static const md4Header_t *s_stefxMdrBlockHeader;
static int s_stefxMdrBlockIndex = -1;

typedef struct
{
	byte *frames;
	qboolean heapAllocated;
	int frameCount;
	int frameSize;
	int numBones;
	char source[MAX_QPATH];
} stefxBorgMdrFrameBase_t;

typedef struct
{
	const md4Header_t *header;
	int frame;
	byte data[STEFX_BORG_MDR_MAX_FRAME_BYTES];
} stefxBorgMdrFrameCache_t;

static stefxBorgMdrFrameBase_t s_stefxBorgMdrFrameBases[STEFX_BORG_MDR_FRAME_GROUPS];
static stefxBorgMdrFrameCache_t s_stefxBorgMdrFrameCache[STEFX_BORG_MDR_FRAME_CACHE_SLOTS];
static int s_stefxBorgMdrFrameCacheReplace;
#if defined(STEFX_ELITE_FORCE_SP)
static int s_stefxMdrFrameLastReturned = -1;
extern "C" volatile unsigned int g_SPXBMdrFramePairProtected = 0;
// STEFX_MDR_FRAME_REPLACEMENT_BEGIN
static int STEFX_MdrFrameReplacement(int next, int lastReturned)
{
	// RB_SurfaceAnim keeps the current frame pointer while fetching the old
	// frame. A hit on the FIFO eviction slot must survive that following miss.
	return next == lastReturned ? (next + 1) % STEFX_BORG_MDR_FRAME_CACHE_SLOTS : next;
}
// STEFX_MDR_FRAME_REPLACEMENT_END
#endif
static model_t *s_stefxBorgMdrModels[MAX_MOD_KNOWN];
static int s_stefxBorgMdrModelCount;

extern void *Z_TryMalloc(int size, memtag_t tag, int alignment);
static byte *STEFX_AllocMdrStorage(int bytes, qboolean *heapAllocated)
{
	byte *memory = (byte *)HeapAlloc(GetProcessHeap(), 0, bytes);
	*heapAllocated = memory ? qtrue : qfalse;
	if (!memory) memory = (byte *)Z_TryMalloc(bytes, TAG_MODEL_MD3, 32);
	XBLog_WriteCriticalf("STEFX_MDR_STORAGE: bytes=%d heap=%d allocated=%d", bytes, *heapAllocated, memory ? 1 : 0);
	return memory;
}

static void STEFX_FreeMdrStorage(byte *memory, qboolean heapAllocated)
{
	if (!memory) return;
	if (heapAllocated) HeapFree(GetProcessHeap(), 0, memory);
	else Z_Free(memory);
}

static void STEFX_ClearMdrFrameCache(void)
{
	s_stefxMdrBlockHeader = NULL;
	s_stefxMdrBlockIndex = -1;
	memset(s_stefxBorgMdrFrameCache, 0, sizeof(s_stefxBorgMdrFrameCache));
	s_stefxBorgMdrFrameCacheReplace = 0;
#if defined(STEFX_ELITE_FORCE_SP)
	s_stefxMdrFrameLastReturned = -1;
#endif
}

static void STEFX_ResetMdrModelRegistry(void)
{
	memset(s_stefxBorgMdrModels, 0, sizeof(s_stefxBorgMdrModels));
	s_stefxBorgMdrModelCount = 0;
	STEFX_ClearMdrFrameCache();
}

static void STEFX_FreeMdrFrameBases(void)
{
	int i;
	for (i = 0; i < STEFX_BORG_MDR_FRAME_GROUPS; ++i)
	{
		if (s_stefxBorgMdrFrameBases[i].frames)
		{
			STEFX_FreeMdrStorage(s_stefxBorgMdrFrameBases[i].frames,
				s_stefxBorgMdrFrameBases[i].heapAllocated);
		}
		memset(&s_stefxBorgMdrFrameBases[i], 0, sizeof(s_stefxBorgMdrFrameBases[i]));
	}
	STEFX_ResetMdrModelRegistry();
}

static qboolean STEFX_PathHasModelDirectory(const char *name, const char *directory)
{
	char slashToken[MAX_QPATH];
	char backslashToken[MAX_QPATH];
	Com_sprintf(slashToken, sizeof(slashToken), "/%s/", directory);
	Com_sprintf(backslashToken, sizeof(backslashToken), "\\%s\\", directory);
	return (qboolean)(strstr(name, slashToken) != NULL || strstr(name, backslashToken) != NULL);
}

static int STEFX_BorgMdrFrameGroup(const char *name)
{
	const qboolean lower = (qboolean)(strstr(name, "/lower.mdr") != NULL ||
		strstr(name, "\\lower.mdr") != NULL);
	const qboolean upper = (qboolean)(strstr(name, "/upper.mdr") != NULL ||
		strstr(name, "\\upper.mdr") != NULL);

	if (!STEFX_IsBorgPlayerModelName(name) || (!lower && !upper))
	{
		return -1;
	}

	if (lower)
	{
		// The unnumbered big/thin pair has a distinct lower-body animation
		// stream.  The remaining Borg lowers are losslessly close to one base.
		if (STEFX_PathHasModelDirectory(name, "borgbig") ||
			STEFX_PathHasModelDirectory(name, "borgthin"))
		{
			return 1;
		}
		return 0;
	}

	if (STEFX_PathHasModelDirectory(name, "borgbig2") ||
		STEFX_PathHasModelDirectory(name, "borgbig3") ||
		STEFX_PathHasModelDirectory(name, "borgbig4"))
	{
		return 2;
	}
	if (STEFX_PathHasModelDirectory(name, "borgbig") ||
		STEFX_PathHasModelDirectory(name, "borgthin"))
	{
		return 3;
	}
	return 4;
}

static unsigned short STEFX_ReadPatchU16(const byte *p)
{
	unsigned short value;
	memcpy(&value, p, sizeof(value));
	return value;
}

static void STEFX_WritePatchU16(byte *p, unsigned short value)
{
	memcpy(p, &value, sizeof(value));
}

static int STEFX_MdrFramePatchSize(const byte *target, const byte *base, int frameSize)
{
	int i = 0;
	int bytes = sizeof(unsigned short);

	while (i < frameSize)
	{
		int start;
		while (i < frameSize && target[i] == base[i])
		{
			++i;
		}
		if (i >= frameSize)
		{
			break;
		}
		start = i;
		while (i < frameSize && target[i] != base[i])
		{
			++i;
		}
		bytes += (int)(sizeof(unsigned short) * 2) + (i - start);
	}
	return bytes;
}

static int STEFX_WriteMdrFramePatch(byte *out, const byte *target, const byte *base, int frameSize)
{
	int i = 0;
	int cursor = sizeof(unsigned short);
	unsigned short runCount = 0;

	while (i < frameSize)
	{
		int start;
		int length;
		while (i < frameSize && target[i] == base[i])
		{
			++i;
		}
		if (i >= frameSize)
		{
			break;
		}
		start = i;
		while (i < frameSize && target[i] != base[i])
		{
			++i;
		}
		length = i - start;
		STEFX_WritePatchU16(out + cursor, (unsigned short)start);
		cursor += sizeof(unsigned short);
		STEFX_WritePatchU16(out + cursor, (unsigned short)length);
		cursor += sizeof(unsigned short);
		memcpy(out + cursor, target + start, length);
		cursor += length;
		++runCount;
	}

	STEFX_WritePatchU16(out, runCount);
	return cursor;
}

static void STEFX_AssignMdrFrameStorage(model_t *mod, const CachedEndianedModelBinary_t &modelBin)
{
	int i;
	byte *allocation = (byte *)modelBin.pModelDiskImage;

	mod->stefxMdrFrameBase = modelBin.pMdrFrameBase;
	mod->stefxMdrFrameBaseCount = modelBin.iMdrFrameBaseCount;
	mod->stefxMdrFrameSize = modelBin.iMdrFrameSize;
	mod->stefxMdrFrameCount = modelBin.iMdrFrameCount;
	mod->stefxMdrFramePatchOffsets = modelBin.iMdrFramePatchOffsetsOffset
		? (const unsigned int *)(allocation + modelBin.iMdrFramePatchOffsetsOffset)
		: NULL;
	mod->stefxMdrFramePatches = modelBin.iMdrFramePatchesOffset
		? allocation + modelBin.iMdrFramePatchesOffset
		: NULL;

	for (i = 0; i < s_stefxBorgMdrModelCount; ++i)
	{
		if (s_stefxBorgMdrModels[i] == mod)
		{
			return;
		}
	}
	if (s_stefxBorgMdrModelCount < MAX_MOD_KNOWN)
	{
		s_stefxBorgMdrModels[s_stefxBorgMdrModelCount++] = mod;
	}
}

static model_t *STEFX_FindMdrModel(const md4Header_t *header)
{
	int i;
	// Prefer the newest registration if an allocator address was reused after
	// an old cached model was released.
	for (i = s_stefxBorgMdrModelCount - 1; i >= 0; --i)
	{
		model_t *mod = s_stefxBorgMdrModels[i];
		if (mod && mod->md4 == header && mod->stefxMdrFrameBase)
		{
			return mod;
		}
	}
	return NULL;
}

const void *R_STEFX_GetMDRFrame(const md4Header_t *header, int frame)
{
	model_t *mod = STEFX_FindMdrModel(header);
	int i;

	if (!mod || !mod->stefxMdrFrameBase || mod->stefxMdrFrameSize <= 0)
	{
		int frameSize;
		if (header->ofsFrames < 0)
		{
			frameSize = (int)(&((md4CompFrame_t *)0)->bones[header->numBones]);
			return (const byte *)header - header->ofsFrames + frame * frameSize;
		}
		frameSize = (int)(&((md4Frame_t *)0)->bones[header->numBones]);
		return (const byte *)header + header->ofsFrames + frame * frameSize;
	}

	if (frame < 0)
	{
		frame = 0;
	}
	else if (frame >= mod->stefxMdrFrameCount)
	{
		frame = mod->stefxMdrFrameCount - 1;
	}

	if (!mod->stefxMdrFramePatchOffsets || !mod->stefxMdrFramePatches)
	{
		return mod->stefxMdrFrameBase + frame * mod->stefxMdrFrameSize;
	}

	for (i = 0; i < STEFX_BORG_MDR_FRAME_CACHE_SLOTS; ++i)
	{
		if (s_stefxBorgMdrFrameCache[i].header == header &&
			s_stefxBorgMdrFrameCache[i].frame == frame)
		{
#if defined(STEFX_ELITE_FORCE_SP)
			s_stefxMdrFrameLastReturned = i;
#endif
			return s_stefxBorgMdrFrameCache[i].data;
		}
	}

	{
#if defined(STEFX_ELITE_FORCE_SP)
		int replacement = STEFX_MdrFrameReplacement(s_stefxBorgMdrFrameCacheReplace,
			s_stefxMdrFrameLastReturned);
		if (replacement != s_stefxBorgMdrFrameCacheReplace) {
			if (!g_SPXBMdrFramePairProtected)
				XBLog_WriteCritical("STEFX_MDR_FRAME_PAIR: protected live frame from FIFO eviction");
			++g_SPXBMdrFramePairProtected;
		}
		s_stefxBorgMdrFrameCacheReplace = replacement;
		s_stefxMdrFrameLastReturned = replacement;
#endif
		stefxBorgMdrFrameCache_t *cache =
			&s_stefxBorgMdrFrameCache[s_stefxBorgMdrFrameCacheReplace];
		const byte *patch;
		unsigned short runCount;
		int run;

		s_stefxBorgMdrFrameCacheReplace =
			(s_stefxBorgMdrFrameCacheReplace + 1) % STEFX_BORG_MDR_FRAME_CACHE_SLOTS;
		cache->header = header;
		cache->frame = frame;
		if (mod->stefxMdrFrameBaseCount == -STEFX_MDR_BLOCK_FRAMES)
		{
			const int block = frame / STEFX_MDR_BLOCK_FRAMES;
			if (s_stefxMdrBlockHeader != header || s_stefxMdrBlockIndex != block)
			{
				int frames = mod->stefxMdrFrameCount - block * STEFX_MDR_BLOCK_FRAMES;
				if (frames > STEFX_MDR_BLOCK_FRAMES) frames = STEFX_MDR_BLOCK_FRAMES;
				const int bytes = frames * mod->stefxMdrFrameSize;
				unsigned int offset = mod->stefxMdrFramePatchOffsets[block];
				unsigned int next = mod->stefxMdrFramePatchOffsets[block + 1];
				const byte *source = mod->stefxMdrFramePatches + offset;
				const int capacity = mod->dataSize - (mod->stefxMdrFramePatches - (const byte *)header);
				if (capacity <= 0 || next > (unsigned int)capacity || next <= offset ||
					!StefxMdrFrame::Decode(source, next - offset, s_stefxMdrBlockRaw,
					 bytes, mod->stefxMdrFrameSize, s_stefxMdrBlockPacked, sizeof(s_stefxMdrBlockPacked)))
					Com_Error(ERR_DROP, "Invalid MDR animation block %s:%d", mod->name, block);
				s_stefxMdrBlockHeader = header;
				s_stefxMdrBlockIndex = block;
			}
			memcpy(cache->data, s_stefxMdrBlockRaw +
				(frame % STEFX_MDR_BLOCK_FRAMES) * mod->stefxMdrFrameSize, mod->stefxMdrFrameSize);
			return cache->data;
		}
		patch = mod->stefxMdrFramePatches + mod->stefxMdrFramePatchOffsets[frame];
		if (frame < mod->stefxMdrFrameBaseCount)
		{
			memcpy(cache->data,
				mod->stefxMdrFrameBase + frame * mod->stefxMdrFrameSize,
				mod->stefxMdrFrameSize);
		}
		else
		{
			memset(cache->data, 0, mod->stefxMdrFrameSize);
		}

		runCount = STEFX_ReadPatchU16(patch);
		patch += sizeof(unsigned short);
		for (run = 0; run < runCount; ++run)
		{
			unsigned short offset = STEFX_ReadPatchU16(patch);
			unsigned short length = STEFX_ReadPatchU16(patch + sizeof(unsigned short));
			patch += sizeof(unsigned short) * 2;
			if ((int)offset + (int)length <= mod->stefxMdrFrameSize)
			{
				memcpy(cache->data + offset, patch, length);
			}
			patch += length;
		}
		return cache->data;
	}
}

static md4Header_t *STEFX_RegisterCompactBorgMdr(model_t *mod, void *buffer,
	const char *modelName, qboolean *alreadyFound)
{
	char cacheName[MAX_QPATH];
	CachedEndianedModelBinary_t *modelBin;
	md4Header_t *source = (md4Header_t *)buffer;
	stefxBorgMdrFrameBase_t *base;
	const byte *sourceFrames;
	int group;
	int frameSize;
	int frameBytes;
	int compactBytes;
	int patchTableBytes = 0;
	int patchBytes = 0;
	int allocBytes;
	int frame;
	qboolean newBase = qfalse;
	qboolean allocationOnHeap = qfalse;
	byte *allocation;
	md4Header_t *compact;

	Q_strncpyz(cacheName, modelName, sizeof(cacheName));
	Q_strlwr(cacheName);
	modelBin = &(*CachedModels)[cacheName];
	if (modelBin->pModelDiskImage)
	{
		if (!modelBin->bMdrCompacted)
		{
			return NULL;
		}
		*alreadyFound = qtrue;
		STEFX_AssignMdrFrameStorage(mod, *modelBin);
		return (md4Header_t *)modelBin->pModelDiskImage;
	}

	group = STEFX_BorgMdrFrameGroup(cacheName);
	if (group < 0 || source->ofsFrames >= 0 || source->numFrames <= 0 ||
		source->numBones <= 0 || source->ofsLODs <= (int)sizeof(md4Header_t) ||
		source->ofsTags < source->ofsLODs || source->ofsEnd < source->ofsTags)
	{
		return NULL;
	}

	frameBytes = source->ofsLODs - (-source->ofsFrames);
	if (frameBytes <= 0 || frameBytes % source->numFrames)
	{
		return NULL;
	}
	frameSize = frameBytes / source->numFrames;
	if (frameSize <= 0 || frameSize > 640)
	{
		return NULL;
	}
	sourceFrames = (const byte *)source - source->ofsFrames;
	base = &s_stefxBorgMdrFrameBases[group];
	if (!base->frames)
	{
		base->frames = STEFX_AllocMdrStorage(frameBytes, &base->heapAllocated);
		if (!base->frames)
		{
			return NULL;
		}
		memcpy(base->frames, sourceFrames, frameBytes);
		base->frameCount = source->numFrames;
		base->frameSize = frameSize;
		base->numBones = source->numBones;
		Q_strncpyz(base->source, cacheName, sizeof(base->source));
		newBase = qtrue;
		XBLF("STEFX: Borg MDR lossless frame base group=%d model='%s' frames=%d frameSize=%d bytes=%d",
			group, cacheName, base->frameCount, base->frameSize, frameBytes);
	}
	else if (base->frameSize != frameSize || base->numBones != source->numBones)
	{
		XBLF("STEFX: Borg MDR frame group mismatch group=%d model='%s' base='%s' bones=%d/%d frameSize=%d/%d",
			group, cacheName, base->source, source->numBones, base->numBones,
			frameSize, base->frameSize);
		return NULL;
	}

	compactBytes = sizeof(md4Header_t) + (source->ofsEnd - source->ofsLODs);
	if (!newBase)
	{
		static const byte zeroFrame[STEFX_BORG_MDR_MAX_FRAME_BYTES] = { 0 };
		patchTableBytes = (source->numFrames + 1) * sizeof(unsigned int);
		for (frame = 0; frame < source->numFrames; ++frame)
		{
			const byte *baseFrame = frame < base->frameCount
				? base->frames + frame * frameSize
				: zeroFrame;
			patchBytes += STEFX_MdrFramePatchSize(
				sourceFrames + frame * frameSize, baseFrame, frameSize);
		}
	}
	allocBytes = compactBytes + patchTableBytes + patchBytes;
	allocation = STEFX_AllocMdrStorage(allocBytes, &allocationOnHeap);
	if (!allocation)
	{
		// A new base has no consumers until this first compact model succeeds.
		if (newBase)
		{
			STEFX_FreeMdrStorage(base->frames, base->heapAllocated);
			memset(base, 0, sizeof(*base));
		}
		XBLF("STEFX: Borg MDR compact allocation failed model='%s' compact=%d patch=%d total=%d",
			cacheName, compactBytes, patchTableBytes + patchBytes, allocBytes);
		return NULL;
	}

	compact = (md4Header_t *)allocation;
	memcpy(compact, source, sizeof(md4Header_t));
	memcpy(allocation + sizeof(md4Header_t),
		(const byte *)source + source->ofsLODs,
		source->ofsEnd - source->ofsLODs);
	compact->ofsLODs = sizeof(md4Header_t);
	compact->ofsTags = sizeof(md4Header_t) + (source->ofsTags - source->ofsLODs);
	compact->ofsEnd = compactBytes;

	// Each MDR surface points back to its owning header.  Relocating the LOD
	// tail requires only this back-reference adjustment; all other surface and
	// LOD offsets remain relative to their copied records.
	{
		md4LOD_t *lod = (md4LOD_t *)(allocation + compact->ofsLODs);
		int lodIndex;
		for (lodIndex = 0; lodIndex < compact->numLODs; ++lodIndex)
		{
			md4Surface_t *surface = (md4Surface_t *)((byte *)lod + lod->ofsSurfaces);
			int surfaceIndex;
			for (surfaceIndex = 0; surfaceIndex < lod->numSurfaces; ++surfaceIndex)
			{
				surface->ofsHeader = -(int)((byte *)surface - allocation);
				surface = (md4Surface_t *)((byte *)surface + surface->ofsEnd);
			}
			lod = (md4LOD_t *)((byte *)lod + lod->ofsEnd);
		}
	}

	if (!newBase)
	{
		static const byte zeroFrame[STEFX_BORG_MDR_MAX_FRAME_BYTES] = { 0 };
		unsigned int *offsets = (unsigned int *)(allocation + compactBytes);
		byte *patchStart = allocation + compactBytes + patchTableBytes;
		int cursor = 0;
		for (frame = 0; frame < source->numFrames; ++frame)
		{
			const byte *baseFrame = frame < base->frameCount
				? base->frames + frame * frameSize
				: zeroFrame;
			offsets[frame] = cursor;
			cursor += STEFX_WriteMdrFramePatch(patchStart + cursor,
				sourceFrames + frame * frameSize, baseFrame, frameSize);
		}
		offsets[source->numFrames] = cursor;
	}

	modelBin->pModelDiskImage = compact;
	modelBin->iAllocSize = allocBytes;
	modelBin->bHeapAllocated = allocationOnHeap;
	modelBin->bFileBuffer = qfalse;
	modelBin->bMdrCompacted = qtrue;
	modelBin->pMdrFrameBase = base->frames;
	modelBin->iMdrFrameBaseCount = base->frameCount;
	modelBin->iMdrFrameSize = frameSize;
	modelBin->iMdrFrameCount = source->numFrames;
	modelBin->iMdrFramePatchOffsetsOffset = newBase ? 0 : compactBytes;
	modelBin->iMdrFramePatchesOffset = newBase ? 0 : compactBytes + patchTableBytes;
	*alreadyFound = qfalse;
	STEFX_AssignMdrFrameStorage(mod, *modelBin);
	XBLF("STEFX: Borg MDR compact exact model='%s' original=%d resident=%d geometry=%d patch=%d baseNew=%d saved=%d",
		cacheName, source->ofsEnd, allocBytes, compactBytes,
		patchTableBytes + patchBytes, newBase ? frameBytes : 0,
		source->ofsEnd - allocBytes);
	return compact;
}
#endif

static qboolean STEFX_RegisterGhoul2Disabled(model_t *mod, const char *name)
{
	if (!STEFX_IsGhoul2ModelName(name))
	{
		return qfalse;
	}

	mod->type = MOD_BAD;
	RE_InsertModelIntoHash(name, mod);
#ifdef _XBOX
	XBLF("STEFX: Ghoul2 model disabled '%s'", name ? name : "(null)");
#endif
	return qtrue;
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean STEFX_TryStreamMdrFile(const char *name, void **buffer, qboolean *cached)
{
	if (!STEFX_IsPlayerModelName(name) || !STEFX_IsMdrModelName(name) ||
		STEFX_IsBorgPlayerModelName(name)) return qfalse;
	fileHandle_t file = 0;
	int length = FS_FOpenFileByMode(name, &file, FS_READ);
	md4Header_t header;
	if (!file) return qfalse;
	if (length < 131072 || FS_Read(&header, sizeof(header), file) != sizeof(header) ||
		header.ident != MD4_IDENT || header.version != MD4_VERSION ||
		header.numFrames <= 0 || header.numFrames > 65536 || header.numBones <= 0 ||
		header.numBones > MD4_MAX_BONES || header.ofsFrames != -(int)sizeof(header) ||
		header.ofsEnd != length || header.ofsTags < header.ofsLODs ||
		header.ofsEnd < header.ofsTags || header.numLODs <= 0 || header.numLODs > 16)
	{
		FS_FCloseFile(file);
		return qfalse;
	}
	const int frameSize = 40 + header.numBones * 24;
	if (frameSize > STEFX_BORG_MDR_MAX_FRAME_BYTES ||
		header.ofsLODs != sizeof(header) + header.numFrames * frameSize)
	{
		FS_FCloseFile(file);
		return qfalse;
	}
	const int blocks = (header.numFrames + STEFX_MDR_BLOCK_FRAMES - 1) / STEFX_MDR_BLOCK_FRAMES;
	const int geometry = sizeof(header) + length - header.ofsLODs;
	const int tableBytes = (blocks + 1) * sizeof(unsigned int);
	int packedBytes = 0, block;
	bool transforms = true;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	transforms = Cvar_Get("stefx_mdr_frame_codec", "1", 0)->integer != 0;
#endif
	byte *transformWork = s_stefxMdrBlockPacked;
	byte *encodedWork = s_stefxMdrBlockPacked + sizeof(s_stefxMdrBlockRaw);
	const int encodedCapacity = sizeof(s_stefxMdrBlockPacked) - sizeof(s_stefxMdrBlockRaw);
	qboolean valid = qtrue;
	// A size pass avoids retaining the complete source file beside the packed
	// model. Both passes read sequentially and use bounded reusable workspace.
	s_stefxMdrBlockHeader = NULL;
	for (block = 0; block < blocks; ++block)
	{
		int frames = header.numFrames - block * STEFX_MDR_BLOCK_FRAMES;
		if (frames > STEFX_MDR_BLOCK_FRAMES) frames = STEFX_MDR_BLOCK_FRAMES;
		int bytes = frames * frameSize;
		if (FS_Read(s_stefxMdrBlockRaw, bytes, file) != bytes) { valid = qfalse; break; }
		int encoded = StefxMdrFrame::Encode(s_stefxMdrBlockRaw, bytes, frameSize, transforms,
			NULL, 0, transformWork, encodedWork, encodedCapacity, s_stefxMdrBlockDictionary);
		if (!encoded) { valid = qfalse; break; }
		packedBytes += encoded;
	}
	const int allocationBytes = geometry + tableBytes + packedBytes;
	if (!valid || allocationBytes >= length - 8192)
	{
		FS_FCloseFile(file);
		return qfalse;
	}
	qboolean heap = qfalse;
	byte *allocation = STEFX_AllocMdrStorage(allocationBytes, &heap);
	if (!allocation) { FS_FCloseFile(file); return qfalse; }
	memcpy(allocation, &header, sizeof(header));
	if (FS_Read(allocation + sizeof(header), length - header.ofsLODs, file) != length - header.ofsLODs)
		valid = qfalse;
	md4Header_t *compact = (md4Header_t *)allocation;
	compact->ofsLODs = sizeof(header);
	compact->ofsTags = sizeof(header) + header.ofsTags - header.ofsLODs;
	compact->ofsEnd = geometry;
	int lodAt = compact->ofsLODs;
	for (int lodIndex = 0; valid && lodIndex < header.numLODs; ++lodIndex)
	{
		if (lodAt < (int)sizeof(header) || lodAt > compact->ofsTags - (int)sizeof(md4LOD_t)) { valid = qfalse; break; }
		md4LOD_t *lod = (md4LOD_t *)(allocation + lodAt);
		if (lod->ofsEnd <= 0 || lod->ofsEnd > compact->ofsTags - lodAt || lod->numSurfaces < 0) { valid = qfalse; break; }
		int surfaceAt = lodAt + lod->ofsSurfaces;
		for (int surfaceIndex = 0; surfaceIndex < lod->numSurfaces; ++surfaceIndex)
		{
			if (surfaceAt < lodAt || surfaceAt > lodAt + lod->ofsEnd - (int)sizeof(md4Surface_t)) { valid = qfalse; break; }
			md4Surface_t *surface = (md4Surface_t *)(allocation + surfaceAt);
			if (surface->ofsEnd <= 0 || surface->ofsEnd > lodAt + lod->ofsEnd - surfaceAt) { valid = qfalse; break; }
			surface->ofsHeader = -surfaceAt;
			surfaceAt += surface->ofsEnd;
		}
		lodAt += lod->ofsEnd;
	}
	if (lodAt != compact->ofsTags) valid = qfalse;
	FS_Seek(file, sizeof(header), FS_SEEK_SET);
	unsigned int *offsets = (unsigned int *)(allocation + geometry);
	byte *packed = allocation + geometry + tableBytes;
	int cursor = 0;
	for (block = 0; valid && block < blocks; ++block)
	{
		int frames = header.numFrames - block * STEFX_MDR_BLOCK_FRAMES;
		if (frames > STEFX_MDR_BLOCK_FRAMES) frames = STEFX_MDR_BLOCK_FRAMES;
		int bytes = frames * frameSize;
		if (FS_Read(s_stefxMdrBlockRaw, bytes, file) != bytes) { valid = qfalse; break; }
		int count = StefxMdrFrame::Encode(s_stefxMdrBlockRaw, bytes, frameSize, transforms,
			packed + cursor, packedBytes - cursor, transformWork, encodedWork, encodedCapacity,
			s_stefxMdrBlockDictionary);
		if (!count) { valid = qfalse; break; }
		offsets[block] = cursor;
		cursor += count;
	}
	FS_FCloseFile(file);
	if (!valid || cursor != packedBytes)
	{
		STEFX_FreeMdrStorage(allocation, heap);
		XBLog_WriteCriticalf("STEFX_MDR_BLOCK: rejected incomplete model='%s'", name);
		return qfalse;
	}
	offsets[blocks] = cursor;
	CachedEndianedModelBinary_t &entry = (*CachedModels)[name];
	entry.pModelDiskImage = compact;
	entry.iAllocSize = allocationBytes;
	entry.bHeapAllocated = heap;
	entry.bFileBuffer = qfalse;
	entry.bMdrCompacted = qtrue;
	entry.bMdrNeedsRegistration = qtrue;
	entry.pMdrFrameBase = allocation;
	entry.iMdrFrameBaseCount = -STEFX_MDR_BLOCK_FRAMES;
	entry.iMdrFrameSize = frameSize;
	entry.iMdrFrameCount = header.numFrames;
	entry.iMdrFramePatchOffsetsOffset = geometry;
	entry.iMdrFramePatchesOffset = geometry + tableBytes;
	*buffer = compact;
	*cached = qfalse;
	XBLog_WriteCriticalf("STEFX_MDR_BLOCK: model='%s' original=%d resident=%d saved=%d blocks=%d",
		name, length, allocationBytes, length - allocationBytes, blocks);
	return qtrue;
}
#endif

static qhandle_t STEFX_RegisterMdrPlaceholderIfPresent(model_t *mod, const char *name)
{
	fileHandle_t f = 0;
	unsigned int ident = 0;
	int len;
	int read;

	g_SPXBModelProbeStage = 2;
	g_SPXBModelProbeNamePtr = (unsigned int)name;
	g_SPXBModelProbePathHash = STEFX_ModelTraceHash(name);
	if (!STEFX_IsMdrModelName(name))
	{
		g_SPXBModelProbeStage = 3;
		return 0;
	}

	g_SPXBModelProbeStage = 4;
	STEFX_RENDER_MODEL_PHASE( 0x20 );
	len = FS_FOpenFileByMode(name, &f, FS_READ);
	g_SPXBModelProbeFileLen = (unsigned int)len;
	g_SPXBModelProbeStage = 5;
	STEFX_RENDER_MODEL_PHASE( 0x21 );
	if (len < 4 || !f)
	{
#ifdef _XBOX
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR probe missing '%s' len=%d handle=%d",
			name ? name : "(null)", len, f);
#endif
		if (f)
		{
			FS_FCloseFile(f);
		}
		return 0;
	}

#if defined(_XBOX)
	if (STEFX_IsPlayerModelName(name) && !STEFX_IsBorgPlayerModelName(name))
	{
		int requestSize = len + 1;
		int realSize = 0;
		int alignPad = 0;
		int largestFreeBlock = 0;
		qboolean wouldFit = Z_WouldAllocFit(requestSize, TAG_MODEL_MD3, 32, &realSize, &alignPad, &largestFreeBlock);

		STEFX_LogMdrMemoryStats("preflight", name, len, requestSize, realSize, alignPad, wouldFit ? 1 : 0);

		if (!wouldFit)
		{
			// FS_ReadFile tries page-backed and heap storage before its zone
			// fallback. A zone-only estimate must not poison the model cache.
			XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR zone estimate insufficient '%s' len=%d request=%d real=%d largest=%d shortfall=%d; trying file allocators",
				name,
				len,
				requestSize,
				realSize,
				largestFreeBlock,
				(realSize > largestFreeBlock) ? (realSize - largestFreeBlock) : 0);
		}
	}

	if (!STEFX_ShouldUseMdrMemoryPlaceholder(name, len))
	{
#ifdef _XBOX
		if (name && (strstr(name, "models/players/borg") || strstr(name, "models\\players\\borg")))
		{
			XBLF("STEFX: RE_RegisterModel Borg MDR placeholder bypass '%s' len=%d", name, len);
		}
#endif
		STEFX_RENDER_MODEL_PHASE( 0x22 );
		FS_FCloseFile(f);
		STEFX_RENDER_MODEL_PHASE( 0x23 );
		return 0;
	}
#else
	FS_FCloseFile(f);
	return 0;
#endif

	read = FS_Read(&ident, 4, f);
	FS_FCloseFile(f);
	ident = LittleLong(ident);

	if (read != 4 || ident != MD4_IDENT)
	{
#ifdef _XBOX
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR probe rejected '%s' read=%d ident=0x%08x", name, read, ident);
#endif
		return 0;
	}

	mod->type = MOD_STEFX_MDR_PLACEHOLDER;
	mod->dataSize += len;
	mod->numLods = 1;
	RE_InsertModelIntoHash(name, mod);
#ifdef _XBOX
	XBLog_WriteCriticalf("STEFX_MODEL_BOOT: accepted MDR placeholder '%s' handle=%d len=%d", name, mod->index, len);
#endif
	return mod->index;
}

#if defined(_XBOX)
static qboolean STEFX_ShouldUseMdrMemoryPlaceholder(const char *name, int size)
{
	const int overCapLimit = 1536 * 1024;

	if (size <= (1536 * 1024))
	{
		return qfalse;
	}

	if (STEFX_IsBorgPlayerModelName(name))
	{
		return qfalse;
	}

	if (STEFX_IsPlayerModelName(name))
	{
#ifdef _XBOX
		XBLF("STEFX: R_LoadMDR allowing exact player model '%s' size=%d cap=%d",
			name ? name : "(null)", size, overCapLimit);
#endif
		return qfalse;
	}

#ifdef _XBOX
	XBLF("STEFX: R_LoadMDR budget placeholder model '%s' size=%d over cap=%d",
		name ? name : "(null)", size, overCapLimit);
#endif
	return qtrue;
}

static void STEFX_LogMdrMemoryStats(const char *phase, const char *name, int fileLen, int requestSize, int realSize, int alignPad, int fit)
{
	zmemstats_t stats;
	int shortfall;

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	extern void Z_GetMemoryStatsForAsset(zmemstats_t *stats, unsigned int assetContext);
	// FNV-1a of phase + '|' + name, copied into the same memory publication.
	// This survives log-ring rotation without retaining ephemeral name pointers.
	unsigned int context = 2166136261u;
	const unsigned char *text = (const unsigned char *)(phase ? phase : "");
	while (*text) { context = (context ^ *text++) * 16777619u; }
	context = (context ^ (unsigned int)'|') * 16777619u;
	text = (const unsigned char *)(name ? name : "");
	while (*text) { context = (context ^ *text++) * 16777619u; }
	Z_GetMemoryStatsForAsset(&stats, context);
#else
	Z_GetMemoryStats(&stats);
#endif
	shortfall = (realSize > stats.largestFreeBlock) ? (realSize - stats.largestFreeBlock) : 0;

	XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR memory %s model='%s' fileLen=%d request=%d real=%d alignPad=%d fit=%d shortfall=%d zoneSize=%d used=%d overhead=%d free=%d largest=%d freeBlocks=%d peak=%d md3=%d glm=%d gla=%d bsp=%d sndRaw=%d filesys=%d",
		phase ? phase : "(null)",
		name ? name : "(null)",
		fileLen,
		requestSize,
		realSize,
		alignPad,
		fit,
		shortfall,
		stats.zoneSize,
		stats.usedBytes,
		stats.overheadBytes,
		stats.freeBytes,
		stats.largestFreeBlock,
		stats.freeBlocks,
		stats.peakBytes,
		stats.modelMd3Bytes,
		stats.modelGlmBytes,
		stats.modelGlaBytes,
		stats.bspBytes,
		stats.soundRawBytes,
		stats.filesysBytes);
}
#endif
#endif
/*
Ghoul2 Insert End
*/


/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
static qhandle_t RE_RegisterModel_Actual( const char *name ) 
{
	model_t		*mod;
	unsigned	*buf;
	int			lod;
	int			ident;
	qboolean	loaded;
//	qhandle_t	hModel;
	int			numLoaded;
/*
Ghoul2 Insert Start
*/
	int			hash;
	modelHash_t	*mh;
/*
Ghoul2 Insert End
*/
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean stefxWeaponModelTrace = STEFX_IsWeaponDiskModelName(name);
	qboolean stefxPlayerModelTrace = STEFX_IsPlayerModelName(name);
#endif

	if ( !name || !name[0] ) {
		VID_Printf( PRINT_WARNING, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		VID_Printf( PRINT_DEVELOPER, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x01 );
	if (stefxPlayerModelTrace)
	{
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer actual entry '%s' trModels=%d", name, tr.numModels);
	}
#endif

/*
Ghoul2 Insert Start
*/
//	if (!tr.registered) {
//		VID_Printf( PRINT_WARNING, "RE_RegisterModel (%s) called before ready!\n",name );
//		return 0;
//	}
	//
	// search the currently loaded models
	//

	hash = generateHashValue(name, FILE_HASH_SIZE);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x02 );
#endif

	//
	// see if the model is already loaded
	//
	for (mh=mhHashTable[hash]; mh; mh=mh->next) {
		if (Q_stricmp(mh->name, name) == 0) {
			if (tr.models[mh->handle]->type == MOD_BAD)
			{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				if (stefxWeaponModelTrace)
				{
					XBLF("STEFX: RE_RegisterModel weapon MOD_BAD cache hit '%s' handle=%d", name, mh->handle);
				}
				if (STEFX_IsBorgPlayerModelName(name))
				{
					XBLF("STEFX: RE_RegisterModel Borg MOD_BAD cache hit exact required '%s' handle=%d", name, mh->handle);
				}
				if (stefxPlayerModelTrace)
				{
					XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer MOD_BAD cache hit '%s' handle=%d", name, mh->handle);
				}
#endif
				return 0;
			}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (stefxWeaponModelTrace)
			{
				XBLF("STEFX: RE_RegisterModel weapon cache hit '%s' handle=%d type=%d", name, mh->handle, tr.models[mh->handle]->type);
			}
			if (STEFX_IsBorgPlayerModelName(name))
			{
				XBLF("STEFX: RE_RegisterModel Borg cache hit '%s' handle=%d type=%d", name, mh->handle, tr.models[mh->handle]->type);
			}
			if (stefxPlayerModelTrace)
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer cache hit '%s' handle=%d type=%d", name, mh->handle, tr.models[mh->handle]->type);
			}
#endif
			return mh->handle;
		}
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x03 );
	if (stefxWeaponModelTrace)
	{
		g_SPXBWeaponModelTraceStage = 10;
		g_SPXBWeaponModelTracePathHash = STEFX_ModelTraceHash( name );
		g_SPXBWeaponModelTraceHandle = 0;
		g_SPXBWeaponModelTraceLoaded = 0;
		g_SPXBWeaponModelTraceFailCode = 0;
		XBLF("STEFX: RE_RegisterModel weapon load required '%s'", name);
	}
	if (STEFX_IsBorgPlayerModelName(name))
	{
		XBLF("STEFX: RE_RegisterModel Borg exact load required '%s'", name);
	}
#endif

/*
Ghoul2 Insert End
*/

	if (name[0] == '#')
	{
		char		temp[MAX_QPATH];

		tr.numBSPModels++;
#ifndef DEDICATED
		RE_LoadWorldMap_Actual(va("maps/%s.bsp", name + 1), tr.bspModels[tr.numBSPModels - 1], tr.numBSPModels);	//this calls R_LoadSubmodels which will put them into the Hash
#endif
		Com_sprintf(temp, MAX_QPATH, "*%d-0", tr.numBSPModels);
		hash = generateHashValue(temp, FILE_HASH_SIZE);
		for (mh=mhHashTable[hash]; mh; mh=mh->next) 
		{
			if (Q_stricmp(mh->name, temp) == 0) 
			{
				return mh->handle;
			}
		}
		
		return 0;
	}

	// allocate a new model_t

	if ( ( mod = R_AllocModel() ) == NULL ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (stefxPlayerModelTrace)
		{
			XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer R_AllocModel failed '%s' trModels=%d", name, tr.numModels);
			STEFX_LogMdrMemoryStats("model-slot-failed", name, 0, 0, 0, 0, 0);
		}
#endif
		VID_Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x04 );
#endif

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (stefxPlayerModelTrace)
	{
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer allocated '%s' index=%d", name, mod->index);
	}
#endif

#ifdef STEFX_ELITE_FORCE_SP
	if (STEFX_RegisterGhoul2Disabled(mod, name))
	{
		return 0;
	}

#if defined(_XBOX)
	STEFX_RENDER_MODEL_PHASE( 0x05 );
	g_SPXBModelProbeStage = 1;
	g_SPXBModelProbeNamePtr = (unsigned int)name;
	g_SPXBModelProbePathHash = STEFX_ModelTraceHash(name);
	qhandle_t stefxMdrPreflightHandle = STEFX_RegisterMdrPlaceholderIfPresent(mod, name);
	g_SPXBModelProbeStage = 6;
	STEFX_RENDER_MODEL_PHASE( 0x06 );
	if (stefxMdrPreflightHandle)
	{
		return stefxMdrPreflightHandle;
	}
#endif

#endif

	// make sure the render thread is stopped
	//R_SyncRenderThread();

	int iLODStart = 0;
	if (strstr (name, ".md3")) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		iLODStart = 0;
#else
		iLODStart = MD3_MAX_LODS-1;	//this loads the md3s in reverse so they can be biased
#endif
	}
	mod->numLods = 0;

	//
	// load the files
	//
	numLoaded = 0;

	for ( lod = iLODStart; lod >= 0 ; lod-- ) {
		char filename[1024];

		strcpy( filename, name );

		if ( lod != 0 ) {
			char namebuf[80];

			if ( strrchr( filename, '.' ) ) {
				*strrchr( filename, '.' ) = 0;
			}
			sprintf( namebuf, "_%d.md3", lod );
			strcat( filename, namebuf );
		}

		qboolean bAlreadyCached = qfalse;		
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_RENDER_MODEL_PHASE( 0x07 );
#endif
		if (!RE_RegisterModels_GetDiskFile(filename, (void **)&buf, &bAlreadyCached))
		{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (stefxPlayerModelTrace)
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer disk miss '%s' lod=%d numLoaded=%d", filename, lod, numLoaded);
			}
#endif
			if (numLoaded)	//we loaded one already, but a higher LOD is missing!
			{
				Com_Error (ERR_DROP, "R_LoadMD3: %s has LOD %d but is missing LOD %d ('%s')!", mod->name, lod+1, lod, filename);
			}
			continue;
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_RENDER_MODEL_PHASE( 0x08 );
#endif
		
		//loadmodel = mod;	// this seems to be fairly pointless

		// important that from now on we pass 'filename' instead of 'name' to all model load functions,
		//	because 'filename' accounts for any LOD mangling etc so guarantees unique lookups for yet more
		//	internal caching...
		//		
		ident = *(unsigned *)buf;
		if (!bAlreadyCached)
		{
			ident = LittleLong(ident);
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (stefxPlayerModelTrace)
		{
			XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer disk ready '%s' lod=%d ident=0x%08x cached=%d",
				filename, lod, ident, bAlreadyCached ? 1 : 0);
		}
#endif

		switch (ident)
		{
			// if you add any new types of model load in this switch-case, tell me, 
			//	or copy what I've done with the cache scheme (-ste).
			//
			case MDXA_IDENT:

				loaded = R_LoadMDXA( mod, buf, filename, bAlreadyCached );
				break;
		
			case MDXM_IDENT:
				
				loaded = R_LoadMDXM( mod, buf, filename, bAlreadyCached );
				break;

			case MD3_IDENT:

				loaded = R_LoadMD3( mod, lod, buf, filename, bAlreadyCached );
				break;

#ifdef STEFX_ELITE_FORCE_SP
			case MD4_IDENT:
#if defined(_XBOX)
				if ( strstr( filename, "models/players/borg" ) || strstr( filename, "models\\players\\borg" ) )
				{
					XBLF( "STEFX: RE_RegisterModel loading Borg MDR '%s' cached=%d", filename, bAlreadyCached ? 1 : 0 );
				}
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				STEFX_RENDER_MODEL_PHASE( 0x09 );
#endif
				loaded = R_LoadMDR( mod, buf, filename, bAlreadyCached );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				STEFX_RENDER_MODEL_PHASE( 0x0a );
#endif
				break;
#endif

			default:

				VID_Printf (PRINT_WARNING,"RE_RegisterModel: unknown fileid for %s\n", filename);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				if (STEFX_IsPlayerModelName(filename))
				{
					XBLF("STEFX: RE_RegisterModel player unknown ident '%s' ident=0x%08x", filename, ident);
				}
				if (stefxWeaponModelTrace)
				{
					g_SPXBWeaponModelTraceStage = 14;
					g_SPXBWeaponModelTraceIdent = ident;
					g_SPXBWeaponModelTraceFailCode = 2;
					XBLF("STEFX: RE_RegisterModel weapon unknown ident '%s' ident=0x%08x", filename, ident);
				}
#endif
				goto fail;
		}
		
		if (!bAlreadyCached){	// important to check!!
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			STEFX_RENDER_MODEL_PHASE( 0x0b );
#endif
			FS_FreeFile (buf);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			STEFX_RENDER_MODEL_PHASE( 0x0c );
#endif
		}

		if ( !loaded ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (stefxPlayerModelTrace)
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer loader failed '%s' lod=%d", filename, lod);
			}
#endif
			if ( lod == 0 ) {
				VID_Printf (PRINT_WARNING,"RE_RegisterModel: cannot load %s\n", filename);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				if (stefxWeaponModelTrace)
				{
					g_SPXBWeaponModelTraceStage = 15;
					g_SPXBWeaponModelTraceLoaded = 0;
					g_SPXBWeaponModelTraceFailCode = 3;
					XBLF("STEFX: RE_RegisterModel weapon load failed '%s' lod=%d", filename, lod);
				}
#endif
				goto fail;
			} else {
				break;
			}
		} else {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if (stefxPlayerModelTrace)
			{
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer loader success '%s' lod=%d", filename, lod);
			}
#endif
			mod->numLods++;
			numLoaded++;
			// if we have a valid model and are biased
			// so that we won't see any higher detail ones,
			// stop loading them
			if ( lod <= r_lodbias->integer ) {
				break;
			}
		}
	}

	if ( numLoaded ) {
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for ( lod-- ; lod >= 0 ; lod-- ) {
			mod->numLods++;
			mod->md3[lod] = mod->md3[lod+1];
		}
/*
Ghoul2 Insert Start
*/

	RE_InsertModelIntoHash(name, mod);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (stefxWeaponModelTrace)
	{
		g_SPXBWeaponModelTraceStage = 20;
		g_SPXBWeaponModelTraceHandle = (unsigned int)mod->index;
		g_SPXBWeaponModelTraceLoaded = (unsigned int)numLoaded;
		g_SPXBWeaponModelTraceFailCode = 0;
		XBLF("STEFX: RE_RegisterModel weapon loaded '%s' handle=%d lods=%d", name, mod->index, mod->numLods);
	}
	if (stefxPlayerModelTrace)
	{
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer loaded '%s' handle=%d lods=%d", name, mod->index, mod->numLods);
	}
#endif
	return mod->index;
/*
Ghoul2 Insert End
*/
	
	}


fail:
	// we still keep the model_t around, so if the model name is asked for
	// again, we won't bother scanning the filesystem
	mod->type = MOD_BAD;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (stefxWeaponModelTrace)
	{
		g_SPXBWeaponModelTraceStage = 30;
		g_SPXBWeaponModelTraceHandle = (unsigned int)mod->index;
		g_SPXBWeaponModelTraceLoaded = (unsigned int)numLoaded;
		if (g_SPXBWeaponModelTraceFailCode == 0)
		{
			g_SPXBWeaponModelTraceFailCode = 4;
		}
		XBLF("STEFX: RE_RegisterModel weapon fail insert MOD_BAD '%s' index=%d numLoaded=%d", name, mod->index, numLoaded);
	}
	if (STEFX_IsPlayerModelName(name))
	{
		const char *fallbackName = STEFX_DefaultPlayerMdrFallbackName(name);
		if (fallbackName && !STEFX_IsDefaultPlayerMdrFallbackName(name))
		{
			qhandle_t fallback = RE_RegisterModel_Actual(fallbackName);
			if (fallback)
			{
				STEFX_InsertModelHandleAliasIntoHash(name, fallback);
				XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR hazard fallback '%s' index=%d -> '%s' handle=%d numLoaded=%d",
					name, mod->index, fallbackName, fallback, numLoaded);
				return fallback;
			}
			XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR hazard fallback failed '%s' index=%d fallback='%s' numLoaded=%d",
				name, mod->index, fallbackName, numLoaded);
		}
		else if (fallbackName)
		{
			XBLog_WriteCriticalf("STEFX_MODEL_BOOT: MDR hazard fallback base failed '%s' index=%d numLoaded=%d",
				name, mod->index, numLoaded);
		}
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer fail insert MOD_BAD '%s' index=%d numLoaded=%d",
			name, mod->index, numLoaded);
	}
#endif
	RE_InsertModelIntoHash(name, mod);
	return 0;
}




// wrapper function needed to avoid problems with mid-function returns so I can safely use this bool to tell the
//	z_malloc-fail recovery code whether it's safe to ditch any model caches...
//
qboolean gbInsideRegisterModel = qfalse;
qhandle_t RE_RegisterModel( const char *name )
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean stefxPlayerModelTrace = STEFX_IsPlayerModelName(name);
	if (stefxPlayerModelTrace)
	{
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer entry '%s'", name);
	}
#endif
	gbInsideRegisterModel = qtrue;	// !!!!!!!!!!!!!!

		qhandle_t q = RE_RegisterModel_Actual( name );

if (!name || strlen(name) < 4 || stricmp(&name[strlen(name)-4],".gla")){
	gbInsideRegisterModel = qfalse;		// GLA files recursively call this, so don't turn off half way. A reference count would be nice, but if any ERR_DROP ever occurs within the load then the refcount will be knackered from then on
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (stefxPlayerModelTrace)
	{
		XBLog_WriteCriticalf("STEFX_MODEL_BOOT: renderer exit '%s' -> %d", name, q);
	}
#endif
	return q;
}


/*
=================
R_LoadMDR
=================
*/
#ifdef STEFX_ELITE_FORCE_SP
static qboolean R_LoadMDR (model_t *mod, void *buffer, const char *mod_name, qboolean &bAlreadyCached ) {
	int					i, j;
	md4Header_t			*pinmodel;
	md4LOD_t			*lod;
	md4Surface_t		*surf;
	int					version;
	int					size;

	pinmodel = (md4Header_t *)buffer;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x30 );
#endif
	version = pinmodel->version;
	size = pinmodel->ofsEnd;

	if (!bAlreadyCached)
	{
		version = LittleLong(version);
		size = LittleLong(size);
	}

	if (version != MD4_VERSION)
	{
		VID_Printf( PRINT_WARNING, "R_LoadMDR: %s has wrong version (%i should be %i)\n",
			mod_name, version, MD4_VERSION );
#if defined(_XBOX)
		XBLF("STEFX: R_LoadMDR wrong version '%s' version=%d expected=%d", mod_name, version, MD4_VERSION);
#endif
		return qfalse;
	}

	if (size <= 0)
	{
		VID_Printf( PRINT_WARNING, "R_LoadMDR: %s has invalid size %i\n", mod_name, size );
#if defined(_XBOX)
		XBLF("STEFX: R_LoadMDR invalid size '%s' size=%d", mod_name, size);
#endif
		return qfalse;
	}

#if defined(_XBOX)
	if (STEFX_IsPlayerModelName(mod_name))
	{
		STEFX_LogMdrMemoryStats("load-start", mod_name, size, size, size, 0, 1);
	}

	if (STEFX_ShouldUseMdrMemoryPlaceholder(mod_name, size))
	{
		mod->type = MOD_STEFX_MDR_PLACEHOLDER;
		mod->dataSize += size;
		mod->numLods = 0;
		mod->md4 = NULL;
		XBLF("STEFX: R_LoadMDR overbudget placeholder '%s' size=%d cap=%d",
			mod_name, size, 1536 * 1024);
		return qtrue;
	}
#endif

	mod->type = MOD_MDR;
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x31 );
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (STEFX_IsBorgPlayerModelName(mod_name))
	{
		mod->md4 = STEFX_RegisterCompactBorgMdr(mod, buffer, mod_name, &bAlreadyFound);
		if (!mod->md4)
		{
			XBLF("STEFX: Borg MDR compact path unavailable; using full resident model '%s' size=%d",
				mod_name, size);
			mod->md4 = (md4Header_t *)RE_RegisterModels_Malloc(size, buffer, mod_name,
				&bAlreadyFound, TAG_MODEL_MD3);
		}
	}
	else
#endif
	{
		mod->md4 = (md4Header_t *)RE_RegisterModels_Malloc(size, buffer, mod_name,
			&bAlreadyFound, TAG_MODEL_MD3);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		char cacheName[MAX_QPATH];
		Q_strncpyz(cacheName, mod_name, sizeof(cacheName));
		Q_strlwr(cacheName);
		CachedEndianedModelBinary_t &entry = (*CachedModels)[cacheName];
		if (entry.bMdrCompacted && entry.iMdrFrameBaseCount == -STEFX_MDR_BLOCK_FRAMES)
		{
			STEFX_AssignMdrFrameStorage(mod, entry);
			mod->dataSize += entry.iAllocSize - size;
		}
#endif
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_RENDER_MODEL_PHASE( 0x32 );
#endif
	if (!mod->md4)
	{
#if defined(_XBOX)
		XBLF("STEFX: R_LoadMDR allocation failed '%s' size=%d", mod_name, size);
#endif
		return qfalse;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (!bAlreadyFound && mod->md4 == buffer)
	{
		bAlreadyCached = qtrue;
		XBLF("STEFX: R_LoadMDR using adopted disk buffer '%s' size=%d", mod_name, size);
	}
#else
	assert(bAlreadyCached == bAlreadyFound);
#endif

	if (!bAlreadyFound)
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_RENDER_MODEL_PHASE( 0x33 );
#endif
#ifdef _XBOX
		if (mod->md4 != buffer
#if defined(STEFX_ELITE_FORCE_SP)
			&& !mod->stefxMdrFrameBase
#endif
			)
		{
			memcpy(mod->md4, buffer, size);
		}
#else
		bAlreadyCached = qtrue;
		assert(mod->md4 == buffer);
#endif

		LL(mod->md4->ident);
		LL(mod->md4->version);
		LL(mod->md4->numFrames);
		LL(mod->md4->numBones);
		LL(mod->md4->ofsFrames);
		LL(mod->md4->numLODs);
		LL(mod->md4->ofsLODs);
		LL(mod->md4->numTags);
		LL(mod->md4->ofsTags);
		LL(mod->md4->ofsEnd);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		STEFX_RENDER_MODEL_PHASE( 0x34 );
#endif

		if (mod->md4->numFrames < 1 || mod->md4->numBones < 1 || mod->md4->numLODs < 1)
		{
			VID_Printf( PRINT_WARNING, "R_LoadMDR: %s has invalid counts frames=%i bones=%i lods=%i\n",
				mod_name, mod->md4->numFrames, mod->md4->numBones, mod->md4->numLODs );
			return qfalse;
		}

		if (mod->md4->numBones > MD4_MAX_BONES)
		{
			VID_Printf( PRINT_WARNING, "R_LoadMDR: %s has too many bones (%i > %i)\n",
				mod_name, mod->md4->numBones, MD4_MAX_BONES );
			return qfalse;
		}

		md4Tag_t *tag = (md4Tag_t *)((byte *)mod->md4 + mod->md4->ofsTags);
		for (i = 0; i < mod->md4->numTags; i++, tag++)
		{
			LL(tag->boneIndex);
			if (tag->boneIndex < 0 || tag->boneIndex >= mod->md4->numBones)
			{
				VID_Printf( PRINT_WARNING, "R_LoadMDR: %s tag %s has invalid bone index %i of %i\n",
					mod_name, tag->name, tag->boneIndex, mod->md4->numBones );
#ifdef _XBOX
				XBLF("STEFX: R_LoadMDR invalid tag model='%s' tag='%s' bone=%d bones=%d",
					mod_name, tag->name, tag->boneIndex, mod->md4->numBones);
#endif
			}
		}

		lod = (md4LOD_t *)((byte *)mod->md4 + mod->md4->ofsLODs);
		for (i = 0; i < mod->md4->numLODs; i++)
		{
			LL(lod->numSurfaces);
			LL(lod->ofsSurfaces);
			LL(lod->ofsEnd);

			surf = (md4Surface_t *)((byte *)lod + lod->ofsSurfaces);
			for (j = 0; j < lod->numSurfaces; j++)
			{
				shader_t *sh;

				LL(surf->ofsHeader);
				LL(surf->numVerts);
				LL(surf->ofsVerts);
				LL(surf->numTriangles);
				LL(surf->ofsTriangles);
				LL(surf->numBoneReferences);
				LL(surf->ofsBoneReferences);
				LL(surf->ofsEnd);

				if (surf->numVerts > SHADER_MAX_VERTEXES)
				{
					Com_Error( ERR_DROP, "R_LoadMDR: %s has more than %i verts on a surface (%i)",
						mod_name, SHADER_MAX_VERTEXES, surf->numVerts );
				}
				if (surf->numTriangles * 3 > SHADER_MAX_INDEXES)
				{
					Com_Error( ERR_DROP, "R_LoadMDR: %s has more than %i triangles on a surface (%i)",
						mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles );
				}

				surf->ident = SF_MDR;
				Q_strlwr(surf->name);

				sh = R_FindShader(surf->shader, lightmapsNone, stylesDefault, qtrue);
				if (sh->defaultShader)
				{
					surf->shaderIndex = 0;
				}
				else
				{
					surf->shaderIndex = sh->index;
				}
				RE_RegisterModels_StoreShaderRequest(mod_name, &surf->shader[0], &surf->shaderIndex);

				surf = (md4Surface_t *)((byte *)surf + surf->ofsEnd);
			}

			lod = (md4LOD_t *)((byte *)lod + lod->ofsEnd);
		}
	}

	mod->numLods = (mod->md4->numLODs > 0) ? (unsigned char)mod->md4->numLODs : 0;
#ifdef _XBOX
	XBLF("STEFX: R_LoadMDR loaded '%s' frames=%d bones=%d lods=%d size=%d",
		mod_name, mod->md4->numFrames, mod->md4->numBones, mod->md4->numLODs, size);
	if (STEFX_IsPlayerModelName(mod_name))
	{
		STEFX_LogMdrMemoryStats("load-done", mod_name, size, size, size, 0, 1);
	}
#endif
	return qtrue;
}
#endif


/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3 (model_t *mod, int lod, void *buffer, const char *mod_name, qboolean &bAlreadyCached ) {
	int					i, j;
	md3Header_t			*pinmodel;
	md3Surface_t		*surf;
	md3Shader_t			*shader;
	int					version;
	int					size;

#ifndef _M_IX86
	md3Frame_t			*frame;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3XyzNormal_t		*xyz;
	md3Tag_t			*tag;
#endif


	pinmodel= (md3Header_t *)buffer;
	//
	// read some fields from the binary, but only LittleLong() them when we know this wasn't an already-cached model...
	//
	version = pinmodel->version;
	size	= pinmodel->ofsEnd;

	if (!bAlreadyCached)
	{
		version = LittleLong(version);
		size	= LittleLong(size);
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (STEFX_IsWeaponDiskModelName(mod_name))
	{
		g_SPXBWeaponModelTraceStage = 11;
		g_SPXBWeaponModelTracePathHash = STEFX_ModelTraceHash( mod_name );
		g_SPXBWeaponModelTraceVersion = (unsigned int)version;
		g_SPXBWeaponModelTraceSize = (unsigned int)size;
		g_SPXBWeaponModelTraceFailCode = 0;
		XBLF("STEFX: R_LoadMD3 weapon begin '%s' lod=%d cached=%d version=%d size=%d",
			mod_name,
			lod,
			bAlreadyCached ? 1 : 0,
			version,
			size);
	}
#endif
	
	if (version != MD3_VERSION) {
		VID_Printf( PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n",
				 mod_name, version, MD3_VERSION);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (STEFX_IsWeaponDiskModelName(mod_name))
		{
			g_SPXBWeaponModelTraceStage = 12;
			g_SPXBWeaponModelTraceVersion = (unsigned int)version;
			g_SPXBWeaponModelTraceFailCode = 5;
			XBLF("STEFX: R_LoadMD3 weapon wrong version '%s' version=%d expected=%d",
				mod_name,
				version,
				MD3_VERSION);
		}
#endif
		return qfalse;
	}

	mod->type      = MOD_MESH;	
	mod->dataSize += size;

	qboolean bAlreadyFound = qfalse;
	mod->md3[lod] = (md3Header_t *) RE_RegisterModels_Malloc(size, buffer, mod_name, &bAlreadyFound, TAG_MODEL_MD3);
	if (!mod->md3[lod])
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (STEFX_IsWeaponDiskModelName(mod_name))
		{
			g_SPXBWeaponModelTraceStage = 13;
			g_SPXBWeaponModelTraceSize = (unsigned int)size;
			g_SPXBWeaponModelTraceFailCode = 6;
		}
		XBLF("STEFX: R_LoadMD3 allocation failed '%s' lod=%d size=%d", mod_name, lod, size);
#endif
		return qfalse;
	}

	assert(bAlreadyCached == bAlreadyFound);

	if (!bAlreadyFound)
	{	
		// horrible new hackery, if !bAlreadyFound then we've just done a tag-morph, so we need to set the 
		//	bool reference passed into this function to true, to tell the caller NOT to do an FS_Freefile since
		//	we've hijacked that memory block...
		//
		// Aaaargh. Kill me now...
		//
#ifdef _XBOX
		memcpy( mod->md3[lod], buffer, size );
#else
		bAlreadyCached = qtrue;
		assert( mod->md3[lod] == buffer );
#endif

		LL(mod->md3[lod]->ident);
		LL(mod->md3[lod]->version);
		LL(mod->md3[lod]->numFrames);
		LL(mod->md3[lod]->numTags);
		LL(mod->md3[lod]->numSurfaces);
		LL(mod->md3[lod]->ofsFrames);
		LL(mod->md3[lod]->ofsTags);
		LL(mod->md3[lod]->ofsSurfaces);
		LL(mod->md3[lod]->ofsEnd);
	}

	if ( mod->md3[lod]->numFrames < 1 ) {
		VID_Printf( PRINT_WARNING, "R_LoadMD3: %s has no frames\n", mod_name );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (STEFX_IsWeaponDiskModelName(mod_name))
		{
			g_SPXBWeaponModelTraceStage = 16;
			g_SPXBWeaponModelTraceFailCode = 7;
			XBLF("STEFX: R_LoadMD3 weapon no frames '%s'", mod_name);
		}
#endif
		return qfalse;
	}

	if (bAlreadyFound)
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if (STEFX_IsWeaponDiskModelName(mod_name))
		{
			g_SPXBWeaponModelTraceStage = 17;
			g_SPXBWeaponModelTraceLoaded = 1;
			g_SPXBWeaponModelTraceFailCode = 0;
			XBLF("STEFX: R_LoadMD3 weapon cache reused '%s' lod=%d", mod_name, lod);
		}
#endif
		return qtrue;	// All done. Stop, go no further, do not pass Go...
	}

#ifndef _M_IX86
	//
	// optimisation, we don't bother doing this for standard intel case since our data's already in that format...
	//

	// swap all the frames
    frame = (md3Frame_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsFrames );
    for ( i = 0 ; i < mod->md3[lod]->numFrames ; i++, frame++) {
    	frame->radius = LittleFloat( frame->radius );
        for ( j = 0 ; j < 3 ; j++ ) {
            frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
            frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
	    	frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
        }
	}

	// swap all the tags
    tag = (md3Tag_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsTags );
    for ( i = 0 ; i < mod->md3[lod]->numTags * mod->md3[lod]->numFrames ; i++, tag++) {
        for ( j = 0 ; j < 3 ; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
        }
	}
#endif

	// swap all the surfaces
	surf = (md3Surface_t *) ( (byte *)mod->md3[lod] + mod->md3[lod]->ofsSurfaces );
	for ( i = 0 ; i < mod->md3[lod]->numSurfaces ; i++) {
        LL(surf->flags);
        LL(surf->numFrames);
        LL(surf->numShaders);
        LL(surf->numTriangles);
        LL(surf->ofsTriangles);
        LL(surf->numVerts);
        LL(surf->ofsShaders);
        LL(surf->ofsSt);
        LL(surf->ofsXyzNormals);
        LL(surf->ofsEnd);
		
		if ( surf->numVerts > SHADER_MAX_VERTEXES ) {
			Com_Error (ERR_DROP, "R_LoadMD3: %s has more than %i verts on a surface (%i)",
				mod_name, SHADER_MAX_VERTEXES, surf->numVerts );
		}
		if ( surf->numTriangles*3 > SHADER_MAX_INDEXES ) {
			Com_Error (ERR_DROP, "R_LoadMD3: %s has more than %i triangles on a surface (%i)",
				mod_name, SHADER_MAX_INDEXES / 3, surf->numTriangles );
		}
	
		// change to surface identifier
		surf->ident = SF_MD3;

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surf->name );

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

        // register the shaders
        shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
        for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
            shader_t	*sh;

            sh = R_FindShader( shader->name, lightmapsNone, stylesDefault, qtrue );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
			RE_RegisterModels_StoreShaderRequest(mod_name, &shader->name[0], &shader->shaderIndex);
        }


#ifndef _M_IX86
//
// optimisation, we don't bother doing this for standard intel case since our data's already in that format...
//

		// swap all the triangles
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for ( j = 0 ; j < surf->numTriangles ; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);
		}

		// swap all the ST
        st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
        for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
            st->st[0] = LittleFloat( st->st[0] );
            st->st[1] = LittleFloat( st->st[1] );
        }

		// swap all the XyzNormals
        xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
        for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ ) 
		{
            xyz->xyz[0] = LittleShort( xyz->xyz[0] );
            xyz->xyz[1] = LittleShort( xyz->xyz[1] );
            xyz->xyz[2] = LittleShort( xyz->xyz[2] );

            xyz->normal = LittleShort( xyz->normal );
        }
#endif

		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if (STEFX_IsWeaponDiskModelName(mod_name))
	{
		g_SPXBWeaponModelTraceStage = 18;
		g_SPXBWeaponModelTraceLoaded = 1;
		g_SPXBWeaponModelTraceFailCode = 0;
	}
#endif
    
	return qtrue;
}


//=============================================================================

void ShaderTableCleanup();
void CM_LoadShaderText(bool forceReload);
void CM_SetupShaderProperties(void);

void R_HunkClearCrap(void)
{
	ShaderTableCleanup();
	tr.numModels = 0;
	memset(tr.models, 0, sizeof(tr.models));
	tr.numShaders = 0;
	tr.numSkins = 0;
}

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration( glconfig_t *glconfigOut ) {
#ifndef _XBOX
	ShaderTableCleanup();
#endif
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: RE_BeginRegistration clearing hunk to mark");
#endif
	Hunk_ClearToMark();

#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: RE_BeginRegistration entering R_Init");
#endif
	R_Init();
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: RE_BeginRegistration R_Init complete");
#endif
	*glconfigOut = glConfig;

	tr.viewCluster = -1;		// force markleafs to regenerate
	RE_ClearScene();
	tr.registered = qtrue;

	R_SyncRenderThread();
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: RE_BeginRegistration complete");
#endif
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit( void ) 
{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_ResetMdrModelRegistry();
#endif
#ifdef _XBOX
	// Sorry Raven, but static maps == fragmentation
	if (!CachedModels)
	{
		CachedModels = new CachedModels_t;
	}
#else
	static CachedModels_t singleton;	// sorry vv, your dynamic allocation was a (false) memory leak
	CachedModels = &singleton;
#endif

	model_t		*mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
/*
Ghoul2 Insert Start
*/

	memset(mhHashTable, 0, sizeof(mhHashTable));
/*
Ghoul2 Insert End
*/

}


/*
================
R_Modellist_f
================
*/
void R_Modellist_f( void ) {
	int		i, j;
	model_t	*mod;
	int		total;
	int		lods;

	total = 0;
	for ( i = 1 ; i < tr.numModels; i++ ) {
		mod = tr.models[i];
		switch (mod->type)
		{
			default:
				assert(0);
				VID_Printf( PRINT_ALL, "UNKNOWN  :      %s\n", mod->name );
				break;

			case MOD_BAD:
				VID_Printf( PRINT_ALL, "MOD_BAD  :      %s\n", mod->name );
				break;

			case MOD_BRUSH:
				VID_Printf( PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, mod->numLods, mod->name );
				break;

#ifdef STEFX_ELITE_FORCE_SP
			case MOD_MDR:
				VID_Printf( PRINT_ALL, "%8i : (%i MDR) %s\n", mod->dataSize, mod->numLods, mod->name );
				break;

			case MOD_STEFX_MDR_PLACEHOLDER:
				VID_Printf( PRINT_ALL, "%8i : (MDR placeholder) %s\n", mod->dataSize, mod->name );
				break;
#endif

			case MOD_MDXA:

				VID_Printf( PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, mod->numLods, mod->name );								
				break;
		
			case MOD_MDXM:
				
				VID_Printf( PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, mod->numLods, mod->name );								
				break;

			case MOD_MESH:

				lods = 1;
				for ( j = 1 ; j < MD3_MAX_LODS ; j++ ) {
					if ( mod->md3[j] && mod->md3[j] != mod->md3[j-1] ) {
						lods++;
					}
				}				
				VID_Printf( PRINT_ALL, "%8i : (%i) %s\n",mod->dataSize, lods, mod->name );
				break;		
		}
		total += mod->dataSize;
	}
	VID_Printf( PRINT_ALL, "%8i : Total models\n", total );

/*	this doesn't work with the new hunks
	if ( tr.world ) {
		VID_Printf( PRINT_ALL, "%8i : %s\n", tr.world->dataSize, tr.world->name );
	} */
}

//=============================================================================


/*
================
R_GetTag for MD3s
================
*/
static md3Tag_t *R_GetTag( md3Header_t *mod, int frame, const char *tagName ) {
	md3Tag_t		*tag;
	int				i;

	if ( frame >= mod->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;	// found it
		}
	}

	return NULL;
}

#ifdef STEFX_ELITE_FORCE_SP
static md4Tag_t *R_STEFX_GetMDRTag( md4Header_t *mod, const char *tagName ) {
	md4Tag_t		*tag;
	int				i;

	if ( !mod || !tagName || mod->numTags <= 0 ) {
		return NULL;
	}

	tag = (md4Tag_t *)((byte *)mod + mod->ofsTags);
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;
		}
	}

	return NULL;
}

static qboolean R_STEFX_GetMDRBone( md4Header_t *mod, int frame, int boneIndex, md4Bone_t *bone ) {
	int				frameSize;

	if ( !mod || !bone || boneIndex < 0 || boneIndex >= mod->numBones || mod->numFrames <= 0 ) {
		return qfalse;
	}

	if ( frame < 0 ) {
		frame = 0;
	} else if ( frame >= mod->numFrames ) {
		frame = mod->numFrames - 1;
	}

	if ( mod->ofsFrames < 0 ) {
		md4CompFrame_t	*cframe;

		frameSize = (int)( &((md4CompFrame_t *)0)->bones[ mod->numBones ] );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		cframe = (md4CompFrame_t *)R_STEFX_GetMDRFrame(mod, frame);
#else
		cframe = (md4CompFrame_t *)((byte *)mod - mod->ofsFrames + frame * frameSize );
#endif
		MC_UnCompress( bone->matrix, cframe->bones[ boneIndex ].Comp );
	} else {
		md4Frame_t		*md4Frame;

		frameSize = (int)( &((md4Frame_t *)0)->bones[ mod->numBones ] );
		md4Frame = (md4Frame_t *)((byte *)mod + mod->ofsFrames + frame * frameSize );
		*bone = md4Frame->bones[ boneIndex ];
	}

	return qtrue;
}

static qboolean R_STEFX_LerpMDRTag( orientation_t *tag, model_t *model, int startFrame, int endFrame,
									float frac, const char *tagName ) {
	md4Tag_t		*mdrTag;
	md4Bone_t		startBone;
	md4Bone_t		finishBone;
	int				axis;
	int				component;
	float			frontLerp;
	float			backLerp;

	if ( !tag || !model || !model->md4 ) {
		return qfalse;
	}

	mdrTag = R_STEFX_GetMDRTag( model->md4, tagName );
	if ( !mdrTag ) {
#if defined(_XBOX)
		static int s_missingTagLogBudget = 0;
		if ( s_missingTagLogBudget < 24 ) {
			XBLF( "STEFX: R_LerpTag MDR missing tag='%s' model='%s' tags=%d",
				tagName ? tagName : "(null)",
				model->name,
				model->md4->numTags );
			s_missingTagLogBudget++;
		}
#endif
		return qfalse;
	}

	if ( !R_STEFX_GetMDRBone( model->md4, startFrame, mdrTag->boneIndex, &startBone ) ||
		 !R_STEFX_GetMDRBone( model->md4, endFrame, mdrTag->boneIndex, &finishBone ) ) {
#if defined(_XBOX)
		static int s_badTagLogBudget = 0;
		if ( s_badTagLogBudget < 24 ) {
			XBLF( "STEFX: R_LerpTag MDR bad bone tag='%s' model='%s' bone=%d bones=%d frames=%d/%d count=%d",
				tagName ? tagName : "(null)",
				model->name,
				mdrTag->boneIndex,
				model->md4->numBones,
				startFrame,
				endFrame,
				model->md4->numFrames );
			s_badTagLogBudget++;
		}
#endif
		return qfalse;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for ( component = 0 ; component < 3 ; component++ ) {
		tag->origin[component] =
			startBone.matrix[component][3] * backLerp +
			finishBone.matrix[component][3] * frontLerp;
	}

	for ( axis = 0 ; axis < 3 ; axis++ ) {
		for ( component = 0 ; component < 3 ; component++ ) {
			tag->axis[axis][component] =
				startBone.matrix[component][axis] * backLerp +
				finishBone.matrix[component][axis] * frontLerp;
		}
		VectorNormalize( tag->axis[axis] );
	}

#if defined(_XBOX)
	{
		static int s_tagLogBudget = 0;
		if ( s_tagLogBudget < 64 ) {
			XBLF( "STEFX: R_LerpTag MDR ok tag='%s' model='%s' bone=%d frames=%d/%d frac=%g origin=(%g,%g,%g) axis0=(%g,%g,%g)",
				tagName ? tagName : "(null)",
				model->name,
				mdrTag->boneIndex,
				startFrame,
				endFrame,
				frac,
				tag->origin[0],
				tag->origin[1],
				tag->origin[2],
				tag->axis[0][0],
				tag->axis[0][1],
				tag->axis[0][2] );
			s_tagLogBudget++;
		}
	}
#endif

	return qtrue;
}
#endif

/*
================
R_LerpTag
================
*/
void	R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, 
					 float frac, const char *tagName ) {
	md3Tag_t	*start, *finish;
	int		i;
	float		frontLerp, backLerp;
	model_t		*model;

	model = R_GetModelByHandle( handle );
	if ( model->md3[0] ) 
	{
		start = R_GetTag( model->md3[0], startFrame, tagName );
		finish = R_GetTag( model->md3[0], endFrame, tagName );
	}
#ifdef STEFX_ELITE_FORCE_SP
	else if ( model->md4 )
	{
		if ( R_STEFX_LerpMDRTag( tag, model, startFrame, endFrame, frac, tagName ) ) {
			return;
		}

		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return;
	}
#endif
	else
	{
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return;
	}

	if ( !start || !finish ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return;
	}

	frontLerp = frac;
	backLerp = 1.0 - frac;

	for ( i = 0 ; i < 3 ; i++ ) {
		tag->origin[i] = start->origin[i] * backLerp +  finish->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  finish->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  finish->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  finish->axis[2][i] * frontLerp;
	}
	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );
}


/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	model_t		*model;

	model = R_GetModelByHandle( handle );

	if ( model->bmodel ) {
		VectorCopy( model->bmodel->bounds[0], mins );
		VectorCopy( model->bmodel->bounds[1], maxs );
		return;
	}

	if ( model->md3[0] ) {
		md3Header_t	*header;
		md3Frame_t	*frame;
		header = model->md3[0];

		frame = (md3Frame_t *)( (byte *)header + header->ofsFrames );

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
	}
	else
	{
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}
}


#ifdef _XBOX
void R_ModelFree(void)
{
	if (CachedModels)
	{
		RE_RegisterModels_DeleteAll();
		delete CachedModels;
		CachedModels = NULL;
	}
}
#endif
