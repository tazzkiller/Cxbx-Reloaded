// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   CxbxKrnl->EmuD3D8->VertexShader.h
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
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef VERTEXSHADER_H
#define VERTEXSHADER_H

#include "Cxbx.h"

// nv2a microcode header
typedef struct
{
    uint08 Type;
    uint08 Version;
    uint08 NumInst;
    uint08 Unknown0;
}
VSH_SHADER_HEADER;

#define VSH_INSTRUCTION_SIZE       4
#define VSH_INSTRUCTION_SIZE_BYTES (VSH_INSTRUCTION_SIZE * sizeof(DWORD))

// recompile xbox vertex shader declaration
extern DWORD EmuRecompileVshDeclaration
(
    DWORD                *pDeclaration,
    DWORD               **ppRecompiledDeclaration,
    DWORD                *pDeclarationSize,
    boolean               IsFixedFunction,
    CxbxVertexShaderDynamicPatch *pVertexDynamicPatch
);

// recompile xbox vertex shader function
extern HRESULT EmuRecompileVshFunction
(
    DWORD        *pFunction,
    LPD3DXBUFFER *ppRecompiled,
    DWORD        *pOriginalSize,
    boolean      bNoReservedConstants,
	boolean		 *pbUseDeclarationOnly
);

extern void FreeVertexDynamicPatch(CxbxVertexShader *pHostVertexShader);

// Checks for failed vertex shaders, and shaders that would need patching
extern bool IsValidCurrentShader(void);
extern bool VshHandleIsValidShader(DWORD Handle);

// On Xbox, a vertex shader handle is either a FVF (Fixed Vertex Format),
// or a shader object address (bit 1 set indicates non-FVF shader handles).
// FVF combine D3DFVF_* flags, and use bit 16 up to 23 for texture sizes.
inline bool VshHandleIsFVF(DWORD Handle) { return ((Handle & D3DFVF_RESERVED0) == 0); }
inline bool VshHandleIsVertexShader(DWORD Handle) { return ((Handle & D3DFVF_RESERVED0) > 0); }

extern CxbxVertexShader *GetHostVertexShader(X_D3DVertexShader *pXboxVertexShader);
extern CxbxVertexShader *VshHandleGetHostVertexShader(DWORD aHandle);
extern X_D3DVertexShader *VshHandleGetXboxVertexShader(DWORD Handle);


#ifdef _DEBUG_TRACK_VS
#define DbgVshPrintf if(g_bPrintfOn) printf
#else
inline void null_func_vsh(...) { }
#define DbgVshPrintf XTL::null_func_vsh
#endif

#endif
