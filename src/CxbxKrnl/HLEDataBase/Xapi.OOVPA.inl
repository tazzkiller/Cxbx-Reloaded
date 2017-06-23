// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->Xapi.1.0.3911.h
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
#ifndef XAPI_3911_H
#define XAPI_3911_H

#include "OOVPA.h"

#include "HLEDataBase/Xapi.1.0.3911.inl"
#include "HLEDataBase/Xapi.1.0.4034.inl"
#include "HLEDataBase/Xapi.1.0.4134.inl"
#include "HLEDataBase/Xapi.1.0.4361.inl"
#include "HLEDataBase/Xapi.1.0.4432.inl"
#include "HLEDataBase/Xapi.1.0.4627.inl"
#include "HLEDataBase/Xapi.1.0.4721.inl"
#include "HLEDataBase/Xapi.1.0.5028.inl"
#include "HLEDataBase/Xapi.1.0.5233.inl"
#include "HLEDataBase/Xapi.1.0.5344.inl"
#include "HLEDataBase/Xapi.1.0.5558.inl"
#include "HLEDataBase/Xapi.1.0.5788.inl"
#include "HLEDataBase/Xapi.1.0.5849.inl"

// ******************************************************************
// * XAPI_OOVPA
// ******************************************************************
OOVPATable XAPI_OOVPA[] = {

	REGISTER_OOVPAS(GetExitCodeThread, 3911), // PATCH 
	REGISTER_OOVPAS(XInitDevices, 3911, 5233), // PATCH 
	// REGISTER_OOVPAS(CreateMutex, 3911), // PATCH Too High Level
	// REGISTER_OOVPAS(CreateThread, 3911), // PATCH Too High Level
	REGISTER_OOVPAS(GetTimeZoneInformation, 3911), // DISABLED
	REGISTER_OOVPAS(XRegisterThreadNotifyRoutine, 3911), // PATCH 
	// REGISTER_OOVPAS(XCalculateSignatureBegin, 3911, 4627), // PATCH 
	// REGISTER_OOVPAS(XCalculateSignatureBeginEx, 4627), // PATCH +s, not necessary?
	// REGISTER_OOVPAS(XCalculateSignatureUpdate, 4627), // PATCH 
	// REGISTER_OOVPAS(XCalculateSignatureEnd, 4627), // PATCH s+
	REGISTER_OOVPAS(XGetDevices, 3911), // PATCH 
	REGISTER_OOVPAS(XGetDeviceChanges, 3911, 5233), // PATCH 
	REGISTER_OOVPAS(XInputOpen, 3911, 4134, 4361), // PATCH 
	REGISTER_OOVPAS(XID_fCloseDevice, 3911, 4361, 4627, 4928, 5558), // XREF
	REGISTER_OOVPAS(XInputClose, 3911, 5558), // PATCH 
	REGISTER_OOVPAS(XInputGetCapabilities, 3911, 4361, 4831, 5233, 5558), // PATCH 
	REGISTER_OOVPAS(XInputGetDeviceDescription, 4831), // PATCH 
	REGISTER_OOVPAS(XInputGetState, 3911, 4134, 4361, 4928, 5558), // PATCH 
	REGISTER_OOVPAS(XInputSetState, 3911, 4361, 4928, 5233), // PATCH 
	REGISTER_OOVPAS(SetThreadPriority, 3911), // PATCH 
	REGISTER_OOVPAS(SetThreadPriorityBoost, 3911, 5788), // PATCH 
	REGISTER_OOVPAS(GetThreadPriority, 3911, 5788), // PATCH 
	// REGISTER_OOVPAS(GetThreadPriorityBoost, 5788, 5849), // PATCH 
	REGISTER_OOVPAS(CreateFiber, 3911), // DISABLED
	REGISTER_OOVPAS(DeleteFiber, 3911), // DISABLED
	REGISTER_OOVPAS(SwitchToFiber, 3911), // DISABLED
	REGISTER_OOVPAS(ConvertThreadToFiber, 3911), // DISABLED
	REGISTER_OOVPAS(SignalObjectAndWait, 3911), // PATCH 
	REGISTER_OOVPAS(QueueUserAPC, 3911), // PATCH 
	// REGISTER_OOVPAS(lstrcmpiW, 3911), // PATCH 
	REGISTER_OOVPAS(XMountAlternateTitleA, 3911, 4928), // PATCH 
	REGISTER_OOVPAS(XUnmountAlternateTitleA, 3911), // PATCH 
	REGISTER_OOVPAS(XMountMUA, 3911, 4134, 4361), // PATCH 
	REGISTER_OOVPAS(XLaunchNewImageA, 3911, 4928, 5558), // PATCH 
	REGISTER_OOVPAS(XGetLaunchInfo, 3911), // DISABLED
	REGISTER_OOVPAS(XAutoPowerDownResetTimer, 3911), // DISABLED Just calls KeSetTimer
	REGISTER_OOVPAS(XMountMURootA, 3911), // PATCH 
	REGISTER_OOVPAS(XMountUtilityDrive, 3911, 4134, 4432), // TODO: This needs to be verified on 4361, not just 4242!
	REGISTER_OOVPAS(OutputDebugStringA, 3911), // PATCH 
	// REGISTER_OOVPAS(ReadFileEx, 3911), // PATCH 
	// REGISTER_OOVPAS(WriteFileEx, 3911), // PATCH 
	// REGISTER_OOVPAS(MoveFileA, 4627), // PATCH 
	// REGISTER_OOVPAS(CloseHandle, 3911), // PATCH 
	// REGISTER_OOVPAS(ExitThread, 3911), // PATCH 
	// REGISTER_OOVPAS(XapiThreadStartup, 4361), // PATCH  obsolete?
	REGISTER_OOVPAS(GetOverlappedResult, 4627), // PATCH 
	REGISTER_OOVPAS(timeKillEvent, 4134, 4627, 5849), // PATCH 
	REGISTER_OOVPAS(XInputPoll, 4134), // PATCH 
	REGISTER_OOVPAS(timeSetEvent, 4134, 4627, 5849), // PATCH 
	REGISTER_OOVPAS(XFormatUtilityDrive, 4627), // PATCH 
	REGISTER_OOVPAS(XSetProcessQuantumLength, 4627), // PATCH 
	REGISTER_OOVPAS(RaiseException, 4627), // PATCH 
	// REGISTER_OOVPAS(RtlCreateHeap, 3911), // PATCH obsolete, (* unchanged since 1.0.4361 *) (* OR FARTHER *)
	// REGISTER_OOVPAS(RtlAllocateHeap, 3911), // PATCH obsolete (* unchanged since 1.0.4361 *) (* OR FARTHER *)
	// REGISTER_OOVPAS(RtlReAllocateHeap, 4627), // PATCH obsolete 
	// REGISTER_OOVPAS(RtlFreeHeap, 4627), // PATCH obsolete 
	// REGISTER_OOVPAS(RtlSizeHeap, 4627), // PATCH obsolete 
	// REGISTER_OOVPAS(RtlDestroyHeap, 4627), // PATCH obsolete 
	// REGISTER_OOVPAS(XapiInitProcess, 4361), // PATCH obsolete, Too High Level
	// REGISTER_OOVPAS(XapiBootDash, 3911), // PATCH obsolete 
	REGISTER_OOVPAS(XapiFiberStartup, 5558), // DISABLED
	// REGISTER_OOVPAS(SwitchToThread, 5788, 5849), // PATCH 
	REGISTER_OOVPAS(XGetDeviceEnumerationStatus, 5788, 5849), // PATCH 
};

// ******************************************************************
// * XAPI_OOVPA_SIZE
// ******************************************************************
uint32 XAPI_OOVPA_SIZE = sizeof(XAPI_OOVPA);

#endif
