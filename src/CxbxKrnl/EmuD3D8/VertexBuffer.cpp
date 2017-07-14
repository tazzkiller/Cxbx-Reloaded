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
// *   CxbxKrnl->EmuD3D8->VertexBuffer.cpp
// *
// *  This file is part of the Cxbx-Reloaded project, a fork of Cxbx.
// *
// *  Cxbx-Reloaded is free software; you can redistribute it
// *  and/or modify it under the terms of the GNU General Public
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
// *  CopyRight (c) 2016-2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

#include "CxbxKrnl/xxhash32.h" // For XXHash32::hash()
#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
#include "CxbxKrnl/ResourceTracker.h"
#include "CxbxKrnl/MemoryManager.h"
#include "CxbxKrnl/EmuD3D8Logging.h"

#include <ctime>

#define HASH_SEED 0

#define VERTEX_BUFFER_CACHE_SIZE 256
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
	Dest = *pPatchedStream;
	if (pPatchedStream->pCachedHostVertexBuffer != nullptr) {
		pPatchedStream->pCachedHostVertexBuffer->AddRef();
	}

	if (pPatchedStream->bIsCacheEntry) {
		// Don't take ownership of pCachedHostVertexStreamZeroData when the cache entry alreay owns it
		Dest.bIsCacheEntry = false;
	}
}

void ActivatePatchedStream
(
	XTL::CxbxDrawContext *pDrawContext,
	UINT uiStream,
	XTL::CxbxPatchedStream *pPatchedStream
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

		if (FAILED(hRet))
			XTL::CxbxKrnlCleanup("Failed to set the type patched buffer as the new stream source!\n");
	}
}

void ReleasePatchedStream(XTL::CxbxPatchedStream *pPatchedStream)
{
	if (pPatchedStream->pCachedHostVertexStreamZeroData != nullptr) {
		if (pPatchedStream->bIsCacheEntry) {
			if (pPatchedStream->bCachedHostVertexStreamZeroDataIsAllocated) {
				free(pPatchedStream->pCachedHostVertexStreamZeroData);
				pPatchedStream->bCachedHostVertexStreamZeroDataIsAllocated = false;
			}

			pPatchedStream->bIsCacheEntry = false;
		}

		pPatchedStream->pCachedHostVertexStreamZeroData = nullptr;
	}

	if (pPatchedStream->pCachedHostVertexBuffer != nullptr) {
		pPatchedStream->pCachedHostVertexBuffer->Release();
		pPatchedStream->pCachedHostVertexBuffer = nullptr;
	}
}

XTL::CxbxVertexBufferConverter::CxbxVertexBufferConverter()
{
    m_uiNbrStreams = 0;
    ZeroMemory(m_PatchedStreams, sizeof(CxbxPatchedStream) * MAX_NBR_STREAMS);
    m_pVertexShaderDynamicPatch = nullptr;
}

XTL::CxbxVertexBufferConverter::~CxbxVertexBufferConverter()
{
}

void XTL::CxbxVertexBufferConverter::DumpCache(void)
{
    printf("--- Dumping streams cache ---\n");
    RTNode *pNode = g_PatchedStreamsCache.getHead();
    while(pNode != nullptr)
    {
        StreamCacheEntry *pStreamCacheEntry = (StreamCacheEntry *)pNode->pResource;
        if (pStreamCacheEntry != nullptr)
        {
            // TODO: Write nicer dump presentation
            printf("Key: 0x%.8X Cache Hits: %d IsUP: %s OrigStride: %d NewStride: %d HashCount: %d HashFreq: %d Length: %d Hash: 0x%.8X\n",
                   pNode->pKey, pStreamCacheEntry->uiCacheHitCount, pStreamCacheEntry->Stream.bCacheIsStreamZeroDrawUP ? "YES" : "NO",
                   pStreamCacheEntry->Stream.uiCachedXboxVertexStride, pStreamCacheEntry->Stream.uiCachedHostVertexStride,
                   pStreamCacheEntry->uiCheckCount, pStreamCacheEntry->uiCheckFrequency,
                   pStreamCacheEntry->Stream.uiCachedXboxVertexDataSize, pStreamCacheEntry->uiHash);
        }

        pNode = pNode->pNext;
    }
}

void XTL::CxbxVertexBufferConverter::CachePatchedStream(CxbxPatchedStream *pPatchedStream)
{
    // Check if the cache is full, if so, throw away the least used stream
    if (g_PatchedStreamsCache.get_count() > VERTEX_BUFFER_CACHE_SIZE) {
        void *pKeyToPrune = NULL;
        uint32 uiMinHitCount = 0xFFFFFFFF;
		clock_t ctExpiryTime = (clock() - MAX_STREAM_NOT_USED_TIME); // Was addition, seems wrong
        RTNode *pStreamCacheNode = g_PatchedStreamsCache.getHead();
        while(pStreamCacheNode != nullptr) {
			StreamCacheEntry *pStreamCacheNodeEntry = (StreamCacheEntry *)pStreamCacheNode->pResource;
            if (pStreamCacheNodeEntry != nullptr) {
                // First, check if there is an "expired" stream in the cache (not recently used)
                if (pStreamCacheNodeEntry->lLastUsed < ctExpiryTime) {
                    printf("!!!Found an old stream, %2.2f\n", ((FLOAT)(ctExpiryTime - pStreamCacheNodeEntry->lLastUsed)) / (FLOAT)CLOCKS_PER_SEC);
                    pKeyToPrune = pStreamCacheNode->pKey;
                    break;
                }

                // Find the least used cached stream
                if (uiMinHitCount > pStreamCacheNodeEntry->uiCacheHitCount) {
                    uiMinHitCount = pStreamCacheNodeEntry->uiCacheHitCount;
                    pKeyToPrune = pStreamCacheNode->pKey;
                }
            }

            pStreamCacheNode = pStreamCacheNode->pNext;
        }

        if (pKeyToPrune != NULL) {
            printf("!!!Removing stream\n\n");
            RemovePatchedStream(pKeyToPrune);
        }
    }

    // Start the actual stream caching
	StreamCacheEntry *pStreamCacheEntry = (StreamCacheEntry *)calloc(1, sizeof(StreamCacheEntry));

	pStreamCacheEntry->uiHash = XXHash32::hash(pPatchedStream->pCachedXboxVertexData, pPatchedStream->uiCachedXboxVertexDataSize, HASH_SEED);
    pStreamCacheEntry->uiCheckFrequency = 1; // Start with checking every 1th Draw..
    pStreamCacheEntry->uiCheckCount = 0;
    pStreamCacheEntry->uiCacheHitCount = 0;
    pStreamCacheEntry->lLastUsed = clock();
	pStreamCacheEntry->Samples[0] = ((DWORD*)pPatchedStream->pCachedXboxVertexData)[0];
	pStreamCacheEntry->Samples[1] = ((DWORD*)pPatchedStream->pCachedXboxVertexData)[1];
	pStreamCacheEntry->Samples[2] = ((DWORD*)pPatchedStream->pCachedXboxVertexData)[2];
	AssignPatchedStream(pStreamCacheEntry->Stream, pPatchedStream);
	// Mark the cache entry as such, but AFTER AssignPatchedStream,
	// so that ownership applies only to the cache entry (not the patch itself) :
	pStreamCacheEntry->Stream.bIsCacheEntry = true;

    g_PatchedStreamsCache.insert(pPatchedStream->pCachedXboxVertexData, pStreamCacheEntry);
}

void XTL::CxbxVertexBufferConverter::RemovePatchedStream(void *pKey)
{
	StreamCacheEntry *pStreamCacheEntry = (StreamCacheEntry *)g_PatchedStreamsCache.remove(pKey);
    if (pStreamCacheEntry != nullptr) {
		ReleasePatchedStream(&pStreamCacheEntry->Stream);
		free(pStreamCacheEntry);
    }
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
    StreamCacheEntry *pStreamCacheEntry = (StreamCacheEntry *)g_PatchedStreamsCache.get(pXboxVertexData);
    if (pStreamCacheEntry != nullptr) {
        pStreamCacheEntry->lLastUsed = clock();
        pStreamCacheEntry->uiCacheHitCount++;
		bFoundInCache = true;

		// Is cached data too small?
		if (pStreamCacheEntry->Stream.uiCachedXboxVertexDataSize < uiXboxVertexDataSize) {
			bFoundInCache = false;
		}
		// Is cached stride too small?
		else if (pStreamCacheEntry->Stream.uiCachedXboxVertexStride < uiXboxVertexStride) {
			bFoundInCache = false;
		} // TODO : Verify that equal (and larger) cached strides can really be reused
		else if (((DWORD*)pXboxVertexData)[0] != pStreamCacheEntry->Samples[0]) {
			bFoundInCache = false;
		}
		else if (((DWORD*)pXboxVertexData)[1] != pStreamCacheEntry->Samples[1]) {
			bFoundInCache = false;
		}
		else if (((DWORD*)pXboxVertexData)[2] != pStreamCacheEntry->Samples[2]) {
			bFoundInCache = false;
		}
		else if (pStreamCacheEntry->uiCheckCount++ >= (pStreamCacheEntry->uiCheckFrequency - 1))
		{
            // Hash the Xbox data over the cached stream length (which is a must for the UP stream)
            uint32_t uiHash = XXHash32::hash((void *)pXboxVertexData, pStreamCacheEntry->Stream.uiCachedXboxVertexDataSize, HASH_SEED);
            if (uiHash != pStreamCacheEntry->uiHash) {
				bFoundInCache = false;
            }
            else {
                // Take a while longer to check
                if (pStreamCacheEntry->uiCheckFrequency < 32*1024)
                    pStreamCacheEntry->uiCheckFrequency *= 2;

				pStreamCacheEntry->uiCheckCount = 0;
            }
		}

		if (!bFoundInCache) {
			// Free pStreamCacheEntry via it's key
			RemovePatchedStream(pXboxVertexData);
		}
		else {
			// Copy back all cached stream values
			AssignPatchedStream(m_PatchedStreams[uiStream], &(pStreamCacheEntry->Stream));
        }
    }

    g_PatchedStreamsCache.Unlock();

	if (bFoundInCache) {
		ActivatePatchedStream(pDrawContext, uiStream, &m_PatchedStreams[uiStream]);
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

	HRESULT hRet;
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
		uiXboxVertexStride = Xbox_g_Stream[uiStream].Stride;
		uiXboxVertexDataSize = g_MemoryManager.QueryAllocationSize(pXboxVertexData);
		// Derive the vertex count
		uiVertexCount = uiXboxVertexDataSize / uiXboxVertexStride;
		// Dxbx note : Don't overwrite pDrawContext.dwVertexCount with uiVertexCount, because an indexed draw
		// can (and will) use less vertices than the supplied nr of indexes. Thix fixes
		// the missing parts in the CompressedVertices sample (in Vertex shader mode).
		uiHostVertexStride = (bNeedVertexPatching) ? pStreamDynamicPatch->ConvertedStride : uiXboxVertexStride;
		dwHostVertexDataSize = uiVertexCount * uiHostVertexStride;

		hRet = g_pD3DDevice8->CreateVertexBuffer(dwHostVertexDataSize, 0, 0, XTL::D3DPOOL_MANAGED, &pNewHostVertexBuffer);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->CreateVertexBuffer");

		if (SUCCEEDED(hRet))
			DbgPrintf("CxbxVertexBufferConverter::ConvertStream : Successfully Created VertexBuffer (0x%.8X)\n", pNewHostVertexBuffer);

		hRet = pNewHostVertexBuffer->Lock(0, 0, &pHostVertexData, D3DLOCK_DISCARD);
		DEBUG_D3DRESULT(hRet, "pNewHostVertexBuffer->Lock");

        if (FAILED(hRet))
            CxbxKrnlCleanup("Couldn't lock the new buffer");

		bNeedStreamCopy = true;
    }

	if (bNeedVertexPatching) {
		// assert(bNeedStreamCopy || "bNeedVertexPatching implies bNeedStreamCopy (but copies via conversions");
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
		//assert(bVshHandleIsFVF);

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
		hRet = pNewHostVertexBuffer->Unlock();
		DEBUG_D3DRESULT(hRet, "pNewHostVertexBuffer->Unlock");

		pPatchedStream->pCachedHostVertexBuffer = pNewHostVertexBuffer;
	}

	ActivatePatchedStream(pDrawContext, uiStream, pPatchedStream);

	return bNeedStreamCopy;
}

void XTL::CxbxVertexBufferConverter::Apply(CxbxDrawContext *pDrawContext)
{
	if ((pDrawContext->XboxPrimitiveType < X_D3DPT_POINTLIST) || (pDrawContext->XboxPrimitiveType > X_D3DPT_POLYGON))
		CxbxKrnlCleanup("Unknown primitive type: 0x%.2X\n", pDrawContext->XboxPrimitiveType);

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
			for (uint32 i = 0; i < pVertexShaderDynamicPatch->NbrStreams; i++) {
				if (pVertexShaderDynamicPatch->pStreamPatches[i].NeedPatch) {
					m_pVertexShaderDynamicPatch = pVertexShaderDynamicPatch;
					m_uiNbrStreams = pVertexShaderDynamicPatch->NbrStreams;
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
			CachePatchedStream(&m_PatchedStreams[uiStream]);
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
				CxbxKrnlCleanup("Unsupported Position Mask (FVF := 0x%.8X dwPos := 0x%.8X)", dwCurFVF, dwPos);
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
			DbgPrintf("IVB PointSize := 0x%.8X\n", g_InlineVertexBuffer_Table[v].PointSize);
		}
#endif

        if (dwCurFVF & D3DFVF_DIFFUSE) {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Diffuse;
            DbgPrintf("IVB Diffuse := 0x%.8X\n", g_InlineVertexBuffer_Table[v].Diffuse);
        }

        if (dwCurFVF & D3DFVF_SPECULAR) {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Specular;
            DbgPrintf("IVB Specular := 0x%.8X\n", g_InlineVertexBuffer_Table[v].Specular);
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
