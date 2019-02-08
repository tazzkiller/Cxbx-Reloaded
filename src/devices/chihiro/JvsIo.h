// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   src->devices->chihiro->JvsIo.h
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

#ifndef JVSIO_H
#define JVSIO_H

#include <cstdint>
#include <vector>

typedef struct {
	uint8_t sync;
	uint8_t target;
	uint8_t count;
} jvs_packet_header_t;

class JvsIo
{
public:
	JvsIo(uint8_t* sense);
	void HandlePacket(jvs_packet_header_t* header, uint8_t* payload);
	size_t SendPacket(jvs_packet_header_t* packet);
	size_t ReceivePacket(void* packet);
	uint8_t GetDeviceId();
private:
	bool BroadcastPacket;					// Set when the last command was a broadcast
	uint8_t* pSense = nullptr;				// Pointer to Sense line
	uint8_t DeviceId = 0;					// Device ID assigned by running title
	std::vector<uint8_t> ResponseBuffer;	// Command Response

	// Device info
	uint8_t CommandFormatRevision;
	uint8_t JvsVersion;
	uint8_t CommunicationVersion;
	std::string BoardID;
};

extern JvsIo* g_pJvsIo;

#endif
