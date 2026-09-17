// Co-op model visibility experiment. Included after tr in the active renderer.
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBCoopModelPvs[8] = {0};
static bool s_coopModelOutsidePvs = false;
static cvar_t *s_coopModelPvsMode;

// STEFX_COOP_MODEL_PVS_GEOMETRY_BEGIN
static bool STEFX_CoopBoxTouchesVisibleLeaf(const float bounds[2][3],
    const mnode_t *root, const cplane_t *planes, int planeCount, int visibleFrame) {
    const mnode_t *stack[32];
    int pending = 0, visited = 0;
    stack[pending++] = root;
    while (pending) {
        const mnode_t *node = stack[--pending];
        if (!node || ++visited > 256) return true;
        if (node->visframe != visibleFrame) continue;
        if (node->contents != -1) return true;
        if (node->planeNum >= (unsigned int)planeCount) return true;
        const cplane_t &plane = planes[node->planeNum];
        float lo = -plane.dist, hi = -plane.dist;
        for (int axis = 0; axis < 3; ++axis) {
            lo += plane.normal[axis] * bounds[plane.normal[axis] >= 0 ? 0 : 1][axis];
            hi += plane.normal[axis] * bounds[plane.normal[axis] >= 0 ? 1 : 0][axis];
        }
        if (!(lo > -1.0e9f && hi < 1.0e9f && lo <= hi) || pending > 29) return true;
        if (hi >= -1.0f) stack[pending++] = node->children[0];
        if (lo <= 1.0f) stack[pending++] = node->children[1];
    }
    return false;
}
static bool STEFX_CoopWorldBounds(const float local[2][3], const refEntity_t &e,
    bool mdrFallback, float bounds[2][3]) {
    for (int a = 0; a < 3; ++a) {
        if (!(local[0][a] > -1.0e6f && local[1][a] < 1.0e6f && local[0][a] <= local[1][a])) return false;
        bounds[0][a] = e.origin[a] - 1.0f;
        bounds[1][a] = e.origin[a] + 1.0f;
        for (int b = 0; b < 3; ++b) {
            float n = e.axis[b][a];
            bounds[0][a] += n * (local[n >= 0 ? 0 : 1][b] + (n >= 0 ? -0.125f : 0.125f));
            bounds[1][a] += n * (local[n >= 0 ? 1 : 0][b] + (n >= 0 ? 0.125f : -0.125f));
        }
        // Preserve the existing MDR frustum-cull fallback's coverage too.
        if (mdrFallback) {
            float center = e.origin[a] + (a == 2 ? 32.0f : 0.0f);
            if (bounds[0][a] > center - 129.0f) bounds[0][a] = center - 129.0f;
            if (bounds[1][a] < center + 129.0f) bounds[1][a] = center + 129.0f;
        }
        if (!(bounds[0][a] > -1.0e7f && bounds[1][a] < 1.0e7f && bounds[0][a] <= bounds[1][a])) return false;
    }
    return true;
}
// STEFX_COOP_MODEL_PVS_GEOMETRY_END

static bool STEFX_CoopModelBounds(const model_t *model, const refEntity_t &e, float bounds[2][3]) {
    float local[2][3];
    int frames[2] = {e.frame, e.oldframe};
    bool any = false;
    if (!(e.backlerp >= 0.0f && e.backlerp <= 1.0f)) return false;
    for (int f = 0; f < 2; ++f) {
        if (frames[f] < 0) return false;
        for (int lod = 0; lod < (model->type == MOD_MESH ? MD3_MAX_LODS : 1); ++lod) {
            const float *b = NULL;
            if (model->type == MOD_MESH) {
                const md3Header_t *h = model->md3[lod];
                if (!h) continue;
                if (frames[f] >= h->numFrames) return false;
                b = ((const md3Frame_t *)((const byte *)h + h->ofsFrames))[frames[f]].bounds[0];
            } else if (model->type == MOD_MDR && model->md4) {
                const md4Header_t *h = model->md4;
                if (frames[f] >= h->numFrames) return false;
                if (h->ofsFrames < 0) b = (const float *)R_STEFX_GetMDRFrame(h, frames[f]);
                else {
                    int size = (int)(&((md4Frame_t *)0)->bones[h->numBones]);
                    b = (const float *)((const byte *)h + h->ofsFrames + frames[f] * size);
                }
            }
            if (!b) return false;
            // Copy before requesting another losslessly packed frame: its cache may rotate.
            for (int a = 0; a < 3; ++a) {
                if (!(b[a] > -1.0e6f && b[a+3] < 1.0e6f && b[a] <= b[a+3])) return false;
                if (!any || b[a] < local[0][a]) local[0][a] = b[a];
                if (!any || b[a+3] > local[1][a]) local[1][a] = b[a+3];
            }
            any = true;
        }
    }
    return any && STEFX_CoopWorldBounds(local, e, model->type == MOD_MDR, bounds);
}
static bool STEFX_CoopModelOutsidePvs(const trRefEntity_t *entity, const model_t *model) {
    if (!s_coopModelPvsMode) s_coopModelPvsMode = Cvar_Get("r_efCoopModelPvs", "0", 0);
    g_SPXBCoopModelPvs[0] = s_coopModelPvsMode->integer;
    if (!s_coopModelPvsMode->integer || !tr.viewParms.stefxSplitView || tr.viewParms.isPortal ||
        !model || (model->type != MOD_MESH && model->type != MOD_MDR) ||
        !tr.world || !tr.world->nodes || !tr.world->planes || tr.world->numplanes <= 0 || tr.viewCluster < 0 ||
        tr.world->nodes[0].visframe != tr.visCount || !r_drawworld->integer ||
        r_nocull->integer || r_novis->integer || r_lockpvs->integer || r_shadows->integer > 1 ||
        (tr.refdef.rdflags & RDF_NOWORLDMODEL)) return false;
    const refEntity_t &e = entity->e;
    if (e.renderfx & ~(RF_THIRD_PERSON | RF_WRAP_FRAMES | RF_CAP_FRAMES | RF_NOSHADOW | RF_LIGHTING_ORIGIN)) return false;
    float bounds[2][3];
    ++g_SPXBCoopModelPvs[1];
    if (!STEFX_CoopModelBounds(model, e, bounds)) { ++g_SPXBCoopModelPvs[4]; return false; }
    if (STEFX_CoopBoxTouchesVisibleLeaf(bounds, tr.world->nodes, tr.world->planes, tr.world->numplanes, tr.visCount)) return false;
    ++g_SPXBCoopModelPvs[2];
    ++g_SPXBCoopModelPvs[model->type == MOD_MDR ? 6 : 5];
    return true;
}
static bool STEFX_CoopSkipHiddenSurface(const surfaceType_t *surface, const shader_t *shader) {
    if (!s_coopModelOutsidePvs || (*surface != SF_MD3 && *surface != SF_MDR) ||
        !shader || shader == tr.shadowShader || shader == tr.projectionShadowShader ||
        shader->remappedShader || shader->numDeforms || shader->sky || shader->sort == SS_PORTAL) return false;
    for (int stage = 0; stage < shader->numUnfoggedPasses; ++stage)
        if ((shader->stages[stage].stateBits & GLS_DEPTHTEST_DISABLE) || shader->stages[stage].ss) return false;
    ++g_SPXBCoopModelPvs[3];
    if (g_SPXBCoopModelPvs[3] == 1) XBLog_WriteCriticalf("STEFX_COOP_MODEL_PVS: mode=%d shader=%s cluster=%d", s_coopModelPvsMode->integer, shader->name, tr.viewCluster);
    if (s_coopModelPvsMode->integer != 1) return false; // mode 2 only observes
    ++g_SPXBCoopModelPvs[7];
    return true;
}
#endif
