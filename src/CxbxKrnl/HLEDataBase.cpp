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
// *   Cxbx->Win32->CxbxKrnl->HLEDataBase.cpp
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

#undef FIELD_OFFSET     // prevent macro redefinition warnings
#include <windows.h>

#include "CxbxKrnl.h" // For xbaddr

extern "C" const char *szHLELastCompileTime = __TIMESTAMP__;

const char *Lib_D3D8 = "D3D8";
const char *Lib_D3D8LTCG = "D3D8LTCG";
const char *Lib_D3DX8 = "D3DX8";
const char *Lib_DSOUND = "DSOUND";
const char *Lib_XACTENG = "XACTENG";
const char *Lib_XAPILIB = "XAPILIB";
const char *Lib_XGRAPHC = "XGRAPHC";
const char *Lib_XNETS = "XNETS";
const char *Lib_XONLINE = "XONLINE"; // TODO : Typo for XONLINES?
const char *Lib_XONLINES = "XONLINES";

#include "Emu.h"
#include "EmuXTL.h"
#include "HLEDataBase.h"
#include "HLEDataBase/D3D8.OOVPA.inl"
//#include "HLEDataBase/DSound.OOVPA.inl"
//#include "HLEDataBase/XactEng.OOVPA.inl"
#include "HLEDataBase/Xapi.OOVPA.inl"
//#include "HLEDataBase/XG.OOVPA.inl"
#include "HLEDataBase/XNet.OOVPA.inl"
#include "HLEDataBase/XOnline.OOVPA.inl"

// ******************************************************************
// * HLEDataBase
// ******************************************************************

const HLEData HLEDataBase[] =
{
	{ Lib_D3D8, D3D8_OOVPA, D3D8_OOVPA_SIZE },
	// { Lib_DSOUND, DSound_OOVPA, DSound_OOVPA_SIZE },
	// { Lib_XACTENG, XactEng_OOVPA, XactEng_OOVPA_SIZE },
	{ Lib_XAPILIB, XAPI_OOVPA, XAPI_OOVPA_SIZE },
	// { Lib_XGRAPHC, XG_OOVPA, XG_OOVPA_SIZE },
	{ Lib_XNETS, XNet_OOVPA, XNet_OOVPA_SIZE },
	{ Lib_XONLINE, XOnline_OOVPA, XOnline_OOVPA_SIZE },
};

// ******************************************************************
// * HLEDataBaseCount
// ******************************************************************
extern const uint32 HLEDataBaseCount = sizeof(HLEDataBase) / sizeof(HLEData);

// ******************************************************************
// * XRefDataBase
// ******************************************************************
extern xbaddr XRefDataBase[XREF_COUNT] = { 0 }; // Reset and populated by EmuHLEIntercept
