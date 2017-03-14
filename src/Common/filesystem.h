// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#ifdef __cplusplus
#pragma once

extern "C" {
#endif

// Source : https://github.com/PatrickvL/Dxbx/blob/master/Source/Delphi/src/uFileSystem.pas

/*
class FileHandle
{
};

class DirHandle
{
};

class SearchInfo
{
protected:
	virtual int_t GetAttributes() {}; // abstract;
	virtual std::string GetFilename() {}; // abstract;
	virtual int64_t GetFileSize() {}; // abstract;
public:
	property Filename: string read GetFilename;
	property Attributes: Integer read GetAttributes;
	property FileSize: Int64 read GetFileSize;
};

// TFileSystem is the base-class for all file systems,
// introducing most of the shared functionality needed
// to access folders and files in the file system.
// Because most code in this is abstract, we instantiate
// a concrete TFileSystem sub-class when actually mounting
// a volume (see RLogicalVolume.Mount).
class FileSystem
{
};

// Logical volume acts as a layer of seperation between the drive letter
// and the type of filesystem currently active behind this letter.
// The only real functionality in this type, is instantiating the correct
// TFileSystem sub-class when mounting a volume (currently based on extension).
struct _LogicalVolume
{
private:
	char FLetter;
	FileSystem *MyFileSystem;
public:
	property Letter: Char read FLetter;
	property FileSystem: TFileSystem read MyFileSystem;

	void Create();

	bool IsMounted();
	void  Unmount();
	bool Mount(string aDevice);

	bool OpenImage(const string aFileName, out aRelativeXBEFilePath : string);
}
LogicalVolume, *PLogicalVolume;

// Drives is the top-level in our emulated filesystem,
// offering an shared entry-point for all Xbox1 volumes
// and some minimal functionality.
struct _Drives
{
private:
	PLogicalVolume MyC;
	PLogicalVolume MyD;
	PLogicalVolume MyE;
	PLogicalVolume MyT;
	PLogicalVolume MyU;
	PLogicalVolume MyX;
	PLogicalVolume MyY;
	PLogicalVolume MyZ;
public:
	void Create();
	void Free();

	PLogicalVolume GetVolume(const char aLetter);

	// Xbox1 can have these partitions :
	property C: PLogicalVolume read MyC;    // C: for system files
	property D: PLogicalVolume read MyD;    // D: Optical drive. Mount game ISO/Folder here
	property E: PLogicalVolume read MyE;    // E: Data drive. Stores savegames, audio rips, etc.
	property T: PLogicalVolume read MyT;    // T: ??
	property U: PLogicalVolume read MyU;    // U: ??
	property X: PLogicalVolume read MyX;    // X, Y, Z: Cache drives. (Map Y: to D: too)
	property Y: PLogicalVolume read MyY;
	property Z: PLogicalVolume read MyZ;
}
Drives, *PDrives;

class XDVDFileSystem : public FileSystem
{

};
*/

#ifdef __cplusplus
}
#endif

#endif // __FILESYSTEM_H__