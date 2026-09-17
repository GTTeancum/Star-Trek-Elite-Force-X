"""Check that real FPS-window code rejects intro and transition samples."""
from pathlib import Path
import subprocess
import re

root = Path(__file__).resolve().parents[2]
out = root/'build/research/letterbox_perf'
text = (root/'code/client/cl_main.cpp').read_text()
frame_body = text[text.index('void CL_Frame ('):text.index('//============================================================================', text.index('void CL_Frame ('))]
def brackets_rendering(body):
    marks = [match.start() for match in re.finditer(r'CL_STEFX_MarkFpsContext\(\);', body)]
    return (len(marks) == 2 and marks[0] < body.index('SCR_UpdateScreen();')
            and marks[1] > body.rindex('SCR_RunCinematic();')
            and marks[1] < body.index('CL_STEFX_WriteFpsWithContext(msg,'))
assert brackets_rendering(frame_body), 'Context checks must bracket actual rendering, not both precede it'
old_placement = frame_body.replace('\tCL_STEFX_MarkFpsContext();', '', 1)
old_placement = old_placement.replace('\tCL_STEFX_MarkFpsContext();', '')
old_placement = old_placement.replace('void CL_Frame ( int msec,float fractionMsec ) {',
    'void CL_Frame ( int msec,float fractionMsec ) {\nCL_STEFX_MarkFpsContext();\nCL_STEFX_MarkFpsContext();')
assert not brackets_rendering(old_placement), 'Negative control must reject adjacent entry checks'
start = text.index('static unsigned int s_stefxFpsExcludedChecks')
end = text.index('\n#endif\nvoid CL_Frame', start)
source = r'''
#include <stdio.h>
#include <string.h>
#define STEFX_ELITE_FORCE_SP 1
#define CA_ACTIVE 1
#define KEYCATCH_UI 2
#define qtrue 1
#define qfalse 0
typedef int qboolean;
struct Client {int state,keyCatchers;} cls;
extern "C" volatile unsigned int g_SPXBMapHash=1;
bool intro;char logged[256];
unsigned int clockMsec;
int Sys_Milliseconds(){return (int)clockMsec;}
qboolean STEFX_XboxSuppressPlayerPresentation(){return intro;}
void XBLog_WriteFpsProfile(const char* msg){strcpy(logged,msg);}
''' + text[start:end] + r'''
int sample(int expected){char msg[256]="fps=60.0";
 CL_STEFX_WriteFpsWithContext(msg,sizeof(msg));
 return (strstr(logged,"gameplay=1 excludedChecks=0")!=0)!=expected;}
int main(){int failed=0;
 cls.state=CA_ACTIVE;intro=true;CL_STEFX_MarkFpsContext();
 intro=false;CL_STEFX_MarkFpsContext();failed+=sample(0);
 CL_STEFX_MarkFpsContext();failed+=sample(1);
 intro=true;CL_STEFX_MarkFpsContext();failed+=sample(0);
 intro=false;cls.keyCatchers=KEYCATCH_UI;CL_STEFX_MarkFpsContext();
 cls.keyCatchers=0;CL_STEFX_MarkFpsContext();failed+=sample(0);
 CL_STEFX_MarkFpsContext();failed+=sample(1);
 g_SPXBMapHash=2;CL_STEFX_MarkFpsContext();failed+=sample(0);
 CL_STEFX_MarkFpsContext();failed+=sample(1);
 cls.state=0;CL_STEFX_MarkFpsContext();cls.state=CA_ACTIVE;
 CL_STEFX_MarkFpsContext();failed+=sample(0);
 cls.state=CA_ACTIVE;CL_STEFX_MarkFpsContext();
 CL_STEFX_RecordFrameTime();
 clockMsec+=10;CL_STEFX_RecordFrameTime();
 clockMsec+=20;CL_STEFX_RecordFrameTime();
 clockMsec+=60;CL_STEFX_RecordFrameTime();
 clockMsec+=300;CL_STEFX_RecordFrameTime();
 failed+=sample(1);failed+=strstr(logged,"ft=4/390/300/300/300/2 ")==0;
 clockMsec+=10;CL_STEFX_RecordFrameTime();failed+=sample(1);
 failed+=strstr(logged,"ft=1/10/10/10/10/0 ")==0;
 s_stefxFrameTimePrevious=0xfffffffau;clockMsec=4;
 CL_STEFX_RecordFrameTime();failed+=sample(1);
 failed+=strstr(logged,"ft=1/10/10/10/10/0 ")==0;
 for(int i=0;i<100;++i){clockMsec+=i<95?10:60;CL_STEFX_RecordFrameTime();}
 failed+=sample(1);failed+=strstr(logged,"ft=100/1250/60/10/60/5 ")==0;
 // Both CL_Frame endpoints are active, but a four-second movie runs between.
 CL_STEFX_MarkFpsContext();CL_STEFX_ExcludeBlockingMovieFromFps();
 clockMsec+=4000;CL_STEFX_RecordFrameTime();CL_STEFX_MarkFpsContext();
 failed+=sample(0);failed+=strstr(logged,"ft=1/4000/4000/4000/4000/1 ")==0;
 clockMsec+=10;CL_STEFX_RecordFrameTime();CL_STEFX_MarkFpsContext();
 failed+=sample(1);failed+=strstr(logged,"ft=1/10/10/10/10/0 ")==0;
 // An equally long real gameplay stall must remain visible and eligible.
 clockMsec+=4000;CL_STEFX_RecordFrameTime();CL_STEFX_MarkFpsContext();
 failed+=sample(1);failed+=strstr(logged,"ft=1/4000/4000/4000/4000/1 ")==0;
 printf("FPS context: 11 exclusion windows and 7 frame-time cases, failures=%d\n",failed);return failed;
}
'''
cpp=out/'fps_context_test.cpp';cpp.write_text(source)
exe=out/'fps_context_test.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'fps_context_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
(out/'fps_context_test.txt').write_text(result.stdout)
print(result.stdout)
print('Frame entry/exit placement and adjacent-entry negative control passed')
