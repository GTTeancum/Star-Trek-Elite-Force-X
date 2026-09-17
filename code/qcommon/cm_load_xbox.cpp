// cmodel.c -- model loading

#include "cm_local.h"
#include "cm_patch.h"
#include "../renderer/tr_local.h"
#include "../RMG/RM_Headers.h"

#include "sparc.h"
#ifdef STEFX_ELITE_FORCE_SP
#include "../win32/xb_log.h"
#include "ef_bsp_xbox_shared.h"
extern "C" volatile unsigned int g_SPXBPhaseLast;
extern "C" volatile unsigned int g_SPXBPackedMapPhase;
extern "C" volatile unsigned int g_SPXBDirectMapStatus;
extern "C" volatile unsigned int g_SPXBDirectMapHash;
extern "C" volatile unsigned int g_SPXBDirectMapQueuedCount;
#endif
#include "../zlib/zlib.h"

static SPARC<byte> visData;

void *SparcAllocator(unsigned int size)
{
	return Z_Malloc(size, TAG_BSP, false);
}

void SparcDeallocator(void *ptr)
{
	Z_Free(ptr);
}

extern world_t	s_worldData;


void CM_LoadShaderText(bool forceReload);

#ifdef BSPC
void SetPlaneSignbits (cplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}
#endif //BSPC

// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define	BOX_BRUSHES		1
#define	BOX_SIDES		6
#define	BOX_LEAFS		2
#define	BOX_PLANES		12

#define	LL(x) x=LittleLong(x)

clipMap_t	cmg;
int			c_pointcontents;
int			c_traces, c_brush_traces, c_patch_traces;

#ifdef STEFX_ELITE_FORCE_SP
static char		s_efRawLoadedMapName[MAX_QPATH];
static unsigned	s_efRawLoadedMapChecksum;
static qboolean	s_efRawLoadedMapReady = qfalse;

static void CM_EFRememberRawMap( const char *name, unsigned checksum )
{
	Q_strncpyz( s_efRawLoadedMapName, name, sizeof( s_efRawLoadedMapName ) );
	s_efRawLoadedMapChecksum = checksum;
	s_efRawLoadedMapReady = qtrue;
}

static void CM_EFForgetRawMap( const char *reason )
{
	if ( s_efRawLoadedMapReady || s_efRawLoadedMapName[0] ) {
		XBLF("STEFX: CM raw BSP cache forget reason='%s' map='%s'",
			reason ? reason : "(null)",
			s_efRawLoadedMapName[0] ? s_efRawLoadedMapName : "(none)");
	}
	s_efRawLoadedMapName[0] = '\0';
	s_efRawLoadedMapChecksum = 0;
	s_efRawLoadedMapReady = qfalse;
}

static qboolean CM_EFCanReuseRawMap( const char *name )
{
	if ( !s_efRawLoadedMapReady || !s_efRawLoadedMapName[0] || !name || !name[0] ) {
		return qfalse;
	}
	if ( Q_stricmp( s_efRawLoadedMapName, name ) ) {
		return qfalse;
	}
	if ( !cmg.cmodels || cmg.numSubModels <= 0 ) {
		return qfalse;
	}
	return qtrue;
}

static void CM_EFLogMemoryStats( const char *label, const char *name )
{
	zmemstats_t stats;

	Z_GetMemoryStats( &stats );
	XBLF("STEFX: BSP_MEM label='%s' map='%s' used=%d peak=%d free=%d largest=%d bsp=%d filesys=%d sound=%d",
		label ? label : "(null)",
		name ? name : "(null)",
		stats.usedBytes,
		stats.peakBytes,
		stats.freeBytes,
		stats.largestFreeBlock,
		stats.bspBytes,
		stats.filesysBytes,
		stats.soundRawBytes);
}
#endif

byte		*cmod_base;

#ifndef BSPC
cvar_t		*cm_noAreas;
cvar_t		*cm_noCurves;
cvar_t		*cm_playerCurveClip;
#endif

cmodel_t	box_model;
cplane_t	*box_planes;
cbrush_t	*box_brush;

int			CM_OrOfAllContentsFlagsInMap;


void	CM_InitBoxHull (void);
void	CM_FloodAreaConnections (void);

clipMap_t	SubBSP[MAX_SUB_BSP];
int			NumSubBSP = 0, TotalSubModels = 0;

/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadShaders
=================
*/
void CMod_LoadShaders( void *data, int len ) {
	dshader_t	*in;
	int			i, count;
	CCMShader	*out;

	in = (dshader_t *)(data);
	if (len % sizeof(*in)) {
		Com_Error (ERR_DROP, "CMod_LoadShaders: funny lump size");
	}
	count = len / sizeof(*in);

	if (count < 1) {
		Com_Error (ERR_DROP, "Map with no shaders");
	}
	cmg.shaders = (CCMShader *) Z_Malloc( count * sizeof( *cmg.shaders ), TAG_BSP, qfalse);
	cmg.numShaders = count;
	s_worldData.shaders = (dshader_t *) Z_Malloc ( count*sizeof(dshader_t), TAG_BSP, qfalse );
	s_worldData.numShaders = count;

	out = cmg.shaders;
	for ( i = 0; i < count; i++, in++, out++ ) 
	{
		Q_strncpyz(out->shader, in->shader, MAX_QPATH);
		out->contentFlags = in->contentFlags;
		out->surfaceFlags = in->surfaceFlags;

		Q_strncpyz(s_worldData.shaders[i].shader, in->shader, MAX_QPATH);
		s_worldData.shaders[i].contentFlags = in->contentFlags;
		s_worldData.shaders[i].surfaceFlags = in->surfaceFlags;

#ifdef STEFX_ELITE_FORCE_SP
		XBLF("STEFX_SHADER_LUMP index=%d name='%s' surf=0x%x cont=0x%x",
			i,
			s_worldData.shaders[i].shader,
			out->surfaceFlags,
			out->contentFlags);
#endif
	}
}


/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels( void *data, int len ) {
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	int			*indexes;

	in = (dmodel_t *)(data);
	if (len % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadSubmodels: funny lump size");
	count = len / sizeof(*in);

	if (count < 1) {
		Com_Error (ERR_DROP, "Map with no models");
	}

	if ( count > MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "MAX_SUBMODELS (%d) exceeded by %d", MAX_SUBMODELS, count-MAX_SUBMODELS );
	}

	cmg.cmodels = (struct cmodel_s *) Z_Malloc( count * sizeof( *cmg.cmodels ), TAG_BSP, qtrue );
	cmg.numSubModels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &cmg.cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j] - 1;
			out->maxs[j] = in->maxs[j] + 1;
		}

		if ( i == 0 ) {
			continue;	// world model doesn't need other info
		}

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numLeafBrushes = in->numBrushes;
		indexes = (int *) Z_Malloc( out->leaf.numLeafBrushes * 4, TAG_BSP, qfalse);
		out->leaf.firstLeafBrush = indexes - cmg.leafbrushes;
		for ( j = 0 ; j < out->leaf.numLeafBrushes ; j++ ) {
			indexes[j] = in->firstBrush + j;
		}

		out->leaf.numLeafSurfaces = in->numSurfaces;
		indexes = (int *) Z_Malloc( out->leaf.numLeafSurfaces * 4, TAG_BSP, qfalse);
		out->leaf.firstLeafSurface = indexes - cmg.leafsurfaces;
		for ( j = 0 ; j < out->leaf.numLeafSurfaces ; j++ ) {
			indexes[j] = in->firstSurface + j;
		}
	}
}

/*
=================
CMod_LoadNodes

=================
*/
void CMod_LoadNodes( void *data, int len ) {
	dnode_t		*in;
	cNode_t		*out;
	int			i, count;
	
	in = (dnode_t *)(data);
	if (len % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = len / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	cmg.nodes = (cNode_t *) Z_Malloc( count * sizeof( *cmg.nodes ), TAG_BSP, qfalse);
	cmg.numNodes = count;

	out = cmg.nodes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planeNum = in->planeNum;
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
	}
}

/*
=================
CM_BoundBrush

=================
*/
void CM_BoundBrush( cbrush_t *b ) {
	b->bounds[0][0] = -cmg.planes[b->sides[0].planeNum.GetValue()].dist;
	b->bounds[1][0] = cmg.planes[b->sides[1].planeNum.GetValue()].dist;

	b->bounds[0][1] = -cmg.planes[b->sides[2].planeNum.GetValue()].dist;
	b->bounds[1][1] = cmg.planes[b->sides[3].planeNum.GetValue()].dist;

	b->bounds[0][2] = -cmg.planes[b->sides[4].planeNum.GetValue()].dist;
	b->bounds[1][2] = cmg.planes[b->sides[5].planeNum.GetValue()].dist;
}


/*
=================
CMod_LoadBrushes

=================
*/
void CMod_LoadBrushes( void *data, int len ) {
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;

	in = (dbrush_t *)(data);
	if (len % sizeof(*in)) {
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	}
	count = len / sizeof(*in);

	cmg.brushes = (cbrush_t *) Z_Malloc( ( BOX_BRUSHES + count ) * sizeof( *cmg.brushes ), TAG_BSP, qfalse);
	cmg.numBrushes = count;

	out = cmg.brushes;

	for ( i=0 ; i<count ; i++, out++, in++ ) {
		out->sides = cmg.brushsides + in->firstSide;
		out->numsides = in->numSides;

		out->shaderNum = in->shaderNum;
		if ( out->shaderNum < 0 || out->shaderNum >= cmg.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushes: bad shaderNum: %i", out->shaderNum );
		}
		out->contents = cmg.shaders[out->shaderNum].contentFlags;
		//TEMP HACK: for water that cuts vis but is not solid!!!
		if ( cmg.shaders[out->shaderNum].surfaceFlags & SURF_SLICK )
		{
			out->contents &= ~CONTENTS_SOLID;
		}

		CM_OrOfAllContentsFlagsInMap |= out->contents;

		CM_BoundBrush( out );
	}

}

/*
=================
CMod_LoadLeafs
=================
*/
void CMod_LoadLeafs (void *data, int len)
{
	int			i;
	cLeaf_t		*out;
	dleaf_t 	*in;
	int			count;
	
	in = (dleaf_t *)(data);
	if (len % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = len / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	cmg.leafs = (cLeaf_t *) Z_Malloc( ( BOX_LEAFS + count ) * sizeof( *cmg.leafs ), TAG_BSP, qfalse);
	cmg.numLeafs = count;
	out = cmg.leafs;	

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->cluster = in->cluster;
		out->area = in->area;
		out->firstLeafBrush = in->firstLeafBrush;
		out->numLeafBrushes = in->numLeafBrushes;
		out->firstLeafSurface = in->firstLeafSurface;
		out->numLeafSurfaces = in->numLeafSurfaces;

		if (out->cluster >= cmg.numClusters)
			cmg.numClusters = out->cluster + 1;
		if (out->area >= cmg.numAreas)
			cmg.numAreas = out->area + 1;
	}

	cmg.areas = (cArea_t *) Z_Malloc( cmg.numAreas * sizeof( *cmg.areas ), TAG_BSP, qtrue );

	extern qboolean vidRestartReloadMap;
	if (!vidRestartReloadMap)
	{
		cmg.areaPortals = (int *) Z_Malloc( cmg.numAreas * cmg.numAreas * sizeof( *cmg.areaPortals ), TAG_BSP, qtrue );
	}
}

/*
=================
CMod_LoadPlanes
=================
*/
void CMod_LoadPlanes (void *data, int len)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (dplane_t *)(data);
	if (len % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = len / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	cmg.planes = (struct cplane_s *) Z_Malloc( ( BOX_PLANES + count ) * sizeof( *cmg.planes ), TAG_BSP, qfalse);
	cmg.numPlanes = count;

	out = cmg.planes;	

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = in->dist;
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
void CMod_LoadLeafBrushes (void *data, int len)
{
	int			*out;
	int		 	*in;
	int			count;
	
	in = (int *)(data);
	if (len % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = len / sizeof(*in);

	cmg.leafbrushes = (int *) Z_Malloc( ( BOX_BRUSHES + count ) * sizeof( *cmg.leafbrushes ), TAG_BSP, qfalse);
	cmg.numLeafBrushes = count;

	out = cmg.leafbrushes;

	memcpy(out, in, len);
}

/*
=================
CMod_LoadBrushSides
=================
*/
void CMod_LoadBrushSides (void *data, int len)
{
	int				i;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int				count;

	in = (dbrushside_t *)(data);
	if ( len % sizeof(*in) ) {
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	}
	count = len / sizeof(*in);

	cmg.brushsides = (cbrushside_t *) Z_Malloc( ( BOX_SIDES + count ) * sizeof( *cmg.brushsides ), TAG_BSP, qfalse);
	cmg.numBrushSides = count;

	out = cmg.brushsides;	

	for ( i=0 ; i<count ; i++, in++, out++) {
		out->planeNum = in->planeNum;
		assert(in->planeNum == out->planeNum.GetValue());

		out->shaderNum = in->shaderNum;
		if ( out->shaderNum < 0 || out->shaderNum >= cmg.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushSides: bad shaderNum: %i", out->shaderNum );
		}
	}
}


/*
=================
CMod_LoadEntityString
=================
*/
void CMod_LoadEntityString( void *data, int len ) {
	cmg.entityString = (char *) Z_Malloc( len, TAG_BSP, qfalse);
	cmg.numEntityChars = len;
	memcpy (cmg.entityString, data, len);
}

/*
=================
CMod_LoadVisibility
=================
*/
#define	VIS_HEADER	8
void CMod_LoadVisibility( void *data, int len ) {
	char	*buf;
	unsigned int loadedSize;

	if ( !len ) {
		cmg.vised = qfalse;
		cmg.visibility = NULL;
		RE_SetWorldVisData(NULL);
		return;
	}
	buf = (char*)data;

	visData.SetAllocator(SparcAllocator, SparcDeallocator);

	cmg.vised = qtrue;
	cmg.numClusters = ((int *)buf)[0];
	cmg.clusterBytes = ((int *)buf)[1];
	loadedSize = visData.Load(buf + VIS_HEADER, len - VIS_HEADER);
	if (!loadedSize) {
		Com_Error(ERR_DROP, "CMod_LoadVisibility: invalid compressed visibility data");
	}
	cmg.visibility = &visData;
	RE_SetWorldVisData(&visData);
}

//==================================================================


/*
=================
CMod_LoadPatches
=================
*/
#define	MAX_PATCH_VERTS		1024

static void CMod_LoadPatchesInternal( void *verts, int vertlen, LumpStream *vertStream,
		void *surfaces, int surfacelen, int numsurfs ) {
	mapVert_t	*dv, *dv_p;
	mapVert_t	*surfaceVerts = NULL;
	dpatch_t	*in;
	int			count;
	int			i, j;
	int			c;
	cPatch_t	*patch;
	vec3_t		points[MAX_PATCH_VERTS];
	int			width, height;
	int			shaderNum;

	count = surfacelen / sizeof(*in);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F01;
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLF("STEFX: CMod_LoadPatches begin patches=%d numsurfs=%d surfaceLen=%d vertLen=%d",
		count, numsurfs, surfacelen, vertlen);
#endif

	cmg.numSurfaces = numsurfs;
	cmg.surfaces = (cPatch_t **) Z_Malloc( cmg.numSurfaces * sizeof( cmg.surfaces[0] ), TAG_BSP, qtrue );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F02;
#endif

	dv = (mapVert_t *)(verts);
	if (vertlen % sizeof(*dv))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	if (vertStream)
	{
		surfaceVerts = (mapVert_t*)Z_Malloc(
			MAX_PATCH_VERTS * sizeof(mapVert_t), TAG_TEMP_WORKSPACE, qfalse, 32);
	}

	unsigned char* patchScratch = (unsigned char*)Z_Malloc( sizeof( *patch ) * count, TAG_BSP, qtrue);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F03;
#endif
	
	extern void CM_GridAlloc();
	extern void CM_PatchCollideFromGridTempAlloc();
	extern void CM_PreparePatchCollide(int num);
	extern void CM_TempPatchPlanesAlloc();
	CM_GridAlloc();
	CM_PatchCollideFromGridTempAlloc();
	CM_PreparePatchCollide(count);
	CM_TempPatchPlanesAlloc();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_Write("STEFX: CMod_LoadPatches temp collision buffers ready");
#endif

	facetLoad_t *facetbuf = (facetLoad_t*)Z_Malloc(
		MAX_PATCH_PLANES*sizeof(facetLoad_t), TAG_TEMP_WORKSPACE, qfalse);
	
	int *gridbuf = (int*)Z_Malloc(
		CM_MAX_GRID_SIZE*CM_MAX_GRID_SIZE*2*sizeof(int), TAG_TEMP_WORKSPACE, qfalse);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_Write("STEFX: CMod_LoadPatches facet/grid buffers allocated");
#endif

	for ( i = 0 ; i < count ; i++) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D1000 | (i & 0x0fff);
#endif
		in = (dpatch_t *)surfaces + i;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D5000 | (i & 0x0fff);
#endif

		cmg.surfaces[ in->code ] = patch = (cPatch_t *) patchScratch;
		patchScratch += sizeof( *patch );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D6000 | (i & 0x0fff);
#endif

		// load the full drawverts onto the stack
		width = in->patchWidth;
		height = in->patchHeight;
		c = width * height;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D7000 | (i & 0x0fff);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ((i % 8) == 0 || i + 1 == count) {
			g_SPXBPackedMapPhase = 0x504DA000 | (i & 0x0fff);
			XBLF("STEFX: CMod_LoadPatches patch %d/%d code=%d shader=%d size=%dx%d verts=%d",
				i + 1, count, in->code, in->shaderNum, width, height, c);
			g_SPXBPackedMapPhase = 0x504DB000 | (i & 0x0fff);
		}
#endif
		if ( c > MAX_PATCH_VERTS ) {
			Com_Error( ERR_DROP, "ParseMesh: MAX_PATCH_VERTS" );
		}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504DC000 | (i & 0x0fff);
#endif
		if (vertStream)
		{
			const int firstVert = in->verts >> 12;
			const int bytes = c * sizeof(mapVert_t);
			if (!vertStream->readAt(firstVert * sizeof(mapVert_t), surfaceVerts, bytes))
			{
				Com_Error(ERR_DROP, "CMod_LoadPatches: packed vertex read failed at patch %d", i);
			}
			dv_p = surfaceVerts;
		}
		else
		{
			dv_p = dv + (in->verts >> 12);
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBDirectMapStatus = (unsigned int)dv;
		g_SPXBDirectMapHash = (unsigned int)dv_p;
		g_SPXBDirectMapQueuedCount = in->verts;
#endif
		for ( j = 0 ; j < c ; j++, dv_p++ ) {
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			g_SPXBPackedMapPhase = 0x504DD000 | (j & 0x0fff);
#endif
			points[j][0] = dv_p->xyz[0];
			points[j][1] = dv_p->xyz[1];
			points[j][2] = dv_p->xyz[2];
		}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D8000 | (i & 0x0fff);
#endif

		shaderNum = in->shaderNum;
		patch->contents = cmg.shaders[shaderNum].contentFlags;
		CM_OrOfAllContentsFlagsInMap |= patch->contents;

		patch->surfaceFlags = cmg.shaders[shaderNum].surfaceFlags;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D9000 | (i & 0x0fff);
#endif

		// create the internal facet structure
		patch->pc = CM_GeneratePatchCollide( width, height, points, facetbuf, gridbuf );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		g_SPXBPackedMapPhase = 0x504D2000 | (i & 0x0fff);
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ((i % 8) == 0 || i + 1 == count) {
			XBLF("STEFX: CMod_LoadPatches patch %d/%d collide=%p",
				i + 1, count, patch->pc);
		}
#endif
	}

	extern void CM_GridDealloc();
	extern void CM_PatchCollideFromGridTempDealloc();
	extern void CM_TempPatchPlanesDealloc();
	CM_PatchCollideFromGridTempDealloc();
	CM_GridDealloc();
	CM_TempPatchPlanesDealloc();

	Z_Free(gridbuf);
	Z_Free(facetbuf);
	if (surfaceVerts)
	{
		Z_Free(surfaceVerts);
	}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	XBLog_Write("STEFX: CMod_LoadPatches done");
#endif
}

#if defined(STEFX_ELITE_FORCE_SP)
static void CMod_LoadRawEFPatches(const efbspFile_t *efbsp, int shaderCount, int numsurfs)
{
	efbspSurface_t *surfaces = (efbspSurface_t *)EFBSP_LumpData(efbsp, EF_LUMP_SURFACES);
	efbspDrawVert_t *verts = (efbspDrawVert_t *)EFBSP_LumpData(efbsp, EF_LUMP_DRAWVERTS);
	int surfaceCount = EFBSP_SurfaceCount(efbsp);
	int count = EFBSP_CountSurfacesOfType(efbsp, EF_MST_PATCH);
	int i, j, c;
	cPatch_t *patch;
	vec3_t points[MAX_PATCH_VERTS];
	int width, height;
	int shaderNum;
	zmemstats_t stats;

	XBLog_WriteCriticalf("STEFX_HW_BOOT: raw collision patches entry patches=%d numsurfs=%d",
		count, numsurfs);

	cmg.numSurfaces = numsurfs;
	cmg.surfaces = (cPatch_t **) Z_Malloc(cmg.numSurfaces * sizeof(cmg.surfaces[0]), TAG_BSP, qtrue);

	unsigned char* patchScratch = (unsigned char*)Z_Malloc(sizeof(*patch) * count, TAG_BSP, qtrue);
	Z_GetMemoryStats(&stats);
	XBLog_WriteCriticalf("STEFX_HW_BOOT: raw collision surface storage complete free=%d largest=%d",
		stats.freeBytes, stats.largestFreeBlock);

	extern void CM_GridAlloc();
	extern void CM_PatchCollideFromGridTempAlloc();
	extern void CM_PreparePatchCollide(int num);
	extern void CM_TempPatchPlanesAlloc();
	CM_GridAlloc();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F04;
#endif
	CM_PatchCollideFromGridTempAlloc();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F05;
#endif
	CM_PreparePatchCollide(count);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F06;
#endif
	CM_TempPatchPlanesAlloc();
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F07;
#endif
	XBLog_WriteCritical("STEFX_HW_BOOT: raw collision shared workspaces complete");

	facetLoad_t *facetbuf = (facetLoad_t*)Z_Malloc(
		MAX_PATCH_PLANES*sizeof(facetLoad_t), TAG_TEMP_WORKSPACE, qfalse);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F08;
#endif

	int *gridbuf = (int*)Z_Malloc(
		CM_MAX_GRID_SIZE*CM_MAX_GRID_SIZE*2*sizeof(int), TAG_TEMP_WORKSPACE, qfalse);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0F09;
#endif
	XBLog_WriteCritical("STEFX_HW_BOOT: raw collision local workspaces complete");

	for (i = 0; i < surfaceCount; ++i)
	{
		efbspSurface_t *in = &surfaces[i];
		dpatch_t patchSurface;

		if (in->surfaceType != EF_MST_PATCH)
		{
			continue;
		}

		EFBSP_FillPatchSurface(in, shaderCount, i, &patchSurface);
		cmg.surfaces[patchSurface.code] = patch = (cPatch_t *)patchScratch;
		patchScratch += sizeof(*patch);

		width = patchSurface.patchWidth;
		height = patchSurface.patchHeight;
		c = width * height;
		if (c > MAX_PATCH_VERTS)
		{
			Com_Error(ERR_DROP, "ParseMesh: MAX_PATCH_VERTS");
		}

		for (j = 0; j < c; ++j)
		{
			const efbspDrawVert_t *dv = &verts[in->firstVert + j];
			points[j][0] = dv->xyz[0];
			points[j][1] = dv->xyz[1];
			points[j][2] = dv->xyz[2];
		}

		shaderNum = patchSurface.shaderNum;
		patch->contents = cmg.shaders[shaderNum].contentFlags;
		CM_OrOfAllContentsFlagsInMap |= patch->contents;
		patch->surfaceFlags = cmg.shaders[shaderNum].surfaceFlags;
		patch->pc = CM_GeneratePatchCollide(width, height, points, facetbuf, gridbuf);
		if ((i & 1023) == 0)
		{
			XBLog_WriteCriticalf("STEFX_HW_BOOT: raw collision patch scan surface=%d/%d", i, surfaceCount);
		}
	}

	extern void CM_GridDealloc();
	extern void CM_PatchCollideFromGridTempDealloc();
	extern void CM_TempPatchPlanesDealloc();
	CM_PatchCollideFromGridTempDealloc();
	CM_GridDealloc();
	CM_TempPatchPlanesDealloc();

	Z_Free(gridbuf);
	Z_Free(facetbuf);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBPackedMapPhase = 0x504D0FFF;
#endif

	XBLog_WriteCriticalf("STEFX_HW_BOOT: raw collision patches done count=%d", count);
}

void CMod_LoadPatches( void *verts, int vertlen, void *surfaces, int surfacelen, int numsurfs ) {
	CMod_LoadPatchesInternal(verts, vertlen, NULL, surfaces, surfacelen, numsurfs);
}

void CMod_LoadPatchesStream( LumpStream *verts, void *surfaces, int surfacelen, int numsurfs ) {
	CMod_LoadPatchesInternal(NULL, verts ? verts->len : 0, verts, surfaces, surfacelen, numsurfs);
}
#endif

//==================================================================

#ifdef BSPC
/*
==================
CM_FreeMap

Free any loaded map and all submodels
==================
*/
void CM_FreeMap(void) {
	memset( &cmg, 0, sizeof( cmg ) );
	Hunk_ClearHigh();
	CM_ClearLevelPatches();
}
#endif //BSPC


/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
void *gpvCachedMapDiskImage = NULL;
char  gsCachedMapDiskImage[MAX_QPATH];
qboolean gbUsingCachedMapDataRightNow = qfalse;	// if true, signifies that you can't delete this at the moment!! (used during z_malloc()-fail recovery attempt)

// called in response to a "devmapbsp blah" or "devmapall blah" command, do NOT use inside CM_Load unless you pass in qtrue
//
// new bool return used to see if anything was freed, used during z_malloc failure re-try
//
qboolean CM_DeleteCachedMap(qboolean bGuaranteedOkToDelete)
{
	qboolean bActuallyFreedSomething = qfalse;

	if (bGuaranteedOkToDelete || !gbUsingCachedMapDataRightNow)
	{
		// dump cached disk image...
		//
		if (gpvCachedMapDiskImage)
		{
			Z_Free(	gpvCachedMapDiskImage );
					gpvCachedMapDiskImage = NULL;

			bActuallyFreedSomething = qtrue;
		}
		gsCachedMapDiskImage[0] = '\0';
		
		// force map loader to ignore cached internal BSP structures for next level CM_LoadMap() call...
		//
		cmg.name[0] = '\0';
#ifdef STEFX_ELITE_FORCE_SP
		CM_EFForgetRawMap( "CM_DeleteCachedMap" );
#endif
	}

	return bActuallyFreedSomething;
}

void CM_Free(void) 
{
#ifdef STEFX_ELITE_FORCE_SP
	CM_EFForgetRawMap( "CM_Free" );
	cmg.name[0] = '\0';
	cmg.cmodels = NULL;
	cmg.numSubModels = 0;
#endif
	CM_ClearLevelPatches();
	RE_SetWorldVisData(NULL);
	visData.Release();
	Z_TagFree(TAG_BSP);
}

void R_LoadSurfaces( int count );
void R_LoadPatches( void *verts, int vertlen, 
					void *surfaces, int surfacelen );
void R_LoadTriSurfs( void *indexdata, int indexlen, 
					void *verts, int vertlen, 
					void *surfaces, int surfacelen );
void R_LoadFaces( void *indexdata, int indexlen, 
					void *verts, int vertlen, 
					void *surfaces, int surfacelen );
void R_LoadPatchesStream( LumpStream *verts, void *surfaces, int surfacelen );
void R_LoadTriSurfsStream( LumpStream *indexes, short *indexScratch,
					LumpStream *verts, void *surfaces, int surfacelen,
                    void *residentIndexes, int residentIndexLen );
void R_LoadFacesStream( LumpStream *indexes, short *indexScratch,
					LumpStream *verts, LumpStream *surfaces,
                    void *residentIndexes, int residentIndexLen );
void R_LoadFlares( void *surfaces, int surfacelen );
extern void R_LoadShaders( void );
extern void R_LoadLightmaps( void *data, int len, const char *psMapName );
#ifdef STEFX_ELITE_FORCE_SP
extern void R_LoadRawLightmaps( void *data, int len, const char *psMapName );
extern qboolean R_LoadXboxOptimizedLightmaps( const char *psMapName );
extern void R_EFBeginRawWorldMapLoad( const char *name );
extern qboolean R_EFLoadRawWorldDataFromBSP( const char *name, const efbspFile_t *efbsp );
#if defined(STEFX_ELITE_FORCE_SP)
extern void R_EFPrecacheRawSurfaceShadersFromBSP( const char *name, const efbspFile_t *efbsp );
extern void R_EFLoadRawDrawSurfacesFromBSP( const char *name, const efbspFile_t *efbsp, int shaderCount, int numSurfs );
#endif
#endif
extern byte *fileBase;
extern void UpdateLoadingAnimation();
#if defined(STEFX_ELITE_FORCE_SP)
// Retained phase durations: read, setup/checksum, lightmaps, shaders,
// render surfaces, collision patches, remaining world data, cleanup, complete.
__declspec(dllexport) unsigned int g_SPXBMapLoadPhases[9] = {0};
// Packed path: setup, lightmaps, patches, triangles, faces, flares,
// collision lumps, finish, complete. Raw and packed arrays are separate.
__declspec(dllexport) unsigned int g_SPXBPackedLoadPhases[9] = {0};
static unsigned int s_stefxMapLoadTick;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(_XBOX)
// Six phase snapshots, ten DWORDs each. No additional memory reserved in retail.
extern "C" {
__declspec(dllexport) unsigned int g_SPXBIndexStreamProof[64] = {0};
}
static void CM_EFRecordIndexMemory(int phase)
{
    zmemstats_t stats;
    MEMORYSTATUS physical;
    Z_GetMemoryStats(&stats);
    GlobalMemoryStatus(&physical);
    unsigned int *out = g_SPXBIndexStreamProof + 4 + phase * 10;
    out[0] = physical.dwAvailPhys;
    out[1] = stats.zoneSize;
    out[2] = stats.usedBytes;
    out[3] = stats.freeBytes;
    out[4] = stats.largestFreeBlock;
    out[5] = stats.peakBytes;
    out[6] = stats.overheadBytes;
    out[7] = stats.filesysBytes;
    out[8] = stats.bspBytes;
    out[9] = stats.freeBlocks;
    g_SPXBIndexStreamProof[3] |= 1u << phase;
    XBLog_WriteCriticalf("STEFX_INDEX_MEMORY: phase=%d stream=%u physFree=%u zoneUsed=%d zoneFree=%d largest=%d peak=%d filesys=%d",
        phase, g_SPXBIndexStreamProof[1], out[0], stats.usedBytes, stats.freeBytes,
        stats.largestFreeBlock, stats.peakBytes, stats.filesysBytes);
}
#else
#define CM_EFRecordIndexMemory(phase) ((void)0)
#endif
static void CM_EFRecordLoadPhase( int phase )
{
	unsigned int now = Sys_Milliseconds();
	g_SPXBMapLoadPhases[phase] = now - s_stefxMapLoadTick;
	s_stefxMapLoadTick = now;
}
static void CM_EFRecordPackedPhase( int phase )
{
	unsigned int now = Sys_Milliseconds();
	g_SPXBPackedLoadPhases[phase] = now - s_stefxMapLoadTick;
	s_stefxMapLoadTick = now;
}
#endif
static void CM_LoadMap_Actual( const char *name, qboolean clientload, int *checksum ) {
	const int		*buf = NULL;
	const int		*surfBuf = NULL;
	static unsigned	last_checksum;
	char			lmName[MAX_QPATH];
	char			loadName[MAX_QPATH];
	char			stripName[MAX_QPATH];
	Lump			outputLump;
	LumpStream		verts;

#ifdef STEFX_ELITE_FORCE_SP
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap entered");
#endif
#endif
	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "CM_LoadMap: NULL name" );
	}
	Q_strncpyz( loadName, name, sizeof( loadName ) );
	name = loadName;

#ifndef BSPC
	cm_noAreas = Cvar_Get ("cm_noAreas", "0", CVAR_CHEAT);
	cm_noCurves = Cvar_Get ("cm_noCurves", "0", CVAR_CHEAT);
	cm_playerCurveClip = Cvar_Get ("cm_playerCurveClip", "1", CVAR_ARCHIVE|CVAR_CHEAT );
#endif
#ifdef STEFX_ELITE_FORCE_SP
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap collision cvars ready");
#endif
#endif
	Com_DPrintf( "CM_LoadMap( %s, %i )\n", name, clientload );

#ifdef STEFX_ELITE_FORCE_SP
	XBLF("EF: CM_LoadMap enter name='%s' clientload=%d cmg='%s' raw='%s' ready=%d submodels=%d cmodels=%08x",
		name,
		clientload,
		cmg.name[0] ? cmg.name : "(none)",
		s_efRawLoadedMapName[0] ? s_efRawLoadedMapName : "(none)",
		s_efRawLoadedMapReady,
		cmg.numSubModels,
		(unsigned int)cmg.cmodels);
	if ( clientload ) {
		XBLF("STEFX: CM_LoadMap client request name='%s' cmg='%s' raw='%s' ready=%d submodels=%d cmodels=%08x checksum=0x%08x",
			name,
			cmg.name[0] ? cmg.name : "(none)",
			s_efRawLoadedMapName[0] ? s_efRawLoadedMapName : "(none)",
			s_efRawLoadedMapReady,
			cmg.numSubModels,
			(unsigned int)cmg.cmodels,
			s_efRawLoadedMapChecksum);
		if ( ( !Q_stricmp( cmg.name, name ) && cmg.cmodels && cmg.numSubModels > 0 ) ||
			 CM_EFCanReuseRawMap( name ) ) {
			unsigned reuseChecksum = s_efRawLoadedMapReady ? s_efRawLoadedMapChecksum : last_checksum;
			if ( !cmg.name[0] ) {
				Q_strncpyz( cmg.name, name, sizeof( cmg.name ) );
			}
			*checksum = reuseChecksum;
			XBLF("STEFX: CM_LoadMap reusing loaded raw BSP '%s' checksum=0x%08x", name, reuseChecksum);
			return;
		}
	}
#else
	if ( !strcmp( cmg.name, name ) && clientload ) {
		*checksum = last_checksum;
		return;
	}
#endif

	// free old stuff
#ifdef STEFX_ELITE_FORCE_SP
	CM_EFForgetRawMap( clientload ? "CM_LoadMap client reload" : "CM_LoadMap server reload" );
#endif
	extern qboolean vidRestartReloadMap;
	int* ap;
	if (vidRestartReloadMap) ap = cmg.areaPortals;
	memset( &cmg, 0, sizeof( cmg ) );
	if (vidRestartReloadMap) cmg.areaPortals = ap;
#ifdef STEFX_ELITE_FORCE_SP
#ifdef _XBOX
	XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap prior raw-map state cleared");
#endif
#endif
	
	if ( !name[0] ) {
		cmg.numLeafs = 1; 
		cmg.numClusters = 1;
		cmg.numAreas = 1;
		cmg.cmodels = (struct cmodel_s *) Z_Malloc( sizeof( *cmg.cmodels ), TAG_BSP, qtrue );
		*checksum = 0;
		return;
	}
	
	last_checksum = crc32(0, (const Bytef *)name, strlen(name));
	COM_StripExtension(name, stripName);

#ifdef STEFX_ELITE_FORCE_SP
#endif
	UpdateLoadingAnimation();
#ifdef STEFX_ELITE_FORCE_SP
#endif

#ifdef STEFX_ELITE_FORCE_SP
	{
		efbspFile_t efbsp;
		qboolean packedLumpsExist;
		memset(g_SPXBMapLoadPhases, 0, sizeof(g_SPXBMapLoadPhases));
		memset(g_SPXBPackedLoadPhases, 0, sizeof(g_SPXBPackedLoadPhases));
		s_stefxMapLoadTick = Sys_Milliseconds();
		memset(&efbsp, 0, sizeof(efbsp));
		XBLF("EF: CM_LoadMap raw probe begin name='%s' clientload=%d", name, clientload);
#ifdef _XBOX
		XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap beginning raw BSP read");
#endif
		packedLumpsExist = EFBSP_XboxPackedLumpsExist(name);
#ifdef _XBOX
		XBLog_WriteCritical(packedLumpsExist ?
			"STEFX_RETAIL_LUMPS: CM packed probe returned true" :
			"STEFX_RETAIL_LUMPS: CM packed probe returned false");
#endif
		if (!packedLumpsExist && EFBSP_LoadFile(name, &efbsp))
		{
			int shaderCount;
			int num_surfs;
			void *shaders;
			void *verts;
			void *indexes;
			void *patches;
			void *trisurfs;
			void *faces;
			void *flares;
			void *leafs;
			void *leafbrushes;
			void *brushsides;
			void *brushes;
			void *models;
			void *nodes;
			void *visibility;
			int shadersLen, vertsLen, indexesLen, patchesLen, trisurfsLen, facesLen, flaresLen;
			int leafsLen, leafbrushesLen, brushsidesLen, brushesLen, modelsLen, nodesLen, visibilityLen;
			qboolean rendererLightmapsLoaded;
			int rendererLightmapMode;
			zmemstats_t rawBspStats;
			CM_EFRecordLoadPhase(0);

			XBLF("EF: CM_LoadMap raw probe loaded name='%s' bytes=%d clientload=%d", name, efbsp.len, clientload);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw BSP read complete");
#endif
			EFBSP_Validate(&efbsp, name);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw BSP validation complete");
#endif
			R_EFBeginRawWorldMapLoad(name);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw world setup begun");
#endif
			last_checksum = LittleLong(Com_BlockChecksum(efbsp.data, efbsp.len));
			shaderCount = EFBSP_ShaderCount(&efbsp);
			num_surfs = EFBSP_SurfaceCount(&efbsp);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw BSP counts ready");
#endif
			rendererLightmapsLoaded = qfalse;
			CM_EFRecordLoadPhase(1);
			rendererLightmapMode = 0;
			if (R_LoadXboxOptimizedLightmaps(name))
			{
				rendererLightmapsLoaded = qtrue;
				rendererLightmapMode = 2;
				XBLog_Write("STEFX: CM_LoadMap optimized lightmaps loaded");
			}
			else if (EFBSP_LumpLen(&efbsp, EF_LUMP_LIGHTMAPS) > 0)
			{
				rendererLightmapsLoaded = qtrue;
				rendererLightmapMode = 1;
				R_LoadRawLightmaps(EFBSP_LumpData(&efbsp, EF_LUMP_LIGHTMAPS), EFBSP_LumpLen(&efbsp, EF_LUMP_LIGHTMAPS), name);
				XBLog_Write("EF: CM_LoadMap raw lightmaps loaded");
			}
			else
			{
				Com_Error(ERR_DROP, "CM_LoadMap: %s has no raw lightmaps and no optimized lightmap sidecar", name);
			}
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap lightmaps complete");
			CM_EFRecordLoadPhase(2);
#endif
			UpdateLoadingAnimation();
			Z_GetMemoryStats(&rawBspStats);
			XBLF("EF: CM_LoadMap raw BSP '%s' bytes=%d checksum=0x%08x shaders=%d surfaces=%d verts=%d indexes=%d lightmaps=%d rendererLightmapsLoaded=%d lightmapMode=%d memUsed=%d memPeak=%d memFree=%d memLargest=%d memBsp=%d memFilesys=%d memSound=%d",
				name,
				efbsp.len,
				last_checksum,
				shaderCount,
				num_surfs,
				EFBSP_CheckedCount("drawverts", EFBSP_LumpLen(&efbsp, EF_LUMP_DRAWVERTS), sizeof(efbspDrawVert_t)),
				EFBSP_CheckedCount("drawindexes", EFBSP_LumpLen(&efbsp, EF_LUMP_DRAWINDEXES), sizeof(int)),
				EFBSP_LumpLen(&efbsp, EF_LUMP_LIGHTMAPS) / (128 * 128 * 3),
				rendererLightmapsLoaded,
				rendererLightmapMode,
				rawBspStats.usedBytes,
				rawBspStats.peakBytes,
				rawBspStats.freeBytes,
				rawBspStats.largestFreeBlock,
				rawBspStats.bspBytes,
				rawBspStats.filesysBytes,
				rawBspStats.soundRawBytes);

			shaders = EFBSP_ConvertShaders(&efbsp, &shadersLen);
			CMod_LoadShaders(shaders, shadersLen);
			EFBSP_FreeTemp(shaders);
			if (!clientload)
			{
				R_LoadShaders();
#if defined(STEFX_ELITE_FORCE_SP)
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw shader precache begin");
				R_EFPrecacheRawSurfaceShadersFromBSP(name, &efbsp);
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw shader precache complete");
#endif
			}
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap shaders complete");
			CM_EFRecordLoadPhase(3);
#endif
			XBLF("EF: CM_LoadMap raw shaders loaded clientload=%d", clientload);
			UpdateLoadingAnimation();

			if (!clientload)
			{
				XBLF("STEFX: CM_LoadMap renderer lightmaps already loaded=%d clientload=%d", rendererLightmapsLoaded, clientload);
			}
			else
			{
				XBLog_Write("STEFX: CM_LoadMap client collision-only load skips renderer lightmaps");
			}

			fileBase = NULL;
#if defined(STEFX_ELITE_FORCE_SP)
			if (!clientload)
			{
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap entering streamed raw render surfaces");
				R_EFLoadRawDrawSurfacesFromBSP(name, &efbsp, shaderCount, num_surfs);
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap streamed raw render surfaces complete");
			}
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap entering streamed raw collision patches");
			CM_EFRecordLoadPhase(4);
			CMod_LoadRawEFPatches(&efbsp, shaderCount, num_surfs);
			CM_EFRecordLoadPhase(5);
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap streamed raw collision patches complete");
#else
			if (!clientload)
			{
				R_LoadSurfaces(num_surfs);
			}
			verts = EFBSP_ConvertVerts(&efbsp, &vertsLen);
			patches = EFBSP_ConvertPatches(&efbsp, shaderCount, &patchesLen);
			XBLF("EF: CM_LoadMap raw surfaces alloc vertsLen=%d patchesLen=%d", vertsLen, patchesLen);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			XBLog_Write("EF: CM_LoadMap raw CMod_LoadPatches begin");
#endif
			CMod_LoadPatches(verts, vertsLen, patches, patchesLen, num_surfs);
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			XBLog_Write("EF: CM_LoadMap raw CMod_LoadPatches done; R_LoadPatches begin");
#endif
			if (!clientload)
			{
				R_LoadPatches(verts, vertsLen, patches, patchesLen);
			}
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			XBLog_Write(clientload ? "EF: CM_LoadMap raw client skipped R_LoadPatches" : "EF: CM_LoadMap raw R_LoadPatches done");
#endif
			EFBSP_FreeTemp(patches);
			UpdateLoadingAnimation();

			if (!clientload)
			{
				indexes = EFBSP_ConvertIndexes(&efbsp, &indexesLen);
				trisurfs = EFBSP_ConvertTriSurfs(&efbsp, shaderCount, &trisurfsLen);
				XBLF("EF: CM_LoadMap raw trisurfs indexesLen=%d trisurfsLen=%d", indexesLen, trisurfsLen);
				R_LoadTriSurfs(indexes, indexesLen, verts, vertsLen, trisurfs, trisurfsLen);
				EFBSP_FreeTemp(trisurfs);

				faces = EFBSP_ConvertFaces(&efbsp, shaderCount, &facesLen);
				XBLF("EF: CM_LoadMap raw facesLen=%d", facesLen);
				R_LoadFaces(indexes, indexesLen, verts, vertsLen, faces, facesLen);
				EFBSP_FreeTemp(faces);
				UpdateLoadingAnimation();

				flares = EFBSP_ConvertFlares(&efbsp, shaderCount, &flaresLen);
				R_LoadFlares(flares, flaresLen);
				EFBSP_FreeTemp(flares);
				EFBSP_FreeTemp(indexes);
				XBLF("EF: CM_LoadMap raw render surfaces loaded flaresLen=%d", flaresLen);
			}
			else
			{
				XBLF("STEFX: CM_LoadMap client collision-only surfaces loaded vertsLen=%d patchesLen=%d",
					vertsLen, patchesLen);
			}
			EFBSP_FreeTemp(verts);
#endif
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap render surfaces complete");
#endif
			UpdateLoadingAnimation();

			leafs = EFBSP_ConvertLeafs(&efbsp, &leafsLen);
			CMod_LoadLeafs(leafs, leafsLen);
			EFBSP_FreeTemp(leafs);

			leafbrushes = EFBSP_CopyLump(&efbsp, EF_LUMP_LEAFBRUSHES, &leafbrushesLen);
			CMod_LoadLeafBrushes(leafbrushes, leafbrushesLen);
			EFBSP_FreeTemp(leafbrushes);

			cmg.leafsurfaces = NULL;
			CMod_LoadPlanes(EFBSP_LumpData(&efbsp, EF_LUMP_PLANES), EFBSP_LumpLen(&efbsp, EF_LUMP_PLANES));

			brushsides = EFBSP_ConvertBrushSides(&efbsp, shaderCount, &brushsidesLen);
			CMod_LoadBrushSides(brushsides, brushsidesLen);
			EFBSP_FreeTemp(brushsides);

			brushes = EFBSP_ConvertBrushes(&efbsp, shaderCount, &brushesLen);
			CMod_LoadBrushes(brushes, brushesLen);
			EFBSP_FreeTemp(brushes);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap primary collision complete");
#endif
			XBLF("EF: CM_LoadMap raw collision loaded leafs=%d brushesLen=%d brushsidesLen=%d",
				leafsLen, brushesLen, brushsidesLen);
			UpdateLoadingAnimation();

			models = EFBSP_ConvertModels(&efbsp, &modelsLen);
			CMod_LoadSubmodels(models, modelsLen);
			EFBSP_FreeTemp(models);

			nodes = EFBSP_ConvertNodes(&efbsp, &nodesLen);
			CMod_LoadNodes(nodes, nodesLen);
			EFBSP_FreeTemp(nodes);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap models and nodes complete");
#endif
			UpdateLoadingAnimation();

			CMod_LoadEntityString(EFBSP_LumpData(&efbsp, EF_LUMP_ENTITIES), EFBSP_LumpLen(&efbsp, EF_LUMP_ENTITIES));
			visibility = EFBSP_ConvertVisibility(&efbsp, &visibilityLen);
			CMod_LoadVisibility(visibility, visibilityLen);
			EFBSP_FreeTemp(visibility);
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap entities and visibility complete");
#endif
			XBLF("EF: CM_LoadMap raw entities/visibility loaded entityLen=%d visibilityLen=%d",
				EFBSP_LumpLen(&efbsp, EF_LUMP_ENTITIES), visibilityLen);
			UpdateLoadingAnimation();

			if (!clientload)
			{
#ifdef _XBOX
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap entering renderer world finalization");
#endif
				if (!R_EFLoadRawWorldDataFromBSP(name, &efbsp))
				{
					Com_Error(ERR_DROP, "CM_LoadMap: failed to finish EF renderer world for %s", name);
				}
#ifdef _XBOX
				XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap renderer world finalization complete");
#endif
				UpdateLoadingAnimation();
			}

			TotalSubModels += cmg.numSubModels;
			CM_InitBoxHull();
			CM_EFRecordLoadPhase(6);
			*checksum = last_checksum;
			CM_FloodAreaConnections();
			Q_strncpyz(cmg.name, name, sizeof(cmg.name));
#ifdef STEFX_ELITE_FORCE_SP
			CM_EFRememberRawMap(name, last_checksum);
#endif
			CM_CleanLeafCache();
#ifdef _XBOX
			XBLog_WriteCritical("STEFX_HW_BOOT: CM_LoadMap raw BSP complete");
#endif
			XBLF("EF: CM_LoadMap raw BSP complete clientload=%d name='%s' submodels=%d clusters=%d areas=%d checksum=%u",
				clientload, cmg.name, cmg.numSubModels, cmg.numClusters, cmg.numAreas, last_checksum);
			CM_EFLogMemoryStats(clientload ? "complete clientload" : "complete serverload", name);
			EFBSP_FreeFile(&efbsp);
			CM_EFRecordLoadPhase(7);
			g_SPXBMapLoadPhases[8] = 1;
			XBLog_WriteCriticalf("STEFX_MAP_LOAD_PHASES: read=%u setup=%u lightmaps=%u shaders=%u surfaces=%u patches=%u world=%u cleanup=%u",
				g_SPXBMapLoadPhases[0], g_SPXBMapLoadPhases[1], g_SPXBMapLoadPhases[2], g_SPXBMapLoadPhases[3],
				g_SPXBMapLoadPhases[4], g_SPXBMapLoadPhases[5], g_SPXBMapLoadPhases[6], g_SPXBMapLoadPhases[7]);
			return;
		}

		if (packedLumpsExist)
		{
			XBLF("STEFX_RETAIL_LUMPS: CM selecting packed map='%s' clientload=%d", name, clientload);
		}
		else
		{
			XBLF("EF: CM_LoadMap raw probe missing name='%s' clientload=%d; emergency sidecar fallback only", name, clientload);
		}
	}
#endif

	// load into heap
#ifdef STEFX_ELITE_FORCE_SP
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before renderer world reset");
#endif
	R_EFBeginRawWorldMapLoad(name);
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after renderer world reset");
#endif
	{
		Lump packedChecksum;
#ifdef _XBOX
		XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before checksum lump read");
#endif
		packedChecksum.load(stripName, "checksum");
#ifdef _XBOX
		XBLog_WriteRingMarkerf("STEFX_RETAIL_LUMPS: CM after checksum lump read len=%d data=%p",
			packedChecksum.len, packedChecksum.data);
#endif
		if (packedChecksum.len == (int)sizeof(unsigned int))
		{
			last_checksum = *(unsigned int *)packedChecksum.data;
		}
#ifdef _XBOX
		XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before checksum lump free");
#endif
		packedChecksum.clear();
#ifdef _XBOX
		XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after checksum lump free");
#endif
	}
#ifdef _XBOX
	XBLog_WriteRingMarkerf("STEFX_RETAIL_LUMPS: CM begin map='%s' checksum=0x%08x",
		name, last_checksum);
#endif
#endif
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before shaders lump read");
#endif
	outputLump.load(stripName, "shaders");
#ifdef _XBOX
	XBLog_WriteRingMarkerf("STEFX_RETAIL_LUMPS: CM after shaders lump read len=%d data=%p",
		outputLump.len, outputLump.data);
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before collision shaders load");
#endif
	CMod_LoadShaders( outputLump.data, outputLump.len );
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after collision shaders load");
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before renderer shaders load");
#endif
	R_LoadShaders();
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after renderer shaders load");
#endif
	outputLump.clear();

	/* Keep the packed vertex lump open and read one surface at a time. Same-process
	   map transitions cannot reliably reserve this multi-megabyte file contiguously. */
	g_SPXBPackedMapPhase = 0x504D000A;
	if (!verts.open(stripName, "verts"))
	{
		Com_Error(ERR_DROP, "CM_LoadMap: failed to open packed verts for %s", name);
	}
	g_SPXBPackedMapPhase = 0x504D000B;
	if (verts.len <= 0 || verts.len % (int)sizeof(mapVert_t))
	{
		Com_Error(ERR_DROP, "CM_LoadMap: invalid packed verts for %s", name);
	}

#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before post-shader loading animation");
#endif
	UpdateLoadingAnimation();
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after post-shader loading animation");
#endif

	strcpy(lmName, name);
#ifdef STEFX_ELITE_FORCE_SP
	CM_EFRecordPackedPhase(0);
#ifdef _XBOX
	XBLog_WriteRingMarkerf("STEFX_RETAIL_LUMPS: CM before optimized lightmaps map='%s'", lmName);
#endif
	if (!R_LoadXboxOptimizedLightmaps(lmName))
	{
		Com_Error(ERR_DROP, "CM_LoadMap: packed map %s has no optimized lightmaps", lmName);
	}
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after optimized lightmaps");
	CM_EFRecordPackedPhase(1);
#endif
#else
	outputLump.load(stripName, "lightmaps");
	R_LoadLightmaps( outputLump.data, outputLump.len, lmName);
#endif

#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM before post-lightmap loading animation");
#endif
	UpdateLoadingAnimation();
#ifdef _XBOX
	XBLog_WriteRingMarker("STEFX_RETAIL_LUMPS: CM after post-lightmap loading animation");
#endif

	{
		g_SPXBPackedMapPhase = 0x504D0001;
		fileBase = NULL;
		outputLump.clear();
		g_SPXBPackedMapPhase = 0x504D0002;

		Lump misc;
		g_SPXBPackedMapPhase = 0x504D0003;
		misc.load(stripName, "misc");
		g_SPXBPackedMapPhase = 0x504D0004;
		if (misc.len != (int)sizeof(int))
		{
			Com_Error(ERR_DROP, "CM_LoadMap: packed map %s has invalid misc lump", name);
		}
		int num_surfs = *(int*)misc.data;
		g_SPXBPackedMapPhase = 0x504D0005;
		misc.clear();
		g_SPXBPackedMapPhase = 0x504D0006;
		
		g_SPXBPackedMapPhase = 0x504D0007;
		R_LoadSurfaces(num_surfs);
		g_SPXBPackedMapPhase = 0x504D0008;

		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D0009;

		Lump patches;
		g_SPXBPackedMapPhase = 0x504D000C;
		patches.load(stripName, "patches");
		g_SPXBPackedMapPhase = 0x504D000D;

		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D000E;

		g_SPXBPackedMapPhase = 0x504D000F;
		CMod_LoadPatchesStream(&verts,
			patches.data, patches.len,
			num_surfs );
		g_SPXBPackedMapPhase = 0x504D0010;
		R_LoadPatchesStream(&verts, patches.data, patches.len);
		g_SPXBPackedMapPhase = 0x504D0011;

		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D0012;

		patches.clear();
		g_SPXBPackedMapPhase = 0x504D0013;
		CM_EFRecordPackedPhase(2);

		LumpStream indexes;
        Lump residentIndexes;
        qboolean streamIndexes = qtrue;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(_XBOX)
        // Diagnostic-only A/B: same binary, same authored map and parser.
        streamIndexes = Cvar_Get("stefx_index_stream", "1", 0)->integer ? qtrue : qfalse;
        memset(g_SPXBIndexStreamProof, 0, sizeof(g_SPXBIndexStreamProof));
        g_SPXBIndexStreamProof[0] = 0x49534D31;
        g_SPXBIndexStreamProof[1] = streamIndexes;
#endif
        CM_EFRecordIndexMemory(0);
        // Packed surface counts use 12 bits: 8 KiB covers every index range.
        g_SPXBPackedMapPhase = 0x504D0014;
        if (streamIndexes)
        {
            if (!indexes.open(stripName, "indexes") || (indexes.len & 1))
                Com_Error(ERR_DROP, "CM_LoadMap: invalid packed index stream for %s", name);
        }
        else
            residentIndexes.load(stripName, "indexes");
        const int indexFileBytes = streamIndexes ? indexes.len : residentIndexes.len;
        short *indexScratch = streamIndexes ?
            (short *)Z_Malloc(8192, TAG_TEMP_WORKSPACE, qfalse, 32) : NULL;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(_XBOX)
        g_SPXBIndexStreamProof[2] = indexFileBytes;
#endif
        CM_EFRecordIndexMemory(1);
        XBLog_WriteCriticalf("STEFX_INDEX_STREAM: map='%s' enabled=%d fileBytes=%d scratchBytes=%d begin",
            name, streamIndexes, indexFileBytes, streamIndexes ? 8192 : 0);
		g_SPXBPackedMapPhase = 0x504D0015;

		Lump trisurfs;
		g_SPXBPackedMapPhase = 0x504D0016;
		trisurfs.load(stripName, "trisurfs");
		g_SPXBPackedMapPhase = 0x504D0017;

		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D0018;

		R_LoadTriSurfsStream(streamIndexes ? &indexes : NULL, indexScratch,
			&verts, trisurfs.data, trisurfs.len, residentIndexes.data, residentIndexes.len);
		g_SPXBPackedMapPhase = 0x504D0019;

		trisurfs.clear();
        CM_EFRecordIndexMemory(2);
		g_SPXBPackedMapPhase = 0x504D001A;
		CM_EFRecordPackedPhase(3);
	
		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D001B;

		LumpStream faces;
		g_SPXBPackedMapPhase = 0x504D001C;
		if (!faces.open(stripName, "faces"))
		{
			Com_Error(ERR_DROP, "CM_LoadMap: failed to open packed faces for %s", name);
		}
		g_SPXBPackedMapPhase = 0x504D001D;

		R_LoadFacesStream(streamIndexes ? &indexes : NULL, indexScratch,
			&verts, &faces, residentIndexes.data, residentIndexes.len);
		g_SPXBPackedMapPhase = 0x504D001E;
		faces.close();
        CM_EFRecordIndexMemory(3);
        XBLog_WriteCriticalf("STEFX_INDEX_STREAM: map='%s' enabled=%d fileBytes=%d done",
            name, streamIndexes, indexFileBytes);
        indexes.close();
        if (indexScratch) Z_Free(indexScratch);
        residentIndexes.clear();
        CM_EFRecordIndexMemory(4);
        CM_EFRecordPackedPhase(4);

		UpdateLoadingAnimation();
		g_SPXBPackedMapPhase = 0x504D001F;

		Lump flares;
		g_SPXBPackedMapPhase = 0x504D0020;
		flares.load(stripName, "flares");
		g_SPXBPackedMapPhase = 0x504D0021;

		R_LoadFlares(flares.data, flares.len);
		g_SPXBPackedMapPhase = 0x504D0022;
		verts.close();
        CM_EFRecordIndexMemory(5);
		CM_EFRecordPackedPhase(5);
	}
	
	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D0023;

	g_SPXBPackedMapPhase = 0x504D0024;
	outputLump.load(stripName, "leafs");
	g_SPXBPackedMapPhase = 0x504D0025;
	CMod_LoadLeafs (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0026;

	g_SPXBPackedMapPhase = 0x504D0027;
	outputLump.load(stripName, "leafbrushes");
	g_SPXBPackedMapPhase = 0x504D0028;
	CMod_LoadLeafBrushes (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0029;
	
	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D002A;

	cmg.leafsurfaces = NULL;
	g_SPXBPackedMapPhase = 0x504D002B;
	outputLump.load(stripName, "planes");
	g_SPXBPackedMapPhase = 0x504D002C;
	CMod_LoadPlanes (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D002D;
	
	g_SPXBPackedMapPhase = 0x504D002E;
	outputLump.load(stripName, "brushsides");
	g_SPXBPackedMapPhase = 0x504D002F;
	CMod_LoadBrushSides (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0030;
	g_SPXBPackedMapPhase = 0x504D0031;
	outputLump.load(stripName, "brushes");
	g_SPXBPackedMapPhase = 0x504D0032;
	CMod_LoadBrushes (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0033;

	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D0034;

	g_SPXBPackedMapPhase = 0x504D0035;
	outputLump.load(stripName, "models");
	g_SPXBPackedMapPhase = 0x504D0036;
	CMod_LoadSubmodels (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0037;

	g_SPXBPackedMapPhase = 0x504D0038;
	outputLump.load(stripName, "nodes");
	g_SPXBPackedMapPhase = 0x504D0039;
	CMod_LoadNodes (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D003A;

	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D003B;

	g_SPXBPackedMapPhase = 0x504D003C;
	outputLump.load(stripName, "entities");
	g_SPXBPackedMapPhase = 0x504D003D;
	CMod_LoadEntityString (outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D003E;

	g_SPXBPackedMapPhase = 0x504D003F;
	outputLump.load(stripName, "visibility");
	g_SPXBPackedMapPhase = 0x504D0040;
	CMod_LoadVisibility( outputLump.data, outputLump.len);
	g_SPXBPackedMapPhase = 0x504D0041;

	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D0042;

	TotalSubModels += cmg.numSubModels;
	CM_EFRecordPackedPhase(6);
	
	CM_InitBoxHull ();
	g_SPXBPackedMapPhase = 0x504D0043;

	*checksum = last_checksum;	

	// do this whether or not the map was cached from last load...
	//
	CM_FloodAreaConnections ();
	g_SPXBPackedMapPhase = 0x504D0044;

	UpdateLoadingAnimation();
	g_SPXBPackedMapPhase = 0x504D0045;

	Q_strncpyz( cmg.name, name, sizeof( cmg.name ) );
#ifdef STEFX_ELITE_FORCE_SP
	CM_EFRememberRawMap(name, last_checksum);
	XBLF("STEFX_RETAIL_LUMPS: CM complete map='%s' submodels=%d clusters=%d areas=%d checksum=0x%08x",
		name, cmg.numSubModels, cmg.numClusters, cmg.numAreas, last_checksum);
	CM_EFLogMemoryStats(clientload ? "packed complete clientload" : "packed complete serverload", name);
#endif
	CM_CleanLeafCache();
	g_SPXBPackedMapPhase = 0x504D0046;
	CM_EFRecordPackedPhase(7);
	g_SPXBPackedLoadPhases[8] = 1;
	XBLog_WriteCriticalf("STEFX_PACKED_LOAD_PHASES: setup=%u lightmaps=%u patches=%u triangles=%u faces=%u flares=%u collision=%u finish=%u",
		g_SPXBPackedLoadPhases[0], g_SPXBPackedLoadPhases[1], g_SPXBPackedLoadPhases[2], g_SPXBPackedLoadPhases[3],
		g_SPXBPackedLoadPhases[4], g_SPXBPackedLoadPhases[5], g_SPXBPackedLoadPhases[6], g_SPXBPackedLoadPhases[7]);
}

// need a wrapper function around this because of multiple returns, need to ensure bool is correct...
//
void CM_LoadMap( const char *name, qboolean clientload, int *checksum )
{
	CM_LoadMap_Actual( name, clientload, checksum );
}

qboolean CM_SameMap(char *server)
{
	if (!cmg.name[0] || !server || !server[0])
	{
		return qfalse;
	}

	if (Q_stricmp(cmg.name, va("maps/%s.bsp", server)))
	{
		return qfalse;
	}

	return qtrue;
}

#ifndef _XBOX
qboolean CM_HasTerrain(void)
{
	if (cmg.landScape)
		return qtrue;
	return qfalse;
}
#endif

/*
==================
CM_ClearMap
==================
*/
void CM_ClearMap( void ) 
{
	int		i;

#ifdef STEFX_ELITE_FORCE_SP
	CM_EFForgetRawMap( "CM_ClearMap" );
#endif

	CM_OrOfAllContentsFlagsInMap = CONTENTS_BODY;

#if !defined(BSPC)
//	CM_ShutdownShaderProperties();
//	MAT_Shutdown();
#endif

#ifndef _XBOX
	if (TheRandomMissionManager)
	{
		delete TheRandomMissionManager;
		TheRandomMissionManager = 0;
	}

	if (cmg.landScape)
	{
		delete cmg.landScape;
		cmg.landScape = 0;
	}
#endif

	memset( &cmg, 0, sizeof( cmg ) );
	CM_ClearLevelPatches();

	for(i = 0; i < NumSubBSP; i++)
	{
		memset(&SubBSP[i], 0, sizeof(SubBSP[0]));
	}
	NumSubBSP = 0;
	TotalSubModels = 0;
}

int CM_TotalMapContents()
{
	return CM_OrOfAllContentsFlagsInMap;
}

/*
==================
CM_ClipHandleToModel
==================
*/
cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle, clipMap_t **clipMap )
{
	int		i;
	int		count;

	if ( handle < 0 ) 
	{
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle );
	}
	if ( handle < cmg.numSubModels ) 
	{
		if (clipMap)
		{
			*clipMap = &cmg;
		}
		return &cmg.cmodels[handle];
	}
	if ( handle == BOX_MODEL_HANDLE ) 
	{
		if (clipMap)
		{
			*clipMap = &cmg;
		}
		return &box_model;
	}

	count = cmg.numSubModels;
	for(i = 0; i < NumSubBSP; i++)
	{
		if (handle < count + SubBSP[i].numSubModels)
		{
			if (clipMap)
			{
				*clipMap = &SubBSP[i];
			}
			return &SubBSP[i].cmodels[handle - count];
		}
		count += SubBSP[i].numSubModels;
	}

	if ( handle < MAX_SUBMODELS ) 
	{
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i < %i < %i", 
			cmg.numSubModels, handle, MAX_SUBMODELS );
	}
	Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle + MAX_SUBMODELS );

	return NULL;
}
/*
==================
CM_InlineModel
==================
*/
clipHandle_t	CM_InlineModel( int index ) {
	if ( index < 0 || index >= TotalSubModels ) {
#if defined(STEFX_ELITE_FORCE_SP)
		static int s_badInlineModelLogCount = 0;
		if (s_badInlineModelLogCount < 32)
		{
			XBLF("STEFX: CM_InlineModel rejected bad index=%d total=%d main=%d; returning world",
				index, TotalSubModels, cmg.numSubModels);
		}
		s_badInlineModelLogCount++;
		return 0;
#else
		Com_Error (ERR_DROP, "CM_InlineModel: bad number %i total=%i main=%i (may need to re-BSP map?)",
			index, TotalSubModels, cmg.numSubModels);
#endif
	}
	return index;
}

int		CM_NumClusters( void ) {
	return cmg.numClusters;
}

int		CM_NumInlineModels( void ) {
	return cmg.numSubModels;
}

char	*CM_EntityString( void ) {
	return cmg.entityString;
}

char *CM_SubBSPEntityString( int index ) 
{
	return SubBSP[index].entityString;
}

int		CM_LeafCluster( int leafnum ) {
	if (leafnum < 0 || leafnum >= cmg.numLeafs) {
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	}
	return cmg.leafs[leafnum].cluster;
}

int		CM_LeafArea( int leafnum ) {
	if ( leafnum < 0 || leafnum >= cmg.numLeafs ) {
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	}
	return cmg.leafs[leafnum].area;
}

//=======================================================================


/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cplane_t	*p;
	cbrushside_t	*s;

	box_planes = &cmg.planes[cmg.numPlanes];

	box_brush = &cmg.brushes[cmg.numBrushes];
	box_brush->numsides = 6;
	box_brush->sides = cmg.brushsides + cmg.numBrushSides;
	box_brush->contents = CONTENTS_BODY;

	box_model.leaf.numLeafBrushes = 1;
//	box_model.leaf.firstLeafBrush = cmg.numBrushes;
	box_model.leaf.firstLeafBrush = cmg.numLeafBrushes;
	cmg.leafbrushes[cmg.numLeafBrushes] = cmg.numBrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = &cmg.brushsides[cmg.numBrushSides+i];
		s->planeNum = cmg.numPlanes+i*2+side;
		s->shaderNum = cmg.numShaders;

		// planes
		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;

		SetPlaneSignbits( p );
	}	
}



/*
===================
CM_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
clipHandle_t CM_TempBoxModelContents( const vec3_t mins, const vec3_t maxs, const int contents ) {
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );

	box_brush->contents = contents;

	return BOX_MODEL_HANDLE;
}

clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs ) {
	return CM_TempBoxModelContents( mins, maxs, CONTENTS_BODY );
}


/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( clipMap_t &cmg, clipHandle_t model, vec3_t mins, vec3_t maxs ) 
{
	cmodel_t	*cmod;

	cmod = CM_ClipHandleToModel( model );
	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
// Hard-linked EF cgame bridge: collision-model bounds, not renderer bounds.
void CM_STEFX_CoopModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs) {
	cmodel_t *cmod=CM_ClipHandleToModel(model);
	VectorCopy(cmod->mins,mins); VectorCopy(cmod->maxs,maxs);
}
#endif

/*
===================
CM_RegisterTerrain

Allows physics to examine the terrain data.
===================
*/
#if !defined(BSPC)
#if 0	// Removing terrain on Xbox
CCMLandScape *CM_RegisterTerrain(const char *config, bool server)
{
	thandle_t		terrainId;
	CCMLandScape	*ls;

	terrainId = atol(Info_ValueForKey(config, "terrainId"));
	if(terrainId && cmg.landScape)
	{
		// Already spawned so just return
		ls = cmg.landScape;
		ls->IncreaseRefCount();
		return(ls);
	}
	// Doesn't exist so create and link in
	//cmg.numTerrains++;
	ls = CM_InitTerrain(config, 1, server);

	// Increment for the next instance
	if (cmg.landScape)
	{
		Com_Error(ERR_DROP, "You can't have more than one terrain brush.");
	}
	cmg.landScape = ls;
	return(ls);
}

/*
===================
CM_ShutdownTerrain
===================
*/

void CM_ShutdownTerrain( thandle_t terrainId)
{
	CCMLandScape	*landscape;

	landscape = cmg.landScape;
	if (landscape)
	{
		landscape->DecreaseRefCount();
		if(landscape->GetRefCount() <= 0)
		{
			delete landscape;
			cmg.landScape = NULL;
		}
	}
}
#endif	// No terrain on Xbox
#endif

int CM_LoadSubBSP(const char *name, qboolean clientload)
{
	int		i;
//	int		checksum;
	int		count;

	count = cmg.numSubModels;
	for(i = 0; i < NumSubBSP; i++)
	{
		if (!stricmp(name, SubBSP[i].name))
		{
			return count;
		}
		count += SubBSP[i].numSubModels;
	}

	if (NumSubBSP == MAX_SUB_BSP)
	{
		Com_Error (ERR_DROP, "CM_LoadSubBSP: too many unique sub BSPs");
	}

#ifdef _XBOX
	assert(0); // MATT! - testing now - fix this later!
#else
	CM_LoadMap_Actual( name, clientload, &checksum, SubBSP[NumSubBSP] );
#endif
	NumSubBSP++;

	return count;
}

int CM_FindSubBSP(int modelIndex)
{
	int		i;
	int		count;

	count = cmg.numSubModels;
	if (modelIndex < count)
	{	// belongs to the main bsp
		return -1;
	}

	for(i = 0; i < NumSubBSP; i++)
	{
		count += SubBSP[i].numSubModels;
		if (modelIndex < count)
		{
			return i;
		}
	}
	return -1;
}

void CM_GetWorldBounds ( vec3_t mins, vec3_t maxs )
{
	VectorCopy ( cmg.cmodels[0].mins, mins );
	VectorCopy ( cmg.cmodels[0].maxs, maxs );
}

int CM_ModelContents_Actual( clipHandle_t model, clipMap_t *cm ) 
{
	cmodel_t	*cmod;
	int			contents = 0;
	int			i;

	if (!cm)
	{
		cm = &cmg;
	}

	cmod = CM_ClipHandleToModel( model, &cm );

	//MCG ADDED - return the contents, too
	if( cmod->leaf.numLeafBrushes )		// check for brush
	{
		int brushNum;
		for ( i = cmod->leaf.firstLeafBrush; i < cmod->leaf.firstLeafBrush+cmod->leaf.numLeafBrushes; i++ )
		{
			brushNum = cm->leafbrushes[i];
			contents |= cm->brushes[brushNum].contents;
		}
	}
	if( cmod->leaf.numLeafSurfaces )	// if not brush, check for patch
	{	
		int surfaceNum;
		for ( i = cmod->leaf.firstLeafSurface; i < cmod->leaf.firstLeafSurface+cmod->leaf.numLeafSurfaces; i++ )
		{
			surfaceNum = cm->leafsurfaces[i];
			if ( cm->surfaces[surfaceNum] != NULL )
			{//HERNH?  How could we have a null surf within our cmod->leaf.numLeafSurfaces?
				contents |= cm->surfaces[surfaceNum]->contents;
			}
		}
	}
	return contents;
}

int CM_ModelContents(  clipHandle_t model, int subBSPIndex )
{
	if (subBSPIndex < 0)
	{
		return CM_ModelContents_Actual(model, NULL);
	}

	return CM_ModelContents_Actual(model, &SubBSP[subBSPIndex]);
}

//support for save/load games
/*
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
// having to proto this stuff again here is crap, but wtf?...
//
qboolean SG_Append(unsigned long chid, const void *data, int length);
int SG_Read(unsigned long chid, void *pvAddress, int iLength, void **ppvAddressPtr = NULL);

void CM_WritePortalState ()
{	
	SG_Append('PRTS', (void *)cmg.areaPortals, cmg.numAreas * cmg.numAreas * sizeof( *cmg.areaPortals ));
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void	CM_ReadPortalState ()
{
	SG_Read('PRTS', (void *)cmg.areaPortals, cmg.numAreas * cmg.numAreas * sizeof( *cmg.areaPortals ));
	CM_FloodAreaConnections ();

}

