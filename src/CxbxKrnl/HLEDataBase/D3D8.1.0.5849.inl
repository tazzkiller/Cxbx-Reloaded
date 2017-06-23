// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->D3D8.1.0.5849.cpp
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

// ******************************************************************
// * D3DDevice_SetRenderState_StencilEnable
// ******************************************************************
OOVPA_NO_XREF(D3DDevice_SetRenderState_StencilEnable, 5849, 8)

        { 0x12, 0x8B },
        { 0x24, 0x33 },
        { 0x37, 0x74 },
        { 0x4A, 0x1E },
        { 0x5D, 0x74 },
        { 0x70, 0xB9 },
        { 0x83, 0x40 },
        { 0x96, 0x04 },
OOVPA_END;

// ******************************************************************
// * D3D::SetFence
// * SOURCE: Spiderman 2
// ******************************************************************
OOVPA_XREF(D3D_SetFence, 5849, 7, XREF_D3D_SETFENCE, XRefZero)
	{ 0x0E, 0x05 },
	{ 0x17, 0xC7 },
	{ 0x3E, 0x40 },
	{ 0x5E, 0x00 },
	{ 0x87, 0x4E },
	{ 0x98, 0x83 },
	{ 0xA8, 0x75 },
OOVPA_END;

// ******************************************************************
// * D3D::BlockOnTime
// * Source: Spiderman 2
// ******************************************************************
OOVPA_XREF(D3D_BlockOnTime, 5849, 6, XREF_D3D_BLOCKONTIME, XRefZero)
	{ 0x09, 0x30 },
	{ 0x27, 0x07 },
	{ 0x7E, 0x2B },
	{ 0xA5, 0x20 },
	{ 0xD9, 0x56 },
	{ 0xF8, 0x47 },
OOVPA_END;

// ******************************************************************
// * Get2DSurfaceDesc
// ******************************************************************
// * NOTE: D3DTexture_GetLevelDesc and D3DSurface_GetDesc redirect here
// ******************************************************************
OOVPA_NO_XREF(Get2DSurfaceDesc, 5849, 10)

        // Get2DSurfaceDesc+0x2B : movzx edx, byte ptr [edi+0x0D]
        { 0x2B, 0x0F }, // (Offset,Value)-Pair #1
        { 0x2C, 0xB6 }, // (Offset,Value)-Pair #2
        { 0x2D, 0x57 }, // (Offset,Value)-Pair #3
        { 0x2E, 0x0D }, // (Offset,Value)-Pair #4

        // Get2DSurfaceDesc+0x52 : mov edx, [eax+0x1A14]
        { 0x52, 0x8B }, // (Offset,Value)-Pair #5
        { 0x53, 0x90 }, // (Offset,Value)-Pair #6
        { 0x54, 0x14 }, // (Offset,Value)-Pair #7
        { 0x55, 0x1A }, // (Offset,Value)-Pair #8

        // Get2DSurfaceDesc+0xAE : retn 0x0C
        { 0xAE, 0xC2 }, // (Offset,Value)-Pair #9
        { 0xAF, 0x0C }, // (Offset,Value)-Pair #10
OOVPA_END;

// ******************************************************************
// * D3DDevice_SetScreenSpaceOffset
// ******************************************************************
OOVPA_NO_XREF(D3DDevice_SetScreenSpaceOffset, 5849, 8)
	// D3DDevice_SetScreenSpaceOffset+0x13 : fstp [esi+0x0EF8]
	{ 0x13, 0xD9 }, // (Offset,Value)-Pair #1
	{ 0x14, 0x9E }, // (Offset,Value)-Pair #2
	{ 0x15, 0xF8 }, // (Offset,Value)-Pair #3
	{ 0x16, 0x0E }, // (Offset,Value)-Pair #4

	// D3DDevice_SetScreenSpaceOffset+0x33 : jb +0x05
	{ 0x33, 0x72 }, // (Offset,Value)-Pair #5
	{ 0x34, 0x05 }, // (Offset,Value)-Pair #6

	// D3DDevice_SetScreenSpaceOffset+0x46 : retn 0x08
	{ 0x46, 0xC2 }, // (Offset,Value)-Pair #7
	{ 0x47, 0x08 }, // (Offset,Value)-Pair #8
OOVPA_END;


// ******************************************************************
// * D3DDevice8::SetDepthClipPlanes
// ******************************************************************
OOVPA_NO_XREF(D3DDevice_SetDepthClipPlanes, 5849, 11)

        // _D3DDevice_SetDepthClipPlanes+0x00 : mov eax, [esp+Flags]
        { 0x00, 0x8B }, // (Offset,Value)-Pair #1
        { 0x01, 0x44 }, // (Offset,Value)-Pair #2
        { 0x02, 0x24 }, // (Offset,Value)-Pair #3
        { 0x03, 0x0C }, // (Offset,Value)-Pair #4

        // _D3DDevice_SetDepthClipPlanes+0x0F : ja short loc_27ABD0 ; jumptable 0027AB71 default case
        { 0x0F, 0x77 }, // (Offset,Value)-Pair #5
        { 0x10, 0x5F }, // (Offset,Value)-Pair #6

        // _D3DDevice_SetDepthClipPlanes+0x11 : jmp ds:off_27ABEC[eax*4] ; switch jump
        { 0x11, 0xFF }, // (Offset,Value)-Pair #7
        { 0x12, 0x24 }, // (Offset,Value)-Pair #8
        { 0x13, 0x85 }, // (Offset,Value)-Pair #9

        // _D3DDevice_SetDepthClipPlanes+0x88 : retn 0Ch
        { 0x88, 0xC2 }, // (Offset,Value)-Pair #10
        { 0x89, 0x0C }, // (Offset,Value)-Pair #11
OOVPA_END;

// ******************************************************************
// * D3DDevice_SetScreenSpaceOffset
// ******************************************************************
#define D3DDevice_SetScreenSpaceOffset_5849 D3DDevice_SetScreenSpaceOffset_5558

// ******************************************************************
// * D3D::SetFence
// ******************************************************************
#define D3D_SetFence_5849 D3D_SetFence_5558

// ******************************************************************
// * D3DDevice_GetViewportOffsetAndScale
// ******************************************************************
#define D3DDevice_GetViewportOffsetAndScale_5849 D3DDevice_GetViewportOffsetAndScale_5558

// ******************************************************************
// * D3DDevice_CreateStateBlock
// ******************************************************************
#define D3DDevice_CreateStateBlock_5849 D3DDevice_CreateStateBlock_4627
