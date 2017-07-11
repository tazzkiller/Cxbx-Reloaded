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
// *   CxbxKrnl->EmuD3D8->PushBuffer.cpp
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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *  CopyRight (c) 2016-2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
#include "CxbxKrnl/EmuNV2A.h" // Nv2AControlDma
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

    EmuExecutePushBufferRaw((PPUSH)pPushBuffer->Data);

    return;
}

// Globals and controller :
PPUSH pdwOrigPushData;
bool bShowPB = false;
PPUSH pdwPushArguments;

DWORD PrevMethod[2] = {};
XTL::INDEX16 *pIndexData = NULL;
PVOID pVertexData = NULL;

DWORD dwVertexShader = -1;
DWORD dwVertexStride = -1;

// cache of last 4 indices
XTL::INDEX16 pIBMem[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

XTL::X_D3DPRIMITIVETYPE  XboxPrimitiveType = XTL::X_D3DPT_INVALID;

DWORD dwMethod;
DWORD dwSubCh;
DWORD dwCount;
bool bNoInc;

int HandledCount;
char *HandledBy;

DWORD NV2AInstance_Registers[8192] = {};

typedef void (*NV2ACallback_t)();

NV2ACallback_t NV2ACallbacks[8192] = {};

void EmuNV2A_NOP() // 0x0100
{
	using namespace XTL; // for NV2A symbols

	HandledCount = dwCount;

	// NOP, when used with an argument, triggers a software-interrupt on NV2A.
	// The CPU handles these interrupts via a DMA trigger, but since we don't
	// emulate these yet, we'll have to emulate them here instead.
	#define NOP_Argument1 (NV2AInstance_Registers[NV2A_CLEAR_DEPTH_VALUE / 4])
	#define NOP_Argument2 (NV2AInstance_Registers[NV2A_CLEAR_VALUE / 4])
	switch (*pdwPushArguments) {
	case 1: {
		// TODO : Present();
		HandledBy = "NOP flip immediate";
		break;
	}
	case 2: {
		// TODO : Present();
		HandledBy = "NOP flip synchronized";
		break;
	}
	case 3: {
		HandledBy = "NOP Pushbuffer run";
		break;
	}
	case 4: {
		HandledBy = "NOP Pushbuffer fixup";
		break;
	}
	case 5: {
		HandledBy = "NOP Fence";
		// TODO : Set event?
		break;
	}
	case 6: {
		XTL::X_D3DCALLBACK Callback = (XTL::X_D3DCALLBACK)NOP_Argument1;
		DWORD Context = NOP_Argument2;
		Callback(Context);
		HandledBy = "NOP read callback";
		break;
	}
	case 7: {
		HandledBy = "NOP write callback";
		break;
	}
	case 8: {
		HandledBy = "NOP DXT1 noise enable";
		break;
	}
	case 9: {
		const DWORD Register = NOP_Argument1;
		const DWORD Value = NOP_Argument2;
		NV2AInstance_Registers[Register / 4] = Value; // TODO : EmuNV2A_Write32(RegisterBase + Register, Value);
		HandledBy = "NOP write register";
		break;
	}
	default: {
		HandledBy = "NOP unknown";
		break;
	}
	}
}

float ZScale = 65535.0f; // TODO : Set to format-dependent divider

void NVPB_Clear()
{
	DWORD Flags = *pdwPushArguments; // NV2A_CLEAR_FLAGS

	// make adjustments to parameters to make sense with windows d3d
	DWORD PCFlags = XTL::EmuXB2PC_D3DCLEAR_FLAGS(Flags);
	{
		if (Flags & X_D3DCLEAR_TARGET) {
			// TODO: D3DCLEAR_TARGET_A, *R, *G, *B don't exist on windows
			if ((Flags & X_D3DCLEAR_TARGET) != X_D3DCLEAR_TARGET)
				EmuWarning("Unsupported : Partial D3DCLEAR_TARGET flag(s) for D3DDevice_Clear : 0x%.08X", Flags & X_D3DCLEAR_TARGET);
		}

		/* Do not needlessly clear Z Buffer
		if (Flags & X_D3DCLEAR_ZBUFFER) {
		if (!g_bHasDepthBits) {
		PCFlags &= ~D3DCLEAR_ZBUFFER;
		EmuWarning("Ignored D3DCLEAR_ZBUFFER flag (there's no Depth component in the DepthStencilSurface)");
		}
		}

		// Only clear depth buffer and stencil if present
		//
		// Avoids following DirectX Debug Runtime error report
		//    [424] Direct3D8: (ERROR) :Invalid flag D3DCLEAR_ZBUFFER: no zbuffer is associated with device. Clear failed.
		if (Flags & X_D3DCLEAR_STENCIL) {
		if (!g_bHasStencilBits) {
		PCFlags &= ~D3DCLEAR_STENCIL;
		EmuWarning("Ignored D3DCLEAR_STENCIL flag (there's no Stencil component in the DepthStencilSurface)");
		}
		}*/

		if (Flags & ~(X_D3DCLEAR_TARGET | X_D3DCLEAR_ZBUFFER | X_D3DCLEAR_STENCIL))
			EmuWarning("Unsupported Flag(s) for D3DDevice_Clear : 0x%.08X", Flags & ~(X_D3DCLEAR_TARGET | X_D3DCLEAR_ZBUFFER | X_D3DCLEAR_STENCIL));
	}

	// Since we filter the flags, make sure there are some left (else, clear isn't necessary) :
	if (PCFlags > 0)
	{
		using namespace XTL; // for NV2A symbols

		// Read NV2A clear arguments
		DWORD Color = NV2AInstance_Registers[NV2A_CLEAR_VALUE / 4];
		DWORD DepthStencil = NV2AInstance_Registers[NV2A_CLEAR_DEPTH_VALUE / 4];
		DWORD X12 = NV2AInstance_Registers[NV2A_CLEAR_X / 4];
		DWORD Y12 = NV2AInstance_Registers[NV2A_CLEAR_Y / 4];

		// Convert NV2A to PC Clear arguments
		XTL::D3DRECT ClearRect = { (X12 & 0xFFFF) + 1, (Y12 & 0xFFFF) + 1, X12 >> 16, Y12 >> 16 };
		float Z = (FLOAT)(DepthStencil >> 8) / ZScale;
		DWORD Stencil = DepthStencil & 0xFF;

		// CxbxUpdateActiveRenderTarget(); // TODO : Or should we have to call DxbxUpdateNativeD3DResources ?
		HRESULT hRet = g_pD3DDevice8->Clear(1, &ClearRect, PCFlags, Color, Z, Stencil);
		// DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->Clear");
	}

	HandledBy = "Clear";
}

void EmuNV2A_FlipStall()
{
//#ifdef DXBX_USE_D3D
	XTL::CxbxPresent();
//#endif
#ifdef DXBX_USE_OPENGL
	SwapBuffers(g_EmuWindowsDC); // TODO : Use glFlush() when single-buffered?
#endif

	HandledBy = "Swap";
}

#if 0 // TODO : Complete this Dxbx back-port
void EmuNV2A_SetRenderState()
{
	XTL::X_D3DRenderStateType XboxRenderState;

#ifdef DXBX_USE_OPENGL
	DWORD GLFlag = 0;
	char *FlagName = "";

	XboxRenderState = 0;
	switch (dwMethod) {
	case NV2A_LIGHTING_ENABLE: { GLFlag = GL_LIGHTING; FlagName = "GL_LIGHTING"; break; }
	case NV2A_CULL_FACE_ENABLE: { GLFlag = GL_CULL_FACE; FlagName = "GL_CULL_FACE"; break; }
	default: {
#endif
	XboxRenderState = CxbxXboxMethodToRenderState(dwMethod);
	if ((int)XboxRenderState < 0)
		CxbxKrnlCleanup("EmuNV2A_SetRenderState coupled to unknown method?");
#ifdef DXBX_USE_OPENGL
	switch (XboxRenderState) {
	case X_D3DRS_FOGENABLE: { GLFlag = GLFlag = GL_FOG; FlagName = "GL_FOG"; break; }
	case X_D3DRS_ALPHATESTENABLE: { GLFlag = GLFlag = GL_ALPHA_TEST; FlagName = "GL_ALPHA_TEST"; break; }
	case X_D3DRS_ALPHABLENDENABLE: { GLFlag = = GL_BLEND; FlagName = "GL_BLEND"; break; }
	case X_D3DRS_ZENABLE: { GLFlag = GL_DEPTH_TEST; FlagName = "GL_DEPTH_TEST"; break; }
	case X_D3DRS_DITHERENABLE: { GLFlag = GL_DITHER; FlagName = "GL_DITHER"; break; }
	case X_D3DRS_STENCILENABLE: { GLFlag = GL_STENCIL_TEST; FlagName = "GL_STENCIL_TEST"; break; }
	case X_D3DRS_NORMALIZENORMALS: { GLFlag = GL_NORMALIZE; FlagName = "GL_NORMALIZ"; break; }
	}
	}

	if (GLFlag > 0) {
		if (*pdwPushArguments != 0) {
			glEnable(GLFlag);
			HandledBy = "glEnable";
		}
		else {
			glDisable(GLFlag);
			HandledBy = "glDisable";
		}

		// HandledBy = HandledBy + '(' + FlagName + ')';
		return;
	}
#endif

	HandledBy = sprintf("SetRenderState(%-33s, 0x%.08X {=%s})", 
		CxbxRenderStateInfo[XboxRenderState].S,
		*pdwPushArguments,
		CxbxTypedValueToString(CxbxRenderStateInfo[XboxRenderState].T, *pdwPushArguments));
}
#endif

void NVPB_SetBeginEnd() // 0x000017FC
{
	if (*pdwPushArguments == 0) {
#ifdef _DEBUG_TRACK_PB
		if (bShowPB) {
			printf("DONE)\n");
		}
#endif
		HandledBy = "DrawEnd()";
	}
	else {
#ifdef _DEBUG_TRACK_PB
		if (bShowPB) {
			printf("PrimitiveType := %d)\n", *pdwPushArguments);
		}
#endif

		XboxPrimitiveType = (XTL::X_D3DPRIMITIVETYPE)*pdwPushArguments;

		HandledBy = "DrawBegin()";
	}
}

void NVPB_InlineVertexArray() // 0x1818
{
	using namespace XTL;

	// All Draw*() calls start with NV2A_VERTEX_BEGIN_END(PrimitiveType) and end with NV2A_VERTEX_BEGIN_END(0).
	// They differ by what they send in between those two commands :
	// DrawVerticesUP() sends NV2A_VERTEX_DATA(NoInc)+ (it copies vertices as-is)
	// DrawIndexedVerticesUP() sends NV2A_VERTEX_DATA(NoInc)+ too (it copies vertexes from a vertex-buffer using an index-buffer)
	// DrawVertices() sends NV2A_VB_VERTEX_BATCH(NoInc), 0x00001400(count|index)+ [we need a define for this]
	// DrawIndexedVertices() sends NV2A_VB_ELEMENT_U16(NoInc)+, 0x1808(one at most) [we need a define for this, probably NV2A_VB_ELEMENT_U32]
	HandledBy = "DrawVertices()"; 
	HandledCount = dwCount;

	pVertexData = (::PVOID)pdwPushArguments;

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
		printf("  dwCount : %d\n", dwCount);
		printf("  dwVertexShader : 0x%08X\n", dwVertexShader);
	}
#endif

	// render vertices
	if (dwVertexShader != -1) {
		uint VertexCount = (dwCount * sizeof(::DWORD)) / dwVertexStride;
		CxbxDrawContext DrawContext = {};
		DrawContext.XboxPrimitiveType = XboxPrimitiveType;
		DrawContext.dwVertexCount = VertexCount;
		DrawContext.pXboxVertexStreamZeroData = pVertexData;
		DrawContext.uiXboxVertexStreamZeroStride = dwVertexStride;
		DrawContext.hVertexShader = dwVertexShader;

		CxbxDrawPrimitiveUP(DrawContext);
	}
}

void NVPB_FixLoop() // 0x1808
{
	using namespace XTL;

	HandledBy = "DrawIndexedVertices()";
	HandledCount = dwCount;

	// Test case : Turok menu's
#ifdef _DEBUG_TRACK_PB
	if (bShowPB) {
		printf("  NVPB_FixLoop(%d)\n", dwCount);
		printf("\n");
		printf("  Index Array Data...\n");
		INDEX16 *pIndices = (INDEX16*)pdwPushArguments;
		for (uint s = 0; s < dwCount; s++) {
			if (s % 8 == 0)
				printf("\n  ");

			printf("  %.04X", *pIndices++);
		}

		printf("\n");
		printf("\n");
	}
#endif

	INDEX16 *pIndices = (INDEX16*)pdwPushArguments;
	for (uint mi = 0; mi < dwCount; mi++) {
		pIBMem[mi + 2] = pIndices[mi];
	}

	// perform rendering
	if (pIBMem[0] != 0xFFFF) {
		uint uiIndexCount = dwCount + 2;
#ifdef _DEBUG_TRACK_PB
		if (!g_PBTrackDisable.exists((void*)pdwOrigPushData))
#endif
			// render indexed vertices
		{
			if (!g_bPBSkipPusher) {
				if (IsValidCurrentShader()) {
					// TODO: This technically should be enabled
					XTL::DxbxUpdateDeferredStates(); // CxbxUpdateNativeD3DResources

					CxbxDrawContext DrawContext = {};
					DrawContext.XboxPrimitiveType = XboxPrimitiveType;
					DrawContext.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, uiIndexCount);
					DrawContext.hVertexShader = g_CurrentVertexShader;

					CxbxDrawIndexed(DrawContext, pIBMem);
				}
			}
		}
	}
}

void NVPB_InlineIndexArray() // 0x1800
{
	using namespace XTL;

	HandledBy = "DrawIndices()";
	HandledCount = dwCount;

	// Test case : Turok menu's

	pIndexData = (INDEX16*)pdwPushArguments;
#ifdef _DEBUG_TRACK_PB
	if (bShowPB) {
		printf("  NVPB_InlineIndexArray(0x%.08X, %d)...\n", pIndexData, dwCount);
		printf("\n");
		printf("  Index Array Data...\n");
		INDEX16 *pIndices = pIndexData;
		for (uint s = 0; s < dwCount; s++) {
			if (s % 8 == 0)
				printf("\n  ");

			printf("  %.04X", *pIndices++);
		}

		printf("\n");

		DbgDumpMesh(pIndexData, dwCount);
	}
#endif

	// perform rendering
	{
		uint dwIndexCount = dwCount * 2; // Each DWORD data in the pushbuffer carries 2 words

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
		if (!g_PBTrackDisable.exists((void*)pdwOrigPushData))
#endif
			// render indexed vertices
		{
			if (!g_bPBSkipPusher) {
				if (IsValidCurrentShader()) {
					// TODO: This technically should be enabled
					XTL::DxbxUpdateDeferredStates(); // CxbxUpdateNativeD3DResources

					CxbxDrawContext DrawContext = {};
					DrawContext.XboxPrimitiveType = XboxPrimitiveType;
					DrawContext.dwVertexCount = EmuD3DIndexCountToVertexCount(XboxPrimitiveType, dwIndexCount);
					DrawContext.hVertexShader = g_CurrentVertexShader;

					CxbxDrawIndexed(DrawContext, pIndexData);
				}
			}
		}
	}
}

void NVPB_SetVertexShaderConstantRegister()
{
	HandledBy = "SetVertexShaderConstantRegister";
}

void NVPB_SetVertexShaderConstants()
{
	using namespace XTL; // for NV2A symbols

	//assert(dwCount >= 4); // Input must at least be 1 set of coordinates
	//assert(dwCount & 3 == 0); // Input must be a multiple of 4

	// Make sure we use the correct index if we enter at an offset other than 0 :
	int Slot = (dwMethod - NV2A_VP_UPLOAD_CONST(0)) / 4;
	// Since we always start at NV2A_VP_UPLOAD_CONST__0, never handle more than allowed :
	//assert((Slot + (dwCount / 4)) <= NV2A_VP_UPLOAD_CONST__SIZE);

	// The VP_UPLOAD_CONST_ID GPU register is always pushed before the actual values, and contains the base Register for this batch :
	DWORD Register = NV2AInstance_Registers[NV2A_VP_UPLOAD_CONST_ID / 4] + Slot;
	void *pConstantData = &(NV2AInstance_Registers[dwMethod / 4]);
	DWORD ConstantCount = dwCount;

	// Just set the constants right from the pushbuffer, as they come in batches and won't exceed the native bounds :
	HRESULT hRet = g_pD3DDevice8->SetVertexShaderConstant(
		Register,
		pConstantData,
		ConstantCount
	);
	//DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetVertexShaderConstant");

	// Adjust the current register :
	Register += ConstantCount;
	NV2AInstance_Registers[NV2A_VP_UPLOAD_CONST_ID / 4] = Register; // TODO : Is this correct?

	HandledCount = dwCount;
	HandledBy = "SetVertexShaderConstant";
}

void NVPB_SetVertexData4f()
{
	// TODO : Collect g_InlineVertexBuffer_Table here (see EMUPATCH(D3DDevice_SetVertexData4f))
}

void NVPB_SetTextureState_BorderColor()
{
	using namespace XTL; // for NV2A symbols

	DWORD Stage = (dwMethod - NV2A_TX_BORDER_COLOR(0)) / 4;
	DWORD XboxValue = *pdwPushArguments;
	const XTL::D3DTEXTURESTAGESTATETYPE PCStateType = XTL::D3DSAMP_BORDERCOLOR;
	DWORD PCValue = 0; // TODO : DxbxTextureStageStateXB2PCCallback[PCStateType](XboxValue);

	HRESULT hRet = g_pD3DDevice8->SetTextureStageState(Stage,  PCStateType, PCValue);
	//DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetTextureStageState");
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

extern PPUSH XTL::EmuExecutePushBufferRaw
(
    PPUSH pdwPushData
	// TODO DWORD *pdwPushEnd
)
{
	static bool NV2ACallbacks_Initialized = false;
	if (!NV2ACallbacks_Initialized) {
		NV2ACallbacks_Initialized = true;
		// Set handlers that do more than just store data in registers :
		NV2ACallbacks[NV2A_NOP / 4] = EmuNV2A_NOP; // 0x0100
		NV2ACallbacks[NV2A_FLIP_STALL / 4] = EmuNV2A_FlipStall; // 0x0130 // // TODO : Should we trigger at NV2A_FLIP_INCREMENT_WRITE instead?
		NV2ACallbacks[NV2A_VERTEX_BEGIN_END / 4] = NVPB_SetBeginEnd; // NV097_SET_BEGIN_END; // 0x000017FC
		NV2ACallbacks[NV2A_VB_ELEMENT_U16 / 4] = NVPB_InlineIndexArray; // NV097_ARRAY_ELEMENT16; // 0x1800
		NV2ACallbacks[(NV2A_VB_ELEMENT_U16 + 8) / 4] = NVPB_FixLoop; // NV097_ARRAY_ELEMENT32; // 0x1808
		NV2ACallbacks[NV2A_VERTEX_DATA / 4] = NVPB_InlineVertexArray; // NV097_INLINE_ARRAY; // 0x1818
		// Not really needed, but helps debugging : 
		NV2ACallbacks[NV2A_VP_UPLOAD_CONST_ID / 4] = NVPB_SetVertexShaderConstantRegister; // NV097_SET_TRANSFORM_CONSTANT_LOAD; // 0x00001EA4
		for (int i = 0; i < 32; i++) {
			NV2ACallbacks[NV2A_VP_UPLOAD_CONST(i) / 4] = NVPB_SetVertexShaderConstants; // NV097_SET_TRANSFORM_CONSTANT; // 0x00000b80
		}
		NV2ACallbacks[NV2A_CLEAR_BUFFERS / 4] = NVPB_Clear; // 0x00001d94
		for (int i = 0; i < 16; i++) {
			NV2ACallbacks[NV2A_VTX_ATTR_4F_X(i) / 4] = NVPB_SetVertexData4f; // 0x00001a00
			// TODO : Register callbacks for other vertex attribute formats (SetVertexData2f, 2s, 4ub, 4s, Color)
		}
		for (int i = 0; i < 4; i++) {
			NV2ACallbacks[NV2A_TX_BORDER_COLOR(i) / 4] = NVPB_SetTextureState_BorderColor;// NV097_SET_TEXTURE_BORDER_COLOR // 0x00001b24
			// TODO : Register callbacks for other texture(-stage) related methods
		}
	}

	// Test case : XDK Sample BeginPush
	if (g_bSkipPush) {
		return pdwPushData; // TODO pdwPushEnd;
	}

	if (pdwPushData == NULL) {
		EmuWarning("pdwPushData is null");
		return NULL;
	}

    pdwOrigPushData = pdwPushData;

    pIndexData = NULL;
    pVertexData = NULL;

    dwVertexShader = -1;
    dwVertexStride = -1;

    // cache of last 4 indices
	pIBMem[0] = 0xFFFF;
	pIBMem[1] = 0xFFFF;
	pIBMem[2] = 0xFFFF;
	pIBMem[3] = 0xFFFF;

    XboxPrimitiveType = X_D3DPT_INVALID;

    #ifdef _DEBUG_TRACK_PB
    bShowPB = false;

    g_PBTrackTotal.insert((void*)pdwPushData);
    if (g_PBTrackShowOnce.remove((void*)pdwPushData) != NULL) {
        printf("\n");
        printf("\n");
        printf("  PushBuffer@0x%.08X...\n", pdwPushData);
        printf("\n");
        bShowPB = true;
    }
    #endif

	DbgPrintf("  NV2A run from 0x%.08X\n", pdwPushData);

	while (true) {
/* TODO :
		if (pdwPushData == pdwPushEnd) {
			break;
		}
*/
		char LogPrefixStr[200];
		int len = sprintf(LogPrefixStr, "  NV2A Get=$%.08X", pdwPushData);

		// Fetch method DWORD
		DWORD dwPushCommand = *pdwPushData++;
		if (dwPushCommand == 0) {
			DbgPrintf("%s BREAK at NULL method at 0x%.08X\n", LogPrefixStr, pdwPushData-1);
			break;
		}

		// Handle jumps and/or calls :
		DWORD PushType = PUSH_TYPE(dwPushCommand);
		if ((PushType == PUSH_TYPE_JMP_FAR) || (PushType == PUSH_TYPE_CALL_FAR)) {
			// Both 'jump' and 'call' just direct execution to the indicated address :
			pdwPushData = (PPUSH)(PUSH_ADDR_FAR(dwPushCommand) | MM_SYSTEM_PHYSICAL_MAP); // 0x80000000
			DbgPrintf("%s Jump far: 0x%.08X\n", LogPrefixStr, pdwPushData);
			continue;
		}

		// Handle instruction
		DWORD PushInstr = PUSH_INSTR(dwPushCommand);
		if (PushInstr == PUSH_INSTR_JMP_NEAR) {
			pdwPushData = (PPUSH)(PUSH_ADDR_NEAR(dwPushCommand) | MM_SYSTEM_PHYSICAL_MAP); // 0x80000000
			DbgPrintf("%s Jump near: 0x%.08X\n", LogPrefixStr, pdwPushData);
			continue;
		}

		// Get method, sub channel and count (should normally be at least 1)
		D3DPUSH_DECODE(dwPushCommand, dwMethod, dwSubCh, dwCount);
		if (dwCount == 0) {
			// NOP might get here, but without arguments it's really a no-op
			continue;
		}

		// Remember address of the argument(s)
		pdwPushArguments = pdwPushData;

		bool bNoInc = (PushInstr == PUSH_INSTR_IMM_NOINC);
		// Append a counter (variable part via %d, count already formatted) :
//		if (MayLog(lfUnit))
		{
			len += sprintf(LogPrefixStr + len, " %%2d/%2d:", dwCount); // intentional %% for StepNr
			if (dwSubCh > 0)
				len += sprintf(LogPrefixStr + len, " [SubCh:%d]", dwSubCh);

			if (bNoInc)
				len += sprintf(LogPrefixStr + len, " [NoInc]");
		}

		// Skip fetch-pointer over the arguments already
		pdwPushData += dwCount;
		// Initialize handled count & name to their default :
		HandledCount = 1;
		HandledBy = nullptr;

		// Simulate writes to the NV2A instance registers; We write all DWORDs before
		// executing them so that the callbacks can read this data via the named
		// NV2AInstance fields (see for example EmuNV2A_SetVertexShaderConstant) :
		// Note this only applies to 'inc' methods (no-inc methods read all data in-place).
		if ((dwSubCh == 0) && (!bNoInc)) {
			//assert((dwMethod + (dwCount * sizeof(DWORD))) <= sizeof(NV2AInstance_Registers));

			memcpy(&(NV2AInstance_Registers[dwMethod / 4]), (void*)pdwPushArguments, dwCount * sizeof(DWORD));
		}

		// Interpret GPU Instruction(s) :
		int StepNr = 1;
		while (dwCount > 0) {
			NV2ACallback_t NV2ACallback = nullptr;

			// Skip all commands not intended for channel 0 :
			if (dwSubCh > 0) {
				HandledCount = dwCount;
				HandledBy = " *CHANNEL IGNORED*";
			}
			else {
				// Assert(dwMethod < SizeOf(NV2AInstance));

				// For 'no inc' methods, write only the first register (in case it's referenced somewhere) :
				if (bNoInc)
					NV2AInstance_Registers[dwMethod / 4]  = *pdwPushArguments;

				// Retrieve the handler callback for this method (if any) :
				NV2ACallback = NV2ACallbacks[dwMethod / sizeof(DWORD)];
			}

//#ifdef DXBX_USE_D3D
			if (g_pD3DDevice8 == nullptr) {
				HandledBy = " *NO DEVICE*"; // Don't do anything if we have no device yet (should not occur anymore, but this helps spotting errors)
				NV2ACallback = nullptr;
			}
//#endif
/*#ifdef DXBX_USE_OPENGL
			if (g_EmuWindowsDC == 0){
				HandledBy = "*NO OGL DC*"; // Don't do anything if we have no device yet (should not occur anymore, but this helps spotting errors)
				NV2ACallback = nullptr;
			}
#endif*/
			// Before handling the method, display it's details :
			if (g_bPrintfOn) {
				printf("[0x%X] ", GetCurrentThreadId());
				printf(LogPrefixStr, StepNr);
				printf(" Method=%.04X Arg[0]=%.08X %s", dwMethod, *pdwPushArguments, NV2AMethodToString(dwMethod));
				if (HandledBy != nullptr) {
					printf(HandledBy);
				}

				printf("\n");
			}

			HandledBy = nullptr;
			if (NV2ACallback != nullptr) {
				NV2ACallback();
			}

			// If there are more details, print them now :
			if (HandledBy != nullptr) {
				DbgPrintf("  NV2A > %s\n", HandledBy);
			}


			// Since some instructions use less arguments, we repeat this loop
			// for the next instruction so any leftover values are handled there :
			pdwPushArguments += HandledCount;
			dwCount -= HandledCount;
			StepNr += HandledCount;

			// The no-increment flag applies to method only :
			if (!bNoInc) {
				if (HandledCount == 1)
					dwMethod += 4; // 1 method further
				else
					dwMethod += HandledCount * sizeof(DWORD); // TODO : Is this correct?

				// Remember the last two methods, in case we need to differentiate contexts (using SeenRecentMethod):
				PrevMethod[1] = PrevMethod[0];
				PrevMethod[0] = dwMethod;
			}

			// Re-initialize handled count & name to their default, for the next command :
			HandledCount = 1;
			HandledBy = nullptr;
		} // while (dwCount > 0)
	} //  while (true)

	// Fake a read by the Nv2A, by moving the DMA 'Get' location
	// up to where the pushbuffer is executed, so that the BusyLoop
	// in CDevice.Init finishes cleanly :
	g_pNV2ADMAChannel->Get = (PPUSH)pdwPushData;
	// TODO : We should probably set g_pNV2ADMAChannel->Put to the same value first?

	// We trigger the DMA semaphore by setting GPU time to CPU time - 2 :
	//*/*D3DDevice.*/m_pGpuTime = /*D3DDevice.*/*m_pCpuTime -2;

	// TODO : We should register vblank counts somewhere?

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

	return pdwPushData;
}

// timing thread procedure
XTL::DWORD WINAPI EmuThreadHandleNV2ADMA(XTL::LPVOID lpVoid)
{
	//using namespace XTL;

	DbgPrintf("NV2A : DMA thread started\n");

	// DxbxLogPushBufferPointers('NV2AThread');

#ifdef DXBX_USE_OPENGL
	// The OpenGL context must be created in the same thread that's going to do all drawing
	// (in this case it's the NV2A push buffer emulator thread doing all the drawing) :
	InitOpenGLContext();
#endif

	XTL::Pusher *pPusher = (XTL::Pusher*)(*((xbaddr *)XTL::Xbox_D3D__Device));

	DbgPrintf("NV2A : DMA thread is running\n");

	// Emulate the GPU engine here, by running the pushbuffer on the correct addresses :
	// Xbox KickOff() signals a work flush.
	// We wait for DEVICE_READ32(PFB) case NV_PFB_WBC sending us a signal
	while (WaitForSingleObject(ghNV2AFlushEvent, INFINITE) == WAIT_OBJECT_0) { // TODO -oDxbx: When do we break out of this while loop ?
		ResetEvent(ghNV2AFlushEvent); // TODO : Must this be done as soon as here, or when setting Get/after handling commands?

		if (XTL::m_pCPUTime == NULL)
			XTL::CxbxLocateCpuTime();

		// Don't process anything as long as the NV2A DMA channel isn't allocated yet (see DEVICE_WRITE32(PRAMIN) case NV_PRAMIN_DMA_LIMIT)
		if (g_pNV2ADMAChannel == NULL)
			continue;

		// DON'T check for (g_pNV2ADMAChannel->Put == NULL), as that's'the initial bootstrap address

		// Start at the DMA's 'Put' address
		xbaddr GPUStart = (xbaddr)g_pNV2ADMAChannel->Put | MM_SYSTEM_PHYSICAL_MAP; // 0x80000000
		// Run up to the end of the pushbuffer (as filled by software) :
		xbaddr GPUEnd = (xbaddr)pPusher->m_pPut; // OR with  MM_SYSTEM_PHYSICAL_MAP (0x80000000) doesn't seem necessary
		if (GPUEnd != GPUStart) {
			// Execute the instructions, this returns the address where execution stopped :
			GPUEnd = (xbaddr)XTL::EmuExecutePushBufferRaw((PPUSH)GPUStart);// , GPUEnd);
			// GPUEnd ^= MM_SYSTEM_PHYSICAL_MAP; // Clearing mask doesn't seem necessary
			// Signal that DMA has finished by resetting the GPU 'Get' pointer, so that busyloops will terminate
			// See EmuNV2A.cpp : DEVICE_READ32(USER) and DEVICE_WRITE32(USER)
			g_pNV2ADMAChannel->Put = (PPUSH)GPUEnd;
			// Register timestamp (needs an offset - but why?)
			*m_pGPUTime = *XTL::m_pCPUTime - 2;
			// TODO : Count number of handled commands here?
		}

		// Always set the ending address in DMA, so Xbox HwGet() will see it, breaking the BusyLoop() cycle
		g_pNV2ADMAChannel->Get = (PPUSH)GPUEnd;
	}

	DbgPrintf("NV2A : DMA thread is finished\n");
	return 0;
} // EmuThreadHandleNV2ADMA

HANDLE g_hNV2ADMAThread = NULL;

void CxbxInitializeNV2ADMA()
{
	if (ghNV2AFlushEvent == NULL) {
		DbgPrintf("NV2A : Creating flush event\n");
		ghNV2AFlushEvent = CreateEvent(
			NULL,                   // default security attributes
			TRUE,                   // manual-reset event
			FALSE,                  // initial state is nonsignaled
			TEXT("NV2AFlushEvent")  // object name
		);
	}

	if (g_hNV2ADMAThread == NULL) {
		// Create our DMA pushbuffer 'handling' thread :
		DbgPrintf("NV2A : Launching DMA handler thread\n");
		::DWORD dwThreadId = 0;
		g_hNV2ADMAThread = CreateThread(nullptr, 0, EmuThreadHandleNV2ADMA, nullptr, 0, &dwThreadId);
		// Make sure callbacks run on the same core as the one that runs Xbox1 code :
		SetThreadAffinityMask(g_hNV2ADMAThread, g_CPUXbox);
		// We set the priority of this thread a bit higher, to assure reliable timing :
		SetThreadPriority(g_hNV2ADMAThread, THREAD_PRIORITY_ABOVE_NORMAL);
	}
}

#ifdef _DEBUG_TRACK_PB
void DbgDumpMesh(XTL::INDEX16 *pIndexData, DWORD dwCount)
{
    if (!XTL::IsValidCurrentShader() || (dwCount == 0))
        return;

    // retrieve stream data
    char szFileName[128];
    sprintf(szFileName, "D:\\_cxbx\\mesh\\CxbxMesh-0x%.08X.x", pIndexData);
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
        fprintf(dbgVertices, "//  Vertex Stream Data (0x%.08X)...\n", pVBData);
        fprintf(dbgVertices, "//\n");
#if 0
		fprintf(dbgVertices, "//  Format : %d\n", VBDesc.Format);
        fprintf(dbgVertices, "//  Size   : %d bytes\n", VBDesc.Size);
        fprintf(dbgVertices, "//  FVF    : 0x%.08X\n", VBDesc.FVF);
#endif
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

		XTL::INDEX16 *pIndexValues = pIndexData;

        max = dwCount;

        DWORD a = *pIndexValues++;
        DWORD b = *pIndexValues++;
        DWORD c = *pIndexValues++;

        DWORD la = a,lb = b,lc = c;

        for(uint i=2;i<max;i++)
        {
            fprintf(dbgVertices, "      3;%d,%d,%d;%s\n",
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