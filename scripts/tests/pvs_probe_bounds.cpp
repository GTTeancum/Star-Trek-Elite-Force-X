#include "../../code/renderer/retail_xbox/pvs_probe_bounds.h"
#include <cassert>
#include <climits>

int main()
{
	assert(STEFX_PvsProbeSurfaceWord(-1) == -1);
	assert(STEFX_PvsProbeSurfaceWord(INT_MIN) == -1);
	assert(STEFX_PvsProbeSurfaceWord(0) == 0);
	assert(STEFX_PvsProbeSurfaceWord(31) == 0);
	assert(STEFX_PvsProbeSurfaceWord(32) == 1);
	assert(STEFX_PvsProbeSurfaceWord(3519) == 109);
	assert(STEFX_PvsProbeSurfaceWord(3520) == -1);
	assert(STEFX_PvsProbeSurfaceWord(3712) == -1); // observed overflowing word 116
	assert(STEFX_PvsProbeSurfaceWord(INT_MAX) == -1);

	struct Guarded { unsigned before; unsigned bitmap[110]; unsigned after; } data = {};
	data.before = 0x12345678;
	data.after = 0x87654321;
	for (int surface = 0; surface < 100000; ++surface)
	{
		int word = STEFX_PvsProbeSurfaceWord(surface);
		if (word >= 0) data.bitmap[word] |= 1u << (surface & 31);
	}
	assert(data.before == 0x12345678 && data.after == 0x87654321);
	for (int word = 0; word < 110; ++word) assert(data.bitmap[word] == UINT_MAX);
}
