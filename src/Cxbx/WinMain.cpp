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
// *   Cxbx->WinMain.cpp
// *
// *  This file is part of the Cxbx-Reloaded project, a fork of Cxbx.
// *
// *  Cxbx-Reloaded is free software; you can redistribute it
// *  and/or modify it under the terms of the GNU General Public
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
// *  (c) 2002-2006 Aaron Robinson <caustik@caustik.com>
// *
// *  All rights reserved
// *
// ******************************************************************

#include "WndMain.h"

#include "CxbxKrnl/CxbxKrnl.h"
#include "CxbxKrnl/Emu.h"
#include "CxbxKrnl/EmuShared.h"
#include <commctrl.h>

// Enable Visual Styles
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name = 'Microsoft.Windows.Common-Controls' version = '6.0.0.0' \
processorArchitecture = '*' publicKeyToken = '6595b64144ccf1df' language = '*'\"")

/*! program entry point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/*! verify Cxbx.exe is loaded to base address 0x00010000 */
	if ((UINT_PTR)GetModuleHandle(nullptr) != CXBX_BASE_ADDR)
	{
		/*! CXBX_BASE_ADDR is defined as 0x00010000, which is the base address of
			the Cxbx.exe host executable.
		    Set in Cxbx Project options, Linker, Advanced, Base Address */
		MessageBox(NULL, "Cxbx.exe is not loaded to base address 0x00010000 (which is a requirement for Xbox emulation)", "Cxbx-Reloaded", MB_OK);
		return 1;
	}

	/* Initialize Cxbx File Paths */
	CxbxInitFilePaths();

    /*! initialize shared memory */
    EmuShared::Init();

	if (__argc >= 2 && strcmp(__argv[1], "/load") == 0 && strlen(__argv[2]) > 0)  {
		CxbxKrnlMain(__argc, __argv);
		return 0;
	}

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

    WndMain *MainWindow = new WndMain(hInstance);

    /*! wait for window to be created, or failure */
    while(!MainWindow->isCreated() && MainWindow->ProcessMessages())
    {
        Sleep(20);
    }

    /*! optionally open xbe and start emulation, if command line parameter was specified */
    if(__argc > 1 && false == MainWindow->HasError())
    {
        MainWindow->OpenXbe(__argv[1]);

        MainWindow->StartEmulation(MainWindow->GetHwnd());
    }

    /*! wait for window to be closed, or failure */
    while(!MainWindow->HasError() && MainWindow->ProcessMessages())
    {
        Sleep(10);
    }

    /*! if an error occurred, notify user */
    if(MainWindow->HasError())
    {
        MessageBox(NULL, MainWindow->GetError().c_str(), "Cxbx-Reloaded", MB_ICONSTOP | MB_OK);
    }

    delete MainWindow;

    /*! cleanup shared memory */
    EmuShared::Cleanup();

    return 0;
}
