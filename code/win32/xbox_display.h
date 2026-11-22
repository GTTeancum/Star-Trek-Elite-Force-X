#ifndef STEFX_XBOX_DISPLAY_H
#define STEFX_XBOX_DISPLAY_H

#include <xtl.h>

struct XboxDisplayMode
{
	DWORD videoFlags;
	int width;
	int height;
	bool widescreen;
	bool progressive;
};

inline XboxDisplayMode XboxGetDisplayMode(bool multiplayer = false)
{
	XboxDisplayMode mode;
	mode.videoFlags = XGetVideoFlags();
	const bool hdtv = XGetAVPack() == XC_AV_PACK_HDTV;
	const bool hd720 = hdtv && (mode.videoFlags & XC_VIDEO_FLAGS_HDTV_720p);
	mode.width = hd720 && !multiplayer ? 1280 : 640;
	mode.height = hd720 && !multiplayer ? 720 : 480;
	mode.widescreen = (hd720 && !multiplayer) || (mode.videoFlags & XC_VIDEO_FLAGS_WIDESCREEN);
	mode.progressive = (hd720 && !multiplayer) || (hdtv && (mode.videoFlags & XC_VIDEO_FLAGS_HDTV_480p));
	return mode;
}

#endif
