#include "../../qcommon/stefx_split_perf_defaults.h"
// Cache the visibility markings actually installed in the shared BSP, not the
// refdef restored after a split view. Frustum traversal remains per viewport.
extern "C" volatile unsigned int g_SPXBSplitVisibility[8] = {0};
// mode, requests, equivalent keys, skips, verified, mismatches, resets, overflow
struct splitVisibilityKey_t {
    const world_t *world;
    int cluster, noVis, ignorePvs, ignoreArea;
    byte areaMask[MAX_MAP_AREA_BYTES];

    bool matches(int viewCluster, int skipPvs, int skipArea) const {
        return world == tr.world && cluster == viewCluster && noVis == r_novis->integer &&
            ignorePvs == skipPvs && ignoreArea == skipArea &&
            !memcmp(areaMask, tr.refdef.areamask, sizeof(areaMask));
    }
    void set(int viewCluster, int skipPvs, int skipArea) {
        world = tr.world;
        cluster = viewCluster;
        noVis = r_novis->integer;
        ignorePvs = skipPvs;
        ignoreArea = skipArea;
        memcpy(areaMask, tr.refdef.areamask, sizeof(areaMask));
    }
};

// A compact list of runs in BSP array order. Physical node and leaf stamps
// remain the visibility authority for traversal, model culling and portals.
struct visibilityRun_t { unsigned short first, last; };
struct splitVisibilitySlot_t {
    splitVisibilityKey_t key;
    visibilityRun_t runs[768];
    int nodeRuns, totalRuns;
    bool attempted;
};
#if defined(STEFX_SP_HOSTED_MP)
static splitVisibilitySlot_t s_splitSlots[4];
#else
static splitVisibilitySlot_t s_splitSlots[2];
#endif
static splitVisibilityKey_t s_installedVisibility;
static int s_splitVisibilityStamp;
static bool s_splitVisibilityFailed;
static splitVisibilitySlot_t *s_splitCapture;
static bool s_loggedSplitReplay;

// Capture only a key observed on successive visits to this viewport. A camera
// crossing a cluster boundary every frame continues using the ordinary walk.
template<class Node>
static int STEFX_CaptureVisibilityRuns(const Node *nodes, int count, visibilityRun_t *runs, int capacity) {
    if ((unsigned int)count > 65536u) return -1;
    int used = 0;
    for (int i = 0; i < count; ) {
        if (nodes[i].visframe != tr.visCount) { ++i; continue; }
        if (used == capacity) return -1;
        runs[used].first = (unsigned short)i;
        do { ++i; } while (i < count && nodes[i].visframe == tr.visCount);
        runs[used++].last = (unsigned short)(i - 1);
    }
    return used;
}

template<class Node>
static void STEFX_InstallVisibilityRuns(Node *nodes, const visibilityRun_t *runs, int count) {
    for (int i = 0; i < count; ++i) {
        for (int j = runs[i].first; j <= runs[i].last; ++j)
            nodes[j].visframe = tr.visCount;
    }
}

static void STEFX_CaptureSplitVisibility(splitVisibilitySlot_t &slot) {
    const int capacity = sizeof(slot.runs) / sizeof(slot.runs[0]);
    slot.attempted = true;
    slot.nodeRuns = STEFX_CaptureVisibilityRuns(tr.world->nodes, tr.world->numnodes, slot.runs, capacity);
    int leafRuns = -1;
    if (slot.nodeRuns >= 0)
        leafRuns = STEFX_CaptureVisibilityRuns(tr.world->leafs, tr.world->numleafs,
            slot.runs + slot.nodeRuns, capacity - slot.nodeRuns);
    slot.totalRuns = leafRuns < 0 ? -1 : slot.nodeRuns + leafRuns;
    if (slot.totalRuns < 0) ++g_SPXBSplitVisibility[7];
}

// BSP leaf order usually puts nearby cluster numbers in the same block.
// Rows with no visible bits in a block's byte interval cannot select a leaf.
enum { SPLIT_LEAF_BLOCK_SIZE = 32, MAX_SPLIT_LEAF_BLOCKS = 512 };
struct splitLeafBlock_t { unsigned short firstByte, lastByte; };
static splitLeafBlock_t s_splitLeafBlocks[MAX_SPLIT_LEAF_BLOCKS];
static const world_t *s_splitLeafBlockWorld;

static bool STEFX_UseSplitLeafBlocks(int ignorePvs) {
#if defined(STEFX_PVS_DIAGNOSTICS)
    return false; // Preserve the probe's individual leaf rejection counts.
#else
    if (g_SPXBSplitVisibility[0] != 1 || ignorePvs) return false;
    const world_t *world = tr.world;
    if (world->numleafs <= 0 || world->numleafs > MAX_SPLIT_LEAF_BLOCKS * SPLIT_LEAF_BLOCK_SIZE ||
        world->numClusters <= 0 || world->numClusters > 32768 ||
        world->clusterBytes < (world->numClusters + 7) / 8) return false;
    if (s_splitLeafBlockWorld == world) return true;

    int blockCount = 0;
    for (int first = 0; first < world->numleafs; first += SPLIT_LEAF_BLOCK_SIZE) {
        splitLeafBlock_t &block = s_splitLeafBlocks[blockCount++];
        block.firstByte = 0xffff;
        block.lastByte = 0;
        int end = first + SPLIT_LEAF_BLOCK_SIZE;
        if (end > world->numleafs) end = world->numleafs;
        for (int i = first; i < end; ++i) {
            const int cluster = world->leafs[i].cluster;
            if (cluster < 0 || cluster >= world->numClusters) continue;
            const unsigned short index = (unsigned short)(cluster >> 3);
            if (index < block.firstByte) block.firstByte = index;
            if (index > block.lastByte) block.lastByte = index;
        }
    }
    s_splitLeafBlockWorld = world;
    XBLog_WriteCriticalf("STEFX_SPLIT_VIS: indexed %d leaf blocks", blockCount);
    return true;
#endif
}

static bool STEFX_SplitLeafBlockHidden(int firstLeaf, const byte *vis) {
    const splitLeafBlock_t &block = s_splitLeafBlocks[firstLeaf / SPLIT_LEAF_BLOCK_SIZE];
    // For widely scattered clusters, individual leaf tests are cheaper.
    if ((int)block.lastByte - (int)block.firstByte >= SPLIT_LEAF_BLOCK_SIZE) return false;
    for (int i = block.firstByte; i <= block.lastByte; ++i)
        if (vis[i]) return false;
    return true;
}

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
    s_installedVisibility.world = NULL;
    s_splitLeafBlockWorld = NULL;
    s_splitCapture = NULL;
    s_loggedSplitReplay = false;
    for (int i = 0; i < (int)(sizeof(s_splitSlots) / sizeof(s_splitSlots[0])); ++i)
        s_splitSlots[i].key.world = NULL;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
    s_splitVisibilityVerify = false;
#endif
    ++g_SPXBSplitVisibility[6];
}

static bool STEFX_InstalledVisibilityMatches(int cluster, int ignorePvs, int ignoreArea) {
    return s_splitVisibilityStamp == tr.visCount && tr.viewCluster == cluster &&
        s_installedVisibility.matches(cluster, ignorePvs, ignoreArea);
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
    s_splitCapture = NULL;
    if (!mode) return false;
    ++g_SPXBSplitVisibility[1];
    if (!STEFX_InstalledVisibilityMatches(cluster, ignorePvs, ignoreArea)) {
        const unsigned int index = tr.viewParms.stefxSplitSlot;
        if (index >= sizeof(s_splitSlots) / sizeof(s_splitSlots[0])) return false;
        splitVisibilitySlot_t &slot = s_splitSlots[index];
        if (!slot.key.matches(cluster, ignorePvs, ignoreArea)) {
            slot.key.set(cluster, ignorePvs, ignoreArea);
            slot.attempted = false;
            slot.totalRuns = -1;
            return false;
        }
        if (!slot.attempted) {
            s_splitCapture = &slot;
            return false;
        }
        if (slot.totalRuns < 0) return false;

        ++tr.visCount; // Unstamped nodes from the previous player become invisible.
        tr.viewCluster = cluster;
        STEFX_InstallVisibilityRuns(tr.world->nodes, slot.runs, slot.nodeRuns);
        STEFX_InstallVisibilityRuns(tr.world->leafs, slot.runs + slot.nodeRuns,
            slot.totalRuns - slot.nodeRuns);
        s_installedVisibility = slot.key;
        s_splitVisibilityStamp = tr.visCount;
        if (!s_loggedSplitReplay) {
            s_loggedSplitReplay = true;
            XBLog_WriteCriticalf("STEFX_SPLIT_VIS: restored slot=%u cluster=%d runs=%d", index, cluster, slot.totalRuns);
        }
    }
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
    if (s_splitCapture) {
        STEFX_CaptureSplitVisibility(*s_splitCapture);
        s_splitCapture = NULL;
    }
    s_splitVisibilityStamp = tr.visCount;
    if (tr.viewParms.isPortal || lookingForWorstLeaf)
        s_installedVisibility.world = NULL;
    else
        s_installedVisibility.set(cluster, ignorePvs, ignoreArea);
}
