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
// *   Cxbx->Win32->CxbxKrnl->EmuD3D->State.cpp
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

#include "Logging.h"
#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"
//#include "EmuNV2A.h"
#include "Convert.h" // GetDxbxRenderStateInfo()

// D3D build version
extern uint32 g_BuildVersion;

extern int g_iWireframe;
extern int X_D3DSCM_CORRECTION_VersionDependent;

namespace XTL {

// TODO : Set these after symbols are scanned/loaded :
DWORD *Xbox_D3D__Device = NULL; // The Xbox1 D3D__Device
#ifdef UNPATCH_STREAMSOURCE
X_Stream *Xbox_g_Stream = { NULL }; // The Xbox1 g_Stream[16] array
#endif
DWORD *Xbox_D3D__RenderState = NULL;
// Texture state lookup table (same size in all XDK versions, so defined as a fixed size array) :
DWORD *Xbox_D3D_TextureState = NULL; // [X_D3DTSS_STAGECOUNT][X_D3DTSS_STAGESIZE] = [(Stage * X_D3DTSS_STAGESIZE) + Offset]

// Deferred state lookup tables
DWORD *Xbox_D3D__RenderState_Deferred = NULL;

// Dxbx addition : Dummy value (and pointer to that) to transparently ignore unsupported render states :
X_D3DRENDERSTATETYPE DummyRenderStateValue = X_D3DRS_FIRST;
X_D3DRENDERSTATETYPE *DummyRenderState = &DummyRenderStateValue; // Unsupported states share this pointer value

// XDK version independent renderstate table, containing pointers to the original locations.
X_D3DRENDERSTATETYPE *EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED + 1] = { NULL }; // 1 extra for the unsupported value itself


DWORD (*DxbxTextureStageStateXB2PCCallback[X_D3DTSS_LAST + 1])(DWORD Value);
DWORD (*DxbxRenderStateXB2PCCallback[X_D3DRS_LAST + 1])(DWORD Value);
X_D3DRENDERSTATETYPE DxbxMapActiveVersionToMostRecent[X_D3DRS_LAST + 1];
DWORD DxbxMapMostRecentToActiveVersion[X_D3DRS_LAST + 1];

// TODO : Extend RegisterAddressLabel so that it adds elements to g_SymbolAddresses too (for better debugging)
#define RegisterAddressLabel(Address, fmt, ...) \
	DbgPrintf("HLE : 0x%p -> "##fmt##"\n", (void *)Address, __VA_ARGS__)

void DxbxBuildRenderStateMappingTable()
{
	if (g_BuildVersion <= 4361)
		X_D3DSCM_CORRECTION_VersionDependent = X_D3DSCM_CORRECTION;
	else
		X_D3DSCM_CORRECTION_VersionDependent = 0;

	// Build a table with converter functions for all renderstates :
	for (int i = X_D3DRS_FIRST; i <= X_D3DRS_LAST; i++)
		DxbxRenderStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[GetDxbxRenderStateInfo(i).T].F);

	// Build a table with converter functions for all texture stage states :
	for (int i = X_D3DTSS_FIRST; i <= X_D3DTSS_LAST; i++)
		DxbxTextureStageStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[DxbxTextureStageStateInfo[i].T].F);

	// Loop over all latest (5911) states :
	DWORD XDKVersion_D3DRS = X_D3DRS_FIRST;
	for (X_D3DRENDERSTATETYPE State = X_D3DRS_FIRST; State <= X_D3DRS_LAST; State++)
	{
		// Check if this state is available in the active SDK version :
		if (g_BuildVersion >= GetDxbxRenderStateInfo(State).V)
		{
			// If it is available, register this offset in the various mapping tables we use :
			DxbxMapActiveVersionToMostRecent[XDKVersion_D3DRS] = State;
			DxbxMapMostRecentToActiveVersion[State] = XDKVersion_D3DRS;
			EmuMappedD3DRenderState[State] = &(Xbox_D3D__RenderState[XDKVersion_D3DRS]);
			// Step to the next offset :
			XDKVersion_D3DRS++;
		}
		else
		{
			// When unavailable, apply a dummy pointer, and *don't* increment the version dependent state,
			// so the mapping table will correspond to the actual (version dependent) layout :
			// DxbxMapActiveVersionToMostRecent shouldn't be set here, as there's no element for this state!
			DxbxMapMostRecentToActiveVersion[State] = X_D3DRS_UNSUPPORTED;
			EmuMappedD3DRenderState[State] = DummyRenderState;
		}
	}
}

void DxbxBuildRenderStateMappingTable2()
{
	// Log the start address of the "deferred" render states (not needed anymore, just to keep logging the same) :
	if (Xbox_D3D__RenderState != NULL)
	{
		// Calculate the location of D3DDeferredRenderState via an XDK-dependent offset to Xbox_D3D__RenderState :
		DWORD XDKVersion_D3DRS_DEFERRED_FIRST = DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST];

		if (Xbox_D3D__RenderState_Deferred == NULL) {
			Xbox_D3D__RenderState_Deferred = Xbox_D3D__RenderState + XDKVersion_D3DRS_DEFERRED_FIRST;
			RegisterAddressLabel(Xbox_D3D__RenderState_Deferred, "Xbox_D3D__RenderState_Deferred");
		}
		else
			if (Xbox_D3D__RenderState_Deferred != Xbox_D3D__RenderState + XDKVersion_D3DRS_DEFERRED_FIRST)
				CxbxKrnlCleanup("DxbxBuildRenderStateMappingTable2 : Xbox D3D__RenderState_Deferred already set differently?");
	}
	else
	{
		// TEMPORARY work-around until Xbox_D3D__RenderState is determined via OOVPA symbol scanning;
		// map all render states based on the first deferred render state (which we have the address
		// of in Xbox_D3D__RenderState_Deferred) :

		// assert(Xbox_D3D__RenderState_Deferred != NULL);

		int delta = (int)(Xbox_D3D__RenderState_Deferred - DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST]);
		for (X_D3DRENDERSTATETYPE State = X_D3DRS_FIRST; State <= X_D3DRS_LAST; State++) {
			if (EmuMappedD3DRenderState[State] != DummyRenderState) {
				DWORD XDKVersion_D3DRS = DxbxMapMostRecentToActiveVersion[State];
				EmuMappedD3DRenderState[State] += delta / sizeof(DWORD); // Increment per DWORD (not per 4!)
				RegisterAddressLabel(EmuMappedD3DRenderState[State], "D3D__RenderState[%d/*=%s*/]", 
					XDKVersion_D3DRS,
					GetDxbxRenderStateInfo(State).S + 2); // Skip "X_" prefix
				// TODO : Should we label "g_Device." members too?
			}
		}

		Xbox_D3D__RenderState = EmuMappedD3DRenderState[X_D3DRS_FIRST];
	}

	// Initialize the dummy render state :
	EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED] = DummyRenderState;
}

// Converts the input render state from a version-dependent into a version-neutral value.
X_D3DRENDERSTATETYPE DxbxVersionAdjust_D3DRS(const X_D3DRENDERSTATETYPE XboxRenderState_VersionDependent)
{
	return DxbxMapActiveVersionToMostRecent[XboxRenderState_VersionDependent];
}

const DWORD OLD_X_D3DTSS_COLOROP = 0;
const DWORD OLD_X_D3DTSS_TEXTURETRANSFORMFLAGS = 9;
const DWORD OLD_X_D3DTSS_ADDRESSU = 10;
const DWORD OLD_X_D3DTSS_ALPHAKILL = 21;
// For 3925, the actual D3DTSS flags have different values.
// This function maps new indexes to old ones, so that we
// can read a specific member from the emulated XBE's
// XTL::Xbox_D3D_TextureState buffer.
X_D3DTEXTURESTAGESTATETYPE DxbxFromNewVersion_D3DTSS(const X_D3DTEXTURESTAGESTATETYPE NewValue)
{
	X_D3DTEXTURESTAGESTATETYPE Result = NewValue;

	if (g_BuildVersion <= 3925)
	{
		// In SDK 3925 (or at somewhere else between 3911 and 4361), the deferred texture states where switched;
		// D3DTSS_COLOROP ..D3DTSS_TEXTURETRANSFORMFLAGS ranged  0.. 9 which has become 12..21
		// D3DTSS_ADDRESSU..D3DTSS_ALPHAKILL             ranged 10..21 which has become  0..11
		if ((NewValue <= X_D3DTSS_TEXTURETRANSFORMFLAGS))
			if ((NewValue <= X_D3DTSS_ALPHAKILL))
				Result += OLD_X_D3DTSS_ADDRESSU;
			else
				Result -= /*NEW*/X_D3DTSS_COLOROP;
	}

	return Result;
}

X_D3DTEXTURESTAGESTATETYPE DxbxFromOldVersion_D3DTSS(const X_D3DTEXTURESTAGESTATETYPE OldValue)
{
	X_D3DTEXTURESTAGESTATETYPE Result = OldValue;
	if (g_BuildVersion <= 3925)
	{
		// In SDK 3925 (or at somewhere else between 3911 and 4361), the deferred texture states where switched;
		// D3DTSS_COLOROP ..D3DTSS_TEXTURETRANSFORMFLAGS ranged  0.. 9 which has become 12..21
		// D3DTSS_ADDRESSU..D3DTSS_ALPHAKILL             ranged 10..21 which has become  0..11
		if ((OldValue <= OLD_X_D3DTSS_ALPHAKILL))
			if ((OldValue <= OLD_X_D3DTSS_TEXTURETRANSFORMFLAGS))
				Result += 12;
			else
				Result -= 10;
	}

	if (Result > X_D3DTSS_LAST)
		Result = X_D3DTSS_UNSUPPORTED;

	return Result;
}

DWORD TransferredRenderStateValues[X_D3DRS_LAST + 1] = { X_D3DRS_UNKNOWN };

DWORD XTL::Dxbx_SetRenderState(const X_D3DRENDERSTATETYPE XboxRenderState, DWORD XboxValue)
{
	D3DRENDERSTATETYPE PCRenderState;
	DWORD PCValue;

//	LOG_INIT // Allows use of DEBUG_D3DRESULT

	TransferredRenderStateValues[XboxRenderState] = XboxValue;

	const RenderStateInfo &Info = GetDxbxRenderStateInfo(XboxRenderState);

	// Check if the render state is mapped :
	if (EmuMappedD3DRenderState[XboxRenderState] == DummyRenderState)
	{
		CxbxKrnlCleanup("Unsupported RenderState : %s (0x%p)", Info.S, (DWORD)XboxRenderState);
		return XboxValue;
	}

	// Skip Xbox extensions :
	if (Info.PC == D3DRS_UNSUPPORTED)
		return XboxValue;

	// Disabled, as it messes up Nvidia rendering too much :
	//  // Dxbx addition : Hack for Smashing drive (on ATI X1300), don't transfer fog (or everything becomes opaque) :
	//  if (IsRunning(TITLEID_SmashingDrive)
	//      && (XboxRenderState  in [X_D3DRS_FOGSTART, X_D3DRS_FOGEND, X_D3DRS_FOGDENSITY]))
	//    return Result;

	// Pixel shader constants are handled in DxbxUpdateActivePixelShader :
	if (XboxRenderState >= X_D3DRS_PSCONSTANT0_0 && XboxRenderState <= X_D3DRS_PSCONSTANT1_7)
		return XboxValue;
	if (XboxRenderState == X_D3DRS_PSFINALCOMBINERCONSTANT0)
		return XboxValue;
	if (XboxRenderState == X_D3DRS_PSFINALCOMBINERCONSTANT1)
		return XboxValue;

	if (XboxRenderState >= X_D3DRS_DEFERRED_FIRST && XboxRenderState <= X_D3DRS_DEFERRED_LAST)
	{
		// Skip unspecified deferred render states :
		if (XboxValue == X_D3DRS_UNKNOWN)
			return XboxValue;
	}

	/*
	case X_D3DRS_TEXTUREFACTOR:
	{
	// TODO : If no pixel shader is set, initialize all 16 pixel shader constants
	//        (X_D3DRS_PSCONSTANT0_0..X_D3DRS_PSCONSTANT1_7) to this value too.
	break;
	}

	case X_D3DRS_CULLMODE:
	{
	if (XboxValue > (DWORD)X_D3DCULL_NONE)
	; // TODO : Update X_D3DRS_FRONTFACE too

	break;
	}
	*/
	if (XboxRenderState == X_D3DRS_FILLMODE)
	{
		// Configurable override on fillmode :
		switch (g_iWireframe) {
		case 0: break; // Use fillmode specified by the XBE
		case 1: XboxValue = (DWORD)X_D3DFILL_WIREFRAME; break;
		default: XboxValue = (DWORD)X_D3DFILL_POINT;
		}
	}

	// Map the Xbox state to a PC state, and check if it's supported :
	PCRenderState = Info.PC;
	if (PCRenderState == D3DRS_UNSUPPORTED)
	{
		EmuWarning("%s is not supported!", Info.S);
		return XboxValue;
	}

	if (g_pD3DDevice8 == nullptr)
		return XboxValue;

	// Convert the value from Xbox format into PC format, and set it locally :
	PCValue = DxbxRenderStateXB2PCCallback[XboxRenderState](XboxValue);

	HRESULT hRet;
#if DXBX_USE_D3D9
	switch (XboxRenderState) {
	case X_D3DRS_EDGEANTIALIAS:
		break; // TODO -oDxbx : What can we do to support this?
	case X_D3DRS_ZBIAS:
	{
		// TODO -oDxbx : We need to calculate the sloped scale depth bias, here's what I know :
		// (see http://blog.csdn.net/qq283397319/archive/2009/02/14/3889014.aspx)
		//   bias = (max * D3DRS_SLOPESCALEDEPTHBIAS) + D3DRS_DEPTHBIAS (which is Value here)
		// > bias - Value = max * D3DRS_SLOPESCALEDEPTHBIAS
		// > D3DRS_SLOPESCALEDEPTHBIAS = (bias - Value) / max
		// TODO : So, what should we use as bias and max?
		hRet = g_pD3DDevice8->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, F2DW(1.0)); // For now.
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderState");
		hRet = g_pD3DDevice8->SetRenderState(D3DRS_DEPTHBIAS, PCValue);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderState");
		break;
	}
	default:
		hRet = g_pD3DDevice8->SetRenderState(PCRenderState, PCValue);
		DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderState");
	}
#else
	hRet = g_pD3DDevice8->SetRenderState(PCRenderState, PCValue);
	//	DEBUG_D3DRESULT(hRet, "g_pD3DDevice8->SetRenderState");
#endif

	return PCValue;
}

bool  TransferAll = true;

void DxbxTransferRenderState(const X_D3DRENDERSTATETYPE XboxRenderState)
{
	// Check if (this render state is supported (so we don't trigger a warning) :
	if (EmuMappedD3DRenderState[XboxRenderState] == DummyRenderState)
		return;

	if (GetDxbxRenderStateInfo(XboxRenderState).PC == D3DRS_UNSUPPORTED)
		return;

	// Read the current Xbox value, and set it locally :
	DWORD XboxValue = CxbxGetRenderState(XboxRenderState);
	// Prevent setting unchanged values :
	if (TransferAll || (TransferredRenderStateValues[XboxRenderState] != XboxValue))
		Dxbx_SetRenderState(XboxRenderState, XboxValue);
}

DWORD TransferredTextureStageStateValues[X_D3DTSS_STAGECOUNT * X_D3DTSS_STAGESIZE] = { X_D3DTSS_UNKNOWN };

// TODO : Move this
DWORD Cxbx_SetTextureStageState(DWORD Stage, X_D3DTEXTURESTAGESTATETYPE State, DWORD XboxValue)
{
	TransferredTextureStageStateValues[(Stage * X_D3DTSS_STAGESIZE) + State] = XboxValue;

	D3DTEXTURESTAGESTATETYPE PCState = EmuXB2PC_D3DTSS(State);

	// Skip unsupported Xbox extensions :
	if (PCState == D3DSAMP_UNSUPPORTED)
		// TODO -oDxbx : Emulate these Xbox extensions somehow
		return XboxValue;

	// Convert Xbox value to PC value for current texture stage state :
	DWORD PCValue = DxbxTextureStageStateXB2PCCallback[State](XboxValue);

#ifdef DXBX_USE_D3D9
	// For Direct3D9, everything below D3DSAMP_MAXANISOTROPY needs to call SetSamplerState :
	if (State <= X_D3DTSS_MAXANISOTROPY) {
		aDirect3DDevice8->SetSamplerState(Stage, PCState, PCValue);
		return PCValue;
	}
#endif

	g_pD3DDevice8->SetTextureStageState(Stage, PCState, PCValue);
	return PCValue;
}

void DxbxTransferTextureStageState(int Stage, X_D3DTEXTURESTAGESTATETYPE State)
{
	// Skip unsupported Xbox extensions :
	if (DxbxTextureStageStateInfo[State].PC == D3DSAMP_UNSUPPORTED)
		// TODO -oDxbx : Emulate these Xbox extensions somehow
		return;

	DWORD XboxValue = Xbox_D3D_TextureState[(Stage * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(State)];
	if (XboxValue == X_D3DTSS_UNKNOWN)
		return;

	// Transfer over the deferred texture stage state to PC :

	// Prevent setting unchanged values :
	if (TransferAll || (TransferredTextureStageStateValues[(Stage * X_D3DTSS_STAGESIZE) + State] != XboxValue))
		Cxbx_SetTextureStageState(Stage, State, XboxValue);
} // DxbxTransferTextureStageState

void CxbxPitchedCopy(BYTE *pDest, BYTE *pSrc, DWORD dwDestPitch, DWORD dwSrcPitch, DWORD dwWidthInBytes, DWORD dwHeight)
{
	// No conversion needed, copy as efficient as possible
	if ((dwSrcPitch == dwWidthInBytes) && (dwDestPitch == dwWidthInBytes))
	{
		// source and destination rows align, so copy all rows in one go
		memcpy(pDest, pSrc, dwHeight * dwWidthInBytes);
	}
	else
	{
		// copy source to destination per row
		for (DWORD v = 0; v < dwHeight; v++)
		{
			memcpy(pDest, pSrc, dwWidthInBytes);
			pDest += dwDestPitch;
			pSrc += dwSrcPitch;
		}
	}
}

void DxbxTransferTextureStage(int Stage, int StateFrom = X_D3DTSS_DEFERRED_FIRST, int StateTo = X_D3DTSS_DEFERRED_LAST)
{
	for (int State = StateFrom; State <= StateTo; State++)
		DxbxTransferTextureStageState(Stage, State);
}

void DxbxUpdateDeferredStates()
{
	// Generic transfer of all Xbox deferred render states to PC :
	for (int State = X_D3DRS_DEFERRED_FIRST; State <= X_D3DRS_DEFERRED_LAST; State++)
		DxbxTransferRenderState((X_D3DRENDERSTATETYPE)State);

	TransferAll = false; // TODO : When do we need to reset this to True?

	// Certain D3DTS values need to be checked on each Draw[Indexed]^Vertices
	if (Xbox_D3D_TextureState == nullptr)
		return;

	if (CxbxGetRenderState(X_D3DRS_POINTSPRITEENABLE) == (DWORD)TRUE) // Dxbx note : DWord cast to prevent warning
	{
#if 1	// TODO : Why must we copy the texure from stage 3 to 0 for X_D3DRS_POINTSPRITEENABLE?
		XTL::IDirect3DBaseTexture8 *pHostBaseTexture = nullptr;

		// Copy the point sprites texture from stage 3 to 0
		// (Not doing this makes PointSprites XDK sample show rectangular dots.)
		if (SUCCEEDED(g_pD3DDevice8->GetTexture(3, &pHostBaseTexture)))
		{
			g_pD3DDevice8->SetTexture(0, pHostBaseTexture);
			if (pHostBaseTexture != nullptr)
				pHostBaseTexture->Release(); // Prevent memory leaks
		}
#endif
		// Transfer other texture stages for stage 3 too (not just the deferred ones)
		DxbxTransferTextureStage(3, X_D3DTSS_OTHER_FIRST, X_D3DTSS_OTHER_LAST);
	}

	// Transfer all deferred texture stage state's from Xbox to Host in the order 0, 3, 1 and 2
	DxbxTransferTextureStage(0);
	DxbxTransferTextureStage(3);
	DxbxTransferTextureStage(1);
	DxbxTransferTextureStage(2);

	// Dxbx note : If we don't transfer the stages in this order, many XDK samples
	// either don't show the controller image in their help screen, or (like in
	// the 'Tiling' and 'BeginPush' XDK samples) colors & textures are wrong!

#if 0 // TODO : Do we need to do this? Dxbx doesn't...
    if(g_bFakePixelShaderLoaded)
    {
        g_pD3DDevice8->SetRenderState(D3DRS_FOGENABLE, FALSE);

        // programmable pipeline
        //*
        for(int v=0;v<4;v++)
        {
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_DISABLE);
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        }
        //*/

        // fixed pipeline
        /*
        for(int v=0;v<4;v++)
        {
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP,   D3DTOP_MODULATE);
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLORARG2, D3DTA_CURRENT);

            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        }

        g_pD3DDevice8->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        g_pD3DDevice8->SetRenderState(D3DRS_LIGHTING,TRUE);
        g_pD3DDevice8->SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFF);
        //*/
    }
#endif
} // DxbxUpdateDeferredStates


void CxbxInitializeTextureStageStates()
{
	for (int Stage = 0; Stage < X_D3DTSS_STAGECOUNT; Stage++) {
		for (X_D3DTEXTURESTAGESTATETYPE State = X_D3DTSS_FIRST; State <= X_D3DTSS_LAST; State++) {
			DWORD NewVersion_TSS = DxbxFromOldVersion_D3DTSS(State); // Map old to new
			void *Addr = &(Xbox_D3D_TextureState[(Stage * X_D3DTSS_STAGESIZE) + State]);
			RegisterAddressLabel(Addr, "D3D__TextureState[/*Stage*/%d][%d/*=%s*/]",
				Stage, State,
				DxbxTextureStageStateInfo[NewVersion_TSS].S + 2); // Skip "X_" prefix
		}
	}
}

void InitD3DDeferredStates()
{
	CxbxInitializeTextureStageStates();

	DxbxBuildRenderStateMappingTable2();

#if 1 // Prevent CxbxKrnlCleanup calls from EmuXB2PC_* functions, by resetting cases without a 0 value

	// This reset prevents CxbxKrnlCleanup calls from EmuXB2PC_D3DBLENDOP
	*EmuMappedD3DRenderState[X_D3DRS_BLENDOP] = X_D3DRS_UNKNOWN;

	for (int s = 0; s < X_D3DTSS_STAGECOUNT; s++) {
		// This reset prevents CxbxKrnlCleanup calls from EmuXB2PC_D3DTEXTUREADDRESS
		Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(X_D3DTSS_ADDRESSU)] = X_D3DTSS_UNKNOWN;
		Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(X_D3DTSS_ADDRESSV)] = X_D3DTSS_UNKNOWN;
		Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(X_D3DTSS_ADDRESSW)] = X_D3DTSS_UNKNOWN;
		// This reset prevents CxbxKrnlCleanup calls from EmuXB2PC_D3DTEXTUREOP
		Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(X_D3DTSS_COLOROP)] = X_D3DTSS_UNKNOWN;
		Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(X_D3DTSS_ALPHAOP)] = X_D3DTSS_UNKNOWN;
#if 0
		// Fake all transfers, perhaps that helps X-Marbles?
		for (int v = 0; v < X_D3DTSS_STAGESIZE; v++) {
			int HostIndex = (s * X_D3DTSS_STAGESIZE) + v;
			int XboxIndex = (s * X_D3DTSS_STAGESIZE) + DxbxFromNewVersion_D3DTSS(v);
			DWORD XboxValue = Xbox_D3D_TextureState[XboxIndex];
			printf("Initial Xbox_D3D_TextureState[%d][%d/*=%s*/] = 0x%.08X \n", s, v,
				DxbxTextureStageStateInfo[v].S, XboxValue);

			TransferredTextureStageStateValues[HostIndex] = XboxValue;
		}
#endif
	}

#if 0
	for (int v = X_D3DRS_FIRST; v <= X_D3DRS_LAST; v++) {
		DWORD XboxValue = *EmuMappedD3DRenderState[v];
		printf("Initial Xbox_D3D_RenderState[%d/*=%s*/] = 0x%.08X \n", v,
			GetDxbxRenderStateInfo(v).S, XboxValue);
		TransferredRenderStateValues[v] = XboxValue;
	}
#endif
#else // Old, fix-em-all approach :
	for (int v = 0; v < 44; v++) {
		Xbox_D3D__RenderState_Deferred[v] = X_D3DRS_UNKNOWN;
	}

	for (int s = 0; s < X_D3DTSS_STAGECOUNT; s++) {
		for (int v = 0; v < X_D3DTSS_STAGESIZE; v++)
			Xbox_D3D_TextureState[(s * X_D3DTSS_STAGESIZE) + v] = X_D3DTSS_UNKNOWN;
	}
#endif
}

}; // end of namespace XTL

