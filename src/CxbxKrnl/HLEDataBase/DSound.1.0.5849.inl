// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->DSound.1.0.5849.cpp
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
// * DirectSound::CDirectSound::SetRolloffFactor
// Xbe Explorer generated pattern, derived from address $00439F47 in "SpiderMan 2" :
// 56E8........833DD4........0FB6F0741685F6740B68E03B4400FF15...... 00 0000 005F ? SetRolloffFactor@CDirectSound@DirectSound@@QAGJMK@Z ^ 0002R ? DirectSoundEnterCriticalSection@@YGHXZ ^0009D _g_fDirectSoundInFinalRelease ^ 001DD __imp__RtlLeaveCriticalSection@4 ^ 0046R ? CommitDeferredSettings@CDirectSound@DirectSound@@QAGJXZ ^ 0055D __imp__RtlLeaveCriticalSection@4
// ******************************************************************
OOVPA_XREF(CDirectSound_SetRolloffFactor, 5849, 20,

    XREF_SETROLLOFFFACTORA,
    XRefZero)

        { 0x00, 0x56 },
        { 0x01, 0xE8 },
        { 0x06, 0x83 },
        { 0x07, 0x3D },
        { 0x0D, 0x0F },
        { 0x0E, 0xB6 },
		{ 0x0F, 0xF0 },
		{ 0x10, 0x74 },
		{ 0x11, 0x16 },
		{ 0x12, 0x85 },
		{ 0x13, 0xF6 },
		{ 0x14, 0x74 },
		{ 0x15, 0x0B },
		{ 0x16, 0x68 },
		{ 0x1A, 0x00 },
		{ 0x1B, 0xFF },
		{ 0x1C, 0x15 },
		{ 0x35, 0x6C },
		{ 0x3C, 0x10 },
		{ 0x5C, 0xC2 },
OOVPA_END;

// ******************************************************************
// * DirectSound::CDirectSoundStream::SetPitch
// ******************************************************************
OOVPA_XREF(CDirectSoundStream_SetPitch, 5849, 12,

    XRefNoSaveIndex,
    XRefOne)

        XREF_ENTRY( 0x36, XREF_DSBUFFERSETPITCHB ),

        { 0x00, 0x56 },
        { 0x0C, 0x00 },
        { 0x14, 0x74 },
        { 0x21, 0xB8 },
        { 0x2A, 0x24 },
        { 0x35, 0xE8 },
        { 0x40, 0x68 },
        { 0x4B, 0x8B },
	{ 0x4F, 0xC2 },
	{ 0x50, 0x08 },
	{ 0x51, 0x00 },
OOVPA_END;

// ******************************************************************
// * DirectSound::CDirectSound::SetAllParameters
// ******************************************************************
#define CDirectSound_SetAllParameters_5849 CDirectSound_SetAllParameters_5558

// ******************************************************************
// * DirectSound::CMcpxVoiceClient::SetFilter
// ******************************************************************
#define CMcpxVoiceClient_SetFilter_5849 CMcpxVoiceClient_SetFilter_5558

// ******************************************************************
// * DirectSound::CMcpxVoiceClient::SetEG
// ******************************************************************
#define CMcpxVoiceClient_SetEG_5849 CMcpxVoiceClient_SetEG_4627

// ******************************************************************
// * DirectSound::CDirectSoundVoice::SetEG
// ******************************************************************
#define CDirectSoundVoice_SetEG_5849 CDirectSoundVoice_SetEG_4627

// ******************************************************************
// * DirectSound::CDirectSoundStream::SetEG
// ******************************************************************
#define CDirectSoundStream_SetEG_5849 CDirectSoundStream_SetEG_4627

// ******************************************************************
// * DirectSound::CDirectSoundBuffer::SetEG
// ******************************************************************
#define CDirectSoundBuffer_SetEG_5849 CDirectSoundBuffer_SetEG_4627

// ******************************************************************
// * IDirectSoundBuffer_SetEG
// ******************************************************************
#define IDirectSoundBuffer_SetEG_5849 IDirectSoundBuffer_SetEG_4627

// ******************************************************************
// CDirectSoundVoice::SetMode
// ******************************************************************
#define CDirectSoundVoice_SetMode_5849 CDirectSoundVoice_SetMode_5344

// ******************************************************************
// CDirectSoundBuffer::SetMode
// ******************************************************************
#define CDirectSoundBuffer_SetMode_5849 CDirectSoundBuffer_SetMode_5233

// ******************************************************************
// * IDirectSoundBuffer_SetMode
// ******************************************************************
#define IDirectSoundBuffer_SetMode_5849 IDirectSoundBuffer_SetMode_5233

// ******************************************************************
// * DirectSound::CDirectSoundVoice::SetI3DL2Source
// ******************************************************************
#define CDirectSoundVoice_SetI3DL2Source_5849 CDirectSoundVoice_SetI3DL2Source_5558

// ******************************************************************
// * DirectSound::CDirectSoundBuffer::SetI3DL2Source
// ******************************************************************
#define CDirectSoundBuffer_SetI3DL2Source_5849 CDirectSoundBuffer_SetI3DL2Source_5558

// ******************************************************************
// * IDirectSoundBuffer_SetI3DL2Source
// ******************************************************************
#define IDirectSoundBuffer_SetI3DL2Source_5849 IDirectSoundBuffer_SetI3DL2Source_5558

// ******************************************************************
// * DirectSound::CDirectSoundVoice::SetAllParameters
// ******************************************************************
#define CDirectSoundVoice_SetAllParameters_5849 CDirectSoundVoice_SetAllParameters_5558
