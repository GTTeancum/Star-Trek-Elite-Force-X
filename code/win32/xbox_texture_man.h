
/*
 * UNPUBLISHED -- Rights  reserved  under  the  copyright  laws  of the 
 * United States.  Use  of a copyright notice is precautionary only and 
 * does not imply publication or disclosure.                            
 *                                                                      
 * THIS DOCUMENTATION CONTAINS CONFIDENTIAL AND PROPRIETARY INFORMATION 
 * OF    VICARIOUS   VISIONS,  INC.    ANY  DUPLICATION,  MODIFICATION, 
 * DISTRIBUTION, OR DISCLOSURE IS STRICTLY PROHIBITED WITHOUT THE PRIOR 
 * EXPRESS WRITTEN PERMISSION OF VICARIOUS VISIONS, INC.
 */

#ifndef __XBOX_TEXTURE_MAN_H__
#define __XBOX_TEXTURE_MAN_H__

#include "glw_win_dx8.h"
#include <xtl.h>


// Texture allocator that never frees anything, just grows until it's totally reset:
class StaticTextureAllocator
{
public:
	StaticTextureAllocator( void ) { }

	void Reserve( unsigned long size )
	{
		if( !base )
			base = (unsigned char *) D3D_AllocContiguousMemory( size, 0 );
		allocPoint = 0;
		poolSize = size;
		maxAlloc = 0;
	}

	void Initialize( unsigned long size )
	{
		Reserve( size );
	}

	// No bookkeeping necessary, texNum is unused:
	void *Allocate( unsigned long size, GLuint texNum )
	{
		if( size > poolSize || allocPoint > poolSize - size )
			throw "Static texture pool full";

		// Current location:
		void *retVal = base + allocPoint;

		// Advance, then round up:
		allocPoint += size;
		allocPoint = (allocPoint + 127) & ~127;

		if( allocPoint > maxAlloc )
			maxAlloc = allocPoint;

		return retVal;
	}

	void Reset( void )
	{
		// Just move our allocation marker back to the start:
		allocPoint = 0;
	}

	unsigned long Size( void ) const
	{
		return allocPoint;
	}

	unsigned long Capacity( void ) const
	{
		return poolSize;
	}

private:
	unsigned char	*base;
	unsigned long	allocPoint;
	unsigned long	poolSize;
	unsigned long	maxAlloc;
};

class SwappingTextureAllocator
{
private:
	struct allocatedTexture_t
	{
		GLuint			texNum;	// Index for textureXlat
		unsigned char	*data;	// Pointer to data
	};

	void _swapAllTexturesToDisk( void )
	{
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		++swapCount;
#endif
		for( int i = 0; i < numTextures; ++i )
		{
			GLuint texNum = allocatedTextures[i].texNum;

			// Get the TextureInfo *
			glwstate_t::texturexlat_t::iterator it =
				glw_state->textureXlat.find( texNum );
			glwstate_t::TextureInfo *info = &it->second;

			//If texture has already been written out, don't write it again.
			if(info->fileOffset != -1) {
				info->inMemory = false;
				continue;
			}

			// Stall until the GPU is done with it:
			info->mipmap->BlockUntilNotBusy();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
			++waitCount;
#endif

			//File pointer may have been moved by a read.  Set it to the
			//proper position.
			if(SetFilePointer(fileHandle, fileOffset, NULL, FILE_BEGIN) ==
					INVALID_SET_FILE_POINTER) {
				throw "Couldn't seek end of file";
			}

			// Write the old texture out to disk:
			DWORD bytesWritten = 0;
			if( !WriteFile( fileHandle, allocatedTextures[i].data,
						info->size, &bytesWritten, NULL ) ||
						(bytesWritten != info->size) )
				throw "Couldn't write file";
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
			bytesWrittenTotal += bytesWritten;
#endif

			// Mark texture as not in memory anymore:
			info->inMemory = false;

			//Set file offset so we can find the texture later.
			info->fileOffset = fileOffset;

			//Increment for the next file.
			fileOffset += info->size;
		}

		ResetMemory();
	}

	void ResetMemory( void )
	{
		allocPoint = 0;
		numTextures = 0;
	}

public:
	SwappingTextureAllocator( void ) { }

	void Reserve( unsigned long size )
	{
		if( !base )
			base = (unsigned char *) D3D_AllocContiguousMemory( size, 0 );
		poolSize = size;
		fileHandle = INVALID_HANDLE_VALUE;
		peakAlloc = 0;
		ResetMemory();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		ResetCounters();
#endif
	}

	void Initialize( unsigned long size )
	{
		Reserve( size );
		Reset();
	}

	// Allocate enough space for size - it's used for texture texNum:
	void *Allocate( unsigned long size, GLuint texNum )
	{
		if( size > poolSize )
			throw "Model texture exceeds pool capacity";
		// Check for fullness - make room
		if( allocPoint + size > poolSize )
		{
			Com_Printf( "Swapping all model textures to disk!\n" );
			_swapAllTexturesToDisk();
		}

		// Current location:
		void *retVal = base + allocPoint;

		// Advance, then round up:
		allocPoint += size;
		allocPoint = (allocPoint + 127) & ~127;

		if( allocPoint > peakAlloc ) peakAlloc = allocPoint;

		// Make a note of this texture:
		allocatedTextures[numTextures].data = (unsigned char *) retVal;
		allocatedTextures[numTextures].texNum = texNum;
		++numTextures;

		return retVal;
	}

	// This should only be called when it's already been determined that the
	// requested texture isn't in memory. This makes room (if necessary)
	// and then re-loads it, fixing up the TextureInfo that goes with it
	void Fetch( GLuint texNum )
	{
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		++fetchCount;
#endif
		glwstate_t::texturexlat_t::iterator i =
			glw_state->textureXlat.find( texNum );
		glwstate_t::TextureInfo *info = &i->second;

		unsigned char *newData = (unsigned char *) Allocate( info->size, texNum );

		//Seek to proper file position.
		if(SetFilePointer(fileHandle, info->fileOffset, NULL, FILE_BEGIN) ==
				INVALID_SET_FILE_POINTER) {
			throw "Couldn't seek for reading";
		}

		// Find the data that goes with this texture on disk:
		DWORD bytesRead = 0;
		if( !ReadFile( fileHandle, newData, info->size, &bytesRead, NULL ) ||
			(bytesRead != info->size) )
			throw "Couldn't read file";
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		bytesReadTotal += bytesRead;
#endif

		// I'm getting errors with locked textures here. How!? Try to make it work,
		// figure out what the fuck is wrong later: VVFIXME
		if( info->mipmap->IsBusy() )
		{
			info->mipmap->BlockUntilNotBusy();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
			++waitCount;
#endif
		}

		// Fix up the texture data and other info:
		info->mipmap->Data = 0;
		info->mipmap->Register( newData );
		info->inMemory = true;
	}

	void Reset( void )
	{
		ResetMemory();
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
		ResetCounters();
#endif

		if(fileHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(fileHandle);
		}

		fileHandle = CreateFile( "z:\\skintextures",
				GENERIC_WRITE | GENERIC_READ, 0,
				NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if( fileHandle == INVALID_HANDLE_VALUE )
			throw "Couldn't create texture swap file";

		fileOffset = 0;
	}

	unsigned long Peak( void ) const { return peakAlloc; }

	// This forces all textures in memory to disk, so that the next frame or
	// so will re-load whatever's needed. It's only useful in situations where
	// you know that you're going to cache-miss on a bunch of textures, then
	// run out of space, and have to re-load them again. (Good when going in
	// and out of the in-game UI) ?
	void Flush( void )
	{
		_swapAllTexturesToDisk();
	}

	// Just a little helper for the memory manager to see how much of the pool is used:
	unsigned long Size( void ) const
	{
		return allocPoint;
	}

	unsigned long Capacity( void ) const
	{
		return poolSize;
	}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned long SwapCount( void ) const { return swapCount; }
	unsigned long FetchCount( void ) const { return fetchCount; }
	unsigned long WaitCount( void ) const { return waitCount; }
	unsigned long BytesWritten( void ) const { return bytesWrittenTotal; }
	unsigned long BytesRead( void ) const { return bytesReadTotal; }
#endif

private:
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	void ResetCounters( void )
	{
		swapCount = 0;
		fetchCount = 0;
		waitCount = 0;
		bytesWrittenTotal = 0;
		bytesReadTotal = 0;
	}
#endif

	unsigned char	*base;
	unsigned long	poolSize;
	unsigned long	allocPoint;
	unsigned long	fileOffset;
	unsigned long	peakAlloc;
	HANDLE 			fileHandle;
#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	unsigned long	swapCount;
	unsigned long	fetchCount;
	unsigned long	waitCount;
	unsigned long	bytesWrittenTotal;
	unsigned long	bytesReadTotal;
#endif

	allocatedTexture_t	allocatedTextures[4096];
	int			numTextures;
};

// Global texture allocators:
extern StaticTextureAllocator	gStaticTextures;
extern SwappingTextureAllocator	gSkinTextures;

extern void BeginSkinTextures( void );
extern void EndSkinTextures( void );
extern void ReserveRetailTexturePoolsEarly( void );

#endif
