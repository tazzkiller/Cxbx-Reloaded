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

	PPUSH m_pCPUTime = NULL;
}

/*
CDevice::Init() // initialization sequence :
- MmAllocateContiguousMemoryEx() // 96 bytes; a semaphore of 32 bytes, and 4 notifiers of 16 bytes
- InitializePushBuffer()
-- MmAllocateContiguousMemoryEx() // 512 KiB; pushbuffer, ends up in 0x800012000
- CMiniport::InitHardware()
-- KeInitializeDpc()
-- KeInitializeEvent() // busy event
-- KeInitializeEvent() // vertical blank event
-- MapRegisters()
--- NV2A_Read32(NV_PBUS_PCI_NV_1) + NV2A_Write32(NV_PBUS_PCI_NV_1) // Disable BUS IRQ
--- NV2A_Read32(NV_PCRTC_INTR_EN_0) + NV2A_Write32(NV_PCRTC_INTR_EN_0) // Disable CRTC IRQ (vblank?)
--- NV2A_Write32(NV_PTIMER_INTR_EN_0)
-- GetGeneralInfo()
--- NV2A_Read32(NV_PBUS_PCI_NV_0) // Returns VENDOR_ID
--- NV2A_Read32(NV_PBUS_PCI_NV_2) // Returns REVISION_ID
--- NV2A_Read32(NV_PFB_CSTATUS)
-- HalGetInterruptVector() + KeInitializeInterrupt() + KeConnectInterrupt() // Calls HalEnableSystemInterrupt (unimplemented)
-- HalRegisterShutdownNotification() // unimplemented
-- InitEngines()
--- NV2A_Write32(NV_PBUS_PCI_NV_12) // ROM_BASE
--- NV2A_Read32(NV_PBUS_PCI_NV_3) // LATENCY_TIMER
--- HalMcControlInit()
---- NV2A_Read32(NV_PMC_ENABLE) + NV2A_Write32(NV_PMC_ENABLE) // Disable PMC (IRQ?)
---- NV2A_Read32(NV_PMC_INTR_EN_0) // Returns PMC interrupt 0 enabled flag
---- NV2A_Write32(NV_PMC_ENABLE) // Enable PMC (IRQ?)
---- NV2A_Read32(NV_PRAMDAC_MPLL_COEFF)
---- NV2A_Read32(0x00000505) // Unknown PRAMDAC Address; = NV_PRAMDAC_MPLL_COEFF + 1
---- NV2A_Read32(NV_PRAMDAC_MPLL_COEFF)
---- NV2A_Read32(NV_PRAMDAC_VPLL_COEFF)
---- NV2A_Read32(0x00000509) // Unknown PRAMDAC Address; = NV_PRAMDAC_VPLL_COEFF + 1
---- NV2A_Read32(NV_PRAMDAC_VPLL_COEFF)
---- NV2A_Read32(NV_PRAMDAC_NVPLL_COEFF)
---- NV2A_Read32(0x00000501) // Unknown PRAMDAC Address; = NV_PRAMDAC_NVPLL_COEFF + 1
---- NV2A_Read32(NV_PRAMDAC_NVPLL_COEFF)
--- NV2A_Read32(NV_PTIMER_NUMERATOR)
--- NV2A_Read32(NV_PTIMER_DENOMINATOR)
--- NV2A_Read32(NV_PTIMER_ALARM_0)
--- HalFbControlInit()
---- NV2A_Read32(NV_PFB_CFG0)
---- NV2A_Read32(NV_PFB_CFG1)
---- NV2A_Read32(NV_PBUS_FBIO_RAM)
---- MmClaimGpuInstanceMemory() // Reserves 20 KiB 
---- NV2A_Write32(NV_PFIFO_RAMHT)
---- NV2A_Write32(NV_PFIFO_RAMFC)
---- NV2A_Read32(NV_PFB_NVM) + NV2A_Write32(NV_PFB_NVM)
---- NV2A_Write32(PRAMIN) // 5120 times, for 20 KiB of writes
--- HalDacControlInit()
----- NV2A_Read32(NV_PRMCIO_CRX__COLOR) + NV2A_Write32(NV_PRMCIO_CRX__COLOR) // six times
----- NV2A_Read32(NV_PVIDEO_DEBUG_2) + NV2A_Write32(NV_PVIDEO_DEBUG_2) // twice
----- NV2A_Read32(NV_PVIDEO_DEBUG_3) + NV2A_Write32(NV_PVIDEO_DEBUG_3) // twice
----- NV2A_Read32(NV_PRMCIO_CRX__COLOR) + NV2A_Write32(NV_PRMCIO_CRX__COLOR) // six times
----- NV2A_Read32(NV_PCRTC_CONFIG) + NV2A_Write32(NV_PCRTC_CONFIG)
----- NV2A_Read32(NV_PRMCIO_CRX__COLOR) + NV2A_Write32(NV_PRMCIO_CRX__COLOR) // twice
--- InitGammaRamp()
--- HalVideoControlInit()
---- NV2A_Write32(NV_PVIDEO_LUMINANCE(0))
---- NV2A_Write32(NV_PVIDEO_CHROMINANCE(0))
---- NV2A_Write32(NV_PVIDEO_DS_DX(0))
---- NV2A_Write32(NV_PVIDEO_DT_DY(0))
---- NV2A_Write32(NV_PVIDEO_POINT_IN(0))
---- NV2A_Write32(NV_PVIDEO_SIZE_IN(0))
---- NV2A_Write32(NV_PVIDEO_LUMINANCE(1))
---- NV2A_Write32(NV_PVIDEO_CHROMINANCE(1))
---- NV2A_Write32(NV_PVIDEO_DS_DX(1))
---- NV2A_Write32(NV_PVIDEO_DT_DY(1))
---- NV2A_Write32(NV_PVIDEO_POINT_IN(1))
---- NV2A_Write32(NV_PVIDEO_SIZE_IN(1))
--- HalGrControlInit()
---- NV2A_Write32(NV_PGRAPH_CHANNEL_CTX_TABLE)
// TODO : Trace
---- NV2A_Write32(NV_PFIFO_CACHE1_PUT)
---- NV2A_Write32(NV_PFIFO_CACHE1_GET)
---- NV2A_Write32(NV_PFIFO_CACHE1_DMA_PUT)
---- NV2A_Write32(NV_PFIFO_CACHE1_DMA_GET)
---- NV2A_Write32(NV_PFIFO_CACHE0_HASH)
---- NV2A_Write32(NV_PFIFO_CACHE1_HASH)
---- NV2A_Write32(NV_PFIFO_MODE)
---- NV2A_Write32(NV_PFIFO_DMA)
---- NV2A_Write32(NV_PFIFO_SIZE)
---- NV2A_Write32(NV_PFIFO_CACHE1_DMA_STATE)
---- NV2A_Write32(NV_PFIFO_RUNOUT_PUT_ADDRESS)
---- NV2A_Write32(NV_PFIFO_RUNOUT_GET_ADDRESS)
--- HalFifoControlInit()
-- LoadEngines()
--- NV2A_Read32(NV_PBUS_PCI_NV_19)
--- NV2A_Write32(NV_PBUS_PCI_NV_19)
--- NV2A_Read32(NV_PBUS_PCI_NV_19)
--- NV2A_Read32(NV_PMC_ENABLE)
--- HalDacLoad()
--- KeQuerySystemTime()
--- NV2A_Write32(NV_PTIMER_TIME_0)
--- NV2A_Write32(NV_PTIMER_TIME_1)
--- NV2A_Write32(NV_PGRAPH_FIFO)
--- HalGrControlLoad()
--- NV2A_Write32(NV_PGRAPH_INTR)
--- NV2A_Write32(NV_PGRAPH_INTR_EN)
--- HalFifoControlLoad()
--- NV2A_Write32(NV_PFIFO_INTR_0)
--- NV2A_Write32(NV_PFIFO_INTR_EN_0)
-CMiniport::CreateCtxDmaObject(11 times, does a lot of nv2a access, 8th we need for notification of semaphore address > m_pGPUTime)
-CMiniport::InitDMAChannel(once or none)
-- HalFifoAllocDMA()
-CMiniport::BindToChannel(11 times)
-CMiniport::CreateGrObject(11 times)
-KickOff()
-HwGet()
-BusyLoop()
-InitializeHardware()
-InitializeFrameBuffers()
-CMiniport::SetVideoMode()

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
	DbgPrintf("NV2A : GPU RegisterBase resides at 0x%.08x\n", GPURegisterBase);

	DbgPrintf("NV2A : Creating flush event\n");
	ghNV2AFlushEvent = CreateEvent(
		NULL,                   // default security attributes
		TRUE,                   // manual-reset event
		FALSE,                  // initial state is nonsignaled
		TEXT("NV2AFlushEvent")  // object name
	);

	// Create our DMA pushbuffer 'handling' thread :
	DbgPrintf("NV2A : Launching DMA handler thread\n");
	::DWORD dwThreadId = 0;
	::HANDLE hThread = CreateThread(nullptr, 0, XTL::EmuThreadHandleNV2ADMA, nullptr, 0, &dwThreadId);
	// Make sure callbacks run on the same core as the one that runs Xbox1 code :
	SetThreadAffinityMask(hThread, g_CPUXbox);
	// We set the priority of this thread a bit higher, to assure reliable timing :
	SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);

	RETURN(true);
}

void XTL::CxbxLocateCpuTime()
{
	if (m_pCPUTime == NULL) {
		// Find the address of the CPU Time variable (this is a temporary solution, until we have
		// a technique to read members from all g_pDevice (D3DDevice) versions in a generic way);

		// Walk through a few members of the D3D device struct :
		m_pCPUTime = (PPUSH)(*((xbaddr *)XTL::Xbox_D3D__Device));
		DbgPrintf("CxbxLocateCpuTime : Searching for m_pCPUTime (residing at 0x%0.8x) in Xbox_D3D__Device from 0x%0.8x\n", m_pGPUTime, m_pCPUTime);

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
	}

	DbgPrintf("CxbxLocateCpuTime : m_pCPUTime resides at 0x%0.8x\n", m_pCPUTime);
}

#if 0 // Patch not needed - NV2A RAMIN handlers are sufficient
// The NV2A DMA channel can be emulated in a few different ways :
// 1: Allocate a fake DMA channel :
//g_NV2ADMAChannel = (Nv2AControlDma*)CxbxCalloc(1, sizeof(Nv2AControlDma));
// 2: Use the actual address
//g_NV2ADMAChannel = (Nv2AControlDma*)((ULONG)XTL::GPURegisterBase + NV_USER_ADDR);
// 3: Use a global variable
//g_pNV2ADMAChannel = &g_NV2ADMAChannel; // <- this is a simple approach, but not accurate
// 4: Let CMiniport::CreateCtxDmaObject run unpatched - it'll initialize and write it to NV2A_PRAMIN+0x6C
//DbgPrintf("NV2A DMA channel is at 0x%0.8x\n", g_pNV2ADMAChannel);

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
#endif

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
	*ppChannel = g_pNV2ADMAChannel;

	return TRUE;
}