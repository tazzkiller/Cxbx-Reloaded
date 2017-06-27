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

#include "CxbxKrnl.h"
#include "EmuXTL.h"
#include "Logging.h"
#include "EmuD3D8Logging.h"

namespace XTL {

void *GPURegisterBase = NULL; // + NV2A_PFB_WC_CACHE = pNV2AWorkTrigger (see Dxbx EmuThreadHandleNV2ADMA)
DWORD *m_pGPUTime = NULL; // See Dxbx XTL_ EmuExecutePushBufferRaw

// Although EmuNV2A catches all NV2A accesses (producing a lot of detailed logging),
// this patch on CMiniport::InitHardware skips all that, and just initialiazes the
// CMiniport::m_RegisterBase so there's some memory to play with.
BOOL __fastcall EMUPATCH(CMiniport_InitHardware)
(
	PVOID This,
	void * _EDX // __thiscall simulation
)
{
#ifdef UNPATCH_CREATEDEVICE
	FUNC_EXPORTS
#endif

	LOG_FUNC_ONE_ARG(This);

#if 0 // Old Dxbx approach :
	// CMiniport::MapRegisters() sets m_RegisterBase (the first member of CMiniport) to 0xFD000000 (NV2A_ADDR)
	// That address isn't memory-mapped, and thus would cause lots of exceptions,
	// so instead we allocate a buffer in host virtual memory
	GPURegisterBase = g_MemoryManager.AllocateZeroed(1, 2 * 1024 * 1024); // PatrickvL : I've seen RegisterBase offsets up to $00100410 so 2 MB should suffice
#else
	GPURegisterBase = (void *)0xFD000000; // TODO : using "EmuNV2A.h" // for NV2A_ADDR
#endif
										  // And put that in the m_RegisterBase field :
	*((void **)This) = GPURegisterBase;
	DbgPrintf("Allocated a block of 2 MB to serve as the GPUs RegisterBase at 0x%.08x\n", GPURegisterBase);

	RETURN(true);
}

INT __fastcall EMUPATCH(CMiniport_CreateCtxDmaObject)
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
#ifdef UNPATCH_CREATEDEVICE
	FUNC_EXPORTS
#endif

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
			  // (we could have trapped MmAllocateContiguousMemoryEx too, but this is simpler) :
		m_pGPUTime = (PDWORD)Base;
		DbgPrintf("Registered m_pGPUTime at 0x%0.8x\n", m_pGPUTime);
		break;
	}
	default: {
		LOG_UNIMPLEMENTED();
	}
	}

	return S_OK;
}

}