// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// prevent name collisions
namespace xboxkrnl
{

//#include <stdlib.h>
//#include <string.h>
//#include <sys/types.h>

//#include <xboxkrnl/xboxkrnl.h> //#include <stdtypes.h>

//#include "XDVDFS Tools\buffered_io.h"
//#include "XDVDFS Tools\xdvdfs.h"

#include "filesystem.h"

// Source : https://github.com/PatrickvL/Dxbx/blob/master/Source/Delphi/src/uFileSystem.pas

/*
BOOL XDVDFS_ReadSectorsFunc(
	void *Data,        //  Pointer to arbitrary data
	void *Buffer,      //  Buffer to fill
	DWORD StartSector, //  Start sector
	DWORD ReadSize     //  Number of sectors to read
)
{
	FileSeek(THandle(Data), Int64(StartSector) * SECTOR_SIZE, 0);

	return FileRead(THandle(Data), Buffer, ReadSize * SECTOR_SIZE) > 0;
}

XDVDFileSystem::TXDVDFileSystem
{
	Create(const aContainer : string);

	FMountPoint = aContainer;
	MyContainer = (THandle)FileOpen(aContainer, fmOpenRead || fmShareDenyWrite);
	MySession = new ;
	memset(MySession, 0, sizeof(MySession));
	if not XDVDFS_Mount(MySession, &TXDVDFS_ReadSectorsFunc, Pointer(MyContainer)) then
		Error('Couldn''t mount image!');
}

TXDVDFileSystem::~TXDVDFileSystem
{
	XDVDFS_UnMount(MySession);
	delete MySession;
	MySession = nullptr;
	FileClose(MyContainer);
	// inherited
}

*/

} // namespace