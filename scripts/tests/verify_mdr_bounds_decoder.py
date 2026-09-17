"""Compare the geometry auditor's decoder with the actual native decoder."""
from pathlib import Path
import subprocess
import sys
import numpy as np
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root/'scripts'))
from audit_mdr_bounds import decode_matrices

out = root/'build/research/letterbox_perf'
text = (root/'code/renderer/matcomp.c').read_text()
start = text.index('void MC_UnCompress(')
decoder = text[start:text.index('\n}', start)+2]
source = '#include <stdio.h>\n' + (root/'code/renderer/matcomp.h').read_text()
source += '\n#define MC_SCALE_VECT (1.0f/(float)((1<<(MC_BITS_VECT-1))-2))\n'
source += decoder + r'''
int main(int argc,char**argv){
 unsigned short raw[12];float matrix[3][4];
 FILE*input=fopen(argv[1],"rb");FILE*output=fopen(argv[2],"wb");
 if(!input||!output)return 2;
 while(fread(raw,sizeof(raw),1,input)==1){
  MC_UnCompress(matrix,(unsigned char*)raw);
  if(fwrite(matrix,sizeof(matrix),1,output)!=1)return 3;
 }
 fclose(input);fclose(output);return 0;
}
'''
cpp=out/'mdr_bounds_decoder.cpp';cpp.write_text(source)
exe=out/'mdr_bounds_decoder.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'mdr_bounds_decoder.obj'),
 '/Fe'+str(exe),'/link','/LIBPATH:'+str(vc/'lib'),
 '/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
raw=np.random.default_rng(1234).integers(0,65536,(1024,12),dtype=np.uint16)
raw[:3]=np.array([0,32768,65535],dtype=np.uint16)[:,None]
inputs=out/'mdr_bounds_decoder_input.bin';outputs=out/'mdr_bounds_decoder_output.bin'
inputs.write_bytes(raw.astype('<u2').tobytes())
subprocess.run([str(exe),str(inputs),str(outputs)],check=True)
expected=np.frombuffer(outputs.read_bytes(),'<f4').reshape(1024,3,4)
actual=decode_matrices(raw)
assert np.array_equal(expected,actual), float(np.abs(expected-actual).max())
result='PASS: 1024 matrices match the actual MC_UnCompress decoder exactly\n'
(out/'mdr_bounds_decoder.txt').write_text(result)
print(result)
