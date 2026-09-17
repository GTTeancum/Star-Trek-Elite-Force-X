"""Exercise the real cache lookup/eviction code with two live frame pointers."""
from pathlib import Path
import subprocess
import argparse

root=Path(__file__).resolve().parents[2]
source=(root/'code/renderer/tr_model.cpp').read_text()
helper=source.split('// STEFX_MDR_FRAME_REPLACEMENT_BEGIN',1)[1].split('// STEFX_MDR_FRAME_REPLACEMENT_END',1)[0]
function=source.split('const void *R_STEFX_GetMDRFrame(',1)[1]
body=function[function.index('\tfor (i = 0; i < STEFX_BORG_MDR_FRAME_CACHE_SLOTS; ++i)'):]
body=body[:body.index('\t\tif (mod->stefxMdrFrameBaseCount')]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define STEFX_BORG_MDR_FRAME_CACHE_SLOTS 32
#define STEFX_ELITE_FORCE_SP 1
typedef unsigned char byte;
struct md4Header_t { int identity; } header;
struct stefxBorgMdrFrameCache_t {const md4Header_t*header; int frame; int data[2];};
static stefxBorgMdrFrameCache_t s_stefxBorgMdrFrameCache[32];
static int s_stefxBorgMdrFrameCacheReplace,s_stefxMdrFrameLastReturned=-1;
unsigned int g_SPXBMdrFramePairProtected;
void XBLog_WriteCritical(const char*){}
void reset(){memset(s_stefxBorgMdrFrameCache,0,sizeof(s_stefxBorgMdrFrameCache));s_stefxBorgMdrFrameCacheReplace=0;s_stefxMdrFrameLastReturned=-1;g_SPXBMdrFramePairProtected=0;}
'''
def get_function(name,actual):
    return 'const void *'+name+'(const md4Header_t *header,int frame){int i;\n'+actual+'\ncache->data[0]=frame;return cache->data;}}\n'
suffix=r'''
int main(){
 reset();for(int i=0;i<32;++i)legacy(&header,i);
 const int*a=(const int*)legacy(&header,0);assert(*a==0);
 const int*b=(const int*)legacy(&header,32);assert(*b==32&&*a==32);
 reset();for(int i=0;i<32;++i)fixed(&header,i);
 a=(const int*)fixed(&header,0);b=(const int*)fixed(&header,32);
 if(!(*a==0&&*b==32&&g_SPXBMdrFramePairProtected==1)){puts("FAIL: actual frame lookup overwrote the live current-frame pointer");return 1;}
 md4Header_t second;
 for(int j=0;j<10000;++j){int current=j%32,old=(j%7)?j+1000:current;
  const md4Header_t*h=(j%2)?&header:&second;
  a=(const int*)fixed(h,current);b=(const int*)fixed(h,old);
  assert(*a==current&&*b==old);
 }
 puts("MDR frame pair: reproduced FIFO hit/miss pointer overwrite; fixed actual lookup/eviction passed 10,000 pairs, two models, equal frames and eviction wraparound.");
}
'''
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode',choices=['sp','mp','both'],default='both')
args=parser.parse_args()
out=root/'build/research/letterbox_perf'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
legacy=body.replace('#if defined(STEFX_SP_HOSTED_MP)','#if 0').replace('#if defined(STEFX_ELITE_FORCE_SP)','#if 0')
for mode in (['sp','mp'] if args.mode=='both' else [args.mode]):
 cpp=out/('mdr_frame_pair_'+mode+'_test.cpp');exe=cpp.with_suffix('.exe')
 definitions='#define STEFX_SP_HOSTED_MP 1\n' if mode=='mp' else ''
 cpp.write_text(definitions+prefix+helper+get_function('legacy',legacy)+get_function('fixed',body)+suffix)
 subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
  '/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),
  '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
 result=subprocess.run([str(exe)])
 print('Configuration',mode,'result',result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
