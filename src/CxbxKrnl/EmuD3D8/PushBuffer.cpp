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

static void DbgDumpMesh(XTL::INDEX16 *pIndexData, DWORD dwCount);

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

// Dxbx Note: This code appeared in EmuExecutePushBufferRaw and occured
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
	if (Result >= (D3DFVF_XYZB1 >> 1)) {
		// Any format from D3DFVF_XYZB1 and above need 1 extra float :
		Result++;
	}
	else {
		// The other formats (XYZ and XYZRHW) need 2 extra floats :
		Result += 2;
	}

	// Express the size in bytes, instead of floats :
	Result *= sizeof(FLOAT);
	// D3DFVF_NORMAL cannot be combined with D3DFVF_XYZRHW :
	if ((dwFVF & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW) {
		if (dwFVF & D3DFVF_NORMAL) {
			Result += sizeof(FLOAT) * 3;
		}
	}

	if (dwFVF & D3DFVF_DIFFUSE) {
		Result += sizeof(XTL::D3DCOLOR);
	}

	if (dwFVF & D3DFVF_SPECULAR) {
		Result += sizeof(XTL::D3DCOLOR);
	}

	if (bIncludeTextures) {
		int NrTextures = ((dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT);
		while (NrTextures > 0) {
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
    if (pFixup != NULL)
        CxbxKrnlCleanup("PushBuffer has fixups\n");

#ifdef _DEBUG_TRACK_PB
	DbgDumpPushBuffer((PPUSH)pPushBuffer->Data, pPushBuffer->Size);
#endif

    EmuExecutePushBufferRaw((DWORD*)pPushBuffer->Data);

    return;
}

extern void XTL::EmuExecutePushBufferRaw
(
    DWORD                 *pdwPushData
)
{
	// Test case : XDK Sample BeginPush
	if (g_bSkipPush) {
		return;
	}

	if (pdwPushData == NULL) {
		EmuWarning("pdwPushData is null");
		return;
	}

    DWORD *pdwOrigPushData = pdwPushData;

    INDEX16 *pIndexData = NULL;
    PVOID pVertexData = NULL;

    DWORD dwVertexShader = -1;
    DWORD dwVertexStride = -1;

    // cache of last 4 indices
	INDEX16 pIBMem[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

    X_D3DPRIMITIVETYPE  XboxPrimitiveType = X_D3DPT_INVALID;

    // TODO: This technically should be enabled
    XTL::DxbxUpdateDeferredStates();

    #ifdef _DEBUG_TRACK_PB
    bool bShowPB = false;

    g_PBTrackTotal.insert(pdwPushData);
    if (g_PBTrackShowOnce.exists(pdwPushData)) {
        g_PBTrackShowOnce.remove(pdwPushData);
        printf("\n");
        printf("\n");
        printf("  PushBuffer@0x%p...\n", pdwPushData);
        printf("\n");
        bShowPB = true;
    }
    #endif

    while (true) {
        DWORD dwCount = (*pdwPushData >> 18);
        DWORD dwMethod = (*pdwPushData & 0x3FFFF);

        // Interpret GPU Instruction
        if (dwMethod == 0x000017FC) { // NVPB_SetBeginEnd
            pdwPushData++;

            #ifdef _DEBUG_TRACK_PB
            if (bShowPB) {
                printf("  NVPB_SetBeginEnd(");
            }
            #endif

            if (*pdwPushData == 0) {
                #ifdef _DEBUG_TRACK_PB
                if (bShowPB) {
                    printf("DONE)\n");
                }
                #endif
                break;  // done?
            }
            else {
                #ifdef _DEBUG_TRACK_PB
                if (bShowPB) {
                    printf("PrimitiveType := %u)\n", *pdwPushData);
                }
                #endif

                XboxPrimitiveType = (X_D3DPRIMITIVETYPE)*pdwPushData;
            }
        }
        else if (dwMethod == 0x1818) { // NVPB_InlineVertexArray
            BOOL bInc = *pdwPushData & 0x40000000;
            if (bInc) {
                dwCount = (*pdwPushData - (0x40000000 | 0x00001818)) >> 18;
            }

            pVertexData = ++pdwPushData;
            pdwPushData += dwCount;
            // retrieve vertex shader
            g_pD3DDevice8->GetVertexShader(&dwVertexShader);
            if (dwVertexShader > 0xFFFF) {
                CxbxKrnlCleanup("Non-FVF Vertex Shaders not yet supported for PushBuffer emulation!");
                dwVertexShader = 0;
            }
            else if (dwVertexShader == NULL) {
                EmuWarning("FVF Vertex Shader is null");
                dwVertexShader = -1;
            }
			/*else if (dwVertexShader == 0x6) {
				dwVertexShader = (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
			}*/

		//	printf( "EmuExecutePushBufferRaw: FVF = 0x%.08X\n" );

            //
            // calculate stride
            //

            dwVertexStride = 0;
            if (VshHandleIsFVF(dwVertexShader)) {
				dwVertexStride = DxbxFVFToVertexSizeInBytes(dwVertexShader, /*bIncludeTextures=*/true);
            }

            /*
            // create cached vertex buffer only once, with maxed out size
            if (pVertexBuffer == 0) {
                HRESULT hRet = g_pD3DDevice8->CreateVertexBuffer(2047*sizeof(DWORD), D3DUSAGE_WRITEONLY, dwVertexShader, D3DPOOL_MANAGED, &pVertexBuffer);
                if (FAILED(hRet))
                    CxbxKrnlCleanup("Unable to create vertex buffer cache for PushBuffer emulation (0x1818, dwCount : %d)", dwCount);
            }

            // copy vertex data
            {
                uint08 *pData = nullptr;
                HRESULT hRet = pVertexBuffer->Lock(0, dwCount * sizeof(DWORD), &pData, D3DLOCK_DISCARD);
                if (FAILED(hRet))
                    CxbxKrnlCleanup("Unable to lock vertex buffer cache for PushBuffer emulation (0x1818, dwCount : %d)", dwCount);

                memcpy(pData, pVertexData, dwCount * sizeof(DWORD));
                pVertexBuffer->Unlock();
            }
            */

            #ifdef _DEBUG_TRACK_PB
            if (bShowPB) {
                printf("NVPB_InlineVertexArray(...)\n");
                printf("  dwCount : %u\n", dwCount);
                printf("  dwVertexShader : 0x%.8X\n", dwVertexShader);
            }
            #endif

            // render vertices
            if (dwVertexShader != -1) {
                UINT VertexCount = (dwCount * sizeof(DWORD)) / dwVertexStride;
				CxbxDrawContext DrawContext = {};
                DrawContext.XboxPrimitiveType = XboxPrimitiveType;
                DrawContext.dwVertexCount = VertexCount;
                DrawContext.pXboxVertexStreamZeroData = pVertexData;
                DrawContext.uiXboxVertexStreamZeroStride = dwVertexStride;
                DrawContext.hVertexShader = dwVertexShader;

				CxbxDrawPrimitiveUP(DrawContext);
            }

            pdwPushData--;
        }
        else if (dwMethod == 0x1808) { // NVPB_FixLoop
			// Test case : Turok menu's
            #ifdef _DEBUG_TRACK_PB
            if (bShowPB) {
                printf("  NVPB_FixLoop(%u)\n", dwCount);
                printf("\n");
                printf("  Index Array Data...\n");
				INDEX16 *pIndices = (INDEX16*)(pdwPushData + 1);
                for(uint s=0;s<dwCount;s++) {
                    if (s%8 == 0)
						printf("\n  ");

                    printf("  %.04X", *pIndices++);
                }

                printf("\n");
                printf("\n");
            }
            #endif

			INDEX16 *pIndices = (INDEX16*)(pdwPushData + 1);
            for(uint mi=0;mi<dwCount;mi++) {
                pIBMem[mi+2] = pIndices[mi];
            }

            // perform rendering
            if (pIBMem[0] != 0xFFFF) {
				UINT uiIndexCount = dwCount + 2;
                #ifdef _DEBUG_TRACK_PB
                if (!g_PBTrackDisable.exists(pdwOrigPushData))
                #endif
                // render indexed vertices
                {
                    if (!g_bPBSkipPusher) {
                        if (IsValidCurrentShader()) {
							CxbxDrawContext DrawContext = {};
							DrawContext.XboxPrimitiveType = XboxPrimitiveType;
							DrawContext.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, uiIndexCount);
							DrawContext.hVertexShader = g_CurrentVertexShader;

							CxbxDrawIndexed(DrawContext, pIBMem);
                        }
                    }
                }
            }

            pdwPushData += dwCount;
        }
        else if (dwMethod == 0x1800) { // NVPB_InlineIndexArray
			// Test case : Turok menu's
            BOOL bInc = *pdwPushData & 0x40000000;
            if (bInc) {
                dwCount = ((*pdwPushData - (0x40000000 | 0x00001818)) >> 18)*2 + 2;
            }

            pIndexData = (INDEX16*)++pdwPushData;
            #ifdef _DEBUG_TRACK_PB
            if (bShowPB) {
                printf("  NVPB_InlineIndexArray(0x%p, %u)...\n", pIndexData, dwCount);
                printf("\n");
                printf("  Index Array Data...\n");
                INDEX16 *pIndices = pIndexData;
                for(uint s=0;s<dwCount;s++) {
                    if (s%8 == 0)
						printf("\n  ");

                    printf("  %.04X", *pIndices++);
                }

                printf("\n");

#if 0
                // retrieve stream data
                XTL::IDirect3DVertexBuffer8 *pActiveVB = nullptr;
                UINT  uiStride;

				// pActiveVB = CxbxUpdateVertexBuffer(Xbox_g_Stream[0].pVertexBuffer);
				// pActiveVB->AddRef(); // Avoid memory-curruption when this is Release()ed later
				// uiStride = Xbox_g_Stream[0].Stride;
                g_pD3DDevice8->GetStreamSource(0, &pActiveVB, &uiStride);
                // retrieve stream desc
                D3DVERTEXBUFFER_DESC VBDesc;
                pActiveVB->GetDesc(&VBDesc);
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

				pActiveVB->Release(); // Was absent (thus leaked memory)
#endif

                DbgDumpMesh(pIndexData, dwCount);
            }
            #endif

            pdwPushData += (dwCount/2) - (bInc ? 0 : 2);

            // perform rendering
            {
				UINT dwIndexCount = dwCount;

				// copy index data
                {
                    // remember last 2 indices
                    if (dwCount >= 2) { // TODO : Is 2 indices enough for all primitive types?
                        pIBMem[0] = pIndexData[dwCount - 2];
                        pIBMem[1] = pIndexData[dwCount - 1];
                    }
                    else {
                        pIBMem[0] = 0xFFFF;
                    }
                }

                #ifdef _DEBUG_TRACK_PB
                if (!g_PBTrackDisable.exists(pdwOrigPushData))
                #endif
                // render indexed vertices
                {
					if (!g_bPBSkipPusher) {
						if (IsValidCurrentShader()) {
							CxbxDrawContext DrawContext = {};
							DrawContext.XboxPrimitiveType = XboxPrimitiveType;
							DrawContext.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, dwIndexCount);
							DrawContext.hVertexShader = g_CurrentVertexShader;

							CxbxDrawIndexed(DrawContext, pIndexData);
						}
					}
                }
            }

            pdwPushData--;
        }
        else {
            CxbxKrnlCleanup("Unknown PushBuffer Operation (0x%.04X, %d)", dwMethod, dwCount);
            return;
        }

        pdwPushData++;
    }

    #ifdef _DEBUG_TRACK_PB
    if (bShowPB) {
        printf("\n");
        printf("CxbxDbg> ");
        fflush(stdout);
    }
    #endif

    if (g_bStepPush) {
        g_pD3DDevice8->Present(0,0,0,0);
		Sleep(500);
    }
}

char *NV2AMethodToString(DWORD dwMethod)
{
	using namespace XTL; // for NV2A symbols

	switch (dwMethod) {

#define ENUM_RANGED_ToString_N(Name, Method, Pitch, N) \
	case Name(N): return #Name ## "((" #N ")*" #Pitch ## ")";

#define ENUM_RANGED_ToString_1(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 0)

#define ENUM_RANGED_ToString_2(Name, Method, Pitch) \
	ENUM_RANGED_ToString_1(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 1)

#define ENUM_RANGED_ToString_3(Name, Method, Pitch) \
	ENUM_RANGED_ToString_2(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 2)

#define ENUM_RANGED_ToString_4(Name, Method, Pitch) \
	ENUM_RANGED_ToString_3(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 3) 

#define ENUM_RANGED_ToString_6(Name, Method, Pitch) \
	ENUM_RANGED_ToString_4(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 4) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 5)

#define ENUM_RANGED_ToString_8(Name, Method, Pitch) \
	ENUM_RANGED_ToString_6(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 6) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 7)

#define ENUM_RANGED_ToString_10(Name, Method, Pitch) \
	ENUM_RANGED_ToString_8(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 8) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 9) \

#define ENUM_RANGED_ToString_16(Name, Method, Pitch) \
	ENUM_RANGED_ToString_10(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 10) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 11) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 12) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 13) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 14) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 15)

#define ENUM_RANGED_ToString_32(Name, Method, Pitch) \
	ENUM_RANGED_ToString_16(Name, Method, Pitch) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 16) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 17) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 18) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 19) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 20) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 21) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 22) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 23) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 24) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 25) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 26) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 27) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 28) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 29) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 30) \
	ENUM_RANGED_ToString_N(Name, Method, Pitch, 31)

#define ENUM_METHOD_ToString(Name, Method) case Method: return #Name;
#define ENUM_RANGED_ToString(Name, Method, Pitch, Repeat) ENUM_RANGED_ToString_##Repeat(Name, Method, Pitch)
#define ENUM_BITFLD_Ignore(Name, Value)
#define ENUM_VALUE_Ignore(Name, Value)

	ENUM_NV2A(ENUM_METHOD_ToString, ENUM_RANGED_ToString, ENUM_BITFLD_Ignore, ENUM_VALUE_Ignore)

	default:
		return "UNLABLED";
	}
}
#ifdef _DEBUG_TRACK_PB
void DbgDumpMesh(XTL::INDEX16 *pIndexData, DWORD dwCount)
{
    if (!XTL::IsValidCurrentShader() || (dwCount == 0))
        return;

    // retrieve stream data
    char szFileName[128];
    sprintf(szFileName, "D:\\_cxbx\\mesh\\CxbxMesh-0x%p.x", pIndexData);
    FILE *dbgVertices = fopen(szFileName, "wt");

    BYTE *pVBData = (BYTE *)XTL::GetDataFromXboxResource(XTL::Xbox_g_Stream[0].pVertexBuffer);
    UINT  uiStride = XTL::Xbox_g_Stream[0].Stride;

    // print out stream data
    {
        XTL::INDEX16 maxIndex = 0;
		XTL::INDEX16 *pIndexCheck = pIndexData;
        for(uint chk=0;chk<dwCount;chk++) {
			XTL::INDEX16 x = *pIndexCheck++;
            if (x > maxIndex)
                maxIndex = x;
        }
#if 0
        if (maxIndex > ((VBDesc.Size/uiStride) - 1))
            maxIndex = (VBDesc.Size / uiStride) - 1;
#endif
        fprintf(dbgVertices, "xof 0303txt 0032\n");
        fprintf(dbgVertices, "\n");
        fprintf(dbgVertices, "//\n");
        fprintf(dbgVertices, "//  Vertex Stream Data (0x%p)...\n", pVBData);
        fprintf(dbgVertices, "//\n");
#if 0
		fprintf(dbgVertices, "//  Format : %d\n", VBDesc.Format);
        fprintf(dbgVertices, "//  Size   : %d bytes\n", VBDesc.Size);
        fprintf(dbgVertices, "//  FVF    : 0x%.08X\n", VBDesc.FVF);
#endif
        fprintf(dbgVertices, "//  iCount : %u\n", dwCount/2);
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

        fprintf(dbgVertices, "      %u;\n", dwCount - 2);

		XTL::INDEX16 *pIndexValues = pIndexData;

        max = dwCount;

        DWORD a = *pIndexValues++;
        DWORD b = *pIndexValues++;
        DWORD c = *pIndexValues++;

        DWORD la = a,lb = b,lc = c;

        for(uint i=2;i<max;i++)
        {
            fprintf(dbgVertices, "      3;%u,%u,%u;%s\n",
                a,b,c, (i < (max - 1)) ? "," : ";");

            a = b;
            b = c;
            c = *pIndexValues++;

            la = a;
            lb = b;
            lc = c;
        }

        fprintf(dbgVertices, "    }\n");
        fprintf(dbgVertices, "  }\n");
        fprintf(dbgVertices, "}\n");

        fclose(dbgVertices);
    }
}

void XTL::DbgDumpPushBuffer(PPUSH PBData, DWORD dwSize)
{
	static int PbNumber = 0;	// Keep track of how many push buffers we've attemted to convert.
	DWORD dwVertexShader;
	char szPB[512];

	// Prevent dumping too many of these!
	if (PbNumber > 300) {
		return;
	}

	// Get a copy of the current vertex shader
	g_pD3DDevice8->GetVertexShader(&dwVertexShader);

	/*if (g_CurrentVertexShader != dwVertexShader) {
		printf( "g_CurrentVertexShader does not match FVF from GetVertexShader!\n"
					"g_CurrentVertexShader = 0x%.08X\n"
					"GetVertexShader = 0x%.08X\n" );
	}*/

	if (dwVertexShader > 0xFFFF) {
		EmuWarning("Cannot dump pushbuffer without an FVF (programmable shaders not supported)");
		return;
	}

	sprintf(szPB, "D:\\cxbx\\_pushbuffer\\pushbuffer%.03d.txt", PbNumber++);
	// Create a new file for this pushbuffer's data
	HANDLE hFile = CreateFile(szPB, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		EmuWarning("Error creating pushbuffer file!");
	}

	DWORD dwBytesWritten;

	// Write pushbuffer data to the file.
	// TODO: Cache the 32-bit XXHash32::hash() of each pushbuffer to ensure that the same
	// pushbuffer is not written twice within a given emulation session.
	WriteFile(hFile, &g_CurrentVertexShader, sizeof(DWORD), &dwBytesWritten, nullptr);
	WriteFile(hFile, (LPCVOID)PBData, dwSize, &dwBytesWritten, nullptr);
	// Close handle
	CloseHandle(hFile);
}

#endif