#!/usr/bin/env python3
"""Exercise actual Xbox log write/flush decisions at the first campaign scene.
The NT/Win32 disk handles are counters, not a simulation of drive latency.
"""
import argparse
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    m = re.search(r"^(?:static )?(?:void|int) " + name + r"\([^;{]*?\)\s*\{", source, re.M)
    if not m:
        raise ValueError(name)
    return source[m.start():source.index("\n}", m.end())+2]


PRE = r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
typedef unsigned int DWORD;
typedef int HANDLE;
struct XBL_IOSB {int ignored;};
const int INVALID_HANDLE_VALUE = -1;
static unsigned int g_fileLogFlushBytes, g_fileLogFlushWrites, g_fileLogBytes;
static int g_verboseLog, g_fileLogCapNotified, g_memoryRingOnly, g_debugStringMirror;
static unsigned int g_SPXBConsoleQualification;
static int g_hLogFile=1,g_hMirrorLogFile=INVALID_HANDLE_VALUE,g_logIsNt;
static std::vector<std::string> written,mirrored;
static unsigned int flushes;
static void OutputDebugStringA(const char*) {}
static void WriteFile(HANDLE h,const char* msg,DWORD n,DWORD*,void*) {assert(h==1);written.push_back(std::string(msg,n));}
static void NtWriteFile(HANDLE h,void*,void*,void*,XBL_IOSB*,void* msg,DWORD n,void*) {assert(h==1);written.push_back(std::string((char *)msg,n));}
static void xbl_MirrorWrite(const char *msg,DWORD n) {mirrored.push_back(std::string(msg,n));}
static void xbl_FlushHandle(HANDLE h,int isNt) {assert(h==1 && isNt==g_logIsNt);++flushes;}
"""

POST = r"""
static void reset() {
    flushes=g_fileLogFlushBytes=g_fileLogFlushWrites=g_fileLogBytes=g_fileLogCapNotified=0;
    written.clear(); mirrored.clear();
}
int main() {
    for (int nt=0;nt<2;++nt) for (int qualification=0;qualification<2;++qualification)
    for (int verbose=0;verbose<2;++verbose) for (int ring=0;ring<2;++ring) {
        reset(); g_logIsNt=nt;g_verboseLog=verbose;g_memoryRingOnly=ring;g_SPXBConsoleQualification=qualification;
        std::vector<std::string> expected;
        for (int i=0;i<9;++i) {expected.push_back(markers[i]); XBLog_Print(markers[i]);}
        bool batch=(ELIGIBLE && !qualification && !verbose);
        assert(mirrored==expected);
        assert(ring ? written.empty() : written==expected);
        assert(flushes==(ring ? 0u : batch ? 3u : 9u));
        if (!qualification && !verbose && !ring && !nt) printf("Full first-scene trace: %u writes, %u forced flushes\n",(unsigned)written.size(),flushes);
    }
    g_memoryRingOnly=g_verboseLog=g_SPXBConsoleQualification=0;
    reset(); g_fileLogBytes=XBL_FILE_SOFT_CAP_BYTES+1;
    for (int i=0;i<9;++i) XBLog_Print(markers[i]);
    assert(written==mirrored && written.size()==9 && !g_fileLogCapNotified);
    const char *critical[]={"STEFX_HW_CHECKPOINT save stage=list.begin\n", "STEFX_HW_CHECKPOINT: first frame render complete\n",
        "STEFX_HW_CHECKPOINT: first frame snapshots complete snap=0 ents=1 ERROR\n",
        "STEFX_HW_CHECKPOINT: first frame snapshots complete snap=garbage ents=3\n",
        "STEFX_HW_CHECKPOINT: first frame snapshots complete snap=0x0 ents= wrong field\n",
        "STEFX_HW_CHECKPOINT: first frame snapshots complete snap=1 ents=2 other work\n",
        "STEFX_HW_CHECKPOINT: first frame prediction complete -- failed\n",
        "STEFX_HW_CHECKPOINT: first frame unexpected new phase\n","FATAL in game\n","FRAME_HEARTBEAT\n"};
    unsigned int n=flushes;
    for (unsigned int i=0;i<sizeof(critical)/sizeof(*critical);++i) {
        XBLog_Print(critical[i]); assert(flushes==++n);
    }
    // Interior-only phases still reach the normal bounded flush budget.
    reset(); for (int i=0;i<XBL_FLUSH_EVERY_WRITES;++i) XBLog_Print(markers[2]);
    assert(flushes==(ELIGIBLE ? 1u : (unsigned)XBL_FLUSH_EVERY_WRITES));
}
"""


def main():
    args = argparse.ArgumentParser()
    args.add_argument('--baseline', nargs='?', const='ee10ff1d95695ab784a2bbc5167f48900453171b',
                      help='use original logger as a negative control (default: v0.9.4 release-manifest source)')
    args.add_argument('--sanitize', action='store_true')
    options = args.parse_args()
    src = 'code/win32/xb_log.cpp'
    source = ((ROOT/src).read_text() if not options.baseline else
              subprocess.check_output(['git','show',options.baseline+':'+src],cwd=ROOT,text=True))
    game = (ROOT/'SP-Mod-Source-Code-master/cgame/cg_main.cpp').read_text(encoding='latin1')
    view = (ROOT/'SP-Mod-Source-Code-master/cgame/cg_view.cpp').read_text(encoding='latin1')
    # Verify the stage producers, their spelling and both enclosing boundaries.
    assert re.search(r'case CG_INIT:\s*#ifdef _XBOX\s*s_stefxFirstActiveFramePending = qtrue;',game)
    assert 'const qboolean stefxHardwareFirstFrame = ( cg.clientFrame == 0 );' in view
    pattern = r'XBLog_WriteCriticalf?\(\s*"(STEFX_HW_CHECKPOINT: first (?:active )?frame [^"]+)"'
    boundary, inner = re.findall(pattern,game), re.findall(pattern,view)
    assert len(boundary)==2 and len(inner)==7
    assert boundary[0].startswith('STEFX_HW_CHECKPOINT: first active frame begin ') and 'first active frame complete ' in boundary[1]
    msgs = [boundary[0]]+inner+[boundary[1]]
    records = ',\n'.join('"'+msg.replace('%p','01234abc').replace('%d','1')+'\\n"' for msg in msgs)
    program = PRE + 'static const char *markers[9]={'+records+'};\n'
    program += '\n'.join(re.findall(r'^#define XBL_(?:FILE_SOFT_CAP_BYTES|FLUSH_EVERY_WRITES|FLUSH_EVERY_BYTES)\s+[^\n]+',source,re.M)) + '\n'
    program += function(source,'xbl_starts_with') + '\n' + function(source,'xbl_IsCriticalLogLine') + '\n'
    if not options.baseline:
        program += function(source,'xbl_IsBatchableFirstFrameStep')+'\n'
    for name in ('xbl_ShouldFlushWrite','xbl_ShouldWriteFileLine','XBLog_Print'):
        program += function(source,name)+'\n'
    with tempfile.TemporaryDirectory(prefix='ef-entry-log-') as tmp:
        cpp,exe = Path(tmp,'check.cpp'),Path(tmp,'check')
        cpp.write_text(program+POST)
        for defines,allowed in (([],True),(['-DSTEFX_SP_HOSTED_MP'],False),(['-DSTEFX_HW_FRAME_DIAGNOSTICS'],False)):
            flags=(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if options.sanitize else [])
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++98','-O2','-D_XBOX','-DSTEFX_ELITE_FORCE_SP',
                            '-DELIGIBLE='+str(int(allowed and not options.baseline)),*defines,*flags,str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
    print('PASS: source stage set; NT/Win32 paths, ring, qualification, verbose, caps, faults and durability boundaries')

if __name__=='__main__':
    main()
