#ifndef STEFX_PVS_PROBE_BOUNDS_H
#define STEFX_PVS_PROBE_BOUNDS_H

#define STEFX_HM_PVS_PROBE_SURFACE_WORDS 110

// The diagnostic bitmap is intentionally bounded; it must not limit rendering.
inline int STEFX_PvsProbeSurfaceWord(int surfaceIndex)
{
	return surfaceIndex >= 0 && surfaceIndex < STEFX_HM_PVS_PROBE_SURFACE_WORDS * 32
		? surfaceIndex >> 5 : -1;
}

#endif
