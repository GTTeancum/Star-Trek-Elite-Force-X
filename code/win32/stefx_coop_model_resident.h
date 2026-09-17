// Co-op-only immutable model-local GPU attributes. Included after world helpers.
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
#define STEFX_COOP_MODEL_CACHE_BYTES (256u * 1024u)
#define STEFX_COOP_MODEL_CACHE_KEYS 256u
struct stefxCoopModelKey_t {
    const md3Surface_t *surface;
    const shader_t *shader;
    const shaderStage_t *stage;
    unsigned int format, color, first, count;
};
static stefxCoopModelKey_t s_coopModelKeys[STEFX_COOP_MODEL_CACHE_KEYS];
static stefxWorldVertex_t *s_coopModelVertices;
static unsigned int s_coopModelUsed;
static bool s_coopModelAllocAttempted;
static const md3Surface_t *s_coopModelSurface;
static bool s_coopModelMultiple;
static cvar_t *s_coopModelMode, *s_coopActualMode, *s_coopActualSplit;
extern "C" volatile unsigned int g_SPXBCoopModelResident[12] = {0};
extern "C" volatile unsigned int g_SPXBCoopModelRejects[8] = {0};
// First rejected policy: provenance, animation, entity, flags, shader, stage,
// color generation, UV generation. Pass/index checks precede these counters.
// mode, eligible, hits, fills, vertices stored, capacity fallbacks, policy rejects,
// verified vertices, mismatches, allocation bytes, allocation failures, saved vertices.
void STEFX_CoopModelBeginBatch(void) {
    s_coopModelSurface=NULL; s_coopModelMultiple=false;
}
void STEFX_CoopModelSurface(const md3Surface_t *surface) {
    if (s_coopModelSurface || tess.numVertexes || tess.numIndexes) s_coopModelMultiple=true;
    else s_coopModelSurface=surface;
}
static void STEFX_CoopModelReset(void) {
    // A cached model can still be referenced by submitted GPU work, even if
    // the world cache is empty. Fence its own lifetime before freeing it.
    if (s_coopModelVertices) {
        glw_state->device->BlockUntilIdle();
        XPhysicalFree(s_coopModelVertices);
    }
    s_coopModelVertices=NULL; s_coopModelUsed=0; s_coopModelAllocAttempted=false;
    memset(s_coopModelKeys,0,sizeof(s_coopModelKeys));
    memset((void*)g_SPXBCoopModelResident,0,sizeof(g_SPXBCoopModelResident));
    memset((void*)g_SPXBCoopModelRejects,0,sizeof(g_SPXBCoopModelRejects));
    STEFX_CoopModelBeginBatch();
}
static void STEFX_CoopModelConfigure(void) {
    // Called at frame begin, before submissions. A mode switch must not leave
    // this optional allocation consuming ordinary SP/MP memory indefinitely.
    if (s_coopModelAllocAttempted &&
        ((s_coopModelMode->integer!=1 && s_coopModelMode->integer!=2) ||
         !s_coopActualSplit->integer || Q_stricmp(s_coopActualMode->string,"coop")))
        STEFX_CoopModelReset();
}
static bool STEFX_CoopModelReject(unsigned int reason) {
    ++g_SPXBCoopModelRejects[reason]; return false;
}
static bool STEFX_CoopModelStatic(int normals, int tex0, int tex1) {
    const md3Surface_t *surface=s_coopModelSurface;
    if (!surface || s_coopModelMultiple || surface->numVerts!=tess.numVertexes ||
        surface->numTriangles!=tess.numIndexes/3) return STEFX_CoopModelReject(0);
    if (surface->numFrames!=1) return STEFX_CoopModelReject(1);
    if (!backEnd.currentEntity || backEnd.currentEntity==&tr.worldEntity ||
        backEnd.currentEntity->e.reType!=RT_MODEL) return STEFX_CoopModelReject(2);
    if (backEnd.currentEntity->e.frame || backEnd.currentEntity->e.oldframe)
        return STEFX_CoopModelReject(1);
    // These flags select visibility, device lighting/depth state or texture
    // binding; none changes the accepted model-local attributes. Wrapped/capped
    // frames have already been normalized by R_AddMD3Surfaces and checked above.
    const unsigned int stateFlags=RF_MORELIGHT | RF_THIRD_PERSON | RF_FIRST_PERSON |
        RF_DEPTHHACK | RF_NODEPTH | RF_NOSHADOW | RF_LIGHTING_ORIGIN | RF_SHADOW_PLANE |
        RF_WRAP_FRAMES | RF_CAP_FRAMES | RF_SETANIMINDEX | RF_STEFX_SPLIT_SLOT0 |
        RF_STEFX_SPLIT_SLOT1 | RF_STEFX_SPLIT_SLOT2 | RF_STEFX_SPLIT_HIDE_SLOT0 |
        RF_STEFX_SPLIT_HIDE_SLOT1;
    if ((unsigned int)backEnd.currentEntity->e.renderfx & ~stateFlags)
        return STEFX_CoopModelReject(3);
    if (!tess.shader || !tess.xstages || tess.shader->numDeforms || tess.shader->sky || tess.fading ||
        tess.currentPass<0 || tess.currentPass>=tess.shader->numUnfoggedPasses)
        return STEFX_CoopModelReject(4);
    const shaderStage_t *stage=&tess.xstages[tess.currentPass];
    if (!stage->active || stage->isBumpMap || stage->isEnvironment || stage->ss ||
        (tess.fogNum && stage->adjustColorsForFog)) return STEFX_CoopModelReject(5);
    // Xbox diffuse color generation writes white vertices and applies object
    // lighting through device state. That state is still set for every draw.
    if (stage->rgbGen!=CGEN_IDENTITY && stage->rgbGen!=CGEN_IDENTITY_LIGHTING &&
        stage->rgbGen!=CGEN_CONST && stage->rgbGen!=CGEN_LIGHTING_DIFFUSE) return STEFX_CoopModelReject(6);
    if (stage->alphaGen!=AGEN_IDENTITY && stage->alphaGen!=AGEN_CONST &&
        stage->alphaGen!=AGEN_SKIP) return STEFX_CoopModelReject(6);
    for (int b=0;b<2;++b) {
        if (!(b ? tex1 : tex0)) continue;
        const textureBundle_t *bundle=&stage->bundle[b];
        if (bundle->numTexMods || (bundle->tcGen!=TCGEN_TEXTURE && bundle->tcGen!=TCGEN_IDENTITY))
            return STEFX_CoopModelReject(7);
    }
    return true;
}
static stefxWorldVertex_t *STEFX_CoopModelClaim(GLenum mode, int count,
    const GLushort *indices, int normals, int tex0, int tex1) {
    if (!s_coopModelMode) {
        s_coopModelMode=Cvar_Get("r_efCoopModelResident","0",0);
        s_coopActualMode=Cvar_Get("stefx_splitScreenMode","coop",0);
        s_coopActualSplit=Cvar_Get("stefx_splitScreen","0",0);
    }
    g_SPXBCoopModelResident[0]=s_coopModelMode->integer;
    if ((s_coopModelMode->integer!=1 && s_coopModelMode->integer!=2) ||
        !s_coopActualSplit->integer || Q_stricmp(s_coopActualMode->string,"coop") ||
        !backEnd.viewParms.stefxSplitView || backEnd.projection2D) return NULL;
    if (!g_stefxWorldBasePass || mode!=GL_TRIANGLES || !indices || indices!=tess.indexes ||
        count<=0 || count!=tess.numIndexes || count%3 || count>SHADER_MAX_INDEXES ||
        tess.numVertexes<=0 || tess.numVertexes>SHADER_MAX_VERTEXES ||
        !STEFX_CoopModelStatic(normals,tex0,tex1)) {
        ++g_SPXBCoopModelResident[6]; return NULL;
    }
    // Bounds remain checked independently of provenance, including verifier mode.
    for (int i=0;i<count;++i) if (indices[i]>=tess.numVertexes) {
        ++g_SPXBCoopModelResident[6]; return NULL;
    }
    ++g_SPXBCoopModelResident[1];
    if (!s_coopModelAllocAttempted) {
        s_coopModelAllocAttempted=true;
        s_coopModelVertices=(stefxWorldVertex_t*)XPhysicalAlloc(STEFX_COOP_MODEL_CACHE_BYTES,
            MAXULONG_PTR,4096,PAGE_READWRITE|PAGE_WRITECOMBINE);
        if (s_coopModelVertices) g_SPXBCoopModelResident[9]=STEFX_COOP_MODEL_CACHE_BYTES;
        else ++g_SPXBCoopModelResident[10];
        XBLog_WriteCriticalf("STEFX_COOP_MODEL_RESIDENT: mode=%d bytes=%u keys=%u metadata=%u",
            s_coopModelMode->integer,g_SPXBCoopModelResident[9],STEFX_COOP_MODEL_CACHE_KEYS,
            (unsigned int)sizeof(s_coopModelKeys));
    }
    if (!s_coopModelVertices) return NULL;
    const shaderStage_t *stage=&tess.xstages[tess.currentPass];
    unsigned int format=(normals?1u:0u)|(tex0?2u:0u)|(tex1?4u:0u);
    // Accepted generators produce a uniform RGBA value; preserve changes to
    // identityLight, stage constants, or disabled color arrays in the key.
    unsigned int color=glw_state->colorArrayState ? tess.svars.colors[0] : glw_state->currentColor;
    unsigned int hash=((unsigned int)s_coopModelSurface>>4)^((unsigned int)stage>>4)^format^color;
    stefxCoopModelKey_t *key=NULL;
    for (unsigned int probe=0;probe<16;++probe) {
        stefxCoopModelKey_t *entry=&s_coopModelKeys[(hash+probe)&(STEFX_COOP_MODEL_CACHE_KEYS-1)];
        if (!entry->surface || (entry->surface==s_coopModelSurface && entry->shader==tess.shader &&
            entry->stage==stage && entry->format==format && entry->color==color &&
            entry->count==(unsigned int)tess.numVertexes)) {key=entry;break;}
    }
    if (!key) {++g_SPXBCoopModelResident[5];return NULL;}
    if (!key->surface) {
        unsigned int capacity=STEFX_COOP_MODEL_CACHE_BYTES/sizeof(stefxWorldVertex_t);
        if ((unsigned int)tess.numVertexes>capacity-s_coopModelUsed) {
            ++g_SPXBCoopModelResident[5];return NULL;
        }
        key->first=s_coopModelUsed; key->count=tess.numVertexes;
        for (int v=0;v<tess.numVertexes;++v)
            STEFX_WorldPackVertex(s_coopModelVertices[s_coopModelUsed+v],v,normals,tex0,tex1);
        s_coopModelUsed+=tess.numVertexes;
        key->surface=s_coopModelSurface;key->shader=tess.shader;key->stage=stage;
        key->format=format;key->color=color;
        ++g_SPXBCoopModelResident[3];g_SPXBCoopModelResident[4]=s_coopModelUsed;
    } else {
        ++g_SPXBCoopModelResident[2];g_SPXBCoopModelResident[11]+=tess.numVertexes;
    }
    stefxWorldVertex_t *data=s_coopModelVertices+key->first;
    if (s_coopModelMode->integer==2) {
        for (int v=0;v<tess.numVertexes;++v) {
            stefxWorldVertex_t reference;
            STEFX_WorldPackVertex(reference,v,normals,tex0,tex1);
            ++g_SPXBCoopModelResident[7];
            if (memcmp(&reference,data+v,sizeof(reference))) {
                ++g_SPXBCoopModelResident[8];
                XBLog_WriteCritical("STEFX_COOP_MODEL_RESIDENT: mismatch; disable and draw original data");
                s_coopModelMode->integer=0;return NULL;
            }
        }
    }
    return data;
}
#endif
