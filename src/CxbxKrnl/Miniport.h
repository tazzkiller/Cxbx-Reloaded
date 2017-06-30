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
// *   CxbxKrnl->Miniport.h
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
#ifndef MINIPORT_H
#define MINIPORT_H

typedef struct {
	ULONG Handle;
	USHORT SubChannel;
	USHORT Engine;
	ULONG ClassNum;
	ULONG Instance;
} OBJECTINFO;

extern void *GPURegisterBase;

extern volatile xbaddr *m_pCPUTime;

void CxbxLocateCpuTime();

// ******************************************************************
// * patch: CMiniport_InitHardware
// ******************************************************************
BOOL __fastcall EMUPATCH(CMiniport_InitHardware)
(
	PVOID This,
	void * _EDX // __thiscall simulation
);

// ******************************************************************
// * patch: CMiniport_CreateCtxDmaObject
// ******************************************************************
BOOL __fastcall EMUPATCH(CMiniport_CreateCtxDmaObject)
(
	PVOID This,
	void * _EDX, // __thiscall simulation
	ULONG Dma,
	ULONG ClassNum,
	PVOID Base,
	ULONG Limit,
	PVOID Object
);

// ******************************************************************
// * patch: CMiniport_InitDMAChannel
// ******************************************************************
BOOL __fastcall EMUPATCH(CMiniport_InitDMAChannel)
(
	PVOID This,
	void * _EDX, // __thiscall simulation
	ULONG Class,
	OBJECTINFO *ErrorContext,
	OBJECTINFO *DataContext,
	ULONG Offset,
	Nv2AControlDma **ppChannel
);

#endif // MINIPORT_H