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
// *   CxbxKrnl->Miniport.cpp
// *
// *  This file is part of the Cxbx-Reloaded project, a fork of Cxbx.
// *
// *  Cxbx-Reloaded is free software; you can redistribute it
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
#define _CXBXKRNL_INTERNAL
#define _XBOXKRNL_DEFEXTRN_

// prevent name collisions
namespace xboxkrnl
{
#include <xboxkrnl/xboxkrnl.h>
};

#include "CxbxUtil.h"
#include "CxbxKrnl.h"
#include "Emu.h"
#include "EmuAlloc.h"
#include "MemoryManager.h"
#include "EmuXTL.h"
#include "Logging.h"
#include "EmuD3D8Logging.h"

namespace XTL {
	void *GPURegisterBase = NULL; // + NV2A_PFB_WC_CACHE = pNV2AWorkTrigger (see EmuThreadHandleNV2ADMA)

	volatile xbaddr *m_pCPUTime = NULL;
	volatile xbaddr *m_pGPUTime = NULL; // See Dxbx XTL_ EmuExecutePushBufferRaw
}

/*
CMiniport initialization sequence :

InitHardware()
CreateCtxDmaObject(11 times, does a lot of nv2a access, 8th we need for notification of semaphore address > m_pGPUTime)
InitDMAChannel(once)
BindToChannel(11 times)
CreateGrObject(11 times)

BusyLoop();

00024AC0 51                   push        ecx
00024AC1 C7 44 24 00 90 01 00 00 mov         dword ptr [esp],190h
00024AC9 8D A4 24 00 00 00 00 lea         esp,[esp]
00024AD0 8B 44 24 00          mov         eax,dword ptr [esp]
00024AD4 48                   dec         eax
00024AD5 89 44 24 00          mov         dword ptr [esp],eax
00024AD9 75 F5                jne         virtual_memory_placeholder+13AD0h (024AD0h)
00024ADB 59                   pop         ecx
00024ADC C3                   ret


?BusyLoop@D3D@@YGXXZ 3911 d3d8 0020 51C7442400900100008DA424000000008B442400488944240075F559C3909090 00 0000
?BusyLoop@D3D@@YGXXZ 4361 d3d8 0020 51C7442400900100008DA424000000008B442400488944240075F559C3909090 00 0000
?BusyLoop@D3D@@YGXXZ 4627 d3d8 0020 51C7442400900100008DA424000000008B442400488944240075F559C3909090 00 0000
?BusyLoop@D3D@@YGXXZ 5344 d3d8 0020 51C7442400900100008DA424000000008B442400488944240075F559C3909090 00 0000
?BusyLoop@D3D@@YGXXZ 5558 d3d8 0013 51C70424900100008B04244889042475F759C3.......................... 00 0000
?BusyLoop@D3D@@YGXXZ 5659 d3d8 0013 51C70424900100008B04244889042475F759C3.......................... 00 0000
?BusyLoop@D3D@@YGXXZ 5788 d3d8 0013 51C70424900100008B04244889042475F759C3.......................... 00 0000
?BusyLoop@D3D@@YGXXZ 5849 d3d8 0013 51C70424900100008B04244889042475F759C3.......................... 00 0000
?BusyLoop@D3D@@YGXXZ 5933 d3d8 0013 51C70424900100008B04244889042475F759C3.......................... 00 0000

00000000 51                    push  ecx
00000001 c7 04 24 90 01 00 00  mov   DWORD PTR [esp],0x190
loc_00000008:
00000008 8b 04 24              mov   eax,DWORD PTR [esp]
0000000b 48                    dec   eax
0000000c 89 04 24              mov   DWORD PTR [esp],eax
0000000f 75 f7                 jne   loc_00000008
00000011 59                    pop   ecx
00000012 c3                    ret
*/

// ******************************************************************
// * patch: CMiniport_InitHardware
// ******************************************************************
// Although EmuNV2A catches all NV2A accesses (producing a lot of detailed logging),
// this patch on CMiniport::InitHardware skips all that, and just initialiazes the
// CMiniport::m_RegisterBase so there's some memory to play with.
BOOL __fastcall XTL::EMUPATCH(CMiniport_InitHardware)
(
	PVOID This, // CMiniport*
	void * _EDX // __thiscall simulation
)
{
	FUNC_EXPORTS

	LOG_FUNC_ONE_ARG(This);

#if 0 // Old Dxbx approach :
	// CMiniport::MapRegisters() sets m_RegisterBase (the first member of CMiniport) to 0xFD000000 (NV2A_ADDR)
	// That address isn't memory-mapped, and thus would cause lots of exceptions,
	// so instead we allocate a buffer in host virtual memory
	GPURegisterBase = g_MemoryManager.AllocateZeroed(1, 2 * 1024 * 1024); // PatrickvL : I've seen RegisterBase offsets up to $00100410 so 2 MB should suffice
#else
	// New Cxbx-Reloaded approach :
	// Just use the actual hardware GPU address, as we have EmuX86 calling
	// into EmuNV2A_Read and EmuNV2A_Write handlers.
	GPURegisterBase = (void *)NV2A_ADDR;
#endif

	// Put the GPURegisterBase value in the CMiniport::m_RegisterBase field (it's the first field in this object) :
	*((void **)This) = GPURegisterBase;
	DbgPrintf("GPU RegisterBase resides at 0x%.08x\n", GPURegisterBase);

	RETURN(true);
}

Nv2AControlDma *CxbxNV2ADMAChannel()
{
	using namespace XTL; // for XTL::m_pCPUTime, XTL::m_pGPUTime

	if (m_pCPUTime == nullptr) {

		// The NV2A DMA channel can be emulated in 3 ways :
		// 1: Allocate a fake DMA channel :
		//g_NV2ADMAChannel = (Nv2AControlDma*)CxbxCalloc(1, sizeof(Nv2AControlDma));
		// 2: Use the actual address
		//g_NV2ADMAChannel = (Nv2AControlDma*)((ULONG)XTL::GPURegisterBase + NV_USER_ADDR);
		// 3: Use a global variable
		//NV2ADMAChannel g_NV2ADMAChannel; // <- this looks like the simplest approach
		DbgPrintf("CxbxNV2ADMAChannel : NV2A DMA channel is at 0x%0.8x\n", &g_NV2ADMAChannel);

		// Create our DMA pushbuffer 'handling' thread :
		DbgPrintf("CxbxNV2ADMAChannel : Launching NV2A DMA handler thread\n");
		DWORD dwThreadId = 0;
		HANDLE hThread = CreateThread(nullptr, 0, XTL::EmuThreadHandleNV2ADMA, nullptr, 0, &dwThreadId);
		// Make sure callbacks run on the same core as the one that runs Xbox1 code :
		SetThreadAffinityMask(hThread, g_CPUXbox);
		// We set the priority of this thread a bit higher, to assure reliable timing :
		SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);

		// Also, find the address of the CPU Time variable (this is a temporary solution, until we have
		// a technique to read members from all g_pDevice (D3DDevice) versions in a generic way);

		// Walk through a few members of the D3D device struct :
		m_pCPUTime = (xbaddr*)(*((xbaddr *)XTL::Xbox_D3D__Device));
		DbgPrintf("CxbxNV2ADMAChannel : Searching for m_pCPUTime from 0x%0.8x (m_pGPUTime is at 0x%0.8x)\n", m_pCPUTime, m_pGPUTime);

		for (int i = 0; i <= 32; i++) {
			if (i == 32)
				CxbxKrnlCleanup("m_pCPUTime not found!");

			// Look for the offset of the GPUTime pointer inside the D3D g_pDevice struct :
			if (*m_pCPUTime == (xbaddr)m_pGPUTime) {
				// The CPU time variable is located right before the GPUTime pointer :
				m_pCPUTime--;
				break;
			}

			m_pCPUTime++;
		}

		DbgPrintf("CxbxNV2ADMAChannel : Found m_pCPUTime at 0x%0.8x\n", m_pCPUTime);
	}

	return &g_NV2ADMAChannel;
}

// ******************************************************************
// * patch: CMiniport_CreateCtxDmaObject
// ******************************************************************
BOOL __fastcall XTL::EMUPATCH(CMiniport_CreateCtxDmaObject)
(
	PVOID This,
	void * _EDX, // __thiscall simulation
	ULONG Dma,
	ULONG ClassNum,
	PVOID Base,
	ULONG Limit,
	PVOID Object
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(This)
		LOG_FUNC_ARG(Dma)
		LOG_FUNC_ARG(ClassNum)
		LOG_FUNC_ARG(Base)
		LOG_FUNC_ARG(Limit)
		LOG_FUNC_ARG(Object)
		LOG_FUNC_END;

	switch (Dma) {
	case 8: { // = notification of semaphore address
		// Remember where the semaphore (starting with a GPU Time DWORD) was allocated
		m_pGPUTime = (xbaddr*)Base;
		DbgPrintf("Registered m_pGPUTime at 0x%0.8x\n", m_pGPUTime);
		break;
	}
	case 6: { // = pusher
		CxbxNV2ADMAChannel();
		break;
	}
	default: {
		LOG_UNIMPLEMENTED();
	}
	}

	return TRUE;
}

// ******************************************************************
// * patch: CMiniport_InitDMAChannel
// ******************************************************************
BOOL __fastcall XTL::EMUPATCH(CMiniport_InitDMAChannel)
(
	PVOID This,
	void * _EDX, // __thiscall simulation
	ULONG Class,
	OBJECTINFO *ErrorContext,
	OBJECTINFO *DataContext,
	ULONG Offset,
	Nv2AControlDma **ppChannel
)
{
	FUNC_EXPORTS

	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(This)
		LOG_FUNC_ARG(Class)
		LOG_FUNC_ARG(ErrorContext)
		LOG_FUNC_ARG(DataContext)
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(ppChannel)
		LOG_FUNC_END;

	// Return our emulated DMA channel :
	*ppChannel = CxbxNV2ADMAChannel();

	return TRUE;
}