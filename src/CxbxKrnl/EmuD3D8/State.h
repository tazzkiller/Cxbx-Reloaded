// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   CxbxKrnl->EmuD3D8->State.h
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
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef STATE_H
#define STATE_H

// Xbox version
#define X_D3DRS_UNSUPPORTED (X_D3DRS_LAST + 1)

// Host version
#define D3DRS_UNSUPPORTED (D3DRENDERSTATETYPE)0

// XDK version independent renderstate table, containing pointers to the original locations.
extern X_D3DRENDERSTATETYPE *EmuMappedD3DRenderState[X_D3DRS_UNSUPPORTED + 1]; // 1 extra for the unsupported value itself

inline DWORD CxbxGetRenderState(XTL::X_D3DRENDERSTATETYPE XboxRenderState)
{
	return *(XTL::EmuMappedD3DRenderState[XboxRenderState]);
}

extern DWORD DxbxMapMostRecentToActiveVersion[X_D3DRS_LAST + 1];

struct X_Stream
{
	DWORD Stride;
	DWORD Offset;
	XTL::X_D3DVertexBuffer *pVertexBuffer;
};

extern DWORD *Xbox_D3D__Device; // The Xbox1 D3D__Device

extern X_Stream *Xbox_g_Stream; // The Xbox1 g_Stream[16] array

extern DWORD *Xbox_D3D__RenderState_Deferred;

extern DWORD *Xbox_D3D_TextureState; // [X_D3DTSS_STAGECOUNT][X_D3DTSS_STAGESIZE] = [(Stage * X_D3DTSS_STAGESIZE) + Offset]

extern void CxbxPitchedCopy(BYTE *pDest, BYTE *pSrc, DWORD dwDestPitch, DWORD dwSrcPitch, DWORD dwWidthInBytes, DWORD dwHeight);

extern void DxbxBuildRenderStateMappingTable();

extern void InitD3DDeferredStates();

extern void DxbxUpdateDeferredStates();

extern X_D3DTEXTURESTAGESTATETYPE DxbxFromNewVersion_D3DTSS(const X_D3DTEXTURESTAGESTATETYPE NewValue);

extern DWORD Dxbx_SetRenderState(const X_D3DRENDERSTATETYPE XboxRenderState, DWORD XboxValue);

extern DWORD Cxbx_SetTextureStageState(DWORD Sampler, X_D3DTEXTURESTAGESTATETYPE Type, DWORD XboxValue);

#endif