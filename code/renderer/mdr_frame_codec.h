#ifndef STEFX_MDR_FRAME_CODEC_H
#define STEFX_MDR_FRAME_CODEC_H
#include "mdr_block_codec.h"
#include <string.h>

// Disk assets remain unchanged. Resident blocks: 0=raw, 1=original LZ,
// 2=XOR, 3=byte delta, 4=transpose, 5=byte delta/transpose,
// 6=word delta/transpose. Every transform preserves the original bytes.
namespace StefxMdrFrame {
static bool Layout(int bytes, int stride) {
    return stride > 0 && stride <= 1024 && !(stride & 1) &&
        bytes > 0 && bytes <= 16384 && bytes % stride == 0 && bytes / stride <= 16;
}

static void Transform(const unsigned char *src, unsigned char *work, int bytes, int stride, int mode) {
    const int frames = bytes / stride;
    for (int i = 0; i < bytes; ++i) {
        unsigned char value = src[i];
        if (i >= stride) {
            if (mode == 1) value ^= src[i - stride];
            if (mode == 2 || mode == 4) value = (unsigned char)(value - src[i - stride]);
            if (mode == 5) {
                const int word = i & ~1;
                const unsigned delta = (unsigned)(src[word] | src[word+1] << 8) -
                    (unsigned)(src[word-stride] | src[word-stride+1] << 8);
                value = (unsigned char)(delta >> ((i & 1) * 8));
            }
        }
        work[mode >= 3 ? (i % stride) * frames + i / stride : i] = value;
    }
}

// Work and candidate are separate caller-owned buffers of at least bytes.
// A null output performs the identical size pass without retaining the block.
static int Encode(const unsigned char *src, int bytes, int stride, bool transforms,
    unsigned char *output, int capacity, unsigned char *work,
    unsigned char *candidate, int scratchCapacity, int *dictionary) {
    if (!src || !work || !candidate || !dictionary || !Layout(bytes, stride) || scratchCapacity < bytes) return 0;
    int best = bytes + 1, bestType = 0;
    for (int mode = 0; mode < (transforms ? 6 : 1); ++mode) {
        const unsigned char *input = src;
        if (mode) { Transform(src, work, bytes, stride, mode); input = work; }
        int size = StefxMdrBlock::Encode(input, bytes, candidate, scratchCapacity, dictionary);
        if (size > 0 && size + 1 < best) { best = size + 1; bestType = mode + 1; }
    }
    if (!output) return best;
    if (capacity < best) return 0;
    output[0] = (unsigned char)bestType;
    if (!bestType) memcpy(output + 1, src, bytes);
    else {
        const unsigned char *input = src;
        if (bestType > 1) { Transform(src, work, bytes, stride, bestType - 1); input = work; }
        if (StefxMdrBlock::Encode(input, bytes, output + 1, capacity - 1, dictionary) != best - 1) return 0;
    }
    return best;
}

static bool Decode(const unsigned char *src, int size, unsigned char *dst,
    int bytes, int stride, unsigned char *work, int workCapacity) {
    if (!src || !dst || !work || size <= 0 || !Layout(bytes, stride) || workCapacity < bytes || src[0] > 6) return false;
    const int type = src[0];
    if (!type) {
        if (size != bytes + 1) return false;
        memcpy(dst, src + 1, bytes); return true;
    }
    if (!StefxMdrBlock::Decode(src + 1, size - 1, type >= 4 ? work : dst, bytes)) return false;
    if (type >= 4) {
        const int frames = bytes / stride;
        for (int i = 0; i < bytes; ++i) dst[i] = work[(i % stride) * frames + i / stride];
    }
    if (type == 2 || type == 3 || type == 5)
        for (int i = stride; i < bytes; ++i)
            dst[i] = type == 2 ? dst[i] ^ dst[i-stride] : (unsigned char)(dst[i] + dst[i-stride]);
    if (type == 6) for (int i = stride; i < bytes; i += 2) {
        unsigned value = (unsigned)(dst[i] | dst[i+1] << 8) + (unsigned)(dst[i-stride] | dst[i-stride+1] << 8);
        dst[i] = (unsigned char)value; dst[i+1] = (unsigned char)(value >> 8);
    }
    return true;
}
}
#endif
