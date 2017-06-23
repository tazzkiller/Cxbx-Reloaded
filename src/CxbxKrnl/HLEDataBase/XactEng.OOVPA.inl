// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->XactEng.OOVPA.inl
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

#ifndef XACTENG_OOVPA_INL
#define XACTENG_OOVPA_INL

#include "OOVPA.h"

#include "HLEDataBase/XactEng.1.0.4627.inl"
#include "HLEDataBase/XactEng.1.0.4928.inl"

// ******************************************************************
// * XactEng_OOVPA
// ******************************************************************
OOVPATable XactEng_OOVPA[] = {

	REGISTER_OOVPAS(XACTEngineCreate, 4627, 4928),
	REGISTER_OOVPAS(XACTEngineDoWork, 4627),
	REGISTER_OOVPAS(XACT_CEngine_RegisterWaveBank, 4627),
	REGISTER_OOVPAS(IXACTEngine_RegisterWaveBank, 4627),
	REGISTER_OOVPAS(XACT_CEngine_RegisterStreamedWaveBank, 4627, 4928),
	REGISTER_OOVPAS(IXACTEngine_RegisterStreamedWaveBank, 4627),
	REGISTER_OOVPAS(IXACTEngine_RegisterStreamedWaveBank, 4928),
	REGISTER_OOVPAS(XACT_CEngine_CreateSoundBank, 4627, 4928),
	REGISTER_OOVPAS(IXACTEngine_CreateSoundBank, 4627, 4928),
	REGISTER_OOVPAS(XACT_CEngine_DownloadEffectsImage, 4627),
	REGISTER_OOVPAS(IXACTEngine_DownloadEffectsImage, 4627),
	REGISTER_OOVPAS(XACT_CEngine_CreateSoundSource, 4627),
	REGISTER_OOVPAS(IXACTEngine_CreateSoundSource, 4627),
	REGISTER_OOVPAS(XACT_CSoundBank_GetSoundCueIndexFromFriendlyName, 4627),
	REGISTER_OOVPAS(IXACTSoundBank_GetSoundCueIndexFromFriendlyName, 4627),
	REGISTER_OOVPAS(IXACTSoundBank_Play, 4627),
	REGISTER_OOVPAS(XACT_CEngine_RegisterNotification, 4627),
	REGISTER_OOVPAS(IXACTEngine_RegisterNotification, 4627),
	REGISTER_OOVPAS(XACT_CEngine_GetNotification, 4627),
	REGISTER_OOVPAS(IXACTEngine_GetNotification, 4627),
	REGISTER_OOVPAS(XACT_CEngine_UnRegisterWaveBank, 4627, 4928),
	REGISTER_OOVPAS(IXACTEngine_UnRegisterWaveBank, 4627, 4928),
};

// ******************************************************************
// * XactEng_OOVPA_SIZE
// ******************************************************************
uint32 XactEng_OOVPA_SIZE = sizeof(XactEng_OOVPA);

#endif