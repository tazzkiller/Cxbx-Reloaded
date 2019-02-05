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

// Global variables used to store JVS related firmware/eeproms
std::vector<uint8_t> g_MainBoardFirmware;
std::vector<uint8_t> g_MainBoardEeprom;

bool JVS_LoadFile(std::string path, std::vector<uint8_t>& data)
{
	FILE* fp = fopen(path.c_str(), "rb");

	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* buffer = (uint8_t*)malloc(size);

	if (buffer == nullptr) {
		return false;
	}

	fread(buffer, size, 1, fp);

	for (unsigned i = 0; i < size; i++) {
		data.push_back(buffer[i]);
	}

	free(buffer);

	return true;
}

void JVS_Init()
{
	std::string romPath = std::string(szFolder_CxbxReloadedData) + std::string("\\EmuDisk\\Chihiro");
	std::string mainBoardFirmwarePath = "ic10_g24lc64.bin";
	std::string mainBoardEepromPath = "ic11_24lc024.bin";

	if (!JVS_LoadFile((romPath + "\\" + mainBoardFirmwarePath).c_str(), g_MainBoardFirmware)) {
		CxbxKrnlCleanup("Failed to load mainboard firmware: %s", mainBoardFirmwarePath.c_str());
	}

	if (!JVS_LoadFile((romPath + "\\" + mainBoardEepromPath).c_str(), g_MainBoardEeprom)) {
		CxbxKrnlCleanup("Failed to load mainboard EEPROM: %s", mainBoardEepromPath.c_str());
	}
}

DWORD WINAPI XTL::EMUPATCH(JvsBACKUP_Read)
(
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsBACKUP_Write)
(
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

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

	if (a4 != 0) {
		LOG_INCOMPLETE();
		return -1;
	}

	memcpy((void*)Buffer, &g_MainBoardEeprom[Offset], Length);

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsEEPROM_Write)
(
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	if (a4 != 0) {
		LOG_INCOMPLETE();
		return -1;
	}

	LOG_UNIMPLEMENTED();

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
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsNodeReceivePacket)
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

DWORD WINAPI XTL::EMUPATCH(JvsNodeSendPacket)
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
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

	RETURN(0);
}

DWORD WINAPI XTL::EMUPATCH(JvsScFirmwareUpload)
(
	DWORD a1,
	DWORD a2,
	DWORD a3,
	DWORD a4
)
{
	LOG_FUNC_BEGIN
		LOG_FUNC_ARG(a1)
		LOG_FUNC_ARG(a2)
		LOG_FUNC_ARG(a3)
		LOG_FUNC_ARG(a4)
		LOG_FUNC_END

	LOG_UNIMPLEMENTED();

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
