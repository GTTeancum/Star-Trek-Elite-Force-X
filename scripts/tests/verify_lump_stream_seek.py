"""Exercise the actual map stream against partial reads and ZIP-like seeks."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'build/research/letterbox_perf'
text=(root/'code/qcommon/qcommon.h').read_text()
start=text.index('struct LumpStream')
body=text[start:text.index('\n};',start)+3]
source=r'''
#include <stdio.h>
#include <string.h>
#define MAX_QPATH 64
#define qfalse 0
#define qtrue 1
#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
typedef int qboolean;typedef int fileHandle_t;typedef unsigned char byte;
byte data[256];int cursor,resets,relative,failSeek;
void Com_sprintf(char*p,int n,const char*fmt,const char*a,const char*b){_snprintf(p,n,fmt,a,b);}
void Com_Printf(const char*,...){}
int FS_FOpenFileRead(const char*,fileHandle_t*f,int){*f=1;cursor=0;return 256;}
int FS_Seek(int,int offset,int origin){if(failSeek)return -1;
 if(origin==FS_SEEK_SET){++resets;cursor=offset;}else{++relative;cursor+=offset;}return cursor;}
int FS_Read(void*p,int n,int){if(n>7)n=7;memcpy(p,data+cursor,n);cursor+=n;return n;}
void FS_FCloseFile(int){}
''' + body + r'''
int main(){int failed=0;for(int i=0;i<256;++i)data[i]=(byte)i;
 LumpStream s;byte b[32];s.open("maps/test","verts");
 failed+=!s.readAt(20,b,16)||memcmp(b,data+20,16)||relative!=1||resets!=0;
 failed+=!s.readAt(36,b,8)||memcmp(b,data+36,8)||relative!=1||resets!=0;
 failed+=!s.readAt(80,b,20)||memcmp(b,data+80,20)||relative!=2||resets!=0;
 failed+=!s.readAt(5,b,10)||memcmp(b,data+5,10)||resets!=1;
 failed+=s.readAt(250,b,10)||s.readAt(-1,b,10)||s.readAt(10,b,-1);
 failSeek=1;failed+=s.readAt(100,b,5)||s.pos!=15;failSeek=0;
 failed+=!s.readAt(100,b,5)||memcmp(b,data+100,5);
 s.close();failed+=s.readAt(0,b,1)||s.forwardSeeks!=0;
 printf("Lump stream: 8 cases, failures=%d\n",failed);return failed;}
'''
cpp=out/'lump_stream_test.cpp';cpp.write_text(source);exe=out/'lump_stream_test.exe'
vc=Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
subprocess.run(['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe','/nologo','/MT','/EHsc',
 '/I'+str(vc/'include'),str(cpp),'/Fo'+str(out/'lump_stream_test.obj'),'/Fe'+str(exe),
 '/link','/LIBPATH:'+str(vc/'lib'),'/LIBPATH:'+str(vc/'PlatformSDK/Lib')],check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
(out/'lump_stream_test.txt').write_text(result.stdout);print(result.stdout)
