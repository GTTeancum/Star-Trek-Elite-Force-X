// Experimental ordered model strips. CPU topology only; no GPU-owned storage.
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
extern "C" volatile unsigned int g_SPXBCoopModelStrips[16] = {0};
// mode, draws, list vertices, strip vertices, cache hits, cache misses,
// fallback draws, verified triangles, mismatches, cache bytes, cache entries,
// storage bytes, state exclusions, table full, arena full, verified draws.
static cvar_t *s_coopStripsMode;
// STEFX_COOP_STRIPS_CORE_BEGIN
struct stefxCoopStripKey_t { unsigned int hash, offset; unsigned short count, vertices, output, occupied; };
static stefxCoopStripKey_t s_coopStripKeys[128];
static GLushort s_coopStripArena[32768], s_coopStripWork[SHADER_MAX_INDEXES];
static unsigned int s_coopStripUsed;
static void STEFX_CoopStripsReset(void) {
    memset(s_coopStripKeys,0,sizeof(s_coopStripKeys));
    s_coopStripUsed=0;
    g_SPXBCoopModelStrips[9]=g_SPXBCoopModelStrips[10]=0;
    g_SPXBCoopModelStrips[11]=sizeof(s_coopStripKeys)+sizeof(s_coopStripArena)+sizeof(s_coopStripWork);
}
static bool STEFX_CoopStripTri(const GLushort *t, unsigned int a, unsigned int b, unsigned int c) {
    return (t[0]==a && t[1]==b && t[2]==c) ||
           (t[1]==a && t[2]==b && t[0]==c) ||
           (t[2]==a && t[0]==b && t[1]==c);
}
static bool STEFX_CoopStripVerify(const GLushort *input,int count,const GLushort *strip,int output) {
    int cursor=0;
    for (int i=0;i+2<output;++i) {
        unsigned int a=strip[i],b=strip[i+1],c=strip[i+2];
        if (a==b || b==c || a==c) continue;
        if (i&1) { unsigned int swap=a; a=b; b=swap; }
        while (cursor<count && (input[cursor]==input[cursor+1] || input[cursor+1]==input[cursor+2] || input[cursor]==input[cursor+2])) cursor+=3;
        if (cursor>=count || !STEFX_CoopStripTri(input+cursor,a,b,c)) return false;
        cursor+=3;
    }
    while (cursor<count && (input[cursor]==input[cursor+1] || input[cursor+1]==input[cursor+2] || input[cursor]==input[cursor+2])) cursor+=3;
    return cursor==count;
}
// Preserve original primitive order/winding. Try the three cyclic seeds, then
// stitch runs using zero-area triangles. Never allocate a larger payload.
static int STEFX_CoopStripBuild(const GLushort *input,int count,GLushort *out) {
    if (!input || count<3 || count%3 || count>SHADER_MAX_INDEXES) return 0;
    int cursor=0,used=0;
    while (cursor<count) {
        int bestEnd=cursor+3,bestRotation=0;
        for (int rotation=0;rotation<3;++rotation) {
            unsigned int a=input[cursor+(rotation+1)%3],b=input[cursor+(rotation+2)%3];
            int end=cursor+3,runVertices=3;
            while (end<count) {
                unsigned int first=a,second=b;
                if ((runVertices-2)&1) { first=b;second=a; }
                int r;
                for (r=0;r<3;++r) if (input[end+r]==first && input[end+(r+1)%3]==second) break;
                if (r==3) break;
                a=b;b=input[end+(r+2)%3];++runVertices;end+=3;
            }
            if (end>bestEnd) {bestEnd=end;bestRotation=rotation;}
        }
        int needed=2+(bestEnd-cursor)/3+(used ? 2+(used&1) : 0);
        if (used+needed>=count) return 0; // Remaining runs cannot reduce this prefix.
        if (used) {
            if (used&1) {out[used]=out[used-1];++used;}
            out[used]=out[used-1];++used;
            out[used++]=input[cursor+bestRotation];
        }
        for (int r=0;r<3;++r) out[used++]=input[cursor+(bestRotation+r)%3];
        for (int end=cursor+3;end<bestEnd;end+=3) {
            unsigned int a=out[used-2],b=out[used-1];
            if ((used-2)&1) {unsigned int swap=a;a=b;b=swap;}
            int r;
            for (r=0;r<3;++r) if (input[end+r]==a && input[end+(r+1)%3]==b) break;
            if (r==3) return 0;
            out[used++]=input[end+(r+2)%3];
        }
        cursor=bestEnd;
    }
    return used;
}
static const GLushort *STEFX_CoopStripLookup(const GLushort *input,int count,int vertices,int *output) {
    // A cheap bucket selector only. Every hit compares ALL original indices.
    unsigned int hash=count*65599u+vertices*31u;
    const int taps[]={0,count/4,count/2,count-1};
    for (int t=0;t<4;++t) hash=hash*33u+input[taps[t]];
    stefxCoopStripKey_t *freeKey=NULL;
    for (unsigned int probe=0;probe<32;++probe) {
        stefxCoopStripKey_t *key=&s_coopStripKeys[(hash+probe)&127];
        if (!key->occupied) {freeKey=key;break;}
        if (key->hash==hash && key->count==count && key->vertices==vertices &&
            !memcmp(s_coopStripArena+key->offset,input,count*sizeof(GLushort))) {
            ++g_SPXBCoopModelStrips[4];*output=key->output;
            return key->output ? s_coopStripArena+key->offset+count : NULL;
        }
    }
    ++g_SPXBCoopModelStrips[5];
    if (!freeKey) {++g_SPXBCoopModelStrips[13];return NULL;}
    const int encoded=STEFX_CoopStripBuild(input,count,s_coopStripWork);
    if (s_coopStripUsed+count+encoded>32768u) {++g_SPXBCoopModelStrips[14];return NULL;}
    // Validate every new plan before it can reach the renderer, including mode1.
    if (encoded && !STEFX_CoopStripVerify(input,count,s_coopStripWork,encoded)) {
        ++g_SPXBCoopModelStrips[8];return NULL;
    }
    freeKey->hash=hash;freeKey->offset=s_coopStripUsed;
    freeKey->count=(unsigned short)count;freeKey->vertices=(unsigned short)vertices;
    freeKey->output=(unsigned short)encoded;freeKey->occupied=1;
    memcpy(s_coopStripArena+s_coopStripUsed,input,count*sizeof(GLushort));s_coopStripUsed+=count;
    memcpy(s_coopStripArena+s_coopStripUsed,s_coopStripWork,encoded*sizeof(GLushort));s_coopStripUsed+=encoded;
    g_SPXBCoopModelStrips[9]=s_coopStripUsed*sizeof(GLushort);++g_SPXBCoopModelStrips[10];
    *output=encoded;
    return encoded ? s_coopStripArena+freeKey->offset+count : NULL;
}
// STEFX_COOP_STRIPS_CORE_END
static const GLushort *STEFX_CoopStripsWanted(const GLushort *input,int count,int *output) {
    if (!s_coopStripsMode) {s_coopStripsMode=Cvar_Get("r_efCoopModelStrips","0",0);STEFX_CoopStripsReset();}
    const int selected=s_coopStripsMode->integer;g_SPXBCoopModelStrips[0]=selected;
    if (selected<1 || selected>2 || !backEnd.currentEntity || backEnd.currentEntity==&tr.worldEntity ||
        backEnd.currentEntity->e.reType!=RT_MODEL || !backEnd.viewParms.stefxSplitView || backEnd.projection2D) return NULL;
    // XDK GetRenderState reads D3D__RenderState, without a device flush. Read
    // actual state rather than tracking wrappers: stencil helpers set it too.
    DWORD shade,front,back;
    glw_state->device->GetRenderState(D3DRS_SHADEMODE,&shade);
    glw_state->device->GetRenderState(D3DRS_FILLMODE,&front);
    glw_state->device->GetRenderState(D3DRS_BACKFILLMODE,&back);
    if (shade!=D3DSHADE_GOURAUD || front!=D3DFILL_SOLID || back!=D3DFILL_SOLID) {
        ++g_SPXBCoopModelStrips[12];return NULL;
    }
    const GLushort *strip=STEFX_CoopStripLookup(input,count,tess.numVertexes,output);
    if (!strip) {++g_SPXBCoopModelStrips[6];return NULL;}
    if (selected==2) {
        if (!STEFX_CoopStripVerify(input,count,strip,*output)) {
            ++g_SPXBCoopModelStrips[8];s_coopStripsMode->integer=0;return NULL;
        }
        g_SPXBCoopModelStrips[7]+=count/3;++g_SPXBCoopModelStrips[15];
    }
    ++g_SPXBCoopModelStrips[1];g_SPXBCoopModelStrips[2]+=count;g_SPXBCoopModelStrips[3]+=*output;
    return strip;
}
#endif
