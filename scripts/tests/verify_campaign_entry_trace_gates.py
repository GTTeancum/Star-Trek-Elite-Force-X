#!/usr/bin/env python3
"""Check that short-circuiting early cgame traces changes no retail log output.

Compile the actual source-time guards (with verbose 0 and 1) and the logger's
actual format filter. Each in-window tracing site is checked as a potential
site, not claimed to execute once per frame. No emulated FPS estimate.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / 'SP-Mod-Source-Code-master/cgame'


def function(source, name):
    start = source.index('static int ' + name + '(')
    return source[start:source.index('\n}', start) + 2]


def main():
    cases = [('cg_main', 'arg0', 4600, 2), ('cg_view', 'serverTime', 4600, 1),
             ('cg_draw', 'cg.time', 5000, 1), ('cg_syscalls', 'cg.time', 5000, 1)]
    source = (ROOT / 'code/win32/xb_log.cpp').read_text()
    header = (ROOT / 'code/win32/xb_log.h').read_text()
    assert re.search(r'#ifndef SP_XBOX_VERBOSE_RUNTIME_LOGS\s+#define SP_XBOX_VERBOSE_RUNTIME_LOGS 0', header)
    # It is safe to gate at the producer only while this is the logger policy.
    setters = re.findall(r'\bg_verboseLog\s*=(?!=)\s*([^;]+);', source)
    assert setters == ['0', 'SP_XBOX_VERBOSE_RUNTIME_LOGS ? 1 : 0'], setters
    expected = 'if (!g_verboseLog && !xbl_FormatMayBeCritical('
    for name in ('XBLog_Printf', 'XBLog_Write'):
        m = re.search(r'^void ' + name + r'\([^;{]*\)\s*\{(.*?)\n\}', source, re.M | re.S)
        assert m and expected in m[1]
    gates, messages = [], []
    for name, clock, upper, count in cases:
        cpp = (GAME / (name + '.cpp')).read_text(encoding='latin1')
        wanted = f'(SP_XBOX_VERBOSE_RUNTIME_LOGS && {clock} >= 3400 && {clock} <= {upper})'
        actual = re.findall(r'\([^;\n]*(?:>= 3400)[^;\n]*\)', cpp)
        assert actual == [wanted] * count, (name, actual)
        gates.append((wanted.replace(clock, 'ticks'), upper))
        msgs = re.findall(r'(?:CG_XBOX_ACTIVE_LOG|XBLog_Write|XBLF)\(\s*"'
                          r'((?:JA: CL_EARLY EF|STEFX: cgame predict state)[^"]*)"', cpp)
        assert msgs
        messages.extend(msgs)
    program = '''
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
static unsigned int scans;
static const char *countedFind(const char *text,const char *pattern) {
    ++scans;
    return std::strstr(text,pattern);
}
#define strstr countedFind
static int g_verboseLog;
'''
    for name in ('xbl_IsHighFrequencyDrawNoise', 'xbl_FormatMayBeCritical'):
        program += function(source, name) + '\n'
    program += 'static const char *const messages[]={' + ',\n'.join(map(json.dumps, messages)) + '};\n'
    program += '''int main() {
        // Instruments the retail format-filter function, not the cost in milliseconds.
        g_verboseLog=0;
        for (size_t i=0;i<sizeof(messages)/sizeof(messages[0]);++i)
            assert(!xbl_FormatMayBeCritical(messages[i]));
        assert(scans>=sizeof(messages)/sizeof(messages[0]));
        printf("verbose-build=%d: %u guarded sites; retail prefilter would reject all after %u substring tests\\n",
               SP_XBOX_VERBOSE_RUNTIME_LOGS,(unsigned)(sizeof(messages)/sizeof(messages[0])),scans);
        for (int ticks=0;ticks<=5001;++ticks) {
'''
    for gate, upper in gates:
        program += f'assert({gate} == (bool(SP_XBOX_VERBOSE_RUNTIME_LOGS) && ticks>=3400 && ticks<={upper}));\n'
    program += '}}\n'
    with tempfile.TemporaryDirectory(prefix='ef-trace-gates-') as tmp:
        cpp, exe = Path(tmp, 'check.cpp'), Path(tmp, 'check')
        cpp.write_text(program)
        for verbose in (0, 1):
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++98', '-O2',
                            '-DSP_XBOX_VERBOSE_RUNTIME_LOGS=' + str(verbose), str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
