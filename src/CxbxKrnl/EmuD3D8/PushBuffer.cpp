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
// *   Cxbx->Win32->CxbxKrnl->EmuD3D->PushBuffer.cpp
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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
#include "CxbxKrnl/EmuD3D8Types.h" // For X_D3DFORMAT
#include "CxbxKrnl/ResourceTracker.h"
#include "CxbxKrnl/MemoryManager.h"
#include "State.h"

uint32  XTL::g_dwPrimaryPBCount = 0;
uint32 *XTL::g_pPrimaryPB = 0;

bool XTL::g_bStepPush = false;
bool XTL::g_bSkipPush = false;
bool XTL::g_bBrkPush  = false;

bool g_bPBSkipPusher = false;

static void DbgDumpMesh(WORD *pIndexData, DWORD dwCount);

int XTL::DxbxFVF_GetTextureSize(DWORD dwFVF, int aTextureIndex)
// Determine the size (in bytes) of the texture format (indexed 0 .. 3).
// This is the reverse of the D3DFVF_TEXCOORDSIZE[0..3] macros.
{
	switch ((dwFVF >> ((aTextureIndex * 2) + 16)) & 3) {
	case D3DFVF_TEXTUREFORMAT1: return 1 * sizeof(FLOAT);
	case D3DFVF_TEXTUREFORMAT2: return 2 * sizeof(FLOAT);
	case D3DFVF_TEXTUREFORMAT3: return 3 * sizeof(FLOAT);
	case D3DFVF_TEXTUREFORMAT4: return 4 * sizeof(FLOAT);
	default:
		//assert(false || "DxbxFVF_GetTextureSize : Unhandled case");
		return 0;
	}
}

// Dxbx Note: This code is taken from EmuExecutePushBufferRaw and occured
// in EmuFlushIVB too, so it's generalize in this single implementation.
UINT XTL::DxbxFVFToVertexSizeInBytes(DWORD dwFVF, BOOL bIncludeTextures)
{
/*
	X_D3DFVF_POSITION_MASK    = $00E; // Dec  /2  #fl

	X_D3DFVF_XYZ              = $002; //  2 > 1 > 3
	X_D3DFVF_XYZRHW           = $004; //  4 > 2 > 4
	X_D3DFVF_XYZB1            = $006; //  6 > 3 > 4
	X_D3DFVF_XYZB2            = $008; //  8 > 4 > 5
	X_D3DFVF_XYZB3            = $00a; // 10 > 5 > 6
	X_D3DFVF_XYZB4            = $00c; // 12 > 6 > 7
*/
	// Divide the D3DFVF by two, this gives almost the number of floats needed for the format :
	UINT Result = (dwFVF & D3DFVF_POSITION_MASK) >> 1;
	if (Result >= (D3DFVF_XYZB1 >> 1))
		// Any format from D3DFVF_XYZB1 and above need 1 extra float :
		Result++;
	else
		// The other formats (XYZ and XYZRHW) need 2 extra floats :
		Result += 2;

	// Express the size in bytes, instead of floats :
	Result *= sizeof(FLOAT);

	// D3DFVF_NORMAL cannot be combined with D3DFVF_XYZRHW :
	if ((dwFVF & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW)
		if (dwFVF & D3DFVF_NORMAL)
			Result += sizeof(FLOAT) * 3;

	if (dwFVF & D3DFVF_DIFFUSE)
		Result += sizeof(XTL::D3DCOLOR);

	if (dwFVF & D3DFVF_SPECULAR)
		Result += sizeof(XTL::D3DCOLOR);

	if (bIncludeTextures)
	{
		int NrTextures = ((dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT);
		while (NrTextures > 0)
		{
			NrTextures--;
			Result += DxbxFVF_GetTextureSize(dwFVF, NrTextures);
		}
	}

	return Result;
}

void XTL::EmuExecutePushBuffer
(
    X_D3DPushBuffer       *pPushBuffer,
    X_D3DFixup            *pFixup
)
{
    if(pFixup != NULL)
        CxbxKrnlCleanup("PushBuffer has fixups\n");

#ifdef _DEBUG_TRACK_PB
	DbgDumpPushBuffer((DWORD*)pPushBuffer->Data, pPushBuffer->Size);
#endif

    EmuExecutePushBufferRaw((DWORD*)pPushBuffer->Data);

    return;
}

extern void XTL::EmuExecutePushBufferRaw
(
    DWORD                 *pdwPushData
)
{
    if(g_bSkipPush)
        return;

	if (pdwPushData == NULL) {
		EmuWarning("pdwPushData is null");
		return;
	}

    DWORD *pdwOrigPushData = pdwPushData;

    PVOID pIndexData = 0;
    PVOID pVertexData = 0;

    DWORD dwVertexShader = -1;
    DWORD dwStride = -1;

    // cache of last 4 indices
    WORD pIBMem[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

    X_D3DPRIMITIVETYPE  XboxPrimitiveType = X_D3DPT_INVALID;

    // TODO: This technically should be enabled
    XTL::DxbxUpdateDeferredStates();

    #ifdef _DEBUG_TRACK_PB
    bool bShowPB = false;

    g_PBTrackTotal.insert(pdwPushData);

    if(g_PBTrackShowOnce.exists(pdwPushData))
    {
        g_PBTrackShowOnce.remove(pdwPushData);

        printf("\n");
        printf("\n");
        printf("  PushBuffer@0x%.08X...\n", pdwPushData);
        printf("\n");

        bShowPB = true;
    }
    #endif

    static LPDIRECT3DINDEXBUFFER8  pIndexBuffer = nullptr;
    // static LPDIRECT3DVERTEXBUFFER8 pVertexBuffer = nullptr;

    static uint maxIBSize = 0;

    while(true)
    {
        DWORD dwCount = (*pdwPushData >> 18);
        DWORD dwMethod = (*pdwPushData & 0x3FFFF);

        // Interpret GPU Instruction
        if(dwMethod == 0x000017FC) // NVPB_SetBeginEnd
        {
            pdwPushData++;

            #ifdef _DEBUG_TRACK_PB
            if(bShowPB)
            {
                printf("  NVPB_SetBeginEnd(");
            }
            #endif

            if(*pdwPushData == 0)
            {
                #ifdef _DEBUG_TRACK_PB
                if(bShowPB)
                {
                    printf("DONE)\n");
                }
                #endif
                break;  // done?
            }
            else
            {
                #ifdef _DEBUG_TRACK_PB
                if(bShowPB)
                {
                    printf("PrimitiveType := %d)\n", *pdwPushData);
                }
                #endif

                XboxPrimitiveType = (X_D3DPRIMITIVETYPE)*pdwPushData;
            }
        }
        else if(dwMethod == 0x1818) // NVPB_InlineVertexArray
        {
            BOOL bInc = *pdwPushData & 0x40000000;

            if(bInc)
            {
                dwCount = (*pdwPushData - (0x40000000 | 0x00001818)) >> 18;
            }

            pVertexData = ++pdwPushData;

            pdwPushData += dwCount;

            // retrieve vertex shader
            g_pD3DDevice8->GetVertexShader(&dwVertexShader);

            if(dwVertexShader > 0xFFFF)
            {
                CxbxKrnlCleanup("Non-FVF Vertex Shaders not yet supported for PushBuffer emulation!");
                dwVertexShader = 0;
            }
            else if(dwVertexShader == NULL)
            {
                EmuWarning("FVF Vertex Shader is null");
                dwVertexShader = -1;
            }
			/*else if(dwVertexShader == 0x6)
			{
				dwVertexShader = (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			}*/

		//	printf( "EmuExecutePushBufferRaw: FVF = 0x%.08X\n" );

            //
            // calculate stride
            //

            dwStride = 0;

            if(VshHandleIsFVF(dwVertexShader))
            {

				dwStride = DxbxFVFToVertexSizeInBytes(dwVertexShader, /*bIncludeTextures=*/true);
            }

            /*
            // create cached vertex buffer only once, with maxed out size
            if(pVertexBuffer == 0)
            {
                HRESULT hRet = g_pD3DDevice8->CreateVertexBuffer(2047*sizeof(DWORD), D3DUSAGE_WRITEONLY, dwVertexShader, D3DPOOL_MANAGED, &pVertexBuffer);

                if(FAILED(hRet))
                    CxbxKrnlCleanup("Unable to create vertex buffer cache for PushBuffer emulation (0x1818, dwCount : %d)", dwCount);

            }

            // copy vertex data
            {
                uint08 *pData = 0;

                HRESULT hRet = pVertexBuffer->Lock(0, dwCount*4, &pData, D3DLOCK_DISCARD);

                if(FAILED(hRet))
                    CxbxKrnlCleanup("Unable to lock vertex buffer cache for PushBuffer emulation (0x1818, dwCount : %d)", dwCount);

                memcpy(pData, pVertexData, dwCount*4);

                pVertexBuffer->Unlock();
            }
            */

            #ifdef _DEBUG_TRACK_PB
            if(bShowPB)
            {
                printf("NVPB_InlineVertexArray(...)\n");
                printf("  dwCount : %d\n", dwCount);
                printf("  dwVertexShader : 0x%08X\n", dwVertexShader);
            }
            #endif

            // render vertices
            if(dwVertexShader != -1)
            {
                UINT VertexCount = (dwCount * sizeof(DWORD)) / dwStride;

                VertexPatchDesc VPDesc;

                VPDesc.XboxPrimitiveType = XboxPrimitiveType;
                VPDesc.dwVertexCount = VertexCount;
                VPDesc.dwPrimitiveCount = 0;
                VPDesc.dwOffset = 0;
                VPDesc.pVertexStreamZeroData = pVertexData;
                VPDesc.uiVertexStreamZeroStride = dwStride;
                VPDesc.hVertexShader = dwVertexShader;

				CxbxDrawPrimitiveUP(VPDesc);
            }

            pdwPushData--;
        }
        else if(dwMethod == 0x1808) // NVPB_FixLoop
        {
            #ifdef _DEBUG_TRACK_PB
            if(bShowPB)
            {
                printf("  NVPB_FixLoop(%d)\n", dwCount);
                printf("\n");
                printf("  Index Array Data...\n");

                WORD *pwVal = (WORD*)(pdwPushData + 1);

                for(uint s=0;s<dwCount;s++)
                {
                    if(s%8 == 0) printf("\n  ");

                    printf("  %.04X", *pwVal++);
                }

                printf("\n");
                printf("\n");
            }
            #endif

            WORD *pwVal = (WORD*)(pdwPushData + 1);
            for(uint mi=0;mi<dwCount;mi++)
            {
                pIBMem[mi+2] = pwVal[mi];
            }

            // perform rendering
            if(pIBMem[0] != 0xFFFF)
            {
				DWORD dwIndexCount = dwCount + 2;
				DWORD dwIndexBufferSize = dwIndexCount * sizeof(WORD);

                // TODO: depreciate maxIBSize after N milliseconds..then N milliseconds later drop down to new highest
                if(maxIBSize < dwIndexBufferSize)
                {
                    if(pIndexBuffer != nullptr)
                        pIndexBuffer->Release();

					HRESULT hRet = g_pD3DDevice8->CreateIndexBuffer(dwIndexBufferSize, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pIndexBuffer);
					if(FAILED(hRet))
						CxbxKrnlCleanup("Unable to create index buffer for PushBuffer emulation (0x1808, dwCount : %d)", dwCount);

                    maxIBSize = dwIndexBufferSize;
                }

                // copy index data
                {
                    WORD *pData = nullptr;

                    pIndexBuffer->Lock(0, dwIndexBufferSize, (BYTE**)(&pData), D3DLOCK_DISCARD);
                    memcpy(pData, pIBMem, dwIndexBufferSize);
                    pIndexBuffer->Unlock();
                }

                #ifdef _DEBUG_TRACK_PB
                if(!g_PBTrackDisable.exists(pdwOrigPushData))
                #endif
                // render indexed vertices
                {
                    if(!g_bPBSkipPusher)
                    {
                        if(IsValidCurrentShader())
                        {
							VertexPatchDesc VPDesc;

							VPDesc.XboxPrimitiveType = XboxPrimitiveType;
							VPDesc.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, dwIndexCount);
							VPDesc.dwPrimitiveCount = 0;
							VPDesc.dwOffset = 0;
							VPDesc.pVertexStreamZeroData = NULL;
							VPDesc.uiVertexStreamZeroStride = 0;
							// TODO: Set the current shader and let the patcher handle it..
							VPDesc.hVertexShader = g_CurrentVertexShader;

							g_pD3DDevice8->SetIndices(pIndexBuffer, 0);

							CxbxDrawIndexed(VPDesc);

		                    g_pD3DDevice8->SetIndices(nullptr, 0);
                        }
                    }
                }
            }

            pdwPushData += dwCount;
        }
        else if(dwMethod == 0x1800) // NVPB_InlineIndexArray
        {
            BOOL bInc = *pdwPushData & 0x40000000;

            if(bInc)
            {
                dwCount = ((*pdwPushData - (0x40000000 | 0x00001818)) >> 18)*2 + 2;
            }

            pIndexData = ++pdwPushData;

            #ifdef _DEBUG_TRACK_PB
            if(bShowPB)
            {
                printf("  NVPB_InlineIndexArray(0x%.08X, %d)...\n", pIndexData, dwCount);
                printf("\n");
                printf("  Index Array Data...\n");

                WORD *pwVal = (WORD*)pIndexData;

                for(uint s=0;s<dwCount;s++)
                {
                    if(s%8 == 0) printf("\n  ");

                    printf("  %.04X", *pwVal++);
                }

                printf("\n");

                // retrieve stream data
                XTL::IDirect3DVertexBuffer8 *pActiveVB = nullptr;
                UINT  uiStride;

#ifdef UNPATCH_STREAMSOURCE
				pActiveVB = CxbxUpdateVertexBuffer(Xbox_g_Stream[0].pVertexBuffer);
				pActiveVB->AddRef(); // Avoid memory-curruption when this is Release()ed later
				uiStride = Xbox_g_Stream[0].Stride;
#else
                g_pD3DDevice8->GetStreamSource(0, &pActiveVB, &uiStride);
#endif

                // retrieve stream desc
                D3DVERTEXBUFFER_DESC VBDesc;

                pActiveVB->GetDesc(&VBDesc);

#if 0
				// unlock just in case
                pActiveVB->Unlock();

                // grab ptr
                BYTE *pVBData = 0;

                pActiveVB->Lock(0, 0, &pVBData, D3DLOCK_READONLY);
#endif

                // print out stream data
                {
                    printf("\n");
                    printf("  Vertex Stream Data (0x%.08X)...\n", pActiveVB);
                    printf("\n");
                    printf("  Format : %d\n", VBDesc.Format);
                    printf("  Size   : %d bytes\n", VBDesc.Size);
                    printf("  FVF    : 0x%.08X\n", VBDesc.FVF);
                    printf("\n");
                }

#if 0
                // release ptr
                pActiveVB->Unlock();
#endif
				pActiveVB->Release(); // Was absent (thus leaked memory)

                DbgDumpMesh((WORD*)pIndexData, dwCount);
            }
            #endif

            pdwPushData += (dwCount/2) - (bInc ? 0 : 2);

            // perform rendering
            {
				DWORD dwIndexCount = dwCount;
				DWORD dwIndexBufferSize = dwIndexCount * sizeof(WORD);

                // TODO: depreciate maxIBSize after N milliseconds..then N milliseconds later drop down to new highest
                if(maxIBSize < dwIndexBufferSize)
                {
                    if(pIndexBuffer != nullptr)
                        pIndexBuffer->Release();

					HRESULT hRet = g_pD3DDevice8->CreateIndexBuffer(dwIndexBufferSize, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pIndexBuffer);
					if(FAILED(hRet))
						CxbxKrnlCleanup("Unable to create index buffer for PushBuffer emulation (0x1800, dwCount : %d)", dwCount);

                    maxIBSize = dwIndexBufferSize;
                }

                // copy index data
                {
                    WORD *pData = nullptr;

                    pIndexBuffer->Lock(0, dwIndexBufferSize, (BYTE **)(&pData), D3DLOCK_DISCARD);
                    memcpy(pData, pIndexData, dwIndexBufferSize);

                    // remember last 2 indices
                    if(dwCount >= 2)
                    {
                        pIBMem[0] = pData[dwCount - 2];
                        pIBMem[1] = pData[dwCount - 1];
                    }
                    else
                    {
                        pIBMem[0] = 0xFFFF;
                    }

                    pIndexBuffer->Unlock();
                }

                #ifdef _DEBUG_TRACK_PB
                if(!g_PBTrackDisable.exists(pdwOrigPushData))
                #endif
                // render indexed vertices
                {
					if (!g_bPBSkipPusher)
					{
						if (IsValidCurrentShader())
						{
							VertexPatchDesc VPDesc;

							VPDesc.XboxPrimitiveType = XboxPrimitiveType;
							VPDesc.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, dwIndexCount);
							VPDesc.dwPrimitiveCount = 0;
							VPDesc.dwOffset = 0;
							VPDesc.pVertexStreamZeroData = NULL;
							VPDesc.uiVertexStreamZeroStride = 0;
							// TODO: Set the current shader and let the patcher handle it..
							VPDesc.hVertexShader = g_CurrentVertexShader;

							g_pD3DDevice8->SetIndices(pIndexBuffer, 0);

							CxbxDrawIndexed(VPDesc);

							g_pD3DDevice8->SetIndices(nullptr, 0);
						}
					}
                }
            }

            pdwPushData--;
        }
        else
        {
            CxbxKrnlCleanup("Unknown PushBuffer Operation (0x%.04X, %d)", dwMethod, dwCount);
            return;
        }

        pdwPushData++;
    }

    #ifdef _DEBUG_TRACK_PB
    if(bShowPB)
    {
        printf("\n");
        printf("CxbxDbg> ");
        fflush(stdout);
    }
    #endif

    if(g_bStepPush)
    {
        g_pD3DDevice8->Present(0,0,0,0);
        Sleep(500);
    }
}

#ifdef _DEBUG_TRACK_PB
void DbgDumpMesh(WORD *pIndexData, DWORD dwCount)
{
    if(!XTL::IsValidCurrentShader() || (dwCount == 0))
        return;

    XTL::IDirect3DVertexBuffer8 *pActiveVB = nullptr;

    XTL::D3DVERTEXBUFFER_DESC VBDesc;

    BYTE *pVBData = 0;
    UINT  uiStride;

    // retrieve stream data
#ifdef UNPATCH_STREAMSOURCE
	pActiveVB = XTL::CxbxUpdateVertexBuffer(XTL::Xbox_g_Stream[0].pVertexBuffer);
	pActiveVB->AddRef(); // Avoid memory-curruption when this is Release()ed later
	uiStride = XTL::Xbox_g_Stream[0].Stride;
#else
	g_pD3DDevice8->GetStreamSource(0, &pActiveVB, &uiStride);
#endif

    char szFileName[128];
    sprintf(szFileName, "D:\\_cxbx\\mesh\\CxbxMesh-0x%.08X.x", pIndexData);
    FILE *dbgVertices = fopen(szFileName, "wt");

    // retrieve stream desc
    pActiveVB->GetDesc(&VBDesc);

    // unlock just in case
    pActiveVB->Unlock();

    // grab ptr
    pActiveVB->Lock(0, 0, &pVBData, D3DLOCK_READONLY);

    // print out stream data
    {
        uint32 maxIndex = 0;

        WORD *pwChk = (WORD*)pIndexData;

        for(uint chk=0;chk<dwCount;chk++)
        {
            DWORD x = *pwChk++;

            if(x > maxIndex)
                maxIndex = x;
        }

        if(maxIndex > ((VBDesc.Size/uiStride) - 1))
            maxIndex = (VBDesc.Size / uiStride) - 1;

        fprintf(dbgVertices, "xof 0303txt 0032\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "//\n");
        fprintf(dbgVertices, "//  Vertex Stream Data (0x%.08X)...\n", pActiveVB);
        fprintf(dbgVertices, "//\n");
        fprintf(dbgVertices, "//  Format : %d\n", VBDesc.Format);
        fprintf(dbgVertices, "//  Size   : %d bytes\n", VBDesc.Size);
        fprintf(dbgVertices, "//  FVF    : 0x%.08X\n", VBDesc.FVF);
        fprintf(dbgVertices, "//  iCount : %d\n", dwCount/2);
        fprintf(dbgVertices, "//\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "Frame SCENE_ROOT {\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "  FrameTransformMatrix {\n");
        fprintf(dbgVertices, "    1.000000,0.000000,0.000000,0.000000,\n");
        fprintf(dbgVertices, "    0.000000,1.000000,0.000000,0.000000,\n");
        fprintf(dbgVertices, "    0.000000,0.000000,1.000000,0.000000,\n");
        fprintf(dbgVertices, "    0.000000,0.000000,0.000000,1.000000;;\n");
        fprintf(dbgVertices, "  }\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "  Frame Turok1 {\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "    FrameTransformMatrix {\n");
        fprintf(dbgVertices, "      1.000000,0.000000,0.000000,0.000000,\n");
        fprintf(dbgVertices, "      0.000000,1.000000,0.000000,0.000000,\n");
        fprintf(dbgVertices, "      0.000000,0.000000,1.000000,0.000000,\n");
        fprintf(dbgVertices, "      0.000000,0.000000,0.000000,1.000000;;\n");
        fprintf(dbgVertices, "    }\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "    Mesh {\n");
        fprintf(dbgVertices, "      %d;\n", maxIndex+1);

        uint max = maxIndex+1;
        for(uint v=0;v<max;v++)
        {
            fprintf(dbgVertices, "      %f;%f;%f;%s\n",
                *(FLOAT*)&pVBData[v*uiStride+0],
                *(FLOAT*)&pVBData[v*uiStride+4],
                *(FLOAT*)&pVBData[v*uiStride+8],
                (v < (max - 1)) ? "," : ";");
        }

        fprintf(dbgVertices, "      %d;\n", dwCount - 2);

        WORD *pwVal = (WORD*)pIndexData;

        max = dwCount;

        DWORD a = *pwVal++;
        DWORD b = *pwVal++;
        DWORD c = *pwVal++;

        DWORD la = a,lb = b,lc = c;

        for(uint i=2;i<max;i++)
        {
            fprintf(dbgVertices, "      3;%d,%d,%d;%s\n",
                a,b,c, (i < (max - 1)) ? "," : ";");

            a = b;
            b = c;
            c = *pwVal++;

            la = a;
            lb = b;
            lc = c;
        }

        fprintf(dbgVertices, "    }\n");
        fprintf(dbgVertices, "  }\n");
        fprintf(dbgVertices, "}\n");

        fclose(dbgVertices);
    }

    // release ptr
    pActiveVB->Unlock();

	pActiveVB->Release(); // Was absent (thus leaked memory)
}

void XTL::DbgDumpPushBuffer( DWORD* PBData, DWORD dwSize )
{
	static int PbNumber = 0;	// Keep track of how many push buffers we've attemted to convert.
	DWORD dwVertexShader;
	char szPB[512];			

	// Prevent dumping too many of these!
	if( PbNumber > 300 )
		return;

	// Get a copy of the current vertex shader
	g_pD3DDevice8->GetVertexShader( &dwVertexShader );

	/*if( g_CurrentVertexShader != dwVertexShader )
	{
		printf( "g_CurrentVertexShader does not match FVF from GetVertexShader!\n"
					"g_CurrentVertexShader = 0x%.08X\n"
					"GetVertexShader = 0x%.08X\n" );
	}*/
	
	if( dwVertexShader > 0xFFFF )
	{
		EmuWarning( "Cannot dump pushbuffer without an FVF (programmable shaders not supported)" );
		return;
	}

	sprintf( szPB, "D:\\cxbx\\_pushbuffer\\pushbuffer%.03d.txt", PbNumber++ );

	// Create a new file for this pushbuffer's data
	HANDLE hFile = CreateFile( szPB, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
	if( hFile == INVALID_HANDLE_VALUE )
		EmuWarning("Error creating pushbuffer file!");

	DWORD dwBytesWritten;

	// Write pushbuffer data to the file.
	// TODO: Cache the 32-bit XXHash32::hash() of each pushbuffer to ensure that the same
	// pushbuffer is not written twice within a given emulation session.
	WriteFile( hFile, &g_CurrentVertexShader, sizeof( DWORD ), &dwBytesWritten, nullptr);
	WriteFile( hFile, PBData, dwSize, &dwBytesWritten, nullptr);

	// Close handle
	CloseHandle( hFile );
}

#endif