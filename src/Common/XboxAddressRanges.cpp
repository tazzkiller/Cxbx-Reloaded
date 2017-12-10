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
// *   Common->XboxAddressRanges.cpp
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx is free software; you can redistribute it
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
// *  (c) 2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h> // For DWORD, CALLBACK, VirtualAlloc, LPVOID, SIZE_T, HMODULE 

#include "XboxAddressRanges.h"

// Maps an XboxAddressRangeType to page protection flags, for use by VirtualAlloc
DWORD XboxAddressRangeTypeToVirtualAllocPageProtectionFlags(XboxAddressRangeType type)
{
	switch (type) {
	case MemLowVirtual:
	case MemPhysical: // Note : We'll allow execution in kernel space
		return PAGE_EXECUTE_READWRITE;

	case MemPageTable:
	case MemNV2APRAMIN:
		return PAGE_READWRITE;

	case DeviceNV2A_a:
	case DeviceNV2A_b:
	case DeviceAPU:
	case DeviceAC97:
	case DeviceUSB0:
	case DeviceUSB1:
	case DeviceNVNet:
	case DeviceFlash_a:
	case DeviceFlash_b:
	case DeviceFlash_c:
	case DeviceFlash_d:
	case DeviceMCPX:
		return PAGE_NOACCESS;

	default:
		return 0; // UNHANDLED
	}
}

// This array keeps track of which ranges have successfully been reserved.
// Having this helps debugging, but isn't strictly necessary, as we could
// retrieve the same information using VirtualQuery.
XboxAddressRange ReservedRanges[128];
int ReservedRangeCount = 0;

// Reserve an address range up to the extend of what the host allows.
bool ReserveMemoryRange(XboxAddressRangeType xart)
{
	// TODO : Instead of hard-coding the blocksize, read it from system's allocation granularity.
	// In practice though, nearly all will be using 64 Kib.
	const int BLOCK_SIZE = KB(64);

	//	assert(XboxAddressRanges[xart].Type == xart);
	unsigned __int32 Start = XboxAddressRanges[xart].Start;
	int Size = XboxAddressRanges[xart].Size;

	bool HadAnyFailure = false;
	if (Start == 0) {
		// The zero page (the entire first 64 KB block) can't be reserved (if we would
		// try to reserve VirtualAlloc at address zero, it would hand us another address)
		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
		HadAnyFailure = true;
	}

	// Safeguard against bounds overflow
	if (ReservedRangeCount < ARRAY_SIZE(ReservedRanges)) {
		// Initialize the reservation of a new range
		ReservedRanges[ReservedRangeCount].Type = xart;
		ReservedRanges[ReservedRangeCount].Start = Start;
		ReservedRanges[ReservedRangeCount].Size = 0;
	}

	// Reserve this range in 64 Kb block increments, so later on our
	// memory-management code can VirtualFree() each block individually :
	bool HadFailure = HadAnyFailure;
	const DWORD Protect = XboxAddressRangeTypeToVirtualAllocPageProtectionFlags(xart);
	while (Size > 0) {
		SIZE_T BlockSize = (SIZE_T)(Size > BLOCK_SIZE) ? BLOCK_SIZE : Size;
		LPVOID Result = VirtualAlloc((LPVOID)Start, BlockSize, MEM_RESERVE, Protect);
		if (Result == NULL) {
			HadFailure = true;
			HadAnyFailure = true;
		}
		else {
			// Safeguard against bounds overflow
			if (ReservedRangeCount < ARRAY_SIZE(ReservedRanges)) {
				if (HadFailure) {
					HadFailure = false;
					// Starting a new range - did the previous one have any size?
					if (ReservedRanges[ReservedRangeCount].Size > 0) {
						// Then start a new range, and copy over the current type
						ReservedRangeCount++;
						ReservedRanges[ReservedRangeCount].Type = xart;
					}

					// Register a new ranges starting address
					ReservedRanges[ReservedRangeCount].Start = Start;
				}

				// Accumulate the size of each successfull reservation
				ReservedRanges[ReservedRangeCount].Size += BlockSize;
			}
		}

		// Handle the next block
		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}

	// Safeguard against bounds overflow
	if (ReservedRangeCount < ARRAY_SIZE(ReservedRanges)) {
		// Keep the current block only if it contains a successfully reserved range
		if (ReservedRanges[ReservedRangeCount].Size > 0) {
			ReservedRangeCount++;
		}
	}

	// Only a complete success when the entire request was reserved in a single range
	// (Otherwise, we have either a complete failure, or reserved it partially over multiple ranges)
	return !HadAnyFailure;
}

void UnreserveMemoryRange(XboxAddressRangeType xart)
{
	const int BLOCK_SIZE = KB(64);

	unsigned __int32 Start = XboxAddressRanges[xart].Start;
	int Size = XboxAddressRanges[xart].Size;

	while (Size > 0) {
		// To release, dwSize must be zero!
		VirtualFree((LPVOID)Start, (SIZE_T)0, MEM_RELEASE);
		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}
}

bool AllocateMemoryRange(XboxAddressRangeType xart)
{
	const int BLOCK_SIZE = KB(64);

	unsigned __int32 Start = XboxAddressRanges[xart].Start;
	int Size = XboxAddressRanges[xart].Size;

	while (Size > 0) {
		int BlockSize = (Size > BLOCK_SIZE) ? BLOCK_SIZE : Size;
		if (nullptr == VirtualAlloc((void*)Start, BlockSize, MEM_COMMIT, PAGE_READWRITE)) { // MEM_RESERVE already done by Cxbx-Loader.exe
			return false;
		}

		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}

	return true;
}
