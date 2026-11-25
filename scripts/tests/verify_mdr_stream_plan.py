#!/usr/bin/env python3
"""Exercise the actual Xbox stream-compaction function against v0.9.4 with host FS and heap substitutes.

Assets are optional. For the 14 upper/lower player models, pass the local Raven demo pak0.pk3.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
from tempfile import TemporaryDirectory
from zipfile import ZipFile

PREFIX = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <vector>
typedef unsigned char byte;
typedef float vec2_t[2], vec3_t[3];
#include "qfiles.h"
#include "mdr_frame_codec.h"

typedef int fileHandle_t;
enum qboolean { qfalse, qtrue };
enum { FS_READ, FS_SEEK_SET };
#define STEFX_HW_FRAME_DIAGNOSTICS 1
struct { int integer; } cvar;
static typeof(cvar) *Cvar_Get(const char *, const char *, int) { return &cvar; }
struct Result { int iAllocSize, iMdrFrameBaseCount, iMdrFrameSize, iMdrFrameCount;
    int iMdrFramePatchOffsetsOffset, iMdrFramePatchesOffset;
    void *pModelDiskImage;
    const byte *pMdrFrameBase;
    qboolean bHeapAllocated, bFileBuffer, bMdrCompacted, bMdrNeedsRegistration;
};
typedef Result CachedEndianedModelBinary_t;
static std::map<std::string, Result> cache;
static std::map<std::string, Result> *CachedModels = &cache;
static int allocations, deallocations, cursor, readBytes, readCalls, seekCalls, closes, failMode;
static bool secondPass, didFail;
static const std::vector<byte> *asset;
static int FS_FOpenFileByMode(const char *, fileHandle_t *file, int mode) {
    assert(mode == FS_READ); cursor = 0; *file = 1; return asset->size();
}
static int FS_Read(void *out, int count, fileHandle_t file) {
    assert(file == 1 && count >= 0 && cursor >= 0 && size_t(cursor + count) <= asset->size());
    ++readCalls;
    if (secondPass && failMode && !didFail) {
        didFail = true;
        if (failMode == 1) --count; // short second pass read
    }
    memcpy(out, &(*asset)[cursor], count);
    if (secondPass && failMode == 2 && seekCalls == 1 && cursor == sizeof(md4Header_t))
        memset(out, 0, count); // changed contents; first block's measured encoded length cannot match
    cursor += count; readBytes += count;
    return count;
}
static void FS_Seek(fileHandle_t file, int off, int mode) {
    assert(file == 1 && mode == FS_SEEK_SET); cursor = off; secondPass = true; ++seekCalls;
}
static void FS_FCloseFile(fileHandle_t file) { assert(file == 1); ++closes; }
static bool STEFX_IsMdrModelName(const char *name) { return strstr(name, ".mdr") != NULL; }
static bool STEFX_IsPlayerModelName(const char *name) { return strstr(name, "models/players/") != NULL; }
static bool STEFX_IsBorgPlayerModelName(const char *name) { return strstr(name, "/borg") != NULL; }
static byte *STEFX_AllocMdrStorage(int length, qboolean *heap) {
    *heap = qtrue; ++allocations; return (byte *)malloc(length);
}
static void STEFX_FreeMdrStorage(void *buf, qboolean heap) { assert(heap); ++deallocations; free(buf); }
static void XBLog_WriteCriticalf(const char *, ...) {}
static struct { int dictionary[4096]; } s_stefxMdrWork;
static void *s_stefxMdrBlockHeader;
static unsigned int s_stefxMdrBoundsKeys[16], s_tagBlockKeys[16];
'''.replace('struct { int integer; } cvar;', 'struct Cvar { int integer; } cvar;').replace('typeof(cvar)', 'Cvar')

SUFFIX = r'''
typedef qboolean (*Loader)(const char *, void **, qboolean *);
struct ReadSummary { int readBytes, readCalls, seekCalls, closes; };
struct Packed {
    Result record;
    ReadSummary reads;
    std::vector<byte> allocation;
    double millis;
};
static const char *kName = "models/players/hazard/lower.mdr";
static Packed Load(Loader loader, const std::vector<byte> &bytes, int transforms, int fault = 0) {
    cache.clear(); asset = &bytes; cvar.integer = transforms;
    secondPass = didFail = false; failMode = fault;
    cursor = allocations = deallocations = readCalls = readBytes = seekCalls = closes = 0;
    void *resident = NULL; qboolean cached = qtrue;
    const clock_t start = clock();
    const qboolean ok = loader(kName, &resident, &cached);
    const double millis = 1000. * (clock() - start) / CLOCKS_PER_SEC;
    Packed result;
    if (fault) {
        assert(!ok && !resident && cached && cache.empty() && closes == 1 && seekCalls == 1);
        assert(allocations == 1 && deallocations == 1 && didFail);
        return result;
    }
    assert(ok && resident && !cached && cache.size() == 1 && closes == 1 && seekCalls == 1);
    result.record = cache[kName];
    assert(result.record.pModelDiskImage == resident && result.record.pMdrFrameBase == resident);
    assert(result.record.bHeapAllocated && !result.record.bFileBuffer && result.record.bMdrCompacted && result.record.bMdrNeedsRegistration);
    const byte *const begin = (const byte *)resident;
    result.allocation.assign(begin, begin + result.record.iAllocSize);
    result.reads.readBytes = readBytes;
    result.reads.readCalls = readCalls;
    result.reads.seekCalls = seekCalls;
    result.reads.closes = closes;
    result.millis = millis;
    STEFX_FreeMdrStorage(resident, result.record.bHeapAllocated);
    assert(allocations == 1 && deallocations == 1);
    return result;
}
static void Verify(const Packed &a, const Packed &b, const std::vector<byte> &source) {
    assert(a.allocation == b.allocation);
    assert(a.reads.readBytes == b.reads.readBytes && a.reads.readCalls == b.reads.readCalls);
    const Result &m = b.record;
    assert(m.iAllocSize == a.record.iAllocSize && m.iMdrFrameBaseCount == -STEFX_MDR_BLOCK_FRAMES);
    assert(m.iMdrFrameBaseCount == a.record.iMdrFrameBaseCount && m.iMdrFrameSize == a.record.iMdrFrameSize);
    assert(m.iMdrFrameCount == a.record.iMdrFrameCount && m.iMdrFramePatchOffsetsOffset == a.record.iMdrFramePatchOffsetsOffset);
    assert(m.iMdrFramePatchesOffset == a.record.iMdrFramePatchesOffset);
    const int stride = m.iMdrFrameSize;
    const int frames = m.iMdrFrameCount;
    const size_t blocks = (frames + STEFX_MDR_BLOCK_FRAMES - 1) / STEFX_MDR_BLOCK_FRAMES;
    assert(m.iMdrFramePatchesOffset == m.iMdrFramePatchOffsetsOffset + int((blocks + 1) * sizeof(unsigned)));
    const byte *data = &b.allocation[0], *offsets = data + m.iMdrFramePatchOffsetsOffset;
    const byte *const packed = data + m.iMdrFramePatchesOffset;
    unsigned start = 0;
    assert(!memcmp(offsets, &start, sizeof(start)));
    byte decoded[sizeof(s_stefxMdrBlockRaw)], work[sizeof(s_stefxMdrBlockRaw)];
    for (size_t block = 0; block < blocks; ++block) {
        unsigned end;
        memcpy(&end, offsets + (block + 1) * sizeof(end), sizeof(end));
        assert(end > start && end <= b.allocation.size() - m.iMdrFramePatchesOffset);
        const int frame = block * STEFX_MDR_BLOCK_FRAMES;
        const int bytes = std::min(STEFX_MDR_BLOCK_FRAMES, frames - frame) * stride;
        assert(StefxMdrFrame::Decode(packed + start, end - start, decoded, bytes, stride, work, sizeof(work)));
        assert(!memcmp(decoded, &source[sizeof(md4Header_t) + frame * stride], bytes));
        start = end;
    }
    assert(int(start) == m.iAllocSize - m.iMdrFramePatchesOffset);
}
static std::vector<byte> ReadModel(const char *path) {
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); const int size = ftell(f); rewind(f);
    assert(size >= (int)sizeof(md4Header_t)); std::vector<byte> bytes(size);
    assert(fread(&bytes[0], 1, size, f) == size_t(size)); fclose(f);
    return bytes;
}
static std::vector<byte> Synthetic(const int frames) {
    const int bones = 41, stride = 40 + bones * 24;
    md4Header_t h = {};
    h.ident = MD4_IDENT; h.version = MD4_VERSION; h.numFrames = frames; h.numBones = bones;
    h.ofsFrames = -int(sizeof(h)); h.numLODs = 1; h.ofsLODs = sizeof(h) + frames * stride;
    assert(stride == STEFX_BORG_MDR_MAX_FRAME_BYTES);
    h.ofsTags = h.ofsEnd = h.ofsLODs + sizeof(md4LOD_t);
    std::vector<byte> bytes(h.ofsEnd);
    memcpy(&bytes[0], &h, sizeof(h));
    const md4LOD_t lod = { 0, sizeof(md4LOD_t), sizeof(md4LOD_t) };
    memcpy(&bytes[h.ofsLODs], &lod, sizeof(lod));
    unsigned seed = 0x5544306bu;
    for (int i = sizeof(h); i < h.ofsLODs; ++i) {
        seed = seed * 1664525u + 1013904223u;
        if (i % 128 < 96) bytes[i] = (byte)(seed >> 24);
    }
    return bytes;
}
int main(int argc, char **argv) {
    assert(sizeof(md4Header_t) == 104);
    const int runs = atoi(argv[1]);
    // A 1024-byte stride leaves 32 bytes: ten plans (30 bytes) fit, eleven (33) do not.
    for (int n = 160; n <= 161; ++n) {
        const std::vector<byte> source = Synthetic(n);
        for (int transforms = 0; transforms <= 1; ++transforms)
            Verify(Load(LegacyLoad, source, transforms), Load(STEFX_TryStreamMdrFile, source, transforms), source);
        Load(STEFX_TryStreamMdrFile, source, true, 1);
        if (n == 160) Load(STEFX_TryStreamMdrFile, source, true, 2);
    }
    puts("PASS exact- and over-capacity plan, both codec modes, short/changed second read and cache/heap ownership");
    std::vector<std::vector<byte> > models;
    for (int i = 2; i < argc; ++i) models.push_back(ReadModel(argv[i]));
    if (models.empty()) return 0;
    std::vector<double> oldTimes, newTimes, oldKlingon, newKlingon;
    size_t packedBytes = 0, totalReads = 0;
    for (int run = -1; run < runs; ++run) {
        double oldSum = 0., newSum = 0., oldK = 0., newK = 0.;
        for (size_t i = 0; i < models.size(); ++i) {
            const std::vector<byte> &input = models[i];
            Packed oldPack, newPack;
            if ((run + 2) % 2) { newPack = Load(STEFX_TryStreamMdrFile, input, 1); oldPack = Load(LegacyLoad, input, 1); }
            else { oldPack = Load(LegacyLoad, input, 1); newPack = Load(STEFX_TryStreamMdrFile, input, 1); }
            Verify(oldPack, newPack, input);
            if (run < 0) {
                packedBytes += newPack.allocation.size(); totalReads += newPack.reads.readBytes;
                Verify(Load(LegacyLoad, input, 0), Load(STEFX_TryStreamMdrFile, input, 0), input);
            }
            oldSum += oldPack.millis; newSum += newPack.millis;
            if (strstr(argv[i + 2], "klingon")) { oldK += oldPack.millis; newK += newPack.millis; }
        }
        if (run >= 0) { oldTimes.push_back(oldSum); newTimes.push_back(newSum); oldKlingon.push_back(oldK); newKlingon.push_back(newK); }
    }
    printf("PASS %lu real models; %lu full allocation bytes and %lu FS read bytes identical to v0.9.4; all frames roundtrip\n",
           (unsigned long)models.size(), (unsigned long)packedBytes, (unsigned long)totalReads);
    if (runs > 0) {
        std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
        std::sort(oldKlingon.begin(), oldKlingon.end()); std::sort(newKlingon.begin(), newKlingon.end());
        printf("Actual stream function, CPU time: %d alternating runs, median all %.2f -> %.2f ms; Klingon %.2f -> %.2f ms\n",
               runs, oldTimes[runs/2], newTimes[runs/2], oldKlingon[runs/2], newKlingon[runs/2]);
    }
}
'''


def loader(text):
    start = text.index('static qboolean STEFX_TryStreamMdrFile(const char *name, void **buffer, qboolean *cached)\n{')
    end = text.index('\n}', start) + 2
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--baseline', default='ee10ff1d95695ab784a2bbc5167f48900453171b',
                        help='v0.9.4 manifest source (default) or another git revision with that loader')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    repo = args.repo.absolute()
    renderer = repo / 'code/renderer'
    model = (renderer / 'tr_model.cpp').read_text()
    scratch = re.search(r'^#define STEFX_BORG_MDR_MAX_FRAME_BYTES 1024\n.*?^static byte s_stefxMdrBlockPacked\[.*?\];', model,
                        re.MULTILINE | re.DOTALL)[0]
    def original(path):
        return subprocess.check_output(['git', '-C', str(repo), 'show', f'{args.baseline}:code/renderer/{path}'], text=True)
    baseline_codec = original('mdr_frame_codec.h').replace('STEFX_MDR_FRAME_CODEC_H', 'TEST_BASELINE_CODEC').replace('namespace StefxMdrFrame', 'namespace LegacyMdrFrame')
    baseline_loader = loader(original('tr_model.cpp')).replace('STEFX_TryStreamMdrFile(', 'LegacyLoad(').replace('StefxMdrFrame::', 'LegacyMdrFrame::')
    with TemporaryDirectory(prefix='ef6-mdr-loader-') as tmp:
        tmp = Path(tmp)
        test = tmp / 'loader.cpp'
        test.write_text(PREFIX + scratch + '\n' + baseline_codec + '\n' + baseline_loader + '\n' + loader(model) + SUFFIX)
        exe = tmp / 'loader'
        flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie'] if args.sanitize else ['-O2']
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++98', '-Wall', '-Wextra', *flags, '-I', str(renderer), '-I', str(repo / 'code/qcommon'), str(test), '-o', str(exe)], check=True)
        assets = []
        if args.assets:
            with ZipFile(args.assets) as source:
                for name in sorted(source.namelist()):
                    if re.fullmatch(r'models/players/[^/]+/(lower|upper)\.mdr', name) and not name.startswith('models/players/borg'):
                        out = tmp / name.replace('/', '-')
                        out.write_bytes(source.read(name))
                        assets.append(out)
            assert len(assets) == 14, 'Expected the 14 real non-Borg upper/lower files in the Raven demo'
        subprocess.run([str(exe), '0' if args.sanitize else '5', *map(str, assets)], check=True)


if __name__ == '__main__':
    main()
