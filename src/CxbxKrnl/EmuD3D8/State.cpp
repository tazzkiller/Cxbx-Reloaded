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
#define _XBOXKRNL_DEFEXTRN_

#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuXTL.h"

extern uint32 g_BuildVersion; // D3D build version
extern int X_D3DSCM_CORRECTION_VersionDependent;

namespace XTL {

// Texture state lookup table (same size in all XDK versions, so defined as a fixed size array) :
DWORD *Xbox_D3D_TextureState = NULL; // [X_D3DTSS_STAGECOUNT][X_D3DTSS_STAGESIZE] = [(Stage * X_D3DTSS_STAGESIZE) + Offset]

// Deferred state lookup tables
DWORD *Xbox_D3D__RenderState_Deferred = NULL;

DWORD *Xbox_D3D__RenderState = NULL; // Set by CxbxInitializeEmuMappedD3DRenderState()

// Dxbx addition : Dummy value (and pointer to that) to transparently ignore unsupported render states :
DWORD DummyRenderStateValue = X_D3DRS_FIRST;
DWORD *DummyRenderState = &DummyRenderStateValue; // Unsupported states share this pointer value

// XDK version independent renderstate table, containing pointers to the original locations.
DWORD *XTL::EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED + 1] = { NULL }; // 1 extra for the unsupported value itself

DWORD DxbxMapMostRecentToActiveVersion[X_D3DRS_LAST + 1];

// TODO : Extend RegisterAddressLabel so that it adds elements to g_SymbolAddresses too (for better debugging)
#define RegisterAddressLabel(Address, fmt, ...) \
	DbgPrintf("HLE : 0x%p -> "##fmt##"\n", (void *)Address, __VA_ARGS__)

void DxbxBuildRenderStateMappingTable() // TODO : Rename to distinct from CxbxInitializeEmuMappedD3DRenderState()
{
	if (g_BuildVersion <= 4361)
		X_D3DSCM_CORRECTION_VersionDependent = X_D3DSCM_CORRECTION;
	else
		X_D3DSCM_CORRECTION_VersionDependent = 0;

#if 0 // See https://github.com/PatrickvL/Cxbx-Reloaded/tree/WIP_LessVertexPatching
	// Build a table with converter functions for all renderstates :
	for (int i = X_D3DRS_FIRST; i <= X_D3DRS_LAST; i++)
		DxbxRenderStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[GetDxbxRenderStateInfo(i).T].F);

	// Build a table with converter functions for all texture stage states :
	for (int i = X_D3DTSS_FIRST; i <= X_D3DTSS_LAST; i++)
		DxbxTextureStageStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[DxbxTextureStageStateInfo[i].T].F);

#endif
	// Loop over all latest (5911) states :
	DWORD XDKVersion_D3DRS = X_D3DRS_FIRST;
	for (int /*X_D3DRENDERSTATETYPE*/ State = X_D3DRS_FIRST; State <= X_D3DRS_LAST; State++)
	{
		// Check if this state is available in the active SDK version :
		if (g_BuildVersion >= GetDxbxRenderStateInfo(State).V)
		{
			// If it is available, register this offset in the various mapping tables we use :
#if 0 // See https://github.com/PatrickvL/Cxbx-Reloaded/tree/WIP_LessVertexPatching
			DxbxMapActiveVersionToMostRecent[XDKVersion_D3DRS] = (X_D3DRENDERSTATETYPE)State;
#endif
			DxbxMapMostRecentToActiveVersion[State] = XDKVersion_D3DRS;
			// Step to the next offset :
			XDKVersion_D3DRS++;
		}
		else
		{
			// When unavailable, apply a dummy pointer, and *don't* increment the version dependent state,
			// so the mapping table will correspond to the actual (version dependent) layout :
			// DxbxMapActiveVersionToMostRecent shouldn't be set here, as there's no element for this state!
			DxbxMapMostRecentToActiveVersion[State] = X_D3DRS_UNSUPPORTED;
		}
	}
}

void CxbxInitializeEmuMappedD3DRenderState() // TODO : Rename to distinct from DxbxBuildRenderStateMappingTable()
{
	// Log the start address of the "deferred" render states (not needed anymore, just to keep logging the same) :
	if (Xbox_D3D__RenderState != NULL) {
		// Calculate the location of D3DDeferredRenderState via an XDK-dependent offset to Xbox_D3D__RenderState :
		DWORD XDKVersion_D3DRS_DEFERRED_FIRST = DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST];

		if (Xbox_D3D__RenderState_Deferred == NULL) {
			Xbox_D3D__RenderState_Deferred = Xbox_D3D__RenderState + XDKVersion_D3DRS_DEFERRED_FIRST;
			RegisterAddressLabel(Xbox_D3D__RenderState_Deferred, "Xbox_D3D__RenderState_Deferred");
		}
		else
			if (Xbox_D3D__RenderState_Deferred != Xbox_D3D__RenderState + XDKVersion_D3DRS_DEFERRED_FIRST)
				CxbxKrnlCleanup("CxbxInitializeEmuMappedD3DRenderState : Xbox D3D__RenderState_Deferred already set differently?");
	}
	else {
		// TEMPORARY work-around until Xbox_D3D__RenderState is determined via OOVPA symbol scanning;
		// Map all render states based on the first deferred render state (which we have the address
		// of in Xbox_D3D__RenderState_Deferred) :
		if (Xbox_D3D__RenderState_Deferred != NULL)
			Xbox_D3D__RenderState = (DWORD*)(Xbox_D3D__RenderState_Deferred - DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST]);
		else
			CxbxKrnlCleanup("CxbxInitializeEmuMappedD3DRenderState : Missing Xbox D3D__RenderState and D3D__RenderState_Deferred!");
	}

	for (int /*X_D3DRENDERSTATETYPE*/ rs = X_D3DRS_FIRST; rs <= X_D3DRS_LAST; rs++) {
		DWORD XDKVersion_D3DRS = DxbxMapMostRecentToActiveVersion[rs];
		if (XDKVersion_D3DRS != X_D3DRS_UNSUPPORTED) {
			EmuMappedD3DRenderState[rs] = &(Xbox_D3D__RenderState[XDKVersion_D3DRS]);
			RegisterAddressLabel(EmuMappedD3DRenderState[rs], "D3D__RenderState[%d/*=%s*/]",
				XDKVersion_D3DRS,
				GetDxbxRenderStateInfo(rs).S + 2); // Skip "X_" prefix
			// TODO : Should we label "g_Device." members too?
		}
		else
			EmuMappedD3DRenderState[rs] = DummyRenderState;
	}

	// Initialize the dummy render state :
	EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED] = DummyRenderState;
}

const DWORD OLD_X_D3DTSS_COLOROP = 0;
const DWORD OLD_X_D3DTSS_TEXTURETRANSFORMFLAGS = 9;
const DWORD OLD_X_D3DTSS_ADDRESSU = 10;
const DWORD OLD_X_D3DTSS_ALPHAKILL = 21;

X_D3DTEXTURESTAGESTATETYPE XTL::DxbxFromOldVersion_D3DTSS(const X_D3DTEXTURESTAGESTATETYPE OldValue)
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

// ******************************************************************
// * patch: UpdateDeferredStates
// ******************************************************************
void XTL::EmuUpdateDeferredStates()
{
    using namespace XTL;

    // Certain D3DRS values need to be checked on each Draw[Indexed]Vertices
    if(Xbox_D3D__RenderState_Deferred != 0)
    {
        if(XTL::Xbox_D3D__RenderState_Deferred[0] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGENABLE, XTL::Xbox_D3D__RenderState_Deferred[0]);

        if(XTL::Xbox_D3D__RenderState_Deferred[1] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGTABLEMODE, XTL::Xbox_D3D__RenderState_Deferred[1]);

        if(XTL::Xbox_D3D__RenderState_Deferred[2] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGSTART, XTL::Xbox_D3D__RenderState_Deferred[2]);

        if(XTL::Xbox_D3D__RenderState_Deferred[3] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGEND, XTL::Xbox_D3D__RenderState_Deferred[3]);

        if(XTL::Xbox_D3D__RenderState_Deferred[4] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGDENSITY, XTL::Xbox_D3D__RenderState_Deferred[4]);

        if(XTL::Xbox_D3D__RenderState_Deferred[5] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_RANGEFOGENABLE, XTL::Xbox_D3D__RenderState_Deferred[5]);

        if(XTL::Xbox_D3D__RenderState_Deferred[6] != X_D3DRS_UNK)
        {
            ::DWORD dwConv = 0;

            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[6] & 0x00000010) ? D3DWRAP_U : 0;
            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[6] & 0x00001000) ? D3DWRAP_V : 0;
            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[6] & 0x00100000) ? D3DWRAP_W : 0;

            g_pD3DDevice8->SetRenderState(D3DRS_WRAP0, dwConv);
        }

        if(XTL::Xbox_D3D__RenderState_Deferred[7] != X_D3DRS_UNK)
        {
            ::DWORD dwConv = 0;

            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[7] & 0x00000010) ? D3DWRAP_U : 0;
            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[7] & 0x00001000) ? D3DWRAP_V : 0;
            dwConv |= (XTL::Xbox_D3D__RenderState_Deferred[7] & 0x00100000) ? D3DWRAP_W : 0;

            g_pD3DDevice8->SetRenderState(D3DRS_WRAP1, dwConv);
        }

        if(XTL::Xbox_D3D__RenderState_Deferred[10] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_LIGHTING, XTL::Xbox_D3D__RenderState_Deferred[10]);

        if(XTL::Xbox_D3D__RenderState_Deferred[11] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_SPECULARENABLE, XTL::Xbox_D3D__RenderState_Deferred[11]);

        if(XTL::Xbox_D3D__RenderState_Deferred[13] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_COLORVERTEX, XTL::Xbox_D3D__RenderState_Deferred[13]);

        if(XTL::Xbox_D3D__RenderState_Deferred[19] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, XTL::Xbox_D3D__RenderState_Deferred[19]);

        if(XTL::Xbox_D3D__RenderState_Deferred[20] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, XTL::Xbox_D3D__RenderState_Deferred[20]);

        if(XTL::Xbox_D3D__RenderState_Deferred[21] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, XTL::Xbox_D3D__RenderState_Deferred[21]);

        if(XTL::Xbox_D3D__RenderState_Deferred[23] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_AMBIENT, XTL::Xbox_D3D__RenderState_Deferred[23]);

        if(XTL::Xbox_D3D__RenderState_Deferred[24] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE, XTL::Xbox_D3D__RenderState_Deferred[24]);

        if(XTL::Xbox_D3D__RenderState_Deferred[25] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MIN, XTL::Xbox_D3D__RenderState_Deferred[25]);

        if(XTL::Xbox_D3D__RenderState_Deferred[26] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSPRITEENABLE, XTL::Xbox_D3D__RenderState_Deferred[26]);

        if(XTL::Xbox_D3D__RenderState_Deferred[27] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALEENABLE, XTL::Xbox_D3D__RenderState_Deferred[27]);

        if(XTL::Xbox_D3D__RenderState_Deferred[28] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_A, XTL::Xbox_D3D__RenderState_Deferred[28]);

        if(XTL::Xbox_D3D__RenderState_Deferred[29] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_B, XTL::Xbox_D3D__RenderState_Deferred[29]);

        if(XTL::Xbox_D3D__RenderState_Deferred[30] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_C, XTL::Xbox_D3D__RenderState_Deferred[30]);

        if(XTL::Xbox_D3D__RenderState_Deferred[31] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MAX, XTL::Xbox_D3D__RenderState_Deferred[31]);

        if(XTL::Xbox_D3D__RenderState_Deferred[33] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_PATCHSEGMENTS, XTL::Xbox_D3D__RenderState_Deferred[33]);

        /** To check for unhandled RenderStates
        for(int v=0;v<117-82;v++)
        {
            if(XTL::Xbox_D3D__RenderState_Deferred[v] != X_D3DRS_UNK)
            {
                if(v !=  0 && v !=  1 && v !=  2 && v !=  3 && v !=  4 && v !=  5 && v !=  6 && v !=  7
                && v != 10 && v != 11 && v != 13 && v != 19 && v != 20 && v != 21 && v != 23 && v != 24
                && v != 25 && v != 26 && v != 27 && v != 28 && v != 29 && v != 30 && v != 31 && v != 33)
                    EmuWarning("Unhandled RenderState Change @ %d (%d)", v, v + 82);
            }
        }
        //**/
    }

	// For 3925, the actual D3DTSS flags have different values.
	bool bHack3925 = (g_BuildVersion == 3925) ? true : false;
	int Adjust1 = bHack3925 ? 12 : 0;
	int Adjust2 = bHack3925 ? 10 : 0;

    // Certain D3DTS values need to be checked on each Draw[Indexed]Vertices
    if(Xbox_D3D_TextureState != 0)
    {
        for(int v=0;v<4;v++)
        {
            ::DWORD *pCur = &Xbox_D3D_TextureState[v*32];

            if(pCur[0+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[0+Adjust2] == 5)
					EmuWarning("ClampToEdge is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ADDRESSU, pCur[0+Adjust2]);
            }

            if(pCur[1+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[1+Adjust2] == 5)
					EmuWarning("ClampToEdge is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ADDRESSV, pCur[1+Adjust2]);
            }

            if(pCur[2+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[2+Adjust2] == 5)
					EmuWarning("ClampToEdge is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ADDRESSW, pCur[2+Adjust2]);
            }

            if(pCur[3+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[3+Adjust2] == 4)
                    EmuWarning("QuinCunx is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MAGFILTER, pCur[3+Adjust2]);
            }

            if(pCur[4+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[4+Adjust2] == 4)
					EmuWarning("QuinCunx is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MINFILTER, pCur[4+Adjust2]);
            }

            if(pCur[5+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[5+Adjust2] == 4)
					EmuWarning("QuinCunx is unsupported (temporarily)");
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MIPFILTER, pCur[5+Adjust2]);
            }

            if(pCur[6+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MIPMAPLODBIAS, pCur[6+Adjust2]);

            if(pCur[7+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MAXMIPLEVEL, pCur[7+Adjust2]);

            if(pCur[8+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_MAXANISOTROPY, pCur[8+Adjust2]);

            if(pCur[12-Adjust1] != X_D3DTSS_UNK)
            {
				// TODO: This would be better split into it's own function, or a lookup array
				switch (pCur[12 - Adjust1]) 
				{
				case X_D3DTOP_DISABLE: 
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_DISABLE);
					break;
				case X_D3DTOP_SELECTARG1:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
					break;
				case X_D3DTOP_SELECTARG2:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
					break;
				case X_D3DTOP_MODULATE:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATE);
					break;
				case X_D3DTOP_MODULATE2X:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
					break;
				case X_D3DTOP_MODULATE4X:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATE4X);
					break;
				case X_D3DTOP_ADD:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_ADD);
					break;
				case X_D3DTOP_ADDSIGNED:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_ADDSIGNED);
					break;
				case X_D3DTOP_ADDSIGNED2X:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_ADDSIGNED2X);
					break;
				case X_D3DTOP_SUBTRACT:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_SUBTRACT);
					break;
				case X_D3DTOP_ADDSMOOTH:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_ADDSMOOTH);
					break;
				case X_D3DTOP_BLENDDIFFUSEALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BLENDDIFFUSEALPHA);
					break;
				case X_D3DTOP_BLENDCURRENTALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA);
					break;
				case X_D3DTOP_BLENDTEXTUREALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
					break;
				case X_D3DTOP_BLENDFACTORALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BLENDFACTORALPHA);
					break;
				case X_D3DTOP_BLENDTEXTUREALPHAPM:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHAPM);
					break;
				case X_D3DTOP_PREMODULATE:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_PREMODULATE);
					break;
				case X_D3DTOP_MODULATEALPHA_ADDCOLOR:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR);
					break;
				case X_D3DTOP_MODULATECOLOR_ADDALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATECOLOR_ADDALPHA);
					break;
				case X_D3DTOP_MODULATEINVALPHA_ADDCOLOR:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATEINVALPHA_ADDCOLOR);
					break;
				case X_D3DTOP_MODULATEINVCOLOR_ADDALPHA:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MODULATEINVCOLOR_ADDALPHA);
					break;
				case X_D3DTOP_DOTPRODUCT3:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
					break;
				case X_D3DTOP_MULTIPLYADD:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
					break;
				case X_D3DTOP_LERP:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_LERP);
					break;
				case X_D3DTOP_BUMPENVMAP:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
					break;
				case X_D3DTOP_BUMPENVMAPLUMINANCE:
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLOROP, D3DTOP_BUMPENVMAPLUMINANCE);
					break;
				default:
					EmuWarning("(Temporarily) Unsupported D3DTSS_COLOROP Value (%d)", pCur[12 - Adjust1]);
					break;
				}
            }

            if(pCur[13-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLORARG0, pCur[13-Adjust1]);

            if(pCur[14-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLORARG1, pCur[14-Adjust1]);

            if(pCur[15-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_COLORARG2, pCur[15-Adjust1]);

            // TODO: Use a lookup table, this is not always a 1:1 map (same as D3DTSS_COLOROP)
            if(pCur[16-Adjust1] != X_D3DTSS_UNK)
            {
                if(pCur[16-Adjust1] > 12 && pCur[16-Adjust1] != 14 && pCur[16-Adjust1] != 13)
                    EmuWarning("(Temporarily) Unsupported D3DTSS_ALPHAOP Value (%d)", pCur[16-Adjust1]);
				else
				if( pCur[16-Adjust1] == 14 )
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP, D3DTOP_BLENDTEXTUREALPHA);
				if( pCur[16-Adjust1] == 15 )
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP, D3DTOP_BLENDFACTORALPHA);
				if( pCur[16-Adjust1] == 13 )
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP, D3DTOP_BLENDCURRENTALPHA);
				else
					g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAOP, pCur[16-Adjust1]);
            }

            if(pCur[17-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAARG0, pCur[17-Adjust1]);

            if(pCur[18-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAARG1, pCur[18-Adjust1]);

            if(pCur[19-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_ALPHAARG2, pCur[19-Adjust1]);

            if(pCur[20-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_RESULTARG, pCur[20-Adjust1]);

            if(pCur[21-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_TEXTURETRANSFORMFLAGS, pCur[21-Adjust1]);

            /*if(pCur[29] != X_D3DTSS_UNK)	// This is NOT a deferred texture state!
                g_pD3DDevice8->SetTextureStageState(v, D3DTSS_BORDERCOLOR, pCur[29]);*/

            /** To check for unhandled texture stage state changes
            for(int r=0;r<32;r++)
            {
                static const int unchecked[] =
                {
                    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 29, 30, 31
                };

                if(pCur[r] != X_D3DTSS_UNK)
                {
                    bool pass = true;

                    for(int q=0;q<sizeof(unchecked)/sizeof(int);q++)
                    {
                        if(r == unchecked[q])
                        {
                            pass = false;
                            break;
                        }
                    }

                    if(pass)
                        EmuWarning("Unhandled TextureState Change @ %d->%d", v, r);
                }
            }
            //**/
        }

        // if point sprites are enabled, copy stage 3 over to 0
        if(Xbox_D3D__RenderState_Deferred[26] == TRUE)
        {
            // pCur = Texture Stage 3 States
            ::DWORD *pCur = &Xbox_D3D_TextureState[2*32];

            IDirect3DBaseTexture8 *pTexture;

            // set the point sprites texture
            g_pD3DDevice8->GetTexture(3, &pTexture);
            g_pD3DDevice8->SetTexture(0, pTexture);

            // disable all other stages
            g_pD3DDevice8->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            g_pD3DDevice8->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

            // in that case we have to copy over the stage by hand
            for(int v=0;v<30;v++)
            {
                if(pCur[v] != X_D3DTSS_UNK)
                {
                    ::DWORD dwValue;

                    g_pD3DDevice8->GetTextureStageState(3, (D3DTEXTURESTAGESTATETYPE)v, &dwValue);
                    g_pD3DDevice8->SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)v, dwValue);
                }
            }
        }
    }
}

void CxbxInitializeTextureStageStates()
{
	for (int Stage = 0; Stage < X_D3DTSS_STAGECOUNT; Stage++) {
		for (X_D3DTEXTURESTAGESTATETYPE State = X_D3DTSS_FIRST; State <= X_D3DTSS_LAST; State++) {
			DWORD NewVersion_TSS = DxbxFromOldVersion_D3DTSS(State); // Map old to new
			void *Addr = &(Xbox_D3D_TextureState[(Stage * X_D3DTSS_STAGESIZE) + State]);
#if 0 // See https://github.com/PatrickvL/Cxbx-Reloaded/tree/WIP_LessVertexPatching
			RegisterAddressLabel(Addr, "D3D__TextureState[/*Stage*/%d][%d/*=%s*/]",
				Stage, State,
				DxbxTextureStageStateInfo[NewVersion_TSS].S + 2); // Skip "X_" prefix
#else
			RegisterAddressLabel(Addr, "D3D__TextureState[/*Stage*/%d][%d]",
				Stage, State);
#endif
		}
	}
}

void InitD3DDeferredStates()
{
	CxbxInitializeTextureStageStates();

	CxbxInitializeEmuMappedD3DRenderState();
}

} // end of namespace XTL
