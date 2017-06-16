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

#include "CxbxKrnl/xxhash32.h" // For XXHash32::hash()
#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuAlloc.h"
#include "CxbxKrnl/EmuXTL.h"
#include "CxbxKrnl/ResourceTracker.h"
#include "CxbxKrnl/MemoryManager.h"

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

XTL::CxbxVertexBufferConverter::CxbxVertexBufferConverter()
{
    this->m_uiNbrStreams = 0;
    ZeroMemory(this->m_pStreams, sizeof(CxbxPatchedStream) * MAX_NBR_STREAMS);
    this->m_bPatched = false;
    this->m_bAllocatedStreamZeroData = false;
    this->m_pNewVertexStreamZeroData = nullptr;
    this->m_pDynamicPatch = nullptr;
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
        CxbxCachedStream *pCachedStream = (CxbxCachedStream *)pNode->pResource;
        if(pCachedStream != nullptr)
        {
            // TODO: Write nicer dump presentation
            printf("Key: 0x%.08X Cache Hits: %d IsUP: %s OrigStride: %d NewStride: %d HashCount: %d HashFreq: %d Length: %d Hash: 0x%.08X\n",
                   pNode->uiKey, pCachedStream->uiCacheHit, pCachedStream->bIsUP ? "YES" : "NO",
                   pCachedStream->Stream.uiOrigStride, pCachedStream->Stream.uiNewStride,
                   pCachedStream->uiCheckCount, pCachedStream->uiCheckFrequency,
                   pCachedStream->uiLength, pCachedStream->uiHash);
        }

        pNode = pNode->pNext;
    }
}

void XTL::CxbxVertexBufferConverter::CacheStream(CxbxDrawContext *pDrawContext,
                                     UINT             uiStream)
{
    void                      *pXboxVertexData = NULL;
    uint32                     uiKey;
    UINT                       uiLength;
    CxbxCachedStream          *pCachedStream = (CxbxCachedStream *)calloc(1, sizeof(CxbxCachedStream));

    // Check if the cache is full, if so, throw away the least used stream
    if(g_PatchedStreamsCache.get_count() > VERTEX_BUFFER_CACHE_SIZE)
    {
        uint32 uiKey = NULL;
        uint32 uiMinHit = 0xFFFFFFFF;
		clock_t ctExpiryTime = (clock() - MAX_STREAM_NOT_USED_TIME); // Was addition, seems wrong
        RTNode *pNode = g_PatchedStreamsCache.getHead();
        while(pNode != nullptr)
        {
			CxbxCachedStream *pNodeStream = (CxbxCachedStream *)pNode->pResource;
            if(pNodeStream != nullptr)
            {
                // First, check if there is an "expired" stream in the cache (not recently used)
                if(pNodeStream->lLastUsed < ctExpiryTime)
                {
                    printf("!!!Found an old stream, %2.2f\n", ((FLOAT)(ctExpiryTime - pNodeStream->lLastUsed)) / (FLOAT)CLOCKS_PER_SEC);
                    uiKey = pNode->uiKey;
                    break;
                }

                // Find the least used cached stream
                if(uiMinHit > pNodeStream->uiCacheHit)
                {
                    uiMinHit = pNodeStream->uiCacheHit;
                    uiKey = pNode->uiKey;
                }
            }

            pNode = pNode->pNext;
        }

        if(uiKey != NULL)
        {
            printf("!!!Removing stream\n\n");
            FreeCachedStream((void*)uiKey);
        }
    }

    // Start the actual stream caching
    if(pDrawContext->pVertexStreamZeroData != NULL)
    {
        // There should only be one stream (stream zero) in this case
        if(uiStream != 0)
            CxbxKrnlCleanup("Trying to patch a Draw..UP with more than stream zero!");

		//uiInputStride  = pDrawContext->uiVertexStreamZeroStride;
        pXboxVertexData = (uint08 *)pDrawContext->pVertexStreamZeroData;
        // TODO: This is sometimes the number of indices, which isn't too good
        uiLength = pDrawContext->dwVertexCount * pDrawContext->uiVertexStreamZeroStride;
        pCachedStream->bIsUP = true;
        pCachedStream->pStreamUP = pXboxVertexData;
        uiKey = (uint32)pXboxVertexData;
    }
	else
	{
		if (pCachedStream->Stream.pHostVertexBuffer != nullptr)
			m_pStreams[uiStream].pHostVertexBuffer->AddRef();

		pXboxVertexData = m_pStreams[uiStream].pXboxVertexData;
		uiLength = g_MemoryManager.QueryAllocationSize(pXboxVertexData);
		uiKey = (uint32)pXboxVertexData;

		pCachedStream->bIsUP = false;
	}

    uint32_t uiHash = XXHash32::hash((void *)pXboxVertexData, uiLength, HASH_SEED);

    pCachedStream->uiHash = uiHash;
    pCachedStream->uiCheckFrequency = 1; // Start with checking every 1th Draw..
    pCachedStream->uiCheckCount = 0;
    pCachedStream->uiCacheHit = 0;
    pCachedStream->lLastUsed = clock();
    pCachedStream->Stream = m_pStreams[uiStream];
    pCachedStream->uiLength = uiLength;
    pCachedStream->Copy = *pDrawContext;

    g_PatchedStreamsCache.insert(uiKey, pCachedStream);
}

void XTL::CxbxVertexBufferConverter::FreeCachedStream(void *pStream)
{
    g_PatchedStreamsCache.Lock();

	CxbxCachedStream *pCachedStream = (CxbxCachedStream *)g_PatchedStreamsCache.get(pStream);
    if(pCachedStream != nullptr)
    {
		if (pCachedStream->bIsUP && (pCachedStream->pStreamUP != nullptr)) {
			free(pCachedStream->pStreamUP);
			pCachedStream->pStreamUP = nullptr;
		}

		if (pCachedStream->Stream.pHostVertexBuffer != nullptr) {
			pCachedStream->Stream.pHostVertexBuffer->Release();
			pCachedStream->Stream.pHostVertexBuffer = nullptr;
		}

		free(pCachedStream);
    }

    g_PatchedStreamsCache.Unlock();
    g_PatchedStreamsCache.remove(pStream);
}

bool XTL::CxbxVertexBufferConverter::ApplyCachedStream
(
	CxbxDrawContext *pDrawContext,
    UINT             uiStream,
	bool			*pbFatalError
)
{
	void                      *pXboxVertexData = NULL;
    UINT                       uiStride;
    UINT                       uiLength;
    bool                       bApplied = false;
    uint32                     uiKey;
    //CxbxCachedStream          *pCachedStream = (CxbxCachedStream *)malloc(sizeof(CxbxCachedStream));

    if(pDrawContext->pVertexStreamZeroData != NULL) {
		// There should only be one stream (stream zero) in this case
		if (uiStream != 0)
			CxbxKrnlCleanup("Trying to find a cached Draw..UP with more than stream zero!");

		pXboxVertexData = (uint08 *)pDrawContext->pVertexStreamZeroData;
		uiStride = pDrawContext->uiVertexStreamZeroStride;
		// TODO: This is sometimes the number of indices, which isn't too good
		uiLength = pDrawContext->dwVertexCount * pDrawContext->uiVertexStreamZeroStride;
		uiKey = (uint32)pXboxVertexData;
		//pCachedStream->bIsUP = true;
		//pCachedStream->pStreamUP = pXboxVertexData;
	}
	else {
		pXboxVertexData = GetDataFromXboxResource(Xbox_g_Stream[uiStream].pVertexBuffer);
		if (pXboxVertexData == NULL) {
			if (pbFatalError)
				*pbFatalError = true;

			return false;
		}

		uiStride = Xbox_g_Stream[uiStream].Stride;
		uiLength = g_MemoryManager.QueryAllocationSize(pXboxVertexData);
		uiKey = (uint32)pXboxVertexData;
		//pCachedStream->bIsUP = false;
    }

    g_PatchedStreamsCache.Lock();

    CxbxCachedStream *pCachedStream = (CxbxCachedStream *)g_PatchedStreamsCache.get(uiKey);
    if(pCachedStream != nullptr)
    {
        pCachedStream->lLastUsed = clock();
        pCachedStream->uiCacheHit++;
        bool bMismatch = false;
        if(pCachedStream->uiCheckCount == (pCachedStream->uiCheckFrequency - 1)) {
            // Use the cached stream length (which is a must for the UP stream)
            uint32_t uiHash = XXHash32::hash((void *)pXboxVertexData, pCachedStream->uiLength, HASH_SEED);
            if(uiHash == pCachedStream->uiHash) {
                // Take a while longer to check
                if(pCachedStream->uiCheckFrequency < 32*1024)
                    pCachedStream->uiCheckFrequency *= 2;

				pCachedStream->uiCheckCount = 0;
            }
            else {
                // TODO: Do something about this
                if(pCachedStream->bIsUP) {
                    FreeCachedStream(pCachedStream->pStreamUP);
					pCachedStream->pStreamUP = nullptr;
                }

                pCachedStream = nullptr;
                bMismatch = true;
            }
		}
        else {
            pCachedStream->uiCheckCount++;
        }

        if(!bMismatch) {
            if(pCachedStream->bIsUP) {
                pDrawContext->pVertexStreamZeroData = pCachedStream->pStreamUP;
                pDrawContext->uiVertexStreamZeroStride = pCachedStream->Stream.uiNewStride;
            }
            else {
                g_pD3DDevice8->SetStreamSource(uiStream, pCachedStream->Stream.pHostVertexBuffer, pCachedStream->Stream.uiNewStride);
				if (pCachedStream->Stream.pHostVertexBuffer != nullptr) {
					pCachedStream->Stream.pHostVertexBuffer->AddRef();
				}

                m_pStreams[uiStream].uiOrigStride = uiStride;
                m_pStreams[uiStream].uiNewStride = pCachedStream->Stream.uiNewStride;
				m_pStreams[uiStream].pHostVertexBuffer = pCachedStream->Stream.pHostVertexBuffer;
            }

            // The primitives were patched, draw with the correct number of primimtives from the cache
			pDrawContext->XboxPrimitiveType = pCachedStream->Copy.XboxPrimitiveType;
			pDrawContext->dwPrimitiveCount = pCachedStream->Copy.dwPrimitiveCount;
			pDrawContext->dwVertexCount = pCachedStream->Copy.dwVertexCount;

            bApplied = true;
            m_bPatched = true;
        }
    }

    g_PatchedStreamsCache.Unlock();

    return bApplied;
}

void XTL::CxbxVertexBufferConverter::ConvertStream
(
	CxbxDrawContext *pDrawContext,
    UINT             uiStream
)
{
	UINT                       uiVertexCount = 0;
    uint08                    *pXboxVertexInputData = NULL;
	UINT                       uiInputStride = 0;
    uint08                    *pOutputData = nullptr;
	UINT                       uiOutputStride = 0;
    DWORD					   dwOutputSize = 0;
    IDirect3DVertexBuffer8    *pHostVertexBuffer = nullptr;

    CxbxPatchedStream         *pStream = &m_pStreams[uiStream];
    CxbxStreamDynamicPatch    *pStreamPatch = (m_pDynamicPatch != nullptr) ? (&m_pDynamicPatch->pStreamPatches[uiStream]) : nullptr;

	bool bNeedVertexPatching = (pStreamPatch != nullptr && pStreamPatch->NeedPatch);
	bool bVshHandleIsFVF = VshHandleIsFVF(pDrawContext->hVertexShader);
	bool bNeedRHWReset = bVshHandleIsFVF && ((pDrawContext->hVertexShader & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW);
	bool bNeedTextureNormalization = false;
	bool bNeedStreamCopy = false;

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

	bNeedStreamCopy = bNeedVertexPatching || bNeedTextureNormalization || bNeedRHWReset;

    if(pDrawContext->pVertexStreamZeroData != NULL)
    {
        // There should only be one stream (stream zero) in this case
        if(uiStream != 0)
            CxbxKrnlCleanup("Trying to patch a Draw..UP with more than stream zero!");

		pXboxVertexInputData = (uint08 *)pDrawContext->pVertexStreamZeroData;
		uiInputStride  = pDrawContext->uiVertexStreamZeroStride;
		uiOutputStride = (pStreamPatch != nullptr) ? max(pStreamPatch->ConvertedStride, uiInputStride) : uiInputStride;
        // TODO: This is sometimes the number of indices, which isn't too good
		uiVertexCount = pDrawContext->dwVertexCount;
        dwOutputSize = uiVertexCount * uiOutputStride;
		if (bNeedStreamCopy) {
			pOutputData = (uint08*)g_MemoryManager.Allocate(dwOutputSize);
			if (pOutputData == nullptr) {
				CxbxKrnlCleanup("Couldn't allocate the new stream zero buffer");
			}

			if (!m_bAllocatedStreamZeroData) {
				// The stream was not previously patched. We'll need this when restoring
				m_bAllocatedStreamZeroData = true;
				m_pNewVertexStreamZeroData = pOutputData;
			}
		}
		else {
			pOutputData = pXboxVertexInputData;
		}
	}
	else
	{
		pXboxVertexInputData = (uint08*)GetDataFromXboxResource(Xbox_g_Stream[uiStream].pVertexBuffer);
		uiInputStride = Xbox_g_Stream[uiStream].Stride;
		// Set a new (exact) vertex count
		uiVertexCount = g_MemoryManager.QueryAllocationSize(pXboxVertexInputData) / uiInputStride;

		// Dxbx addition : Don't update pDrawContext.dwVertexCount because an indexed draw
		// can (and will) use less vertices than the supplied nr of indexes. Thix fixes
		// the missing parts in the CompressedVertices sample (in Vertex shader mode).
		uiOutputStride = (pStreamPatch != nullptr) ? max(pStreamPatch->ConvertedStride, uiInputStride) : uiInputStride;
		dwOutputSize = uiVertexCount * uiOutputStride;

		g_pD3DDevice8->CreateVertexBuffer(dwOutputSize, 0, 0, XTL::D3DPOOL_MANAGED, &pHostVertexBuffer);
        if(FAILED(pHostVertexBuffer->Lock(0, 0, &pOutputData, D3DLOCK_DISCARD)))
            CxbxKrnlCleanup("Couldn't lock the new buffer");
        
		bNeedStreamCopy = true;
		if (!pStream->pXboxVertexData) {
			// The stream was not previously patched, we'll need this when restoring
			pStream->pXboxVertexData = pXboxVertexInputData;
		}

		// We'll need this when restoring
        // pStream->pOriginalStream = pOrigVertexBuffer;
    }

	if (bNeedVertexPatching) {
		for (uint32 uiVertex = 0; uiVertex < uiVertexCount; uiVertex++) {
			uint08 *pOrigVertex = &pXboxVertexInputData[uiVertex * uiInputStride];
			uint08 *pNewDataPos = &pOutputData[uiVertex * uiOutputStride];
			for (UINT uiType = 0; uiType < pStreamPatch->NbrTypes; uiType++) {
				// Dxbx note : The following code handles only the D3DVSDT enums that need conversion;
				// All other cases are catched by the memcpy in the default-block.
				switch (pStreamPatch->pTypes[uiType]) {
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
					memcpy(pNewDataPos, pOrigVertex, pStreamPatch->pSizes[uiType]);
					pOrigVertex += pStreamPatch->pSizes[uiType];
					break;
				}
				} // switch

				// Increment the new pointer :
				pNewDataPos += pStreamPatch->pSizes[uiType];
			}
		}
	} else {
		if (bNeedStreamCopy) {
			memcpy(pOutputData, pXboxVertexInputData, dwOutputSize);
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
			FLOAT *pVertexDataAsFloat = (FLOAT*)(&pOutputData[uiOutputStride * uiVertex]);

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
			if (bNeedTextureNormalization) {
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

	if(pDrawContext->pVertexStreamZeroData != NULL) {
        pDrawContext->pVertexStreamZeroData = pOutputData;
        pDrawContext->uiVertexStreamZeroStride = uiOutputStride;
    }
	else {
		pHostVertexBuffer->Unlock();
		if (FAILED(g_pD3DDevice8->SetStreamSource(uiStream, pHostVertexBuffer, uiOutputStride)))
			CxbxKrnlCleanup("Failed to set the type patched buffer as the new stream source!\n");

		if (pStream->pHostVertexBuffer != nullptr)
			// The stream was already primitive patched, release the previous vertex buffer to avoid memory leaks
			pStream->pHostVertexBuffer->Release();

		pStream->pHostVertexBuffer = pHostVertexBuffer;
	}

    pStream->uiOrigStride = uiInputStride;
    pStream->uiNewStride = uiOutputStride;
    m_bPatched = true;
}

bool XTL::CxbxVertexBufferConverter::Apply(CxbxDrawContext *pDrawContext)
{
	if ((pDrawContext->XboxPrimitiveType < X_D3DPT_POINTLIST) || (pDrawContext->XboxPrimitiveType > X_D3DPT_POLYGON))
		CxbxKrnlCleanup("Unknown primitive type: 0x%.02X\n", pDrawContext->XboxPrimitiveType);

	bool bFatalError = false;

	// Determine the number of streams of a patch (was GetNbrStreams)
	m_uiNbrStreams = 0;
	if (pDrawContext->hVertexShader > 0)
	{
		m_uiNbrStreams = 1; // Could be more, but it doesn't matter as long as we're not going to patch the types
		if (VshHandleIsVertexShader(pDrawContext->hVertexShader)) {
			// VshGetVertexDynamicPatch
			X_D3DVertexShader *pD3DVertexShader = VshHandleGetVertexShader(pDrawContext->hVertexShader);
			CxbxVertexShader *pVertexShader = (CxbxVertexShader *)pD3DVertexShader->Handle;
			CxbxVertexDynamicPatch *pDynamicPatch = &(pVertexShader->VertexDynamicPatch);
			for (uint32 i = 0; i < pDynamicPatch->NbrStreams; i++) {
				if (pDynamicPatch->pStreamPatches[i].NeedPatch) {
					m_pDynamicPatch = pDynamicPatch;
					m_uiNbrStreams = pDynamicPatch->NbrStreams;
					break;
				}
			}
		}
	}

    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++) {
#if 0
		if(ApplyCachedStream(pDrawContext, uiStream, &bFatalError)) {
            m_pStreams[uiStream].bUsedCached = true;
            continue;
        }
#endif
		ConvertStream(pDrawContext, uiStream);

#if 0
		//if(pDrawContext->pVertexStreamZeroData == NULL)
		{
			// Insert the patched stream in the cache
			CacheStream(pDrawContext, uiStream);
			m_pStreams[uiStream].bUsedCached = true;
		}
#endif
    }
        
	// Update the primitive of the stream
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
		pDrawContext->XboxPrimitiveType = X_D3DPT_TRIANGLESTRIP;
	}

	if (pDrawContext->XboxPrimitiveType == X_D3DPT_POLYGON) {
		// Convex polygon is the same as a triangle fan.
		// No need to set : pDrawContext->XboxPrimitiveType = X_D3DPT_TRIANGLEFAN;
		LOG_TEST_CASE("X_D3DPT_POLYGON");
	}

	pDrawContext->dwPrimitiveCount = EmuD3DVertex2PrimitiveCount(pDrawContext->XboxPrimitiveType, pDrawContext->dwVertexCount);

	return bFatalError;
}

void XTL::CxbxVertexBufferConverter::Restore()
{
    if(!this->m_bPatched)
        return;

    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++)
    {
#if 0 // TODO : Restore previous stream?
		if(m_pStreams[uiStream].pOriginalStream != nullptr && m_pStreams[uiStream].pHostVertexBuffer != nullptr)
        {
            g_pD3DDevice8->SetStreamSource(0, m_pStreams[uiStream].pOriginalStream, m_pStreams[uiStream].uiOrigStride);
        }

        if(m_pStreams[uiStream].pOriginalStream != nullptr)
        {
            // Release the reference to original stream we got via GetStreamSource() :
            UINT a = m_pStreams[uiStream].pOriginalStream->Release();
			/* TODO : Although correct, this currently leads to a null-pointer exception :
            if (a == 0)
                m_pStreams[uiStream].pOriginalStream = nullptr;*/
        }
#endif
        if(m_pStreams[uiStream].pHostVertexBuffer != nullptr)
        {
            UINT b = m_pStreams[uiStream].pHostVertexBuffer->Release();
			/* TODO : Although correct, this currently leads to a null-pointer exception :
			if (b == 0)
                m_pStreams[uiStream].pHostVertexBuffer = nullptr;*/
        }

		if (m_pStreams[uiStream].bUsedCached) {
			m_pStreams[uiStream].bUsedCached = false;
		} else {
            if(this->m_bAllocatedStreamZeroData) {
                free(m_pNewVertexStreamZeroData);
				// Cleanup, just to be sure :
				m_pNewVertexStreamZeroData = nullptr;
				this->m_bAllocatedStreamZeroData = false;
            }
        }
    }

    return;
}

VOID XTL::EmuFlushIVB()
{
    XTL::DxbxUpdateDeferredStates();

    // Parse IVB table with current FVF shader if possible.
    boolean bFVF = VshHandleIsFVF(g_CurrentVertexShader);
    DWORD dwCurFVF;
    if(bFVF && ((g_CurrentVertexShader & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW))
    {
        dwCurFVF = g_CurrentVertexShader;

		// HACK: Halo...
		if(dwCurFVF == 0)
		{
			EmuWarning("EmuFlushIVB(): using g_InlineVertexBuffer_FVF instead of current FVF!");
			dwCurFVF = g_InlineVertexBuffer_FVF;
		}
    }
    else
    {
        dwCurFVF = g_InlineVertexBuffer_FVF;
    }

    DbgPrintf("g_InlineVertexBuffer_TableOffset := %d\n", g_InlineVertexBuffer_TableOffset);

    // Do this once, not inside the for-loop :
    DWORD dwPos = dwCurFVF & D3DFVF_POSITION_MASK;
	DWORD dwTexN = (dwCurFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

	// Use a tooling function to determine the vertex stride :
	UINT uiStride = DxbxFVFToVertexSizeInBytes(dwCurFVF, /*bIncludeTextures=*/true);

	// Make sure the output buffer is big enough 
	UINT NeededSize = uiStride * g_InlineVertexBuffer_TableOffset;
	if (g_InlineVertexBuffer_DataSize < NeededSize) {
		g_InlineVertexBuffer_DataSize = NeededSize;
		if (g_InlineVertexBuffer_pData != nullptr)
			free(g_InlineVertexBuffer_pData);
		g_InlineVertexBuffer_pData = (FLOAT*)malloc(g_InlineVertexBuffer_DataSize);
	}

	FLOAT *pVertexBufferData = g_InlineVertexBuffer_pData;
	for(uint v=0;v<g_InlineVertexBuffer_TableOffset;v++)
    {
		*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.x;
        *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.y;
        *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Position.z;

        if(dwPos == D3DFVF_XYZRHW)
        {
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Rhw;

            DbgPrintf("IVB Position := {%f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Rhw);
        }
		else // XYZRHW cannot be combined with NORMAL, but the other XYZ formats can :
		{
			if (dwPos == D3DFVF_XYZ)
			{
				DbgPrintf("IVB Position := {%f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z);
			}
			else if (dwPos == D3DFVF_XYZB1)
			{
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;

				DbgPrintf("IVB Position := {%f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1);
			}
			else if (dwPos == D3DFVF_XYZB2)
			{
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2);
			}
			else if (dwPos == D3DFVF_XYZB3)
			{
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend3;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2, g_InlineVertexBuffer_Table[v].Blend3);
			}
			else if (dwPos == D3DFVF_XYZB4)
			{
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend1;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend2;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend3;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Blend4;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f, %f}\n", g_InlineVertexBuffer_Table[v].Position.x, g_InlineVertexBuffer_Table[v].Position.y, g_InlineVertexBuffer_Table[v].Position.z, g_InlineVertexBuffer_Table[v].Blend1, g_InlineVertexBuffer_Table[v].Blend2, g_InlineVertexBuffer_Table[v].Blend3, g_InlineVertexBuffer_Table[v].Blend4);
			}
			else // 0 or D3DFVF_XYZB5
			{
				CxbxKrnlCleanup("Unsupported Position Mask (FVF := 0x%.08X dwPos := 0x%.08X)", dwCurFVF, dwPos);
			}

			if (dwCurFVF & D3DFVF_NORMAL)
			{
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

        if(dwCurFVF & D3DFVF_DIFFUSE)
        {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Diffuse;

            DbgPrintf("IVB Diffuse := 0x%.08X\n", g_InlineVertexBuffer_Table[v].Diffuse);
        }

        if(dwCurFVF & D3DFVF_SPECULAR)
        {
            *(DWORD*)pVertexBufferData++ = g_InlineVertexBuffer_Table[v].Specular;

            DbgPrintf("IVB Specular := 0x%.08X\n", g_InlineVertexBuffer_Table[v].Specular);
        }

		// TODO -oDxbx : Handle other sizes than D3DFVF_TEXCOORDSIZE2 too!
		// See D3DTSS_TEXTURETRANSFORMFLAGS values other than D3DTTFF_COUNT2
		// See and/or X_D3DVSD_DATATYPEMASK values other than D3DVSDT_FLOAT2
		if(dwTexN >= 1)
        {
			// DxbxFVF_GetTextureSize
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.x;
            *pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.y;
			//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord1.z;

            DbgPrintf("IVB TexCoord1 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord1.x, g_InlineVertexBuffer_Table[v].TexCoord1.y);

			if(dwTexN >= 2)
			{
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.x;
				*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.y;
				//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord2.z;

				DbgPrintf("IVB TexCoord2 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord2.x, g_InlineVertexBuffer_Table[v].TexCoord2.y);

				if(dwTexN >= 3)
				{
					*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.x;
					*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.y;
					//*pVertexBufferData++ = g_InlineVertexBuffer_Table[v].TexCoord3.z;

					DbgPrintf("IVB TexCoord3 := {%f, %f}\n", g_InlineVertexBuffer_Table[v].TexCoord3.x, g_InlineVertexBuffer_Table[v].TexCoord3.y);

					if(dwTexN >= 4)
					{
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
			if (VertexBufferUsage != uiStride)
				CxbxKrnlCleanup("EmuFlushIVB uses wrong stride!");
		}
	}

    CxbxDrawContext DrawContext;

    DrawContext.XboxPrimitiveType = g_InlineVertexBuffer_PrimitiveType;
    DrawContext.dwVertexCount = g_InlineVertexBuffer_TableOffset;
	DrawContext.dwPrimitiveCount = 0;
	DrawContext.dwStartVertex = 0;
    DrawContext.pVertexStreamZeroData = g_InlineVertexBuffer_pData;
    DrawContext.uiVertexStreamZeroStride = uiStride;
    DrawContext.hVertexShader = g_CurrentVertexShader;

    // Disable this 'fix', as it doesn't really help; On ATI, it isn't needed (and causes missing
    // textures if enabled). On Nvidia, it stops the jumping (but also removes the font from view).
    // So I think it's better to keep this bug visible, as a motivation for a real fix, and better
    // rendering on ATI chipsets...

//    bFVF = true; // This fixes jumping triangles on Nvidia chipsets, as suggested by Defiance
    // As a result however, this change also seems to remove the texture of the fonts in XSokoban!?!

    if(bFVF)
        g_pD3DDevice8->SetVertexShader(dwCurFVF);

	CxbxDrawPrimitiveUP(DrawContext);

    if(bFVF)
        g_pD3DDevice8->SetVertexShader(g_CurrentVertexShader);

    return;
}
