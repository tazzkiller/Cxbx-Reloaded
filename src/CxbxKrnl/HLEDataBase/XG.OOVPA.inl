// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->XG.OOVPA.inl
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

#ifndef XG_OOVPA_INL
#define XG_OOVPA_INL

#include "OOVPA.h"

#include "HLEDataBase/XG.1.0.3911.inl"
#include "HLEDataBase/XG.1.0.4034.inl"
#include "HLEDataBase/XG.1.0.4361.inl"
#include "HLEDataBase/XG.1.0.4432.inl"
#include "HLEDataBase/XG.1.0.4627.inl"
#include "HLEDataBase/XG.1.0.5028.inl"
#include "HLEDataBase/XG.1.0.5233.inl"
#include "HLEDataBase/XG.1.0.5344.inl"
#include "HLEDataBase/XG.1.0.5558.inl"
#include "HLEDataBase/XG.1.0.5788.inl"
#include "HLEDataBase/XG.1.0.5849.inl"

// ******************************************************************
// * XG_OOVPA
// ******************************************************************
OOVPATable XG_OOVPA[] = {

	REGISTER_OOVPAS(XGIsSwizzledFormat, 3911, 4361),
	// REGISTER_OOVPAS(XGSwizzleRect, 3911, 4361), // TODO : Uncomment
	// REGISTER_OOVPAS(XGUnswizzleRect, 3911), // TODO : Uncomment
	REGISTER_OOVPAS(XGSwizzleBox, 3911, 4627),
	REGISTER_OOVPAS(XGUnswizzleBox, 5558), // (* UNTESTED *)
	REGISTER_OOVPAS(XGWriteSurfaceOrTextureToXPR, 3911, 4627),
	REGISTER_OOVPAS(XGSetTextureHeader, 3911),
	REGISTER_OOVPAS(XGSetVertexBufferHeader, 4361),
	REGISTER_OOVPAS(XGSetIndexBufferHeader, 4361),
	REGISTER_OOVPAS(XGCompressRect, 4361),
	// REGISTER_OOVPAS(XFONT_OpenBitmapFontFromMemory, 5788),
};

// ******************************************************************
// * XG_OOVPA_SIZE
// ******************************************************************
uint32 XG_OOVPA_SIZE = sizeof(XG_OOVPA);

#endif