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

// inline vertex buffer emulation
XTL::DWORD                  *XTL::g_pIVBVertexBuffer = nullptr;
XTL::X_D3DPRIMITIVETYPE      XTL::g_IVBPrimitiveType = XTL::X_D3DPT_INVALID;
UINT                         XTL::g_IVBTblOffs = 0;
struct XTL::_D3DIVB         *XTL::g_IVBTable = nullptr;
extern DWORD                 XTL::g_IVBFVF = 0;

extern DWORD				XTL::g_dwPrimPerFrame = 0;

XTL::VertexPatcher::VertexPatcher()
{
    this->m_uiNbrStreams = 0;
    ZeroMemory(this->m_pStreams, sizeof(PATCHEDSTREAM) * MAX_NBR_STREAMS);
    this->m_bPatched = false;
    this->m_bAllocatedStreamZeroData = false;
    this->m_pNewVertexStreamZeroData = NULL;
    this->m_pDynamicPatch = NULL;
}

XTL::VertexPatcher::~VertexPatcher()
{
}

void XTL::VertexPatcher::DumpCache(void)
{
    printf("--- Dumping streams cache ---\n");
    RTNode *pNode = g_PatchedStreamsCache.getHead();
    while(pNode != nullptr)
    {
        CACHEDSTREAM *pCachedStream = (CACHEDSTREAM *)pNode->pResource;
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

void XTL::VertexPatcher::CacheStream(VertexPatchDesc *pPatchDesc,
                                     UINT             uiStream)
{
    //UINT                       uiStride;
    IDirect3DVertexBuffer8    *pOrigVertexBuffer = nullptr;
    void                      *pCalculateData = NULL;
    uint32                     uiKey;
    UINT                       uiLength;
    CACHEDSTREAM              *pCachedStream = (CACHEDSTREAM *)calloc(1, sizeof(CACHEDSTREAM));

    // Check if the cache is full, if so, throw away the least used stream
    if(g_PatchedStreamsCache.get_count() > VERTEX_BUFFER_CACHE_SIZE)
    {
        uint32 uiKey = 0;
        uint32 uiMinHit = 0xFFFFFFFF;

        RTNode *pNode = g_PatchedStreamsCache.getHead();
        while(pNode != nullptr)
        {
            if(pNode->pResource != nullptr)
            {
                // First, check if there is an "expired" stream in the cache (not recently used)
                if(((CACHEDSTREAM *)pNode->pResource)->lLastUsed < (clock() + MAX_STREAM_NOT_USED_TIME))
                {
                    printf("!!!Found an old stream, %2.2f\n", ((FLOAT)((clock() + MAX_STREAM_NOT_USED_TIME) - ((CACHEDSTREAM *)pNode->pResource)->lLastUsed)) / (FLOAT)CLOCKS_PER_SEC);
                    uiKey = pNode->uiKey;
                    break;
                }

                // Find the least used cached stream
                if((uint32)((CACHEDSTREAM *)pNode->pResource)->uiCacheHit < uiMinHit)
                {
                    uiMinHit = ((CACHEDSTREAM *)pNode->pResource)->uiCacheHit;
                    uiKey = pNode->uiKey;
                }
            }

            pNode = pNode->pNext;
        }

        if(uiKey != 0)
        {
            printf("!!!Removing stream\n\n");
            FreeCachedStream((void*)uiKey);
        }
    }

    // Start the actual stream caching
    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
		XTL::D3DVERTEXBUFFER_DESC  Desc;

        pOrigVertexBuffer = m_pStreams[uiStream].pOriginalStream;
        pOrigVertexBuffer->AddRef();
        m_pStreams[uiStream].pPatchedStream->AddRef();
        if(FAILED(pOrigVertexBuffer->GetDesc(&Desc)))
            CxbxKrnlCleanup("Could not retrieve original buffer size");

		if(FAILED(pOrigVertexBuffer->Lock(0, 0, (uint08**)&pCalculateData, 0)))
            CxbxKrnlCleanup("Couldn't lock the original buffer");

        uiLength = Desc.Size;
        pCachedStream->bIsUP = false;
        uiKey = (uint32)pOrigVertexBuffer;
    }
    else
    {
        // There should only be one stream (stream zero) in this case
        if(uiStream != 0)
            CxbxKrnlCleanup("Trying to patch a Draw..UP with more than stream zero!");

		//uiStride  = pPatchDesc->uiVertexStreamZeroStride;
        pCalculateData = (uint08 *)pPatchDesc->pVertexStreamZeroData;
        // TODO: This is sometimes the number of indices, which isn't too good
        uiLength = pPatchDesc->dwVertexCount * pPatchDesc->uiVertexStreamZeroStride;
        pCachedStream->bIsUP = true;
        pCachedStream->pStreamUP = pCalculateData;
        uiKey = (uint32)pCalculateData;
    }

    uint32_t uiHash = XXHash32::hash((void *)pCalculateData, uiLength, HASH_SEED);
    if(pOrigVertexBuffer != nullptr)
        pOrigVertexBuffer->Unlock();

    pCachedStream->uiHash = uiHash;
    pCachedStream->Stream = m_pStreams[uiStream];
    pCachedStream->uiCheckFrequency = 1; // Start with checking every 1th Draw..
    pCachedStream->uiCheckCount = 0;
    pCachedStream->uiLength = uiLength;
    pCachedStream->uiCacheHit = 0;
    pCachedStream->dwPrimitiveCount = pPatchDesc->dwPrimitiveCount;
    pCachedStream->lLastUsed = clock();

    g_PatchedStreamsCache.insert(uiKey, pCachedStream);
}

void XTL::VertexPatcher::FreeCachedStream(void *pStream)
{
    g_PatchedStreamsCache.Lock();

	CACHEDSTREAM *pCachedStream = (CACHEDSTREAM *)g_PatchedStreamsCache.get(pStream);
    if(pCachedStream != nullptr)
    {
        if(pCachedStream->bIsUP && pCachedStream->pStreamUP)
            free(pCachedStream->pStreamUP);

		if(pCachedStream->Stream.pOriginalStream)
            pCachedStream->Stream.pOriginalStream->Release();

		if(pCachedStream->Stream.pPatchedStream)
            pCachedStream->Stream.pPatchedStream->Release();

		free(pCachedStream);
    }

    g_PatchedStreamsCache.Unlock();
    g_PatchedStreamsCache.remove(pStream);
}

bool XTL::VertexPatcher::ApplyCachedStream(VertexPatchDesc *pPatchDesc,
                                           UINT             uiStream,
										   bool			   *pbFatalError)
{
    UINT                       uiStride;
    IDirect3DVertexBuffer8    *pOrigVertexBuffer = nullptr;
    void                      *pCalculateData = NULL;
    UINT                       uiLength;
    bool                       bApplied = false;
    uint32                     uiKey;
    //CACHEDSTREAM              *pCachedStream = (CACHEDSTREAM *)malloc(sizeof(CACHEDSTREAM));

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
        g_pD3DDevice8->GetStreamSource(
			uiStream, 
			&pOrigVertexBuffer, 
			&uiStride);
        if(!pOrigVertexBuffer)
		{
			if(pbFatalError)
				*pbFatalError = true;

			return false;
		}

		XTL::D3DVERTEXBUFFER_DESC  Desc;
		if(FAILED(pOrigVertexBuffer->GetDesc(&Desc)))
            CxbxKrnlCleanup("Could not retrieve original buffer size");

        uiLength = Desc.Size;
        uiKey = (uint32)pOrigVertexBuffer;
        //pCachedStream->bIsUP = false;
    }
    else
    {
        // There should only be one stream (stream zero) in this case
        if(uiStream != 0)
            CxbxKrnlCleanup("Trying to find a cached Draw..UP with more than stream zero!");

        uiStride  = pPatchDesc->uiVertexStreamZeroStride;
        pCalculateData = (uint08 *)pPatchDesc->pVertexStreamZeroData;
        // TODO: This is sometimes the number of indices, which isn't too good
        uiLength = pPatchDesc->dwVertexCount * pPatchDesc->uiVertexStreamZeroStride;
        uiKey = (uint32)pCalculateData;
        //pCachedStream->bIsUP = true;
        //pCachedStream->pStreamUP = pCalculateData;
    }

    g_PatchedStreamsCache.Lock();

    CACHEDSTREAM *pCachedStream = (CACHEDSTREAM *)g_PatchedStreamsCache.get(uiKey);
    if(pCachedStream != nullptr)
    {
        pCachedStream->lLastUsed = clock();
        pCachedStream->uiCacheHit++;
        bool bMismatch = false;
        if(pCachedStream->uiCheckCount == (pCachedStream->uiCheckFrequency - 1))
        {
            if(pOrigVertexBuffer != nullptr)
                if(FAILED(pOrigVertexBuffer->Lock(0, 0, (uint08**)&pCalculateData, 0)))
                    CxbxKrnlCleanup("Couldn't lock the original buffer");

            // Use the cached stream length (which is a must for the UP stream)
            uint32_t uiHash = XXHash32::hash((void *)pCalculateData, pCachedStream->uiLength, HASH_SEED);
            if(uiHash == pCachedStream->uiHash)
            {
                // Take a while longer to check
                if(pCachedStream->uiCheckFrequency < 32*1024)
                    pCachedStream->uiCheckFrequency *= 2;

				pCachedStream->uiCheckCount = 0;
            }
            else
            {
                // TODO: Do something about this
                if(pCachedStream->bIsUP)
                {
                    FreeCachedStream(pCachedStream->pStreamUP);
                }
                else
                {
                    FreeCachedStream(pCachedStream->Stream.pOriginalStream);
                }
                pCachedStream = NULL;
                bMismatch = true;
            }

            if(pOrigVertexBuffer != nullptr)
                pOrigVertexBuffer->Unlock();
        }
        else
        {
            pCachedStream->uiCheckCount++;
        }

        if(!bMismatch)
        {
            if(!pCachedStream->bIsUP)
            {
                m_pStreams[uiStream].pOriginalStream = pOrigVertexBuffer;
                m_pStreams[uiStream].uiOrigStride = uiStride;
                g_pD3DDevice8->SetStreamSource(uiStream, pCachedStream->Stream.pPatchedStream, pCachedStream->Stream.uiNewStride);
                pCachedStream->Stream.pPatchedStream->AddRef();
                pCachedStream->Stream.pOriginalStream->AddRef();
                m_pStreams[uiStream].pPatchedStream = pCachedStream->Stream.pPatchedStream;
                m_pStreams[uiStream].uiNewStride = pCachedStream->Stream.uiNewStride;
            }
            else
            {
                pPatchDesc->pVertexStreamZeroData = pCachedStream->pStreamUP;
                pPatchDesc->uiVertexStreamZeroStride = pCachedStream->Stream.uiNewStride;
            }

            if(pCachedStream->dwPrimitiveCount > 0)
            {
                // The primitives were patched, draw with the correct number of primimtives from the cache
                pPatchDesc->dwPrimitiveCount = pCachedStream->dwPrimitiveCount;
            }

            bApplied = true;
            m_bPatched = true;
        }
    }

    g_PatchedStreamsCache.Unlock();

    if(pOrigVertexBuffer != nullptr)
        pOrigVertexBuffer->Release();

    return bApplied;
}

UINT XTL::VertexPatcher::GetNbrStreams(VertexPatchDesc *pPatchDesc)
{
    if(VshHandleIsVertexShader(pPatchDesc->hVertexShader))
    {
        VERTEX_DYNAMIC_PATCH *pDynamicPatch = VshGetVertexDynamicPatch(pPatchDesc->hVertexShader);
        if(pDynamicPatch)
            return pDynamicPatch->NbrStreams;

		return 1; // Could be more, but it doesn't matter as we're not going to patch the types
    }

	if(pPatchDesc->hVertexShader)
		return 1;

	return 0;
}

bool XTL::VertexPatcher::PatchStream(VertexPatchDesc *pPatchDesc,
                                     UINT             uiStream)
{
    // FVF buffers doesn't have Xbox extensions, but texture coordinates may
    // need normalization if used with linear textures.
    if(VshHandleIsFVF(pPatchDesc->hVertexShader))
    {
        if(pPatchDesc->hVertexShader & D3DFVF_TEXCOUNT_MASK)
        {
            return NormalizeTexCoords(pPatchDesc, uiStream);
        }
        else
        {
            return false;
        }
    }

    if(!m_pDynamicPatch || !m_pDynamicPatch->pStreamPatches[uiStream].NeedPatch)
    {
        return false;
    }

    // Do some groovy patchin'
    
    IDirect3DVertexBuffer8    *pOrigVertexBuffer;
    IDirect3DVertexBuffer8    *pNewVertexBuffer;
    uint08                    *pOrigData;
    uint08                    *pNewData;
	UINT                       uiVertexCount;
    UINT                       uiStride;
    PATCHEDSTREAM             *pStream = &m_pStreams[uiStream];
    STREAM_DYNAMIC_PATCH      *pStreamPatch = &m_pDynamicPatch->pStreamPatches[uiStream];
    DWORD dwNewSize;

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
		XTL::D3DVERTEXBUFFER_DESC  Desc;

        g_pD3DDevice8->GetStreamSource(
			uiStream, 
			&pOrigVertexBuffer, 
			&uiStride);
        if(FAILED(pOrigVertexBuffer->GetDesc(&Desc)))
            CxbxKrnlCleanup("Could not retrieve original buffer size");

		// Set a new (exact) vertex count
		uiVertexCount = Desc.Size / uiStride;
		// Dxbx addition : Don't update pPatchDesc.dwVertexCount because an indexed draw
		// can (and will) use less vertices than the supplied nr of indexes. Thix fixes
		// the missing parts in the CompressedVertices sample (in Vertex shader mode).
		pStreamPatch->ConvertedStride = max(pStreamPatch->ConvertedStride, uiStride); // ??
		dwNewSize = uiVertexCount * pStreamPatch->ConvertedStride;

        if(FAILED(pOrigVertexBuffer->Lock(0, 0, &pOrigData, 0)))
            CxbxKrnlCleanup("Couldn't lock the original buffer");

		g_pD3DDevice8->CreateVertexBuffer(dwNewSize, 0, 0, XTL::D3DPOOL_MANAGED, &pNewVertexBuffer);
        if(FAILED(pNewVertexBuffer->Lock(0, 0, &pNewData, 0)))
            CxbxKrnlCleanup("Couldn't lock the new buffer");
        
		if(!pStream->pOriginalStream)
        {
            // The stream was not previously patched, we'll need this when restoring
            pStream->pOriginalStream = pOrigVertexBuffer;
        }
    }
    else
    {
        // There should only be one stream (stream zero) in this case
        if(uiStream != 0)
            CxbxKrnlCleanup("Trying to patch a Draw..UP with more than stream zero!");

		uiStride  = pPatchDesc->uiVertexStreamZeroStride;
		pStreamPatch->ConvertedStride = max(pStreamPatch->ConvertedStride, uiStride); // ??
		pOrigData = (uint08 *)pPatchDesc->pVertexStreamZeroData;
        // TODO: This is sometimes the number of indices, which isn't too good
		uiVertexCount = pPatchDesc->dwVertexCount;
        dwNewSize = uiVertexCount * pStreamPatch->ConvertedStride;
        pNewVertexBuffer = NULL;
        pNewData = (uint08*)g_MemoryManager.Allocate(dwNewSize);
        if(pNewData == nullptr)
            CxbxKrnlCleanup("Couldn't allocate the new stream zero buffer");
    }

	for (uint32 uiVertex = 0; uiVertex < uiVertexCount; uiVertex++)
	{
		uint08 *pOrigVertex = &pOrigData[uiVertex * uiStride];
		uint08 *pNewDataPos = &pNewData[uiVertex * pStreamPatch->ConvertedStride];
		for (UINT uiType = 0; uiType < pStreamPatch->NbrTypes; uiType++)
		{
			// Dxbx note : The following code handles only the D3DVSDT enums that need conversion;
			// All other cases are catched by the memcpy in the default-block.
			switch (pStreamPatch->pTypes[uiType])
			{
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
				// UNTESTED - Need test-case!
				((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
				//((FLOAT *)pNewDataPos)[1] = 0.0f; // Would be needed for FLOAT2
				pOrigVertex += 1 * sizeof(SHORT);
				break;
			}
#if !DXBX_USE_D3D9 // No need for patching in D3D9
			case X_D3DVSDT_NORMSHORT2: { // 0x21: // Make it FLOAT2
				// UNTESTED - Need test-case!
				((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
				((FLOAT *)pNewDataPos)[1] = ((FLOAT)((SHORT*)pOrigVertex)[1]) / 32767.0f;
				pOrigVertex += 2 * sizeof(SHORT);
				break;
			}
#endif
			case X_D3DVSDT_NORMSHORT3: { // 0x31: // Make it FLOAT3
				// UNTESTED - Need test-case!
				((FLOAT *)pNewDataPos)[0] = ((FLOAT)((SHORT*)pOrigVertex)[0]) / 32767.0f;
				((FLOAT *)pNewDataPos)[1] = ((FLOAT)((SHORT*)pOrigVertex)[1]) / 32767.0f;
				((FLOAT *)pNewDataPos)[2] = ((FLOAT)((SHORT*)pOrigVertex)[2]) / 32767.0f;
				pOrigVertex += 3 * sizeof(SHORT);
				break;
			}
#if !DXBX_USE_D3D9 // No need for patching in D3D9
			case X_D3DVSDT_NORMSHORT4: { // 0x41: // Make it FLOAT4
				// UNTESTED - Need test-case!
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

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
        //if(pNewVertexBuffer != nullptr) // Dxbx addition
			pNewVertexBuffer->Unlock();
		//if (pOrigVertexBuffer != nullptr) // Dxbx addition
			pOrigVertexBuffer->Unlock();

        if(FAILED(g_pD3DDevice8->SetStreamSource(uiStream, pNewVertexBuffer, pStreamPatch->ConvertedStride)))
            CxbxKrnlCleanup("Failed to set the type patched buffer as the new stream source!\n");

		if(pStream->pPatchedStream)
            // The stream was already primitive patched, release the previous vertex buffer to avoid memory leaks
            pStream->pPatchedStream->Release();

		pStream->pPatchedStream = pNewVertexBuffer;
    }
    else
    {
        pPatchDesc->pVertexStreamZeroData = pNewData;
        pPatchDesc->uiVertexStreamZeroStride = pStreamPatch->ConvertedStride;
        if(!m_bAllocatedStreamZeroData)
        {
            // The stream was not previously patched. We'll need this when restoring
            m_bAllocatedStreamZeroData = true;
            m_pNewVertexStreamZeroData = pNewData;
        }
    }

    pStream->uiOrigStride = uiStride;
    pStream->uiNewStride = pStreamPatch->ConvertedStride;
    m_bPatched = true;

    return true;
}

bool XTL::VertexPatcher::NormalizeTexCoords(VertexPatchDesc *pPatchDesc, UINT uiStream)
{
    // Check for active linear textures.
	bool bHasLinearTex = false;
	struct { bool bTexIsLinear; int Width; int Height; } pActivePixelContainer[TEXTURE_STAGES] = { 0 };

    for(uint i = 0; i < TEXTURE_STAGES; i++)
    {
		X_D3DBaseTexture *pXboxBaseTexture = EmuD3DTextureStages[i];
		if (pXboxBaseTexture)
		{ 
			// TODO : Use GetXboxPixelContainerFormat
			XTL::X_D3DFORMAT XBFormat = (XTL::X_D3DFORMAT)((pXboxBaseTexture->Format & X_D3DFORMAT_FORMAT_MASK) >> X_D3DFORMAT_FORMAT_SHIFT);
			if (EmuXBFormatIsLinear(XBFormat))
			{
				// This is often hit by the help screen in XDK samples.
				bHasLinearTex = true;
				// Remember linearity, width and height :
				pActivePixelContainer[i].bTexIsLinear = true;
				// TODO : Use DecodeD3DSize
				pActivePixelContainer[i].Width = (pXboxBaseTexture->Size & X_D3DSIZE_WIDTH_MASK) + 1;
				pActivePixelContainer[i].Height = ((pXboxBaseTexture->Size & X_D3DSIZE_HEIGHT_MASK) >> X_D3DSIZE_HEIGHT_SHIFT) + 1;
			}
        }
    }

    if(!bHasLinearTex)
        return false;

	uint uiStride;
	uint uiVertexCount;
	uint08 *pData;
    IDirect3DVertexBuffer8 *pNewVertexBuffer;
    PATCHEDSTREAM *pStream;

    if(pPatchDesc->pVertexStreamZeroData != nullptr)
    {
        // In-place patching of inline buffer.
        uiStride = pPatchDesc->uiVertexStreamZeroStride;
        uiVertexCount = pPatchDesc->dwVertexCount;
        pData = (uint08 *)pPatchDesc->pVertexStreamZeroData;
        pNewVertexBuffer = nullptr;
		pStream = nullptr;
    }
    else
    {
        // Copy stream for patching and caching.
		IDirect3DVertexBuffer8 *pOrigVertexBuffer;
        XTL::D3DVERTEXBUFFER_DESC Desc;

        g_pD3DDevice8->GetStreamSource(uiStream, &pOrigVertexBuffer, &uiStride);
        if(!pOrigVertexBuffer || FAILED(pOrigVertexBuffer->GetDesc(&Desc)))
            CxbxKrnlCleanup("Could not retrieve original FVF buffer size.");

		uiVertexCount = Desc.Size / uiStride;
        
        uint08 *pOrigData;
        if(FAILED(pOrigVertexBuffer->Lock(0, 0, &pOrigData, 0)))
            CxbxKrnlCleanup("Couldn't lock original FVF buffer.");

		g_pD3DDevice8->CreateVertexBuffer(Desc.Size, 0, 0, XTL::D3DPOOL_MANAGED, &pNewVertexBuffer);
        if(FAILED(pNewVertexBuffer->Lock(0, 0, &pData, 0)))
            CxbxKrnlCleanup("Couldn't lock new FVF buffer.");

		memcpy(pData, pOrigData, Desc.Size);
        pOrigVertexBuffer->Unlock();

        pStream = &m_pStreams[uiStream];
        if(!pStream->pOriginalStream)
        {
            pStream->pOriginalStream = pOrigVertexBuffer;
        }
    }

    // Normalize texture coordinates.
    DWORD dwTexN = (pPatchDesc->hVertexShader & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	// Don't normalize coordinates not used by the shader :
	while (dwTexN < TEXTURE_STAGES) // X_D3DTS_STAGECOUNT
	{
		pActivePixelContainer[dwTexN].bTexIsLinear = false;
		dwTexN++;
	}

	// Locate texture coordinate offset in vertex structure.
	UINT uiOffset = DxbxFVFToVertexSizeInBytes(pPatchDesc->hVertexShader, /*bIncludeTextures=*/false);
	FLOAT *pUVData = (FLOAT*)(pData + uiOffset);

	for(uint uiVertex = 0; uiVertex < uiVertexCount; uiVertex++)
    {
        if(pActivePixelContainer[0].bTexIsLinear)
        {
            pUVData[0] /= pActivePixelContainer[0].Width;
            pUVData[1] /= pActivePixelContainer[0].Height;
        }

		if (pActivePixelContainer[1].bTexIsLinear)
		{
            pUVData[2] /= pActivePixelContainer[1].Width;
            pUVData[3] /= pActivePixelContainer[1].Height;
        }

		if (pActivePixelContainer[2].bTexIsLinear)
		{
            pUVData[4] /= pActivePixelContainer[2].Width;
            pUVData[5] /= pActivePixelContainer[2].Height;
        }

		if (pActivePixelContainer[3].bTexIsLinear)
		{
            pUVData[6] /= pActivePixelContainer[3].Width;
            pUVData[7] /= pActivePixelContainer[3].Height;
        }


		pUVData = (FLOAT *)((uint08*)pUVData + uiStride);
    }

    if(pNewVertexBuffer != nullptr)
    {
        pNewVertexBuffer->Unlock();

        if(FAILED(g_pD3DDevice8->SetStreamSource(uiStream, pNewVertexBuffer, uiStride)))
            CxbxKrnlCleanup("Failed to set the texcoord patched FVF buffer as the new stream source.");

		if(pStream->pPatchedStream)
            pStream->pPatchedStream->Release();

        pStream->pPatchedStream = pNewVertexBuffer;
        pStream->uiOrigStride = uiStride;
        pStream->uiNewStride = uiStride;
        m_bPatched = true;
    }

    return m_bPatched;
}

bool XTL::VertexPatcher::PatchPrimitive(VertexPatchDesc *pPatchDesc,
                                        UINT             uiStream)
{
    PATCHEDSTREAM *pStream = &m_pStreams[uiStream];

    if((pPatchDesc->XboxPrimitiveType < X_D3DPT_POINTLIST) || (pPatchDesc->XboxPrimitiveType > X_D3DPT_POLYGON))
        CxbxKrnlCleanup("Unknown primitive type: 0x%.02X\n", pPatchDesc->XboxPrimitiveType);

    // Unsupported primitives that don't need deep patching.
    switch(pPatchDesc->XboxPrimitiveType)
    {
        // Quad strip is just like a triangle strip, but requires two
        // vertices per primitive.
        case X_D3DPT_QUADSTRIP:
            pPatchDesc->dwVertexCount -= pPatchDesc->dwVertexCount % 2;
            pPatchDesc->XboxPrimitiveType = X_D3DPT_TRIANGLESTRIP;
            break;

        // Convex polygon is the same as a triangle fan.
        case X_D3DPT_POLYGON:
            pPatchDesc->XboxPrimitiveType = X_D3DPT_TRIANGLEFAN;
            break;
    }

    pPatchDesc->dwPrimitiveCount = EmuD3DVertex2PrimitiveCount(pPatchDesc->XboxPrimitiveType, pPatchDesc->dwVertexCount);

    // Skip primitives that don't need further patching.
    switch(pPatchDesc->XboxPrimitiveType)
    {
        case X_D3DPT_QUADLIST:
			//EmuWarning("VertexPatcher::PatchPrimitive: Processing D3DPT_QUADLIST");
			break;
        case X_D3DPT_LINELOOP:
			//EmuWarning("VertexPatcher::PatchPrimitive: Processing D3DPT_LINELOOP");
            break;

        default:
            return false;
    }

	if (pPatchDesc->pVertexStreamZeroData != nullptr)
		if(uiStream > 0)
			CxbxKrnlCleanup("Draw..UP call with more than one stream!\n");

    pStream->uiOrigStride = 0;

    // sizes of our part in the vertex buffer
    DWORD dwOriginalSize    = 0;
    DWORD dwNewSize         = 0;

    // sizes with the rest of the buffer
    DWORD dwOriginalSizeWR  = 0;
    DWORD dwNewSizeWR       = 0;

    // vertex data arrays
    BYTE *pOrigVertexData = nullptr;
    BYTE *pPatchedVertexData = nullptr;

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
        g_pD3DDevice8->GetStreamSource(0, &pStream->pOriginalStream, &pStream->uiOrigStride);
        pStream->uiNewStride = pStream->uiOrigStride; // The stride is still the same
    }
    else
    {
        pStream->uiOrigStride = pPatchDesc->uiVertexStreamZeroStride;
    }

    // Quad list
    if(pPatchDesc->XboxPrimitiveType == X_D3DPT_QUADLIST)
    {
        pPatchDesc->dwPrimitiveCount *= 2;

        // This is a list of sqares/rectangles, so we convert it to a list of triangles
        dwOriginalSize  = pPatchDesc->dwPrimitiveCount * pStream->uiOrigStride * 2;
        dwNewSize       = pPatchDesc->dwPrimitiveCount * pStream->uiOrigStride * 3;
    }
    // Line loop
    else if(pPatchDesc->XboxPrimitiveType == X_D3DPT_LINELOOP)
    {
        pPatchDesc->dwPrimitiveCount += 1;

        // We will add exactly one more line
        dwOriginalSize  = pPatchDesc->dwPrimitiveCount * pStream->uiOrigStride;
        dwNewSize       = pPatchDesc->dwPrimitiveCount * pStream->uiOrigStride + pStream->uiOrigStride;
    }

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
        // Retrieve the original buffer size
        {
            XTL::D3DVERTEXBUFFER_DESC Desc;

            if(FAILED(pStream->pOriginalStream->GetDesc(&Desc)))
                CxbxKrnlCleanup("Could not retrieve buffer size");

            // Here we save the full buffer size
            dwOriginalSizeWR = Desc.Size;
            // So we can now calculate the size of the rest (dwOriginalSizeWR - dwOriginalSize) and
            // add it to our new calculated size of the patched buffer
            dwNewSizeWR = dwNewSize + dwOriginalSizeWR - dwOriginalSize;
        }

        g_pD3DDevice8->CreateVertexBuffer(dwNewSizeWR, 0, 0, XTL::D3DPOOL_MANAGED, &pStream->pPatchedStream);

		if(pStream->pOriginalStream != nullptr)
            pStream->pOriginalStream->Lock(0, 0, &pOrigVertexData, 0);

        if(pStream->pPatchedStream != nullptr)
            pStream->pPatchedStream->Lock(0, 0, &pPatchedVertexData, 0);
    }
    else
    {
        dwOriginalSizeWR = dwOriginalSize;
        dwNewSizeWR = dwNewSize;

        m_pNewVertexStreamZeroData = (uint08*)malloc(dwNewSizeWR);
        m_bAllocatedStreamZeroData = true;

        pPatchedVertexData = (uint08*)m_pNewVertexStreamZeroData;
        pOrigVertexData = (uint08*)pPatchDesc->pVertexStreamZeroData;

        pPatchDesc->pVertexStreamZeroData = pPatchedVertexData;
    }

    // Copy the nonmodified data
    memcpy(pPatchedVertexData, pOrigVertexData, pPatchDesc->dwOffset);
    memcpy(&pPatchedVertexData[pPatchDesc->dwOffset+dwNewSize],
           &pOrigVertexData[pPatchDesc->dwOffset+dwOriginalSize],
           dwOriginalSizeWR - pPatchDesc->dwOffset - dwOriginalSize);

    // Quad list
    if(pPatchDesc->XboxPrimitiveType == X_D3DPT_QUADLIST)
    {
        uint08 *pPatch1 = &pPatchedVertexData[pPatchDesc->dwOffset     * pStream->uiOrigStride];
        uint08 *pPatch2 = &pPatchedVertexData[(pPatchDesc->dwOffset + 3) * pStream->uiOrigStride];
        uint08 *pPatch3 = &pPatchedVertexData[(pPatchDesc->dwOffset + 4) * pStream->uiOrigStride];
        uint08 *pPatch4 = &pPatchedVertexData[(pPatchDesc->dwOffset + 5) * pStream->uiOrigStride];

        uint08 *pOrig1 = &pOrigVertexData[pPatchDesc->dwOffset     * pStream->uiOrigStride];
        uint08 *pOrig2 = &pOrigVertexData[(pPatchDesc->dwOffset + 2) * pStream->uiOrigStride];
        uint08 *pOrig3 = &pOrigVertexData[(pPatchDesc->dwOffset + 3) * pStream->uiOrigStride];

        for(uint32 i = 0;i < pPatchDesc->dwPrimitiveCount/2;i++)
        {
		//	__asm int 3;
		//	DbgPrintf( "pPatch1 = 0x%.08X pOrig1 = 0x%.08X pStream->uiOrigStride * 3 = 0x%.08X\n", pPatch1, pOrig1, pStream->uiOrigStride * 3 );
            memcpy(pPatch1, pOrig1, pStream->uiOrigStride * 3); // Vertex 0,1,2 := Vertex 0,1,2
		//	__asm int 3;
            memcpy(pPatch2, pOrig2, pStream->uiOrigStride);     // Vertex 3     := Vertex 2
		//	__asm int 3;
            memcpy(pPatch3, pOrig3, pStream->uiOrigStride);     // Vertex 4     := Vertex 3
		//	__asm int 3;
            memcpy(pPatch4, pOrig1, pStream->uiOrigStride);     // Vertex 5     := Vertex 0

            pPatch1 += pStream->uiOrigStride * 6;
            pPatch2 += pStream->uiOrigStride * 6;
            pPatch3 += pStream->uiOrigStride * 6;
            pPatch4 += pStream->uiOrigStride * 6;

            pOrig1 += pStream->uiOrigStride * 4;
            pOrig2 += pStream->uiOrigStride * 4;
            pOrig3 += pStream->uiOrigStride * 4;

            if(pPatchDesc->hVertexShader & D3DFVF_XYZRHW)
            {
                for(int z = 0; z < 6; z++)
                {
                    if(((FLOAT*)&pPatchedVertexData[pPatchDesc->dwOffset + i * pStream->uiOrigStride * 6 + z * pStream->uiOrigStride])[2] == 0.0f)
                        ((FLOAT*)&pPatchedVertexData[pPatchDesc->dwOffset + i * pStream->uiOrigStride * 6 + z * pStream->uiOrigStride])[2] = 1.0f;
                    if(((FLOAT*)&pPatchedVertexData[pPatchDesc->dwOffset + i * pStream->uiOrigStride * 6 + z * pStream->uiOrigStride])[3] == 0.0f)
                        ((FLOAT*)&pPatchedVertexData[pPatchDesc->dwOffset + i * pStream->uiOrigStride * 6 + z * pStream->uiOrigStride])[3] = 1.0f;
                }
            }
        }
    }
    // Line loop
    else if(pPatchDesc->XboxPrimitiveType == X_D3DPT_LINELOOP)
    {
        memcpy(&pPatchedVertexData[pPatchDesc->dwOffset], &pOrigVertexData[pPatchDesc->dwOffset], dwOriginalSize);
	    memcpy(&pPatchedVertexData[pPatchDesc->dwOffset + dwOriginalSize], &pOrigVertexData[pPatchDesc->dwOffset], pStream->uiOrigStride);
    }

    if(pPatchDesc->pVertexStreamZeroData == nullptr)
    {
        pStream->pOriginalStream->Unlock();
        pStream->pPatchedStream->Unlock();

        g_pD3DDevice8->SetStreamSource(0, pStream->pPatchedStream, pStream->uiOrigStride);
    }

    m_bPatched = true;

    return true;
}

bool XTL::VertexPatcher::Apply(VertexPatchDesc *pPatchDesc, bool *pbFatalError)
{
    bool Patched = false;
    // Get the number of streams
    m_uiNbrStreams = GetNbrStreams(pPatchDesc);
    if(VshHandleIsVertexShader(pPatchDesc->hVertexShader))
    {
        m_pDynamicPatch = &((VERTEX_SHADER *)VshHandleGetVertexShader(pPatchDesc->hVertexShader)->Handle)->VertexDynamicPatch;
    }

    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++)
    {
        bool LocalPatched = false;

        if(ApplyCachedStream(pPatchDesc, uiStream, pbFatalError))
        {
            m_pStreams[uiStream].bUsedCached = true;
            continue;
        }

        LocalPatched |= PatchPrimitive(pPatchDesc, uiStream);
        LocalPatched |= PatchStream(pPatchDesc, uiStream);
		if (LocalPatched)
			if(pPatchDesc->pVertexStreamZeroData == nullptr)
			{
				// Insert the patched stream in the cache
				CacheStream(pPatchDesc, uiStream);
				m_pStreams[uiStream].bUsedCached = true;
			}

        Patched |= LocalPatched;
    }

    return Patched;
}

bool XTL::VertexPatcher::Restore()
{
    if(!this->m_bPatched)
        return false;

    for(UINT uiStream = 0; uiStream < m_uiNbrStreams; uiStream++)
    {
        if(m_pStreams[uiStream].pOriginalStream != NULL && m_pStreams[uiStream].pPatchedStream != NULL)
        {
            g_pD3DDevice8->SetStreamSource(0, m_pStreams[uiStream].pOriginalStream, m_pStreams[uiStream].uiOrigStride);
        }

        if(m_pStreams[uiStream].pOriginalStream != NULL)
        {
            UINT a = m_pStreams[uiStream].pOriginalStream->Release();
        }

        if(m_pStreams[uiStream].pPatchedStream != NULL)
        {
            UINT b = m_pStreams[uiStream].pPatchedStream->Release();
        }

        if(!m_pStreams[uiStream].bUsedCached)
        {

            if(this->m_bAllocatedStreamZeroData)
            {
                free(m_pNewVertexStreamZeroData);
				// Cleanup, just to be sure :
				m_pNewVertexStreamZeroData = nullptr;
				this->m_bAllocatedStreamZeroData = false;
            }
        }
        else
        {
            m_pStreams[uiStream].bUsedCached = false;
        }

    }

    return true;
}

VOID XTL::EmuFlushIVB()
{
    XTL::EmuUpdateDeferredStates();

    DWORD *pdwVB = (DWORD*)g_pIVBVertexBuffer;

    // Parse IVB table with current FVF shader if possible.
    bool bFVF = VshHandleIsFVF(g_CurrentVertexShader);
    DWORD dwCurFVF;
    if(bFVF && ((g_CurrentVertexShader & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW))
    {
        dwCurFVF = g_CurrentVertexShader;

		// HACK: Halo...
		if(dwCurFVF == 0)
		{
			EmuWarning("EmuFlushIVB(): using g_IVBFVF instead of current FVF!");
			dwCurFVF = g_IVBFVF;
		}
    }
    else
    {
        dwCurFVF = g_IVBFVF;
    }

    DbgPrintf("g_IVBTblOffs := %d\n", g_IVBTblOffs);

    DWORD dwPos = dwCurFVF & D3DFVF_POSITION_MASK;
	DWORD dwTexN = (dwCurFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

	for(uint v=0;v<g_IVBTblOffs;v++)
    {
        if(dwPos == D3DFVF_XYZRHW)
        {
            *(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
            *(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
            *(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;
            *(FLOAT*)pdwVB++ = g_IVBTable[v].Rhw;

            DbgPrintf("IVB Position := {%f, %f, %f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z, g_IVBTable[v].Position.z, g_IVBTable[v].Rhw);
        }
		else // XYZRHW cannot be combined with NORMAL, but the other XYZ formats can :
		{
			if (dwPos == D3DFVF_XYZ)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;

				DbgPrintf("IVB Position := {%f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z);
			}
			else if (dwPos == D3DFVF_XYZB1)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend1;

				DbgPrintf("IVB Position := {%f, %f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z, g_IVBTable[v].Blend1);
			}
			else if (dwPos == D3DFVF_XYZB2)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend1;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend2;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z, g_IVBTable[v].Blend1, g_IVBTable[v].Blend2);
			}
			else if (dwPos == D3DFVF_XYZB3)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend1;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend2;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend3;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z, g_IVBTable[v].Blend1, g_IVBTable[v].Blend2, g_IVBTable[v].Blend3);
			}
			else if (dwPos == D3DFVF_XYZB4)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Position.z;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend1;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend2;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend3;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Blend4;

				DbgPrintf("IVB Position := {%f, %f, %f, %f, %f, %f, %f}\n", g_IVBTable[v].Position.x, g_IVBTable[v].Position.y, g_IVBTable[v].Position.z, g_IVBTable[v].Blend1, g_IVBTable[v].Blend2, g_IVBTable[v].Blend3, g_IVBTable[v].Blend4);
			}
			else
			{
				CxbxKrnlCleanup("Unsupported Position Mask (FVF := 0x%.08X dwPos := 0x%.08X)", dwCurFVF, dwPos);
			}

			//      if(dwPos == D3DFVF_NORMAL)	// <- This didn't look right but if it is, change it back...
			if (dwCurFVF & D3DFVF_NORMAL)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Normal.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Normal.y;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].Normal.z;

				DbgPrintf("IVB Normal := {%f, %f, %f}\n", g_IVBTable[v].Normal.x, g_IVBTable[v].Normal.y, g_IVBTable[v].Normal.z);
			}
		}

        if(dwCurFVF & D3DFVF_DIFFUSE)
        {
            *(DWORD*)pdwVB++ = g_IVBTable[v].Diffuse;

            DbgPrintf("IVB Diffuse := 0x%.08X\n", g_IVBTable[v].Diffuse);
        }

        if(dwCurFVF & D3DFVF_SPECULAR)
        {
            *(DWORD*)pdwVB++ = g_IVBTable[v].Specular;

            DbgPrintf("IVB Specular := 0x%.08X\n", g_IVBTable[v].Specular);
        }

		// TODO -oDxbx : Handle other sizes than D3DFVF_TEXCOORDSIZE2 too!
		// See D3DTSS_TEXTURETRANSFORMFLAGS values other than D3DTTFF_COUNT2
		// See and/or X_D3DVSD_DATATYPEMASK values other than D3DVSDT_FLOAT2
		if(dwTexN >= 1)
        {
            *(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord1.x;
            *(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord1.y;

            DbgPrintf("IVB TexCoord1 := {%f, %f}\n", g_IVBTable[v].TexCoord1.x, g_IVBTable[v].TexCoord1.y);

			if(dwTexN >= 2)
			{
				*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord2.x;
				*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord2.y;

				DbgPrintf("IVB TexCoord2 := {%f, %f}\n", g_IVBTable[v].TexCoord2.x, g_IVBTable[v].TexCoord2.y);

				if(dwTexN >= 3)
				{
					*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord3.x;
					*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord3.y;

					DbgPrintf("IVB TexCoord3 := {%f, %f}\n", g_IVBTable[v].TexCoord3.x, g_IVBTable[v].TexCoord3.y);

					if(dwTexN >= 4)
					{
						*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord4.x;
						*(FLOAT*)pdwVB++ = g_IVBTable[v].TexCoord4.y;

						DbgPrintf("IVB TexCoord4 := {%f, %f}\n", g_IVBTable[v].TexCoord4.x, g_IVBTable[v].TexCoord4.y);
					}
				}
			}
        }

		uint VertexBufferUsage = (BYTE *)pdwVB - (BYTE *)g_pIVBVertexBuffer;
		if (VertexBufferUsage >= IVB_BUFFER_SIZE)
			CxbxKrnlCleanup("Overflow g_pIVBVertexBuffer  : %d", v);
	}

	// Dxbx note : Instead of calculating this above (when v=0),
	// we use a tooling function to determine the vertex stride :
	UINT uiStride = DxbxFVFToVertexSizeInBytes(dwCurFVF, /*bIncludeTextures=*/true);

    VertexPatchDesc VPDesc;

    VPDesc.XboxPrimitiveType = g_IVBPrimitiveType;
    VPDesc.dwVertexCount = g_IVBTblOffs;
    VPDesc.dwOffset = 0;
    VPDesc.pVertexStreamZeroData = g_pIVBVertexBuffer;
    VPDesc.uiVertexStreamZeroStride = uiStride;
    VPDesc.hVertexShader = g_CurrentVertexShader;

    VertexPatcher VertPatch;

    bool bPatched = VertPatch.Apply(&VPDesc, NULL);

    if(bFVF)
        g_pD3DDevice8->SetVertexShader(dwCurFVF);

    g_pD3DDevice8->DrawPrimitiveUP(
		EmuXB2PC_D3DPrimitiveType(VPDesc.XboxPrimitiveType),
        VPDesc.dwPrimitiveCount,
        VPDesc.pVertexStreamZeroData,
        VPDesc.uiVertexStreamZeroStride);

	g_dwPrimPerFrame += VPDesc.dwPrimitiveCount;

    if(bFVF)
        g_pD3DDevice8->SetVertexShader(g_CurrentVertexShader);

    VertPatch.Restore();

    g_IVBTblOffs = 0;

    return;
}
