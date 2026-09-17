#ifndef STEFX_SHADOW_CACHE_H
#define STEFX_SHADOW_CACHE_H

// Exact world-mark results, never an approximation of moving shadow geometry.
// Capacity excess uses the original calculation without reducing its limits.
typedef int (*stefxMarkFunction_t)(int, const vec3_t *, const vec3_t,
    int, vec3_t, int, markFragment_t *);
struct stefxShadowEntry_t {
    bool valid;
    unsigned int age;
    vec3_t polygon[4], projection;
    int maxPoints, maxFragments, numPoints, numFragments;
    vec3_t points[64];
    markFragment_t fragments[16];
};
struct stefxShadowCache_t {
    stefxShadowEntry_t entries[32];
    unsigned int serial;
    bool failed;
    void Reset() { memset(entries,0,sizeof(entries)); serial=0; }
    int Call(int mode, const vec3_t *polygon, const vec3_t projection,
        int maxPoints, vec3_t points, int maxFragments, markFragment_t *fragments,
        stefxMarkFunction_t original, volatile unsigned int *stats)
    {
        if (failed || (mode!=1 && mode!=2))
            return original(4,polygon,projection,maxPoints,points,maxFragments,fragments);
        ++stats[1];
        if (++serial==0) { Reset(); ++serial; ++stats[11]; }
        stefxShadowEntry_t *hit=0, *victim=&entries[0];
        for (int i=0;i<32;++i) {
            stefxShadowEntry_t *e=&entries[i];
            if (!e->valid || (victim->valid && e->age<victim->age)) victim=e;
            if (e->valid && e->maxPoints==maxPoints && e->maxFragments==maxFragments &&
                !memcmp(e->polygon,polygon,sizeof(e->polygon)) &&
                !memcmp(e->projection,projection,sizeof(e->projection))) { hit=e; break; }
        }
        if (hit) {
            hit->age=serial; ++stats[2];
            if (mode==1) {
                memcpy(points,hit->points,hit->numPoints*sizeof(vec3_t));
                memcpy(fragments,hit->fragments,hit->numFragments*sizeof(markFragment_t));
                stats[8]+=hit->numFragments; stats[9]+=hit->numPoints;
                return hit->numFragments;
            }
            const int n=original(4,polygon,projection,maxPoints,points,maxFragments,fragments);
            ++stats[6];
            bool same=n==hit->numFragments;
            for (int j=0;same && j<n;++j) {
                const markFragment_t *f=&hit->fragments[j];
                same=fragments[j].firstPoint==f->firstPoint && fragments[j].numPoints==f->numPoints;
                if (same && f->numPoints) same=!memcmp(points+3*f->firstPoint,hit->points[f->firstPoint],f->numPoints*sizeof(vec3_t));
            }
            if (!same) { failed=true; ++stats[7]; stats[0]=0; }
            return n; // The verifier always retains the original result.
        }
        ++stats[3];
        const int n=original(4,polygon,projection,maxPoints,points,maxFragments,fragments);
        int used=0;
        if (n<0 || n>16 || n>maxFragments) { ++stats[5]; return n; }
        for (int j=0;j<n;++j) {
            const int first=fragments[j].firstPoint, count=fragments[j].numPoints;
            if (first<0 || count<0 || first>64 || count>64-first || first>maxPoints || count>maxPoints-first) {
                ++stats[5]; return n;
            }
            if (first+count>used) used=first+count;
        }
        victim->valid=true; victim->age=serial;
        memcpy(victim->polygon,polygon,sizeof(victim->polygon));
        memcpy(victim->projection,projection,sizeof(victim->projection));
        victim->maxPoints=maxPoints; victim->maxFragments=maxFragments;
        victim->numPoints=used; victim->numFragments=n;
        memcpy(victim->points,points,used*sizeof(vec3_t));
        memcpy(victim->fragments,fragments,n*sizeof(markFragment_t));
        ++stats[4];
        return n;
    }
};
#endif
