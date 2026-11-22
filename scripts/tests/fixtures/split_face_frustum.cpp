#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cplane_t { float normal[3], dist; unsigned char signbits; };
struct shaderStage_t { bool ss; };
struct shader_t {
    int sort, numDeforms, sky, remappedShader, numUnfoggedPasses;
    shaderStage_t stages[2];
};
struct srfSurfaceFace_t { int type, numPoints, flags; unsigned short *srfPoints; };
struct msurface_t { int *data; shader_t *shader; };
struct world_t { msurface_t *surfaces; };
static struct { world_t *world; struct { bool isPortal; cplane_t frustum[4]; } viewParms; } tr;
struct cvar_t { int integer; };
static cvar_t noCull, showNormals, showTris;
static cvar_t *r_nocull = &noCull, *r_shownormals = &showNormals, *r_showtris = &showTris;
enum { SF_FACE = 1, SS_OPAQUE = 3 };
#define XBLog_WriteCriticalf(...) ((void)0)
#include "split_face.inc"

static unsigned seed = 31073;
static int sample(int limit) {
    seed = 1664525u * seed + 1013904223u;
    return (seed >> 16) % limit;
}

static bool allVerticesBehind(const srfSurfaceFace_t &face, const cplane_t &plane) {
    const int stride = NEXT_SURFPOINT(face.flags);
    const short *point = (const short *)face.srfPoints;
    for (int i = 0; i < face.numPoints; ++i, point += stride) {
        double distance = 0;
        for (int axis = 0; axis < 3; ++axis)
            distance += double(point[axis]) * plane.normal[axis];
        if (distance >= plane.dist) return false;
    }
    return true;
}

int main() {
    // Both surfaces occupy the same direct-mapped cache slot.
    static msurface_t surfaces[SPLIT_FACE_CACHE_SIZE + 1];
    world_t world = { surfaces };
    tr.world = &world;
    shader_t shader = { SS_OPAQUE, 0, 0, 0, 2 };
    static short packed[2][5 * 24];
    srfSurfaceFace_t faces[2];
    for (int f = 0; f < 2; ++f) {
        srfSurfaceFace_t &face = faces[f];
        face.type = SF_FACE;
        face.numPoints = 5;
        face.flags = f ? 0x84 : 1;
        face.srfPoints = (unsigned short *)packed[f];
        int stride = NEXT_SURFPOINT(face.flags);
        assert(stride * face.numPoints <= sizeof(packed[f]) / sizeof(short));
        for (int v = 0; v < face.numPoints; ++v)
            for (int axis = 0; axis < 3; ++axis)
                packed[f][stride * v + axis] = short((f ? 28000 : -28000) + sample(6000) - 3000);
        surfaces[f * SPLIT_FACE_CACHE_SIZE].data = &face.type;
        surfaces[f * SPLIT_FACE_CACHE_SIZE].shader = &shader;
    }

    unsigned rejected = 0;
    for (int i = 0; i < 32768; ++i) {
        int f = (i / 3) & 1;  // Alternate cache misses and hits.
        int mask = i & 15;
        bool separatingPlane = false;
        for (int p = 0; p < 4; ++p) {
            cplane_t &plane = tr.viewParms.frustum[p];
            plane.signbits = 0;
            plane.dist = 0;
            for (int axis = 0; axis < 3; ++axis) {
                plane.normal[axis] = (sample(201) - 100) * .01f;
                if (plane.normal[axis] < 0) plane.signbits |= 1 << axis;
                plane.dist += plane.normal[axis] * packed[f][axis];
            }
            plane.dist += (i & 16) ? (sample(20001) - 10000) : (sample(9) - 4) * .25f;
            if ((mask & (1 << p)) && allVerticesBehind(faces[f], plane)) separatingPlane = true;
        }
        if (R_SplitFaceOutsideFrustum(&surfaces[f * SPLIT_FACE_CACHE_SIZE], mask)) {
            assert(separatingPlane);
            ++rejected;
        }
    }
    assert(rejected > 1000 && g_SPXBSplitFaceCull[1] < g_SPXBSplitFaceCull[0]);

    cplane_t &plane = tr.viewParms.frustum[0];
    memset(&plane, 0, sizeof(plane));
    plane.normal[0] = 1;
    plane.dist = 32767;
    assert(R_SplitFaceOutsideFrustum(surfaces, 1));
    shader.stages[1].ss = true;
    assert(!R_SplitFaceOutsideFrustum(surfaces, 1));
    shader.stages[1].ss = false;
    shader.numDeforms = 1;
    assert(!R_SplitFaceOutsideFrustum(surfaces, 1));
    shader.numDeforms = 0;
    tr.viewParms.isPortal = true;
    assert(!R_SplitFaceOutsideFrustum(surfaces, 1));
    tr.viewParms.isPortal = false;
    noCull.integer = 1;
    assert(!R_SplitFaceOutsideFrustum(surfaces, 1));
    noCull.integer = 0;

    // A reloaded map can reuse both the surface index and its memory address.
    R_STEFX_ResetSplitFaceBounds();
    packed[0][0] = 32767;
    assert(!R_SplitFaceOutsideFrustum(surfaces, 1));
    assert(g_SPXBSplitFaceCull[1] == 1);
    R_STEFX_ResetSplitFaceBounds();
    printf("32768 plane-mask comparisons passed, %u conservative rejections.\n", rejected);
    return 0;
}
