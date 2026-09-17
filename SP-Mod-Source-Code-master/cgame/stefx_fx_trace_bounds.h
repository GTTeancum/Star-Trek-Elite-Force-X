#ifndef STEFX_FX_TRACE_BOUNDS_H
#define STEFX_FX_TRACE_BOUNDS_H

// Conservative broad phase only. The original collision code remains the
// authority for every possible contact, including stationary/startsolid tests.
static bool STEFX_FxSweptBounds(const float *start, const float *end,
    const float *mins, const float *maxs, float *lo, float *hi)
{
    for (int i=0; i<3; ++i) {
        const float mn=mins ? mins[i] : 0.0f, mx=maxs ? maxs[i] : 0.0f;
        // Unknown/non-finite input cannot justify rejecting a collision.
        if (!(start[i]>-1e6f && start[i]<1e6f && end[i]>-1e6f && end[i]<1e6f &&
              mn>-1e6f && mx<1e6f && mn<=mx)) return false;
        lo[i]=(start[i]<end[i] ? start[i] : end[i])+mn-1.0f;
        hi[i]=(start[i]>end[i] ? start[i] : end[i])+mx+1.0f;
    }
    return true;
}
static bool STEFX_FxBoundsDisjoint(const float *lo, const float *hi,
    const float *origin, const float *mins, const float *maxs)
{
    for (int i=0; i<3; ++i)
        if (!(origin[i]>-1e6f && origin[i]<1e6f && mins[i]>-1e6f &&
              maxs[i]<1e6f && mins[i]<=maxs[i])) return false;
    for (int i=0; i<3; ++i)
        if (hi[i]<origin[i]+mins[i] || lo[i]>origin[i]+maxs[i]) return true;
    return false;
}
static bool STEFX_FxWorldBounds(const float *origin, const float *mins,
    const float *maxs, float *lo, float *hi)
{
    // A deliberately bounded numeric range keeps rounding well below the
    // one-unit sweep margin. Larger or invalid inputs use the original trace.
    for (int i=0; i<3; ++i) {
        if (!(origin[i]>-1e6f && origin[i]<1e6f && mins[i]>-1e6f &&
              maxs[i]<1e6f && mins[i]<=maxs[i])) return false;
        lo[i]=origin[i]+mins[i]; hi[i]=origin[i]+maxs[i];
    }
    return true;
}
static bool STEFX_FxWorldBoundsDisjoint(const float *lo, const float *hi,
    const float *worldLo, const float *worldHi)
{
    for (int i=0; i<3; ++i)
        if (hi[i]<worldLo[i] || lo[i]>worldHi[i]) return true;
    return false;
}
#endif
