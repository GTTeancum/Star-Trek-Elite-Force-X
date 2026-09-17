#include "../../code/renderer/mdr_block_codec.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <cassert>
#include <cstring>

static int verify(const unsigned char *data, int size)
{
    int dict[4096];
    std::vector<unsigned char> encoded(size * 2 + 32), decoded(size);
    int count = StefxMdrBlock::Encode(data, size, &encoded[0], (int)encoded.size(), dict);
    assert(count > 0);
    assert(StefxMdrBlock::Decode(&encoded[0], count, &decoded[0], size));
    assert(!memcmp(data, &decoded[0], size));
    assert(!StefxMdrBlock::Decode(&encoded[0], count - 1, &decoded[0], size));
    assert(!StefxMdrBlock::Decode(&encoded[0], count, &decoded[0], size - 1));
    return count < size ? count : size;
}

int main(int argc, char **argv)
{
    unsigned seed = 0x12345678;
    for (int size = 1; size <= 16384; size = size < 256 ? size + 1 : size * 2)
        for (int mode = 0; mode < 3; ++mode)
        {
            std::vector<unsigned char> data(size);
            for (int i = 0; i < size; ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                data[i] = mode == 0 ? 0 : mode == 1 ? i % 17 : seed >> 24;
            }
            verify(&data[0], size);
        }
    unsigned char bad[] = { 128, 0, 0 }, target[8];
    assert(!StefxMdrBlock::Decode(bad, 3, target, 3));
    bad[1] = 1;
    assert(!StefxMdrBlock::Decode(bad, 3, target, 3));
    for (int arg = 1; arg < argc; ++arg)
    {
        std::ifstream file(argv[arg], std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        assert(data.size() >= 104);
        int header[8]; memcpy(header, &data[72], sizeof(header));
        assert(header[2] < 0 && header[0] > 0);
        int bytes = header[4] + header[2], frameSize = bytes / header[0], total = 0;
        for (int frame = 0; frame < header[0]; frame += 16)
        {
            int count = header[0] - frame; if (count > 16) count = 16;
            total += 1 + verify(&data[-header[2] + frame * frameSize], count * frameSize);
        }
        total += ((header[0] + 15) / 16 + 1) * 4;
        std::cout << argv[arg] << " frame_bytes=" << bytes << " packed=" << total << " saved=" << bytes-total << "\n";
    }
    std::cout << "PASS: round trips, boundary sizes, overlap and malformed blocks\n";
}
