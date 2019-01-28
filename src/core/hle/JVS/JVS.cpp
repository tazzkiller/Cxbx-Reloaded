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
#include "core\kernel\support\Emu.h"
#include "core\kernel\support\EmuXTL.h"
#include "core\hle\Intercept.hpp"


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

		XB_trampoline(DWORD, WINAPI, JvsBACKUP_Read, (DWORD, DWORD, DWORD, DWORD));

	// If we didn't find the unpatched function, try the other variant
	if (XB_JvsBACKUP_Read == nullptr) {
		XB_trampoline(DWORD, WINAPI, JvsBACKUP_Read2, (DWORD, DWORD, DWORD, DWORD));
		XB_JvsBACKUP_Read = XB_JvsBACKUP_Read2;
	}

	DWORD result = XB_JvsBACKUP_Read(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsBACKUP_Write, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsBACKUP_Write(a1, a2, a3, a4);

	RETURN(result);
}

DWORD WINAPI XTL::EMUPATCH(JvsEEPROM_Read)
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

	XB_trampoline(DWORD, WINAPI, JvsEEPROM_Read, (DWORD, DWORD, DWORD, DWORD));

	// If we didn't find the unpatched function, try the other variant
	if (XB_JvsEEPROM_Read == nullptr) {
		XB_trampoline(DWORD, WINAPI, JvsEEPROM_Read2, (DWORD, DWORD, DWORD, DWORD));
		XB_JvsEEPROM_Read = XB_JvsEEPROM_Read2;
	}

	DWORD result = XB_JvsEEPROM_Read(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsEEPROM_Write, (DWORD, DWORD, DWORD, DWORD));

	// If we didn't find the unpatched function, try the other variant
	if (XB_JvsEEPROM_Write == nullptr) {
		XB_trampoline(DWORD, WINAPI, JvsEEPROM_Write2, (DWORD, DWORD, DWORD, DWORD));
		XB_JvsEEPROM_Write = XB_JvsEEPROM_Write2;
	}

	DWORD result = XB_JvsEEPROM_Write(a1, a2, a3, a4);

	RETURN(result);
}

DWORD WINAPI XTL::EMUPATCH(JvsFirmwareDownload)
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

	XB_trampoline(DWORD, WINAPI, JvsFirmwareDownload, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsFirmwareDownload(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsFirmwareUpload, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsFirmwareUpload(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsNodeReceivePacket, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsNodeReceivePacket(a1, a2, a3);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsNodeSendPacket, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsNodeSendPacket(a1, a2, a3);

	RETURN(result);
}

DWORD WINAPI XTL::EMUPATCH(JvsRTC_Read)
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

	XB_trampoline(DWORD, WINAPI, JvsRTC_Read, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsRTC_Read(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScFirmwareDownload, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScFirmwareDownload(a1, a2, a3, a4);
	
	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScFirmwareUpload, (DWORD, DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScFirmwareUpload(a1, a2, a3, a4);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScReceiveMidi, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScReceiveMidi(a1, a2, a3);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScSendMidi, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScSendMidi(a1, a2, a3);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScReceiveRs323c, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScReceiveRs323c(a1, a2, a3);

	RETURN(result);
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

	XB_trampoline(DWORD, WINAPI, JvsScSendRs323c, (DWORD, DWORD, DWORD));
	DWORD result = XB_JvsScSendRs323c(a1, a2, a3);

	RETURN(result);
}
