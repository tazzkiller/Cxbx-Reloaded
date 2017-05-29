// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  0x0x0x              Y0x0x0xP     0x0x""""Y0x0x     Y0x0x0xP
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
#include "Convert.h" // DxbxRenderStateInfo

extern uint32 g_BuildVersion;
extern int g_iWireframe;
extern int X_D3DSCM_CORRECTION_VersionDependent;

namespace XTL {

// Dxbx addition : Dummy value (and pointer to that) to transparently ignore unsupported render states :
X_D3DRENDERSTATETYPE DummyRenderStateValue = X_D3DRS_FIRST;
X_D3DRENDERSTATETYPE *DummyRenderState = &DummyRenderStateValue; // Unsupported states share this pointer value

// XDK version independent renderstate table, containing pointers to the original locations.
X_D3DRENDERSTATETYPE *EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED] = { NULL }; // 1 extra for the unsupported value

// Deferred state lookup tables
DWORD *EmuD3DDeferredRenderState = NULL;
// Texture state lookup table (same size in all XDK versions, so defined as a fixed size array) :
DWORD *EmuD3DDeferredTextureState = NULL; // [X_D3DTSS_STAGECOUNT][X_D3DTSS_STAGESIZE] = [(Stage * X_D3DTSS_STAGESIZE) + Offset]

// TODO : Set these after symbols are scanned/loaded :
DWORD *_D3D__Device = NULL; // The Xbox1 D3D__Device
DWORD *_D3D__RenderState = NULL;

DWORD (*DxbxTextureStageStateXB2PCCallback[X_D3DTSS_LAST])(DWORD Value);
DWORD (*DxbxRenderStateXB2PCCallback[X_D3DRS_LAST])(DWORD Value);
X_D3DRENDERSTATETYPE DxbxMapActiveVersionToMostRecent[X_D3DRS_LAST];
X_D3DRENDERSTATETYPE DxbxMapMostRecentToActiveVersion[X_D3DRS_LAST];

void DxbxBuildRenderStateMappingTable()
{
	if (_D3D__RenderState == nullptr)
		return;

	if (g_BuildVersion <= 4361)
		X_D3DSCM_CORRECTION_VersionDependent = X_D3DSCM_CORRECTION;
	else
		X_D3DSCM_CORRECTION_VersionDependent = 0;

	// Loop over all latest (5911) states :
	X_D3DRENDERSTATETYPE State_VersionDependent = X_D3DRS_FIRST;
	for (X_D3DRENDERSTATETYPE State = X_D3DRS_FIRST; State < X_D3DRS_LAST; State++)
	{
		// Check if this state is available in the active SDK version :
		if (g_BuildVersion >= DxbxRenderStateInfo[State].V)
		{
			// If it is available, register this offset in the various mapping tables we use :
			DxbxMapActiveVersionToMostRecent[State_VersionDependent] = State;
			DxbxMapMostRecentToActiveVersion[State] = State_VersionDependent;
			EmuMappedD3DRenderState[State] = &(_D3D__RenderState[State_VersionDependent]);
			// Step to the next offset :
			State_VersionDependent++;
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

	// Initialize the dummy render state :
	EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED] = DummyRenderState;

	// Log the start address of the "deferred" render states (not needed anymore, just to keep logging the same) :
	{
		// Calculate the location of D3DDeferredRenderState via an XDK-dependent offset to _D3D__RenderState :
		EmuD3DDeferredRenderState = _D3D__RenderState;
		// Dxbx note : XTL_EmuD3DDeferredRenderState:PDWORDs cast to UIntPtr to avoid incrementing with that many array-sizes!
		EmuD3DDeferredRenderState += DxbxMapMostRecentToActiveVersion[X_D3DRS_DEFERRED_FIRST];
		DbgPrintf("HLE: 0x%.08X -> EmuD3DDeferredRenderState", EmuD3DDeferredRenderState);
	}

	// Build a table with converter functions for all renderstates :
	for (int i = X_D3DRS_FIRST; i < X_D3DRS_LAST; i++)
		DxbxRenderStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[DxbxRenderStateInfo[i].T].F);

	// Build a table with converter functions for all texture stage states :
	for (int i = X_D3DTSS_FIRST; i < X_D3DTSS_LAST; i++)
		DxbxTextureStageStateXB2PCCallback[i] = (TXB2PCFunc)(DxbxXBTypeInfo[DxbxTextureStageStateInfo[i].T].F);
}

// Converts the input render state from a version-dependent into a version-neutral value.
X_D3DRENDERSTATETYPE DxbxVersionAdjust_D3DRS(const X_D3DRENDERSTATETYPE XboxRenderState_VersionDependent)
{
	return DxbxMapActiveVersionToMostRecent[XboxRenderState_VersionDependent];
}

void InitD3DDeferredStates()
{
	DxbxBuildRenderStateMappingTable();

	for (int v = 0; v < 44; v++) {
		EmuD3DDeferredRenderState[v] = X_D3DRS_UNK;
	}

	for (int s = 0; s < X_D3DTSS_STAGECOUNT; s++) {
		for (int v = 0; v < X_D3DTSS_STAGESIZE; v++)
			EmuD3DDeferredTextureState[(s * X_D3DTSS_STAGESIZE) + v] = X_D3DTSS_UNK;
	}
}

}; // end of namespace XTL

// ******************************************************************
// * patch: UpdateDeferredStates
// ******************************************************************
void XTL::EmuUpdateDeferredStates()
{
    using namespace XTL;

    // Certain D3DRS values need to be checked on each Draw[Indexed]Vertices
    if(EmuD3DDeferredRenderState != 0)
    {
        if(XTL::EmuD3DDeferredRenderState[0] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGENABLE, XTL::EmuD3DDeferredRenderState[0]);

        if(XTL::EmuD3DDeferredRenderState[1] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGTABLEMODE, XTL::EmuD3DDeferredRenderState[1]);

        if(XTL::EmuD3DDeferredRenderState[2] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGSTART, XTL::EmuD3DDeferredRenderState[2]);

        if(XTL::EmuD3DDeferredRenderState[3] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGEND, XTL::EmuD3DDeferredRenderState[3]);

        if(XTL::EmuD3DDeferredRenderState[4] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_FOGDENSITY, XTL::EmuD3DDeferredRenderState[4]);

        if(XTL::EmuD3DDeferredRenderState[5] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_RANGEFOGENABLE, XTL::EmuD3DDeferredRenderState[5]);

        if(XTL::EmuD3DDeferredRenderState[6] != X_D3DRS_UNK)
        {
            ::DWORD dwConv = 0;

            dwConv |= (XTL::EmuD3DDeferredRenderState[6] & 0x00000010) ? D3DWRAP_U : 0;
            dwConv |= (XTL::EmuD3DDeferredRenderState[6] & 0x00001000) ? D3DWRAP_V : 0;
            dwConv |= (XTL::EmuD3DDeferredRenderState[6] & 0x00100000) ? D3DWRAP_W : 0;

            g_pD3DDevice8->SetRenderState(D3DRS_WRAP0, dwConv);
        }

        if(XTL::EmuD3DDeferredRenderState[7] != X_D3DRS_UNK)
        {
            ::DWORD dwConv = 0;

            dwConv |= (XTL::EmuD3DDeferredRenderState[7] & 0x00000010) ? D3DWRAP_U : 0;
            dwConv |= (XTL::EmuD3DDeferredRenderState[7] & 0x00001000) ? D3DWRAP_V : 0;
            dwConv |= (XTL::EmuD3DDeferredRenderState[7] & 0x00100000) ? D3DWRAP_W : 0;

            g_pD3DDevice8->SetRenderState(D3DRS_WRAP1, dwConv);
        }

        if(XTL::EmuD3DDeferredRenderState[10] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_LIGHTING, XTL::EmuD3DDeferredRenderState[10]);

        if(XTL::EmuD3DDeferredRenderState[11] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_SPECULARENABLE, XTL::EmuD3DDeferredRenderState[11]);

        if(XTL::EmuD3DDeferredRenderState[13] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_COLORVERTEX, XTL::EmuD3DDeferredRenderState[13]);

        if(XTL::EmuD3DDeferredRenderState[19] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, XTL::EmuD3DDeferredRenderState[19]);

        if(XTL::EmuD3DDeferredRenderState[20] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, XTL::EmuD3DDeferredRenderState[20]);

        if(XTL::EmuD3DDeferredRenderState[21] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, XTL::EmuD3DDeferredRenderState[21]);

        if(XTL::EmuD3DDeferredRenderState[23] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_AMBIENT, XTL::EmuD3DDeferredRenderState[23]);

        if(XTL::EmuD3DDeferredRenderState[24] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE, XTL::EmuD3DDeferredRenderState[24]);

        if(XTL::EmuD3DDeferredRenderState[25] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MIN, XTL::EmuD3DDeferredRenderState[25]);

        if(XTL::EmuD3DDeferredRenderState[26] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSPRITEENABLE, XTL::EmuD3DDeferredRenderState[26]);

        if(XTL::EmuD3DDeferredRenderState[27] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALEENABLE, XTL::EmuD3DDeferredRenderState[27]);

        if(XTL::EmuD3DDeferredRenderState[28] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_A, XTL::EmuD3DDeferredRenderState[28]);

        if(XTL::EmuD3DDeferredRenderState[29] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_B, XTL::EmuD3DDeferredRenderState[29]);

        if(XTL::EmuD3DDeferredRenderState[30] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSCALE_C, XTL::EmuD3DDeferredRenderState[30]);

        if(XTL::EmuD3DDeferredRenderState[31] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_POINTSIZE_MAX, XTL::EmuD3DDeferredRenderState[31]);

        if(XTL::EmuD3DDeferredRenderState[33] != X_D3DRS_UNK)
            g_pD3DDevice8->SetRenderState(D3DRS_PATCHSEGMENTS, XTL::EmuD3DDeferredRenderState[33]);

        /** To check for unhandled RenderStates
        for(int v=0;v<117-82;v++)
        {
            if(XTL::EmuD3DDeferredRenderState[v] != X_D3DRS_UNK)
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
    if(EmuD3DDeferredTextureState != NULL)
    {
        for(int s=0;s<X_D3DTSS_STAGECOUNT;s++)
        {
            ::DWORD *pCur = &EmuD3DDeferredTextureState[s * X_D3DTSS_STAGESIZE];

            if(pCur[0+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[0+Adjust2] == 5)
                    CxbxKrnlCleanup("ClampToEdge is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ADDRESSU, pCur[0+Adjust2]);
            }

            if(pCur[1+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[1+Adjust2] == 5)
                    CxbxKrnlCleanup("ClampToEdge is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ADDRESSV, pCur[1+Adjust2]);
            }

            if(pCur[2+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[2+Adjust2] == 5)
                    CxbxKrnlCleanup("ClampToEdge is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ADDRESSW, pCur[2+Adjust2]);
            }

            if(pCur[3+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[3+Adjust2] == 4)
                    CxbxKrnlCleanup("QuinCunx is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MAGFILTER, pCur[3+Adjust2]);
            }

            if(pCur[4+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[4+Adjust2] == 4)
                    CxbxKrnlCleanup("QuinCunx is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MINFILTER, pCur[4+Adjust2]);
            }

            if(pCur[5+Adjust2] != X_D3DTSS_UNK)
            {
                if(pCur[5+Adjust2] == 4)
                    CxbxKrnlCleanup("QuinCunx is unsupported (temporarily)");

                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MIPFILTER, pCur[5+Adjust2]);
            }

            if(pCur[6+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MIPMAPLODBIAS, pCur[6+Adjust2]);

            if(pCur[7+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MAXMIPLEVEL, pCur[7+Adjust2]);

            if(pCur[8+Adjust2] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_MAXANISOTROPY, pCur[8+Adjust2]);

            if(pCur[12-Adjust1] != X_D3DTSS_UNK)
            {
				// TODO: This would be better split into it's own function, or a lookup array
				switch (pCur[12 - Adjust1]) 
				{
				case X_D3DTOP_DISABLE: 
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_DISABLE);
					break;
				case X_D3DTOP_SELECTARG1:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
					break;
				case X_D3DTOP_SELECTARG2:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
					break;
				case X_D3DTOP_MODULATE:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATE);
					break;
				case X_D3DTOP_MODULATE2X:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
					break;
				case X_D3DTOP_MODULATE4X:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATE4X);
					break;
				case X_D3DTOP_ADD:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_ADD);
					break;
				case X_D3DTOP_ADDSIGNED:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_ADDSIGNED);
					break;
				case X_D3DTOP_ADDSIGNED2X:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_ADDSIGNED2X);
					break;
				case X_D3DTOP_SUBTRACT:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_SUBTRACT);
					break;
				case X_D3DTOP_ADDSMOOTH:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_ADDSMOOTH);
					break;
				case X_D3DTOP_BLENDDIFFUSEALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BLENDDIFFUSEALPHA);
					break;
				case X_D3DTOP_BLENDCURRENTALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA);
					break;
				case X_D3DTOP_BLENDTEXTUREALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
					break;
				case X_D3DTOP_BLENDFACTORALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BLENDFACTORALPHA);
					break;
				case X_D3DTOP_BLENDTEXTUREALPHAPM:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHAPM);
					break;
				case X_D3DTOP_PREMODULATE:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_PREMODULATE);
					break;
				case X_D3DTOP_MODULATEALPHA_ADDCOLOR:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR);
					break;
				case X_D3DTOP_MODULATECOLOR_ADDALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATECOLOR_ADDALPHA);
					break;
				case X_D3DTOP_MODULATEINVALPHA_ADDCOLOR:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATEINVALPHA_ADDCOLOR);
					break;
				case X_D3DTOP_MODULATEINVCOLOR_ADDALPHA:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MODULATEINVCOLOR_ADDALPHA);
					break;
				case X_D3DTOP_DOTPRODUCT3:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
					break;
				case X_D3DTOP_MULTIPLYADD:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
					break;
				case X_D3DTOP_LERP:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_LERP);
					break;
				case X_D3DTOP_BUMPENVMAP:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
					break;
				case X_D3DTOP_BUMPENVMAPLUMINANCE:
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLOROP, D3DTOP_BUMPENVMAPLUMINANCE);
					break;
				default:
					CxbxKrnlCleanup("(Temporarily) Unsupported D3DTSS_COLOROP Value (%d)", pCur[12 - Adjust1]);
					break;
				}
            }

            if(pCur[13-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLORARG0, pCur[13-Adjust1]);

            if(pCur[14-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLORARG1, pCur[14-Adjust1]);

            if(pCur[15-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_COLORARG2, pCur[15-Adjust1]);

            // TODO: Use a lookup table, this is not always a 1:1 map (same as D3DTSS_COLOROP)
            if(pCur[16-Adjust1] != X_D3DTSS_UNK)
            {
                if(pCur[16-Adjust1] > 12 && pCur[16-Adjust1] != 14 && pCur[16-Adjust1] != 13)
                    CxbxKrnlCleanup("(Temporarily) Unsupported D3DTSS_ALPHAOP Value (%d)", pCur[16-Adjust1]);

				if( pCur[16-Adjust1] == 14 )
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAOP, D3DTOP_BLENDTEXTUREALPHA);
				if( pCur[16-Adjust1] == 15 )
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAOP, D3DTOP_BLENDFACTORALPHA);
				if( pCur[16-Adjust1] == 13 )
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAOP, D3DTOP_BLENDCURRENTALPHA);
				else
					g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAOP, pCur[16-Adjust1]);
            }

            if(pCur[17-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAARG0, pCur[17-Adjust1]);

            if(pCur[18-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAARG1, pCur[18-Adjust1]);

            if(pCur[19-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_ALPHAARG2, pCur[19-Adjust1]);

            if(pCur[20-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_RESULTARG, pCur[20-Adjust1]);

            if(pCur[21-Adjust1] != X_D3DTSS_UNK)
                g_pD3DDevice8->SetTextureStageState(s, D3DTSS_TEXTURETRANSFORMFLAGS, pCur[21-Adjust1]); // TODO : Handle all, not just D3DTTFF_COUNT2

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
        if(*EmuMappedD3DRenderState[X_D3DRS_POINTSPRITEENABLE] == TRUE)
        {
            // pCur = Texture Stage 3 States
            ::DWORD *pCur = &EmuD3DDeferredTextureState[2 * X_D3DTSS_STAGESIZE];

            IDirect3DBaseTexture8 *pTexture;

            // set the point sprites texture
            g_pD3DDevice8->GetTexture(3, &pTexture);
            g_pD3DDevice8->SetTexture(0, pTexture);

			// TODO -oDXBX: Should we clear the pPCTexture interface (and how)?

            // disable all other stages
            g_pD3DDevice8->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            g_pD3DDevice8->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

            // in that case we have to copy over the stage by hand
            for(int v=0; v<X_D3DTSS_STAGESIZE; v++)
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
}

DWORD XTL::Dxbx_SetRenderState(const X_D3DRENDERSTATETYPE XboxRenderState, DWORD XboxValue)
{
	D3DRENDERSTATETYPE PCRenderState;
	DWORD PCValue;

	LOG_INIT //

	// Check if the render state is mapped :
	if (EmuMappedD3DRenderState[XboxRenderState] == DummyRenderState)
	{
		CxbxKrnlCleanup("Unsupported RenderState : %s (0x%.08X)", DxbxRenderStateInfo[XboxRenderState].S, (int)XboxRenderState);
		return XboxValue;
	}

	// Set this value into the RenderState structure too (so other code will read the new current value) :
	*(EmuMappedD3DRenderState[XboxRenderState]) = XboxValue;

	// Skip Xbox extensions :
	if (DxbxRenderStateInfo[XboxRenderState].PC == D3DRS_UNSUPPORTED)
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
		if (XboxValue == X_D3DTSS_UNK) // TODO : These are no texture stage states, so X_D3DTSS_UNK is incorrect. Use D3DRS_UNSUPPORTED perhaps?
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
	PCRenderState = DxbxRenderStateInfo[XboxRenderState].PC;
	if (PCRenderState == D3DRS_UNSUPPORTED)
	{
		EmuWarning("%s is not supported!", DxbxRenderStateInfo[XboxRenderState].S);
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
