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
// *   Cxbx-Loader->loader.cpp
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

#include "..\Common\XboxAddressRanges.h" // For XboxAddressRangeType, XboxAddressRanges

// Reserve the first 128 MB MemLowVirtual address range without inflating the EXE size,
// by simply declaring an array, first thing, in global scope. It gets placed in the BSS segment,
// so it still uses space in RAM, but not the actual executable.
// This variable *MUST* be this large, for it to take up address space
// so that all other code and data in this module are placed outside of the
// maximum virtual memory range.
#define VM_PLACEHOLDER_SIZE 128 * 1024 * 1024

// Note : In the old setup, we used #pragma section(".text"); __declspec(allocate(".text"))
// to put this variable at the exact image base address 0x00010000, but that resulted in
// an increase of the executable by 128 Mb. Since we won't return to the tiny loader code,
// and the executable doesn't contain any data that we refer too once we entered the emulation DLL,
// this will be all right. The only bit of data I transfer over, is on the stack, but the stack
// (of the initial thread) resides far outside of the reserved range, so that's all right too.
unsigned char virtual_memory_placeholder[VM_PLACEHOLDER_SIZE] = { 0 }; // = { OPCODE_NOP_90 };

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

	case DeviceNV2A:
	case DeviceAPU:
	case DeviceAC97:
	case DeviceUSB0:
	case DeviceUSB1:
	case DeviceNVNet:
	case DeviceFlash:
	case DeviceMCPX:
		return PAGE_NOACCESS;

	default:
		return 0; // UNHANDLED
	}
}

#define ARRAY_SIZE(a)                               \
  ((sizeof(a) / sizeof(*(a))) /                     \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

// Note : This executable is meant to be as tiny as humanly possible.
// The C++ runtime library is removed using https://stackoverflow.com/a/39220245/12170

DWORD CALLBACK rawMain()
{
	(void)virtual_memory_placeholder; // prevent optimization removing this data

	// Loop over all Xbox address ranges
	for (int i = 0; i < ARRAY_SIZE(XboxAddressRanges); i++) {
		// Try to reserve each address range
		if (nullptr == VirtualAlloc(
			(LPVOID)XboxAddressRanges[i].Start,
			(SIZE_T)XboxAddressRanges[i].Size,
			MEM_RESERVE,
			XboxAddressRangeTypeToVirtualAllocPageProtectionFlags(XboxAddressRanges[i].Type)
		)) {
			// Ranges that fail under Windows 7 Wow64 :
			//
			// { MemLowVirtual, 0x00000000, MB(128) }, // .. 0x08000000 (Retail Xbox uses 64 MB)
			// { MemTiled,      0xF0000000, MB( 64) }, // .. 0xF4000000
			// { DeviceUSB1,    0xFED08000, KB(  4) }, // .. 0xFED09000
			// { DeviceFlash,   0xFFC00000, MB(  4) }, // .. 0xFFFFFFFF (Flash mirror 4) - Will probably fail reservation
			// { DeviceMCPX,    0xFFFFFE00,    512  }, // .. 0xFFFFFFFF (not Chihiro, Xbox - if enabled)
			//
			// .. none of which are an issue for now.
			switch (XboxAddressRanges[i].Type) {
			case MemLowVirtual: // Already reserved via virtual_memory_placeholder
				continue;
			case MemTiled: // Even though it can't be reserved, MapViewOfFileEx to this range still works!?
				continue;
			case DeviceUSB1: // Won't be emulated for a long while
				continue;
			case DeviceMCPX: // Can safely be ignored
				continue;
			case DeviceFlash: // Losing mirror 4 is acceptable - the 3 others work just fine
				if (XboxAddressRanges[i].Start == 0xFFC00000)
					continue;
			}

			// If we get here, emulation lacks important address ranges; Don't launch
			OutputDebugString("Required address range couldn't be reserved!");
			return ERROR_NOT_ENOUGH_MEMORY;
		}
	}

	// Only after the required memory ranges are reserved, load our emulation DLL
	HMODULE hEmulationDLL = LoadLibrary("Cxbx-Emulator.dll");
	if (!hEmulationDLL) {
		DWORD err = GetLastError();

		// Translate ErrorCode to String.
		LPTSTR Error = 0;
		if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			err,
			0,
			(LPTSTR)&Error,
			0,
			NULL) == 0)
		{
			// Failed in translating.
		}

		// Free the buffer.
		if (Error)
		{
			::LocalFree(Error);
			Error = 0;
		}

		return ERROR_RESOURCE_NOT_FOUND;
	}

	// Find the main emulation function in our DLL
	typedef DWORD (WINAPI * Emulate_t)();
	Emulate_t pfnEmulate = (Emulate_t)GetProcAddress(hEmulationDLL, "Emulate");
	if(!pfnEmulate)
		return ERROR_RESOURCE_NOT_FOUND;

	// Call the main emulation function in our DLL, passing in the results
	// of the address range reservations
	return pfnEmulate(); // TODO : Pass along the command line
}