"""Compile the actual handle selectors and stress their concurrent ownership."""
from pathlib import Path
import subprocess
import re

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/research/audio_probe'

def function(path, name):
    text = (ROOT / path).read_text()
    start = text.index(name + '(') if name + '(' in text else text.index(name + '(void)')
    start = text.rfind('\n', 0, start) + 1
    # Both selectors use a column-zero closing brace; conditional branches
    # deliberately contain alternative opening braces before preprocessing.
    end = text.index('\n}', start) + 2
    return text[start:end]

fs = function('code/qcommon/files_common.cpp', 'FS_HandleForFile')
wf = function('code/win32/win_file_xbox.cpp', 'WF_GetFreeHandle')
source = r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#define _XBOX 1
#define MAX_FILE_HANDLES 32
#define WF_MAX_OPEN_FILES 32
#define qtrue 1
#define qfalse 0
#define ERR_DROP 1
typedef int fileHandle_t;
typedef int wfhandle_t;
struct Fs { LONG used; char name[4]; } fsh[32];
struct Wf { LONG m_bUsed; } wfTable[32];
Wf *s_FileTable = wfTable;
void Com_Printf(const char *, ...) {}
void Com_Error(int,const char *,...) { exit(3); }
''' + fs + '\n' + wf + r'''
volatile LONG owners[2][32], failures, operations;
HANDLE startEvent;
DWORD WINAPI worker(void *) {
 WaitForSingleObject(startEvent,INFINITE);
 for(int i=0;i<20000;i++) {
  int kind=i&1;
  int slot=kind?WF_GetFreeHandle():FS_HandleForFile();
  if(slot<0 || slot>=32) {InterlockedIncrement(&failures);continue;}
  Sleep(0); // Open yields before publishing its resource.
  if(InterlockedIncrement(&owners[kind][slot])!=1) InterlockedIncrement(&failures);
  volatile LONG *used=kind?&wfTable[slot].m_bUsed:&fsh[slot].used;
#ifdef NEGATIVE_CONTROL
  *used=1;
#else
  if(*used!=1) InterlockedIncrement(&failures);
#endif
  Sleep(0); // Close cleans state before releasing ownership.
  InterlockedDecrement(&owners[kind][slot]);
  InterlockedExchange((LONG *)used,0);
  InterlockedIncrement(&operations);
 }
 return 0;
}
int main() {
 HANDLE threads[8];startEvent=CreateEvent(0,TRUE,FALSE,0);
 for(int i=0;i<8;i++) threads[i]=CreateThread(0,0,worker,0,0,0);
 SetEvent(startEvent);WaitForMultipleObjects(8,threads,TRUE,INFINITE);
 printf("operations=%ld ownership_failures=%ld\n",operations,failures);
 return failures?1:0;
}
'''
OUT.mkdir(parents=True, exist_ok=True)
vc = Path('C:/Program Files (x86)/Microsoft Visual Studio 8/VC')
cpp = OUT / 'file_handle_reservation_test.cpp'
cpp.write_text(source)
exe = OUT / 'file_handle_reservation_test.exe'
args = ['C:/XDK_5558/XDK/xbox/bin/vc71/CL.Exe', '/nologo', '/MT', '/EHsc',
        '/I'+str(vc/'include'), '/I'+str(vc/'PlatformSDK/include'),
        str(cpp), '/Fo'+str(OUT/'file_handle_reservation_test.obj'), '/Fe'+str(exe),
        '/link', '/LIBPATH:'+str(vc/'lib'), '/LIBPATH:'+str(vc/'PlatformSDK/Lib'), 'kernel32.lib']
subprocess.run(args, check=True)
result = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
(OUT/'file_handle_reservation_test.txt').write_text(result.stdout)
print(result.stdout)
old = source.replace('InterlockedCompareExchange((LONG *)&fsh[i].used, qtrue, qfalse) == qfalse', '!fsh[i].used')
old = old.replace('InterlockedCompareExchange(&s_FileTable[i].m_bUsed, 1, 0) == 0', '!s_FileTable[i].m_bUsed')
assert old != source
cpp.write_text('#define NEGATIVE_CONTROL 1\n' + old)
subprocess.run(args, check=True)
negative = subprocess.run([str(exe)], capture_output=True, text=True)
assert negative.returncode == 1 and re.search(r'ownership_failures=[1-9]', negative.stdout), negative.stdout
print('Original selectors, expected failure: ' + negative.stdout)
with (OUT/'file_handle_reservation_test.txt').open('a') as report:
    report.write('Original selectors, expected failure: ' + negative.stdout)
# Leave the generated source/binary testing the production implementation.
cpp.write_text(source)
subprocess.run(args, check=True)
