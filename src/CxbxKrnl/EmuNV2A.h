// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   CxbxKrnl->EmuNV2A.h
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
// *  (c) 2016 Luke Usher <luke.usher@outlook.com>
// *  CopyRight (c) 2016-2017 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef EMUNV2A_H
#define EMUNV2A_H

// Valid after PCI init :
#define NV20_REG_BASE_KERNEL 0xFD000000

#define NV2A_ADDR  0xFD000000
#define NV2A_SIZE             0x01000000

#define NV_PMC_ADDR      0x00000000
#define NV_PMC_SIZE                 0x001000
#define NV_PBUS_ADDR     0x00001000
#define NV_PBUS_SIZE                0x001000
#define NV_PFIFO_ADDR    0x00002000
#define _NV_PFIFO_SIZE               0x002000 // Underscore prefix to prevent clash with NV_PFIFO_SIZE
#define NV_PRMA_ADDR     0x00007000
#define NV_PRMA_SIZE                0x001000
#define NV_PVIDEO_ADDR   0x00008000
#define NV_PVIDEO_SIZE              0x001000
#define NV_PTIMER_ADDR   0x00009000
#define NV_PTIMER_SIZE              0x001000
#define NV_PCOUNTER_ADDR 0x0000A000
#define NV_PCOUNTER_SIZE            0x001000
#define NV_PVPE_ADDR     0x0000B000
#define NV_PVPE_SIZE                0x001000
#define NV_PTV_ADDR      0x0000D000
#define NV_PTV_SIZE                 0x001000
#define NV_PRMFB_ADDR    0x000A0000
#define NV_PRMFB_SIZE               0x020000
#define NV_PRMVIO_ADDR   0x000C0000
#define NV_PRMVIO_SIZE              0x008000 // Was 0x001000
#define NV_PFB_ADDR      0x00100000
#define NV_PFB_SIZE                 0x001000
#define NV_PSTRAPS_ADDR  0x00101000
#define NV_PSTRAPS_SIZE             0x001000
#define NV_PGRAPH_ADDR   0x00400000
#define NV_PGRAPH_SIZE              0x002000
#define NV_PCRTC_ADDR    0x00600000
#define NV_PCRTC_SIZE               0x001000
#define NV_PRMCIO_ADDR   0x00601000
#define NV_PRMCIO_SIZE              0x001000
#define NV_PRAMDAC_ADDR  0x00680000
#define NV_PRAMDAC_SIZE             0x001000
#define NV_PRMDIO_ADDR   0x00681000
#define NV_PRMDIO_SIZE              0x001000
#define NV_PRAMIN_ADDR   0x00700000 // Was 0x00710000
#define NV_PRAMIN_SIZE              0x100000
#define NV_USER_ADDR     0x00800000
#define NV_USER_SIZE                0x400000 // Was 0x800000
#define NV_UREMAP_ADDR   0x00C00000 // Looks like a mapping of NV_USER_ADDR
#define NV_UREMAP_SIZE              0x400000

typedef struct {
	DWORD Ignored[0x10];
	void *Put; // On Xbox1, this field is only written to by the CPU (the GPU uses this as a trigger to start executing from the given address)
	void *Get; // On Xbox1, this field is only read from by the CPU (the GPU reflects in here where it is/stopped executing)
	void *Reference;
	DWORD Ignored2[0x7ED];
} Nv2AControlDma;

extern Nv2AControlDma *g_pNV2ADMAChannel;

extern volatile xbaddr *m_pGPUTime; // set (to 0x80011000 or something) by DEVICE_WRITE32(PRAMIN) case 0x00000098

extern HANDLE ghNV2AFlushEvent;

uint32_t EmuNV2A_Read(xbaddr addr, int size);
void EmuNV2A_Write(xbaddr addr, uint32_t value, int size);

#define NV2A_JMP_FLAG          0x00000001 // 1 bit
#define NV2A_CALL_FLAG         0x00000002 // 1 bit TODO : Should JMP & CALL be switched?
#define NV2A_ADDR_MASK         0xFFFFFFFC // 30 bits
#define NV2A_METHOD_MASK       0x00001FFC // 12 bits
#define NV2A_METHOD_SHIFT      0 // Dxbx note : Not 2, because methods are actually DWORD offsets (and thus defined with increments of 4)
#define NV2A_SUBCH_MASK        0x0000E000 // 3 bits
#define NV2A_SUBCH_SHIFT       13 // Was 12
#define NV2A_COUNT_MASK        0x0FFF0000 // 12 bits
#define NV2A_COUNT_SHIFT       16 // Was 18
#define NV2A_NOINCREMENT_FLAG  0x40000000 // 1 bit
// Dxbx note : What do the other bits mean (mask $B0000000) ?

#define NV2A_METHOD_MAX ((NV2A_METHOD_MASK | 3) >> NV2A_METHOD_SHIFT) // = 8191
#define NV2A_SUBCH_MAX (NV2A_SUBCH_MASK >> NV2A_SUBCH_SHIFT) // = 7
#define NV2A_COUNT_MAX (NV2A_COUNT_MASK >> NV2A_COUNT_SHIFT) // = 2047

inline void D3DPUSH_DECODE(const DWORD dwPushCommand, DWORD &dwMethod, DWORD &dwSubCh, DWORD &dwCount, bool &bNoInc)
{
	dwMethod = (dwPushCommand & NV2A_METHOD_MASK); // >> NV2A_METHOD_SHIFT;
	dwSubCh = (dwPushCommand & NV2A_SUBCH_MASK) >> NV2A_SUBCH_SHIFT;
	dwCount = (dwPushCommand & NV2A_COUNT_MASK) >> NV2A_COUNT_SHIFT;
	bNoInc = (dwPushCommand & NV2A_NOINCREMENT_FLAG) > 0;
}

void InitOpenGLContext();

#endif // EMUNV2A_H
