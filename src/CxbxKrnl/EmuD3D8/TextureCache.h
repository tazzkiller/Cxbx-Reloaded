// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->EmuD3D8->TextureCache.h
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx and Cxbe are free software; you can redistribute them
// *  and/or modify them under the terms of the GNU General Public
// *  License as published by the Free Software Foundation; either
// *  version 2 of the license, or (at your option) any later version.
// *
// *  This program is distributed in the hope that it will be useful,
// *  but WITHOUT ANY WARRANTY; without even the implied warranty of
// *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// *  GNU General Public License for more details.
// *
// *  You should have recieved a copy of the GNU General Public License
// *  along with this program; see the file COPYING.
// *  If not, write to the Free Software Foundation, Inc.,
// *  59 Temple Place - Suite 330, Bostom, MA 02111-1307, USA.
// *
// *  (c) 2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#pragma once

#include "xxhash32.h"
#include "EmuXTL.h"

#define MAX_CACHE_SIZE_TEXTURES 256

// TODO : Split TextureCache into a abstract base class (for all caches) and a specialization for textures
class TextureCache {
public:
	// Since the same memory address can be used for multiple textures,
	// this struct construct a key out of all identifiying values.
	// This applies to X_D3DCOMMON_TYPE_TEXTURE and X_D3DCOMMON_TYPE_SURFACE.
	struct TextureResourceKey {
		xbaddr TextureData;
		DWORD Format; // See X_D3DFORMAT_* masks and flags
		DWORD Size; // See X_D3DSIZE_* masks

		inline bool operator< (const struct TextureResourceKey& other) const
		{
			// std::tuple's lexicographic ordering does all the actual work for you
			// and using std::tie means no actual copies are made
			return std::tie(TextureData, Format, Size) < std::tie(other.TextureData, other.Format, other.Size);
		}
	};

	struct TextureCacheEntry {
		uint32_t PaletteHash = 0;
		uint32_t Hash = 0;
		uint32_t XboxDataSize = 0;
		DWORD XboxDataSamples[16] = {}; // Read sample indices using https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs
		XTL::IDirect3DBaseTexture8* pConvertedHostTexture = nullptr;

#if 0
		~TextureCacheEntry()
		{
			if (pConvertedHostTexture != nullptr) {
				pConvertedHostTexture->Release();
				pConvertedHostTexture = nullptr;
			}
		}
#endif
	};

private:
	std::mutex m_Mutex;
	std::map<struct TextureResourceKey, struct TextureCacheEntry> g_TextureCacheEntries;

	void Evict() // this must be called locked
	{
		// Poor-mans cache-eviction : Clear when full.
		if (g_TextureCacheEntries.size() >= MAX_CACHE_SIZE_TEXTURES) {
			DbgPrintf("Texture cache full - clearing for entire repopulation");
			for (auto it = g_TextureCacheEntries.begin(); it != g_TextureCacheEntries.end(); ++it) {
				auto pHostTexture = it->second.pConvertedHostTexture;
				if (pHostTexture != nullptr) {
					pHostTexture->Release(); // avoid memory leaks
				}
			}

			g_TextureCacheEntries.clear();
		}
	}

public:
	TextureCache() {}
	~TextureCache() { Clear(); }

	void Clear()
	{
		m_Mutex.lock();
		g_TextureCacheEntries.clear();
		m_Mutex.unlock();
	}

	const uint32_t GetPolynomial(const int32_t aRange)
	{
		static uint32_t Polynomials[] = {
			0x00000000,
			0x00000001, 0x00000003, 0x00000006, 0x0000000C,
			0x00000014, 0x00000030, 0x00000060, 0x000000B8,
			0x00000110, 0x00000240, 0x00000500, 0x00000CA0,
			0x00001B00, 0x00003500, 0x00006000, 0x0000B400,
			0x00012000, 0x00020400, 0x00072000, 0x00090000,
			0x00140000, 0x00300000, 0x00400000, 0x00D80000,
			0x01200000, 0x03880000, 0x07200000, 0x09000000,
			0x14000000, 0x32800000, 0x48000000, 0xA3000000
		};
		DWORD bsr; _BitScanReverse(&bsr, aRange - 1); // MSVC intrinsic; GCC has __builtin_clz
		return Polynomials[bsr];
	}

	struct TextureCacheEntry &Find(const struct TextureResourceKey textureKey, const DWORD *pPalette)
	{
		m_Mutex.lock();
		Evict();
		// Reference the converted texture (when the entry is not present, it's added) :
		struct TextureCacheEntry &cacheEntry = g_TextureCacheEntries[textureKey];

		uint32_t PaletteHash = textureKey.Format ^ textureKey.Size; // seed with characteristics
		if (pPalette != NULL)
			PaletteHash = XXHash32::hash((void *)pPalette, (uint64_t)256 * sizeof(XTL::D3DCOLOR), PaletteHash);

		uint32_t XboxDataSize = g_MemoryManager.QueryAllocationSize((void*)textureKey.TextureData);

		// Check if the data needs an updated conversion or not, first via a few quick checks
		if (cacheEntry.pConvertedHostTexture != nullptr) {
			boolean CanReuseCachedTexture = true;			
			if (cacheEntry.XboxDataSize != XboxDataSize)
				CanReuseCachedTexture = false;

			if (cacheEntry.PaletteHash != PaletteHash)
				CanReuseCachedTexture = false;

			uint32_t lfsr = 0;
			uint32_t magic = GetPolynomial(XboxDataSize);
			for (int i = 0; i < 16; i++) {
				// Read sample indices using https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs
				do {
					if (lfsr & 1)
						lfsr >>= 1;
					else
						lfsr = (lfsr >> 1) ^ magic;
				} while (lfsr >= XboxDataSize);

				DWORD Sample = ((DWORD*)textureKey.TextureData)[lfsr];
				if (cacheEntry.XboxDataSamples[i] != Sample)
					CanReuseCachedTexture = false;

				cacheEntry.XboxDataSamples[i] = Sample; // Always set (no harm done if reuse stays possible, but avoids another loop if not)
			}

			if (CanReuseCachedTexture) {
				m_Mutex.unlock();
				return cacheEntry;
			}
		}

		// TODO : Don't hash every time (see CxbxVertexBufferConverter::ApplyCachedStream)
		uint32_t uiHash = XXHash32::hash((void*)textureKey.TextureData, (uint64_t)XboxDataSize, PaletteHash);

		// Check if the data needs an updated conversion or not, now via the hash
		if (cacheEntry.pConvertedHostTexture != nullptr) {
			if (uiHash == cacheEntry.Hash) {
				// Hash is still the same - assume the converted resource doesn't require updating
				// TODO : Maybe, if the converted resource gets too old, an update might still be wise
				// to cater for differences that didn't cause a hash-difference (slight chance, but still).
				m_Mutex.unlock();
				return cacheEntry;
			}

			cacheEntry.pConvertedHostTexture->Release();
			cacheEntry.pConvertedHostTexture = nullptr;
		}

		cacheEntry.PaletteHash = PaletteHash;
		cacheEntry.Hash = uiHash;
		cacheEntry.XboxDataSize = XboxDataSize;
		m_Mutex.unlock();
		return cacheEntry;
	}

	void AddConvertedResource(struct TextureCacheEntry &convertedTexture, XTL::IDirect3DBaseTexture8* pHostTexture)
	{
		m_Mutex.lock();
		pHostTexture->AddRef();
		convertedTexture.pConvertedHostTexture = pHostTexture;
		m_Mutex.unlock();
	}
};

#endif // TEXTURE_CACHE_H