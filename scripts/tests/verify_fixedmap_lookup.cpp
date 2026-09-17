#include <cassert>
#include <cstdio>
#include "../../code/qcommon/fixedmap.h"
int main() {
    VVFixedMap<int, unsigned int> empty(0);
    assert(empty.Find(0) == NULL);
    VVFixedMap<int, unsigned int> one(1);
    assert(one.Find(7) == NULL);
    assert(one.Insert(42, 7));
    one.Sort();
    assert(one.Find(0) == NULL);
    assert(one.Find(8) == NULL);
    assert(one.Find(7) && *one.Find(7) == 42);
    VVFixedMap<int, unsigned int> many(128);
    assert(many.Find(0) == NULL);
    for (int i = 127; i >= 0; --i) assert(many.Insert(i * 3, i * 2 + 2));
    many.Sort();
    for (unsigned int key = 0; key < 260; ++key) {
        int *value = many.Find(key);
        if (key >= 2 && key <= 256 && key % 2 == 0)
            assert(value && *value == (int)(key / 2 - 1) * 3);
        else assert(value == NULL);
    }
    puts("PASS: empty, singleton, boundary, gaps, and 128 stored keys");
}
