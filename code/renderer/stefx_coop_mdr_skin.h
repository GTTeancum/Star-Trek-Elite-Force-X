// Included by tr_animation.cpp only. CPU output, never GPU-owned storage.
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBMainLoopCount;
// mode, requests, hits, fills, reused vertices, capacity fallbacks, allocation
// failures, verified vertices, mismatches, bytes, pose hits, pose fills,
// palette bytes copied, resets, failed latch, non-split view rejections.
extern "C" volatile unsigned int g_SPXBCoopMdrSkin[16] = {0};
enum { COOP_SKIN_VERTICES=8192, COOP_SKIN_KEYS=256, COOP_SKIN_POSES=128,
       COOP_SKIN_BONES=1365 };
struct coopSkinVertex_t { float xyz[3], normal[3], uv[2]; };
struct coopSkinPose_t {
    const md4Header_t *header;
    int frame, oldFrame;
    unsigned int lerpBits, first, count;
};
struct coopSkinKey_t {
    const md4Surface_t *surface;
    unsigned int pose, first, count;
};
struct coopSkinStorage_t {
    coopSkinVertex_t vertices[COOP_SKIN_VERTICES];
    md4Bone_t bones[COOP_SKIN_BONES];
    coopSkinPose_t poses[COOP_SKIN_POSES];
    coopSkinKey_t keys[COOP_SKIN_KEYS];
};
static coopSkinStorage_t *s_coopSkin;
static unsigned int s_coopSkinVerts, s_coopSkinBones, s_coopSkinPoses, s_coopSkinKeys;
static unsigned int s_coopSkinLoop=~0u;
static int s_coopSkinFrame=-1;
static bool s_coopSkinTried, s_coopSkinFailed;
static cvar_t *s_coopSkinMode, *s_coopSkinSplit, *s_coopSkinPlayers, *s_coopSkinGame;

static int R_STEFX_CoopSkinMode(void) {
    if (!s_coopSkinMode) {
        s_coopSkinMode=Cvar_Get("r_efCoopMdrSkin", "0", 0);
        s_coopSkinSplit=Cvar_Get("stefx_splitScreen", "0", 0);
        s_coopSkinPlayers=Cvar_Get("stefx_splitScreenPlayers", "1", 0);
        s_coopSkinGame=Cvar_Get("stefx_splitScreenMode", "coop", 0);
    }
    if (s_coopSkinFailed || !s_coopSkinSplit->integer || s_coopSkinPlayers->integer<2 ||
        Q_stricmp(s_coopSkinGame->string,"coop")) return 0;
    return (s_coopSkinMode->integer==1 || s_coopSkinMode->integer==2) ? s_coopSkinMode->integer : 0;
}

void R_STEFX_CoopSkinShutdown(void) {
    if (s_coopSkin) HeapFree(GetProcessHeap(),0,s_coopSkin);
    s_coopSkin=NULL; s_coopSkinTried=false;
    s_coopSkinVerts=s_coopSkinBones=s_coopSkinPoses=s_coopSkinKeys=0;
    s_coopSkinLoop=~0u; s_coopSkinFrame=-1;
    g_SPXBCoopMdrSkin[9]=0;
    ++g_SPXBCoopMdrSkin[13];
    // A verifier failure remains latched until process restart.
}

void R_STEFX_CoopSkinBeginFrame(void) {
    g_SPXBCoopMdrSkin[0]=R_STEFX_CoopSkinMode();
    if (!g_SPXBCoopMdrSkin[0] && (s_coopSkin || s_coopSkinTried))
        R_STEFX_CoopSkinShutdown();
}

static coopSkinKey_t *R_STEFX_ClaimCoopSkin(md4Surface_t *surface, md4Header_t *header,
    float backlerp, const md4Bone_t *palette, bool *hit) {
    *hit=false;
    const int mode=R_STEFX_CoopSkinMode();
    g_SPXBCoopMdrSkin[0]=mode;
    if (!mode) return NULL;
    if (!backEnd.viewParms.stefxSplitView) { ++g_SPXBCoopMdrSkin[15]; return NULL; }
    if (surface->numVerts<=0 || header->numBones<=0 || header->numBones>MD4_MAX_BONES) return NULL;
    ++g_SPXBCoopMdrSkin[1];
    if (!s_coopSkinTried) {
        s_coopSkinTried=true;
        s_coopSkin=(coopSkinStorage_t*)HeapAlloc(GetProcessHeap(),0,sizeof(coopSkinStorage_t));
        if (!s_coopSkin) ++g_SPXBCoopMdrSkin[6];
        g_SPXBCoopMdrSkin[9]=s_coopSkin ? sizeof(coopSkinStorage_t) : 0;
        XBLog_WriteCriticalf("STEFX_COOP_MDR_SKIN: allocated=%u bytes=%u mode=%d",
            s_coopSkin ? 1u:0u,g_SPXBCoopMdrSkin[9],mode);
    }
    if (!s_coopSkin) return NULL;
    if (s_coopSkinLoop!=g_SPXBMainLoopCount || s_coopSkinFrame!=tr.frameCount) {
        s_coopSkinLoop=g_SPXBMainLoopCount; s_coopSkinFrame=tr.frameCount;
        s_coopSkinVerts=s_coopSkinBones=s_coopSkinPoses=s_coopSkinKeys=0;
    }
    unsigned int lerpBits,pose;
    memcpy(&lerpBits,&backlerp,4);
    for (pose=0;pose<s_coopSkinPoses;++pose) {
        const coopSkinPose_t *p=&s_coopSkin->poses[pose];
        if (p->header==header && p->frame==backEnd.currentEntity->e.frame &&
            p->oldFrame==backEnd.currentEntity->e.oldframe && p->lerpBits==lerpBits &&
            p->count==(unsigned int)header->numBones &&
            !memcmp(&s_coopSkin->bones[p->first],palette,p->count*sizeof(md4Bone_t))) {
            ++g_SPXBCoopMdrSkin[10]; break;
        }
    }
    if (pose==s_coopSkinPoses) {
        if (pose==COOP_SKIN_POSES || (unsigned int)header->numBones>COOP_SKIN_BONES-s_coopSkinBones) {
            ++g_SPXBCoopMdrSkin[5]; return NULL;
        }
        coopSkinPose_t *p=&s_coopSkin->poses[s_coopSkinPoses++];
        p->header=header; p->frame=backEnd.currentEntity->e.frame;
        p->oldFrame=backEnd.currentEntity->e.oldframe; p->lerpBits=lerpBits;
        p->first=s_coopSkinBones; p->count=header->numBones;
        memcpy(&s_coopSkin->bones[p->first],palette,p->count*sizeof(md4Bone_t));
        s_coopSkinBones+=p->count;
        ++g_SPXBCoopMdrSkin[11]; g_SPXBCoopMdrSkin[12]+=p->count*sizeof(md4Bone_t);
    }
    for (unsigned int i=0;i<s_coopSkinKeys;++i) {
        coopSkinKey_t *key=&s_coopSkin->keys[i];
        if (key->surface==surface && key->pose==pose && key->count==(unsigned int)surface->numVerts) {
            *hit=true; ++g_SPXBCoopMdrSkin[2];
            g_SPXBCoopMdrSkin[4]+=key->count; return key;
        }
    }
    if (s_coopSkinKeys==COOP_SKIN_KEYS || (unsigned int)surface->numVerts>COOP_SKIN_VERTICES-s_coopSkinVerts) {
        ++g_SPXBCoopMdrSkin[5]; return NULL;
    }
    coopSkinKey_t *key=&s_coopSkin->keys[s_coopSkinKeys++];
    key->surface=surface; key->pose=pose; key->first=s_coopSkinVerts; key->count=surface->numVerts;
    s_coopSkinVerts+=key->count; ++g_SPXBCoopMdrSkin[3]; return key;
}

static void R_STEFX_CopyCoopSkin(const coopSkinKey_t *key,int baseVertex) {
    for (unsigned int j=0;j<key->count;++j) {
        const coopSkinVertex_t *v=&s_coopSkin->vertices[key->first+j];
        memcpy(tess.xyz[baseVertex+j],v->xyz,sizeof(v->xyz));
        memcpy(tess.normal[baseVertex+j],v->normal,sizeof(v->normal));
        memcpy(tess.texCoords[baseVertex+j][0],v->uv,sizeof(v->uv));
    }
}

static void R_STEFX_StoreOrVerifyCoopSkin(const coopSkinKey_t *key,int baseVertex,bool hit) {
    for (unsigned int j=0;j<key->count;++j) {
        coopSkinVertex_t *v=&s_coopSkin->vertices[key->first+j];
        if (hit) {
            ++g_SPXBCoopMdrSkin[7];
            if (memcmp(tess.xyz[baseVertex+j],v->xyz,sizeof(v->xyz)) ||
                memcmp(tess.normal[baseVertex+j],v->normal,sizeof(v->normal)) ||
                memcmp(tess.texCoords[baseVertex+j][0],v->uv,sizeof(v->uv))) {
                ++g_SPXBCoopMdrSkin[8]; s_coopSkinFailed=true;
                g_SPXBCoopMdrSkin[14]=1; g_SPXBCoopMdrSkin[0]=0;
                XBLog_WriteCriticalf("STEFX_COOP_MDR_SKIN: byte mismatch; disabled surface=%p vertex=%u",key->surface,j);
                return; // Original tess data retained, even on failure.
            }
        } else {
            memcpy(v->xyz,tess.xyz[baseVertex+j],sizeof(v->xyz));
            memcpy(v->normal,tess.normal[baseVertex+j],sizeof(v->normal));
            memcpy(v->uv,tess.texCoords[baseVertex+j][0],sizeof(v->uv));
        }
    }
}
#endif
