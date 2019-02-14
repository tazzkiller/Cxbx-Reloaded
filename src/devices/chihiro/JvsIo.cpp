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

int JvsIo::Jvs_Command_F0_Reset(uint8_t* data)
{
	uint8_t ensure_reset = data[1];

	if (ensure_reset == 0xD9) {
		// Set sense to 3 (2.5v) to instruct the baseboard we're ready.
		*pSense = 3;
		ResponseBuffer.push_back(ReportCode::Handled); // Note : Without this, Chihiro software stops sending packets (but JVS V3 doesn't send this?)
		DeviceId = 0;
	}
#if 0 // TODO : Is the following required?
	else {
		ResponseBuffer.push_back(ReportCode::InvalidParameter);
	}
#endif

#if 0 // TODO : Is the following required?
	// Detect a consecutive reset
	if (data[2] == 0xF0) {
		// TODO : Probably ensure the second reset too : if (data[3] == 0xD9) {
		// TODO : Handle two consecutive reset's here?

		return 3;
	}
#endif

	return 1;
}

int JvsIo::Jvs_Command_F1_SetDeviceId(uint8_t* data)
{
	// Set Address
	DeviceId = data[1];

	*pSense = 0; // Set sense to 0v
	ResponseBuffer.push_back(ReportCode::Handled);

	return 1;
}

int JvsIo::Jvs_Command_10_GetBoardId()
{
	// Get Board ID
	ResponseBuffer.push_back(ReportCode::Handled);

	for (char& c : BoardID) {
		ResponseBuffer.push_back(c);
	}

	return 0;
}

int JvsIo::Jvs_Command_11_GetCommandFormat()
{
	ResponseBuffer.push_back(ReportCode::Handled);
	ResponseBuffer.push_back(CommandFormatRevision);

	return 0;
}

int JvsIo::Jvs_Command_12_GetJvsRevision()
{
	ResponseBuffer.push_back(ReportCode::Handled);
	ResponseBuffer.push_back(JvsVersion);

	return 0;
}

int JvsIo::Jvs_Command_13_GetCommunicationVersion()
{
	ResponseBuffer.push_back(ReportCode::Handled);
	ResponseBuffer.push_back(CommunicationVersion);

	return 0;
}

int JvsIo::Jvs_Command_14_GetCapabilities()
{
	ResponseBuffer.push_back(ReportCode::Handled);

	// Capabilities list (4 bytes each)

	{ // Input capabilities
		ResponseBuffer.push_back(0x01); // Player button-sets
		ResponseBuffer.push_back(JVS_MAX_PLAYERS); // number of players
		ResponseBuffer.push_back(13); // 13 button switches per player
		ResponseBuffer.push_back(0);

		ResponseBuffer.push_back(0x02); // Coin slots
		ResponseBuffer.push_back(JVS_MAX_COINS); // number of coin slots
		ResponseBuffer.push_back(0);
		ResponseBuffer.push_back(0);

		ResponseBuffer.push_back(0x03); // Analog inputs
		ResponseBuffer.push_back(JVS_MAX_ANALOG); // number of analog input channels
		ResponseBuffer.push_back(16); // 16 bits per analog input channel
		ResponseBuffer.push_back(0);

		// TODO : Rotary input (0x04), JVS_MAX_ROTARY, 0, 0

		// TODO : Keycode input (0x05), 0, 0, 0

		// TODO : Screen pointer input (0x06), Xbits, Ybits, JVS_MAX_POINTERS

		// TODO : Switch input (0x07), 0, 0, 0
	}

	{ // Output capabilities
		// TODO : Card system (0x10), JVS_MAX_CARDS, 0, 0

		// TODO : Medal hopper (0x11), max?, 0, 0

		ResponseBuffer.push_back(0x12); // General-purpose outputs
		ResponseBuffer.push_back(6); // number of outputs
		ResponseBuffer.push_back(0);
		ResponseBuffer.push_back(0);

		// TODO : Analog output (0x13), channels, 0, 0

		// TODO : Character output (0x14), width, height, type

		// TODO : Backup data (0x15), 0, 0, 0
	}

	ResponseBuffer.push_back(0x00); // End of capabilities

	return 0;
}

int JvsIo::Jvs_Command_20_ReadSwitchInputs(uint8_t* data)
{
	static jvs_switch_player_inputs_t default_switch_player_input;
	uint8_t nr_switch_players = data[1];
	uint8_t bytesPerSwitchPlayerInput = data[2];

	ResponseBuffer.push_back(ReportCode::Handled);

	ResponseBuffer.push_back(Inputs.switches.system);

	for (int i = 0; i < nr_switch_players; i++) {
		for (int j = 0; j < bytesPerSwitchPlayerInput; j++) {
			// If a title asks for more switch player inputs than we support, pad with dummy data
			jvs_switch_player_inputs_t &switch_player_input = (i >= JVS_MAX_COINS) ? default_switch_player_input : Inputs.switches.player[i];
			uint8_t value
				= (j == 0) ? switch_player_input.GetByte0()
				: (j == 1) ? switch_player_input.GetByte1()
				: 0; // Pad any remaining bytes with 0, as we don't have that many inputs available
			ResponseBuffer.push_back(value);
		}
	}

	return 2;
}

int JvsIo::Jvs_Command_21_ReadCoinInputs(uint8_t* data)
{
	static jvs_coin_slots_t default_coin_slot;
	uint8_t nr_coin_slots = data[1];
	
	ResponseBuffer.push_back(ReportCode::Handled);

	for (int i = 0; i < nr_coin_slots; i++) {
		const uint8_t bytesPerCoinSlot = 2;
		for (int j = 0; j < bytesPerCoinSlot; j++) {
			// If a title asks for more coin slots than we support, pad with dummy data
			jvs_coin_slots_t &coin_slot = (i >= JVS_MAX_COINS) ? default_coin_slot : Inputs.coins[i];
			uint8_t value
				= (j == 0) ? coin_slot.GetByte0()
				: (j == 1) ? coin_slot.GetByte1()
				: 0; // Pad any remaining bytes with 0, as we don't have that many inputs available
			ResponseBuffer.push_back(value);
		}
	}

	return 1;
}

int JvsIo::Jvs_Command_22_ReadAnalogInputs(uint8_t* data)
{
	static jvs_analog_input_t default_analog;
	uint8_t nr_analog_inputs = data[1];

	ResponseBuffer.push_back(ReportCode::Handled);

	for (int i = 0; i < nr_analog_inputs; i++) {
		const uint8_t bytesPerAnalogInput = 2;
		for (int j = 0; j < bytesPerAnalogInput; j++) {
			// If a title asks for more analog input than we support, pad with dummy data
			jvs_analog_input_t &analog_input = (i >= JVS_MAX_ANALOG) ? default_analog : Inputs.analog[i];
			uint8_t value
				= (j == 0) ? analog_input.GetByte0()
				: (j == 1) ? analog_input.GetByte1()
				: 0; // Pad any remaining bytes with 0, as we don't have that many inputs available
			ResponseBuffer.push_back(value);
		}
	}

	return 1;
}

int JvsIo::Jvs_Command_32_GeneralPurposeOutput(uint8_t* data)
{
	uint8_t banks = data[1];

	ResponseBuffer.push_back(ReportCode::Handled);

	// TODO: Handle output

	// Data size is 1 byte indicating the number of banks, followed by one byte per bank
	return 1 + banks;
}

uint8_t JvsIo::GetEscapedByte(uint8_t* &payload)
{
	uint8_t value = *payload++;

	// Special case: 0xD0 is an exception byte that actually returns the next byte + 1
	if (value = EscapeByte) {
		value = *payload++ + 1;
	}

	return value;
}

void JvsIo::HandlePacket(jvs_packet_header_t* header, std::vector<uint8_t>& packet)
{
	// Clear the response buffer
	ResponseBuffer.clear();

	ResponseBuffer.push_back(0x01);
	
	// It's possible for a JVS packet to contain multiple commands, so we must iterate through it
	for (size_t i = 0; i < packet.size(); i++) {
		BroadcastPacket = packet[i] >= 0xF0; // Set a flag when broadcast packet

		uint8_t* command_data = &packet[i];
		switch (packet[i]) {
			// Broadcast Commands
			case 0xF0: i += Jvs_Command_F0_Reset(command_data); break;
			case 0xF1: i += Jvs_Command_F1_SetDeviceId(command_data); break;
			// Init Commands
			case 0x10: i += Jvs_Command_10_GetBoardId(); break;
			case 0x11: i += Jvs_Command_11_GetCommandFormat();	break;
			case 0x12: i += Jvs_Command_12_GetJvsRevision(); break;
			case 0x13: i += Jvs_Command_13_GetCommunicationVersion(); break;
			case 0x14: i += Jvs_Command_14_GetCapabilities(); break;
			case 0x20: i += Jvs_Command_20_ReadSwitchInputs(command_data); break;
			case 0x21: i += Jvs_Command_21_ReadCoinInputs(command_data); break;
			case 0x22: i += Jvs_Command_22_ReadAnalogInputs(command_data); break;
			case 0x32: i += Jvs_Command_32_GeneralPurposeOutput(command_data); break;
			default:
				printf("JvsIo::HandlePacket: Unhandled Command %02X\n", packet[i]);
				break;
		}
	}
}

size_t JvsIo::SendPacket(uint8_t* buffer)
{
	// Remember where the buffer started (so we can calculate the number of bytes we've handled)
	uint8_t* buffer_start = buffer;

	// Scan the packet header
	jvs_packet_header_t header;

	// First, read the sync byte
	header.sync = *buffer++;
	if (header.sync != SyncByte) {
		// If it's wrong, return we've processed (actually, skipped) one byte
		return 1;
	}

	// Decode the target and count fields
	header.target = GetEscapedByte(buffer);
	header.count = GetEscapedByte(buffer);

	// Decode the payload data
	std::vector<uint8_t> packet;
	for (int i = 0; i < header.count; i++) {
		packet.push_back(GetEscapedByte(buffer));
	}

	// The last byte is the checksum
	uint8_t checksum = packet.back(); packet.pop_back();
	// TODO : verify checksum - skip packet if invalid?

	// Total Size = Packet Count + Sync, Mode, Checksum
	size_t totalPacketSize = buffer - buffer_start;

	// If the packet was intended for us, we need to handle it
	if (header.target == 0xFF || header.target == DeviceId) {
		HandlePacket(&header, packet);
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
	header->sync = SyncByte;
	header->target = 0x00; // Target Master Device
	header->count = uint8_t(ResponseBuffer.size()) + 1; // Set data size to payload + checksum

	// Get a pointer to the payload (skip the header bytes)
	uint8_t* payload = ((uint8_t*)packet) + 3;

	// Copy the payload
	memcpy(payload, &ResponseBuffer[0], ResponseBuffer.size());

	// Calculate the checksum
	uint8_t checksum = 0;
	for (size_t i = 2; i < ResponseBuffer.size() + 3; i++) {
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
