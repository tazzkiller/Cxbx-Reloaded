// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->DSound.1.0.5028.cpp
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
// * XFileCreateMediaObjectEx
// ******************************************************************
OOVPA_NO_XREF(XFileCreateMediaObjectEx, 5028, 8)

        { 0x03, 0x56 },
        { 0x22, 0x1B },
        { 0x89, 0x1B },
        { 0xA9, 0x85 },
        { 0xAA, 0xF6 },
        { 0xAB, 0x7C },
        { 0xAC, 0x0C },
        { 0xAD, 0x57 },
OOVPA_END;

// ******************************************************************
// * DSound_5028
// ******************************************************************
OOVPATable DSound_5028[] = {

	REGISTER_OOVPAS(DirectSoundCreate, 4134), // PATCH 
	REGISTER_OOVPAS(DirectSoundDoWork, 4134), // PATCH 
	REGISTER_OOVPAS(DirectSoundGetSampleTime, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundBuffer, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPlayRegion, 4361), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPlayRegion, 4361), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetLoopRegion, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetLoopRegion, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetI3DL2Listener, 4134), // XREF
	REGISTER_OOVPAS(IDirectSound_SetI3DL2Listener, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetMixBinHeadroom, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_SetMixBinHeadroom, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetHeadroomA, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetHeadroom, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetVelocity, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_SetVelocity, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetAllParametersA, 4627), // XREF
	REGISTER_OOVPAS(CDirectSound_SetAllParametersA, 4721), // XREF
	REGISTER_OOVPAS(CDirectSound_SetAllParametersA, 4831), // XREF
	REGISTER_OOVPAS(CDirectSound_SetAllParameters, 4928), // XREF
	REGISTER_OOVPAS(IDirectSound_SetAllParameters, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoiceSettings_SetMixBins, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBins, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMixBins, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBins, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoiceSettings_SetMixBinVolumes, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBinVolumes, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMixBinVolumes, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBinVolumes2, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBinVolumes2, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetPositionA, 4627), // XREF
	REGISTER_OOVPAS(CDirectSound_SetPositionA, 4134), // XREF TODO: Find a cure for laziness...
	REGISTER_OOVPAS(IDirectSound_SetPosition, 3936), // PATCH 
	REGISTER_OOVPAS(DirectSoundCreateBuffer, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSound_CreateSoundBuffer, 4134), // PATCH 
	REGISTER_OOVPAS(IDirectSound_AddRef, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetFrequency, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFrequency, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFrequency, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetVolume, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetVolume, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetVolume, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVolume, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetVolume, 4134), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_LockA, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Lock, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundStream, 4361), // XREF
	REGISTER_OOVPAS(IDirectSound_CreateSoundStream, 3936), // PATCH 
	REGISTER_OOVPAS(DirectSoundCreateStream, 4361), // PATCH 
	REGISTER_OOVPAS(CMcpxStream_Pause, 4361), // XREF
	REGISTER_OOVPAS(CMcpxStream_Pause, 4928), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_Pause, 4361), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_SetBufferData, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetBufferData, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetBufferData, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 4134), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 4721), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 4831), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_GetStatus, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetStatus, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_SetCurrentPosition, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetCurrentPosition, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetCurrentPosition, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetCurrentPosition, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_GetCurrentPosition, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetCurrentPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_GetSpeakerConfig, 4627), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Play, 4361), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_Play, 4721), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_Play, 4928), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Play, 4361), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Play, 4361), // PATCH 
	REGISTER_OOVPAS(IDirectSound_Release, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_DownloadEffectsImage, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_SetOrientation, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetDistanceFactorA, 4134), // XREF
	REGISTER_OOVPAS(CDirectSound_SetDistanceFactorA, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDistanceFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetRolloffFactor, 4134), // XREF
	REGISTER_OOVPAS(IDirectSound_SetRolloffFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetDopplerFactor, 4134), // XREF
	REGISTER_OOVPAS(CDirectSound_SetDopplerFactor, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDopplerFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CommitDeferredSettings, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMaxDistance, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMaxDistance, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMaxDistance, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMaxDistance, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMinDistance, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMinDistance, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMinDistance, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMinDistance, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetRolloffFactor, 4134), // XREF s+
	REGISTER_OOVPAS(CDirectSoundBuffer_SetRolloffFactor, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetRolloffFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetRolloffFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetDistanceFactor, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetDistanceFactor, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetDistanceFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetConeAngles, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetConeAngles, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetConeAngles, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetConeOrientation, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetConeOrientation, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetConeOrientation, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOrientation, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetConeOutsideVolume, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetConeOutsideVolume, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetConeOutsideVolume, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOutsideVolume, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetPosition, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPosition, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetPosition, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetVelocity, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetVelocity, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVelocity, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetVelocity, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetDopplerFactor, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetDopplerFactor, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetDopplerFactor, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetI3DL2Source, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetI3DL2Source, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetI3DL2Source, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetI3DL2Source, 4134), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_Stop, 4134), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_Release, 3936), // PATCH +s
	REGISTER_OOVPAS(CDirectSoundVoice_SetFormat, 4721), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFormat, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFormat, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSound_EnableHeadphones, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_EnableHeadphones, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_AddRef, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_GetOutputLevels, 4627), // XREF
	REGISTER_OOVPAS(IDirectSound_GetOutputLevels, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetAllParameters, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetAllParameters, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetAllParameters, 4134), // PATCH Use that for now. Okay, it's your call pal...
	REGISTER_OOVPAS(CDirectSoundStream_SetAllParameters, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Pause, 4928), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Pause, 4928), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Pause, 4928), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 4721), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetStatus, 4721), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBins, 4627), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetEG, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetEG, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetEG, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetEG, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetEG, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_FlushEx, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundStream_FlushEx, 4627), // PATCH 
	REGISTER_OOVPAS(XAudioDownloadEffectsImage, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMode, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMode, 4134), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMode, 4134), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMode, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetFilter, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetFilter, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetFilter, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetFilter, 4627), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFilter, 4134), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Play2, 4361), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_PlayEx, 4361), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_PlayEx, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Stop, 4361), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_Stop2, 4361), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_StopEx, 4361), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_StopEx, 4361), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetVelocity, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetPitch, 4134), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPitch, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPitch, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetHeadroom, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetHeadroom, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetOutputBuffer, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetOutputBuffer, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetOutputBuffer, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetOutputBuffer, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetRolloffCurve, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetRolloffCurve, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetRolloffCurve, 4627), // PATCH Was XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetRolloffCurve, 4627), // PATCH 
	REGISTER_OOVPAS(XFileCreateMediaObjectEx, 5028), // PATCH 
	REGISTER_OOVPAS(XWaveFileCreateMediaObject, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetNotificationPositions, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetNotificationPositions, 4627), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetLFO, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetLFO, 4627), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_SetLFO, 4627), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetLFO, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetLFO, 4627), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetPitch, 4627), // PATCH 
};

// ******************************************************************
// * DSound_5028_SIZE
// ******************************************************************
uint32 DSound_5028_SIZE = sizeof(DSound_5028);
