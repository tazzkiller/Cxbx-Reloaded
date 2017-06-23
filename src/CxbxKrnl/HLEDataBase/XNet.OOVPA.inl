// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Cxbx->Win32->CxbxKrnl->XNet.OOVPA.cpp
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
// *  (c) 2002-2003 Aaron Robinson <caustik@caustik.com>
// *
// *  All rights reserved
// *
// ******************************************************************
#ifndef XNET_OOVPA_INL
#define XNET_OOVPA_INL

#include "OOVPA.h"

#include "HLEDataBase/XNet.1.0.3911.inl"
#include "HLEDataBase/XNet.1.0.4627.inl"

// ******************************************************************
// * XNet_OOVPA
// ******************************************************************
OOVPATable XNet_OOVPA[] = {

	REGISTER_OOVPAS(XNetStartup, 3911, 4627), // PATCH same as xonline 4361
	REGISTER_OOVPAS(WSAStartup, 3911, 4627), // PATCH same as xonline 4361
	REGISTER_OOVPAS(XnInit, 3911, 4627), // XREF
	REGISTER_OOVPAS(XNetGetEthernetLinkStatus, 3911), // PATCH 
	REGISTER_OOVPAS(socket, 4627), // PATCH 
	REGISTER_OOVPAS(connect, 4627), // PATCH 
	REGISTER_OOVPAS(send, 4627), // PATCH 
	REGISTER_OOVPAS(recv, 4627), // PATCH 
	REGISTER_OOVPAS(ioctlsocket, 4627), // PATCH 

};

// ******************************************************************
// * XNet_OOVPASIZE
// ******************************************************************
uint32 XNet_OOVPA_SIZE = sizeof(XNet_3911);

#endif
