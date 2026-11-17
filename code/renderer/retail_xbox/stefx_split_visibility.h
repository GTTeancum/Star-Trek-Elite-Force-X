#include "../../qcommon/stefx_split_perf_defaults.h"
// Cache the visibility markings actually installed in the shared BSP, not the
// refdef restored after a split view. Frustum traversal remains per viewport.
extern "C" volatile unsigned int g_SPXBSplitVisibility[8] = {0};
// mode, requests, equivalent keys, skips, verified, mismatches, resets, overflow
static bool s_splitVisibilityValid, s_splitVisibilityFailed;
static const world_t *s_splitVisibilityWorld;
static int s_splitVisibilityStamp, s_splitVisibilityCluster;
static int s_splitVisibilityIgnorePvs, s_splitVisibilityIgnoreArea, s_splitVisibilityNoVis;
static bool s_splitVisibilityWorstLeaf;
static byte s_splitVisibilityMask[MAX_MAP_AREA_BYTES];
static cvar_t *s_splitVisibilityMode;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
static byte s_splitVisibilityBefore[8192];
static bool s_splitVisibilityVerify;
static bool STEFX_VisibilityBit(int index) {
    return index < tr.world->numnodes
        ? tr.world->nodes[index].visframe == tr.visCount
        : tr.world->leafs[index-tr.world->numnodes].visframe == tr.visCount;
}
#endif

void R_STEFX_ResetVisibilityReuse(void) {
    s_splitVisibilityValid = false;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
    s_splitVisibilityVerify = false;
#endif
    ++g_SPXBSplitVisibility[6];
}

static bool STEFX_ReuseSplitVisibility(int cluster, int ignorePvs, int ignoreArea) {
    if (!s_splitVisibilityMode)
        s_splitVisibilityMode = Cvar_Get("r_efSplitVisReuse", STEFX_SPLIT_VIS_DEFAULT, 0);
    int mode = s_splitVisibilityMode->integer;
    if (s_splitVisibilityFailed || !tr.viewParms.stefxSplitView ||
        tr.viewParms.isPortal || lookingForWorstLeaf || mode < 1 || mode > 2) mode = 0;
#if !defined(STEFX_HW_FRAME_DIAGNOSTICS)
    if (mode == 2) mode = 0;
#endif
    g_SPXBSplitVisibility[0] = mode;
    if (!mode) return false;
    ++g_SPXBSplitVisibility[1];
    if (!s_splitVisibilityValid || s_splitVisibilityWorld != tr.world ||
        s_splitVisibilityStamp != tr.visCount || s_splitVisibilityCluster != cluster ||
        tr.viewCluster != cluster || s_splitVisibilityIgnorePvs != ignorePvs ||
        s_splitVisibilityIgnoreArea != ignoreArea || s_splitVisibilityNoVis != r_novis->integer ||
        s_splitVisibilityWorstLeaf != lookingForWorstLeaf ||
        memcmp(s_splitVisibilityMask, tr.refdef.areamask, MAX_MAP_AREA_BYTES)) return false;
    ++g_SPXBSplitVisibility[2];
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
    if (mode == 2) {
        int count = tr.world->numnodes + tr.world->numleafs;
        if (count < 0 || count > sizeof(s_splitVisibilityBefore)*8) {
            ++g_SPXBSplitVisibility[7];
            return false;
        }
        memset(s_splitVisibilityBefore, 0, sizeof(s_splitVisibilityBefore));
        for (int i=0; i<count; ++i)
            if (STEFX_VisibilityBit(i)) s_splitVisibilityBefore[i>>3] |= 1u << (i&7);
        s_splitVisibilityVerify = true;
        return false; // Recompute normally, then compare every node and leaf.
    }
#endif
    ++g_SPXBSplitVisibility[3];
    return true;
}

static void STEFX_RecordSplitVisibility(int cluster, int ignorePvs, int ignoreArea) {
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
    if (s_splitVisibilityVerify) {
        s_splitVisibilityVerify = false;
        ++g_SPXBSplitVisibility[4];
        for (int i=0; i<tr.world->numnodes+tr.world->numleafs; ++i) {
            const bool before = (s_splitVisibilityBefore[i>>3] & (1u<<(i&7))) != 0;
            if (before != STEFX_VisibilityBit(i)) {
                ++g_SPXBSplitVisibility[5];
                s_splitVisibilityFailed = true;
                XBLog_WriteCriticalf("STEFX_SPLIT_VIS: mismatch index=%d cluster=%d; reuse disabled", i, cluster);
                break;
            }
        }
    }
#endif
    s_splitVisibilityWorld = tr.world;
    s_splitVisibilityStamp = tr.visCount;
    s_splitVisibilityCluster = cluster;
    s_splitVisibilityIgnorePvs = ignorePvs;
    s_splitVisibilityIgnoreArea = ignoreArea;
    s_splitVisibilityNoVis = r_novis->integer;
    s_splitVisibilityWorstLeaf = lookingForWorstLeaf;
    memcpy(s_splitVisibilityMask, tr.refdef.areamask, MAX_MAP_AREA_BYTES);
    // A portal's temporary markings cannot be reused by a normal split view.
    s_splitVisibilityValid = !tr.viewParms.isPortal && !lookingForWorstLeaf;
}
