#include "../../../../code/renderer/mdr_block_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv) {
    int dictionary[4096];
    for (int arg = 1; arg < argc; ++arg) {
        FILE *file = fopen(argv[arg], "rb"); assert(file);
        fseek(file, 0, SEEK_END); int length = ftell(file); rewind(file);
        unsigned char *source = (unsigned char *)malloc(length); assert(source);
        assert(fread(source, 1, length, file) == (size_t)length); fclose(file);
        assert(length >= 104 && !memcmp(source, "RDM5", 4));
        int header[8]; memcpy(header, source + 72, 32);
        int frames = header[0], stride = 40 + header[1] * 24;
        assert(frames > 0 && stride > 40 && stride * 16 <= 65535);
        assert(header[2] == -104 && header[4] == 104 + frames * stride && header[7] == length);
        unsigned char raw[65536], packed[131104], decoded[65536], scratch[65536];
        int totals[6] = {0,0,0,0,0,0}, bestTotal = 0, winners[6] = {0,0,0,0,0,0};
        for (int first = 0; first < frames; first += 16) {
            int count = frames - first; if (count > 16) count = 16;
            int bytes = count * stride, best = bytes + 1, winner = 0;
            const unsigned char *original = source + 104 + first * stride;
            for (int mode = 0; mode < 6; ++mode) {
                memcpy(raw, original, bytes);
                if (mode == 1 || mode == 2 || mode == 4) for (int i = bytes - 1; i >= stride; --i)
                    raw[i] = mode == 1 ? raw[i] ^ raw[i - stride] : (unsigned char)(raw[i] - raw[i - stride]);
                if (mode == 5) for (int i = bytes - 2; i >= stride; i -= 2) {
                    unsigned delta = (unsigned)(raw[i] | raw[i+1] << 8) - (unsigned)(raw[i-stride] | raw[i-stride+1] << 8);
                    raw[i] = (unsigned char)delta; raw[i+1] = (unsigned char)(delta >> 8);
                }
                if (mode >= 3) {
                    for (int b = 0; b < stride; ++b) for (int f = 0; f < count; ++f)
                        scratch[b * count + f] = raw[f * stride + b];
                    memcpy(raw, scratch, bytes);
                }
                int encoded = StefxMdrBlock::Encode(raw, bytes, packed, sizeof(packed), dictionary);
                assert(encoded > 0);
                assert(StefxMdrBlock::Decode(packed, encoded, decoded, bytes));
                if (mode >= 3) {
                    for (int b = 0; b < stride; ++b) for (int f = 0; f < count; ++f)
                        scratch[f * stride + b] = decoded[b * count + f];
                    memcpy(decoded, scratch, bytes);
                }
                if (mode == 1 || mode == 2 || mode == 4) for (int i = stride; i < bytes; ++i)
                    decoded[i] = mode == 1 ? decoded[i] ^ decoded[i - stride] : (unsigned char)(decoded[i] + decoded[i - stride]);
                if (mode == 5) for (int i = stride; i < bytes; i += 2) {
                    unsigned value = (unsigned)(decoded[i] | decoded[i+1] << 8) + (unsigned)(decoded[i-stride] | decoded[i-stride+1] << 8);
                    decoded[i] = (unsigned char)value; decoded[i+1] = (unsigned char)(value >> 8);
                }
                assert(!memcmp(decoded, original, bytes));
                int stored = 1 + (encoded < bytes ? encoded : bytes);
                totals[mode] += stored;
                if (stored < best) { best = stored; winner = mode; }
            }
            bestTotal += best; ++winners[winner];
        }
        printf("%s", argv[arg]);
        for (int mode = 0; mode < 6; ++mode) printf("\t%d", totals[mode]);
        printf("\t%d", bestTotal);
        for (int mode = 0; mode < 6; ++mode) printf("\t%d", winners[mode]);
        printf("\n");
        free(source);
    }
    return 0;
}
