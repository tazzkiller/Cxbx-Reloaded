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

#pragma optimize("", off)

uint32_t MediaBoard::LpcRead(uint32_t addr, int size)
{
	switch (addr) {
		case 0x40F0: return 0;	// 0 if media board is present
	}

	printf("MediaBoard::LpcRead: Unknown Addr %08X\n", addr);
	return 0xFFFFFFFF;
}

void MediaBoard::LpcWrite(uint32_t addr, uint32_t value, int size)
{
	printf("MediaBoard::LpcWrite: Unknown Addr %08X\n", addr);
}

void MediaBoard::ComRead(uint32_t offset, void* buffer, uint32_t length)
{
	// Copy the current read buffer to the output
	memcpy(buffer, readBuffer, length);
}

void MediaBoard::ComWrite(uint32_t offset, void* buffer, uint32_t length)
{
	// Copy the written data to our internal, so we don't trash the original data
	memcpy(writeBuffer, buffer, length);

	// Create accessor pointers 
	auto writeBuffer16 = (uint16_t*)writeBuffer;
	auto readBuffer16 = (uint16_t*)readBuffer;

	// Copy the given command to the read buffer
	writeBuffer16[0] = readBuffer16[0];

	// If no command word was specified, do nothing
	if (writeBuffer16[0] == 0) {
		return;
	}

	// TODO: Verify the meaning of this, seems to be some kind of ACK?
	readBuffer16[1] = 0x8001;

	// Read the given Command and handle it
	uint32_t command = writeBuffer16[2];
	switch (command) {
		default: printf("Unhandled MediaBoard Command: %04X\n", command);
	}
}
