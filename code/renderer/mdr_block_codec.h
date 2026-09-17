#ifndef STEFX_MDR_BLOCK_CODEC_H
#define STEFX_MDR_BLOCK_CODEC_H

// Independent, allocation-free blocks. Tokens below 128 contain token+1
// literal bytes. Other tokens copy (token&127)+3 bytes from a preceding
// little-endian 16-bit distance. Overlapping copies are intentional.
namespace StefxMdrBlock
{
static int Encode(const unsigned char *src, int size, unsigned char *dst,
    int capacity, int *dictionary)
{
    int i, cursor = 0, written = 0, literal = 0;
    for (i = 0; i < 4096; ++i) dictionary[i] = -1;
    if (size <= 0 || size > 65535) return 0;
    while (cursor < size)
    {
        int match = -1, length = 0;
        if (cursor + 2 < size)
        {
            unsigned hash = ((unsigned)src[cursor] * 251u +
                (unsigned)src[cursor + 1] * 31u + src[cursor + 2]) & 4095u;
            match = dictionary[hash];
            dictionary[hash] = cursor;
            if (match >= 0 && cursor - match <= 65535)
                while (length < 130 && cursor + length < size &&
                    src[match + length] == src[cursor + length]) ++length;
        }
        if (length >= 4)
        {
            while (literal < cursor)
            {
                int count = cursor - literal;
                if (count > 128) count = 128;
                if (written + count + 1 > capacity) return 0;
                dst[written++] = (unsigned char)(count - 1);
                for (i = 0; i < count; ++i) dst[written++] = src[literal++];
            }
            if (written + 3 > capacity) return 0;
            int distance = cursor - match;
            dst[written++] = (unsigned char)(128 | (length - 3));
            dst[written++] = (unsigned char)distance;
            dst[written++] = (unsigned char)(distance >> 8);
            cursor += length;
            literal = cursor;
        }
        else ++cursor;
    }
    while (literal < size)
    {
        int count = size - literal;
        if (count > 128) count = 128;
        if (written + count + 1 > capacity) return 0;
        dst[written++] = (unsigned char)(count - 1);
        for (i = 0; i < count; ++i) dst[written++] = src[literal++];
    }
    return written;
}

static bool Decode(const unsigned char *src, int size, unsigned char *dst, int expected)
{
    int input = 0, output = 0;
    if (size <= 0 || expected <= 0) return false;
    while (input < size)
    {
        unsigned token = src[input++];
        int count = token < 128 ? (int)token + 1 : (int)(token & 127) + 3;
        if (count > expected - output) return false;
        if (token < 128)
        {
            if (count > size - input) return false;
            while (count--) dst[output++] = src[input++];
        }
        else
        {
            if (size - input < 2) return false;
            int distance = src[input] | ((int)src[input + 1] << 8);
            input += 2;
            if (distance <= 0 || distance > output) return false;
            while (count--) { dst[output] = dst[output - distance]; ++output; }
        }
    }
    return output == expected;
}
}
#endif
