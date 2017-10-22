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
// *   Cxbx->Win32->CxbxKrnl->EmuD3D8->VertexBuffer.cpp
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
// *  (c) 2002-2004 Aaron Robinson <caustik@caustik.com>
// *                Kingofc <kingofc@freenet.de>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

#include <assert.h>

#include "Logging.h"
#include "CxbxKrnl/xxhash32.h" // For XXHash32::hash()
#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
#include "CxbxKrnl/ResourceTracker.h" // For ResourceTracker
#include "CxbxKrnl/MemoryManager.h"

#include <ctime>

#define HASH_SEED 0

#define VERTEX_BUFFER_CACHE_SIZE 256 // TODO : Restore to 256 once 'too big' drawing in X-Marbles and Turok menu's is fixed (0 crashes Cartoon - see ActivatePatchedStream)
#define MAX_STREAM_NOT_USED_TIME (2 * CLOCKS_PER_SEC) // TODO: Trim the not used time

// Inline vertex buffer emulation
extern XTL::X_D3DPRIMITIVETYPE XTL::g_InlineVertexBuffer_PrimitiveType = XTL::X_D3DPT_INVALID;
extern DWORD                   XTL::g_InlineVertexBuffer_FVF = 0;
extern struct XTL::_D3DIVB    *XTL::g_InlineVertexBuffer_Table = nullptr;
extern UINT                    XTL::g_InlineVertexBuffer_TableLength = 0;
extern UINT                    XTL::g_InlineVertexBuffer_TableOffset = 0;

FLOAT *g_InlineVertexBuffer_pData = nullptr;
UINT   g_InlineVertexBuffer_DataSize = 0;

void AssignPatchedStream(XTL::CxbxPatchedStream &Dest, XTL::CxbxPatchedStream *pPatchedStream)
{
	assert(pPatchedStream->bCacheIsUsed);

	Dest = *pPatchedStream;
	if (pPatchedStream->pCachedHostVertexBuffer != nullptr) {
		pPatchedStream->pCachedHostVertexBuffer->AddRef();
	}
}

void ActivatePatchedStream
(
	XTL::CxbxDrawContext *pDrawContext,
	UINT uiStream,
	XTL::CxbxPatchedStream *pPatchedStream,
	bool bRelease
)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	// Use the cached stream values on the host
	if (pPatchedStream->bCacheIsStreamZeroDrawUP) {
		// Set the UserPointer variables in the drawing context
		pDrawContext->pHostVertexStreamZeroData = pPatchedStream->pCachedHostVertexStreamZeroData;
		pDrawContext->uiHostVertexStreamZeroStride = pPatchedStream->uiCachedHostVertexStride;
	}
	else {
		HRESULT hRet = g_pD3DDevice8->SetStreamSource(uiStream, pPatchedStream->pCachedHostVertexBuffer, pPatchedStream->uiCachedHostVertexStride);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetStreamSource");
		if (FAILED(hRet)) {
			XTL::CxbxKrnlCleanup("Failed to set the type patched buffer as the new stream source!\n");
			// TODO : Cartoon hits the above case when the vertex cache size is 0.
		}

		// TODO : The following doesn't fix that - find our why and fix it for real
		if (bRelease) {
			// Always release to prevent leaks when it wasn't read from cache:
			pPatchedStream->pCachedHostVertexBuffer->Release();
			// NOTE : Even this doesn't prevent Cartoon breaking : g_pD3DDevice8->ResourceManagerDiscardBytes(0);
		}
	}
}

void ReleasePatchedStream(XTL::CxbxPatchedStream *pPatchedStream)
{
	if (pPatchedStream->bCachedHostVertexStreamZeroDataIsAllocated) {
		if (pPatchedStream->pCachedHostVertexStreamZeroData != nullptr) {
			g_MemoryManager.Free(pPatchedStream->pCachedHostVertexStreamZeroData);
			pPatchedStream->pCachedHostVertexStreamZeroData = nullptr;
		}

		pPatchedStream->bCachedHostVertexStreamZeroDataIsAllocated = false;
	}

	if (pPatchedStream->pCachedHostVertexBuffer != nullptr) {
		pPatchedStream->pCachedHostVertexBuffer->Release();
		pPatchedStream->pCachedHostVertexBuffer = nullptr;
	}
}

void ResourceDisposePatchedStream(void *pResource)
{
	XTL::CxbxCachedStream *pCachedStream = (XTL::CxbxCachedStream *)pResource;
	if (pCachedStream != nullptr) {
		ReleasePatchedStream(&pCachedStream->Stream);
		free(pCachedStream);
	}
}

ResourceTracker g_PatchedStreamsCache(ResourceDisposePatchedStream);

/* CxbxVertexBufferConverter */

XTL::CxbxVertexBufferConverter::CxbxVertexBufferConverter()
{
    this->m_uiNbrStreams = 0;
    ZeroMemory(this->m_PatchedStreams, sizeof(CxbxPatchedStream) * MAX_NBR_STREAMS);
    this->m_pVertexShaderDynamicPatch = nullptr;
}

XTL::CxbxVertexBufferConverter::~CxbxVertexBufferConverter()
{
}

void XTL::CxbxVertexBufferConverter::DumpCache(void)
{
    printf("--- Dumping streams cache ---\n");
	g_PatchedStreamsCache.Lock();
    RTNode *pNode = g_PatchedStreamsCache.getHead();
    while(pNode != nullptr)
    {
        CxbxCachedStream *pCachedStream = (CxbxCachedStream *)pNode->pResource;
        if (pCachedStream != nullptr)
        {
            // TODO: Write nicer dump presentation
            printf("Key: 0x%p Cache Hits: %u IsUP: %s OrigStride: %u NewStride: %u HashCount: %u HashFreq: %u Length: %u Hash: 0x%.8X\n",
                   pNode->pKey, pCachedStream->uiCacheHitCount, pCachedStream->Stream.bCacheIsStreamZeroDrawUP ? "YES" : "NO",
                   pCachedStream->Stream.uiCachedXboxVertexStride, pCachedStream->Stream.uiCachedHostVertexStride,
                   pCachedStream->uiCheckCount, pCachedStream->uiCheckFrequency,
                   pCachedStream->Stream.uiCachedXboxVertexDataSize, pCachedStream->uiHash);
        }

        pNode = pNode->pNext;
    }
	g_PatchedStreamsCache.Unlock();
}

void XTL::CxbxVertexBufferConverter::CacheStream
(
	CxbxPatchedStream *pPatchedStream
)
{
    // Check if the cache is full, if so, throw away the least used stream
    while (g_PatchedStreamsCache.get_count() > VERTEX_BUFFER_CACHE_SIZE) {
        void *pKey = NULL;
        uint32 uiMinHitCount = 0xFFFFFFFF;
		clock_t ctExpiryTime = (clock() - MAX_STREAM_NOT_USED_TIME); // Was addition, seems wrong
		g_PatchedStreamsCache.Lock();
        RTNode *pNode = g_PatchedStreamsCache.getHead();
        while(pNode != nullptr) {
			CxbxCachedStream *pNodeStream = (CxbxCachedStream *)pNode->pResource;
            if (pNodeStream != nullptr) {
                // First, check if there is an "expired" stream in the cache (not recently used)
                if (pNodeStream->lLastUsed < ctExpiryTime) {
                    printf("!!!Found an old stream, %2.2f\n", ((FLOAT)(ctExpiryTime - pNodeStream->lLastUsed)) / (FLOAT)CLOCKS_PER_SEC);
                    pKey = pNode->pKey;
                    break;
                }

                // Find the least used cached stream
                if (uiMinHitCount > pNodeStream->uiCacheHitCount) {
                    uiMinHitCount = pNodeStream->uiCacheHitCount;
                    pKey = pNode->pKey;
                }
            }

            pNode = pNode->pNext;
        }

		g_PatchedStreamsCache.Unlock();
        if (pKey != NULL) {
            printf("!!!Removing stream\n\n");
            FreeCachedStream(pKey);
        }
    }

    // Start the actual stream caching
	CxbxCachedStream *pCachedStream = (CxbxCachedStream *)calloc(1, sizeof(CxbxCachedStream));

	pCachedStream->uiHash = XXHash32::hash(pPatchedStream->pCachedXboxVertexData, pPatchedStream->uiCachedXboxVertexDataSize, HASH_SEED);
    pCachedStream->uiCheckFrequency = 1; // Start with checking every 1th Draw..
    pCachedStream->uiCheckCount = 0;
    pCachedStream->uiCacheHitCount = 0;
    pCachedStream->lLastUsed = clock();
	AssignPatchedStream(pCachedStream->Stream, pPatchedStream);

    g_PatchedStreamsCache.insert(pPatchedStream->pCachedXboxVertexData, pCachedStream);
}

void XTL::CxbxVertexBufferConverter::FreeCachedStream(void *pKey)
{
	g_PatchedStreamsCache.remove(pKey); // Note : Leads to a call to ResourceDisposePatchedStream
}

bool XTL::CxbxVertexBufferConverter::ApplyCachedStream
(
	CxbxDrawContext *pDrawContext,
    UINT             uiStream,
	bool			*pbFatalError
)
{
	void *pXboxVertexData = NULL;
    UINT uiXboxVertexStride = 0;
    UINT uiXboxVertexDataSize = 0;

    if (pDrawContext->pXboxVertexStreamZeroData != NULL) {
		// There should only be one stream (stream zero) in this case
		if (uiStream != 0)
			CxbxKrnlCleanup("Trying to find a cached Draw..UP with more than stream zero!");

		pXboxVertexData = (uint08 *)pDrawContext->pXboxVertexStreamZeroData;
		uiXboxVertexStride = pDrawContext->uiXboxVertexStreamZeroStride;
		// TODO : When called from D3DDevice_DrawIndexedVerticesUP, dwVertexCount
		// is actually the number of indices, which isn't too good. FIXME
		uiXboxVertexDataSize = pDrawContext->dwVertexCount * uiXboxVertexStride;
	}
	else {
		pXboxVertexData = GetDataFromXboxResource(Xbox_g_Stream[uiStream].pVertexBuffer);
		if (pXboxVertexData == NULL) {
			if (pbFatalError)
				*pbFatalError = true;

			return false;
		}

		uiXboxVertexStride = Xbox_g_Stream[uiStream].Stride;
		uiXboxVertexDataSize = g_MemoryManager.QueryAllocationSize(pXboxVertexData);
    }

    bool bFoundInCache = false;

    g_PatchedStreamsCache.Lock();
    CxbxCachedStream *pCachedStream = (CxbxCachedStream *)g_PatchedStreamsCache.get(pXboxVertexData);
    if (pCachedStream != nullptr) {
        pCachedStream->lLastUsed = clock();
        pCachedStream->uiCacheHitCount++;
		bFoundInCache = true;

		// Is cached data too small?
		if (pCachedStream->Stream.uiCachedXboxVertexDataSize < uiXboxVertexDataSize) {
			bFoundInCache = false;
		}
		// Is cached stride too small?
		else if (pCachedStream->Stream.uiCachedXboxVertexStride < uiXboxVertexStride) {
			bFoundInCache = false;
		} // TODO : Verify that equal (and larger) cached strides can really be reused
		else
        if (pCachedStream->uiCheckCount++ >= (pCachedStream->uiCheckFrequency - 1)) {
            // Hash the Xbox data over the cached stream length (which is a must for the UP stream)
            uint32_t uiHash = XXHash32::hash((void *)pXboxVertexData, pCachedStream->Stream.uiCachedXboxVertexDataSize, HASH_SEED);
            if (uiHash != pCachedStream->uiHash) {
				bFoundInCache = false;
            }
            else {
                // Take a while longer to check
                if (pCachedStream->uiCheckFrequency < 32*1024)
                    pCachedStream->uiCheckFrequency *= 2;

				pCachedStream->uiCheckCount = 0;
            }
		}

		if (!bFoundInCache) {
			// Free pCachedStream via it's key
			FreeCachedStream(pXboxVertexData);
		}
		else {
			// Copy back all cached stream values
			AssignPatchedStream(m_PatchedStreams[uiStream], &(pCachedStream->Stream));
        }
    }

    g_PatchedStreamsCache.Unlock();

	if (bFoundInCache) {
		ActivatePatchedStream(pDrawContext, uiStream, &m_PatchedStreams[uiStream], false);
	}

    return bFoundInCache;
}

bool XTL::CxbxVertexBufferConverter::ConvertStream
(
	CxbxDrawContext *pDrawContext,
    UINT             uiStream
)
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

	bool bVshHandleIsFVF = VshHandleIsFVF(pDrawContext->hVertexShader);
	bool bNeedTextureNormalization = false;
	struct { bool bTexIsLinear; int Width; int Height; } pActivePixelContainer[X_D3DTSS_STAGECOUNT] = { 0 };

	if (bVshHandleIsFVF) {
		DWORD dwTexN = (pDrawContext->hVertexShader & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
		// Check for active linear textures.
		for (uint i = 0; i < X_D3DTSS_STAGECOUNT; i++) {
			// Only normalize coordinates used by the FVF shader :
			pActivePixelContainer[i].bTexIsLinear = false;
			if (i + 1 <= dwTexN) {
				XTL::X_D3DBaseTexture *pXboxBaseTexture = XTL::GetXboxBaseTexture(i);
				if (pXboxBaseTexture != NULL) {
					// TODO : Use GetXboxPixelContainerFormat
					XTL::X_D3DFORMAT XBFormat = (XTL::X_D3DFORMAT)((pXboxBaseTexture->Format & X_D3DFORMAT_FORMAT_MASK) >> X_D3DFORMAT_FORMAT_SHIFT);
					if (EmuXBFormatIsLinear(XBFormat)) {
						// This is often hit by the help screen in XDK samples.
						bNeedTextureNormalization = true;
						// Remember linearity, width and height :
						pActivePixelContainer[i].bTexIsLinear = true;
						// TODO : Use DecodeD3DSize
						pActivePixelContainer[i].Width = (pXboxBaseTexture->Size & X_D3DSIZE_WIDTH_MASK) + 1;
						pActivePixelContainer[i].Height = ((pXboxBaseTexture->Size & X_D3DSIZE_HEIGHT_MASK) >> X_D3DSIZE_HEIGHT_SHIFT) + 1;
					}
				}
			}
		}
	}

    CxbxStreamDynamicPatch    *pStreamDynamicPatch = (m_pVertexShaderDynamicPatch != nullptr) ? (&m_pVertexShaderDynamicPatch->pStreamPatches[uiStream]) : nullptr;
	bool bNeedVertexPatching = (pStreamDynamicPatch != nullptr && pStreamDynamicPatch->NeedPatch);
	bool bNeedRHWReset = bVshHandleIsFVF && ((pDrawContext->hVertexShader & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW);
	bool bNeedStreamCopy = bNeedTextureNormalization || bNeedVertexPatching || bNeedRHWReset;

	uint08 *pXboxVertexData;
	UINT uiXboxVertexStride;
	UINT uiXboxVertexDataSize;
	UINT uiVertexCount;
	UINT uiHostVertexStride;
	DWORD dwHostVertexDataSize;
	uint08 *pHostVertexData;
	IDirect3DVertexBuffer8 *pNewHostVertexBuffer = nullptr;

	if (pDrawContext->pXboxVertexStreamZeroData != NULL) {
        // There should only be one stream (stream zero) in this case
        if (uiStream != 0)
            CxbxKrnlCleanup("Trying to patch a Draw..UP with more than stream zero!");

		pXboxVertexData = (uint08 *)pDrawContext->pXboxVertexStreamZeroData;
		uiXboxVertexStride  = pDrawContext->uiXboxVertexStreamZeroStride;
		// TODO : When called from D3DDevice_DrawIndexedVerticesUP, dwVertexCount
		// is actually the number of indices, which isn't too good. FIXME
		uiVertexCount = pDrawContext->dwVertexCount;
		uiXboxVertexDataSize = uiVertexCount * uiXboxVertexStride;

		uiHostVertexStride = (bNeedVertexPatching) ? pStreamDynamicPatch->ConvertedStride : uiXboxVertexStride;
		dwHostVertexDataSize = uiVertexCount * uiHostVertexStride;
		if (bNeedStreamCopy) {
			pHostVertexData = (uint08*)g_MemoryManager.Allocate(dwHostVertexDataSize);
			if (pHostVertexData == nullptr) {
				CxbxKrnlCleanup("Couldn't allocate the new stream zero buffer");
			}
		}
		else {
			pHostVertexData = pXboxVertexData;
		}
	} 
	else {
		pXboxVertexData = (uint08*)GetDataFromXboxResource(Xbox_g_Stream[uiStream].pVertexBuffer);
		if (pXboxVertexData == NULL) {
			HRESULT hRet = g_pD3DDevice8->SetStreamSource(uiStream, nullptr, 0);
			DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetStreamSource");
			if (FAILED(hRet))
				XTL::CxbxKrnlCleanup("g_pD3DDevice8->SetStreamSource(uiStream, nullptr, 0)\n");

			return false;
		}

		uiXboxVertexStride = Xbox_g_Stream[uiStream].Stride;
		uiXboxVertexDataSize = g_MemoryManager.QueryAllocationSize(pXboxVertexData);
		// Derive the vertex count
		uiVertexCount = uiXboxVertexDataSize / uiXboxVertexStride;
		// Dxbx note : Don't overwrite pDrawContext.dwVertexCount with uiVertexCount, because an indexed draw
		// can (and will) use less vertices than the supplied nr of indexes. Thix fixes
		// the missing parts in the CompressedVertices sample (in Vertex shader mode).
		uiHostVertexStride = (bNeedVertexPatching) ? pStreamDynamicPatch->ConvertedStride : uiXboxVertexStride;
		dwHostVertexDataSize = uiVertexCount * uiHostVertexStride;
		HRESULT hRet = g_pD3DDevice8->CreateVertexBuffer(dwHostVertexDataSize, 0, 0, XTL::D3DPOOL_MANAGED, &pNewHostVertexBuffer);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexBuffer");
		if (FAILED(hRet))
			CxbxKrnlCleanup("Couldn't CreateVertexBuffer");

		hRet = pNewHostVertexBuffer->Lock(0, 0, &pHostVertexData, D3DLOCK_DISCARD);
		DEBUG_D3DRESULT(hRet, "pNewHostVertexBuffer->Lock");
		if (FAILED(hRet))
            CxbxKrnlCleanup("Couldn't lock the new buffer");
        
		bNeedStreamCopy = true;
    }

	if (bNeedVertexPatching) {
		assert(bNeedStreamCopy || "bNeedVertexPatching implies bNeedStreamCopy (but copies via conversions");
		for (uint32 uiVertex = 0; uiVertex < uiVertexCount; uiVertex++) {
			uint08 *pOrigVertex = &pXboxVertexData[uiVertex * uiXboxVertexStride];
			uint08 *pNewDataPos = &pHostVertexData[uiVertex * uiHostVertexStride];
			for (UINT uiType = 0; uiType < pStreamDynamicPatch->NbrTypes; uiType++) {
				// Dxbx note : The following code handles only the D3DVSDT enums that need conversion;
				// All other cases are catched by the memcpy in the default-block.
				switch (pStreamDynamicPatch->pTypes[uiType]) {
				case X_D3DVSDT_NORMPACKED3: { // 0x16: // Make it FLOAT3
					// Hit by Dashboard
					int32 iPacked = ((int32 *)pOrigVertex)[0];
					// Cxbx note : to make each component signed, two need to be shifted towards the sign-bit first :
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((iPacked << 21) >> 21)) / 1023.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((iPacked << 10) >> 21)) / 1023.0f;
					((FLOAT *)pNewDataPos)[2] = ((FLOAT)((iPacked      ) >> 22)) / 511.0f;
					pOrigVertex += 1 * sizeof(int32);
					break;
				}
				case X_D3DVSDT_SHORT1: { // 0x15: // Make it SHORT2 and set the second short to 0
					((SHORT *)pNewDataPos)[0] = ((SHORT*)pOrigVertex)[0];
					((SHORT *)pNewDataPos)[1] = 0x00;
					pOrigVertex += 1 * sizeof(SHORT);
					break;
				}
				case X_D3DVSDT_SHORT3: { // 0x35: // Make it a SHORT4 and set the fourth short to 1
					// Hit by Turok
					memcpy(pNewDataPos, pOrigVertex, 3 * sizeof(SHORT));
					((SHORT *)pNewDataPos)[3] = 0x01;
					pOrigVertex += 3 * sizeof(SHORT);
					break;
				}
				case X_D3DVSDT_PBYTE1: { // 0x14:  // Make it FLOAT1
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((BYTE*)pOrigVertex)[0]) / 255.0f;
					pOrigVertex += 1 * sizeof(BYTE);
					break;
				}
				case X_D3DVSDT_PBYTE2: { // 0x24:  // Make it FLOAT2
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((BYTE*)pOrigVertex)[0]) / 255.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((BYTE*)pOrigVertex)[1]) / 255.0f;
					pOrigVertex += 2 * sizeof(BYTE);
					break;
				}
				case X_D3DVSDT_PBYTE3: { // 0x34: // Make it FLOAT3
					// Hit by Turok
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((BYTE*)pOrigVertex)[0]) / 255.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((BYTE*)pOrigVertex)[1]) / 255.0f;
					((FLOAT *)pNewDataPos)[2] = ((FLOAT)((BYTE*)pOrigVertex)[2]) / 255.0f;
					pOrigVertex += 3 * sizeof(BYTE);
					break;
				}
				case X_D3DVSDT_PBYTE4: { // 0x44: // Make it FLOAT4
					// Hit by Jet Set Radio Future
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((BYTE*)pOrigVertex)[0]) / 255.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((BYTE*)pOrigVertex)[1]) / 255.0f;
					((FLOAT *)pNewDataPos)[2] = ((FLOAT)((BYTE*)pOrigVertex)[2]) / 255.0f;
					((FLOAT *)pNewDataPos)[3] = ((FLOAT)((BYTE*)pOrigVertex)[3]) / 255.0f;
					pOrigVertex += 4 * sizeof(BYTE);
					break;
				}
				case X_D3DVSDT_NORMSHORT1: { // 0x11: // Make it FLOAT1
					LOG_TEST_CASE("X_D3DVSDT_NORMSHORT1"); // UNTESTED - Need test-case!

					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
					//((FLOAT *)pNewDataPos)[1] = 0.0f; // Would be needed for FLOAT2
					pOrigVertex += 1 * sizeof(SHORT);
					break;
				}
#if !DXBX_USE_D3D9 // No need for patching in D3D9
				case X_D3DVSDT_NORMSHORT2: { // 0x21: // Make it FLOAT2
					LOG_TEST_CASE("X_D3DVSDT_NORMSHORT2"); // UNTESTED - Need test-case!
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((SHORT*)pOrigVertex)[1]) / 32767.0f;
					pOrigVertex += 2 * sizeof(SHORT);
					break;
				}
#endif
				case X_D3DVSDT_NORMSHORT3: { // 0x31: // Make it FLOAT3
					LOG_TEST_CASE("X_D3DVSDT_NORMSHORT3"); // UNTESTED - Need test-case!
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((SHORT*)pOrigVertex)[1]) / 32767.0f;
					((FLOAT *)pNewDataPos)[2] = ((FLOAT)((SHORT*)pOrigVertex)[2]) / 32767.0f;
					pOrigVertex += 3 * sizeof(SHORT);
					break;
				}
#if !DXBX_USE_D3D9 // No need for patching in D3D9
				case X_D3DVSDT_NORMSHORT4: { // 0x41: // Make it FLOAT4
					LOG_TEST_CASE("X_D3DVSDT_NORMSHORT4"); // UNTESTED - Need test-case!
					((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
					((FLOAT *)pNewDataPos)[1] = ((FLOAT)((SHORT*)pOrigVertex)[1]) / 32767.0f;
					((FLOAT *)pNewDataPos)[2] = ((FLOAT)((SHORT*)pOrigVertex)[2]) / 32767.0f;
					((FLOAT *)pNewDataPos)[3] = ((FLOAT)((SHORT*)pOrigVertex)[3]) / 32767.0f;
					pOrigVertex += 4 * sizeof(SHORT);
					break;
				}
#endif
				case X_D3DVSDT_FLOAT2H: { // 0x72: // Make it FLOAT4 and set the third float to 0.0
					((FLOAT *)pNewDataPos)[0] = ((FLOAT*)pOrigVertex)[0];
					((FLOAT *)pNewDataPos)[1] = ((FLOAT*)pOrigVertex)[1];
					((FLOAT *)pNewDataPos)[2] = 0.0f;
					((FLOAT *)pNewDataPos)[3] = ((FLOAT*)pOrigVertex)[2];
					pOrigVertex += 3 * sizeof(FLOAT);
					break;
				}
				/*TODO
				case X_D3DVSDT_NONE: { // 0x02:
					printf("D3DVSDT_NONE / xbox ext. nsp /");
					dwNewDataType = 0xFF;
					break;
				}
				*/
				default: {
					// Generic 'conversion' - just make a copy :
					memcpy(pNewDataPos, pOrigVertex, pStreamDynamicPatch->pSizes[uiType]);
					pOrigVertex += pStreamDynamicPatch->pSizes[uiType];
					break;
				}
				} // switch

				// Increment the new pointer :
				pNewDataPos += pStreamDynamicPatch->pSizes[uiType];
			}
		}
	}
	else {
		if (bNeedStreamCopy) {
			memcpy(pHostVertexData, pXboxVertexData, dwHostVertexDataSize);
		}
	}

	// FVF buffers don't have Xbox extensions, but texture coordinates may
	// need normalization if used with linear textures.
	if (bNeedTextureNormalization || bNeedRHWReset)
	{
		assert(bVshHandleIsFVF);

		UINT uiUVOffset = 0;

		// Locate texture coordinate offset in vertex structure.
		if (bNeedTextureNormalization) {
			uiUVOffset = XTL::DxbxFVFToVertexSizeInBytes(pDrawContext->hVertexShader, /*bIncludeTextures=*/false);
		}

		for (uint32 uiVertex = 0; uiVertex < uiVertexCount; uiVertex++) {
			FLOAT *pVertexDataAsFloat = (FLOAT*)(&pHostVertexData[uiVertex * uiHostVertexStride]);

			// Handle pre-transformed vertices (which bypass the vertex shader pipeline)
			if (bNeedRHWReset) {
				// Check Z. TODO : Why reset Z from 0.0 to 1.0 ? (Maybe fog-related?)
				if (pVertexDataAsFloat[2] == 0.0f) {
					// LOG_TEST_CASE("D3DFVF_XYZRHW (Z)"); // Test-case : Many XDK Samples (AlphaFog, PointSprites)
					pVertexDataAsFloat[2] = 1.0f;
				}

				// Check RHW. TODO : Why reset from 0.0 to 1.0 ? (Maybe 1.0 indicates that the vertices are not to be transformed)
				if (pVertexDataAsFloat[3] == 0.0f) {
					// LOG_TEST_CASE("D3DFVF_XYZRHW (RHW)"); // Test-case : Many XDK Samples (AlphaFog, PointSprites)
					pVertexDataAsFloat[3] = 1.0f;
				}
			}

			// Normalize texture coordinates in FVF stream if needed
			if (uiUVOffset > 0) { // uiUVOffset > 0 implies bNeedTextureNormalization (using one is more efficient than both)
				FLOAT *pVertexUVData = (FLOAT*)((uintptr_t)pVertexDataAsFloat + uiUVOffset);
				for (uint i = 0; i < X_D3DTSS_STAGECOUNT; i++) {
					if (pActivePixelContainer[i].bTexIsLinear) {
						pVertexUVData[(i * 2) + 0] /= pActivePixelContainer[i].Width;
						pVertexUVData[(i * 2) + 1] /= pActivePixelContainer[i].Height;
					}
				}
			}
		}
	}

	CxbxPatchedStream *pPatchedStream = &m_PatchedStreams[uiStream];

	pPatchedStream->pCachedXboxVertexData = pXboxVertexData;
    pPatchedStream->uiCachedXboxVertexStride = uiXboxVertexStride;
	pPatchedStream->uiCachedXboxVertexDataSize = uiXboxVertexDataSize;
    pPatchedStream->uiCachedHostVertexStride = uiHostVertexStride;
	pPatchedStream->bCacheIsStreamZeroDrawUP = (pDrawContext->pXboxVertexStreamZeroData != NULL);
	if (pPatchedStream->bCacheIsStreamZeroDrawUP) {
		pPatchedStream->pCachedHostVertexStreamZeroData = pHostVertexData;
		pPatchedStream->bCachedHostVertexStreamZeroDataIsAllocated = bNeedStreamCopy;
    }
	else {
		assert(pNewHostVertexBuffer != nullptr);

		pNewHostVertexBuffer->Unlock();
		pPatchedStream->pCachedHostVertexBuffer = pNewHostVertexBuffer;
	}

	ActivatePatchedStream(pDrawContext, uiStream, pPatchedStream, 
		/*Release=*/!bNeedStreamCopy); // Release when it won't get cached

	return bNeedStreamCopy;
} // ConvertStream

void XTL::CxbxVertexBufferConverter::Apply(CxbxDrawContext *pDrawContext)
{
	if ((pDrawContext->XboxPrimitiveType < X_D3DPT_POINTLIST) || (pDrawContext->XboxPrimitiveType > X_D3DPT_POLYGON))
		CxbxKrnlCleanup("Unknown primitive type: 0x%.02X\n", pDrawContext->XboxPrimitiveType);

	bool bFatalError = false;

	// Determine the number of streams of a patch (was GetNbrStreams)
	m_uiNbrStreams = 0;
	if (pDrawContext->hVertexShader > 0)
	{
		m_uiNbrStreams = 1; // Could be more, but it doesn't matter as long as we're not going to patch the types
		CxbxVertexShader *pHostVertexShader = VshHandleGetHostVertexShader(pDrawContext->hVertexShader);
		if (pHostVertexShader != nullptr) {
			// TODO : Introduce VshGetVertexDynamicPatch(Handle) ?
			CxbxVertexShaderDynamicPatch *pVertexShaderDynamicPatch = &(pHostVertexShader->VertexShaderDynamicPatch);
			m_uiNbrStreams = pVertexShaderDynamicPatch->NbrStreams;
			for (uint32 i = 0; i < pVertexShaderDynamicPatch->NbrStreams; i++) {
				if (pVertexShaderDynamicPatch->pStreamPatches[i].NeedPatch) {
					m_pVertexShaderDynamicPatch = pVertexShaderDynamicPatch;
					break;
				}
			}
		}
	}

    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++) {
#ifndef VERTEX_CACHE_DISABLED
		if (ApplyCachedStream(pDrawContext, uiStream, &bFatalError)) {
            continue;
        }
#endif

		if (ConvertStream(pDrawContext, uiStream)) {
#ifndef VERTEX_CACHE_DISABLED
			// Insert the patched stream in the cache
			m_PatchedStreams[uiStream].bCacheIsUsed = true;
			CacheStream(&m_PatchedStreams[uiStream]);
#endif
		}
    }
        
	if (pDrawContext->XboxPrimitiveType == X_D3DPT_QUADSTRIP) {
		// Quad strip is just like a triangle strip, but requires two vertices per primitive.
		// A quadstrip starts with 4 vertices and adds 2 vertices per additional quad.
		// This is much like a trianglestrip, which starts with 3 vertices and adds
		// 1 vertex per additional triangle, so we use that instead. The planar nature
		// of the quads 'survives' through this change. There's a catch though :
		// In a trianglestrip, every 2nd triangle has an opposing winding order,
		// which would cause backface culling - but this seems to be intelligently
		// handled by d3d :
		// Test-case : XDK Samples (FocusBlur, MotionBlur, Trees, PaintEffect, PlayField)
		// No need to set : pDrawContext->XboxPrimitiveType = X_D3DPT_TRIANGLESTRIP;
		pDrawContext->dwHostPrimitiveCount = EmuD3DVertex2PrimitiveCount(X_D3DPT_TRIANGLESTRIP, pDrawContext->dwVertexCount);
	} else {
		pDrawContext->dwHostPrimitiveCount = EmuD3DVertex2PrimitiveCount(pDrawContext->XboxPrimitiveType, pDrawContext->dwVertexCount);
	}

	if (pDrawContext->XboxPrimitiveType == X_D3DPT_POLYGON) {
		// Convex polygon is the same as a triangle fan.
		// No need to set : pDrawContext->XboxPrimitiveType = X_D3DPT_TRIANGLEFAN;
		LOG_TEST_CASE("X_D3DPT_POLYGON");
	}
}

void XTL::CxbxVertexBufferConverter::Restore()
{
    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++) {
		// Skip freeing resources when they're cached
		if (m_PatchedStreams[uiStream].bCacheIsUsed) {
			m_PatchedStreams[uiStream].bCacheIsUsed = false;
			continue;
		}

		ReleasePatchedStream(&m_PatchedStreams[uiStream]);
    }
}

VOID XTL::EmuFlushIVB()
{
	LOG_INIT // Allows use of DEBUG_D3DRESULT

    XTL::DxbxUpdateDeferredStates();

    // Parse IVB table with current FVF shader if possible.
    bool bFVF = VshHandleIsFVF(g_CurrentVertexShader);
    DWORD dwCurFVF;
    if (bFVF && ((g_CurrentVertexShader & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW)) {
        dwCurFVF = g_CurrentVertexShader;
		// HACK: Halo...
		if (dwCurFVF == 0) {
			EmuWarning("EmuFlushIVB(): using g_InlineVertexBuffer_FVF instead of current FVF!");
			dwCurFVF = g_InlineVertexBuffer_FVF;
		}
    }
    else {
        dwCurFVF = g_InlineVertexBuffer_FVF;
    }

    DbgPrintf("g_InlineVertexBuffer_TableOffset := %d\n", g_InlineVertexBuffer_TableOffset);

    // Do this once, not inside the for-loop :
    DWORD dwPos = dwCurFVF & D3DFVF_POSITION_MASK;
	DWORD dwTexN = (dwCurFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	// Use a tooling function to determine the vertex stride :
	UINT uiStride = DxbxFVFToVertexSizeInBytes(dwCurFVF, /*bIncludeTextures=*/true);
	// Make sure the output buffer is big enough 
	UINT NeededSize = g_InlineVertexBuffer_TableOffset * uiStride;
	if (g_InlineVertexBuffer_DataSize < NeededSize) {
		g_InlineVertexBuffer_DataSize = NeededSize;
		if (g_InlineVertexBuffer_pData != nullptr) {
			free(g_InlineVertexBuffer_pData);
		}

		g_InlineVertexBuffer_pData = (FLOAT*)malloc(g_InlineVertexBuffer_DataSize);
	}

	FLOAT *pVertexBufferData = g_InlineVertexBuffer_pData;
	for(uint v=0;v<g_InlineVertexBuffer_TableOffset;v++) {
		*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.x;
        *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.y;
        *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.z;
        if (dwPos == D3DFVF_XYZRHW) {
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Rhw;
            DbgPrintf("IVB Position := {%f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Rhw);
        }
		else { // XYZRHW cannot be combined with NORMAL, but the other XYZ formats can :
			if (dwPos == D3DFVF_XYZ) {
				DbgPrintf("IVB Position := {%f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z);
			}
			else if (dwPos == D3DFVF_XYZB1) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				DbgPrintf("IVB Position := {%f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1);
			}
			else if (dwPos == D3DFVF_XYZB2) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;
				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2);
			}
			else if (dwPos == D3DFVF_XYZB3) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend3;
				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2, g_InlineVertexBuffer_Table[v].Blend3);
			}
			else if (dwPos == D3DFVF_XYZB4) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend3;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend4;
				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2, g_InlineVertexBuffer_Table[v].Blend3, g_InlineVertexBuffer_Table[v].Blend4);
			}
			else { // 0 or D3DFVF_XYZB5
				CxbxKrnlCleanup("Unsupported Position Mask (FVF := 0x%.08X dwPos := 0x%.08X)", dwCurFVF, dwPos);
			}

			if (dwCurFVF & D3DFVF_NORMAL) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Normal.x;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Normal.y;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Normal.z;
				DbgPrintf("IVB Normal := {%f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Normal.x, g_InlineVertexBuffer_Table[v].Normal.y, g_InlineVertexBuffer_Table[v].Normal.z);
			}
		}

#if 0 // TODO : Was this support on Xbox from some point in time (pun intended)?
		if (dwCurFVF & D3DFVF_PSIZE) {
			*(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].PointSize;
			DbgPrintf("IVB PointSize := 0x%.08X\n", g_InlineVertexBuffer_Table[v].PointSize);
		}
#endif

        if (dwCurFVF & D3DFVF_DIFFUSE) {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Diffuse;
            DbgPrintf("IVB Diffuse := 0x%.08X\n", g_InlineVertexBuffer_Table[v].Diffuse);
        }

        if (dwCurFVF & D3DFVF_SPECULAR) {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Specular;
            DbgPrintf("IVB Specular := 0x%.08X\n", g_InlineVertexBuffer_Table[v].Specular);
        }

		// TODO -oDxbx : Handle other sizes than D3DFVF_TEXCOORDSIZE2 too!
		// See D3DTSS_TEXTURETRANSFORMFLAGS values other than D3DTTFF_COUNT2
		// See and/or X_D3DVSD_DATATYPEMASK values other than D3DVSDT_FLOAT2
		if (dwTexN >= 1) {
			// DxbxFVF_GetTextureSize
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.x;
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.y;
			//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.z;
            DbgPrintf("IVB TexCoord1 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord1.x, g_InlineVertexBuffer_Table[v].TexCoord1.y);
			if (dwTexN >= 2) {
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.x;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.y;
				//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.z;
				DbgPrintf("IVB TexCoord2 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord2.x, g_InlineVertexBuffer_Table[v].TexCoord2.y);
				if (dwTexN >= 3) {
					*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.x;
					*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.y;
					//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.z;
					DbgPrintf("IVB TexCoord3 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord3.x, g_InlineVertexBuffer_Table[v].TexCoord3.y);
					if (dwTexN >= 4) {
						*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord4.x;
						*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord4.y;
						//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord4.z;
						DbgPrintf("IVB TexCoord4 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord4.x, g_InlineVertexBuffer_Table[v].TexCoord4.y);
					}
				}
			}
        }

		if (v == 0) {
			uint VertexBufferUsage = (uintptr_t)pVertexBufferData - (uintptr_t)g_InlineVertexBuffer_pData;
			if (VertexBufferUsage != uiStride) {
				CxbxKrnlCleanup("EmuFlushIVB uses wrong stride!");
			}
		}
	}

	CxbxDrawContext DrawContext = {};
    DrawContext.XboxPrimitiveType = g_InlineVertexBuffer_PrimitiveType;
    DrawContext.dwVertexCount = g_InlineVertexBuffer_TableOffset;
    DrawContext.pXboxVertexStreamZeroData = g_InlineVertexBuffer_pData;
    DrawContext.uiXboxVertexStreamZeroStride = uiStride;
    DrawContext.hVertexShader = g_CurrentVertexShader;

    // Disable this 'fix', as it doesn't really help; On ATI, it isn't needed (and causes missing
    // textures if enabled). On Nvidia, it stops the jumping (but also removes the font from view).
    // So I think it's better to keep this bug visible, as a motivation for a real fix, and better
    // rendering on ATI chipsets...

//    bFVF = true; // This fixes jumping triangles on Nvidia chipsets, as suggested by Defiance
    // As a result however, this change also seems to remove the texture of the fonts in XSokoban!?!

	HRESULT hRet;

	if (bFVF) {
		hRet = g_pD3DDevice8->SetVertexShader(dwCurFVF);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader");
	}

	CxbxDrawPrimitiveUP(DrawContext);
	if (bFVF) {
		hRet = g_pD3DDevice8->SetVertexShader(g_CurrentVertexShader);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShader");
	}
}
