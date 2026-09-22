
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

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"


/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake3 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#include <float.h>
#include "../renderer/tr_local.h"
#include "glw_win_dx8.h"
#include "win_local.h"

#include "xbox_texture_man.h"

#if defined(STEFX_RETAIL_RENDERER_ACTIVE)
extern int R_STEFX_FogModeForView( void );
#endif

#ifdef _XBOX
#include <xgraphics.h>
#include "win_lighteffects.h"
#include "win_highdynamicrange.h"
#include "xb_perf.h"
#include "xb_log.h"
extern "C" volatile unsigned int g_SPXBClTailStage;

static cvar_t *s_stefxSafeAreaLeft;
static cvar_t *s_stefxSafeAreaTop;
static cvar_t *s_stefxSafeAreaRight;
static cvar_t *s_stefxSafeAreaBottom;

// Opt-in co-op experiment: preserve the display mode and buffer capacities.
// The SDK scales rasterization/presentation; this does not reclaim RAM.
static qboolean STEFX_CoopLowResolution(void)
{
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	static cvar_t *enabled;
	if (!enabled)
		enabled = Cvar_Get("r_efCoopLowRes", "0", 0);
	return enabled->integer == 1 &&
		Cvar_VariableIntegerValue("stefx_splitScreen") &&
		Cvar_VariableIntegerValue("stefx_splitScreenPlayers") == 2 &&
		!Q_stricmp(Cvar_VariableString("stefx_splitScreenMode"), "coop");
#else
	return qfalse;
#endif
}

extern "C" {
volatile unsigned int g_SPXBCoopResolution[8];
}

static int STEFX_SafeAreaClamp(int value, int maximum)
{
	if (value < 0)
		return 0;
	if (value > maximum)
		return maximum;
	return value;
}

static void STEFX_GetSafeArea(int *left, int *top, int *right, int *bottom)
{
	if (!s_stefxSafeAreaLeft)
	{
		s_stefxSafeAreaLeft = Cvar_Get("stefx_safeAreaLeft", "0", CVAR_ARCHIVE);
		s_stefxSafeAreaTop = Cvar_Get("stefx_safeAreaTop", "0", CVAR_ARCHIVE);
		s_stefxSafeAreaRight = Cvar_Get("stefx_safeAreaRight", "0", CVAR_ARCHIVE);
		s_stefxSafeAreaBottom = Cvar_Get("stefx_safeAreaBottom", "0", CVAR_ARCHIVE);
	}

	*left = STEFX_SafeAreaClamp(s_stefxSafeAreaLeft->integer, glConfig.vidWidth * 15 / 100);
	*top = STEFX_SafeAreaClamp(s_stefxSafeAreaTop->integer, glConfig.vidHeight * 15 / 100);
	*right = STEFX_SafeAreaClamp(s_stefxSafeAreaRight->integer, glConfig.vidWidth * 15 / 100);
	*bottom = STEFX_SafeAreaClamp(s_stefxSafeAreaBottom->integer, glConfig.vidHeight * 15 / 100);
	if (STEFX_CoopLowResolution() && glw_state && glw_state->isWidescreen)
	{
		// A 3/4-width region in 16:9 output is physically 4:3.
		const int border = glConfig.vidWidth / 8;
		if (*left < border) *left = border;
		if (*right < border) *right = border;
	}
}

// Map every requested D3D-space rectangle through one display-safe rectangle.
// Split-screen seams therefore still meet exactly; only the outside display
// edge can receive an intentional calibration border.
static qboolean STEFX_ApplySafeArea(GLint& x, GLint& y, GLsizei& width, GLsizei& height)
{
	int left, top, right, bottom;
	int safeWidth, safeHeight;
	int x2, y2;
	static int lastLeft = -1;
	static int lastTop = -1;
	static int lastRight = -1;
	static int lastBottom = -1;

	STEFX_GetSafeArea(&left, &top, &right, &bottom);
	if (!left && !top && !right && !bottom)
		return qfalse;

	if (left != lastLeft || top != lastTop || right != lastRight || bottom != lastBottom)
	{
		XBLF("STEFX_SAFE_AREA: apply left=%d top=%d right=%d bottom=%d screen=%dx%d",
			left, top, right, bottom, glConfig.vidWidth, glConfig.vidHeight);
		lastLeft = left;
		lastTop = top;
		lastRight = right;
		lastBottom = bottom;
	}

	safeWidth = glConfig.vidWidth - left - right;
	safeHeight = glConfig.vidHeight - top - bottom;
	x2 = x + width;
	y2 = y + height;
	x = left + (x * safeWidth + glConfig.vidWidth / 2) / glConfig.vidWidth;
	y = top + (y * safeHeight + glConfig.vidHeight / 2) / glConfig.vidHeight;
	x2 = left + (x2 * safeWidth + glConfig.vidWidth / 2) / glConfig.vidWidth;
	y2 = top + (y2 * safeHeight + glConfig.vidHeight / 2) / glConfig.vidHeight;
	width = x2 - x;
	height = y2 - y;
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;
	return qtrue;
}

#ifdef _XBOX
/*
** STEFX vertex scratch ring
**
** The inherited indexed path copies every draw's full vertex payload inline
** into the primary push buffer behind a jump packet, so payload dwords count
** against the 1 MiB primary buffer and BeginPush stalls on GPU consumption
** once a frame's aggregate reservations approach that capacity.  This ring
** keeps the payload in two fenced write-combined physical buffers outside
** the push buffer; each draw then reserves only its command/index dwords.
** NV2A vertex DMA reads the payload through the same physical stream
** pointers either way, so the produced pixels are identical.
**
** r_efScratchVerts 0/1 selects the path per frame.  r_efScratchAB <seconds>
** overrides it with wall-clock alternation windows for paired A/B timing in
** one session; samples carry the window serial so boundary samples can be
** discarded offline.
*/
#define STEFX_SCRATCH_BUFFER_COUNT 3 /* bounded MP layout experiments */
#define STEFX_SCRATCH_BUFFER_DWORDS (256 * 1024) /* baseline: 1 MiB per buffer */
static int s_stefxScratchBufferCount = 2;
static unsigned int s_stefxScratchCapacityDwords = STEFX_SCRATCH_BUFFER_DWORDS;
extern "C" volatile unsigned int g_SPXBScratchLayoutProof[4] = { 2, STEFX_SCRATCH_BUFFER_DWORDS, 0, 0 };

// Startup-only experiments; at most 4.5 MiB total, 2.5 MiB above the control.
// Wider buffers test capacity; the third slot tests GPU reuse latency.
static void STEFX_ScratchSelectLayout(int buffers, int kilobytes)
{
	s_stefxScratchBufferCount = buffers == 3 ? 3 : 2;
	s_stefxScratchCapacityDwords = (kilobytes == 1536)
		? 384u * 1024u : STEFX_SCRATCH_BUFFER_DWORDS;
}

extern "C" volatile unsigned int g_SPXBScratchMode = 0;
extern "C" volatile unsigned int g_SPXBScratchFlip = 0;
extern "C" volatile unsigned int g_SPXBScratchDraws = 0;
extern "C" volatile unsigned int g_SPXBScratchFallbacks = 0;
extern "C" volatile unsigned int g_SPXBScratchWaitMsec = 0;

static DWORD *s_stefxScratchBase[STEFX_SCRATCH_BUFFER_COUNT];
static DWORD s_stefxScratchFence[STEFX_SCRATCH_BUFFER_COUNT];
static unsigned int s_stefxScratchOffset;
static unsigned int s_stefxScratchPeakOffset;
static unsigned int s_stefxScratchDwords;
static unsigned int s_stefxScratchFenceWaits;
static int s_stefxScratchIndex;
static int s_stefxScratchReady;
static int s_stefxScratchInitDone;
static int s_stefxScratchFrameActive;
static cvar_t *s_stefxScratchCvar;
static cvar_t *s_stefxScratchABCvar;
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
static void STEFX_CoopWorldConfigure(void);
static void STEFX_CoopModelReset(void);
static void STEFX_CoopModelConfigure(void);
static bool s_coopWorldBorrowed;
extern "C" volatile unsigned int g_SPXBCoopWorldStorage[8] = {0};
#endif

static qboolean STEFX_ScratchAllocateLayout(void)
{
	int i;
	for (i = 0; i < s_stefxScratchBufferCount; ++i)
	{
		s_stefxScratchBase[i] = (DWORD *)XPhysicalAlloc(
			s_stefxScratchCapacityDwords * sizeof(DWORD), MAXULONG_PTR, 16,
			PAGE_READWRITE | PAGE_WRITECOMBINE);
		s_stefxScratchFence[i] = 0;
		if (!s_stefxScratchBase[i])
		{
			while (i > 0)
			{
				--i;
				XPhysicalFree(s_stefxScratchBase[i]);
				s_stefxScratchBase[i] = NULL;
			}
			return qfalse;
		}
	}
	return qtrue;
}

static void STEFX_ScratchInit(void)
{
	s_stefxScratchInitDone = 1;
	s_stefxScratchCvar = Cvar_Get("r_efScratchVerts", "1", 0);
	s_stefxScratchABCvar = Cvar_Get("r_efScratchAB", "0", 0);
#if defined(STEFX_SP_HOSTED_MP)
	// Keep the production control as the default until measured qualification.
	STEFX_ScratchSelectLayout(Cvar_Get("r_efScratchBuffers", "2", 0)->integer,
		Cvar_Get("r_efScratchKB", "1024", 0)->integer);
#endif
	s_stefxScratchReady = STEFX_ScratchAllocateLayout();
	if (!s_stefxScratchReady && (s_stefxScratchBufferCount != 2 ||
		s_stefxScratchCapacityDwords != STEFX_SCRATCH_BUFFER_DWORDS))
	{
		g_SPXBScratchLayoutProof[2] = 1;
		XBLog_WriteCritical("STEFX_D3D8: larger vertex scratch allocation failed; retrying original 2x1024KB");
		STEFX_ScratchSelectLayout(2, 1024);
		s_stefxScratchReady = STEFX_ScratchAllocateLayout();
	}
	g_SPXBScratchLayoutProof[0] = (unsigned int)s_stefxScratchBufferCount;
	g_SPXBScratchLayoutProof[1] = s_stefxScratchCapacityDwords;
	g_SPXBScratchLayoutProof[3] = (unsigned int)s_stefxScratchReady;
	XBLog_WriteCriticalf(
		"STEFX_D3D8: vertexScratchRing ready=%d buffers=%dx%uKB enable=%d ab=%d",
		s_stefxScratchReady, s_stefxScratchBufferCount,
		(unsigned int)(s_stefxScratchCapacityDwords * sizeof(DWORD) / 1024u),
		s_stefxScratchCvar->integer, s_stefxScratchABCvar->integer);
}

static void STEFX_ScratchFrameBegin(void)
{
	int enabled = 0;

	if (!s_stefxScratchInitDone)
	{
		STEFX_ScratchInit();
	}
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	// Ownership changes happen before any draw reservations for this frame.
	STEFX_CoopWorldConfigure();
	STEFX_CoopModelConfigure();
#endif
	if (s_stefxScratchReady && glw_state->device)
	{
		const int abSeconds = s_stefxScratchABCvar->integer;
		if (abSeconds > 0)
		{
			// Paired A/B: alternate on absolute wall-clock windows so both
			// arms interleave through one session.  Even windows run the
			// ring, odd windows run the original inline path.
			const unsigned int window =
				(unsigned int)(Sys_Milliseconds() / (abSeconds * 1000));
			enabled = ((window & 1u) == 0u) ? 1 : 0;
			g_SPXBScratchFlip = window;
		}
		else
		{
			enabled = s_stefxScratchCvar->integer ? 1 : 0;
			g_SPXBScratchFlip = 0;
		}
	}
	if (enabled)
	{
		if (++s_stefxScratchIndex >= s_stefxScratchBufferCount)
			s_stefxScratchIndex = 0;
		if (s_stefxScratchFence[s_stefxScratchIndex] &&
			glw_state->device->IsFencePending(
				s_stefxScratchFence[s_stefxScratchIndex]))
		{
			const int waitStart = Sys_Milliseconds();
			++s_stefxScratchFenceWaits;
			glw_state->device->BlockOnFence(
				s_stefxScratchFence[s_stefxScratchIndex]);
			g_SPXBScratchWaitMsec +=
				(unsigned int)(Sys_Milliseconds() - waitStart);
		}
		s_stefxScratchOffset = 0;
	}
	s_stefxScratchFrameActive = enabled;
	g_SPXBScratchMode = (unsigned int)enabled;
}

#if defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP)
static void STEFX_WorldVerticesReport(void);
#endif
static void STEFX_ScratchFrameEnd(void)
{
#if defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP)
	STEFX_WorldVerticesReport();
#endif
	if (s_stefxScratchFrameActive && s_stefxScratchReady && glw_state->device)
	{
		s_stefxScratchFence[s_stefxScratchIndex] =
			glw_state->device->InsertFence();
		if (s_stefxScratchOffset > s_stefxScratchPeakOffset)
		{
			s_stefxScratchPeakOffset = s_stefxScratchOffset;
		}
	}
}

static __forceinline DWORD *STEFX_ScratchClaim(unsigned int dwords)
{
	const unsigned int aligned = (dwords + 3u) & ~3u;
	DWORD *claimed;

	if (dwords > s_stefxScratchCapacityDwords ||
		aligned > s_stefxScratchCapacityDwords - s_stefxScratchOffset)
	{
		// Ring full this frame: the caller falls back to the inline path.
		++g_SPXBScratchFallbacks;
		return NULL;
	}
	claimed = s_stefxScratchBase[s_stefxScratchIndex] + s_stefxScratchOffset;
	s_stefxScratchOffset += aligned;
	++g_SPXBScratchDraws;
	s_stefxScratchDwords += aligned;
	return claimed;
}
#endif // _XBOX

#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP) || defined(STEFX_HW_FRAME_DIAGNOSTICS))
// Base-material marker is also used by SP/co-op diagnostic draw classification.
bool g_stefxWorldBasePass = false;
#endif

// STEFX_WORLD_VERTICES_BEGIN
// Private immutable storage for the native indexed draw path.
// Immutable, per-surface/per-stage vertices; visible indices remain dynamic.
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP))
#define STEFX_WORLD_VERTEX_CAPACITY 21840u
#if defined(STEFX_SP_HOSTED_MP)
#define STEFX_WORLD_ARRAY_CAPACITY 43680u
#define STEFX_WORLD_VERTEX_KEYS 8192u
#else
// Co-op borrows exactly one existing 1 MiB scratch slab; no GPU allocation.
#define STEFX_WORLD_ARRAY_CAPACITY 21840u
#define STEFX_WORLD_VERTEX_KEYS 4096u
// Leave headroom for small BSP faces and bound misses when the cache fills.
#define STEFX_COOP_WORLD_MAX_PROBES 32u
#endif
struct stefxWorldVertex_t {
    float xyz[3], normal[3];
    DWORD color;
    float uv0[2], uv1[2];
    DWORD pad;
};
typedef char stefxWorldVertexSize[(sizeof(stefxWorldVertex_t) == 48) ? 1 : -1];
struct stefxWorldVertexKey_t {
    const void *surface;
    const shaderStage_t *stage;
    const shader_t *shader;
    unsigned short first, count;
    unsigned int format;
};
struct stefxWorldSpan_t { const void *surface; int first, count; };
static stefxWorldVertex_t *s_worldVertices;
static stefxWorldVertexKey_t s_worldVertexKeys[STEFX_WORLD_VERTEX_KEYS];
static stefxWorldSpan_t s_worldSpans[SHADER_MAX_VERTEXES];
static unsigned short s_worldRemap[SHADER_MAX_VERTEXES];
static GLushort s_worldIndices[SHADER_MAX_INDEXES];
static unsigned int s_worldVertexUsed;
static int s_worldSpanCount, s_worldCovered;
static cvar_t *s_worldVertexCvar, *s_worldVerifyCvar;
static bool s_worldAllocAttempted;
static int s_worldStorageMode;
static bool s_worldArraysActive;
struct stefxWorldArrayRun_t { int span, indexFirst, count; unsigned int vertexFirst; };
static stefxWorldArrayRun_t s_worldArrayRuns[SHADER_MAX_VERTEXES];
static unsigned short s_worldArrayCounts[STEFX_WORLD_VERTEX_KEYS];
static unsigned short s_worldVertexOwners[SHADER_MAX_VERTEXES];
static unsigned char s_worldArraySeen[SHADER_MAX_VERTEXES];
static int s_worldArrayRunCount, s_worldArrayCommandCount;
extern "C" volatile unsigned int g_SPXBWorldArrays[8] = {0};
extern "C" volatile unsigned int g_SPXBWorldVertices[8] = {0};
extern "C" volatile unsigned int g_SPXBWorldRejects[8] = {0};
#if defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBFxCull[3];
extern "C" volatile unsigned int g_SPXBFxMerged;
#endif
static void STEFX_WorldVerticesReport(void) {
    static unsigned int frames;
    if ((++frames & 511u) != 0u) return;
#if defined(STEFX_SP_HOSTED_MP)
    XBLog_WriteCriticalf("STEFX_RENDER_REUSE: eligible=%u draws=%u storedSurfaces=%u vertices=%u full=%u hits=%u mismatches=%u badIndexes=%u fxTest=%u fxCull=%u fxMerged=%u",
        g_SPXBWorldVertices[0], g_SPXBWorldVertices[1], g_SPXBWorldVertices[2],
        g_SPXBWorldVertices[3], g_SPXBWorldVertices[4], g_SPXBWorldVertices[5],
        g_SPXBWorldVertices[6], g_SPXBWorldVertices[7], g_SPXBFxCull[0],
        g_SPXBFxCull[1], g_SPXBFxMerged);
#else
    if (s_coopWorldBorrowed)
        XBLog_WriteCriticalf("STEFX_COOP_WORLD: draws=%u vertices=%u hits=%u full=%u mismatches=%u scratchBuffers=%d",
            g_SPXBWorldVertices[1], s_worldVertexUsed, g_SPXBWorldVertices[5],
            g_SPXBWorldVertices[4], g_SPXBWorldVertices[6], s_stefxScratchBufferCount);
#endif
}

void STEFX_WorldVerticesBeginBatch(void) {
    s_worldSpanCount = s_worldCovered = 0;
}
void STEFX_WorldVerticesSurface(const void *surface, int count) {
#if !defined(STEFX_SP_HOSTED_MP)
    if (!s_coopWorldBorrowed || !s_worldVertexCvar || s_worldVertexCvar->integer != 2) return;
#endif
    // Gaps indicate an unsupported surface was mixed into this batch.
    if (backEnd.currentEntity != &tr.worldEntity ||
        s_worldCovered != tess.numVertexes || count <= 0 ||
        count > SHADER_MAX_VERTEXES - s_worldCovered ||
        s_worldSpanCount >= SHADER_MAX_VERTEXES) return;
    stefxWorldSpan_t &span = s_worldSpans[s_worldSpanCount++];
    span.surface = surface; span.first = tess.numVertexes; span.count = count;
    s_worldCovered += count;
}
void STEFX_WorldVerticesReset(void) {
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
    STEFX_CoopModelReset();
#endif
    // Never overwrite/release vertices that an earlier frame still references.
    if (s_worldVertices && glw_state && glw_state->device)
        glw_state->device->BlockUntilIdle();
#if defined(STEFX_SP_HOSTED_MP)
    if (s_worldVertices) XPhysicalFree(s_worldVertices);
#endif
    s_worldVertices = NULL;
    s_worldAllocAttempted = false;
    s_worldVertexUsed = 0;
    s_worldStorageMode = 0;
    s_worldArraysActive = false;
    memset(s_worldArrayCounts, 0, sizeof(s_worldArrayCounts));
    memset((void *)g_SPXBWorldArrays, 0, sizeof(g_SPXBWorldArrays));
    g_stefxWorldBasePass = false;
    memset(s_worldVertexKeys, 0, sizeof(s_worldVertexKeys));
    memset((void *)g_SPXBWorldVertices, 0, sizeof(g_SPXBWorldVertices));
    STEFX_WorldVerticesBeginBatch();
}

static bool STEFX_WorldReject(unsigned int reason) {
    if (++g_SPXBWorldRejects[reason] == 1u) {
        const shaderStage_t *s = tess.xstages && tess.currentPass >= 0 &&
            tess.shader && tess.currentPass < tess.shader->numUnfoggedPasses ? &tess.xstages[tess.currentPass] : NULL;
        XBLog_WriteCriticalf("STEFX_WORLD_REJECT: reason=%u shader=%s world=%d split=%d spans=%d covered=%d verts=%d fog=%d dl=%d rgb=%d alpha=%d mods=%d,%d",
            reason, tess.shader ? tess.shader->name : "null", backEnd.currentEntity == &tr.worldEntity,
            backEnd.viewParms.stefxSplitView, s_worldSpanCount, s_worldCovered, tess.numVertexes,
            tess.fogNum, tess.dlightBits, s ? s->rgbGen : -1, s ? s->alphaGen : -1,
            s ? s->bundle[0].numTexMods : -1, s ? s->bundle[1].numTexMods : -1);
    }
    return false;
}
static bool STEFX_WorldStageStatic(int normals, int tex0, int tex1) {
    if (!tess.shader || !tess.xstages || tess.currentPass < 0 ||
        tess.currentPass >= tess.shader->numUnfoggedPasses) return STEFX_WorldReject(0);
    if (tess.shader->numDeforms || tess.shader->sky || tess.fading) return STEFX_WorldReject(1);
    if (!g_stefxWorldBasePass || backEnd.projection2D ||
        backEnd.currentEntity != &tr.worldEntity ||
        backEnd.currentEntity->e.renderfx ||
        !backEnd.viewParms.stefxSplitView) return STEFX_WorldReject(2);
    if (!s_worldSpanCount || s_worldCovered != tess.numVertexes) return STEFX_WorldReject(3);
    const shaderStage_t *stage = &tess.xstages[tess.currentPass];
    if (!stage->active || stage->isBumpMap || stage->isEnvironment || stage->ss)
        return STEFX_WorldReject(4);
    // Dynamic lights are applied in a later pass. Only the explicitly marked
    // base material draw reaches this path, so its stable attributes can be
    // reused even when the batch also needs lighting. Fog-modulated vertex
    // colors remain dynamic; hardware fog state does not modify vertex data.
    if (tess.fogNum && stage->adjustColorsForFog) return STEFX_WorldReject(1);
    // These RGB generators set all four components. Thus AGEN_SKIP is safe
    // here too; vertex-lit surfaces can follow mutable light styles.
    if (stage->rgbGen != CGEN_IDENTITY && stage->rgbGen != CGEN_IDENTITY_LIGHTING &&
        stage->rgbGen != CGEN_CONST) return STEFX_WorldReject(5);
    if (stage->alphaGen != AGEN_IDENTITY && stage->alphaGen != AGEN_CONST &&
        stage->alphaGen != AGEN_SKIP) return STEFX_WorldReject(6);
    for (int b = 0; b < 2; ++b) {
        if (!(b ? tex1 : tex0)) continue;
        const textureBundle_t *bundle = &stage->bundle[b];
        if (bundle->numTexMods ||
            (bundle->tcGen != TCGEN_IDENTITY && bundle->tcGen != TCGEN_TEXTURE &&
             bundle->tcGen != TCGEN_LIGHTMAP && bundle->tcGen != TCGEN_LIGHTMAP1 &&
             bundle->tcGen != TCGEN_LIGHTMAP2 && bundle->tcGen != TCGEN_LIGHTMAP3 &&
             bundle->tcGen != TCGEN_VECTOR)) return STEFX_WorldReject(7);
    }
    return true;
}
static void STEFX_WorldPackVertex(stefxWorldVertex_t &v, int i,
    int normals, int tex0, int tex1) {
    memset(&v, 0, sizeof(v));
    memcpy(v.xyz, tess.xyz[i], sizeof(v.xyz));
    if (normals) memcpy(v.normal, tess.normal[i], sizeof(v.normal));
    v.color = glw_state->colorArrayState ? tess.svars.colors[i] : glw_state->currentColor;
    if (tex0) memcpy(v.uv0, tess.svars.texcoords[0][i], sizeof(v.uv0));
    if (tex1) memcpy(v.uv1, tess.svars.texcoords[1][i], sizeof(v.uv1));
}
static stefxWorldVertex_t *STEFX_WorldArraysClaim(int count, const GLushort *indices,
    int normals, int tex0, int tex1);
static void STEFX_WorldInitCvars(void) {
    if (s_worldVertexCvar) return;
#if defined(STEFX_SP_HOSTED_MP)
    s_worldVertexCvar = Cvar_Get("r_efWorldVertices", "2", 0);
    s_worldVerifyCvar = Cvar_Get("r_efWorldVerticesVerify", "0", 0);
#else
    // 0: original two slabs; 1: one-slab control; 2: one slab + resident world.
    s_worldVertexCvar = Cvar_Get("r_efCoopWorldVertices", "0", 0);
    s_worldVerifyCvar = Cvar_Get("r_efCoopWorldVerify", "0", 0);
#endif
}
static stefxWorldVertex_t *STEFX_WorldVerticesClaim(GLenum mode, int count,
    const GLushort *indices, int normals, int tex0, int tex1) {
    s_worldArraysActive = false;
    STEFX_WorldInitCvars();
#if !defined(STEFX_SP_HOSTED_MP)
    if (!s_coopWorldBorrowed || s_worldVertexCvar->integer != 2) return NULL;
#endif
    if (s_worldVertexCvar->integer < 1 || s_worldVertexCvar->integer > 2 || mode != GL_TRIANGLES ||
        count <= 0 || count > SHADER_MAX_INDEXES ||
        !STEFX_WorldStageStatic(normals, tex0, tex1)) return NULL;
    ++g_SPXBWorldVertices[0]; // eligible draws
    // Changing storage mode requires a map reset; never reinterpret live data.
    if (s_worldAllocAttempted && s_worldStorageMode != s_worldVertexCvar->integer) return NULL;
    if (s_worldVertexCvar->integer == 2)
        return STEFX_WorldArraysClaim(count, indices, normals, tex0, tex1);
    if (!s_worldAllocAttempted) {
        s_worldAllocAttempted = true;
        s_worldStorageMode = 1;
        s_worldVertices = (stefxWorldVertex_t *)XPhysicalAlloc(
            STEFX_WORLD_VERTEX_CAPACITY * sizeof(stefxWorldVertex_t),
            MAXULONG_PTR, 4096, PAGE_READWRITE | PAGE_WRITECOMBINE);
        XBLog_WriteCriticalf("STEFX_WORLD_VERTICES: allocated=%u capacity=%u bytes=%u verify=%d",
            s_worldVertices ? 1u : 0u, STEFX_WORLD_VERTEX_CAPACITY,
            STEFX_WORLD_VERTEX_CAPACITY * sizeof(stefxWorldVertex_t), s_worldVerifyCvar->integer);
    }
    if (!s_worldVertices) return NULL;
    const shaderStage_t *stage = &tess.xstages[tess.currentPass];
    // Identity lighting and disabled color arrays are part of the immutable key.
    unsigned int format = (normals ? 1u : 0u) | (tex0 ? 2u : 0u) | (tex1 ? 4u : 0u) |
        (glw_state->colorArrayState ? 8u : 0u) | ((unsigned int)tr.identityLightByte << 8);
    if (!glw_state->colorArrayState) return NULL;
    for (int s = 0; s < s_worldSpanCount; ++s) {
        const stefxWorldSpan_t &span = s_worldSpans[s];
        unsigned int slot = (((unsigned int)span.surface >> 4) ^
            ((unsigned int)stage >> 3) ^ format) & (STEFX_WORLD_VERTEX_KEYS - 1);
        unsigned int probes;
        stefxWorldVertexKey_t *key = NULL;
        for (probes = 0; probes < STEFX_WORLD_VERTEX_KEYS; ++probes) {
            key = &s_worldVertexKeys[slot];
            if (!key->surface || (key->surface == span.surface && key->stage == stage &&
                key->shader == tess.shader && key->format == format && key->count == span.count)) break;
            slot = (slot + 1) & (STEFX_WORLD_VERTEX_KEYS - 1);
        }
        if (probes == STEFX_WORLD_VERTEX_KEYS || (!key->surface &&
            (unsigned int)span.count > STEFX_WORLD_VERTEX_CAPACITY - s_worldVertexUsed)) {
            ++g_SPXBWorldVertices[4]; return NULL; // bounded fallback; never evict in-flight data
        }
        if (!key->surface) {
            key->surface = span.surface; key->stage = stage; key->shader = tess.shader;
            key->format = format; key->first = (unsigned short)s_worldVertexUsed;
            key->count = (unsigned short)span.count;
            for (int v = 0; v < span.count; ++v) {
                stefxWorldVertex_t packed;
                STEFX_WorldPackVertex(packed, span.first + v, normals, tex0, tex1);
                memcpy(&s_worldVertices[s_worldVertexUsed + v], &packed, sizeof(packed));
            }
            s_worldVertexUsed += span.count;
            ++g_SPXBWorldVertices[2];
            g_SPXBWorldVertices[3] = s_worldVertexUsed;
        } else {
            ++g_SPXBWorldVertices[5]; // reused surfaces, independent of visible batch composition
            if (s_worldVerifyCvar->integer) {
                for (int v = 0; v < span.count; ++v) {
                    stefxWorldVertex_t packed;
                    STEFX_WorldPackVertex(packed, span.first + v, normals, tex0, tex1);
                    if (memcmp(&s_worldVertices[key->first + v], &packed, sizeof(packed))) {
                        ++g_SPXBWorldVertices[6];
                        XBLog_WriteCriticalf("STEFX_WORLD_VERTICES: mismatch shader=%s pass=%d vertex=%d",
                            tess.shader->name, tess.currentPass, v);
                        s_worldVertexCvar->integer = 0;
                        return NULL;
                    }
                }
            }
        }
        for (int v = 0; v < span.count; ++v)
            s_worldRemap[span.first + v] = (unsigned short)(key->first + v);
    }
    for (int i = 0; i < count; ++i) {
        if (indices[i] >= tess.numVertexes) { ++g_SPXBWorldVertices[7]; return NULL; }
        s_worldIndices[i] = s_worldRemap[indices[i]];
    }
    ++g_SPXBWorldVertices[1]; // successful persistent draws
    return s_worldVertices;
}

// Expanded immutable triangles avoid a changing index list on each visible
// world batch. Surface order and triangle order are retained exactly. The
// source spans are exclusively immutable BSP faces/triangles, not model LODs,
// generated effects, patches, or partial per-triangle visibility results.
static stefxWorldVertex_t *STEFX_WorldArraysClaim(int count, const GLushort *indices,
    int normals, int tex0, int tex1) {
    if ((count % 3) || !glw_state->colorArrayState) return NULL;
    memset(s_worldArraySeen, 0, sizeof(s_worldArraySeen));
    s_worldArrayRunCount = s_worldArrayCommandCount = 0;
    for (int s = 0; s < s_worldSpanCount; ++s) {
        const stefxWorldSpan_t &span = s_worldSpans[s];
        for (int v = 0; v < span.count; ++v) s_worldVertexOwners[span.first + v] = (unsigned short)s;
    }
    int previous = -1;
    for (int i = 0; i < count; i += 3) {
        if (indices[i] >= tess.numVertexes || indices[i+1] >= tess.numVertexes || indices[i+2] >= tess.numVertexes) {
            ++g_SPXBWorldVertices[7]; return NULL;
        }
        int owner = s_worldVertexOwners[indices[i]];
        if (owner != s_worldVertexOwners[indices[i+1]] || owner != s_worldVertexOwners[indices[i+2]]) {
            ++g_SPXBWorldArrays[3]; return NULL;
        }
        if (owner != previous) {
            if (s_worldArraySeen[owner] || s_worldArrayRunCount == SHADER_MAX_VERTEXES) {
                ++g_SPXBWorldArrays[3]; return NULL;
            }
            s_worldArraySeen[owner] = 1;
            stefxWorldArrayRun_t &run = s_worldArrayRuns[s_worldArrayRunCount++];
            run.span = owner; run.indexFirst = i; run.count = 0;
            previous = owner;
        }
        s_worldArrayRuns[s_worldArrayRunCount-1].count += 3;
    }
    for (int r = 0; r < s_worldArrayRunCount; ++r)
        s_worldArrayCommandCount += (s_worldArrayRuns[r].count + 254) / 255;
    // Bound the native multi-range packet; unusual fragmented batches retain
    // the existing indexed path. 255 vertices keeps every range triangle-aligned.
    if (s_worldArrayCommandCount > 511) { ++g_SPXBWorldArrays[3]; return NULL; }
    if (!s_worldAllocAttempted) {
        s_worldAllocAttempted = true; s_worldStorageMode = 2;
#if defined(STEFX_SP_HOSTED_MP)
        s_worldVertices = (stefxWorldVertex_t *)XPhysicalAlloc(
            STEFX_WORLD_ARRAY_CAPACITY * sizeof(stefxWorldVertex_t),
            MAXULONG_PTR, 4096, PAGE_READWRITE | PAGE_WRITECOMBINE);
#else
        s_worldVertices = (stefxWorldVertex_t *)s_stefxScratchBase[1];
#endif
        XBLog_WriteCriticalf("STEFX_WORLD_ARRAYS: allocated=%u capacity=%u bytes=%u verify=%d",
            s_worldVertices ? 1u : 0u, STEFX_WORLD_ARRAY_CAPACITY,
            STEFX_WORLD_ARRAY_CAPACITY * sizeof(stefxWorldVertex_t), s_worldVerifyCvar->integer);
    }
    if (!s_worldVertices) return NULL;
    const shaderStage_t *stage = &tess.xstages[tess.currentPass];
    unsigned int format = (normals ? 1u : 0u) | (tex0 ? 2u : 0u) | (tex1 ? 4u : 0u) |
        8u | ((unsigned int)tr.identityLightByte << 8);
    for (int r = 0; r < s_worldArrayRunCount; ++r) {
        stefxWorldArrayRun_t &run = s_worldArrayRuns[r];
        const stefxWorldSpan_t &span = s_worldSpans[run.span];
        unsigned int slot = (((unsigned int)span.surface >> 4) ^ ((unsigned int)stage >> 3) ^ format) & (STEFX_WORLD_VERTEX_KEYS - 1);
        stefxWorldVertexKey_t *key = NULL;
        unsigned int probes;
#if defined(STEFX_SP_HOSTED_MP)
        const unsigned int probeLimit = STEFX_WORLD_VERTEX_KEYS;
#else
        const unsigned int probeLimit = STEFX_COOP_WORLD_MAX_PROBES;
#endif
        for (probes = 0; probes < probeLimit; ++probes) {
            key = &s_worldVertexKeys[slot];
            if (!key->surface || (key->surface == span.surface && key->stage == stage &&
                key->shader == tess.shader && key->format == format && key->count == span.count)) break;
            slot = (slot + 1) & (STEFX_WORLD_VERTEX_KEYS - 1);
        }
        if (probes == probeLimit || (!key->surface &&
            (unsigned int)run.count > STEFX_WORLD_ARRAY_CAPACITY - s_worldVertexUsed)) {
            ++g_SPXBWorldVertices[4]; ++g_SPXBWorldArrays[4]; return NULL;
        }
        if (!key->surface) {
            key->surface = span.surface; key->stage = stage; key->shader = tess.shader;
            key->format = format; key->first = (unsigned short)s_worldVertexUsed;
            key->count = (unsigned short)span.count;
            s_worldArrayCounts[slot] = (unsigned short)run.count;
            for (int v = 0; v < run.count; ++v) {
                int source = indices[run.indexFirst + v];
                stefxWorldVertex_t packed;
                STEFX_WorldPackVertex(packed, source, normals, tex0, tex1);
                packed.pad = source - span.first; // topology witness; not a GPU attribute
                memcpy(&s_worldVertices[s_worldVertexUsed + v], &packed, sizeof(packed));
            }
            s_worldVertexUsed += run.count;
            ++g_SPXBWorldVertices[2]; g_SPXBWorldVertices[3] = s_worldVertexUsed;
        } else {
            if (s_worldArrayCounts[slot] != run.count) { ++g_SPXBWorldArrays[3]; return NULL; }
            ++g_SPXBWorldVertices[5];
        }
        if (s_worldVerifyCvar->integer) {
            for (int v = 0; v < run.count; ++v) {
                int source = indices[run.indexFirst + v];
                stefxWorldVertex_t packed;
                STEFX_WorldPackVertex(packed, source, normals, tex0, tex1);
                packed.pad = source - span.first;
                ++g_SPXBWorldArrays[5];
                if (memcmp(&s_worldVertices[key->first + v], &packed, sizeof(packed))) {
                    ++g_SPXBWorldVertices[6]; ++g_SPXBWorldArrays[6];
                    XBLog_WriteCriticalf("STEFX_WORLD_ARRAYS: mismatch shader=%s pass=%d vertex=%d",
                        tess.shader->name, tess.currentPass, v);
                    s_worldVertexCvar->integer = 0;
                    return NULL;
                }
            }
        }
        run.vertexFirst = key->first;
    }
    s_worldArraysActive = true;
    ++g_SPXBWorldArrays[0]; g_SPXBWorldArrays[1] = s_worldVertexUsed;
    g_SPXBWorldArrays[2] += s_worldArrayCommandCount; g_SPXBWorldArrays[7] = 2;
    ++g_SPXBWorldVertices[1];
    return s_worldVertices;
}

static DWORD *STEFX_WorldArraysWrite(DWORD *packet, unsigned int primitive) {
    *packet++ = D3DPUSH_ENCODE(D3DPUSH_SET_BEGIN_END, 1);
    *packet++ = primitive;
    *packet++ = D3DPUSH_ENCODE(D3DPUSH_NOINCREMENT_FLAG | 0x1810, s_worldArrayCommandCount);
    for (int r = 0; r < s_worldArrayRunCount; ++r) {
        unsigned int first = s_worldArrayRuns[r].vertexFirst;
        int remaining = s_worldArrayRuns[r].count;
        while (remaining) {
            unsigned int vertices = remaining > 255 ? 255 : remaining;
            *packet++ = first | ((vertices - 1u) << 24);
            first += vertices; remaining -= vertices;
        }
    }
    return packet; // existing caller emits END and submits the packet
}
#if !defined(STEFX_SP_HOSTED_MP)
// STEFX_COOP_WORLD_CONFIGURE_BEGIN
static void STEFX_CoopWorldConfigure(void) {
    STEFX_WorldInitCvars();
    const int mode = s_worldVertexCvar->integer;
    const bool borrow = (mode == 1 || mode == 2) && Cvar_VariableIntegerValue("stefx_splitScreen") && s_stefxScratchReady &&
        s_stefxScratchBase[1] &&
        s_stefxScratchCapacityDwords * sizeof(DWORD) >= STEFX_WORLD_ARRAY_CAPACITY * sizeof(stefxWorldVertex_t);
    if (borrow != s_coopWorldBorrowed) {
        if (!glw_state || !glw_state->device) return;
        // The second slab may still belong to earlier GPU draws. Fence all
        // prior users before either making it immutable or returning it.
        glw_state->device->BlockUntilIdle();
        STEFX_WorldVerticesReset();
        s_coopWorldBorrowed = borrow;
        s_stefxScratchBufferCount = borrow ? 1 : 2;
        s_stefxScratchIndex = 0;
        s_stefxScratchOffset = 0;
        g_SPXBScratchLayoutProof[0] = s_stefxScratchBufferCount;
        ++g_SPXBCoopWorldStorage[7];
        XBLog_WriteCriticalf("STEFX_COOP_WORLD_STORAGE: mode=%d borrowed=%d scratchBuffers=%d physicalBytes=%u",
            mode, borrow ? 1 : 0, s_stefxScratchBufferCount,
            2u * s_stefxScratchCapacityDwords * sizeof(DWORD));
    }
    g_SPXBCoopWorldStorage[0] = mode;
    g_SPXBCoopWorldStorage[1] = s_coopWorldBorrowed ? 1 : 0;
    g_SPXBCoopWorldStorage[2] = s_stefxScratchBufferCount;
    g_SPXBCoopWorldStorage[3] = s_stefxScratchReady ? 2u * s_stefxScratchCapacityDwords * sizeof(DWORD) : 0;
    g_SPXBCoopWorldStorage[4] = s_coopWorldBorrowed ? (unsigned int)s_stefxScratchBase[1] : 0;
    g_SPXBCoopWorldStorage[5] = STEFX_WORLD_ARRAY_CAPACITY * sizeof(stefxWorldVertex_t);
    g_SPXBCoopWorldStorage[6] = sizeof(s_worldVertexKeys) + sizeof(s_worldSpans) + sizeof(s_worldRemap) +
        sizeof(s_worldIndices) + sizeof(s_worldArrayRuns) + sizeof(s_worldArrayCounts) +
        sizeof(s_worldVertexOwners) + sizeof(s_worldArraySeen);
}
// STEFX_COOP_WORLD_CONFIGURE_END
#endif
#endif

// STEFX_WORLD_VERTICES_END

// STEFX_INTERLEAVED_VERTICES_BEGIN
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
struct stefxPackedLayout_t {
    unsigned int stride, normal, color, tex0, tex1; // byte offsets
};
extern "C" volatile unsigned int g_SPXBInterleavedVertices[8] = {0};
static cvar_t *s_interleavedCvar;
static cvar_t *s_interleavedVerifyCvar;

static stefxPackedLayout_t STEFX_PackedLayout(int normals, int tex0, int tex1)
{
    stefxPackedLayout_t l;
    l.normal = 12;
    l.color = 12 + (normals ? 12 : 0);
    l.tex0 = l.color + 4;
    l.tex1 = l.tex0 + (tex0 ? 8 : 0);
    l.stride = l.tex1 + (tex1 ? 8 : 0);
    return l;
}

static bool STEFX_InterleavedWanted(void)
{
    if (!s_interleavedCvar) {
        s_interleavedCvar = Cvar_Get("r_efInterleavedVertices", "0", 0);
        s_interleavedVerifyCvar = Cvar_Get("r_efInterleavedVerify", "0", 0);
    }
    return s_interleavedCvar->integer && !backEnd.projection2D &&
        backEnd.viewParms.stefxSplitView && tess.numVertexes > 0 &&
        tess.numVertexes <= SHADER_MAX_VERTEXES;
}

static void STEFX_PackInterleaved(DWORD *destination, const stefxPackedLayout_t &l,
                                int normals, int tex0, int tex1)
{
    unsigned char *p = (unsigned char *)destination;
    for (int v = 0; v < tess.numVertexes; ++v, p += l.stride) {
        // The GPU declaration reads FLOAT3; the fourth source component was
        // never consumed. Copy bits, including signed zero, without arithmetic.
        memcpy(p, tess.xyz[v], 12);
        if (normals) memcpy(p + l.normal, tess.normal[v], 12);
        const DWORD color = glw_state->colorArrayState ?
            tess.svars.colors[v] : glw_state->currentColor;
        memcpy(p + l.color, &color, 4);
        if (tex0) memcpy(p + l.tex0, tess.svars.texcoords[0][v], 8);
        if (tex1) memcpy(p + l.tex1, tess.svars.texcoords[1][v], 8);
    }
}

static bool STEFX_VerifyInterleaved(const DWORD *source, const stefxPackedLayout_t &l,
                                  int normals, int tex0, int tex1)
{
    const unsigned char *p = (const unsigned char *)source;
    for (int v = 0; v < tess.numVertexes; ++v, p += l.stride) {
        const DWORD color = glw_state->colorArrayState ?
            tess.svars.colors[v] : glw_state->currentColor;
        if (memcmp(p, tess.xyz[v], 12) ||
            (normals && memcmp(p + l.normal, tess.normal[v], 12)) ||
            memcmp(p + l.color, &color, 4) ||
            (tex0 && memcmp(p + l.tex0, tess.svars.texcoords[0][v], 8)) ||
            (tex1 && memcmp(p + l.tex1, tess.svars.texcoords[1][v], 8))) return false;
    }
    return true;
}
#endif
// STEFX_INTERLEAVED_VERTICES_END


#if !defined(FINAL_BUILD) && !defined(_XBOX_VC71_MIGRATION)
#include <d3d8perf.h>
#endif

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
typedef struct stefxShaderRuntimeKey_s
{
	unsigned int hash;
	unsigned int stateBits;
	unsigned int texture0;
	unsigned int texture1;
	short shaderIndex;
	byte pass;
	byte slot;
	byte enableMask;
	byte env0;
	byte env1;
	byte occupied;
} stefxShaderRuntimeKey_t;

#define STEFX_SHADER_RUNTIME_KEYS 4096
static stefxShaderRuntimeKey_t s_stefxShaderRuntimeKeys[STEFX_SHADER_RUNTIME_KEYS];
static unsigned int s_stefxShaderRuntimeKeyCount;
static unsigned int s_stefxShaderRuntimeDrawCount;
static unsigned int s_stefxShaderRuntimeMissingTextures;
static unsigned int s_stefxShaderRuntimeBindingMismatches;
static unsigned int s_stefxShaderRuntimeStateMismatches;
static unsigned int s_stefxShaderRuntimeViewportMismatches;
static unsigned int s_stefxShaderRuntimeResidencyMismatches;
static unsigned int s_stefxShaderRuntimeMatrixMismatches;
static unsigned int s_stefxShaderRuntimeInvalidIndices;
static unsigned int s_stefxShaderRuntimeNonFiniteValues;
static unsigned int s_stefxShaderRuntimeBlackIdentityColors;
static unsigned int s_stefxShaderRuntimePackedMismatches;
static unsigned int s_stefxShaderRuntimePushOverruns;
static unsigned int s_stefxShaderRuntimeNextSummary;
static qboolean s_stefxShaderRuntimeStarted;
static qboolean s_stefxShaderRuntimeTableFullLogged;
static cvar_t *s_stefxShaderTraceCvar;

static __forceinline qboolean STEFX_ShaderTraceEnabled( void )
{
	if ( !s_stefxShaderTraceCvar )
	{
		s_stefxShaderTraceCvar = Cvar_Get( "stefx_hm_shader_trace", "0", 0 );
		XBLog_WriteCritical(
			"STEFX_D3D8: shaderTraceHotPath=pointer-cached default=0" );
	}
	return s_stefxShaderTraceCvar && s_stefxShaderTraceCvar->integer
		? qtrue : qfalse;
}

typedef struct stefxShaderDataTrace_s
{
	qboolean active;
	unsigned int draw;
	int slot;
	int shaderIndex;
	int pass;
	int numVertexes;
	unsigned int xyzHash;
	unsigned int normalHash;
	unsigned int colorHash;
	unsigned int tex0Hash;
	unsigned int tex1Hash;
	unsigned int indexHash;
} stefxShaderDataTrace_t;

static stefxShaderDataTrace_t s_stefxShaderDataTrace;

static unsigned int STEFX_ShaderHashWords( const DWORD *words, unsigned int count )
{
	unsigned int hash = 2166136261u;
	unsigned int i;
	for ( i = 0; i < count; ++i )
	{
		hash = ( hash ^ (unsigned int)words[i] ) * 16777619u;
	}
	return hash;
}

static unsigned int STEFX_ShaderHashIndices( const GLushort *indices, unsigned int count )
{
	unsigned int hash = 2166136261u;
	unsigned int i;
	for ( i = 0; i < count; ++i )
	{
		hash = ( hash ^ (unsigned int)indices[i] ) * 16777619u;
	}
	return hash;
}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
/*
 * Measure exact, reusable world-geometry payloads across local viewports.
 * This is deliberately diagnostic-only: it hashes the data copied into the
 * native D3D8 push packet, but never changes submission or rendering.
 */
typedef struct stefxSplitReuseKey_s
{
	unsigned int hash;
	unsigned int xyzHash;
	unsigned int normalHash;
	unsigned int colorHash;
	unsigned int tex0Hash;
	unsigned int tex1Hash;
	unsigned int indexHash;
	unsigned int stateBits;
	unsigned int texture0;
	unsigned int texture1;
	unsigned int numVertexes;
	unsigned int numIndexes;
	unsigned int streamMask;
	unsigned int shaderIndex;
	unsigned int pass;
	unsigned int cullMode;
	unsigned int primitiveMode;
	unsigned int slotMask;
	unsigned int reserveDwords;
} stefxSplitReuseKey_t;

#define STEFX_SPLIT_REUSE_KEYS 1024
static stefxSplitReuseKey_t s_stefxSplitReuseKeys[STEFX_SPLIT_REUSE_KEYS];
static unsigned int s_stefxSplitReuseSampleSerial;

static void STEFX_RecordSplitWorldPayloadReuse( GLenum mode, GLsizei count,
	const GLushort *indices, qboolean normals, qboolean tex0, qboolean tex1,
	unsigned int reserveDwords )
{
	// Full payload hashes are a separate reuse experiment, not ordinary timing.
	// Keep their CPU cost out of draw-kind diagnosis unless explicitly requested.
	static cvar_t *reuseProbe;
	if (!reuseProbe) reuseProbe = Cvar_Get("r_efReuseProbe", "0", 0);
	if (!reuseProbe->integer) return;
	stefxSplitReuseKey_t candidate;
	unsigned __int64 start;
	unsigned int slotBit;
	unsigned int probe;
	unsigned int attempt;
	int slot;

	if ( !g_SPXBPerfSampleActive ||
		!backEnd.viewParms.stefxSplitThreePlusEconomy ||
		backEnd.currentEntity != &tr.worldEntity || !tess.shader ||
		count <= 0 || tess.numVertexes <= 0 )
	{
		return;
	}

	slot = backEnd.viewParms.stefxSplitSlot;
	if ( slot < 0 || slot > 3 )
	{
		return;
	}

	start = STEFX_XboxReadTsc();
	if ( s_stefxSplitReuseSampleSerial != (unsigned int)g_SPXBPerfSampleSerial )
	{
		s_stefxSplitReuseSampleSerial = (unsigned int)g_SPXBPerfSampleSerial;
		memset( s_stefxSplitReuseKeys, 0, sizeof(s_stefxSplitReuseKeys) );
	}

	memset( &candidate, 0, sizeof(candidate) );
	candidate.xyzHash = STEFX_ShaderHashWords(
		(const DWORD *)tess.xyz, (unsigned int)tess.numVertexes * 4u );
	candidate.normalHash = normals ? STEFX_ShaderHashWords(
		(const DWORD *)tess.normal, (unsigned int)tess.numVertexes * 4u ) : 0u;
	candidate.colorHash = glw_state->colorArrayState ? STEFX_ShaderHashWords(
		(const DWORD *)tess.svars.colors, (unsigned int)tess.numVertexes ) :
		(unsigned int)glw_state->currentColor;
	candidate.tex0Hash = tex0 ? STEFX_ShaderHashWords(
		(const DWORD *)tess.svars.texcoords[0], (unsigned int)tess.numVertexes * 2u ) : 0u;
	candidate.tex1Hash = tex1 ? STEFX_ShaderHashWords(
		(const DWORD *)tess.svars.texcoords[1], (unsigned int)tess.numVertexes * 2u ) : 0u;
	candidate.indexHash = STEFX_ShaderHashIndices( indices, (unsigned int)count );
	candidate.stateBits = (unsigned int)glState.glStateBits;
	candidate.texture0 = (unsigned int)glw_state->currentTexture[0];
	candidate.texture1 = (unsigned int)glw_state->currentTexture[1];
	candidate.numVertexes = (unsigned int)tess.numVertexes;
	candidate.numIndexes = (unsigned int)count;
	candidate.streamMask = ( normals ? 1u : 0u ) | ( tex0 ? 2u : 0u ) |
		( tex1 ? 4u : 0u ) | ( glw_state->colorArrayState ? 8u : 0u );
	candidate.shaderIndex = (unsigned int)tess.shader->index;
	candidate.pass = (unsigned int)tess.currentPass;
	candidate.cullMode = (unsigned int)glState.faceCulling;
	candidate.primitiveMode = (unsigned int)mode;
	candidate.reserveDwords = reserveDwords;

	candidate.hash = 2166136261u;
#define STEFX_REUSE_HASH(v) candidate.hash = ( candidate.hash ^ (unsigned int)(v) ) * 16777619u
	STEFX_REUSE_HASH( candidate.xyzHash );
	STEFX_REUSE_HASH( candidate.normalHash );
	STEFX_REUSE_HASH( candidate.colorHash );
	STEFX_REUSE_HASH( candidate.tex0Hash );
	STEFX_REUSE_HASH( candidate.tex1Hash );
	STEFX_REUSE_HASH( candidate.indexHash );
	STEFX_REUSE_HASH( candidate.stateBits );
	STEFX_REUSE_HASH( candidate.texture0 );
	STEFX_REUSE_HASH( candidate.texture1 );
	STEFX_REUSE_HASH( candidate.numVertexes );
	STEFX_REUSE_HASH( candidate.numIndexes );
	STEFX_REUSE_HASH( candidate.streamMask );
	STEFX_REUSE_HASH( candidate.shaderIndex );
	STEFX_REUSE_HASH( candidate.pass );
	STEFX_REUSE_HASH( candidate.cullMode );
	STEFX_REUSE_HASH( candidate.primitiveMode );
#undef STEFX_REUSE_HASH
	if ( !candidate.hash ) candidate.hash = 1u;

	++g_SPXBPerfReuseCandidatesCurrent;
	g_SPXBPerfReuseCandidateDwordsCurrent += reserveDwords;
	slotBit = 1u << (unsigned int)slot;
	probe = candidate.hash & ( STEFX_SPLIT_REUSE_KEYS - 1 );
	for ( attempt = 0; attempt < STEFX_SPLIT_REUSE_KEYS; ++attempt )
	{
		stefxSplitReuseKey_t *key = &s_stefxSplitReuseKeys[probe];
		if ( !key->hash )
		{
			candidate.slotMask = slotBit;
			*key = candidate;
			++g_SPXBPerfReuseUniqueCurrent;
			g_SPXBPerfReuseHashCyclesCurrent += STEFX_XboxElapsedCycles( start );
			return;
		}
		if ( key->hash == candidate.hash &&
			key->xyzHash == candidate.xyzHash &&
			key->normalHash == candidate.normalHash &&
			key->colorHash == candidate.colorHash &&
			key->tex0Hash == candidate.tex0Hash &&
			key->tex1Hash == candidate.tex1Hash &&
			key->indexHash == candidate.indexHash &&
			key->stateBits == candidate.stateBits &&
			key->texture0 == candidate.texture0 &&
			key->texture1 == candidate.texture1 &&
			key->numVertexes == candidate.numVertexes &&
			key->numIndexes == candidate.numIndexes &&
			key->streamMask == candidate.streamMask &&
			key->shaderIndex == candidate.shaderIndex &&
			key->pass == candidate.pass &&
			key->cullMode == candidate.cullMode &&
			key->primitiveMode == candidate.primitiveMode )
		{
			if ( !( key->slotMask & slotBit ) )
			{
				key->slotMask |= slotBit;
				++g_SPXBPerfReuseCrossViewHitsCurrent;
				g_SPXBPerfReuseCrossViewDwordsCurrent += reserveDwords;
			}
			g_SPXBPerfReuseHashCyclesCurrent += STEFX_XboxElapsedCycles( start );
			return;
		}
		probe = ( probe + 1 ) & ( STEFX_SPLIT_REUSE_KEYS - 1 );
	}

	++g_SPXBPerfReuseTableFullCurrent;
	g_SPXBPerfReuseHashCyclesCurrent += STEFX_XboxElapsedCycles( start );
}
#endif

static qboolean STEFX_ShaderFloatBitsNonFinite( DWORD bits )
{
	return ( bits & 0x7f800000u ) == 0x7f800000u;
}

static unsigned int STEFX_ShaderRuntimeHash( int slot, int shaderIndex, int pass,
	unsigned int stateBits, unsigned int texture0, unsigned int texture1,
	unsigned int enableMask, unsigned int env0, unsigned int env1 )
{
	unsigned int hash = 2166136261u;
#define STEFX_SHADER_HASH_VALUE(v) hash = ( hash ^ (unsigned int)(v) ) * 16777619u
	STEFX_SHADER_HASH_VALUE( slot );
	STEFX_SHADER_HASH_VALUE( shaderIndex );
	STEFX_SHADER_HASH_VALUE( pass );
	STEFX_SHADER_HASH_VALUE( stateBits );
	STEFX_SHADER_HASH_VALUE( texture0 );
	STEFX_SHADER_HASH_VALUE( texture1 );
	STEFX_SHADER_HASH_VALUE( enableMask );
	STEFX_SHADER_HASH_VALUE( env0 );
	STEFX_SHADER_HASH_VALUE( env1 );
#undef STEFX_SHADER_HASH_VALUE
	return hash ? hash : 1u;
}

static qboolean STEFX_ShaderRuntimeFirstCombination( int slot, int shaderIndex, int pass,
	unsigned int stateBits, unsigned int texture0, unsigned int texture1,
	unsigned int enableMask, unsigned int env0, unsigned int env1 )
{
	unsigned int hash = STEFX_ShaderRuntimeHash( slot, shaderIndex, pass, stateBits,
		texture0, texture1, enableMask, env0, env1 );
	unsigned int probe = hash & ( STEFX_SHADER_RUNTIME_KEYS - 1 );
	unsigned int attempt;

	for ( attempt = 0; attempt < STEFX_SHADER_RUNTIME_KEYS; ++attempt )
	{
		stefxShaderRuntimeKey_t *key = &s_stefxShaderRuntimeKeys[probe];
		if ( !key->occupied )
		{
			key->hash = hash;
			key->stateBits = stateBits;
			key->texture0 = texture0;
			key->texture1 = texture1;
			key->shaderIndex = (short)shaderIndex;
			key->pass = (byte)pass;
			key->slot = (byte)slot;
			key->enableMask = (byte)enableMask;
			key->env0 = (byte)env0;
			key->env1 = (byte)env1;
			key->occupied = 1;
			++s_stefxShaderRuntimeKeyCount;
			return qtrue;
		}
		if ( key->hash == hash && key->stateBits == stateBits &&
			key->texture0 == texture0 && key->texture1 == texture1 &&
			key->shaderIndex == shaderIndex && key->pass == pass && key->slot == slot &&
			key->enableMask == enableMask && key->env0 == env0 && key->env1 == env1 )
		{
			return qfalse;
		}
		probe = ( probe + 1 ) & ( STEFX_SHADER_RUNTIME_KEYS - 1 );
	}

	if ( !s_stefxShaderRuntimeTableFullLogged )
	{
		s_stefxShaderRuntimeTableFullLogged = qtrue;
		XBLog_WriteCritical( "STEFX_HM_SHADER_TRACE_ERROR: runtime combination table full" );
	}
	return qfalse;
}

static void STEFX_TraceShaderRuntimeDraw( GLsizei count, const GLushort *indices,
	qboolean normals, qboolean tex0, qboolean tex1 )
{
	shader_t *shader = tess.shader;
	int slot = backEnd.viewParms.stefxSplitSlot;
	int pass = tess.currentPass;
	unsigned int enableMask = 0;
	unsigned int anomalyMask = 0;
	unsigned int texture0 = glw_state->currentTexture[0];
	unsigned int texture1 = glw_state->currentTexture[1];
	IDirect3DBaseTexture8 *actualTextures[GLW_MAX_TEXTURE_STAGES];
	IDirect3DBaseTexture8 *expectedTextures[GLW_MAX_TEXTURE_STAGES];
	DWORD colorOps[GLW_MAX_TEXTURE_STAGES];
	DWORD alphaOps[GLW_MAX_TEXTURE_STAGES];
	DWORD colorArg1[GLW_MAX_TEXTURE_STAGES];
	DWORD colorArg2[GLW_MAX_TEXTURE_STAGES];
	DWORD alphaArg1[GLW_MAX_TEXTURE_STAGES];
	DWORD alphaArg2[GLW_MAX_TEXTURE_STAGES];
	DWORD texCoordIndex[GLW_MAX_TEXTURE_STAGES];
	DWORD textureTransform[GLW_MAX_TEXTURE_STAGES];
	DWORD textureSizes[GLW_MAX_TEXTURE_STAGES];
	DWORD textureData[GLW_MAX_TEXTURE_STAGES];
	int textureFileOffsets[GLW_MAX_TEXTURE_STAGES];
	D3DXMATRIX actualProjection;
	D3DXMATRIX actualView;
	D3DXMATRIX actualTexture0;
	D3DXMATRIX actualTexture1;
	D3DVIEWPORT8 actualViewport;
	DWORD colorWrite = 0;
	DWORD alphaBlend = 0;
	DWORD srcBlend = 0;
	DWORD dstBlend = 0;
	DWORD zEnable = 0;
	DWORD zWrite = 0;
	DWORD zFunc = 0;
	DWORD cullMode = 0;
	DWORD vertexShader = 0;
	DWORD pixelShader = 0;
	unsigned int maxIndex = 0;
	unsigned int invalidIndices = 0;
	unsigned int nonFiniteValues = 0;
	unsigned int blackRgb = 0;
	unsigned int residentMask = 0;
	unsigned int matrixMismatch = 0;
	unsigned int expectedTexCoord;
	shaderStage_t *shaderStage = NULL;
	qboolean blackIdentityError = qfalse;
	int stage;
	int i;

	if ( !STEFX_ShaderTraceEnabled() || !shader ||
		slot < 0 || slot > 3 )
	{
		return;
	}
	memset( &s_stefxShaderDataTrace, 0, sizeof(s_stefxShaderDataTrace) );

	if ( !s_stefxShaderRuntimeStarted )
	{
		s_stefxShaderRuntimeStarted = qtrue;
		s_stefxShaderRuntimeNextSummary = (unsigned int)backEnd.refdef.time + 5000u;
		XBLog_WriteCriticalf( "STEFX_HM_SHADER_RUNTIME_BEGIN: path='%s' stages=%d table=%d",
			XBLog_GetPath() ? XBLog_GetPath() : "<none>", GLW_MAX_TEXTURE_STAGES,
			STEFX_SHADER_RUNTIME_KEYS );
	}

	++s_stefxShaderRuntimeDrawCount;
	memset( actualTextures, 0, sizeof(actualTextures) );
	memset( expectedTextures, 0, sizeof(expectedTextures) );
	memset( colorOps, 0, sizeof(colorOps) );
	memset( alphaOps, 0, sizeof(alphaOps) );
	memset( colorArg1, 0, sizeof(colorArg1) );
	memset( colorArg2, 0, sizeof(colorArg2) );
	memset( alphaArg1, 0, sizeof(alphaArg1) );
	memset( alphaArg2, 0, sizeof(alphaArg2) );
	memset( texCoordIndex, 0, sizeof(texCoordIndex) );
	memset( textureTransform, 0, sizeof(textureTransform) );
	memset( textureSizes, 0, sizeof(textureSizes) );
	memset( textureData, 0, sizeof(textureData) );
	memset( textureFileOffsets, 0xff, sizeof(textureFileOffsets) );
	memset( &actualViewport, 0, sizeof(actualViewport) );
	memset( &actualProjection, 0, sizeof(actualProjection) );
	memset( &actualView, 0, sizeof(actualView) );
	memset( &actualTexture0, 0, sizeof(actualTexture0) );
	memset( &actualTexture1, 0, sizeof(actualTexture1) );

	s_stefxShaderDataTrace.active = qtrue;
	s_stefxShaderDataTrace.draw = s_stefxShaderRuntimeDrawCount;
	s_stefxShaderDataTrace.slot = slot;
	s_stefxShaderDataTrace.shaderIndex = shader->index;
	s_stefxShaderDataTrace.pass = pass;
	s_stefxShaderDataTrace.numVertexes = tess.numVertexes;
	s_stefxShaderDataTrace.xyzHash = STEFX_ShaderHashWords(
		(const DWORD *)tess.xyz, (unsigned int)tess.numVertexes * 4u );
	s_stefxShaderDataTrace.normalHash = normals ? STEFX_ShaderHashWords(
		(const DWORD *)tess.normal, (unsigned int)tess.numVertexes * 4u ) : 0u;
	if ( glw_state->colorArrayState )
	{
		s_stefxShaderDataTrace.colorHash = STEFX_ShaderHashWords(
			(const DWORD *)tess.svars.colors, (unsigned int)tess.numVertexes );
	}
	else
	{
		unsigned int hash = 2166136261u;
		for ( i = 0; i < tess.numVertexes; ++i )
		{
			hash = ( hash ^ (unsigned int)glw_state->currentColor ) * 16777619u;
		}
		s_stefxShaderDataTrace.colorHash = hash;
	}
	s_stefxShaderDataTrace.tex0Hash = tex0 ? STEFX_ShaderHashWords(
		(const DWORD *)tess.svars.texcoords[0], (unsigned int)tess.numVertexes * 2u ) : 0u;
	s_stefxShaderDataTrace.tex1Hash = tex1 ? STEFX_ShaderHashWords(
		(const DWORD *)tess.svars.texcoords[1], (unsigned int)tess.numVertexes * 2u ) : 0u;
	s_stefxShaderDataTrace.indexHash = STEFX_ShaderHashIndices( indices, (unsigned int)count );

	for ( i = 0; i < tess.numVertexes; ++i )
	{
		const DWORD *xyzBits = (const DWORD *)&tess.xyz[i][0];
		const DWORD color = glw_state->colorArrayState ?
			(DWORD)tess.svars.colors[i] : (DWORD)glw_state->currentColor;
		int component;
		for ( component = 0; component < 4; ++component )
		{
			if ( STEFX_ShaderFloatBitsNonFinite( xyzBits[component] ) ) ++nonFiniteValues;
		}
		if ( normals )
		{
			const DWORD *normalBits = (const DWORD *)&tess.normal[i][0];
			for ( component = 0; component < 4; ++component )
			{
				if ( STEFX_ShaderFloatBitsNonFinite( normalBits[component] ) ) ++nonFiniteValues;
			}
		}
		if ( tex0 )
		{
			const DWORD *texBits = (const DWORD *)&tess.svars.texcoords[0][i][0];
			if ( STEFX_ShaderFloatBitsNonFinite( texBits[0] ) ) ++nonFiniteValues;
			if ( STEFX_ShaderFloatBitsNonFinite( texBits[1] ) ) ++nonFiniteValues;
		}
		if ( tex1 )
		{
			const DWORD *texBits = (const DWORD *)&tess.svars.texcoords[1][i][0];
			if ( STEFX_ShaderFloatBitsNonFinite( texBits[0] ) ) ++nonFiniteValues;
			if ( STEFX_ShaderFloatBitsNonFinite( texBits[1] ) ) ++nonFiniteValues;
		}
		if ( ( color & 0x00ffffffu ) == 0u ) ++blackRgb;
	}
	for ( i = 0; i < count; ++i )
	{
		const unsigned int index = (unsigned int)indices[i];
		if ( index > maxIndex ) maxIndex = index;
		if ( index >= (unsigned int)tess.numVertexes ) ++invalidIndices;
	}

	if ( pass >= 0 && pass < shader->numUnfoggedPasses )
	{
		shaderStage = &shader->stages[pass];
	}
	if ( shaderStage && blackRgb == (unsigned int)tess.numVertexes && tess.numVertexes > 0 &&
		!tess.fading && shaderStage->adjustColorsForFog == ACFF_NONE &&
		( shaderStage->rgbGen == CGEN_IDENTITY ||
			shaderStage->rgbGen == CGEN_IDENTITY_LIGHTING ) &&
		!( glState.glStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
		backEnd.currentEntity &&
		!( backEnd.currentEntity->e.renderfx &
			( RF_DISINTEGRATE1 | RF_DISINTEGRATE2 | RF_VOLUMETRIC | RF_RGB_TINT ) ) )
	{
		blackIdentityError = qtrue;
	}
	if ( invalidIndices || nonFiniteValues || blackIdentityError )
	{
		s_stefxShaderRuntimeInvalidIndices += invalidIndices;
		s_stefxShaderRuntimeNonFiniteValues += nonFiniteValues;
		if ( blackIdentityError ) ++s_stefxShaderRuntimeBlackIdentityColors;
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_ERROR: draw=%u slot=%d shader=%d name='%s' pass=%d data invalidIndices=%u maxIndex=%u verts=%d nonFinite=%u blackRgb=%u rgbGen=%d fogAdjust=%d fading=%d hashes=%08x/%08x/%08x/%08x/%08x/%08x",
			s_stefxShaderRuntimeDrawCount, slot, shader->index, shader->name, pass,
			invalidIndices, maxIndex, tess.numVertexes, nonFiniteValues, blackRgb,
			shaderStage ? (int)shaderStage->rgbGen : -1,
			shaderStage ? (int)shaderStage->adjustColorsForFog : -1,
			tess.fading ? 1 : 0, s_stefxShaderDataTrace.xyzHash,
			s_stefxShaderDataTrace.normalHash, s_stefxShaderDataTrace.colorHash,
			s_stefxShaderDataTrace.tex0Hash, s_stefxShaderDataTrace.tex1Hash,
			s_stefxShaderDataTrace.indexHash );
	}

	for ( stage = 0; stage < GLW_MAX_TEXTURE_STAGES; ++stage )
	{
		const qboolean shouldBind = glw_state->textureStageEnable[stage] &&
			glw_state->currentTexture[stage];
		glwstate_t::texturexlat_t::iterator found;
		if ( glw_state->textureStageEnable[stage] )
		{
			enableMask |= 1u << stage;
		}
		found = glw_state->textureXlat.find( glw_state->currentTexture[stage] );
		if ( shouldBind && found == glw_state->textureXlat.end() )
		{
			anomalyMask |= 1u << stage;
			++s_stefxShaderRuntimeMissingTextures;
		}
		else if ( shouldBind )
		{
			expectedTextures[stage] = found->second.mipmap;
			textureSizes[stage] = found->second.size;
			textureData[stage] = (DWORD)found->second.data;
			textureFileOffsets[stage] = found->second.fileOffset;
			if ( found->second.inMemory )
			{
				residentMask |= 1u << stage;
			}
			else
			{
				anomalyMask |= 1u << ( 20 + stage );
				++s_stefxShaderRuntimeResidencyMismatches;
			}
		}
		glw_state->device->GetTexture( stage, &actualTextures[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_COLOROP, &colorOps[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_ALPHAOP, &alphaOps[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_COLORARG1, &colorArg1[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_COLORARG2, &colorArg2[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_ALPHAARG1, &alphaArg1[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_ALPHAARG2, &alphaArg2[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_TEXCOORDINDEX, &texCoordIndex[stage] );
		glw_state->device->GetTextureStageState( stage, D3DTSS_TEXTURETRANSFORMFLAGS,
			&textureTransform[stage] );
		if ( actualTextures[stage] != expectedTextures[stage] )
		{
			anomalyMask |= 1u << ( 4 + stage );
			++s_stefxShaderRuntimeBindingMismatches;
		}
		if ( shouldBind )
		{
			expectedTexCoord = (unsigned int)stage;
			if ( shaderStage && shaderStage->isEnvironment )
			{
				expectedTexCoord |= D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
			}
			if ( colorOps[stage] != (DWORD)glw_state->textureEnv[stage] ||
				alphaOps[stage] != (DWORD)glw_state->textureEnv[stage] ||
				colorArg1[stage] != D3DTA_TEXTURE || colorArg2[stage] != D3DTA_CURRENT ||
				alphaArg1[stage] != D3DTA_TEXTURE || alphaArg2[stage] != D3DTA_CURRENT ||
				texCoordIndex[stage] != expectedTexCoord ||
				textureTransform[stage] != D3DTTFF_COUNT2 )
			{
				anomalyMask |= 1u << ( 8 + stage );
				++s_stefxShaderRuntimeStateMismatches;
			}
		}
		else if ( colorOps[stage] != D3DTOP_DISABLE || alphaOps[stage] != D3DTOP_DISABLE )
		{
			anomalyMask |= 1u << ( 8 + stage );
			++s_stefxShaderRuntimeStateMismatches;
		}
	}

	glw_state->device->GetViewport( &actualViewport );
	if ( actualViewport.X != glw_state->viewport.X || actualViewport.Y != glw_state->viewport.Y ||
		actualViewport.Width != glw_state->viewport.Width ||
		actualViewport.Height != glw_state->viewport.Height )
	{
		anomalyMask |= 1u << 16;
		++s_stefxShaderRuntimeViewportMismatches;
	}
	glw_state->device->GetTransform( D3DTS_PROJECTION, &actualProjection );
	glw_state->device->GetTransform( D3DTS_VIEW, &actualView );
	glw_state->device->GetTransform( D3DTS_TEXTURE0, &actualTexture0 );
	glw_state->device->GetTransform( D3DTS_TEXTURE1, &actualTexture1 );
	if ( memcmp( &actualProjection,
		glw_state->matrixStack[glwstate_t::MatrixMode_Projection]->GetTop(),
		sizeof(actualProjection) ) != 0 ) matrixMismatch |= 1u;
	if ( memcmp( &actualView,
		glw_state->matrixStack[glwstate_t::MatrixMode_Model]->GetTop(),
		sizeof(actualView) ) != 0 ) matrixMismatch |= 2u;
	if ( memcmp( &actualTexture0,
		glw_state->matrixStack[glwstate_t::MatrixMode_Texture0]->GetTop(),
		sizeof(actualTexture0) ) != 0 ) matrixMismatch |= 4u;
	if ( memcmp( &actualTexture1,
		glw_state->matrixStack[glwstate_t::MatrixMode_Texture1]->GetTop(),
		sizeof(actualTexture1) ) != 0 ) matrixMismatch |= 8u;
	if ( matrixMismatch )
	{
		anomalyMask |= 1u << 17;
		++s_stefxShaderRuntimeMatrixMismatches;
	}
	glw_state->device->GetRenderState( D3DRS_COLORWRITEENABLE, &colorWrite );
	glw_state->device->GetRenderState( D3DRS_ALPHABLENDENABLE, &alphaBlend );
	glw_state->device->GetRenderState( D3DRS_SRCBLEND, &srcBlend );
	glw_state->device->GetRenderState( D3DRS_DESTBLEND, &dstBlend );
	glw_state->device->GetRenderState( D3DRS_ZENABLE, &zEnable );
	glw_state->device->GetRenderState( D3DRS_ZWRITEENABLE, &zWrite );
	glw_state->device->GetRenderState( D3DRS_ZFUNC, &zFunc );
	glw_state->device->GetRenderState( D3DRS_CULLMODE, &cullMode );
	glw_state->device->GetVertexShader( &vertexShader );
	glw_state->device->GetPixelShader( &pixelShader );

	if ( STEFX_ShaderRuntimeFirstCombination( slot, shader->index, pass,
		(unsigned int)glState.glStateBits, texture0, texture1, enableMask,
		(unsigned int)glw_state->textureEnv[0], (unsigned int)glw_state->textureEnv[1] ) )
	{
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_DRAW: draw=%u slot=%d shader=%d name='%s' pass=%d indexes=%d verts=%d state=0x%08x enable=0x%x dirty=0x%x tex=%u,%u resident=0x%x texMeta=%u@%08x/%d,%u@%08x/%d expected=0x%08x,0x%08x actual=0x%08x,0x%08x env=%u,%u op=%u/%u,%u/%u args=%u/%u/%u/%u,%u/%u/%u/%u tc=%u/%u xform=%u/%u data=%08x/%08x/%08x/%08x/%08x/%08x maxIndex=%u blackRgb=%u nonFinite=%u viewport=%u,%u,%ux%u scissor=%d:%d,%d-%d,%d colorWrite=0x%x blend=%u/%u/%u z=%u/%u/%u cull=%u vs=0x%x ps=0x%x matrix=0x%x anomaly=0x%x",
			s_stefxShaderRuntimeDrawCount, slot, shader->index, shader->name, pass,
			(int)count, tess.numVertexes, (unsigned int)glState.glStateBits, enableMask,
			(glw_state->textureStageDirty[0] ? 1u : 0u) |
				(glw_state->textureStageDirty[1] ? 2u : 0u), texture0, texture1,
			residentMask, (unsigned int)textureSizes[0], (unsigned int)textureData[0],
			textureFileOffsets[0], (unsigned int)textureSizes[1],
			(unsigned int)textureData[1], textureFileOffsets[1],
			(unsigned int)expectedTextures[0], (unsigned int)expectedTextures[1],
			(unsigned int)actualTextures[0], (unsigned int)actualTextures[1],
			(unsigned int)glw_state->textureEnv[0], (unsigned int)glw_state->textureEnv[1],
			(unsigned int)colorOps[0], (unsigned int)alphaOps[0],
			(unsigned int)colorOps[1], (unsigned int)alphaOps[1],
			(unsigned int)colorArg1[0], (unsigned int)colorArg2[0],
			(unsigned int)alphaArg1[0], (unsigned int)alphaArg2[0],
			(unsigned int)colorArg1[1], (unsigned int)colorArg2[1],
			(unsigned int)alphaArg1[1], (unsigned int)alphaArg2[1],
			(unsigned int)texCoordIndex[0], (unsigned int)texCoordIndex[1],
			(unsigned int)textureTransform[0], (unsigned int)textureTransform[1],
			s_stefxShaderDataTrace.xyzHash, s_stefxShaderDataTrace.normalHash,
			s_stefxShaderDataTrace.colorHash, s_stefxShaderDataTrace.tex0Hash,
			s_stefxShaderDataTrace.tex1Hash, s_stefxShaderDataTrace.indexHash,
			maxIndex, blackRgb, nonFiniteValues,
			(unsigned int)actualViewport.X, (unsigned int)actualViewport.Y,
			(unsigned int)actualViewport.Width, (unsigned int)actualViewport.Height,
			glw_state->scissorEnable ? 1 : 0, glw_state->scissorBox.x1,
			glw_state->scissorBox.y1, glw_state->scissorBox.x2, glw_state->scissorBox.y2,
			(unsigned int)colorWrite, (unsigned int)alphaBlend, (unsigned int)srcBlend,
			(unsigned int)dstBlend, (unsigned int)zEnable, (unsigned int)zWrite,
			(unsigned int)zFunc, (unsigned int)cullMode, (unsigned int)vertexShader,
			(unsigned int)pixelShader, matrixMismatch, anomalyMask );
	}

	if ( anomalyMask )
	{
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_ERROR: draw=%u slot=%d shader=%d name='%s' pass=%d anomaly=0x%x enable=0x%x tex=%u,%u resident=0x%x texMeta=%u@%08x/%d,%u@%08x/%d expected=0x%08x,0x%08x actual=0x%08x,0x%08x env=%u,%u op=%u/%u,%u/%u args=%u/%u/%u/%u,%u/%u/%u/%u tc=%u/%u xform=%u/%u matrix=0x%x viewport=%u,%u,%ux%u cachedViewport=%u,%u,%ux%u",
			s_stefxShaderRuntimeDrawCount, slot, shader->index, shader->name, pass,
			anomalyMask, enableMask, texture0, texture1,
			residentMask, (unsigned int)textureSizes[0], (unsigned int)textureData[0],
			textureFileOffsets[0], (unsigned int)textureSizes[1],
			(unsigned int)textureData[1], textureFileOffsets[1],
			(unsigned int)expectedTextures[0], (unsigned int)expectedTextures[1],
			(unsigned int)actualTextures[0], (unsigned int)actualTextures[1],
			(unsigned int)glw_state->textureEnv[0], (unsigned int)glw_state->textureEnv[1],
			(unsigned int)colorOps[0], (unsigned int)alphaOps[0],
			(unsigned int)colorOps[1], (unsigned int)alphaOps[1],
			(unsigned int)colorArg1[0], (unsigned int)colorArg2[0],
			(unsigned int)alphaArg1[0], (unsigned int)alphaArg2[0],
			(unsigned int)colorArg1[1], (unsigned int)colorArg2[1],
			(unsigned int)alphaArg1[1], (unsigned int)alphaArg2[1],
			(unsigned int)texCoordIndex[0], (unsigned int)texCoordIndex[1],
			(unsigned int)textureTransform[0], (unsigned int)textureTransform[1],
			matrixMismatch,
			(unsigned int)actualViewport.X, (unsigned int)actualViewport.Y,
			(unsigned int)actualViewport.Width, (unsigned int)actualViewport.Height,
			(unsigned int)glw_state->viewport.X, (unsigned int)glw_state->viewport.Y,
			(unsigned int)glw_state->viewport.Width, (unsigned int)glw_state->viewport.Height );
	}

	if ( (int)((unsigned int)backEnd.refdef.time - s_stefxShaderRuntimeNextSummary) >= 0 )
	{
		s_stefxShaderRuntimeNextSummary = (unsigned int)backEnd.refdef.time + 5000u;
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_SUMMARY: time=%d draws=%u combinations=%u missingTextures=%u bindingMismatch=%u residencyMismatch=%u stageStateMismatch=%u viewportMismatch=%u matrixMismatch=%u invalidIndices=%u nonFinite=%u blackIdentity=%u packedMismatch=%u pushOverrun=%u staticTex=%lu/%lu skinTex=%lu/%lu",
			backEnd.refdef.time, s_stefxShaderRuntimeDrawCount, s_stefxShaderRuntimeKeyCount,
			s_stefxShaderRuntimeMissingTextures, s_stefxShaderRuntimeBindingMismatches,
			s_stefxShaderRuntimeResidencyMismatches, s_stefxShaderRuntimeStateMismatches,
			s_stefxShaderRuntimeViewportMismatches, s_stefxShaderRuntimeMatrixMismatches,
			s_stefxShaderRuntimeInvalidIndices, s_stefxShaderRuntimeNonFiniteValues,
			s_stefxShaderRuntimeBlackIdentityColors, s_stefxShaderRuntimePackedMismatches,
			s_stefxShaderRuntimePushOverruns, gStaticTextures.Size(), gStaticTextures.Capacity(),
			gSkinTextures.Size(), gSkinTextures.Capacity() );
	}
	for ( stage = 0; stage < GLW_MAX_TEXTURE_STAGES; ++stage )
	{
		if ( actualTextures[stage] ) actualTextures[stage]->Release();
	}
}

static void STEFX_TraceShaderPackedDraw( const DWORD *stream, qboolean normals,
	qboolean tex0, qboolean tex1, unsigned int reservedDwords, unsigned int usedDwords )
{
	const stefxShaderDataTrace_t *trace = &s_stefxShaderDataTrace;
	const DWORD *cursor = stream;
	unsigned int xyzHash;
	unsigned int normalHash = 0;
	unsigned int colorHash;
	unsigned int tex0Hash = 0;
	unsigned int tex1Hash = 0;
	unsigned int mismatch = 0;
	const unsigned int verts = (unsigned int)trace->numVertexes;

	if ( !trace->active ) return;

	xyzHash = STEFX_ShaderHashWords( cursor, verts * 4u );
	cursor += verts * 4u;
	if ( normals )
	{
		normalHash = STEFX_ShaderHashWords( cursor, verts * 4u );
		cursor += verts * 4u;
	}
	colorHash = STEFX_ShaderHashWords( cursor, verts );
	cursor += verts;
	if ( tex0 )
	{
		tex0Hash = STEFX_ShaderHashWords( cursor, verts * 2u );
		cursor += verts * 2u;
	}
	if ( tex1 )
	{
		tex1Hash = STEFX_ShaderHashWords( cursor, verts * 2u );
	}

	if ( xyzHash != trace->xyzHash ) mismatch |= 1u;
	if ( normalHash != trace->normalHash ) mismatch |= 2u;
	if ( colorHash != trace->colorHash ) mismatch |= 4u;
	if ( tex0Hash != trace->tex0Hash ) mismatch |= 8u;
	if ( tex1Hash != trace->tex1Hash ) mismatch |= 16u;
	if ( usedDwords > reservedDwords )
	{
		mismatch |= 32u;
		++s_stefxShaderRuntimePushOverruns;
	}
	if ( mismatch )
	{
		++s_stefxShaderRuntimePackedMismatches;
		XBLog_WriteCriticalf(
			"STEFX_HM_SHADER_TRACE_ERROR: draw=%u slot=%d shader=%d pass=%d packed mismatch=0x%x reserved=%u used=%u source=%08x/%08x/%08x/%08x/%08x packed=%08x/%08x/%08x/%08x/%08x",
			trace->draw, trace->slot, trace->shaderIndex, trace->pass, mismatch,
			reservedDwords, usedDwords, trace->xyzHash, trace->normalHash,
			trace->colorHash, trace->tex0Hash, trace->tex1Hash,
			xyzHash, normalHash, colorHash, tex0Hash, tex1Hash );
	}
	s_stefxShaderDataTrace.active = qfalse;
}
#endif

#endif

#ifdef _XBOX
extern "C" volatile unsigned int g_SPXBNativeSubmitStage;
extern "C" volatile unsigned int g_SPXBNativeSubmitCount;
extern "C" volatile unsigned int g_SPXBNativeSubmitVerts;
extern "C" volatile unsigned int g_SPXBNativeSubmitState;
extern "C" volatile unsigned int g_SPXBNativeSubmitStreams;
extern "C" volatile unsigned int g_SPXBNativeSubmitReserve;
extern "C" volatile unsigned int g_SPXBNativeSubmitSerial;
#endif

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBHMScoreSubmitArmed;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitCalls;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitIndexes;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitState;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitTexture;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissor;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissorXY;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitScissorWH;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitTarget;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitColorWrite;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitCull;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitBlend;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitViewportXY;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitViewportWH;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitProj00;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitProj11;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitProj30;
extern "C" volatile unsigned int g_SPXBHMScoreSubmitProj31;
extern "C" volatile unsigned int g_SPXBHMScoreTextureData0;
extern "C" volatile unsigned int g_SPXBHMScoreTextureSize;
extern "C" volatile unsigned int g_SPXBHMScoreTextureWH;
extern "C" volatile unsigned int g_SPXBHMScoreTextureFormat;
extern "C" volatile unsigned int g_SPXBHMScoreStageColor;
extern "C" volatile unsigned int g_SPXBHMScoreStageAlpha;
extern "C" volatile unsigned int g_SPXBHMScoreDepthState;
extern "C" volatile unsigned int g_SPXBHMScoreVertexShader;
extern "C" volatile unsigned int g_SPXBHMScorePixelShader;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0X;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0Y;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0Z;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0W;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0Color;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0U;
extern "C" volatile unsigned int g_SPXBHMScoreVertex0V;
extern "C" volatile unsigned int g_SPXBHMScoreVertexCount;
extern "C" volatile unsigned int g_SPXBHMScoreVertexStride;
extern "C" volatile unsigned int g_SPXBHMScoreIndex012;
#endif

// Global texture allocators:
StaticTextureAllocator		gStaticTextures;
SwappingTextureAllocator	gSkinTextures;
static bool					s_bUseSkinAllocator = false;
static bool					s_texturePoolMemoryReserved = false;
static bool					s_texturePoolsInitialized = false;
bool connectSwapOverride = false;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
static unsigned __int64 s_xboxImmediateDrawStart = 0;
#endif

void ReserveRetailTexturePoolsEarly( void )
{
	if (!s_texturePoolMemoryReserved)
	{
		gStaticTextures.Reserve( 10 * 1024 * 1024 );
#if defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
		gSkinTextures.Reserve( 2 * 1024 * 1024 );
#else
		gSkinTextures.Reserve( 4 * 1024 * 1024 );
#endif
		s_texturePoolMemoryReserved = true;
	}
}

static void InitializeRetailTexturePoolsDeferred( void )
{
	OutputDebugStringA("JA: initializing retail texture pools after CRT startup\n");
	ReserveRetailTexturePoolsEarly();
	if (!s_texturePoolsInitialized)
	{
		gStaticTextures.Reset();
		gSkinTextures.Reset();
		s_texturePoolsInitialized = true;
	}
	else
	{
		gStaticTextures.Reset();
		gSkinTextures.Reset();
	}
	OutputDebugStringA("JA: retail texture pools ready\n");
}

void BeginSkinTextures( void )
{
	s_bUseSkinAllocator = true;
}

void EndSkinTextures( void )
{
	s_bUseSkinAllocator = false;
}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" unsigned int STEFX_SkinTextureSwapCount( void )
{
	return gSkinTextures.SwapCount();
}

extern "C" unsigned int STEFX_SkinTextureFetchCount( void )
{
	return gSkinTextures.FetchCount();
}

extern "C" unsigned int STEFX_SkinTextureWaitCount( void )
{
	return gSkinTextures.WaitCount();
}

extern "C" unsigned int STEFX_SkinTextureBytesWritten( void )
{
	return gSkinTextures.BytesWritten();
}

extern "C" unsigned int STEFX_SkinTextureBytesRead( void )
{
	return gSkinTextures.BytesRead();
}

extern "C" unsigned int STEFX_StaticTextureUsed( void )
{
	return gStaticTextures.Size();
}

extern "C" unsigned int STEFX_StaticTextureCapacity( void )
{
	return gStaticTextures.Capacity();
}

extern "C" unsigned int STEFX_SkinTextureUsed( void )
{
	return gSkinTextures.Size();
}

extern "C" void STEFX_ReportTexturePools( void )
{
	XBLog_WriteCriticalf("STEFX_HW_TEXTURE_POOLS: staticUsed=%u staticCap=%u skinUsed=%u skinCap=%u skinPeak=%u swaps=%u fetches=%u readBytes=%u writeBytes=%u",
		gStaticTextures.Size(), gStaticTextures.Capacity(), gSkinTextures.Size(),
		gSkinTextures.Capacity(), gSkinTextures.Peak(), gSkinTextures.SwapCount(),
		gSkinTextures.FetchCount(), gSkinTextures.BytesRead(), gSkinTextures.BytesWritten());
}

extern "C" unsigned int STEFX_SkinTextureCapacity( void )
{
	return gSkinTextures.Capacity();
}
#endif

#include <vector>

extern void Z_SetNewDeleteTemporary(bool);

#define GLW_USE_TRI_STRIPS 1

#ifdef _XBOX
#define GLW_MAX_DRAW_PACKET_SIZE 2040
#else
#define GLW_MAX_DRAW_PACKET_SIZE (SHADER_MAX_VERTEXES*12)
#endif

#define MEMORY_PROFILE 1

int texMemSize = 0;

#if MEMORY_PROFILE
 
static int getTexMemSize(IDirect3DTexture8* mipmap)
{
	int levels = mipmap->GetLevelCount();
	int size = 0;
	while (levels--)
	{ 
		D3DSURFACE_DESC desc;
		mipmap->GetLevelDesc(levels, &desc);
		size += desc.Size;
	}
	return size;
}
#endif

void QGL_EnableLogging( qboolean enable );

void ( * qglAccum )(GLenum op, GLfloat value);
void ( * qglAlphaFunc )(GLenum func, GLclampf ref);
GLboolean ( * qglAreTexturesResident )(GLsizei n, const GLuint *textures, GLboolean *residences);
void ( * qglArrayElement )(GLint i);
void ( * qglBegin )(GLenum mode);
void ( * qglBeginEXT )(GLenum mode, GLint verts, GLint colors, GLint normals, GLint tex0, GLint tex1);//, GLint tex2, GLint tex3);
GLboolean ( * qglBeginFrame )(void);
void ( * qglBeginShadow )(void);
void ( * qglBindTexture )(GLenum target, GLuint texture);
void ( * qglBitmap )(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void ( * qglBlendFunc )(GLenum sfactor, GLenum dfactor);
void ( * qglCallList )(GLuint lnum);
void ( * qglCallLists )(GLsizei n, GLenum type, const GLvoid *lists);
void ( * qglClear )(GLbitfield mask);
void ( * qglClearAccum )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( * qglClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ( * qglClearDepth )(GLclampd depth);
void ( * qglClearIndex )(GLfloat c);
void ( * qglClearStencil )(GLint s);
void ( * qglClipPlane )(GLenum plane, const GLdouble *equation);
void ( * qglColor3b )(GLbyte red, GLbyte green, GLbyte blue);
void ( * qglColor3bv )(const GLbyte *v);
void ( * qglColor3d )(GLdouble red, GLdouble green, GLdouble blue);
void ( * qglColor3dv )(const GLdouble *v);
void ( * qglColor3f )(GLfloat red, GLfloat green, GLfloat blue);
void ( * qglColor3fv )(const GLfloat *v);
void ( * qglColor3i )(GLint red, GLint green, GLint blue);
void ( * qglColor3iv )(const GLint *v);
void ( * qglColor3s )(GLshort red, GLshort green, GLshort blue);
void ( * qglColor3sv )(const GLshort *v);
void ( * qglColor3ub )(GLubyte red, GLubyte green, GLubyte blue);
void ( * qglColor3ubv )(const GLubyte *v);
void ( * qglColor3ui )(GLuint red, GLuint green, GLuint blue);
void ( * qglColor3uiv )(const GLuint *v);
void ( * qglColor3us )(GLushort red, GLushort green, GLushort blue);
void ( * qglColor3usv )(const GLushort *v);
void ( * qglColor4b )(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void ( * qglColor4bv )(const GLbyte *v);
void ( * qglColor4d )(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void ( * qglColor4dv )(const GLdouble *v);
void ( * qglColor4f )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( * qglColor4fv )(const GLfloat *v);
void ( * qglColor4i )(GLint red, GLint green, GLint blue, GLint alpha);
void ( * qglColor4iv )(const GLint *v);
void ( * qglColor4s )(GLshort red, GLshort green, GLshort blue, GLshort alpha);
void ( * qglColor4sv )(const GLshort *v);
void ( * qglColor4ub )(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void ( * qglColor4ubv )(const GLubyte *v);
void ( * qglColor4ui )(GLuint red, GLuint green, GLuint blue, GLuint alpha);
void ( * qglColor4uiv )(const GLuint *v);
void ( * qglColor4us )(GLushort red, GLushort green, GLushort blue, GLushort alpha);
void ( * qglColor4usv )(const GLushort *v);
void ( * qglColorMask )(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void ( * qglColorMaterial )(GLenum face, GLenum mode);
void ( * qglColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( * qglCopyPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void ( * qglCopyTexImage1D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void ( * qglCopyTexImage2D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void ( * qglCopyTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void ( * qglCopyTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void ( * qglCullFace )(GLenum mode);
void ( * qglDeleteLists )(GLuint lnum, GLsizei range);
void ( * qglDeleteTextures )(GLsizei n, const GLuint *textures);
void ( * qglDepthFunc )(GLenum func);
void ( * qglDepthMask )(GLboolean flag);
void ( * qglDepthRange )(GLclampd zNear, GLclampd zFar);
void ( * qglDisable )(GLenum cap);
void ( * qglDisableClientState )(GLenum array);
void ( * qglDrawArrays )(GLenum mode, GLint first, GLsizei count);
void ( * qglDrawBuffer )(GLenum mode);
void ( * qglDrawElements )(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void ( * qglDrawPixels )(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglEdgeFlag )(GLboolean flag);
void ( * qglEdgeFlagPointer )(GLsizei stride, const GLvoid *pointer);
void ( * qglEdgeFlagv )(const GLboolean *flag);
void ( * qglEnable )(GLenum cap);
void ( * qglEnableClientState )(GLenum array);
void ( * qglEnd )(void);
void ( * qglEndFrame )(void);
void ( * qglEndShadow )(void);
void ( * qglEndList )(void);
void ( * qglEvalCoord1d )(GLdouble u);
void ( * qglEvalCoord1dv )(const GLdouble *u);
void ( * qglEvalCoord1f )(GLfloat u);
void ( * qglEvalCoord1fv )(const GLfloat *u);
void ( * qglEvalCoord2d )(GLdouble u, GLdouble v);
void ( * qglEvalCoord2dv )(const GLdouble *u);
void ( * qglEvalCoord2f )(GLfloat u, GLfloat v);
void ( * qglEvalCoord2fv )(const GLfloat *u);
void ( * qglEvalMesh1 )(GLenum mode, GLint i1, GLint i2);
void ( * qglEvalMesh2 )(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void ( * qglEvalPoint1 )(GLint i);
void ( * qglEvalPoint2 )(GLint i, GLint j);
void ( * qglFeedbackBuffer )(GLsizei size, GLenum type, GLfloat *buffer);
void ( * qglFinish )(void);
void ( * qglFlush )(void);
void ( * qglFlushShadow )(void);
void ( * qglFogf )(GLenum pname, GLfloat param);
void ( * qglFogfv )(GLenum pname, const GLfloat *params);
void ( * qglFogi )(GLenum pname, GLint param);
void ( * qglFogiv )(GLenum pname, const GLint *params);
void ( * qglFrontFace )(GLenum mode);
void ( * qglFrustum )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint ( * qglGenLists )(GLsizei range);
void ( * qglGenTextures )(GLsizei n, GLuint *textures);
void ( * qglGetBooleanv )(GLenum pname, GLboolean *params);
void ( * qglGetClipPlane )(GLenum plane, GLdouble *equation);
void ( * qglGetDoublev )(GLenum pname, GLdouble *params);
GLenum ( * qglGetError )(void);
void ( * qglGetFloatv )(GLenum pname, GLfloat *params);
void ( * qglGetIntegerv )(GLenum pname, GLint *params);
void ( * qglGetLightfv )(GLenum light, GLenum pname, GLfloat *params);
void ( * qglGetLightiv )(GLenum light, GLenum pname, GLint *params);
void ( * qglGetMapdv )(GLenum target, GLenum query, GLdouble *v);
void ( * qglGetMapfv )(GLenum target, GLenum query, GLfloat *v);
void ( * qglGetMapiv )(GLenum target, GLenum query, GLint *v);
void ( * qglGetMaterialfv )(GLenum face, GLenum pname, GLfloat *params);
void ( * qglGetMaterialiv )(GLenum face, GLenum pname, GLint *params);
void ( * qglGetPixelMapfv )(GLenum gmap, GLfloat *values);
void ( * qglGetPixelMapuiv )(GLenum gmap, GLuint *values);
void ( * qglGetPixelMapusv )(GLenum gmap, GLushort *values);
void ( * qglGetPointerv )(GLenum pname, GLvoid* *params);
void ( * qglGetPolygonStipple )(GLubyte *mask);
const GLubyte * ( * qglGetString )(GLenum name);
void ( * qglGetTexEnvfv )(GLenum target, GLenum pname, GLfloat *params);
void ( * qglGetTexEnviv )(GLenum target, GLenum pname, GLint *params);
void ( * qglGetTexGendv )(GLenum coord, GLenum pname, GLdouble *params);
void ( * qglGetTexGenfv )(GLenum coord, GLenum pname, GLfloat *params);
void ( * qglGetTexGeniv )(GLenum coord, GLenum pname, GLint *params);
void ( * qglGetTexImage )(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
void ( * qglGetTexLevelParameterfv )(GLenum target, GLint level, GLenum pname, GLfloat *params);
void ( * qglGetTexLevelParameteriv )(GLenum target, GLint level, GLenum pname, GLint *params);
void ( * qglGetTexParameterfv )(GLenum target, GLenum pname, GLfloat *params);
void ( * qglGetTexParameteriv )(GLenum target, GLenum pname, GLint *params);
void ( * qglHint )(GLenum target, GLenum mode);
void ( * qglIndexedTriToStrip )(GLsizei count, const GLushort *indices);
void ( * qglIndexMask )(GLuint mask);
void ( * qglIndexPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( * qglIndexd )(GLdouble c);
void ( * qglIndexdv )(const GLdouble *c);
void ( * qglIndexf )(GLfloat c);
void ( * qglIndexfv )(const GLfloat *c);
void ( * qglIndexi )(GLint c);
void ( * qglIndexiv )(const GLint *c);
void ( * qglIndexs )(GLshort c);
void ( * qglIndexsv )(const GLshort *c);
void ( * qglIndexub )(GLubyte c);
void ( * qglIndexubv )(const GLubyte *c);
void ( * qglInitNames )(void);
void ( * qglInterleavedArrays )(GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean ( * qglIsEnabled )(GLenum cap);
GLboolean ( * qglIsList )(GLuint lnum);
GLboolean ( * qglIsTexture )(GLuint texture);
void ( * qglLightModelf )(GLenum pname, GLfloat param);
void ( * qglLightModelfv )(GLenum pname, const GLfloat *params);
void ( * qglLightModeli )(GLenum pname, GLint param);
void ( * qglLightModeliv )(GLenum pname, const GLint *params);
void ( * qglLightf )(GLenum light, GLenum pname, GLfloat param);
void ( * qglLightfv )(GLenum light, GLenum pname, const GLfloat *params);
void ( * qglLighti )(GLenum light, GLenum pname, GLint param);
void ( * qglLightiv )(GLenum light, GLenum pname, const GLint *params);
void ( * qglLineStipple )(GLint factor, GLushort pattern);
void ( * qglLineWidth )(GLfloat width);
void ( * qglListBase )(GLuint base);
void ( * qglLoadIdentity )(void);
void ( * qglLoadMatrixd )(const GLdouble *m);
void ( * qglLoadMatrixf )(const GLfloat *m);
void ( * qglLoadName )(GLuint name);
void ( * qglLogicOp )(GLenum opcode);
void ( * qglMap1d )(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
void ( * qglMap1f )(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void ( * qglMap2d )(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
void ( * qglMap2f )(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
void ( * qglMapGrid1d )(GLint un, GLdouble u1, GLdouble u2);
void ( * qglMapGrid1f )(GLint un, GLfloat u1, GLfloat u2);
void ( * qglMapGrid2d )(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void ( * qglMapGrid2f )(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void ( * qglMaterialf )(GLenum face, GLenum pname, GLfloat param);
void ( * qglMaterialfv )(GLenum face, GLenum pname, const GLfloat *params);
void ( * qglMateriali )(GLenum face, GLenum pname, GLint param);
void ( * qglMaterialiv )(GLenum face, GLenum pname, const GLint *params);
void ( * qglMatrixMode )(GLenum mode);
void ( * qglMultMatrixd )(const GLdouble *m);
void ( * qglMultMatrixf )(const GLfloat *m);
void ( * qglNewList )(GLuint lnum, GLenum mode);
void ( * qglNormal3b )(GLbyte nx, GLbyte ny, GLbyte nz);
void ( * qglNormal3bv )(const GLbyte *v);
void ( * qglNormal3d )(GLdouble nx, GLdouble ny, GLdouble nz);
void ( * qglNormal3dv )(const GLdouble *v);
void ( * qglNormal3f )(GLfloat nx, GLfloat ny, GLfloat nz);
void ( * qglNormal3fv )(const GLfloat *v);
void ( * qglNormal3i )(GLint nx, GLint ny, GLint nz);
void ( * qglNormal3iv )(const GLint *v);
void ( * qglNormal3s )(GLshort nx, GLshort ny, GLshort nz);
void ( * qglNormal3sv )(const GLshort *v);
void ( * qglNormalPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( * qglOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( * qglPassThrough )(GLfloat token);
void ( * qglPixelMapfv )(GLenum gmap, GLsizei mapsize, const GLfloat *values);
void ( * qglPixelMapuiv )(GLenum gmap, GLsizei mapsize, const GLuint *values);
void ( * qglPixelMapusv )(GLenum gmap, GLsizei mapsize, const GLushort *values);
void ( * qglPixelStoref )(GLenum pname, GLfloat param);
void ( * qglPixelStorei )(GLenum pname, GLint param);
void ( * qglPixelTransferf )(GLenum pname, GLfloat param);
void ( * qglPixelTransferi )(GLenum pname, GLint param);
void ( * qglPixelZoom )(GLfloat xfactor, GLfloat yfactor);
void ( * qglPointSize )(GLfloat size);
void ( * qglPolygonMode )(GLenum face, GLenum mode);
void ( * qglPolygonOffset )(GLfloat factor, GLfloat units);
void ( * qglPolygonStipple )(const GLubyte *mask);
void ( * qglPopAttrib )(void);
void ( * qglPopClientAttrib )(void);
void ( * qglPopMatrix )(void);
void ( * qglPopName )(void);
void ( * qglPrioritizeTextures )(GLsizei n, const GLuint *textures, const GLclampf *priorities);
void ( * qglPushAttrib )(GLbitfield mask);
void ( * qglPushClientAttrib )(GLbitfield mask);
void ( * qglPushMatrix )(void);
void ( * qglPushName )(GLuint name);
void ( * qglRasterPos2d )(GLdouble x, GLdouble y);
void ( * qglRasterPos2dv )(const GLdouble *v);
void ( * qglRasterPos2f )(GLfloat x, GLfloat y);
void ( * qglRasterPos2fv )(const GLfloat *v);
void ( * qglRasterPos2i )(GLint x, GLint y);
void ( * qglRasterPos2iv )(const GLint *v);
void ( * qglRasterPos2s )(GLshort x, GLshort y);
void ( * qglRasterPos2sv )(const GLshort *v);
void ( * qglRasterPos3d )(GLdouble x, GLdouble y, GLdouble z);
void ( * qglRasterPos3dv )(const GLdouble *v);
void ( * qglRasterPos3f )(GLfloat x, GLfloat y, GLfloat z);
void ( * qglRasterPos3fv )(const GLfloat *v);
void ( * qglRasterPos3i )(GLint x, GLint y, GLint z);
void ( * qglRasterPos3iv )(const GLint *v);
void ( * qglRasterPos3s )(GLshort x, GLshort y, GLshort z);
void ( * qglRasterPos3sv )(const GLshort *v);
void ( * qglRasterPos4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( * qglRasterPos4dv )(const GLdouble *v);
void ( * qglRasterPos4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( * qglRasterPos4fv )(const GLfloat *v);
void ( * qglRasterPos4i )(GLint x, GLint y, GLint z, GLint w);
void ( * qglRasterPos4iv )(const GLint *v);
void ( * qglRasterPos4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( * qglRasterPos4sv )(const GLshort *v);
void ( * qglReadBuffer )(GLenum mode);
void ( * qglReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void ( * qglCopyBackBufferToTexEXT )(float width, float height, float u1, float v1, float u2, float v2);
void ( * qglCopyBackBufferToTex )(void);
void ( * qglRectd )(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void ( * qglRectdv )(const GLdouble *v1, const GLdouble *v2);
void ( * qglRectf )(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void ( * qglRectfv )(const GLfloat *v1, const GLfloat *v2);
void ( * qglRecti )(GLint x1, GLint y1, GLint x2, GLint y2);
void ( * qglRectiv )(const GLint *v1, const GLint *v2);
void ( * qglRects )(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void ( * qglRectsv )(const GLshort *v1, const GLshort *v2);
GLint ( * qglRenderMode )(GLenum mode);
void ( * qglRotated )(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void ( * qglRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void ( * qglScaled )(GLdouble x, GLdouble y, GLdouble z);
void ( * qglScalef )(GLfloat x, GLfloat y, GLfloat z);
void ( * qglScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( * qglSelectBuffer )(GLsizei size, GLuint *buffer);
void ( * qglShadeModel )(GLenum mode);
void ( * qglStencilFunc )(GLenum func, GLint ref, GLuint mask);
void ( * qglStencilMask )(GLuint mask);
void ( * qglStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
void ( * qglTexCoord1d )(GLdouble s);
void ( * qglTexCoord1dv )(const GLdouble *v);
void ( * qglTexCoord1f )(GLfloat s);
void ( * qglTexCoord1fv )(const GLfloat *v);
void ( * qglTexCoord1i )(GLint s);
void ( * qglTexCoord1iv )(const GLint *v);
void ( * qglTexCoord1s )(GLshort s);
void ( * qglTexCoord1sv )(const GLshort *v);
void ( * qglTexCoord2d )(GLdouble s, GLdouble t);
void ( * qglTexCoord2dv )(const GLdouble *v);
void ( * qglTexCoord2f )(GLfloat s, GLfloat t);
void ( * qglTexCoord2fv )(const GLfloat *v);
void ( * qglTexCoord2i )(GLint s, GLint t);
void ( * qglTexCoord2iv )(const GLint *v);
void ( * qglTexCoord2s )(GLshort s, GLshort t);
void ( * qglTexCoord2sv )(const GLshort *v);
void ( * qglTexCoord3d )(GLdouble s, GLdouble t, GLdouble r);
void ( * qglTexCoord3dv )(const GLdouble *v);
void ( * qglTexCoord3f )(GLfloat s, GLfloat t, GLfloat r);
void ( * qglTexCoord3fv )(const GLfloat *v);
void ( * qglTexCoord3i )(GLint s, GLint t, GLint r);
void ( * qglTexCoord3iv )(const GLint *v);
void ( * qglTexCoord3s )(GLshort s, GLshort t, GLshort r);
void ( * qglTexCoord3sv )(const GLshort *v);
void ( * qglTexCoord4d )(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void ( * qglTexCoord4dv )(const GLdouble *v);
void ( * qglTexCoord4f )(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void ( * qglTexCoord4fv )(const GLfloat *v);
void ( * qglTexCoord4i )(GLint s, GLint t, GLint r, GLint q);
void ( * qglTexCoord4iv )(const GLint *v);
void ( * qglTexCoord4s )(GLshort s, GLshort t, GLshort r, GLshort q);
void ( * qglTexCoord4sv )(const GLshort *v);
void ( * qglTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( * qglTexEnvf )(GLenum target, GLenum pname, GLfloat param);
void ( * qglTexEnvfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( * qglTexEnvi )(GLenum target, GLenum pname, GLint param);
void ( * qglTexEnviv )(GLenum target, GLenum pname, const GLint *params);
void ( * qglTexGend )(GLenum coord, GLenum pname, GLdouble param);
void ( * qglTexGendv )(GLenum coord, GLenum pname, const GLdouble *params);
void ( * qglTexGenf )(GLenum coord, GLenum pname, GLfloat param);
void ( * qglTexGenfv )(GLenum coord, GLenum pname, const GLfloat *params);
void ( * qglTexGeni )(GLenum coord, GLenum pname, GLint param);
void ( * qglTexGeniv )(GLenum coord, GLenum pname, const GLint *params);
void ( * qglTexImage1D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglTexImage2DEXT )(GLenum target, GLint level, GLint numlevels, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglTexParameterf )(GLenum target, GLenum pname, GLfloat param);
void ( * qglTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( * qglTexParameteri )(GLenum target, GLenum pname, GLint param);
void ( * qglTexParameteriv )(GLenum target, GLenum pname, const GLint *params);
void ( * qglTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( * qglTranslated )(GLdouble x, GLdouble y, GLdouble z);
void ( * qglTranslatef )(GLfloat x, GLfloat y, GLfloat z);
void ( * qglVertex2d )(GLdouble x, GLdouble y);
void ( * qglVertex2dv )(const GLdouble *v);
void ( * qglVertex2f )(GLfloat x, GLfloat y);
void ( * qglVertex2fv )(const GLfloat *v);
void ( * qglVertex2i )(GLint x, GLint y);
void ( * qglVertex2iv )(const GLint *v);
void ( * qglVertex2s )(GLshort x, GLshort y);
void ( * qglVertex2sv )(const GLshort *v);
void ( * qglVertex3d )(GLdouble x, GLdouble y, GLdouble z);
void ( * qglVertex3dv )(const GLdouble *v);
void ( * qglVertex3f )(GLfloat x, GLfloat y, GLfloat z);
void ( * qglVertex3fv )(const GLfloat *v);
void ( * qglVertex3i )(GLint x, GLint y, GLint z);
void ( * qglVertex3iv )(const GLint *v);
void ( * qglVertex3s )(GLshort x, GLshort y, GLshort z);
void ( * qglVertex3sv )(const GLshort *v);
void ( * qglVertex4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( * qglVertex4dv )(const GLdouble *v);
void ( * qglVertex4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( * qglVertex4fv )(const GLfloat *v);
void ( * qglVertex4i )(GLint x, GLint y, GLint z, GLint w);
void ( * qglVertex4iv )(const GLint *v);
void ( * qglVertex4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( * qglVertex4sv )(const GLshort *v);
void ( * qglVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( * qglViewport )(GLint x, GLint y, GLsizei width, GLsizei height);

#if 0
void ( * qglMultiTexCoord2fARB )( GLenum texture, GLfloat s, GLfloat t );
void ( * qglActiveTextureARB )( GLenum texture );
void ( * qglClientActiveTextureARB )( GLenum texture );
#endif

static void _d3d_check(HRESULT err, const char* func)
{
	if (err != D3D_OK)
	{
		MEMORYSTATUS status;
		GlobalMemoryStatus(&status);
		Sys_Print(va("%s returned %d!  Memfree=%d\n", func, err, status.dwAvailPhys));
	}
}

#ifdef _WINDOWS
static bool surfaceToBMP(LPDIRECT3DDEVICE8 pd3dDevice, LPDIRECT3DSURFACE8 lpSurface, const char *fname)
{
	DWORD outpixel;
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER bi;
	int outbyte, BufferIndex, width, height, pitch;
	char *WriteBuffer;
	FILE *file;
	HRESULT Error;
	IDirect3DSurface8 *pTempSurf = NULL;

	// Get the surface description first
	D3DSURFACE_DESC ddsd;
	D3DLOCKED_RECT lrSurf;
	
	Error = lpSurface->GetDesc(&ddsd);
	// This writes out 32 bit values, so whatever surface format we were passed in,
	// copy it into a 32 bit surface
	Error = pd3dDevice->CreateImageSurface(ddsd.Width, ddsd.Height, D3DFMT_A8R8G8B8, &pTempSurf);

	Error = D3DXLoadSurfaceFromSurface(pTempSurf, NULL, NULL, lpSurface, NULL, NULL, D3DX_DEFAULT, 0);

	file = fopen(fname, "wb");
	if(!file)
		return FALSE;

	Error = pTempSurf->LockRect(&lrSurf, NULL, 0);

	BufferIndex = 0;
	width = ddsd.Width;
	height = ddsd.Height;
	pitch = lrSurf.Pitch;
	WriteBuffer = new char[width * height * 3];

	// Setup the file headers
	((char*)&(fh.bfType))[0] = 'B';
	((char*)&(fh.bfType))[1] = 'M';
	fh.bfSize = (long)(sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER) + width * height * 3);
	fh.bfReserved1 = 0;
	fh.bfReserved2 = 0;
	fh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = height;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 10000;
	bi.biYPelsPerMeter = 10000;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	fwrite(&fh, sizeof(BITMAPFILEHEADER), 1, file);
	fwrite(&bi, sizeof(BITMAPINFOHEADER), 1, file);

	char *Bitmap_in = (char*)lrSurf.pBits;

	for(int y = height - 1; y >= 0; y--) 
	{
		for(int x = 0; x < width; x++)
		{
			outpixel = *((DWORD *)(Bitmap_in + x * 4 + y * pitch)); //Load a word

			//Load up the Blue component and output it
			outbyte = (((outpixel)&0x000000ff));//blue
			WriteBuffer [BufferIndex++] = outbyte;

			//Load up the green component and output it 
			outbyte = (((outpixel>>8)&0x000000ff)); 
			WriteBuffer [BufferIndex++] = outbyte;

			//Load up the red component and output it 
			outbyte = (((outpixel>>16)&0x000000ff));
			WriteBuffer [BufferIndex++] = outbyte;
		}
	}

	//At this point the buffer should be full, so just write it out
	fwrite(WriteBuffer, BufferIndex, 1, file);

	//Now unlock the surface and we're done
	pTempSurf->UnlockRect();
	pTempSurf->Release();

	fclose(file);

	delete [] WriteBuffer;
	return true;
}
#endif

/*
=================
_fixupScreenCoords

Clamp coords to screen dimensions and fix Y direction.
=================
*/
static void _fixupScreenCoords(GLint& x, GLint& y, GLsizei& width, GLsizei& height)
{
	if (x < 0) x = 0;
	else if (x > glConfig.vidWidth) x = glConfig.vidWidth;
	if (y < 0) y = 0;
	else if (y > glConfig.vidHeight) y = glConfig.vidHeight;
	
	if (width < 0) width = 0;
	else if (x + width > glConfig.vidWidth) width = glConfig.vidWidth - x;
	if (height < 0) height = 0;
	else if (y + height > glConfig.vidHeight) height = glConfig.vidHeight - y;

	// GL and DX disagree on the direction of Y
	y = glConfig.vidHeight - (y + height);
}


/*
=================
_convertCompare

Convert GL compare function to DX function.
=================
*/
static D3DCMPFUNC _convertCompare(GLenum func)
{
	switch (func)
	{
	case GL_NEVER: return D3DCMP_NEVER;
	case GL_LESS: return D3DCMP_LESS;
	case GL_EQUAL: return D3DCMP_EQUAL;
	case GL_LEQUAL: return D3DCMP_LESSEQUAL;
	case GL_GREATER: return D3DCMP_GREATER;
	case GL_NOTEQUAL: return D3DCMP_NOTEQUAL;
	case GL_GEQUAL: return D3DCMP_GREATEREQUAL;
	default: case GL_ALWAYS: return D3DCMP_ALWAYS;
	}
}


/*
=================
_convertBlendFactor

Convert GL blend mode to DX blend mode.
=================
*/
static D3DBLEND _convertBlendFactor(GLenum factor)
{
	switch (factor)
	{
	case GL_ZERO: return D3DBLEND_ZERO;
	default: case GL_ONE: return D3DBLEND_ONE;
	case GL_SRC_COLOR: return D3DBLEND_SRCCOLOR;
	case GL_ONE_MINUS_SRC_COLOR: return D3DBLEND_INVSRCCOLOR;
	case GL_SRC_ALPHA: return D3DBLEND_SRCALPHA;
	case GL_ONE_MINUS_SRC_ALPHA: return D3DBLEND_INVSRCALPHA;
	case GL_DST_COLOR: return D3DBLEND_DESTCOLOR;
	case GL_ONE_MINUS_DST_COLOR: return D3DBLEND_INVDESTCOLOR;
	case GL_DST_ALPHA: return D3DBLEND_DESTALPHA;
	case GL_ONE_MINUS_DST_ALPHA: return D3DBLEND_INVDESTALPHA;
	case GL_SRC_ALPHA_SATURATE: return D3DBLEND_SRCALPHASAT;
	}
}


/*
=================
_convertPrimMode

Convert GL primitive mode to DX primitive mode.
=================
*/
static D3DPRIMITIVETYPE _convertPrimMode(GLenum mode)
{
	switch (mode)
	{
	case GL_POINTS: return D3DPT_POINTLIST;
	case GL_LINES: return D3DPT_LINELIST;
	case GL_LINE_STRIP: return D3DPT_LINESTRIP;
	case GL_TRIANGLES: return D3DPT_TRIANGLELIST;
	case GL_TRIANGLE_STRIP: return D3DPT_TRIANGLESTRIP;
	case GL_TRIANGLE_FAN: return D3DPT_TRIANGLEFAN;
#ifdef _XBOX
	case GL_QUADS: return D3DPT_QUADLIST;
	case GL_QUAD_STRIP: return D3DPT_QUADSTRIP;
#else
	case GL_QUADS: return D3DPT_TRIANGLELIST;
	case GL_QUAD_STRIP: return D3DPT_TRIANGLESTRIP;
#endif
	case GL_POLYGON: return D3DPT_TRIANGLEFAN;
	default: assert(0); return D3DPT_TRIANGLEFAN;
	}
}


/*
=================
_updateDrawStride

Update the stride of the draw array based on
the number of vertex attributes.  The stride
is in DWORDs.
=================
*/
static void _updateDrawStride(GLint normal, GLint tex0, GLint tex1)
{
	glw_state->drawStride = 4;
	if (normal) glw_state->drawStride += 3;
	if (tex0) glw_state->drawStride += 2;
	if (tex1) glw_state->drawStride += 2;
}

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static unsigned int s_stefxVertexShaderRequests;
static unsigned int s_stefxVertexShaderEmits;
static unsigned int s_stefxVertexShaderSkips;
static unsigned int s_stefxStreamSourceRequests;
static unsigned int s_stefxStreamSourceEmits;
static unsigned int s_stefxStreamSourceSkips;
#endif

HRESULT STEFX_D3D8_SetVertexShaderTracked( DWORD shader )
{
	HRESULT result;

	if ( !glw_state || !glw_state->device )
	{
		return E_FAIL;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	++s_stefxVertexShaderRequests;
	if ( backEnd.viewParms.stefxSplitThreePlusEconomy &&
		glw_state->shaderMaskValid &&
		glw_state->shaderMask == shader )
	{
		++s_stefxVertexShaderSkips;
		return D3D_OK;
	}
#endif
	result = glw_state->device->SetVertexShader( shader );
	if ( SUCCEEDED( result ) )
	{
		glw_state->shaderMask = shader;
		glw_state->shaderMaskValid = true;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		++s_stefxVertexShaderEmits;
#endif
	}
	else
	{
		glw_state->shaderMaskValid = false;
	}
	return result;
}

HRESULT STEFX_D3D8_SetStreamSourceZeroTracked( UINT stride )
{
	HRESULT result;

	if ( !glw_state || !glw_state->device )
	{
		return E_FAIL;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	++s_stefxStreamSourceRequests;
	if ( backEnd.viewParms.stefxSplitThreePlusEconomy &&
		glw_state->streamSourceZeroValid &&
		glw_state->streamSourceZeroStride == stride )
	{
		++s_stefxStreamSourceSkips;
		return D3D_OK;
	}
#endif
	result = glw_state->device->SetStreamSource( 0, NULL, stride );
	if ( SUCCEEDED( result ) )
	{
		glw_state->streamSourceZeroStride = stride;
		glw_state->streamSourceZeroValid = true;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		++s_stefxStreamSourceEmits;
#endif
	}
	else
	{
		glw_state->streamSourceZeroValid = false;
	}
	return result;
}

/*
=================
_updateShader

Set the vertex shader based on the number
of texture coordinates.
=================
*/
static void _updateShader(bool normal, bool tex0, bool tex1)//, bool tex2, bool tex3)
{
	DWORD mask = D3DFVF_XYZ;
	if (normal) mask |= D3DFVF_NORMAL;
	mask |= D3DFVF_DIFFUSE;
	if (tex0 && !tex1) mask |= D3DFVF_TEX1;
	else if (tex1) mask |= D3DFVF_TEX2;

	STEFX_D3D8_SetVertexShaderTracked( mask );
}


/*
=================
_getCurrentTexture

Get the texture information for the currently
bound texture at a stage.
=================
*/
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
extern "C" __declspec(dllexport) volatile unsigned int g_SPXBStasisD3D[48] = {0};
static void STEFX_TraceStasisD3D(void)
{
	if (!tess.shader || Q_stricmp(tess.shader->name, "textures/stasis/scum_256")) return;
	volatile unsigned int *r = g_SPXBStasisD3D;
	++r[0];
	if ((r[0] & 255u) != 1u) return;
	++r[1]; r[2] = tess.shader->index;
	DWORD shader = 0; glw_state->device->GetVertexShader(&shader);
	r[3] = shader; r[4] = glw_state->shaderMask; r[5] = glw_state->drawStride;
	r[6] = (glw_state->texCoordArrayState[0] ? 1u : 0u) | (glw_state->texCoordArrayState[1] ? 2u : 0u);
	r[7] = glw_state->colorArrayState;
	for (int stage = 0; stage < 2; ++stage)
	{
		volatile unsigned int *s = r + 8 + stage * 16;
		glwstate_t::texturexlat_t::iterator it = glw_state->textureXlat.find(glw_state->currentTexture[stage]);
		IDirect3DBaseTexture8 *actual = NULL;
		glw_state->device->GetTexture(stage, &actual);
		s[0] = glw_state->currentTexture[stage];
		s[1] = it == glw_state->textureXlat.end() ? 0 : (unsigned int)it->second.mipmap;
		s[2] = (unsigned int)actual;
		s[3] = glw_state->textureStageEnable[stage];
		if (s[1] != s[2]) ++r[40];
		if (actual) actual->Release();
		D3DTEXTURESTAGESTATETYPE types[8] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
			D3DTSS_ALPHAOP, D3DTSS_TEXCOORDINDEX, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTSS_ADDRESSU, D3DTSS_ADDRESSV};
		for (int i = 0; i < 8; ++i) { DWORD value=0; glw_state->device->GetTextureStageState(stage, types[i], &value); s[4+i]=value; }
		s[12] = glw_state->textureEnv[stage]; s[13] = glw_state->texCoordStride[stage];
		if (glw_state->texCoordArrayState[stage] && glw_state->texCoordPointer[stage])
		{
			const unsigned int *uv = (const unsigned int *)glw_state->texCoordPointer[stage];
			s[14] = uv[0]; s[15] = uv[1];
			if (uv[0] != ((const unsigned int *)tess.svars.texcoords[stage])[0] ||
				uv[1] != ((const unsigned int *)tess.svars.texcoords[stage])[1]) ++r[41];
		}
	}
}
#endif

static glwstate_t::TextureInfo* _getCurrentTexture(int stage)
{
	glwstate_t::texturexlat_t::iterator i = glw_state->textureXlat.find(
		glw_state->currentTexture[stage]);

	if (i == glw_state->textureXlat.end()) return NULL;

	glwstate_t::TextureInfo *info = &i->second;
	if (!info->inMemory)
		gSkinTextures.Fetch( glw_state->currentTexture[stage] );
	return info;
}


#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static DWORD s_stefxTextureStageState[GLW_MAX_TEXTURE_STAGES][D3DTSS_MAX];
static unsigned char s_stefxTextureStageStateValid[GLW_MAX_TEXTURE_STAGES][D3DTSS_MAX];
static unsigned int s_stefxTextureStageRequests;
static unsigned int s_stefxTextureStageEmits;
static unsigned int s_stefxTextureStageSkips;
static unsigned int s_stefxTextureStageFrames;

void STEFX_D3D8_InvalidateTextureStageCache( void )
{
	memset( s_stefxTextureStageStateValid, 0,
		sizeof( s_stefxTextureStageStateValid ) );
}

static __forceinline HRESULT STEFX_D3D8_SetTextureStageStateCached(
	DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value )
{
	++s_stefxTextureStageRequests;
	if ( stage < GLW_MAX_TEXTURE_STAGES && type < D3DTSS_MAX &&
		s_stefxTextureStageStateValid[stage][type] &&
		s_stefxTextureStageState[stage][type] == value )
	{
		++s_stefxTextureStageSkips;
		return D3D_OK;
	}

	const HRESULT result = glw_state->device->SetTextureStageState(
		stage, type, value );
	if ( result == D3D_OK )
	{
		if ( stage < GLW_MAX_TEXTURE_STAGES && type < D3DTSS_MAX )
		{
			s_stefxTextureStageState[stage][type] = value;
			s_stefxTextureStageStateValid[stage][type] = 1;
		}
		++s_stefxTextureStageEmits;
	}
	return result;
}
#else
void STEFX_D3D8_InvalidateTextureStageCache( void )
{
}

#define STEFX_D3D8_SetTextureStageStateCached(stage, type, value) \
	glw_state->device->SetTextureStageState((stage), (type), (value))
#endif


/*
=================
_updateTextures

Setup texture stages with color operations, filters
and wrapping modes as needed.
=================
*/
static void _updateTextures(void)
{
	for (int t = 0; t < GLW_MAX_TEXTURE_STAGES; ++t)
	{
		if (glw_state->textureStageDirty[t])
		{
			glw_state->textureStageDirty[t] = false;

			if (glw_state->textureStageEnable[t] && glw_state->currentTexture[t])
			{
				glwstate_t::TextureInfo* info = _getCurrentTexture(t);
				if (!info) continue;

				glw_state->device->SetTexture( t, info->mipmap );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_COLOROP, glw_state->textureEnv[t] );

#if !defined(_XBOX) || !defined(STEFX_SP_HOSTED_MP)
				glw_state->device->SetTextureStageState(t, D3DTSS_COLORARG1, 
						D3DTA_TEXTURE);
				glw_state->device->SetTextureStageState(t, D3DTSS_COLORARG2, 
						D3DTA_CURRENT);
				glw_state->device->SetTextureStageState(t, D3DTSS_ALPHAARG1, 
						D3DTA_TEXTURE);
				glw_state->device->SetTextureStageState(t, D3DTSS_ALPHAARG2, 
						D3DTA_CURRENT);
#endif
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_ALPHAOP, glw_state->textureEnv[t] );

				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_MAXANISOTROPY, (DWORD)info->anisotropy );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_MINFILTER, info->minFilter );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_MIPFILTER, info->mipFilter );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_MAGFILTER, info->magFilter );

				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_ADDRESSU, info->wrapU );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_ADDRESSV, info->wrapV );

				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_TEXCOORDINDEX, t );

#if !defined(_XBOX) || !defined(STEFX_SP_HOSTED_MP)
				glw_state->device->SetTextureStageState( t, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
#endif

				if(tess.shader)
				{
					if(tess.currentPass < tess.shader->numUnfoggedPasses)
					{ 
						if(tess.shader->stages[tess.currentPass].isEnvironment)
						{
							STEFX_D3D8_SetTextureStageStateCached( t,
								D3DTSS_TEXCOORDINDEX,
								t | D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR );
						}
					}
				}				
			}
			else
			{
				glw_state->device->SetTexture( t, NULL );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_COLOROP, D3DTOP_DISABLE );
				STEFX_D3D8_SetTextureStageStateCached(
					t, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
			}
		}
		else
		{
			// Hard-wired check for turning on hardware environment mapping
			if( glw_state->textureStageEnable[t] &&
				glw_state->currentTexture[t] &&
				tess.shader &&
				tess.currentPass < tess.shader->numUnfoggedPasses &&
				tess.shader->stages[tess.currentPass].isEnvironment)
			{
					STEFX_D3D8_SetTextureStageStateCached( t,
						D3DTSS_TEXCOORDINDEX,
						t | D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR );
			}
		}
	}
}


/*
=================
_updateMatrices

Set the current projection and view transforms to
the matrices at the top of the relevant stacks.
=================
*/
static void _updateMatrices(void)
{
	if(glw_state->matricesDirty[glwstate_t::MatrixMode_Projection])
	{
		glw_state->device->SetTransform(D3DTS_PROJECTION, 
			glw_state->matrixStack[glwstate_t::MatrixMode_Projection]->GetTop());

		glw_state->matricesDirty[glwstate_t::MatrixMode_Projection] = false;
	}

	if(glw_state->matricesDirty[glwstate_t::MatrixMode_Model])
	{
		glw_state->device->SetTransform(D3DTS_VIEW, 
			glw_state->matrixStack[glwstate_t::MatrixMode_Model]->GetTop());

		glw_state->matricesDirty[glwstate_t::MatrixMode_Model] = false;
	}

	if(glw_state->matricesDirty[glwstate_t::MatrixMode_Texture0])
	{
		glw_state->device->SetTransform(D3DTS_TEXTURE0,
			glw_state->matrixStack[glwstate_t::MatrixMode_Texture0]->GetTop());

		glw_state->matricesDirty[glwstate_t::MatrixMode_Texture0] = false;
	}

	if(glw_state->matricesDirty[glwstate_t::MatrixMode_Texture1])
	{
		glw_state->device->SetTransform(D3DTS_TEXTURE1,
			glw_state->matrixStack[glwstate_t::MatrixMode_Texture1]->GetTop());

		glw_state->matricesDirty[glwstate_t::MatrixMode_Texture1] = false;
	}
}


/*
=================
_getMaxVerts

Calculate the maximum number of verts to draw
given a total number to draw, stride and max
packet size.
=================
*/
static int _getMaxVerts(void)
{
	int max = glw_state->totalVertices;
	if (max > GLW_MAX_DRAW_PACKET_SIZE / glw_state->drawStride)
	{
		max = GLW_MAX_DRAW_PACKET_SIZE / glw_state->drawStride;
	}
	return max;
}

static int _getMaxIndices(void)
{
	int max = glw_state->totalIndices;
	if(max > 1022)
		max = 1022;

	return max;
}

#ifdef _XBOX
/*
=================
_restartDrawPacket

Encode a new draw packet header into the draw array.
=================
*/
inline static DWORD* _restartDrawPacket(DWORD* packet, int verts)
{
	packet[0] = D3DPUSH_ENCODE(D3DPUSH_SET_BEGIN_END, 1);
	packet[1] = glw_state->primitiveMode;
	packet[2] = D3DPUSH_ENCODE(
		D3DPUSH_NOINCREMENT_FLAG|D3DPUSH_INLINE_ARRAY, 
		glw_state->drawStride * verts);
	return packet + 3;
}

/*
=================
_terminateDrawPacket

Finish up the last draw packet.
=================
*/
inline static DWORD* _terminateDrawPacket(DWORD* packet)
{
	packet[0] = D3DPUSH_ENCODE(D3DPUSH_SET_BEGIN_END, 1);
	packet[1] = 0;
	return packet + 2;
}

#define CMD_DRAW_INDEX_BATCH 0x1800
inline static DWORD* _restartIndexPacket(DWORD* packet, int numIndices)
{
	packet[0] = D3DPUSH_ENCODE(D3DPUSH_SET_BEGIN_END, 1);
	packet[1] = glw_state->primitiveMode;
	packet[2] = D3DPUSH_ENCODE(	D3DPUSH_NOINCREMENT_FLAG | CMD_DRAW_INDEX_BATCH, numIndices / 2 );
	return packet + 3;
}

inline static DWORD* _terminateIndexPacket(DWORD* packet)
{
	packet[0] = D3DPUSH_ENCODE(D3DPUSH_SET_BEGIN_END, 1);
	packet[1] = 0;
	return packet + 2;
}


/*
=================
_handleDrawOverflow

Prevent a draw packet from getting too
big for the hardware by restarting it as needed.
=================
*/
static void _handleDrawOverflow(void)
{
	if (glw_state->numVertices >= glw_state->maxVertices)
	{
		glw_state->drawArray += glw_state->numVertices *
			glw_state->drawStride;
		
		glw_state->totalVertices -= glw_state->numVertices;
		glw_state->maxVertices = _getMaxVerts();
		glw_state->numVertices = 0;

		glw_state->drawArray = _restartDrawPacket(
			glw_state->drawArray, glw_state->maxVertices);
	}
}
#else _XBOX
inline static DWORD* _restartDrawPacket(DWORD* packet, int verts)
{
	return packet;
}

inline static DWORD* _terminateDrawPacket(DWORD* packet)
{
	return packet;
}

static void _handleDrawOverflow(void)
{
}
#endif _XBOX


/*
=================
_vertexElement

Copy position information from the source vertex
array into a draw array.
=================
*/
#define _vertexElement(push, i)									\
{																\
	DWORD* vert = (DWORD*)((BYTE*)glw_state->vertexPointer +	\
		(i) * glw_state->vertexStride);							\
	(push)[0] = vert[0];										\
	(push)[1] = vert[1];										\
	(push)[2] = vert[2];										\
}

/*
=================
_colorElement

Copy color information from the source color
array into a draw array.
=================
*/
#define _colorElement(push, i)									\
{																\
	DWORD col = *(DWORD*)((BYTE*)glw_state->colorPointer +		\
		(i) * glw_state->colorStride);							\
	(push)[0] =													\
		((col & 0xFF000000) >> 0) |								\
		((col & 0x00FF0000) >> 16) |							\
		((col & 0x0000FF00) << 0) |								\
		((col & 0x000000FF) << 16);								\
}

/*
=================
_texCoordElement

Copy tex coord information from the source tex coord
array into a draw array.
=================
*/
#define _texCoordElement(push, i, t)							\
{																\
	DWORD* tc = (DWORD*)((BYTE*)glw_state->texCoordPointer[t] +	\
		(i) * glw_state->texCoordStride[t]);					\
	(push)[0] = tc[0];											\
	(push)[1] = tc[1];											\
}

/*
=================
_normalElement

  Copy normal information from the source normal
  array into a draw array
=================
*/
#define _normalElement(push, i)									\
{																\
	DWORD* norm = (DWORD*)((BYTE*)glw_state->normalPointer +	\
		(i) * glw_state->normalStride);							\
	(push)[0] = norm[0];										\
	(push)[1] = norm[1];										\
	(push)[2] = norm[2];										\
}

				
#define _tangentElement(push, i)								\
{																\
	DWORD* tang = (DWORD*)((BYTE*)&tess.tangent[i]);				\
	(push)[0] = tang[0];										\
	(push)[1] = tang[1];										\
	(push)[2] = tang[2];										\
}



/*
=========================================================
FAST INDEXED GEOMETRY DRAW LOOPS

Used by core draw routines to quickly copy
geometry from various source arrays to main
draw array.
=========================================================
*/
static void _drawElementsV(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		push[3] = glw_state->currentColor;
		push += 4;
	}
}

static void _drawElementsVN(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		push[6] = glw_state->currentColor;
		push += 7;
	}
}

static void _drawElementsVC(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_colorElement(&push[3], indices[i]);
		push += 4;
	}
}

static void _drawElementsVCN(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		_colorElement(&push[6], indices[i]);
		push += 7;
	}
}

static void _drawElementsVCT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_colorElement(&push[3], indices[i]);
		_texCoordElement(&push[4], indices[i], 0);
		push += 6;
	}
}

static void _drawElementsVCNT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		_colorElement(&push[6], indices[i]);
		_texCoordElement(&push[7], indices[i], 0);
		push += 9;
	}
}

static void _drawElementsVCTT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_colorElement(&push[3], indices[i]);
		_texCoordElement(&push[4], indices[i], 0);
		_texCoordElement(&push[6], indices[i], 1);
		push += 8;
	}
}

static void _drawElementsVCNTT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		_colorElement(&push[6], indices[i]);
		_texCoordElement(&push[7], indices[i], 0);
		_texCoordElement(&push[9], indices[i], 1);
		push += 11;
	}
}

static void _drawElementsVT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		push[3] = glw_state->currentColor;
		_texCoordElement(&push[4], indices[i], 0);
		push += 6;
	}
}

static void _drawElementsVNT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		push[6] = glw_state->currentColor;
		_texCoordElement(&push[7], indices[i], 0);
		push += 9;
	}
}

static void _drawElementsVTT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		push[3] = glw_state->currentColor;
		_texCoordElement(&push[4], indices[i], 0);
		_texCoordElement(&push[6], indices[i], 1);
		push += 8;
	}
}

static void _drawElementsVNTT(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for (int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		push[6] = glw_state->currentColor;
		_texCoordElement(&push[7], indices[i], 0);
		_texCoordElement(&push[9], indices[i], 1);
		push += 11;
	}
}


static void _drawElementsLightShader(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for(int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		_texCoordElement(&push[6], indices[i], 0);
		_tangentElement(&push[8], indices[i]);
		push += 11;
	}
}

static void _drawElementsBumpShader(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for(int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		_texCoordElement(&push[6], indices[i], 0);
		_texCoordElement(&push[8], indices[i], 1);
		_tangentElement(&push[10], indices[i]);
		push += 13;
	}
}

static void _drawElementsEnvShader(GLsizei count, const GLushort* indices)
{
	DWORD* push = glw_state->drawArray;
	for(int i = 0; i < count; ++i)
	{
		_vertexElement(&push[0], indices[i]);
		_normalElement(&push[3], indices[i]);
		push += 6;
	}
}

typedef void(*drawelemfunc_t)(GLsizei, const GLushort*);
static drawelemfunc_t _drawElementFuncTable[12] =
{
	_drawElementsV,
	_drawElementsVN,
	_drawElementsVT,
	_drawElementsVNT,
	_drawElementsVTT,
	_drawElementsVNTT,
	_drawElementsVC,
	_drawElementsVCN,
	_drawElementsVCT,
	_drawElementsVCNT,
	_drawElementsVCTT,
	_drawElementsVCNTT,
};



/*
=========================================================
FAST GEOMETRY DRAW LOOPS

Used by core draw routines to quickly copy
geometry from various source arrays to main
draw array.
=========================================================
*/
static void _drawArraysV(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		push[3] = glw_state->currentColor;
		push += 4;
	}
}

static void _drawArraysVN(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		push[6] = glw_state->currentColor;
		push += 7;
	}
}

static void _drawArraysVC(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_colorElement(&push[3], i);
		push += 4;
	}
}

static void _drawArraysVCN(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		_colorElement(&push[6], i);
		push += 7;
	}
}

static void _drawArraysVCT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_colorElement(&push[3], i);
		_texCoordElement(&push[4], i, 0);
		push += 6;
	}
}

static void _drawArraysVCNT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		_colorElement(&push[6], i);
		_texCoordElement(&push[7], i, 0);
		push += 9;
	}
}

static void _drawArraysVCTT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_colorElement(&push[3], i);
		_texCoordElement(&push[4], i, 0);
		_texCoordElement(&push[6], i, 1);
		push += 8;
	}
}

static void _drawArraysVCNTT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		_colorElement(&push[6], i);
		_texCoordElement(&push[7], i, 0);
		_texCoordElement(&push[9], i, 1);
		push += 11;
	}
}

static void _drawArraysVT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		push[3] = glw_state->currentColor;
		_texCoordElement(&push[4], i, 0);
		push += 6;
	}
}

static void _drawArraysVNT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		push[6] = glw_state->currentColor;
		_texCoordElement(&push[7], i, 0);
		push += 9;
	}
}

static void _drawArraysVTT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		push[3] = glw_state->currentColor;
		_texCoordElement(&push[4], i, 0);
		_texCoordElement(&push[6], i, 1);
		push += 8;
	}
}

static void _drawArraysVNTT(GLsizei first, GLsizei last)
{
	DWORD* push = glw_state->drawArray;
	for (int i = first; i < last; ++i)
	{
		_vertexElement(&push[0], i);
		_normalElement(&push[3], i);
		push[6] = glw_state->currentColor;
		_texCoordElement(&push[7], i, 0);
		_texCoordElement(&push[9], i, 1);
		push += 11;
	}
}

typedef void(*drawarrayfunc_t)(GLsizei, GLsizei);
static drawarrayfunc_t _drawArrayFuncTable[12] =
{
	_drawArraysV,
	_drawArraysVN,
	_drawArraysVT,
	_drawArraysVNT,
	_drawArraysVTT,
	_drawArraysVNTT,
	_drawArraysVC,
	_drawArraysVCN,
	_drawArraysVCT,
	_drawArraysVCNT,
	_drawArraysVCTT,
	_drawArraysVCNTT,
};


/*
=================
_getDrawFunc

Figure which drawing function we need based on
what vertex components we have.  Use the returned
integer to index the draw function tables.
=================
*/
static int _getDrawFunc(void)
{
	int func = 0;
	if (glw_state->colorArrayState) func += 6;
	if (glw_state->texCoordArrayState[0]) func += 2;
	if (glw_state->texCoordArrayState[1]) func += 2;
	if (glw_state->normalArrayState) ++func;
	return func;
}


static void dllAccum(GLenum op, GLfloat value)
{
	assert(false);
}

static void dllAlphaFunc(GLenum func, GLclampf ref)
{
	D3DCMPFUNC f = _convertCompare(func);
	glw_state->device->SetRenderState(D3DRS_ALPHAFUNC, f);
	glw_state->device->SetRenderState(D3DRS_ALPHAREF, (DWORD)(ref * 255.));
}

GLboolean dllAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences)
{
	assert(false);
	return 1;
}

static void dllArrayElement(GLint i)
{
	assert(glw_state->inDrawBlock);
	
	_handleDrawOverflow();

	DWORD* push = &glw_state->drawArray[glw_state->numVertices * 
		glw_state->drawStride];

	_vertexElement(push, i);
	push += 3;
	
	if (glw_state->colorArrayState)
	{
		_colorElement(push, i);
		++push;
	}
	else
	{
		*push++ = glw_state->currentColor;
	}
	
	for (int t = 0; t < GLW_MAX_TEXTURE_STAGES; ++t)
	{
		if (glw_state->texCoordArrayState[t])
		{
			_texCoordElement(push, i, t);
			push += 2;
		}
	}

	++glw_state->numVertices;
}

// EXTENSION: Begin a drawing block with at verts vertices
static void dllBeginEXT(GLenum mode, GLint verts, GLint colors, GLint normals, GLint tex0, GLint tex1)//, GLint tex2, GLint tex3)
{
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (g_SPXBPerfSampleActive) {
		s_xboxImmediateDrawStart = STEFX_XboxReadTsc();
		++g_SPXBPerfSubmitCallsCurrent;
		++g_SPXBPerfImmediateSubmitCallsCurrent;
	} else {
		s_xboxImmediateDrawStart = 0;
	}
#endif
	assert(!glw_state->inDrawBlock);

	// start the draw block
	glw_state->inDrawBlock = true;
	glw_state->primitiveMode = _convertPrimMode(mode);

	// update DX with any pending state changes
	_updateDrawStride(normals, tex0, tex1);//, tex2, tex3);
	_updateShader(normals, tex0, tex1);//, tex2, tex3);
	_updateTextures();
	_updateMatrices();

	// set vertex counters
	glw_state->numVertices = 0;
	glw_state->totalVertices = verts;
	glw_state->maxVertices = _getMaxVerts();	

#ifdef _XBOX
	// open a draw packet
	//int num_packets = ((verts * glw_state->drawStride) / GLW_MAX_DRAW_PACKET_SIZE) + 1;
	int num_packets;
	if(glw_state->maxVertices == 0) {
		num_packets = 1;
	} else {
		num_packets = (verts / glw_state->maxVertices) + (!!(verts % glw_state->maxVertices));
	}
	int cmd_size = num_packets * 3;
	int vert_size = glw_state->drawStride * verts;
	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (g_SPXBPerfSampleActive) {
		g_SPXBPerfImmediateReserveDwordsCurrent +=
			(unsigned int)(vert_size + cmd_size + 2);
	}
	#endif
	
	glw_state->device->BeginPush(vert_size + cmd_size + 2, 
		&glw_state->drawArray);
	
	glw_state->drawArray = _restartDrawPacket(
		glw_state->drawArray, glw_state->maxVertices);
#endif
}

static void dllBegin(GLenum mode)
{
	assert(0);
}

// EXTENSION: Start a new drawing frame
GLboolean dllBeginFrame(void)
{
	STEFX_D3D8_InvalidateTextureStageCache();
#ifdef _XBOX
	{
		static int previous = -1;
		const int active = STEFX_CoopLowResolution() ? 1 : 0;
		// Set scale before any drawing. Restore it when returning to ordinary SP.
		if (active || previous == 1)
			glw_state->device->SetBackBufferScale(active ? 0.8f : 1.0f,
				active ? 0.8f : 1.0f);
		if (active != previous)
		{
			float sx = 1.0f, sy = 1.0f;
			glw_state->device->GetBackBufferScale(&sx, &sy);
			int left, top, right, bottom;
			STEFX_GetSafeArea(&left, &top, &right, &bottom);
			g_SPXBCoopResolution[0] = 1;
			g_SPXBCoopResolution[1] = active;
			g_SPXBCoopResolution[2] = (unsigned int)(sx * 1000.0f + 0.5f);
			g_SPXBCoopResolution[3] = (unsigned int)(sy * 1000.0f + 0.5f);
			g_SPXBCoopResolution[4] = left;
			g_SPXBCoopResolution[5] = right;
			g_SPXBCoopResolution[6] = glConfig.vidWidth;
			g_SPXBCoopResolution[7] = glConfig.vidHeight;
			XBLF("STEFX_COOP_RESOLUTION: active=%d scale=%g,%g logical=%dx%d borders=%d,%d backing_buffers_unchanged=1",
				active, sx, sy, glConfig.vidWidth, glConfig.vidHeight, left, right);
			previous = active;
		}
	}
	STEFX_ScratchFrameBegin();
#endif
	GLboolean result = glw_state->device->BeginScene() == D3D_OK;
#ifdef _XBOX
	if (result)
	{
		int left, top, right, bottom;
		STEFX_GetSafeArea(&left, &top, &right, &bottom);
		if (left || top || right || bottom)
		{
			glw_state->device->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
		}
	}
#endif
	return result;
}

// EXTENSION: Begin shadow draw mode
static void dllBeginShadow(void)
{
	//Intentionally left blank
}

static void dllBindTexture(GLenum target, GLuint texture)
{
	assert(target == GL_TEXTURE_2D);

	if (glw_state->currentTexture[glw_state->serverTU] != texture)
	{
		glw_state->currentTexture[glw_state->serverTU] = texture;
		glw_state->textureStageDirty[glw_state->serverTU] = true;
	}
}

static void dllBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	assert(false);
}

static void dllBlendFunc(GLenum sfactor, GLenum dfactor)
{
	D3DBLEND s = _convertBlendFactor(sfactor);
	D3DBLEND d = _convertBlendFactor(dfactor);
	
	glw_state->device->SetRenderState(D3DRS_SRCBLEND, s);
	glw_state->device->SetRenderState(D3DRS_DESTBLEND, d);
}

static void dllCallList(GLuint lnum)
{
	assert(0);
}

static void dllCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	assert(0);
}

static void dllClear(GLbitfield mask)
{
	DWORD m = 0;

	if (mask & GL_COLOR_BUFFER_BIT) m |= D3DCLEAR_TARGET;
	if (mask & GL_STENCIL_BUFFER_BIT) m |= D3DCLEAR_STENCIL;

#ifdef _XBOX
	// Clearing stencil when clearing depth buffer
	// is faster on Xbox than just clearing depth alone.
	if (mask & GL_DEPTH_BUFFER_BIT) m |= D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL;
#else
	if (mask & GL_DEPTH_BUFFER_BIT) m |= D3DCLEAR_ZBUFFER;
#endif
	
	glw_state->device->Clear(0, NULL, m, glw_state->clearColor, 
		glw_state->clearDepth, glw_state->clearStencil);
}

static void dllClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	assert(0);
}

static void dllClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	glw_state->clearColor = D3DCOLOR_COLORVALUE(red, green, blue, alpha);
}

static void dllClearDepth(GLclampd depth)
{
	glw_state->clearDepth = depth;
}

static void dllClearIndex(GLfloat c)
{
	assert(0);
}

static void dllClearStencil(GLint s)
{
	glw_state->clearStencil = s;
}

static void dllClipPlane(GLenum plane, const GLdouble *equation)
{
	//FIXME
}

static void setIntColor(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	glw_state->currentColor = D3DCOLOR_RGBA(red, green, blue, alpha);
}

static void setFloatColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	glw_state->currentColor = D3DCOLOR_COLORVALUE(red, green, blue, alpha);
}

static void dllColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3bv(const GLbyte *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	setFloatColor(red, green, blue, 1.f);
}

static void dllColor3dv(const GLdouble *v)
{
	setFloatColor(v[0], v[1], v[2], 1.f);
}

static void dllColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	setFloatColor(red, green, blue, 1.f);
}

static void dllColor3fv(const GLfloat *v)
{
	setFloatColor(v[0], v[1], v[2], 1.f);
}

static void dllColor3i(GLint red, GLint green, GLint blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3iv(const GLint *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor3s(GLshort red, GLshort green, GLshort blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3sv(const GLshort *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3ubv(const GLubyte *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor3ui(GLuint red, GLuint green, GLuint blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3uiv(const GLuint *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor3us(GLushort red, GLushort green, GLushort blue)
{
	setIntColor(red, green, blue, 127);
}

static void dllColor3usv(const GLushort *v)
{
	setIntColor(v[0], v[1], v[2], 127);
}

static void dllColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4bv(const GLbyte *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	setFloatColor(red, green, blue, alpha);
}

static void dllColor4dv(const GLdouble *v)
{
	setFloatColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	setFloatColor(red, green, blue, alpha);
}

static void dllColor4fv(const GLfloat *v)
{
	setFloatColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4iv(const GLint *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4sv(const GLshort *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4ubv(const GLubyte *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4uiv(const GLuint *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	setIntColor(red, green, blue, alpha);
}

static void dllColor4usv(const GLushort *v)
{
	setIntColor(v[0], v[1], v[2], v[3]);
}

static void dllColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	DWORD m = 0;
	if (red) m |= D3DCOLORWRITEENABLE_RED;
	if (green) m |= D3DCOLORWRITEENABLE_GREEN;
	if (blue) m |= D3DCOLORWRITEENABLE_BLUE;
	if (alpha) m |= D3DCOLORWRITEENABLE_ALPHA;
	glw_state->device->SetRenderState(D3DRS_COLORWRITEENABLE, m);
}

static void dllColorMaterial(GLenum face, GLenum mode)
{
	assert(0);
}

static void dllColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	assert(!glw_state->inDrawBlock);
	assert(size == 4 && type == GL_UNSIGNED_BYTE);
	
	stride = (stride == 0) ? sizeof(GLint) : stride;

	glw_state->colorPointer = pointer; 
	glw_state->colorStride = stride;
}

static void dllCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	assert(0);
}

static void dllCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
	assert(0);
}

/**********
copies a portion of the backbuffer to the current texture.
the current texture must be a linear format texture, if
a swizzled texture format is needed, use
dllCopyBackBufferToTexEXT
**********/
static void dllCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	// check to make sure everything passed in is supported
	assert((target == GL_TEXTURE_2D) && (level == 0) && (border == 0));

	// locals
	RECT						rSrc;
	POINT						ptUpperLeft;
	LPDIRECT3DSURFACE8			tSurf;
	LPDIRECT3DSURFACE8			backbuffer;
	glwstate_t::TextureInfo*	tex;
	HRESULT						res;
		
	// get the current texture
	tex = _getCurrentTexture(glw_state->serverTU);
	if (tex == NULL)
	{
		return;
	}

	// set up the source rectangle
	rSrc.left	= x;
	rSrc.right	= x + width;
	rSrc.top	= (480 - y) - height;
	rSrc.bottom	= (480 - y);

	// set up the target point
	ptUpperLeft.x	= 0;
	ptUpperLeft.y	= 0;

	// attach the current texture to a surface
	tex->mipmap->GetSurfaceLevel(0, &tSurf);

	// attach the back buffer to a surface
	res = glw_state->device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

	// copy the data
	res = glw_state->device->CopyRects(backbuffer, &rSrc, 0, tSurf, &ptUpperLeft);

	// release surfaces
	tSurf->Release();
	backbuffer->Release();
}

static void dllCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	assert(0);
}

static void dllCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	assert(0);
}

static void dllCullFace(GLenum mode)
{
	switch (mode)
	{
	default: case GL_BACK: glw_state->cullMode = D3DCULL_CW; break;
	case GL_FRONT: glw_state->cullMode = D3DCULL_CCW; break;
	}

	glw_state->device->SetRenderState(D3DRS_CULLMODE, glw_state->cullMode);
}

static void dllDeleteLists(GLuint lnum, GLsizei range)
{
	assert(0);
}

static void dllDeleteTextures(GLsizei n, const GLuint *textures)
{
	glw_state->textureStageDirty[0] = true;
	glw_state->textureStageDirty[1] = true;
	glw_state->device->SetTexture(0, NULL);
	glw_state->device->SetTexture(1, NULL);
//	glw_state->device->SetTexture(2, NULL);
//	glw_state->device->SetTexture(3, NULL);

	for (int t = 0; t < n; ++t)
	{
		glwstate_t::texturexlat_t::iterator i = 
			glw_state->textureXlat.find(textures[t]);

		if (i != glw_state->textureXlat.end())
		{
#if MEMORY_PROFILE
			texMemSize -= getTexMemSize(i->second.mipmap);
#endif
			i->second.mipmap->BlockUntilNotBusy();
			delete i->second.mipmap;
			glw_state->textureXlat.erase(i);
		}
	}
}

static void dllDepthFunc(GLenum func)
{
	D3DCMPFUNC f = _convertCompare(func);
	glw_state->device->SetRenderState(D3DRS_ZFUNC, f);
}

static void dllDepthMask(GLboolean flag)
{
	glw_state->device->SetRenderState(D3DRS_ZWRITEENABLE, flag);
}

static void dllDepthRange(GLclampd zNear, GLclampd zFar)
{
	glw_state->viewport.MinZ = zNear;
	glw_state->viewport.MaxZ = zFar;
	glw_state->device->SetViewport(&glw_state->viewport);
}

#ifdef _XBOX
// Preserve scan mode and aspect ratio when changing the presentation interval.
// Ratio of displayed pixel width to pixel height for the active framebuffer.
float GLW_GetPixelAspect(void)
{
    if (!glw_state || !glw_state->isWidescreen || glConfig.vidWidth <= 0 || glConfig.vidHeight <= 0)
        return 1.0f;
    const float displayAspect = STEFX_CoopLowResolution() ? (4.0f / 3.0f) : (16.0f / 9.0f);
    return displayAspect * (float)glConfig.vidHeight / (float)glConfig.vidWidth;
}

static DWORD s_xboxPresentationFlags = 0;
static void setPresent(bool vsync)
{
	//extern void ShowOSMemory();
	//ShowOSMemory();

	D3DPRESENT_PARAMETERS pp;
	pp.BackBufferWidth = glConfig.vidWidth;
	pp.BackBufferHeight = glConfig.vidHeight;
	pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	pp.BackBufferCount = 1;
	pp.MultiSampleType  = D3DMULTISAMPLE_NONE; //D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_LINEAR;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = 0;
	pp.Windowed  = FALSE;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;
	pp.Flags = s_xboxPresentationFlags;
	pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	pp.FullScreen_PresentationInterval = 
		vsync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	pp.BufferSurfaces[0] = pp.BufferSurfaces[1] = pp.BufferSurfaces[2] = 0;
	pp.DepthStencilSurface = 0;
	glw_state->device->PersistDisplay();
	glw_state->device->Reset(&pp);

	//ShowOSMemory();
}
#endif

static void setCap(GLenum cap, bool flag)
{
	switch (cap)
	{
	case GL_ALPHA_TEST: glw_state->device->SetRenderState(D3DRS_ALPHATESTENABLE, flag); break;
	case GL_BLEND: glw_state->device->SetRenderState(D3DRS_ALPHABLENDENABLE, flag); break;
	case GL_CULL_FACE:
		glw_state->cullEnable = flag;
		glw_state->device->SetRenderState(D3DRS_CULLMODE, 
			flag ? glw_state->cullMode : D3DCULL_NONE);
		break;
	case GL_DEPTH_TEST: glw_state->device->SetRenderState(D3DRS_ZENABLE, flag); break;
	case GL_LIGHTING: glw_state->device->SetRenderState(D3DRS_LIGHTING, flag); break;
#ifdef _XBOX
	case GL_POLYGON_OFFSET_POINT:
		glw_state->device->SetRenderState(D3DRS_POINTOFFSETENABLE, flag);
		break;
	case GL_POLYGON_OFFSET_LINE:
		glw_state->device->SetRenderState(D3DRS_WIREFRAMEOFFSETENABLE, flag);
		break;
	case GL_POLYGON_OFFSET_FILL:
		glw_state->device->SetRenderState(D3DRS_SOLIDOFFSETENABLE, flag);
		break;
	case GL_SCISSOR_TEST:
		glw_state->scissorEnable = flag;
		glw_state->device->SetScissors(flag ? 1 : 0, FALSE, &glw_state->scissorBox);
		break;
#endif
	case GL_STENCIL_TEST: glw_state->device->SetRenderState(D3DRS_STENCILENABLE, flag); break;
	case GL_TEXTURE_2D: 
		glw_state->textureStageEnable[glw_state->serverTU] = flag;
		glw_state->textureStageDirty[glw_state->serverTU] = true;
		#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
		memset( s_stefxTextureStageStateValid[glw_state->serverTU], 0,
			sizeof( s_stefxTextureStageStateValid[glw_state->serverTU] ) );
		#endif
		break;
	case GL_FOG:
		glw_state->device->SetRenderState(D3DRS_FOGENABLE, flag);
		break;
#ifdef _XBOX
	case GL_VSYNC:
		setPresent(flag);
		break;
#endif
	default: break;
	}
}

static void dllDisable(GLenum cap)
{
	setCap(cap, false);
}

static void setArrayState(GLenum cap, bool state)
{
	switch (cap)
	{
	case GL_COLOR_ARRAY: glw_state->colorArrayState = state; break; 
	case GL_TEXTURE_COORD_ARRAY: glw_state->texCoordArrayState[glw_state->clientTU] = state; break;
	case GL_VERTEX_ARRAY: glw_state->vertexArrayState = state; break;
	case GL_NORMAL_ARRAY: glw_state->normalArrayState = state; break;
	}
}

static void dllDisableClientState(GLenum array)
{
	assert(!glw_state->inDrawBlock);
	setArrayState(array, false);
}

#ifdef _WINDOWS
static void _convertQuadsToTris(GLint first, GLsizei count)
{
	glw_state->vertexPointerBack = glw_state->vertexPointer;
	glw_state->normalPointerBack = glw_state->normalPointer;
	glw_state->colorPointerBack = glw_state->colorPointer;
	glw_state->texCoordPointerBack[0] = glw_state->texCoordPointer[0];
	glw_state->texCoordPointerBack[1] = glw_state->texCoordPointer[1];
	
	{
		glw_state->vertexPointer = 
			Z_Malloc(count * glw_state->vertexStride * 3 / 2, 
			TAG_TEMP_WORKSPACE, qfalse);
		for (int i = 0; i < count; i += 4)
		{
			int stride = glw_state->vertexStride / sizeof(float);
			float* dst = (float*)glw_state->vertexPointer + (i * 3 / 2) * stride;
			const float* src = (const float*)glw_state->vertexPointerBack + 
				(first + i) * stride;
			
			for (int j = 0; j < 3; ++j)
			{
				dst[0 * stride + j] = src[0 * stride + j];
				dst[1 * stride + j] = src[1 * stride + j];
				dst[2 * stride + j] = src[2 * stride + j];
				dst[3 * stride + j] = src[0 * stride + j];
				dst[4 * stride + j] = src[2 * stride + j];
				dst[5 * stride + j] = src[3 * stride + j];
			}
		}
	}
	
	if (glw_state->normalArrayState)
	{
		glw_state->normalPointer = 
			Z_Malloc(count * glw_state->normalStride * 3 / 2, 
			TAG_TEMP_WORKSPACE, qfalse);
		
		for (int i = 0; i < count; i += 4)
		{
			int stride = glw_state->normalStride / sizeof(float);
			float* dst = (float*)glw_state->normalPointer + (i * 3 / 2) * stride;
			const float* src = (const float*)glw_state->normalPointerBack + 
				(first + i) * stride;
			
			for (int j = 0; j < 3; ++j)
			{
				dst[0 * stride + j] = src[0 * stride + j];
				dst[1 * stride + j] = src[1 * stride + j];
				dst[2 * stride + j] = src[2 * stride + j];
				dst[3 * stride + j] = src[0 * stride + j];
				dst[4 * stride + j] = src[2 * stride + j];
				dst[5 * stride + j] = src[3 * stride + j];
			}
		}
	}
	
	if (glw_state->colorArrayState)
	{
		glw_state->colorPointer = 
			Z_Malloc(count * glw_state->colorStride * 3 / 2, 
			TAG_TEMP_WORKSPACE, qfalse);
		
		for (int i = 0; i < count; i += 4)
		{
			int stride = glw_state->colorStride / sizeof(DWORD);
			DWORD* dst = (DWORD*)glw_state->colorPointer + (i * 3 / 2) * stride;
			const DWORD* src = (const DWORD*)glw_state->colorPointerBack + 
				(first + i) * stride;
			
			dst[0 * stride] = src[0 * stride];
			dst[1 * stride] = src[1 * stride];
			dst[2 * stride] = src[2 * stride];
			dst[3 * stride] = src[0 * stride];
			dst[4 * stride] = src[2 * stride];
			dst[5 * stride] = src[3 * stride];
		}
	}
	
	for (int t = 0; t < GLW_MAX_TEXTURE_STAGES; ++t)
	{
		if (glw_state->texCoordArrayState[t])
		{
			glw_state->texCoordPointer[t] = 
				Z_Malloc(count * glw_state->texCoordStride[t] * 3 / 2, 
				TAG_TEMP_WORKSPACE, qfalse);
			
			for (int i = 0; i < count; i += 4)
			{
				int stride = glw_state->texCoordStride[t] / sizeof(float);
				float* dst = (float*)glw_state->texCoordPointer[t] + (i * 3 / 2) * stride;
				const float* src = (const float*)glw_state->texCoordPointerBack[t] + 
					(first + i) * stride;
				
				for (int j = 0; j < 2; ++j)
				{
					dst[0 * stride + j] = src[0 * stride + j];
					dst[1 * stride + j] = src[1 * stride + j];
					dst[2 * stride + j] = src[2 * stride + j];
					dst[3 * stride + j] = src[0 * stride + j];
					dst[4 * stride + j] = src[2 * stride + j];
					dst[5 * stride + j] = src[3 * stride + j];
				}
			}
		}
	}
}

static void _cleanupQuadsToTris(void)
{
	Z_Free(const_cast<void*>(glw_state->vertexPointer));
	glw_state->vertexPointer = glw_state->vertexPointerBack;
	
	if (glw_state->normalArrayState)
	{
		Z_Free(const_cast<void*>(glw_state->normalPointer));
		glw_state->normalPointer = glw_state->normalPointerBack;
	}
	
	if (glw_state->colorArrayState)
	{
		Z_Free(const_cast<void*>(glw_state->colorPointer));
		glw_state->colorPointer = glw_state->colorPointerBack;
	}

	for (int t = 0; t < GLW_MAX_TEXTURE_STAGES; ++t)
	{
		if (glw_state->texCoordArrayState[t])
		{
			Z_Free(const_cast<void*>(glw_state->texCoordPointer[t]));
			glw_state->texCoordPointer[t] = glw_state->texCoordPointerBack[t];
		}
	}
}
#endif

// NOTE: This is a core draw routine.  It should be fast.
static void dllDrawArrays(GLenum mode, GLint first, GLsizei count)
{
#ifdef _WINDOWS
	if (mode == GL_QUADS)
	{
		_convertQuadsToTris(first, count);
		count = count * 3 / 2;
		first = 0;
	}
#endif

	// start the draw mode
	qglBeginEXT(mode, count, glw_state->colorArrayState ? count : 0, 
		glw_state->normalArrayState ? count : 0,
		glw_state->texCoordArrayState[0] ? count : 0,
		glw_state->texCoordArrayState[1] ? count : 0);

	// get the draw function we need
	drawarrayfunc_t func = _drawArrayFuncTable[_getDrawFunc()];

#ifndef _XBOX
	DWORD* base = glw_state->drawArray;
#endif

	int inc = glw_state->maxVertices;
	// loop taking care not to draw too much at a time
	for (int start = first; ; start += inc)//glw_state->maxVertices)
	{
		// draw glw_state->maxVertices amount of geometry
		func(start, start + glw_state->maxVertices);
		
		// are we done yet?
		glw_state->totalVertices -= glw_state->maxVertices;
		if (glw_state->totalVertices <= 0)
		{
			glw_state->numVertices = glw_state->maxVertices;
			break;
		}
		
		// ready for another cycle
		glw_state->drawArray += glw_state->maxVertices *
			glw_state->drawStride;
		glw_state->maxVertices = _getMaxVerts();

		glw_state->drawArray = _restartDrawPacket(
			glw_state->drawArray, glw_state->maxVertices); 
	}
	
#ifndef _XBOX
	glw_state->drawArray = base;
#endif

#ifdef _WINDOWS
	if (mode == GL_QUADS)
	{
		_cleanupQuadsToTris();
	}
#endif

	// finish up the draw
	qglEnd();
}

static void dllDrawBuffer(GLenum mode)
{
	//FIXME
}


static void PushIndices(GLsizei count, const GLushort *indices)
{
	// open the index packet
	// can only send 2047 indices thru at a time
	// BUT, Microsoft recommends 511 pairs at a time (?)
	int num_packets, numpairs;
	bool singleindex = false;

	numpairs = count / 2;

	if(numpairs <= 511) 
	{
		num_packets = 1;

		if(glw_state->maxIndices % 2)
		{
			glw_state->maxIndices -= 1;
			singleindex = true;
		}
	} else 
	{
		num_packets = (count / glw_state->maxIndices) + (!!(count % glw_state->maxIndices));
	}

	glw_state->drawArray = _restartIndexPacket(glw_state->drawArray, glw_state->maxIndices);

	int inc = glw_state->maxIndices;
	for (int start = 0; ; start += inc)
	{
		// memcpy is faster than looping copy:
		memcpy( glw_state->drawArray, indices+start, glw_state->maxIndices * sizeof(WORD) );
		glw_state->drawArray += glw_state->maxIndices / 2;

		/*
		for(int i = start; i < start + glw_state->maxIndices; i += 2)
		{
			*glw_state->drawArray++ = (DWORD)(((WORD)indices[i + 1] << 16) + (WORD)indices[i]);
		}
		*/

		// are we done yet?
		glw_state->totalIndices -= glw_state->maxIndices;
		if (glw_state->totalIndices <= 1)
		{
			glw_state->numIndices = glw_state->maxIndices;
			break;
		}

		// ready for another cycle
		//glw_state->drawArray += glw_state->maxVertices * glw_state->drawStride;
		glw_state->maxIndices = _getMaxIndices();

		if(glw_state->maxIndices % 2)
		{
			glw_state->maxIndices -= 1;
			singleindex = true;
		}

		glw_state->drawArray = _restartIndexPacket(glw_state->drawArray, glw_state->maxIndices);
	}

#define CMD_DRAW_INDEX_LAST 0x1808 
	if(singleindex)
	{
		*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_DRAW_INDEX_LAST, 1);
		*glw_state->drawArray++ = indices[count - 1];
	}
}

// NOTE: This is a core draw routine.  It should be fast.
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBPerfDrawKindsCurrent[48];
extern "C" volatile unsigned int g_SPXBPerfModelDrawsCurrent[2056];
// STEFX_MODEL_DRAW_PROFILE_BEGIN
static void STEFX_RecordModelDraw(unsigned int handle, unsigned int shader,
    unsigned int model, unsigned int vertices, unsigned int indices,
    unsigned int cycles, unsigned int beginCycles)
{
    // Power-of-two hash table bounds diagnostic lookup work; the last row
    // always retains totals for keys that do not fit. Never evict frame data.
    unsigned int slot = ((handle * 2654435761u) ^ (shader >> 4)) & 255u;
    unsigned int probe;
    for (probe = 0; probe < 256; ++probe) {
        volatile unsigned int *candidate = g_SPXBPerfModelDrawsCurrent + slot * 8;
        if (!candidate[2] || (candidate[0] == handle && candidate[1] == shader)) break;
        slot = (slot + 1u) & 255u;
    }
    if (probe == 256) slot = 256;
    volatile unsigned int *row = g_SPXBPerfModelDrawsCurrent + slot * 8;
    row[0] = slot == 256 ? 0xffffffffu : handle;
    row[1] = slot == 256 ? 0 : shader;
    ++row[2]; row[3] += vertices; row[4] += indices;
    row[5] += cycles; row[6] += beginCycles;
    row[7] = slot == 256 ? 0 : model;
}
// STEFX_MODEL_DRAW_PROFILE_END
#endif
#include "stefx_coop_model_resident.h"
static void dllDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	int normals, tex0, tex1, num_streams = 2;
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440000; /* 'ND00': entry */
	++g_SPXBNativeSubmitSerial;
	g_SPXBNativeSubmitCount = (unsigned int)count;
	g_SPXBNativeSubmitVerts = (unsigned int)tess.numVertexes;
	g_SPXBNativeSubmitState = (unsigned int)glState.glStateBits;
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( g_SPXBHMScoreSubmitArmed ) {
		++g_SPXBHMScoreSubmitCalls;
		g_SPXBHMScoreSubmitIndexes = (unsigned int)count;
		g_SPXBHMScoreSubmitState = (unsigned int)glState.glStateBits;
		g_SPXBHMScoreSubmitTexture = (unsigned int)glw_state->currentTexture[0];
		g_SPXBHMScoreSubmitScissor = glw_state->scissorEnable ? 1u : 0u;
		g_SPXBHMScoreSubmitScissorXY =
			((unsigned int)(glw_state->scissorBox.x1 & 0xffff)) |
			((unsigned int)(glw_state->scissorBox.y1 & 0xffff) << 16);
		g_SPXBHMScoreSubmitScissorWH =
			((unsigned int)((glw_state->scissorBox.x2 - glw_state->scissorBox.x1) & 0xffff)) |
			((unsigned int)((glw_state->scissorBox.y2 - glw_state->scissorBox.y1) & 0xffff) << 16);
	}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	const int xboxPerfSample = g_SPXBPerfSampleActive ? 1 : 0;
	unsigned __int64 xboxDrawStart = 0;
	unsigned __int64 xboxPhaseStart = 0;
	unsigned int xboxPhaseCycles = 0;
	const unsigned int kindBeginBefore = g_SPXBPerfDrawBeginPushCyclesCurrent;
	const unsigned int kindPackBefore = g_SPXBPerfDrawPackCyclesCurrent;
	const unsigned int kindStateBefore = g_SPXBPerfDrawStateCyclesCurrent;
	if (xboxPerfSample) {
		xboxDrawStart = STEFX_XboxReadTsc();
		++g_SPXBPerfSubmitCallsCurrent;
		++g_SPXBPerfIndexedSubmitCallsCurrent;
	}
#endif

	assert(type == GL_UNSIGNED_SHORT);

	normals = glw_state->normalArrayState ? tess.numVertexes : 0;
	tex0 = glw_state->texCoordArrayState[0] ? tess.numVertexes : 0;
	tex1 = glw_state->texCoordArrayState[1] ? tess.numVertexes : 0;

	num_streams += ((normals > 0) + (tex0 > 0) + (tex1 > 0));
#ifdef _XBOX
	g_SPXBNativeSubmitStreams = ((unsigned int)num_streams & 0xffu) |
		(tex0 ? 0x100u : 0u) | (tex1 ? 0x200u : 0u) | (normals ? 0x400u : 0u);
#endif

	// start the draw block
	glw_state->inDrawBlock = true;
	glw_state->primitiveMode = _convertPrimMode(mode);

	// update DX with any pending state changes
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) xboxPhaseStart = STEFX_XboxReadTsc();
#endif
	_updateDrawStride(normals, tex0, tex1);
	_updateShader(normals, tex0, tex1);
	_updateTextures();
	_updateMatrices();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_TraceStasisD3D();
#endif
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440001; /* 'ND01': state ready */
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	STEFX_TraceShaderRuntimeDraw( count, (const GLushort *)indices,
		normals ? qtrue : qfalse, tex0 ? qtrue : qfalse, tex1 ? qtrue : qfalse );
#endif
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( g_SPXBHMScoreSubmitArmed ) {
		IDirect3DSurface8 *renderTarget = NULL;
		IDirect3DSurface8 *backBuffer = NULL;
		glwstate_t::TextureInfo *textureInfo = _getCurrentTexture( 0 );
		D3DSURFACE_DESC textureDesc;
		D3DMATRIX projection;
		DWORD colorWrite = 0;
		DWORD cullMode = 0;
		DWORD alphaBlend = 0;
		DWORD srcBlend = 0;
		DWORD dstBlend = 0;
		DWORD colorOp = 0;
		DWORD colorArg1 = 0;
		DWORD colorArg2 = 0;
		DWORD alphaOp = 0;
		DWORD alphaArg1 = 0;
		DWORD alphaArg2 = 0;
		DWORD zEnable = 0;
		DWORD zWrite = 0;
		DWORD zFunc = 0;
		DWORD vertexShader = 0;
		DWORD pixelShader = 0;
		union { float f; unsigned int u; } bits;
		memset( &textureDesc, 0, sizeof(textureDesc) );
		glw_state->device->GetRenderTarget( &renderTarget );
		glw_state->device->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer );
		g_SPXBHMScoreSubmitTarget = ( renderTarget ? 1u : 0u ) |
			( backBuffer ? 2u : 0u ) | ( renderTarget == backBuffer ? 4u : 0u );
		glw_state->device->GetRenderState( D3DRS_COLORWRITEENABLE, &colorWrite );
		glw_state->device->GetRenderState( D3DRS_CULLMODE, &cullMode );
		glw_state->device->GetRenderState( D3DRS_ALPHABLENDENABLE, &alphaBlend );
		glw_state->device->GetRenderState( D3DRS_SRCBLEND, &srcBlend );
		glw_state->device->GetRenderState( D3DRS_DESTBLEND, &dstBlend );
		glw_state->device->GetRenderState( D3DRS_ZENABLE, &zEnable );
		glw_state->device->GetRenderState( D3DRS_ZWRITEENABLE, &zWrite );
		glw_state->device->GetRenderState( D3DRS_ZFUNC, &zFunc );
		glw_state->device->GetTextureStageState( 0, D3DTSS_COLOROP, &colorOp );
		glw_state->device->GetTextureStageState( 0, D3DTSS_COLORARG1, &colorArg1 );
		glw_state->device->GetTextureStageState( 0, D3DTSS_COLORARG2, &colorArg2 );
		glw_state->device->GetTextureStageState( 0, D3DTSS_ALPHAOP, &alphaOp );
		glw_state->device->GetTextureStageState( 0, D3DTSS_ALPHAARG1, &alphaArg1 );
		glw_state->device->GetTextureStageState( 0, D3DTSS_ALPHAARG2, &alphaArg2 );
		glw_state->device->GetVertexShader( &vertexShader );
		glw_state->device->GetPixelShader( &pixelShader );
		g_SPXBHMScoreSubmitColorWrite = (unsigned int)colorWrite;
		g_SPXBHMScoreSubmitCull = (unsigned int)cullMode;
		g_SPXBHMScoreSubmitBlend = ((unsigned int)alphaBlend & 0xffu) |
			(((unsigned int)srcBlend & 0xffu) << 8) |
			(((unsigned int)dstBlend & 0xffu) << 16);
		g_SPXBHMScoreStageColor = ((unsigned int)colorOp & 0xffu) |
			(((unsigned int)colorArg1 & 0xffu) << 8) |
			(((unsigned int)colorArg2 & 0xffu) << 16);
		g_SPXBHMScoreStageAlpha = ((unsigned int)alphaOp & 0xffu) |
			(((unsigned int)alphaArg1 & 0xffu) << 8) |
			(((unsigned int)alphaArg2 & 0xffu) << 16);
		g_SPXBHMScoreDepthState = ((unsigned int)zEnable & 0xffu) |
			(((unsigned int)zWrite & 0xffu) << 8) |
			(((unsigned int)zFunc & 0xffu) << 16);
		g_SPXBHMScoreVertexShader = (unsigned int)vertexShader;
		g_SPXBHMScorePixelShader = (unsigned int)pixelShader;
		if ( textureInfo ) {
			g_SPXBHMScoreTextureSize = (unsigned int)textureInfo->size;
			g_SPXBHMScoreTextureData0 = textureInfo->data ?
				*((const unsigned int *)textureInfo->data) : 0u;
			if ( textureInfo->mipmap &&
				SUCCEEDED( textureInfo->mipmap->GetLevelDesc( 0, &textureDesc ) ) ) {
				g_SPXBHMScoreTextureWH = ((unsigned int)textureDesc.Width & 0xffffu) |
					(((unsigned int)textureDesc.Height & 0xffffu) << 16);
				g_SPXBHMScoreTextureFormat = (unsigned int)textureDesc.Format;
			}
		}
		g_SPXBHMScoreVertexCount = (unsigned int)tess.numVertexes;
		g_SPXBHMScoreVertexStride = (unsigned int)glw_state->drawStride;
		if ( tess.numVertexes > 0 ) {
			bits.f = tess.xyz[0][0]; g_SPXBHMScoreVertex0X = bits.u;
			bits.f = tess.xyz[0][1]; g_SPXBHMScoreVertex0Y = bits.u;
			bits.f = tess.xyz[0][2]; g_SPXBHMScoreVertex0Z = bits.u;
			bits.f = tess.xyz[0][3]; g_SPXBHMScoreVertex0W = bits.u;
			g_SPXBHMScoreVertex0Color = glw_state->colorArrayState ?
				(unsigned int)tess.svars.colors[0] : (unsigned int)glw_state->currentColor;
			if ( tex0 ) {
				bits.f = tess.svars.texcoords[0][0][0]; g_SPXBHMScoreVertex0U = bits.u;
				bits.f = tess.svars.texcoords[0][0][1]; g_SPXBHMScoreVertex0V = bits.u;
			}
		}
		if ( count >= 3 ) {
			const GLushort *scoreIndexes = (const GLushort *)indices;
			g_SPXBHMScoreIndex012 = ((unsigned int)scoreIndexes[0] & 0x3ffu) |
				(((unsigned int)scoreIndexes[1] & 0x3ffu) << 10) |
				(((unsigned int)scoreIndexes[2] & 0x3ffu) << 20);
		}
		g_SPXBHMScoreSubmitViewportXY = ((unsigned int)glw_state->viewport.X & 0xffffu) |
			(((unsigned int)glw_state->viewport.Y & 0xffffu) << 16);
		g_SPXBHMScoreSubmitViewportWH = ((unsigned int)glw_state->viewport.Width & 0xffffu) |
			(((unsigned int)glw_state->viewport.Height & 0xffffu) << 16);
		glw_state->device->GetTransform( D3DTS_PROJECTION, &projection );
		bits.f = projection._11; g_SPXBHMScoreSubmitProj00 = bits.u;
		bits.f = projection._22; g_SPXBHMScoreSubmitProj11 = bits.u;
		bits.f = projection._41; g_SPXBHMScoreSubmitProj30 = bits.u;
		bits.f = projection._42; g_SPXBHMScoreSubmitProj31 = bits.u;
		if ( renderTarget ) renderTarget->Release();
		if ( backBuffer ) backBuffer->Release();
	}
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawStateCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
	}
#endif

	glw_state->drawStride += normals ? 2 : 1;

	glw_state->numIndices = 0;
	glw_state->totalIndices = count;
	glw_state->maxIndices = _getMaxIndices();

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) xboxPhaseStart = STEFX_XboxReadTsc();
#endif
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440002; /* 'ND02': set stream source */
#endif
	STEFX_D3D8_SetStreamSourceZeroTracked( glw_state->drawStride * 4 );
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440003; /* 'ND03': stream source ready */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawSetStreamCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
	}
#endif

	DWORD *modelPayload = NULL;
	int payloadVertices = tess.numVertexes;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	modelPayload = (DWORD *)STEFX_CoopModelClaim(mode,count,(const GLushort *)indices,normals,tex0,tex1);
#endif
	int vert_size = glw_state->drawStride * payloadVertices;
	int index_size = count / 2;
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP))
	stefxWorldVertex_t *worldPayload = STEFX_WorldVerticesClaim(mode, count,
		(const GLushort *)indices, normals, tex0, tex1);
	if (worldPayload && s_worldArraysActive) index_size = s_worldArrayCommandCount;
#else
	DWORD *worldPayload = NULL;
#endif
	DWORD *residentPayload = worldPayload ? (DWORD *)worldPayload : modelPayload;
#ifdef _XBOX
	DWORD *scratchPayload = NULL;
	bool interleavedPayload = false;
	unsigned int interleavedStride = 0;
#if defined(STEFX_SP_HOSTED_MP)
	stefxPackedLayout_t packedLayout;
	if (!worldPayload && STEFX_InterleavedWanted()) {
		interleavedPayload = true;
		packedLayout = STEFX_PackedLayout(normals, tex0, tex1);
		interleavedStride = packedLayout.stride;
		++g_SPXBInterleavedVertices[7];
		g_SPXBInterleavedVertices[2] += vert_size * 4 - interleavedStride * tess.numVertexes;
		vert_size = (interleavedStride / 4) * tess.numVertexes;
	}
#endif
	if (!residentPayload && s_stefxScratchFrameActive)
	{
		scratchPayload = STEFX_ScratchClaim((unsigned int)vert_size);
	}
	int push_reserve_dwords = (residentPayload || scratchPayload)
		? (index_size + 60)
		: (vert_size + index_size + 60);
	g_SPXBNativeSubmitReserve = (unsigned int)push_reserve_dwords;
#else
	bool interleavedPayload = false;
	unsigned int interleavedStride = 0;
	int push_reserve_dwords = vert_size + index_size + 60;
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned int xboxStateBits = 0;
	if (xboxPerfSample) {
		xboxStateBits = (unsigned int)glState.glStateBits;
		g_SPXBPerfIndexedReserveDwordsCurrent +=
			(unsigned int)push_reserve_dwords;
		if (tex1) {
			++g_SPXBPerfIndexedTex1CallsCurrent;
		}
		if (xboxStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) {
			++g_SPXBPerfIndexedBlendCallsCurrent;
			g_SPXBPerfIndexedBlendIndexesCurrent += (unsigned int)count;
		} else {
			++g_SPXBPerfIndexedOpaqueCallsCurrent;
		}
		if (xboxStateBits & GLS_ATEST_BITS) {
			++g_SPXBPerfIndexedAlphaTestCallsCurrent;
			g_SPXBPerfIndexedAlphaTestIndexesCurrent += (unsigned int)count;
		}
		if (!(xboxStateBits & GLS_DEPTHMASK_TRUE)) {
			++g_SPXBPerfIndexedNoDepthWriteCallsCurrent;
			g_SPXBPerfIndexedNoDepthWriteIndexesCurrent += (unsigned int)count;
		}
		if (xboxStateBits & GLS_DEPTHTEST_DISABLE) {
			++g_SPXBPerfIndexedNoDepthTestCallsCurrent;
		}
		if (glState.faceCulling == CT_TWO_SIDED) {
			++g_SPXBPerfIndexedTwoSidedCallsCurrent;
			g_SPXBPerfIndexedTwoSidedIndexesCurrent += (unsigned int)count;
		}
		#if defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
		STEFX_RecordSplitWorldPayloadReuse( mode, count,
			(const GLushort *)indices, normals ? qtrue : qfalse,
			tex0 ? qtrue : qfalse, tex1 ? qtrue : qfalse,
			(unsigned int)push_reserve_dwords );
		#endif
	}
	if (xboxPerfSample) xboxPhaseStart = STEFX_XboxReadTsc();
#endif
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440004; /* 'ND04': reserve push buffer */
#endif
	glw_state->device->BeginPush(push_reserve_dwords, &glw_state->drawArray);
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440005; /* 'ND05': push buffer reserved */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		xboxPhaseCycles = STEFX_XboxElapsedCycles(xboxPhaseStart);
		g_SPXBPerfDrawReserveCyclesCurrent += xboxPhaseCycles;
		g_SPXBPerfDrawBeginPushCyclesCurrent += xboxPhaseCycles;
		if (xboxPhaseCycles > g_SPXBPerfDrawBeginPushMaxCyclesCurrent) {
			g_SPXBPerfDrawBeginPushMaxCyclesCurrent = xboxPhaseCycles;
			g_SPXBPerfDrawBeginPushMaxDwordsCurrent = (unsigned int)push_reserve_dwords;
			g_SPXBPerfDrawBeginPushMaxStateCurrent = xboxStateBits;
		}
		if (xboxPhaseCycles > 73333u) ++g_SPXBPerfDrawBeginPushOver100KCurrent;
		if (xboxPhaseCycles > 733333u) ++g_SPXBPerfDrawBeginPushOver1MsecCurrent;
		if (xboxPhaseCycles > 7333333u) ++g_SPXBPerfDrawBeginPushOver10MsecCurrent;
	}
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) xboxPhaseStart = STEFX_XboxReadTsc();
#endif
	DWORD *pushBase = glw_state->drawArray;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawPointerCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
		xboxPhaseStart = STEFX_XboxReadTsc();
	}
#endif

	DWORD *stream = 0, *payload = 0;
	if (residentPayload) {
		stream = residentPayload;
	} else {

#ifdef _XBOX
	if (scratchPayload)
	{
		// Payload lives in the fenced scratch ring; the push packet carries
		// only the layout, stream pointers, and indexes.
		payload = scratchPayload;
	}
	else
#endif
	{
		// Original inline layout: jump the FIFO over the in-push payload.
		DWORD *jumpaddress = pushBase + (vert_size + 1);
		*glw_state->drawArray++ = ((DWORD)jumpaddress & 0x7fffffff) | 1;
		payload = glw_state->drawArray;
	}

	// Set up our own fake vertex buffer
	stream = payload;
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	if (interleavedPayload) {
		STEFX_PackInterleaved(payload, packedLayout, normals, tex0, tex1);
		++g_SPXBInterleavedVertices[0];
		if (g_SPXBInterleavedVertices[0] == 1) {
			XBLog_WriteCriticalf("STEFX_INTERLEAVED: active stride=%u normals=%d tex=%d/%d verify=%d",
				interleavedStride, normals ? 1 : 0, tex0 ? 1 : 0, tex1 ? 1 : 0,
				s_interleavedVerifyCvar->integer);
		}
		g_SPXBInterleavedVertices[1] += tess.numVertexes;
		g_SPXBInterleavedVertices[6] = interleavedStride;
		if (!scratchPayload) ++g_SPXBInterleavedVertices[3];
		if (s_interleavedVerifyCvar->integer) {
			g_SPXBInterleavedVertices[4] += tess.numVertexes;
			if (!STEFX_VerifyInterleaved(payload, packedLayout, normals, tex0, tex1)) {
				++g_SPXBInterleavedVertices[5];
				XBLog_WriteCriticalf("STEFX_INTERLEAVED: verification failed shader=%s stride=%u", tess.shader->name, interleavedStride);
				s_interleavedCvar->integer = 0;
				glw_state->device->EndPush(pushBase);
				glw_state->inDrawBlock = false;
				return;
			}
		}
		payload += vert_size;
	} else
#endif
	{
	memcpy(payload, tess.xyz, sizeof(vec4_t) * tess.numVertexes);
	payload += tess.numVertexes * 4;

	if(normals)
	{
		memcpy(payload, tess.normal, sizeof(vec4_t) * tess.numVertexes);
		payload += tess.numVertexes * 4;
	}

	if(glw_state->colorArrayState)
	{
		memcpy(payload, tess.svars.colors, sizeof(D3DCOLOR) * tess.numVertexes);
	}
	else
	{
		for( int v = 0; v < tess.numVertexes; ++v )
			payload[v] = glw_state->currentColor;
	}
	payload += tess.numVertexes;

	if(tex0)
	{
		memcpy(payload, tess.svars.texcoords[0], sizeof(vec2_t) * tess.numVertexes);
		payload += tess.numVertexes * 2;
	}

	if(tex1)
	{
		memcpy(payload, tess.svars.texcoords[1], sizeof(vec2_t) * tess.numVertexes);
		payload += tess.numVertexes * 2;
	}
	} // original planar packing

#ifdef _XBOX
	if (!scratchPayload)
#endif
	{
		glw_state->drawArray = payload;
	}
	} // dynamic vertex packing
	const unsigned int xyzStride = residentPayload ? 48u : (interleavedPayload ? interleavedStride : 16u);
	const unsigned int colorStride = residentPayload ? 48u : (interleavedPayload ? interleavedStride : 4u);
	const unsigned int texStride = residentPayload ? 48u : (interleavedPayload ? interleavedStride : 8u);

	// Write the vertex shader
#define CMD_STREAM_STRIDEANDTYPE0 0x1760
	{
	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_STREAM_STRIDEANDTYPE0, 16);
	*glw_state->drawArray++ = (xyzStride << 8)|D3DVSDT_FLOAT3;

	if(1)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	if(normals)
	{
		*glw_state->drawArray++ = (xyzStride << 8) | D3DVSDT_FLOAT3;
	}
	else
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	*glw_state->drawArray++ = (colorStride << 8) | D3DVSDT_D3DCOLOR;

	for(int i = 0; i < 5; i++)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	if(tex0)
	{
		*glw_state->drawArray++ = (texStride << 8) | D3DVSDT_FLOAT2;
	}
	else
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	if(tex1)
	{
		*glw_state->drawArray++ = (texStride << 8) | D3DVSDT_FLOAT2;
	}
	else
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	for(i = 0; i < 5; i++)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}
	}

//	 Write the indicator to our vertex stream
#define CMD_VERTEXSTREAM_XYZ		0x1720
#define CMD_VERTEXSTREAM_NORMAL		0x1728
#define CMD_VERTEXSTREAM_COLOR		0x172c
#define CMD_VERTEXSTREAM_TEX0		0x1744
#define CMD_VERTEXSTREAM_TEX1		0x1748

	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_VERTEXSTREAM_XYZ, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += (residentPayload || interleavedPayload) ? 3 : payloadVertices * 4;

	if(normals)
	{
		*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_VERTEXSTREAM_NORMAL, 1);
		*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
		stream += (residentPayload || interleavedPayload) ? 3 : payloadVertices * 4;
	}
	if (residentPayload) stream = residentPayload + 6;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_VERTEXSTREAM_COLOR, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += (residentPayload || interleavedPayload) ? 1 : payloadVertices;

	if(tex0)
	{
		*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_VERTEXSTREAM_TEX0, 1);
		*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
		stream += (residentPayload || interleavedPayload) ? 2 : payloadVertices * 2;
	}
	if (residentPayload) stream = residentPayload + 9;

	if(tex1)
	{
		*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_VERTEXSTREAM_TEX1, 1);
		*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	}

	// Send thru the index data
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawPackCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
		xboxPhaseStart = STEFX_XboxReadTsc();
	}
#endif
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP))
	if (worldPayload && s_worldArraysActive) {
		glw_state->drawArray = STEFX_WorldArraysWrite(glw_state->drawArray, glw_state->primitiveMode);
		glw_state->totalIndices = 0;
		glw_state->numIndices = count;
	} else {
		PushIndices(count, worldPayload ? s_worldIndices : (GLushort*)indices);
	}
#else
	PushIndices(count, (GLushort*)indices);
#endif
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440006; /* 'ND06': indexes packed */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawIndexCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
	}
#endif

	// finish up the draw
	glw_state->inDrawBlock = false;

	DWORD* push = _terminateIndexPacket(glw_state->drawArray);

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && defined(STEFX_SP_HOSTED_MP)
	// Persistent interleaved vertices use their own byte-for-byte verifier.
	// The legacy tracer expects a freshly packed, planar tess payload.
	if (!worldPayload && !interleavedPayload) STEFX_TraceShaderPackedDraw(
		scratchPayload ? scratchPayload : pushBase + 1,
		normals ? qtrue : qfalse,
		tex0 ? qtrue : qfalse, tex1 ? qtrue : qfalse,
		(unsigned int)push_reserve_dwords,
		(unsigned int)( push - pushBase ) );
#endif

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) xboxPhaseStart = STEFX_XboxReadTsc();
#endif
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440007; /* 'ND07': submit push buffer */
#endif
	glw_state->device->EndPush(push);
#ifdef _XBOX
	g_SPXBNativeSubmitStage = 0x4E440008; /* 'ND08': push buffer submitted */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (xboxPerfSample) {
		g_SPXBPerfDrawSubmitCyclesCurrent += STEFX_XboxElapsedCycles(xboxPhaseStart);
		g_SPXBPerfDrawCyclesCurrent += STEFX_XboxElapsedCycles(xboxDrawStart);
		int kind;
		if (backEnd.projection2D) kind = 0;
		else if (backEnd.currentEntity == &tr.worldEntity)
			kind = worldPayload ? 1 : (g_stefxWorldBasePass ? 2 : 3);
		else kind = backEnd.currentEntity && backEnd.currentEntity->e.reType == RT_MODEL ? 4 : 5;
		volatile unsigned int *k = g_SPXBPerfDrawKindsCurrent + kind * 8;
		++k[0]; k[1] += tess.numVertexes; k[2] += count;
		k[3] += STEFX_XboxElapsedCycles(xboxDrawStart);
		k[4] += g_SPXBPerfDrawBeginPushCyclesCurrent - kindBeginBefore;
		k[5] += g_SPXBPerfDrawPackCyclesCurrent - kindPackBefore;
		k[6] += g_SPXBPerfDrawStateCyclesCurrent - kindStateBefore;
		k[7] += push_reserve_dwords;
		if (kind == 4) {
			STEFX_RecordModelDraw((unsigned int)backEnd.currentEntity->e.hModel,
				(unsigned int)tess.shader,
				(unsigned int)R_GetModelByHandle(backEnd.currentEntity->e.hModel),
				(unsigned int)tess.numVertexes, (unsigned int)count,
				STEFX_XboxElapsedCycles(xboxDrawStart),
				g_SPXBPerfDrawBeginPushCyclesCurrent - kindBeginBefore);
		}
	}
#endif
}

static void dllDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	assert(0);
}

static void dllEdgeFlag(GLboolean flag)
{
	assert(0);
}

static void dllEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	assert(0);
}

static void dllEdgeFlagv(const GLboolean *flag)
{
	assert(0);
}

static void dllEnable(GLenum cap)
{
	setCap(cap, true);
}

static void dllEnableClientState(GLenum array)
{
	assert(!glw_state->inDrawBlock);
	setArrayState(array, true);
}

static void dllEnd(void)
{
	assert(glw_state->inDrawBlock);
	glw_state->inDrawBlock = false;
#ifdef _XBOX
	// on Xbox, just close the draw packet
	DWORD* push = _terminateDrawPacket(
		&glw_state->drawArray[glw_state->numVertices * 
		glw_state->drawStride]);
	glw_state->device->EndPush(push);
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (g_SPXBPerfSampleActive && s_xboxImmediateDrawStart) {
		g_SPXBPerfDrawCyclesCurrent += STEFX_XboxElapsedCycles(s_xboxImmediateDrawStart);
	}
	s_xboxImmediateDrawStart = 0;
#endif
#else
	// on the PC, use DrawPrimitiveUp (a little slow)
	int num = 0;
	switch (glw_state->primitiveMode)
	{
	case D3DPT_POINTLIST: num = glw_state->numVertices; break;
	case D3DPT_LINELIST: num = glw_state->numVertices / 2; break;
	case D3DPT_LINESTRIP: num = glw_state->numVertices - 1; break;
	case D3DPT_TRIANGLELIST: num = glw_state->numVertices / 3; break;
	case D3DPT_TRIANGLESTRIP: num = glw_state->numVertices - 2; break;
	case D3DPT_TRIANGLEFAN: num = glw_state->numVertices - 2; break;
	}
	
	glw_state->device->DrawPrimitiveUP(
		glw_state->primitiveMode, num, 
		glw_state->drawArray, glw_state->drawStride * sizeof(DWORD));
#endif
}

// EXTENSION: End drawing for a frame
static void dllEndFrame(void)
{
	assert(!glw_state->inDrawBlock);

#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	static char s_stefxTextureStageProfile[256];
	static char s_stefxVertexShaderProfile[256];
	static char s_stefxStreamSourceProfile[256];
	++s_stefxTextureStageFrames;
	if ( ( s_stefxTextureStageFrames & 255u ) == 0u )
	{
		const unsigned int skipPercent = s_stefxTextureStageRequests
			? ( 100u * s_stefxTextureStageSkips ) /
				s_stefxTextureStageRequests
			: 0u;
		_snprintf(
			s_stefxTextureStageProfile,
			sizeof(s_stefxTextureStageProfile) - 1,
			"STEFX_HW_STAGE_CACHE: sample=%u players=%d fog=%d requests=%u emitted=%u skipped=%u skipPct=%u",
			s_stefxTextureStageFrames,
			Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ),
			( Cvar_VariableIntegerValue( "r_splitScreenEconomy" ) &&
			  Cvar_VariableIntegerValue( "stefx_splitScreen" ) &&
			  Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ) >= 3 )
				? 0 : R_STEFX_FogModeForView(), s_stefxTextureStageRequests,
			s_stefxTextureStageEmits, s_stefxTextureStageSkips,
			skipPercent );
		s_stefxTextureStageProfile[sizeof(s_stefxTextureStageProfile) - 1] = '\0';
		XBLog_WriteProfile( s_stefxTextureStageProfile );

		const unsigned int vertexSkipPercent = s_stefxVertexShaderRequests
			? ( 100u * s_stefxVertexShaderSkips ) /
				s_stefxVertexShaderRequests
			: 0u;
		_snprintf(
			s_stefxVertexShaderProfile,
			sizeof(s_stefxVertexShaderProfile) - 1,
			"STEFX_HW_VERTEX_SHADER_CACHE: sample=%u players=%d policy=three-plus requests=%u emitted=%u skipped=%u skipPct=%u",
			s_stefxTextureStageFrames,
			Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ),
			s_stefxVertexShaderRequests, s_stefxVertexShaderEmits,
			s_stefxVertexShaderSkips, vertexSkipPercent );
		s_stefxVertexShaderProfile[sizeof(s_stefxVertexShaderProfile) - 1] = '\0';
		XBLog_WriteProfile( s_stefxVertexShaderProfile );

		const unsigned int streamSkipPercent = s_stefxStreamSourceRequests
			? ( 100u * s_stefxStreamSourceSkips ) /
				s_stefxStreamSourceRequests
			: 0u;
		_snprintf(
			s_stefxStreamSourceProfile,
			sizeof(s_stefxStreamSourceProfile) - 1,
			"STEFX_HW_STREAM_SOURCE_CACHE: sample=%u players=%d policy=three-plus requests=%u emitted=%u skipped=%u skipPct=%u",
			s_stefxTextureStageFrames,
			Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" ),
			s_stefxStreamSourceRequests, s_stefxStreamSourceEmits,
			s_stefxStreamSourceSkips, streamSkipPercent );
		s_stefxStreamSourceProfile[sizeof(s_stefxStreamSourceProfile) - 1] = '\0';
		XBLog_WriteProfile( s_stefxStreamSourceProfile );
	}
#endif

#ifdef _XBOX
	STEFX_ScratchFrameEnd();
	{
		static unsigned int s_stefxScratchProfileFrames;
		static char s_stefxScratchProfile[192];
		++s_stefxScratchProfileFrames;
		if ((s_stefxScratchProfileFrames & 255u) == 0u && s_stefxScratchInitDone)
		{
			_snprintf(s_stefxScratchProfile,
				sizeof(s_stefxScratchProfile) - 1,
				"STEFX_HW_SCRATCH_RING: sample=%u mode=%u flip=%u draws=%u fallbacks=%u dwordsK=%u peakDwords=%u waits=%u waitMsec=%u",
				s_stefxScratchProfileFrames, g_SPXBScratchMode,
				g_SPXBScratchFlip, g_SPXBScratchDraws,
				g_SPXBScratchFallbacks, s_stefxScratchDwords / 1024u,
				s_stefxScratchPeakOffset, s_stefxScratchFenceWaits,
				g_SPXBScratchWaitMsec);
			s_stefxScratchProfile[sizeof(s_stefxScratchProfile) - 1] = '\0';
			XBLog_WriteProfile(s_stefxScratchProfile);
		}
	}
#endif

	// the blend state can get reset by Present()...
	GLboolean blend = qglIsEnabled(GL_BLEND);

	g_SPXBClTailStage = 0x47503030; /* 'GP00' */
	glw_state->device->EndScene();
	g_SPXBClTailStage = 0x47503031; /* 'GP01' */
	
	qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	g_SPXBClTailStage = 0x47503032; /* 'GP02' */
	glw_state->device->Present(NULL, NULL, NULL, NULL);
	g_SPXBClTailStage = 0x47503033; /* 'GP03' */

	// restore the pre-Present state
	if (blend) qglEnable(GL_BLEND);
	else qglDisable(GL_BLEND);
}

// EXTENSION: End shadow draw mode
static void dllEndShadow(void)
{
	//Intentionally left blank
}

static void dllEndList(void)
{
	assert(0);
}

static void dllEvalCoord1d(GLdouble u)
{
	assert(0);
}

static void dllEvalCoord1dv(const GLdouble *u)
{
	assert(0);
}

static void dllEvalCoord1f(GLfloat u)
{
	assert(0);
}

static void dllEvalCoord1fv(const GLfloat *u)
{
	assert(0);
}

static void dllEvalCoord2d(GLdouble u, GLdouble v)
{
	assert(0);
}

static void dllEvalCoord2dv(const GLdouble *u)
{
	assert(0);
}

static void dllEvalCoord2f(GLfloat u, GLfloat v)
{
	assert(0);
}

static void dllEvalCoord2fv(const GLfloat *u)
{
	assert(0);
}

static void dllEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	assert(0);
}

static void dllEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	assert(0);
}

static void dllEvalPoint1(GLint i)
{
	assert(0);
}

static void dllEvalPoint2(GLint i, GLint j)
{
	assert(0);
}

static void dllFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	assert(0);
}

static void dllFinish(void)
{
#ifdef _XBOX
	g_SPXBClTailStage = 0x47463030; /* 'GF00' */
	glw_state->device->BlockUntilIdle();
	g_SPXBClTailStage = 0x47463031; /* 'GF01' */
#endif
}

static void dllFlush(void)
{
#ifdef _XBOX
	glw_state->device->BlockUntilIdle();
#endif
}

// EXTENSION: Draw the shadow
static void dllFlushShadow(void)
{
	//Intentionally left blank
}

static D3DFOGMODE _convertFogMode(GLint param)
{
	switch(param)
	{
	case GL_LINEAR: return D3DFOG_LINEAR; break;
	case GL_EXP: return D3DFOG_EXP; break;
	case GL_EXP2: return D3DFOG_EXP2; break;
	}

	return D3DFOG_NONE;
}

static void dllFogf(GLenum pname, GLfloat param)
{
	assert(pname == GL_FOG_DENSITY || pname == GL_FOG_START || pname == GL_FOG_END);	

	switch(pname)
	{
	case GL_FOG_DENSITY: glw_state->device->SetRenderState( D3DRS_FOGDENSITY, *(DWORD*)&param ); break;
	case GL_FOG_START: glw_state->device->SetRenderState( D3DRS_FOGSTART, *(DWORD*)&param ); break;
	case GL_FOG_END: glw_state->device->SetRenderState( D3DRS_FOGEND, *(DWORD*)&param ); break;
	}
}

static void dllFogfv(GLenum pname, const GLfloat *params)
{
	assert(pname == GL_FOG_COLOR);

	D3DCOLOR color = D3DCOLOR_ARGB(0x00, 
								  (int)(params[0] * 255.0f),
								  (int)(params[1] * 255.0f),
								  (int)(params[2] * 255.0f));

	glw_state->device->SetRenderState( D3DRS_FOGCOLOR, color );
}

static void dllFogi(GLenum pname, GLint param)
{
	assert(pname == GL_FOG_MODE);

	glw_state->device->SetRenderState( D3DRS_FOGTABLEMODE, _convertFogMode(param) );
}

static void dllFogiv(GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllFrontFace(GLenum mode)
{
	assert(0);
}

static void dllFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	D3DXMATRIX m;
	D3DXMatrixPerspectiveOffCenterRH(&m, left, right, bottom, top, zNear, zFar);
	glw_state->matrixStack[glw_state->matrixMode]->MultMatrix(&m);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

GLuint dllGenLists(GLsizei range)
{
	assert(0);
	return 0;
}

static void dllGenTextures(GLsizei n, GLuint *textures)
{
	for (int i = 0; i < n; ++i)
	{
		textures[i] = glw_state->textureBindNum++;
	}
}

// Implemented only the states we use.
template <typename T>
static void _getState(GLenum pname, T *params)
{
	switch (pname)
	{
	case GL_CULL_FACE: params[0] = (T)glw_state->cullEnable; break;
	case GL_MAX_TEXTURE_SIZE: params[0] = (T)512; break;
	case GL_MAX_ACTIVE_TEXTURES_ARB: params[0] = GLW_MAX_TEXTURE_STAGES; break;
	case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: params[0] = 4; break;
	default:
		assert(0);
		params[0] = (T)0;
		break;
	}
}

static void dllGetBooleanv(GLenum pname, GLboolean *params)
{
	_getState(pname, params);
}

static void dllGetClipPlane(GLenum plane, GLdouble *equation)
{
	assert(0);
}

static void dllGetDoublev(GLenum pname, GLdouble *params)
{
	_getState(pname, params);
}

GLenum dllGetError(void)
{
	return 0;
}

static void dllGetFloatv(GLenum pname, GLfloat *params)
{
	_getState(pname, params);
}

static void dllGetIntegerv(GLenum pname, GLint *params)
{
	_getState(pname, params);
}

static void dllGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	assert(0);
}

static void dllGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	assert(0);
}

static void dllGetMapiv(GLenum target, GLenum query, GLint *v)
{
	assert(0);
}

static void dllGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllGetPixelMapfv(GLenum map, GLfloat *values)
{
	assert(0);
}

static void dllGetPixelMapuiv(GLenum map, GLuint *values)
{
	assert(0);
}

static void dllGetPixelMapusv(GLenum map, GLushort *values)
{
	assert(0);
}

static void dllGetPointerv(GLenum pname, GLvoid* *params)
{
	assert(0);
}

static void dllGetPolygonStipple(GLubyte *mask)
{
	assert(0);
}

const GLubyte * dllGetString(GLenum name)
{
	switch (name)
	{
	case GL_VENDOR: return (const unsigned char*)"Vicarious Visions";
	case GL_RENDERER: return (const unsigned char*)"Optimized DX8/OpenGL Layer";
	case GL_VERSION: return (const unsigned char*)"0.1";
	case GL_EXTENSIONS:
		return (const unsigned char*)
			"EXT_texture_env_add GL_ARB_multitexture EXT_texture_filter_anisotropic";
	default: return (const unsigned char*)"";
	}
}

static void dllGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	assert(0);
}

static void dllGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	assert(0);
}

static void dllGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	assert(0);
}

static void dllGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	assert(0);
}

static void dllHint(GLenum target, GLenum mode)
{
	assert(0);
}

// Convert an triangle index array (indices) to a 
// triangle strip index array (dest) with primitive 
// length array.
static void buildStrips(GLuint* len, GLsizei* num_lens, GLushort* dest, GLsizei* num_indices, const GLushort* src)
{
	GLushort last[3];

	// prime the strip
	GLsizei cur_index = 0;
	dest[cur_index++] = src[0];
	dest[cur_index++] = src[1];
	dest[cur_index++] = src[2];
	GLuint cur_length = 3;
	GLsizei num_strips = 0;

	GLuint max_length = GLW_MAX_DRAW_PACKET_SIZE / glw_state->drawStride;
	
	last[0] = src[0];
	last[1] = src[1];
	last[2] = src[2];

	qboolean even = qfalse;

	for ( GLsizei i = 3; i < *num_indices; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( src[i+0] == last[2] ) && ( src[i+1] == last[1] ) &&
				cur_length < max_length )
			{
				++cur_length;
				dest[cur_index++] = src[i+2];
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				len[num_strips++] = cur_length;
				cur_length = 3;

				dest[cur_index++] = src[i+0];
				dest[cur_index++] = src[i+1];
				dest[cur_index++] = src[i+2];

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == src[i+1] ) && ( last[0] == src[i+0] ) &&
				cur_length < max_length )
			{
				++cur_length;
				dest[cur_index++] = src[i+2];
				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				len[num_strips++] = cur_length;
				cur_length = 3;

				dest[cur_index++] = src[i+0];
				dest[cur_index++] = src[i+1];
				dest[cur_index++] = src[i+2];

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = src[i+0];
		last[1] = src[i+1];
		last[2] = src[i+2];
	}

	len[num_strips++] = cur_length;
	*num_lens = num_strips;
	*num_indices = cur_index;

	assert(num_strips <= GLW_MAX_STRIPS);
}

#ifdef _XBOX
void renderObject_Light( int numIndexes, const glIndex_t *indexes )
{
	int i;

	g_SPXBNativeSubmitStage = 0x4E4C0000; /* 'NL00': light draw entry */

	// start the draw mode
	assert(!glw_state->inDrawBlock);

	glw_state->inDrawBlock = true;
	glw_state->primitiveMode = D3DPT_TRIANGLELIST;

	glw_state->drawStride = 14;

	glw_state->numIndices = 0;
	glw_state->totalIndices = numIndexes;
	glw_state->maxIndices = _getMaxIndices();

	STEFX_D3D8_SetStreamSourceZeroTracked( glw_state->drawStride * 4 );
	g_SPXBNativeSubmitStage = 0x4E4C0001; /* 'NL01': stream source ready */

	int vert_size = glw_state->drawStride * tess.numVertexes;
	int index_size = numIndexes / 2;
	g_SPXBNativeSubmitStage = 0x4E4C0002; /* 'NL02': reserve push buffer */
	glw_state->device->BeginPush(vert_size + index_size + 60, &glw_state->drawArray);
	g_SPXBNativeSubmitStage = 0x4E4C0003; /* 'NL03': push buffer reserved */

	DWORD *pushBase = glw_state->drawArray;

	DWORD *jumpaddress = 0, *stream = 0;

	// Determine where the end of the vertex data is gonna be,
	// that's where we're going to jump to
	jumpaddress = pushBase + (vert_size + 1);

	// Write the jump address
	*glw_state->drawArray++ = ((DWORD)jumpaddress & 0x7fffffff) | 1;

	// Set up our own fake vertex buffer
	stream = glw_state->drawArray;

	memcpy(glw_state->drawArray, tess.xyz, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	memcpy(glw_state->drawArray, tess.normal, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	memcpy(glw_state->drawArray, tess.svars.texcoords[0], sizeof(vec2_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 2;

	memcpy(glw_state->drawArray, tess.tangent, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;
	g_SPXBNativeSubmitStage = 0x4E4C0004; /* 'NL04': vertex streams copied */

	// Write the vertex shader
#define CMD_STREAM_STRIDEANDTYPE0 0x1760
	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_STREAM_STRIDEANDTYPE0, 16);

	// Position
	*glw_state->drawArray++ = (16 << 8)|D3DVSDT_FLOAT3;

	// Normal
	*glw_state->drawArray++ = (16 << 8) | D3DVSDT_FLOAT3;

	// Tex Coord
	*glw_state->drawArray++ = (8 << 8) | D3DVSDT_FLOAT2;

	// Tangent
	*glw_state->drawArray++ = (16 << 8) | D3DVSDT_FLOAT3;

	for(i = 0; i < 12; i++)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	//	 Write the indicator to our vertex stream
	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1720, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1724, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1728, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 2;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x172c, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	// Send thru the index data
	g_SPXBNativeSubmitStage = 0x4E4C0005; /* 'NL05': pack indexes */
	PushIndices(numIndexes, (GLushort*)indexes);
	g_SPXBNativeSubmitStage = 0x4E4C0006; /* 'NL06': indexes packed */

	// finish up the draw
	glw_state->inDrawBlock = false;

	DWORD* push = _terminateIndexPacket(glw_state->drawArray);

	g_SPXBNativeSubmitStage = 0x4E4C0007; /* 'NL07': submit push buffer */
	glw_state->device->EndPush(push);
	g_SPXBNativeSubmitStage = 0x4E4C0008; /* 'NL08': push buffer submitted */
}

void renderObject_Bump()
{
	// start the draw mode
	assert(!glw_state->inDrawBlock);

	glw_state->inDrawBlock = true;
	glw_state->primitiveMode = D3DPT_TRIANGLELIST;

	glw_state->drawStride = 16;

	glw_state->numIndices = 0;
	glw_state->totalIndices = tess.numIndexes;
	glw_state->maxIndices = _getMaxIndices();

	STEFX_D3D8_SetStreamSourceZeroTracked( glw_state->drawStride * 4 );

	int vert_size = glw_state->drawStride * tess.numVertexes;
	int index_size = tess.numIndexes / 2;

	glw_state->device->BeginPush(vert_size + index_size + 60, &glw_state->drawArray);

	DWORD *pushBase = glw_state->drawArray;

	DWORD *jumpaddress = 0, *stream = 0;

	// Determine where the end of the vertex data is gonna be,
	// that's where we're going to jump to
	jumpaddress = pushBase + (vert_size + 1);

	// Write the jump address
	*glw_state->drawArray++ = ((DWORD)jumpaddress & 0x7fffffff) | 1;

	// Set up our own fake vertex buffer
	stream = glw_state->drawArray;

	memcpy(glw_state->drawArray, tess.xyz, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	memcpy(glw_state->drawArray, tess.normal, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	memcpy(glw_state->drawArray, tess.svars.texcoords[0], sizeof(vec2_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 2;

	memcpy(glw_state->drawArray, tess.svars.texcoords[1], sizeof(vec2_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 2;

	memcpy(glw_state->drawArray, tess.tangent, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	// Write the vertex shader
#define CMD_STREAM_STRIDEANDTYPE0 0x1760
	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_STREAM_STRIDEANDTYPE0, 16);

	// Position
	*glw_state->drawArray++ = (16 << 8)|D3DVSDT_FLOAT3;

	// Normal
	*glw_state->drawArray++ = (16 << 8) | D3DVSDT_FLOAT3;

	// Tex Coord 0
	*glw_state->drawArray++ = (8 << 8) | D3DVSDT_FLOAT2;

	// Tex Coord 1
	*glw_state->drawArray++ = (8 << 8) | D3DVSDT_FLOAT2;

	// Tangent
	*glw_state->drawArray++ = (16 << 8) | D3DVSDT_FLOAT3;

	for(int i = 0; i < 11; i++)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	//	 Write the indicator to our vertex stream
	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1720, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1724, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1728, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 2;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x172c, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 2;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1730, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	// Send thru the index data
	PushIndices(tess.numIndexes, (GLushort*)tess.indexes);

	// finish up the draw
	glw_state->inDrawBlock = false;

	DWORD* push = _terminateIndexPacket(glw_state->drawArray);

	glw_state->device->EndPush(push);
}

void renderObject_Env()
{
	// start the draw mode
	assert(!glw_state->inDrawBlock);

	glw_state->inDrawBlock = true;
	glw_state->primitiveMode = D3DPT_TRIANGLELIST;

	glw_state->drawStride = 9;
	_updateTextures();

	glw_state->numIndices = 0;
	glw_state->totalIndices = tess.numIndexes;
	glw_state->maxIndices = _getMaxIndices();

	STEFX_D3D8_SetStreamSourceZeroTracked( glw_state->drawStride * 4 );

	int vert_size = glw_state->drawStride * tess.numVertexes;
	int index_size = tess.numIndexes / 2;

	glw_state->device->BeginPush(vert_size + index_size + 60, &glw_state->drawArray);

	DWORD *pushBase = glw_state->drawArray;

	DWORD *jumpaddress = 0, *stream = 0;

	// Determine where the end of the vertex data is gonna be,
	// that's where we're going to jump to
	jumpaddress = pushBase + (vert_size + 1);

	// Write the jump address
	*glw_state->drawArray++ = ((DWORD)jumpaddress & 0x7fffffff) | 1;

	// Set up our own fake vertex buffer
	stream = glw_state->drawArray;

	memcpy(glw_state->drawArray, tess.xyz, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4;

	memcpy(glw_state->drawArray, tess.normal, sizeof(vec4_t) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes * 4; 

	memcpy(glw_state->drawArray, tess.svars.colors, sizeof(D3DCOLOR) * tess.numVertexes);
	glw_state->drawArray += tess.numVertexes;
 
	// Write the vertex shader
#define CMD_STREAM_STRIDEANDTYPE0 0x1760
	*glw_state->drawArray++ = D3DPUSH_ENCODE(CMD_STREAM_STRIDEANDTYPE0, 16);

	// Position
	*glw_state->drawArray++ = (16 << 8)|D3DVSDT_FLOAT3;

	// Normal
	*glw_state->drawArray++ = (16 << 8) | D3DVSDT_FLOAT3;

	// Color
	*glw_state->drawArray++ = (4 << 8) | D3DVSDT_D3DCOLOR;

	for(int i = 0; i < 13; i++)
	{
		*glw_state->drawArray++ = ((glw_state->drawStride * 4) << 8) | D3DVSDT_NONE;
	}

	//	 Write the indicator to our vertex stream
	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1720, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1724, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes * 4;

	*glw_state->drawArray++ = D3DPUSH_ENCODE(0x1728, 1);
	*glw_state->drawArray++ = (DWORD)stream & 0x7fffffff;
	stream += tess.numVertexes;

	// Send thru the index data
	PushIndices(tess.numIndexes, (GLushort*)tess.indexes);

	// finish up the draw
	glw_state->inDrawBlock = false;

	DWORD* push = _terminateIndexPacket(glw_state->drawArray);

	glw_state->device->EndPush(push);
}
#endif

// EXTENSION: Take an array of triangle indices and draw
// the appropriate triangle strips.  Virtually ALL geometry
// is drawn with this function so it better be fast.
static void dllIndexedTriToStrip(GLsizei count, const GLushort *indices)
{
#ifndef _XBOX
#ifdef GLW_USE_TRI_STRIPS

	// update the render state
	_updateDrawStride(glw_state->normalArrayState,
		glw_state->texCoordArrayState[0] ? count : 0,
		glw_state->texCoordArrayState[1] ? count : 0);
	_updateShader(glw_state->normalArrayState,
		glw_state->texCoordArrayState[0],
		glw_state->texCoordArrayState[1]);
	_updateTextures();
	_updateMatrices();
	
	// convert triangles to strips -- guarantees that
	// no strip exceeds the max draw packet size
	if(tess.currentPass == 0)
	{
		buildStrips(glw_state->strip_lengths, 
			&glw_state->num_strip_lengths, glw_state->strip_dest, &count, indices);
	}

	// Yeah, its a hack, but I gotta do this so bumpmapping
	// doesnt go all crazy on the 'force speed' effect and 
	// 'disintegration' effect
	if(tess.shader && 
		tess.shader->isBumpMap && 
		(backEnd.currentEntity->e.renderfx & 
// VVFIXME : This is probably wrong. It looks like RF_ALPHA_FADE is renamed
// RF_RGB_TINT in MP. Substitute?
#ifndef _JK2MP
		(RF_ALPHA_FADE | RF_DISINTEGRATE1 | RF_DISINTEGRATE2)))
#else
		(RF_DISINTEGRATE1 | RF_DISINTEGRATE2)))
#endif
	{
		if(tess.currentPass != 2)
			return;
	}

#ifdef _XBOX
	glw_state->primitiveMode = D3DPT_TRIANGLESTRIP;

	// get the necessary draw function
	drawelemfunc_t func = _drawElementFuncTable[_getDrawFunc()];
	int stride = glw_state->drawStride;
	
	int index = 0;
	for (int l = 0; l < glw_state->num_strip_lengths; ++l)
	{
		int cur_len = glw_state->strip_lengths[l];
		
		// start a draw packet
		DWORD* push;
		glw_state->device->BeginPush(stride * cur_len + 5, &push);
		push = _restartDrawPacket(push, cur_len);
		
		// draw the geometry
	glw_state->drawArray = push;
		func(cur_len, &glw_state->strip_dest[index]);
		index += cur_len;
		
		// finish the draw packet
		push = _terminateDrawPacket(&push[stride * cur_len]);
		glw_state->device->EndPush(push);
	}
#else _XBOX
	// simplified render on the PC
	int index = 0;
	for (int l = 0; l < glw_state->num_strip_lengths; ++l)
	{
		dllDrawElements(GL_TRIANGLE_STRIP, glw_state->strip_lengths[l], 
			GL_UNSIGNED_SHORT, &glw_state->strip_dest[index]);
		index += glw_state->strip_lengths[l];
	}
#endif _XBOX

#else GLW_USE_TRI_STRIPS
	// just render simple triangles
	dllDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices);
#endif GLW_USE_TRI_STRIPS
#endif 
}

static void dllIndexMask(GLuint mask)
{
	assert(0);
}

static void dllIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	assert(0);
}

static void dllIndexd(GLdouble c)
{
	assert(0);
}

static void dllIndexdv(const GLdouble *c)
{
	assert(0);
}

static void dllIndexf(GLfloat c)
{
	assert(0);
}

static void dllIndexfv(const GLfloat *c)
{
	assert(0);
}

static void dllIndexi(GLint c)
{
	assert(0);
}

static void dllIndexiv(const GLint *c)
{
	assert(0);
}

static void dllIndexs(GLshort c)
{
	assert(0);
}

static void dllIndexsv(const GLshort *c)
{
	assert(0);
}

static void dllIndexub(GLubyte c)
{
	assert(0);
}

static void dllIndexubv(const GLubyte *c)
{
	assert(0);
}

static void dllInitNames(void)
{
	assert(0);
}

static void dllInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	assert(0);
}

GLboolean dllIsEnabled(GLenum cap)
{
	DWORD flag;
	switch (cap)
	{
	case GL_ALPHA_TEST: glw_state->device->GetRenderState(D3DRS_ALPHATESTENABLE, &flag); break;
	case GL_BLEND: glw_state->device->GetRenderState(D3DRS_ALPHABLENDENABLE, &flag); break;
	case GL_CULL_FACE: return glw_state->cullEnable;
	case GL_DEPTH_TEST: glw_state->device->GetRenderState(D3DRS_ZENABLE, &flag); break;
	case GL_FOG: glw_state->device->GetRenderState(D3DRS_FOGENABLE, &flag); break;
	case GL_LIGHTING: glw_state->device->GetRenderState(D3DRS_LIGHTING, &flag); break;
#ifdef _XBOX
	case GL_POLYGON_OFFSET_FILL: glw_state->device->GetRenderState(D3DRS_SOLIDOFFSETENABLE, &flag); break;
#else
	case GL_POLYGON_OFFSET_FILL: return FALSE;
#endif
	case GL_SCISSOR_TEST: return glw_state->scissorEnable;
	case GL_STENCIL_TEST: glw_state->device->GetRenderState(D3DRS_STENCILENABLE, &flag); break;
	case GL_TEXTURE_2D: return glw_state->textureStageEnable[glw_state->serverTU];
	default: return FALSE;
	}
	return flag;
}

GLboolean dllIsList(GLuint lnum)
{
	assert(0);
	return 1;
}

GLboolean dllIsTexture(GLuint texture)
{
	assert(0);
	return 1;
}

static void dllLightModelf(GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllLightModelfv(GLenum pname, const GLfloat *params)
{
	assert(0);
}

static void dllLightModeli(GLenum pname, GLint param)
{
	assert(0);
}

static void dllLightModeliv(GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllLightf(GLenum light, GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	switch(pname)
	{
	case GL_AMBIENT:
		{
			glw_state->dirLight[light].Ambient.r = params[0] / 255.0f;
			glw_state->dirLight[light].Ambient.g = params[1] / 255.0f;
			glw_state->dirLight[light].Ambient.b = params[2] / 255.0f;
		}
		break;

	case GL_DIFFUSE:
		{
			glw_state->dirLight[light].Diffuse.r = params[0] / 255.0f;
			glw_state->dirLight[light].Diffuse.g = params[1] / 255.0f;
			glw_state->dirLight[light].Diffuse.b = params[2] / 255.0f;
		}
		break;

	case GL_SPECULAR:
		{
			glw_state->dirLight[light].Specular.r = params[0] / 255.0f;
			glw_state->dirLight[light].Specular.g = params[1] / 255.0f;
			glw_state->dirLight[light].Specular.b = params[2] / 255.0f;
		}
		break;
	case GL_POSITION:
		{
			glw_state->dirLight[light].Position.x = params[0];
			glw_state->dirLight[light].Position.y = params[1];
			glw_state->dirLight[light].Position.z = params[2];
		}
		break;

	case GL_SPOT_DIRECTION:
		{
			glw_state->dirLight[light].Direction.x = -params[0];
			glw_state->dirLight[light].Direction.y = -params[1];
			glw_state->dirLight[light].Direction.z = -params[2];
		}
		break;

	default:
		assert(0);
		break;
	}

	// Sanity check to avoid assert in D3D?
	if( (glw_state->dirLight[light].Direction.x * glw_state->dirLight[light].Direction.x +
		glw_state->dirLight[light].Direction.y * glw_state->dirLight[light].Direction.y +
		glw_state->dirLight[light].Direction.z * glw_state->dirLight[light].Direction.z) <= 0 )
	{
		assert( 0 && "Negative light direction in SetLight!" );
		glw_state->dirLight[light].Direction.x = 1.0f;
	}
	glw_state->device->SetLight(light, &glw_state->dirLight[light]);
	glw_state->device->LightEnable(light, true);

	// Do this so a dynamic light that has disappeared is removed
	if(!light)
		glw_state->device->LightEnable(1, false);
}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
// Co-op only. Complete one light before submitting it; never cache device state
// across draws, views, state blocks or device resets.
// mode, requests, original SetLight calls, combined SetLight calls, restores,
// verified, mismatches, API failures, failure latch, sampled cycles/calls/max,
// avoided SetLight/LightEnable calls, reserved, reserved.
extern "C" volatile unsigned int g_SPXBCoopLight[16]={0};
static bool s_coopLightFailed;

// STEFX_COOP_DIFFUSE_LIGHT_BEGIN
static void STEFX_CoopOriginalLight(trRefEntity_t *ent) {
	qglLightfv(0,GL_AMBIENT,ent->ambientLight);
	qglLightfv(0,GL_DIFFUSE,ent->directedLight);
	if (VectorLengthSquared(ent->lightDir)<=0.0001f) {
		ent->lightDir[0]=0.0f; ent->lightDir[1]=1.0f; ent->lightDir[2]=0.0f;
	}
	qglLightfv(0,GL_SPOT_DIRECTION,ent->lightDir);
	g_SPXBCoopLight[2]+=3;
}

static bool STEFX_CoopCombinedLight(trRefEntity_t *ent) {
	D3DLIGHT8 &light=glw_state->dirLight[0];
	light.Ambient.r=ent->ambientLight[0]/255.0f;
	light.Ambient.g=ent->ambientLight[1]/255.0f;
	light.Ambient.b=ent->ambientLight[2]/255.0f;
	light.Diffuse.r=ent->directedLight[0]/255.0f;
	light.Diffuse.g=ent->directedLight[1]/255.0f;
	light.Diffuse.b=ent->directedLight[2]/255.0f;
	if (VectorLengthSquared(ent->lightDir)<=0.0001f) {
		ent->lightDir[0]=0.0f; ent->lightDir[1]=1.0f; ent->lightDir[2]=0.0f;
	}
	light.Direction.x=-ent->lightDir[0];
	light.Direction.y=-ent->lightDir[1];
	light.Direction.z=-ent->lightDir[2];
	if (light.Direction.x*light.Direction.x+light.Direction.y*light.Direction.y+
		light.Direction.z*light.Direction.z<=0) light.Direction.x=1.0f;
	++g_SPXBCoopLight[3];
	const HRESULT set=glw_state->device->SetLight(0,&light);
	const HRESULT enable=glw_state->device->LightEnable(0,TRUE);
	const HRESULT disable=glw_state->device->LightEnable(1,FALSE);
	return SUCCEEDED(set) && SUCCEEDED(enable) && SUCCEEDED(disable);
}

bool STEFX_CoopDiffuseLight(trRefEntity_t *ent) {
	static cvar_t *mode,*split,*players,*game;
	static unsigned int loggedModes;
	if (!mode) {
		mode=Cvar_Get("r_efCoopLight","0",0);
		split=Cvar_Get("stefx_splitScreen","0",0);
		players=Cvar_Get("stefx_splitScreenPlayers","1",0);
		game=Cvar_Get("stefx_splitScreenMode","coop",0);
	}
	int selected=mode->integer;
	if (!ent || !glw_state || !glw_state->device || s_coopLightFailed ||
		!split->integer || players->integer<2 || Q_stricmp(game->string,"coop") ||
		!backEnd.viewParms.stefxSplitView || backEnd.projection2D || selected<1 || selected>3)
		selected=0;
#if !defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (selected==2) selected=0;
#endif
	g_SPXBCoopLight[0]=selected;
	if (!selected) return false;
	if (!(loggedModes&(1u<<selected))) {
		loggedModes|=1u<<selected;
		XBLog_WriteCriticalf("STEFX_COOP_LIGHT: mode=%d",selected);
	}
	++g_SPXBCoopLight[1];
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	const bool sample=g_SPXBPerfSampleActive!=0;
	const unsigned __int64 start=sample ? STEFX_XboxReadTsc() : 0;
#endif
	bool failed=false;
	if (selected==3) STEFX_CoopOriginalLight(ent);
	else if (selected==1) {
		if (!STEFX_CoopCombinedLight(ent)) {
			++g_SPXBCoopLight[7]; failed=true;
			STEFX_CoopOriginalLight(ent);
		} else {
			g_SPXBCoopLight[12]+=2; g_SPXBCoopLight[13]+=4;
		}
	}
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	else {
		const D3DLIGHT8 before=glw_state->dirLight[0];
		STEFX_CoopOriginalLight(ent);
		const D3DLIGHT8 originalCpu=glw_state->dirLight[0];
		D3DLIGHT8 originalDevice,combinedDevice;
		BOOL original0=FALSE,original1=TRUE,combined0=FALSE,combined1=TRUE;
		bool ok=SUCCEEDED(glw_state->device->GetLight(0,&originalDevice));
		ok=SUCCEEDED(glw_state->device->GetLightEnable(0,&original0)) && ok;
		ok=SUCCEEDED(glw_state->device->GetLightEnable(1,&original1)) && ok;
		if (ok) {
			glw_state->dirLight[0]=before;
			ok=STEFX_CoopCombinedLight(ent);
			ok=SUCCEEDED(glw_state->device->GetLight(0,&combinedDevice)) && ok;
			ok=SUCCEEDED(glw_state->device->GetLightEnable(0,&combined0)) && ok;
			ok=SUCCEEDED(glw_state->device->GetLightEnable(1,&combined1)) && ok;
			if (ok) {
				++g_SPXBCoopLight[5];
				if (memcmp(&originalCpu,&glw_state->dirLight[0],sizeof(originalCpu)) ||
					memcmp(&originalDevice,&combinedDevice,sizeof(originalDevice)) ||
					original0!=combined0 || original1!=combined1) {
					++g_SPXBCoopLight[6]; failed=true;
				}
			}
			// A diagnostic always renders the original state, even on mismatch.
			glw_state->dirLight[0]=originalCpu;
			ok=SUCCEEDED(glw_state->device->SetLight(0,&originalDevice)) && ok;
			ok=SUCCEEDED(glw_state->device->LightEnable(0,original0)) && ok;
			ok=SUCCEEDED(glw_state->device->LightEnable(1,original1)) && ok;
			++g_SPXBCoopLight[4];
		}
		if (!ok) { ++g_SPXBCoopLight[7]; failed=true; STEFX_CoopOriginalLight(ent); }
	}
	if (sample) {
		const unsigned int cycles=STEFX_XboxElapsedCycles(start);
		g_SPXBCoopLight[9]+=cycles; ++g_SPXBCoopLight[10];
		if (cycles>g_SPXBCoopLight[11]) g_SPXBCoopLight[11]=cycles;
	}
#endif
	if (failed) {
		s_coopLightFailed=true; g_SPXBCoopLight[8]=1;
		XBLog_WriteCriticalf("STEFX_COOP_LIGHT: disabled mismatches=%u apiFailures=%u",g_SPXBCoopLight[6],g_SPXBCoopLight[7]);
	}
	return true;
}
// STEFX_COOP_DIFFUSE_LIGHT_END
#endif

static void dllLighti(GLenum light, GLenum pname, GLint param)
{
	assert(0);
}

static void dllLightiv(GLenum light, GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllLineStipple(GLint factor, GLushort pattern)
{
	assert(0);
}

static void dllLineWidth(GLfloat width)
{
//	assert(0);
}

static void dllListBase(GLuint base)
{
	assert(0);
}

static void dllLoadIdentity(void)
{
	glw_state->matrixStack[glw_state->matrixMode]->LoadIdentity();
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllLoadMatrixd(const GLdouble *m)
{
	assert(0);
}

static void dllLoadMatrixf(const GLfloat *m)
{
	glw_state->matrixStack[glw_state->matrixMode]->LoadMatrix((D3DXMATRIX*)m);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllLoadName(GLuint name)
{
	assert(0);
}

static void dllLogicOp(GLenum opcode)
{
	assert(0);
}

static void dllMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	assert(0);
}

static void dllMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	assert(0);
}

static void dllMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	assert(0);
}

static void dllMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	assert(0);
}

static void dllMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	assert(0);
}

static void dllMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	assert(0);
}

static void dllMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	assert(0);
}

static void dllMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	assert(0);
}

static void dllMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	switch(pname)
	{
	case GL_AMBIENT:
		glw_state->mtrl.Ambient.r = params[0] / 255.0f;
		glw_state->mtrl.Ambient.g = params[1] / 255.0f;
		glw_state->mtrl.Ambient.b = params[2] / 255.0f;
		glw_state->mtrl.Ambient.a = params[3] / 255.0f;
		break;

	case GL_DIFFUSE:
		glw_state->mtrl.Diffuse.r = params[0] / 255.0f;
		glw_state->mtrl.Diffuse.g = params[1] / 255.0f;
		glw_state->mtrl.Diffuse.b = params[2] / 255.0f;
		glw_state->mtrl.Diffuse.a = params[3] / 255.0f;
		break;

	case GL_SPECULAR:
		glw_state->mtrl.Specular.r = params[0] / 255.0f;
		glw_state->mtrl.Specular.g = params[1] / 255.0f;
		glw_state->mtrl.Specular.b = params[2] / 255.0f;
		glw_state->mtrl.Specular.a = params[3] / 255.0f;
		break;

	case GL_EMISSION:
		glw_state->mtrl.Emissive.r = params[0] / 255.0f;
		glw_state->mtrl.Emissive.g = params[1] / 255.0f;
		glw_state->mtrl.Emissive.b = params[2] / 255.0f;
		glw_state->mtrl.Emissive.a = params[3] / 255.0f;
		break;

	default:
		assert(0);
		break;
	}

	glw_state->device->SetMaterial(&glw_state->mtrl);
}

static void dllMateriali(GLenum face, GLenum pname, GLint param)
{
	assert(0);
}

static void dllMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllMatrixMode(GLenum mode)
{
	switch (mode)
	{
	case GL_MODELVIEW: glw_state->matrixMode = glwstate_t::MatrixMode_Model; break;
	case GL_PROJECTION: glw_state->matrixMode = glwstate_t::MatrixMode_Projection; break;
#ifdef _XBOX
	case GL_TEXTURE0: glw_state->matrixMode = glwstate_t::MatrixMode_Texture0; break;
	case GL_TEXTURE1: glw_state->matrixMode = glwstate_t::MatrixMode_Texture1; break;
#endif
	default: assert(false); break;
	}
}

static void dllMultMatrixd(const GLdouble *m)
{
	assert(0);
}

static void dllMultMatrixf(const GLfloat *m)
{
	glw_state->matrixStack[glw_state->matrixMode]->MultMatrixLocal((D3DXMATRIX*)m);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllNewList(GLuint lnum, GLenum mode)
{
	assert(0);
}

static void setNormal(float x, float y, float z)
{
	assert(glw_state->inDrawBlock);

	_handleDrawOverflow();
	
	DWORD* push = &glw_state->drawArray[glw_state->numVertices * glw_state->drawStride + 4];
	push[0] = *((DWORD*)&x);
	push[1] = *((DWORD*)&y);
	push[2] = *((DWORD*)&z);
	push[3] = glw_state->currentColor;
}
static void dllNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	assert(0);
}

static void dllNormal3bv(const GLbyte *v)
{
	assert(0);
}

static void dllNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	assert(0);
}

static void dllNormal3dv(const GLdouble *v)
{
	assert(0);
}

static void dllNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	setNormal(nx, ny, nz);
}

static void dllNormal3fv(const GLfloat *v)
{
	setNormal(v[0], v[1], v[2]);
}

static void dllNormal3i(GLint nx, GLint ny, GLint nz)
{
	assert(0);
}

static void dllNormal3iv(const GLint *v)
{
	assert(0);
}

static void dllNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	assert(0);
}

static void dllNormal3sv(const GLshort *v)
{
	assert(0);
}

static void dllNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	assert(type == GL_FLOAT);
	
	stride = (stride == 0) ? (sizeof(GLfloat) * 3) : stride;

	glw_state->normalPointer = pointer;
	glw_state->normalStride = stride;
}

static void dllOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	D3DXMATRIX m;
	D3DXMatrixOrthoOffCenterRH(&m, left, right, top, bottom, zNear, zFar);
	glw_state->matrixStack[glw_state->matrixMode]->MultMatrix(&m);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllPassThrough(GLfloat token)
{
	assert(0);
}

static void dllPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	assert(0);
}

static void dllPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	assert(0);
}

static void dllPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	assert(0);
}

static void dllPixelStoref(GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllPixelStorei(GLenum pname, GLint param)
{
	assert(0);
}

static void dllPixelTransferf(GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllPixelTransferi(GLenum pname, GLint param)
{
	assert(0);
}

static void dllPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	assert(0);
}

static void dllPointSize(GLfloat size)
{
	glw_state->device->SetRenderState(D3DRS_POINTSCALEENABLE, TRUE);
	glw_state->device->SetRenderState(D3DRS_POINTSIZE, *((DWORD*)&size));
}

static void dllPolygonMode(GLenum face, GLenum mode)
{
	D3DFILLMODE m;
	switch (mode)
	{
	case GL_POINT: m = D3DFILL_POINT; break;
	case GL_LINE: m = D3DFILL_WIREFRAME; break;
	case GL_FILL: m = D3DFILL_SOLID; break;
	default: assert(0); break;
	}

	switch (face)
	{
	case GL_FRONT:
		glw_state->device->SetRenderState(D3DRS_FILLMODE, m);
		break;
	case GL_BACK:
#ifdef _XBOX
		glw_state->device->SetRenderState(D3DRS_BACKFILLMODE, m);
#endif
		break;
	case GL_FRONT_AND_BACK:
		glw_state->device->SetRenderState(D3DRS_FILLMODE, m);
#ifdef _XBOX
		glw_state->device->SetRenderState(D3DRS_BACKFILLMODE, m);
#endif
		break;
	}
}

static void dllPolygonOffset(GLfloat factor, GLfloat units)
{
#ifdef _XBOX
	glw_state->device->SetRenderState(D3DRS_POLYGONOFFSETZOFFSET, *((DWORD*)&factor));
	glw_state->device->SetRenderState(D3DRS_POLYGONOFFSETZSLOPESCALE, *((DWORD*)&units));
#endif
}

static void dllPolygonStipple(const GLubyte *mask)
{
	assert(0);
}

static void dllPopAttrib(void)
{
	assert(0);
}

static void dllPopClientAttrib(void)
{
	assert(0);
}

static void dllPopMatrix(void)
{
	glw_state->matrixStack[glw_state->matrixMode]->Pop();
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllPopName(void)
{
	assert(0);
}

static void dllPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	assert(0);
}

static void dllPushAttrib(GLbitfield mask)
{
	assert(0);
}

static void dllPushClientAttrib(GLbitfield mask)
{
	assert(0);
}

static void dllPushMatrix(void)
{
	glw_state->matrixStack[glw_state->matrixMode]->Push();
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllPushName(GLuint name)
{
	assert(0);
}

static void dllRasterPos2d(GLdouble x, GLdouble y)
{
	assert(0);
}

static void dllRasterPos2dv(const GLdouble *v)
{
	assert(0);
}

static void dllRasterPos2f(GLfloat x, GLfloat y)
{
	assert(0);
}

static void dllRasterPos2fv(const GLfloat *v)
{
	assert(0);
}

static void dllRasterPos2i(GLint x, GLint y)
{
	assert(0);
}

static void dllRasterPos2iv(const GLint *v)
{
	assert(0);
}

static void dllRasterPos2s(GLshort x, GLshort y)
{
	assert(0);
}

static void dllRasterPos2sv(const GLshort *v)
{
	assert(0);
}

static void dllRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	assert(0);
}

static void dllRasterPos3dv(const GLdouble *v)
{
	assert(0);
}

static void dllRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	assert(0);
}

static void dllRasterPos3fv(const GLfloat *v)
{
	assert(0);
}

static void dllRasterPos3i(GLint x, GLint y, GLint z)
{
	assert(0);
}

static void dllRasterPos3iv(const GLint *v)
{
	assert(0);
}

static void dllRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	assert(0);
}

static void dllRasterPos3sv(const GLshort *v)
{
	assert(0);
}

static void dllRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	assert(0);
}

static void dllRasterPos4dv(const GLdouble *v)
{
	assert(0);
}

static void dllRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	assert(0);
}

static void dllRasterPos4fv(const GLfloat *v)
{
	assert(0);
}

static void dllRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	assert(0);
}

static void dllRasterPos4iv(const GLint *v)
{
	assert(0);
}

static void dllRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	assert(0);
}

static void dllRasterPos4sv(const GLshort *v)
{
	assert(0);
}

static void dllReadBuffer(GLenum mode)
{
	assert(0);
}

static void dllReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	assert( 0 );
	return;

}

/**********
dllCopyBackBufferToTex
Does a direct copy of the backbuffer to the current texture. The current texture
must be linear, and it must be 640 x 480 in size. If a more complex copy is
needed, use dllCopyBackBufferToTexEXT.
**********/
static void dllCopyBackBufferToTex()
{
	glwstate_t::TextureInfo* info = _getCurrentTexture(glw_state->serverTU);
	if (info == NULL) return;

	LPDIRECT3DSURFACE8 surf;
	LPDIRECT3DSURFACE8 backbuffer;

	info->mipmap->GetSurfaceLevel(0, &surf);
	glw_state->device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

	glw_state->device->CopyRects(backbuffer, NULL, 0, surf, NULL);

	surf->Release();
	backbuffer->Release();
}

/**********
dllCopyBackBufferToTexEXT
Copies a portion of the backbuffer to a texture
If the destination is a DXT1 texture, then the buffer will be compressed
width	- width of the backbuffer polygon rendered to the destination texture
height	- height of the backbuffer polygon rendered to the destination texture
u,v		- describes the potion of the backbuffer to be copied in screen coords

The active texture (that we're replacing) NEEDS to already have enough space!
**********/
static void dllCopyBackBufferToTexEXT(float width, float height, float u1, float v1, float u2, float v2)
{
	glwstate_t::TextureInfo* info = _getCurrentTexture(glw_state->serverTU);
	if (info == NULL) return;

	// Rewriting a resource header does not enlarge its backing allocation.
	// Validate capacity before changing device state or submitting GPU writes.
	D3DSURFACE_DESC desc;
	info->mipmap->GetLevelDesc(0, &desc);
	IDirect3DTexture8 captureHeader;
	const DWORD captureBytes = XGSetTextureHeader((UINT)width, (UINT)height,
		1, 0, desc.Format, 0, &captureHeader, 0, 0);
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBStasisD3D[42] = ((unsigned int)width << 16) | (unsigned int)height;
	g_SPXBStasisD3D[43] = info->size;
	g_SPXBStasisD3D[44] = captureBytes;
	g_SPXBStasisD3D[45] = (desc.Width << 16) | desc.Height;
	++g_SPXBStasisD3D[47];
#endif
	if (!captureBytes || captureBytes > info->size)
	{
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
		++g_SPXBStasisD3D[46];
#endif
		XBLog_WriteCriticalf("STEFX_CAPTURE_CAPACITY: rejected tex=%u capture=%ux%u required=%u capacity=%u",
			glw_state->currentTexture[glw_state->serverTU], (UINT)width, (UINT)height,
			captureBytes, info->size);
		return;
	}
	static bool s_loggedCaptureCapacity = false;
	if (!s_loggedCaptureCapacity)
	{
		XBLog_WriteCriticalf("STEFX_CAPTURE_CAPACITY: ready tex=%u capture=%ux%u required=%u capacity=%u",
			glw_state->currentTexture[glw_state->serverTU], (UINT)width, (UINT)height,
			captureBytes, info->size);
		s_loggedCaptureCapacity = true;
	}

	struct QUAD { D3DXVECTOR4 p; FLOAT tu,tv;} q[4];
	q[0].p	= D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f );
	q[0].tu = u1;		q[0].tv = v1;
	q[1].p	= D3DXVECTOR4( width, 0.0f, 1.0f, 1.0f );
	q[1].tu = u2;	q[1].tv = v1;
	q[2].p	= D3DXVECTOR4( 0.0f, height , 1.0f, 1.0f );
	q[2].tu = u1;		q[2].tv = v2;
	q[3].p	= D3DXVECTOR4( width, height, 1.0f, 1.0f );
	q[3].tu = u2;	q[3].tv = v2;


	LPDIRECT3DSURFACE8	pSurface;
	LPDIRECT3DSURFACE8	pBackBuffer;
	LPDIRECT3DSURFACE8	pStencilBuffer;
	D3DTexture*			pRenderTex;
	int					w	= 0;
	int					h	= 0;

	DWORD srcblend, destblend, alphablend, alphatest, zwrite, zenable, vShader, pShader;
	DWORD colorop, colorarg1, addressu, addressv, minfilter, magfilter, colorwriteenable;

	// save the current state
	glw_state->device->GetRenderState( D3DRS_SRCBLEND, &srcblend );
	glw_state->device->GetRenderState( D3DRS_DESTBLEND, &destblend );
	glw_state->device->GetRenderState( D3DRS_ALPHABLENDENABLE, &alphablend );
	glw_state->device->GetRenderState( D3DRS_ALPHATESTENABLE, &alphatest );
	glw_state->device->GetRenderState( D3DRS_ZWRITEENABLE, &zwrite );
	glw_state->device->GetRenderState( D3DRS_ZENABLE, &zenable );
	glw_state->device->GetRenderState( D3DRS_COLORWRITEENABLE, &colorwriteenable);
	glw_state->device->GetVertexShader( &vShader );
	glw_state->device->GetPixelShader( &pShader );
	// This function no longer makes ANY attempt to restore texture stages
	glw_state->device->SetTexture(0, NULL);
	glw_state->device->SetTexture(1, NULL);
	glw_state->device->SetTexture(2, NULL);
	glw_state->device->SetTexture(3, NULL);
	glw_state->device->GetTextureStageState(0, D3DTSS_COLOROP, &colorop);
	glw_state->device->GetTextureStageState(0, D3DTSS_COLORARG1, &colorarg1);
	glw_state->device->GetTextureStageState(0, D3DTSS_ADDRESSU, &addressu);
	glw_state->device->GetTextureStageState(0, D3DTSS_ADDRESSV, &addressv);
	glw_state->device->GetTextureStageState(0, D3DTSS_MINFILTER, &minfilter);
	glw_state->device->GetTextureStageState(0, D3DTSS_MAGFILTER, &magfilter);

	// get the buffers
	glw_state->device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	glw_state->device->GetDepthStencilSurface(&pStencilBuffer);

	// get a surface desc
	info->mipmap->GetLevelDesc(0, &desc);

	// Check to see if the texture needs to be resized
	if( desc.Width != width || desc.Height != height)
	{
		// We don't actually destroy/create the texture anymore.
		// We just adjust the texture header. Thus, make sure the texture
		// is big enough when you make it the first time!

		// Replacing this with a while( IsBusy() ) loop makes it hang sometimes
		// But I can't figure out who the fuck has a lock on the texture, or
		// and it doesn't seem to cause any problems. (ie: using push on cloaked guys)
		info->mipmap->BlockUntilNotBusy();

		// Change the texture size
		XGSetTextureHeader( width,
							height,
							1,
							0,
							desc.Format,
							0,
							info->mipmap,
							0,
							0 );

		// Re-register the data:
		info->mipmap->Register( info->data );
	}

	// check to see if we want a compressed output texture
	if( desc.Format == D3DFMT_DXT1)
	{

		w	= desc.Width;
		h	= desc.Height;

		// create a new texture to use as a render target
		_d3d_check(glw_state->device->CreateTexture( w,
													 h,
													 1,
													 0,
													 D3DFMT_LIN_X8R8G8B8,
													 0,
													 &pRenderTex ), "CreateTexture");
	}
	else
	{
		pRenderTex = info->mipmap;
		
	}

	// make our current surface a render target
	pRenderTex->GetSurfaceLevel(0, &pSurface);
	glw_state->device->SetRenderTarget( pSurface, NULL );
	
	// set texture 0 to the back buffer data
	glw_state->device->SetTexture(0,(LPDIRECT3DTEXTURE8)pBackBuffer);

	// set the texture 0 state
	glw_state->device->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    glw_state->device->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    glw_state->device->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    glw_state->device->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    glw_state->device->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    glw_state->device->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
	
	// set the render state
	glw_state->device->SetRenderState( D3DRS_ZENABLE,         FALSE );
    glw_state->device->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
	glw_state->device->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
	glw_state->device->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	glw_state->device->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA | D3DBLEND_INVSRCALPHA );
	glw_state->device->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);

	// set our vertex shader and draw the backbuffer to the texture
	STEFX_D3D8_SetVertexShaderTracked( D3DFVF_XYZRHW|D3DFVF_TEX1 );
	glw_state->device->SetPixelShader( NULL );
	glw_state->device->Clear(NULL,NULL,D3DCLEAR_TARGET,D3DCOLOR_COLORVALUE(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, 0);
	glw_state->device->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, q, sizeof(QUAD) );

	// now that everything is rendered, check again to see
	// if we want a compressed texture
	if( desc.Format == D3DFMT_DXT1)
	{
		LPDIRECT3DTEXTURE8	pSrcTex;
		LPDIRECT3DTEXTURE8	pDstTex;
		D3DLOCKED_RECT		srcLock;
		D3DLOCKED_RECT		dstLock;

		pSrcTex	= pRenderTex;
		pDstTex	= info->mipmap;

		// lock our textures
		pSrcTex->LockRect(0, &srcLock, NULL, 0);
		pDstTex->LockRect(0, &dstLock, NULL, 0);

		// compress the texture
		XGCompressRect(	dstLock.pBits,
						D3DFMT_DXT1,
						dstLock.Pitch,
						w,
						h,
						srcLock.pBits,
						D3DFMT_LIN_X8R8G8B8,
						srcLock.Pitch,
						1,
						0 );

		// unlock
		pSrcTex->UnlockRect(0);
		pDstTex->UnlockRect(0);

		// release the render texture
		pRenderTex->Release();
	}

	// return our state
	glw_state->device->SetRenderState( D3DRS_SRCBLEND, srcblend );
	glw_state->device->SetRenderState( D3DRS_DESTBLEND, destblend );
	glw_state->device->SetRenderState( D3DRS_ALPHABLENDENABLE, alphablend );
	glw_state->device->SetRenderState( D3DRS_ALPHATESTENABLE, alphatest );
	glw_state->device->SetRenderState( D3DRS_ZWRITEENABLE, zwrite );
	glw_state->device->SetRenderState( D3DRS_ZENABLE, zenable );
	glw_state->device->SetRenderState( D3DRS_COLORWRITEENABLE, colorwriteenable);

	// Clear stage zero again. We're not being nice.
	glw_state->device->SetTexture(0, NULL);
	glw_state->textureStageDirty[0] = true;
	glw_state->textureStageDirty[1] = true;

	glw_state->device->SetTextureStageState(0, D3DTSS_COLOROP, colorop);
	glw_state->device->SetTextureStageState(0, D3DTSS_COLORARG1, colorarg1);
	glw_state->device->SetTextureStageState(0, D3DTSS_ADDRESSU, addressu);
	glw_state->device->SetTextureStageState(0, D3DTSS_ADDRESSV, addressv);
	glw_state->device->SetTextureStageState(0, D3DTSS_MINFILTER, minfilter);
	glw_state->device->SetTextureStageState(0, D3DTSS_MAGFILTER, magfilter);

	STEFX_D3D8_SetVertexShaderTracked( vShader );
	glw_state->device->SetPixelShader( pShader );

	glw_state->device->SetRenderTarget( pBackBuffer, pStencilBuffer );

	pSurface->Release();
	pBackBuffer->Release();
	pStencilBuffer->Release();

	glw_state->device->SetViewport(&glw_state->viewport);
	STEFX_D3D8_InvalidateTextureStageCache();
}

static void dllRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	assert(0);
}

static void dllRectdv(const GLdouble *v1, const GLdouble *v2)
{
	assert(0);
}

static void dllRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	assert(0);
}

static void dllRectfv(const GLfloat *v1, const GLfloat *v2)
{
	assert(0);
}

static void dllRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	assert(0);
}

static void dllRectiv(const GLint *v1, const GLint *v2)
{
	assert(0);
}

static void dllRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	assert(0);
}

static void dllRectsv(const GLshort *v1, const GLshort *v2)
{
	assert(0);
}

GLint dllRenderMode(GLenum mode)
{
	assert(0);
	return 0;
}

static void dllRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	assert(0);
}

static void dllRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	D3DXVECTOR3 v(x, y, z);
	glw_state->matrixStack[glw_state->matrixMode]->RotateAxisLocal(&v, angle);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllScaled(GLdouble x, GLdouble y, GLdouble z)
{
	assert(0);
}

static void dllScalef(GLfloat x, GLfloat y, GLfloat z)
{
	glw_state->matrixStack[glw_state->matrixMode]->Scale(x, y, z);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void dllScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
#ifdef _XBOX
	_fixupScreenCoords(x, y, width, height);
	STEFX_ApplySafeArea(x, y, width, height);

	glw_state->scissorBox.x1 = x;
	glw_state->scissorBox.y1 = y;
	glw_state->scissorBox.x2 = x + width;
	glw_state->scissorBox.y2 = y + height;
	
	if (glw_state->scissorEnable)
	{
		glw_state->device->SetScissors(1, FALSE, &glw_state->scissorBox);
	}
#endif
}

static void dllSelectBuffer(GLsizei size, GLuint *buffer)
{
	assert(0);
}

static void dllShadeModel(GLenum mode)
{
	D3DSHADEMODE m;
	switch (mode)
	{
	case GL_FLAT: m = D3DSHADE_FLAT; break;
	case GL_SMOOTH: default: m = D3DSHADE_GOURAUD; break;
	}
	
	glw_state->device->SetRenderState(D3DRS_SHADEMODE, m);
}

static void dllStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	D3DCMPFUNC f = _convertCompare(func);

	glw_state->device->SetRenderState(D3DRS_STENCILFUNC, f);
	glw_state->device->SetRenderState(D3DRS_STENCILREF, ref);
	glw_state->device->SetRenderState(D3DRS_STENCILMASK, mask);
}

static void dllStencilMask(GLuint mask)
{
	glw_state->device->SetRenderState(D3DRS_STENCILWRITEMASK, mask);
}

static D3DSTENCILOP _convertStencilOp(GLenum op)
{
	switch (op)
	{
	default: case GL_KEEP: return D3DSTENCILOP_KEEP;
	case GL_ZERO: return D3DSTENCILOP_ZERO;
	case GL_REPLACE: return D3DSTENCILOP_REPLACE;
	case GL_INCR: return D3DSTENCILOP_INCR;
	case GL_DECR: return D3DSTENCILOP_DECR;
	case GL_INVERT: return D3DSTENCILOP_INVERT;
	}
}

static void dllStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	D3DSTENCILOP f = _convertStencilOp(fail);
	D3DSTENCILOP zf = _convertStencilOp(zfail);
	D3DSTENCILOP zp = _convertStencilOp(zpass);

	glw_state->device->SetRenderState(D3DRS_STENCILFAIL, f);
	glw_state->device->SetRenderState(D3DRS_STENCILZFAIL, zf);
	glw_state->device->SetRenderState(D3DRS_STENCILPASS, zp);
}

static void dllTexCoord1d(GLdouble s)
{
	assert(0);
}

static void dllTexCoord1dv(const GLdouble *v)
{
	assert(0);
}

static void dllTexCoord1f(GLfloat s)
{
	assert(0);
}

static void dllTexCoord1fv(const GLfloat *v)
{
	assert(0);
}

static void dllTexCoord1i(GLint s)
{
	assert(0);
}

static void dllTexCoord1iv(const GLint *v)
{
	assert(0);
}

static void dllTexCoord1s(GLshort s)
{
	assert(0);
}

static void dllTexCoord1sv(const GLshort *v)
{
	assert(0);
}

static void setTexCoord(float s, float t)
{
	assert(glw_state->inDrawBlock);

	_handleDrawOverflow();

	int off = 0;
	if(glw_state->normalArrayState)
		off = 3;
	
	DWORD* push = &glw_state->drawArray[
		glw_state->numVertices * glw_state->drawStride + 
		4 + off + glw_state->serverTU * 2];
	
	*push++ = *((DWORD*)&s);
	*push++ = *((DWORD*)&t);
}

static void dllTexCoord2d(GLdouble s, GLdouble t)
{
	assert(0);
}

static void dllTexCoord2dv(const GLdouble *v)
{
	assert(0);
}

static void dllTexCoord2f(GLfloat s, GLfloat t)
{
	setTexCoord(s, t);
}

static void dllTexCoord2fv(const GLfloat *v)
{
	setTexCoord(v[0], v[1]);
}

static void dllTexCoord2i(GLint s, GLint t)
{
	assert(0);
}

static void dllTexCoord2iv(const GLint *v)
{
	assert(0);
}

static void dllTexCoord2s(GLshort s, GLshort t)
{
	assert(0);
}

static void dllTexCoord2sv(const GLshort *v)
{
	assert(0);
}

static void dllTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	assert(0);
}

static void dllTexCoord3dv(const GLdouble *v)
{
	assert(0);
}

static void dllTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	assert(0);
}

static void dllTexCoord3fv(const GLfloat *v)
{
	assert(0);
}

static void dllTexCoord3i(GLint s, GLint t, GLint r)
{
	assert(0);
}

static void dllTexCoord3iv(const GLint *v)
{
	assert(0);
}

static void dllTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	assert(0);
}

static void dllTexCoord3sv(const GLshort *v)
{
	assert(0);
}

static void dllTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	assert(0);
}

static void dllTexCoord4dv(const GLdouble *v)
{
	assert(0);
}

static void dllTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	assert(0);
}

static void dllTexCoord4fv(const GLfloat *v)
{
	assert(0);
}

static void dllTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	assert(0);
}

static void dllTexCoord4iv(const GLint *v)
{
	assert(0);
}

static void dllTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	assert(0);
}

static void dllTexCoord4sv(const GLshort *v)
{
	assert(0);
}

static void dllTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	assert(size == 2 && type == GL_FLOAT);

	stride = (stride == 0) ? (sizeof(GLfloat) * 2) : stride;
	
	glw_state->texCoordPointer[glw_state->clientTU] = pointer;
	glw_state->texCoordStride[glw_state->clientTU] = stride;
}

static void dllTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	qglTexEnvi(target, pname, (GLint)param);
}

static void dllTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	assert(0);
}

static void dllTexEnvi(GLenum target, GLenum pname, GLint param)
{
	assert(target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE);

	/*glwstate_t::TextureInfo* info = _getCurrentTexture(glw_state->serverTU);
	if (!info) return;*/

	D3DTEXTUREOP env;
	switch (param)
	{
	case GL_MODULATE: default: env = D3DTOP_MODULATE; break;
	case GL_REPLACE: env = D3DTOP_SELECTARG1; break;
	// MATT! - I use GL_DECAL as the bumpmapping state
	case GL_DECAL: env = D3DTOP_DOTPRODUCT3; break;
	case GL_ADD: env = D3DTOP_ADD; break;
	case GL_NONE: env = D3DTOP_DISABLE; break;
	}

	if (glw_state->textureEnv[glw_state->serverTU] != env)
	{
		glw_state->textureEnv[glw_state->serverTU] = env;
		glw_state->textureStageDirty[glw_state->serverTU] = true;
	}
}

static void dllTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	assert(0);
}

static void dllTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	assert(0);
}

static void dllTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	assert(0);
}

static void dllTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	assert(0);
}

static void dllTexGeni(GLenum coord, GLenum pname, GLint param)
{
	assert(0);
}

static void dllTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	assert(0);
}

static void dllTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	assert(0);
}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
// Bounded diagnostic for the two reported stasis materials; absent in production.
extern "C" __declspec(dllexport) volatile unsigned int g_SPXBStasisUpload[432] = {0};
static int s_stefxStasisUploadSlot = -1;
void JkaFakeglSetTextureDebugName(const char *name)
{
	char material[MAX_QPATH];
	s_stefxStasisUploadSlot = -1;
	if (!name) return;
	Q_strncpyz(material, name, sizeof(material));
	COM_StripExtension(material, material);
	if (!Q_stricmp(material, "textures/stasis/scum_256"))
		s_stefxStasisUploadSlot = 0;
	else if (!Q_stricmp(material, "textures/stasis/m_stasiswall_b"))
		s_stefxStasisUploadSlot = 1;
	else if (!strncmp(material, "*maps/stasis1/lightmap", 22))
	{
		int index = atoi(material + 22);
		if (index >= 0 && index < 25) s_stefxStasisUploadSlot = index + 2;
	}
}
static unsigned int STEFX_TextureByteHash(const void *data, unsigned int bytes)
{
	const unsigned char *p = (const unsigned char *)data;
	unsigned int hash = 2166136261u;
	while (bytes--) { hash ^= *p++; hash *= 16777619u; }
	return hash;
}
#endif

static void _texImageDDS(glwstate_t::TextureInfo* info, GLint numlevels, GLsizei width, GLsizei height, GLenum format, const GLvoid *pixels)
{
	D3DFORMAT f = D3DFMT_UNKNOWN;
	switch( format )
	{
	case GL_DDS1_EXT:
		f = D3DFMT_DXT1;
		break;
	case GL_DDS5_EXT:
		f = D3DFMT_DXT5;
		break;
	case GL_DDS_RGB16_EXT:
		f = D3DFMT_R5G6B5;
		break;
	case GL_DDS_RGBA32_EXT:
		f = D3DFMT_A8R8G8B8;
		break;
	}

	if( numlevels == 0)
		numlevels = 1;

	info->mipmap = new IDirect3DTexture8;
	info->size = XGSetTextureHeader( width,
					height,
					numlevels,
					0,
					f,
					0,
					info->mipmap,
					0,
					0 );

	DWORD fileSize = Z_Size(const_cast<void*>(pixels));
	info->data = s_bUseSkinAllocator ?
		gSkinTextures.Allocate( info->size, glw_state->currentTexture[glw_state->serverTU] ) :
		gStaticTextures.Allocate( info->size, glw_state->currentTexture[glw_state->serverTU] );
	// Lightmaps need to be swizzled, they're in 565:
	if( f == D3DFMT_R5G6B5 )
	{
		byte *pSrc = ((byte *)pixels)+(fileSize-info->size);
		byte *pDst = (byte *)info->data;
		DWORD level = numlevels;
		DWORD curWidth = width;
		DWORD curHeight = height;
		while (level--)
		{
			XGSwizzleRect(pSrc, 0, NULL, pDst, curWidth, curHeight, NULL, 2);
			pSrc += curWidth*curHeight*2;
			pDst += curWidth*curHeight*2;
			curWidth >>= 1;
			curHeight >>= 1;
		}
	}
	else
	{
		memcpy( info->data, ((byte *)pixels)+(fileSize-info->size), info->size );
	}
	info->mipmap->Register( info->data );
	info->inMemory = true;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && defined(STEFX_ELITE_FORCE_SP)
	if (s_stefxStasisUploadSlot >= 0)
	{
		volatile unsigned int *row = g_SPXBStasisUpload + s_stefxStasisUploadSlot * 16;
		++row[0]; row[1] = width; row[2] = height; row[3] = numlevels;
		row[4] = fileSize; row[5] = info->size; row[6] = format;
		row[7] = glw_state->currentTexture[glw_state->serverTU];
		row[8] = (unsigned int)info->data;
		row[9] = STEFX_TextureByteHash(pixels, fileSize);
		row[10] = STEFX_TextureByteHash(info->data, info->size);
		row[11] = ((const unsigned int *)info->mipmap)[3];
		row[12] = ((const unsigned int *)info->mipmap)[4];
		XBLog_WriteCriticalf("STEFX_STASIS_UPLOAD: slot=%d tex=%u file=%u gpu=%u sourceHash=%08x gpuHash=%08x",
			s_stefxStasisUploadSlot, row[7], row[4], row[5], row[9], row[10]);
	}
#endif
}

static void _texImageRGBA(glwstate_t::TextureInfo* info, GLint numlevels, GLint internalformat, GLsizei width, GLsizei height, GLenum format, const GLvoid *pixels)
{
	// Fix number of levels:
	if( numlevels == 0 )
		numlevels = 1;

	// What format should the resultant texture be:
	D3DFORMAT dstFormat = D3DFMT_UNKNOWN;
	switch(internalformat)
	{
	case GL_RGB5:
	case GL_RGB4_S3TC:
			dstFormat = D3DFMT_R5G6B5;
		break;

	case GL_RGBA4:
			dstFormat = D3DFMT_A4R4G4B4;
		break;

	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			dstFormat = D3DFMT_DXT1;
		break;

	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			dstFormat = D3DFMT_DXT5;
		break;

	case GL_RGB8:
	case 3:
			dstFormat = D3DFMT_X8R8G8B8;
		break;

	case GL_LIN_RGBA8:
			dstFormat = D3DFMT_LIN_A8R8G8B8;
		break;
	case GL_LIN_RGB8:
			dstFormat = D3DFMT_LIN_X8R8G8B8;
		break;

	case GL_RGBA8:
	case 4:
			dstFormat = D3DFMT_A8R8G8B8;
		break;
	case GL_RGB:
			dstFormat = D3DFMT_X8R8G8B8;
		break;

	default:
		assert(0);
	}

	// What format is our source data in:
	D3DFORMAT srcFormat = D3DFMT_UNKNOWN;
	float bpp;
	int pitch;
	switch(format)
	{
	case GL_RGB:
			srcFormat = D3DFMT_X8R8G8B8;
		bpp = 3;
		break;
		
	case GL_RGBA:
			srcFormat = D3DFMT_A8R8G8B8;
		bpp = 4;
		break;

	case GL_LIN_RGBA:
			srcFormat = D3DFMT_LIN_A8R8G8B8;
		bpp = 4;
		break;

	case GL_LIN_RGB:
			srcFormat = D3DFMT_LIN_X8R8G8B8;
		bpp = 4;
		break;

	case GL_LIN_RGB8:
			srcFormat = D3DFMT_LIN_X8R8G8B8;
		bpp = 4;
		break;

	case GL_RGB8:
			srcFormat = D3DFMT_X8R8G8B8;
		bpp = 4;
		break;

	case GL_RGB_SWIZZLE_EXT:
			srcFormat = D3DFMT_R5G6B5;
		bpp = 2;
		break;

	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			srcFormat = D3DFMT_DXT1;
		bpp = 0.5;
		break;

	default:
		assert(0);
	}

	pitch = (int)((float)width * bpp);

	RECT	srcRect;

	srcRect.top = 0;
	srcRect.left = 0;
	srcRect.right = width;
	srcRect.bottom = height;

	info->mipmap = new IDirect3DTexture8;
	info->size = XGSetTextureHeader( width,
					height,
					numlevels,
					0,
					dstFormat,
					0,
					info->mipmap,
					0,
					0 );

	info->data = s_bUseSkinAllocator ?
		gSkinTextures.Allocate( info->size, glw_state->currentTexture[glw_state->serverTU] ) :
		gStaticTextures.Allocate( info->size, glw_state->currentTexture[glw_state->serverTU] );
	info->mipmap->Register( info->data );
	info->inMemory = true;

	IDirect3DSurface8 *pSurf = NULL;
	info->mipmap->GetSurfaceLevel( 0, &pSurf );

	D3DXLoadSurfaceFromMemory( pSurf,
							   NULL,
							   NULL,
							   pixels,
							   srcFormat,
							   pitch,
							   NULL,
							   &srcRect,
							   D3DX_DEFAULT,
							   0 );

	pSurf->Release();

	// Generate mipmaps
	if( numlevels > 1)
	{
		D3DXFilterTexture( info->mipmap,
						   NULL,
						   D3DX_DEFAULT,
						   D3DX_DEFAULT );
	}
}

// EXTENSION: glTexImage2D plus "numlevels" number of mipmaps
static void dllTexImage2DEXT(GLenum target, GLint level, GLint numlevels, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	assert(target == GL_TEXTURE_2D && border == 0 && type == GL_UNSIGNED_BYTE);

	// In Direct3D, setting 0 for number of mipmap 
	// levels means create the whole chain....
	/*if(numlevels == 0)
		numlevels = 1;*/

	glwstate_t::TextureInfo* info;

	glwstate_t::texturexlat_t::iterator current = 
		glw_state->textureXlat.find(
		glw_state->currentTexture[glw_state->serverTU]);

	// If we already have a texture bound to this ID, remove it.
	if (current != glw_state->textureXlat.end())
	{
		info = &current->second;

		delete info->mipmap;
		assert( 0 );	// Why is this happening? We're leaking texture memory!
	}
	// Otherwise, initialize it.
	else
	{
		info = &glw_state->textureXlat[
			glw_state->currentTexture[glw_state->serverTU]];

		info->minFilter = D3DTEXF_NONE;
		info->mipFilter = D3DTEXF_NONE;
		info->magFilter = D3DTEXF_NONE;
		info->anisotropy = 1.f;
		info->wrapU = D3DTADDRESS_CLAMP;
		info->wrapV = D3DTADDRESS_CLAMP;
		info->fileOffset = -1;

		glw_state->textureStageDirty[glw_state->serverTU] = true;
	}

	// force any DX allocs to temp memory
//	Z_SetNewDeleteTemporary(true);

	if (format == GL_DDS1_EXT || 
		format == GL_DDS5_EXT || 
		format == GL_DDS_RGB16_EXT || 
		format == GL_DDS_RGBA32_EXT)
	{
		_texImageDDS(info, numlevels, width, height, format, pixels);
	}
	else
	{
		_texImageRGBA(info, numlevels, 
			internalformat, width, height, 
			format, pixels);
	}

	// Done DX calls to new and delete
//	Z_SetNewDeleteTemporary(false);

#if MEMORY_PROFILE
	texMemSize += getTexMemSize(info->mipmap);
#endif
}

static void dllTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	dllTexImage2DEXT(target, level, 1, internalformat, width, height, border, format, type, pixels);
}

static void dllTexParameteri(GLenum target, GLenum pname, GLint param)
{
	assert(target == GL_TEXTURE_2D);
	
	if (glw_state->currentTexture[glw_state->serverTU] == 0) return;

	glwstate_t::TextureInfo* info = _getCurrentTexture(glw_state->serverTU);
	if (!info) return;

	glw_state->textureStageDirty[glw_state->serverTU] = true;
	
	switch (pname)
	{
	case GL_TEXTURE_MIN_FILTER:
		switch (param)
		{
		case GL_NEAREST:
			info->minFilter = D3DTEXF_POINT;
			info->mipFilter = D3DTEXF_NONE;
			break;
		case GL_LINEAR:
			info->minFilter = D3DTEXF_LINEAR;
			info->mipFilter = D3DTEXF_NONE;
			break;
		case GL_NEAREST_MIPMAP_NEAREST:
			info->minFilter = D3DTEXF_POINT;
			info->mipFilter = D3DTEXF_POINT;
			break;
		case GL_LINEAR_MIPMAP_NEAREST:
			info->minFilter = D3DTEXF_LINEAR;
			info->mipFilter = D3DTEXF_POINT;
			break;
		case GL_NEAREST_MIPMAP_LINEAR:
			info->minFilter = D3DTEXF_POINT;
			info->mipFilter = D3DTEXF_LINEAR;
			break;
		case GL_LINEAR_MIPMAP_LINEAR:
			info->minFilter = D3DTEXF_LINEAR;
			info->mipFilter = D3DTEXF_LINEAR;
			break;
		}
		info->anisotropy = 1.f;
		break;
	case GL_TEXTURE_MAG_FILTER:
		switch (param)
		{
		case GL_NEAREST:
			info->magFilter = D3DTEXF_POINT;
			break;
		case GL_LINEAR:
			info->magFilter = D3DTEXF_LINEAR;
			break;
		}
		info->anisotropy = 1.f;
		break;
	case GL_TEXTURE_WRAP_S:
		switch (param)
		{
		case GL_REPEAT: info->wrapU = D3DTADDRESS_WRAP; break;
		case GL_CLAMP: info->wrapU = D3DTADDRESS_CLAMP; break;
		}
		break;
	case GL_TEXTURE_WRAP_T:
		switch (param)
		{
		case GL_REPEAT: info->wrapV = D3DTADDRESS_WRAP; break;
		case GL_CLAMP: info->wrapV = D3DTADDRESS_CLAMP; break;
		}
		break;
	case GL_TEXTURE_MAX_ANISOTROPY_EXT:
		info->anisotropy = (float)param;
		info->minFilter = D3DTEXF_ANISOTROPIC;
		info->magFilter = D3DTEXF_ANISOTROPIC;
		break;
	}
}

static void dllTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	dllTexParameteri(target, pname, param);
}

static void dllTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	// Intentionally left blank
}

static void dllTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	// Intentionally left blank
}

static void dllTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
	assert(0);
}

static void dllTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	assert(target == GL_TEXTURE_2D && level == 0 && type == GL_UNSIGNED_BYTE);

	glwstate_t::TextureInfo* info = _getCurrentTexture(glw_state->serverTU);
	if (info == NULL) return;

#if defined(STEFX_ELITE_FORCE_SP)
	// Full RGBA movie frames already match the destination pixel format.
	// D3DX creates temporary surfaces even for this update; on a loaded map it
	// can return E_OUTOFMEMORY, leaving the first movie frame on screen.
	D3DSURFACE_DESC desc;
	if (pixels && info->mipmap && info->data && level == 0 &&
		format == GL_RGBA && type == GL_UNSIGNED_BYTE && xoffset == 0 && yoffset == 0 &&
		width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
		(width & (width - 1)) == 0 && (height & (height - 1)) == 0 &&
		SUCCEEDED(info->mipmap->GetLevelDesc(0, &desc)) &&
		desc.Format == D3DFMT_A8R8G8B8 && desc.Width == (UINT)width && desc.Height == (UINT)height &&
		(unsigned int)width * (unsigned int)height * 4 <= info->size)
	{
		info->mipmap->BlockUntilNotBusy();
		D3DLOCKED_RECT locked;
		// TILED exposes the existing swizzled backing store, avoiding an
		// allocation for an unswizzled lock. Unlock preserves SDK coherency.
		HRESULT lockResult = info->mipmap->LockRect(0, &locked, NULL, D3DLOCK_TILED);
		if (FAILED(lockResult))
		{
			XBLog_WriteCriticalf("STEFX_MOVIE_TEXTURE: lock failed hr=%08x", (unsigned int)lockResult);
			return;
		}
		XGSwizzleRect(pixels, 0, NULL, locked.pBits, width, height, NULL, 4);
		info->mipmap->UnlockRect(0);
		static unsigned int updates = 0;
		++updates;
		if (updates <= 4 || updates % 120 == 0)
		{
			const unsigned int *source = (const unsigned int *)pixels;
			const unsigned int words = width * height;
			const unsigned int stride = words >= 1024 ? words / 1024 : 1;
			unsigned int hash = 2166136261u;
			for (unsigned int i = 0; i < words; i += stride) hash = (hash ^ source[i]) * 16777619u;
			XBLog_WriteCriticalf("STEFX_MOVIE_TEXTURE: update=%u size=%dx%d sourceHash=%08x temporaryBytes=0",
				updates, width, height, hash);
		}
		return;
	}
#endif

	RECT sr;
	sr.top = 0;
	sr.left = 0;
	sr.right = width;
	sr.bottom = height;

	RECT dr;
	dr.top = xoffset;
	dr.left = yoffset;
	dr.right = xoffset + width;
	dr.bottom = yoffset + height;

	Z_SetNewDeleteTemporary(true);

	LPDIRECT3DSURFACE8 surf;
	info->mipmap->GetSurfaceLevel(0, &surf);
	
	// We use the supplied format to handle pixel data correctly, the way OGL would
	D3DFORMAT srcFormat;
	switch(format)
	{
		case GL_RGB:
			srcFormat = D3DFMT_LIN_X8R8G8B8;
			break;

		case GL_RGBA:
			srcFormat = D3DFMT_LIN_A8R8G8B8;
			break;

		default:
			assert(0 && "Unsupported format in dllTexSubImage2D");
			return;
	}

	HRESULT uploadResult = D3DXLoadSurfaceFromMemory(surf, NULL, &dr, pixels,
		srcFormat, width * 4, NULL, &sr, D3DX_DEFAULT, 0);
	if (FAILED(uploadResult))
	{
		static unsigned int failures = 0;
		if (++failures <= 4)
			XBLog_WriteCriticalf("STEFX_TEXTURE_UPDATE: failed hr=%08x size=%dx%d offset=%d,%d format=%x",
				(unsigned int)uploadResult, width, height, xoffset, yoffset, format);
	}

	surf->Release();

	Z_SetNewDeleteTemporary(false);
}

static void dllTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	assert(0);
}

static void dllTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	glw_state->matrixStack[glw_state->matrixMode]->TranslateLocal(x, y, z);
	glw_state->matricesDirty[glw_state->matrixMode] = true;
}

static void setVertex(float x, float y, float z)
{
	assert(glw_state->inDrawBlock);

	_handleDrawOverflow();
	
	DWORD* push = &glw_state->drawArray[glw_state->numVertices * glw_state->drawStride];
	push[0] = *((DWORD*)&x);
	push[1] = *((DWORD*)&y);
	push[2] = *((DWORD*)&z);
	push[3] = glw_state->currentColor;

	++glw_state->numVertices;
}

static void dllVertex2d(GLdouble x, GLdouble y)
{
	assert(0);
}

static void dllVertex2dv(const GLdouble *v)
{
	assert(0);
}

static void dllVertex2f(GLfloat x, GLfloat y)
{
	setVertex(x, y, 0.f);
}

static void dllVertex2fv(const GLfloat *v)
{
	setVertex(v[0], v[1], 0.f);
}

static void dllVertex2i(GLint x, GLint y)
{
	assert(0);
}

static void dllVertex2iv(const GLint *v)
{
	assert(0);
}

static void dllVertex2s(GLshort x, GLshort y)
{
	assert(0);
}

static void dllVertex2sv(const GLshort *v)
{
	assert(0);
}

static void dllVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	assert(0);
}

static void dllVertex3dv(const GLdouble *v)
{
	assert(0);
}

static void dllVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	setVertex(x, y, z);
}

static void dllVertex3fv(const GLfloat *v)
{
	setVertex(v[0], v[1], v[2]);
}

static void dllVertex3i(GLint x, GLint y, GLint z)
{
	assert(0);
}

static void dllVertex3iv(const GLint *v)
{
	assert(0);
}

static void dllVertex3s(GLshort x, GLshort y, GLshort z)
{
	assert(0);
}

static void dllVertex3sv(const GLshort *v)
{
	assert(0);
}

static void dllVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	assert(0);
}

static void dllVertex4dv(const GLdouble *v)
{
	assert(0);
}

static void dllVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	setVertex(x, y, z);
}

static void dllVertex4fv(const GLfloat *v)
{
	setVertex(v[0], v[1], v[2]);
}

static void dllVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	assert(0);
}

static void dllVertex4iv(const GLint *v)
{
	assert(0);
}

static void dllVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	assert(0);
}

static void dllVertex4sv(const GLshort *v)
{
	assert(0);
}

static void dllVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	assert(size == 3 && type == GL_FLOAT);
	
	stride = (stride == 0) ? (sizeof(GLfloat) * 3) : stride;

	glw_state->vertexPointer = pointer;
	glw_state->vertexStride = stride;
}

static void dllViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	_fixupScreenCoords(x, y, width, height);
#ifdef _XBOX
	STEFX_ApplySafeArea(x, y, width, height);
#endif

	glw_state->viewport.X = x;
	glw_state->viewport.Y = y;
	glw_state->viewport.Width = width;
	glw_state->viewport.Height = height;
	glw_state->device->SetViewport(&glw_state->viewport);
}


static void dllMultiTexCoord2fARB(GLenum texture, GLfloat s, GLfloat t)
{
	assert(glw_state->inDrawBlock);

	_handleDrawOverflow();

	DWORD* push = &glw_state->drawArray[
		glw_state->numVertices * glw_state->drawStride + 
		4 + (texture - GL_TEXTURE0_ARB) * 2];
	
	*push++ = *((DWORD*)&s);
	*push++ = *((DWORD*)&t);
}

static void dllActiveTextureARB(GLenum texture)
{
	assert(GLW_MAX_TEXTURE_STAGES > texture - GL_TEXTURE0_ARB);
	glw_state->serverTU = texture - GL_TEXTURE0_ARB;
}

static void dllClientActiveTextureARB(GLenum texture)
{
	assert(GLW_MAX_TEXTURE_STAGES > texture - GL_TEXTURE0_ARB);
	glw_state->clientTU = texture - GL_TEXTURE0_ARB;
}


/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.  This
** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
*/
void QGL_Shutdown( void )
{
	Com_Printf ("...shutting down QGL\n" );

	qglAccum                     = NULL;
	qglAlphaFunc                 = NULL;
	qglAreTexturesResident       = NULL;
	qglArrayElement              = NULL;
	qglBegin                     = NULL;
	qglBeginEXT                  = NULL;
	qglBeginFrame                = NULL;
	qglBeginShadow               = NULL;
	qglBindTexture               = NULL;
	qglBitmap                    = NULL;
	qglBlendFunc                 = NULL;
	qglCallList                  = NULL;
	qglCallLists                 = NULL;
	qglClear                     = NULL;
	qglClearAccum                = NULL;
	qglClearColor                = NULL;
	qglClearDepth                = NULL;
	qglClearIndex                = NULL;
	qglClearStencil              = NULL;
	qglClipPlane                 = NULL;
	qglColor3b                   = NULL;
	qglColor3bv                  = NULL;
	qglColor3d                   = NULL;
	qglColor3dv                  = NULL;
	qglColor3f                   = NULL;
	qglColor3fv                  = NULL;
	qglColor3i                   = NULL;
	qglColor3iv                  = NULL;
	qglColor3s                   = NULL;
	qglColor3sv                  = NULL;
	qglColor3ub                  = NULL;
	qglColor3ubv                 = NULL;
	qglColor3ui                  = NULL;
	qglColor3uiv                 = NULL;
	qglColor3us                  = NULL;
	qglColor3usv                 = NULL;
	qglColor4b                   = NULL;
	qglColor4bv                  = NULL;
	qglColor4d                   = NULL;
	qglColor4dv                  = NULL;
	qglColor4f                   = NULL;
	qglColor4fv                  = NULL;
	qglColor4i                   = NULL;
	qglColor4iv                  = NULL;
	qglColor4s                   = NULL;
	qglColor4sv                  = NULL;
	qglColor4ub                  = NULL;
	qglColor4ubv                 = NULL;
	qglColor4ui                  = NULL;
	qglColor4uiv                 = NULL;
	qglColor4us                  = NULL;
	qglColor4usv                 = NULL;
	qglColorMask                 = NULL;
	qglColorMaterial             = NULL;
	qglColorPointer              = NULL;
	qglCopyPixels                = NULL;
	qglCopyTexImage1D            = NULL;
	qglCopyTexImage2D            = NULL;
	qglCopyTexSubImage1D         = NULL;
	qglCopyTexSubImage2D         = NULL;
	qglCullFace                  = NULL;
	qglDeleteLists               = NULL;
	qglDeleteTextures            = NULL;
	qglDepthFunc                 = NULL;
	qglDepthMask                 = NULL;
	qglDepthRange                = NULL;
	qglDisable                   = NULL;
	qglDisableClientState        = NULL;
	qglDrawArrays                = NULL;
	qglDrawBuffer                = NULL;
	qglDrawElements              = NULL;
	qglDrawPixels                = NULL;
	qglEdgeFlag                  = NULL;
	qglEdgeFlagPointer           = NULL;
	qglEdgeFlagv                 = NULL;
	qglEnable                    = NULL;
	qglEnableClientState         = NULL;
	qglEnd                       = NULL;
	qglEndFrame                  = NULL;
	qglEndShadow                 = NULL;
	qglEndList                   = NULL;
	qglEvalCoord1d               = NULL;
	qglEvalCoord1dv              = NULL;
	qglEvalCoord1f               = NULL;
	qglEvalCoord1fv              = NULL;
	qglEvalCoord2d               = NULL;
	qglEvalCoord2dv              = NULL;
	qglEvalCoord2f               = NULL;
	qglEvalCoord2fv              = NULL;
	qglEvalMesh1                 = NULL;
	qglEvalMesh2                 = NULL;
	qglEvalPoint1                = NULL;
	qglEvalPoint2                = NULL;
	qglFeedbackBuffer            = NULL;
	qglFinish                    = NULL;
	qglFlush                     = NULL;
	qglFlushShadow               = NULL;
	qglFogf                      = NULL;
	qglFogfv                     = NULL;
	qglFogi                      = NULL;
	qglFogiv                     = NULL;
	qglFrontFace                 = NULL;
	qglFrustum                   = NULL;
	qglGenLists                  = NULL;
	qglGenTextures               = NULL;
	qglGetBooleanv               = NULL;
	qglGetClipPlane              = NULL;
	qglGetDoublev                = NULL;
	qglGetError                  = NULL;
	qglGetFloatv                 = NULL;
	qglGetIntegerv               = NULL;
	qglGetLightfv                = NULL;
	qglGetLightiv                = NULL;
	qglGetMapdv                  = NULL;
	qglGetMapfv                  = NULL;
	qglGetMapiv                  = NULL;
	qglGetMaterialfv             = NULL;
	qglGetMaterialiv             = NULL;
	qglGetPixelMapfv             = NULL;
	qglGetPixelMapuiv            = NULL;
	qglGetPixelMapusv            = NULL;
	qglGetPointerv               = NULL;
	qglGetPolygonStipple         = NULL;
	qglGetString                 = NULL;
	qglGetTexEnvfv               = NULL;
	qglGetTexEnviv               = NULL;
	qglGetTexGendv               = NULL;
	qglGetTexGenfv               = NULL;
	qglGetTexGeniv               = NULL;
	qglGetTexImage               = NULL;
	qglGetTexLevelParameterfv    = NULL;
	qglGetTexLevelParameteriv    = NULL;
	qglGetTexParameterfv         = NULL;
	qglGetTexParameteriv         = NULL;
	qglHint                      = NULL;
	qglIndexedTriToStrip         = NULL;
	qglIndexMask                 = NULL;
	qglIndexPointer              = NULL;
	qglIndexd                    = NULL;
	qglIndexdv                   = NULL;
	qglIndexf                    = NULL;
	qglIndexfv                   = NULL;
	qglIndexi                    = NULL;
	qglIndexiv                   = NULL;
	qglIndexs                    = NULL;
	qglIndexsv                   = NULL;
	qglIndexub                   = NULL;
	qglIndexubv                  = NULL;
	qglInitNames                 = NULL;
	qglInterleavedArrays         = NULL;
	qglIsEnabled                 = NULL;
	qglIsList                    = NULL;
	qglIsTexture                 = NULL;
	qglLightModelf               = NULL;
	qglLightModelfv              = NULL;
	qglLightModeli               = NULL;
	qglLightModeliv              = NULL;
	qglLightf                    = NULL;
	qglLightfv                   = NULL;
	qglLighti                    = NULL;
	qglLightiv                   = NULL;
	qglLineStipple               = NULL;
	qglLineWidth                 = NULL;
	qglListBase                  = NULL;
	qglLoadIdentity              = NULL;
	qglLoadMatrixd               = NULL;
	qglLoadMatrixf               = NULL;
	qglLoadName                  = NULL;
	qglLogicOp                   = NULL;
	qglMap1d                     = NULL;
	qglMap1f                     = NULL;
	qglMap2d                     = NULL;
	qglMap2f                     = NULL;
	qglMapGrid1d                 = NULL;
	qglMapGrid1f                 = NULL;
	qglMapGrid2d                 = NULL;
	qglMapGrid2f                 = NULL;
	qglMaterialf                 = NULL;
	qglMaterialfv                = NULL;
	qglMateriali                 = NULL;
	qglMaterialiv                = NULL;
	qglMatrixMode                = NULL;
	qglMultMatrixd               = NULL;
	qglMultMatrixf               = NULL;
	qglNewList                   = NULL;
	qglNormal3b                  = NULL;
	qglNormal3bv                 = NULL;
	qglNormal3d                  = NULL;
	qglNormal3dv                 = NULL;
	qglNormal3f                  = NULL;
	qglNormal3fv                 = NULL;
	qglNormal3i                  = NULL;
	qglNormal3iv                 = NULL;
	qglNormal3s                  = NULL;
	qglNormal3sv                 = NULL;
	qglNormalPointer             = NULL;
	qglOrtho                     = NULL;
	qglPassThrough               = NULL;
	qglPixelMapfv                = NULL;
	qglPixelMapuiv               = NULL;
	qglPixelMapusv               = NULL;
	qglPixelStoref               = NULL;
	qglPixelStorei               = NULL;
	qglPixelTransferf            = NULL;
	qglPixelTransferi            = NULL;
	qglPixelZoom                 = NULL;
	qglPointSize                 = NULL;
	qglPolygonMode               = NULL;
	qglPolygonOffset             = NULL;
	qglPolygonStipple            = NULL;
	qglPopAttrib                 = NULL;
	qglPopClientAttrib           = NULL;
	qglPopMatrix                 = NULL;
	qglPopName                   = NULL;
	qglPrioritizeTextures        = NULL;
	qglPushAttrib                = NULL;
	qglPushClientAttrib          = NULL;
	qglPushMatrix                = NULL;
	qglPushName                  = NULL;
	qglRasterPos2d               = NULL;
	qglRasterPos2dv              = NULL;
	qglRasterPos2f               = NULL;
	qglRasterPos2fv              = NULL;
	qglRasterPos2i               = NULL;
	qglRasterPos2iv              = NULL;
	qglRasterPos2s               = NULL;
	qglRasterPos2sv              = NULL;
	qglRasterPos3d               = NULL;
	qglRasterPos3dv              = NULL;
	qglRasterPos3f               = NULL;
	qglRasterPos3fv              = NULL;
	qglRasterPos3i               = NULL;
	qglRasterPos3iv              = NULL;
	qglRasterPos3s               = NULL;
	qglRasterPos3sv              = NULL;
	qglRasterPos4d               = NULL;
	qglRasterPos4dv              = NULL;
	qglRasterPos4f               = NULL;
	qglRasterPos4fv              = NULL;
	qglRasterPos4i               = NULL;
	qglRasterPos4iv              = NULL;
	qglRasterPos4s               = NULL;
	qglRasterPos4sv              = NULL;
	qglReadBuffer                = NULL;
	qglReadPixels                = NULL;
	qglCopyBackBufferToTexEXT	 = NULL;
	qglCopyBackBufferToTex		 = NULL;
	qglRectd                     = NULL;
	qglRectdv                    = NULL;
	qglRectf                     = NULL;
	qglRectfv                    = NULL;
	qglRecti                     = NULL;
	qglRectiv                    = NULL;
	qglRects                     = NULL;
	qglRectsv                    = NULL;
	qglRenderMode                = NULL;
	qglRotated                   = NULL;
	qglRotatef                   = NULL;
	qglScaled                    = NULL;
	qglScalef                    = NULL;
	qglScissor                   = NULL;
	qglSelectBuffer              = NULL;
	qglShadeModel                = NULL;
	qglStencilFunc               = NULL;
	qglStencilMask               = NULL;
	qglStencilOp                 = NULL;
	qglTexCoord1d                = NULL;
	qglTexCoord1dv               = NULL;
	qglTexCoord1f                = NULL;
	qglTexCoord1fv               = NULL;
	qglTexCoord1i                = NULL;
	qglTexCoord1iv               = NULL;
	qglTexCoord1s                = NULL;
	qglTexCoord1sv               = NULL;
	qglTexCoord2d                = NULL;
	qglTexCoord2dv               = NULL;
	qglTexCoord2f                = NULL;
	qglTexCoord2fv               = NULL;
	qglTexCoord2i                = NULL;
	qglTexCoord2iv               = NULL;
	qglTexCoord2s                = NULL;
	qglTexCoord2sv               = NULL;
	qglTexCoord3d                = NULL;
	qglTexCoord3dv               = NULL;
	qglTexCoord3f                = NULL;
	qglTexCoord3fv               = NULL;
	qglTexCoord3i                = NULL;
	qglTexCoord3iv               = NULL;
	qglTexCoord3s                = NULL;
	qglTexCoord3sv               = NULL;
	qglTexCoord4d                = NULL;
	qglTexCoord4dv               = NULL;
	qglTexCoord4f                = NULL;
	qglTexCoord4fv               = NULL;
	qglTexCoord4i                = NULL;
	qglTexCoord4iv               = NULL;
	qglTexCoord4s                = NULL;
	qglTexCoord4sv               = NULL;
	qglTexCoordPointer           = NULL;
	qglTexEnvf                   = NULL;
	qglTexEnvfv                  = NULL;
	qglTexEnvi                   = NULL;
	qglTexEnviv                  = NULL;
	qglTexGend                   = NULL;
	qglTexGendv                  = NULL;
	qglTexGenf                   = NULL;
	qglTexGenfv                  = NULL;
	qglTexGeni                   = NULL;
	qglTexGeniv                  = NULL;
	qglTexImage1D                = NULL;
	qglTexImage2D                = NULL;
	qglTexImage2DEXT             = NULL;
	qglTexParameterf             = NULL;
	qglTexParameterfv            = NULL;
	qglTexParameteri             = NULL;
	qglTexParameteriv            = NULL;
	qglTexSubImage1D             = NULL;
	qglTexSubImage2D             = NULL;
	qglTranslated                = NULL;
	qglTranslatef                = NULL;
	qglVertex2d                  = NULL;
	qglVertex2dv                 = NULL;
	qglVertex2f                  = NULL;
	qglVertex2fv                 = NULL;
	qglVertex2i                  = NULL;
	qglVertex2iv                 = NULL;
	qglVertex2s                  = NULL;
	qglVertex2sv                 = NULL;
	qglVertex3d                  = NULL;
	qglVertex3dv                 = NULL;
	qglVertex3f                  = NULL;
	qglVertex3fv                 = NULL;
	qglVertex3i                  = NULL;
	qglVertex3iv                 = NULL;
	qglVertex3s                  = NULL;
	qglVertex3sv                 = NULL;
	qglVertex4d                  = NULL;
	qglVertex4dv                 = NULL;
	qglVertex4f                  = NULL;
	qglVertex4fv                 = NULL;
	qglVertex4i                  = NULL;
	qglVertex4iv                 = NULL;
	qglVertex4s                  = NULL;
	qglVertex4sv                 = NULL;
	qglVertexPointer             = NULL;
	qglViewport                  = NULL;

	qglActiveTextureARB          = NULL;
	qglClientActiveTextureARB    = NULL;
	qglMultiTexCoord2fARB        = NULL;
}

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
*/
qboolean QGL_Init( const char *dllname )
{
	qglAccum                     = dllAccum;
	qglAlphaFunc                 = dllAlphaFunc;
	qglAreTexturesResident       = dllAreTexturesResident;
	qglArrayElement              = dllArrayElement;
	qglBegin                     = dllBegin;
	qglBeginEXT                  = dllBeginEXT;
	qglBeginFrame                = dllBeginFrame;
	qglBeginShadow               = dllBeginShadow;
	qglBindTexture               = dllBindTexture;
	qglBitmap                    = dllBitmap;
	qglBlendFunc                 = dllBlendFunc;
	qglCallList                  = dllCallList;
	qglCallLists                 = dllCallLists;
	qglClear                     = dllClear;
	qglClearAccum                = dllClearAccum;
	qglClearColor                = dllClearColor;
	qglClearDepth                = dllClearDepth;
	qglClearIndex                = dllClearIndex;
	qglClearStencil              = dllClearStencil;
	qglClipPlane                 = dllClipPlane;
	qglColor3b                   = dllColor3b;
	qglColor3bv                  = dllColor3bv;
	qglColor3d                   = dllColor3d;
	qglColor3dv                  = dllColor3dv;
	qglColor3f                   = dllColor3f;
	qglColor3fv                  = dllColor3fv;
	qglColor3i                   = dllColor3i;
	qglColor3iv                  = dllColor3iv;
	qglColor3s                   = dllColor3s;
	qglColor3sv                  = dllColor3sv;
	qglColor3ub                  = dllColor3ub;
	qglColor3ubv                 = dllColor3ubv;
	qglColor3ui                  = dllColor3ui;
	qglColor3uiv                 = dllColor3uiv;
	qglColor3us                  = dllColor3us;
	qglColor3usv                 = dllColor3usv;
	qglColor4b                   = dllColor4b;
	qglColor4bv                  = dllColor4bv;
	qglColor4d                   = dllColor4d;
	qglColor4dv                  = dllColor4dv;
	qglColor4f                   = dllColor4f;
	qglColor4fv                  = dllColor4fv;
	qglColor4i                   = dllColor4i;
	qglColor4iv                  = dllColor4iv;
	qglColor4s                   = dllColor4s;
	qglColor4sv                  = dllColor4sv;
	qglColor4ub                  = dllColor4ub;
	qglColor4ubv                 = dllColor4ubv;
	qglColor4ui                  = dllColor4ui;
	qglColor4uiv                 = dllColor4uiv;
	qglColor4us                  = dllColor4us;
	qglColor4usv                 = dllColor4usv;
	qglColorMask                 = dllColorMask;
	qglColorMaterial             = dllColorMaterial;
	qglColorPointer              = dllColorPointer;
	qglCopyPixels                = dllCopyPixels;
	qglCopyTexImage1D            = dllCopyTexImage1D;
	qglCopyTexImage2D            = dllCopyTexImage2D;
	qglCopyTexSubImage1D         = dllCopyTexSubImage1D;
	qglCopyTexSubImage2D         = dllCopyTexSubImage2D;
	qglCullFace                  = dllCullFace;
	qglDeleteLists               = dllDeleteLists;
	qglDeleteTextures            = dllDeleteTextures;
	qglDepthFunc                 = dllDepthFunc;
	qglDepthMask                 = dllDepthMask;
	qglDepthRange                = dllDepthRange;
	qglDisable                   = dllDisable;
	qglDisableClientState        = dllDisableClientState;
	qglDrawArrays                = dllDrawArrays;
	qglDrawBuffer                = dllDrawBuffer;
	qglDrawElements              = dllDrawElements;
	qglDrawPixels                = dllDrawPixels;
	qglEdgeFlag                  = dllEdgeFlag;
	qglEdgeFlagPointer           = dllEdgeFlagPointer;
	qglEdgeFlagv                 = dllEdgeFlagv;
	qglEnable                    = 	dllEnable                   ;
	qglEnableClientState         = 	dllEnableClientState        ;
	qglEnd                       = 	dllEnd                      ;
	qglEndFrame                  = 	dllEndFrame                 ;
	qglEndShadow                 = 	dllEndShadow                ;
	qglEndList                   = 	dllEndList                  ;
	qglEvalCoord1d				 = 	dllEvalCoord1d				;
	qglEvalCoord1dv              = 	dllEvalCoord1dv             ;
	qglEvalCoord1f               = 	dllEvalCoord1f              ;
	qglEvalCoord1fv              = 	dllEvalCoord1fv             ;
	qglEvalCoord2d               = 	dllEvalCoord2d              ;
	qglEvalCoord2dv              = 	dllEvalCoord2dv             ;
	qglEvalCoord2f               = 	dllEvalCoord2f              ;
	qglEvalCoord2fv              = 	dllEvalCoord2fv             ;
	qglEvalMesh1                 = 	dllEvalMesh1                ;
	qglEvalMesh2                 = 	dllEvalMesh2                ;
	qglEvalPoint1                = 	dllEvalPoint1               ;
	qglEvalPoint2                = 	dllEvalPoint2               ;
	qglFeedbackBuffer            = 	dllFeedbackBuffer           ;
	qglFinish                    = 	dllFinish                   ;
	qglFlush                     = 	dllFlush                    ;
	qglFlushShadow               = 	dllFlushShadow              ;
	qglFogf                      = 	dllFogf                     ;
	qglFogfv                     = 	dllFogfv                    ;
	qglFogi                      = 	dllFogi                     ;
	qglFogiv                     = 	dllFogiv                    ;
	qglFrontFace                 = 	dllFrontFace                ;
	qglFrustum                   = 	dllFrustum                  ;
	qglGenLists                  = 	dllGenLists                 ;
	qglGenTextures               = 	dllGenTextures              ;
	qglGetBooleanv               = 	dllGetBooleanv              ;
	qglGetClipPlane              = 	dllGetClipPlane             ;
	qglGetDoublev                = 	dllGetDoublev               ;
	qglGetError                  = 	dllGetError                 ;
	qglGetFloatv                 = 	dllGetFloatv                ;
	qglGetIntegerv               = 	dllGetIntegerv              ;
	qglGetLightfv                = 	dllGetLightfv               ;
	qglGetLightiv                = 	dllGetLightiv               ;
	qglGetMapdv                  = 	dllGetMapdv                 ;
	qglGetMapfv                  = 	dllGetMapfv                 ;
	qglGetMapiv                  = 	dllGetMapiv                 ;
	qglGetMaterialfv             = 	dllGetMaterialfv            ;
	qglGetMaterialiv             = 	dllGetMaterialiv            ;
	qglGetPixelMapfv             = 	dllGetPixelMapfv            ;
	qglGetPixelMapuiv            = 	dllGetPixelMapuiv           ;
	qglGetPixelMapusv            = 	dllGetPixelMapusv           ;
	qglGetPointerv               = 	dllGetPointerv              ;
	qglGetPolygonStipple         = 	dllGetPolygonStipple        ;
	qglGetString                 = 	dllGetString                ;
	qglGetTexEnvfv               = 	dllGetTexEnvfv              ;
	qglGetTexEnviv               = 	dllGetTexEnviv              ;
	qglGetTexGendv               = 	dllGetTexGendv              ;
	qglGetTexGenfv               = 	dllGetTexGenfv              ;
	qglGetTexGeniv               = 	dllGetTexGeniv              ;
	qglGetTexImage               = 	dllGetTexImage              ;
//	qglGetTexLevelParameterfv    = 	dllGetTexLevelParameterfv   ;
//	qglGetTexLevelParameteriv    = 	dllGetTexLevelParameteriv   ;
	qglGetTexParameterfv         = 	dllGetTexParameterfv        ;
	qglGetTexParameteriv         = 	dllGetTexParameteriv        ;
	qglHint                      = 	dllHint                     ;
	qglIndexedTriToStrip         =  dllIndexedTriToStrip        ;
	qglIndexMask                 = 	dllIndexMask                ;
	qglIndexPointer              = 	dllIndexPointer             ;
	qglIndexd                    = 	dllIndexd                   ;
	qglIndexdv                   = 	dllIndexdv                  ;
	qglIndexf                    = 	dllIndexf                   ;
	qglIndexfv                   = 	dllIndexfv                  ;
	qglIndexi                    = 	dllIndexi                   ;
	qglIndexiv                   = 	dllIndexiv                  ;
	qglIndexs                    = 	dllIndexs                   ;
	qglIndexsv                   = 	dllIndexsv                  ;
	qglIndexub                   = 	dllIndexub                  ;
	qglIndexubv                  = 	dllIndexubv                 ;
	qglInitNames                 = 	dllInitNames                ;
	qglInterleavedArrays         = 	dllInterleavedArrays        ;
	qglIsEnabled                 = 	dllIsEnabled                ;
	qglIsList                    = 	dllIsList                   ;
	qglIsTexture                 = 	dllIsTexture                ;
	qglLightModelf               = 	dllLightModelf              ;
	qglLightModelfv              = 	dllLightModelfv             ;
	qglLightModeli               = 	dllLightModeli              ;
	qglLightModeliv              = 	dllLightModeliv             ;
	qglLightf                    = 	dllLightf                   ;
	qglLightfv                   = 	dllLightfv                  ;
	qglLighti                    = 	dllLighti                   ;
	qglLightiv                   = 	dllLightiv                  ;
	qglLineStipple               = 	dllLineStipple              ;
	qglLineWidth                 = 	dllLineWidth                ;
	qglListBase                  = 	dllListBase                 ;
	qglLoadIdentity              = 	dllLoadIdentity             ;
	qglLoadMatrixd               = 	dllLoadMatrixd              ;
	qglLoadMatrixf               = 	dllLoadMatrixf              ;
	qglLoadName                  = 	dllLoadName                 ;
	qglLogicOp                   = 	dllLogicOp                  ;
	qglMap1d                     = 	dllMap1d                    ;
	qglMap1f                     = 	dllMap1f                    ;
	qglMap2d                     = 	dllMap2d                    ;
	qglMap2f                     = 	dllMap2f                    ;
	qglMapGrid1d                 = 	dllMapGrid1d                ;
	qglMapGrid1f                 = 	dllMapGrid1f                ;
	qglMapGrid2d                 = 	dllMapGrid2d                ;
	qglMapGrid2f                 = 	dllMapGrid2f                ;
	qglMaterialf                 = 	dllMaterialf                ;
	qglMaterialfv                = 	dllMaterialfv               ;
	qglMateriali                 = 	dllMateriali                ;
	qglMaterialiv                = 	dllMaterialiv               ;
	qglMatrixMode                = 	dllMatrixMode               ;
	qglMultMatrixd               = 	dllMultMatrixd              ;
	qglMultMatrixf               = 	dllMultMatrixf              ;
	qglNewList                   = 	dllNewList                  ;
	qglNormal3b                  = 	dllNormal3b                 ;
	qglNormal3bv                 = 	dllNormal3bv                ;
	qglNormal3d                  = 	dllNormal3d                 ;
	qglNormal3dv                 = 	dllNormal3dv                ;
	qglNormal3f                  = 	dllNormal3f                 ;
	qglNormal3fv                 = 	dllNormal3fv                ;
	qglNormal3i                  = 	dllNormal3i                 ;
	qglNormal3iv                 = 	dllNormal3iv                ;
	qglNormal3s                  = 	dllNormal3s                 ;
	qglNormal3sv                 = 	dllNormal3sv                ;
	qglNormalPointer             = 	dllNormalPointer            ;
	qglOrtho                     = 	dllOrtho                    ;
	qglPassThrough               = 	dllPassThrough              ;
	qglPixelMapfv                = 	dllPixelMapfv               ;
	qglPixelMapuiv               = 	dllPixelMapuiv              ;
	qglPixelMapusv               = 	dllPixelMapusv              ;
	qglPixelStoref               = 	dllPixelStoref              ;
	qglPixelStorei               = 	dllPixelStorei              ;
	qglPixelTransferf            = 	dllPixelTransferf           ;
	qglPixelTransferi            = 	dllPixelTransferi           ;
	qglPixelZoom                 = 	dllPixelZoom                ;
	qglPointSize                 = 	dllPointSize                ;
	qglPolygonMode               = 	dllPolygonMode              ;
	qglPolygonOffset             = 	dllPolygonOffset            ;
	qglPolygonStipple            = 	dllPolygonStipple           ;
	qglPopAttrib                 = 	dllPopAttrib                ;
	qglPopClientAttrib           = 	dllPopClientAttrib          ;
	qglPopMatrix                 = 	dllPopMatrix                ;
	qglPopName                   = 	dllPopName                  ;
	qglPrioritizeTextures        = 	dllPrioritizeTextures       ;
	qglPushAttrib                = 	dllPushAttrib               ;
	qglPushClientAttrib          = 	dllPushClientAttrib         ;
	qglPushMatrix                = 	dllPushMatrix               ;
	qglPushName                  = 	dllPushName                 ;
	qglRasterPos2d               = 	dllRasterPos2d              ;
	qglRasterPos2dv              = 	dllRasterPos2dv             ;
	qglRasterPos2f               = 	dllRasterPos2f              ;
	qglRasterPos2fv              = 	dllRasterPos2fv             ;
	qglRasterPos2i               = 	dllRasterPos2i              ;
	qglRasterPos2iv              = 	dllRasterPos2iv             ;
	qglRasterPos2s               = 	dllRasterPos2s              ;
	qglRasterPos2sv              = 	dllRasterPos2sv             ;
	qglRasterPos3d               = 	dllRasterPos3d              ;
	qglRasterPos3dv              = 	dllRasterPos3dv             ;
	qglRasterPos3f               = 	dllRasterPos3f              ;
	qglRasterPos3fv              = 	dllRasterPos3fv             ;
	qglRasterPos3i               = 	dllRasterPos3i              ;
	qglRasterPos3iv              = 	dllRasterPos3iv             ;
	qglRasterPos3s               = 	dllRasterPos3s              ;
	qglRasterPos3sv              = 	dllRasterPos3sv             ;
	qglRasterPos4d               = 	dllRasterPos4d              ;
	qglRasterPos4dv              = 	dllRasterPos4dv             ;
	qglRasterPos4f               = 	dllRasterPos4f              ;
	qglRasterPos4fv              = 	dllRasterPos4fv             ;
	qglRasterPos4i               = 	dllRasterPos4i              ;
	qglRasterPos4iv              = 	dllRasterPos4iv             ;
	qglRasterPos4s               = 	dllRasterPos4s              ;
	qglRasterPos4sv              = 	dllRasterPos4sv             ;
	qglReadBuffer                = 	dllReadBuffer               ;
	qglReadPixels                = 	dllReadPixels               ;
	qglCopyBackBufferToTexEXT	 =	dllCopyBackBufferToTexEXT	;
	qglCopyBackBufferToTex		 =	dllCopyBackBufferToTex		;
	qglRectd                     = 	dllRectd                    ;
	qglRectdv                    = 	dllRectdv                   ;
	qglRectf                     = 	dllRectf                    ;
	qglRectfv                    = 	dllRectfv                   ;
	qglRecti                     = 	dllRecti                    ;
	qglRectiv                    = 	dllRectiv                   ;
	qglRects                     = 	dllRects                    ;
	qglRectsv                    = 	dllRectsv                   ;
	qglRenderMode                = 	dllRenderMode               ;
	qglRotated                   = 	dllRotated                  ;
	qglRotatef                   = 	dllRotatef                  ;
	qglScaled                    = 	dllScaled                   ;
	qglScalef                    = 	dllScalef                   ;
	qglScissor                   = 	dllScissor                  ;
	qglSelectBuffer              = 	dllSelectBuffer             ;
	qglShadeModel                = 	dllShadeModel               ;
	qglStencilFunc               = 	dllStencilFunc              ;
	qglStencilMask               = 	dllStencilMask              ;
	qglStencilOp                 = 	dllStencilOp                ;
	qglTexCoord1d                = 	dllTexCoord1d               ;
	qglTexCoord1dv               = 	dllTexCoord1dv              ;
	qglTexCoord1f                = 	dllTexCoord1f               ;
	qglTexCoord1fv               = 	dllTexCoord1fv              ;
	qglTexCoord1i                = 	dllTexCoord1i               ;
	qglTexCoord1iv               = 	dllTexCoord1iv              ;
	qglTexCoord1s                = 	dllTexCoord1s               ;
	qglTexCoord1sv               = 	dllTexCoord1sv              ;
	qglTexCoord2d                = 	dllTexCoord2d               ;
	qglTexCoord2dv               = 	dllTexCoord2dv              ;
	qglTexCoord2f                = 	dllTexCoord2f               ;
	qglTexCoord2fv               = 	dllTexCoord2fv              ;
	qglTexCoord2i                = 	dllTexCoord2i               ;
	qglTexCoord2iv               = 	dllTexCoord2iv              ;
	qglTexCoord2s                = 	dllTexCoord2s               ;
	qglTexCoord2sv               = 	dllTexCoord2sv              ;
	qglTexCoord3d                = 	dllTexCoord3d               ;
	qglTexCoord3dv               = 	dllTexCoord3dv              ;
	qglTexCoord3f                = 	dllTexCoord3f               ;
	qglTexCoord3fv               = 	dllTexCoord3fv              ;
	qglTexCoord3i                = 	dllTexCoord3i               ;
	qglTexCoord3iv               = 	dllTexCoord3iv              ;
	qglTexCoord3s                = 	dllTexCoord3s               ;
	qglTexCoord3sv               = 	dllTexCoord3sv              ;
	qglTexCoord4d                = 	dllTexCoord4d               ;
	qglTexCoord4dv               = 	dllTexCoord4dv              ;
	qglTexCoord4f                = 	dllTexCoord4f               ;
	qglTexCoord4fv               = 	dllTexCoord4fv              ;
	qglTexCoord4i                = 	dllTexCoord4i               ;
	qglTexCoord4iv               = 	dllTexCoord4iv              ;
	qglTexCoord4s                = 	dllTexCoord4s               ;
	qglTexCoord4sv               = 	dllTexCoord4sv              ;
	qglTexCoordPointer           = 	dllTexCoordPointer          ;
	qglTexEnvf                   = 	dllTexEnvf                  ;
	qglTexEnvfv                  = 	dllTexEnvfv                 ;
	qglTexEnvi                   = 	dllTexEnvi                  ;
	qglTexEnviv                  = 	dllTexEnviv                 ;
	qglTexGend                   = 	dllTexGend                  ;
	qglTexGendv                  = 	dllTexGendv                 ;
	qglTexGenf                   = 	dllTexGenf                  ;
	qglTexGenfv                  = 	dllTexGenfv                 ;
	qglTexGeni                   = 	dllTexGeni                  ;
	qglTexGeniv                  = 	dllTexGeniv                 ;
	qglTexImage1D                = 	dllTexImage1D               ;
	qglTexImage2D                = 	dllTexImage2D               ;
	qglTexImage2DEXT             = 	dllTexImage2DEXT            ;
	qglTexParameterf             = 	dllTexParameterf            ;
	qglTexParameterfv            = 	dllTexParameterfv           ;
	qglTexParameteri             = 	dllTexParameteri            ;
	qglTexParameteriv            = 	dllTexParameteriv           ;
	qglTexSubImage1D             = 	dllTexSubImage1D            ;
	qglTexSubImage2D             = 	dllTexSubImage2D            ;
	qglTranslated                = 	dllTranslated               ;
	qglTranslatef                = 	dllTranslatef               ;
	qglVertex2d                  = 	dllVertex2d                 ;
	qglVertex2dv                 = 	dllVertex2dv                ;
	qglVertex2f                  = 	dllVertex2f                 ;
	qglVertex2fv                 = 	dllVertex2fv                ;
	qglVertex2i                  = 	dllVertex2i                 ;
	qglVertex2iv                 = 	dllVertex2iv                ;
	qglVertex2s                  = 	dllVertex2s                 ;
	qglVertex2sv                 = 	dllVertex2sv                ;
	qglVertex3d                  = 	dllVertex3d                 ;
	qglVertex3dv                 = 	dllVertex3dv                ;
	qglVertex3f                  = 	dllVertex3f                 ;
	qglVertex3fv                 = 	dllVertex3fv                ;
	qglVertex3i                  = 	dllVertex3i                 ;
	qglVertex3iv                 = 	dllVertex3iv                ;
	qglVertex3s                  = 	dllVertex3s                 ;
	qglVertex3sv                 = 	dllVertex3sv                ;
	qglVertex4d                  = 	dllVertex4d                 ;
	qglVertex4dv                 = 	dllVertex4dv                ;
	qglVertex4f                  = 	dllVertex4f                 ;
	qglVertex4fv                 = 	dllVertex4fv                ;
	qglVertex4i                  = 	dllVertex4i                 ;
	qglVertex4iv                 = 	dllVertex4iv                ;
	qglVertex4s                  = 	dllVertex4s                 ;
	qglVertex4sv                 = 	dllVertex4sv                ;
	qglVertexPointer             = 	dllVertexPointer            ;
	qglViewport                  = 	dllViewport                 ;

	qglActiveTextureARB          =  dllActiveTextureARB         ;
	qglClientActiveTextureARB    =  dllClientActiveTextureARB   ;
	qglMultiTexCoord2fARB        =  dllMultiTexCoord2fARB       ;

	return qtrue;
}

void QGL_EnableLogging( qboolean enable )
{
}

// Extra functions bound to d3d_ commands for controlling crazy D3D performance things
#if !defined(FINAL_BUILD) && !defined(_XBOX_VC71_MIGRATION)

// D3D_AutoPerfData controls automatic display of performance information:
// framerate, push buffer data, etc... Usage:
// d3d_autoperf				- Toggle on and off
// d3d_autoperf n			- Set display frequency in ms (default 5000)
static void D3D_AutoPerfData_f( void )
{
	static DWORD sdwInterval = 5000;
	static bool sbEnabled = false;

	int numArgs = Cmd_Argc();

	if (numArgs > 2)
	{
		Com_Printf("D3D_AutoPerfData_f: Too many arguments.\n");
	}
	else if (numArgs <= 1)
	{
		sbEnabled = !sbEnabled;
		D3DPERF_SetShowFrameRateInterval(sbEnabled ? sdwInterval : 0);
	}
	else // numArgs == 2 -> Exactly one real argument
	{
		int new_interval = atoi(Cmd_Argv(1));

		if (!new_interval)
		{
			// Fancy way to turn it off, don't change stored interval
			sbEnabled = false;
		}
		else
		{
			// Force it on
			sdwInterval = new_interval;
			sbEnabled = true;
		}
		D3DPERF_SetShowFrameRateInterval(sbEnabled ? sdwInterval : 0);
	}
}

#endif

extern void GLimp_SetGamma(float);


static void _createWindow(int width, int height, int colorbits, qboolean cdsFullscreen)
{
	glConfig.colorBits = colorbits;

	if ( r_depthbits->integer == 0 ) {
		if ( colorbits > 16 ) {
			glConfig.depthBits = 24;
		} else {
			glConfig.depthBits = 16;
		}
	} else {
		glConfig.depthBits = r_depthbits->integer;
	}
	
	glConfig.stencilBits = r_stencilbits->integer;
	if ( glConfig.depthBits < 24 )
	{
		glConfig.stencilBits = 0;
	}
	
	glConfig.displayFrequency = 75;
	glConfig.stereoEnabled = qfalse;

	// VVFIXME : This is surely wrong.
	glConfig.vidHeight = height;
	glConfig.vidWidth = width;

}

enum VideoModes
{
	VM_480i = 0,
	VM_480p,
	VM_720p,
	VM_1080i
};

// Global lighteffects instance, to avoid allocating any memory
// during GLW_Init()
static LightEffects *getLightEffects( void )
{
	static LightEffects le;
	return &le;
}

void GLW_Init(int width, int height, int colorbits, qboolean cdsFullscreen)
{
//	glw_state = new glwstate_t;
	glw_state = &g_glwState;
	InitializeRetailTexturePoolsDeferred();
	int mode = VM_480i;

	glw_state->isWidescreen = false;
	if( XGetVideoFlags() & XC_VIDEO_FLAGS_WIDESCREEN )
	{
		glw_state->isWidescreen = true;
		width = 640;


		/*if( XGetVideoFlags() & XC_VIDEO_FLAGS_HDTV_720p )
		{
			width = 1280;
			height = 720;
			mode = VM_720p;
		}

		if( XGetVideoFlags() & XC_VIDEO_FLAGS_HDTV_1080i )
		{
			width = 1920;
			height = 1080;
			mode = VM_1080i;
		}*/
	}

	// 480p capability is independent of the dashboard's aspect-ratio setting.
	if( XGetVideoFlags() & XC_VIDEO_FLAGS_HDTV_480p )
	{
		width = 640;
		height = 480;
		mode = VM_480p;
	}

	_createWindow(width, height, colorbits, cdsFullscreen);
	
	glw_state->matrixMode = glwstate_t::MatrixMode_Model;
	glw_state->inDrawBlock = false;
	
	glw_state->serverTU = 0;
	glw_state->clientTU = 0;
	
	glw_state->colorArrayState = false;
	glw_state->vertexArrayState = false;
	glw_state->normalArrayState = false;

	glw_state->cullEnable = true;
	glw_state->cullMode = D3DCULL_CCW;

	glw_state->scissorEnable = false;
	glw_state->scissorBox.x1 = 0;
	glw_state->scissorBox.y1 = 0;
	glw_state->scissorBox.x2 = glConfig.vidWidth;
	glw_state->scissorBox.y2 = glConfig.vidHeight;

	glw_state->shaderMask = 0;
	glw_state->shaderMaskValid = false;
	glw_state->streamSourceZeroStride = 0;
	glw_state->streamSourceZeroValid = false;

	glw_state->clearColor = D3DCOLOR_RGBA(255, 255, 255, 255);
	glw_state->clearDepth = 1.f;
	glw_state->clearStencil = 0;

	glw_state->currentColor = D3DCOLOR_RGBA(255, 255, 255, 255);

	glw_state->viewport.MinZ = 0.f;
	glw_state->viewport.MaxZ = 1.f;

	for (int t = 0; t < GLW_MAX_TEXTURE_STAGES; ++t)
	{
		glw_state->textureEnv[t] = D3DTOP_MODULATE;
		glw_state->texCoordArrayState[t] = false;
		glw_state->currentTexture[t] = 0;
		glw_state->textureStageDirty[t] = false;
	}

	glw_state->textureBindNum = 1;
	
D3DPRESENT_PARAMETERS present;
	present.BackBufferWidth = width;
	present.BackBufferHeight = height;
	present.BackBufferFormat = D3DFMT_A8R8G8B8;
	present.BackBufferCount = 1;
	present.MultiSampleType = D3DMULTISAMPLE_NONE;
	present.SwapEffect = D3DSWAPEFFECT_DISCARD;
	present.hDeviceWindow = 0;
	present.Windowed = FALSE;
	present.EnableAutoDepthStencil = TRUE;
	present.AutoDepthStencilFormat = D3DFMT_LIN_D24S8;
	present.Flags = 0;
	if( glw_state->isWidescreen )
	{
		present.Flags = D3DPRESENTFLAG_WIDESCREEN;
		//else if(mode == VM_720p)
		//{
		//	present.Flags |= D3DPRESENTFLAG_PROGRESSIVE;
		//}
		//else if(mode == VM_1080i)
		//{
		//	present.Flags |= D3DPRESENTFLAG_INTERLACED; // | D3DPRESENTFLAG_FIELD;
		//}


        present.Flags |= D3DPRESENTFLAG_WIDESCREEN;
	}

	if(mode == VM_480p)
	{
		present.Flags |= D3DPRESENTFLAG_PROGRESSIVE;
	}
	s_xboxPresentationFlags = present.Flags;

	present.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	present.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	present.BufferSurfaces[0] = NULL;
	present.BufferSurfaces[1] = NULL;
	present.BufferSurfaces[2] = NULL;
	present.DepthStencilSurface = NULL;

	if (IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT,
								D3DDEVTYPE_HAL,
								NULL,
								D3DCREATE_HARDWARE_VERTEXPROCESSING,
								&present,
								&glw_state->device) != D3D_OK)
	{
		Com_Printf("Failed to create device. That's bad.\n");
	}
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
	else
	{
		int textureStage;
		for ( textureStage = 0; textureStage < GLW_MAX_TEXTURE_STAGES; ++textureStage )
		{
			glw_state->device->SetTextureStageState(
				textureStage, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			glw_state->device->SetTextureStageState(
				textureStage, D3DTSS_COLORARG2, D3DTA_CURRENT );
			glw_state->device->SetTextureStageState(
				textureStage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			glw_state->device->SetTextureStageState(
				textureStage, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
			glw_state->device->SetTextureStageState(
				textureStage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
		}
		XBLog_WriteCriticalf(
			"STEFX_D3D8: invariantTextureStageState=hoisted stages=%d sharedDevice=1",
			GLW_MAX_TEXTURE_STAGES );
	}
#endif
//	qglEnable(GL_VSYNC);
	
	for (int m = 0; m < glwstate_t::Num_MatrixModes; ++m)
	{
		D3DXCreateMatrixStack(0, &glw_state->matrixStack[m]);
		glw_state->matrixStack[m]->LoadIdentity();
		glw_state->matricesDirty[m] = false;
	}

	// VVFIXME: Hack - turn off lighting
	dllDisable(GL_LIGHTING);

	// Set a material (for lighting)
	memset( &glw_state->mtrl, 0, sizeof(D3DMATERIAL8) );
	glw_state->mtrl.Diffuse.r = glw_state->mtrl.Ambient.r = 1.0f;
	glw_state->mtrl.Diffuse.g = glw_state->mtrl.Ambient.g = 1.0f;
	glw_state->mtrl.Diffuse.b = glw_state->mtrl.Ambient.b = 1.0f;
	glw_state->mtrl.Diffuse.a = glw_state->mtrl.Ambient.a = 1.0f;
	glw_state->device->SetMaterial( &glw_state->mtrl );
	// Gamma hack
	GLimp_SetGamma(1.3f);

	// Set up our directional light (used for diffuse lighting)
	for(int i = 0; i < 2; i++)
	{
		memset(&glw_state->dirLight[i], 0, sizeof(D3DLIGHT8));

		// Set up a white point light.
		glw_state->dirLight[i].Type = D3DLIGHT_DIRECTIONAL;
		glw_state->dirLight[i].Diffuse.r  = 1.0f;
		glw_state->dirLight[i].Diffuse.g  = 1.0f;
		glw_state->dirLight[i].Diffuse.b  = 1.0f;
		glw_state->dirLight[i].Direction.x = 1.0f;
		glw_state->dirLight[i].Direction.y = 0.0f;
		glw_state->dirLight[i].Direction.z = 0.0f;

		// Don't attenuate.
		glw_state->dirLight[i].Attenuation0 = 1.0f; 
		glw_state->dirLight[i].Range        = 1000.0f;
	}

	//glw_state->drawArray = new DWORD[SHADER_MAX_VERTEXES * 12];
	glw_state->drawArray = NULL;

#ifdef _XBOX
#ifdef VV_LIGHTING
//	glw_state->lightEffects = new LightEffects;
	glw_state->lightEffects = getLightEffects();
#endif // VV_LIGHTING
//	HDREffect.Initialize();
#endif

#if !defined(FINAL_BUILD) && !defined(_XBOX_VC71_MIGRATION)
	Cmd_AddCommand("d3d_autoperf", D3D_AutoPerfData_f);
#endif
}

void GLW_Shutdown(void)
{
#if defined(_XBOX) && (defined(STEFX_SP_HOSTED_MP) || defined(STEFX_ELITE_FORCE_SP))
	STEFX_WorldVerticesReset();
#endif
#ifdef _XBOX
#ifdef VV_LIGHTING
//	delete glw_state->lightEffects;
	glw_state->lightEffects = NULL;
#endif
#endif

	for (int m = 0; m < glwstate_t::Num_MatrixModes; ++m)
	{
		glw_state->matrixStack[m]->Release();
	}

	glw_state->device->Release();

#ifdef _XBOX
//	delete glw_state->flareEffect;
#endif

//	delete glw_state;
	glw_state = NULL;
}

#define CSS_IMAGE_HDR_SIZE 2048
#define CSS_IMAGE_DATA_SIZE ((SAVE_GAME_IMAGE_W * SAVE_GAME_IMAGE_H) / 2)

struct XprImageHeader
{
	XPR_HEADER xpr;
	IDirect3DTexture8 txt;
	DWORD dwEndOfHeader;
};

struct XprImage
{
	XprImageHeader hdr;
	CHAR strPad[CSS_IMAGE_HDR_SIZE - sizeof(XprImageHeader)];
	BYTE pBits[CSS_IMAGE_DATA_SIZE];
};

void SaveCompressedScreenshot(void)
{
	LPDIRECT3DSURFACE8 screenShot = 0;
	LPDIRECT3DSURFACE8 compressedSaveGameImage = 0;
	RECT srcRect;
	DWORD dwSize;

	glw_state->device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &screenShot);
	srcRect.left = 48;
	srcRect.top = 36;
	srcRect.right = 592;
	srcRect.bottom = 444;
	glw_state->device->CreateImageSurface(SAVE_GAME_IMAGE_W, SAVE_GAME_IMAGE_H,
		D3DFMT_DXT1, &compressedSaveGameImage);
	D3DXLoadSurfaceFromSurface(compressedSaveGameImage, NULL, NULL, screenShot,
		NULL, &srcRect, D3DX_DEFAULT, D3DCOLOR(0));
	XGWriteSurfaceOrTextureToXPR(compressedSaveGameImage,
		"Z:\\screenshot.xbx", TRUE);
	if (compressedSaveGameImage)
	{
		compressedSaveGameImage->Release();
		compressedSaveGameImage = 0;
	}

	byte xbxFile[32 * 1024];
	HANDLE hFile = CreateFile("Z:\\screenshot.xbx", GENERIC_READ | GENERIC_WRITE,
		0, 0, OPEN_EXISTING, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		ReadFile(hFile, xbxFile, sizeof(xbxFile), &dwSize, NULL);
		XCALCSIG_SIGNATURE xbxSig;
		HANDLE hSig = XCalculateSignatureBegin(XCALCSIG_FLAG_SAVE_GAME);
		XCalculateSignatureUpdate(hSig, xbxFile, dwSize);
		XCalculateSignatureEnd(hSig, &xbxSig);
		SetFilePointer(hFile, 0, NULL, FILE_END);
		WriteFile(hFile, &xbxSig, sizeof(xbxSig), &dwSize, NULL);
		CloseHandle(hFile);
	}

	glw_state->device->CreateImageSurface(64, 64, D3DFMT_DXT1,
		&compressedSaveGameImage);
	srcRect.left = (glConfig.vidWidth / 2) - 120;
	srcRect.right = srcRect.left + 240;
	srcRect.top = (glConfig.vidHeight / 2) - 120;
	if (Cvar_VariableValue("cg_thirdPerson"))
	{
		srcRect.top += 60;
	}
	srcRect.bottom = srcRect.top + 240;
	D3DXLoadSurfaceFromSurface(compressedSaveGameImage, NULL, NULL, screenShot,
		NULL, &srcRect, D3DX_DEFAULT, D3DCOLOR(0));
	XGWriteSurfaceOrTextureToXPR(compressedSaveGameImage,
		"Z:\\saveimage.xbx", TRUE);
	if (compressedSaveGameImage)
	{
		compressedSaveGameImage->Release();
	}
	if (screenShot)
	{
		screenShot->Release();
	}
}

BOOL LoadCompressedScreenshot(const char *filename)
{
	glwstate_t::TextureInfo *info = _getCurrentTexture(glw_state->serverTU);
	if (!info)
	{
		return FALSE;
	}

	DWORD dwBytesRead;
	BOOL success;
	HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		glw_state->device->SetTexture(0, NULL);
		glw_state->device->SetTexture(1, NULL);
		glw_state->textureStageDirty[0] = true;
		glw_state->textureStageDirty[1] = true;
		while (info->mipmap->IsBusy()) {}
		D3DLOCKED_RECT lockedRect;
		info->mipmap->LockRect(0, &lockedRect, NULL, D3DLOCK_TILED);
		memset(lockedRect.pBits, 0, CSS_IMAGE_DATA_SIZE);
		info->mipmap->UnlockRect(0);
		return FALSE;
	}

	XprImage image;
	success = ReadFile(hFile, &image, sizeof(image), &dwBytesRead, NULL);
	success &= dwBytesRead == sizeof(image) &&
		image.hdr.xpr.dwMagic == XPR_MAGIC_VALUE &&
		image.hdr.xpr.dwTotalSize == CSS_IMAGE_HDR_SIZE + CSS_IMAGE_DATA_SIZE &&
		image.hdr.xpr.dwHeaderSize == CSS_IMAGE_HDR_SIZE &&
		image.hdr.dwEndOfHeader == 0xFFFFFFFF;

	XCALCSIG_SIGNATURE fileSignature;
	success &= ReadFile(hFile, &fileSignature, sizeof(fileSignature),
		&dwBytesRead, NULL);
	if (SetFilePointer(hFile, 0, NULL, FILE_END) !=
		(sizeof(image) + sizeof(fileSignature)))
	{
		success = FALSE;
	}
	CloseHandle(hFile);

	XCALCSIG_SIGNATURE dataSignature;
	HANDLE hSig = XCalculateSignatureBegin(XCALCSIG_FLAG_SAVE_GAME);
	XCalculateSignatureUpdate(hSig, (const BYTE *)&image, sizeof(image));
	XCalculateSignatureEnd(hSig, &dataSignature);
	if (memcmp(&fileSignature, &dataSignature, sizeof(fileSignature)) != 0)
	{
		success = FALSE;
	}

	glw_state->device->SetTexture(0, NULL);
	glw_state->device->SetTexture(1, NULL);
	glw_state->textureStageDirty[0] = true;
	glw_state->textureStageDirty[1] = true;
	while (info->mipmap->IsBusy()) {}
	D3DLOCKED_RECT lockedRect;
	info->mipmap->LockRect(0, &lockedRect, NULL, D3DLOCK_TILED);
	if (success)
	{
		memcpy(lockedRect.pBits, image.pBits, sizeof(image.pBits));
	}
	else
	{
		memset(lockedRect.pBits, 0, CSS_IMAGE_DATA_SIZE);
	}
	info->mipmap->UnlockRect(0);
	return success;
}




bool CreateVertexShader( const CHAR* strFilename, const DWORD* pdwVertexDecl, DWORD* pdwVertexShader )
{
    HRESULT hr;

    // Open the vertex shader file
    HANDLE hFile;
    DWORD dwNumBytesRead;
    hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
        return false;

    // Allocate memory to read the vertex shader file
    DWORD dwSize = GetFileSize(hFile, NULL);
    BYTE* pData  = new BYTE[dwSize+4];
    if( NULL == pData )
    {
        CloseHandle( hFile );
        return false;
    }
    ZeroMemory( pData, dwSize+4 );

    // Read the pre-compiled vertex shader microcode
    ReadFile(hFile, pData, dwSize, &dwNumBytesRead, 0);
    
    // Create the vertex shader
    hr = glw_state->device->CreateVertexShader( pdwVertexDecl, (const DWORD*)pData,
                                        pdwVertexShader, 0 );

    // Cleanup and return
    CloseHandle( hFile );
    delete [] pData;

	if(hr == S_OK)
		return true;

	return false;
}


bool CreatePixelShader( const CHAR* strFilename, DWORD* pdwPixelShader )
{
    HRESULT hr;

    // Open the pixel shader file
    HANDLE hFile;
    DWORD dwNumBytesRead;
    hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
        return false;

    // Load the pre-compiled pixel shader microcode
    D3DPIXELSHADERDEF_FILE psdf;
    ReadFile( hFile, &psdf, sizeof(D3DPIXELSHADERDEF_FILE), &dwNumBytesRead, NULL );
    
    // Make sure the pixel shader is valid
    if( psdf.FileID != D3DPIXELSHADERDEF_FILE_ID )
    {
        CloseHandle( hFile );
        return false;
    }

    // Create the pixel shader
    if( FAILED( hr = glw_state->device->CreatePixelShader( &(psdf.Psd), pdwPixelShader ) ) )
    {
        CloseHandle( hFile );
        return false;
    }

    // Cleanup
    CloseHandle( hFile );

    return true;
}
