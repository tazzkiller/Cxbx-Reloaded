#pragma once

// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->EmuMem.cpp
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx and Cxbe are free software; you can redistribute them
// *  and/or modify them under the terms of the GNU General Public
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
// *  (c) 2016 Patrick van Logchem <pvanlogchem@gmail.com>
// *  All rights reserved
// *
// ******************************************************************
#include "EmuMem.h"

// handlers to read/write host memory
READ32_MEMBER(mem_host32_r)
{
	return *(u32*)offset;
}

WRITE32_MEMBER(mem_host32_w)
{
	*(u32*)offset = data;
}

// global address handler arrays for level 1
handle32_r g_AddrLvl1_r[LEVEL1_SIZE] = {};
handle32_w g_AddrLvl1_w[LEVEL1_SIZE] = {};

void mem_handlers_init()
{
	for (int i = 0; i < LEVEL1_SIZE; i++)
	{
		// by default, use host-memory accessors for all addresses
		g_AddrLvl1_r[i] = &mem_host32_r;
		g_AddrLvl1_w[i] = &mem_host32_w;
	}
}

// determine table indexes based on the address
u32 level1_index(offs_t address) { return address >> LEVEL2_BITS; }

void mem_handler_install_r(offs_t addrstart, offs_t addrend, offs_t addrmask, handle32_r handler_r)
{
	// TODO : Copy over mem_handler_install_w, update for reading-side
}

// Note : addrend must be EXcluding
// Note : this will overwrite previously installed handlers (within the given mask)
void mem_handler_install_w(offs_t addrstart, offs_t addrend, offs_t addrmask, handle32_w handler_w)
{
	// TODO : assert aligned addrstart 
	// TODO : assert aligned addrend
	// TODO : assert addrstart < addrend
	// TODO : assert addrmask != 0
	// TODO : assert handler_w != nullptr

	// determine first level starting and ending slots
	const int slot_start = level1_index(addrstart);
	const int slot_end = level1_index(addrend);

	// determine starting and ending addresses (buth INcluding) of these slots
	const offs_t slot_start_addr = slot_start << LEVEL2_BITS;
	const offs_t slot_end_addr = ((slot_end + 1) << LEVEL2_BITS) - sizeof(offs_t);

	int slot_curr = slot_start;

	// does the supplied start address start on the first address of this slot?
	if (addrstart == slot_start_addr)
	{
		// is the current slot not the last one?
		while ((slot_curr < slot_end) &&
			// and is this slot occupied with the default handler ?
			(g_AddrLvl1_w[slot_curr] == &mem_host32_w))
				// replace the entire slot with the new handler, and step to the next slot
				g_AddrLvl1_w[slot_curr++] = handler_w;
	}

	// is the current slot occupied with the default handler?
	if (g_AddrLvl1_w[slot_curr] == &mem_host32_w)
	{
		// TODO : split the current slot, so we can diverge in level 2
	}

	// TODO : Continue with this until addrend
}
