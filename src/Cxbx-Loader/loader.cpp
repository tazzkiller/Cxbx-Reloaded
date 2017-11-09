// Based on https://stackoverflow.com/questions/39217241/visual-studio-2015-compile-c-c-without-a-runtime-library/39220245#39220245

#include <Windows.h> // For DWORD, CALLBACK, VirtualAlloc, LPVOID, SIZE_T, HMODULE 

#include "..\Common\XboxAddressRanges.h"

DWORD XboxMemoryTypeToPageProtectionFlags(XboxAddressRangeType type)
{
	static DWORD ExecutableMemPageProtection = PAGE_EXECUTE_READWRITE;
	static DWORD MemPageProtection = PAGE_READWRITE;
	static DWORD DevicePageProtection = PAGE_READWRITE; //  TODO : (PAGE_NOACCESS | PAGE_GUARD) doesn't work - why?
	
	switch (type) {
	case MemLowVirtual:
		return ExecutableMemPageProtection;
	case DeviceNV2A:
	case DeviceAPU:
	case DeviceAC97:
	case DeviceUSB0:
	case DeviceUSB1:
	case DeviceNVNet:
	case DeviceFlash:
	case DeviceMCPX:
		return DevicePageProtection;
	}

	// case MemPhysical: // Note : If we ever use a real kernel, 'MemPhysical' would need 
	// ExecutableMemPageProtection, as the kernel is loaded to & runs from this memory region.
	// case MemPageTable:
	// case MemNV2APRAMIN:
	return MemPageProtection;

#if 0 // research if we need to use any of these page protection flags instead :
#define PAGE_NOACCESS          0x01     
#define PAGE_READONLY          0x02     
#define PAGE_READWRITE         0x04     
#define PAGE_WRITECOPY         0x08     
#pragma region Desktop Family           
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) 
#define PAGE_EXECUTE           0x10     
#define PAGE_EXECUTE_READ      0x20     
#define PAGE_EXECUTE_READWRITE 0x40     
#define PAGE_EXECUTE_WRITECOPY 0x80     
#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */ 
#pragma endregion                       
#define PAGE_GUARD            0x100     
#define PAGE_NOCACHE          0x200     
#define PAGE_WRITECOMBINE     0x400     
#define PAGE_REVERT_TO_FILE_MAP     0x80000000     
#define MEM_COMMIT                  0x1000      
#define MEM_RESERVE                 0x2000      
#define MEM_DECOMMIT                0x4000      
#define MEM_RELEASE                 0x8000      
#define MEM_FREE                    0x10000     
#define MEM_PRIVATE                 0x20000     
#define MEM_MAPPED                  0x40000     
#define MEM_RESET                   0x80000     
#define MEM_TOP_DOWN                0x100000    
#define MEM_WRITE_WATCH             0x200000    
#define MEM_PHYSICAL                0x400000    
#define MEM_ROTATE                  0x800000    
#define MEM_DIFFERENT_IMAGE_BASE_OK 0x800000    
#define MEM_RESET_UNDO              0x1000000   
#define MEM_LARGE_PAGES             0x20000000  
#define MEM_4MB_PAGES               0x80000000  
#define SEC_FILE           0x800000     
#define SEC_IMAGE         0x1000000     
#define SEC_PROTECTED_IMAGE  0x2000000  
#define SEC_RESERVE       0x4000000     
#define SEC_COMMIT        0x8000000     
#define SEC_NOCACHE      0x10000000     
#define SEC_WRITECOMBINE 0x40000000     
#define SEC_LARGE_PAGES  0x80000000     
#define SEC_IMAGE_NO_EXECUTE (SEC_IMAGE | SEC_NOCACHE)     
#define MEM_IMAGE         SEC_IMAGE     
#define WRITE_WATCH_FLAG_RESET  0x01     
#define MEM_UNMAP_WITH_TRANSIENT_BOOST  0x01     
#endif
}

#define ARRAY_SIZE(a)                               \
  ((sizeof(a) / sizeof(*(a))) /                     \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

DWORD CALLBACK rawMain()
{
	// Reserve space for the result of reserving all ranges
	LPVOID VirtualAllocResults[ARRAY_SIZE(XboxAddressRanges)];

Sleep(1000 * 60); // DEBUG : One minute to take an initial snapshot using SysInternals VMMap

	// Note: 0 is a marker, indicating the first page above the loader executable itself (0x00010000 + image-size)
	// Loop over all Xbox ranges
	for (int i = 0; i < ARRAY_SIZE(XboxAddressRanges); i++) {
		xbaddr Start = XboxAddressRanges[i].Start;
		if (XboxAddressRanges[i].Type == MemLowVirtual)
			Start = 0x00040000; // TODO : Read this from image header

		// Reserve this memory, and store the result
		VirtualAllocResults[i] = VirtualAlloc(
			(LPVOID)Start,
			(SIZE_T)XboxAddressRanges[i].Size,
			MEM_RESERVE,
			XboxMemoryTypeToPageProtectionFlags(XboxAddressRanges[i].Type)
		);
	}

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