// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->DSound.OOVPA.inl
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

#ifndef DSOUND_OOVPA_INL
#define DSOUND_OOVPA_INL

#include "OOVPA.h"

#include "HLEDataBase/DSound.1.0.3936.inl"
#include "HLEDataBase/DSound.1.0.4134.inl"
#include "HLEDataBase/DSound.1.0.4361.inl"
#include "HLEDataBase/DSound.1.0.4432.inl"
#include "HLEDataBase/DSound.1.0.4627.inl"
#include "HLEDataBase/DSound.1.0.5028.inl"
#include "HLEDataBase/DSound.1.0.5233.inl"
#include "HLEDataBase/DSound.1.0.5344.inl"
#include "HLEDataBase/DSound.1.0.5558.inl"
#include "HLEDataBase/DSound.1.0.5788.inl"
#include "HLEDataBase/DSound.1.0.5849.inl"

// ******************************************************************
// * DSound_OOVPA
// ******************************************************************
OOVPATable DSound_OOVPA[] = {

	REGISTER_OOVPAS(DirectSoundCreate, 3936), // PATCH 
	REGISTER_OOVPAS(DirectSoundDoWorkB, 3936), // XREF
	REGISTER_OOVPAS(DirectSoundDoWorkA, 3936), // XREF
	REGISTER_OOVPAS(DirectSoundDoWork, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundStream, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_CreateSoundStream, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CreateSoundBuffer, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_CreateBuffer, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_Release, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPitchB, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPitchA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPitch, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetStatus, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_GetStatus, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetStatus, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVolumeB, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVolumeA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVolume, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetCurrentPositionB, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetCurrentPositionA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetCurrentPosition, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPlayRegionA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPlayRegion, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_LockA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Lock, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetHeadroomA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetHeadroom, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetBufferDataA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetBufferData, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetMixBins, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBins, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBins, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBins, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_GetCurrentPosition, 3936), // XREF
	REGISTER_OOVPAS(CMcpxBuffer_GetCurrentPosition2, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_GetCurrentPosition, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_GetCurrentPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxBuffer_Play, 3925), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_Play, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Play, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_Stop, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_Stop, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_SetVolume, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetVolume, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetConeAnglesB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeAnglesA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeAngles, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOutsideVolumeB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOutsideVolumeA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOutsideVolume, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_Set3dParameters, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetAllParameters, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetAllParameters, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMaxDistanceC, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMaxDistanceB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMaxDistanceA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMaxDistance, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMinDistanceC, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMinDistanceB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMinDistanceA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetMinDistance, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetVelocityC, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetVelocityB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetVelocityA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetVelocity, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetVelocity, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetVelocity, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOrientationC, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOrientationB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOrientationA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetConeOrientation, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetPositionC, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetPositionB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetPositionA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetPosition, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetFrequencyB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetFrequencyA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetFrequency, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetFrequency, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_Set3dMode, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetMode, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMode, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetHeadroom, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetHeadroom, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_SetOrientation, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_CommitDeferredSettingsB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_CommitDeferredSettingsA, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_CommitDeferredSettings, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_Release, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetDistanceFactorB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetDistanceFactorA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDistanceFactor, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetRolloffFactorB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetRolloffFactorA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetRolloffFactor, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetMixBinHeadroomB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetMixBinHeadroomA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetMixBinHeadroom, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetPositionB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetPositionA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetPosition, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetVelocityB, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetVelocityA, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetVelocity, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxAPU_Set3dParameters, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetAllParameters, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetAllParameters, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_DownloadEffectsImage, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMode, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxAPU_Set3dDopplerFactor, 3936), // XREF
	REGISTER_OOVPAS(CDirectSound_SetDopplerFactor, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetDopplerFactor, 3936), // PATCH 
	REGISTER_OOVPAS(CSensaura3d_GetFullHRTFFilterPair, 3925), // XREF
	REGISTER_OOVPAS(CSensaura3d_GetFullHRTFFilterPair, 3936), // XREF
	REGISTER_OOVPAS(DirectSoundUseFullHRTF, 3925), // PATCH 
	REGISTER_OOVPAS(CDirectSound_GetCaps, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_GetCaps, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetConeAngles, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetConeAngles, 3936), // PATCH 
	REGISTER_OOVPAS(DirectSoundEnterCriticalSection, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundBuffer_PlayEx, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_PlayEx, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSound_AddRef, 3936), // PATCH 
	REGISTER_OOVPAS(DirectSoundGetSampleTime, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_AddRef, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMinDistance, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMinDistance, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetMaxDistance, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMaxDistance, 3936), // PATCH 
	REGISTER_OOVPAS(CMcpxVoiceClient_Commit3dSettings, 3936), // XREF
	REGISTER_OOVPAS(CMcpxVoiceClient_SetI3DL2Source, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundVoice_SetI3DL2Source, 3936), // XREF
	REGISTER_OOVPAS(CDirectSoundStream_SetI3DL2Source, 3936), // PATCH 
	REGISTER_OOVPAS(IDirectSoundBuffer_SetI3DL2Source, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundVoice_SetMixBinVolumes, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetMixBinVolumes, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_SetI3DL2Listener, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_SetI3DL2Listener, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSound_EnableHeadphones, 3936), // XREF
	REGISTER_OOVPAS(IDirectSound_EnableHeadphones, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_SetMixBinVolumes, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundBuffer_SetNotificationPositions, 3936), // XREF
	REGISTER_OOVPAS(IDirectSoundBuffer_SetNotificationPositions, 3936), // PATCH 
	REGISTER_OOVPAS(CDirectSoundStream_Pause, 3936), // PATCH 
};

// ******************************************************************
// * DSound_OOVPA_SIZE
// ******************************************************************
uint32 DSound_OOVPA_SIZE = sizeof(DSound_OOVPA);

#endif