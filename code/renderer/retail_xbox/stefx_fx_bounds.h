// Bounds of the active EF generators, in world coordinates. Unknown or
// state-mutating legacy generators deliberately remain on the original path.
#if defined(_XBOX) && defined(STEFX_SP_HOSTED_MP)
static bool STEFX_FxBounds(const refEntity_t &e, float bounds[2][3]) {
    float end[3], width = 0.0f;
    bool curve = false;
    VectorCopy(e.oldorigin, end);
    switch (e.reType) {
    case RT_LINE: width = fabsf(e.radius); break;
    case RT_TEXTURED_LINE: width = fabsf(e.radius) * 0.5f; break;
    case RT_ORIENTED_LINE: width = fabsf(e.stefxData.line.width) * 0.5f; break;
    case RT_TAPERED_LINE:
        width = 0.5f * (fabsf(e.radius) > fabsf(e.backlerp) ? fabsf(e.radius) : fabsf(e.backlerp)); break;
    case RT_BEZIER: width = fabsf(e.radius) * 0.5f; curve = true; break;
    case RT_EF_LIGHTNING: width = 8.0f; break;
    case RT_EF_ELECTRICITY:
        // Sixteen accumulated random steps, including the penultimate step.
        // Per-axis displacement is bounded by |deviation| * segment length.
        width = fabsf(e.stefxData.electricity.width) * 0.5f +
            fabsf(e.stefxData.electricity.deviation) * Distance(e.origin, e.oldorigin);
        break;
    case RT_EF_CYLINDER:
        if (fabsf(DotProduct(e.axis[0], e.axis[0]) - 1.0f) > 0.01f) return false;
        VectorMA(e.origin, e.stefxData.cylinder.height, e.axis[0], end);
        width = (fabsf(e.stefxData.cylinder.width) > fabsf(e.stefxData.cylinder.width2) ?
            fabsf(e.stefxData.cylinder.width) : fabsf(e.stefxData.cylinder.width2)); break;
    case RT_SPRITE:
    case RT_ORIENTED_QUAD:
    case RT_EF_ORIENTED_SPRITE:
    case RT_EF_ALPHA_VERT_POLY:
        VectorCopy(e.origin, end);
        if (e.reType == RT_SPRITE) width = 2.0f * fabsf(e.radius);
        else if (e.reType == RT_EF_ALPHA_VERT_POLY) width = 2.0f * fabsf(e.stefxData.sprite.radius);
        else width = 2.0f * (VectorLength(e.axis[1]) + VectorLength(e.axis[2])) *
            fabsf(e.reType == RT_ORIENTED_QUAD ? e.radius : e.stefxData.sprite.radius);
        break;
    default: return false;
    }
    // Invalid data must never turn a conservative rejection into disappearing geometry.
    if (!(width >= 0.0f && width < 1.0e8f)) return false;
    for (int a = 0; a < 3; ++a) {
        float lo = e.origin[a] < end[a] ? e.origin[a] : end[a];
        float hi = e.origin[a] > end[a] ? e.origin[a] : end[a];
        if (!(e.origin[a] > -1.0e8f && e.origin[a] < 1.0e8f &&
              end[a] > -1.0e8f && end[a] < 1.0e8f)) return false;
        if (curve) {
            for (int c = 0; c < 2; ++c) {
                if (!(e.axis[c][a] > -1.0e8f && e.axis[c][a] < 1.0e8f)) return false;
                if (e.axis[c][a] < lo) lo = e.axis[c][a];
                if (e.axis[c][a] > hi) hi = e.axis[c][a];
            }
            // The inherited loop emits t=33/32 too. Negative Bernstein
            // weights sum to <0.1 there; 0.125 of the hull span covers it.
            float extension = (hi - lo) * 0.125f;
            lo -= extension; hi += extension;
        }
        bounds[0][a] = lo - width - 1.0f;
        bounds[1][a] = hi + width + 1.0f;
    }
    return true;
}
static bool STEFX_FxOutsidePlanes(const float bounds[2][3], const cplane_t *planes) {
    for (int p = 0; p < 4; ++p) {
        float maximum = -planes[p].dist;
        for (int a = 0; a < 3; ++a)
            maximum += planes[p].normal[a] * bounds[planes[p].normal[a] >= 0.0f ? 1 : 0][a];
        if (maximum < 0.0f) return true;
    }
    return false;
}
static bool STEFX_FxCullEnabled(void) {
    static cvar_t *enabled;
    if (!enabled) enabled = Cvar_Get("r_efFxCull", "1", 0);
    return enabled->integer != 0;
}
extern "C" volatile unsigned int g_SPXBFxCull[3] = {0};
static bool STEFX_CullExtendedFx(const refEntity_t &e) {
    float bounds[2][3];
    if (e.renderfx & (RF_NODEPTH | RF_DEPTHHACK) || r_nocull->integer ||
        !STEFX_FxBounds(e, bounds)) return false;
    ++g_SPXBFxCull[0];
    if (!STEFX_FxOutsidePlanes(bounds, tr.viewParms.frustum)) return false;
    ++g_SPXBFxCull[1];
    g_SPXBFxCull[2] = (unsigned int)e.reType;
    return true;
}
#endif
