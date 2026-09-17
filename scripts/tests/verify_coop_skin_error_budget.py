"""Check the diagnostic-only numerical budget with the compiled classifier."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'code/renderer/tr_animation.cpp').read_text()
helper=s.split('// STEFX_COOP_SKIN_ERROR_BUDGET_BEGIN')[1].split('// STEFX_COOP_SKIN_ERROR_BUDGET_END')[0]
test=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
'''+helper+r'''
unsigned int bits(float f){unsigned int b;memcpy(&b,&f,4);return b;}
int main(){
 unsigned int a[8]={0},b[8]={0};float p=0,n=0;unsigned int ulp=0;
 assert(!R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&&!p&&!n&&!ulp);
 a[0]=0x40a0badf;b[0]=0x40a0bae1;a[3]=0x3f27aa98;b[3]=0x3f27aa9a;
 assert(!R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&&p<0.000001f&&n<0.000001f&&ulp==2);
 a[6]=1;assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&4);a[6]=0;
 a[0]=bits(1.0f);b[0]=bits(1.0001f);assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&1);
 b[0]=bits(1.00005f);assert(!R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp));
 a[3]=bits(1.0f);b[3]=bits(1.000002f);assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&2);
 b[3]=bits(1.0000005f);assert(!R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp));
 a[1]=0x80000000;b[1]=0;assert(!R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp));
 a[0]=0x7fc00000;assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&8);
 a[0]=0x7f800000;assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&8);
 a[0]=0xff7fffff;b[0]=0x7f7fffff;assert(R_STEFX_SkinErrorBudget(a,b,&p,&n,&ulp)&1);
 puts("Diagnostic skin error budget: observed tiny differences, position/normal limits, exact UVs, signed zero and non-finite rejection passed.");
}
'''
out=root/'build/research/letterbox_perf';cpp=out/'coop_skin_error_budget_test.cpp';exe=out/'coop_skin_error_budget_test.exe';cpp.write_text(test)
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc','/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'coop_skin_error_budget_test.obj'),'/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
subprocess.run([str(exe)],check=True)
