#include "../../code/renderer/mdr_frame_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char work[16384], candidate[16416], output[32768], decoded[16384], source[16384];
static int dictionary[4096];
static int Check(const unsigned char *data, int bytes, int stride) {
    int legacy = StefxMdrFrame::Encode(data, bytes, stride, false, 0, 0, work, candidate, sizeof(candidate), dictionary);
    int count = StefxMdrFrame::Encode(data, bytes, stride, true, 0, 0, work, candidate, sizeof(candidate), dictionary);
    assert(count > 0 && count <= legacy);
    assert(StefxMdrFrame::Encode(data, bytes, stride, true, output, count, work, candidate, sizeof(candidate), dictionary) == count);
    assert(StefxMdrFrame::Decode(output, count, decoded, bytes, stride, work, sizeof(work)));
    assert(!memcmp(data, decoded, bytes));
    assert(!StefxMdrFrame::Decode(output, count-1, decoded, bytes, stride, work, sizeof(work)));
    output[0] = 0xa5;
    assert(!StefxMdrFrame::Encode(data, bytes, stride, true, output, count-1, work, candidate, sizeof(candidate), dictionary));
    assert(output[0] == 0xa5);
    // Exercise every decoder type, including types not selected by this input.
    for (int mode = 0; mode < 6; ++mode) {
        StefxMdrFrame::Transform(data, work, bytes, stride, mode);
        int packed = StefxMdrBlock::Encode(work, bytes, output+1, sizeof(output)-1, dictionary);
        assert(packed > 0); output[0] = (unsigned char)(mode+1);
        assert(StefxMdrFrame::Decode(output, packed+1, decoded, bytes, stride, candidate, sizeof(candidate)));
        assert(!memcmp(data, decoded, bytes));
    }
    output[0] = 0; memcpy(output+1, data, bytes);
    assert(StefxMdrFrame::Decode(output, bytes+1, decoded, bytes, stride, work, sizeof(work)));
    output[0] = 7;
    assert(!StefxMdrFrame::Decode(output, bytes+1, decoded, bytes, stride, work, sizeof(work)));
    return count;
}
int main(int argc, char **argv) {
    unsigned seed = 1;
    for (int stride = 2; stride <= 1024; stride *= 2) for (int frames = 1; frames <= 16; ++frames) {
        for (int i = 0; i < stride*frames; ++i) { seed = seed*1664525u+1013904223u; source[i] = (unsigned char)(seed>>24); }
        Check(source, stride*frames, stride);
        memset(source, 0, stride*frames); Check(source, stride*frames, stride);
    }
    assert(!StefxMdrFrame::Layout(0, 2)); assert(!StefxMdrFrame::Layout(3, 2));
    assert(!StefxMdrFrame::Layout(17*2, 2)); assert(!StefxMdrFrame::Layout(1026, 1026));
    for (int arg = 1; arg < argc; ++arg) {
        FILE *f = fopen(argv[arg], "rb"); assert(f);
        fseek(f,0,SEEK_END); int length = ftell(f); rewind(f);
        unsigned char *data = (unsigned char *)malloc(length); assert(data);
        assert(fread(data,1,length,f) == (size_t)length); fclose(f);
        assert(length >= 104 && !memcmp(data,"RDM5",4));
        int h[8]; memcpy(h,data+72,32); int stride = 40+h[1]*24;
        assert(h[2] == -104 && h[4] == 104+h[0]*stride && h[7] == length);
        if (stride > 1024) { printf("SKIP existing loader limit: %s\n",argv[arg]); free(data); continue; }
        int total = 0;
        for (int frame=0; frame<h[0]; frame+=16) {
            int frames=h[0]-frame; if(frames>16) frames=16;
            total += Check(data+104+frame*stride,frames*stride,stride);
        }
        printf("PASS %s packed=%d\n",argv[arg],total); free(data);
    }
    puts("PASS boundaries, all seven types, exact-size writes, truncated/invalid blocks and byte-exact frames");
    return 0;
}
