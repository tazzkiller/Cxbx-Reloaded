// Based on https://stackoverflow.com/questions/39217241/visual-studio-2015-compile-c-c-without-a-runtime-library/39220245#39220245

#include <Windows.h> // For DWORD, CALLBACK, VirtualAlloc, LPVOID, SIZE_T, HMODULE 

#include "..\Common\XboxAddressRanges.h"

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

DWORD CALLBACK rawMain()
{
	// Reserve space for the result of reserving all address ranges
	LPVOID VirtualAllocResults[ARRAY_SIZE(XboxAddressRanges)];

//Sleep(1000 * 60); // DEBUG : One minute to take an initial snapshot using SysInternals VMMap

	// Loop over all Xbox address ranges
	for (int i = 0; i < ARRAY_SIZE(XboxAddressRanges); i++) {
		// Low memory needs special treatment
		xbaddr Start = XboxAddressRanges[i].Start;
		if (XboxAddressRanges[i].Type == MemLowVirtual)
			Start = 0x00040000; // TODO : Read this from image header

		// Reserve this address range, and store the result
		VirtualAllocResults[i] = VirtualAlloc(
			(LPVOID)Start,
			(SIZE_T)XboxAddressRanges[i].Size,
			MEM_RESERVE,
			XboxAddressRangeTypeToVirtualAllocPageProtectionFlags(XboxAddressRanges[i].Type)
		);
	}

	// Note : Ranges that failed under Windows 7 Wow64 :
	//
	// { DeviceUSB1, 0xFED08000, KB(4) }, // .. 0xFED09000
	// { DeviceFlash,   0xFFC00000, MB(4) }, // .. 0xFFFFFFFF (Flash mirror 4) - Will probably fail reservation
	// { DeviceMCPX,    0xFFFFFE00,    512 }, // .. 0xFFFFFFFF (not Chihiro, Xbox - if enabled)
	//
	// .. none of which are an issue for now.
	// 
	// More problematic however, is this failure :
	//
	// { MemLowVirtual, 0x00000000, MB(128) }, // .. 0x08000000 (Retail Xbox uses 64 MB)
	//
	// SysInternals VMMap shows there are a few 'Unusable' pages in there, a DLL,
	// thread stack, private and shared data, all in the lower 128 MB....
	//
	// TODO : We *really* need to fix that, otherwise the loaded-approach has little added value.

Sleep(1000 * 60 * 5); // DEBUG : Five minutes to look at the result in SysInternals VMMap

	// Only after the required memory ranges are reserved, load our emulation DLL
	HMODULE hEmulationDLL = LoadLibrary(L"Cxbx-Reloaded.DLL");
	if (!hEmulationDLL)
		return ERROR_RESOURCE_NOT_FOUND;

	// Find the main emulation function in our DLL
	typedef DWORD (WINAPI * Emulate_t)(LPVOID *);
	Emulate_t pfnEmulate = (Emulate_t)GetProcAddress(hEmulationDLL, "Emulate");
	if(!pfnEmulate)
		return ERROR_RESOURCE_NOT_FOUND;

	// Call the main emulation function in our DLL
	return pfnEmulate(VirtualAllocResults);
}