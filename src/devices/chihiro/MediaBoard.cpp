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
// *   src->devices->chihiro->MediaBoard.cpp
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
// *  (c) 2019 Luke Usher
// *
// *  All rights reserved
// *
// ******************************************************************

#include "MediaBoard.h"
#include <cstdio>
#include <string>

#define _XBOXKRNL_DEFEXTRN_

namespace xboxkrnl
{
	#include <xboxkrnl/xboxkrnl.h>
};

#include "core\kernel\exports\EmuKrnl.h" // for HalSystemInterrupts

#pragma optimize("", off)

uint32_t MediaBoard::LpcRead(uint32_t addr, int size)
{
	switch (addr) {
		// 0x1E-0x24 = XBAM string/version info?
		// SEGABOOT assumes Media Board is not present if these values change
		case 0x401E: return 0x1000;
		case 0x4020: return 0x00A0;
		case 0x4022: return 0x4258;  
		case 0x4024: return 0x4D41;

		case 0x40F0: return 0x00;	// Media Board Type (Type-1 vs Type-3), 0x00 = Type 1
		case 0x40F4: return 0x02;	// 512MB
	}
	
	printf("MediaBoard::LpcRead: Unknown Addr %08X\n", addr);
	return 0;
}

void MediaBoard::LpcWrite(uint32_t addr, uint32_t value, int size)
{
	switch (addr) {
		case 0x40E1: HalSystemInterrupts[10].Assert(false); break;
		default:
			printf("MediaBoard::LpcWrite: Unknown Addr %08X\n", addr);
	}	
}

void MediaBoard::ComRead(uint32_t offset, void* buffer, uint32_t length)
{
	// Copy the current read buffer to the output
	memcpy(buffer, readBuffer, 0x20);
}

void MediaBoard::ComWrite(uint32_t offset, void* buffer, uint32_t length)
{
	if (offset == 0x900000) { // Some kind of reset?
		memcpy(readBuffer, buffer, 0x20);
		return;
	} else if (offset == 0x900200) { // Command Sector
		// Copy the written data to our internal, so we don't trash the original data
		memcpy(writeBuffer, buffer, 0x20);

		// Create accessor pointers 
		auto writeBuffer16 = (uint16_t*)writeBuffer;
		auto readBuffer16 = (uint16_t*)readBuffer;
		auto readBuffer32 = (uint32_t*)readBuffer;

		// If no command word was specified, do nothing
		if (writeBuffer16[0] == 0) {
			return;
		}

		// First word of output gets set to first word of the input
		readBuffer16[0] = writeBuffer16[0];
		readBuffer16[1] = 0x8001; // TODO: Verify the meaning of this, seems to be some kind of ACK?

		// Read the given Command and handle it
		uint32_t command = writeBuffer16[1];
		switch (command) {
			case 0x0001: // DIMM board size register
				readBuffer32[1] = 1; // TODO: Figure out encoding, this value gives an invalid size (4GB)
				break;
			case 0x0100: // Loading Info
				readBuffer32[1] = 5; // TODO: Why 5?
				readBuffer32[2] = 0; // TODO: Why 0?
				break;
			case 0x0101: // Firmware Version
				readBuffer32[1] = 0x0100;
				break;
			case 0x0103: // Serial Number
				memcpy(&readBuffer32[1], "-000-000-000-000", 16);
				break;
			default: printf("Unhandled MediaBoard Command: %04X\n", command);
		}

		// Clear the command bytes
		writeBuffer16[0] = 0;
		writeBuffer16[1] = 0;

		// Trigger LPC Interrupt
		HalSystemInterrupts[10].Assert(true);
		return;
	}

	printf("Unhandled MediaBoard mbcom: offset %08X\n", offset);
}
