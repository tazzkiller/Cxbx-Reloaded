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
- MmAllocateContiguousMemoryEx() // 96 bytes; a semaphore of 32 bytes, and 4 notifiers of 16 bytes, ends up in 0x800011000
- InitializePushBuffer()
-- MmAllocateContiguousMemoryEx() // 512 KiB; pushbuffer, ends up in 0x800012000
- CMiniport::InitHardware()
-- KeInitializeDpc()
-- KeInitializeEvent() // busy event
-- KeInitializeEvent() // vertical blank event
-- MapRegisters()
--- NV2A_Read32(NV_PBUS_PCI_NV_1) + NV2A_Write32(NV_PBUS_PCI_NV_1) // Disable BUS IRQ
--- NV2A_Write32(NV_PCRTC_INTR_EN_0) // Disable CRTC IRQ (vblank?)
--- NV2A_Write32(NV_PTIMER_INTR_EN_0) // Disable TIMER IRQ
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
-- HalReadPCISpace()
-- HalWritePCISpace()
-- LoadEngines()
--- NV2A_Read32(NV_PBUS_PCI_NV_19)
--- NV2A_Write32(NV_PBUS_PCI_NV_19)
--- NV2A_Read32(NV_PBUS_PCI_NV_19)
--- NV2A_Read32(NV_PMC_ENABLE)
--- EnableInterrupts()
--- HalDacLoad()
--- KeQuerySystemTime() -- invisible, inlined
--- NV2A_Write32(NV_PTIMER_TIME_0)
--- NV2A_Write32(NV_PTIMER_TIME_1)
--- NV2A_Write32(NV_PGRAPH_FIFO)
--- HalGrControlLoad()
---- NV2A_Write32(NV_PGRAPH_DEBUG_0) - NV2A_Write32(NV_PGRAPH_DEBUG_10)
---- NV2A_Write32(NV_PGRAPH_CHANNEL_CTX_TABLE)
---- HalGrIdle()
----- NV2A_Read32(NV_PGRAPH_STATUS)
----- ServiceGrInterrupt()
----- VBlank()
---- NV2A_Read32(NV_PFB_TLIMIT[0..7])
   + NV2A_Write32(NV_PGRAPH_TLIMIT[0..7]) 
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX) 
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
   + NV2A_Read32(NV_PFB_TSIZE[0..7]) 
   + NV2A_Write32(NV_PGRAPH_TSIZE[0..7])
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
   + NV2A_Read32(NV_PFB_TILE[0..7])
   + NV2A_Write32(NV_PGRAPH_TTILE[0..7])
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
---- NV2A_Read32(NV_PFB_ZCOMP[0..7])
   + NV2A_Write32(NV_PGRAPH_ZCOMP[0..7])
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
---- NV2A_Read32(NV_PFB_ZCOMP_OFFSET)
   + NV2A_Write32(NV_PGRAPH_ZCOMP_OFFSET)
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
---- NV2A_Read32(NV_PFB_CFG0)
   + NV2A_Write32(NV_PGRAPH_FBCFG0)
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
---- NV2A_Read32(NV_PFB_CFG1)
   + NV2A_Write32(NV_PGRAPH_FBCFG1)
   + NV2A_Write32(NV_PGRAPH_RDI_INDEX)
   + NV2A_Write32(NV_PGRAPH_RDI_DATA)
---- NV2A_Write32(NV_PGRAPH_CTX_SWITCH1)
---- NV2A_Write32(NV_PGRAPH_CTX_SWITCH2)
---- NV2A_Write32(NV_PGRAPH_CTX_SWITCH3)
---- NV2A_Write32(NV_PGRAPH_CTX_SWITCH4)
---- NV2A_Write32(NV_PGRAPH_CTX_CONTROL)
---- NV2A_Write32(NV_PGRAPH_FFINTFC_ST2)
---- HalGrLoadChannelContext();
----- NV2A_Read32(NV_PGRAPH_INTR) 
----- NV2A_Write32(NV_PGRAPH_CTX_CONTROL)
// TODO : Trace
--- NV2A_Write32(NV_PGRAPH_INTR)
--- NV2A_Write32(NV_PGRAPH_INTR_EN)
--- HalFifoControlLoad()
---- NV2A_Write32(NV_PFIFO_CACHE1_DMA_FETCH)
---- NV2A_Write32(NV_PFIFO_DMA_TIMESLICE)
---- NV2A_Write32(NV_PFIFO_DELAY_0)
---- NV2A_Write32(NV_PFIFO_CACHES)
---- NV2A_Write32(NV_PFIFO_CACHE0_PUSH0)
---- NV2A_Write32(NV_PFIFO_CACHE0_PULL0)
---- NV2A_Write32(NV_PFIFO_CACHE1_PUSH0)
---- NV2A_Write32(NV_PFIFO_CACHE1_PULL0)
---- NV2A_Write32(NV_PFIFO_CACHE1_DMA_PUSH)
---- HalFifoContextSwitch()
----- NV2A_Read32(NV_PFIFO_CACHES)
----- NV2A_Read32(NV_PFIFO_CACHE1_PUSH0)
----- NV2A_Read32(NV_PFIFO_CACHE1_PULL0)
----- NV2A_Write32(NV_PFIFO_CACHES)
----- NV2A_Write32(NV_PFIFO_CACHE1_PUSH0)
----- NV2A_Write32(NV_PFIFO_CACHE1_PULL0)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_PUT)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_GET)
----- NV2A_Read32(NV_PFIFO_CACHE1_REF)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_INSTANCE)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_STATE)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_FETCH)
----- NV2A_Read32(NV_PFIFO_CACHE1_ENGINE)
----- NV2A_Read32(NV_PFIFO_CACHE1_PULL1)
----- NV2A_Read32(NV_PFIFO_CACHE1_ACQUIRE_2)
----- NV2A_Read32(NV_PFIFO_CACHE1_ACQUIRE_1)
----- NV2A_Read32(NV_PFIFO_CACHE1_ACQUIRE_0)
----- NV2A_Read32(NV_PFIFO_CACHE1_SEMAPHORE)
----- NV2A_Read32(NV_PFIFO_CACHE1_DMA_SUBROUTINE)
---- NV2A_Write32(NV_PFIFO_CACHE1_PUT, 0)
---- NV2A_Write32(NV_PFIFO_CACHE1_GET, 0)
---- NV2A_Write32(NV_PFIFO_CACHE1_PULL0)
---- NV2A_Write32(NV_PFIFO_CACHE1_PUSH0)
---- NV2A_Write32(NV_PFIFO_CACHES, 1)
---- NV2A_Write32(NV_PFIFO_CACHES, 0)
--- NV2A_Write32(NV_PFIFO_INTR_0)
--- NV2A_Write32(NV_PFIFO_INTR_EN_0)
// TODO : Debug, see why EmuX86 starts reading from 0x00000001
-- DumpClocks() DEBUG builds only
- CMiniport::CreateCtxDmaObject(11 times, does a lot of nv2a access, 8th we need for notification of semaphore address > m_pGPUTime)
- CMiniport::InitDMAChannel(once or none)
-- HalFifoAllocDMA()
- CMiniport::BindToChannel(11 times)
- CMiniport::CreateGrObject(11 times)
- KickOff()
- HwGet()
- BusyLoop()
- InitializeHardware()
- InitializeFrameBuffers()
- CMiniport::SetVideoMode()

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

//	FUNC_EXPORTS

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

	CxbxInitializeNV2ADMA();

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

#if 1 // Patch not needed - NV2A RAMIN handlers are sufficient
// The NV2A DMA channel can be emulated in a few different ways :
// 1: Allocate a fake DMA channel :
//g_NV2ADMAChannel = (Nv2AControlDma*)CxbxCalloc(1, sizeof(Nv2AControlDma));
// 2: Use a fixed address (simple but not accurate)
//g_NV2ADMAChannel = (Nv2AControlDma*)((ULONG)XTL::GPURegisterBase + NV_USER_ADDR);
// 3: Use a global variable (this is a simple approach, but not accurate)
//g_pNV2ADMAChannel = &g_NV2ADMAChannel;
// 4: Let CMiniport::CreateCtxDmaObject run unpatched - it'll initialize and write it to NV2A_PRAMIN+0x6C
// DEVICE_WRITE32(PRAMIN) case NV_PRAMIN_DMA_LIMIT(0) : 
// DWORD Address = DEVICE_REG32_ADDR(pramin, NV_PRAMIN_DMA_ADDRESS(DMASlot));
// g_pNV2ADMAChannel = (Nv2AControlDma*)((Address & ~3) | MM_SYSTEM_PHYSICAL_MAP); // 0x8000000

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
		LOG_FUNC_ARG(This) // This shouldn't be 0x0000001 !
		LOG_FUNC_ARG(Dma)
		LOG_FUNC_ARG(ClassNum)
		LOG_FUNC_ARG(Base)
		LOG_FUNC_ARG(Limit)
		LOG_FUNC_ARG(Object)
		LOG_FUNC_END;

	CxbxPopupMessage("This shouldn't be 0x00000001!"); // TODO : Fix ESI - gets reset somewhere, suspect stack-misalignment
/* CreateDevice tutorial :

Direct3D_CreateDevice
00019000 A1 D4 88 02 00       mov         eax,dword ptr ds:[000288D4h]
00019005 85 C0                test        eax,eax
00019007 75 0A                jne         virtual_memory_placeholder+8013h (019013h)
00019009 C7 05 D4 88 02 00 00 00 08 00 mov         dword ptr ds:[288D4h],80000h
00019013 A1 D0 88 02 00       mov         eax,dword ptr ds:[000288D0h]
00019018 85 C0                test        eax,eax
0001901A 75 0A                jne         virtual_memory_placeholder+8026h (019026h)
0001901C C7 05 D0 88 02 00 00 80 00 00 mov         dword ptr ds:[288D0h],8000h
00019026 8B 44 24 10          mov         eax,dword ptr [esp+10h]
0001902A 8B 4C 24 14          mov         ecx,dword ptr [esp+14h]
0001902E 56                   push        esi
0001902F 8B 35 A8 5C 02 00    mov         esi,dword ptr ds:[25CA8h]
00019035 57                   push        edi
00019036 83 E0 10             and         eax,10h
00019039 BF A0 5C 02 00       mov         edi,25CA0h
0001903E 0B F0                or          esi,eax
00019040 51                   push        ecx
00019041 8B CF                mov         ecx,edi
00019043 89 3D E8 88 02 00    mov         dword ptr ds:[288E8h],edi
00019049 C7 05 A0 61 02 00 01 00 00 00 mov         dword ptr ds:[261A0h],1
00019053 89 35 A8 5C 02 00    mov         dword ptr ds:[25CA8h],esi   // Hier al 0
00019059 E8 52 19 00 00       call        virtual_memory_placeholder+99B0h (01A9B0h)  // CDevice::Init
0001905E 8B F0                mov         esi,eax
00019060 85 F6                test        esi,esi
00019062 7D 2F                jge         virtual_memory_placeholder+8093h (019093h)
00019064 8B CF                mov         ecx,edi
00019066 E8 75 1E 00 00       call        virtual_memory_placeholder+9EE0h (01AEE0h)  // Device->UnInit();
0001906B 8B 44 24 20          mov         eax,dword ptr [esp+20h]
0001906F 85 C0                test        eax,eax
00019071 74 06                je          virtual_memory_placeholder+8079h (019079h)
00019073 C7 00 00 00 00 00    mov         dword ptr [eax],0
00019079 33 C0                xor         eax,eax
0001907B B9 0C 0B 00 00       mov         ecx,0B0Ch
00019080 F3 AB                rep stos    dword ptr es:[edi]  // ZeroMemory
00019082 5F                   pop         edi
00019083 8B C6                mov         eax,esi
00019085 C7 05 E8 88 02 00 00 00 00 00 mov         dword ptr ds:[288E8h],0
0001908F 5E                   pop         esi
00019090 C2 18 00             ret         18h
00019093 8B 44 24 20          mov         eax,dword ptr [esp+20h]
00019097 85 C0                test        eax,eax
00019099 74 02                je          virtual_memory_placeholder+809Dh (01909Dh)
0001909B 89 38                mov         dword ptr [eax],edi
0001909D 5F                   pop         edi
0001909E 33 C0                xor         eax,eax
000190A0 5E                   pop         esi
000190A1 C2 18 00             ret         18h

CDevice::Init
0001A9B0 81 EC 68 01 00 00    sub         esp,168h
0001A9B6 6A 04                push        4
0001A9B8 6A 00                push        0
0001A9BA 68 FF FF FF 07       push        7FFFFFFh
0001A9BF 6A 00                push        0
0001A9C1 6A 60                push        60h
0001A9C3 89 4C 24 14          mov         dword ptr [esp+14h],ecx
0001A9C7 FF 15 A0 91 02 00    call        dword ptr ds:[291A0h]  MmAllocateContiguousMemoryEx
0001A9CD 8B 4C 24 00          mov         ecx,dword ptr [esp]
0001A9D1 89 81 20 2C 00 00    mov         dword ptr [ecx+2C20h],eax
0001A9D7 8B 4C 24 00          mov         ecx,dword ptr [esp]
0001A9DB 8B 81 20 2C 00 00    mov         eax,dword ptr [ecx+2C20h]
0001A9E1 85 C0                test        eax,eax
0001A9E3 75 0E                jne         virtual_memory_placeholder+99F3h (01A9F3h)   if (!m_pCachedContiguousMemoryBase)
0001A9E5 B8 0E 00 07 80       mov         eax,8007000Eh  								return E_OUTOFMEMORY;
0001A9EA 81 C4 68 01 00 00    add         esp,168h
0001A9F0 C2 04 00             ret         4
0001A9F3 89 41 34             mov         dword ptr [ecx+34h],eax
0001A9F6 8B 44 24 00          mov         eax,dword ptr [esp]
0001A9FA 8B 50 34             mov         edx,dword ptr [eax+34h]
0001A9FD 83 C2 20             add         edx,20h
0001AA00 89 90 28 2C 00 00    mov         dword ptr [eax+2C28h],edx
0001AA06 8B 44 24 00          mov         eax,dword ptr [esp]
0001AA0A 8B 88 28 2C 00 00    mov         ecx,dword ptr [eax+2C28h]
0001AA10 83 C1 20             add         ecx,20h
0001AA13 89 88 24 2C 00 00    mov         dword ptr [eax+2C24h],ecx
0001AA19 8B 54 24 00          mov         edx,dword ptr [esp]
0001AA1D 56                   push        esi
0001AA1E 57                   push        edi
0001AA1F 8B BA 28 2C 00 00    mov         edi,dword ptr [edx+2C28h]
0001AA25 33 C0                xor         eax,eax
0001AA27 B9 10 00 00 00       mov         ecx,10h
0001AA2C F3 AB                rep stos    dword ptr es:[edi]
0001AA2E 8B 4C 24 08          mov         ecx,dword ptr [esp+8]
0001AA32 E8 F9 27 00 00       call        virtual_memory_placeholder+0C230h (01D230h)   InitializePushBuffer();
0001AA37 85 C0                test        eax,eax
0001AA39 0F 8C 94 04 00 00    jl          virtual_memory_placeholder+9ED3h (01AED3h)
0001AA3F 8B 15 F4 88 02 00    mov         edx,dword ptr ds:[288F4h]
0001AA45 8B 74 24 08          mov         esi,dword ptr [esp+8]  // = esi, pushed at 0001AA1D
0001AA49 81 CA 7F 7F 00 00    or          edx,7F7Fh
0001AA4F 81 C6 C0 23 00 00    add         esi,23C0h
0001AA55 8B CE                mov         ecx,esi
0001AA57 89 15 F4 88 02 00    mov         dword ptr ds:[288F4h],edx   // Hier is esi nog goed

EAX = 00000000 EBX = 00000000 ECX = 00028060 EDX = 00007F7F ESI = 00028060 EDI = 80011060 EIP = 0001AA5D ESP = 0948FC9C EBP = 0948FED0 EFL = 00000206

0001AA5D E8 44 68 00 00       call        virtual_memory_placeholder+102A6h (0212A6h)  InitHardware(); // geeft exceptions

EAX = 00000001 EBX = 80011060 ECX = 00000000 EDX = 00000000 ESI = 00000001 EDI = 000281C4 EIP = 0001AA62 ESP = 0948FC9C EBP = 0948FED0 EFL = 00000213

0001AA62 8D 84 24 B0 00 00 00 lea         eax,[esp+0B0h]   // Na exception handling, esi = 0!
0001AA69 50                   push        eax
0001AA6A 68 FF AF FF 07       push        7FFAFFFh
0001AA6F 6A 00                push        0
0001AA71 6A 3D                push        3Dh
0001AA73 6A 03                push        3
0001AA75 8B CE                mov         ecx,esi   // esi = 1 -> NO GOOD
0001AA77 E8 5F 65 00 00       call        virtual_memory_placeholder+0FFDBh (020FDBh)  CreateCtxDmaObject(


InitHardware
EAX = 00000000 EBX = 00000000 ECX = 00028060 EDX = 00007F7F ESI = 00028060 EDI = 80011060 EIP = 000212A6 ESP = 0948FC98 EBP = 0948FED0 EFL = 00000206
000212A6 55                   push        ebp
000212A7 8B EC                mov         ebp,esp
000212A9 83 EC 10             sub         esp,10h
000212AC 53                   push        ebx
EAX = 00000000 EBX = 00000000 ECX = 00028060 EDX = 00007F7F ESI = 00028060 EDI = 80011060 EIP = 000212AD ESP = 0988FC80 EBP = 0988FC94 EFL = 00000206
000212AD 56                   push        esi
EAX = 00000000 EBX = 00000000 ECX = 00028060 EDX = 00007F7F ESI = 00028060 EDI = 80011060 EIP = 000212AE ESP = 0948FC7C EBP = 0948FC94 EFL = 00000206
000212AE 8B F1                mov         esi,ecx
000212B0 56                   push        esi
EAX = 00000000 EBX = 00000000 ECX = 00028060 EDX = 00007F7F ESI = 00028060 EDI = 80011060 EIP = 000212B1 ESP = 0948FC78 EBP = 0948FC94 EFL = 00000206
ESP 0948FC78 : 60 80 02 00 60 80 02 00
ESP 0958FC78 : 60 80 02 00 00 00 00 00
000212B1 68 60 22 02 00       push        22260h
000212B6 8D 86 84 00 00 00    lea         eax,[esi+84h]
000212BC 50                   push        eax
000212BD FF 15 80 91 02 00    call        dword ptr ds:[29180h]
000212C3 80 A6 A4 01 00 00 00 and         byte ptr [esi+1A4h],0
000212CA 80 A6 94 01 00 00 00 and         byte ptr [esi+194h],0
000212D1 8D 86 AC 01 00 00    lea         eax,[esi+1ACh]
000212D7 33 DB                xor         ebx,ebx
000212D9 89 86 B0 01 00 00    mov         dword ptr [esi+1B0h],eax
000212DF 89 00                mov         dword ptr [eax],eax
000212E1 8D 86 9C 01 00 00    lea         eax,[esi+19Ch]
000212E7 43                   inc         ebx
000212E8 8B CE                mov         ecx,esi
000212EA C6 86 A6 01 00 00 04 mov         byte ptr [esi+1A6h],4
000212F1 89 9E A8 01 00 00    mov         dword ptr [esi+1A8h],ebx
000212F7 C6 86 96 01 00 00 04 mov         byte ptr [esi+196h],4
000212FE 89 9E 98 01 00 00    mov         dword ptr [esi+198h],ebx
00021304 89 86 A0 01 00 00    mov         dword ptr [esi+1A0h],eax
0002130A 89 00                mov         dword ptr [eax],eax
0002130C E8 E9 FA FF FF       call        virtual_memory_placeholder+0FDFAh (020DFAh)  // MapRegisters
ESP 0948FC78 : 11 13 02 00 60 80 02 00 <- 1e 2 bytes beschadigd (correct, is return address)

00021311 85 C0                test        eax,eax
00021313 75 07                jne         virtual_memory_placeholder+1031Ch (02131Ch)
00021315 33 C0                xor         eax,eax
00021317 E9 E8 00 00 00       jmp         virtual_memory_placeholder+10404h (021404h)
0002131C 8B CE                mov         ecx,esi
0002131E E8 02 FB FF FF       call        virtual_memory_placeholder+0FE25h (020E25h)
00021323 85 C0                test        eax,eax
00021325 74 EE                je          virtual_memory_placeholder+10315h (021315h)
00021327 57                   push        edi
00021328 8D 45 F0             lea         eax,[ebp-10h]
0002132B 50                   push        eax
0002132C 6A 03                push        3
0002132E FF 15 B8 91 02 00    call        dword ptr ds:[291B8h]
00021334 53                   push        ebx
00021335 6A 00                push        0
00021337 FF 75 F0             push        dword ptr [ebp-10h]
0002133A 8D 7E 14             lea         edi,[esi+14h]
0002133D 50                   push        eax
0002133E 56                   push        esi
0002133F 68 B0 18 02 00       push        218B0h
00021344 57                   push        edi
00021345 FF 15 B4 91 02 00    call        dword ptr ds:[291B4h]
0002134B 57                   push        edi
0002134C FF 15 B0 91 02 00    call        dword ptr ds:[291B0h]
00021352 84 C0                test        al,al
00021354 75 07                jne         virtual_memory_placeholder+1035Dh (02135Dh)
00021356 33 C0                xor         eax,eax
00021358 E9 A6 00 00 00       jmp         virtual_memory_placeholder+10403h (021403h)
0002135D 83 A6 68 01 00 00 00 and         dword ptr [esi+168h],0
00021364 8D 86 64 01 00 00    lea         eax,[esi+164h]
0002136A 53                   push        ebx
0002136B 50                   push        eax
0002136C C7 00 80 1F 02 00    mov         dword ptr [eax],21F80h
00021372 FF 15 AC 91 02 00    call        dword ptr ds:[291ACh]
00021378 8B CE                mov         ecx,esi
0002137A E8 E7 FA FF FF       call        virtual_memory_placeholder+0FE66h (020E66h)
0002137F 85 C0                test        eax,eax
00021381 74 D3                je          virtual_memory_placeholder+10356h (021356h)
00021383 8D 86 DC 02 00 00    lea         eax,[esi+2DCh]
00021389 89 45 FC             mov         dword ptr [ebp-4],eax
0002138C C7 45 F4 02 00 00 00 mov         dword ptr [ebp-0Ch],2
00021393 BA 00 01 00 00       mov         edx,100h
00021398 33 C0                xor         eax,eax
0002139A 8B 4D FC             mov         ecx,dword ptr [ebp-4]
0002139D 03 C8                add         ecx,eax
0002139F BF 00 FF FF FF       mov         edi,0FFFFFF00h
000213A4 88 04 0F             mov         byte ptr [edi+ecx],al
000213A7 BF 00 01 00 00       mov         edi,100h
000213AC 88 01                mov         byte ptr [ecx],al
000213AE 88 04 0F             mov         byte ptr [edi+ecx],al
000213B1 40                   inc         eax
000213B2 3B C2                cmp         eax,edx
000213B4 72 E4                jb          virtual_memory_placeholder+1039Ah (02139Ah)
000213B6 81 45 FC 00 03 00 00 add         dword ptr [ebp-4],300h
000213BD FF 4D F4             dec         dword ptr [ebp-0Ch]
000213C0 75 D6                jne         virtual_memory_placeholder+10398h (021398h)
000213C2 8B 3D A8 91 02 00    mov         edi,dword ptr ds:[291A8h]
000213C8 6A 00                push        0
000213CA 66 BA C0 80          mov         dx,80C0h
000213CE 8A C3                mov         al,bl
000213D0 6A 04                push        4
000213D2 EE                   out         dx,al
000213D3 8D 45 F8             lea         eax,[ebp-8]
000213D6 50                   push        eax
000213D7 6A 4C                push        4Ch
000213D9 6A 00                push        0
000213DB 53                   push        ebx
000213DC FF D7                call        edi
000213DE 80 4D FB 1F          or          byte ptr [ebp-5],1Fh
000213E2 53                   push        ebx
000213E3 6A 04                push        4
000213E5 8D 45 F8             lea         eax,[ebp-8]
000213E8 50                   push        eax
000213E9 6A 4C                push        4Ch
000213EB 6A 00                push        0
000213ED 53                   push        ebx
000213EE FF D7                call        edi
000213F0 8B CE                mov         ecx,esi
000213F2 89 9E A0 00 00 00    mov         dword ptr [esi+0A0h],ebx
000213F8 E8 EC FA FF FF       call        virtual_memory_placeholder+0FEE9h (020EE9h)
000213FD F7 D8                neg         eax
000213FF 1B C0                sbb         eax,eax
00021401 F7 D8                neg         eax
00021403 5F                   pop         edi
00021404 5E                   pop         esi
ESP 0988FC78 : 60 10 01 80 60 80 02 00
EAX = 00000001 EBX = 00000001 ECX = 00000000 EDX = 00000000 ESI = 00000001 EDI = 000281C4 EIP = 00021405 ESP = 0988FC78 EBP = 0988FC94 EFL = 00000213
00021405 5B                   pop         ebx
00021406 C9                   leave
00021407 C3                   ret

MapRegisters
00020DFA C7 01 00 00 00 FD    mov         dword ptr [ecx],0FD000000h
00020E00 A1 04 18 00 FD       mov         eax,dword ptr ds:[FD001804h]
00020E05 83 C8 04             or          eax,4
00020E08 A3 04 18 00 FD       mov         dword ptr ds:[FD001804h],eax
00020E0D 33 C0                xor         eax,eax
00020E0F C7 05 40 01 60 FD 00 00 00 00 mov         dword ptr ds:[0FD600140h],0
00020E19 C7 05 40 91 00 FD 00 00 00 00 mov         dword ptr ds:[0FD009140h],0
00020E23 40                   inc         eax
00020E24 C3                   ret
00020E25 8B 11                mov         edx,dword ptr [ecx]
00020E27 8B 82 00 18 00 00    mov         eax,dword ptr [edx+1800h]
00020E2D C1 E8 10             shr         eax,10h
00020E30 25 FC FF 00 00       and         eax,0FFFCh
00020E35 89 81 A8 00 00 00    mov         dword ptr [ecx+0A8h],eax
00020E3B 8B 82 08 18 00 00    mov         eax,dword ptr [edx+1808h]
00020E41 25 FF 00 00 00       and         eax,0FFh
00020E46 89 81 C0 00 00 00    mov         dword ptr [ecx+0C0h],eax
00020E4C 8B 92 0C 02 10 00    mov         edx,dword ptr [edx+10020Ch]
00020E52 33 C0                xor         eax,eax
00020E54 89 91 AC 00 00 00    mov         dword ptr [ecx+0ACh],edx
00020E5A C7 81 BC 00 00 00 2A 50 FE 00 mov         dword ptr [ecx+0BCh],0FE502Ah
00020E64 40                   inc         eax
00020E65 C3                   ret

CreateCtxDmaObject
00020FDB 55                   push        ebp
00020FDC 8B EC                mov         ebp,esp
00020FDE 51                   push        ecx
00020FDF 51                   push        ecx
00020FE0 53                   push        ebx
00020FE1 56                   push        esi
00020FE2 57                   push        edi
00020FE3 33 C0                xor         eax,eax
00020FE5 50                   push        eax
00020FE6 89 45 F8             mov         dword ptr [ebp-8],eax
00020FE9 89 45 FC             mov         dword ptr [ebp-4],eax
00020FEC 8D 45 FC             lea         eax,[ebp-4]
00020FEF 50                   push        eax
00020FF0 8D 45 F8             lea         eax,[ebp-8]
00020FF3 50                   push        eax
00020FF4 FF 75 10             push        dword ptr [ebp+10h]
00020FF7 8B D1                mov         edx,ecx  // <- ecx = 1, komt van esi
00020FF9 8B 3A                mov         edi,dword ptr [edx]  // <- BOEM
00020FFB E8 81 FF FF FF       call        virtual_memory_placeholder+0FF81h (020F81h)
00021000 8B 5D F8             mov         ebx,dword ptr [ebp-8]
00021003 8D 8A 60 01 00 00    lea         ecx,[edx+160h]
00021009 8B 11                mov         edx,dword ptr [ecx]
0002100B 8D 72 01             lea         esi,[edx+1]
0002100E 89 31                mov         dword ptr [ecx],esi
00021010 8B 4D 0C             mov         ecx,dword ptr [ebp+0Ch]
00021013 6A 02                push        2
00021015 8B C3                mov         eax,ebx
00021017 5E                   pop         esi
00021018 83 C8 03             or          eax,3
0002101B 2B CE                sub         ecx,esi
0002101D 74 0A                je          virtual_memory_placeholder+10029h (021029h)
0002101F 49                   dec         ecx
00021020 74 04                je          virtual_memory_placeholder+10026h (021026h)
00021022 6A 3D                push        3Dh
00021024 EB 02                jmp         virtual_memory_placeholder+10028h (021028h)
00021026 6A 03                push        3
00021028 5E                   pop         esi
00021029 C1 E3 14             shl         ebx,14h
0002102C 81 CB 00 30 00 00    or          ebx,3000h
00021032 0B F3                or          esi,ebx
00021034 83 7D FC 02          cmp         dword ptr [ebp-4],2
00021038 74 1A                je          virtual_memory_placeholder+10054h (021054h)
0002103A 83 7D FC 03          cmp         dword ptr [ebp-4],3
0002103E 75 08                jne         virtual_memory_placeholder+10048h (021048h)
00021040 81 CE 00 00 03 00    or          esi,30000h
00021046 EB 0C                jmp         virtual_memory_placeholder+10054h (021054h)
00021048 83 7D FC 01          cmp         dword ptr [ebp-4],1
0002104C 75 06                jne         virtual_memory_placeholder+10054h (021054h)
0002104E 81 CE 00 00 02 00    or          esi,20000h
00021054 8B CA                mov         ecx,edx
00021056 C1 E1 04             shl         ecx,4
00021059 03 CF                add         ecx,edi
0002105B 89 81 08 00 70 00    mov         dword ptr [ecx+700008h],eax
00021061 89 81 0C 00 70 00    mov         dword ptr [ecx+70000Ch],eax
00021067 81 CE 00 80 00 00    or          esi,8000h
0002106D 8D 82 00 00 07 00    lea         eax,[edx+70000h]
00021073 C1 E0 04             shl         eax,4
00021076 89 34 38             mov         dword ptr [eax+edi],esi
00021079 8B 45 14             mov         eax,dword ptr [ebp+14h]
0002107C 89 81 04 00 70 00    mov         dword ptr [ecx+700004h],eax
00021082 8B 4D 18             mov         ecx,dword ptr [ebp+18h]
00021085 33 C0                xor         eax,eax
00021087 8B F9                mov         edi,ecx
00021089 AB                   stos        dword ptr es:[edi]
0002108A AB                   stos        dword ptr es:[edi]
0002108B AB                   stos        dword ptr es:[edi]
0002108C AB                   stos        dword ptr es:[edi]
0002108D 8B 45 08             mov         eax,dword ptr [ebp+8]
00021090 66 83 61 06 00       and         word ptr [ecx+6],0
00021095 89 01                mov         dword ptr [ecx],eax
00021097 8B 45 0C             mov         eax,dword ptr [ebp+0Ch]
0002109A 5F                   pop         edi
0002109B 89 41 08             mov         dword ptr [ecx+8],eax
0002109E 33 C0                xor         eax,eax
000210A0 5E                   pop         esi
000210A1 89 51 0C             mov         dword ptr [ecx+0Ch],edx
000210A4 40                   inc         eax
000210A5 5B                   pop         ebx
000210A6 C9                   leave
000210A7 C2 14 00             ret         14h


*/
	return TRUE;
}
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