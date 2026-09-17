"""Exercise actual phase-scope accounting with a controlled cycle clock."""
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
header=(root/'SP-Mod-Source-Code-master/cgame/stefx_player_profile.h').read_text()
prefix='''#include <assert.h>
#include <stdio.h>
static unsigned __int64 now;
unsigned __int64 STEFX_XboxReadTsc(){return now;}
unsigned int STEFX_XboxElapsedCycles(unsigned __int64 start){return (unsigned int)(now-start);}
'''
body='''
void early(){STEFX_PlayerProfile scope; now+=5; STEFX_ProfilePlayerPhase(0x81);now+=7;return;}
int main(){
 STEFX_ResetPlayerProfile(false);early();assert(!s_playerEntered&&!s_playerCompleted&&!s_playerProfile);
 STEFX_ResetPlayerProfile(true);early();
 assert(s_playerEntered==1&&s_playerCompleted==1&&!s_playerProfile);
 assert(s_playerCycles[0]==5&&s_playerCycles[0x81]==7&&s_playerVisits[0x81]==1);
 STEFX_ResetPlayerProfile(true);
 {STEFX_PlayerProfile outer;now+=3;STEFX_ProfilePlayerPhase(4);now+=11;
  {STEFX_PlayerProfile inner;now+=13;STEFX_ProfilePlayerPhase(5);now+=17;}
  now+=19;}
 assert(s_playerEntered==2&&s_playerCompleted==2&&!s_playerProfile);
 assert(s_playerCycles[0]==16&&s_playerCycles[4]==30&&s_playerCycles[5]==17);
 unsigned int total=0;for(int i=0;i<256;++i)total+=s_playerCycles[i];assert(total==63);
 STEFX_ResetPlayerProfile(true);for(int j=0;j<256;++j)assert(!s_playerCycles[j]&&!s_playerVisits[j]);
 assert(!s_playerEntered&&!s_playerCompleted);
 puts("Actor phase accounting: inactive, early-return, nested scopes, exclusive intervals and reset passed.");
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_player_profile_test.cpp';exe=cpp.with_suffix('.exe')
cpp.write_text(prefix+header+body)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/O2','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(cpp.with_suffix('.obj')),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
r=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
print(r.stdout);(out/'coop_player_profile_test.txt').write_text(r.stdout)
