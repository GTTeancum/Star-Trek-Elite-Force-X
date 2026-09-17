"""Compile the actual model-release routine against allocator ownership guards."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=ROOT/'code/renderer/tr_model.cpp')
p.add_argument('--compiler', default=r'C:/Program Files/LLVM/bin/clang++.exe')
a = p.parse_args()
s = a.source.read_text(encoding='cp1252')
start = s.index('static void RE_RegisterModels_FreeDiskImage(')
end = s.index('\n#endif', s.index('\n}', start))
function = s[start:end]
prefix = r"""
#include <cassert>
#include <cstdio>
#include <cstdlib>
using qboolean = bool;
const bool qfalse = false;
struct CachedEndianedModelBinary_t {
    void *pModelDiskImage;
    bool bHeapAllocated;
    bool bFileBuffer;
};
int heapFrees, fileFrees, zoneFrees;
void *expected;
int mode;
void *GetProcessHeap() { return 0; }
void HeapFree(void *, int, void *p) {
    assert(mode == 1 && p == expected); ++heapFrees;
}
bool FS_STEFX_FreeHeapFileBuffer(void *p) {
    // The file allocator must never probe an ordinary heap/zone block.
    assert(mode == 2 && p == expected); ++fileFrees; return true;
}
void Z_Free(void *p) { assert(mode == 3 && p == expected); ++zoneFrees; }
"""
suffix = r"""
int main() {
    int token;
    expected = &token;
    for (mode = 1; mode <= 3; ++mode) {
        CachedEndianedModelBinary_t model = { expected, mode != 3, mode == 2 };
        RE_RegisterModels_FreeDiskImage(model);
        assert(!model.pModelDiskImage && !model.bHeapAllocated && !model.bFileBuffer);
        RE_RegisterModels_FreeDiskImage(model); // no duplicate release
    }
    assert(heapFrees == 1 && fileFrees == 1 && zoneFrees == 1);
    puts("PASS: plain heap, adopted file buffer, zone, and repeated cleanup ownership");
}
"""
with tempfile.TemporaryDirectory(prefix='stefx_model_owner_') as d:
    d = Path(d); cpp = d/'ownership.cpp'; exe = d/'ownership.exe'
    cpp.write_text(prefix + function + suffix)
    subprocess.run([a.compiler, '-O1', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
