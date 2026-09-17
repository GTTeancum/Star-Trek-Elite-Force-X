"""Compile actual conservative per-view model visibility code with VC71."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/research/letterbox_perf'
helper=(root/'code/renderer/retail_xbox/stefx_coop_model_pvs.h').read_text()
prefix=r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define _XBOX 1
#define STEFX_ELITE_FORCE_SP 1
#define MD3_MAX_LODS 3
typedef unsigned char byte;typedef int surfaceType_t;
enum{MOD_MESH=1,MOD_MDR=2,SF_MD3=7,SF_MDR=12,SS_PORTAL=1,GLS_DEPTHTEST_DISABLE=1,RDF_NOWORLDMODEL=1};
enum{RF_THIRD_PERSON=1,RF_WRAP_FRAMES=2,RF_CAP_FRAMES=4,RF_NOSHADOW=8,RF_LIGHTING_ORIGIN=16,RF_DEPTHHACK=32};
struct cplane_t{float normal[3],dist;};
struct mnode_t{int contents,visframe;unsigned int planeNum;mnode_t*children[2];};
struct refEntity_t{float origin[3],axis[3][3],backlerp;int frame,oldframe,renderfx;};
struct trRefEntity_t{refEntity_t e;};
struct md3Header_t{int numFrames,ofsFrames;};struct md3Frame_t{float bounds[2][3];};
struct md4Header_t{int numFrames,ofsFrames,numBones;};struct md4Frame_t{float bounds[2][3];float bones[1][12];};
struct model_t{int type;md3Header_t*md3[3];md4Header_t*md4;};
struct shader_t{const char*name;shader_t*remappedShader;int numDeforms,sort,numUnfoggedPasses;void*sky;struct{int stateBits;void*ss;}stages[1];};
struct world_t{mnode_t*nodes;cplane_t*planes;int numplanes;};
struct{struct{int stefxSplitView,isPortal;}viewParms;world_t*world;int viewCluster,visCount;struct{int rdflags;}refdef;shader_t*shadowShader,*projectionShadowShader;}tr;
struct cvar_t{int integer;};cvar_t mode={1},on={1},off={0};
cvar_t*r_drawworld=&on,*r_nocull=&off,*r_novis=&off,*r_lockpvs=&off,*r_shadows=&off;
cvar_t*Cvar_Get(const char*,const char*,int){return &mode;}
void XBLog_WriteCriticalf(const char*,...){}
float frames[2][6]={{-2,-3,-4,2,3,4},{-5,-6,-7,5,6,7}},rotating[6];
const void*R_STEFX_GetMDRFrame(const md4Header_t*,int n){memcpy(rotating,frames[n],sizeof(rotating));return rotating;}
"""
suffix=r"""
int main(){
 cplane_t plane={ {1,0,0},0 };mnode_t visible={0,7,0,{0,0}},hidden={0,6,0,{0,0}},root={-1,7,0,{&visible,&hidden}};
 world_t world={&root,&plane,1};tr.world=&world;tr.visCount=7;tr.viewCluster=0;tr.viewParms.stefxSplitView=1;
 trRefEntity_t ent;memset(&ent,0,sizeof(ent));for(int i=0;i<3;++i)ent.e.axis[i][i]=1;
 struct{md3Header_t h;md3Frame_t f[2];}mesh;mesh.h.numFrames=2;mesh.h.ofsFrames=sizeof(mesh.h);memcpy(mesh.f,frames,sizeof(frames));
 model_t model;memset(&model,0,sizeof(model));model.type=MOD_MESH;model.md3[0]=&mesh.h;
 ent.e.origin[0]=-20;ent.e.oldframe=1;float b[2][3];assert(STEFX_CoopModelBounds(&model,ent.e,b));
 assert(b[0][0]==-26.125f&&b[1][0]==-13.875f);assert(STEFX_CoopModelOutsidePvs(&ent,&model));
 ent.e.origin[0]=-2;assert(!STEFX_CoopModelOutsidePvs(&ent,&model)); // crosses partition
 ent.e.origin[0]=-20;ent.e.axis[0][0]=0;ent.e.axis[0][1]=2;ent.e.axis[1][0]=-3;ent.e.axis[1][1]=0;
 assert(STEFX_CoopModelBounds(&model,ent.e,b));assert(b[0][0]==-39.375f&&b[1][0]==-0.625f);
 assert(!STEFX_CoopModelOutsidePvs(&ent,&model)); // enlarged rotated box grazes plane
 ent.e.axis[0][0]=1;ent.e.axis[0][1]=0;ent.e.axis[1][0]=0;ent.e.axis[1][1]=1;
 ent.e.frame=2;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));ent.e.frame=0;
 ent.e.backlerp=2;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));ent.e.backlerp=0;
 ent.e.renderfx=RF_DEPTHHACK;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));ent.e.renderfx=0;
 mode.integer=0;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));mode.integer=1;
 tr.viewParms.isPortal=1;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));tr.viewParms.isPortal=0;
 tr.viewParms.stefxSplitView=0;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));tr.viewParms.stefxSplitView=1;
 root.visframe=6;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));root.visframe=7;
 root.planeNum=2;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));root.planeNum=0;
 root.children[1]=0;assert(!STEFX_CoopModelOutsidePvs(&ent,&model));root.children[1]=&hidden;
 mnode_t cyclic={-1,7,0,{&visible,0}};cyclic.children[1]=&cyclic;assert(STEFX_CoopBoxTouchesVisibleLeaf(b,&cyclic,&plane,1,7));
 md4Header_t mdr={2,-1,1};model.type=MOD_MDR;model.md4=&mdr;
 assert(STEFX_CoopModelBounds(&model,ent.e,b));assert(b[0][0]==-149&&b[1][0]==109&&b[1][2]==161);
 // Bound union copied before frame cache rotation; use extents beyond fallback.
 frames[0][0]=-300;frames[1][3]=400;assert(STEFX_CoopModelBounds(&model,ent.e,b));assert(b[0][0]==-321.125f&&b[1][0]==381.125f);
 shader_t shader;memset(&shader,0,sizeof(shader));shader.name="fixture";shader.numUnfoggedPasses=1;surfaceType_t type=SF_MD3;
 s_coopModelOutsidePvs=true;assert(STEFX_CoopSkipHiddenSurface(&type,&shader));
 mode.integer=2;assert(!STEFX_CoopSkipHiddenSurface(&type,&shader));mode.integer=1;
 shader.numDeforms=1;assert(!STEFX_CoopSkipHiddenSurface(&type,&shader));shader.numDeforms=0;
 shader.stages[0].stateBits=GLS_DEPTHTEST_DISABLE;assert(!STEFX_CoopSkipHiddenSurface(&type,&shader));shader.stages[0].stateBits=0;
 shader.remappedShader=&shader;assert(!STEFX_CoopSkipHiddenSurface(&type,&shader));shader.remappedShader=0;
 tr.shadowShader=&shader;assert(!STEFX_CoopSkipHiddenSurface(&type,&shader));tr.shadowShader=0;
 puts("Co-op model PVS: transformed whole bounds, frame union/cache rotation, partition grazing, malformed trees, unsupported state and observe mode passed.");
}
"""
cpp=out/'coop_model_pvs_test.cpp';exe=out/'coop_model_pvs_test.exe';cpp.write_text(prefix+helper+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_model_pvs_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
