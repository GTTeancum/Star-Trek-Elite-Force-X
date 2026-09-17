"""Exercise actual FX batching and BSP visibility predicates at their boundaries."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/research/letterbox_perf'
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')


def run(name, source):
    cpp, exe = out/(name+'.cpp'), out/(name+'.exe')
    cpp.write_text(source)
    subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe', '/nologo', '/MT',
                    '/I'+str(vc/'include'), str(cpp), '/Fo'+str(out/(name+'.obj')),
                    '/Fe'+str(exe), '/link', '/LIBPATH:'+str(vc/'lib'),
                    '/LIBPATH:'+str(vc/'PlatformSDK/Lib')], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
    print(result.stdout)
    (out/(name+'.txt')).write_text(result.stdout)


backend = (root/'code/renderer/retail_xbox/tr_backend_retail.cpp').read_text()
contract = backend.split('// STEFX_FX_BATCH_CONTRACT_BEGIN', 1)[1].split('// STEFX_FX_BATCH_CONTRACT_END', 1)[0]
run('fx_batch_contract_test', r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;
#define qtrue 1
#define qfalse 0
enum {RF_THIRD_PERSON=1,RF_FIRST_PERSON=2,RF_DEPTHHACK=4};
enum {CGEN_VERTEX,CGEN_EXACT_VERTEX,CGEN_IDENTITY,CGEN_IDENTITY_LIGHTING,CGEN_CONST,CGEN_WAVEFORM,CGEN_ENTITY};
enum {AGEN_VERTEX,AGEN_IDENTITY,AGEN_SKIP,AGEN_CONST,AGEN_WAVEFORM,AGEN_ENTITY};
enum {GF_SIN,GF_NOISE,GF_RAND};
enum {TMOD_NONE,TMOD_TRANSFORM,TMOD_TURBULENT,TMOD_SCROLL,TMOD_SCALE,TMOD_ROTATE,TMOD_STRETCH,TMOD_ENTITY_TRANSLATE};
enum {TCGEN_TEXTURE,TCGEN_ENVIRONMENT};
enum {CT_TWO_SIDED,CT_BACK_SIDED};
enum {RT_SPRITE,RT_ORIENTED_QUAD,RT_LINE,RT_TEXTURED_LINE,RT_ORIENTED_LINE,RT_TAPERED_LINE,RT_BEZIER,RT_EF_ORIENTED_SPRITE,RT_EF_ALPHA_VERT_POLY,RT_EF_LIGHTNING,RT_EF_ELECTRICITY,RT_MODEL};
enum {GLS_SRCBLEND_BITS=15,GLS_DSTBLEND_BITS=240,GLS_SRCBLEND_ONE=1,GLS_DSTBLEND_ONE=16,GLS_DEPTHMASK_TRUE=256,GLS_DEPTHTEST_DISABLE=512,GLS_ATEST_BITS=1024};
struct waveForm_t {int func;};
struct texModInfo_t {int type;waveForm_t wave;};
struct bundle_t {void*image;int numTexMods,tcGen;texModInfo_t*texMods;};
struct shaderStage_t {int active;void*ss;int isBumpMap,isEnvironment,rgbGen,alphaGen,stateBits;waveForm_t rgbWave,alphaWave;bundle_t bundle[2];};
struct shader_t {int numUnfoggedPasses,numDeforms,needsNormal,needsTangent;void*sky;shader_t*remappedShader;int cullType;shaderStage_t stages[1];};
struct refEntity_t {int renderfx,reType;float shaderTime;};
struct {struct{struct{refEntity_t e;}entities[2];}refdef;}backEnd;
qboolean R_STEFX_FxMergeReject(int,const shader_t*,const refEntity_t&){return qfalse;}
''' + contract + r'''
int main(){
 shader_t s;memset(&s,0,sizeof(s));s.numUnfoggedPasses=1;s.cullType=CT_TWO_SIDED;
 shaderStage_t &p=s.stages[0];p.active=1;p.rgbGen=CGEN_VERTEX;p.alphaGen=AGEN_VERTEX;p.bundle[0].tcGen=TCGEN_TEXTURE;
 p.stateBits=GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE;refEntity_t&e=backEnd.refdef.entities[0].e;e.reType=RT_LINE;
 assert(R_STEFX_CanMergeExtendedFx(&s,0,false));
 texModInfo_t mod;memset(&mod,0,sizeof(mod));mod.type=TMOD_SCROLL;p.bundle[0].numTexMods=1;p.bundle[0].texMods=&mod;
 assert(!R_STEFX_CanMergeExtendedFx(&s,0,false));assert(R_STEFX_CanMergeExtendedFx(&s,0,true));
 mod.type=TMOD_ENTITY_TRANSLATE;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));mod.type=TMOD_STRETCH;mod.wave.func=GF_RAND;
 assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));mod.wave.func=GF_SIN;assert(R_STEFX_CanMergeExtendedFx(&s,0,true));
 p.rgbGen=CGEN_WAVEFORM;p.rgbWave.func=GF_NOISE;assert(R_STEFX_CanMergeExtendedFx(&s,0,true));
 p.rgbWave.func=GF_RAND;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));p.rgbWave.func=GF_SIN;
 p.alphaGen=AGEN_WAVEFORM;p.alphaWave.func=GF_RAND;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));p.alphaWave.func=GF_SIN;
 e.shaderTime=1;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));e.shaderTime=0;
 e.renderfx=RF_DEPTHHACK;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));e.renderfx=0;
 p.rgbGen=CGEN_ENTITY;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));p.rgbGen=CGEN_VERTEX;
 p.alphaGen=AGEN_ENTITY;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));p.alphaGen=AGEN_VERTEX;
 s.remappedShader=&s;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));s.remappedShader=0;
 p.stateBits|=GLS_DEPTHMASK_TRUE;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));p.stateBits&=~GLS_DEPTHMASK_TRUE;
 e.reType=RT_MODEL;assert(!R_STEFX_CanMergeExtendedFx(&s,0,true));
 puts("FX batching: deterministic animation admitted; offsets, random state, entity uniforms, remaps, depth hacks and model geometry remain separate.");
}
''')

main = (root/'code/renderer/retail_xbox/tr_main_retail.cpp').read_text()
pvs = main.split('// STEFX_FX_PVS_BEGIN', 1)[1].split('// STEFX_FX_PVS_END', 1)[0]
run('fx_pvs_test', r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {RF_NODEPTH=1,RF_DEPTHHACK=2,RF_DISTORTION=4,RDF_NOWORLDMODEL=8,GLS_DEPTHTEST_DISABLE=16};
struct cplane_t{float normal[3],dist;};
struct mnode_t{int contents,visframe;unsigned int planeNum;mnode_t*children[2];};
struct refEntity_t{int renderfx,reType,customShader;};
struct shader_t{shader_t*remappedShader;int numUnfoggedPasses;struct{int stateBits;}stages[1];}shader;
shader_t*R_GetShaderByHandle(int){return &shader;}
struct world_t{mnode_t*nodes;cplane_t*planes;int numplanes;};
struct{struct{int stefxSplitView,isPortal;}viewParms;world_t*world;int viewCluster,visCount;struct{int rdflags;}refdef;}tr;
struct cvar_t{int integer;};cvar_t on={1},off={0};cvar_t*r_drawworld=&on,*r_nocull=&off,*r_novis=&off,*r_lockpvs=&off;
cvar_t*Cvar_Get(const char*,const char*,int){return &on;}
void XBLog_WriteCriticalf(const char*,...){}
float box[2][3];bool boundsValid=true;
bool STEFX_FxBounds(const refEntity_t&,float b[2][3]){memcpy(b,box,sizeof(box));return boundsValid;}
''' + pvs + r'''
int main(){
 cplane_t p[2]={{{1,0,0},-2},{{1,0,0},2}};
 mnode_t leaves[3]={{0,0,0,{0,0}},{0,7,0,{0,0}},{0,0,0,{0,0}}};
 mnode_t second={-1,7,1,{&leaves[2],&leaves[1]}};
 mnode_t root={-1,7,0,{&second,&leaves[0]}};
 world_t world={&root,p,2};tr.world=&world;tr.visCount=7;tr.viewCluster=0;tr.viewParms.stefxSplitView=1;
 shader.numUnfoggedPasses=1;refEntity_t e={0,0,0};
 box[0][0]=-10;box[1][0]=10;assert(STEFX_FxTouchesVisibleLeaf(box,&root,p,2,7));assert(!STEFX_CullFxOutsidePvs(e));
 box[0][0]=-8;box[1][0]=-6;assert(STEFX_CullFxOutsidePvs(e));
 box[0][0]=6;box[1][0]=8;assert(STEFX_CullFxOutsidePvs(e));
 box[0][0]=-3;box[1][0]=-2;assert(!STEFX_CullFxOutsidePvs(e));
 box[0][0]=-.5f;box[1][0]=.5f;assert(!STEFX_CullFxOutsidePvs(e));
 box[0][0]=6;box[1][0]=8;e.renderfx=RF_NODEPTH;assert(!STEFX_CullFxOutsidePvs(e));e.renderfx=0;
 shader.stages[0].stateBits=GLS_DEPTHTEST_DISABLE;assert(!STEFX_CullFxOutsidePvs(e));shader.stages[0].stateBits=0;
 tr.viewParms.isPortal=1;assert(!STEFX_CullFxOutsidePvs(e));tr.viewParms.isPortal=0;
 tr.viewCluster=-1;assert(!STEFX_CullFxOutsidePvs(e));tr.viewCluster=0;
 tr.refdef.rdflags=RDF_NOWORLDMODEL;assert(!STEFX_CullFxOutsidePvs(e));tr.refdef.rdflags=0;
 root.visframe=6;assert(!STEFX_CullFxOutsidePvs(e));root.visframe=7;
 r_lockpvs=&on;assert(!STEFX_CullFxOutsidePvs(e));r_lockpvs=&off;
 root.planeNum=99;assert(!STEFX_CullFxOutsidePvs(e));root.planeNum=0;
 root.children[0]=0;assert(!STEFX_CullFxOutsidePvs(e));root.children[0]=&second;
 mnode_t deep[140];for(int i=0;i<140;++i){deep[i].contents=-1;deep[i].visframe=7;deep[i].planeNum=0;deep[i].children[0]=&deep[(i+1)%140];deep[i].children[1]=&leaves[0];}
 assert(STEFX_FxTouchesVisibleLeaf(box,deep,p,2,7));assert(g_SPXBFxPvs[2]>=3);
 puts("FX visibility: crossing beams and partition grazing retained; hidden leaves rejected; through-wall/portal/stale-PVS cases and bounded traversal fail open.");
}
''')
