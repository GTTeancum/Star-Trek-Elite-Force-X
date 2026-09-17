"""Exercise the actual deferred-model timing accumulator."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root/'build/research/letterbox_perf'
text = (root/'SP-Mod-Source-Code-master/cgame/cg_players.cpp').read_text()
start = text.index('extern "C" __declspec(dllexport) volatile unsigned int g_SPXBModelRegisterStats')
end = text.index('\n#endif', start)
source = '''#include <stdio.h>
struct {int time;} cg;
unsigned int now;
int cgi_Milliseconds(){return (int)now;}
''' + text[start:end] + '''
int main(){int failed=0;cg.time=100;now=110;
failed+=STEFX_RecordModelRegistration(7,100)!=10;
cg.time=200;now=220;failed+=STEFX_RecordModelRegistration(9,200)!=20;
cg.time=300;now=4;failed+=STEFX_RecordModelRegistration(11,0xfffffffa)!=10;
unsigned int expected[8]={3,40,20,9,200,11,10,300};
for(int i=0;i<8;++i)failed+=g_SPXBModelRegisterStats[i]!=expected[i];
printf("Model registration timing: duration, peak attribution, latest event, wrap; failures=%d\\n",failed);
return failed;}
'''
cpp=out/'model_registration_timing.cpp'; cpp.write_text(source)
exe=out/'model_registration_timing.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'model_registration_timing.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
(out/'model_registration_timing.txt').write_text(result.stdout)
print(result.stdout)
