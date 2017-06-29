// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   CxbxKrnl->EmuD3D8->PushBuffer.h
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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef PUSHBUFFER_H
#define PUSHBUFFER_H

extern int DxbxFVF_GetTextureSize(DWORD dwFVF, int aTextureIndex);

extern UINT DxbxFVFToVertexSizeInBytes(DWORD dwFVF, BOOL bIncludeTextures);

extern void CxbxDrawIndexed(CxbxDrawContext &DrawContext, INDEX16 *pIndexData);
extern void CxbxDrawPrimitiveUP(CxbxDrawContext &DrawContext);

extern void EmuExecutePushBuffer
(
    XTL::X_D3DPushBuffer       *pPushBuffer,
	XTL::X_D3DFixup            *pFixup
);

extern DWORD *EmuExecutePushBufferRaw
(
	DWORD                 *pdwPushData
);

extern void DbgDumpPushBuffer
( 
	DWORD*				  PBData, 
	DWORD				  dwSize 
);

typedef struct {
	volatile DWORD *m_pPut; // This is the address to where the CPU will write it's next GPU instruction
	volatile DWORD *m_pThreshold; // This is the upper limit for m_pPut (when it's reached, MakeSpace() is called,
	// which just forwards the call to MakeRequestedSpace, passing it m_PushSegmentSize/2 as 'minimum space',
	// and m_PushSegmentSize (without division) as 'requested space')
} Pusher;

DWORD WINAPI EmuThreadHandleNV2ADMA(LPVOID);

// primary push buffer
extern uint32  g_dwPrimaryPBCount;
extern uint32 *g_pPrimaryPB;

// push buffer debugging
extern bool g_bStepPush;
extern bool g_bSkipPush;
extern bool g_bBrkPush;

#endif // PUSHBUFFER_H