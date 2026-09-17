// Elite Force version-46 BSP adapter for the Xbox packed BSP loaders.
//
// This does not write sidecar lumps.  It reads a raw EF/Q3 BSP from the
// filesystem and converts individual raw lumps in memory to the packed structs
// already consumed by cm_load_xbox.cpp and tr_bsp_xbox.cpp.

#ifndef EF_BSP_XBOX_SHARED_H
#define EF_BSP_XBOX_SHARED_H

#ifdef STEFX_ELITE_FORCE_SP

#include <math.h>
#include "sparc.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
#endif

#define EF_BSP_IDENT_STRING "IBSP"
#define EF_BSP_VERSION 46
#define EF_BSP_HEADER_LUMPS 17

#define EF_LUMP_ENTITIES      0
#define EF_LUMP_SHADERS       1
#define EF_LUMP_PLANES        2
#define EF_LUMP_NODES         3
#define EF_LUMP_LEAFS         4
#define EF_LUMP_LEAFSURFACES  5
#define EF_LUMP_LEAFBRUSHES   6
#define EF_LUMP_MODELS        7
#define EF_LUMP_BRUSHES       8
#define EF_LUMP_BRUSHSIDES    9
#define EF_LUMP_DRAWVERTS     10
#define EF_LUMP_DRAWINDEXES   11
#define EF_LUMP_FOGS          12
#define EF_LUMP_SURFACES      13
#define EF_LUMP_LIGHTMAPS     14
#define EF_LUMP_LIGHTGRID     15
#define EF_LUMP_VISIBILITY    16

#define EF_MST_PLANAR         1
#define EF_MST_PATCH          2
#define EF_MST_TRIANGLE_SOUP  3
#define EF_MST_FLARE          4

#define EF_LIGHTMAP_NONE      -1
#define EF_LIGHTMAP_BY_VERTEX -3
#define EF_LIGHTGRID_RECORD_SIZE 8

#pragma pack(push, 4)
typedef struct
{
	int fileofs;
	int filelen;
} efbspLump_t;

typedef struct
{
	char ident[4];
	int version;
	efbspLump_t lumps[EF_BSP_HEADER_LUMPS];
} efbspHeader_t;

typedef struct
{
	float mins[3];
	float maxs[3];
	int firstSurface;
	int numSurfaces;
	int firstBrush;
	int numBrushes;
} efbspModel_t;

typedef struct
{
	int planeNum;
	int children[2];
	int mins[3];
	int maxs[3];
} efbspNode_t;

typedef struct
{
	int cluster;
	int area;
	int mins[3];
	int maxs[3];
	int firstLeafSurface;
	int numLeafSurfaces;
	int firstLeafBrush;
	int numLeafBrushes;
} efbspLeaf_t;

typedef struct
{
	int firstSide;
	int numSides;
	int shaderNum;
} efbspBrush_t;

typedef struct
{
	int planeNum;
	int shaderNum;
} efbspBrushSide_t;

typedef struct
{
	float xyz[3];
	float st[2];
	float lightmap[2];
	float normal[3];
	byte color[4];
} efbspDrawVert_t;

typedef struct
{
	int shaderNum;
	int fogNum;
	int surfaceType;
	int firstVert;
	int numVerts;
	int firstIndex;
	int numIndexes;
	int lightmapNum;
	int lightmapX;
	int lightmapY;
	int lightmapWidth;
	int lightmapHeight;
	float lightmapOrigin[3];
	float lightmapVecs[3][3];
	int patchWidth;
	int patchHeight;
} efbspSurface_t;
#pragma pack(pop)

typedef struct
{
	byte *data;
	int len;
	efbspHeader_t *header;
} efbspFile_t;

extern qboolean FS_PK3PatchFileExists( const char *filename );

static qboolean EFBSP_BuildXboxPatchName(const char *name, char *out, int outSize)
{
	const char *prefix = "maps/";
	const char *ext;
	const char *base;
	int baseLen;

	if (!name || !out || outSize <= 0)
	{
		return qfalse;
	}
	if (Q_stricmpn(name, prefix, 5))
	{
		return qfalse;
	}

	ext = strrchr(name, '.');
	if (!ext || Q_stricmp(ext, ".bsp"))
	{
		return qfalse;
	}

	base = name + 5;
	if (strstr(base, "..") || base[0] == '/' || strchr(base, '\\'))
	{
		return qfalse;
	}

	baseLen = (int)(ext - base);
	if (baseLen <= 0 || baseLen >= 48)
	{
		return qfalse;
	}

	Com_sprintf(out, outSize, "maps/xbox/%.*s.bsp", baseLen, base);
	return qtrue;
}

static void *EFBSP_AllocTemp(int size)
{
	if (size <= 0)
	{
		return NULL;
	}
	return Z_Malloc(size, TAG_TEMP_WORKSPACE, qfalse, 32);
}

static void EFBSP_FreeTemp(void *data)
{
	if (data)
	{
		Z_Free(data);
	}
}

static qboolean EFBSP_LoadFile(const char *name, efbspFile_t *bsp)
{
	char patchName[MAX_QPATH];
	const char *loadName;

	memset(bsp, 0, sizeof(*bsp));
	loadName = name;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_WriteCritical("STEFX_HW_BOOT: EFBSP_LoadFile resolving Xbox map path");
#endif
	if (EFBSP_BuildXboxPatchName(name, patchName, sizeof(patchName)) && FS_PK3PatchFileExists(patchName))
	{
		Com_Printf("STEFX: EF BSP using Xbox patch '%s' for '%s'\n", patchName, name);
		loadName = patchName;
	}

	XBLF("STEFX: EF BSP load request name='%s' load='%s'", name, loadName);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_WriteCritical(va("STEFX_HW_BOOT: EFBSP_LoadFile reading '%s'", loadName));
#endif
	bsp->len = FS_ReadFile(loadName, (void **)&bsp->data);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_WriteCritical(va("STEFX_HW_BOOT: EFBSP_LoadFile read returned len=%d data=%p", bsp->len, bsp->data));
#endif
	XBLF("STEFX: EF BSP load result load='%s' len=%d data=%p", loadName, bsp->len, bsp->data);
	if (bsp->len <= 0 || !bsp->data)
	{
		if (loadName != name)
		{
			Com_Printf("STEFX: EF BSP patch read failed '%s'; falling back to '%s'\n", loadName, name);
			bsp->len = FS_ReadFile(name, (void **)&bsp->data);
			XBLF("STEFX: EF BSP fallback result name='%s' len=%d data=%p", name, bsp->len, bsp->data);
			if (bsp->len > 0 && bsp->data)
			{
				bsp->header = (efbspHeader_t *)bsp->data;
				return qtrue;
			}
		}
		memset(bsp, 0, sizeof(*bsp));
		return qfalse;
	}
	bsp->header = (efbspHeader_t *)bsp->data;
	return qtrue;
}

static void EFBSP_FreeFile(efbspFile_t *bsp)
{
	if (bsp->data)
	{
		FS_FreeFile(bsp->data);
	}
	memset(bsp, 0, sizeof(*bsp));
}

static void EFBSP_Validate(const efbspFile_t *bsp, const char *name)
{
	int i;

	if (!bsp->data || bsp->len < (int)sizeof(efbspHeader_t))
	{
		Com_Error(ERR_DROP, "EF BSP: %s is too small for a BSP header", name);
	}
	if (memcmp(bsp->header->ident, EF_BSP_IDENT_STRING, 4) || bsp->header->version != EF_BSP_VERSION)
	{
		Com_Error(ERR_DROP, "EF BSP: %s is not IBSP version 46", name);
	}

	for (i = 0; i < EF_BSP_HEADER_LUMPS; ++i)
	{
		int ofs = bsp->header->lumps[i].fileofs;
		int len = bsp->header->lumps[i].filelen;
		if (ofs < 0 || len < 0 || ofs > bsp->len || len > bsp->len - ofs)
		{
			Com_Error(ERR_DROP, "EF BSP: %s has invalid lump %d ofs=%d len=%d file=%d",
				name, i, ofs, len, bsp->len);
		}
	}
}

static byte *EFBSP_LumpData(const efbspFile_t *bsp, int lump)
{
	return bsp->data + bsp->header->lumps[lump].fileofs;
}

static int EFBSP_LumpLen(const efbspFile_t *bsp, int lump)
{
	return bsp->header->lumps[lump].filelen;
}

static int EFBSP_CheckedCount(const char *what, int len, int size)
{
	if (size <= 0 || len % size)
	{
		Com_Error(ERR_DROP, "EF BSP: funny lump size for %s len=%d size=%d", what, len, size);
	}
	return len / size;
}

static int EFBSP_ClampInt(int value, int low, int high, const char *what)
{
	if (value < low || value > high)
	{
		Com_Error(ERR_DROP, "EF BSP: %s value %d outside %d..%d", what, value, low, high);
	}
	return value;
}

static short EFBSP_FloatToShort(float value, const char *what)
{
	int ivalue = (int)value;
	return (short)EFBSP_ClampInt(ivalue, -32768, 32767, what);
}

static unsigned int EFBSP_PackFirstCount(int first, int count, const char *what)
{
	EFBSP_ClampInt(first, 0, 0xFFFFF, what);
	EFBSP_ClampInt(count, 0, 0xFFF, what);
	return ((unsigned int)first << 12) | ((unsigned int)count & 0xFFF);
}

static byte EFBSP_PackLightmapNum(int lightmapNum)
{
	EFBSP_ClampInt(lightmapNum, -4, 251, "lightmapNum");
	return (byte)(lightmapNum + 4);
}

static void EFBSP_SetSingleLightStyle(byte styles[MAXLIGHTMAPS])
{
	styles[0] = LS_NORMAL;
	styles[1] = LS_NONE;
	styles[2] = LS_NONE;
	styles[3] = LS_NONE;
}

static void EFBSP_SetLightmapNums(int lightmapNum, byte nums[MAXLIGHTMAPS])
{
	nums[0] = EFBSP_PackLightmapNum(lightmapNum);
	nums[1] = EFBSP_PackLightmapNum(EF_LIGHTMAP_NONE);
	nums[2] = EFBSP_PackLightmapNum(EF_LIGHTMAP_NONE);
	nums[3] = EFBSP_PackLightmapNum(EF_LIGHTMAP_NONE);
}

static int EFBSP_ShaderCount(const efbspFile_t *bsp)
{
	return EFBSP_CheckedCount("shaders", EFBSP_LumpLen(bsp, EF_LUMP_SHADERS), sizeof(dshader_t));
}

static int EFBSP_SurfaceCount(const efbspFile_t *bsp)
{
	return EFBSP_CheckedCount("surfaces", EFBSP_LumpLen(bsp, EF_LUMP_SURFACES), sizeof(efbspSurface_t));
}

static void *EFBSP_ConvertShaders(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_ShaderCount(bsp);
	dshader_t *in = (dshader_t *)EFBSP_LumpData(bsp, EF_LUMP_SHADERS);
	dshader_t *out = (dshader_t *)EFBSP_AllocTemp(count * sizeof(dshader_t));
	int i;

	for (i = 0; i < count; ++i)
	{
		out[i] = in[i];

#ifdef _XBOX
		if (strstr(out[i].shader, "common/") || strstr(out[i].shader, "junk") ||
			strstr(out[i].shader, "sky") || strstr(out[i].shader, "portal"))
		{
			XBLF("STEFX_EFBSP_FLAGS shader='%s' surf=0x%x cont=0x%x",
				out[i].shader,
				out[i].surfaceFlags,
				out[i].contentFlags);
		}
#endif
	}

	*outLen = count * sizeof(dshader_t);
	return out;
}

static void *EFBSP_CopyLump(const efbspFile_t *bsp, int lump, int *outLen)
{
	int len = EFBSP_LumpLen(bsp, lump);
	void *out = EFBSP_AllocTemp(len);
	if (len > 0)
	{
		memcpy(out, EFBSP_LumpData(bsp, lump), len);
	}
	*outLen = len;
	return out;
}

static void *EFBSP_ConvertModels(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("models", EFBSP_LumpLen(bsp, EF_LUMP_MODELS), sizeof(efbspModel_t));
	efbspModel_t *in = (efbspModel_t *)EFBSP_LumpData(bsp, EF_LUMP_MODELS);
	dmodel_t *out = (dmodel_t *)EFBSP_AllocTemp(count * sizeof(dmodel_t));
	int i, j;

	for (i = 0; i < count; ++i)
	{
		for (j = 0; j < 3; ++j)
		{
			out[i].mins[j] = in[i].mins[j];
			out[i].maxs[j] = in[i].maxs[j];
		}
		out[i].firstSurface = in[i].firstSurface;
		out[i].numSurfaces = (unsigned short)EFBSP_ClampInt(in[i].numSurfaces, 0, 0xFFFF, "model numSurfaces");
		out[i].firstBrush = in[i].firstBrush;
		out[i].numBrushes = (unsigned short)EFBSP_ClampInt(in[i].numBrushes, 0, 0xFFFF, "model numBrushes");
	}

	*outLen = count * sizeof(dmodel_t);
	return out;
}

static void *EFBSP_ConvertNodes(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("nodes", EFBSP_LumpLen(bsp, EF_LUMP_NODES), sizeof(efbspNode_t));
	efbspNode_t *in = (efbspNode_t *)EFBSP_LumpData(bsp, EF_LUMP_NODES);
	dnode_t *out = (dnode_t *)EFBSP_AllocTemp(count * sizeof(dnode_t));
	int i, j;

	for (i = 0; i < count; ++i)
	{
		out[i].planeNum = in[i].planeNum;
		out[i].children[0] = (short)EFBSP_ClampInt(in[i].children[0], -32768, 32767, "node child0");
		out[i].children[1] = (short)EFBSP_ClampInt(in[i].children[1], -32768, 32767, "node child1");
		for (j = 0; j < 3; ++j)
		{
			out[i].mins[j] = (short)EFBSP_ClampInt(in[i].mins[j], -32768, 32767, "node mins");
			out[i].maxs[j] = (short)EFBSP_ClampInt(in[i].maxs[j], -32768, 32767, "node maxs");
		}
	}

	*outLen = count * sizeof(dnode_t);
	return out;
}

static void *EFBSP_ConvertLeafs(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("leafs", EFBSP_LumpLen(bsp, EF_LUMP_LEAFS), sizeof(efbspLeaf_t));
	efbspLeaf_t *in = (efbspLeaf_t *)EFBSP_LumpData(bsp, EF_LUMP_LEAFS);
	dleaf_t *out = (dleaf_t *)EFBSP_AllocTemp(count * sizeof(dleaf_t));
	int i, j;

	for (i = 0; i < count; ++i)
	{
		out[i].cluster = (short)EFBSP_ClampInt(in[i].cluster, -32768, 32767, "leaf cluster");
		out[i].area = (signed char)EFBSP_ClampInt(in[i].area, -128, 127, "leaf area");
		for (j = 0; j < 3; ++j)
		{
			out[i].mins[j] = (short)EFBSP_ClampInt(in[i].mins[j], -32768, 32767, "leaf mins");
			out[i].maxs[j] = (short)EFBSP_ClampInt(in[i].maxs[j], -32768, 32767, "leaf maxs");
		}
		out[i].firstLeafSurface = (unsigned short)EFBSP_ClampInt(in[i].firstLeafSurface, 0, 0xFFFF, "leaf firstLeafSurface");
		out[i].numLeafSurfaces = (unsigned short)EFBSP_ClampInt(in[i].numLeafSurfaces, 0, 0xFFFF, "leaf numLeafSurfaces");
		out[i].firstLeafBrush = (unsigned short)EFBSP_ClampInt(in[i].firstLeafBrush, 0, 0xFFFF, "leaf firstLeafBrush");
		out[i].numLeafBrushes = (unsigned short)EFBSP_ClampInt(in[i].numLeafBrushes, 0, 0xFFFF, "leaf numLeafBrushes");
	}

	*outLen = count * sizeof(dleaf_t);
	return out;
}

static void *EFBSP_ConvertBrushes(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int count = EFBSP_CheckedCount("brushes", EFBSP_LumpLen(bsp, EF_LUMP_BRUSHES), sizeof(efbspBrush_t));
	efbspBrush_t *in = (efbspBrush_t *)EFBSP_LumpData(bsp, EF_LUMP_BRUSHES);
	dbrush_t *out = (dbrush_t *)EFBSP_AllocTemp(count * sizeof(dbrush_t));
	int i;

	for (i = 0; i < count; ++i)
	{
		out[i].firstSide = in[i].firstSide;
		out[i].numSides = (byte)EFBSP_ClampInt(in[i].numSides, 0, 0xFF, "brush numSides");
		out[i].shaderNum = (unsigned short)EFBSP_ClampInt(in[i].shaderNum, 0, shaderCount - 1, "brush shaderNum");
	}

	*outLen = count * sizeof(dbrush_t);
	return out;
}

static void *EFBSP_ConvertBrushSides(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int count = EFBSP_CheckedCount("brushsides", EFBSP_LumpLen(bsp, EF_LUMP_BRUSHSIDES), sizeof(efbspBrushSide_t));
	efbspBrushSide_t *in = (efbspBrushSide_t *)EFBSP_LumpData(bsp, EF_LUMP_BRUSHSIDES);
	dbrushside_t *out = (dbrushside_t *)EFBSP_AllocTemp(count * sizeof(dbrushside_t));
	int i;

	for (i = 0; i < count; ++i)
	{
		out[i].planeNum = in[i].planeNum;
		out[i].shaderNum = (unsigned short)EFBSP_ClampInt(in[i].shaderNum, 0, shaderCount - 1, "brushside shaderNum");
	}

	*outLen = count * sizeof(dbrushside_t);
	return out;
}

static void *EFBSP_ConvertVerts(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("drawverts", EFBSP_LumpLen(bsp, EF_LUMP_DRAWVERTS), sizeof(efbspDrawVert_t));
	efbspDrawVert_t *in = (efbspDrawVert_t *)EFBSP_LumpData(bsp, EF_LUMP_DRAWVERTS);
	mapVert_t *out = (mapVert_t *)EFBSP_AllocTemp(count * sizeof(mapVert_t));
	int i, j, k;

	for (i = 0; i < count; ++i)
	{
		for (j = 0; j < MAXLIGHTMAPS; ++j)
		{
			if (j == 0)
			{
				out[i].lightmap[j][0] = in[i].lightmap[0] * POINTS_LIGHT_SCALE;
				out[i].lightmap[j][1] = in[i].lightmap[1] * POINTS_LIGHT_SCALE;
			}
			else
			{
				out[i].lightmap[j][0] = 0.0f;
				out[i].lightmap[j][1] = 0.0f;
			}
			for (k = 0; k < 4; ++k)
			{
				out[i].color[j][k] = in[i].color[k];
			}
		}

		out[i].st[0] = in[i].st[0];
		out[i].st[1] = in[i].st[1];
		for (j = 0; j < 3; ++j)
		{
			float n = in[i].normal[j];
			if (n < -1.0f) n = -1.0f;
			if (n > 1.0f) n = 1.0f;
			out[i].xyz[j] = EFBSP_FloatToShort(in[i].xyz[j], "vertex xyz");
			out[i].normal[j] = (short)(n * 32767.0f);
		}
	}

	*outLen = count * sizeof(mapVert_t);
	return out;
}

static void *EFBSP_ConvertIndexes(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("drawindexes", EFBSP_LumpLen(bsp, EF_LUMP_DRAWINDEXES), sizeof(int));
	int *in = (int *)EFBSP_LumpData(bsp, EF_LUMP_DRAWINDEXES);
	short *out = (short *)EFBSP_AllocTemp(count * sizeof(short));
	int i;

	for (i = 0; i < count; ++i)
	{
		out[i] = (short)EFBSP_ClampInt(in[i], -32768, 32767, "draw index");
	}

	*outLen = count * sizeof(short);
	return out;
}

static void EFBSP_FillSurfaceLight(const efbspSurface_t *in, byte styles[MAXLIGHTMAPS], byte nums[MAXLIGHTMAPS])
{
	EFBSP_SetSingleLightStyle(styles);
	EFBSP_SetLightmapNums(in->lightmapNum, nums);
}

static qboolean EFBSP_XboxPackedLumpsExist(const char *name)
{
	char mapName[MAX_QPATH];
	char stripName[MAX_QPATH];
	char probeName[MAX_QPATH];
	qboolean exists;

	if (!name || !name[0])
	{
		return qfalse;
	}

	Q_strncpyz(mapName, name, sizeof(mapName));
	COM_StripExtension(mapName, stripName);
	Com_sprintf(probeName, sizeof(probeName), "%s/misc.mle", stripName);
#ifdef _XBOX
	XBLog_WriteCriticalf("STEFX_RETAIL_LUMPS: packed probe begin map='%s' file='%s'", mapName, probeName);
#endif
	exists = FS_PK3PatchFileExists(probeName);
#ifdef _XBOX
	XBLog_WriteCriticalf("STEFX_RETAIL_LUMPS: packed probe end map='%s' file='%s' exists=%d", mapName, probeName, exists);
#endif
	return exists;
}

#if defined(STEFX_ELITE_FORCE_SP)
static void EFBSP_FillMapVert(const efbspDrawVert_t *in, mapVert_t *out)
{
	int j, k;

	for (j = 0; j < MAXLIGHTMAPS; ++j)
	{
		if (j == 0)
		{
			out->lightmap[j][0] = in->lightmap[0] * POINTS_LIGHT_SCALE;
			out->lightmap[j][1] = in->lightmap[1] * POINTS_LIGHT_SCALE;
		}
		else
		{
			out->lightmap[j][0] = 0.0f;
			out->lightmap[j][1] = 0.0f;
		}
		for (k = 0; k < 4; ++k)
		{
			out->color[j][k] = in->color[k];
		}
	}

	out->st[0] = in->st[0];
	out->st[1] = in->st[1];
	for (j = 0; j < 3; ++j)
	{
		float n = in->normal[j];
		if (n < -1.0f) n = -1.0f;
		if (n > 1.0f) n = 1.0f;
		out->xyz[j] = EFBSP_FloatToShort(in->xyz[j], "vertex xyz");
		out->normal[j] = (short)(n * 32767.0f);
	}
}

static void EFBSP_FillSurfaceVerts(const efbspFile_t *bsp, const efbspSurface_t *surface, mapVert_t *out)
{
	efbspDrawVert_t *verts = (efbspDrawVert_t *)EFBSP_LumpData(bsp, EF_LUMP_DRAWVERTS);
	int i;

	for (i = 0; i < surface->numVerts; ++i)
	{
		EFBSP_FillMapVert(&verts[surface->firstVert + i], &out[i]);
	}
}

static int EFBSP_CountSurfacesOfType(const efbspFile_t *bsp, int surfaceType)
{
	int surfaceCount = EFBSP_SurfaceCount(bsp);
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(bsp, EF_LUMP_SURFACES);
	int i, count = 0;

	for (i = 0; i < surfaceCount; ++i)
	{
		if (surfaces[i].surfaceType == surfaceType)
		{
			++count;
		}
	}

	return count;
}

static void EFBSP_FillPatchSurface(const efbspSurface_t *in, int shaderCount, int code, dpatch_t *out)
{
	int j;

	EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "patch shaderNum");
	EFBSP_ClampInt(in->fogNum, -128, 127, "patch fogNum");
	out->code = code;
	out->shaderNum = (unsigned short)in->shaderNum;
	out->fogNum = (signed char)in->fogNum;
	out->verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "patch verts");
	EFBSP_FillSurfaceLight(in, out->lightmapStyles, out->lightmapNum);
	for (j = 0; j < 3; ++j)
	{
		out->lightmapVecs[0][j] = EFBSP_FloatToShort(in->lightmapVecs[0][j], "patch lod0");
		out->lightmapVecs[1][j] = EFBSP_FloatToShort(in->lightmapVecs[1][j], "patch lod1");
	}
	out->patchWidth = (byte)EFBSP_ClampInt(in->patchWidth, 0, 255, "patch width");
	out->patchHeight = (byte)EFBSP_ClampInt(in->patchHeight, 0, 255, "patch height");
}

static void EFBSP_FillTriSurfSurface(const efbspSurface_t *in, int shaderCount, int code, dtrisurf_t *out)
{
	EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "trisurf shaderNum");
	EFBSP_ClampInt(in->fogNum, -128, 127, "trisurf fogNum");
	out->code = code;
	out->shaderNum = (unsigned short)in->shaderNum;
	out->fogNum = (signed char)in->fogNum;
	out->verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "trisurf verts");
	out->indexes = EFBSP_PackFirstCount(in->firstIndex, in->numIndexes, "trisurf indexes");
	EFBSP_SetSingleLightStyle(out->lightmapStyles);
}

static void EFBSP_FillFaceSurface(const efbspFile_t *bsp, const efbspSurface_t *in, int shaderCount, int code, dface_t *out)
{
	int vertCount = EFBSP_CheckedCount("drawverts", EFBSP_LumpLen(bsp, EF_LUMP_DRAWVERTS), sizeof(efbspDrawVert_t));
	int indexCount = EFBSP_CheckedCount("drawindexes", EFBSP_LumpLen(bsp, EF_LUMP_DRAWINDEXES), sizeof(int));
	int *indexes = (int *)EFBSP_LumpData(bsp, EF_LUMP_DRAWINDEXES);
	int j;

	EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "face shaderNum");
	EFBSP_ClampInt(in->fogNum, -128, 127, "face fogNum");
	EFBSP_ClampInt(in->numVerts, 0, 255, "face numVerts");
	if (in->firstVert < 0 || in->numVerts < 0 || in->firstVert + in->numVerts > vertCount)
	{
		Com_Error(ERR_DROP, "EF BSP: face %d verts outside drawvert lump", code);
	}
	if (in->firstIndex < 0 || in->numIndexes < 0 || in->firstIndex + in->numIndexes > indexCount)
	{
		Com_Error(ERR_DROP, "EF BSP: face %d indexes outside drawindex lump", code);
	}
	for (j = 0; j < in->numIndexes; ++j)
	{
		int localIndex = indexes[in->firstIndex + j];
		EFBSP_ClampInt(localIndex, 0, in->numVerts - 1, "face local index");
		EFBSP_ClampInt(localIndex, 0, 255, "face byte index");
	}

	out->code = code;
	out->shaderNum = (unsigned short)in->shaderNum;
	out->fogNum = (signed char)in->fogNum;
	out->verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "face verts");
	out->indexes = EFBSP_PackFirstCount(in->firstIndex, in->numIndexes, "face indexes");
	EFBSP_FillSurfaceLight(in, out->lightmapStyles, out->lightmapNum);
	for (j = 0; j < 3; ++j)
	{
		float n = in->lightmapVecs[2][j];
		if (n < -1.0f) n = -1.0f;
		if (n > 1.0f) n = 1.0f;
		out->lightmapVecs[j] = (short)(n * 32767.0f);
	}
}

static void EFBSP_FillFlareSurface(const efbspSurface_t *in, int shaderCount, int code, dflare_t *out)
{
	int j;

	EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "flare shaderNum");
	EFBSP_ClampInt(in->fogNum, -128, 127, "flare fogNum");
	out->code = code;
	out->shaderNum = (unsigned short)in->shaderNum;
	out->fogNum = (signed char)in->fogNum;
	for (j = 0; j < 3; ++j)
	{
		float n = in->lightmapVecs[2][j];
		if (n < -1.0f) n = -1.0f;
		if (n > 1.0f) n = 1.0f;
		out->origin[j] = EFBSP_FloatToShort(in->lightmapOrigin[j], "flare origin");
		out->normal[j] = (short)(n * 32767.0f);
		out->color[j] = (byte)EFBSP_ClampInt((int)in->lightmapVecs[0][j], 0, 255, "flare color");
	}
}
#endif

static void *EFBSP_ConvertFaces(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int surfaceCount = EFBSP_SurfaceCount(bsp);
	int indexCount = EFBSP_CheckedCount("drawindexes", EFBSP_LumpLen(bsp, EF_LUMP_DRAWINDEXES), sizeof(int));
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(bsp, EF_LUMP_SURFACES);
	int *indexes = (int *)EFBSP_LumpData(bsp, EF_LUMP_DRAWINDEXES);
	dface_t *out = (dface_t *)EFBSP_AllocTemp(surfaceCount * sizeof(dface_t));
	int i, j, count;

	count = 0;
	for (i = 0; i < surfaceCount; ++i)
	{
		efbspSurface_t *in = &surfaces[i];
		if (in->surfaceType != EF_MST_PLANAR)
		{
			continue;
		}

		EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "face shaderNum");
		EFBSP_ClampInt(in->fogNum, -128, 127, "face fogNum");
		EFBSP_ClampInt(in->numVerts, 0, 255, "face numVerts");
		if (in->firstIndex < 0 || in->numIndexes < 0 || in->firstIndex + in->numIndexes > indexCount)
		{
			Com_Error(ERR_DROP, "EF BSP: face %d indexes outside drawindex lump", i);
		}
		for (j = 0; j < in->numIndexes; ++j)
		{
			int localIndex = indexes[in->firstIndex + j];
			EFBSP_ClampInt(localIndex, 0, in->numVerts - 1, "face local index");
			EFBSP_ClampInt(localIndex, 0, 255, "face byte index");
		}

		out[count].code = i;
		out[count].shaderNum = (unsigned short)in->shaderNum;
		out[count].fogNum = (signed char)in->fogNum;
		out[count].verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "face verts");
		out[count].indexes = EFBSP_PackFirstCount(in->firstIndex, in->numIndexes, "face indexes");
		EFBSP_FillSurfaceLight(in, out[count].lightmapStyles, out[count].lightmapNum);
		for (j = 0; j < 3; ++j)
		{
			float n = in->lightmapVecs[2][j];
			if (n < -1.0f) n = -1.0f;
			if (n > 1.0f) n = 1.0f;
			out[count].lightmapVecs[j] = (short)(n * 32767.0f);
		}
		++count;
	}

	*outLen = count * sizeof(dface_t);
	return out;
}

static void *EFBSP_ConvertPatches(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int surfaceCount = EFBSP_SurfaceCount(bsp);
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(bsp, EF_LUMP_SURFACES);
	dpatch_t *out = (dpatch_t *)EFBSP_AllocTemp(surfaceCount * sizeof(dpatch_t));
	int i, j, count;

	count = 0;
	for (i = 0; i < surfaceCount; ++i)
	{
		efbspSurface_t *in = &surfaces[i];
		if (in->surfaceType != EF_MST_PATCH)
		{
			continue;
		}
		EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "patch shaderNum");
		EFBSP_ClampInt(in->fogNum, -128, 127, "patch fogNum");
		out[count].code = i;
		out[count].shaderNum = (unsigned short)in->shaderNum;
		out[count].fogNum = (signed char)in->fogNum;
		out[count].verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "patch verts");
		EFBSP_FillSurfaceLight(in, out[count].lightmapStyles, out[count].lightmapNum);
		for (j = 0; j < 3; ++j)
		{
			out[count].lightmapVecs[0][j] = EFBSP_FloatToShort(in->lightmapVecs[0][j], "patch lod0");
			out[count].lightmapVecs[1][j] = EFBSP_FloatToShort(in->lightmapVecs[1][j], "patch lod1");
		}
		out[count].patchWidth = (byte)EFBSP_ClampInt(in->patchWidth, 0, 255, "patch width");
		out[count].patchHeight = (byte)EFBSP_ClampInt(in->patchHeight, 0, 255, "patch height");
		++count;
	}

	*outLen = count * sizeof(dpatch_t);
	return out;
}

static void *EFBSP_ConvertTriSurfs(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int surfaceCount = EFBSP_SurfaceCount(bsp);
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(bsp, EF_LUMP_SURFACES);
	dtrisurf_t *out = (dtrisurf_t *)EFBSP_AllocTemp(surfaceCount * sizeof(dtrisurf_t));
	int i, count;

	count = 0;
	for (i = 0; i < surfaceCount; ++i)
	{
		efbspSurface_t *in = &surfaces[i];
		if (in->surfaceType != EF_MST_TRIANGLE_SOUP)
		{
			continue;
		}
		EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "trisurf shaderNum");
		EFBSP_ClampInt(in->fogNum, -128, 127, "trisurf fogNum");
		out[count].code = i;
		out[count].shaderNum = (unsigned short)in->shaderNum;
		out[count].fogNum = (signed char)in->fogNum;
		out[count].verts = EFBSP_PackFirstCount(in->firstVert, in->numVerts, "trisurf verts");
		out[count].indexes = EFBSP_PackFirstCount(in->firstIndex, in->numIndexes, "trisurf indexes");
		EFBSP_SetSingleLightStyle(out[count].lightmapStyles);
		++count;
	}

	*outLen = count * sizeof(dtrisurf_t);
	return out;
}

static void *EFBSP_ConvertFlares(const efbspFile_t *bsp, int shaderCount, int *outLen)
{
	int surfaceCount = EFBSP_SurfaceCount(bsp);
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(bsp, EF_LUMP_SURFACES);
	dflare_t *out = (dflare_t *)EFBSP_AllocTemp(surfaceCount * sizeof(dflare_t));
	int i, j, count;

	count = 0;
	for (i = 0; i < surfaceCount; ++i)
	{
		efbspSurface_t *in = &surfaces[i];
		if (in->surfaceType != EF_MST_FLARE)
		{
			continue;
		}
		EFBSP_ClampInt(in->shaderNum, 0, shaderCount - 1, "flare shaderNum");
		EFBSP_ClampInt(in->fogNum, -128, 127, "flare fogNum");
		out[count].code = i;
		out[count].shaderNum = (unsigned short)in->shaderNum;
		out[count].fogNum = (signed char)in->fogNum;
		for (j = 0; j < 3; ++j)
		{
			float n = in->lightmapVecs[2][j];
			if (n < -1.0f) n = -1.0f;
			if (n > 1.0f) n = 1.0f;
			out[count].origin[j] = EFBSP_FloatToShort(in->lightmapOrigin[j], "flare origin");
			out[count].normal[j] = (short)(n * 32767.0f);
			out[count].color[j] = (byte)EFBSP_ClampInt((int)in->lightmapVecs[0][j], 0, 255, "flare color");
		}
		++count;
	}

	*outLen = count * sizeof(dflare_t);
	return out;
}

static void *EFBSP_ConvertVisibility(const efbspFile_t *bsp, int *outLen)
{
	byte *in;
	int len = EFBSP_LumpLen(bsp, EF_LUMP_VISIBILITY);
	int maxOut;
	char *out;
	SPARC<byte> vis;
	int saved;

	if (len == 0)
	{
		*outLen = 0;
		return NULL;
	}
	if (len < 8)
	{
		Com_Error(ERR_DROP, "EF BSP: visibility lump is too small");
	}

	in = EFBSP_LumpData(bsp, EF_LUMP_VISIBILITY);
	maxOut = len * 2 + 64;
	out = (char *)EFBSP_AllocTemp(maxOut);
	((int *)out)[0] = ((int *)in)[0];
	((int *)out)[1] = ((int *)in)[1];

	vis.Compress(in + 8, len - 8, 0);
	saved = vis.Save(out + 8, maxOut - 8, false) + 8;
	*outLen = saved;
	return out;
}

static void *EFBSP_ConvertLightGrid(const efbspFile_t *bsp, int *outLen)
{
	int count = EFBSP_CheckedCount("lightgrid", EFBSP_LumpLen(bsp, EF_LUMP_LIGHTGRID), EF_LIGHTGRID_RECORD_SIZE);
	byte *in = EFBSP_LumpData(bsp, EF_LUMP_LIGHTGRID);
	int headerSize = count * 7;
	byte *out = (byte *)EFBSP_AllocTemp(count * 14);
	byte *pool = out + headerSize;
	int poolUsed = 0;
	int i;

	for (i = 0; i < count; ++i)
	{
		byte *src = in + i * EF_LIGHTGRID_RECORD_SIZE;
		byte flags = (1 << 4);
		int dataOfs = headerSize + poolUsed;
		byte *rec = out + i * 7;

		pool[poolUsed++] = LS_NORMAL;
		if (src[0] || src[1] || src[2] || src[3] || src[4] || src[5])
		{
			flags |= 1;
			memcpy(pool + poolUsed, src, 6);
			poolUsed += 6;
		}

		rec[0] = flags;
		rec[1] = src[6];
		rec[2] = src[7];
		*(int *)(rec + 3) = dataOfs;
	}

	*outLen = headerSize + poolUsed;
	return out;
}

static int EFBSP_ExpectedLightGridElements(const efbspFile_t *bsp)
{
	efbspModel_t *models = (efbspModel_t *)EFBSP_LumpData(bsp, EF_LUMP_MODELS);
	float gridSize[3];
	int bounds[3];
	int i;

	EFBSP_CheckedCount("models", EFBSP_LumpLen(bsp, EF_LUMP_MODELS), sizeof(efbspModel_t));
	gridSize[0] = 64.0f;
	gridSize[1] = 64.0f;
	gridSize[2] = 128.0f;

	for (i = 0; i < 3; ++i)
	{
		float origin = gridSize[i] * (float)ceil(models[0].mins[i] / gridSize[i]);
		float maxs = gridSize[i] * (float)floor(models[0].maxs[i] / gridSize[i]);
		bounds[i] = (int)((maxs - origin) / gridSize[i] + 1.0f);
	}

	return bounds[0] * bounds[1] * bounds[2];
}

static void *EFBSP_ConvertLightArray(const efbspFile_t *bsp, int *outLen)
{
	int rawCount = EFBSP_CheckedCount("lightgrid", EFBSP_LumpLen(bsp, EF_LUMP_LIGHTGRID), EF_LIGHTGRID_RECORD_SIZE);
	int expected = EFBSP_ExpectedLightGridElements(bsp);
	unsigned short *out;
	int i;

	if (rawCount != expected || rawCount > 0xFFFF)
	{
		*outLen = 0;
		return NULL;
	}

	out = (unsigned short *)EFBSP_AllocTemp(rawCount * sizeof(unsigned short));
	for (i = 0; i < rawCount; ++i)
	{
		out[i] = (unsigned short)i;
	}

	*outLen = rawCount * sizeof(unsigned short);
	return out;
}

#endif // STEFX_ELITE_FORCE_SP

#endif // EF_BSP_XBOX_SHARED_H
