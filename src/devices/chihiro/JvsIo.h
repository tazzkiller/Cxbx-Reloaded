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

typedef struct {
	bool start = false;
	bool service = false;
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool button[7] = { false };

	uint8_t GetByte0() {
		uint8_t value = 0;
		value |= start     ? 1 << 7 : 0;
		value |= service   ? 1 << 6 : 0;
		value |= up        ? 1 << 5 : 0;
		value |= down      ? 1 << 4 : 0;
		value |= left      ? 1 << 3 : 0;
		value |= right     ? 1 << 2 : 0;
		value |= button[0] ? 1 << 1 : 0;
		value |= button[1] ? 1 << 0 : 0;
		return value;
	}

	uint8_t GetByte1() {
		uint8_t value = 0;
		value |= button[2] ? 1 << 7 : 0;
		value |= button[3] ? 1 << 6 : 0;
		value |= button[4] ? 1 << 5 : 0;
		value |= button[5] ? 1 << 4 : 0;
		value |= button[6] ? 1 << 3 : 0;
		return value;
	}
} jvs_switch_player_inputs_t;

typedef struct {
	uint8_t system = false;
	jvs_switch_player_inputs_t player[2];
} jvs_switch_inputs_t;

typedef struct {
	uint16_t value = 0x8000;

	uint8_t GetByte0() {
		return (value >> 8) & 0xFF;
	}

	uint8_t GetByte1() {
		return value & 0xFF;
	}
} jvs_analog_input_t;

typedef struct {
	uint16_t coins = 0;
	uint8_t status = 0;

	uint8_t GetByte0() {
		uint8_t value = 0;
		value |= (status << 7) & 0x3;
		value |= (coins & 0x3F00) >> 8;
		return value;
	}

	uint8_t GetByte1() {
		return coins & 0xFF;
	}
} jvs_coin_slots_t;

typedef struct {
	jvs_switch_inputs_t switches;
	jvs_analog_input_t analog[8];
	jvs_coin_slots_t coins[2];
} jvs_input_states_t;

class JvsIo
{
public:
	JvsIo(uint8_t* sense);
	void HandlePacket(jvs_packet_header_t* header, uint8_t* payload);
	size_t SendPacket(jvs_packet_header_t* packet);
	size_t ReceivePacket(void* packet);
	uint8_t GetDeviceId();
	void Update();

private:
	// Commands
	// These return the additional param bytes used
	int Jvs_Command_Reset();
	int Jvs_Command_SetDeviceId(uint8_t* data);
	int Jvs_Command_GetBoardId();
	int Jvs_Command_GetCommandFormat();
	int Jvs_Command_GetJvsRevision();
	int Jvs_Command_GetCommunicationVersion();
	int Jvs_Command_GetCapabilities();
	int Jvs_Command_ReadSwitchInputs(uint8_t* data);
	int Jvs_Command_ReadCoinInputs(uint8_t* data);
	int Jvs_Command_ReadAnalogInputs(uint8_t* data);
	int Jvs_Command_GeneralPurposeOutput(uint8_t* data);

	bool BroadcastPacket;					// Set when the last command was a broadcast
	uint8_t* pSense = nullptr;				// Pointer to Sense line
	uint8_t DeviceId = 0;					// Device ID assigned by running title
	std::vector<uint8_t> ResponseBuffer;	// Command Response

	// Device info
	uint8_t CommandFormatRevision;
	uint8_t JvsVersion;
	uint8_t CommunicationVersion;
	std::string BoardID;
	jvs_input_states_t Inputs;
};

extern JvsIo* g_pJvsIo;

#endif
