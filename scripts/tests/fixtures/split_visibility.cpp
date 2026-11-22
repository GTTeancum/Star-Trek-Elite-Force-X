// Host fixture for the Xbox R_MarkLeaves path. Compile via verify_split_visibility.py.
#ifdef NDEBUG
#error The split visibility oracle requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

typedef unsigned char byte;
enum { MAX_MAP_AREA_BYTES = 32, CONTENTS_SOLID = 1 };
struct mnode_s {
    int contents, visframe;
    mnode_s *parent;
    mnode_s() : contents(0), visframe(0), parent(NULL) {}
};
typedef mnode_s mnode_t;
struct mleaf_s : mnode_t { int cluster, area; };
struct world_t {
    mnode_s *nodes;
    mleaf_s *leafs;
    int numnodes, numleafs, numClusters, clusterBytes;
};
struct {
    world_t *world;
    int visCount, viewCluster;
    struct { byte areamask[MAX_MAP_AREA_BYTES]; bool areamaskModified; } refdef;
    struct { int stefxSplitSlot; bool stefxSplitView, isPortal; float pvsOrigin[3]; } viewParms;
} tr;
struct cvar_t { int integer; };
static cvar_t mode = {1}, lockVar, noVis;
static cvar_t *r_novis = &noVis, *r_lockpvs = &lockVar;
static bool lookingForWorstLeaf;
static int ignorePvs, ignoreArea, walks, checks, leafTests;
static std::vector<byte> visibility;
static cvar_t *Cvar_Get(const char *, const char *, int) { return &mode; }
static int Cvar_VariableIntegerValue(const char *name) {
    return strstr(name, "areamask") ? ignoreArea : ignorePvs;
}
static void XBLog_WriteCriticalf(const char *, ...) {}
static mleaf_s *R_PointInLeaf(float *) { assert(false); return NULL; }
static const byte *R_ClusterPVS(int cluster) {
    ++walks;
    return &visibility[cluster * tr.world->clusterBytes];
}

#include "stefx_split_visibility.h"
#include "mark_leaves.inc"

static void checkView(int slot, int cluster) {
    world_t &w = *tr.world;
    tr.viewParms.stefxSplitSlot = slot;
    mleaf_s eye;
    eye.contents = 0;
    eye.cluster = cluster;
    R_MarkLeaves(&eye);
    assert(tr.viewCluster == cluster);

    // Independent visibility oracle. Follow every eligible leaf's ancestors
    // without relying on stamps or early-outs from the renderer.
    std::vector<byte> nodes(w.numnodes), leaves(w.numleafs);
    if (noVis.integer || cluster < 0) {
        for (int i = 0; i < w.numnodes; ++i) nodes[i] = w.nodes[i].contents != CONTENTS_SOLID;
    } else {
        const byte *row = &visibility[cluster * w.clusterBytes];
        for (int i = 0; i < w.numleafs; ++i) {
            const mleaf_s &leaf = w.leafs[i];
            if (leaf.cluster < 0 || leaf.cluster >= w.numClusters) continue;
            if (!ignorePvs && !(row[leaf.cluster / 8] & (1 << (leaf.cluster % 8)))) continue;
            if (!lookingForWorstLeaf && !ignoreArea && leaf.area >= 0 &&
                (tr.refdef.areamask[leaf.area / 8] & (1 << (leaf.area % 8)))) continue;
            leaves[i] = 1;
            for (mnode_s *p = leaf.parent; p; p = p->parent) nodes[p - w.nodes] = 1;
        }
    }
    for (int i = 0; i < w.numnodes; ++i) assert((w.nodes[i].visframe == tr.visCount) == !!nodes[i]);
    for (int i = 0; i < w.numleafs; ++i) assert((w.leafs[i].visframe == tr.visCount) == !!leaves[i]);
    ++checks;
}

static void cycle(const int *clusters, int players) {
    for (int i = 0; i < players; ++i) checkView(i, clusters[i]);
}

static void exercise(const int *clusters, int players, bool expectFit) {
    R_STEFX_ResetVisibilityReuse();
    tr.viewParms.stefxSplitView = true;
    mode.integer = 0;
    int oldStart = walks;
    for (int i = 0; i < 8; ++i) cycle(clusters, players);
    int originalWalks = walks - oldStart;
    assert(originalWalks == 8 * players);
    R_STEFX_ResetVisibilityReuse();
    mode.integer = 1;
    cycle(clusters, players);
    cycle(clusters, players);
    int before = walks;
    for (int i = 0; i < 8; ++i) cycle(clusters, players);
    int warmWalks = walks - before;
    if (expectFit) assert(warmWalks == 0);
    else assert(warmWalks == 8 * players && g_SPXBSplitVisibility[7] > 0);
    printf("  %d alternating warmed views: %d -> %d full PVS walks\n", 8 * players, originalWalks, warmWalks);

    // A closed door, reopened door, visibility controls, portal and ordinary
    // view can all interrupt the alternating views at the same camera cluster.
    memset(tr.refdef.areamask, 0x55, sizeof(tr.refdef.areamask));
    cycle(clusters, players);
    tr.refdef.areamaskModified = true;
    cycle(clusters, players);
    tr.refdef.areamaskModified = false;
    ignorePvs = 1; cycle(clusters, players);
    ignoreArea = 1; cycle(clusters, players);
    ignorePvs = ignoreArea = 0;
    tr.viewParms.isPortal = true; cycle(clusters, players);
    tr.viewParms.isPortal = false;
    lookingForWorstLeaf = true; cycle(clusters, players);
    lookingForWorstLeaf = false;
    tr.viewParms.stefxSplitView = false;
    checkView(0, clusters[0]);
    memset(tr.refdef.areamask, 0, sizeof(tr.refdef.areamask));
    checkView(0, clusters[0]);
    noVis.integer = 1; checkView(0, clusters[0]);
    noVis.integer = 0; checkView(0, clusters[0]);
    tr.viewParms.stefxSplitView = true;
    checkView(0, -1);
    checkView(6, clusters[0]);  // invalid slot still renders normally
    lockVar.integer = 1;
    int stamp = tr.visCount;
    R_MarkLeaves(NULL);
    assert(tr.visCount == stamp);
    lockVar.integer = 0;
    mode.integer = 0; cycle(clusters, players);
    mode.integer = 2;
    unsigned int verified = g_SPXBSplitVisibility[4];
    for (int i = 0; i < 4; ++i) cycle(clusters, players);
    if (expectFit) assert(g_SPXBSplitVisibility[4] > verified);
    assert(g_SPXBSplitVisibility[5] == 0);
    mode.integer = 1;
}

static void readInts(FILE *f, int *dst, int count) {
    assert(count >= 0 && fread(dst, sizeof(int), count, f) == (unsigned)count);
}

static void realMap(const char *path, int players) {
    FILE *f = NULL;
    fopen_s(&f, path, "rb");
    assert(f);
    world_t world = {};
    int header[4];
    readInts(f, header, 4);
    world.numnodes = header[0]; world.numleafs = header[1];
    world.numClusters = header[2]; world.clusterBytes = header[3];
    std::vector<mnode_s> nodes(world.numnodes);
    std::vector<mleaf_s> leaves(world.numleafs);
    world.nodes = &nodes[0]; world.leafs = &leaves[0];
    for (int i = 0; i < world.numnodes; ++i) {
        int p; readInts(f, &p, 1);
        assert(p >= -1 && p < world.numnodes);
        nodes[i].contents = -1; nodes[i].parent = p < 0 ? NULL : &nodes[p];
    }
    for (int i = 0; i < world.numleafs; ++i) {
        int data[3]; readInts(f, data, 3);
        assert(data[0] >= -1 && data[0] < world.numnodes);
        leaves[i].parent = data[0] < 0 ? NULL : &nodes[data[0]];
        leaves[i].cluster = data[1]; leaves[i].area = data[2];
    }
    visibility.resize(world.numClusters * world.clusterBytes);
    assert(fread(&visibility[0], 1, visibility.size(), f) == visibility.size());
    int clusters[4]; readInts(f, clusters, players);
    int samples;
    readInts(f, &samples, 1);
    assert(samples >= 0);
    std::vector<int> views(samples * players);
    if (samples) readInts(f, &views[0], samples * players);
    assert(fgetc(f) == EOF); fclose(f);
    tr.world = &world;
    printf("%s (%d leaves)\n", path, world.numleafs);
    // Measure one cold walk per camera with each policy. The first group
    // establishes the result without replaying any warmed visibility slot.
    int coldVisits[2];
    for (int enabled = 0; enabled <= 1; ++enabled) {
        R_STEFX_ResetVisibilityReuse();
        tr.viewParms.stefxSplitView = true;
        mode.integer = enabled;
        int before = leafTests;
        cycle(clusters, players);
        coldVisits[enabled] = leafTests - before;
    }
    assert(coldVisits[1] < coldVisits[0]);
    printf("  cold views: %d -> %d individual leaf tests\n", coldVisits[0], coldVisits[1]);
    exercise(clusters, players, true);
    if (samples) {
        for (int enabled = 0; enabled <= 1; ++enabled) {
            R_STEFX_ResetVisibilityReuse();
            tr.viewParms.stefxSplitView = true;
            mode.integer = enabled;
            const int startWalks = walks;
            const int startTests = leafTests;
            for (int i = 0; i < samples; ++i) cycle(&views[i * players], players);
            printf("  sampled views=%d reuse=%d PVS walks=%d individual leaf tests=%d\n",
                samples * players, enabled, walks - startWalks, leafTests - startTests);
        }
    }
}

int main(int argc, char **argv) {
    const int players = sizeof(s_splitSlots) / sizeof(s_splitSlots[0]);
    int clusters[] = {0, 1, 2, 3};
    for (int size = 32; size <= 4096; size *= 128) {
        // Reuse the same world address across a level reset, once for a compact
        // visibility set and once for more alternating runs than a slot can hold.
        world_t world = {};
        mnode_s root;
        root.contents = -1;
        std::vector<mleaf_s> leaves(size);
        for (int i = 0; i < size; ++i) {
            leaves[i].parent = &root; leaves[i].area = i % 3; leaves[i].cluster = i % 4;
        }
        world.nodes = &root; world.numnodes = 1; world.leafs = &leaves[0];
        world.numleafs = size; world.numClusters = 4; world.clusterBytes = 1;
        visibility.resize(4);
        for (int i = 0; i < 4; ++i) visibility[i] = 1 << i;
        tr.world = &world;
        exercise(clusters, players, size == 32);
    }
    for (int i = 1; i < argc; ++i) realMap(argv[i], players);
    printf("%d players: %d complete node/leaf comparisons passed\n", players, checks);
    return 0;
}
