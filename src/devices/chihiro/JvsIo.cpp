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
// *   src->devices->chihiro->JvsIo.cpp
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

#include "JvsIo.h"
#include <cstdio>
#include <string>

JvsIo* g_pJvsIo;

//#define DEBUG_JVS_PACKETS
#include <vector>

// We will emulate SEGA 837-13551 IO Board
JvsIo::JvsIo(uint8_t* sense)
{
	pSense = sense;

	// Version info BCD Format: X.X
	CommandFormatRevision = 0x11;
	JvsVersion = 0x20;
	CommunicationVersion = 0x10;

	BoardID = "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551";
}

void JvsIo::Update()
{
	// TODO: Update Jvs inputs based on user configuration
}

uint8_t JvsIo::GetDeviceId()
{
	return BroadcastPacket ? 0x00 : DeviceId;
}

int JvsIo::Jvs_Command_Reset()
{
	// Set sense to 3 (2.5v) to instruct the baseboard we're ready.
	*pSense = 3;
	ResponseBuffer.push_back(0x01);
	DeviceId = 0;

	return 1;
}

int JvsIo::Jvs_Command_SetDeviceId(uint8_t* data)
{
	// Set Address
	DeviceId = data[1];
	*pSense = 0; // Set sense to 0v
	ResponseBuffer.push_back(0x01);
	return 1;
}

int JvsIo::Jvs_Command_GetBoardId()
{
	// Get Board ID
	ResponseBuffer.push_back(0x01);

	for (char& c : BoardID) {
		ResponseBuffer.push_back(c);
	}

	return 0;
}

int JvsIo::Jvs_Command_GetCommandFormat()
{
	ResponseBuffer.push_back(0x01);
	ResponseBuffer.push_back(CommandFormatRevision);

	return 0;
}

int JvsIo::Jvs_Command_GetJvsRevision()
{
	ResponseBuffer.push_back(0x01);
	ResponseBuffer.push_back(JvsVersion);

	return 0;
}

int JvsIo::Jvs_Command_GetCommunicationVersion()
{
	ResponseBuffer.push_back(0x01);
	ResponseBuffer.push_back(CommunicationVersion);

	return 0;
}

int JvsIo::Jvs_Command_GetCapabilities()
{
	ResponseBuffer.push_back(0x01);

	// 2 players, 13 switches per player
	ResponseBuffer.push_back(0x01);
	ResponseBuffer.push_back(2);
	ResponseBuffer.push_back(13);
	ResponseBuffer.push_back(0);

	// Two Coin Slots
	ResponseBuffer.push_back(0x02);
	ResponseBuffer.push_back(2);
	ResponseBuffer.push_back(0);
	ResponseBuffer.push_back(0);

	// Analog Input, 8 channels, 16 bits per channel
	ResponseBuffer.push_back(0x03);
	ResponseBuffer.push_back(8);
	ResponseBuffer.push_back(16);
	ResponseBuffer.push_back(0);

	// 6 Outputs
	ResponseBuffer.push_back(0x12);
	ResponseBuffer.push_back(6);
	ResponseBuffer.push_back(0);
	ResponseBuffer.push_back(0);

	return 0;
}

int JvsIo::Jvs_Command_ReadSwitchInputs(uint8_t* data)
{
	uint8_t players = data[1];
	uint8_t bytesPerPlayer = data[2];

	ResponseBuffer.push_back(0x01);

	ResponseBuffer.push_back(Inputs.switches.system);

	for (int i = 0; i < players; i++) {
		// We only support two players, so we pad any extras with null bytes
		if (i >= 2) {
			for (int j = 0; j < bytesPerPlayer; j++) {
				ResponseBuffer.push_back(0);
			}
		}	

		if (bytesPerPlayer >= 1) {
			ResponseBuffer.push_back(Inputs.switches.player[i].GetByte0());
		}

		if (bytesPerPlayer >= 2) {
			ResponseBuffer.push_back(Inputs.switches.player[i].GetByte1());
		}

		// Pad any remaining bytes with 0, as we don't have that many inputs available
		if (bytesPerPlayer > 2) {
			for (int j = 0; i < bytesPerPlayer - 2; j++) {
				ResponseBuffer.push_back(0);
			}
		}
	}
		
	return 2;
}

int JvsIo::Jvs_Command_ReadCoinInputs(uint8_t* data)
{
	uint8_t slots = data[1];
	
	ResponseBuffer.push_back(0x01);

	for (int i = 0; i < (slots); i++) {
		// We only have two coin slots, if a title should ask for more, pad with dummy data
		if (i >= 2) {
			ResponseBuffer.push_back(0);
			ResponseBuffer.push_back(0);
			continue;
		}

		ResponseBuffer.push_back(Inputs.coins[i].GetByte0());
		ResponseBuffer.push_back(Inputs.coins[i].GetByte1());
	}

	return 1;
}

int JvsIo::Jvs_Command_ReadAnalogInputs(uint8_t* data)
{
	uint8_t inputs = data[1];

	ResponseBuffer.push_back(0x01);

	for (int i = 0; i < (inputs); i++) {
		if (inputs >= 8) {
			// We only have 8 analog inputs, if a title should ask for more, pad with dummy data
			ResponseBuffer.push_back(0x80);
			ResponseBuffer.push_back(0x00);
			continue;
		}

		ResponseBuffer.push_back(Inputs.analog->GetByte0());
		ResponseBuffer.push_back(Inputs.analog->GetByte1());
	}

	return 1;
}

int JvsIo::Jvs_Command_GeneralPurposeOutput(uint8_t* data)
{
	uint8_t banks = data[1];

	ResponseBuffer.push_back(0x01);

	// TODO: Handle output

	// Data size is n.banks followed by one byte per bank
	return 1 + banks;
}

void JvsIo::HandlePacket(jvs_packet_header_t* header, uint8_t* payload)
{
	// Decode the payload data
	std::vector<uint8_t> packet;
	for (unsigned i = 0; i < header->count; i++) {
		uint8_t value = payload[i];

		// Special case: 0xD0 is an exception byte that actually returns the next byte + 1
		if (value == 0xD0) {
			value = payload[i++] + 1;
		}

		packet.push_back(value);
	}

	// Clear the response buffer
	ResponseBuffer.clear();

	ResponseBuffer.push_back(0x01);
	
	// It's possible for a JVS packet to contain multiple commands, so we must iterate through it
	// -1 is to skip the checksum byte
	for (int i = 0; i < packet.size() - 1; i++) {
		BroadcastPacket = packet[i] >= 0xF0; // Set a flag when broadcast packet

		switch (packet[i]) {
			// Broadcast Commands
			case 0xF0: i += Jvs_Command_Reset(); break;
			case 0xF1: i += Jvs_Command_SetDeviceId(&packet[i]); break;
			// Init Commands
			case 0x10: i += Jvs_Command_GetBoardId(); break;
			case 0x11: i += Jvs_Command_GetCommandFormat();	break;
			case 0x12: i += Jvs_Command_GetJvsRevision(); break;
			case 0x13: i += Jvs_Command_GetCommunicationVersion(); break;
			case 0x14: i += Jvs_Command_GetCapabilities(); break;
			case 0x20: i += Jvs_Command_ReadSwitchInputs(&packet[i]); break;
			case 0x21: i += Jvs_Command_ReadCoinInputs(&packet[i]); break;
			case 0x22: i += Jvs_Command_ReadAnalogInputs(&packet[i]); break;
			case 0x32: i += Jvs_Command_GeneralPurposeOutput(&packet[i]); break;
			default:
				printf("JvsIo::HandlePacket: Unhandled Command %02X\n", packet[i]);
				break;

		}
	}
}

size_t JvsIo::SendPacket(jvs_packet_header_t* packet)
{
	// Total Size = Packet Count + Sync, Mode, Checksum
	size_t totalPacketSize = packet->count + 3;
	uint8_t* payload = (uint8_t*)packet + 3;

#ifdef DEBUG_JVS_PACKETS
	printf("JvsIo::SendPacket: ");
	for (unsigned i = 0; i < totalPacketSize; i++) {
		printf("[%02X]", ((uint8_t*)packet)[i]);
	}

	printf("\n");
#endif

	// If the packet was intended for us, we need to handle it
	if (packet->target == 0xFF || packet->target == DeviceId) {
		HandlePacket(packet, payload);
	}

	return totalPacketSize;
}

size_t JvsIo::ReceivePacket(void* packet)
{
	if (ResponseBuffer.empty()) {
		return 0;
	}

	// Build a JVS Response Packet containing the payload
	jvs_packet_header_t* header = (jvs_packet_header_t*)packet;
	header->sync = 0xE0;
	header->target = 0x00; // Target Master Device
	header->count = ResponseBuffer.size() + 1; // Set data size to payload + checksum

	// Get a pointer to the payload (skip the header bytes)
	uint8_t* payload = ((uint8_t*)packet) + 3;

	// Copy the payload
	memcpy(payload, &ResponseBuffer[0], ResponseBuffer.size());

	// Calculate the checksum
	uint8_t checksum = 0;
	for (int i = 2; i < ResponseBuffer.size() + 3; i++) {
		checksum += ((uint8_t*)packet)[i];
	}

	// Write the checksum
	payload[ResponseBuffer.size()] = checksum;

	ResponseBuffer.clear();

#ifdef DEBUG_JVS_PACKETS
	printf("JvsIo::ReceivePacket: ");
	for (unsigned i = 0; i < header->count + 3; i++) {
		printf("[%02X]", ((uint8_t*)packet)[i]);
	}

	printf("\n");
#endif

	// Return the total packet size including header
	return header->count + 3;
}
