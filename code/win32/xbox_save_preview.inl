// Xbox save previews are optional. This file is included by the native renderer,
// and by a host test that supplies failure-injectable D3D, file and signature APIs.
#include "xbox_save_preview.h"

static const char s_largeSavePreviewPath[] = "Z:\\screenshot.xbx";
static const char s_iconSavePreviewPath[] = "Z:\\saveimage.xbx";
static bool s_largeSavePreviewReady;
static bool s_iconSavePreviewReady;

static bool XB_ClearPreviewScratch(const char *path)
{
	if (DeleteFile(path))
		return true;
	const DWORD error = GetLastError();
	return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static bool XB_FinishPreviewFile(const char *path, DWORD dataBytes, bool sign)
{
	const DWORD expected = CSS_IMAGE_HDR_SIZE + dataBytes;
	HANDLE file = CreateFile(path, sign ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
		0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD high = 0, done = 0;
	XPR_HEADER header;
	bool ok = GetFileSize(file, &high) == expected && high == 0 &&
		ReadFile(file, &header, sizeof(header), &done, NULL) && done == sizeof(header) &&
		header.dwMagic == XPR_MAGIC_VALUE && header.dwHeaderSize == CSS_IMAGE_HDR_SIZE &&
		header.dwTotalSize == expected;
	if (ok && sign)
	{
		// Keep the original signature over the entire XPR, without a 32 KiB
		// buffer on the pause-menu call stack.
		ok = SetFilePointer(file, 0, NULL, FILE_BEGIN) == 0;
		HANDLE hash = ok ? XCalculateSignatureBegin(XCALCSIG_FLAG_SAVE_GAME) : INVALID_HANDLE_VALUE;
		if (!hash || hash == INVALID_HANDLE_VALUE)
			ok = false;
		else
		{
			BYTE chunk[2048];
			XCALCSIG_SIGNATURE signature;
			for (DWORD left = expected; ok && left; )
			{
				const DWORD n = left > sizeof(chunk) ? sizeof(chunk) : left;
				ok = ReadFile(file, chunk, n, &done, NULL) && done == n &&
					XCalculateSignatureUpdate(hash, chunk, n) == ERROR_SUCCESS;
				left -= n;
			}
			// End releases the hash even if a read or update failed.
			const DWORD endStatus = XCalculateSignatureEnd(hash, &signature);
			ok = ok && endStatus == ERROR_SUCCESS &&
				WriteFile(file, &signature, sizeof(signature), &done, NULL) &&
				done == sizeof(signature);
		}
	}
	const BOOL closed = CloseHandle(file);
	return ok && closed;
}

static bool XB_WriteSavePreview(LPDIRECT3DDEVICE8 device, LPDIRECT3DSURFACE8 source,
	const RECT &crop, unsigned int width, unsigned int height, const char *path, bool sign)
{
	LPDIRECT3DSURFACE8 dest = NULL;
	HRESULT hr = device->CreateImageSurface(width, height, D3DFMT_DXT1, &dest);
	bool ok = SUCCEEDED(hr) && dest != NULL;
	if (ok)
		ok = SUCCEEDED(D3DXLoadSurfaceFromSurface(dest, NULL, NULL, source, NULL,
			&crop, D3DX_DEFAULT, D3DCOLOR(0)));
	if (ok)
		ok = SUCCEEDED(XGWriteSurfaceOrTextureToXPR(dest, path, TRUE));
	if (dest)
		dest->Release();
	if (ok)
		ok = XB_FinishPreviewFile(path, (width * height) / 2, sign);
	if (!ok)
		DeleteFile(path); // best effort; a failed capture is never marked ready
	return ok;
}

static void XB_CaptureSavePreviews(LPDIRECT3DDEVICE8 device, bool thirdPerson)
{
	// A previous save's scratch files may still exist, even from an earlier
	// launch. Do not publish either until its own current capture is complete.
	s_largeSavePreviewReady = s_iconSavePreviewReady = false;
	const bool largeCleared = XB_ClearPreviewScratch(s_largeSavePreviewPath);
	const bool iconCleared = XB_ClearPreviewScratch(s_iconSavePreviewPath);
	if (!device || (!largeCleared && !iconCleared))
		return;

	LPDIRECT3DSURFACE8 source = NULL;
	const HRESULT hr = device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &source);
	D3DSURFACE_DESC desc;
	if (SUCCEEDED(hr) && source && SUCCEEDED(source->GetDesc(&desc)) &&
		desc.Width > 0 && desc.Height > 0)
	{
		const int w = (int)desc.Width, h = (int)desc.Height;
		RECT crop;
		if (largeCleared)
		{
			crop.left = w * 3 / 40;
			crop.top = h * 3 / 40;
			crop.right = w - crop.left;
			crop.bottom = h - crop.top;
			s_largeSavePreviewReady = XB_WriteSavePreview(device, source, crop,
				SAVE_GAME_IMAGE_W, SAVE_GAME_IMAGE_H, s_largeSavePreviewPath, true);
		}
		if (iconCleared)
		{
			const int width = w < 240 ? w : 240, height = h < 240 ? h : 240;
			crop.left = (w - width) / 2;
			crop.top = (h - height) / 2 + (thirdPerson ? 60 : 0);
			if (crop.top > h - height)
				crop.top = h - height;
			crop.right = crop.left + width;
			crop.bottom = crop.top + height;
			s_iconSavePreviewReady = XB_WriteSavePreview(device, source, crop,
				64, 64, s_iconSavePreviewPath, false);
		}
	}
	if (source)
		source->Release();
}

static void XB_CopyOneSavePreview(bool ready, const char *scratch, const char *dest)
{
	if (ready && CopyFile(scratch, dest, FALSE))
		return;
	if (!CopyFile("d:\\base\\media\\defaultsaveimage.xbx", dest, FALSE))
	{
		// No packaged default: leave this slot without an optional preview,
		// not with another capture or a partial copy. Never touch the save file.
		DeleteFile(dest);
	}
}

void XB_CopyCurrentSavePreviews(const char *iconDestination, const char *largeDestination)
{
	XB_CopyOneSavePreview(s_iconSavePreviewReady, s_iconSavePreviewPath, iconDestination);
	XB_CopyOneSavePreview(s_largeSavePreviewReady, s_largeSavePreviewPath, largeDestination);
}
