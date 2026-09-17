"""Check the actual optional allocation and model-storage ownership helpers."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/research/letterbox_perf'
zone=(root/'code/qcommon/z_memman_console.cpp').read_text()
start=zone.index('void *Z_TryMalloc(');zone=zone[start:zone.index('\n#endif',start)]
model=(root/'code/renderer/tr_model.cpp').read_text()
start=model.index('static byte *STEFX_AllocMdrStorage(')
helpers=model[start:model.index('static void STEFX_ClearMdrFrameCache',start)]
start=model.index('\t\t// A new base has no consumers')
cleanup=model[start:model.index('\t\tXBLF(',start)]
source=r'''
#include <stdio.h>
#include <string.h>
typedef unsigned char byte;typedef unsigned long DWORD;typedef int memtag_t;typedef int qboolean;
#define qtrue 1
#define qfalse 0
#define INFINITE 0xffffffff
#define WAIT_OBJECT_0 0
#define WAIT_ABANDONED_0 0x80
#define TAG_MODEL_MD3 12
bool s_Initialized=true,heapOK,fit;int s_Mutex=1,depth=0,zoneCalls=0,heapFrees=0,zoneFrees=0,bad=0;
DWORD waitResult=0;byte heapData[128],zoneData[128];
void Com_InitZoneMemory(){s_Initialized=true;}
DWORD WaitForSingleObject(int,DWORD){if(waitResult==0||waitResult==0x80)++depth;return waitResult;}
void ReleaseMutex(int){--depth;}
bool Z_WouldAllocFit(int size,int,int,int*,int*,int*){bad+=depth!=1;return fit&&size>0;}
void*Z_Malloc(int,int,int,int){bad+=depth!=1;++zoneCalls;return zoneData;}
void Z_Free(void*p){bad+=p!=zoneData;++zoneFrees;}
int GetProcessHeap(){return 1;}
void*HeapAlloc(int,int,int){return heapOK?heapData:0;}
void HeapFree(int,int,void*p){bad+=p!=heapData;++heapFrees;}
void XBLog_WriteCriticalf(const char*,...){}
''' + zone + helpers + r'''
struct Base {byte*frames;qboolean heapAllocated;int count;};
void discardNewBase(bool newBase,Base*base){
''' + cleanup + r'''
}
int main(){int failed=0;qboolean onHeap;byte*p;
 heapOK=true;fit=true;p=STEFX_AllocMdrStorage(80,&onHeap);
 failed+=p!=heapData||!onHeap||zoneCalls;STEFX_FreeMdrStorage(p,onHeap);failed+=heapFrees!=1;
 heapOK=false;p=STEFX_AllocMdrStorage(80,&onHeap);
 failed+=p!=zoneData||onHeap||depth;STEFX_FreeMdrStorage(p,onHeap);failed+=zoneFrees!=1;
 fit=false;failed+=STEFX_AllocMdrStorage(80,&onHeap)!=0||depth||zoneCalls!=1;
 fit=true;waitResult=0x102;failed+=Z_TryMalloc(80,12,32)!=0||depth;
 waitResult=0x80;failed+=Z_TryMalloc(80,12,32)!=zoneData||depth;
 Base b={zoneData,0,10};discardNewBase(false,&b);failed+=b.frames!=zoneData||zoneFrees!=1;
 discardNewBase(true,&b);failed+=b.frames!=0||b.count!=0||zoneFrees!=2;
 failed+=bad;printf("MDR storage: 9 cases, failures=%d\n",failed);return failed;}
'''
cpp=out/'mdr_storage_test.cpp';cpp.write_text(source);exe=out/'mdr_storage_test.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'mdr_storage_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
(out/'mdr_storage_test.txt').write_text(r.stdout);print(r.stdout)
