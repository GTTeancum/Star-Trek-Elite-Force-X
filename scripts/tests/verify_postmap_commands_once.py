"""Run the actual diagnostic command-file loop against case-insensitive paths."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=ROOT/'code/win32/win_main_console.cpp')
p.add_argument('--compiler', default='C:/Program Files/LLVM/bin/clang++.exe')
a = p.parse_args()
s = a.source.read_text(encoding='cp1252')
start = s.index('\tfor (postMapCommandPathIndex = 0;')
end = s.index('\n\tif (treatAsDirectMap)', start)
loop = s[start:end]
prefix = r'''
#include <cstdio>
#include <cstring>
#include <string>
#define XBLF(...) ((void)0)
static int opens, mode, g_SPXBDirectMapStatus;
static std::string queued;
static FILE *OpenFixture(const char *, const char *) {
    ++opens;
    if (mode == 2 || (mode == 1 && opens == 1)) return NULL;
    FILE *f = tmpfile();
    if (!f) return NULL;
    fputs("wait 240\nsave test_slot\n", f);
    rewind(f);
    return f;
}
#define fopen OpenFixture
static void Cbuf_AddText(const char *s) { queued += s; }
static void Run(bool builtInLaunchIntent) {
    const char *postMapCommandPaths[] = {"D:/commands", "d:/commands", NULL};
    int postMapCommandPathIndex;
'''
suffix = r'''
}
int main() {
    for (mode = 0; mode < 3; ++mode) {
        opens = 0; queued.clear(); Run(false);
        const char *expected = mode == 2 ? "" : "wait 240\nsave test_slot\n";
        if (queued != expected || opens != (mode == 0 ? 1 : 2)) {
            printf("FAIL: mode=%d opens=%d queued=%s\n", mode, opens, queued.c_str());
            return 1;
        }
    }
    opens = 0; queued.clear(); Run(true);
    if (opens || !queued.empty()) return 2;
    puts("PASS: one dispatch, lowercase fallback, missing file, normal launch exclusion");
}
'''
with tempfile.TemporaryDirectory(prefix='stefx_postmap_') as temp:
    d = Path(temp); cpp = d/'postmap.cpp'; exe = d/'postmap.exe'
    cpp.write_text(prefix + loop + suffix)
    subprocess.run([a.compiler, '-O1', str(cpp), '-o', str(exe)], check=True)
    raise SystemExit(subprocess.call([str(exe)]))
