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
// *   src->core->HLE->JVS->JVS.cpp
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
// *  (c) 2019 Luke Usher <luke.usher@outlook.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#define _XBOXKRNL_DEFEXTRN_

#define LOG_PREFIX CXBXR_MODULE::JVS

#undef FIELD_OFFSET     // prevent macro redefinition warnings

#include "EmuShared.h"
#include "common\Logging.h"
#include "core\kernel\init\CxbxKrnl.h"
#include "core\kernel\support\Emu.h"
#include "core\kernel\support\EmuXTL.h"
#include "core\hle\Intercept.hpp"
#include <thread>

#pragma warning(disable:4244) // Silence mio compiler warnings
#include "mio\mmap.hpp"
#pragma warning(default:4244)

// Global variables used to store JVS related firmware/eeproms
mio::mmap_sink g_MainBoardFirmware;		// QC Microcontroller firmware
mio::mmap_sink g_MainBoardScFirmware;	// SC Microcontroller firmware
mio::mmap_sink g_MainBoardEeprom;		// Config EEPROM
mio::mmap_sink g_MainBoardBackup;		// Backup Memory (high-scores, etc)

// Points to the Main/Filter Board State variables (including dipsw, test/service buttons)
DWORD* g_pJvsFilterBoardState = nullptr;

void JVS_SetTestButtonState(bool state)
{
	if (g_pJvsFilterBoardState == nullptr) {
		return;
	}

	uint32_t mask = 1 << 6;
	if (state) {
		*g_pJvsFilterBoardState &= ~mask;
	} else {
		*g_pJvsFilterBoardState |= mask;
	}
}

void JVS_SetServiceButtonState(bool state)
{
	if (g_pJvsFilterBoardState == nullptr) {
		return;
	}

	uint32_t mask = 1 << 7;
	if (state) {
		*g_pJvsFilterBoardState &= ~mask;
	} else {
		*g_pJvsFilterBoardState |= mask;
	}
}

bool JVS_LoadFile(std::string path, mio::mmap_sink& data)
{
	FILE* fp = fopen(path.c_str(), "rb");

	if (fp == nullptr) {
		return false;
	}

	std::error_code error;
	data = mio::make_mmap_sink(path, error);

	if (error) {
		return false;
	}

	return true;
}

void JvsInputThread()
{
	SetThreadAffinityMask(GetCurrentThread(), g_CPUOthers);

	while (true) {
		JVS_SetTestButtonState(GetAsyncKeyState(VK_F1));
		JVS_SetServiceButtonState(GetAsyncKeyState(VK_F2));
		Sleep(10);
	}
}


void XTL::JVS_Init()
{
	std::string romPath = std::string(szFolder_CxbxReloadedData) + std::string("\\EmuDisk\\Chihiro");
	std::string mainBoardFirmwarePath = "ic10_g24lc64.bin";
	std::string mainBoardScFirmwarePath = "pc20_g24lc64.bin";
	std::string mainBoardEepromPath = "ic11_24lc024.bin";
	std::string mainBoardBackupPath = "backup_ram.bin";

	if (!JVS_LoadFile((romPath + "\\" + mainBoardFirmwarePath).c_str(), g_MainBoardFirmware)) {
		CxbxKrnlCleanup("Failed to load mainboard firmware: %s", mainBoardFirmwarePath.c_str());
	}

	if (!JVS_LoadFile((romPath + "\\" + mainBoardScFirmwarePath).c_str(), g_MainBoardScFirmware)) {
		CxbxKrnlCleanup("Failed to load mainboard qc firmware: %s", mainBoardScFirmwarePath.c_str());
	}

	if (!JVS_LoadFile((romPath + "\\" + mainBoardEepromPath).c_str(), g_MainBoardEeprom)) {
		CxbxKrnlCleanup("Failed to load mainboard EEPROM: %s", mainBoardEepromPath.c_str());
	}

	// backup ram is a special case, we can create it automatically if it doesn't exist
	if (!std::experimental::filesystem::exists(romPath + "\\" + mainBoardBackupPath)) {
		FILE* fp = fopen((romPath + "\\" + mainBoardBackupPath).c_str(), "w");
		if (fp == nullptr) {
			CxbxKrnlCleanup("Could not create Backup File: %s", mainBoardBackupPath.c_str());
		}

		// Create 128kb empty file for backup ram
		fseek(fp, (128 * 1024) - 1, SEEK_SET);
		fputc('\0', fp);
		fclose(fp);
	}

	if (!JVS_LoadFile((romPath + "\\" + mainBoardBackupPath).c_str(), g_MainBoardBackup)) {
		CxbxKrnlCleanup("Failed to load mainboard BACKUP RAM: %s", mainBoardBackupPath.c_str());
	}

	// Determine which version of JVS_SendCommand this title is using and derive the offset
	static int JvsSendCommandVersion = -1;
	g_pJvsFilterBoardState = nullptr;

	auto JvsSendCommandOffset1 = (uintptr_t)GetXboxSymbolPointer("JVS_SendCommand");
	auto JvsSendCommandOffset2 = (uintptr_t)GetXboxSymbolPointer("JVS_SendCommand2");
	auto JvsSendCommandOffset3 = (uintptr_t)GetXboxSymbolPointer("JVS_SendCommand3");

	if (JvsSendCommandOffset1) {
		JvsSendCommandVersion = 1;
		g_pJvsFilterBoardState = *(DWORD**)(JvsSendCommandOffset1 + 0x2A0);
	}

	if (JvsSendCommandOffset2) {
		JvsSendCommandVersion = 2;
		g_pJvsFilterBoardState = *(DWORD**)(JvsSendCommandOffset2 + 0x312);
	}

	if (JvsSendCommandOffset3) {
		JvsSendCommandVersion = 3;
		g_pJvsFilterBoardState = *(DWORD**)(JvsSendCommandOffset3 + 0x307);

		if ((DWORD)g_pJvsFilterBoardState > XBE_MAX_VA) { 
			// This was invalid, we must have the other varient of SendCommand3 (SEGABOOT)
			g_pJvsFilterBoardState = *(DWORD**)(JvsSendCommandOffset3 + 0x302);
		}
	}

	// Set a sane initial state
	if (g_pJvsFilterBoardState) {
		// TODO: Choose a good set of defaults
		// Bit 0:		1 = Horizontal Display, 0 = Vertical Display
		// Bits 1-2:    Sets D3D Resolution: 0 = 1024x768, 1 = 640x480, 2 = 800x600, 3 = 640x480. CreateDevice fails when != 3
		// Bit 3:		Unknown
		// Bit 4:		0 = Hardware Vertex Processing, 1 = Software Vertex processing (Causes D3D to fail).. Why does this exist?
		// Bit 5:		Unknown
		// Bit 6:		Test Switch (0 = Pressed, 1 = Released)
		// Bit 7:		Service Switch (0 = Pressed, 1 = Released)
		*g_pJvsFilterBoardState = 0b11010111;
	}

	// Spawn the Chihiro/JVS Input Thread
	std::thread(JvsInputThread).detach();

}

DWORD WINAPI XTL::EMUPATCH(JVS_SendCommand)
(
	DWORD a1,
	DWORD Command,
	DWORD a3,
	DWORD Length,
	DWORD a5,
	DWORD a6,
	DWORD a7,
	DWORD a8
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(Command)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(a5)
		LOG_FUNC_ARG(a6)
		LOG_FUNC_ARG(a7)
		LOG_FUNC_ARG(a8)
		LOG_FUNC_END;

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsBACKUP_Read)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy((void*)Buffer, &g_MainBoardBackup[Offset], Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsBACKUP_Write)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy(&g_MainBoardBackup[Offset], (void*)Buffer, Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsEEPROM_Read)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG_OUT(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy((void*)Buffer, &g_MainBoardEeprom[Offset], Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsEEPROM_Write)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG_OUT(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy(&g_MainBoardEeprom[Offset], (void*)Buffer, Length);

	std::error_code error;
	g_MainBoardEeprom.sync(error);

	if (error) {
		EmuLog(LOG_LEVEL::WARNING, "Couldn't sync EEPROM to disk");
	}

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsFirmwareDownload)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG_OUT(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy((void*)Buffer, &g_MainBoardFirmware[Offset], Length);

	RETURN(0);
}


DWORD WINAPI XTL::EMUPATCH(JvsFirmwareUpload)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy(&g_MainBoardFirmware[Offset], (void*)Buffer, Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsNodeReceivePacket)
(
	PUCHAR Buffer,
	PDWORD a2,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsNodeSendPacket)
(
	PUCHAR Buffer,
	DWORD Length,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	// Buffer contains opening two bytes 00 XX where XX is the number of JVS packets to send
	// Each JVS packet is prependec with 00, the rest of the packet is as-per the JVS I/O standard.

	unsigned packetCount = Buffer[1];
	uint8_t* packetPtr = &Buffer[2]; // First JVS packet starts at offset 2;

	printf("JvsNodeSendPacket: Sending %d Packets\n", packetCount);

	for (unsigned i = 0; i < packetCount; i++) {
		// Skip the 0 seperator between packets
		packetPtr++;

		printf("Packet %d: ", i);
		jvs_packet_header_t* header = (jvs_packet_header_t*)packetPtr;
		for (unsigned j = 0; j <= header->count; j++) {
			printf("[%02X]", *packetPtr);
			packetPtr++;
		}
		
		// Finally, print the checksum byte
		printf("[%02X]", *packetPtr);

		// Increment the pointer to start at the next JVS Packet
		packetPtr++;

		printf("\n");
	}

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

// Binary Coded Decimal to Decimal conversion
uint8_t BcdToUint8(uint8_t value)
{
	return value - 6 * (value >> 4);
}

uint8_t Uint8ToBcd(uint8_t value)
{
	return value + 6 * (value / 10);
}

DWORD WINAPI XTL::EMUPATCH(JvsRTC_Read)
(
	DWORD a1,
	DWORD a2,
	JvsRTCTime* pTime,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG_OUT(time)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	time_t hostTime;
	struct tm* hostTimeInfo;
	time(&hostTime);
	hostTimeInfo = localtime(&hostTime);

	memset(pTime, 0, sizeof(JvsRTCTime));

	pTime->day = Uint8ToBcd(hostTimeInfo->tm_mday);
	pTime->month = Uint8ToBcd(hostTimeInfo->tm_mon + 1);	// Chihiro month counter stats at 1
	pTime->year = Uint8ToBcd(hostTimeInfo->tm_year - 100);	// Chihiro starts counting from year 2000

	pTime->hour = Uint8ToBcd(hostTimeInfo->tm_hour);
	pTime->minute = Uint8ToBcd(hostTimeInfo->tm_min);
	pTime->second = Uint8ToBcd(hostTimeInfo->tm_sec);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsRTC_Write)
(
	DWORD a1,
	DWORD a2,
	JvsRTCTime* pTime,
	DWORD a4
	)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG_OUT(time)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScFirmwareDownload)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG_OUT(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy((void*)Buffer, &g_MainBoardScFirmware[Offset], Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScFirmwareUpload)
(
	DWORD Offset,
	DWORD Length,
	PUCHAR Buffer,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(Offset)
		LOG_FUNC_ARG(Length)
		LOG_FUNC_ARG(Buffer)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	memcpy(&g_MainBoardScFirmware[Offset], (void*)Buffer, Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScReceiveMidi)
(
	DWORD a1,
	DWORD a2,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScSendMidi)
(
	DWORD a1,
	DWORD a2,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScReceiveRs323c)
(
	DWORD a1,
	DWORD a2,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}


DWORD WINAPI XTL::EMUPATCH(JvsScSendRs323c)
(
	DWORD a1,
	DWORD a2,
	DWORD a3
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}
