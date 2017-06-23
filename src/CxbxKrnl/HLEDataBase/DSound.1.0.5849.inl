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

// ******************************************************************
// * DSound_5849
// ******************************************************************
OOVPATable DSound_5849[] = {

	REGISTER_OOVPAS(DirectSoundCreate, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundBuffer, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPlayRegion, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPlayRegion, 4361), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_SetBufferData, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetBufferData, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetBufferData, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_GetStatus, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetStatus, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetI3DL2Listener, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_SetI3DL2Listener, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetFormat, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFormat, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFormat, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoiceSettings_SetMixBinVolumes, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBinVolumes, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMixBinVolumes, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBinVolumes2, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBinVolumes2, 5788), // PATCH 
	REGISTER_OOVPAS(IDirectSound_CreateSoundBuffer, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetFrequency, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFrequency, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFrequency, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetVolume, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetVolume, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetVolume, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVolume, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetVolume, 4134), // PATCH 
	REGISTER_OOVPAS(IDirectSound_Release, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_DownloadEffectsImage, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_SetOrientation, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMaxDistance, 5344), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMaxDistance, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMaxDistance, 5788), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMinDistance, 5344), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMinDistance, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMinDistance, 5788), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Play, 4361), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_Stop, 4361), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_Stop2, 4361), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_StopEx, 4361), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_StopEx, 4361), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_Stop, 4134), // PATCH (Possibly weak, but quite OK for 4627 DSOUND)
	REGISTER_OOVPAS(IDirectSoundBuffer_Release, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetHeadroom, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetHeadroom, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetHeadroom, 5558), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_Lock, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoiceSettings_SetMixBins, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBins, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMixBins, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBins, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBins, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetMixBinHeadroom, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_SetMixBinHeadroom, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetPosition, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_SetPosition, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetVelocity, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_SetVelocity, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CommitDeferredSettings, 5788), // PATCH 
	REGISTER_OOVPAS(DirectSoundCreateBuffer, 4627), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_SetCurrentPosition, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetCurrentPosition, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetCurrentPosition, 5788), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_GetCurrentPosition, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetCurrentPosition, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetLoopRegion, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetLoopRegion, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetRolloffFactor, 5849), // XREF
	REGISTER_OOVPAS(IDirectSound_SetRolloffFactor, 4134), // PATCH TODO : Use 5344?
	REGISTER_OOVPAS(CDirectSound_SetDopplerFactor, 5788), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDopplerFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetPitch, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPitch, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPitch, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_PlayEx, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_PlayEx, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetRolloffFactor, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetRolloffFactor, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetRolloffFactor, 5788), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetDopplerFactor, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetDopplerFactor, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetDopplerFactor, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetPosition, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPosition, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPosition, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetVelocity, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetVelocity, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVelocity, 5558), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Pause, 4928), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Pause, 4928), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Pause, 4928), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundStream, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_CreateSoundStream, 5558), // PATCH 
	REGISTER_OOVPAS(DirectSoundCreateStream, 5788), // PATCH 
	REGISTER_OOVPAS(CMcpxStream_Pause, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_Pause, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_FlushEx, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundStream_FlushEx, 4627), // PATCH 
	REGISTER_OOVPAS(DirectSoundDoWork, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SynchPlayback, 5558), // PATCH 
	REGISTER_OOVPAS(XAudioDownloadEffectsImage, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSound_SetEffectData, 5344), // PATCH 
	REGISTER_OOVPAS(CMemoryManager_PoolAlloc, 5788), // XREF
	REGISTER_OOVPAS(XFileCreateMediaObjectAsync, 5788), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetFormat, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetPitch, 5849), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetHeadroom, 5558), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Play2, 5788), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Play, 5788), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Play, 5558), // PATCH 
	REGISTER_OOVPAS(IDirectSound_AddRef, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetDistanceFactorA, 5558), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDistanceFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetAllParameters, 5849), // XREF
	REGISTER_OOVPAS(IDirectSound_SetAllParameters, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetFilter, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetFilter, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFilter, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetFilter, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFilter, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetEG, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetEG, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetEG, 5849), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetEG, 5849), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetEG, 5849), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMode, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMode, 5849), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMode, 5849), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetI3DL2Source, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetI3DL2Source, 5849), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetI3DL2Source, 5849), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetAllParameters, 5849), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetAllParameters, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetAllParameters, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_Use3DVoiceData, 5558), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Use3DVoiceData, 5558), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Use3DVoiceData, 5558), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_Use3DVoiceData, 5558), // XREF
};

// ******************************************************************
// * DSound_5849_SIZE
// ******************************************************************
uint32 DSound_5849_SIZE = sizeof(DSound_5849);
