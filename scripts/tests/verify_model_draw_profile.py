"""Compile actual bounded model/material aggregation helper with the Xbox compiler."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'code/win32/win_qgl_dx8.cpp').read_text()
helper=s.split('// STEFX_MODEL_DRAW_PROFILE_BEGIN')[1].split('// STEFX_MODEL_DRAW_PROFILE_END')[0]
prefix='#include <assert.h>\n#include <string.h>\nstruct Guarded { unsigned int before; volatile unsigned int words[2056]; unsigned int after; } storage;\n#define g_SPXBPerfModelDrawsCurrent storage.words\n'
suffix=r'''
unsigned int shaderFor(unsigned int handle){return 0x20000+(((handle*2654435761u)&255u)<<4);}
void check(unsigned int keys){
 memset(&storage,0,sizeof(storage));storage.before=0x12345678;storage.after=0x87654321;
 unsigned int i;
 // All keys deliberately collide; exercise wraparound, full table and overflow.
 for(i=0;i<keys;++i)STEFX_RecordModelDraw(i,shaderFor(i),0x30000+i*4,10,12,15,8);
 STEFX_RecordModelDraw(1,shaderFor(1),0x30004,10,12,15,8);
 unsigned int calls=0,vertices=0,cycles=0,duplicates=0;
 for(i=0;i<257;++i){
  volatile unsigned int *row=storage.words+i*8;
  calls+=row[2];vertices+=row[3];cycles+=row[5];
  if(row[2] && i<256){
   assert(row[0]<keys && row[1]==shaderFor(row[0]) && row[7]==0x30000+row[0]*4);
   assert(row[2]==(row[0]==1?2u:1u));
   if(row[0]==1)++duplicates;
  }
 }
 assert(calls==keys+1 && vertices==(keys+1)*10 && cycles==(keys+1)*15 && duplicates==1);
 assert(storage.words[2050]==(keys>256?keys-256:0));
 if(keys>256)assert(storage.words[2048]==0xffffffffu && storage.words[2049]==0 && storage.words[2055]==0);
 assert(storage.before==0x12345678 && storage.after==0x87654321);
}
int main(){check(70);check(256);check(270);return 0;}
'''
out=root/'build/research/letterbox_perf';cpp=out/'model_draw_profile_test.cpp';exe=out/'model_draw_profile_test.exe'
cpp.write_text(prefix+helper+suffix)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'model_draw_profile_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
print('Model draw profile: bounded keys, aggregation and overflow passed')
